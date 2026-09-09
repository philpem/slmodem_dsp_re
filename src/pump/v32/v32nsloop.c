/*
 * v32nsloop.c -- ITU-T V.32 / V.32bis: the LOCAL LOOPBACK half-duplex
 *                next-state dispatcher.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V32LocLoopNextState   .text 0x0864a0   794
 *
 * The blob's `V32loop.c`; v32hdxst.h records the FILE-symbol mapping.  It is
 * the third of the five `V32*NextState` dispatchers a spent `TxHdx*` or
 * `RxHdx*` state reaches through `V32NextState[hdx->mode]`, and it advances
 * the handshake one step, installing the next transmit state at hdx + 0x6c and
 * the next receive state at hdx + 0x70.
 *
 * ---------------------------------------------------------------------------
 * WHY IT OCCUPIES TWO SLOTS OF `V32NextState`, AND WHAT THE OBJECT ACTUALLY
 * SETTLES
 *
 * `V32NextState[2]` and `[3]` are both this function -- read from the table's
 * own relocations by `tools/tabdump.py`, finding F8560, and reproduced in
 * `src/pump/v32/v32hdx_tables.c`.  NOTHING IN THIS FUNCTION EXPLAINS THAT; it
 * never reads hdx + 0x76 and cannot tell which slot called it.  What settles
 * it is the set of writers of hdx + 0x76 across the whole 1.2 MB, which is six
 * instructions and no more:
 *
 *      7e9a0  V32FP_recreate   mode = 0     obj->protocol == 0
 *      7f62b  V32FP_recreate   mode = 1     obj->protocol == 1
 *      7f3bd  V32FP_recreate   mode = 2     every other protocol
 *      8469c  V32FP_control    mode = 4     unless the mode was already 5
 *      82991  v32_data         mode = 5
 *      82735  V32FP_modem      mode = 6     with state 34, parking the machine
 *
 * `V32FP_recreate`'s three form one ladder on obj + 0x00 -- `test %ax,%ax;
 * jne`, then `dec %ax; je` -- so protocol 0 and 1 pick the originate and
 * answer dispatchers and EVERY other value falls into slot 2.  **Nothing
 * writes 3.**  So the duplicate entry makes two adjacent protocol values run
 * the same machine, and in this object only one of the two is reachable; slot
 * 3 is dead.  That is the measurement.  Which pair of loopbacks the author had
 * in mind is not in the object and is not guessed at here.
 *
 * ---------------------------------------------------------------------------
 * IT HAS NO TRACE, AND THAT IS A REAL DIFFERENCE FROM ITS FOUR SIBLINGS
 *
 * `V32RngInitNextState` and `V32RngRespNextState` both end with
 *
 *      if (dsplibs_debug_level > 1)
 *              dsplibs_debug_printf("state %s(%d)\n", V32StateName(s), s);
 *
 * and this function does not: its default label (86550) is the three
 * instructions of the epilogue and a `ret`, with no `cmpl $0x1` against
 * `dsplibs_debug_level` anywhere in its 794 bytes.  So the trace is not a
 * property of "a V.32 next-state function"; it belongs to `V32RNG.c` and not
 * to `V32loop.c`.
 *
 * ---------------------------------------------------------------------------
 * THE MACHINE
 *
 *      A  -> B   when the countdown has expired; retunes the tone to 0 Hz
 *      B  -> C   TxHdxCarrierState, tone 600 Hz
 *      C  -> D   RxHdxPhsReversal, and the only writer of hdx + 0x90 here
 *      D  -> E
 *      E  -> F   RxHdxData
 *      F  -> G   TxHdxScrSequence / RxHdxSequence
 *      G  -> H   RxHdxData; clears V32_FLAG_SILENCE and sets V32_FLAG_CARRIER
 *      H  -> I   RxHdxData; both mode setters, from the negotiated rate
 *      I         TERMINAL -- it does NOT advance the state
 *      ERROR     reloads the countdown, as in both RNG machines
 *
 * State I is the connect arm and is the one arm of any of the three machines
 * in this batch that leaves hdx + 0x74 alone: it reloads the countdown, turns
 * the equaliser's narrow step on, sets three flag bits and posts
 * `V32_CONNECT[rate]` as the status.  Re-entering it therefore repeats it for
 * ever, which is what a loopback with nothing to negotiate against would want
 * and is not read as more than that.
 *
 * The states B2 (2) and D2 (5) are DEFAULT entries in this table and are live
 * arms in `V32RngInitNextState`'s, so the two machines really do differ in
 * which sub-states they use rather than only in what each does.
 *
 * ---------------------------------------------------------------------------
 * NAMING
 *
 * hdx + 0x78 is `V32TXHDX.c`'s `V32HDX_STATE_LEFT`, spelled the same here
 * behind `#ifndef`.  hdx + 0x7c, + 0x80, + 0x90 and + 0x96 are MODELLED,
 * UNNAMED and wear their own offsets, which is `v32hshake.c`'s treatment of
 * obj + 0x31's unnamed bits; nothing in the object names any of the four.  The
 * two flag bits this file DOES name -- 0x20 and 0x40 -- are `v32demod.h`'s
 * V32_FLAG_CARRIER and V32_FLAG_SILENCE, and state G writes them as one
 * read-modify-write, clearing the second and setting the first.
 */

#include "dsplib/v32hdxst.h"

#include "dsplib/v32fpctl.h"
#include "dsplib/v32demod.h"		/* V32_FLAG_CARRIER, V32_FLAG_SILENCE */
#include "dsplib/v32seq.h"
#include "dsplib/v32state.h"

/* The instance is not modelled; see v32hdx.h.  These are the accessors. */
#define FIELD(obj, off)		((unsigned char *)(void *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_INT(obj, off)	(*(int *)(void *)FIELD((obj), (off)))
#define FIELD_S16(obj, off)	(*(short *)(void *)FIELD((obj), (off)))
#define FIELD_U8(obj, off)	(*(unsigned char *)FIELD((obj), (off)))

#define HDX(m)		FIELD_PTR((m), V32_OBJ_HDX)

/*
 * The handshake state's countdown, in symbols.  Derived in V32TXHDX.c from the
 * transmit states, which are what subtract from it.
 */
#ifndef V32HDX_STATE_LEFT
#define V32HDX_STATE_LEFT	0x78	/* int, <= 0 transitions             */
#endif

/*
 * MODELLED, UNNAMED.  +0x7c is cleared by seven of the ten arms and
 * accumulated into by `RxHdxNull`; +0x80 is what six arms RELOAD the countdown
 * from and is also `RxHdxNull`'s limit for +0x7c.  Two roles for +0x80, no
 * format string for either, so both keep neutral names.
 */
#ifndef V32HDX_INT_7C
#define V32HDX_INT_7C		0x7c	/* int                               */
#endif
#ifndef V32HDX_INT_80
#define V32HDX_INT_80		0x80	/* int                               */
#endif

/*
 * MODELLED, UNNAMED -- a 32-bit field state C sets to 1 and the receive states
 * `RxHdxPhsReversal` and `RxHdxRateSequence` read and write.  It is set here
 * exactly where `RxHdxPhsReversal` is installed, which bounds it and does not
 * name it.
 */
#ifndef V32HDX_INT_90
#define V32HDX_INT_90		0x90	/* int                               */
#endif

/*
 * MODELLED, UNNAMED -- a 16-bit value `RxHdxRateSequence` writes and state E
 * copies into scratch register 0.  Loaded `movswl` here into a `short`
 * parameter, so the extension is dead and says nothing (finding F614).
 */
#ifndef V32HDX_SHORT_96
#define V32HDX_SHORT_96		0x96	/* short                             */
#endif

/*
 * MODELLED, UNNAMED -- three of the eight bits of obj + 0x31, ORed on together
 * by state I.  `v32hshake.c` spells the first two the same way.
 */
#ifndef V32_FLAG_01
#define V32_FLAG_01		0x01
#endif
#ifndef V32_FLAG_08
#define V32_FLAG_08		0x08
#endif
#ifndef V32_FLAG_10
#define V32_FLAG_10		0x10
#endif

/* ------------------------------------------------------------------------ */

void
V32LocLoopNextState(void *modem)
{
	unsigned char *hdx = (unsigned char *)HDX(modem);
	short rate;

	switch (FIELD_S16(hdx, V32HDX_STATE)) {
	case V32_STATE_A:
		if (FIELD_INT(hdx, V32HDX_STATE_LEFT) > 0)
			break;
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_B;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 0xb4;
		SetToneDetect(modem, 0);
		break;

	case V32_STATE_B:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_C;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		FIELD_PTR(hdx, V32HDX_TXSTATE) = (void *)TxHdxCarrierState;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = FIELD_INT(hdx,
							      V32HDX_INT_80);
		InitGenSequence(modem, 3, 4, 2);
		SetToneDetect(modem, 600);
		break;

	case V32_STATE_C:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_D;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 0x200;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		FIELD_INT(hdx, V32HDX_INT_90) = 1;
		FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxPhsReversal;
		break;

	case V32_STATE_D:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_E;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = FIELD_INT(hdx,
							      V32HDX_INT_80);
		InitGenSequence(modem, 0xc, 4, 2);
		break;

	case V32_STATE_E:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_F;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 0x100;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxData;

		StoreReg(modem, FIELD_S16(hdx, V32HDX_SHORT_96), 0);
		SetRxLoopsV32(modem, 2);
		SetTxModeV32(modem, V32_MODE_ABS4);
		InitGenSequence(modem, 1, 4, 2);
		break;

	case V32_STATE_F:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_G;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		FIELD_PTR(hdx, V32HDX_TXSTATE) = (void *)TxHdxScrSequence;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = FIELD_INT(hdx,
							      V32HDX_INT_80);
		FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxSequence;

		InitGenSequence(modem, 0xf, 4, 2);
		SetTxModeV32(modem, V32_MODE_DIF4);
		SeedScramblerV32(modem, 0);
		InitDetSequence(modem, 0xffff, 0xffff, 0xffff, 2);
		break;

	case V32_STATE_G:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_H;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 0x2000;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxData;

		SetAdaptEqV32(modem, V32_ADAPTEQ_MU0);
		FIELD_U8(modem, V32_OBJ_FLAGS) = (unsigned char)
			((FIELD_U8(modem, V32_OBJ_FLAGS) & ~V32_FLAG_SILENCE)
			 | V32_FLAG_CARRIER);
		break;

	case V32_STATE_H:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_I;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 0x960;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxData;

		SetAdaptEqV32(modem, V32_ADAPTEQ_OFF);
		rate = (short)GetRateV32(modem);
		SetTxModeV32(modem, V32_TX_MODE[rate]);
		SetRxModeV32(modem, V32_RX_MODE[rate]);
		InitGenSequence(modem, 0xffff, 0x10, 8);
		break;

	case V32_STATE_I:
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = FIELD_INT(hdx,
							      V32HDX_INT_80);

		SetAdaptEqV32(modem, V32_ADAPTEQ_MU1);
		rate = (short)GetRateV32(modem);
		/*
		 * The STATUS store and the FLAGS or-in are independent and the
		 * object emits them in this order -- STATUS first -- rather
		 * than the other way round; the two-cell enumeration is
		 * unique (finding F10193).
		 */
		FIELD_U8(modem, V32_OBJ_STATUS) =
			(unsigned char)V32_CONNECT[rate];
		FIELD_U8(modem, V32_OBJ_FLAGS) |=
			V32_FLAG_01 | V32_FLAG_08 | V32_FLAG_10;
		break;

	case V32_STATE_ERROR:
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = FIELD_INT(hdx,
							      V32HDX_INT_80);
		break;

	default:
		break;
	}
}

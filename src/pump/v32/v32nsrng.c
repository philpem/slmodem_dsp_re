/*
 * v32nsrng.c -- ITU-T V.32 / V.32bis: the two RING half-duplex next-state
 *               dispatchers.
 *
 * Reconstructed from dsplibs.o, in the object's address order:
 *
 *   V32RngInitNextState   .text 0x084c80  1610
 *   V32RngRespNextState   .text 0x0852d0  1310
 *
 * Both live in the blob's `V32RNG.c` (v32hdxst.h records the FILE-symbol
 * mapping), which is why they share a file here.  They are slots 4 and 5 of
 * `V32NextState`: a transmit or receive state that has spent its budget calls
 * `V32NextState[hdx->mode]`, and these two advance the handshake one step and
 * install the next `TxHdx*` at hdx + 0x6c and the next `RxHdx*` at hdx + 0x70.
 *
 * ---------------------------------------------------------------------------
 * THE SHAPE OF BOTH, WHICH IS ONE SWITCH AND ONE TRACE
 *
 *      hdx   = modem->hdx;
 *      switch (hdx->state) { ... }
 *      if (dsplibs_debug_level > 1)
 *              dsplibs_debug_printf("state %s(%d)\n", ...);
 *
 * The bound is the object's: `cmp $0x21,%eax` with an UNSIGNED `ja`, on the
 * SIGN-EXTENDED 16-bit state, so 0..33 reach the table and 34
 * (`V32_STATE_DONT_CARE`) and every negative value take the default.  Each
 * jump table is 34 entries at .rodata 0x7f58 and 0x7fe0; both were read with
 * their relocations, not from the trimmed disassembly.
 *
 * THE FORMAT STRING IS THE AUTHOR'S OWN WORD.  `.rodata.str1.1 + 0x3971` is
 * "state %s(%d)\n" and both functions pass `V32StateName(hdx->state)` and the
 * state itself to it -- which is why the epilogue re-reads `modem->hdx` twice
 * (84d40 and 84d4f, 85360 and 8536f): the source names `modem->hdx->state`
 * once per argument and nothing caches it across the `V32StateName` call.
 * `V32LocLoopNextState` has no such trace at all; see v32nsloop.c.
 *
 * ---------------------------------------------------------------------------
 * THE STATES EACH MACHINE HAS
 *
 * V32RngInitNextState reaches A, B, C, D, D2, E, F, G, DONE and ERROR; every
 * other index of its table is the trace-only default.  The chain is
 *
 *      A  -> B    TxHdxCarrierState / RxHdxToneData
 *      B  -> C    (when the countdown expires)  RxHdxToneData
 *      C  -> D    TxHdxScrSequence, and RxHdxSequence on one arm
 *      D  -> D2   RxHdxSequenceE, or RxHdxSequence on the other arm
 *      D2 -> E
 *      E  -> F
 *      F  -> G    RxHdxSequence   -- or straight to DONE
 *      G  -> DONE RxHdxData
 *      DONE       posts the connect status; ERROR reloads the countdown
 *
 * V32RngRespNextState reaches A, B, C, D, E, F, G, H, DONE and ERROR:
 *
 *      A  -> B    TxHdxData / RxHdxSequence
 *      B  -> C    TxHdxCarrierState / RxHdxSequenceE
 *      C  -> D
 *      D  -> E    TxHdxScrSequence
 *      E  -> F    TxHdxScrSequence
 *      F  -> G    -- or to CLEARDOWN when there is no rate in common
 *      G  -> H    RxHdxSequence   -- or straight to DONE
 *      H  -> DONE RxHdxData
 *      DONE       posts the connect status; ERROR reloads the countdown
 *
 * so the responder has an H the initiator does not, and the initiator's F
 * carries the cleardown test the responder puts one state earlier.
 *
 * ---------------------------------------------------------------------------
 * THE INITIATOR'S DONE ARM OVERWRITES ITS OWN CLEARDOWN, AND THAT IS THE
 * OBJECT'S
 *
 * `V32RngInitNextState`'s DONE arm decodes TWO rate signals -- the one the far
 * end sent, in regs[4], and the one in regs[1] -- and if EITHER is
 * `V32_RATE_NONE` it sets the countdown to 0x40, posts V32_FLAG_FAULT with
 * status 0x0e, and moves to V32_STATE_CLEARDOWN.  It then FALLS INTO the
 * common tail (84cf7) that the non-fault path jumps to, and that tail
 * overwrites the countdown with hdx + 0x80 and the status byte with
 * `V32_CONNECT[rate]`, and ORs 0x05 onto the flags on top of the fault bit.
 *
 * So on the fault path only the STATE and the fault bit survive; the reason
 * code and the countdown the arm just wrote are both clobbered before the
 * function returns.  That is what the object does -- 85246 is a two
 * instruction block whose only content is reloading `hdx` and jumping into
 * 84cf7 -- and it is reproduced rather than tidied.  `V32RngRespNextState`
 * does NOT have the defect: its cleardown arm is in state F and returns.
 * Finding F8588.
 *
 * ---------------------------------------------------------------------------
 * WHAT `obj + 0x00` DECIDES, AND IT IS READ FIVE TIMES ACROSS THE TWO
 *
 * `v32seq.h` types obj + 0x00 as `protocol`, from `V32FP_recreate`'s own
 * "V32FP Config: protocol=%d,..." dump.  Four arms branch on it, always as
 * `protocol == 0` and always through the same branchless idiom
 * (`cmp $1,%reg; sbb %reg,%reg; ...`), an UNSIGNED compare so only zero takes
 * the first arm:
 *
 *   RngInit A   InitGenSequence pattern  0  or 3     SetToneDetect 600 or 1800
 *   RngInit B   InitGenSequence pattern 15  or 12
 *   RngInit C   v32_smc::state[0]       12  or 0
 *   RngResp B   InitGenSequence pattern  0  or 3
 *   RngResp C   InitGenSequence pattern 15  or 12
 *   RngResp D   v32_smc::state[0]       12  or 0
 *
 * The two machines therefore run the SAME protocol split one state apart.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS NAMED HERE AND WHAT IS NOT
 *
 * hdx + 0x78 and + 0x84 already have names -- `src/pump/v32/v32txhdx.c`
 * derived them from the transmit states and this file uses the same spellings
 * behind `#ifndef`.  hdx + 0x7c, + 0x80, + 0x48 and obj + 0x30's two reason
 * codes do not: they are spelled as constants wearing their own values, which
 * is `v32hshake.c`'s treatment of obj + 0x31's unnamed bits and CLAUDE.md's
 * "modelled, unnamed" state.  Naming one wrongly is worse than numbering it.
 *
 * hdx + 0x48 IS typed here even though it is not named: this file loads it
 * `movswl` and USES the 32-bit result (`0x18 - it`, stored into an `int`),
 * which finding F613's rule makes FORCED -- so the field is `short`.  The
 * twelve `RxHdx*` states load the same field `movzwl` and store sixteen bits
 * back, which is F614's dead extension and says nothing about the field.
 */

#include "dsplib/v32hdxst.h"

#include "dsplib/v32fpctl.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32smc.h"
#include "dsplib/v32dec.h"
#include "dsplib/v32state.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/debug.h"

/* The instance is not modelled; see v32hdx.h.  These are the accessors. */
#define FIELD(obj, off)		((unsigned char *)(void *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_INT(obj, off)	(*(int *)(void *)FIELD((obj), (off)))
#define FIELD_S16(obj, off)	(*(short *)(void *)FIELD((obj), (off)))
#define FIELD_U16(obj, off)	(*(unsigned short *)(void *)FIELD((obj), (off)))
#define FIELD_U8(obj, off)	(*(unsigned char *)FIELD((obj), (off)))

#define HDX(m)		FIELD_PTR((m), V32_OBJ_HDX)
#define FP(m)		FIELD_PTR((m), V32_OBJ_FP)
#define SMC(fp)		((struct v32_smc *)(void *)FIELD((fp), V32FP_SMC))
#define FSE(fp)		((struct fpm_fse *)(void *)FIELD((fp), V32FP_FSE))
#define DEC(fp)		((struct v32_dec *)FSE(fp)->cfg.owner)

/*
 * `LoadReg` and `StoreReg` reach V32HDX_REGS through an index; these two
 * functions reach two of the five DIRECTLY, and always UNSIGNED -- every load
 * is `movzwl` and every one of them feeds `DecodeRateSeq`, whose parameter is
 * `unsigned short`.  v32seq.c spells the same array `short` because `LoadReg`
 * returns `short`; per finding F614 the FIELD's type is not what varies.
 */
#define REG(hdx, n)	(((unsigned short *)(void *)FIELD((hdx), V32HDX_REGS))\
			 [(n)])

/*
 * WHICH REGISTER HOLDS WHAT IS DESCRIBED AND NOT NAMED.  Three of the five are
 * used across these two functions and the indices are literals in the object:
 *
 *   0   written from `CodeRateSeq(RateToSeq(GetRateV32()))` -- built from this
 *       end's own configured rate, and in `V32RngRespNextState` read back and
 *       put through `CodeRateSeq` a SECOND time
 *   1   written from `GetSequence()` during the rate-signal phase, and read by
 *       every later arm that needs the negotiated rate
 *   4   written from `GetSequence()` during the E-sequence phase, and the one
 *       `V32_CONNECT` is indexed by at DONE; also the zero/non-zero test that
 *       decides whether F (init) or G (resp) goes to DONE directly
 *
 * That is usage inference about the CONTENTS and no more, so the indices are
 * spelled as the object spells them.
 */

/* obj + 0x00, `V32FP_recreate`'s own "protocol=%d"; see the header comment. */
#ifndef V32_OBJ_PROTOCOL
#define V32_OBJ_PROTOCOL	0x00	/* unsigned short                    */
#endif

/*
 * The handshake state's countdown, in symbols.  Derived in v32txhdx.c from the
 * transmit states, which are what subtract from it; guarded because that file
 * and this one spell the same field.
 */
#ifndef V32HDX_STATE_LEFT
#define V32HDX_STATE_LEFT	0x78	/* int, <= 0 transitions             */
#endif

/*
 * MODELLED, UNNAMED.  +0x7c is cleared by almost every arm here and
 * accumulated into by `RxHdxNull` against a limit at +0x80; +0x80 is also what
 * a dozen arms RELOAD the countdown from.  Two roles for +0x80 and no format
 * string for either, so both keep neutral names.
 */
#ifndef V32HDX_INT_7C
#define V32HDX_INT_7C		0x7c	/* int                               */
#endif
#ifndef V32HDX_INT_80
#define V32HDX_INT_80		0x80	/* int                               */
#endif

/*
 * MODELLED, UNNAMED -- a 16-bit counter the receive states increment and this
 * file charges against a budget of 0x18.  `short`, and that is forced; see the
 * header comment.
 */
#ifndef V32HDX_BLOCK_CHARGE
#define V32HDX_BLOCK_CHARGE	0x84	/* short, = V32_SYMBOL_LEN[rate]     */
#endif

#ifndef V32HDX_SHORT_48
#define V32HDX_SHORT_48		0x48	/* short                             */
#endif

/*
 * MODELLED, UNNAMED -- five of the eight bits of obj + 0x31.  0x02, 0x20 and
 * 0x40 are named elsewhere (V32_FLAG_FAULT, V32_FLAG_CARRIER,
 * V32_FLAG_SILENCE); nothing in the object says what these five indicate, so
 * they wear their values exactly as `v32hshake.c`'s three do.
 */
#ifndef V32_FLAG_01
#define V32_FLAG_01		0x01
#endif
#ifndef V32_FLAG_04
#define V32_FLAG_04		0x04
#endif
#ifndef V32_FLAG_08
#define V32_FLAG_08		0x08
#endif
#ifndef V32_FLAG_10
#define V32_FLAG_10		0x10
#endif

/*
 * MODELLED, UNNAMED -- the two reason codes these functions post into
 * obj + 0x30 beside V32_FLAG_FAULT, both on "no rate in common".  0x17 is
 * numerically `v32fpctl.h`'s V32_STATUS_BAD_MODE, but that name was derived
 * from `SetTxModeV32`'s out-of-range arm, which is a different site with a
 * different cause; reusing it here would assert an identity the object does
 * not state.
 */
#define V32_STATUS_0E		0x0e
#define V32_STATUS_17		0x17

/*
 * The two detector patterns these machines arm, and the mask they share.
 * Literals in the object; named only so the two that differ in one nibble do
 * not read as a typo.
 */
#define V32_DET_MASK		((int)0xf111f111)
#define V32_DET_SEQ		0x01110111
#define V32_DET_ESEQ		0x0111f111

/* The handshake trace, and its re-read; see the header comment. */
#define TRACE_STATE(m)		FIELD_S16(HDX(m), V32HDX_STATE)

/* ------------------------------------------------------------------------ */

void
V32RngInitNextState(void *modem)
{
	unsigned char *hdx = (unsigned char *)HDX(modem);
	unsigned char *fp;
	short rate;
	short seq;

	switch (FIELD_S16(hdx, V32HDX_STATE)) {
	case V32_STATE_A:
		SetAdaptEqV32(modem, V32_ADAPTEQ_OFF);
		SetAdaptEcV32(modem, V32_ADAPTEC_OFF);
		SetRxLoopsV32(modem, 1);

		hdx = (unsigned char *)HDX(modem);
		fp = (unsigned char *)FP(modem);
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_B;
		FIELD_INT(fp, V32FP_R00) = 1;
		FIELD_PTR(hdx, V32HDX_TXSTATE) = (void *)TxHdxCarrierState;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 0x38;

		SetTxModeV32(modem, V32_MODE_ABS4);
		InitGenSequence(modem,
				(unsigned short)
				(FIELD_U16(modem, V32_OBJ_PROTOCOL) == 0
				 ? 0 : 3),
				4, 2);

		FIELD_U8(modem, V32_OBJ_FLAGS) = (unsigned char)
			((FIELD_U8(modem, V32_OBJ_FLAGS) & ~V32_FLAG_08)
			 | V32_FLAG_04);
		SetToneDetect(modem,
			      (short)(FIELD_U16(modem, V32_OBJ_PROTOCOL) == 0
				      ? 600 : 1800));

		hdx = (unsigned char *)HDX(modem);
		FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxToneData;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		break;

	/*
	 * B AND C SHARE THE `RxHdxToneData` STORE AND NOT THE FLAG CLEAR, AND
	 * THE TWO ARE ONE INSTRUCTION APART.  84e99 clears obj + 0x31's 0x04
	 * and falls into 84e9d, which installs the state; C's `> 0` arm and B's
	 * `> 0` arm both enter at 84e99, and B's TRANSITION path enters at
	 * 84e9d -- `eb ae` at 84eed, four bytes further on.  Reading that jump
	 * as 84e99 makes the transition clear a bit it must leave alone, which
	 * is invisible unless the bit was set on entry AND the countdown had
	 * expired.  Finding F8590.
	 */
	case V32_STATE_B:
		if (FIELD_INT(hdx, V32HDX_STATE_LEFT) <= 0) {
			FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_C;
			FIELD_INT(hdx, V32HDX_STATE_LEFT) = 8;
			InitGenSequence(modem,
					(unsigned short)
					(FIELD_U16(modem, V32_OBJ_PROTOCOL) == 0
					 ? 15 : 12),
					4, 2);
			hdx = (unsigned char *)HDX(modem);
		} else {
			FIELD_U8(modem, V32_OBJ_FLAGS) &=
				(unsigned char)~V32_FLAG_04;
		}
		FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxToneData;
		break;

	case V32_STATE_C:
		if (FIELD_INT(hdx, V32HDX_STATE_LEFT) > 0) {
			FIELD_U8(modem, V32_OBJ_FLAGS) &=
				(unsigned char)~V32_FLAG_04;
			FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxToneData;
			break;
		}

		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_D;
		FIELD_PTR(hdx, V32HDX_TXSTATE) = (void *)TxHdxScrSequence;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = FIELD_INT(hdx,
							      V32HDX_INT_80);

		if ((FIELD_U8(modem, V32_OBJ_FLAGS) & V32_FLAG_04) == 0) {
			FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxSequence;
			SetRxModeV32(modem, V32_MODE_DIF4);
			InitDetSequence(modem, V32_DET_SEQ, V32_DET_MASK, -1,
					2);
		}

		rate = (short)GetRateV32(modem);
		seq = (short)CodeRateSeq(modem, RateToSeq(modem, rate));
		StoreReg(modem, seq, 0);
		InitGenSequence(modem, (unsigned short)seq, 0x10, 2);
		SetTxModeV32(modem, V32_MODE_DIF4);
		SeedScramblerV32(modem, 0);
		SMC(FP(modem))->state[0] =
			(short)(FIELD_U16(modem, V32_OBJ_PROTOCOL) == 0
				? 12 : 0);
		break;

	case V32_STATE_D:
		if ((FIELD_U8(modem, V32_OBJ_FLAGS) & V32_FLAG_04) != 0) {
			FIELD_U8(modem, V32_OBJ_FLAGS) &=
				(unsigned char)~V32_FLAG_04;
			FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxSequence;
			SetRxModeV32(modem, V32_MODE_DIF4);
			InitDetSequence(modem, V32_DET_SEQ, V32_DET_MASK, -1,
					2);
			break;
		}

		StoreReg(modem, (short)GetSequence(modem), 1);

		hdx = (unsigned char *)HDX(modem);
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_D2;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		REG(hdx, 4) = 0;
		FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxSequenceE;
		InitDetSequence(modem, V32_DET_ESEQ, V32_DET_MASK, -1, 2);

		{
			int base = 0x40;
			int used;

			hdx = (unsigned char *)HDX(modem);
			used = FIELD_INT(hdx, V32HDX_INT_80)
				- FIELD_INT(hdx, V32HDX_STATE_LEFT);
			if (used > 0x3f) {
				used &= 7;
				base = 8;
			}
			FIELD_INT(hdx, V32HDX_STATE_LEFT) = base - used;
		}
		break;

	case V32_STATE_D2:
		SetAdaptEqV32(modem, V32_ADAPTEQ_MU1);
		SetRxLoopsV32(modem, 3);

		hdx = (unsigned char *)HDX(modem);
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_E;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 8;

		InitGenSequence(modem,
				CodeESeq(modem,
					 (unsigned short)LoadReg(modem,
								 1)),
				0x10, 2);
		break;

	case V32_STATE_E:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_F;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 0x18;
		FIELD_PTR(hdx, V32HDX_TXSTATE) = (void *)TxHdxScrSequence;

		rate = DecodeRateSeq(modem,
				     (unsigned short)LoadReg(modem, 1));
		SetTxModeV32(modem, V32_TX_MODE[rate]);
		InitGenSequence(modem, 0xffff, 0x10, 8);
		if (rate > 2)
			SMC(FP(modem))->f10 = 0;
		break;

	case V32_STATE_F:
		FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_08;

		rate = DecodeRateSeq(modem,
				     (unsigned short)LoadReg(modem, 1));
		SetTxModeV32(modem, V32_TX_MODE[rate]);
		InitGenSequence(modem, 0xffff, 0x10, 8);

		hdx = (unsigned char *)HDX(modem);
		if (REG(hdx, 4) == 0) {
			FIELD_INT(hdx, V32HDX_INT_7C) = 0;
			FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_G;
			FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxSequence;
			FIELD_INT(hdx, V32HDX_STATE_LEFT) =
				FIELD_INT(hdx, V32HDX_INT_80);
			SetRxModeV32(modem, V32_MODE_DIF4);
		} else {
			FIELD_INT(hdx, V32HDX_INT_7C) = 0;
			FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_DONE;
			FIELD_INT(hdx, V32HDX_STATE_LEFT) =
				0x18 - FIELD_S16(hdx, V32HDX_SHORT_48);
		}
		break;

	case V32_STATE_G:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_DONE;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 0x24;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxData;

		REG(hdx, 4) = (unsigned short)GetSequence(modem);
		rate = DecodeRateSeq(modem,
				     REG((unsigned char *)HDX(modem),
					 4));
		SetRxModeV32(modem, V32_RX_MODE[rate]);
		SetAdaptEqV32(modem, V32_ADAPTEQ_MU1);
		SetRxLoopsV32(modem, 3);
		break;

	case V32_STATE_DONE:
	{
		short other;

		rate = DecodeRateSeq(modem, REG(hdx, 4));
		other = DecodeRateSeq(modem,
				    REG((unsigned char *)HDX(modem),
					1));

		hdx = (unsigned char *)HDX(modem);
		if (rate == V32_RATE_NONE || other == V32_RATE_NONE) {
			/*
			 * Every one of these four but the state is undone by
			 * the tail below.  See the header comment: F8588.
			 */
			FIELD_INT(hdx, V32HDX_STATE_LEFT) = 0x40;
			FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_FAULT;
			FIELD_U8(modem, V32_OBJ_STATUS) = V32_STATUS_0E;
			FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_CLEARDOWN;
		}

		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = FIELD_INT(hdx,
							      V32HDX_INT_80);
		FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_01 | V32_FLAG_04;
		FIELD_U8(modem, V32_OBJ_STATUS) =
			(unsigned char)V32_CONNECT[rate];
		DEC(FP(modem))->retrain = 0;
		break;
	}

	case V32_STATE_ERROR:
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = FIELD_INT(hdx,
							      V32HDX_INT_80);
		break;

	default:
		break;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("state %s(%d)\n",
				     V32StateName(TRACE_STATE(modem)),
				     TRACE_STATE(modem));
}

/* ------------------------------------------------------------------------ */

void
V32RngRespNextState(void *modem)
{
	unsigned char *hdx = (unsigned char *)HDX(modem);
	unsigned char *fp;
	short rate;
	short seq;

	switch (FIELD_S16(hdx, V32HDX_STATE)) {
	case V32_STATE_A:
		SetAdaptEqV32(modem, V32_ADAPTEQ_OFF);
		SetRxLoopsV32(modem, 1);
		SetAdaptEcV32(modem, V32_ADAPTEC_OFF);

		hdx = (unsigned char *)HDX(modem);
		fp = (unsigned char *)FP(modem);
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_B;
		FIELD_INT(fp, V32FP_R00) = 1;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = FIELD_INT(hdx,
							      V32HDX_INT_80);
		FIELD_PTR(hdx, V32HDX_TXSTATE) = (void *)TxHdxData;
		FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxSequence;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;

		InitDetSequence(modem, V32_DET_SEQ, V32_DET_MASK, 0xffff, 2);
		SetRxModeV32(modem, V32_MODE_DIF4);
		FIELD_U8(modem, V32_OBJ_FLAGS) &= (unsigned char)~V32_FLAG_04;

		rate = (short)GetRateV32(modem);
		seq = (short)CodeRateSeq(modem, RateToSeq(modem, rate));
		StoreReg(modem, seq, 0);
		break;

	case V32_STATE_B:
		SetAdaptEqV32(modem, V32_ADAPTEQ_MU1);
		SetRxLoopsV32(modem, 3);

		hdx = (unsigned char *)HDX(modem);
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_C;
		REG(hdx, 4) = 0;
		FIELD_PTR(hdx, V32HDX_TXSTATE) = (void *)TxHdxCarrierState;
		FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxSequenceE;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 0x38;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;

		InitGenSequence(modem,
				(unsigned short)
				(FIELD_U16(modem, V32_OBJ_PROTOCOL) == 0
				 ? 0 : 3),
				4, 2);
		SetTxModeV32(modem, V32_MODE_ABS4);
		StoreReg(modem, (short)GetSequence(modem), 1);
		InitDetSequence(modem, V32_DET_ESEQ, V32_DET_MASK, -1, 2);
		FIELD_U8(modem, V32_OBJ_FLAGS) &= (unsigned char)~V32_FLAG_08;
		break;

	case V32_STATE_C:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_D;
		FIELD_S16(hdx, V32HDX_BLOCK_CHARGE) = 0;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 8;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		InitGenSequence(modem,
				(unsigned short)
				(FIELD_U16(modem, V32_OBJ_PROTOCOL) == 0
				 ? 15 : 12),
				4, 2);
		break;

	case V32_STATE_D:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_E;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 0x40;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		FIELD_PTR(hdx, V32HDX_TXSTATE) = (void *)TxHdxScrSequence;

		SetTxModeV32(modem, V32_MODE_DIF4);
		InitGenSequence(modem,
				CodeRateSeq(modem,
					    (unsigned short)LoadReg(modem, 0)),
				0x10, 2);
		SeedScramblerV32(modem, 0);
		SMC(FP(modem))->state[0] =
			(short)(FIELD_U16(modem, V32_OBJ_PROTOCOL) == 0
				? 12 : 0);
		break;

	case V32_STATE_E:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_F;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 8;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		FIELD_PTR(hdx, V32HDX_TXSTATE) = (void *)TxHdxScrSequence;

		InitGenSequence(modem,
				CodeESeq(modem,
					 (unsigned short)LoadReg(modem,
								 1)),
				0x10, 2);
		break;

	case V32_STATE_F:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_G;

		rate = DecodeRateSeq(modem,
				     (unsigned short)LoadReg(modem, 1));
		SetTxModeV32(modem, V32_TX_MODE[rate]);

		if (rate == V32_RATE_NONE) {
			hdx = (unsigned char *)HDX(modem);
			FIELD_INT(hdx, V32HDX_STATE_LEFT) = 0x28;
			FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_FAULT;
			FIELD_U8(modem, V32_OBJ_STATUS) = V32_STATUS_17;
			FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_CLEARDOWN;
			break;
		}

		InitGenSequence(modem, 0xffff, 0x10, 8);
		hdx = (unsigned char *)HDX(modem);
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 0x18;
		FIELD_PTR(hdx, V32HDX_TXSTATE) = (void *)TxHdxScrSequence;
		if (rate > 2)
			SMC(FP(modem))->f10 = 0;
		break;

	case V32_STATE_G:
		FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_08;

		rate = DecodeRateSeq(modem,
				     (unsigned short)LoadReg(modem, 1));
		SetTxModeV32(modem, V32_TX_MODE[rate]);

		hdx = (unsigned char *)HDX(modem);
		if (REG(hdx, 4) != 0) {
			FIELD_INT(hdx, V32HDX_INT_7C) = 0;
			FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_DONE;
			FIELD_INT(hdx, V32HDX_STATE_LEFT) =
				0x18 - FIELD_S16(hdx, V32HDX_SHORT_48);
		} else {
			FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxSequence;
			FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_H;
			FIELD_INT(hdx, V32HDX_STATE_LEFT) =
				FIELD_INT(hdx, V32HDX_INT_80);
		}
		break;

	case V32_STATE_H:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_DONE;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = 0x24;
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxData;

		REG(hdx, 4) = (unsigned short)GetSequence(modem);
		rate = DecodeRateSeq(modem,
				     REG((unsigned char *)HDX(modem),
					 4));
		SetRxModeV32(modem, V32_RX_MODE[rate]);
		break;

	case V32_STATE_DONE:
		FIELD_INT(hdx, V32HDX_INT_7C) = 0;
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = FIELD_INT(hdx,
							      V32HDX_INT_80);
		FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_01 | V32_FLAG_04;

		rate = DecodeRateSeq(modem, REG(hdx, 4));
		FIELD_U8(modem, V32_OBJ_STATUS) =
			(unsigned char)V32_CONNECT[rate];
		DEC(FP(modem))->retrain = 0;
		break;

	case V32_STATE_ERROR:
		FIELD_INT(hdx, V32HDX_STATE_LEFT) = FIELD_INT(hdx,
							      V32HDX_INT_80);
		break;

	default:
		break;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("state %s(%d)\n",
				     V32StateName(TRACE_STATE(modem)),
				     TRACE_STATE(modem));
}

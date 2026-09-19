/*
 * v32nsorg.c -- ITU-T V.32 / V.32bis: the ORIGINATE handshake's next-state
 *               function.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V32OrgNextState   .text 0x082be0   2580
 *
 * The blob's own `FILE` symbol for this address is `V32org.c`; this tree
 * splits V.32 by role rather than by the object's translation units, so the
 * name here is the role.  `include/dsplib/v32hdxst.h` is the roster of the
 * twenty-five functions this one belongs to and the reason they link together
 * or not at all.
 *
 * ---------------------------------------------------------------------------
 * WHAT IT IS
 *
 * The half-duplex machine keeps its handshake state at hdx + 0x74 -- the 0..34
 * lettering of `include/dsplib/v32state.h`, which is the Recommendation's own
 * call-setup labels -- and its MODE at hdx + 0x76.  A transmit or receive state
 * that has finished calls `*V32NextState[hdx->mode]`; slot 0 of that table is
 * this function, so this is what advances an ORIGINATING call by one step.
 *
 * Every step does the same four things, in some subset:
 *
 *   1. write the next handshake state to hdx + 0x74;
 *   2. install the next `TxHdx*` at hdx + 0x6c and the next `RxHdx*` at
 *      hdx + 0x70 -- either, both or neither, and a step that installs
 *      neither leaves the previous pair running;
 *   3. reload the step's own duration into hdx + 0x78 and clear the elapsed
 *      count at hdx + 0x7c;
 *   4. reconfigure the datapump for the next step -- the mode setters, the
 *      adaptation switches, the sequence generator and detector.
 *
 * The dispatch is a jump table at `.rodata + 0x7dec` with 34 slots, 0x00..0x21,
 * and the sign-extended state is compared UNSIGNED against 0x21 -- so a
 * negative state, V32_STATE_DONT_CARE (34) and the nine states W..DONE (24..32)
 * all take the default, which does nothing but the trace below.  Slot 0x21,
 * V32_STATE_ERROR, is NOT a default: it has a body, and it is the only body
 * that does not change the state.
 *
 * THE STATE PROGRESSION IS state -> state + 1 THROUGHOUT, with two exceptions
 * that are the object's and not a misreading:
 *
 *   - V32_STATE_V (23) writes 23 again.  It is the CONNECT step -- it posts
 *     V32_CONNECT[rate] as the instance's status -- and both U and V transition
 *     into it.
 *   - V32_STATE_ERROR (33) writes nothing.
 *
 * Three steps are CONDITIONAL and fall through to the trace when the condition
 * fails, leaving the state where it was:
 *
 *   B  (1)  only when hdx + 0x7c is above 0x95f, an UNSIGNED test (`jbe`)
 *   D  (4)  only when hdx + 0xa8 is above 0x48, a SIGNED 16-bit test (`jle`)
 *   S (20)  always advances, but posts a fault when the two ends have no rate
 *           in common
 *
 * ---------------------------------------------------------------------------
 * WHICH TRANSMIT AND RECEIVE STATE EACH STEP INSTALLS
 *
 *   from  to    transmit             receive
 *   ----  ----  -------------------  ------------------
 *   A     B     -                    -
 *   B     B2    TxHdxCarrierState    RxHdxPhsReversal
 *   B2    C     -                    -
 *   C     D     -                    -
 *   D     D2    TxHdxNoCarrier       RxHdxNoSignal
 *   D2    E     TxHdxNoCarrier       RxHdxSTone
 *   E     F     TxHdxNoCarrier       RxHdxEpoch
 *   F     G     TxHdxNoCarrier       RxHdxData
 *   G     H     TxHdxNoCarrier       RxHdxRateSequence
 *   H     I     TxHdxNoCarrier       RxHdxNull
 *   I     J     TxHdxCarrierState    RxHdxData
 *   J     K     TxHdxCarrierState    RxHdxData
 *   K     L     TxHdxTRN             -
 *   L     M     TxHdxScrSequence     -
 *   M     N     TxHdxScrSequence     -
 *   N     O     TxHdxScrSequence     RxHdxSTone
 *   O     P     TxHdxScrSequence     RxHdxEpoch
 *   P     Q     TxHdxScrSequence     RxHdxData
 *   Q     R     TxHdxScrSequence     RxHdxSequence
 *   R     S     -                    -
 *   S     T     TxHdxScrSequence     RxHdxSequence
 *   T     U     -                    -
 *   U     V     -                    RxHdxData
 *   V     V     -                    RxHdxData
 *   ERROR -     -                    -
 *
 * `TxHdxTone`, `TxHdxData`, `TxHdxFinishFrame`, `TxHdxNull`, `RxHdxTone`,
 * `RxHdxSequenceE`, `RxHdxToneData` and `RxHdxError` are NOT reached from here;
 * they belong to the other four dispatchers and to the states themselves.
 *
 * ---------------------------------------------------------------------------
 * THE AUTHOR'S OWN WORDS
 *
 * One format string lands in this function, at `.rodata.str1.1 + 0x37e5`:
 *
 *      "state %s(%d)\n"
 *
 * printed with `V32StateName(hdx->state)` and `hdx->state`, gated on
 * `dsplibs_debug_level > 1`.  It is the tail of the function and every arm
 * falls into it, so it traces the state the step has just moved TO.  Read
 * against `V32StateName`, it is what says hdx + 0x74 is `v32state.h`'s index
 * and not a private numbering.  Nothing else here is named by the object's own
 * text; the fields below that carry `type_NNNN` names are named for their
 * shape because no stronger evidence exists.
 *
 * ---------------------------------------------------------------------------
 * WHY THE CONTEXT POINTER IS RE-READ AT EVERY USE
 *
 * The object reloads obj + 0x64 and obj + 0x68 after every call rather than
 * keeping either in a callee-saved register, which is `V32FP_delete`'s idiom
 * and `v32fpctl.c`'s `HDX()`/`FP()` macros.  Two sites settle that it is the
 * SOURCE and not the scheduler:
 *
 *   - 83274, in the V32_STATE_I arm: the store `mov %ecx,0x78(%esi)` goes
 *     through the context pointer loaded at function ENTRY, and the very next
 *     statement reads the same field through a FRESHLY loaded one.  A single
 *     local would have used one register for both; a macro that re-dereferences
 *     gives exactly this, because the loads either side of one expression may
 *     be hoisted above the call inside it while a new statement may not.
 *   - 82d0f, in the trace: `V32StateName(hdx->state)` is called and only then
 *     is the context re-read for the `%d` argument.
 *
 * So `HDX()` and `FP()` below are macros and not locals, deliberately.
 *
 * ---------------------------------------------------------------------------
 * THE TWO CALLS WHOSE RESULT IS DISCARDED
 *
 * The V32_STATE_I arm calls `LoadReg(modem, 0)` twice: once at 8326e, whose
 * result is added into hdx + 0x78, and once at 832a3, whose result is dead in
 * %eax and never read.  `LoadReg` has no side effect at all -- it is a bounds
 * check and a load -- so the second call is a statement the original author
 * wrote and the compiler could not remove across the call boundary.  It is
 * reproduced because removing it changes the call trace, which is exactly what
 * a differential test of a dispatcher measures.
 */

#include "dsplib/v32hdxst.h"
#include "dsplib/v32struct.h"

#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v32cfg.h"		/* AGC_DEF_ALPHA, AGC_DEF_BETA        */
#include "dsplib/v32demod.h"		/* V32_FLAG_CARRIER, V32_FLAG_SILENCE */
#include "dsplib/v32data.h"		/* V32_OBJ_FP, V32FP_SMC              */
#include "dsplib/v32dec.h"		/* struct v32_dec                     */
#include "dsplib/v32fpctl.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32smc.h"		/* struct v32_smc                     */
#include "dsplib/v32state.h"

/* The instance is `struct v32_modem`; see v32hdx.h.  These are the only accessors. */


/* Re-dereferenced at every use, on purpose -- see the header comment. */
#define HDX(m) ((m)->hdx)
#define FP(m) ((m)->fp)


/*
 * THE HALF-DUPLEX CONTEXT'S FIELDS, and where each name comes from.
 *
 * SIX OF THE EIGHT ARE ALREADY NAMED BY THE STATES THEMSELVES, in
 * `V32TXHDX.c` and `V32rxhdx.c`, and are spelled the same here behind
 * `#ifndef` -- one offset under two names in two files is worse than either
 * name, and the states saw the evidence this function cannot.
 *
 *   +0x78  V32HDX_STATE_LEFT, `V32TXHDX.c`'s: the TRANSMIT states subtract
 *          from it and transition at zero.  Every step here reloads it, from
 *          a literal (8, 0x10, 0x80, 0x100, 0x400, 0x500, 0x1b00, 0x4d0),
 *          from +0x80, from +0x9e, from +0x7c, or computed.  32 bits.
 *   +0x7c  V32HDX_TIMER and
 *   +0x80  V32HDX_LIMIT, `V32rxhdx.c`'s pair: eleven receive states add
 *          `V32HDX_SYMBOL_LEN` to the first and post a fault once it reaches
 *          the second, and all ten of their comparisons are `jb`/`jbe`.  THIS
 *          FUNCTION CORROBORATES THE SIGNEDNESS INDEPENDENTLY: the
 *          V32_STATE_B arm's `cmpl $0x95f,0x7c(%esi)` branches `jbe`, so the
 *          counter is unsigned here too.  Nearly every step clears the
 *          counter, and eight reload the countdown from the bound.
 *   +0x84  V32HDX_BLOCK_CHARGE, `V32TXHDX.c`'s.  The V32_STATE_B2 arm here
 *          loads +0x9e into it and the A and C arms clear it.
 *   +0x90  V32HDX_INT_90, `V32rxhdx.c`'s latch inside `RxHdxPhsReversal`.
 *          The V32_STATE_B arm is one of its two writers and writes zero.
 *   +0x96  V32HDX_RTD, and THE NAME IS THE AUTHOR'S OWN -- `V32rxhdx.c`
 *          records the value being printed through "v32 RTD = %d\n".  The
 *          V32_STATE_D2 arm here parks it in `StoreReg`'s register 0, which
 *          is where V32_STATE_I reads a delay back out of.
 *
 * TWO ARE THIS FILE'S AND ARE MODELLED, UNNAMED -- they wear their offsets:
 *
 *   +0x46  written once, in the V32_STATE_P arm, as +0x80 minus +0x78, and
 *          read once, in V32_STATE_R, back into that same difference.  A
 *          16-bit snapshot of how much of a step was actually used.
 *   +0xa8  cleared in the V32_STATE_C arm and compared `> 0x48` with a SIGNED
 *          16-bit branch in V32_STATE_D.  `V32rxhdx.c` has
 *          `RxHdxPhsReversal` charging `V32HDX_SYMBOL_LEN` into it once per
 *          block, so D waits a fixed number of blocks; what the 0x48 MEANS is
 *          not established and the name stays neutral.
 */
#ifndef V32HDX_SHORT_46
#define V32HDX_SHORT_46		0x46	/* short                              */
#endif
#ifndef V32HDX_STATE_LEFT
#define V32HDX_STATE_LEFT	0x78	/* int, <= 0 transitions              */
#endif
#ifndef V32HDX_TIMER
#define V32HDX_TIMER		0x7c	/* unsigned int                       */
#endif
#ifndef V32HDX_LIMIT
#define V32HDX_LIMIT		0x80	/* unsigned int, read-only here       */
#endif
#ifndef V32HDX_BLOCK_CHARGE
#define V32HDX_BLOCK_CHARGE	0x84	/* short                              */
#endif
#ifndef V32HDX_INT_90
#define V32HDX_INT_90		0x90	/* int, cleared here                  */
#endif
#ifndef V32HDX_RTD
#define V32HDX_RTD		0x96	/* short; the author's own name       */
#endif
#ifndef V32HDX_SHORT_A8
#define V32HDX_SHORT_A8		0xa8	/* short                              */
#endif

/*
 * MODELLED, UNNAMED -- a second flag byte on the INSTANCE, next to
 * `V32_OBJ_STATUS` (0x30) and `V32_OBJ_FLAGS` (0x31).
 *
 * Exactly two instructions in the whole 1.2 MB object touch it and both are
 * `orb $0x8`: 83439 here and 86265 in `V32AnsNextState`.  Nothing reads it
 * anywhere, and no format string prints it, so there is nothing to name it
 * from and the bit is written as its value.
 */
#ifndef V32_OBJ_U8_32
#define V32_OBJ_U8_32		0x32	/* unsigned char                      */
#endif

/*
 * MODELLED, UNNAMED -- the fixed-point AGC inside the datapump block.
 *
 * `FPM_AGC_Freeze` (8303a) and `FPM_AGC_Release` (83138) are both handed
 * fp + 0x1d8, which is what types it; the two pointer stores at +0x1e4 and
 * +0x1e8 then land on `fpm_agc_cfg`'s `alpha` and `beta` exactly.
 */
#ifndef V32FP_AGC
#define V32FP_AGC		0x1d8	/* struct fpm_agc                     */
#endif

/*
 * MODELLED, UNNAMED -- the two reason codes posted into obj + 0x30.
 *
 * 0x0f is this function's own, from the V32_STATE_D2 step, and nothing in the
 * object explains it.  0x17 is numerically `v32fpctl.h`'s V32_STATUS_BAD_MODE
 * and is spelled by value here for `v32nsrng.c`'s reason: that name was
 * derived from `SetTxModeV32`'s out-of-range arm, and this is a different site
 * with a different cause -- no rate in common -- so reusing the name would
 * assert an identity the object does not state.
 */
#define V32_STATUS_0F		0x0f
#define V32_STATUS_17		0x17

/*
 * The bits of `V32_OBJ_FLAGS` this file writes.
 *
 * 0x20 and 0x40 are `v32demod.h`'s V32_FLAG_CARRIER and V32_FLAG_SILENCE, and
 * the V32_STATE_D2 step writes both in ONE read-modify-write, clearing SILENCE
 * and setting CARRIER -- which is exactly what `V32LocLoopNextState`'s state G
 * does.  0x02 is V32_FLAG_FAULT.  The other three are `v32nsrng.c`'s unnamed
 * bits, spelled by value there and here: the CONNECT step sets all three at
 * once as 0x19, and nothing in the object says what any of them indicates.
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

/*
 * The sequence detector's patterns.  The first three are `v32nsrng.c`'s names
 * for the same three literals; the AC pair is this file's, from the
 * V32_STATE_E step, and is the only detector arming in the batch that is not
 * one of the rate or E sequences.
 */
#define V32_DET_TARGET_AC	0x0000cc0c
#define V32_DET_MASK_AC		0x0000fc0f
#define V32_DET_SEQ		0x01110111
#define V32_DET_ESEQ		0x0111f111
#define V32_DET_MASK		((int)0xf111f111)

/* The AA/CC detector's frequency, in Hz, as `SetToneDetect` takes it. */
#define V32_TONE_AA_HZ		600

void
V32OrgNextState(struct v32_modem *modem)
{
	switch (HDX(modem)->state) {

	case V32_STATE_A:
		HDX(modem)->state = V32_STATE_B;
		HDX(modem)->state_left =
			(int)HDX(modem)->limit;
		HDX(modem)->timer = 0;
		HDX(modem)->block_charge = 0;
		break;

	case V32_STATE_B:
		/* UNSIGNED: the object branches `jbe`, not `jle`. */
		if (HDX(modem)->timer > 0x95fu) {
			struct fpm_tone *tone;

			HDX(modem)->state = V32_STATE_B2;
			InitGenSequence(modem, 0, 4, 2);

			tone = (struct fpm_tone *)
				HDX(modem)->tone0;
			tone->cfg.f08 = 0x4000;
			tone->cfg.ratio = 0x6666;
			tone->cfg.min_level = (short)(tone->cfg.min_level >> 1);
			SetToneDetect(modem, V32_TONE_AA_HZ);

			HDX(modem)->int_90 = 0;
			HDX(modem)->tx_state =
				(void *)TxHdxCarrierState;
			HDX(modem)->rx_state =
				(void *)RxHdxPhsReversal;
			HDX(modem)->state_left =
				(int)HDX(modem)->limit;
			HDX(modem)->timer = 0;
		}
		break;

	case V32_STATE_B2:
		HDX(modem)->state = V32_STATE_C;
		HDX(modem)->state_left =
			(int)HDX(modem)->timer;
		/*
		 * `movzwl` and not `movswl`: the extension is dead -- the value
		 * is stored straight back as sixteen bits -- so per finding
		 * F7803 it follows the LOCAL's type and not the field's, and
		 * `v32hdx.h` keeps +0x9e a `short`.
		 */
		HDX(modem)->block_charge =
			(unsigned short)HDX(modem)->symbol_len;
		break;

	case V32_STATE_C:
		/* The call comes FIRST here; the state store is at 834fa. */
		InitGenSequence(modem, 0xf, 4, 2);
		HDX(modem)->state = V32_STATE_D;
		HDX(modem)->state_left =
			(int)HDX(modem)->limit;
		HDX(modem)->timer = 0;
		HDX(modem)->block_charge = 0;
		HDX(modem)->short_a8 = 0;
		break;

	case V32_STATE_D:
		/* SIGNED, and 16-bit: `cmpw $0x48` then `jle`. */
		if (HDX(modem)->short_a8 > 0x48) {
			HDX(modem)->state = V32_STATE_D2;
			HDX(modem)->tx_state =
				(void *)TxHdxNoCarrier;
			HDX(modem)->rx_state =
				(void *)RxHdxNoSignal;
			HDX(modem)->state_left =
				(int)HDX(modem)->limit;
		}
		break;

	case V32_STATE_D2: {
		struct fpm_tone *tone;

		modem->byte_32 |= 0x08;
		modem->status = V32_STATUS_0F;
		modem->flags = (unsigned char)
			((modem->flags
			  & ~(unsigned)V32_FLAG_SILENCE)
			 | V32_FLAG_CARRIER);

		StoreReg(modem, HDX(modem)->rtd, 0);

		HDX(modem)->state = V32_STATE_E;
		HDX(modem)->state_left =
			(int)HDX(modem)->limit;
		HDX(modem)->timer = 0;
		HDX(modem)->tx_state =
			(void *)TxHdxNoCarrier;
		HDX(modem)->rx_state = (void *)RxHdxSTone;

		tone = (struct fpm_tone *)HDX(modem)->tone0;
		tone->cfg.min_level = (short)(tone->cfg.min_level >> 1);
		break;
	}

	case V32_STATE_E:
		HDX(modem)->state = V32_STATE_F;
		HDX(modem)->state_left =
			(int)HDX(modem)->limit;
		HDX(modem)->timer = 0;
		HDX(modem)->tx_state =
			(void *)TxHdxNoCarrier;
		HDX(modem)->rx_state = (void *)RxHdxEpoch;
		SetRxModeV32(modem, V32_MODE_ABS4);
		SetRxLoopsV32(modem, 2);
		InitDetSequence(modem, V32_DET_TARGET_AC, V32_DET_MASK_AC,
				0xffff, 2);
		break;

	case V32_STATE_F:
		HDX(modem)->state = V32_STATE_G;
		HDX(modem)->state_left = 0x500;
		HDX(modem)->timer = 0;
		HDX(modem)->tx_state =
			(void *)TxHdxNoCarrier;
		HDX(modem)->rx_state = (void *)RxHdxData;
		SetAdaptEqV32(modem, V32_ADAPTEQ_MU0);
		/* The TRACKING pair; the V32_STATE_N step installs [0]. */
		FP(modem)->agc.cfg.alpha = &AGC_DEF_ALPHA[1];
		FP(modem)->agc.cfg.beta = &AGC_DEF_BETA[1];
		break;

	case V32_STATE_G:
		HDX(modem)->state = V32_STATE_H;
		HDX(modem)->state_left =
			(int)HDX(modem)->limit;
		HDX(modem)->timer = 0;
		HDX(modem)->tx_state =
			(void *)TxHdxNoCarrier;
		HDX(modem)->rx_state =
			(void *)RxHdxRateSequence;
		SetRxModeV32(modem, V32_MODE_DIF4);
		SetAdaptEqV32(modem, V32_ADAPTEQ_MU1);
		InitDetSequence(modem, V32_DET_SEQ, V32_DET_MASK, -1, 2);
		/*
		 * EASY TO MISS: this arm reaches the trace by jumping into the
		 * V32_STATE_Q arm at 83010, three instructions before that
		 * arm's own `InitDetSequence` -- so it shares the FREEZE that
		 * follows as well as the detector arming.  The two arms differ
		 * only in the detector's out_mask, -1 here and 0xffff there.
		 */
		FPM_AGC_Freeze(&FP(modem)->agc);
		break;

	case V32_STATE_H:
		HDX(modem)->state = V32_STATE_I;
		HDX(modem)->state_left =
			HDX(modem)->symbol_len;
		HDX(modem)->timer = 0;
		HDX(modem)->tx_state =
			(void *)TxHdxNoCarrier;
		HDX(modem)->rx_state = (void *)RxHdxNull;
		break;

	case V32_STATE_I:
		HDX(modem)->state = V32_STATE_J;
		HDX(modem)->state_left =
			0x100 + HDX(modem)->turnaround
			+ LoadReg(modem, 0);
		/* Rounded UP to even. */
		if (HDX(modem)->state_left & 1)
			HDX(modem)->state_left++;
		HDX(modem)->timer = 0;
		/* Result dead in the object; see the header comment. */
		LoadReg(modem, 0);
		HDX(modem)->tx_state =
			(void *)TxHdxCarrierState;
		HDX(modem)->rx_state = (void *)RxHdxData;
		SetAdaptEqV32(modem, V32_ADAPTEQ_OFF);
		SetRxLoopsV32(modem, 1);
		StoreReg(modem, (short)GetSequence(modem), 1);
		SetTxModeV32(modem, V32_MODE_ABS4);
		InitGenSequence(modem, 1, 4, 2);
		break;

	case V32_STATE_J:
		HDX(modem)->state = V32_STATE_K;
		HDX(modem)->state_left = 0x10;
		HDX(modem)->timer = 0;
		HDX(modem)->tx_state =
			(void *)TxHdxCarrierState;
		HDX(modem)->rx_state = (void *)RxHdxData;
		InitGenSequence(modem, 0xe, 4, 2);
		SetECRndTripDelayV32(modem, LoadReg(modem, 0));
		break;

	case V32_STATE_K:
		HDX(modem)->state = V32_STATE_L;
		HDX(modem)->state_left = 0x100;
		HDX(modem)->timer = 0;
		HDX(modem)->tx_state =
			(void *)TxHdxTRN;
		InitGenSequence(modem, 0xf, 4, 2);
		SeedScramblerV32(modem, 0);
		break;

	case V32_STATE_L:
		HDX(modem)->state = V32_STATE_M;
		HDX(modem)->state_left = 0x400;
		HDX(modem)->timer = 0;
		HDX(modem)->tx_state =
			(void *)TxHdxScrSequence;
		SetAdaptEcV32(modem, V32_ADAPTEC_ON);
		break;

	case V32_STATE_M:
		HDX(modem)->state = V32_STATE_N;
		HDX(modem)->state_left = 0x1b00;
		HDX(modem)->timer = 0;
		HDX(modem)->tx_state =
			(void *)TxHdxScrSequence;
		SetAdaptEcV32(modem, V32_ADAPTEC_SLOW);
		break;

	case V32_STATE_N: {
		unsigned short seq;
		struct fpm_mtd *mtd;

		HDX(modem)->state = V32_STATE_O;
		HDX(modem)->state_left =
			(int)HDX(modem)->limit;
		HDX(modem)->timer = 0;
		HDX(modem)->tx_state =
			(void *)TxHdxScrSequence;
		SetAdaptEcV32(modem, V32_ADAPTEC_OFF);

		seq = CodeRateSeq(modem, (unsigned short)LoadReg(modem, 1));
		StoreReg(modem, (short)seq, 2);

		HDX(modem)->rx_state = (void *)RxHdxSTone;

		/*
		 * The detector is re-created FROM ITS OWN CONFIGURATION, which
		 * works because `struct fpm_mtd_cfg` is the first member of
		 * `struct fpm_mtd` and create copies it before touching
		 * anything -- the same trick `SetAdaptEcV32`'s reset arm plays
		 * on the echo canceller.  The object loads hdx + 0x38 TWICE,
		 * once before the coefficient store and once after, because
		 * that store is through a pointer that might alias the field
		 * it came from; both loads are kept.
		 */
		mtd = (struct fpm_mtd *)HDX(modem)->mtd;
		mtd->cfg.coeff = V32_S_DATA_COEF;
		FPM_MTD_create((struct fpm_mtd *)
			       HDX(modem)->mtd, &mtd->cfg);

		InitGenSequence(modem, seq, 0x10, 2);
		InitDetSequence(modem, V32_DET_SEQ, V32_DET_MASK,
				0xffff, 2);
		SetTxModeV32(modem, V32_MODE_DIF4);
		FPM_AGC_Release(&FP(modem)->agc);
		/* The ACQUISITION pair; V32_STATE_F and P install [1]. */
		FP(modem)->agc.cfg.alpha = &AGC_DEF_ALPHA[0];
		FP(modem)->agc.cfg.beta = &AGC_DEF_BETA[0];
		break;
	}

	case V32_STATE_O:
		/* The only step that does not touch +0x78. */
		HDX(modem)->state = V32_STATE_P;
		HDX(modem)->timer = 0;
		HDX(modem)->tx_state =
			(void *)TxHdxScrSequence;
		HDX(modem)->rx_state = (void *)RxHdxEpoch;
		SetRxModeV32(modem, V32_MODE_ABS4);
		SetRxLoopsV32(modem, 2);
		FPM_SRE_init(&FP(modem)->sre, &SREv32_CFG, 0);
		FP(modem)->fse.sym_count = 0;
		break;

	case V32_STATE_P:
		HDX(modem)->state = V32_STATE_Q;
		/* Reads +0x78 before the reload below overwrites it. */
		HDX(modem)->short_46 = (short)
			((int)HDX(modem)->limit
			 - HDX(modem)->state_left);
		HDX(modem)->state_left = 0x4d0;
		HDX(modem)->timer = 0;
		HDX(modem)->tx_state =
			(void *)TxHdxScrSequence;
		HDX(modem)->rx_state = (void *)RxHdxData;
		SetAdaptEqV32(modem, V32_ADAPTEQ_MU0);
		FP(modem)->agc.cfg.alpha = &AGC_DEF_ALPHA[1];
		FP(modem)->agc.cfg.beta = &AGC_DEF_BETA[1];
		break;

	case V32_STATE_Q:
		HDX(modem)->state = V32_STATE_R;
		HDX(modem)->state_left =
			(int)HDX(modem)->limit;
		HDX(modem)->timer = 0;
		HDX(modem)->tx_state =
			(void *)TxHdxScrSequence;
		HDX(modem)->rx_state = (void *)RxHdxSequence;
		SetAdaptEqV32(modem, V32_ADAPTEQ_MU1);
		InitDetSequence(modem, V32_DET_SEQ, V32_DET_MASK,
				0xffff, 2);
		FPM_AGC_Freeze(&FP(modem)->agc);
		break;

	case V32_STATE_R:
		HDX(modem)->state = V32_STATE_S;
		/*
		 * Align what is left of the step onto an eight-symbol
		 * boundary: `8 - (x & 7)`, never zero and never above eight.
		 */
		HDX(modem)->state_left = 8 -
			((0x4d0 + ((int)HDX(modem)->limit
				   - HDX(modem)->state_left)
			  + HDX(modem)->short_46) & 7);
		StoreReg(modem, (short)GetSequence(modem), 3);
		break;

	case V32_STATE_S:
		HDX(modem)->state = V32_STATE_T;
		HDX(modem)->state_left = 8;
		HDX(modem)->timer = 0;
		HDX(modem)->tx_state =
			(void *)TxHdxScrSequence;
		HDX(modem)->rx_state = (void *)RxHdxSequence;
		/*
		 * NO RATE IN COMMON, and the step CONTINUES either way: the
		 * object posts the two bytes at 835e7 and jumps BACK into the
		 * flow at 82c57 rather than returning.  See V32_STATUS_17
		 * above for why the reason code is spelled by value.
		 */
		if (DecodeRateSeq(modem, (unsigned short)LoadReg(modem, 3))
		    == V32_RATE_NONE) {
			modem->flags |= V32_FLAG_FAULT;
			modem->status = V32_STATUS_17;
		}
		SetAdaptEcV32(modem, V32_ADAPTEC_OFF);
		InitGenSequence(modem,
				CodeESeq(modem,
					 (unsigned short)LoadReg(modem, 3)),
				0x10, 2);
		InitDetSequence(modem, V32_DET_ESEQ, V32_DET_MASK, -1, 2);
		break;

	case V32_STATE_T: {
		unsigned short rate;

		HDX(modem)->state = V32_STATE_U;
		HDX(modem)->state_left =
			(int)HDX(modem)->limit;
		HDX(modem)->timer = 0;

		rate = (unsigned short)
			DecodeRateSeq(modem,
				      (unsigned short)LoadReg(modem, 3));
		SetTxModeV32(modem, V32_TX_MODE[rate]);
		InitGenSequence(modem, 0xffff, 0x10, 8);
		/* UNSIGNED 16-bit: `cmp $0x2,%si` then `jbe`. */
		if (rate > 2)
			FP(modem)->tx_smc.trellis_state = 0;
		break;
	}

	case V32_STATE_U: {
		short seq;

		HDX(modem)->state = V32_STATE_V;
		HDX(modem)->state_left = 0x80;
		HDX(modem)->timer = 0;
		HDX(modem)->rx_state = (void *)RxHdxData;

		seq = (short)GetSequence(modem);
		StoreReg(modem, seq, 4);
		SetRxModeV32(modem,
			     V32_RX_MODE[DecodeRateSeq(modem,
						       (unsigned short)seq)]);
		break;
	}

	case V32_STATE_V: {
		unsigned short rate;

		/* The CONNECT step, and it stays in V32_STATE_V. */
		HDX(modem)->state = V32_STATE_V;
		HDX(modem)->state_left =
			(int)HDX(modem)->limit;
		HDX(modem)->timer = 0;
		HDX(modem)->rx_state = (void *)RxHdxData;
		modem->flags |=
			(V32_FLAG_01 | V32_FLAG_08 | V32_FLAG_10);

		rate = (unsigned short)
			DecodeRateSeq(modem,
				      (unsigned short)LoadReg(modem, 3));
		/*
		 * `movzwl` into a byte store: the extension is dead, so per
		 * F7803 it is the LOCAL that is `unsigned short` and not
		 * `V32_CONNECT`, which `v32hdxst.h` keeps as `short`.
		 */
		modem->status =
			(unsigned char)(unsigned short)V32_CONNECT[rate];
		((struct v32_dec *)FP(modem)->fse.cfg.owner)->retrain = 0;
		SetAdaptEqV32(modem, V32_ADAPTEQ_MU1);
		SetAdaptEcV32(modem, V32_ADAPTEC_OFF);
		break;
	}

	case V32_STATE_ERROR:
		/* The one arm that does not advance the state. */
		HDX(modem)->state_left =
			(int)HDX(modem)->limit;
		break;

	default:
		break;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("state %s(%d)\n",
				     V32StateName(HDX(modem)->state),
				     HDX(modem)->state);
}

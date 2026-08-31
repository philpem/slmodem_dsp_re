/*
 * v32nsans.c -- ITU-T V.32 / V.32bis: the ANSWER side's handshake step.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V32AnsNextState   .text 0x085ab0   2539     (the blob's own `V32ans.c`)
 *
 * `V32NextState[V32_MODE_ANSWER]` is this function, so it is what a transmit
 * or receive state reaches through `call *V32NextState[hdx->mode]` when it has
 * finished.  One call advances the handshake by exactly one step: it writes
 * the next state number to hdx + V32HDX_STATE, installs the `TxHdx*` that
 * sends the next signal at hdx + V32HDX_TXSTATE and the `RxHdx*` that looks
 * for the next one at hdx + V32HDX_RXSTATE, sets the budgets the two states
 * count against, and reconfigures the datapump for whatever the new step
 * needs.  `include/dsplib/v32hdx.h` is the contract those states are written
 * to and `include/dsplib/v32hdxst.h` is the roster.
 *
 * ---------------------------------------------------------------------------
 * IT IS A `switch` ON THE HANDSHAKE STATE, AND THE ARMS ARE ENUMERATED
 *
 * The dispatch is
 *
 *      85ac2  movswl 0x74(%ebx),%eax
 *      85ac6  cmp    $0x21,%eax
 *      85ac9  ja     <default>
 *      85acb  jmp    *.rodata+0x8068(,%eax,4)
 *
 * -- a 34-entry jump table, so the switch's own range is 0..33 and the `ja` is
 * unsigned, which sends a negative state to the default as well.  FIVE OF THE
 * THIRTY-FOUR SLOTS POINT AT THE DEFAULT LABEL, and that is not a hole in the
 * reading: it is how GCC fills a dense table for cases the source did not
 * write.  So the author's arms are known exactly, and they are
 *
 *   A B B2 C D  E F G H I J K L M N O P Q  T U V W X Y Z END F2 X2  ERROR
 *
 * with D2 (5), R (19), S (20), CLEARDOWN (31), DONE (32) and DONT_CARE (34)
 * ABSENT -- 34 is past the table entirely and takes the same default.
 *
 * The chain the arms form, each labelled with what it installs:
 *
 *   A     -> B     tx -             rx -              (SetToneDetect 0)
 *   B     -> B2    tx CarrierState  rx -              (SetToneDetect 1800)
 *   B2    -> C     tx -             rx -           (or -> ERROR, see below)
 *   C     -> D     tx -             rx PhsReversal
 *   D     -> E     tx -             rx NoSignal       (SetToneDetect 1800)
 *   E     -> F     tx -             rx -
 *   F     -> F2    tx NoCarrier     rx Data
 *   G     -> H     tx CarrierState  rx -
 *   H     -> I     tx CarrierState  rx -
 *   I     -> J     tx TRN           rx -
 *   J     -> K     tx ScrSequence   rx -
 *   K     -> L     tx -             rx -
 *   L     -> M     tx -             rx STone
 *   M     -> N     tx NoCarrier     rx Data
 *   N     -> O     tx -             rx STone
 *   O     -> P     tx -             rx Epoch
 *   P     -> Q     tx NoCarrier     rx Data
 *   Q     -> T     tx -             rx RateSequence
 *   T     -> U     tx CarrierState  rx Data
 *   U     -> V     tx CarrierState  rx -
 *   V     -> W     tx TRN           rx -
 *   W     -> X     tx ScrSequence   rx -
 *   X     -> X2    tx ScrSequence   rx Sequence
 *   X2    -> Y     tx -             rx -
 *   Y     -> Z     tx ScrSequence   rx Data
 *   Z     -> END   tx ScrSequence   rx Data
 *   END   -> END   tx -             rx -
 *   F2    -> G     tx FinishFrame   rx -
 *   ERROR -> ERROR tx -             rx -
 *
 * END TRANSITIONING TO ITSELF IS THE OBJECT'S OWN TEXT, not a misreading of
 * the table: slot 28 reaches 0x85bca and 0x85bca stores 0x1c back into
 * hdx + 0x74.  Z reaches END, and a further call in END re-posts the connect
 * status and re-arms the equaliser rather than moving on.
 *
 * THE ONLY BRANCH OUT OF THE CHAIN IS B2's: if the transmit budget has already
 * run out when B2 is reached, the arm installs `TxHdxNoCarrier` /
 * `RxHdxError`, posts V32_FLAG_FAULT with status 0x16, and goes to ERROR
 * (0x8647a).  Six other arms are CONDITIONAL and simply do nothing until their
 * condition holds -- A, B2, C, D, E and F -- which is what makes this a
 * "poll me again" step function rather than a table walk.
 *
 * ---------------------------------------------------------------------------
 * THE FOUR CONTEXT COUNTERS, AND WHY THEY ARE NOT ALL THE SAME TYPE
 *
 * Four 32-bit fields of the context are driven from here and none of them was
 * modelled before.  They are MODELLED, UNNAMED -- their shape and their
 * signedness are read off the object, their meaning is usage inference, and
 * CLAUDE.md's rule is that a wrong name is worse than a numbered one:
 *
 *   +0x78  int           the TRANSMIT budget.  Every arm that installs a
 *                        `TxHdx*` sets it, and the transmit states SUBTRACT
 *                        from it -- `sub %ecx,0x78(%edx)` at 7fdf8, 7fec7,
 *                        7ffb7, 80098 and 80231, five of the eight.  Four arms
 *                        here test it and they test it SIGNED (`test`/`jg` at
 *                        86451, 863c3, 862b7 and `jle` at 863c8), so it is
 *                        `int` and it is expected to go negative.
 *   +0x7c  unsigned int  the RECEIVE counter.  The receive states ADD into it
 *                        (`add %eax,0x7c(%edx)` at 83925, 83a1f, 83b95 and six
 *                        more) and compare it against +0x80.  EVERY compare on
 *                        it in the object is UNSIGNED -- `jb` at 83978, 83a9c,
 *                        83cc3, `jbe` at 83a73 and at 86361 here -- so it is
 *                        unsigned, and that is forced rather than chosen.
 *   +0x80  unsigned int  the limit +0x7c is measured against, and the value
 *                        eight arms here load into +0x78.  `V32FP_recreate`
 *                        (7f116) is the only writer: it stores
 *                        (obj + 0x8) * 0x4ccc >> 13, once, at construction.
 *   +0x90  int           written 1 by the C and D arms; `RxHdxPhsReversal`
 *                        (83b9b, 83bf2) reads and rewrites it.  Nothing here
 *                        reads it back, so it is an output of this function
 *                        and an input to that state, and nothing more.
 *
 * and three 16-bit ones:
 *
 *   +0x96  short   read once, by the F arm, and handed to `StoreReg`.
 *                  `RxHdxRateSequence` (83c94, 83d73, 83d8c) writes it.
 *   +0xa8  short   zeroed by the C arm, tested `> 0x48` SIGNED by the D arm
 *                  (`cmpw`/`jle` at 86305), counted up by `RxHdxPhsReversal`
 *                  (83b87 movzwl / 83ba1).
 *   +0xaa  short   zeroed by the E arm, tested `> 0x5f` SIGNED by the F arm,
 *                  counted up by `RxHdxNoSignal` (83a22 / 83a30).
 *
 * The two counters are read with `movzwl` by the states that increment them
 * and compared with a SIGNED `cmpw` here.  Both are dead extensions on values
 * this small (F614), so `short` is what the compare forces and the increment
 * does not contradict.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE AUTHOR'S OWN WORDS SAY, WHICH IS ONE STRING
 *
 * The tail is `.rodata.str1.1+0x397f`, "state %s(%d)\n", printed through
 * `dsplibs_debug_printf` under `dsplibs_debug_level > 1`.  It names nothing
 * this function did not already name -- the arguments are `V32StateName` of
 * the new state and the new state -- but it does settle that hdx + 0x74 is
 * the thing `V32StateName` indexes, which is `v32state.h`'s table, and that
 * the print happens AFTER the transition on every path including the ones
 * that changed nothing.  `tools/relocscan.py --at .rodata.str1.1:0x397f`
 * finds the referrers; the string is shared with `V32OrgNextState`'s twin at
 * 0x396f only in text, not in address.
 *
 * THE STATE IS RE-READ TWICE IN THE TAIL (85b28 and 85b37, each preceded by
 * its own load of obj + 0x64), so the print is not passed a cached value.
 *
 * ---------------------------------------------------------------------------
 * THE CONTEXT POINTER IS CACHED AND ALSO RE-READ, AND BOTH ARE THE OBJECT'S
 *
 * `mov 0x64(%esi),%ebx` at 85abf loads it once for the switch, and %ebx stays
 * live through every arm.  Eight arms nevertheless reload obj + 0x64 after an
 * external call -- 86440, 863b2, 862f4, 8628d, 86079, 8604d, 85c6e, 86440 --
 * and the M arm (86065) uses BOTH in one statement: the reloaded pointer for
 * +0x7c, +0x6c, +0x70 and +0x94, and the cached one for the +0x78 store.
 * That is not something a compiler invents, so this file spells the reloads
 * as `HDX(modem)` and the cached uses as `hdx`, exactly where the object puts
 * them.  Behaviourally the two are one pointer -- nothing called from here
 * writes obj + 0x64 -- so no test can separate them and only tier 3 can.
 *
 * ---------------------------------------------------------------------------
 * THE ARMS SHARE TAILS BECAUSE THE COMPILER MERGED THEM, NOT BECAUSE THE
 * SOURCE DID
 *
 * Six arms jump into the middle of another arm's body: H into U (861ec ->
 * 85ad8), P into X (85fe7 -> 85e14), J and L into V (86190, 8616b -> 85e96 /
 * 85e9a), G into U (8624c -> 85b04) and A into B (8646f -> 86438).  Every one
 * of those is a common suffix of two arms, which is GCC's cross-jumping.  The
 * source below writes each arm out in full; the merge is the compiler's to
 * redo.
 *
 * ---------------------------------------------------------------------------
 * ONE CORRECTION TO src/pump/v32/v32hdx_tables.c
 *
 * That file says `V32_CONNECT`'s entries are "handshake state numbers ...
 * copied into hdx + 0x74".  They are not copied there by anybody.  All five
 * sites in the object -- 82d99 (`V32OrgNextState`), 84d16 and 853c3
 * (the two ring dispatchers), 85c12 (here) and 86591 (`V32LocLoopNextState`)
 * -- store the entry as a BYTE into obj + 0x30, which is `V32_OBJ_STATUS`.
 * So it is a per-rate STATUS code and not a state number, and the value range
 * happening to fall inside 0..34 is a coincidence the reading leaned on.
 * Finding F8585.
 */

#include "dsplib/v32hdxst.h"

#include "dsplib/v32state.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32data.h"
#include "dsplib/v32demod.h"
#include "dsplib/v32dec.h"
#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_tone.h"

/* ------------------------------------------------------------------------ */
/* The instance is not modelled; see v32fpctl.h.  These are the accessors.   */

#define FIELD(obj, off)		((unsigned char *)(void *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_INT(obj, off)	(*(int *)(void *)FIELD((obj), (off)))
#define FIELD_U32(obj, off)	(*(unsigned int *)(void *)FIELD((obj), (off)))
#define FIELD_S16(obj, off)	(*(short *)(void *)FIELD((obj), (off)))
#define FIELD_U8(obj, off)	(*(unsigned char *)FIELD((obj), (off)))

/*
 * The two state slots are typed rather than written through `void *`, because
 * converting a function pointer to `void *` is not something C defines and
 * the typedefs for both already exist in v32hdx.h.
 */
#define FIELD_TXFN(obj, off)	(*(v32_txhdx_fn *)(void *)FIELD((obj), (off)))
#define FIELD_RXFN(obj, off)	(*(v32_rxhdx_fn *)(void *)FIELD((obj), (off)))

#define HDX(m)			FIELD_PTR((m), V32_OBJ_HDX)
#define FP(m)			FIELD_PTR((m), V32_OBJ_FP)

#define TONE0(h)		((struct fpm_tone *)FIELD_PTR((h), \
							      V32_HDX_TONE0))
#define MTD(h)			((struct fpm_mtd *)FIELD_PTR((h), V32_HDX_MTD))
#define AGC(f)			((struct fpm_agc *)(void *)FIELD((f), \
								 V32FP_AGC))
#define FSE(f)			((struct fpm_fse *)(void *)FIELD((f), \
								 V32FP_FSE))
#define DEC(f)			((struct v32_dec *)FSE(f)->cfg.owner)

/* ------------------------------------------------------------------------ */
/* The context fields this function drives that no header owns yet.          */

/*
 * MODELLED, UNNAMED.  The header comment above carries the derivation of each
 * one and of its signedness; nothing in the object names any of them, so they
 * wear their offsets in the tree's `type_NNNN` spelling rather than a role.
 * Guarded because they are the obvious names for these offsets and the other
 * halves of the half-duplex machine are being written alongside this file.
 */
#ifndef V32_HDX_INT_78
#define V32_HDX_INT_78		0x78	/* int, the transmit budget           */
#endif
#ifndef V32_HDX_U32_7C
#define V32_HDX_U32_7C		0x7c	/* unsigned, the receive counter      */
#endif
#ifndef V32_HDX_U32_80
#define V32_HDX_U32_80		0x80	/* unsigned, its limit                */
#endif
#ifndef V32_HDX_INT_90
#define V32_HDX_INT_90		0x90	/* int, read by RxHdxPhsReversal      */
#endif
#ifndef V32_HDX_SHORT_96
#define V32_HDX_SHORT_96	0x96	/* short, written by RxHdxRateSequence*/
#endif
#ifndef V32_HDX_SHORT_A8
#define V32_HDX_SHORT_A8	0xa8	/* short, RxHdxPhsReversal's count    */
#endif
#ifndef V32_HDX_SHORT_AA
#define V32_HDX_SHORT_AA	0xaa	/* short, RxHdxNoSignal's count       */
#endif

/*
 * MODELLED, UNNAMED -- two more bytes of the instance.
 *
 * +0x11 is an option byte.  Bit 0x02 is tested HERE, in the B arm, and picks
 * the longer of two transmit budgets; bit 0x04 is tested by `V32FP_recreate`
 * (7f38e, 7f49f) and by `RxHdxTone` (839c4), and `V32FP_control` (845a2)
 * rewrites the byte.  Nothing prints it and nothing types it, so what the
 * option IS is not established and the bit keeps its value for a name.
 *
 * +0x32 is a second flags byte, distinct from V32_OBJ_FLAGS at +0x31.  Only
 * two sites in the whole object touch it -- `orb $0x8` here at 86265 and the
 * same instruction at 83439 inside `RxHdxSTone` -- and neither says what the
 * bit means.
 */
#ifndef V32_OBJ_U8_11
#define V32_OBJ_U8_11		0x11
#endif
#ifndef V32_OPT11_02
#define V32_OPT11_02		0x02
#endif
#ifndef V32_OBJ_U8_32
#define V32_OBJ_U8_32		0x32
#endif
#ifndef V32_BIT32_08
#define V32_BIT32_08		0x08
#endif

/*
 * Three of the eight bits of V32_OBJ_FLAGS, ORed in together by the END arm.
 * `src/pump/v32/v32hshake.c` spells 0x01 and 0x08 the same way and for the
 * same reason: V32_FLAG_FAULT (0x02), V32_FLAG_CARRIER (0x20) and
 * V32_FLAG_SILENCE (0x40) are the three that ARE named and these are not.
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
 * Status codes posted at V32_OBJ_STATUS.  V32_STATUS_BAD_MODE (0x17) is
 * v32fpctl.h's and is what the T arm posts when the two stations have no rate
 * in common; 0x16 is what the B2 arm posts when the transmit budget ran out
 * and 0x0f is what the F arm posts on the way into F2.  The receive states
 * post 0x10, 0x11 and 0x12 from their own failures, so the byte is a dense
 * per-site reason code -- which is all that can be said about these two.
 */
#define V32_ANS_STATUS_TIMEOUT	0x16
#define V32_ANS_STATUS_CARRIER	0x0f

/*
 * The detector thresholds this function writes straight into
 * `fpm_tone_cfg::ratio`, which fpm_tone.h types "the share of the energy the
 * tone must hold, Q15".  The field is the callee's, so the TYPE is evidence
 * grade 2; the two values are the object's literals and are not named further.
 */
#define V32_ANS_TONE_RATIO_HI	0x747a
#define V32_ANS_TONE_RATIO_LO	0x4a38

/* The answer side listens for 1800 Hz -- V.32's AC signal. */
#define V32_ANS_TONE_HZ		1800

/*
 * `InitDetSequence`'s two armings.  Both mask with 0xf111f111 and leave the
 * whole register (`out_mask` of -1); the target is what differs, and the
 * nibbles are the Recommendation's rate-sequence framing rather than anything
 * this function computes.
 */
#define V32_ANS_DET_MASK	0xf111f111
#define V32_ANS_DET_TARGET_Q	0x01110111
#define V32_ANS_DET_TARGET_X	0x0111f111

/* ------------------------------------------------------------------------ */

/*
 * Advance the answering station's handshake by one step.
 *
 * Nothing is returned: every call site is `push modem; call *V32NextState[m]`
 * with %eax dead afterwards, and the function's own exits set nothing.
 */
void
V32AnsNextState(void *modem)
{
	unsigned char *hdx = (unsigned char *)HDX(modem);
	unsigned char *h2;
	short seq;
	int n;

	switch (FIELD_S16(hdx, V32HDX_STATE)) {
	case V32_STATE_A:
		/* Wait out the budget, then listen for answer tone. */
		if (FIELD_INT(hdx, V32_HDX_INT_78) <= 0) {
			FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_B;
			FIELD_INT(hdx, V32_HDX_INT_78) = 0xb4;
			SetToneDetect(modem, 0);
			TONE0(HDX(modem))->cfg.ratio =
				V32_ANS_TONE_RATIO_HI;
		}
		break;

	case V32_STATE_B:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_B2;
		FIELD_INT(hdx, V32_HDX_INT_78) = 0x100;
		if (FIELD_U8(modem, V32_OBJ_U8_11) & V32_OPT11_02)
			FIELD_INT(hdx, V32_HDX_INT_78) = 0xa60;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_TXFN(hdx, V32HDX_TXSTATE) = TxHdxCarrierState;
		InitGenSequence(modem, 3, 4, 2);
		SetToneDetect(modem, V32_ANS_TONE_HZ);
		TONE0(HDX(modem))->cfg.ratio = V32_ANS_TONE_RATIO_HI;
		break;

	case V32_STATE_B2:
		/*
		 * The one branch out of the chain: the budget expiring here is
		 * a failure and not a step.
		 */
		if (FIELD_INT(hdx, V32_HDX_INT_78) <= 0) {
			FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_FAULT;
			FIELD_U8(modem, V32_OBJ_STATUS) =
				V32_ANS_STATUS_TIMEOUT;
			FIELD_TXFN(hdx, V32HDX_TXSTATE) = TxHdxNoCarrier;
			FIELD_RXFN(hdx, V32HDX_RXSTATE) = RxHdxError;
			FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_ERROR;
		} else {
			FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_C;
			FIELD_INT(hdx, V32_HDX_INT_90) = 1;
			FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		}
		break;

	case V32_STATE_C:
		if (FIELD_U32(hdx, V32_HDX_U32_7C) > 0x8b) {
			FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_D;
			FIELD_INT(hdx, V32_HDX_INT_90) = 1;
			FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
			FIELD_INT(hdx, V32_HDX_INT_78) =
				(int)FIELD_U32(hdx, V32_HDX_U32_80);
			FIELD_RXFN(hdx, V32HDX_RXSTATE) = RxHdxPhsReversal;
			InitGenSequence(modem, 0xc, 4, 2);
			FIELD_S16(HDX(modem), V32_HDX_SHORT_A8) = 0;
		}
		break;

	case V32_STATE_D:
		if (FIELD_S16(hdx, V32_HDX_SHORT_A8) > 0x48) {
			FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_E;
			n = (int)FIELD_U32(hdx, V32_HDX_U32_7C);
			FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
			FIELD_RXFN(hdx, V32HDX_RXSTATE) = RxHdxNoSignal;
			/*
			 * Rounded UP to even: `and $1` then `cmp $1` /
			 * `sbb $-1` at 86331..86337, which is n + (n & 1).
			 */
			FIELD_INT(hdx, V32_HDX_INT_78) = n + (n & 1);
			SetToneDetect(modem, V32_ANS_TONE_HZ);
			TONE0(HDX(modem))->cfg.ratio =
				V32_ANS_TONE_RATIO_LO;
		}
		break;

	case V32_STATE_E:
		if (FIELD_INT(hdx, V32_HDX_INT_78) <= 0) {
			FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_F;
			FIELD_INT(hdx, V32_HDX_INT_78) =
				(int)FIELD_U32(hdx, V32_HDX_U32_80);
			InitGenSequence(modem, 3, 4, 2);
			FIELD_S16(HDX(modem), V32_HDX_SHORT_AA) = 0;
		}
		break;

	case V32_STATE_F:
		if (FIELD_S16(hdx, V32_HDX_SHORT_AA) > 0x5f) {
			FIELD_U8(modem, V32_OBJ_U8_32) |= V32_BIT32_08;
			FIELD_U8(modem, V32_OBJ_STATUS) =
				V32_ANS_STATUS_CARRIER;
			FIELD_U8(modem, V32_OBJ_FLAGS) = (unsigned char)
				((FIELD_U8(modem, V32_OBJ_FLAGS)
				  & (unsigned char)~V32_FLAG_SILENCE)
				 | V32_FLAG_CARRIER);
			StoreReg(modem, FIELD_S16(hdx, V32_HDX_SHORT_96), 0);

			h2 = (unsigned char *)HDX(modem);
			FIELD_S16(h2, V32HDX_STATE) = V32_STATE_F2;
			FIELD_INT(h2, V32_HDX_INT_78) = 0x10;
			FIELD_U32(h2, V32_HDX_U32_7C) = 0;
			FIELD_TXFN(h2, V32HDX_TXSTATE) = TxHdxNoCarrier;
			FIELD_RXFN(h2, V32HDX_RXSTATE) = RxHdxData;
		}
		break;

	case V32_STATE_G:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_H;
		FIELD_INT(hdx, V32_HDX_INT_78) = 0x100;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_TXFN(hdx, V32HDX_TXSTATE) = TxHdxCarrierState;
		SetECRndTripDelayV32(modem, LoadReg(modem, 0));
		SetTxModeV32(modem, 0);
		InitGenSequence(modem, 1, 4, 2);
		break;

	case V32_STATE_H:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_I;
		FIELD_INT(hdx, V32_HDX_INT_78) = 0x10;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_TXFN(hdx, V32HDX_TXSTATE) = TxHdxCarrierState;
		InitGenSequence(modem, 0xe, 4, 2);
		break;

	case V32_STATE_I:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_J;
		FIELD_INT(hdx, V32_HDX_INT_78) = 0x100;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_TXFN(hdx, V32HDX_TXSTATE) = TxHdxTRN;
		InitGenSequence(modem, 0xf, 4, 2);
		SeedScramblerV32(modem, 0);
		break;

	case V32_STATE_J:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_K;
		FIELD_INT(hdx, V32_HDX_INT_78) = 0x400;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_TXFN(hdx, V32HDX_TXSTATE) = TxHdxScrSequence;
		SetAdaptEcV32(modem, V32_ADAPTEC_ON);
		break;

	case V32_STATE_K:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_L;
		FIELD_INT(hdx, V32_HDX_INT_78) = 0x1a6c;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		SetAdaptEcV32(modem, V32_ADAPTEC_SLOW);
		break;

	case V32_STATE_L:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_M;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_INT(hdx, V32_HDX_INT_78) =
			(int)FIELD_U32(hdx, V32_HDX_U32_80);
		FIELD_RXFN(hdx, V32HDX_RXSTATE) = RxHdxSTone;
		/*
		 * Retune the multi-tone detector to the DATA-mode S tone.  The
		 * config the detector is rebuilt from is its OWN, which is why
		 * both arguments hold the same address: `struct fpm_mtd`'s
		 * first member is the `struct fpm_mtd_cfg` being handed back.
		 */
		MTD(hdx)->cfg.coeff = V32_S_DATA_COEF;
		FPM_MTD_create(MTD(hdx), &MTD(hdx)->cfg);

		seq = (short)CodeRateSeq(modem,
					 RateToSeq(modem,
						   (short)GetRateV32(modem)));
		StoreReg(modem, seq, 1);
		InitGenSequence(modem, (unsigned short)seq, 0x10, 2);
		SetTxModeV32(modem, 1);
		SetAdaptEcV32(modem, V32_ADAPTEC_OFF);
		break;

	case V32_STATE_M:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_N;
		n = LoadReg(modem, 0);

		h2 = (unsigned char *)HDX(modem);
		FIELD_U32(h2, V32_HDX_U32_7C) = 0;
		FIELD_TXFN(h2, V32HDX_TXSTATE) = TxHdxNoCarrier;
		FIELD_RXFN(h2, V32HDX_RXSTATE) = RxHdxData;
		/* The store is through the CACHED context; see the header. */
		FIELD_INT(hdx, V32_HDX_INT_78) =
			n + FIELD_S16(h2, V32_HDX_SHORT_94);
		break;

	case V32_STATE_N:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_O;
		FIELD_INT(hdx, V32_HDX_INT_78) =
			(int)FIELD_U32(hdx, V32_HDX_U32_80);
		SetToneDetect(modem, V32_ANS_TONE_HZ);

		h2 = (unsigned char *)HDX(modem);
		FIELD_RXFN(h2, V32HDX_RXSTATE) = RxHdxSTone;
		TONE0(h2)->cfg.ratio = V32_ANS_TONE_RATIO_HI;
		break;

	case V32_STATE_O:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_P;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_RXFN(hdx, V32HDX_RXSTATE) = RxHdxEpoch;
		FIELD_INT(hdx, V32_HDX_INT_78) =
			(int)FIELD_U32(hdx, V32_HDX_U32_80);
		SetRxModeV32(modem, 0);
		SetRxLoopsV32(modem, 2);
		break;

	case V32_STATE_P:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_Q;
		FIELD_INT(hdx, V32_HDX_INT_78) = 0x610;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_TXFN(hdx, V32HDX_TXSTATE) = TxHdxNoCarrier;
		FIELD_RXFN(hdx, V32HDX_RXSTATE) = RxHdxData;
		SetAdaptEqV32(modem, V32_ADAPTEQ_MU0);
		break;

	case V32_STATE_Q:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_T;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_RXFN(hdx, V32HDX_RXSTATE) = RxHdxRateSequence;
		FIELD_INT(hdx, V32_HDX_INT_78) =
			(int)FIELD_U32(hdx, V32_HDX_U32_80);
		SetRxModeV32(modem, 1);
		SetAdaptEqV32(modem, V32_ADAPTEQ_MU1);
		InitDetSequence(modem, V32_ANS_DET_TARGET_Q,
				(int)V32_ANS_DET_MASK, -1, 2);
		break;

	case V32_STATE_T:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_U;
		FIELD_INT(hdx, V32_HDX_INT_78) = 0x100;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_TXFN(hdx, V32HDX_TXSTATE) = TxHdxCarrierState;
		FIELD_RXFN(hdx, V32HDX_RXSTATE) = RxHdxData;
		SetTxModeV32(modem, 0);
		InitGenSequence(modem, 1, 4, 2);
		StoreReg(modem, (short)GetSequence(modem), 2);
		/*
		 * The rate sequence just detected decodes to "no rate in
		 * common", which is a fault and not a step -- but the arm has
		 * ALREADY moved to U, so the handshake goes on and the status
		 * is what carries the failure out.
		 */
		if (DecodeRateSeq(modem, (unsigned short)LoadReg(modem, 2))
		    == V32_RATE_NONE) {
			FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_FAULT;
			FIELD_U8(modem, V32_OBJ_STATUS) = V32_STATUS_BAD_MODE;
		}
		break;

	case V32_STATE_U:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_V;
		FIELD_INT(hdx, V32_HDX_INT_78) = 0x10;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_TXFN(hdx, V32HDX_TXSTATE) = TxHdxCarrierState;
		InitGenSequence(modem, 0xe, 4, 2);
		break;

	case V32_STATE_V:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_W;
		FIELD_INT(hdx, V32_HDX_INT_78) = 0x100;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_TXFN(hdx, V32HDX_TXSTATE) = TxHdxTRN;
		InitGenSequence(modem, 0xf, 4, 2);
		SeedScramblerV32(modem, 0);
		SetAdaptEcV32(modem, V32_ADAPTEC_OFF);
		break;

	case V32_STATE_W:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_X;
		FIELD_INT(hdx, V32_HDX_INT_78) = 0x1bbc;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_TXFN(hdx, V32HDX_TXSTATE) = TxHdxScrSequence;
		break;

	case V32_STATE_X:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_X2;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_TXFN(hdx, V32HDX_TXSTATE) = TxHdxScrSequence;
		FIELD_INT(hdx, V32_HDX_INT_78) =
			(int)FIELD_U32(hdx, V32_HDX_U32_80);
		FIELD_RXFN(hdx, V32HDX_RXSTATE) = RxHdxSequence;
		InitGenSequence(modem,
				CodeFinalRateSeq(modem, LoadReg(modem, 2)),
				0x10, 2);
		SetTxModeV32(modem, 1);
		InitDetSequence(modem, V32_ANS_DET_TARGET_X,
				(int)V32_ANS_DET_MASK, -1, 2);
		SetAdaptEqV32(modem, V32_ADAPTEQ_OFF);
		break;

	case V32_STATE_X2:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_Y;
		/*
		 * Round the transmit budget up to the next multiple of eight
		 * measured from the receive limit: 8 - ((limit - budget) & 7).
		 */
		FIELD_INT(hdx, V32_HDX_INT_78) = 8
			- (int)((FIELD_U32(hdx, V32_HDX_U32_80)
				 - (unsigned int)FIELD_INT(hdx,
							   V32_HDX_INT_78))
				& 7u);
		StoreReg(modem, (short)GetSequence(modem), 4);
		break;

	case V32_STATE_Y:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_Z;
		FIELD_INT(hdx, V32_HDX_INT_78) = 8;
		FIELD_TXFN(hdx, V32HDX_TXSTATE) = TxHdxScrSequence;
		FIELD_RXFN(hdx, V32HDX_RXSTATE) = RxHdxData;
		SetRxModeV32(modem,
			     V32_RX_MODE[DecodeRateSeq(modem,
						       (unsigned short)
						       LoadReg(modem, 4))]);
		InitGenSequence(modem,
				CodeESeq(modem,
					 (unsigned short)LoadReg(modem, 4)),
				0x10, 2);
		FPM_AGC_Freeze(AGC(FP(modem)));
		break;

	case V32_STATE_Z:
		InitGenSequence(modem, 0xffff, 0x10, 8);

		h2 = (unsigned char *)HDX(modem);
		FIELD_S16(h2, V32HDX_STATE) = V32_STATE_END;
		FIELD_INT(h2, V32_HDX_INT_78) = 0x80;
		FIELD_U32(h2, V32_HDX_U32_7C) = 0;
		FIELD_TXFN(h2, V32HDX_TXSTATE) = TxHdxScrSequence;
		FIELD_RXFN(h2, V32HDX_RXSTATE) = RxHdxData;
		SetTxModeV32(modem,
			     V32_TX_MODE[DecodeRateSeq(modem,
						       (unsigned short)
						       LoadReg(modem, 4))]);
		break;

	case V32_STATE_END:
		/* END transitions to END; see the header comment. */
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_END;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_INT(hdx, V32_HDX_INT_78) =
			(int)FIELD_U32(hdx, V32_HDX_U32_80);
		FIELD_U8(modem, V32_OBJ_FLAGS) |=
			V32_FLAG_01 | V32_FLAG_08 | V32_FLAG_10;
		FIELD_U8(modem, V32_OBJ_STATUS) = (unsigned char)
			V32_CONNECT[DecodeRateSeq(modem,
						  (unsigned short)
						  LoadReg(modem, 4))];
		SetAdaptEqV32(modem, V32_ADAPTEQ_MU1);
		SetRxLoopsV32(modem, 3);
		DEC(FP(modem))->retrain = 0;
		break;

	case V32_STATE_F2:
		FIELD_S16(hdx, V32HDX_STATE) = V32_STATE_G;
		FIELD_U32(hdx, V32_HDX_U32_7C) = 0;
		FIELD_TXFN(hdx, V32HDX_TXSTATE) = TxHdxFinishFrame;
		FIELD_INT(hdx, V32_HDX_INT_78) =
			FIELD_S16(hdx, V32HDX_SYMBOL_LEN);
		break;

	case V32_STATE_ERROR:
		FIELD_INT(hdx, V32_HDX_INT_78) =
			(int)FIELD_U32(hdx, V32_HDX_U32_80);
		break;

	default:
		break;
	}

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("state %s(%d)\n",
				     V32StateName(FIELD_S16(HDX(modem),
							    V32HDX_STATE)),
				     FIELD_S16(HDX(modem), V32HDX_STATE));
}

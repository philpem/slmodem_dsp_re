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
 * hdx + 0x78 and + 0x84 already have names -- `src/pump/v32/V32TXHDX.c`
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
#include "dsplib/v32struct.h"

#include "dsplib/v32fpctl.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32smc.h"
#include "dsplib/v32dec.h"
#include "dsplib/v32state.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/debug.h"

/* The instance is not modelled; see v32hdx.h.  These are the accessors. */

#define HDX(m) (((struct v32_modem *)(m))->hdx)
#define FP(m) (((struct v32_modem *)(m))->fp)

/*
 * `LoadReg` and `StoreReg` reach V32HDX_REGS through an index; these two
 * functions reach two of the five DIRECTLY, and always UNSIGNED -- every load
 * is `movzwl` and every one of them feeds `DecodeRateSeq`, whose parameter is
 * `unsigned short`.  v32seq.c spells the same array `short` because `LoadReg`
 * returns `short`; per finding F614 the FIELD's type is not what varies.
 */

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
 * The handshake state's countdown, in symbols.  Derived in V32TXHDX.c from the
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

/* ------------------------------------------------------------------------ */

void
V32RngInitNextState(void *modem)
{
	struct v32_hdx *hdx = HDX(modem);
	struct v32_fp *fp;
	short rate;
	short seq;

	switch (hdx->state) {
	case V32_STATE_A:
		SetAdaptEqV32(modem, V32_ADAPTEQ_OFF);
		SetAdaptEcV32(modem, V32_ADAPTEC_OFF);
		SetRxLoopsV32(modem, 1);

		hdx = HDX(modem);
		fp = FP(modem);
		hdx->state = V32_STATE_B;
		fp->int_00 = 1;
		hdx->tx_state = (void *)TxHdxCarrierState;
		hdx->state_left = 0x38;

		SetTxModeV32(modem, V32_MODE_ABS4);
		InitGenSequence(modem,
				(unsigned short)
				(((struct v32_modem *)modem)->params.protocol == 0
				 ? 0 : 3),
				4, 2);

		((struct v32_modem *)modem)->flags = (unsigned char)
			((((struct v32_modem *)modem)->flags & ~V32_FLAG_08)
			 | V32_FLAG_04);
		SetToneDetect(modem,
			      (short)(((struct v32_modem *)modem)->params.protocol == 0
				      ? 600 : 1800));

		hdx = HDX(modem);
		hdx->rx_state = (void *)RxHdxToneData;
		hdx->timer = 0;
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
		if (hdx->state_left <= 0) {
			hdx->state = V32_STATE_C;
			hdx->state_left = 8;
			InitGenSequence(modem,
					(unsigned short)
					(((struct v32_modem *)modem)->params.protocol == 0
					 ? 15 : 12),
					4, 2);
			hdx = HDX(modem);
		} else {
			((struct v32_modem *)modem)->flags &=
				(unsigned char)~V32_FLAG_04;
		}
		hdx->rx_state = (void *)RxHdxToneData;
		break;

	case V32_STATE_C:
		if (hdx->state_left > 0) {
			((struct v32_modem *)modem)->flags &=
				(unsigned char)~V32_FLAG_04;
			hdx->rx_state = (void *)RxHdxToneData;
			break;
		}

		hdx->state = V32_STATE_D;
		hdx->tx_state = (void *)TxHdxScrSequence;
		hdx->state_left = hdx->limit;

		if ((((struct v32_modem *)modem)->flags & V32_FLAG_04) == 0) {
			hdx->rx_state = (void *)RxHdxSequence;
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
		FP(modem)->tx_smc.state[0] =
			(short)(((struct v32_modem *)modem)->params.protocol == 0
				? 12 : 0);
		break;

	case V32_STATE_D:
		if ((((struct v32_modem *)modem)->flags & V32_FLAG_04) != 0) {
			((struct v32_modem *)modem)->flags &=
				(unsigned char)~V32_FLAG_04;
			hdx->rx_state = (void *)RxHdxSequence;
			SetRxModeV32(modem, V32_MODE_DIF4);
			InitDetSequence(modem, V32_DET_SEQ, V32_DET_MASK, -1,
					2);
			break;
		}

		StoreReg(modem, (short)GetSequence(modem), 1);

		hdx = HDX(modem);
		hdx->state = V32_STATE_D2;
		hdx->timer = 0;
		hdx->regs[4] = 0;
		hdx->rx_state = (void *)RxHdxSequenceE;
		InitDetSequence(modem, V32_DET_ESEQ, V32_DET_MASK, -1, 2);

		{
			int base = 0x40;
			int used;

			hdx = HDX(modem);
			used = hdx->limit
				- hdx->state_left;
			if (used > 0x3f) {
				used &= 7;
				base = 8;
			}
			hdx->state_left = base - used;
		}
		break;

	case V32_STATE_D2:
		SetAdaptEqV32(modem, V32_ADAPTEQ_MU1);
		SetRxLoopsV32(modem, 3);

		hdx = HDX(modem);
		hdx->state = V32_STATE_E;
		hdx->state_left = 8;

		InitGenSequence(modem,
				CodeESeq(modem,
					 (unsigned short)LoadReg(modem,
								 1)),
				0x10, 2);
		break;

	case V32_STATE_E:
		hdx->state = V32_STATE_F;
		hdx->state_left = 0x18;
		hdx->tx_state = (void *)TxHdxScrSequence;

		rate = DecodeRateSeq(modem,
				     (unsigned short)LoadReg(modem, 1));
		SetTxModeV32(modem, V32_TX_MODE[rate]);
		InitGenSequence(modem, 0xffff, 0x10, 8);
		if (rate > 2)
			FP(modem)->tx_smc.f10 = 0;
		break;

	case V32_STATE_F:
		((struct v32_modem *)modem)->flags |= V32_FLAG_08;

		rate = DecodeRateSeq(modem,
				     (unsigned short)LoadReg(modem, 1));
		SetTxModeV32(modem, V32_TX_MODE[rate]);
		InitGenSequence(modem, 0xffff, 0x10, 8);

		hdx = HDX(modem);
		if (hdx->regs[4] == 0) {
			hdx->timer = 0;
			hdx->state = V32_STATE_G;
			hdx->rx_state = (void *)RxHdxSequence;
			hdx->state_left =
				hdx->limit;
			SetRxModeV32(modem, V32_MODE_DIF4);
		} else {
			hdx->timer = 0;
			hdx->state = V32_STATE_DONE;
			hdx->state_left =
				0x18 - hdx->short_48;
		}
		break;

	case V32_STATE_G:
		hdx->state = V32_STATE_DONE;
		hdx->state_left = 0x24;
		hdx->timer = 0;
		hdx->rx_state = (void *)RxHdxData;

		hdx->regs[4] = (unsigned short)GetSequence(modem);
		rate = DecodeRateSeq(modem,
				     HDX(modem)->regs[4]);
		SetRxModeV32(modem, V32_RX_MODE[rate]);
		SetAdaptEqV32(modem, V32_ADAPTEQ_MU1);
		SetRxLoopsV32(modem, 3);
		break;

	case V32_STATE_DONE:
	{
		short other;

		rate = DecodeRateSeq(modem, hdx->regs[4]);
		other = DecodeRateSeq(modem,
				    HDX(modem)->regs[1]);

		hdx = HDX(modem);
		if (rate == V32_RATE_NONE || other == V32_RATE_NONE) {
			/*
			 * Every one of these four but the state is undone by
			 * the tail below.  See the header comment: F8588.
			 */
			hdx->state_left = 0x40;
			((struct v32_modem *)modem)->flags |= V32_FLAG_FAULT;
			((struct v32_modem *)modem)->status = V32_STATUS_0E;
			hdx->state = V32_STATE_CLEARDOWN;
		}

		hdx->timer = 0;
		hdx->state_left = hdx->limit;
		((struct v32_modem *)modem)->flags |= V32_FLAG_01 | V32_FLAG_04;
		((struct v32_modem *)modem)->status =
			(unsigned char)V32_CONNECT[rate];
		((struct v32_dec *)FP(modem)->fse.cfg.owner)->retrain = 0;
		break;
	}

	case V32_STATE_ERROR:
		hdx->state_left = hdx->limit;
		break;

	default:
		break;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("state %s(%d)\n",
				     V32StateName(HDX(modem)->state),
				     HDX(modem)->state);
}

/* ------------------------------------------------------------------------ */

void
V32RngRespNextState(void *modem)
{
	struct v32_hdx *hdx = HDX(modem);
	struct v32_fp *fp;
	short rate;
	short seq;

	switch (hdx->state) {
	case V32_STATE_A:
		SetAdaptEqV32(modem, V32_ADAPTEQ_OFF);
		SetRxLoopsV32(modem, 1);
		SetAdaptEcV32(modem, V32_ADAPTEC_OFF);

		hdx = HDX(modem);
		fp = FP(modem);
		hdx->state = V32_STATE_B;
		fp->int_00 = 1;
		hdx->state_left = hdx->limit;
		hdx->tx_state = (void *)TxHdxData;
		hdx->rx_state = (void *)RxHdxSequence;
		hdx->timer = 0;

		InitDetSequence(modem, V32_DET_SEQ, V32_DET_MASK, 0xffff, 2);
		SetRxModeV32(modem, V32_MODE_DIF4);
		((struct v32_modem *)modem)->flags &= (unsigned char)~V32_FLAG_04;

		rate = (short)GetRateV32(modem);
		seq = (short)CodeRateSeq(modem, RateToSeq(modem, rate));
		StoreReg(modem, seq, 0);
		break;

	case V32_STATE_B:
		SetAdaptEqV32(modem, V32_ADAPTEQ_MU1);
		SetRxLoopsV32(modem, 3);

		hdx = HDX(modem);
		hdx->state = V32_STATE_C;
		hdx->regs[4] = 0;
		hdx->tx_state = (void *)TxHdxCarrierState;
		hdx->rx_state = (void *)RxHdxSequenceE;
		hdx->state_left = 0x38;
		hdx->timer = 0;

		InitGenSequence(modem,
				(unsigned short)
				(((struct v32_modem *)modem)->params.protocol == 0
				 ? 0 : 3),
				4, 2);
		SetTxModeV32(modem, V32_MODE_ABS4);
		StoreReg(modem, (short)GetSequence(modem), 1);
		InitDetSequence(modem, V32_DET_ESEQ, V32_DET_MASK, -1, 2);
		((struct v32_modem *)modem)->flags &= (unsigned char)~V32_FLAG_08;
		break;

	case V32_STATE_C:
		hdx->state = V32_STATE_D;
		hdx->block_charge = 0;
		hdx->state_left = 8;
		hdx->timer = 0;
		InitGenSequence(modem,
				(unsigned short)
				(((struct v32_modem *)modem)->params.protocol == 0
				 ? 15 : 12),
				4, 2);
		break;

	case V32_STATE_D:
		hdx->state = V32_STATE_E;
		hdx->state_left = 0x40;
		hdx->timer = 0;
		hdx->tx_state = (void *)TxHdxScrSequence;

		SetTxModeV32(modem, V32_MODE_DIF4);
		InitGenSequence(modem,
				CodeRateSeq(modem,
					    (unsigned short)LoadReg(modem, 0)),
				0x10, 2);
		SeedScramblerV32(modem, 0);
		FP(modem)->tx_smc.state[0] =
			(short)(((struct v32_modem *)modem)->params.protocol == 0
				? 12 : 0);
		break;

	case V32_STATE_E:
		hdx->state = V32_STATE_F;
		hdx->state_left = 8;
		hdx->timer = 0;
		hdx->tx_state = (void *)TxHdxScrSequence;

		InitGenSequence(modem,
				CodeESeq(modem,
					 (unsigned short)LoadReg(modem,
								 1)),
				0x10, 2);
		break;

	case V32_STATE_F:
		hdx->state = V32_STATE_G;

		rate = DecodeRateSeq(modem,
				     (unsigned short)LoadReg(modem, 1));
		SetTxModeV32(modem, V32_TX_MODE[rate]);

		if (rate == V32_RATE_NONE) {
			hdx = HDX(modem);
			hdx->state_left = 0x28;
			((struct v32_modem *)modem)->flags |= V32_FLAG_FAULT;
			((struct v32_modem *)modem)->status = V32_STATUS_17;
			hdx->state = V32_STATE_CLEARDOWN;
			break;
		}

		InitGenSequence(modem, 0xffff, 0x10, 8);
		hdx = HDX(modem);
		hdx->state_left = 0x18;
		hdx->tx_state = (void *)TxHdxScrSequence;
		if (rate > 2)
			FP(modem)->tx_smc.f10 = 0;
		break;

	case V32_STATE_G:
		((struct v32_modem *)modem)->flags |= V32_FLAG_08;

		rate = DecodeRateSeq(modem,
				     (unsigned short)LoadReg(modem, 1));
		SetTxModeV32(modem, V32_TX_MODE[rate]);

		hdx = HDX(modem);
		if (hdx->regs[4] != 0) {
			hdx->timer = 0;
			hdx->state = V32_STATE_DONE;
			hdx->state_left =
				0x18 - hdx->short_48;
		} else {
			hdx->rx_state = (void *)RxHdxSequence;
			hdx->state = V32_STATE_H;
			hdx->state_left =
				hdx->limit;
		}
		break;

	case V32_STATE_H:
		hdx->state = V32_STATE_DONE;
		hdx->state_left = 0x24;
		hdx->timer = 0;
		hdx->rx_state = (void *)RxHdxData;

		hdx->regs[4] = (unsigned short)GetSequence(modem);
		rate = DecodeRateSeq(modem,
				     HDX(modem)->regs[4]);
		SetRxModeV32(modem, V32_RX_MODE[rate]);
		break;

	case V32_STATE_DONE:
		hdx->timer = 0;
		hdx->state_left = hdx->limit;
		((struct v32_modem *)modem)->flags |= V32_FLAG_01 | V32_FLAG_04;

		rate = DecodeRateSeq(modem, hdx->regs[4]);
		((struct v32_modem *)modem)->status =
			(unsigned char)V32_CONNECT[rate];
		((struct v32_dec *)FP(modem)->fse.cfg.owner)->retrain = 0;
		break;

	case V32_STATE_ERROR:
		hdx->state_left = hdx->limit;
		break;

	default:
		break;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("state %s(%d)\n",
				     V32StateName(HDX(modem)->state),
				     HDX(modem)->state);
}

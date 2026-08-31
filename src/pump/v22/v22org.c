/*
 * v22org.c -- V.22 / V.22bis: the ORIGINATE and ANSWER protocol states.
 *
 * Reconstructed from dsplibs.o:
 *
 *   v22_answer     .text 0x08abf0  1,789 bytes
 *   v22_originate  .text 0x08b2f0  2,655 bytes
 *
 * `include/dsplib/v22org.h` carries the derivation -- the eleven format
 * strings that name the nodes, the ones that name the carrier verdicts, the
 * two jump tables, and what `FPM_TONE_generate` really returns.  This file is
 * the code.
 *
 * They are written in the object's address order, `v22_answer` first.  That is
 * a guess about the translation unit and not a claim, exactly as
 * src/pump/v22/v22conn.c records for its own pair.
 *
 * ---------------------------------------------------------------------------
 * THE INSTANCE POINTER IS READ AGAIN AFTER EVERY CALL
 *
 * The same observation src/pump/v22/v22conn.c and v22data.c record.  Every arm
 * here reloads `0x50(%ebx)` after a call rather than keeping the block pointer
 * in a callee-saved register, so `fp->hdx` is spelled out at each use.
 *
 * ---------------------------------------------------------------------------
 * `v22_answer`'s NODE_0 SETS `hdx->connect_substate` TO 3 TWICE
 *
 * Once before `SetTxRate`/`SetRxRate` and again after `ModDataV22`, with five
 * calls in between and no other writer.  Both stores are `movw $0x3,0xc(...)`
 * and the second is dead.  It is two stores in the object and it is two here.
 *
 * ---------------------------------------------------------------------------
 * ARMS THAT SHARE A TAIL ARE WRITTEN OUT TWICE
 *
 * `v22_answer`'s NODE_0 and NODE_1 both jump to the retune tail at 0x8af46;
 * its two carrier verdicts share everything from `SetAdaptEqV22` onwards;
 * `v22_originate`'s NODE_4 and NODE_5 share their closing `RxClampV22` and a
 * whole second copy of the epilogue.  Each is written out in full rather than
 * shared, on the same argument v22conn.c gives for its carrier-loss tail: GCC
 * tail-merges identical blocks, so one copy in the object is not evidence of
 * one copy in the source.
 */

#include "dsplib/v22org.h"

#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v22conn.h"
#include "dsplib/v22data.h"
#include "dsplib/v22det.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22prc.h"
#include "dsplib/v22rate.h"
#include "dsplib/v22status.h"

/*
 * `.bss` + 0x380, two bytes, LOCAL, and referenced by `v22_answer` alone --
 * node 1 clears it, node 14 increments it and compares it with four.  Read
 * with `movzwl`, which is what makes it unsigned.  `static` for the same
 * reason src/pump/v22/v22status.c's own `PROTOCOL` table is: the object's
 * symbol is LOCAL.
 */
static unsigned short iSilenceAfter2100;

/* ------------------------------------------------------------------------ */

void
v22_answer(struct v22fp *fp, unsigned short *txsym, short *txout,
	   short *rxin, unsigned short *rxsym,
	   unsigned short *txcount, unsigned short *rxcount)
{
	struct v22_status st;
	struct fpm_tone *tone;
	unsigned short nsym;	/* DemodDataV22's symbol count               */
	short detected;		/* FPM_MTD_detect's verdict for this block   */
	short ones;		/* Detect_1s's, in milliseconds              */
	/*
	 * 0 when this block's S1 detector fired and V22_BLOCK_MS when it did
	 * not.  Zeroed at function scope rather than inside NODE_3, which is
	 * where the object zeroes it -- the store at the top of the function
	 * runs on every arm, including the ones with no case at all.
	 */
	short gap = 0;

	fp->status = V22_STATUS_01;

	switch (fp->hdx->connect_substate) {
	case V22_ANS_NODE_0:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_answer,NODE_0\n");

		fp->hdx->gtimer = 0;
		fp->hdx->r08 = 0;
		fp->hdx->r0a = 0;

		/*
		 * The report is taken for ONE BIT of it: +0x15 bit 0, which
		 * v22status.h establishes is `params.flags` bit 10 copied out.
		 * V22_status writes that bit on every path and leaves the other
		 * seven of the byte alone, so reading an otherwise uninitialised
		 * block here is the object's behaviour and is deterministic.
		 */
		V22_status(fp, &st);

		*rxcount = 0;
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		if (st.flags2 & V22_STATUS2_PARAM_BIT10) {
			/*
			 * No answer tone wanted: hand the transmitter nothing
			 * and wait in NODE_1 anyway.
			 */
			TxNOP(fp, txsym, txout, (short *)txcount);
			fp->hdx->connect_substate = V22_ANS_NODE_1;
			break;
		}

		fp->hdx->connect_substate = V22_ANS_NODE_3;
		SetTxRate(fp, V22_RATE_1200);
		SetRxRate(fp, V22_RATE_1200);

		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_1200);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		/* Dead: nothing above has moved it.  See the file header. */
		fp->hdx->connect_substate = V22_ANS_NODE_3;

		tone = fp->hdx->tone;
		tone->cfg.freq = V22_ANS_TONE_HZ;
		tone->cfg.scale = (short)((tone->cfg.scale
					   * V22_ORG_TONE_SCALE_Q15) >> 15);
		FPM_TONE_create(fp->hdx->tone, &fp->hdx->tone->cfg);
		break;

	case V22_ANS_NODE_1:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_answer,NODE_1\n");

		/*
		 * The answer tone.  `FPM_TONE_generate` returns its `count` in
		 * the object and fpm_tone.h declares it `void`; the count is the
		 * literal below, so the store is spelled out.  See v22org.h.
		 */
		FPM_TONE_generate(fp->hdx->tone, txout, V22_ORG_BLOCK);
		*txcount = V22_ORG_BLOCK;

		*rxcount = 0;
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		if ((unsigned int)ReadGTimer(fp) > V22_ANS_NODE_1_MS) {
			fp->hdx->connect_substate = V22_ANS_NODE_SILENCE_AFTER_2100;
			fp->hdx->gtimer = 0;
			iSilenceAfter2100 = 0;
			fp->flags |= V22FP_FLAG_1D_BIT4;
			SetTxRate(fp, V22_RATE_1200);
			SetRxRate(fp, V22_RATE_1200);

			tone = fp->hdx->tone;
			tone->cfg.freq = V22_ANS_TONE_HZ;
			tone->cfg.scale =
			    (short)((tone->cfg.scale
				     * V22_ORG_TONE_SCALE_Q15) >> 15);
			FPM_TONE_create(fp->hdx->tone, &fp->hdx->tone->cfg);
		}
		break;

	case V22_ANS_NODE_3:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_answer,NODE_3\n");

		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_1200);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		/*
		 * Past its own deadline the transmitter stops sending data and
		 * sends the tone instead, over the top of what `ModDataV22` has
		 * just produced.
		 */
		if ((unsigned int)fp->hdx->gtimer > V22_ANS_NODE_3_TONE_MS) {
			FPM_TONE_generate(fp->hdx->tone, txout, V22_ORG_BLOCK);
			*txcount = V22_ORG_BLOCK;
		}

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);

		/*
		 * The S1 detector runs on the rate converter's own output
		 * buffer rather than on the caller's symbols, and it runs
		 * unconditionally -- before the test that decides whether its
		 * answer is used.
		 */
		detected = FPM_MTD_detect(fp->hdx->mtd_s1, fp->dsp->rx_scratch,
					  fp->dsp->rx_count);

		if (*rxcount != 0 && SignalDetect(fp) == 1) {
			if (detected != 0) {
				gap = 0;
				if ((unsigned short)fp->hdx->r08
				    <= V22_ORG_MIN_RUN_MS)
					fp->hdx->r08 = 0;
			} else {
				gap = V22_BLOCK_MS;
				fp->hdx->r08 = (short)(fp->hdx->r08
						       + V22_BLOCK_MS);
			}

			/*
			 * The first pass looks at the symbols as they came off
			 * the equaliser; only if that finds nothing does the
			 * block get descrambled and asked again.  Both calls
			 * take the same four arguments.
			 */
			ones = (short)Detect_1s(rxsym, rxcount,
						V22_ORG_DETECT_BPS,
						V22_ORG_DETECT_THRESH);
			if (ones == 0) {
				DescrambleDataV22(fp, rxsym, *rxcount);
				ones = (short)Detect_1s(rxsym, rxcount,
							V22_ORG_DETECT_BPS,
							V22_ORG_DETECT_THRESH);
				fp->hdx->r0a = (short)(fp->hdx->r0a + ones);
				/*
				 * The reset `v22_originate`'s two copies of
				 * this block do NOT have.  See v22org.h.
				 */
				if (ones == 0 && (unsigned short)fp->hdx->r0a
						 <= V22_ORG_MIN_RUN_MS)
					fp->hdx->r0a = 0;
			}
		}

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		if ((unsigned short)fp->hdx->r08 > V22_ORG_MIN_RUN_MS
		    && gap == 0
		    && fp->params.bps2 == V22_STATUS_BPS_2400) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "Detected V22bis Carrier\n");
			fp->hdx->gtimer = 0;
			fp->hdx->r08 = 0;
			fp->hdx->r0a = 0;
			fp->hdx->connect_substate = V22_ANS_NODE_4;
			SetAdaptEqV22(fp, V22_ORG_EQ_MODE_CARRIER);
			fp->r1e[0] |= V22FP_R1E_BIT3;
			FPM_AGC_Freeze(&fp->dsp->agc);
		} else if ((unsigned short)fp->hdx->r0a > V22_ORG_ONES_MS) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("Detected V22 Carrier\n");
			fp->hdx->gtimer = 0;
			fp->hdx->r08 = 0;
			fp->hdx->r0a = 0;
			fp->hdx->connect_substate = V22_NODE_1200_12;
			SetAdaptEqV22(fp, V22_ORG_EQ_MODE_CARRIER);
			fp->r1e[0] |= V22FP_R1E_BIT3;
			FPM_AGC_Freeze(&fp->dsp->agc);
		} else if ((unsigned int)ReadGTimer(fp)
			   > (unsigned int)fp->hdx->node_deadline) {
			fp->flags |= V22FP_FLAGS_TIMEOUT;
			fp->status = V22_MSG_ERROR5;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V22_MSG_ERROR5\n");
		}
		break;

	case V22_ANS_NODE_4:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_answer,NODE_4\n");

		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_S1);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;
		DescrambleDataV22(fp, rxsym, nsym);

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		if ((unsigned int)ReadGTimer(fp) > V22_ANS_NODE_4_MS)
			fp->hdx->connect_substate = V22_NODE_2400A;
		break;

	case V22_NODE_2400A:
	case V22_NODE_2400B:
	case V22_NODE_2400C:
	case V22_NODE_2400D:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_answer, NODE %d\n ",
					     (int)fp->hdx->connect_substate);
		connect_2400(fp, txsym, txout, rxin, rxsym, txcount, rxcount);
		break;

	case V22_NODE_1200_12:
	case V22_NODE_1200_13:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_answer, NODE %d\n ",
					     (int)fp->hdx->connect_substate);
		connect_1200(fp, txsym, txout, rxin, rxsym, txcount, rxcount);
		break;

	case V22_ANS_NODE_SILENCE_AFTER_2100:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V22_answer,NODE_SILENCE_AFTER_2100\n");

		if (++iSilenceAfter2100 == V22_ANS_SILENCE_BLOCKS)
			fp->hdx->connect_substate = V22_ANS_NODE_3;
		TxNOP(fp, txsym, txout, (short *)txcount);
		break;

	default:
		break;
	}
}

/* ------------------------------------------------------------------------ */

void
v22_originate(struct v22fp *fp, unsigned short *txsym, short *txout,
	      short *rxin, unsigned short *rxsym,
	      unsigned short *txcount, unsigned short *rxcount)
{
	struct v22_status st;
	struct fpm_tone *tone;
	unsigned short nsym;
	short detected;
	short ones;
	short gap = 0;

	fp->status = V22_STATUS_01;

	switch (fp->hdx->connect_substate) {
	case V22_ORG_NODE_0:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_originate,NODE_0\n");

		fp->hdx->gtimer = 0;
		fp->hdx->r08 = 0;
		fp->hdx->r0a = 0;
		fp->hdx->connect_substate = V22_ORG_NODE_1;

		/*
		 * THE REPORT IS ASKED FOR AND THEN IGNORED.  `v22_answer`'s
		 * NODE_0 tests one bit of it; this one tests nothing, and no
		 * path here reads `st` at all.  The call is in the object and
		 * it is here.
		 */
		V22_status(fp, &st);

		TxNOP(fp, txsym, txout, (short *)txcount);

		*rxcount = 0;
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		SetTxRate(fp, V22_RATE_1200);
		SetRxRate(fp, V22_RATE_1200);

		tone = fp->hdx->tone;
		tone->cfg.freq = V22_ORG_TONE_HZ;
		tone->cfg.rev_period = 0;
		tone->cfg.scale = (short)((tone->cfg.scale
					   * V22_ORG_TONE_SCALE_Q15) >> 15);
		FPM_TONE_create(fp->hdx->tone, &fp->hdx->tone->cfg);
		break;

	case V22_ORG_NODE_1:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_originate, NODE_1\n");

		TxNOP(fp, txsym, txout, (short *)txcount);

		if (Detect_v22(fp, rxin)) {
			/*
			 * The demodulated count is stored and then thrown away
			 * two statements later by the unconditional clear
			 * below.  Both stores are the object's.
			 */
			*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
			fp->hdx->gtimer = 0;
			fp->hdx->connect_substate = V22_ORG_NODE_3;
			fp->flags |= V22FP_FLAG_1D_BIT4;
		}

		*rxcount = 0;
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		if ((unsigned int)ReadGTimer(fp)
		    > (unsigned int)fp->hdx->node_deadline) {
			fp->flags |= V22FP_FLAGS_TIMEOUT;
			fp->status = V22_MSG_ERROR1;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V22_MSG_ERROR1\n");
		}
		break;

	case V22_ORG_NODE_3:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_originate, NODE_3\n");

		/* The received level for this block, kept for the mean. */
		fp->hdx->r28 = FPM_rms(rxin, V22_ORG_BLOCK);

		TxNOP(fp, txsym, txout, (short *)txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;

		if (nsym != 0) {
			fp->hdx->r2c += (unsigned short)fp->hdx->r28;
			fp->hdx->r08 = (short)(fp->hdx->r08
					       + Detect_1s(rxsym, rxcount,
							   V22_ORG_DETECT_BPS,
							   V22_ORG_DETECT_THRESH));
			fp->hdx->r32++;
		}

		if ((unsigned short)fp->hdx->r08 > V22_ORG_NODE_3_RUN_MS) {
			fp->hdx->gtimer = 0;
			fp->hdx->r08 = 0;
			fp->hdx->connect_substate = V22_ORG_NODE_4;
			/*
			 * The mean level over the blocks counted.  An UNSIGNED
			 * divide, and by `r32` -- which is zero until the first
			 * block with symbols in it.  See v22org.h.
			 */
			fp->hdx->r2c = (int)((unsigned int)fp->hdx->r2c
					     / (unsigned short)fp->hdx->r32);
			if ((unsigned int)fp->hdx->r2c
			    > (unsigned short)fp->hdx->r30)
				fp->hdx->rx_shift = 1;
		}

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		if ((unsigned int)ReadGTimer(fp)
		    > (unsigned int)fp->hdx->node_deadline) {
			fp->flags |= V22FP_FLAGS_TIMEOUT;
			fp->status = V22_MSG_ERROR3;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V22_MSG_ERROR3\n");
		}
		break;

	case V22_ORG_NODE_4:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_originate, NODE_4\n");

		TxNOP(fp, txsym, txout, (short *)txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;
		DescrambleDataV22(fp, rxsym, nsym);

		if ((unsigned int)ReadGTimer(fp) > V22_ORG_NODE_4_MS) {
			fp->hdx->connect_substate = V22_ORG_NODE_5;
			SetAdaptEqV22(fp, V22_ORG_EQ_MODE_NODE_5);
		}

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		break;

	case V22_ORG_NODE_5:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_originate, NODE_5\n");

		/*
		 * THE PATTERN AND THE SCRAMBLER ARE THE SAME TEST TWICE.  At
		 * 1200 the node sends scrambled ones; at 2400 it sends the
		 * unscrambled S1 pattern.  The object reads `params.bps2`
		 * twice rather than keeping the flag, which is what the two
		 * `cmpw $0x4b0,0x4(%esi)` say.
		 */
		MakeTxData((short *)txsym, (const short *)txcount,
			   (short)(fp->params.bps2 == V22_STATUS_BPS_1200
				   ? V22_TXDATA_ONES_1200 : V22_TXDATA_S1));
		if (fp->params.bps2 == V22_STATUS_BPS_1200)
			ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);

		detected = FPM_MTD_detect(fp->hdx->mtd_s1, fp->dsp->rx_scratch,
					  fp->dsp->rx_count);

		if (*rxcount != 0 && SignalDetect(fp) == 1) {
			/*
			 * The same two counters NODE_6 judges, and NOTE that
			 * this copy keeps no `gap`: NODE_5 reaches no verdict,
			 * so nothing here needs to know whether the detector
			 * fired on this particular block.
			 */
			if (detected != 0) {
				if ((unsigned short)fp->hdx->r08
				    <= V22_ORG_MIN_RUN_MS)
					fp->hdx->r08 = 0;
			} else {
				fp->hdx->r08 = (short)(fp->hdx->r08
						       + V22_BLOCK_MS);
			}

			ones = (short)Detect_1s(rxsym, rxcount,
						V22_ORG_DETECT_BPS,
						V22_ORG_DETECT_THRESH);
			if (ones == 0) {
				DescrambleDataV22(fp, rxsym, *rxcount);
				fp->hdx->r0a =
				    (short)(fp->hdx->r0a
					    + Detect_1s(rxsym, rxcount,
							V22_ORG_DETECT_BPS,
							V22_ORG_DETECT_THRESH));
			}
		}

		if ((unsigned int)ReadGTimer(fp) > V22_ORG_NODE_5_MS)
			fp->hdx->connect_substate = V22_ORG_NODE_6;

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		break;

	case V22_ORG_NODE_6:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_originate, NODE_6\n");

		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_1200);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);

		detected = FPM_MTD_detect(fp->hdx->mtd_s1, fp->dsp->rx_scratch,
					  fp->dsp->rx_count);

		if (*rxcount != 0 && SignalDetect(fp) == 1) {
			if (detected != 0) {
				gap = 0;
				if ((unsigned short)fp->hdx->r08
				    <= V22_ORG_MIN_RUN_MS)
					fp->hdx->r08 = 0;
			} else {
				gap = V22_BLOCK_MS;
				fp->hdx->r08 = (short)(fp->hdx->r08
						       + V22_BLOCK_MS);
			}

			ones = (short)Detect_1s(rxsym, rxcount,
						V22_ORG_DETECT_BPS,
						V22_ORG_DETECT_THRESH);
			if (ones == 0) {
				DescrambleDataV22(fp, rxsym, *rxcount);
				fp->hdx->r0a =
				    (short)(fp->hdx->r0a
					    + Detect_1s(rxsym, rxcount,
							V22_ORG_DETECT_BPS,
							V22_ORG_DETECT_THRESH));
			}
		}

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		/*
		 * The 2400 verdict, and NOTE what it does NOT do: it neither
		 * prints nor sets the `r1e` bit its 1200 sibling sets, and it
		 * does not look at `params.bps2` the way `v22_answer`'s does.
		 */
		if ((unsigned short)fp->hdx->r08 > V22_ORG_MIN_RUN_MS
		    && gap == 0) {
			fp->hdx->gtimer = 0;
			fp->hdx->r08 = 0;
			fp->hdx->r0a = 0;
			fp->hdx->connect_substate = V22_NODE_2400A;
			SetAdaptEqV22(fp, V22_ORG_EQ_MODE_CARRIER);
			FPM_AGC_Freeze(&fp->dsp->agc);
		} else if ((unsigned short)fp->hdx->r0a > V22_ORG_ONES_MS) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V.22 %d modem det true\n",
				    V22_ORG_DET_TRUE_ARG);
			fp->r1e[0] |= V22FP_R1E_BIT3;
			fp->hdx->gtimer = 0;
			fp->hdx->r08 = 0;
			fp->hdx->r0a = 0;
			fp->hdx->connect_substate = V22_NODE_1200_12;
			SetAdaptEqV22(fp, V22_ORG_EQ_MODE_CARRIER);
			FPM_AGC_Freeze(&fp->dsp->agc);
		} else if ((unsigned int)ReadGTimer(fp) > V22_ORG_NODE_6_MS) {
			fp->flags |= V22FP_FLAGS_TIMEOUT;
			fp->status = V22_MSG_ERROR4;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V22_MSG_ERROR4\n");
		}
		break;

	case V22_NODE_2400A:
	case V22_NODE_2400B:
	case V22_NODE_2400C:
	case V22_NODE_2400D:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_originate, NODE %d\n ",
					     (int)fp->hdx->connect_substate);
		/* Set here and NOT on the connect_1200 arm below. */
		fp->r1e[0] |= V22FP_R1E_BIT3;
		connect_2400(fp, txsym, txout, rxin, rxsym, txcount, rxcount);
		break;

	case V22_NODE_1200_12:
	case V22_NODE_1200_13:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_originate, NODE %d\n ",
					     (int)fp->hdx->connect_substate);
		connect_1200(fp, txsym, txout, rxin, rxsym, txcount, rxcount);
		break;

	default:
		break;
	}
}

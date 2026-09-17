/* v22org.c -- V.22 originate protocol state. */

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
		fp->hdx->ones_detect_ms = 0;
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
		fp->hdx->rx_rms = FPM_rms(rxin, V22_ORG_BLOCK);

		TxNOP(fp, txsym, txout, (short *)txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;

		if (nsym != 0) {
			fp->hdx->rms_accum += (unsigned short)fp->hdx->rx_rms;
			fp->hdx->r08 = (short)(fp->hdx->r08
					       + Detect_1s(rxsym, rxcount,
							   V22_ORG_DETECT_BPS,
							   V22_ORG_DETECT_THRESH));
			fp->hdx->rms_blocks++;
		}

		if ((unsigned short)fp->hdx->r08 > V22_ORG_NODE_3_RUN_MS) {
			fp->hdx->gtimer = 0;
			fp->hdx->r08 = 0;
			fp->hdx->connect_substate = V22_ORG_NODE_4;
			/*
			 * The mean level over the blocks counted.  An UNSIGNED
			 * divide, and by `rms_blocks` -- which is zero until the first
			 * block with symbols in it.  See v22org.h.
			 */
			fp->hdx->rms_accum = (int)((unsigned int)fp->hdx->rms_accum
					     / (unsigned short)fp->hdx->rms_blocks);
			if ((unsigned int)fp->hdx->rms_accum
			    > (unsigned short)fp->hdx->rms_threshold)
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
				fp->hdx->ones_detect_ms =
				    (short)(fp->hdx->ones_detect_ms
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
				fp->hdx->ones_detect_ms =
				    (short)(fp->hdx->ones_detect_ms
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
			fp->hdx->ones_detect_ms = 0;
			fp->hdx->connect_substate = V22_NODE_2400A;
			SetAdaptEqV22(fp, V22_ORG_EQ_MODE_CARRIER);
			FPM_AGC_Freeze(&fp->dsp->agc);
		} else if ((unsigned short)fp->hdx->ones_detect_ms > V22_ORG_ONES_MS) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V.22 %d modem det true\n",
				    V22_ORG_DET_TRUE_ARG);
			fp->r1e[0] |= V22FP_R1E_BIT3;
			fp->hdx->gtimer = 0;
			fp->hdx->r08 = 0;
			fp->hdx->ones_detect_ms = 0;
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

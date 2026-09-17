/*
 * v22hdx.c -- V.22 / V.22bis: the retrain and originate-remote-loopback-2
 * protocol state handlers.
 *
 *   v22_retrain      .text 0x0892c0  2,284 bytes
 *   v22_org_rmloop2  .text 0x089bb0  1,050 bytes
 *
 * The two are ADJACENT in the object -- 0x892c0 + 2,284 is 0x89bac and the
 * next function starts at 0x89bb0 -- so they are written here in that order.
 *
 * See include/dsplib/v22hdx.h for what a handler is, what the seven arguments
 * are, and why the pointer types are the callees' rather than this file's.
 *
 * THE SHAPE EVERY SUBSTATE HAS.  Transmit first, then receive, then clamp,
 * then look at the clock:
 *
 *     MakeTxData      lay a pattern down in txdata
 *     ScrambleDataV22 optionally scramble it in place
 *     ModDataV22      modulate it into txout; *txcount becomes a sample count
 *     DemodDataV22    demodulate rxin into rxsym; *rxcount becomes a symbol
 *                     count
 *     ...             whatever this substate is watching for
 *     RxClampV22      overwrite rxsym and *rxcount on the way out
 *     ReadGTimer      tick, and compare against this substate's timeout
 *
 * so a substate differs from its neighbours only in which pattern it sends,
 * which detector it runs, and how long it is willing to wait.
 *
 * ONE THING IS NOT REPRODUCED FOR ITS OWN SAKE AND IS WORTH POINTING AT.
 * `v22_retrain` substate 7 calls `RxClampV22` TWICE when `RxTrained2400`
 * succeeds: once inside the success arm and once on the common path it falls
 * into.  Both calls are in the object -- two separate argument set-ups and two
 * separate `call` instructions, 0x89b52 and 0x8952f -- and `RxClampV22` is
 * idempotent, so no differential test can tell one call from two.  It is
 * written as the object has it.
 */

#include "dsplib/v22data.h"
#include "dsplib/v22det.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22hdx.h"
#include "dsplib/v22prc.h"
#include "dsplib/v22rate.h"

#include "dsplib/fpm_mtd.h"

/*
 * Retrain.
 *
 * `added` is the object's 16-bit stack slot at 0x1c(%esp): how many
 * milliseconds THIS block contributed to `hdx->r08`.  Substates 0 and 3 both
 * use it, and both use it the same way -- `r08` non-zero AND `added` zero is
 * the first block after a run of the thing they were counting ENDED, which is
 * what advances the state.  The initialisation is shared and hoisted to the
 * top of the function in the object, which is why it is written here rather
 * than in each arm.
 */
void
v22_retrain(struct v22fp *fp, unsigned short *txdata, short *txout,
	    short *rxin, unsigned short *rxsym, unsigned short *txcount,
	    unsigned short *rxcount)
{
	short added = 0;
	short mtd;

	fp->status = 2;

	switch (fp->hdx->connect_substate) {
	case 0:
		/*
		 * Send scrambled ones at whatever rate is current and wait for
		 * the far end's own signal to stop.
		 */
		MakeTxData((short *)txdata, (const short *)txcount,
			   (short)(V22_TXDATA_ONES_1200 +
				   (fp->params.bps2 != V22_HDX_BPS_1200)));
		ScrambleDataV22(fp, txdata, *txcount);
		*txcount = ModDataV22(fp, txdata, txout, *txcount);

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		mtd = FPM_MTD_detect(fp->hdx->mtd_s1, fp->dsp->rx_scratch,
				     fp->dsp->rx_count);
		if (*rxcount != 0) {
			added = 0;
			if (mtd == FPM_MTD_ABSENT) {
				added = V22_HDX_TICK_MS;
				fp->hdx->r08 += V22_HDX_TICK_MS;
			}
		}
		if (fp->hdx->r08 != 0 && added == 0) {
			fp->hdx->gtimer = 0;
			fp->hdx->r08 = 0;
			fp->hdx->connect_substate = 1;
			SetAdaptEqV22(fp, 2);
		}

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		if ((unsigned int)ReadGTimer(fp) > V22_RETRAIN_T_SILENCE) {
			SetTxRate(fp, V22_RATE_2400);
			SetRxRate(fp, V22_RATE_2400);
			SetAdaptEqV22(fp, 3);
			fp->hdx->protocol = 0;
			fp->hdx->connect_substate = 0;
		}
		break;

	case 1:
		/*
		 * Restart: zero the clock and the pattern timer, drop both
		 * directions to 1200, and send one block of unscrambled S1.
		 * The `FPM_MTD_detect` verdict is discarded here -- the call is
		 * made for the running energy estimate it leaves in the
		 * detector -- and so is `ReadGTimer`'s, which is a tail call in
		 * the object.
		 */
		fp->hdx->gtimer = 0;
		fp->hdx->r08 = 0;
		fp->hdx->ones_detect_ms = 0;
		fp->hdx->connect_substate = 2;
		ResetRx(fp);
		SetTxRate(fp, V22_RATE_1200);
		SetRxRate(fp, V22_RATE_1200);

		MakeTxData((short *)txdata, (const short *)txcount, V22_TXDATA_S1);
		*txcount = ModDataV22(fp, txdata, txout, *txcount);

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		FPM_MTD_detect(fp->hdx->mtd_s1, fp->dsp->rx_scratch,
			       fp->dsp->rx_count);

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		ReadGTimer(fp);
		break;

	case 2:
		/*
		 * Keep sending S1 for 100 ms, counting how long the far end has
		 * been unrecognisable, and then branch on the halfword at
		 * hdx + 0x38: substate 3 when it is 1 and substate 4 otherwise.
		 */
		MakeTxData((short *)txdata, (const short *)txcount, V22_TXDATA_S1);
		*txcount = ModDataV22(fp, txdata, txout, *txcount);

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		mtd = FPM_MTD_detect(fp->hdx->mtd_s1, fp->dsp->rx_scratch,
				     fp->dsp->rx_count);
		if (*rxcount != 0 && mtd == FPM_MTD_ABSENT)
			fp->hdx->r08 += V22_HDX_TICK_MS;

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		if ((unsigned int)ReadGTimer(fp) > V22_RETRAIN_T_S1)
			fp->hdx->connect_substate =
				(short)(3 + (V22_HDX_R38(fp->hdx) != 1));
		break;

	case 3:
		/*
		 * The same wait as substate 0, but sending 1200 bit/s ones and
		 * with a hard stop that gives up on the retrain entirely.
		 */
		MakeTxData((short *)txdata, (const short *)txcount, V22_TXDATA_ONES_1200);
		ScrambleDataV22(fp, txdata, *txcount);
		*txcount = ModDataV22(fp, txdata, txout, *txcount);

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		mtd = FPM_MTD_detect(fp->hdx->mtd_s1, fp->dsp->rx_scratch,
				     fp->dsp->rx_count);
		if (*rxcount != 0) {
			added = 0;
			if (mtd == FPM_MTD_ABSENT) {
				added = V22_HDX_TICK_MS;
				fp->hdx->r08 += V22_HDX_TICK_MS;
			}
		}
		if (fp->hdx->r08 != 0 && added == 0) {
			fp->hdx->gtimer = 0;
			fp->hdx->r08 = 0;
			fp->hdx->connect_substate = 4;
			SetAdaptEqV22(fp, 2);
		}

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		if ((unsigned int)ReadGTimer(fp) > V22_RETRAIN_T_SILENCE) {
			RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
			SetTxRate(fp, V22_RATE_2400);
			SetRxRate(fp, V22_RATE_2400);
			SetAdaptEqV22(fp, 3);
			fp->flags |= 0x01;
			fp->status = 3;
		}
		break;

	case 4:
		/* 450 ms of 1200 bit/s ones, then bring the receiver up. */
		MakeTxData((short *)txdata, (const short *)txcount, V22_TXDATA_ONES_1200);
		ScrambleDataV22(fp, txdata, *txcount);
		*txcount = ModDataV22(fp, txdata, txout, *txcount);

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		if ((unsigned int)ReadGTimer(fp) > V22_RETRAIN_T_ONES1200) {
			SetRxRate(fp, V22_RATE_2400);
			fp->hdx->connect_substate = 5;
		}
		break;

	case 5:
		/*
		 * Still sending 1200 bit/s ones, but now descrambling what
		 * comes back -- the receiver moved to 2400 at the end of
		 * substate 4.  After 600 ms the transmitter follows.
		 */
		MakeTxData((short *)txdata, (const short *)txcount, V22_TXDATA_ONES_1200);
		ScrambleDataV22(fp, txdata, *txcount);
		*txcount = ModDataV22(fp, txdata, txout, *txcount);

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		DescrambleDataV22(fp, rxsym, *rxcount);

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		if ((unsigned int)ReadGTimer(fp) > V22_RETRAIN_T_DESCR1200) {
			SetTxRate(fp, V22_RATE_2400);
			fp->hdx->connect_substate = 6;
		}
		break;

	case 6:
		/* Both directions at 2400 now; 800 ms, then let the equaliser
		 * adapt in its second mode. */
		MakeTxData((short *)txdata, (const short *)txcount, V22_TXDATA_ONES_2400);
		ScrambleDataV22(fp, txdata, *txcount);
		*txcount = ModDataV22(fp, txdata, txout, *txcount);

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		DescrambleDataV22(fp, rxsym, *rxcount);

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		if ((unsigned int)ReadGTimer(fp) > V22_RETRAIN_T_ONES2400) {
			SetAdaptEqV22(fp, 3);
			fp->hdx->connect_substate = 7;
		}
		break;

	case 7:
		/*
		 * The last wait.  `RxTrained2400` succeeding is what ends the
		 * retrain; two seconds without it ends it the other way, and
		 * both exits stamp `fp->status` the same.  The extra
		 * `RxClampV22` is the object's -- see the file header.
		 */
		MakeTxData((short *)txdata, (const short *)txcount, V22_TXDATA_ONES_2400);
		ScrambleDataV22(fp, txdata, *txcount);
		*txcount = ModDataV22(fp, txdata, txout, *txcount);

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		DescrambleDataV22(fp, rxsym, *rxcount);
		if (RxTrained2400((const short *)rxsym,
				  (const unsigned short *)rxcount)) {
			fp->flags |= 0x29;
			fp->status = 3;
			RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		}

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		if ((unsigned int)ReadGTimer(fp) > V22_RETRAIN_T_TRAIN2400) {
			fp->flags |= 0x01;
			fp->status = 3;
		}
		break;
	}
}

/*
 * The originating end of remote loopback 2.
 *
 * `ack` is the object's 16-bit stack slot at 0x18(%esp), and only substate 1
 * reads it: an acknowledgement seen in the same block that took `hdx->r08`
 * past its threshold means the far end is already looping, so the equaliser is
 * left alone; no acknowledgement means it has to be told to adapt.
 */
void
v22_org_rmloop2(struct v22fp *fp, unsigned short *txdata, short *txout,
		short *rxin, unsigned short *rxsym, unsigned short *txcount,
		unsigned short *rxcount)
{
	short ack = 0;

	fp->r1e[0] |= 0x02;
	fp->status = 1;

	switch (fp->hdx->connect_substate) {
	case 0:
		/*
		 * Entry: clear the clock, the pattern timer and the "equaliser
		 * has been told" flag, stop the equaliser adapting, and send
		 * one block of unscrambled ones.
		 */
		fp->hdx->gtimer = 0;
		fp->hdx->r08 = 0;
		fp->hdx->trained = 0;
		SetAdaptEqV22(fp, 1);

		MakeTxData((short *)txdata, (const short *)txcount,
			   (short)(V22_TXDATA_ONES_1200 +
				   (fp->params.bps2 != V22_HDX_BPS_1200)));
		*txcount = ModDataV22(fp, txdata, txout, *txcount);

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		if (*rxcount != 0)
			fp->hdx->r08 = (short)(fp->hdx->r08 + Detect_Rmloop2_ACK(
				rxsym, (const unsigned short *)rxcount,
				fp->params.bps2));

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		fp->hdx->connect_substate = 1;
		break;

	case 1:
		/*
		 * Count acknowledgement.  231 ms of it moves on; 1,300 ms
		 * without it gives up and goes back to substate 0.
		 */
		MakeTxData((short *)txdata, (const short *)txcount,
			   (short)(V22_TXDATA_ONES_1200 +
				   (fp->params.bps2 != V22_HDX_BPS_1200)));
		*txcount = ModDataV22(fp, txdata, txout, *txcount);

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		if (*rxcount != 0) {
			ack = (short)Detect_Rmloop2_ACK(
				rxsym, (const unsigned short *)rxcount,
				fp->params.bps2);
			fp->hdx->r08 = (short)(fp->hdx->r08 + ack);
		}

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		if ((unsigned short)fp->hdx->r08 > V22_RMLOOP2_PATTERN_MS) {
			fp->hdx->gtimer = 0;
			fp->hdx->r08 = 0;
			fp->hdx->connect_substate = 2;
			if (ack == 0) {
				fp->hdx->trained = 1;
				SetAdaptEqV22(fp, 3);
			}
		}
		if ((unsigned int)ReadGTimer(fp) > V22_RMLOOP2_TIMEOUT_MS) {
			SetAdaptEqV22(fp, 3);
			fp->hdx->protocol = 0;
			fp->hdx->connect_substate = 0;
		}
		break;

	case 2:
		/*
		 * Looped: send SCRAMBLED ones and descramble what comes back,
		 * which is the loop closing.  `hdx->trained` remembers that the
		 * equaliser has already been told to adapt, so the
		 * acknowledgement is only looked for until it has been.
		 *
		 * `Detect_1s` runs on every block, acknowledged or not, and
		 * takes its threshold from here rather than carrying one.
		 */
		MakeTxData((short *)txdata, (const short *)txcount,
			   (short)(V22_TXDATA_ONES_1200 +
				   (fp->params.bps2 != V22_HDX_BPS_1200)));
		ScrambleDataV22(fp, txdata, *txcount);
		*txcount = ModDataV22(fp, txdata, txout, *txcount);

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		if (*rxcount != 0) {
			if (fp->hdx->trained == 0
			    && (short)Detect_Rmloop2_ACK(
				       rxsym, (const unsigned short *)rxcount,
				       fp->params.bps2) == 0) {
				fp->hdx->trained = 1;
				SetAdaptEqV22(fp, 3);
			}
			DescrambleDataV22(fp, rxsym, *rxcount);
		}
		{
			short n = Detect_1s(rxsym,
					     (const unsigned short *)rxcount,
					     fp->params.bps2,
					     V22_RMLOOP2_ONES_Q15);
			fp->hdx->r08 = (short)(fp->hdx->r08 + n);
		}

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		if ((unsigned short)fp->hdx->r08 > V22_RMLOOP2_PATTERN_MS) {
			fp->hdx->gtimer = 0;
			fp->hdx->r08 = 0;
			SetAdaptEqV22(fp, 3);
			fp->status = 6;
		}
		if ((unsigned int)ReadGTimer(fp) > V22_RMLOOP2_TIMEOUT_MS) {
			SetAdaptEqV22(fp, 3);
			fp->status = 7;
		}
		break;
	}
}

/*
 * v22ans.c -- V.22 / V.22bis: the data state and the answering station's
 * remote-loopback state.  See include/dsplib/v22ans.h, which carries the
 * evidence for the argument list, the field readings and the status codes.
 *
 * WRITTEN IN THE OBJECT'S ORDER: v22_data at 0x088910, v22_ans_rmloop2 at
 * 0x089fd0.  CLAUDE.md's register-allocation lever follows a translation
 * unit's emission order, so the two are laid out here the way the blob lays
 * them out even though nothing yet says they share a translation unit.
 *
 * THE INSTANCE POINTER IS RE-READ AFTER EVERY CALL.  A load of the object's
 * +0x50 appears NINE times in `v22_data` and SEVEN in `v22_ans_rmloop2` --
 * counted from `dis.py`, not remembered -- always with the instance itself
 * live in a callee-saved register, so the object is reaching `fp->hdx`
 * through the object rather than through a local that survives the call.
 * v22status.c records the same shape for the same reason.
 *
 * TWO ARMS THAT SHARE A TAIL, and both are the shape finding F8528 warns
 * about:
 *
 *   - `v22_ans_rmloop2` sub-states 1 and 2 differ only in the timer limit
 *     they compare against; sub-state 1's arm jumps into the middle of
 *     sub-state 2's, four instructions past the comparison, and shares the
 *     `SetAdaptEqV22(fp, 3)` and the status store.  Reading the fall-through
 *     alone gives a complete-looking function with one timer.
 *   - `v22_data`'s retrain-request path does NOT rejoin the common tail: it
 *     jumps past `fp->flags |= 5` and past the status store, writing
 *     `fp->status` itself.  Every other path in the function goes through
 *     both.
 */

#include "dsplib/v22ans.h"

#include "dsplib/debug.h"
#include "dsplib/v22ctl.h"
#include "dsplib/v22data.h"
#include "dsplib/v22det.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22prc.h"
#include "dsplib/v22rate.h"
#include "dsplib/v22status.h"

/*
 * `hdx + 0x38`.  Sixteen bits, inside v22fp.h's `unsigned char r36[6]`, which
 * is the region `V22FP_create` never writes.  Both retrain paths set it and
 * the retrain-request path clears it; nothing reconstructed reads it back, so
 * it is reached here rather than modelled there.
 */
#define HDX_0038(h)	(*(short *)((h)->r36 + 2))

void
v22_data(struct v22fp *fp, unsigned short *txsym, short *txout,
	 short *rxin, unsigned short *rxsym, unsigned short *txcount,
	 unsigned short *rxcount)
{
	struct v22_status st;
	struct v22fp_hdx *hdx;
	/*
	 * The status this call will report.  Zero on every quiet block: the
	 * object clears the slot before the first branch and only the three
	 * message paths write it.
	 */
	int status = 0;

	V22_status(fp, &st);

	if (st.flags & V22_STATUS_SCRAMBLER)
		ScrambleDataV22(fp, txsym, *txcount);
	*txcount = ModDataV22(fp, txsym, txout, *txcount);

	if (fp->params.flags & V22_PARAMS_BIT11) {
		/*
		 * The receiver is not run at all on this arm -- the count is
		 * zeroed rather than demodulated -- and the only thing that
		 * happens is the carrier-loss timer, against `params.r08`
		 * instead of `params.r18`.  The comparison is UNSIGNED in the
		 * object (`ja`), which is what the cast reproduces.
		 */
		*rxcount = 0;
		hdx = fp->hdx;
		hdx->r3c++;
		if ((unsigned int)(hdx->r3c * V22_BLOCK_MS)
		    > (unsigned int)fp->params.r08) {
			status = V22_ST_NO_CARRIER;
			RxClampV22(fp, rxin, (short *)rxsym,
				   (short *)rxcount);
		}
	} else {
		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);

		/*
		 * The return is discarded.  The object calls it here and
		 * calls it AGAIN inside the retrain-level test below, so this
		 * is not a value being kept in a register across the branch;
		 * it is two calls.
		 */
		GetSignalQuality(fp);

		if (st.flags & V22_STATUS_R20)
			TxClockSync(fp);

		if (*rxcount != 0) {
			/*
			 * The retrain-request detector runs at 2400 only, and
			 * its result is narrowed to sixteen bits before the
			 * test -- see the header on `Detect_Retrain`'s type.
			 */
			if (fp->dsp->r2a == 1
			    && (short)Detect_Retrain(rxsym, rxcount) > 0) {
				hdx = fp->hdx;
				hdx->gtimer = 0;
				hdx->r08 = 0;
				hdx->r0a = 0;
				hdx->r0c = 0;
				HDX_0038(hdx) = 0;
				ResetRx(fp);
				SetAdaptEqV22(fp, 1);
				SetTxRate(fp, V22_RATE_1200);
				SetRxRate(fp, V22_RATE_1200);
				fp->hdx->r0e = V22_HDX_R0E_SIX;
				RxClampV22(fp, rxin, (short *)rxsym,
					   (short *)rxcount);
				/*
				 * This path leaves without the common tail:
				 * no `fp->flags |= 5`, and the status is
				 * stored here rather than at the bottom.
				 */
				fp->status = V22_ST_RETRAIN_REQ;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V.22: Retrain request "
						"detected.");
				return;
			}
			if (st.flags & V22_STATUS_DESCRAMBLER)
				DescrambleDataV22(fp, rxsym, *rxcount);
		}

		/* The retrain-level timer, at 2400 and with bit 9 set only. */
		if (fp->dsp->r2a == 1 && (st.flags & V22_STATUS_PARAM_BIT9)) {
			if (GetSignalQuality(fp) > V22_DATA_RETRAIN_LEVEL) {
				fp->hdx->r08 = 0;
			} else {
				hdx = fp->hdx;
				hdx->r08 = (short)((unsigned short)hdx->r08
						   + V22_BLOCK_MS);
				if ((unsigned short)hdx->r08
				    > V22_DATA_QUALITY_MAX) {
					hdx->r0e = V22_HDX_R0E_SIX;
					hdx->r0c = 1;
					SetAdaptEqV22(fp, 1);
					status = V22_ST_RETRAIN;
					HDX_0038(fp->hdx) = 1;
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
							"Signal quality < "
							"Retrain level. "
							"Retrain initiated.");
				}
			}
		}

		if (CarrierDetect(fp)) {
			hdx = fp->hdx;
			if (hdx->r3c > V22_DATA_CARRIER_BACK
			    && fp->dsp->r2a == 1) {
				hdx->r0e = V22_HDX_R0E_SIX;
				hdx->r0c = 1;
				SetAdaptEqV22(fp, 1);
				status = V22_ST_RETRAIN;
				hdx = fp->hdx;
				HDX_0038(hdx) = 1;
				hdx->r3c = 0;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"Carrier back during "
						"carrier_loss_time (v22_data)"
						". Retrain initiated.");
			} else {
				hdx->r3c = 0;
			}
		} else {
			hdx = fp->hdx;
			hdx->r3c++;
			/*
			 * SIGNED and sixteen bits: the object narrows the
			 * product with `movswl` and compares the two halves
			 * as words.  The format string names both -- the
			 * product is `carrier_loss_time` and `params.r18` is
			 * the limit it is measured against.
			 */
			if (fp->params.r18
			    >= (short)(hdx->r3c * V22_BLOCK_MS)) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V22_MSG_NO_CARRIER won't be "
						"reported (carrier_loss_time "
						"%d of %d ms)\n",
						(int)(short)(hdx->r3c
							     * V22_BLOCK_MS),
						(int)fp->params.r18);
			} else {
				status = V22_ST_NO_CARRIER;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V22_MSG_NO_CARRIER2\n");
				RxClampV22(fp, rxin, (short *)rxsym,
					   (short *)rxcount);
			}
		}
	}

	fp->flags = (unsigned char)(fp->flags | V22_DATA_FLAGS_SET);
	fp->status = (unsigned char)status;
}

void
v22_ans_rmloop2(struct v22fp *fp, unsigned short *txsym, short *txout,
		short *rxin, unsigned short *rxsym, unsigned short *txcount,
		unsigned short *rxcount)
{
	struct v22_status st;
	struct v22fp_hdx *hdx = fp->hdx;
	/*
	 * Set on sub-state 1's detector arm only, and read there.  It stays
	 * zero when the demodulator produced nothing, which is the case that
	 * decides whether the transition into sub-state 2 also freezes the
	 * equaliser.
	 */
	int detected = 0;

	fp->r1e[0] = (unsigned char)(fp->r1e[0] | V22_ANS_R1E_SET);
	fp->status = V22_ST_ANS_RMLOOP2;

	switch (hdx->r0c) {
	case V22_RMLOOP2_START:
		hdx->gtimer = 0;
		hdx->r08 = 0;
		hdx->r10 = 0;
		hdx->r0c = V22_RMLOOP2_DETECT;
		SetAdaptEqV22(fp, 1);

		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);
		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		if (*rxcount != 0) {
			int n = Detect_1s(rxsym, rxcount, fp->params.bps,
					  V22_DET_THRESH_Q15);

			hdx = fp->hdx;
			hdx->r08 = (short)((unsigned short)hdx->r08 + n);
		}
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		break;

	case V22_RMLOOP2_DETECT:
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);
		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		if (*rxcount != 0) {
			detected = (short)Detect_1s(rxsym, rxcount,
						    fp->params.bps,
						    V22_DET_THRESH_Q15);
			hdx = fp->hdx;
			hdx->r08 = (short)((unsigned short)hdx->r08
					   + detected);
		}
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		hdx = fp->hdx;
		if ((unsigned short)hdx->r08 > V22_RMLOOP2_DETECT_MAX) {
			hdx->gtimer = 0;
			hdx->r08 = 0;
			hdx->r0c = V22_RMLOOP2_ANSWER;
			/*
			 * The equaliser is frozen into sub-state 2 only when
			 * the last block produced no symbols at all -- the
			 * detector's own zero does NOT reach here, because
			 * `detected` is only written when the count was
			 * non-zero.
			 */
			if (detected == 0) {
				hdx->r10 = 1;
				SetAdaptEqV22(fp, 3);
			}
		}
		/*
		 * The shared tail.  Sub-state 2 reaches these three lines with
		 * V22_RMLOOP2_T2_MS in place of V22_RMLOOP2_T1_MS and nothing
		 * else changed; the comparison is UNSIGNED in the object.
		 */
		if ((unsigned int)ReadGTimer(fp) > V22_RMLOOP2_T1_MS) {
			SetAdaptEqV22(fp, 3);
			fp->status = V22_ST_07;
		}
		break;

	case V22_RMLOOP2_ANSWER:
		MakeTxData((short *)txsym, (const short *)txcount,
			   fp->params.bps == V22_RMLOOP2_BPS_1200
				   ? V22_TXDATA_SYMBOL_2
				   : V22_TXDATA_SYMBOL_10);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);
		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		if (*rxcount != 0
		    && (short)Detect_1s(rxsym, rxcount, fp->params.bps,
					V22_DET_THRESH_Q15) == 0) {
			hdx = fp->hdx;
			hdx->r10 = 0;
			hdx->r08 = 0;
			hdx->r0c = V22_RMLOOP2_LOOP;
			SetAdaptEqV22(fp, 3);
			/*
			 * Clamped TWICE on this arm: once here and once on
			 * the common line below, which every other path in
			 * this sub-state also takes.
			 */
			RxClampV22(fp, rxin, (short *)rxsym,
				   (short *)rxcount);
			fp->status = V22_ST_05;
		}
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		if ((unsigned int)ReadGTimer(fp) > V22_RMLOOP2_T2_MS) {
			SetAdaptEqV22(fp, 3);
			fp->status = V22_ST_07;
		}
		break;

	case V22_RMLOOP2_LOOP:
		V22_status(fp, &st);
		/*
		 * The loop itself.  `txsym` is not read on this arm at all:
		 * the demodulated symbols are scrambled in place and
		 * modulated straight back out.
		 */
		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		ScrambleDataV22(fp, rxsym, *rxcount);
		*txcount = ModDataV22(fp, rxsym, txout, *rxcount);
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		if (CarrierDetect(fp)) {
			fp->hdx->r08 = 0;
		} else {
			hdx = fp->hdx;
			hdx->r08 = (short)((unsigned short)hdx->r08
					   + V22_BLOCK_MS);
			if ((unsigned short)hdx->r08 > V22_RMLOOP2_LOSS_MAX) {
				fp->status = V22_ST_08;
				break;
			}
		}
		if (st.flags & V22_STATUS_R20)
			TxClockSync(fp);
		break;

	default:
		/* Silently, and with the two stores at the top standing. */
		break;
	}
}

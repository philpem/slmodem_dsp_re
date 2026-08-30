/*
 * v22loop.c -- V.22 / V.22bis: the local-loopback state of the protocol
 * machine.  See include/dsplib/v22loop.h, which carries the evidence for the
 * jump table, the sub-state numbering, the two accumulators and the one
 * prototype in the tree that this function proves wrong.
 *
 * ONE FUNCTION, 0x08a7a0, 1,104 bytes.  Nothing else lives in this file yet,
 * so there is no emission order to preserve; the object's neighbours in the
 * 0x8a460..0x8abf0 block are `connect_1200` (src/pump/v22/v22conn.c) and this,
 * with `v22_originate` and `v22_answer` between them.
 *
 * THE INSTANCE POINTER IS RE-READ AFTER EVERY CALL.  `mov 0x50(%ebx),...`
 * appears TWELVE times -- counted from `dis.py`, one at entry and eleven after
 * a call -- always with the instance itself live in `%ebx`, so `fp->hdx` is
 * reached through the object and never
 * through a local that survives a call.  The one place a local IS carried
 * across is sub-state 3's `hdx` at the very top, which is used for the two
 * loads before the first call and then dropped.  v22ans.c and v22status.c
 * record the same shape.
 *
 * THE ARM THAT JUMPS INTO ANOTHER ONE.  Finding F8528's shape is here twice
 * and neither is where it looks:
 *
 *   - Sub-state 0's "bit clear" branch (0x8aaf8) sets `hdx->r0c` to 3 and then
 *     jumps BACKWARDS into the middle of sub-state 1's timeout arm (0x8aa0a),
 *     four instructions past the store that sets `r0c` to 2.  So the two share
 *     `fp->flags |= 0x10`, `SetTxRate(fp, 0)` and `SetRxRate(fp, 0)`, and
 *     reading either arm's fall-through alone gives a complete-looking arm
 *     that writes the wrong sub-state.
 *   - Sub-state 3's two threshold arms (0x8ab06 and 0x8a8c1) differ in exactly
 *     one immediate -- the sub-state stored -- and share the four zeroing
 *     stores' shape and the whole of `SetAdaptEqV22(fp, 2)` at 0x8a8d9.
 */

#include "dsplib/v22loop.h"

#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v22conn.h"
#include "dsplib/v22data.h"
#include "dsplib/v22det.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22prc.h"
#include "dsplib/v22rate.h"
#include "dsplib/v22status.h"

void
v22_local_loop(struct v22fp *fp, unsigned short *txsym, short *txout,
	       short *rxin, unsigned short *rxsym, unsigned short *txcount,
	       unsigned short *rxcount)
{
	struct v22_status st;
	struct v22fp_hdx *hdx = fp->hdx;

	fp->r1e[0] = (unsigned char)(fp->r1e[0] | V22_LOOP_R1E_SET);
	fp->status = V22_STATUS_01;

	switch (hdx->r0c) {
	case V22_LOOP_NODE_0:
		hdx->gtimer = 0;
		hdx->r08 = 0;
		hdx->r0a = 0;

		V22_status(fp, &st);
		TxNOP(fp, txsym, txout, (short *)txcount);
		*rxcount = 0;
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		/*
		 * ONE BIT of the report decides the successor, and it is the
		 * byte at +0x15 and not the flags byte at +0x14 -- the object
		 * tests `0x35(%esp)` against a block based at `0x20(%esp)`.
		 * v22status.h establishes it as `params.flags` bit 10.
		 */
		if (st.flags2 & V22_STATUS2_PARAM_BIT10) {
			fp->hdx->r0c = V22_LOOP_NODE_1;
		} else {
			fp->hdx->r0c = V22_LOOP_NODE_3;
			fp->flags = (unsigned char)(fp->flags
						    | V22FP_FLAG_1D_BIT4);
			SetTxRate(fp, V22_RATE_1200);
			SetRxRate(fp, V22_RATE_1200);
		}
		break;

	case V22_LOOP_NODE_1:
		/*
		 * The object stores `FPM_TONE_generate`'s return into
		 * `*txcount`, and that return is its own `count` argument --
		 * see the derivation in v22loop.h.  fpm_tone.h declares the
		 * function `void`, so the value is spelled here as the
		 * constant the object would have got back.  The two are the
		 * same by that derivation and by nothing weaker; a retyped
		 * fpm_tone.h would let this become one statement again.
		 */
		FPM_TONE_generate(hdx->tone, txout, V22_TX_BLOCK);
		*txcount = V22_TX_BLOCK;

		*rxcount = 0;
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		if ((unsigned int)ReadGTimer(fp) > V22_LOOP_TONE_MS) {
			hdx = fp->hdx;
			hdx->gtimer = 0;
			hdx->r0c = V22_LOOP_NODE_2;
			fp->flags = (unsigned char)(fp->flags
						    | V22FP_FLAG_1D_BIT4);
			SetTxRate(fp, V22_RATE_1200);
			SetRxRate(fp, V22_RATE_1200);
		}
		break;

	case V22_LOOP_NODE_2:
		TxNOP(fp, txsym, txout, (short *)txcount);
		*rxcount = 0;
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		/*
		 * `rxin` and not `rxsym`: the clamp above writes the SYMBOL
		 * buffer, and the detector is handed the raw samples the
		 * caller supplied.  Note also the polarity -- fpm_tone.h
		 * records that zero means the tone IS present, so this waits
		 * for the line to go quiet rather than for the tone to
		 * arrive.
		 */
		if (FPM_TONE_detect(fp->hdx->tone, rxin, V22_TX_BLOCK)
		    == FPM_TONE_NOSIGNAL) {
			hdx = fp->hdx;
			hdx->gtimer = 0;
			hdx->r0c = V22_LOOP_NODE_3;
		}
		break;

	case V22_LOOP_NODE_3: {
		/*
		 * The transmit pattern: S1 at 2400 and continuous ones at
		 * 1200, with the 1200 pattern forced once `hdx->r08` is past
		 * V22_LOOP_S1_MS.  The object computes the comparison into a
		 * register with `sete` and then overwrites it, which is this
		 * shape and not a short-circuiting `||`.
		 *
		 * The override is DEAD from inside the machine: the only way
		 * r08 gets past V22_LOOP_S1_MS is the tail below, which in the
		 * same block zeroes it and leaves the sub-state.  It is
		 * reproduced because it is what the object encodes, and the
		 * test reaches it by poking r08.
		 */
		short pattern = (short)(fp->params.bps2 == V22_LOOP_BPS_1200);
		int detected;

		if ((unsigned short)hdx->r08 > V22_LOOP_S1_MS)
			pattern = V22_TXDATA_ONES_1200;

		MakeTxData((short *)txsym, (const short *)txcount, pattern);

		/*
		 * The scrambler gate here is the RATE and not the status
		 * block's bit 0, which is what `v22_data` uses.  Same
		 * `cmpw $0x4b0, 0x4(%ebx)` as the pattern above.
		 */
		if (fp->params.bps2 == V22_LOOP_BPS_1200)
			ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);

		/*
		 * The S1 detector runs on the rate converter's OUTPUT, not on
		 * the caller's samples and not on the symbols: `dsp->rx_count`
		 * and `dsp->rx_scratch` are what `DemodDataV22` just left
		 * behind, and v22fp.h establishes both.  It is called
		 * unconditionally, before the count is even looked at.
		 */
		detected = FPM_MTD_detect(fp->hdx->mtd_s1, fp->dsp->rx_scratch,
					  fp->dsp->rx_count);

		if (*rxcount != 0 && SignalDetect(fp) == 1) {
			if (detected != FPM_MTD_ABSENT) {
				/*
				 * The hysteresis.  Past V22_LOOP_S1_HOLD_MS a
				 * detection no longer resets the counter, so
				 * the run to V22_LOOP_S1_MS cannot be
				 * restarted by one good block near the end.
				 */
				hdx = fp->hdx;
				if ((unsigned short)hdx->r08
				    <= V22_LOOP_S1_HOLD_MS)
					hdx->r08 = 0;
			} else {
				hdx = fp->hdx;
				hdx->r08 = (short)((unsigned short)hdx->r08
						   + V22_LOOP_BLOCK_MS);
			}

			/*
			 * Narrowed to sixteen bits before the test -- see the
			 * note in v22loop.h on why the second call below is
			 * NOT narrowed.
			 */
			if ((short)Detect_1s(rxsym, rxcount, V22_LOOP_DET_BPS,
					     V22_LOOP_ONES_Q15) == 0) {
				DescrambleDataV22(fp, rxsym, *rxcount);
				hdx = fp->hdx;
				hdx->r0a = (short)((unsigned short)hdx->r0a
						   + Detect_1s(rxsym, rxcount,
							V22_LOOP_DET_BPS,
							V22_LOOP_ONES_Q15));
			}
		}

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		hdx = fp->hdx;
		if ((unsigned short)hdx->r08 > V22_LOOP_S1_MS) {
			hdx->gtimer = 0;
			hdx->r08 = 0;
			hdx->r0a = 0;
			hdx->r0c = V22_NODE_2400A;
			SetAdaptEqV22(fp, 2);
		} else if ((unsigned short)hdx->r0a > V22_LOOP_ONES_MS) {
			hdx->gtimer = 0;
			hdx->r08 = 0;
			hdx->r0a = 0;
			hdx->r0c = V22_NODE_1200_12;
			SetAdaptEqV22(fp, 2);
		} else if ((unsigned int)ReadGTimer(fp)
			   > (unsigned int)fp->hdx->r04) {
			fp->flags = (unsigned char)(fp->flags
						    | V22FP_FLAGS_TIMEOUT);
			fp->status = V22_STATUS_16;
		}
		break;
	}

	case V22_NODE_2400A:
	case V22_NODE_2400B:
	case V22_NODE_2400C:
	case V22_NODE_2400D:
		connect_2400(fp, txsym, txout, rxin, rxsym, txcount, rxcount);
		break;

	case V22_NODE_1200_12:
	case V22_NODE_1200_13:
		connect_1200(fp, txsym, txout, rxin, rxsym, txcount, rxcount);
		break;

	default:
		/* Silently, and with the two stores at the top standing. */
		break;
	}
}

/*
 * v22mod.c -- V.22 / V.22bis: the modem state machine.
 *
 * The blob's v22mod.c translation unit, recovered from the over-split
 * state-layer files.  `ld -r` concatenates .text in FILE order, so the run
 * between V22Dec.c's last function and v22prc.c's first is v22mod.c's, and
 * its LOCAL objects pin it: `V22_PROTOCOL` is read only by V22FP_modem and
 * `iSilenceAfter2100` only by v22_answer, so both ends of the run are in this
 * unit.  The handlers are the seven entries of V22_PROTOCOL, which is a
 * static table here; the tail primitives they call are v22prc.c's.
 *
 * Functions in the object's own emission order:
 *
 *   V22FP_modem       .text 0x0887b0
 *   v22_data          .text 0x088910
 *   connect_2400      .text 0x088cd0
 *   v22_retrain       .text 0x0892c0
 *   v22_org_rmloop2   .text 0x089bb0
 *   v22_ans_rmloop2   .text 0x089fd0
 *   connect_1200      .text 0x08a460
 *   v22_local_loop    .text 0x08a7a0
 *   v22_answer        .text 0x08abf0
 *   v22_originate     .text 0x08b2f0
 *
 * Every body moved VERBATIM from the layer files; no declaration, type or
 * flag changed.
 */

#include <string.h>

#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sysdep.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22_iir.h"
#include "dsplib/v22_mrf.h"
#include "dsplib/v22_pps.h"
#include "dsplib/v22_sre.h"
#include "dsplib/v22ans.h"
#include "dsplib/v22conn.h"
#include "dsplib/v22ctl.h"
#include "dsplib/v22data.h"
#include "dsplib/v22dec.h"
#include "dsplib/v22det.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22hdx.h"
#include "dsplib/v22loop.h"
#include "dsplib/v22org.h"
#include "dsplib/v22prc.h"
#include "dsplib/v22rate.h"
#include "dsplib/v22status.h"
#include "dsplib/v22tab.h"
#include "dsplib/v22txtab.h"

/*
 * `.bss` + 0x380, two bytes, LOCAL, and referenced by `v22_answer` alone --
 * node 1 clears it, node 14 increments it and compares it with four.  Read
 * with `movzwl`, which is what makes it unsigned.  `static` for the same
 * reason src/pump/v22/v22status.c's own `PROTOCOL` table is: the object's
 * symbol is LOCAL.
 */
static unsigned short iSilenceAfter2100;

/*
 * The three staging buffers, with the original's own names, from .bss:0x3a0,
 * 0x480 and 0x560.  All three are file-static in the object, so two V.22
 * datapumps in one process share them -- reproduced as-is, exactly as
 * b103fp.c reproduces its pair.  The names COLLIDE across modules: `nm` finds
 * three `tx_in_internal` and three `rx_out_internal` in the 1.2 MB, so
 * `tools/symmap.py` can only alias the unique one, `rx_in_internal`.
 *
 * The element types are the handler signature's, which is what forces them:
 * the two the handler sees as symbols are `unsigned short` -- the copy-out
 * below is a `movzwl` -- and the sample buffer is `short`.
 */
#define V22FP_TX_IN_ENTRIES	100	/* .bss 0x3a0, 0xc8 bytes */
#define V22FP_RX_OUT_ENTRIES	100	/* .bss 0x480, 0xc8 bytes */
#define V22FP_RX_IN_ENTRIES	160	/* .bss 0x560, 0x140 bytes */

static unsigned short tx_in_internal[V22FP_TX_IN_ENTRIES];
static unsigned short rx_out_internal[V22FP_RX_OUT_ENTRIES];
static short rx_in_internal[V22FP_RX_IN_ENTRIES];

/*
 * The seven protocol states, in the object's own order -- `.rodata` + 0x8544,
 * 28 bytes, LOCAL (`nm` shows a lower-case `r`, so `static const`).  Each
 * entry carries a RELOCATION naming its handler, so the order below is read
 * off the object rather than inferred; `include/dsplib/v22status.h` records
 * the same seven against `V22_status`'s own parallel table.
 *
 * `hdx->protocol` indexes it, sign-extended and WITH NO BOUNDS CHECK.
 */
static void (* const V22_PROTOCOL[7])(struct v22fp *fp, unsigned short *txsym,
				      short *txout, short *rxin,
				      unsigned short *rxsym,
				      unsigned short *txcount,
				      unsigned short *rxcount) = {
	v22_data,		/* 0 */
	v22_originate,		/* 1 */
	v22_answer,		/* 2 */
	v22_local_loop,		/* 3 */
	v22_org_rmloop2,	/* 4 */
	v22_ans_rmloop2,	/* 5 */
	v22_retrain		/* 6 */
};

/*
 * `hdx + 0x38`.  Sixteen bits, inside v22fp.h's `unsigned char r36[6]`, which
 * is the region `V22FP_create` never writes.  Both retrain paths set it and
 * the retrain-request path clears it; nothing reconstructed reads it back, so
 * it is reached here rather than modelled there.
 */
#define HDX_0038(h)	(*(short *)((h)->r36 + 2))

/* `hdx` + 0x38, a 16-bit field inside v22fp.h's unmodelled `r36[6]`. */
#define HDX_R38(h)	(*(short *)(void *)&(h)->r36[2])

int
V22FP_modem(struct v22fp *fp, const int *tx_bits, short *tx_out,
	    const short *rx_in, int *rx_bits, int *n_tx, int *n_rx)
{
	/*
	 * The handler's own pair, sixteen bits wide and adjacent on the
	 * stack.  Both are seeded from the caller's before anything else
	 * happens, which matters: the second staging loop below reads
	 * `*n_rx` and the handler is handed the copy, not the original.
	 */
	unsigned short tx_syms = (unsigned short)*n_tx;
	unsigned short rx_syms = (unsigned short)*n_rx;
	int i;

	/*
	 * Three flag bits and one more in the next byte, cleared on entry to
	 * every block.  What they indicate is NOT established -- nothing
	 * reconstructed reads either byte -- so they stay masks with the
	 * instruction beside them rather than becoming names that would be
	 * believed.  `andb $0xf8,0x1d` and `andb $0xfd,0x1e`.
	 */
	fp->flags &= (unsigned char)~0x07;
	fp->r1e[0] &= (unsigned char)~0x02;

	/* Stage the transmit words, narrowing int to short. */
	for (i = 0; i < *n_tx; i++)
		tx_in_internal[i] = (unsigned short)tx_bits[i];

	/*
	 * Stage the input samples, right-shifted by the receive input shift.
	 * The load is `movswl` and the shift `sar`, so the arithmetic is
	 * signed; the shift count is `movzwl`, so `hdx->rx_shift` is read as an
	 * unsigned sixteen-bit field.
	 */
	for (i = 0; i < *n_rx; i++)
		rx_in_internal[i] =
			(short)((int)rx_in[i] >> fp->hdx->rx_shift);

	V22_PROTOCOL[fp->hdx->protocol](fp, tx_in_internal, tx_out, rx_in_internal,
				   rx_out_internal, &tx_syms, &rx_syms);

	*n_rx = rx_syms;

	/*
	 * THE STATE TRANSITION, which no handler carries: five status values
	 * put the machine back into state 0 -- `v22_data` -- with its
	 * sub-state cleared.  3 is V22_MSG_CONNECT_2400 and 4 is the 1200
	 * connect (finding F8538, from `v22_process`'s own jump table), so
	 * this reads as "any connect code enters the data state"; 6, 7 and 8
	 * are named by nothing and stay values.  Two separate range tests in
	 * the object, in this order, with the status byte re-read between
	 * them.
	 */
	if (fp->status == 3 || fp->status == 4) {
		fp->hdx->protocol = 0;
		fp->hdx->connect_substate = 0;
	}
	if (fp->status >= 6 && fp->status <= 8) {
		fp->hdx->protocol = 0;
		fp->hdx->connect_substate = 0;
	}

	/* Widen the recovered symbols back out to int. */
	for (i = 0; i < *n_rx; i++)
		rx_bits[i] = rx_out_internal[i];

	/*
	 * The transmit output gain, Q15, over exactly V22_TX_BLOCK samples --
	 * a literal 160 in the object and not `*n_tx` or a parameter, which is
	 * the same block length `TxNOP` emits.  `params.tx_gain` is 13014 in the
	 * template, which is 0.397.
	 */
	for (i = 0; i < V22_TX_BLOCK; i++)
		tx_out[i] = (short)(((int)tx_out[i] * fp->params.tx_gain) >> 15);

	/*
	 * Anything but status 0 reports NO received symbols, whatever the
	 * handler said -- and this happens AFTER the copy-out above, so the
	 * caller's array is still written and only the count is suppressed.
	 */
	if (fp->status != 0)
		*n_rx = 0;

	{
		int word;

		memcpy(&word, &fp->status, sizeof word);
		return word;
	}
}
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
		 * instead of `params.carrier_loss_ms`.  The comparison is UNSIGNED in the
		 * object (`ja`), which is what the cast reproduces.
		 */
		*rxcount = 0;
		hdx = fp->hdx;
		hdx->carrier_loss_blocks++;
		if ((unsigned int)(hdx->carrier_loss_blocks * V22_BLOCK_MS)
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
				hdx->ones_detect_ms = 0;
				hdx->connect_substate = 0;
				HDX_0038(hdx) = 0;
				ResetRx(fp);
				SetAdaptEqV22(fp, 1);
				SetTxRate(fp, V22_RATE_1200);
				SetRxRate(fp, V22_RATE_1200);
				fp->hdx->protocol = V22_PROTOCOL_RETRAIN;
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
						"V.22: Retrain request " "detected.");
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
					hdx->protocol = V22_PROTOCOL_RETRAIN;
					hdx->connect_substate = 1;
					SetAdaptEqV22(fp, 1);
					status = V22_ST_RETRAIN;
					HDX_0038(fp->hdx) = 1;
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
							"Signal quality < " "Retrain level. " "Retrain initiated.");
				}
			}
		}

		if (CarrierDetect(fp)) {
			hdx = fp->hdx;
			if (hdx->carrier_loss_blocks > V22_DATA_CARRIER_BACK
			    && fp->dsp->r2a == 1) {
				hdx->protocol = V22_PROTOCOL_RETRAIN;
				hdx->connect_substate = 1;
				SetAdaptEqV22(fp, 1);
				status = V22_ST_RETRAIN;
				hdx = fp->hdx;
				HDX_0038(hdx) = 1;
				hdx->carrier_loss_blocks = 0;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"Carrier back during " "carrier_loss_time (v22_data)"
						". Retrain initiated.");
			} else {
				hdx->carrier_loss_blocks = 0;
			}
		} else {
			hdx = fp->hdx;
			hdx->carrier_loss_blocks++;
			/*
			 * SIGNED and sixteen bits: the object narrows the
			 * product with `movswl` and compares the two halves
			 * as words.  The format string names both -- the
			 * product is `carrier_loss_time` and `params.carrier_loss_ms` is
			 * the limit it is measured against.
			 */
			if (fp->params.carrier_loss_ms
			    >= (short)(hdx->carrier_loss_blocks * V22_BLOCK_MS)) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V22_MSG_NO_CARRIER won't be " "reported (carrier_loss_time "
						"%d of %d ms)\n",
						(int)(short)(hdx->carrier_loss_blocks
							     * V22_BLOCK_MS),
						(int)fp->params.carrier_loss_ms);
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
/* ------------------------------------------------------------------------ */

void
connect_2400(struct v22fp *fp, unsigned short *txsym, short *txout,
	     short *rxin, unsigned short *rxsym,
	     unsigned short *txcount, unsigned short *rxcount)
{
	unsigned short nsym;
	unsigned int now;
	short elapsed;

	fp->status = V22_STATUS_01;

	switch (fp->hdx->connect_substate) {
	case V22_NODE_2400A:
		/* Training at 1200: send ones, receive, clamp, wait. */
		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_1200);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;
		DescrambleDataV22(fp, rxsym, nsym);

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		now = (unsigned int)ReadGTimer(fp);
		if (now > V22_NODE_2400A_MS) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "connect_2400, NODE_2400B\n");
			fp->hdx->connect_substate = V22_NODE_2400B;
			SetRxRate(fp, V22_RATE_2400);
		}
		break;

	case V22_NODE_2400B:
		/* The receiver is at 2400 now; the transmitter follows. */
		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_1200);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;
		DescrambleDataV22(fp, rxsym, nsym);

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		now = (unsigned int)ReadGTimer(fp);
		if (now > V22_NODE_2400B_MS) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "connect_2400, NODE_2400C\n");
			fp->hdx->connect_substate = V22_NODE_2400C;
			SetTxRate(fp, V22_RATE_2400);
			SetAdaptEqV22(fp, 3);
		}
		break;

	case V22_NODE_2400C:
		/*
		 * Both directions at 2400.  `r10` latches the first
		 * "receiver trained" verdict and is not re-tested once set.
		 */
		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_2400);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;
		DescrambleDataV22(fp, rxsym, nsym);

		if (fp->hdx->trained == 0)
			fp->hdx->trained = RxTrained2400((const short *)rxsym,
						     rxcount);

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		now = (unsigned int)ReadGTimer(fp);
		if (now > V22_NODE_2400C_MS) {
			if (fp->hdx->trained == 1) {
				fp->flags |= V22FP_FLAGS_CONNECT;
				fp->status = V22_MSG_CONNECT_2400;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V22_MSG_CONNECT_2400 In " "NODE_2400C\n");
			} else {
				fp->hdx->gtimer = 0;
				fp->hdx->connect_substate = V22_NODE_2400D;
			}
		}
		break;

	case V22_NODE_2400D:
		/*
		 * The last node tests the verdict fresh on every block rather
		 * than latching it, and its deadline is `hdx->node_deadline` -- the
		 * caller's own limit, 60000 ms as `v22_create` configures it.
		 */
		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_2400);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;
		DescrambleDataV22(fp, rxsym, nsym);

		if (RxTrained2400((const short *)rxsym, rxcount)) {
			fp->flags |= V22FP_FLAGS_CONNECT;
			fp->status = V22_MSG_CONNECT_2400;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V22_MSG_CONNECT_2400 In NODE_2400D\n");
		}

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		now = (unsigned int)ReadGTimer(fp);
		if (now > (unsigned int)fp->hdx->node_deadline) {
			fp->flags |= V22FP_FLAGS_TIMEOUT;
			fp->status = V22_MSG_ERROR7;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V22_MSG_ERROR7\n");
		}
		break;

	default:
		break;
	}

	/*
	 * The carrier-loss tail.  Every arm above falls into it, including
	 * the ones that have just declared the call connected.
	 */
	if (CarrierDetect(fp)) {
		/*
		 * Carrier back, and `r3c` non-zero means it went away inside
		 * the grace window rather than never having been lost.  That
		 * is a retrain, not a connect.
		 */
		if (fp->hdx->carrier_loss_blocks != 0) {
			fp->hdx->protocol = V22_HDX_R0E_RETRAIN;
			fp->hdx->connect_substate = V22_NODE_RETRAIN;
			SetAdaptEqV22(fp, 1);
			HDX_R38(fp->hdx) = V22_HDX_R38_RETRAIN;
			fp->status = V22_STATUS_0B;
			fp->hdx->carrier_loss_blocks = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "Carrier back during carrier_loss_time "
				    "(Connect_2400). Retrain initiated.");
		}
		return;
	}

	fp->hdx->carrier_loss_blocks++;
	elapsed = (short)(fp->hdx->carrier_loss_blocks * V22_BLOCK_MS);
	if (fp->params.carrier_loss_ms < elapsed) {
		fp->status = V22_MSG_NO_CARRIER;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_MSG_NO_CARRIER3\n");
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		return;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V22_MSG_NO_CARRIER won't be reported "
				     "(carrier_loss_time %d of %d ms)",
				     (int)elapsed, (int)fp->params.carrier_loss_ms);
}
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

	switch (hdx->connect_substate) {
	case V22_RMLOOP2_START:
		hdx->gtimer = 0;
		hdx->r08 = 0;
		hdx->trained = 0;
		hdx->connect_substate = V22_RMLOOP2_DETECT;
		SetAdaptEqV22(fp, 1);

		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);
		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		if (*rxcount != 0) {
			/* `bps2`, not `bps` -- see the note at F10194. */
			int n = Detect_1s(rxsym, rxcount, fp->params.bps2,
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
			/* `bps2`, not `bps` -- see the note at F10194. */
			detected = (short)Detect_1s(rxsym, rxcount,
						    fp->params.bps2,
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
			hdx->connect_substate = V22_RMLOOP2_ANSWER;
			/*
			 * The equaliser is frozen into sub-state 2 only when
			 * the last block produced no symbols at all -- the
			 * detector's own zero does NOT reach here, because
			 * `detected` is only written when the count was
			 * non-zero.
			 */
			if (detected == 0) {
				hdx->trained = 1;
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
		/*
		 * `fp->params.bps2` here, not `bps` -- v22fp.h's own note
		 * records the two as always equal, but the object's compare
		 * is `cmpw $0x4b0,0x4(...)`, the `bps2` offset (F10194).
		 */
		MakeTxData((short *)txsym, (const short *)txcount,
			   fp->params.bps2 == V22_RMLOOP2_BPS_1200
				   ? V22_TXDATA_SYMBOL_2
				   : V22_TXDATA_SYMBOL_10);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);
		*rxcount = DemodDataV22(fp, rxin, rxsym, *rxcount);
		if (*rxcount != 0
		    && (short)Detect_1s(rxsym, rxcount, fp->params.bps2,
					V22_DET_THRESH_Q15) == 0) {
			hdx = fp->hdx;
			hdx->trained = 0;
			hdx->r08 = 0;
			hdx->connect_substate = V22_RMLOOP2_LOOP;
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
/* ------------------------------------------------------------------------ */

void
connect_1200(struct v22fp *fp, unsigned short *txsym, short *txout,
	     short *rxin, unsigned short *rxsym,
	     unsigned short *txcount, unsigned short *rxcount)
{
	unsigned short nsym;
	unsigned int now;
	short elapsed;

	fp->status = V22_STATUS_01;

	switch (fp->hdx->connect_substate) {
	case V22_NODE_1200_12:
		/*
		 * The latching node: `r10` takes the first verdict and the
		 * deadline then reads it once.  Same shape as NODE_2400C, and
		 * the only difference is the rate and the constants.
		 */
		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_1200);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;
		DescrambleDataV22(fp, rxsym, nsym);

		if (fp->hdx->trained == 0)
			fp->hdx->trained = RxTrained1200((const short *)rxsym,
						     rxcount);

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		now = (unsigned int)ReadGTimer(fp);
		if (now > V22_NODE_1200_12_MS) {
			if (fp->hdx->trained == 1) {
				SetAdaptEqV22(fp, 3);
				fp->flags |= V22FP_FLAGS_CONNECT;
				fp->status = V22_STATUS_04;
			} else {
				fp->hdx->gtimer = 0;
				fp->hdx->connect_substate = V22_NODE_1200_13;
			}
		}
		break;

	case V22_NODE_1200_13:
		/*
		 * The last node.  NOTE THE DOUBLE CLAMP: the connect arm
		 * calls `RxClampV22` and then falls into the unconditional
		 * call below, so a block that completes training clamps
		 * twice.  The second one is what the object executes and it
		 * writes the same twelve values over the first, so nothing
		 * observable turns on it -- but it is two calls in the object
		 * and it is two here.  NODE_2400D, which is otherwise the
		 * same node, has only the unconditional one.
		 */
		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_1200);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;
		DescrambleDataV22(fp, rxsym, nsym);

		if (RxTrained1200((const short *)rxsym, rxcount)) {
			RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
			fp->flags |= V22FP_FLAGS_CONNECT;
			fp->status = V22_STATUS_04;
		}

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		now = (unsigned int)ReadGTimer(fp);
		if (now > (unsigned int)fp->hdx->node_deadline) {
			fp->flags |= V22FP_FLAGS_TIMEOUT;
			fp->status = V22_STATUS_18;
		}
		break;

	default:
		break;
	}

	/*
	 * The same tail, minus the retrain: connect_1200 returns the instant
	 * the carrier is back and does not look at `r3c` at all.
	 */
	if (CarrierDetect(fp))
		return;

	fp->hdx->carrier_loss_blocks++;
	elapsed = (short)(fp->hdx->carrier_loss_blocks * V22_BLOCK_MS);
	if (fp->params.carrier_loss_ms < elapsed) {
		fp->status = V22_MSG_NO_CARRIER;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_MSG_NO_CARRIER4\n");
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		return;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V22_MSG_NO_CARRIER won't be reported "
				     "(carrier_loss_time %d of %d ms)",
				     (int)elapsed, (int)fp->params.carrier_loss_ms);
}
void
v22_local_loop(struct v22fp *fp, unsigned short *txsym, short *txout,
	       short *rxin, unsigned short *rxsym, unsigned short *txcount,
	       unsigned short *rxcount)
{
	struct v22_status st;
	struct v22fp_hdx *hdx = fp->hdx;

	fp->r1e[0] = (unsigned char)(fp->r1e[0] | V22_LOOP_R1E_SET);
	fp->status = V22_STATUS_01;

	switch (hdx->connect_substate) {
	case V22_LOOP_NODE_0:
		hdx->gtimer = 0;
		hdx->r08 = 0;
		hdx->ones_detect_ms = 0;

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
			fp->hdx->connect_substate = V22_LOOP_NODE_1;
		} else {
			fp->hdx->connect_substate = V22_LOOP_NODE_3;
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
		 * see the derivation in v22loop.h and F10194, which retyped
		 * fpm_tone.h so this is one statement again.
		 */
		*txcount = FPM_TONE_generate(hdx->tone, txout, V22_TX_BLOCK);

		*rxcount = 0;
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		if ((unsigned int)ReadGTimer(fp) > V22_LOOP_TONE_MS) {
			hdx = fp->hdx;
			hdx->gtimer = 0;
			hdx->connect_substate = V22_LOOP_NODE_2;
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
			hdx->connect_substate = V22_LOOP_NODE_3;
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
			if (detected == FPM_MTD_ABSENT) {
				hdx = fp->hdx;
				hdx->r08 = (short)((unsigned short)hdx->r08
						   + V22_LOOP_BLOCK_MS);
			} else {
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
			}

			/*
			 * Narrowed to sixteen bits before the test -- see the
			 * note in v22loop.h on why the second call below is
			 * NOT narrowed.
			 */
			if ((short)Detect_1s(rxsym, rxcount, V22_LOOP_DET_BPS,
					     V22_LOOP_ONES_Q15) == 0) {
				DescrambleDataV22(fp, rxsym, *rxcount);
				fp->hdx->ones_detect_ms = (short)((unsigned short)fp->hdx->ones_detect_ms
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
			hdx->ones_detect_ms = 0;
			hdx->connect_substate = V22_NODE_2400A;
			SetAdaptEqV22(fp, 2);
		} else if ((unsigned short)hdx->ones_detect_ms > V22_LOOP_ONES_MS) {
			hdx->gtimer = 0;
			hdx->r08 = 0;
			hdx->ones_detect_ms = 0;
			hdx->connect_substate = V22_NODE_1200_12;
			SetAdaptEqV22(fp, 2);
		} else if ((unsigned int)ReadGTimer(fp)
			   > (unsigned int)fp->hdx->node_deadline) {
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
		fp->hdx->ones_detect_ms = 0;

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
		 * The answer tone.  `FPM_TONE_generate` returns its `count`
		 * in the object; see v22org.h and F10194, which retyped
		 * fpm_tone.h so this is one statement again.
		 */
		*txcount = FPM_TONE_generate(fp->hdx->tone, txout, V22_ORG_BLOCK);

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
		if ((unsigned int)fp->hdx->gtimer > V22_ANS_NODE_3_TONE_MS)
			*txcount = FPM_TONE_generate(fp->hdx->tone, txout,
						      V22_ORG_BLOCK);

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
			gap = 0;
			if (detected == 0) {
				gap = V22_BLOCK_MS;
				fp->hdx->r08 = (short)(fp->hdx->r08
						       + V22_BLOCK_MS);
			} else if ((unsigned short)fp->hdx->r08
				    <= V22_ORG_MIN_RUN_MS) {
				fp->hdx->r08 = 0;
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
				fp->hdx->ones_detect_ms = (short)(fp->hdx->ones_detect_ms + ones);
				/*
				 * The reset `v22_originate`'s two copies of
				 * this block do NOT have.  See v22org.h.
				 */
				if (ones == 0 && (unsigned short)fp->hdx->ones_detect_ms
						 <= V22_ORG_MIN_RUN_MS)
					fp->hdx->ones_detect_ms = 0;
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
			fp->hdx->ones_detect_ms = 0;
			fp->hdx->connect_substate = V22_ANS_NODE_4;
			SetAdaptEqV22(fp, V22_ORG_EQ_MODE_CARRIER);
			fp->r1e[0] |= V22FP_R1E_BIT3;
			FPM_AGC_Freeze(&fp->dsp->agc);
		} else if ((unsigned short)fp->hdx->ones_detect_ms > V22_ORG_ONES_MS) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("Detected V22 Carrier\n");
			fp->hdx->gtimer = 0;
			fp->hdx->r08 = 0;
			fp->hdx->ones_detect_ms = 0;
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

/* v22mod.c -- V.22 marshalling and answer-state module. */

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
 * ---------------------------------------------------------------------------
 * V22FP_modem -- .text 0x0887b0, 346 bytes.
 *
 * One block of the modulation, and the only caller of the seven-state
 * machine.  It is a marshalling layer with one piece of policy in it:
 *
 *   1. stage the caller's transmit words, int to short, into
 *      `tx_in_internal`;
 *   2. stage the caller's input samples, right-shifted by `hdx->rx_shift`, into
 *      `rx_in_internal`;
 *   3. dispatch through `V22_PROTOCOL[hdx->protocol]`;
 *   4. take the symbol count back, and -- THE POLICY -- move the machine to
 *      state 0 for five values of `fp->status`;
 *   5. copy `rx_out_internal` out to the caller's int array;
 *   6. scale exactly V22_TX_BLOCK transmit samples by `params.tx_gain` in Q15;
 *   7. report no symbols at all unless `fp->status` is zero.
 *
 * THE COUNTS CHANGE WIDTH ACROSS THE CALL.  The caller's are `int`; the
 * handlers' are `unsigned short` (finding F8534), and the two 16-bit slots
 * live side by side on this function's own stack.  Only the RECEIVE count is
 * written back -- the transmit count the handler leaves is dropped on the
 * floor, which is the object's and not an omission here.
 *
 * THE RETURN IS THE WHOLE 32-BIT WORD AT fp+0x1c, not `fp->status`: the
 * object emits `mov 0x1c(%ebp),%eax` where a byte field would force a
 * `movzbl`, and the same four bytes are read as a BYTE three times in the
 * lines above it.  `B103FP_modem` does exactly this, and this file follows
 * its spelling.  A draft argued that no test could separate the two readings
 * because `v22_process` only looks at the low byte; the test separated them
 * on the first call, 0 against 17664 (finding F8538).  Measure, do not argue.
 */

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

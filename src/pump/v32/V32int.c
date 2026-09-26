/*
 * V32int.c -- ITU-T V.32 / V.32bis: the datapump's interface functions and
 *             data-path leaves.
 *
 * Reconstructed from dsplibs.o.  The blob's STT_FILE order places this file
 * between V32dec.c and V32mod.c, and its .text run is
 * [SetTxModeV32 0x081680, V32FP_modem 0x0827a0) -- twenty-five functions,
 * plus the three rate tables and the three length tables their code reads.
 *
 *   SetTxModeV32             .text 0x081680  520
 *   SetRxModeV32             .text 0x081890  577
 *   SeedScramblerV32         .text 0x081ae0   15
 *   GetRateV32               .text 0x081af0   78
 *   ScrambleDataV32          .text 0x081b40   28
 *   DescrambleDataV32        .text 0x081b60   30
 *   ModDataV32               .text 0x081b80  121
 *   DemodDataV32             .text 0x081c00  604
 *   SetAdaptEqV32            .text 0x081e60   79
 *   SetAdaptEcV32            .text 0x081eb0  319
 *   SetRxLoopsV32            .text 0x081ff0   83
 *   SetECRndTripDelayV32     .text 0x082050  233
 *   TxClockSyncV32           .text 0x082140    1
 *   EpochDetectV32           .text 0x082150   17
 *   RetrainDetectV32         .text 0x082170   53
 *   RenegotiateDetectV32     .text 0x0821b0   44
 *   RateToSeq                .text 0x0821e0   14
 *   SeqToRate                .text 0x0821f0  150
 *   CodeESeq                 .text 0x082290  183
 *   CodeRateSeq              .text 0x082350  164
 *   CodeFinalRateSeq         .text 0x082400  164
 *   DecodeRateSeq            .text 0x0824b0  152
 *   TxNoCarrierV32           .text 0x082550  152
 *   RxClampV32               .text 0x0825f0   49
 *   V32FP_modem              .text 0x082630  356
 *
 *   V32_RATE_SEQ             .data 0x007690   14
 *   V32_FINAL_RATE_SEQ       .data 0x00769e   14
 *   V32_ESEQ                 .data 0x0076ac   14
 *   V32_TURNAROUND_DLY       .data 0x0076c0    4
 *   V32_SYMBOL_LEN           .data 0x0076c4    4
 *   V32_SAMPLE_LEN           .data 0x0076c8    4
 *
 * Every body is the text it was reconstructed in, moved verbatim.  The
 * functions were spread over the invented units v32fpctl.c (the interface
 * functions), v32data.c (ModDataV32, TxNoCarrierV32), v32demod.c
 * (DemodDataV32) and v32seq.c (the rate codec); the FILE order and the .text
 * run it pins put them here.  The three rate tables are read only by this
 * file's six rate functions (v32seq.h measures all nine referencing
 * instructions), and the three length tables follow them in the blob's own
 * .data run.  See finding F11395.
 */

#include <string.h>

#include "dsplib/v32fpctl.h"

#include "dsplib/v32data.h"
#include "dsplib/v32dec.h"
#include "dsplib/v32demod.h"
#include "dsplib/v32hdx.h"
#include "dsplib/v32scram.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32smc.h"
#include "dsplib/v32struct.h"
#include "dsplib/vtb.h"
#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_ecc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sysdep.h"
#include "v32fpdisp-common.h"

#define SDM_TX(fp)	(&((struct v32_fp *)(fp))->scrambler)
#define SDM_RX(fp)	(&((struct v32_fp *)(fp))->descrambler)
#define SMC(fp)		(&((struct v32_fp *)(fp))->tx_smc)
#define PPS(fp)		(&((struct v32_fp *)(fp))->pps)
#define RING(fp)	(&((struct v32_fp *)(fp))->symout)
#define ECC(fp)		(&((struct v32_fp *)(fp))->ecc)
#define FSE(fp)		(&((struct v32_fp *)(fp))->fse)
#define DEC(fp)		((struct v32_dec *)FSE(fp)->cfg.owner)

#define AGC_OF(fp)	(&(fp)->agc)
#define ECC_OF(fp)	(&(fp)->ecc)
#define FSE_OF(fp)	(&(fp)->fse)
#define MRF_OF(fp)	(&(fp)->mrf)
#define SRE_OF(fp)	(&(fp)->sre)
#define RXBUF_OF(fp)	((fp)->rx_buf)
#define CLEAN_OF(fp)	((fp)->clean_buf)

short V32_RATE_SEQ[V32_RATE_COUNT] = {
	0x0d11, 0x0b11, 0x0b91, 0x09d1, 0x09b1, 0x0ff9, 0x0997
};

short V32_FINAL_RATE_SEQ[V32_RATE_COUNT] = {
	0x0d11, 0x0b11, 0x0b91, 0x09d1, 0x09b1, 0x0999, 0x0997
};

/*
 * The E sequence.
 *
 * `V32_ESEQ[i] == V32_FINAL_RATE_SEQ[i] | 0xf000` at every one of the seven
 * indices -- INCLUDING index 5, where the FINAL table and `V32_RATE_SEQ`
 * differ, so it is the FINAL one this tracks and not the other.  That is a
 * property of the bytes, stated here so a reader who edits one table knows
 * which other one moved with it; the object holds three independent arrays and
 * so does this file.
 */
short V32_ESEQ[V32_RATE_COUNT] = {
	0xfd11, 0xfb11, 0xfb91, 0xf9d1, 0xf9b1, 0xf999, 0xf997
};

/*
 * THE THREE LENGTH TABLES ARE ONE FAMILY AND THE OBJECT LAYS THEM OUT AS ONE.
 *
 *   V32_TURNAROUND_DLY  .data 0x0076c0   4   { 64, 360 }
 *   V32_SYMBOL_LEN      .data 0x0076c4   4   { 12,  48 }
 *   V32_SAMPLE_LEN      .data 0x0076c8   4   { 40, 160 }
 *
 * Twelve contiguous bytes, all three GLOBAL, all three two entries, and all
 * three indexed by the SAME selector at every site -- `V32FP_recreate` uses
 * obj + 0x16 for all three within twenty instructions (7f13d, 7f150, 7f165)
 * and `V32FP_control` uses obj + 0x18 for two of them (84611, 84624).  They
 * are declared here in the object's address order for that reason.
 *
 * The ratio between the two columns is 4:1 in samples (40 and 160), 4:1 in
 * symbols (12 and 48) and 45:8 in turnaround delay (64 and 360), so the third
 * is not simply the other two scaled and the selector is not a sample-rate
 * switch alone.  Nothing written establishes what the two columns ARE; what
 * is established is that one selector picks a column in all three.
 *
 * `.data` in the object -- so not const, even though nothing writes any of
 * them.
 */
short V32_TURNAROUND_DLY[2] = { 64, 360 };
short V32_SYMBOL_LEN[2] = { 12, 48 };
short V32_SAMPLE_LEN[2] = { 40, 160 };

static unsigned short tx_in_internal[V32_BIT_BUFFER];
static unsigned short rx_out_internal[V32_BIT_BUFFER];

/*
 * The best rate index this station and `seq` have in common.
 *
 * `static` because the object has no symbol for it: it is inlined into all
 * five of its callers and the worklist therefore never listed it.
 */
static int
v32_common_rate(struct v32_modem *modem, unsigned short seq)
{
	int rate = V32_RATE_NONE;
	struct v32_fp *fp = (struct v32_fp *)
		modem->fp;
	short local = V32_RATE_SEQ[fp->rx_rate_index];

	if ((seq & 0x0008) && (local & 0x0008))
		rate = 5;
	else if ((seq & 0x0020) && (local & 0x0020))
		rate = 4;
	else if ((seq & 0x0200) && (local & 0x0200))
		rate = ((seq & 0x0080) && (local & 0x0080)) ? 2 : 1;
	else if ((seq & 0x0040) && (local & 0x0040))
		rate = 3;
	else if ((seq & 0x0400) && (local & 0x0400))
		rate = 0;

	return rate;
}

/*
 * Configure the transmitter.
 *
 * The dispatch sign-extends `mode` and then tests it UNSIGNED against 6, so a
 * negative mode reaches the default arm along with 7 and above.
 *
 * The default arm posts a reason code and still runs the scrambler tail, with
 * a shift of zero -- an out-of-range request leaves `outmask` at 0 and
 * `regmask` at all-ones, which is a scrambler that emits nothing.
 */
void
SetTxModeV32(struct v32_modem *modem, short mode)
{
	struct v32_fp *fp;
	struct v32_sdm *sdm;
	unsigned int mask;
	short sel;
	int shift;

	switch (mode) {
	case V32_MODE_ABS4:
	case V32_MODE_DIF4:
		/*
		 * One arm for both: the object tests `mode` a second time
		 * inside it and the only thing that differs is the encoder,
		 * absolute against differential.  `smc->pad02` takes the same
		 * value as the selector, which is the first evidence anywhere
		 * that anything writes that field at all -- v32smc.h has it as
		 * "not read by any encoder", which stays true.  Finding F8216.
		 */
		fp = FP(modem);
		sel = (short)(mode == V32_MODE_ABS4 ? 1 : 0);
		SMC(fp)->pad02 = sel;
		fp->encoder_sel = sel;
		SDM_TX(fp)->group = 2;
		SMC(fp)->mode = 0;
		SMC(fp)->shift = 0;
		PPS(fp)->cfg.imap = SMCv32_IMAP16;
		PPS(fp)->cfg.qmap = SMCv32_QMAP16;
		fp->short_2c = 0;
		shift = 2;
		break;

	case V32_MODE_16:
		fp = FP(modem);
		SDM_TX(fp)->group = 4;
		SMC(fp)->mode = 1;
		SMC(fp)->pad02 = 0;
		fp->encoder_sel = 0;
		SMC(fp)->shift = 2;
		PPS(fp)->cfg.imap = SMCv32_IMAP16;
		PPS(fp)->cfg.qmap = SMCv32_QMAP16;
		fp->short_2c = 1;
		shift = 4;
		break;

	case V32_MODE_32T:
		fp = FP(modem);
		SDM_TX(fp)->group = 4;
		SMC(fp)->uncoded_bits = 2;
		SMC(fp)->mode = 2;
		SMC(fp)->pad02 = 2;
		fp->encoder_sel = 2;
		SMC(fp)->shift = 2;
		PPS(fp)->cfg.imap = VTBv32_IMAP32;
		PPS(fp)->cfg.qmap = VTBv32_QMAP32;
		fp->short_2c = 1;
		shift = 4;
		break;

	case V32_MODE_16T:
		fp = FP(modem);
		SDM_TX(fp)->group = 3;
		SMC(fp)->uncoded_bits = 1;
		SMC(fp)->mode = 3;
		fp->encoder_sel = 2;
		PPS(fp)->cfg.imap = VTBv32_IMAP16T;
		PPS(fp)->cfg.qmap = VTBv32_QMAP16T;
		fp->short_2c = 3;
		shift = 3;
		break;

	case V32_MODE_64T:
		fp = FP(modem);
		SDM_TX(fp)->group = 5;
		SMC(fp)->uncoded_bits = 3;
		SMC(fp)->mode = 4;
		fp->encoder_sel = 2;
		PPS(fp)->cfg.imap = VTBv32_IMAP64;
		PPS(fp)->cfg.qmap = VTBv32_QMAP64;
		fp->short_2c = 4;
		shift = 5;
		break;

	case V32_MODE_128T:
		fp = FP(modem);
		SDM_TX(fp)->group = 6;
		SMC(fp)->uncoded_bits = 4;
		SMC(fp)->mode = 5;
		SMC(fp)->pad02 = 2;
		fp->encoder_sel = 2;
		PPS(fp)->cfg.imap = VTBv32_IMAP128;
		PPS(fp)->cfg.qmap = VTBv32_QMAP128;
		fp->short_2c = 5;
		shift = 3;		/* six bits, sent as two groups of 3 */
		break;

	default:
		modem->status = V32_STATUS_BAD_MODE;
		modem->flags |= V32_FLAG_FAULT;
		fp = FP(modem);
		shift = 0;
		break;
	}

	sdm = SDM_TX(fp);
	mask = (unsigned int)((1 << shift) - 1);
	sdm->outmask = mask;
	sdm->regmask = ~mask;
	sdm->tap1 = (short)(sdm->tap1_pos - shift);
	sdm->tap2 = (short)(sdm->tap2_pos - shift);
}

/*
 * Configure the receiver, mode for mode with the transmitter.
 *
 * Three things differ from the transmit side and all three are the object's:
 * the reason code and the fault flag are written the other way round (no
 * comparison can separate the two orders -- both leave the same two bytes);
 * the four trellis arms call `VTBv32_init` on the decoder's Viterbi state,
 * which is why the shift survives the call in a callee-saved register; and
 * the two four-point arms clear the decoder's differential context.
 */
void
SetRxModeV32(struct v32_modem *modem, short mode)
{
	struct v32_fp *fp;
	struct v32_dec *dec;
	struct v32_sdm *sdm;
	unsigned int mask;
	int shift;

	switch (mode) {
	case V32_MODE_ABS4:
	case V32_MODE_DIF4:
		fp = FP(modem);
		SDM_RX(fp)->group = 2;
		fp->rx_smc.shift = 0;
		dec = DEC(fp);
		fp->rx_smc.mode = 0;
		dec->short_04 = 0;
		dec->chan = 0;
		fp->short_2e = 0;
		dec->count = 0;
		if (mode == V32_MODE_ABS4) {
			dec->rate_change = 0;
			FSE(fp)->cfg.decision = FSE_decision_AB;
		} else {
			FSE(fp)->cfg.decision = FSE_decision_4pt;
		}
		shift = 2;
		break;

	case V32_MODE_16:
		fp = FP(modem);
		SDM_RX(fp)->group = 4;
		dec = DEC(fp);
		fp->rx_smc.mode = 1;
		fp->rx_smc.shift = 2;
		dec->chan = 1;
		dec->short_04 = 2;
		FSE(fp)->cfg.decision = FSE_decision_16pt;
		fp->short_2e = 1;
		shift = 4;
		break;

	case V32_MODE_32T:
		fp = FP(modem);
		fp->rx_smc.mode = 1;
		dec = DEC(fp);
		SDM_RX(fp)->group = 4;
		fp->rx_smc.shift = 2;
		dec->chan = 1;
		dec->short_04 = 2;
		FSE(fp)->cfg.decision = FSE_decision_32pt;
		fp->short_2e = 1;
		VTBv32_init(&dec->vtb, 2, 0);
		shift = 4;
		break;

	case V32_MODE_16T:
		fp = FP(modem);
		SDM_RX(fp)->group = 3;
		FSE(fp)->cfg.decision = FSE_decision_16Tpt;
		fp->short_2e = 3;
		VTBv32_init(&DEC(fp)->vtb, 3, 0);
		shift = 3;
		break;

	case V32_MODE_64T:
		fp = FP(modem);
		SDM_RX(fp)->group = 5;
		FSE(fp)->cfg.decision = FSE_decision_64pt;
		fp->short_2e = 4;
		VTBv32_init(&DEC(fp)->vtb, 4, 0);
		shift = 5;
		break;

	case V32_MODE_128T:
		fp = FP(modem);
		SDM_RX(fp)->group = 6;
		FSE(fp)->cfg.decision = FSE_decision_128pt;
		fp->short_2e = 5;
		VTBv32_init(&DEC(fp)->vtb, 5, 0);
		shift = 3;		/* see SetTxModeV32 */
		break;

	default:
		modem->flags |= V32_FLAG_FAULT;
		modem->status = V32_STATUS_BAD_MODE;
		shift = 0;
		break;
	}

	sdm = SDM_RX(FP(modem));
	mask = (unsigned int)((1 << shift) - 1);
	sdm->outmask = mask;
	sdm->regmask = ~mask;
	sdm->tap1 = (short)(sdm->tap1_pos - shift);
	sdm->tap2 = (short)(sdm->tap2_pos - shift);
}

/* Load the transmit scrambler's shift register. */
void
SeedScramblerV32(struct v32_modem *modem, unsigned int seed)
{
	SDM_TX(FP(modem))->reg = seed;
}

/*
 * The line rate, as a V32_RATE_*.
 *
 * 9600 splits on V32_OBJ_TRELLIS -- the two 9600 modes are the 32-point
 * trellis one and the 16-point uncoded one -- and an unrecognised bit rate
 * reports V32_RATE_INVALID rather than any of the five.
 */
int
GetRateV32(struct v32_modem *modem)
{
	unsigned short bps = modem->params.rx_rate;

	if (bps == 14400)
		return V32_RATE_14400;
	if (bps == 12000)
		return V32_RATE_12000;
	if (bps == 9600)
		return modem->params.trellis ? V32_RATE_9600
							 : V32_RATE_9600_NT;
	if (bps == 7200)
		return V32_RATE_7200;
	if (bps == 4800)
		return V32_RATE_4800;
	return V32_RATE_INVALID;
}

/* Scramble `count` words of `buf` in place.  A tail call in the object. */
void
ScrambleDataV32(struct v32_modem *modem, short *buf, unsigned short count)
{
	SDMv32_scrambler(SDM_TX(FP(modem)), buf, count);
}

/* And the receive direction, through the descrambler at fp + 0x50b0. */
void
DescrambleDataV32(struct v32_modem *modem, short *buf, unsigned short count)
{
	SDMv32_descrambler(SDM_RX(FP(modem)), buf, count);
}

unsigned short
ModDataV32(struct v32_modem *modem, short *data, short *out, unsigned short count)
{
	const v32_encoder_fn *tbl;
	struct v32_fp *fp;
	short sel;

	fp = modem->fp;
	tbl = fp->encoders;
	sel = fp->encoder_sel;
	tbl[sel](&fp->tx_smc, &fp->symout, data, count);

	fp = modem->fp;
	return FPM_PPS_filter(&fp->pps,
			      /* D431: the same bytes as the v32_symout above */
			      (struct fpm_smc_ring *)(void *)
					&fp->symout,
			      out, count);
}

unsigned short
DemodDataV32(struct v32_modem *modem, short *in, unsigned short *out, unsigned short count)
{
	int ec_training = 0;
	unsigned short n, m;
	int enables;
	struct v32_modem *owner = modem;
	struct v32_hdx *hdx;
	struct v32_fp *fp;
	int i;

	fp = owner->fp;
	n = (unsigned short)FPM_MRF_filter(MRF_OF(fp), in, RXBUF_OF(fp),
					   (short)count);

	fp = owner->fp;
	if (ECC_OF(fp)->adapt_near != 0 || ECC_OF(fp)->adapt_far != 0)
		ec_training = 1;
	FPM_ECC_cancel(ECC_OF(fp), RXBUF_OF(fp), n);

	/* Keep a copy of the cancelled block for V32FP_GetCleanedSamples. */
	fp = owner->fp;
	for (i = 0; i < (int)n; i++)
		CLEAN_OF(fp)[i] = RXBUF_OF(fp)[i];
	fp->clean_n = (unsigned short)n;
	fp->rx_len = (unsigned short)n;

	if (ec_training != 0)
		return 0;

	hdx = owner->hdx;
	if (hdx->mode == V32_MODE_6) {
		if (owner->params.disconnect_thresh
		    > FPM_rms(RXBUF_OF(fp), n)) {
			owner->flags &=
				(unsigned char)~V32_FLAG_CARRIER;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("v32 low sig energy\n");
			return 0;
		}
		fp = owner->fp;
	}

	/* The object passes a fourth argument here; see the header. */
	FPM_AGC_agc(AGC_OF(fp), RXBUF_OF(fp), n);

	fp = owner->fp;
	owner->flags =
		(unsigned char)((owner->flags
				 & (unsigned char)~V32_FLAG_SILENCE)
				| (AGC_OF(fp)->signal == 0
				   ? V32_FLAG_SILENCE : 0));

	enables = AGC_OF(fp)->f18;
	SRE_OF(fp)->adapt = fp->int_04 & enables;
	m = FPM_SRE_recover(SRE_OF(fp), RXBUF_OF(fp), in, (short)n);

	fp = owner->fp;
	if (SRE_OF(fp)->active == 0) {
		if (DSPLIB_DEBUG_VERBOSE())
			dsplibs_debug_printf("sre no carrier\n");
		owner->flags &=
			(unsigned char)~V32_FLAG_CARRIER;
		return 0;
	}
	owner->flags |= V32_FLAG_CARRIER;

	hdx = owner->hdx;
	FSE_OF(fp)->pll_on = fp->int_08 & enables;
	if (hdx->mode == V32_MODE_6) {
		if (SRE_OF(fp)->mode == 0) {
			FSE_OF(fp)->tilt_on = 0;
			FSE_OF(fp)->lms_on = 0;
		} else {
			FSE_OF(fp)->tilt_on =
				fp->int_0c & enables;
			FSE_OF(fp)->lms_on =
				fp->eq_adapt & enables;
		}
	} else {
		FSE_OF(fp)->tilt_on = 1;
		/* NOT masked with `enables`, unlike its three siblings: D491. */
		FSE_OF(fp)->lms_on = fp->eq_adapt;
	}

	return FPM_FSE_receive(FSE_OF(fp), in, out, m);
}

/*
 * Equaliser adaptation.
 *
 * V32_ADAPTEQ_MU0 and _MU1 both switch adaptation on and differ only in which
 * of `fpm_fse_cfg::mu[]` the LMS update takes its step size from.  Any other
 * value does nothing at all -- not even the switch-off.
 * The MU1, OFF, MU0 case order recovers the period emission (F10227).
 */
void
SetAdaptEqV32(struct v32_modem *modem, unsigned short mode)
{
	struct v32_fp *fp;

	switch (mode) {
	case V32_ADAPTEQ_MU1:
		fp = FP(modem);
		fp->eq_adapt = 1;
		FSE(fp)->mu_sel = 1;
		break;
	case V32_ADAPTEQ_OFF:
		fp = FP(modem);
		fp->eq_adapt = 0;
		break;
	case V32_ADAPTEQ_MU0:
		fp = FP(modem);
		fp->eq_adapt = 1;
		FSE(fp)->mu_sel = 0;
		break;
	default:
		break;
	}
}

/*
 * Echo canceller.
 *
 * V32_ADAPTEC_RESET re-initialises it in place -- note that the config it
 * hands `FPM_ECC_init` is the canceller's OWN, which works because
 * `struct fpm_ecc` opens with its `struct fpm_ecc_cfg` and init copies it
 * before touching anything.  The two delay inputs are set first because init
 * reads them and never writes them, and the three geometry fields afterwards
 * because init has just overwritten them.
 *
 * V32_ADAPTEC_ON clears the delay line before enabling the two update loops.
 * V32_ADAPTEC_SLOW divides the update gain by TEN and changes nothing else.
 * The object writes that as the usual reciprocal multiply -- `imul
 * $0x66666667`, `sar $2` on the high half, less the sign -- and the shift is
 * what says ten rather than five: 0x66666667 / 2^32 is 0.4, and the extra two
 * places take it to 0.1.  Both readings compile; only one agrees.
 */
void
SetAdaptEcV32(struct v32_modem *modem, unsigned short mode)
{
	struct v32_fp *fp;
	struct fpm_ecc *ecc;
	short i;

	switch (mode) {
	case V32_ADAPTEC_OFF:
		fp = FP(modem);
		ECC(fp)->adapt_near = 0;
		ECC(fp)->adapt_far = 0;
		fp->int_14 = 0;
		fp->int_18 = 0;
		break;

	case V32_ADAPTEC_ON:
		fp = FP(modem);
		ecc = ECC(fp);
		for (i = 0; i < ecc->line_len; i++)
			ecc->line[i] = 0;
		fp->int_14 = 1;
		ecc->unk1a = 0;
		ecc->adapt_near = 1;
		ecc->adapt_far = 1;
		fp->int_18 = 1;
		break;

	case V32_ADAPTEC_SLOW:
		ecc = ECC(FP(modem));
		ecc->mu = (short)(ecc->mu / 10);
		break;

	case V32_ADAPTEC_RESET:
		ecc = ECC(FP(modem));
		ecc->near_delay = (short)modem->params.ec_near_delay;
		ecc->far_delay = 0x30;
		FPM_ECC_init(ecc, (const struct fpm_ecc_cfg *)(void *)ecc, 0);
		ecc = ECC(FP(modem));
		ecc->line_len = 0x30;
		ecc->near_rd = 0;
		ecc->far_rd = 0;
		break;

	default:
		break;
	}
}

/*
 * The four switches at V32FP_R00..V32FP_R0C, as one group.
 *
 * The store order is +0x08, +0x0c, +0x00, +0x04 in both arms and is written
 * that way here; no comparison can separate it from any other order, since all
 * four end up holding the same value.
 */
void
SetRxLoopsV32(struct v32_modem *modem, unsigned short mode)
{
	struct v32_fp *fp;

	switch (mode) {
	case 1:
		fp = FP(modem);
		fp->int_08 = 0;
		fp->int_0c = 0;
		fp->int_00 = 0;
		fp->int_04 = 0;
		break;
	case 2:
	case 3:
		fp = FP(modem);
		fp->int_08 = 1;
		fp->int_0c = 1;
		fp->int_00 = 1;
		fp->int_04 = 1;
		break;
	default:
		break;
	}
}

/*
 * Place the echo canceller for a round-trip delay of `delay` symbols.
 *
 * The requested delay is raised to the far section's length, clamped at zero,
 * and then held down to the far lag; `fpm_ecc.h` has both of those as config
 * fields the canceller is built around, so the effect is "as much delay as the
 * geometry can express".  From that:
 *
 *     far_rd    = one symbol period
 *     line_len  = the context's own delay + the round trip + two symbols
 *     near_rd   = round trip + one symbol, wrapped into the line
 *     ring      = the same again with half a symbol added, and the ring's
 *                 length set to the line's
 *
 * and the line is preset to the no-carrier constellation index.  THAT LAST
 * IDENTIFICATION IS USAGE INFERENCE: the object writes a bare 0x10, and
 * v32data.h's V32_SYMBOL_NOCARRIER is the same literal reached the same way --
 * the seventeenth entry of a sixteen-point map -- but nothing states they are
 * one constant.
 *
 * Every wrap is `if (line_len <= x) x -= line_len`, which is a single
 * conditional subtraction and not a modulus; it is correct only while `x` is
 * below twice the line length, and every `x` here is.
 */
void
SetECRndTripDelayV32(struct v32_modem *modem, short delay)
{
	struct v32_fp *fp;
	struct fpm_ecc *ecc;
	struct v32_symout *ring;
	short lag, symlen, d, t, i;

	lag = HDX(modem)->short_9c;
	symlen = V32_SYMBOL_LEN[modem->params.symlen_sel];
	fp = FP(modem);
	ecc = ECC(fp);
	ring = RING(fp);

	d = ecc->cfg.far_taps;
	if (d < delay)
		d = delay;
	if (d < 0)
		d = 0;
	if (ecc->cfg.far_lag < d)
		d = ecc->cfg.far_lag;

	ecc->far_rd = symlen;
	ecc->line_len = (short)(lag + d + 2 * symlen);

	t = (short)(d + symlen);
	if (ecc->line_len <= t)
		t = (short)(t - ecc->line_len);
	ecc->near_rd = t;

	t = (short)(lag + (short)(symlen / 2 + ecc->near_rd));
	if (ecc->line_len <= t)
		t = (short)(t - ecc->line_len);
	ring->pad0e = t;
	ring->limit = ecc->line_len;
	ring->widx = ring->pad0e;

	for (i = 0; i < ecc->line_len; i++)
		ecc->line[i] = V32_SYMBOL_NOCARRIER;
}

/*
 * A `ret` and nothing else.
 *
 * Its one caller, `v32_data`, passes the instance and ignores the result, so
 * nothing about the signature is recoverable beyond "at most one argument is
 * read, and none is".  Either the transmit clock needs no correction in V.32 --
 * V.22's `TxClockSync` at 0x8e610 is 22 bytes and does real work -- or the
 * function was emptied.  Nothing here decides which.
 */
void
TxClockSyncV32(struct v32_modem *modem)
{
	(void)modem;
}

/* The decoder's rate-change report, straight through and not cleared. */
int
EpochDetectV32(struct v32_modem *modem)
{
	return DEC(FP(modem))->rate_change;
}

/*
 * Poll and clear the decoder's retrain request, and count it.
 *
 * v32dec.h has +0x62 as `retrain`, "1 or 2; a request, not a state -- nothing
 * here reads it back".  This is what reads it back, and the two pollers name
 * the two bits between them.  The phase counter is zeroed BEFORE the request
 * bit is cleared; both orders leave the same bytes, so no test can separate
 * them and the object's order is the one written.
 */
int
RetrainDetectV32(struct v32_modem *modem)
{
	struct v32_dec *dec = DEC(FP(modem));
	unsigned short req = (unsigned short)dec->retrain;

	if (!(req & V32_DEC_RETRAIN_REQ))
		return 0;

	dec->count = 0;
	dec->retrain = (short)(req & ~(unsigned short)V32_DEC_RETRAIN_REQ);
	dec->short_74 =
		(unsigned short)(dec->short_74 + 1);
	return 1;
}

/* The renegotiation request, bit 1 of the same word.  It is not counted. */
int
RenegotiateDetectV32(struct v32_modem *modem)
{
	struct v32_dec *dec = DEC(FP(modem));
	unsigned short req = (unsigned short)dec->retrain;

	if (!(req & V32_DEC_RENEG_REQ))
		return 0;

	dec->count = 0;
	dec->retrain = (short)(req & ~(unsigned short)V32_DEC_RENEG_REQ);
	return 1;
}

unsigned short
RateToSeq(struct v32_modem *modem, short rate)
{
	(void)modem;			/* never read; D404 */

	return (unsigned short)V32_RATE_SEQ[rate];
}

int
SeqToRate(struct v32_modem *modem, unsigned short seq)
{
	return v32_common_rate(modem, seq);
}

/*
 * The E sequence for the negotiated rate.
 *
 * The object loads `V32_ESEQ[rate]` UNCONDITIONALLY, before the test, and
 * discards it on the no-rate path.  That is safe only because index 6 is
 * inside the table -- the ladder's "none" value is the table's last entry, not
 * one past it -- and it is why this is written as a conditional expression
 * rather than as the `if` the two rate functions above use.
 */
unsigned short
CodeESeq(struct v32_modem *modem, unsigned short seq)
{
	short rate = (short)v32_common_rate(modem, seq);

	return rate == V32_RATE_NONE ? V32_ESEQ_NONE
				     : (unsigned short)V32_ESEQ[rate];
}

unsigned short
CodeRateSeq(struct v32_modem *modem, unsigned short seq)
{
	short rate = (short)v32_common_rate(modem, seq);
	unsigned short out = V32_RATE_SEQ_NONE;

	if (rate != V32_RATE_NONE)
		out = (unsigned short)V32_RATE_SEQ[rate];

	return out;
}

unsigned short
CodeFinalRateSeq(struct v32_modem *modem, unsigned short seq)
{
	short rate = (short)v32_common_rate(modem, seq);
	unsigned short out = V32_RATE_SEQ_NONE;

	if (rate != V32_RATE_NONE)
		out = (unsigned short)V32_FINAL_RATE_SEQ[rate];

	return out;
}

short
DecodeRateSeq(struct v32_modem *modem, unsigned short seq)
{
	return (short)v32_common_rate(modem, seq);
}

unsigned short
TxNoCarrierV32(struct v32_modem *modem, const short *data, short *out,
	       unsigned short count)
{
	struct v32_symout *ring;
	struct v32_smc *smc;
	unsigned short i;
	short widx, limit, quad;
	struct v32_fp *fp;

	(void)data;			/* never read; see v32data.h */

	fp = modem->fp;
	ring = &fp->symout;
	smc = &fp->tx_smc;
	quad = smc->quad;
	widx = ring->widx;
	limit = ring->limit;

	for (i = 0; i < count; i++) {
		short next;

		ring->buf[widx] = V32_SYMBOL_NOCARRIER;
		next = (short)(widx + 1);
		quad = (short)((quad + 3) & 3);
		widx = (short)(next < limit ? next : 0);
	}

	/* BOTH write-backs precede the shaper here, unlike V.17 and V.29. */
	smc->quad = quad;
	ring->widx = widx;

	return FPM_PPS_filter(&fp->pps,
			      /* D431 again */
			      (struct fpm_smc_ring *)(void *)
					&fp->symout,
			      out, count);
}

/*
 * Fill one block's worth of `out` with a clamped word and report the count.
 *
 * The counter is a SHORT and the loop tests it against -1, so a count of zero
 * writes nothing and a NEGATIVE count runs to 65535-ish writes rather than
 * none.  That is the object's loop and it is reproduced; D482 records it.
 *
 * `in` and `count` are never read.  Their types are `RxHdxNull`'s call and no
 * stronger -- the whole Rx state family takes the same four.
 */
unsigned short
RxClampV32(struct v32_modem *modem, short *in, short *out, unsigned short count)
{
	struct v32_hdx *hdx = HDX(modem);
	short i;

	(void)in;
	(void)count;

	for (i = (short)(hdx->symbol_len - 1); i != -1; i--)
		*out++ = 0xff;

	return hdx->symbol_len;
}

/*
 * V32FP_modem -- .text 0x082630.
 *
 * One block.  The two counts are in/out and they CROSS OVER: `nout` arrives as
 * a count of transmit BITS and leaves as a count of output SAMPLES, `nin`
 * arrives as input samples and leaves as receive bits.  Everything around the
 * dispatch is marshalling -- the caller's `int` arrays are narrowed into the
 * two `short` statics and widened back.
 *
 * THE RETURN IS A 32-BIT LOAD OF obj + 0x30 (82779), where every other access
 * to that byte in this tree is `movb` or `movzbl`.  Reproduced as a 32-bit
 * read: the status is the low byte and `v32.c` masks it, so `V32_OBJ_FLAGS`
 * and the two bytes above it reach the caller and are discarded.
 */
int
V32FP_modem(struct v32_modem *modem, const int *txbits, short *out, const short *in,
	    int *rxbits, int *nout, int *nin)
{
	struct v32_fp *fp;
	struct v32_hdx *hdx;
	short nsamples;
	unsigned short rxcount;
	int scale;
	int n;
	int i;

	nsamples = (short)*nout;
	rxcount = (unsigned short)*nin;

	fp = FP(modem);
	if (*nin > 0) {
		short *clean = (short *)fp->clean_buf;

		for (i = 0; i < *nin; i++)
			clean[i] = in[i];
	}
	fp->clean_n = (unsigned short)*nin;

	for (i = 0; i < *nout; i++)
		tx_in_internal[i] = (unsigned short)txbits[i];

	modem->flags &= (unsigned char)~V32_FLAG_FAULT;
	hdx = HDX(modem);
	V32_PROTOCOL[hdx->mode](modem, tx_in_internal, out,
						  (short *)in, rx_out_internal,
						  &nsamples, &rxcount);

	*nout = (int)(unsigned short)nsamples;
	*nin = (int)rxcount;

	for (i = 0; i < *nin; i++)
		rxbits[i] = (int)rx_out_internal[i];

	if ((modem->flags & V32_FLAG_DATA) != 0) {
		hdx = HDX(modem);
		hdx->mode = V32_PROTO_DATA;
		hdx->state = V32_STATE_DONT_CARE;
	}

	n = *nout;
	if (n > 0) {
		scale = PARAMS(modem)->tx_scale;
		for (i = 0; i < n; i++)
			out[i] = (short)((out[i] * scale) >> 15);
	}

	return modem->status_word;
}

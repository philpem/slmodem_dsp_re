/*
 * v22rate.c -- V.22 / V.22bis: rate selection, receiver reset, and the
 * receive chain.  See include/dsplib/v22rate.h.
 *
 * THE INSTANCE POINTER IS READ AGAIN AFTER EVERY CALL, exactly as
 * src/pump/v22/v22data.c records for the same block: `DemodDataV22` reloads
 * `0x54(%edi)` after each of its six calls while the instance itself stays in
 * a callee-saved register, which a spilled local would not do.  Written that
 * way here -- `fp->dsp->` at every use, and no cached sub-object pointer
 * except where a value has to survive a call.
 */

#include "dsplib/v22rate.h"

#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22_iir.h"
#include "dsplib/v22_mrf.h"
#include "dsplib/v22_pps.h"
#include "dsplib/v22_sre.h"
#include "dsplib/v22dec.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22txtab.h"
#include "dsplib/fpm.h"

void
SetTxRate(struct v22fp *fp, short rate)
{
	struct v22fp_dsp *dsp;

	switch (rate) {
	case V22_RATE_1200:
		dsp = fp->dsp;
		/* FPM_SDM_init's width arithmetic, open-coded. */
		dsp->sdm.cfg.nbits = V22_SDM_BITS_1200;
		dsp->sdm.mask = (1 << V22_SDM_BITS_1200) - 1;
		dsp->sdm.notmask = ~((1 << V22_SDM_BITS_1200) - 1);
		dsp->sdm.reg = 0;
		dsp->sdm.shift1 = (short)(dsp->sdm.cfg.tap1
					  - V22_SDM_BITS_1200);
		dsp->sdm.shift2 = (short)(dsp->sdm.cfg.tap2
					  - V22_SDM_BITS_1200);
		/* No amplitude bits at 1200, so no quadrant shift either. */
		dsp->smc.cfg.qshift = 0;
		dsp->smc.cfg.amask = 0;
		dsp->pps.imap = SMCv22_IMAP_1200BPS;
		dsp->pps.qmap = SMCv22_QMAP_1200BPS;
		dsp->r28 = 0;
		break;
	case V22_RATE_2400:
		dsp = fp->dsp;
		dsp->sdm.cfg.nbits = V22_SDM_BITS_2400;
		dsp->sdm.mask = (1 << V22_SDM_BITS_2400) - 1;
		dsp->sdm.notmask = ~((1 << V22_SDM_BITS_2400) - 1);
		dsp->sdm.reg = 0;
		dsp->sdm.shift1 = (short)(dsp->sdm.cfg.tap1
					  - V22_SDM_BITS_2400);
		dsp->sdm.shift2 = (short)(dsp->sdm.cfg.tap2
					  - V22_SDM_BITS_2400);
		/* Two amplitude bits below two quadrant bits. */
		dsp->smc.cfg.qshift = 2;
		dsp->smc.cfg.amask = 3;
		dsp->pps.imap = SMCv22_IMAP_2400BPS;
		dsp->pps.qmap = SMCv22_QMAP_2400BPS;
		dsp->r28 = 1;
		break;
	default:
		break;
	}
}

void
SetRxRate(struct v22fp *fp, short rate)
{
	struct v22fp_dsp *dsp;

	switch (rate) {
	case V22_RATE_1200:
		dsp = fp->dsp;
		dsp->sdm2.cfg.nbits = V22_SDM_BITS_1200;
		dsp->sdm2.mask = (1 << V22_SDM_BITS_1200) - 1;
		dsp->sdm2.notmask = ~((1 << V22_SDM_BITS_1200) - 1);
		dsp->sdm2.reg = 0;
		dsp->sdm2.shift1 = (short)(dsp->sdm2.cfg.tap1
					   - V22_SDM_BITS_1200);
		dsp->sdm2.shift2 = (short)(dsp->sdm2.cfg.tap2
					   - V22_SDM_BITS_1200);
		dsp->r2a = 0;
		dsp->fse.decision = FSEv22_decision12;
		break;
	case V22_RATE_2400:
		dsp = fp->dsp;
		dsp->sdm2.cfg.nbits = V22_SDM_BITS_2400;
		dsp->sdm2.mask = (1 << V22_SDM_BITS_2400) - 1;
		dsp->sdm2.notmask = ~((1 << V22_SDM_BITS_2400) - 1);
		dsp->sdm2.reg = 0;
		dsp->sdm2.shift1 = (short)(dsp->sdm2.cfg.tap1
					   - V22_SDM_BITS_2400);
		dsp->sdm2.shift2 = (short)(dsp->sdm2.cfg.tap2
					   - V22_SDM_BITS_2400);
		dsp->r2a = 1;
		dsp->fse.decision = FSEv22_decision24;
		break;
	default:
		break;
	}
}

unsigned short
DemodDataV22(struct v22fp *fp, short *in, unsigned short *sym,
	     unsigned short count)
{
	struct v22fp_hdx *hdx = fp->hdx;
	short *iir = hdx->iir;
	unsigned short n_rate;	/* samples the resampler produced      */
	unsigned short n_sym;	/* symbol-clock samples it then found  */
	int agc_state;		/* dsp->agc.f18, needed after a call    */
	int adapt;		/* the equaliser's enable for this block */

	/*
	 * The IIR front end, mode 0 only.  The tone generator fills a block
	 * of V22_IIR_BLOCK shorts -- the filter's own block length -- but is
	 * asked for only `count` of them, so a short block leaves the tail of
	 * the mixer at whatever the stack held.  The object's.
	 */
	if (fp->dsp->r2e == V22_FRONTEND_IIR) {
		short mix[V22_IIR_BLOCK];

		FPM_TONE_generate_demod(hdx->tone, mix, (short)count);
		V22_iir_filt_demod(in,
				   iir + V22_IIR_OFF_B, iir + V22_IIR_OFF_A,
				   iir + V22_IIR_OFF_X, iir + V22_IIR_OFF_Y,
				   mix);
	}

	n_rate = (unsigned short)V22_MRF_filter(&fp->dsp->mrf, in,
						fp->dsp->rx_scratch,
						(short)count);
	fp->dsp->rx_count = (short)n_rate;

	/*
	 * The disconnect check, skipped entirely when hdx->protocol is set.  Note
	 * the comparison is against `params.disconnect_thresh`, which
	 * `V22FP_create` loads from V22DiconnectThreshTable regardless of what
	 * the template held -- see v22fp.h.
	 */
	if (fp->hdx->protocol == 0
	    && fp->params.disconnect_thresh
	       > FPM_rms(fp->dsp->rx_scratch, n_rate)) {
		fp->dsp->sre.active = 0;
		return 0;
	}

	/*
	 * The object passes a FOURTH argument here, the constant 1, which
	 * FPM_AGC_agc does not have -- the same extra argument v22data.c's
	 * Detect_v22 and bwchdem.c record at their own call sites, and
	 * ignored in the same way.
	 */
	FPM_AGC_agc(&fp->dsp->agc, fp->dsp->rx_scratch, n_rate);

	/*
	 * `agc.f18` is read ONCE and used twice across the clock-recovery
	 * call, so it cannot be a re-read: the object keeps it in a
	 * callee-saved register.  fpm_agc.h has it as init's reset flag and
	 * nothing in fpm_agc writes it, so what it means here is the caller's
	 * business and it keeps its neutral name.
	 */
	agc_state = fp->dsp->agc.f18;
	fp->dsp->sre.adapt = fp->dsp->r04 & agc_state;

	n_sym = (unsigned short)V22_SRE_recover(&fp->dsp->sre,
						fp->dsp->rx_scratch, in,
						(short)n_rate);

	/*
	 * While the clock loop is still acquiring, the equaliser adapts on
	 * nothing; once it has settled, bit 0 of the AGC word decides.
	 */
	adapt = 0;
	if (fp->dsp->sre.acquiring == 0)
		adapt = agc_state & 1;

	fp->dsp->fse.pll_on = fp->dsp->r08;
	if (fp->dsp->sre.mode != 0) {
		fp->dsp->fse.r18 = fp->dsp->r0c & adapt;
		fp->dsp->fse.lms_on = adapt & fp->dsp->eq_adapt;
	} else {
		fp->dsp->fse.r18 = 0;
		fp->dsp->fse.lms_on = 0;
	}

	V22_FSE_receive(&fp->dsp->fse, in, sym, (short)n_sym);

	/* Not V22_FSE_receive's return, which the object throws away. */
	return (unsigned short)fp->dsp->fse.n_out;
}

void
ResetRx(struct v22fp *fp)
{
	V22_SRE_init(&fp->dsp->sre, 0);
	/* The equaliser as its own configuration; see v22rate.h. */
	V22_FSE_init(&fp->dsp->fse,
		     (const struct v22_fse_cfg *)&fp->dsp->fse, 0);
}

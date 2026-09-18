/*
 * V17r_prc.c -- ITU-T V.17 receiver processing, the run the object keeps in
 *              its own input file between V.29's transmitter and V.21's.
 *
 * Reconstructed from dsplibs.o:
 *
 *   DemodDataV17          .text 0x0a50a0  415
 *   DescrambleDataV17     .text 0x0a5240   30
 *   CarrierDetectV17      .text 0x0a5260  121
 *   DataCarrierDetectV17  .text 0x0a52e0  625
 *   QualityDetectV17      .text 0x0a5560  266
 *   EpochDetectV17        .text 0x0a5670   22
 *   GetSNRV17             .text 0x0a5690   23
 *   StoreCoefV17          .text 0x0a56b0   81
 *   Restore_rateV17       .text 0x0a5710   37
 *
 * `readelf -sW` carries a `V17r_prc.c` STT_FILE record and no `v17.c`; the
 * nine functions above are one contiguous address run, and every call to
 * them is a relocation from the V.17 interface run (0x09ff80-0x0a094c), so
 * the object had them in a translation unit of their own.  Issue #156.
 */

#include <string.h>

#include "dsplib/v17fax.h"

#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/sysdep.h"
#include "dsplib/v17cfg.h"
#include "dsplib/v17dec.h"
#include "dsplib/v32smc.h"	/* TrellisEncodeDifTable, TrellisTransitionTable:
				 * SMCv17_encoder_tcm reuses V.32's trellis
				 * coder tables, see v17data.h              */
#include "dsplib/vtb.h"

#define RXROOT(modem)		((struct v17rx *)(modem))
#define TXROOT(modem)		((struct v17tx *)(modem))
#define RXCTL(modem)		(RXROOT(modem)->ctl)
#define RXSTATE(modem)		(RXROOT(modem)->state)
#define TXPRIV(modem)		(TXROOT(modem)->priv)
#define TXBLOCK(modem)		(TXROOT(modem)->fp)
#define CTL(modem)		RXCTL(modem)
#define RXS(modem)		RXSTATE(modem)
#define TXP(modem)		TXPRIV(modem)
#define TXFP(modem)		TXBLOCK(modem)

/*
 * DemodDataV17 -- .text 0x0a50a0, 415 bytes.  See v17fax.h for the shape, for
 * why the pre-pass copy does NOT halve where `DemodDataV29`'s does, and for
 * where `signal` comes from.
 *
 * THE STORE ORDER OF THE THREE EQUALISER ENABLES IS THE OBJECT'S.  `tilt_on`
 * is written first (0x0a51dd), then `pll_on` (0x0a51f0), then `lms_on`
 * (0x0a51fa) -- the same three fields in the same order as `DemodDataV29`.
 */
unsigned short
DemodDataV17(void *modem, short *in, unsigned short *bits, unsigned short count)
{
	int signal;
	unsigned short n;
	struct v17rx_state *rxs;

	FPM_AGC_agc(&RXS(modem)->agc.value, in, count);
	/* Not the object's `%eax`; the same value.  D1091. */
	signal = RXS(modem)->agc.value.signal;

	if (RXCTL(modem)->state == V17RX_STATE_START) {
		short *buf = (short *)RXCTL(modem)->scratch;
		unsigned short i;

		/* No `>> 1` here.  F9103. */
		for (i = 0; i < count; i++)
			buf[i] = in[i];

		FPM_TONE_kill((struct fpm_tone *)
				RXCTL(modem)->tone,
			      (short *)RXCTL(modem)->scratch,
			      (short)count);

		if (FPM_MTD_detect((struct fpm_mtd *)
					RXCTL(modem)->mtd,
				   (const short *)
					RXCTL(modem)->scratch,
				   (short)count) != 0)
			return 0;
	}

	n = (unsigned short)FPM_MRF_filter(
			&RXS(modem)->mrf,
			in,
			(short *)RXSTATE(modem)->buf_mrf,
			(short)count);

	rxs = RXS(modem);
	rxs->sre.adapt = signal & rxs->r04;

	n = FPM_SRE_recover(&RXS(modem)->sre,
			    (const short *)
				RXSTATE(modem)->buf_mrf,
			    (short *)RXSTATE(modem)->buf_sre,
			    (short)n);

	if (n > V17RXS_SRE_MAX && DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("ERROR: SRE buffer violation!(%d)", n);

	rxs = RXS(modem);
	rxs->fse.tilt_on = 0;
	rxs->fse.pll_on = signal & rxs->r08;
	rxs->fse.lms_on = signal & rxs->r10;

	return FPM_FSE_receive(&RXS(modem)->fse,
			       (const short *)
				RXSTATE(modem)->buf_sre,
			       bits, n);
}

/* --------------------------------------------------------------------- */

void
DescrambleDataV17(void *modem, unsigned short *data, unsigned short count)
{
	SDM_descrambler((struct fpm_sdm *)(void *)
				&RXSTATE(modem)->sdm,
			data, count);
}

/* --------------------------------------------------------------------- */

int
CarrierDetectV17(void *modem)
{
	struct v17rx_state *rx;
	struct v17rx_priv *ctl;
	int r;

	rx = RXSTATE(modem);
	ctl = RXCTL(modem);

	/* +0xd0 is read 32-bit HERE and 16-bit in QualityDetectV17. */
	r = rx->agc.value.signal & rx->sre.active;

	if (ctl->short_train != 0
	    && rx->fse.lms_force != 0
	    && (short)rx->dec.sym_count > V17RXS_0094_MIN) {
		if (rx->fse.mse > V17RXS_DEC_ERROR_MAX)
			r = 0;
		else
			r &= 1;
		/*
		 * The object tests the same field twice, with the two arms
		 * merged in between; it is two `if`s in the source and not
		 * one, or the compare would have been shared.
		 */
		if (rx->fse.mse > V17RXS_DEC_ERROR_MAX) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"V17 Decoder error too big..." " no carrier\n");
		}
	}

	return r;
}

/* --------------------------------------------------------------------- */

short
DataCarrierDetectV17(void *modem, const short *in, unsigned short count)
{
	struct v17rx_state *rx;
	struct v17rx_priv *ctl;
	short r;

	rx = RXSTATE(modem);
	ctl = RXCTL(modem);

	/* +0xd0 is read 16-bit HERE and 32-bit in CarrierDetectV17. */
	r = (short)(rx->agc.narrow.signal & rx->sre.active);

	if (ctl->r20 == 0) {
		/*
		 * The same three gates and the same two arms as
		 * `CarrierDetectV17`, including its doubled test of the
		 * decoder error and its format string.
		 */
		if (ctl->short_train != 0
		    && rx->fse.lms_force != 0
		    && (short)rx->dec.sym_count > V17RXS_0094_MIN) {
			if (rx->fse.mse > V17RXS_DEC_ERROR_MAX)
				r = 0;
			else
				r &= 1;
			if (rx->fse.mse > V17RXS_DEC_ERROR_MAX) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V17 Decoder error too big..." " no carrier\n");
			}
		}
	} else {
		if (rx->fse.mse > V17RXS_DEC_ERROR_MAX
		    || (r & 1) == 0)
			ctl->offband_latch = 1;

		r = 1;
		if (ctl->offband_latch != 0) {
			short *buf;
			short i;

			buf = (short *)ctl->buf2;
			for (i = 0; i < (int)count; i++)
				buf[i] = (short)(unsigned short)in[i];

			/*
			 * The object passes FOUR arguments here, the fourth a
			 * constant 1 the callee never reads; see D1031.
			 */
			FPM_AGC_agc((struct fpm_agc *)(void *)
					&ctl->agc,
				    (short *)ctl->buf2,
				    count);

			if (FPM_MTD_detect((struct fpm_mtd *)
						ctl->mtd2,
					   (const short *)
						ctl->buf2,
					   (short)count) != 0)
				ctl->offband = 0;
			else
				ctl->offband = (short)
					((unsigned short)ctl->offband + count);

			if (ctl->offband > V17RXC_OFFBAND_MAX) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V17: V21 Carrier detected\n");
				r = 0;
			}
		}
	}

	if (rx->energy_watch != 0) {
		short rms;
		unsigned short phase;

		rms = FPM_rms(in, count);

		/*
		 * No rounding term on this one, unlike QualityDetectV17's
		 * smoothing: the object is `imul $0x32fe ; sar $0xf` and
		 * nothing else.
		 */
		if ((int)rms < ((int)rx->rms_ref
				* V17RXS_RMS_DROP_Q15) >> 15) {
			r = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"sudden energy drop > 8[dB]," " no carrier");
		}

		phase = (unsigned short)((unsigned short)rx->rms_phase + 1);
		if ((short)phase == V17RXS_RMS_PERIOD) {
			rx->rms_ref = rms;
			rx->rms_phase = 0;
		} else {
			rx->rms_phase = (short)phase;
		}
	}

	return r;
}

/* --------------------------------------------------------------------- */

short
QualityDetectV17(void *modem)
{
	struct v17rx_state *rx;
	short r;
	short err;
	short n;

	rx = RXSTATE(modem);

	/* +0xd0 is read 16-bit HERE and 32-bit in CarrierDetectV17. */
	r = (short)(rx->agc.narrow.signal & rx->sre.active);

	/*
	 * Read BEFORE the diagnostic, because the object reads it before the
	 * call and no compiler may hoist a load across one.
	 */
	err = rx->fse.mse;

	if (r == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"V17 Dec error too big..." " unreliable data\n");
		r = V17_QUALITY_UNRELIABLE;
	}

	n = (short)(unsigned short)rx->qcount;
	if (n == 0) {
		rx->qavg = err;
		rx->qcount = 1;
		return r;
	}

	if (n > V17RXS_QCOUNT_SETTLE) {
		if (n != V17RXS_QCOUNT_JUDGE)
			return r;
		if (rx->qavg
		    <= (short)(unsigned short)rx->quality_threshold)
			rx->r4fb2 = 1;
	} else {
		rx->qavg = (short)
			(((err * V17RXS_QWEIGHT_NEW + V17RXS_QROUND) >> 15)
			 + ((rx->qavg * V17RXS_QWEIGHT_OLD
			     + V17RXS_QROUND) >> 15));
	}

	rx->qcount = (short)(n + 1);
	return r;
}

/* --------------------------------------------------------------------- */

int
EpochDetectV17(void *modem)
{
	struct v17rx_state *rx;

	rx = RXSTATE(modem);
	return rx->fse.lms_force != 0;
}

/* --------------------------------------------------------------------- */

short
GetSNRV17(void *modem)
{
	struct v17rx_state *rx;

	rx = RXSTATE(modem);
	return (short)(13 - (unsigned short)rx->fse.mse);
}

/* --------------------------------------------------------------------- */

void
StoreCoefV17(void *modem)
{
	struct v17rx_state *rx;
	short *d0;
	short *d1;
	const unsigned short *s0;
	const unsigned short *s1;
	unsigned short i;

	rx = RXSTATE(modem);
	d0 = (short *)RXROOT(modem)->cfg.coefsave0;
	d1 = (short *)RXROOT(modem)->cfg.coefsave1;
	s0 = (const unsigned short *)rx->fse.icoeff;
	s1 = (const unsigned short *)rx->fse.qcoeff;

	for (i = 0; i < V17_COEF_N; i++) {
		d0[i] = (short)s0[i];
		d1[i] = (short)s1[i];
	}

	*(short *)RXROOT(modem)->cfg.ratesave =
		(short)rx->fse.freq;
}

/* --------------------------------------------------------------------- */

void
Restore_rateV17(void *modem)
{
	struct v17rx_state *rx;
	const short *saved;

	rx = RXSTATE(modem);
	saved = (const short *)RXROOT(modem)->cfg.ratesave;

	rx->fse.freq = *saved;
	rx->fse.sym_count =
		(short)(rx->fse.cfg.train_sym + 5);
}

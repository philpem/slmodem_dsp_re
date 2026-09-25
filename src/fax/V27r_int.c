/*
 * V27r_int.c -- split out of the merged v27.c so the definitions sit in the
 * translation unit the object's FILE order gives them.  Bodies moved
 * verbatim; no source text changed.  See finding F11390.
 */
#include <string.h>

#include "dsplib/v27fax.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/faxfifo.h"
#include "dsplib/sdmv27.h"
#include "dsplib/sgd.h"
#include "dsplib/smc.h"
#include "dsplib/sysdep.h"
#include "dsplib/v27cfg.h"

/*
 * One block through the receive chain.
 *
 * THE TONE TEST HAS NO COPY AND NO NOTCH, WHERE V.17 AND V.29 HAVE BOTH.
 * Both of those halve the caller's block into a scratch buffer, run
 * `FPM_TONE_kill` over the copy and hand the copy to the detector; V.27ter
 * hands `FPM_MTD_detect` the CALLER's buffer, which by then is the buffer
 * `FPM_AGC_agc` has rewritten in place.  There is no copy loop in the object's
 * 331 bytes -- no loop at all -- and no `FPM_TONE_kill` relocation.  Checked
 * against the bytes rather than inferred from the size, because it is the one
 * structural difference between the three demodulators.  Finding F9115.
 *
 * A DETECTION ABANDONS THE WHOLE CALL: it returns 0 having run the gain
 * control and nothing else, so the caller's samples are left gain-controlled
 * and the resampler, the recoverer and the equaliser do not advance.
 *
 * THE TWO WIDENINGS OF `count` ARE DIFFERENT AND BOTH ARE FORCED.  The gain
 * control takes an `unsigned short` and gets `movzwl`; the tone detector takes
 * a `short` and gets `movswl` (0x0a599f).  The resampler's is `movzwl` again
 * only because the register already held that value and the callee's parameter
 * is 16 bits wide -- F614's free case, not a third reading.
 *
 * THE THREE EQUALISER FLAGS ARE STORED IN THE OBJECT'S ORDER, which is
 * `tilt_on`, `lms_on`, `pll_on` (0x0a5a24, 0x0a5a2f, 0x0a5a37).  That is NOT
 * V.17's or V.29's order, and the difference is kept because a store order is
 * evidence about one function and does not carry to its siblings.
 */
unsigned short
DemodDataV27(void *modem, short *in, unsigned short *bits, unsigned short count)
{
	int signal;
	unsigned short n;
	unsigned short m;
	struct v27_rx_block *rx;
	struct v27_rx_shared *sh;

	FPM_AGC_agc(&((struct v27_rx *)modem)->rx->agc, in, count);
	/* Not the object's `%eax`; the same value.  D1094. */
	signal = ((struct v27_rx *)modem)->rx->agc.signal;

	sh = ((struct v27_rx *)modem)->shared;
	/*
	 * "the machine is still in START".  The object compares the field
	 * against zero in memory (`cmpw $0x0,0x10(%edx)` at 0x0a5994), so it
	 * does not say which extension the author's declaration carried and
	 * this site is unchanged by the rename.  See v27fax.h and F9300.
	 */
	if (sh->rx_state == V27RX_STATE_START) {
		if (FPM_MTD_detect((struct fpm_mtd *)sh->mtd,
				   in, (short)count) != 0)
			return 0;
	}

	rx = ((struct v27_rx *)modem)->rx;
	n = (unsigned short)FPM_MRF_filter((&rx->mrf), in,
					   (short *)rx->buf_a,
					   (short)count);

	rx = ((struct v27_rx *)modem)->rx;
	(&rx->sre)->adapt = signal & rx->en_sre_adapt;
	m = FPM_SRE_recover((&rx->sre),
			    (const short *)rx->buf_a,
			    (short *)rx->buf_b,
			    (short)n);

	if (m > V27RX_SRE_MAX && DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("ERROR: SRE buffer violation(%d)", m);

	rx = ((struct v27_rx *)modem)->rx;
	(&rx->fse)->tilt_on = 0;
	(&rx->fse)->lms_on = signal & rx->en_fse_lms;
	(&rx->fse)->pll_on = signal & rx->en_fse_pll;

	return FPM_FSE_receive((&rx->fse),
			       (const short *)rx->buf_b,
			       bits, m);
}

/*
 * The receiver's descrambler, and its sibling `ScrambleDataV27` at the foot of
 * this file.  One indirection, one addition and a tail call each; the only
 * work either does is sign-extending `count`.
 */
void
DescrambleDataV27(void *modem, unsigned short *data, short count)
{
	SDMv27_descrambler(&((struct v27_rx *)modem)->rx->sdm,
			   data, count);
}

int
CarrierDetectV27(void *modem)
{
	struct v27_rx_block *rx = ((struct v27_rx *)modem)->rx;

	return (&rx->agc)->signal & (&rx->sre)->active;
}

/*
 * Carrier, and the two things that can take it away.
 *
 * The V.21 arm exists because a fax receiver that has lost the image carrier
 * must notice the sending end going back to the control channel.  It is armed
 * -- and stays armed -- the moment the equaliser's error goes bad or carrier
 * drops, and from then on every block is copied out, gain-controlled and run
 * past the V.21 tone detector.  0x4ff samples without a hit is what the
 * detector needs to be believed.
 */
short
DataCarrierDetectV27(void *modem, short *samples, unsigned short count)
{
	struct v27_rx_block *rx = ((struct v27_rx *)modem)->rx;
	struct v27_rx_shared *sh = ((struct v27_rx *)modem)->shared;
	struct v27_rx_decoder *dec = &rx->dec;
	short cd;

	cd = (short)((&rx->agc)->signal & (&rx->sre)->active);

	if (sh->v21_watch == 0) {
		if (((short)dec->sym_count) > V27RX_DEC_SETTLED) {
			if ((&rx->fse)->mse > V27RX_MSE_NO_CARRIER)
				cd = 0;
			else
				cd &= 1;
		}
		if ((&rx->fse)->mse > V27RX_MSE_NO_CARRIER && DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27 Decoder error too big..." " no carrier\n");
	} else {
		short i;

		if ((&rx->fse)->mse > V27RX_MSE_NO_CARRIER || (cd & 1) == 0)
			sh->v21_armed = 1;

		cd = 1;
		if (sh->v21_armed != 0) {
			short *buf = (short *)sh->buf;

			for (i = 0; i < (int)count; i = (short)(i + 1))
				buf[i] = samples[i];

			FPM_AGC_agc(&sh->agc, buf, count);

			if (FPM_MTD_detect((struct fpm_mtd *)
						sh->mtd_v21,
					   buf, (short)count) != 0)
				sh->v21_samples = 0;
			else
				sh->v21_samples =
					(unsigned short)
					(sh->v21_samples
					 + count);

			if (((short)sh->v21_samples)
			    > V27SH_V21_TIMEOUT) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V27: V21 Carrier detected\n");
				cd = 0;
			}
		}
	}

	if (rx->rms_on != 0) {
		short level = FPM_rms(samples, count);
		unsigned short n;

		if (level < (short)((rx->rms_ref
				     * V27RX_RMS_DROP_Q15) >> 15)) {
			cd = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("sudden energy drop >" " 8[dB], no carrier");
		}

		n = (unsigned short)(rx->rms_count + 1);
		if (n == 2) {
			rx->rms_ref = level;
			rx->rms_count = 0;
		} else {
			rx->rms_count = n;
		}
	}

	return cd;
}

/*
 * Grade the data, and keep the running error average the grade will be read
 * off later.
 *
 * The average runs for exactly 0x32 blocks and then stops; the block after it
 * -- and only that one -- compares the result against the limit and latches
 * the verdict.  The counter keeps incrementing past that, so the latch fires
 * once.
 */
short
QualityDetectV27(void *modem)
{
	struct v27_rx_block *rx = ((struct v27_rx *)modem)->rx;
	short mse = (&rx->fse)->mse;
	short verdict;
	unsigned short n;

	verdict = (short)((&rx->agc)->signal & (&rx->sre)->active);
	if (verdict == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27 Dec error too big..." " unreliable data\n");
		verdict = V27_QUALITY_UNRELIABLE;
	}

	n = rx->q_count;
	if (n == 0) {
		rx->q_acc = mse;
		rx->q_count = 1;
	} else if ((short)n <= 0x31) {
		rx->q_acc = (short)
			(((rx->q_acc * 0x7333 + 0x4000) >> 15)
			 + ((mse * 0xccd + 0x4000) >> 15));
		rx->q_count = (unsigned short)(n + 1);
	} else if ((short)n == 0x32) {
		if (rx->q_acc <= ((short)rx->q_limit))
			rx->q_flag = 1;
		rx->q_count = (unsigned short)(n + 1);
	}

	return verdict;
}

int
EpochDetectV27(void *modem)
{
	struct v27_rx_block *rx = ((struct v27_rx *)modem)->rx;

	return (&rx->fse)->lms_force != 0;
}

short
GetSNRV27(void *modem)
{
	(void)modem;			/* never read; see v27fax.h */
	return 10;
}

/*
 * V29r_int.c -- split out of the merged v29.c / v29data.c so the definitions sit in
 * the translation unit the object's FILE order gives them.  Bodies moved
 * verbatim; no source text changed.  See finding F11390.
 */
#include <stddef.h>
#include <string.h>

#include "dsplib/v29fax.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/smc.h"
#include "dsplib/sysdep.h"
#include "dsplib/v29cfg.h"
#include "dsplib/v29data.h"

/*
 * ---------------------------------------------------------------------------
 * DemodDataV29 -- .text 0x0a5ff0, 398 bytes.
 *
 * One block through the receive chain: gain control, an optional tone pre-pass,
 * resample, symbol recovery, equalise and slice.
 *
 * THE PRE-PASS ABANDONS THE WHOLE CALL.  While the detection block's +0x14 is
 * zero the demodulator halves the input into a scratch buffer, notches a tone
 * out of it and asks the tone detector whether it fired -- and if it did, it
 * returns zero without touching the resampler, the recoverer or the equaliser.
 * The AGC has already run by then and its effect on the caller's buffer stands.
 *
 * THE TWO LOOP COUNTERS IN THIS FILE ARE DIFFERENT TYPES AND THAT IS FORCED.
 * Here the halving loop compares `cmp %di,%dx` with `jb` -- 16 bits, unsigned
 * -- so the index is an `unsigned short`.  In `DataCarrierDetectV29` the copy
 * loop compares `cmp %esi,%edx` with `jl` after a `movswl` -- 32 bits, signed
 * -- so that index is a `short` widened to int.  Two loops over the same
 * `count`, written by the same author, spelled differently; neither reading can
 * be carried to the other.  Finding F8879.
 *
 * THE CARRIER BIT COMES FROM THE FIELD, NOT FROM `%eax`.  See the note at the
 * top of this file, finding F8875 and deviation D1036.
 */
unsigned short
DemodDataV29(void *modem, short *in, unsigned short *out, unsigned short count)
{
	int signal;
	unsigned short n;
	struct v29_rx_block *rx;

	FPM_AGC_agc(&((struct v29_rx *)modem)->rx->agc, in, count);
	/* Not the object's `%eax`; the same value.  D1036. */
	signal = ((struct v29_rx *)modem)->rx->agc.signal;

	if (((struct v29_rx *)modem)->det->state == 0) {
		short *buf = (short *)((struct v29_rx *)modem)->det->buf;
		unsigned short i;

		for (i = 0; i < count; i++)
			buf[i] = (short)(in[i] >> 1);

		FPM_TONE_kill((struct fpm_tone *)
				((struct v29_rx *)modem)->det->tone,
			      (short *)((struct v29_rx *)modem)->det->buf,
			      (short)count);

		if (FPM_MTD_detect((struct fpm_mtd *)
					((struct v29_rx *)modem)->det->mtd,
				   (const short *)
					((struct v29_rx *)modem)->det->buf,
				   (short)count) != 0)
			return 0;
	}

	n = (unsigned short)FPM_MRF_filter(
			&((struct v29_rx *)modem)->rx->mrf,
			in,
			(short *)((struct v29_rx *)modem)->rx->buf_mrf,
			(short)count);

	rx = ((struct v29_rx *)modem)->rx;
	rx->sre.adapt = signal & rx->int_0004;

	n = FPM_SRE_recover(&((struct v29_rx *)modem)->rx->sre,
			    (const short *)((struct v29_rx *)modem)->rx->buf_mrf,
			    (short *)((struct v29_rx *)modem)->rx->buf_sre,
			    (short)n);

	if (n > V29RX_SRE_MAX && DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("ERROR: SRE buffer violation(%d)", n);

	rx = ((struct v29_rx *)modem)->rx;
	rx->fse.tilt_on = 0;
	rx->fse.pll_on = signal & rx->int_0008;
	rx->fse.lms_on = signal & rx->int_0020;

	return FPM_FSE_receive(&((struct v29_rx *)modem)->rx->fse,
			       (const short *)
				((struct v29_rx *)modem)->rx->buf_sre,
			       out, n);
}

/*
 * ---------------------------------------------------------------------------
 * DescrambleDataV29 -- .text 0x0a6180, 30 bytes.
 *
 * Two instructions and a tail jump: pick the `fpm_sdm` out of the receive
 * block and hand the caller's buffer and count straight to `SDM_descrambler`.
 *
 * NO INTERMEDIATE LOCAL, and that is measured rather than a style choice --
 * `ScrambleDataV22` and `DescrambleDataV22` enumerated seven spellings and
 * exactly one reproduces them, the one with no local (finding F8120).  The
 * local costs the register: with it GCC puts the sub-object pointer in %edx
 * and pays the six-byte `add $imm32,%edx`, and the object has the five-byte
 * `add $imm32,%eax` here at 0x0a6190.  The same evidence, in the same form,
 * at a fourth site.
 *
 * IT JUMPS TO `SDM_descrambler`, NOT TO `FPM_SDM_descrambler`.  The two are
 * byte for byte the same code at two addresses (`include/dsplib/sdm.h`), and
 * which one a call site names is settled by the relocation and not by which
 * would work -- 0x0a6199 carries `R_386_PC32 SDM_descrambler`.
 */
void
DescrambleDataV29(void *modem, unsigned short *data, unsigned short count)
{
	SDM_descrambler((struct fpm_sdm *)(void *)
			&((struct v29_rx *)modem)->rx->sdm,
			data, count);
}

/*
 * ---------------------------------------------------------------------------
 * CarrierDetectV29 -- .text 0x0a61a0, 22 bytes.
 *
 * The AGC's own signal bit, gated.  Both loads are 32 bits wide, which is what
 * makes `rx + 0x80` an `int` and therefore `struct fpm_agc`'s `signal` rather
 * than a `short` of its own: a 16-bit field could not be read with a 32-bit
 * `mov` at all.
 */
int
CarrierDetectV29(void *modem)
{
	struct v29_rx_block *rx = ((struct v29_rx *)modem)->rx;

	return rx->agc.signal & rx->sre.active;
}

/*
 * ---------------------------------------------------------------------------
 * DataCarrierDetectV29 -- .text 0x0a61c0, 579 bytes.
 *
 * Three questions, and the answer to any one of them can be the answer.
 *
 *   1. the carrier bit, masked down to its low bit once the receiver has been
 *      running long enough (rx + 0x46 past 999) and zeroed outright if the
 *      decoder's error is over 0x3fff;
 *   2. the V.21 scan, which runs only once the detection block's +0x2a has
 *      latched, and which reports NO data carrier as soon as it has seen
 *      0x4ff samples of V.21 -- that is the branch the author's own
 *      "V29: V21 Carrier detected" message sits in;
 *   3. the energy-drop check, which is gated on rx + 0x4f64 and reports no
 *      carrier when this block's RMS falls below 0.39984 of the reference.
 *
 * THE IDIOM `x & (err <= limit)` OCCURS TWICE AND IS WHAT THE BRANCHES SAY.
 * The object does not compute a boolean and AND it; it branches on the
 * comparison and either zeroes the value or masks it with 1.  That is exactly
 * how GCC compiles `x &= (a <= b)` when it cannot prove `x` is already 0 or 1,
 * and the two sites do it the same way round.  Written as the object's
 * branches would be a different function with the same behaviour; written as
 * the idiom it is one statement in each place.
 */
int
DataCarrierDetectV29(void *modem, short *in, unsigned short count)
{
	struct v29_rx_block *rx = ((struct v29_rx *)modem)->rx;
	struct v29_rx_detector *det = ((struct v29_rx *)modem)->det;
	int detected = (short)(rx->sre.active & rx->agc.signal);
	short rms;

	if (det->gate_1c == 0) {
		if ((short)rx->dec.sym_count > 999)
			detected &= rx->fse.mse
					<= V29RX_DEC_ERROR_MAX;

		if (rx->fse.mse > V29RX_DEC_ERROR_MAX
		    && DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"V29 Decoder error too big... no carrier\n");
	} else {
		if ((detected & (rx->fse.mse
				 <= V29RX_DEC_ERROR_MAX)) == 0)
			det->v21_enable = 1;

		detected = 1;

		if (det->v21_enable != 0) {
			short *buf = (short *)det->v21_buf;
			short i;

			for (i = 0; i < (int)count; i++)
				buf[i] = in[i];

			FPM_AGC_agc((struct fpm_agc *)(void *)
					&det->v21_agc,
				    (short *)det->v21_buf,
				    count);

			det = ((struct v29_rx *)modem)->det;
			if (FPM_MTD_detect((struct fpm_mtd *)
						det->v21_mtd,
					   (const short *)
						det->v21_buf,
					   (short)count) != 0)
				((struct v29_rx *)modem)->det->v21_samples = 0;
			else
				det->v21_samples =
					(short)(det->v21_samples + count);

			if (det->v21_samples
			    > V29DET_V21_THRESHOLD) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V29: V21 Carrier detected\n");
				detected = 0;
			}

			rx = ((struct v29_rx *)modem)->rx;
		}
	}

	if (rx->short_4f64 == 0)
		return detected;

	rms = FPM_rms(in, count);
	rx = ((struct v29_rx *)modem)->rx;

	if (rms < ((rx->rms_ref * V29RX_RMS_DROP_Q15)
		   >> V29RX_RMS_SHIFT)) {
		detected = 0;
		if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf(
				"sudden energy drop > 8[dB], no carrier");
			rx = ((struct v29_rx *)modem)->rx;
		}
	}

	if ((short)(rx->rms_n + 1) == 2) {
		rx->rms_ref = rms;
		rx->rms_n = 0;
	} else {
		rx->rms_n =
			(short)(rx->rms_n + 1);
	}

	return detected;
}

/*
 * ---------------------------------------------------------------------------
 * QualityDetectV29 -- .text 0x0a6410, 266 bytes.
 *
 * Folds this block's decoder error into a running average and reports the
 * carrier bit, or 2 when the carrier bit is clear.
 *
 * The counter runs 0, 1, 2 ... and does three different things:
 *
 *   0            seed the average with this block's error outright
 *   1 .. 0x31    fold it in at one part in ten
 *   0x32         test the average against the limit ONCE, then stop
 *   0x33 and up  nothing at all -- neither the average nor the counter moves
 *
 * so the average is a fifty-block measurement taken once per carrier and then
 * frozen, not a filter that runs for ever.
 */
int
QualityDetectV29(void *modem)
{
	struct v29_rx_block *rx = ((struct v29_rx *)modem)->rx;
	int err = rx->fse.mse;
	int verdict = (short)(rx->sre.active & rx->agc.signal);
	short n;

	if (verdict == 0) {
		if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf(
				"V29 Dec error too big... unreliable data\n");
			rx = ((struct v29_rx *)modem)->rx;
		}
		verdict = V29Q_NO_CARRIER;
	}

	n = rx->dec_error_n;
	if (n == 0) {
		rx->dec_error_avg = (short)err;
		rx->dec_error_n = 1;
		return verdict;
	}

	if (n > V29Q_AVG_BLOCKS) {
		if (n != V29Q_VERDICT_BLOCK)
			return verdict;
		if (!(rx->dec_error_avg
		      > rx->dec_error_limit))
			rx->short_4f62 = 1;
	} else {
		rx->dec_error_avg = (short)
			(((rx->dec_error_avg * V29Q_AVG_KEEP
			   + V29Q_AVG_ROUND) >> V29Q_AVG_SHIFT)
			 + ((err * V29Q_AVG_NEW + V29Q_AVG_ROUND)
			    >> V29Q_AVG_SHIFT));
	}

	rx->dec_error_n =
		(short)(rx->dec_error_n + 1);

	return verdict;
}

/*
 * ---------------------------------------------------------------------------
 * EpochDetectV29 -- .text 0x0a6520, 22 bytes.
 */
int
EpochDetectV29(void *modem)
{
	return ((struct v29_rx *)modem)->rx->fse.lms_force != 0;
}

/*
 * ---------------------------------------------------------------------------
 * GetSNRV29 -- .text 0x0a6540, 23 bytes.
 *
 * The constant is 14; `GetSNRV17`'s is 13.  The object loads the error
 * `movzwl` where `QualityDetectV29` loads the same field `movswl`, and that
 * disagreement is FREE rather than forced: the difference is truncated to 16
 * bits by a `cwtl` before it leaves, so the upper half never reaches anything.
 * Finding F614's case, and the field's type is settled by the site where it is
 * NOT free.
 */
short
GetSNRV29(void *modem)
{
	return (short)(14 - ((struct v29_rx *)modem)->rx->fse.mse);
}

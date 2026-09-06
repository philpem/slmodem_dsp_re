/*
 * fpm_tone.c -- Fixed Point Modem: tone generation and detection.
 *
 * Reconstructed from dsplibs.o fpm_tone.c.  Every function in the translation
 * unit is here; the two written last, FPM_TONE_find_rev at .text 0x0ab170 and
 * FPM_TONE_filter at 0x0ab3a0, are placed between FPM_TONE_detect and
 * FPM_TONE_kill because that is where the object puts them.
 *
 * The whole 0x108-byte object is now modelled as fields in
 * include/dsplib/fpm_tone.h.  The span that resisted longest, +0x4c..+0xf2,
 * is FPM_TONE_find_rev's entire working set and nothing else's.
 */

#include <stddef.h>

#include "dsplib/fpm_tone.h"
#include "dsplib/fpm_phasor.h"
#include "dsplib/fpm_iir.h"
#include "dsplib/sysdep.h"

#define NELEMS(a) (sizeof(a) / sizeof((a)[0]))

/*
 * Hz to phase increment.  A cycle is FPM_PHASOR_CYCLE (0x8000) units, so the
 * increment is hz * 32768 / fs -- and the original hard-codes fs = 8000 as
 * the constant 0x8312 in Q13 (33554 / 8192 = 4.096 = 32768 / 8000).
 *
 * Reproduced exactly, including the +0x1000 rounding bias.  Recorded as R-9
 * in docs/rate_assumptions.md; do not "fix" it without reading that entry.
 */
void
FPM_TONE_set_freq(struct fpm_tone *state, short hz)
{
	state->inc = (short)(((int)hz * 0x8312 + 0x1000) >> 13);
}

void
FPM_TONE_set_scale(struct fpm_tone *state, short scale)
{
	state->cfg.scale = scale;
}

short
FPM_TONE_generate(struct fpm_tone *state, short *out, short count)
{
	struct fpm_phasor p;
	int scale = state->cfg.scale;
	int period = state->cfg.rev_period;
	int elapsed;
	int i;

	p.phase = (unsigned short)state->phase;
	p.inc = (unsigned short)state->inc;
	p.cos = p.sin = 0;

	for (i = 0; i < count; i++) {
		FPM_phasor(&p);
		out[i] = (short)((scale * p.sin) >> 14);
	}

	/*
	 * Phase-reversal bookkeeping.  The counter advances in units of eight
	 * samples, so at 8 kHz it ticks in milliseconds and the default period
	 * of 450 is the ITU-T V.25 figure directly.
	 */
	elapsed = (unsigned short)state->rev_count
		  + (count >> 3);

	if (period > 0 && period <= elapsed) {
		int phase = (short)p.phase;

		state->rev_count = 0;

		/*
		 * Half of a 0x8000 cycle is 180 degrees.  The original adds
		 * 0x4000 and, if that overflows the positive range, subtracts
		 * 0x4000 from the *original* phase instead -- which is the same
		 * rotation the other way, not a wrap.
		 */
		if (phase + 0x4000 > 0x7fff)
			phase = phase - 0x4000;
		else
			phase = phase + 0x4000;

		p.phase = (unsigned short)phase;
	} else {
		state->rev_count = (short)elapsed;
	}

	state->phase = (short)p.phase;

	return count;
}

/*
 * FPM_TONE_generate2 -- .text 0x0aae30, 148 bytes.
 *
 * The QUADRATURE pair, not a second tone: one call fills two buffers with the
 * cosine and the sine of the same oscillator at the same instant.  Named
 * "generate2" for the two outputs; the object still carries one frequency and
 * one phase, and only `phase` is written back.
 *
 * So it sits between the other two generators rather than beside them --
 * FPM_TONE_generate takes the sine, FPM_TONE_generate_demod takes the cosine,
 * this one takes both.  Like _generate_demod and unlike _generate it does no
 * phase-reversal bookkeeping at all, and it returns `count`.
 *
 * `state->cfg.scale` is re-read from the object on each of the two multiplies,
 * which is what the original does (two `movswl 0x2(%ebp)` in one iteration,
 * around a 16-bit store the compiler had to assume might alias).  Written the
 * same way, so a caller whose output buffer overlaps the object sees the same
 * thing we do.
 *
 * The counter is 16-bit and the loop tests for -1 rather than for zero, so a
 * count of 0 writes nothing and a NEGATIVE count runs about 65536 times --
 * the same shape, and the same hazard, as FPM_TONE_detect and
 * FPM_TONE_generate_demod.
 *
 * The original leaves the phasor's cos and sin uninitialised on the stack;
 * they are cleared here so the reconstruction has no indeterminate reads.
 * FPM_phasor writes both before either is read, so this cannot differ.
 */
short
FPM_TONE_generate2(struct fpm_tone *state, short *cos_out, short *sin_out,
		   short count)
{
	struct fpm_phasor p;
	int i;

	p.phase = state->phase;
	p.inc = state->inc;
	p.cos = p.sin = 0;

	for (i = (short)(count - 1); i != -1; i = (short)(i - 1)) {
		FPM_phasor(&p);
		*cos_out++ = (short)((state->cfg.scale * p.cos) >> 14);
		*sin_out++ = (short)((state->cfg.scale * p.sin) >> 14);
	}

	state->phase = p.phase;
	return count;
}

/* Pointer-sized field access, for the slots that hold buffers. */
/* The built-in configuration: the ITU-T V.25 answer tone. */

struct fpm_tone *
FPM_TONE_create(struct fpm_tone *state, const struct fpm_tone_cfg *cfg)
{
	struct fpm_phasor p;
	int owned = 0;
	int increment;
	int damp;
	int len;
	int i;

	if (state == NULL) {
		/*
		 * sizeof, NOT the 0x108 literal.  The two are equal under the
		 * 32-bit ABI the blob uses -- the assertion at the bottom of
		 * this file says so -- but the struct holds five pointers, so
		 * on a 64-bit target it is larger and the literal would
		 * under-allocate by twenty-odd bytes.  That is a heap overrun
		 * that `make check64` cannot see, because it only compiles.
		 */
		state = (struct fpm_tone *)sysdep_malloc(sizeof(*state));
		if (state == NULL)
			return NULL;
		owned = 1;
	}

	sysdep_memcpy(&state->cfg,
		      cfg != NULL ? (const void *)cfg
				  : (const void *)&FPM_TONE_CFG,
		      sizeof(state->cfg));

	len = state->cfg.len;

	/*
	 * Buffers belong to whoever allocated the object.  A caller supplying
	 * its own state supplies its own buffers, so this is skipped -- see
	 * the ownership contract in docs/findings.md.
	 */
	if (owned && len > 0) {
		state->kernel = (short *)sysdep_malloc((unsigned)len * 2);
		state->history = (short *)sysdep_malloc(
			(unsigned)(len + state->cfg.extra) * 2);
		state->rev_block = (short *)sysdep_malloc(10);
		state->rev_acc = (short *)sysdep_malloc(8);
	}

	/*
	 * Evaluate cos and sin *at* the phase increment, without advancing:
	 * phase = increment, inc = 0.  That gives cos(omega) for
	 * omega = 2*pi*f/8000, which is what the Goertzel coefficient needs.
	 */
	increment = ((int)state->cfg.freq * 0x8312 + 0x1000) >> 13;
	p.phase = (unsigned short)increment;
	p.inc = 0;
	p.cos = p.sin = 0;
	FPM_phasor(&p);

	state->phase = 0;
	state->rev_count = 0;
	state->inc = (short)p.phase;

	damp = state->cfg.damp;

	/* Exact-frequency Goertzel section. */
	state->iir_coeff[0] = 0x4000;
	state->iir_coeff[1] = 0x4000;
	state->iir_coeff[2] = (short)-(2 * p.cos);
	state->iir_coeff[3] = (short)((damp * damp) >> 16);
	state->iir_coeff[4] = (short)((-(p.cos * damp)) >> 14);
	/*
	 * Clear the running state: the notch's history, both energy
	 * estimates, and the whole of the phase-reversal search's working set.
	 *
	 * The original does this as two loops, 0x40..0x50 and 0x52..0xf0,
	 * split around the hist_idx write between them.  Those bounds are what
	 * says where `rev_hist` begins and ends -- the first loop stops on
	 * `rev_energy` and the second is exactly the eighty words of the delay
	 * line -- so the field split in fpm_tone.h is the object's own and not
	 * an inference from FPM_TONE_find_rev.
	 *
	 * CORRECTION: an earlier version of this stopped at 0xf0 and claimed
	 * the last word of the region was deliberately left alone.  It is
	 * not -- there is a separate store of zero to +0xf2 immediately after
	 * the loop, outside it.  The mistake was invisible for as long as the
	 * test harness handed out zeroed memory; it showed up the moment
	 * fresh allocations were filled with a non-zero pattern.
	 */
	for (i = 0; i < (int)NELEMS(state->iir_state); i++)
		state->iir_state[i] = 0;
	state->e_tone = 0;
	state->e_total = 0;
	state->rev_age = 0;
	state->rev_corr = 0;
	state->rev_energy = 0;
	for (i = 0; i < (int)NELEMS(state->rev_hist); i++)
		state->rev_hist[i] = 0;
	state->rev_idx = 0;
	state->hist_idx = 0;

	/*
	 * Correlation reference: the source waveform modulated by a cosine
	 * sweeping at the tone frequency.  Setting inc to the current phase is
	 * what starts that sweep.
	 */
	p.inc = p.phase;
	{
		const short *src = (const short *)state->cfg.src;
		short *ref = (short *)state->kernel;
		short *zero = (short *)state->history;

		for (i = 0; i < len; i++) {
			FPM_phasor(&p);
			zero[i] = 0;
			ref[i] = (short)((2 * src[i] * p.cos) >> 14);
		}
	}

	/*
	 * Damped resonator, r = 0.96.  Evaluated from phase 0 so cos = 1.0;
	 * a caller retunes it later via FPM_TONE_set_freq.
	 */
	state->iir_self = state->iir_coeff;
	state->kill_state[0] = 0;
	state->kill_state[1] = 0;
	state->kill_state[2] = 0;
	state->kill_state[3] = 0;

	p.phase = 0;
	p.inc = 0;
	FPM_phasor(&p);
	{
		short *blk = (short *)state->rev_block;
		short *acc = (short *)state->rev_acc;

		blk[0] = 0x4000;
		blk[1] = 0x4000;
		blk[2] = (short)-(2 * p.cos);
		blk[3] = 0x3afb;			/* 0.96^2       */
		blk[4] = (short)((p.cos * -31457) >> 14);	/* -2 * 0.96 */

		acc[0] = acc[1] = acc[2] = acc[3] = 0;
	}

	return state;
}

/*
 * Tear down a tone object.
 *
 * Note the asymmetry with create, which is the original's and is reproduced.
 * create allocates the four buffers only when it allocated the object *and*
 * len > 0; delete frees them whenever len > 0, with no ownership test, and
 * then frees the object unconditionally.
 *
 * So a caller that supplied its own state with a positive len would have its
 * buffer slots freed without them ever having been allocated, and its state
 * freed even if it lived on the stack.  That never happens in practice --
 * every call site passes NULL to create -- but the mismatch is real, and
 * "tidying" delete to match create would change behaviour for the paths that
 * do exist.  See D5 in docs/deviations.md.
 */
void
FPM_TONE_delete(struct fpm_tone *state)
{
	if (state->cfg.len > 0) {
		sysdep_free(state->rev_acc);
		sysdep_free(state->rev_block);
		sysdep_free(state->history);
		sysdep_free(state->kernel);
	}
	sysdep_free(state);
}

/*
 * ---------------------------------------------------------------------------
 * FPM_TONE_detect -- .text 0x0aaf80, 488 bytes.
 *
 * The detector half of the tone object.  Same shape as FPM_MTD_detect: measure
 * total energy and in-band energy, and call the tone present when what is left
 * over is a small enough fraction of the total.
 *
 * Per sample:
 *
 *   1. push the sample into a circular history of `taps` words and correlate
 *      the whole history against `kernel` -- the ToneLPF prototype
 *      FPM_TONE_create copied there;
 *   2. square the result: that is the TOTAL energy in this sample;
 *   3. run the same result through the one-section Goertzel resonator that
 *      FPM_TONE_create built in `iir_coeff`, and square that;
 *   4. smooth both, with the difference standing in for out-of-band energy.
 *
 * The two smoothers use 31130 and 1638 -- 0.95 and 0.05 in Q15, summing to
 * exactly 32768, i.e. unity DC gain.  Worth noticing: 31130 is precisely the
 * alpha that `AGC_DEF_ALPHA`'s slow pair should have carried alongside its
 * beta of 1638 and does not.  Whoever wrote this got it right; see D6.
 *
 * The verdict, and its polarity, which is the reverse of the obvious reading:
 *
 *   total < state[+0x0a]                 -> 2, no signal at all
 *   tone_share <= ratio * total / 32768  -> 1, signal, but not this tone
 *   otherwise                            -> 0, THE TONE IS PRESENT
 *
 * The reason is the biquad: { 1, -2cos(w), 1 } is a pair of zeros on the unit
 * circle, i.e. a NOTCH at the tone frequency, and the poles just inside it at
 * radius r only narrow the notch.  So `filtered` is the signal with the tone
 * taken OUT, and `energy - filtered^2` is the tone's own contribution.  A
 * large share means the tone dominates, which is why the large case returns
 * zero.  Measured: 2100 Hz in gives 0, 2000 and 2200 give 1.
 */
short
FPM_TONE_detect(struct fpm_tone *state, const short *samples, short count)
{
	const short *kernel = (const short *)state->kernel;
	short *hist = (short *)state->history;
	short *iir_coeff = state->iir_coeff;
	short *iir_state = state->iir_state;
	int taps = state->cfg.len;
	int idx = state->hist_idx;
	int out_of_band = state->e_tone;
	int total = state->e_total;
	int ratio = state->cfg.ratio;
	int min_level = state->cfg.min_level;
	int i;

	/*
	 * Counted as a 16-bit value and tested against -1, so `count` of 0
	 * does nothing and a NEGATIVE count runs about 65536 times.  Written
	 * out rather than as `i < count` because the difference is real.
	 */
	for (i = (short)(count - 1); i != -1; i = (short)(i - 1)) {
		const short *c = kernel;
		short *p;
		int acc = 0;
		int filtered, energy, in_band, excess;
		short sample;
		int k;

		/*
		 * Advance the write index, wrapping to zero.  The original
		 * does this branchlessly (setl/neg/and), which is GCC 3.4.2
		 * rendering exactly this conditional.
		 */
		idx = ((short)(idx + 1) < taps) ? (short)(idx + 1) : 0;
		hist[idx] = *samples++;

		/*
		 * Correlate the whole history against the kernel, newest
		 * first, wrapping once at the bottom of the buffer.  Two
		 * loops rather than a modulo: idx+1 taps then taps-1-idx.
		 */
		p = &hist[idx];
		for (k = idx; k >= 0; k--)
			acc += *p-- * *c++;
		p += taps;		/* p is at hist[-1]; wrap to the top */
		for (k = taps - 1; k > idx; k--)
			acc += *p-- * *c++;

		sample = (short)(acc >> 15);

		/*
		 * Total energy.  Note this can come out as -32768: a sample of
		 * -32768 squares to 2^30, and 2^30 >> 15 is 32768, which does
		 * not fit.  Faithful, and the smoother recovers.
		 */
		energy = (short)(((int)sample * sample) >> 15);

		/* In-band energy: the same sample through the resonator. */
		filtered = sample;
		{
			short one = (short)filtered;

			FPM_iir_filt_II(&one, iir_coeff, iir_state, 1, 1);
			filtered = one;
		}
		in_band = ((int)filtered * filtered) >> 15;

		/*
		 * `filtered` has the tone notched OUT, so this difference is
		 * the tone's own share of the energy -- not, as the name in
		 * the state suggests, what is left over.
		 */
		excess = (short)(energy - in_band);

		out_of_band = (short)((31130 * out_of_band + 1638 * excess) >> 15);
		total = (short)((31130 * total + 1638 * energy) >> 15);
	}

	/*
	 * Negative out-of-band energy is clamped away on the way into the
	 * state -- the subtraction above can undershoot -- but only here, so
	 * it stays negative for the rest of this call.  The original does it
	 * branchlessly as `(~v >> 15) & v`.
	 */
	if (out_of_band < 0)
		out_of_band = 0;

	state->hist_idx = (short)idx;
	state->e_total = (short)total;
	state->e_tone = (short)out_of_band;

	if ((short)total < min_level)
		return FPM_TONE_NOSIGNAL;
	return (out_of_band <= ((ratio * (short)total) >> 15))
		? FPM_TONE_OTHER : FPM_TONE_PRESENT;
}

/*
 * FPM_TONE_generate_demod -- .text 0x0aaed0, 125 bytes.
 *
 * The reference oscillator the demodulator correlates against: the same
 * generator as FPM_TONE_generate, minus two things.
 *
 *   - it uses the COSINE, via FPM_phasor_demod, where the modulator uses the
 *     sine.  Same tone, ninety degrees apart.
 *   - there is no phase-reversal bookkeeping at all.  This is a plain
 *     oscillator; the reversals belong to the ANSam transmitter.
 *
 * It also returns `count`, which FPM_TONE_generate does not.
 *
 * The original leaves the phasor's cos and sin fields uninitialised on the
 * stack.  Harmless -- FPM_phasor_demod writes cos before anything reads it,
 * and sin is never touched -- but they are cleared here so the reconstruction
 * has no indeterminate reads.
 */
short
FPM_TONE_generate_demod(struct fpm_tone *state, short *out, short count)
{
	struct fpm_phasor p;
	int scale = state->cfg.scale;
	int i;

	p.phase = (unsigned short)state->phase;
	p.inc = (unsigned short)state->inc;
	p.cos = p.sin = 0;

	for (i = (short)(count - 1); i != -1; i = (short)(i - 1)) {
		FPM_phasor_demod(&p);
		*out++ = (short)((scale * p.cos) >> 14);
	}

	state->phase = (short)p.phase;
	return count;
}

/*
 * ---------------------------------------------------------------------------
 * FPM_TONE_find_rev -- .text 0x0ab170, 547 bytes.
 *
 * The receive-side counterpart of FPM_TONE_generate's reversal bookkeeping.
 *
 * WHAT IT MEASURES is a sign change in the input's autocorrelation at a lag of
 * cfg.rev_lag samples, reported as the interval since the last one.  That is a 180
 * degree phase reversal ONLY for a carrier whose period divides the lag, and
 * the config it shares is not such a carrier: cfg.rev_lag is 40, which at 8 kHz is
 * ten and a HALF cycles of the built-in 2100 Hz, so a steady tone already
 * correlates negatively, the test below is satisfied continuously and the
 * function degenerates into a 20 ms metronome.  At 1800 Hz the same lag is
 * nine whole cycles and it behaves as its name says.  Do not read it as a
 * V.25 answer-tone detector on the strength of the configuration it is
 * declared beside.
 *
 * Its one caller in the object is `RxHdxPhsReversal` at .text 0x083b20, which
 * narrows the result with `movswl %ax,%edi` -- so the return type is `short`,
 * and that is the caller's word for it rather than an inference.
 *
 * Three things happen per call.
 *
 * 1. The whole buffer is filtered IN PLACE through the single biquad at
 *    `rev_block`, with `rev_acc` as its state.  This happens BEFORE the count
 *    is looked at, so a call with a count of zero still filters nothing but
 *    still runs the filter's own argument checks.
 *
 * 2. Each filtered sample is pushed into `rev_hist`, which is 2*cfg.rev_lag words
 *    long, and two sliding sums are updated:
 *
 *      rev_corr    sum over the last cfg.rev_lag products x[k] * x[k - cfg.rev_lag]
 *      rev_energy  sum over the last 2*cfg.rev_lag squares, each Q15-scaled
 *
 *    Both are maintained by adding the new term and subtracting the one
 *    falling out of the window, which is what makes the single read of
 *    `rev_hist[idx]` -- the sample from 2*cfg.rev_lag ago, about to be
 *    overwritten -- serve as both operands of the removal.
 *
 * 3. A reversal is declared when
 *
 *      2 * rev_corr  <  (cfg.rev_thresh * rev_energy) >> 15
 *
 *    i.e. when the lag-cfg.rev_lag correlation drops below a configured fraction
 *    of the energy.  `rev_age` counts samples and is reported, in units of
 *    eight, only if it has passed 160 -- so reversals closer together than 20
 *    ms are swallowed and the result is in the same units as cfg.rev_period.
 *    The last one found in the block is the one returned.
 *
 * WHAT THE SATURATION DOES AND DOES NOT REACH, because it is the one place a
 * plausible rewrite diverges.  The two sums run as 32-bit values for the whole
 * block and are NEVER written back from their clamped forms; only the clamped
 * short reaches the threshold test and the object.  So
 * `corr = SAT(corr + term)` is wrong -- it would fold the clamp back into the
 * running sum -- while the clamp still persists between calls, because the
 * next call re-seeds the 32-bit sum from the stored short.  N one-sample calls
 * therefore do not equal one N-sample call once either sum leaves +-32767, and
 * the differential test drives both.
 *
 * `count` is compared against, not counted down through: a negative count does
 * nothing at all here, where FPM_TONE_detect and the generators would run
 * about 65536 times.  Reproduced as written.
 */
short
FPM_TONE_find_rev(struct fpm_tone *state, short *samples, short count)
{
	short *hist = state->rev_hist;
	int lag = state->cfg.rev_lag;
	int idx = state->rev_idx;
	int corr = state->rev_corr;
	int energy = state->rev_energy;
	short corr_s = state->rev_corr;
	short energy_s = state->rev_energy;
	short period = 0;
	short n;

	FPM_iir_filt_II(samples, state->rev_block, state->rev_acc, 1, count);

	for (n = 0; n < count; n++) {
		short j;

		/*
		 * Advance the write position, wrapping at 2*lag; then step
		 * back by `lag` around the same ring for the delayed sample.
		 * Both are 16-bit, and the wrap of the second is a single
		 * add rather than a modulo because it can only underflow once.
		 */
		idx = ((short)(idx + 1) < 2 * lag) ? (short)(idx + 1) : 0;
		j = (short)(idx - lag);
		if (j < 0)
			j = (short)(j + 2 * lag);

		/*
		 * hist[idx] is the sample from 2*lag ago and is read before it
		 * is overwritten, so one load serves as the term leaving the
		 * correlation window and the term leaving the energy window.
		 */
		corr += ((samples[n] - hist[idx]) * hist[j] + 0x4000) >> 15;
		if (corr > 32767)
			corr_s = 32767;
		else if (corr < -32768)
			corr_s = -32768;
		else
			corr_s = (short)corr;

		energy += ((samples[n] * samples[n]) >> 15)
			  - ((hist[idx] * hist[idx]) >> 15);
		if (energy > 32767)
			energy_s = 32767;
		else if (energy < -32768)
			energy_s = -32768;
		else
			energy_s = (short)energy;

		if (2 * corr_s < (state->cfg.rev_thresh * energy_s) >> 15) {
			if (state->rev_age > 160) {
				period = (short)(state->rev_age >> 3);
				state->rev_age = 0;
			}
		}
		state->rev_age++;

		hist[idx] = samples[n];
	}

	state->rev_idx = (short)idx;
	state->rev_corr = corr_s;
	state->rev_energy = energy_s;

	return period;
}

/*
 * ---------------------------------------------------------------------------
 * FPM_TONE_filter -- .text 0x0ab3a0, 235 bytes.
 *
 * The detector's correlator on its own, run over the caller's buffer in place.
 * Sample for sample it is FPM_TONE_detect's step 1 and nothing after it: the
 * same `history` ring of cfg.len words, the same `kernel`, the same two loops
 * around the wrap, the same unsaturated 32-bit accumulator and the same
 * `>> 15` on the way out.
 *
 * It shares `hist_idx` with FPM_TONE_detect rather than keeping its own -- the
 * object loads and stores +0x34, not a second index -- which is the one thing
 * worth knowing before calling both on one tone object.
 *
 * The counter is 16-bit and tested against -1, so a count of 0 writes nothing
 * and a NEGATIVE count runs about 65536 times.  That is the module's usual
 * shape and it is FPM_TONE_find_rev, not this, that is the exception.
 *
 * Nothing in dsplibs.o calls it: `readelf -r` finds relocations naming
 * FPM_TONE_detect, _kill and _find_rev and none naming this, and the symbol is
 * GLOBAL, so a same-translation-unit call would still have left one.
 */
void
FPM_TONE_filter(struct fpm_tone *state, short *samples, short count)
{
	const short *kernel = (const short *)state->kernel;
	short *hist = (short *)state->history;
	int taps = state->cfg.len;
	int idx = state->hist_idx;
	int i;

	for (i = (short)(count - 1); i != -1; i = (short)(i - 1)) {
		const short *c = kernel;
		short *p;
		int acc = 0;
		int k;

		idx = ((short)(idx + 1) < taps) ? (short)(idx + 1) : 0;
		hist[idx] = *samples;

		p = &hist[idx];
		for (k = idx; k >= 0; k--)
			acc += *p-- * *c++;
		p += taps;		/* p is at hist[-1]; wrap to the top */
		for (k = taps - 1; k > idx; k--)
			acc += *p-- * *c++;

		*samples++ = (short)(acc >> 15);
	}

	state->hist_idx = (short)idx;
}

/*
 * FPM_TONE_kill -- .text 0x0ab490, 61 bytes.
 *
 * One tail call's worth of function: the detector's notch, run over the
 * caller's buffer to take the tone back out of it.
 *
 * Three things it does NOT do, each of which would be the natural guess:
 *
 *   - it does not use `iir_coeff` directly.  The object stores a pointer to
 *     its own coefficients at +0xfc and this reads that, `mov 0xfc(%edx)`,
 *     rather than the `lea 0x36(%edx)` an array reference would give.
 *   - it does not share `iir_state`.  +0x100 is a second four-word direct
 *     form I state, so a kill pass and a detect pass over different streams
 *     do not corrupt each other's history.
 *   - it does not touch the phase-reversal fields at +0xf4 / +0xf8, which are
 *     FPM_TONE_find_rev's.
 *
 * The section count is the immediate 1: one biquad, matching the five
 * coefficients FPM_TONE_create lays down at +0x36.
 */
void
FPM_TONE_kill(struct fpm_tone *state, short *samples, short count)
{
	FPM_iir_filt_II(samples, state->iir_self, state->kill_state, 1, count);
}

/*
 * The reserved region is a byte count from a 32-bit build, so these are
 * compiled only under that ABI -- see the same note in src/pump/b103/b103fp.c.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define TONE_ASSERT_OFF(field, off) \
	typedef char fpm_tone_off_##field[ \
		((int)__builtin_offsetof(struct fpm_tone, field) == (off)) \
			? 1 : -1]

TONE_ASSERT_OFF(phase, 0x24);
TONE_ASSERT_OFF(inc, 0x26);
TONE_ASSERT_OFF(rev_count, 0x28);
TONE_ASSERT_OFF(kernel, 0x2c);
TONE_ASSERT_OFF(history, 0x30);
TONE_ASSERT_OFF(hist_idx, 0x34);
TONE_ASSERT_OFF(iir_coeff, 0x36);
TONE_ASSERT_OFF(iir_state, 0x40);
TONE_ASSERT_OFF(e_tone, 0x48);
TONE_ASSERT_OFF(e_total, 0x4a);
TONE_ASSERT_OFF(rev_age, 0x4c);
TONE_ASSERT_OFF(rev_corr, 0x4e);
TONE_ASSERT_OFF(rev_energy, 0x50);
TONE_ASSERT_OFF(rev_hist, 0x52);
TONE_ASSERT_OFF(rev_idx, 0xf2);
TONE_ASSERT_OFF(rev_block, 0xf4);
TONE_ASSERT_OFF(rev_acc, 0xf8);
TONE_ASSERT_OFF(iir_self, 0xfc);
TONE_ASSERT_OFF(kill_state, 0x100);

typedef char fpm_tone_cfg_size[(sizeof(struct fpm_tone_cfg) == 0x24) ? 1 : -1];
typedef char fpm_tone_size[(sizeof(struct fpm_tone) == FPM_TONE_STATE_SIZE)
			   ? 1 : -1];

#endif /* 32-bit */

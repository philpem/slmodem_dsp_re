/*
 * fpm_fsd.c -- Fixed Point Modem: Frequency Shift Demodulator.
 *
 * Reconstructed from dsplibs.o fpm_fsd.c:
 *   FPM_FSD_init  .text 0x0a78e0
 *   FPM_FSD_free  .text 0x0a7ce0
 *   FPM_FSD_demodulate .text 0x0a79f0, 745 bytes -- the largest single
 *                      function on the Bell 103 path
 */

#include "dsplib/fpm_fsd.h"
#include "dsplib/fpm_iir.h"
#include "dsplib/sysdep.h"

void
FPM_FSD_init(struct fpm_fsd *state, const struct fpm_fsd_cfg *cfg, int fresh)
{
	int i;

	state->cfg = *cfg;
	state->hist_idx = 0;
	state->bit = 0;
	state->since_bit = 0;
	state->disagreements = 0;

	/*
	 * Unlike FPM_MRF_init, `fresh` here does not consult the existing
	 * buffers at all -- it simply allocates.  Re-initialising in place
	 * (fresh == 0) reuses whatever is there, so the caller must not change
	 * the lengths between calls.
	 */
	if (fresh) {
		state->fir_hist = (short *)sysdep_malloc(
			(unsigned)state->cfg.fir_taps * sizeof(short));
		state->iir_hist = (short *)sysdep_malloc(
			(unsigned)state->cfg.iir_len * 2 * sizeof(short));
		state->trace = (short *)sysdep_malloc(
			(unsigned)state->cfg.trace_len * sizeof(short));
	}

	for (i = 0; i < state->cfg.fir_taps; i++)
		state->fir_hist[i] = 0;

	for (i = 0; i < 2 * state->cfg.iir_len; i++)
		state->iir_hist[i] = 0;

	state->last_count = 0;
	state->f22 = (short)(state->cfg.bit_samples >> 1);

	for (i = 0; i < state->cfg.trace_len; i++)
		state->trace[i] = 0;
}

/*
 * Frees all three buffers unconditionally but *not* the state, which is the
 * opposite of FPM_TONE_delete.  The caller owns the state -- B103FP_create
 * embeds it in a larger block rather than allocating it separately.
 */
void
FPM_FSD_free(struct fpm_fsd *state)
{
	sysdep_free(state->trace);
	sysdep_free(state->iir_hist);
	sysdep_free(state->fir_hist);
}

/*
 * ---------------------------------------------------------------------------
 * The demodulator.
 *
 * Four stages per input sample, then a bit clock that only sometimes ticks.
 *
 * 1. INPUT FIR.  A circular history of `fir_taps` words correlated against the
 *    kernel, exactly as in FPM_TONE_detect: newest first, two loops rather
 *    than a modulo.
 *
 * 2. DISCRIMINATOR.  The filtered sample multiplied by one from `delay`
 *    samples ago.  For a tone at frequency f the product's DC term is
 *    cos(2*pi*f*delay/fs)/2, so a frequency change moves the DC level -- which
 *    is the whole trick.  The shift is 13, not the 15 used everywhere else, so
 *    the product carries a gain of four into the next stage.
 *
 *    The delayed index wraps by adding `fir_taps` when it goes negative.
 *    Worth stating because the alternative -- reading below the buffer -- is
 *    live for the first `delay` samples after a reset, and would make every
 *    result depend on whatever the allocator left there.
 *
 * 3. LOWPASS.  FPM_iir_filt, `iir_len` sections, removing the tone-frequency
 *    component and leaving the slowly-varying DC.
 *
 * 4. SLICER.  A Schmitt trigger, not a comparator: the bit changes only when
 *    the lowpass output passes +/- `slice_level`, and holds its previous value
 *    in between.  This is what makes the frequency plan work despite finding
 *    31 -- the two tones do not straddle zero, they straddle whatever the
 *    lowpass leaves, and hysteresis around a dead zone is what keeps a noisy
 *    signal near the boundary from chattering.
 *
 * THE BIT CLOCK is the part worth reading twice.  Two counters:
 *
 *   since_bit      samples since the last bit was emitted
 *   disagreements  samples on which the slicer disagreed with the committed
 *                  bit -- CUMULATIVE within the current bit period, not
 *                  consecutive.  It is only ever reset when a bit is emitted.
 *
 * and two ways for a bit to come out:
 *
 *   - the slicer agrees with the committed bit and a full `bit_samples` have
 *     elapsed: emit the committed bit, reset both counters.  This is the
 *     free-running case, carrying a run of identical bits.
 *
 *   - the slicer has disagreed on `bit_samples / 2` samples: the transition is
 *     believed.  Commit the new bit, emit it, and reset both counters -- which
 *     RESYNCHRONISES the bit clock to the edge.  That is the timing recovery:
 *     there is no separate phase detector, the edge simply restarts the count.
 *
 * A glitch shorter than half a bit therefore never reaches the output, and a
 * genuine transition drags the clock back into step with the sender.
 *
 * OUTPUT CAP.  The loop also stops once `max_bits + 1` bits have been written,
 * and the remaining input samples are thrown away rather than held over.  A
 * caller handing it more than about `(max_bits + 2) * bit_samples` samples
 * loses the tail silently.  Bell 103 uses max_bits = 6 and bit_samples = 8, so
 * the ceiling is 8 bits from 64 samples; DemodDataB103 feeds it 48 at a time.
 */
short
FPM_FSD_demodulate(struct fpm_fsd *state, const short *samples,
		   unsigned short *bits_out, unsigned short count)
{
	const short *fir = state->cfg.fir;
	const short *iir = state->cfg.iir;
	short *fir_hist = state->fir_hist;
	short *iir_hist = state->iir_hist;
	short *trace = state->trace;
	int taps = state->cfg.fir_taps;
	int sections = state->cfg.iir_len;
	int delay = state->cfg.delay;
	int slice_level = state->cfg.slice_level;
	int bit_hi = state->cfg.high_bit;
	int bit_lo = (short)(1 - state->cfg.high_bit);
	int idx = state->hist_idx;
	int nbits = 0;
	int i;

	for (i = (short)((short)count - 1); i != -1; i = (short)(i - 1)) {
		const short *c = fir;
		short *p;
		int acc = 0;
		int filtered, delayed, product, level, sliced, half;
		int bit_samples, k, d;

		/* --- 1. input FIR ------------------------------------- */
		idx = ((short)(idx + 1) < taps) ? (short)(idx + 1) : 0;
		fir_hist[idx] = *samples++;

		p = &fir_hist[idx];
		for (k = idx; k >= 0; k--)
			acc += *p-- * *c++;
		p += taps;
		for (k = taps - 1; k > idx; k--)
			acc += *p-- * *c++;

		filtered = (short)(acc >> 15);

		/* --- 2. delay-line discriminator ---------------------- */
		d = (short)(idx - delay);
		if (d < 0)
			d = (short)(d + taps);
		delayed = fir_hist[d];
		product = (short)((filtered * delayed) >> 13);

		/* --- 3. lowpass -------------------------------------- */
		level = (short)FPM_iir_filt((short)product, iir, iir_hist,
					    (short)sections);

		/*
		 * The trace buffer.  Written from the start on every call and
		 * never read; the pointer is a local, so nothing carries over.
		 * Nothing bounds it against trace_len either -- a call longer
		 * than trace_len samples runs off the end.
		 */
		*trace++ = (short)level;

		/* --- 4. Schmitt slicer ------------------------------- */
		sliced = state->bit;
		if (level < -slice_level)
			sliced = bit_lo;
		if (level > slice_level)
			sliced = bit_hi;
		sliced &= 1;

		/*
		 * Half a bit, rounded UP for a five-sample symbol and down for
		 * everything else.  The original special-cases 5 explicitly
		 * rather than rounding, so 5 gives 3 while 7 gives 3 as well.
		 */
		bit_samples = (unsigned short)state->cfg.bit_samples;
		half = (bit_samples == 5) ? 3 : (int)((unsigned)bit_samples >> 1);

		/* --- the bit clock ----------------------------------- */
		state->since_bit = (short)(state->since_bit + 1);

		if ((unsigned short)state->bit == (unsigned short)sliced) {
			if ((unsigned short)state->since_bit
			    == (unsigned short)bit_samples) {
				bits_out[nbits++] = (unsigned short)state->bit;
				state->disagreements = 0;
				state->since_bit = 0;
			}
		} else {
			state->disagreements =
				(short)(state->disagreements + 1);
			if ((unsigned short)state->disagreements
			    == (unsigned short)half) {
				state->bit = (short)sliced;
				bits_out[nbits++] = (unsigned short)sliced;
				state->disagreements = 0;
				state->since_bit = 0;
			}
		}

		nbits = (short)nbits;
		if (nbits > state->cfg.max_bits + 1)
			break;
	}

	state->hist_idx = (short)idx;
	state->last_count = (short)count;
	return (short)(unsigned short)nbits;
}

/*
 * The library default, from .data:0x812c.  Both filters are absent -- the
 * caller supplies them -- but every scalar is the one Bell 103 uses, so
 * B103FP_create patches only the two filters, the tap count, the delay and
 * (for V.21) `high_bit`.
 */
const struct fpm_fsd_cfg FPM_FSD_CFG_data = {
	.slice_level = 10,
	.high_bit = 1,
	.bit_samples = 8,
	.max_bits = 6,
	.trace_len = 160
	/*
	 * fir, fir_taps, delay, iir and iir_len are all zero: the caller
	 * supplies both filters.  B103FP_create patches exactly those five,
	 * plus high_bit for V.21.
	 */
};

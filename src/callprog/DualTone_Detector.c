/*
 * DualTone_Detector.c -- Dual Tone Detector: answer-tone detection.
 *
 * Reconstructed from dsplibs.o DualTone_Detector.c:
 *
 *   Dual_TONE_create               .text 0x07e2e0
 *   Dual_TONE_delete               .text 0x07e320
 *   Dual_TONE_detect               .text 0x07e330
 *
 * See dualtone.h for what it decides and why the measurement is a notch
 * difference.  This file is about how the arithmetic is spelled, which has
 * several details that a straightforward rewrite gets wrong.
 *
 * SCALING
 *
 * Q14 here, unlike toneiir.c's Q13.  Every filter section computes its sum in
 * 32 bits, shifts right by 14, and truncates to 16 -- no rounding, no
 * saturation, wrap on overflow.
 *
 * The input bandpass additionally *doubles* each section's output before
 * storing it, so its coefficients are effectively Q15 held in Q14 words: b0
 * reads 8192 where the design says 1.0.  The doubling happens after the
 * truncation to 16 bits, not before, so it is not the same as one extra bit
 * of shift.
 *
 * THE SHIFT TABLE HAS A UNUSED FIRST ENTRY
 *
 * The table is { 3, 3, 3, 0 }: the input shift and one per section.  But the
 * input shift is written into the code as a literal 3 and the table's first
 * entry is never read.  The two agree, so nothing is wrong; it just means
 * changing the table alone would not change the input scaling.
 */

#include "dsplib/dualtone.h"
#include "dsplib/sysdep.h"

/*
 * ---------------------------------------------------------------------------
 * Tables (.rodata; all file statics in the original, so none of them can be
 * compared against the blob directly -- they are checked through behaviour).
 */

/*
 * Input bandpass, .rodata+0x6d36.  Three sections, Q14, coefficient order
 * { a2, b2, a1, b1, b0 } -- the reverse of the usual, and the reverse of what
 * fpm_iir.h uses.  The zeros of sections 1 and 2 sit on the unit circle at DC
 * and Nyquist; the passband is centred at 0.26 of the sample rate, which is
 * 2100 Hz at the 8000 Hz this module runs at.
 */
static const short DualTone_bp_coeff[5 * DUAL_TONE_BP_SECTIONS] = {
	6862, -8192,  1495,      0, 8192,
	7508,  8192, -1140, -16384, 8192,
	7531,  8192,  4185,  16384, 8192
};

/*
 * .rodata+0x6d2e.  Entry 0 duplicates the literal input shift and is never
 * read; entries 1 to 3 rescale each section's output.
 */
static const short DualTone_bp_shift[4] = { 3, 3, 3, 0 };

/*
 * The three notches.  All share pole radius 0.9, so all share a2; only b1 and
 * a1 place the frequency.  Written out rather than tabulated because the
 * original inlines them as immediates -- there is no table in the object.
 */
#define NOTCH_A2	13271		/* 0.81 = 0.9^2, Q14              */
#define NOTCH_A_B1	 2571		/* zero at 0.26250 fs -- 2100 Hz  */
#define NOTCH_A_A1	 2314
#define NOTCH_B_B1	-5126		/* zero at 0.22500 fs -- 1800 Hz  */
#define NOTCH_B_A1	-4613
#define NOTCH_C_B1	 6393		/* zero at 0.28125 fs -- 2250 Hz  */
#define NOTCH_C_A1	 5753

/* Energy smoother, Q8: y = (205 y + 51 x) / 256, a 0.8/0.2 one-pole. */
#define ENERGY_ALPHA	205
#define ENERGY_BETA	51

/*
 * ---------------------------------------------------------------------------
 * Dual_TONE_create / delete
 */
struct dual_tone *
Dual_TONE_create(void)
{
	struct dual_tone *st;

	/*
	 * sizeof, not the 0x44 literal the original uses.  They agree on the
	 * 32-bit differential target; on a 64-bit host they would not, and
	 * the literal would under-allocate.
	 */
	st = sysdep_malloc(sizeof(*st));
	if (st == 0)
		return 0;

	sysdep_memset(st, 0, sizeof(*st));
	st->ratio = 226;
	st->min_energy = 1;

	return st;
}

void
Dual_TONE_delete(struct dual_tone *st)
{
	sysdep_free(st);
}

/*
 * ---------------------------------------------------------------------------
 * Dual_TONE_detect
 */

/*
 * One biquad of the input bandpass.  Coefficients { a2, b2, a1, b1, b0 };
 * history { x[n-1], x[n-2], y[n-1], y[n-2] }.
 *
 * The accumulator is allowed to wrap, which is what the original's 32-bit
 * add and sub do; spelled through unsigned so it is defined rather than left
 * to the compiler.  Five products of two 16-bit values do not fit in 32 bits
 * in general, and nothing here bounds the history.
 */
static short
bp_section(short *h, const short *c, short x)
{
	int acc = 0;
	short y;

	acc = (int)((unsigned)acc + (unsigned)(c[4] * (int)x));
	acc = (int)((unsigned)acc + (unsigned)(c[3] * (int)h[0]));
	acc = (int)((unsigned)acc + (unsigned)(c[1] * (int)h[1]));
	acc = (int)((unsigned)acc - (unsigned)(c[2] * (int)h[2]));
	acc = (int)((unsigned)acc - (unsigned)(c[0] * (int)h[3]));

	/*
	 * Truncate to 16 bits, THEN double.  Doing it the other way round --
	 * shifting by 13 instead of 14 -- differs whenever the truncation
	 * discards a bit, which is most of the time.
	 */
	y = (short)(2 * (short)(acc >> 14));

	h[1] = h[0];
	h[0] = x;
	h[3] = h[2];
	h[2] = y;

	return y;
}

/*
 * One notch.  b0 and b2 are exactly 1.0, hence the shifts rather than
 * multiplies; a2 is shared by all three.
 */
static short
notch(short *h, short x, int b1, int a1)
{
	int acc = 0;
	short y;

	acc = (int)((unsigned)acc + ((unsigned)(int)x << 14));
	acc = (int)((unsigned)acc + (unsigned)(b1 * (int)h[0]));
	acc = (int)((unsigned)acc + ((unsigned)(int)h[1] << 14));
	acc = (int)((unsigned)acc - (unsigned)(a1 * (int)h[2]));
	acc = (int)((unsigned)acc - (unsigned)(NOTCH_A2 * (int)h[3]));

	y = (short)(acc >> 14);

	h[1] = h[0];
	h[0] = x;
	h[3] = h[2];
	h[2] = y;

	return y;
}

/*
 * Energy removed by a notch: the total, less what survived it, floored at
 * zero.  Both squares are scaled down by 256 first, which is what stops the
 * smoother's 205x multiply from overflowing.
 *
 * The comparison is unsigned in the original.  It cannot matter here -- both
 * operands are squares shifted right logically, so both are non-negative --
 * but it is reproduced rather than reasoned away.
 */
static unsigned
notch_energy(unsigned total, short filtered)
{
	unsigned f = (unsigned)((int)filtered * (int)filtered) >> 8;

	return f > total ? 0u : total - f;
}

static unsigned
smooth(unsigned acc, unsigned x)
{
	return (ENERGY_ALPHA * acc + ENERGY_BETA * x) >> 8;
}

int
Dual_TONE_detect(struct dual_tone *st, const short *samples, int count)
{
	unsigned energy_a = (unsigned)st->energy_a;
	unsigned energy_b = (unsigned)st->energy_b;
	unsigned energy = (unsigned)st->energy;
	unsigned threshold;
	short i;

	/*
	 * Both hold timers advance by the whole block before a single sample
	 * is looked at, and both are 16 bits, so they wrap silently after
	 * about eight seconds of a tone that never wins.  Nothing depends on
	 * that: whichever tone does not win has its timer cleared below.
	 */
	st->hold_a = (short)(st->hold_a + count);
	st->hold_b = (short)(st->hold_b + count);

	/*
	 * `i` is truncated to 16 bits each time round in the original, so a
	 * block of more than 32767 samples would never terminate.  Reproduced
	 * with a short rather than papered over -- see docs/deviations.md.
	 */
	for (i = 0; i < count; i++) {
		short x = (short)(samples[i] >> 3);
		unsigned total;
		short ya, yb, yc;
		int s;

		for (s = 0; s < DUAL_TONE_BP_SECTIONS; s++) {
			x = bp_section(&st->bp[4 * s], &DualTone_bp_coeff[5 * s],
				       x);
			x = (short)((int)x >> DualTone_bp_shift[s + 1]);
		}

		ya = notch(st->notch_a, x, NOTCH_A_B1, NOTCH_A_A1);
		yb = notch(st->notch_b, x, NOTCH_B_B1, NOTCH_B_A1);
		yc = notch(st->notch_c, yb, NOTCH_C_B1, NOTCH_C_A1);

		total = (unsigned)((int)x * (int)x) >> 8;

		energy_a = smooth(energy_a, notch_energy(total, ya));
		energy_b = smooth(energy_b, notch_energy(total, yc));
		energy = smooth(energy, total);
	}

	st->energy_a = (int)energy_a;
	st->energy_b = (int)energy_b;
	st->energy = (int)energy;

	if (energy < (unsigned)st->min_energy) {
		st->hold_a = 0;
		st->hold_b = 0;
		return DUAL_TONE_NOSIGNAL;
	}

	threshold = ((unsigned)st->ratio * energy) >> 8;

	if (energy_a > threshold) {
		/*
		 * A wins, so B's timer restarts.  On the confirmed branch A's
		 * restarts too, which re-arms the detector rather than
		 * latching -- a tone held for a second reports confirmed once
		 * every 160 ms, not continuously.
		 */
		if (st->hold_a < DUAL_TONE_HOLD) {
			st->hold_b = 0;
			return DUAL_TONE_A;
		}
		st->hold_a = 0;
		st->hold_b = 0;
		return DUAL_TONE_A_CONFIRMED;
	}

	if (energy_b > threshold) {
		if (st->hold_b < DUAL_TONE_HOLD) {
			st->hold_a = 0;
			return DUAL_TONE_B;
		}
		st->hold_b = 0;
		st->hold_a = 0;
		return DUAL_TONE_B_CONFIRMED;
	}

	st->hold_a = 0;
	st->hold_b = 0;
	return DUAL_TONE_OTHER;
}

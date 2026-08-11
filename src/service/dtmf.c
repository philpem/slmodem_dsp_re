/*
 * dtmf.c -- reconstructed from dsplibs.o Dtmf.c.
 *
 *   dtmf_test      .text 0x0ada10  412 bytes
 *   dtmf_detect    .text 0x0adc20  711 bytes
 *   dtmf_progress  .text 0x0adfe0  105 bytes
 *   dtmf_set_easy  .text 0x0ae050   17 bytes
 *
 * The TU's other three functions are not here.  `create_dtmf` is reached
 * from `detector_create` rather than from this closure; `check_for_valid`
 * and `check_for_valid_easy` are global but nothing in the object calls
 * them -- dtmf_detect carries both inline, and the two out-of-line copies
 * are dead.  Both readings are recorded in finding 1410.
 *
 * See include/dsplib/dtmf.h for the object and for why this is a bank of
 * notches rather than a bank of bandpasses.
 */

#include "dsplib/dtmf.h"
#include "dsplib/notch.h"

/*
 * The block length, in decimated (4 kHz) samples.  Europe accumulates five
 * more than the US does, which is 11.25 ms against 10.
 */
#define DTMF_BLOCK	40
#define DTMF_BLOCK_EUR	5

/*
 * Initial value of the running minimum.  The object materialises 0x7e967699
 * through the stack rather than loading it from .rodata; that bit pattern is
 * exactly 1e38f.
 */
#define DTMF_HUGE	1e38f

/*
 * Round an x87 value to `float`, and mean it.
 *
 * The object stores the bias notch's result back to its `float` stack slot
 * and reloads it -- `fstps 0x30(%esp)` at .text+0xade5e, `flds 0x30(%esp)`
 * at +0xadc72 -- because that slot is the incoming argument and the source
 * assigned to it.  `notch` returns an 80-bit value in st(0), so that store
 * ROUNDS, and everything downstream sees the rounded sample.
 *
 * A modern GCC at this tree's flags (-mfpmath=387, -fexcess-precision=fast,
 * deliberately no -ffloat-store) keeps the returned value at 80 bits through
 * a plain `x = notch(...)`, and the difference reaches `total` and every
 * notch state within a few samples -- 2,534 of 46,080 comparisons on the
 * European plan before this was forced.  Not a tolerance to widen.
 *
 * Only the bias assignment needs it.  The per-tone loop squares `notch`'s
 * result straight out of st(0) in the object too (`fmul %st(0),%st` with no
 * intervening store), so there both sides carry 80 bits and must.  This is
 * the same argument, and the same fix, as Resampler.cpp's `round32`.
 */
static float
round32(float v)
{
	volatile float r = v;

	return r;
}

/*
 * The block decision.
 *
 * `e[i]` is the energy that came OUT of notch i, so the tone that is present
 * is the one with the SMALLEST energy -- the search below is for a minimum,
 * and that is not a transcription slip.  `max_lo` / `max_hi` are the group
 * maxima, standing in for "what a tone that is absent looks like", and
 * `rest` is the energy left in the six notches that were not picked.
 *
 * Five ratio tests then have to agree before a digit is reported:
 *
 *   elo * 7.94 >= ehi     the two nulls are within about 9 dB of each other,
 *   ehi * 2.82 >= elo     which is the twist limit, asymmetric on purpose
 *   max_lo * 0.96 >= elo  the winner really is below its own group
 *   max_hi * 0.96 >= ehi
 *   rest * 1.36 >= elo + ehi   and the other six notches did not null out
 *
 * plus an upper bound on the same quantity, `rest * 0.79 > elo + ehi`, which
 * rejects a signal that the two picked notches barely touched.
 */
int
dtmf_test(const float *e, float total, short mode)
{
	float threshold;
	float min_lo, min_hi;
	float max_lo, max_hi;
	float sum, elo, ehi, rest, pair;
	short lo, hi;
	short i;
	int ok;

	threshold = (mode == DTMF_MODE_EUR) ? 0.0022f : 0.002f;

	/*
	 * `lo` and `hi` start at zero and are only moved by the search, so a
	 * group whose every entry is >= 1e38 reports index 0 for both.  That
	 * is the object's behaviour and it is preserved deliberately.
	 */
	lo = 0;
	hi = 0;
	min_lo = DTMF_HUGE;
	max_lo = 0.0f;
	for (i = 0; i <= 3; i++) {
		if (e[i] < min_lo) {
			min_lo = e[i];
			lo = i;
		}
		if (e[i] > max_lo)
			max_lo = e[i];
	}

	min_hi = DTMF_HUGE;
	max_hi = 0.0f;
	for (i = 0; i <= 3; i++) {
		if (e[i + 4] < min_hi) {
			min_hi = e[i + 4];
			hi = (short)(i + 4);
		}
		if (e[i + 4] > max_hi)
			max_hi = e[i + 4];
	}

	sum = 0.0f;
	for (i = 0; i <= 7; i++)
		sum += e[i];

	/*
	 * Re-read rather than reuse `min_lo` / `min_hi`: the object throws the
	 * running minima away and indexes the array again, which differs only
	 * in the all-huge case above but differs there.
	 */
	ehi = e[hi];
	elo = e[lo];
	rest = (sum - ehi) - elo;

	if (total < threshold)
		return DTMF_NO_DIGIT;

	rest = rest * (1.0f / 6.0f);
	pair = elo + ehi;

	ok = (elo * 7.94f >= ehi)
	    && (ehi * 2.82f >= elo)
	    && (max_lo * 0.96f >= elo)
	    && (max_hi * 0.96f >= ehi)
	    && (rest * 1.36f >= pair);

	if (rest * 0.79f > pair)
		return DTMF_NO_DIGIT;
	if (!ok)
		return DTMF_NO_DIGIT;

	return (short)(lo + hi * 4 - 16);
}

/*
 * check_for_valid, inlined.  The block just decided `d->hist[0]`; this asks
 * whether it is the second of exactly two, i.e. the same digit as the block
 * before it and different from the four before that.  Reporting on the
 * SECOND block is what makes a single stray block harmless.
 */
static int
check_for_valid(const short *h)
{
	if (h[0] != h[1])
		return 0;
	if (h[0] != h[2])
		return 0;
	if (h[0] == h[3])
		return 0;
	if (h[0] == h[4])
		return 0;
	if (h[0] == h[5])
		return 0;
	if (h[0] == h[6])
		return 0;
	return 1;
}

/* The same question over a shorter window: two the same, two before unlike. */
static int
check_for_valid_easy(const short *h)
{
	if (h[0] != h[1])
		return 0;
	if (h[0] == h[2])
		return 0;
	if (h[0] == h[3])
		return 0;
	return 1;
}

/*
 * One 8 kHz sample.
 *
 * The digit is reported when the tone STOPS: while `held` is set every block
 * returns -1, and the block in which dtmf_test first says -1 again hands
 * back the digit that was being held.  A caller polling this therefore sees
 * one report per keypress, at the end of it.
 */
int
dtmf_detect(float x, struct dtmf *d, short mode)
{
	int result = DTMF_NOT_YET;
	const float *coef;
	float *state;
	short phase;
	short block;
	short digit;
	short i;

	/* 8000 -> 4000: only every second sample goes any further. */
	phase = (short)(d->phase + 1);
	if (phase != 2) {
		d->phase = phase;
		return result;
	}
	d->phase = 0;

	if (mode == DTMF_MODE_EUR)
		x = round32(notch(x, d->bias_state, biascoef));

	d->total = x * x + d->total;

	state = &d->notch_state[0][0];
	coef = (mode == DTMF_MODE_EUR) ? eur_coef : us_coef;
	for (i = 0; i <= 7; i++) {
		float y = notch(x, state, coef);

		d->energy[i] = y * y + d->energy[i];
		state += 2;
		coef += 4;
	}

	d->count++;
	block = (short)(DTMF_BLOCK
			+ (mode == DTMF_MODE_EUR ? DTMF_BLOCK_EUR : 0));
	if (d->count != block)
		return result;

	for (i = 7; i > 0; i--)
		d->hist[i] = d->hist[i - 1];

	digit = (short)dtmf_test(d->energy, d->total, mode);
	d->hist[0] = digit;
	d->count = 0;

	if (d->held) {
		if (digit == DTMF_NO_DIGIT) {
			result = d->digit;
			d->held = 0;
			d->digit = DTMF_NO_DIGIT;
		} else {
			result = DTMF_NO_DIGIT;
		}
	} else if (digit == DTMF_NO_DIGIT) {
		result = DTMF_NO_DIGIT;
	} else {
		if (check_for_valid(d->hist)) {
			result = DTMF_NO_DIGIT;
			d->held = 1;
			d->digit = digit;
		}
		if (d->easy && check_for_valid_easy(d->hist)) {
			d->held = 1;
			d->digit = digit;
			result = DTMF_NO_DIGIT;
		}
	}

	for (i = 0; i <= 7; i++)
		d->energy[i] = 0.0f;
	d->total = 0.0f;

	return result;
}

/*
 * A buffer of 8 kHz samples, returning the last real digit in it.  -1 and -2
 * are both "nothing to report" and are filtered out here rather than at the
 * caller, so a block that spans the end of a keypress reports the digit even
 * though later samples in the same block said nothing.
 */
short
dtmf_progress(struct dtmf *d, const float *samples, short count, short mode)
{
	short result = DTMF_NO_DIGIT;
	int i;

	for (i = 0; i < count; i++) {
		short digit = (short)dtmf_detect(samples[i], d, mode);

		if (digit != DTMF_NOT_YET && digit != DTMF_NO_DIGIT)
			result = digit;
	}

	return result;
}

/* One store and no read-back; nothing in the object clears it again. */
void
dtmf_set_easy(struct dtmf *d)
{
	d->easy = 1;
}

/*
 * t_fpm_lmsupd.c -- differential test of the LMS coefficient update.
 *
 * The update is a read-modify-write over the whole coefficient array, so the
 * two things that can go wrong independently are the ARITHMETIC (the rounding
 * constant and the 18-place shift, which no small error changes) and the
 * PAIRING of coefficient index against history index, which only shows up
 * when the history is not symmetric and `widx` is not at either end.
 *
 * A canary past the end of each array catches a walk that writes one entry
 * too many, which the pairing check on its own would not.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm.h"

extern void ref_FPM_lmsupd(short *coeff, const short *hist, short widx,
			   short taps, short err);
extern void ref_FPM_lmsupd2(short *coeff, const short *hist, short widx,
			    short taps, short err, short mu);
extern void ref_FPM_block_update(short *coeff, short taps, const short *hist,
				 short widx, short hlen, const short *x,
				 short count, short step, short gain);

#define MAX	110

static short c_a[MAX + 4], c_b[MAX + 4], hist[MAX];

static unsigned seed;

static int
rnd(int range)
{
	seed = seed * 1103515245u + 12345u;
	return (int)((seed >> 13) % (unsigned)range);
}

static void
run(short taps, short widx, short err, long id)
{
	int k;

	for (k = 0; k < MAX; k++)
		hist[k] = (short)(rnd(65535) - 32768);
	for (k = 0; k < MAX + 4; k++)
		c_a[k] = c_b[k] = (short)(rnd(65535) - 32768);

	ref_FPM_lmsupd(c_b, hist, widx, taps, err);
	FPM_lmsupd(c_a, hist, widx, taps, err);

	for (k = 0; k < MAX + 4; k++)
		diff_eq_int("coeff[%ld]", c_a[k], c_b[k], k);
	(void)id;
}

/*
 * ---------------------------------------------------------------------------
 * FPM_lmsupd2 -- the same walk, correction formed in two rounded stages.
 *
 * The walk is already covered above, so what is new here is the arithmetic,
 * and specifically the `(short)` BETWEEN the two multiplies.  Fold the two
 * shifts into one `>> 22` and the function still runs, still converges and
 * differs only where the intermediate leaves 16 bits -- which ordinary small
 * `mu` never does.  `narrow_seen` counts the inputs where the narrowing
 * actually changed the value, and the guard at the end of main asserts it
 * fired: without that this test would pass against a version that has no cast
 * at all.
 */
static short l2_a[MAX + 4], l2_b[MAX + 4];
static int narrow_seen;

static void
run2(short taps, short widx, short err, short mu)
{
	int k;

	for (k = 0; k < MAX; k++)
		hist[k] = (short)(rnd(65535) - 32768);
	for (k = 0; k < MAX + 4; k++)
		l2_a[k] = l2_b[k] = (short)(rnd(65535) - 32768);

	for (k = 0; k < taps && k < MAX; k++) {
		int t = (hist[k] * mu + 0x10) >> 5;

		if (t != (short)t)
			narrow_seen++;
	}

	ref_FPM_lmsupd2(l2_b, hist, widx, taps, err, mu);
	FPM_lmsupd2(l2_a, hist, widx, taps, err, mu);

	for (k = 0; k < MAX + 4; k++)
		diff_eq_int("lmsupd2 coeff[%ld]", l2_a[k], l2_b[k], k);
}

/*
 * ---------------------------------------------------------------------------
 * FPM_block_update -- correlate a block against a circular history.
 *
 * Three properties are load-bearing and each has its own non-vacuity guard,
 * because every one of them is invisible on gentle inputs:
 *
 *   - the accumulator is a SHORT and truncates on every inner iteration, not
 *     an int narrowed at the end (`acc_trunc_seen`);
 *   - the circular wrap is ONE conditional add, so an index that has run below
 *     `-hlen` stays negative after it and reads below `hist` (`wrap_neg_seen`);
 *   - `gain * acc` is added to the coefficient and truncated.
 *
 * `hist` therefore points into the MIDDLE of a much larger array, so the
 * out-of-contract negative indices are ordinary reads of initialised memory on
 * both sides rather than undefined behaviour in the harness.  Both sides read
 * the same words, so the comparison is still exact; what it is comparing is
 * simply a wider domain than any caller would use.
 */
#define BTAPS	64
#define BHLEN	64
#define BPAD	1024

static short bhist_store[BPAD + BHLEN + BPAD];
static short bx[BTAPS];
static short bc_a[BTAPS + 4], bc_b[BTAPS + 4];
static int wrap_neg_seen, acc_trunc_seen;

static void
runb(short taps, short widx, short hlen, short count, short step, short gain)
{
	short *h = bhist_store + BPAD;
	int i, j, k;

	for (k = 0; k < BPAD + BHLEN + BPAD; k++)
		bhist_store[k] = (short)(rnd(65535) - 32768);
	for (k = 0; k < BTAPS; k++)
		bx[k] = (short)(rnd(65535) - 32768);
	for (k = 0; k < BTAPS + 4; k++)
		bc_a[k] = bc_b[k] = (short)(rnd(65535) - 32768);

	/* What did these inputs actually reach?  See the guards in main. */
	for (i = 0; i < taps; i++) {
		short pos = (short)(widx - i);
		short acc = 0;

		for (j = 0; j < count; j++) {
			short idx = pos < 0 ? (short)(pos + hlen) : pos;
			int wide;

			if (idx < 0)
				wrap_neg_seen++;
			wide = acc + bx[j] * h[idx];
			if (wide != (short)wide)
				acc_trunc_seen++;
			acc = (short)wide;
			pos = (short)(pos - step);
		}
	}

	ref_FPM_block_update(bc_b, taps, h, widx, hlen, bx, count, step, gain);
	FPM_block_update(bc_a, taps, h, widx, hlen, bx, count, step, gain);

	for (k = 0; k < BTAPS + 4; k++)
		diff_eq_int("block_update coeff[%ld]", bc_a[k], bc_b[k], k);
}

/*
 * ---------------------------------------------------------------------------
 */
int
main(void)
{
	int t, w, e;
	int rc = 0;

	seed = 4242u;
	diff_begin("FPM_lmsupd");

	/* V.32's own shape first: 103 taps, every write index. */
	for (w = 0; w < 103; w += 7)
		run(103, (short)w, (short)(rnd(30000) - 15000), w);

	/* Both ends of the walk, where one of the two loops does nothing. */
	run(103, 0, 1234, 200);
	run(103, 102, -1234, 201);
	run(103, -1, 4000, 202);

	/* Degenerate lengths. */
	run(1, 0, 9999, 300);
	run(2, 0, -9999, 301);
	run(2, 1, 32767, 302);

	/* An error of zero must leave the array alone, and the sign of the
	 * error is what the rounding constant biases. */
	run(103, 40, 0, 400);
	for (e = -32768; e < 32767; e += 6553)
		run(97, 33, (short)e, 500 + e);

	/* A sweep of lengths, so no tap count is special. */
	for (t = 1; t <= MAX; t += 9)
		for (w = -1; w < t; w += 5)
			run((short)t, (short)w, (short)(rnd(20000) - 10000),
			    600 + t * 200 + w);

	rc |= diff_end();


	/* --- FPM_lmsupd2 ------------------------------------------------ */

	diff_begin("FPM_lmsupd2");

	/* The same shapes the single-stage walk is driven with. */
	for (w = 0; w < 103; w += 7)
		run2(103, (short)w, (short)(rnd(30000) - 15000),
		     (short)(rnd(4000) - 2000));

	run2(103, 0, 1234, 300);
	run2(103, 102, -1234, -300);
	run2(103, -1, 4000, 77);

	run2(1, 0, 9999, 12);
	run2(2, 0, -9999, -12);
	run2(2, 1, 32767, 32767);

	/* Either multiplier at zero must leave the array alone. */
	run2(103, 40, 0, 5000);
	run2(103, 40, 5000, 0);

	/*
	 * Both multipliers swept over their whole range.  This is where the
	 * intermediate narrowing bites: `mu` beyond a few hundred pushes
	 * (hist * mu) >> 5 past 16 bits for most history words.
	 */
	for (e = -32768; e < 32767; e += 6553)
		run2(97, 33, (short)e, (short)(rnd(65535) - 32768));
	for (e = -32768; e < 32767; e += 6553)
		run2(97, 33, (short)(rnd(65535) - 32768), (short)e);

	/* Deliberately small `mu`, where the narrowing must NOT fire. */
	run2(97, 33, 20000, 1);
	run2(97, 33, -20000, -1);

	for (t = 1; t <= MAX; t += 9)
		for (w = -1; w < t; w += 5)
			run2((short)t, (short)w,
			     (short)(rnd(20000) - 10000),
			     (short)(rnd(20000) - 10000));

	rc |= diff_end();

	/*
	 * Non-vacuity: the cast between the two stages is the only thing
	 * separating this from a single `>> 22`, and an input set that never
	 * overflows the intermediate does not test it.
	 */
	diff_begin("FPM_lmsupd2 intermediate narrowing reached");
	diff_eq_int("narrowing fired (%ld times)", narrow_seen > 0, 1,
		    narrow_seen);
	rc |= diff_end();


	/* --- FPM_block_update ------------------------------------------- */

	diff_begin("FPM_block_update");

	/* Ordinary shapes: step 1, index inside the history. */
	runb(BTAPS, 40, BHLEN, 16, 1, 300);
	runb(BTAPS, 0, BHLEN, 16, 1, 300);
	runb(BTAPS, 63, BHLEN, 16, 1, -300);
	runb(1, 10, BHLEN, 8, 1, 1000);
	runb(BTAPS, 40, BHLEN, 1, 1, 1000);

	/* A zero-length block and a zero tap count are both no-ops. */
	runb(BTAPS, 40, BHLEN, 0, 1, 1000);
	runb(0, 40, BHLEN, 16, 1, 1000);

	/* A gain of zero must leave the coefficients alone. */
	runb(BTAPS, 40, BHLEN, 16, 1, 0);

	/* Decimating steps, which is what makes `pos` run away from zero. */
	for (t = 1; t <= 8; t++)
		runb(BTAPS, 40, BHLEN, 16, (short)t,
		     (short)(rnd(20000) - 10000));

	/* Every write index, so no starting phase is special. */
	for (w = 0; w < BHLEN; w += 5)
		runb(BTAPS, (short)w, BHLEN, 12, 3,
		     (short)(rnd(20000) - 10000));

	/*
	 * Deep enough that `pos` falls below -hlen and the single conditional
	 * add cannot bring it back: 40 - 63 - 8*63 = -527, against an hlen of
	 * 64.  This is out of contract and is exactly what the comment in
	 * src/dsp/fpm_lmsupd.c claims, so it is checked rather than asserted.
	 */
	runb(BTAPS, 40, BHLEN, BTAPS, 8, 4000);
	runb(BTAPS, 63, BHLEN, BTAPS, 8, -4000);

	/* A gain and history large enough to make the 16-bit accumulator
	 * wrap many times over. */
	runb(BTAPS, 40, BHLEN, BTAPS, 1, 32767);

	rc |= diff_end();

	diff_begin("FPM_block_update reached its edge cases");
	diff_eq_int("accumulator truncation fired (%ld times)",
		    acc_trunc_seen > 0, 1, acc_trunc_seen);
	diff_eq_int("index still negative after the wrap (%ld times)",
		    wrap_neg_seen > 0, 1, wrap_neg_seen);
	rc |= diff_end();

	return rc;
}

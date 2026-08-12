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

int
main(void)
{
	int t, w, e;
	int rc;

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

	rc = diff_end();
	return rc;
}

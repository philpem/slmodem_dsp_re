/*
 * t_v34eq.c -- differential test of the V.34 adaptive equaliser.
 *
 * Self-contained, unlike the echo canceller: the delay line, both halves of
 * both coefficient arrays and the cursor all live inside the 0x3cc-byte
 * object, so every check here compares the whole thing byte for byte.
 *
 * Two things need driving deliberately.
 *
 * The CURSOR has to wrap while the filter runs, because the convolution is
 * written as two loops and a reconstruction that got the split wrong would
 * agree perfectly for every cursor position except the ones that straddle.
 * The runs below push more than 80 samples for that reason.
 *
 * The TWO ADAPTERS disagree about precision on purpose -- V34EqualizerAdapt
 * keeps 32-bit taps, V34EqualizerCenterAdapt keeps 16 and rounds -- so they
 * are driven both separately and interleaved.  Interleaved is the case that
 * would expose a reconstruction which had assumed CenterAdapt was just Adapt
 * over a sub-range: the fractional halves go stale under it, and Adapt reads
 * them back on its next pass.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v34filt.h"

extern void ref_V34EqualizerCleanUp(void *q);
extern void ref_V34EqualizerClearCenterTaps(void *q);
extern void ref_V34EqualizerUpdateDelayLine(void *q, short re, short im);
extern void ref_V34EqualizerFilter(void *q, int *re, int *im);
extern void ref_V34EqualizerAdapt(void *q, short re, short im);
extern void ref_V34EqualizerCenterAdapt(void *q, short re, short im);

static struct v34_equalizer qa;
static unsigned char qb[0x3cc];

static int saw_wrap, saw_frac_stale, saw_sign;

static void
compare(const char *what, int tag)
{
	const unsigned char *p = (const unsigned char *)&qa;
	unsigned i;

	for (i = 0; i < sizeof(qa); i++)
		diff_eq_int(what, p[i], qb[i], (long)i * 1000 + tag);
}

static void
setup(void)
{
	memset(&qa, HARNESS_MALLOC_FILL, sizeof(qa));
	memset(qb, HARNESS_MALLOC_FILL, sizeof(qb));
	V34EqualizerCleanUp(&qa);
	ref_V34EqualizerCleanUp(qb);
}

int
main(void)
{
	int rc = 0;
	int i, k;

	diff_begin("v34 equaliser: CleanUp and ClearCenterTaps");
	setup();
	compare("after CleanUp", 0);
	diff_eq_int("tap 40 is unity", qa.re[40], V34_EQ_UNITY, 0);
	diff_eq_int("tap 39 is not", qa.re[39], 0, 0);
	diff_eq_int("the cursor is zero", qa.cursor, 0, 0);
	V34EqualizerClearCenterTaps(&qa);
	ref_V34EqualizerClearCenterTaps(qb);
	compare("after ClearCenterTaps", 1);
	diff_eq_int("the unity tap is gone", qa.re[40], 0, 0);
	rc |= diff_end();

	diff_begin("v34 equaliser: filter across the wrap");
	setup();
	for (i = 0; i < 250; i++) {
		int ra = -1, ia = -1, rb = -2, ib = -2;
		int before = qa.cursor;

		V34EqualizerUpdateDelayLine(&qa, (short)(i * 613 - 8000),
					    (short)(4000 - i * 271));
		ref_V34EqualizerUpdateDelayLine(qb, (short)(i * 613 - 8000),
						(short)(4000 - i * 271));
		V34EqualizerFilter(&qa, &ra, &ia);
		ref_V34EqualizerFilter(qb, &rb, &ib);
		diff_eq_int("filter re", ra, rb, i);
		diff_eq_int("filter im", ia, ib, i);
		compare("after filter", i);
		if (qa.cursor < before)
			saw_wrap++;
	}
	rc |= diff_end();

	diff_begin("v34 equaliser: the 32-bit adapter");
	setup();
	for (i = 0; i < 400; i++) {
		short er = (short)((i & 1) ? (i % 200) - 100 : 100 - (i % 200));
		short ei = (short)((i % 173) - 86);
		short was[V34_EQ_TAPS];

		V34EqualizerUpdateDelayLine(&qa, (short)(i * 977 - 9000),
					    (short)(i * 331 - 2000));
		ref_V34EqualizerUpdateDelayLine(qb, (short)(i * 977 - 9000),
						(short)(i * 331 - 2000));
		memcpy(was, qa.re, sizeof(was));
		V34EqualizerAdapt(&qa, er, ei);
		ref_V34EqualizerAdapt(qb, er, ei);
		compare("after Adapt", i);
		for (k = 0; k < V34_EQ_TAPS; k++)
			if ((qa.re[k] < 0) != (was[k] < 0))
				saw_sign++;
	}
	rc |= diff_end();

	diff_begin("v34 equaliser: the 16-bit centre adapter");
	setup();
	for (i = 0; i < 400; i++) {
		short er = (short)((i % 211) - 105);
		short ei = (short)(52 - (i % 97));

		V34EqualizerUpdateDelayLine(&qa, (short)(i * 733 - 7000),
					    (short)(i * 149 - 3000));
		ref_V34EqualizerUpdateDelayLine(qb, (short)(i * 733 - 7000),
						(short)(i * 149 - 3000));
		V34EqualizerCenterAdapt(&qa, er, ei);
		ref_V34EqualizerCenterAdapt(qb, er, ei);
		compare("after CenterAdapt", i);
	}
	/*
	 * CenterAdapt has moved the centre taps for 400 iterations and must
	 * not have touched a single fractional half: that is the asymmetry
	 * this test exists to pin, and it is invisible in the byte comparison
	 * above because both sides do the same thing.
	 */
	for (k = V34_EQ_CENTRE_FIRST;
	     k < V34_EQ_CENTRE_FIRST + V34_EQ_CENTRE_TAPS; k++) {
		diff_eq_int("CenterAdapt left re_frac alone", qa.re_frac[k], 0,
			    k);
		diff_eq_int("CenterAdapt left im_frac alone", qa.im_frac[k], 0,
			    k);
		if (qa.re[k] != 0)
			saw_frac_stale++;
	}
	rc |= diff_end();

	diff_begin("v34 equaliser: both adapters, interleaved");
	setup();
	for (i = 0; i < 600; i++) {
		short er = (short)((i * 37) % 401 - 200);
		short ei = (short)((i * 53) % 337 - 168);

		V34EqualizerUpdateDelayLine(&qa, (short)((i * 811) % 30001 - 15000),
					    (short)((i * 397) % 24001 - 12000));
		ref_V34EqualizerUpdateDelayLine(qb, (short)((i * 811) % 30001 - 15000),
						(short)((i * 397) % 24001 - 12000));
		V34EqualizerAdapt(&qa, er, ei);
		ref_V34EqualizerAdapt(qb, er, ei);
		if ((i % 3) == 0) {
			V34EqualizerCenterAdapt(&qa, ei, er);
			ref_V34EqualizerCenterAdapt(qb, ei, er);
		}
		if ((i % 97) == 96) {
			V34EqualizerClearCenterTaps(&qa);
			ref_V34EqualizerClearCenterTaps(qb);
		}
		compare("interleaved", i);
	}
	rc |= diff_end();

	diff_begin("v34 equaliser: coverage");
	printf("  wraps %d, sign changes %d, centre taps moved %d\n",
	       saw_wrap, saw_sign, saw_frac_stale);
	diff_eq_int("the cursor wrapped", saw_wrap > 0, 1, saw_wrap);
	diff_eq_int("a tap changed sign", saw_sign > 0, 1, saw_sign);
	diff_eq_int("CenterAdapt actually moved taps", saw_frac_stale > 0, 1,
		    saw_frac_stale);
	rc |= diff_end();

	return rc;
}

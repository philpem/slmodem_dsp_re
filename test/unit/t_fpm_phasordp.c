/*
 * t_fpm_phasordp.c -- differential test of the double-precision phase
 * accumulator, FPM_phasor_dp.
 *
 * WHY IT IS A BINARY OF ITS OWN rather than three more sections in
 * `t_fpm_phasor`: that test already sweeps 65536 phases across fourteen
 * increments twice over, and this one has a SECOND 16-bit field to sweep with
 * them.  The product is the whole cost, so the two sweeps are kept apart and
 * each says what its own denominator is.
 *
 * WHAT IS EXHAUSTIVE AND WHAT IS NOT, stated because a sampled sweep read as
 * an exhaustive one is how a hole gets called coverage:
 *
 *   - `phase` is swept over all 65536 values, TWICE: once with the fractional
 *     half at rest, and once with it carrying.  Neither output depends on
 *     `inc`, so this is the exhaustive part and it is exact.
 *   - `frac_phase` x `frac_inc` is 2^32 pairs and is NOT swept exhaustively.
 *     What is swept is the CARRY BOUNDARY -- every pair whose halved sum lands
 *     within a few units of 0x8000, of 0 and of -0x8000 -- because that is
 *     where `phase` moves and where the object's `sar $1` and its `cwtl` can
 *     be told apart from the things they are not.
 *   - the wrap at 0x3fffffff is driven directly, from both sides and at the
 *     boundary itself.
 *
 * The whole object is compared with `diff_eq_obj` on every trial, so a write
 * to a field this test does not name is still a failure.
 *
 * Finding F8168 is the decode; D950 is the unexplained halving; D392 covers
 * the unmasked quadrant, which this function shares with `FPM_phasor` and
 * which is why the sweeps run to 0x10000 rather than to 0x8000.
 */

#include <stdio.h>

#include "harness.h"
#include "dsplib/fpm_phasor.h"

extern void ref_FPM_phasor_dp(struct fpm_phasor_dp *p);

/*
 * Increments spanning DC, real tones and the extremes.
 *
 * THE TOP HALF IS NOT DECORATION.  `inc` is an `unsigned short` field the
 * object loads with `movswl` (0x0a9480), so a value of 0x8000 or more is a
 * NEGATIVE increment and the accumulator runs backwards.  `t_fpm_phasor`'s
 * increment list stops at 0x7fff, and with it the mutation "the phase
 * increment is read unsigned" survives: an unsigned read differs from the
 * object's only above 0x7fff, and nothing below it can tell the two apart.
 * The last three entries are what separates them.
 */
static const int increments[] = {
	0, 1, 2, 7, 0x100, 0x1000,
	4506,	/* ~1100 Hz at 8 kHz: Bell 103 originate mark  */
	8601,	/* ~2100 Hz: answer tone                       */
	0x4000, 0x7fff,
	0x8000, 0xc000, 0xffff,		/* read signed: -32768, -16384, -1 */
};

/* Fractional increments, including both signs and both extremes. */
static const int frac_incs[] = {
	0, 1, -1, 2, 3, 0x100, -0x100, 0x4000, -0x4000,
	0x7fff, -0x7fff, -0x8000,
};

static void
seed(struct fpm_phasor_dp *p, int phase, int inc, int fp, int fi)
{
	p->phase = (unsigned short)phase;
	p->cos = (short)0x5a5a;
	p->sin = (short)0xa5a5;
	p->inc = (unsigned short)inc;
	p->frac_phase = (short)fp;
	p->frac_inc = (short)fi;
}

/* One trial: seed both sides identically, run both, compare the whole object. */
static void
trial(int phase, int inc, int fp, int fi, long tag)
{
	struct fpm_phasor_dp a, b;

	seed(&a, phase, inc, fp, fi);
	seed(&b, phase, inc, fp, fi);
	ref_FPM_phasor_dp(&a);
	FPM_phasor_dp(&b);
	diff_eq_obj("whole object", struct fpm_phasor_dp, &b, &a, tag);
}

int
main(void)
{
	int failed = 0;
	unsigned k, j;
	int i;
	long trials = 0;
	int moved_up = 0, moved_down = 0, wrapped = 0;

	/*
	 * SWEEP 1 -- every phase, fractional half at rest.
	 *
	 * With `frac_phase` and `frac_inc` both zero the advance reduces to
	 * `FPM_phasor`'s, so this section is the part that says the shared
	 * lookup really is shared: sine, cosine and the coarse phase must
	 * agree with the object over the whole 16-bit domain including the
	 * 32768 out-of-domain values D392 covers.
	 */
	diff_begin("FPM_phasor_dp exhaustive over phase, fraction at rest");
	for (k = 0; k < sizeof(increments) / sizeof(increments[0]); k++)
		for (i = 0; i < 0x10000; i++, trials++)
			trial(i, increments[k], 0, 0, i);
	failed |= diff_end();

	/*
	 * SWEEP 2 -- every phase again, with the fractional half carrying.
	 *
	 * 0x7fff + 0x7fff halves to 0x7fff, which is one short of a carry, and
	 * the pair below carries on every call.  Run over the whole phase
	 * domain because the carry is added AFTER the shift, so it interacts
	 * with the wrap at every phase and not only near the top.
	 */
	diff_begin("FPM_phasor_dp exhaustive over phase, fraction carrying");
	for (i = 0; i < 0x10000; i++, trials++)
		trial(i, 1, 0x7fff, 0x7fff, i);
	for (i = 0; i < 0x10000; i++, trials++)
		trial(i, 0, (short)0x8000, (short)0x8000, i);
	failed |= diff_end();

	/*
	 * SWEEP 3 -- the fractional pair, at the carry boundary.
	 *
	 * Not exhaustive and says so: 2^32 pairs is the full product.  What is
	 * driven is every `frac_inc` of interest against a `frac_phase` walked
	 * through the neighbourhood of each place the halved sum can cross --
	 * -0x8000, 0 and 0x7fff -- which is where the `>> 1` and the narrowing
	 * are observable at all.
	 */
	diff_begin("FPM_phasor_dp fractional carry boundary");
	{
		static const int centres[] = { -0x8000, -1, 0, 1, 0x7fff };

		for (k = 0; k < sizeof(frac_incs) / sizeof(frac_incs[0]); k++)
			for (j = 0; j < sizeof(centres) / sizeof(centres[0]);
			     j++)
				for (i = -8; i <= 8; i++) {
					int fp = centres[j] + i;

					if (fp < -0x8000 || fp > 0x7fff)
						continue;
					trials++;
					trial(0x1234, 3, fp, frac_incs[k],
					      (long)fp);
					trials++;
					trial(0x7ffe, 3, fp, frac_incs[k],
					      (long)fp);
					trials++;
					trial(0xfffe, 3, fp, frac_incs[k],
					      (long)fp);
				}
	}
	failed |= diff_end();

	/*
	 * SWEEP 4 -- the wrap at 0x3fffffff, driven from both sides.
	 *
	 * `acc` is `((phase + inc) << 15) + ((frac_phase + frac_inc) >> 1)`, so
	 * a chosen (phase, inc) puts the coarse part at a chosen multiple of
	 * 0x8000 and the fractional pair walks `acc` across the threshold one
	 * unit at a time.  0x7fff << 15 is 0x3fff8000, so a halved fraction of
	 * 0x7fff .. 0x8000 straddles it exactly.
	 */
	diff_begin("FPM_phasor_dp accumulator wrap");
	for (i = -4; i <= 4; i++) {
		/* halved sum = 0x7ffc .. 0x8004, i.e. acc = 0x3ffffffc .. */
		int fp = 0x7ff8 + 2 * i;

		trials++;
		trial(0x7fff, 0, fp, 0x7fff, (long)i);
		trials++;
		trial(0x7ffe, 1, fp, 0x7fff, (long)i);
		trials++;
		trial(0x4000, 0x3fff, fp, 0x7fff, (long)i);
	}
	failed |= diff_end();

	/*
	 * SWEEP 5 -- the NEGATIVE side of the accumulator, which the wrap does
	 * not touch and the narrowing does.
	 *
	 * `phase` is read as a signed short, so `phase + inc` runs to -65536
	 * and `acc >> 15` runs outside the 16-bit range in both directions.
	 * The object stores the TRUNCATED quotient and then subtracts what it
	 * stored, so a remainder computed against the untruncated quotient
	 * differs here and nowhere else.
	 */
	diff_begin("FPM_phasor_dp out-of-range quotient");
	for (k = 0; k < sizeof(increments) / sizeof(increments[0]); k++) {
		static const int phases[] = {
			0x8000, 0x8001, 0xc000, 0xfffe, 0xffff,
			0x7fff, 0x7ffe, 0x4000, 0, 1,
		};

		for (j = 0; j < sizeof(phases) / sizeof(phases[0]); j++)
			for (i = 0; i < 3; i++) {
				static const int fp[] = { 0, 0x7fff, -0x8000 };

				trials++;
				trial(phases[j], increments[k], fp[i],
				      0x7fff, (long)phases[j]);
			}
	}
	failed |= diff_end();

	/*
	 * SWEEP 6 -- a run of calls, so the fractional phase is FED BACK.
	 *
	 * Every section above is one call from a seeded state, which cannot
	 * tell a written-back `frac_phase` from one the caller happened to
	 * supply.  This one runs the accumulator forward and lets it carry on
	 * its own; the increment is chosen so the fraction accumulates slowly
	 * and the coarse phase steps only every few hundred calls.
	 */
	diff_begin("FPM_phasor_dp run, fractional phase fed back");
	{
		struct fpm_phasor_dp a, b;
		int n;

		seed(&a, 0, 4506, 0, 0x0123);
		seed(&b, 0, 4506, 0, 0x0123);
		for (n = 0; n < 20000; n++, trials++) {
			int before = (short)a.phase;

			ref_FPM_phasor_dp(&a);
			FPM_phasor_dp(&b);
			diff_eq_obj("whole object", struct fpm_phasor_dp,
				    &b, &a, n);
			if ((short)a.phase > before)
				moved_up++;
			else if ((short)a.phase < before)
				moved_down++;
		}
	}
	failed |= diff_end();

	/*
	 * NON-VACUITY, and it is about SEPARATION rather than reachability
	 * (finding F8163).  A count that only says a branch was entered proves
	 * nothing about whether the test can tell it from its alternative.
	 * These three say the run above actually moved the coarse phase in
	 * both directions -- so the wrap and the carry are both exercised by
	 * observed OUTPUT and not merely by a path taken.
	 */
	{
		struct fpm_phasor_dp w;
		int n;

		seed(&w, 0x7fff, 1, 0x7fff, 0x7fff);
		for (n = 0; n < 4; n++) {
			short prev = (short)w.phase;

			FPM_phasor_dp(&w);
			if ((short)w.phase < prev)
				wrapped++;
		}
	}
	printf("  %ld differential trials; fed-back run moved the coarse "
	       "phase up %d times and down %d; %d wrap(s) observed\n",
	       trials, moved_up, moved_down, wrapped);

	diff_begin("non-vacuity");
	diff_eq_int("the fed-back run advanced the coarse phase (%ld times)",
		    moved_up > 0, 1, moved_up);
	diff_eq_int("the fed-back run wrapped it back down (%ld times)",
		    moved_down > 0, 1, moved_down);
	diff_eq_int("the accumulator wrap was reached (%ld times)",
		    wrapped > 0, 1, wrapped);
	diff_eq_int("trial count (%ld)", trials > 700000, 1, trials);
	failed |= diff_end();

	return failed;
}

/*
 * t_mtkphasor.c -- the differential test for MTK_phasor (blob 0xb0690, 271
 * bytes), the object's only reader of the MTK sine tables.
 *
 * Everything about this function is floating point, so every comparison
 * here is on the BIT PATTERN and not on a tolerance: the four fields of the
 * object go through `fbits` before they are compared. A one-ULP difference
 * in the interpolation is a difference.
 *
 * WHAT THE SWEEP HAS TO COVER, and it is not "some angles".
 *
 *   - both arms of the reduction, so the phase set includes negatives; the
 *     object wraps a negative angle by ADDING the double 2*pi to a value it
 *     reduced against the float one, which is why D993 exists;
 *   - both arms of the quarter-wave fold, `n & 0x100`, which is the half of
 *     the table read backwards with the fraction reversed;
 *   - all four quadrants, because the sign comes from a four-entry vector
 *     indexed by `n >> 8` and three of the four entries are negative in at
 *     least one of the pair;
 *   - both arms of the advance's wrap at pi;
 *   - the exact table stations, k * pi / 512, where `frac` is 0 and the
 *     interpolation degenerates -- those are the values a wrong index shows
 *     up at most clearly -- and the midpoints between them, where `frac` is
 *     furthest from both ends.
 *
 * Each of those is COUNTED FROM THE RUN and asserted at the end (F134). The
 * classifier that does the counting mirrors the object's own arithmetic; it
 * is a coverage counter and never an oracle, and if it were wrong the
 * assertions would report a case as unseen rather than pass quietly.
 *
 * THE ONE INPUT RANGE THAT IS NOT DRIVEN, and it is deviation D993: a phase
 * in [-6.3573431e-08, 0). The reduction leaves such a value alone (its
 * magnitude is below the modulus), the correction adds the DOUBLE
 * 6.28318530718 to it, and the sum rounds UP to the float 2*pi -- which
 * scales to 1024.0000038, so `n >> 8` is 4 and the object indexes one past
 * the end of both four-entry sign vectors. Driving it would compare our
 * out-of-bounds read against the blob's, which are different memory, so the
 * sweep stops at the two floats either side of that window and asserts it
 * stopped there.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/fdspkrnl.h"
#include "dsplib/mtk.h"

extern void ref_MTK_phasor(struct mtk_phasor *p);

/* The object's own three constants, for the coverage classifier only. */
#define MTKP_TWOPI_F	6.28318530718f
#define MTKP_TWOPI_D	6.28318530718
#define MTKP_PI_D	3.141592653589793
#define MTKP_K		162.97466172610083

/* The unsafe window's edge: see the header and D993. */
#define MTKP_UNSAFE_MAG	6.357343096397017e-08

static unsigned long
fbits(float f)
{
	unsigned long u = 0;

	memcpy(&u, &f, 4);
	return u;
}

static float
frombits(unsigned long u)
{
	unsigned int t = (unsigned int)u;
	float f;

	memcpy(&f, &t, 4);
	return f;
}

/* Coverage counters, filled by the classifier below. */
static int seen_quadrant[5];
static int seen_fold[2];
static int seen_negative[2];
static int seen_advance_wrap[2];
static int skipped_unsafe;
static int runs_into_window;
static int runs_complete;

/*
 * A MIRROR OF THE OBJECT'S ARITHMETIC, used only to say which case an input
 * exercised.  It decides nothing: every value it classifies is compared
 * against the blob regardless.
 */
static int
classify(float phase, float step)
{
	float x = fmodf(phase, MTKP_TWOPI_F);
	float y;
	float s;
	int n, q;

	if (x < 0.0f) {
		seen_negative[1] = 1;
		x = x + MTKP_TWOPI_D;
	} else {
		seen_negative[0] = 1;
	}
	y = x * MTKP_K;
	n = (short)y;
	q = n >> 8;
	if (q < 0 || q > 4)
		return 0;
	seen_quadrant[q] = 1;
	seen_fold[(n & 0x100) ? 1 : 0] = 1;
	s = x + step;
	seen_advance_wrap[s >= MTKP_PI_D ? 1 : 0] = 1;
	return q <= 3;
}

/*
 * Is this phase inside D993's window?  The test refuses to drive it, and
 * counts the refusals so a sweep that silently stopped covering the edge is
 * visible.
 */
static int
unsafe_phase(float phase)
{
	float x = fmodf(phase, MTKP_TWOPI_F);

	if (x < 0.0f && -x <= MTKP_UNSAFE_MAG) {
		skipped_unsafe++;
		return 1;
	}
	return 0;
}

static void
run_pair(long tag, float phase, float step)
{
	struct mtk_phasor a, b;

	if (unsafe_phase(phase))
		return;
	(void)classify(phase, step);

	memset(&a, 0x5a, sizeof(a));
	memset(&b, 0x5a, sizeof(b));
	a.phase = b.phase = phase;
	a.step = b.step = step;

	ref_MTK_phasor(&a);
	MTK_phasor(&b);

	diff_eq_int("phase bits %ld", (long)fbits(b.phase),
		    (long)fbits(a.phase), tag);
	diff_eq_int("cosine bits %ld", (long)fbits(b.cosine),
		    (long)fbits(a.cosine), tag);
	diff_eq_int("sine bits %ld", (long)fbits(b.sine),
		    (long)fbits(a.sine), tag);
	diff_eq_int("step untouched %ld", (long)fbits(b.step),
		    (long)fbits(a.step), tag);
	diff_eq_int("step is the input %ld", (long)fbits(b.step),
		    (long)fbits(step), tag);
}

int
main(void)
{
	int failed = 0;
	int i, k;

	/*
	 * SECTION 1 -- the exact table stations and their midpoints.  At a
	 * station `frac` is zero and the interpolation returns a table entry
	 * whole, so a wrong index shows here and nowhere else so plainly;
	 * at a midpoint `frac` is furthest from both ends.
	 */
	diff_begin("MTK_phasor stations");
	{
		for (k = 0; k <= 1024; k++) {
			float ph = (float)((double)k * MTKP_PI_D / 512.0);

			run_pair((long)k, ph, 0.0f);
			run_pair(2000 + (long)k,
				 (float)(((double)k + 0.5) * MTKP_PI_D
					 / 512.0), 0.0f);
			run_pair(4000 + (long)k, -ph, 0.0f);
		}
	}
	failed |= diff_end();

	/*
	 * SECTION 2 -- a dense sweep of the whole reduced range and beyond
	 * it, so the reduction itself is exercised rather than assumed, with
	 * a spread of steps that puts the advance either side of pi.
	 */
	diff_begin("MTK_phasor sweep");
	{
		static const float steps[] = {
			0.0f, 1e-7f, 0.0007853982f, 0.1f, 1.0f, 3.0f, 3.2f,
			6.2f, 12.0f, -0.1f, -3.0f, -7.0f
		};
		static const int nstep = (int)(sizeof(steps)
					       / sizeof(steps[0]));
		int si;

		for (i = -400; i <= 400; i++)
		for (si = 0; si < nstep; si++) {
			float ph = (float)((double)i * 0.0201f);

			run_pair((long)((i + 400) * 100 + si), ph,
				 steps[si]);
		}
		/* well outside one turn, so `fprem` has real work to do */
		for (i = -50; i <= 50; i++)
			for (si = 0; si < nstep; si++)
				run_pair(200000 + (long)((i + 50) * 100 + si),
					 (float)((double)i * 61.0),
					 steps[si]);
	}
	failed |= diff_end();

	/*
	 * SECTION 3 -- the edges.  Zero and both signed zeros, the float
	 * either side of D993's window, the largest float below the float
	 * 2*pi, the float 2*pi itself, and pi either side.
	 */
	diff_begin("MTK_phasor edges");
	{
		static const unsigned long edge_bits[] = {
			0x00000000UL,	/* +0.0f                            */
			0x80000000UL,	/* -0.0f: fmodf keeps the sign, and
					 *        -0.0 < 0.0 is FALSE       */
			0x00000001UL,	/* the smallest positive subnormal  */
			0x40c90fdbUL,	/* the float 2*pi, the modulus      */
			0x40c90fdaUL,	/* one below it                     */
			0xc0c90fdbUL,	/* its negative                     */
			0x40490fdbUL,	/* the float pi                     */
			0xc0490fdbUL,
			0x3f800000UL,	/* 1.0f                             */
			0x4b000000UL	/* 2^23, far outside one turn       */
		};
		static const int nedge = (int)(sizeof(edge_bits)
					       / sizeof(edge_bits[0]));
		int ei;

		/*
		 * The two floats that bracket D993's window, driven from
		 * OUTSIDE it on both sides.  The first is the largest
		 * magnitude the window covers, so it must be refused; the
		 * second is one float further out and must be driven.
		 */
		float just_in = -(float)MTKP_UNSAFE_MAG;
		float just_out = -frombits(fbits((float)MTKP_UNSAFE_MAG) + 1);
		int before = skipped_unsafe;

		run_pair(900, just_in, 0.0f);
		diff_eq_int("the window is refused",
			    skipped_unsafe - before, 1, 0);
		before = skipped_unsafe;
		run_pair(901, just_out, 0.0f);
		diff_eq_int("one float further out is driven",
			    skipped_unsafe - before, 0, 0);

		for (ei = 0; ei < nedge; ei++)
			for (k = 0; k < 3; k++)
				run_pair(1000 + (long)(ei * 10 + k),
					 frombits(edge_bits[ei]),
					 k == 0 ? 0.0f
						: (k == 1 ? 0.1f : -0.1f));
	}
	failed |= diff_end();

	/*
	 * SECTION 4 -- the oscillator run.  The single-call sweep cannot see
	 * a phase that drifts, because it plants the phase every time; this
	 * feeds each side its OWN previous output for eight thousand samples
	 * at a few different steps, which is what TONE_generate does.
	 */
	diff_begin("MTK_phasor free running");
	{
		static const float steps[] = {
			0.7853981634f,		/* 1 kHz at 8 kHz         */
			0.6283185307f,		/* 800 Hz                 */
			0.0007853982f,		/* very slow              */
			2.9f, 3.15f, -0.7853981634f
		};
		static const int nstep = (int)(sizeof(steps)
					       / sizeof(steps[0]));
		int si;

		for (si = 0; si < nstep; si++) {
			struct mtk_phasor a, b;
			int stopped = 0;

			memset(&a, 0, sizeof(a));
			memset(&b, 0, sizeof(b));
			a.phase = b.phase = 0.0f;
			a.step = b.step = steps[si];

			for (i = 0; i < 8000; i++) {
				if (fbits(a.phase) != fbits(b.phase)) {
					stopped = 2;	/* the sides parted */
					break;
				}
				if (unsafe_phase(b.phase)) {
					stopped = 1;	/* D993's window */
					break;
				}
				(void)classify(b.phase, b.step);
				ref_MTK_phasor(&a);
				MTK_phasor(&b);
				diff_eq_int("run phase %ld",
					    (long)fbits(b.phase),
					    (long)fbits(a.phase),
					    (long)(si * 10000 + i));
				diff_eq_int("run cosine %ld",
					    (long)fbits(b.cosine),
					    (long)fbits(a.cosine),
					    (long)(si * 10000 + i));
				diff_eq_int("run sine %ld",
					    (long)fbits(b.sine),
					    (long)fbits(a.sine),
					    (long)(si * 10000 + i));
			}
			/*
			 * TWO REASONS TO STOP, AND ONLY ONE OF THEM IS
			 * ALLOWED.  `2` is the sides having parted, which is
			 * a defect and must never happen.  `1` is the run
			 * walking into D993's window, which is the deviation
			 * doing what the deviation says: a free-running
			 * oscillator DOES reach it, which is the reachability
			 * evidence a single-call sweep cannot give.  It is
			 * counted per run and reported below rather than
			 * being quietly tolerated.
			 */
			diff_eq_int("the sides never parted %ld",
				    stopped == 2, 0, (long)si);
			if (stopped == 1)
				runs_into_window++;
			else if (stopped == 0)
				runs_complete++;
		}
	}
	failed |= diff_end();

	/*
	 * SECTION 5 -- coverage, asserted from the run (F134).  Without this
	 * a sweep that quietly stopped reaching a quadrant would report a
	 * clean pass over three of the four sign entries.
	 */
	diff_begin("MTK_phasor coverage");
	{
		for (k = 0; k <= 3; k++)
			diff_eq_int("quadrant seen %ld", seen_quadrant[k], 1,
				    (long)k);
		diff_eq_int("the forward half of the fold ran",
			    seen_fold[0], 1, 0);
		diff_eq_int("the reversed half of the fold ran",
			    seen_fold[1], 1, 0);
		diff_eq_int("a non-negative reduction ran",
			    seen_negative[0], 1, 0);
		diff_eq_int("a negative reduction ran",
			    seen_negative[1], 1, 0);
		diff_eq_int("the advance stayed below pi",
			    seen_advance_wrap[0], 1, 0);
		diff_eq_int("the advance wrapped at pi",
			    seen_advance_wrap[1], 1, 0);
		/* and D993's window was met, refused and counted */
		diff_eq_int("the unsafe window was reached at all",
			    skipped_unsafe > 0, 1, 0);
		/*
		 * Most free runs must get all the way to eight thousand
		 * samples -- otherwise "the sides never parted" is passing
		 * on runs that barely started.  The rest are D993's window,
		 * and that at least one run reaches it from a plain
		 * oscillator is the reachability the deviation records as
		 * unmeasured from a real caller.
		 */
		diff_eq_int("most free runs completed", runs_complete >= 4, 1,
			    0);
		diff_eq_int("a free run reached D993's window",
			    runs_into_window >= 1, 1, 0);
	}
	failed |= diff_end();

	return failed;
}

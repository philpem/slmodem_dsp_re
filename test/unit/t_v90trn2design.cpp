/*
 * t_v90trn2design.cpp -- differential test of the three small
 * `V90TRN2Designer` members: `setNofUcodesInTrn2`, `setTrn2DummyConstel` and
 * `maxK`.
 *
 * The constructor/destructor pair is tested in `t_v90designers.cpp`; this file
 * is the working half of the class and every case here drives a value the
 * object's OWN INSTRUCTIONS distinguish, rather than a value that merely
 * exercises a line.
 *
 * WHAT EACH SWEEP SEPARATES, and the mutation that has to make it red:
 *
 *   setNofUcodesInTrn2  the argument is compared `cmpw`, so 0x10000 -- a
 *                       non-zero `int` whose low sixteen bits are zero -- must
 *                       behave like 0 and not like 1.  Widening the compare to
 *                       `!= 0` on an `int` is the mutation, and only that trial
 *                       catches it.
 *
 *   setTrn2DummyConstel the descending fill wraps: 78 entries take the value
 *                       down to 1, 79 to 0 and 80 to 255, so a count at or
 *                       above 79 is what separates `unsigned char` from `int`
 *                       in the running value.  The count is also driven at 0
 *                       (write nothing) and at 128 (fill the row exactly).
 *
 *   maxK                three independent things, and each needs its own input:
 *                         * the 1e-6 guard only changes the answer when the
 *                           product is an EXACT POWER OF TWO, because that is
 *                           the only place a truncated logarithm lands one
 *                           short.  Dropping it must turn a power-of-two trial
 *                           red and nothing else.
 *                         * the round-toward-zero control word only shows up
 *                           where log2(product) has a fractional part at or
 *                           above 0.5 -- product 3 is 1.585 and truncates to 1
 *                           where rounding to nearest gives 2.
 *                         * the `product == 0` arm needs a zero length.
 *
 * The two sides are called by symbol through `asm()` labels, ours by the
 * mangled name and the blob's by the `ref_` alias the harness builds, which is
 * the convention `t_v90designers.cpp` explains.  Plain cdecl with `this` as the
 * first stack argument (finding 215).
 */

#include <string.h>

#include "harness.h"

#include "dsplib/V90TRN2Designer.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Parameters.h"

extern "C" {

void our_trn2_setnof(void *, int)
	asm("_ZN15V90TRN2Designer18setNofUcodesInTrn2Es");
void ref_trn2_setnof(void *, int)
	asm("ref__ZN15V90TRN2Designer18setNofUcodesInTrn2Es");

void our_trn2_dummy(void *, void *)
	asm("_ZN15V90TRN2Designer19setTrn2DummyConstelEP16V90MappingParams");
void ref_trn2_dummy(void *, void *)
	asm("ref__ZN15V90TRN2Designer19setTrn2DummyConstelEP16V90MappingParams");

int our_trn2_maxk(void *, void *)
	asm("_ZN15V90TRN2Designer4maxKEP16V90MappingParams");
int ref_trn2_maxk(void *, void *)
	asm("ref__ZN15V90TRN2Designer4maxKEP16V90MappingParams");

}

/*
 * The designer is eight bytes and holds two pointers; `power` is never read by
 * any of the three, and `params` is read by two of them.  Both sides are handed
 * the SAME `params` block for the read-only cases and their own for the case
 * that writes to it.
 */
#define GUARD	64

static unsigned char designer[8 + GUARD] __attribute__((aligned(8)));

static unsigned char paramsOurs[0x558 + GUARD] __attribute__((aligned(8)));
static unsigned char paramsTheirs[0x558 + GUARD] __attribute__((aligned(8)));
static unsigned char paramsBefore[0x558 + GUARD];

#define MP_BYTES	((int)sizeof(V90MappingParams))

static unsigned char mpOurs[MP_BYTES + GUARD] __attribute__((aligned(8)));
static unsigned char mpTheirs[MP_BYTES + GUARD] __attribute__((aligned(8)));
static unsigned char mpBefore[MP_BYTES + GUARD];

static unsigned lfsr;

static unsigned char
next_byte(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)(lfsr >> 3);
}

/*
 * Neither side is ever zeroed: both blocks get the same varied fill and the
 * seed moves with the trial, so "equal afterwards" is a statement about varied
 * content rather than about zeros agreeing with themselves (findings 223, 224).
 */
static void
fill_pair(unsigned char *a, unsigned char *b, int n, int trial)
{
	int i;

	lfsr = 0x51a3u + 0x9e37u * (unsigned)trial + 1u;
	for (i = 0; i < n; i++) {
		unsigned char v = next_byte();

		a[i] = v;
		b[i] = v;
	}
}

static void
set_designer(void *params)
{
	int i;

	for (i = 8; i < 8 + GUARD; i++)
		designer[i] = (unsigned char)(0x5a + i);
	((V90TRN2Designer *)designer)->params = (V90Parameters *)params;
	((V90TRN2Designer *)designer)->power = 0;
}

/* ------------------------------------------------------------------ */

/*
 * `setNofUcodesInTrn2` -- the argument is a FLAG and it is read as a short.
 *
 * The sweep drives 0x10000 and 0x1ffff0000 truncations deliberately: an `int`
 * compare would take them for "on" and a `cmpw` does not.  It also drives a
 * value whose low sixteen bits are non-zero but whose SIGN as a short is
 * negative (0x8001), because `!= 0` and `> 0` are different tests and only one
 * of them is the object's.
 */
static int
run_setnof(void)
{
	static const int args[] = {
		0, 1, -1, 2, 0x7fff, 0x8000, 0x8001, 0xffff,
		0x10000, 0x10001, 0x7fff0000, 0x0badbeef
	};
	static const int nargs = (int)(sizeof(args) / sizeof(args[0]));
	int trial, moved = 0, unmoved = 0;

	diff_begin("V90TRN2Designer::setNofUcodesInTrn2");

	for (trial = 0; trial < nargs * 4; trial++) {
		int arg = args[trial % nargs];
		V90Parameters *po = (V90Parameters *)paramsOurs;
		V90Parameters *pt = (V90Parameters *)paramsTheirs;

		fill_pair(paramsOurs, paramsTheirs, 0x558 + GUARD, trial);
		memcpy(paramsBefore, paramsOurs, sizeof(paramsBefore));

		/*
		 * The two fields the member moves between are seeded to
		 * DIFFERENT values, so "it copied +0x080 to +0x078" is
		 * distinguishable from "it left +0x078 alone".
		 */
		po->nofUcodesInTrn2 = pt->nofUcodesInTrn2 = 0x11223344;
		po->unnamed_080 = pt->unnamed_080 = 0x55667788 + trial;
		memcpy(paramsBefore, paramsOurs, sizeof(paramsBefore));

		set_designer(paramsOurs);
		our_trn2_setnof(designer, arg);
		set_designer(paramsTheirs);
		ref_trn2_setnof(designer, arg);

		diff_eq_int("params equal after setNofUcodesInTrn2(%#lx)",
			    memcmp(paramsOurs, paramsTheirs,
				   sizeof(paramsOurs)) == 0, 1, arg);
		diff_eq_int("nothing but +0x078 moved, arg %#lx",
			    (memcmp(paramsOurs, paramsBefore, 0x78) == 0 &&
			     memcmp(paramsOurs + 0x7c, paramsBefore + 0x7c,
				    sizeof(paramsOurs) - 0x7c) == 0), 1, arg);

		if ((short)arg != 0) {
			diff_eq_int("arg %#lx is on, so +0x078 took +0x080",
				    po->nofUcodesInTrn2, po->unnamed_080, arg);
			moved++;
		} else {
			diff_eq_int("arg %#lx is off, so +0x078 is untouched",
				    po->nofUcodesInTrn2, 0x11223344, arg);
			unmoved++;
		}
	}

	/*
	 * Both outcomes, or the sweep proves only that two objects agree about
	 * doing nothing (findings 149, 223).
	 */
	diff_eq_int("the copy happened on %ld trials", moved > 0, 1, moved);
	diff_eq_int("the copy was skipped on %ld trials", unmoved > 0, 1,
		    unmoved);

	return diff_end();
}

/* ------------------------------------------------------------------ */

/*
 * `setTrn2DummyConstel` -- 78 down to whatever the count reaches, into both
 * byte tables of all six constellations.
 *
 * THE COUNTS ARE CHOSEN AGAINST THE WRAP.  78 lands the last entry on 1, 79 on
 * 0 and 80 on 255; a running value declared `int` or `short` would give -1 and
 * -2 there and store the same low byte, so what the 80 trial actually separates
 * is the DECREMENT WIDTH only where the object reloads and re-narrows it --
 * which it does, `movzbl` at 0x3ce47 in the caller and `dec %bl` here.  128 is
 * the row length exactly and is the largest count driven: the loop has no bound
 * of its own and 129 would write into the next row, which the header records
 * and the test does not do.
 */
static int
run_dummy(void)
{
	static const int counts[] = {
		0, 1, 2, 6, 8, 77, 78, 79, 80, 100, 127, 128, -1, -5
	};
	static const int ncounts = (int)(sizeof(counts) / sizeof(counts[0]));
	int trial, wrote = 0, skipped = 0;

	diff_begin("V90TRN2Designer::setTrn2DummyConstel");

	for (trial = 0; trial < ncounts; trial++) {
		int n = counts[trial];
		V90Parameters *p = (V90Parameters *)paramsOurs;

		fill_pair(mpOurs, mpTheirs, MP_BYTES + GUARD, trial);
		memcpy(mpBefore, mpOurs, sizeof(mpBefore));

		memset(paramsOurs, 0, sizeof(paramsOurs));
		p->nofUcodesInTrn2 = n;

		set_designer(paramsOurs);
		our_trn2_dummy(designer, mpOurs);
		ref_trn2_dummy(designer, mpTheirs);

		diff_eq_obj("after setTrn2DummyConstel", V90MappingParams,
			    mpOurs, mpTheirs, n);
		diff_eq_int("no store past the object, count %ld",
			    memcmp(mpOurs + MP_BYTES, mpBefore + MP_BYTES,
				   GUARD) == 0 &&
			    memcmp(mpTheirs + MP_BYTES, mpBefore + MP_BYTES,
				   GUARD) == 0, 1, n);

		if (n > 0) {
			V90MappingParams *mp = (V90MappingParams *)mpOurs;
			int k, i, bad = 0;

			for (k = 0; k < 6; k++)
				for (i = 0; i < n && i < 128; i++) {
					unsigned char want =
						(unsigned char)(78 - i);

					if (mp->constellation[k][i] != want ||
					    mp->codecConstellation[k][i] != want)
						bad = 1;
				}
			diff_eq_int("the run is 78 downwards, count %ld", bad,
				    0, n);
			wrote++;
		} else {
			diff_eq_int("count %ld wrote nothing",
				    memcmp(mpOurs, mpBefore,
					   sizeof(mpOurs)) == 0, 1, n);
			skipped++;
		}
	}

	diff_eq_int("the fill ran on %ld trials", wrote > 0, 1, wrote);
	diff_eq_int("the fill was skipped on %ld trials", skipped > 0, 1,
		    skipped);

	return diff_end();
}

/* ------------------------------------------------------------------ */

/*
 * `maxK` -- log2 of the product of the six lengths, truncated, with a 1e-6
 * guard.
 *
 * EVERY ROW BELOW IS A SEPARATING TRIAL and the comment says what it separates.
 * `wantK` is the value the reading predicts, and it is asserted against the
 * BLOB as well as against ours -- a row whose prediction is wrong fails loudly
 * rather than agreeing with a shared mistake.
 */
struct maxk_case {
	unsigned int size[6];
	int wantK;
	const char *why;
};

static const struct maxk_case maxk_cases[] = {
	/* the zero arm: any zero length makes the product zero */
	{ { 0, 4, 4, 4, 4, 4 },  0, "a zero length short-circuits" },
	{ { 4, 4, 4, 4, 4, 0 },  0, "the zero can be the last one" },
	{ { 0, 0, 0, 0, 0, 0 },  0, "all zero" },

	/*
	 * Exact powers of two: the 1e-6 guard is the only thing holding these
	 * up, AND IT STOPS WORKING AT 2^22.  The divisor is `(float)log10(2)`,
	 * which is 4.757e-8 too large in relative terms, so the quotient comes
	 * back short by k * 4.757e-8 and the guard covers that only while
	 * k <= 21.020.  2^21 clears it by 9.6e-10 and 2^22 misses by 4.7e-8,
	 * which is why both are here: they are one apart and they bracket the
	 * whole behaviour.  See finding 4400.
	 */
	{ { 2, 1, 1, 1, 1, 1 },  1, "2^1 exactly" },
	{ { 2, 2, 2, 2, 2, 2 },  6, "2^6 exactly" },
	{ { 8, 8, 8, 8, 8, 8 }, 18, "2^18 exactly" },
	{ { 2097152u, 1, 1, 1, 1, 1 }, 21, "2^21, the last one the guard saves" },
	{ { 4194304u, 1, 1, 1, 1, 1 }, 21, "2^22, one short -- the guard has run out" },
	{ { 64, 64, 64, 64, 64, 64 }, 35, "2^36, one short" },
	{ { 128, 128, 128, 128, 128, 128 }, 41, "2^42, one short" },
	{ { 1024, 1024, 1024, 1024, 1024, 1024 }, 59, "2^60, one short" },

	/* fractional part >= 0.5: truncation and rounding disagree */
	{ { 3, 1, 1, 1, 1, 1 },  1, "log2 3 = 1.585, truncates to 1" },
	{ { 6, 1, 1, 1, 1, 1 },  2, "log2 6 = 2.585" },
	{ { 3, 3, 1, 1, 1, 1 },  3, "log2 9 = 3.17" },
	{ { 12, 1, 1, 1, 1, 1 },  3, "log2 12 = 3.585" },
	{ { 7, 7, 7, 7, 7, 7 }, 16, "log2 7^6 = 16.84" },
	{ { 3, 3, 3, 3, 3, 3 },  9, "log2 3^6 = 9.51" },

	/* fractional part < 0.5, the other side of the same test */
	{ { 5, 1, 1, 1, 1, 1 },  2, "log2 5 = 2.32" },
	{ { 5, 5, 5, 5, 5, 5 }, 13, "log2 5^6 = 13.93" },
	{ { 9, 1, 1, 1, 1, 1 },  3, "log2 9 = 3.17 again, one term" },

	/* ones only: log2 1 = 0, the smallest non-zero product */
	{ { 1, 1, 1, 1, 1, 1 },  0, "the product is 1" },

	/* the default TRN2 shape: all six lengths equal nofUcodesInTrn2 */
	{ { 8, 8, 8, 8, 8, 8 }, 18, "the default eight ucodes" },
	{ { 4, 4, 4, 4, 4, 4 }, 12, "four ucodes" },

	/* large enough that the deficit above is several bits' worth */
	{ { 16777217u, 1, 1, 1, 1, 1 }, 23, "2^24 + 1" },
	{ { 0xffffffffu, 1, 1, 1, 1, 1 }, 31, "the largest single length" },
	{ { 0xffffffffu, 0xffffffffu, 0xffffffffu,
	    0xffffffffu, 0xffffffffu, 0xffffffffu }, 191, "six of them" }
};

static int
run_maxk(void)
{
	static const int ncases =
		(int)(sizeof(maxk_cases) / sizeof(maxk_cases[0]));
	int trial, sawZero = 0, sawNonZero = 0, distinct = 0, firstK = -1;

	diff_begin("V90TRN2Designer::maxK");

	for (trial = 0; trial < ncases; trial++) {
		const struct maxk_case *c = &maxk_cases[trial];
		V90MappingParams *mp = (V90MappingParams *)mpOurs;
		int got, want, i;

		fill_pair(mpOurs, mpTheirs, MP_BYTES + GUARD, trial);
		for (i = 0; i < 6; i++)
			mp->constellationSize[i] = c->size[i];
		memcpy(mpBefore, mpOurs, sizeof(mpBefore));

		/*
		 * `this` is not touched by `maxK` at all, so the designer is
		 * pointed at nothing on purpose: a member read would fault
		 * rather than quietly agree.
		 */
		set_designer(0);
		got = our_trn2_maxk(designer, mpOurs);
		want = ref_trn2_maxk(designer, mpOurs);

		diff_eq_int("maxK on case %ld", got, want, trial);
		diff_eq_int("maxK case %ld is the predicted value", got,
			    c->wantK, trial);
		diff_eq_int("maxK wrote nothing, case %ld",
			    memcmp(mpOurs, mpBefore, sizeof(mpOurs)) == 0, 1,
			    trial);

		if (got == 0)
			sawZero++;
		else
			sawNonZero++;
		if (firstK < 0)
			firstK = got;
		else if (got != firstK)
			distinct = 1;
	}

	diff_eq_int("maxK returned zero on %ld cases", sawZero > 0, 1, sawZero);
	diff_eq_int("maxK returned non-zero on %ld cases", sawNonZero > 0, 1,
		    sawNonZero);
	diff_eq_int("maxK did not return the same value every case", distinct,
		    1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_setnof();
	rc |= run_dummy();
	rc |= run_maxk();

	return rc;
}

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
#include "dsplib/V90ConstellationPower.h"
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

/* ------------------------------------------------------------------ */

/*
 * `V90TRN2Design` -- the whole class in one call.
 *
 * WHAT HAS TO BE DRIVEN, and none of it is optional:
 *
 *   both arms of `dmin[k] != 0`, EACH reaching both outcomes.  The two share
 *   one `slot` variable with opposite senses -- a count upwards in one and a
 *   descending index in the other -- and both converge on the same
 *   `slot >= 0` test.  Get that wrong and the failure path fires on the
 *   wrong inputs while every ordinary case still passes.
 *
 *   `codecPcmType == pcmType` and `!=`.  The codec fill takes `min(c, u)` in
 *   the first and the raw companded value in the second.
 *
 *   both `PcmType` values.  There are six `linear2alaw`/`linear2ulaw` pairs.
 *
 *   `cond == V90_SPECTRAL_GERMAN_PBX` and not, which selects between two
 *   disjoint runs of `V90Parameters`.
 *
 *   `unnamed_360` at 0x100.  Its low byte is zero and its int value is 256:
 *   the iterative arm compares it as a SIGNED INT against a scan index that
 *   never reaches 128, and the other arm takes it as an UNSIGNED BYTE and
 *   gets 0.  Nothing else in the sweep separates the two widths.
 *
 *   `maxTxIndex` over a range where `averagePowerLimits` differs, which is
 *   what separates the object's `[maxTxIndex + 4]` from `[maxTxIndex]`.
 *
 *   `dsplibs_debug_level` at 0, 1 and 2.  The gated dump is a large fraction
 *   of the function and `debugcov.py` counts sites that never execute.
 *
 *   `unused` varying, asserting nothing changes -- which is how "+0x120 is
 *   never read" becomes a measurement instead of a claim about a listing.
 *
 * THE TABLES ARE SHARED AND THE MAPPING BLOCKS ARE NOT.  Both sides read the
 * same `ucode`, `alt`, `allow`, `dmin`, `topUcode` and `V90Parameters` -- none
 * of which this function writes -- and each writes its own `V90MappingParams`
 * and its own `V90ConstellationPower`, both of which are compared whole.
 *
 * THE ROWS HAVE SLACK PAST THEM ON PURPOSE.  `while (compand(...) > maxLevel)
 * top[k]--` has no floor, so a table whose entries never fall under the
 * ceiling wraps the `unsigned char` to 255 and reads past row 5.  Entry 0 of
 * every row is zero, whose companded value is 0 and therefore always under
 * the ceiling, so the walk terminates -- and the slack is there so that a
 * defect which walks anyway reads defined bytes that BOTH sides see, rather
 * than turning a wrong answer into a crash.
 */
extern "C" {
short our_trn2_design(void *, void *, void *, void *, void *, void *,
		      int, int, int, void *, unsigned int, int, int)
	asm("_ZN15V90TRN2Designer13V90TRN2DesignEP16V90MappingParamsPA128_sS3_"
	    "PA128_hPs7PcmTypeS7_sPhjh28V90SpecialSpectralConditions");
short ref_trn2_design(void *, void *, void *, void *, void *, void *,
		      int, int, int, void *, unsigned int, int, int)
	asm("ref__ZN15V90TRN2Designer13V90TRN2DesignEP16V90MappingParamsPA128_"
	    "sS3_PA128_hPs7PcmTypeS7_sPhjh28V90SpecialSpectralConditions");
extern unsigned int dsplibs_debug_level;
}

#define ROWS		6
#define ROWLEN		128
#define SLACK		512
#define TABLE		(ROWS * ROWLEN + SLACK)

static short tabUcode[TABLE];
static short tabAlt[TABLE];
static unsigned char tabAllow[TABLE];
static short tabDmin[ROWS];
static unsigned char tabTop[ROWS];

static unsigned char powerOurs[256] __attribute__((aligned(8)));
static unsigned char powerTheirs[256] __attribute__((aligned(8)));

static unsigned char mp2Ours[MP_BYTES + GUARD] __attribute__((aligned(8)));
static unsigned char mp2Theirs[MP_BYTES + GUARD] __attribute__((aligned(8)));

/*
 * A rising level table, so that walking an index DOWN lowers the companded
 * value and the unbounded walk above terminates.  `shape` moves the top of
 * the range across the four power decades the ladder covers, which is what
 * makes some trials design cleanly and others run out of candidates.
 */
static void
build_tables(int trial, int shape)
{
	int k, i;

	lfsr = 0x7ac1u + 0x9e37u * (unsigned)trial + 1u;
	for (i = 0; i < TABLE; i++) {
		tabUcode[i] = 0;
		tabAlt[i] = 0;
		tabAllow[i] = 0;
	}
	for (k = 0; k < ROWS; k++) {
		int base = k * ROWLEN;
		int step = 4 + shape * 13 + k;

		for (i = 0; i < ROWLEN; i++) {
			int v = i * step + (i * i) / (2 + (shape & 3));

			tabUcode[base + i] = (short)v;
			/*
			 * The alternate table is close to the first but not
			 * equal to it, because the scan tests BOTH and takes
			 * their minimum: two identical tables would make the
			 * second test unreachable.
			 */
			tabAlt[base + i] = (short)(v - (int)(next_byte() & 7));
			tabAllow[base + i] =
				(unsigned char)((next_byte() % 5) != 0);
		}
		tabUcode[base] = 0;
		tabAlt[base] = 0;
		tabTop[k] = (unsigned char)(96 + (int)(next_byte() % 32));
	}
}

static int
run_design(void)
{
	int trial;
	int sawOk = 0, sawFail = 0;
	int sawTargetOk = 0, sawTargetFail = 0;
	int sawFreeOk = 0, sawFreeFail = 0;
	int sawSamePcm = 0, sawDiffPcm = 0, sawPbx = 0, sawPlain = 0;
	int sawLevel[3];
	int distinct = 0;
	unsigned char firstMp[MP_BYTES];
	int haveFirst = 0;

	sawLevel[0] = sawLevel[1] = sawLevel[2] = 0;

	diff_begin("V90TRN2Designer::V90TRN2Design");

	for (trial = 0; trial < 240; trial++) {
		V90Parameters *pp = (V90Parameters *)paramsOurs;
		int shape = trial % 6;
		int dminMask = trial % 64;
		int pcm = (trial >> 1) & 1;
		int codecPcm = (trial >> 2) & 1;
		int level = trial % 3;
		static const int nofs[] = { 1, 2, 3, 4, 7, 8, 10, 13, 21 };
		int nof = nofs[trial % 9];
		int lower = (trial % 7) == 0 ? 0x100
					     : (trial % 7) == 1 ? 0 : trial % 7;
		int maxTx = trial % 21;
		int cond = (trial % 5) == 0 ? V90_SPECTRAL_GERMAN_PBX : 0;
		unsigned int look = (unsigned int)(trial * 37);
		int unusedArg = (int)(short)(0x1234 + trial * 977);
		short gotOurs, gotRef;
		int k;

		build_tables(trial, shape);

		/*
		 * The parameter block is SHARED: nothing in this function
		 * writes it, so both sides read byte-for-byte the same input
		 * and a difference cannot come from the parameters.
		 */
		fill_pair(paramsOurs, paramsTheirs, 0x558 + GUARD, trial);
		pp->nofUcodesInTrn2 = nof;
		pp->maxUcode = 60 + (trial % 40);
		pp->unnamed_360 = lower;
		pp->SPECTRAL_SHAPER_A1 = 1.5f;
		pp->SPECTRAL_SHAPER_A2 = -0.25f;
		pp->SPECTRAL_SHAPER_B1 = 0.75f;
		pp->SPECTRAL_SHAPER_B2 = 0.125f;
		pp->SPECTRAL_SHAPER_SR = 3 + (trial % 4);
		pp->SPECTRAL_SHAPER_ID = 5 + (trial % 9);
		pp->GERMAN_PBX_SPECTRAL_SHAPER_A1 = -1.5f;
		pp->GERMAN_PBX_SPECTRAL_SHAPER_A2 = 0.25f;
		pp->GERMAN_PBX_SPECTRAL_SHAPER_B1 = -0.75f;
		pp->GERMAN_PBX_SPECTRAL_SHAPER_B2 = -0.125f;
		pp->GERMAN_PBX_SPECTRAL_SHAPER_SR = 2 + (trial % 5);
		pp->GERMAN_PBX_SPECTRAL_SHAPER_ID = 3 + (trial % 11);

		for (k = 0; k < ROWS; k++)
			tabDmin[k] = (dminMask & (1 << k)) ? (short)(40 + k)
							   : (short)0;

		memcpy(paramsBefore, paramsOurs, sizeof(paramsBefore));
		fill_pair(mp2Ours, mp2Theirs, MP_BYTES + GUARD, trial);
		fill_pair(powerOurs, powerTheirs, 256, trial + 1);
		memcpy(mpBefore, mp2Ours, sizeof(mpBefore));

		dsplibs_debug_level = (unsigned int)level;
		sawLevel[level]++;

		set_designer(paramsOurs);
		((V90TRN2Designer *)designer)->power =
			(V90ConstellationPower *)powerOurs;
		gotOurs = our_trn2_design(designer, mp2Ours, tabUcode, tabAlt,
					  tabAllow, tabDmin, codecPcm, pcm,
					  unusedArg, tabTop, look, maxTx, cond);

		/*
		 * The SAME parameter block, deliberately: this function only
		 * reads it, so handing both sides one object makes the input
		 * identical by construction rather than by a fill that has to
		 * be kept in step.
		 */
		((V90TRN2Designer *)designer)->power =
			(V90ConstellationPower *)powerTheirs;
		gotRef = ref_trn2_design(designer, mp2Theirs, tabUcode, tabAlt,
					 tabAllow, tabDmin, codecPcm, pcm,
					 unusedArg, tabTop, look, maxTx, cond);
		dsplibs_debug_level = 0;

		diff_eq_int("V90TRN2Design returned, trial %ld",
			    gotOurs, gotRef, trial);
		diff_eq_obj("after V90TRN2Design", V90MappingParams,
			    mp2Ours, mp2Theirs, trial);
		/*
		 * THE POWER OBJECT IS COMPARED PAST ITS FIRST FOUR BYTES, and
		 * the skipped four are checked a different way.  +0x00 is a
		 * pointer INTO the caller's mapping block, so the two sides
		 * hold two different addresses and always will -- CLAUDE.md's
		 * case for a loop rather than a whole-object compare.  What
		 * has to be equal is the OFFSET each points at, which is the
		 * part the function chose.
		 */
		diff_eq_int("the power object agrees past its pointer, "
			    "trial %ld",
			    memcmp(powerOurs + 4, powerTheirs + 4,
				   sizeof(powerOurs) - 4) == 0, 1, trial);
		/*
		 * ONLY WHERE THE DESIGN SUCCEEDED.  The failure path returns
		 * before `getPower`, so the pointer is still the fill and the
		 * two sides hold the same garbage at two different bases --
		 * which the offset comparison would read as a difference.
		 * That it is untouched at all is what the compare above says.
		 */
		if (gotOurs) {
			unsigned char *co =
			    ((V90ConstellationPower *)powerOurs)->constellation;
			unsigned char *ct =
			    ((V90ConstellationPower *)powerTheirs)
				->constellation;

			diff_eq_int("the selected constellation is the same "
				    "offset, trial %ld",
				    co - mp2Ours, ct - mp2Theirs, trial);
		}
		diff_eq_int("no store past the mapping block, trial %ld",
			    memcmp(mp2Ours + MP_BYTES, mpBefore + MP_BYTES,
				   GUARD) == 0 &&
			    memcmp(mp2Theirs + MP_BYTES, mpBefore + MP_BYTES,
				   GUARD) == 0, 1, trial);
		diff_eq_int("the shared parameter block was not written, "
			    "trial %ld",
			    memcmp(paramsOurs, paramsBefore,
				   sizeof(paramsOurs)) == 0, 1, trial);

		if (gotOurs)
			sawOk++;
		else
			sawFail++;
		if (dminMask != 0) {
			if (gotOurs)
				sawTargetOk++;
			else
				sawTargetFail++;
		}
		if (dminMask != 63) {
			if (gotOurs)
				sawFreeOk++;
			else
				sawFreeFail++;
		}
		if (codecPcm == pcm)
			sawSamePcm++;
		else
			sawDiffPcm++;
		if (cond == V90_SPECTRAL_GERMAN_PBX)
			sawPbx++;
		else
			sawPlain++;

		if (!haveFirst) {
			memcpy(firstMp, mp2Ours, MP_BYTES);
			haveFirst = 1;
		} else if (memcmp(firstMp, mp2Ours, MP_BYTES) != 0) {
			distinct = 1;
		}
	}

	/*
	 * THE RECIPROCAL, SEPARATED.  `x * (1.0f / (N - 0.5f))` and
	 * `x / (N - 0.5f)` agree over almost every input and differ in the
	 * last bits, which only shows once the `(short)` truncation lands on
	 * the far side of an integer.  Searched rather than guessed, in 80-bit
	 * arithmetic over every `(N, level)` up to 32 x 32768: the first pair
	 * that separates is N = 21, level = 41, where the reciprocal gives
	 * dMin 1 and the divide gives 2.
	 *
	 * A different dMin then designs a DIFFERENT constellation -- with 1 it
	 * takes consecutive ucodes and with 2 it takes every other one -- so
	 * the difference reaches the compared object rather than dying inside
	 * an intermediate.  The table is linear and fully permitted so that
	 * both spellings SUCCEED and the comparison is between two designs
	 * rather than between two failures, which `setTrn2DummyConstel` would
	 * make identical again.
	 */
	{
		V90Parameters *pp = (V90Parameters *)paramsOurs;
		int k, i;
		short a, b;

		for (i = 0; i < TABLE; i++) {
			tabUcode[i] = 0;
			tabAlt[i] = 0;
			tabAllow[i] = 1;
		}
		for (k = 0; k < ROWS; k++) {
			for (i = 0; i < ROWLEN; i++) {
				tabUcode[k * ROWLEN + i] = (short)i;
				tabAlt[k * ROWLEN + i] = (short)i;
			}
			tabTop[k] = 41;
			tabDmin[k] = 1;
		}
		fill_pair(paramsOurs, paramsTheirs, 0x558 + GUARD, 11);
		pp->nofUcodesInTrn2 = 21;
		pp->maxUcode = 92;
		pp->unnamed_360 = 0;
		pp->SPECTRAL_SHAPER_SR = 3;
		pp->SPECTRAL_SHAPER_ID = 5;
		pp->SPECTRAL_SHAPER_A1 = 1.0f;
		pp->SPECTRAL_SHAPER_A2 = 0.0f;
		pp->SPECTRAL_SHAPER_B1 = 0.0f;
		pp->SPECTRAL_SHAPER_B2 = 0.0f;

		fill_pair(mp2Ours, mp2Theirs, MP_BYTES + GUARD, 11);
		fill_pair(powerOurs, powerTheirs, 256, 12);
		dsplibs_debug_level = 0;
		set_designer(paramsOurs);
		((V90TRN2Designer *)designer)->power =
			(V90ConstellationPower *)powerOurs;
		a = our_trn2_design(designer, mp2Ours, tabUcode, tabAlt,
				    tabAllow, tabDmin, 1, 1, 0, tabTop, 9u,
				    7, 0);
		((V90TRN2Designer *)designer)->power =
			(V90ConstellationPower *)powerTheirs;
		b = ref_trn2_design(designer, mp2Theirs, tabUcode, tabAlt,
				    tabAllow, tabDmin, 1, 1, 0, tabTop, 9u,
				    7, 0);

		diff_eq_int("the reciprocal case returned", a, b, 0);
		diff_eq_obj("the reciprocal case designed", V90MappingParams,
			    mp2Ours, mp2Theirs, 0);
		/*
		 * The trial is only a separator if it DESIGNED: a failure on
		 * both sides is `setTrn2DummyConstel`'s output either way and
		 * separates nothing.  Assert the outcome the search predicted.
		 */
		diff_eq_int("the reciprocal case designed rather than failed",
			    a, 1, 0);
		diff_eq_int("and it took consecutive ucodes, so dMin was 1",
			    ((V90MappingParams *)mp2Ours)->constellation[0][1],
			    40, 0);
	}

	/*
	 * `unused` is never read, and this is the measurement rather than the
	 * claim: the same trial run twice with two different values in that
	 * slot must produce the same mapping block.
	 */
	{
		unsigned char a[MP_BYTES + GUARD];
		int same;

		build_tables(3, 2);
		fill_pair(paramsOurs, paramsTheirs, 0x558 + GUARD, 3);
		((V90Parameters *)paramsOurs)->nofUcodesInTrn2 = 8;
		((V90Parameters *)paramsOurs)->maxUcode = 92;
		((V90Parameters *)paramsOurs)->unnamed_360 = 2;
		tabDmin[0] = 1;
		tabDmin[1] = 0;
		tabDmin[2] = 1;
		tabDmin[3] = 0;
		tabDmin[4] = 1;
		tabDmin[5] = 0;

		fill_pair(mp2Ours, mp2Theirs, MP_BYTES + GUARD, 3);
		set_designer(paramsOurs);
		((V90TRN2Designer *)designer)->power =
			(V90ConstellationPower *)powerOurs;
		our_trn2_design(designer, mp2Ours, tabUcode, tabAlt, tabAllow,
				tabDmin, 1, 1, 0x0000, tabTop, 9u, 7, 0);
		memcpy(a, mp2Ours, sizeof(a));

		fill_pair(mp2Ours, mp2Theirs, MP_BYTES + GUARD, 3);
		our_trn2_design(designer, mp2Ours, tabUcode, tabAlt, tabAllow,
				tabDmin, 1, 1, 0x7fff, tabTop, 9u, 7, 0);
		same = memcmp(a, mp2Ours, sizeof(a)) == 0;
		diff_eq_int("the eighth argument is never read", same, 1, 0);
	}

	/*
	 * Every outcome, or the sweep proves only that two objects agree about
	 * one path (findings 149, 223, 3509).
	 */
	diff_eq_int("designed on %ld trials", sawOk > 0, 1, sawOk);
	diff_eq_int("failed on %ld trials", sawFail > 0, 1, sawFail);
	diff_eq_int("the dmin arm designed on %ld trials", sawTargetOk > 0, 1,
		    sawTargetOk);
	diff_eq_int("the dmin arm failed on %ld trials", sawTargetFail > 0, 1,
		    sawTargetFail);
	diff_eq_int("the free arm designed on %ld trials", sawFreeOk > 0, 1,
		    sawFreeOk);
	diff_eq_int("the free arm failed on %ld trials", sawFreeFail > 0, 1,
		    sawFreeFail);
	diff_eq_int("equal PcmType on %ld trials", sawSamePcm > 0, 1,
		    sawSamePcm);
	diff_eq_int("unequal PcmType on %ld trials", sawDiffPcm > 0, 1,
		    sawDiffPcm);
	diff_eq_int("the German PBX arm on %ld trials", sawPbx > 0, 1, sawPbx);
	diff_eq_int("the plain shaper arm on %ld trials", sawPlain > 0, 1,
		    sawPlain);
	diff_eq_int("debug level 0 on %ld trials", sawLevel[0] > 0, 1,
		    sawLevel[0]);
	diff_eq_int("debug level 1 on %ld trials", sawLevel[1] > 0, 1,
		    sawLevel[1]);
	diff_eq_int("debug level 2 on %ld trials", sawLevel[2] > 0, 1,
		    sawLevel[2]);
	diff_eq_int("the design was not the same block every trial", distinct,
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
	rc |= run_design();

	return rc;
}

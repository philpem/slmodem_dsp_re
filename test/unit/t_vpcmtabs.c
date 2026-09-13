/*
 * t_vpcmtabs.c -- the ten coefficient tables of the V.PCM construction path,
 * against the blob's own bytes.
 *
 * There is nothing to drive here: the claim is that our copy IS the blob's
 * copy, so every table is compared byte for byte against its `ref_` alias.
 * All ten are file-local in the object and are only reachable because the
 * Makefile globalizes file-local symbols before renaming them, the same route
 * `t_v34pcmtab.c` takes to `V34DisconnectThreshTable`.
 *
 * THE COMPARISON IS OVER BYTES AND NOT OVER VALUES, deliberately.  A table
 * emitted through `%f` and reparsed can land on a neighbouring float and
 * compare equal to the eye; and -0.0 == 0.0 while differing in every bit that
 * matters to a sign test.  `diff_eq_obj` on a wrapper struct is a memcmp that
 * reports the first differing run, so both are caught.
 *
 * ANTI-VACUITY IS NOT ONE CHECK BUT THREE, because these tables have three
 * different ways of being vacuously equal:
 *
 *   - a table of one repeated value passes any element-wise comparison
 *     against a copy of itself, so `spread()` counts distinct 8-byte words;
 *   - a table whose values all round-trip through six decimal digits proves
 *     nothing about the emitter, so `ragged()` counts values with their low
 *     mantissa bits set;
 *   - and `v34initialbauds` REALLY IS all one value, six bytes of 1, so it
 *     gets neither check and is asserted against its literal contents
 *     instead.  Saying that out loud is the point: the exemption is in the
 *     one place it is true and nowhere else.
 *
 * The structural assertions at the end are independent of the comparison on
 * purpose.  They are the facts src/pump/v90/vpcm_tables.c's comments claim --
 * the leading 1.0 of each denominator, the zeros on the odd taps of
 * `IIR2100_Coef_B_9600`, the mirror symmetry of `v92TxPreFilter` -- and a
 * comment nothing checks is a comment that goes stale.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/vpcm_tables.h"

extern double ref_entFiltNum[VPCM_ENTFILT_TAPS] asm("ref_entFiltNum");
extern double ref_entFiltDen[VPCM_ENTFILT_TAPS] asm("ref_entFiltDen");
extern double ref_IIR2100_Coef_A_8000[IIR2100_TAPS_8000]
	asm("ref_IIR2100_Coef_A_8000");
extern double ref_IIR2100_Coef_B_8000[IIR2100_TAPS_8000]
	asm("ref_IIR2100_Coef_B_8000");
extern double ref_IIR2100_Coef_A_9600[IIR2100_TAPS_9600]
	asm("ref_IIR2100_Coef_A_9600");
extern double ref_IIR2100_Coef_B_9600[IIR2100_TAPS_9600]
	asm("ref_IIR2100_Coef_B_9600");
extern float ref_v92echoPreFilter_a[V92_ECHO_PREFILTER_TAPS]
	asm("ref_v92echoPreFilter_a");
extern float ref_v92echoPreFilter_b[V92_ECHO_PREFILTER_TAPS]
	asm("ref_v92echoPreFilter_b");
extern float ref_v92TxPreFilter[V92_TXPREFILTER_TAPS]
	asm("ref_v92TxPreFilter");
extern const unsigned char ref_v34initialbauds[V34_INITIAL_BAUDS]
	asm("ref_v34initialbauds");

/*
 * Ours: FILE-LOCAL in the object, so it is `static` in
 * VPcmFloModemCtor.cpp, its only consumer, and vpcm_tables.h no longer
 * declares it.  The test tier links a globalized copy (tools/testvisible.py).
 */
extern const unsigned char v34initialbauds[V34_INITIAL_BAUDS];

/*
 * The seven moved tables are FILE-LOCAL `static` in their reconstructed
 * consumer now, so vpcm_tables.h no longer declares them.  A differential
 * test can still name the plain symbol because the test tier links a
 * globalized copy of each reconstructed object (tools/testvisible.py); the
 * declarations are supplied here.
 */
extern double entFiltNum[VPCM_ENTFILT_TAPS];
extern double entFiltDen[VPCM_ENTFILT_TAPS];
extern double IIR2100_Coef_A_8000[IIR2100_TAPS_8000];
extern double IIR2100_Coef_B_8000[IIR2100_TAPS_8000];
extern double IIR2100_Coef_A_9600[IIR2100_TAPS_9600];
extern double IIR2100_Coef_B_9600[IIR2100_TAPS_9600];
extern float v92TxPreFilter[V92_TXPREFILTER_TAPS];

/* Wrappers, so `diff_eq_obj` coalesces a differing run into one report. */
struct tab_d5 { double v[5]; };
struct tab_d11 { double v[11]; };
struct tab_d13 { double v[13]; };
struct tab_f12 { float v[12]; };
struct tab_f36 { float v[36]; };
struct tab_u6 { unsigned char v[6]; };

/* How many DISTINCT 8-byte words the table holds.  1 means all one value. */
static int
spread(const void *p, int nbytes, int elemsize)
{
	const unsigned char *b = (const unsigned char *)p;
	int n = nbytes / elemsize;
	int distinct = 0, i, j;

	for (i = 0; i < n; i++) {
		for (j = 0; j < i; j++)
			if (memcmp(b + (size_t)i * elemsize,
				   b + (size_t)j * elemsize,
				   (size_t)elemsize) == 0)
				break;
		if (j == i)
			distinct++;
	}
	return distinct;
}

/*
 * How many elements have their low mantissa bits set.  A value a six-digit
 * decimal round trip would have survived tends to have a clean tail; one that
 * carries eight or nine significant figures does not.
 */
static int
ragged(const void *p, int nbytes, int elemsize)
{
	const unsigned char *b = (const unsigned char *)p;
	int n = nbytes / elemsize;
	int count = 0, i;

	for (i = 0; i < n; i++)
		if ((b[(size_t)i * elemsize] & 0x0f) != 0)
			count++;
	return count;
}

static int
run_bytes(void)
{
	diff_begin("V.PCM coefficient tables against the blob");

	diff_eq_obj("entFiltNum", struct tab_d5, entFiltNum, ref_entFiltNum, 0);
	diff_eq_obj("entFiltDen", struct tab_d5, entFiltDen, ref_entFiltDen, 0);
	diff_eq_obj("IIR2100_Coef_A_8000", struct tab_d13, IIR2100_Coef_A_8000,
		    ref_IIR2100_Coef_A_8000, 0);
	diff_eq_obj("IIR2100_Coef_B_8000", struct tab_d13, IIR2100_Coef_B_8000,
		    ref_IIR2100_Coef_B_8000, 0);
	diff_eq_obj("IIR2100_Coef_A_9600", struct tab_d11, IIR2100_Coef_A_9600,
		    ref_IIR2100_Coef_A_9600, 0);
	diff_eq_obj("IIR2100_Coef_B_9600", struct tab_d11, IIR2100_Coef_B_9600,
		    ref_IIR2100_Coef_B_9600, 0);
	diff_eq_obj("v92echoPreFilter_a", struct tab_f12, v92echoPreFilter_a,
		    ref_v92echoPreFilter_a, 0);
	diff_eq_obj("v92echoPreFilter_b", struct tab_f12, v92echoPreFilter_b,
		    ref_v92echoPreFilter_b, 0);
	diff_eq_obj("v92TxPreFilter", struct tab_f36, v92TxPreFilter,
		    ref_v92TxPreFilter, 0);
	diff_eq_obj("v34initialbauds", struct tab_u6, v34initialbauds,
		    ref_v34initialbauds, 0);

	/*
	 * The declared lengths as well as the contents: a table one element
	 * short compares equal on every element it has, and the wrapper
	 * structs above would then be comparing somebody else's bytes.
	 */
	diff_eq_int("entFiltNum is %ld bytes", (long)sizeof(entFiltNum), 40,
		    40);
	diff_eq_int("entFiltDen is %ld bytes", (long)sizeof(entFiltDen), 40,
		    40);
	diff_eq_int("IIR2100_Coef_A_8000 is %ld bytes",
		    (long)sizeof(IIR2100_Coef_A_8000), 104, 104);
	diff_eq_int("IIR2100_Coef_B_8000 is %ld bytes",
		    (long)sizeof(IIR2100_Coef_B_8000), 104, 104);
	diff_eq_int("IIR2100_Coef_A_9600 is %ld bytes",
		    (long)sizeof(IIR2100_Coef_A_9600), 88, 88);
	diff_eq_int("IIR2100_Coef_B_9600 is %ld bytes",
		    (long)sizeof(IIR2100_Coef_B_9600), 88, 88);
	diff_eq_int("v92echoPreFilter_a is %ld bytes",
		    (long)sizeof(v92echoPreFilter_a), 48, 48);
	diff_eq_int("v92echoPreFilter_b is %ld bytes",
		    (long)sizeof(v92echoPreFilter_b), 48, 48);
	diff_eq_int("v92TxPreFilter is %ld bytes",
		    (long)sizeof(v92TxPreFilter), 144, 144);
	diff_eq_int("v34initialbauds is %ld bytes",
		    (long)sizeof(v34initialbauds), 6, 6);

	return diff_end();
}

static int
run_antivacuity(void)
{
	diff_begin("the tables are not vacuously equal");

	/*
	 * Not all one value.  Every one of the nine coefficient tables has at
	 * least three distinct elements; the weakest is IIR2100_Coef_B_9600,
	 * which is half zeros and still holds six.
	 */
	diff_eq_int("entFiltNum has >2 distinct values",
		    spread(entFiltNum, 40, 8) > 2, 1, 0);
	diff_eq_int("entFiltDen has >2 distinct values",
		    spread(entFiltDen, 40, 8) > 2, 1, 0);
	diff_eq_int("IIR2100_Coef_A_8000 has >2 distinct values",
		    spread(IIR2100_Coef_A_8000, 104, 8) > 2, 1, 0);
	diff_eq_int("IIR2100_Coef_B_8000 has >2 distinct values",
		    spread(IIR2100_Coef_B_8000, 104, 8) > 2, 1, 0);
	diff_eq_int("IIR2100_Coef_A_9600 has >2 distinct values",
		    spread(IIR2100_Coef_A_9600, 88, 8) > 2, 1, 0);
	diff_eq_int("IIR2100_Coef_B_9600 has >2 distinct values",
		    spread(IIR2100_Coef_B_9600, 88, 8) > 2, 1, 0);
	diff_eq_int("v92echoPreFilter_a has >2 distinct values",
		    spread(v92echoPreFilter_a, 48, 4) > 2, 1, 0);
	diff_eq_int("v92echoPreFilter_b has >2 distinct values",
		    spread(v92echoPreFilter_b, 48, 4) > 2, 1, 0);
	diff_eq_int("v92TxPreFilter has >2 distinct values",
		    spread(v92TxPreFilter, 144, 4) > 2, 1, 0);

	/*
	 * And the blob's copies too, taken the same way.  If the globalizing
	 * rename ever hands us a zero-filled or absent symbol, the comparison
	 * above goes quiet and this does not.
	 */
	diff_eq_int("ref_entFiltNum has >2 distinct values",
		    spread(ref_entFiltNum, 40, 8) > 2, 1, 0);
	diff_eq_int("ref_v92TxPreFilter has >2 distinct values",
		    spread(ref_v92TxPreFilter, 144, 4) > 2, 1, 0);
	diff_eq_int("ref_IIR2100_Coef_A_8000 has >2 distinct values",
		    spread(ref_IIR2100_Coef_A_8000, 104, 8) > 2, 1, 0);

	/*
	 * Low mantissa bits set: what a decimal round trip through too few
	 * digits would have flattened.  The floats are checked at eight
	 * elements and the doubles at four, which is well under what the
	 * tables actually carry.
	 */
	diff_eq_int("entFiltNum carries full precision",
		    ragged(entFiltNum, 40, 8) >= 4, 1, 0);
	diff_eq_int("entFiltDen carries full precision",
		    ragged(entFiltDen, 40, 8) >= 4, 1, 0);
	diff_eq_int("IIR2100_Coef_A_8000 carries full precision",
		    ragged(IIR2100_Coef_A_8000, 104, 8) >= 4, 1, 0);
	diff_eq_int("IIR2100_Coef_B_8000 carries full precision",
		    ragged(IIR2100_Coef_B_8000, 104, 8) >= 4, 1, 0);
	diff_eq_int("IIR2100_Coef_A_9600 carries full precision",
		    ragged(IIR2100_Coef_A_9600, 88, 8) >= 4, 1, 0);
	diff_eq_int("v92echoPreFilter_a carries full precision",
		    ragged(v92echoPreFilter_a, 48, 4) >= 8, 1, 0);
	diff_eq_int("v92echoPreFilter_b carries full precision",
		    ragged(v92echoPreFilter_b, 48, 4) >= 8, 1, 0);
	diff_eq_int("v92TxPreFilter carries full precision",
		    ragged(v92TxPreFilter, 144, 4) >= 8, 1, 0);

	/*
	 * `v34initialbauds` IS all one value and gets neither check.  Six
	 * bytes of 1, asserted literally, so a zero-filled or truncated symbol
	 * fails here rather than comparing equal to another zero-filled one.
	 */
	{
		int i, ones = 0;

		for (i = 0; i < V34_INITIAL_BAUDS; i++) {
			diff_eq_int("v34initialbauds[%ld] is 1",
				    v34initialbauds[i], 1, i);
			if (ref_v34initialbauds[i] == 1)
				ones++;
		}
		diff_eq_int("the blob's v34initialbauds is six ones", ones, 6,
			    0);
	}

	return diff_end();
}

static int
run_structure(void)
{
	int i, zeros = 0, nonzeros = 0, mirrored = 0;

	diff_begin("what the tables' shapes claim");

	/* Every denominator is normalised; no numerator is. */
	diff_eq_int("entFiltDen[0] is 1.0", entFiltDen[0] == 1.0, 1, 0);
	diff_eq_int("IIR2100_Coef_A_8000[0] is 1.0",
		    IIR2100_Coef_A_8000[0] == 1.0, 1, 0);
	diff_eq_int("IIR2100_Coef_A_9600[0] is 1.0",
		    IIR2100_Coef_A_9600[0] == 1.0, 1, 0);
	diff_eq_int("v92echoPreFilter_a[0] is 1.0f",
		    v92echoPreFilter_a[0] == 1.0f, 1, 0);
	diff_eq_int("entFiltNum[0] is not 1.0", entFiltNum[0] != 1.0, 1, 0);
	diff_eq_int("v92echoPreFilter_b[0] is not 1.0f",
		    v92echoPreFilter_b[0] != 1.0f, 1, 0);

	/*
	 * IIR2100_Coef_B_9600 is (1 - z^-2)^5 scaled: the odd taps are exactly
	 * zero and the even ones are 1, -5, 10, -10, 5, -1 times the first.
	 * The ratios are checked as ratios and not as literals, so this stays
	 * true of a rescaled design and false of a transcription error.
	 */
	{
		static const double want[6] = { 1.0, -5.0, 10.0, -10.0, 5.0,
						-1.0 };
		double b0 = IIR2100_Coef_B_9600[0];

		for (i = 0; i < IIR2100_TAPS_9600; i++) {
			if (i & 1) {
				diff_eq_int("IIR2100_Coef_B_9600[%ld] is zero",
					    IIR2100_Coef_B_9600[i] == 0.0, 1,
					    i);
				zeros++;
			} else {
				double r = IIR2100_Coef_B_9600[i] / b0;
				double w = want[i / 2];
				double e = r - w;

				if (e < 0)
					e = -e;
				diff_eq_int("IIR2100_Coef_B_9600[%ld] / b0 is "
					    "binomial", e < 1e-12, 1, i);
				nonzeros++;
			}
		}
		diff_eq_int("five odd taps are zero", zeros, 5, 0);
		diff_eq_int("six even taps are not", nonzeros, 6, 0);
	}

	/*
	 * v92TxPreFilter is a 35-tap linear-phase FIR about tap 17, with a
	 * 36th slot that is padding.  Compared as BITS, because a mirror that
	 * held -0.0 against +0.0 would pass a float compare.
	 */
	for (i = 0; i <= 17; i++) {
		unsigned lo, hi;

		memcpy(&lo, &v92TxPreFilter[17 - i], sizeof(lo));
		memcpy(&hi, &v92TxPreFilter[17 + i], sizeof(hi));
		diff_eq_int("v92TxPreFilter mirrors about tap 17 (k=%ld)",
			    lo == hi, 1, i);
		if (lo == hi)
			mirrored++;
	}
	diff_eq_int("eighteen mirrored pairs", mirrored, 18, 0);
	{
		unsigned last;

		memcpy(&last, &v92TxPreFilter[35], sizeof(last));
		diff_eq_int("v92TxPreFilter's 36th slot is +0.0f", last, 0, 35);
	}
	diff_eq_int("v92TxPreFilter's centre tap is the big one",
		    v92TxPreFilter[17] > 0.9f, 1, 17);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_bytes();
	rc |= run_antivacuity();
	rc |= run_structure();

	return rc;
}

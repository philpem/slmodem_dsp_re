/*
 * t_mtktab.c -- the six MTK tables, byte for byte against the blob's own.
 *
 * WHY A BYTE COMPARISON AND NOT A LOOKUP TEST.  D955/F8587: a table proved by
 * "a lookup agreed" proves nothing about the entries the lookup did not
 * reach, and a blob-against-blob dry run cannot see an unplanted index at all
 * because both sides read the same wrong cell.  So every one of the 3,628
 * bytes of the six tables is compared here, in two independent ways:
 *
 *   - a `memcmp` per table, which catches anything a value comparison can
 *     call equal (a -0.0f against a 0.0f, or a NaN against a different NaN);
 *   - a per-element comparison, which is what SAYS WHICH ENTRY is wrong when
 *     one is -- `diff_eq_float` reports the two values and their ULP
 *     distance, where the memcmp reports only that the table differs.
 *
 * THE DENOMINATOR IS PRINTED (F134/F2401).  A table test whose loop bound
 * came out zero would be silently green, so `t_sizes` compares every
 * `sizeof` against the object's `st_size` first, `main` prints the number of
 * elements and bytes actually compared, and the run fails if either is zero.
 *
 * THE TWO EXACT IDENTITIES are checked here rather than described in a
 * comment, because a comment cannot fail: `MTK_cos_table[i]` is
 * `MTK_sin_table[256 - i]` for all 257 entries, and `MTK_xor_table[i]` is
 * the population count of `i` for all 256.  Both hold bit for bit in the
 * blob, so they are as much a property of the data as the bytes are.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/mtk.h"

extern const float ref_MTK_sin_table[257];
extern const float ref_MTK_cos_table[257];
extern const float ref_MTK_sin_sign[4];
extern const float ref_MTK_cos_sign[4];
extern const float ref_MTK_atan_table[257];
extern const short ref_MTK_xor_table[256];

static long elems_compared;
static long bytes_compared;

/*
 * The object's own `st_size` for each of the six, read out of
 * `readelf -sW ref/slmodemd/dsplibs.o`.  A `sizeof` that disagreed would mean
 * the reconstruction had the wrong number of entries, which every loop below
 * would then quietly stop short of.
 */
static int
t_sizes(void)
{
	diff_begin("each table is the size the object gives it");
	diff_eq_int("sizeof MTK_sin_table", (long)sizeof MTK_sin_table, 1028,
		    0);
	diff_eq_int("sizeof MTK_cos_table", (long)sizeof MTK_cos_table, 1028,
		    0);
	diff_eq_int("sizeof MTK_atan_table", (long)sizeof MTK_atan_table, 1028,
		    0);
	diff_eq_int("sizeof MTK_sin_sign", (long)sizeof MTK_sin_sign, 16, 0);
	diff_eq_int("sizeof MTK_cos_sign", (long)sizeof MTK_cos_sign, 16, 0);
	diff_eq_int("sizeof MTK_xor_table", (long)sizeof MTK_xor_table, 512, 0);
	return diff_end();
}

static void
cmp_floats(const char *what, const float *ours, const float *ref, int n,
	   size_t nbytes)
{
	int i;
	char msg[80];

	sprintf(msg, "%s: memcmp against the blob", what);
	diff_eq_int(msg, memcmp(ours, ref, nbytes) == 0, 1, 0);
	bytes_compared += (long)nbytes;

	for (i = 0; i < n; i++) {
		sprintf(msg, "%s[%%ld]", what);
		diff_eq_float(msg, ours[i], ref[i], (long)i);
		elems_compared++;
	}
}

static int
t_tables(void)
{
	int i;

	diff_begin("the six MTK tables, byte for byte and entry by entry");

	cmp_floats("MTK_sin_table", MTK_sin_table, ref_MTK_sin_table, 257,
		   sizeof MTK_sin_table);
	cmp_floats("MTK_cos_table", MTK_cos_table, ref_MTK_cos_table, 257,
		   sizeof MTK_cos_table);
	cmp_floats("MTK_atan_table", MTK_atan_table, ref_MTK_atan_table, 257,
		   sizeof MTK_atan_table);
	cmp_floats("MTK_sin_sign", MTK_sin_sign, ref_MTK_sin_sign, 4,
		   sizeof MTK_sin_sign);
	cmp_floats("MTK_cos_sign", MTK_cos_sign, ref_MTK_cos_sign, 4,
		   sizeof MTK_cos_sign);

	diff_eq_int("MTK_xor_table: memcmp against the blob",
		    memcmp(MTK_xor_table, ref_MTK_xor_table,
			   sizeof MTK_xor_table) == 0, 1, 0);
	bytes_compared += (long)sizeof MTK_xor_table;
	for (i = 0; i < 256; i++) {
		diff_eq_int("MTK_xor_table[%ld]", MTK_xor_table[i],
			    ref_MTK_xor_table[i], (long)i);
		elems_compared++;
	}

	return diff_end();
}

/*
 * The two identities.  They are read off the BLOB's copies, not ours, so they
 * are a statement about the object and stay true if this file's tables were
 * ever regenerated from a formula.
 */
static int
t_identities(void)
{
	int i;
	int bits, v;

	diff_begin("the identities the blob's own tables satisfy exactly");

	for (i = 0; i <= 256; i++)
		diff_eq_float("cos[%ld] == sin[256-i]", ref_MTK_cos_table[i],
			      ref_MTK_sin_table[256 - i], (long)i);

	for (i = 0; i < 256; i++) {
		bits = 0;
		for (v = i; v; v >>= 1)
			bits += v & 1;
		diff_eq_int("xor[%ld] == popcount(i)", ref_MTK_xor_table[i],
			    bits, (long)i);
	}

	/*
	 * And the shape MTK_phasor depends on: 257 entries so that reading
	 * [i] and [i+1] is defined for every index the phasor can form, with
	 * the two ends pinned at 0 and 1.
	 */
	diff_eq_float("sin[0]", ref_MTK_sin_table[0], 0.0f, 0);
	diff_eq_float("sin[256]", ref_MTK_sin_table[256], 1.0f, 0);
	diff_eq_float("cos[0]", ref_MTK_cos_table[0], 1.0f, 0);
	diff_eq_float("cos[256]", ref_MTK_cos_table[256], 0.0f, 0);

	return diff_end();
}

int
main(void)
{
	int failed = 0;

	failed |= t_sizes();
	failed |= t_tables();
	failed |= t_identities();

	fprintf(stderr,
		"t_mtktab: compared %ld elements and %ld bytes over 6 tables\n",
		elems_compared, bytes_compared);
	if (elems_compared != 1035 || bytes_compared != 3628) {
		fprintf(stderr, "t_mtktab: denominator wrong -- expected "
				"1035 elements and 3628 bytes\n");
		failed = 1;
	}
	return failed;
}

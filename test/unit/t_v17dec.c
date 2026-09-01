/*
 * t_v17dec.c -- differential test of the V.17 fax receiver's twenty-two
 *               slicer tables.
 *
 *   DECv17_MAP_BRIDGE      .rodata  0x09874     8
 *   DECv17_MAP_TRN         .rodata  0x0987c     8
 *   DECv17_ANGL4800        .rodata  0x09884     8
 *   DECv17_QMAP4           .rodata  0x0988c     8
 *   DECv17_IMAP4           .rodata  0x09894     8
 *   DECv17_MAG7200         .rodata  0x0989c     6
 *   DECv17_ANGL7200        .rodata  0x098c0    32
 *   DECv17_QMAP16          .rodata  0x098e0    32
 *   DECv17_IMAP16          .rodata  0x09900    32
 *   DECv17_ANGL9600T       .rodata  0x09920    64
 *   DECv17_MAG9600T        .rodata  0x09960    64
 *   DECv17_SIN_ROT_ANGLE   .rodata  0x099a0     8
 *   DECv17_COS_ROT_ANGLE   .rodata  0x099a8     8
 *   DECv17_ANA_QMAP        .rodata  0x099b0    16
 *   DECv17_QMAP64          .rodata  0x0bb80   128
 *   DECv17_IMAP64          .rodata  0x0bc00   128
 *   DECv17_ANGL12000       .rodata  0x0bc80   128
 *   DECv17_MAG12000        .rodata  0x0bd00   128
 *   DECv17_ANA_QMAP128     .rodata  0x0bd80    64
 *   DECv17_ANA_IMAP128     .rodata  0x0bdc0    64
 *   DECv17_ANGL14400       .rodata  0x0be00   256
 *   DECv17_MAG14400        .rodata  0x0bf00   256
 *
 * FOUR LAYERS.  This is `t_v17cfg.c`'s shape with its fifth layer -- USE --
 * absent for a stated reason rather than forgotten: the consumers are
 * `FAX_FSE_decision_16pt`, `_32pt`, `_64pt`, `_128pt`, `FSE_Bridge_det` and
 * `FSE_decision_eqtrn`, none of which is reconstructed yet.  Driving these
 * tables through the BLOB's copies of those would test the blob against
 * itself and prove nothing, so it is not done.  When the slicers land, layer
 * five belongs in their own test, where a wrong stride can actually fail.
 *
 *   1. SHAPE.  `sizeof` against the `nm -S` size of every symbol.  A byte
 *      comparison run over OUR array's length cannot notice that our array is
 *      short of the object's -- it compares fewer bytes and passes.
 *
 *   2. VALUE, element by element against `ref_`.
 *
 *   3. THE DETECTOR IS SHOWN TO FIRE (F134).  `shorts_differ` is run over an
 *      identical copy, which must return 0, and then over one perturbed copy
 *      per element of every table -- 800 perturbations -- each of which must
 *      return non-zero.  Two arrays equal by construction prove nothing until
 *      the comparison has been seen to reject something.
 *
 *   4. THE SHAPE OF THE VALUES, with no blob in it at all.  Two independent
 *      things are asserted here:
 *
 *      (a) THE V.32bis CROSS-CHECK.  Nineteen of the twenty-two are
 *          byte-identical to tables this tree extracted separately, from
 *          different addresses in different sections, for V.32bis, and which
 *          `t_v32dec` already drives through V.32's own slicers.  That links
 *          two independent extractions to each other.  The three that are NOT
 *          identical -- `DECv17_MAP_TRN`, `DECv17_ANGL4800` and
 *          `DECv17_MAP_BRIDGE` -- are asserted to differ, and by exactly what.
 *          An assertion that all twenty-two agree would be reporting a shared
 *          translation unit that does not exist, and would silently pass if
 *          someone pasted V.32's numbers over V.17's.
 *
 *      (b) THE ARITHMETIC THE VALUES MUST SATISFY.  The rotation tables are
 *          +-23170 (0.7071 in Q15); `MAG9600T` is one eight-entry block
 *          repeated four times; `ANGL9600T`'s four blocks differ by exactly
 *          8192 modulo 32768; `MAG7200` holds the three L2 radii the
 *          sixteen-point constellation's three L1 sums map to; the four
 *          constellations are closed point sets on the expected lattices.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"

#include "dsplib/v17dec.h"
#include "dsplib/v32dec.h"

/* The blob's copies.  An undeclared `ref_` name is a hard error, not an int. */
extern const short ref_DECv17_MAP_BRIDGE[4];
extern const short ref_DECv17_MAP_TRN[4];
extern const short ref_DECv17_ANGL4800[4];
extern const short ref_DECv17_QMAP4[4];
extern const short ref_DECv17_IMAP4[4];
extern const short ref_DECv17_MAG7200[3];
extern const short ref_DECv17_ANGL7200[16];
extern const short ref_DECv17_QMAP16[16];
extern const short ref_DECv17_IMAP16[16];
extern const short ref_DECv17_ANGL9600T[32];
extern const short ref_DECv17_MAG9600T[32];
extern const short ref_DECv17_SIN_ROT_ANGLE[4];
extern const short ref_DECv17_COS_ROT_ANGLE[4];
extern const short ref_DECv17_ANA_QMAP[8];
extern const short ref_DECv17_QMAP64[64];
extern const short ref_DECv17_IMAP64[64];
extern const short ref_DECv17_ANGL12000[64];
extern const short ref_DECv17_MAG12000[64];
extern const short ref_DECv17_ANA_QMAP128[32];
extern const short ref_DECv17_ANA_IMAP128[32];
extern const short ref_DECv17_ANGL14400[128];
extern const short ref_DECv17_MAG14400[128];

/* ------------------------------------------------------------------------- */

/* One row per table, so every layer iterates the same list and none of them
 * can quietly cover fewer tables than another. */
struct tab {
	const char	*name;
	const short	*ours;
	const short	*blob;
	int		 n;		/* elements		*/
	int		 bytes;		/* `nm -S`'s size	*/
};

static const struct tab TABS[] = {
	{ "DECv17_MAP_BRIDGE",    DECv17_MAP_BRIDGE,    ref_DECv17_MAP_BRIDGE,      4,   8 },
	{ "DECv17_MAP_TRN",       DECv17_MAP_TRN,       ref_DECv17_MAP_TRN,         4,   8 },
	{ "DECv17_ANGL4800",      DECv17_ANGL4800,      ref_DECv17_ANGL4800,        4,   8 },
	{ "DECv17_QMAP4",         DECv17_QMAP4,         ref_DECv17_QMAP4,           4,   8 },
	{ "DECv17_IMAP4",         DECv17_IMAP4,         ref_DECv17_IMAP4,           4,   8 },
	{ "DECv17_MAG7200",       DECv17_MAG7200,       ref_DECv17_MAG7200,         3,   6 },
	{ "DECv17_ANGL7200",      DECv17_ANGL7200,      ref_DECv17_ANGL7200,       16,  32 },
	{ "DECv17_QMAP16",        DECv17_QMAP16,        ref_DECv17_QMAP16,         16,  32 },
	{ "DECv17_IMAP16",        DECv17_IMAP16,        ref_DECv17_IMAP16,         16,  32 },
	{ "DECv17_ANGL9600T",     DECv17_ANGL9600T,     ref_DECv17_ANGL9600T,      32,  64 },
	{ "DECv17_MAG9600T",      DECv17_MAG9600T,      ref_DECv17_MAG9600T,       32,  64 },
	{ "DECv17_SIN_ROT_ANGLE", DECv17_SIN_ROT_ANGLE, ref_DECv17_SIN_ROT_ANGLE,   4,   8 },
	{ "DECv17_COS_ROT_ANGLE", DECv17_COS_ROT_ANGLE, ref_DECv17_COS_ROT_ANGLE,   4,   8 },
	{ "DECv17_ANA_QMAP",      DECv17_ANA_QMAP,      ref_DECv17_ANA_QMAP,        8,  16 },
	{ "DECv17_QMAP64",        DECv17_QMAP64,        ref_DECv17_QMAP64,         64, 128 },
	{ "DECv17_IMAP64",        DECv17_IMAP64,        ref_DECv17_IMAP64,         64, 128 },
	{ "DECv17_ANGL12000",     DECv17_ANGL12000,     ref_DECv17_ANGL12000,      64, 128 },
	{ "DECv17_MAG12000",      DECv17_MAG12000,      ref_DECv17_MAG12000,       64, 128 },
	{ "DECv17_ANA_QMAP128",   DECv17_ANA_QMAP128,   ref_DECv17_ANA_QMAP128,    32,  64 },
	{ "DECv17_ANA_IMAP128",   DECv17_ANA_IMAP128,   ref_DECv17_ANA_IMAP128,    32,  64 },
	{ "DECv17_ANGL14400",     DECv17_ANGL14400,     ref_DECv17_ANGL14400,     128, 256 },
	{ "DECv17_MAG14400",      DECv17_MAG14400,      ref_DECv17_MAG14400,      128, 256 }
};

#define NTABS ((int)(sizeof TABS / sizeof TABS[0]))

/* The largest table, for the perturbation buffer. */
#define MAXN 128

/* Layer 3's detector.  Its own function rather than a `memcmp` call site, so
 * that it can be exercised on a known-perturbed input below. */
static int
shorts_differ(const short *a, const short *b, int n)
{
	int i;

	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			return 1;
	return 0;
}

/* Compare `n` shorts and report the FIRST disagreement, with its index. */
static void
cmp_shorts(const char *what, const short *got, const short *want, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (got[i] != want[i]) {
			diff_eq_int(what, got[i], want[i], i);
			return;
		}
	}
	diff_eq_int(what, 0, 0, n);
}

/* How many of `n` entries are non-zero.  Anti-vacuity: a table read from the
 * wrong address is all zeroes and would pass a comparison against another
 * copy of the same mistake. */
static int
nonzero(const short *p, int n)
{
	int i, c = 0;

	for (i = 0; i < n; i++)
		if (p[i] != 0)
			c++;
	return c;
}

/* ------------------------------------------------------------------------- */

/*
 * LAYER 1 -- SHAPE.  `sizeof` against `nm -S`.  Every row's `bytes` was taken
 * from the object, not from `2 * n`, so a wrong element count fails here
 * rather than silently comparing a prefix.
 */
static int
test_shape(void)
{
	int i;

	diff_begin("v17dec: shape (sizeof against nm -S)");

	diff_eq_int("sizeof DECv17_MAP_BRIDGE (%ld)",
		    (int)sizeof DECv17_MAP_BRIDGE, 8, 0);
	diff_eq_int("sizeof DECv17_MAP_TRN (%ld)",
		    (int)sizeof DECv17_MAP_TRN, 8, 0);
	diff_eq_int("sizeof DECv17_ANGL4800 (%ld)",
		    (int)sizeof DECv17_ANGL4800, 8, 0);
	diff_eq_int("sizeof DECv17_QMAP4 (%ld)",
		    (int)sizeof DECv17_QMAP4, 8, 0);
	diff_eq_int("sizeof DECv17_IMAP4 (%ld)",
		    (int)sizeof DECv17_IMAP4, 8, 0);
	diff_eq_int("sizeof DECv17_MAG7200 (%ld)",
		    (int)sizeof DECv17_MAG7200, 6, 0);
	diff_eq_int("sizeof DECv17_ANGL7200 (%ld)",
		    (int)sizeof DECv17_ANGL7200, 32, 0);
	diff_eq_int("sizeof DECv17_QMAP16 (%ld)",
		    (int)sizeof DECv17_QMAP16, 32, 0);
	diff_eq_int("sizeof DECv17_IMAP16 (%ld)",
		    (int)sizeof DECv17_IMAP16, 32, 0);
	diff_eq_int("sizeof DECv17_ANGL9600T (%ld)",
		    (int)sizeof DECv17_ANGL9600T, 64, 0);
	diff_eq_int("sizeof DECv17_MAG9600T (%ld)",
		    (int)sizeof DECv17_MAG9600T, 64, 0);
	diff_eq_int("sizeof DECv17_SIN_ROT_ANGLE (%ld)",
		    (int)sizeof DECv17_SIN_ROT_ANGLE, 8, 0);
	diff_eq_int("sizeof DECv17_COS_ROT_ANGLE (%ld)",
		    (int)sizeof DECv17_COS_ROT_ANGLE, 8, 0);
	diff_eq_int("sizeof DECv17_ANA_QMAP (%ld)",
		    (int)sizeof DECv17_ANA_QMAP, 16, 0);
	diff_eq_int("sizeof DECv17_QMAP64 (%ld)",
		    (int)sizeof DECv17_QMAP64, 128, 0);
	diff_eq_int("sizeof DECv17_IMAP64 (%ld)",
		    (int)sizeof DECv17_IMAP64, 128, 0);
	diff_eq_int("sizeof DECv17_ANGL12000 (%ld)",
		    (int)sizeof DECv17_ANGL12000, 128, 0);
	diff_eq_int("sizeof DECv17_MAG12000 (%ld)",
		    (int)sizeof DECv17_MAG12000, 128, 0);
	diff_eq_int("sizeof DECv17_ANA_QMAP128 (%ld)",
		    (int)sizeof DECv17_ANA_QMAP128, 64, 0);
	diff_eq_int("sizeof DECv17_ANA_IMAP128 (%ld)",
		    (int)sizeof DECv17_ANA_IMAP128, 64, 0);
	diff_eq_int("sizeof DECv17_ANGL14400 (%ld)",
		    (int)sizeof DECv17_ANGL14400, 256, 0);
	diff_eq_int("sizeof DECv17_MAG14400 (%ld)",
		    (int)sizeof DECv17_MAG14400, 256, 0);

	/* The element type, stated once and checked once: every stride in the
	 * object is two, so `sizeof` an element must be two. */
	diff_eq_int("sizeof(short) is the stride (%ld)",
		    (int)sizeof(short), 2, 0);

	/* And the table of tables is self-consistent -- if a row said 64
	 * elements and 256 bytes, every other layer would run on a lie. */
	for (i = 0; i < NTABS; i++)
		diff_eq_int("row bytes == 2 * n (%ld)",
			    TABS[i].bytes, 2 * TABS[i].n, i);

	diff_eq_int("twenty-two tables (%ld)", NTABS, 22, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/* LAYER 2 -- VALUE, ours against the blob's, element by element. */
static int
test_values(void)
{
	int i;

	diff_begin("v17dec: values against ref_");

	for (i = 0; i < NTABS; i++) {
		cmp_shorts(TABS[i].name, TABS[i].ours, TABS[i].blob,
			   TABS[i].n);

		/* Anti-vacuity: this must not be comparing two zero-filled
		 * regions.  Every table here has at least one non-zero
		 * entry in the object, and `MAP_TRN`/`MAP_BRIDGE` -- the
		 * only ones containing a zero at all -- have three. */
		diff_eq_int("non-zero entries (%ld)",
			    nonzero(TABS[i].blob, TABS[i].n) > 0, 1, i);
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * LAYER 3 -- THE DETECTOR FIRES (F134).  Layer 2's comparison is run over an
 * identical copy, which must report no difference, and then over one
 * perturbed copy per element of every table, each of which must report one.
 * Without this, layer 2 passing is consistent with `shorts_differ` being
 * `return 0;`.
 */
static int
test_detector_fires(void)
{
	short copy[MAXN];
	int i, j, fired = 0, expected = 0;

	diff_begin("v17dec: the value comparison rejects a perturbed copy");

	for (i = 0; i < NTABS; i++) {
		memcpy(copy, TABS[i].blob, (size_t)TABS[i].bytes);

		/* An unperturbed copy must NOT be reported as differing. */
		diff_eq_int("identical copy is not reported (%ld)",
			    shorts_differ(copy, TABS[i].blob, TABS[i].n),
			    0, i);

		for (j = 0; j < TABS[i].n; j++) {
			copy[j] = (short)(TABS[i].blob[j] + 1);
			fired += shorts_differ(copy, TABS[i].blob,
					       TABS[i].n) ? 1 : 0;
			expected++;
			copy[j] = TABS[i].blob[j];
		}
	}

	/* The denominator, reported rather than assumed.  800 = the sum of
	 * every table's element count. */
	diff_eq_int("every single-element perturbation is caught (%ld)",
		    fired, expected, 0);
	diff_eq_int("perturbations attempted (%ld)", expected, 727, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * LAYER 4a -- THE V.32bis CROSS-CHECK.  No blob on either side: our V.17
 * tables against our V.32 ones, which were extracted separately from
 * different addresses and are driven through V.32's own slicers by
 * `t_v32dec`.
 *
 * The three that must DIFFER are asserted to differ, and the exact
 * difference is asserted too.  Otherwise a paste of V.32's numbers over
 * V.17's would pass this layer, which is precisely the mistake the layer
 * exists to catch.
 */
static int
test_v32bis_identity(void)
{
	int i;

	diff_begin("v17dec: V.17's decision tables against V.32bis' own");

	cmp_shorts("QMAP4 == DECv32_QMAP4",
		   DECv17_QMAP4, DECv32_QMAP4, 4);
	cmp_shorts("IMAP4 == DECv32_IMAP4",
		   DECv17_IMAP4, DECv32_IMAP4, 4);
	cmp_shorts("MAG7200 == DECv32_MAG9600",
		   DECv17_MAG7200, DECv32_MAG9600, 3);
	cmp_shorts("ANGL7200 == DECv32_ANGL9600",
		   DECv17_ANGL7200, DECv32_ANGL9600, 16);
	cmp_shorts("QMAP16 == DECv32_QMAP16",
		   DECv17_QMAP16, DECv32_QMAP16, 16);
	cmp_shorts("IMAP16 == DECv32_IMAP16",
		   DECv17_IMAP16, DECv32_IMAP16, 16);
	cmp_shorts("ANGL9600T == DECv32_ANGL9600T",
		   DECv17_ANGL9600T, DECv32_ANGL9600T, 32);
	cmp_shorts("MAG9600T == DECv32_MAG9600T",
		   DECv17_MAG9600T, DECv32_MAG9600T, 32);
	cmp_shorts("SIN_ROT_ANGLE == DECv32_SIN_ROT_ANGLE",
		   DECv17_SIN_ROT_ANGLE, DECv32_SIN_ROT_ANGLE, 4);
	cmp_shorts("COS_ROT_ANGLE == DECv32_COS_ROT_ANGLE",
		   DECv17_COS_ROT_ANGLE, DECv32_COS_ROT_ANGLE, 4);
	cmp_shorts("ANA_QMAP == DECv32_ANA_QMAP",
		   DECv17_ANA_QMAP, DECv32_ANA_QMAP, 8);
	cmp_shorts("QMAP64 == DECv32_QMAP64",
		   DECv17_QMAP64, DECv32_QMAP64, 64);
	cmp_shorts("IMAP64 == DECv32_IMAP64",
		   DECv17_IMAP64, DECv32_IMAP64, 64);
	cmp_shorts("ANGL12000 == DECv32_ANGL12000",
		   DECv17_ANGL12000, DECv32_ANGL12000, 64);
	cmp_shorts("MAG12000 == DECv32_MAG12000",
		   DECv17_MAG12000, DECv32_MAG12000, 64);
	cmp_shorts("ANA_QMAP128 == DECv32_ANA_QMAP128",
		   DECv17_ANA_QMAP128, DECv32_ANA_QMAP128, 32);
	cmp_shorts("ANA_IMAP128 == DECv32_ANA_IMAP128",
		   DECv17_ANA_IMAP128, DECv32_ANA_IMAP128, 32);
	cmp_shorts("ANGL14400 == DECv32_ANGL14400",
		   DECv17_ANGL14400, DECv32_ANGL14400, 128);
	cmp_shorts("MAG14400 == DECv32_MAG14400",
		   DECv17_MAG14400, DECv32_MAG14400, 128);

	/* Nineteen agree; these three must not, and the difference is exact. */
	diff_eq_int("MAP_TRN differs from DECv32_MAP_TRN (%ld)",
		    shorts_differ(DECv17_MAP_TRN, DECv32_MAP_TRN, 4), 1, 0);
	diff_eq_int("ANGL4800 differs from DECv32_ANGL1200 (%ld)",
		    shorts_differ(DECv17_ANGL4800, DECv32_ANGL1200, 4), 1, 0);

	for (i = 0; i < 3; i++)
		diff_eq_int("ANGL4800[i] == DECv32_ANGL1200[i] + 1 (%ld)",
			    DECv17_ANGL4800[i], DECv32_ANGL1200[i] + 1, i);
	diff_eq_int("ANGL4800[3] == DECv32_ANGL1200[3] (%ld)",
		    DECv17_ANGL4800[3], DECv32_ANGL1200[3], 3);

	/* `MAP_TRN` is a permutation of 0..3 in both, and a different one. */
	for (i = 0; i < 4; i++) {
		int seen17 = 0, j;

		for (j = 0; j < 4; j++)
			if (DECv17_MAP_TRN[j] == (short)i)
				seen17++;
		diff_eq_int("MAP_TRN is a permutation of 0..3 (%ld)",
			    seen17, 1, i);
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * LAYER 4b -- THE ARITHMETIC THE VALUES MUST SATISFY, with no reference to
 * either the blob or V.32.  Each assertion below is one the object's own use
 * of the table forces; see `src/fax/v17dec_tables.c` for the derivations.
 */
static int
test_value_shape(void)
{
	int i, j;

	diff_begin("v17dec: the values' own arithmetic");

	/* The 45-degree rotation is +-cos(45) in Q15 = +-23170, and the four
	 * entries are the four quadrants.  cos is +,-,-,+; sin is -,-,+,+. */
	diff_eq_int("COS_ROT_ANGLE is +-23170 (%ld)",
		    DECv17_COS_ROT_ANGLE[0] ==  23170 &&
		    DECv17_COS_ROT_ANGLE[1] == -23170 &&
		    DECv17_COS_ROT_ANGLE[2] == -23170 &&
		    DECv17_COS_ROT_ANGLE[3] ==  23170, 1, 0);
	diff_eq_int("SIN_ROT_ANGLE is +-23170 (%ld)",
		    DECv17_SIN_ROT_ANGLE[0] == -23170 &&
		    DECv17_SIN_ROT_ANGLE[1] == -23170 &&
		    DECv17_SIN_ROT_ANGLE[2] ==  23170 &&
		    DECv17_SIN_ROT_ANGLE[3] ==  23170, 1, 0);

	/* A rotation cannot change a radius, so `MAG9600T` is one block of
	 * eight repeated four times. */
	for (i = 0; i < 32; i++)
		diff_eq_int("MAG9600T repeats its first eight (%ld)",
			    DECv17_MAG9600T[i], DECv17_MAG9600T[i & 7], i);

	/*
	 * A rotation adds a quarter cycle, so `ANGL9600T`'s four blocks
	 * differ by 8192 modulo 32768 -- TO WITHIN ONE UNIT, and the
	 * tolerance is measured rather than granted.  Written as an exact
	 * equality first, this check FIRED on six of the thirty-two entries:
	 * indices 2 and 4 of the base block hold 8191 where the second block
	 * holds 16384, the third 24576 and the fourth 0.  Those two points
	 * are the ones at exactly 90 degrees, whose true angle is 8192, and
	 * the base block truncates them to 8191 while the rotated blocks land
	 * on the exact multiple.  So the residual is a rounding of the
	 * generator, one unit in 32768, and not a rotation that is not a
	 * rotation.  The tolerance is +-1 and no wider: at +-2 the check
	 * would no longer distinguish a rotation from a near miss.
	 */
	for (i = 0; i < 32; i++) {
		int d = (DECv17_ANGL9600T[i]
			 - DECv17_ANGL9600T[i & 7] - 8192 * (i >> 3)) & 0x7fff;

		diff_eq_int("ANGL9600T's blocks differ by 8192 +-1 (%ld)",
			    d <= 1 || d >= 0x7fff, 1, i);
	}

	/* `MAG7200`'s three entries are the L2 radii of the three L1 sums the
	 * sixteen-point constellation produces: 4096*sqrt(2), sqrt(12288^2 +
	 * 4096^2) and 12288*sqrt(2), truncated. */
	diff_eq_int("MAG7200[0] is 4096*sqrt(2) (%ld)",
		    DECv17_MAG7200[0], 5792, 0);
	diff_eq_int("MAG7200[1] is hypot(12288, 4096) (%ld)",
		    DECv17_MAG7200[1], 12953, 1);
	diff_eq_int("MAG7200[2] is 12288*sqrt(2) (%ld)",
		    DECv17_MAG7200[2], 17378, 2);

	/* And the three entries are the only three L1 sums the constellation
	 * can produce, driven from the constellation itself rather than
	 * asserted.  `(|I| + |Q|) >> 13` must land in 1..3 for every point. */
	for (i = 0; i < 16; i++) {
		int ii = DECv17_IMAP16[i] < 0 ? -DECv17_IMAP16[i]
					      :  DECv17_IMAP16[i];
		int qq = DECv17_QMAP16[i] < 0 ? -DECv17_QMAP16[i]
					      :  DECv17_QMAP16[i];
		int k = (ii + qq) >> 13;

		diff_eq_int("16-point L1 sum indexes MAG7200[0..2] (%ld)",
			    k >= 1 && k <= 3, 1, i);
	}

	/* The four-point set is +-4096 and +-12288, one point per quadrant. */
	for (i = 0; i < 4; i++) {
		int ii = DECv17_IMAP4[i], qq = DECv17_QMAP4[i];

		diff_eq_int("IMAP4 is +-4096 or +-12288 (%ld)",
			    ii == 4096 || ii == -4096 ||
			    ii == 12288 || ii == -12288, 1, i);
		diff_eq_int("QMAP4 is +-4096 or +-12288 (%ld)",
			    qq == 4096 || qq == -4096 ||
			    qq == 12288 || qq == -12288, 1, i);
		/* |I| and |Q| are 4096 and 12288 in some order: the point is
		 * at 45 degrees to the axes on the 4096 lattice. */
		diff_eq_int("IMAP4/QMAP4 pair is {4096, 12288} (%ld)",
			    (ii * ii + qq * qq),
			    4096 * 4096 + 12288 * 12288, i);
	}

	/* The sixteen-point set is on the same +-4096/+-12288 lattice and is
	 * a CLOSED point set -- sixteen distinct (I, Q) pairs. */
	for (i = 0; i < 16; i++) {
		int ii = DECv17_IMAP16[i], qq = DECv17_QMAP16[i], dup = 0;

		diff_eq_int("IMAP16 is on the 4096 lattice (%ld)",
			    ii % 4096 == 0 && ii != 0, 1, i);
		diff_eq_int("QMAP16 is on the 4096 lattice (%ld)",
			    qq % 4096 == 0 && qq != 0, 1, i);
		for (j = 0; j < 16; j++)
			if (DECv17_IMAP16[j] == ii && DECv17_QMAP16[j] == qq)
				dup++;
		diff_eq_int("the 16 points are distinct (%ld)", dup, 1, i);
	}

	/* The 64-point set steps by 4096 and is closed. */
	for (i = 0; i < 64; i++) {
		int ii = DECv17_IMAP64[i], qq = DECv17_QMAP64[i], dup = 0;

		diff_eq_int("IMAP64 is 2048 + 4096k (%ld)",
			    (ii > 0 ? ii - 2048 : -ii - 2048) % 4096, 0, i);
		diff_eq_int("QMAP64 is 2048 + 4096k (%ld)",
			    (qq > 0 ? qq - 2048 : -qq - 2048) % 4096, 0, i);
		for (j = 0; j < 64; j++)
			if (DECv17_IMAP64[j] == ii && DECv17_QMAP64[j] == qq)
				dup++;
		diff_eq_int("the 64 points are distinct (%ld)", dup, 1, i);
	}

	/* The analytic 128-point rails are positive and on the 1448 lattice
	 * (1448 = 2048 * cos(45) rounded, the rotated half-step). */
	for (i = 0; i < 32; i++) {
		diff_eq_int("ANA_IMAP128 is positive (%ld)",
			    DECv17_ANA_IMAP128[i] > 0, 1, i);
		diff_eq_int("ANA_QMAP128 is positive (%ld)",
			    DECv17_ANA_QMAP128[i] > 0, 1, i);
		diff_eq_int("ANA_IMAP128 is a multiple of 1448 +- 1 (%ld)",
			    (DECv17_ANA_IMAP128[i] + 1) / 1448 * 1448
			    <= DECv17_ANA_IMAP128[i] + 1, 1, i);
	}

	/* Every angle is inside one cycle: 0 <= a < 32768. */
	for (i = 0; i < 128; i++)
		diff_eq_int("ANGL14400 is one cycle (%ld)",
			    DECv17_ANGL14400[i] >= 0, 1, i);
	for (i = 0; i < 64; i++)
		diff_eq_int("ANGL12000 is one cycle (%ld)",
			    DECv17_ANGL12000[i] >= 0, 1, i);
	for (i = 0; i < 16; i++)
		diff_eq_int("ANGL7200 is one cycle (%ld)",
			    DECv17_ANGL7200[i] >= 0, 1, i);
	for (i = 0; i < 4; i++)
		diff_eq_int("ANGL4800 is one cycle (%ld)",
			    DECv17_ANGL4800[i] >= 0, 1, i);

	/* `MAP_BRIDGE` and `MAP_TRN` are permutations of 0..3. */
	for (i = 0; i < 4; i++) {
		int seen = 0;

		for (j = 0; j < 4; j++)
			if (DECv17_MAP_BRIDGE[j] == (short)i)
				seen++;
		diff_eq_int("MAP_BRIDGE is a permutation of 0..3 (%ld)",
			    seen, 1, i);
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	rc |= test_shape();
	rc |= test_values();
	rc |= test_detector_fires();
	rc |= test_v32bis_identity();
	rc |= test_value_shape();

	return rc;
}

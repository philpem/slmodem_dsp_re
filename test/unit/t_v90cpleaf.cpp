/*
 * t_v90cpleaf.cpp -- differential test of the seven V90CP members that are
 * leaves: `getBitVector`, `resetDetector`, `reset`, `resetCRC`,
 * `calcSequenceLength`, `printNofRecievedMpMpNot` and `calcCRC`.
 *
 * NOTHING HERE ALLOCATES, so unlike t_v90cp and t_v90cpinfo the two objects
 * can be seeded byte for byte identically and compared whole -- the six heap
 * pointers hold the same garbage on both sides because neither side is asked
 * to dereference them.  That is checked rather than assumed: every run ends
 * by comparing all 0x3bc0 bytes and the 64-byte guard past them.
 *
 * WHAT EACH RUN HAS TO SEPARATE, because a differential test that only
 * compares two objects passes for a body that is wrong in the same way on
 * both sides -- it cannot be, here, since one side is the blob, but it CAN
 * pass for a body no input ever reaches:
 *
 *   calcCRC       THE REGISTER IS CARRIED IN, NOT SEEDED.  The object holds
 *                 no store of 1 anywhere in it.  `run_cp_calccrc` runs the
 *                 same bit vector twice from two different `crc[]` states and
 *                 requires the two results to DIFFER, which is an observable
 *                 the mutation "seed the register the way resetCRC does"
 *                 cannot survive.  It also flips one bit at a multiple of
 *                 seventeen and requires NO change, then one that is not and
 *                 requires a change: that pair is what kills "drop the frame
 *                 skip", and neither half proves it alone.
 *
 *   reset         IT IS NOT THE CONSTRUCTOR.  `byte_13` is seeded non-zero
 *                 and read back non-zero afterwards, which is the one field
 *                 that separates the two bodies (V90CP.h, +0x0013).
 *
 *   resetDetector IT IS NOT `reset`.  The three fields `reset` adds --
 *                 +0x3bb4, +0x3bb8, +0x3bbc -- are seeded non-zero and
 *                 required to still hold the seed afterwards.
 *
 *   calcSeqLength BOTH ARMS.  The group sizes are swept so that some trials
 *                 divide exactly and some do not, and the two are counted
 *                 separately; the equal arm and the rounding arm store
 *                 different values, so a body with only one of them fails.
 *
 * THE DIVISOR IS NEVER ZERO.  `calcSequenceLength` is a `div` with no guard,
 * so a zero +0x3ba8 raises #DE and takes both sides down together -- the same
 * shape as D390, a blob behaviour that is not a difference and that no test
 * could report.  The sweep therefore excludes it and says so here.
 *
 * THE BIT VECTOR IS KEPT INSIDE THE OBJECT.  `calcCRC` walks from 0x12 to
 * `word_3bb0 - 0x11`, unsigned, so a `word_3bb0` below 0x11 wraps the bound
 * to near 2^32 and both sides run off the end.  The sweep keeps `word_3bb0`
 * inside [0x11, 2000], which is well inside the 12000-byte vector.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90CP.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

/*
 * `unsigned int *` where the member takes `unsigned int &`: the same thing to
 * the ABI, and this side has to name a C type because the blob's symbol has
 * no class to be a member of.
 */
unsigned char *ref_cp_getbitvector(void *, unsigned int *)
	asm("ref__ZN5V90CP12getBitVectorERj");

void ref_cp_reset(void *) asm("ref__ZN5V90CP5resetEv");
void ref_cp_resetdetector(void *) asm("ref__ZN5V90CP13resetDetectorEv");
void ref_cp_resetcrc(void *) asm("ref__ZN5V90CP8resetCRCEv");
void ref_cp_calcseqlen(void *) asm("ref__ZN5V90CP18calcSequenceLengthEv");
void ref_cp_calccrc(void *) asm("ref__ZN5V90CP7calcCRCEv");
void ref_cp_printnof(void *) asm("ref__ZN5V90CP23printNofRecievedMpMpNotEv");
}

/* ------------------------------------------------------------------ seeds */

static unsigned lfsr;

static unsigned char
next_byte(int mode)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	switch (mode) {
	case 1:
		return 0x01;			/* every byte a live bit     */
	case 2:
		return 0xff;			/* every bit set             */
	default:
		return (unsigned char)(lfsr >> 3);
	}
}

static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/* ----------------------------------------------------------- the storage */

#define CP_SLOT	((unsigned)sizeof(V90CP) + 64u)

static unsigned char cp_a[CP_SLOT] __attribute__((aligned(8)));
static unsigned char cp_b[CP_SLOT] __attribute__((aligned(8)));
static unsigned char cp_s[CP_SLOT];		/* the seed, for the guard  */

#define CPA	((V90CP *)cp_a)
#define CPB	((V90CP *)cp_b)

/*
 * The same varied bytes into both sides.  Never zeros -- finding F230 -- and
 * no pointer is re-installed afterwards, because nothing under test reads one.
 */
static void
seed_pair(int trial, int mode)
{
	unsigned i;

	lfsr = 0x2f19u + 0x9e37u * (unsigned)trial + 0x51edu * (unsigned)mode;
	for (i = 0; i < CP_SLOT; i++) {
		unsigned char v = next_byte(mode);

		cp_a[i] = v;
		cp_b[i] = v;
		cp_s[i] = v;
	}
}

/* Compare the whole object and the guard past it, on both sides. */
static void
compare_pair(const char *what, long tag)
{
	unsigned n = CP_SLOT - (unsigned)sizeof(V90CP);

	diff_eq_obj_(__FILE__, __LINE__, what, "V90CP", cp_a, cp_b,
		     sizeof(V90CP), tag);
	diff_eq_int("ours stored past the object (%ld)",
		    memcmp(cp_a + sizeof(V90CP), cp_s + sizeof(V90CP), n) == 0,
		    1, tag);
	diff_eq_int("the blob stored past the object (%ld)",
		    memcmp(cp_b + sizeof(V90CP), cp_s + sizeof(V90CP), n) == 0,
		    1, tag);
}

/* --------------------------------------------- getBitVector (22 bytes) */

static int
run_cp_getbitvector(void)
{
	static const unsigned int lens[] = {
		0u, 1u, 17u, 0x22u, 0x80u, 0xffffu, 0x7fffffffu, 0xffffffffu
	};
	int trial, varied = 0, high_seen = 0;
	unsigned int first_len = 0;

	diff_begin("V90CP::getBitVector");
	set_level(0);

	for (trial = 0; trial < 24; trial++) {
		unsigned char before_a[CP_SLOT], before_b[CP_SLOT];
		long tag = 2000 + trial;
		unsigned char *ra, *rb;
		unsigned int la, lb;
		unsigned int want =
		    lens[(unsigned)trial % (sizeof(lens) / sizeof(lens[0]))];

		seed_pair(trial + 300, trial % 3);
		CPA->word_3bac = CPB->word_3bac = want;
		if (want >= 0x80000000u)
			high_seen = 1;
		memcpy(before_a, cp_a, CP_SLOT);
		memcpy(before_b, cp_b, CP_SLOT);

		/*
		 * THE OUT PARAMETERS ARE SEEDED AND NOT ZEROED, with DIFFERENT
		 * values, so a call that wrote nothing cannot pass by leaving
		 * two matching zeros behind.
		 */
		la = 0xa5a5a5a5u;
		lb = 0x5a5a5a5au;

		ra = CPA->getBitVector(la);
		rb = ref_cp_getbitvector(cp_b, &lb);

		diff_eq_int("the length matches (%ld)", (long)la, (long)lb,
			    tag);
		diff_eq_int("the length is +0x3bac (%ld)", (long)lb,
			    (long)want, tag);
		diff_eq_int("ours returned this+0x%lx",
			    (long)(ra - cp_a), 0xcb8, tag);
		diff_eq_int("the blob returned this+0x%lx",
			    (long)(rb - cp_b), 0xcb8, tag);
		diff_eq_int("and that is &bits[0] (%ld)",
			    (long)((unsigned char *)CPB->bits - cp_b), 0xcb8,
			    tag);

		/* Twenty-two bytes and no store: the object cannot move. */
		diff_eq_int("ours wrote nothing (%ld)",
			    memcmp(before_a, cp_a, CP_SLOT) == 0, 1, tag);
		diff_eq_int("the blob wrote nothing (%ld)",
			    memcmp(before_b, cp_b, CP_SLOT) == 0, 1, tag);

		if (trial == 0)
			first_len = lb;
		else if (lb != first_len)
			varied = 1;
	}

	diff_eq_int("a length with the top bit set was tried", high_seen, 1, 0);
	diff_eq_int("the length varied between trials", varied, 1, 0);
	return diff_end();
}

/* ------------------------------------------- resetDetector (46 bytes) */

static int
run_cp_resetdetector(void)
{
	int trial, moved = 0;

	diff_begin("V90CP::resetDetector");
	set_level(0);

	for (trial = 0; trial < 24; trial++) {
		unsigned char before[CP_SLOT];
		long tag = 2200 + trial;

		seed_pair(trial + 400, trial % 3);

		/*
		 * The three fields `reset` adds and this one does not, seeded
		 * to values no reset writes, so "it is not `reset`" is
		 * answerable from the object afterwards.
		 */
		CPA->nofRecievedMp = CPB->nofRecievedMp = 0x1234 + trial;
		CPA->nofRecievedMpNot = CPB->nofRecievedMpNot = 0x5678 + trial;
		CPA->word_3bbc = CPB->word_3bbc = 0x2a2a2a2a;
		CPA->byte_13 = CPB->byte_13 = (unsigned char)(0x41 + trial);
		memcpy(before, cp_b, CP_SLOT);

		CPA->resetDetector();
		ref_cp_resetdetector(cp_b);

		compare_pair("after resetDetector", tag);

		if (memcmp(before, cp_b, sizeof(V90CP)) != 0)
			moved = 1;

		/* Read off the BLOB: the five it writes, and the four it does not. */
		diff_eq_int("+0xcac is 18 (%ld)", (long)CPB->word_cac, 18, tag);
		diff_eq_int("+0xcb0 is zero (%ld)", (long)CPB->word_cb0, 0,
			    tag);
		diff_eq_int("+0xca4 is zero (%ld)", (long)CPB->word_ca4, 0,
			    tag);
		diff_eq_int("+0xca9 is zero (%ld)", (long)CPB->byte_ca9, 0,
			    tag);
		diff_eq_int("+0xcaa is zero (%ld)", (long)CPB->byte_caa, 0,
			    tag);

		diff_eq_int("it left +0x3bb4 alone (%ld)",
			    (long)CPB->nofRecievedMp, 0x1234 + trial, tag);
		diff_eq_int("it left +0x3bb8 alone (%ld)",
			    (long)CPB->nofRecievedMpNot, 0x5678 + trial, tag);
		diff_eq_int("it left +0x3bbc alone (%ld)",
			    (long)CPB->word_3bbc, 0x2a2a2a2a, tag);
		diff_eq_int("it left +0x13 alone (%ld)", (long)CPB->byte_13,
			    0x41 + trial, tag);
	}

	diff_eq_int("resetDetector changed the object", moved, 1, 0);
	return diff_end();
}

/* --------------------------------------------------- reset (73 bytes) */

static int
run_cp_reset(void)
{
	int trial, moved = 0;

	diff_begin("V90CP::reset");
	set_level(0);

	for (trial = 0; trial < 24; trial++) {
		unsigned char before[CP_SLOT];
		long tag = 2400 + trial;

		seed_pair(trial + 500, trial % 3);
		CPA->nofRecievedMp = CPB->nofRecievedMp = 0x1234 + trial;
		CPA->nofRecievedMpNot = CPB->nofRecievedMpNot = 0x5678 + trial;
		CPA->word_3bbc = CPB->word_3bbc = 0x2a2a2a2a;

		/* NOT the constructor: +0x13 has to survive. */
		CPA->byte_13 = CPB->byte_13 = (unsigned char)(0x41 + trial);
		memcpy(before, cp_b, CP_SLOT);

		CPA->reset();
		ref_cp_reset(cp_b);

		compare_pair("after reset", tag);

		if (memcmp(before, cp_b, sizeof(V90CP)) != 0)
			moved = 1;

		diff_eq_int("+0xcac is 18 (%ld)", (long)CPB->word_cac, 18, tag);
		diff_eq_int("+0xcb0 is zero (%ld)", (long)CPB->word_cb0, 0,
			    tag);
		diff_eq_int("+0xca4 is zero (%ld)", (long)CPB->word_ca4, 0,
			    tag);
		diff_eq_int("+0xca9 is zero (%ld)", (long)CPB->byte_ca9, 0,
			    tag);
		diff_eq_int("+0xcaa is zero (%ld)", (long)CPB->byte_caa, 0,
			    tag);
		diff_eq_int("the MP counter is zero (%ld)",
			    (long)CPB->nofRecievedMp, 0, tag);
		diff_eq_int("the MPNot counter is zero (%ld)",
			    (long)CPB->nofRecievedMpNot, 0, tag);
		diff_eq_int("+0x3bbc is -1 (%ld)", (long)CPB->word_3bbc, -1,
			    tag);

		/*
		 * THE ONE THAT SEPARATES IT FROM THE CONSTRUCTOR, read off the
		 * blob rather than restated from our source.
		 */
		diff_eq_int("it left +0x13 alone (%ld)", (long)CPB->byte_13,
			    0x41 + trial, tag);
	}

	diff_eq_int("reset changed the object", moved, 1, 0);
	return diff_end();
}

/* ------------------------------------------------ resetCRC (32 bytes) */

static int
run_cp_resetcrc(void)
{
	int trial, moved = 0;

	diff_begin("V90CP::resetCRC");
	set_level(0);

	for (trial = 0; trial < 16; trial++) {
		unsigned char before[CP_SLOT];
		long tag = 2600 + trial;
		int i, ones = 0;

		seed_pair(trial + 600, trial % 3);
		memcpy(before, cp_b, CP_SLOT);

		CPA->resetCRC();
		ref_cp_resetcrc(cp_b);

		compare_pair("after resetCRC", tag);

		if (memcmp(before, cp_b, sizeof(V90CP)) != 0)
			moved = 1;

		for (i = 0; i < V90CP_CRC; i++)
			if (CPB->crc[i] == 1)
				ones++;
		diff_eq_int("all sixteen are one (%ld)", ones, V90CP_CRC, tag);

		/*
		 * Sixteen and not seventeen: the byte after the register is
		 * +0x3ba8, which `calcSequenceLength` owns.  Read as the low
		 * byte of the seed, on the blob's side.
		 */
		diff_eq_int("the seventeenth byte is untouched (%ld)",
			    (long)cp_b[0x3ba8], (long)cp_s[0x3ba8], tag);
	}

	diff_eq_int("resetCRC changed the object", moved, 1, 0);
	return diff_end();
}

/* -------------------------------------- calcSequenceLength (101 bytes) */

static int
run_cp_calcseqlen(void)
{
	/* Never zero: a zero divisor is an unguarded `div`.  See the top. */
	static const unsigned int groups[] = {
		1u, 2u, 5u, 8u, 16u, 17u, 34u, 0xffffffffu
	};
	static const unsigned int lens[] = {
		0u, 1u, 16u, 33u, 0x22u, 0x99u, 1000u, 0xfffffffeu
	};
	int trial, exact = 0, rounded = 0, moved = 0;

	diff_begin("V90CP::calcSequenceLength");
	set_level(0);

	for (trial = 0; trial < 64; trial++) {
		unsigned char before[CP_SLOT];
		long tag = 2800 + trial;
		unsigned int g = groups[(unsigned)trial %
					(sizeof(groups) / sizeof(groups[0]))];
		unsigned int n = lens[((unsigned)trial / 8u) %
				      (sizeof(lens) / sizeof(lens[0]))];

		seed_pair(trial + 700, trial % 3);
		CPA->word_3ba8 = CPB->word_3ba8 = g;
		CPA->word_3bb0 = CPB->word_3bb0 = n;
		memcpy(before, cp_b, CP_SLOT);

		CPA->calcSequenceLength();
		ref_cp_calcseqlen(cp_b);

		compare_pair("after calcSequenceLength", tag);

		if (memcmp(before, cp_b, sizeof(V90CP)) != 0)
			moved = 1;

		/*
		 * Which arm the BLOB took, decided from its inputs and not
		 * from our source, and the two arms store different things.
		 */
		if ((n + 1u) % g == 0u) {
			exact = 1;
			diff_eq_int("the exact arm stored n+1 (%ld)",
				    (long)CPB->word_3bac, (long)(n + 1u), tag);
		} else {
			rounded = 1;
			diff_eq_int("the rounding arm rounded up (%ld)",
				    (long)CPB->word_3bac,
				    (long)(((n + 1u) / g + 1u) * g), tag);
			diff_eq_int("and that is not n+1 (%ld)",
				    CPB->word_3bac != n + 1u, 1, tag);
		}

		/* It writes one word and nothing else. */
		diff_eq_int("+0x3ba8 is untouched (%ld)",
			    (long)CPB->word_3ba8, (long)g, tag);
		diff_eq_int("+0x3bb0 is untouched (%ld)",
			    (long)CPB->word_3bb0, (long)n, tag);
	}

	diff_eq_int("the exact arm was taken", exact, 1, 0);
	diff_eq_int("the rounding arm was taken", rounded, 1, 0);
	diff_eq_int("calcSequenceLength changed the object", moved, 1, 0);
	return diff_end();
}

/* ------------------------------------------------- calcCRC (570 bytes) */

/*
 * Run the blob's calcCRC over the state now in cp_b and hand back the
 * sixteen bytes it left.  Used by the separating trials below, which need to
 * ask the BLOB what changed rather than ask our source.
 */
static void
blob_crc_after(unsigned char out[V90CP_CRC])
{
	ref_cp_calccrc(cp_b);
	memcpy(out, CPB->crc, V90CP_CRC);
}

static int
run_cp_calccrc(void)
{
	static const unsigned int lens[] = {
		0x11u, 0x12u, 0x23u, 0x24u, 0x40u, 0x99u, 300u, 2000u
	};
	int trial, ran = 0, empty = 0, moved = 0;
	int saw_carry_in = 0, saw_skip = 0, saw_nonskip = 0;

	diff_begin("V90CP::calcCRC");
	set_level(0);

	for (trial = 0; trial < 48; trial++) {
		unsigned char before[CP_SLOT];
		long tag = 3000 + trial;
		unsigned int n = lens[(unsigned)trial %
				      (sizeof(lens) / sizeof(lens[0]))];
		int i;

		seed_pair(trial + 800, trial % 3);
		CPA->word_3bb0 = CPB->word_3bb0 = n;

		/*
		 * The bit vector holds 0 and 1 the way a real sequence does,
		 * and the register holds a seeded state that is NOT all ones.
		 */
		for (i = 0; i < 2100; i++) {
			unsigned char v = (unsigned char)((lfsr =
			    (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u)) & 1u);

			CPA->bits[i] = CPB->bits[i] = v;
		}
		for (i = 0; i < V90CP_CRC; i++) {
			unsigned char v =
			    (unsigned char)((trial + i) & 1);

			CPA->crc[i] = CPB->crc[i] = v;
		}
		memcpy(before, cp_b, CP_SLOT);

		CPA->calcCRC();
		ref_cp_calccrc(cp_b);

		compare_pair("after calcCRC", tag);

		if (memcmp(before, cp_b, sizeof(V90CP)) != 0)
			moved = 1;

		/*
		 * `end` is `n - 0x11` and the walk starts at 0x12, so 0x23 is
		 * the largest extent that consumes NOTHING -- the boundary is
		 * strict, and 0x23 and 0x24 are both in the sweep so that both
		 * sides of it are exercised.
		 */
		if (n > 0x23u) {
			ran = 1;
			diff_eq_int("a live extent moved the register (%ld)",
				    memcmp(before + 0x3b98, cp_b + 0x3b98,
					   V90CP_CRC) != 0, 1, tag);
		} else {
			empty = 1;
			diff_eq_int("an empty extent left it alone (%ld)",
				    memcmp(before + 0x3b98, cp_b + 0x3b98,
					   V90CP_CRC) == 0, 1, tag);
		}

		/* Nothing outside the register moves. */
		diff_eq_int("+0x3bb0 is untouched (%ld)",
			    (long)CPB->word_3bb0, (long)n, tag);
		diff_eq_int("the bit vector is untouched (%ld)",
			    memcmp(before + 0xcb8, cp_b + 0xcb8, 2100) == 0, 1,
			    tag);
	}

	/*
	 * THE REGISTER IS CARRIED IN.  Same vector, two starting states, and
	 * the BLOB's two answers have to differ -- which is what a body that
	 * seeded the register the way `resetCRC` does could not produce.  The
	 * mutation adjudicates; this counter only records that the trial ran.
	 */
	{
		unsigned char from_zeros[V90CP_CRC], from_ones[V90CP_CRC];
		unsigned char keep[CP_SLOT];
		int i;

		seed_pair(999, 0);
		CPA->word_3bb0 = CPB->word_3bb0 = 300u;
		for (i = 0; i < 2100; i++) {
			unsigned char v = (unsigned char)((lfsr =
			    (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u)) & 1u);

			CPB->bits[i] = v;
		}
		memcpy(keep, cp_b, CP_SLOT);

		for (i = 0; i < V90CP_CRC; i++)
			CPB->crc[i] = 0;
		blob_crc_after(from_zeros);

		memcpy(cp_b, keep, CP_SLOT);
		for (i = 0; i < V90CP_CRC; i++)
			CPB->crc[i] = 1;
		blob_crc_after(from_ones);

		diff_eq_int("the blob carried the register in",
			    memcmp(from_zeros, from_ones, V90CP_CRC) != 0, 1,
			    0);
		saw_carry_in = 1;
	}

	/*
	 * THE FRAME BITS ARE STEPPED OVER.  One flip at a multiple of
	 * seventeen must change nothing and one that is not must change
	 * something; both halves are read off the BLOB, and neither on its
	 * own separates the skip from its absence.
	 */
	{
		unsigned char base[V90CP_CRC], flipped[V90CP_CRC];
		unsigned char keep[CP_SLOT];
		int i;

		seed_pair(998, 0);
		CPA->word_3bb0 = CPB->word_3bb0 = 300u;
		for (i = 0; i < 2100; i++) {
			unsigned char v = (unsigned char)((lfsr =
			    (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u)) & 1u);

			CPB->bits[i] = v;
		}
		for (i = 0; i < V90CP_CRC; i++)
			CPB->crc[i] = (unsigned char)(i & 1);
		memcpy(keep, cp_b, CP_SLOT);

		blob_crc_after(base);

		/* 0x44 == 68 == 4 * 17, inside [0x12, 300 - 0x11). */
		memcpy(cp_b, keep, CP_SLOT);
		CPB->bits[0x44] ^= 1;
		blob_crc_after(flipped);
		diff_eq_int("a flip at a framing bit changed nothing",
			    memcmp(base, flipped, V90CP_CRC) == 0, 1, 0);
		saw_skip = 1;

		/* 0x45 == 69, the very next index, and not a multiple. */
		memcpy(cp_b, keep, CP_SLOT);
		CPB->bits[0x45] ^= 1;
		blob_crc_after(flipped);
		diff_eq_int("a flip at the next bit changed the register",
			    memcmp(base, flipped, V90CP_CRC) != 0, 1, 0);
		saw_nonskip = 1;
	}

	diff_eq_int("a live extent was tried", ran, 1, 0);
	diff_eq_int("an empty extent was tried", empty, 1, 0);
	diff_eq_int("calcCRC changed the object", moved, 1, 0);
	diff_eq_int("the carry-in trial ran", saw_carry_in, 1, 0);
	diff_eq_int("the framing-bit trial ran", saw_skip, 1, 0);
	diff_eq_int("the non-framing-bit trial ran", saw_nonskip, 1, 0);
	return diff_end();
}

/* ------------------------------ printNofRecievedMpMpNot (56 bytes) */

static int
run_cp_printnof(void)
{
	/*
	 * The counters are `int` and the diagnostic prints them with %d, so a
	 * negative one is a different string from its unsigned reading.
	 */
	static const int counts[] = {
		0, 1, -1, 255, -255, 0x7fffffff, (-0x7fffffff - 1), 12345
	};
	int trial, above = 0, below = 0;
	unsigned lvl;

	diff_begin("V90CP::printNofRecievedMpMpNot");

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level(lvl);
		for (trial = 0; trial < 16; trial++) {
			unsigned char before_a[CP_SLOT], before_b[CP_SLOT];
			long tag = (long)lvl * 1000 + trial;
			unsigned n = sizeof(counts) / sizeof(counts[0]);

			seed_pair(trial + 900, trial % 3);
			CPA->nofRecievedMp = CPB->nofRecievedMp =
			    counts[(unsigned)trial % n];
			CPA->nofRecievedMpNot = CPB->nofRecievedMpNot =
			    counts[((unsigned)trial + 3) % n];
			memcpy(before_a, cp_a, CP_SLOT);
			memcpy(before_b, cp_b, CP_SLOT);

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			CPA->printNofRecievedMpMpNot();
			ref_cp_printnof(cp_b);

			dsplib_debug_capture_on = 0;

			diff_eq_int("transcript matches (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("line counts match (%ld)",
				    (int)dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1), tag);

			if (lvl > 1) {
				diff_eq_int("above the gate the blob printed "
					    "one line (%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    1, tag);
				above = 1;
			} else {
				diff_eq_int("below the gate the blob was "
					    "silent (%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    0, tag);
				diff_eq_int("below the gate ours was silent "
					    "(%ld)",
					    (int)dsplib_debug_capture_lines(0),
					    0, tag);
				below = 1;
			}

			/* It prints; it does not count. */
			diff_eq_int("ours wrote nothing (%ld)",
				    memcmp(before_a, cp_a, CP_SLOT) == 0, 1,
				    tag);
			diff_eq_int("the blob wrote nothing (%ld)",
				    memcmp(before_b, cp_b, CP_SLOT) == 0, 1,
				    tag);
		}
	}

	set_level(0);
	diff_eq_int("the gate was tried open", above, 1, 0);
	diff_eq_int("the gate was tried shut", below, 1, 0);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_cp_getbitvector();
	rc |= run_cp_resetdetector();
	rc |= run_cp_reset();
	rc |= run_cp_resetcrc();
	rc |= run_cp_calcseqlen();
	rc |= run_cp_calccrc();
	rc |= run_cp_printnof();

	return rc;
}

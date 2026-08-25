/*
 * t_v92mpunpck.cpp -- differential test of `setParamsInfoFromV92CPUnPck`.
 *
 * ONE SYMBOL, 601 BYTES, AND NOT ONE DIAGNOSTIC IN IT.  `tools/dis.py` over
 * 0x33c60..0x33eb9 prints no relocation banner at all, so the function calls
 * nothing and references no data symbol; there is no transcript tier here and
 * one debug level is the whole of it.  That is the same shape its V.90 twin
 * `setParamsInfoFromCPUnPck` has (t_v90unpck.cpp) and the OPPOSITE of
 * `V92setParamsInfoFromCPUnPck` (t_v92unpck.c), half of which is prints.
 *
 * THREE NAMES, AND THEY ARE THREE FUNCTIONS.  `nm -S ref/slmodemd/dsplibs.o`:
 *
 *	000336b0  0000026e  T  setParamsInfoFromCPUnPck	      622 B
 *	00033c60  00000259  T  setParamsInfoFromV92CPUnPck    601 B   <- this one
 *	00012f00  00000a87  T  V92setParamsInfoFromCPUnPck   2695 B
 *
 * The third is `src/pump/v90/V92ParamsInfo.c`'s and fills a `V92ParamsInfo`
 * from the message block; it is 0x20000 away and has two callers.  This one
 * fills a `V90MappingParams` from a `V92CP` and has none.
 *
 * IT IS CALLERLESS AND THE TEST IS HOW IT IS REACHED.  Zero relocations of any
 * kind name the symbol anywhere in the object, so there is no live entry point
 * to drive it through; the harness calls it and the blob's `ref_` alias
 * directly, which is finding 7000's correction to 702.
 *
 * WHAT IS SHARED AND WHAT IS NOT.  The `V92CP` is INPUT: one copy, pointed at
 * by both sides, because two separately seeded inputs would agree whatever was
 * read out of them.  The parameter block is OUTPUT and there are two of it,
 * seeded identically with varied bytes so that a store of zero that never
 * happened cannot pass (findings 223, 224), with a 64-byte guard past each
 * compared against the seed.
 *
 * ===========================================================================
 * WHAT EACH AXIS IS FOR.
 *
 *   the gate, BOTH WAYS       `cp->byte_24` chooses which mask block the CODEC
 *                             tables are unpacked from, and when it is CLEAR
 *                             they come from the ORDINARY block at +0x42 --
 *                             the same source, the other destination (+0x304).
 *                             A fixture that only ever set it could not tell
 *                             the two destination bases apart; one that only
 *                             ever cleared it could not see a wrong source
 *                             base in the else arm.  Every case is run both
 *                             ways, and the SET pass uses 0xa5 as well as 1 so
 *                             that `setne` producing an exact 1 in `word_61c`
 *                             is separable from a whole-byte copy.
 *
 *   the two mask blocks       ... and they must DIFFER, or the gate is
 *   differ                    invisible: with equal blocks both arms produce
 *                             the same answer and the `if` might as well not
 *                             be there.  `arraysDiffer` below is declared per
 *                             case and checked in both directions, so this
 *                             cannot rot into a vacuous run.
 *
 *   word_28 is NOT the        the block each constellation is unpacked from is
 *   identity                  `cp->short_42[cp->word_28[i]]`, and with an
 *                             identity permutation a body that used `i`
 *                             directly would agree on every trial.  That is
 *                             7458's shape.  Repeats are in there too, because
 *                             two constellations sharing one mask block is
 *                             what the field is FOR -- `word_28` holds
 *                             `getConstellationsIndex`'s GROUP numbers on the
 *                             way out, and groups repeat by construction.
 *
 *   asymmetric bitmaps,       bit `b` of word `j` becomes the byte
 *   and single bits           `j * 16 + (15 - b)`, and the table comes out in
 *                             DESCENDING byte order.  A palindromic or
 *                             all-ones bitmap pins neither the mapping nor the
 *                             order, so `SINGLE` puts exactly one bit in a
 *                             different place in each of the six and
 *                             `TWOWORDS` puts two bits in two different words.
 *
 *   empty and full            an all-zero block leaves the length at 0 with
 *                             the loop having stored nothing; an all-ones one
 *                             stores 128 bytes, which is EXACTLY the
 *                             constellation's extent, so the two together
 *                             straddle the whole range the function can write.
 *
 *   the two arms differ       the codec blocks have a different POPULATION
 *   in weight                 from the ordinary ones in the `HEAVY` cases,
 *                             which is what exposes the length word being
 *                             SHARED: the second half of the function zeroes
 *                             and refills the same `constellationSize[i]` the
 *                             first half wrote, so with the gate set the
 *                             length describes the CODEC table and the first
 *                             table keeps entries past it.  With equal
 *                             populations that is unobservable.
 *
 *   both arms of the rate,    `word_0` is `cp->char_02` plus 0x14 or plus 8,
 *   and a rate that wraps     chosen on `cp->char_01`.  The two constants
 *                             differ by 12, so the rate has to be compared and
 *                             not just present; `char_02` is a SIGNED byte and
 *                             -128 is carried, so the sum going negative and
 *                             the store into an `unsigned int word_0` wrapping
 *                             is exercised rather than assumed.  `char_01` is
 *                             driven at 0, 1 and a value that is neither, so a
 *                             gate written `== 1` would fail.
 *
 *   the destination's FILL    finding 7622: an unwritten `V90MappingParams`
 *                             block is 0xa5a5a5a5 under the harness allocator
 *                             and a zero-filled one runs but asks for zero
 *                             bits for ever.  The block is seeded from the
 *                             LFSR on most repetitions and ALL-0xa5 and
 *                             ALL-ZERO on two dedicated ones, so "the function
 *                             overwrites every field the chain reads" is
 *                             tested against both of the fills that matter and
 *                             not only against varied bytes.
 *
 * WHAT CANNOT BE REACHED FROM HERE, stated rather than implied: the helper's
 * `which < 6 ? which : 0` clamp.  `which` is the loop counter and the loop
 * runs 0..5, so the false arm is dead in this caller -- it is reachable only
 * through the object's own `setConstellationMask` /
 * `setCodecConstellationMask` globals, which are not written.  A mutation of
 * the `<` is therefore equivalent and a mutation of the `6` downwards is not;
 * both are recorded in test/mutations/v92mpunpck.json.
 *
 * AND `word_28` IS KEPT INSIDE 0..5, which is the fixture's choice and not the
 * object's.  The field is an `int[6]` that the object neither masks nor
 * bounds, and it scales the mask base by sixteen -- so a 6, or a negative
 * value, reads outside the two mask blocks.  The blob would do that too, but
 * it is undefined in the reconstruction, and a trial that reaches undefined
 * behaviour is not a trial (D561).  That the object does not guard it is
 * recorded in the .cpp, not exercised here.
 *
 * THE FOUR `float` FIELDS ARE LEFT AS RANDOM BIT PATTERNS ON PURPOSE.  They
 * are copied with `movl` at both ends (finding 5820), so NaNs -- signalling
 * ones included -- pass through untouched; a spelling that went through the
 * x87 (`flds`/`fstps`) would quieten a signalling NaN and change the bits.
 * Sanitising them would delete that axis, so they are not sanitised.
 * ===========================================================================
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/V92CP.h"
#include "dsplib/V90MappingParams.h"

extern "C" {
void blobUnpack(V90MappingParams *params, V92CP *cp)
	asm("ref_setParamsInfoFromV92CPUnPck");
}

#define GUARD	64

struct slot {
	V90MappingParams p;
	unsigned char guard[GUARD];
};

static struct slot ours, theirs, seedcopy;
static V92CP cp;

static unsigned int lfsr;

static unsigned char
nextbyte(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned int)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)(lfsr >> 3);
}

static void
fill(void *dst, size_t n)
{
	unsigned char *q = (unsigned char *)dst;
	size_t i;

	for (i = 0; i < n; i++)
		q[i] = nextbyte();
}

/* How a mask block is built.  See the file comment. */
enum maskMode {
	MM_RANDOM = 0,	/* varied, moderately populated                     */
	MM_ZERO,	/* every word zero: the length ends at 0            */
	MM_ONES,	/* every bit set: the length ends at 128, the extent */
	MM_SINGLE,	/* one bit each, in a different place in each        */
	MM_TWOWORDS,	/* two bits each, in two different words             */
	MM_HEAVY	/* densely populated, to differ in weight from the
			 * ordinary blocks in the same trial                */
};

/* How the destination block is seeded before the call.  Finding 7622. */
enum fillMode {
	FM_VARIED = 0,	/* LFSR bytes: a store of zero cannot pass          */
	FM_A5,		/* 0xa5a5a5a5, the harness allocator's own fill     */
	FM_ZERO		/* all zero, the other fill that reaches the chain  */
};

static void
build_masks(short m[V92CP_GROUPS][V92CP_MASKS], int mode, unsigned int salt)
{
	int c, w;

	for (c = 0; c < V92CP_GROUPS; c++)
		for (w = 0; w < V92CP_MASKS; w++) {
			unsigned int v = 0;

			switch (mode) {
			case MM_ZERO:
				v = 0;
				break;
			case MM_ONES:
				v = 0xffffu;
				break;
			case MM_SINGLE:
				/*
				 * ONE bit in the whole block per group, at
				 * word (c + salt) % 8 and bit
				 * (3 * c + salt) % 16, so the six groups pin
				 * six different (word, bit) pairs and
				 * therefore six different bytes.
				 */
				if (w == (int)((unsigned int)c + salt) % 8)
					v = 1u << (((unsigned int)(3 * c)
						    + salt) % 16u);
				break;
			case MM_TWOWORDS:
				/* Two words apart, so the descending order
				 * across words is observable. */
				if (w == 0)
					v = 1u << (((unsigned int)c + salt)
						   % 16u);
				else if (w == 7)
					v = 1u << (((unsigned int)(c + 5)
						    + salt) % 16u);
				break;
			case MM_HEAVY:
				v = 0xfffeu ^ (unsigned int)((c + w + salt)
							     & 1);
				break;
			default:
				v = (unsigned int)(0x9e37u * (c + 1)
						   + 0x2545u * (w + 1)
						   + 0x1b3fu * salt);
				v ^= v >> 5;
				v &= 0xffffu;
				break;
			}
			m[c][w] = (short)(unsigned short)v;
		}
}

static unsigned int
popcount_masks(const short m[V92CP_GROUPS][V92CP_MASKS], int c)
{
	unsigned int n = 0;
	int w, b;

	for (w = 0; w < V92CP_MASKS; w++)
		for (b = 0; b < 16; b++)
			if ((((unsigned int)(unsigned short)m[c][w]) >> b) & 1u)
				n++;

	return n;
}

struct tcase {
	const char *name;
	int maskMode;
	int codecMaskMode;
	int group[V92CP_GROUPS];	/* what goes into cp.word_28  */
	signed char islong;		/* cp.char_01                 */
	signed char rate;		/* cp.char_02                 */
	/*
	 * WHETHER THE TWO MASK BLOCKS DIFFER, DECLARED AND THEN CHECKED.
	 * The gate only means anything where they do, so a case that quietly
	 * stopped separating them would run both arms to the same answer and
	 * prove nothing about the `if`.  Two cases here deliberately do NOT
	 * differ -- "both empty" and "both full", which exist for the two
	 * extremes of the length rather than for the gate -- and saying so in
	 * the row is what stops the assertion being weakened into "0 or 1".
	 */
	int arraysDiffer;
};

static const struct tcase cases[] = {
 { "identity, random both",	 MM_RANDOM,   MM_HEAVY,
				 {0, 1, 2, 3, 4, 5},  0,   0x2a,	1 },
 { "permuted, random both",	 MM_RANDOM,   MM_HEAVY,
				 {5, 3, 0, 4, 1, 2},  1,   0x2a,	1 },
 { "permuted, repeats",		 MM_RANDOM,   MM_HEAVY,
				 {2, 2, 5, 0, 5, 2},  0,   0x14,	1 },
 { "all one mask block",	 MM_RANDOM,   MM_HEAVY,
				 {3, 3, 3, 3, 3, 3},  1,   0x14,	1 },
 { "empty ordinary, heavy codec", MM_ZERO,    MM_HEAVY,
				 {4, 0, 2, 1, 5, 3},  0,   0x08,	1 },
 { "heavy ordinary, empty codec", MM_HEAVY,   MM_ZERO,
				 {4, 0, 2, 1, 5, 3},  1,   0x08,	1 },
 { "full ordinary, single codec", MM_ONES,    MM_SINGLE,
				 {1, 0, 3, 2, 5, 4},  0,   0x00,	1 },
 { "single ordinary, full codec", MM_SINGLE,  MM_ONES,
				 {1, 0, 3, 2, 5, 4},  1,   0x00,	1 },
 { "single both, different",	 MM_SINGLE,   MM_TWOWORDS,
				 {0, 2, 4, 1, 3, 5},  0,   0x21,	1 },
 { "two words both",		 MM_TWOWORDS, MM_SINGLE,
				 {5, 4, 3, 2, 1, 0},  1,   0x21,	1 },
 { "both empty",		 MM_ZERO,     MM_ZERO,
				 {0, 1, 2, 3, 4, 5},  0,   0x40,	0 },
 { "both full",			 MM_ONES,     MM_ONES,
				 {2, 4, 0, 5, 1, 3},  1,   0x40,	0 },
 /*
  * `char_02` is a SIGNED byte and the sum lands in an `unsigned int`, so -128
  * plus 8 is 0xffffff88 and -128 plus 0x14 is 0xffffff94.  Both arms carry it.
  */
 { "rate is -128, long arm",	 MM_RANDOM,   MM_HEAVY,
				 {3, 1, 4, 0, 2, 5},  1,   -128,	1 },
 { "rate is -128, short arm",	 MM_RANDOM,   MM_HEAVY,
				 {3, 1, 4, 0, 2, 5},  0,   -128,	1 },
 { "rate is -1, long arm",	 MM_RANDOM,   MM_HEAVY,
				 {1, 5, 2, 0, 4, 3},  1,   -1,		1 },
 /* `char_01` neither 0 nor 1: a gate written `== 1` fails here. */
 { "islong is 0x5a, not 1",	 MM_RANDOM,   MM_HEAVY,
				 {1, 5, 2, 0, 4, 3},  0x5a, 0x1f,	1 },
 { "islong is -1",		 MM_RANDOM,   MM_HEAVY,
				 {2, 0, 4, 3, 1, 5},  -1,  0x7f,	1 }
};
#define NCASE ((int)(sizeof(cases) / sizeof(cases[0])))

/*
 * The gate byte and the destination's fill are set from the loop rather than
 * from the case, because EVERY case is run both ways and over all three fills.
 */
static void
seed_all(unsigned int s, const struct tcase *t, unsigned char gate, int fm)
{
	int i;

	switch (fm) {
	case FM_A5:
		memset(&seedcopy, 0xa5, sizeof(seedcopy));
		break;
	case FM_ZERO:
		memset(&seedcopy, 0, sizeof(seedcopy));
		break;
	default:
		lfsr = 0x51a7u + 0x9e37u * s;
		fill(&seedcopy, sizeof(seedcopy));
		break;
	}
	memcpy(&ours, &seedcopy, sizeof(ours));
	memcpy(&theirs, &seedcopy, sizeof(theirs));

	/*
	 * The whole source object, floats included -- see the file comment for
	 * why the four `float` members are NOT sanitised afterwards.
	 */
	lfsr = 0x3c19u + 0x4e6du * s;
	fill(&cp, sizeof(cp));

	build_masks(cp.short_42, t->maskMode, s);
	build_masks(cp.short_a2, t->codecMaskMode, s + 1u);

	for (i = 0; i < V92CP_GROUPS; i++)
		cp.word_28[i] = t->group[i];

	cp.byte_24 = gate;
	cp.char_01 = t->islong;
	cp.char_02 = t->rate;
}

static void
compare_run(const char *what, long tag)
{
	diff_eq_obj_(__FILE__, __LINE__, what, "V90MappingParams",
		     &ours.p, &theirs.p, sizeof(V90MappingParams), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the guard past the block",
		     ours.guard, seedcopy.guard, GUARD, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the reference's guard",
		     theirs.guard, seedcopy.guard, GUARD, tag);
}

static int
run_map(void)
{
	diff_begin("the two blocks' maps");

	diff_eq_int("sizeof(V90MappingParams) is 0x%lx",
		    (long)sizeof(V90MappingParams), 0x650, 0x650);
	diff_eq_int("constellation is at +0x%lx",
		    (long)offsetof(V90MappingParams, constellation), 4, 4);
	diff_eq_int("codecConstellation is at +0x%lx",
		    (long)offsetof(V90MappingParams, codecConstellation),
		    0x304, 0x304);
	diff_eq_int("constellationSize is at +0x%lx",
		    (long)offsetof(V90MappingParams, constellationSize),
		    0x604, 0x604);
	diff_eq_int("word_61c is at +0x%lx",
		    (long)offsetof(V90MappingParams, word_61c), 0x61c, 0x61c);
	diff_eq_int("shaperSR is at +0x%lx",
		    (long)offsetof(V90MappingParams, shaperSR), 0x620, 0x620);
	diff_eq_int("distinctIndex is at +0x%lx",
		    (long)offsetof(V90MappingParams, distinctIndex),
		    0x638, 0x638);

	/*
	 * The V92CP side: every displacement this function forms off its
	 * second argument, which is the whole of the evidence that the source
	 * type is a `V92CP` and not something new.  `__builtin_offsetof`
	 * because the class has a constructor.
	 */
	diff_eq_int("CP: char_01 is at +0x%lx",
		    (long)__builtin_offsetof(V92CP, char_01), 0x01, 0x01);
	diff_eq_int("CP: char_02 is at +0x%lx",
		    (long)__builtin_offsetof(V92CP, char_02), 0x02, 0x02);
	diff_eq_int("CP: word_08 is at +0x%lx",
		    (long)__builtin_offsetof(V92CP, word_08), 0x08, 0x08);
	diff_eq_int("CP: word_0c is at +0x%lx",
		    (long)__builtin_offsetof(V92CP, word_0c), 0x0c, 0x0c);
	diff_eq_int("CP: flt_14 is at +0x%lx",
		    (long)__builtin_offsetof(V92CP, flt_14), 0x14, 0x14);
	diff_eq_int("CP: flt_18 is at +0x%lx",
		    (long)__builtin_offsetof(V92CP, flt_18), 0x18, 0x18);
	diff_eq_int("CP: flt_1c is at +0x%lx",
		    (long)__builtin_offsetof(V92CP, flt_1c), 0x1c, 0x1c);
	diff_eq_int("CP: flt_20 is at +0x%lx",
		    (long)__builtin_offsetof(V92CP, flt_20), 0x20, 0x20);
	diff_eq_int("CP: byte_24 is at +0x%lx",
		    (long)__builtin_offsetof(V92CP, byte_24), 0x24, 0x24);
	diff_eq_int("CP: word_28 is at +0x%lx",
		    (long)__builtin_offsetof(V92CP, word_28), 0x28, 0x28);
	diff_eq_int("CP: short_42 is at +0x%lx",
		    (long)__builtin_offsetof(V92CP, short_42), 0x42, 0x42);
	diff_eq_int("CP: short_a2 is at +0x%lx",
		    (long)__builtin_offsetof(V92CP, short_a2), 0xa2, 0xa2);

	/*
	 * THE TWO MASK BLOCKS ABUT, which is the structural difference from
	 * the V.90 message (0x9c - 0x3a = 0x62, two bytes nothing reads).
	 * 0xa2 - 0x42 = 0x60 = 6 * 16, and the row stride is sixteen bytes
	 * because the object scales `word_28[i]` by `shl $0x4`.
	 */
	diff_eq_int("CP: the two mask blocks abut, 0x%lx apart",
		    (long)(__builtin_offsetof(V92CP, short_a2)
			   - __builtin_offsetof(V92CP, short_42)),
		    0x60, 0x60);
	diff_eq_int("CP: one mask row is %ld bytes",
		    (long)sizeof(cp.short_42[0]), 16, 16);
	diff_eq_int("CP: word_28's stride is %ld",
		    (long)sizeof(cp.word_28[0]), 4, 4);

	return diff_end();
}

static int
run_trials(void)
{
	int c, g, rep;
	int sawEmpty = 0;
	int sawFull = 0;
	int sawWeightSplit = 0;
	int sawA5 = 0;
	int sawZeroFill = 0;
	int sawNegativeRate = 0;

	diff_begin("setParamsInfoFromV92CPUnPck");

	for (c = 0; c < NCASE; c++)
		for (g = 0; g < 2; g++)
			for (rep = 0; rep < 3; rep++) {
				unsigned int s;
				unsigned char gate;
				long tag;
				unsigned int want;
				int fm;
				int i;

				s = (unsigned int)((c * 3 + rep) * 2 + g) + 1u;
				gate = (unsigned char)(g == 0 ? 0
						       : (rep == 0 ? 1 : 0xa5));
				fm = (rep == 0) ? FM_VARIED
				     : (rep == 1 ? FM_A5 : FM_ZERO);
				tag = (long)((c * 6) + (g * 3) + rep);

				seed_all(s, &cases[c], gate, fm);

				if (fm == FM_A5)
					sawA5 = 1;
				if (fm == FM_ZERO)
					sawZeroFill = 1;

				/*
				 * ANTI-VACUITY ON THE INPUT, not on the
				 * output: the gate only means anything if the
				 * two mask blocks differ, and a case whose two
				 * modes happened to coincide would run both
				 * arms to the same answer and prove nothing.
				 * Checked in BOTH directions against what the
				 * row declares, so a case that stopped
				 * separating them is a failure and not a quiet
				 * weakening.
				 */
				diff_eq_int("the two mask blocks differ as the"
					    " case says (%ld)",
					    memcmp(cp.short_42, cp.short_a2,
						   sizeof(cp.short_42)) != 0,
					    cases[c].arraysDiffer, tag);

				setParamsInfoFromV92CPUnPck(&ours.p, &cp);
				blobUnpack(&theirs.p, &cp);

				compare_run(cases[c].name, tag);

				/*
				 * The rate arithmetic, checked against what
				 * the two constants say rather than only
				 * against the other side -- two identical
				 * wrong constants would agree.
				 */
				want = (unsigned int)((int)cases[c].rate
				       + (cases[c].islong != 0 ? 0x14 : 8));
				diff_eq_int("word_0 is char_02 + %ld",
					    (long)ours.p.word_0, (long)want,
					    (long)(cases[c].islong != 0
						   ? 0x14 : 8));
				if ((int)cases[c].rate < 0)
					sawNegativeRate = 1;

				/*
				 * The gate reaches word_61c as an exact 0 or
				 * 1 and not as the byte, which 0xa5 is what
				 * separates.
				 */
				diff_eq_int("word_61c is 0 or 1 (case %ld)",
					    (long)ours.p.word_61c,
					    (long)(gate != 0 ? 1 : 0), tag);

				/*
				 * The six spectral words, checked against the
				 * SOURCE and not only against the other side.
				 * The four floats are compared as bit
				 * patterns, because that is what a `movl` copy
				 * preserves and a comparison of values would
				 * not separate a NaN from another NaN.
				 */
				diff_eq_int("shaperSR (case %ld)",
					    (long)ours.p.shaperSR,
					    (long)(int)cp.word_08, tag);
				diff_eq_int("shaperId (case %ld)",
					    (long)ours.p.shaperId,
					    (long)cp.word_0c, tag);
				diff_eq_obj_(__FILE__, __LINE__,
					     cases[c].name,
					     "the four shaper floats, as bits",
					     &ours.p.shaperA1, &cp.flt_14,
					     4 * sizeof(float), tag);

				for (i = 0; i < V92CP_GROUPS; i++) {
					unsigned int b =
					    (unsigned int)cases[c].group[i];
					unsigned int wantN;

					diff_eq_int("distinctIndex[%ld]",
						    (long)ours.p.distinctIndex[i],
						    (long)b, (long)i);

					/*
					 * The length is the CODEC block's
					 * population whenever the gate is set,
					 * and the ordinary one's when it is
					 * clear -- because the second half
					 * rewrites the same word.  Computed
					 * here from the input rather than read
					 * back from either side.
					 */
					wantN = (gate != 0)
					    ? popcount_masks(cp.short_a2,
							     (int)b)
					    : popcount_masks(cp.short_42,
							     (int)b);
					diff_eq_int("constellationSize[%ld]",
						    (long)ours.p.constellationSize[i],
						    (long)wantN, (long)i);

					if (wantN == 0)
						sawEmpty = 1;
					if (wantN == 128)
						sawFull = 1;
					if (gate != 0
					    && popcount_masks(cp.short_42,
							      (int)b) != wantN)
						sawWeightSplit = 1;
				}
			}

	/*
	 * The denominators of the shapes the file comment says this fixture
	 * exists to reach.  A run that stopped reaching one of them would
	 * still be green on every comparison above, which is exactly the
	 * failure findings 134 and 2400 are about.
	 */
	diff_eq_int("some trial ended with an EMPTY constellation",
		    sawEmpty, 1, 0);
	diff_eq_int("some trial FILLED a constellation to 128", sawFull, 1, 0);
	diff_eq_int("some trial had the two blocks at different weights",
		    sawWeightSplit, 1, 0);
	diff_eq_int("some trial started from the allocator's 0xa5 fill",
		    sawA5, 1, 0);
	diff_eq_int("some trial started from an all-zero block",
		    sawZeroFill, 1, 0);
	diff_eq_int("some trial carried a NEGATIVE char_02",
		    sawNegativeRate, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_map();
	rc |= run_trials();

	return rc;
}

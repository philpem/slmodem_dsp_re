/*
 * t_v90unpck.cpp -- differential test of `setParamsInfoFromCPUnPck`.
 *
 * ONE SYMBOL, 622 BYTES, AND NOT ONE DIAGNOSTIC IN IT.  Unlike its V.92
 * cousin `V92setParamsInfoFromCPUnPck` -- half of which is prints, and whose
 * fixture therefore runs at two debug levels -- this function calls nothing
 * and prints nothing, so there is no transcript tier here and one level is
 * the whole of it.  `tools/dis.py` over 0x336b0..0x3391e shows no relocation
 * of any kind.
 *
 * WHAT IS SHARED AND WHAT IS NOT.  The CP block and the `tagV90AdditionalCPinfo`
 * beside it are INPUT: one copy each, pointed at by both sides, because two
 * separately seeded inputs would agree whatever was read out of them.  The
 * parameter block is OUTPUT and there are two of it, seeded identically with
 * varied bytes so that a store of zero that never happened cannot pass
 * (findings 223, 224), with a 64-byte guard past each compared against the
 * seed.
 *
 * ===========================================================================
 * WHAT EACH AXIS IS FOR.  Three of the function's constants are one byte
 * apart from a plausible wrong one, so the fixture is built round separating
 * them rather than round covering lines.
 *
 *   the gate, BOTH WAYS       `codecConstellationPresent` chooses which
 *                             bitmap array the CODEC tables are unpacked
 *                             from, and when it is CLEAR they come from the
 *                             ORDINARY bitmaps -- the same source, the other
 *                             destination.  A fixture that only ever set it
 *                             could not tell the two destination bases apart
 *                             (0x4 against 0x304); a fixture that only ever
 *                             cleared it could not see a wrong source base in
 *                             the else arm.  Every case is run both ways.
 *
 *   the two mask arrays       ... and they must DIFFER, or the gate is
 *   differ                    invisible: with equal arrays both arms produce
 *                             the same answer and the `if` might as well not
 *                             be there.  `masks_differ` below is asserted, so
 *                             this cannot rot into a vacuous run.
 *
 *   distinctIndex is NOT      the bitmap each constellation is unpacked from
 *   the identity              is `constellationMask[cp->distinctIndex[i]]`,
 *                             and with an identity permutation a body that
 *                             used `i` directly would agree on every trial.
 *                             This is 7458's shape and t_v90cmask's own
 *                             argument for the same field.  Repeats are in
 *                             there too, because two constellations sharing
 *                             one bitmap is what the field is FOR.
 *
 *   asymmetric bitmaps,       bit `b` of word `j` becomes the byte
 *   and single bits           `j * 16 + (15 - b)`, and the table comes out in
 *                             DESCENDING byte order.  A palindromic or
 *                             all-ones bitmap pins neither the mapping nor
 *                             the order, so `SINGLE` puts exactly one bit in
 *                             a different place in each of the six and
 *                             `TWOWORDS` puts two bits in two different
 *                             words of each.
 *
 *   empty and full            an all-zero bitmap leaves the length at 0 with
 *                             the loop having stored nothing; an all-ones one
 *                             stores 128 bytes, which is EXACTLY the
 *                             constellation's extent, so the two together
 *                             straddle the whole range the function can
 *                             write.
 *
 *   the two arms differ       the codec bitmaps have a different POPULATION
 *   in weight                 from the ordinary ones in the `HEAVY` cases,
 *                             which is what exposes the length word being
 *                             SHARED: the second half of the function zeroes
 *                             and refills the same `constellationSize[i]` the
 *                             first half wrote, so with the gate set the
 *                             length describes the CODEC table and the first
 *                             table keeps entries past it.  With equal
 *                             populations that is unobservable.
 *
 *   both arms of islong,      `word_0` is the data bit rate plus 0x14 or plus
 *   and a rate that wraps     8.  The two constants differ by 12, so the rate
 *                             has to be compared and not just present; and
 *                             0xfffffff8 is carried so that the sum wrapping
 *                             is exercised rather than assumed -- the field
 *                             is unsigned and nothing bounds it, exactly as
 *                             `getDataBitRate` records for the inverse.
 *
 * WHAT CANNOT BE REACHED FROM HERE, stated rather than implied: the helper's
 * `which < 6 ? which : 0` clamp.  `which` is the loop counter and the loop
 * runs 0..5, so the false arm is dead in this caller -- it is reachable only
 * through the object's own `setConstellationMask`/`setCodecConstellationMask`
 * globals, which are not written.  A mutation of the `<` is therefore
 * equivalent and a mutation of the `6` downwards is not; both are recorded in
 * test/mutations/v90unpck.json.
 *
 * The `(unsigned char)` on `j * 16 + k` is unobservable for the same kind of
 * reason: the largest value is 7 * 16 + 15 = 127 and it cannot wrap.
 * ===========================================================================
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/V90CPUnPck.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/tagV90AdditionalCPinfo.h"

extern "C" {
void blobUnpack(V90MappingParams *params, V90CPUnPck *cp)
	asm("ref_setParamsInfoFromCPUnPck");
}

#define GUARD	64

struct slot {
	V90MappingParams p;
	unsigned char guard[GUARD];
};

static struct slot ours, theirs, seedcopy;
static V90CPUnPck cp;
static tagV90AdditionalCPinfo info;

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

/* How a bitmap array is built.  See the file comment. */
enum maskMode {
	MM_RANDOM = 0,	/* varied, moderately populated                     */
	MM_ZERO,	/* every word zero: the length ends at 0            */
	MM_ONES,	/* every bit set: the length ends at 128, the extent */
	MM_SINGLE,	/* one bit each, in a different place in each        */
	MM_TWOWORDS,	/* two bits each, in two different words             */
	MM_HEAVY	/* densely populated, to differ in weight from the
			 * ordinary bitmaps in the same trial               */
};

static void
build_masks(short m[V90_CPUNPCK_CONSTELS][V90_CPUNPCK_MASK_WORDS], int mode,
	    unsigned int salt)
{
	int c, w;

	for (c = 0; c < V90_CPUNPCK_CONSTELS; c++)
		for (w = 0; w < V90_CPUNPCK_MASK_WORDS; w++) {
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
				 * ONE bit in the whole array per
				 * constellation, at word (c + salt) % 8 and
				 * bit (3 * c + salt) % 16, so the six
				 * constellations pin six different
				 * (word, bit) pairs and therefore six
				 * different bytes.
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
popcount_masks(const short m[V90_CPUNPCK_CONSTELS][V90_CPUNPCK_MASK_WORDS],
	       int c)
{
	unsigned int n = 0;
	int w, b;

	for (w = 0; w < V90_CPUNPCK_MASK_WORDS; w++)
		for (b = 0; b < 16; b++)
			if ((((unsigned int)(unsigned short)m[c][w]) >> b) & 1u)
				n++;

	return n;
}

struct tcase {
	const char *name;
	int maskMode;
	int codecMaskMode;
	unsigned char distinct[V90_CPUNPCK_CONSTELS];
	int islong;
	unsigned int rate;
	/*
	 * WHETHER THE TWO BITMAP ARRAYS DIFFER, DECLARED AND THEN CHECKED.
	 * The gate only means anything where they do, so a case that quietly
	 * stopped separating them would run both arms to the same answer and
	 * prove nothing about the `if`.  Two cases here deliberately do NOT
	 * differ -- "both empty" and "both full", which exist for the two
	 * extremes of the length rather than for the gate -- and saying so in
	 * the row is what stops the assertion being weakened into "0 or 1".
	 */
	int arraysDiffer;
};

/*
 * `distinct` is kept inside 0..5 everywhere.  The field is an unbounded
 * `unsigned char` that the object neither masks nor checks, so a 6 would read
 * past the bitmap array -- which the blob would do too, but which is
 * undefined in the reconstruction, and a trial that reaches undefined
 * behaviour is not a trial (D561).  That the object does not guard it is
 * recorded in include/dsplib/V90CPUnPck.h, not exercised here.
 */
static const struct tcase cases[] = {
 { "identity, random both",     MM_RANDOM,   MM_HEAVY,
				{0, 1, 2, 3, 4, 5}, 0, 0x2a,         1 },
 { "permuted, random both",     MM_RANDOM,   MM_HEAVY,
				{5, 3, 0, 4, 1, 2}, 1, 0x2a,         1 },
 { "permuted, repeats",         MM_RANDOM,   MM_HEAVY,
				{2, 2, 5, 0, 5, 2}, 0, 0x14,         1 },
 { "all one bitmap",            MM_RANDOM,   MM_HEAVY,
				{3, 3, 3, 3, 3, 3}, 1, 0x14,         1 },
 { "empty ordinary, heavy codec", MM_ZERO,   MM_HEAVY,
				{4, 0, 2, 1, 5, 3}, 0, 0x08,         1 },
 { "heavy ordinary, empty codec", MM_HEAVY,  MM_ZERO,
				{4, 0, 2, 1, 5, 3}, 1, 0x08,         1 },
 { "full ordinary, single codec", MM_ONES,   MM_SINGLE,
				{1, 0, 3, 2, 5, 4}, 0, 0x00,         1 },
 { "single ordinary, full codec", MM_SINGLE, MM_ONES,
				{1, 0, 3, 2, 5, 4}, 1, 0x00,         1 },
 { "single both, different",    MM_SINGLE,   MM_TWOWORDS,
				{0, 2, 4, 1, 3, 5}, 0, 0x21,         1 },
 { "two words both",            MM_TWOWORDS, MM_SINGLE,
				{5, 4, 3, 2, 1, 0}, 1, 0x21,         1 },
 { "both empty",                MM_ZERO,     MM_ZERO,
				{0, 1, 2, 3, 4, 5}, 0, 0x40,         0 },
 { "both full",                 MM_ONES,     MM_ONES,
				{2, 4, 0, 5, 1, 3}, 1, 0x40,         0 },
 { "rate wraps on the long arm", MM_RANDOM,  MM_HEAVY,
				{3, 1, 4, 0, 2, 5}, 1, 0xfffffff8u,  1 },
 { "rate wraps on the short arm", MM_RANDOM, MM_HEAVY,
				{3, 1, 4, 0, 2, 5}, 0, 0xfffffff8u,  1 },
 { "gate value that is not 1",  MM_RANDOM,   MM_HEAVY,
				{1, 5, 2, 0, 4, 3}, 1, 0x1f,         1 }
};
#define NCASE ((int)(sizeof(cases) / sizeof(cases[0])))

/*
 * The gate byte is set from the loop below rather than from the case, because
 * EVERY case is run both ways; `gate` is the value written into it, and 0xa5
 * rather than 1 on one pass so that `setne` producing an exact 1 in
 * `word_61c` is separable from a whole-byte copy.
 */
static void
seed_all(unsigned int s, const struct tcase *t, unsigned char gate)
{
	int i;

	lfsr = 0x51a7u + 0x9e37u * s;
	fill(&seedcopy, sizeof(seedcopy));
	memcpy(&ours, &seedcopy, sizeof(ours));
	memcpy(&theirs, &seedcopy, sizeof(theirs));

	lfsr = 0x3c19u + 0x4e6du * s;
	fill(&cp, sizeof(cp));
	fill(&info, sizeof(info));

	build_masks(cp.constellationMask, t->maskMode, s);
	build_masks(cp.codecConstellationMask, t->codecMaskMode, s + 1u);

	for (i = 0; i < V90_CPUNPCK_CONSTELS; i++)
		cp.distinctIndex[i] = t->distinct[i];

	cp.codecConstellationPresent = gate;
	cp.dataBitRate = t->rate;
	cp.info = &info;
	info.word_04 = (unsigned int)t->islong;
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

	/* The CP side, which nothing else in the tree pins. */
	diff_eq_int("CP: dataBitRate is at +0x%lx",
		    (long)offsetof(V90CPUnPck, dataBitRate), 0x14, 0x14);
	diff_eq_int("CP: shaperSR is at +0x%lx",
		    (long)offsetof(V90CPUnPck, shaperSR), 0x18, 0x18);
	diff_eq_int("CP: shaperB2 is at +0x%lx",
		    (long)offsetof(V90CPUnPck, shaperB2), 0x2c, 0x2c);
	diff_eq_int("CP: the gate is at +0x%lx",
		    (long)offsetof(V90CPUnPck, codecConstellationPresent),
		    0x30, 0x30);
	diff_eq_int("CP: distinctIndex is at +0x%lx",
		    (long)offsetof(V90CPUnPck, distinctIndex), 0x31, 0x31);
	diff_eq_int("CP: constellationMask is at +0x%lx",
		    (long)offsetof(V90CPUnPck, constellationMask), 0x3a, 0x3a);
	diff_eq_int("CP: codecConstellationMask is at +0x%lx",
		    (long)offsetof(V90CPUnPck, codecConstellationMask),
		    0x9c, 0x9c);
	diff_eq_int("CP: info is at +0x%lx",
		    (long)offsetof(V90CPUnPck, info), 0xfc, 0xfc);

	return diff_end();
}

static int
run_trials(void)
{
	int c, g, rep;
	int sawEmpty = 0;
	int sawFull = 0;
	int sawWeightSplit = 0;

	diff_begin("setParamsInfoFromCPUnPck");

	for (c = 0; c < NCASE; c++)
		for (g = 0; g < 2; g++)
			for (rep = 0; rep < 2; rep++) {
				unsigned int s;
				unsigned char gate;
				long tag;
				unsigned int want;
				int i;

				s = (unsigned int)((c * 2 + rep) * 2 + g) + 1u;
				gate = (unsigned char)(g == 0 ? 0
						       : (rep == 0 ? 1 : 0xa5));
				tag = (long)((c * 4) + (g * 2) + rep);

				seed_all(s, &cases[c], gate);

				/*
				 * ANTI-VACUITY ON THE INPUT, not on the
				 * output: the gate only means anything if the
				 * two bitmap arrays differ, and a case whose
				 * two modes happened to coincide would run
				 * both arms to the same answer and prove
				 * nothing.  Checked in BOTH directions
				 * against what the row declares, so a case
				 * that stopped separating them is a failure
				 * and not a quiet weakening.
				 */
				diff_eq_int("the two mask arrays differ as the"
					    " case says (%ld)",
					    memcmp(cp.constellationMask,
						   cp.codecConstellationMask,
						   sizeof(cp.constellationMask))
					    != 0, cases[c].arraysDiffer, tag);

				setParamsInfoFromCPUnPck(&ours.p, &cp);
				blobUnpack(&theirs.p, &cp);

				compare_run(cases[c].name, tag);

				/*
				 * The rate arithmetic, checked against what
				 * the two constants say rather than only
				 * against the other side -- two identical
				 * wrong constants would agree.
				 */
				want = cases[c].rate
				       + (cases[c].islong != 0 ? 0x14u : 8u);
				diff_eq_int("word_0 is rate + %ld",
					    (long)ours.p.word_0, (long)want,
					    (long)(cases[c].islong != 0
						   ? 0x14 : 8));

				/*
				 * The gate reaches word_61c as an exact 0 or
				 * 1 and not as the byte, which 0xa5 is what
				 * separates.
				 */
				diff_eq_int("word_61c is 0 or 1 (case %ld)",
					    (long)ours.p.word_61c,
					    (long)(gate != 0 ? 1 : 0), tag);

				for (i = 0; i < V90_CPUNPCK_CONSTELS; i++) {
					unsigned int b = cases[c].distinct[i];
					unsigned int wantN;

					diff_eq_int("distinctIndex[%ld]",
						    (long)ours.p.distinctIndex[i],
						    (long)b, (long)i);

					/*
					 * The length is the CODEC bitmap's
					 * population whenever the gate is
					 * set, and the ordinary one's when it
					 * is clear -- because the second half
					 * rewrites the same word.  Computed
					 * here from the input rather than
					 * read back from either side.
					 */
					wantN = (gate != 0)
					    ? popcount_masks(cp.codecConstellationMask,
							     (int)b)
					    : popcount_masks(cp.constellationMask,
							     (int)b);
					diff_eq_int("constellationSize[%ld]",
						    (long)ours.p.constellationSize[i],
						    (long)wantN, (long)i);

					if (wantN == 0)
						sawEmpty = 1;
					if (wantN == 128)
						sawFull = 1;
					if (gate != 0
					    && popcount_masks(cp.constellationMask,
							      (int)b) != wantN)
						sawWeightSplit = 1;
				}
			}

	/*
	 * The denominators of the three shapes the file comment says this
	 * fixture exists to reach.  A run that stopped reaching one of them
	 * would still be green on every comparison above, which is exactly the
	 * failure findings 134 and 2400 are about.
	 */
	diff_eq_int("some trial ended with an EMPTY constellation",
		    sawEmpty, 1, 0);
	diff_eq_int("some trial FILLED a constellation to 128", sawFull, 1, 0);
	diff_eq_int("some trial had the two bitmaps at different weights",
		    sawWeightSplit, 1, 0);

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

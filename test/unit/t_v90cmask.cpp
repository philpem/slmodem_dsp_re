/*
 * t_v90cmask.cpp -- differential test of getConstellationsIndex,
 * getConstellationMask and getCodecConstellationMask.
 *
 * EVERY BUFFER IS SEEDED WITH VARIED BYTES AND COMPARED WHOLE.  The mask
 * buffers are 24 shorts against a function that can reach 16 of them, so the
 * tail is a guard region neither side may touch; the parameter block is
 * compared in full after each call, so a write to a field the other side
 * leaves alone is a failure whichever side does it.  The seed is 0x5a.. and
 * not 0 or 1, which are the values the functions themselves write.
 *
 * THE INDEX FUNCTION AND THE MASK FUNCTIONS NEED DIFFERENT FIXTURES.
 *
 *   getConstellationsIndex writes distinctIndex[] itself and only ever reads
 *   entries it has already written, so it may be handed a block whose
 *   distinctIndex is random -- and is, because that is what proves the
 *   entries it does NOT write are the same on both sides.
 *
 *   The mask functions read distinctIndex[which] and use it to index
 *   constellationSize[] and the byte tables, so a random one would index far
 *   off the end.  They get an explicit permutation, and the permutation is
 *   DELIBERATELY NOT THE IDENTITY: with an identity the indirection is
 *   invisible, and a version that used `which` directly would agree with the
 *   object on every case.
 *
 * WHAT THE FIXTURE HAS TO CONTAIN, and why each one is here:
 *
 *   a zero length          getConstellationsIndex's byte loop is skipped and
 *                          its acceptance test compares 0 against 0, so two
 *                          empty constellations are declared identical.  A
 *                          fixture of non-empty constellations never reaches
 *                          the branch at 0x333b0 at all.
 *   a table byte >= 0x80   the high nibble is not masked, so the byte
 *                          addresses mask[8..15] -- eight entries the
 *                          function never cleared.  A fixture of small bytes
 *                          makes that unreachable.
 *   tables agreeing past   the comparison stops at `length`, so two
 *   the length             constellations that differ only beyond it are the
 *                          same constellation.
 *   A equal but B not,     getConstellationsIndex compares BOTH tables and
 *   and the other way      the two mask functions read one each; a fixture
 *                          where they always agree cannot tell them apart.
 *   equal tables but       the length is compared before the bytes are.
 *   different lengths
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V90MappingParams.h"

extern "C" {
unsigned int blobIndex(V90MappingParams *params, int *group)
	asm("ref_getConstellationsIndex");
void blobMask(V90MappingParams *params, int which, short *mask)
	asm("ref_getConstellationMask");
void blobCodecMask(V90MappingParams *params, int which, short *mask)
	asm("ref_getCodecConstellationMask");
}

/* 16 entries are reachable (a byte of 0xff addresses entry 15); 8 are guard. */
#define NMASK 24

struct maskBuf {
	short v[NMASK];
};

struct groupBuf {
	int v[8];
};

static V90MappingParams ourParams, theirParams;
static struct maskBuf ourMask, theirMask, seedMask;
static struct groupBuf ourGroup, theirGroup, seedGroup;

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/*
 * A table byte that depends only on (id, n, trial, mode), so two
 * constellations given the same id hold byte-for-byte the same table and two
 * given different ids differ -- at n = 0 and at most later positions, which
 * is what makes "differs inside the length" and "differs only past it"
 * separable below.
 */
static unsigned char
table_byte(int id, int n, int trial, int mode)
{
	unsigned x = (unsigned)(id * 0x9e37u + n * 0x2545u + trial * 0x1b3fu);

	x ^= x >> 7;
	x *= 0x85ebu;
	x ^= x >> 5;

	switch (mode) {
	case 1:
		/* Every byte at or above 0x80: reaches mask[8..15]. */
		return (unsigned char)((x & 0x7fu) | 0x80u);
	case 2:
		/* Every byte below 0x80: reaches only mask[0..7]. */
		return (unsigned char)(x & 0x7fu);
	case 3:
		/* Few distinct nibbles, so bytes collide in one mask entry. */
		return (unsigned char)(((id + n) & 3) * 0x11u);
	default:
		return (unsigned char)x;
	}
}

struct shape {
	unsigned size[6];	/* constellationSize                       */
	int aid[6];		/* content id of the first table           */
	int bid[6];		/* content id of the second table          */
	int pokeK;		/* -1, or a constellation to disturb       */
	int pokeAt;		/* byte offset of the disturbance          */
};

/*
 * `mode` varies the byte alphabet; `shape` varies everything the loops key
 * off.  Both parameter blocks are filled from the same fill and are byte
 * identical when this returns.
 */
static void
seed_params(int trial, int mode, const struct shape *sh, int setIndex)
{
	unsigned char *p = (unsigned char *)&ourParams;
	unsigned int i;
	int k, n;

	lfsr_state = 0x5eedu + 0x4f1bu * (unsigned)trial + (unsigned)mode + 1u;

	for (i = 0; i < sizeof(ourParams); i++)
		p[i] = next_byte();

	for (k = 0; k < 6; k++) {
		ourParams.constellationSize[k] = sh->size[k];
		for (n = 0; n < V90_CONSTELLATION_MAX; n++) {
			ourParams.constellation[k][n] =
				table_byte(sh->aid[k], n, trial, mode);
			ourParams.codecConstellation[k][n] =
				table_byte(sh->bid[k] + 64, n, trial, mode);
		}
	}

	if (sh->pokeK >= 0)
		ourParams.constellation[sh->pokeK][sh->pokeAt] ^= 0x5a;

	/*
	 * The mask fixture's permutation.  Not the identity, and it reaches
	 * every constellation exactly once.
	 */
	if (setIndex) {
		static const int perm[6] = { 3, 5, 0, 4, 1, 2 };

		for (k = 0; k < 6; k++)
			ourParams.distinctIndex[k] = perm[k];
	}

	memcpy(&theirParams, &ourParams, sizeof(ourParams));
}

static void
seed_masks(int trial)
{
	int i;

	lfsr_state = 0x1234u + 0x9e37u * (unsigned)trial + 1u;
	for (i = 0; i < NMASK; i++) {
		short v = (short)(0x5a00 + (next_byte() & 0xff));

		seedMask.v[i] = v;
		ourMask.v[i] = v;
		theirMask.v[i] = v;
	}
}

static void
seed_groups(int trial)
{
	int i;

	lfsr_state = 0x77abu + 0x1d2fu * (unsigned)trial + 1u;
	for (i = 0; i < 8; i++) {
		int v = 0x3c00 + (int)next_byte();

		seedGroup.v[i] = v;
		ourGroup.v[i] = v;
		theirGroup.v[i] = v;
	}
}

/* ------------------------------------------------------------------ shapes */

#define POKE_NONE	-1

static const struct shape shapes[] = {
	/* six distinct constellations */
	{ { 8, 8, 8, 8, 8, 8 }, { 0, 1, 2, 3, 4, 5 }, { 0, 1, 2, 3, 4, 5 },
	  POKE_NONE, 0 },
	/* one constellation, six times */
	{ { 8, 8, 8, 8, 8, 8 }, { 7, 7, 7, 7, 7, 7 }, { 9, 9, 9, 9, 9, 9 },
	  POKE_NONE, 0 },
	/* EVERY LENGTH ZERO: the byte loop never runs and all six collapse */
	{ { 0, 0, 0, 0, 0, 0 }, { 0, 1, 2, 3, 4, 5 }, { 5, 4, 3, 2, 1, 0 },
	  POKE_NONE, 0 },
	/* zero lengths mixed with real ones */
	{ { 0, 4, 0, 4, 0, 4 }, { 0, 1, 2, 1, 4, 3 }, { 0, 1, 2, 1, 4, 3 },
	  POKE_NONE, 0 },
	/* the first tables agree, the second ones do not */
	{ { 6, 6, 6, 6, 6, 6 }, { 1, 1, 1, 1, 1, 1 }, { 1, 2, 3, 4, 5, 6 },
	  POKE_NONE, 0 },
	/* the second tables agree, the first ones do not */
	{ { 6, 6, 6, 6, 6, 6 }, { 1, 2, 3, 4, 5, 6 }, { 1, 1, 1, 1, 1, 1 },
	  POKE_NONE, 0 },
	/* identical content, different lengths */
	{ { 1, 2, 3, 4, 5, 6 }, { 2, 2, 2, 2, 2, 2 }, { 2, 2, 2, 2, 2, 2 },
	  POKE_NONE, 0 },
	/* alternating pairs */
	{ { 5, 5, 5, 5, 5, 5 }, { 0, 1, 0, 1, 0, 1 }, { 0, 1, 0, 1, 0, 1 },
	  POKE_NONE, 0 },
	/* the full table */
	{ { 128, 128, 128, 128, 128, 128 }, { 0, 0, 1, 1, 0, 1 },
	  { 0, 0, 1, 1, 0, 1 }, POKE_NONE, 0 },
	/* a difference at the LAST byte inside the length: not equal */
	{ { 16, 16, 16, 16, 16, 16 }, { 3, 3, 3, 3, 3, 3 },
	  { 3, 3, 3, 3, 3, 3 }, 2, 15 },
	/* a difference just PAST the length: still equal */
	{ { 16, 16, 16, 16, 16, 16 }, { 3, 3, 3, 3, 3, 3 },
	  { 3, 3, 3, 3, 3, 3 }, 2, 16 },
	/* a difference at the first byte */
	{ { 16, 16, 16, 16, 16, 16 }, { 3, 3, 3, 3, 3, 3 },
	  { 3, 3, 3, 3, 3, 3 }, 4, 0 },
	/* one long, the rest short */
	{ { 127, 1, 1, 1, 1, 127 }, { 8, 9, 9, 9, 9, 8 },
	  { 8, 9, 9, 9, 9, 8 }, POKE_NONE, 0 },
	/* a single zero-length among distinct ones */
	{ { 3, 3, 0, 3, 3, 3 }, { 0, 1, 2, 3, 4, 0 }, { 0, 1, 2, 3, 4, 0 },
	  POKE_NONE, 0 }
};

#define NSHAPE ((int)(sizeof(shapes) / sizeof(shapes[0])))
#define NMODE 4

/*
 * Shape 2 is six DISTINCT contents whose lengths are all zero; shape 10 is
 * six identical ones disturbed only past the length.  Both are named here so
 * that reordering the table cannot silently retarget the assertions.
 */
#define SHAPE_ALL_EMPTY		2
#define SHAPE_POKE_PAST_LEN	10
#define SHAPE_POKE_IN_LEN	9

/* --------------------------------------------------------------- the index */

static int
run_index(void)
{
	int s, mode, trial = 0;
	int sawOne = 0, sawSix = 0, sawMiddle = 0;
	int sawEmptyCollapse = 0, sawPastLenEqual = 0, sawInLenSplit = 0;
	int wroteIndex = 0;

	diff_begin("getConstellationsIndex");

	for (mode = 0; mode < NMODE; mode++)
		for (s = 0; s < NSHAPE; s++, trial++) {
			unsigned int ourN, theirN;
			V90MappingParams before;

			seed_params(trial, mode, &shapes[s], 0);
			seed_groups(trial);
			memcpy(&before, &ourParams, sizeof(before));

			ourN = getConstellationsIndex(&ourParams, ourGroup.v);
			theirN = blobIndex(&theirParams, theirGroup.v);

			diff_eq_int("the group count (trial %ld)",
				    (long)ourN, (long)theirN, trial);
			diff_eq_obj("the group of each constellation",
				    struct groupBuf, &ourGroup, &theirGroup,
				    trial);
			diff_eq_obj("the parameter block", V90MappingParams,
				    &ourParams, &theirParams, trial);
			diff_eq_int("the group guard entries are untouched "
				    "(trial %ld)",
				    theirGroup.v[6] == seedGroup.v[6] &&
				    theirGroup.v[7] == seedGroup.v[7], 1,
				    trial);

			if (memcmp(&before, &theirParams, sizeof(before)) != 0)
				wroteIndex = 1;

			if (theirN == 1)
				sawOne = 1;
			if (theirN == 6)
				sawSix = 1;
			if (theirN > 1 && theirN < 6)
				sawMiddle = 1;
			if (s == SHAPE_ALL_EMPTY && theirN == 1)
				sawEmptyCollapse = 1;
			if (s == SHAPE_POKE_PAST_LEN && theirN == 1)
				sawPastLenEqual = 1;
			if (s == SHAPE_POKE_IN_LEN && theirN == 2)
				sawInLenSplit = 1;
		}

	/*
	 * The blob wrote every group entry: no entry still holds its seed.
	 * Without this the object could be writing nothing at all and the two
	 * sides would still agree -- the vacuous case this tree has hit three
	 * times.
	 */
	{
		int i, untouched = 0;

		seed_params(9999, 0, &shapes[0], 0);
		seed_groups(9999);
		(void)blobIndex(&theirParams, theirGroup.v);
		for (i = 0; i < 6; i++)
			if (theirGroup.v[i] == seedGroup.v[i])
				untouched++;
		diff_eq_int("every group entry was written", untouched, 0, 0);
	}

	diff_eq_int("a fixture collapsed to one group", sawOne, 1, 0);
	diff_eq_int("a fixture stayed at six groups", sawSix, 1, 0);
	diff_eq_int("a fixture landed between the two", sawMiddle, 1, 0);
	diff_eq_int("six distinct EMPTY constellations collapse to one",
		    sawEmptyCollapse, 1, 0);
	diff_eq_int("a difference past the length is not a difference",
		    sawPastLenEqual, 1, 0);
	diff_eq_int("a difference at the last byte inside the length splits",
		    sawInLenSplit, 1, 0);
	diff_eq_int("the parameter block was modified", wroteIndex, 1, 0);

	return diff_end();
}

/* --------------------------------------------------------------- the masks */

static int
run_masks(void)
{
	int s, mode, which, trial = 0;
	int sawBit = 0, sawHighEntry = 0, sawEmptyMask = 0;
	int sawTablesDiffer = 0, sawWhichMatters = 0;

	diff_begin("getConstellationMask / getCodecConstellationMask");

	for (mode = 0; mode < NMODE; mode++)
		for (s = 0; s < NSHAPE; s++, trial++)
			for (which = 0; which < 10; which++) {
				struct maskBuf codec;
				int i, allZero = 1, guardOK = 1;

				/* --- the first table --- */
				seed_params(trial * 16 + which, mode,
					    &shapes[s], 1);
				seed_masks(trial * 16 + which);

				getConstellationMask(&ourParams, which,
						     ourMask.v);
				blobMask(&theirParams, which, theirMask.v);

				diff_eq_obj("the constellation mask",
					    struct maskBuf, &ourMask,
					    &theirMask, trial);
				diff_eq_obj("the parameter block",
					    V90MappingParams, &ourParams,
					    &theirParams, trial);

				memcpy(&codec, &theirMask, sizeof(codec));

				for (i = 0; i < 8; i++)
					if (theirMask.v[i] != 0)
						sawBit = 1, allZero = 0;
				for (i = 8; i < 16; i++)
					if (theirMask.v[i] != seedMask.v[i])
						sawHighEntry = 1;
				for (i = 16; i < NMASK; i++)
					if (theirMask.v[i] != seedMask.v[i])
						guardOK = 0;
				if (allZero)
					sawEmptyMask = 1;

				diff_eq_int("nothing past mask[15] was written "
					    "(trial %ld)", guardOK, 1, trial);

				/* --- the second table, same inputs --- */
				seed_params(trial * 16 + which, mode,
					    &shapes[s], 1);
				seed_masks(trial * 16 + which);

				getCodecConstellationMask(&ourParams, which,
							  ourMask.v);
				blobCodecMask(&theirParams, which,
					      theirMask.v);

				diff_eq_obj("the codec constellation mask",
					    struct maskBuf, &ourMask,
					    &theirMask, trial);
				diff_eq_obj("the parameter block",
					    V90MappingParams, &ourParams,
					    &theirParams, trial);

				if (memcmp(&codec, &theirMask,
					   sizeof(codec)) != 0)
					sawTablesDiffer = 1;
			}

	/*
	 * `which` is not ignored: with the permutation above, entry 0 and
	 * entry 1 select constellations 3 and 5, and shape 0 gives those two
	 * different contents.  `which` at or above 6 selects entry 0, which is
	 * the object's `cmp $0x6; setl; neg; and`.
	 */
	{
		struct maskBuf a, b, c;

		seed_params(4242, 0, &shapes[0], 1);
		seed_masks(4242);
		blobMask(&theirParams, 0, theirMask.v);
		memcpy(&a, &theirMask, sizeof(a));

		seed_params(4242, 0, &shapes[0], 1);
		seed_masks(4242);
		blobMask(&theirParams, 1, theirMask.v);
		memcpy(&b, &theirMask, sizeof(b));

		seed_params(4242, 0, &shapes[0], 1);
		seed_masks(4242);
		blobMask(&theirParams, 9, theirMask.v);
		memcpy(&c, &theirMask, sizeof(c));

		if (memcmp(&a, &b, sizeof(a)) != 0)
			sawWhichMatters = 1;

		diff_eq_obj("`which` at or above 6 selects entry 0",
			    struct maskBuf, &c, &a, 0);
	}

	diff_eq_int("some mask had a bit set in entries 0..7", sawBit, 1, 0);
	diff_eq_int("some mask reached entries 8..15, which are not cleared",
		    sawHighEntry, 1, 0);
	diff_eq_int("some mask was empty, so the loop bound was reached",
		    sawEmptyMask, 1, 0);
	diff_eq_int("the two functions read different tables",
		    sawTablesDiffer, 1, 0);
	diff_eq_int("`which` selects different constellations",
		    sawWhichMatters, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_index();
	rc |= run_masks();

	return rc;
}

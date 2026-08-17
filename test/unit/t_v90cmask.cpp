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
#include "dsplib/V92CP.h"
#include "dsplib/tagV90AdditionalCPinfo.h"

extern "C" {
extern unsigned int dsplibs_debug_level;
extern unsigned int ref_dsplibs_debug_level;

unsigned int blobIndex(V90MappingParams *params, int *group)
	asm("ref_getConstellationsIndex");
void blobMask(V90MappingParams *params, int which, short *mask)
	asm("ref_getConstellationMask");
void blobCodecMask(V90MappingParams *params, int which, short *mask)
	asm("ref_getCodecConstellationMask");
void blobPack(V90MappingParams *params, tagV90AdditionalCPinfo *info,
	      V92CP *cp) asm("ref_setV92CPpckFromParamsInfo");
void blobDisplay(V90MappingParams *params) asm("ref_displaySpectralParams");
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

/* ================================================================= the pack */

/*
 * `setV92CPpckFromParamsInfo` -- the same six constellations, packed into a
 * `V92CP` together with five dwords out of a `tagV90AdditionalCPinfo`.
 *
 * WHY IT IS IN THIS FILE AND NOT ITS OWN.  It is `getConstellationsIndex`,
 * `getConstellationMask` and `getCodecConstellationMask` inlined by the
 * compiler, over the same block the fixture above already knows how to shape;
 * every case in `shapes[]` is a case for this function too, and a separate
 * fixture would be a second copy of `seed_params` drifting away from this one.
 *
 * WHAT IS COMPARED is the whole 0x918-byte `V92CP` on each side, plus the
 * `V90MappingParams` -- which this function WRITES, through
 * `getConstellationsIndex`'s `distinctIndex` -- and the `tagV90AdditionalCPinfo`,
 * which it must not write at all.
 *
 * `word_10c` IS NOT SWEPT PAST SIX.  Both mask blocks hold six groups and the
 * two loops trust the field, so a larger value writes past `short_42` into
 * `short_a2` and past `short_a2` into `word_104` -- D570 measured exactly that
 * on `V92CP::infoToBits`.  The value here is `getConstellationsIndex`'s return,
 * which is 1..6 by construction, and the fixture does not disturb it: a trial
 * that reaches undefined behaviour in the RECONSTRUCTION is not a differential
 * trial (D561).  What IS asserted is that the field ends up in 1..6, which is
 * the property that keeps the two loops in bounds.
 *
 * AND THE BYTE ALPHABET IS RESTRICTED FOR THE SAME REASON, WHICH IS THE PART
 * THAT COST A RUN.  `getConstellationMask` clears `mask[0..7]` and then sets
 * `mask[b >> 4]` UNMASKED, so a table byte of 0x80 or more addresses entries 8
 * to 15 -- the object's behaviour, driven deliberately by `run_masks` above
 * into a buffer with eight guard entries.  Here the buffer is a ROW of
 * `cp->short_42`, so the same write runs into the next row, and at row 5 it
 * runs out of `short_42` into `short_a2`; the codec loop's row 5 then reaches
 * +0x102..+0x111, which is `word_104`, `suv` AND `word_10c` -- the field that
 * is bounding the loop it is inside.  Both sides do it and both sides agree,
 * and it is still out of bounds in OUR source, which D561 says is not a trial.
 *
 * So the two alphabets that can produce a byte at or above 0x80 are left out
 * of this sweep, and D790 records the behaviour with the measurement rather
 * than the sweep driving it.  Modes 2 and 3 are the two that cannot: mode 2
 * masks every byte below 0x80 and mode 3's are `((id + n) & 3) * 0x11`, at
 * most 0x33.
 */
static V92CP ourCp, theirCp;
static tagV90AdditionalCPinfo ourInfo, theirInfo;

static void
seed_cp(int trial, int gate)
{
	unsigned char *a = (unsigned char *)&ourCp;
	unsigned char *b = (unsigned char *)&ourInfo;
	unsigned int i;

	lfsr_state = 0x2f19u + 0x7c1du * (unsigned)trial + 1u;
	for (i = 0; i < sizeof(ourCp); i++)
		a[i] = next_byte();
	for (i = 0; i < sizeof(ourInfo); i++)
		b[i] = next_byte();

	/*
	 * `byte_24` is a GATE and comes from `params->word_61c`, not from the
	 * fill -- so it is set there rather than here, and both values are
	 * driven.  What the fill leaves in `cp->byte_24` is overwritten before
	 * the gate is read.
	 */
	ourParams.word_61c = (unsigned int)(gate ? 0x1234ff01u : 0x1234ff00u);
	theirParams.word_61c = ourParams.word_61c;

	/*
	 * Through `unsigned char *`: `V92CP` has a constructor, so GCC 13
	 * warns about a `memcpy` naming the class, and the point here is the
	 * BYTES rather than the object.
	 */
	memcpy((unsigned char *)&theirCp, a, sizeof(theirCp));
	memcpy((unsigned char *)&theirInfo, b, sizeof(theirInfo));
}

static int
run_pack(void)
{
	int s, mode, gate, trial = 0;
	int sawGateOn = 0, sawGateOff = 0;
	int sawShortMsg = 0, sawLongMsg = 0;
	int sawOneGroup = 0, sawSixGroups = 0;
	int sawMaskWritten = 0, sawCodecUntouched = 0;

	diff_begin("setV92CPpckFromParamsInfo");

	for (s = 0; s < NSHAPE; s++)
	for (mode = 2; mode < NMODE; mode++)		/* see the note above */
	for (gate = 0; gate <= 1; gate++) {
		long t = trial;
		unsigned before_a2;

		seed_params(trial, mode, &shapes[s], 0);
		seed_cp(trial, gate);
		before_a2 = (unsigned)(unsigned short)ourCp.short_a2[0][0];

		setV92CPpckFromParamsInfo(&ourParams, &ourInfo, &ourCp);
		blobPack(&theirParams, &theirInfo, &theirCp);

		diff_eq_obj("V92CP block", V92CP, &ourCp, &theirCp, t);
		diff_eq_obj("mapping params", V90MappingParams,
			    &ourParams, &theirParams, t);
		diff_eq_obj("additional CP info", tagV90AdditionalCPinfo,
			    &ourInfo, &theirInfo, t);

		/*
		 * The record is READ-ONLY, which the comparison above cannot
		 * say on its own -- both sides writing the same thing would
		 * still agree.
		 */
		diff_eq_int("the record is not written %ld",
			    memcmp(&ourInfo, &theirInfo, sizeof(ourInfo)) == 0,
			    1, t);

		/*
		 * The group count is what bounds both mask loops, and it has
		 * to be in range for the trial to be a trial at all.
		 */
		diff_eq_int("group count in 1..6 %ld",
			    ourCp.word_10c >= 1 && ourCp.word_10c <= 6, 1, t);
		if (ourCp.word_10c == 1)
			sawOneGroup = 1;
		if (ourCp.word_10c == 6)
			sawSixGroups = 1;

		/*
		 * THE GATE IS A SEPARATING TRIAL AND THE OBSERVABLE IS THE
		 * SECOND MASK BLOCK.  With `byte_24` zero the codec block must
		 * still hold the fill; with it non-zero it must not.  Held
		 * against our side alone, so it is a claim about the
		 * reconstruction and not about agreement.
		 */
		if (gate) {
			sawGateOn = 1;
			if ((unsigned)(unsigned short)ourCp.short_a2[0][0]
			    != before_a2)
				sawMaskWritten = 1;
		} else {
			sawGateOff = 1;
			diff_eq_int("the gate leaves the codec block %ld",
				    (unsigned)(unsigned short)
				    ourCp.short_a2[0][0], (long)before_a2, t);
			sawCodecUntouched = 1;
		}

		/*
		 * `char_01` selects between the two closing constants, and it
		 * comes from the record's +0x04 -- which the varied fill makes
		 * zero about one trial in 256, so both arms are counted rather
		 * than assumed.
		 */
		if (ourCp.char_01 == 0)
			sawShortMsg = 1;
		else
			sawLongMsg = 1;

		trial++;
	}

	/*
	 * The `char_01` = 0 arm is rare under a varied fill, so it gets its
	 * own trials rather than being hoped for.  Both constants are checked
	 * against our side directly: 0x14 and 8 differ by 12, so a wrong one
	 * is visible in the byte and the differential would catch it anyway --
	 * but only if this arm is ever entered, which is the part that needed
	 * arranging.
	 */
	{
		int k;

		for (k = 0; k <= 1; k++) {
			long t = 900 + k;

			seed_params(3, 2, &shapes[0], 0);
			seed_cp(3, 1);
			ourInfo.word_04 = theirInfo.word_04 = (unsigned)k;
			ourParams.word_0 = theirParams.word_0 = 0x40u;

			setV92CPpckFromParamsInfo(&ourParams, &ourInfo,
						  &ourCp);
			blobPack(&theirParams, &theirInfo, &theirCp);

			diff_eq_obj("V92CP block, closing byte", V92CP,
				    &ourCp, &theirCp, t);
			diff_eq_int("the closing byte %ld",
				    (long)ourCp.char_02,
				    k ? (long)(0x40 - 0x14) : (long)(0x40 - 8),
				    t);
			if (k == 0)
				sawShortMsg = 1;
			else
				sawLongMsg = 1;
		}
	}

	diff_eq_int("the codec gate was on", sawGateOn, 1, 0);
	diff_eq_int("the codec gate was off", sawGateOff, 1, 0);
	diff_eq_int("the codec block was written", sawMaskWritten, 1, 0);
	diff_eq_int("the codec block was left alone", sawCodecUntouched, 1, 0);
	diff_eq_int("char_01 zero was reached", sawShortMsg, 1, 0);
	diff_eq_int("char_01 non-zero was reached", sawLongMsg, 1, 0);
	diff_eq_int("one group was reached", sawOneGroup, 1, 0);
	diff_eq_int("six groups were reached", sawSixGroups, 1, 0);

	return diff_end();
}

/* ============================================================== the display */

/*
 * `displaySpectralParams` -- SIX `edprintf` LINES AND NOTHING ELSE.  It writes
 * no memory and returns nothing, so the transcript is its whole observable
 * surface and this is a tier-4 test in `docs/method/tiers.md`'s sense.
 * `test/unit/t_printtitle.cpp` is the template and its two rules are followed
 * here: BOTH `dsplibs_debug_level` and `ref_dsplibs_debug_level` are raised
 * together, and level 0 is tested with its own anti-vacuity guard.
 *
 * WHAT LEVEL 0 PROVES HERE IS DIFFERENT FROM `printTitle`'s, and the
 * difference is worth stating.  `printTitle`'s call sites are GATED -- each
 * one is behind `dsplibs_debug_level > 1` -- so level 0 proves the gates did
 * not fire.  There is no gate anywhere in this function: `tools/dis.py` over
 * 0x33ec0..0x3412b shows fifteen relocations and not one `cmpl $0x1,
 * dsplibs_debug_level`.  So at level 0 all six calls happen, `edprintf`
 * formats and encodes all six lines and prints none of them, and what the
 * empty transcript proves is that `edprintf` swallowed them.  Both readings
 * are checked -- the transcripts agree, and they are empty exactly when the
 * level is down.
 *
 * THE VALUES ARE CHOSEN FOR THE LAST DIGIT.  The `%06d` fraction is
 * `abs((int)((v - (int)v) * 1e6))`, computed in the x87's extended registers,
 * so a reconstruction that rounded the intermediate through `float` would
 * agree on most inputs and differ on the ones whose scaled fraction lands
 * within a few units of an integer.  `edge[]` below is those: both zeros,
 * exact halves and quarters, sixths and thirds, values a unit in the last
 * place under and over a whole number, the two ends of `float`'s exact
 * integer range at 2^23, and two denormals.
 *
 * EVERY ENTRY CONVERTS TO `int` WITHOUT OVERFLOWING, and that is a
 * constraint and not an accident.  Both helpers evaluate `(int)v` -- once as
 * `(int)fabsf(v)` and once inside the subtraction -- and an out-of-range
 * conversion is undefined in C, so a trial holding one is not a differential
 * trial (D561).  On x87 it would not even look like one: `fistl` answers the
 * integer indefinite deterministically, so both sides would agree and the
 * trial would read as a pass.  The largest magnitudes here are +/-2.0e9,
 * inside `INT_MAX`, and the smallest are denormals that convert to zero.
 *
 * WHAT IS THEREFORE NOT DRIVEN is the infinities and the NaNs, for the same
 * reason and with the same consequence: nothing here says what either
 * function does with them.  D790 is this batch's other entry of that shape.
 */
static const float edge[] = {
	0.0f, -0.0f, 1.0f, -1.0f,
	0.5f, -0.5f, 0.25f, 0.125f,
	1.0f / 3.0f, -1.0f / 3.0f, 2.0f / 3.0f, 1.0f / 6.0f,
	0.999999f, 0.9999995f, 1.000001f, -0.999999f,
	0.1f, 0.2f, 0.3f, 0.7f,
	123.456789f, -98765.4321f, 1e-7f, -1e-7f,
	8388607.0f, 8388608.0f, 16777216.0f, -16777216.0f,
	1.1754944e-38f, 5.877472e-39f, 2.0e9f, -2.0e9f
};
#define NEDGE ((int)(sizeof(edge) / sizeof(edge[0])))

static int
run_display(void)
{
	static const unsigned levels[4] = { 0u, 1u, 2u, 3u };
	int li, k, trial = 0;
	int sawPrinted = 0, sawSilent = 0, sawNegative = 0, sawFraction = 0;

	diff_begin("displaySpectralParams");

	for (li = 0; li < 4; li++)
	for (k = 0; k < NEDGE; k++) {
		long t = trial;
		unsigned lines_ours, lines_theirs;

		seed_params(k, 0, &shapes[0], 0);
		ourParams.shaperSR = theirParams.shaperSR =
		    (int)(k * 37 - 300);
		ourParams.shaperId = theirParams.shaperId =
		    (unsigned int)(k * 0x01010101u);
		ourParams.shaperA1 = theirParams.shaperA1 = edge[k];
		ourParams.shaperA2 = theirParams.shaperA2 =
		    edge[(k + 7) % NEDGE];
		ourParams.shaperB1 = theirParams.shaperB1 =
		    edge[(k + 13) % NEDGE];
		ourParams.shaperB2 = theirParams.shaperB2 =
		    edge[(k + 19) % NEDGE];

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = levels[li];
		ref_dsplibs_debug_level = levels[li];

		displaySpectralParams(&ourParams);
		blobDisplay(&theirParams);

		dsplibs_debug_level = 0u;
		ref_dsplibs_debug_level = 0u;
		dsplib_debug_capture_on = 0;

		lines_ours = dsplib_debug_capture_lines(0);
		lines_theirs = dsplib_debug_capture_lines(1);

		diff_eq_int("transcript line count %ld", (long)lines_ours,
			    (long)lines_theirs, t);
		diff_eq_int("transcript text %ld",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, t);

		/*
		 * IT WRITES NOTHING, which no transcript comparison can say.
		 * Both blocks are compared to catch a store, and ours is
		 * compared against the seed to catch two sides storing the
		 * same thing.
		 */
		diff_eq_obj("mapping params", V90MappingParams,
			    &ourParams, &theirParams, t);

		/* The anti-vacuity guard, both sides separately. */
		if (levels[li] > 1u) {
			diff_eq_int("we printed at level > 1 %ld",
				    lines_ours > 0, 1, t);
			diff_eq_int("the blob printed too %ld",
				    lines_theirs > 0, 1, t);
			sawPrinted = 1;
			if (edge[k] < 0.0f)
				sawNegative = 1;
			if (edge[k] != (float)(int)edge[k])
				sawFraction = 1;
		} else {
			diff_eq_int("nothing reached the log %ld",
				    (long)lines_ours, 0, t);
			diff_eq_int("...on the blob's side either %ld",
				    (long)lines_theirs, 0, t);
			sawSilent = 1;
		}

		trial++;
	}

	/*
	 * SIX LINES AND NOT FIVE OR SEVEN, taken from the BLOB at level 2 so
	 * that it is a statement about the object.  A reconstruction that
	 * dropped one of the four float prints would be caught by the text
	 * comparison above; this is what catches the harness quietly counting
	 * something other than lines.
	 */
	{
		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = ref_dsplibs_debug_level = 2u;
		blobDisplay(&theirParams);
		diff_eq_int("the blob prints six lines",
			    (long)dsplib_debug_capture_lines(1), 6, 0);
		dsplib_debug_capture_reset();
		displaySpectralParams(&ourParams);
		diff_eq_int("...and so do we",
			    (long)dsplib_debug_capture_lines(0), 6, 0);
		dsplibs_debug_level = ref_dsplibs_debug_level = 0u;
		dsplib_debug_capture_on = 0;
	}

	diff_eq_int("something was printed", sawPrinted, 1, 0);
	diff_eq_int("the silent levels were driven", sawSilent, 1, 0);
	diff_eq_int("a negative value was printed", sawNegative, 1, 0);
	diff_eq_int("a fractional value was printed", sawFraction, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_index();
	rc |= run_masks();
	rc |= run_pack();
	rc |= run_display();

	return rc;
}

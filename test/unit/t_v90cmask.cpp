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
#include "dsplib/V90CPpck.h"
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
int blobRate(V90MappingParams *params, int islong) asm("ref_getDataBitRate");
}

/*
 * THE TWO MANGLED ONES NEED THE MANGLED `ref_` NAME.  `V90CPPacker` and the
 * `short *` overload of `float2Bits` are C++ symbols, so the alias the
 * Makefile builds is `ref_` prefixed to the MANGLING and not to the source
 * name -- and `_Z10float2BitsfPhi`, the `unsigned char *` overload at
 * .text+0x4ec00, is a different function that must not be reached from here.
 * Writing the asm name out is what makes which one is being tested explicit.
 */
extern float blobFltTable1[7] asm("ref_fltTable1");
extern float blobFltTable2[16] asm("ref_fltTable2");

/*
 * `V92CP.cpp`'s SEPARATE pair, at .data+0x69e0 and +0x6a20.  Declared here
 * only so that the two can be shown to be different objects -- see
 * `run_float2bits`.  Not `V90CPpck.h`'s job: that header owns the unsuffixed
 * pair and nothing else.
 */
extern float fltTable_1[7];
extern float fltTable_2[16];

int blobPacker(V90MappingParams *params, tagV90AdditionalCPinfo *info,
	       short *bits, int cleardown)
	asm("ref__Z11V90CPPackerP16V90MappingParamsP22tagV90AdditionalCPinfoPsi");
void blobF2B(float f, short *bits, int mode) asm("ref__Z10float2BitsfPsi");

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

/* ========================================================== float2Bits */

/*
 * `float2Bits(float, short *, int)` -- .text+0x3be30, the `short *` overload
 * and NOT the `unsigned char *` one at .text+0x4ec00.  The `asm` name at the
 * top of this file is what settles that; the two are ordinary overloads and
 * share nothing.
 *
 * FORTY ENTRIES FOR A FUNCTION THAT REACHES SIXTEEN.  The tail is guard
 * space, and the whole buffer is seeded with 0x5a?? on both sides -- which
 * matters more here than anywhere else in this file, because the third arm of
 * the mode test WRITES NOTHING AT ALL.  Against a zero-filled buffer "mode 2
 * returns immediately" and "mode 2 expands to all zeros" are the same
 * observation, and 7105 is that mistake three times over.
 *
 * WHAT EACH INPUT CLASS IS FOR:
 *
 *   2^-8 exactly    THE TABLE QUIRK.  `fltTable2` has no 2^-8 and holds
 *                   2^-9 TWICE, so 0.00390625 sets entries 10 and 11 --
 *                   bits[5] and bits[4] -- where a repaired geometric table
 *                   would set entry 10 alone.  Nothing else in the sweep can
 *                   tell a transcribed table from a generated one.
 *   a negative      Mode 0 has NO sign entry: the object takes `fabs` and
 *                   sends the magnitude, so f and -f must give the same
 *                   sixteen bits.  Checked directly, below.
 *   out of range    8.5, -0.001 (mode 0) and 1.5, -1.5 (mode 1) reach the
 *                   two warning arms, which are the ONLY observable those
 *                   arms have -- nothing is clamped.  Driven at all four
 *                   debug levels so the `> 1` gate is measured and not
 *                   assumed.
 *   the two sums    7.9998779296875 is every `fltTable2` weight added up and
 *                   0.984375 is every `fltTable1` one, so the expansion
 *                   saturates: every magnitude entry set and no remainder.
 *   below the last  1e-7 is under 2^-13, so every entry stays clear.
 *
 * NO NaN AND NO INFINITY, deliberately: the range tests are ordered `fcom`
 * compares under `-mno-ieee-fp` (finding 1990), which reads an unordered
 * result as "below", and `test/mutations/v92info.json` states the same
 * exclusion for the twin expansion in `V92CP`.  Every value below converts
 * and compares without reaching that.
 */
#define NFB 40

struct fbBuf {
	short v[NFB];
};

static struct fbBuf ourFb, theirFb, seedFb;

static void
seed_fb(int trial)
{
	int i;

	lfsr_state = 0x33c7u + 0x5bd1u * (unsigned)trial + 1u;
	for (i = 0; i < NFB; i++) {
		short v = (short)(0x5a00 + (next_byte() & 0xff));

		seedFb.v[i] = v;
		ourFb.v[i] = v;
		theirFb.v[i] = v;
	}
}

static const float fbEdge[] = {
	0.0f, -0.0f, 1.0f, -1.0f,
	0.5f, -0.5f, 0.25f, 0.125f,
	2.0f, 4.0f, 8.0f, 8.5f,
	7.9998779296875f, 0.984375f, -0.984375f, 3.0f,
	0.00390625f, 0.005f, 0.0029296875f, 0.001953125f,
	0.015625f, 0.0078125f, 1e-7f, -1e-7f,
	1.5f, -1.5f, -0.001f, 0.1f,
	0.3f, 0.7f, -3.0f, 123.456f
};
#define NFBEDGE ((int)(sizeof(fbEdge) / sizeof(fbEdge[0])))

/* The index of 2^-8 in fbEdge, named so reordering cannot retarget it. */
#define FBEDGE_QUIRK 16

/* The modes: two live arms and three that must write nothing. */
static const int fbModes[] = { 0, 1, 2, -1, 5 };
#define NFBMODE ((int)(sizeof(fbModes) / sizeof(fbModes[0])))

static int
run_float2bits(void)
{
	int li, k, mi, trial = 0;
	int sawWrote0 = 0, sawWrote1 = 0, sawInert = 0;
	int sawSignSet = 0, sawSignClear = 0;
	int sawSaturated = 0, sawAllClear = 0;
	int sawWarnHigh0 = 0, sawWarnLow0 = 0;
	int sawWarnHigh1 = 0, sawWarnLow1 = 0;
	int sawQuiet = 0;

	diff_begin("float2Bits(float, short *, int)");

	/*
	 * THE TABLES ARE COMPARED DIRECTLY, entry for entry, against the
	 * blob's own.  The expansion only ever reaches the entries an input
	 * can reach, so a functional sweep cannot say the transcription is
	 * right in the entries it does not exercise -- and `fltTable2`'s whole
	 * point is one entry being wrong.
	 */
	for (k = 0; k < 16; k++)
		diff_eq_float("fltTable2[%ld]", fltTable2[k],
			      blobFltTable2[k], k);
	for (k = 0; k < 7; k++)
		diff_eq_float("fltTable1[%ld]", fltTable1[k],
			      blobFltTable1[k], k);

	/*
	 * AND THEY ARE NOT `V92CP.cpp`'s PAIR.  Same contents, different
	 * symbols -- so this asserts the ADDRESSES differ, which is the only
	 * thing that separates "two tables" from "one table named twice" and
	 * is exactly what a merge of the two files would break.
	 */
	diff_eq_int("fltTable2 and fltTable_2 are different objects",
		    (const void *)fltTable2 != (const void *)fltTable_2, 1, 0);
	diff_eq_int("fltTable1 and fltTable_1 are different objects",
		    (const void *)fltTable1 != (const void *)fltTable_1, 1, 0);

	for (li = 0; li < 4; li++)
	for (mi = 0; mi < NFBMODE; mi++)
	for (k = 0; k < NFBEDGE; k++, trial++) {
		long t = trial;
		int mode = fbModes[mi];
		unsigned lines_ours, lines_theirs;
		int i, inert = 1, mags = 0;
		int nbits = (mode == 0) ? 16 : 8;

		seed_fb(trial);

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = (unsigned)li;
		ref_dsplibs_debug_level = (unsigned)li;

		float2Bits(fbEdge[k], ourFb.v, mode);
		blobF2B(fbEdge[k], theirFb.v, mode);

		dsplibs_debug_level = 0u;
		ref_dsplibs_debug_level = 0u;
		dsplib_debug_capture_on = 0;

		lines_ours = dsplib_debug_capture_lines(0);
		lines_theirs = dsplib_debug_capture_lines(1);

		diff_eq_obj("the bit vector", struct fbBuf, &ourFb, &theirFb,
			    t);
		diff_eq_int("transcript line count %ld", (long)lines_ours,
			    (long)lines_theirs, t);
		diff_eq_int("transcript text %ld",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, t);

		/* Nothing past what the mode reaches, on the blob's side. */
		for (i = nbits; i < NFB; i++)
			if (theirFb.v[i] != seedFb.v[i])
				diff_eq_int("guard entry %ld was written",
					    0, 1, i);

		for (i = 0; i < NFB; i++)
			if (theirFb.v[i] != seedFb.v[i])
				inert = 0;

		if (mode != 0 && mode != 1) {
			/*
			 * THE INERT ARM.  Not "wrote zeros" -- the seed is
			 * still there, every entry, which is a claim a
			 * zero-filled fixture cannot make.
			 */
			diff_eq_int("mode %ld touches nothing", inert, 1,
				    mode);
			sawInert = 1;
		} else {
			diff_eq_int("mode %ld wrote something", inert, 0,
				    mode);
			if (mode == 0)
				sawWrote0 = 1;
			else
				sawWrote1 = 1;
		}

		if (mode == 0) {
			for (i = 0; i < 16; i++)
				if (theirFb.v[i] != 0)
					mags++;
			if (mags == 16)
				sawSaturated = 1;
			if (mags == 0)
				sawAllClear = 1;
		} else if (mode == 1) {
			if (theirFb.v[7] != 0)
				sawSignSet = 1;
			else
				sawSignClear = 1;
			for (i = 0; i < 7; i++)
				if (theirFb.v[i] != 0)
					mags++;
			if (mags == 7)
				sawSaturated = 1;
		}

		/*
		 * THE FOUR WARNING ARMS, counted on the BLOB's transcript so
		 * they are statements about the object.  Only at level > 1:
		 * below it the arm is still entered and prints nothing, which
		 * is what `sawQuiet` records.
		 */
		if (li > 1) {
			if (mode == 0 && fbEdge[k] > 8.0f && lines_theirs > 0)
				sawWarnHigh0 = 1;
			if (mode == 0 && fbEdge[k] < 0.0f && lines_theirs > 0)
				sawWarnLow0 = 1;
			if (mode == 1 && fbEdge[k] > 1.0f && lines_theirs > 0)
				sawWarnHigh1 = 1;
			if (mode == 1 && fbEdge[k] < -1.0f && lines_theirs > 0)
				sawWarnLow1 = 1;
		} else if ((mode == 0 && (fbEdge[k] > 8.0f ||
					  fbEdge[k] < 0.0f)) ||
			   (mode == 1 && (fbEdge[k] > 1.0f ||
					  fbEdge[k] < -1.0f))) {
			diff_eq_int("the gate held at level %ld",
				    (long)lines_theirs, 0, li);
			sawQuiet = 1;
		}
	}

	/*
	 * THE TABLE QUIRK, ON OUR SIDE DIRECTLY.  2^-8 is absent from
	 * `fltTable2` and 2^-9 is there twice, so the greedy expansion of
	 * 0.00390625 sets entries 10 AND 11 -- bits[5] and bits[4].  A
	 * repaired table sets entry 10 only, and the differential above would
	 * catch that; this states which pattern is the right one so that a
	 * reader does not have to re-derive it.
	 */
	{
		int i;

		seed_fb(7777);
		float2Bits(0.00390625f, ourFb.v, 0);
		diff_eq_int("2^-8 sets bits[5]", ourFb.v[5], 1, 0);
		diff_eq_int("2^-8 sets bits[4] too, which is the doubled 2^-9",
			    ourFb.v[4], 1, 0);
		for (i = 0; i < 4; i++)
			diff_eq_int("...and nothing below it (bits[%ld])",
				    ourFb.v[i], 0, i);
		for (i = 6; i < 16; i++)
			diff_eq_int("...nor above it (bits[%ld])",
				    ourFb.v[i], 0, i);
		diff_eq_int("the quirk value is still in the sweep",
			    fbEdge[FBEDGE_QUIRK] == 0.00390625f, 1, 0);
	}

	/*
	 * MODE 0 HAS NO SIGN AND TAKES `fabs`: f and -f give the same sixteen
	 * bits.  Held against our side alone, so it is a claim about the
	 * reconstruction and not about two sides agreeing.
	 */
	{
		struct fbBuf pos;

		seed_fb(7778);
		float2Bits(1.5f, ourFb.v, 0);
		memcpy(&pos, &ourFb, sizeof(pos));
		seed_fb(7778);
		float2Bits(-1.5f, ourFb.v, 0);
		diff_eq_obj("mode 0 drops the sign", struct fbBuf, &ourFb,
			    &pos, 0);
	}

	/*
	 * MODE 1's SIGN IS AT bits[7], PAST the seven magnitude entries, and
	 * it is taken BEFORE the `fabs`.  A version that computed it after
	 * would store zero for every input.
	 */
	{
		seed_fb(7779);
		float2Bits(-0.5f, ourFb.v, 1);
		diff_eq_int("a negative sets bits[7]", ourFb.v[7], 1, 0);
		diff_eq_int("...and bits[6] is still the 1.0 entry",
			    ourFb.v[6], 0, 0);
		diff_eq_int("...and bits[5] is the 0.5 entry", ourFb.v[5], 1,
			    0);

		seed_fb(7780);
		float2Bits(0.5f, ourFb.v, 1);
		diff_eq_int("a positive clears bits[7]", ourFb.v[7], 0, 0);
	}

	diff_eq_int("mode 0 was driven", sawWrote0, 1, 0);
	diff_eq_int("mode 1 was driven", sawWrote1, 1, 0);
	diff_eq_int("a mode outside 0 and 1 left the seed alone", sawInert, 1,
		    0);
	diff_eq_int("the sign entry was set", sawSignSet, 1, 0);
	diff_eq_int("the sign entry was clear", sawSignClear, 1, 0);
	diff_eq_int("an expansion saturated", sawSaturated, 1, 0);
	diff_eq_int("an expansion set nothing", sawAllClear, 1, 0);
	diff_eq_int("Q3.13 warned above 8", sawWarnHigh0, 1, 0);
	diff_eq_int("Q3.13 warned below 0", sawWarnLow0, 1, 0);
	diff_eq_int("Q1.6 warned above 1", sawWarnHigh1, 1, 0);
	diff_eq_int("Q1.6 warned below -1", sawWarnLow1, 1, 0);
	diff_eq_int("the same inputs were silent below the gate", sawQuiet, 1,
		    0);

	return diff_end();
}

/* ====================================================== getDataBitRate */

/*
 * Twenty-four bytes, two arms, and the only thing worth arranging is that the
 * FLAG matters: with a fixture that only ever passes non-zero the two
 * constants are indistinguishable.  Both are driven, and the block value is
 * swept over a set that includes one small enough to make the long arm
 * NEGATIVE -- `V90CPPacker` shifts the result with `sar`, so the sign is
 * observable in the five bits it sends.
 */
static int
run_databitrate(void)
{
	static const unsigned int blockVals[] = {
		0u, 1u, 8u, 0x14u, 0x40u, 0x7fu, 0x80u,
		0xffffu, 0x80000000u, 0xffffffffu
	};
	static const int flags[] = { 0, 1, -1, 0x1234 };
	int b, f;
	int sawLong = 0, sawShort = 0, sawNegative = 0;

	diff_begin("getDataBitRate");

	for (b = 0; b < (int)(sizeof(blockVals) / sizeof(blockVals[0])); b++)
	for (f = 0; f < (int)(sizeof(flags) / sizeof(flags[0])); f++) {
		long t = (long)(b * 4 + f);
		int ours, theirs;

		seed_params(b, 0, &shapes[0], 0);
		ourParams.word_0 = theirParams.word_0 = blockVals[b];

		ours = getDataBitRate(&ourParams, flags[f]);
		theirs = blobRate(&theirParams, flags[f]);

		diff_eq_int("the rate %ld", (long)ours, (long)theirs, t);
		diff_eq_obj("the block is not written", V90MappingParams,
			    &ourParams, &theirParams, t);

		if (flags[f] != 0) {
			sawLong = 1;
			diff_eq_int("the long arm subtracts 0x14 %ld",
				    (long)ours,
				    (long)((int)blockVals[b] - 0x14), t);
		} else {
			sawShort = 1;
			diff_eq_int("the short arm subtracts 8 %ld",
				    (long)ours,
				    (long)((int)blockVals[b] - 8), t);
		}
		if (theirs < 0)
			sawNegative = 1;
	}

	diff_eq_int("the long arm was driven", sawLong, 1, 0);
	diff_eq_int("the short arm was driven", sawShort, 1, 0);
	diff_eq_int("a negative rate was produced", sawNegative, 1, 0);

	return diff_end();
}

/* ========================================================== V90CPPacker */

/*
 * `V90CPPacker` -- the analogue modem's own CP builder, and the widest arm in
 * this batch.  IT IS IN THIS FILE FOR THE REASON `run_pack` IS: what it needs
 * constructed is a populated `V90MappingParams` driving
 * `getConstellationsIndex` to a group count, the two mask functions over that
 * count, and a `tagV90AdditionalCPinfo` beside it -- which is exactly the
 * fixture above and nothing that `t_v90cp*`'s `V90CP` object provides.  The
 * packer never touches a `V90CP`.
 *
 * AND IT CAN DRIVE THE HIGH BYTE ALPHABET WHERE `run_pack` CANNOT, which is
 * the one real difference between the two.  `run_pack`'s destination is a ROW
 * of `V92CP::short_42`, so `getConstellationMask`'s unmasked `mask[b >> 4]`
 * runs into the next row and out of the object -- undefined behaviour in OUR
 * source, so D790 records it instead of driving it.  Here the destination is
 * a LOCAL sized for what the callee reaches (D791), so modes 0 and 1 are in
 * the sweep and the entries 8..15 case is exercised end to end.
 *
 * THE BIT BUFFER IS SEEDED WITH 0x5a?? AND IS 1920 LONG.  The function writes
 * 0 and 1 almost everywhere, so a zero-filled buffer would make every framing
 * store unfalsifiable (7105); and the longest message -- six groups with the
 * codec block -- ends at index 1787 and returns 1788, so everything above
 * that is guard.
 *
 * FOUR FIELDS REACH THE MESSAGE TRUNCATED RATHER THAN AS FLAGS: bits[19],
 * bits[30], bits[33] and bits[35] are `(short)` of a whole dword.  With the
 * 0/1 values a flag fixture would use, "store the low sixteen bits" and
 * "store x != 0" are the same function -- 7458's shape exactly -- so the
 * dedicated block below drives 0x1234 through bits[19] and checks the VALUE.
 * Their parity also feeds the CRC, because index 19 is inside its extent.
 *
 * THE FOUR SHAPER FLOATS AND `float_08` ARE SET FROM `fbEdge` AND NOT FROM
 * THE BYTE FILL.  A random fill produces NaNs and infinities, and
 * `float2Bits`'s range tests are ordered compares whose unordered result is
 * read as "below" under `-mno-ieee-fp`: a trial holding one would be
 * measuring the flag rather than the source, and would need a gccdiverge
 * entry to survive the modern build.  Same exclusion as `run_display`'s.
 */
#define NBITS 1920

struct bitsBuf {
	short v[NBITS];
};

static struct bitsBuf ourBits, theirBits, seedBits;

static void
seed_bits(int trial)
{
	int i;

	lfsr_state = 0x6b19u + 0x271du * (unsigned)trial + 1u;
	for (i = 0; i < NBITS; i++) {
		short v = (short)(0x5a00 + (next_byte() & 0xff));

		seedBits.v[i] = v;
		ourBits.v[i] = v;
		theirBits.v[i] = v;
	}
}

/*
 * The record.  Filled with varied bytes and then given finite floats, so that
 * every dword field is a wide value rather than a flag -- see the note above
 * on the four truncating stores.
 */
static void
seed_info(int trial)
{
	unsigned char *b = (unsigned char *)&ourInfo;
	unsigned int i;

	lfsr_state = 0x4d2bu + 0x3f07u * (unsigned)trial + 1u;
	for (i = 0; i < sizeof(ourInfo); i++)
		b[i] = next_byte();

	ourInfo.float_08 = fbEdge[trial % NFBEDGE];
	memcpy((unsigned char *)&theirInfo, b, sizeof(theirInfo));
}

static void
seed_shapers(int trial)
{
	ourParams.shaperA1 = fbEdge[(trial + 3) % NFBEDGE];
	ourParams.shaperA2 = fbEdge[(trial + 11) % NFBEDGE];
	ourParams.shaperB1 = fbEdge[(trial + 17) % NFBEDGE];
	ourParams.shaperB2 = fbEdge[(trial + 23) % NFBEDGE];
	theirParams.shaperA1 = ourParams.shaperA1;
	theirParams.shaperA2 = ourParams.shaperA2;
	theirParams.shaperB1 = ourParams.shaperB1;
	theirParams.shaperB2 = ourParams.shaperB2;
}

static int
run_packer(void)
{
	/*
	 * `word_61c` is compared against ONE with `sete`, not tested for
	 * non-zero, and it gates the codec block AND the message length.  Two
	 * is the value that separates the two readings.
	 */
	static const unsigned int gateVals[] = { 1u, 0u, 2u };
	int s, mode, g, cd, trial = 0;
	int groupsSeen = 0;
	int sawCodec = 0, sawNoCodec = 0, sawGateTwo = 0;
	int sawCleardown = 0, sawNormal = 0;
	int sawHighEntry = 0, sawLowOnly = 0;
	int sawShortRate = 0, sawLongRate = 0;

	diff_begin("V90CPPacker");

	for (s = 0; s < NSHAPE; s++)
	for (mode = 0; mode < NMODE; mode++)
	for (g = 0; g < 3; g++)
	for (cd = 0; cd <= 1; cd++, trial++) {
		long t = trial;
		int ourRet, theirRet;
		int i, groups, hi = 0;

		seed_params(trial, mode, &shapes[s], 0);
		seed_shapers(trial);
		seed_info(trial);
		seed_bits(trial);
		ourParams.word_61c = theirParams.word_61c = gateVals[g];

		ourRet = V90CPPacker(&ourParams, &ourInfo, ourBits.v, cd);
		theirRet = blobPacker(&theirParams, &theirInfo, theirBits.v,
				      cd);

		diff_eq_int("the length %ld", (long)ourRet, (long)theirRet, t);
		diff_eq_obj("the bit vector", struct bitsBuf, &ourBits,
			    &theirBits, t);
		diff_eq_obj("mapping params", V90MappingParams, &ourParams,
			    &theirParams, t);
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

		diff_eq_int("the length is in range %ld",
			    theirRet > 0 && theirRet <= NBITS, 1, t);

		/* Nothing past the returned length, on the blob's side. */
		for (i = theirRet; i < NBITS; i++)
			if (theirBits.v[i] != seedBits.v[i])
				diff_eq_int("guard entry %ld was written", 0,
					    1, i);

		/*
		 * The framing grid: every index that is a multiple of
		 * seventeen below the CRC block holds a zero, and index 0..16
		 * hold ones.  Read off the BLOB, so it is a statement about
		 * the object and not a restatement of our source.
		 */
		for (i = 0; i <= 16; i++)
			diff_eq_int("the opening ones, index %ld",
				    theirBits.v[i], 1, i);
		for (i = 17; i < theirRet - 20 + 1; i += 17)
			diff_eq_int("the framing zero at %ld",
				    theirBits.v[i], 0, i);

		groups = (theirRet - 20 - 136) / 136;
		if (theirBits.v[128] != 0)
			groups /= 2;
		if (groups >= 1 && groups <= 6)
			groupsSeen |= 1 << groups;

		if (theirBits.v[128] != 0)
			sawCodec = 1;
		else
			sawNoCodec = 1;
		if (gateVals[g] == 2u) {
			diff_eq_int("word_61c = 2 is NOT the gate %ld",
				    theirBits.v[128], 0, t);
			sawGateTwo = 1;
		}

		if (cd) {
			sawCleardown = 1;
			for (i = 20; i <= 24; i++)
				diff_eq_int("cleardown zeroes rate bit %ld",
					    theirBits.v[i], 0, i);
		} else {
			sawNormal = 1;
			if (ourInfo.word_04 != 0)
				sawLongRate = 1;
			else
				sawShortRate = 1;
		}

		/*
		 * D791's driven half: a table byte at or above 0x80 makes
		 * `getConstellationMask` write entries 8..15 of a buffer it
		 * was handed as eight.  Both sides discard those entries, and
		 * the point of counting it here is that the sweep REACHES the
		 * case rather than avoiding it as `run_pack` has to.
		 */
		for (i = 0; i < 6 && !hi; i++) {
			int n;

			for (n = 0; n < (int)ourParams.constellationSize[i];
			     n++)
				if (ourParams.constellation[i][n] >= 0x80) {
					hi = 1;
					break;
				}
		}
		if (hi)
			sawHighEntry = 1;
		else
			sawLowOnly = 1;
	}

	/*
	 * THE FOUR TRUNCATING STORES.  Wide values, so that "the low sixteen
	 * bits" and "x != 0" are separable; and the differential is what
	 * decides, with the direct reads below stating which reading is the
	 * right one.
	 */
	{
		long t = 900;

		seed_params(11, 2, &shapes[0], 0);
		seed_shapers(11);
		seed_info(11);
		seed_bits(11);
		ourParams.word_61c = theirParams.word_61c = 1u;
		ourInfo.word_04 = theirInfo.word_04 = 0x00051234u;
		ourInfo.word_10 = theirInfo.word_10 = 0x00007ffeu;
		ourInfo.word_00 = theirInfo.word_00 = 0x000300ffu;
		ourInfo.word_0c = theirInfo.word_0c = 0xfffffffdu;
		ourInfo.short_14 = theirInfo.short_14 = (short)0xa5a5;

		(void)V90CPPacker(&ourParams, &ourInfo, ourBits.v, 0);
		(void)blobPacker(&theirParams, &theirInfo, theirBits.v, 0);

		diff_eq_obj("the bit vector, wide fields", struct bitsBuf,
			    &ourBits, &theirBits, t);
		diff_eq_int("bits[19] is word_04 truncated, not a flag",
			    ourBits.v[19], 0x1234, t);
		diff_eq_int("bits[30] is word_10 truncated",
			    ourBits.v[30], 0x7ffe, t);
		diff_eq_int("bits[33] is word_00 truncated",
			    ourBits.v[33], 0x00ff, t);
		diff_eq_int("bits[35] is word_0c truncated",
			    ourBits.v[35], -3, t);

		/*
		 * Thirteen bits of a SIGNED short, LSB first.  0xa5a5 is
		 * negative and its bit 12 is 0, so a version that took twelve
		 * bits or sixteen differs, and so does one that read the field
		 * unsigned and stopped at a different place.
		 */
		{
			int i, v = (int)(short)0xa5a5;

			for (i = 0; i <= 12; i++) {
				diff_eq_int("short_14 bit %ld",
					    ourBits.v[36 + i], (v >> i) & 1,
					    i);
			}
		}
	}

	/*
	 * BOTH `getDataBitRate` ARMS THROUGH THE MESSAGE.  0x14 and 8 differ
	 * by twelve, which is not a multiple of 32, so the five bits that
	 * survive differ whatever the block value is.  The second pair drives
	 * a block small enough to make the long arm negative, which is what
	 * makes the `sar` observable.
	 */
	{
		static const unsigned int blocks[] = { 0x40u, 4u };
		int bi, k;

		for (bi = 0; bi < 2; bi++)
		for (k = 0; k <= 1; k++) {
			long t = 910 + bi * 2 + k;
			int rate, i;

			seed_params(13, 2, &shapes[0], 0);
			seed_shapers(13);
			seed_info(13);
			seed_bits(13);
			ourParams.word_61c = theirParams.word_61c = 0u;
			ourParams.word_0 = theirParams.word_0 = blocks[bi];
			ourInfo.word_04 = theirInfo.word_04 = (unsigned)k;

			(void)V90CPPacker(&ourParams, &ourInfo, ourBits.v, 0);
			(void)blobPacker(&theirParams, &theirInfo,
					 theirBits.v, 0);

			diff_eq_obj("the bit vector, rate arms",
				    struct bitsBuf, &ourBits, &theirBits, t);

			rate = (int)blocks[bi] - (k ? 0x14 : 8);
			for (i = 0; i <= 4; i++)
				diff_eq_int("rate bit %ld",
					    ourBits.v[20 + i],
					    (rate >> i) & 1, i);
			if (k)
				sawLongRate = 1;
			else
				sawShortRate = 1;
		}
	}

	/*
	 * THE MESSAGE NAME.  Three `%s` off three different fields, one of
	 * them INVERTED, and the transcript is their only observable.  All
	 * eight combinations, at level 2, compared as text -- which is what
	 * makes "CPt" versus "CP" versus "CPs'" a thing this test can tell
	 * apart.  Under the random fill all three are non-zero almost always,
	 * so this cannot be left to the sweep.
	 */
	{
		int f00, f04, f10;
		int sawName = 0;

		for (f00 = 0; f00 <= 1; f00++)
		for (f04 = 0; f04 <= 1; f04++)
		for (f10 = 0; f10 <= 1; f10++) {
			long t = 920 + f00 * 4 + f04 * 2 + f10;

			seed_params(17, 2, &shapes[0], 0);
			seed_shapers(17);
			seed_info(17);
			seed_bits(17);
			ourParams.word_61c = theirParams.word_61c = 1u;
			ourInfo.word_00 = theirInfo.word_00 = (unsigned)f00;
			ourInfo.word_04 = theirInfo.word_04 = (unsigned)f04;
			ourInfo.word_10 = theirInfo.word_10 = (unsigned)f10;

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();
			dsplibs_debug_level = ref_dsplibs_debug_level = 2u;

			(void)V90CPPacker(&ourParams, &ourInfo, ourBits.v, 0);
			(void)blobPacker(&theirParams, &theirInfo,
					 theirBits.v, 0);

			dsplibs_debug_level = ref_dsplibs_debug_level = 0u;
			dsplib_debug_capture_on = 0;

			diff_eq_int("the message name, text %ld",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, t);
			diff_eq_int("the message name, lines %ld",
				    (long)dsplib_debug_capture_lines(0),
				    (long)dsplib_debug_capture_lines(1), t);
			diff_eq_obj("the bit vector, name trials",
				    struct bitsBuf, &ourBits, &theirBits, t);
			sawName = 1;
		}

		diff_eq_int("all eight name combinations were driven",
			    sawName, 1, 0);
	}

	/*
	 * THE TWO PRINT SITES ARE ASYMMETRIC AND BOTH ARE CHECKED.  The eight
	 * `edprintf` lines -- the name and the seven rows of seventeen -- are
	 * UNGATED, because the level test is inside `edprintf`; CLEARDOWN is a
	 * `dsplibs_debug_printf` behind `> 1`.  So at level 0 and 1 nothing
	 * reaches the log at all, and above it the cleardown arm prints
	 * exactly ONE line more than the normal arm over the same inputs.
	 *
	 * THE ABSOLUTE COUNT IS NOT EIGHT AND ASSERTING THAT IT WAS COST A
	 * RUN.  `float2Bits`'s two range warnings are also gated `> 1` and
	 * fire from inside this function on shaper values outside [0, 8] and
	 * [-1, 1], of which `fbEdge` has plenty -- so the total is eight plus
	 * however many of the five expansions warned.  The DIFFERENCE between
	 * the two arms is the invariant that isolates the cleardown site, and
	 * the floor of eight is what says the ungated ones ran.
	 */
	{
		static const unsigned int levels[4] = { 0u, 1u, 2u, 3u };
		int li;
		int sawLoud = 0, sawSilent = 0;

		for (li = 0; li < 4; li++) {
			long t = 940 + li;
			unsigned lo[2], lt[2];
			int cd2;

			for (cd2 = 0; cd2 <= 1; cd2++) {
				seed_params(19, 2, &shapes[0], 0);
				seed_shapers(19);
				seed_info(19);
				seed_bits(19);
				ourParams.word_61c = 1u;
				theirParams.word_61c = 1u;

				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();
				dsplibs_debug_level = levels[li];
				ref_dsplibs_debug_level = levels[li];

				(void)V90CPPacker(&ourParams, &ourInfo,
						  ourBits.v, cd2);
				(void)blobPacker(&theirParams, &theirInfo,
						 theirBits.v, cd2);

				dsplibs_debug_level = 0u;
				ref_dsplibs_debug_level = 0u;
				dsplib_debug_capture_on = 0;

				lo[cd2] = dsplib_debug_capture_lines(0);
				lt[cd2] = dsplib_debug_capture_lines(1);

				diff_eq_int("transcript line count %ld",
					    (long)lo[cd2], (long)lt[cd2], t);
				diff_eq_int("transcript text %ld",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1)
						  ) == 0, 1, t);
			}

			if (levels[li] > 1u) {
				diff_eq_int("the ungated eight ran %ld",
					    (long)lt[0] >= 8, 1, t);
				diff_eq_int("cleardown adds exactly one %ld",
					    (long)lt[1], (long)lt[0] + 1, t);
				sawLoud = 1;
			} else {
				diff_eq_int("nothing reached the log %ld",
					    (long)(lt[0] + lt[1]), 0, t);
				sawSilent = 1;
			}
		}

		diff_eq_int("the printing levels were driven", sawLoud, 1, 0);
		diff_eq_int("the quiet levels were driven", sawSilent, 1, 0);
	}

	/*
	 * ANTI-VACUITY OVER THE WHOLE MESSAGE: no index below the returned
	 * length still holds its seed.  The fields are chosen so that none of
	 * them can coincide with a 0x5a?? seed entry.
	 */
	{
		int i, untouched = 0, ret;

		seed_params(23, 2, &shapes[0], 0);
		seed_shapers(23);
		seed_info(23);
		seed_bits(23);
		ourParams.word_61c = theirParams.word_61c = 1u;
		theirInfo.word_04 = 0x11110001u;
		theirInfo.word_10 = 0x22220002u;
		theirInfo.word_00 = 0x33330003u;
		theirInfo.word_0c = 0x44440004u;
		theirInfo.short_14 = 0x0123;

		ret = blobPacker(&theirParams, &theirInfo, theirBits.v, 0);
		for (i = 0; i < ret; i++)
			if (theirBits.v[i] == seedBits.v[i])
				untouched++;
		diff_eq_int("every entry below the length was written",
			    untouched, 0, 0);
		diff_eq_int("...over a message of this many entries",
			    ret > 900, 1, 0);
	}

	diff_eq_int("one group was reached", (groupsSeen >> 1) & 1, 1, 0);
	diff_eq_int("six groups were reached", (groupsSeen >> 6) & 1, 1, 0);
	diff_eq_int("a count between the two was reached",
		    (groupsSeen & 0x3c) != 0, 1, 0);
	/*
	 * AND THE EXACT SET, not just "varied".  `pos`, the CRC extent and
	 * the return all scale with the group count, so which counts the
	 * sweep reached is the denominator of everything above it; a run that
	 * only ever produced six would test one bound and read as green.
	 * Bit g of the mask is "g groups was reached", and 0x7e is all six of
	 * 1..6.
	 *
	 * IF THIS FAILS, RECOMPUTE IT -- DO NOT RELAX IT.  The value is keyed
	 * on `shapes[]` and on `seed_params`'s LFSR, so a later batch adding
	 * a constellation shape for its own reasons moves it legitimately and
	 * the failure is not a defect in the packer.  Widening it to "some
	 * counts were reached" would give back exactly the denominator this
	 * line exists to state.
	 */
	diff_eq_int("the exact set of group counts reached", groupsSeen,
		    0x7e, 0);
	diff_eq_int("the codec block ran", sawCodec, 1, 0);
	diff_eq_int("the codec block was skipped", sawNoCodec, 1, 0);
	diff_eq_int("word_61c = 2 was driven", sawGateTwo, 1, 0);
	diff_eq_int("cleardown was driven", sawCleardown, 1, 0);
	diff_eq_int("a normal call was driven", sawNormal, 1, 0);
	diff_eq_int("the long rate arm was driven", sawLongRate, 1, 0);
	diff_eq_int("the short rate arm was driven", sawShortRate, 1, 0);
	diff_eq_int("a table byte at or above 0x80 was driven", sawHighEntry,
		    1, 0);
	diff_eq_int("a fixture with no high byte was driven too", sawLowOnly,
		    1, 0);

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
	rc |= run_float2bits();
	rc |= run_databitrate();
	rc |= run_packer();

	return rc;
}

/*
 * t_v92p4gen.cpp -- differential test of the fifteen V92Phase4Modulator
 * members that drive the phase 4 upstream state machine: the six `recived`
 * handlers, the two `exit` boundaries, the three `resetBefor`/`resetRRN`
 * members, and the four generators that need no sub-object of their own.
 *
 * SEPARATE FROM t_v92p4mod.cpp for the reason t_v92btosproc is separate from
 * t_v92tx: that file drives a LIFETIME -- build, destroy, count the
 * allocations -- and this one drives a RUNNING object that is never built and
 * never destroyed.  Sharing a fixture would mean every state trial paying for
 * a constructor whose two heap blocks it then has to poison out of the
 * comparison.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS BUILT BY HAND AND WHY IT IS SAFE
 *
 * None of the fifteen follows `mapper`, `params`, `mappingParams` or
 * `pattern`, and none touches the scrambler subobject.  Those regions are
 * therefore seeded with the SAME bytes on both sides and compared as data --
 * not poisoned, so a member that started following one of them would fail
 * here rather than be excused.  `bitsToSymbol` and `cp` ARE followed, so each
 * side gets its own, and those two words are the only ones the object
 * comparison masks.
 *
 * THE V92CP IS ONE PER SIDE, NOT SHARED, and that is the whole point of it.
 * Four of the fifteen write into the caller's V92CP (`recivedCP`,
 * `recivedPartOneSilenceRrnSUV`, `resetBeforRRN`, `resetRRNSecondSection`).
 * With one shared block our side failing to write would be covered up by the
 * reference writing a moment later -- the final state is identical either way,
 * which is finding 224's shape.  Two blocks seeded identically and compared
 * against each other is what witnesses OUR store.
 *
 * The same argument applies to `bitsToSymbol`, which
 * `generateDataSymbolBefore{FPE,RRN}` mutate through
 * `V92BitsToSymbol::process(unsigned int &, short *)`, and to the
 * `V92Transmitter` and `V92ModulusEncoder` blocks that hang off it: FPE's last
 * act is `bitsToSymbol->transmitter->modulusEncoder->field_50 = 1`, three
 * dereferences deep, and only a chain per side can see whether OUR side made
 * that store.
 *
 * `V92BitsToSymbol::process(unsigned int &, short *)` never dereferences
 * `transmitter` -- t_v92btosproc.cpp part 1 is built on that and states the
 * evidence -- so the chain here can be three raw blocks rather than a
 * constructed transmitter.  It is still a REAL chain: the pointers are wired
 * up and the modulus encoder block is compared.
 *
 * ---------------------------------------------------------------------------
 * THE GRID, AND THE THREE THINGS IT HAS TO SEPARATE
 *
 * `state` runs over every value the fifteen mention plus a NEGATIVE one and
 * two the switches do not name; `symbolCount` over 0, the boundary at 24 that
 * `exitCPt` subtracts, either side of it, several whole multiples, and
 * 0xffffffff so the subtraction wraps; `word_1b0` over moduli that do and do
 * not divide those counts.  Crossed with them, four one-bit inputs --
 * `flag_20`, `e2uExtended`, `word_1c0` and the V92CP's own `word_110` -- which
 * are the guards three of the handlers test.
 *
 *   1. `flag_20` is tested at the top of `recivedEd` and `recivedFirstRrnEd`
 *      and set at the bottom, so a trial that only ever starts with it clear
 *      cannot tell "return early" from "do it twice".
 *   2. `symbolCount % word_1b0` decides between announcing E2u and stepping to
 *      an intermediate state, and `exitCPt` reduces `symbolCount - 24` rather
 *      than `symbolCount`, so a grid whose counts are all multiples of the
 *      modulus, or all above 24, proves nothing about either.
 *   3. `e2uExtended` chooses 13 over 12 for `word_1b8` in two handlers and
 *      prints a second message in a third.  One bit, both values, every state.
 *
 * WHAT THE DEBUG PASS ADDS.  Nine of the fifteen call `edprintf`, whose output
 * is encoded but whose key is reset per call (encode.h), so two sides that
 * printed the same message with the same argument produce the same text.  The
 * whole grid is therefore run at `dsplibs_debug_level` 0, 1 and 2, with the
 * transcripts captured and compared at the last two as well as the objects.
 * That is what pins the ARGUMENT to each message -- the object state alone
 * cannot tell "@ symbolCount" from "@ 0", because the count is cleared on the
 * same path.
 *
 * LEVEL 1 IS IN THE LIST FOR ONE MUTATION AND WOULD OTHERWISE NOT BE.  Every
 * gate in the object is `> 1` (debug.h), so 0 and 2 fall the same side of `> 1`
 * and of `> 0` alike: a reconstruction that wrote `> 0` for `recivedSUVtag`'s
 * trace passes both.  One is the only level that separates them, and
 * test/mutations/v92p4gen.json's "gated at level 1 rather than above it" went
 * NOT CAUGHT until this pass existed.
 *
 * ANTI-VACUITY.  The run asserts that it saw at least one trial take each of
 * the paths that matter: a state change, a `flag_20` that blocked one, a V92CP
 * write, a non-empty transcript, and both arms of `generateRu`'s six-symbol
 * pattern.  A grid that reached none of them would otherwise pass silently.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V92Phase4Modulator.h"
#include "dsplib/V92BitsToSymbol.h"
#include "dsplib/V92CP.h"
#include "dsplib/V92ModulusEncoder.h"
#include "dsplib/V92Transmitter.h"
#include "dsplib/debug.h"

extern "C" {
int our_generateRu(void *) asm("_ZN18V92Phase4Modulator10generateRuEv");
int our_generateRuNot(void *) asm("_ZN18V92Phase4Modulator13generateRuNotEv");
int our_genFPE(void *)
	asm("_ZN18V92Phase4Modulator27generateDataSymbolBeforeFPEEv");
int our_genRRN(void *)
	asm("_ZN18V92Phase4Modulator27generateDataSymbolBeforeRRNEv");
void our_enterRepeatedCP(void *)
	asm("_ZN18V92Phase4Modulator15enterRepeatedCPEv");
void our_recivedCP(void *) asm("_ZN18V92Phase4Modulator9recivedCPEv");
void our_recivedCPtag(void *) asm("_ZN18V92Phase4Modulator12recivedCPtagEv");
void our_recivedRt(void *) asm("_ZN18V92Phase4Modulator9recivedRtEv");
void our_recivedSUV(void *) asm("_ZN18V92Phase4Modulator10recivedSUVEv");
void our_recivedP1tag(void *)
	asm("_ZN18V92Phase4Modulator30recivedPartOneSilenceRrnSUVtagEv");
void our_recivedP2(void *)
	asm("_ZN18V92Phase4Modulator27recivedPartTwoSilenceRrnSUVEv");
void our_recivedEd(void *) asm("_ZN18V92Phase4Modulator9recivedEdEv");
void our_recivedFirstRrnEd(void *)
	asm("_ZN18V92Phase4Modulator17recivedFirstRrnEdEv");
void our_recivedSUVtag(void *) asm("_ZN18V92Phase4Modulator13recivedSUVtagEv");
void our_recivedP1(void *)
	asm("_ZN18V92Phase4Modulator27recivedPartOneSilenceRrnSUVEv");
void our_recivedP2tag(void *)
	asm("_ZN18V92Phase4Modulator30recivedPartTwoSilenceRrnSUVtagEv");
void our_exitCPt(void *) asm("_ZN18V92Phase4Modulator7exitCPtEv");
void our_exitTRN2u(void *) asm("_ZN18V92Phase4Modulator9exitTRN2uEv");
void our_resetBeforFPE(void *) asm("_ZN18V92Phase4Modulator13resetBeforFPEEv");
void our_resetBeforRRN(void *) asm("_ZN18V92Phase4Modulator13resetBeforRRNEv");
void our_resetRRN2(void *)
	asm("_ZN18V92Phase4Modulator21resetRRNSecondSectionEv");

int ref_generateRu(void *) asm("ref__ZN18V92Phase4Modulator10generateRuEv");
int ref_generateRuNot(void *)
	asm("ref__ZN18V92Phase4Modulator13generateRuNotEv");
int ref_genFPE(void *)
	asm("ref__ZN18V92Phase4Modulator27generateDataSymbolBeforeFPEEv");
int ref_genRRN(void *)
	asm("ref__ZN18V92Phase4Modulator27generateDataSymbolBeforeRRNEv");
void ref_enterRepeatedCP(void *)
	asm("ref__ZN18V92Phase4Modulator15enterRepeatedCPEv");
void ref_recivedCP(void *) asm("ref__ZN18V92Phase4Modulator9recivedCPEv");
void ref_recivedCPtag(void *)
	asm("ref__ZN18V92Phase4Modulator12recivedCPtagEv");
void ref_recivedRt(void *) asm("ref__ZN18V92Phase4Modulator9recivedRtEv");
void ref_recivedSUV(void *) asm("ref__ZN18V92Phase4Modulator10recivedSUVEv");
void ref_recivedP1tag(void *)
	asm("ref__ZN18V92Phase4Modulator30recivedPartOneSilenceRrnSUVtagEv");
void ref_recivedP2(void *)
	asm("ref__ZN18V92Phase4Modulator27recivedPartTwoSilenceRrnSUVEv");
void ref_recivedEd(void *) asm("ref__ZN18V92Phase4Modulator9recivedEdEv");
void ref_recivedFirstRrnEd(void *)
	asm("ref__ZN18V92Phase4Modulator17recivedFirstRrnEdEv");
void ref_recivedSUVtag(void *)
	asm("ref__ZN18V92Phase4Modulator13recivedSUVtagEv");
void ref_recivedP1(void *)
	asm("ref__ZN18V92Phase4Modulator27recivedPartOneSilenceRrnSUVEv");
void ref_recivedP2tag(void *)
	asm("ref__ZN18V92Phase4Modulator30recivedPartTwoSilenceRrnSUVtagEv");
void ref_exitCPt(void *) asm("ref__ZN18V92Phase4Modulator7exitCPtEv");
void ref_exitTRN2u(void *) asm("ref__ZN18V92Phase4Modulator9exitTRN2uEv");
void ref_resetBeforFPE(void *)
	asm("ref__ZN18V92Phase4Modulator13resetBeforFPEEv");
void ref_resetBeforRRN(void *)
	asm("ref__ZN18V92Phase4Modulator13resetBeforRRNEv");
void ref_resetRRN2(void *)
	asm("ref__ZN18V92Phase4Modulator21resetRRNSecondSectionEv");

extern unsigned int ref_dsplibs_debug_level;
}

/* ------------------------------------------------------------------ */

#define GUARD	64u
#define OBJSZ	((unsigned)sizeof(V92Phase4Modulator))
#define SLOT	(OBJSZ + GUARD)
#define CPSZ	((unsigned)sizeof(V92CP))
#define BTSSZ	((unsigned)sizeof(V92BitsToSymbol))
#define TXSZ	((unsigned)sizeof(V92Transmitter))
#define MESZ	((unsigned)sizeof(V92ModulusEncoder))
#define NSYM	64

static unsigned char obj[2][SLOT] __attribute__((aligned(8)));
static unsigned char objseed[SLOT];
static unsigned char cmp_a[SLOT], cmp_b[SLOT];

static unsigned char cpbuf[2][CPSZ] __attribute__((aligned(8)));
static unsigned char btsbuf[2][BTSSZ] __attribute__((aligned(8)));
static unsigned char txbuf[2][TXSZ] __attribute__((aligned(8)));
static unsigned char mebuf[2][MESZ] __attribute__((aligned(8)));
static short symbuf[2][NSYM];

/* The four pointers nothing under test follows.  ONE value for both sides, so
 * that they compare as data; if a member ever starts following one of them it
 * will fault here rather than pass. */
static unsigned char shared_pattern[64];
static unsigned char shared_mapper[64];
static unsigned char shared_params[64];
static unsigned char shared_mp[64];

static unsigned int lfsr;

static unsigned char
nextbyte(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned int)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)(lfsr >> 3);
}

static void
fill(void *dst, unsigned n)
{
	unsigned char *q = (unsigned char *)dst;
	unsigned i;

	for (i = 0; i < n; i++)
		q[i] = nextbyte();
}

static V92Phase4Modulator *
M(int s)
{
	return (V92Phase4Modulator *)obj[s];
}

/* ------------------------------------------------------------------ */

/*
 * The grid.  `states` carries a negative value and two the switches do not
 * name as well as every one they do; `counts` straddles exitCPt's base of 24
 * and ends at the value whose `- 24` wraps; `mods` divide some of the counts
 * and not others.
 */
static const int states[] = {
	-1, 0, 1, 2, 3, 4, 5, 6, 9, 11, 12, 13, 14, 15, 19, 23, 26
};
#define NSTATE ((int)(sizeof(states) / sizeof(states[0])))

/*
 * 2400 and 2412 are `recivedRt`'s gate -- `cmp $0x95f; jbe` and then a
 * reciprocal divide by twelve -- and 2399 is the value just under it.  Without
 * the three, six mutations of that member's guard read NOT CAUGHT while every
 * differential check passed, which is finding 134's shape.
 */
static const unsigned int counts[] = {
	0u, 1u, 2u, 12u, 23u, 24u, 25u, 36u, 48u, 1200u, 2399u, 2400u,
	2406u, 2412u, 0xffffffffu
};
#define NCOUNT ((int)(sizeof(counts) / sizeof(counts[0])))

/*
 * NEVER ZERO.  Every use of this field is the divisor of an unsigned `divl`,
 * so a zero would trap on both sides at once and prove nothing.
 */
static const unsigned int mods[] = { 1u, 2u, 6u, 12u };
#define NMOD ((int)(sizeof(mods) / sizeof(mods[0])))

#define NBIT	16		/* flag_20, e2uExtended, word_1c0, cp->word_110 */
#define NTRIAL	(NSTATE * NCOUNT * NMOD * NBIT)

/*
 * Seed both sides from one draw, then wire each side's own three-block chain
 * and its own V92CP in.  Everything else is byte-identical between the sides.
 */
static void
setup(int trial)
{
	int s;
	int si = trial % NSTATE;
	int ci = (trial / NSTATE) % NCOUNT;
	int mi = (trial / (NSTATE * NCOUNT)) % NMOD;
	int b = (trial / (NSTATE * NCOUNT * NMOD)) % NBIT;

	lfsr = 0x2f6bu + 0x9e37u * (unsigned)trial + 1u;

	fill(objseed, SLOT);
	memcpy(obj[0], objseed, SLOT);
	memcpy(obj[1], objseed, SLOT);

	fill(cpbuf[0], CPSZ);
	memcpy(cpbuf[1], cpbuf[0], CPSZ);
	fill(btsbuf[0], BTSSZ);
	memcpy(btsbuf[1], btsbuf[0], BTSSZ);
	fill(txbuf[0], TXSZ);
	memcpy(txbuf[1], txbuf[0], TXSZ);
	fill(mebuf[0], MESZ);
	memcpy(mebuf[1], mebuf[0], MESZ);
	fill(symbuf[0], sizeof(symbuf[0]));
	memcpy(symbuf[1], symbuf[0], sizeof(symbuf[0]));

	fill(shared_pattern, sizeof(shared_pattern));
	fill(shared_mapper, sizeof(shared_mapper));
	fill(shared_params, sizeof(shared_params));
	fill(shared_mp, sizeof(shared_mp));

	for (s = 0; s < 2; s++) {
		V92Phase4Modulator *o = M(s);
		V92BitsToSymbol *bts = (V92BitsToSymbol *)btsbuf[s];
		V92Transmitter *tx = (V92Transmitter *)txbuf[s];

		o->state = states[si];
		o->symbolCount = counts[ci];
		o->word_1b0 = mods[mi];
		o->flag_20 = (unsigned)((b >> 0) & 1);
		o->e2uExtended = (unsigned)((b >> 1) & 1);
		o->word_1c0 = (unsigned)((b >> 2) & 1);
		o->patternLength = 1u + (mods[mi] & 7u);

		o->pattern = shared_pattern;
		o->mapper = (V92Mapper *)shared_mapper;
		o->params = (V92Parameters *)shared_params;
		o->mappingParams = (V92MappingParams *)shared_mp;
		o->cp = (V92CP *)cpbuf[s];
		o->bitsToSymbol = bts;

		((V92CP *)cpbuf[s])->word_110 = (unsigned)((b >> 3) & 1);

		/*
		 * THE SIX MEMBERS ADDED WITH `V92CP::infoToBits` PACK A REAL
		 * MESSAGE, so the CP's own fields have to be inside what that
		 * function can survive.  All three bounds are the ones
		 * t_v92info.cpp states and docs/deviations.md D570 and D571
		 * record; none of them is tidiness:
		 *
		 *   - `bitsPerSymbol` non-zero.  It is the divisor of an
		 *     unsigned `div` TWICE per call -- once inside
		 *     `infoToBits` and once at `patternLength /
		 *     cp->bitsPerSymbol` here -- so zero traps on both sides
		 *     at once and proves nothing.
		 *   - `word_10c` at most six, which is how many groups the two
		 *     mask blocks hold.  Above that `infoToBits` walks off
		 *     them, which is out of bounds in OUR source and would
		 *     leave the answer to the compiler (D561's argument).
		 *   - the five floats FINITE.  `infoToBits` takes the first
		 *     sign entry as `0 > f` and the other three as `f < 0`,
		 *     which are the same predicate for every number and
		 *     opposite ones for a NaN under `-mno-ieee-fp`.  Random
		 *     bytes would supply NaNs at about one draw in 250.
		 *
		 * `bitsPerSymbol` also feeds THIS class: `recivedRt` copies
		 * +0x43 into the CP's +0x128, so both are set here.
		 */
		{
			V92CP *c = (V92CP *)cpbuf[s];
			int f;

			c->bitsPerSymbol =
			    (unsigned char)(1 + (trial % 6));
			c->word_10c = (unsigned short)(trial % 7);
			c->char_01 = (signed char)((trial % 5) - 1);
			c->byte_00 = (unsigned char)(trial % 3);

			for (f = 0; f < 5; f++) {
				float v = (float)((trial % 17) - 8) / 4.0f;

				switch (f) {
				case 0:	c->flt_10 = v;	break;
				case 1:	c->flt_14 = -v;	break;
				case 2:	c->flt_18 = v;	break;
				case 3:	c->flt_1c = -v;	break;
				default: c->flt_20 = v;	break;
				}
			}

			/*
			 * DELIBERATELY NOT THE SAME as the CP's.  `recivedRt`
			 * copies +0x43 into the CP's +0x128, and seeding the
			 * two equal made that copy unobservable -- the
			 * mutation that deletes it read NOT CAUGHT while every
			 * differential check passed.
			 */
			o->bitsPerSymbol =
			    (unsigned char)(1 + ((trial + 3) % 6));
			o->word_38 = (unsigned)((trial >> 1) & 1);

			/*
			 * `word_1c4` is `recivedSUV`'s latch and it has to
			 * take BOTH values or that member never runs: the
			 * object seed alone leaves it non-zero 255 draws in
			 * 256.  Taken off a trial bit rather than added to the
			 * grid, which would multiply every member's trial
			 * count for one member's benefit.
			 */
			o->word_1c4 = (unsigned)((trial >> 2) & 1);
		}

		/*
		 * The staging chain.  THE BLOCK SIZE IS ONE AND `symbolsDone`
		 * IS NEVER ZERO, and both of those are forced by what the two
		 * generators hand `process` rather than chosen for tidiness:
		 *
		 *   - `process(unsigned int &, short *)` copies
		 *     `symbolsBlockSize` shorts into `out`, and `out` here is
		 *     the address of ONE `short` on the generator's frame
		 *     (`lea 0x16(%esp)` at .text+0x178a8).  Any block size
		 *     above one overruns that frame -- which is what a first
		 *     revision of this fixture did, and it took the process
		 *     down.  `setMappingParams` forces the field to 1 for
		 *     exactly this reason, and one is therefore the value the
		 *     object itself runs with.
		 *   - a block size of ZERO makes `process` return early
		 *     WITHOUT assigning `nbits`, and `nbits` is passed
		 *     uninitialised (the object's own `lea 0x10(%esp)` with
		 *     nothing stored there).  The generator would then branch
		 *     on stack residue, which differs between our frame and
		 *     the blob's for reasons that are not the modem's.
		 *   - `symbolsDone` of zero takes the underflow arm, which
		 *     copies nothing, so the returned symbol would be stack
		 *     residue for the same reason.
		 *
		 * With the block size at one, `symbolsDone` of 1 leaves
		 * nothing behind and `nofBitsForNextTime` returns
		 * `bitsPerFrame`, while 2 and 3 leave a symbol behind and it
		 * returns zero -- so both arms of the generators' `if (nbits)`
		 * are reached, and `bitsPerFrame` of zero reaches the third
		 * combination.
		 */
		bts->transmitter = tx;
		bts->params = (V92Parameters *)shared_params;
		bts->symbols = symbuf[s];
		bts->nSymbols = NSYM;
		bts->symbolsDone = 1u + (unsigned)(trial % 3);
		bts->bitsPerFrame = (unsigned)(trial % 7);
		bts->symbolsBlockSize = 1u;
		bts->flag_1c = (unsigned char)(trial & 1);
		memset(bts->pad_1d, 0, sizeof(bts->pad_1d));

		tx->modulusEncoder = (V92ModulusEncoder *)mebuf[s];
	}
}

/*
 * The object comparison.  Only the two words that hold a per-side address are
 * masked; `mapper`, `params`, `mappingParams`, `pattern` and the whole
 * scrambler subobject are compared as they stand.
 */
static void
compare_obj(const char *what, long trial)
{
	memcpy(cmp_a, obj[0], OBJSZ);
	memcpy(cmp_b, obj[1], OBJSZ);
	memset(cmp_a + 0x6c, 0x77, sizeof(void *));
	memset(cmp_b + 0x6c, 0x77, sizeof(void *));
	memset(cmp_a + 0x74, 0x77, sizeof(void *));
	memset(cmp_b + 0x74, 0x77, sizeof(void *));

	/*
	 * `pattern` is compared AS DATA for the eleven members that never
	 * write it -- both sides hold `shared_pattern` and a side that started
	 * following its own CP would fail here.  The six that call
	 * `getBitVector` replace it with a pointer INTO their own V92CP, which
	 * is a per-side address by construction; those are normalised to the
	 * offset within that block, so the check becomes "the same place in
	 * each side's own CP" rather than being masked away.
	 */
	{
		int t;

		for (t = 0; t < 2; t++) {
			unsigned char *pa = (unsigned char *)M(t)->pattern;
			unsigned char *base = cpbuf[t];
			unsigned char *dst = (t == 0 ? cmp_a : cmp_b) + 0x1a8;
			unsigned long off;

			if (pa < base || pa >= base + CPSZ)
				continue;
			off = (unsigned long)(pa - base);
			memcpy(dst, &off, sizeof(void *));
		}
	}

	diff_eq_obj_(__FILE__, __LINE__, what, "V92Phase4Modulator", cmp_a,
		     cmp_b, (size_t)OBJSZ, trial);
}

/* The guard past the end of the object, on both sides, against the seed. */
static int
guard_intact(void)
{
	return memcmp(obj[0] + OBJSZ, objseed + OBJSZ, GUARD) == 0
	    && memcmp(obj[1] + OBJSZ, objseed + OBJSZ, GUARD) == 0;
}

/*
 * The chain, compared block by block with each block's own per-side pointers
 * masked.  V92BitsToSymbol's `transmitter` and `symbols` and V92Transmitter's
 * `modulusEncoder` are addresses by construction; nothing else in the three
 * blocks is.
 */
static void
compare_chain(const char *what, long trial)
{
	static unsigned char a[TXSZ], b[TXSZ];
	const V92BitsToSymbol *x = (const V92BitsToSymbol *)btsbuf[0];
	const V92BitsToSymbol *y = (const V92BitsToSymbol *)btsbuf[1];

	diff_eq_int("bts symbolsDone agrees, %s", (long)x->symbolsDone,
		    (long)y->symbolsDone, trial);
	diff_eq_int("bts symbolsBlockSize agrees, %s",
		    (long)x->symbolsBlockSize, (long)y->symbolsBlockSize,
		    trial);
	diff_eq_int("bts bitsPerFrame agrees, %s", (long)x->bitsPerFrame,
		    (long)y->bitsPerFrame, trial);
	diff_eq_int("bts nSymbols agrees, %s", (long)x->nSymbols,
		    (long)y->nSymbols, trial);
	diff_eq_int("bts flag_1c agrees, %s", (long)x->flag_1c,
		    (long)y->flag_1c, trial);
	diff_eq_int("bts staging buffer agrees, %s",
		    memcmp(symbuf[0], symbuf[1], sizeof(symbuf[0])) == 0, 1,
		    trial);

	memcpy(a, txbuf[0], TXSZ);
	memcpy(b, txbuf[1], TXSZ);
	memset(a + 0x48, 0x77, sizeof(void *));
	memset(b + 0x48, 0x77, sizeof(void *));
	diff_eq_int("the transmitter block agrees, %s",
		    memcmp(a, b, TXSZ) == 0, 1, trial);

	diff_eq_int("the modulus encoder block agrees, %s",
		    memcmp(mebuf[0], mebuf[1], MESZ) == 0, 1, trial);
}

static void
compare_cp(const char *what, long trial)
{
	diff_eq_int("the V92CP agrees, %s",
		    memcmp(cpbuf[0], cpbuf[1], CPSZ) == 0, 1, trial);
}

static void
compare_text(const char *what, long trial)
{
	diff_eq_int("the transcript line count agrees, %s",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), trial);
	diff_eq_int("the transcript agrees, %s",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, trial);
}

/* ------------------------------------------------------------------ */

typedef void (*void_fn)(void *);
typedef int (*int_fn)(void *);

struct member {
	const char *name;
	void_fn our_v;
	void_fn ref_v;
	int_fn our_i;
	int_fn ref_i;
};

static const struct member members[] = {
	{ "enterRepeatedCP",	our_enterRepeatedCP,	ref_enterRepeatedCP,
								 0, 0 },
	{ "recivedCP",		our_recivedCP,		ref_recivedCP,	 0, 0 },
	{ "recivedCPtag",	our_recivedCPtag,	ref_recivedCPtag, 0, 0 },
	{ "recivedRt",		our_recivedRt,		ref_recivedRt,	 0, 0 },
	{ "recivedSUV",		our_recivedSUV,		ref_recivedSUV,	 0, 0 },
	{ "recivedPartOneSilenceRrnSUVtag",
				our_recivedP1tag,	ref_recivedP1tag, 0, 0 },
	{ "recivedPartTwoSilenceRrnSUV",
				our_recivedP2,		ref_recivedP2,	 0, 0 },
	{ "recivedEd",		our_recivedEd,		ref_recivedEd,	 0, 0 },
	{ "recivedFirstRrnEd",	our_recivedFirstRrnEd,	ref_recivedFirstRrnEd,
								 0, 0 },
	{ "recivedSUVtag",	our_recivedSUVtag,	ref_recivedSUVtag,
								 0, 0 },
	{ "recivedPartOneSilenceRrnSUV",
				our_recivedP1,		ref_recivedP1,	 0, 0 },
	{ "recivedPartTwoSilenceRrnSUVtag",
				our_recivedP2tag,	ref_recivedP2tag, 0, 0 },
	{ "exitCPt",		our_exitCPt,		ref_exitCPt,	 0, 0 },
	{ "exitTRN2u",		our_exitTRN2u,		ref_exitTRN2u,	 0, 0 },
	{ "resetBeforFPE",	our_resetBeforFPE,	ref_resetBeforFPE,
								 0, 0 },
	{ "resetBeforRRN",	our_resetBeforRRN,	ref_resetBeforRRN,
								 0, 0 },
	{ "resetRRNSecondSection",
				our_resetRRN2,		ref_resetRRN2,	 0, 0 },
	{ "generateRu",		0, 0, our_generateRu,	ref_generateRu },
	{ "generateRuNot",	0, 0, our_generateRuNot, ref_generateRuNot },
	{ "generateDataSymbolBeforeFPE",
				0, 0, our_genFPE,	ref_genFPE },
	{ "generateDataSymbolBeforeRRN",
				0, 0, our_genRRN,	ref_genRRN }
};
#define NMEMBER ((int)(sizeof(members) / sizeof(members[0])))

/* What the run must have SEEN, so that a grid reaching none of it fails. */
static int saw_state_change;
static int saw_blocked_by_flag20;
static int saw_cp_write;
static int saw_transcript;
static int saw_message_rebuilt;
static int saw_null_cp;
static int saw_ru_pos;
static int saw_ru_neg;
static int saw_nonzero_return;

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

static int
run_member(int m, unsigned int lvl)
{
	char title[160];
	long trial;
	int guards = 0;

	strcpy(title, "V92Phase4Modulator::");
	strcat(title, members[m].name);
	strcat(title, lvl == 0 ? ", level 0"
		     : (lvl == 1 ? ", level 1, transcripts"
				 : ", level 2, transcripts"));
	diff_begin(title);

	set_level(lvl);
	dsplib_debug_capture_on = (lvl != 0);

	for (trial = 0; trial < NTRIAL; trial++) {
		int before_state, after_state;
		unsigned char before_cp[CPSZ];
		int flag20;

		setup((int)trial);
		before_state = M(0)->state;
		flag20 = (int)M(0)->flag_20;
		memcpy(before_cp, cpbuf[0], CPSZ);

		if (lvl != 0)
			dsplib_debug_capture_reset();

		/*
		 * `recivedRt` HAS A NULL-CP ARM and it is a real one: the
		 * object sets state 29 and prints an ERROR there, so it
		 * expects to reach it.  Every other member dereferences `cp`
		 * without checking, so the null is given to this member only
		 * and on one trial in eight.
		 */
		if (strcmp(members[m].name, "recivedRt") == 0
		    && (trial % 5) == 0) {
			int t;

			/*
			 * The two guards above the arm are forced open as
			 * well.  Leaving them to the grid made the arm
			 * reachable only where three independent trial bits
			 * happened to line up, and on this grid they never
			 * did: the mutation that deletes `state = 29` read
			 * NOT CAUGHT.  A trial that cannot reach the code it
			 * names is finding 134's shape.
			 */
			for (t = 0; t < 2; t++) {
				M(t)->cp = 0;
				M(t)->word_38 = 1;
				M(t)->state = 23;
				M(t)->symbolCount = 2400;
			}
			saw_null_cp = 1;
		}

		if (members[m].our_v != 0) {
			members[m].our_v(obj[0]);
			members[m].ref_v(obj[1]);
		} else {
			int a = members[m].our_i(obj[0]);
			int b = members[m].ref_i(obj[1]);

			diff_eq_int("the symbol agrees (trial %ld)", (long)a,
				    (long)b, trial);
			if (a != 0)
				saw_nonzero_return = 1;
			/*
			 * BY NAME, NOT BY INDEX.  This read `m == 11` until
			 * six members were added above it, at which point it
			 * silently started watching `generateDataSymbolBefore-
			 * RRN` and the two anti-vacuity flags went to zero --
			 * which the gate caught, because they are asserted.
			 */
			if (strcmp(members[m].name, "generateRu") == 0) {
				if (a == M(0)->amplitude)
					saw_ru_pos = 1;
				if (a == -(int)(short)M(0)->amplitude)
					saw_ru_neg = 1;
			}
		}

		/*
		 * DID ANY TRIAL ACTUALLY PACK A MESSAGE?  The six members
		 * added with `V92CP::infoToBits` reach their tail only through
		 * a guard and a modulus test, so a grid that never satisfied
		 * both would pass every check above while exercising nothing.
		 * `pattern` pointing INTO the side's own V92CP is the
		 * observable outcome of that tail and of nothing else -- the
		 * seed puts `shared_pattern` there, which is outside it.
		 */
		if ((unsigned char *)M(0)->pattern >= cpbuf[0]
		    && (unsigned char *)M(0)->pattern < cpbuf[0] + CPSZ)
			saw_message_rebuilt = 1;

		after_state = M(0)->state;
		if (after_state != before_state)
			saw_state_change = 1;
		if (flag20 != 0 && after_state == before_state)
			saw_blocked_by_flag20 = 1;
		if (memcmp(before_cp, cpbuf[0], CPSZ) != 0)
			saw_cp_write = 1;

		compare_obj(members[m].name, trial);
		compare_cp(members[m].name, trial);
		compare_chain(members[m].name, trial);
		if (lvl != 0) {
			compare_text(members[m].name, trial);
			if (dsplib_debug_capture_lines(0) != 0)
				saw_transcript = 1;
		}
		if (!guard_intact())
			guards++;
	}

	diff_eq_int("nothing wrote past the object", guards, 0, 0);

	dsplib_debug_capture_on = 0;
	set_level(0);

	return diff_end();
}

static int
run_antivacuity(void)
{
	diff_begin("the grid reached the paths it claims to");

	diff_eq_int("some trial changed the state", saw_state_change, 1, 0);
	diff_eq_int("some trial was blocked by flag_20",
		    saw_blocked_by_flag20, 1, 0);
	diff_eq_int("some trial wrote into the V92CP", saw_cp_write, 1, 0);
	diff_eq_int("some trial printed something", saw_transcript, 1, 0);
	diff_eq_int("some trial packed a CP message", saw_message_rebuilt, 1,
		    0);
	diff_eq_int("some trial gave recivedRt a null CP", saw_null_cp, 1, 0);
	diff_eq_int("generateRu returned +amplitude somewhere", saw_ru_pos, 1,
		    0);
	diff_eq_int("generateRu returned -amplitude somewhere", saw_ru_neg, 1,
		    0);
	diff_eq_int("some generator returned non-zero", saw_nonzero_return, 1,
		    0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;
	int m;

	for (m = 0; m < NMEMBER; m++) {
		rc |= run_member(m, 0);
		rc |= run_member(m, 1);
		rc |= run_member(m, 2);
	}
	rc |= run_antivacuity();

	return rc;
}

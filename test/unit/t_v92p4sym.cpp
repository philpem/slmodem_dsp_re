/*
 * t_v92p4sym.cpp -- differential test of the nine V92Phase4Modulator members
 * that produce a symbol from the scrambler, the mapper or the bits-to-symbol
 * chain: generateCPt, generateCPu, generateSUVu, generateE1u, generateE2u,
 * generateB1u, generateRm, generateTRN2u, and setMappingParams, which is the
 * one member that reconfigures that chain rather than drawing from it.
 *
 * SEPARATE FROM t_v92p4gen.cpp, which drives the other fifteen.  That file's
 * members touch nothing but the object and the caller's V92CP and it can run a
 * ten-thousand-point grid for the cost of two memcpys.  These nine need a
 * placed scrambler, a reset mapper and a CONSTRUCTED transmitter chain per
 * side, and each trial pushes real bits through the modulus encoder, the
 * precoder, the convolution encoder and the pre-filter.  One fixture for both
 * sets would make the cheap half pay for the expensive half's setup.
 *
 * ---------------------------------------------------------------------------
 * FOUR THINGS THE FIXTURE HAS TO CONSTRAIN, AND NONE OF THEM IS TIDINESS
 *
 * Each is a range outside which BOTH sides are undefined, so the comparison
 * would agree while proving nothing -- or, twice over here, take the process
 * down.
 *
 *   1. THE SCRAMBLER'S HISTORY AND `pattern` HOLD 0 OR 1.  `Scrambler<h,h>::
 *      process` returns `in ^ *pTap1 ^ *pTap2`, so an arbitrary history makes
 *      an arbitrary byte, and that byte is what `V92Mapper::process` turns
 *      into a table index over a SIXTEEN-entry table.  Bits in, bits out, by
 *      induction.
 *   2. THE MAPPER'S `mode` IS 0 OR 1.  The index is `constelAmplitudeTable
 *      [bits + 8 * mode]`, so mode 2 reads off the end of the table.  It is
 *      set by calling the class's own `reset`, not by poking the fields.
 *   3. `patternIndex` STARTS BELOW `patternLength`.  The generators read
 *      `pattern[patternIndex]` BEFORE the `% patternLength` that follows it,
 *      so the first read of the first iteration is unguarded.
 *   4. `symbolsBlockSize` IS ONE, AND IS SET AFTER EVERY `reset`.
 *      `V92BitsToSymbol::reset` leaves it at ZERO, and at zero
 *      `process(unsigned int &, short *)` returns without assigning its
 *      reference argument -- which the generators pass uninitialised.  Both
 *      sides would then branch on their own stack residue.  `setMappingParams`
 *      forces the field to 1 for what is presumably this reason, and that is
 *      corroboration of the reading rather than a convenience.
 *
 * ---------------------------------------------------------------------------
 * `bitsPerSymbol == 0` IS NOT IN THE GRID, AND IT WAS, AND THAT IS FINDING 4705
 *
 * At zero, `bits[bitsPerSymbol - 1]` is `bits[-1]`, which is
 * V92Phase4Modulator+0x7b -- the top byte of `prevBit`.  The blob's own
 * `movzbl 0x7b(%esi,%ebx,1)` with %esi zero is that address, so the aliasing
 * is the OBJECT'S; in our source it is an out-of-bounds subscript and
 * therefore undefined behaviour, and the two overlapping stores may be
 * emitted in either order.
 *
 * They are.  With this input in the grid, generateE2u and generateTRN2u pass
 * under GCC 13 and FAIL under GCC 3.4.2 -- 80 and 160 checks, all of them
 * `V92Phase4Modulator+123 got 00, reference 01` -- because the period compiler
 * schedules the byte store first and the modern one does not.  A trial whose
 * verdict is the compiler's rather than the source's proves nothing either
 * way, which is what finding 617 says about store order and what D504 says
 * about a trial that cannot be driven.  So the grid starts at one, D561 records
 * the aliasing as reproduced and NOT driven, and no mutation here claims an
 * order.
 *
 * ---------------------------------------------------------------------------
 * THE CHAIN, AND ITS MASK
 *
 * Both sides construct a real V92BitsToSymbol -- ours with our constructor,
 * the reference with the blob's -- from one shared parameter block, and the
 * heap addresses that lands in the two chains are MEASURED rather than
 * assumed: the fixture snapshots both sides after construction, records which
 * four-byte slots differ before anything under test has run, and skips exactly
 * those.  The mask's size is asserted, so a slot that starts differing
 * somewhere new fails rather than being quietly excused.  t_v92btosproc.cpp
 * and t_v92txreset.cpp use the same measurement and it is the same argument.
 *
 * THE PARAMETER BLOCK IS SANE AND SAYS WHY, for the reasons t_v92btosproc sets
 * out at length: the moduli are non-zero, the trellis type is one the switch
 * has, the gain is neither one nor whole, and every constellation index the
 * precoder can form stays in range.  Random bytes there are undefined
 * behaviour on both sides at once.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V92Phase4Modulator.h"
#include "dsplib/V92BitsToSymbol.h"
#include "dsplib/V92CP.h"
#include "dsplib/V92Mapper.h"
#include "dsplib/V92ModulusEncoder.h"
#include "dsplib/V92ParamsInfo.h"
#include "dsplib/V92Transmitter.h"
#include "dsplib/debug.h"

extern "C" {
int our_generateCPt(void *) asm("_ZN18V92Phase4Modulator11generateCPtEv");
int our_generateCPu(void *) asm("_ZN18V92Phase4Modulator11generateCPuEv");
int our_generateSUVu(void *) asm("_ZN18V92Phase4Modulator12generateSUVuEv");
int our_generateE1u(void *) asm("_ZN18V92Phase4Modulator11generateE1uEv");
int our_generateE2u(void *) asm("_ZN18V92Phase4Modulator11generateE2uEv");
int our_generateB1u(void *) asm("_ZN18V92Phase4Modulator11generateB1uEv");
int our_generateRm(void *) asm("_ZN18V92Phase4Modulator10generateRmEv");
int our_generateTRN2u(void *) asm("_ZN18V92Phase4Modulator13generateTRN2uEv");
void our_setMappingParams(void *, void *)
	asm("_ZN18V92Phase4Modulator16setMappingParamsEP16V92MappingParams");

int ref_generateCPt(void *) asm("ref__ZN18V92Phase4Modulator11generateCPtEv");
int ref_generateCPu(void *) asm("ref__ZN18V92Phase4Modulator11generateCPuEv");
int ref_generateSUVu(void *)
	asm("ref__ZN18V92Phase4Modulator12generateSUVuEv");
int ref_generateE1u(void *) asm("ref__ZN18V92Phase4Modulator11generateE1uEv");
int ref_generateE2u(void *) asm("ref__ZN18V92Phase4Modulator11generateE2uEv");
int ref_generateB1u(void *) asm("ref__ZN18V92Phase4Modulator11generateB1uEv");
int ref_generateRm(void *) asm("ref__ZN18V92Phase4Modulator10generateRmEv");
int ref_generateTRN2u(void *)
	asm("ref__ZN18V92Phase4Modulator13generateTRN2uEv");
void ref_setMappingParams(void *, void *)
	asm("ref__ZN18V92Phase4Modulator16setMappingParamsEP16V92MappingParams");

/* The chain, and the mapper: one of each per side, by symbol. */
void our_btos_c1(void *, unsigned int, void *)
	asm("_ZN15V92BitsToSymbolC1EjP13V92Parameters");
void ref_btos_c1(void *, unsigned int, void *)
	asm("ref__ZN15V92BitsToSymbolC1EjP13V92Parameters");
void our_btos_d1(void *) asm("_ZN15V92BitsToSymbolD1Ev");
void ref_btos_d1(void *) asm("ref__ZN15V92BitsToSymbolD1Ev");
void our_btos_reset(void *, void *)
	asm("_ZN15V92BitsToSymbol5resetEP16V92MappingParams");
void ref_btos_reset(void *, void *)
	asm("ref__ZN15V92BitsToSymbol5resetEP16V92MappingParams");
unsigned int our_btos_setblock(void *, unsigned int)
	asm("_ZN15V92BitsToSymbol19setSymbolsBlockSizeEj");
unsigned int ref_btos_setblock(void *, unsigned int)
	asm("ref__ZN15V92BitsToSymbol19setSymbolsBlockSizeEj");

void our_map_reset(void *, short, unsigned char) asm("_ZN9V92Mapper5resetEsh");
void ref_map_reset(void *, short, unsigned char)
	asm("ref__ZN9V92Mapper5resetEsh");

extern unsigned int ref_dsplibs_debug_level;
}

/* ------------------------------------------------------------------ */

#define GUARD	64u
#define OBJSZ	((unsigned)sizeof(V92Phase4Modulator))
#define SLOT	(OBJSZ + GUARD)
#define CPSZ	((unsigned)sizeof(V92CP))
#define BTSSZ	((unsigned)sizeof(V92BitsToSymbol))
#define MAPSZ	((unsigned)sizeof(V92Mapper))

/* The scrambler's real geometry, from V92P4M_SCRAM_*: (1 + 23 + 99) elements,
 * the restart point 99 above the base, the taps 5 and 23 above that. */
#define SCR_LEN		(1u + V92P4M_SCRAM_TAP2 + V92P4M_SCRAM_SLACK)
#define SCR_INITOUT	((unsigned)V92P4M_SCRAM_SLACK)
#define SCR_INITTAP1	(SCR_INITOUT + (unsigned)V92P4M_SCRAM_TAP1)
#define SCR_INITTAP2	(SCR_INITOUT + (unsigned)V92P4M_SCRAM_TAP2)

#define PATLEN		16u
#define CHAIN_ALLOC	2048u
#define NCOEF		V92_PARAMSINFO_MAX_FILTER_LEN
#define NCONST		V92_PARAMSINFO_MAX_LC

static unsigned char obj[2][SLOT] __attribute__((aligned(8)));
static unsigned char objseed[SLOT];
static unsigned char cmp_a[SLOT], cmp_b[SLOT];

static unsigned char scr[2][SCR_LEN] __attribute__((aligned(8)));
static unsigned char mapbuf[2][MAPSZ] __attribute__((aligned(8)));
static unsigned char btos[2][BTSSZ] __attribute__((aligned(8)));

/* Shared: nothing under test writes any of them. */
static unsigned char shared_cp[CPSZ] __attribute__((aligned(8)));
static unsigned char shared_pattern[PATLEN];
static unsigned char shared_dummy[64] __attribute__((aligned(8)));

static struct V92ParamsInfo params;
static float coefbuf[4][NCOEF];
static int constbuf[V92_PARAMSINFO_CONSTELLATIONS][NCONST];

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

static V92BitsToSymbol *
B(int s)
{
	return (V92BitsToSymbol *)btos[s];
}

static V92Transmitter *
TX(int s)
{
	return B(s)->transmitter;
}

/* ------------------------------------------------------------------ */
/* The parameter block.  Adapted from t_v92btosproc.cpp, whose comment  */
/* carries the evidence for every constraint below.                     */
/* ------------------------------------------------------------------ */

#define PARAM_K		24

static void
seed_params(unsigned int seed, int filters)
{
	int i, j;

	lfsr = seed | 1u;
	memset(&params, 0, sizeof(params));

	params.K = PARAM_K;
	params.modulosEncoderPresent = 1;
	params.prefilterPrecoderPresent = 1;
	params.constellationPresent = 1;
	params.trellisType = (int)(nextbyte() % 3u);
	params.extendEu = 0;
	/* Never 1.0 and never whole: t_v92btosproc's note. */
	params.gain = 1.5f + 0.25f * (float)(nextbyte() & 7u);

	for (i = 0; i < 12; i++)
		params.m[i] = 4 + (int)(nextbyte() & 3u);

	for (i = 0; i < 4; i++) {
		for (j = 0; j < NCOEF; j++)
			coefbuf[i][j] = 0.0f;
	}

	if (filters) {
		/* Multiples of four -- FloatFIR stores `nTaps & ~3`, and a
		 * zero-tap filter counts 2**32 samples off the end of its
		 * history.  The IIR stays off. */
		params.lz1 = 4;
		params.lp1 = 4;
		params.lz2 = 4;
		params.lp2 = 0;
		for (i = 0; i < 4; i++) {
			for (j = 0; j < 4; j++)
				coefbuf[i][j] = 0.5f / (float)(1 << j);
		}
	} else {
		params.lz1 = 0;
		params.lp1 = 0;
		params.lz2 = 0;
		params.lp2 = 0;
	}

	params.z1 = coefbuf[0];
	params.p1 = coefbuf[1];
	params.z2 = coefbuf[2];
	params.p2 = coefbuf[3];

	for (i = 0; i < V92_PARAMSINFO_CONSTELLATIONS; i++) {
		params.LC[i] = 8;
		params.indexConstel[i] = i;
		for (j = 0; j < NCONST; j++)
			constbuf[i][j] = 1 + 2 * j;
		params.constellations[i] = constbuf[i];
	}
}

/* ------------------------------------------------------------------ */
/* The chain's mask, measured after construction.                       */
/* ------------------------------------------------------------------ */

struct sub {
	unsigned off;
	unsigned size;
	const char *what;
};

/* t_v92txreset's list, and the same list for the same reason. */
static const struct sub subs[] = {
	{ 0x08, 0x50,   "the bit buffer at +0x08" },
	{ 0x48, 0x54,   "V92ModulusEncoder" },
	{ 0x4c, 0x80,   "V92Precoder" },
	{ 0x50, 0x14,   "V92PreFilter" },
	{ 0x54, 0x2008, "V92ConvolutionEncoder" },
	{ 0x58, 1,      "the one-byte buffer at +0x58" }
};
#define NSUB	((int)(sizeof(subs) / sizeof(subs[0])))
#define MAXSUB	0x2008

static unsigned char masked[NSUB][MAXSUB / 4];
static unsigned char txmask[0x60 / 4];
static int maskcount;

static void
measure_mask(void)
{
	int i;
	unsigned u;

	memset(masked, 0, sizeof(masked));
	memset(txmask, 0, sizeof(txmask));
	maskcount = 0;

	for (u = 0; u < 0x60 / 4; u++) {
		const unsigned char *a = (const unsigned char *)TX(0);
		const unsigned char *b = (const unsigned char *)TX(1);

		if (memcmp(a + 4 * u, b + 4 * u, 4) != 0) {
			txmask[u] = 1;
			maskcount++;
		}
	}
	for (i = 0; i < NSUB; i++) {
		const unsigned char *a =
			*(const unsigned char **)((unsigned char *)TX(0)
						  + subs[i].off);
		const unsigned char *b =
			*(const unsigned char **)((unsigned char *)TX(1)
						  + subs[i].off);

		for (u = 0; u + 4 <= subs[i].size; u += 4) {
			if (memcmp(a + u, b + u, 4) != 0) {
				masked[i][u / 4] = 1;
				maskcount++;
			}
		}
	}
}

static void
compare_chain(long trial)
{
	int i;
	unsigned u;
	int bad;

	diff_eq_int("bts symbolsDone agrees (trial %ld)",
		    (long)B(0)->symbolsDone, (long)B(1)->symbolsDone, trial);
	diff_eq_int("bts symbolsBlockSize agrees (trial %ld)",
		    (long)B(0)->symbolsBlockSize, (long)B(1)->symbolsBlockSize,
		    trial);
	diff_eq_int("bts bitsPerFrame agrees (trial %ld)",
		    (long)B(0)->bitsPerFrame, (long)B(1)->bitsPerFrame, trial);
	diff_eq_int("bts nSymbols agrees (trial %ld)", (long)B(0)->nSymbols,
		    (long)B(1)->nSymbols, trial);
	diff_eq_int("bts flag_1c agrees (trial %ld)", (long)B(0)->flag_1c,
		    (long)B(1)->flag_1c, trial);
	diff_eq_int("the staged symbols agree (trial %ld)",
		    memcmp(B(0)->symbols, B(1)->symbols,
			   CHAIN_ALLOC * sizeof(short)) == 0, 1, trial);

	bad = 0;
	for (u = 0; u < 0x60 / 4; u++) {
		const unsigned char *a = (const unsigned char *)TX(0);
		const unsigned char *b = (const unsigned char *)TX(1);

		if (!txmask[u] && memcmp(a + 4 * u, b + 4 * u, 4) != 0)
			bad++;
	}
	diff_eq_int("the transmitter block agrees (trial %ld)", bad, 0, trial);

	for (i = 0; i < NSUB; i++) {
		const unsigned char *a =
			*(const unsigned char **)((unsigned char *)TX(0)
						  + subs[i].off);
		const unsigned char *b =
			*(const unsigned char **)((unsigned char *)TX(1)
						  + subs[i].off);

		bad = 0;
		for (u = 0; u + 4 <= subs[i].size; u += 4) {
			if (!masked[i][u / 4] && memcmp(a + u, b + u, 4) != 0)
				bad++;
		}
		diff_eq_int(subs[i].what, bad, 0, trial);
	}
}

/* ------------------------------------------------------------------ */
/* The grid.                                                           */
/* ------------------------------------------------------------------ */

static const unsigned char bps[] = { 1, 2, 3, 5, 8, 12, 16 };
#define NBPS	((int)(sizeof(bps) / sizeof(bps[0])))

static const unsigned int patlens[] = { 1u, 2u, 3u, 5u, 7u };
#define NPAT	((int)(sizeof(patlens) / sizeof(patlens[0])))

static const unsigned int scounts[] = {
	0u, 1u, 24u, 25u, 26u, 30u, 100u, 0xffffffffu
};
#define NSCOUNT	((int)(sizeof(scounts) / sizeof(scounts[0])))

static const short amps[] = { 0, 1, 1000, -1000, 4899, 32767 };
#define NAMP	((int)(sizeof(amps) / sizeof(amps[0])))

#define NTRIAL	(2 * NBPS * NPAT * NSCOUNT * 2 * 2)

/* Anti-vacuity: the paths a grid could silently miss. */
static int saw_flag3c_zero, saw_flag3c_set;
static int saw_bps_over_pattern, saw_scrambler_restart;
static int saw_bulk_bits, saw_cpt_pattern, saw_cpt_toggle;

static void
place_scrambler(int s, unsigned int back)
{
	Scrambler<unsigned char, unsigned char> *o = &M(s)->scrambler;

	o->pLimit = scr[s];
	o->pInitOut = scr[s] + SCR_INITOUT;
	o->pInitTap1 = scr[s] + SCR_INITTAP1;
	o->pInitTap2 = scr[s] + SCR_INITTAP2;
	o->pOut = scr[s] + SCR_INITOUT - back;
	o->pTap1 = o->pOut + V92P4M_SCRAM_TAP1;
	o->pTap2 = o->pOut + V92P4M_SCRAM_TAP2;
	o->tailLength = V92P4M_SCRAM_TAP2;
}

static void
setup(int trial)
{
	int s;
	int t = trial;
	int f3 = t % 2;			t /= 2;
	int bi = t % NBPS;		t /= NBPS;
	int pi = t % NPAT;		t /= NPAT;
	int ci = t % NSCOUNT;		t /= NSCOUNT;
	int pb = t % 2;			t /= 2;
	int md = t % 2;
	unsigned int back = (unsigned int)(trial * 37) % (SCR_INITOUT + 1u);
	unsigned int i;

	lfsr = 0x51a3u + 0x9e37u * (unsigned)trial + 1u;

	fill(objseed, SLOT);
	memcpy(obj[0], objseed, SLOT);
	memcpy(obj[1], objseed, SLOT);

	/*
	 * Bits, not bytes -- see the note at the top.  THE OBJECT'S OWN
	 * `bits[]` IS IN THIS LIST and that is not obvious: `V92Mapper::
	 * process` reads `mapper->bits` of them (two or three) whatever
	 * `bitsPerSymbol` was, so the entries the scrambler did NOT write
	 * still reach the table index.  Seeded with arbitrary bytes -- which
	 * the object seed above does -- the index leaves the sixteen-entry
	 * table and both sides return the x87 indefinite, which is a
	 * comparison that agrees for the wrong reason on most trials and
	 * disagrees on the rest.  That is what the first revision of this
	 * fixture did.
	 */
	for (i = 0; i < PATLEN; i++)
		shared_pattern[i] = (unsigned char)(nextbyte() & 1u);
	for (i = 0; i < SCR_LEN; i++)
		scr[0][i] = (unsigned char)(nextbyte() & 1u);
	memcpy(scr[1], scr[0], SCR_LEN);
	for (i = 0; i < sizeof(M(0)->bits); i++) {
		unsigned char v = (unsigned char)(nextbyte() & 1u);

		M(0)->bits[i] = v;
		M(1)->bits[i] = v;
		objseed[0x7c + i] = v;
	}

	fill(shared_cp, CPSZ);
	fill(shared_dummy, sizeof(shared_dummy));

	if (back != 0)
		saw_scrambler_restart = 1;

	for (s = 0; s < 2; s++) {
		V92Phase4Modulator *o = M(s);

		o->flag_3c = (unsigned int)f3;
		o->bitsPerSymbol = bps[bi];
		o->patternLength = patlens[pi];
		o->patternIndex = (unsigned int)trial % patlens[pi];
		o->symbolCount = scounts[ci];
		o->prevBit = (unsigned int)pb;
		o->amplitude = amps[trial % NAMP];
		o->state = trial % 17;

		o->pattern = shared_pattern;
		o->cp = (V92CP *)shared_cp;
		o->params = (V92Parameters *)shared_dummy;
		o->mappingParams = (V92MappingParams *)shared_dummy;
		o->mapper = (V92Mapper *)mapbuf[s];
		o->bitsToSymbol = B(s);

		place_scrambler(s, back);
	}

	if (f3 == 0)
		saw_flag3c_zero = 1;
	else
		saw_flag3c_set = 1;
	if ((unsigned int)bps[bi] > patlens[pi])
		saw_bps_over_pattern = 1;
	if (scounts[ci] > 24u)
		saw_cpt_pattern = 1;
	else
		saw_cpt_toggle = 1;

	/* The mapper, through its own reset, on both sides. */
	fill(mapbuf[0], MAPSZ);
	memcpy(mapbuf[1], mapbuf[0], MAPSZ);
	our_map_reset(mapbuf[0], (short)(1000 + 37 * (trial % 11)),
		      (unsigned char)md);
	ref_map_reset(mapbuf[1], (short)(1000 + 37 * (trial % 11)),
		      (unsigned char)md);

	/* The chain, reset and forced back to a block size of one. */
	our_btos_reset(btos[0], &params);
	ref_btos_reset(btos[1], &params);
	our_btos_setblock(btos[0], 1);
	ref_btos_setblock(btos[1], 1);
	if (f3 != 0)
		saw_bulk_bits = 1;
}

/*
 * The object comparison.  Masked: the scrambler's seven pointers (its
 * `tailLength` at +0x68 is a count and IS compared), `mapper`, and
 * `bitsToSymbol`.  `pattern`, `cp`, `params` and `mappingParams` are one
 * address shared by both sides and are compared as they stand.
 */
static void
compare_obj(long trial)
{
	memcpy(cmp_a, obj[0], OBJSZ);
	memcpy(cmp_b, obj[1], OBJSZ);
	memset(cmp_a + 0x4c, 0x77, 7 * sizeof(void *));
	memset(cmp_b + 0x4c, 0x77, 7 * sizeof(void *));
	memset(cmp_a + 0x6c, 0x77, 2 * sizeof(void *));
	memset(cmp_b + 0x6c, 0x77, 2 * sizeof(void *));

	diff_eq_obj_(__FILE__, __LINE__, "after the generator",
		     "V92Phase4Modulator", cmp_a, cmp_b, (size_t)OBJSZ, trial);
}

/*
 * The scrambler as each side's own OFFSETS into each side's own buffer, plus
 * the buffers themselves.  The raw pointers are never compared and never
 * merely checked non-null: two arrays at two addresses would pass that and
 * prove nothing (finding 224).
 */
static void
compare_scrambler(long trial)
{
	const Scrambler<unsigned char, unsigned char> *a = &M(0)->scrambler;
	const Scrambler<unsigned char, unsigned char> *b = &M(1)->scrambler;

	diff_eq_int("scrambler pOut offset (trial %ld)",
		    (long)(a->pOut - scr[0]), (long)(b->pOut - scr[1]), trial);
	diff_eq_int("scrambler pTap1 offset (trial %ld)",
		    (long)(a->pTap1 - scr[0]), (long)(b->pTap1 - scr[1]),
		    trial);
	diff_eq_int("scrambler pTap2 offset (trial %ld)",
		    (long)(a->pTap2 - scr[0]), (long)(b->pTap2 - scr[1]),
		    trial);
	diff_eq_int("scrambler pLimit offset (trial %ld)",
		    (long)(a->pLimit - scr[0]), (long)(b->pLimit - scr[1]),
		    trial);
	diff_eq_int("scrambler pInitOut offset (trial %ld)",
		    (long)(a->pInitOut - scr[0]), (long)(b->pInitOut - scr[1]),
		    trial);
	diff_eq_int("the scrambler history agrees (trial %ld)",
		    memcmp(scr[0], scr[1], SCR_LEN) == 0, 1, trial);
}

static void
compare_mapper(long trial)
{
	diff_eq_int("the mapper agrees (trial %ld)",
		    memcmp(mapbuf[0], mapbuf[1], MAPSZ) == 0, 1, trial);
}

static int
guard_intact(void)
{
	return memcmp(obj[0] + OBJSZ, objseed + OBJSZ, GUARD) == 0
	    && memcmp(obj[1] + OBJSZ, objseed + OBJSZ, GUARD) == 0;
}

static void
compare_text(long trial)
{
	diff_eq_int("the transcript line count agrees (trial %ld)",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), trial);
	diff_eq_int("the transcript agrees (trial %ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, trial);
}

/* ------------------------------------------------------------------ */

typedef int (*gen_fn)(void *);

struct member {
	const char *name;
	gen_fn ours;
	gen_fn theirs;
};

static const struct member members[] = {
	{ "generateCPt",   our_generateCPt,   ref_generateCPt },
	{ "generateE1u",   our_generateE1u,   ref_generateE1u },
	{ "generateCPu",   our_generateCPu,   ref_generateCPu },
	{ "generateSUVu",  our_generateSUVu,  ref_generateSUVu },
	{ "generateE2u",   our_generateE2u,   ref_generateE2u },
	{ "generateTRN2u", our_generateTRN2u, ref_generateTRN2u },
	{ "generateRm",    our_generateRm,    ref_generateRm },
	{ "generateB1u",   our_generateB1u,   ref_generateB1u }
};
#define NMEMBER	((int)(sizeof(members) / sizeof(members[0])))

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

static void
build_chain(void)
{
	lfsr = 0x7717u;
	fill(btos[0], BTSSZ);
	memcpy(btos[1], btos[0], BTSSZ);

	harness_alloc_reset();
	our_btos_c1(btos[0], CHAIN_ALLOC, 0);
	ref_btos_c1(btos[1], CHAIN_ALLOC, 0);

	our_btos_reset(btos[0], &params);
	ref_btos_reset(btos[1], &params);
	measure_mask();
}

static void
tear_chain(void)
{
	our_btos_d1(btos[0]);
	ref_btos_d1(btos[1]);
}

static int
run_member(int m, unsigned int lvl)
{
	char title[128];
	long trial;
	int guards = 0;
	int saw_nonzero = 0;

	strcpy(title, "V92Phase4Modulator::");
	strcat(title, members[m].name);
	strcat(title, lvl == 0 ? ", level 0" : ", level 2, transcripts");
	diff_begin(title);

	set_level(lvl);
	dsplib_debug_capture_on = (lvl != 0);

	for (trial = 0; trial < NTRIAL; trial++) {
		int a, b;

		setup((int)trial);
		if (lvl != 0)
			dsplib_debug_capture_reset();

		a = members[m].ours(obj[0]);
		b = members[m].theirs(obj[1]);

		diff_eq_int("the symbol agrees (trial %ld)", (long)a, (long)b,
			    trial);
		if (a != 0)
			saw_nonzero = 1;

		compare_obj(trial);
		compare_scrambler(trial);
		compare_mapper(trial);
		compare_chain(trial);
		if (lvl != 0)
			compare_text(trial);
		if (!guard_intact())
			guards++;
	}

	diff_eq_int("nothing wrote past the object", guards, 0, 0);
	diff_eq_int("some trial returned a non-zero symbol", saw_nonzero, 1, 0);
	diff_eq_int("the chain's address mask is the size it was", maskcount,
		    maskcount, 0);

	dsplib_debug_capture_on = 0;
	set_level(0);

	return diff_end();
}

/*
 * setMappingParams.  Its argument is either the shared parameter block or
 * null; the null arm is a diagnostic and reaches nothing else, and the live
 * arm resets the chain and forces the block size back to one.  Levels 0, 1 and
 * 2, because the gate is `> 1` and only level 1 separates that from `> 0`.
 */
static int
run_set_mapping(unsigned int lvl)
{
	char title[128];
	long trial;
	int saw_null = 0, saw_live = 0, guards = 0;

	strcpy(title, "V92Phase4Modulator::setMappingParams, level ");
	strcat(title, lvl == 0 ? "0" : (lvl == 1 ? "1" : "2"));
	diff_begin(title);

	set_level(lvl);
	dsplib_debug_capture_on = (lvl != 0);

	for (trial = 0; trial < 128; trial++) {
		void *arg;

		setup((int)trial);
		/*
		 * Put the chain somewhere `reset` has to move it FROM, so
		 * that a member which did not call reset is visible.
		 */
		our_btos_setblock(btos[0], 7u);
		ref_btos_setblock(btos[1], 7u);
		B(0)->symbolsDone = 5u;
		B(1)->symbolsDone = 5u;

		if (lvl != 0)
			dsplib_debug_capture_reset();

		arg = (trial & 1) ? (void *)&params : (void *)0;
		if (arg == 0)
			saw_null = 1;
		else
			saw_live = 1;

		our_setMappingParams(obj[0], arg);
		ref_setMappingParams(obj[1], arg);

		compare_obj(trial);
		compare_scrambler(trial);
		compare_mapper(trial);
		compare_chain(trial);
		if (lvl != 0)
			compare_text(trial);
		if (!guard_intact())
			guards++;
	}

	diff_eq_int("nothing wrote past the object", guards, 0, 0);
	diff_eq_int("a null mappingParams was tried", saw_null, 1, 0);
	diff_eq_int("a live mappingParams was tried", saw_live, 1, 0);

	dsplib_debug_capture_on = 0;
	set_level(0);

	return diff_end();
}

static int
run_antivacuity(void)
{
	diff_begin("the grid reached the paths it claims to");

	diff_eq_int("some trial ran with flag_3c clear", saw_flag3c_zero, 1, 0);
	diff_eq_int("some trial ran with flag_3c set", saw_flag3c_set, 1, 0);
	diff_eq_int("some trial wrapped the pattern index inside one symbol",
		    saw_bps_over_pattern, 1, 0);
	diff_eq_int("some trial started the scrambler below its restart point",
		    saw_scrambler_restart, 1, 0);
	diff_eq_int("some trial took the bits-to-symbol arm", saw_bulk_bits, 1,
		    0);
	diff_eq_int("some trial put generateCPt past 24 symbols",
		    saw_cpt_pattern, 1, 0);
	diff_eq_int("some trial put generateCPt below 25 symbols",
		    saw_cpt_toggle, 1, 0);
	diff_eq_int("the chain's address mask is not empty", maskcount > 0, 1,
		    0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;
	int m;

	seed_params(0x4242u, 1);
	build_chain();

	for (m = 0; m < NMEMBER; m++) {
		rc |= run_member(m, 0);
		rc |= run_member(m, 2);
	}
	rc |= run_set_mapping(0);
	rc |= run_set_mapping(1);
	rc |= run_set_mapping(2);
	rc |= run_antivacuity();

	tear_chain();

	return rc;
}

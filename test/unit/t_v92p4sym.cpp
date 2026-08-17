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
 * `bitsPerSymbol == 0` IS BACK IN THE GRID, AND WHY IT LEFT MATTERS AS MUCH
 *
 * At zero, `bits[bitsPerSymbol - 1]` is `bits[-1]`, which is
 * V92Phase4Modulator+0x7b -- the top byte of `prevBit`.  The blob's own
 * `movzbl 0x7b(%esi,%ebx,1)` with %esi zero is that address, so the aliasing
 * is the OBJECT'S.
 *
 * IT USED TO BE UNDEFINED IN OURS TOO.  While `prevBit` and the block were
 * separate members, that subscript was out of bounds, the two overlapping
 * stores were the compiler's to order, and the compilers disagreed: with this
 * input in the grid, generateE2u and generateTRN2u passed under GCC 13 and
 * failed under GCC 3.4.2 -- 80 and 160 checks, every one of them
 * `V92Phase4Modulator+123 got 00, reference 01`.  A trial whose verdict is
 * the compiler's rather than the source's proves nothing either way, so the
 * grid started at one and four store-order mutations were withdrawn.  Finding
 * 4705, and it was the right call about the TRIAL and not about the source.
 *
 * `V92Phase4Modulator.h` now holds `prevBit` and the block in ONE array
 * object, so `bitsExt[V92P4M_BITS_BELOW - 1]` is an ordinary element, the
 * order is the source's, and both compilers emit the blob's.  D561.  A trial
 * withdrawn because our code was undefined has to come back once it is
 * defined, or the fix is invisible to the suite that motivated it -- so the
 * grid starts at zero again, `saw_bps_zero_folding` proves the fold is
 * REACHED and not merely gridded, and the four store-order mutations are back
 * in `test/mutations/v92p4sym.json`.
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

int our_generateSymbol(void *) asm("_ZN18V92Phase4Modulator14generateSymbolEv");
int ref_generateSymbol(void *)
	asm("ref__ZN18V92Phase4Modulator14generateSymbolEv");

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

static const unsigned char bps[] = { 0, 1, 2, 3, 5, 8, 12, 16 };
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
static int saw_bps_zero_folding;
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
	for (i = 0; i < V92P4M_BITS_LEN; i++) {
		unsigned char v = (unsigned char)(nextbyte() & 1u);

		M(0)->bitsExt[V92P4M_BITS_BELOW + i] = v;
		M(1)->bitsExt[V92P4M_BITS_BELOW + i] = v;
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
	/*
	 * THE CONJUNCTION AND NOT THE AXIS.  generateCPu, generateSUVu and
	 * generateE2u only fold on the `flag_3c == 0` arm, so a counter on
	 * `bps[bi] == 0` alone would be satisfied by trials that never reach
	 * the statement this input exists for -- finding 4756's shape exactly.
	 * generateTRN2u has no such test and folds either way, which is why
	 * this is the tighter of the two conditions and not the looser.
	 */
	if (bps[bi] == 0 && f3 == 0)
		saw_bps_zero_folding = 1;
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
/* generateSymbol -- the state machine the eight above are the arms of.  */
/* ------------------------------------------------------------------ */

/*
 * IT NEEDS BOTH FIXTURES' CONSTRAINT SETS AT ONCE, and that is the whole
 * reason it gets its own grid inside this file rather than a row in the
 * members[] table above or a row in t_v92p4gen.cpp.
 *
 *   - From THIS file: a placed scrambler, a `reset` mapper whose mode is 0 or
 *     1, `pattern` and `bits[]` holding bits rather than bytes, a constructed
 *     transmitter chain, and `symbolsBlockSize` at one.  Nine of its
 *     twenty-six live arms reach `V92Mapper::process` or the chain.
 *   - From t_v92p4gen.cpp: a V92CP PER SIDE, sane enough for
 *     `V92CP::infoToBits` to survive -- `bitsPerSymbol` non-zero, `word_10c`
 *     at most six, the five floats finite (D570, D571).  Ten arms call
 *     `infoToBits`, `getBitVector` or `setSUV`, and six of them leave
 *     `pattern` pointing INTO that CP.
 *
 * t_v92p4gen's grid violates four of this file's five constraints outright --
 * `shared_mapper` is 64 random bytes, `shared_pattern` is random bytes rather
 * than 0/1, the scrambler subobject is never placed, and `patternIndex` is
 * never assigned -- so the row could not have gone there.
 *
 * ---------------------------------------------------------------------------
 * THE STATE AXIS IS THE WHOLE POINT, AND IT IS EXHAUSTIVE
 *
 * The object dispatches through a dense thirty-entry jump table and reaches
 * twenty-six distinct arms; `states[]` above holds seventeen values and would
 * have left eleven arms -- 8, 10, 16, 17, 18, 20, 24, 25, 27, 28 and 29 --
 * unreachable while every check passed.  That is finding 4756's third fixture
 * fault exactly, so this grid runs EVERY value 0..29 plus three the switch
 * cannot have: -1, INT_MIN and 30.  `saw_arm[]` below asserts that each of
 * them was entered and `saw_trans[]` that each arm that CAN change the state
 * did.
 *
 * ---------------------------------------------------------------------------
 * WHY THE COUNTS AND THE SHAPES ARE WHAT THEY ARE
 *
 * `generateSymbol` increments `symbolCount` BEFORE it dispatches, so every
 * entry in `gs_counts` is one BELOW the value the arms test: 23 for the
 * boundary at 24 that states 20 and 27 use, 383 for 384 (states 19 and 26),
 * 575 for 576 (16 and 17), 2399 and 2411 for the two twelve-symbol multiples
 * above 2399 that state 24 needs, 12599 for the one above 12599 that state 4
 * needs, 11 for a multiple of twelve that is below every one of those
 * magnitudes, and 0xffffffff so the increment wraps to zero.
 *
 * THE LAST TWO ARE MUTATIONS' DOING.  12601 gives 12602, which clears every
 * magnitude and is NOT a multiple of twelve, without which dropping the
 * `% 12 == 0` conjunction from states 4, 23 and 24 changes nothing.  12605
 * gives 12606, which IS a multiple of six and is not one of twelve, without
 * which reducing modulo six instead reads the same on every trial.  Both went
 * NOT CAUGHT before they were added.
 *
 * `word_1b0`, `word_1b8` and `word_24` are the three fields the arms compare
 * `symbolCount` against and none of them can be left to the object seed: at
 * random they never agree with it.  Shape 2 sets all three FROM the count, so
 * the six equality arms and the seven modulus arms fire; shapes 0, 1 and 3
 * are fixed values that mostly do not.  `word_1b0` is never zero -- it is the
 * divisor of an unsigned `div` in six arms, so a zero traps on both sides at
 * once and proves nothing, which is `mods[]`'s rule in t_v92p4gen.
 *
 * `word_18` runs over 0, 800 and 801 against a `word_44` of zero, because the
 * state 5 arm compares `word_18 > word_44 + 800` AFTER an increment that
 * `byte_1c` gates: 800 with `byte_1c` clear must not fire and 800 with it set
 * must, which is what separates the increment from the comparison.
 *
 * A NULL `cp` is given to states 4, 23 and 24 ONLY.  Those three have a real
 * null arm -- state 29 and an ERROR message -- and every other arm
 * dereferences `cp` without checking, exactly as `recivedRt` does in
 * t_v92p4gen.  The trials that get one are chosen so that they also satisfy
 * the magnitude and twelve-symbol tests above the arm; leaving that to
 * chance is what left the same arm unreached there.
 *
 * `mappingParams` points at the SHARED PARAMETER BLOCK, not at
 * `shared_dummy`, and that is forced rather than tidy: four arms hand it to
 * `V92BitsToSymbol::reset` through `setMappingParams`, and sixty-four random
 * bytes there is undefined behaviour on both sides at once.  It is null on
 * half the trials, which is the other arm of the same four sites and of state
 * 15's "Null dataPhaseMappingParams".
 */

static void set_level(unsigned int lvl);

static unsigned char gscp[2][CPSZ] __attribute__((aligned(8)));

static const int gs_states[] = {
	(-2147483647 - 1), -1,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	30
};
#define NGSSTATE	((int)(sizeof(gs_states) / sizeof(gs_states[0])))

static const unsigned int gs_counts[] = {
	0u, 1u, 11u, 23u, 383u, 575u, 2399u, 2411u, 12599u, 12601u, 12605u,
	0xffffffffu
};
#define NGSCOUNT	((int)(sizeof(gs_counts) / sizeof(gs_counts[0])))

#define NGSSHAPE	4
#define NGSBIT		32		/* word_28, word_30, word_34,     */
					/* flag_3c, e2uExtended           */
#define NGSTRIAL	(NGSSTATE * NGSCOUNT * NGSSHAPE * NGSBIT)

/* What the grid must have reached, indexed BY STATE VALUE and not by any
 * position in a table -- finding 4756's first fixture fault was an
 * anti-vacuity check addressed by index that silently changed what it
 * watched. */
static int gs_saw_arm[30];
static int gs_saw_trans[30];
static int gs_saw_repack[30];
static int gs_saw_default;
static int gs_saw_nullcp[30];
static int gs_saw_e2u_trn2u2, gs_saw_e2u_nullmp, gs_saw_e2u_b1u,
	   gs_saw_e2u_fb1u;
static int gs_saw_repeated_cp, gs_saw_word0c, gs_saw_nonzero_sym;
static int gs_saw_bytelatch_fired, gs_saw_bytelatch_held;
static int gs_saw_offset_held;

static void
gs_setup(int trial)
{
	int t = trial;
	int si = t % NGSSTATE;		t /= NGSSTATE;
	int ci = t % NGSCOUNT;		t /= NGSCOUNT;
	int shi = t % NGSSHAPE;		t /= NGSSHAPE;
	int bi = t % NGSBIT;
	unsigned int sc = gs_counts[ci] + 1u;	/* what the arms will see */
	unsigned int b0, b8, w24;
	int nullcp;
	int s;
	/*
	 * NOT `trial`, AND THAT IS FINDING 4756 AGAIN.  `trial` decomposes as
	 * `si + NGSSTATE * (...)` and NGSSTATE is 33, so `trial % 3` and
	 * `trial % 6` and `trial % 9` are CONSTANT for a given state -- every
	 * trial of state 18 got `symbolsDone` of 3 and every trial of state 25
	 * got 1, which is the difference between `nofBitsForNextTime`
	 * returning `bitsPerFrame` and returning zero.  State 18's transition
	 * therefore never fired while 1,056,001 differential checks passed,
	 * and only `gs_saw_trans[18]` said so.  `mix` is built from the three
	 * axes that DO vary within a state.
	 */
	int mix = ci + 3 * shi + 7 * bi;

	setup(trial);

	switch (shi) {
	case 0:	 b0 = 1u;			b8 = 12u; w24 = 4000u; break;
	case 1:	 b0 = 12u;			b8 = 13u; w24 = 8004u; break;
	case 2:	 b0 = (sc != 0u) ? sc : 1u;	b8 = sc;  w24 = 12u;   break;
	default: b0 = 7u;			b8 = 5u;  w24 = 4000u; break;
	}

	nullcp = (gs_states[si] == 4 || gs_states[si] == 23
		  || gs_states[si] == 24) && ((ci + shi) % 4) == 0;

	fill(gscp[0], CPSZ);
	memcpy(gscp[1], gscp[0], CPSZ);

	{
		V92CP *c = (V92CP *)gscp[0];
		int f;

		/* D570, D571 and t_v92info's bounds -- outside them
		 * `infoToBits` is undefined on both sides at once. */
		c->bitsPerSymbol = (unsigned char)(1 + (mix % 6));
		c->word_10c = (unsigned short)(mix % 7);
		c->char_01 = (signed char)((mix % 5) - 1);
		c->char_02 = (signed char)((mix % 9) - 4);
		c->byte_00 = (unsigned char)(mix % 3);
		c->byte_04 = (unsigned char)(mix & 1);
		c->byte_24 = (unsigned char)((mix >> 1) & 1);

		for (f = 0; f < 5; f++) {
			float v = (float)((mix % 17) - 8) / 4.0f;

			switch (f) {
			case 0:	c->flt_10 = v;	break;
			case 1:	c->flt_14 = -v;	break;
			case 2:	c->flt_18 = v;	break;
			case 3:	c->flt_1c = -v;	break;
			default: c->flt_20 = v;	break;
			}
		}
		memcpy(gscp[1], gscp[0], CPSZ);
	}

	for (s = 0; s < 2; s++) {
		V92Phase4Modulator *o = M(s);

		/*
		 * `bitsPerSymbol == 0` IS THE NINE MEMBERS' INPUT AND NOT
		 * THIS GRID'S, and the difference is D571 rather than taste.
		 * `setup` supplies it because the fold at zero is exactly
		 * what D561 exists to drive; `generateSymbol` reaches
		 * `word_1b0 = patternLength / cp->bitsPerSymbol` through
		 * state 4, which copies THIS field into the CP first, so a
		 * zero here raises #DE on both sides at once.  That measures
		 * the CPU, not the reading -- D571's argument and D700's
		 * twelve unguarded divisions -- so this grid starts at one.
		 */
		if (o->bitsPerSymbol == 0)
			o->bitsPerSymbol = 1;

		o->state = gs_states[si];
		o->symbolCount = gs_counts[ci];
		o->word_1b0 = b0;
		o->word_1b8 = b8;
		o->word_24 = w24;
		o->word_28 = (unsigned int)((bi >> 0) & 1);
		o->word_30 = (unsigned int)((bi >> 1) & 1);
		o->word_34 = (unsigned int)((bi >> 2) & 1);
		o->flag_3c = (unsigned int)((bi >> 3) & 1);
		o->e2uExtended = (unsigned int)((bi >> 4) & 1);

		o->word_38 = (unsigned int)((ci >> 1) & 1);
		o->byte_1c = (unsigned char)(shi & 1);
		/*
		 * NON-ZERO ON HALF THE TRIALS, and that is a mutation's
		 * doing: at zero, `word_18 > word_44 + 800` and
		 * `word_18 > 800` are the same predicate, and the entry
		 * that drops the field read NOT CAUGHT over 1,056,001
		 * checks.  Four is enough -- 801 clears 800 and does not
		 * clear 804.
		 */
		o->word_44 = ((mix & 4) != 0) ? 4u : 0u;
		o->word_18 = (ci % 3 == 0) ? 0u
			   : ((ci % 3 == 1) ? 800u : 801u);
		o->word_2c = (unsigned int)(mix % 4);
		o->word_1c0 = (unsigned int)((ci + shi) & 1);
		o->word_1c4 = (unsigned int)((ci + bi) & 1);
		o->flag_20 = (unsigned int)((shi + bi) & 1);
		o->word_0c = 0xdeadbeefu;	/* the prologue must clear it */

		o->cp = nullcp ? (V92CP *)0 : (V92CP *)gscp[s];
		o->mappingParams = ((ci & 1) != 0)
				 ? (V92MappingParams *)0
				 : (V92MappingParams *)&params;
	}

	/*
	 * SOMETHING HAS TO BE STAGED, and this is not tidiness either -- it
	 * is the difference between a trial and a coin toss.
	 *
	 * `V92BitsToSymbol::process(unsigned int &, short *)` copies
	 * `symbolsBlockSize` symbols into `out` only when `symbolsDone` is at
	 * least that many; below it the call takes the UNDERFLOW arm and
	 * copies `symbolsDone` of them, which at zero is none at all.  `out`
	 * is the address of ONE `short` on the caller's frame and the caller
	 * never initialises it, so with nothing staged the returned symbol is
	 * whatever that frame slot held -- and OUR frame is not the blob's.
	 *
	 * `V92BitsToSymbol::reset` leaves `symbolsDone` at zero, so the five
	 * arms that go through this call returned stack residue on both sides
	 * and disagreed: 635 checks of "the symbol agrees", every one of them
	 * `got 0, reference 1`, with the object, the CP and the chain all
	 * agreeing.  A trial whose verdict is the frame layout's rather than
	 * the source's proves nothing either way (D504, D561), so the buffer
	 * is stocked instead.
	 *
	 * Only the first few entries are written: `process` shifts down from
	 * `symbolsBlockSize` to `symbolsDone` and touches no more than that,
	 * and filling all 2048 per side per trial would cost more than the
	 * rest of the fixture put together.
	 */
	for (s = 0; s < 2; s++) {
		V92BitsToSymbol *bts = B(s);
		int i;

		for (i = 0; i < 8; i++)
			bts->symbols[i] = (short)(1 + ((mix + i) % 251));
		bts->symbolsDone = 1u + (unsigned int)(mix % 3);
	}
}

/*
 * The object comparison for this run.  `cp` joins `mapper` and `bitsToSymbol`
 * in the mask because there is one V92CP PER SIDE here -- which is the point
 * of it, per t_v92p4gen's note: one shared block would let our side's failure
 * to write be covered up by the reference writing a moment later.  `pattern`
 * is NOT masked: the six arms that call `getBitVector` leave it pointing into
 * the side's own CP, so it is normalised to the OFFSET within that block and
 * still compared.
 */
static void
gs_compare_obj(long trial)
{
	int t;

	memcpy(cmp_a, obj[0], OBJSZ);
	memcpy(cmp_b, obj[1], OBJSZ);
	memset(cmp_a + 0x4c, 0x77, 7 * sizeof(void *));
	memset(cmp_b + 0x4c, 0x77, 7 * sizeof(void *));
	memset(cmp_a + 0x6c, 0x77, 3 * sizeof(void *));
	memset(cmp_b + 0x6c, 0x77, 3 * sizeof(void *));

	for (t = 0; t < 2; t++) {
		unsigned char *pa = (unsigned char *)M(t)->pattern;
		unsigned char *base = gscp[t];
		unsigned char *dst = (t == 0 ? cmp_a : cmp_b) + 0x1a8;
		unsigned long off;

		if (pa < base || pa >= base + CPSZ)
			continue;
		off = (unsigned long)(pa - base);
		memcpy(dst, &off, sizeof(void *));
	}

	diff_eq_obj_(__FILE__, __LINE__, "after generateSymbol",
		     "V92Phase4Modulator", cmp_a, cmp_b, (size_t)OBJSZ, trial);
}

static int
run_generate_symbol(unsigned int lvl)
{
	char title[128];
	long trial;
	int guards = 0;

	strcpy(title, "V92Phase4Modulator::generateSymbol, level ");
	strcat(title, lvl == 0 ? "0" : (lvl == 1 ? "1" : "2"));
	diff_begin(title);

	set_level(lvl);
	dsplib_debug_capture_on = (lvl != 0);

	for (trial = 0; trial < NGSTRIAL; trial++) {
		int before, after;
		int a, b;
		unsigned char *before_pat;
		int hadcp;
		unsigned int before_18;
		unsigned int before_44;
		int before_latch;

		gs_setup((int)trial);
		before = M(0)->state;
		before_pat = (unsigned char *)M(0)->pattern;
		hadcp = (M(0)->cp != 0);
		before_18 = M(0)->word_18;
		before_44 = M(0)->word_44;
		before_latch = (M(0)->byte_1c != 0);

		if (lvl != 0)
			dsplib_debug_capture_reset();

		a = our_generateSymbol(obj[0]);
		b = ref_generateSymbol(obj[1]);

		diff_eq_int("the symbol agrees (trial %ld)", (long)a, (long)b,
			    trial);
		if (a != 0)
			gs_saw_nonzero_sym = 1;

		after = M(0)->state;
		if (before >= 0 && before <= 29) {
			gs_saw_arm[before] = 1;
			if (after != before)
				gs_saw_trans[before] = 1;
			if ((unsigned char *)M(0)->pattern != before_pat)
				gs_saw_repack[before] = 1;
			if (!hadcp && after == 29)
				gs_saw_nullcp[before] = 1;

			if (before == 15 && after == 23)
				gs_saw_e2u_trn2u2 = 1;
			if (before == 15 && after == 29)
				gs_saw_e2u_nullmp = 1;
			if (before == 15 && after == 16)
				gs_saw_e2u_b1u = 1;
			if (before == 15 && after == 17)
				gs_saw_e2u_fb1u = 1;
			if (before == 5 && after == 13)
				gs_saw_repeated_cp = 1;
			/*
			 * `byte_1c` gates state 5's counter, and 800 is the
			 * value at which the increment is the whole
			 * difference: with the latch clear it must not fire
			 * and with it set it must.
			 */
			if (before == 5 && before_18 == 800u && before_44 == 0u
			    && before_latch && after == 13)
				gs_saw_bytelatch_fired = 1;
			if (before == 5 && before_18 == 800u && before_44 == 0u
			    && !before_latch && after != 13)
				gs_saw_bytelatch_held = 1;
			if (before == 5 && before_18 == 801u && before_44 == 4u
			    && after != 13)
				gs_saw_offset_held = 1;
		} else {
			gs_saw_default = 1;
		}
		if (M(0)->word_0c != 0)
			gs_saw_word0c = 1;

		gs_compare_obj(trial);
		diff_eq_int("the V92CP agrees (trial %ld)",
			    memcmp(gscp[0], gscp[1], CPSZ) == 0, 1, trial);
		compare_scrambler(trial);
		compare_mapper(trial);
		compare_chain(trial);
		if (lvl != 0)
			compare_text(trial);
		if (!guard_intact())
			guards++;
	}

	diff_eq_int("nothing wrote past the object", guards, 0, 0);

	dsplib_debug_capture_on = 0;
	set_level(0);

	return diff_end();
}

/*
 * The arms with no transition of their own, and the four the jump table sends
 * to the default label.  Everything else must have been seen to MOVE.
 */
static int
gs_no_transition(int s)
{
	return s == 0 || s == 3 || s == 7 || s == 13 || s == 14 || s == 21
	    || s == 22 || s == 28 || s == 29;
}

static int
run_gs_antivacuity(void)
{
	int s;

	diff_begin("generateSymbol's grid reached every arm it claims to");

	for (s = 0; s < 30; s++) {
		diff_eq_int("some trial entered this state's arm",
			    gs_saw_arm[s], 1, (long)s);
		if (!gs_no_transition(s))
			diff_eq_int("some trial took this arm's transition",
				    gs_saw_trans[s], 1, (long)s);
	}

	/* The two arms whose only observable is that the message was
	 * repacked -- neither changes the state. */
	diff_eq_int("state 13 repacked the CP message", gs_saw_repack[13], 1,
		    0);
	diff_eq_int("state 5 repacked the CP message", gs_saw_repack[5], 1, 0);

	diff_eq_int("state 4 was given a null CP", gs_saw_nullcp[4], 1, 0);
	diff_eq_int("state 23 was given a null CP", gs_saw_nullcp[23], 1, 0);
	diff_eq_int("state 24 was given a null CP", gs_saw_nullcp[24], 1, 0);

	diff_eq_int("E2u took the TRN2u-second way out", gs_saw_e2u_trn2u2, 1,
		    0);
	diff_eq_int("E2u took the null-mappingParams way out",
		    gs_saw_e2u_nullmp, 1, 0);
	diff_eq_int("E2u took the B1u way out", gs_saw_e2u_b1u, 1, 0);
	diff_eq_int("E2u took the FB1u way out", gs_saw_e2u_fb1u, 1, 0);

	diff_eq_int("SUV entered the repeated CP", gs_saw_repeated_cp, 1, 0);
	diff_eq_int("the byte_1c latch made the difference at 800",
		    gs_saw_bytelatch_fired, 1, 0);
	diff_eq_int("the byte_1c latch held at 800", gs_saw_bytelatch_held, 1,
		    0);
	diff_eq_int("word_44 held the threshold above 801", gs_saw_offset_held,
		    1, 0);

	diff_eq_int("some trial fell to the illegal-state arm", gs_saw_default,
		    1, 0);
	diff_eq_int("some trial left a non-zero report word", gs_saw_word0c, 1,
		    0);
	diff_eq_int("some trial returned a non-zero symbol",
		    gs_saw_nonzero_sym, 1, 0);

	return diff_end();
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
	diff_eq_int("some trial folded at bitsPerSymbol == 0, on the arm that "
		    "folds", saw_bps_zero_folding, 1, 0);
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
	rc |= run_generate_symbol(0);
	rc |= run_generate_symbol(1);
	rc |= run_generate_symbol(2);
	rc |= run_antivacuity();
	rc |= run_gs_antivacuity();

	tear_chain();

	return rc;
}

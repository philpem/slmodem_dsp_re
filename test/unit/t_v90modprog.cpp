/*
 * t_v90modprog.cpp -- differential test of the V.90 DIGITAL side's transmit
 * chain: the five members of `V90Modem::progress`'s closure.
 *
 *     V90Modulator::progress(int *, unsigned int &, float *, unsigned int)
 *     V90Modulator::initiateRRN()
 *     V90BitsToSymbol::process(unsigned char *, unsigned int &, short *)
 *     V90Phase4Modulator::generateSymbol()
 *     V90Modem::progress(int *, unsigned int &, float *, unsigned int)
 *
 * NONE OF THESE IS EVER CONSTRUCTED BY THE SHIPPED slmodemd, and the
 * differential tier does not care: it calls the BLOB's symbols through their
 * `ref_` aliases and never drives the shipped modem, so "unreachable in
 * production" and "unreachable in this binary" are different questions
 * (findings 701, 702, 7000).
 *
 * ===========================================================================
 * A WHOLE V90Modem ON EACH SIDE, BECAUSE THAT IS THE CHEAPEST REAL CHAIN
 * ===========================================================================
 *
 * `V90Modem`'s constructor allocates `V90Parameters`, `V90Phase2Info`,
 * `V90Jd`, `V92Jd` and -- on side 0 -- the `V90Modulator`, whose own
 * constructor allocates a symbol buffer, a frame buffer, a `V90BitsToSymbol`
 * (which allocates a `V90Mapper`), a `V90Phase3Modulator` and a
 * `V90Phase4Modulator`.  Fourteen objects from one call, on each side,
 * through `modem_ctor1` / `ref_modem_ctor1` -- t_v90modemctor.cpp's shape.
 *
 * `V90MP`, `V90CP` and BOTH `V90MappingParams` are EMBEDDED in `V90Modem`,
 * which is what makes this safe: `initiateRRN` writes `V90CP::byte_13` and
 * `V90MP::CPack`, so a shared block would have each side overwriting the
 * other's evidence.  Here each side has its own.
 *
 * ===========================================================================
 * WHAT CANNOT AGREE, AND HOW IT IS MADE TO
 * ===========================================================================
 *
 * Every heap address differs between the sides for ever.  A word is therefore
 * TRANSLATED before comparison, exactly as t_v90modemctor.cpp does it: into
 * one of the fourteen named blocks it becomes a tag carrying WHICH block plus
 * the offset, into any other live allocation a tag plus the offset with the
 * block's identity dropped, and anything else is left alone.  The named tags
 * are what make a SWAP visible -- handing the phase 4 modulator the V.90
 * mapping parameters where the object hands it the V.92 ones moves a word
 * from one tagged block to another, and both would otherwise become the same
 * anonymous number on both sides.
 *
 * Offsets INTO a block survive the translation, which is what makes an
 * embedded `Scrambler` comparable without the special handling
 * t_v90modchain.cpp needs: its seven pointers all point into one allocation,
 * so each becomes that block's tag plus its own distance and a tap set to the
 * wrong distance still differs.
 *
 * A word whose RAW bytes already agree is never translated (t_v90modemctor's
 * argument: translation can only unmatch a matched pair, and an integer that
 * happens to land inside one side's heap would otherwise differ over a word
 * neither implementation wrote).
 *
 * ===========================================================================
 * THE TRAP THIS FILE EXISTS FOR
 * ===========================================================================
 *
 * `V90Modulator::progress`'s data-phase message computes its rate from
 * `bitsToSymbol->mapper->bitsPerFrame` (+0x40 -> +0x00 -> +0x04) and NOT from
 * `bitsToSymbol->bitsPerFrame` (+0x14).  Both fields exist, both are named
 * `bitsPerFrame`, and both are seeded from `V90MappingParams::word_0` by
 * their own class's `reset` -- so in any naturally reset object they hold the
 * same number and a body reading the wrong one passes every trial.
 *
 * `split_bpf` drives them apart on purpose (42 in the mapper, 48 in the
 * converter), the trial runs at debug level 2 because the message is
 * `dsplibs_debug_printf` behind `DSPLIB_DEBUG_ON()` and is invisible below
 * it, and the assertion reads the BLOB's transcript for the rate the MAPPER's
 * field gives (8000 * 42 / 6 = 56000) and requires the converter's answer
 * (8000 * 48 / 6 = 64000) to be absent.  The same trial asserts the two
 * fields still differ at the moment of the call, so a future reset
 * re-equalising them fails loudly rather than turning the discriminator off.
 *
 * ===========================================================================
 * THE ARGUMENT RANGES ARE A REAL DIGITAL-SIDE SESSION'S
 * ===========================================================================
 *
 * V.90 downstream is 8000 symbols a second, so the modem is built with
 * `nofSymbols` = 80 -- a 10 ms block -- and `progress` is called with n = 80,
 * with n = 24 (a short block, which also separates the ARGUMENT from the
 * MEMBER of the same name), and with n = 0, which every loop must treat as
 * "write nothing".  The mapping parameters are poked to a plausible V.90 set
 * before anything resets from them: 42 bits to a six-symbol frame is
 * 56 kbit/s, `shaperSR` is 3, and no constellation is longer than 128.  Left
 * at the constructor's seed they are random, and `V90Mapper::process` then
 * indexes wildly.
 *
 * WHAT IS POKED RATHER THAN REACHED, and it is stated rather than implied:
 * `V90Modulator::state` is never set to 1 by any member of the class --
 * `reset` sets 0, `progress` sets 2 and 3, `initiateRRN` sets 2 -- because
 * entry to phase 3 comes from `vPcmResetPhase3Modem`, which is outside this
 * translation unit and is not written.  So the fixture writes `state`
 * directly, identically on both sides, and the same goes for the phase 3
 * modulator's DIL cursors and the phase 4 modulator's terminal counts.
 *
 * EACH SIDE PREPARES ITSELF WITH ITS OWN CODE.  Every setup member --
 * `V90Modulator::reset`, `V90BitsToSymbol::reset`, `setSymbolsBlockSize`,
 * `V90Phase4Modulator::reset` -- is called on our object through the class
 * and on the blob's through a `ref_` alias, and the whole graph is compared
 * BEFORE the call under test as well as after it.  A fixture that prepared
 * the blob's object with our code would be proving less and would hide a
 * defect in whichever setup member differed.
 */

#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include "harness.h"

#include "dsplib/debug.h"
#include "dsplib/modem_params.h"
#include "dsplib/V90BitsToSymbol.h"
#include "dsplib/V90CP.h"
#include "dsplib/V90Demodulator.h"
#include "dsplib/V90Mapper.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Modem.h"
#include "dsplib/V90Modulator.h"
#include "dsplib/V90MP.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Phase2Info.h"
#include "dsplib/V90Phase3Modulator.h"
#include "dsplib/V90Phase4Modulator.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

/*
 * The lifecycle, both sides.  C++ has no syntax for running a constructor
 * over storage that already exists, so both sides go through asm() labels;
 * findings 223 and 224.
 */
void modem_ctor1(void *self, unsigned int side, void *modemParams, void *dil,
		 unsigned int nofSymbols, int compMode, unsigned int flag)
	asm("_ZN8V90ModemC1E12V90ModemSideP19_tagModemParametersP19tagV90DIL"
	    "descriptorj20V90ComputationalModej");
void ref_modem_ctor1(void *self, unsigned int side, void *modemParams,
		     void *dil, unsigned int nofSymbols, int compMode,
		     unsigned int flag)
	asm("ref__ZN8V90ModemC1E12V90ModemSideP19_tagModemParametersP19tagV90"
	    "DILdescriptorj20V90ComputationalModej");
void modem_dtor1(void *self) asm("_ZN8V90ModemD1Ev");
void ref_modem_dtor1(void *self) asm("ref__ZN8V90ModemD1Ev");

/*
 * The five under test, blob side.  `unsigned int *` where the member takes
 * `unsigned int &`: the same thing to the ABI, and this side has to name a C
 * type because the blob's symbol has no class to be a member of.
 */
void ref_modem_progress(void *self, int *bits, unsigned int *nofBits,
			float *out, unsigned int n)
	asm("ref__ZN8V90Modem8progressEPiRjPfj");
void ref_mod_progress(void *self, int *bits, unsigned int *nofBits, float *out,
		      unsigned int n)
	asm("ref__ZN12V90Modulator8progressEPiRjPfj");
int ref_mod_initiaterrn(void *self)
	asm("ref__ZN12V90Modulator11initiateRRNEv");
unsigned int ref_bts_process3(void *self, unsigned char *bits,
			      unsigned int *nofBits, short *out)
	asm("ref__ZN15V90BitsToSymbol7processEPhRjPs");
int ref_p4m_gensym(void *self)
	asm("ref__ZN18V90Phase4Modulator14generateSymbolEv");

/* The setup members, blob side.  See the file comment. */
void ref_mod_reset(void *self) asm("ref__ZN12V90Modulator5resetEv");
void ref_bts_reset(void *self, void *mp, int pcm)
	asm("ref__ZN15V90BitsToSymbol5resetEP16V90MappingParams7PcmType");
unsigned int ref_bts_setblock(void *self, unsigned int n)
	asm("ref__ZN15V90BitsToSymbol19setSymbolsBlockSizeEj");
void ref_p4m_reset(void *self, int law, unsigned char code, int st,
		   unsigned int nofSymbols, unsigned int arg5)
	asm("ref__ZN18V90Phase4Modulator5resetE7PcmTypeh20Phase4Modulator"
	    "Statejj");
void ref_dem_reset(void *self, unsigned int quick)
	asm("ref__ZN14V90Demodulator5resetEj");
}

/* ==================================================================== sizes */

#define MODEM_SIZE	0x49c0
#define GUARD		64
#define MODEM_SLOT	(MODEM_SIZE + GUARD)

typedef char v90modprog_modem_is_0x49c0[(sizeof(V90Modem) == MODEM_SIZE)
					? 1 : -1];

/* 80 symbols is 10 ms at V.90's 8000 downstream symbols a second. */
#define NOFSYM		80u
#define SYMBUF_SYMS	NOFSYM			/* V90Modulator's +0x68     */
#define FRAMEBUF_BYTES	(8u * NOFSYM)		/* ... and its +0x6c        */
/* `V90Modulator`'s converter is built with 3 * nofSymbols + 0x1388 symbols. */
#define BTS_SYMBOLS	(3u * NOFSYM + 0x1388u)

#define DIL_BYTES	96
#define MP_SLOT		(sizeof(struct _tagModemParameters) + 64)

/* Bits and samples.  `bits` is only read on the data-phase arm. */
#define NBITS		1024
#define NOUT		(NOFSYM + 16u)

/* The two `bitsPerFrame` values the rate-message trial drives apart. */
#define BPF_MAPPER	42u			/* 8000 * 42 / 6 = 56000    */
#define BPF_BTS		48u			/* 8000 * 48 / 6 = 64000    */

static unsigned char mobj[2][MODEM_SLOT] __attribute__((aligned(8)));
static unsigned char sown[MODEM_SLOT];		/* the seed, for the guard  */
static unsigned char mpb[MP_SLOT] __attribute__((aligned(8)));
static unsigned char dilb[DIL_BYTES] __attribute__((aligned(8)));
static unsigned char mp_pre[MP_SLOT], mp_ours[MP_SLOT];
static unsigned char dil_pre[DIL_BYTES], dil_ours[DIL_BYTES];

static int bits_in[2][NBITS];
static int bits_seed[NBITS];
static float out_f[2][NOUT];
static float out_seed[NOUT];
static unsigned int nb[2];

#define MPARAMS		((struct _tagModemParameters *)(void *)mpb)
#define MODEM(s)	((V90Modem *)(void *)mobj[s])
#define MOD(s)		(MODEM(s)->modulator)
#define P3M(s)		(MOD(s)->phase3Modulator)
#define P4M(s)		(MOD(s)->phase4Modulator)
#define BTS(s)		(MOD(s)->bitsToSymbol)
#define MAPPER(s)	(BTS(s)->mapper)
#define PARAMS(s)	(MODEM(s)->ptr_49b4)

/* ================================================================= seeding */

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	/* `| 1` so no seeded byte is ever zero; finding 230. */
	return (unsigned char)((lfsr >> 3) | 1u);
}

static void
fill(void *p, size_t n)
{
	unsigned char *q = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++)
		q[i] = nextb();
}

static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/*
 * A plausible V.90 mapping set, written into BOTH embedded blocks before the
 * constructor runs -- nothing in `V90Modem` touches them, so this is the
 * state every later `reset` derives from.  `bpf` bits to a six-symbol frame;
 * `shaperSR` is 3, which divides six, so `V90BitsToSymbol::reset`'s
 * `6 * shaperId / shaperSR` is the shaper's own block length; no
 * constellation is longer than 128, which is `V90Mapper`'s row length and the
 * bound `resetNoSpectral`'s fill loop runs to.
 */
static void
plausible_mapping(V90MappingParams *m, unsigned int bpf, int tag)
{
	unsigned int i, j;

	m->word_0 = bpf;
	for (i = 0; i < V90_CONSTELLATIONS; i++) {
		/*
		 * THE SIZES ARE BOUNDED FROM BOTH ENDS AND BOTH BOUNDS ARE THE
		 * OBJECT'S.  128 is `V90MAPPER_LEVELS`, the row length
		 * `V90Mapper::reset` fills to.  The LOWER bound is
		 * `ModulusEncoder::progress`, which reads `bitsPerFrame -
		 * signBitsPerFrame` = 42 - 3 = 39 bits as one integer and
		 * writes it out mixed-radix: the sixth digit is whatever is
		 * left after five divisions, so the six sizes must multiply to
		 * more than 2**39 or `constellation[k][codes[k]]` indexes past
		 * its row.  112 * 116 * 120 * 124 * 128 * 128 is 3.2e12
		 * against 5.5e11, which is also why a real V.90 session at
		 * 56 kbit/s carries constellations of about this size.
		 */
		unsigned int len = 112u + 4u * i;

		if (len > V90_CONSTELLATION_MAX)
			len = V90_CONSTELLATION_MAX;
		m->constellationSize[i] = len;
		for (j = 0; j < V90_CONSTELLATION_MAX; j++) {
			m->constellation[i][j] =
			    (unsigned char)(0x11u + 3u * j + 7u * i
					    + (unsigned)tag);
			m->codecConstellation[i][j] =
			    (unsigned char)(0x21u + 5u * j + 3u * i
					    + (unsigned)tag);
		}
		m->distinctIndex[i] = (int)i;
	}
	m->word_61c = 1;
	m->shaperSR = 3;
	m->shaperId = 2;
	m->shaperA1 = 0.25f;
	m->shaperA2 = -0.125f;
	m->shaperB1 = 0.5f;
	m->shaperB2 = 0.0625f;
}

/*
 * The CP and the MP into a state `infoToBits` can survive.  Both are embedded
 * and their constructors leave most of the block as they found it, so the
 * fields those two members READ have to be given values -- counts small
 * enough that seventeen bits an entry stays inside `bits`, and a group size
 * that is not zero, which is t_v90cpinfo.cpp's and t_v90mp.cpp's argument for
 * the same two functions.  The `bits` arrays are NOT touched: they keep the
 * seed, so a call that failed to re-encode is visible.
 */
static void
plausible_cp(V90CP *cp, int trial)
{
	int k, j;

	cp->word_00 = 1;
	cp->word_04 = trial & 1;
	cp->word_08 = (trial >> 1) & 1;
	cp->word_0c = (trial >> 2) & 1;
	cp->byte_10 = (signed char)(trial * 7);
	cp->byte_11 = (unsigned char)(trial % 4);
	cp->byte_12 = (unsigned char)(trial & 1);
	cp->word_14 = 0x1234 * (trial + 1);
	for (k = 0; k < 12; k++)
		cp->word_18[k] = 0x33 * (trial + k) - 0x1000;
	for (k = 0; k < 4; k++) {
		cp->nof_58[k] = (unsigned int)((trial * (k + 3)) % 17);
		for (j = 0; j < V90CP_SHORTS; j++)
			cp->short_58[k][j] =
			    (short)(0x5bu * (unsigned)(trial + k * 7 + j));
	}
	for (k = 0; k < V90CP_BUFS; k++) {
		cp->nof_buf[k] = (unsigned int)((trial * (k + 2)) % 17);
		cp->word_c70[k] = 0x17 * (trial + k);
	}
	cp->word_ca0 = (unsigned int)(trial * 3);
	cp->word_3ba8 = 17u;
	/*
	 * NOT ZERO, because `initiateRRN`'s V.92 arm stores a zero here and a
	 * field that already held the value the store writes cannot fail
	 * (finding 7105).
	 */
	cp->byte_13 = 1;
}

static void
plausible_mp(V90MP *mp, int trial)
{
	mp->Type = (char)(trial & 1);
	mp->Rate = (char)(trial % 12);
	mp->Trellis = (char)(trial % 4);
	mp->NonLin = (char)((trial >> 1) & 1);
	mp->Shaping = (char)((trial >> 2) & 1);
	mp->rateMask = (short)(0x0f0f + trial);
	mp->h1Real = (short)(0x1234 + trial);
	mp->h1Imag = (short)(0x2345 + trial);
	mp->h2Real = (short)(0x3456 + trial);
	mp->h2Imag = (short)(0x4567 + trial);
	mp->h3Real = (short)(0x5678 + trial);
	mp->h3Imag = (short)(0x6789 + trial);
	mp->word_114 = 4u;		/* the group size; zero faults      */
	/* The same 7105 argument as `V90CP::byte_13` above. */
	mp->CPack = 1;
}

/* ======================================================= the live allocations */

#define MAXLIVE	256

static void *live[MAXLIVE];
static unsigned long liveend[MAXLIVE];
static int nlive;

static void
live_refresh(void)
{
	int i;

	nlive = harness_alloc_live_set(live, MAXLIVE);
	if (nlive > MAXLIVE)
		nlive = MAXLIVE;
	for (i = 0; i < nlive; i++)
		liveend[i] = (unsigned long)live[i]
		    + malloc_usable_size(live[i]);
}

static int
live_index(unsigned long v)
{
	int i;

	for (i = 0; i < nlive; i++)
		if (v >= (unsigned long)live[i] && v <= liveend[i])
			return i;
	return -1;
}

/*
 * The fourteen blocks whose IDENTITY the comparison keeps.  Everything else
 * the two graphs allocate becomes one anonymous tag: those are the callees'
 * claims and have their own files (t_v90modchain.cpp, t_v90demctor.cpp,
 * t_v90params.cpp).
 */
#define B_MODEM		0
#define B_MOD		1
#define B_DEM		2
#define B_P2INFO	3
#define B_JD		4
#define B_JD92		5
#define B_PARAMS	6
#define B_P3M		7
#define B_P4M		8
#define B_BTS		9
#define B_MAPPER	10
#define B_SYMBUF	11
#define B_FRAMEBUF	12
#define B_BTSSYMS	13
#define NBLK		14

struct blkrange {
	unsigned long lo, hi;
};

static struct blkrange blk[2][NBLK];

static void
blk_set(int s, int k, const void *p)
{
	unsigned long v = (unsigned long)p;

	blk[s][k].lo = 0;
	blk[s][k].hi = 0;
	if (p == 0 || live_index(v) < 0)
		return;
	blk[s][k].lo = v;
	blk[s][k].hi = v + malloc_usable_size((void *)p);
}

/*
 * Take both graphs.  Called after both sides have run, because a block only
 * has a range while it is live.
 */
static void
blkmap_take(unsigned int side)
{
	int s;

	live_refresh();
	for (s = 0; s < 2; s++) {
		memset(blk[s], 0, sizeof(blk[s]));
		blk[s][B_MODEM].lo = (unsigned long)mobj[s];
		blk[s][B_MODEM].hi = (unsigned long)mobj[s] + MODEM_SIZE;
		blk_set(s, B_P2INFO, MODEM(s)->phase2Info);
		blk_set(s, B_JD, MODEM(s)->jd);
		blk_set(s, B_JD92, MODEM(s)->jd92);
		blk_set(s, B_PARAMS, MODEM(s)->ptr_49b4);
		if (side == 1)
			blk_set(s, B_DEM, MODEM(s)->demodulator);
		if (side != 0)
			continue;
		blk_set(s, B_MOD, MOD(s));
		if (blk[s][B_MOD].lo == 0)
			continue;
		blk_set(s, B_P3M, P3M(s));
		blk_set(s, B_P4M, P4M(s));
		blk_set(s, B_BTS, BTS(s));
		blk_set(s, B_SYMBUF, MOD(s)->symbolBuf);
		blk_set(s, B_FRAMEBUF, MOD(s)->frameBuf);
		if (blk[s][B_BTS].lo != 0) {
			blk_set(s, B_MAPPER, BTS(s)->mapper);
			blk_set(s, B_BTSSYMS, BTS(s)->symbols);
		}
	}
}

static unsigned long
translate(unsigned long v, int s)
{
	int k;

	if (v == 0)
		return v;
	for (k = 0; k < NBLK; k++)
		if (blk[s][k].lo != 0 && v >= blk[s][k].lo && v <= blk[s][k].hi)
			return 0x50000000ul + (unsigned long)k * 0x00100000ul
			    + (v - blk[s][k].lo);
	k = live_index(v);
	if (k >= 0)
		return 0x60000000ul + (v - (unsigned long)live[k]);
	return v;
}

static void
translate_block(unsigned char *dst, const unsigned char *src, size_t n, int s)
{
	size_t o;

	memcpy(dst, src, n);
	for (o = 0; o + 4 <= n; o += 4) {
		unsigned int w;

		memcpy(&w, dst + o, sizeof(w));
		w = (unsigned int)translate((unsigned long)w, s);
		memcpy(dst + o, &w, sizeof(w));
	}
}

/*
 * A word the two sides ALREADY agree on is never translated.  `translate`
 * cannot tell a pointer from an integer, and these objects are full of
 * integers; when one lands inside a live allocation on one side only -- the
 * heap base is randomised, so it does on some runs and not others -- it would
 * be tagged there and left alone on the other.  The guard is sound in the
 * direction that matters: translation can only UNMATCH a matched pair, so a
 * real disagreement is never suppressed.  t_v90modemctor.cpp's argument.
 */
#define TR_MAX	0x4a00

static unsigned char tra[TR_MAX], trb[TR_MAX];

/*
 * THE WORDS THAT HOLD A STATIC ADDRESS OF THAT SIDE'S OWN.  Two words inside
 * the demodulator cannot be made to agree by any fixture, because each side
 * installs the address of ITS OWN copy of a static object and both copies are
 * linked into this binary at two addresses:
 *
 *   +0x06c  `preFilter.fir.coefficients`, which `V90PreFilter::reset` points
 *           at `&preFilterCoefType1[0][0]`.  t_v90prefilter.cpp's claim.
 *   +0x094  the `V90Resampler` vptr, which is that side's own vtable.  A
 *           vtable cannot be content-checked -- ours holds our member
 *           addresses and the blob's holds the blob's.
 *
 * Both offsets are t_v90demctor.cpp's and t_v90modemctor.cpp names the same
 * pair; they are listed here rather than discovered, so that a THIRD such word
 * appearing is a failure and not a silently widened hole.  They are POISONED
 * to a constant in both copies rather than skipped, which keeps `diff_eq_obj`
 * reporting the first REAL difference first.  `demod_alias_check` is the
 * "an exclusion that never fires is a hole for nothing" half.
 */
static const unsigned demod_alias[] = { 0x06cu, 0x094u };

#define NDEMOD_ALIAS	((int)(sizeof(demod_alias) / sizeof(demod_alias[0])))

static int demod_alias_fired[NDEMOD_ALIAS];
static int demod_alias_wrong[NDEMOD_ALIAS];

static void
cmp_region_p(const char *what, const char *type, const void *a, const void *b,
	     size_t n, long tag, const unsigned *pz, int npz)
{
	const unsigned char *sa = (const unsigned char *)a;
	const unsigned char *sb = (const unsigned char *)b;
	size_t o;
	int k;

	if (a == 0 || b == 0 || n == 0)
		return;
	if (n > TR_MAX)
		n = TR_MAX;
	translate_block(tra, sa, n, 0);
	translate_block(trb, sb, n, 1);
	for (o = 0; o + 4 <= n; o += 4) {
		unsigned int ra, rb;

		memcpy(&ra, sa + o, 4);
		memcpy(&rb, sb + o, 4);
		if (ra == rb) {
			memcpy(tra + o, &ra, 4);
			memcpy(trb + o, &rb, 4);
		}
	}
	for (k = 0; k < npz; k++)
		if ((size_t)pz[k] + 4 <= n) {
			memset(tra + pz[k], 0x77, 4);
			memset(trb + pz[k], 0x77, 4);
		}
	diff_eq_obj_(__FILE__, __LINE__, what, type, tra, trb, n, tag);
}

static void
cmp_region(const char *what, const char *type, const void *a, const void *b,
	   size_t n, long tag)
{
	cmp_region_p(what, type, a, b, n, tag, (const unsigned *)0, 0);
}

static void
demod_alias_check(void)
{
	const unsigned char *pa = (const unsigned char *)MODEM(0)->demodulator;
	const unsigned char *pb = (const unsigned char *)MODEM(1)->demodulator;
	int i;

	if (pa == 0 || pb == 0)
		return;
	for (i = 0; i < NDEMOD_ALIAS; i++) {
		unsigned int wa, wb;

		memcpy(&wa, pa + demod_alias[i], 4);
		memcpy(&wb, pb + demod_alias[i], 4);
		if (wa != wb)
			demod_alias_fired[i] = 1;
		if (wa == 0 || wb == 0
		    || live_index((unsigned long)wa) >= 0
		    || live_index((unsigned long)wb) >= 0)
			demod_alias_wrong[i] = 1;
	}
}

/*
 * The whole graph, translated: the modem itself -- which carries the embedded
 * `V90MP`, `V90CP` and both `V90MappingParams` -- and every block hanging off
 * it that this test reaches.
 */
static void
compare_graph(const char *what, unsigned int side, long tag)
{
	blkmap_take(side);

	cmp_region(what, "V90Modem", mobj[0], mobj[1], MODEM_SIZE, tag);
	diff_eq_int("nothing stored past the modem (%ld)",
		    memcmp(mobj[0] + MODEM_SIZE, sown + MODEM_SIZE, GUARD) == 0
		    && memcmp(mobj[1] + MODEM_SIZE, sown + MODEM_SIZE,
			      GUARD) == 0, 1, tag);

	cmp_region(what, "V90Phase2Info", MODEM(0)->phase2Info,
		   MODEM(1)->phase2Info, 0x24, tag);
	cmp_region(what, "V90Parameters", MODEM(0)->ptr_49b4,
		   MODEM(1)->ptr_49b4, sizeof(V90Parameters), tag);

	if (side == 1) {
		cmp_region_p(what, "V90Demodulator", MODEM(0)->demodulator,
			     MODEM(1)->demodulator, 0x298, tag, demod_alias,
			     NDEMOD_ALIAS);
		demod_alias_check();
	}
	if (side != 0 || MOD(0) == 0 || MOD(1) == 0)
		return;

	cmp_region(what, "V90Modulator", MOD(0), MOD(1), 0x70, tag);
	cmp_region(what, "V90Phase3Modulator", P3M(0), P3M(1), 0x398, tag);
	cmp_region(what, "V90Phase4Modulator", P4M(0), P4M(1), 0x2fac, tag);
	cmp_region(what, "V90BitsToSymbol", BTS(0), BTS(1), 0x24, tag);
	cmp_region(what, "V90Mapper", MAPPER(0), MAPPER(1), 0x704, tag);
	cmp_region(what, "the symbol buffer", MOD(0)->symbolBuf,
		   MOD(1)->symbolBuf, 2u * SYMBUF_SYMS, tag);
	cmp_region(what, "the frame buffer", MOD(0)->frameBuf,
		   MOD(1)->frameBuf, FRAMEBUF_BYTES, tag);
	cmp_region(what, "the converter's symbols", BTS(0)->symbols,
		   BTS(1)->symbols, 2u * BTS_SYMBOLS, tag);
}

/* ========================================================= build and destroy */

static void
build_pair(long trial, unsigned int side, unsigned int flag, unsigned int bpf)
{
	int s;

	lfsr = 0x4d1bu + 0x9e37u * (unsigned)trial;
	fill(sown, MODEM_SLOT);
	memcpy(mobj[0], sown, MODEM_SLOT);
	memcpy(mobj[1], sown, MODEM_SLOT);
	fill(dilb, sizeof(dilb));

	/*
	 * The modem parameter block is ZEROED and not seeded: every field of
	 * `V90Parameters` is derived from it, and a seeded one gives a
	 * signalling NaN about one word in 250 and an allocation length of
	 * two billion rather more often than that.  t_v90demctor.cpp and
	 * t_v90modemctor.cpp do the same and for the same reason.
	 */
	memset(mpb, 0, sizeof(mpb));
	MPARAMS->codecType = (int)(trial % 5);

	/*
	 * Before the constructor, because the two embedded blocks are what
	 * every later `reset` reads and nothing in `V90Modem` writes them.
	 * The two get DIFFERENT tables, so a body reaching for the wrong one
	 * of the pair fails.
	 */
	for (s = 0; s < 2; s++) {
		plausible_mapping(&MODEM(s)->mappingParams, bpf, 0);
		plausible_mapping(&MODEM(s)->mappingParamsAlt, bpf, 1);
	}

	harness_alloc_reset();

	memcpy(mp_pre, mpb, MP_SLOT);
	memcpy(dil_pre, dilb, DIL_BYTES);
	modem_ctor1(mobj[0], side, mpb, dilb, NOFSYM, 0, flag);
	memcpy(mp_ours, mpb, MP_SLOT);
	memcpy(dil_ours, dilb, DIL_BYTES);
	memcpy(mpb, mp_pre, MP_SLOT);
	memcpy(dilb, dil_pre, DIL_BYTES);
	ref_modem_ctor1(mobj[1], side, mpb, dilb, NOFSYM, 0, flag);

	diff_eq_int("the shared parameter block agrees (%ld)",
		    memcmp(mpb, mp_ours, MP_SLOT) == 0, 1, trial);
	diff_eq_int("the shared DIL block agrees (%ld)",
		    memcmp(dilb, dil_ours, DIL_BYTES) == 0, 1, trial);
}

/*
 * For a side outside {0, 1} the constructor wrote NEITHER pointer, so +0x00
 * and +0x04 still hold seed bytes; they are nulled before the destructor,
 * which calls `~V90Modulator` and `~V90Demodulator` THROUGH them.
 * t_v90modemctor.cpp's teardown, and the same argument.
 */
static void
destroy_pair(unsigned int side, long trial)
{
	int s;

	if (side > 1)
		for (s = 0; s < 2; s++) {
			MODEM(s)->modulator = 0;
			MODEM(s)->demodulator = 0;
		}
	modem_dtor1(mobj[0]);
	ref_modem_dtor1(mobj[1]);
	diff_eq_int("nothing left allocated (%ld)", harness_alloc.live, 0,
		    trial);
	diff_eq_int("no bad free (%ld)", harness_alloc.bad_free, 0, trial);
}

/*
 * Everything both graphs need before an arm is chosen: a phase 2 description
 * the phase 4 reset can compand, a CP and an MP `infoToBits` can survive, the
 * RRN timer disarmed, and each side's own `reset` over its own objects.
 */
static void
setup_common(int trial)
{
	int s;

	for (s = 0; s < 2; s++) {
		MODEM(s)->phase2Info->pcmType = PCM_TYPE_MU_LAW;
		MODEM(s)->phase2Info->Uinfo = 0x40;
		MODEM(s)->phase2Info->rtd = 7;
		plausible_cp(&MODEM(s)->cp, trial);
		plausible_mp(&MODEM(s)->mp, trial);
		PARAMS(s)->DEBUG_DIGITAL_MODEM_INITIATE_RRN = 0;
		PARAMS(s)->DEBUG_DIGITAL_MODEM_INITIATE_RRN_TIME = 0;
	}

	MOD(0)->reset();
	ref_mod_reset(MOD(1));
	BTS(0)->reset(&MODEM(0)->mappingParamsAlt, PCM_TYPE_MU_LAW);
	ref_bts_reset(BTS(1), &MODEM(1)->mappingParamsAlt, 0);
}

/*
 * The DIL generator, poked identically on both sides so that the DIL_END arm
 * terminates the phase exactly `fire` symbols into the block.  `dilSymbol`
 * takes `segmentPos + 1` and restarts the segment when it reaches
 * `segmentLength[segmentIndex]`, which is what puts 6 in `eventCode`; a
 * `fire` of -1 sets a position that cannot be reached inside any block this
 * file drives.
 */
static void
setup_phase3(int fire)
{
	int s, k;

	for (s = 0; s < 2; s++) {
		V90Phase3Modulator *m = P3M(s);

		m->state = P3M_STATE_DIL_END;
		m->pcmType = PCM_TYPE_MU_LAW;
		m->symbolCount = 5;
		m->eventCode = 0;
		m->codeLevel = 1500;
		m->codeLevelAlt = -1500;
		m->idleLevel = 8;
		m->dilCount = 4;
		m->seq1Length = 4;
		m->seq2Length = 4;
		for (k = 0; k < 128; k++) {
			m->seq1[k] = (unsigned char)((k + 1) & 1);
			m->seq2[k] = (unsigned char)((k >> 1) & 1);
		}
		/*
		 * A segment LONGER than any block this file drives is what
		 * "does not terminate" means: with a twelve-symbol segment an
		 * eighty-symbol block wraps six times whatever the starting
		 * position, so the arm that keeps the state could not be
		 * reached at all.
		 */
		for (k = 0; k < 8; k++) {
			m->segmentLength[k] = (fire < 0)
			    ? 1024u + 6u * (unsigned)k
			    : 12u + 6u * (unsigned)k;
			m->segmentLevel[k] = (short)(64 + 128 * k);
		}
		for (k = 0; k < 256; k++)
			m->dilLevel[k] = (short)(1000 + 37 * k);
		m->seq1Index = 0;
		m->seq2Index = 0;
		m->dilIndex = 0;
		m->segmentIndex = 0;
		m->usingSegmentLevel = 0;
		m->segmentPos = (fire < 0)
		    ? 0u
		    : m->segmentLength[0] - 1u - (unsigned)fire;
	}
}

/*
 * The phase 4 modulator, through each side's own `reset`, then the terminal
 * count poked so that `case P4M_STATE_B1D` reaches 0x120 exactly `fire`
 * symbols into the block.  The converter is given a one-symbol block, which
 * is the size `initiateRRN` sets and the one the phase 4 pump really runs on.
 */
static void
setup_phase4(int state, int fire)
{
	int s;

	P4M(0)->reset(PCM_TYPE_MU_LAW, 0x40, (Phase4ModulatorState)state, 0, 7);
	ref_p4m_reset(P4M(1), (int)PCM_TYPE_MU_LAW, 0x40, state, 0, 7);

	BTS(0)->setSymbolsBlockSize(1);
	ref_bts_setblock(BTS(1), 1);

	for (s = 0; s < 2; s++) {
		BTS(s)->symbolsDone = 0;
		if (fire >= 0)
			P4M(s)->symbolCount = 0x120u - 1u - (unsigned)fire;
	}
}

/* ============================================ V90Modulator::progress, the arms */

/*
 * `p4state` of -1 leaves the phase 4 modulator alone, which is what the
 * phase 3 arm wants: `progress` resets it itself when the phase terminates,
 * and that reset is part of the claim.
 */
struct mtrial {
	const char	*name;
	int		state;		/* poked into V90Modulator::state  */
	unsigned int	n;		/* progress()'s fourth argument    */
	unsigned int	lvl;		/* the debug level                 */
	int		p3fire;		/* -1, or where phase 3 terminates */
	int		p4state;	/* -1, or the phase 4 state        */
	int		p4fire;		/* -1, or where B1D terminates     */
	int		rrn;		/* 0 off, 1 armed and due, 2 not   */
	unsigned int	flag;		/* sessionFlag at construction     */
	int		split;		/* drive the two bitsPerFrame apart*/
};

static const struct mtrial mtrial_v[] = {
	/* name              state  n  lvl p3  p4st p4f rrn flag split */
	{ "silence",             0, 80, 0, -1,  -1, -1, 0, 0, 0 },
	{ "silence, no symbols", 0,  0, 2, -1,  -1, -1, 0, 0, 0 },
	{ "silence, short",      0, 24, 1, -1,  -1, -1, 0, 1, 0 },
	{ "phase 3",             1, 80, 0, -1,  -1, -1, 0, 0, 0 },
	{ "phase 3 into 4",      1,  8, 2,  2,  -1, -1, 0, 0, 0 },
	{ "phase 3 ends first",  1,  8, 0,  0,  -1, -1, 0, 1, 0 },
	{ "phase 3, no symbols", 1,  0, 2, -1,  -1, -1, 0, 0, 0 },
	{ "phase 4",             2, 80, 0, -1, 0x00, -1, 0, 0, 0 },
	{ "phase 4 into data",   2,  8, 2, -1, 0x11,  3, 0, 0, 1 },
	{ "phase 4 into data 92",2,  8, 2, -1, 0x11,  3, 0, 1, 0 },
	{ "phase 4, short",      2, 24, 1, -1, 0x00, -1, 0, 1, 0 },
	{ "data phase",          3, 80, 0, -1,  -1, -1, 0, 0, 0 },
	{ "data phase, RRN off", 3, 80, 2, -1,  -1, -1, 2, 0, 0 },
	{ "data phase, RRN due", 3, 80, 2, -1,  -1, -1, 1, 0, 0 },
	{ "data phase, RRN 92",  3, 24, 0, -1,  -1, -1, 1, 1, 0 },
	/*
	 * THE COUNT EXACTLY ON THE DEADLINE.  The object's test is
	 * `symbolCount > TIME` and every other armed trial is far from the
	 * boundary, so `>` and `>=` would agree on all of them; this is the
	 * one input that separates the two.
	 */
	{ "data phase, RRN at it",3,40, 0, -1,  -1, -1, 3, 0, 0 },
	{ "data, no symbols",    3,  0, 1, -1,  -1, -1, 0, 0, 0 },
	{ "negative state",     -1, 80, 2, -1,  -1, -1, 0, 0, 0 },
	{ "very negative state",-99,  8, 0, -1,  -1, -1, 0, 1, 0 },
	{ "state four",          4, 80, 2, -1,  -1, -1, 0, 0, 0 },
	{ "state nine",          9, 24, 0, -1,  -1, -1, 0, 1, 0 }
};

#define NMTRIAL	((int)(sizeof(mtrial_v) / sizeof(mtrial_v[0])))

/* The `symbolCount` seed: never zero, so `symbolCount += n` is visible. */
#define SYMCOUNT_SEED	0x2000u
/* The `eventCode` seed, for the same reason: `progress` stores a zero. */
#define EVENTCODE_SEED	0x37u
/*
 * The demand handed to the data phase.  With 80 symbols to a block and 42
 * bits to a frame, `nofBitsForNextTime` asks for 14 frames -- 588 bits --
 * which is what a real session presents and which fits the 640-byte frame
 * buffer the scrambler writes into.
 */
#define DATA_BITS	588u

static void
seed_call_buffers(long trial, int same_nofbits)
{
	unsigned int i;

	for (i = 0; i < NBITS; i++)
		bits_seed[i] = (int)((0x9e37u * (unsigned)(trial + 1)
				      + 7u * i) & 1u);
	memcpy(bits_in[0], bits_seed, sizeof(bits_seed));
	memcpy(bits_in[1], bits_seed, sizeof(bits_seed));

	for (i = 0; i < NOUT; i++)
		out_seed[i] = (float)(-1000 - (int)i);
	memcpy(out_f[0], out_seed, sizeof(out_seed));
	memcpy(out_f[1], out_seed, sizeof(out_seed));

	/*
	 * THE REFERENCE PARAMETER IS SEEDED DIFFERENTLY ON THE TWO SIDES
	 * wherever it is an OUTPUT, because the default and negative-state
	 * arms do not write it and "the two agree" would then be satisfied by
	 * two untouched equal values.  On the data-phase arm it is an INPUT
	 * as well -- the scrambler is asked for that many bits -- so there it
	 * has to be the same number on both sides.  t_v90btsproc.cpp's rule.
	 */
	if (same_nofbits) {
		nb[0] = DATA_BITS;
		nb[1] = DATA_BITS;
	} else {
		nb[0] = 0xa5a5a5a5u;
		nb[1] = 0x5a5a5a5au;
	}
}

static void
check_call_buffers(unsigned int n, long tag)
{
	unsigned int i;

	diff_eq_int("the sample blocks agree (%ld)",
		    memcmp(out_f[0], out_f[1], NOUT * sizeof(float)) == 0, 1,
		    tag);
	for (i = n; i < NOUT; i++) {
		diff_eq_int("ours left sample %ld alone",
			    memcmp(&out_f[0][i], &out_seed[i], sizeof(float))
			    == 0, 1, (long)i);
		diff_eq_int("the blob left sample %ld alone",
			    memcmp(&out_f[1][i], &out_seed[i], sizeof(float))
			    == 0, 1, (long)i);
	}
	diff_eq_int("ours left the bits alone (%ld)",
		    memcmp(bits_in[0], bits_seed, sizeof(bits_seed)) == 0, 1,
		    tag);
	diff_eq_int("the blob left the bits alone (%ld)",
		    memcmp(bits_in[1], bits_seed, sizeof(bits_seed)) == 0, 1,
		    tag);
}

static int
run_mod_progress(void)
{
	int trial;
	int arm0 = 0, arm1 = 0, arm2 = 0, arm3 = 0, armneg = 0, armhigh = 0;
	int inner3 = 0, inner4 = 0, p4term = 0, rrnfired = 0, rrnheld = 0;
	int rrnedge = 0;
	int spoke = 0, silent = 0, zeron = 0, shortn = 0;

	diff_begin("V90Modulator::progress");

	for (trial = 0; trial < NMTRIAL; trial++) {
		const struct mtrial *t = &mtrial_v[trial];
		long tag = 7000 + trial;
		unsigned int bpf = t->split ? BPF_MAPPER : BPF_MAPPER;
		int s;

		build_pair(trial, 0, t->flag, bpf);
		setup_common(trial);

		if (t->state == 1)
			setup_phase3(t->p3fire);
		if (t->p4state >= 0)
			setup_phase4(t->p4state, t->p4fire);

		if (t->state == 3) {
			BTS(0)->setSymbolsBlockSize(NOFSYM);
			ref_bts_setblock(BTS(1), NOFSYM);
			for (s = 0; s < 2; s++)
				BTS(s)->symbolsDone = 0;
		}

		for (s = 0; s < 2; s++) {
			MOD(s)->state = t->state;
			MOD(s)->symbolCount = SYMCOUNT_SEED;
			MOD(s)->eventCode = EVENTCODE_SEED;
			if (t->rrn != 0) {
				PARAMS(s)->DEBUG_DIGITAL_MODEM_INITIATE_RRN = 1;
				PARAMS(s)->
				    DEBUG_DIGITAL_MODEM_INITIATE_RRN_TIME =
				    (t->rrn == 1)
				    ? (int)SYMCOUNT_SEED
				    : (t->rrn == 3)
				    ? (int)(SYMCOUNT_SEED + t->n)
				    : (int)(SYMCOUNT_SEED + t->n + 1u);
			}
			/*
			 * The trap: the converter's `bitsPerFrame` and the
			 * MAPPER's, which are the same number in any naturally
			 * reset object, driven apart before the trial that
			 * reaches the rate message.
			 */
			if (t->split) {
				MAPPER(s)->bitsPerFrame = BPF_MAPPER;
				BTS(s)->bitsPerFrame = BPF_BTS;
			}
		}

		if (t->split)
			diff_eq_int("the two bitsPerFrame really differ (%ld)",
				    MAPPER(1)->bitsPerFrame
				    != BTS(1)->bitsPerFrame, 1, tag);

		seed_call_buffers(trial, t->state == 3);
		compare_graph("before progress", 0, tag);

		set_level(t->lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		MOD(0)->progress(bits_in[0], nb[0], out_f[0], t->n);
		ref_mod_progress(MOD(1), bits_in[1], &nb[1], out_f[1], t->n);
		dsplib_debug_capture_on = 0;
		set_level(0);

		diff_eq_int("the transcripts match (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		compare_graph("after progress", 0, tag);
		check_call_buffers(t->n, tag);

		if (t->n == 0)
			zeron = 1;
		if (t->n != 0 && t->n != NOFSYM)
			shortn = 1;
		if (t->lvl > 1) {
			if (strlen(dsplib_debug_capture_text(1)) > 0)
				spoke = 1;
		} else {
			silent = 1;
		}

		/*
		 * THE REFERENCE PARAMETER, COMPARED ACROSS THE SIDES AND NOT
		 * ONLY AGAINST A CONSTANT.  Every arm below asserted what the
		 * BLOB answered and three of the four never looked at ours,
		 * so `nofBits = 0` in the phase 3 arm and
		 * `nofBits = nofBitsForNextTime()` at the data phase's entry
		 * could both be deleted from `src/` with every trial still
		 * green -- the mutation suite is what found that, which is
		 * what a mutation suite is for.  The four writing arms are
		 * states 0..3; the default arm writes nothing and each side
		 * has to keep its own distinct seed, which is asserted there.
		 */
		if (t->state >= 0 && t->state <= 3)
			diff_eq_int("the two answer the same demand (%ld)",
				    (long)nb[0], (long)nb[1], tag);

		if (t->state == 0) {
			unsigned int i;
			const short *sb = MOD(1)->symbolBuf;

			arm0 = 1;
			diff_eq_int("silence answers no bits (%ld)",
				    (long)nb[1], 0, tag);
			diff_eq_int("and ours does too (%ld)", (long)nb[0], 0,
				    tag);
			for (i = 0; i < t->n; i++)
				diff_eq_int("silence symbol %ld is zero",
					    (long)sb[i], 0, (long)i);
			/*
			 * The blob's untouched symbol buffer still holds the
			 * harness allocator's fill, so a body that cleared
			 * MORE than the block fails here rather than passing
			 * against a zeroed fixture.
			 */
			for (i = t->n; i < SYMBUF_SYMS; i++)
				diff_eq_int("symbol %ld past the block is "
					    "untouched",
					    (long)(unsigned short)sb[i],
					    (long)0xa5a5, (long)i);
			diff_eq_int("symbolCount took the block (%ld)",
				    (long)MOD(1)->symbolCount,
				    (long)(SYMCOUNT_SEED + t->n), tag);
			diff_eq_int("eventCode was cleared (%ld)",
				    (long)MOD(1)->eventCode, 0, tag);
		} else if (t->state == 1) {
			arm1 = 1;
			diff_eq_int("phase 3 answers no bits (%ld)",
				    (long)nb[1], 0, tag);
			if (t->p3fire >= 0 && (int)t->n > t->p3fire + 1) {
				inner3 = 1;
				inner4 = 1;
				diff_eq_int("phase 3 handed on to phase 4 "
					    "(%ld)", (long)MOD(1)->state, 2,
					    tag);
				/*
				 * THE SECOND INNER ARM, COUNTED RATHER THAN
				 * ASSUMED.  `progress` resets the phase 4
				 * modulator on the transition, so the symbols
				 * it generated afterwards are exactly the
				 * count left in the block.
				 */
				diff_eq_int("the phase 4 modulator ran for the"
					    " rest of the block (%ld)",
					    (long)P4M(1)->symbolCount,
					    (long)((int)t->n - t->p3fire - 1),
					    tag);
			} else if (t->n != 0) {
				inner3 = 1;
				diff_eq_int("phase 3 kept the state (%ld)",
					    (long)MOD(1)->state, 1, tag);
			}
		} else if (t->state == 2) {
			arm2 = 1;
			if (t->p4fire >= 0) {
				p4term = 1;
				diff_eq_int("phase 4 entered the data phase "
					    "(%ld)", (long)MOD(1)->state, 3,
					    tag);
				diff_eq_int("and said so (%ld)",
					    (long)MOD(1)->eventCode, 8, tag);
				diff_eq_int("and asked for bits (%ld)",
					    (long)(nb[1] != 0), 1, tag);
				diff_eq_int("symbolCount restarted (%ld)",
					    (long)MOD(1)->symbolCount, 0, tag);
			} else {
				diff_eq_int("no termination answers no bits "
					    "(%ld)", (long)nb[1], 0, tag);
				diff_eq_int("and keeps the state (%ld)",
					    (long)MOD(1)->state, 2, tag);
			}
		} else if (t->state == 3) {
			arm3 = 1;
			if (t->rrn == 1) {
				rrnfired = 1;
				diff_eq_int("the RRN timer fired (%ld)",
					    (long)MOD(1)->eventCode, 9, tag);
				diff_eq_int("and initiateRRN moved the state "
					    "(%ld)", (long)MOD(1)->state, 2,
					    tag);
			} else {
				if (t->rrn == 2)
					rrnheld = 1;
				if (t->rrn == 3)
					rrnedge = 1;
				diff_eq_int("the data phase kept the state "
					    "(%ld)", (long)MOD(1)->state, 3,
					    tag);
				diff_eq_int("symbolCount took the block (%ld)",
					    (long)MOD(1)->symbolCount,
					    (long)(SYMCOUNT_SEED + t->n), tag);
			}
		} else {
			if (t->state < 0)
				armneg = 1;
			else
				armhigh = 1;
			/*
			 * THE ARM THAT DOES NOT WRITE THE REFERENCE, so each
			 * side must still hold its own seed.
			 */
			diff_eq_int("ours left the reference alone (%ld)",
				    (long)nb[0], (long)0xa5a5a5a5u, tag);
			diff_eq_int("the blob left the reference alone (%ld)",
				    (long)nb[1], (long)0x5a5a5a5au, tag);
			diff_eq_int("the state is untouched (%ld)",
				    (long)MOD(1)->state, (long)t->state, tag);
			diff_eq_int("symbolCount took the block (%ld)",
				    (long)MOD(1)->symbolCount,
				    (long)(SYMCOUNT_SEED + t->n), tag);
			diff_eq_int("eventCode was cleared (%ld)",
				    (long)MOD(1)->eventCode, 0, tag);
		}

		destroy_pair(0, tag);
	}

	diff_eq_int("the silence arm was reached", arm0, 1, 0);
	diff_eq_int("the phase 3 arm was reached", arm1, 1, 0);
	diff_eq_int("the phase 4 arm was reached", arm2, 1, 0);
	diff_eq_int("the data phase arm was reached", arm3, 1, 0);
	diff_eq_int("a NEGATIVE state was tried", armneg, 1, 0);
	diff_eq_int("a state above three was tried", armhigh, 1, 0);
	diff_eq_int("the phase 3 inner arm was reached", inner3, 1, 0);
	diff_eq_int("the phase 4 inner arm was reached", inner4, 1, 0);
	diff_eq_int("phase 4 terminated inside a block", p4term, 1, 0);
	diff_eq_int("the RRN timer fired", rrnfired, 1, 0);
	diff_eq_int("the RRN timer was armed and not due", rrnheld, 1, 0);
	diff_eq_int("the RRN deadline was met exactly", rrnedge, 1, 0);
	diff_eq_int("a zero-length block was tried", zeron, 1, 0);
	diff_eq_int("a block shorter than nofSymbols was tried", shortn, 1, 0);
	diff_eq_int("the gate was tried open", spoke, 1, 0);
	diff_eq_int("the gate was tried shut", silent, 1, 0);
	return diff_end();
}

/* ================================================ the rate message, on its own */

/*
 * THE ONE TRIAL THAT SEPARATES THE TWO `bitsPerFrame` FIELDS.  It is here
 * rather than folded into the sweep because the claim is about the BLOB's
 * transcript and not about the two sides agreeing: with both fields holding
 * the same number -- which is what any reset leaves -- a body reading either
 * one passes, so the test has to read the number back out of the message.
 */
/*
 * `mapper` is the field the object reads, `conv` the one it does not, `want`
 * the message the first gives and `wrong` the message the second would.  The
 * second row's rate does NOT divide exactly -- 8000 * 44 / 6 is 58666.67 --
 * so `truncated` is the message a body without the `0.5f +` would print, and
 * it is the only input in this file that separates a round from a truncation.
 */
struct rate_case {
	unsigned int	mapper, conv;
	const char	*want;
	const char	*wrong;
	const char	*truncated;
};

static const struct rate_case rate_v[] = {
	{ 42u, 48u, "Rate = 56000 [bps]", "Rate = 64000", "Rate = 55999" },
	/*
	 * THE CONVERTER'S FIGURE IS TWO OF THE MAPPER'S FRAMES AND THAT IS
	 * NOT DECORATION.  The converter's `bitsPerFrame` is what the phase 4
	 * pump's bit demand is measured in and the mapper's is what a frame
	 * costs, so a demand SHORTER than a frame leaves the mapper with a
	 * part-filled buffer, the drain underflows, and
	 * `generateDataSymbolBeforeFPE`'s `short` comes back unwritten on both
	 * sides -- deviation D661, two stack frames rather than two
	 * implementations.  88 is two 44-bit frames, so every fill completes
	 * at least one.
	 */
	{ 44u, 88u, "Rate = 58667 [bps]", "Rate = 117333", "Rate = 58666" }
};

#define NRATE	((int)(sizeof(rate_v) / sizeof(rate_v[0])))

static int
run_bpf_split(void)
{
	int trial;

	diff_begin("V90Modulator::progress -- the data phase rate");

	for (trial = 0; trial < NRATE; trial++) {
		const struct rate_case *r = &rate_v[trial];
		long tag = 7400 + trial;
		const char *text;
		int s;

		build_pair(400 + trial, 0, 0, BPF_MAPPER);
		setup_common(40 + trial);
		setup_phase4(0x11, 3);

		for (s = 0; s < 2; s++) {
			MOD(s)->state = 2;
			MOD(s)->symbolCount = SYMCOUNT_SEED;
			MOD(s)->eventCode = EVENTCODE_SEED;
			MAPPER(s)->bitsPerFrame = r->mapper;
			BTS(s)->bitsPerFrame = r->conv;
		}

		diff_eq_int("the mapper's bitsPerFrame is not the converter's"
			    " (%ld)",
			    MAPPER(1)->bitsPerFrame != BTS(1)->bitsPerFrame, 1,
			    tag);
		diff_eq_int("the mapper holds what it was given (%ld)",
			    (long)MAPPER(1)->bitsPerFrame, (long)r->mapper,
			    tag);
		diff_eq_int("the converter holds what it was given (%ld)",
			    (long)BTS(1)->bitsPerFrame, (long)r->conv, tag);

		seed_call_buffers(400 + trial, 0);
		compare_graph("before the rate message", 0, tag);

		set_level(2);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		MOD(0)->progress(bits_in[0], nb[0], out_f[0], 8);
		ref_mod_progress(MOD(1), bits_in[1], &nb[1], out_f[1], 8);
		dsplib_debug_capture_on = 0;
		set_level(0);

		text = dsplib_debug_capture_text(1);
		diff_eq_int("the blob said something (%ld)",
			    (long)(strlen(text) > 0), 1, tag);
		diff_eq_int("the data phase was entered (%ld)",
			    (long)MOD(1)->state, 3, tag);
		diff_eq_int("the rate came from the mapper's bitsPerFrame "
			    "(%ld)", (long)(strstr(text, r->want) != 0), 1,
			    tag);
		diff_eq_int("and not from the converter's (%ld)",
			    (long)(strstr(text, r->wrong) != 0), 0, tag);
		diff_eq_int("and it is a round and not a truncation (%ld)",
			    (long)(strstr(text, r->truncated) != 0), 0, tag);
		diff_eq_int("the transcripts match (%ld)",
			    strcmp(dsplib_debug_capture_text(0), text) == 0, 1,
			    tag);
		compare_graph("after the rate message", 0, tag);

		destroy_pair(0, tag);
	}

	return diff_end();
}

/* =========================================================== initiateRRN */

struct rtrial {
	const char	*name;
	int		state;		/* poked into V90Modulator::state  */
	unsigned int	done;		/* the converter's symbolsDone     */
	unsigned int	lvl;
	unsigned int	flag;		/* sessionFlag: the CP/MP fork     */
};

static const struct rtrial rtrial_v[] = {
	{ "not approved, silent",   0, 0, 0, 0 },
	{ "not approved, loud",     2, 0, 2, 0 },
	{ "not approved, negative",-1, 0, 2, 1 },
	{ "not approved, high",     7, 0, 0, 1 },
	{ "approved, bits owed",    3, 0, 0, 0 },
	{ "approved, bits owed 92", 3, 0, 2, 1 },
	{ "approved, nothing owed", 3, 4, 0, 0 },
	{ "approved, nothing 92",   3, 4, 2, 1 }
};

#define NRTRIAL	((int)(sizeof(rtrial_v) / sizeof(rtrial_v[0])))

/*
 * THE TWO MESSAGES ARE COMPARED AS TRANSCRIPTS AND NOT SEARCHED FOR.  Both go
 * through `edprintf`, whose capture stays ENCODED -- `dsplib_encode_plain` is
 * ours alone (D40) -- so no substring of the source literal appears in the
 * text.  What the run asserts instead is a property of the BLOB: its
 * transcript for the RdModulation arm differs from its transcript for the
 * DataToRdModulation one, and neither is empty.  Swap the two literals in the
 * source and the per-trial transcript comparison is what moves.
 */
#define RRN_TEXT	512

static char text_owed[RRN_TEXT];
static char text_nothing[RRN_TEXT];

static void
keep_text(char *dst, const char *src)
{
	unsigned n = (unsigned)strlen(src);

	if (n >= RRN_TEXT)
		n = RRN_TEXT - 1;
	memcpy(dst, src, n);
	dst[n] = '\0';
}

static int
run_initiaterrn(void)
{
	int trial;
	int approved = 0, refused = 0, owed = 0, nothing = 0;
	int v90arm = 0, v92arm = 0, spoke = 0, silent = 0;

	diff_begin("V90Modulator::initiateRRN");

	for (trial = 0; trial < NRTRIAL; trial++) {
		const struct rtrial *t = &rtrial_v[trial];
		long tag = 7600 + trial;
		int ra, rb, s;

		build_pair(600 + trial, 0, t->flag, BPF_MAPPER);
		setup_common(trial + 3);
		setup_phase4(0x00, -1);

		for (s = 0; s < 2; s++) {
			MOD(s)->state = t->state;
			MOD(s)->symbolCount = SYMCOUNT_SEED;
			MOD(s)->eventCode = EVENTCODE_SEED;
			BTS(s)->symbolsDone = t->done;
		}

		compare_graph("before initiateRRN", 0, tag);

		set_level(t->lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		ra = MOD(0)->initiateRRN();
		rb = ref_mod_initiaterrn(MOD(1));
		dsplib_debug_capture_on = 0;
		set_level(0);

		diff_eq_int("the answers match (%ld)", (long)ra, (long)rb, tag);
		diff_eq_int("the transcripts match (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		compare_graph("after initiateRRN", 0, tag);

		if (t->lvl > 1)
			spoke = 1;
		else
			silent = 1;

		if (t->state != 3) {
			refused = 1;
			diff_eq_int("a state that is not 3 is refused (%ld)",
				    (long)rb, -1, tag);
			diff_eq_int("and nothing moved (%ld)",
				    (long)MOD(1)->state, (long)t->state, tag);
			diff_eq_int("and symbolCount is untouched (%ld)",
				    (long)MOD(1)->symbolCount,
				    (long)SYMCOUNT_SEED, tag);
			diff_eq_int("and the CP tag is untouched (%ld)",
				    (long)MODEM(1)->cp.byte_13, 1, tag);
			diff_eq_int("and CPack is untouched (%ld)",
				    (long)MODEM(1)->mp.CPack, 1, tag);
			if (t->lvl <= 1)
				diff_eq_int("below the gate the blob was "
					    "silent (%ld)",
					    (long)strlen(
						dsplib_debug_capture_text(1)),
					    0, tag);
			destroy_pair(0, tag);
			continue;
		}

		approved = 1;
		diff_eq_int("the data phase is approved (%ld)", (long)rb, 0,
			    tag);
		diff_eq_int("the state went to phase 4 (%ld)",
			    (long)MOD(1)->state, 2, tag);
		diff_eq_int("symbolCount restarted (%ld)",
			    (long)MOD(1)->symbolCount, 0, tag);
		diff_eq_int("eventCode was cleared (%ld)",
			    (long)MOD(1)->eventCode, 0, tag);
		diff_eq_int("the block size went to one (%ld)",
			    (long)BTS(1)->symbolsBlockSize, 1, tag);

		/*
		 * WHICH PHASE 4 STATE, and it is the answer of
		 * `nofBitsForNextTime` that chooses.  Both messages are
		 * UNGATED `edprintf`, so they appear at every level.
		 */
		if (t->done == 0) {
			owed = 1;
			diff_eq_int("bits still owed enters RdModulation "
				    "(%ld)", (long)P4M(1)->state, 0x15, tag);
			keep_text(text_owed, dsplib_debug_capture_text(1));
		} else {
			nothing = 1;
			diff_eq_int("nothing owed enters DataToRdModulation "
				    "(%ld)", (long)P4M(1)->state, 0x14, tag);
			keep_text(text_nothing, dsplib_debug_capture_text(1));
		}

		/* The session flag's fork: two fields in two classes. */
		if (t->flag != 0) {
			v92arm = 1;
			diff_eq_int("V.92 clears the CP tag (%ld)",
				    (long)MODEM(1)->cp.byte_13, 0, tag);
			diff_eq_int("and leaves CPack alone (%ld)",
				    (long)MODEM(1)->mp.CPack, 1, tag);
		} else {
			v90arm = 1;
			diff_eq_int("V.90 clears CPack (%ld)",
				    (long)MODEM(1)->mp.CPack, 0, tag);
			diff_eq_int("and leaves the CP tag alone (%ld)",
				    (long)MODEM(1)->cp.byte_13, 1, tag);
		}

		destroy_pair(0, tag);
	}

	diff_eq_int("a refusal was reached", refused, 1, 0);
	diff_eq_int("an approval was reached", approved, 1, 0);
	diff_eq_int("bits still owed was reached", owed, 1, 0);
	diff_eq_int("nothing owed was reached", nothing, 1, 0);
	diff_eq_int("the blob said something on the approved path",
		    (int)(strlen(text_owed) > 0), 1, 0);
	diff_eq_int("the two phase 4 states carry two different messages",
		    strcmp(text_owed, text_nothing) != 0, 1, 0);
	diff_eq_int("the V.90 arm was reached", v90arm, 1, 0);
	diff_eq_int("the V.92 arm was reached", v92arm, 1, 0);
	diff_eq_int("the gate was tried open", spoke, 1, 0);
	diff_eq_int("the gate was tried shut", silent, 1, 0);
	return diff_end();
}

/* ============================= V90BitsToSymbol::process, driven directly */

/*
 * The FOUR outcomes, counted from what the BLOB answered rather than
 * predicted: status 1 (`symbolsBlockSize` zero), status 3 (fewer symbols
 * ready than a block, which also EMPTIES the buffer), status 2 (`symbolsDone`
 * past the buffer's capacity -- a POKED state, finding 7430, because nothing
 * in a properly constructed object can raise it without the mapper having
 * already written past the allocation), status 0 with nothing left over and
 * status 0 with a leftover run the shift loop actually moves.
 *
 * The capacity is shrunk rather than `symbolsDone` grown, so the mapper's
 * write still lands well inside the real allocation on both sides.
 */
struct btrial {
	unsigned int	block;
	unsigned int	done;
	unsigned int	cap;		/* the object's own nofSymbols     */
	unsigned int	nofBits;
	unsigned char	pending;
	unsigned int	lvl;
};

static const struct btrial btrial_v[] = {
	{  0u,  0u,  400u,  84u, 1, 0 },	/* SIZE_NOT_SET             */
	{  0u,  3u,  400u, 168u, 0, 2 },	/* SIZE_NOT_SET, loud       */
	/*
	 * THE LEVEL ON THE GATE.  `dsplibs_debug_level > 1` and `> 0` differ
	 * at exactly one value, so a sweep that only uses 0 and 2 cannot tell
	 * the object's gate from a looser one -- and it has to be a trial
	 * whose status is NON-ZERO, because a silent path is silent at every
	 * level.  t_v90btsproc.cpp's argument.
	 */
	{  0u,  5u,  400u,  84u, 1, 1 },	/* SIZE_NOT_SET, at the gate*/
	{ 40u,  1u,  400u,  42u, 1, 1 },	/* underflow, at the gate   */
	{ 40u,  0u,  400u,  84u, 1, 0 },	/* short fill: underflow    */
	{ 40u,  2u,  400u,  42u, 0, 2 },	/* underflow, loud          */
	{  4u,  0u,  400u, 252u, 1, 0 },	/* a block and a remainder  */
	{  4u,  1u,  400u, 252u, 0, 1 },	/* ... at the gate          */
	{  6u,  0u,  400u,  84u, 1, 0 },	/* about one block          */
	{  6u,  6u,  400u,   0u, 0, 0 },	/* nothing to fill          */
	{  4u, 18u,   20u, 168u, 1, 2 },	/* capacity: BUFFER_OVERFLOW*/
	{  4u, 18u,   20u, 168u, 0, 0 },	/* ... quietly              */
	/*
	 * THE CAPACITY BOUNDARY.  The object's test is `symbolsDone >
	 * nofSymbols`, and every other capacity trial is far enough past the
	 * limit that `>` and `>=` agree on all of them.  Landing EXACTLY on
	 * `nofSymbols` is the only input that separates the two, and how many
	 * symbols the mapper returns for a given bit count is not something
	 * this fixture can predict -- `V90Mapper::process`'s priming
	 * countdown suppresses whole frames -- so the sweep approaches the
	 * limit from several starting points and one of them lands on it.
	 * The mutation `the capacity test is not strict` in
	 * test/mutations/v90modprogbts.json is what says whether it did.
	 */
	{  6u, 14u,   20u,  42u, 1, 1 },	/* capacity boundary, -6    */
	{  6u, 18u,   20u,  42u, 0, 0 },	/* ... exactly ON it        */
	{  6u, 20u,   20u,  42u, 1, 0 },	/* ... -0                   */
	{ 12u,  0u,  400u, 504u, 1, 1 },	/* a long fill              */
	{  1u,  0u,  400u,  42u, 0, 0 }		/* the phase 4 block size   */
};

#define NBTRIAL	((int)(sizeof(btrial_v) / sizeof(btrial_v[0])))

#define BOUT	512

static short bout[2][BOUT];
static short bout_seed[BOUT];
static unsigned char bbits[2][NBITS];
static unsigned char bbits_seed[NBITS];

static int
run_bts_process(void)
{
	int trial;
	int st0 = 0, st1 = 0, st2 = 0, st3 = 0, kept_run = 0, kept_none = 0;
	int pend = 0, nopend = 0, spoke = 0, silent = 0, midgate = 0;

	diff_begin("V90BitsToSymbol::process(unsigned char *, unsigned int &, "
		   "short *)");

	for (trial = 0; trial < NBTRIAL; trial++) {
		const struct btrial *t = &btrial_v[trial];
		long tag = 7800 + trial;
		unsigned int ra, rb, i, done_before;
		unsigned int nofbits[2];

		build_pair(800 + trial, 0, 0, BPF_MAPPER);
		setup_common(trial + 1);

		for (i = 0; i < BOUT; i++)
			bout_seed[i] = (short)(0x7000 - (int)i * 11);
		memcpy(bout[0], bout_seed, sizeof(bout_seed));
		memcpy(bout[1], bout_seed, sizeof(bout_seed));
		for (i = 0; i < NBITS; i++)
			bbits_seed[i] = (unsigned char)((0x5au + 3u * i
							 + (unsigned)trial)
							& 1u);
		memcpy(bbits[0], bbits_seed, sizeof(bbits_seed));
		memcpy(bbits[1], bbits_seed, sizeof(bbits_seed));

		for (i = 0; i < 2; i++) {
			BTS(i)->symbolsBlockSize = t->block;
			BTS(i)->symbolsDone = t->done;
			BTS(i)->nofSymbols = t->cap;
			BTS(i)->extraSymbolsPending = t->pending;
		}
		done_before = t->done;

		/* The reference is an INPUT here and has to be the same. */
		nofbits[0] = t->nofBits;
		nofbits[1] = t->nofBits;

		compare_graph("before process", 0, tag);

		set_level(t->lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		ra = BTS(0)->process(bbits[0], nofbits[0], bout[0]);
		rb = ref_bts_process3(BTS(1), bbits[1], &nofbits[1], bout[1]);
		dsplib_debug_capture_on = 0;
		set_level(0);

		diff_eq_int("the statuses match (%ld)", (long)ra, (long)rb,
			    tag);
		diff_eq_int("the demands match (%ld)", (long)nofbits[0],
			    (long)nofbits[1], tag);
		diff_eq_int("the transcripts match (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		diff_eq_int("the output blocks agree (%ld)",
			    memcmp(bout[0], bout[1], sizeof(bout_seed)) == 0,
			    1, tag);
		diff_eq_int("ours left the bits alone (%ld)",
			    memcmp(bbits[0], bbits_seed, sizeof(bbits_seed))
			    == 0, 1, tag);
		diff_eq_int("the blob left the bits alone (%ld)",
			    memcmp(bbits[1], bbits_seed, sizeof(bbits_seed))
			    == 0, 1, tag);
		compare_graph("after process", 0, tag);

		if (t->pending)
			pend = 1;
		else
			nopend = 1;
		diff_eq_int("the pending flag is clear afterwards (%ld)",
			    (long)BTS(1)->extraSymbolsPending, 0, tag);

		if (t->lvl > 1) {
			if (rb != 0)
				spoke = 1;
		} else {
			silent = 1;
			diff_eq_int("below the gate the blob was silent (%ld)",
				    (long)strlen(dsplib_debug_capture_text(1)),
				    0, tag);
			if (t->lvl == 1u && rb != 0)
				midgate = 1;
		}

		if (rb == 1) {
			st1 = 1;
			diff_eq_int("SIZE_NOT_SET leaves the demand (%ld)",
				    (long)nofbits[1], (long)t->nofBits, tag);
			diff_eq_int("and copies nothing out (%ld)",
				    memcmp(bout[1], bout_seed,
					   sizeof(bout_seed)) == 0, 1, tag);
			diff_eq_int("and does not move symbolsDone (%ld)",
				    (long)BTS(1)->symbolsDone,
				    (long)done_before, tag);
		} else if (rb == 3) {
			st3 = 1;
			diff_eq_int("BUFFER_UNDERFLOW empties the buffer "
				    "(%ld)", (long)BTS(1)->symbolsDone, 0,
				    tag);
		} else if (rb == 2) {
			st2 = 1;
			diff_eq_int("BUFFER_OVERFLOW clamps to the block "
				    "(%ld)", (long)BTS(1)->symbolsDone, 0,
				    tag);
		} else {
			st0 = 1;
			if (BTS(1)->symbolsDone != 0)
				kept_run = 1;
			else
				kept_none = 1;
		}

		destroy_pair(0, tag);
	}

	diff_eq_int("the quiet status was reached", st0, 1, 0);
	diff_eq_int("SIZE_NOT_SET was reached", st1, 1, 0);
	diff_eq_int("BUFFER_OVERFLOW was reached", st2, 1, 0);
	diff_eq_int("BUFFER_UNDERFLOW was reached", st3, 1, 0);
	diff_eq_int("a leftover run was kept", kept_run, 1, 0);
	diff_eq_int("an exactly-emptied block was tried", kept_none, 1, 0);
	diff_eq_int("a set pending flag was tried", pend, 1, 0);
	diff_eq_int("a clear pending flag was tried", nopend, 1, 0);
	diff_eq_int("the gate was tried open", spoke, 1, 0);
	diff_eq_int("the gate was tried shut", silent, 1, 0);
	diff_eq_int("the level ON the gate was tried", midgate, 1, 0);
	return diff_end();
}

/* ================================== V90Phase4Modulator::generateSymbol */

/*
 * BOTH `sessionFlag` ARMS ON STATES WHERE THE TWO PUMPS DIFFER.
 * `generateV90Symbol`'s jump table runs 0x00..0x1b and `generateV92Symbol`'s
 * 0x00..0x1e, so 0x1c dispatches under V.92 and takes the "Illegal state"
 * default under V.90; and only the V.92 pump sets `word_000c` to 4 on TRN2d's
 * exits.  A fork that always chose one pump therefore fails on the object,
 * not merely on the transcript.
 */
static const int p4state_v[] = { 0x00, 0x03, 0x11, 0x1c };

#define NP4STATE ((int)(sizeof(p4state_v) / sizeof(p4state_v[0])))

static int
run_p4_gensym(void)
{
	int flagi, sti, lvli, s;
	int v90 = 0, v92 = 0, differed = 0, spoke = 0, silent = 0;

	diff_begin("V90Phase4Modulator::generateSymbol");

	for (flagi = 0; flagi < 2; flagi++)
		for (sti = 0; sti < NP4STATE; sti++)
			for (lvli = 0; lvli < 2; lvli++) {
				unsigned int flag = (unsigned int)flagi;
				int st = p4state_v[sti];
				unsigned int lvl = lvli ? 2u : 0u;
				long tag = 8000 + flagi * 100 + sti * 10 + lvli;
				int ra, rb;

				build_pair(900 + tag, 0, flag, BPF_MAPPER);
				setup_common(sti + 2);
				setup_phase4(st, -1);

				/*
				 * A SYMBOL HAS TO BE READY, and that is
				 * deviation D661 rather than a convenience.
				 * `generateDataSymbolBeforeFPE` -- the arm
				 * state 0x1c dispatches to under V.92 -- reads
				 * a `short` the drain leaves UNWRITTEN when
				 * fewer symbols are ready than a block, and
				 * the object has the same hole, so the two
				 * sides would be comparing two stack frames
				 * rather than two implementations.  With
				 * `symbolsDone` at 1 the drain empties the
				 * buffer and the demand that follows is
				 * non-zero, which is the arm that moves the
				 * state; at 4 it is zero and the state stays.
				 */
				for (s = 0; s < 2; s++)
					BTS(s)->symbolsDone = lvli ? 1u : 4u;

				diff_eq_int("the session flag reached the "
					    "modulator (%ld)",
					    (long)P4M(1)->sessionFlag,
					    (long)flag, tag);

				compare_graph("before generateSymbol", 0, tag);

				set_level(lvl);
				dsplib_debug_capture_reset();
				dsplib_debug_capture_on = 1;
				ra = P4M(0)->generateSymbol();
				rb = ref_p4m_gensym(P4M(1));
				dsplib_debug_capture_on = 0;
				set_level(0);

				diff_eq_int("the symbols match (%ld)",
					    (long)ra, (long)rb, tag);
				diff_eq_int("the transcripts match (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag);
				compare_graph("after generateSymbol", 0, tag);

				if (flag)
					v92 = 1;
				else
					v90 = 1;
				if (lvl > 1) {
					if (strlen(dsplib_debug_capture_text(1))
					    > 0)
						spoke = 1;
				} else {
					silent = 1;
				}
				/*
				 * 0x1c is the discriminator: the V.90 pump has
				 * no such state and answers zero from its
				 * default arm.
				 */
				if (st == 0x1c) {
					if (flag == 0)
						diff_eq_int("V.90 has no state "
							    "0x1c (%ld)",
							    (long)rb, 0, tag);
					else
						differed = 1;
				}

				destroy_pair(0, tag);
			}

	diff_eq_int("the V.90 pump was chosen", v90, 1, 0);
	diff_eq_int("the V.92 pump was chosen", v92, 1, 0);
	diff_eq_int("a state only one pump has was driven", differed, 1, 0);
	diff_eq_int("the gate was tried open", spoke, 1, 0);
	diff_eq_int("the gate was tried shut", silent, 1, 0);
	return diff_end();
}

/* ============================================ V90Modem::progress, the sides */

/*
 * THE SIDE SWITCH AND THE FORWARDING.  Side 0 must arrive at
 * `V90Modulator::progress` with all four arguments unchanged, side >= 2 must
 * reach NEITHER pointer, and the block length is varied so that a body
 * forwarding the modulator's `nofSymbols` MEMBER rather than the `n` ARGUMENT
 * -- the two are 80 and 24 here -- fails.
 */
struct dtrial {
	unsigned int	side;
	int		state;
	unsigned int	n;
	unsigned int	lvl;
};

static const struct dtrial dtrial_v[] = {
	{ 0, 0, 80, 0 },
	{ 0, 0, 24, 2 },		/* n != nofSymbols                  */
	{ 0, 3, 24, 0 },		/* the data phase, through the modem*/
	{ 0, 3, 80, 2 },
	{ 0, 2,  0, 1 },		/* a zero-length block              */
	{ 2, 0, 80, 2 },		/* Illegal modemSide, loud          */
	{ 2, 0, 24, 0 },		/* ... and silent                   */
	{ 5, 0,  0, 2 }
};

#define NDTRIAL	((int)(sizeof(dtrial_v) / sizeof(dtrial_v[0])))

static int
run_modem_progress(void)
{
	int trial;
	int digital = 0, illegal = 0, spoke = 0, silent = 0, varied = 0;

	diff_begin("V90Modem::progress");

	for (trial = 0; trial < NDTRIAL; trial++) {
		const struct dtrial *t = &dtrial_v[trial];
		long tag = 8400 + trial;
		int s;

		build_pair(1000 + trial, t->side, 0, BPF_MAPPER);

		if (t->side == 0) {
			setup_common(trial + 4);
			if (t->state == 2)
				setup_phase4(0x00, -1);
			if (t->state == 3) {
				BTS(0)->setSymbolsBlockSize(NOFSYM);
				ref_bts_setblock(BTS(1), NOFSYM);
				for (s = 0; s < 2; s++)
					BTS(s)->symbolsDone = 0;
			}
			for (s = 0; s < 2; s++) {
				MOD(s)->state = t->state;
				MOD(s)->symbolCount = SYMCOUNT_SEED;
				MOD(s)->eventCode = EVENTCODE_SEED;
			}
		}

		seed_call_buffers(1000 + trial, t->side == 0 && t->state == 3);
		compare_graph("before V90Modem::progress", t->side, tag);

		set_level(t->lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		MODEM(0)->progress(bits_in[0], nb[0], out_f[0], t->n);
		ref_modem_progress(mobj[1], bits_in[1], &nb[1], out_f[1],
				   t->n);
		dsplib_debug_capture_on = 0;
		set_level(0);

		diff_eq_int("the transcripts match (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		compare_graph("after V90Modem::progress", t->side, tag);
		check_call_buffers(t->side == 0 ? t->n : 0, tag);

		if (t->lvl > 1)
			spoke = 1;
		else
			silent = 1;

		if (t->side == 0) {
			digital = 1;
			if (t->n != NOFSYM)
				varied = 1;
			/*
			 * THE FOURTH ARGUMENT ARRIVED AS ITSELF.  `progress`
			 * adds it to `symbolCount`, so a body forwarding the
			 * member instead would add 80 where 24 was asked for.
			 */
			diff_eq_int("the block length arrived (%ld)",
				    (long)MOD(1)->symbolCount,
				    (long)(SYMCOUNT_SEED + t->n), tag);
			/* And the third: the samples past n are untouched. */
			diff_eq_int("the sample pointer arrived (%ld)",
				    memcmp(&out_f[1][t->n], &out_seed[t->n],
					   (NOUT - t->n) * sizeof(float)) == 0,
				    1, tag);
		} else {
			illegal = 1;
			/*
			 * NEITHER CALLEE WAS REACHED: the reference parameter
			 * is untouched and each side still holds its own seed,
			 * and no sample was written.
			 */
			diff_eq_int("ours left the reference alone (%ld)",
				    (long)nb[0], (long)0xa5a5a5a5u, tag);
			diff_eq_int("the blob left the reference alone (%ld)",
				    (long)nb[1], (long)0x5a5a5a5au, tag);
			diff_eq_int("no sample was written (%ld)",
				    memcmp(out_f[1], out_seed,
					   sizeof(out_seed)) == 0, 1, tag);
			if (t->lvl > 1)
				diff_eq_int("and it said so (%ld)",
					    (long)(strstr(
						dsplib_debug_capture_text(1),
						"Illegal modemSide") != 0), 1,
					    tag);
			else
				diff_eq_int("and below the gate it was silent "
					    "(%ld)",
					    (long)strlen(
						dsplib_debug_capture_text(1)),
					    0, tag);
		}

		destroy_pair(t->side, tag);
	}

	diff_eq_int("the digital arm was reached", digital, 1, 0);
	diff_eq_int("the illegal arm was reached", illegal, 1, 0);
	diff_eq_int("a block length other than nofSymbols was forwarded",
		    varied, 1, 0);
	diff_eq_int("the gate was tried open", spoke, 1, 0);
	diff_eq_int("the gate was tried shut", silent, 1, 0);
	return diff_end();
}

/* ============================================ V90Modem::progress, the analog arm */

/*
 * SIDE 1 FORWARDS TO `V90Demodulator::progress`, which is written and is
 * differentially tested by t_v90demprog.  It is driven here only to say that
 * the ARM reaches it with the four arguments unchanged: the samples go in,
 * the bits come out, and the demodulator's own claims are that file's.
 */
static int
run_analog_arm(void)
{
	int trial;
	int reached = 0, varied = 0;
	static const unsigned int nv[] = { 8u, 40u, 0u };

	diff_begin("V90Modem::progress -- the analog arm");

	for (trial = 0; trial < 3; trial++) {
		long tag = 8600 + trial;
		unsigned int n = nv[trial];
		unsigned int i;

		build_pair(1200 + trial, 1, 0, BPF_MAPPER);

		MODEM(0)->demodulator->reset(0);
		ref_dem_reset(MODEM(1)->demodulator, 0);

		/*
		 * The samples are the INPUT here and the bits the OUTPUT, so
		 * the two buffers change roles: `bits_in` is written by the
		 * callee and is not required to come back unchanged.
		 */
		for (i = 0; i < NOUT; i++)
			out_seed[i] = (float)((int)(i % 37) * 211 - 3800);
		memcpy(out_f[0], out_seed, sizeof(out_seed));
		memcpy(out_f[1], out_seed, sizeof(out_seed));
		memset(bits_in[0], 0x5a, sizeof(bits_in[0]));
		memset(bits_in[1], 0x5a, sizeof(bits_in[1]));
		nb[0] = 0xa5a5a5a5u;
		nb[1] = 0x5a5a5a5au;

		compare_graph("before the analog arm", 1, tag);

		set_level(0);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		MODEM(0)->progress(bits_in[0], nb[0], out_f[0], n);
		ref_modem_progress(mobj[1], bits_in[1], &nb[1], out_f[1], n);
		dsplib_debug_capture_on = 0;

		diff_eq_int("the transcripts match (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		diff_eq_int("the bit blocks agree (%ld)",
			    memcmp(bits_in[0], bits_in[1],
				   sizeof(bits_in[0])) == 0, 1, tag);
		diff_eq_int("the counts agree (%ld)", (long)nb[0], (long)nb[1],
			    tag);
		diff_eq_int("the sample blocks agree (%ld)",
			    memcmp(out_f[0], out_f[1], sizeof(out_seed)) == 0,
			    1, tag);
		compare_graph("after the analog arm", 1, tag);

		reached = 1;
		if (n != 0)
			varied = 1;

		destroy_pair(1, tag);
	}

	diff_eq_int("the analog arm was reached", reached, 1, 0);
	diff_eq_int("a non-empty block was forwarded", varied, 1, 0);
	for (trial = 0; trial < NDEMOD_ALIAS; trial++) {
		printf("    excluded: V90Demodulator +0x%03x\n",
		       demod_alias[trial]);
		diff_eq_int("the excluded word really does hold two different"
			    " addresses (%ld)", demod_alias_fired[trial], 1,
			    (long)trial);
		diff_eq_int("and they are static addresses, not heap ones"
			    " (%ld)", demod_alias_wrong[trial], 0,
			    (long)trial);
	}
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_mod_progress();
	rc |= run_bpf_split();
	rc |= run_initiaterrn();
	rc |= run_bts_process();
	rc |= run_p4_gensym();
	rc |= run_modem_progress();
	rc |= run_analog_arm();

	return rc;
}

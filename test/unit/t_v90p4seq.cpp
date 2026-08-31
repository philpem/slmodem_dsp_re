/*
 * t_v90p4seq.cpp -- differential test of the six callerless phase 4 symbol
 * sources:
 *
 *     V90Phase4Modulator::generateMP()      .text+0x2dbd0   167 bytes
 *     V90Phase4Modulator::generateCPd()     .text+0x2e540   167 bytes
 *     V90Phase4Modulator::generateSUVd()    .text+0x2e5f0   167 bytes
 *
 * and, since the VPcmV34Main leaf pass, the training half of the family:
 *
 *     V90Phase4Modulator::generateB1d()     .text+0x2d9f0   149 bytes
 *     V90Phase4Modulator::generateTRN2d()   .text+0x2da90   149 bytes
 *     V90Phase4Modulator::generateEd()      .text+0x2db30   149 bytes
 *
 * The training three scramble GENERATED bits -- ones for B1d/TRN2d, zeros
 * for Ed -- so they read neither (pointer, length) field pair; B1d and
 * TRN2d are byte-identical siblings the way CPd and SUVd are, and
 * `run_discriminate_train` holds them to it.
 *
 * `readelf -r ref/slmodemd/dsplibs.o` finds ZERO relocations of any type
 * naming any of the three, against 43 naming
 * `V90BitsToSymbol::nofBitsForNextTime` -- so nothing in the shipped modem can
 * drive them and nothing here adds a caller.  They are reached through the
 * `ref_` aliases exactly as any other member is; finding F7000's correction of
 * 702 is that a path the vendor never executed still has a tier-1 oracle.
 *
 * ===========================================================================
 * THE TRAP THIS FILE EXISTS TO AVOID: THREE NEAR-IDENTICAL SIBLINGS
 *
 * `generateCPd` and `generateSUVd` are BYTE-IDENTICAL in the blob, all 167
 * bytes of them, and `generateMP` differs from both in exactly three bytes --
 * the low bytes of the field displacements 0x2f5c/0x2f58 against
 * 0x2f90/0x2f8c.  A fixture that cannot drive the MP pair apart from the CP
 * pair is testing one function three times and passing.
 *
 * So `run_discriminate` below runs all three in the SAME configuration and
 * counts, for each of the three pairs, how many configurations left a
 * different answer -- return value AND the whole compared state -- and the
 * counts are ASSERTED rather than printed:
 *
 *   generateMP vs generateCPd    must differ on at least one non-zero-arm
 *   generateMP vs generateSUVd   configuration, or the fixture is broken
 *   generateCPd vs generateSUVd  must be ZERO everywhere, because they are
 *                                the same 167 bytes and cannot differ on any
 *                                input at all
 *
 * THE COUNTS ARE SPLIT BY ARM, and that is not decoration.  On the arm where
 * `nofBitsForNextTime` answers 0 none of the three touches `mpBits`,
 * `mpBitCount`, `cpBits` or `cpBitCount`, so all three pairs are necessarily
 * identical there and a pooled count would dilute the number that matters.
 *
 * TWO ANTI-VACUITY ASSERTIONS ON THE INPUT hold the discrimination up, in the
 * shape t_v90unpck uses: every case asserts that `mpBitCount != cpBitCount`
 * and that the two bit vectors differ in content.  A case that stopped
 * separating them would leave every comparison green and silently drop the
 * counts to zero.
 *
 * ===========================================================================
 * WHY `symbolsBlockSize` IS 1 AND NEVER ANYTHING ELSE
 *
 * All three end in `bitsToSymbol->process(nofBits, &symbol)` with `&symbol`
 * pointing at ONE `short` in the caller's frame -- 0x22(%esp) inside a
 * 0x24-byte frame, so `outSymbols[1]` would land on the saved `%ebx`.
 * `V90BitsToSymbol::process(unsigned int &, short *)` writes
 * `symbolsBlockSize` entries on its ready arm and `symbolsDone` on its
 * underflow arm, so only `symbolsBlockSize == 1` is safe.  That is also the
 * REAL configuration: `V90Phase4Modulator::setMappingParams` ends in
 * `bitsToSymbol->setSymbolsBlockSize(1)`, so phase 4 draws one symbol at a
 * time.  t_v90p4mgen's file comment makes the same argument for the two data
 * pumps and stops at (2, 1) because those have no fill call in front of the
 * drain; here the fill moves `symbolsDone` up before the drain reads it, so
 * (2, 1) would smash the frame and 1 is the only value.
 *
 * WHICH ARM IS TAKEN IS THEREFORE `symbolsDone` ALONE.  `nofBitsForNextTime`
 * answers 0 exactly when `symbolsBlockSize <= symbolsDone`:
 *
 *     symbolsDone 0        non-zero: scramble, fill, drain
 *     symbolsDone 1 or 3   zero:     drain only
 *
 * Both are swept and both are counted; a run that reached only one of them
 * could not tell the two arms apart and `sawZeroArm`/`sawNonZeroArm` fail it.
 *
 * ===========================================================================
 * WHAT IS EXCLUDED FROM THE GRID, AND WHY -- D561's rule
 *
 * `short symbol;` is uninitialised in all three, exactly as `unsigned int
 * nofBits` is in the two data pumps (D661).  `process(unsigned int &, short
 * *)` leaves `*outSymbols` alone when `symbolsBlockSize` is zero (it prints
 * SIZE_NOT_SET and returns) and when its underflow arm has `symbolsDone ==
 * 0`.  Either configuration makes the returned value indeterminate in the
 * reconstruction as much as in the object, so a trial reaching it is not a
 * differential trial.
 *
 *   - `symbolsBlockSize == 0` is out of the grid entirely: it is 1 always.
 *   - the underflow arm with nothing to hand over is out by CONSTRUCTION on
 *     the zero arm (`symbolsDone >= 1` there) and is CHECKED on the non-zero
 *     arm: at debug level 2 the callee prints "BUFFER_UNDERFLOW" when it
 *     takes that arm, and every level-2 trial asserts the transcript does not
 *     contain it.  `underflowChecked` is the denominator of that check.
 *
 * D661 ITSELF DOES NOT APPLY.  Ours is `nofBits = nofBitsForNextTime()`
 * before the call -- `mov %eax,0x1c(%esp)` at +0x2dbe4 -- where the two data
 * pumps pass the slot untouched.
 *
 * ===========================================================================
 * WHAT IS SHARED AND WHAT IS SPLIT
 *
 * SHARED, one copy pointed at by both sides, because they are INPUT and two
 * separately seeded inputs would agree whatever was read out of them
 * (t_v90unpck's argument): the `V90Parameters` block (nothing reads it -- the
 * mapper and the modulator only store the pointer), the `V90MappingParams`
 * block, and the two bit vectors `mpBits` and `cpBits` point at.  That also
 * means +0x48, +0x4c, +0x50, +0x54, +0x2f58, +0x2f8c and +0x2fa8 hold the
 * SAME address on both sides and stay in the object comparison rather than
 * being blanked out of it.
 *
 * SPLIT, one per side, because they are written: the `V90BitsToSymbol` (both
 * `process` overloads move it), its `V90Mapper`, its symbol array, and the
 * `Scrambler`'s history buffer.  The scrambler is EMBEDDED at +0x58, so its
 * seven pointer words are blanked from the object comparison and its history
 * is compared as CONTENT plus the three running indexes as OFFSETS from
 * `pLimit` -- position-independent, which two `sysdep_malloc` results are
 * not.
 *
 * `scrambledBits` at +0x78 is embedded too and is 12,000 bytes of the 12,204
 * compared, which is where most of the signal is.
 *
 * THE OBJECTS ARE NEVER ZEROED -- finding F230 -- and every trial rebuilds
 * both sides from a fresh pseudorandom fill and runs each side's OWN
 * constructor over it, so the scrambler's malloc'd history and the mapper are
 * in their real post-construction state and not a poke.
 *
 * `V90MP *` and `V90CP *` are passed as one shared dummy address each: the
 * three functions never dereference either field, and the constructor only
 * stores it.
 *
 * FINDING F7105's CHECK.  `V90BitsToSymbol::reset` writes `symbolsDone`,
 * `symbolsBlockSize` and `extraSymbolsPending`, so a seed alone cannot hold
 * them; every one of the three is poked AFTER the reset, and `bitsPerFrame`
 * and `extraSymbols` are the two the constructor leaves alone and `reset`
 * fills, which is why the mapping block has to be plausible before the reset
 * rather than after it.
 * ===========================================================================
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/Scrambler.h"
#include "dsplib/V90BitsToSymbol.h"
#include "dsplib/V90Mapper.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Phase3Modulator.h"		/* PcmType */
#include "dsplib/V90Phase4Modulator.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

/*
 * The lifecycle, both sides.  C++ has no syntax for running a constructor over
 * storage that already exists, so both sides go through asm() labels; findings
 * F223 and F224, and t_v90modprog does the same for `V90Modem`.
 */
void p4m_ctor1(void *self, void *params, unsigned int flag, void *bts,
	       void *mp, void *mpsA, void *mpsB, void *cp, unsigned int arg8)
	asm("_ZN18V90Phase4ModulatorC1EP13V90ParametersjP15V90BitsToSymbolP5"
	    "V90MPP16V90MappingParamsS7_P5V90CPj");
void ref_p4m_ctor1(void *self, void *params, unsigned int flag, void *bts,
		   void *mp, void *mpsA, void *mpsB, void *cp,
		   unsigned int arg8)
	asm("ref__ZN18V90Phase4ModulatorC1EP13V90ParametersjP15V90BitsToSymbol"
	    "P5V90MPP16V90MappingParamsS7_P5V90CPj");
void p4m_dtor1(void *self)	asm("_ZN18V90Phase4ModulatorD1Ev");
void ref_p4m_dtor1(void *self)	asm("ref__ZN18V90Phase4ModulatorD1Ev");

void bts_ctor1(void *self, unsigned int n, void *params)
	asm("_ZN15V90BitsToSymbolC1EjP13V90Parameters");
void ref_bts_ctor1(void *self, unsigned int n, void *params)
	asm("ref__ZN15V90BitsToSymbolC1EjP13V90Parameters");
void bts_dtor1(void *self)	asm("_ZN15V90BitsToSymbolD1Ev");
void ref_bts_dtor1(void *self)	asm("ref__ZN15V90BitsToSymbolD1Ev");

void ref_bts_reset(void *self, void *mp, int pcm)
	asm("ref__ZN15V90BitsToSymbol5resetEP16V90MappingParams7PcmType");
unsigned int ref_bts_fill(void *self, unsigned char *bits, unsigned int nofBits)
	asm("ref__ZN15V90BitsToSymbol7processEPhj");

/* The three under test, blob side.  `short` for the reason the .cpp gives. */
short ref_p4m_generateMP(void *self)
	asm("ref__ZN18V90Phase4Modulator10generateMPEv");
short ref_p4m_generateCPd(void *self)
	asm("ref__ZN18V90Phase4Modulator11generateCPdEv");
short ref_p4m_generateSUVd(void *self)
	asm("ref__ZN18V90Phase4Modulator12generateSUVdEv");

/*
 * ... and the three TRAINING sources the VPcmV34Main leaf pass added, the
 * other half of the same family: the scrambler's input is generated
 * (constant ones for B1d and TRN2d, zeros for Ed) instead of fetched, so
 * they read neither bit-vector field and this fixture already stands up
 * everything they touch.
 */
short ref_p4m_generateB1d(void *self)
	asm("ref__ZN18V90Phase4Modulator11generateB1dEv");
short ref_p4m_generateTRN2d(void *self)
	asm("ref__ZN18V90Phase4Modulator13generateTRN2dEv");
short ref_p4m_generateEd(void *self)
	asm("ref__ZN18V90Phase4Modulator10generateEdEv");
}

/* ------------------------------------------------------------- the storage */

#define GUARD		64
#define P4M_SLOT	((unsigned)sizeof(V90Phase4Modulator) + GUARD)
#define BTS_SLOT	((unsigned)sizeof(V90BitsToSymbol) + GUARD)

/*
 * The converter's symbol array.  `V90Mapper::process` writes `nofOut` entries
 * at `symbols + symbolsDone` and `V90BitsToSymbol::process` only notices an
 * overrun AFTERWARDS, so this has to be larger than any fill the grid can
 * produce: 42 bits to a six-symbol frame and at most 252 bits in a call is at
 * most 36 symbols, less the shaper's priming.
 */
#define NSYM		128u

/* One byte per bit, and long enough for the largest count in `cases`. */
#define NBITS		512

/* 42 bits to a six-symbol frame at 8 kHz is 56,000 bit/s; finding F7622. */
#define BPF		42u

static unsigned char p4m_s[2][P4M_SLOT] __attribute__((aligned(8)));
static unsigned char p4m_seed[P4M_SLOT];
static unsigned char p4m_cmp[2][P4M_SLOT];

static unsigned char bts_s[2][BTS_SLOT] __attribute__((aligned(8)));
static unsigned char bts_seed[BTS_SLOT];
static unsigned char bts_cmp[2][BTS_SLOT];

/*
 * The mapper is `sysdep_malloc`'d by the converter's constructor, so it is
 * compared through a scrubbed copy of its own; 0x704 bytes plus nothing.
 */
static unsigned char map_cmp[2][sizeof(V90Mapper)];

/* Shared inputs. */
static unsigned char par_s[sizeof(V90Parameters)] __attribute__((aligned(8)));
static unsigned char mapp_s[sizeof(V90MappingParams)] __attribute__((aligned(8)));
static unsigned char mpdummy[64] __attribute__((aligned(8)));
static unsigned char cpdummy[64] __attribute__((aligned(8)));
static unsigned char mpbits[NBITS];
static unsigned char cpbits[NBITS];

#define P4M(s)		(*(V90Phase4Modulator *)(void *)p4m_s[s])
#define BTS(s)		(*(V90BitsToSymbol *)(void *)bts_s[s])
#define MAPP		((V90MappingParams *)(void *)mapp_s)
#define PARAMS		((void *)par_s)

/* ---------------------------------------------------------------- seeding */

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned)(-(int)(lfsr & 1u) & 0xb400u);
	/* `| 1` so no seeded byte is ever zero; finding F230. */
	return (unsigned char)((lfsr >> 3) | 1u);
}

static void
fill(void *p, unsigned n)
{
	unsigned char *q = (unsigned char *)p;
	unsigned i;

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
 * A plausible V.90 mapping set, the same shape t_v90modprog uses and for the
 * same reasons: `bpf` bits to a six-symbol frame, `shaperSR` 3 which divides
 * six, no constellation longer than `V90Mapper`'s 128-entry row, and the six
 * sizes multiplying past 2**(bpf - 3) so `ModulusEncoder::progress`'s sixth
 * mixed-radix digit cannot index off a row.
 */
static void
plausible_mapping(V90MappingParams *m, unsigned int bpf)
{
	unsigned int i, j;

	m->word_0 = bpf;
	for (i = 0; i < V90_CONSTELLATIONS; i++) {
		unsigned int len = 112u + 4u * i;

		if (len > V90_CONSTELLATION_MAX)
			len = V90_CONSTELLATION_MAX;
		m->constellationSize[i] = len;
		for (j = 0; j < V90_CONSTELLATION_MAX; j++) {
			m->constellation[i][j] =
			    (unsigned char)(0x11u + 3u * j + 7u * i);
			m->codecConstellation[i][j] =
			    (unsigned char)(0x21u + 5u * j + 3u * i);
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

/* ------------------------------------------------------------- the cases */

/*
 * THE TWO BIT COUNTS ARE NEVER EQUAL AND THE TWO VECTORS NEVER MATCH, and
 * both are asserted per trial rather than trusted.  Whole numbers of frames
 * either side of one another; the largest is 252, which is what bounds NSYM
 * above.
 */
struct bitcase {
	const char	*name;
	unsigned int	mpCount;
	unsigned int	cpCount;
	unsigned char	mpPattern;	/* which bit pattern each vector gets */
	unsigned char	cpPattern;
};

static const struct bitcase cases[] = {
	{ "MP short, CP long",	 1u * BPF, 6u * BPF, 0, 1 },
	{ "MP long, CP short",	 5u * BPF, 2u * BPF, 2, 3 },
	{ "one frame apart",	 3u * BPF, 4u * BPF, 1, 2 },
	{ "MP all ones",	 2u * BPF, 3u * BPF, 4, 0 }
};

#define NCASES		((int)(sizeof cases / sizeof cases[0]))

/*
 * One byte per bit, 0 or 1.  Values above 1 are not used: `V90Mapper::process`
 * folds the bits into a mixed-radix integer that indexes a constellation row,
 * so a "bit" of 0xa5 walks off the row and the trial stops being about these
 * three functions.
 */
static void
bitpattern(unsigned char *dst, unsigned int n, unsigned char which)
{
	unsigned int i;

	for (i = 0; i < n; i++) {
		unsigned int v;

		switch (which) {
		case 0:  v = i & 1u; break;
		case 1:  v = (i >> 1) & 1u; break;
		case 2:  v = (i * 7u + 3u) & 1u; break;
		case 3:  v = ((i / 3u) ^ i) & 1u; break;
		default: v = 1u; break;
		}
		dst[i] = (unsigned char)v;
	}
}

/* ------------------------------------------------------------- the fixture */

/*
 * `cp` and `bitsToSymbol` are the only per-side pointers in the modulator;
 * the scrambler's seven at +0x58..+0x70 are the embedded object's.  +0x54 is
 * NOT here: the CP record is a shared dummy.
 */
static const unsigned p4m_skip[] = {
	0x44u,						/* bitsToSymbol   */
	0x58u, 0x5cu, 0x60u, 0x64u, 0x68u, 0x6cu, 0x70u,/* Scrambler's own */
	~0u
};

/* mapper, params, symbols. */
static const unsigned bts_skip[] = { 0x00u, 0x04u, 0x08u, ~0u };

/*
 * The mapper's own three allocations: `params` (stored, never read), `buf`,
 * and the spectral shaper's `delayLine`/`trialLine` -- the shaper is embedded
 * at +0x68c and its two pointers are at its own +0x28 and +0x2c.  The two
 * delay lines are compared as CONTENT below; `buf` is 0x50 bytes the mapper
 * uses as scratch and is compared the same way.
 */
#define V90MAP_SHAPER	0x68cu
#define V90SS_DELAY	0x28u
#define V90SS_TRIAL	0x2cu
#define V90SS_LINE_SH	24u			/* shorts in each line */

/*
 * ... AND A FOURTH, one level further in: the shaper's
 * `ParallelDifferentialEncoder<unsigned char>` at its +0x3c owns a `state_`
 * of `capacity_` bytes, and `capacity_` sits at +0x40 beside it.  Found by
 * the comparison failing at V90Mapper+1736 rather than by reading the
 * headers, which is why it is named here with its offset arithmetic.
 */
#define V90SS_PDE	0x3cu			/* pde.state_          */
#define V90SS_PDE_CAP	0x40u			/* pde.capacity_       */

static const unsigned map_skip[] = {
	0x000u, 0x018u,
	V90MAP_SHAPER + V90SS_DELAY, V90MAP_SHAPER + V90SS_TRIAL,
	V90MAP_SHAPER + V90SS_PDE,
	~0u
};

static void
scrub(unsigned char *dst, const unsigned char *src, unsigned n,
      const unsigned *skip)
{
	int i;

	memcpy(dst, src, n);
	for (i = 0; skip[i] != ~0u; i++)
		memset(dst + skip[i], 0, 4);
}

/*
 * Stand both sides up from scratch: fresh pseudorandom storage, each side's
 * own two constructors over it, `reset` from the shared mapping block, then
 * the three converter words `reset` itself writes (7105) and the two field
 * pairs under test.
 */
static void
setup(long seed, const struct bitcase *bc, unsigned int done0,
      unsigned char pending)
{
	int s;

	lfsr = 0x2c19u ^ (unsigned)seed * 0x9e37u;

	bitpattern(mpbits, NBITS, bc->mpPattern);
	bitpattern(cpbits, NBITS, bc->cpPattern);

	fill(p4m_s[0], P4M_SLOT);
	memcpy(p4m_s[1], p4m_s[0], P4M_SLOT);
	memcpy(p4m_seed, p4m_s[0], P4M_SLOT);

	/*
	 * ONE FILL AND A COPY, not two fills: two `fill` calls run the LFSR on
	 * and give the two sides different seed bytes, which reads as a
	 * difference in every byte neither side writes.
	 */
	fill(bts_s[0], BTS_SLOT);
	memcpy(bts_s[1], bts_s[0], BTS_SLOT);
	memcpy(bts_seed, bts_s[0], BTS_SLOT);

	for (s = 0; s < 2; s++) {
		V90BitsToSymbol *b;
		V90Phase4Modulator *m;


		if (s == 0) {
			bts_ctor1(bts_s[0], NSYM, PARAMS);
			p4m_ctor1(p4m_s[0], PARAMS, 0u, bts_s[0], mpdummy,
				  mapp_s, mapp_s, cpdummy, 0u);
		} else {
			ref_bts_ctor1(bts_s[1], NSYM, PARAMS);
			ref_p4m_ctor1(p4m_s[1], PARAMS, 0u, bts_s[1], mpdummy,
				      mapp_s, mapp_s, cpdummy, 0u);
		}

		b = &BTS(s);
		m = &P4M(s);

		/*
		 * `reset` is what fills `bitsPerFrame` and `extraSymbols` -- the
		 * two fields the constructor leaves alone -- and what stands the
		 * mapper up from the shared block.
		 */
		if (s == 0)
			b->reset(MAPP, PCM_TYPE_MU_LAW);
		else
			ref_bts_reset(bts_s[1], mapp_s, (int)PCM_TYPE_MU_LAW);

		b->symbolsBlockSize = 1u;
		b->symbolsDone = done0;
		b->extraSymbolsPending = pending;

		m->mpBits = mpbits;
		m->mpBitCount = bc->mpCount;
		m->cpBits = cpbits;
		m->cpBitCount = bc->cpCount;
	}
}

static void
teardown(void)
{
	p4m_dtor1(p4m_s[0]);
	ref_p4m_dtor1(p4m_s[1]);
	bts_dtor1(bts_s[0]);
	ref_bts_dtor1(bts_s[1]);
}

/* The scrambler's own storage, which is malloc'd and therefore not comparable
 * as an address.  (1 + b + c) elements with the constructor's (0x12, 0x17,
 * 0x63); `V90Phase4Modulator`'s member initialiser is where those come from. */
#define SCRAM_ELEMS	(1u + 0x17u + 0x63u)

static const unsigned char *
scram_base(int s)
{
	return (const unsigned char *)P4M(s).scrambler.pLimit;
}

static long
scram_off(int s, const unsigned char *p)
{
	return (long)(p - scram_base(s));
}

/*
 * `which`: 0 = the mapper's own scratch `buf`, 1 = the shaper's delay line,
 * 2 = its trial line.  Reached through the raw bytes rather than the types
 * because `V90SpectralShaper` is an embedded member this file has no need to
 * name.
 */
static const unsigned char *
shaper_ptr(int s, int which)
{
	const unsigned char *m = (const unsigned char *)BTS(s).mapper;
	unsigned off;

	if (which == 0)
		off = 0x018u;
	else if (which == 1)
		off = V90MAP_SHAPER + V90SS_DELAY;
	else if (which == 2)
		off = V90MAP_SHAPER + V90SS_TRIAL;
	else
		off = V90MAP_SHAPER + V90SS_PDE;
	return *(const unsigned char *const *)(m + off);
}

static unsigned
pde_capacity(int s)
{
	const unsigned char *m = (const unsigned char *)BTS(s).mapper;

	return *(const unsigned *)(const void *)(m + V90MAP_SHAPER
						 + V90SS_PDE_CAP);
}

static void
compare_all(const char *what, long tag)
{
	unsigned n = (unsigned)sizeof(V90Phase4Modulator);
	int s;

	scrub(p4m_cmp[0], p4m_s[0], P4M_SLOT, p4m_skip);
	scrub(p4m_cmp[1], p4m_s[1], P4M_SLOT, p4m_skip);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase4Modulator",
		     p4m_cmp[0], p4m_cmp[1], n, tag);
	for (s = 0; s < 2; s++)
		diff_eq_int("the guard past the modulator held (%ld)",
			    memcmp(p4m_s[s] + n, p4m_seed + n,
				   P4M_SLOT - n) == 0, 1, tag);

	/* The scrambler: its history as CONTENT and its indexes as OFFSETS. */
	diff_eq_int("the scrambler's history (%ld)",
		    memcmp(scram_base(0), scram_base(1), SCRAM_ELEMS) == 0, 1,
		    tag);
	diff_eq_int("the scrambler's pOut (%ld)",
		    scram_off(0, (const unsigned char *)P4M(0).scrambler.pOut),
		    scram_off(1, (const unsigned char *)P4M(1).scrambler.pOut),
		    tag);
	diff_eq_int("the scrambler's pTap1 (%ld)",
		    scram_off(0, (const unsigned char *)P4M(0).scrambler.pTap1),
		    scram_off(1, (const unsigned char *)P4M(1).scrambler.pTap1),
		    tag);
	diff_eq_int("the scrambler's pTap2 (%ld)",
		    scram_off(0, (const unsigned char *)P4M(0).scrambler.pTap2),
		    scram_off(1, (const unsigned char *)P4M(1).scrambler.pTap2),
		    tag);

	/* The converter, its three pointer words blanked, and its symbols. */
	scrub(bts_cmp[0], bts_s[0], BTS_SLOT, bts_skip);
	scrub(bts_cmp[1], bts_s[1], BTS_SLOT, bts_skip);
	diff_eq_obj_(__FILE__, __LINE__, "the bits-to-symbol converter",
		     "V90BitsToSymbol", bts_cmp[0], bts_cmp[1],
		     (unsigned)sizeof(V90BitsToSymbol), tag);
	for (s = 0; s < 2; s++)
		diff_eq_int("the guard past the converter held (%ld)",
			    memcmp(bts_s[s] + sizeof(V90BitsToSymbol),
				   bts_seed + sizeof(V90BitsToSymbol),
				   BTS_SLOT - sizeof(V90BitsToSymbol)) == 0, 1,
			    tag);
	diff_eq_int("the converter's symbols (%ld)",
		    memcmp(BTS(0).symbols, BTS(1).symbols,
			   NSYM * sizeof(short)) == 0, 1, tag);

	/* The mapper, its four allocated pointers blanked. */
	scrub(map_cmp[0], (const unsigned char *)BTS(0).mapper,
	      (unsigned)sizeof(V90Mapper), map_skip);
	scrub(map_cmp[1], (const unsigned char *)BTS(1).mapper,
	      (unsigned)sizeof(V90Mapper), map_skip);
	diff_eq_obj_(__FILE__, __LINE__, "the mapper", "V90Mapper",
		     map_cmp[0], map_cmp[1], (unsigned)sizeof(V90Mapper), tag);
	diff_eq_int("the mapper's scratch buffer (%ld)",
		    memcmp(shaper_ptr(0, 0), shaper_ptr(1, 0), 0x50u) == 0, 1,
		    tag);
	diff_eq_int("the shaper's delay line (%ld)",
		    memcmp(shaper_ptr(0, 1), shaper_ptr(1, 1),
			   V90SS_LINE_SH * sizeof(short)) == 0, 1, tag);
	diff_eq_int("the shaper's trial line (%ld)",
		    memcmp(shaper_ptr(0, 2), shaper_ptr(1, 2),
			   V90SS_LINE_SH * sizeof(short)) == 0, 1, tag);
	diff_eq_int("the two encoders' capacities (%ld)",
		    (long)pde_capacity(0), (long)pde_capacity(1), tag);
	diff_eq_int("the sign encoder's state (%ld)",
		    memcmp(shaper_ptr(0, 3), shaper_ptr(1, 3),
			   pde_capacity(0)) == 0, 1, tag);

	diff_eq_int("the transcripts agreed (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
}

/* ------------------------------------------------------------ the digest */

/*
 * A digest over exactly what `compare_all` compares, taken from OUR side only
 * and used to answer "did these two functions leave different state".  It
 * cannot contain an address: the two scrubbed objects and the scrambler's
 * history are all position-independent, and the three running indexes go in as
 * offsets.
 */
static unsigned long
digest(void)
{
	unsigned long h = 2166136261UL;
	const unsigned char *p;
	unsigned i;

	scrub(p4m_cmp[0], p4m_s[0], P4M_SLOT, p4m_skip);
	scrub(bts_cmp[0], bts_s[0], BTS_SLOT, bts_skip);

	p = p4m_cmp[0];
	for (i = 0; i < (unsigned)sizeof(V90Phase4Modulator); i++)
		h = (h ^ p[i]) * 16777619UL;
	p = bts_cmp[0];
	for (i = 0; i < (unsigned)sizeof(V90BitsToSymbol); i++)
		h = (h ^ p[i]) * 16777619UL;
	p = (const unsigned char *)BTS(0).symbols;
	for (i = 0; i < NSYM * sizeof(short); i++)
		h = (h ^ p[i]) * 16777619UL;
	p = scram_base(0);
	for (i = 0; i < SCRAM_ELEMS; i++)
		h = (h ^ p[i]) * 16777619UL;

	h = (h ^ (unsigned long)scram_off(0,
	     (const unsigned char *)P4M(0).scrambler.pOut)) * 16777619UL;
	h = (h ^ (unsigned long)scram_off(0,
	     (const unsigned char *)P4M(0).scrambler.pTap1)) * 16777619UL;
	h = (h ^ (unsigned long)scram_off(0,
	     (const unsigned char *)P4M(0).scrambler.pTap2)) * 16777619UL;
	return h;
}

/* -------------------------------------------------------------- the sweep */

typedef short (*ourfn)(V90Phase4Modulator *);
typedef short (*reffn)(void *);

static short our_mp(V90Phase4Modulator *m)	{ return m->generateMP(); }
static short our_cpd(V90Phase4Modulator *m)	{ return m->generateCPd(); }
static short our_suvd(V90Phase4Modulator *m)	{ return m->generateSUVd(); }
static short our_b1d(V90Phase4Modulator *m)	{ return m->generateB1d(); }
static short our_trn2d(V90Phase4Modulator *m)	{ return m->generateTRN2d(); }
static short our_ed(V90Phase4Modulator *m)	{ return m->generateEd(); }

struct arm {
	const char	*name;
	ourfn		ours;
	reffn		theirs;
};

static const struct arm arms[] = {
	{ "generateMP",		our_mp,		ref_p4m_generateMP },
	{ "generateCPd",	our_cpd,	ref_p4m_generateCPd },
	{ "generateSUVd",	our_suvd,	ref_p4m_generateSUVd },
	/* The training half; only the first NARMS rows join the message
	 * discrimination, whose assertions are specific to those three. */
	{ "generateB1d",	our_b1d,	ref_p4m_generateB1d },
	{ "generateTRN2d",	our_trn2d,	ref_p4m_generateTRN2d },
	{ "generateEd",		our_ed,		ref_p4m_generateEd }
};

#define NARMS		3
#define NARMS_ALL	((int)(sizeof arms / sizeof arms[0]))

/* `symbolsDone` on entry.  0 takes the scrambling arm; the rest do not. */
static const unsigned int dones[] = { 0u, 1u, 3u };

#define NDONES		((int)(sizeof dones / sizeof dones[0]))

static int sawZeroArm;
static int sawNonZeroArm;
static int underflowChecked;
static int scrambledMoved;

/*
 * One trial: stand both sides up, run one of the three on each, compare
 * everything, and return the digest of ours.
 */
static unsigned long
one(int a, const struct bitcase *bc, unsigned int done0, unsigned char pending,
    unsigned lvl, long seed, long tag, short *rcOut)
{
	short rc[2];
	unsigned long h;

	/*
	 * THE SEED IS SEPARATE FROM THE TAG ON PURPOSE.  `run_discriminate`
	 * runs all three arms in one configuration and compares their digests,
	 * so the three must be handed IDENTICAL storage; a seed taken from the
	 * tag would make them differ for a reason that has nothing to do with
	 * the functions and the discrimination counts would be meaningless.
	 */
	setup(seed, bc, done0, pending);

	/*
	 * ANTI-VACUITY ON THE INPUT.  The whole discrimination between the MP
	 * source and the two CP ones rests on these two, so they are checked
	 * every trial in both directions rather than declared once.
	 */
	diff_eq_int("the two bit counts differ (%ld)",
		    P4M(0).mpBitCount != P4M(0).cpBitCount, 1, tag);
	diff_eq_int("the two bit vectors differ (%ld)",
		    memcmp(mpbits, cpbits,
			   bc->mpCount < bc->cpCount ? bc->mpCount
						     : bc->cpCount) != 0,
		    1, tag);

	if (done0 == 0u)
		sawNonZeroArm++;
	else
		sawZeroArm++;

	set_level(lvl);
	dsplib_debug_capture_reset();
	dsplib_debug_capture_on = 1;
	rc[0] = arms[a].ours(&P4M(0));
	rc[1] = arms[a].theirs(p4m_s[1]);
	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the returned symbol (%ld)", (long)rc[0],
		    (long)rc[1], tag);
	compare_all(arms[a].name, tag);

	/*
	 * THE RETURNED SYMBOL WAS WRITTEN, checked and not assumed.  The only
	 * way `process(unsigned int &, short *)` can leave the slot alone here
	 * is its underflow arm with nothing to copy, and that arm announces
	 * itself at debug level 2.  See the exclusion note at the top.
	 */
	if (lvl > 1) {
		diff_eq_int("no BUFFER_UNDERFLOW, so the symbol was written"
			    " (%ld)",
			    strstr(dsplib_debug_capture_text(0),
				   "BUFFER_UNDERFLOW") == 0, 1, tag);
		underflowChecked++;
	}

	/*
	 * The scrambler wrote into the object on the arm that scrambles, which
	 * is what makes the 12,000-byte buffer part of the comparison rather
	 * than 12,000 bytes of seed.
	 */
	if (done0 == 0u &&
	    memcmp(p4m_s[0] + __builtin_offsetof(V90Phase4Modulator,
						 scrambledBits),
		   p4m_seed + __builtin_offsetof(V90Phase4Modulator,
						 scrambledBits),
		   bc->mpCount < bc->cpCount ? bc->mpCount : bc->cpCount) != 0)
		scrambledMoved++;

	h = digest();
	*rcOut = rc[0];
	teardown();
	return h;
}

/* ------------------------------------------------------------ the offsets */

static int
run_map(void)
{
	diff_begin("the fields the three read");

	diff_eq_int("sizeof(V90Phase4Modulator) is 0x%lx",
		    (long)sizeof(V90Phase4Modulator), 0x2fac, 0x2fac);
	diff_eq_int("bitsToSymbol is at +0x%lx",
		    (long)__builtin_offsetof(V90Phase4Modulator, bitsToSymbol),
		    0x44, 0x44);
	diff_eq_int("scrambler is at +0x%lx",
		    (long)__builtin_offsetof(V90Phase4Modulator, scrambler),
		    0x58, 0x58);
	diff_eq_int("scrambledBits is at +0x%lx",
		    (long)__builtin_offsetof(V90Phase4Modulator, scrambledBits),
		    0x78, 0x78);
	diff_eq_int("mpBits is at +0x%lx",
		    (long)__builtin_offsetof(V90Phase4Modulator, mpBits),
		    0x2f58, 0x2f58);
	diff_eq_int("mpBitCount is at +0x%lx",
		    (long)__builtin_offsetof(V90Phase4Modulator, mpBitCount),
		    0x2f5c, 0x2f5c);
	diff_eq_int("cpBits is at +0x%lx",
		    (long)__builtin_offsetof(V90Phase4Modulator, cpBits),
		    0x2f8c, 0x2f8c);
	diff_eq_int("cpBitCount is at +0x%lx",
		    (long)__builtin_offsetof(V90Phase4Modulator, cpBitCount),
		    0x2f90, 0x2f90);

	return diff_end();
}

/*
 * THE CONFIGURATION'S OWN DENOMINATOR.  The non-zero arm is only a
 * differential trial if the fill leaves the converter with at least one symbol
 * for the drain to hand over; below that the drain takes its underflow arm and
 * the returned `short` is indeterminate.  This runs the fill alone, on both
 * sides, for every bit count the grid uses, and asserts it.
 *
 * The bits fed here are the RAW vectors and not the scrambled ones, which is
 * sound for this question and only for this question: `V90Mapper::process`
 * emits a number of symbols that depends on how many bits it was given and not
 * on their values.
 */
static int
run_config(void)
{
	int c, s;
	long tag = 900000L;

	diff_begin("the fill produces symbols for the drain");

	for (c = 0; c < NCASES; c++) {
		unsigned int counts[2];
		int k;

		counts[0] = cases[c].mpCount;
		counts[1] = cases[c].cpCount;

		for (k = 0; k < 2; k++) {
			setup(tag, &cases[c], 0u, 0u);
			for (s = 0; s < 2; s++) {
				if (s == 0)
					BTS(0).process(k ? cpbits : mpbits,
						       counts[k]);
				else
					ref_bts_fill(bts_s[1],
						     k ? cpbits : mpbits,
						     counts[k]);
				diff_eq_int("%ld bits fill at least one"
					    " symbol",
					    (long)(BTS(s).symbolsDone >= 1u), 1,
					    (long)counts[k]);
			}
			teardown();
			tag++;
		}
	}

	return diff_end();
}

static int
run_trials(void)
{
	long tag = 910000L;
	int a, c, d, p, lvl;

	diff_begin("V90Phase4Modulator::generate{MP,CPd,SUVd,B1d,TRN2d,Ed}");

	for (a = 0; a < NARMS_ALL; a++)
		for (c = 0; c < NCASES; c++)
			for (d = 0; d < NDONES; d++)
				for (p = 0; p < 2; p++)
					for (lvl = 0; lvl < 2; lvl++) {
						short rc;

						one(a, &cases[c], dones[d],
						    (unsigned char)p,
						    lvl ? 2u : 0u, tag, tag,
						    &rc);
						tag++;
					}

	diff_eq_int("the zero arm was reached (%ld)", sawZeroArm > 0, 1, tag);
	diff_eq_int("the scrambling arm was reached (%ld)", sawNonZeroArm > 0,
		    1, tag);
	diff_eq_int("the underflow exclusion was checked (%ld)",
		    underflowChecked > 0, 1, tag);
	diff_eq_int("the scrambler wrote into scrambledBits (%ld)",
		    scrambledMoved > 0, 1, tag);

	return diff_end();
}

/* ------------------------------------------------------- discrimination */

/*
 * The check that the suite is testing three functions and not one function
 * three times.  Same configuration, all three arms, pairwise.
 */
static int
run_discriminate(void)
{
	long tag = 920000L;
	int c, d, p;
	int nzTrials = 0, zTrials = 0;
	int mpVsCpdNZ = 0, mpVsSuvdNZ = 0, cpdVsSuvdNZ = 0;
	int mpVsCpdZ = 0, mpVsSuvdZ = 0, cpdVsSuvdZ = 0;

	diff_begin("the three are three functions");

	for (c = 0; c < NCASES; c++)
		for (d = 0; d < NDONES; d++)
			for (p = 0; p < 2; p++) {
				unsigned long h[NARMS];
				short rc[NARMS];
				int a;
				int nz = (dones[d] == 0u);

				for (a = 0; a < NARMS; a++)
					h[a] = one(a, &cases[c], dones[d],
						   (unsigned char)p, 0u,
						   tag, tag + a, &rc[a]);

				if (nz)
					nzTrials++;
				else
					zTrials++;

				if (h[0] != h[1] || rc[0] != rc[1]) {
					if (nz)
						mpVsCpdNZ++;
					else
						mpVsCpdZ++;
				}
				if (h[0] != h[2] || rc[0] != rc[2]) {
					if (nz)
						mpVsSuvdNZ++;
					else
						mpVsSuvdZ++;
				}
				if (h[1] != h[2] || rc[1] != rc[2]) {
					if (nz)
						cpdVsSuvdNZ++;
					else
						cpdVsSuvdZ++;
				}
				tag += NARMS;
			}

	/*
	 * The denominators, so a run that stopped reaching one of the two arms
	 * cannot read as a clean sweep (findings F134, F2400).
	 */
	diff_eq_int("scrambling-arm configurations (%ld)", nzTrials,
		    NCASES * 2, 0);
	diff_eq_int("zero-arm configurations (%ld)", zTrials,
		    NCASES * 2 * (NDONES - 1), 0);

	/*
	 * MP against either CP source MUST be separable, and only on the arm
	 * that reads the field pair.  If either of these is zero the fixture
	 * is not driving the three bytes that differ between the bodies.
	 */
	diff_eq_int("generateMP differs from generateCPd on EVERY"
		    " scrambling-arm configuration (%ld)", mpVsCpdNZ,
		    nzTrials, nzTrials);
	diff_eq_int("generateMP differs from generateSUVd on EVERY"
		    " scrambling-arm configuration (%ld)", mpVsSuvdNZ,
		    nzTrials, nzTrials);

	/*
	 * ... AND MUST NOT BE SEPARABLE ON THE OTHER ARM, because neither
	 * field pair is read there.  A non-zero count here would mean the
	 * fixture had picked up state that has nothing to do with the message.
	 */
	diff_eq_int("generateMP agrees with generateCPd on the zero arm"
		    " (%ld)", mpVsCpdZ, 0, 0);
	diff_eq_int("generateMP agrees with generateSUVd on the zero arm"
		    " (%ld)", mpVsSuvdZ, 0, 0);

	/*
	 * AND THE TWO CP SOURCES ARE THE SAME 167 BYTES, so this is zero on
	 * every configuration and is CORRECT rather than a hole.  It is
	 * asserted so that a future change making them differ fails loudly.
	 */
	diff_eq_int("generateCPd and generateSUVd never differ, scrambling arm"
		    " (%ld)", cpdVsSuvdNZ, 0, 0);
	diff_eq_int("generateCPd and generateSUVd never differ, zero arm"
		    " (%ld)", cpdVsSuvdZ, 0, 0);

	return diff_end();
}

/*
 * The same move for the training trio: `generateB1d` and `generateTRN2d`
 * are BYTE-IDENTICAL in the blob (all 149 bytes) and may never differ;
 * `generateEd` scrambles zeros where they scramble ones, so it must differ
 * from both on every scrambling-arm configuration and on none of the
 * zero-arm ones.
 */
static int
run_discriminate_train(void)
{
	long tag = 940000L;
	int c, d, p;
	int nzTrials = 0, zTrials = 0;
	int b1dVsTrn2dAny = 0;
	int b1dVsEdNZ = 0, b1dVsEdZ = 0;

	diff_begin("the training three are two bodies");

	for (c = 0; c < NCASES; c++)
		for (d = 0; d < NDONES; d++)
			for (p = 0; p < 2; p++) {
				unsigned long h[3];
				short rc[3];
				int a;
				int nz = (dones[d] == 0u);

				for (a = 0; a < 3; a++)
					h[a] = one(NARMS + a, &cases[c],
						   dones[d],
						   (unsigned char)p, 0u,
						   tag, tag + a, &rc[a]);

				if (nz)
					nzTrials++;
				else
					zTrials++;

				if (h[0] != h[1] || rc[0] != rc[1])
					b1dVsTrn2dAny++;
				if (h[0] != h[2] || rc[0] != rc[2]) {
					if (nz)
						b1dVsEdNZ++;
					else
						b1dVsEdZ++;
				}
				tag += 3;
			}

	diff_eq_int("scrambling-arm configurations (%ld)", nzTrials,
		    NCASES * 2, 0);
	diff_eq_int("zero-arm configurations (%ld)", zTrials,
		    NCASES * 2 * (NDONES - 1), 0);
	diff_eq_int("generateB1d and generateTRN2d never differ (%ld)",
		    b1dVsTrn2dAny, 0, 0);
	diff_eq_int("generateB1d differs from generateEd on EVERY"
		    " scrambling-arm configuration (%ld)", b1dVsEdNZ,
		    nzTrials, nzTrials);
	diff_eq_int("and agrees with it on the zero arm (%ld)", b1dVsEdZ, 0,
		    0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	fill(par_s, (unsigned)sizeof par_s);
	fill(mpdummy, (unsigned)sizeof mpdummy);
	fill(cpdummy, (unsigned)sizeof cpdummy);
	fill(mapp_s, (unsigned)sizeof mapp_s);
	plausible_mapping(MAPP, BPF);

	rc |= run_map();
	rc |= run_config();
	rc |= run_trials();
	rc |= run_discriminate();
	rc |= run_discriminate_train();

	return rc;
}

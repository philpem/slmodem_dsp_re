/*
 * t_v92p4reset.cpp -- differential test of V92Phase4Modulator::reset
 * (`_ZN18V92Phase4Modulator5resetEsh23V92Phase4ModulatorStatejj`, 290 bytes at
 * .text+0x19030), the last of the class's 34 members.
 *
 * SEPARATE FROM t_v92p4sym.cpp, which drives the nine symbol producers and
 * `generateSymbol` itself, and from t_v92p4gen.cpp, which drives the fifteen
 * handlers.  `reset` needs BOTH of their fixtures at once -- a placed
 * scrambler and a constructed transmitter chain from the first, a V92CP PER
 * SIDE from the second -- because it writes the object, writes the caller's
 * V92CP, repacks the CP message, takes `pattern` back out of it, and then
 * calls `generateSymbol` in a loop.  It also needs something neither of them
 * has: a grid over its own FIVE ARGUMENTS.
 *
 * ---------------------------------------------------------------------------
 * WHAT reset CLAIMS, IN THE OBJECT'S ORDER
 *
 *     word_0c = 0                 word_44 = suvLimit
 *     amplitude = amplitudeArg    byte_42 = bitsArg
 *     state = stateArg            bitsPerSymbol = bitsArg + 2
 *     symbolCount = 0
 *     mapper->reset(amplitude, bitsArg)
 *     scrambler.reset(0)
 *     prevBit = 0
 *     word_1c0 = 0 ; cp->word_110 = 0 ; word_1c4 = 0
 *     word_28 = 0 ; flag_3c = 0 ; word_2c = 0 ; word_30 = 0 ; word_34 = 0
 *     word_18 = 0 ; byte_1c = 0 ; flag_20 = 0
 *     cp->bitsPerSymbol = 1 ; cp->byte_00 = 0 ; cp->infoToBits()
 *     pattern = cp->getBitVector(patternLength)
 *     e2uExtended = 0
 *     for (i = 0; i < nSymbols; i++) generateSymbol()
 *
 * Sixteen of those stores write a ZERO and the seventeenth claim of the same
 * kind is `scrambler.reset(0)`'s argument; that is how the mutation set counts
 * to seventeen, and it says so in its NOTE.
 *
 * ---------------------------------------------------------------------------
 * TWO RUNS, AND THE SPLIT IS THE POINT OF THE BATCH
 *
 * RUN A drives `nSymbols == 0`, so `generateSymbol` is never called and every
 * store above is measured against the object seed with no state machine on top
 * of it.  Nine of the sixteen clears are re-written by `generateSymbol` on
 * paths this class does not choose -- `word_0c` is cleared by its prologue on
 * every call, `symbolCount` is incremented by it, `word_1b0` and `word_1b8`
 * and `patternLength` are rewritten by six arms -- so a grid that ALWAYS ran
 * the loop would be measuring the state machine and calling it `reset`.
 *
 * RUN B drives `nSymbols` over 1..4 so the loop, its bound, and the discarding
 * of `generateSymbol`'s return value are covered.  Its `bitsArg` axis is
 * {0, 1} and ONLY that, which is not tidiness: `reset` hands `bitsArg` RAW to
 * `V92Mapper::reset`, whose second argument is the mapper's `mode`, and
 * `V92Mapper::process` indexes `constelAmplitudeTable[acc + 8 * mode]` over a
 * SIXTEEN-entry table.  Any `mode` above one reads off the end of it on both
 * sides at once, which is a comparison that agrees for the wrong reason.
 *
 * ---------------------------------------------------------------------------
 * THE bitsArg + 2 WRAP, WHICH IS WHAT THIS BATCH IS FOR
 *
 * `bitsPerSymbol` (+0x43) is computed `add $0x2,%dl` in ONE BYTE at
 * .text+0x19065, on the eight-bit argument, so `bitsArg == 254` leaves it ZERO
 * and 255 leaves it ONE.  Both are in RUN A's grid and `saw_bits254_bps_zero`
 * asserts that a trial really ran at 254 and really left `+0x43 == 0` on BOTH
 * SIDES -- not that the value was merely gridded.
 *
 * THEY ARE DRIVEN AT `nSymbols == 0` DELIBERATELY.  `reset` itself survives a
 * zero there: it forces `cp->bitsPerSymbol = 1` before `infoToBits`, so the
 * one division it can reach has a divisor of 12 whatever was passed.
 * `generateSymbol` does not -- D700's twelve unguarded divisions include
 * `patternLength / bitsPerSymbol` in six arms -- and a trial that drove it
 * would raise #DE identically on both sides, which measures the CPU and not
 * the reading (D571's argument).  The observable is the state `reset` leaves.
 *
 * ---------------------------------------------------------------------------
 * FIVE THINGS THE FIXTURE HAS TO CONSTRAIN, AND NONE OF THEM IS TIDINESS
 *
 * The first four are t_v92p4sym.cpp's, verbatim, and they apply here because
 * `reset` ends by calling `generateSymbol`.  Each is a range outside which
 * BOTH sides are undefined, so the comparison would agree while proving
 * nothing -- or take the process down.
 *
 *   1. THE SCRAMBLER'S HISTORY AND `pattern` HOLD 0 OR 1.  `Scrambler<h,h>::
 *      process` returns `in ^ *pTap1 ^ *pTap2`, and that byte is what
 *      `V92Mapper::process` turns into a table index.  The object's own
 *      `bits[]` is in this list too: the mapper reads `mapper->bits` of them
 *      whatever `bitsPerSymbol` was.  `pattern` is 0/1 before `reset` runs
 *      and, after it, points into `cp->bits`, which `infoToBits` has just
 *      filled with one byte per bit and zero-padded to `vectorLen`.
 *   2. THE MAPPER'S `mode` IS 0 OR 1 -- see RUN B's `bitsArg` axis above.
 *   3. `patternIndex` STARTS BELOW `patternLength`.  `reset` does NOT write
 *      `patternIndex` (there is no store to +0x08 in the 290 bytes), and the
 *      generators read `pattern[patternIndex]` BEFORE the `% patternLength`
 *      that follows, so the first read is unguarded.  It is seeded at 0..3 and
 *      `patternLength` after `reset` is `cp->vectorLen`, a multiple of twelve;
 *      `run_antivacuity` asserts the relation held rather than assuming it.
 *   4. `symbolsBlockSize` IS ONE, AND IS SET AFTER EVERY `V92BitsToSymbol::
 *      reset`.  At zero, `process(unsigned int &, short *)` returns without
 *      assigning its reference argument, which the arms pass uninitialised.
 *
 * And the fifth is t_v92p4gen.cpp's and is about the V92CP, which `reset`
 * repacks on every single trial:
 *
 *   5. THE V92CP IS SANE ENOUGH FOR `infoToBits` -- `bitsPerSymbol` in 1..6,
 *      `word_10c` at most six, the five floats finite (D570, D571).  The
 *      bound on `bitsPerSymbol` is not inherited politeness: the mutation that
 *      DROPS `cp->bitsPerSymbol = 1` lets the seeded value reach
 *      `vectorLen = (word_11c / (12 * bps) + 1) * (12 * bps)` and then
 *      `for (i = word_11c; i < vectorLen; i++) bits[i] = 0`, so a seed of 200
 *      zero-fills to 2400 against a `V92CP_BITS` of 2000 -- straight through
 *      `crc`, `vectorLen`, `msgLen` and out of the object.  That mutant would
 *      be "caught" by a scribble, which is a verdict nobody can read.
 *
 * ---------------------------------------------------------------------------
 * THE V92CP IS ONE PER SIDE, SO `pattern` IS CHECKED AS AN OFFSET
 *
 * `reset` writes three fields of the caller's V92CP and then calls
 * `infoToBits`, which rewrites the whole bit vector, `vectorLen` and `msgLen`.
 * One shared block would let our side's failure to write be covered up by the
 * reference writing a moment later, so each side gets its own and the two are
 * compared whole.
 *
 * That makes `pattern` (+0x1a8) a PER-SIDE ADDRESS by construction -- it is
 * `cp->getBitVector`'s return, which is `cp + 0x129` -- so the object compare
 * normalises it to the offset within that side's own CP rather than masking
 * it.  That check is load-bearing: it is the only witness that `pattern` was
 * assigned at all.  `run_antivacuity` also asserts it directly, against
 * `cpbuf[s]->bits`, because an offset comparison between two sides that both
 * failed to write would still agree.
 *
 * ---------------------------------------------------------------------------
 * WHAT reset DOES NOT WRITE, AND WHY EVERY SEED IS NON-ZERO
 *
 * `word_24`, `word_38`, `word_1b0`, `word_1b8`, `pad_10`, `+0x1d..+0x1f`
 * (`pad_1d` until finding F10145 folded it into the compiler's own tail
 * alignment after `byte_1c`), `patternIndex`, `mappingParams`,
 * `bitsToSymbol`, `mapper`, `cp` and `params` are untouched.  A field seeded to ZERO cannot tell "reset left it alone"
 * apart from "reset cleared it", so in RUN A every one of them is non-zero on
 * every trial and a reconstruction that helpfully cleared one fails.  RUN B
 * varies the two that the state machine READS as booleans (`word_38`, and
 * `mappingParams` against null) because that is where they are inputs rather
 * than witnesses.
 *
 * ---------------------------------------------------------------------------
 * THREE CLAIMS THE OBJECT COMPARE ALONE CANNOT WITNESS
 *
 * Each has a `diff_eq_int` of its own, per side, because the object compare
 * only ever says "the two sides agree" and two sides that both failed the same
 * way agree perfectly:
 *
 *   - `pattern == cp->bits`, seeded to `shared_pattern` (outside the CP) so
 *     that "never assigned" is deterministic and not lucky.
 *   - `patternLength == cp->vectorLen`, seeded to 7 -- which `infoToBits`
 *     cannot produce, since `vectorLen` is a multiple of `12 * bitsPerSymbol`
 *     and `reset` has just forced that to 1.  This is the witness for
 *     `getBitVector`'s REFERENCE argument.
 *   - `state == stateArg`, on both sides.  Our source takes
 *     `V92Phase4ModulatorState`, whose one enumerator is 0; the tests below
 *     feed the argument through a thunk declared `int` (see the extern "C"
 *     block) so the suite never forms an out-of-range enum value itself, but
 *     the SOURCE still copies through the enum type.  If that copy ever
 *     narrows, this check names it instead of leaving a bare byte-0 diff.
 *
 * ---------------------------------------------------------------------------
 * DIAGNOSTICS: `reset` PRINTS NOTHING, AND THAT IS READ OFF THE DISASSEMBLY
 *
 * The 290 bytes at .text+0x19030 contain no reference to `dsplibs_debug_level`
 * and no call to `dsplibs_debug_printf`: the six calls in them are
 * `V92Mapper::reset`, `Scrambler<h,h>::reset`, `V92CP::infoToBits`,
 * `V92CP::getBitVector` and `V92Phase4Modulator::generateSymbol`, and there is
 * no `cmpl $0x1,dsplibs_debug_level` anywhere between the prologue and the
 * `ret`.  So there is no debug run here and no transcript comparison: the
 * transcripts that RUN B could produce belong to `generateSymbol`, which
 * t_v92p4sym.cpp already drives at levels 0, 1 and 2 over its own grid.  Both
 * sides' levels are pinned at zero so that neither takes a diagnostic branch
 * for a reason unrelated to `reset`.
 */

#include <string.h>
#include <stdio.h>

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
/*
 * THE THIRD ARGUMENT IS DECLARED `int` HERE AND THAT IS DELIBERATE.  The
 * mangling says `23V92Phase4ModulatorState`, and the i386 ABI passes an enum
 * in the same four-byte slot an `int` gets -- the object reads it with one
 * `mov 0x2c(%esp),%eax`.  Declaring the thunk with `int` lets the grid feed
 * every state code 0..29 without the TEST forming a value outside the enum's
 * range, which C++98 leaves undefined for an enum with one zero enumerator.
 * The source under test is unchanged and still takes the enum.
 */
void our_reset(void *, short, unsigned char, int, unsigned int, unsigned int)
	asm("_ZN18V92Phase4Modulator5resetEsh23V92Phase4ModulatorStatejj");
void ref_reset(void *, short, unsigned char, int, unsigned int, unsigned int)
	asm("ref__ZN18V92Phase4Modulator5resetEsh23V92Phase4ModulatorStatejj");

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

extern unsigned int ref_dsplibs_debug_level;
}

/* ------------------------------------------------------------------ */

#define GUARD	64u
#define OBJSZ	((unsigned)sizeof(V92Phase4Modulator))
#define SLOT	(OBJSZ + GUARD)
#define CPSZ	((unsigned)sizeof(V92CP))
#define BTSSZ	((unsigned)sizeof(V92BitsToSymbol))
#define MAPSZ	((unsigned)sizeof(V92Mapper))

/* The scrambler's real geometry, from V92P4M_SCRAM_*. */
#define SCR_LEN		(1u + V92P4M_SCRAM_TAP2 + V92P4M_SCRAM_SLACK)
#define SCR_INITOUT	((unsigned)V92P4M_SCRAM_SLACK)
#define SCR_INITTAP1	(SCR_INITOUT + (unsigned)V92P4M_SCRAM_TAP1)
#define SCR_INITTAP2	(SCR_INITOUT + (unsigned)V92P4M_SCRAM_TAP2)

/*
 * LARGER THAN `V92CP_BITS`, AND THAT IS FOR THE MUTANTS.  The seeded
 * `pattern` is what a reconstruction that never assigned the field would go on
 * using, with a `patternLength` of `cp->vectorLen` -- up to 2000.  A sixteen
 * byte block there would put such a mutant out of bounds of one of OUR statics,
 * and a verdict reached by reading the next array along is not a verdict.
 */
#define PATLEN		2048u
#define CHAIN_ALLOC	2048u
#define NCOEF		V92_PARAMSINFO_MAX_FILTER_LEN
#define NCONST		V92_PARAMSINFO_MAX_LC

/* The sentinel `patternLength` starts at.  NOT a multiple of twelve, so
 * `infoToBits` cannot produce it and "getBitVector wrote the reference" is
 * distinguishable from "it did not". */
#define PATLEN_SENTINEL	7u

static unsigned char obj[2][SLOT] __attribute__((aligned(8)));
static unsigned char objseed[SLOT];
static unsigned char cmp_a[SLOT], cmp_b[SLOT];

static unsigned char scr[2][SCR_LEN] __attribute__((aligned(8)));
static unsigned char mapbuf[2][MAPSZ] __attribute__((aligned(8)));
static unsigned char btos[2][BTSSZ] __attribute__((aligned(8)));
static unsigned char cpbuf[2][CPSZ] __attribute__((aligned(8)));

/* Shared: `reset` writes neither, and both are compared raw in the object. */
static unsigned char shared_pattern[PATLEN];
static unsigned char shared_dummy[64] __attribute__((aligned(8)));

static struct V92ParamsInfo params;
static float coefbuf[4][NCOEF];
static int constbuf[V92_PARAMSINFO_CONSTELLATIONS][NCONST];

static unsigned int lfsr;

static long trials_a, trials_b;

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

static V92CP *
C(int s)
{
	return (V92CP *)cpbuf[s];
}

static V92Transmitter *
TX(int s)
{
	return B(s)->transmitter;
}

/* ------------------------------------------------------------------ */
/* The parameter block.  t_v92p4sym.cpp's, and its comment carries the  */
/* evidence for every constraint.                                       */
/* ------------------------------------------------------------------ */

#define PARAM_K		24

static void
seed_params(unsigned int seed)
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

	/* Multiples of four -- FloatFIR stores `nTaps & ~3`. */
	params.lz1 = 4;
	params.lp1 = 4;
	params.lz2 = 4;
	params.lp2 = 0;
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++)
			coefbuf[i][j] = 0.5f / (float)(1 << j);
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
/* The chain's address mask, measured after construction.               */
/* ------------------------------------------------------------------ */

struct sub {
	unsigned off;
	unsigned size;
	const char *what;
};

/* t_v92p4sym's list, and the same list for the same reason. */
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

/* RUN A: the whole of `reset` with the loop switched off. */
static const short amps_a[] = { 0, 1, -1, 1000, -1000, 32767, -32768 };
#define NAMP_A	((int)(sizeof(amps_a) / sizeof(amps_a[0])))

/*
 * 254 AND 255 ARE THE POINT OF THIS AXIS: `add $0x2,%dl` is eight bits wide,
 * so they leave `bitsPerSymbol` at 0 and 1.  253 is beside them because it is
 * the last value that does NOT wrap, and 0 because `bitsPerSymbol` of 2 is
 * what `V92Modulator::enterPhase4` produces.
 */
static const unsigned char bits_a[] = {
	0, 1, 2, 3, 5, 12, 16, 253, 254, 255
};
#define NBITS_A	((int)(sizeof(bits_a) / sizeof(bits_a[0])))

/* RUN B: `mode` must stay inside V92Mapper's two-row table -- see the file
 * comment. */
static const unsigned char bits_b[] = { 0, 1 };
#define NBITS_B	((int)(sizeof(bits_b) / sizeof(bits_b[0])))

/*
 * Every state code the thirty-entry jump table has, plus three it cannot:
 * -1, INT_MIN and 30.  `reset` stores the argument whole and does not dispatch
 * on it, but RUN B's loop hands it straight to `generateSymbol`.
 */
static const int states[] = {
	(-2147483647 - 1), -1,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	30
};
#define NSTATE	((int)(sizeof(states) / sizeof(states[0])))

static const unsigned int suvs_a[] = { 0u, 4u, 800u, 0xffffffffu };
#define NSUV_A	((int)(sizeof(suvs_a) / sizeof(suvs_a[0])))

static const unsigned int suvs_b[] = { 0u, 4u };
#define NSUV_B	((int)(sizeof(suvs_b) / sizeof(suvs_b[0])))

static const short amps_b[] = { 1000, -1000, 4899 };
#define NAMP_B	((int)(sizeof(amps_b) / sizeof(amps_b[0])))

static const unsigned int nsyms_b[] = { 1u, 2u, 3u, 4u };
#define NNSYM_B	((int)(sizeof(nsyms_b) / sizeof(nsyms_b[0])))

#define NTRIAL_A	(NAMP_A * NBITS_A * NSTATE * NSUV_A)
#define NTRIAL_B	(NBITS_B * NSTATE * NNSYM_B * NAMP_B * NSUV_B)

/* What `reset` is called with on this trial. */
struct args {
	short amp;
	unsigned char bits;
	int state;
	unsigned int nsym;
	unsigned int suv;
};

/*
 * ANTI-VACUITY, EVERY ONE ADDRESSED BY NAME OR BY STATE VALUE.
 *
 * Finding F4756's first fixture fault was a counter addressed by INDEX into a
 * table that was later reordered: it went on passing while watching a
 * different member.  `saw_state[]` below is indexed by the STATE CODE itself
 * and not by its position in `states[]`, and the three codes outside 0..29 get
 * flags of their own.
 *
 * THEY ARE CUMULATIVE ACROSS BOTH RUNS AND ASSERTED ONCE, AT THE END.  Nothing
 * clears them between `run_no_symbols` and `run_with_symbols` -- `saw_nsym_zero`
 * is set only by the first and `saw_e2u_witness` only by the second, and
 * `run_antivacuity` reads both.  Anything that zeroed the block between the two
 * runs would leave every assertion still passing while half of them had
 * silently changed which run they were about, so do not add one.
 */
static int saw_bits_254, saw_bits_255, saw_bits254_bps_zero;
static int saw_bits_253, saw_bits_zero;
static int saw_nsym_zero, saw_nsym_nonzero;
static int saw_nsym[8];
static int saw_state[30];
static int saw_state_neg, saw_state_min, saw_state_30;
static int saw_amp_zero, saw_amp_min, saw_amp_max, saw_amp_neg;
static int saw_suv_zero, saw_suv_max;
static int saw_cp_bps_forced;		/* char_01 != 0 && seed != 1        */
static int saw_cp_w110_witness;		/* byte_04 == 0 && seed != 0        */
static int saw_cp_byte00_witness;	/* seeded byte_00 != 0              */
static int saw_pattern_moved;		/* pattern left `shared_pattern`    */
static int saw_patlen_moved;		/* patternLength left the sentinel  */
static int saw_scrambler_restart;	/* pOut started below pInitOut      */
static int saw_loop_count_exact;	/* a no-transition arm counted up   */
static int saw_e2u_witness;		/* an arm read the cleared flag     */
static int saw_patindex_in_range;	/* constraint 3 held after `reset`  */

/* The two state codes whose arm neither transitions nor resets `symbolCount`,
 * so `symbolCount` after `reset` is exactly `nSymbols`.  Named, because that
 * is the direct witness for the loop bound. */
#define COUNTABLE_STATE_A	0
#define COUNTABLE_STATE_B	V92P4M_STATE_TRN2U_MOD

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

/*
 * The seed, identical on both sides except for each side's own buffers.
 *
 * `runb` picks the two axes that are INPUTS to the state machine rather than
 * witnesses of an absent store -- see the file comment's "what reset does not
 * write".
 */
static void
setup(long trial, const struct args *a, int runb)
{
	int s;
	unsigned int mix = (unsigned int)trial;
	unsigned int back = (unsigned int)(trial * 37) % (SCR_INITOUT + 1u);
	unsigned int i;
	unsigned char seed_cpbps;
	int seed_char01;
	unsigned char seed_byte04;
	unsigned char seed_byte00;

	lfsr = 0x51a3u + 0x9e37u * (unsigned)trial + 1u;

	fill(objseed, SLOT);
	memcpy(obj[0], objseed, SLOT);
	memcpy(obj[1], objseed, SLOT);

	/* Bits, not bytes -- constraint 1. */
	for (i = 0; i < PATLEN; i++)
		shared_pattern[i] = (unsigned char)(nextbyte() & 1u);
	for (i = 0; i < SCR_LEN; i++)
		scr[0][i] = (unsigned char)(nextbyte() & 1u);
	memcpy(scr[1], scr[0], SCR_LEN);
	for (i = 0; i < V92P4M_BITS_LEN; i++) {
		unsigned char v = (unsigned char)(nextbyte() & 1u);

		M(0)->bitsExt[V92P4M_BITS_BELOW + i] = v;
		M(1)->bitsExt[V92P4M_BITS_BELOW + i] = v;
	}

	fill(shared_dummy, sizeof(shared_dummy));

	if (back != 0)
		saw_scrambler_restart = 1;

	/*
	 * THE V92CP.  Its bounds are constraint 5; `seed_cpbps` and
	 * `seed_char01` and `seed_byte04` are kept because the three masked
	 * claims below are CONJUNCTIONS of them and not axes of their own.
	 */
	seed_cpbps = (unsigned char)(1u + (mix % 6u));
	seed_char01 = (int)(mix % 5u) - 1;
	seed_byte04 = (unsigned char)(((mix % 4u) == 0u) ? 1u : 0u);
	seed_byte00 = (unsigned char)(1u + (mix % 3u));

	fill(cpbuf[0], CPSZ);
	{
		V92CP *c = C(0);
		int f;

		c->bitsPerSymbol = seed_cpbps;
		c->word_10c = (unsigned short)(mix % 7u);
		c->char_01 = (signed char)seed_char01;
		c->char_02 = (signed char)((int)(mix % 9u) - 4);
		/* NEVER ZERO: `reset` clears it, and a zero seed could not
		 * tell the store from its absence. */
		c->byte_00 = seed_byte00;
		c->byte_04 = seed_byte04;
		c->byte_24 = (unsigned char)((mix >> 1) & 1u);
		/* NEVER ZERO, for the same reason. */
		c->word_110 = 0xa5a5a5a5u;

		for (f = 0; f < 5; f++) {
			float v = (float)((int)(mix % 17u) - 8) / 4.0f;

			switch (f) {
			case 0:	c->flt_10 = v;	break;
			case 1:	c->flt_14 = -v;	break;
			case 2:	c->flt_18 = v;	break;
			case 3:	c->flt_1c = -v;	break;
			default: c->flt_20 = v;	break;
			}
		}
	}
	memcpy(cpbuf[1], cpbuf[0], CPSZ);

	/*
	 * CONDITIONAL, LIKE ITS TWO NEIGHBOURS, and not a restatement of the
	 * line that seeded it.  A counter assigned unconditionally is a
	 * self-test that cannot fail, which is the dead detector in its purest
	 * form (finding F3110); written this way it witnesses the SEED, so a
	 * later edit that let `byte_00` reach zero fails here rather than
	 * quietly making the `cp->byte_00 = 0` mutation unobservable.
	 */
	if (seed_byte00 != 0)
		saw_cp_byte00_witness = 1;
	if (seed_char01 != 0 && seed_cpbps != 1)
		saw_cp_bps_forced = 1;
	if (seed_byte04 == 0)
		saw_cp_w110_witness = 1;

	for (s = 0; s < 2; s++) {
		V92Phase4Modulator *o = M(s);
		unsigned int j;

		/*
		 * EVERY FIELD `reset` WRITES IS SEEDED AWAY FROM WHAT IT WILL
		 * WRITE, so each store is separable from its absence.
		 */
		o->state = 0x7eadbee1;
		o->symbolCount = 0x11110000u + (unsigned)trial;
		o->word_0c = 0xdeadbeefu;
		o->word_18 = 0x18181818u;
		o->byte_1c = 0x9cu;
		o->flag_20 = 0x20202020u;
		o->word_28 = 0x28282828u;
		o->word_2c = 0x2c2c2c2cu;
		o->word_30 = 0x30303030u;
		o->word_34 = 0x34343434u;
		o->flag_3c = 0x3c3c3c3cu;
		o->amplitude = (short)(0x4141 + (int)(mix % 7u));
		o->byte_42 = (unsigned char)(0x42u + (mix % 5u));
		o->bitsPerSymbol = (unsigned char)(1u + (mix % 9u));
		o->word_44 = 0x44444444u;
		o->prevBit = 1u + (mix % 3u);
		o->e2uExtended = 0xbcbcbcbcu;
		o->word_1c0 = 0xc0c0c0c0u;
		o->word_1c4 = 0xc4c4c4c4u;

		/*
		 * AND EVERY FIELD IT DOES NOT WRITE IS NON-ZERO, so that a
		 * reconstruction which helpfully cleared one fails.  The two
		 * the state machine READS as booleans vary in RUN B only.
		 */
		o->patternIndex = (unsigned int)(trial & 3);
		o->word_24 = 0x24242424u;
		o->word_38 = runb ? (unsigned int)((trial >> 2) & 1)
				  : 0x38383838u;
		/*
		 * NEVER ONE IN RUN A, AND THAT IS ABOUT THE MUTANTS.  `reset`
		 * does not read this field and RUN A never calls
		 * `generateSymbol`, so on unmutated source the value is
		 * immaterial and only its being NON-ZERO is the claim.  The
		 * mutation that runs the loop one extra time DOES call it,
		 * with a `bitsPerSymbol` that the 254 and 255 rows leave at 0
		 * and 1 -- and six arms reach `word_1b0 = patternLength /
		 * bitsPerSymbol` behind `symbolCount % word_1b0 == 0`, which
		 * at the `symbolCount` of 1 that one call produces is true for
		 * a `word_1b0` of ONE and false for anything else.  At one the
		 * mutant would raise #DE and be "caught" by a SIGFPE, which is
		 * not a reading of anything (D571's argument); away from one
		 * it is caught by the checks.  RUN B keeps the one, because
		 * there it is what makes those arms fire at all.
		 */
		o->word_1b0 = runb ? (1u + (unsigned int)(trial % 3) * 5u)
				   : (2u + (unsigned int)(trial % 3) * 5u);
		/* NEVER 12 OR 13: `saw_e2u_witness` reads a 12 here as proof
		 * that an arm evaluated `(e2uExtended != 0) ? 13 : 12` with
		 * the flag already cleared. */
		o->word_1b8 = 99u + (unsigned int)(trial % 4) * 111u;
		for (j = 0; j < sizeof(o->pad_10); j++)
			o->pad_10[j] = (unsigned char)(0x80u | (j + mix));
		/*
		 * +0x1d..+0x1f was `pad_1d[3]`, removed under the pad-removal
		 * workstream (F10145): `byte_1c` ends at +0x1d and `flag_20`
		 * is a 4-byte-aligned `unsigned int` at +0x20, so the compiler
		 * now inserts these three bytes itself.  They are still real
		 * memory inside the object -- an implicit tail is not an
		 * absent one -- so the same canary still has to land there for
		 * the raw byte-for-byte compare below to mean anything; reached
		 * by offset now that there is no named member to reach it
		 * through.
		 */
		{
			unsigned char *raw = (unsigned char *)o;
			for (j = 0; j < 3; j++)
				raw[0x1d + j] = (unsigned char)(0x80u | (j + mix));
		}

		o->pattern = shared_pattern;
		o->patternLength = PATLEN_SENTINEL;
		o->cp = C(s);
		o->params = (V92Parameters *)shared_dummy;
		o->mappingParams = (runb && ((trial & 1) != 0))
				 ? (V92MappingParams *)0
				 : (V92MappingParams *)&params;
		o->mapper = (V92Mapper *)mapbuf[s];
		o->bitsToSymbol = B(s);

		place_scrambler(s, back);
	}

	/*
	 * The mapper: identical random bytes on both sides.  `reset` calls
	 * `V92Mapper::reset` itself, so nothing here has to establish a valid
	 * `mode` -- and the four bytes that member does NOT write stay as
	 * seeded on both sides and are compared with the rest.
	 */
	fill(mapbuf[0], MAPSZ);
	memcpy(mapbuf[1], mapbuf[0], MAPSZ);

	/* The chain, reset and forced back to a block size of one --
	 * constraint 4. */
	our_btos_reset(btos[0], &params);
	ref_btos_reset(btos[1], &params);
	our_btos_setblock(btos[0], 1);
	ref_btos_setblock(btos[1], 1);

	/*
	 * SOMETHING HAS TO BE STAGED whenever the loop will run: five of
	 * `generateSymbol`'s arms return `V92BitsToSymbol::process`'s
	 * reference argument uninitialised when nothing is (D701), and our
	 * frame is not the blob's.  `reset` DISCARDS the return value, so this
	 * cannot reach the comparison here -- but staging costs nothing and
	 * keeps the two sides on the same arm of `process`.
	 */
	if (runb) {
		for (s = 0; s < 2; s++) {
			V92BitsToSymbol *bts = B(s);
			int k;

			for (k = 0; k < 8; k++)
				bts->symbols[k] =
				    (short)(1 + ((int)(mix + (unsigned)k) % 251));
			bts->symbolsDone = 1u + (unsigned int)(mix % 3u);
		}
	}

	(void)a;
}

/*
 * The object comparison.  Poisoned: the scrambler's seven pointers (its
 * `tailLength` at +0x68 is a count and IS compared), `bitsToSymbol`, `mapper`
 * and `cp`, which are one per side by construction.  `mappingParams` and
 * `params` are ONE address shared by both sides and are compared as they
 * stand.  `pattern` is normalised to the offset within the side's own V92CP
 * and still compared -- it is the only witness in the object that the field
 * was assigned at all.
 */
static void
compare_obj(const char *what, long trial)
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
		unsigned char *base = cpbuf[t];
		unsigned char *dst = (t == 0 ? cmp_a : cmp_b) + 0x1a8;
		unsigned long off;

		if (pa < base || pa >= base + CPSZ)
			continue;
		off = (unsigned long)(pa - base);
		memcpy(dst, &off, sizeof(void *));
	}

	diff_eq_obj_(__FILE__, __LINE__, what, "V92Phase4Modulator", cmp_a,
		     cmp_b, (size_t)OBJSZ, trial);
}

/*
 * The V92CP, whole and both of them.  `reset` writes three of its fields and
 * then has `infoToBits` rewrite the bit vector, `vectorLen` and `msgLen`, so
 * anything less than the whole object leaves the repack untested.  No poison:
 * the class holds no pointers.
 */
static void
compare_cp(long trial)
{
	diff_eq_obj_(__FILE__, __LINE__, "the V92CP after reset", "V92CP",
		     cpbuf[0], cpbuf[1], (size_t)CPSZ, trial);
}

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

/*
 * THE THREE CLAIMS THE OBJECT COMPARE CANNOT WITNESS, checked per side.
 *
 * Two sides that both failed to write a field agree perfectly, so each of
 * these is `got` against a value derived from that side's OWN V92CP rather
 * than against the other side.
 */
static void
compare_absolute(const struct args *a, long trial)
{
	int s;

	for (s = 0; s < 2; s++) {
		diff_eq_int("pattern is this side's own cp->bits (trial %ld)",
			    (long)(M(s)->pattern == C(s)->bits), 1, trial);
		diff_eq_int("patternLength is this side's own vectorLen"
			    " (trial %ld)",
			    (long)M(s)->patternLength, (long)C(s)->vectorLen,
			    trial);
		diff_eq_int("state is the argument it was handed (trial %ld)",
			    (long)M(s)->state, (long)a->state, trial);
		diff_eq_int("word_44 is the fifth argument (trial %ld)",
			    (long)M(s)->word_44, (long)a->suv, trial);
		diff_eq_int("byte_42 is the second argument raw (trial %ld)",
			    (long)M(s)->byte_42, (long)a->bits, trial);
		diff_eq_int("amplitude is the first argument (trial %ld)",
			    (long)M(s)->amplitude, (long)a->amp, trial);
		diff_eq_int("bitsPerSymbol is the second argument plus two,"
			    " in one byte (trial %ld)",
			    (long)M(s)->bitsPerSymbol,
			    (long)(unsigned char)(a->bits + 2), trial);
	}

	if (M(0)->pattern != shared_pattern && M(1)->pattern != shared_pattern)
		saw_pattern_moved = 1;
	if (M(0)->patternLength != PATLEN_SENTINEL
	    && M(1)->patternLength != PATLEN_SENTINEL)
		saw_patlen_moved = 1;
	if (M(0)->patternIndex < M(0)->patternLength
	    && M(1)->patternIndex < M(1)->patternLength)
		saw_patindex_in_range = 1;
}

/* ------------------------------------------------------------------ */
/* RUN A -- nSymbols == 0, so the loop never runs.                      */
/* ------------------------------------------------------------------ */

static int
run_no_symbols(void)
{
	long trial;
	int guards = 0;

	diff_begin("V92Phase4Modulator::reset, no symbols");

	for (trial = 0; trial < NTRIAL_A; trial++) {
		struct args a;
		long t = trial;
		int ai = (int)(t % NAMP_A);	t /= NAMP_A;
		int bi = (int)(t % NBITS_A);	t /= NBITS_A;
		int si = (int)(t % NSTATE);	t /= NSTATE;
		int vi = (int)(t % NSUV_A);

		a.amp = amps_a[ai];
		a.bits = bits_a[bi];
		a.state = states[si];
		a.nsym = 0u;
		a.suv = suvs_a[vi];

		setup(trial, &a, 0);

		our_reset(obj[0], a.amp, a.bits, a.state, a.nsym, a.suv);
		ref_reset(obj[1], a.amp, a.bits, a.state, a.nsym, a.suv);

		compare_obj("after reset with no symbols", trial);
		compare_cp(trial);
		compare_scrambler(trial);
		compare_mapper(trial);
		compare_chain(trial);
		compare_absolute(&a, trial);

		/*
		 * THE WRAP, ON BOTH SIDES.  Gridding 254 is not the same claim
		 * as reaching it: this asserts the trial ran AND that both
		 * sides left +0x43 at zero.
		 */
		if (a.bits == 254) {
			saw_bits_254 = 1;
			if (M(0)->bitsPerSymbol == 0
			    && M(1)->bitsPerSymbol == 0)
				saw_bits254_bps_zero = 1;
		}
		if (a.bits == 255)
			saw_bits_255 = 1;
		if (a.bits == 253)
			saw_bits_253 = 1;
		if (a.bits == 0)
			saw_bits_zero = 1;

		saw_nsym_zero = 1;
		if (a.state >= 0 && a.state <= 29)
			saw_state[a.state] = 1;
		else if (a.state == -1)
			saw_state_neg = 1;
		else if (a.state == 30)
			saw_state_30 = 1;
		else
			saw_state_min = 1;

		if (a.amp == 0)
			saw_amp_zero = 1;
		if (a.amp < 0)
			saw_amp_neg = 1;
		if (a.amp == 32767)
			saw_amp_max = 1;
		if (a.amp == -32768)
			saw_amp_min = 1;
		if (a.suv == 0u)
			saw_suv_zero = 1;
		if (a.suv == 0xffffffffu)
			saw_suv_max = 1;

		if (!guard_intact())
			guards++;
		trials_a++;
	}

	diff_eq_int("nothing wrote past the object", guards, 0, 0);
	diff_eq_int("the chain's address mask is not empty", maskcount > 0, 1,
		    0);

	return diff_end();
}

/* ------------------------------------------------------------------ */
/* RUN B -- nSymbols over 1..4, so the loop runs.                       */
/* ------------------------------------------------------------------ */

static int
run_with_symbols(void)
{
	long trial;
	int guards = 0;

	diff_begin("V92Phase4Modulator::reset, 1..4 symbols");

	for (trial = 0; trial < NTRIAL_B; trial++) {
		struct args a;
		long t = trial;
		int bi = (int)(t % NBITS_B);	t /= NBITS_B;
		int si = (int)(t % NSTATE);	t /= NSTATE;
		int ni = (int)(t % NNSYM_B);	t /= NNSYM_B;
		int ai = (int)(t % NAMP_B);	t /= NAMP_B;
		int vi = (int)(t % NSUV_B);

		a.amp = amps_b[ai];
		a.bits = bits_b[bi];
		a.state = states[si];
		a.nsym = nsyms_b[ni];
		a.suv = suvs_b[vi];

		setup(trial, &a, 1);

		our_reset(obj[0], a.amp, a.bits, a.state, a.nsym, a.suv);
		ref_reset(obj[1], a.amp, a.bits, a.state, a.nsym, a.suv);

		compare_obj("after reset with symbols", trial);
		compare_cp(trial);
		compare_scrambler(trial);
		compare_mapper(trial);
		compare_chain(trial);

		/*
		 * `compare_absolute`'s `pattern`/`patternLength` claims do not
		 * survive the loop -- six of `generateSymbol`'s arms repack the
		 * message and take a NEW bit vector -- so only the four the
		 * state machine cannot touch are re-asserted here.  RUN A is
		 * where the other three are measured.
		 */
		{
			int s;

			for (s = 0; s < 2; s++) {
				diff_eq_int("word_44 is the fifth argument"
					    " (trial %ld)",
					    (long)M(s)->word_44, (long)a.suv,
					    trial);
				diff_eq_int("byte_42 is the second argument"
					    " raw (trial %ld)",
					    (long)M(s)->byte_42, (long)a.bits,
					    trial);
				diff_eq_int("amplitude is the first argument"
					    " (trial %ld)",
					    (long)M(s)->amplitude, (long)a.amp,
					    trial);
				diff_eq_int("bitsPerSymbol is the second"
					    " argument plus two (trial %ld)",
					    (long)M(s)->bitsPerSymbol,
					    (long)(unsigned char)(a.bits + 2),
					    trial);
			}
		}

		/*
		 * THE LOOP BOUND, DIRECTLY.  States 0 and 3 neither transition
		 * nor clear `symbolCount`, and `generateSymbol` increments it
		 * once per call before dispatching -- so `reset` leaving
		 * `symbolCount` at exactly `nSymbols` is the count of
		 * iterations, read off the object.
		 */
		if (a.state == COUNTABLE_STATE_A
		    || a.state == COUNTABLE_STATE_B) {
			int s;

			for (s = 0; s < 2; s++)
				diff_eq_int("the loop ran exactly nSymbols"
					    " times (trial %ld)",
					    (long)M(s)->symbolCount,
					    (long)a.nsym, trial);
			saw_loop_count_exact = 1;
		}

		/*
		 * THE CLEAR OF `e2uExtended` HAPPENED BEFORE THE LOOP, and
		 * this is the conjunction that witnesses it: `word_1b8` was
		 * seeded to something that is never 12 or 13, six arms assign
		 * it `(e2uExtended != 0) ? 13 : 12`, and the field was seeded
		 * NON-ZERO -- so a 12 here can only have come from an arm that
		 * read the flag after `reset` had cleared it.
		 */
		if (M(0)->word_1b8 == 12u && M(1)->word_1b8 == 12u)
			saw_e2u_witness = 1;

		saw_nsym_nonzero = 1;
		if (a.nsym < 8u)
			saw_nsym[a.nsym] = 1;
		if (a.state >= 0 && a.state <= 29)
			saw_state[a.state] = 1;
		else if (a.state == -1)
			saw_state_neg = 1;
		else if (a.state == 30)
			saw_state_30 = 1;
		else
			saw_state_min = 1;

		if (!guard_intact())
			guards++;
		trials_b++;
	}

	diff_eq_int("nothing wrote past the object", guards, 0, 0);

	return diff_end();
}

/* ------------------------------------------------------------------ */

static int
run_antivacuity(void)
{
	int s;

	diff_begin("the grid reached what it claims to");

	diff_eq_int("some trial ran with nSymbols == 0", saw_nsym_zero, 1, 0);
	diff_eq_int("some trial ran with nSymbols > 0", saw_nsym_nonzero, 1, 0);
	for (s = 1; s <= 4; s++)
		diff_eq_int("some trial ran this many symbols", saw_nsym[s], 1,
			    (long)s);

	diff_eq_int("a trial ran at bitsArg == 254", saw_bits_254, 1, 0);
	diff_eq_int("a trial ran at bitsArg == 255", saw_bits_255, 1, 0);
	diff_eq_int("a trial ran at bitsArg == 253, the last that does not"
		    " wrap", saw_bits_253, 1, 0);
	diff_eq_int("a trial ran at bitsArg == 0", saw_bits_zero, 1, 0);
	/* THE ONE THIS BATCH EXISTS FOR. */
	diff_eq_int("bitsArg == 254 left bitsPerSymbol at zero on BOTH sides",
		    saw_bits254_bps_zero, 1, 0);

	for (s = 0; s < 30; s++)
		diff_eq_int("some trial fed this state code", saw_state[s], 1,
			    (long)s);
	diff_eq_int("some trial fed the state code -1", saw_state_neg, 1, 0);
	diff_eq_int("some trial fed the state code INT_MIN", saw_state_min, 1,
		    0);
	diff_eq_int("some trial fed the state code 30", saw_state_30, 1, 0);

	diff_eq_int("some trial ran at amplitude 0", saw_amp_zero, 1, 0);
	diff_eq_int("some trial ran at a negative amplitude", saw_amp_neg, 1,
		    0);
	diff_eq_int("some trial ran at amplitude 32767", saw_amp_max, 1, 0);
	diff_eq_int("some trial ran at amplitude -32768", saw_amp_min, 1, 0);
	diff_eq_int("some trial ran at suvLimit 0", saw_suv_zero, 1, 0);
	diff_eq_int("some trial ran at suvLimit 0xffffffff", saw_suv_max, 1, 0);

	/*
	 * THE THREE CONJUNCTIONS, and each is a conjunction because the loose
	 * axis alone is satisfied by trials that cannot distinguish anything
	 * -- finding F4756's shape, and `saw_bps_zero_folding`'s in
	 * t_v92p4sym.cpp.
	 */
	diff_eq_int("a trial could see cp->bitsPerSymbol being forced to one"
		    " (char_01 non-zero, seed not already one)",
		    saw_cp_bps_forced, 1, 0);
	diff_eq_int("a trial could see cp->word_110 being cleared (byte_04"
		    " clear, seed non-zero)", saw_cp_w110_witness, 1, 0);
	diff_eq_int("a trial could see cp->byte_00 being cleared (seed"
		    " non-zero)", saw_cp_byte00_witness, 1, 0);
	diff_eq_int("an arm read e2uExtended after reset had cleared it",
		    saw_e2u_witness, 1, 0);

	diff_eq_int("some trial started the scrambler below its restart point",
		    saw_scrambler_restart, 1, 0);
	diff_eq_int("pattern left the seeded block on both sides",
		    saw_pattern_moved, 1, 0);
	diff_eq_int("patternLength left its sentinel on both sides",
		    saw_patlen_moved, 1, 0);
	diff_eq_int("patternIndex stayed below patternLength after reset",
		    saw_patindex_in_range, 1, 0);
	diff_eq_int("the loop's iteration count was read off a countable arm",
		    saw_loop_count_exact, 1, 0);

	return diff_end();
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

int
main(void)
{
	int rc = 0;

	/* `reset` prints nothing -- see the file comment -- so both sides are
	 * pinned at level 0 and neither can take a diagnostic branch. */
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;
	dsplib_debug_capture_on = 0;

	seed_params(0x4242u);
	build_chain();

	rc |= run_no_symbols();
	rc |= run_with_symbols();
	rc |= run_antivacuity();

	/* THE DENOMINATOR, printed whatever the verdict: findings F134, F2400
	 * and 2401.  A suite that says PASS without saying over how much is
	 * indistinguishable from a suite that measured nothing. */
	printf("t_v92p4reset: %ld trials with no symbols, %ld with 1..4"
	       " symbols, %ld in all\n", trials_a, trials_b,
	       trials_a + trials_b);

	our_btos_d1(btos[0]);
	ref_btos_d1(btos[1]);

	return rc;
}

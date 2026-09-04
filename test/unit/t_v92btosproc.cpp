/*
 * t_v92btosproc.cpp -- differential test of the six V92BitsToSymbol members
 * that are not the constructor or the destructor, and of
 * V92Transmitter::process, which is the worker two of them call.
 *
 * SEPARATE FROM t_v92tx.cpp AND t_v92txreset.cpp for the reason those two are
 * separate from each other: each drives a different lifetime.  t_v92tx builds
 * and destroys and never lets an object survive a call; t_v92txreset resets a
 * built one and stops; this one runs the thing.
 *
 * ---------------------------------------------------------------------------
 * TWO FIXTURES, BECAUSE HALF THE CLASS NEEDS NO HEAP AT ALL.
 *
 * `nofBitsForNextTime`, `setSymbolsBlockSize` and `process(unsigned int &,
 * short *)` never dereference `transmitter` -- there is no call in any of the
 * three -- so part 1 builds the object BY HAND, with the two owned pointers
 * poisoned to a value that would fault if anything followed them, and drives
 * the arithmetic directly.  That buys the sharp cases: the two sides can be
 * put into states a constructed object would take a contrived history to
 * reach, and every scalar can be compared by name instead of through a mask.
 *
 * Part 2 constructs both sides for real, resets them from ONE shared
 * parameter block, and pushes bits through -- which is the only way to reach
 * `V92Transmitter::process`, the modulus encoder, the precoder, the
 * convolution encoder and the pre-filter, none of which can be stubbed
 * because they are reached through pointers the constructor installs.
 *
 * ---------------------------------------------------------------------------
 * THE TRIAL THAT ADJUDICATES `nofBitsForNextTime` IS THE OVERFLOW ONE, and it
 * is the reason this file exists in the shape it does.  The object rounds a
 * symbol count up to a whole frame in two different expressions:
 *
 *     left % 12 != 0   ->   (left / 12 + 1) * bitsPerFrame
 *     left % 12 == 0   ->   left * bitsPerFrame / 12
 *
 * Over 32-bit arithmetic those agree on every input where `left *
 * bitsPerFrame` fits, which is every input a modem will ever produce.  They
 * separate the moment it wraps: with left = 12 and bitsPerFrame = 0x20000000
 * the first gives 0x20000000 and the second 0x0aaaaaaa.  A trial grid that
 * stops at plausible values cannot tell the two readings apart, so the grid
 * below deliberately leaves the plausible range -- finding F3052's trap, and
 * the mutation "the two arms are one expression" in
 * test/mutations/v92btosproc.json is what proves the trial bites.
 *
 * ---------------------------------------------------------------------------
 * PART 2'S MASK IS MEASURED, NOT ASSUMED, exactly as t_v92txreset's is: two
 * objects built from one seed differ only where a heap address landed, so the
 * fixture snapshots both sides after construction, records which 4-byte slots
 * differ, and skips exactly those afterwards.  The mask's size is asserted,
 * so a slot that starts differing somewhere new fails rather than being
 * quietly excused.
 *
 * THE PARAMETER BLOCK IS SANE AND SAYS WHY.  Random bytes are wrong here in a
 * way they are not in t_v92txreset: `V92ModulusEncoder::progress` divides by
 * all twelve moduli, `V92ConvolutionEncoder::process` switches on a mode with
 * no default arm, and `V92Precoder::process` indexes a constellation with
 * `k * step + x`.  A block that violates any of those is undefined behaviour
 * on BOTH sides and would compare equal while proving nothing.  So the moduli
 * are non-zero, the trellis type is one of the three the switch has, and the
 * constellation sizes are small enough that every index the search can form
 * stays inside the 128 entries the arrays hold.  What is still randomised is
 * the bits, the block sizes, the gain and the coefficients.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/V92BitsToSymbol.h"
#include "dsplib/V92Transmitter.h"
#include "dsplib/V92ParamsInfo.h"
#include "dsplib/debug.h"

extern "C" {
void our_btos_c1(void *, unsigned int, void *)
	asm("_ZN15V92BitsToSymbolC1EjP13V92Parameters");
void our_btos_d1(void *) asm("_ZN15V92BitsToSymbolD1Ev");
void our_btos_reset(void *, void *)
	asm("_ZN15V92BitsToSymbol5resetEP16V92MappingParams");
unsigned int our_btos_nbnt(void *) asm("_ZN15V92BitsToSymbol18nofBitsForNextTimeEv");
unsigned int our_btos_setsz(void *, unsigned int)
	asm("_ZN15V92BitsToSymbol19setSymbolsBlockSizeEj");
int our_btos_pb(void *, unsigned char *, unsigned int)
	asm("_ZN15V92BitsToSymbol7processEPhj");
int our_btos_pbs(void *, unsigned char *, unsigned int &, short *)
	asm("_ZN15V92BitsToSymbol7processEPhRjPs");
int our_btos_ps(void *, unsigned int &, short *)
	asm("_ZN15V92BitsToSymbol7processERjPs");

void ref_btos_c1(void *, unsigned int, void *)
	asm("ref__ZN15V92BitsToSymbolC1EjP13V92Parameters");
void ref_btos_d1(void *) asm("ref__ZN15V92BitsToSymbolD1Ev");
void ref_btos_reset(void *, void *)
	asm("ref__ZN15V92BitsToSymbol5resetEP16V92MappingParams");
unsigned int ref_btos_nbnt(void *)
	asm("ref__ZN15V92BitsToSymbol18nofBitsForNextTimeEv");
unsigned int ref_btos_setsz(void *, unsigned int)
	asm("ref__ZN15V92BitsToSymbol19setSymbolsBlockSizeEj");
int ref_btos_pb(void *, unsigned char *, unsigned int)
	asm("ref__ZN15V92BitsToSymbol7processEPhj");
int ref_btos_pbs(void *, unsigned char *, unsigned int &, short *)
	asm("ref__ZN15V92BitsToSymbol7processEPhRjPs");
int ref_btos_ps(void *, unsigned int &, short *)
	asm("ref__ZN15V92BitsToSymbol7processERjPs");

void our_tx_c1(void *) asm("_ZN14V92TransmitterC1Ev");
void our_tx_d1(void *) asm("_ZN14V92TransmitterD1Ev");
void our_tx_reset(void *, void *)
	asm("_ZN14V92Transmitter5resetEP16V92MappingParams");
void our_tx_process(void *, unsigned char *, unsigned int, short *,
		    unsigned int &)
	asm("_ZN14V92Transmitter7processEPhjPsRj");
void ref_tx_c1(void *) asm("ref__ZN14V92TransmitterC1Ev");
void ref_tx_d1(void *) asm("ref__ZN14V92TransmitterD1Ev");
void ref_tx_reset(void *, void *)
	asm("ref__ZN14V92Transmitter5resetEP16V92MappingParams");
void ref_tx_process(void *, unsigned char *, unsigned int, short *,
		    unsigned int &)
	asm("ref__ZN14V92Transmitter7processEPhjPsRj");

extern unsigned int ref_dsplibs_debug_level;
}

/* ------------------------------------------------------------------ */

#define NSYM		256		/* the staging buffer, in shorts */
#define NOUT		512		/* the caller's output buffer    */
#define NBITS		512

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

/* ==================================================================== */
/* Part 1 -- the three members that never touch the transmitter.        */
/* ==================================================================== */

/*
 * The poison.  Nothing in the three members under test may follow either
 * pointer, and 0x1 would fault if one did; it is compared as data instead,
 * which is why both sides get the same value.
 */
#define POISON_TX	((V92Transmitter *)0x1)
#define POISON_PARAMS	((V92Parameters *)0x2)

/*
 * Raw storage and a cast, because the class has no default constructor --
 * every member under test is driven through its mangled name anyway, so the
 * fixture never needs one.
 */
static unsigned char purebuf[2][sizeof(V92BitsToSymbol)]
	__attribute__((aligned(8)));
static short puresym[2][NSYM];
static short pureout[2][NOUT];

static V92BitsToSymbol *
P(int s)
{
	return (V92BitsToSymbol *)purebuf[s];
}

struct purecase {
	const char *name;
	unsigned int nSymbols;
	unsigned int symbolsDone;
	unsigned int bitsPerFrame;
	unsigned int symbolsBlockSize;
	unsigned char flag;
};

/*
 * The grid.  The first block walks `left = blockSize - done` across every
 * residue that matters -- 0, 1, 11, 12, 13, 23, 24 -- because twelve is the
 * divisor and the two arms of the rounding split on the residue.  The second
 * block is the states `process` distinguishes.  The last two are the overflow
 * pair described at the top of the file, and they are the only trials in
 * which the two readings of the rounding disagree.
 */
static const struct purecase pcases[] = {
	{ "block size zero",		NSYM,	0,	7,	0,	1 },
	{ "block size zero, flag down",	NSYM,	0,	7,	0,	0 },
	{ "done == block",		NSYM,	24,	7,	24,	1 },
	{ "done one past block",	NSYM,	25,	7,	24,	1 },
	{ "done far past block",	NSYM,	100,	7,	24,	1 },
	{ "left = 1",			NSYM,	23,	7,	24,	1 },
	{ "left = 11",			NSYM,	13,	7,	24,	1 },
	{ "left = 12",			NSYM,	12,	7,	24,	1 },
	{ "left = 13",			NSYM,	11,	7,	24,	1 },
	{ "left = 23",			NSYM,	1,	7,	24,	1 },
	{ "left = 24",			NSYM,	0,	7,	24,	1 },
	{ "left = 24, done = 0",	NSYM,	0,	13,	24,	1 },
	{ "left = 1 from zero",		NSYM,	0,	7,	1,	1 },
	{ "left = 12 from zero",	NSYM,	0,	7,	12,	1 },
	{ "bits per frame zero",	NSYM,	0,	0,	24,	1 },
	{ "bits per frame one",		NSYM,	5,	1,	24,	1 },
	{ "done wraps past block",	NSYM,	0xfffffff0u, 7,	24,	1 },
	/*
	 * THE TWO THAT SEPARATE THE READINGS.  `left * bitsPerFrame` wraps in
	 * both, and `left` is divisible by twelve in both, so the multiply
	 * happens before the divide and the answer is not `left / 12 *
	 * bitsPerFrame`.  0x20000000 * 12 = 0x1_80000000, which truncates to
	 * 0x80000000 and then divides to 0x0aaaaaaa; the other reading gives
	 * 0x20000000.  The second case does the same with a larger multiple.
	 */
	{ "the multiply wraps, left = 12",  NSYM, 0, 0x20000000u, 12, 1 },
	{ "the multiply wraps, left = 24",  NSYM, 0, 0x11111111u, 24, 1 },
	/* And the residue arm at the same magnitude, where they agree. */
	{ "no wrap, left = 13",		NSYM, 0, 0x20000000u, 13, 1 }
};
#define NPCASE ((int)(sizeof(pcases) / sizeof(pcases[0])))

static void
pure_setup(int c, unsigned int seed)
{
	int s;

	lfsr = seed | 1u;

	for (s = 0; s < 2; s++) {
		P(s)->transmitter	 = POISON_TX;
		P(s)->params		 = POISON_PARAMS;
		P(s)->symbols		 = puresym[s];
		P(s)->nSymbols	 = pcases[c].nSymbols;
		P(s)->symbolsDone	 = pcases[c].symbolsDone;
		P(s)->bitsPerFrame	 = pcases[c].bitsPerFrame;
		P(s)->symbolsBlockSize = pcases[c].symbolsBlockSize;
		P(s)->flag_1c		 = pcases[c].flag;
		/*
		 * The trailing three bytes here used to be the named field
		 * `pad_1d`, zeroed for determinism even though nothing in
		 * this file compares it; `pad_1d` was removed from
		 * V92BitsToSymbol as compiler-inserted alignment (finding
		 * F10151), so this clears the same physical bytes by offset
		 * instead -- +0x1d is where it started and `sizeof(*P(s))`
		 * is unchanged at 0x20.
		 */
		memset((char *)P(s) + 0x1d, 0, sizeof(*P(s)) - 0x1d);
	}

	/* One draw, copied, so both sides start from identical shorts. */
	fill(puresym[0], sizeof(puresym[0]));
	memcpy(puresym[1], puresym[0], sizeof(puresym[0]));

	fill(pureout[0], sizeof(pureout[0]));
	memcpy(pureout[1], pureout[0], sizeof(pureout[0]));
}

static void
pure_compare(const char *what, long tag)
{
	diff_eq_int("symbolsDone agrees, %s (%ld)", (long)P(0)->symbolsDone,
		    (long)P(1)->symbolsDone, tag);
	diff_eq_int("symbolsBlockSize agrees, %s (%ld)",
		    (long)P(0)->symbolsBlockSize,
		    (long)P(1)->symbolsBlockSize, tag);
	diff_eq_int("bitsPerFrame agrees, %s (%ld)", (long)P(0)->bitsPerFrame,
		    (long)P(1)->bitsPerFrame, tag);
	diff_eq_int("nSymbols agrees, %s (%ld)", (long)P(0)->nSymbols,
		    (long)P(1)->nSymbols, tag);
	diff_eq_int("flag_1c agrees, %s (%ld)", (long)P(0)->flag_1c,
		    (long)P(1)->flag_1c, tag);
	diff_eq_int("the transmitter was not followed, %s (%ld)",
		    (long)(P(0)->transmitter == POISON_TX
			   && P(1)->transmitter == POISON_TX), 1, tag);
	diff_eq_int("the staging buffer agrees, %s (%ld)",
		    memcmp(puresym[0], puresym[1], sizeof(puresym[0])) == 0,
		    1, tag);
	diff_eq_int("the output buffer agrees, %s (%ld)",
		    memcmp(pureout[0], pureout[1], sizeof(pureout[0])) == 0,
		    1, tag);
	(void)what;
}

static int
run_pure(void)
{
	int c;
	int rc;

	diff_begin("nofBitsForNextTime, on a hand-built object");

	for (c = 0; c < NPCASE; c++) {
		unsigned int o;
		unsigned int r;

		pure_setup(c, 0x1234u + (unsigned int)c);
		o = our_btos_nbnt(P(0));
		r = ref_btos_nbnt(P(1));

		diff_eq_int("the count agrees (%ld)", (long)o, (long)r,
			    (long)c);
		pure_compare(pcases[c].name, (long)c);
	}

	rc = diff_end();

	diff_begin("setSymbolsBlockSize, on a hand-built object");

	for (c = 0; c < NPCASE; c++) {
		/*
		 * The argument is swept independently of the state, so the
		 * store and the count that follows it are both exercised over
		 * a residue grid of their own.
		 */
		static const unsigned int arg[] = {
			0, 1, 11, 12, 13, 24, 0x20000000u, 0xfffffff0u
		};
		int a;

		for (a = 0; a < (int)(sizeof(arg) / sizeof(arg[0])); a++) {
			unsigned int o;
			unsigned int r;
			long tag = (long)(c * 100 + a);

			pure_setup(c, 0x5678u + (unsigned int)tag);
			o = our_btos_setsz(P(0), arg[a]);
			r = ref_btos_setsz(P(1), arg[a]);

			diff_eq_int("the count agrees (%ld)", (long)o, (long)r,
				    tag);
			diff_eq_int("the block size was stored (%ld)",
				    (long)P(1)->symbolsBlockSize,
				    (long)arg[a], tag);
			pure_compare(pcases[c].name, tag);
		}
	}

	rc |= diff_end();

	/*
	 * `process` LOOPS OVER THE STATE, so the grid above cannot be run at
	 * it whole: `done wraps past block` sets `symbolsDone` to 0xfffffff0,
	 * and the shift-down the object then performs would walk four billion
	 * shorts off the end of any buffer a test can allocate.  That trial
	 * belongs to the two members that only do arithmetic; the ones that
	 * copy get the subset that stays inside NSYM, and PURE_SAFE is where
	 * that is decided rather than in a comment.
	 */
#define PURE_SAFE(c)	(pcases[c].symbolsDone <= NSYM \
			 && pcases[c].symbolsBlockSize <= NSYM)

	diff_begin("process(unsigned int &, short *), on a hand-built object");

	for (c = 0; c < NPCASE; c++) {
		unsigned int on = 0xdeadbeefu;
		unsigned int rn = 0xdeadbeefu;
		int o;
		int r;

		if (!PURE_SAFE(c))
			continue;

		pure_setup(c, 0x9abcu + (unsigned int)c);
		o = our_btos_ps(P(0), on, pureout[0]);
		r = ref_btos_ps(P(1), rn, pureout[1]);

		diff_eq_int("the status agrees (%ld)", (long)o, (long)r,
			    (long)c);
		diff_eq_int("nbits agrees (%ld)", (long)on, (long)rn, (long)c);
		pure_compare(pcases[c].name, (long)c);
	}

	rc |= diff_end();

	/*
	 * Anti-vacuity.  A grid that never reaches an arm proves nothing about
	 * it, so the four statuses are counted and every one must have been
	 * produced by the reference at least once.  SIZE_NOT_SET and
	 * BUFFER_UNDERFLOW are the two the grid above can reach through this
	 * overload; OK is the third.  BUFFER_OVERFLOW is not reachable here --
	 * this overload never calls the transmitter and so can never grow
	 * `symbolsDone` -- and part 2 is what covers it.
	 */
	diff_begin("the status grid is not vacuous");
	{
		int seen[4];
		int c2;

		memset(seen, 0, sizeof(seen));

		for (c2 = 0; c2 < NPCASE; c2++) {
			unsigned int rn = 0;
			int r;

			if (!PURE_SAFE(c2))
				continue;

			pure_setup(c2, 0x9abcu + (unsigned int)c2);
			r = ref_btos_ps(P(1), rn, pureout[1]);
			if (r >= 0 && r < 4)
				seen[r]++;
		}

		diff_eq_int("the grid produced OK (%ld)", seen[V92BTOS_OK] > 0,
			    1, 0);
		diff_eq_int("the grid produced SIZE_NOT_SET (%ld)",
			    seen[V92BTOS_SIZE_NOT_SET] > 0, 1, 0);
		diff_eq_int("the grid produced BUFFER_UNDERFLOW (%ld)",
			    seen[V92BTOS_BUFFER_UNDERFLOW] > 0, 1, 0);
	}
	rc |= diff_end();

	return rc;
}

/* ==================================================================== */
/* Part 2 -- the constructed chain.                                     */
/* ==================================================================== */

#define GUARD		64
#define BTOSSLOT	(sizeof(V92BitsToSymbol) + GUARD)
#define NCOEF		V92_PARAMSINFO_MAX_FILTER_LEN
#define NCONST		V92_PARAMSINFO_MAX_LC

/*
 * The staging buffer both sides are really given.  See the note above
 * `struct chaincase` for why this is not the same number as `nSymbols`.
 */
#define CHAIN_ALLOC	2048

static unsigned char btos[2][BTOSSLOT] __attribute__((aligned(8)));

static struct V92ParamsInfo params;
static float coefbuf[4][NCOEF];
static int constbuf[V92_PARAMSINFO_CONSTELLATIONS][NCONST];

static unsigned char bits[NBITS];
static short chainout[2][NOUT];

/*
 * The sub-objects, by the offset in V92Transmitter that holds them and the
 * size their own headers pin -- t_v92txreset's list, and it is the same list
 * for the same reason.
 */
struct sub {
	unsigned off;
	unsigned size;
	const char *what;
};

static const struct sub subs[] = {
	{ 0x08, 0x50,   "the bit buffer at +0x08" },
	{ 0x48, 0x54,   "V92ModulusEncoder" },
	{ 0x4c, 0x80,   "V92Precoder" },
	{ 0x50, 0x14,   "V92PreFilter" },
	{ 0x54, 0x2008, "V92ConvolutionEncoder" },
	{ 0x58, 1,      "the one-byte buffer at +0x58" }
};
#define NSUB ((int)(sizeof(subs) / sizeof(subs[0])))

#define MAXSUB 0x2008
static unsigned char masked[NSUB][MAXSUB / 4];
static int maskcount;

/* The transmitter block itself, minus the six pointers that are addresses. */
static unsigned char txmask[0x60 / 4];

static V92Transmitter *
tx_of(int s)
{
	return ((V92BitsToSymbol *)btos[s])->transmitter;
}

/*
 * The moduli are non-zero and small, the trellis type is one the switch has,
 * and every constellation index the precoder can form stays inside NCONST.
 * The header comment says why none of that may be randomised.
 */
/*
 * `filters` turns the two pre-filter FIRs and the two precoder FIRs on.  With
 * every length at zero -- which is what the first revision of this fixture
 * used -- `V92PreFilter::process` takes its copy arm and `V92Precoder::process`
 * skips both `fir->process` calls, so three claims in
 * test/mutations/v92txproc.json went NOT CAUGHT for want of a filter to be
 * wrong about rather than for want of a trial.  Half the cases run with them
 * on.  Only FIRs are used: V92PreFilter's `lp2` drives a FloatIIR, and an
 * unstable one would make the comparison a race between two divergences
 * rather than a test.
 */
static void
seed_params(unsigned int seed, unsigned int K, int filters)
{
	int i;
	int j;

	lfsr = seed | 1u;

	memset(&params, 0, sizeof(params));

	params.K = (int)K;
	params.modulosEncoderPresent = 1;
	params.prefilterPrecoderPresent = 1;
	params.constellationPresent = 1;
	params.trellisType = (int)(nextbyte() % 3u);
	params.extendEu = 0;
	/*
	 * NEVER 1.0, AND NEVER A WHOLE NUMBER.  A gain of one makes "the gain
	 * is not applied" a no-op, and an integral one makes truncation and
	 * rounding agree on a constellation of odd integers; both mutations
	 * survived until this line stopped drawing them.
	 */
	params.gain = 1.5f + 0.25f * (float)(nextbyte() & 7u);

	for (i = 0; i < 12; i++)
		params.m[i] = 4 + (int)(nextbyte() & 3u);	/* 4..7 */

	for (i = 0; i < 4; i++) {
		for (j = 0; j < NCOEF; j++)
			coefbuf[i][j] = 0.0f;
	}

	if (filters) {
		/*
		 * MULTIPLES OF FOUR, and that is not tidiness.  FloatFIR's
		 * constructor and `setCoefficients` both store `nTaps & ~3`,
		 * so a length of three becomes a filter of ZERO taps -- and
		 * `floatfir_carry_tail` counts `taps - 1` down to zero with a
		 * `do`/`while`, which at zero taps is 2**32 iterations off the
		 * end of the history buffer.  src/dsp/FloatFIR.cpp records
		 * that as the blob's own behaviour and declines to make it
		 * safe; a fixture that hands it a three therefore crashes,
		 * which is what the first revision of this file did.
		 */
		params.lz1 = 4;
		params.lp1 = 4;
		params.lz2 = 4;
		params.lp2 = 0;		/* the IIR stays off -- see above */

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

static void
construct(unsigned int seed, unsigned int nsym)
{
	lfsr = seed | 1u;

	fill(btos[0], BTOSSLOT);
	memcpy(btos[1], btos[0], BTOSSLOT);

	harness_alloc_reset();
	our_btos_c1(btos[0], nsym, 0);
	ref_btos_c1(btos[1], nsym, 0);
}

static void
teardown(void)
{
	our_btos_d1(btos[0]);
	ref_btos_d1(btos[1]);
}

/*
 * Which 4-byte slots already differ because a heap address landed in them.
 * Measured after construction and before anything under test runs, and its
 * SIZE is asserted so that a slot that starts differing somewhere new fails.
 */
static void
measure_mask(void)
{
	int i;
	unsigned u;

	memset(masked, 0, sizeof(masked));
	memset(txmask, 0, sizeof(txmask));
	maskcount = 0;

	for (u = 0; u < 0x60 / 4; u++) {
		const unsigned char *a = (const unsigned char *)tx_of(0);
		const unsigned char *b = (const unsigned char *)tx_of(1);

		if (memcmp(a + 4 * u, b + 4 * u, 4) != 0) {
			txmask[u] = 1;
			maskcount++;
		}
	}

	for (i = 0; i < NSUB; i++) {
		const unsigned char *a =
			*(const unsigned char **)((unsigned char *)tx_of(0)
						  + subs[i].off);
		const unsigned char *b =
			*(const unsigned char **)((unsigned char *)tx_of(1)
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
compare_all(const char *what, long tag)
{
	const V92BitsToSymbol *o = (const V92BitsToSymbol *)btos[0];
	const V92BitsToSymbol *r = (const V92BitsToSymbol *)btos[1];
	int i;
	unsigned u;
	int bad;

	diff_eq_int("symbolsDone agrees, %s (%ld)", (long)o->symbolsDone,
		    (long)r->symbolsDone, tag);
	diff_eq_int("symbolsBlockSize agrees, %s (%ld)",
		    (long)o->symbolsBlockSize, (long)r->symbolsBlockSize, tag);
	diff_eq_int("bitsPerFrame agrees, %s (%ld)", (long)o->bitsPerFrame,
		    (long)r->bitsPerFrame, tag);
	diff_eq_int("nSymbols agrees, %s (%ld)", (long)o->nSymbols,
		    (long)r->nSymbols, tag);
	diff_eq_int("flag_1c agrees, %s (%ld)", (long)o->flag_1c,
		    (long)r->flag_1c, tag);

	diff_eq_int("the staging buffer agrees, %s (%ld)",
		    memcmp(o->symbols, r->symbols,
			   CHAIN_ALLOC * sizeof(short)) == 0, 1, tag);

	/* The guard past the object proper. */
	diff_eq_int("the guard is untouched, %s (%ld)",
		    memcmp(btos[0] + sizeof(V92BitsToSymbol),
			   btos[1] + sizeof(V92BitsToSymbol), GUARD) == 0, 1,
		    tag);

	bad = 0;
	for (u = 0; u < 0x60 / 4; u++) {
		const unsigned char *a = (const unsigned char *)tx_of(0);
		const unsigned char *b = (const unsigned char *)tx_of(1);

		if (!txmask[u] && memcmp(a + 4 * u, b + 4 * u, 4) != 0)
			bad = (int)(4 * u) + 1;
	}
	diff_eq_int("the transmitter block agrees, %s (%ld)", bad, 0, tag);

	for (i = 0; i < NSUB; i++) {
		const unsigned char *a =
			*(const unsigned char **)((unsigned char *)tx_of(0)
						  + subs[i].off);
		const unsigned char *b =
			*(const unsigned char **)((unsigned char *)tx_of(1)
						  + subs[i].off);

		bad = 0;
		for (u = 0; u + 4 <= subs[i].size; u += 4) {
			if (!masked[i][u / 4] && memcmp(a + u, b + u, 4) != 0)
				bad = (int)u + 1;
		}
		diff_eq_int("%s agrees (%ld)", bad, 0, tag);
	}

	(void)what;
}

/*
 * The block sizes, and the bit counts fed against them.  A block of 24 with
 * K = 24 fills in two frames; a block of 12 fills in one; a block larger than
 * the staging buffer is what reaches BUFFER_OVERFLOW, which nothing in part 1
 * can produce.
 */
/*
 * `nsym` IS THE FIELD, NOT THE ALLOCATION, and the two are deliberately
 * different.  BUFFER_OVERFLOW is the arm where the transmitter has ALREADY
 * written past the end -- the object diagnoses it after the fact and the
 * clamp only tidies the count -- so a fixture that made the allocation as
 * small as the field would be corrupting its own heap to reach the arm, and
 * would then be comparing two corrupted heaps.  Both sides therefore
 * construct with CHAIN_ALLOC shorts and have `nSymbols` written down to
 * `nsym` afterwards: the comparison the object makes is against the FIELD,
 * which is what the arm needs, and every store lands inside real storage.
 */
struct chaincase {
	const char *name;
	unsigned int nsym;	/* what nSymbols is forced to */
	unsigned int K;
	unsigned int block;
	unsigned int nbits;
};

static const struct chaincase ccases[] = {
	{ "one frame exactly",		64,	24,	12,	24 },
	{ "two frames",			64,	24,	24,	48 },
	{ "a partial frame",		64,	24,	24,	20 },
	{ "more bits than the block",	64,	24,	12,	120 },
	{ "block larger than staged",	64,	24,	48,	24 },
	{ "block of zero",		64,	24,	0,	24 },
	{ "no bits at all",		64,	24,	12,	0 },
	{ "K of one",			64,	1,	12,	24 },
	{ "K of 63",			64,	63,	12,	126 },
	{ "K of 64, the two-limb arm",	64,	64,	12,	128 },
	{ "K of 80, the buffer's size",	64,	80,	24,	160 },
	{ "overflow: 12 frames, 8 slots", 8,	24,	4,	288 },
	{ "overflow: 6 frames, 16 slots", 16,	24,	8,	144 },
	{ "overflow by one symbol",	 12,	24,	12,	24 }
};
#define NCCASE ((int)(sizeof(ccases) / sizeof(ccases[0])))

static int
run_chain(int level)
{
	int c;

	diff_begin(level == 0 ? "the constructed chain, level 0"
			      : "the constructed chain, level 2");

	for (c = 0; c < NCCASE; c++) {
		unsigned int on;
		unsigned int rn;
		int o;
		int r;
		unsigned int i;
		long tag = (long)c;

		seed_params(0x2000u + (unsigned int)c, ccases[c].K, c & 1);
		construct(0x3000u + (unsigned int)c, CHAIN_ALLOC);
		((V92BitsToSymbol *)btos[0])->nSymbols = ccases[c].nsym;
		((V92BitsToSymbol *)btos[1])->nSymbols = ccases[c].nsym;
		measure_mask();

		/*
		 * The mask is asserted rather than trusted: two pointers into
		 * the object and one into each sub-object is what construction
		 * leaves differing, and anything else means the fixture has
		 * stopped measuring what it thinks it is.
		 */
		diff_eq_int("the mask is small (%ld)",
			    (int)(maskcount > 0 && maskcount < 64), 1, tag);

		/*
		 * DIRTY WHAT RESET CLEARS, or its two stores of zero are
		 * invisible: a freshly constructed object already has both at
		 * zero, so a reset that dropped them would compare equal.
		 * Both sides get the SAME rubbish, which is what makes the
		 * differential comparison after the call the detector --
		 * the reference clears, and a mutated ours would not.
		 */
		((V92BitsToSymbol *)btos[0])->symbolsDone = 0x5a5a5a5au;
		((V92BitsToSymbol *)btos[1])->symbolsDone = 0x5a5a5a5au;
		((V92BitsToSymbol *)btos[0])->symbolsBlockSize = 0xa5a5a5a5u;
		((V92BitsToSymbol *)btos[1])->symbolsBlockSize = 0xa5a5a5a5u;
		((V92BitsToSymbol *)btos[0])->flag_1c = 0;
		((V92BitsToSymbol *)btos[1])->flag_1c = 0;

		our_btos_reset(btos[0], &params);
		ref_btos_reset(btos[1], &params);

		diff_eq_int("reset cleared the cursor (%ld)",
			    (long)((V92BitsToSymbol *)btos[1])->symbolsDone, 0,
			    tag);
		diff_eq_int("reset cleared the block size (%ld)",
			    (long)((V92BitsToSymbol *)btos[1])->symbolsBlockSize,
			    0, tag);
		diff_eq_int("reset raised the flag (%ld)",
			    (long)((V92BitsToSymbol *)btos[1])->flag_1c, 1,
			    tag);

		diff_eq_int("reset took K from the block (%ld)",
			    (long)((V92BitsToSymbol *)btos[1])->bitsPerFrame,
			    (long)params.K, tag);

		compare_all(ccases[c].name, tag);

		our_btos_setsz(btos[0], ccases[c].block);
		ref_btos_setsz(btos[1], ccases[c].block);

		lfsr = 0x4000u + (unsigned int)c;
		for (i = 0; i < NBITS; i++)
			bits[i] = (unsigned char)(nextbyte() & 1u);

		memset(chainout[0], 0x5a, sizeof(chainout[0]));
		memcpy(chainout[1], chainout[0], sizeof(chainout[0]));

		if (level == 2) {
			dsplib_debug_capture_reset();
			dsplibs_debug_level = ref_dsplibs_debug_level = 2;
			dsplib_debug_capture_on = 1;
		}

		/* Bits in, nothing out. */
		o = our_btos_pb(btos[0], bits, ccases[c].nbits);
		r = ref_btos_pb(btos[1], bits, ccases[c].nbits);
		diff_eq_int("process(bits, n) status agrees (%ld)", (long)o,
			    (long)r, tag);
		compare_all(ccases[c].name, 100 + tag);

		/*
		 * AND AGAIN, WITH THE CURSOR ALREADY OFF ZERO.  The first
		 * call runs from an empty staging buffer, where "append at
		 * `symbols + symbolsDone`" and "write at `symbols`" are the
		 * same instruction and the running total and a plain
		 * assignment are the same number.  Only a second call
		 * separates them.
		 */
		o = our_btos_pb(btos[0], bits + 1, ccases[c].nbits);
		r = ref_btos_pb(btos[1], bits + 1, ccases[c].nbits);
		diff_eq_int("the second process(bits, n) agrees (%ld)",
			    (long)o, (long)r, tag);
		compare_all(ccases[c].name, 150 + tag);

		/* Symbols out. */
		on = rn = 0;
		o = our_btos_ps(btos[0], on, chainout[0]);
		r = ref_btos_ps(btos[1], rn, chainout[1]);
		diff_eq_int("process(&n, out) status agrees (%ld)", (long)o,
			    (long)r, tag);
		diff_eq_int("process(&n, out) nbits agrees (%ld)", (long)on,
			    (long)rn, tag);
		diff_eq_int("process(&n, out) output agrees (%ld)",
			    memcmp(chainout[0], chainout[1],
				   sizeof(chainout[0])) == 0, 1, tag);
		compare_all(ccases[c].name, 200 + tag);

		/* Both halves at once, twice, so state carries. */
		on = rn = ccases[c].nbits;
		o = our_btos_pbs(btos[0], bits, on, chainout[0]);
		r = ref_btos_pbs(btos[1], bits, rn, chainout[1]);
		diff_eq_int("process(bits, &n, out) status agrees (%ld)",
			    (long)o, (long)r, tag);
		diff_eq_int("process(bits, &n, out) nbits agrees (%ld)",
			    (long)on, (long)rn, tag);
		diff_eq_int("process(bits, &n, out) output agrees (%ld)",
			    memcmp(chainout[0], chainout[1],
				   sizeof(chainout[0])) == 0, 1, tag);
		compare_all(ccases[c].name, 300 + tag);

		on = rn = ccases[c].nbits;
		o = our_btos_pbs(btos[0], bits + 3, on, chainout[0]);
		r = ref_btos_pbs(btos[1], bits + 3, rn, chainout[1]);
		diff_eq_int("second pass status agrees (%ld)", (long)o,
			    (long)r, tag);
		diff_eq_int("second pass nbits agrees (%ld)", (long)on,
			    (long)rn, tag);
		diff_eq_int("second pass output agrees (%ld)",
			    memcmp(chainout[0], chainout[1],
				   sizeof(chainout[0])) == 0, 1, tag);
		compare_all(ccases[c].name, 400 + tag);

		if (level == 2) {
			const char *a;
			const char *b;

			dsplib_debug_capture_on = 0;
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;

			a = dsplib_debug_capture_text(0);
			b = dsplib_debug_capture_text(1);

			diff_eq_int("transcript matches (%ld)",
				    strcmp(a, b) == 0, 1, tag);
			diff_eq_int("line counts match (%ld)",
				    (int)dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1), tag);
			diff_eq_int("not at the buffer limit (%ld)",
				    (int)(strlen(b) < 15000u), 1, tag);
		}

		teardown();
	}

	return diff_end();
}

/*
 * Anti-vacuity for part 2, and it is the counterpart of part 1's: the level-2
 * run must actually have printed one of the three error strings, or the
 * transcript comparison above is comparing two empty strings and passing.
 */
static int
run_transcript_bite(void)
{
	unsigned int on = 0;
	unsigned int lines;
	const char *b;

	diff_begin("the level-2 transcript is not empty");

	seed_params(0x7777u, 24, 1);
	construct(0x8888u, CHAIN_ALLOC);

	our_btos_reset(btos[0], &params);
	ref_btos_reset(btos[1], &params);

	/* Block size left at zero, which is the SIZE_NOT_SET arm. */
	dsplib_debug_capture_reset();
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;
	dsplib_debug_capture_on = 1;

	(void)our_btos_ps(btos[0], on, chainout[0]);
	(void)ref_btos_ps(btos[1], on, chainout[1]);

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	b = dsplib_debug_capture_text(1);
	lines = dsplib_debug_capture_lines(1);

	diff_eq_int("the reference printed something (%ld)",
		    (int)(lines >= 1), 1, 0);
	diff_eq_int("and it is the SIZE_NOT_SET line (%ld)",
		    (int)(strstr(b, "SIZE_NOT_SET") != 0), 1, 0);
	diff_eq_int("and both sides printed the same (%ld)",
		    strcmp(dsplib_debug_capture_text(0), b) == 0, 1, 0);

	teardown();

	return diff_end();
}

/*
 * The other two strings, reached by driving the three-argument overload into
 * the underflow and the overflow arms on purpose.
 */
static int
run_error_strings(void)
{
	int c;

	diff_begin("the other two error strings are reached");

	for (c = 0; c < 2; c++) {
		unsigned int on;
		const char *b;
		const char *want = c == 0 ? "BUFFER_UNDERFLOW"
					  : "BUFFER_OVERFLOW";

		seed_params(0x9999u + (unsigned int)c, 24, c & 1);
		/* c == 1 asks for far more symbols than the FIELD allows. */
		construct(0xaaaau + (unsigned int)c, CHAIN_ALLOC);
		((V92BitsToSymbol *)btos[0])->nSymbols = c == 0 ? 64u : 8u;
		((V92BitsToSymbol *)btos[1])->nSymbols = c == 0 ? 64u : 8u;

		our_btos_reset(btos[0], &params);
		ref_btos_reset(btos[1], &params);

		our_btos_setsz(btos[0], c == 0 ? 48u : 4u);
		ref_btos_setsz(btos[1], c == 0 ? 48u : 4u);

		lfsr = 0xbbbbu;
		{
			unsigned int i;

			for (i = 0; i < NBITS; i++)
				bits[i] = (unsigned char)(nextbyte() & 1u);
		}

		dsplib_debug_capture_reset();
		dsplibs_debug_level = ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		on = c == 0 ? 24u : 240u;
		(void)our_btos_pbs(btos[0], bits, on, chainout[0]);
		on = c == 0 ? 24u : 240u;
		(void)ref_btos_pbs(btos[1], bits, on, chainout[1]);

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;

		b = dsplib_debug_capture_text(1);

		diff_eq_int("the reference printed it (%ld)",
			    (int)(strstr(b, want) != 0), 1, (long)c);
		diff_eq_int("both sides printed the same (%ld)",
			    strcmp(dsplib_debug_capture_text(0), b) == 0, 1,
			    (long)c);

		teardown();
	}

	return diff_end();
}

/*
 * V92Transmitter::process on its own, so a difference in it is attributed to
 * it rather than to the class above.  The transmitter is built directly here
 * -- V92BitsToSymbol's constructor is not involved -- and the same mask
 * discipline applies.
 */
static unsigned char txobj[2][0x60 + GUARD] __attribute__((aligned(8)));

static int
run_tx_process(void)
{
	int c;

	diff_begin("V92Transmitter::process on its own");

	for (c = 0; c < NCCASE; c++) {
		unsigned int on = 0xdeadbeefu;
		unsigned int rn = 0xdeadbeefu;
		unsigned int i;
		int s;
		long tag = (long)c;

		seed_params(0xc000u + (unsigned int)c, ccases[c].K, c & 1);

		lfsr = 0xd000u + (unsigned int)c;
		fill(txobj[0], sizeof(txobj[0]));
		memcpy(txobj[1], txobj[0], sizeof(txobj[0]));

		our_tx_c1(txobj[0]);
		ref_tx_c1(txobj[1]);

		our_tx_reset(txobj[0], &params);
		ref_tx_reset(txobj[1], &params);

		/*
		 * THE ONE STATE THE CLASS ABOVE CANNOT PRODUCE, and the only
		 * trial that separates `bitsBuffered -= K` from
		 * `bitsBuffered = 0`.  Driven from V92BitsToSymbol the count
		 * starts at zero and rises one bit at a time, so it lands on
		 * K exactly and the two are the same statement.  Started ABOVE
		 * K -- which is what this poke does, identically on both sides
		 * -- the subtraction leaves a remainder and the clear does
		 * not, and every byte the next frame reads is shifted by it.
		 * Odd cases only, so the even ones still cover the ordinary
		 * path from zero.
		 */
		if ((c & 1) != 0 && ccases[c].K + 3 < V92TX_BUF08_BYTES) {
			((V92Transmitter *)txobj[0])->bitsBuffered =
				ccases[c].K + 3;
			((V92Transmitter *)txobj[1])->bitsBuffered =
				ccases[c].K + 3;
		}

		lfsr = 0xe000u + (unsigned int)c;
		for (i = 0; i < NBITS; i++)
			bits[i] = (unsigned char)(nextbyte() & 1u);

		memset(chainout[0], 0x33, sizeof(chainout[0]));
		memcpy(chainout[1], chainout[0], sizeof(chainout[0]));

		our_tx_process(txobj[0], bits, ccases[c].nbits, chainout[0],
			       on);
		ref_tx_process(txobj[1], bits, ccases[c].nbits, chainout[1],
			       rn);

		diff_eq_int("nout agrees (%ld)", (long)on, (long)rn, tag);
		diff_eq_int("the output agrees (%ld)",
			    memcmp(chainout[0], chainout[1],
				   sizeof(chainout[0])) == 0, 1, tag);
		diff_eq_int("bitsBuffered agrees (%ld)",
			    (long)((V92Transmitter *)txobj[0])->bitsBuffered,
			    (long)((V92Transmitter *)txobj[1])->bitsBuffered,
			    tag);
		diff_eq_int("convEncoderOutput agrees (%ld)",
			    (long)((V92Transmitter *)txobj[0])
				    ->convEncoderOutput,
			    (long)((V92Transmitter *)txobj[1])
				    ->convEncoderOutput, tag);
		diff_eq_int("modulusOut agrees (%ld)",
			    memcmp(((V92Transmitter *)txobj[0])->modulusOut,
				   ((V92Transmitter *)txobj[1])->modulusOut,
				   sizeof(((V92Transmitter *)txobj[0])
					  ->modulusOut)) == 0, 1, tag);
		diff_eq_int("the guard is untouched (%ld)",
			    memcmp(txobj[0] + 0x60, txobj[1] + 0x60,
				   GUARD) == 0, 1, tag);

		/*
		 * Anti-vacuity: a case that produced no samples at all would
		 * pass every comparison above without exercising a frame.
		 * Only the two that cannot fill K are allowed to be empty.
		 */
		if (ccases[c].nbits >= ccases[c].K)
			diff_eq_int("at least one frame came out (%ld)",
				    (int)(rn >= 12), 1, tag);

		for (s = 0; s < 2; s++)
			(s == 0 ? our_tx_d1 : ref_tx_d1)(txobj[s]);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	diff_begin("the class's map");
	diff_eq_int("sizeof(V92BitsToSymbol) is %ld",
		    (long)sizeof(V92BitsToSymbol), 0x20, 0x20);
	diff_eq_int("symbolsDone is at +0x%lx",
		    (long)offsetof(V92BitsToSymbol, symbolsDone), 0x10, 0x10);
	diff_eq_int("bitsPerFrame is at +0x%lx",
		    (long)offsetof(V92BitsToSymbol, bitsPerFrame), 0x14, 0x14);
	diff_eq_int("symbolsBlockSize is at +0x%lx",
		    (long)offsetof(V92BitsToSymbol, symbolsBlockSize), 0x18,
		    0x18);
	diff_eq_int("bitsBuffered is at +0x%lx",
		    (long)offsetof(V92Transmitter, bitsBuffered), 0x0c, 0x0c);
	diff_eq_int("modulusOut is at +0x%lx",
		    (long)offsetof(V92Transmitter, modulusOut), 0x10, 0x10);
	diff_eq_int("modulusOut is twelve words (%ld)",
		    (long)sizeof(((V92Transmitter *)0)->modulusOut), 48, 48);
	diff_eq_int("convEncoderOutput is at +0x%lx",
		    (long)offsetof(V92Transmitter, convEncoderOutput), 0x40,
		    0x40);
	rc |= diff_end();

	rc |= run_pure();
	rc |= run_tx_process();
	rc |= run_chain(0);
	rc |= run_chain(2);
	rc |= run_transcript_bite();
	rc |= run_error_strings();

	return rc;
}

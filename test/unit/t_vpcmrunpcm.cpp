/*
 * t_vpcmrunpcm.cpp -- `VPcmFloModem::runPcmModem` (.text+0xe430, 2,041 bytes)
 * against the blob.
 *
 * ===========================================================================
 * WHAT MAKES THIS TEST HARD IS THE GRAPH, NOT THE ARITHMETIC
 * ===========================================================================
 *
 * The function is three dispatches and about forty statements, but every one
 * of those statements reaches through a 32 KB object into a heap graph the
 * constructor built: two embedded modems, a modulator and a demodulator, a
 * phase 4 modulator two levels down, an echo canceller with two float arrays,
 * a V.92 CP block and a mapping-parameter block.  A fixture that seeded that
 * graph by hand would be seeding the thing it is trying to measure.
 *
 * So it does not.  BOTH SIDES ARE BUILT BY `VPCMXF_Create`, which
 * t_vpcmctor.cpp has already shown produces the same object on both, and the
 * trial then pokes ONLY the dozen fields that select an arm.  Everything else
 * -- allocation sizes, sub-object wiring, the parameter block derived from
 * the runtime record -- is whatever the real constructor left.
 *
 * ===========================================================================
 * THE COMPARISON SURFACE IS DISCOVERED TRANSITIVELY
 * ===========================================================================
 *
 * `runPcmModem` writes into at least five different blocks: the object
 * itself, the V92Modulator (`float_28`, and the phase codes its `exit*`
 * members set), the V92Phase4Modulator two levels down (`silenceRrnRequest`, `word_30`,
 * `word_38`, `e2uExtended`), the V92CP the packer fills, the V92ParamsInfo
 * the unpacker fills, and the `_tagModemParameters` and `struct v34_object`
 * outside the modem entirely.  Naming those by hand would be a list nobody
 * could keep right.
 *
 * `regions()` therefore walks the graph: three roots -- the modem, the V.34
 * object and the runtime record -- and then every 4-aligned word of every
 * region that is, ON BOTH SIDES, the base of a live `sysdep_malloc`.  A word
 * that is a live base on one side and not the other is itself a failure and
 * is reported.  That is t_vpcmctor.cpp's slot discovery made transitive,
 * which is what it takes to see two levels down.
 *
 * ===========================================================================
 * THE TWO SIDES CAN ONLY DISAGREE ABOUT AN ADDRESS
 * ===========================================================================
 *
 * Both graphs are built by the same constructor from the same inputs, so
 * every value in them that is computed from DATA is bit-identical.  A word
 * that differs therefore differs because it holds a pointer -- and that one
 * fact is what lets the comparison be word-wise and exemption-free rather
 * than a list of excluded offsets.  `compare_regions` carries the three cases
 * and their counts; the counts are printed on every run, because a comparison
 * whose exempt class quietly grew to the whole object would pass while
 * measuring nothing (findings F2400 and F3100).
 *
 * ===========================================================================
 * WHAT IS DELIBERATELY NOT DRIVEN, AND WHY
 * ===========================================================================
 *
 * `V90Modem::side` and `V92Modem::modemSide` are both held OUTSIDE the two
 * values their `progress` fans out on, so neither `V90Demodulator::progress`
 * nor `V92Modulator::progress` runs.  Three reasons, and the first is the
 * one that decides:
 *
 *   1. `V90Equalizer::process` is one of the seven entries in
 *      tools/gccdiverge.json -- x87 excess precision, green on the period
 *      compiler and 731 checks red on GCC 13.  Driving the real demodulator
 *      would drag that divergence into THIS binary, which would have to be
 *      declared, and a declared binary cannot carry a mutation suite
 *      (findings F2157 and F3002).  The cost of driving it is the whole
 *      mutation surface of this file.
 *   2. `V92Modulator::progress` WRITES `eventCode`, which is dispatch 3's
 *      selector.  With the real transmitter running, that axis could not be
 *      swept at all -- finding F7458's failure mode exactly.
 *   3. The fan-out itself is t_v90modprog's and t_v92modem's claim, already
 *      made; t_vpcmflomodem.cpp holds the same side outside {0, 1} for the
 *      same reason.
 *
 * WHAT THAT COSTS is stated rather than hidden: the four arguments
 * `runPcmModem` forwards to `V90Modem::progress` (`rxbits`, `*nrx`, the
 * receive buffer, `n`) and the four it forwards to `V92Modem::progress`
 * (`txbits`, `*nbits`, `out`, `n`) are not proved to be in that order by this
 * file, because neither callee reads them.  What IS proved about the same
 * buffers is that `in` and `n` reach `V92EchoCanceller::process`, which does
 * run for real, that `out` and `n` reach the scaling loop and
 * `updateEchoHistory`, and that the receive buffer really is +0x6c0c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"

#include "dsplib/debug.h"
#include "dsplib/modem_params.h"
#include "dsplib/v34fsk.h"
#include "dsplib/V90CP.h"
#include "dsplib/V90ConnectionEvaluator.h"
#include "dsplib/V90Demodulator.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Resampler.h"
#include "dsplib/V92CP.h"
#include "dsplib/V92EchoCanceller.h"
#include "dsplib/V92Jd.h"
#include "dsplib/V92Modulator.h"
#include "dsplib/V92Phase3Modulator.h"
#include "dsplib/V92Phase4Modulator.h"
#include "dsplib/VPcmFloModem.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void *VPCMXF_Create(int digitalSide, void *v34Object,
		    struct _tagModemParameters *dpRuntime,
		    unsigned int durationMs, int mode);
void VPCMXF_Delete(void *self);
void *ref_VPCMXF_Create(int digitalSide, void *v34Object,
			struct _tagModemParameters *dpRuntime,
			unsigned int durationMs, int mode);
void ref_VPCMXF_Delete(void *self);

int ref_runPcmModem(void *self, float *in, float *out, unsigned int n,
		    int *rxbits, int *nrx, int *txbits, int *nbits)
	asm("ref__ZN12VPcmFloModem11runPcmModemEPfS0_jPiS1_S1_S1_");

/*
 * NOT UNDER TEST HERE, AND CALLED AS FIXTURE.
 *
 * `V90Demodulator::enterPhase3`, which dispatch 3's `eventCode == 3` arm
 * reaches, ends by printing the Phase 2 record -- and that walks `L2`, which
 * a freshly constructed record holds as a null pointer.  The object's own way
 * of filling it is `setPhaseIIinfo`, which installs the modem's four float
 * arrays; t_vpcmflomodem.cpp is where that member is compared against the
 * blob, so calling it here is using a proved piece rather than seeding by
 * hand.  Each side calls its own.
 */
void ref_setPhaseIIinfo(void *self, int *info0, int rtd)
	asm("ref__ZN12VPcmFloModem14setPhaseIIinfoEPii");
}

/* ================================================================ the fixture */

#define FLO_SIZE	0x7f68

typedef char vpcmrun_is_0x7f68[(sizeof(VPcmFloModem) == FLO_SIZE) ? 1 : -1];

/*
 * The offsets the trials poke, spelled here rather than reached through the
 * class, because several of them are inside heap blocks this file has no
 * declaration for.  Each carries the disassembly site that reads it.
 */
#define OFF_BAUDALLOW	0x0217		/* v34BaudAllow, 0xe91f      */
#define OFF_FLAGS173A	0x173a		/* getConstelationSize, 0xe568 */
#define OFF_FLAG173D	0x173d		/* 0xe913                    */
#define OFF_MODEM	0x1758
#define OFF_DEMOD	0x175c		/* V90Modem::demodulator     */
#define OFF_MAPPING	0x1770		/* V90Modem::mappingParams   */
#define OFF_MAPPINGALT	0x1dc0		/* V90Modem::mappingParamsAlt */
#define OFF_CPINFO	0x2410		/* V90Modem::additionalCPinfo */
#define OFF_JD92	0x1768		/* V90Modem::jd92, 0xe576    */
#define OFF_CP		0x254c		/* V90Modem::cp, 0xeacb      */
#define OFF_CP_EXTENDEU	(OFF_CP + 0x12)	/* cmpb $0x0,0x255e, 0xeae3  */
#define OFF_V90PARAMS	0x610c		/* V90Modem::params        */
#define OFF_SIDE	0x6114		/* V90Modem::side, +0x49bc   */
#define OFF_BYTE6118	0x6118
#define OFF_BYTE6119	0x6119		/* cmpb $0x0,0x6119, 0xe85a  */
#define OFF_LAYOUT	0x6120		/* info0Layout, 0xe470       */
#define OFF_V92MODEM	0x6124
#define OFF_V92MODULATOR (OFF_V92MODEM + 0x000)
#define OFF_V92MAPPING	(OFF_V92MODEM + 0xaa0)	/* the unpacker's target     */
#define OFF_V92CP	(OFF_V92MODEM + 0xaa4)	/* the packer's target       */
#define OFF_V92SIDE	(OFF_V92MODEM + 0xaa8)
#define OFF_BLOCK6C0C	0x6c0c

/* The V92EchoCanceller, embedded at +0x6bd0; include/dsplib/VPcmFloModem.h. */
#define OFF_ECHOCANCELLER	0x6bd0
#define ECHO_FILTERLENGTH	0x14	/* V92EchoCanceller::filterLength */
#define ECHO_HISTORYALLOC	0x1c	/* V92EchoCanceller::historyAlloc */
#define ECHO_COEFF		0x20	/* V92EchoCanceller::echoCoeff    */
#define ECHO_HISTORY		0x24	/* V92EchoCanceller::echoHistory  */
#define OFF_WORD7F60	0x7f60
#define OFF_WORD7F64	0x7f64

/* Inside V90Demodulator; include/dsplib/V90Demodulator.h. */
#define DEM_INPHASE3	0x34
#define DEM_WORD3C	0x3c

/* Inside V92Modulator; include/dsplib/V92Modulator.h. */
#define MOD_FLOAT28	0x28
#define MOD_PHASE	0x2c
#define MOD_WORD34	0x34

/* V90Parameters::ENABLE_ERROR_CORRECTION_RRN, read at 0xe73a. */
#define PARAM_EC_RRN	0x480

/*
 * Two more of the parameter block, and they are here for a reason that is
 * not this function's.  Dispatch 3's `eventCode == 3` arm calls
 * `V90Demodulator::enterPhase3`, which reaches `V90PreFilter::selectFilter`;
 * with `PRE_FILTER_GAIN` at -1 that runs `autoSelection`, which reads the
 * Phase 2 record's `L2` array through a pointer a freshly constructed
 * demodulator has not filled.  Holding the gain off -1 and the connection
 * type at 0 takes the arm that does not, which keeps the trial inside
 * defined behaviour (D561) without changing anything `runPcmModem` reads.
 */
#define PARAM_CONNECTION_TYPE	0x0c
#define PARAM_PRE_FILTER_GAIN	0x4c

#define NSAMP		48		/* durationMs 5 -> 5 * 9.6 + 0.5 */
#define DURATION_MS	5

#define MP_SLOT		(sizeof(struct _tagModemParameters))

static struct v34_object v34obj[2];
static unsigned char mparams[2][MP_SLOT] __attribute__((aligned(8)));
static unsigned char *base[2];

#define MPARAMS(s)	((struct _tagModemParameters *)mparams[s])

/* The four buffers the function is handed, one set per side. */
static float sig_in[2][NSAMP];
static float sig_seed[NSAMP];
static float sig_out[2][NSAMP];
static int rxbits[2][NSAMP];
static int txbits[2][NSAMP];
static int nrx[2];
static int nbits[2];

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)((lfsr >> 3) | 1u);
}

/* ==================================================== the discovered regions */

#define MAXREG		512
#define MAXLIVE		4096

struct region {
	unsigned char	*a;
	unsigned char	*b;
	unsigned	size;
	unsigned char	*pre_a;
	unsigned char	*pre_b;
	/*
	 * THE MODELLED FLOAT SPAN, and 0 means "none".  A word inside
	 * [foff, foff + fcount*4) is compared AS A FLOAT with the fixture's
	 * modern-tier budget rather than byte-for-byte; every other word in
	 * the same region stays exact.  See mark_float_regions() for why the
	 * spans are identified by the pointer path that discovered the region
	 * rather than by a hard-coded region index.
	 */
	unsigned	foff;
	unsigned	fcount;
};

static struct region reg[MAXREG];
static int nreg;
static int p4region = -1;	/* see P4M_PATTERN below */
static void *live[MAXLIVE];
static int nlive;
static int region_overflow;
static int lopsided;

static int
is_live_base(const void *p)
{
	int i;

	if (p == 0)
		return 0;
	for (i = 0; i < nlive; i++)
		if (live[i] == p)
			return 1;
	return 0;
}

static int
already(const void *p)
{
	int i;

	for (i = 0; i < nreg; i++)
		if (reg[i].a == p || reg[i].b == p)
			return 1;
	return 0;
}

static void
add_region(void *a, void *b, unsigned size)
{
	struct region *r;

	if (nreg >= MAXREG) {
		region_overflow = 1;
		return;
	}
	r = &reg[nreg++];
	r->a = (unsigned char *)a;
	r->b = (unsigned char *)b;
	r->size = size;
	r->pre_a = (unsigned char *)malloc(size);
	r->pre_b = (unsigned char *)malloc(size);
	r->foff = 0;
	r->fcount = 0;
}

static void
free_regions(void)
{
	int i;

	for (i = 0; i < nreg; i++) {
		free(reg[i].pre_a);
		free(reg[i].pre_b);
	}
	nreg = 0;
}

/*
 * Walk the graph.  The three roots are the two constructed modems, the two
 * V.34 objects and the two runtime records; everything else is reached by
 * finding a word that is the base of a live allocation on both sides.
 */
static void
discover(void)
{
	int r;
	unsigned off;

	region_overflow = 0;
	lopsided = 0;
	nlive = harness_alloc_live_set(live, MAXLIVE);

	add_region(base[0], base[1], FLO_SIZE);
	add_region(&v34obj[0], &v34obj[1], sizeof(struct v34_object));
	add_region(mparams[0], mparams[1], (unsigned)MP_SLOT);

	for (r = 0; r < nreg; r++) {
		for (off = 0; off + 4 <= reg[r].size; off += 4) {
			void *pa, *pb;
			unsigned sa, sb;

			memcpy(&pa, reg[r].a + off, sizeof pa);
			memcpy(&pb, reg[r].b + off, sizeof pb);

			if (is_live_base(pa) != is_live_base(pb)) {
				lopsided++;
				continue;
			}
			if (!is_live_base(pa))
				continue;
			if (already(pa) || already(pb))
				continue;

			sa = harness_alloc_reqsize(pa);
			sb = harness_alloc_reqsize(pb);
			diff_eq_int("paired allocation lengths", sa, sb, off);
			if (sa == sb && sa != 0)
				add_region(pa, pb, sa);
		}
	}

	/* Which region is the phase 4 modulator; see P4M_PATTERN. */
	p4region = -1;
	{
		unsigned char *mod;
		void *p4;
		int k;

		memcpy(&mod, base[0] + OFF_V92MODULATOR, sizeof mod);
		memcpy(&p4, mod + 0x48, sizeof p4);
		for (k = 0; k < nreg; k++)
			if (reg[k].a == p4)
				p4region = k;
	}
}

/* Snapshot both sides, before the call. */
static void
snapshot(void)
{
	int r;

	for (r = 0; r < nreg; r++) {
		memcpy(reg[r].pre_a, reg[r].a, reg[r].size);
		memcpy(reg[r].pre_b, reg[r].b, reg[r].size);
	}
}

/*
 * Where a pointer lands, as (region, offset).  -1 for "not in any discovered
 * region", which is what a pointer into static data gives.
 */
static int
locate(const void *p, int side, unsigned *off)
{
	unsigned long v = (unsigned long)p;
	int k;

	for (k = 0; k < nreg; k++) {
		unsigned char *b = side == 0 ? reg[k].a : reg[k].b;

		if (v >= (unsigned long)b
		    && v < (unsigned long)b + reg[k].size) {
			*off = (unsigned)(v - (unsigned long)b);
			return k;
		}
	}
	return -1;
}

/*
 * Modelled binary32 words use diff_eq_float_word: raw words in the period
 * build, a scoped |a-b| <= 1e-4 + 1e-6*|reference| policy in the modern
 * build. This owner-authorized fixture policy is provisional, not an
 * independently established algorithmic error bound. Input integrity and
 * unrelated output comparisons do not receive this allowance.
 */

/*
 * The shared classifier validates both pointer paths, allocation lengths,
 * span bounds and alignment. Unknown or mismatched paths fail closed.
 */

/*
 * THE FLOAT REGIONS, AND WHY THEY ARE FOUND BY POINTER PATH RATHER THAN BY
 * INDEX.
 *
 * The rule in `compare_regions` was written for the period tier, where the
 * two sides can differ only in an address.  On the modern tier that is false
 * in exactly one way: the sinc/FIR coefficient DESIGN diverges (finding
 * F11363 -- the object narrows `sinc<float>` to binary32 and GCC 14 does not),
 * and every float buffer downstream of a designed resampler inherits the
 * difference.  The regions below are those buffers, each reached from a field
 * whose type the headers already establish, so a heap address moving between
 * runs cannot make the marking wrong:
 *
 *   echoHistory            float *echoHistory, V92EchoCanceller +0x24
 *   echo arma's m_yhist    float *m_yhist,    FloatARMA +0x0c
 *   FloatARMA m_fwd/m_fbk  float,             FloatARMA +0x2c, two words
 *   demod resampler coeffs float *coeffs,     Resampler +0x04, through
 *                          V90Demodulator +0x98 (its embedded V90Resampler)
 *   modulator resampler    float *coeffs,     Resampler +0x04, through
 *     coeffs               V92Modem.modulator +0x50 (its ResamplerTimingOffset)
 *
 * Everything else -- the object's own counters, flags and pointers, the
 * echoed float buffers' lengths, and the two genuinely borrowed static tables
 * (the entrance filter's coefficients and the demodulator's prefilter
 * coefficients and resampler vptr) -- stays byte-exact, which is the negative
 * control.
 */
#include "region_float_graph.h"
static void mark_float_regions(void)
{
	mark_float_graph(OFF_ECHOCANCELLER, OFF_DEMOD, OFF_V92MODEM);
}

/*
 * Compare, WORD BY WORD, and the rule rests on one fact about this fixture:
 *
 *   THE ONLY THING THE TWO SIDES CAN LEGITIMATELY DISAGREE ABOUT IS AN
 *   ADDRESS.  Every block is built by the same constructor from the same
 *   inputs, so any value computed from data is bit-identical on both sides.
 *   A word that differs therefore differs because it holds a pointer, and
 *   there are exactly three cases:
 *
 *     CORRESPONDING  both values land inside a discovered region, and the
 *                    claim is that they land in the SAME region at the SAME
 *                    offset.  This is a real check: a callee that stored the
 *                    wrong sub-object's address fails it.
 *     UNRESOLVED     neither lands in a discovered region -- our static table
 *                    against the blob's copy of it, which can never be equal
 *                    and which nothing here can pair up.
 *                    `V92Phase4Modulator::pattern` (+0x1a8) is the one this
 *                    file meets: `V92Modulator::initiateRRN` resets the phase
 *                    4 modulator and the reset installs one of the static
 *                    patterns.  The claim left is that both sides wrote it or
 *                    neither did, and the count is printed.
 *     otherwise      a failure, including one side null and the other not.
 *
 * Equal words are not examined at all, so the mask is a consequence of the
 * data rather than a list, and it cannot grow silently: the three counts are
 * printed on every run.
 */
static long words_equal;
static long words_corresponded;
static long words_unresolved;
static long words_static;
static long words_float;
static int nregion_last;

/*
 * THE ONE WORD THAT IS EXCUSED BY NAME, AND WHY IT IS BY NAME.
 *
 * `V92Phase4Modulator::pattern` (+0x1a8) is set by the phase 4 modulator's
 * reset to one of the STATIC bit patterns, so our side holds an address in
 * our `.data` and the blob's holds one in its own copy.  Nothing can pair
 * those up, and the claim left is that both sides wrote it or neither did.
 *
 * IT IS CURRENTLY DEAD, and the run prints so: the trial set reaches
 * `initiateRRN` only with the transmitter outside its data phase, where it
 * returns before resetting anything, so `pattern installs` reads 0.  It is
 * kept rather than deleted because it was reached by an earlier trial set and
 * will be again; the printed count is what stops it becoming a silent
 * exemption (finding F134's argument).
 *
 * IT IS NAMED RATHER THAN INFERRED, AND THE MUTATION SET IS WHY.  A general
 * rule -- "a differing word neither side can resolve is a static pointer" --
 * was tried first and it silently swallowed SEVEN real mutations, because
 * `progressState` and `retrainLatch` share a word with two bytes the constructor
 * leaves uninitialised, so a mutant latching 2 where the object latches 3
 * produced two large differing values that the rule could not tell from a
 * pair of addresses.  Tightening the rule to "both values look like
 * addresses" did not help for the same reason.  There is no property of a
 * WORD that separates the two cases; only knowing which field it is does.
 * Finding F7521's shape: the suite measured the comparator, not the code.
 */
#define P4M_PATTERN	0x1a8

static void
compare_regions(long trial)
{
	int r;
	int reported = 0;

	for (r = 0; r < nreg; r++) {
		unsigned i;

		for (i = 0; i + 4 <= reg[r].size; i += 4) {
			unsigned int va, vb, pva, pvb;
			void *pa, *pb;
			unsigned oa = 0, ob = 0;
			int ka, kb;
			int bad;

			memcpy(&va, reg[r].a + i, 4);
			memcpy(&vb, reg[r].b + i, 4);
			if (va == vb) {
				words_equal++;
				continue;
			}
			/*
			 * A modelled float word is compared as a float with
			 * the fixture's budget BEFORE the pointer/static
			 * cases. On the period tier differing words are raw
			 * integer failures, including signed zero and NaN payloads.
			 * On the modern tier it is what
			 * separates a designed-coefficient or echo-history
			 * last-bit difference from a decision.
			 */
			if (reg[r].fcount
			    && i >= reg[r].foff
			    && i < reg[r].foff + reg[r].fcount * 4) {
				float fa, fb;
				int failures = diff_failures;
				char label[80];

				memcpy(&fa, &va, 4);
				memcpy(&fb, &vb, 4);
				snprintf(label, sizeof label, "region %d float offset %u", r, i);
				diff_eq_float_word(label, va, vb, 1.0e-4, 1.0e-6, trial);
				if (diff_failures == failures) {
					words_float++;
				} else if (reported < 8) {
					reported++;
					printf("    trial %ld region %d float "
					       "offset %u: ours %.9g blob %.9g"
					       "\n", trial, r, i,
					       (double)fa, (double)fb);
					fflush(stdout);
				}
				continue;
			}
			memcpy(&pva, reg[r].pre_a + i, 4);
			memcpy(&pvb, reg[r].pre_b + i, 4);

			memcpy(&pa, reg[r].a + i, sizeof pa);
			memcpy(&pb, reg[r].b + i, sizeof pb);
			ka = locate(pa, 0, &oa);
			kb = locate(pb, 1, &ob);

			if (ka >= 0 && kb >= 0) {
				bad = ka != kb || oa != ob;
				if (!bad)
					words_corresponded++;
			} else if (pva != pvb && va == pva && vb == pvb) {
				/*
				 * Divergent before the call and untouched by
				 * it: a borrowed coefficient table the
				 * CONSTRUCTOR installed, ours against the
				 * blob's copy.  t_vpcmctor.cpp is where those
				 * are pinned by name; nothing here wrote it,
				 * which is the whole claim this file needs.
				 */
				bad = 0;
				words_static++;
			} else if (r == p4region && i == P4M_PATTERN) {
				bad = (va != pva) != (vb != pvb);
				if (!bad)
					words_unresolved++;
			} else {
				bad = 1;
			}

			diff_eq_int("region %ld word", bad, 0, (long)r);
			if (bad && reported < 8) {
				reported++;
				printf("    trial %ld region %d offset %u: "
				       "ours 0x%08x (was 0x%08x) blob 0x%08x "
				       "(was 0x%08x) ka %d/%u kb %d/%u\n",
				       trial, r, i, va, pva, vb, pvb,
				       ka, oa, kb, ob);
				fflush(stdout);
			}
		}

		/* The tail, if the block is not a whole number of words. */
		for (; i < reg[r].size; i++)
			diff_eq_int("region %ld tail byte",
				    reg[r].a[i] != reg[r].b[i], 0, (long)r);
	}
}

/* ======================================================== the trial matrix */

struct trial {
	const char	*what;
	unsigned char	b6118;
	int		layout;
	unsigned	w3c;
	unsigned	w34;
	int		phase;
	unsigned char	b6119;
	unsigned	w7f60;
	unsigned	w7f64;
	unsigned	inPhase3;
	int		ecRRN;
	unsigned char	extendEu;
	unsigned char	cfg3;
	unsigned	level;
	/*
	 * THE PHASE 4 MODULATOR'S OWN GATES, and they are an axis because
	 * without them they are a CONSTANT.  Every `recived*` member of
	 * V92Phase4Modulator returns immediately unless `state`, `flag_20`,
	 * `word_1c4`, `word_38` and `symbolCount % word_1b0` are right, so a
	 * sweep that left them at what the constructor produced would call
	 * twelve different members and observe nothing from any of them --
	 * finding F7458's failure mode, and the mutation set is what showed it
	 * happening (eight member-swap mutations uncaught).
	 */
	int		p4state;
	unsigned	p4symbolCount;
	unsigned	p4word1b0;
	unsigned	p4word38;
	/*
	 * The recovered timing offset, which five arms log.  Zero is the
	 * constructor's, and at zero the scale factor and the width of the
	 * conversion are both invisible; 5000 ppm puts `ppm * 10` past a
	 * short, which is what makes the object's `fistps` measurable.
	 */
	float		timingOffset;
	unsigned char	jdFirst;
	unsigned char	jdSecond;
	int		ecState;
	/*
	 * `p3state`, sentinel -1 for "leave it": `V92Modulator::exitJa`,
	 * `::exitSuSecond` and `::exitTRN1uSecond` each guard on
	 * `phase3Modulator->state` being exactly V92P3M_STATE_JA/SU_SECOND/
	 * TRN1U_SECOND before doing anything observable, and the constructor
	 * does not leave it at any of the three -- so word_3c 1/7/0x14's own
	 * exit* call is a guaranteed no-op unless a trial asks for the
	 * matching state by name.
	 */
	int		p3state;
	/*
	 * `rxSeedOn`/`rxSeedVal`: pre-load block_6c0c's rx[0] before the call.
	 * The constructor already clears it, so testing "the non-ramp arm
	 * clears rx[0]" needs a STALE non-zero value there first, or clearing
	 * an already-zero word is invisible.
	 */
	unsigned char	rxSeedOn;
	float		rxSeedVal;
	/*
	 * Compare the debug transcript for this trial.  NOT the default: the
	 * pre-existing "level 2" trials print `V90Modem::progress`'s "Illegal
	 * modemSide" diagnostic (side is deliberately 2, see the file header),
	 * and the blob's copy of it carries one more blank line than ours --
	 * a real, pre-existing formatting difference this suite never had a
	 * way to see before and which is not one of the seven mutations this
	 * batch is closing.  Comparing transcripts everywhere would fail
	 * `make one` on that unrelated gap instead of the two claims this
	 * field exists for.
	 */
	unsigned char	cmpTranscript;
};

/*
 * `w3c` 0x36 is one past the table and takes the `ja` at 0xe50b, which is the
 * only way the default arm is reached other than through one of its 33 own
 * entries.  `b6118` 5 and 6 are the same claim for the first table's `ja`.
 */
#define TAIL	, 0, 0, 1, 0, 0.0f, 3, 7, V92_ECHO_FILTER_ONLY, -1, 0, 0.0f, 0
/* TAIL with `p3state` set, for the three exit* guards. */
#define TAILP3(st) \
	, 0, 0, 1, 0, 0.0f, 3, 7, V92_ECHO_FILTER_ONLY, (st), 0, 0.0f, 0
/* TAIL with block_6c0c's rx[0] pre-loaded, for the non-ramp clear. */
#define TAILRX(val) \
	, 0, 0, 1, 0, 0.0f, 3, 7, V92_ECHO_FILTER_ONLY, -1, 1, (val), 0
/* TAIL with the debug transcript compared, for the two print-only claims. */
#define TAILCMP \
	, 0, 0, 1, 0, 0.0f, 3, 7, V92_ECHO_FILTER_ONLY, -1, 0, 0.0f, 1
#define D(w3c)	{ "word_3c", 0, 1, (w3c), 0, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL }

static const struct trial trial_v[] = {
	/*
	 * Dispatch 1 alone: `info0Layout` zero, so the function returns
	 * whatever the first table seeded and touches nothing else.  Case 3
	 * takes the `ret = 2` arm without dereferencing the demodulator.
	 */
	{ "b6118 gate", 0, 0, 0, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "b6118 gate", 1, 0, 0, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "b6118 gate", 2, 0, 0, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "b6118 gate", 3, 0, 0, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "b6118 gate", 4, 0, 0, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "b6118 gate", 5, 0, 0, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "b6118 gate", 6, 0, 0, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },

	/* Dispatch 1 with the gate open, so the whole body runs. */
	{ "b6118 open", 0, 1, 0, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "b6118 open", 1, 1, 0, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "b6118 open", 2, 1, 0, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "b6118 open", 4, 1, 0, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "b6118 open", 5, 1, 0, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },

	/*
	 * Case 3's three exits: the retrain, the phase-4 test failing, and
	 * the parameter test failing.  All three must move `ret` and only the
	 * first may move `progressState`.
	 */
	{ "b6118=3 retrain",	3, 1, 0, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "b6118=3 not p4",	3, 1, 0, 0, 3, 1, 0, 0, 3, 1, 1, 0, 0 TAIL },
	{ "b6118=3 no param",	3, 1, 0, 0, 3, 1, 0, 0, 4, 0, 1, 0, 0 TAIL },

	/* Dispatch 2, every case label and one past the end of the table. */
	D(0x00), D(0x01), D(0x02), D(0x03), D(0x04), D(0x05), D(0x06),
	D(0x07), D(0x08), D(0x09), D(0x0a), D(0x0b), D(0x0c), D(0x0d),
	D(0x0e), D(0x0f), D(0x10), D(0x11), D(0x12), D(0x13), D(0x14),
	D(0x15), D(0x16), D(0x17), D(0x18), D(0x19), D(0x1a), D(0x1b),
	D(0x1c), D(0x1d), D(0x1e), D(0x1f), D(0x20), D(0x21), D(0x22),
	D(0x23), D(0x24), D(0x25), D(0x26), D(0x27), D(0x28), D(0x29),
	D(0x2a), D(0x2b), D(0x2c), D(0x2d), D(0x2e), D(0x2f), D(0x30),
	D(0x31), D(0x32), D(0x33), D(0x34), D(0x35), D(0x36),

	/* The inner branches of dispatch 2, taken the other way. */
	{ "0x16, no 6119",	0, 1, 0x16, 0, 3, 0, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "0x22, not phase 3",	0, 1, 0x22, 0, 2, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "0x23, not phase 3",	0, 1, 0x23, 0, 2, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "0x24, not phase 3",	0, 1, 0x24, 0, 2, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "0x25, not phase 3",	0, 1, 0x25, 0, 2, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "0x2d, extendEu 0",	0, 1, 0x2d, 0, 3, 1, 0, 0, 4, 1, 0, 0, 0 TAIL },
	{ "0x07, phase2 set",	0, 1, 0x07, 0, 3, 1, 0, 0, 4, 1, 1, 0xff, 0 TAIL },
	{ "0x07, phase2 clear",	0, 1, 0x07, 0, 3, 1, 0, 0, 4, 1, 1, 0xfd, 0 TAIL },

	/*
	 * THE THREE EXIT* GUARDS, OPENED BY NAME.  `V92Modulator::exitJa`,
	 * `::exitSuSecond` and `::exitTRN1uSecond` each return at once unless
	 * `phase3Modulator->state` is exactly V92P3M_STATE_JA/SU_SECOND/
	 * TRN1U_SECOND, which nothing else in this fixture ever sets -- so
	 * without these three, dropping any of the three calls is invisible.
	 */
	{ "0x01, phase3=JA", 0, 1, 0x01, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0
	  TAILP3(V92P3M_STATE_JA) },
	{ "0x07, phase3=SuSecond", 0, 1, 0x07, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0
	  TAILP3(V92P3M_STATE_SU_SECOND) },
	{ "0x14, phase3=TRN1uSecond", 0, 1, 0x14, 0, 3, 1, 0, 0, 4, 1, 1, 0, 0
	  TAILP3(V92P3M_STATE_TRN1U_SECOND) },

	/* Dispatch 3, over and around {2, 3, 10}. */
	{ "w34 0",	0, 1, 0, 0,	3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "w34 1",	0, 1, 0, 1,	3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "w34 2",	0, 1, 0, 2,	3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "w34 3",	0, 1, 0, 3,	3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	/*
	 * THE V.90 FALLBACK ARM, AND WHAT IT TAKES TO REACH IT.
	 * `V90Demodulator::enterPhase3` sets `word_3c = 0` -- so the re-read
	 * at 0xe710 can only see 0x20 if the demodulator was ALREADY in phase
	 * 3, which is the early return at the top of that member.  Every
	 * other trial holds `inPhase3` at 4, and with it there the arm is
	 * unreachable and eight statements of this function go unmeasured;
	 * the mutation set is what said so.
	 */
	{ "w34 3 + 0x20", 0, 1, 0x20, 3, 3, 1, 0, 0, 1, 1, 1, 0, 0 TAIL },
	{ "w34 3 + 0x20 not p3", 0, 1, 0x20, 3, 3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "w34 4",	0, 1, 0, 4,	3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "w34 9",	0, 1, 0, 9,	3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "w34 10",	0, 1, 0, 10,	3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },
	{ "w34 11",	0, 1, 0, 11,	3, 1, 0, 0, 4, 1, 1, 0, 0 TAIL },

	/* The echo-canceller sentinel ramp, both sides of every test in it. */
	{ "ramp off",	0, 1, 0, 0, 3, 1, 0x20, 0xaa, 4, 1, 1, 0, 0 TAIL },
	{ "ramp step",	0, 1, 0, 0, 3, 1, 0x21, 0xaa, 4, 1, 1, 0, 0 TAIL },
	{ "ramp at end", 0, 1, 0, 0, 3, 1, 0x21, 0xb1, 4, 1, 1, 0, 0 TAIL },
	{ "ramp near end", 0, 1, 0, 0, 3, 1, 0x21, 0xb0, 4, 1, 1, 0, 0 TAIL },
	/*
	 * THE NON-RAMP ARM'S CLEAR, WITH SOMETHING TO CLEAR.  The constructor
	 * already zeroes block_6c0c, so rx[0] is 0.0f before the call whether
	 * or not `rx[0] = 0.0f;` runs; pre-loading the sentinel here is what
	 * makes dropping that store observable -- the mode is 0x20, not
	 * ramp, so the store (or its absence) is the only thing standing
	 * between the sentinel BYPASS arm and the real filter.
	 */
	{ "rx0 stale sentinel", 0, 1, 0, 0, 3, 1, 0x20, 0, 4, 1, 1, 0, 0
	  TAILRX(177.0f) },

	/*
	 * The five gated diagnostics, driven at level 2 so the print sites
	 * execute.  What comes out is not compared -- the transcript tier is
	 * not this file's -- but a gate nobody enters is a gate nobody has
	 * measured (docs/method/gates.md).
	 */
	{ "level 2, 0x01", 0, 1, 0x01, 0, 3, 1, 0, 0, 4, 1, 1, 0, 2 TAIL },
	{ "level 2, 0x06", 0, 1, 0x06, 0, 3, 1, 0, 0, 4, 1, 1, 0, 2 TAIL },
	{ "level 2, 0x07", 0, 1, 0x07, 0, 3, 1, 0, 0, 4, 1, 1, 0, 2 TAIL },
	{ "level 2, w34 2", 0, 1, 0, 2, 3, 1, 0, 0, 4, 1, 1, 0, 2 TAIL },
	{ "level 2, w34 3", 0, 1, 0, 3, 3, 1, 0, 0, 4, 1, 1, 0, 2 TAIL },
	/*
	 * "level 2, fallback" IS THE FALLBACK ROW ABOVE ("w34 3 + 0x20"),
	 * PRINT-COMPARED.  `TAILCMP` is what turns the transcript check on
	 * for it; the region walk cannot see this one at all -- see
	 * `test/mutations/vpcmrunpcm.json`'s own note.
	 */
	{ "level 2, fallback", 0, 1, 0x20, 3, 3, 1, 0, 0, 1, 1, 1, 0, 2
	  TAILCMP },
	/*
	 * THE FPE GUARD, AT THE LEVEL WHERE ITS OWN MESSAGE PRINTS.  `phase`
	 * is 2 (not V92MOD_PHASE_DATA), so both `initiateFPE`'s call-site
	 * guard in `runPcmModem` and its own internal one are closed; edprintf
	 * "requested but NOT approved" is the one thing that differs when the
	 * OUTER guard alone is dropped, so it needs `run()`'s transcript
	 * compare, not the region walk.
	 */
	{ "level 2, 0x24 not phase 3", 0, 1, 0x24, 0, 2, 1, 0, 0, 4, 1, 1, 0, 2
	  TAILCMP }
};

#define NTRIAL	((int)(sizeof(trial_v) / sizeof(trial_v[0])))

/*
 * AND THE SAME 55-VALUE SWEEP AGAIN, FOUR TIMES, WITH THE CALLEES' OWN GATES
 * SOMEWHERE ELSE.
 *
 * A `case` label is only measured through what its arm DOES, and twelve of
 * dispatch 2's arms do nothing but call one member of V92Phase4Modulator --
 * whose members all begin by testing `state`, `word_38`, `flag_20`,
 * `word_1c4` and `symbolCount % word_1b0`.  Left at the constructor's values
 * every one of them returns immediately, so swapping two of them changes
 * nothing observable and the mutation set said so.  These four presets put
 * the modulator where different members act, and the timing offset where the
 * five logging arms produce different numbers.
 */
struct p4preset {
	int		state;
	unsigned	symbolCount;
	unsigned	word1b0;
	unsigned	word38;
	float		timingOffset;
	int		ecState;
};

static const struct p4preset p4_v[] = {
	{ V92P4M_STATE_TRN2U_SECOND, 2400, 12, 7,  5000.0f,
	  V92_ECHO_FILTER_ONLY },
	{ V92P4M_STATE_SUV,          2400, 12, 7,   -0.5f,
	  V92_ECHO_COUNT_DELAY },
	{ V92P4M_STATE_CPU,          2401, 12, 7,    0.0f,
	  V92_ECHO_FILTER_ONLY },
	/*
	 * A GEOMETRIC SPREAD, and it is not decoration.
	 * `getTimingOffsetPPM` is `1e6f * timingOffset / ppmScale`, so a
	 * whole-number offset puts the result in the millions and `fistps`
	 * answers the x87 indefinite for `ppm * 10` and for `ppm * 1` alike
	 * -- the two compare equal and the scale factor is unmeasurable.
	 * These four span eight decades so that at least one trial lands with
	 * `ppm * 10` inside a short and `ppm` well inside it.
	 */
	{ V92P4M_STATE_TRN2U_SECOND, 2401, 12, 7,  0.0015f,
	  V92_ECHO_FAST_TRAINING }
};

#define NP4	((int)(sizeof(p4_v) / sizeof(p4_v[0])))
#define NW3C	0x37
#define NSWEEP	(NP4 * NW3C)
#define NALL	(NTRIAL + NSWEEP)

/* Build trial `k` of the synthesised sweep on top of the D() defaults. */
static void
sweep_trial(struct trial *t, int k)
{
	const struct p4preset *q = &p4_v[k / NW3C];
	static const struct trial base_trial = D(0);

	*t = base_trial;
	t->what = "sweep";
	t->w3c = (unsigned)(k % NW3C);
	t->p4state = q->state;
	t->p4symbolCount = q->symbolCount;
	t->p4word1b0 = q->word1b0;
	t->p4word38 = q->word38;
	t->timingOffset = q->timingOffset;
	t->ecState = q->ecState;
}

/* ================================================================== the run */

/*
 * THE TWO MAPPING BLOCKS ARE SEEDED DIFFERENTLY, AND THAT IS THE POINT.
 *
 * Arms 0x14 and 0x19 differ in exactly one thing: the first packs the CP
 * message out of `V90Modem::mappingParams` (+0x1770) and the second out of
 * `mappingParamsAlt` (+0x1dc0).  V90Modem.h records that findings F1301 and
 * F1307 are two batches that shipped that pair the wrong way round, and that
 * the only way a swap becomes visible is to drive it through the callee.  So
 * every field `setV92CPpckFromParamsInfo` reads is given a different value in
 * the two blocks, and the V92CP the packer fills is inside the compared
 * region set.
 *
 * The lengths are held at 4 and the table bytes below 0x80 because
 * `getConstellationMask` shifts a table byte right by four IN AN 8-BIT
 * REGISTER and indexes an eight-entry mask with the result: a byte of 0x80 or
 * more writes past the mask, which is the object's own behaviour and D-noted
 * where it is written, but a trial that reaches it is not a trial (D561).
 * The constructor leaves `constellationSize` at whatever the parameter
 * derivation put there, which is not bounded by anything this file controls.
 */
#define SEED_CONSTEL_LEN	4

static void
seed_mapping(unsigned char *blk, int tag)
{
	V90MappingParams *p = (V90MappingParams *)blk;
	int c, j;

	p->word_0 = (unsigned int)(0x30 + tag);
	p->word_61c = 1;			/* so the codec masks run too */
	p->shaperSR = 0x2c00 + tag;
	p->shaperId = (unsigned int)(5 + tag);
	p->shaperA1 = 0.125f * (float)(tag + 1);
	p->shaperA2 = 0.25f * (float)(tag + 2);
	p->shaperB1 = 0.5f * (float)(tag + 3);
	p->shaperB2 = 0.0625f * (float)(tag + 4);

	for (c = 0; c < V90_CONSTELLATIONS; c++) {
		p->constellationSize[c] = SEED_CONSTEL_LEN;
		p->distinctIndex[c] = 0;
		for (j = 0; j < V90_CONSTELLATION_MAX; j++) {
			p->constellation[c][j] = (unsigned char)
			    ((j * 13 + c * 7 + tag * 29) & 0x7f);
			p->codecConstellation[c][j] = (unsigned char)
			    ((j * 5 + c * 11 + tag * 17) & 0x7f);
		}
	}
}

/*
 * THE V92CP HAS TO BE IN RANGE BEFORE THE RATE-RENEGOTIATION ARMS RUN.
 *
 * Arms 0x22 and 0x23 reach `V92Modulator::initiateRRN`, which resets the
 * phase 4 modulator, which calls `V92CP::infoToBits` -- and that walks
 * `word_10c` groups of eight 17-bit fields into a 2,000-byte vector starting
 * at 136, so anything above 13 writes off the end.  A freshly constructed
 * `V92CP` does not bound it; the packer does, and these two arms do not call
 * the packer.  Three groups is well inside the vector (952 bytes with both
 * mask blocks) and is what these trials use.  D561: a trial that reaches
 * undefined behaviour in the reconstruction is not a trial.
 */
#define SEED_CP_GROUPS	3

static void
seed_v92cp(V92CP *cp)
{
	int i, k;

	cp->word_10c = SEED_CP_GROUPS;
	cp->byte_24 = 1;
	for (i = 0; i < V92CP_GROUPS; i++) {
		cp->distinctIndex[i] = i * 3 + 1;
		for (k = 0; k < V92CP_MASKS; k++) {
			cp->constellationMask[i][k] = (short)(0x1000 + i * 16 + k);
			cp->codecConstellationMask[i][k] = (short)(0x2000 + i * 16 + k);
		}
	}
}

static void
seed_cpinfo(unsigned char *blk)
{
	tagV90AdditionalCPinfo *info = (tagV90AdditionalCPinfo *)blk;

	info->word_00 = 0x51;
	info->word_04 = 0x03;			/* char_01, and non-zero */
	info->float_08 = 1.75f;
	info->word_0c = 0x27;
	info->word_10 = 0x1234;
	info->short_14 = 0x33;
}

/* +0x6bd0 + 0x08, `V92EchoCanceller::state`; V92EchoCanceller.h. */
static void
echoCanceller_state(unsigned char *o, int st)
{
	*(int *)(o + 0x6bd0 + 0x08) = st;
}

/* The 41 INFO0 bits `setPhaseIIinfo` reads, and the round-trip delay. */
static int info0_bits[48];

static void
poke(int s, const struct trial *t)
{
	unsigned char *o = base[s];
	unsigned char *dem;
	unsigned char *mod;
	unsigned char *par;
	int i;

	for (i = 0; i < 48; i++)
		info0_bits[i] = (i * 7 + 3) & 1;
	if (s == 0)
		((VPcmFloModem *)o)->setPhaseIIinfo(info0_bits, 37);
	else
		ref_setPhaseIIinfo(o, info0_bits, 37);

	memcpy(&dem, o + OFF_DEMOD, sizeof dem);
	memcpy(&mod, o + OFF_V92MODULATOR, sizeof mod);
	memcpy(&par, o + OFF_V90PARAMS, sizeof par);

	/*
	 * Both `progress` fan-outs off; see the header for why.  2 is outside
	 * {0, 1} for the V.90 side and outside {DIGITAL, ANALOG} for the V.92
	 * one, which is what makes each a no-op.
	 */
	*(unsigned int *)(o + OFF_SIDE) = 2;
	*(unsigned int *)(o + OFF_V92SIDE) = 2;

	o[OFF_BYTE6118] = t->b6118;
	o[OFF_BYTE6119] = t->b6119;
	*(int *)(o + OFF_LAYOUT) = t->layout;
	*(unsigned int *)(o + OFF_WORD7F60) = t->w7f60;
	*(unsigned int *)(o + OFF_WORD7F64) = t->w7f64;
	o[OFF_CP_EXTENDEU] = t->extendEu;
	MPARAMS(s)->unnamed_0003 = t->cfg3;

	seed_mapping(o + OFF_MAPPING, 1);
	seed_mapping(o + OFF_MAPPINGALT, 2);
	seed_cpinfo(o + OFF_CPINFO);

	{
		V92CP *cp;

		memcpy(&cp, o + OFF_V92CP, sizeof cp);
		seed_v92cp(cp);
	}

	*(unsigned int *)(dem + DEM_WORD3C) = t->w3c;
	*(unsigned int *)(dem + DEM_INPHASE3) = t->inPhase3;
	*(unsigned int *)(mod + MOD_WORD34) = t->w34;
	*(int *)(mod + MOD_PHASE) = t->phase;
	*(int *)(par + PARAM_EC_RRN) = t->ecRRN;

	{
		V90Demodulator *d = (V90Demodulator *)dem;
		V92Modulator *m = (V92Modulator *)mod;
		V92Phase4Modulator *p4 = m->phase4Modulator;
		V92Jd *jd;

		d->resampler.timingOffset = t->timingOffset;

		/*
		 * The rate-renegotiation threshold arms 0x22 and 0x23 copy
		 * into the phase 4 modulator.  A constructed evaluator holds
		 * zero there and the modulator holds zero too, so the copy
		 * would be a store of 0 over 0 and deleting it would be
		 * invisible; the mutation set said exactly that.
		 */
		d->connectionEvaluator->silenceRrnRequest = 0x5a3c1;

		p4->state = t->p4state;
		p4->symbolCount = t->p4symbolCount;
		p4->word_1b0 = t->p4word1b0;
		p4->word_38 = t->p4word38;
		p4->flag_20 = 0;
		p4->word_1c4 = 0;
		p4->e2uExtended = 0;
		p4->silenceRrnRequest = 0;
		p4->word_30 = 0;

		memcpy(&jd, o + OFF_JD92, sizeof jd);
		jd->phaseBits[29] = t->jdFirst;
		jd->phaseBits[30] = t->jdSecond;

		echoCanceller_state(o, t->ecState);

		/*
		 * See the field comment on `p3state`: without this,
		 * `exitJa`/`exitSuSecond`/`exitTRN1uSecond` all guard on a
		 * state the constructor never leaves them at, and dropping
		 * the call is invisible.  EACH ALSO GUARDS ON `symbolCount !=
		 * 0` A SECOND TIME, in `V92Phase3Modulator::exitJa`/
		 * `exitSuSecond`/`exitTRN1u` themselves (src/pump/v90/
		 * V92Phase3Modulator.cpp) -- the constructor leaves it at
		 * zero too, so `state` alone is not enough.
		 */
		if (t->p3state >= 0) {
			m->phase3Modulator->state =
			    (V92Phase3ModulatorState)t->p3state;
			m->phase3Modulator->symbolCount = 7;
		}
	}
	*(int *)(par + PARAM_CONNECTION_TYPE) = 0;
	*(int *)(par + PARAM_PRE_FILTER_GAIN) = 0;

	/*
	 * THE ECHO CANCELLER'S OWN FILTER, GIVEN SOMETHING TO CANCEL.
	 *
	 * The constructor leaves `echoCoeff`/`echoHistory` at zero, so
	 * `V92EchoCanceller::process`'s `state == V92_ECHO_FILTER_ONLY` arm
	 * computes `sum == 0` on every call and `out[i] = in[i] - 0`  --
	 * bit-identical to the `out[0] == 177.0f` BYPASS arm's `out[i] =
	 * in[i]`.  So neither "the ramp value is stored before the step, not
	 * after" nor "the non-ramp arm leaves rx[0] alone" can be observed:
	 * both mutations only change whether rx[0] READS as the 177.0f
	 * sentinel, and with a zeroed filter the two arms it selects between
	 * are the same arithmetic.  A real (if arbitrary) filter makes them
	 * different arithmetic, which is what the ramp arm's job actually is.
	 */
	{
		unsigned char *ec = o + OFF_ECHOCANCELLER;
		unsigned int filterLength, historyAlloc, k;
		float *coeff, *hist;

		memcpy(&filterLength, ec + ECHO_FILTERLENGTH,
		       sizeof filterLength);
		memcpy(&historyAlloc, ec + ECHO_HISTORYALLOC,
		       sizeof historyAlloc);
		memcpy(&coeff, ec + ECHO_COEFF, sizeof coeff);
		memcpy(&hist, ec + ECHO_HISTORY, sizeof hist);
		for (k = 0; k < filterLength; k++)
			coeff[k] = 0.01f;
		for (k = 0; k < historyAlloc; k++)
			hist[k] = 0.02f;
	}

	if (t->rxSeedOn)
		*(float *)(o + OFF_BLOCK6C0C) = t->rxSeedVal;
}

/*
 * The four buffers.  `in` is what the echo canceller filters, `out` is what
 * the scaling loop multiplies and `updateEchoHistory` then absorbs -- and
 * because the two `progress` calls are no-ops, `out` arrives at the loop
 * exactly as it was seeded, which is what makes the 0.4f visible at every
 * one of the 48 positions rather than only where a transmitter happened to
 * put a non-zero.
 *
 * NOT DYADIC, for finding F230's reason at one remove: a block of values that
 * are all exact multiples of a power of two survives the 0.4f multiply with
 * the same relative error everywhere, and a mutation that used 0.5f would
 * still be caught but one that reassociated would not.
 */
static void
seed_buffers(long trial)
{
	int i;

	lfsr = 0x51a7u + 0x9e37u * (unsigned)trial;
	for (i = 0; i < NSAMP; i++) {
		float a = (float)((int)nextb() - 128) * 0.013671875f
			  + (float)i * 0.0009765625f;
		float b = (float)((int)nextb() - 128) * 0.02734375f
			  - (float)(i * i) * 0.000030517578125f;

		sig_in[0][i] = sig_in[1][i] = a;
		sig_seed[i] = a;
		sig_out[0][i] = sig_out[1][i] = b;
		rxbits[0][i] = rxbits[1][i] = (int)nextb();
		txbits[0][i] = txbits[1][i] = (int)nextb();
	}
	nrx[0] = nrx[1] = 7;
	nbits[0] = nbits[1] = 11;
}

static int
run(void)
{
	long trial;
	int rc;

	diff_begin("VPcmFloModem::runPcmModem against the blob, over both "
		   "jump tables and the compare chain below them");

	/*
	 * THE MODELLED FLOAT SPANS' BUDGET, modern-tier only (the setter is a
	 * no-op and `harness_float_tol()`/`harness_float_atol()` stay 0.0
	 * without `HARNESS_FLOAT_TOL`, so `make period` is bit-exact).  It is
	 * the harness's mixed form, `|a-b| <= atol + rtol*|reference|`,
	 * because the divergence passes through zero: the measured worst
	 * ABSOLUTE difference over every trial is 4.0e-5 (the modulator's
	 * designed coefficient bank) and 3.9e-5 (the demodulator's), while the
	 * worst RELATIVE difference on those same banks is 7.2e-2 -- a
	 * near-zero coefficient.  `atol` 1.0e-4 carries those with a 2.5x
	 * margin, NOT an independent correctness bound. This remains an
	 * owner-authorized provisional fixture policy; the same
	 * sinc/FIR design divergence is finding F11363's, and t_resampler.cpp
	 * uses the same mechanism for it.  This reaches only the modelled
	 * spans through diff_eq_float_word; no fixture-wide mixed setter.
	 * Every other word of every region stays byte-exact.
	 */
	harness_float_tol_fixture(0);

	for (trial = 0; trial < NALL; trial++) {
		struct trial synth;
		const struct trial *t;
		int ra, rb;
		int i;
		int allocs;

		if (trial < NTRIAL) {
			t = &trial_v[trial];
		} else {
			sweep_trial(&synth, (int)(trial - NTRIAL));
			t = &synth;
		}

		memset(&v34obj[0], 0, sizeof v34obj[0]);
		memset(&v34obj[1], 0, sizeof v34obj[1]);
		memset(mparams[0], 0, MP_SLOT);
		memset(mparams[1], 0, MP_SLOT);

		harness_alloc_reset();
		base[0] = (unsigned char *)VPCMXF_Create(0, &v34obj[0],
							 MPARAMS(0),
							 DURATION_MS, 3);
		base[1] = (unsigned char *)ref_VPCMXF_Create(0, &v34obj[1],
							     MPARAMS(1),
							     DURATION_MS, 3);

		diff_eq_int("both sides constructed (%ld)",
			    base[0] != 0 && base[1] != 0, 1, trial);
		if (base[0] == 0 || base[1] == 0)
			continue;

		poke(0, t);
		poke(1, t);
		seed_buffers(trial);

		discover();
		mark_float_regions();
		diff_eq_int("no region overflow (%ld)", region_overflow, 0,
			    trial);
		diff_eq_int("no lopsided pointer word (%ld)", lopsided, 0,
			    trial);
		snapshot();
		nregion_last = nreg;

		allocs = harness_alloc.allocs;

		dsplibs_debug_level = t->level;
		ref_dsplibs_debug_level = t->level;
		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();

		ra = ((VPcmFloModem *)base[0])->runPcmModem(sig_in[0],
		    sig_out[0], NSAMP, rxbits[0], &nrx[0], txbits[0],
		    &nbits[0]);
		rb = ref_runPcmModem(base[1], sig_in[1], sig_out[1], NSAMP,
				     rxbits[1], &nrx[1], txbits[1], &nbits[1]);

		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
		dsplib_debug_capture_on = 0;

		diff_eq_int("return value (%ld)", ra, rb, trial);
		/*
		 * THE TRANSCRIPT, WHICH THE REGION WALK CANNOT SEE, AND ONLY
		 * FOR THE TWO TRIALS THAT SET `cmpTranscript`.  "the FPE arm
		 * starts FPE whatever phase it is in" and "the fallback test
		 * is against 0x21 rather than 0x20" are both real only in
		 * `edprintf`/`dsplibs_debug_printf` output that changes
		 * nothing in memory, per this suite's own note at the top of
		 * test/mutations/vpcmrunpcm.json.  NOT compared everywhere:
		 * the pre-existing "level 2" trials drive `V90Modem::progress`
		 * with its deliberately-illegal side (the file header's own
		 * design) and its "Illegal modemSide" print carries one more
		 * blank line on the blob's side than ours -- a real,
		 * pre-existing formatting gap this suite had no way to see
		 * before and which is not one of the seven claims this batch
		 * is closing.  Comparing transcripts on those trials too would
		 * fail `make one` on that unrelated gap instead.
		 */
		if (t->cmpTranscript)
			diff_eq_int("debug transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, trial);
		diff_eq_int("nothing was allocated (%ld)",
			    harness_alloc.allocs - allocs, 0, trial);

		for (i = 0; i < NSAMP; i++)
			diff_eq_float("out[%ld]", sig_out[0][i],
				      sig_out[1][i], (long)i);
		for (i = 0; i < NSAMP; i++) {
			diff_eq_int("rxbits[%ld]", rxbits[0][i], rxbits[1][i],
				    (long)i);
			diff_eq_int("txbits[%ld]", txbits[0][i], txbits[1][i],
				    (long)i);
		}
		for (i = 0; i < NSAMP; i++)
			diff_eq_int("in[%ld] is untouched",
				    memcmp(&sig_in[0][i], &sig_seed[i], 4) == 0 &&
				    memcmp(&sig_in[1][i], &sig_seed[i], 4) == 0, 1, (long)i);
		diff_eq_int("*nrx (%ld)", nrx[0], nrx[1], trial);
		diff_eq_int("*nbits (%ld)", nbits[0], nbits[1], trial);

		compare_regions(trial);

		free_regions();
		VPCMXF_Delete(base[0]);
		ref_VPCMXF_Delete(base[1]);
		base[0] = 0;
		base[1] = 0;
	}

	/*
	 * THE DENOMINATORS.  Printed rather than asserted at a threshold,
	 * because the right number is not known in advance -- what matters is
	 * that a reader of a green run can see how much of the surface was
	 * actually compared and how much was exempt.  The two exempt classes
	 * are asserted to be a small minority of the whole, which is the
	 * check that a swallowed comparison would fail.
	 *
	 * `words within float tolerance` is a COMPARED class, not an exempt
	 * one: each of those words went through `diff_eq_float` and the
	 * fixture's budget, and on the period tier the budget is 0 so the
	 * count is 0 and every one of them is in `words equal` instead.  It
	 * is reported so a reader can see how much of the modern surface the
	 * tolerance carried, and it gets its own minority assertion -- if the
	 * float spans ever grew to the whole object the tolerance would have
	 * become an off switch and that check fails.
	 */
	printf("    surface: %d regions, %ld words equal, %ld words within "
	       "float tolerance, %ld corresponding pointer pairs, %ld borrowed "
	       "tables untouched, %ld pattern installs\n",
	       nregion_last, words_equal, words_float, words_corresponded,
	       words_static, words_unresolved);
	diff_eq_int("the exempt classes are a minority of the surface",
		    (words_corresponded + words_static + words_unresolved) * 20
		    < words_equal, 1, 0);
	diff_eq_int("the float tolerance is a minority of the surface",
		    classified_float_words > 0 &&
		    classified_float_words * 2 < classified_total_words, 1, 0);
	printf("    classified float words %ld / %ld total words\n",
	       classified_float_words, classified_total_words);

	rc = diff_end();
	return rc;
}

/*
 * The return value is the whole point of the first table, so it gets its own
 * claim: eight distinct codes reach `VPcmV34Progress` and 4 is not one of
 * them.  Asserted here rather than left implicit, because a mutation that
 * turned one arm's 5 into a 4 would still be caught by the comparison above
 * and this says WHAT the set is.
 */
static int
run_return_set(void)
{
	static const int expect[] = { 0, 1, 2, 3, 5, 6, 7, 8 };
	int seen[16];
	int i;
	long trial;
	int rc;

	diff_begin("the codes runPcmModem can return");
	memset(seen, 0, sizeof seen);

	for (trial = 0; trial < NALL; trial++) {
		struct trial synth;
		const struct trial *t;
		int r;

		if (trial < NTRIAL) {
			t = &trial_v[trial];
		} else {
			sweep_trial(&synth, (int)(trial - NTRIAL));
			t = &synth;
		}

		memset(&v34obj[1], 0, sizeof v34obj[1]);
		memset(mparams[1], 0, MP_SLOT);
		base[1] = (unsigned char *)ref_VPCMXF_Create(0, &v34obj[1],
							     MPARAMS(1),
							     DURATION_MS, 3);
		if (base[1] == 0)
			continue;
		poke(1, t);
		seed_buffers(trial);
		r = ref_runPcmModem(base[1], sig_in[1], sig_out[1], NSAMP,
				    rxbits[1], &nrx[1], txbits[1], &nbits[1]);
		if (r >= 0 && r < 16)
			seen[r] = 1;
		ref_VPCMXF_Delete(base[1]);
		base[1] = 0;
	}

	for (i = 0; i < 16; i++) {
		int want = 0;
		unsigned k;

		for (k = 0; k < sizeof expect / sizeof expect[0]; k++)
			if (expect[k] == i)
				want = 1;
		diff_eq_int("code %ld reachable", seen[i], want, (long)i);
	}

	rc = diff_end();
	return rc;
}

int
main(void)
{
	int rc = 0;

	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	rc |= run();
	rc |= run_return_set();

	return rc;
}

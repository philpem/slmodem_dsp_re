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
 * members set), the V92Phase4Modulator two levels down (`word_2c`, `word_30`,
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
 *   2. `V92Modulator::progress` WRITES `word_34`, which is dispatch 3's
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
 * `V90Demodulator::enterPhase3`, which dispatch 3's `word_34 == 3` arm
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
#define OFF_V90PARAMS	0x610c		/* V90Modem::ptr_49b4        */
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
 * not this function's.  Dispatch 3's `word_34 == 3` arm calls
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
			add_region(pa, pb, sa < sb ? sa : sb);
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
};

/*
 * `w3c` 0x36 is one past the table and takes the `ja` at 0xe50b, which is the
 * only way the default arm is reached other than through one of its 33 own
 * entries.  `b6118` 5 and 6 are the same claim for the first table's `ja`.
 */
#define TAIL	, 0, 0, 1, 0, 0.0f, 3, 7, V92_ECHO_FILTER_ONLY
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
	{ "level 2, fallback", 0, 1, 0x20, 3, 3, 1, 0, 0, 1, 1, 1, 0, 2 TAIL }
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
		cp->word_28[i] = i * 3 + 1;
		for (k = 0; k < V92CP_MASKS; k++) {
			cp->short_42[i][k] = (short)(0x1000 + i * 16 + k);
			cp->short_a2[i][k] = (short)(0x2000 + i * 16 + k);
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
		d->connectionEvaluator->word_90 = 0x5a3c1;

		p4->state = t->p4state;
		p4->symbolCount = t->p4symbolCount;
		p4->word_1b0 = t->p4word1b0;
		p4->word_38 = t->p4word38;
		p4->flag_20 = 0;
		p4->word_1c4 = 0;
		p4->e2uExtended = 0;
		p4->word_2c = 0;
		p4->word_30 = 0;

		memcpy(&jd, o + OFF_JD92, sizeof jd);
		jd->phaseBits[29] = t->jdFirst;
		jd->phaseBits[30] = t->jdSecond;

		echoCanceller_state(o, t->ecState);
	}
	*(int *)(par + PARAM_CONNECTION_TYPE) = 0;
	*(int *)(par + PARAM_PRE_FILTER_GAIN) = 0;
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
		diff_eq_int("no region overflow (%ld)", region_overflow, 0,
			    trial);
		diff_eq_int("no lopsided pointer word (%ld)", lopsided, 0,
			    trial);
		snapshot();
		nregion_last = nreg;

		allocs = harness_alloc.allocs;

		dsplibs_debug_level = t->level;
		ref_dsplibs_debug_level = t->level;

		ra = ((VPcmFloModem *)base[0])->runPcmModem(sig_in[0],
		    sig_out[0], NSAMP, rxbits[0], &nrx[0], txbits[0],
		    &nbits[0]);
		rb = ref_runPcmModem(base[1], sig_in[1], sig_out[1], NSAMP,
				     rxbits[1], &nrx[1], txbits[1], &nbits[1]);

		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;

		diff_eq_int("return value (%ld)", ra, rb, trial);
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
			diff_eq_float("in[%ld] is untouched", sig_in[0][i],
				      sig_in[1][i], (long)i);
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
	 */
	printf("    surface: %d regions, %ld words equal, %ld corresponding "
	       "pointer pairs, %ld borrowed tables untouched, %ld pattern "
	       "installs\n",
	       nregion_last, words_equal, words_corresponded, words_static,
	       words_unresolved);
	diff_eq_int("the exempt classes are a minority of the surface",
		    (words_corresponded + words_static + words_unresolved) * 20
		    < words_equal, 1, 0);

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

/*
 * t_vpcmcreate.c -- differential test of `VPcmV34Create`, on a V.PCM graph the
 * BLOB built.
 *
 * WHAT MADE THIS POSSIBLE, AND WHY IT IS NOT THE FIXTURE ANYONE EXPECTED.
 * `VPcmV34Create` configures a graph it does not allocate: it is handed a
 * `struct v34_object`, memsets it, restores two pointers across the memset,
 * and then reaches through those pointers into a VPcmFloModem, a V90Modem, a
 * V90Demodulator, an equaliser, a connection evaluator, a constellation
 * designer, a K56FlexFloModem, a V92EchoCanceller and a GenericIIR.  Standing
 * all of that up by hand is `test/harness/v90demfix.h` again with the V.92 and
 * K56flex halves added, and that was recorded as the blocker.
 *
 * IT DOES NOT HAVE TO BE STOOD UP.  Findings F800-806 measured that the blob's
 * own constructor is aliasable and that a blob-constructed object is a VALID
 * differential fixture -- two blob-code pointers in 265,520 bytes, both of them
 * `struct v34_object` scrambler hooks this tree has already reconstructed.  So
 * this file BORROWS the graph:
 *
 *     ref_dp_vpcm_init()                             registers the ops table
 *     ops->create(m, 34, caller, 9600, 48, ops)      builds the whole graph
 *
 * and `VPcmV34Create` is then driven on it.
 *
 * AND FINDING F803'S CONGRUENCE PROBLEM DOES NOT ARISE, because there is only
 * ONE graph.  Two constructions come back at 125 different addresses and every
 * pointer field then differs for a reason that is not a defect; this file
 * builds one and runs both sides on the SAME memory at the SAME addresses,
 * finding F805's three-pass shape applied to a constructor:
 *
 *     snapshot every live region
 *     run OURS      -> copy every region away
 *     restore       -> and CHECK the restore restored
 *     run the BLOB's -> compare region by region
 *
 * Every pointer field of the graph is identical by construction.  THIRTEEN
 * WORDS ARE STILL EXCLUDED and they are not graph pointers: they are the
 * static-object addresses three of the callees install, which point into our
 * image on our side and into the blob's on the blob's.  See the alias-word
 * block below; the exclusion is LEARNED by running each of those callees on
 * its own, and checked rather than asserted.
 *
 * THE COMPARISON IS THE WHOLE HEAP GRAPH, not a hand-listed set of
 * sub-objects.  `harness_alloc_live_set` enumerates every live allocation with
 * its size, so a store into a region this file could not have named is a
 * failure rather than a silence.  That is the one thing finding F806's
 * uncommitted probe needed a malloc interposer for and this harness already
 * has.
 *
 * THE BOUND ON THE CLAIM, STATED PLAINLY.  `ops->create` has already called
 * `ref_VPcmV34Create` once by the time it returns, so both calls here are
 * RE-initialisations of an initialised object.  That is the same on both sides
 * and it is what the object does in service -- `VPcmV34InitiateRetrain` is the
 * other caller -- but an arm reachable only on a virgin allocation is not
 * driven by this file.  `run_virgin_vs_reinit` below measures whether such an
 * arm exists, by comparing the graph after `create` against the graph after one
 * further `VPcmV34Create` and reporting the difference rather than asserting it
 * away.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "harness.h"

#include "dsplib/dp.h"
#include "dsplib/modem_params.h"
#include "dsplib/v34recv.h"

extern void ref_dp_vpcm_init(void);
extern void *ref_dp_runtime_create(void *modem);
extern unsigned int ref_dsplibs_debug_level;
extern unsigned int dsplibs_debug_level;

/*
 * `VPcmV34Create` is an unmangled export of `VPcmV34Main.cpp` and lives in
 * `src/pump/v34/v34pcmif.c`'s neighbourhood, so it is reachable by name; the
 * blob's is the ordinary `ref_` alias.  The signature is the call site's at
 * `vpcm_create+0x270`.
 */
extern int ref_VPcmV34Create(void *obj, int side, int arg3, void *dpRuntime,
			     int sessionType);

/*
 * ============================================================================
 * THIS IS NOW A DIFFERENTIAL TEST, and it was not when it was written.
 *
 * The file was committed as a FIXTURE -- both sides of every comparison were
 * the blob's, and what it asserted was the three properties a differential
 * test of a constructor needs before its verdict can mean anything: that the
 * whole 125-region heap graph can be snapshotted and RESTORED with the restore
 * checked rather than assumed; that the call CHANGES the graph, so an agreeing
 * comparison is not two untouched images (finding F805's vacuous passes 0 and
 * 1); and that the comparison REPORTS a single flipped byte and goes quiet
 * when it is put back.
 *
 * All three assertions are still here and still run.  What changed is one
 * line: `VPcmV34Create` is now OURS, from src/pump/v34/v34pcmcreate.cpp,
 * and
 * `ref_VPcmV34Create` is the blob's.  Every comparison below now has two
 * different implementations on its two sides.
 *
 * FIVE ARGUMENTS, not the four the fixture's own instructions said to declare:
 * that sentence predated finding F1119, which found the fifth by reading the
 * five sites that touch `0x50(%esp)` and the five slots `vpcm_create` pushes.
 *
 * FINDING F803'S CONGRUENCE PROBLEM DOES NOT ARISE and no HEAP pointer is
 * excluded.  125 heap regions coming back at 125 different addresses, so that
 * every pointer-valued field differs for a reason that is not a defect, is
 * what happens when TWO constructions are compared.  This file builds ONE and
 * runs both implementations over the SAME memory at the SAME addresses, so
 * every heap pointer is identical by construction.
 *
 * WHAT IS EXCLUDED IS THIRTEEN WORDS OF STATIC-OBJECT ADDRESS -- 52 bytes of
 * 265,520 -- none of them written by `VPcmV34Create` itself.  Twelve are
 * `v34handshakinit`'s and one is `V90Demodulator::enterChannelVerification`'s.
 * `run_alias_words` both builds that set and is the check that no store of
 * this function's own can hide behind it.
 * ============================================================================
 */
extern int VPcmV34Create(void *obj, int side, int arg3, void *dpRuntime,
			 int sessionType);
/* C fixture declarations preserve the helpers' mangled tagV34Object ABI. */
extern void fixture_set_rates(void *obj)
	__asm__("_Z24VPcmV34SetMinMaxBitRatesP12tagV34Object");
extern void ref_fixture_set_rates(void *obj)
	__asm__("ref__Z24VPcmV34SetMinMaxBitRatesP12tagV34Object");
extern void fixture_set_floor(void *obj)
	__asm__("_Z25VPcmV34SetMinimumSigLevelP12tagV34Object");
extern void ref_fixture_set_floor(void *obj)
	__asm__("ref__Z25VPcmV34SetMinimumSigLevelP12tagV34Object");
extern void fixture_set_delays(void *obj)
	__asm__("_Z16VPcmV34SetDelaysP12tagV34Object");
extern void ref_fixture_set_delays(void *obj)
	__asm__("ref__Z16VPcmV34SetDelaysP12tagV34Object");
extern void VPcmV34InitiateRetrain(void *obj, unsigned char requestedDp);
extern void ref_VPcmV34InitiateRetrain(void *obj, unsigned char requestedDp);
/* Both local bodies consume root in EAX and short delay in DX. */
extern void ApplyBulkDelay(void *obj, short delay)
	__attribute__((regparm(2)));
extern void ref_ApplyBulkDelay(void *obj, short delay)
	__attribute__((regparm(2)));
extern void txinit(void *obj);
extern void ref_txinit(void *obj);
extern void ref_v34modeminit(void *obj);
extern short GetVPcmMinimalTxPowerReduction(void *obj);
extern short ref_GetVPcmMinimalTxPowerReduction(void *obj);
extern void settxlevel(void *obj, const short *mp);
extern void ref_settxlevel(void *obj, const short *mp);
extern void txmitdibit(void *obj, short bits);
extern void ref_txmitdibit(void *obj, short bits);
extern void txmitquadbit(void *obj, short bits);
extern void ref_txmitquadbit(void *obj, short bits);
struct v34_modulator;
extern void ref_V34SetupModulator(struct v34_modulator *mod, short baud,
		short carrier, short preemp, int v90, int reset);
extern void VPcmV34SetV90RateReneg(void *obj, short rrn, unsigned char constellation);
extern void ref_VPcmV34SetV90RateReneg(void *obj, short rrn, unsigned char constellation);
extern void V34XF_IndicateJdReceived(void *obj, unsigned char constellation,
		unsigned char silence);
extern void ref_V34XF_IndicateJdReceived(void *obj, unsigned char constellation,
		unsigned char silence);
extern void V34XF_IndicateDilReceived(void *obj, unsigned char constellation);
extern void ref_V34XF_IndicateDilReceived(void *obj, unsigned char constellation);
extern void V34XF_IndicateTrn2dReceived(void *obj);
extern void ref_V34XF_IndicateTrn2dReceived(void *obj);
extern int k56FlexPhase34(void *obj);
extern int ref_k56FlexPhase34(void *obj);
extern void V34XF_IndicateK56FlexRateDetermined(void *obj);
extern void ref_V34XF_IndicateK56FlexRateDetermined(void *obj);
extern void VPcmV34InitiateHangUp(void *obj);
extern void ref_VPcmV34InitiateHangUp(void *obj);
extern void VPcmV34InitMOH(void *obj, int message, unsigned char late,
			 unsigned char flag);
extern void ref_VPcmV34InitMOH(void *obj, int message, unsigned char late,
			     unsigned char flag);

/* Native and reconstruction local helper ABIs both consume root in EAX.
 * Globalization is test-only and does not change this private method boundary. */
extern void fixture_get_mp(void *obj) __asm__("_Z14getMPrecvdBitsP12tagV34Object")
    __attribute__((regparm(1)));
extern void ref_fixture_get_mp(void *obj) __asm__("ref__Z14getMPrecvdBitsP12tagV34Object")
    __attribute__((regparm(1)));
extern int v90RateReneg(void *obj);
extern int ref_v90RateReneg(void *obj);
extern int v90RateRenegSilence(void *obj);
extern int ref_v90RateRenegSilence(void *obj);
extern void ref_mp_reset(void *mp) __asm__("ref__ZN5V90MP5resetEv");
extern void ref_mp_pack(void *mp) __asm__("ref__ZN5V90MP10infoToBitsEv");
extern int ref_mp_receive(void *mp, int bit) __asm__("ref__ZN5V90MP10bitsToInfoEi");
extern void ref_copy_mp(void *session) __asm__("ref__ZN12VPcmFloModem22copyMpInfoForInterfaceEv");

/* --- the configuration, all of it derived; docs/configuration.md ---------- */

#define CFG_MIN_RATE	300
#define CFG_MAX_RATE	56000
#define CFG_IODELAY	0
#define CFG_CODECTYPE	4
#define CFG_SRATE	9600		/* `cmp $0x2580,%esi`, and MODEM_RATE */
#define CFG_MAX_FRAG	48		/* `cmpl $0x30,..` / `jg`, MODEM_FRAG */

#define ROOT_V34	0x2c		/* `lea 0x2c(%ebx),%ebp` at 0x3add   */
#define V34_LEN		0xac4c		/* the memset at the top of the call  */

/*
 * ---------------------------------------------------------------------------
 * THE CONFIGURATION IS SWEPT TOO, and that is where nearly all the branching
 * is.  Sweeping (side, sessionType) alone leaves the interesting forks dark:
 * three quarters of `VPcmV34Create`'s decisions are taken on words of the
 * dp-runtime block at +0xac3c and on two bytes of the heap graph, and one
 * fixed configuration executes exactly one arm of each.
 *
 * The pokes go in AFTER each `graph_restore()` and not before -- the restore
 * puts the runtime block back with everything else, so a poke applied first is
 * undone before the call sees it.
 *
 * Offsets are `VPcmV34Create`'s own reads, spelled here rather than included
 * from the source under test on purpose: a test that shares the map with the
 * thing it tests cannot catch the map being wrong.
 */
#define CFG_V92LITE	0x02		/* bit 4: quick connect from phase 1 */
#define CFG_ANSPCM	0x0c
#define CFG_UQTS	0x10
#define CFG_TXMD	0x14
#define CFG_MINRATE	0x30
#define CFG_MAXRATE	0x34
#define CFG_F54		0x54
#define CFG_MINLEVEL	0x60
#define CFG_FILTDELAY	0x64
#define CFG_EXTDELAY	0x68
#define CFG_ENTRANCE	0x70

#define OB_P3548	0x3548		/* the VPcmFloModem                  */
#define OB_PAC18	0xac18		/* the K56FlexFloModem               */
#define SESS_DEMOD	0x175c
#define SESS_GATE	0x6120		/* the INFO0 layout gate             */
#define DEMOD_DESIGN	0x0208
#define K56_ENABLED	0x08

#define NCFG		20

static struct dsp_info dspinfo;
static struct _tagModemParameters *runtime;
static char *root, *v34obj;

/* --- the heap graph ------------------------------------------------------- */

#define MAXREG	512

static void *reg[MAXREG];
static size_t regsz[MAXREG];
static unsigned char *snap[MAXREG];	/* the before-image                  */
static unsigned char *ours[MAXREG];	/* what our call left                */
static int nreg;
static int root_idx = -1;

/*
 * ---------------------------------------------------------------------------
 * THE THIRTEEN WORDS THAT CANNOT AGREE, AND WHY THEY ARE NOT
 * `VPcmV34Create`'S.
 *
 * Three of `VPcmV34Create`'s callees install the ADDRESS OF A STATIC OBJECT
 * into the graph.  Ours point at our copies and the blob's at the blob's
 * `ref_` aliases, because both copies are linked into the same test binary and
 * they are at different addresses.  Nothing about that is a defect: it is
 * finding F802's measurement arriving from the other direction, where two
 * blob-code pointers were counted in a blob-constructed graph and these are
 * the same kind of word counted in an our-code one.
 *
 *   twelve   `v34handshakinit`, in the V.34 object: ten coefficient tables
 *            and two scrambler entry points
 *   one      `V90Demodulator::enterChannelVerification`, in a 668-byte
 *            sub-object, holding `V90PreFilter::preFilterCoefType1`; only
 *            the quick-connect arm reaches it
 *   none     `V34InitializeImplementationSpecific`, which is asserted
 *
 * THE MASK IS LEARNED FROM THOSE THREE RUNS, not hand-listed -- see
 * `run_alias_words`.  The twelve in the root object are ALSO listed by name
 * below, and that list is a CHECK on the learned mask rather than the mask
 * itself: each must hold two DIFFERENT addresses whose targets are
 * byte-identical over the size of the object named, or, for the two function
 * slots, our symbol against the blob's `ref_` alias.
 *
 * WHAT BOUNDS THE HOLE.  Fifty-two bytes of 265,520; none of the thirteen
 * offsets appears anywhere in `VPcmV34Create`'s own transcription; and the
 * learning pass asserts that each callee disagrees on NOTHING ELSE, so a store
 * of `VPcmV34Create`'s own cannot hide behind the exclusion without also being
 * a store one of the three callees makes at the same word.
 */
struct alias_word {
	unsigned	off;		/* word offset in the V.34 object    */
	unsigned	n;		/* bytes at the target, 0 = a function */
	const void	*fours[2];	/* the candidate functions, ours     */
	const void	*fblob[2];	/* and the blob's                    */
	const char	*name;
};

/*
 * The two function slots do not always hold the SAME function: the handshake
 * initialiser picks a G-polynomial per role, so the descrambler slot holds one
 * of two symbols and the scrambler slot one of two others.  Both candidates
 * are listed, which is tighter than "it is a code pointer" and does not
 * pretend the choice is fixed.
 */
extern void descrambleGPA(void);
extern void descrambleGPC(void);
extern void scrambleGPA(void);
extern void scrambleGPC(void);
extern void ref_descrambleGPA(void);
extern void ref_descrambleGPC(void);
extern void ref_scrambleGPA(void);
extern void ref_scrambleGPC(void);

#define DATAW(o, n, nm)	{ (o), (n), { 0, 0 }, { 0, 0 }, (nm) }

static const struct alias_word aliasword[] = {
	DATAW(0x0418,  64, "hsine1800"),
	DATAW(0x0508, 120, "bpv22high"),
	DATAW(0x0620,  80, "V34TimingPrefilterCoeff"),
	DATAW(0x0624,  80, "V34TimingHPFilterCoeff"),
	DATAW(0x0a28, 128, "Convolve16"),
	{ 0x0e48, 0,
	  { (const void *)&descrambleGPA, (const void *)&descrambleGPC },
	  { (const void *)&ref_descrambleGPA,
	    (const void *)&ref_descrambleGPC },
	  "the descrambler" },
	DATAW(0x1460,  32, "hsine1200"),
	DATAW(0x20cc,  84, "ec_prem_coef_B3429"),
	DATAW(0x2100,  32, "preemp0"),
	DATAW(0x2608, 128, "Convolve16"),
	{ 0x2a28, 0,
	  { (const void *)&scrambleGPA, (const void *)&scrambleGPC },
	  { (const void *)&ref_scrambleGPA, (const void *)&ref_scrambleGPC },
	  "the scrambler" },
	DATAW(0x3564,  16, "c2400_")
};

#define NALIAS	((int)(sizeof(aliasword) / sizeof(aliasword[0])))

/*
 * THE MASK IS LEARNED, NOT LISTED.  `run_alias_words` runs each shared callee
 * ours-against-the-blob's on the same graph and marks the aligned words they
 * disagree on; the sweep then excludes exactly those.  A hand-written offset
 * list could not do the job: region indices are not stable between runs, and
 * one of the excluded words is not in the root object at all but in a 668-byte
 * sub-object of the demodulator, where it holds `V90PreFilter`'s coefficient
 * table.
 */
static unsigned char *amask[MAXREG];

static int
is_alias_byte(int i, size_t j)
{
	return amask[i] != 0 && amask[i][j] != 0;
}

/* Mark every ALIGNED WORD ours and the blob differ on; return words added. */
static long
alias_learn(void)
{
	long added = 0;
	int i;
	size_t j, k, w;

	for (i = 0; i < nreg; i++) {
		const unsigned char *p = (const unsigned char *)reg[i];

		for (j = 0; j < regsz[i]; j++) {
			if (ours[i][j] == p[j] || amask[i][j])
				continue;
			w = j & ~(size_t)3;
			added++;
			for (k = w; k < w + 4 && k < regsz[i]; k++)
				amask[i][k] = 1;
		}
	}
	return added;
}

static void
alias_report(void)
{
	int i;
	size_t j, run;

	for (i = 0; i < nreg; i++)
		for (j = 0; j < regsz[i]; j++) {
			if (!amask[i][j])
				continue;
			run = j;
			while (j + 1 < regsz[i] && amask[i][j + 1])
				j++;
			if (i == root_idx)
				printf("      excluded: the V.34 object"
				       " +0x%lx (%lu bytes)\n",
				       (unsigned long)(run - ROOT_V34),
				       (unsigned long)(j - run + 1));
			else
				printf("      excluded: region %d (%lu bytes)"
				       " +0x%lx (%lu bytes)\n", i,
				       (unsigned long)regsz[i],
				       (unsigned long)run,
				       (unsigned long)(j - run + 1));
		}
}

/*
 * Every one of the twelve root words, checked to BE an alias pair rather than
 * assumed to be one: two DIFFERENT addresses whose targets hold the same bytes
 * over the size of the object named, or, for the two function slots, our
 * symbol against the blob's `ref_` alias.  Two of the ten tables --
 * `ec_prem_coef_B3429` and `preemp0` -- are file-static in OUR build and
 * cannot be named here, which is why the contents check exists at all.
 */
static int
check_alias_words(int subset)
{
	int k, bad = 0;

	for (k = 0; k < NALIAS; k++) {
		const void *a, *b;
		size_t w = (size_t)ROOT_V34 + aliasword[k].off;

		if (subset == 1 && aliasword[k].off != 0x0a28
		    && aliasword[k].off != 0x0e48
		    && aliasword[k].off != 0x2608
		    && aliasword[k].off != 0x2a28)
			continue;
		/* Modes 1/4 do not run the mode-0 timing-filter initializer. */
		if (subset == 2 && (aliasword[k].off == 0x0620
				    || aliasword[k].off == 0x0624))
			continue;

		memcpy(&a, ours[root_idx] + w, sizeof a);
		memcpy(&b, (const unsigned char *)reg[root_idx] + w, sizeof b);

		if (a == b) {
			printf("    %s at v34+0x%x: both sides %p -- not an"
			       " alias pair\n", aliasword[k].name,
			       aliasword[k].off, a);
			bad++;
			continue;
		}
		if (aliasword[k].n == 0) {
			if ((a != aliasword[k].fours[0]
			     || b != aliasword[k].fblob[0])
			    && (a != aliasword[k].fours[1]
				|| b != aliasword[k].fblob[1])) {
				printf("    %s at v34+0x%x: %p/%p is neither"
				       " candidate pair\n", aliasword[k].name,
				       aliasword[k].off, a, b);
				bad++;
			}
			continue;
		}
		if (memcmp(a, b, aliasword[k].n) != 0) {
			printf("    %s at v34+0x%x: %p and %p differ over"
			       " %u bytes\n", aliasword[k].name,
			       aliasword[k].off, a, b, aliasword[k].n);
			bad++;
		}
	}
	return bad;
}

static void
graph_take(void)
{
	int i;

	nreg = harness_alloc_live_set(reg, MAXREG);
	if (nreg > MAXREG) {
		printf("FIXTURE: %d live regions, table holds %d\n", nreg,
		       MAXREG);
		exit(1);
	}
	for (i = 0; i < nreg; i++) {
		regsz[i] = malloc_usable_size(reg[i]);
		snap[i] = (unsigned char *)malloc(regsz[i]);
		ours[i] = (unsigned char *)malloc(regsz[i]);
		amask[i] = (unsigned char *)calloc(regsz[i], 1);
		if (snap[i] == 0 || ours[i] == 0 || amask[i] == 0) {
			printf("FIXTURE: out of memory snapshotting\n");
			exit(1);
		}
		memcpy(snap[i], reg[i], regsz[i]);
		if (reg[i] == (void *)root)
			root_idx = i;
	}
	if (root_idx < 0) {
		printf("FIXTURE: the V.PCM root is not in the live set\n");
		exit(1);
	}
}

static void
graph_save_ours(void)
{
	int i;

	for (i = 0; i < nreg; i++)
		memcpy(ours[i], reg[i], regsz[i]);
}

static void
graph_restore(void)
{
	int i;

	for (i = 0; i < nreg; i++)
		memcpy(reg[i], snap[i], regsz[i]);
}

/* How many bytes of the whole graph differ between two images. */
static long
graph_diff_bytes(unsigned char *const *a, unsigned char *const *b)
{
	long n = 0;
	int i;
	size_t j;

	for (i = 0; i < nreg; i++)
		for (j = 0; j < regsz[i]; j++)
			if (a[i][j] != b[i][j])
				n++;
	return n;
}

/*
 * `ex` excludes the learned alias words, and is 1 only where the two sides are
 * two different implementations.  Every other comparison in this file is one
 * implementation against itself and excludes nothing.
 */
static long
graph_diff_live(unsigned char *const *a, int ex)
{
	long n = 0;
	int i;
	size_t j;

	for (i = 0; i < nreg; i++) {
		const unsigned char *p = (const unsigned char *)reg[i];

		for (j = 0; j < regsz[i]; j++)
			if (a[i][j] != p[j] && !(ex && is_alias_byte(i, j)))
				n++;
	}
	return n;
}

/*
 * Where the first difference is, named as region index and offset, and whether
 * that region is the V.PCM root.  `diff_eq_obj` cannot be used across a
 * 126-region graph, so the report is built here.
 */
static void
report_first_diff(const char *what, unsigned char *const *a, int ex)
{
	int i;
	size_t j;

	for (i = 0; i < nreg; i++) {
		const unsigned char *p = (const unsigned char *)reg[i];

		for (j = 0; j < regsz[i]; j++)
			if (a[i][j] != p[j]
			    && !(ex && is_alias_byte(i, j))) {
				printf("    %s: region %d (%p, %lu bytes)"
				       " offset %lu: ours %02x blob %02x%s\n",
				       what, i, reg[i], (unsigned long)regsz[i],
				       (unsigned long)j, a[i][j], p[j],
				       i == root_idx
				       ? "   [the V.PCM root]" : "");
				return;
			}
	}
}

/* Every differing byte, coalesced into runs, for the measurement passes. */
static void
report_all_diffs(const char *what, unsigned char *const *a, int ex)
{
	int i;
	size_t j, run;

	for (i = 0; i < nreg; i++) {
		const unsigned char *p = (const unsigned char *)reg[i];

		for (j = 0; j < regsz[i]; j++) {
			unsigned wa, wb;
			size_t w;

			if (a[i][j] == p[j] || (ex && is_alias_byte(i, j)))
				continue;
			run = j;
			while (j + 1 < regsz[i] && a[i][j + 1] != p[j + 1]
			       && !(ex && is_alias_byte(i, j + 1)))
				j++;
			w = run & ~(size_t)3;
			wa = wb = 0;
			if (w + 4 <= regsz[i]) {
				memcpy(&wa, a[i] + w, 4);
				memcpy(&wb, p + w, 4);
			}
			if (i == root_idx)
				printf("      %s: the V.34 object +0x%lx"
				       " (%lu bytes)  word %08x/%08x\n", what,
				       (unsigned long)(run - ROOT_V34),
				       (unsigned long)(j - run + 1), wa, wb);
			else
				printf("      %s: region %d (%lu bytes) +0x%lx"
				       " (%lu bytes)  word %08x/%08x\n", what,
				       i, (unsigned long)regsz[i],
				       (unsigned long)run,
				       (unsigned long)(j - run + 1), wa, wb);
		}
	}
}

/* --- construction --------------------------------------------------------- */

static struct dp_operations *
vpcm_ops(void)
{
	int i;

	harness_reg_reset();
	ref_dp_vpcm_init();
	for (i = 0; i < harness_reg_ref.count; i++)
		if (harness_reg_ref.id[i] == 34)
			return harness_reg_ref.ops[i];
	return 0;
}

static int
build(struct dp_operations *ops, int caller)
{
	struct dp *d;

	memset(&dspinfo, 0, sizeof(dspinfo));
	harness_param_set(MDMPRM_DSPINFO, (long)(size_t)&dspinfo);
	harness_param_set(MDMPRM_CODECTYPE, CFG_CODECTYPE);

	runtime = (struct _tagModemParameters *)
		ref_dp_runtime_create((void *)0xD1A1u);
	if (runtime == 0)
		return 0;

	harness_param_set(MDMPRM_DPRUNTIME, (long)(size_t)runtime);
	harness_param_set(MDMPRM_MIN_RATE, CFG_MIN_RATE);
	harness_param_set(MDMPRM_MAX_RATE, CFG_MAX_RATE);
	harness_param_set(MDMPRM_IODELAY, CFG_IODELAY);

	d = ops->create((void *)0xD1A1u, 34, caller, CFG_SRATE, CFG_MAX_FRAG,
			ops);
	root = (char *)d;
	v34obj = root + ROOT_V34;
	return d != 0;
}

/* --- seeding the object with varied bytes --------------------------------- */

/*
 * THE FIXTURE'S ONE REAL BOUND, AND HOW TO GET ROUND IT.
 *
 * `ops->create` has already run `VPcmV34Create` once, so every case here is a
 * RE-initialisation of an initialised object -- and that makes the leading
 * memset nearly unobservable.  A byte the memset should have cleared and did
 * not is already 0 from the previous run, so shortening the memset changes
 * nothing and a differential sweep cannot see it.  That was measured, not
 * feared: `test/mutations/vpcmcreate.json`'s "the object memset is the
 * receiver's length" survived until this existed.
 *
 * So half the sweep SEEDS THE V.34 OBJECT WITH VARIED BYTES first, which is
 * what a fresh `sysdep_malloc` would hand it and never zero.  The seed is
 * deterministic and goes in identically on both legs, so it is not a source of
 * disagreement -- it is a source of OBSERVABILITY, and any byte the function
 * fails to write now shows the pattern instead of a previous run's zero.
 *
 * TWO WORDS ARE PUT BACK, and they are the two the function reads BEFORE the
 * memset and restores after it: +0x3548 and +0xac18.  Seeding those would be
 * handing the constructor a garbage `VPcmFloModem *` to dereference, which
 * tests nothing about `VPcmV34Create` and faults in a callee.
 */
static void
seed_v34(void)
{
	unsigned char *m = (unsigned char *)v34obj;
	const unsigned char *s0 = snap[root_idx] + ROOT_V34;
	unsigned i;

	for (i = 0; i < V34_LEN; i++)
		m[i] = (unsigned char)(0x5au + i * 0x6du + (i >> 5));

	memcpy(m + OB_P3548, s0 + OB_P3548, sizeof(void *));
	memcpy(m + OB_PAC18, s0 + OB_PAC18, sizeof(void *));
}

/* --- the configuration sweep ---------------------------------------------- */

/*
 * One configuration variant, written into the graph AFTER a restore.  Each
 * names the fork it is there to reach; 0 is the configuration `create` itself
 * left behind and is the one the fixture used to run on alone.
 */
static const char *
cfg_apply(int v)
{
	unsigned char *c = (unsigned char *)runtime;
	unsigned char *sess = *(unsigned char **)(v34obj + OB_P3548);
	unsigned char *k56 = *(unsigned char **)(v34obj + OB_PAC18);
	unsigned char *demod;

	switch (v) {
	case 0:
		return "as create left it";
	case 1:
		*(int *)(c + CFG_ENTRANCE) = 1;
		return "entrance filter forced on";
	case 2:
		*(int *)(c + CFG_ENTRANCE) = -1;
		*(int *)(c + CFG_F54) = 14;
		return "entrance filter from HW, and HW says yes";
	case 3:
		*(int *)(c + CFG_ENTRANCE) = -1;
		*(int *)(c + CFG_F54) = 3;
		return "entrance filter from HW, and HW says no";
	case 4:
		*(int *)(c + CFG_ENTRANCE) = 7;
		return "entrance filter off, by a value that is not 0";
	case 5:
		*(int *)(c + CFG_MINLEVEL) = -0x30;
		return "disconnect threshold, entry 0";
	case 6:
		*(int *)(c + CFG_MINLEVEL) = -0x29;
		return "disconnect threshold, entry 7";
	case 7:
		*(int *)(c + CFG_MINLEVEL) = 100;
		return "disconnect threshold, over the top -> entry 3";
	case 8:
		*(int *)(c + CFG_MINLEVEL) = -0x31;
		return "disconnect threshold, under the bottom -> entry 3";
	case 9:
		*(unsigned *)(c + CFG_MINRATE) = 64000u;
		*(unsigned *)(c + CFG_MAXRATE) = 64000u;
		return "rate_min over the index cap";
	case 10:
		*(unsigned *)(c + CFG_MINRATE) = 2400u;
		*(unsigned *)(c + CFG_MAXRATE) = 2400u;
		return "rate_max == 1, which forces the low baud rate";
	case 11:
		*(unsigned *)(c + CFG_MINRATE) = 24000u;
		*(unsigned *)(c + CFG_MAXRATE) = 2400u;
		return "rate_max below rate_min";
	case 12:
		*(unsigned *)(c + CFG_MINRATE) = 0u;
		*(unsigned *)(c + CFG_MAXRATE) = 0u;
		return "both rate limits zero";
	case 13:
		c[CFG_V92LITE] = (unsigned char)(c[CFG_V92LITE] | 0x10);
		*(int *)(c + CFG_UQTS) = -3;
		*(int *)(c + CFG_ANSPCM) = 0x1234;
		return "quick connect asked for, with two odd indices";
	case 14:
		c[CFG_V92LITE] = (unsigned char)(c[CFG_V92LITE] & ~0x10);
		return "quick connect refused";
	case 15:
		/*
		 * The V90ConstellationDesigner arm, and it is only turned on
		 * when the chain it dereferences is really there -- the point
		 * is to compare two implementations, not to fault both.
		 */
		if (sess == 0)
			return "the designer arm (no session)";
		demod = *(unsigned char **)(sess + SESS_DEMOD);
		if (demod == 0 || *(void **)(demod + DEMOD_DESIGN) == 0)
			return "the designer arm (no designer)";
		*(int *)(sess + SESS_GATE) = 1;
		return "the INFO0 gate on: the designer arm";
	case 16:
		if (k56 != 0)
			k56[K56_ENABLED] = 1;
		return "the K56flex object enabled: its setMinMaxRates arm";
	case 17:
		*(int *)(c + CFG_FILTDELAY) = -7;
		*(int *)(c + CFG_EXTDELAY) = -20;
		return "negative delays, for the arithmetic shift";
	case 18:
		*(unsigned *)(c + CFG_TXMD) = 37u;
		return "the 2.4 scale on a value that is not a multiple of 5";
	case 19:
		*(unsigned *)(c + CFG_TXMD) = 100000u;
		return "the 2.4 scale on a value that overflows the short";
	}
	return "?";
}

/* --- the three passes ----------------------------------------------------- */

/*
 * One (side, arg2) case.  `side` is `vpcm_create`'s `sete` of `caller` and
 * `arg2` is its own fourth stack argument; both are swept, because the object
 * branches on the first and this file has no independent statement of what the
 * second is.
 */
static void
one_case(int side, int sessionType, int arg3, int variant, int seed, long tag,
	 long *changedp, int *reportedp)
{
	int rc_ours, rc_blob;
	const char *what;
	long n;

	/*
	 * RESTORE, CHECK, THEN POKE, in that order.  The restore check has to
	 * see the graph exactly as `create` left it, and the variant has to be
	 * applied after it or the restore would undo it -- the dp-runtime
	 * block is one of the 125 regions being restored.
	 */
	graph_restore();
	diff_eq_int("the restore restored the graph (%ld)",
		    graph_diff_live(snap, 0) == 0 ? 1 : 0, 1, tag);
	what = cfg_apply(variant);
	if (seed)
		seed_v34();

	rc_ours = VPcmV34Create(v34obj, side, arg3, runtime, sessionType);
	graph_save_ours();

	/*
	 * ANTI-VACUITY.  Finding F805's passes 0 and 1 changed zero bytes and
	 * the probe correctly called them vacuous: "it ran and did not fault"
	 * on a constructed object is not a result.  The call has to have
	 * written something before two after-images agreeing means anything.
	 */
	n = graph_diff_bytes(ours, snap);
	diff_eq_int("our call changed the graph (%ld)", n > 0 ? 1 : 0, 1, tag);
	if (n > 0)
		*changedp = n;

	graph_restore();
	diff_eq_int("the restore restored it again (%ld)",
		    graph_diff_live(snap, 0) == 0 ? 1 : 0, 1, tag);
	cfg_apply(variant);
	if (seed)
		seed_v34();

	rc_blob = ref_VPcmV34Create(v34obj, side, arg3, runtime, sessionType);

	diff_eq_int("the return value (%ld)", rc_ours, rc_blob, tag);

	n = graph_diff_live(ours, 1);
	if (n != 0) {
		if (*reportedp < 8) {
			printf("    side %d type %d arg3 %#x cfg %d seed %d"
			       " (%s)\n", side, sessionType, (unsigned)arg3,
			       variant, seed, what);
			if (*reportedp == 0)
				report_all_diffs("after VPcmV34Create",
						 ours, 1);
			else
				report_first_diff("after VPcmV34Create",
						  ours, 1);
		}
		++*reportedp;
	}
	diff_eq_int("the whole graph, byte for byte (%ld differing bytes)",
		    n == 0 ? 1 : 0, 1, n);
}

static int
run_create(void)
{
	/*
	 * THE FIFTH ARGUMENT IS THE SESSION TYPE and it is the function's
	 * primary dispatch: a five-way switch on 0/1/2/3/4.  `vpcm_create`
	 * only ever passes 0, 1 or 2 -- it computes
	 * `(x == 0x5c) ? 2 : (x == 0x5a)` -- so 3 and 4 are unreachable from
	 * within this object and are swept here anyway, because a
	 * reconstruction has to agree on them too.  A negative value takes
	 * the same arm as 0: the dispatch's second test is a SIGNED `jle`.
	 */
	static const int type_v[] = { 0, 1, 2, 3, 4, -1 };
#define NTYPE ((int)(sizeof(type_v) / sizeof(type_v[0])))
	/*
	 * The third argument lands in +0x8 and is read by nothing else here,
	 * so two values is the whole of what there is to say about it -- but
	 * zero alone could not tell "stores the argument" from "stores zero".
	 */
	static const int arg3_v[] = { 0, (int)0x55aa1234 };
#define NARG3 ((int)(sizeof(arg3_v) / sizeof(arg3_v[0])))
	int side, ai, gi, ci, sd;
	long tag = 0;
	long changed = 0;
	int reported = 0;

	diff_begin("VPcmV34Create against the blob's, over the whole graph");

	for (side = 0; side < 2; side++)
		for (ai = 0; ai < NTYPE; ai++)
			for (gi = 0; gi < NARG3; gi++)
				for (ci = 0; ci < NCFG; ci++)
					for (sd = 0; sd < 2; sd++)
						one_case(side, type_v[ai],
							 arg3_v[gi], ci, sd,
							 tag++, &changed,
							 &reported);

	diff_eq_int("some case changed the graph", changed > 0 ? 1 : 0, 1,
		    changed);
	diff_eq_int("and the twelve root alias words were still alias pairs"
		    " after the sweep (%ld bad)", check_alias_words(0), 0, 0);
	diff_eq_int("and no case was reported as differing (%ld)", reported, 0,
		    reported);

	return diff_end();
}

/*
 * THE FOURTEEN DIAGNOSTICS, WHICH THE SWEEP ABOVE CANNOT SEE AT ALL.
 *
 * `main` runs at debug level 0 and `DSPLIB_DEBUG_ON()` is `> 1`, so thirteen
 * of this function's fourteen print sites are dark in every one of the 960
 * cases above and the fourteenth writes nothing to the heap.  A wrong format
 * string, a wrong argument, or a whole announcement dropped would all pass.
 * `debugaudit.py --invented` proves the STRINGS are the blob's; only this pass
 * says anything about the arguments, and one of them needed saying -- the
 * object folds the `+0x35a4` load in "Setting desired TX MD" to a constant,
 * so a literal 0 and a field read are the same code and only the transcript
 * distinguishes them from a wrong field.
 *
 * `t_v34retrain.c` carries the same pass for the neighbouring function and its
 * five diagnostic mutations are all recorded caught, which is what says the
 * mechanism works rather than that it ran.
 *
 * EIGHT CONFIGURATIONS, NOT TWENTY: the ones that separate the print sites
 * from each other -- the three entrance-filter arms, the threshold fallback,
 * quick connect and the negative delays.  The other twelve differ only in
 * values the sweep above already compares.
 *
 * THE ONE THING THIS DOES NOT REACH is the ungated `edprintf` in arms 3 and 4.
 * It is not `dsplibs_debug_printf` and the harness does not interpose it, so
 * its two sides encode through two separate rotating counters and cannot be
 * compared as text.  Its argument is `k56 + 0xc`, which the sweep above does
 * compare, and its string is `debugaudit`'s; the call itself is unverified and
 * is the only part of this function that is.
 */
static int
run_transcripts(void)
{
	static const int type_v[] = { 0, 1, 2, 3, 4, -1 };
	static const int cfg_v[] = { 0, 1, 2, 3, 4, 7, 13, 17 };
	int side, ai, ci;
	long tag = 0;
	int printed = 0;

	diff_begin("VPcmV34Create's diagnostics, both sides talking");

	for (side = 0; side < 2; side++)
		for (ai = 0; ai < (int)(sizeof type_v / sizeof type_v[0]); ai++)
			for (ci = 0;
			     ci < (int)(sizeof cfg_v / sizeof cfg_v[0]); ci++) {
				graph_restore();
				cfg_apply(cfg_v[ci]);

				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();
				dsplibs_debug_level = 2;
				ref_dsplibs_debug_level = 2;

				VPcmV34Create(v34obj, side, 0, runtime,
					      type_v[ai]);
				graph_save_ours();
				graph_restore();
				cfg_apply(cfg_v[ci]);
				ref_VPcmV34Create(v34obj, side, 0, runtime,
						  type_v[ai]);

				dsplibs_debug_level = 0;
				ref_dsplibs_debug_level = 0;
				dsplib_debug_capture_on = 0;

				diff_eq_int("the transcripts agree (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0 ? 1 : 0, 1, tag);
				diff_eq_int("and the graph still agrees (%ld)",
					    graph_diff_live(ours, 1) == 0
					    ? 1 : 0, 1, tag);
				if (dsplib_debug_capture_lines(1) > 0)
					printed = 1;
				tag++;
			}

	/*
	 * ANTI-VACUITY.  Two empty transcripts compare equal, which is exactly
	 * the state every case in `run_create` is in.
	 */
	diff_eq_int("and some case actually printed something", printed, 1, 0);

	graph_restore();

	return diff_end();
}

/*
 * THE COMPARISON MADE TO FAIL.  Finding F805's pass 3 is the model: run ours,
 * poke one byte of the graph, and require the comparison to say so.  Without
 * this the whole file could be comparing an image against itself.
 */
static int
run_made_to_fail(void)
{
	long n;
	unsigned char *p;

	diff_begin("the VPcmV34Create fixture: the comparison is live");

	graph_restore();
	VPcmV34Create(v34obj, 0, 0, runtime, 0);
	graph_save_ours();
	graph_restore();
	ref_VPcmV34Create(v34obj, 0, 0, runtime, 0);

	diff_eq_int("undisturbed, the two agree",
		    graph_diff_live(ours, 1) == 0 ? 1 : 0, 1, 0);

	/* One byte of the V.34 object, changed on the blob's side only. */
	p = (unsigned char *)v34obj + 0x260;
	*p = (unsigned char)(*p ^ 0xffu);
	n = graph_diff_live(ours, 1);
	diff_eq_int("one flipped byte is reported (%ld)", n > 0 ? 1 : 0, 1, n);
	*p = (unsigned char)(*p ^ 0xffu);
	diff_eq_int("and putting it back silences it",
		    graph_diff_live(ours, 1) == 0 ? 1 : 0, 1, 0);

	graph_restore();

	return diff_end();
}

/*
 * VIRGIN AGAINST RE-INIT, measured rather than assumed.
 *
 * `ops->create` has already run `VPcmV34Create` once, so every case above is a
 * re-initialisation.  If the function is idempotent -- if running it a second
 * time on its own output changes nothing -- then the re-init path and the
 * virgin path leave the same state and the bound above costs nothing.  If it
 * is NOT, the difference is what a virgin-only arm would look like, and this
 * reports how many bytes it is rather than asserting either answer.
 */
static int
run_virgin_vs_reinit(void)
{
	long n;

	diff_begin("the VPcmV34Create fixture: re-init over create's own output");

	/*
	 * THE SIDE HAS TO MATCH THE ONE `create` USED, or this measures
	 * finding F803's fourteen caller-configured bytes instead of the
	 * re-initialisation.  `vpcm_create` does `test %edx,%edx / sete` on
	 * `caller`, so a caller of 1 gives a side of 0.
	 */
	graph_restore();
	ref_VPcmV34Create(v34obj, 0, 0, runtime, 0);
	n = graph_diff_live(snap, 0);
	printf("    a second VPcmV34Create at the SAME side moves %ld byte(s)"
	       " of %d region(s)\n", n, nreg);
	report_all_diffs("same side", snap, 0);

	graph_restore();
	ref_VPcmV34Create(v34obj, 1, 0, runtime, 0);
	n = graph_diff_live(snap, 0);
	printf("    and at the OTHER side, %ld byte(s)\n", n);
	report_all_diffs("other side", snap, 0);
	diff_eq_int("the measurement ran", nreg > 0 ? 1 : 0, 1, nreg);

	graph_restore();

	return diff_end();
}

/*
 * WHERE THE EXCLUDED WORDS COME FROM, established rather than claimed, AND
 * THIS PASS IS WHAT BUILDS THE EXCLUSION -- nothing below it is hand-listed.
 *
 * `VPcmV34Create` calls three functions that install addresses of STATIC
 * objects into the graph, and each of the three is run here alone, ours
 * against the blob's, from the same snapshot and on the same memory.  Every
 * aligned word they disagree on is marked, and the sweep excludes exactly the
 * marked words.
 *
 *   - `V34InitializeImplementationSpecific` must disagree NOWHERE, so it
 *     contributes nothing to the mask and the assertion says so.
 *   - `v34handshakinit` contributes twelve words of the V.34 object -- ten
 *     coefficient tables and two scrambler entry points.
 *   - `V90Demodulator::enterChannelVerification`, which only the quick-connect
 *     arm reaches, contributes ONE word of a 668-byte sub-object, holding
 *     `V90PreFilter::preFilterCoefType1`.
 *
 * WHAT BOUNDS THE HOLE.  Thirteen words is 52 bytes of 265,520, none of the
 * thirteen offsets appears anywhere in `VPcmV34Create`'s own transcription,
 * and `check_alias_words` requires each of the twelve in the root to hold two
 * DIFFERENT addresses whose targets are byte-identical over the size of the
 * object named -- or, for the two function slots, our symbol against the
 * blob's `ref_` alias.  So "these are pointers into two images of the same
 * data" is measured three ways and not assumed once.
 *
 * The member function is reached by its MANGLED NAME declared as a C
 * identifier.  `_ZN14V90Demodulator24enterChannelVerificationEss` is a legal C
 * identifier and a non-virtual member's `this` is an ordinary leading
 * argument on this ABI, so a C test can call one; the alternative was to make
 * this file C++ for a single call in a measurement pass.
 */
extern void V34InitializeImplementationSpecific(void *obj);
extern void ref_V34InitializeImplementationSpecific(void *obj);
extern void v34handshakinit(void *obj, int mode);
extern void ref_v34handshakinit(void *obj, int mode);
extern void _ZN14V90Demodulator24enterChannelVerificationEss(void *thisp,
							    short a, short b);
extern void ref__ZN14V90Demodulator24enterChannelVerificationEss(void *thisp,
								 short a,
								 short b);

static int
run_alias_words(void)
{
	unsigned char *sess = *(unsigned char **)(v34obj + OB_P3548);
	void *demod = *(void **)(sess + SESS_DEMOD);
	long n, w;

	diff_begin("the excluded words, and which callee installs each");

	graph_restore();
	V34InitializeImplementationSpecific(v34obj);
	graph_save_ours();
	graph_restore();
	ref_V34InitializeImplementationSpecific(v34obj);
	n = graph_diff_live(ours, 0);
	if (n != 0)
		report_all_diffs("V34InitializeImplementationSpecific", ours,
				 0);
	diff_eq_int("V34InitializeImplementationSpecific agrees everywhere"
		    " (%ld)", n == 0 ? 1 : 0, 1, n);

	graph_restore();
	v34handshakinit(v34obj, 0);
	graph_save_ours();
	graph_restore();
	ref_v34handshakinit(v34obj, 0);
	w = alias_learn();
	diff_eq_int("v34handshakinit installs twelve alias words (%ld)", w, 12,
		    w);
	diff_eq_int("and every one of the twelve is really an alias pair (%ld"
		    " bad)", check_alias_words(0), 0, 0);
	n = graph_diff_live(ours, 1);
	diff_eq_int("and disagrees on nothing else (%ld)", n == 0 ? 1 : 0, 1,
		    n);

	graph_restore();
	_ZN14V90Demodulator24enterChannelVerificationEss(demod, -3, 0x1234);
	graph_save_ours();
	graph_restore();
	ref__ZN14V90Demodulator24enterChannelVerificationEss(demod, -3, 0x1234);
	w = alias_learn();
	diff_eq_int("enterChannelVerification installs one more (%ld)", w, 1,
		    w);
	n = graph_diff_live(ours, 1);
	if (n != 0)
		report_all_diffs("enterChannelVerification, outside the mask",
				 ours, 1);
	diff_eq_int("and disagrees on nothing else (%ld)", n == 0 ? 1 : 0, 1,
		    n);

	alias_report();

	graph_restore();

	return diff_end();
}

/*
 * A HangUp history on the existing registered-create graph.  No states are
 * planted: history 0 uses ops->create's output, and histories 1/2 reinitialize
 * it with the exported constructor for the two V.34 roles.  These histories
 * all leave status 0.  They cover the non-PCM request boundary and a repeated
 * request, not negotiated status 1/2, audio processing, or diagnostic reentry.
 * Each implementation starts from the same restored, blob-created graph.
 */
static int
hangup_int(unsigned off)
{
	int v;
	memcpy(&v, v34obj + off, sizeof v);
	return v;
}

static int
hangup_short(unsigned off)
{
	short v;
	memcpy(&v, v34obj + off, sizeof v);
	return v;
}

/* Keep every learned exclusion outside preinitdigital's four words visible. */
static long
hangup_unchanged_alias_diff(void)
{
	int i;
	size_t j;
	long bad = 0;
	for (i = 0; i < nreg; ++i)
	for (j = 0; j < regsz[i]; ++j) {
		size_t off = j - ROOT_V34;
		if (i == root_idx
		    && ((off >= 0x0a28 && off < 0x0a2c)
			|| (off >= 0x0e48 && off < 0x0e4c)
			|| (off >= 0x2608 && off < 0x260c)
			|| (off >= 0x2a28 && off < 0x2a2c)))
			continue;
		if (is_alias_byte(i, j)
		    && ours[i][j] != ((unsigned char *)reg[i])[j])
			++bad;
	}
	return bad;
}

static int
run_hangup_history(void)
{
	int history, level, requests, side, k;
	int cases = 0;
	long calls = 0;

	diff_begin("VPcmV34InitiateHangUp: constructed status-0 histories");
	for (history = 0; history < 3; ++history)
	for (level = 0; level < 3; ++level)
	for (requests = 1; requests <= 2; ++requests) {
		long tag = history * 100 + level * 10 + requests;
		for (side = 0; side < 2; ++side) {
			int rate_now;
			graph_restore();
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;
			if (history != 0)
				ref_VPcmV34Create(v34obj, history - 1, 0,
						  runtime, 0);
			diff_eq_int("HangUp constructed status", hangup_int(0),
				    0, tag);
			rate_now = hangup_int(0x228);
			if (side == 0) {
				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();
			}
			dsplibs_debug_level = ref_dsplibs_debug_level = level;
			for (k = 0; k < requests; ++k) {
				if (side == 0)
					VPcmV34InitiateHangUp(v34obj);
				else
					ref_VPcmV34InitiateHangUp(v34obj);
				++calls;
			}
			diff_eq_int("HangUp progress", hangup_int(4), 6, tag);
			diff_eq_int("HangUp mode", hangup_int(0x2218), 5, tag);
			diff_eq_int("HangUp rate min", hangup_int(0x220), 0, tag);
			diff_eq_int("HangUp rate max", hangup_int(0x224), 0, tag);
			diff_eq_int("HangUp rate want", hangup_int(0x22c), 0, tag);
			diff_eq_int("HangUp keeps current rate", hangup_int(0x228),
				    rate_now, tag);
			diff_eq_int("HangUp bad run", hangup_short(0x4bc), 0, tag);
			diff_eq_int("HangUp bad long run", hangup_short(0x4be),
				    0, tag);
			diff_eq_int("HangUp good run", hangup_short(0x4c0), 0, tag);
			if (side == 0)
				graph_save_ours();
		}
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		if (graph_diff_live(ours, 1) != 0)
			report_first_diff("HangUp constructed graph", ours, 1);
		diff_eq_int("HangUp constructed graph", graph_diff_live(ours, 1),
			    0, tag);
		diff_eq_int("HangUp shell static aliases", check_alias_words(1), 0,
			    tag);
		diff_eq_int("HangUp unchanged static aliases",
			    hangup_unchanged_alias_diff(), 0, tag);
		diff_eq_int("HangUp transcript complete ours",
			    dsplib_debug_capture_complete(0), 1, tag);
		diff_eq_int("HangUp transcript complete blob",
			    dsplib_debug_capture_complete(1), 1, tag);
		diff_eq_int("HangUp diagnostic call boundaries",
			    dsplib_debug_capture_boundaries_equal(), 1, tag);
		diff_eq_int("HangUp diagnostic bytes",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		diff_eq_int("HangUp diagnostics active ours",
			    dsplib_debug_capture_lines(0) > 0, level > 1, tag);
		diff_eq_int("HangUp diagnostics active blob",
			    dsplib_debug_capture_lines(1) > 0, level > 1, tag);
		++cases;
	}
	diff_eq_int("HangUp constructed history case denominator", cases, 18, 0);
	diff_eq_int("HangUp constructed call denominator", calls, 54, 0);
	graph_restore();
	return diff_end();
}

/*
 * InitMOH at the initialized component boundary, using the same registered
 * graph and the two constructor roles as HangUp.  The method has no internal
 * caller in the blob: this does not establish negotiated MOH reachability.
 * No status, handshake state, or graph pointer is planted.  Byte arguments
 * here are 0/1; t_v34pcmapi's patterned-state probes retain their wider byte
 * and message domains as synthetic fidelity coverage.
 *
 * Mode 4 installs ten root alias words: it does not run mode 0's timing-filter
 * initializer, so +0x620/+0x624 stay unchanged.  Check all ten pairs and require
 * every other excluded byte to agree.  No new mask is learned for this method.
 */
static long
reset_unchanged_alias_diff(void)
{
	int i, k;
	size_t j;
	long bad = 0;
	for (i = 0; i < nreg; ++i)
	for (j = 0; j < regsz[i]; ++j) {
		int installed = 0;
		if (i == root_idx)
			for (k = 0; k < NALIAS; ++k)
				if (aliasword[k].off != 0x0620
				    && aliasword[k].off != 0x0624
				    && j >= ROOT_V34 + aliasword[k].off
				    && j < ROOT_V34 + aliasword[k].off + 4)
					installed = 1;
		if (!installed && is_alias_byte(i, j)
		    && ours[i][j] != ((unsigned char *)reg[i])[j])
			++bad;
	}
	return bad;
}

static int
run_initmoh_history(void)
{
	int history, level, message, late, flag, side, request;
	int cases = 0;
	long calls = 0;

	diff_begin("VPcmV34InitMOH: initialized component histories");
	for (history = 0; history < 3; ++history)
	for (level = 0; level < 3; ++level)
	for (message = 0; message < 3; ++message)
	for (late = 0; late < 2; ++late)
	for (flag = 0; flag < 2; ++flag) {
		long tag = history * 100 + level * 30 + message * 4 + late * 2 + flag;
		for (side = 0; side < 2; ++side) {
			graph_restore();
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;
			if (history != 0)
				ref_VPcmV34Create(v34obj, history - 1, 0,
						  runtime, 0);
			diff_eq_int("InitMOH constructed status", hangup_int(0),
				    0, tag);
			if (side == 0) {
				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();
			}
			dsplibs_debug_level = ref_dsplibs_debug_level = level;
			/* Repeated request over the first method's own output. */
			for (request = 0; request < 2; ++request) {
				if (side == 0)
					VPcmV34InitMOH(v34obj, message,
						(unsigned char)late, (unsigned char)flag);
				else
					ref_VPcmV34InitMOH(v34obj, message,
						(unsigned char)late, (unsigned char)flag);
				++calls;
				diff_eq_int("InitMOH progress", hangup_int(4), 7, tag);
				diff_eq_int("InitMOH status", hangup_int(0), 0, tag);
				diff_eq_int("InitMOH mode", hangup_int(0x2218), 2, tag);
				/* First transmitted word, produced by the real mode-4
				 * handshake/message builder, not a planted bit source. */
				diff_eq_int("InitMOH generated message word",
					    hangup_short(0xa94c),
					    message == 2 ? (late ? 0x96 : 0x95) : 0x33,
					    tag);
			}
			if (side == 0)
				graph_save_ours();
		}
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		if (graph_diff_live(ours, 1) != 0)
			report_first_diff("InitMOH constructed graph", ours, 1);
		diff_eq_int("InitMOH constructed graph", graph_diff_live(ours, 1),
			    0, tag);
		diff_eq_int("InitMOH ten static aliases", check_alias_words(2),
			    0, tag);
		diff_eq_int("InitMOH unchanged static aliases",
			    reset_unchanged_alias_diff(), 0, tag);
		diff_eq_int("InitMOH transcript complete ours",
			    dsplib_debug_capture_complete(0), 1, tag);
		diff_eq_int("InitMOH transcript complete blob",
			    dsplib_debug_capture_complete(1), 1, tag);
		diff_eq_int("InitMOH diagnostic call boundaries",
			    dsplib_debug_capture_boundaries_equal(), 1, tag);
		diff_eq_int("InitMOH diagnostic bytes",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		diff_eq_int("InitMOH diagnostics active ours",
			    dsplib_debug_capture_lines(0) > 0, level > 1, tag);
		diff_eq_int("InitMOH diagnostics active blob",
			    dsplib_debug_capture_lines(1) > 0, level > 1, tag);
		++cases;
	}
	diff_eq_int("InitMOH constructed case denominator", cases, 108, 0);
	diff_eq_int("InitMOH constructed call denominator", calls, 432, 0);
	/* Calibrate the new mask check on saved bytes, never live DSP state. */
	{
		size_t off = (size_t)ROOT_V34 + 0x620;
		unsigned char saved = ours[root_idx][off];
		ours[root_idx][off] ^= 1;
		diff_eq_int("InitMOH untouched mask detects one changed byte",
			    reset_unchanged_alias_diff(), 1, 0);
		ours[root_idx][off] = saved;
		diff_eq_int("InitMOH untouched mask restored",
			    reset_unchanged_alias_diff(), 0, 0);
		off = (size_t)ROOT_V34 + 0xa28;
		saved = ours[root_idx][off];
		ours[root_idx][off] ^= 1;
		diff_eq_int("InitMOH installed alias excluded from raw mask check",
			    reset_unchanged_alias_diff(), 0, 0);
		ours[root_idx][off] = saved;
		diff_eq_int("InitMOH saved alias bytes restored",
			    reset_unchanged_alias_diff(), 0, 0);
	}
	graph_restore();
	return diff_end();
}


/*
 * Retrain on the registered-create graph, without planting status, receiver
 * flags, state-machine values, or pointers.  Histories 0/1/2 are the factory
 * output and the two exported-constructor roles.  Every call starts at status
 * 0; the repeat uses the method's own output.  This is an initialized component
 * boundary, not evidence of negotiated PCM status or a remote retrain.
 *
 * Mode 1, like mode 4, runs v34modeminit but not rxtiminginit: ten static
 * aliases are installed.  The timing words +0x620/+0x624 and every other
 * learned exclusion remain visible through reset_unchanged_alias_diff.
 */
static int
run_retrain_history(void)
{
	static const unsigned char dp[] = { 0, 34, 56, 90, 92 };
	int history, level, choice, requests, side, request;
	int cases = 0;
	long calls = 0;
	long rate_arms[3] = { 0, 0, 0 };
	long accepted_pcm[2] = { 0, 0 };
	size_t graph_bytes = 0, masked_bytes = 0;
	int region;

	diff_begin("VPcmV34InitiateRetrain: initialized component histories");
	for (history = 0; history < 3; ++history)
	for (level = 0; level < 3; ++level)
	for (choice = 0; choice < 5; ++choice)
	for (requests = 1; requests <= 2; ++requests) {
		long tag = history * 1000 + level * 100 + choice * 10 + requests;
		for (side = 0; side < 2; ++side) {
			graph_restore();
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;
			if (history != 0)
				ref_VPcmV34Create(v34obj, history - 1, 0,
						  runtime, 0);
			diff_eq_int("Retrain constructed status", hangup_int(0), 0, tag);
			if (side == 0) {
				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();
			}
			dsplibs_debug_level = ref_dsplibs_debug_level = level;
			for (request = 0; request < requests; ++request) {
				if (side == 0) {
					unsigned char *sess, *k56;
					int gate;
					memcpy(&sess, v34obj + OB_P3548, sizeof sess);
					memcpy(&k56, v34obj + OB_PAC18, sizeof k56);
					memcpy(&gate, sess + SESS_GATE, sizeof gate);
					if (hangup_int(0x24c) != 0 && gate != 0)
						++rate_arms[0];
					else if (hangup_int(0x250) != 0
						 && k56[K56_ENABLED] != 0)
						++rate_arms[1];
					else
						++rate_arms[2];
					VPcmV34InitiateRetrain(v34obj, dp[choice]);
					if (dp[choice] == 90 || dp[choice] == 92)
						accepted_pcm[dp[choice] == 92] +=
							hangup_int(0x24c) != 0;
				} else
					ref_VPcmV34InitiateRetrain(v34obj, dp[choice]);
				++calls;
				diff_eq_int("Retrain progress", hangup_int(4), 7, tag);
				diff_eq_int("Retrain status", hangup_int(0), 0, tag);
				diff_eq_int("Retrain mode", hangup_int(0x2218), 2, tag);
				diff_eq_int("Retrain bad run", hangup_short(0x4bc), 0, tag);
				diff_eq_int("Retrain bad long run", hangup_short(0x4be), 0, tag);
				diff_eq_int("Retrain good run", hangup_short(0x4c0), 0, tag);
				diff_eq_int("Retrain DC estimate", hangup_short(0x256), 0, tag);
				diff_eq_int("Retrain DC accumulator", hangup_int(0x258), 0, tag);
			}
			if (side == 0)
				graph_save_ours();
		}
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		if (graph_diff_live(ours, 1) != 0)
			report_first_diff("Retrain constructed graph", ours, 1);
		diff_eq_int("Retrain constructed graph", graph_diff_live(ours, 1), 0, tag);
		diff_eq_int("Retrain ten static aliases", check_alias_words(2), 0, tag);
		diff_eq_int("Retrain unchanged static aliases",
			    reset_unchanged_alias_diff(), 0, tag);
		diff_eq_int("Retrain transcript complete ours",
			    dsplib_debug_capture_complete(0), 1, tag);
		diff_eq_int("Retrain transcript complete blob",
			    dsplib_debug_capture_complete(1), 1, tag);
		diff_eq_int("Retrain diagnostic call boundaries",
			    dsplib_debug_capture_boundaries_equal(), 1, tag);
		diff_eq_int("Retrain diagnostic bytes",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		diff_eq_int("Retrain diagnostics active ours",
			    dsplib_debug_capture_lines(0) > 0, level > 1, tag);
		diff_eq_int("Retrain diagnostics active blob",
			    dsplib_debug_capture_lines(1) > 0, level > 1, tag);
		++cases;
	}
	diff_eq_int("Retrain constructed case denominator", cases, 90, 0);
	diff_eq_int("Retrain constructed call denominator", calls, 270, 0);
	diff_eq_int("Retrain rate-arm observed call denominator",
		    rate_arms[0] + rate_arms[1] + rate_arms[2], 135, 0);
	for (region = 0; region < nreg; ++region) {
		size_t byte;
		graph_bytes += regsz[region];
		for (byte = 0; byte < regsz[region]; ++byte)
			masked_bytes += is_alias_byte(region, byte) != 0;
	}
	printf("    retrain graph: %d regions, %lu bytes; %lu learned mask bytes, "
	       "40 installed alias bytes checked as ten pairs, other masked bytes raw\n",
	       nreg, (unsigned long)graph_bytes, (unsigned long)masked_bytes);
	printf("    retrain component: %d cases, %ld calls, status 0 only; "
	       "ours rate arms designer/K56/V34 %ld/%ld/%ld of 135; "
	       "accepted V90/V92 %ld/%ld of 27 each\n", cases, calls,
	       rate_arms[0], rate_arms[1], rate_arms[2], accepted_pcm[0], accepted_pcm[1]);
	/* Calibrate only saved observation bytes, never live DSP state. */
	{
		size_t off = (size_t)ROOT_V34 + 0x620;
		unsigned char saved = ours[root_idx][off];
		ours[root_idx][off] ^= 1;
		diff_eq_int("Retrain untouched mask detects changed byte",
			    reset_unchanged_alias_diff(), 1, 0);
		ours[root_idx][off] = saved;
		diff_eq_int("Retrain untouched mask restored",
			    reset_unchanged_alias_diff(), 0, 0);
		off = (size_t)ROOT_V34 + 0xa28;
		saved = ours[root_idx][off];
		ours[root_idx][off] ^= 1;
		diff_eq_int("Retrain installed alias excluded from raw mask check",
			    reset_unchanged_alias_diff(), 0, 0);
		ours[root_idx][off] = saved;
		diff_eq_int("Retrain saved alias bytes restored",
			    reset_unchanged_alias_diff(), 0, 0);
	}
	graph_restore();
	return diff_end();
}


/*
 * Exercise the public out-of-line helpers before any handshake can overwrite
 * their output.  The registered graph supplies all owners and pointers.  The
 * second configuration supplies ordinary caller-owned runtime values at this
 * component boundary: rates 2400..14400, level -48, delays 64/200.  It does not
 * plant negotiated state or establish that a modem call produces these values.
 * No helper installs a static alias, so compare every graph byte without masks.
 */
static int
run_retrain_helper_history(void)
{
	void (*const fn[2][3])(void *) = {
		{ fixture_set_rates, fixture_set_floor, fixture_set_delays },
		{ ref_fixture_set_rates, ref_fixture_set_floor, ref_fixture_set_delays }
	};
	int history, level, config, method, side;
	int cases = 0;
	long calls = 0, observed_changes = 0;

	diff_begin("VPcmV34 retrain helpers: constructed owner/configuration boundary");
	for (history = 0; history < 3; ++history)
	for (level = 0; level < 3; ++level)
	for (config = 0; config < 2; ++config)
	for (method = 0; method < 3; ++method) {
		long tag = history * 1000 + level * 100 + config * 10 + method;
		for (side = 0; side < 2; ++side) {
			int before, after;
			graph_restore();
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;
			if (history != 0)
				ref_VPcmV34Create(v34obj, history - 1, 0, runtime, 0);
			diff_eq_int("helper constructed status", hangup_int(0), 0, tag);
			diff_eq_int("helper constructed V90 receiver", hangup_int(0x24c), 0, tag);
			diff_eq_int("helper constructed K56 receiver", hangup_int(0x250), 0, tag);
			if (config != 0) {
				runtime->vpcmRateLimitLow = 2400;
				runtime->vpcmRateLimitHigh = 14400;
				runtime->unnamed_0060 = -48;
				runtime->hwDelay = 64;
				runtime->dmaDelay = 200;
			}
			before = method == 0 ? hangup_int(0x220)
				: method == 1 ? hangup_int(0x230) : hangup_short(0xaa7c);
			if (side == 0) {
				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();
			}
			dsplibs_debug_level = ref_dsplibs_debug_level = level;
			fn[side][method](v34obj);
			++calls;
			after = method == 0 ? hangup_int(0x220)
				: method == 1 ? hangup_int(0x230) : hangup_short(0xaa7c);
			if (config != 0) {
				diff_eq_int("helper configured output changed", after != before, 1, tag);
				if (side == 0 && after != before)
					++observed_changes;
			}
			if (side == 0)
				graph_save_ours();
		}
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		if (graph_diff_live(ours, 0) != 0)
			report_first_diff("retrain helper raw graph", ours, 0);
		diff_eq_int("retrain helper raw graph", graph_diff_live(ours, 0), 0, tag);
		diff_eq_int("helper transcript complete ours",
			    dsplib_debug_capture_complete(0), 1, tag);
		diff_eq_int("helper transcript complete blob",
			    dsplib_debug_capture_complete(1), 1, tag);
		diff_eq_int("helper diagnostic call boundaries",
			    dsplib_debug_capture_boundaries_equal(), 1, tag);
		diff_eq_int("helper diagnostic bytes",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		diff_eq_int("helper diagnostics active ours",
			    dsplib_debug_capture_lines(0) > 0, level > 1 && method != 0, tag);
		diff_eq_int("helper diagnostics active blob",
			    dsplib_debug_capture_lines(1) > 0, level > 1 && method != 0, tag);
		++cases;
	}
	diff_eq_int("helper constructed case denominator", cases, 54, 0);
	diff_eq_int("helper constructed call denominator", calls, 108, 0);
	diff_eq_int("helper configured changed-output denominator", observed_changes, 27, 0);
	printf("    retrain helpers: %d cases, %ld calls; all graph bytes raw; "
	       "%ld/27 configured outputs changed\n", cases, calls, observed_changes);
	graph_restore();
	return diff_end();
}

/* Constructed component inputs, before protocol RTD negotiation. The native
 * constructor supplies bulk_len=9600 and the real ring/echo owners. No internal
 * state is planted. Zero and saturation are method-boundary probes, not claims
 * that each selected delay occurs on a modem call. This helper installs no
 * static aliases, so compare all graph bytes immediately with no masks. */
static int
run_bulkdelay_history(void)
{
	static const short delays[] = { 0, 1, 29, 30, 143, 144, 9599, 9600, 32767 };
	int history, level, side;
	unsigned d;
	int cases = 0;
	long calls = 0, active = 0;

	diff_begin("ApplyBulkDelay: constructed ring and echo histories");
	for (history = 0; history < 3; ++history)
	for (level = 0; level < 3; ++level)
	for (d = 0; d < sizeof(delays) / sizeof(delays[0]); ++d) {
		long tag = history * 1000 + level * 100 + d;
		for (side = 0; side < 2; ++side) {
			int before_tail, before_far;
			short before_dma;
			graph_restore();
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;
			if (history != 0)
				ref_VPcmV34Create(v34obj, history - 1, 0, runtime, 0);
			diff_eq_int("bulk constructed status", hangup_int(0), 0, tag);
			diff_eq_int("bulk constructed ring length", hangup_int(0x35b4), 9600, tag);
			before_tail = hangup_int(0x35ac);
			before_far = hangup_short(0xa23c);
			before_dma = hangup_short(0x25c);
			if (side == 0) {
				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();
			}
			dsplibs_debug_level = ref_dsplibs_debug_level = level;
			if (side == 0)
				ApplyBulkDelay(v34obj, delays[d]);
			else
				ref_ApplyBulkDelay(v34obj, delays[d]);
			++calls;
			if (side == 0) {
				if (hangup_int(0x35ac) != before_tail
				    || hangup_short(0xa23c) != before_far
				    || hangup_short(0x25c) != before_dma)
					++active;
				graph_save_ours();
			}
		}
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		if (graph_diff_live(ours, 0) != 0)
			report_first_diff("bulk raw graph", ours, 0);
		diff_eq_int("bulk raw graph", graph_diff_live(ours, 0), 0, tag);
		diff_eq_int("bulk transcript complete ours", dsplib_debug_capture_complete(0), 1, tag);
		diff_eq_int("bulk transcript complete blob", dsplib_debug_capture_complete(1), 1, tag);
		diff_eq_int("bulk callback boundaries", dsplib_debug_capture_boundaries_equal(), 1, tag);
		diff_eq_int("bulk transcript bytes", strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)) == 0, 1, tag);
		diff_eq_int("bulk diagnostics active", dsplib_debug_capture_lines(1) > 0, level > 1, tag);
		++cases;
	}
	diff_eq_int("bulk constructed cases", cases, 81, 0);
	diff_eq_int("bulk constructed calls", calls, 162, 0);
	diff_eq_int("bulk changes constructed output", active, cases, 0);
	printf("    bulk component: %d cases, %ld calls, %ld active; all graph bytes raw\n", cases, calls, active);
	graph_restore();
	return diff_end();
}

/* Valid allocated component owners, with optional native modem initialization.
 * The external MP words exercise this method's consumed power fields; this is
 * not evidence that each word was received on a negotiated modem connection.
 * No internal state is planted, and neither method installs static aliases. */
static int
run_tx_component_history(void)
{
	static const short mp_words[] = { 0, 0x20, 0xe0, 0xfc, (short)0xffff };
	int history, initialized, level, method, side;
	int cases = 0, calls = 0, active[2] = { 0, 0 };

	diff_begin("V34 TX: constructed reset and power component histories");
	for (history = 0; history < 3; ++history)
	for (initialized = 0; initialized < 2; ++initialized)
	for (level = 0; level < 3; ++level)
	for (method = 0; method < 6; ++method) {
		long tag = history * 1000 + initialized * 100 + level * 10 + method;
		short mp = method == 0 ? 0 : mp_words[method - 1];
		for (side = 0; side < 2; ++side) {
			graph_restore();
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;
			if (history != 0)
				ref_VPcmV34Create(v34obj, history - 1, 0, runtime, 0);
			if (initialized)
				ref_v34modeminit(v34obj);
			if (side == 0) {
				graph_save_ours();
				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();
			}
			dsplibs_debug_level = ref_dsplibs_debug_level = level;
			if (method == 0) {
				if (side == 0) txinit(v34obj);
				else ref_txinit(v34obj);
			} else {
				if (side == 0) settxlevel(v34obj, &mp);
				else ref_settxlevel(v34obj, &mp);
			}
			++calls;
			if (side == 0) {
				if (graph_diff_live(ours, 0) != 0)
					++active[method != 0];
				graph_save_ours();
			}
		}
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		if (graph_diff_live(ours, 0) != 0)
			report_first_diff("TX component raw graph", ours, 0);
		diff_eq_int("TX component raw graph", graph_diff_live(ours, 0), 0, tag);
		diff_eq_int("TX transcript complete ours", dsplib_debug_capture_complete(0), 1, tag);
		diff_eq_int("TX transcript complete blob", dsplib_debug_capture_complete(1), 1, tag);
		diff_eq_int("TX callback boundaries", dsplib_debug_capture_boundaries_equal(), 1, tag);
		diff_eq_int("TX transcript bytes", strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)) == 0, 1, tag);
		/* txinit's echo cleanup also emits diagnostics at level two. */
		diff_eq_int("TX diagnostics active", dsplib_debug_capture_lines(1) > 0, level > 1, tag);
		++cases;
	}
	diff_eq_int("TX constructed cases", cases, 108, 0);
	diff_eq_int("TX constructed calls", calls, 216, 0);
	diff_eq_int("power cases change constructed output", active[1] > 0, 1, 0);
	printf("    TX component: %d cases, %d calls; active reset %d, power %d; all graph bytes raw\n",
	       cases, calls, active[0], active[1]);
	graph_restore();
	return diff_end();
}

/* Training-point component histories. Native initialization sets the carrier,
 * scale, queues and polynomial; both encoders then run bounded prefixes. The
 * Two additional public modulator setups exercise 2400/3429-baud QAM alongside
 * the modem initializer's 600-baud setup, without planting internal state. The
 * observed queue bound is checked after every call. This does not establish
 * that these prefixes are emitted by the protocol state machine. */
static int
run_tx_point_history(void)
{
	int history, profile, quad, debug, length, side, i;
	int cases = 0, calls = 0, active = 0;

	diff_begin("V34 TX points: initialized scrambler and modulator histories");
	for (history = 0; history < 2; ++history)
	for (profile = 0; profile < 3; ++profile)
	for (quad = 0; quad < 2; ++quad)
	for (debug = 0; debug < 2; ++debug)
	for (length = 1; length <= 8; ++length) {
		long tag = history * 10000 + profile * 1000 + quad * 100 + debug * 10 + length;
		for (side = 0; side < 2; ++side) {
			graph_restore();
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;
			ref_VPcmV34Create(v34obj, history, 0, runtime, 0);
			ref_v34modeminit(v34obj);
			if (profile != 0)
				ref_V34SetupModulator(
					(struct v34_modulator *)(v34obj + 0x1450),
					(short)(profile == 1 ? 2400 : 3429),
					(short)(profile == 1 ? 1600 : 1800), 0, 0, 1);
			diff_eq_int("TX point primed queue", hangup_int(0x221c), 32, tag);
			if (side == 0) {
				graph_save_ours();
				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();
			}
			dsplibs_debug_level = ref_dsplibs_debug_level = debug * 2;
			for (i = 0; i < length; ++i) {
				short bits = (short)((i * 5 + length) & (quad ? 15 : 3));
				if (quad) {
					if (side == 0) txmitquadbit(v34obj, bits);
					else ref_txmitquadbit(v34obj, bits);
				} else {
					if (side == 0) txmitdibit(v34obj, bits);
					else ref_txmitdibit(v34obj, bits);
				}
				++calls;
				diff_eq_int("TX point queue stays within capacity",
					    hangup_int(0x221c) >= 32 && hangup_int(0x221c) <= 230, 1, tag);
			}
			if (side == 0) {
				if (graph_diff_live(ours, 0) != 0) ++active;
				graph_save_ours();
			}
		}
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		if (graph_diff_live(ours, 0) != 0)
			report_first_diff("TX point raw graph", ours, 0);
		diff_eq_int("TX point raw graph", graph_diff_live(ours, 0), 0, tag);
		diff_eq_int("TX point transcript complete ours", dsplib_debug_capture_complete(0), 1, tag);
		diff_eq_int("TX point transcript complete blob", dsplib_debug_capture_complete(1), 1, tag);
		diff_eq_int("TX point callback boundaries", dsplib_debug_capture_boundaries_equal(), 1, tag);
		diff_eq_int("TX point transcript bytes", strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)) == 0, 1, tag);
		++cases;
	}
	diff_eq_int("TX point prefix cases", cases, 192, 0);
	diff_eq_int("TX point encoded calls", calls, 1728, 0);
	diff_eq_int("TX point prefixes change output", active, cases, 0);
	printf("    TX point component: %d prefixes, %d calls, %d active; queue bounded; all graph bytes raw\n",
	       cases, calls, active);
	graph_restore();
	return diff_end();
}

/* Direct method boundary on constructed owners, before negotiated PCM state.
 * preinitdigital installs the same four shell aliases as HangUp. Check their
 * referents, and keep every other learned exclusion bytewise visible. */
static int
run_setv90_history(void)
{
	int history, rrn, constellation, debug, side;
	int cases = 0, calls = 0, active = 0;

	diff_begin("SetV90RateReneg: constructed component histories");
	for (history = 0; history < 3; ++history)
	for (rrn = 0; rrn < 2; ++rrn)
	for (constellation = 0; constellation < 2; ++constellation)
	for (debug = 0; debug < 2; ++debug) {
		long tag = history * 100 + rrn * 10 + constellation * 2 + debug;
		for (side = 0; side < 2; ++side) {
			graph_restore();
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;
			if (history != 0)
				ref_VPcmV34Create(v34obj, history - 1, 0, runtime, 0);
			diff_eq_int("SetV90 constructed status", hangup_int(0), 0, tag);
			if (side == 0) {
				graph_save_ours();
				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();
			}
			dsplibs_debug_level = ref_dsplibs_debug_level = debug * 2;
			if (side == 0)
				VPcmV34SetV90RateReneg(v34obj, (short)rrn, (unsigned char)constellation);
			else
				ref_VPcmV34SetV90RateReneg(v34obj, (short)rrn, (unsigned char)constellation);
			++calls;
			if (side == 0) {
				if (graph_diff_live(ours, 1) != 0) ++active;
				graph_save_ours();
			}
		}
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		if (graph_diff_live(ours, 1) != 0)
			report_first_diff("SetV90 constructed graph", ours, 1);
		diff_eq_int("SetV90 constructed graph", graph_diff_live(ours, 1), 0, tag);
		diff_eq_int("SetV90 four shell aliases", check_alias_words(1), 0, tag);
		diff_eq_int("SetV90 other aliases raw", hangup_unchanged_alias_diff(), 0, tag);
		diff_eq_int("SetV90 transcript complete ours", dsplib_debug_capture_complete(0), 1, tag);
		diff_eq_int("SetV90 transcript complete blob", dsplib_debug_capture_complete(1), 1, tag);
		diff_eq_int("SetV90 callback boundaries", dsplib_debug_capture_boundaries_equal(), 1, tag);
		diff_eq_int("SetV90 transcript bytes", strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)) == 0, 1, tag);
		diff_eq_int("SetV90 diagnostics active", dsplib_debug_capture_lines(1) > 0, debug != 0, tag);
		++cases;
	}
	diff_eq_int("SetV90 constructed cases", cases, 24, 0);
	diff_eq_int("SetV90 constructed calls", calls, 48, 0);
	diff_eq_int("SetV90 changes constructed output", active, cases, 0);
	printf("    SetV90 component: %d cases, %d calls, %d active; four shell aliases checked\n",
	       cases, calls, active);
	graph_restore();
	return diff_end();
}

/* Indication component methods on a constructed graph. The byte arguments
 * select their documented local arms; no received-protocol history is claimed.
 * These methods install no static aliases, so all graph bytes remain visible. */
static int
run_indication_history(void)
{
	int history, method, debug, side;
	int cases = 0, calls = 0, active = 0;

	diff_begin("V34 indications: constructed component histories");
	for (history = 0; history < 3; ++history)
	for (method = 0; method < 7; ++method)
	for (debug = 0; debug < 2; ++debug) {
		long tag = history * 100 + method * 10 + debug;
		for (side = 0; side < 2; ++side) {
			graph_restore();
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;
			if (history != 0)
				ref_VPcmV34Create(v34obj, history - 1, 0, runtime, 0);
			diff_eq_int("indication constructed status", hangup_int(0), 0, tag);
			if (side == 0) {
				graph_save_ours();
				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();
			}
			dsplibs_debug_level = ref_dsplibs_debug_level = debug * 2;
			if (method < 4) {
				if (side == 0)
					V34XF_IndicateJdReceived(v34obj, (unsigned char)(method & 1), (unsigned char)(method >> 1));
				else
					ref_V34XF_IndicateJdReceived(v34obj, (unsigned char)(method & 1), (unsigned char)(method >> 1));
			} else if (method < 6) {
				if (side == 0) V34XF_IndicateDilReceived(v34obj, (unsigned char)(method - 4));
				else ref_V34XF_IndicateDilReceived(v34obj, (unsigned char)(method - 4));
			} else {
				if (side == 0) V34XF_IndicateK56FlexRateDetermined(v34obj);
				else ref_V34XF_IndicateK56FlexRateDetermined(v34obj);
			}
			++calls;
			if (side == 0) {
				if (graph_diff_live(ours, 0) != 0) ++active;
				graph_save_ours();
			}
		}
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		if (graph_diff_live(ours, 0) != 0)
			report_first_diff("indication raw graph", ours, 0);
		diff_eq_int("indication raw graph", graph_diff_live(ours, 0), 0, tag);
		diff_eq_int("indication transcript complete ours", dsplib_debug_capture_complete(0), 1, tag);
		diff_eq_int("indication transcript complete blob", dsplib_debug_capture_complete(1), 1, tag);
		diff_eq_int("indication callback boundaries", dsplib_debug_capture_boundaries_equal(), 1, tag);
		diff_eq_int("indication transcript bytes", strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)) == 0, 1, tag);
		diff_eq_int("indication diagnostics active", dsplib_debug_capture_lines(1) > 0, debug != 0, tag);
		++cases;
	}
	diff_eq_int("indication component cases", cases, 42, 0);
	diff_eq_int("indication component calls", calls, 84, 0);
	diff_eq_int("indication changes constructed output", active, cases, 0);
	printf("    indication component: %d cases, %d calls, %d active; all graph bytes raw\n", cases, calls, active);
	graph_restore();
	return diff_end();
}

/* Constructed component entry, with bounded prefixes before queue exhaustion.
 * Native Create -> modeminit -> public SetV90 establishes states 11/15 and
 * the chosen constellation. Six calls never leave that initial state:
 * this is useful processing coverage, not a negotiated rate-renegotiation
 * history or coverage of states 12..14/16..20. No protocol state is planted.
 * The setter runs AFTER modeminit, which otherwise resets its publications.
 * Both target implementations inherit the SAME native static aliases, so the
 * complete heap, including all previously learned exclusions, is compared raw.
 */
static int
run_rate_prefix_history(void)
{
    int history, rrn, constellation, profile, debug, length, side, i;
    int cases = 0, calls = 0, active = 0;
    diff_begin("V90 rate transmitters: setter-created bounded component prefixes");
    for (history = 0; history < 2; ++history)
    for (rrn = 0; rrn < 2; ++rrn)
    for (constellation = 0; constellation < 2; ++constellation)
    for (profile = 0; profile < 3; ++profile)
    for (debug = 0; debug < 2; ++debug)
    for (length = 1; length <= 6; ++length) {
        long tag = history*100000 + rrn*10000 + constellation*1000 + profile*100 + debug*10 + length;
        for (side = 0; side < 2; ++side) {
            graph_restore();
            dsplibs_debug_level = ref_dsplibs_debug_level = 0;
            ref_VPcmV34Create(v34obj, history, 0, runtime, 0);
            ref_v34modeminit(v34obj);
            if (profile != 0)
                ref_V34SetupModulator((struct v34_modulator *)(v34obj + 0x1450),
                    (short)(profile == 1 ? 2400 : 3429),
                    (short)(profile == 1 ? 1600 : 1800), 0, 0, 1);
            ref_VPcmV34SetV90RateReneg(v34obj, (short)rrn, (unsigned char)constellation);
            diff_eq_int("rate prefix native entry state", hangup_int(0x24c), rrn ? 15 : 11, tag);
            diff_eq_int("rate prefix native entry counter", hangup_short(0x25c0), 0, tag);
            if (side == 0) {
                graph_save_ours();
                dsplib_debug_capture_on = 1;
                dsplib_debug_capture_reset();
            }
            dsplibs_debug_level = ref_dsplibs_debug_level = debug*2;
            for (i = 0; i < length; ++i) {
                int result;
                if (rrn)
                    result = side ? ref_v90RateRenegSilence(v34obj) : v90RateRenegSilence(v34obj);
                else
                    result = side ? ref_v90RateReneg(v34obj) : v90RateReneg(v34obj);
                ++calls;
                diff_eq_int("rate prefix return", result, 0, tag);
                diff_eq_int("rate prefix bounded queue", hangup_short(0x221c) >= 32 && hangup_short(0x221c) <= 230, 1, tag);
                diff_eq_int("rate prefix published counter", hangup_short(0x25c0), (i+1)*2, tag);
                diff_eq_int("rate prefix stays in constructed entry", hangup_int(0x24c), rrn ? 15 : 11, tag);
            }
            if (side == 0) {
                if (graph_diff_live(ours, 0) != 0) ++active;
                graph_save_ours();
            }
        }
        dsplib_debug_capture_on = 0;
        dsplibs_debug_level = ref_dsplibs_debug_level = 0;
        if (graph_diff_live(ours, 0) != 0)
            report_first_diff("rate prefix raw graph", ours, 0);
        diff_eq_int("rate prefix raw graph", graph_diff_live(ours, 0), 0, tag);
        diff_eq_int("rate prefix transcript complete ours", dsplib_debug_capture_complete(0), 1, tag);
        diff_eq_int("rate prefix transcript complete blob", dsplib_debug_capture_complete(1), 1, tag);
        diff_eq_int("rate prefix callback boundaries", dsplib_debug_capture_boundaries_equal(), 1, tag);
        diff_eq_int("rate prefix transcript bytes", strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)) == 0, 1, tag);
        ++cases;
    }
    diff_eq_int("rate prefix cases", cases, 288, 0);
    diff_eq_int("rate prefix target calls", calls, 2016, 0);
    diff_eq_int("rate prefixes change constructed output", active, cases, 0);
    printf("    rate component: %d prefixes, %d target calls, %d active; states11/15 only; all graph bytes raw\n", cases, calls, active);
    graph_restore();
    return diff_end();
}

/* Native constructors and public indications supply these method inputs.
 * Repetition deliberately crosses 10 -> 14 and 18 -> 20: the indication is
 * not idempotent on every output. This is a component callback boundary;
 * it does not establish that a modem received TRN2d over a channel.
 */
static int
run_trn2d_history(void)
{
    static const int entry[] = { 0, 3, 6, 11, 15 };
    static const int outputs[5][3] = {
        { 10, 14, 14 }, { 10, 14, 14 }, { 10, 14, 14 },
        { 14, 14, 14 }, { 18, 20, 20 }
    };
    int role, method, debug, side, step, region;
    int cases = 0, calls = 0, active = 0, regions = 0;
    diff_begin("Trn2d indication: constructed callback component histories");
    for (role = 0; role < 2; ++role)
    for (method = 0; method < 5; ++method)
    for (debug = 0; debug < 2; ++debug) {
        long tag = role*100 + method*10 + debug;
        for (side = 0; side < 2; ++side) {
            graph_restore();
            dsplibs_debug_level = ref_dsplibs_debug_level = 0;
            ref_VPcmV34Create(v34obj, role, 0, runtime, 0);
            ref_v34modeminit(v34obj);
            if (method == 1) ref_V34XF_IndicateJdReceived(v34obj, 0, 0);
            if (method == 2) ref_V34XF_IndicateDilReceived(v34obj, 1);
            if (method >= 3) ref_VPcmV34SetV90RateReneg(v34obj, (short)(method-3), 0);
            diff_eq_int("Trn2d native entry", hangup_int(0x24c), entry[method], tag);
            if (side == 0) {
                graph_save_ours();
                dsplib_debug_capture_on = 1;
                dsplib_debug_capture_reset();
            }
            dsplibs_debug_level = ref_dsplibs_debug_level = debug*2;
            for (step = 0; step < 3; ++step) {
                if (side) ref_V34XF_IndicateTrn2dReceived(v34obj);
                else V34XF_IndicateTrn2dReceived(v34obj);
                ++calls;
                diff_eq_int("Trn2d published state", hangup_int(0x24c), outputs[method][step], tag*10+step);
            }
            if (side == 0) {
                if (graph_diff_live(ours, 0) != 0) ++active;
                graph_save_ours();
            }
        }
        dsplib_debug_capture_on = 0;
        dsplibs_debug_level = ref_dsplibs_debug_level = 0;
        for (region = 0; region < nreg; ++region) {
            diff_eq_obj("Trn2d raw live region", unsigned char[regsz[region]], ours[region], reg[region], tag*nreg+region);
            ++regions;
        }
        diff_eq_int("Trn2d transcript complete ours", dsplib_debug_capture_complete(0), 1, tag);
        diff_eq_int("Trn2d transcript complete blob", dsplib_debug_capture_complete(1), 1, tag);
        diff_eq_int("Trn2d callback boundaries", dsplib_debug_capture_boundaries_equal(), 1, tag);
        diff_eq_int("Trn2d transcript bytes", strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)) == 0, 1, tag);
        diff_eq_int("Trn2d diagnostic lines", dsplib_debug_capture_lines(1), debug ? 3 : 0, tag);
        ++cases;
    }
    diff_eq_int("Trn2d case denominator", cases, 20, 0);
    diff_eq_int("Trn2d target call denominator", calls, 120, 0);
    diff_eq_int("Trn2d active histories", active, cases, 0);
    diff_eq_int("Trn2d compared region denominator", regions, cases*nreg, 0);
    printf("    Trn2d component: %d histories, %d target calls, %d active; %d raw region comparisons\n", cases, calls, active, regions);
    graph_restore();
    return diff_end();
}

/* Native K56 session construction -> modeminit -> optional public modulator
 * setup -> six Ja calls. No receive flags/state or queue pointers are planted.
 * Native Ja/MP providers return zero and write nothing, so this history cannot
 * enter their completion tails. The queue is deliberately bounded before
 * capacity; this does not establish a negotiated K56 handshake.
 */
static int
run_k56_prefix_history(void)
{
    int role, session, profile, debug, side, step, region;
    int cases = 0, calls = 0, active = 0, regions = 0;
    diff_begin("K56 Phase34: constructed Ja component prefixes");
    for (role = 0; role < 2; ++role)
    for (session = 3; session <= 4; ++session)
    for (profile = 0; profile < 3; ++profile)
    for (debug = 0; debug < 2; ++debug) {
        long tag = role*1000 + session*100 + profile*10 + debug;
        for (side = 0; side < 2; ++side) {
            graph_restore();
            dsplibs_debug_level = ref_dsplibs_debug_level = 0;
            ref_VPcmV34Create(v34obj, role, 0, runtime, session);
            ref_v34modeminit(v34obj);
            if (profile != 0)
                ref_V34SetupModulator((struct v34_modulator *)(v34obj + 0x1450),
                    (short)(profile == 1 ? 2400 : 3429),
                    (short)(profile == 1 ? 1600 : 1800), 0, 0, 1);
            diff_eq_int("K56 constructed receiver", hangup_int(0x250), 1, tag);
            diff_eq_int("K56 constructed Ja guard", hangup_short(0x386) & V34_RX_FLAG_DATA, 0, tag);
            if (side == 0) {
                graph_save_ours();
                dsplib_debug_capture_on = 1;
                dsplib_debug_capture_reset();
            }
            dsplibs_debug_level = ref_dsplibs_debug_level = debug*2;
            for (step = 0; step < 6; ++step) {
                int queued = hangup_short(0x221c);
                int result = side ? ref_k56FlexPhase34(v34obj) : k56FlexPhase34(v34obj);
                ++calls;
                diff_eq_int("K56 Ja return", result, 0, tag*10+step);
                diff_eq_int("K56 Ja produced samples", hangup_short(0x221c) > queued, 1, tag*10+step);
                diff_eq_int("K56 bounded queue", hangup_short(0x221c) > 0 && hangup_short(0x221c) <= 230, 1, tag*10+step);
                diff_eq_int("K56 no Ja completion state", hangup_int(0x250), 1, tag*10+step);
                diff_eq_int("K56 no Ja completion guard", hangup_short(0x386) & V34_RX_FLAG_DATA, 0, tag*10+step);
            }
            if (side == 0) {
                if (graph_diff_live(ours, 0) != 0) ++active;
                graph_save_ours();
            }
        }
        dsplib_debug_capture_on = 0;
        dsplibs_debug_level = ref_dsplibs_debug_level = 0;
        for (region = 0; region < nreg; ++region) {
            diff_eq_obj("K56 raw live region", unsigned char[regsz[region]], ours[region], reg[region], tag*nreg+region);
            ++regions;
        }
        diff_eq_int("K56 transcript complete ours", dsplib_debug_capture_complete(0), 1, tag);
        diff_eq_int("K56 transcript complete blob", dsplib_debug_capture_complete(1), 1, tag);
        diff_eq_int("K56 callback boundaries", dsplib_debug_capture_boundaries_equal(), 1, tag);
        diff_eq_int("K56 transcript bytes", strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)) == 0, 1, tag);
        ++cases;
    }
    diff_eq_int("K56 prefix case denominator", cases, 24, 0);
    diff_eq_int("K56 prefix call denominator", calls, 288, 0);
    diff_eq_int("K56 active prefixes", active, cases, 0);
    diff_eq_int("K56 compared region denominator", regions, cases*nreg, 0);
    printf("    K56 component: %d Ja prefixes, %d target calls, %d active; %d raw region comparisons\n", cases, calls, active, regions);
    graph_restore();
    return diff_end();
}

/* MP packet component boundary. The caller supplies the public message fields
 * and grouping parameter of the already-owned V90MP, not its detector state.
 * Native infoToBits builds a complete type-1 packet; native reset/bitsToInfo
 * receives those bits from its initial detector state, and only an accepted
 * MP/MPnot is copied through native copyMpInfoForInterface. Both helper calls
 * then see that same owned, decoded session. Group36 is a component input;
 * neither this packet recipe nor the setter establishes negotiated V90 PCM.
 * Type0 is excluded because the native encoder overwrites its short CRC.
 * No receiver/progress/provider state or internal pointer is planted.
 */
struct fixture_mp_payload { char flags[6]; short words[7]; };
static int
run_received_mp_history(void)
{
    int history, packet, debug, side, i, cases = 0, calls = 0, active = 0;
    diff_begin("getMPrecvdBits: native encoded and accepted MP component packets");
    for (history = 0; history < 2; ++history)
    for (packet = 0; packet < 4; ++packet)
    for (debug = 0; debug < 2; ++debug) {
        long tag = history*100 + packet*10 + debug;
        for (side = 0; side < 2; ++side) {
            unsigned char bits[230];
            unsigned char *session, *mp;
            unsigned int group = 36;
            struct fixture_mp_payload payload;
            int accepted = 0;
            graph_restore();
            dsplibs_debug_level = ref_dsplibs_debug_level = 0;
            ref_VPcmV34Create(v34obj, history, 0, runtime, 0);
            ref_v34modeminit(v34obj);
            ref_VPcmV34SetV90RateReneg(v34obj, 0, (unsigned char)(packet & 1));
            session = *(unsigned char **)(v34obj + 0x3548);
            mp = session + 0x2428;
            payload.flags[0] = 1;
            payload.flags[1] = (char)(packet == 3 ? 14 : packet*3+1);
            payload.flags[2] = (char)packet;
            payload.flags[3] = (char)(packet & 1);
            payload.flags[4] = (char)((packet >> 1) & 1);
            payload.flags[5] = (char)(packet & 1);
            payload.words[0] = (short)(0x2001 | (1 << (packet+1)));
            for (i = 1; i < 7; ++i)
                payload.words[i] = (short)((i & 1 ? 1 : -1)*(packet+1)*i*23);
            memcpy(mp, &payload, sizeof payload);
            memcpy(mp + 0x114, &group, sizeof group);
            ref_mp_pack(mp);
            diff_eq_int("MP native encoded group length", mp[0x118], 216, tag);
            memcpy(bits, mp + 0x1c, sizeof bits);
            ref_mp_reset(mp);
            for (i = 0; i < 216; ++i) {
                int result = ref_mp_receive(mp, bits[i]);
                if (result == 1 || result == 2) {
                    ++accepted;
                    diff_eq_int("MP detector completion at packet end", i, 215, tag);
                    diff_eq_int("MP detector packet identity", result, packet & 1 ? 2 : 1, tag);
                }
            }
            diff_eq_int("MP packet accepted once", accepted, 1, tag);
            diff_eq_obj("MP native decoded supplied payload", struct fixture_mp_payload,
                        mp, &payload, tag);
            ref_copy_mp(session);
            if (side == 0) {
                graph_save_ours();
                dsplib_debug_capture_on = 1;
                dsplib_debug_capture_reset();
            }
            dsplibs_debug_level = ref_dsplibs_debug_level = debug*2;
            if (side == 0) fixture_get_mp(v34obj);
            else ref_fixture_get_mp(v34obj);
            ++calls;
            if (side == 0) {
                if (graph_diff_live(ours, 0) != 0) ++active;
                graph_save_ours();
            }
        }
        dsplib_debug_capture_on = 0;
        dsplibs_debug_level = ref_dsplibs_debug_level = 0;
        if (graph_diff_live(ours, 0) != 0)
            report_first_diff("getMP received packet raw graph", ours, 0);
        diff_eq_int("getMP received packet raw graph", graph_diff_live(ours, 0), 0, tag);
        diff_eq_int("getMP transcript complete ours", dsplib_debug_capture_complete(0), 1, tag);
        diff_eq_int("getMP transcript complete blob", dsplib_debug_capture_complete(1), 1, tag);
        diff_eq_int("getMP callback boundaries", dsplib_debug_capture_boundaries_equal(), 1, tag);
        diff_eq_int("getMP transcript bytes", strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)) == 0, 1, tag);
        diff_eq_int("getMP diagnostics active", dsplib_debug_capture_lines(1) > 0, debug != 0, tag);
        ++cases;
    }
    diff_eq_int("getMP received packet cases", cases, 16, 0);
    diff_eq_int("getMP helper calls", calls, 32, 0);
    diff_eq_int("getMP packets change constructed output", active, cases, 0);
    printf("    getMP component: %d packets, %d helper calls, %d active; native pack/reset/receive/copy; all graph bytes raw\n", cases, calls, active);
    graph_restore();
    return diff_end();
}

/* Native Create and modeminit establish the real session/params graph;
 * session types 0/1/3/4 establish disabled/V90/K56 receive selectors. The
 * optional public SetV90 indication supplies a later, distinct V90 selector.
 * The caller-owned runtime is configured through its declared 32-bit fields
 * at the getter boundary. The final two rows probe low-word consumption of
 * that transport, not values produced by dp_runtime_create (which sets 6).
 * No private protocol/ISP flag is planted. The analog graph has info0Layout=1
 * and native initSession has sensitivity=0; the other gates remain covered by
 * t_v34pcmif's synthetic probes. These are component histories, not negotiated
 * modem calls. The getter installs no aliases: compare all live bytes raw.
 */
static int
run_min_power_history(void)
{
    static const struct {
        int session, role, later, want, codec, expected;
    } input[] = {
        { 0, 0, 0, -1, 4, 0 },
        { 1, 0, 0, -11, 4, -10 },
        { 1, 1, 0, -10, 5, -10 },
        { 1, 0, 0, -9, 4, -9 },
        { 1, 1, 0, -1, 5, -1 },
        { 1, 0, 0, 0, 4, 0 },
        { 1, 1, 0, 1, 5, 1 },
        { 1, 0, 0, 6, 4, 6 },
        { 1, 1, 0, 7, 5, 7 },
        { 1, 0, 0, 8, 4, 7 },
        { 3, 0, 0, -11, 4, -10 },
        { 3, 1, 0, 8, 5, 7 },
        { 4, 0, 0, -1, 5, -1 },
        { 0, 1, 0, 8, 5, 0 },
        { 1, 0, 1, 0x10000, 4, 0 },
        { 1, 1, 1, 0x1ffff, 5, -1 }
    };
    unsigned item;
    int debug, side, region, cases = 0, calls = 0, active = 0, regions = 0;
    int positive = 0, nonpositive = 0, clamped = 0, disabled = 0;
    diff_begin("Minimum power: constructed selectors and declared configuration boundary");
    for (item = 0; item < sizeof(input)/sizeof(input[0]); ++item)
    for (debug = 0; debug < 2; ++debug) {
        long tag = item*10 + debug;
        short result[2];
        for (side = 0; side < 2; ++side) {
            unsigned char *session, *params;
            int flag;
            graph_restore();
            dsplibs_debug_level = ref_dsplibs_debug_level = 0;
            ref_VPcmV34Create(v34obj, input[item].role, 0, runtime, input[item].session);
            ref_v34modeminit(v34obj);
            if (input[item].later)
                ref_VPcmV34SetV90RateReneg(v34obj, 0, 0);
            session = *(unsigned char **)(v34obj + 0x3548);
            params = *(unsigned char **)(session + 0x610c);
            diff_eq_int("minimum native session layout", *(int *)(session + 0x6120), 1, tag);
            diff_eq_int("minimum native sensitivity", *(int *)(params + 0x4f8), 0, tag);
            diff_eq_int("minimum native V90 selector", hangup_int(0x24c),
                        input[item].later ? 11 : input[item].session == 1 ? 1 : 0, tag);
            diff_eq_int("minimum native K56 selector", hangup_int(0x250),
                        input[item].session >= 3 ? 1 : 0, tag);
            runtime->unnamed_0044 = input[item].want;
            runtime->codecType = input[item].codec;
            if (side == 0) {
                graph_save_ours();
                dsplib_debug_capture_on = 1;
                dsplib_debug_capture_reset();
            }
            dsplibs_debug_level = ref_dsplibs_debug_level = debug*2;
            result[side] = side ? ref_GetVPcmMinimalTxPowerReduction(v34obj)
                                : GetVPcmMinimalTxPowerReduction(v34obj);
            ++calls;
            diff_eq_int("minimum expected reduction", result[side], input[item].expected, tag);
            memcpy(&flag, params + 0x4f4, sizeof flag);
            diff_eq_int("minimum high-level flag", flag, input[item].expected <= 0, tag);
            diff_eq_int("minimum echo start", hangup_int(0x3554), 2000, tag);
            diff_eq_int("minimum echo decay", hangup_int(0x3558),
                        input[item].expected > 0 ? 0x7fdf : 0x7fcb, tag);
            diff_eq_int("minimum echo beta", hangup_int(0x355c),
                        input[item].expected > 0 ? 2 : input[item].codec == 4 ? 4 : 6, tag);
            if (side == 0) {
                if (graph_diff_live(ours, 0) != 0) ++active;
                graph_save_ours();
            }
        }
        dsplib_debug_capture_on = 0;
        dsplibs_debug_level = ref_dsplibs_debug_level = 0;
        diff_eq_int("minimum paired return", result[0], result[1], tag);
        for (region = 0; region < nreg; ++region) {
            diff_eq_obj("minimum raw live region", unsigned char[regsz[region]],
                        ours[region], reg[region], tag*nreg + region);
            ++regions;
        }
        diff_eq_int("minimum transcript complete ours", dsplib_debug_capture_complete(0), 1, tag);
        diff_eq_int("minimum transcript complete blob", dsplib_debug_capture_complete(1), 1, tag);
        diff_eq_int("minimum callback boundaries", dsplib_debug_capture_boundaries_equal(), 1, tag);
        diff_eq_int("minimum transcript bytes", strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)) == 0, 1, tag);
        diff_eq_int("minimum diagnostic lines", dsplib_debug_capture_lines(1), debug ? 2 : 0, tag);
        if (input[item].expected > 0) ++positive; else ++nonpositive;
        if (input[item].want == -11 || input[item].want == 8) ++clamped;
        if (input[item].session == 0) ++disabled;
        ++cases;
    }
    diff_eq_int("minimum case denominator", cases, 32, 0);
    diff_eq_int("minimum target call denominator", calls, 64, 0);
    diff_eq_int("minimum region denominator", regions, cases*nreg, 0);
    diff_eq_int("minimum changes observed", active > 0, 1, 0);
    diff_eq_int("minimum positive outputs", positive, 10, 0);
    diff_eq_int("minimum nonpositive outputs", nonpositive, 22, 0);
    diff_eq_int("minimum clamp input cases", clamped, 10, 0);
    diff_eq_int("minimum disabled selector cases", disabled, 4, 0);
    printf("    Minimum power component: %d cases, %d calls, %d active; %d positive/%d nonpositive, %d clamp inputs, %d disabled; %d raw region comparisons\n",
           cases, calls, active, positive, nonpositive, clamped, disabled, regions);
    graph_restore();
    return diff_end();
}

int
main(void)
{
	struct dp_operations *ops;
	int rc = 0;

	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	ops = vpcm_ops();
	if (ops == 0 || ops->create == 0) {
		printf("FIXTURE: no V.PCM ops table\n");
		return 1;
	}
	if (!build(ops, 1)) {
		printf("FIXTURE: the blob's create failed\n");
		return 1;
	}

	graph_take();
	printf("    the blob built %d live region(s)\n", nreg);

	rc |= run_alias_words();
	rc |= run_virgin_vs_reinit();
	rc |= run_create();
	rc |= run_hangup_history();
	rc |= run_initmoh_history();
	rc |= run_retrain_history();
	rc |= run_retrain_helper_history();
	rc |= run_bulkdelay_history();
	rc |= run_tx_component_history();
	rc |= run_tx_point_history();
	rc |= run_setv90_history();
	rc |= run_indication_history();
	rc |= run_rate_prefix_history();
	rc |= run_trn2d_history();
	rc |= run_k56_prefix_history();
	rc |= run_received_mp_history();
	rc |= run_min_power_history();
	rc |= run_transcripts();
	rc |= run_made_to_fail();

	return rc;
}

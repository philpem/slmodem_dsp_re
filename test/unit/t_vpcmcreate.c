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
 * IT DOES NOT HAVE TO BE STOOD UP.  Findings 800-806 measured that the blob's
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
 * AND FINDING 803'S CONGRUENCE PROBLEM DOES NOT ARISE, because there is only
 * ONE graph.  Two constructions come back at 125 different addresses and every
 * pointer field then differs for a reason that is not a defect; this file
 * builds one and runs both sides on the SAME memory at the SAME addresses,
 * finding 805's three-pass shape applied to a constructor:
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
 * failure rather than a silence.  That is the one thing finding 806's
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
 * comparison is not two untouched images (finding 805's vacuous passes 0 and
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
 * that sentence predated finding 1119, which found the fifth by reading the
 * five sites that touch `0x50(%esp)` and the five slots `vpcm_create` pushes.
 *
 * FINDING 803'S CONGRUENCE PROBLEM DOES NOT ARISE and no HEAP pointer is
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
 * finding 802's measurement arriving from the other direction, where two
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
check_alias_words(void)
{
	int k, bad = 0;

	for (k = 0; k < NALIAS; k++) {
		const void *a, *b;
		size_t w = (size_t)ROOT_V34 + aliasword[k].off;

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
	 * ANTI-VACUITY.  Finding 805's passes 0 and 1 changed zero bytes and
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
		    " after the sweep (%ld bad)", check_alias_words(), 0, 0);
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
 * THE COMPARISON MADE TO FAIL.  Finding 805's pass 3 is the model: run ours,
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
	 * finding 803's fourteen caller-configured bytes instead of the
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
		    " bad)", check_alias_words(), 0, 0);
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
	rc |= run_transcripts();
	rc |= run_made_to_fail();

	return rc;
}

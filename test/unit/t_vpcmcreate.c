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
 * Every pointer field is identical by construction and nothing is excluded.
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
extern int ref_VPcmV34Create(void *obj, int side, int arg2, void *dpRuntime);

/*
 * ============================================================================
 * READ THIS BEFORE READING ANYTHING ELSE IN THIS FILE.
 *
 * `VPcmV34Create` IS NOT RECONSTRUCTED YET, so BOTH SIDES OF EVERY COMPARISON
 * BELOW ARE THE BLOB'S.  This file is therefore **NOT a differential test of
 * `VPcmV34Create`** and nothing in it says our reconstruction is right --
 * there is no reconstruction.  What it is, and what it is committed for, is
 * the FIXTURE: the three properties a differential test of a constructor needs
 * before its verdict means anything, asserted on code that is known good.
 *
 *     - the whole 126-region heap graph can be snapshotted and RESTORED, and
 *       the restore is checked rather than assumed;
 *     - the call CHANGES the graph, so an agreeing comparison is not two
 *       untouched images (finding 805's vacuous passes 0 and 1);
 *     - the comparison REPORTS a single flipped byte and goes quiet when it
 *       is put back.
 *
 * `t_v34conn.c` carries "the blob's V.34 constructor, as a fixture" for the
 * same reason and this is the same kind of thing.
 *
 * TO TURN IT INTO A DIFFERENTIAL TEST, delete the `#define` below and declare
 * `int VPcmV34Create(void *, int, int, void *);` instead.  That is the whole
 * change; every assertion here is already written against two sides.  Finding
 * 1118 is why the fixture exists and 1117 is what is left to write.
 * ============================================================================
 */
#define VPcmV34Create ref_VPcmV34Create

/* --- the configuration, all of it derived; docs/configuration.md ---------- */

#define CFG_MIN_RATE	300
#define CFG_MAX_RATE	56000
#define CFG_IODELAY	0
#define CFG_CODECTYPE	4
#define CFG_SRATE	9600		/* `cmp $0x2580,%esi`, and MODEM_RATE */
#define CFG_MAX_FRAG	48		/* `cmpl $0x30,..` / `jg`, MODEM_FRAG */

#define ROOT_V34	0x2c		/* `lea 0x2c(%ebx),%ebp` at 0x3add   */
#define V34_LEN		0xac4c		/* the memset at the top of the call  */

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
		if (snap[i] == 0 || ours[i] == 0) {
			printf("FIXTURE: out of memory snapshotting\n");
			exit(1);
		}
		memcpy(snap[i], reg[i], regsz[i]);
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

static long
graph_diff_live(unsigned char *const *a)
{
	long n = 0;
	int i;
	size_t j;

	for (i = 0; i < nreg; i++) {
		const unsigned char *p = (const unsigned char *)reg[i];

		for (j = 0; j < regsz[i]; j++)
			if (a[i][j] != p[j])
				n++;
	}
	return n;
}

/*
 * Where the first difference is, named as region index and offset, and whether
 * that region is the V.PCM root.  `diff_eq_obj` cannot be used across a
 * 125-region graph, so the report is built here.
 */
static void
report_first_diff(const char *what, unsigned char *const *a)
{
	int i;
	size_t j;

	for (i = 0; i < nreg; i++) {
		const unsigned char *p = (const unsigned char *)reg[i];

		for (j = 0; j < regsz[i]; j++)
			if (a[i][j] != p[j]) {
				printf("    %s: region %d (%p, %lu bytes)"
				       " offset %lu: ours %02x blob %02x%s\n",
				       what, i, reg[i], (unsigned long)regsz[i],
				       (unsigned long)j, a[i][j], p[j],
				       reg[i] == (void *)root
				       ? "   [the V.PCM root]" : "");
				return;
			}
	}
}

/* Every differing byte, coalesced into runs, for the measurement passes. */
static void
report_all_diffs(const char *what, unsigned char *const *a)
{
	int i;
	size_t j, run;

	for (i = 0; i < nreg; i++) {
		const unsigned char *p = (const unsigned char *)reg[i];

		for (j = 0; j < regsz[i]; j++) {
			if (a[i][j] == p[j])
				continue;
			run = j;
			while (j + 1 < regsz[i] && a[i][j + 1] != p[j + 1])
				j++;
			printf("      %s: region %d%s +0x%lx..+0x%lx"
			       " (%lu bytes)\n", what, i,
			       reg[i] == (void *)root ? " [root]" : "",
			       (unsigned long)run, (unsigned long)j,
			       (unsigned long)(j - run + 1));
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

/* --- the three passes ----------------------------------------------------- */

/*
 * One (side, arg2) case.  `side` is `vpcm_create`'s `sete` of `caller` and
 * `arg2` is its own fourth stack argument; both are swept, because the object
 * branches on the first and this file has no independent statement of what the
 * second is.
 */
static void
one_case(int side, int arg2, long tag, long *changedp, int *reportedp)
{
	int rc_ours, rc_blob;
	long n;

	graph_restore();
	diff_eq_int("the restore restored the graph (%ld)",
		    graph_diff_live(snap) == 0 ? 1 : 0, 1, tag);

	rc_ours = VPcmV34Create(v34obj, side, arg2, runtime);
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
		    graph_diff_live(snap) == 0 ? 1 : 0, 1, tag);

	rc_blob = ref_VPcmV34Create(v34obj, side, arg2, runtime);

	diff_eq_int("the return value (%ld)", rc_ours, rc_blob, tag);

	n = graph_diff_live(ours);
	if (n != 0) {
		report_first_diff("after VPcmV34Create", ours);
		*reportedp = 1;
	}
	diff_eq_int("the whole graph, byte for byte (%ld differing bytes)",
		    n == 0 ? 1 : 0, 1, n);
}

static int
run_create(void)
{
	static const int arg2_v[] = { 0, 1, 2, -1 };
	int side, ai;
	long tag = 0;
	long changed = 0;
	int reported = 0;

	diff_begin("the VPcmV34Create fixture: blob against blob, whole graph");

	for (side = 0; side < 2; side++)
		for (ai = 0; ai < 4; ai++)
			one_case(side, arg2_v[ai], tag++, &changed, &reported);

	diff_eq_int("some case changed the graph", changed > 0 ? 1 : 0, 1,
		    changed);
	diff_eq_int("and no case was reported as differing", reported, 0, 0);

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
	VPcmV34Create(v34obj, 1, 0, runtime);
	graph_save_ours();
	graph_restore();
	ref_VPcmV34Create(v34obj, 1, 0, runtime);

	diff_eq_int("undisturbed, the two agree",
		    graph_diff_live(ours) == 0 ? 1 : 0, 1, 0);

	/* One byte of the V.34 object, changed on the blob's side only. */
	p = (unsigned char *)v34obj + 0x260;
	*p = (unsigned char)(*p ^ 0xffu);
	n = graph_diff_live(ours);
	diff_eq_int("one flipped byte is reported (%ld)", n > 0 ? 1 : 0, 1, n);
	*p = (unsigned char)(*p ^ 0xffu);
	diff_eq_int("and putting it back silences it",
		    graph_diff_live(ours) == 0 ? 1 : 0, 1, 0);

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
	ref_VPcmV34Create(v34obj, 0, 0, runtime);
	n = graph_diff_live(snap);
	printf("    a second VPcmV34Create at the SAME side moves %ld byte(s)"
	       " of %d region(s)\n", n, nreg);
	report_all_diffs("same side", snap);

	graph_restore();
	ref_VPcmV34Create(v34obj, 1, 0, runtime);
	n = graph_diff_live(snap);
	printf("    and at the OTHER side, %ld byte(s)\n", n);
	report_all_diffs("other side", snap);
	diff_eq_int("the measurement ran", nreg > 0 ? 1 : 0, 1, nreg);

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

	rc |= run_virgin_vs_reinit();
	rc |= run_create();
	rc |= run_made_to_fail();

	return rc;
}

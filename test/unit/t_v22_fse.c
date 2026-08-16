/*
 * t_v22_fse.c -- differential test of the V.22 equaliser's init, free and
 * diagnostic stub.
 *
 * THE PROTOTYPE THIS IS DRIVEN WITH IS NOT `FSEv22_COFFS`, AND THAT IS THE
 * WHOLE POINT.  `V22_FSE_init` builds its working coefficients by REVERSING
 * the configuration's arrays -- `icoeff[48 - i] = icoff[i] >> 2` -- and the
 * object's own prototype is symmetric about its centre, so every entry of it
 * satisfies `coff[i] == coff[48 - i]` and a reconstruction that copied in
 * order would agree with the blob on every single tap.  That is finding 3052's
 * shape exactly: a plausible reading that agrees with the true one over every
 * realistic input.  So the driving arrays here are deliberately asymmetric,
 * deliberately different from each other, and deliberately full of negative
 * values that are not multiples of four, and `main()` refuses to report a pass
 * unless the counters below show that each of those actually separated
 * something:
 *
 *   reversal   entries where in-order and reversed disagree
 *   scaling    entries the `>> 2` changes
 *   arithmetic entries where an arithmetic and a logical `>> 2` disagree
 *   i_vs_q     entries where the I and Q arrays disagree, so an I/Q swap fails
 *   hist_tail  entries of `hist` above the coefficient loop's 49 that the
 *              second loop has to clear, marked beforehand so their clearing
 *              is visible
 *
 * `FSEv22_COFFS` and `FSEv22_CFG` are still compared against the blob's copies
 * entry by entry -- they are the deliverable -- they are just not what the
 * behaviour is measured with.
 *
 * The allocation SIZES are asserted, not only the count: seven buffers of four
 * distinct sizes, and a buffer that is too small is invisible until something
 * reads off the end of it.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v22_fse.h"

extern void ref_V22_FSE_init(struct v22_fse *state,
			     const struct v22_fse_cfg *cfg, int fresh);
extern void ref_V22_FSE_free(struct v22_fse *state);
extern int ref_V22_FSE_getdiag(struct v22_fse *state);

extern const short ref_FSEv22_COFFS[];
extern const struct v22_fse_cfg ref_FSEv22_CFG;

/*
 * A marker no coefficient and no scalar of this object can be confused with.
 * Anything init is supposed to write and does not shows up as a survivor.
 */
#define MARK 0x5ead

/* Set by the tests below; see the guards in main(). */
static int sep_reversal;
static int sep_scaling;
static int sep_arithmetic;
static int sep_i_vs_q;
static int sep_hist_tail;

/*
 * The driving prototype.  Asymmetric by construction, and the Q array is not
 * a copy of the I array: a mixed-radix walk over the index, so no two entries
 * repeat and no entry is a multiple of four.
 */
static short drive_i[V22_FSE_TAPS];
static short drive_q[V22_FSE_TAPS];
static struct v22_fse_cfg drive_cfg;

static void
make_drive(void)
{
	int i;

	for (i = 0; i < V22_FSE_TAPS; i++) {
		int v = (i * 2357 + 811) & 0x7fff;

		/* Half of them negative, and none of them 0 mod 4. */
		if (i & 1)
			v = -v;
		v |= 1;
		drive_i[i] = (short)v;
		drive_q[i] = (short)(-v + 3 * i + 1);
	}

	drive_cfg.icoff = drive_i;
	drive_cfg.qcoff = drive_q;
}

/*
 * Count what the driving arrays actually separate.  Done once, against the
 * arrays themselves, so a change to `make_drive` that quietly made them
 * symmetric would fail the guard in `main` rather than weaken the test.
 */
static void
count_separations(void)
{
	int i;

	for (i = 0; i < V22_FSE_TAPS; i++) {
		short direct = (short)(drive_i[i] >> 2);
		short reversed = (short)(drive_i[V22_FSE_TAPS - 1 - i] >> 2);
		unsigned short logical = (unsigned short)drive_i[i];

		if (direct != reversed)
			sep_reversal++;
		if (drive_i[i] != direct)
			sep_scaling++;
		if ((short)(logical >> 2) != direct)
			sep_arithmetic++;
		if (drive_i[i] != drive_q[i])
			sep_i_vs_q++;
	}
}

/*
 * The seven heap pointers are seven different addresses on the two sides and
 * always will be, so they are the sanctioned skip.  Blanking them in a copy
 * keeps diff_eq_obj's field-level report and its run coalescing, which an
 * open-coded byte loop would throw away.
 */
static void
blank_pointers(struct v22_fse *dst, const struct v22_fse *src)
{
	*dst = *src;
	dst->out_i = (short *)0;
	dst->out_q = (short *)0;
	dst->icoeff = (short *)0;
	dst->qcoeff = (short *)0;
	dst->hist = (short *)0;
	dst->r44 = (short *)0;
	dst->r48 = (short *)0;
}

static void
compare_buffers(const struct v22_fse *ours, const struct v22_fse *ref)
{
	int i;

	for (i = 0; i < V22_FSE_TAPS; i++) {
		diff_eq_int("icoeff[%ld]", ours->icoeff[i], ref->icoeff[i], i);
		diff_eq_int("qcoeff[%ld]", ours->qcoeff[i], ref->qcoeff[i], i);
	}
	for (i = 0; i < V22_FSE_HIST; i++)
		diff_eq_int("hist[%ld]", ours->hist[i], ref->hist[i], i);
	for (i = 0; i < V22_FSE_AUX; i++) {
		diff_eq_int("r44[%ld]", ours->r44[i], ref->r44[i], i);
		diff_eq_int("r48[%ld]", ours->r48[i], ref->r48[i], i);
	}
	for (i = 0; i < V22_FSE_OUT; i++) {
		diff_eq_int("out_i[%ld]", ours->out_i[i], ref->out_i[i], i);
		diff_eq_int("out_q[%ld]", ours->out_q[i], ref->out_q[i], i);
	}
}

static int
tables(void)
{
	int i;

	diff_begin("v22 fse tables");
	for (i = 0; i < V22_FSE_TAPS; i++)
		diff_eq_int("FSEv22_COFFS[%ld]", FSEv22_COFFS[i],
			    ref_FSEv22_COFFS[i], i);
	diff_eq_obj("FSEv22_CFG", struct v22_fse_cfg, &FSEv22_CFG,
		    &ref_FSEv22_CFG, 0);
	/*
	 * And the fact that makes the asymmetric driving arrays necessary,
	 * stated rather than assumed: the object's prototype is symmetric.
	 */
	for (i = 0; i < V22_FSE_TAPS; i++)
		diff_eq_int("FSEv22_COFFS is symmetric at %ld",
			    FSEv22_COFFS[i], FSEv22_COFFS[V22_FSE_TAPS - 1 - i],
			    i);
	return diff_end();
}

/*
 * init with `fresh` set: both sides allocate their own seven buffers.  The
 * harness fills every allocation with HARNESS_MALLOC_FILL, so even the four
 * buffers init deliberately never writes are comparable -- both sides see the
 * same pattern in them, and a reconstruction that cleared them would fail here
 * rather than pass silently.
 */
static int
run_init_fresh(const char *label)
{
	struct v22_fse a, b, ma, mb;
	int allocs_ref, allocs_ours;
	int rc, i;

	memset(&a, MARK & 0xff, sizeof a);
	memset(&b, MARK & 0xff, sizeof b);

	diff_begin(label);

	/*
	 * One reset, not one per side: resetting between them would drop the
	 * reference's blocks from the live set, and the frees at the end would
	 * then be counted as frees of pointers nobody handed out.
	 */
	harness_alloc_reset();
	ref_V22_FSE_init(&a, &drive_cfg, 1);
	allocs_ref = harness_alloc.allocs;

	V22_FSE_init(&b, &drive_cfg, 1);
	allocs_ours = harness_alloc.allocs - allocs_ref;

	diff_eq_int("allocations (%ld)", allocs_ours, allocs_ref, 0);
	diff_eq_int("allocations is seven (%ld)", allocs_ours, 7, 0);

	/* Sizes, not just counts.  Four distinct ones. */
	diff_eq_int("icoeff bytes (%ld)", (long)harness_alloc_reqsize(b.icoeff),
		    (long)(V22_FSE_TAPS * sizeof(short)), 0);
	diff_eq_int("qcoeff bytes (%ld)", (long)harness_alloc_reqsize(b.qcoeff),
		    (long)(V22_FSE_TAPS * sizeof(short)), 0);
	diff_eq_int("hist bytes (%ld)", (long)harness_alloc_reqsize(b.hist),
		    (long)(V22_FSE_HIST * sizeof(short)), 0);
	diff_eq_int("r44 bytes (%ld)", (long)harness_alloc_reqsize(b.r44),
		    (long)(V22_FSE_AUX * sizeof(short)), 0);
	diff_eq_int("r48 bytes (%ld)", (long)harness_alloc_reqsize(b.r48),
		    (long)(V22_FSE_AUX * sizeof(short)), 0);
	diff_eq_int("out_i bytes (%ld)", (long)harness_alloc_reqsize(b.out_i),
		    (long)(V22_FSE_OUT * sizeof(short)), 0);
	diff_eq_int("out_q bytes (%ld)", (long)harness_alloc_reqsize(b.out_q),
		    (long)(V22_FSE_OUT * sizeof(short)), 0);

	blank_pointers(&ma, &a);
	blank_pointers(&mb, &b);
	diff_eq_obj("init(fresh) state", struct v22_fse, &mb, &ma, 0);

	compare_buffers(&b, &a);

	/*
	 * The reading itself, spelled out against the driving arrays rather
	 * than only against the blob: reversed, and shifted down two.  If both
	 * sides were wrong in the same way the comparison above would still
	 * pass, and this would not.
	 */
	for (i = 0; i < V22_FSE_TAPS; i++) {
		diff_eq_int("icoeff[%ld] is icoff[48-i] >> 2", b.icoeff[i],
			    (short)(drive_i[V22_FSE_TAPS - 1 - i] >> 2), i);
		diff_eq_int("qcoeff[%ld] is qcoff[48-i] >> 2", b.qcoeff[i],
			    (short)(drive_q[V22_FSE_TAPS - 1 - i] >> 2), i);
	}

	/* The configuration pointers are kept, not copied through. */
	diff_eq_int("icoff kept (%ld)", b.icoff == drive_i, 1, 0);
	diff_eq_int("qcoff kept (%ld)", b.qcoff == drive_q, 1, 0);

	rc = diff_end();

	ref_V22_FSE_free(&a);
	V22_FSE_free(&b);
	return rc;
}

/*
 * init with `fresh` clear: the buffers are ours, pre-loaded with MARK, so
 * every word init writes and every word it leaves alone can be named.  This is
 * also what pins the flag's SENSE -- `FPM_FSE_init` reads the same argument
 * the other way round, and a reconstruction that copied its polarity would
 * allocate here and be caught by the "allocates nothing" check.
 */
static int
run_init_reuse(const char *label)
{
	static short ic_a[V22_FSE_TAPS], ic_b[V22_FSE_TAPS];
	static short qc_a[V22_FSE_TAPS], qc_b[V22_FSE_TAPS];
	static short hi_a[V22_FSE_HIST], hi_b[V22_FSE_HIST];
	static short x4_a[V22_FSE_AUX], x4_b[V22_FSE_AUX];
	static short x8_a[V22_FSE_AUX], x8_b[V22_FSE_AUX];
	static short oi_a[V22_FSE_OUT], oi_b[V22_FSE_OUT];
	static short oq_a[V22_FSE_OUT], oq_b[V22_FSE_OUT];
	struct v22_fse a, b, ma, mb;
	int rc, i;

	memset(&a, MARK & 0xff, sizeof a);
	memset(&b, MARK & 0xff, sizeof b);

	for (i = 0; i < V22_FSE_TAPS; i++)
		ic_a[i] = ic_b[i] = qc_a[i] = qc_b[i] = (short)MARK;
	for (i = 0; i < V22_FSE_HIST; i++)
		hi_a[i] = hi_b[i] = (short)MARK;
	for (i = 0; i < V22_FSE_AUX; i++)
		x4_a[i] = x4_b[i] = x8_a[i] = x8_b[i] = (short)MARK;
	for (i = 0; i < V22_FSE_OUT; i++)
		oi_a[i] = oi_b[i] = oq_a[i] = oq_b[i] = (short)MARK;

	a.icoeff = ic_a; a.qcoeff = qc_a; a.hist = hi_a;
	a.r44 = x4_a; a.r48 = x8_a; a.out_i = oi_a; a.out_q = oq_a;
	b.icoeff = ic_b; b.qcoeff = qc_b; b.hist = hi_b;
	b.r44 = x4_b; b.r48 = x8_b; b.out_i = oi_b; b.out_q = oq_b;

	diff_begin(label);

	harness_alloc_reset();
	ref_V22_FSE_init(&a, &drive_cfg, 0);
	V22_FSE_init(&b, &drive_cfg, 0);

	/* Nothing may be allocated on this path, by either side. */
	diff_eq_int("reuse allocates nothing (%ld)", harness_alloc.allocs, 0,
		    0);

	blank_pointers(&ma, &a);
	blank_pointers(&mb, &b);
	diff_eq_obj("init(reuse) state", struct v22_fse, &mb, &ma, 0);

	compare_buffers(&b, &a);

	/* Every coefficient must have been written. */
	for (i = 0; i < V22_FSE_TAPS; i++) {
		diff_eq_int("icoeff[%ld] was written",
			    ic_b[i] != (short)MARK, 1, i);
		diff_eq_int("qcoeff[%ld] was written",
			    qc_b[i] != (short)MARK, 1, i);
	}

	/*
	 * The whole history is cleared, all 98 of it, and the entries above 48
	 * are the evidence for the SECOND loop: the coefficient loop only
	 * reaches 0..48, so without it these would still hold MARK.
	 */
	for (i = 0; i < V22_FSE_HIST; i++)
		diff_eq_int("hist[%ld] cleared", hi_b[i], 0, i);
	for (i = V22_FSE_TAPS; i < V22_FSE_HIST; i++)
		sep_hist_tail++;

	/* And the four buffers init never writes must still hold MARK. */
	for (i = 0; i < V22_FSE_AUX; i++) {
		diff_eq_int("r44[%ld] untouched", x4_b[i], (short)MARK, i);
		diff_eq_int("r48[%ld] untouched", x8_b[i], (short)MARK, i);
	}
	for (i = 0; i < V22_FSE_OUT; i++) {
		diff_eq_int("out_i[%ld] untouched", oi_b[i], (short)MARK, i);
		diff_eq_int("out_q[%ld] untouched", oq_b[i], (short)MARK, i);
	}

	rc = diff_end();
	return rc;
}

/*
 * free releases the seven buffers and nothing else.  With the harness's
 * accounting that is a statement a test can actually make: seven frees, no
 * frees of anything the allocator never handed out, nothing left live, and the
 * object itself unchanged.
 */
static int
run_free(const char *label)
{
	struct v22_fse a, b, before_a, before_b;
	int rc;

	memset(&a, MARK & 0xff, sizeof a);
	memset(&b, MARK & 0xff, sizeof b);

	diff_begin(label);

	harness_alloc_reset();
	ref_V22_FSE_init(&a, &drive_cfg, 1);
	V22_FSE_init(&b, &drive_cfg, 1);
	diff_eq_int("fourteen buffers live (%ld)", harness_alloc.live, 14, 0);

	before_a = a;
	before_b = b;

	ref_V22_FSE_free(&a);
	diff_eq_int("ref freed seven (%ld)", harness_alloc.frees, 7, 0);
	V22_FSE_free(&b);
	diff_eq_int("frees total (%ld)", harness_alloc.frees, 14, 0);
	diff_eq_int("nothing left live (%ld)", harness_alloc.live, 0, 0);
	diff_eq_int("no stray free (%ld)", harness_alloc.bad_free, 0, 0);
	diff_eq_int("no null free (%ld)", harness_alloc.free_null, 0, 0);
	diff_eq_int("live set never overflowed (%ld)", harness_alloc.overflow,
		    0, 0);

	/*
	 * free leaves the pointers dangling on both sides, so each object is
	 * still byte-identical to what init left it -- including the seven
	 * addresses, which is why these comparisons are against each side's own
	 * snapshot rather than against each other.
	 */
	diff_eq_obj("ref object unchanged by free", struct v22_fse, &a,
		    &before_a, 0);
	diff_eq_obj("our object unchanged by free", struct v22_fse, &b,
		    &before_b, 0);

	rc = diff_end();
	return rc;
}

/*
 * The diagnostic stub.  Three bytes in the object, and what a test can say
 * about it is that it returns zero and touches nothing -- which is exactly the
 * reading, and separates it from every other constant and from any version
 * that writes through its argument.
 */
static int
run_getdiag(const char *label)
{
	struct v22_fse a, b, before_a, before_b;
	int rc;

	memset(&a, MARK & 0xff, sizeof a);
	memset(&b, MARK & 0xff, sizeof b);
	before_a = a;
	before_b = b;

	diff_begin(label);

	harness_alloc_reset();
	diff_eq_int("getdiag returns (%ld)", V22_FSE_getdiag(&b),
		    ref_V22_FSE_getdiag(&a), 0);
	diff_eq_int("getdiag returns zero (%ld)", V22_FSE_getdiag(&b), 0, 0);
	diff_eq_int("getdiag allocates nothing (%ld)", harness_alloc.allocs, 0,
		    0);
	diff_eq_int("getdiag frees nothing (%ld)", harness_alloc.frees, 0, 0);
	diff_eq_obj("ref state untouched", struct v22_fse, &a, &before_a, 0);
	diff_eq_obj("our state untouched", struct v22_fse, &b, &before_b, 0);

	rc = diff_end();
	return rc;
}

int
main(void)
{
	int rc = 0;

	make_drive();
	count_separations();

	rc |= tables();
	rc |= run_init_fresh("v22 fse init(fresh)");
	rc |= run_init_reuse("v22 fse init(reuse)");
	rc |= run_free("v22 fse free");
	rc |= run_getdiag("v22 fse getdiag");

	/*
	 * The guards.  A pass with any of these at zero would mean the run
	 * never exercised the thing it claims to have checked -- see the file
	 * comment for why the reversal one in particular is not optional.
	 */
	diff_begin("v22 fse separation");
	diff_eq_int("reversal separated (%ld)", sep_reversal > 0, 1,
		    sep_reversal);
	diff_eq_int("scaling separated (%ld)", sep_scaling > 0, 1,
		    sep_scaling);
	diff_eq_int("arithmetic shift separated (%ld)", sep_arithmetic > 0, 1,
		    sep_arithmetic);
	diff_eq_int("I and Q separated (%ld)", sep_i_vs_q > 0, 1, sep_i_vs_q);
	diff_eq_int("history tail separated (%ld)", sep_hist_tail > 0, 1,
		    sep_hist_tail);
	rc |= diff_end();

	return rc;
}

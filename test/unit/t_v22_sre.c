/*
 * t_v22_sre.c -- differential test of V.22's symbol-timing recovery block.
 *
 * V22_SRE_recover is not reconstructed yet (it needs FPM_atan first, which
 * the harness renames out from under any caller), so what is tested here is
 * the object's construction: the seven tables, V22_SRE_init on both its
 * paths, and V22_SRE_free's three deallocations.
 *
 * Three things here are easy to get wrong and would look perfectly healthy:
 *
 *   - the coefficient buffer is 270 entries and SREv22_COFFS is 271.  Copy
 *     271 and you overrun by two bytes; copy 270 but permute them wrongly
 *     and every branch of the resampler is a different filter that still
 *     looks like a filter.  The permutation is checked entry by entry
 *     against the blob's own result, and separately shown to have happened.
 *   - `hist` is allocated as 54 shorts and init clears 27 of them.  Both
 *     numbers are asserted, the second by leaving a marker in the upper half
 *     and requiring it to survive.
 *   - init takes a `fresh` flag.  On the fresh path it allocates and does
 *     not look at the existing pointers; the allocation SIZES are asserted,
 *     because a buffer that is too small is invisible until something reads
 *     off the end of it.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v22_sre.h"

extern void ref_V22_SRE_init(struct v22_sre *sre, int fresh);
extern void ref_V22_SRE_free(struct v22_sre *sre);

extern const short ref_SREv22_COFFS[];
extern const short ref_SREv22_xCLOCK[];
extern const short ref_SREv22_yCLOCK[];
extern const short ref_SRE_ALPHA_AVG[];
extern const short ref_SRE_BETA_AVG[];
extern const short ref_SREv22_PLL_K1[];
extern const short ref_SREv22_PLL_K2[];

/*
 * A marker no coefficient, no history sample and no scalar of this object
 * can be confused with.  Anything init is supposed to write and does not
 * shows up as a survivor.
 */
#define MARK 0x5ead

/* What the harness's allocator fills a fresh block with, as a short. */
#define FILL16 ((short)((HARNESS_MALLOC_FILL << 8) | HARNESS_MALLOC_FILL))

/* Set by the tests below; see the guards in main(). */
static int permuted;
static int upper_half_survived;

/*
 * The three heap pointers are two different addresses and always will be, so
 * they are the sanctioned skip.  Blanking them in a copy keeps diff_eq_obj's
 * field-level report and its run coalescing, which an open-coded byte loop
 * would throw away.
 */
static void
blank_pointers(struct v22_sre *dst, const struct v22_sre *src)
{
	*dst = *src;
	dst->coeff = (short *)0;
	dst->hist = (short *)0;
	dst->clk = (short *)0;
}

static void
compare_buffers(const struct v22_sre *ours, const struct v22_sre *ref,
		int hist_words)
{
	int i;

	for (i = 0; i < V22_SRE_COEFFS; i++)
		diff_eq_int("coeff[%ld]", ours->coeff[i], ref->coeff[i], i);
	for (i = 0; i < hist_words; i++)
		diff_eq_int("hist[%ld]", ours->hist[i], ref->hist[i], i);
	for (i = 0; i < V22_SRE_CLOCK; i++)
		diff_eq_int("clk[%ld]", ours->clk[i], ref->clk[i], i);
}

/*
 * init with `fresh` set: both sides allocate their own three buffers.  The
 * harness fills every allocation with HARNESS_MALLOC_FILL, so even the part
 * of `hist` that init deliberately leaves alone is comparable -- both sides
 * see the same pattern there, and a reconstruction that cleared it would
 * fail this rather than pass silently.
 */
static int
run_init_fresh(const char *label)
{
	struct v22_sre a, b, ma, mb;
	int allocs_ref, allocs_ours;
	unsigned sz_coeff, sz_hist, sz_clk;
	int rc, i;

	memset(&a, MARK & 0xff, sizeof a);
	memset(&b, MARK & 0xff, sizeof b);

	diff_begin(label);

	/*
	 * One reset, not one per side: resetting between them would drop the
	 * reference's blocks from the live set, and the frees at the end
	 * would then be counted as frees of pointers nobody handed out.
	 */
	harness_alloc_reset();
	ref_V22_SRE_init(&a, 1);
	allocs_ref = harness_alloc.allocs;

	V22_SRE_init(&b, 1);
	allocs_ours = harness_alloc.allocs - allocs_ref;
	sz_coeff = harness_alloc_reqsize(b.coeff);
	sz_hist = harness_alloc_reqsize(b.hist);
	sz_clk = harness_alloc_reqsize(b.clk);

	diff_eq_int("allocations (%ld)", allocs_ours, allocs_ref, 0);
	diff_eq_int("allocations is three (%ld)", allocs_ours, 3, 0);

	/*
	 * Sizes, not just counts.  540 = 270 coefficients, 108 = 54 history
	 * shorts (twice the tap count), 12 = the six clock words.
	 */
	diff_eq_int("coeff bytes (%ld)", (long)sz_coeff,
		    (long)(V22_SRE_COEFFS * sizeof(short)), 0);
	diff_eq_int("hist bytes (%ld)", (long)sz_hist,
		    (long)(V22_SRE_HIST * sizeof(short)), 0);
	diff_eq_int("clk bytes (%ld)", (long)sz_clk,
		    (long)(V22_SRE_CLOCK * sizeof(short)), 0);

	blank_pointers(&ma, &a);
	blank_pointers(&mb, &b);
	diff_eq_obj("init(fresh) state", struct v22_sre, &mb, &ma, 0);

	compare_buffers(&b, &a, V22_SRE_HIST);

	/*
	 * Non-vacuity for the permutation.  If init merely copied the
	 * prototype, every entry would match SREv22_COFFS in order.  Count
	 * the ones that do not; the guard in main() requires some.
	 */
	for (i = 0; i < V22_SRE_COEFFS; i++)
		if (b.coeff[i] != SREv22_COFFS[i])
			permuted++;

	/*
	 * The one spot check taken straight from the disassembly: the outer
	 * loop starts at index 260 and the inner walks down by 10, so the
	 * first word of the permuted filter is prototype tap 260.
	 */
	diff_eq_int("coeff[0] is COFFS[260] (%ld)", b.coeff[0],
		    SREv22_COFFS[V22_SRE_COEFFS - V22_SRE_BRANCHES], 0);

	rc = diff_end();

	ref_V22_SRE_free(&a);
	V22_SRE_free(&b);
	return rc;
}

/*
 * init with `fresh` clear: the buffers are ours, pre-loaded with MARK, so
 * every word init writes and every word it leaves alone can be named.
 */
static int
run_init_reuse(const char *label)
{
	static short coeff_a[V22_SRE_COEFFS], coeff_b[V22_SRE_COEFFS];
	static short hist_a[V22_SRE_HIST], hist_b[V22_SRE_HIST];
	static short clk_a[V22_SRE_CLOCK], clk_b[V22_SRE_CLOCK];
	struct v22_sre a, b, ma, mb;
	int rc, i;

	memset(&a, MARK & 0xff, sizeof a);
	memset(&b, MARK & 0xff, sizeof b);

	for (i = 0; i < V22_SRE_COEFFS; i++)
		coeff_a[i] = coeff_b[i] = (short)MARK;
	for (i = 0; i < V22_SRE_HIST; i++)
		hist_a[i] = hist_b[i] = (short)MARK;
	for (i = 0; i < V22_SRE_CLOCK; i++)
		clk_a[i] = clk_b[i] = (short)MARK;

	a.coeff = coeff_a; a.hist = hist_a; a.clk = clk_a;
	b.coeff = coeff_b; b.hist = hist_b; b.clk = clk_b;

	diff_begin(label);

	harness_alloc_reset();
	ref_V22_SRE_init(&a, 0);
	V22_SRE_init(&b, 0);

	/* Nothing may be allocated on this path, by either side. */
	diff_eq_int("reuse allocates nothing (%ld)", harness_alloc.allocs, 0,
		    0);

	blank_pointers(&ma, &a);
	blank_pointers(&mb, &b);
	diff_eq_obj("init(reuse) state", struct v22_sre, &mb, &ma, 0);

	compare_buffers(&b, &a, V22_SRE_HIST);

	/* Every coefficient and every clock word must have been written. */
	for (i = 0; i < V22_SRE_COEFFS; i++)
		diff_eq_int("coeff[%ld] was written",
			    coeff_b[i] != (short)MARK, 1, i);
	for (i = 0; i < V22_SRE_CLOCK; i++)
		diff_eq_int("clk[%ld] was written", clk_b[i] != (short)MARK, 1,
			    i);

	/*
	 * And the history's two halves must differ: the lower `taps` cleared,
	 * the upper left exactly as it was found.  This is the whole evidence
	 * for the buffer being twice as long as the clear.
	 */
	for (i = 0; i < V22_SRE_TAPS; i++)
		diff_eq_int("hist[%ld] cleared", hist_b[i], 0, i);
	for (i = V22_SRE_TAPS; i < V22_SRE_HIST; i++) {
		diff_eq_int("hist[%ld] untouched", hist_b[i], (short)MARK, i);
		if (hist_b[i] == (short)MARK)
			upper_half_survived++;
	}

	rc = diff_end();
	return rc;
}

/*
 * free releases the three buffers and nothing else.  With the harness's
 * accounting that is a statement a test can actually make: three frees, no
 * frees of anything the allocator never handed out, nothing left live, and
 * the object itself unchanged.
 */
static int
run_free(const char *label)
{
	struct v22_sre a, b, before_a, before_b;
	int rc;

	memset(&a, MARK & 0xff, sizeof a);
	memset(&b, MARK & 0xff, sizeof b);

	diff_begin(label);

	harness_alloc_reset();
	ref_V22_SRE_init(&a, 1);
	V22_SRE_init(&b, 1);
	diff_eq_int("six buffers live (%ld)", harness_alloc.live, 6, 0);

	before_a = a;
	before_b = b;

	ref_V22_SRE_free(&a);
	diff_eq_int("ref freed three (%ld)", harness_alloc.frees, 3, 0);
	V22_SRE_free(&b);
	diff_eq_int("frees total (%ld)", harness_alloc.frees, 6, 0);
	diff_eq_int("nothing left live (%ld)", harness_alloc.live, 0, 0);
	diff_eq_int("no stray free (%ld)", harness_alloc.bad_free, 0, 0);
	diff_eq_int("no null free (%ld)", harness_alloc.free_null, 0, 0);
	diff_eq_int("live set never overflowed (%ld)", harness_alloc.overflow,
		    0, 0);

	/*
	 * free leaves the pointers dangling on both sides, so the objects are
	 * still byte-identical to what init left -- including the three
	 * addresses, which is why these two comparisons are against each
	 * side's own snapshot rather than against each other.
	 */
	diff_eq_obj("ref object unchanged by free", struct v22_sre, &a,
		    &before_a, 0);
	diff_eq_obj("our object unchanged by free", struct v22_sre, &b,
		    &before_b, 0);

	rc = diff_end();
	return rc;
}

/* ------------------------------------------------------------------------ */
/* V22_SRE_recover                                                           */
/* ------------------------------------------------------------------------ */

extern short ref_V22_SRE_recover(struct v22_sre *sre, const short *in,
				 short *out, short count);

#define MAXBLOCK 160
#define MAXOUT   (4 * MAXBLOCK)

/* Coverage, all measured on the reference side. */
static long saw_mode[3];
static long saw_interpolated;
static long saw_active_set;
static long saw_active_cleared;
static long saw_frac_wrap;
static long saw_branch_moved;
static long saw_need_not_one;
static long saw_short_call;
static long saw_slide;
static long saw_settle_full;

/* One cycle of a 600 Hz tone at 3600 Hz, at close to full scale. */
static const short tone6[V22_SRE_CLOCK] = {
	0, 28377, 28377, 0, -28377, -28377
};

enum wave { W_TONE, W_NOISE, W_MIX, W_QUIET, W_DRIFT, W_RAMP };

static void
fill_block(int wave, int amp, unsigned *lfsr, long *pos, short *buf, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		long p = (*pos)++;
		int v = 0;
		int r;

		*lfsr = (*lfsr >> 1) ^ (unsigned)(-(int)(*lfsr & 1u)
						  & 0xB400u);
		r = (int)(*lfsr & 0xffffu) - 0x8000;

		switch (wave) {
		case W_TONE:
			v = tone6[p % V22_SRE_CLOCK];
			break;
		case W_NOISE:
			v = r;
			break;
		case W_MIX:
			v = tone6[p % V22_SRE_CLOCK] / 2 + r / 8;
			break;
		case W_QUIET:
			v = 0;
			break;
		case W_DRIFT:
			/* One extra sample every 97: a slow phase slip, so
			 * the loop has something to chase. */
			v = tone6[(p + p / 97) % V22_SRE_CLOCK];
			break;
		default:
			v = (int)((p % 211) - 105) * 300;
			break;
		}

		buf[i] = (short)((v * amp) / 32767);
	}
}

/* How the state is armed before a stream, identically on both sides. */
enum arm {
	ARM_NONE,
	ARM_ACQUIRED,	/* settle just short of the 0 -> 1 shift */
	ARM_TRACKING,	/* mode 2, so the interpolator runs */
	ARM_FROZEN,	/* adapt clear: measure but do not steer */
	ARM_FRAC_HIGH,	/* frac one below its wrap */
	ARM_FRAC_LOW,	/* frac at zero, so a negative step must borrow */
	ARM_FAST	/* groups = 1: a PLL update every six samples */
};

static void
arm_state(struct v22_sre *s, int how)
{
	switch (how) {
	case ARM_ACQUIRED:
		s->settle = 0x3f;
		s->mode = 0;
		s->active = 1;
		s->mag_avg = 4000;
		break;
	case ARM_TRACKING:
		s->settle = 65;
		s->mode = 2;
		s->groups = 12;
		s->acquiring = 0;
		s->active = 1;
		s->mag_avg = 4000;
		s->frac = 1000;
		break;
	case ARM_FROZEN:
		s->adapt = 0;
		s->active = 1;
		s->mag_avg = 4000;
		break;
	case ARM_FRAC_HIGH:
		s->settle = 65;
		s->mode = 1;
		s->active = 1;
		s->mag_avg = 4000;
		s->frac = V22_SRE_FRAC_ONE - 1;
		s->branch = 9;
		break;
	case ARM_FRAC_LOW:
		s->settle = 65;
		s->mode = 1;
		s->active = 1;
		s->mag_avg = 4000;
		s->frac = 0;
		s->branch = 0;
		break;
	case ARM_FAST:
		s->settle = 65;
		s->mode = 1;
		s->groups = 1;
		s->active = 1;
		s->mag_avg = 4000;
		break;
	default:
		break;
	}
}

static void
observe(const struct v22_sre *before, const struct v22_sre *after,
	const short *coeff_before, short need_in, short count)
{
	int i;

	if (after->mode >= 0 && after->mode < V22_SRE_MODES)
		saw_mode[after->mode]++;
	if (after->active && !before->active)
		saw_active_set++;
	if (!after->active && before->active)
		saw_active_cleared++;
	if (after->branch != before->branch)
		saw_branch_moved++;
	if (after->frac != before->frac)
		saw_frac_wrap++;
	if (after->need != 1)
		saw_need_not_one++;
	if (after->settle > 0x40)
		saw_settle_full++;
	if (count < need_in)
		saw_short_call++;
	/* The slide is the only way `fill` can fall. */
	if (after->fill < before->fill)
		saw_slide++;

	for (i = 0; i < V22_SRE_COEFFS; i++)
		if (after->coeff[i] != coeff_before[i]) {
			saw_interpolated++;
			break;
		}
}

/*
 * `blocks` calls of `count` samples each.  State and both variable-length
 * buffers are compared after EVERY call, because a timing loop carries
 * everything across calls and a divergence on block 40 is invisible if only
 * the last one is checked.
 */
static int
run_stream(const char *label, int wave, int amp, int count, int blocks,
	   int how)
{
	struct v22_sre a, b, ma, mb;
	static short coeff_before[V22_SRE_COEFFS];
	short in[MAXBLOCK];
	short oa[MAXOUT], ob[MAXOUT];
	struct v22_sre before;
	unsigned lfsr = 0x51F3u;
	long pos = 0;
	short na, nb, need_in;
	int rc, n, i;

	/*
	 * Both objects start from the same bytes, because NEITHER SIDE EVER
	 * WRITES pad3a -- init's stores stop at +0x38 and recover's do too --
	 * so without this the tail padding is two different pieces of stack
	 * garbage and every whole-object comparison reports +0x3a and
	 * nothing else.  That is worth saying rather than skipping the field:
	 * the padding being untouched is a fact about the original, and this
	 * is the form of the test that still asserts it.
	 */
	memset(&a, MARK & 0xff, sizeof a);
	memset(&b, MARK & 0xff, sizeof b);

	ref_V22_SRE_init(&a, 1);
	V22_SRE_init(&b, 1);
	arm_state(&a, how);
	arm_state(&b, how);

	diff_begin(label);
	for (n = 0; n < blocks; n++) {
		fill_block(wave, amp, &lfsr, &pos, in, count);
		for (i = 0; i < MAXOUT; i++)
			oa[i] = ob[i] = (short)MARK;

		before = a;
		for (i = 0; i < V22_SRE_COEFFS; i++)
			coeff_before[i] = a.coeff[i];
		need_in = a.need;

		na = ref_V22_SRE_recover(&a, in, oa, (short)count);
		nb = V22_SRE_recover(&b, in, ob, (short)count);

		diff_eq_int("block %ld: return", nb, na, n);
		for (i = 0; i < MAXOUT; i++)
			diff_eq_int("block: out[%ld]", ob[i], oa[i], i);

		blank_pointers(&ma, &a);
		blank_pointers(&mb, &b);
		diff_eq_obj("state after block", struct v22_sre, &mb, &ma, n);
		compare_buffers(&b, &a, V22_SRE_HIST);

		observe(&before, &a, coeff_before, need_in, (short)count);

		/*
		 * An output count of zero for a full-length block would mean
		 * the loop had stopped moving and every later check was
		 * comparing two idle machines.
		 */
		if (count >= V22_SRE_CLOCK)
			diff_eq_int("block %ld produced something",
				    na > 0, 1, n);
	}
	rc = diff_end();

	ref_V22_SRE_free(&a);
	V22_SRE_free(&b);
	return rc;
}

int
main(void)
{
	int rc = 0;
	int i;

	/* --- the seven tables ------------------------------------------ */

	diff_begin("v22 sre tables");
	for (i = 0; i < V22_SRE_COFFS_LEN; i++)
		diff_eq_int("SREv22_COFFS[%ld]", SREv22_COFFS[i],
			    ref_SREv22_COFFS[i], i);
	for (i = 0; i < V22_SRE_CLOCK; i++) {
		diff_eq_int("SREv22_xCLOCK[%ld]", SREv22_xCLOCK[i],
			    ref_SREv22_xCLOCK[i], i);
		diff_eq_int("SREv22_yCLOCK[%ld]", SREv22_yCLOCK[i],
			    ref_SREv22_yCLOCK[i], i);
	}
	for (i = 0; i < V22_SRE_MODES; i++) {
		diff_eq_int("SRE_ALPHA_AVG[%ld]", SRE_ALPHA_AVG[i],
			    ref_SRE_ALPHA_AVG[i], i);
		diff_eq_int("SRE_BETA_AVG[%ld]", SRE_BETA_AVG[i],
			    ref_SRE_BETA_AVG[i], i);
		diff_eq_int("SREv22_PLL_K1[%ld]", SREv22_PLL_K1[i],
			    ref_SREv22_PLL_K1[i], i);
		diff_eq_int("SREv22_PLL_K2[%ld]", SREv22_PLL_K2[i],
			    ref_SREv22_PLL_K2[i], i);
	}
	rc |= diff_end();

	/* --- V22_SRE_init ---------------------------------------------- */

	rc |= run_init_fresh("v22 sre init, fresh");
	rc |= run_init_reuse("v22 sre init, reusing buffers");

	/* --- V22_SRE_free ---------------------------------------------- */

	rc |= run_free("v22 sre free");

	/* --- V22_SRE_recover ------------------------------------------- */

	/*
	 * The organic runs.  A tone at exactly one sixth of the sample rate
	 * is what this block exists to lock to, so that is the case that has
	 * to be long enough for `settle` to reach its limit -- 65 updates at
	 * 18 samples each is 1170 samples, hence the block count.
	 */
	rc |= run_stream("v22 sre recover, 600 Hz tone", W_TONE, 32767,
			 MAXBLOCK, 24, ARM_NONE);
	rc |= run_stream("v22 sre recover, tone with slip", W_DRIFT, 32767,
			 MAXBLOCK, 24, ARM_NONE);
	rc |= run_stream("v22 sre recover, tone in noise", W_MIX, 32767,
			 MAXBLOCK, 16, ARM_NONE);
	rc |= run_stream("v22 sre recover, noise only", W_NOISE, 32767,
			 MAXBLOCK, 12, ARM_NONE);

	/*
	 * Silence: the envelope decays through the squelch, which is the only
	 * way `active` is ever cleared and the only path that resets `groups`
	 * and `settle` from inside the loop.
	 */
	rc |= run_stream("v22 sre recover, tone then silence", W_QUIET, 0,
			 MAXBLOCK, 8, ARM_ACQUIRED);
	rc |= run_stream("v22 sre recover, ramp", W_RAMP, 32767, MAXBLOCK, 8,
			 ARM_NONE);

	/*
	 * Armed states, because reaching mode 2 organically needs both a
	 * converged loop and 1200 outputs, and missing it would leave the
	 * interpolator -- the largest single block in the function -- and the
	 * near-in-phase skip completely untested.
	 */
	rc |= run_stream("v22 sre recover, tracking mode", W_DRIFT, 32767,
			 MAXBLOCK, 16, ARM_TRACKING);
	rc |= run_stream("v22 sre recover, tracking on noise", W_NOISE, 32767,
			 MAXBLOCK, 12, ARM_TRACKING);
	rc |= run_stream("v22 sre recover, at the mode shift", W_TONE, 32767,
			 MAXBLOCK, 12, ARM_ACQUIRED);
	rc |= run_stream("v22 sre recover, frozen", W_DRIFT, 32767, MAXBLOCK,
			 8, ARM_FROZEN);
	rc |= run_stream("v22 sre recover, frac at its wrap", W_DRIFT, 32767,
			 MAXBLOCK, 8, ARM_FRAC_HIGH);
	rc |= run_stream("v22 sre recover, frac at zero", W_DRIFT, 32767,
			 MAXBLOCK, 8, ARM_FRAC_LOW);
	rc |= run_stream("v22 sre recover, an update every six", W_DRIFT,
			 32767, MAXBLOCK, 12, ARM_FAST);
	rc |= run_stream("v22 sre recover, quiet drive", W_MIX, 300, MAXBLOCK,
			 8, ARM_NONE);

	/*
	 * Fragment lengths.  Anything shorter than `need` takes the tail path
	 * and returns without producing, and the history slide has to survive
	 * being reached from either path.
	 */
	rc |= run_stream("v22 sre recover, one sample at a time", W_DRIFT,
			 32767, 1, 400, ARM_NONE);
	rc |= run_stream("v22 sre recover, three at a time", W_TONE, 32767, 3,
			 200, ARM_NONE);
	rc |= run_stream("v22 sre recover, seven at a time", W_MIX, 32767, 7,
			 120, ARM_NONE);
	rc |= run_stream("v22 sre recover, 53 at a time", W_DRIFT, 32767, 53,
			 40, ARM_TRACKING);
	rc |= run_stream("v22 sre recover, zero length", W_TONE, 32767, 0, 4,
			 ARM_NONE);

	/* --- the guards ------------------------------------------------ */

	/*
	 * Finding 134's argument.  Both of these would go quiet if the tests
	 * above were weakened, and with them the only coverage of the two
	 * properties of init that are not obvious from its name.
	 */
	diff_begin("v22 sre coverage");
	diff_eq_int("prototype really was permuted (%ld)", permuted > 200, 1,
		    0);
	diff_eq_int("upper history half really was left alone (%ld)",
		    upper_half_survived, V22_SRE_HIST - V22_SRE_TAPS, 0);

	/*
	 * Everything below is measured on the REFERENCE side, so it says the
	 * blob went there, not that our copy thinks it did.  Each one covers
	 * a branch that a shorter or gentler test would leave dead.
	 */
	for (i = 0; i < V22_SRE_MODES; i++)
		diff_eq_int("mode %ld was occupied", saw_mode[i] > 0, 1, i);
	diff_eq_int("the interpolator ran (%ld)", saw_interpolated > 0, 1, 0);
	diff_eq_int("squelch opened (%ld)", saw_active_set > 0, 1, 0);
	diff_eq_int("squelch closed (%ld)", saw_active_cleared > 0, 1, 0);
	diff_eq_int("the sampling phase moved (%ld)", saw_branch_moved > 0, 1,
		    0);
	diff_eq_int("the sub-branch phase moved (%ld)", saw_frac_wrap > 0, 1,
		    0);
	diff_eq_int("a block needed other than one input (%ld)",
		    saw_need_not_one > 0, 1, 0);
	diff_eq_int("the short-call path was taken (%ld)", saw_short_call > 0,
		    1, 0);
	diff_eq_int("the history slid (%ld)", saw_slide > 0, 1, 0);
	diff_eq_int("settle reached its limit (%ld)", saw_settle_full > 0, 1,
		    0);
	rc |= diff_end();

	return rc;
}

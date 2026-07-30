/*
 * t_b103create.c -- differential test of B103FP_create and B103FP_delete.
 *
 * 2151 bytes of constructor, and the thing that makes reconstructing it safe
 * is not reading it carefully -- it is comparing the whole object tree
 * afterwards.  Every field, named or not, in all three allocations, plus the
 * contents of every buffer they point at.  A field left unwritten, written
 * with the wrong value, or written in a branch that should not have been
 * taken all show up as a byte that differs.
 *
 * Pointer slots are skipped and their targets compared by content instead --
 * two objects necessarily hold different addresses.  Function pointers are
 * mapped through the same identity table t_b103hdx uses.
 *
 * Run across every call type and both tone plans, and for both the
 * self-allocating and caller-supplied paths, because the second is where the
 * ownership flags decide whether a sub-buffer is allocated or reused.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/b103fp.h"

extern struct b103fp *ref_B103FP_create(struct b103fp *state, const void *cfg);
extern void ref_B103FP_delete(struct b103fp *state);
extern short ref_TxHdxStartB103, ref_TxHdxMarksB103, ref_RxHdxDataB103;

/*
 * Offsets holding a pointer.  DERIVED with offsetof rather than listed: the
 * first version of this file hand-listed them, got four wrong, and reported
 * the high halves of two heap addresses as field mismatches.  Deriving them
 * cannot drift when a struct changes either.
 */
#define OFF(t, f) ((int)__builtin_offsetof(t, f))

static int
fp_is_pointer(int off)
{
	return off == OFF(struct b103fp, trace)
	       || off == OFF(struct b103fp, fsd_count)
	       || off == OFF(struct b103fp, hdx)
	       || off == OFF(struct b103fp, dsp);
}

static int
hdx_is_pointer(int off)
{
	return off == OFF(struct b103_hdx, tx)
	       || off == OFF(struct b103_hdx, rx)
	       || off == OFF(struct b103_hdx, tone_detect)
	       || off == OFF(struct b103_hdx, tone_lo);
}

static int
dsp_is_pointer(int off)
{
	static const int agc[] = { OFF(struct fpm_agc_cfg, alpha),
				   OFF(struct fpm_agc_cfg, beta) };
	static const int mrf[] = { OFF(struct fpm_mrf_cfg, coeff),
				   OFF(struct fpm_mrf_cfg, aux),
				   OFF(struct fpm_mrf, history) };
	static const int fsd[] = { OFF(struct fpm_fsd_cfg, fir),
				   OFF(struct fpm_fsd_cfg, iir),
				   OFF(struct fpm_fsd, trace),
				   OFF(struct fpm_fsd, fir_hist),
				   OFF(struct fpm_fsd, iir_hist) };
	unsigned i;

	for (i = 0; i < sizeof(agc) / sizeof(agc[0]); i++)
		if (off == OFF(struct b103_dsp, agc) + agc[i]
		    || off == OFF(struct b103_dsp, det_agc) + agc[i])
			return 1;
	for (i = 0; i < sizeof(mrf) / sizeof(mrf[0]); i++)
		if (off == OFF(struct b103_dsp, tx_mrf) + mrf[i]
		    || off == OFF(struct b103_dsp, rx_mrf) + mrf[i])
			return 1;
	for (i = 0; i < sizeof(fsd) / sizeof(fsd[0]); i++)
		if (off == OFF(struct b103_dsp, fsd) + fsd[i])
			return 1;

	return off == OFF(struct b103_dsp, fsm) + OFF(struct fpm_fsm, tone)
	       || off == OFF(struct b103_dsp, mtd)
	       || off == OFF(struct b103_dsp, scratch)
	       || off == OFF(struct b103_dsp, rx_scratch)
	       || off == OFF(struct b103_dsp, bpf_hist)
	       || off == OFF(struct b103_dsp, bpf);
}

/*
 * The tone object's pointers -- plus one word that is not a pointer and must
 * be skipped anyway.
 *
 * FPM_TONE_create clears its reserved region from +0x4c to +0xf0 and stops
 * there, so the LAST word, +0xf2, keeps whatever the allocator left.  Two
 * objects therefore hold different garbage in it, and comparing it compares
 * the allocator rather than the code.
 *
 * That is precisely the mistake that produced D7, so it is called out here
 * rather than quietly excluded: an uninitialised word is not a difference
 * between implementations.
 */
static int
tone_is_pointer(int off)
{
	return off == OFF(struct fpm_tone, r4c)
			+ (int)sizeof(((struct fpm_tone *)0)->r4c) - 2
	       || off == OFF(struct fpm_tone, cfg) + OFF(struct fpm_tone_cfg, src)
	       || off == OFF(struct fpm_tone, kernel)
	       || off == OFF(struct fpm_tone, history)
	       || off == OFF(struct fpm_tone, rev_block)
	       || off == OFF(struct fpm_tone, rev_acc)
	       || off == OFF(struct fpm_tone, iir_self);
}

static void
compare_block(const char *what, const unsigned char *ours,
	      const unsigned char *ref, int size, int (*is_ptr)(int), int tag)
{
	char buf[96];
	int i;

	for (i = 0; i < size; i += 2) {
		if (is_ptr(i) || is_ptr(i - 2))
			continue;
		snprintf(buf, sizeof(buf), "%s word 0x%02x (%%ld)", what, i);
		diff_eq_int(buf, *(const short *)(ours + i),
			    *(const short *)(ref + i), tag);
	}
}

static void
compare_shorts(const char *what, const short *ours, const short *ref, int n,
	       int tag)
{
	char buf[96];
	int i;

	if (ours == 0 || ref == 0) {
		snprintf(buf, sizeof(buf), "%s: both present (%%ld)", what);
		diff_eq_int(buf, ours != 0, ref != 0, tag);
		return;
	}
	snprintf(buf, sizeof(buf), "%s[%%ld]", what);
	for (i = 0; i < n; i++)
		diff_eq_int(buf, ours[i], ref[i], i);
	(void)tag;
}

/* Map a state function pointer to an identity both sides agree on. */
static int
fn_id(const void *p)
{
	if (p == 0)
		return -1;
	if (p == (const void *)TxHdxStartB103
	    || p == (const void *)&ref_TxHdxStartB103)
		return 0;
	if (p == (const void *)TxHdxMarksB103
	    || p == (const void *)&ref_TxHdxMarksB103)
		return 1;
	if (p == (const void *)RxHdxDataB103
	    || p == (const void *)&ref_RxHdxDataB103)
		return 2;
	return -2;
}

static void
compare_tree(const char *what, struct b103fp *a, struct b103fp *b, int tag)
{
	char buf[96];

	compare_block(what, (unsigned char *)a, (unsigned char *)b,
		      (int)sizeof(*a), fp_is_pointer, tag);
	compare_block(what, (unsigned char *)a->hdx, (unsigned char *)b->hdx,
		      (int)sizeof(*a->hdx), hdx_is_pointer, tag);
	compare_block(what, (unsigned char *)a->dsp, (unsigned char *)b->dsp,
		      (int)sizeof(*a->dsp), dsp_is_pointer, tag);

	snprintf(buf, sizeof(buf), "%s: hdx->tx", what);
	diff_eq_int(buf, fn_id((const void *)a->hdx->tx),
		    fn_id((const void *)b->hdx->tx), tag);
	snprintf(buf, sizeof(buf), "%s: hdx->rx", what);
	diff_eq_int(buf, fn_id((const void *)a->hdx->rx),
		    fn_id((const void *)b->hdx->rx), tag);

	/* The two tone objects, whole, minus their own pointer slots. */
	snprintf(buf, sizeof(buf), "%s tone_lo", what);
	compare_block(buf, (unsigned char *)a->hdx->tone_lo,
		      (unsigned char *)b->hdx->tone_lo,
		      FPM_TONE_STATE_SIZE, tone_is_pointer, tag);
	snprintf(buf, sizeof(buf), "%s: tone_detect present (%%ld)", what);
	diff_eq_int(buf, a->hdx->tone_detect != 0, b->hdx->tone_detect != 0, tag);
	if (a->hdx->tone_detect && b->hdx->tone_detect) {
		snprintf(buf, sizeof(buf), "%s tone_detect", what);
		compare_block(buf, (unsigned char *)a->hdx->tone_detect,
			      (unsigned char *)b->hdx->tone_detect,
			      FPM_TONE_STATE_SIZE, tone_is_pointer, tag);
	}

	/*
	 * Buffers, by content.
	 *
	 * bpf_hist ONLY when a filter is installed.  Loopback allocates the
	 * buffer and never clears it -- the memset lives in the branch that
	 * installs the bandpass, and loopback has no such branch -- so for
	 * loopback it holds allocator garbage in both objects and comparing
	 * it compares the heap.
	 *
	 * This is the third time that trap has been walked into in this
	 * project: it produced D7, then the tone object's +0xf2, then this.
	 * The rule it keeps teaching: before comparing a buffer, establish
	 * that something wrote it.
	 */
	if (a->dsp->bpf_taps > 0 && b->dsp->bpf_taps == a->dsp->bpf_taps) {
		snprintf(buf, sizeof(buf), "%s: bpf_hist", what);
		compare_shorts(buf, a->dsp->bpf_hist, b->dsp->bpf_hist,
			       a->dsp->bpf_taps, tag);
	}
	snprintf(buf, sizeof(buf), "%s: tx_mrf history", what);
	compare_shorts(buf, a->dsp->tx_mrf.history, b->dsp->tx_mrf.history,
		       a->dsp->tx_mrf.history_len, tag);
	snprintf(buf, sizeof(buf), "%s: rx_mrf history", what);
	compare_shorts(buf, a->dsp->rx_mrf.history, b->dsp->rx_mrf.history,
		       a->dsp->rx_mrf.history_len, tag);
	snprintf(buf, sizeof(buf), "%s: fsd fir_hist", what);
	compare_shorts(buf, a->dsp->fsd.fir_hist, b->dsp->fsd.fir_hist,
		       a->dsp->fsd.cfg.fir_taps, tag);
	snprintf(buf, sizeof(buf), "%s: fsd iir_hist", what);
	compare_shorts(buf, a->dsp->fsd.iir_hist, b->dsp->fsd.iir_hist,
		       a->dsp->fsd.cfg.iir_len * 2, tag);
	snprintf(buf, sizeof(buf), "%s: scratch", what);
	compare_shorts(buf, a->dsp->scratch, b->dsp->scratch, 162, tag);
}

int
main(void)
{
	static const struct {
		int call_type, v21, high;
		const char *name;
	} cases[] = {
		{ B103_CALL_ORIGINATE, 0, 0, "originate" },
		{ B103_CALL_ORIGINATE, 1, 0, "originate v21" },
		{ B103_CALL_ANSWER,    0, 0, "answer" },
		{ B103_CALL_ANSWER,    1, 0, "answer v21" },
		{ B103_CALL_LOOPBACK,  0, 0, "loopback" },
		{ B103_CALL_LOOPBACK,  0, 1, "loopback high" },
		{ 99,                  0, 0, "out of range" }
	};
	int rc = 0;
	unsigned k;

	diff_begin("B103FP_create, self-allocating");
	for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
		struct b103_cfg cfg = B103_CFG_data;
		struct b103fp *a, *b;

		cfg.call_type = cases[k].call_type;
		cfg.v21 = cases[k].v21;
		cfg.loop_high_channel = cases[k].high;

		b = ref_B103FP_create(0, &cfg);
		a = B103FP_create(0, &cfg);
		if (a == 0 || b == 0) {
			diff_eq_int("both built (%ld)", a != 0 && b != 0, 1,
				    (long)k);
			continue;
		}
		compare_tree(cases[k].name, a, b, (long)k);
		B103FP_delete(a);
		ref_B103FP_delete(b);
	}
	rc |= diff_end();

	/*
	 * The caller-supplied path.  The object must be zeroed first -- the
	 * sub-pointers are tested for NULL to decide whether to allocate --
	 * and this is where `fresh` matters: it is what stops every sub-object
	 * allocating its buffers a second time.
	 */
	diff_begin("B103FP_create, caller-supplied");
	for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
		struct b103_cfg cfg = B103_CFG_data;
		static struct b103fp sa, sb;
		struct b103fp *a, *b;

		cfg.call_type = cases[k].call_type;
		cfg.v21 = cases[k].v21;
		cfg.loop_high_channel = cases[k].high;

		memset(&sa, 0, sizeof(sa));
		memset(&sb, 0, sizeof(sb));
		b = ref_B103FP_create(&sb, &cfg);
		a = B103FP_create(&sa, &cfg);
		diff_eq_int("returned the caller's buffer (%ld)",
			    a == &sa && b == &sb, 1, (long)k);
		compare_tree(cases[k].name, a, b, (long)k);
		/*
		 * Not deleted: B103FP_delete would free these static buffers
		 * (D8).  The sub-allocations leak for the rest of the run,
		 * which is what t_b103alloc measures deliberately.
		 */
	}
	rc |= diff_end();

	/* Allocation counts must match the reference exactly. */
	diff_begin("B103FP_create allocation counts");
	for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
		struct b103_cfg cfg = B103_CFG_data;
		struct b103fp *fp;
		int ra, rb, rf, af, ab;

		cfg.call_type = cases[k].call_type;
		cfg.v21 = cases[k].v21;
		cfg.loop_high_channel = cases[k].high;

		harness_alloc_reset();
		fp = ref_B103FP_create(0, &cfg);
		ra = harness_alloc.allocs;
		ref_B103FP_delete(fp);
		rf = harness_alloc.frees;

		harness_alloc_reset();
		fp = B103FP_create(0, &cfg);
		ab = harness_alloc.allocs;
		B103FP_delete(fp);
		af = harness_alloc.frees;

		printf("  %-14s ref %2d/%2d  ours %2d/%2d\n",
		       cases[k].name, ra, rf, ab, af);
		diff_eq_int("%ld: allocation count", ab, ra, (long)k);
		diff_eq_int("%ld: free count", af, rf, (long)k);
		diff_eq_int("%ld: nothing leaked", harness_alloc.live, 0,
			    (long)k);
		(void)rb;
	}
	rc |= diff_end();

	return rc;
}

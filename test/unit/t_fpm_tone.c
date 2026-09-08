/*
 * t_fpm_tone.c -- differential test of the tone generator.
 *
 * FPM_TONE_create is compared with explicit and NULL configurations, including
 * the allocated buffers.  Generator tests also use cloned reference-built
 * objects to isolate their state updates; detector tests build independent
 * buffers for both sides.  The whole 0x108-byte object is compared afterwards,
 * so a write to any field, named or not, shows up.
 *
 * The reversal path needs care to reach: at the default 450-unit period and
 * 8 samples per unit, it takes 3600 samples to fire once.  Short bursts never
 * touch it, which is exactly how it would slip through a casual test.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_tone.h"

extern void *ref_FPM_TONE_create(void *state, void *cfg);
extern void ref_FPM_TONE_set_freq(void *state, short hz);
extern void ref_FPM_TONE_set_scale(void *state, short scale);
extern void ref_FPM_TONE_generate(void *state, short *out, short count);
extern short ref_FPM_TONE_CFG[];
extern short ref_FPM_TONE_generate_demod(void *state, short *out,
					 short count);
extern short ref_FPM_TONE_generate2(void *state, short *cos_out,
				    short *sin_out, short count);
extern short ref_FPM_TONE_detect(void *state, const short *samples,
				 short count);
extern void ref_FPM_TONE_delete(void *state);
extern void ref_FPM_TONE_kill(void *state, short *samples, short count);
extern short ref_FPM_TONE_find_rev(void *state, short *samples, short count);
extern void ref_FPM_TONE_filter(void *state, short *samples, short count);

/*
 * Pass 0's filtered output, kept so pass 1 can be shown to differ from it.
 * 40 trials of at most 17 samples; sized generously rather than exactly.
 */
static short kill_pass0[40][64];

/* V.25's answer-tone checks are intentionally run on each implementation
 * alone.  A fixed-point scale has no calibrated dBm0 mapping here, so this
 * covers only independently derivable frequency, reversal period and phase. */
static int
t_fpm_v25_one(const char *name, int reference)
{
	struct fpm_tone *t;
	short out[160];
	int inc, block, previous, reversal_sample = -1;
	int phase_delta = -1;

	t = reference
		? ref_FPM_TONE_create(NULL, ref_FPM_TONE_CFG)
		: FPM_TONE_create(NULL, &FPM_TONE_CFG);
	diff_begin(name);
	diff_eq_int("FPM tone object allocated (%ld)", t != NULL, 1, 0);
	if (t == NULL)
		return diff_end();

	/* V.25 §2.2: 2100 +/- 15 Hz.  The implementation's documented 8 kHz
	 * phase-increment formula is evaluated here, rather than trusting an
	 * output FFT or the configuration field by itself. */
	diff_eq_int("V.25 configured frequency (%ld)", t->cfg.freq, 2100, 0);
	inc = ((int)t->cfg.freq * 0x8312 + 0x1000) >> 13;
	diff_eq_int("independent phase increment (%ld)", t->inc, inc, 0);
	diff_eq_int("V.25 quantized frequency is legal (%ld)",
		    inc * 8000 >= 2085 * 32768 && inc * 8000 <= 2115 * 32768,
		    1, inc);

	/* The 450 counter units are eight samples at 8 kHz.  In the production
	 * V.23 framing path the counter advances 20 units per 160-sample call,
	 * so its first legal reversal occurs at 23*160 = 3680 samples (460 ms).
	 * Check the actual oscillator state jump too: one half of its 0x8000
	 * cycle is exactly 180 degrees, inside §2.3's 180 +/- 10 degrees. */
	diff_eq_int("V.25 configured reversal period (%ld)",
		    t->cfg.rev_period, 450, 0);
	previous = t->rev_count;
	for (block = 1; block <= 23; block++) {
		int before = (unsigned short)t->phase & 0x7fff;
		int normal = (before + 160 * inc) & 0x7fff;
		int now;

		if (reference)
			ref_FPM_TONE_generate(t, out, 160);
		else
			FPM_TONE_generate(t, out, 160);
		now = t->rev_count;
		if (now < previous) {
			reversal_sample = block * 160;
			phase_delta = (((unsigned short)t->phase & 0x7fff)
				       - normal) & 0x7fff;
		}
		previous = now;
	}
	diff_eq_int("reversal was observed (%ld)", reversal_sample != -1, 1, 0);
	diff_eq_int("V.25 reversal interval is legal (%ld)",
		    reversal_sample >= 425 * 8 && reversal_sample <= 475 * 8,
		    1, reversal_sample);
	diff_eq_int("V.25 reversal is 180 degrees (%ld)", phase_delta, 0x4000,
		    reversal_sample);

	if (reference)
		ref_FPM_TONE_delete(t);
	else
		FPM_TONE_delete(t);
	return diff_end();
}

/* Compare the whole object, so unnamed fields are covered too. */
static void
compare_state(const unsigned char *ours, const unsigned char *ref,
	      const char *what)
{
	int i;

	for (i = 0; i < FPM_TONE_STATE_SIZE; i++)
		diff_eq_int("state byte 0x%02lx", ours[i], ref[i], i);
	(void)what;
}

/*
 * Compare two created objects.  The four buffer pointers and the internal
 * self-pointer at +0xfc necessarily differ between builds, so those slots are
 * skipped and the buffers compared by content instead.
 */
static int
is_pointer_slot(int off)
{
	return off == 0x10 || off == 0x2c || off == 0x30
	       || off == 0xf4 || off == 0xf8 || off == 0xfc;
}

static void
compare_created(const unsigned char *ours, const unsigned char *ref, int len,
		int extra, int same_cfg)
{
	int i;

	for (i = 0; i < FPM_TONE_STATE_SIZE; i += 2) {
		if (is_pointer_slot(i) || is_pointer_slot(i - 2))
			continue;
		diff_eq_int("created byte 0x%02lx",
			    *(const short *)(ours + i),
			    *(const short *)(ref + i), i);
	}

	if (same_cfg) {
		/* Same config object, so the copied pointer must be identical. */
		diff_eq_int("source pointer copied (%ld)",
			    *(void *const *)(ours + 0x10)
			    == *(void *const *)(ref + 0x10), 1, 0);
	} else {
		/*
		 * Different config objects -- ours uses our extracted ToneLPF,
		 * the reference its own.  The addresses cannot match, so check
		 * the thing that actually matters: that our extraction of the
		 * prototype agrees with the blob's, tap for tap.
		 */
		const short *pa = *(const short *const *)(ref + 0x10);
		const short *pb = *(const short *const *)(ours + 0x10);

		for (i = 0; i < len; i++)
			diff_eq_int("ToneLPF[%ld]", pb[i], pa[i], i);
	}

	/* Buffer contents, not addresses. */
	{
		const short *ra = *(const short *const *)(ref + 0x2c);
		const short *rb = *(const short *const *)(ours + 0x2c);
		const short *za = *(const short *const *)(ref + 0x30);
		const short *zb = *(const short *const *)(ours + 0x30);
		const short *ka = *(const short *const *)(ref + 0xf4);
		const short *kb = *(const short *const *)(ours + 0xf4);
		const short *aa = *(const short *const *)(ref + 0xf8);
		const short *ab = *(const short *const *)(ours + 0xf8);

		for (i = 0; i < len; i++)
			diff_eq_int("reference[%ld]", rb[i], ra[i], i);
		for (i = 0; i < len + extra; i++)
			diff_eq_int("zeroed[%ld]", zb[i], za[i], i);
		for (i = 0; i < 5; i++)
			diff_eq_int("resonator coeff[%ld]", kb[i], ka[i], i);
		for (i = 0; i < 4; i++)
			diff_eq_int("resonator acc[%ld]", ab[i], aa[i], i);
	}
}

/*
 * ---------------------------------------------------------------------------
 * FPM_TONE_detect.
 *
 * Unlike the generator tests, the two sides must NOT share an object: detect
 * writes into the history buffer through the pointer at +0x30, so cloning one
 * reference object into two byte-identical copies would have both sides
 * scribbling on the same array and comparing it with itself.  Two separately
 * created objects it is, which means the pointer slots differ and are skipped.
 */
static int detect_verdicts[3];

static void
compare_detect(unsigned char *ours, unsigned char *ref, int taps, int tag)
{
	const short *ho = ((const struct fpm_tone *)ours)->history;
	const short *hr = ((const struct fpm_tone *)ref)->history;
	int i;

	for (i = 0; i < FPM_TONE_STATE_SIZE; i += 2) {
		/*
		 * Pointers are four bytes, so both halves must be skipped --
		 * skipping only the low one leaves the high half of two
		 * different heap addresses being compared, which agrees
		 * roughly eleven runs in twelve.  A test that usually passes
		 * is worse than one that fails.
		 */
		if (is_pointer_slot(i) || is_pointer_slot(i - 2))
			continue;
		diff_eq_int("state 0x%02lx",
			    *(short *)(ours + i), *(short *)(ref + i), i);
	}
	for (i = 0; i < taps; i++)
		diff_eq_int("history[%ld]", ho[i], hr[i], i);
	(void)tag;
}

static int
detect_stream(const char *what, void *src, int freq, int scale,
	      int blocks, int len)
{
	unsigned char *a, *b;
	static short buf[4096];
	int taps, k, rc;

	a = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
	b = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
	if (a == 0 || b == 0) {
		diff_begin(what);
		diff_eq_int("objects built (%ld)", 0, 1, 0);
		return diff_end();
	}
	taps = ((struct fpm_tone *)a)->cfg.len;

	ref_FPM_TONE_set_freq(src, (short)freq);
	ref_FPM_TONE_set_scale(src, (short)scale);

	diff_begin(what);
	for (k = 0; k < blocks; k++) {
		short va, vb;

		ref_FPM_TONE_generate(src, buf, (short)len);

		va = ref_FPM_TONE_detect(a, buf, (short)len);
		vb = FPM_TONE_detect((struct fpm_tone *)b, buf, (short)len);

		diff_eq_int("verdict at block %ld", vb, va, k);
		compare_detect(b, a, taps, k);

		if (va >= 0 && va <= 2)
			detect_verdicts[va]++;
	}
	rc = diff_end();
	return rc;
}

/*
 * ---------------------------------------------------------------------------
 * FPM_TONE_find_rev and FPM_TONE_filter.
 *
 * Both write through a pointer held in the object -- find_rev through
 * `rev_acc` at +0xf8, filter through `history` at +0x30 -- so, like detect and
 * unlike the generators, the two sides need separately created objects rather
 * than two copies of one.  The pointer slots therefore differ and are skipped,
 * and what they point at is compared by content.
 *
 * There are THREE observables for find_rev, not one: it returns a value, it
 * filters the caller's buffer in place on the way in, and it updates five
 * fields of the object.  All three are compared here.
 *
 * REACHING THE REVERSAL BRANCH IS THE WHOLE PROBLEM, exactly as it is for
 * FPM_TONE_generate higher up this file.  find_rev correlates the input
 * against itself delayed by `cfg.rev_lag` samples, which is 40, and declares a
 * reversal when that correlation falls below cfg.rev_thresh/2 of the windowed energy.
 * A tone whose period does not divide the lag correlates NEGATIVELY to begin
 * with, so the branch is either always or never taken and a reversal moves
 * nothing.  At 1800 Hz the lag is exactly nine cycles, so a steady tone
 * correlates positively and an inversion drives it negative -- which is what
 * makes the stimulus below a 1800 Hz tone rather than the config's own 2100.
 * The 2100 Hz stream is kept as the control for the always-taken case.
 *
 * The counters at the end are not decoration: each is a count of trials in
 * which a COMPARED quantity took a particular value, and between them they say
 * that the saturating and non-saturating arms of both accumulators, both arms
 * of the delay-line wrap, the debounce and the report all actually ran.
 */
static short rev_stim[3600];
static int rev_reports, rev_resets, rev_sat_pos, rev_sat_neg, rev_plain,
	   rev_esat, rev_idx_low, rev_idx_high, rev_filtered;
static int filt_moved, filt_wrapped;

static void
compare_rev(unsigned char *ours, unsigned char *ref, int tag)
{
	const short *ao = ((const struct fpm_tone *)ours)->rev_acc;
	const short *ar = ((const struct fpm_tone *)ref)->rev_acc;
	int i;

	for (i = 0; i < FPM_TONE_STATE_SIZE; i += 2) {
		if (is_pointer_slot(i) || is_pointer_slot(i - 2))
			continue;
		diff_eq_int("rev state 0x%02lx",
			    *(short *)(ours + i), *(short *)(ref + i), i);
	}
	/*
	 * The input biquad's own state, which lives on the heap and is the
	 * only part of find_rev's working set that is not inside the object.
	 */
	for (i = 0; i < 4; i++)
		diff_eq_int("rev_acc[%ld]", ao[i], ar[i], i);
	(void)tag;
}

/*
 * Fill `rev_stim` with `n` samples of a tone that reverses phase every
 * `period` eight-sample ticks.  Generated eight samples at a time because
 * FPM_TONE_generate advances its reversal counter by count>>3, so a shorter
 * call would never reach the reversal at all.
 */
static void
build_rev_stimulus(int freq, int scale, int period, int n)
{
	void *src = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
	int i;

	memset(rev_stim, 0, sizeof(rev_stim));
	if (src == 0)
		return;
	ref_FPM_TONE_set_freq(src, (short)freq);
	ref_FPM_TONE_set_scale(src, (short)scale);
	((struct fpm_tone *)src)->cfg.rev_period = (short)period;
	for (i = 0; i + 8 <= n && i + 8 <= (int)(sizeof(rev_stim) / sizeof(rev_stim[0])); i += 8)
		ref_FPM_TONE_generate(src, rev_stim + i, 8);
	ref_FPM_TONE_delete(src);
}

static int
find_rev_stream(const char *what, int total, int len)
{
	static short ba[1024], bb[1024];
	unsigned char *a, *b;
	int off, prev_age = 0;

	a = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
	b = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);

	diff_begin(what);
	if (a == 0 || b == 0) {
		diff_eq_int("objects built (%ld)", 0, 1, 0);
		return diff_end();
	}

	for (off = 0; off + len <= total; off += len) {
		struct fpm_tone *t = (struct fpm_tone *)a;
		short va, vb;
		int i;

		memcpy(ba, rev_stim + off, (unsigned)len * sizeof(short));
		memcpy(bb, rev_stim + off, (unsigned)len * sizeof(short));

		va = ref_FPM_TONE_find_rev(a, ba, (short)len);
		vb = FPM_TONE_find_rev((struct fpm_tone *)b, bb, (short)len);

		diff_eq_int("returned (%ld)", vb, va, off);
		for (i = 0; i < len; i++) {
			diff_eq_int("filtered sample %ld", bb[i], ba[i], i);
			if (ba[i] != rev_stim[off + i])
				rev_filtered++;
		}
		compare_rev(b, a, off);

		if (va != 0)
			rev_reports++;
		if (t->rev_age < prev_age)
			rev_resets++;
		prev_age = t->rev_age;
		if (t->rev_corr == 32767)
			rev_sat_pos++;
		else if (t->rev_corr == -32768)
			rev_sat_neg++;
		else
			rev_plain++;
		if (t->rev_energy == 32767 || t->rev_energy == -32768)
			rev_esat++;
		/*
		 * rev_idx below cfg.rev_lag is the only way the `j -= lag` step can
		 * go negative and take the wrapping arm, so the two counters
		 * together say both arms ran.  Exact only where a block is one
		 * sample long, which the fragmented stream below is.
		 */
		if (t->rev_idx < t->cfg.rev_lag)
			rev_idx_low++;
		else
			rev_idx_high++;
	}
	return diff_end();
}

/*
 * `taps_override` shortens cfg.len AFTER creation, so the object keeps its
 * full-length buffers while the correlator runs over fewer of them.  That is
 * the only way to put a REAL, non-zero value one place past the last tap the
 * loops are entitled to read: with taps at the config's own 53 the kernel ends
 * with the allocation, an off-by-one reads whatever the allocator left there,
 * and a mutation that walks one tap too far is invisible whenever that happens
 * to be zero.  It was: the "second loop covers the write position a second
 * time" mutation went UNCAUGHT until this argument existed.
 */
static int
filter_stream(const char *what, int total, int len, int taps_override)
{
	static short ba[1024], bb[1024];
	unsigned char *a, *b;
	int off, taps, prev_idx = 0;

	a = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
	b = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);

	diff_begin(what);
	if (a == 0 || b == 0) {
		diff_eq_int("objects built (%ld)", 0, 1, 0);
		return diff_end();
	}
	if (taps_override > 0) {
		((struct fpm_tone *)a)->cfg.len = (short)taps_override;
		((struct fpm_tone *)b)->cfg.len = (short)taps_override;
	}
	taps = ((struct fpm_tone *)a)->cfg.len;

	for (off = 0; off + len <= total; off += len) {
		int i;

		memcpy(ba, rev_stim + off, (unsigned)len * sizeof(short));
		memcpy(bb, rev_stim + off, (unsigned)len * sizeof(short));

		ref_FPM_TONE_filter(a, ba, (short)len);
		FPM_TONE_filter((struct fpm_tone *)b, bb, (short)len);

		for (i = 0; i < len; i++) {
			diff_eq_int("correlated sample %ld", bb[i], ba[i], i);
			if (ba[i] != rev_stim[off + i])
				filt_moved++;
		}
		compare_detect(b, a, taps, off);

		if (((struct fpm_tone *)a)->hist_idx < prev_idx)
			filt_wrapped++;
		prev_idx = ((struct fpm_tone *)a)->hist_idx;
	}
	return diff_end();
}

int
main(void)
{
	unsigned char a[FPM_TONE_STATE_SIZE], b[FPM_TONE_STATE_SIZE];
	static short oa[8192], ob[8192];
	void *built;
	int rc = 0;
	int k;

	/* One reference object, cloned for both sides. */
	built = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
	if (built == 0) {
		diff_begin("fpm_tone create");
		diff_eq_int("reference built an object (%ld)", 0, 1, 0);
		return diff_end();
	}

	/* Independent V.25 configuration/orbit checks.  These are not folded
	 * into the differential groups below: matching a blob is not a standards
	 * oracle. */
	rc |= t_fpm_v25_one("V.25 FPM_TONE_CFG/reconstruction", 0);
	rc |= t_fpm_v25_one("V.25 FPM_TONE_CFG/blob", 1);

	/* create: the whole object, buffers included. */
	diff_begin("FPM_TONE_create default cfg");
	{
		unsigned char *ca = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
		unsigned char *cb = (unsigned char *)FPM_TONE_create(0, (const struct fpm_tone_cfg *)ref_FPM_TONE_CFG);

		diff_eq_int("ours built an object (%ld)", cb != 0, 1, 0);
		if (cb != 0)
			compare_created(cb, ca,
					((struct fpm_tone *)ca)->cfg.len,
					((struct fpm_tone *)ca)->cfg.extra, 1);
	}
	rc |= diff_end();

	/* create with NULL cfg must fall back to the built-in configuration. */
	diff_begin("FPM_TONE_create NULL cfg");
	{
		unsigned char *ca = ref_FPM_TONE_create(0, 0);
		unsigned char *cb = (unsigned char *)FPM_TONE_create(0, 0);

		diff_eq_int("ours built an object (%ld)", cb != 0, 1, 0);
		if (cb != 0)
			compare_created(cb, ca,
					((struct fpm_tone *)ca)->cfg.len,
					((struct fpm_tone *)ca)->cfg.extra, 0);
	}
	rc |= diff_end();

	diff_begin("FPM_TONE_set_freq");
	for (k = 0; k < 4000; k += 7) {
		memcpy(a, built, sizeof(a));
		memcpy(b, built, sizeof(b));
		ref_FPM_TONE_set_freq((struct fpm_tone *)a, (short)k);
		FPM_TONE_set_freq((struct fpm_tone *)b, (short)k);
		compare_state(b, a, "set_freq");
	}
	rc |= diff_end();

	diff_begin("FPM_TONE_set_scale");
	for (k = -32768; k < 32768; k += 251) {
		memcpy(a, built, sizeof(a));
		memcpy(b, built, sizeof(b));
		ref_FPM_TONE_set_scale((struct fpm_tone *)a, (short)k);
		FPM_TONE_set_scale((struct fpm_tone *)b, (short)k);
		compare_state(b, a, "set_scale");
	}
	rc |= diff_end();

	/* Short bursts: exercises the sample loop, never the reversal. */
	diff_begin("FPM_TONE_generate short");
	memcpy(a, built, sizeof(a));
	memcpy(b, built, sizeof(b));
	ref_FPM_TONE_set_freq((struct fpm_tone *)a, 2100);
	FPM_TONE_set_freq((struct fpm_tone *)b, 2100);
	for (k = 0; k < 60; k++) {
		int n = (k % 13) + 1, i;

		ref_FPM_TONE_generate((struct fpm_tone *)a, oa, (short)n);
		FPM_TONE_generate((struct fpm_tone *)b, ob, (short)n);
		for (i = 0; i < n; i++)
			diff_eq_int("burst sample %ld", ob[i], oa[i], i);
		compare_state(b, a, "generate");
	}
	rc |= diff_end();

	/*
	 * Long run: 3600 samples per reversal at the default period, so this
	 * crosses several.  Chunked unevenly, because the counter advances by
	 * count>>3 per call -- so the *chunking* changes when reversals land,
	 * and a reconstruction that accumulated differently would diverge here
	 * and nowhere else.
	 */
	diff_begin("FPM_TONE_generate reversals");
	memcpy(a, built, sizeof(a));
	memcpy(b, built, sizeof(b));
	ref_FPM_TONE_set_freq((struct fpm_tone *)a, 2100);
	FPM_TONE_set_freq((struct fpm_tone *)b, 2100);
	{
		int resets = 0, prev = 0;

		for (k = 0; k < 400; k++) {
			int n = 40 + (k % 7) * 24, i, now;

			ref_FPM_TONE_generate((struct fpm_tone *)a, oa, (short)n);
			FPM_TONE_generate((struct fpm_tone *)b, ob, (short)n);
			for (i = 0; i < n; i++)
				diff_eq_int("long sample %ld", ob[i], oa[i], i);
			compare_state(b, a, "generate long");

			now = ((struct fpm_tone *)b)->rev_count;
			if (now < prev)
				resets++;
			prev = now;
		}

		/*
		 * Guard against this group going vacuous.  It takes 3600
		 * samples per reversal at the default period, so if the run
		 * length or the period ever changes, the reversal branch could
		 * stop being reached and every check above would still pass
		 * while testing nothing.  Assert it actually fired.
		 */
		diff_eq_int("reversals actually fired (%ld)", resets > 0, 1, 0);
	}
	rc |= diff_end();

	/* Counts that are not multiples of 8: the counter drops the remainder. */
	diff_begin("FPM_TONE_generate ragged counts");
	memcpy(a, built, sizeof(a));
	memcpy(b, built, sizeof(b));
	ref_FPM_TONE_set_freq((struct fpm_tone *)a, 1650);
	FPM_TONE_set_freq((struct fpm_tone *)b, 1650);
	for (k = 0; k < 900; k++) {
		int n = (k % 8) + 1, i;

		ref_FPM_TONE_generate((struct fpm_tone *)a, oa, (short)n);
		FPM_TONE_generate((struct fpm_tone *)b, ob, (short)n);
		for (i = 0; i < n; i++)
			diff_eq_int("ragged sample %ld", ob[i], oa[i], i);
		compare_state(b, a, "generate ragged");
	}
	rc |= diff_end();



	/*
	 * FPM_TONE_generate_demod: cosine, no reversals, returns count.
	 *
	 * Run long enough to pass several reversal periods -- 3600 samples
	 * apiece -- because the thing worth proving is that the reversal
	 * bookkeeping is ABSENT.  A short burst cannot tell the two
	 * generators apart.
	 */
	diff_begin("FPM_TONE_generate_demod");
	memcpy(a, built, sizeof(a));
	memcpy(b, built, sizeof(b));
	ref_FPM_TONE_set_freq((struct fpm_tone *)a, 2100);
	FPM_TONE_set_freq((struct fpm_tone *)b, 2100);
	ref_FPM_TONE_set_scale((struct fpm_tone *)a, 32767);
	FPM_TONE_set_scale((struct fpm_tone *)b, 32767);
	for (k = 0; k < 400; k++) {
		int n = (k % 37) + 1, i;
		short ra, rb;

		ra = ref_FPM_TONE_generate_demod((struct fpm_tone *)a, oa, (short)n);
		rb = FPM_TONE_generate_demod((struct fpm_tone *)b, ob, (short)n);

		diff_eq_int("returned count (%ld)", rb, ra, n);
		for (i = 0; i < n; i++)
			diff_eq_int("demod sample %ld", ob[i], oa[i], i);
		compare_state(b, a, "generate_demod");
	}
	rc |= diff_end();

	/* Zero count writes the phase back unchanged and returns zero. */
	diff_begin("FPM_TONE_generate_demod zero count");
	{
		short ra = ref_FPM_TONE_generate_demod((struct fpm_tone *)a, oa, 0);
		short rb = FPM_TONE_generate_demod((struct fpm_tone *)b, ob, 0);

		diff_eq_int("returned (%ld)", rb, ra, 0);
		compare_state(b, a, "generate_demod zero");
	}
	rc |= diff_end();

	/*
	 * And that it really is the cosine of the same oscillator: after the
	 * same number of samples from the same start, generate_demod's phase
	 * must match generate's minus whatever the reversal moved.  Compare
	 * against a fresh pair well inside one reversal period.
	 */
	diff_begin("generate_demod is generate's cosine");
	{
		unsigned char ga[FPM_TONE_STATE_SIZE], gb[FPM_TONE_STATE_SIZE];
		int i;

		memcpy(ga, built, sizeof(ga));
		memcpy(gb, built, sizeof(gb));
		ref_FPM_TONE_set_freq(ga, 2100);
		ref_FPM_TONE_set_freq(gb, 2100);
		ref_FPM_TONE_set_scale(ga, 32767);
		ref_FPM_TONE_set_scale(gb, 32767);

		FPM_TONE_generate((struct fpm_tone *)ga, oa, 1000);
		FPM_TONE_generate_demod((struct fpm_tone *)gb, ob, 1000);

		/* Same phase advance, so the accumulators agree. */
		diff_eq_int("phase agrees (%ld)",
			    ((struct fpm_tone *)gb)->phase,
			    ((struct fpm_tone *)ga)->phase, 0);
		/* And the two waveforms are a quarter cycle apart, not equal. */
		for (i = 0, k = 0; i < 1000; i++)
			if (oa[i] != ob[i])
				k++;
		diff_eq_int("waveforms differ (%ld)", k > 900, 1, k);
	}
	rc |= diff_end();

	/*
	 * FPM_TONE_generate2: the quadrature pair from one oscillator.
	 *
	 * Two things need proving beyond sample equality.  First that the two
	 * buffers really are cosine and sine of the SAME phase and not two
	 * copies of one of them -- so the cosine half is checked against
	 * FPM_TONE_generate_demod and the sine half against FPM_TONE_generate,
	 * from an identical starting state.  Second that the phase advances
	 * once per sample and not twice, which is what a naive two-call
	 * implementation would do; the phase comparison against the other two
	 * generators covers that.
	 */
	diff_begin("FPM_TONE_generate2");
	{
		static short ca[8192], sa[8192], cb[8192], sb[8192];
		int i;

		memcpy(a, built, sizeof(a));
		memcpy(b, built, sizeof(b));
		ref_FPM_TONE_set_freq((struct fpm_tone *)a, 1800);
		FPM_TONE_set_freq((struct fpm_tone *)b, 1800);
		ref_FPM_TONE_set_scale((struct fpm_tone *)a, 32767);
		FPM_TONE_set_scale((struct fpm_tone *)b, 32767);

		for (k = 0; k < 400; k++) {
			int n = (k % 37) + 1;
			short ra, rb;

			ra = ref_FPM_TONE_generate2((struct fpm_tone *)a,
						    ca, sa, (short)n);
			rb = FPM_TONE_generate2((struct fpm_tone *)b,
						cb, sb, (short)n);

			diff_eq_int("returned count (%ld)", rb, ra, n);
			for (i = 0; i < n; i++) {
				diff_eq_int("cos sample %ld", cb[i], ca[i], i);
				diff_eq_int("sin sample %ld", sb[i], sa[i], i);
			}
			compare_state(b, a, "generate2");
		}

		/* Ragged scale settings, including negative and zero gain. */
		for (k = -32768; k < 32768; k += 997) {
			ref_FPM_TONE_set_scale((struct fpm_tone *)a, (short)k);
			FPM_TONE_set_scale((struct fpm_tone *)b, (short)k);
			ref_FPM_TONE_generate2((struct fpm_tone *)a,
					       ca, sa, 17);
			FPM_TONE_generate2((struct fpm_tone *)b, cb, sb, 17);
			for (i = 0; i < 17; i++) {
				diff_eq_int("scaled cos %ld", cb[i], ca[i], i);
				diff_eq_int("scaled sin %ld", sb[i], sa[i], i);
			}
			compare_state(b, a, "generate2 scaled");
		}
	}
	rc |= diff_end();

	/* Zero count writes the phase back unchanged and returns zero. */
	diff_begin("FPM_TONE_generate2 zero count");
	{
		static short ca[8], sa[8], cb[8], sb[8];
		short ra = ref_FPM_TONE_generate2((struct fpm_tone *)a,
						  ca, sa, 0);
		short rb = FPM_TONE_generate2((struct fpm_tone *)b,
					      cb, sb, 0);

		diff_eq_int("returned (%ld)", rb, ra, 0);
		compare_state(b, a, "generate2 zero");
	}
	rc |= diff_end();

	/*
	 * A NEGATIVE count, which is the only input that can tell the loop's
	 * real shape apart from `for (i = 0; i < count; i++)`.  The counter is
	 * 16-bit and the exit test is `!= -1`, so a count of -1 starts at -2
	 * and wraps the whole way round: 65535 samples out of a call that asks
	 * for none.  The buffers are sized for that, and sized the same under
	 * any misreading of the loop, because they have to survive being wrong.
	 */
	diff_begin("FPM_TONE_generate2 negative count");
	{
		static short ca[65600], sa[65600], cb[65600], sb[65600];
		int i;
		short ra, rb;

		memcpy(a, built, sizeof(a));
		memcpy(b, built, sizeof(b));
		ref_FPM_TONE_set_freq((struct fpm_tone *)a, 1800);
		FPM_TONE_set_freq((struct fpm_tone *)b, 1800);
		ref_FPM_TONE_set_scale((struct fpm_tone *)a, 32767);
		FPM_TONE_set_scale((struct fpm_tone *)b, 32767);

		ra = ref_FPM_TONE_generate2((struct fpm_tone *)a, ca, sa, -1);
		rb = FPM_TONE_generate2((struct fpm_tone *)b, cb, sb, -1);

		diff_eq_int("returned (%ld)", rb, ra, -1);
		for (i = 0; i < 65535; i++) {
			diff_eq_int("wrapped cos %ld", cb[i], ca[i], i);
			diff_eq_int("wrapped sin %ld", sb[i], sa[i], i);
		}
		compare_state(b, a, "generate2 negative");
	}
	rc |= diff_end();

	/*
	 * And that it is the quadrature pair of the same oscillator: from one
	 * starting state, generate2's cosine is generate_demod's output and
	 * its sine is generate's, sample for sample, with all three leaving
	 * the accumulator at the same phase.  Well inside one reversal period,
	 * so generate's extra bookkeeping does not enter.
	 */
	diff_begin("FPM_TONE_generate2 is the quadrature pair");
	{
		unsigned char qa[FPM_TONE_STATE_SIZE];
		unsigned char qb[FPM_TONE_STATE_SIZE];
		unsigned char qc[FPM_TONE_STATE_SIZE];
		static short cq[2048], sq[2048];
		int i;

		memcpy(qa, built, sizeof(qa));
		memcpy(qb, built, sizeof(qb));
		memcpy(qc, built, sizeof(qc));
		for (i = 0; i < 3; i++) {
			unsigned char *p = i == 0 ? qa : (i == 1 ? qb : qc);

			ref_FPM_TONE_set_freq((struct fpm_tone *)p, 1800);
			ref_FPM_TONE_set_scale((struct fpm_tone *)p, 32767);
		}

		FPM_TONE_generate2((struct fpm_tone *)qa, cq, sq, 1000);
		FPM_TONE_generate_demod((struct fpm_tone *)qb, oa, 1000);
		FPM_TONE_generate((struct fpm_tone *)qc, ob, 1000);

		for (i = 0; i < 1000; i++) {
			diff_eq_int("cos half is generate_demod at %ld",
				    cq[i], oa[i], i);
			diff_eq_int("sin half is generate at %ld",
				    sq[i], ob[i], i);
		}
		diff_eq_int("phase matches generate_demod (%ld)",
			    ((struct fpm_tone *)qa)->phase,
			    ((struct fpm_tone *)qb)->phase, 0);
		diff_eq_int("phase matches generate (%ld)",
			    ((struct fpm_tone *)qa)->phase,
			    ((struct fpm_tone *)qc)->phase, 0);
	}
	rc |= diff_end();

	/* --- FPM_TONE_detect -------------------------------------------- */

	/*
	 * Stimulus comes from a third tone object, so the in-band case really
	 * is the frequency the detector was configured for rather than
	 * something merely nearby.
	 */
	{
		void *src = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);

		if (src == 0) {
			diff_begin("FPM_TONE_detect stimulus");
			diff_eq_int("stimulus object built (%ld)", 0, 1, 0);
			rc |= diff_end();
		} else {
			/* On frequency: should settle to PRESENT. */
			rc |= detect_stream("detect 2100 Hz", src, 2100,
					    32767, 40, 80);
			/* Off frequency: energy, but not this tone. */
			rc |= detect_stream("detect 1650 Hz", src, 1650,
					    32767, 40, 80);
			rc |= detect_stream("detect 400 Hz", src, 400,
					    32767, 20, 80);
			/* Too quiet: below the minimum level. */
			rc |= detect_stream("detect quiet", src, 2100,
					    2, 20, 80);
			/* Fragmentation: the history index must carry. */
			rc |= detect_stream("detect frag 1", src, 2100,
					    32767, 300, 1);
			rc |= detect_stream("detect frag 7", src, 2100,
					    32767, 200, 7);
			rc |= detect_stream("detect frag 160", src, 2100,
					    32767, 30, 160);
			/* Longer than the correlator, so the history wraps
			 * several times inside a single call. */
			rc |= detect_stream("detect frag 500", src, 2100,
					    32767, 10, 500);
		}
	}

	/* A zero-length call re-evaluates the verdict and changes nothing else. */
	diff_begin("FPM_TONE_detect zero count");
	{
		unsigned char *za = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
		unsigned char *zb = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
		short dummy = 0;

		if (za != 0 && zb != 0) {
			short va = ref_FPM_TONE_detect(za, &dummy, 0);
			short vb = FPM_TONE_detect((struct fpm_tone *)zb, &dummy, 0);

			diff_eq_int("verdict (%ld)", vb, va, 0);
			compare_detect(zb, za, 0, 0);
		}
	}
	rc |= diff_end();

	/*
	 * Anti-vacuity: all three verdicts must have been produced.  A
	 * detector stuck on any single answer would otherwise agree with a
	 * reconstruction that was stuck on the same one.
	 */
	diff_begin("FPM_TONE_detect coverage");
	diff_eq_int("ABSENT seen (%ld)", detect_verdicts[0] > 0, 1,
		    detect_verdicts[0]);
	diff_eq_int("PRESENT seen (%ld)", detect_verdicts[1] > 0, 1,
		    detect_verdicts[1]);
	diff_eq_int("NOSIGNAL seen (%ld)", detect_verdicts[2] > 0, 1,
		    detect_verdicts[2]);
	rc |= diff_end();

	/*
	 * FPM_TONE_find_rev.  One stimulus, six chunkings, because the 32-bit
	 * accumulators are re-seeded from their SATURATED shorts on every
	 * entry: once either sum leaves +-32767, N one-sample calls stop being
	 * the same thing as one N-sample call, and only running both says so.
	 */
	build_rev_stimulus(1800, 27852, 30, 3200);
	rc |= find_rev_stream("find_rev 1800 Hz blocks of 40", 3200, 40);
	rc |= find_rev_stream("find_rev 1800 Hz one at a time", 1600, 1);
	rc |= find_rev_stream("find_rev 1800 Hz blocks of 7", 3200, 7);
	rc |= find_rev_stream("find_rev 1800 Hz blocks of 500", 3000, 500);

	/*
	 * A level low enough that neither accumulator reaches its rail, so the
	 * plain `(short)sum` arm of both clamps is the one taken.
	 */
	build_rev_stimulus(1800, 600, 30, 3200);
	rc |= find_rev_stream("find_rev low level", 3200, 13);

	/*
	 * The control: the config's own 2100 Hz, where the lag is ten and a
	 * half cycles and the steady tone correlates NEGATIVELY, so the
	 * reversal test is satisfied continuously and the debounce -- not the
	 * signal -- is what decides when a report comes out.
	 */
	build_rev_stimulus(2100, 27852, 450, 3200);
	rc |= find_rev_stream("find_rev 2100 Hz", 3200, 32);

	/* Silence: correlation and energy both zero, so nothing is ever
	 * reported and rev_age simply runs. */
	memset(rev_stim, 0, sizeof(rev_stim));
	rc |= find_rev_stream("find_rev silence", 1600, 64);

	/*
	 * Zero and negative counts.  The input filter is called BEFORE the
	 * count is looked at, and the three state words are written back
	 * whether the loop runs or not, so both are observable rather than
	 * vacuous.  A negative count does nothing here -- the opposite of
	 * FPM_TONE_filter below, which runs about 65536 times.
	 */
	diff_begin("FPM_TONE_find_rev zero and negative count");
	{
		unsigned char *za = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
		unsigned char *zb = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
		short da = 1234, db = 1234;

		diff_eq_int("objects built (%ld)", za != 0 && zb != 0, 1, 0);
		if (za != 0 && zb != 0) {
			short va = ref_FPM_TONE_find_rev(za, &da, 0);
			short vb = FPM_TONE_find_rev((struct fpm_tone *)zb,
						     &db, 0);

			diff_eq_int("zero count returned (%ld)", vb, va, 0);
			diff_eq_int("zero count sample (%ld)", db, da, 0);
			compare_rev(zb, za, 0);

			va = ref_FPM_TONE_find_rev(za, &da, -1);
			vb = FPM_TONE_find_rev((struct fpm_tone *)zb, &db, -1);

			diff_eq_int("negative count returned (%ld)", vb, va, -1);
			diff_eq_int("negative count sample (%ld)", db, da, -1);
			compare_rev(zb, za, -1);
		}
	}
	rc |= diff_end();

	/*
	 * Anti-vacuity.  Every line is a count of trials in which a COMPARED
	 * value took a particular form, so a find_rev that never reported, or
	 * never railed, or never wrapped its delay line would fail here while
	 * still agreeing sample for sample with a reconstruction that did the
	 * same nothing.
	 */
	diff_begin("FPM_TONE_find_rev coverage");
	diff_eq_int("reversals reported (%ld)", rev_reports > 0, 1,
		    rev_reports);
	diff_eq_int("rev_age reset (%ld)", rev_resets > 0, 1, rev_resets);
	diff_eq_int("correlation railed high (%ld)", rev_sat_pos > 0, 1,
		    rev_sat_pos);
	diff_eq_int("correlation railed low (%ld)", rev_sat_neg > 0, 1,
		    rev_sat_neg);
	diff_eq_int("correlation off the rails (%ld)", rev_plain > 0, 1,
		    rev_plain);
	diff_eq_int("energy railed (%ld)", rev_esat > 0, 1, rev_esat);
	diff_eq_int("delay index below the lag (%ld)", rev_idx_low > 0, 1,
		    rev_idx_low);
	diff_eq_int("delay index at or above it (%ld)", rev_idx_high > 0, 1,
		    rev_idx_high);
	diff_eq_int("samples the input filter moved (%ld)", rev_filtered > 0,
		    1, rev_filtered);
	rc |= diff_end();

	/*
	 * FPM_TONE_filter -- the detector's correlator alone, in place.
	 *
	 * The observables are the buffer, the shared write index at +0x34 and
	 * the history ring on the heap, and all three are compared.  Chunked
	 * several ways because the correlator restarts its two loops from
	 * whatever index the previous call left behind.
	 */
	build_rev_stimulus(2100, 27852, 450, 3200);
	rc |= filter_stream("filter blocks of 53", 3180, 53, 0);
	rc |= filter_stream("filter one at a time", 800, 1, 0);
	rc |= filter_stream("filter blocks of 7", 3199, 7, 0);
	rc |= filter_stream("filter blocks of 500", 3000, 500, 0);
	/* Taps short of the buffer, so the tap past the end is a real one. */
	rc |= filter_stream("filter with 40 of 53 taps", 3200, 25, 40);

	diff_begin("FPM_TONE_filter coverage");
	diff_eq_int("samples the correlator moved (%ld)", filt_moved > 0, 1,
		    filt_moved);
	diff_eq_int("history ring wrapped (%ld)", filt_wrapped > 0, 1,
		    filt_wrapped);
	rc |= diff_end();

	/*
	 * The claim that filter and detect SHARE `hist_idx` rather than each
	 * keeping its own, which is the one thing about this function that a
	 * reader would guess wrongly.  Interleaving the two on one object is
	 * what separates the readings: with a private index the two would walk
	 * different positions and every sample after the first would differ.
	 */
	diff_begin("FPM_TONE_filter shares the detector's index");
	{
		unsigned char *ia = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
		unsigned char *ib = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
		static short fa[64], fb[64];
		int taps, off, moved = 0;

		diff_eq_int("objects built (%ld)", ia != 0 && ib != 0, 1, 0);
		if (ia != 0 && ib != 0) {
			taps = ((struct fpm_tone *)ia)->cfg.len;
			for (off = 0; off + 16 <= 1600; off += 16) {
				short va, vb;
				int i;

				memcpy(fa, rev_stim + off, 16 * sizeof(short));
				memcpy(fb, rev_stim + off, 16 * sizeof(short));

				ref_FPM_TONE_filter(ia, fa, 16);
				FPM_TONE_filter((struct fpm_tone *)ib, fb, 16);
				for (i = 0; i < 16; i++)
					diff_eq_int("interleaved sample %ld",
						    fb[i], fa[i], i);

				va = ref_FPM_TONE_detect(ia, rev_stim + off, 16);
				vb = FPM_TONE_detect((struct fpm_tone *)ib,
						     rev_stim + off, 16);
				diff_eq_int("interleaved verdict (%ld)", vb, va,
					    off);
				compare_detect(ib, ia, taps, off);

				if (((struct fpm_tone *)ia)->hist_idx
				    != ((struct fpm_tone *)ia)->cfg.len - 1)
					moved++;
			}
			diff_eq_int("the shared index moved (%ld)", moved > 0,
				    1, moved);
		}
	}
	rc |= diff_end();

	/*
	 * Negative count: 16-bit and tested against -1, so it runs about
	 * 65536 times rather than doing nothing.  Same shape as
	 * FPM_TONE_generate2's above, and the opposite of FPM_TONE_find_rev's.
	 */
	diff_begin("FPM_TONE_filter negative count");
	{
		static short na[65600], nb[65600];
		unsigned char *fa = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
		unsigned char *fb = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
		int i;

		diff_eq_int("objects built (%ld)", fa != 0 && fb != 0, 1, 0);
		if (fa != 0 && fb != 0) {
			for (i = 0; i < 65536; i++)
				na[i] = nb[i] = (short)
					(11000 * ((i & 1) ? -1 : 1) + (i & 511));

			ref_FPM_TONE_filter(fa, na, -1);
			FPM_TONE_filter((struct fpm_tone *)fb, nb, -1);

			for (i = 0; i < 65535; i++)
				diff_eq_int("wrapped sample %ld", nb[i], na[i],
					    i);
			compare_detect(fb, fa,
				       ((struct fpm_tone *)fa)->cfg.len, -1);
		}
	}
	rc |= diff_end();

	/*
	 * FPM_TONE_kill -- the notch run over the caller's buffer, in place.
	 *
	 * The observables are the BUFFER, which is filtered in place, and the
	 * whole object, which carries the filter's four-word state at +0x100.
	 * Both are compared, so a pass that read the wrong state words or wrote
	 * the wrong ones is visible even where the samples happen to agree.
	 *
	 * `kill_coeffs` is the point of the second pass and it is not
	 * decoration.  The object stores a pointer to its own coefficients at
	 * +0xfc, and the blob LOADS it (`mov 0xfc(%edx),%ecx`) where a
	 * reference to the `iir_coeff` array would have been a `lea`.  Those
	 * two readings agree for ever on an object built by FPM_TONE_create,
	 * because create sets the pointer to exactly that address -- so the
	 * only way to separate them is to point +0xfc somewhere else and see
	 * which set of coefficients comes out.  Pass 1 does that.
	 */
	diff_begin("FPM_TONE_kill");
	{
		/*
		 * A one-pole-ish section with an obviously different response
		 * from the 2100 Hz notch, in FPM_iir_filt_II's own coefficient
		 * order { b0, b2, b1, a2, a1 }.
		 */
		static short kill_coeffs[5] = { 8192, -4096, 2048, 1024, -3072 };
		int pass, changed = 0, stateful = 0, separated = 0;

		for (pass = 0; pass < 2; pass++) {
			for (k = 0; k < 40; k++) {
				int n = (k % 17) + 1, i;
				short ka[64], kb[64];

				memcpy(a, built, sizeof(a));
				memcpy(b, built, sizeof(b));
				/*
				 * Pass 0 leaves +0xfc as the memcpy left it,
				 * pointing at `built`'s own taps -- the same
				 * address on both sides, so the whole-object
				 * comparison still holds.  Pass 1 redirects
				 * both sides to a different filter, which is
				 * what separates the stored pointer from the
				 * array it usually equals.
				 */
				if (pass) {
					((struct fpm_tone *)a)->iir_self =
						kill_coeffs;
					((struct fpm_tone *)b)->iir_self =
						kill_coeffs;
				}
				/*
				 * A non-zero starting state, so reading the
				 * detector's `iir_state` at +0x40 instead of
				 * +0x100 separates on the first sample rather
				 * than only after the history has filled.
				 */
				for (i = 0; i < 4; i++) {
					((struct fpm_tone *)a)->kill_state[i] =
					((struct fpm_tone *)b)->kill_state[i] =
						(short)(1000 * (i + 1) - 2500);
					((struct fpm_tone *)a)->iir_state[i] =
					((struct fpm_tone *)b)->iir_state[i] =
						(short)(-700 * (i + 1));
				}

				for (i = 0; i < n; i++) {
					/* A 2100 Hz tone plus a slow ramp. */
					ka[i] = kb[i] = (short)
						(9000 * ((i & 1) ? -1 : 1)
						 + 40 * (k + i));
				}

				ref_FPM_TONE_kill(a, ka, (short)n);
				FPM_TONE_kill((struct fpm_tone *)b, kb,
					      (short)n);

				for (i = 0; i < n; i++) {
					diff_eq_int("kill sample %ld", kb[i],
						    ka[i], i);
					/*
					 * Observable-difference counters: a
					 * filtered sample that differs from
					 * its input, and a run whose two
					 * coefficient sets disagree.  Both
					 * count COMPARED bytes, not a path.
					 */
					if (ka[i] != (short)
					    (9000 * ((i & 1) ? -1 : 1)
					     + 40 * (k + i)))
						changed++;
					if (pass == 1 && kill_pass0[k][i]
					    != ka[i])
						separated++;
					if (pass == 0)
						kill_pass0[k][i] = ka[i];
				}
				compare_state(b, a, "kill");
				if (((struct fpm_tone *)a)->kill_state[0]
				    != (short)-1500)
					stateful++;
			}
		}

		/*
		 * Anti-vacuity.  Every one of these is a count of trials whose
		 * COMPARED result differed -- an output sample that the filter
		 * moved, an object byte the filter wrote, and a pair of runs
		 * that the two coefficient sets drove apart.  A kill that did
		 * nothing, that kept no state, or that ignored +0xfc would fail
		 * one of the three while still agreeing sample for sample with
		 * a reconstruction that did the same nothing.
		 */
		diff_eq_int("samples the filter moved (%ld)", changed > 0, 1,
			    changed);
		diff_eq_int("trials that wrote kill_state (%ld)",
			    stateful > 0, 1, stateful);
		diff_eq_int("samples separating +0xfc from iir_coeff (%ld)",
			    separated > 0, 1, separated);
	}
	rc |= diff_end();

	/*
	 * Destruction, measured by the allocator rather than by looking at
	 * freed memory.  Both branches: a detector whose `cfg.len` is
	 * positive owns four buffers as well as itself, one whose is not owns
	 * only itself, and the number of frees is the whole behaviour.
	 *
	 * The second branch is reached by clearing `len` on a built object
	 * rather than by configuring one that way -- `FPM_TONE_create` faults
	 * on a zero length, so a zero-length object is not something it can
	 * produce.  The four buffers are then deliberately leaked, equally on
	 * both sides, which is what the branch does.
	 */
	diff_begin("FPM_TONE_delete");
	{
		int pass;

		for (pass = 0; pass < 2; pass++) {
			struct fpm_tone *ra, *rb;
			int fa, fb, la, lb;

			harness_alloc_reset();
			ra = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
			rb = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
			diff_eq_int("both built (%ld)", ra != 0 && rb != 0, 1,
				    pass);
			if (ra == 0 || rb == 0)
				continue;
			if (pass == 1)
				ra->cfg.len = rb->cfg.len = 0;

			la = harness_alloc.live;
			fa = harness_alloc.frees;
			ref_FPM_TONE_delete(ra);
			fa = harness_alloc.frees - fa;
			la = la - harness_alloc.live;

			lb = harness_alloc.live;
			fb = harness_alloc.frees;
			FPM_TONE_delete(rb);
			fb = harness_alloc.frees - fb;
			lb = lb - harness_alloc.live;

			diff_eq_int("frees (%ld)", fb, fa, pass);
			diff_eq_int("allocations released (%ld)", lb, la,
				    pass);
			diff_eq_int("nothing unknown was freed (%ld)",
				    harness_alloc.bad_free, 0, pass);
			/*
			 * Anti-vacuity, and the difference between the two
			 * branches: five frees with a kernel, one without.
			 */
			diff_eq_int("frees for this branch (%ld)", fa,
				    pass == 0 ? 5 : 1, pass);
		}
		harness_alloc_reset();
	}
	rc |= diff_end();

	printf("t_fpm_tone: verdicts absent/present/nosignal %d/%d/%d\n",
	       detect_verdicts[0], detect_verdicts[1], detect_verdicts[2]);
	printf("t_fpm_tone: find_rev reports %d, resets %d, corr rails %d/%d "
	       "plain %d, energy rails %d\n",
	       rev_reports, rev_resets, rev_sat_pos, rev_sat_neg, rev_plain,
	       rev_esat);

	return rc;
}

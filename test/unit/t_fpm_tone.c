/*
 * t_fpm_tone.c -- differential test of the tone generator.
 *
 * FPM_TONE_create is not reconstructed yet, so objects are built with the
 * *reference* implementation and then copied: both sides operate on
 * byte-identical starting state.  That isolates the functions under test from
 * the one that is still pending, and it is stronger than it sounds -- the
 * whole 0x108-byte object is compared afterwards, so a write to any field,
 * named or not, shows up.
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

	return rc;
}

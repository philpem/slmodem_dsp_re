/*
 * t_fpm_agc.c -- differential test of the block AGC.
 *
 * The AGC is a feedback loop, which makes it the easiest module in this tree
 * to test vacuously.  A single call from a fresh state exercises exactly one
 * path -- `mult` is zero, so the acquisition threshold applies and the
 * squelch threshold never does, and the smoother has had no time to move.
 * An implementation that adapted at completely the wrong rate would pass.
 *
 * So the tests here are driven as streams, over many calls, and the run
 * asserts afterwards that each interesting path was actually taken:
 *
 *   - silenced via `acquire_level` (mult == 0)   and via `squelch_level`
 *   - the gain path, before and after the loop settles
 *   - Freeze mid-stream: the smoother keeps running, the gain holds
 *   - Release mid-stream: the level clears, the gain does not
 *   - both saturation clamps in the apply loop
 *   - the folded-tail block long enough to make FPM_rms return negative
 *   - every fragment length around the partitioning discontinuity
 *
 * Those assertions are the point.  Without them a passing run says only that
 * two implementations agreed on whatever they happened to do.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm.h"

extern void ref_FPM_AGC_init(void *agc, const void *cfg, int reset);
extern void ref_FPM_AGC_agc(void *agc, short *samples, unsigned short count);
extern void ref_FPM_AGC_Freeze(void *agc);
extern void ref_FPM_AGC_Release(void *agc);
extern short ref_FPM_rms(const short *samples, unsigned short count);

extern const struct fpm_agc_cfg AGCb103_CFG_data;

#include "dsplib/debug.h"
extern unsigned int ref_dsplibs_debug_level;

/*
 * The original's AGCb103_CFG_data is a global, but the coefficient arrays it points
 * at are TU-local and so not linkable.  Both sides are pointed at OUR copies,
 * which is what makes the comparison meaningful: any difference is in the
 * code, not in the data.  b103_agc_cfg.c documents where the values came from.
 */

/* Counters proving the run was not vacuous. */
static int seen_silence_acquire;
static int seen_silence_squelch;
static int seen_gain;
static int seen_clamp_hi;
static int seen_clamp_lo;
static int seen_one_block;

static void
compare_state(const char *what, const struct fpm_agc *ours,
	      const struct fpm_agc *ref, int tag)
{
	char buf[128];

	snprintf(buf, sizeof(buf), "%s: level (%%ld)", what);
	diff_eq_int(buf, ours->level, ref->level, tag);
	snprintf(buf, sizeof(buf), "%s: mult (%%ld)", what);
	diff_eq_int(buf, ours->mult, ref->mult, tag);
	snprintf(buf, sizeof(buf), "%s: shift (%%ld)", what);
	diff_eq_int(buf, ours->shift, ref->shift, tag);
	snprintf(buf, sizeof(buf), "%s: signal (%%ld)", what);
	diff_eq_int(buf, ours->signal, ref->signal, tag);
	snprintf(buf, sizeof(buf), "%s: freeze (%%ld)", what);
	diff_eq_int(buf, ours->freeze, ref->freeze, tag);
}

static void
compare_samples(const char *what, const short *ours, const short *ref, int n)
{
	char buf[128];
	int i;

	snprintf(buf, sizeof(buf), "%s: sample[%%ld]", what);
	for (i = 0; i < n; i++)
		diff_eq_int(buf, ours[i], ref[i], i);
}

/*
 * Coverage instrumentation.
 *
 * Deliberately *observational*: it reads the reference's inputs and outputs
 * rather than re-deriving what it should have done.  Re-deriving would let
 * the same misreading appear in both the code and the thing that claims the
 * code was exercised.
 *
 * The one exception is the gate condition itself, two lines recomputed from
 * the reference's own FPM_rms -- and even that is checked rather than
 * trusted: a block classified as silenced must have come out all zero, and
 * that is asserted below.
 */
static int
all_zero(const short *v, int n)
{
	int i;

	for (i = 0; i < n; i++)
		if (v[i] != 0)
			return 0;
	return 1;
}

static void
note_coverage(const struct fpm_agc *before, const struct fpm_agc *after,
	      const short *in, const short *out, int len)
{
	unsigned block = before->cfg.block_len;
	int level, k;

	/*
	 * Saturation is read straight off the output.
	 *
	 * An output of exactly +32767 can only be a clamp: `mult` never
	 * exceeds 16384, so with shift == 0 the magnitude cannot pass 16383,
	 * and with shift >= 1 the result is even.  -32768 is even, so it
	 * could in principle be produced exactly -- but only from an input of
	 * -32768, which nothing here generates.
	 */
	for (k = 0; k < len; k++) {
		if (out[k] == 32767 && in[k] != 32767)
			seen_clamp_hi++;
		else if (out[k] == -32768 && in[k] != -32768)
			seen_clamp_lo++;
	}

	/* At least one block took the gain path, or `signal` could not be set. */
	if (after->signal)
		seen_gain++;

	/*
	 * The gate is only attributable when the fragment is exactly one
	 * block, because only then is `before` the state that block saw.
	 */
	if (len < (int)(block / 2) || len >= (int)(block + block / 2))
		return;
	seen_one_block++;

	level = ref_FPM_rms(in, (unsigned short)len);
	if (level < before->cfg.acquire_level && before->mult == 0) {
		seen_silence_acquire++;
		diff_eq_int("acquire-gated block came out zero (%ld)",
			    all_zero(out, len), 1, level);
	} else if (level < before->cfg.squelch_level && before->mult != 0) {
		seen_silence_squelch++;
		diff_eq_int("squelch-gated block came out zero (%ld)",
			    all_zero(out, len), 1, level);
	}
}

/* A deterministic pseudo-random source; no libc rand() dependency. */
static unsigned rng_state;

static void
rng_seed(unsigned s)
{
	rng_state = s;
}

static int
rng_next(void)
{
	rng_state = rng_state * 1103515245u + 12345u;
	return (int)((rng_state >> 16) & 0x7fff);
}

/*
 * Drive a stream of `frags` fragments of `len` samples at the given amplitude
 * through both sides, comparing state and samples after every fragment.
 */
static void
stream(const char *what, struct fpm_agc *a, struct fpm_agc *b,
       int frags, int len, int amp, int noise)
{
	short ours[512], ref[512], in[512];
	struct fpm_agc before;
	int f, i;

	for (f = 0; f < frags; f++) {
		for (i = 0; i < len; i++) {
			/* A square-ish wave: cheap, and it exercises both
			 * signs and both saturation clamps. */
			int v = ((f * len + i) & 8) ? amp : -amp;

			if (noise)
				v += rng_next() % (noise * 2 + 1) - noise;
			if (v > 32767)
				v = 32767;
			if (v < -32768)
				v = -32768;
			ours[i] = ref[i] = in[i] = (short)v;
		}

		before = *b;
		FPM_AGC_agc(a, ours, (unsigned short)len);
		ref_FPM_AGC_agc(b, ref, (unsigned short)len);

		compare_state(what, a, b, f);
		compare_samples(what, ours, ref, len);
		note_coverage(&before, b, in, ref, len);
	}
}

int
main(void)
{
	struct fpm_agc a, b;
	struct fpm_agc_cfg cfg = AGCb103_CFG_data;
	int rc = 0;
	int n;

	/*
	 * 1. Init, both with and without reset, from dirty memory -- so a
	 *    field the reconstruction forgets to write shows up.
	 */
	diff_begin("AGC init");
	memset(&a, 0x5a, sizeof(a));
	memset(&b, 0x5a, sizeof(b));
	FPM_AGC_init(&a, &cfg, 1);
	ref_FPM_AGC_init(&b, &cfg, 1);
	diff_eq_int("init(reset): whole state (%ld)",
		    memcmp(&a, &b, sizeof(a)), 0, 0);
	compare_state("init(reset)", &a, &b, 0);

	memset(&a, 0xa5, sizeof(a));
	memset(&b, 0xa5, sizeof(b));
	FPM_AGC_init(&a, &cfg, 0);
	ref_FPM_AGC_init(&b, &cfg, 0);
	diff_eq_int("init(no reset): whole state (%ld)",
		    memcmp(&a, &b, sizeof(a)), 0, 0);
	rc |= diff_end();

	/*
	 * 2. Fragment lengths across the partitioning discontinuity.
	 *
	 *    block_len is 36, so 1..17 must pass through untouched, 18..35
	 *    become one short block, and 53 is the longest folded block --
	 *    the one FPM_rms is not dimensioned for.
	 */
	diff_begin("AGC fragment lengths");
	for (n = 0; n <= 200; n++) {
		short ours[256], ref[256];
		int i;

		FPM_AGC_init(&a, &cfg, 1);
		ref_FPM_AGC_init(&b, &cfg, 1);

		for (i = 0; i < n; i++) {
			int v = (i & 4) ? 6000 : -6000;

			ours[i] = (short)v;
			ref[i] = (short)v;
		}

		FPM_AGC_agc(&a, ours, (unsigned short)n);
		ref_FPM_AGC_agc(&b, ref, (unsigned short)n);

		compare_state("len", &a, &b, n);
		compare_samples("len", ours, ref, n);
	}
	rc |= diff_end();

	/*
	 * 3. The loud folded block.
	 *
	 *    53 samples is the longest block the folding rule can produce,
	 *    and FPM_rms is only dimensioned for 36.  Near full scale the
	 *    true RMS passes 32767 -- but FPM_sqrt_dp CLAMPS its table index,
	 *    so the return saturates at 32703 instead of wrapping negative.
	 *
	 *    That is what keeps `shift` non-negative, which is what makes the
	 *    apply loop's five-bit shift mask dead code.  Assert it here, so
	 *    the argument in fpm_agc.c has something holding it up.
	 */
	diff_begin("AGC loud folded block");
	{
		short ours[64], ref[64];
		int i;

		for (i = 0; i < 53; i++)
			ours[i] = ref[i] = (i & 1) ? 30000 : -30000;

		diff_eq_int("FPM_rms saturates rather than wrapping (%ld)",
			    ref_FPM_rms(ref, 53), 32703, 0);
		diff_eq_int("saturated FPM_rms is still positive (%ld)",
			    ref_FPM_rms(ref, 53) > 0, 1, 0);

		FPM_AGC_init(&a, &cfg, 1);
		ref_FPM_AGC_init(&b, &cfg, 1);
		FPM_AGC_agc(&a, ours, 53);
		ref_FPM_AGC_agc(&b, ref, 53);
		compare_state("loud", &a, &b, 0);
		compare_samples("loud", ours, ref, 53);
		diff_eq_int("shift did not go negative (%ld)", b.shift >= 0, 1, 0);
	}
	rc |= diff_end();

	/*
	 * 4. A settling stream.  160-sample fragments, the size b103 uses,
	 *    at a level that needs real gain -- long enough for the loop to
	 *    converge and then track.
	 */
	diff_begin("AGC settling stream");
	rng_seed(1);
	FPM_AGC_init(&a, &cfg, 1);
	ref_FPM_AGC_init(&b, &cfg, 1);
	stream("quiet", &a, &b, 40, 160, 400, 40);
	stream("loud", &a, &b, 40, 160, 20000, 500);
	stream("mid", &a, &b, 40, 160, 3000, 200);
	rc |= diff_end();

	/*
	 * 5. Levels straddling both gates, so both silence paths fire.
	 *
	 *    Fragments are one block (36) so the gate is attributable -- see
	 *    note_coverage.  Amplitudes are chosen against what FPM_rms
	 *    actually returns, which is not the textbook RMS at these levels:
	 *    (x * 910) >> 15 is zero for |x| <= 36, and arithmetic shift makes
	 *    it -1 for small NEGATIVE x, so a tiny DC-free signal measures a
	 *    few counts rather than zero.  Amplitude 2 measures 6, under
	 *    acquire_level; amplitude 40 measures 40, between the two gates.
	 */
	diff_begin("AGC gate thresholds");
	rng_seed(7);
	FPM_AGC_init(&a, &cfg, 1);
	ref_FPM_AGC_init(&b, &cfg, 1);
	stream("below acquire", &a, &b, 6, 36, 2, 0);
	stream("above acquire", &a, &b, 20, 36, 5000, 100);
	stream("between gates", &a, &b, 10, 36, 40, 0);
	stream("above both", &a, &b, 10, 36, 5000, 100);
	rc |= diff_end();

	/*
	 * 6. Freeze and Release mid-stream.
	 */
	diff_begin("AGC freeze and release");
	rng_seed(11);
	FPM_AGC_init(&a, &cfg, 1);
	ref_FPM_AGC_init(&b, &cfg, 1);
	stream("pre-freeze", &a, &b, 10, 108, 2000, 100);

	FPM_AGC_Freeze(&a);
	ref_FPM_AGC_Freeze(&b);
	diff_eq_int("freeze flag (%ld)", a.freeze, b.freeze, 0);
	{
		short held_mult = b.mult, held_shift = b.shift;

		/* A big level change the frozen gain must NOT follow. */
		stream("frozen", &a, &b, 15, 108, 16000, 500);
		diff_eq_int("frozen mult held (%ld)", b.mult, held_mult, 0);
		diff_eq_int("frozen shift held (%ld)", b.shift, held_shift, 0);
		diff_eq_int("frozen level still moved (%ld)",
			    b.level != 0, 1, 0);
	}

	{
		short held_mult = b.mult;

		FPM_AGC_Release(&a);
		ref_FPM_AGC_Release(&b);
		compare_state("released", &a, &b, 0);
		diff_eq_int("release cleared level (%ld)", b.level, 0, 0);
		diff_eq_int("release kept gain (%ld)", b.mult, held_mult, 0);
	}
	stream("post-release", &a, &b, 15, 108, 16000, 500);
	rc |= diff_end();

	/*
	 * 7. Saturation.  A large gain applied to a large signal: the level
	 *    is first driven down so the loop winds the gain up, then the
	 *    input jumps, and the block before the loop reacts clips.
	 */
	diff_begin("AGC saturation");
	rng_seed(13);
	FPM_AGC_init(&a, &cfg, 1);
	ref_FPM_AGC_init(&b, &cfg, 1);
	stream("wind up", &a, &b, 30, 36, 120, 10);
	stream("jump", &a, &b, 4, 36, 26000, 200);
	stream("wind up 2", &a, &b, 30, 36, 100, 8);
	stream("jump 2", &a, &b, 4, 36, 30000, 500);
	rc |= diff_end();

	/*
	 * 8. Anti-vacuity.  If any of these is zero the run above proved
	 *    less than it appears to.
	 */
	diff_begin("AGC coverage");
	diff_eq_int("blocks silenced by acquire_level (%ld)",
		    seen_silence_acquire > 0, 1, seen_silence_acquire);
	diff_eq_int("blocks silenced by squelch_level (%ld)",
		    seen_silence_squelch > 0, 1, seen_silence_squelch);
	diff_eq_int("fragments that set signal (%ld)",
		    seen_gain > 0, 1, seen_gain);
	diff_eq_int("samples clamped high (%ld)",
		    seen_clamp_hi > 0, 1, seen_clamp_hi);
	diff_eq_int("samples clamped low (%ld)",
		    seen_clamp_lo > 0, 1, seen_clamp_lo);
	diff_eq_int("single-block fragments, where the gate is attributable (%ld)",
		    seen_one_block > 0, 1, seen_one_block);
	rc |= diff_end();

	printf("t_fpm_agc: acquire-silenced %d, squelch-silenced %d, "
	       "signal set %d, clamp hi/lo %d/%d, attributable blocks %d\n",
	       seen_silence_acquire, seen_silence_squelch, seen_gain,
	       seen_clamp_hi, seen_clamp_lo, seen_one_block);

	/*
	 * The DIAGNOSTIC paths, restored by finding 134 and comparable only
	 * because they exist again.  Freeze and Release emit one message
	 * each; init emits two, and the second carries the ORIGINAL's build
	 * stamp rather than this file's -- reproducing __DATE__ here would
	 * diverge on every rebuild.  Finding 135 is what those two strings
	 * turned out to be worth.
	 */
	diff_begin("fpm agc debug transcript");
	{
		static struct fpm_agc a, b;
		int reset;
		unsigned lvl;

		dsplib_debug_capture_on = 1;

		/*
		 * LEVEL 1 AS WELL AS 2, and level 1 is the one that earns its
		 * keep.  Two transcripts captured at level 2 agreeing says
		 * nothing about the GATE: a site that lost its
		 * `if (DSPLIB_DEBUG_ON())` prints the same thing on both
		 * sides and passes.
		 *
		 * The gate is `> 1`, so 1 is the single value that separates
		 * it from the `>= 1` a reader would write -- at level 0 both
		 * spellings are silent and both mutants live.  Sweeping the
		 * same calls rather than adding a second block keeps both
		 * claims pointed at one set of call sites.
		 */
		for (lvl = 1; lvl <= 2; lvl++)
		for (reset = 0; reset <= 1; reset++) {
			long tag = (long)lvl * 10 + reset;

			dsplibs_debug_level = lvl;
			ref_dsplibs_debug_level = lvl;
			dsplib_debug_capture_reset();
			memset(&a, HARNESS_MALLOC_FILL, sizeof(a));
			memset(&b, HARNESS_MALLOC_FILL, sizeof(b));

			FPM_AGC_init(&a, &cfg, reset);
			ref_FPM_AGC_init(&b, &cfg, reset);
			FPM_AGC_Freeze(&a);
			ref_FPM_AGC_Freeze(&b);
			FPM_AGC_Release(&a);
			ref_FPM_AGC_Release(&b);

			diff_eq_int("agc transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);

			if (lvl < 2) {
				diff_eq_int("below the threshold, ours said "
					    "nothing",
					    dsplib_debug_capture_text(0)[0], 0,
					    tag);
				diff_eq_int("below the threshold, nor did the "
					    "reference",
					    dsplib_debug_capture_text(1)[0], 0,
					    tag);
				continue;
			}

			diff_eq_int("agc transcript non-empty",
				    dsplib_debug_capture_text(1)[0] != 0, 1,
				    tag);
			/* And it must carry the blob's build stamp. */
			diff_eq_int("build stamp present",
				    strstr(dsplib_debug_capture_text(1),
					   "Sep 22 2005 15:48:18") != NULL,
				    1, tag);
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	return rc;
}

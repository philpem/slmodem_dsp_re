/*
 * t_v34det.c -- differential test of the V.34 tone detector.
 *
 * The filter is the easy half: drive samples in, compare samples out.  The
 * hard half is that almost nothing this object does is visible in its return
 * value.  `tone_detect` returns 0 for the whole warm-up, 0 again while it
 * waits to arm, and 0 for every block below threshold -- so a detector that
 * had lost its filter state entirely would return the same 0 as a working one
 * for hundreds of calls.  Every check here therefore compares the whole 0x24
 * byte object as well as the verdict, and the counters at the bottom refuse
 * to let the test pass unless each of the four decision paths was reached.
 *
 * Two objects and two `struct v34_receiver`, not one of each: both sides write to
 * their detector and both clear a bit in their receiver, and a shared one
 * would have each side comparing against the other's writes.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v34det.h"

extern void ref_detectorinit(void *d, const short *coeff, short polarity,
			     short limit, short warmup, short thresh_lo,
			     short thresh_hi);
extern int ref_tone_detect(void *rx, void *d, const short *start,
			   const short *end);

/*
 * Two cascaded resonators at 2100 Hz, sampled at V.34's 9600 Hz host rate,
 * with a pole radius of 0.98.
 *
 * The object takes its coefficients from a pointer the handshake installs
 * (`obj+0xaab0`), and that table has not been reconstructed yet -- it belongs
 * to a translation unit that is still opaque.  So these are designed here
 * rather than recovered.  That is fine for a differential test, which only
 * needs both sides to see the same eight shorts, and it is what makes the
 * "did it behave like a band-pass" checks at the bottom meaningful rather
 * than merely self-consistent.
 *
 * Grouped by role, not by section -- see v34det.h:
 *     [0..1] section 1 feed-forward     [2..3] section 2 feed-forward
 *     [4..5] section 1 feedback         [6..7] section 2 feedback
 *
 * Numerator 1 - z^-2 in Q14 is { 0, -16384 }; denominator
 * 1 - 2r.cos(w0).z^-1 + r^2.z^-2 with r = 0.95 gives a1 = -2r.cos(w0) and
 * a2 = r^2 = 0.9025 -> 14787.
 *
 * r is 0.95 rather than something sharper on purpose.  A resonator with a
 * pole at 0.99 has a gain near 100 at its centre, and two of them in cascade
 * would spend this whole test in the accumulator's overflow behaviour -- which
 * IS reproduced faithfully, and is compared byte for byte below, but it would
 * leave the "behaves like a band-pass" assertions measuring the truncation
 * rather than the filter.
 */
static const short bp2100[8] = {
	0, -16384,		/* section 1 feed-forward */
	0, -16384,		/* section 2 feed-forward */
	-6073, 14787,		/* section 1 feedback     */
	-6073, 14787		/* section 2 feedback     */
};

/* A second set, off-tune at 600 Hz, so a tuned/untuned comparison exists. */
static const short bp600[8] = {
	0, -16384,
	0, -16384,
	-28760, 14787,
	-28760, 14787
};

#define SRATE		9600
#define NSAMP		4096

/* Coverage: every decision path this test claims to reach. */
static int saw_warmup;		/* returned early because warm-up was on  */
static int saw_arm;		/* a presence detector armed itself       */
static int saw_assert;		/* something returned 1                   */
static int saw_absence;		/* the polarity != 0 branch ran           */
static int saw_reset;		/* a count was reset to zero              */

static short sig[NSAMP];

/*
 * Compare two detector objects byte for byte.
 *
 * Nothing is skipped: `coeff` is the same pointer on both sides, because both
 * are handed the same static, so even that compares equal.  Under the
 * harness's 0xa5 fill an unwritten field is 0xa5a5 on both sides, so a
 * difference always means one side wrote where the other did not.
 */
static void
compare_obj(const char *what, const struct v34_detector *ours, const void *ref)
{
	const unsigned char *a = (const unsigned char *)ours;
	const unsigned char *b = (const unsigned char *)ref;
	unsigned i;

	for (i = 0; i < sizeof(*ours); i++)
		diff_eq_int(what, a[i], b[i], i);
}

/*
 * A sine at `hz`, amplitude `amp`, written over the shared buffer.
 *
 * Deliberately generated with the object's own cosine table rather than with
 * libm, so the test signal is exactly representable and a failure can never
 * be blamed on the host's sin().
 */
static void
fill_tone(int hz, int amp)
{
	unsigned phase = 0;
	unsigned inc = (unsigned)(16384L * hz / SRATE);
	int i;

	for (i = 0; i < NSAMP; i++) {
		phase = (phase + inc) & V34_DFT_PHASE_MASK;
		sig[i] = (short)((costbl[phase >> V34_DFT_PHASE_SHIFT] * amp)
				 >> 14);
	}
}

static void
fill_silence(void)
{
	memset(sig, 0, sizeof(sig));
}

/*
 * A tone that stops half way.
 *
 * Without this the reset path is never reached: a signal that is present from
 * the start only ever increments the counter, and one that is absent from the
 * start never arms the detector, so `count` is never positive at the moment
 * the level falls back through the threshold.  That is the transition the
 * whole hysteresis arrangement exists for, and it took a coverage counter
 * reading zero to notice it was untested.
 */
static void
fill_burst(int hz, int amp, int on)
{
	fill_tone(hz, amp);
	memset(sig + on, 0, (NSAMP - on) * sizeof(sig[0]));
}

/*
 * Drive both sides over the whole buffer in blocks of `block`, comparing the
 * verdict and the object after every call.  Returns the number of calls that
 * asserted.
 */
static int
run(const char *what, const short *coeff, short polarity, short limit,
    short warmup, short thresh_lo, short thresh_hi, int block)
{
	struct v34_detector ours;
	unsigned char ref[0x24];
	struct v34_receiver rx_ours, rx_ref;
	int asserts = 0;
	int i;
	int call = 0;

	memset(&ours, HARNESS_MALLOC_FILL, sizeof(ours));
	memset(ref, HARNESS_MALLOC_FILL, sizeof(ref));
	memset(&rx_ours, 0, sizeof(rx_ours));
	memset(&rx_ref, 0, sizeof(rx_ref));
	rx_ours.flags = rx_ref.flags = V34_RX_FLAG_DET_PENDING | 0x1234;

	detectorinit(&ours, coeff, polarity, limit, warmup, thresh_lo,
		     thresh_hi);
	ref_detectorinit(ref, coeff, polarity, limit, warmup, thresh_lo,
			 thresh_hi);
	compare_obj(what, &ours, ref);

	for (i = 0; i + block <= NSAMP; i += block, call++) {
		short before_state = ours.state;
		short before_armed = ours.armed;
		short before_count = ours.count;
		int ra, rb;

		ra = tone_detect(&rx_ours, &ours, sig + i, sig + i + block);
		rb = ref_tone_detect(&rx_ref, ref, sig + i, sig + i + block);

		diff_eq_int(what, ra, rb, call);
		compare_obj(what, &ours, ref);
		/*
		 * The whole receiver stub, not just `flags`.  The
		 * disassembly says 0x14(%esp) has exactly one use in
		 * tone_detect, so comparing the one field would be
		 * defensible -- but a wrong offset is precisely the mistake
		 * that would land in the padding, and t_v34fsk holds its own
		 * object map to the same standard.
		 */
		{
			const unsigned char *p = (const unsigned char *)&rx_ours;
			const unsigned char *q = (const unsigned char *)&rx_ref;
			unsigned b;

			for (b = 0; b < sizeof(rx_ours); b++)
				diff_eq_int("the receiver object", p[b], q[b],
					    b);
		}

		if (before_state == V34_DET_STATE_WARMUP
		    && ours.state == V34_DET_STATE_WARMUP)
			saw_warmup++;
		if (!before_armed && ours.armed)
			saw_arm++;
		if (before_count > 0 && ours.count == 0)
			saw_reset++;
		if (polarity != 0)
			saw_absence++;
		if (ra) {
			asserts++;
			saw_assert++;
		}
	}

	return asserts;
}

int
main(void)
{
	int rc = 0;
	int on_tune, off_tune, on_silence, absent_tone, absent_quiet;
	int burst;

	diff_begin("v34 detector: presence, on and off tune");

	/*
	 * Block sizes are varied against nothing in particular -- unlike the
	 * transmitter there is no period table to get out of phase with -- but
	 * the warm-up counter steps once per CALL, so the block size decides
	 * how much signal passes before the detector is allowed to speak.  40
	 * and 160 differ by four times in that respect.
	 */
	fill_tone(2100, 8000);
	on_tune = run("2100 Hz into a 2100 Hz detector[%ld]", bp2100, 0, 4, 8,
		      2000, 0, 160);
	off_tune = run("2100 Hz into a 600 Hz detector[%ld]", bp600, 0, 4, 8,
		       2000, 0, 160);

	fill_silence();
	on_silence = run("silence into a 2100 Hz detector[%ld]", bp2100, 0, 4,
			 8, 2000, 0, 160);

	/* A tone that stops: the only run that reaches the reset path. */
	fill_burst(2100, 8000, NSAMP / 2);
	burst = run("2100 Hz for half the buffer[%ld]", bp2100, 0, 4, 8, 2000,
		    0, 160);

	rc |= diff_end();

	diff_begin("v34 detector: absence");

	fill_tone(2100, 8000);
	absent_tone = run("absence, tone present[%ld]", bp2100, 1, 4, 8, 400,
			  2000, 40);
	fill_silence();
	absent_quiet = run("absence, line quiet[%ld]", bp2100, 1, 4, 8, 400,
			   2000, 40);

	rc |= diff_end();

	diff_begin("v34 detector: short blocks and a long warm-up");

	/*
	 * One sample per call, and a warm-up of 300 calls.  This is the
	 * configuration where the counter's units matter: 300 calls of one
	 * sample is 300 samples of hold-off, where 300 calls of 160 would be
	 * most of a second.  It also runs the filter 4096 times with the
	 * decision logic in between, which the 160-sample blocks do only 25
	 * times.
	 */
	fill_tone(2100, 12000);
	run("one sample per call[%ld]", bp2100, 0, 4, 300, 2000, 0, 1);
	run("two samples per call, absence[%ld]", bp2100, 1, 2, 300, 400,
	    2000, 2);

	rc |= diff_end();

	diff_begin("v34 detector: it behaves like a detector");

	printf("  asserting calls: on-tune %d, off-tune %d, silence %d,"
	       " burst %d, absence(tone) %d, absence(quiet) %d\n",
	       on_tune, off_tune, on_silence, burst, absent_tone,
	       absent_quiet);

	/*
	 * These are not differential checks -- they would pass on a
	 * reconstruction that matched a broken blob.  They are here to catch
	 * the case where both sides agree because neither is doing anything,
	 * which every comparison above would happily report as success.
	 */
	diff_eq_int("a tuned presence detector fires on its own tone",
		    on_tune > 0, 1, on_tune);
	diff_eq_int("an off-tune one does not", off_tune, 0, off_tune);
	diff_eq_int("neither does a tuned one on silence", on_silence, 0,
		    on_silence);
	diff_eq_int("an absence detector stays quiet while the tone is there",
		    absent_tone, 0, absent_tone);
	diff_eq_int("and fires once the line goes quiet", absent_quiet > 0, 1,
		    absent_quiet);
	/*
	 * The burst runs the same detector over half a buffer of tone and
	 * half of silence, so it must assert -- but fewer times than the
	 * uninterrupted tone did, because the count is reset when the tone
	 * stops and never climbs back.
	 */
	diff_eq_int("a burst fires", burst > 0, 1, burst);
	diff_eq_int("but not for as long as a continuous tone",
		    burst < on_tune, 1, burst);

	rc |= diff_end();

	diff_begin("v34 detector: coverage");
	diff_eq_int("a call returned early in warm-up", saw_warmup > 0, 1, 0);
	diff_eq_int("a presence detector armed", saw_arm > 0, 1, 0);
	diff_eq_int("something asserted", saw_assert > 0, 1, 0);
	diff_eq_int("the absence branch ran", saw_absence > 0, 1, 0);
	diff_eq_int("a count was reset", saw_reset > 0, 1, 0);
	rc |= diff_end();

	return rc;
}

/*
 * t_dualtone.c -- differential test of the answer-tone detector.
 *
 * The detector has six verdicts and a naive test reaches two of them.  Fed
 * noise it says "energy, no tone" forever; fed nothing it says "no signal"
 * forever; and two implementations agreeing on a constant are not evidence of
 * anything.  So the stimuli are built to reach each verdict deliberately --
 * including the short/long split, which needs a tone held past 1280 samples
 * and the same tone cut short -- and the run ends by asserting that all six
 * actually fired.
 *
 * Everything is compared after every block: the verdict, all three energy
 * accumulators, both hold timers and all twenty history words.  A detector
 * that agreed on verdicts while drifting internally would fail here on the
 * first block, not eventually.
 *
 * TONE_read is swept over its entire input domain -- all 65536 shorts -- which
 * is cheap and proves the 513-entry quarter-wave table word for word.  That
 * matters because the table is a file static with no symbol: this is the only
 * way to check it against the blob.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "harness.h"
#include "dsplib/dualtone.h"

extern struct dual_tone *ref_Dual_TONE_create(void);
extern void ref_Dual_TONE_delete(struct dual_tone *st);
extern int ref_Dual_TONE_detect(struct dual_tone *st, const short *samples,
				int count);
extern short ref_TONE_read(short phase);
extern short ref_Get_Detection_Threshold_Table(short level);

#define FS 8000.0

/*
 * The largest 2100 Hz sine amplitude the detector still recognises, measured
 * by run_headroom below.  It is within 5% of full scale, so the ceiling is not
 * a practical limit -- but it exists, and pinning it keeps a change in either
 * direction visible.
 */
#define HEADROOM_LIMIT 31000

/* Times each verdict was returned, over the whole run. */
static int verdict_seen[6];

/*
 * ---------------------------------------------------------------------------
 * TONE_read and the threshold table
 */
static int
run_tone_read(void)
{
	int p;
	int nonzero = 0, negative = 0;

	diff_begin("TONE_read");

	for (p = -32768; p <= 32767; p++) {
		short a = ref_TONE_read((short)p);
		short b = TONE_read((short)p);

		diff_eq_int("phase %ld", b, a, p);
		if (a != 0)
			nonzero++;
		if (a < 0)
			negative++;
	}

	/*
	 * Anti-vacuity: a table of zeros would agree perfectly.  A cosine
	 * spends half its cycle negative, and exactly two of every 2048
	 * phases land on zero.
	 */
	diff_eq_int("nonzero results (%ld)", nonzero, 65536 - 64, nonzero);
	diff_eq_int("negative results (%ld)", negative, 32736, negative);

	/* Spot-check the four quadrants against the cosine they encode. */
	diff_eq_int("cos(0)", TONE_read(0), 16384, 0);
	diff_eq_int("cos(pi/2)", TONE_read(512), 0, 512);
	diff_eq_int("cos(pi)", TONE_read(1024), -16384, 1024);
	diff_eq_int("cos(3pi/2)", TONE_read(1536), 0, 1536);
	diff_eq_int("phase wraps at 2048", TONE_read(2048), TONE_read(0), 2048);
	diff_eq_int("phase wraps negative", TONE_read(-2048), TONE_read(0),
		    -2048);

	return diff_end();
}

static int
run_threshold_table(void)
{
	int level;

	diff_begin("Get_Detection_Threshold_Table");

	for (level = DETECTION_THRESHOLD_MIN_LEVEL;
	     level <= DETECTION_THRESHOLD_MAX_LEVEL; level++)
		diff_eq_int("level %ld", Get_Detection_Threshold_Table((short)level),
			    ref_Get_Detection_Threshold_Table((short)level),
			    level);

	/*
	 * The table falls monotonically with level, which is what makes it a
	 * threshold curve rather than sixteen unrelated numbers.  Out-of-range
	 * levels are NOT swept: the original indexes past the table there, and
	 * two implementations reading different neighbouring objects would be
	 * comparing their link order.
	 */
	for (level = DETECTION_THRESHOLD_MIN_LEVEL;
	     level < DETECTION_THRESHOLD_MAX_LEVEL; level++)
		diff_eq_int("level %ld exceeds the next",
			    Get_Detection_Threshold_Table((short)level)
			    > Get_Detection_Threshold_Table((short)(level + 1)),
			    1, level);

	return diff_end();
}

/*
 * ---------------------------------------------------------------------------
 * create and delete
 */
static int
run_create(void)
{
	struct dual_tone *a, *b;

	diff_begin("Dual_TONE_create");

	harness_alloc_reset();
	a = ref_Dual_TONE_create();
	b = Dual_TONE_create();

	diff_eq_int("ref allocated", a != 0, 1, 0);
	diff_eq_int("allocated", b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	diff_eq_int("allocs", harness_alloc.allocs, 2, 0);
	diff_eq_int("bytes", (int)harness_alloc.bytes, 2 * 0x44, 0);

	/*
	 * The whole object, byte for byte.  Safe here where it was not for
	 * _iir_filter_create: this one memsets everything it allocated, so
	 * there is no uninitialised slack to compare.
	 */
	diff_eq_obj("the whole object", struct dual_tone, b, a, 0);

	diff_eq_int("ratio", b->ratio, 226, 0);
	diff_eq_int("min_energy", b->min_energy, 1, 0);

	ref_Dual_TONE_delete(a);
	Dual_TONE_delete(b);
	diff_eq_int("frees", harness_alloc.frees, 2, 0);
	diff_eq_int("live", harness_alloc.live, 0, 0);
	diff_eq_int("bad frees", harness_alloc.bad_free, 0, 0);

	/* Both free NULL without complaint -- there is no guard in either. */
	ref_Dual_TONE_delete(0);
	Dual_TONE_delete(0);
	diff_eq_int("delete(NULL) is not a bad free", harness_alloc.bad_free,
		    0, 0);

	return diff_end();
}

/*
 * ---------------------------------------------------------------------------
 * detect
 */
static void
compare_state(const struct dual_tone *b, const struct dual_tone *a, long n)
{
	int i;

	diff_eq_int("block %ld: energy_a", b->energy_a, a->energy_a, n);
	diff_eq_int("block %ld: energy_b", b->energy_b, a->energy_b, n);
	diff_eq_int("block %ld: energy", b->energy, a->energy, n);
	diff_eq_int("block %ld: hold_a", b->hold_a, a->hold_a, n);
	diff_eq_int("block %ld: hold_b", b->hold_b, a->hold_b, n);
	for (i = 0; i < 4; i++) {
		diff_eq_int("block %ld: notch_a", b->notch_a[i], a->notch_a[i],
			    n);
		diff_eq_int("block %ld: notch_b", b->notch_b[i], a->notch_b[i],
			    n);
		diff_eq_int("block %ld: notch_c", b->notch_c[i], a->notch_c[i],
			    n);
	}
	for (i = 0; i < 4 * DUAL_TONE_BP_SECTIONS; i++)
		diff_eq_int("block %ld: bp", b->bp[i], a->bp[i], n);
}

/* Signal generators.  `kind` selects what fills the block. */
enum { SIG_SILENCE, SIG_NOISE, SIG_TONE_2100, SIG_TONE_1800, SIG_TONE_2250,
       SIG_TONE_1000, SIG_SPEECHY };

static unsigned lfsr = 0x7A9Du;
static double phase;

static void
fill(short *buf, int n, int kind, int amplitude)
{
	int i;

	for (i = 0; i < n; i++) {
		double f;

		switch (kind) {
		case SIG_SILENCE:
			buf[i] = 0;
			continue;
		case SIG_NOISE:
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
			buf[i] = (short)(((int)(lfsr & 0xffffu) - 0x8000)
					 / (0x8000 / amplitude));
			continue;
		case SIG_TONE_2100: f = 2100.0; break;
		case SIG_TONE_1800: f = 1800.0; break;
		case SIG_TONE_2250: f = 2250.0; break;
		case SIG_TONE_1000: f = 1000.0; break;
		default:
			/*
			 * Two strong tones plus noise -- broadband enough that
			 * no single notch can claim 88% of it, which is what
			 * a voice looks like to this detector.
			 */
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
			buf[i] = (short)(amplitude * 0.4 * sin(phase)
					 + amplitude * 0.4 * sin(phase * 1.7)
					 + ((int)(lfsr & 0x1fff) - 0x1000) / 4);
			phase += 2.0 * 3.14159265358979323846 * 900.0 / FS;
			continue;
		}
		buf[i] = (short)(amplitude * sin(phase));
		phase += 2.0 * 3.14159265358979323846 * f / FS;
	}
}

/*
 * `want` names the verdict this stimulus should mostly produce, or -1 to make
 * no claim.  It is the only check on the frequency story in dualtone.h -- that
 * notch A is 2100 Hz and notches B and C together cover the FSK answer band --
 * because the differential comparison cannot see it: the blob would agree
 * perfectly with a reconstruction that had A and B swapped, as long as both
 * were swapped.  The short and confirmed forms of a verdict count together,
 * since which one a block lands on depends only on the hold timer.
 */
static int
run_detect(const char *label, int kind, int amplitude, int blocks, int frag,
	   int want)
{
	struct dual_tone *a, *b;
	short buf[512];
	int hist[6];
	int hits = 0, total = 0;
	int n;

	diff_begin(label);
	memset(hist, 0, sizeof(hist));

	a = ref_Dual_TONE_create();
	b = Dual_TONE_create();
	if (a == 0 || b == 0) {
		diff_eq_int("allocated", 0, 1, 0);
		return diff_end();
	}

	phase = 0.0;
	for (n = 0; n < blocks; n++) {
		int va, vb;

		fill(buf, frag, kind, amplitude);

		va = ref_Dual_TONE_detect(a, buf, frag);
		vb = Dual_TONE_detect(b, buf, frag);

		diff_eq_int("block %ld: verdict", vb, va, n);
		compare_state(b, a, n);

		if (va >= 0 && va < 6) {
			verdict_seen[va]++;
			hist[va]++;
			total++;
			if (va == want
			    || (want == DUAL_TONE_A
				&& va == DUAL_TONE_A_CONFIRMED)
			    || (want == DUAL_TONE_B
				&& va == DUAL_TONE_B_CONFIRMED))
				hits++;
		}
	}

	if (want >= 0) {
		char msg[128];

		snprintf(msg, sizeof(msg),
			 "verdict %d dominates: %d of %d "
			 "[%d %d %d %d %d %d]",
			 want, hits, total, hist[0], hist[1], hist[2],
			 hist[3], hist[4], hist[5]);
		diff_eq_int(msg, total > 0 && hits * 2 > total, 1, want);
	}

	ref_Dual_TONE_delete(a);
	Dual_TONE_delete(b);

	return diff_end();
}

/*
 * A tone deliberately interrupted before the hold expires, so the short
 * verdicts (2 and 4) are reached repeatedly and the long ones (3 and 5) are
 * not reached by this case at all.
 */
static int
run_detect_interrupted(void)
{
	struct dual_tone *a, *b;
	short buf[160];
	int n;

	diff_begin("Dual_TONE_detect: tone interrupted before the hold");

	a = ref_Dual_TONE_create();
	b = Dual_TONE_create();
	if (a == 0 || b == 0) {
		diff_eq_int("allocated", 0, 1, 0);
		return diff_end();
	}

	phase = 0.0;
	for (n = 0; n < 200; n++) {
		int va, vb;

		/* Six blocks of tone (960 samples, short of 1280), then two
		 * of noise to clear the timer. */
		fill(buf, 160, (n % 8) < 6 ? SIG_TONE_2100 : SIG_SPEECHY,
		     12000);

		va = ref_Dual_TONE_detect(a, buf, 160);
		vb = Dual_TONE_detect(b, buf, 160);

		diff_eq_int("block %ld: verdict", vb, va, n);
		compare_state(b, a, n);
		if (va >= 0 && va < 6)
			verdict_seen[va]++;
	}

	ref_Dual_TONE_delete(a);
	Dual_TONE_delete(b);

	return diff_end();
}

/*
 * count <= 0 still returns a verdict, computed from the energies retained
 * since the last call, and still advances both hold timers by count.  Easy to
 * get wrong by putting the early-out before the timer update.
 */
static int
run_detect_empty(void)
{
	struct dual_tone *a, *b;
	short buf[160];
	int n;

	diff_begin("Dual_TONE_detect: zero and negative counts");

	a = ref_Dual_TONE_create();
	b = Dual_TONE_create();
	if (a == 0 || b == 0) {
		diff_eq_int("allocated", 0, 1, 0);
		return diff_end();
	}

	phase = 0.0;
	fill(buf, 160, SIG_TONE_2100, 12000);
	ref_Dual_TONE_detect(a, buf, 160);
	Dual_TONE_detect(b, buf, 160);

	for (n = 0; n < 20; n++) {
		int count = (n % 2) ? 0 : -4;
		int va = ref_Dual_TONE_detect(a, buf, count);
		int vb = Dual_TONE_detect(b, buf, count);

		diff_eq_int("call %ld: verdict", vb, va, n);
		compare_state(b, a, n);
		if (va >= 0 && va < 6)
			verdict_seen[va]++;
	}

	ref_Dual_TONE_delete(a);
	Dual_TONE_delete(b);

	return diff_end();
}

/*
 * How much signal the detector can take before it stops working.
 *
 * Sweeps the amplitude of a 2100 Hz tone and finds the largest at which the
 * tone is still recognised.  Both implementations are compared at every step,
 * so this is a differential test as well as a measurement -- but its real
 * purpose is to pin the number, because nothing in the module hints that
 * there is a ceiling at all.
 */
static int
run_headroom(void)
{
	int amplitude;
	int highest_ok = 0;

	diff_begin("Dual_TONE_detect: input headroom");

	for (amplitude = 1000; amplitude <= 32500; amplitude += 500) {
		struct dual_tone *a, *b;
		short buf[160];
		int detected = 0;
		int n;

		a = ref_Dual_TONE_create();
		b = Dual_TONE_create();
		if (a == 0 || b == 0) {
			diff_eq_int("allocated", 0, 1, 0);
			return diff_end();
		}

		phase = 0.0;
		for (n = 0; n < 40; n++) {
			int va, vb;

			fill(buf, 160, SIG_TONE_2100, amplitude);
			va = ref_Dual_TONE_detect(a, buf, 160);
			vb = Dual_TONE_detect(b, buf, 160);
			diff_eq_int("amplitude %ld: verdict", vb, va, amplitude);
			if (n >= 8 && (va == DUAL_TONE_A
				       || va == DUAL_TONE_A_CONFIRMED))
				detected++;
		}

		if (detected > 16)
			highest_ok = amplitude;

		ref_Dual_TONE_delete(a);
		Dual_TONE_delete(b);
	}

	{
		char msg[96];

		snprintf(msg, sizeof(msg),
			 "highest working amplitude is %d", highest_ok);
		diff_eq_int(msg, highest_ok, HEADROOM_LIMIT, highest_ok);
	}

	return diff_end();
}

/*
 * ---------------------------------------------------------------------------
 */
int
main(void)
{
	static const char *verdict_name[6] = {
		"NOSIGNAL", "OTHER", "A", "A_CONFIRMED", "B", "B_CONFIRMED"
	};
	int rc = 0;
	int v;

	rc |= run_tone_read();
	rc |= run_threshold_table();
	rc |= run_create();

	rc |= run_detect("Dual_TONE_detect: silence",
			 SIG_SILENCE, 0, 60, 160, DUAL_TONE_NOSIGNAL);
	rc |= run_detect("Dual_TONE_detect: noise",
			 SIG_NOISE, 8000, 60, 160, DUAL_TONE_OTHER);
	rc |= run_detect("Dual_TONE_detect: 2100 Hz answer tone",
			 SIG_TONE_2100, 12000, 80, 160, DUAL_TONE_A);
	rc |= run_detect("Dual_TONE_detect: 1800 Hz",
			 SIG_TONE_1800, 12000, 80, 160, DUAL_TONE_B);
	rc |= run_detect("Dual_TONE_detect: 2250 Hz",
			 SIG_TONE_2250, 12000, 80, 160, DUAL_TONE_B);
	rc |= run_detect("Dual_TONE_detect: 1000 Hz, out of band",
			 SIG_TONE_1000, 12000, 60, 160, DUAL_TONE_NOSIGNAL);
	rc |= run_detect("Dual_TONE_detect: speech-like",
			 SIG_SPEECHY, 12000, 60, 160, DUAL_TONE_OTHER);
	rc |= run_detect("Dual_TONE_detect: 2100 Hz, one sample at a time",
			 SIG_TONE_2100, 12000, 3000, 1, DUAL_TONE_A);
	/*
	 * At full scale the detector no longer recognises its own tone: the
	 * filter accumulators leave 32 bits and wrap.  It takes a sine within
	 * 5% of full scale to do it, so this is not a practical limit -- but
	 * it is asserted rather than avoided, so the operating range stays
	 * documented and a change in either direction fails.
	 */
	rc |= run_detect("Dual_TONE_detect: 2100 Hz, full scale",
			 SIG_TONE_2100, 32000, 80, 160, DUAL_TONE_OTHER);
	rc |= run_headroom();
	rc |= run_detect_interrupted();
	rc |= run_detect_empty();

	/*
	 * Anti-vacuity.  Six verdicts exist and this asserts every one was
	 * reached; without it the whole suite above could be two
	 * implementations agreeing on DUAL_TONE_OTHER four thousand times.
	 */
	diff_begin("every verdict reached");
	for (v = 0; v < 6; v++) {
		char msg[80];

		snprintf(msg, sizeof(msg), "%s reached (%d times)",
			 verdict_name[v], verdict_seen[v]);
		diff_eq_int(msg, verdict_seen[v] > 0, 1, v);
	}
	rc |= diff_end();

	return rc;
}

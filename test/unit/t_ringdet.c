/*
 * t_ringdet.c -- differential test of the whole software ring detector:
 * `RingDetector_{Reset,Create,Delete,GetLastRing,Process}` and the four
 * `RD_*` entry points slmodemd calls.
 *
 * WHAT COUNTS AS AN OBSERVABLE RESULT HERE.  Four things:
 *
 *   - the whole 0x52-byte detector state, through `diff_eq_obj`, after every
 *     single block.  There are 29 fields and 25 of them are internal, so the
 *     return value alone would leave most of the machine untested;
 *   - the return value of `Process`/`RD_process`, which is the edge flag;
 *   - the pair `GetLastRing` hands back, which is what slmodemd turns into a
 *     ring report;
 *   - the diagnostic transcript, at debug levels 0 and 2, because there are
 *     four gated sites across these functions and a run at one level cannot
 *     separate `> 1` from `> 0` (debug.h, finding F150).
 *
 * PREFILL IS 0xa5, NOT ZERO.  An unwritten field must MATCH BY BEING
 * UNWRITTEN; zero is the one filler that makes "never written" indis-
 * tinguishable from "written with the right value" (the t_dialstring
 * argument).
 *
 * COVERAGE IS ASSERTED FROM THE RUN, NOT FROM THE PLAN (finding F134).  The
 * `seen_*` counters below are incremented from the REFERENCE side's state
 * after each block -- i.e. from what the blob actually did with the samples
 * this file generated -- and `main` fails if any of them is still zero.  A
 * scenario that stops reaching a state therefore breaks the build instead of
 * quietly testing less; the counters were confirmed to fire by watching each
 * one move before it was asserted on.
 *
 * WHY THE SIGNAL IS A SINE AND NOT A SQUARE.  Both sides see the same
 * `short` buffer whatever generates it, so generation is not part of the
 * comparison -- but the guard band between `lock_level` and `threshold` is
 * only entered by a waveform that spends time there, and a square wave would
 * leave `guard_run` at zero for the whole run.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/ringdet.h"
#include "dsplib/modem_params.h"

extern unsigned int ref_dsplibs_debug_level;
extern void ref_RingDetector_Reset(struct ring_detector *s,
				   struct ring_detector_cfg *c);
extern struct ring_detector *ref_RingDetector_Create(
					struct ring_detector_cfg *c);
extern void ref_RingDetector_Delete(struct ring_detector *s);
extern void ref_RingDetector_GetLastRing(struct ring_detector *s, int *freq,
					 int *dur);
extern int ref_RingDetector_Process(struct ring_detector *s, short *in,
				    unsigned int count);
extern void *ref_RD_create(void *modem, unsigned int rate);
extern void ref_RD_delete(void *obj);
extern int ref_RD_process(void *obj, void *in, int count);
extern void ref_RD_ring_details(void *obj, int *freq, int *duration);

/* Per-side debug transcripts; see test/harness/runtime.c. */
extern int dsplib_debug_capture_on;
void dsplib_debug_capture_reset(void);
unsigned dsplib_debug_capture_lines(int side);
const char *dsplib_debug_capture_text(int side);

#define MAXBLK 1024

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/* ---------------------------------------------------------------- */
/* Coverage counters, all read from the REFERENCE side after a block. */

static long seen_state[4];	/* [0] is "some other value", never expected */
static long seen_ring_on;
static long seen_ring_off;
static long seen_edge_start;	/* an edge reported with freq == 0    */
static long seen_edge_end;	/* an edge reported with freq >  0    */
static long seen_guard;		/* guard_run left non-zero            */
static long seen_measure;	/* at least one frequency averaged in */
static long seen_cycles;
static long seen_lost;		/* the "no crossing for a period" reset */
static long seen_zero_block;

static void
note_state(const struct ring_detector *r)
{
	if (r->state >= 1 && r->state <= 3)
		seen_state[r->state]++;
	else
		seen_state[0]++;
	if (r->ring_active)
		seen_ring_on++;
	else
		seen_ring_off++;
	if (r->guard_run)
		seen_guard++;
	if (r->freq_n)
		seen_measure++;
	if (r->cycles)
		seen_cycles++;
}

/* ---------------------------------------------------------------- */

struct pair {
	struct ring_detector *ours;
	struct ring_detector *ref;
};

static void
pair_create(struct pair *p, struct ring_detector_cfg *cfg, long tag)
{
	p->ref = ref_RingDetector_Create(cfg);
	p->ours = RingDetector_Create(cfg);
	diff_eq_int("create returned an object (%ld)",
		    p->ours != 0 && p->ref != 0, 1, tag);
	diff_eq_int("create asked for the same size (%ld)",
		    (int)harness_alloc_reqsize(p->ours),
		    (int)harness_alloc_reqsize(p->ref), tag);
	diff_eq_obj("fresh detector", struct ring_detector, p->ours, p->ref,
		    tag);
}

static void
pair_free(struct pair *p)
{
	RingDetector_Delete(p->ours);
	ref_RingDetector_Delete(p->ref);
	p->ours = p->ref = 0;
}

/*
 * One block through both sides.  Returns our verdict so a caller can assert
 * on it, and folds the reference's state into the coverage counters.
 */
static int
pair_block(struct pair *p, const short *src, unsigned int n, long tag)
{
	short a[MAXBLK], b[MAXBLK];
	int ra, rb;

	memcpy(a, src, (size_t)n * sizeof a[0]);
	memcpy(b, src, (size_t)n * sizeof b[0]);

	rb = ref_RingDetector_Process(p->ref, b, n);
	ra = RingDetector_Process(p->ours, a, n);

	diff_eq_int("verdict (%ld)", ra, rb, tag);
	diff_eq_int("input untouched (%ld)",
		    memcmp(a, b, (size_t)n * sizeof a[0]) == 0, 1, tag);
	diff_eq_obj("detector after the block", struct ring_detector,
		    p->ours, p->ref, tag);

	if (n == 0)
		seen_zero_block++;
	note_state(p->ref);
	if (rb) {
		int fa, da, fb, db;

		RingDetector_GetLastRing(p->ours, &fa, &da);
		ref_RingDetector_GetLastRing(p->ref, &fb, &db);
		diff_eq_int("edge frequency (%ld)", fa, fb, tag);
		diff_eq_int("edge duration (%ld)", da, db, tag);
		if (fb == 0)
			seen_edge_start++;
		else if (fb > 0)
			seen_edge_end++;
	}
	return ra;
}

/* ---------------------------------------------------------------- */

/*
 * A sine at `freq` Hz, `amp` counts peak, plus a constant offset.  `phase` is
 * carried across calls so a scenario can be split into blocks of any size
 * without a discontinuity at every boundary.
 */
static void
fill_sine(short *buf, unsigned int n, double *phase, double freq, double fs,
	  double amp, double offset)
{
	unsigned int i;

	for (i = 0; i < n; i++) {
		double v = offset + amp * sin(*phase);

		*phase += 2.0 * 3.14159265358979323846 * freq / fs;
		if (*phase > 2.0 * 3.14159265358979323846)
			*phase -= 2.0 * 3.14159265358979323846;
		if (v > 32767.0)
			v = 32767.0;
		if (v < -32768.0)
			v = -32768.0;
		buf[i] = (short)v;
	}
}

static void
fill_const(short *buf, unsigned int n, int v)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		buf[i] = (short)v;
}

/* Deterministic 32-bit LCG; both sides always see the same stream. */
static unsigned int lcg_state = 0x1234567u;
static unsigned int
lcg(void)
{
	lcg_state = lcg_state * 1664525u + 1013904223u;
	return lcg_state;
}

static void
fill_noise(short *buf, unsigned int n, int amp)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		buf[i] = (short)((int)(lcg() >> 16) % (2 * amp + 1) - amp);
}

/*
 * Run `ms` milliseconds of a tone (amp 0 is silence) through the pair, split
 * into blocks of `blk` samples.  Returns how many edges were reported.
 */
static int
run_tone(struct pair *p, double *phase, double freq, double fs, double amp,
	 double offset, int ms, unsigned int blk, long tag)
{
	short buf[MAXBLK];
	long total = (long)(fs * ms / 1000.0);
	long done = 0;
	int edges = 0;

	while (done < total) {
		unsigned int n = blk;

		if ((long)n > total - done)
			n = (unsigned int)(total - done);
		if (amp == 0.0 && offset == 0.0)
			fill_const(buf, n, 0);
		else
			fill_sine(buf, n, phase, freq, fs, amp, offset);
		if (pair_block(p, buf, n, tag))
			edges++;
		done += n;
	}
	return edges;
}

/* ---------------------------------------------------------------- */

static void
cfg_default(struct ring_detector_cfg *cfg, int fs, int thr)
{
	cfg->fs = fs;
	cfg->min_freq = 15;
	cfg->max_freq = 80;
	cfg->min_on_dur = 120;
	cfg->min_off_dur = 120;
	cfg->threshold = thr;
}

/* ---------------------------------------------------------------- */

static int
t_reset(void)
{
	static const int fss[] = { 80, 160, 7200, 8000, 9600 };
	static const int minfs[] = { 1, 13, 14, 50 };
	static const int maxfs[] = { 50, 100, 101, 300 };
	static const int ons[] = { 0, 39, 40, 500 };
	static const int offs[] = { 0, 119, 120, 500 };
	static const int thrs[] = { -32000, -120, -1, 0, 1, 500, 32000 };
	unsigned int a, b, c, d, e, f, lvl;
	long cases = 0;

	set_level(0);
	diff_begin("RingDetector_Reset over the clamp/sign grid x 2 levels");
	for (lvl = 0; lvl < 2; lvl++) {
		for (a = 0; a < 5; a++)
		for (b = 0; b < 4; b++)
		for (c = 0; c < 4; c++)
		for (d = 0; d < 4; d++)
		for (e = 0; e < 4; e++)
		for (f = 0; f < 7; f++) {
			struct ring_detector_cfg cfg;
			struct ring_detector sa, sb;

			/*
			 * The level-2 pass would print ~18k transcript
			 * lines over the full grid; the gate is the same
			 * code whatever the values, so the printing pass
			 * runs on the threshold sweep only.
			 */
			if (lvl && (a | b | c | d | e))
				continue;
			set_level(lvl ? 2 : 0);
			cfg.fs = fss[a];
			cfg.min_freq = minfs[b];
			cfg.max_freq = maxfs[c];
			cfg.min_on_dur = ons[d];
			cfg.min_off_dur = offs[e];
			cfg.threshold = thrs[f];

			memset(&sa, 0xa5, sizeof(sa));
			memcpy(&sb, &sa, sizeof(sa));
			ref_RingDetector_Reset(&sa, &cfg);
			RingDetector_Reset(&sb, &cfg);
			diff_eq_obj("state after reset",
				    struct ring_detector, &sb, &sa, cases);
			cases++;
		}
	}
	set_level(0);

	/*
	 * One configuration with the transcripts captured: the level-2 line
	 * carries six formatted values, so comparing the text compares them
	 * all, and the counts prove the gate fired rather than idled.
	 */
	{
		struct ring_detector_cfg cfg;
		struct ring_detector sa, sb;

		cfg.fs = 8000;
		cfg.min_freq = 20;
		cfg.max_freq = 90;
		cfg.min_on_dur = 60;
		cfg.min_off_dur = 200;
		cfg.threshold = -450;
		memset(&sa, 0xa5, sizeof(sa));
		memcpy(&sb, &sa, sizeof(sa));
		set_level(2);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		ref_RingDetector_Reset(&sa, &cfg);
		RingDetector_Reset(&sb, &cfg);
		dsplib_debug_capture_on = 0;
		set_level(0);
		diff_eq_int("transcript lines ours",
			    dsplib_debug_capture_lines(0), 1, 0);
		diff_eq_int("transcript lines ref",
			    dsplib_debug_capture_lines(1), 1, 0);
		diff_eq_int("transcript text equal",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)), 0, 0);
		diff_eq_obj("state after captured reset",
			    struct ring_detector, &sb, &sa, cases);
	}
	fprintf(stderr, "t_ringdet: reset over %ld configurations\n", cases);
	return diff_end();
}

/*
 * Create is malloc + Reset, and Delete is a guarded free.  The grid is
 * smaller than t_reset's because Reset is already swept there; what is being
 * checked here is the allocation size, the absence of a NULL check, and the
 * transcript, which Create shares with Reset.
 */
static int
t_create_delete(void)
{
	static const int fss[] = { 8000, 9600 };
	static const int thrs[] = { -3000, -650, 0, 650, 1000, 32000 };
	unsigned int a, f;
	long cases = 0;

	set_level(0);
	diff_begin("RingDetector_Create / _Delete");
	for (a = 0; a < 2; a++)
	for (f = 0; f < 6; f++) {
		struct ring_detector_cfg cfg;
		struct pair p;

		cfg_default(&cfg, fss[a], thrs[f]);
		harness_alloc_reset();
		pair_create(&p, &cfg, cases);
		diff_eq_int("create asked for 0x54 bytes (%ld)",
			    (int)harness_alloc_reqsize(p.ref), 0x54, cases);
		pair_free(&p);
		diff_eq_int("both blocks freed (%ld)",
			    harness_alloc.live, 0, cases);
		cases++;
	}

	/*
	 * Delete(NULL) reaches no free.  `sysdep_free` counts the null frees
	 * that get to it, so a delete that dropped its `if (s)` would show
	 * here even though free(NULL) is otherwise harmless.
	 */
	{
		int nulls = harness_alloc.free_null;

		ref_RingDetector_Delete(0);
		RingDetector_Delete(0);
		diff_eq_int("Delete(NULL) reaches no free",
			    harness_alloc.free_null, nulls, 0);
	}

	/* GetLastRing is two loads; sweep the sign and the width of both. */
	{
		static const int fq[] = { -32768, -1, 0, 1, 25, 32767 };
		static const int du[] = { -1000000, -1, 0, 1, 120, 1000000 };
		unsigned int i, j;

		for (i = 0; i < 6; i++)
		for (j = 0; j < 6; j++) {
			struct ring_detector sa, sb;
			int fa = 0x5a5a, da = 0x5a5a, fb = 0x5a5a, db = 0x5a5a;

			memset(&sa, 0xa5, sizeof(sa));
			sa.last_freq = (short)fq[i];
			sa.last_duration = du[j];
			memcpy(&sb, &sa, sizeof(sa));
			ref_RingDetector_GetLastRing(&sa, &fb, &db);
			RingDetector_GetLastRing(&sb, &fa, &da);
			diff_eq_int("GetLastRing freq (%ld)", fa, fb, cases);
			diff_eq_int("GetLastRing duration (%ld)", da, db,
				    cases);
			diff_eq_obj("GetLastRing wrote nothing",
				    struct ring_detector, &sb, &sa, cases);
			cases++;
		}
	}
	fprintf(stderr, "t_ringdet: create/delete over %ld cases\n", cases);
	return diff_end();
}

/*
 * The detector proper.  Every scenario is a cadence built out of tone and
 * silence at a chosen frequency, amplitude, offset and block size, and every
 * block is compared in full.
 */
static int
t_process(void)
{
	static const unsigned int blks[] = { 1, 7, 160, 1024 };
	static const int thrs[] = { 1000, -3000, 650 };
	static const int fss[] = { 8000, 9600 };
	unsigned int bi, ti, si;
	long cases = 0;

	set_level(0);
	diff_begin("RingDetector_Process over cadences, rates and block sizes");

	for (si = 0; si < 2; si++)
	for (ti = 0; ti < 3; ti++)
	for (bi = 0; bi < 4; bi++) {
		struct ring_detector_cfg cfg;
		struct pair p;
		double phase = 0.0;
		int fs = fss[si];

		cfg_default(&cfg, fs, thrs[ti]);
		pair_create(&p, &cfg, cases);

		/* Two full ring cadences: 400 ms on, 300 ms off, twice. */
		run_tone(&p, &phase, 25.0, fs, 8000.0, 0.0, 400, blks[bi],
			 cases);
		run_tone(&p, &phase, 25.0, fs, 0.0, 0.0, 300, blks[bi], cases);
		run_tone(&p, &phase, 25.0, fs, 8000.0, 0.0, 400, blks[bi],
			 cases);
		run_tone(&p, &phase, 25.0, fs, 0.0, 0.0, 300, blks[bi], cases);

		/* A tone below the band and one above it: neither is a ring. */
		run_tone(&p, &phase, 5.0, fs, 8000.0, 0.0, 400, blks[bi],
			 cases);
		run_tone(&p, &phase, 400.0, fs, 8000.0, 0.0, 300, blks[bi],
			 cases);
		run_tone(&p, &phase, 25.0, fs, 0.0, 0.0, 300, blks[bi], cases);

		/* Both edges of the pass band. */
		run_tone(&p, &phase, 14.0, fs, 9000.0, 0.0, 500, blks[bi],
			 cases);
		run_tone(&p, &phase, 25.0, fs, 0.0, 0.0, 300, blks[bi], cases);
		run_tone(&p, &phase, 100.0, fs, 9000.0, 0.0, 400, blks[bi],
			 cases);
		run_tone(&p, &phase, 25.0, fs, 0.0, 0.0, 300, blks[bi], cases);

		/*
		 * Amplitudes around the comparator's two levels: below the
		 * crossing level, between it and the threshold (which is the
		 * guard band and nothing else reaches), and just over.
		 */
		run_tone(&p, &phase, 25.0, fs, 60.0, 0.0, 200, blks[bi],
			 cases);
		run_tone(&p, &phase, 25.0, fs, 500.0, 0.0, 300, blks[bi],
			 cases);
		run_tone(&p, &phase, 25.0, fs, 1100.0, 0.0, 300, blks[bi],
			 cases);
		run_tone(&p, &phase, 25.0, fs, 3100.0, 0.0, 400, blks[bi],
			 cases);
		run_tone(&p, &phase, 25.0, fs, 0.0, 0.0, 300, blks[bi], cases);

		/* A ring riding on a DC offset, and one that clips. */
		run_tone(&p, &phase, 25.0, fs, 6000.0, 2000.0, 400, blks[bi],
			 cases);
		run_tone(&p, &phase, 25.0, fs, 0.0, 0.0, 300, blks[bi], cases);
		run_tone(&p, &phase, 25.0, fs, 40000.0, 0.0, 400, blks[bi],
			 cases);
		run_tone(&p, &phase, 25.0, fs, 0.0, 0.0, 300, blks[bi], cases);

		pair_free(&p);
		cases++;
	}

	/*
	 * A square wave of amplitude EXACTLY the threshold.  SEARCH latches on
	 * `sample > threshold`, so this waveform must never latch; a `>=`
	 * there latches on the first block and every later comparison moves.
	 * Nothing generated from a sine reaches this, because a sine's samples
	 * land on the level only by accident.
	 */
	{
		struct ring_detector_cfg cfg;
		struct pair p;
		short buf[MAXBLK];
		int i;

		cfg_default(&cfg, 8000, 1000);
		pair_create(&p, &cfg, cases);
		for (i = 0; i < 50; i++) {
			fill_const(buf, 160, i & 1 ? 1000 : -1000);
			pair_block(&p, buf, 160, cases);
		}
		diff_eq_int("a tone at exactly the threshold never latches",
			    p.ref->state, RD_STATE_SEARCH, cases);
		pair_free(&p);
		cases++;
	}

	/*
	 * The guard band, held long enough to time out.  Latch on a full
	 * amplitude, then sit at a DC level between the crossing level (100)
	 * and the threshold (1000): every sample is in the band, so guard_run
	 * climbs to guard_limit and the detector gives up.  Stepped one sample
	 * at a time because the whole claim is WHICH sample it gives up on,
	 * and a 160-sample block would hide a one-sample error.
	 *
	 * The latch is DC and not a tone, deliberately.  A tone leaves the
	 * comparator in whichever half cycle the last block ended in, and if
	 * that is LOW the DC first has to cross back -- which resets both
	 * counters and makes the arithmetic below depend on where the tone
	 * stopped.  From a DC latch the state is HIGH, cross_samples is a
	 * known small number, and guard_limit (400) is reached well before
	 * fs/min_freq (533) so it is the guard reset and not the timeout that
	 * fires.
	 */
	{
		struct ring_detector_cfg cfg;
		struct pair p;
		short buf[4];
		int i, saw_guard_reset = 0;

		cfg_default(&cfg, 8000, 1000);
		pair_create(&p, &cfg, cases);
		fill_const(buf, 1, 20000);
		for (i = 0; i < 20; i++)
			pair_block(&p, buf, 1, cases);
		diff_eq_int("DC over the threshold latches HIGH",
			    p.ref->state, RD_STATE_HIGH, cases);
		fill_const(buf, 1, 500);
		for (i = 0; i < 700; i++) {
			int prev = p.ref->state;

			pair_block(&p, buf, 1, cases);
			if (prev != RD_STATE_SEARCH
			    && p.ref->state == RD_STATE_SEARCH
			    && p.ref->idle_samples == 3 * 8000 / (4 * 15))
				saw_guard_reset++;
		}
		diff_eq_int("the guard band timed out and seeded idle_samples",
			    saw_guard_reset > 0, 1, cases);
		pair_free(&p);
		cases++;
	}

	/*
	 * Frequencies ON the band edges.  The measurement admits
	 * `f < max_freq + 1 && f >= min_freq`, so the two boundary values have
	 * to be produced exactly: at 8000 Hz a tone of f Hz gives a full cycle
	 * of 8000/f samples, and the estimate is fs/(cycle length), so 80 Hz
	 * and 15 Hz land on max_freq and min_freq themselves.  The neighbours
	 * are swept with them because a one-off in either bound moves which of
	 * the five is accepted.
	 */
	{
		static const double edge[] = { 14.0, 15.0, 16.0, 78.0, 79.0,
					       80.0, 81.0, 82.0 };
		unsigned int k;

		for (k = 0; k < 8; k++) {
			struct ring_detector_cfg cfg;
			struct pair p;
			double phase = 0.0;

			cfg_default(&cfg, 8000, 1000);
			pair_create(&p, &cfg, cases);
			run_tone(&p, &phase, edge[k], 8000.0, 9000.0, 0.0,
				 600, 160, cases);
			run_tone(&p, &phase, edge[k], 8000.0, 0.0, 0.0,
				 300, 160, cases);
			pair_free(&p);
			cases++;
		}
	}

	/*
	 * The closing edge's floor, which the cadences above cannot reach.
	 * A ring closes when the silence passes minOffDur, so the elapsed time
	 * at the closing edge is always at least that and the reported
	 * duration is always at least minOnDur + 20 -- the `ms < min_on_dur`
	 * clamp is unreachable by playing audio.  It IS reachable by making
	 * the silence counter old without making the report counter old, which
	 * is what the object's two separate counters allow, so idle_samples is
	 * poked past the window -- identically through `struct ring_detector *`
	 * on both sides, which is itself a check on the layout claim.
	 */
	{
		struct ring_detector_cfg cfg;
		struct pair p;
		short buf[4];
		double phase = 0.0;
		int i, fa, da, fb, db;

		cfg_default(&cfg, 8000, 1000);
		pair_create(&p, &cfg, cases);
		/* Tone until the opening edge is reported. */
		for (i = 0; i < 40 && !p.ref->ring_reported; i++) {
			short blk[160];

			fill_sine(blk, 160, &phase, 25.0, 8000.0, 8000.0, 0.0);
			pair_block(&p, blk, 160, cases);
		}
		diff_eq_int("the ring was reported before the poke",
			    p.ref->ring_reported, 1, cases);
		p.ours->idle_samples = p.ref->idle_samples = 100000;
		fill_const(buf, 1, 0);
		diff_eq_int("one silent sample now closes the ring",
			    pair_block(&p, buf, 1, cases), 1, cases);
		RingDetector_GetLastRing(p.ours, &fa, &da);
		ref_RingDetector_GetLastRing(p.ref, &fb, &db);
		diff_eq_int("and its duration is held at minOnDur", da, db,
			    cases);
		diff_eq_int("which is the floor, not the elapsed 0 ms",
			    db, p.ref->min_on_dur, cases);
		pair_free(&p);
		cases++;
	}

	/* Noise, a zero-length block, and a run at debug level 2. */
	{
		struct ring_detector_cfg cfg;
		struct pair p;
		short buf[MAXBLK];
		double phase = 0.0;
		int i;

		cfg_default(&cfg, 8000, 1000);
		pair_create(&p, &cfg, cases);
		diff_eq_int("zero-length block returns 0",
			    pair_block(&p, buf, 0, cases), 0, cases);
		for (i = 0; i < 40; i++) {
			fill_noise(buf, 160, 5000);
			pair_block(&p, buf, 160, cases);
		}
		for (i = 0; i < 40; i++) {
			fill_const(buf, 160, i & 1 ? 9000 : -9000);
			pair_block(&p, buf, 160, cases);
		}
		run_tone(&p, &phase, 25.0, 8000.0, 0.0, 0.0, 400, 160, cases);
		pair_free(&p);
		cases++;
	}

	/*
	 * The "no crossing for a whole period" reset.  A tone that stops
	 * mid-cycle leaves the comparator latched in HIGH or LOW; after
	 * fs/min_freq samples with no crossing the detector must fall back to
	 * SEARCH.  Held at a DC level well ABOVE the threshold, so the guard
	 * band is never occupied and this is the only reset that can fire --
	 * which is also how the two are told apart, because the period reset
	 * seeds idle_samples with fs/min_freq and the guard reset seeds it
	 * with guard_limit.
	 *
	 * Stepped one sample at a time because the machine RE-ARMS: SEARCH
	 * relatches after idle_debounce samples of DC and the period starts
	 * again, so a block long enough to reach the reset is also long
	 * enough to leave it, and looking only at the state after the block
	 * sees HIGH.  That is what the first version of this check did.
	 */
	{
		struct ring_detector_cfg cfg;
		struct pair p;
		short buf[4];
		double phase = 0.0;
		int before, i;

		cfg_default(&cfg, 8000, 1000);
		pair_create(&p, &cfg, cases);
		run_tone(&p, &phase, 25.0, 8000.0, 8000.0, 0.0, 400, 160,
			 cases);
		before = p.ref->state;
		diff_eq_int("a latched comparator was reached",
			    before == RD_STATE_HIGH || before == RD_STATE_LOW,
			    1, cases);
		fill_const(buf, 1, 20000);
		for (i = 0; i < 2000; i++) {
			int prev = p.ref->state;

			pair_block(&p, buf, 1, cases);
			if (prev != RD_STATE_SEARCH
			    && p.ref->state == RD_STATE_SEARCH
			    && p.ref->idle_samples == 8000 / 15)
				seen_lost++;
		}
		diff_eq_int("and DC drops it back to SEARCH by timeout",
			    seen_lost > 0, 1, cases);
		pair_free(&p);
		cases++;
	}

	fprintf(stderr, "t_ringdet: process over %ld scenarios\n", cases);
	return diff_end();
}

/*
 * The four `RD_*` entry points: the codec-type switch that picks the
 * threshold, the rate refusal, the wrapper's own two-word object, and the
 * three gated transcript lines.
 */
static int
t_rd(void)
{
	static const int codecs[] = { -1, 0, 3, 4, 5, 11, 12, 13, 14, 15, 16,
				      99 };
	unsigned int c;
	long cases = 0;
	int modem_ours = 0, modem_ref = 0;

	set_level(0);
	diff_begin("RD_create / RD_delete / RD_process / RD_ring_details");

	for (c = 0; c < 12; c++) {
		struct rd *ours, *refr;
		double phase = 0.0;
		short buf[MAXBLK];
		int i;

		harness_param_reset();
		harness_param_set(MDMPRM_CODECTYPE, codecs[c]);
		harness_alloc_reset();

		refr = (struct rd *)ref_RD_create(&modem_ref, 8000);
		ours = (struct rd *)RD_create(&modem_ours, 8000);
		diff_eq_int("RD_create returned an object (%ld)",
			    ours != 0 && refr != 0, 1, cases);
		if (!ours || !refr) {
			cases++;
			continue;
		}
		diff_eq_int("RD_create kept the modem handle (%ld)",
			    ours->modem == &modem_ours
			    && refr->modem == &modem_ref, 1, cases);
		diff_eq_int("RD_create asked for 8 bytes (%ld)",
			    (int)harness_alloc_reqsize(refr), 8, cases);
		diff_eq_int("RD_create asked the codec type (%ld)",
			    (int)harness_param_ref.last_param,
			    MDMPRM_CODECTYPE, cases);
		diff_eq_obj("the detector RD_create built",
			    struct ring_detector, ours->det, refr->det, cases);

		/* And it runs: a ring cadence through the wrapper. */
		for (i = 0; i < 60; i++) {
			int ra, rb;

			fill_sine(buf, 160, &phase, 25.0, 8000.0,
				  i < 25 ? 8000.0 : 0.0, 0.0);
			rb = ref_RD_process(refr, buf, 160);
			ra = RD_process(ours, buf, 160);
			diff_eq_int("RD_process verdict (%ld)", ra, rb, cases);
			diff_eq_obj("detector after RD_process",
				    struct ring_detector, ours->det, refr->det,
				    cases);
			if (rb) {
				int fa, da, fb, db;

				RD_ring_details(ours, &fa, &da);
				ref_RD_ring_details(refr, &fb, &db);
				diff_eq_int("RD_ring_details freq (%ld)",
					    fa, fb, cases);
				diff_eq_int("RD_ring_details duration (%ld)",
					    da, db, cases);
			}
			note_state(refr->det);
		}

		RD_delete(ours);
		ref_RD_delete(refr);
		diff_eq_int("RD_delete freed both objects (%ld)",
			    harness_alloc.live, 0, cases);
		cases++;
	}

	/* Every rate but 8000 and 9600 is refused before anything is taken. */
	{
		static const unsigned int rates[] = { 0, 1, 7999, 8000, 8001,
						      9599, 9600, 9601, 16000 };
		unsigned int i;

		harness_param_reset();
		harness_param_set(MDMPRM_CODECTYPE, 4);
		for (i = 0; i < 9; i++) {
			void *a, *b;

			harness_alloc_reset();
			b = ref_RD_create(&modem_ref, rates[i]);
			a = RD_create(&modem_ours, rates[i]);
			diff_eq_int("RD_create(%ld) accepted?",
				    a != 0, b != 0, (long)rates[i]);
			if (a)
				RD_delete(a);
			if (b)
				ref_RD_delete(b);
			diff_eq_int("nothing leaked (%ld)", harness_alloc.live,
				    0, (long)rates[i]);
		}
	}

	/*
	 * The transcripts.  RD_create, RD_delete and RD_process have one
	 * gated site each and RD_process's carries two formatted values, so
	 * capturing a cadence at level 2 compares the text of all three.
	 */
	{
		struct rd *ours, *refr;
		double phase = 0.0;
		short buf[MAXBLK];
		int i;

		harness_param_reset();
		harness_param_set(MDMPRM_CODECTYPE, 4);
		set_level(2);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		refr = (struct rd *)ref_RD_create(&modem_ref, 8000);
		ours = (struct rd *)RD_create(&modem_ours, 8000);
		for (i = 0; i < 60; i++) {
			fill_sine(buf, 160, &phase, 25.0, 8000.0,
				  i < 25 ? 8000.0 : 0.0, 0.0);
			ref_RD_process(refr, buf, 160);
			RD_process(ours, buf, 160);
		}
		RD_delete(ours);
		ref_RD_delete(refr);
		dsplib_debug_capture_on = 0;
		set_level(0);
		diff_eq_int("both sides printed the same number of lines",
			    (int)dsplib_debug_capture_lines(0),
			    (int)dsplib_debug_capture_lines(1), 0);
		diff_eq_int("and at least the create, the two edges and the "
			    "delete",
			    dsplib_debug_capture_lines(1) >= 5, 1, 0);
		diff_eq_int("transcript text equal",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)), 0, 0);
	}

	fprintf(stderr, "t_ringdet: RD_* over %ld codec types\n", cases);
	return diff_end();
}

/*
 * The coverage gate.  Every counter here is incremented from the REFERENCE
 * detector's own state during the runs above, so a zero means the samples
 * this file generates no longer reach that part of the machine -- not that
 * the machine lacks it.  Finding F134.
 */
static int
t_coverage(void)
{
	diff_begin("the scenarios above reached every arm they claim to");
	diff_eq_int("SEARCH was entered", seen_state[RD_STATE_SEARCH] > 0, 1,
		    0);
	diff_eq_int("HIGH was entered", seen_state[RD_STATE_HIGH] > 0, 1, 0);
	diff_eq_int("LOW was entered", seen_state[RD_STATE_LOW] > 0, 1, 0);
	diff_eq_int("no out-of-range state was ever seen", seen_state[0], 0, 0);
	diff_eq_int("a ring was active", seen_ring_on > 0, 1, 0);
	diff_eq_int("and inactive", seen_ring_off > 0, 1, 0);
	diff_eq_int("a start edge was reported", seen_edge_start > 0, 1, 0);
	diff_eq_int("an end edge was reported", seen_edge_end > 0, 1, 0);
	diff_eq_int("the guard band was occupied", seen_guard > 0, 1, 0);
	diff_eq_int("a frequency was measured", seen_measure > 0, 1, 0);
	diff_eq_int("cycles were counted", seen_cycles > 0, 1, 0);
	diff_eq_int("the lost-carrier reset fired", seen_lost > 0, 1, 0);
	diff_eq_int("a zero-length block was run", seen_zero_block > 0, 1, 0);
	fprintf(stderr,
		"t_ringdet: coverage search=%ld high=%ld low=%ld ring=%ld/%ld "
		"edges=%ld/%ld guard=%ld meas=%ld cyc=%ld lost=%ld\n",
		seen_state[RD_STATE_SEARCH], seen_state[RD_STATE_HIGH],
		seen_state[RD_STATE_LOW], seen_ring_on, seen_ring_off,
		seen_edge_start, seen_edge_end, seen_guard, seen_measure,
		seen_cycles, seen_lost);
	return diff_end();
}

int
main(void)
{
	int failed = 0;

	failed |= t_reset();
	failed |= t_create_delete();
	failed |= t_process();
	failed |= t_rd();
	failed |= t_coverage();
	return failed;
}

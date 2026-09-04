/*
 * t_detector.c -- differential test of `detector_create` and
 * `detector_progress` (src/service/detector.c).
 *
 * WHAT MAKES THIS HARD TO TEST HONESTLY, and what is done about each.
 *
 * 1. SEVEN OF THE NINE SYMBOLS ARE DATA, and all seven are `static` in our
 *    translation unit -- so nothing here can read our copies directly.  They
 *    are checked two ways instead: the blob's own globalized locals are
 *    compared against the literals the source claims (`tables()`, below),
 *    and every one of them is then exercised THROUGH BEHAVIOUR, because a
 *    table that is only asserted against itself proves nothing about the
 *    code that reads it.
 *
 * 2. AN UNPLANTED SUBSCRIPT IS INVISIBLE TO A BLOB-AGAINST-BLOB DRY RUN
 *    (D955/F8587): both sides read the same wild index and agree.
 *    `detector_progress` subscripts SIX arrays -- `enable[i]`, `tone_char[i]`,
 *    `status[i]`, `tone_integration_threshold[i]`, `d->tone[i]`,
 *    `d->tone_integration[i]` -- plus `lookup_table[r]`.  Every one is driven
 *    over its whole index range here, and the four thresholds are DISTINCT
 *    (25, 21, 12, 25) so a swapped subscript changes WHICH block reports.
 *
 * 3. A SINGLE BLOCK CANNOT REACH THE TONE ARM AT ALL (F8790).  The counters
 *    integrate to 26, 22, 13 and 26 blocks; every tone run here is 60 blocks
 *    long and the per-tone first-report block is asserted individually.
 *
 * 4. COVERAGE IS COUNTED FROM THE RUN, NOT FROM THE TRIAL TABLE (F134).  The
 *    arms are internal, so they are counted from the REFERENCE's own debug
 *    transcript -- one distinct format string per arm -- and `main` fails if
 *    any arm that is reachable at all fired zero times.  The denominators are
 *    printed whether or not anything failed.
 *
 * THE ONE ARM THAT IS NOT REACHABLE, AND IT IS PROVED RATHER THAN EXCUSED.
 * "false dtmf detect %d" fires when `lookup_table[r]` would be negative,
 * which needs `r` outside 0..15 and outside {-1,-2}.  `dtmf_progress` returns
 * `dtmf_test`'s verdict, which is `low + 4 * high` over two 0..3 groups, or
 * -1, or -2 -- so 0..15 and nothing else.  `probe_false_dtmf()` below asserts
 * that bound against the BLOB over the whole digit set rather than asserting
 * it from the header.
 */

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/cadence.h"
#include "dsplib/debug.h"
#include "dsplib/detector.h"
#include "dsplib/dtmf.h"
#include "dsplib/fdspkrnl.h"
#include "dsplib/modem_params.h"
#include "dsplib/toneiir.h"
#include "dsplib/vce.h"

extern struct detector *ref_detector_create(struct detector *d, void *modem,
					    detector_sreg_fn get_sreg);
extern int ref_detector_progress(struct detector *d, float *samples,
				 short count, unsigned char *out,
				 unsigned short *outlen);
extern void ref_detector_delete(struct detector *d);
extern short ref_dtmf_progress(struct dtmf *d, const float *samples,
			       short count, short mode);
extern struct dtmf *ref_create_dtmf(struct dtmf *d);
extern unsigned int ref_dsplibs_debug_level;

/* The blob's own copies of the seven file-local data symbols. */
extern int ref_status[4];
extern char ref_tone_char[4];
extern unsigned short ref_tone_integration_threshold[4];
extern float ref_tone[4];
extern struct fdsp_tone_cfg ref_TONEamode_CFG;
extern const int ref_enable[4];
extern const char ref_lookup_table[16];

#define FS		8000.0
#define TWOPI		6.283185307179586

/* One `detector_progress` call's worth of samples. */
#define BLOCK		80
#define OUTBUF		8192
#define GUARD		0x5a

/* ------------------------------------------------------------------ *
 * Per-arm coverage, counted from the reference's transcript.
 * ------------------------------------------------------------------ */
static long cov_dtmf_emit;	/* "detected dtmf %c"                 */
static long cov_dtmf_false;	/* "false dtmf detect %d"             */
static long cov_tone_report;	/* "detected %c"                      */
static long cov_busy;		/* "busy detected by cadence"         */
static long cov_dial;		/* "dial detected by cadence"         */
static long cov_dle;		/* _status's "DLE %d" -- stream mode  */
static long cov_tone_reset;	/* a counter went back to zero        */
static long cov_status_ret;	/* a non-zero return -- status mode   */
static long cov_held;		/* a block skipped by dtmf->held      */
static long cov_masked;		/* a block with a tone bit cleared    */
static long cov_create_cb;	/* creates with a non-NULL callback   */
static long cov_create_nocb;	/* creates with a NULL callback       */
static long cov_create_reuse;	/* creates handed an existing object  */
static long tone_seen[4];	/* reports, per tone index            */

/* The callbacks.  One per side, so the call counts cannot be confused. */
static long cb_calls_ours, cb_calls_ref;
static int cb_num_ours, cb_num_ref;
static unsigned int cb_value = VCE_DIALTONE_DETECT_DELAY;

static unsigned int
cb_ours(void *modem, int num)
{
	(void)modem;
	cb_calls_ours++;
	cb_num_ours = num;
	return cb_value;
}

static unsigned int
cb_ref(void *modem, int num)
{
	(void)modem;
	cb_calls_ref++;
	cb_num_ref = num;
	return cb_value;
}

/* ------------------------------------------------------------------ *
 * The object's own tables, against the literals the source claims.
 * ------------------------------------------------------------------ */
static int
tables(void)
{
	static const int want_status[4] = { 3, 4, 5, 6 };
	static const char want_char[4] = { 'e', 'c', 'a', 'f' };
	static const unsigned short want_thresh[4] = { 25, 21, 12, 25 };
	static const float want_tone[4] = {
		1300.0f, 1100.0f, 2100.0f, 2225.0f
	};
	static const int want_enable[4] = { 2, 4, 8, 0x10 };
	static const char want_lookup[16] = "147*2580369#ABCD";
	int i;

	diff_begin("detector: the object's own tables");

	for (i = 0; i < 4; i++) {
		diff_eq_int("status[%ld]", ref_status[i], want_status[i], i);
		diff_eq_int("tone_char[%ld]", ref_tone_char[i],
			    want_char[i], i);
		diff_eq_int("tone_integration_threshold[%ld]",
			    ref_tone_integration_threshold[i],
			    want_thresh[i], i);
		diff_eq_float("tone[%ld]", ref_tone[i], want_tone[i], i);
		diff_eq_int("enable[%ld]", ref_enable[i], want_enable[i], i);
	}
	for (i = 0; i < 16; i++)
		diff_eq_int("lookup_table[%ld]", ref_lookup_table[i],
			    want_lookup[i], i);

	/*
	 * The thresholds must not be all one value, or a swapped subscript
	 * would be undetectable by construction -- which is the whole reason
	 * the tone runs below assert a per-tone first-report block.
	 */
	diff_eq_int("thresholds are not all equal (%ld)",
		    ref_tone_integration_threshold[0]
		    != ref_tone_integration_threshold[2], 1, 0);

	/* TONEamode_CFG: twelve words, and the NULL fir_proto D995 warned of. */
	diff_eq_float("cfg.freq", ref_TONEamode_CFG.freq, 980.0f, 0);
	diff_eq_float("cfg.amp", ref_TONEamode_CFG.amp, 0.353599995f, 0);
	diff_eq_float("cfg.duration", ref_TONEamode_CFG.duration, 0.0f, 0);
	diff_eq_float("cfg.float_000c", ref_TONEamode_CFG.float_000c,
		      0.75f, 0);
	diff_eq_float("cfg.float_0010", ref_TONEamode_CFG.float_0010,
		      0.01f, 0);
	diff_eq_float("cfg.float_0014", ref_TONEamode_CFG.float_0014,
		      0.000199999995f, 0);
	diff_eq_float("cfg.pole_radius", ref_TONEamode_CFG.pole_radius,
		      0.9375f, 0);
	diff_eq_int("cfg.fir_proto is NULL in .data (%ld)",
		    ref_TONEamode_CFG.fir_proto == 0, 1, 0);
	diff_eq_int("cfg.fir_len", ref_TONEamode_CFG.fir_len, 53, 0);
	diff_eq_int("cfg.int_0024", ref_TONEamode_CFG.int_0024, 0, 0);
	diff_eq_int("cfg.int_0028", ref_TONEamode_CFG.int_0028, 0, 0);
	diff_eq_int("cfg.int_002c", ref_TONEamode_CFG.int_002c, 0, 0);
	diff_eq_int("sizeof fdsp_tone_cfg (%ld)",
		    (long)sizeof(struct fdsp_tone_cfg), 48, 0);

	return diff_end();
}

/*
 * The bound that makes the "false dtmf detect" arm unreachable, measured on
 * the BLOB rather than read off a header: over every digit the receiver can
 * report, `dtmf_progress`'s result is -2, -1 or 0..15.
 */
static int
probe_false_dtmf(void)
{
	static const double low_hz[4] = { 697.0, 770.0, 852.0, 941.0 };
	static const double high_hz[4] = { 1209.0, 1336.0, 1477.0, 1633.0 };
	static float buf[BLOCK];
	struct dtmf d;
	int lo, hi, blk, n, seen = 0;
	int seen_index[16];

	diff_begin("detector: dtmf_progress's result range (the blob's)");
	memset(seen_index, 0, sizeof(seen_index));

	for (hi = 0; hi < 4; hi++) {
		for (lo = 0; lo < 4; lo++) {
			double p1 = 0.0, p2 = 0.0;

			memset(&d, 0, sizeof(d));
			ref_create_dtmf(&d);
			for (blk = 0; blk < 20; blk++) {
				for (n = 0; n < BLOCK; n++) {
					if (blk < 14) {
						buf[n] = (float)
						    (0.5 * sin(p1)
						     + 0.5 * sin(p2));
						p1 += TWOPI * low_hz[lo] / FS;
						p2 += TWOPI * high_hz[hi] / FS;
					} else {
						buf[n] = 0.0f;
					}
				}
				{
					short r = ref_dtmf_progress(&d, buf,
								   BLOCK, 2);

					diff_eq_int("result in -2..15 (%ld)",
						    r >= -2 && r <= 15, 1,
						    (long)r);
					if (r >= 0 && r < 16) {
						if (!seen_index[r])
							seen++;
						seen_index[r] = 1;
					}
				}
			}
		}
	}
	/* Anti-vacuity: the sweep really did report digits, not just -2. */
	diff_eq_int("distinct digit indices seen (%ld)", seen, 16, 0);
	return diff_end();
}

/* ------------------------------------------------------------------ *
 * detector_create
 * ------------------------------------------------------------------ */

/* The country parameters cadence_create consults. */
static void
set_params(void)
{
	unsigned k;
	static const int on_off[4] = {
		GetMaxBusyCadenceOnTime, GetMinBusyCadenceOnTime,
		GetMinBusyCadenceOffTime, GetMaxBusyCadenceOffTime
	};

	harness_param_reset();
	harness_param_set(GetDialToneCallProgressFilterIndex, 1);
	harness_param_set(GetBusyToneCallProgressFilterIndex, 1);
	harness_param_set(GetCongestionToneCallProgressFilterIndex, 1);
	harness_param_set(GetRingbackToneCallProgressFilterIndex, 1);
	harness_param_set(GetDialToneFilterSubindex, 0);
	harness_param_set(GetCallProgressSamplesBufferLength, 666);
	harness_param_set(GetDialToneValidationTime, 50);
	harness_param_set(GetDialToneDetectionThreshold, 40);
	harness_param_set(GetBusyToneLooseDetectionEnabled, 0);
	harness_param_set(GetBusyDetectionCyclesNumber, 3);
	harness_param_set(GetCongestionDetectionCyclesNumber, 3);
	harness_param_set(GetRingbackDetectionCyclesNumber, 3);
	harness_param_set(GetBusyToneDiffTime, 3);
	for (k = 0; k < 4; k++)
		harness_param_set(on_off[k], (int)(60 + k * 13));
}

/*
 * Two cadence objects, word by word, skipping the five pointers and
 * `int_27c`.
 *
 * `int_27c` is `cadence_setup.w3`, and detector_create leaves that word of
 * its setup UNWRITTEN (deviation D1000) -- so each side hands
 * `cadence_create` whatever was on its own stack.  Nothing in the object
 * reads `int_27c` back, so the difference is inert; it is skipped here and
 * asserted nowhere.
 */
static void
cmp_cadence(const char *what, const struct cadence *b, const struct cadence *a,
	    long tag)
{
	int i;

	(void)what;
	for (i = 0; i < (int)(sizeof(struct cadence) / sizeof(int)); i++) {
		const int *pa = (const int *)a, *pb = (const int *)b;
		size_t off = (size_t)i * sizeof(int);

		if (off == offsetof(struct cadence, filter)
		    || off == offsetof(struct cadence, sel_a)
		    || off == offsetof(struct cadence, sel_b)
		    || off == offsetof(struct cadence, sel_scales)
		    || off == offsetof(struct cadence, name)
		    || off == offsetof(struct cadence, modem)
		    || off == offsetof(struct cadence, int_27c))
			continue;
		diff_eq_int("cadence word at +0x%lx", pb[i], pa[i], (long)off);
	}
	diff_eq_int("cadence name (%ld)", strcmp(b->name, a->name), 0, tag);

	/*
	 * AND THE WHOLE TONEIIR UNDERNEATH, not a hand-picked five.
	 *
	 * The first version of this compared `n`, `env_band`, `interval`,
	 * `threshold` and `need` -- and `env_band` is reset to zero at every
	 * verdict, so with a block that is a whole number of intervals it was
	 * ALWAYS zero on both sides.  The result was that the sample handed
	 * to `cadence_progress` was not observed at all: injecting
	 * `samples[0]` for `samples[i]`, and 32767 for the object's 16000,
	 * both passed.  `env_prev` is the field that carries an interval's
	 * level across the reset, and it is inside this loop.
	 *
	 * The three config POINTERS are skipped: each side's filter points
	 * at its own coefficient tables in the runs that do not force a
	 * passthrough.
	 */
	for (i = 0; i < (int)(sizeof(struct toneiir) / sizeof(int)); i++) {
		const int *pa = (const int *)a->filter;
		const int *pb = (const int *)b->filter;
		size_t off = (size_t)i * sizeof(int);

		if (off == offsetof(struct toneiir, cfg.a)
		    || off == offsetof(struct toneiir, cfg.b)
		    || off == offsetof(struct toneiir, cfg.scales))
			continue;
		diff_eq_int("toneiir word at +0x%lx", pb[i], pa[i], (long)off);
	}
}

/* One tone object, word by word, skipping its six pointers. */
static void
cmp_tone(const struct fdsp_tone *b, const struct fdsp_tone *a, long tag)
{
	int i;

	for (i = 0; i < (int)(sizeof(struct fdsp_tone) / sizeof(int)); i++) {
		const int *pa = (const int *)a, *pb = (const int *)b;
		size_t off = (size_t)i * sizeof(int);

		if (off == offsetof(struct fdsp_tone, fir_proto)
		    || off == offsetof(struct fdsp_tone, fir_coef)
		    || off == offsetof(struct fdsp_tone, fir_dly)
		    || off == offsetof(struct fdsp_tone, ptr_01b4)
		    || off == offsetof(struct fdsp_tone, ptr_01b8)
		    || off == offsetof(struct fdsp_tone, iir_coef))
			continue;
		diff_eq_int("tone word at +0x%lx", pb[i], pa[i], (long)off);
	}
	/* The blocks behind those pointers, by content. */
	for (i = 0; i < b->fir_len && i < a->fir_len; i++) {
		diff_eq_float("fir_proto[%ld]", b->fir_proto[i],
			      a->fir_proto[i], i);
		diff_eq_float("fir_coef[%ld]", b->fir_coef[i],
			      a->fir_coef[i], i);
		diff_eq_float("fir_dly[%ld]", b->fir_dly[i], a->fir_dly[i], i);
	}
	for (i = 0; i < 3; i++)
		diff_eq_float("iir_coef[%ld]", b->iir_coef[i],
			      a->iir_coef[i], i);
	(void)tag;
}

static void
cmp_detector(const struct detector *b, const struct detector *a, long tag)
{
	int i;

	diff_eq_int("enable (%ld)", b->enable, a->enable, tag);
	diff_eq_int("output_mode (%ld)", b->output_mode, a->output_mode, tag);
	diff_eq_int("int_002c (%ld)", b->int_002c, a->int_002c, tag);
	diff_eq_int("dialtone_detect_delay (%ld)", b->dialtone_detect_delay,
		    a->dialtone_detect_delay, tag);
	for (i = 0; i < 4; i++)
		diff_eq_int("tone_integration[%ld]", b->tone_integration[i],
			    a->tone_integration[i], i);
	diff_eq_int("cadence_0008 both NULL (%ld)",
		    (b->cadence_0008 == 0) == (a->cadence_0008 == 0), 1, tag);
	diff_eq_obj("dtmf receiver", struct dtmf, b->dtmf, a->dtmf, tag);
	for (i = 0; i < 4; i++)
		cmp_tone(b->tone[i], a->tone[i], i);
	cmp_cadence("busy", b->cadence_busy, a->cadence_busy, 0);
	cmp_cadence("dial", b->cadence_dial, a->cadence_dial, 1);
}

/*
 * Build both sides.  `reuse` runs the create a second time over the objects
 * the first one made, which is the path a caller supplying its own detector
 * takes -- every sub-object is re-initialised in place there rather than
 * replaced.
 */
static int
run_create(const char *label, unsigned int sreg, int with_cb, int reuse,
	   struct detector **ap, struct detector **bp)
{
	struct detector *a, *b;
	long ours0, ref0;

	diff_begin(label);
	set_params();
	cb_value = sreg;
	ours0 = cb_calls_ours;
	ref0 = cb_calls_ref;

	a = ref_detector_create(0, (void *)0xD00Du, with_cb ? cb_ref : 0);
	b = detector_create(0, (void *)0xD00Du, with_cb ? cb_ours : 0);
	diff_eq_int("both created (%ld)", (a != 0) && (b != 0), 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	if (reuse) {
		struct detector *a2, *b2;

		a2 = ref_detector_create(a, (void *)0xD00Du,
					 with_cb ? cb_ref : 0);
		b2 = detector_create(b, (void *)0xD00Du,
				     with_cb ? cb_ours : 0);
		diff_eq_int("reuse returns the same object (%ld)",
			    (a2 == a) && (b2 == b), 1, 0);
		cov_create_reuse++;
	}

	cmp_detector(b, a, 0);
	diff_eq_int("parameter calls (%ld)", harness_param_ours.calls,
		    harness_param_ref.calls, 0);
	diff_eq_int("callback calls (%ld)", cb_calls_ours - ours0,
		    cb_calls_ref - ref0, 0);
	if (with_cb) {
		diff_eq_int("callback asked for sreg %ld", cb_num_ours,
			    SREG_VOICE_DIALTONE_DETECT_DELAY, 0);
		diff_eq_int("callback asked for the same sreg (%ld)",
			    cb_num_ours, cb_num_ref, 0);
		/* The scale, against the object rather than against us. */
		diff_eq_int("delay is sreg * 50 / 4 (%ld)",
			    a->dialtone_detect_delay,
			    (long)((sreg * 50u) / 4u), (long)sreg);
		cov_create_cb++;
	} else {
		diff_eq_int("no callback leaves the delay 0 (%ld)",
			    a->dialtone_detect_delay, 0, 0);
		cov_create_nocb++;
	}
	/* The created defaults, asserted against the BLOB's values. */
	diff_eq_int("created enable is 0x3f (%ld)", a->enable,
		    DETECTOR_ENABLE_ALL, 0);
	diff_eq_int("created output_mode is IN_STREAM (%ld)", a->output_mode,
		    DETECTOR_OUTPUT_IN_STREAM, 0);
	diff_eq_int("created cadence_0008 is NULL (%ld)",
		    a->cadence_0008 == 0, 1, 0);
	/* Each resonator got ITS OWN frequency out of the table. */
	{
		int i;

		for (i = 0; i < 4; i++)
			diff_eq_float("tone[%ld] freq", a->tone[i]->freq,
				      ref_tone[i], i);
		diff_eq_float("tone[0] pole_radius", a->tone[0]->pole_radius,
			      0.9375f, 0);
		/* The NULL fir_proto is patched from TONE_CFG's, not kept. */
		diff_eq_int("fir_proto is TONE_CFG's (%ld)",
			    a->tone[0]->fir_proto == TONE_CFG.fir_proto
			    || b->tone[0]->fir_proto == TONE_CFG.fir_proto,
			    1, 0);
		diff_eq_int("fir_len is 53 (%ld)", a->tone[0]->fir_len, 53, 0);
	}
	/* And the two cadences are BUSY and DIAL, in that order. */
	diff_eq_int("busy cadence is continuous=0 (%ld)",
		    a->cadence_busy->continuous, 0, 0);
	diff_eq_int("the two cadences differ (%ld)",
		    a->cadence_busy->buflen != a->cadence_dial->buflen
		    || a->cadence_busy->max_on != a->cadence_dial->max_on, 1,
		    0);

	*ap = a;
	*bp = b;
	return diff_end();
}

/* ------------------------------------------------------------------ *
 * detector_progress
 * ------------------------------------------------------------------ */

/*
 * A passthrough IIR, so a cadence's toneiir returns TONEIIR_PRESENT for any
 * non-silent input.  b = unity in Q13, a = 0, no interstage shifts.  Both
 * sides are pointed at THESE arrays, so the filter is identical on both --
 * the point is to reach `cadence_progress`'s detect path deterministically,
 * not to test the filter (t_cadence does that).
 */
static const short pass_b[IIR_FILTER_COEFF] = {
	IIR_FILTER_ONE, 0, 0, IIR_FILTER_ONE, 0, 0,
	IIR_FILTER_ONE, 0, 0, IIR_FILTER_ONE, 0, 0
};
static const short pass_a[IIR_FILTER_COEFF] = { 0 };
static const short pass_scales[IIR_FILTER_SCALES] = { 0 };

static void
force_cadence(struct cadence *c, int on)
{
	c->continuous = on;
	c->filter->cfg.a = pass_a;
	c->filter->cfg.b = pass_b;
	c->filter->cfg.scales = pass_scales;
	c->filter->cfg.interval = 16;
	c->filter->cfg.threshold = 0;
	c->filter->cfg.stability = 0;
	c->filter->need = 0;
	c->filter->n = 0;
	c->filter->env_band = 0;
	c->filter->env_in = 0;
	c->filter->env_prev = 0;
	c->filter->total = 0;
	c->filter->run = 0;
}

/* Count how many times each format string fired on the REFERENCE side. */
static void
tally(const char *text)
{
	const char *p;

	for (p = text; *p != '\0'; ) {
		const char *nl = strchr(p, '\n');
		size_t n = (nl != 0) ? (size_t)(nl - p) : strlen(p);

		if (strncmp(p, "detected dtmf ", 14) == 0)
			cov_dtmf_emit++;
		else if (strncmp(p, "false dtmf detect ", 18) == 0)
			cov_dtmf_false++;
		else if (strncmp(p, "detected ", 9) == 0)
			cov_tone_report++;
		else if (strncmp(p, "busy detected by cadence", 24) == 0)
			cov_busy++;
		else if (strncmp(p, "dial detected by cadence", 24) == 0)
			cov_dial++;
		else if (strncmp(p, "DLE ", 4) == 0)
			cov_dle++;
		p += n;
		if (nl == 0)
			break;
		p++;
	}
}

/*
 * SIGNAL SHAPES.  Each one is chosen for the arm it drives.
 *
 * WHICH WAY ROUND `TONE_detect` REPORTS, because it is the opposite of what
 * the name suggests and the first fixture here had it backwards.  Its biquad
 * is a NOTCH at the tone frequency, `float_005c` smooths the energy the notch
 * REMOVED, and the verdict is `e_tot * 0.75 >= e_res` -- so 1 means the notch
 * took out less than three quarters of the band energy, i.e. NO tone, and
 * **0 means the tone is there**.  `detector_progress` integrates the ZEROS,
 * which is what makes that reading the only consistent one.
 *
 *   SIG_SILENCE  e_tot falls under `float_0014` and the verdict is 2, which
 *                RESETS the counters -- an arm a tone run never reaches.
 *   SIG_TONE     one of the four frequencies, driven alone, so exactly one
 *                counter climbs and the subscripts are pinned individually.
 *   SIG_NOISE    broadband, for the DTMF and cadence arms.
 *   SIG_DTMF     a DTMF pair, then silence, so a digit is reported.
 */
#define SIG_SILENCE	0
#define SIG_NOISE	1
#define SIG_DTMF	2
#define SIG_TONE	3

/* Which of the four `tone[]` frequencies SIG_TONE emits. */
static int sig_tone_idx;

static unsigned long lcg_state;

static float
noise(void)
{
	lcg_state = lcg_state * 1103515245UL + 12345UL;
	return (float)((long)((lcg_state >> 12) & 0xffffUL) - 32768L)
	    / 40000.0f;
}

static void
fill(float *buf, int shape, int blk, double *p1, double *p2)
{
	int n;

	for (n = 0; n < BLOCK; n++) {
		switch (shape) {
		case SIG_SILENCE:
			buf[n] = 0.0f;
			break;
		case SIG_NOISE:
			buf[n] = noise();
			break;
		case SIG_TONE:
			buf[n] = (float)(0.8 * sin(*p1));
			*p1 += TWOPI * (double)ref_tone[sig_tone_idx] / FS;
			break;
		default:
			/*
			 * A different digit every twenty blocks, and the
			 * three are chosen for the RANGE TEST: index 15 is
			 * the only value `r > 15` and `r > 14` disagree on,
			 * and index 0 is the only one `lookup_table[r]` and
			 * `lookup_table[15 - r]` cannot be told apart by
			 * without it.  Without 15 in the sequence, narrowing
			 * the object's bound by one is undetectable.
			 */
			if (blk % 20 < 14) {
				static const double low_hz[4] = {
					697.0, 770.0, 852.0, 941.0
				};
				static const double high_hz[4] = {
					1209.0, 1336.0, 1477.0, 1633.0
				};
				static const int seq[3] = { 15, 0, 9 };
				int k = seq[(blk / 20) % 3];

				buf[n] = (float)(0.4 * sin(*p1)
						 + 0.4 * sin(*p2));
				*p1 += TWOPI * low_hz[k & 3] / FS;
				*p2 += TWOPI * high_hz[k >> 2] / FS;
			} else {
				buf[n] = 0.0f;
			}
			break;
		}
	}
}

/*
 * One multi-block run.  `blocks` calls of `detector_progress`, comparing the
 * return, the whole output buffer, the byte count and the whole object graph
 * after EVERY block -- a difference that appears on block 27 and is gone by
 * block 60 is exactly what a one-block fixture cannot see (F8790).
 */
static int
run_progress(const char *label, int shape, int blocks, int mask, int stream,
	     int busy_on, int dial_on, int held, int level)
{
	static float buf[BLOCK];
	static unsigned char outa[OUTBUF], outb[OUTBUF];
	struct detector *a, *b;
	unsigned short lena = 0, lenb = 0;
	double p1 = 0.0, p2 = 0.0;
	int blk, i, rc;
	int prev_int[4];

	diff_begin(label);
	set_params();
	cb_value = VCE_DIALTONE_DETECT_DELAY;
	lcg_state = 0x12345678UL;

	a = ref_detector_create(0, (void *)0xD00Du, cb_ref);
	b = detector_create(0, (void *)0xD00Du, cb_ours);
	if (a == 0 || b == 0) {
		diff_eq_int("both created (%ld)", 0, 1, 0);
		return diff_end();
	}

	a->enable = (unsigned short)mask;
	b->enable = (unsigned short)mask;
	a->output_mode = stream;
	b->output_mode = stream;
	a->dtmf->held = (short)held;
	b->dtmf->held = (short)held;
	/*
	 * ONLY where the run wants it.  Forcing BOTH cadences in every run
	 * made the dial arm fire on nearly every block, and since it runs
	 * LAST it overwrote `ret` -- so the tone arm's `ret = status[i]` was
	 * never observed and a rotated `status[]` passed the whole file.
	 * Each detector is now forced separately, and the busy-only run is
	 * what makes `ret = 1` distinguishable from `ret = 2`.
	 */
	if (busy_on) {
		force_cadence(a->cadence_busy, 1);
		force_cadence(b->cadence_busy, 1);
	}
	if (dial_on) {
		force_cadence(a->cadence_dial, 1);
		force_cadence(b->cadence_dial, 1);
	}
	if (mask != DETECTOR_ENABLE_ALL)
		cov_masked++;
	if (held)
		cov_held++;

	memset(outa, GUARD, sizeof(outa));
	memset(outb, GUARD, sizeof(outb));
	for (i = 0; i < 4; i++)
		prev_int[i] = 0;

	if (level) {
		dsplibs_debug_level = ref_dsplibs_debug_level =
		    (unsigned int)level;
		dsplib_debug_capture_on = 1;
	}

	for (blk = 0; blk < blocks; blk++) {
		int ra, rb;

		if (level)
			dsplib_debug_capture_reset();
		fill(buf, shape, blk, &p1, &p2);
		/*
		 * `out` is advanced by the DTMF arm and by nothing else, so
		 * each side is handed its buffer BASE plus its own count --
		 * which is what a real caller does and what makes the
		 * cursor advance observable.
		 */
		ra = ref_detector_progress(a, buf, BLOCK, outa + lena, &lena);
		rb = detector_progress(b, buf, BLOCK, outb + lenb, &lenb);

		diff_eq_int("block %ld: return", rb, ra, blk);
		diff_eq_int("block %ld: outlen", lenb, lena, blk);
		diff_eq_int("block %ld: output bytes",
			    memcmp(outa, outb, sizeof(outa)) == 0, 1, blk);
		cmp_detector(b, a, blk);
		if (ra != 0)
			cov_status_ret++;
		for (i = 0; i < 4; i++) {
			if (prev_int[i] > 0 && a->tone_integration[i] == 0)
				cov_tone_reset++;
			prev_int[i] = a->tone_integration[i];
		}
		/*
		 * THE INVARIANT THAT PINS EVERY SUBSCRIPT IN THE TONE ARM,
		 * asserted on the BLOB's transcript block by block: tone `i`
		 * reported on this block if and only if it is enabled, the
		 * DTMF receiver is not holding, and its counter is now
		 * STRICTLY ABOVE its own threshold.
		 *
		 * `tone_integration_threshold` has four distinct values, so a
		 * swapped index moves the block this first becomes true on;
		 * `tone_char` supplies the character searched for, so a
		 * swapped index there changes which line appears; and
		 * `enable[]` decides whether the arm ran at all.  A
		 * blob-against-blob comparison could not see any of the three
		 * (D955/F8587) -- this is measured against the object's own
		 * arithmetic instead.
		 */
		if (level) {
			const char *t = dsplib_debug_capture_text(1);

			diff_eq_int("block %ld: transcripts agree",
				    strcmp(dsplib_debug_capture_text(0), t)
				    == 0, 1, blk);
			diff_eq_int("block %ld: transcript line counts",
				    (int)dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1), blk);
			for (i = 0; i < 4 && level > 1; i++) {
				char pat[16];
				int want, got;

				pat[0] = 'd'; pat[1] = 'e'; pat[2] = 't';
				pat[3] = 'e'; pat[4] = 'c'; pat[5] = 't';
				pat[6] = 'e'; pat[7] = 'd'; pat[8] = ' ';
				pat[9] = ref_tone_char[i];
				pat[10] = '\n'; pat[11] = '\0';
				got = strstr(t, pat) != 0;
				/*
				 * `held` is read from the OBJECT and not from
				 * this function's argument: the DTMF arm runs
				 * first and `dtmf_progress` rewrites the very
				 * field the tone arm's gate then reads, so a
				 * planted 1 does not survive the block it was
				 * planted for.
				 */
				want = a->dtmf->held == 0
				    && (mask & ref_enable[i]) != 0
				    && a->tone_integration[i]
				       > ref_tone_integration_threshold[i];
				diff_eq_int("block %ld: tone report iff "
					    "counter > threshold", got, want,
					    blk);
				if (got)
					tone_seen[i]++;
			}
			/*
			 * Every gate in these two functions is `> 1`, so
			 * level 1 must print NOTHING -- which is also what
			 * makes the level-1 run a check rather than a hole in
			 * the invariant above.
			 */
			if (level == 1)
				diff_eq_int("block %ld: level 1 is silent",
					    (int)dsplib_debug_capture_lines(1),
					    0, blk);
			tally(t);
		}
	}

	if (level) {
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		dsplib_debug_capture_on = 0;
	}

	/* The guard past the written region is untouched on both sides. */
	for (i = (int)lena; i < (int)lena + 16 && i < OUTBUF; i++) {
		diff_eq_int("guard past out[%ld] (ref)", outa[i], GUARD, i);
		diff_eq_int("guard past out[%ld] (ours)", outb[i], GUARD, i);
	}

	rc = diff_end();
	ref_detector_delete(a);
	detector_delete(b);
	return rc;
}

int
main(void)
{
	struct detector *a, *b;
	int failed = 0;
	int i;

	harness_alloc_reset();

	failed |= tables();
	failed |= probe_false_dtmf();

	/* ---- detector_create ---- */
	failed |= run_create("detector_create: allocating, callback present",
			     VCE_DIALTONE_DETECT_DELAY, 1, 0, &a, &b);
	if (a != 0) { ref_detector_delete(a); detector_delete(b); }

	failed |= run_create("detector_create: allocating, no callback",
			     0, 0, 0, &a, &b);
	if (a != 0) { ref_detector_delete(a); detector_delete(b); }

	/*
	 * The *50/4 scale, over values where truncation bites: 1 -> 12,
	 * 3 -> 37, 7 -> 87.  A `* 12` or a `* 25 / 2` would agree with the
	 * object on some of these and not on all.
	 */
	for (i = 0; i < 6; i++) {
		static const unsigned int sregs[6] = { 0, 1, 3, 7, 30, 255 };

		failed |= run_create("detector_create: the delay scale",
				     sregs[i], 1, 0, &a, &b);
		if (a != 0) { ref_detector_delete(a); detector_delete(b); }
	}

	failed |= run_create("detector_create: re-created in place",
			     VCE_DIALTONE_DETECT_DELAY, 1, 1, &a, &b);
	if (a != 0) { ref_detector_delete(a); detector_delete(b); }

	/* ---- detector_progress ---- */

	/*
	 * ONE TONE AT A TIME, SIXTY BLOCKS EACH.  Sixty is not a round number
	 * picked for comfort: the counters have to reach 26, 22, 13 and 26
	 * before anything is reported at all, so no fixture shorter than
	 * about thirty blocks can enter this arm (F8790).  Each run drives
	 * exactly one of the four frequencies, and the per-block invariant
	 * inside `run_progress` then pins `enable[]`, `tone_char[]`,
	 * `status[]` and `tone_integration_threshold[]` by INDEX.
	 */
	for (i = 0; i < 4; i++) {
		sig_tone_idx = i;
		failed |= run_progress("detector_progress: tone arm, stream "
				       "mode", SIG_TONE, 60,
				       DETECTOR_ENABLE_ALL,
				       DETECTOR_OUTPUT_IN_STREAM, 0, 0, 0, 2);
		failed |= run_progress("detector_progress: tone arm, status "
				       "mode", SIG_TONE, 60,
				       DETECTOR_ENABLE_ALL,
				       DETECTOR_OUTPUT_STATUS, 0, 0, 0, 2);
	}

	/* Silence: TONE_detect returns 2, the counters are held at zero. */
	failed |= run_progress("detector_progress: silence resets the counters",
			       SIG_SILENCE, 40, DETECTOR_ENABLE_ALL,
			       DETECTOR_OUTPUT_IN_STREAM, 0, 0, 0, 2);

	/* A tone that stops: the counter climbs, reports, then is reset. */
	sig_tone_idx = 2;
	failed |= run_progress("detector_progress: noise after a tone",
			       SIG_NOISE, 40, DETECTOR_ENABLE_ALL,
			       DETECTOR_OUTPUT_IN_STREAM, 0, 0, 0, 2);

	/* DTMF digits, in both output modes. */
	failed |= run_progress("detector_progress: dtmf arm, stream mode",
			       SIG_DTMF, 60, DETECTOR_ENABLE_ALL,
			       DETECTOR_OUTPUT_IN_STREAM, 0, 0, 0, 2);
	failed |= run_progress("detector_progress: dtmf arm, status mode",
			       SIG_DTMF, 60, DETECTOR_ENABLE_ALL,
			       DETECTOR_OUTPUT_STATUS, 0, 0, 0, 2);
	/*
	 * A DIGIT AND A CADENCE EVENT IN THE SAME BLOCK, which is the only
	 * arrangement under which the DTMF arm's cursor advance is visible
	 * at all: with nothing writing after it, dropping `out += 2` changes
	 * nothing a caller can see, and the run above passes either way.
	 * See F8805.
	 */
	failed |= run_progress("detector_progress: a digit and a cadence "
			       "event in one block", SIG_DTMF, 60,
			       DETECTOR_ENABLE_ALL,
			       DETECTOR_OUTPUT_IN_STREAM, 1, 0, 0, 2);

	/*
	 * The cadence arm, in four combinations.  BUSY ALONE is the one that
	 * matters most: the dial detector runs second and its `ret = 2`
	 * overwrites the busy arm's `ret = 1`, so a run with both forced
	 * cannot tell the two status codes apart.
	 */
	failed |= run_progress("detector_progress: busy cadence, stream mode",
			       SIG_NOISE, 20, DETECTOR_ENABLE_ALL,
			       DETECTOR_OUTPUT_IN_STREAM, 1, 0, 0, 2);
	failed |= run_progress("detector_progress: busy cadence, status mode",
			       SIG_NOISE, 20, DETECTOR_ENABLE_ALL,
			       DETECTOR_OUTPUT_STATUS, 1, 0, 0, 2);
	failed |= run_progress("detector_progress: dial cadence, status mode",
			       SIG_NOISE, 20, DETECTOR_ENABLE_ALL,
			       DETECTOR_OUTPUT_STATUS, 0, 1, 0, 2);
	failed |= run_progress("detector_progress: both cadences, stream mode",
			       SIG_NOISE, 20, DETECTOR_ENABLE_ALL,
			       DETECTOR_OUTPUT_IN_STREAM, 1, 1, 0, 2);

	/*
	 * `dtmf->held` holds the whole tone arm off -- driven with the tone
	 * that would otherwise report soonest, so the gate is the only reason
	 * nothing appears.  The DTMF bit is CLEARED for this run: leave it on
	 * and `dtmf_progress` rewrites `held` on the first block, which is
	 * what the first version of this run measured instead of the gate.
	 */
	sig_tone_idx = 2;
	failed |= run_progress("detector_progress: held by a pending digit",
			       SIG_TONE, 40,
			       DETECTOR_ENABLE_ALL & ~DETECTOR_ENABLE_DTMF,
			       DETECTOR_OUTPUT_IN_STREAM, 0, 0, 1, 2);
	/* And the same tone with the gate open, so the run is not vacuous. */
	failed |= run_progress("detector_progress: the same tone, not held",
			       SIG_TONE, 40,
			       DETECTOR_ENABLE_ALL & ~DETECTOR_ENABLE_DTMF,
			       DETECTOR_OUTPUT_IN_STREAM, 0, 0, 0, 2);

	/*
	 * The enable mask, one bit at a time, driven with 2100 Hz -- so the
	 * ONLY mask under which a tone line may appear is 0x08.  That is what
	 * pins `enable[]`'s subscript: a rotated table would report under
	 * 0x04 or 0x10 instead, and every other check in this file would
	 * still pass.
	 */
	{
		static const int masks[8] = {
			0, DETECTOR_ENABLE_DTMF, DETECTOR_ENABLE_1300,
			DETECTOR_ENABLE_1100, DETECTOR_ENABLE_2100,
			DETECTOR_ENABLE_2225, DETECTOR_ENABLE_CADENCE,
			DETECTOR_ENABLE_1300 | DETECTOR_ENABLE_CADENCE
		};

		sig_tone_idx = 2;
		for (i = 0; i < 8; i++)
			failed |= run_progress("detector_progress: one enable "
					       "bit at a time", SIG_TONE, 40,
					       masks[i],
					       DETECTOR_OUTPUT_IN_STREAM,
					       i >= 6, i >= 6, 0, 2);
	}

	/* Level 1: every debug gate here is `> 1`, so nothing is printed. */
	sig_tone_idx = 0;
	failed |= run_progress("detector_progress: level 1 is silent",
			       SIG_TONE, 40, DETECTOR_ENABLE_ALL,
			       DETECTOR_OUTPUT_IN_STREAM, 1, 1, 0, 1);

	/* ---- coverage, from the run ---- */

	/*
	 * PRINTED, not just asserted.  A coverage check that only says "more
	 * than zero" is a gate; the numbers beside it are what tell a reader
	 * whether an arm was reached once by luck or driven deliberately
	 * (findings F134, F2401 -- a detector must report its denominator).
	 */
	printf("  detector arms, counted from the REFERENCE's transcript:\n");
	printf("    dtmf emit %ld, dtmf false %ld, tone report %ld"
	       " (per tone %ld/%ld/%ld/%ld)\n",
	       cov_dtmf_emit, cov_dtmf_false, cov_tone_report,
	       tone_seen[0], tone_seen[1], tone_seen[2], tone_seen[3]);
	printf("    busy %ld, dial %ld, DLE %ld, status returns %ld,"
	       " counter resets %ld\n",
	       cov_busy, cov_dial, cov_dle, cov_status_ret, cov_tone_reset);
	printf("    creates: %ld with a callback, %ld without, %ld in place;"
	       " %ld held, %ld masked runs\n",
	       cov_create_cb, cov_create_nocb, cov_create_reuse, cov_held,
	       cov_masked);

	diff_begin("detector: per-arm coverage, counted from the run");
	diff_eq_int("dtmf emit arm fired (%ld)", cov_dtmf_emit > 0, 1,
		    cov_dtmf_emit);
	diff_eq_int("tone report arm fired (%ld)", cov_tone_report > 0, 1,
		    cov_tone_report);
	/* EVERY tone index, not just some tone -- the subscript is the point. */
	for (i = 0; i < 4; i++)
		diff_eq_int("tone[%ld] reported at least once",
			    tone_seen[i] > 0, 1, i);
	diff_eq_int("tone reset arm fired (%ld)", cov_tone_reset > 0, 1,
		    cov_tone_reset);
	diff_eq_int("busy cadence arm fired (%ld)", cov_busy > 0, 1, cov_busy);
	diff_eq_int("dial cadence arm fired (%ld)", cov_dial > 0, 1, cov_dial);
	diff_eq_int("stream-mode _status fired (%ld)", cov_dle > 0, 1,
		    cov_dle);
	diff_eq_int("status-mode return fired (%ld)", cov_status_ret > 0, 1,
		    cov_status_ret);
	diff_eq_int("held gate exercised (%ld)", cov_held > 0, 1, cov_held);
	diff_eq_int("masked enable exercised (%ld)", cov_masked > 0, 1,
		    cov_masked);
	diff_eq_int("create with a callback (%ld)", cov_create_cb > 0, 1,
		    cov_create_cb);
	diff_eq_int("create without a callback (%ld)", cov_create_nocb > 0, 1,
		    cov_create_nocb);
	diff_eq_int("create over an existing object (%ld)",
		    cov_create_reuse > 0, 1, cov_create_reuse);
	/*
	 * PROVED UNREACHABLE, not merely unvisited: `probe_false_dtmf` above
	 * measured the blob's own result range.  Asserted at ZERO so that a
	 * future change making it reachable fails here rather than passing
	 * quietly.
	 */
	diff_eq_int("false-dtmf arm is unreachable (%ld)", cov_dtmf_false, 0,
		    cov_dtmf_false);
	failed |= diff_end();

	return failed;
}

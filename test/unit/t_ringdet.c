/*
 * t_ringdet.c -- differential test of RingDetector_Reset.
 *
 * Reset is pure configuration: read a cfg block, clamp, derive, zero.  The
 * whole 0x52-byte state is compared after every call, prefilled with 0xa5
 * on both sides so an unwritten field must MATCH by being unwritten (the
 * t_dialstring argument: zero is the one filler that makes "never written"
 * look deliberate, so it is avoided).
 *
 * The grid crosses every clamp boundary, both threshold signs (the sign
 * selects a mode) and both debug levels.  fs stays >= 80 and min_freq >= 1
 * because the object divides by fs/80 and by 4*min_freq with no guard;
 * feeding it a trap is not a differential result.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/ringdet.h"

extern unsigned int ref_dsplibs_debug_level;
extern void ref_RingDetector_Reset(struct ring_detector *s,
				   struct ring_detector_cfg *c);

/* Per-side debug transcripts; see test/harness/runtime.c. */
extern int dsplib_debug_capture_on;
void dsplib_debug_capture_reset(void);
unsigned dsplib_debug_capture_lines(int side);
const char *dsplib_debug_capture_text(int side);

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

int
main(void)
{
	static const int fss[] = { 80, 160, 7200, 8000, 9600 };
	static const int minfs[] = { 1, 13, 14, 50 };
	static const int maxfs[] = { 50, 100, 101, 300 };
	static const int ons[] = { 0, 39, 40, 500 };
	static const int offs[] = { 0, 119, 120, 500 };
	static const int thrs[] = { -32000, -120, -1, 0, 1, 500, 32000 };
	unsigned int a, b, c, d, e, f, lvl;
	int failed = 0;
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
	failed |= diff_end();
	fprintf(stderr, "t_ringdet: %ld configurations\n", cases);
	return failed;
}

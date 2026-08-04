/*
 * t_fpm_mrf.c -- differential test of the multi-rate filter's setup.
 *
 * The history pointer necessarily differs between the two builds, so the
 * states are compared field by field rather than as a blob, with the buffer
 * checked by content and length instead of by address.
 *
 * Both of Bell 103's real configurations are covered (10:9 for transmit,
 * 3:10 for receive), plus the three paths through init: fresh allocation,
 * reuse of an adequate buffer, and replacement of one that is too small.
 * That last path is the one that frees, so it is where a mismatched
 * ownership model would show up.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/fpm_mrf.h"

extern unsigned int ref_dsplibs_debug_level;
extern void ref_FPM_MRF_init(void *state, const void *cfg, int fresh);
extern void ref_FPM_MRF_free(void *state);
extern short ref_B103_MRF_FILT_TX[];
extern short ref_B103_MRF_FILT_RX[];

static void
compare(const struct fpm_mrf *ours, const struct fpm_mrf *ref, const char *tag)
{
	int i;

	(void)tag;
	diff_eq_int("branches (%ld)", ours->cfg.branches, ref->cfg.branches, 0);
	diff_eq_int("decimate (%ld)", ours->cfg.decimate, ref->cfg.decimate, 0);
	diff_eq_int("taps (%ld)", ours->cfg.taps, ref->cfg.taps, 0);
	diff_eq_int("coeff ptr matches cfg (%ld)",
		    ours->cfg.coeff == ref->cfg.coeff, 1, 0);
	diff_eq_int("need (%ld)", ours->need, ref->need, 0);
	diff_eq_int("phase (%ld)", ours->phase, ref->phase, 0);
	diff_eq_int("widx (%ld)", ours->widx, ref->widx, 0);
	diff_eq_int("history_len (%ld)", ours->history_len, ref->history_len, 0);
	diff_eq_int("history allocated (%ld)", ours->history != 0, 1, 0);

	/* Contents, not address: the two allocations cannot match. */
	for (i = 0; i < ours->history_len; i++)
		diff_eq_int("history[%ld] cleared", ours->history[i],
			    ref->history[i], i);
}

static int
run(const char *label, short branches, short decimate, short *coeff,
    short taps)
{
	struct fpm_mrf a, b;
	struct fpm_mrf_cfg cfg;
	int rc;

	memset(&cfg, 0, sizeof(cfg));
	cfg.branches = branches;
	cfg.decimate = decimate;
	cfg.coeff = coeff;
	cfg.taps = taps;

	diff_begin(label);

	/* Fresh: both allocate from a zeroed state. */
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	ref_FPM_MRF_init(&a, &cfg, 1);
	FPM_MRF_init(&b, &cfg, 1);
	compare(&b, &a, "fresh");
	diff_eq_int("taps/branches (%ld)", b.history_len, taps / branches, 0);

	/* Re-init with the same size: the buffer must be reused, not replaced. */
	{
		short *keep_a = a.history, *keep_b = b.history;

		ref_FPM_MRF_init(&a, &cfg, 0);
		FPM_MRF_init(&b, &cfg, 0);
		compare(&b, &a, "reuse");
		diff_eq_int("ours reused the buffer (%ld)",
			    b.history == keep_b, 1, 0);
		diff_eq_int("reference reused too (%ld)",
			    a.history == keep_a, 1, 0);
	}

	/*
	 * Re-init needing more room: this is the path that frees the old
	 * buffer before allocating.  Shrink history_len by hand to force it.
	 */
	a.history_len = 1;
	b.history_len = 1;
	ref_FPM_MRF_init(&a, &cfg, 0);
	FPM_MRF_init(&b, &cfg, 0);
	compare(&b, &a, "regrow");

	rc = diff_end();
	ref_FPM_MRF_free(&a);
	FPM_MRF_free(&b);
	return rc;
}

int
main(void)
{
	int rc = 0;

	/* Bell 103 transmit: 7200 -> 8000. */
	rc |= run("MRF 10:9 (7200->8000)", 10, 9, ref_B103_MRF_FILT_TX, 270);
	/* Bell 103 receive: 8000 -> 2400. */
	rc |= run("MRF 3:10 (8000->2400)", 3, 10, ref_B103_MRF_FILT_RX, 90);
	/* A ratio with no remainder in taps/branches, and a degenerate 1:1. */
	rc |= run("MRF 5:4", 5, 4, ref_B103_MRF_FILT_TX, 100);
	rc |= run("MRF 1:1", 1, 1, ref_B103_MRF_FILT_RX, 32);

	/*
	 * The reallocate path, which is the one announcement in this file and
	 * had never executed.  Every case above re-inits at the SAME size, so
	 * the buffer is reused and the `history_len < per_phase` arm is never
	 * taken.  Growing per_phase -- taps 40 over 4 branches after taps 20
	 * over 4 -- takes it, and the message has no newline, unlike every
	 * other one here.
	 */
	diff_begin("FPM_MRF_init: a bigger buffer says so");
	{
		struct fpm_mrf a, b;
		struct fpm_mrf_cfg cfg;
		unsigned lines = 0;
		int lvl;

		for (lvl = 1; lvl <= 3; lvl++) {
			memset(&cfg, 0, sizeof(cfg));
			cfg.branches = 4;
			cfg.decimate = 3;
			cfg.coeff = ref_B103_MRF_FILT_TX;
			cfg.taps = 20;
			memset(&a, 0, sizeof(a));
			memset(&b, 0, sizeof(b));
			ref_FPM_MRF_init(&a, &cfg, 1);
			FPM_MRF_init(&b, &cfg, 1);

			cfg.taps = 40;          /* per_phase 5 -> 10 */
			dsplibs_debug_level = ref_dsplibs_debug_level =
				(unsigned)lvl;
			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();
			ref_FPM_MRF_init(&a, &cfg, 0);
			FPM_MRF_init(&b, &cfg, 0);
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;
			dsplib_debug_capture_on = 0;

			compare(&b, &a, "regrown");
			diff_eq_int("history grew", b.history_len, 10, lvl);
			diff_eq_int("transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, lvl);
			if (lvl == 1)
				diff_eq_int("level 1 silent",
					    (int)dsplib_debug_capture_lines(1),
					    0, lvl);
			else
				lines += dsplib_debug_capture_lines(1);
			ref_FPM_MRF_free(&a);
			FPM_MRF_free(&b);
		}
		diff_eq_int("it said something (%ld)", lines > 0, 1,
			    (long)lines);
	}
	rc |= diff_end();

	return rc;
}

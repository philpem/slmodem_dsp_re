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
#include "dsplib/fpm_mrf.h"

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

	return rc;
}

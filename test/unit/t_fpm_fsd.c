/*
 * t_fpm_fsd.c -- differential test of the demodulator's setup.
 *
 * Driven with Bell 103's real configuration, read out of B103FP_create: the
 * 15-tap channel filter and the IIR lowpass, with the same scalars.
 *
 * Buffer pointers differ between builds, so they are compared by length and
 * contents.  Both the fresh-allocate and re-init-in-place paths are covered.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_fsd.h"

extern void ref_FPM_FSD_init(void *state, const void *cfg, int fresh);
extern void ref_FPM_FSD_free(void *state);
extern short ref_B103_CHAN_INTRP[];
extern short ref_B103_IIR_LPF[];

static void
compare(const struct fpm_fsd *ours, const struct fpm_fsd *ref)
{
	int i;

	diff_eq_int("fir_taps (%ld)", ours->cfg.fir_taps, ref->cfg.fir_taps, 0);
	diff_eq_int("iir_len (%ld)", ours->cfg.iir_len, ref->cfg.iir_len, 0);
	diff_eq_int("f16 (%ld)", ours->cfg.f16, ref->cfg.f16, 0);
	diff_eq_int("f20 (%ld)", ours->f20, ref->f20, 0);
	diff_eq_int("f22 = f12/2 (%ld)", ours->f22, ref->f22, 0);
	diff_eq_int("f28 (%ld)", ours->f28, ref->f28, 0);
	diff_eq_int("f30 (%ld)", ours->f30, ref->f30, 0);
	diff_eq_int("f32 (%ld)", ours->f32, ref->f32, 0);
	diff_eq_int("f34 (%ld)", ours->f34, ref->f34, 0);
	diff_eq_int("fir pointer copied (%ld)",
		    ours->cfg.fir == ref->cfg.fir, 1, 0);
	diff_eq_int("iir pointer copied (%ld)",
		    ours->cfg.iir == ref->cfg.iir, 1, 0);

	for (i = 0; i < ours->cfg.fir_taps; i++)
		diff_eq_int("fir_hist[%ld]", ours->fir_hist[i],
			    ref->fir_hist[i], i);
	for (i = 0; i < 2 * ours->cfg.iir_len; i++)
		diff_eq_int("iir_hist[%ld]", ours->iir_hist[i],
			    ref->iir_hist[i], i);
	for (i = 0; i < ours->cfg.f16; i++)
		diff_eq_int("buf1c[%ld]", ours->buf1c[i], ref->buf1c[i], i);
}

int
main(void)
{
	struct fpm_fsd a, b;
	struct fpm_fsd_cfg cfg;
	int rc = 0;

	/* Bell 103's configuration, as B103FP_create builds it. */
	memset(&cfg, 0, sizeof(cfg));
	cfg.fir = ref_B103_CHAN_INTRP;
	cfg.fir_taps = 15;
	cfg.f06 = 4;
	cfg.iir = ref_B103_IIR_LPF;
	cfg.iir_len = 3;
	cfg.f12 = 8;
	cfg.f16 = 12;

	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));

	diff_begin("FSD init fresh");
	ref_FPM_FSD_init(&a, &cfg, 1);
	FPM_FSD_init(&b, &cfg, 1);
	compare(&b, &a);
	rc |= diff_end();

	/* Re-init in place must not reallocate, and must re-clear. */
	diff_begin("FSD init in place");
	{
		short *keep = b.fir_hist;

		b.fir_hist[0] = 0x1234;
		a.fir_hist[0] = 0x1234;
		ref_FPM_FSD_init(&a, &cfg, 0);
		FPM_FSD_init(&b, &cfg, 0);
		compare(&b, &a);
		diff_eq_int("buffer not reallocated (%ld)",
			    b.fir_hist == keep, 1, 0);
		diff_eq_int("history re-cleared (%ld)", b.fir_hist[0], 0, 0);
	}
	rc |= diff_end();

	ref_FPM_FSD_free(&a);
	FPM_FSD_free(&b);
	return rc;
}

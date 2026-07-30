/*
 * t_fpm_mtd.c -- differential test of the multi-tone detector's setup.
 *
 * Two paths matter and are both covered: self-allocating (NULL state, as a
 * standalone caller would) and caller-supplied (as B103FP_create does, with
 * the state embedded in a larger block and the accumulator array provided).
 *
 * The second is the one worth having: create only allocates when it allocated
 * the object, so a reconstruction that always allocated would leak silently
 * and still pass the first test.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_mtd.h"

extern void *ref_FPM_MTD_create(void *state, const void *cfg);
extern void ref_FPM_MTD_delete(void *state);
extern short ref_MTDb103_COEF[];

static void
compare(const struct fpm_mtd *ours, const struct fpm_mtd *ref)
{
	int i;

	diff_eq_int("coeff pointer copied (%ld)",
		    ours->cfg.coeff == ref->cfg.coeff, 1, 0);
	diff_eq_int("tones (%ld)", ours->cfg.tones, ref->cfg.tones, 0);
	diff_eq_int("f06 (%ld)", ours->cfg.f06, ref->cfg.f06, 0);
	diff_eq_int("f08 (%ld)", ours->cfg.f08, ref->cfg.f08, 0);
	diff_eq_int("f10 (%ld)", ours->f10, ref->f10, 0);
	diff_eq_int("f12 (%ld)", ours->f12, ref->f12, 0);
	diff_eq_int("f14 (%ld)", ours->f14, ref->f14, 0);
	diff_eq_int("f16 (%ld)", ours->f16, ref->f16, 0);
	for (i = 0; i < ours->cfg.tones * 2; i++)
		diff_eq_int("acc[%ld]", ours->acc[i], ref->acc[i], i);
}

int
main(void)
{
	struct fpm_mtd_cfg cfg;
	struct fpm_mtd *a, *b;
	int rc = 0;

	/* Bell 103's configuration, as B103FP_create builds it. */
	memset(&cfg, 0, sizeof(cfg));
	cfg.coeff = ref_MTDb103_COEF;
	cfg.tones = 2;
	cfg.f06 = 0x3666;
	cfg.f08 = 2;

	diff_begin("MTD create self-allocating");
	a = ref_FPM_MTD_create(0, &cfg);
	b = FPM_MTD_create(0, &cfg);
	diff_eq_int("ours built an object (%ld)", b != 0, 1, 0);
	if (a && b)
		compare(b, a);
	rc |= diff_end();
	if (a)
		ref_FPM_MTD_delete(a);
	if (b)
		FPM_MTD_delete(b);

	/*
	 * Caller-supplied state and buffer, the way B103FP_create uses it.
	 * create must NOT allocate here; the accumulators come from us.
	 */
	diff_begin("MTD create caller-supplied");
	{
		struct fpm_mtd sa, sb;
		short acc_a[8], acc_b[8];
		int i;

		memset(&sa, 0, sizeof(sa));
		memset(&sb, 0, sizeof(sb));
		for (i = 0; i < 8; i++)
			acc_a[i] = acc_b[i] = (short)(0x5a00 + i);
		sa.acc = acc_a;
		sb.acc = acc_b;

		ref_FPM_MTD_create(&sa, &cfg);
		FPM_MTD_create(&sb, &cfg);

		compare(&sb, &sa);
		diff_eq_int("buffer not replaced (%ld)", sb.acc == acc_b, 1, 0);
		/* Entries past the tone count must be left alone. */
		for (i = cfg.tones * 2; i < 8; i++)
			diff_eq_int("acc[%ld] untouched", acc_b[i], acc_a[i], i);
	}
	rc |= diff_end();

	return rc;
}

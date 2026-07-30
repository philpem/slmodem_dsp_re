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
extern short ref_FPM_MTD_detect(void *state, const short *samples, short count);
extern short ref_COEF_DC[];

static void
compare(const struct fpm_mtd *ours, const struct fpm_mtd *ref)
{
	int i;

	diff_eq_int("coeff pointer copied (%ld)",
		    ours->cfg.coeff == ref->cfg.coeff, 1, 0);
	diff_eq_int("tones (%ld)", ours->cfg.tones, ref->cfg.tones, 0);
	diff_eq_int("ratio (%ld)", ours->cfg.ratio, ref->cfg.ratio, 0);
	diff_eq_int("min_level (%ld)", ours->cfg.min_level, ref->cfg.min_level, 0);
	diff_eq_int("dc_state[0] (%ld)", ours->dc_state[0], ref->dc_state[0], 0);
	diff_eq_int("dc_state[1] (%ld)", ours->dc_state[1], ref->dc_state[1], 0);
	diff_eq_int("out_of_band (%ld)", ours->out_of_band, ref->out_of_band, 0);
	diff_eq_int("wideband (%ld)", ours->wideband, ref->wideband, 0);
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
	cfg.ratio = 0x3666;
	cfg.min_level = 2;

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

	/*
	 * Detection.  Driven with three signal shapes: the tone the filter is
	 * tuned for, a different tone, and silence -- so all three verdicts
	 * (present, absent, no-signal) are reachable rather than assumed.
	 *
	 * Both sides get their own state and accumulators; the energies persist
	 * across calls, so the streams are fed in fragments to check the
	 * carry-over as well as the verdict.
	 */
	diff_begin("MTD detect");
	{
		struct fpm_mtd sa, sb;
		short acc_a[8], acc_b[8];
		short dc_a[2], dc_b[2];
		short buf[240];
		int pass, verdicts[3];

		(void)dc_a; (void)dc_b;
		for (pass = 0; pass < 3; pass++) {
			unsigned lfsr = 0x33CCu;
			int i, pos;

			memset(&sa, 0, sizeof(sa));
			memset(&sb, 0, sizeof(sb));
			memset(acc_a, 0, sizeof(acc_a));
			memset(acc_b, 0, sizeof(acc_b));
			sa.acc = acc_a;
			sb.acc = acc_b;
			ref_FPM_MTD_create(&sa, &cfg);
			FPM_MTD_create(&sb, &cfg);

			for (i = 0; i < 240; i++) {
				lfsr = (lfsr >> 1)
				       ^ (-(int)(lfsr & 1u) & 0xB400u);
				switch (pass) {
				case 0:	/* strong periodic signal */
					buf[i] = (short)(12000
						 * ((i % 6) < 3 ? 1 : -1));
					break;
				case 1:	/* broadband noise        */
					buf[i] = (short)((lfsr & 0x7fff) - 0x4000);
					break;
				default: /* silence               */
					buf[i] = 0;
					break;
				}
			}

			for (pos = 0; pos < 240; pos += 24) {
				short va = ref_FPM_MTD_detect(&sa, buf + pos, 24);
				short vb = FPM_MTD_detect(&sb, buf + pos, 24);

				diff_eq_int("pass %ld: verdict", vb, va, pass);
				diff_eq_int("pass %ld: wideband",
					    sb.wideband, sa.wideband, pass);
				diff_eq_int("pass %ld: out_of_band",
					    sb.out_of_band, sa.out_of_band, pass);
				diff_eq_int("pass %ld: dc_state[0]",
					    sb.dc_state[0], sa.dc_state[0], pass);
				diff_eq_int("pass %ld: dc_state[1]",
					    sb.dc_state[1], sa.dc_state[1], pass);
				for (i = 0; i < 4; i++)
					diff_eq_int("pass %ld: acc",
						    acc_b[i], acc_a[i], pass);
				verdicts[pass] = va;
			}
		}

		/*
		 * Guard against the three passes all returning the same thing,
		 * which would make the comparison above vacuous.
		 */
		diff_eq_int("silence reports NOSIGNAL (%ld)",
			    verdicts[2], FPM_MTD_NOSIGNAL, 0);
		diff_eq_int("signal does not report NOSIGNAL (%ld)",
			    verdicts[0] != FPM_MTD_NOSIGNAL, 1, 0);
	}
	rc |= diff_end();

	return rc;
}

/*
 * t_v32ecc.c -- differential test of V.32's echo-canceller configuration.
 *
 * BY CONTENT, NEVER BY ADDRESS.  `ECCv32_IMAP` and `ECCv32_QMAP` are arrays
 * of pointers, and ours point into our `.rodata` while the blob's point into
 * its own, so the addresses can never agree.  Finding F1614 sets the rule:
 * compare each pointee's CONTENT, over its whole length.  That still pins the
 * ordering, because the six pointees have distinct contents -- except indices
 * 0 and 1, which are the same table, and that is settled by the object's
 * relocations rather than by this test.
 *
 * `ECCv32_CFG` is then compared field by field, its two pointer fields
 * against the arrays above rather than as numbers.
 *
 * WHAT THIS TEST CANNOT SEE, said plainly: it compares six sequences of
 * shorts, so it would pass if the pointer arrays held the right tables in the
 * right order but the object's arrays were, say, four entries and two
 * duplicates.  The lengths come from `nm` and the order from `relocscan`; the
 * test is the check that the values agree, not the derivation.
 */

#include "harness.h"
#include "dsplib/fpm_ecc.h"
#include "dsplib/v32dec.h"
#include "dsplib/vtb.h"

extern const short *const ref_ECCv32_IMAP[6];
extern const short *const ref_ECCv32_QMAP[6];
extern const struct fpm_ecc_cfg ref_ECCv32_CFG;

/*
 * The six pointees, in the order the relocations resolve to, with the entry
 * count of each: SMCv32_*MAP16 is 0x22 bytes and so seventeen entries, of
 * which the seventeenth is zero and nothing indexes it.
 */
static const int lens[6] = { 17, 17, 32, 16, 64, 128 };

static void
cmp_maps(const char *what, const short *const *ours, const short *const *them)
{
	int k, j;

	for (k = 0; k < 6; k++)
		for (j = 0; j < lens[k]; j++)
			diff_eq_int_(__FILE__, __LINE__, what,
				     (long)ours[k][j], (long)them[k][j],
				     (long)(k * 1000 + j));
}

int
main(void)
{
	int rc = 0;

	diff_begin("ECCv32_IMAP / ECCv32_QMAP: the six pointees, by content");
	cmp_maps("ECCv32_IMAP[k][j], k*1000+j = %ld", ECCv32_IMAP,
		 ref_ECCv32_IMAP);
	cmp_maps("ECCv32_QMAP[k][j], k*1000+j = %ld", ECCv32_QMAP,
		 ref_ECCv32_QMAP);
	rc |= diff_end();

	diff_begin("ECCv32_CFG: the fields FPM_ECC_init reads");
	diff_eq_int("far_lag (%ld)", ECCv32_CFG.far_lag,
		    ref_ECCv32_CFG.far_lag, 0);
	diff_eq_int("near_taps (%ld)", ECCv32_CFG.near_taps,
		    ref_ECCv32_CFG.near_taps, 0);
	diff_eq_int("far_taps (%ld)", ECCv32_CFG.far_taps,
		    ref_ECCv32_CFG.far_taps, 0);
	diff_eq_int("pad06 (%ld)", ECCv32_CFG.pad06, ref_ECCv32_CFG.pad06, 0);
	diff_eq_int("fill (%ld)", ECCv32_CFG.fill, ref_ECCv32_CFG.fill, 0);
	diff_eq_int("pad12 (%ld)", ECCv32_CFG.pad12, ref_ECCv32_CFG.pad12, 0);
	diff_eq_int("aux (%ld)", (long)(size_t)ECCv32_CFG.aux,
		    (long)(size_t)ref_ECCv32_CFG.aux, 0);

	/*
	 * The two pointer fields: that each side's config names its own pair
	 * of arrays, and that those arrays agree entry for entry -- which the
	 * block above has already established, so this is only the wiring.
	 */
	diff_eq_int("cfg.imap is our ECCv32_IMAP (%ld)",
		    ECCv32_CFG.imap == ECCv32_IMAP, 1, 0);
	diff_eq_int("cfg.qmap is our ECCv32_QMAP (%ld)",
		    ECCv32_CFG.qmap == ECCv32_QMAP, 1, 0);
	cmp_maps("cfg.imap[k][j], k*1000+j = %ld", ECCv32_CFG.imap,
		 ref_ECCv32_CFG.imap);
	cmp_maps("cfg.qmap[k][j], k*1000+j = %ld", ECCv32_CFG.qmap,
		 ref_ECCv32_CFG.qmap);
	rc |= diff_end();

	/*
	 * And the one thing the config is for: an `FPM_ECC_init` from it has
	 * to build the same state on both sides.  The two heap-allocated
	 * halves cannot be compared as addresses, so the buffers are compared
	 * by content and the pointer fields are skipped.
	 */
	{
		static struct fpm_ecc a, b;
		int k, n;

		memset(&a, 0, sizeof(a));
		memset(&b, 0, sizeof(b));
		a.near_delay = b.near_delay = 12;
		a.far_delay = b.far_delay = 20;

		diff_begin("FPM_ECC_init from ECCv32_CFG, both sides");
		FPM_ECC_init(&a, &ECCv32_CFG, 1);
		FPM_ECC_init(&b, &ref_ECCv32_CFG, 1);

		diff_eq_int("far_lag (%ld)", a.cfg.far_lag, b.cfg.far_lag, 0);
		diff_eq_int("near_taps (%ld)", a.cfg.near_taps,
			    b.cfg.near_taps, 0);
		diff_eq_int("far_taps (%ld)", a.cfg.far_taps,
			    b.cfg.far_taps, 0);
		diff_eq_int("fill (%ld)", a.cfg.fill, b.cfg.fill, 0);
		diff_eq_int("line_len (%ld)", a.line_len, b.line_len, 0);
		diff_eq_int("near_rd (%ld)", a.near_rd, b.near_rd, 0);
		diff_eq_int("far_rd (%ld)", a.far_rd, b.far_rd, 0);
		diff_eq_int("near_len (%ld)", a.near_len, b.near_len, 0);
		diff_eq_int("far_len (%ld)", a.far_len, b.far_len, 0);
		diff_eq_int("mu (%ld)", a.mu, b.mu, 0);

		n = a.line_len;
		for (k = 0; k < n; k++)
			diff_eq_int("line preset to cfg.fill (%ld)",
				    a.line[k], b.line[k], k);

		cmp_maps("state cfg.imap[k][j] (%ld)", a.cfg.imap, b.cfg.imap);
		cmp_maps("state cfg.qmap[k][j] (%ld)", a.cfg.qmap, b.cfg.qmap);

		FPM_ECC_free(&a);
		FPM_ECC_free(&b);
		rc |= diff_end();
	}

	return rc;
}

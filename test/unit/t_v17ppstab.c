/*
 * t_v17ppstab.c -- differential test of `V17TX_create`'s pulse-shaper setup
 *                  tables: FPM_PPS_CFG (shared, fpm_pps.c), SMCv17_IMAP4,
 *                  SMCv17_QMAP4, V17TX_PPS_SCALE, V17TX_PATTERN_SCR1,
 *                  PPSv17_ICOFFS and PPSv17_QCOFFS.
 *
 * None of these is exercised by any function this tree has reconstructed --
 * `V17TX_create` itself is not written -- so this file compares bytes and
 * then, for `FPM_PPS_CFG` together with V.17's own maps and coefficients,
 * drives `FPM_PPS_filter` over a real symbol stream exactly the way
 * `t_v32fptab.c`'s `test_pps_use` drives `PPSv32_CFG`. A wrong field
 * boundary or a wrong table pairing can leave every value matching while the
 * block behaves differently, and only that layer would see it.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/v17fax.h"

extern const struct fpm_pps_cfg ref_FPM_PPS_CFG;
extern const short ref_SMCv17_IMAP4[5];
extern const short ref_SMCv17_QMAP4[5];
extern const int ref_V17TX_PPS_SCALE[4];
extern const short ref_V17TX_PATTERN_SCR1[4];
extern const short ref_PPSv17_ICOFFS[120];
extern const short ref_PPSv17_QCOFFS[120];

extern void ref_FPM_PPS_init(void *state, const void *cfg, int fresh);
extern unsigned short ref_FPM_PPS_filter(void *state, void *src, short *out,
					 unsigned short count);
extern void ref_FPM_PPS_free(void *state);

/* ------------------------------------------------------------------------- */

static int
test_fpm_pps_cfg(void)
{
	diff_begin("FPM_PPS_CFG (struct fpm_pps_cfg)");

	diff_eq_int("phases (%ld)", FPM_PPS_CFG.phases, ref_FPM_PPS_CFG.phases,
		    0x00);
	diff_eq_int("step (%ld)", FPM_PPS_CFG.step, ref_FPM_PPS_CFG.step,
		    0x02);
	diff_eq_int("mapped (%ld)", FPM_PPS_CFG.mapped, ref_FPM_PPS_CFG.mapped,
		    0x04);
	diff_eq_int("scale (%ld)", FPM_PPS_CFG.scale, ref_FPM_PPS_CFG.scale,
		    0x08);
	diff_eq_int("step_adj (%ld)", FPM_PPS_CFG.step_adj,
		    ref_FPM_PPS_CFG.step_adj, 0x0c);
	diff_eq_int("pad0e (%ld)", FPM_PPS_CFG.pad0e, ref_FPM_PPS_CFG.pad0e,
		    0x0e);
	diff_eq_int("coeffs (%ld)", FPM_PPS_CFG.coeffs, ref_FPM_PPS_CFG.coeffs,
		    0x20);
	diff_eq_int("pad22 (%ld)", FPM_PPS_CFG.pad22, ref_FPM_PPS_CFG.pad22,
		    0x22);

	/* The four pointers are NULL on both sides -- a caller patches them. */
	diff_eq_int("imap is NULL (%ld)", FPM_PPS_CFG.imap == NULL, 1, 0x10);
	diff_eq_int("qmap is NULL (%ld)", FPM_PPS_CFG.qmap == NULL, 1, 0x14);
	diff_eq_int("coeff_i is NULL (%ld)", FPM_PPS_CFG.coeff_i == NULL, 1,
		    0x18);
	diff_eq_int("coeff_q is NULL (%ld)", FPM_PPS_CFG.coeff_q == NULL, 1,
		    0x1c);
	diff_eq_int("aux is NULL (%ld)", FPM_PPS_CFG.aux == NULL, 1, 0x24);
	diff_eq_int("ref imap is NULL too (%ld)",
		    ref_FPM_PPS_CFG.imap == NULL, 1, 0x10);
	diff_eq_int("ref aux is NULL too (%ld)", ref_FPM_PPS_CFG.aux == NULL,
		    1, 0x24);

	diff_eq_int("sizeof struct fpm_pps_cfg (%ld)",
		    (long)sizeof(struct fpm_pps_cfg), 0x28, 0);

	return diff_end();
}

static int
test_v17_tables(void)
{
	int i;

	diff_begin("SMCv17_IMAP4 / SMCv17_QMAP4 / V17TX_PPS_SCALE / "
		   "V17TX_PATTERN_SCR1");

	for (i = 0; i < 5; i++) {
		diff_eq_int("SMCv17_IMAP4[%ld]", SMCv17_IMAP4[i],
			    ref_SMCv17_IMAP4[i], i);
		diff_eq_int("SMCv17_QMAP4[%ld]", SMCv17_QMAP4[i],
			    ref_SMCv17_QMAP4[i], i);
	}
	for (i = 0; i < 4; i++) {
		diff_eq_int("V17TX_PPS_SCALE[%ld]", V17TX_PPS_SCALE[i],
			    ref_V17TX_PPS_SCALE[i], i);
		diff_eq_int("V17TX_PATTERN_SCR1[%ld]", V17TX_PATTERN_SCR1[i],
			    ref_V17TX_PATTERN_SCR1[i], i);
	}

	/* IMAP4 and QMAP4 are quadrature rotations of one another, not the
	 * same table twice; a copy-paste swap would still pass the loop
	 * above against the wrong `ref_` symbol if both were identical. */
	diff_eq_int("IMAP4 and QMAP4 differ at index 0 (%ld)",
		    SMCv17_IMAP4[0] != SMCv17_QMAP4[0], 1, 0);

	return diff_end();
}

static int
test_pps_coeffs(void)
{
	int i;

	diff_begin("PPSv17_ICOFFS / PPSv17_QCOFFS");

	for (i = 0; i < 120; i++) {
		diff_eq_int("PPSv17_ICOFFS[%ld]", PPSv17_ICOFFS[i],
			    ref_PPSv17_ICOFFS[i], i);
		diff_eq_int("PPSv17_QCOFFS[%ld]", PPSv17_QCOFFS[i],
			    ref_PPSv17_QCOFFS[i], i);
	}

	/*
	 * The quadrature symmetry V.32's own pair has (t_v32fptab.c): I even
	 * about the centre, Q odd.  A single mistyped coefficient breaks one
	 * of the sixty pairs.
	 */
	for (i = 0; i < 60; i++) {
		diff_eq_int("PPSv17_ICOFFS is symmetric at %ld",
			    PPSv17_ICOFFS[i], PPSv17_ICOFFS[119 - i], i);
		diff_eq_int("PPSv17_QCOFFS is anti-symmetric at %ld",
			    PPSv17_QCOFFS[i], -PPSv17_QCOFFS[119 - i], i);
	}

	return diff_end();
}

/*
 * Layer 2.  Drive both configurations through the shaper they configure,
 * exactly as `V17TX_create` will once it exists: `FPM_PPS_CFG` with `imap`,
 * `qmap`, `coeff_i` and `coeff_q` patched to V.17's own four tables.
 *
 * `mapped` is set (FPM_PPS_CFG.mapped == 1), so the ring's `sym` rail is what
 * is read and the two constellation maps are what turn an index into I and
 * Q.  `count` is deliberately not a multiple of the ring's length, so `ridx`
 * wraps inside a call.
 */
static int
test_pps_use(void)
{
	struct fpm_pps_cfg cfg_a, cfg_b;
	struct fpm_pps ours;
	unsigned char theirs[256];
	struct fpm_smc_ring ring_a, ring_b;
	short sym[20];
	short out_a[512], out_b[512];
	int i;
	int pass;
	int moved = 0;

	diff_begin("FPM_PPS_CFG driving FPM_PPS_filter, V.17's own tables");

	for (i = 0; i < 20; i++)
		sym[i] = (short)((i * 7 + 3) & 0x03);	/* SMCv17_*MAP4 has 4 real
							 * entries, index 0..3 */

	cfg_a = ref_FPM_PPS_CFG;
	cfg_a.imap = ref_SMCv17_IMAP4;
	cfg_a.qmap = ref_SMCv17_QMAP4;
	cfg_a.coeff_i = ref_PPSv17_ICOFFS;
	cfg_a.coeff_q = ref_PPSv17_QCOFFS;

	cfg_b = FPM_PPS_CFG;
	cfg_b.imap = SMCv17_IMAP4;
	cfg_b.qmap = SMCv17_QMAP4;
	cfg_b.coeff_i = PPSv17_ICOFFS;
	cfg_b.coeff_q = PPSv17_QCOFFS;

	memset(&ours, 0, sizeof(ours));
	memset(theirs, 0, sizeof(theirs));
	memset(&ring_a, 0, sizeof(ring_a));
	memset(&ring_b, 0, sizeof(ring_b));

	ring_a.sym = sym;
	ring_a.len = 20;
	ring_b = ring_a;

	ref_FPM_PPS_init(theirs, &cfg_a, 1);
	FPM_PPS_init(&ours, &cfg_b, 1);

	for (pass = 0; pass < 6; pass++) {
		unsigned short na, nb;

		memset(out_a, 0, sizeof(out_a));
		memset(out_b, 0, sizeof(out_b));

		na = ref_FPM_PPS_filter(theirs, &ring_a, out_a, 13);
		nb = FPM_PPS_filter(&ours, &ring_b, out_b, 13);

		diff_eq_int("samples produced, pass %ld", nb, na, pass);
		for (i = 0; i < (int)na && i < 512; i++) {
			diff_eq_int("pass %ld sample", out_b[i], out_a[i],
				    pass);
			if (out_a[i] != 0)
				moved = 1;
		}
		diff_eq_int("ring ridx, pass %ld", ring_b.ridx, ring_a.ridx,
			    pass);
	}

	/* F134's rule: a clean run from a filter that produced nothing is
	 * not a clean run. */
	diff_eq_int("the shaper produced a non-zero sample (%ld)", moved, 1,
		    0);

	ref_FPM_PPS_free(theirs);
	FPM_PPS_free(&ours);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= test_fpm_pps_cfg();
	rc |= test_v17_tables();
	rc |= test_pps_coeffs();
	rc |= test_pps_use();

	return rc;
}

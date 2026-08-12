/*
 * t_fpm_fse.c -- differential test of the fractionally spaced equaliser's
 * setup, and of V.32's instance of its configuration.
 *
 * The five heap pointers necessarily differ between the two builds, so the
 * states are compared as objects with those five fields blanked on both
 * sides, and the buffers themselves are checked by CONTENT, by REQUESTED SIZE
 * and by allocation ORDINAL.  Requested size is the check that matters most
 * here: both sizes are computed in 16 bits before being widened, and nothing
 * about the resulting state records how many bytes were asked for.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/v32fse.h"

extern unsigned int ref_dsplibs_debug_level;
extern void ref_FPM_FSE_init(void *state, const void *cfg, int fresh);
extern void ref_FPM_FSE_free(void *state);
extern short ref_FSEv32_ICOFF[];
extern short ref_FSEv32_QCOFF[];
extern struct fpm_fse_cfg ref_FSEv32_CFG;
extern short ref_CRRv32_CLK[];
extern short ref_CRRv32_PLL_K1[];
extern short ref_CRRv32_PLL_K2[];

/* 20 KB apiece, so not on the stack. */
static struct fpm_fse ours, theirs;
static struct fpm_fse blank_a, blank_b;

static void
compare_state(const char *tag, long id)
{
	blank_a = ours;
	blank_b = theirs;

	blank_a.out_i = blank_b.out_i = 0;
	blank_a.out_q = blank_b.out_q = 0;
	blank_a.icoeff = blank_b.icoeff = 0;
	blank_a.qcoeff = blank_b.qcoeff = 0;
	blank_a.hist = blank_b.hist = 0;

	diff_eq_obj(tag, struct fpm_fse, &blank_a, &blank_b, id);
}

/* Same request, same order, same contents -- everything but the address. */
static void
compare_buffers(long id)
{
	int i;
	int taps = ours.cfg.taps;

	diff_eq_int("icoeff size (%ld)", harness_alloc_reqsize(ours.icoeff),
		    harness_alloc_reqsize(theirs.icoeff), id);
	diff_eq_int("qcoeff size (%ld)", harness_alloc_reqsize(ours.qcoeff),
		    harness_alloc_reqsize(theirs.qcoeff), id);
	diff_eq_int("hist size (%ld)", harness_alloc_reqsize(ours.hist),
		    harness_alloc_reqsize(theirs.hist), id);
	diff_eq_int("out_i size (%ld)", harness_alloc_reqsize(ours.out_i),
		    harness_alloc_reqsize(theirs.out_i), id);
	diff_eq_int("out_q size (%ld)", harness_alloc_reqsize(ours.out_q),
		    harness_alloc_reqsize(theirs.out_q), id);

	/*
	 * The order the five requests were made in.  Both sides allocate from
	 * the same allocator in one run, so the ordinals themselves differ;
	 * what has to agree is which of ours corresponds to which of theirs,
	 * and every ordinal below is relative to its own side's first.
	 */
	diff_eq_int("qcoeff is the 2nd (%ld)",
		    harness_alloc_ordinal(ours.qcoeff)
		    - harness_alloc_ordinal(ours.icoeff),
		    harness_alloc_ordinal(theirs.qcoeff)
		    - harness_alloc_ordinal(theirs.icoeff), id);
	diff_eq_int("hist is the 3rd (%ld)",
		    harness_alloc_ordinal(ours.hist)
		    - harness_alloc_ordinal(ours.icoeff),
		    harness_alloc_ordinal(theirs.hist)
		    - harness_alloc_ordinal(theirs.icoeff), id);
	diff_eq_int("out_i is the 4th (%ld)",
		    harness_alloc_ordinal(ours.out_i)
		    - harness_alloc_ordinal(ours.icoeff),
		    harness_alloc_ordinal(theirs.out_i)
		    - harness_alloc_ordinal(theirs.icoeff), id);
	diff_eq_int("out_q is the 5th (%ld)",
		    harness_alloc_ordinal(ours.out_q)
		    - harness_alloc_ordinal(ours.icoeff),
		    harness_alloc_ordinal(theirs.out_q)
		    - harness_alloc_ordinal(theirs.icoeff), id);

	for (i = 0; i < taps; i++) {
		diff_eq_int("icoeff[%ld]", ours.icoeff[i], theirs.icoeff[i], i);
		diff_eq_int("qcoeff[%ld]", ours.qcoeff[i], theirs.qcoeff[i], i);
		diff_eq_int("hist[%ld] cleared", ours.hist[i], theirs.hist[i],
			    i);
	}
}

/*
 * One configuration, driven through both paths: a fresh init from a zeroed
 * state, then a re-init that has to free all five buffers and allocate again.
 */
static int
run(const char *label, struct fpm_fse_cfg *cfg)
{
	struct alloc_log before;
	int ref_allocs, ref_frees;

	diff_begin(label);

	memset(&ours, 0, sizeof(ours));
	memset(&theirs, 0, sizeof(theirs));

	before = harness_alloc;
	ref_FPM_FSE_init(&theirs, cfg, 1);
	ref_allocs = harness_alloc.allocs - before.allocs;
	ref_frees = harness_alloc.frees - before.frees;

	before = harness_alloc;
	FPM_FSE_init(&ours, cfg, 1);
	diff_eq_int("fresh: allocations (%ld)",
		    harness_alloc.allocs - before.allocs, ref_allocs, 1);
	diff_eq_int("fresh: frees (%ld)", harness_alloc.frees - before.frees,
		    ref_frees, 1);

	compare_state("fresh init", 1);
	compare_buffers(1);

	/* Re-init: five frees, then five fresh allocations. */
	before = harness_alloc;
	ref_FPM_FSE_init(&theirs, cfg, 0);
	ref_allocs = harness_alloc.allocs - before.allocs;
	ref_frees = harness_alloc.frees - before.frees;

	before = harness_alloc;
	FPM_FSE_init(&ours, cfg, 0);
	diff_eq_int("reinit: allocations (%ld)",
		    harness_alloc.allocs - before.allocs, ref_allocs, 2);
	diff_eq_int("reinit: frees (%ld)", harness_alloc.frees - before.frees,
		    ref_frees, 2);
	diff_eq_int("reinit freed all five (%ld)", ref_frees, 5, 2);
	diff_eq_int("no wild frees (%ld)", harness_alloc.bad_free, 0, 2);

	compare_state("re-init", 2);
	compare_buffers(2);

	/* And free: the block must give back everything it took. */
	before = harness_alloc;
	ref_FPM_FSE_free(&theirs);
	ref_frees = harness_alloc.frees - before.frees;
	before = harness_alloc;
	FPM_FSE_free(&ours);
	diff_eq_int("free: frees (%ld)", harness_alloc.frees - before.frees,
		    ref_frees, 3);
	diff_eq_int("free released five (%ld)", ref_frees, 5, 3);
	diff_eq_int("still no wild frees (%ld)", harness_alloc.bad_free, 0, 3);

	return diff_end();
}

/* A configuration of our own, so the arithmetic is exercised off V.32's. */
static short small_i[8] = { 1, -2, 3, -4, 5, -6, 7, -8 };
static short small_q[8] = { -9, 10, -11, 12, -13, 14, -15, 16 };
static short small_clk[2] = { 0, 16384 };
static short small_k[3] = { 7, 8, 9 };

static void
fill_small(struct fpm_fse_cfg *cfg, short block, short interp, short taps)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->block = block;
	cfg->interp = interp;
	cfg->icoff = small_i;
	cfg->qcoff = small_q;
	cfg->taps = taps;
	cfg->mu[0] = 100;
	cfg->mu[1] = 200;
	cfg->mu[2] = 300;
	cfg->clk = small_clk;
	cfg->clk_mod = 2;
	cfg->clk_inc = 1;
	cfg->train_sym = 10;
	cfg->err_hi = 900;
	cfg->err_lo = 100;
	cfg->pll_k1 = small_k;
	cfg->pll_k2 = small_k;
}

int
main(void)
{
	struct fpm_fse_cfg cfg;
	int rc = 0;
	int lvl;

	/*
	 * The layout, against the offsets the disassembly uses.  A wrong one
	 * would show up in the object comparison too, but as an unexplained
	 * run of bytes rather than as the field that moved.
	 */
	diff_begin("FPM_FSE: the state's layout");
	diff_eq_int("sizeof cfg (%ld)", (long)sizeof(struct fpm_fse_cfg),
		    0x38, 0);
	diff_eq_int("cfg.taps (%ld)", (long)offsetof(struct fpm_fse_cfg, taps),
		    0x0c, 0);
	diff_eq_int("cfg.mu (%ld)", (long)offsetof(struct fpm_fse_cfg, mu),
		    0x0e, 0);
	diff_eq_int("cfg.clk (%ld)", (long)offsetof(struct fpm_fse_cfg, clk),
		    0x14, 0);
	diff_eq_int("cfg.pll_k1 (%ld)",
		    (long)offsetof(struct fpm_fse_cfg, pll_k1), 0x24, 0);
	diff_eq_int("cfg.owner (%ld)",
		    (long)offsetof(struct fpm_fse_cfg, owner), 0x2c, 0);
	diff_eq_int("cfg.decision (%ld)",
		    (long)offsetof(struct fpm_fse_cfg, decision), 0x30, 0);
	diff_eq_int("mu_sel (%ld)", (long)offsetof(struct fpm_fse, mu_sel),
		    0x38, 0);
	diff_eq_int("lms_on (%ld)", (long)offsetof(struct fpm_fse, lms_on),
		    0x4c, 0);
	diff_eq_int("out_i (%ld)", (long)offsetof(struct fpm_fse, out_i),
		    0x54, 0);
	diff_eq_int("n_out (%ld)", (long)offsetof(struct fpm_fse, n_out),
		    0x5e, 0);
	diff_eq_int("hist (%ld)", (long)offsetof(struct fpm_fse, hist),
		    0x68, 0);
	diff_eq_int("phase_acc (%ld)",
		    (long)offsetof(struct fpm_fse, phase_acc), 0x70, 0);
	diff_eq_int("tilt_coeff (%ld)",
		    (long)offsetof(struct fpm_fse, tilt_coeff), 0x78, 0);
	diff_eq_int("need (%ld)", (long)offsetof(struct fpm_fse, need),
		    0x8a, 0);
	diff_eq_int("diag (%ld)", (long)offsetof(struct fpm_fse, diag),
		    0x8c, 0);
	diff_eq_int("diag2 (%ld)", (long)offsetof(struct fpm_fse, diag2),
		    0xf8c, 0);
	diff_eq_int("diag_n (%ld)", (long)offsetof(struct fpm_fse, diag_n),
		    0x4e0c, 0);
	diff_eq_int("diag2_n (%ld)", (long)offsetof(struct fpm_fse, diag2_n),
		    0x4e14, 0);
	diff_eq_int("sizeof state (%ld)", (long)sizeof(struct fpm_fse),
		    0x4e18, 0);
	rc |= diff_end();

	/*
	 * V.32's tables and its instance of the configuration.  Reading the
	 * blob's 56 bytes through our struct is what proves the field
	 * boundaries: a mis-declared width would land the wrong halves of the
	 * wrong words in these comparisons.
	 */
	diff_begin("FSEv32: coefficients and configuration");
	{
		int i;

		for (i = 0; i < FSEV32_TAPS; i++) {
			diff_eq_int("ICOFF[%ld]", FSEv32_ICOFF[i],
				    ref_FSEv32_ICOFF[i], i);
			diff_eq_int("QCOFF[%ld]", FSEv32_QCOFF[i],
				    ref_FSEv32_QCOFF[i], i);
		}
		for (i = 0; i < 4; i++)
			diff_eq_int("CLK[%ld]", CRRv32_CLK[i],
				    ref_CRRv32_CLK[i], i);
		for (i = 0; i < 3; i++) {
			diff_eq_int("PLL_K1[%ld]", CRRv32_PLL_K1[i],
				    ref_CRRv32_PLL_K1[i], i);
			diff_eq_int("PLL_K2[%ld]", CRRv32_PLL_K2[i],
				    ref_CRRv32_PLL_K2[i], i);
			diff_eq_int("CFG.mu[%ld]", FSEv32_CFG.mu[i],
				    ref_FSEv32_CFG.mu[i], i);
		}
		diff_eq_int("CFG.block (%ld)", FSEv32_CFG.block,
			    ref_FSEv32_CFG.block, 0);
		diff_eq_int("CFG.interp (%ld)", FSEv32_CFG.interp,
			    ref_FSEv32_CFG.interp, 0);
		diff_eq_int("CFG.taps (%ld)", FSEv32_CFG.taps,
			    ref_FSEv32_CFG.taps, 0);
		diff_eq_int("CFG.clk_mod (%ld)", FSEv32_CFG.clk_mod,
			    ref_FSEv32_CFG.clk_mod, 0);
		diff_eq_int("CFG.clk_inc (%ld)", FSEv32_CFG.clk_inc,
			    ref_FSEv32_CFG.clk_inc, 0);
		diff_eq_int("CFG.train_sym (%ld)", FSEv32_CFG.train_sym,
			    ref_FSEv32_CFG.train_sym, 0);
		diff_eq_int("CFG.err_hi (%ld)", FSEv32_CFG.err_hi,
			    ref_FSEv32_CFG.err_hi, 0);
		diff_eq_int("CFG.err_lo (%ld)", FSEv32_CFG.err_lo,
			    ref_FSEv32_CFG.err_lo, 0);
		diff_eq_int("CFG.pad22 (%ld)", FSEv32_CFG.pad22,
			    ref_FSEv32_CFG.pad22, 0);
		diff_eq_int("CFG.owner is patched in (%ld)",
			    FSEv32_CFG.owner == 0
			    && ref_FSEv32_CFG.owner == 0, 1, 0);
		diff_eq_int("CFG.decision is patched in (%ld)",
			    FSEv32_CFG.decision == 0
			    && ref_FSEv32_CFG.decision == 0, 1, 0);
		diff_eq_int("CFG.reserved34 (%ld)",
			    FSEv32_CFG.reserved34 == 0
			    && ref_FSEv32_CFG.reserved34 == 0, 1, 0);

		/* The pointers cannot match; what they point at must. */
		diff_eq_int("CFG.icoff -> ICOFF (%ld)",
			    FSEv32_CFG.icoff == FSEv32_ICOFF, 1, 0);
		diff_eq_int("ref CFG.icoff -> ref ICOFF (%ld)",
			    ref_FSEv32_CFG.icoff == ref_FSEv32_ICOFF, 1, 0);
		diff_eq_int("CFG.qcoff -> QCOFF (%ld)",
			    FSEv32_CFG.qcoff == FSEv32_QCOFF, 1, 0);
		diff_eq_int("ref CFG.qcoff -> ref QCOFF (%ld)",
			    ref_FSEv32_CFG.qcoff == ref_FSEv32_QCOFF, 1, 0);
		diff_eq_int("CFG.clk -> CLK (%ld)",
			    FSEv32_CFG.clk == CRRv32_CLK, 1, 0);
		diff_eq_int("ref CFG.clk -> ref CLK (%ld)",
			    ref_FSEv32_CFG.clk == ref_CRRv32_CLK, 1, 0);
		diff_eq_int("CFG.pll_k1 -> K1 (%ld)",
			    FSEv32_CFG.pll_k1 == CRRv32_PLL_K1, 1, 0);
		diff_eq_int("ref CFG.pll_k1 -> ref K1 (%ld)",
			    ref_FSEv32_CFG.pll_k1 == ref_CRRv32_PLL_K1, 1, 0);
		diff_eq_int("CFG.pll_k2 -> K2 (%ld)",
			    FSEv32_CFG.pll_k2 == CRRv32_PLL_K2, 1, 0);
		diff_eq_int("ref CFG.pll_k2 -> ref K2 (%ld)",
			    ref_FSEv32_CFG.pll_k2 == ref_CRRv32_PLL_K2, 1, 0);
	}
	rc |= diff_end();

	/* V.32's own configuration, through both init paths. */
	cfg = FSEv32_CFG;
	rc |= run("FPM_FSE_init: FSEv32_CFG (144/3, 103 taps)", &cfg);

	/* An odd division, and one where block/interp does not divide. */
	fill_small(&cfg, 20, 2, 8);
	rc |= run("FPM_FSE_init: 20/2, 8 taps", &cfg);
	fill_small(&cfg, 21, 4, 3);
	rc |= run("FPM_FSE_init: 21/4, 3 taps", &cfg);
	fill_small(&cfg, 1, 1, 1);
	rc |= run("FPM_FSE_init: 1/1, 1 tap", &cfg);

	/*
	 * 2 * (32767 / 1) + 4 is 65538, which is 2 as a short.  Nothing in
	 * the state records the size, so only `harness_alloc_reqsize` can
	 * tell the object's truncated arithmetic from the obvious untruncated
	 * reading -- and only this case makes them differ.
	 */
	fill_small(&cfg, 32767, 1, 4);
	rc |= run("FPM_FSE_init: block 32767 wraps the symbol buffer", &cfg);

	/*
	 * The one announcement in this file, and it is on the re-init path.
	 * Swept over all three levels because a site at the wrong threshold
	 * produces an identical transcript at any single one.
	 */
	diff_begin("FPM_FSE_init: the reallocation says so");
	{
		unsigned lines = 0;

		for (lvl = 1; lvl <= 3; lvl++) {
			cfg = FSEv32_CFG;
			memset(&ours, 0, sizeof(ours));
			memset(&theirs, 0, sizeof(theirs));
			ref_FPM_FSE_init(&theirs, &cfg, 1);
			FPM_FSE_init(&ours, &cfg, 1);

			dsplibs_debug_level = ref_dsplibs_debug_level =
				(unsigned)lvl;
			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();
			ref_FPM_FSE_init(&theirs, &cfg, 0);
			FPM_FSE_init(&ours, &cfg, 0);
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;
			dsplib_debug_capture_on = 0;

			compare_state("announced re-init", lvl);
			diff_eq_int("transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, lvl);
			if (lvl == 1)
				diff_eq_int("level 1 is silent",
					    (int)dsplib_debug_capture_lines(1),
					    0, lvl);
			else
				lines += dsplib_debug_capture_lines(1);

			ref_FPM_FSE_free(&theirs);
			FPM_FSE_free(&ours);
		}
		diff_eq_int("it said something (%ld)", lines > 0, 1,
			    (long)lines);
	}
	rc |= diff_end();

	return rc;
}

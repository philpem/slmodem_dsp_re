/*
 * t_v29cfg.c -- differential test of the V.29 fax receiver's fourteen tables,
 *               of the three library built-in configurations they are patched
 *               into, and of the V.21 bank all three fax receivers share.
 *
 *   AGCv29_CFG           .rodata 0x00b27c    24
 *   V29RX_MRF_FILT       .rodata 0x00b060   540
 *   V29RX_SRE_FILT       .rodata 0x00aee0   362
 *   V29RX_XB_COFFS       .rodata 0x00b04a    22
 *   V29_MTD_COEFF        .data   0x007da0    20
 *   V29RX_FSE_QFILT      .data   0x007ec0    98
 *   V29RX_FSE_IFILT      .data   0x007f40    98
 *   V29RX_FSE_PLLK2      .data   0x007fa2     6
 *   V29RX_FSE_PLLK1      .data   0x007fa8     6
 *   V29RX_CRR_TABLE      .data   0x007fc0   144
 *   V29RX_SRE_PLLK2      .data   0x008050     6
 *   V29RX_SRE_PLLK1      .data   0x008056     6
 *   V29RX_YCLOCK         .data   0x00805c     6
 *   V29RX_XCLOCK         .data   0x008062     6
 *   V21_CHAN2_MTD_COEFF  .data   0x007a60    20
 *   FPM_FSE_CFG          .data   0x008160    56
 *   FPM_MTD_CFG          .data   0x0081b0    12
 *   FPM_SRE_CFG          .rodata 0x00c4e0    56
 *
 * FIVE LAYERS, and each one exists because of a specific wrong reading the
 * layer below it would pass.
 *
 *   1. SHAPE.  `sizeof` against the `nm -S` size of every symbol.  A byte
 *      comparison run over OUR array's length cannot notice that our array is
 *      one element short of the object's -- it would simply compare fewer
 *      bytes and pass.  `V29RX_SRE_FILT` is the live case: `sre.coeffs` is 180
 *      and the table is 181, because the interpolator reads `proto[i+1]` at
 *      `i == coeffs-1`, so an array sized from the count rather than from the
 *      symbol would be wrong by exactly one entry and invisible.
 *
 *   2. VALUE, element by element against `ref_`.  This is what fixes the
 *      bytes, and on its own it is the weakest-looking layer: three of these
 *      tables are mostly zero.
 *
 *   3. THE DETECTOR IS SHOWN TO FIRE (F134).  `shorts_differ` is run over an
 *      identical copy, which must return 0, and then over one perturbed copy
 *      per table with a single element changed by one, which must all return
 *      non-zero.  A comparison of two arrays that are equal by construction
 *      proves nothing until it has been seen to reject something.
 *
 *   4. THE SHAPE OF THE VALUES, independently of `ref_`.  The two long filters
 *      are asserted SYMMETRIC about their centres, the equaliser's rails are
 *      asserted zero at every third tap, and `V29RX_CRR_TABLE` is asserted
 *      equal to `round(i * 32768 / 72)` for all 72 entries.  That last one is
 *      the evidence that the table is a phase ramp in Q16 and therefore an
 *      array of `short`, and it closes on V.29's own 1700 Hz carrier: the
 *      receiver runs at 7200 Hz and `V29RX_create` steps this table by 17.
 *
 *   5. USE.  Every table is driven through the DSP block that consumes it,
 *      ours against the blob's, with the configuration built exactly as
 *      `V29RX_create` builds it -- copy the library built-in, patch the
 *      tables and the lengths, call init.  A wrong element stride survives
 *      layers 1 to 4 only if it also reproduces the bytes, but a wrong
 *      LENGTH FIELD does not survive this one: `FPM_SRE_init` copies
 *      `cfg.coeffs` entries out of `proto`, so a count that disagrees with
 *      the object's shows up in the coefficient buffer.
 *
 * WHAT IS COMPARED BY CONTENT RATHER THAN BY VALUE.  Every pointer here holds
 * an address, and ours can never equal the blob's: `AGCv29_CFG.alpha`,
 * `FPM_MTD_CFG.coeff` and the six patched into the `fpm_sre_cfg` all point at
 * our copy on our side and at the object's on the blob's.  Each is compared by
 * dereferencing it, which is the only claim a differential test can make about
 * a pointer -- and it is the claim that matters, since what a table IS is its
 * contents.
 *
 * `AGC_DEF_ALPHA` AND `AGC_DEF_BETA` HAVE NO `ref_` ALIAS AT ALL.  The object
 * defines each of those names six times, five of them local, so `symmap.py`
 * gives them none and there is nothing to compare by name.  They are reached
 * here only through `AGCv29_CFG.alpha` and `.beta`, which is the F9058 rule:
 * where a name is ambiguous in the object, the consumer is the comparison.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"

#include "dsplib/v29cfg.h"
#include "dsplib/faxcfg.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/sysdep.h"

/* The blob's copies.  An undeclared `ref_` name is a hard error, not an int. */
extern const struct fpm_agc_cfg ref_AGCv29_CFG;
extern const short ref_V29RX_MRF_FILT[270];
extern const short ref_V29RX_SRE_FILT[181];
extern const short ref_V29RX_XB_COFFS[11];
extern short ref_V29_MTD_COEFF[10];
extern short ref_V29RX_FSE_QFILT[49];
extern short ref_V29RX_FSE_IFILT[49];
extern short ref_V29RX_FSE_PLLK2[3];
extern short ref_V29RX_FSE_PLLK1[3];
extern short ref_V29RX_CRR_TABLE[72];
extern short ref_V29RX_SRE_PLLK2[3];
extern short ref_V29RX_SRE_PLLK1[3];
extern short ref_V29RX_YCLOCK[3];
extern short ref_V29RX_XCLOCK[3];
extern short ref_V21_CHAN2_MTD_COEFF[10];
extern short ref_V29RX_DEC_IMAP[16];
extern short ref_V29RX_DEC_QMAP[16];
extern short ref_V29RX_DEC_ANGLE[16];
extern short ref_V29RX_DEC_MAG[16];
extern short ref_V29RX_DEC_PMAP[8];

extern struct fpm_fse_cfg ref_FPM_FSE_CFG;
extern struct fpm_mtd_cfg ref_FPM_MTD_CFG;
extern const struct fpm_sre_cfg ref_FPM_SRE_CFG;

extern void ref_FPM_AGC_init(struct fpm_agc *agc,
			     const struct fpm_agc_cfg *cfg, int reset);
extern struct fpm_mtd *ref_FPM_MTD_create(struct fpm_mtd *state,
					  const struct fpm_mtd_cfg *cfg);
extern void ref_FPM_SRE_init(struct fpm_sre *sre,
			     const struct fpm_sre_cfg *cfg, int fresh);
extern void ref_FPM_FSE_init(struct fpm_fse *state,
			     const struct fpm_fse_cfg *cfg, int fresh);
extern void ref_FPM_FSE_free(struct fpm_fse *state);
extern void ref_FPM_SRE_free(struct fpm_sre *sre);
extern void ref_FPM_MRF_init(struct fpm_mrf *state,
			     const struct fpm_mrf_cfg *cfg, int fresh);

/* ------------------------------------------------------------------------- */

/*
 * Layer 3's detector.  Its own function rather than a `memcmp` call site, so
 * that it can be exercised on a known-perturbed input below.
 */
static int
shorts_differ(const short *a, const short *b, int n)
{
	int i;

	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			return 1;
	return 0;
}

/* Compare `n` shorts and report the FIRST disagreement, with its index. */
static void
cmp_shorts(const char *what, const short *got, const short *want, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (got[i] != want[i]) {
			diff_eq_int(what, got[i], want[i], i);
			return;
		}
	}
	/* One passing check per table, so the count is not dominated by
	 * thousands of trivially equal entries. */
	diff_eq_int(what, 0, 0, n);
}

/* How many of `n` entries are non-zero.  Anti-vacuity for layer 2. */
static int
nonzero(const short *p, int n)
{
	int i, c = 0;

	for (i = 0; i < n; i++)
		if (p[i] != 0)
			c++;
	return c;
}

/* ------------------------------------------------------------------------- */

static int
test_shape(void)
{
	diff_begin("v29cfg: sizes against the object's symbol table");

	/*
	 * Every one of these is `nm -S`'s size for the symbol.  A table whose
	 * C array is shorter than the object's symbol compares equal over its
	 * own length and is still wrong.
	 */
	diff_eq_int("sizeof AGCv29_CFG (%ld)",
		    (long)sizeof(AGCv29_CFG), 24, 0);
	diff_eq_int("sizeof V29RX_MRF_FILT (%ld)",
		    (long)sizeof(V29RX_MRF_FILT), 540, 0);
	diff_eq_int("sizeof V29RX_SRE_FILT (%ld)",
		    (long)sizeof(V29RX_SRE_FILT), 362, 0);
	diff_eq_int("sizeof V29RX_XB_COFFS (%ld)",
		    (long)sizeof(V29RX_XB_COFFS), 22, 0);
	diff_eq_int("sizeof V29_MTD_COEFF (%ld)",
		    (long)sizeof(V29_MTD_COEFF), 20, 0);
	diff_eq_int("sizeof V29RX_FSE_QFILT (%ld)",
		    (long)sizeof(V29RX_FSE_QFILT), 98, 0);
	diff_eq_int("sizeof V29RX_FSE_IFILT (%ld)",
		    (long)sizeof(V29RX_FSE_IFILT), 98, 0);
	diff_eq_int("sizeof V29RX_FSE_PLLK2 (%ld)",
		    (long)sizeof(V29RX_FSE_PLLK2), 6, 0);
	diff_eq_int("sizeof V29RX_FSE_PLLK1 (%ld)",
		    (long)sizeof(V29RX_FSE_PLLK1), 6, 0);
	diff_eq_int("sizeof V29RX_CRR_TABLE (%ld)",
		    (long)sizeof(V29RX_CRR_TABLE), 144, 0);
	diff_eq_int("sizeof V29RX_SRE_PLLK2 (%ld)",
		    (long)sizeof(V29RX_SRE_PLLK2), 6, 0);
	diff_eq_int("sizeof V29RX_SRE_PLLK1 (%ld)",
		    (long)sizeof(V29RX_SRE_PLLK1), 6, 0);
	diff_eq_int("sizeof V29RX_YCLOCK (%ld)",
		    (long)sizeof(V29RX_YCLOCK), 6, 0);
	diff_eq_int("sizeof V29RX_XCLOCK (%ld)",
		    (long)sizeof(V29RX_XCLOCK), 6, 0);
	diff_eq_int("sizeof V21_CHAN2_MTD_COEFF (%ld)",
		    (long)sizeof(V21_CHAN2_MTD_COEFF), 20, 0);
	diff_eq_int("sizeof V29RX_DEC_QMAP (%ld)",
		    (long)sizeof(V29RX_DEC_QMAP), 32, 0);
	diff_eq_int("sizeof V29RX_DEC_IMAP (%ld)",
		    (long)sizeof(V29RX_DEC_IMAP), 32, 0);
	diff_eq_int("sizeof V29RX_DEC_ANGLE (%ld)",
		    (long)sizeof(V29RX_DEC_ANGLE), 32, 0);
	diff_eq_int("sizeof V29RX_DEC_MAG (%ld)",
		    (long)sizeof(V29RX_DEC_MAG), 32, 0);
	diff_eq_int("sizeof V29RX_DEC_PMAP (%ld)",
		    (long)sizeof(V29RX_DEC_PMAP), 16, 0);
	/* The slicer's two search bounds, which is why the four maps are 16
	 * entries and not 8.  `V29RX_decision` computes 8 or 16 branchlessly
	 * from the rate selector and always starts the search at zero. */
	diff_eq_int("DEC maps hold the 16-point superset (%ld)",
		    (long)(sizeof(V29RX_DEC_IMAP) / sizeof(short)), 16, 0);
	diff_eq_int("DEC_PMAP holds one octant (%ld)",
		    (long)(sizeof(V29RX_DEC_PMAP) / sizeof(short)), 8, 0);
	diff_eq_int("sizeof FPM_FSE_CFG (%ld)",
		    (long)sizeof(FPM_FSE_CFG), 56, 0);
	diff_eq_int("sizeof FPM_MTD_CFG (%ld)",
		    (long)sizeof(FPM_MTD_CFG), 12, 0);
	diff_eq_int("sizeof FPM_SRE_CFG (%ld)",
		    (long)sizeof(FPM_SRE_CFG), 56, 0);

	/*
	 * The two relationships `V29RX_create` depends on, stated as
	 * arithmetic rather than as two independent literals.
	 *
	 * `proto` must hold one more entry than `coeffs`, and the discriminant
	 * exactly FPM_SRE_DISC.
	 */
	diff_eq_int("SRE_FILT holds coeffs+1 entries (%ld)",
		    (long)(sizeof(V29RX_SRE_FILT) / sizeof(short)), 180 + 1, 0);
	diff_eq_int("XB_COFFS holds FPM_SRE_DISC entries (%ld)",
		    (long)(sizeof(V29RX_XB_COFFS) / sizeof(short)),
		    FPM_SRE_DISC, 0);
	diff_eq_int("XCLOCK holds clock_len entries (%ld)",
		    (long)(sizeof(V29RX_XCLOCK) / sizeof(short)), 3, 0);
	diff_eq_int("SRE_PLLK1 holds FPM_SRE_MODES entries (%ld)",
		    (long)(sizeof(V29RX_SRE_PLLK1) / sizeof(short)),
		    FPM_SRE_MODES, 0);
	diff_eq_int("MRF_FILT holds 9 branches x 30 taps (%ld)",
		    (long)(sizeof(V29RX_MRF_FILT) / sizeof(short)), 9 * 30, 0);
	diff_eq_int("FSE_IFILT holds fse.taps entries (%ld)",
		    (long)(sizeof(V29RX_FSE_IFILT) / sizeof(short)), 49, 0);
	diff_eq_int("CRR_TABLE holds fse.clk_mod entries (%ld)",
		    (long)(sizeof(V29RX_CRR_TABLE) / sizeof(short)), 72, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

static int
test_values(void)
{
	diff_begin("v29cfg: table values against the blob");

	cmp_shorts("V29RX_MRF_FILT[%ld]", V29RX_MRF_FILT,
		   ref_V29RX_MRF_FILT, 270);
	cmp_shorts("V29RX_SRE_FILT[%ld]", V29RX_SRE_FILT,
		   ref_V29RX_SRE_FILT, 181);
	cmp_shorts("V29RX_XB_COFFS[%ld]", V29RX_XB_COFFS,
		   ref_V29RX_XB_COFFS, 11);
	cmp_shorts("V29_MTD_COEFF[%ld]", V29_MTD_COEFF, ref_V29_MTD_COEFF, 10);
	cmp_shorts("V29RX_FSE_QFILT[%ld]", V29RX_FSE_QFILT,
		   ref_V29RX_FSE_QFILT, 49);
	cmp_shorts("V29RX_FSE_IFILT[%ld]", V29RX_FSE_IFILT,
		   ref_V29RX_FSE_IFILT, 49);
	cmp_shorts("V29RX_FSE_PLLK2[%ld]", V29RX_FSE_PLLK2,
		   ref_V29RX_FSE_PLLK2, 3);
	cmp_shorts("V29RX_FSE_PLLK1[%ld]", V29RX_FSE_PLLK1,
		   ref_V29RX_FSE_PLLK1, 3);
	cmp_shorts("V29RX_CRR_TABLE[%ld]", V29RX_CRR_TABLE,
		   ref_V29RX_CRR_TABLE, 72);
	cmp_shorts("V29RX_SRE_PLLK2[%ld]", V29RX_SRE_PLLK2,
		   ref_V29RX_SRE_PLLK2, 3);
	cmp_shorts("V29RX_SRE_PLLK1[%ld]", V29RX_SRE_PLLK1,
		   ref_V29RX_SRE_PLLK1, 3);
	cmp_shorts("V29RX_YCLOCK[%ld]", V29RX_YCLOCK, ref_V29RX_YCLOCK, 3);
	cmp_shorts("V29RX_XCLOCK[%ld]", V29RX_XCLOCK, ref_V29RX_XCLOCK, 3);
	cmp_shorts("V21_CHAN2_MTD_COEFF[%ld]", V21_CHAN2_MTD_COEFF,
		   ref_V21_CHAN2_MTD_COEFF, 10);
	cmp_shorts("V29RX_DEC_QMAP[%ld]", V29RX_DEC_QMAP,
		   ref_V29RX_DEC_QMAP, 16);
	cmp_shorts("V29RX_DEC_IMAP[%ld]", V29RX_DEC_IMAP,
		   ref_V29RX_DEC_IMAP, 16);
	cmp_shorts("V29RX_DEC_ANGLE[%ld]", V29RX_DEC_ANGLE,
		   ref_V29RX_DEC_ANGLE, 16);
	cmp_shorts("V29RX_DEC_MAG[%ld]", V29RX_DEC_MAG, ref_V29RX_DEC_MAG, 16);
	cmp_shorts("V29RX_DEC_PMAP[%ld]", V29RX_DEC_PMAP,
		   ref_V29RX_DEC_PMAP, 8);

	/* AGCv29_CFG, field by field; the two pointers by their contents. */
	diff_eq_int("AGCv29_CFG.ref_level (%ld)", AGCv29_CFG.ref_level,
		    ref_AGCv29_CFG.ref_level, 0x00);
	diff_eq_int("AGCv29_CFG.acquire_level (%ld)",
		    AGCv29_CFG.acquire_level, ref_AGCv29_CFG.acquire_level,
		    0x02);
	diff_eq_int("AGCv29_CFG.squelch_level (%ld)",
		    AGCv29_CFG.squelch_level, ref_AGCv29_CFG.squelch_level,
		    0x04);
	diff_eq_int("AGCv29_CFG.f06 (%ld)", AGCv29_CFG.f06,
		    ref_AGCv29_CFG.f06, 0x06);
	diff_eq_int("AGCv29_CFG.f08 (%ld)", AGCv29_CFG.f08,
		    ref_AGCv29_CFG.f08, 0x08);
	diff_eq_int("AGCv29_CFG.block_len (%ld)", AGCv29_CFG.block_len,
		    ref_AGCv29_CFG.block_len, 0x0a);
	diff_eq_int("AGCv29_CFG.f14 (%ld)", AGCv29_CFG.f14,
		    ref_AGCv29_CFG.f14, 0x14);
	diff_eq_int("AGCv29_CFG.f16 (%ld)", AGCv29_CFG.f16,
		    ref_AGCv29_CFG.f16, 0x16);
	cmp_shorts("AGCv29_CFG.alpha[%ld]", AGCv29_CFG.alpha,
		   ref_AGCv29_CFG.alpha, 2);
	cmp_shorts("AGCv29_CFG.beta[%ld]", AGCv29_CFG.beta,
		   ref_AGCv29_CFG.beta, 2);

	/* FPM_FSE_CFG. */
	diff_eq_int("FPM_FSE_CFG.block (%ld)", FPM_FSE_CFG.block,
		    ref_FPM_FSE_CFG.block, 0x00);
	diff_eq_int("FPM_FSE_CFG.interp (%ld)", FPM_FSE_CFG.interp,
		    ref_FPM_FSE_CFG.interp, 0x02);
	diff_eq_int("FPM_FSE_CFG.taps (%ld)", FPM_FSE_CFG.taps,
		    ref_FPM_FSE_CFG.taps, 0x0c);
	cmp_shorts("FPM_FSE_CFG.mu[%ld]", FPM_FSE_CFG.mu,
		   ref_FPM_FSE_CFG.mu, 3);
	diff_eq_int("FPM_FSE_CFG.clk_mod (%ld)", FPM_FSE_CFG.clk_mod,
		    ref_FPM_FSE_CFG.clk_mod, 0x18);
	diff_eq_int("FPM_FSE_CFG.clk_inc (%ld)", FPM_FSE_CFG.clk_inc,
		    ref_FPM_FSE_CFG.clk_inc, 0x1a);
	diff_eq_int("FPM_FSE_CFG.train_sym (%ld)", FPM_FSE_CFG.train_sym,
		    ref_FPM_FSE_CFG.train_sym, 0x1c);
	diff_eq_int("FPM_FSE_CFG.err_hi (%ld)", FPM_FSE_CFG.err_hi,
		    ref_FPM_FSE_CFG.err_hi, 0x1e);
	diff_eq_int("FPM_FSE_CFG.err_lo (%ld)", FPM_FSE_CFG.err_lo,
		    ref_FPM_FSE_CFG.err_lo, 0x20);
	diff_eq_int("FPM_FSE_CFG.pad22 (%ld)", FPM_FSE_CFG.pad22,
		    ref_FPM_FSE_CFG.pad22, 0x22);
	/* The six pointers are NULL in the object; asserted as such on both
	 * sides, because "equal" here would be satisfied by two garbage
	 * addresses that happen to match. */
	diff_eq_int("FPM_FSE_CFG pointers null, ours (%ld)",
		    FPM_FSE_CFG.icoff == 0 && FPM_FSE_CFG.qcoff == 0 &&
		    FPM_FSE_CFG.clk == 0 && FPM_FSE_CFG.pll_k1 == 0 &&
		    FPM_FSE_CFG.pll_k2 == 0 && FPM_FSE_CFG.owner == 0 &&
		    FPM_FSE_CFG.decision == 0 &&
		    FPM_FSE_CFG.reserved34 == 0, 1, 0);
	diff_eq_int("FPM_FSE_CFG pointers null, blob (%ld)",
		    ref_FPM_FSE_CFG.icoff == 0 && ref_FPM_FSE_CFG.qcoff == 0 &&
		    ref_FPM_FSE_CFG.clk == 0 && ref_FPM_FSE_CFG.pll_k1 == 0 &&
		    ref_FPM_FSE_CFG.pll_k2 == 0 && ref_FPM_FSE_CFG.owner == 0 &&
		    ref_FPM_FSE_CFG.decision == 0 &&
		    ref_FPM_FSE_CFG.reserved34 == 0, 1, 0);

	/* FPM_SRE_CFG. */
	diff_eq_int("FPM_SRE_CFG.clock_len (%ld)", FPM_SRE_CFG.clock_len,
		    ref_FPM_SRE_CFG.clock_len, 0x00);
	diff_eq_int("FPM_SRE_CFG.groups_acq (%ld)", FPM_SRE_CFG.groups_acq,
		    ref_FPM_SRE_CFG.groups_acq, 0x02);
	diff_eq_int("FPM_SRE_CFG.groups_trk (%ld)", FPM_SRE_CFG.groups_trk,
		    ref_FPM_SRE_CFG.groups_trk, 0x04);
	diff_eq_int("FPM_SRE_CFG.settle (%ld)", FPM_SRE_CFG.settle,
		    ref_FPM_SRE_CFG.settle, 0x06);
	diff_eq_int("FPM_SRE_CFG.acc_down (%ld)", FPM_SRE_CFG.acc_down,
		    ref_FPM_SRE_CFG.acc_down, 0x08);
	diff_eq_int("FPM_SRE_CFG.acc_up (%ld)", FPM_SRE_CFG.acc_up,
		    ref_FPM_SRE_CFG.acc_up, 0x0a);
	diff_eq_int("FPM_SRE_CFG.coeffs (%ld)", FPM_SRE_CFG.coeffs,
		    ref_FPM_SRE_CFG.coeffs, 0x0c);
	diff_eq_int("FPM_SRE_CFG.pad0e (%ld)", FPM_SRE_CFG.pad0e,
		    ref_FPM_SRE_CFG.pad0e, 0x0e);
	diff_eq_int("FPM_SRE_CFG.mag_hi (%ld)", FPM_SRE_CFG.mag_hi,
		    ref_FPM_SRE_CFG.mag_hi, 0x28);
	diff_eq_int("FPM_SRE_CFG.mag_lo (%ld)", FPM_SRE_CFG.mag_lo,
		    ref_FPM_SRE_CFG.mag_lo, 0x2a);
	diff_eq_int("FPM_SRE_CFG.err_hi (%ld)", FPM_SRE_CFG.err_hi,
		    ref_FPM_SRE_CFG.err_hi, 0x2c);
	diff_eq_int("FPM_SRE_CFG.err_lo (%ld)", FPM_SRE_CFG.err_lo,
		    ref_FPM_SRE_CFG.err_lo, 0x2e);
	diff_eq_int("FPM_SRE_CFG.rms_min (%ld)", FPM_SRE_CFG.rms_min,
		    ref_FPM_SRE_CFG.rms_min, 0x30);
	diff_eq_int("FPM_SRE_CFG.rms_len (%ld)", FPM_SRE_CFG.rms_len,
		    ref_FPM_SRE_CFG.rms_len, 0x32);
	diff_eq_int("FPM_SRE_CFG.pad34 (%ld)", FPM_SRE_CFG.pad34,
		    ref_FPM_SRE_CFG.pad34, 0x34);
	diff_eq_int("FPM_SRE_CFG.pad36 (%ld)", FPM_SRE_CFG.pad36,
		    ref_FPM_SRE_CFG.pad36, 0x36);
	diff_eq_int("FPM_SRE_CFG pointers null, ours (%ld)",
		    FPM_SRE_CFG.proto == 0 && FPM_SRE_CFG.disc == 0 &&
		    FPM_SRE_CFG.xclock == 0 && FPM_SRE_CFG.yclock == 0 &&
		    FPM_SRE_CFG.pll_k1 == 0 && FPM_SRE_CFG.pll_k2 == 0, 1, 0);
	diff_eq_int("FPM_SRE_CFG pointers null, blob (%ld)",
		    ref_FPM_SRE_CFG.proto == 0 && ref_FPM_SRE_CFG.disc == 0 &&
		    ref_FPM_SRE_CFG.xclock == 0 &&
		    ref_FPM_SRE_CFG.yclock == 0 &&
		    ref_FPM_SRE_CFG.pll_k1 == 0 &&
		    ref_FPM_SRE_CFG.pll_k2 == 0, 1, 0);

	/* FPM_MTD_CFG, and the file-static bank it points at. */
	diff_eq_int("FPM_MTD_CFG.tones (%ld)", FPM_MTD_CFG.tones,
		    ref_FPM_MTD_CFG.tones, 0x04);
	diff_eq_int("FPM_MTD_CFG.ratio (%ld)", FPM_MTD_CFG.ratio,
		    ref_FPM_MTD_CFG.ratio, 0x06);
	diff_eq_int("FPM_MTD_CFG.min_level (%ld)", FPM_MTD_CFG.min_level,
		    ref_FPM_MTD_CFG.min_level, 0x08);
	diff_eq_int("FPM_MTD_CFG.f0a (%ld)", FPM_MTD_CFG.f0a,
		    ref_FPM_MTD_CFG.f0a, 0x0a);
	/* The pointer is NOT null here -- it is the one `fpm_*_cfg` built-in
	 * whose table is real -- so it is compared by dereferencing. */
	diff_eq_int("FPM_MTD_CFG.coeff non-null, ours (%ld)",
		    FPM_MTD_CFG.coeff != 0, 1, 0);
	diff_eq_int("FPM_MTD_CFG.coeff non-null, blob (%ld)",
		    ref_FPM_MTD_CFG.coeff != 0, 1, 0);
	cmp_shorts("FPM_MTD_CFG.coeff[%ld] (DEF_COEFS)", FPM_MTD_CFG.coeff,
		   ref_FPM_MTD_CFG.coeff, 10);

	/*
	 * ANTI-VACUITY.  Three of the tables above are mostly zero and two of
	 * the configs are entirely zero except for a handful of fields, so
	 * assert that the comparison saw real data somewhere.
	 */
	diff_eq_int("MRF_FILT has non-zero entries (%ld)",
		    nonzero(V29RX_MRF_FILT, 270) > 260, 1, 0);
	diff_eq_int("SRE_FILT has non-zero entries (%ld)",
		    nonzero(V29RX_SRE_FILT, 181) > 170, 1, 0);
	/* 49 taps, zero at the sixteen multiples of three other than the
	 * centre tap 24, so 33 are non-zero.  See test_value_shape. */
	diff_eq_int("FSE_IFILT non-zero entries (%ld)",
		    nonzero(V29RX_FSE_IFILT, 49), 33, 0);
	diff_eq_int("FSE_QFILT non-zero entries (%ld)",
		    nonzero(V29RX_FSE_QFILT, 49), 32, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * Layer 3.  `cmp_shorts` and `shorts_differ` are the same loop; perturbing one
 * element of each table and requiring the detector to notice is what separates
 * "the tables agree" from "the comparison is dead".
 */
static int
test_detector_fires(void)
{
	static short copy[540];
	int i;

	struct { const short *p; int n; const char *name; } tab[] = {
		{ V29RX_MRF_FILT,     270, "V29RX_MRF_FILT"      },
		{ V29RX_SRE_FILT,     181, "V29RX_SRE_FILT"      },
		{ V29RX_XB_COFFS,      11, "V29RX_XB_COFFS"      },
		{ V29_MTD_COEFF,       10, "V29_MTD_COEFF"       },
		{ V29RX_FSE_QFILT,     49, "V29RX_FSE_QFILT"     },
		{ V29RX_FSE_IFILT,     49, "V29RX_FSE_IFILT"     },
		{ V29RX_FSE_PLLK2,      3, "V29RX_FSE_PLLK2"     },
		{ V29RX_FSE_PLLK1,      3, "V29RX_FSE_PLLK1"     },
		{ V29RX_CRR_TABLE,     72, "V29RX_CRR_TABLE"     },
		{ V29RX_SRE_PLLK2,      3, "V29RX_SRE_PLLK2"     },
		{ V29RX_SRE_PLLK1,      3, "V29RX_SRE_PLLK1"     },
		{ V29RX_YCLOCK,         3, "V29RX_YCLOCK"        },
		{ V29RX_XCLOCK,         3, "V29RX_XCLOCK"        },
		{ V21_CHAN2_MTD_COEFF, 10, "V21_CHAN2_MTD_COEFF" },
		{ V29RX_DEC_QMAP,      16, "V29RX_DEC_QMAP"      },
		{ V29RX_DEC_IMAP,      16, "V29RX_DEC_IMAP"      },
		{ V29RX_DEC_ANGLE,     16, "V29RX_DEC_ANGLE"     },
		{ V29RX_DEC_MAG,       16, "V29RX_DEC_MAG"       },
		{ V29RX_DEC_PMAP,       8, "V29RX_DEC_PMAP"      }
	};
	const int ntab = (int)(sizeof(tab) / sizeof(tab[0]));

	diff_begin("v29cfg: the comparison rejects a perturbed table");

	for (i = 0; i < ntab; i++) {
		int j;

		memcpy(copy, tab[i].p, (size_t)tab[i].n * sizeof(short));
		diff_eq_int("identical copy compares equal (%ld)",
			    shorts_differ(copy, tab[i].p, tab[i].n), 0, i);

		/* Every element in turn, so a detector that only looks at the
		 * first or the last one is caught too. */
		for (j = 0; j < tab[i].n; j++) {
			copy[j] = (short)(copy[j] + 1);
			diff_eq_int("one-element perturbation is seen (%ld)",
				    shorts_differ(copy, tab[i].p, tab[i].n),
				    1, j);
			copy[j] = (short)(copy[j] - 1);
		}
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * Layer 4.  Facts about the values that hold independently of the blob, and
 * that a transcription error would break even if it were transcribed into both
 * sides -- which cannot happen here, but which is exactly what a generator
 * written from these bytes later WOULD risk.
 */
static int
test_value_shape(void)
{
	int i, sym;

	diff_begin("v29cfg: the shape of the values");

	/*
	 * The carrier ramp, in closed form.  `round(i * 32768 / 72)` computed
	 * in integers as `(i * 65536 + 72) / 144`.
	 */
	for (i = 0; i < 72; i++)
		diff_eq_int("CRR_TABLE[%ld] is round(i*32768/72)",
			    V29RX_CRR_TABLE[i], (i * 65536 + 72) / 144, i);

	/* And the ramp's step is what makes the carrier 1700 Hz at 7200 Hz:
	 * 72 entries is one turn, 17 entries per sample. */
	diff_eq_int("7200 * clk_inc / clk_mod is V.29's carrier (%ld)",
		    7200 * 17 / 72, 1700, 0);

	/* The two long prototypes are linear phase: symmetric about centre. */
	sym = 1;
	for (i = 0; i < 270; i++)
		if (V29RX_MRF_FILT[i] != V29RX_MRF_FILT[269 - i])
			sym = 0;
	diff_eq_int("MRF_FILT is symmetric (%ld)", sym, 1, 0);

	sym = 1;
	for (i = 0; i < 181; i++)
		if (V29RX_SRE_FILT[i] != V29RX_SRE_FILT[180 - i])
			sym = 0;
	diff_eq_int("SRE_FILT is symmetric (%ld)", sym, 1, 0);

	/*
	 * THE ZERO PATTERN, AND IT IS NOT THE ONE THIS TEST FIRST ASSERTED.
	 * At three samples per symbol the rails carry energy only at the
	 * symbol instants, so every third tap is zero -- EXCEPT tap 24 of the
	 * I rail, which is 13653 and is the main tap.  The Q rail has no such
	 * exception: its centre is 0, which is what antisymmetry about the
	 * centre forces.  So the I rail is "identity plus the interpolating
	 * taps" and the Q rail is a pure Hilbert-shaped pair, and the
	 * asymmetry between them is the object's and is the whole reason the
	 * first, tidier, reading was wrong.
	 */
	for (i = 0; i < 49; i += 3) {
		if (i != 24)
			diff_eq_int("FSE_IFILT[%ld] is zero",
				    V29RX_FSE_IFILT[i], 0, i);
		diff_eq_int("FSE_QFILT[%ld] is zero", V29RX_FSE_QFILT[i], 0, i);
	}
	diff_eq_int("FSE_IFILT[24] is the main tap (%ld)",
		    V29RX_FSE_IFILT[24], 13653, 24);
	diff_eq_int("FSE_QFILT[24] is zero (%ld)", V29RX_FSE_QFILT[24], 0, 24);
	sym = 1;
	for (i = 0; i < 49; i++)
		if (V29RX_FSE_IFILT[i] != V29RX_FSE_IFILT[48 - i])
			sym = 0;
	diff_eq_int("FSE_IFILT is symmetric (%ld)", sym, 1, 0);
	sym = 1;
	for (i = 0; i < 49; i++)
		if (V29RX_FSE_QFILT[i] != -V29RX_FSE_QFILT[48 - i])
			sym = 0;
	diff_eq_int("FSE_QFILT is antisymmetric (%ld)", sym, 1, 0);

	/* Both PLL gain arrays start with a zero integral term. */
	diff_eq_int("FSE_PLLK2[0] is zero (%ld)", V29RX_FSE_PLLK2[0], 0, 0);
	diff_eq_int("SRE_PLLK2[0] is zero (%ld)", V29RX_SRE_PLLK2[0], 0, 0);

	/*
	 * The clock legs are three phases 120 degrees apart in Q14: the X leg
	 * is (1, -1/2, -1/2) and the Y leg (0, +sqrt(3)/2, -sqrt(3)/2).  Both
	 * legs sum to zero, which is what makes the correlation a discriminant
	 * rather than a level measurement.
	 */
	diff_eq_int("XCLOCK sums to zero (%ld)",
		    V29RX_XCLOCK[0] + V29RX_XCLOCK[1] + V29RX_XCLOCK[2], 0, 0);
	diff_eq_int("YCLOCK sums to zero (%ld)",
		    V29RX_YCLOCK[0] + V29RX_YCLOCK[1] + V29RX_YCLOCK[2], 0, 0);
	diff_eq_int("XCLOCK[0] is 1.0 in Q14 (%ld)", V29RX_XCLOCK[0],
		    16384, 0);

	/*
	 * The two MTD banks and the library's default are all two five-short
	 * biquad sections, and the two fax ones share their first two
	 * coefficients across both sections.
	 */
	diff_eq_int("V29_MTD_COEFF sections share coeff 0 (%ld)",
		    V29_MTD_COEFF[0] == V29_MTD_COEFF[5], 1, 0);
	diff_eq_int("V21_CHAN2 sections share coeff 0 (%ld)",
		    V21_CHAN2_MTD_COEFF[0] == V21_CHAN2_MTD_COEFF[5], 1, 0);
	/* ... and are NOT the same bank as each other, which a copy-paste
	 * between the two files would have made them. */
	diff_eq_int("the two banks differ (%ld)",
		    shorts_differ(V29_MTD_COEFF, V21_CHAN2_MTD_COEFF, 10),
		    1, 0);

	return diff_end();
}

/* Nearest integer to sqrt(n), in integers -- no libm, no rounding mode. */
static long
isqrt_round(long n)
{
	long r = 0;

	while ((r + 1) * (r + 1) <= n)
		r++;
	/* round up when the true root is nearer to r+1 */
	if (n - r * r > (r + 1) * (r + 1) - n)
		r++;
	return r;
}

/*
 * The V.29 constellation, checked against ITU-T V.29's own amplitudes rather
 * than against the blob.  This is the layer that types these five tables: a
 * transcription that reproduced the bytes but split them into the wrong number
 * of elements, or that read them as an unsigned magnitude table, would not
 * satisfy four independent published numbers at once.
 */
static int
test_constellation(void)
{
	/*
	 * V.29 Table 1: amplitude 3 on the axes and sqrt(2) on the diagonals
	 * for the inner ring, 5 and 3*sqrt(2) for the outer.  Scaled by 2048,
	 * the axis values are exact integers and only the diagonals round.
	 */
	static const short axis[2] = { 3 * 2048, 5 * 2048 };
	/* A * cos(45 degrees) * 2048 for the two diagonal amplitudes:
	 * sqrt(2)*2048/sqrt(2) = 2048, and 3*sqrt(2)*2048/sqrt(2) = 6144. */
	static const short diag[2] = { 2048, 3 * 2048 };
	int ring, k, i, seen[8];

	diff_begin("v29cfg: the decision tables are V.29's own constellation");

	for (ring = 0; ring < 2; ring++) {
		for (k = 0; k < 8; k++) {
			short a = axis[ring];
			short d = diag[ring];
			short wi, wq;

			i = ring * 8 + k;

			/*
			 * Eight phases 45 degrees apart.  The even ones lie on
			 * an axis at amplitude `a`; the odd ones on a diagonal
			 * at (d, d) with the signs of that octant.
			 */
			switch (k) {
			case 0: wi =  a; wq =  0; break;
			case 1: wi =  d; wq =  d; break;
			case 2: wi =  0; wq =  a; break;
			case 3: wi = (short)-d; wq =  d; break;
			case 4: wi = (short)-a; wq =  0; break;
			case 5: wi = (short)-d; wq = (short)-d; break;
			case 6: wi =  0; wq = (short)-a; break;
			default: wi = d; wq = (short)-d; break;
			}

			diff_eq_int("DEC_IMAP[%ld] is V.29's own I",
				    V29RX_DEC_IMAP[i], wi, i);
			diff_eq_int("DEC_QMAP[%ld] is V.29's own Q",
				    V29RX_DEC_QMAP[i], wq, i);

			/*
			 * `DEC_MAG` is the point's radius, to the nearest
			 * integer -- which is what makes 2896 and 8689 the
			 * rounded sqrt(2) and 3*sqrt(2) rather than two
			 * arbitrary constants.
			 */
			diff_eq_int("DEC_MAG[%ld] is the point's radius",
				    V29RX_DEC_MAG[i],
				    (short)isqrt_round((long)wi * wi +
						       (long)wq * wq), i);

			/* 4096 counts per 45 degrees in a 32768-count turn. */
			diff_eq_int("DEC_ANGLE[%ld] is k * 45 degrees",
				    V29RX_DEC_ANGLE[i], k * 4096, i);
		}
	}

	/*
	 * The two rings are at the SAME eight phases -- so the angle table
	 * repeats -- and differ only in amplitude, in the ratio 5:3 on the
	 * axes.  Both are stated as arithmetic on the table rather than as
	 * literals, so a swapped ring would fail them.
	 */
	for (i = 0; i < 8; i++)
		diff_eq_int("DEC_ANGLE repeats for the outer ring (%ld)",
			    V29RX_DEC_ANGLE[i + 8], V29RX_DEC_ANGLE[i], i);
	diff_eq_int("the rings are in the ratio 5:3 (%ld)",
		    3 * (int)V29RX_DEC_IMAP[8], 5 * (int)V29RX_DEC_IMAP[0], 0);

	/*
	 * The differential map is a PERMUTATION of 0..7.  Eight small integers
	 * is exactly where a transcription slip hides, and "every value is in
	 * range" would not catch a repeat.
	 */
	for (i = 0; i < 8; i++)
		seen[i] = 0;
	for (i = 0; i < 8; i++) {
		diff_eq_int("DEC_PMAP[%ld] is in 0..7",
			    V29RX_DEC_PMAP[i] >= 0 && V29RX_DEC_PMAP[i] < 8,
			    1, i);
		if (V29RX_DEC_PMAP[i] >= 0 && V29RX_DEC_PMAP[i] < 8)
			seen[V29RX_DEC_PMAP[i]]++;
	}
	for (i = 0; i < 8; i++)
		diff_eq_int("DEC_PMAP hits %ld exactly once", seen[i], 1, i);

	/* And it is not the identity, which is the one permutation a missing
	 * table would look like. */
	diff_eq_int("DEC_PMAP is not the identity (%ld)",
		    V29RX_DEC_PMAP[0] != 0 || V29RX_DEC_PMAP[1] != 1, 1, 0);

	/* isqrt_round is doing real work, so show it rejecting: the radius of
	 * the inner diagonal point is 2896 and not 2895 or 2897. */
	diff_eq_int("isqrt_round(2048^2 * 2) (%ld)",
		    isqrt_round(2048L * 2048 * 2), 2896, 0);
	diff_eq_int("isqrt_round(6144^2 * 2) (%ld)",
		    isqrt_round(6144L * 6144 * 2), 8689, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * Layer 5.  `V29RX_create` builds each configuration by copying the library
 * built-in and patching it; these helpers do exactly that, once per side, so
 * that the two inits see the same construction over different tables.
 */
static void
build_sre_cfg(struct fpm_sre_cfg *c, const struct fpm_sre_cfg *base,
	      const short *proto, const short *disc, const short *xclk,
	      const short *yclk, const short *k1, const short *k2)
{
	*c = *base;
	c->clock_len = 3;
	c->groups_acq = 3;
	c->groups_trk = 16;
	c->settle = 43;
	c->acc_down = 0x2000;
	c->acc_up = 0x4000;
	c->coeffs = 180;
	c->proto = proto;
	c->disc = disc;
	c->xclock = xclk;
	c->yclock = yclk;
	c->pll_k1 = k1;
	c->pll_k2 = k2;
	c->mag_hi = 2;
	c->mag_lo = 1;
	c->err_hi = 0x2666;
	c->err_lo = 200;
	c->rms_min = 0;
	c->rms_len = 9;
}

static void
build_fse_cfg(struct fpm_fse_cfg *c, const struct fpm_fse_cfg *base,
	      const short *icoff, const short *qcoff, const short *clk,
	      const short *k1, const short *k2)
{
	*c = *base;
	c->interp = 3;
	c->icoff = icoff;
	c->qcoff = qcoff;
	c->taps = 49;
	c->mu[0] = 0;
	c->mu[1] = 0;
	c->clk = clk;
	c->clk_mod = 72;
	c->clk_inc = 17;
	c->train_sym = 456;
	c->err_hi = 0x199a;
	c->err_lo = 0xccd;
	c->pll_k1 = k1;
	c->pll_k2 = k2;
	c->owner = 0;
	c->decision = 0;
}

static int
test_use_agc(void)
{
	struct fpm_agc a, b;
	int reset;

	diff_begin("v29cfg: AGCv29_CFG through FPM_AGC_init");

	for (reset = 0; reset <= 1; reset++) {
		memset(&a, 0xa5, sizeof(a));
		memset(&b, 0xa5, sizeof(b));

		FPM_AGC_init(&a, &AGCv29_CFG, reset);
		ref_FPM_AGC_init(&b, &ref_AGCv29_CFG, reset);

		diff_eq_int("agc.cfg.ref_level (%ld)", a.cfg.ref_level,
			    b.cfg.ref_level, reset);
		diff_eq_int("agc.cfg.acquire_level (%ld)",
			    a.cfg.acquire_level, b.cfg.acquire_level, reset);
		diff_eq_int("agc.cfg.squelch_level (%ld)",
			    a.cfg.squelch_level, b.cfg.squelch_level, reset);
		diff_eq_int("agc.cfg.block_len (%ld)", a.cfg.block_len,
			    b.cfg.block_len, reset);
		diff_eq_int("agc.cfg.f14 (%ld)", a.cfg.f14, b.cfg.f14, reset);
		diff_eq_int("agc.cfg.f16 (%ld)", a.cfg.f16, b.cfg.f16, reset);
		/* The pointers were copied wholesale; what they reach is the
		 * only thing the two sides can agree about. */
		cmp_shorts("agc.cfg.alpha[%ld]", a.cfg.alpha, b.cfg.alpha, 2);
		cmp_shorts("agc.cfg.beta[%ld]", a.cfg.beta, b.cfg.beta, 2);

		diff_eq_int("agc.f18 (%ld)", a.f18, b.f18, reset);
		diff_eq_int("agc.signal (%ld)", a.signal, b.signal, reset);
		diff_eq_int("agc.level (%ld)", a.level, b.level, reset);
		diff_eq_int("agc.mult (%ld)", a.mult, b.mult, reset);
		diff_eq_int("agc.shift (%ld)", a.shift, b.shift, reset);
		diff_eq_int("agc.freeze (%ld)", a.freeze, b.freeze, reset);
	}

	return diff_end();
}

static int
test_use_mtd(void)
{
	static struct fpm_mtd a, b;
	static short acc_a[8], acc_b[8];
	struct fpm_mtd_cfg ca, cb;
	int which;

	diff_begin("v29cfg: the three MTD banks through FPM_MTD_create");

	/*
	 * Three configurations: the library default (whose bank is the
	 * file-static DEF_COEFS), and the two `V29RX_create` builds -- one
	 * around V29_MTD_COEFF and one around V21_CHAN2_MTD_COEFF, with the
	 * thresholds the object writes beside each.
	 */
	for (which = 0; which < 3; which++) {
		memset(&a, 0xa5, sizeof(a));
		memset(&b, 0xa5, sizeof(b));
		a.acc = acc_a;
		b.acc = acc_b;

		if (which == 0) {
			ca = FPM_MTD_CFG;
			cb = ref_FPM_MTD_CFG;
		} else if (which == 1) {
			ca = FPM_MTD_CFG;
			cb = ref_FPM_MTD_CFG;
			ca.coeff = V29_MTD_COEFF;
			cb.coeff = ref_V29_MTD_COEFF;
			ca.tones = cb.tones = 2;
			ca.ratio = cb.ratio = 0x199a;
			ca.min_level = cb.min_level = 50;
		} else {
			ca = FPM_MTD_CFG;
			cb = ref_FPM_MTD_CFG;
			ca.coeff = V21_CHAN2_MTD_COEFF;
			cb.coeff = ref_V21_CHAN2_MTD_COEFF;
			ca.tones = cb.tones = 2;
			ca.ratio = cb.ratio = 0x4ccd;
			ca.min_level = cb.min_level = 300;
		}

		FPM_MTD_create(&a, &ca);
		ref_FPM_MTD_create(&b, &cb);

		diff_eq_int("mtd.cfg.tones (%ld)", a.cfg.tones, b.cfg.tones,
			    which);
		diff_eq_int("mtd.cfg.ratio (%ld)", a.cfg.ratio, b.cfg.ratio,
			    which);
		diff_eq_int("mtd.cfg.min_level (%ld)", a.cfg.min_level,
			    b.cfg.min_level, which);
		diff_eq_int("mtd.cfg.f0a (%ld)", a.cfg.f0a, b.cfg.f0a, which);
		cmp_shorts("mtd.cfg.coeff[%ld]", a.cfg.coeff, b.cfg.coeff, 10);
		diff_eq_int("mtd.dc_state[0] (%ld)", a.dc_state[0],
			    b.dc_state[0], which);
		diff_eq_int("mtd.dc_state[1] (%ld)", a.dc_state[1],
			    b.dc_state[1], which);
		diff_eq_int("mtd.out_of_band (%ld)", a.out_of_band,
			    b.out_of_band, which);
		diff_eq_int("mtd.wideband (%ld)", a.wideband, b.wideband,
			    which);
		cmp_shorts("mtd.acc[%ld]", a.acc, b.acc, 2 * 2);
	}

	return diff_end();
}

static int
test_use_sre(void)
{
	static struct fpm_sre a, b;
	struct fpm_sre_cfg ca, cb;

	diff_begin("v29cfg: the SRE tables through FPM_SRE_init");

	memset(&a, 0xa5, sizeof(a));
	memset(&b, 0xa5, sizeof(b));

	build_sre_cfg(&ca, &FPM_SRE_CFG, V29RX_SRE_FILT, V29RX_XB_COFFS,
		      V29RX_XCLOCK, V29RX_YCLOCK, V29RX_SRE_PLLK1,
		      V29RX_SRE_PLLK2);
	build_sre_cfg(&cb, &ref_FPM_SRE_CFG, ref_V29RX_SRE_FILT,
		      ref_V29RX_XB_COFFS, ref_V29RX_XCLOCK, ref_V29RX_YCLOCK,
		      ref_V29RX_SRE_PLLK1, ref_V29RX_SRE_PLLK2);

	FPM_SRE_init(&a, &ca, 1);
	ref_FPM_SRE_init(&b, &cb, 1);

	diff_eq_int("sre.taps (%ld)", a.taps, b.taps, 0);
	diff_eq_int("sre.mode (%ld)", a.mode, b.mode, 0);
	diff_eq_int("sre.groups (%ld)", a.groups, b.groups, 0);
	diff_eq_int("sre.settle (%ld)", a.settle, b.settle, 0);
	diff_eq_int("sre.need (%ld)", a.need, b.need, 0);
	diff_eq_int("sre.rms_on (%ld)", a.rms_on, b.rms_on, 0);
	diff_eq_int("sre.acquiring (%ld)", a.acquiring, b.acquiring, 0);
	diff_eq_int("sre.cfg.coeffs (%ld)", a.cfg.coeffs, b.cfg.coeffs, 0);

	/*
	 * THE POINT OF THIS BLOCK.  `FPM_SRE_init` copies `cfg.coeffs` entries
	 * out of `proto` into its own buffer, so this compares 180 shorts that
	 * reached the state THROUGH the count `V29RX_create` wrote -- a table
	 * of the right bytes and the wrong length cannot pass it.
	 */
	cmp_shorts("sre.coeff[%ld]", a.coeff, b.coeff, 180);

	/* The four tables the state keeps by pointer, by their contents. */
	cmp_shorts("sre.cfg.disc[%ld]", a.cfg.disc, b.cfg.disc, FPM_SRE_DISC);
	cmp_shorts("sre.cfg.xclock[%ld]", a.cfg.xclock, b.cfg.xclock, 3);
	cmp_shorts("sre.cfg.yclock[%ld]", a.cfg.yclock, b.cfg.yclock, 3);
	cmp_shorts("sre.cfg.pll_k1[%ld]", a.cfg.pll_k1, b.cfg.pll_k1,
		   FPM_SRE_MODES);
	cmp_shorts("sre.cfg.pll_k2[%ld]", a.cfg.pll_k2, b.cfg.pll_k2,
		   FPM_SRE_MODES);
	/* proto's last entry is past `coeffs` and is read only by the
	 * interpolator, so it never reaches `sre.coeff`.  Compared here. */
	diff_eq_int("sre.cfg.proto[180] (%ld)", a.cfg.proto[180],
		    b.cfg.proto[180], 180);

	FPM_SRE_free(&a);
	ref_FPM_SRE_free(&b);

	return diff_end();
}

static int
test_use_fse(void)
{
	static struct fpm_fse a, b;
	struct fpm_fse_cfg ca, cb;

	diff_begin("v29cfg: the FSE tables through FPM_FSE_init");

	memset(&a, 0xa5, sizeof(a));
	memset(&b, 0xa5, sizeof(b));

	build_fse_cfg(&ca, &FPM_FSE_CFG, V29RX_FSE_IFILT, V29RX_FSE_QFILT,
		      V29RX_CRR_TABLE, V29RX_FSE_PLLK1, V29RX_FSE_PLLK2);
	build_fse_cfg(&cb, &ref_FPM_FSE_CFG, ref_V29RX_FSE_IFILT,
		      ref_V29RX_FSE_QFILT, ref_V29RX_CRR_TABLE,
		      ref_V29RX_FSE_PLLK1, ref_V29RX_FSE_PLLK2);

	FPM_FSE_init(&a, &ca, 1);
	ref_FPM_FSE_init(&b, &cb, 1);

	diff_eq_int("fse.cfg.block (%ld)", a.cfg.block, b.cfg.block, 0);
	diff_eq_int("fse.cfg.taps (%ld)", a.cfg.taps, b.cfg.taps, 0);
	diff_eq_int("fse.cfg.clk_mod (%ld)", a.cfg.clk_mod, b.cfg.clk_mod, 0);
	diff_eq_int("fse.mu_sel (%ld)", a.mu_sel, b.mu_sel, 0);
	diff_eq_int("fse.pll_sel (%ld)", a.pll_sel, b.pll_sel, 0);
	diff_eq_int("fse.widx (%ld)", a.widx, b.widx, 0);
	diff_eq_int("fse.need (%ld)", a.need, b.need, 0);

	/* The coefficients that reached the state through `cfg.taps`. */
	cmp_shorts("fse.icoeff[%ld]", a.icoeff, b.icoeff, 49);
	cmp_shorts("fse.qcoeff[%ld]", a.qcoeff, b.qcoeff, 49);
	cmp_shorts("fse.cfg.clk[%ld]", a.cfg.clk, b.cfg.clk, 72);
	cmp_shorts("fse.cfg.pll_k1[%ld]", a.cfg.pll_k1, b.cfg.pll_k1, 3);
	cmp_shorts("fse.cfg.pll_k2[%ld]", a.cfg.pll_k2, b.cfg.pll_k2, 3);
	cmp_shorts("fse.tilt_coeff[%ld]", a.tilt_coeff, b.tilt_coeff, 4);

	FPM_FSE_free(&a);
	ref_FPM_FSE_free(&b);

	return diff_end();
}

static int
test_use_mrf(void)
{
	static struct fpm_mrf a, b;
	struct fpm_mrf_cfg ca, cb;

	diff_begin("v29cfg: V29RX_MRF_FILT through FPM_MRF_init");

	memset(&a, 0xa5, sizeof(a));
	memset(&b, 0xa5, sizeof(b));

	/* As `V29RX_create` builds it: 9 branches, decimate 10, 270 taps. */
	ca = FPM_MRF_CFG;
	cb = FPM_MRF_CFG;
	ca.branches = cb.branches = 9;
	ca.decimate = cb.decimate = 10;
	ca.taps = cb.taps = 270;
	ca.coeff = V29RX_MRF_FILT;
	cb.coeff = ref_V29RX_MRF_FILT;
	ca.aux = cb.aux = 0;

	FPM_MRF_init(&a, &ca, 1);
	ref_FPM_MRF_init(&b, &cb, 1);

	diff_eq_int("mrf.cfg.branches (%ld)", a.cfg.branches, b.cfg.branches,
		    0);
	diff_eq_int("mrf.cfg.decimate (%ld)", a.cfg.decimate, b.cfg.decimate,
		    0);
	diff_eq_int("mrf.cfg.taps (%ld)", a.cfg.taps, b.cfg.taps, 0);
	diff_eq_int("mrf.history_len (%ld)", a.history_len, b.history_len, 0);
	diff_eq_int("mrf.history_len is taps/branches (%ld)",
		    a.history_len, 30, 0);
	diff_eq_int("mrf.need (%ld)", a.need, b.need, 0);
	diff_eq_int("mrf.phase (%ld)", a.phase, b.phase, 0);
	diff_eq_int("mrf.widx (%ld)", a.widx, b.widx, 0);
	cmp_shorts("mrf.cfg.coeff[%ld]", a.cfg.coeff, b.cfg.coeff, 270);
	cmp_shorts("mrf.history[%ld]", a.history, b.history, 30);

	sysdep_free(a.history);
	sysdep_free(b.history);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	rc |= test_shape();
	rc |= test_values();
	rc |= test_detector_fires();
	rc |= test_value_shape();
	rc |= test_constellation();
	rc |= test_use_agc();
	rc |= test_use_mtd();
	rc |= test_use_sre();
	rc |= test_use_fse();
	rc |= test_use_mrf();

	return rc;
}

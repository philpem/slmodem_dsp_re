/*
 * t_v21cfg.c -- differential test of the nine tables `V21RX_create` (0x098e70)
 *               references, of `FPM_FSD_CFG` -- the library built-in it copies
 *               onto its stack -- and of the DSP blocks all ten are patched
 *               into.
 *
 *   AGCv21_CFG           .rodata 0x00a0e4    24   struct fpm_agc_cfg
 *   AGC_DEF_ALPHA_v21    .rodata 0x00a100     4   short[2]
 *   AGC_DEF_BETA_v21     .rodata 0x00a0fc     4   short[2]
 *   V21RX_IIR_LPF        .rodata 0x00a088    30   short[15]
 *   V21RX_CHAN2_INTRP    .rodata 0x00a0a6    30   short[15]
 *   V21RX_CHAN1_INTRP    .rodata 0x00a0c4    30   short[15]
 *   V21_MRF_FILT         .rodata 0x00c000   720   short[360]
 *   V21_CHAN1_MTD_COEFF  .data   0x007a74    20   short[10]
 *   V21RX_CFG            .data   0x007ab4    24   struct v21rx_cfg
 *   FPM_FSD_CFG          .data   0x00812c    28   struct fpm_fsd_cfg
 *
 * FIVE LAYERS, which are `t_v29cfg.c`'s and are kept in the same order and for
 * the same reasons.  Each one exists because of a specific wrong reading the
 * layer below it would pass.
 *
 *   1. SHAPE.  `sizeof` against `nm -S`'s size for every symbol.  A byte
 *      comparison run over OUR array's length cannot notice that our array is
 *      one element short of the object's -- it compares fewer bytes and
 *      passes.  Layer 1 also states each count as the ARITHMETIC that fixes
 *      it, because every one of them is written down twice in the object:
 *      once as `st_size` and once as the length `V21RX_create` stores beside
 *      the pointer.
 *
 *   2. VALUE, element by element against `ref_`.  This is what fixes the
 *      bytes.
 *
 *   3. THE DETECTOR IS SHOWN TO FIRE (F134).  `shorts_differ` is run over an
 *      identical copy, which must return 0, and then over one perturbed copy
 *      per element -- every element of every table in turn -- which must all
 *      return non-zero.  A comparison of two arrays that are equal by
 *      construction proves nothing until it has been seen to reject
 *      something, and a detector that only inspects the first or last entry
 *      is caught by sweeping the whole array.
 *
 *   4. THE SHAPE OF THE VALUES, independently of `ref_`.  The resampler's
 *      prototype is asserted symmetric about its centre; the two AGC
 *      coefficient arrays are asserted to have unity DC gain in both
 *      elements; and the two tone-detector banks are asserted to BE
 *      resonators at V.21's own four frequencies.  That last one is the
 *      evidence that fixes the element type and the stride independently of
 *      `st_size`, and it is described at the test itself.
 *
 *   5. USE.  Every table is driven through the DSP block that consumes it,
 *      ours against the blob's, with the configuration built exactly as
 *      `V21RX_create` builds it -- copy the library built-in, patch the
 *      tables and the lengths, call init.  A wrong element stride survives
 *      layers 1 to 4 only if it also reproduces the bytes; a wrong LENGTH
 *      FIELD does not survive this one, because `FPM_MRF_init` derives
 *      `history_len` from `taps / branches` and `FPM_FSD_init` sizes three
 *      buffers from three separate counts.
 *
 * WHAT IS COMPARED BY CONTENT RATHER THAN BY VALUE.  `AGCv21_CFG.alpha` and
 * `.beta` hold addresses, and ours can never equal the blob's -- each points
 * at its own side's copy.  Each is compared by dereferencing it, which is the
 * only claim a differential test can make about a pointer, and it is the claim
 * that matters, since what a table IS is its contents.
 *
 * UNLIKE V.17'S, V.27'S AND V.29'S, V.21'S TWO AGC ARRAYS ARE COMPARED BY
 * NAME.  F9144 measured `AGC_DEF_ALPHA` and `AGC_DEF_BETA` as defined six
 * times each in the blob, so F9058 denies them a `ref_` alias and the consumer
 * is the only comparison available.  `AGC_DEF_ALPHA_v21` and
 * `AGC_DEF_BETA_v21` are defined ONCE each and are GLOBAL, so they do have an
 * alias -- and this test uses both routes, by name in layer 2 and through
 * `AGCv21_CFG` in layer 5, because the second is what proves the pointers in
 * the config reach them.
 */

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "harness.h"

#include "dsplib/v21cfg.h"
#include "dsplib/faxcfg.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fsd.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/sysdep.h"

/* The blob's copies.  An undeclared `ref_` name is a hard error, not an int. */
extern const struct fpm_agc_cfg ref_AGCv21_CFG;
extern const short ref_AGC_DEF_ALPHA_v21[2];
extern const short ref_AGC_DEF_BETA_v21[2];
extern const short ref_V21RX_IIR_LPF[15];
extern const short ref_V21RX_CHAN2_INTRP[15];
extern const short ref_V21RX_CHAN1_INTRP[15];
extern const short ref_V21_MRF_FILT[360];
extern short ref_V21_CHAN1_MTD_COEFF[10];
extern short ref_V21_CHAN2_MTD_COEFF[10];
extern struct v21rx_cfg ref_V21RX_CFG;
extern struct fpm_fsd_cfg ref_FPM_FSD_CFG;
extern struct fpm_mtd_cfg ref_FPM_MTD_CFG;

extern void ref_FPM_AGC_init(struct fpm_agc *agc,
			     const struct fpm_agc_cfg *cfg, int reset);
extern void ref_FPM_MRF_init(struct fpm_mrf *state,
			     const struct fpm_mrf_cfg *cfg, int fresh);
extern void ref_FPM_MRF_free(struct fpm_mrf *state);
extern void ref_FPM_FSD_init(struct fpm_fsd *state,
			     const struct fpm_fsd_cfg *cfg, int fresh);
extern void ref_FPM_FSD_free(struct fpm_fsd *state);
extern struct fpm_mtd *ref_FPM_MTD_create(struct fpm_mtd *state,
					  const struct fpm_mtd_cfg *cfg);

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
	 * hundreds of trivially equal entries. */
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
	diff_begin("v21cfg: sizes against the object's symbol table");

	diff_eq_int("sizeof AGCv21_CFG (%ld)",
		    (long)sizeof(AGCv21_CFG), 24, 0);
	diff_eq_int("sizeof AGC_DEF_ALPHA_v21 (%ld)",
		    (long)sizeof(AGC_DEF_ALPHA_v21), 4, 0);
	diff_eq_int("sizeof AGC_DEF_BETA_v21 (%ld)",
		    (long)sizeof(AGC_DEF_BETA_v21), 4, 0);
	diff_eq_int("sizeof V21RX_IIR_LPF (%ld)",
		    (long)sizeof(V21RX_IIR_LPF), 30, 0);
	diff_eq_int("sizeof V21RX_CHAN2_INTRP (%ld)",
		    (long)sizeof(V21RX_CHAN2_INTRP), 30, 0);
	diff_eq_int("sizeof V21RX_CHAN1_INTRP (%ld)",
		    (long)sizeof(V21RX_CHAN1_INTRP), 30, 0);
	diff_eq_int("sizeof V21_MRF_FILT (%ld)",
		    (long)sizeof(V21_MRF_FILT), 720, 0);
	diff_eq_int("sizeof V21_CHAN1_MTD_COEFF (%ld)",
		    (long)sizeof(V21_CHAN1_MTD_COEFF), 20, 0);
	diff_eq_int("sizeof V21RX_CFG (%ld)",
		    (long)sizeof(V21RX_CFG), 24, 0);
	diff_eq_int("sizeof FPM_FSD_CFG (%ld)",
		    (long)sizeof(FPM_FSD_CFG), 28, 0);

	/*
	 * The counts as arithmetic rather than as literals.  Each of these is
	 * the SECOND reading of a number `st_size` above already gave once --
	 * see the file header, and F9140 for the method.
	 */
	diff_eq_int("MRF_FILT holds 9 branches x 40 taps (%ld)",
		    (long)(sizeof(V21_MRF_FILT) / sizeof(short)), 9 * 40, 0);
	diff_eq_int("CHAN1_INTRP holds fsd.fir_taps entries (%ld)",
		    (long)(sizeof(V21RX_CHAN1_INTRP) / sizeof(short)), 15, 0);
	diff_eq_int("CHAN2_INTRP holds fsd.fir_taps entries (%ld)",
		    (long)(sizeof(V21RX_CHAN2_INTRP) / sizeof(short)), 15, 0);
	diff_eq_int("IIR_LPF holds fsd.iir_len sections of 5 (%ld)",
		    (long)(sizeof(V21RX_IIR_LPF) / sizeof(short)), 3 * 5, 0);
	diff_eq_int("CHAN1_MTD_COEFF holds mtd.tones sections of 5 (%ld)",
		    (long)(sizeof(V21_CHAN1_MTD_COEFF) / sizeof(short)),
		    2 * 5, 0);
	diff_eq_int("CHAN2_MTD_COEFF holds mtd.tones sections of 5 (%ld)",
		    (long)(sizeof(V21_CHAN2_MTD_COEFF) / sizeof(short)),
		    2 * 5, 0);
	/* Both AGC arrays are the two-element shape layer 4 re-derives from
	 * the DC gain, and not one `int`. */
	diff_eq_int("AGC_DEF_ALPHA_v21 holds 2 entries (%ld)",
		    (long)(sizeof(AGC_DEF_ALPHA_v21) / sizeof(short)), 2, 0);
	diff_eq_int("AGC_DEF_BETA_v21 holds 2 entries (%ld)",
		    (long)(sizeof(AGC_DEF_BETA_v21) / sizeof(short)), 2, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

static int
test_values(void)
{
	diff_begin("v21cfg: table values against the blob");

	cmp_shorts("AGC_DEF_ALPHA_v21[%ld]", AGC_DEF_ALPHA_v21,
		   ref_AGC_DEF_ALPHA_v21, 2);
	cmp_shorts("AGC_DEF_BETA_v21[%ld]", AGC_DEF_BETA_v21,
		   ref_AGC_DEF_BETA_v21, 2);
	cmp_shorts("V21RX_IIR_LPF[%ld]", V21RX_IIR_LPF,
		   ref_V21RX_IIR_LPF, 15);
	cmp_shorts("V21RX_CHAN2_INTRP[%ld]", V21RX_CHAN2_INTRP,
		   ref_V21RX_CHAN2_INTRP, 15);
	cmp_shorts("V21RX_CHAN1_INTRP[%ld]", V21RX_CHAN1_INTRP,
		   ref_V21RX_CHAN1_INTRP, 15);
	cmp_shorts("V21_MRF_FILT[%ld]", V21_MRF_FILT, ref_V21_MRF_FILT, 360);
	cmp_shorts("V21_CHAN1_MTD_COEFF[%ld]", V21_CHAN1_MTD_COEFF,
		   ref_V21_CHAN1_MTD_COEFF, 10);

	/* `AGCv21_CFG` field by field.  The two pointers are compared by what
	 * they reach, which is all a differential test can say about an
	 * address. */
	diff_eq_int("AGCv21_CFG.ref_level (%ld)", AGCv21_CFG.ref_level,
		    ref_AGCv21_CFG.ref_level, 0);
	diff_eq_int("AGCv21_CFG.acquire_level (%ld)",
		    AGCv21_CFG.acquire_level, ref_AGCv21_CFG.acquire_level, 0);
	diff_eq_int("AGCv21_CFG.squelch_level (%ld)",
		    AGCv21_CFG.squelch_level, ref_AGCv21_CFG.squelch_level, 0);
	diff_eq_int("AGCv21_CFG.f06 (%ld)", AGCv21_CFG.f06,
		    ref_AGCv21_CFG.f06, 0);
	diff_eq_int("AGCv21_CFG.f08 (%ld)", AGCv21_CFG.f08,
		    ref_AGCv21_CFG.f08, 0);
	diff_eq_int("AGCv21_CFG.block_len (%ld)", AGCv21_CFG.block_len,
		    ref_AGCv21_CFG.block_len, 0);
	diff_eq_int("AGCv21_CFG.f14 (%ld)", AGCv21_CFG.f14,
		    ref_AGCv21_CFG.f14, 0);
	diff_eq_int("AGCv21_CFG.f16 (%ld)", AGCv21_CFG.f16,
		    ref_AGCv21_CFG.f16, 0);
	cmp_shorts("AGCv21_CFG.alpha[%ld]", AGCv21_CFG.alpha,
		   ref_AGCv21_CFG.alpha, 2);
	cmp_shorts("AGCv21_CFG.beta[%ld]", AGCv21_CFG.beta,
		   ref_AGCv21_CFG.beta, 2);
	/*
	 * The relocation sweep said the two pointers in this table reach the
	 * two arrays above and not some other copy.  On our side that is a
	 * pointer identity and can be asserted directly; on the blob's it is
	 * what the byte comparison two lines up already proved.
	 */
	diff_eq_int("AGCv21_CFG.alpha is AGC_DEF_ALPHA_v21 (%ld)",
		    AGCv21_CFG.alpha == AGC_DEF_ALPHA_v21, 1, 0);
	diff_eq_int("AGCv21_CFG.beta is AGC_DEF_BETA_v21 (%ld)",
		    AGCv21_CFG.beta == AGC_DEF_BETA_v21, 1, 0);

	/* `V21RX_CFG` field by field. */
	diff_eq_int("V21RX_CFG.chan2 (%ld)", V21RX_CFG.chan2,
		    ref_V21RX_CFG.chan2, 0);
	diff_eq_int("V21RX_CFG.short_0002 (%ld)", V21RX_CFG.short_0002,
		    ref_V21RX_CFG.short_0002, 0);
	diff_eq_int("V21RX_CFG.bit_rate (%ld)", V21RX_CFG.bit_rate,
		    ref_V21RX_CFG.bit_rate, 0);
	diff_eq_int("V21RX_CFG.short_0006 (%ld)", V21RX_CFG.short_0006,
		    ref_V21RX_CFG.short_0006, 0);
	diff_eq_int("V21RX_CFG.int_0008 (%ld)", V21RX_CFG.int_0008,
		    ref_V21RX_CFG.int_0008, 0);
	diff_eq_int("V21RX_CFG.int_000c (%ld)", V21RX_CFG.int_000c,
		    ref_V21RX_CFG.int_000c, 0);
	diff_eq_int("V21RX_CFG.int_0010 (%ld)", V21RX_CFG.int_0010,
		    ref_V21RX_CFG.int_0010, 0);
	diff_eq_int("V21RX_CFG.aux is null on both sides (%ld)",
		    V21RX_CFG.aux == 0 && ref_V21RX_CFG.aux == 0, 1, 0);
	/* V.21's only bit rate, and the object's own literal. */
	diff_eq_int("V21RX_CFG.bit_rate is 300 (%ld)", V21RX_CFG.bit_rate,
		    300, 0);

	/*
	 * `FPM_FSD_CFG` field by field.  This is the blob's own symbol; the
	 * `FPM_FSD_CFG_data` stub beside it is D1180's duplicate and is
	 * asserted equal to it below, which is the measurement that entry
	 * rests on.
	 */
	diff_eq_int("FPM_FSD_CFG.fir_taps (%ld)", FPM_FSD_CFG.fir_taps,
		    ref_FPM_FSD_CFG.fir_taps, 0);
	diff_eq_int("FPM_FSD_CFG.delay (%ld)", FPM_FSD_CFG.delay,
		    ref_FPM_FSD_CFG.delay, 0);
	diff_eq_int("FPM_FSD_CFG.iir_len (%ld)", FPM_FSD_CFG.iir_len,
		    ref_FPM_FSD_CFG.iir_len, 0);
	diff_eq_int("FPM_FSD_CFG.slice_level (%ld)", FPM_FSD_CFG.slice_level,
		    ref_FPM_FSD_CFG.slice_level, 0);
	diff_eq_int("FPM_FSD_CFG.high_bit (%ld)", FPM_FSD_CFG.high_bit,
		    ref_FPM_FSD_CFG.high_bit, 0);
	diff_eq_int("FPM_FSD_CFG.bit_samples (%ld)", FPM_FSD_CFG.bit_samples,
		    ref_FPM_FSD_CFG.bit_samples, 0);
	diff_eq_int("FPM_FSD_CFG.max_bits (%ld)", FPM_FSD_CFG.max_bits,
		    ref_FPM_FSD_CFG.max_bits, 0);
	diff_eq_int("FPM_FSD_CFG.trace_len (%ld)", FPM_FSD_CFG.trace_len,
		    ref_FPM_FSD_CFG.trace_len, 0);
	diff_eq_int("FPM_FSD_CFG.f18 (%ld)", FPM_FSD_CFG.f18,
		    ref_FPM_FSD_CFG.f18, 0);
	diff_eq_int("FPM_FSD_CFG.pad1a (%ld)", FPM_FSD_CFG.pad1a,
		    ref_FPM_FSD_CFG.pad1a, 0);
	/* Neither filter is in the table: every caller supplies both. */
	diff_eq_int("FPM_FSD_CFG.fir is null on both sides (%ld)",
		    FPM_FSD_CFG.fir == 0 && ref_FPM_FSD_CFG.fir == 0, 1, 0);
	diff_eq_int("FPM_FSD_CFG.iir is null on both sides (%ld)",
		    FPM_FSD_CFG.iir == 0 && ref_FPM_FSD_CFG.iir == 0, 1, 0);

	/*
	 * D1180's claim, measured.  The `_data` stub and the object's own
	 * symbol must hold identical values, or the duplicate is a divergence
	 * rather than a redundancy.
	 */
	diff_eq_int("FPM_FSD_CFG_data equals FPM_FSD_CFG (%ld)",
		    memcmp(&FPM_FSD_CFG_data, &FPM_FSD_CFG,
			   sizeof(FPM_FSD_CFG)) == 0, 1, 0);

	/*
	 * Anti-vacuity.  Three of these tables would compare equal to a block
	 * of zeros, so say how many entries actually carry a value.
	 */
	diff_eq_int("V21_MRF_FILT is not mostly zero (%ld)",
		    nonzero(V21_MRF_FILT, 360) > 300, 1, 0);
	diff_eq_int("V21RX_IIR_LPF has no zero entry (%ld)",
		    nonzero(V21RX_IIR_LPF, 15), 15, 0);
	diff_eq_int("V21_CHAN1_MTD_COEFF has no zero entry (%ld)",
		    nonzero(V21_CHAN1_MTD_COEFF, 10), 10, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

static int
test_detector_fires(void)
{
	static short copy[360];
	int i;

	struct { const short *p; int n; const char *name; } tab[] = {
		{ AGC_DEF_ALPHA_v21,     2, "AGC_DEF_ALPHA_v21"   },
		{ AGC_DEF_BETA_v21,      2, "AGC_DEF_BETA_v21"    },
		{ V21RX_IIR_LPF,        15, "V21RX_IIR_LPF"       },
		{ V21RX_CHAN2_INTRP,    15, "V21RX_CHAN2_INTRP"   },
		{ V21RX_CHAN1_INTRP,    15, "V21RX_CHAN1_INTRP"   },
		{ V21_MRF_FILT,        360, "V21_MRF_FILT"        },
		{ V21_CHAN1_MTD_COEFF,  10, "V21_CHAN1_MTD_COEFF" },
		{ V21_CHAN2_MTD_COEFF,  10, "V21_CHAN2_MTD_COEFF" }
	};
	const int ntab = (int)(sizeof(tab) / sizeof(tab[0]));

	diff_begin("v21cfg: the comparison rejects a perturbed table");

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
 * Layer 4.  Facts about the values that hold independently of the blob.
 */
static int
test_value_shape(void)
{
	int i;

	diff_begin("v21cfg: the shape of the values");

	/*
	 * The resampler's prototype is a linear-phase FIR: symmetric about its
	 * centre across the whole 360 taps, not merely within a branch.  All
	 * 180 pairs, so a single transposed entry is caught.
	 */
	for (i = 0; i < 180; i++)
		diff_eq_int("V21_MRF_FILT is symmetric (%ld)",
			    V21_MRF_FILT[i], V21_MRF_FILT[359 - i], i);

	/*
	 * The AGC smoother `y += alpha*y + beta*x` has unity DC gain when
	 * alpha + beta is 32768 in Q15.  BOTH elements satisfy it exactly,
	 * which is what fixes the arrays at two `short` rather than one `int`
	 * -- and it is a property three of the object's six other copies do
	 * NOT have (D6), so it is a real check and not a tautology.
	 */
	for (i = 0; i < 2; i++)
		diff_eq_int("AGC alpha+beta is unity DC gain (%ld)",
			    (int)AGC_DEF_ALPHA_v21[i] + AGC_DEF_BETA_v21[i],
			    32768, i);

	return diff_end();
}

/*
 * The two tone-detector banks ARE resonators at V.21's own four frequencies,
 * and this is the evidence that fixes their element type and their stride
 * independently of `st_size`.
 *
 * Each bank is `mtd.tones` = 2 sections of five shorts in Q14, and each
 * section has the form
 *
 *      { -r^2, 1.0, 2*r*cos(w), -2*cos(w), 1.0 }
 *
 * with r = 0.9 and w = 2*pi*f/8000.  Rounding that to integers reproduces all
 * twenty entries of both banks EXACTLY from four published numbers -- 980 and
 * 1180 Hz for channel 1, 1650 and 1850 Hz for channel 2, which are V.21's mark
 * and space tones as the Recommendation defines them.  A reading that made
 * these anything but shorts in Q14, five to a section, would not produce them.
 *
 * WHY THE COMPARISON ALLOWS +/-1 RATHER THAN DEMANDING THE EXACT ROUND.  All
 * twenty ARE exact, and the tightest margin from a rounding boundary is
 * 0.0346 -- far outside anything two libm implementations disagree by.  But
 * this file is compiled by two compilers against two C libraries, `cos` is not
 * required to be correctly rounded by either, and the claim being made here is
 * about the STRIDE and the SCALING, which a unit of slack cannot weaken: a
 * wrong Q, a wrong r or a wrong section length is out by thousands, not by
 * one.  Layer 2 already pins every byte against the blob exactly, so nothing
 * is lost by making layer 4 robust.  Finding F9350.
 */
static int
test_mtd_is_v21_tones(void)
{
	static const struct {
		const short *tab;
		const char *name;
		int f[2];
	} bank[2] = {
		{ V21_CHAN1_MTD_COEFF, "V21_CHAN1_MTD_COEFF",
		  { V21_CHAN1_MARK_HZ, V21_CHAN1_SPACE_HZ } },
		{ V21_CHAN2_MTD_COEFF, "V21_CHAN2_MTD_COEFF",
		  { V21_CHAN2_MARK_HZ, V21_CHAN2_SPACE_HZ } }
	};
	const double q = 16384.0;
	const double r = 0.9;
	int b, s;

	diff_begin("v21cfg: the MTD banks are V.21's own four tones");

	for (b = 0; b < 2; b++) {
		for (s = 0; s < 2; s++) {
			const short *sec = bank[b].tab + s * 5;
			double w = 2.0 * 3.14159265358979323846
				 * (double)bank[b].f[s]
				 / (double)V21_MTD_SAMPLE_RATE;
			double pred[5];
			int k;

			pred[0] = -r * r * q;
			pred[1] = q;
			pred[2] = 2.0 * r * cos(w) * q;
			pred[3] = -2.0 * cos(w) * q;
			pred[4] = q;

			for (k = 0; k < 5; k++) {
				long want = (long)(pred[k] < 0
						   ? -(long)(-pred[k] + 0.5)
						   :  (long)(pred[k] + 0.5));
				long got = sec[k];
				long err = got > want ? got - want
						      : want - got;

				diff_eq_int("resonator coefficient (%ld)",
					    err <= 1, 1, b * 10 + s * 5 + k);
			}

			/* The two rails and the damping are shared by every
			 * section of both banks, which is what makes the
			 * five-short stride visible without the cosine. */
			diff_eq_int("section damping is -r^2 in Q14 (%ld)",
				    sec[0], -13271, b * 2 + s);
			diff_eq_int("section rail 1 is unity in Q14 (%ld)",
				    sec[1], 16384, b * 2 + s);
			diff_eq_int("section rail 2 is unity in Q14 (%ld)",
				    sec[4], 16384, b * 2 + s);
		}
	}

	/*
	 * The four frequencies are ordered, and the resonator coefficient
	 * falls monotonically with frequency over the whole 980-1850 Hz span.
	 * Pure integer, so it holds whatever `cos` does.
	 */
	diff_eq_int("980 Hz above 1180 Hz (%ld)",
		    V21_CHAN1_MTD_COEFF[2] > V21_CHAN1_MTD_COEFF[7], 1, 0);
	diff_eq_int("1180 Hz above 1650 Hz (%ld)",
		    V21_CHAN1_MTD_COEFF[7] > V21_CHAN2_MTD_COEFF[2], 1, 0);
	diff_eq_int("1650 Hz above 1850 Hz (%ld)",
		    V21_CHAN2_MTD_COEFF[2] > V21_CHAN2_MTD_COEFF[7], 1, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

static int
test_use_agc(void)
{
	struct fpm_agc a, b;
	int reset;

	diff_begin("v21cfg: AGCv21_CFG through FPM_AGC_init");

	for (reset = 0; reset <= 1; reset++) {
		memset(&a, 0xa5, sizeof(a));
		memset(&b, 0xa5, sizeof(b));

		FPM_AGC_init(&a, &AGCv21_CFG, reset);
		ref_FPM_AGC_init(&b, &ref_AGCv21_CFG, reset);

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
		 * only thing the two sides can agree about -- and this is the
		 * route by which the blob's own AGC arrays are compared. */
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
test_use_mrf(void)
{
	static struct fpm_mrf a, b;
	struct fpm_mrf_cfg ca, cb;

	diff_begin("v21cfg: V21_MRF_FILT through FPM_MRF_init");

	memset(&a, 0xa5, sizeof(a));
	memset(&b, 0xa5, sizeof(b));

	/*
	 * Exactly as `V21RX_create` builds it at 0x098f0c-0x098f57: copy
	 * `FPM_MRF_CFG`, then write branches 9, decimate 10, the filter, and
	 * taps 360.  The first two already equal the built-in's, which the
	 * object writes anyway.
	 */
	ca = FPM_MRF_CFG;
	cb = FPM_MRF_CFG;
	ca.branches = cb.branches = 9;
	ca.decimate = cb.decimate = 10;
	ca.taps = cb.taps = 360;
	ca.coeff = V21_MRF_FILT;
	cb.coeff = ref_V21_MRF_FILT;
	ca.aux = cb.aux = 0;

	FPM_MRF_init(&a, &ca, 1);
	ref_FPM_MRF_init(&b, &cb, 1);

	diff_eq_int("mrf.cfg.branches (%ld)", a.cfg.branches, b.cfg.branches,
		    0);
	diff_eq_int("mrf.cfg.decimate (%ld)", a.cfg.decimate, b.cfg.decimate,
		    0);
	diff_eq_int("mrf.cfg.taps (%ld)", a.cfg.taps, b.cfg.taps, 0);
	diff_eq_int("mrf.history_len (%ld)", a.history_len, b.history_len, 0);
	/* The length field's second reading: taps / branches. */
	diff_eq_int("mrf.history_len is taps/branches (%ld)",
		    a.history_len, 40, 0);
	diff_eq_int("mrf.need (%ld)", a.need, b.need, 0);
	diff_eq_int("mrf.phase (%ld)", a.phase, b.phase, 0);
	diff_eq_int("mrf.widx (%ld)", a.widx, b.widx, 0);
	cmp_shorts("mrf.cfg.coeff[%ld]", a.cfg.coeff, b.cfg.coeff, 360);
	cmp_shorts("mrf.history[%ld]", a.history, b.history, 40);

	FPM_MRF_free(&a);
	ref_FPM_MRF_free(&b);

	return diff_end();
}

static int
test_use_fsd(void)
{
	static struct fpm_fsd a, b;
	struct fpm_fsd_cfg ca, cb;
	int chan2;

	diff_begin("v21cfg: the FSD tables through FPM_FSD_init");

	/*
	 * Both arms of `V21RX_create`'s `chan2` test, built exactly as the
	 * object builds them at 0x098f79-0x09901b: copy `FPM_FSD_CFG`, then
	 * write the interpolator and `delay` from whichever arm was taken, and
	 * `fir_taps` 15, the lowpass, `iir_len` 3, `high_bit` 0 and
	 * `bit_samples` 24 in both.
	 */
	for (chan2 = 0; chan2 <= 1; chan2++) {
		memset(&a, 0xa5, sizeof(a));
		memset(&b, 0xa5, sizeof(b));

		ca = FPM_FSD_CFG;
		cb = ref_FPM_FSD_CFG;

		ca.fir = chan2 ? V21RX_CHAN2_INTRP : V21RX_CHAN1_INTRP;
		cb.fir = chan2 ? ref_V21RX_CHAN2_INTRP
			       : ref_V21RX_CHAN1_INTRP;
		ca.delay = cb.delay = (short)(chan2 ? 3 : 5);
		ca.fir_taps = cb.fir_taps = 15;
		ca.iir = V21RX_IIR_LPF;
		cb.iir = ref_V21RX_IIR_LPF;
		ca.iir_len = cb.iir_len = 3;
		ca.high_bit = cb.high_bit = 0;
		ca.bit_samples = cb.bit_samples = 24;
		ca.f18 = cb.f18 = 0;
		ca.pad1a = cb.pad1a = 0;

		FPM_FSD_init(&a, &ca, 1);
		ref_FPM_FSD_init(&b, &cb, 1);

		diff_eq_int("fsd.cfg.fir_taps (%ld)", a.cfg.fir_taps,
			    b.cfg.fir_taps, chan2);
		diff_eq_int("fsd.cfg.delay (%ld)", a.cfg.delay, b.cfg.delay,
			    chan2);
		diff_eq_int("fsd.cfg.iir_len (%ld)", a.cfg.iir_len,
			    b.cfg.iir_len, chan2);
		diff_eq_int("fsd.cfg.slice_level (%ld)", a.cfg.slice_level,
			    b.cfg.slice_level, chan2);
		diff_eq_int("fsd.cfg.high_bit (%ld)", a.cfg.high_bit,
			    b.cfg.high_bit, chan2);
		diff_eq_int("fsd.cfg.bit_samples (%ld)", a.cfg.bit_samples,
			    b.cfg.bit_samples, chan2);
		diff_eq_int("fsd.cfg.max_bits (%ld)", a.cfg.max_bits,
			    b.cfg.max_bits, chan2);
		diff_eq_int("fsd.cfg.trace_len (%ld)", a.cfg.trace_len,
			    b.cfg.trace_len, chan2);
		diff_eq_int("fsd.last_count (%ld)", a.last_count,
			    b.last_count, chan2);
		diff_eq_int("fsd.f22 (%ld)", a.f22, b.f22, chan2);
		diff_eq_int("fsd.hist_idx (%ld)", a.hist_idx, b.hist_idx,
			    chan2);
		diff_eq_int("fsd.bit (%ld)", a.bit, b.bit, chan2);
		diff_eq_int("fsd.since_bit (%ld)", a.since_bit, b.since_bit,
			    chan2);
		diff_eq_int("fsd.disagreements (%ld)", a.disagreements,
			    b.disagreements, chan2);

		/* Both filters reached through the configuration, and the
		 * three buffers init sized from three separate counts. */
		cmp_shorts("fsd.cfg.fir[%ld]", a.cfg.fir, b.cfg.fir, 15);
		cmp_shorts("fsd.cfg.iir[%ld]", a.cfg.iir, b.cfg.iir, 15);
		cmp_shorts("fsd.fir_hist[%ld]", a.fir_hist, b.fir_hist, 15);
		cmp_shorts("fsd.iir_hist[%ld]", a.iir_hist, b.iir_hist,
			   2 * 3);
		cmp_shorts("fsd.trace[%ld]", a.trace, b.trace, 160);

		FPM_FSD_free(&a);
		ref_FPM_FSD_free(&b);
	}

	return diff_end();
}

static int
test_use_mtd(void)
{
	static struct fpm_mtd a, b;
	static short acc_a[8], acc_b[8];
	struct fpm_mtd_cfg ca, cb;
	int chan2;

	diff_begin("v21cfg: the two MTD banks through FPM_MTD_create");

	/*
	 * Both arms, with the thresholds `V21RX_create` writes beside the
	 * bank at 0x099060-0x099074: tones 2, ratio 0x4ccd (0.6 in Q15) and
	 * min_level 300, which is V.21's own 300 bit/s literal.
	 */
	for (chan2 = 0; chan2 <= 1; chan2++) {
		memset(&a, 0xa5, sizeof(a));
		memset(&b, 0xa5, sizeof(b));
		a.acc = acc_a;
		b.acc = acc_b;

		ca = FPM_MTD_CFG;
		cb = ref_FPM_MTD_CFG;
		ca.coeff = chan2 ? V21_CHAN2_MTD_COEFF
				 : V21_CHAN1_MTD_COEFF;
		cb.coeff = chan2 ? ref_V21_CHAN2_MTD_COEFF
				 : ref_V21_CHAN1_MTD_COEFF;
		ca.tones = cb.tones = 2;
		ca.ratio = cb.ratio = 0x4ccd;
		ca.min_level = cb.min_level = 300;

		FPM_MTD_create(&a, &ca);
		ref_FPM_MTD_create(&b, &cb);

		diff_eq_int("mtd.cfg.tones (%ld)", a.cfg.tones, b.cfg.tones,
			    chan2);
		diff_eq_int("mtd.cfg.ratio (%ld)", a.cfg.ratio, b.cfg.ratio,
			    chan2);
		diff_eq_int("mtd.cfg.min_level (%ld)", a.cfg.min_level,
			    b.cfg.min_level, chan2);
		diff_eq_int("mtd.cfg.f0a (%ld)", a.cfg.f0a, b.cfg.f0a, chan2);
		cmp_shorts("mtd.cfg.coeff[%ld]", a.cfg.coeff, b.cfg.coeff, 10);
		diff_eq_int("mtd.dc_state[0] (%ld)", a.dc_state[0],
			    b.dc_state[0], chan2);
		diff_eq_int("mtd.dc_state[1] (%ld)", a.dc_state[1],
			    b.dc_state[1], chan2);
		diff_eq_int("mtd.out_of_band (%ld)", a.out_of_band,
			    b.out_of_band, chan2);
		diff_eq_int("mtd.wideband (%ld)", a.wideband, b.wideband,
			    chan2);
		cmp_shorts("mtd.acc[%ld]", a.acc, b.acc, 2 * 2);
	}

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
	rc |= test_mtd_is_v21_tones();
	rc |= test_use_agc();
	rc |= test_use_mrf();
	rc |= test_use_fsd();
	rc |= test_use_mtd();

	return rc;
}

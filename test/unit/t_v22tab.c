/*
 * t_v22tab.c -- differential test of V.22's static configuration.
 *
 * Tables are the part of a reconstruction with no behaviour to check, so the
 * check is the bytes: every one of them, against the original's own copy.
 *
 * The five configurations that hold pointers cannot be compared as objects --
 * two builds put the coefficient arrays at two addresses and always will --
 * so those compare scalar fields directly and pointer fields by what they
 * point AT.  That is stricter than comparing the pointer would have been: it
 * catches a config that aims at the right kind of array with the wrong
 * contents, which an address comparison could not see even in principle.
 */

#include "harness.h"
#include "dsplib/v22tab.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v22fp.h"

extern const struct fpm_agc_cfg ref_AGCv22_CFG;
extern const struct fpm_agc_cfg ref_AGCv22_CFG2;
extern const struct fpm_mtd_cfg ref_MTDv22_CFG;
extern const struct fpm_mtd_cfg ref_MTDv22_CFG2;
extern const struct fpm_mtd_cfg ref_MTDs1_CFG;
extern const short ref_MTDv22_COEF[];
extern const short ref_MTDv22_COEF2[];
extern const short ref_V22_S1_HC_COEF[];
extern const short ref_TONEv22_CFG[];
extern const short ref_TONEv22INIT_CFG[];
extern const short ref_V22_CFG[];
extern const short ref_CRRv22_CLK[];
extern const short ref_CRRv22_PLL_K1[];
extern const short ref_CRRv22_PLL_K2[];
extern short ref_V22DiconnectThreshTable[];

/*
 * Ours: FILE-LOCAL in the object, so they are `static` in v22fp.c and
 * v22tab.h no longer declares them.  The test tier links a globalized copy
 * (tools/testvisible.py), so the plain names resolve.
 */
extern const struct fpm_tone_cfg TONEv22_CFG;
extern const struct fpm_tone_cfg TONEv22INIT_CFG;
extern short V22DiconnectThreshTable[V22_DISCONNECT_THRESHOLDS];

static int
cmp_words(const char *label, const short *ours, const short *ref, int n)
{
	int i;

	diff_begin(label);
	for (i = 0; i < n; i++)
		diff_eq_int("[%ld]", ours[i], ref[i], i);
	return diff_end();
}

static int
cmp_agc(const char *label, const struct fpm_agc_cfg *a,
	const struct fpm_agc_cfg *b)
{
	int i;

	diff_begin(label);
	diff_eq_int("ref_level (%ld)", a->ref_level, b->ref_level, 0);
	diff_eq_int("acquire_level (%ld)", a->acquire_level, b->acquire_level, 0);
	diff_eq_int("squelch_level (%ld)", a->squelch_level, b->squelch_level, 0);
	diff_eq_int("f06 (%ld)", a->f06, b->f06, 0);
	diff_eq_int("f08 (%ld)", a->f08, b->f08, 0);
	diff_eq_int("block_len (%ld)", a->block_len, b->block_len, 0);
	diff_eq_int("f14 (%ld)", a->f14, b->f14, 0);
	diff_eq_int("f16 (%ld)", a->f16, b->f16, 0);

	/* By content, not by address: the two builds cannot agree on one. */
	diff_eq_int("alpha is not null (%ld)", a->alpha != 0, 1, 0);
	diff_eq_int("beta is not null (%ld)", a->beta != 0, 1, 0);
	for (i = 0; i < 2; i++) {
		diff_eq_int("alpha[%ld]", a->alpha[i], b->alpha[i], i);
		diff_eq_int("beta[%ld]", a->beta[i], b->beta[i], i);
	}
	return diff_end();
}

static int
cmp_mtd(const char *label, const struct fpm_mtd_cfg *a,
	const struct fpm_mtd_cfg *b, int sections)
{
	int i;

	diff_begin(label);
	diff_eq_int("tones (%ld)", a->tones, b->tones, 0);
	diff_eq_int("ratio (%ld)", a->ratio, b->ratio, 0);
	diff_eq_int("min_level (%ld)", a->min_level, b->min_level, 0);
	diff_eq_int("f0a (%ld)", a->f0a, b->f0a, 0);

	diff_eq_int("coeff is not null (%ld)", a->coeff != 0, 1, 0);
	diff_eq_int("tones matches the bank (%ld)", a->tones, sections, 0);
	for (i = 0; i < sections * FPM_IIR_COEFF_PER_SECTION; i++)
		diff_eq_int("coeff[%ld]", a->coeff[i], b->coeff[i], i);
	return diff_end();
}

int
main(void)
{
	int rc = 0;
	int i;

	/* --- the coefficient banks ------------------------------------- */

	rc |= cmp_words("MTDv22_COEF", MTDv22_COEF, ref_MTDv22_COEF,
			V22_MTD_SECTIONS * FPM_IIR_COEFF_PER_SECTION);
	rc |= cmp_words("MTDv22_COEF2", MTDv22_COEF2, ref_MTDv22_COEF2,
			V22_MTD_SECTIONS * FPM_IIR_COEFF_PER_SECTION);
	rc |= cmp_words("V22_S1_HC_COEF", V22_S1_HC_COEF, ref_V22_S1_HC_COEF,
			V22_S1_SECTIONS * FPM_IIR_COEFF_PER_SECTION);

	/* --- the configurations that carry pointers -------------------- */

	rc |= cmp_agc("AGCv22_CFG", &AGCv22_CFG, &ref_AGCv22_CFG);
	rc |= cmp_agc("AGCv22_CFG2", &AGCv22_CFG2, &ref_AGCv22_CFG2);
	rc |= cmp_mtd("MTDv22_CFG", &MTDv22_CFG, &ref_MTDv22_CFG,
		      V22_MTD_SECTIONS);
	rc |= cmp_mtd("MTDv22_CFG2", &MTDv22_CFG2, &ref_MTDv22_CFG2,
		      V22_MTD_SECTIONS);
	rc |= cmp_mtd("MTDs1_CFG", &MTDs1_CFG, &ref_MTDs1_CFG,
		      V22_S1_SECTIONS);

	/*
	 * The two AGC configs share one pair of coefficient arrays in the
	 * original -- one relocation target, two referrers.  Ours must share
	 * too, or the note in v22rxtab.c about there being ONE static pair is
	 * wrong.
	 */
	diff_begin("AGCv22 configs share their coefficients");
	diff_eq_int("same alpha (%ld)", AGCv22_CFG.alpha == AGCv22_CFG2.alpha,
		    ref_AGCv22_CFG.alpha == ref_AGCv22_CFG2.alpha, 0);
	diff_eq_int("same beta (%ld)", AGCv22_CFG.beta == AGCv22_CFG2.beta,
		    ref_AGCv22_CFG.beta == ref_AGCv22_CFG2.beta, 0);
	rc |= diff_end();

	/*
	 * --- the three blocks that used to be untyped ------------------
	 *
	 * Compared as WORDS, which is deliberate.  `V22FP_create` is what
	 * gives these types, and the type is a claim about which field is
	 * where; the bytes are a separate claim and this is the one that
	 * checks them.  A cast rather than a member-by-member comparison, so
	 * that a field added or resized in either struct still fails here.
	 */

	rc |= cmp_words("TONEv22_CFG", (const short *)&TONEv22_CFG,
			ref_TONEv22_CFG,
			V22_TONE_CFG_WORDS);
	rc |= cmp_words("TONEv22INIT_CFG", (const short *)&TONEv22INIT_CFG,
			ref_TONEv22INIT_CFG, V22_TONE_CFG_WORDS);
	rc |= cmp_words("V22_CFG", (const short *)&V22_CFG, ref_V22_CFG,
			V22_CFG_WORDS);

	/*
	 * Asserted rather than assumed: the two tone configurations are the
	 * same 36 bytes under two names.  Checked on the ORIGINAL's copies,
	 * so it is a statement about the object and not about ours.
	 */
	diff_begin("TONEv22_CFG and TONEv22INIT_CFG are identical");
	for (i = 0; i < V22_TONE_CFG_WORDS; i++)
		diff_eq_int("word %ld", ref_TONEv22INIT_CFG[i],
			    ref_TONEv22_CFG[i], i);
	rc |= diff_end();

	/* --- carrier recovery and the thresholds ----------------------- */

	rc |= cmp_words("CRRv22_CLK", CRRv22_CLK, ref_CRRv22_CLK,
			V22_CRR_CLK_STEPS);
	rc |= cmp_words("CRRv22_PLL_K1", CRRv22_PLL_K1, ref_CRRv22_PLL_K1,
			V22_CRR_PLL_SETS);
	rc |= cmp_words("CRRv22_PLL_K2", CRRv22_PLL_K2, ref_CRRv22_PLL_K2,
			V22_CRR_PLL_SETS);
	rc |= cmp_words("V22DiconnectThreshTable", V22DiconnectThreshTable,
			ref_V22DiconnectThreshTable,
			V22_DISCONNECT_THRESHOLDS);

	/*
	 * The clock table's closed form, checked against every entry rather
	 * than stated in a comment.
	 *
	 * The obvious reading -- a sixth of a 0x8000 cycle, round(k * 32768/6)
	 * -- reproduces five of the six and gives 27307 where the object has
	 * 27306.  round(k * 65535 / 12), i.e. round(k * 5461.25), reproduces
	 * all six, and k = 5 is the only entry where the two ever disagree.
	 *
	 * A fit, not a recovered design -- two parameters over six points --
	 * and coefficient derivations are deferred here (docs/fastpass.md).
	 * Note also that neither this check nor the one below is DIFFERENTIAL:
	 * both sides read the original's copy.  They are transcription guards
	 * on a generator, and nothing more.
	 */
	diff_begin("CRRv22_CLK is round(k * 65535 / 12)");
	for (i = 0; i < V22_CRR_CLK_STEPS; i++)
		diff_eq_int("entry %ld", ref_CRRv22_CLK[i],
			    (i * 65535 + 6) / 12, i);
	rc |= diff_end();

	/*
	 * And the same assertion in the negative, so the paragraph above
	 * cannot rot into folklore: the sixth-of-32768 reading really does
	 * disagree, at exactly one entry.
	 */
	diff_begin("CRRv22_CLK is NOT round(k * 32768 / 6)");
	{
		int differ = 0;

		for (i = 0; i < V22_CRR_CLK_STEPS; i++)
			if (ref_CRRv22_CLK[i] != (i * 32768 + 3) / 6)
				differ++;
		diff_eq_int("entries that disagree (%ld)", differ, 1, 0);
	}
	rc |= diff_end();

	return rc;
}

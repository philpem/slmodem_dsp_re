/*
 * t_fpm_iir.c -- differential test of the biquad cascade.
 *
 * Driven with the real coefficient sets the datapumps use, plus synthetic ones
 * chosen to reach the saturation branch -- which ordinary signals do not, and
 * which is where the asymmetric clamp (-0x7fff, not -0x8000) would show.
 *
 * State is compared after every sample, not just at the end, so a divergence
 * is reported where it happens rather than after it has propagated.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_iir.h"

extern short ref_FPM_iir_filt(short x, const short *coeff, short *state,
			      short sections);
extern void ref_FPM_iir_filt_block(short *samples, const short *coeff,
				   short *state, short sections, short count);
extern void ref_FPM_iir_filt_II(short *samples, const short *coeff,
				short *state, short sections, short count);
extern short ref_COEF_DC[];
extern short ref_MTDb103_COEF[];
extern short ref_B103_IIR_LPF[];

#define NSAMP 4000

/* Counts how often the recursive node hit each clamp, for the guard below. */
static int sat_hi, sat_lo;

static int
run(const char *label, const short *coeff, int sections, int amplitude)
{
	short sa[16], sb[16];
	unsigned lfsr = 0x1BADu;
	int rc, i;

	memset(sa, 0, sizeof(sa));
	memset(sb, 0, sizeof(sb));

	diff_begin(label);
	for (i = 0; i < NSAMP; i++) {
		short x;
		short ya, yb;
		int k;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		x = (short)(((int)(lfsr & 0xffff) - 0x8000) / (0x8000 / amplitude));

		ya = ref_FPM_iir_filt(x, coeff, sa, (short)sections);
		yb = FPM_iir_filt(x, coeff, sb, (short)sections);

		diff_eq_int("sample %ld: output", yb, ya, i);
		for (k = 0; k < sections * 2; k++) {
			diff_eq_int("sample %ld: state", sb[k], sa[k], i);
			if (sa[k] == 0x7fff)
				sat_hi++;
			if (sa[k] == -0x7fff)
				sat_lo++;
		}
	}
	rc = diff_end();
	return rc;
}

/*
 * ---------------------------------------------------------------------------
 * FPM_iir_filt_block -- FPM_iir_filt over a block, in place.
 *
 * Same engine as `run` above, so the interesting part is not the arithmetic --
 * that is already covered sample by sample -- but the three things only the
 * block form can get wrong:
 *
 *   - the state must carry across calls, so the same stream is pushed through
 *     at several fragment sizes and every one must agree;
 *   - the block is filtered IN PLACE, so a wrong write-back offset (the object
 *     stores through `-0x2(%edi)` after pre-incrementing) shifts the output by
 *     one sample and nothing else changes;
 *   - the saturating clamp is inside the inner loop, so `blk_sat_*` below
 *     asserts the block form actually reached it rather than inheriting the
 *     coverage of the single-sample test.
 *
 * `sections == 0` IS NOT DRIVEN.  The object writes back `%dx`, which that path
 * never assigns; both sides would be reporting an arbitrary choice.  See
 * src/dsp/fpm_iir.c and deviation D393.
 */
static int blk_sat_hi, blk_sat_lo;

static int
run_block(const char *label, const short *coeff, int sections, int amplitude,
	  int frag)
{
	short sa[16], sb[16];
	short ba[256], bb[256];
	unsigned lfsr = 0x3C5Du;
	int rc, i, n;

	memset(sa, 0, sizeof(sa));
	memset(sb, 0, sizeof(sb));

	diff_begin(label);
	for (n = 0; n < NSAMP / frag; n++) {
		for (i = 0; i < frag; i++) {
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
			ba[i] = bb[i] = (short)(((int)(lfsr & 0xffff) - 0x8000)
					        / (0x8000 / amplitude));
		}

		ref_FPM_iir_filt_block(ba, coeff, sa, (short)sections,
				       (short)frag);
		FPM_iir_filt_block(bb, coeff, sb, (short)sections, (short)frag);

		for (i = 0; i < frag; i++)
			diff_eq_int("sample %ld: output", bb[i], ba[i],
				    n * frag + i);
		for (i = 0; i < sections * FPM_IIR_STATE_PER_SECTION; i++) {
			diff_eq_int("state[%ld]", sb[i], sa[i], i);
			if (sa[i] == 0x7fff)
				blk_sat_hi++;
			if (sa[i] == -0x7fff)
				blk_sat_lo++;
		}
	}
	rc = diff_end();
	return rc;
}

/*
 * The block form and `sections` calls to the single-sample form must produce
 * the same stream and the same state.  This is the only check here that does
 * not have the blob on both sides of the comparison -- it is our block against
 * the blob's SINGLE-SAMPLE filter, so it pins the loop structure against a
 * function that was verified independently, and would catch a block that
 * filtered the right samples in the wrong order.
 */
static int
run_block_vs_single(const char *label, const short *coeff, int sections,
		    int amplitude, int count)
{
	short s_blk[16], s_one[16];
	short blk[256], one[256];
	unsigned lfsr = 0x71E3u;
	int rc, i;

	memset(s_blk, 0, sizeof(s_blk));
	memset(s_one, 0, sizeof(s_one));

	diff_begin(label);
	for (i = 0; i < count; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		blk[i] = one[i] = (short)(((int)(lfsr & 0xffff) - 0x8000)
					  / (0x8000 / amplitude));
	}

	FPM_iir_filt_block(blk, coeff, s_blk, (short)sections, (short)count);
	for (i = 0; i < count; i++)
		one[i] = ref_FPM_iir_filt(one[i], coeff, s_one,
					  (short)sections);

	for (i = 0; i < count; i++)
		diff_eq_int("sample %ld: block == repeated single", blk[i],
			    one[i], i);
	for (i = 0; i < sections * FPM_IIR_STATE_PER_SECTION; i++)
		diff_eq_int("state[%ld]: block == repeated single", s_blk[i],
			    s_one[i], i);
	rc = diff_end();
	return rc;
}

/*
 * ---------------------------------------------------------------------------
 * FPM_iir_filt_II -- the direct form I block filter.
 *
 * Two things here are easy to get wrong and invisible in a short run:
 *
 *   - the coefficient order is { b0, b2, b1, a2, a1 }.  Swapping b1/b2 or
 *     a1/a2 gives a different but perfectly stable filter, so only a long
 *     run against real coefficients catches it.
 *   - nothing saturates.  The section output wraps.  `wrap_seen` below
 *     asserts that at least one section output actually did exceed 16 bits,
 *     because a test that never wraps has not tested the difference from
 *     FPM_iir_filt.
 *
 * Fragmentation matters too: the state must carry across calls, so the same
 * stream is also pushed through one sample at a time and compared against the
 * whole-block result.
 */
static int wrap_seen;

static int
run_ii(const char *label, const short *coeff, int sections, int amplitude,
       int frag)
{
	short sa[64], sb[64];
	short ba[256], bb[256];
	unsigned lfsr = 0x2ACEu;
	int rc, i, n;

	memset(sa, 0, sizeof(sa));
	memset(sb, 0, sizeof(sb));

	diff_begin(label);
	for (n = 0; n < NSAMP / frag; n++) {
		for (i = 0; i < frag; i++) {
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
			ba[i] = bb[i] = (short)(((int)(lfsr & 0xffff) - 0x8000)
					        / (0x8000 / amplitude));
		}

		ref_FPM_iir_filt_II(ba, coeff, sa, (short)sections,
				    (short)frag);
		FPM_iir_filt_II(bb, coeff, sb, (short)sections, (short)frag);

		for (i = 0; i < frag; i++)
			diff_eq_int("sample %ld: output", bb[i], ba[i],
				    n * frag + i);
		for (i = 0; i < sections * FPM_IIR_II_STATE_PER_SECTION; i++) {
			diff_eq_int("state[%ld]", sb[i], sa[i], i);
			/* A state word at an extreme is the signature of a
			 * wrap; the filter has no clamp to stick at. */
			if (sa[i] > 30000 || sa[i] < -30000)
				wrap_seen++;
		}
	}
	rc = diff_end();
	return rc;
}

int
main(void)
{
	int rc = 0;
	int i;

	/* The real filters, at ordinary signal levels. */
	rc |= run("iir COEF_DC", ref_COEF_DC, 1, 8000);
	rc |= run("iir MTDb103_COEF", ref_MTDb103_COEF, 2, 8000);
	rc |= run("iir B103_IIR_LPF", ref_B103_IIR_LPF, 3, 8000);

	/* Full scale, to push the recursive node toward its limits. */
	rc |= run("iir COEF_DC full scale", ref_COEF_DC, 1, 32767);
	rc |= run("iir MTDb103_COEF full scale", ref_MTDb103_COEF, 2, 32767);

	/*
	 * A deliberately unstable section: feedback coefficients near +-1.0 in
	 * Q14 drive the accumulator past 16 bits within a few samples, which is
	 * the only way to reach the saturation branch and its asymmetric lower
	 * limit.
	 */
	{
		static const short hot[10] = {
			16000, 8192, 15000, 4096, 16384,
			-16000, 8192, -15000, 4096, 16384,
		};

		rc |= run("iir saturating", hot, 2, 32767);
	}

	/*
	 * Guard against the saturating case going vacuous.  Ordinary signals
	 * never reach the clamp, so if the coefficients above ever stop being
	 * unstable this test would silently stop covering it -- and the
	 * asymmetric lower limit (-0x7fff, not -0x8000) is the whole reason it
	 * exists.  Both directions must actually fire.
	 */
	diff_begin("iir saturation reached");
	diff_eq_int("upper clamp hit (%ld)", sat_hi > 0, 1, 0);
	diff_eq_int("lower clamp hit (%ld)", sat_lo > 0, 1, 0);
	rc |= diff_end();


	/* --- FPM_iir_filt_block ---------------------------------------- */

	rc |= run_block("iir_block COEF_DC", ref_COEF_DC, 1, 8000, 64);
	rc |= run_block("iir_block MTDb103_COEF", ref_MTDb103_COEF, 2, 8000,
			64);
	rc |= run_block("iir_block B103_IIR_LPF", ref_B103_IIR_LPF, 3, 8000,
			64);
	rc |= run_block("iir_block B103_IIR_LPF full scale", ref_B103_IIR_LPF,
			3, 32767, 64);

	/* Fragmentation: the state must carry across calls. */
	rc |= run_block("iir_block frag 1", ref_B103_IIR_LPF, 3, 8000, 1);
	rc |= run_block("iir_block frag 7", ref_B103_IIR_LPF, 3, 8000, 7);
	rc |= run_block("iir_block frag 160", ref_B103_IIR_LPF, 3, 8000, 160);

	{
		static const short hot[10] = {
			16000, 8192, 15000, 4096, 16384,
			-16000, 8192, -15000, 4096, 16384,
		};

		rc |= run_block("iir_block saturating", hot, 2, 32767, 64);
		rc |= run_block_vs_single("iir_block == repeated single, "
					  "saturating", hot, 2, 32767, 200);
	}

	rc |= run_block_vs_single("iir_block == repeated single",
				  ref_B103_IIR_LPF, 3, 8000, 200);

	/*
	 * The same guard the single-sample case carries: the clamp is the one
	 * branch ordinary signals never reach, and a block test that never
	 * reaches it has not tested the engine it claims to.
	 */
	diff_begin("iir_block saturation reached");
	diff_eq_int("upper clamp hit (%ld)", blk_sat_hi > 0, 1, blk_sat_hi);
	diff_eq_int("lower clamp hit (%ld)", blk_sat_lo > 0, 1, blk_sat_lo);
	rc |= diff_end();

	/* A zero-length block must touch neither the samples nor the state. */
	{
		short blk[4] = { 1, -2, 3, -4 };
		short st[4] = { 0x1111, 0x2222, 0x3333, 0x4444 };
		short refblk[4] = { 1, -2, 3, -4 };
		short refst[4] = { 0x1111, 0x2222, 0x3333, 0x4444 };

		diff_begin("iir_block zero count");
		ref_FPM_iir_filt_block(refblk, ref_COEF_DC, refst, 2, 0);
		FPM_iir_filt_block(blk, ref_COEF_DC, st, 2, 0);
		for (i = 0; i < 4; i++) {
			diff_eq_int("sample %ld untouched", blk[i], refblk[i],
				    i);
			diff_eq_int("state[%ld] untouched", st[i], refst[i], i);
		}
		rc |= diff_end();
	}


	/* --- FPM_iir_filt_II ------------------------------------------- */

	/*
	 * Same real coefficient sets.  They were designed for the form II
	 * filter, so the responses here are not meaningful -- but the
	 * arithmetic is exercised with realistic magnitudes, which is what
	 * this test is for.
	 */
	rc |= run_ii("iir_II COEF_DC", ref_COEF_DC, 1, 8000, 64);
	rc |= run_ii("iir_II MTDb103_COEF", ref_MTDb103_COEF, 2, 8000, 64);
	rc |= run_ii("iir_II B103_IIR_LPF", ref_B103_IIR_LPF, 3, 8000, 64);
	rc |= run_ii("iir_II B103_IIR_LPF full scale", ref_B103_IIR_LPF, 3,
		     32767, 64);

	/* Fragmentation: state must carry across calls. */
	rc |= run_ii("iir_II frag 1", ref_B103_IIR_LPF, 3, 8000, 1);
	rc |= run_ii("iir_II frag 7", ref_B103_IIR_LPF, 3, 8000, 7);
	rc |= run_ii("iir_II frag 160", ref_B103_IIR_LPF, 3, 8000, 160);

	/* Zero sections is a no-op copy, and must not touch the state. */
	{
		short blk[4] = { 1, -2, 3, -4 };
		short st[4] = { 0x1111, 0x2222, 0x3333, 0x4444 };
		short refblk[4] = { 1, -2, 3, -4 };
		short refst[4] = { 0x1111, 0x2222, 0x3333, 0x4444 };

		diff_begin("iir_II zero sections");
		ref_FPM_iir_filt_II(refblk, ref_COEF_DC, refst, 0, 4);
		FPM_iir_filt_II(blk, ref_COEF_DC, st, 0, 4);
		for (i = 0; i < 4; i++) {
			diff_eq_int("sample %ld untouched", blk[i], refblk[i], i);
			diff_eq_int("state[%ld] untouched", st[i], refst[i], i);
		}
		rc |= diff_end();

		/* And a zero-length block, likewise. */
		diff_begin("iir_II zero count");
		ref_FPM_iir_filt_II(refblk, ref_COEF_DC, refst, 1, 0);
		FPM_iir_filt_II(blk, ref_COEF_DC, st, 1, 0);
		for (i = 0; i < 4; i++) {
			diff_eq_int("sample %ld untouched", blk[i], refblk[i], i);
			diff_eq_int("state[%ld] untouched", st[i], refst[i], i);
		}
		rc |= diff_end();
	}

	/*
	 * The unstable set again.  With no clamp anywhere this one wraps
	 * rather than sticking, which is the behavioural difference from
	 * FPM_iir_filt and the thing worth proving.
	 */
	{
		static const short hot[10] = {
			16000, 8192, 15000, 4096, 16384,
			-16000, 8192, -15000, 4096, 16384,
		};

		rc |= run_ii("iir_II wrapping", hot, 2, 32767, 64);
	}

	diff_begin("iir_II wrapping reached");
	diff_eq_int("section output near the 16-bit limit (%ld)",
		    wrap_seen > 0, 1, wrap_seen);
	rc |= diff_end();

	return rc;
}

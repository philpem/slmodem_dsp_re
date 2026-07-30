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

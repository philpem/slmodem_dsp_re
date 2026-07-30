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

int
main(void)
{
	int rc = 0;

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

	return rc;
}

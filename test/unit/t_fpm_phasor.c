/*
 * t_fpm_phasor.c -- differential test of the sine/cosine phase accumulator.
 *
 * FPM_phasor is a pure function of (phase, inc), and phase has only 32768
 * legal values, so the sweep is exhaustive rather than sampled: every phase
 * against every increment of interest, checking sine, cosine and the advanced
 * phase.
 *
 * Also checks the recovered table derivation against all 257 extracted
 * entries, so a future regeneration at another scale cannot silently drift.
 */

#include <math.h>

#include "harness.h"
#include "dsplib/fpm_phasor.h"

extern void ref_FPM_phasor(struct fpm_phasor *p);
extern void ref_FPM_phasor_demod(struct fpm_phasor *p);

/* Increments spanning DC, the tones Bell 103 and V.21 use, and the extremes. */
static const int increments[] = {
	0, 1, 2, 7, 0x100, 0x1000,
	4506,	/* ~1100 Hz at 8 kHz: Bell 103 originate mark  */
	5324,	/* ~1300 Hz: Bell 103 originate space          */
	8601,	/* ~2100 Hz: answer tone                       */
	6758,	/* ~1650 Hz: V.21 channel 2 mark               */
	7578,	/* ~1850 Hz: V.21 channel 2 space              */
	0x4000, 0x7ffe, 0x7fff,
};

int
main(void)
{
	int rc = 0;
	unsigned k;
	int i;

	diff_begin("phasor table generator");
	for (i = 0; i < FPM_PHASOR_TABLE; i++) {
		diff_eq_int("cos[%ld]",
			    FPM_phasor_cos_entry(i),
			    (unsigned short)(32768.0 * cos(i * M_PI / 512.0)), i);
		diff_eq_int("sin[%ld]",
			    FPM_phasor_sin_entry(i),
			    (unsigned short)(32768.0 * sin(i * M_PI / 512.0)), i);
	}
	rc |= diff_end();

	diff_begin("FPM_phasor exhaustive");
	for (k = 0; k < sizeof(increments) / sizeof(increments[0]); k++) {
		for (i = 0; i < 0x8000; i++) {
			struct fpm_phasor a, b;

			a.phase = b.phase = (unsigned short)i;
			a.inc = b.inc = (unsigned short)increments[k];
			a.cos = b.cos = a.sin = b.sin = 0;

			ref_FPM_phasor(&a);
			FPM_phasor(&b);

			diff_eq_int("phase 0x%04lx: cos", b.cos, a.cos, i);
			diff_eq_int("phase 0x%04lx: sin", b.sin, a.sin, i);
			diff_eq_int("phase 0x%04lx: next", b.phase, a.phase, i);
		}
	}
	rc |= diff_end();

	/*
	 * FPM_phasor_demod: the same accumulator, cosine only.  Swept
	 * exhaustively for the same reason, and with `sin` pre-loaded with a
	 * sentinel because the interesting part of its contract is what it
	 * does NOT write -- an implementation that zeroed `sin`, or that
	 * computed it anyway, would pass a test that only looked at `cos`.
	 */
	diff_begin("FPM_phasor_demod exhaustive");
	for (k = 0; k < sizeof(increments) / sizeof(increments[0]); k++) {
		for (i = 0; i < 0x8000; i++) {
			struct fpm_phasor a, b;

			a.phase = b.phase = (unsigned short)i;
			a.inc = b.inc = (unsigned short)increments[k];
			a.cos = b.cos = 0;
			a.sin = b.sin = (short)0x5a5a;

			ref_FPM_phasor_demod(&a);
			FPM_phasor_demod(&b);

			diff_eq_int("phase 0x%04lx: cos", b.cos, a.cos, i);
			diff_eq_int("phase 0x%04lx: next", b.phase, a.phase, i);
			diff_eq_int("phase 0x%04lx: sin untouched",
				    b.sin, (short)0x5a5a, i);
			diff_eq_int("phase 0x%04lx: ref sin untouched too",
				    a.sin, (short)0x5a5a, i);
		}
	}
	rc |= diff_end();

	/*
	 * And that demod's cosine really is FPM_phasor's cosine, so the two
	 * cannot drift apart under a future edit to the shared lookup.
	 */
	diff_begin("FPM_phasor_demod matches FPM_phasor");
	for (i = 0; i < 0x8000; i += 7) {
		struct fpm_phasor full, cosonly;

		full.phase = cosonly.phase = (unsigned short)i;
		full.inc = cosonly.inc = 4506;
		full.cos = cosonly.cos = full.sin = cosonly.sin = 0;

		FPM_phasor(&full);
		FPM_phasor_demod(&cosonly);
		diff_eq_int("phase 0x%04lx: cos agrees", cosonly.cos, full.cos, i);
		diff_eq_int("phase 0x%04lx: phase agrees",
			    cosonly.phase, full.phase, i);
	}
	rc |= diff_end();

	/*
	 * Run a tone to completion and confirm the phase returns home: with a
	 * cycle of 0x8000 and an increment dividing it, the accumulator must
	 * be exact, not merely close.
	 */
	diff_begin("FPM_phasor cycle closure");
	{
		struct fpm_phasor a, b;
		int n;

		a.phase = b.phase = 0;
		a.inc = b.inc = 0x40;		/* 0x8000 / 0x40 = 512 steps */
		for (n = 0; n < 512; n++) {
			ref_FPM_phasor(&a);
			FPM_phasor(&b);
			diff_eq_int("step %ld: sin", b.sin, a.sin, n);
			diff_eq_int("step %ld: cos", b.cos, a.cos, n);
		}
		diff_eq_int("phase wrapped to 0 (%ld)", b.phase, 0, 0);
		diff_eq_int("reference agrees (%ld)", a.phase, 0, 0);
	}
	rc |= diff_end();

	return rc;
}

/* Differential phasor test: all 65536 phases, selected signed increments,
 * original linked sign-table neighbourhood and public writable storage.
 * Negative quadrants deliberately retain the original out-of-symbol reads.
 * This fixture checks their actual period link placement before the sweeps. */

#include <math.h>
#include <stdio.h>

#include "harness.h"
#include "dsplib/fpm_phasor.h"
#include "dsplib/fpm_iir.h"

extern short ref_COEF_DC[FPM_IIR_COEFF_PER_SECTION];
extern void ref_FPM_phasor_dp(struct fpm_phasor_dp *p);

extern void ref_FPM_phasor(struct fpm_phasor *p);
extern void ref_FPM_phasor_demod(struct fpm_phasor *p);

extern short ref_FPM_cos_sign[4];
extern short ref_FPM_sin_sign[4];

/* Measurement apparatus: volatile base prevents folding away the linked
 * negative-index neighbourhood check on either side. */
static short
below(const short *table, int i)
{
	const short *volatile base = table;

	return base[i - 4];
}

/*
 * Increments spanning DC, the tones Bell 103 and V.21 use, and the extremes.
 *
 * THE LAST THREE ARE ABOVE 0x7fff AND THEY ARE THE POINT OF THIS PARAGRAPH.
 * `inc` is an `unsigned short` field, and both `FPM_phasor` and
 * `FPM_phasor_demod` load it with `movswl` (0x0a9389, 0x0a9569) -- so a value
 * of 0x8000 or more is a NEGATIVE increment and the accumulator runs backwards.
 * The list used to stop at 0x7fff, which left "the increment is read unsigned"
 * untestable in both functions: below 0x8000 the two readings agree on every
 * value, which is 613's shape exactly.  Found while writing `t_fpm_phasordp`,
 * where the same hole let the same mutation survive; finding F8323.
 */
static const int increments[] = {
	0, 1, 2, 7, 0x100, 0x1000,
	4506,	/* ~1100 Hz at 8 kHz: Bell 103 originate mark  */
	5324,	/* ~1300 Hz: Bell 103 originate space          */
	8601,	/* ~2100 Hz: answer tone                       */
	6758,	/* ~1650 Hz: V.21 channel 2 mark               */
	7578,	/* ~1850 Hz: V.21 channel 2 space              */
	0x4000, 0x7ffe, 0x7fff,
	0x8000, 0xc000, 0xffff,		/* read signed: -32768, -16384, -1 */
};


/* Exported writable tables are a component boundary, not a modem-write claim.
 * Each assignment stays inside its declared array. Negative phase checks also
 * exercise the original cosine->sine and sine->COEF_DC neighbourhood aliases. */
static int
writable_signs(void)
{
	short cos_saved[4], sin_saved[4], dc_saved[5];
	int i, phase, mode;
	int rc;

	for (i = 0; i < 4; i++) {
		cos_saved[i] = FPM_cos_sign[i];
		sin_saved[i] = FPM_sin_sign[i];
		FPM_cos_sign[i] = ref_FPM_cos_sign[i] = (short)(1000 + 2000*i);
		FPM_sin_sign[i] = ref_FPM_sin_sign[i] = (short)(-2000 - 3000*i);
	}
	for (i = 0; i < 5; i++) {
		dc_saved[i] = COEF_DC[i];
		COEF_DC[i] = ref_COEF_DC[i] = (short)(3000 + 1000*i);
	}
	diff_begin("phasor writable sign storage: component table writes");
	for (phase = 0; phase < 65536; phase += 4096) {
		for (mode = 0; mode < 2; mode++) {
			struct fpm_phasor a = {0}, b = {0};
			a.phase = b.phase = (unsigned short)phase;
			a.inc = b.inc = 17;
			a.sin = b.sin = 123;
			if (mode) { ref_FPM_phasor_demod(&a); FPM_phasor_demod(&b); }
			else { ref_FPM_phasor(&a); FPM_phasor(&b); }
			diff_eq_obj("writable sign result", struct fpm_phasor, &b, &a, phase + mode);
		}
		{
			struct fpm_phasor_dp a = {0}, b = {0};
			a.phase = b.phase = (unsigned short)phase;
			a.inc = b.inc = 17;
			ref_FPM_phasor_dp(&a); FPM_phasor_dp(&b);
			diff_eq_obj("writable sign dp result", struct fpm_phasor_dp, &b, &a, phase);
		}
	}
	rc = diff_end();
	for (i = 0; i < 4; i++) {
		FPM_cos_sign[i] = ref_FPM_cos_sign[i] = cos_saved[i];
		FPM_sin_sign[i] = ref_FPM_sin_sign[i] = sin_saved[i];
	}
	for (i = 0; i < 5; i++) COEF_DC[i] = ref_COEF_DC[i] = dc_saved[i];
	return rc;
}

int
main(void)
{
	int rc = 0;
	unsigned k;
	int i;
	int q;
	int sep_sin_below[FPM_PHASOR_SIGN_BELOW];
	int sep_cos_below[FPM_PHASOR_SIGN_BELOW];

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

	/* Compare the actual period-linked owners and negative neighbourhood. */
	diff_begin("sign table neighbourhood");
	for (i = 0; i < 4; i++) {
		/* the four the object reads BELOW each table */
		diff_eq_int("cos_sign[-4 + %ld]",
			    below(FPM_cos_sign, i), below(ref_FPM_cos_sign, i), i);
		diff_eq_int("sin_sign[-4 + %ld]",
			    below(FPM_sin_sign, i), below(ref_FPM_sin_sign, i), i);
		/* The public table values. */
		diff_eq_int("cos_sign[%ld]",
			    FPM_cos_sign[i], ref_FPM_cos_sign[i], i);
		diff_eq_int("sin_sign[%ld]",
			    FPM_sin_sign[i], ref_FPM_sin_sign[i], i);
	}
	rc |= diff_end();

	for (i = 0; i < 4; i++)
		sep_sin_below[i] = sep_cos_below[i] = 0;

	diff_begin("FPM_phasor exhaustive");
	for (k = 0; k < sizeof(increments) / sizeof(increments[0]); k++) {
		for (i = 0; i < 0x10000; i++) {
			struct fpm_phasor a, b;

			a.phase = b.phase = (unsigned short)i;
			a.inc = b.inc = (unsigned short)increments[k];
			a.cos = b.cos = a.sin = b.sin = 0;

			ref_FPM_phasor(&a);
			FPM_phasor(&b);

			diff_eq_int("phase 0x%04lx: cos", b.cos, a.cos, i);
			diff_eq_int("phase 0x%04lx: sin", b.sin, a.sin, i);
			diff_eq_int("phase 0x%04lx: next", b.phase, a.phase, i);

			/*
			 * Counted on the first increment only, so the figure
			 * is per PHASE and not per trial: neither output
			 * depends on `inc`.
			 */
			if (k == 0 && i >= 0x8000) {
				int q = ((((short)i) >> 5) >> 8)
					+ FPM_PHASOR_SIGN_BELOW;

				if (a.sin != 0)
					sep_sin_below[q]++;
				if (a.cos != 0)
					sep_cos_below[q]++;
			}
		}
	}
	rc |= diff_end();

	/*
	 * WHICH OF THOSE 32768 OUT-OF-DOMAIN PHASES ACTUALLY SEPARATE, AND THE
	 * COINCIDENCE THIS EXISTS TO EXPOSE.
	 *
	 * An out-of-domain trial multiplies the interpolated magnitude by ONE
	 * word of the window, so it can only tell that word apart from zero
	 * when the reference's own output is non-zero.  Counting those is
	 * therefore a count of trials whose OBSERVED VALUE separates the two
	 * readings, not a count of trials that took a path (finding F3509).
	 *
	 * The sine's fourth window word IS zero -- it is the two bytes of
	 * padding at .data 0x081da, not a coefficient -- so quadrant -1
	 * (phases 0xE000 .. 0xFFFF) returns zero on both sides for every
	 * phase and separates NOTHING.  It agreed before any of this work,
	 * when our `static const` layout put `fpm_cos_table[256]` there and
	 * that is zero too, and finding F3623 records that as two layouts
	 * coinciding rather than as coverage.  It is asserted at 0 here so
	 * that nobody reads the sweep as covering it: the check that this
	 * word is the object's is the neighbourhood block above, which
	 * compares it against the blob's byte directly, and the `fpmphasor`
	 * mutation that makes it non-zero is what proves the sweep would
	 * notice if it were wrong in the other direction.
	 *
	 * The cosine's window has no zero in it, so all four of its quadrants
	 * separate.  The mutation set adjudicates all of this; these numbers
	 * are the denominator behind it.
	 */
	printf("  out-of-domain separating trials, of 8192 phases per quadrant:\n");
	for (q = 0; q < 4; q++)
		printf("    quadrant %2d   sin %5d   cos %5d\n",
		       q - FPM_PHASOR_SIGN_BELOW,
		       sep_sin_below[q], sep_cos_below[q]);

	diff_begin("out-of-domain separation");
	for (q = 0; q < 3; q++)
		diff_eq_int("sine quadrant %ld separates the window from zero",
			    sep_sin_below[q] > 0, 1, q - FPM_PHASOR_SIGN_BELOW);
	diff_eq_int("sine quadrant -1 separates nothing (%ld): its word is the pad and is 0",
		    sep_sin_below[3], 0, sep_sin_below[3]);
	for (q = 0; q < 4; q++)
		diff_eq_int("cosine quadrant %ld separates the window from zero",
			    sep_cos_below[q] > 0, 1, q - FPM_PHASOR_SIGN_BELOW);
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
		for (i = 0; i < 0x10000; i++) {
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
	for (i = 0; i < 0x10000; i += 7) {
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

	rc |= writable_signs();
	return rc;
}

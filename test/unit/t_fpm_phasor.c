/*
 * t_fpm_phasor.c -- differential test of the sine/cosine phase accumulator.
 *
 * FPM_phasor is a pure function of (phase, inc), and phase is a 16-bit field
 * with only 65536 possible values, so the sweep is exhaustive rather than
 * sampled: every phase against every increment of interest, checking sine,
 * cosine and the advanced phase.
 *
 * THE SWEEP RUNS TO 0x10000, NOT 0x8000, and that is the whole point of it.
 * A cycle is 0x8000 units, so 0x8000 and above are out of the DESIGNED domain
 * -- but the object reads `phase` as a SIGNED short and indexes its quadrant
 * sign tables with the result UNMASKED, so those values reach four entries
 * BEFORE each table and return something perfectly definite.  What they read
 * is a property of the LINK, not of the code, and reproducing it is what
 * `FPM_sin_sign` and `FPM_cos_sign` being adjacent globals in `.data` is for.
 *
 * THE COSINE IS COMPARED OVER THE WHOLE RANGE AND THE SINE IS NOT, and the
 * asymmetry is derived rather than convenient: the cosine's out-of-domain
 * sign is `FPM_sin_sign`, the next `.data` object of the SAME translation
 * unit, and the sine's is the tail of `COEF_DC` in the PREVIOUS one.  A
 * compiler that appends anything to that unit's `.data` -- `--coverage` does,
 * and `tools/debugcov.py` builds exactly that -- displaces the second and not
 * the first.  The layout block below asserts the half that is assertable and
 * says why the other half is not.  D392, findings 3588, 3623 and 3624.
 *
 * Also checks the recovered table derivation against all 257 extracted
 * entries, so a future regeneration at another scale cannot silently drift.
 */

#include <math.h>

#include "harness.h"
#include "dsplib/fpm_phasor.h"

extern void ref_FPM_phasor(struct fpm_phasor *p);
extern void ref_FPM_phasor_demod(struct fpm_phasor *p);

/*
 * The object's own sign tables, so the layout block can compare what lies
 * before ours against what lies before the object's.  `dsplibs_ref.o` is one
 * link contribution, so the relative placement of ref_COEF_DC, ref_FPM_sin_sign
 * and ref_FPM_cos_sign is the object's, byte for byte.
 */
extern short ref_FPM_cos_sign[4];
extern short ref_FPM_sin_sign[4];

/*
 * Read four entries BEFORE a table.  Through a volatile pointer, so the
 * compiler cannot fold the out-of-bounds index against the array's declared
 * size -- this is the whole point of the check and not an accident to warn
 * about.  Test plumbing only; `src/dsp/fpm_phasor.c` indexes through an
 * ordinary pointer parameter and needs no such thing.
 */
static short
below(const short *table, int i)
{
	const short *volatile base = table;

	return base[i - 4];
}

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

	/*
	 * THE LAYOUT, AND IT IS AN OBSERVABLE RATHER THAN AN INVARIANT.
	 *
	 * `FPM_phasor` does not mask the quadrant, so a phase of 0x8000 or
	 * more indexes the sign tables at -4 .. -1.  Those sixteen bytes are
	 * decided by the LINK, not by any statement in `fpm_phasor.c`, and the
	 * two halves of that are NOT equally assertable:
	 *
	 *   - the four before `FPM_cos_sign` are `FPM_sin_sign` itself, which
	 *     is the SAME translation unit.  Nothing a compiler does to
	 *     fpm_phasor.c can come between two adjacent `.data` globals of
	 *     fpm_phasor.c, so this is checked here and the cosine is compared
	 *     over the whole 16-bit range below;
	 *   - the four before `FPM_sin_sign` are the tail of `COEF_DC`, which
	 *     is fpm_mtd.c's -- ANOTHER translation unit -- plus the two bytes
	 *     of padding the boundary costs.  They agree in the build that
	 *     ships, and they do NOT agree in the instrumented build
	 *     `tools/debugcov.py` makes, because `--coverage` appends
	 *     `__gcov_.FPM_MTD_create/delete/detect` to fpm_mtd.c's `.data`
	 *     exactly there.  So it is not asserted and the SINE is compared
	 *     only inside 0 .. 0x7fff.  D392 is partially closed on that
	 *     boundary and findings 3623 and 3624 are the derivation; the
	 *     `fpmmtdlayout` mutation set is the register of what it costs.
	 */
	diff_begin("sign table neighbourhood");
	for (i = 0; i < 4; i++) {
		diff_eq_int("cos_sign[-4 + %ld]",
			    below(FPM_cos_sign, i), below(ref_FPM_cos_sign, i), i);
		diff_eq_int("cos_sign[%ld]",
			    FPM_cos_sign[i], ref_FPM_cos_sign[i], i);
		diff_eq_int("sin_sign[%ld]",
			    FPM_sin_sign[i], ref_FPM_sin_sign[i], i);
	}
	rc |= diff_end();

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
			/*
			 * THE SINE STOPS AT 0x8000 AND THE COSINE DOES NOT.
			 * Not a tolerance: the cosine's out-of-domain sign is
			 * `FPM_sin_sign`, one translation unit's own next
			 * object, and the sine's is another unit's tail, which
			 * `--coverage` displaces.  See the layout block above.
			 */
			if (i < 0x8000)
				diff_eq_int("phase 0x%04lx: sin",
					    b.sin, a.sin, i);
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

	return rc;
}

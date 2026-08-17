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
 * BEFORE each table and return something perfectly definite.
 *
 * BOTH HALVES ARE COMPARED OVER THE WHOLE RANGE, and they are only able to be
 * because the eight out-of-range words are now VALUES in `src/` rather than
 * an arrangement of memory.  They used to be a property of OUR link -- two
 * `.data` globals in a chosen declaration order, with `COEF_DC` positioned to
 * land before one of them -- which is undefined behaviour we could not
 * guarantee, and which `--coverage` displaced, so the sine could not be
 * compared at all above 0x7fff.  `FPM_cos_sign_ext` and `FPM_sin_sign_ext`
 * carry those words as leading entries and the phasor indexes
 * `ext[FPM_PHASOR_SIGN_BELOW + quad]`.  The neighbourhood block below compares
 * all sixteen of them against `dsplibs_ref.o`'s own `.data`, which is the
 * BLOB's bytes and nothing we compile, so no instrumentation of ours can move
 * it.  D392, findings 3588, 3620-3624 and 3700-3703.
 *
 * Also checks the recovered table derivation against all 257 extracted
 * entries, so a future regeneration at another scale cannot silently drift.
 */

#include <math.h>
#include <stdio.h>

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
 * about.
 *
 * USED ON THE REFERENCE SIDE ONLY.  `ref_FPM_sin_sign` and `ref_FPM_cos_sign`
 * are the blob's symbols and the bytes below them are the blob's `.data`, so
 * reading before them is how the object's own window is recovered.  Our side
 * does not do this any more and no longer may: `src/dsp/fpm_phasor.c` carries
 * the same sixteen bytes as leading entries of `FPM_cos_sign_ext` and
 * `FPM_sin_sign_ext` and every index it forms is inside one array object.
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

	/*
	 * THE OUT-OF-RANGE WINDOW, AS SIXTEEN VALUES RATHER THAN A LAYOUT.
	 *
	 * `FPM_phasor` does not mask the quadrant, so a phase of 0x8000 or
	 * more indexes the sign tables at -4 .. -1.  On the OBJECT's side
	 * those sixteen bytes are decided by its link, and `dsplibs_ref.o` is
	 * one contribution, so `ref_FPM_sin_sign[-4 .. -1]` and
	 * `ref_FPM_cos_sign[-4 .. -1]` are the blob's own bytes -- read here
	 * through `below()`, which is the only side that still needs it.
	 *
	 * On OUR side they are the leading entries of `FPM_cos_sign_ext` and
	 * `FPM_sin_sign_ext`: ordinary array elements, read with a defined
	 * index.  So this block is a plain differential comparison of sixteen
	 * words, it does not depend on where anything is linked, and NOTHING
	 * `--coverage` appends to any translation unit's `.data` can perturb
	 * it.  That is what lets both sweeps below run the whole 16-bit range.
	 *
	 * It also ties the two copies of the four real signs together: the
	 * exported `FPM_cos_sign`/`FPM_sin_sign` carry the object's symbols
	 * and the extended arrays' upper halves carry the same four words, and
	 * both are compared against the same reference, so neither can drift.
	 */
	diff_begin("sign table neighbourhood");
	for (i = 0; i < 4; i++) {
		/* the four the object reads BELOW each table */
		diff_eq_int("cos_sign_ext[%ld] = cos_sign[-4 + i]",
			    FPM_cos_sign_ext[i], below(ref_FPM_cos_sign, i), i);
		diff_eq_int("sin_sign_ext[%ld] = sin_sign[-4 + i]",
			    FPM_sin_sign_ext[i], below(ref_FPM_sin_sign, i), i);
		/* the four real signs, in the extended arrays ... */
		diff_eq_int("cos_sign_ext[4 + %ld]",
			    FPM_cos_sign_ext[FPM_PHASOR_SIGN_BELOW + i],
			    ref_FPM_cos_sign[i], i);
		diff_eq_int("sin_sign_ext[4 + %ld]",
			    FPM_sin_sign_ext[FPM_PHASOR_SIGN_BELOW + i],
			    ref_FPM_sin_sign[i], i);
		/* ... and at the object's own two symbols */
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

			/*
			 * BOTH OUTPUTS, OVER THE WHOLE RANGE.  The sine used
			 * to stop at 0x8000 because its out-of-domain sign was
			 * a neighbouring translation unit's tail that our own
			 * instrumented build displaced; it is a value in
			 * `FPM_sin_sign_ext` now and the asymmetry is gone.
			 */
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
	 * readings, not a count of trials that took a path (finding 3509).
	 *
	 * The sine's fourth window word IS zero -- it is the two bytes of
	 * padding at .data 0x081da, not a coefficient -- so quadrant -1
	 * (phases 0xE000 .. 0xFFFF) returns zero on both sides for every
	 * phase and separates NOTHING.  It agreed before any of this work,
	 * when our `static const` layout put `fpm_cos_table[256]` there and
	 * that is zero too, and finding 3623 records that as two layouts
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

	return rc;
}

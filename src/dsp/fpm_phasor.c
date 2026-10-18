/*
 * fpm_phasor.c -- Fixed Point Modem: sine/cosine phase accumulator.
 *
 * Reconstructed from dsplibs.o fpm_phasor.c, .text 0x0a9300, with tables at
 * .rodata 0x0cde0 (cos) and 0x0cbc0 (sin) and signs at .data 0x081e4.
 *
 * Method: the 15-bit phase splits into a quadrant, a 256-entry table index and
 * a 5-bit fraction.  Odd quadrants read the table backwards, so one quarter
 * wave covers the whole cycle; the sign per quadrant comes from a 4-entry
 * table rather than a branch.
 *
 *     idx  = phase >> 5                  0 .. 1023
 *     frac = phase - (idx << 5)          0 .. 31
 *     quad = idx >> 8                    0 .. 3
 *     if (quad & 1) { frac = 32 - frac; idx = ~idx; }
 *     idx &= 0xff
 *
 * The tables hold 257 entries, not 256, precisely so the linear interpolation
 * can read idx+1 without a bounds check.  (Worth contrasting with FPM_sqrt,
 * whose table is one entry *short* of what its index expression produces --
 * deviation D1.  Same author, same era, opposite outcome.)
 *
 * Table derivation, recovered and checked against every entry:
 *
 *     cos[i] = trunc(32768 * cos(i * pi / 512))
 *     sin[i] = trunc(32768 * sin(i * pi / 512))       i in [0, 257)
 *
 * Truncated, not rounded -- rounding differs on 114 of the 257 entries.  Note
 * also sin[i] == cos[256-i] exactly, so the two tables are one quarter wave
 * stored twice; both are kept because the original has both and the code
 * indexes them independently.
 *
 * Entry 0 is 32768, which does not fit a signed short -- hence unsigned tables
 * with the sign applied afterwards from the quadrant.
 */

#include <math.h>

#include "dsplib/fpm_phasor.h"



/* Original writable .data owners, in source order. Period GCC emits sine
 * before cosine. fpm_mtd.c's preceding data ends in COEF_DC plus two alignment
 * bytes, so negative signed quadrants retain the original neighbour reads.
 * This intentionally preserves original out-of-symbol indexing; link layout
 * is part of that behavior and is checked by the period fixture. */
short FPM_cos_sign[4] = { 16384, -16384, -16384, 16384 };
short FPM_sin_sign[4] = { 16384,  16384, -16384, -16384 };



/* One table lookup with linear interpolation between adjacent entries. */
static int
interpolate(const unsigned short *table, int idx, int frac)
{
	int base = table[idx];

	return base + ((((int)table[idx + 1] - base) * frac) >> 5);
}

/*
 * Split the phase into a table index, an interpolation fraction and a
 * quadrant.  Odd quadrants are the mirror of even ones, so the table is
 * walked backwards and the fraction reflected with it.
 */
static void
phasor_split(int phase, int *idx_out, int *frac_out, int *quad_out)
{
	int idx = phase >> 5;
	/* The reference narrows both this difference and its reflection below. */
	short frac = (short)(phase - (idx << 5));
	int quad = idx >> 8;

	if (quad & 1) {
		frac = 32 - frac;
		idx = ~idx;
	}

	*idx_out = idx & 0xff;
	*frac_out = frac;
	*quad_out = quad;
}

/* The original uses an unmasked signed quadrant (-4..3). Preserve its
 * direct public-table load, including negative reads into adjacent storage.
 * Interpolation narrows to unsigned short before applying the sign. */
static short
phasor_value(const unsigned short *table, const short *sign,
	     int idx, int frac, int quad)
{
	return (short)(((unsigned short)interpolate(table, idx, frac)
			* sign[quad]) >> 15);
}

/* Advance by one increment, wrapping at one cycle. */
static unsigned short
phasor_advance(int phase, int inc)
{
	phase += inc;
	if (phase > 0x7fff)
		phase -= FPM_PHASOR_CYCLE;
	return (unsigned short)phase;
}

void
FPM_phasor(struct fpm_phasor *p)
{
	int phase = (short)p->phase;
	int idx, frac, quad;

	phasor_split(phase, &idx, &frac, &quad);
	p->cos = phasor_value(FPM_cos_table, FPM_cos_sign, idx, frac, quad);
	p->sin = phasor_value(FPM_sin_table, FPM_sin_sign, idx, frac, quad);
	p->phase = phasor_advance(phase, (short)p->inc);
}

/*
 * FPM_phasor_dp -- .text 0x0a93e0, 246 bytes.
 *
 * `FPM_phasor` with a FRACTIONAL phase carried between calls, so the tone's
 * frequency is not restricted to a whole phase unit per sample.  Placed here,
 * between `FPM_phasor` and `FPM_phasor_demod`, because that is the object's own
 * emission order (0x0a9300, 0x0a93e0, 0x0a94e0) and emission order is a codegen
 * carrier for everything after it (finding F7796).
 *
 * THE LOOKUP IS `FPM_phasor`'S, INSTRUCTION FOR INSTRUCTION.  The object's two
 * functions are byte-identical from their prologues to 0x0a944f -- same `>> 5`
 * index, same `>> 8` quadrant, same odd-quadrant reflection, same `& 0xff`,
 * same interpolation, same `(unsigned short)` narrowing before the quadrant
 * sign, same `>> 15` -- so `phasor_split`, `interpolate` and `phasor_value` are
 * reused verbatim rather than copied.  It is the third user of the UNMASKED
 * quadrant, with relocations at 0x0a944b against `FPM_cos_sign` and 0x0a9472
 * against `FPM_sin_sign`, so it uses the same direct public-table loads, including the negative
 * quadrant neighbourhood.
 *
 * ONLY THE ADVANCE DIFFERS.  A 30-bit accumulator is assembled from the two
 * halves, advanced, wrapped at one cycle and split back:
 *
 *     acc = ((phase + inc) << 15) + ((frac_phase + frac_inc) >> 1);
 *     if (acc > 0x3fffffff) acc -= 0x40000000;
 *     phase      = (short)(acc >> 15);
 *     frac_phase = (short)(acc - ((short)(acc >> 15) << 15));
 *
 * The `sar $1` at 0x0a949e is the `>> 1`, and it is REAL: it moves the output,
 * and no Q-format we can point at explains it.  See D950, and do not read a
 * scale into the two fractional fields on the strength of it.
 *
 * THE `(short)` ON `whole` IS THE OBJECT'S `cwtl` AT 0x0a94b9 AND IS
 * BEHAVIOURALLY DEAD.  It is written this way because the object encodes it,
 * not because anything can observe it: `p->phase` truncates to sixteen bits
 * anyway, and every candidate spelling of `whole` differs from this one by a
 * multiple of 65536, whose contribution to `whole << 15` is a multiple of 2^31
 * and therefore zero in the low sixteen bits that `p->frac_phase` keeps.  So
 * the claim it carries belongs to the codegen tier and no differential test
 * can ever adjudicate it.  This paragraph replaces a first draft that said the
 * opposite; `test/mutations/fpmphasordp.json` carries the three mutations that
 * proved it, marked equivalent with the argument (finding F8163's shape, and
 * finding F8322).
 *
 * Finding F8168 carries the decode; this is its implementation.
 */
void
FPM_phasor_dp(struct fpm_phasor_dp *p)
{
	int phase = (short)p->phase;
	int idx, frac, quad;
	int acc, whole;

	phasor_split(phase, &idx, &frac, &quad);
	p->cos = phasor_value(FPM_cos_table, FPM_cos_sign, idx, frac, quad);
	p->sin = phasor_value(FPM_sin_table, FPM_sin_sign, idx, frac, quad);

	acc = ((phase + (short)p->inc) << 15)
	    + ((p->frac_phase + p->frac_inc) >> 1);
	if (acc > 0x3fffffff)
		acc -= FPM_PHASOR_DP_CYCLE;

	whole = (short)(acc >> 15);
	p->phase = (unsigned short)whole;
	p->frac_phase = (short)(acc - (whole << 15));
}

/*
 * FPM_phasor_demod -- .text 0x0a94e0, 161 bytes.
 *
 * Identical to FPM_phasor except that it does not compute the sine: `p->sin`
 * is left exactly as the caller found it, not zeroed.  The original is a
 * separate function rather than a flag, and the two were clearly written by
 * copying; the shared parts are factored here because the behaviour is
 * identical, not because the original shared them.
 *
 * Its one caller is FPM_TONE_generate_demod, which correlates against a
 * reference tone and only needs one phase of it.
 */
void
FPM_phasor_demod(struct fpm_phasor *p)
{
	int phase = (short)p->phase;
	int idx, frac, quad;

	phasor_split(phase, &idx, &frac, &quad);
	p->cos = phasor_value(FPM_cos_table, FPM_cos_sign, idx, frac, quad);
	p->phase = phasor_advance(phase, (short)p->inc);
}

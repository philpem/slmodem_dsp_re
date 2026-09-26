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



/*
 * Quadrant signs, in Q15.  These are +-0.5, not +-1.0, so a full-scale table
 * entry yields 16384 rather than 32768 -- the generator's own scale factor
 * makes up the difference (FPM_TONE_generate multiplies by state->scale >> 14,
 * and a scale of 32767 restores full amplitude).
 *
 * GLOBAL and in `.data`, because the object's are: `FPM_sin_sign` at .data
 * 0x081dc and `FPM_cos_sign` at 0x081e4, both `D` and not `R`.  These two
 * carry the object's symbols and the object's four words each.  NOTHING in
 * `src/` indexes them -- see the extended pair below for why -- and they are
 * kept because the reconstruction's symbol table is part of the deliverable
 * and because `t_fpm_phasor` compares them against `dsplibs_ref.o`'s own,
 * which is what stops the two copies of the four signs drifting apart.
 */
short FPM_cos_sign[4] = { 16384, -16384, -16384, 16384 };
short FPM_sin_sign[4] = { 16384,  16384, -16384, -16384 };

/*
 * THE ARRAYS THE PHASOR INDEXES, AND WHY THEY ARE NOT THE TWO ABOVE.
 *
 * `FPM_phasor` does not mask the quadrant (see below), so it runs -4 .. 3 and
 * the object reads four entries BEFORE each of its two symbols.  We reproduce
 * that behaviour, and we used to reproduce it BY ARRANGING MEMORY -- the two
 * tables were declared in a particular order so that both compilers' reverse
 * `.data` emission put them adjacent, and `COEF_DC` was moved into fpm_mtd.c
 * so its tail landed before `FPM_sin_sign`.  That made the reconstruction's
 * correctness a property of the linker and of two compilers' emission order,
 * and reading before an array is undefined behaviour whatever the link does.
 * It is gone.
 *
 * WHAT REPLACES IT IS D4'S FIX, BACKWARDS.  `FPM_div` indexes a 128-entry
 * table with 0 .. 128, and `src/dsp/fpm_div.c` reproduces the overrun by
 * giving OUR table a 129th entry holding the neighbour's value: the adjacency
 * became a VALUE and the layout dependence vanished.  The same thing here, at
 * the other end.  The object's eight out-of-range words are CONSTANTS --
 *
 *     FPM_cos_sign[-4 .. -1] = { 16384, 16384, -16384, -16384 }
 *                              `FPM_sin_sign` itself, 8 bytes below it
 *     FPM_sin_sign[-4 .. -1] = { 28620, -25834, 12917, 0 }
 *                              COEF_DC[2 .. 4] (fpm_mtd.c, .data 0x081d4)
 *                              and the two bytes of padding the .data
 *                              translation-unit boundary costs at 0x081da
 *
 * -- measured from the blob, so they are carried here as the LEADING entries
 * of one array each and the phasor indexes `ext[FPM_PHASOR_SIGN_BELOW + quad]`.
 * Every index the function can form is then 0 .. 7 inside a single array
 * object: defined C, no layout assumption, and the same value for every one of
 * the 65536 phases.
 *
 * WHAT IT UNLOCKS is the whole of D392.  Both halves of the out-of-domain
 * window are now values in `src/`, so `t_fpm_phasor` compares them against
 * `dsplibs_ref.o`'s own `.data` -- the blob's bytes, which our `--coverage`
 * build cannot displace because we do not compile the blob -- and sweeps BOTH
 * sine and cosine over all 65536 phases.  Finding F3624's objection was that
 * `--coverage` appends `__gcov_.FPM_MTD_*` to fpm_mtd.c's `.data` at exactly
 * those offsets; with no adjacency left to assert, it no longer applies.
 *
 * FOR WHOEVER WRITES `FPM_phasor_dp` (0x0a93e0, not reconstructed): it is the
 * third user of these two tables and reads them the same unmasked way --
 * `readelf -rW` puts its relocations at 0x0a944b against `FPM_cos_sign` and
 * 0x0a9472 against `FPM_sin_sign`.  Index `FPM_cos_sign_ext` and
 * `FPM_sin_sign_ext` with the same `FPM_PHASOR_SIGN_BELOW` bias.  The
 * four-entry `FPM_cos_sign`/`FPM_sin_sign` above are the symbol table and are
 * NOT what the phasor reads; indexing those with a negative quadrant is the
 * undefined behaviour this arrangement exists to remove.
 *
 * `COEF_DC` STAYS IN fpm_mtd.c.  Its attribution rests on evidence that is
 * independent of any of this -- its one reference in 1.2 MB is inside
 * `FPM_MTD_detect`, `nm` marks it `D` and not `R`, and the two-byte pad at
 * 0x081da is itself proof of a translation-unit boundary.  Only the
 * DEPENDENCE on where it lands has been removed, not the attribution.
 * Findings F3588, F3620-3624 and 3700-3703, deviation D392.
 */
short FPM_cos_sign_ext[FPM_PHASOR_SIGN_BELOW + 4] = {
	/* [-4 .. -1]: what the object reads below FPM_cos_sign, which is
	 * FPM_sin_sign -- the same translation unit's next .data object. */
	 16384,  16384, -16384, -16384,
	/* [ 0 ..  3]: FPM_cos_sign itself. */
	 16384, -16384, -16384,  16384,
};

short FPM_sin_sign_ext[FPM_PHASOR_SIGN_BELOW + 4] = {
	/* [-4 .. -1]: what the object reads below FPM_sin_sign -- COEF_DC's
	 * last three words and the boundary pad, .data 0x081d4 .. 0x081db.
	 * The trailing 0 is the PAD and is not a coefficient; see the note in
	 * t_fpm_phasor.c about not mistaking a zero that agrees for a zero
	 * that was checked. */
	 28620, -25834,  12917,      0,
	/* [ 0 ..  3]: FPM_sin_sign itself. */
	 16384,  16384, -16384, -16384,
};

unsigned short
FPM_phasor_cos_entry(int i)
{
	return (i >= 0 && i < FPM_PHASOR_TABLE) ? FPM_cos_table[i] : 0;
}

unsigned short
FPM_phasor_sin_entry(int i)
{
	return (i >= 0 && i < FPM_PHASOR_TABLE) ? FPM_sin_table[i] : 0;
}

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
	int frac = phase - (idx << 5);
	int quad = idx >> 8;

	if (quad & 1) {
		frac = 32 - frac;
		idx = ~idx;
	}

	*idx_out = idx & 0xff;
	*frac_out = frac;
	*quad_out = quad;
}

/*
 * One table lookup with the quadrant sign applied.
 *
 * The interpolated value is truncated to 16 bits before the sign -- entry 0
 * is 32768, so this is not a no-op at the extremes.
 *
 * THE QUADRANT IS NOT MASKED and that is the object's, at 0x0a9367 and
 * 0x0a9392: `movswl 0x0(%esi,%esi,1)` with `%esi` the sign-extended quadrant.
 * `phase` is read as a signed short, so `quad` runs -4 .. 3 and the object
 * reads below its table for every phase of 0x8000 or more.  `sign` is the
 * EXTENDED array and the bias puts every one of those reads inside it; see
 * the definitions above.  The bound is exact rather than defensive:
 * phase in -32768 .. 32767, idx = phase >> 5 in -1024 .. 1023,
 * quad = idx >> 8 in -4 .. 3, so the index is 0 .. 7 and cannot be otherwise.
 */
static short
phasor_value(const unsigned short *table, const short *sign,
	     int idx, int frac, int quad)
{
	return (short)(((unsigned short)interpolate(table, idx, frac)
			* sign[FPM_PHASOR_SIGN_BELOW + quad]) >> 15);
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
	p->cos = phasor_value(FPM_cos_table, FPM_cos_sign_ext, idx, frac, quad);
	p->sin = phasor_value(FPM_sin_table, FPM_sin_sign_ext, idx, frac, quad);
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
 * against `FPM_sin_sign`, so it indexes the extended arrays with the same
 * `FPM_PHASOR_SIGN_BELOW` bias and D392 covers it too.
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
	p->cos = phasor_value(FPM_cos_table, FPM_cos_sign_ext, idx, frac, quad);
	p->sin = phasor_value(FPM_sin_table, FPM_sin_sign_ext, idx, frac, quad);

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
	p->cos = phasor_value(FPM_cos_table, FPM_cos_sign_ext, idx, frac, quad);
	p->phase = phasor_advance(phase, (short)p->inc);
}

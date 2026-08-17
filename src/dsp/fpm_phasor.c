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

static const unsigned short fpm_cos_table[FPM_PHASOR_TABLE] = {
	32768, 32767, 32765, 32762, 32758, 32752, 32745, 32737,
	32728, 32718, 32706, 32693, 32679, 32663, 32647, 32629,
	32610, 32589, 32568, 32545, 32521, 32496, 32469, 32442,
	32413, 32383, 32351, 32319, 32285, 32250, 32214, 32176,
	32138, 32098, 32057, 32015, 31971, 31927, 31881, 31834,
	31785, 31736, 31685, 31634, 31581, 31526, 31471, 31414,
	31357, 31298, 31237, 31176, 31114, 31050, 30985, 30919,
	30852, 30784, 30714, 30644, 30572, 30499, 30425, 30350,
	30273, 30196, 30117, 30037, 29956, 29874, 29791, 29707,
	29621, 29535, 29447, 29359, 29269, 29178, 29086, 28993,
	28898, 28803, 28707, 28609, 28511, 28411, 28310, 28208,
	28106, 28002, 27897, 27791, 27684, 27576, 27466, 27356,
	27245, 27133, 27020, 26905, 26790, 26674, 26557, 26438,
	26319, 26199, 26077, 25955, 25832, 25708, 25583, 25457,
	25330, 25201, 25073, 24943, 24812, 24680, 24547, 24414,
	24279, 24144, 24007, 23870, 23732, 23593, 23453, 23312,
	23170, 23027, 22884, 22740, 22594, 22448, 22301, 22154,
	22005, 21856, 21706, 21555, 21403, 21250, 21097, 20942,
	20787, 20631, 20475, 20318, 20159, 20001, 19841, 19681,
	19519, 19358, 19195, 19032, 18868, 18703, 18537, 18371,
	18204, 18037, 17869, 17700, 17530, 17360, 17189, 17018,
	16846, 16673, 16499, 16325, 16151, 15976, 15800, 15623,
	15446, 15269, 15090, 14912, 14732, 14552, 14372, 14191,
	14010, 13828, 13645, 13462, 13278, 13094, 12910, 12725,
	12539, 12353, 12167, 11980, 11793, 11605, 11416, 11228,
	11039, 10849, 10659, 10469, 10278, 10087, 9896, 9704,
	9512, 9319, 9126, 8933, 8739, 8545, 8351, 8156,
	7961, 7766, 7571, 7375, 7179, 6983, 6786, 6589,
	6392, 6195, 5997, 5800, 5602, 5403, 5205, 5006,
	4808, 4609, 4409, 4210, 4011, 3811, 3611, 3411,
	3211, 3011, 2811, 2611, 2410, 2210, 2009, 1808,
	1607, 1407, 1206, 1005, 804, 603, 402, 201,
	0,
};

static const unsigned short fpm_sin_table[FPM_PHASOR_TABLE] = {
	0, 201, 402, 603, 804, 1005, 1206, 1407,
	1607, 1808, 2009, 2210, 2410, 2611, 2811, 3011,
	3211, 3411, 3611, 3811, 4011, 4210, 4409, 4609,
	4808, 5006, 5205, 5403, 5602, 5800, 5997, 6195,
	6392, 6589, 6786, 6983, 7179, 7375, 7571, 7766,
	7961, 8156, 8351, 8545, 8739, 8933, 9126, 9319,
	9512, 9704, 9896, 10087, 10278, 10469, 10659, 10849,
	11039, 11228, 11416, 11605, 11793, 11980, 12167, 12353,
	12539, 12725, 12910, 13094, 13278, 13462, 13645, 13828,
	14010, 14191, 14372, 14552, 14732, 14912, 15090, 15269,
	15446, 15623, 15800, 15976, 16151, 16325, 16499, 16673,
	16846, 17018, 17189, 17360, 17530, 17700, 17869, 18037,
	18204, 18371, 18537, 18703, 18868, 19032, 19195, 19358,
	19519, 19681, 19841, 20001, 20159, 20318, 20475, 20631,
	20787, 20942, 21097, 21250, 21403, 21555, 21706, 21856,
	22005, 22154, 22301, 22448, 22594, 22740, 22884, 23027,
	23170, 23312, 23453, 23593, 23732, 23870, 24007, 24144,
	24279, 24414, 24547, 24680, 24812, 24943, 25073, 25201,
	25330, 25457, 25583, 25708, 25832, 25955, 26077, 26199,
	26319, 26438, 26557, 26674, 26790, 26905, 27020, 27133,
	27245, 27356, 27466, 27576, 27684, 27791, 27897, 28002,
	28106, 28208, 28310, 28411, 28511, 28609, 28707, 28803,
	28898, 28993, 29086, 29178, 29269, 29359, 29447, 29535,
	29621, 29707, 29791, 29874, 29956, 30037, 30117, 30196,
	30273, 30350, 30425, 30499, 30572, 30644, 30714, 30784,
	30852, 30919, 30985, 31050, 31114, 31176, 31237, 31298,
	31357, 31414, 31471, 31526, 31581, 31634, 31685, 31736,
	31785, 31834, 31881, 31927, 31971, 32015, 32057, 32098,
	32138, 32176, 32214, 32250, 32285, 32319, 32351, 32383,
	32413, 32442, 32469, 32496, 32521, 32545, 32568, 32589,
	32610, 32629, 32647, 32663, 32679, 32693, 32706, 32718,
	32728, 32737, 32745, 32752, 32758, 32762, 32765, 32767,
	32768,
};

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
 * sine and cosine over all 65536 phases.  Finding 3624's objection was that
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
 * Findings 3588, 3620-3624 and 3700-3703, deviation D392.
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
	return (i >= 0 && i < FPM_PHASOR_TABLE) ? fpm_cos_table[i] : 0;
}

unsigned short
FPM_phasor_sin_entry(int i)
{
	return (i >= 0 && i < FPM_PHASOR_TABLE) ? fpm_sin_table[i] : 0;
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
	p->cos = phasor_value(fpm_cos_table, FPM_cos_sign_ext, idx, frac, quad);
	p->sin = phasor_value(fpm_sin_table, FPM_sin_sign_ext, idx, frac, quad);
	p->phase = phasor_advance(phase, (short)p->inc);
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
	p->cos = phasor_value(fpm_cos_table, FPM_cos_sign_ext, idx, frac, quad);
	p->phase = phasor_advance(phase, (short)p->inc);
}

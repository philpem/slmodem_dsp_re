/*
 * cid_mtd.c -- reconstructed from dsplibs.o `Cidmtd.c`.
 *
 *   CID_MTD_detect  .text 0x0926a0  259 bytes
 *
 * The whole translation unit: one function and the four coefficient tables it
 * reads, all four file-static (`nm` shows them lower-case `d`, unlike the
 * GLOBAL MTD1..MTD8 family next door in .data, which belongs to the DTMF tone
 * bank).
 *
 * WHAT IT DOES, and why it answers backwards.
 *
 * It is the FSK message's gate: `cid_progress` will not hand samples to the
 * demodulator until this says a mark tone is on the line.  Two notches in
 * cascade take out 1200 Hz and 1300 Hz, and what survives them is compared
 * with the energy that went in --
 *
 *     detected  <=>  input > 150  AND  input / 2 > residual
 *
 * so more than half the block's energy has to be inside those two notches.
 * The return is the NEGATION of that: **0 means the tone is there**.  That is
 * not a reading, it is what `cid_modem` does with it (`test %ax,%ax`, and a
 * zero is what advances its confidence counter).
 *
 * THE TWO FREQUENCIES.  Solving each table back through
 *
 *     b = g * { 1, -2cos(w0), 1 },  a = { 1, 2r cos(w0), -r^2 }
 *
 * in FPM_iir_filt's own { a2, b2, a1, b1, b0 } order gives, at the table's
 * own rate and from the zeros and the poles independently:
 *
 *     MTD_COEF_1_8000   1200.0 Hz   r = 0.95   g = 0.95
 *     MTD_COEF_2_8000   1300.0 Hz   r = 0.90   g = 1.00
 *     MTD_COEF_1_9600   1200.0 Hz   r = 0.90   g = 1.00
 *     MTD_COEF_2_9600   1300.0 Hz   r = 0.90   g = 1.00
 *
 * 1200 Hz is Bell 202's mark and 1300 Hz is V.23's, so one detector covers
 * both regions' Caller ID.  MTD_COEF_1_8000 is the odd one of the four -- a
 * tighter pole radius and a gain that matches it -- and nothing here explains
 * why; finding 1507 records it without a reading.
 *
 * The four are reproduced as bytes.  The closed form above is exact to within
 * 0.05 Hz but does not regenerate them bit-for-bit, which is the same
 * position dtmf_mtd_coeffs.c is in (finding 1413).
 */

#include "dsplib/cid.h"
#include "dsplib/fpm_iir.h"

/*
 * `create_cid` allocates 0x160 bytes for this object (blob 0x91b64).
 * `make offsets` checks every field's offset and nothing checks the TOTAL, so
 * the size is asserted here: `diff_eq_obj` compares `sizeof(type)` bytes, and
 * a pad read short by eighteen would quietly narrow every comparison in
 * t_cid_mtd and t_cid_fsd while every annotated offset still matched.
 *
 * 32-bit only, following `dtmf_rx.c`'s reasoning: 0x160 is a claim about the
 * ABI the blob was built for, and the object really does hold a pointer --
 * the `fpm_mrf` at +0x0c has one at +0x24 (finding 1510).  This header hides
 * it inside `pad_000`, so the assertion would happen to hold at 64 bits today
 * and would stop holding the moment a later batch names that field.
 * `make check64` is where that would surface, and the guard is what keeps it
 * a real check there rather than an accident.
 */
#if defined(__i386__)
typedef char cid_size_check[sizeof(struct cid) == 0x160 ? 1 : -1];
#endif

/*
 * Declared in the object's own .data order, 0x7858 upwards: the 8000 pair
 * first and, within each pair, the 1300 Hz table before the 1200 Hz one.
 */
static const short MTD_COEF_2_8000[5] = { -13271, 16384, 15409, -17121, 16384 };
static const short MTD_COEF_1_8000[5] = { -14786, 15564, 18296, -18296, 15564 };
static const short MTD_COEF_2_9600[5] = { -13271, 16384, 19445, -21605, 16384 };
static const short MTD_COEF_1_9600[5] = { -13271, 16384, 20853, -23170, 16384 };

/*
 * Reach a table by name from outside the translation unit, which nothing in
 * the original does and the differential test cannot do without: a static has
 * no `ref_` alias to compare against.  Same arrangement as FPM_div_table's.
 */
const short *
CID_MTD_coeff(int which, int rate)
{
	if (rate == CID_RATE_9600)
		return which == 1 ? MTD_COEF_1_9600 :
		       which == 2 ? MTD_COEF_2_9600 : 0;
	if (rate == CID_RATE_8000)
		return which == 1 ? MTD_COEF_1_8000 :
		       which == 2 ? MTD_COEF_2_8000 : 0;
	return 0;
}

short
CID_MTD_detect(const short *samples, short count, struct cid *cid)
{
	const short *coef1 = MTD_COEF_1_9600;
	const short *coef2 = MTD_COEF_2_9600;
	unsigned int wide = 0;
	unsigned int narrow = 0;
	short i;

	if (cid->rate != CID_RATE_9600) {
		coef1 = MTD_COEF_1_8000;
		coef2 = MTD_COEF_2_8000;
	}

	for (i = 0; i < count; i++) {
		short x = samples[i];
		short y;

		/*
		 * Both energies are accumulated with the same rounding shift,
		 * so the comparison at the bottom is scale-free.  x*x cannot
		 * overflow -- 0x8000 squared is 2^30 -- but the SUM can, and
		 * 257 full-scale samples are enough to do it; see D306.
		 */
		wide += (unsigned int)(((int)x * x + 32) >> 6);

		/* The two notches in cascade, one biquad each. */
		y = FPM_iir_filt(x, coef1, cid->mtd1_state, 1);
		y = FPM_iir_filt(y, coef2, cid->mtd2_state, 1);

		narrow += (unsigned int)(((int)y * y + 32) >> 6);
	}

	/* Both comparisons are unsigned in the object.  0 means detected. */
	return (short)!(wide > 150 && wide / 2 > narrow);
}

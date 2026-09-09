/*
 * mtk.c -- MTK_phasor, the object's only reader of the MTK sine tables.
 *
 * Reconstructed from dsplibs.o:
 *
 *   MTK_phasor      .text 0x0b0690    271 bytes
 *
 * It sits in the `Fdspkrnl.c` span, next to the silence detector, and it is
 * a quarter-wave table oscillator: reduce the phase modulo 2pi, scale it by
 * 512/pi so the integer part is a 10-bit angle, interpolate the quarter
 * wave with the fraction, and pick the sign from the quadrant.  One call
 * produces one cosine and one sine and advances the phase by `step`.
 *
 * WHY IT IS IN A FILE OF ITS OWN.  `src/service/mtk_tables.c` holds the six
 * tables and nothing else, and this is the function that fixes their shape;
 * putting the code beside the data would put 3.5 KB of `.data` in the same
 * translation unit as the one function that reads it, which the object does
 * not do -- the tables are at `.data` 0x8500 and this is at 0xb0690, a long
 * way from each other and from anything else either touches.
 *
 * THE THREE CONSTANTS ARE NOT WHAT YOU WOULD GUESS, AND TWO SPELLINGS OF
 * 2*PI LIVE HERE AT ONCE.  `.rodata.cst8` 0x1f8 is 6.28318530718, which is
 * NOT the double nearest 2*pi -- the author typed eleven decimal places and
 * stopped -- while `.rodata.cst8` 0x208 is 3.141592653589793, which IS the
 * double nearest pi.  `.rodata.cst4` 0x57c is the FLOAT 2*pi, and the float
 * nearest 6.28318530718 and the float nearest 2*pi are the same float, so
 * that one says nothing about which the author wrote.  `TONE_generate` in
 * `Fdspkrnl.c` carries the same truncated double, which is why it is kept
 * rather than corrected.
 *
 * `fprem` IS THE ONE IN THE WHOLE OBJECT, and no `fmod` symbol is defined
 * or referenced anywhere in the 1.2 MB -- so the original's build expanded
 * the remainder inline, exactly as finding F8762 argues for `fsin`/`fcos`.
 * No pragma is set here: GCC 14 already emits the `fprem` loop for `fmodf`
 * with an out-of-line fallback, and licensing reassociation across this
 * function to chase a period-compiler mnemonic would risk the interpolation
 * for nothing the differential tier can see.
 */

#include <math.h>

#include "dsplib/fdspkrnl.h"
#include "dsplib/mtk.h"

/*
 * One step of the oscillator.
 *
 * `p->phase` is written TWICE -- once with the reduced angle before the
 * table lookups and once with the advanced angle at the end -- which is two
 * statements in the object (`fsts (%ecx)` at 0xb06db, `fstps (%ecx)` at
 * 0xb0789 or 0xb0792) and not a transcription slip.
 *
 * So is each output.  `fsts` stores the interpolation, then `fmuls` scales
 * the 80-bit value the register still holds and `fstps` stores that: the
 * sign is applied to the UNNARROWED interpolation, which a single
 * `interp * sign` statement would not emit.
 *
 * `frac` is never narrowed at all in the object -- it comes out of the
 * subtraction at 0xb071e and feeds both interpolations straight from the
 * x87 stack.  That is the shape `tools/gccdiverge.json` records for
 * `t_v90equproc` and `t_psd`, so the period tier is what decides here.
 */
void
MTK_phasor(struct mtk_phasor *p)
{
	float x;
	float y;
	float frac;
	float s;
	int n;
	int quadrant;
	int i;

	/*
	 * The reduction is against the FLOAT 2*pi and the correction adds
	 * the DOUBLE one, so a phase a hair below zero comes back as a
	 * value slightly larger than the modulus was.  Deviation D993.
	 */
	x = fmodf(p->phase, 6.28318530718f);
	if (x < 0.0f)
		x = (float)(x + 6.28318530718);
	p->phase = x;

	/* 162.97466172610083 is 512/pi, so `n` is the angle in 1/1024ths. */
	y = (float)(x * 162.97466172610083);
	n = (short)y;
	quadrant = n >> 8;
	frac = y - (float)n;

	/*
	 * Bits 0..7 index the quarter wave and bit 8 says which way along
	 * it: on the way back the fraction runs the other way and the index
	 * is complemented, which is `not %eax` and not a subtraction.
	 */
	if (n & 0x100) {
		frac = 1.0f - frac;
		n = ~n;
	}
	i = n & 0xff;

	p->cosine = MTK_cos_table[i]
		    + (MTK_cos_table[i + 1] - MTK_cos_table[i]) * frac;
	p->cosine = p->cosine * MTK_cos_sign[quadrant];
	p->sine = MTK_sin_table[i]
		    + (MTK_sin_table[i + 1] - MTK_sin_table[i]) * frac;
	p->sine = p->sine * MTK_sin_sign[quadrant];

	/*
	 * The advance wraps at PI and not at 2*pi, so the phase this leaves
	 * behind is in [-pi, pi) and the next call's `fmodf` sees a
	 * negative angle roughly half the time.
	 */
	s = x + p->step;
	if (s >= 3.141592653589793)
		s = (float)(s - 6.28318530718);
	p->phase = s;
}

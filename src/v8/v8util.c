/*
 * v8util.c -- the arithmetic leaves of the V.8 handshake.
 *
 * Six small helpers that everything else in V.8 is built from: a Q14
 * multiply, an absolute value, the cosine table, the CRC bit step, a coeff
 * copy and the energy pass over a DFT bin array.  They are reconstructed
 * first because the whole of V.8 bottoms out here -- `V8Create` reaches the
 * signal layer through `v8handshakinit`, and the signal layer reaches these.
 */

#include "dsplib/v8.h"

/*
 * A Q14 cosine table, one full cycle in 256 steps.  Every entry but one is
 * exactly `(short)(16384.0 * cos(2 * PI * i / 256))` with C truncation
 * towards zero.
 *
 * The exception is index 128, half a cycle, where this holds -16383 and the
 * arithmetic says -16384.  That is the floating point showing through: their
 * cosine returned slightly more than -1, so truncation dropped a unit.  Kept
 * verbatim -- it is one LSB and reproducing it costs nothing, whereas
 * regenerating the table would silently change one sample of every tone V.8
 * emits.
 */
const short v8_costab[V8_COSTAB_SIZE] = {
	 16384,  16379,  16364,  16339,  16305,  16260,  16206,  16142,
	 16069,  15985,  15892,  15790,  15678,  15557,  15426,  15286,
	 15136,  14978,  14810,  14634,  14449,  14255,  14053,  13842,
	 13622,  13395,  13159,  12916,  12665,  12406,  12139,  11866,
	 11585,  11297,  11002,  10701,  10393,  10079,   9759,   9434,
	  9102,   8765,   8423,   8075,   7723,   7366,   7005,   6639,
	  6269,   5896,   5519,   5139,   4756,   4369,   3980,   3589,
	  3196,   2801,   2404,   2005,   1605,   1205,    803,    402,
	     0,   -402,   -803,  -1205,  -1605,  -2005,  -2404,  -2801,
	 -3196,  -3589,  -3980,  -4369,  -4756,  -5139,  -5519,  -5896,
	 -6269,  -6639,  -7005,  -7366,  -7723,  -8075,  -8423,  -8765,
	 -9102,  -9434,  -9759, -10079, -10393, -10701, -11002, -11297,
	-11585, -11866, -12139, -12406, -12665, -12916, -13159, -13395,
	-13622, -13842, -14053, -14255, -14449, -14634, -14810, -14978,
	-15136, -15286, -15426, -15557, -15678, -15790, -15892, -15985,
	-16069, -16142, -16206, -16260, -16305, -16339, -16364, -16379,
	-16383, -16379, -16364, -16339, -16305, -16260, -16206, -16142,
	-16069, -15985, -15892, -15790, -15678, -15557, -15426, -15286,
	-15136, -14978, -14810, -14634, -14449, -14255, -14053, -13842,
	-13622, -13395, -13159, -12916, -12665, -12406, -12139, -11866,
	-11585, -11297, -11002, -10701, -10393, -10079,  -9759,  -9434,
	 -9102,  -8765,  -8423,  -8075,  -7723,  -7366,  -7005,  -6639,
	 -6269,  -5896,  -5519,  -5139,  -4756,  -4369,  -3980,  -3589,
	 -3196,  -2801,  -2404,  -2005,  -1605,  -1205,   -803,   -402,
	     0,    402,    803,   1205,   1605,   2005,   2404,   2801,
	  3196,   3589,   3980,   4369,   4756,   5139,   5519,   5896,
	  6269,   6639,   7005,   7366,   7723,   8075,   8423,   8765,
	  9102,   9434,   9759,  10079,  10393,  10701,  11002,  11297,
	 11585,  11866,  12139,  12406,  12665,  12916,  13159,  13395,
	 13622,  13842,  14053,  14255,  14449,  14634,  14810,  14978,
	 15136,  15286,  15426,  15557,  15678,  15790,  15892,  15985,
	 16069,  16142,  16206,  16260,  16305,  16339,  16364,  16379
};

/* Q14 multiply: the product of two Q14 values, back in Q14. */
short
v8_mpyint(short a, short b)
{
	return (short)((a * b) >> 14);
}

/*
 * Absolute value, with the usual two's-complement corner left in place:
 * `v8_absfn(-32768)` is -32768, because negating it overflows and the result
 * is narrowed back to a short.  No caller reaches it -- the signal path is
 * scaled well below full scale -- so it is reproduced rather than fixed.
 */
short
v8_absfn(short x)
{
	if (x < 0)
		return (short)(-x);
	return x;
}

/* One entry of the cosine table.  The index is a byte, so it wraps freely. */
short
v8_cosread(unsigned char phase)
{
	return v8_costab[phase];
}

/*
 * One bit into the CRC-16-CCITT register the handshake carries in its state.
 * Polynomial 0x1021, MSB first, no reflection: shift up, and if the bit
 * leaving the top disagrees with the bit going in, fold the polynomial back.
 *
 * `bit` is compared 16 bits at a time, so a value whose low half is zero
 * counts as a zero bit whatever the upper half holds.
 */
void
v8_crc(struct v8_handshake *hs, int bit)
{
	unsigned int crc = (unsigned short)hs->crc;
	int msb = ((int)(short)crc) < 0 ? 1 : 0;

	crc += crc;
	if ((short)bit != 0)
		msb ^= 1;
	if (msb != 0)
		crc ^= 0x1021;
	hs->crc = (short)crc;
}

/* Copy `n` coefficients.  The counter is a short, so `n` above 32767 never
 * terminates -- no caller comes close. */
void
v8_copycoeff(short *dst, const short *src, short n)
{
	short i;

	for (i = 0; i < n; i++)
		dst[i] = src[i];
}

/*
 * Energy of each DFT bin: the real and imaginary parts are shifted up by
 * `shift`, taken down to their top 16 bits, squared and summed, and the top
 * 16 bits of that are stored.  The shift is how the caller keeps a bin that
 * has grown small from squaring away to nothing.
 */
void
v8_dftenergy(struct v8_dft_bin *bin, short n, short shift)
{
	short i;

	for (i = 0; i < n; i++) {
		int re = (int)((unsigned int)bin[i].re << shift) >> 16;
		int im = (int)((unsigned int)bin[i].im << shift) >> 16;

		bin[i].energy = (short)((re * re + im * im) >> 16);
	}
}

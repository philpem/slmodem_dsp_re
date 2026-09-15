/*
 * V8Dftc.c -- counter-independent DFT/cosine ownership recovered from
 * LOCAL v8_costbl and its three reference relocations: two in
 * v8_dftupdate and one in v8_cosread. Bodies and data are unchanged.
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
static const short v8_costbl[V8_COSTAB_SIZE] = {
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

/*
 * Advance a sliding DFT.  One oscillator per bin rather than a transform over
 * a block: each bin steps its own phase, reads cosine and sine out of the one
 * table a quarter cycle apart, and adds the products into its running sums.
 */
void
v8_dftupdate(struct v8_dft_bin *bins, short nbins, const short *samples,
	     short nsamples)
{
	short j;

	for (j = 0; j < nsamples; j++) {
		short i;

		for (i = 0; i < nbins; i++) {
			struct v8_dft_bin *b = &bins[i];
			unsigned phase;
			unsigned idx;
			int x = samples[j];

			phase = ((unsigned)(unsigned short)b->phase
				 + (unsigned short)b->step) & 0x3fff;
			b->phase = (short)phase;

			idx = phase >> 6;
			b->re += (v8_cosread((unsigned char)idx) * x) >> 6;
			b->im += (v8_cosread((unsigned char)(idx + 0x40)) * x)
				 >> 6;
		}
	}
}

/* One entry of the cosine table.  The index is a byte, so it wraps freely. */
short
v8_cosread(unsigned char phase)
{
	return v8_costbl[phase];
}


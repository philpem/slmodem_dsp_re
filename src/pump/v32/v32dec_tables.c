/*
 * v32dec_tables.c -- V.32's constellation and decision tables.
 *
 * Reconstructed from dsplibs.o:
 *   DECv32_ANGL1200 .data 0x074e0    8
 *   DECv32_MAP_TRN  .data 0x074d8    8
 *   DECv32_IMAP4    .data 0x074f0    8
 *   DECv32_QMAP4    .data 0x074e8    8
 *   DECv32_MAG9600  .data 0x074f8    6
 *   DECv32_ANGL9600 .data 0x07500   32
 *   DECv32_IMAP16   .data 0x07540   32
 *   DECv32_QMAP16   .data 0x07520   32
 *   SMCv32_IMAP16  .rodata 0x07d80  34   (out of batch -- see the header)
 *   SMCv32_QMAP16  .rodata 0x07d40  34
 *   SMCv32_PMAP16  .rodata 0x07daa   8
 *
 * Every one is indexed `(%reg,%reg,1)` off its base with a 16-bit load, so
 * the element width is two bytes in all eleven cases.
 *
 * The four-point constellation is the V.32 "A/B/C/D" set at 45 degrees; the
 * sixteen-point one is V.32bis' 9600 bit/s constellation.  Coordinates are
 * +-4096 and +-12288, angles are in the phasor's units where 0x8000 is a
 * full cycle.
 */

#include "dsplib/v32dec.h"

short DECv32_ANGL1200[4] = { 9869, 18061, 26253, 1678 };

/* Applied to the TRN symbol between symbol 256 and symbol 1279. */
short DECv32_MAP_TRN[4] = { 1, 2, 0, 3 };

short DECv32_IMAP4[4] = { -4096, -12288, 4096, 12288 };
short DECv32_QMAP4[4] = { 12288, -4096, -12288, 4096 };

/* Indexed by (|I| + |Q|) >> 1, less one. */
short DECv32_MAG9600[3] = { 5792, 12953, 17378 };

short DECv32_ANGL9600[16] = {
	12287, 14706,  9869, 12287, 18061, 20480, 20480, 22898,
	 6514,  4095,  4095,  1677, 28672, 26253, 31090, 28671
};

short DECv32_IMAP16[16] = {
	-12288, -12288,  -4096,  -4096, -12288, -12288,  -4096,  -4096,
	  4096,   4096,  12288,  12288,   4096,   4096,  12288,  12288
};

short DECv32_QMAP16[16] = {
	 12288,   4096,  12288,   4096,  -4096, -12288,  -4096, -12288,
	 12288,   4096,  12288,   4096,  -4096, -12288,  -4096, -12288
};

/*
 * The encoder's own ordering of the same sixteen points, which is what turns
 * a decided (I, Q) back into the four bits that produced it.  Both are 34
 * bytes in the object and only 0..15 is indexed.
 */
const short SMCv32_IMAP16[17] = {
	 -4096, -12288,  -4096, -12288,   4096,   4096,  12288,  12288,
	  4096,  12288,   4096,  12288,  -4096,  -4096, -12288, -12288,
	     0
};

const short SMCv32_QMAP16[17] = {
	 -4096,  -4096, -12288, -12288,  -4096, -12288,  -4096, -12288,
	  4096,   4096,  12288,  12288,   4096,  12288,   4096,  12288,
	     0
};

/* Quadrant rotation -> the two differentially encoded bits, times four. */
const short SMCv32_PMAP16[4] = { 4, 0, 8, 12 };

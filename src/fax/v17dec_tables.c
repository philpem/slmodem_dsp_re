/*
 * v17dec_tables.c -- the V.17 fax receiver's constellation, magnitude and
 *                    angle tables, as `FAX_FSE_decision_*` index them.
 *
 * Reconstructed from dsplibs.o:
 *   DECv17_MAP_BRIDGE      .rodata  0x09874     8
 *   DECv17_MAP_TRN         .rodata  0x0987c     8
 *   DECv17_ANGL4800        .rodata  0x09884     8
 *   DECv17_QMAP4           .rodata  0x0988c     8
 *   DECv17_IMAP4           .rodata  0x09894     8
 *   DECv17_MAG7200         .rodata  0x0989c     6
 *   DECv17_ANGL7200        .rodata  0x098c0    32
 *   DECv17_QMAP16          .rodata  0x098e0    32
 *   DECv17_IMAP16          .rodata  0x09900    32
 *   DECv17_ANGL9600T       .rodata  0x09920    64
 *   DECv17_MAG9600T        .rodata  0x09960    64
 *   DECv17_SIN_ROT_ANGLE   .rodata  0x099a0     8
 *   DECv17_COS_ROT_ANGLE   .rodata  0x099a8     8
 *   DECv17_ANA_QMAP        .rodata  0x099b0    16
 *   DECv17_QMAP64          .rodata  0x0bb80   128
 *   DECv17_IMAP64          .rodata  0x0bc00   128
 *   DECv17_ANGL12000       .rodata  0x0bc80   128
 *   DECv17_MAG12000        .rodata  0x0bd00   128
 *   DECv17_ANA_QMAP128     .rodata  0x0bd80    64
 *   DECv17_ANA_IMAP128     .rodata  0x0bdc0    64
 *   DECv17_ANGL14400       .rodata  0x0be00   256
 *   DECv17_MAG14400        .rodata  0x0bf00   256
 *
 * ALL TWENTY-TWO ARE `.rodata`, so all twenty-two are `const`.  V.32bis'
 * equivalent set is split between `.data` and `.rodata` and this tree spells
 * that split out in `v32dec_tables.c`; V.17's is not split, and the section
 * each symbol is defined in is the whole of the reason either way.
 *
 * THE ELEMENT TYPE HAS TWO INDEPENDENT READINGS and `include/dsplib/v17dec.h`
 * states both: the object's own sixteen-bit `(%reg,%reg,1)` loads, and
 * byte-identity with V.32bis' already-reconstructed set for nineteen of the
 * twenty-two.  The three that differ are named in that header.
 *
 * WHAT THE VALUES MEAN, where the object's arithmetic settles it:
 *
 *   Coordinates are in the equaliser's units.  The four-point set is +-4096
 *   and +-12288 at 45 degrees; the sixteen-point set is the same four
 *   quadrant patterns; the 64- and 128-point sets step by 4096 and 2048.
 *
 *   Angles are in the phasor's units, where 0x8000 is one full cycle.  That
 *   is visible in `DECv17_ANGL9600T`, whose four blocks of eight differ by
 *   8192 -- a quarter cycle -- because the four blocks are the four rotations
 *   of one eight-point set, and a rotation adds a constant angle.  It is
 *   visible again in `DECv17_MAG9600T`, which is eight magnitudes repeated
 *   four times, because a rotation cannot change a radius.
 *
 *   `DECv17_MAG7200` is indexed `(|I| + |Q|) >> 13`, LESS ONE -- the object
 *   loads it `movzwl -0x2(%ebp,%ebp,1)` at 0x984e5 after `sar $0xd` at
 *   0x984e2.  Three entries cover the only three L1 sums the sixteen-point
 *   constellation can produce, and each is the corresponding L2 radius:
 *   4096*sqrt(2) = 5792.6, sqrt(12288^2 + 4096^2) = 12953.0 and
 *   12288*sqrt(2) = 17377.9.
 *
 *   NOTE THAT V.17 DOES NOT CARRY V.32's D302.  `FSE_decision_16pt` shifts by
 *   1 where it should shift by 13 and reads thousands of entries past the end
 *   of `DECv32_MAG9600`; `FAX_FSE_decision_16pt` shifts by 13 (`sar $0xd` at
 *   0x984e2) and reads entries 0..2.  Same table contents, same three-entry
 *   length, different code.  Checked because the two functions are otherwise
 *   the same shape, and assuming the defect transferred would have been the
 *   easy mistake.
 *
 * `DECv17_MAP_BRIDGE` IS DEAD IN THE OBJECT.  `relocscan.py --into` over the
 * whole 1.2 MB, with the fix that resolves relocations naming their target,
 * reports it unreferenced -- no relocation anywhere points at it.  It is
 * written because it is a global the object defines and it costs eight bytes,
 * not because anything reads it.  Its neighbour `DECv17_MAP_TRN` IS read, by
 * `FSE_decision_eqtrn`, and the pair reads as the equaliser-training and
 * bridge-phase symbol remappings of one handshake.
 */

#include "dsplib/v17dec.h"

/* DECv17_MAP_BRIDGE  .rodata 0x09874  8 bytes.
 * Dead in the object -- nothing references it.  See the file banner.
 */
const short DECv17_MAP_BRIDGE[4] = {
	     1,      0,      2,      3,
};

/* DECv17_MAP_TRN  .rodata 0x0987c  8 bytes.
 * Applied to the TRN symbol by `FSE_decision_eqtrn`, which indexes
 * it at 0x9895e and 0x98999.  V.32bis' own is { 1, 2, 0, 3 }.
 */
const short DECv17_MAP_TRN[4] = {
	     3,      0,      2,      1,
};

/* DECv17_ANGL4800  .rodata 0x09884  8 bytes.
 * The four handshake-constellation angles, one per quadrant.  Read by
 * `FAX_FSE_decision_AB`, `FSE_Bridge_det` and `FSE_decision_eqtrn`.
 * V.32's DECv32_ANGL1200 holds { 9869, 18061, 26253, 1678 } -- the first
 * three of these are each exactly one greater, the fourth is equal.
 */
const short DECv17_ANGL4800[4] = {
	  9870,  18062,  26254,   1678,
};

/* DECv17_QMAP4  .rodata 0x0988c  8 bytes.
 * The four-point handshake constellation, +-4096 and +-12288.
 */
const short DECv17_QMAP4[4] = {
	 12288,  -4096, -12288,   4096,
};

/* DECv17_IMAP4  .rodata 0x09894  8 bytes.
 * The four-point handshake constellation, +-4096 and +-12288.
 */
const short DECv17_IMAP4[4] = {
	 -4096, -12288,   4096,  12288,
};

/* DECv17_MAG7200  .rodata 0x0989c  6 bytes.
 * Three L2 radii of the sixteen-point set, indexed `((|I|+|Q|)>>13)-1`.
 */
const short DECv17_MAG7200[3] = {
	  5792,  12953,  17378,
};

/* DECv17_ANGL7200  .rodata 0x098c0  32 bytes.
 * Sixteen angles, 0x8000 to the cycle.
 */
const short DECv17_ANGL7200[16] = {
	 12287,  14706,   9869,  12287,  18061,  20480,  20480,  22898,
	  6514,   4095,   4095,   1677,  28672,  26253,  31090,  28671,
};

/* DECv17_QMAP16  .rodata 0x098e0  32 bytes.
 * The sixteen-point 7200 bit/s constellation.
 */
const short DECv17_QMAP16[16] = {
	 12288,   4096,  12288,   4096,  -4096, -12288,  -4096, -12288,
	 12288,   4096,  12288,   4096,  -4096, -12288,  -4096, -12288,
};

/* DECv17_IMAP16  .rodata 0x09900  32 bytes.
 * The sixteen-point 7200 bit/s constellation.
 */
const short DECv17_IMAP16[16] = {
	-12288, -12288,  -4096,  -4096, -12288, -12288,  -4096,  -4096,
	  4096,   4096,  12288,  12288,   4096,   4096,  12288,  12288,
};

/* DECv17_ANGL9600T  .rodata 0x09920  64 bytes.
 * Eight points by four rotations; the blocks differ by 8192.
 */
const short DECv17_ANGL9600T[32] = {
	 11258,  10610,   8191,   9469,   8191,   5773,   6914,   5125,
	 19450,  18802,  16384,  17661,  16384,  13965,  15106,  13317,
	 27642,  26994,  24576,  25853,  24576,  22157,  23298,  21509,
	  3066,   2418,      0,   1277,      0,  30349,  31490,  29701,
};

/* DECv17_MAG9600T  .rodata 0x09960  64 bytes.
 * Eight radii repeated four times -- a rotation cannot change a radius.
 */
const short DECv17_MAG9600T[32] = {
	 14768,   9158,   4096,  16888,  12288,   9158,  16888,  14768,
	 14768,   9158,   4096,  16888,  12288,   9158,  16888,  14768,
	 14768,   9158,   4096,  16888,  12288,   9158,  16888,  14768,
	 14768,   9158,   4096,  16888,  12288,   9158,  16888,  14768,
};

/* DECv17_SIN_ROT_ANGLE  .rodata 0x099a0  8 bytes.
 * +-23170 is 0.7071 in Q15: the 45-degree rotation, per quadrant.
 */
const short DECv17_SIN_ROT_ANGLE[4] = {
	-23170, -23170,  23170,  23170,
};

/* DECv17_COS_ROT_ANGLE  .rodata 0x099a8  8 bytes.
 * +-23170 is 0.7071 in Q15: the 45-degree rotation, per quadrant.
 */
const short DECv17_COS_ROT_ANGLE[4] = {
	 23170, -23170, -23170,  23170,
};

/* DECv17_ANA_QMAP  .rodata 0x099b0  16 bytes.
 * The rails the 32-point slicer clamps the rotated Q against.
 */
const short DECv17_ANA_QMAP[8] = {
	 14481,   8689,   2896,  14481,   8689,   2896,   8689,   2896,
};

/* DECv17_QMAP64  .rodata 0x0bb80  128 bytes.
 * The sixty-four-point 12000 bit/s constellation, stepping by 4096.
 */
const short DECv17_QMAP64[64] = {
	 14336,  10240,  14336,  10240,   6144,   2048,   6144,   2048,
	 14336,  10240,  14336,  10240,   6144,   2048,   6144,   2048,
	 -2048,  -6144,  -2048,  -6144, -10240, -14336, -10240, -14336,
	 -2048,  -6144,  -2048,  -6144, -10240, -14336, -10240, -14336,
	 14336,  10240,  14336,  10240,   6144,   2048,   6144,   2048,
	 14336,  10240,  14336,  10240,   6144,   2048,   6144,   2048,
	 -2048,  -6144,  -2048,  -6144, -10240, -14336, -10240, -14336,
	 -2048,  -6144,  -2048,  -6144, -10240, -14336, -10240, -14336,
};

/* DECv17_IMAP64  .rodata 0x0bc00  128 bytes.
 * The sixty-four-point 12000 bit/s constellation, stepping by 4096.
 */
const short DECv17_IMAP64[64] = {
	-14336, -14336, -10240, -10240, -14336, -14336, -10240, -10240,
	 -6144,  -6144,  -2048,  -2048,  -6144,  -6144,  -2048,  -2048,
	-14336, -14336, -10240, -10240, -14336, -14336, -10240, -10240,
	 -6144,  -6144,  -2048,  -2048,  -6144,  -6144,  -2048,  -2048,
	  2048,   2048,   6144,   6144,   2048,   2048,   6144,   6144,
	 10240,  10240,  14336,  14336,  10240,  10240,  14336,  14336,
	  2048,   2048,   6144,   6144,   2048,   2048,   6144,   6144,
	 10240,  10240,  14336,  14336,  10240,  10240,  14336,  14336,
};

/* DECv17_ANGL12000  .rodata 0x0bc80  128 bytes.
 * Sixty-four angles, 0x8000 to the cycle.
 */
const short DECv17_ANGL12000[64] = {
	 12287,  13149,  11426,  12287,  14272,  15643,  13565,  15354,
	 10303,  11010,   8932,   9221,  12287,  14706,   9869,  12287,
	 17124,  18495,  17413,  19202,  19618,  20480,  20480,  21341,
	 18061,  20480,  20480,  22898,  21757,  22464,  23546,  23835,
	  7451,   7162,   6080,   5373,   6514,   4095,   4095,   1677,
	  4957,   4095,   4095,   3234,   2818,   1029,   2111,    740,
	 28671,  26253,  31090,  28671,  25605,  25316,  27394,  26687,
	 31738,  29949,  32027,  30656,  28671,  27810,  29533,  28671,
};

/* DECv17_MAG12000  .rodata 0x0bd00  128 bytes.
 * Sixty-four L2 radii.
 */
const short DECv17_MAG12000[64] = {
	 20274,  17617,  17617,  14481,  15597,  14481,  11941,  10442,
	 15597,  11941,  14481,  10442,   8688,   6476,   6476,   2896,
	 14481,  15597,  10442,  11941,  17617,  20274,  14481,  17617,
	  6476,   8688,   2896,   6476,  11941,  15597,  10442,  14481,
	 14481,  10442,  15597,  11941,   6476,   2896,   8688,   6476,
	 17617,  14481,  20274,  17617,  11941,  10442,  15597,  14481,
	  2896,   6476,   6476,   8688,  10442,  14481,  11941,  15597,
	 10442,  11941,  14481,  15597,  14481,  17617,  17617,  20274,
};

/* DECv17_ANA_QMAP128  .rodata 0x0bd80  64 bytes.
 * The rails the 128-point slicer clamps the rotated I and Q against.
 */
const short DECv17_ANA_QMAP128[32] = {
	 15929,  13033,  15929,  13033,  10137,   7240,  10137,   7240,
	  4344,   1448,   4344,   1448,  15929,  13033,  15929,  13033,
	 10137,   7240,  10137,   7240,   4344,   1448,   4344,   1448,
	 10137,   7240,  10137,   7240,   4344,   1448,   4344,   1448,
};

/* DECv17_ANA_IMAP128  .rodata 0x0bdc0  64 bytes.
 * The rails the 128-point slicer clamps the rotated I and Q against.
 */
const short DECv17_ANA_IMAP128[32] = {
	  1448,   1448,   4344,   4344,   1448,   1448,   4344,   4344,
	  1448,   1448,   4344,   4344,   7240,   7240,  10137,  10137,
	  7240,   7240,  10137,  10137,   7240,   7240,  10137,  10137,
	 13033,  13033,  15929,  15929,  13033,  13033,  15929,  15929,
};

/* DECv17_ANGL14400  .rodata 0x0be00  256 bytes.
 * One hundred and twenty-eight angles, 0x8000 to the cycle.
 */
const short DECv17_ANGL14400[128] = {
	 11815,  11710,  10899,  10610,  11547,  11258,  10176,   9469,
	 10610,   8191,   8191,   5773,  10063,   9643,   9332,   8840,
	  9053,   8191,   8191,   7330,   6914,   5125,   6207,   4836,
	  7543,   6740,   7051,   6320,   5773,   4673,   5484,   4568,
	 20007,  19902,  19091,  18802,  19739,  19450,  18368,  17661,
	 18802,  16384,  16384,  13965,  18255,  17835,  17524,  17032,
	 17245,  16384,  16384,  15522,  15106,  13317,  14399,  13028,
	 15735,  14932,  15243,  14512,  13965,  12865,  13676,  12760,
	 28199,  28094,  27283,  26994,  27931,  27642,  26560,  25853,
	 26994,  24576,  24576,  22157,  26447,  26027,  25716,  25224,
	 25437,  24576,  24576,  23714,  23298,  21509,  22591,  21220,
	 23927,  23124,  23435,  22704,  22157,  21057,  21868,  20952,
	  3623,   3518,   2707,   2418,   3355,   3066,   1984,   1277,
	  2418,      0,      0,  30349,   1871,   1451,   1140,    648,
	   861,      0,      0,  31906,  31490,  29701,  30783,  29412,
	 32119,  31316,  31627,  30896,  30349,  29249,  30060,  29144,
};

/* DECv17_MAG14400  .rodata 0x0bf00  256 bytes.
 * One hundred and twenty-eight L2 radii.
 */
const short DECv17_MAG14400[128] = {
	 15995,  13113,  16511,  13738,  10240,   7384,  11028,   8444,
	  4579,   2048,   6144,   4579,  17498,  14909,  18881,  16511,
	 12457,  10240,  14336,  12457,   8444,   7384,  11028,  10240,
	 16511,  14909,  18881,  17498,  13738,  13113,  16511,  15995,
	 15995,  13113,  16511,  13738,  10240,   7384,  11028,   8444,
	  4579,   2048,   6144,   4579,  17498,  14909,  18881,  16511,
	 12457,  10240,  14336,  12457,   8444,   7384,  11028,  10240,
	 16511,  14909,  18881,  17498,  13738,  13113,  16511,  15995,
	 15995,  13113,  16511,  13738,  10240,   7384,  11028,   8444,
	  4579,   2048,   6144,   4579,  17498,  14910,  18881,  16511,
	 12457,  10240,  14336,  12457,   8444,   7384,  11028,  10240,
	 16511,  14909,  18881,  17498,  13738,  13113,  16511,  15995,
	 15995,  13113,  16511,  13738,  10240,   7384,  11028,   8444,
	  4579,   2048,   6144,   4579,  17498,  14909,  18881,  16511,
	 12457,  10240,  14336,  12457,   8444,   7384,  11028,  10240,
	 16511,  14909,  18881,  17498,  13738,  13113,  16511,  15995,
};

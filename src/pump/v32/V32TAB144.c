/*
 * v32vtb_tables.c -- V.32bis' trellis constellations.
 *
 * Extracted from dsplibs.o with tools/tabdump.py and compared entry by entry
 * against the blob's own copies by t_v32vtb.  All eight are `.rodata` in
 * the object, so all eight are const.
 *
 *   VTBv32_{I,Q}MAP16T   .rodata 0x06e00 / 0x06de0    16 points, 7200
 *   VTBv32_{I,Q}MAP32    .rodata 0x06e60 / 0x06e20    32 points, 9600
 *   VTBv32_{I,Q}MAP64    .rodata 0x07880 / 0x07800    64 points, 12000
 *   VTBv32_{I,Q}MAP128   .rodata 0x07a00 / 0x07900   128 points, 14400
 *
 * WIDTH.  Two bytes, and it is measured three ways rather than assumed.
 * `VTB_decoder` loads a boundary `movswl (%ebx)` and a region entry
 * `movswl (%edi,%eax,2)` and uses both 32-bit results as indices; the region
 * index it computes cannot exceed 2*grid*grid - 1, which is exactly each
 * region table's entry count under the two-byte reading and twice it under a
 * four-byte one; and the largest region entry plus the decoder's 24-entry
 * quadrant offset plus the eight entries it then reads is exactly each
 * boundary table's entry count, again only at two bytes.  A byte-for-byte
 * comparison of the extracted bytes cannot tell those readings apart --
 * the bytes are the same either way -- which is why the length arithmetic
 * and the paired decoder test in t_v32vtb are what settle it.
 *
 * THE CONSTELLATIONS are V.32bis' trellis-coded sets at +-4096, +-12288 and,
 * for the 32-point one, +-8192 and +-16384 on the axes.  The 16- and
 * 64-point sets are the 45-degree-rotated ones, which is why the decoder
 * rotates the received point before the region lookup and not after.
 *
 * REGIONS AND BOUNDARIES ARE NOT THIS FILE'S.  The `VTB_REGION_*`/`VTB_BOUND_*`
 * boundary and region tables sit at `.rodata` 0x0d120..0x0ed50, inside the
 * fpm translation unit's slot (between fpm_tone.c's local `ToneLPF` and
 * fpm_vtb.c's local `VTB_DIFF_TBL`), not in V.32's.  They are defined in
 * `src/dsp/fpm_tren.c`; F11402 records the address argument.  This file keeps
 * only the V.32bis constellations, which are in V.32's own .rodata slot.
 */

#include "dsplib/vtb.h"

const short VTBv32_IMAP16T[16] = {
	12288, -4096, -12288, 4096, 12288, -4096, -12288, 4096,
	-4096, 12288, 4096, -12288, -12288, 4096, 12288, -4096,
};

const short VTBv32_QMAP16T[16] = {
	-12288, 4096, 12288, -4096, 4096, -12288, -4096, 12288,
	12288, -4096, -12288, 4096, -12288, 4096, 12288, -4096,
};

const short VTBv32_IMAP32[32] = {
	-16384, 0, 0, 16384, 16384, 0, 0, -16384,
	-8192, -8192, 8192, 8192, 8192, 8192, -8192, -8192,
	-12288, 4096, -12288, 4096, 12288, -4096, 12288, -4096,
	4096, -12288, 4096, 4096, -4096, 12288, -4096, -4096,
};

const short VTBv32_QMAP32[32] = {
	4096, -12288, 4096, 4096, -4096, 12288, -4096, -4096,
	12288, -4096, 12288, -4096, -12288, 4096, -12288, 4096,
	-8192, -8192, 8192, 8192, 8192, 8192, -8192, -8192,
	16384, 0, 0, -16384, -16384, 0, 0, 16384,
};

const short VTBv32_IMAP64[64] = {
	14336, 6144, 14336, -10240, 6144, -2048, -2048, -10240,
	-14336, -6144, -14336, 10240, -6144, 2048, 2048, 10240,
	-2048, -10240, 14336, -10240, 6144, -2048, 14336, 6144,
	2048, 10240, -14336, 10240, -6144, 2048, -14336, -6144,
	-10240, -2048, -10240, 14336, -2048, 6144, 6144, 14336,
	10240, 2048, 10240, -14336, 2048, -6144, -6144, -14336,
	2048, 10240, -14336, 10240, -6144, 2048, -14336, -6144,
	-2048, -10240, 14336, -10240, 6144, -2048, 14336, 6144,
};

const short VTBv32_QMAP64[64] = {
	2048, 10240, -14336, 10240, -6144, 2048, -14336, -6144,
	-2048, -10240, 14336, -10240, 6144, -2048, 14336, 6144,
	10240, 2048, 10240, -14336, 2048, -6144, -6144, -14336,
	-10240, -2048, -10240, 14336, -2048, 6144, 6144, 14336,
	-2048, -10240, 14336, -10240, 6144, -2048, 14336, 6144,
	2048, 10240, -14336, 10240, -6144, 2048, -14336, -6144,
	-14336, -6144, -14336, 10240, -6144, 2048, 2048, 10240,
	14336, 6144, 14336, -10240, 6144, -2048, -2048, -10240,
};

const short VTBv32_IMAP128[128] = {
	-16384, 16384, 8192, 8192, -8192, -8192, 0, 0,
	-16384, 16384, 8192, 8192, -8192, -8192, 0, 0,
	16384, -16384, -8192, -8192, 8192, 8192, 0, 0,
	16384, -16384, -8192, -8192, 8192, 8192, 0, 0,
	4096, 4096, 4096, 12288, 4096, 12288, 4096, 12288,
	-4096, -4096, -4096, -12288, -4096, -12288, -4096, -12288,
	-4096, -4096, -4096, -12288, -4096, -12288, -4096, -12288,
	4096, 4096, 4096, 12288, 4096, 12288, 4096, 12288,
	18432, -14336, -6144, -6144, 10240, 10240, 2048, 2048,
	18432, -14336, -6144, -6144, 10240, 10240, 2048, 2048,
	-18432, 14336, 6144, 6144, -10240, -10240, -2048, -2048,
	-18432, 14336, 6144, 6144, -10240, -10240, -2048, -2048,
	-6144, -6144, -6144, -14336, -6144, -14336, -6144, -14336,
	2048, 2048, 2048, 10240, 2048, 10240, 2048, 10240,
	6144, 6144, 6144, 14336, 6144, 14336, 6144, 14336,
	-2048, -2048, -2048, -10240, -2048, -10240, -2048, -10240,
};

const short VTBv32_QMAP128[128] = {
	-6144, -6144, -6144, -14336, -6144, -14336, -6144, -14336,
	2048, 2048, 2048, 10240, 2048, 10240, 2048, 10240,
	6144, 6144, 6144, 14336, 6144, 14336, 6144, 14336,
	-2048, -2048, -2048, -10240, -2048, -10240, -2048, -10240,
	-18432, 14336, 6144, 6144, -10240, -10240, -2048, -2048,
	-18432, 14336, 6144, 6144, -10240, -10240, -2048, -2048,
	18432, -14336, -6144, -6144, 10240, 10240, 2048, 2048,
	18432, -14336, -6144, -6144, 10240, 10240, 2048, 2048,
	4096, 4096, 4096, 12288, 4096, 12288, 4096, 12288,
	-4096, -4096, -4096, -12288, -4096, -12288, -4096, -12288,
	-4096, -4096, -4096, -12288, -4096, -12288, -4096, -12288,
	4096, 4096, 4096, 12288, 4096, 12288, 4096, 12288,
	16384, -16384, -8192, -8192, 8192, 8192, 0, 0,
	16384, -16384, -8192, -8192, 8192, 8192, 0, 0,
	-16384, 16384, 8192, 8192, -8192, -8192, 0, 0,
	-16384, 16384, 8192, 8192, -8192, -8192, 0, 0,
};

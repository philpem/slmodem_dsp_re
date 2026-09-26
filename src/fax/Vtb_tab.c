/*
 * Vtb_tab.c -- the V.17 trellis constellations, the `VTBv17_*` map tables.
 *
 * The blob's FILE records name `Vtb_tab.c` between `Vmi_v29.c` and
 * `cDATArx.c`, and these eight arrays sit at .rodata 0x00b580..0x00ba21, the
 * slot between `V29txtab.c`'s last table (0x00b570..) and `cHDLCrx.c`'s
 * `GAIN_THRESHOLD_TABLE` (0x00ba22), with `Vmi_v17..29.c` and `cDATA*.c`
 * contributing no .rodata.  The name matches VTB directly; the reference set
 * (V17rx.c, V17t_int.c) does not separate those two, but it is the only
 * candidate in that slot.  Moved verbatim out of `V17rxtab.c` (finding
 * F11391).
 */

#include "dsplib/v17cfg.h"

/*
 * THE FOUR TRELLIS CONSTELLATIONS, and they are V.32bis' own.
 *
 * `V17RX_create` reads the rate selector it derived from 7200/9600/12000/
 * 14400 baud and installs one (imap, qmap, bound, region) set into the shared
 * `struct vtb`, with `nsub` 1, 2, 3 or 4, `grid = 2 * nsub` and
 * `mask = (1 << (nsub + 2)) - 1`.  That is exactly `VTBv32_init`'s own
 * arithmetic, the `bound` and `region` tables ARE V.32's -- `VTB_BOUND_7200`
 * and friends, already written in `src/pump/v32/v32vtb_tables.c` -- and each
 * map's first N entries are byte-identical to `VTBv32_?MAP*`'s N.  So V.17's
 * trellis is V.32bis' trellis, which is what the Recommendation says, and
 * `t_v17cfg.c` asserts the identity table by table rather than trusting it.
 *
 * WHY N+1 AND NOT N.  `nm -S` gives these eight symbols 2*N+2 bytes where
 * V.32's give 2*N, and the extra short is zero in all eight.  The consumer
 * cannot reach it: `VTB_decoder` indexes `imap[]` and `qmap[]` by a point
 * index taken from `bound[]`, and `nsub` bounds that at N-1.  So the count
 * that the DECODER fixes is N, and the count the SYMBOL fixes is N+1; the
 * declaration has to be N+1 or our array is two bytes short of the object's,
 * which no value comparison run over our own length could ever notice.  That
 * is layer 1 of the test's whole reason for existing.
 */
const short VTBv17_QMAP128[129] = {
	 -6144,  -6144,  -6144, -14336,  -6144, -14336,  -6144, -14336,
	  2048,   2048,   2048,  10240,   2048,  10240,   2048,  10240,
	  6144,   6144,   6144,  14336,   6144,  14336,   6144,  14336,
	 -2048,  -2048,  -2048, -10240,  -2048, -10240,  -2048, -10240,
	-18432,  14336,   6144,   6144, -10240, -10240,  -2048,  -2048,
	-18432,  14336,   6144,   6144, -10240, -10240,  -2048,  -2048,
	 18432, -14336,  -6144,  -6144,  10240,  10240,   2048,   2048,
	 18432, -14336,  -6144,  -6144,  10240,  10240,   2048,   2048,
	  4096,   4096,   4096,  12288,   4096,  12288,   4096,  12288,
	 -4096,  -4096,  -4096, -12288,  -4096, -12288,  -4096, -12288,
	 -4096,  -4096,  -4096, -12288,  -4096, -12288,  -4096, -12288,
	  4096,   4096,   4096,  12288,   4096,  12288,   4096,  12288,
	 16384, -16384,  -8192,  -8192,   8192,   8192,      0,      0,
	 16384, -16384,  -8192,  -8192,   8192,   8192,      0,      0,
	-16384,  16384,   8192,   8192,  -8192,  -8192,      0,      0,
	-16384,  16384,   8192,   8192,  -8192,  -8192,      0,      0,
	     0,
};

const short VTBv17_IMAP128[129] = {
	-16384,  16384,   8192,   8192,  -8192,  -8192,      0,      0,
	-16384,  16384,   8192,   8192,  -8192,  -8192,      0,      0,
	 16384, -16384,  -8192,  -8192,   8192,   8192,      0,      0,
	 16384, -16384,  -8192,  -8192,   8192,   8192,      0,      0,
	  4096,   4096,   4096,  12288,   4096,  12288,   4096,  12288,
	 -4096,  -4096,  -4096, -12288,  -4096, -12288,  -4096, -12288,
	 -4096,  -4096,  -4096, -12288,  -4096, -12288,  -4096, -12288,
	  4096,   4096,   4096,  12288,   4096,  12288,   4096,  12288,
	 18432, -14336,  -6144,  -6144,  10240,  10240,   2048,   2048,
	 18432, -14336,  -6144,  -6144,  10240,  10240,   2048,   2048,
	-18432,  14336,   6144,   6144, -10240, -10240,  -2048,  -2048,
	-18432,  14336,   6144,   6144, -10240, -10240,  -2048,  -2048,
	 -6144,  -6144,  -6144, -14336,  -6144, -14336,  -6144, -14336,
	  2048,   2048,   2048,  10240,   2048,  10240,   2048,  10240,
	  6144,   6144,   6144,  14336,   6144,  14336,   6144,  14336,
	 -2048,  -2048,  -2048, -10240,  -2048, -10240,  -2048, -10240,
	     0,
};

const short VTBv17_QMAP64[65] = {
	  2048,  10240, -14336,  10240,  -6144,   2048, -14336,  -6144,
	 -2048, -10240,  14336, -10240,   6144,  -2048,  14336,   6144,
	 10240,   2048,  10240, -14336,   2048,  -6144,  -6144, -14336,
	-10240,  -2048, -10240,  14336,  -2048,   6144,   6144,  14336,
	 -2048, -10240,  14336, -10240,   6144,  -2048,  14336,   6144,
	  2048,  10240, -14336,  10240,  -6144,   2048, -14336,  -6144,
	-14336,  -6144, -14336,  10240,  -6144,   2048,   2048,  10240,
	 14336,   6144,  14336, -10240,   6144,  -2048,  -2048, -10240,
	     0,
};

const short VTBv17_IMAP64[65] = {
	 14336,   6144,  14336, -10240,   6144,  -2048,  -2048, -10240,
	-14336,  -6144, -14336,  10240,  -6144,   2048,   2048,  10240,
	 -2048, -10240,  14336, -10240,   6144,  -2048,  14336,   6144,
	  2048,  10240, -14336,  10240,  -6144,   2048, -14336,  -6144,
	-10240,  -2048, -10240,  14336,  -2048,   6144,   6144,  14336,
	 10240,   2048,  10240, -14336,   2048,  -6144,  -6144, -14336,
	  2048,  10240, -14336,  10240,  -6144,   2048, -14336,  -6144,
	 -2048, -10240,  14336, -10240,   6144,  -2048,  14336,   6144,
	     0,
};

const short VTBv17_QMAP32[33] = {
	  4096, -12288,   4096,   4096,  -4096,  12288,  -4096,  -4096,
	 12288,  -4096,  12288,  -4096, -12288,   4096, -12288,   4096,
	 -8192,  -8192,   8192,   8192,   8192,   8192,  -8192,  -8192,
	 16384,      0,      0, -16384, -16384,      0,      0,  16384,
	     0,
};

const short VTBv17_IMAP32[33] = {
	-16384,      0,      0,  16384,  16384,      0,      0, -16384,
	 -8192,  -8192,   8192,   8192,   8192,   8192,  -8192,  -8192,
	-12288,   4096, -12288,   4096,  12288,  -4096,  12288,  -4096,
	  4096, -12288,   4096,   4096,  -4096,  12288,  -4096,  -4096,
	     0,
};

const short VTBv17_QMAP16T[17] = {
	-12288,   4096,  12288,  -4096,   4096, -12288,  -4096,  12288,
	 12288,  -4096, -12288,   4096, -12288,   4096,  12288,  -4096,
	     0,
};

const short VTBv17_IMAP16T[17] = {
	 12288,  -4096, -12288,   4096,  12288,  -4096, -12288,   4096,
	 -4096,  12288,   4096, -12288, -12288,   4096,  12288,  -4096,
	     0,
};

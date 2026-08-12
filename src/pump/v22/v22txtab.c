/*
 * v22txtab.c -- V.22/V.22bis transmit tables: scrambler and symbol coder.
 *
 * Reconstructed from dsplibs.o v22txtab.c, all of it .rodata:
 *   SMCv22_CFG           0x008d20   44 bytes   THREE POINTERS, see below
 *   SMCv22_QMAP_1200BPS  0x008d60   32 bytes
 *   SMCv22_IMAP_1200BPS  0x008d80   32 bytes
 *   SMCv22_QMAP_2400BPS  0x008da0   32 bytes
 *   SMCv22_IMAP_2400BPS  0x008dc0   32 bytes
 *   SMCv22_PMAP          0x008de0    8 bytes
 *   SDMv22_CFG           0x008de8    6 bytes
 *
 * ---------------------------------------------------------------------------
 * The constellation
 *
 * Every coordinate here is 8192 or 24576, positive or negative -- 1 and 3 in
 * units of 8192, which is ITU-T V.22bis Table 2 verbatim with the odd
 * coordinates scaled by 8192.  Reading the maps as (I, Q) pairs:
 *
 *   2400 bit/s, index 0..3   (1,1) (3,1) (1,3) (3,3)     first quadrant
 *              index 4..7    (-1,1) (-1,3) (-3,1) (-3,3)   each group the
 *              index 8..11   (-1,-1) (-3,-1) (-1,-3) (-3,-3)  previous one
 *              index 12..15  (1,-1) (1,-3) (3,-1) (3,-3)    turned 90 deg
 *
 * so bits 2..3 of the index are the quadrant and bits 0..1 choose within it.
 * At 1200 bit/s the amplitude bits are masked off (SMCv22_CFG.amask is 0), so
 * only indices 0, 4, 8 and 12 are ever formed -- and the 1200 maps carry each
 * quadrant's single point (3,1) replicated four times, which is V.22's
 * four-point set and the same 90-degree rotation.  The replication is what
 * lets one table serve both an index that steps in 4s and one that does not.
 *
 * SMCv22_PMAP is V.22bis Table 1, the dibit-to-quadrant-change map, in the
 * same units: 4 is 90 degrees.
 *
 *   dibit 00 -> +90    01 -> 0    10 -> +180    11 -> +270
 *
 * The derivation of the scale factor is deferred with every other coefficient
 * derivation; the bytes are proved byte-exact by t_fpm_smc.
 *
 * ---------------------------------------------------------------------------
 * What is not here
 *
 * Nothing selects the 2400 bit/s maps from this file.  SMCv22_CFG's `imap`
 * and `qmap` name the 1200 pair and no relocation anywhere points the config
 * at the 2400 pair; the rate switch happens at run time in SetTxRate, which
 * writes the map pointers into the PULSE SHAPING FILTER's object and not into
 * the symbol coder's.  Finding 1522 has the offsets.
 *
 * SDMv22_CFG likewise carries `nbits` for 2400 bit/s (4) and no second copy
 * for 1200: V22FP_create copies this struct onto the stack and patches
 * `nbits` to 2 or 4 before calling FPM_SDM_init.  The taps are the only part
 * of it that never varies.
 */

#include "dsplib/v22txtab.h"

/*
 * MIXED STRUCT: `pmap`, `imap` and `qmap` are R_386_32 relocations into
 * .rodata.  Dumped as int16 they read 0, 0, 0, 0, 0, 0 and look like six
 * unused fields.
 *
 * Everything the encoder uses is set for 1200 bit/s: no shift, the quadrant
 * dibit in bits 0..1, no amplitude bits.  SetTxRate patches `qshift` to 2 and
 * `amask` to 3 for 2400.
 */
const struct fpm_smc_cfg SMCv22_CFG = {
	.f00 = 1,		/* not read by fpm_smc                      */
	.direct = 0,		/* V.22bis is differentially encoded        */
	.rot_step = 0,		/* no carrier rotation folded in            */
	.rot_mod = 16,		/* ... but the sum still wraps at 16        */
	.qshift = 0,		/* 1200 bit/s: the dibit is already at bit 0 */
	.qmask = 3,
	.amask = 0,		/* 1200 bit/s: no amplitude bits            */
	.pmask = 15,		/* four quadrants, four points each         */
	.pmap = SMCv22_PMAP,
	.imap = SMCv22_IMAP_1200BPS,
	.qmap = SMCv22_QMAP_1200BPS
	/* f20, f24 and f28 are zero */
};

/* V.22 1200 bit/s: (3,1) and its three 90-degree rotations, each replicated
 * across its quadrant's four index slots. */
const short SMCv22_QMAP_1200BPS[16] = {
	 8192,  8192,  8192,  8192,
	24576, 24576, 24576, 24576,
	-8192, -8192, -8192, -8192,
	-24576, -24576, -24576, -24576
};

const short SMCv22_IMAP_1200BPS[16] = {
	24576, 24576, 24576, 24576,
	-8192, -8192, -8192, -8192,
	-24576, -24576, -24576, -24576,
	 8192,  8192,  8192,  8192
};

/* V.22bis 2400 bit/s: the full 16 points of Table 2. */
const short SMCv22_QMAP_2400BPS[16] = {
	 8192,  8192, 24576, 24576,
	 8192, 24576,  8192, 24576,
	-8192, -8192, -24576, -24576,
	-8192, -24576, -8192, -24576
};

const short SMCv22_IMAP_2400BPS[16] = {
	 8192, 24576,  8192, 24576,
	-8192, -8192, -24576, -24576,
	-8192, -24576, -8192, -24576,
	 8192,  8192, 24576, 24576
};

/* V.22bis Table 1, in quarter-quadrant units: 4 == 90 degrees. */
const unsigned short SMCv22_PMAP[4] = {
	4, 0, 8, 12
};

/*
 * The V.22bis calling-modem scrambler, 1 + x^-14 + x^-17 (ITU-T V.22bis
 * section 2.5).  `nbits` is the 2400 bit/s value; V22FP_create overwrites it
 * with 2 for a 1200 bit/s connection.
 */
const struct fpm_sdm_cfg SDMv22_CFG = {
	4,			/* nbits: 4 bits per symbol at 2400 bit/s   */
	14,			/* tap1                                     */
	17			/* tap2                                     */
};

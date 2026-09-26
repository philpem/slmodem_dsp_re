/*
 * v17.c -- ITU-T V.17 (fax): the receiver's primitives, and the transmit-side
 *          setters that sit beside them.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V17RX_create      .text 0x096eb0 3201
 *   V17RX_delete      .text 0x097b40  251
 *   V17TX_create      .text 0x0989e0 1043
 *   V17TX_delete      .text 0x098e00  107
 *   SMCv17_encoder_dif .text 0x09fcc0  164
 *   SMCv17_encoder_abs .text 0x09fd70  135
 *   SMCv17_encoder_tcm .text 0x09fe00  374
 *   V17RX_modem       .text 0x09ff80  127
 *   RxHdxDataV17      .text 0x0a0000  226
 *   RxHdxErrorV17     .text 0x0a00f0   59
 *   RxNextStateV17    .text 0x0a0130  730
 *   RxHdxIdleV17      .text 0x0a0410  132
 *   RxHdxScramV17     .text 0x0a04a0  266
 *   RxHdxBridgeV17    .text 0x0a05b0  210
 *   RxHdxPrtcolV17    .text 0x0a0690  210
 *   RxHdxEpochDetV17  .text 0x0a0770  156
 *   RxHdxStartV17     .text 0x0a0810  105
 *   V17RX_control     .text 0x0a0880  131
 *   V17RX_status      .text 0x0a0910  190
 *   ScrambleDataV17   .text 0x0a09d0   28
 *   SeedScramblerV17  .text 0x0a09f0   15
 *   SetEncoderV17     .text 0x0a0a00   90
 *   SMCv17_init       .text 0x0a0a60   87
 *   SetTxModeV17      .text 0x0a0ac0  625
 *   V17TX_modem       .text 0x0a0e40  182
 *   TxNextStateV17    .text 0x0a0f00 1439
 *   TxHdxIdleV17      .text 0x0a14a0  116
 *   TxHdxDataV17      .text 0x0a1520  349
 *   TxHdxSCR1V17      .text 0x0a1680  206
 *   TxHdxBridgeV17    .text 0x0a1750  206
 *   TxHdxEQCondV17    .text 0x0a1820  206
 *   TxHdxABV17        .text 0x0a18f0  190
 *   TxHdxTEP_V17      .text 0x0a19b0  179
 *   TxHdxSilenceV17   .text 0x0a1a70  158
 *   TxHdxStartV17     .text 0x0a1b10   21
 *   V17TX_status      .text 0x0a1bd0  106
 *   DemodDataV17      .text 0x0a50a0  415
 *   DescrambleDataV17 .text 0x0a5240   30
 *   CarrierDetectV17      .text 0x0a5260  121
 *   DataCarrierDetectV17  .text 0x0a52e0  625
 *   QualityDetectV17      .text 0x0a5560  266
 *   EpochDetectV17    .text 0x0a5670   22
 *   GetSNRV17         .text 0x0a5690   23
 *   StoreCoefV17      .text 0x0a56b0   81
 *   Restore_rateV17   .text 0x0a5710   37
 *
 * `include/dsplib/v17fax.h` carries the offset evidence and the naming.
 *
 * ---------------------------------------------------------------------------
 * THIS IS NOT ONE TRANSLATION UNIT, AND THE ADDRESSES SAY SO
 *
 * `tools/tumap.py` brackets 95 units together as `class1tx.c +94`, so it
 * cannot separate them -- but the symbols above span 0x09ff80 to 0x0a5735,
 * about 22 KB, with hundreds of unrelated functions between them.  GCC emits
 * one unit's functions contiguously, so at least three units are represented
 * here.  The definitions are ORDERED BY THE OBJECT'S OWN ADDRESSES anyway,
 * because emission order is a register-allocation carrier (CLAUDE.md, finding
 * F7796) and the object's order is the only one that is evidence.
 *
 * ---------------------------------------------------------------------------
 * THE SAME FIELD IS NOT ALWAYS THE SAME WIDTH, AND EACH SITE FOLLOWS THE OBJECT
 *
 * `CarrierDetectV17` loads receiver state + 0xd0 with a 32-bit `mov`;
 * `QualityDetectV17` loads it with `movswl`.  Neither instruction was free --
 * a `short` cannot produce the first and an `int` cannot produce the second --
 * so the two functions did not share a declaration and this file does not
 * make them share one.  The readings differ whenever the short at +0xd2 is
 * non-zero, and `t_v17fax.c` runs that case on purpose.  Finding F8853.
 *
 * The field itself is `struct fpm_agc::signal`, which the four FPM objects'
 * exact tiling of the state block identifies -- see `V17RXS_AGC_SIGNAL` in
 * v17fax.h and finding F8854.  It only ever holds 0 or 1, so on any state a
 * real receiver can reach the two readings agree; the width is followed
 * because the object was not free to choose it, not because it is reachable.
 *
 * ---------------------------------------------------------------------------
 * THE RELOADS ARE FORCED, SO THEY ARE WRITTEN AS RELOADS
 *
 * Every function below that calls anything re-reads `V17RX_OBJ_CTL` or
 * `V17RX_OBJ_STATE` after the call rather than keeping it in a register.  That
 * is not a style: a call clobbers memory the compiler cannot see through, so a
 * source that read the field once could not have produced it.  The `CTL()` and
 * `RXS()` macros therefore expand at each use, and the object's reload pattern
 * comes out of the C rather than being imitated.  `src/fax/v29.c` records the
 * same thing for the same reason.
 */

#include <string.h>

#include "dsplib/v17fax.h"

#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/sysdep.h"
#include "dsplib/v17cfg.h"
#include "dsplib/v17dec.h"
#include "dsplib/v32smc.h"	/* TrellisEncodeDifTable, TrellisTransitionTable:
				 * SMCv17_encoder_tcm reuses V.32's trellis
				 * coder tables, see v17data.h              */
#include "dsplib/vtb.h"

#define RXROOT(modem)		((struct v17rx *)(modem))
#define TXROOT(modem)		((struct v17tx *)(modem))
#define RXCTL(modem)		(RXROOT(modem)->ctl)
#define RXSTATE(modem)		(RXROOT(modem)->state)
#define TXPRIV(modem)		(TXROOT(modem)->priv)
#define TXBLOCK(modem)		(TXROOT(modem)->fp)
#define CTL(modem)		RXCTL(modem)
#define RXS(modem)		RXSTATE(modem)
#define TXP(modem)		TXPRIV(modem)
#define TXFP(modem)		TXBLOCK(modem)

/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */

/*
 * The transmit configuration `V17TX_create` copies onto the handle's first
 * 0x20 bytes when the caller passes no config of its own.  See
 * `struct v17tx_cfg` in v17fax.h for the field-by-field derivation.
 */
struct v17tx_cfg V17TX_CFG = {
	0,		/* protocol                                          */
	14400,		/* bitrate                                           */
	0,		/* short_0004                                        */
	0,		/* short_0006                                        */
	60000,		/* int_0008                                          */
	1,		/* int_000c                                          */
	0,		/* int_0010                                          */
	1,		/* fifo_size_factor -- V17TX_create's own transmit FIFO
			   size is fifo_size_factor * 3 * 16                */
	0,		/* int_0018 -- V17TX_create's own V17TXP_INT_000C     */
	0,		/* int_001c -- V17TX_create's own FPM_PPS_CFG.aux     */
};



/* --------------------------------------------------------------------- */

/*
 * The trellis coder's three tables.  Bytes taken straight from the object;
 * widths from the loads (`movzwl`, scale 2 -- unsigned short, forced).
 *
 * `SMCv17_MOD` is bytewise identical to `SMCv32_MOD` (`v32smc.c`): the same
 * eight rotation words, `0x6170 0x7061 0x5342 0x4253 0x2534 0x3425 0x1706
 * 0x0617`, confirmed by reading `.rodata` at both addresses rather than by
 * assuming the reuse.  `TrellisEncodeDifTable` and `TrellisTransitionTable`
 * (indexed by `SMCv17_encoder_tcm` below) are the SAME symbols V.32's coder
 * already defines in `v32smc.c` -- one trellis coder, two protocols.
 */
const unsigned short SMCv17_PMAP4[4] = { 1, 0, 2, 3 };
const unsigned short SMCv17_ABS4[4]  = { 0, 1, 3, 2 };
const unsigned short SMCv17_MOD[8] = {
	0x6170, 0x7061, 0x5342, 0x4253,
	0x2534, 0x3425, 0x1706, 0x0617
};

/*
 * `V17TX_create`'s pulse-shaper setup tables (see v17data.h for the read
 * sites; `V17TX_create` itself is not reconstructed).  The two constellation
 * maps, `SMCv17_IMAP4`/`SMCv17_QMAP4`, are five signed shorts each, bytes
 * `00 10 00 30 00 f0 00 d0 00 00` / `00 30 00 f0 00 d0 00 10 00 00`.
 * `V17TX_PPS_SCALE` is four `int`s (a 32-bit `imul`, scale 4, is what forces
 * the width), bytes `78 69 00 00 78 69 00 00 a0 5f 00 00 a0 5f 00 00`.
 */
const short SMCv17_IMAP4[5] = { 0x3000, -0x1000, -0x3000, 0x1000, 0 };
const short SMCv17_QMAP4[5] = { 0x1000, 0x3000, -0x1000, -0x3000, 0 };
const int V17TX_PPS_SCALE[4] = { 27000, 27000, 24480, 24480 };

/*
 * `TxNextStateV17`'s scrambler-pattern table -- see v17data.h for why it is
 * here rather than with V.17's transmit half-duplex machine (F9600), which
 * this batch does not otherwise touch.
 */
const short V17TX_PATTERN_SCR1[4] = { 7, 15, 31, 63 };

/*
 * `V17TX_create`'s two shaper coefficient arrays, `coeff_i`/`coeff_q` in the
 * local `struct fpm_pps_cfg` it builds from `FPM_PPS_CFG` (fpm_pps.h).
 * Bytes taken straight from `.rodata` (0x9f40, 0x9e40); `t_v17ppstab.c`
 * checks the quadrature symmetry (I even, Q odd) V.32's own pair has, and
 * that this pair has it too.
 */
const short PPSv17_ICOFFS[120] = {
	    -8,     -9,     -6,      1,      6,      2,    -12,    -28,
	   -33,    -17,     20,     59,     75,     55,     10,    -25,
	    -9,     72,    191,    282,    288,    197,     67,     -3,
	    65,    267,    496,    601,    482,    174,   -151,   -284,
	  -113,    269,    597,    582,    114,   -633,  -1266,  -1409,
	  -965,   -238,    206,   -116,  -1258,  -2718,  -3681,  -3507,
	 -2206,   -567,    203,   -805,  -3524,  -6696,  -8326,  -6665,
	 -1214,   6781,  14693,  19599,  19599,  14693,   6781,  -1214,
	 -6665,  -8326,  -6696,  -3524,   -805,    203,   -567,  -2206,
	 -3507,  -3681,  -2718,  -1258,   -116,    206,   -238,   -965,
	 -1409,  -1266,   -633,    114,    582,    597,    269,   -113,
	  -284,   -151,    174,    482,    601,    496,    267,     65,
	    -3,     67,    197,    288,    282,    191,     72,     -9,
	   -25,     10,     55,     75,     59,     20,    -17,    -33,
	   -28,    -12,      2,      6,      1,     -6,     -9,     -8
};

const short PPSv17_QCOFFS[120] = {
	    -2,     -8,    -13,    -15,     -9,     -1,      1,    -12,
	   -39,    -69,    -83,    -69,    -31,      4,      6,    -41,
	  -117,   -174,   -163,    -68,     69,    168,    161,     43,
	  -107,   -164,    -39,    249,    564,    727,    630,    332,
	    47,     21,    366,    951,   1449,   1529,   1082,    338,
	  -232,   -203,    498,   1477,   2052,   1666,    290,  -1453,
	 -2583,  -2361,   -847,    942,   1460,   -527,  -5102, -10876,
	-15427, -16372, -12549,  -4705,   4705,  12549,  16372,  15427,
	 10876,   5102,    527,  -1460,   -942,    847,   2361,   2583,
	  1453,   -290,  -1666,  -2052,  -1477,   -498,    203,    232,
	  -338,  -1082,  -1529,  -1449,   -951,   -366,    -21,    -47,
	  -332,   -630,   -727,   -564,   -249,     39,    164,    107,
	   -43,   -161,   -168,    -69,     68,    163,    174,    117,
	    41,     -6,     -4,     31,     69,     83,     69,     39,
	    12,     -1,      1,      9,     15,     13,      8,      2
};









/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */



/* --------------------------------------------------------------------- */

/*
 * `SetTxModeV17`'s two `.rodata` tables.  See `v17data.h` for the evidence;
 * the bytes are the object's, `03 00 04 00 05 00 06 00` and `00 00 01 00`.
 */
const short V17TX_SYM_SIZE[4] = { 3, 4, 5, 6 };
const short SMCv17_CFG[2] = { 0, 1 };


/* --------------------------------------------------------------------- */









/* See TxHdxEQCondV17's own comment: the same body, a different symbol. */




/* --------------------------------------------------------------------- */

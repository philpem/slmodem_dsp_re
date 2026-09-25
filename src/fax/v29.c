/*
 * v29.c -- ITU-T V.29 (fax): the receiver's entry points, and the two
 *          transmitter accessors that sit in the same run of addresses.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V29RX_create          .text 0x09ad40 2127
 *   V29RX_delete          .text 0x09b590  220
 *   V29RX_epoch_det       .text 0x09b670  413
 *   V29RX_eq_train        .text 0x09b810  221
 *   V29RX_decision        .text 0x09b8f0  260
 *   V29TX_delete          .text 0x09be90  135
 *   V29RX_modem           .text 0x0a3f50  127
 *   RxHdxDataV29          .text 0x0a3fd0  226
 *   RxHdxErrorV29         .text 0x0a40c0   59
 *   RxNextStateV29        .text 0x0a4100  467
 *   RxHdxIdleV29          .text 0x0a42e0  132
 *   RxHdxPrtcolV29        .text 0x0a4370  244
 *   RxHdxEpochDetV29      .text 0x0a4470  156
 *   RxHdxStartV29         .text 0x0a4510  105
 *   V29RX_status          .text 0x0a45f0  190
 *   V29TX_modem           .text 0x0a46b0  182
 *   V29TX_status          .text 0x0a5030  100
 *   DemodDataV29          .text 0x0a5ff0  398
 *   DescrambleDataV29     .text 0x0a6180   30
 *   CarrierDetectV29      .text 0x0a61a0   22
 *   DataCarrierDetectV29  .text 0x0a61c0  579
 *   QualityDetectV29      .text 0x0a6410  266
 *   EpochDetectV29        .text 0x0a6520   22
 *   GetSNRV29             .text 0x0a6540   23
 *   ScrambleDataV29       .text 0x0a6560   28
 *   SeedScramblerV29      .text 0x0a6580   15
 *   SetEncoderV29         .text 0x0a6590   43
 *   ModDataV29            .text 0x0a65c0   89
 *
 * `include/dsplib/v29fax.h` carries the offset evidence; this file carries
 * the reasoning that is about the CODE.
 *
 * ---------------------------------------------------------------------------
 * THE ORDER OF DEFINITIONS IS THE OBJECT'S, AND IT IS A GUESS ABOUT ONE
 * TRANSLATION UNIT AND NOT A CLAIM ABOUT ELEVEN
 *
 * `tools/tumap.py` brackets these among 95 translation units it cannot
 * separate, so nothing establishes that they were one file.  A RUN of them IS
 * contiguous in the object -- 0x0a5ff0 through 0x0a6618 with no
 * foreign symbol between them, `ModDataV29` now closing that run -- and the
 * others are not: `V29RX_delete` and `V29TX_delete` sit 36 KB earlier, the
 * three half-duplex symbols 8 KB earlier, and the two status fillers and
 * `V29TX_modem` in between.  They are kept together here because they are one
 * layer, and written in ascending address order because emission order is a
 * register-allocation carrier (CLAUDE.md's lever, finding F7796) and the
 * object's own order is the only ordering with any evidence behind it.
 *
 * ---------------------------------------------------------------------------
 * THE AGC'S RETURN VALUE, WHICH IS NOT ONE
 *
 * `DemodDataV29` calls `FPM_AGC_agc` and then uses `%eax`.  `FPM_AGC_agc` is
 * `void` -- measured, not assumed: it takes three arguments (the object reads
 * 0x50, 0x54 and 0x58 of its frame and never 0x5c) and `include/dsplib/
 * fpm_agc.h` declares it that way.  So the calling translation unit declared
 * it as returning `int` while the defining one returned nothing, and what
 * `%eax` actually holds is whatever the definition left there.
 *
 * WHAT IT LEAVES THERE IS `agc->signal`, and that is a property of the object
 * rather than of C: `FPM_AGC_agc` has exactly one `ret`, every path funnels
 * through the same epilogue, and the two instructions before it are
 * `movzbl %dl,%eax` / `mov %eax,0x1c(%edi)` -- the store to `signal` itself.
 *
 * So this file reads the field.  It cannot spell what the object spells,
 * because `fpm_agc.h` is right and a second declaration disagreeing with it
 * would be the "one type, one home" failure in its function-prototype form;
 * and it does not need to, because the two are the same value on every path.
 * `t_v29fax.c` MEASURES that rather than believing it -- it declares
 * `ref_FPM_AGC_agc` as returning `int` and asserts the return equals
 * `agc.signal` over every trial, so if a future blob ever broke the identity
 * the test would say so.  Finding F8875, deviation D1035.
 *
 * ---------------------------------------------------------------------------
 * THE RELOADS ARE FORCED, SO THEY ARE WRITTEN AS RELOADS
 *
 * Every one of these functions re-reads `modem + 0x4c` or `modem + 0x50` after
 * each call rather than keeping it in a register.  That is not a style: a call
 * clobbers memory the compiler cannot see through, so a source that reads the
 * field once could not have produced it.  The `RX()` and `DET()` macros below
 * therefore expand at each use, and the object's reload pattern comes out of
 * the C rather than being imitated.
 */

#include "dsplib/v29fax.h"

#include <stddef.h>
#include <string.h>

#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/smc.h"
#include "dsplib/sysdep.h"
#include "dsplib/v29cfg.h"
#include "dsplib/v29data.h"

/* Shared period owner layouts and the observed report prefix live in v29fax.h. */



/* The decoder block, `fse->cfg.owner`, and the receive block's rx + 0x28. */






/*
 * The transmit configuration and the tables `V29TX_create` builds its DSP
 * sub-objects from.  `protocol`/`bitrate`/`flags` are typed by
 * `V29TX_status`'s own reads (`V29TXS_PROTOCOL`/`_BITRATE`/`_FLAGS_10`,
 * v29fax.h); `int_0008` is 60000, `v21tx_cfg`'s own value at the identical
 * offset.  The rest are usage inference; see v29data.h.
 */
struct v29tx_cfg V29TX_CFG = {
	0,			/* protocol                                  */
	9600,			/* bitrate                                   */
	0,			/* short_0004                                */
	0,			/* short_0006                                */
	60000,			/* int_0008                                  */
	1,			/* int_000c                                  */
	0,			/* flags                                     */
	0,			/* short_0012                                */
	1,			/* fifo_size_factor -- V29TX_create's own
					transmit FIFO size is this * 48,
					`v17tx_cfg`'s own field, same name    */
	0,			/* int_0018 -- V29TX_create's own FPM_PPS_CFG
					aux, across the (void *)(long) idiom */
};

/* Indexed by V29TXP_RATE.  Fed to FPM_PPS_init's `scale` at 0x9bd25. */
int V29TX_PPS_SCALE[2] = { 45016, 25480 };

/* Indexed by V29TXP_RATE, read inside TxNextStateV29 itself at 0xa4937. */
short V29TX_PATTERN_SCR1[2] = { 7, 15 };

/* The transmit pulse shaper's I and Q coefficient tables, 120 shorts each,
 * fed to FPM_PPS_init at 0x9bd57. */
const short V29TX_PPS_IFILT[120] = {
	   -22,   -192,   -455,   -670,   -707,   -522,   -190,    125,
	   258,    137,   -162,   -453,   -529,   -287,    207,    738,
	  1051,    997,    622,    164,    -75,    110,    690,   1406,
	  1887,   1859,   1304,    499,   -123,   -201,    333,   1191,
	  1856,   1858,   1046,   -291,  -1545,  -2104,  -1699,   -598,
	   487,    759,   -222,  -2234,  -4436,  -5760,  -5483,  -3691,
	 -1359,     51,   -675,  -3781,  -8197, -11812, -12306,  -8208,
	   298,  11207,  21310,  27371,  27371,  21310,  11207,    298,
	 -8208, -12306, -11812,  -8197,  -3781,   -675,     51,  -1359,
	 -3691,  -5483,  -5760,  -4436,  -2234,   -222,    759,    487,
	  -598,  -1699,  -2104,  -1545,   -291,   1046,   1858,   1856,
	  1191,    333,   -201,   -123,    499,   1304,   1859,   1887,
	  1406,    690,    110,    -75,    164,    622,    997,   1051,
	   738,    207,   -287,   -529,   -453,   -162,    137,    258,
	   125,   -190,   -522,   -707,   -670,   -455,   -192,    -22,
};

const short V29TX_PPS_QFILT[120] = {
	    99,    244,    224,      9,   -326,   -628,   -747,   -628,
	  -345,    -72,      6,   -194,   -603,  -1017,  -1206,  -1044,
	  -589,    -65,    248,    177,   -240,   -756,  -1033,   -836,
	  -173,    686,   1339,   1469,   1036,    318,   -210,   -141,
	   630,   1810,   2836,   3167,   2597,   1406,    246,   -186,
	   451,   1901,   3389,   3989,   3135,    990,  -1546,  -3237,
	 -3164,  -1305,   1283,   2823,   1630,  -3000, -10235, -17804,
	-22775, -22725, -16800,  -6193,   6193,  16800,  22725,  22775,
	 17804,  10235,   3000,  -1630,  -2823,  -1283,   1305,   3164,
	  3237,   1546,   -990,  -3135,  -3989,  -3389,  -1901,   -451,
	   186,   -246,  -1406,  -2597,  -3167,  -2836,  -1810,   -630,
	   141,    210,   -318,  -1036,  -1469,  -1339,   -686,    173,
	   836,   1033,    756,    240,   -177,   -248,     65,    589,
	  1044,   1206,   1017,    603,    194,     -6,     72,    345,
	   628,    747,    628,    326,     -9,   -224,   -244,    -99,
};

/*
 * The symbol coder's carrier phasor (24 steps, 1700 Hz at 2400 baud with
 * `rot_step` = 0x11) and its constellation maps, fed to SMC_init at
 * 0x9bcba.  smc.h names cosine/sine's slots from this function's own
 * relocations.
 */
const short V29TX_SMC_COSINE[24] = {
	32767,  31651,  28378,  23170,  16384,   8481,      0,  -8481,
       -16384, -23170, -28378, -31651, -32767, -31651, -28378, -23170,
       -16384,  -8481,      0,   8481,  16384,  23170,  28378,  31651,
};

const short V29TX_SMC_SINE[24] = {
	    0,   8481,  16384,  23170,  28378,  31651,  32767,  31651,
	28378,  23170,  16384,   8481,      0,  -8481, -16384, -23170,
       -28378, -31651, -32767, -31651, -28378, -23170, -16384,  -8481,
};

const short V29TX_SMC_IMAP[16] = {
	 6144,  2048,     0, -2048, -6144, -2048,     0,  2048,
	10240,  6144,     0, -6144,-10240, -6144,     0,  6144,
};

const short V29TX_SMC_QMAP[16] = {
	    0,  2048,  6144,  2048,     0, -2048, -6144, -2048,
	    0,  6144, 10240,  6144,     0, -6144,-10240, -6144,
};

/* dibit -> quadrant increment; struct fpm_smc_cfg::pmap's own type. */
const unsigned short V29TX_SMC_PMAP[8] = { 1, 0, 2, 3, 6, 7, 5, 4 };



































/*
 * The offsets this file states independently, checked against the one struct
 * it borrows.  `__SIZEOF_POINTER__` is a GCC 4.6+ predefine, so under the
 * period compiler this reads `#if 0` and the assertions vanish -- see
 * docs/method/compilers.md; that is the tree's established idiom and not an
 * oversight here.
 */
#if __SIZEOF_POINTER__ == 4
typedef char v29fax_agc_signal[
	(V29RX_AGC + (int)offsetof(struct fpm_agc, signal)
	 == V29RX_AGC_SIGNAL) ? 1 : -1];
typedef char v29fax_sre_active[
	(V29RX_SRE + (int)offsetof(struct fpm_sre, active)
	 == V29RX_SRE_ACTIVE) ? 1 : -1];
typedef char v29fax_sre_adapt[
	(V29RX_SRE + (int)offsetof(struct fpm_sre, adapt)
	 == V29RX_SRE_ADAPT) ? 1 : -1];
typedef char v29fax_fse_lms_force[
	(V29RX_FSE + (int)offsetof(struct fpm_fse, lms_force)
	 == V29RX_FSE_LMS_FORCE) ? 1 : -1];
typedef char v29fax_fse_pll_on[
	(V29RX_FSE + (int)offsetof(struct fpm_fse, pll_on)
	 == V29RX_FSE_PLL_ON) ? 1 : -1];
typedef char v29fax_fse_tilt_on[
	(V29RX_FSE + (int)offsetof(struct fpm_fse, tilt_on)
	 == V29RX_FSE_TILT_ON) ? 1 : -1];
typedef char v29fax_fse_lms_on[
	(V29RX_FSE + (int)offsetof(struct fpm_fse, lms_on)
	 == V29RX_FSE_LMS_ON) ? 1 : -1];
typedef char v29fax_fse_mse[
	(V29RX_FSE + (int)offsetof(struct fpm_fse, mse)
	 == V29RX_FSE_MSE) ? 1 : -1];
/*
 * And that the sub-objects do not overlap: the SRE ends where 0x120 begins
 * and the FSE ends below the two buffers.  Without this the eight readings
 * above would be arithmetic rather than layout.
 */
typedef char v29fax_sre_fits[
	(V29RX_SRE + (int)sizeof(struct fpm_sre) <= V29RX_FSE) ? 1 : -1];
typedef char v29fax_fse_fits[
	(V29RX_FSE + (int)sizeof(struct fpm_fse) <= V29RX_BUF_MRF) ? 1 : -1];
typedef char v29fax_agc_fits[
	(V29RX_AGC + (int)sizeof(struct fpm_agc) <= V29RX_SRE) ? 1 : -1];
typedef char v29fax_smc_direct[
	(V29FP_SMC + (int)offsetof(struct fpm_smc, cfg)
	 + (int)offsetof(struct fpm_smc_cfg, direct) == 0x38) ? 1 : -1];
typedef char v29fax_tx_fp[(V29_OBJ_TX == V29TX_OBJ_FP) ? 1 : -1];
#endif

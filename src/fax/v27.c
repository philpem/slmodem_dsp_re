/*
 * v27.c -- ITU-T V.27ter (fax): the receiver's primitives, and the
 * transmitter's status filler.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V27RX_create         .text 0x099660 2210
 *   V27RX_delete         .text 0x099f10  193
 *   V27RX_epoch_det      .text 0x099fe0  303
 *   V27TX_delete         .text 0x09a7c0  107
 *   V27RX_eq_train       .text 0x09a110  255
 *   V27RX_decision       .text 0x09a210  284
 *   V27RX_modem          .text 0x0a2c60  127
 *   RxHdxDataV27         .text 0x0a2ce0  226
 *   RxHdxErrorV27        .text 0x0a2dd0   59
 *   RxNextStateV27       .text 0x0a2e10  518
 *   RxHdxIdleV27         .text 0x0a3020  132
 *   RxHdxPrtcolV27       .text 0x0a30b0  224
 *   RxHdxEpochDetV27     .text 0x0a3190  156
 *   RxHdxStartV27        .text 0x0a3230  101
 *   V27RX_status         .text 0x0a3320   11
 *   V27TX_status         .text 0x0a3ed0  118
 *   DemodDataV27         .text 0x0a5950  331
 *   DescrambleDataV27    .text 0x0a5aa0   28
 *   CarrierDetectV27     .text 0x0a5ac0   22
 *   DataCarrierDetectV27 .text 0x0a5ae0  579
 *   QualityDetectV27     .text 0x0a5d30  266
 *   EpochDetectV27       .text 0x0a5e40   22
 *   GetSNRV27            .text 0x0a5e60    6
 *   ScrambleDataV27      .text 0x0a5e70   28
 *   ModDataV27           .text 0x0a5ef0   89
 *
 * `tools/tumap.py` brackets these across `class1tx.c +94` and `class1.c`, so
 * it cannot say which translation units they are; they are kept in one file
 * because they are one layer -- everything V.27ter's datapump wrapper reaches
 * that is not the state machine itself -- and not because a translation unit
 * has been established.  `include/dsplib/v27fax.h` carries the offset
 * evidence.
 *
 * ---------------------------------------------------------------------------
 * THE FIVE RECEIVE-MACHINE SYMBOLS ARE ONE INDIVISIBLE UNIT
 *
 * A relocation probe over the whole 1.2 MB (`objdump -r`, both `R_386_32` and
 * `R_386_PC32`) finds exactly twelve edges naming any of them:
 * `RxNextStateV27` STORES the addresses of `RxHdxEpochDetV27`,
 * `RxHdxPrtcolV27`, `RxHdxIdleV27` and `RxHdxDataV27` in its transition arms,
 * and each of `RxHdxStartV27`, `RxHdxIdleV27`, `RxHdxPrtcolV27` and
 * `RxHdxEpochDetV27` CALLS `RxNextStateV27` back.  That is a cycle, and under
 * findings F8492/F8493 a reference from `src/` to a symbol this tree has not
 * written is an undefined reference that fails every test binary -- for a
 * stored function pointer exactly as for a call.  So no proper subset of the
 * five links, and they were written together.
 *
 * `DemodDataV27` carries NO relocation against any `RxHdx*`, which is where
 * V.27ter differs from V.21: `DemodDataV21` does, so V.21's unit was five with
 * the demodulator inside it and V.27ter's is five with the demodulator
 * outside.
 *
 * The five are laid out below in the object's own address order, which is the
 * one lever this tree has on register allocation across a translation unit
 * (finding F7796).  It is not a claim that the author had them in one file.
 *
 * ---------------------------------------------------------------------------
 * THE AGC'S RETURN VALUE, WHICH IS NOT ONE
 *
 * `DemodDataV27` calls `FPM_AGC_agc`, passes it a FOURTH argument (the literal
 * 1) that it does not have, and then USES `%eax`.  `FPM_AGC_agc` is `void` --
 * `include/dsplib/fpm_agc.h` says so and the object's own frame reads confirm
 * it -- so the calling translation unit declared it as returning `int` while
 * the defining one returned nothing, and `%eax` holds whatever the definition
 * left there.
 *
 * WHAT IT LEAVES THERE IS `agc->signal`: the two instructions before its only
 * `ret` are `movzbl %dl,%eax` / `mov %eax,0x1c(%edi)`, the store to `signal`
 * itself.  So this file READS THE FIELD, which it can spell without a second
 * prototype disagreeing with `fpm_agc.h`, and `t_v27fax.c` MEASURES the
 * identity rather than believing it -- it declares `ref_FPM_AGC_agc` as
 * returning `int` and asserts the return equals `agc.signal` on every trial.
 * This is the fourth site with that shape; findings F8875 and F9116,
 * deviations D1035 and D1094.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE FOUR READING FUNCTIONS SHARE
 *
 * All of them start `rx = *(void **)(modem + 0x54)` and then read a field of
 * one of the four FPM modules embedded in that block.  Three flags account
 * for nearly every one of them:
 *
 *   fpm_agc::signal   more than half the last call's blocks were above the
 *                     gate.  `CarrierDetectV27`, `QualityDetectV27` and
 *                     `DataCarrierDetectV27` all AND it with
 *   fpm_sre::active   the symbol recovery's own squelch let the PLL run, and
 *   fpm_fse::mse      the equaliser's smoothed squared decision error, which
 *                     is what "Decoder error too big" is about.
 *
 * So "carrier" here means the level gate and the timing loop agree, and
 * "quality" means the equaliser is not struggling.  Neither is inferred from
 * a name: the fields are `fpm_agc.h`'s, `fpm_sre.h`'s and `fpm_fse.h`'s, and
 * the offsets land on them because the four modules tile the block with no
 * gap -- see the arithmetic in `v27fax.h`.
 *
 * ---------------------------------------------------------------------------
 * THE THREE `FPM_*_free` CALLS TAKE A SECOND ARGUMENT THEY DO NOT HAVE
 *
 * `V27RX_delete` stores the constant 1 at 0x4(%esp) before each of
 * `FPM_FSE_free`, `FPM_SRE_free` and `FPM_MRF_free`, and none of the three
 * reads it -- the same extra argument `v22data.c` and `bwchdem.c` record at
 * their `FPM_AGC_agc` sites.  cdecl makes it harmless and it is not
 * reproduced.  Finding F8870.
 */

#include <string.h>

#include "dsplib/v27fax.h"

#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/faxfifo.h"
#include "dsplib/sdmv27.h"
#include "dsplib/sgd.h"
#include "dsplib/smc.h"
#include "dsplib/sysdep.h"
#include "dsplib/v27cfg.h"

/* The instance is not modelled; see v27fax.h.  These are the only accessors. */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */





/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */




/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */


/* ------------------------------------------------------------------ */
/* The transmit half-duplex machine and its constructor.  F9751.       */

/*
 * The transmit configuration `V27TX_create` builds its DSP sub-objects from,
 * .data 0x007d60, 32 bytes.  Dumped from the object's own bytes, not typed by
 * any function: `bitrate` is 9600, which is neither 2400 nor 4800, so the
 * DEFAULT INSTANCE itself takes `V27TX_create`'s own default arm -- the same
 * shape `t_v29txcreate.c`'s "explicit, bitrate == 1200 (default arm)" case
 * exercises deliberately, except here it is what a NULL `params` gets.  See
 * v27fax.h.
 */
struct v27tx_cfg V27TX_CFG = {
	0,			/* protocol                                  */
	9600,			/* bitrate -- neither of V.27ter's own rates  */
	0,			/* int_0004                                  */
	60000,			/* int_0008 -- v27rx_cfg's own value          */
	1,			/* scale_mul -- the PPS gain multiplier       */
	0,			/* flags                                     */
	0,			/* short_0012                                */
	1,			/* fifo_size_factor -- the FIFO's own size
					multiplier, `v17tx_cfg`'s own name   */
	0,			/* int_0018 -- V27TXP_TRAIN_LONG's source     */
	0,			/* int_001c -- FPM_PPS_CFG's aux              */
};

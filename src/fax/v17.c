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

/* The instances are not modelled; see v17fax.h.  These are the only accessors. */
#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))

#define AT_S(p, off)		(*(short *)(void *)FIELD((p), (off)))
#define AT_US(p, off)		(*(unsigned short *)(void *)FIELD((p), (off)))
#define AT_I(p, off)		(*(int *)(void *)FIELD((p), (off)))
#define AT_B(p, off)		(*(unsigned char *)FIELD((p), (off)))
#define AT_SB(p, off)		(*(signed char *)(void *)FIELD((p), (off)))

/*
 * The SMCv17 coder's fields, offset from `smc` (== `V17FP_SMC`) rather than
 * from `fp` -- `V17FP_SMC_SHORT_NN - V17FP_SMC`, tied to those constants
 * rather than restated.  See v17data.h for the derivation.
 */
#define SMC_MODE(smc)		AT_SB((smc), 0x00 - 0x00)
#define SMC_QUAD(smc)		AT_S((smc), V17FP_SMC_SHORT_06 - V17FP_SMC)
#define SMC_STATE(smc)		AT_S((smc), V17FP_SMC_SHORT_08 - V17FP_SMC)
#define SMC_TRELLIS(smc)	AT_S((smc), V17FP_SMC_SHORT_0C - V17FP_SMC)
#define SMC_PREV(smc)		AT_S((smc), V17FP_SMC_SHORT_0E - V17FP_SMC)
#define SMC_NBITS(smc)		AT_US((smc), V17FP_SMC_SHORT_12 - V17FP_SMC)

#define CTL(modem)		FIELD_PTR((modem), V17RX_OBJ_CTL)
#define RXS(modem)		FIELD_PTR((modem), V17RX_OBJ_STATE)

/*
 * The dispatch slot as an lvalue.  `V17RX_modem` already spells the CALL this
 * way; the state machine is what writes it, and storing a handler's address is
 * a link-time reference exactly as a call is (CLAUDE.md, finding F8493).
 */
#define CTL_PROCESS(modem)	\
	(*(v17rx_process_fn *)(void *)FIELD(CTL(modem), V17RXC_PROCESS))

/*
 * The four FPM objects the receive chain runs, reached the long way round
 * because the block they tile is not modelled.  See F8854 for the tiling and
 * v17fax.h for each offset's evidence.
 */
#define RXS_MRF(rxs)	((struct fpm_mrf *)(void *)FIELD((rxs), V17RXS_MRF))
#define RXS_AGC(rxs)	((struct fpm_agc *)(void *)FIELD((rxs), V17RXS_AGC))
#define RXS_SRE(rxs)	((struct fpm_sre *)(void *)FIELD((rxs), V17RXS_SRE))
#define RXS_FSE(rxs)	((struct fpm_fse *)(void *)FIELD((rxs), V17RXS_FSE))

/*
 * The slicers' view of the receiver state, `fpm_fse_cfg::owner`.  `v17dec.h`
 * derives its base as `V17RX_OBJ_STATE + 0x2c` from this function's own
 * `lea 0x2c(%ebp)` at 0x0974a1, and `V17RXS_SGD` is that struct's first
 * member -- so the two names are one address and this is the writer of both.
 */
#define RXS_DEC(rxs)	((struct v17_dec *)(void *)FIELD((rxs), V17RXS_SGD))

/* --------------------------------------------------------------------- */

/*
 * V17RX_create -- .text 0x096eb0, 3,201 bytes.
 *
 * The largest function in the object's fax half, and the writer of every
 * field `v17fax.h`, `v17cfg.h` and `v17dec.h` describe as "planted at
 * construction".  It allocates the instance and its two sub-blocks, builds
 * seven DSP configurations on its stack by copying the library's built-in and
 * patching it, and hands each to its module.
 *
 * ---------------------------------------------------------------------------
 * THE INSTANCE'S HEAD IS A `struct v17rx_cfg`, AND THAT IS A STRUCT
 * ASSIGNMENT AND NOT TEN COPIED WORDS
 *
 * At 0x096ef5 the object copies ten dwords from the second argument to the
 * instance, and at 0x097948 the same ten from `V17RX_CFG` when that argument
 * is NULL -- interleaved load/store pairs over rotating registers, which is
 * GCC's expansion of a 40-byte struct assignment.  Forty bytes is exactly
 * `sizeof(struct v17rx_cfg)`, exactly what `init_vmi_v17rx` allocates
 * (`faxcfg.h`), and the field boundaries agree one for one with what this
 * function then reads back:
 *
 *     +0x04 bit_rate    -> the rate switch at 0x097113, `movswl`
 *     +0x14 int_0014    -> `V17RXC_INT_0010` at 0x09709c
 *     +0x18 coefsave0   -> `fpm_fse_cfg::icoff`  = `V17RX_OBJ_COEFSAVE0`
 *     +0x1c coefsave1   -> `fpm_fse_cfg::qcoff`  = `V17RX_OBJ_COEFSAVE1`
 *     +0x24 ptr_0024    -> three configurations' tail context slot
 *
 * So the second parameter is typed by the object rather than by us, and the
 * three pointers `init_vmi_v17rx` fills with `sysdep_malloc(0x62)`, `(0x62)`
 * and `(2)` are the two 49-entry coefficient saves (`coefsave0`/`coefsave1`,
 * `faxcfg.h`) and the one-short rate save (`ratesave`) `StoreCoefV17`
 * writes.  Finding F9470, names applied at F10169.
 *
 * ---------------------------------------------------------------------------
 * THE TWO "FRESH" FLAGS ARE NOT THE SAME FLAG, AND ONE OF THEM IS WRONG
 *
 * `owned` (the object's `0x2c(%esp)`) is set only when THIS CALL allocated the
 * instance; `ctl_fresh` (its `%ebp`, live 0x096f3d..0x0970f2) only when this
 * call allocated the CONTROL BLOCK.  `FPM_AGC_init` #1 gets `ctl_fresh`, which
 * is right.  `FPM_MRF_init`, `FPM_AGC_init` #2, `FPM_SRE_init` and
 * `FPM_FSE_init` all get `owned` -- and the block they are initialising is the
 * DEMODULATOR STATE, whose own allocation is guarded on
 * `V17RX_OBJ_STATE == NULL` and not on `owned` at all.  Deviation D1221; the
 * wild write and the garbage handle that follow from the same premise are
 * D1222.
 *
 * ---------------------------------------------------------------------------
 * `FPM_TONE_CFG` IS SPELLED `FPM_TONE_CFG_data` HERE, AND THAT IS THE TREE'S
 * SPLIT AND NOT A SECOND TABLE
 *
 * The object's `FPM_TONE_CFG` is the 36-byte structure itself (`R` at .rodata
 * 0xd000, `st_size` 0x24), and this function copies all nine of its dwords.
 * `src/dsp/fpm_tone_cfg.c` names those bytes `FPM_TONE_CFG_data` and keeps
 * `FPM_TONE_CFG` as a `const short *const` pointing at them, which every other
 * caller in the tree already works round the same way (`b103fp.c`,
 * `v22fp.c`, `v23rx.c`, `fpm_fsm.c`).  `t_v17rxcreate.c` compares
 * `FPM_TONE_CFG_data` against `ref_FPM_TONE_CFG` byte for byte rather than
 * assuming it.
 *
 * ---------------------------------------------------------------------------
 * `fpm_sre_cfg` + 0x34 IS ONE 32-BIT FIELD AND THE HEADER MODELS TWO SHORTS
 *
 * 0x097265 is `mov %ecx,0xf4(%esp)`, a DWORD store of the instance's +0x24
 * into the recoverer configuration's +0x34 -- the same slot `fpm_mrf_cfg`
 * spells `aux` and `fpm_fse_cfg` spells `reserved34`, all three filled from
 * one value.  `fpm_sre.h` calls it `pad34`/`pad36`, which is a defect in the
 * tree's model rather than in the object; it is NOT corrected here, because
 * `src/pump/v32/v32fprecr.c` initialises the same struct positionally and is
 * not this pass's to edit.  The slot is written through `memcpy` for exactly
 * the reason that file gives -- the object's single 32-bit store without a
 * strict-aliasing pun -- and finding F9475 records the rename as still owed.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE SHORT-RETRAIN FLAG DOES, RECORDED AS AN INFERENCE
 *
 * `V17RXC_INT_0010` -- copied here from the instance's +0x14, which the
 * caller's parameter block supplied -- governs five things at once: the
 * recoverer's `settle` (48 against 85), the equaliser's `train_sym` (256
 * against 1500), both loops' proportional-gain tables (the `_S` pair against
 * the plain pair), and whether the equaliser starts from the caller's saved
 * coefficients or from `FSEv17_ICOFF`/`FSEv17_QCOFF`.  `StoreCoefV17` fills
 * those same two saved arrays.  That reads as a short retrain and it is
 * recorded as a derivation only: no format string and no callee names it, so
 * the field keeps its neutral name.  Finding F9477.
 *
 * ---------------------------------------------------------------------------
 * NO ERROR PATH EXISTS.  Eight `sysdep_malloc` calls, none checked, and one
 * `ret` at 0x097820 that every path reaches; the return is the instance
 * pointer and there is no way to report failure.  Deviation D1223.
 */
void *
V17RX_create(void *modem, const struct v17rx_cfg *params)
{
	struct fpm_mtd_cfg mtdcfg;
	struct fpm_tone_cfg tonecfg;
	struct fpm_mrf_cfg mrfcfg;
	struct fpm_sre_cfg srecfg;
	struct fpm_fse_cfg fsecfg;
	struct sgd_cfg sgdcfg;
	struct fpm_sdm_cfg sdmcfg;
	struct vtb *v;
	short *mrfbuf;
	short *srebuf;
	void *aux;
	short rate;
	int owned;
	int ctl_fresh;
	int i;

	owned = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V.17 RX Create ");

	if (modem == NULL) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("New allocation\n");
		modem = sysdep_malloc(0x64);
		FIELD_PTR(modem, V17RX_OBJ_CTL) = NULL;
		FIELD_PTR(modem, V17RX_OBJ_STATE) = NULL;
		owned = 1;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("\n");

	/* See the head of this function: 40 bytes, one struct assignment. */
	if (params != NULL)
		*(struct v17rx_cfg *)modem = *params;
	else
		*(struct v17rx_cfg *)modem = V17RX_CFG;

	/* ---- the control block ---------------------------------------- */

	ctl_fresh = 0;
	if (CTL(modem) == NULL) {
		FIELD_PTR(modem, V17RX_OBJ_CTL) = sysdep_malloc(0x5c);
		/*
		 * Three handles and nothing else.  The other 0x4c bytes of the
		 * block keep whatever the allocator left until the code below
		 * writes them, which is why `t_v17rxcreate.c` compares the
		 * whole 0x5c under the harness's 0xa5 fill.
		 */
		FIELD_PTR(CTL(modem), V17RXC_MTD) = NULL;
		FIELD_PTR(CTL(modem), V17RXC_TONE) = NULL;
		FIELD_PTR(CTL(modem), V17RXC_SCRATCH) = sysdep_malloc(0x140);
		FIELD_PTR(CTL(modem), V17RXC_BUF2) = sysdep_malloc(0x140);
		FIELD_PTR(CTL(modem), V17RXC_MTD2) = NULL;
		ctl_fresh = 1;
	}

	/*
	 * The V.17 tone detector.  `FPM_MTD_create` reuses a non-NULL state,
	 * which is what makes handing it the field it is about to overwrite
	 * the module's documented contract rather than a defect.
	 */
	mtdcfg = FPM_MTD_CFG;
	mtdcfg.coeff = V17_MTD_COEFF;
	mtdcfg.tones = 2;
	mtdcfg.ratio = 0x4ccd;
	mtdcfg.min_level = 100;
	FIELD_PTR(CTL(modem), V17RXC_MTD) = FPM_MTD_create(
		(struct fpm_mtd *)FIELD_PTR(CTL(modem), V17RXC_MTD), &mtdcfg);

	/*
	 * The notch the demodulator's pre-pass runs, retuned from the built-in
	 * V.25 answer tone to V.17's own 1800 Hz carrier and otherwise copied
	 * whole.
	 */
	tonecfg = FPM_TONE_CFG_data;
	tonecfg.freq = 1800;
	FIELD_PTR(CTL(modem), V17RXC_TONE) = FPM_TONE_create(
		(struct fpm_tone *)FIELD_PTR(CTL(modem), V17RXC_TONE),
		&tonecfg);

	AT_S(CTL(modem), V17RXC_STATE) = V17RX_STATE_START;
	AT_S(CTL(modem), V17RXC_COUNTDOWN) = 0;
	AT_I(CTL(modem), V17RXC_INT_0008) = 0;
	CTL_PROCESS(modem) = RxHdxStartV17;
	AT_I(CTL(modem), V17RXC_INT_0010) =
		((const struct v17rx_cfg *)modem)->int_0014;

	/*
	 * The SECOND detector, and its band is V.21 CHANNEL 2 -- not V.17's.
	 * `v17fax.h` left `V17RXC_OFFBAND`'s band open because this function
	 * was the only thing that could settle it; the coefficient bank is
	 * `V21_CHAN2_MTD_COEFF` and the level floor is three times the first
	 * detector's.  Finding F9471.
	 */
	mtdcfg = FPM_MTD_CFG;
	mtdcfg.coeff = V21_CHAN2_MTD_COEFF;
	mtdcfg.tones = 2;
	mtdcfg.ratio = 0x4ccd;
	mtdcfg.min_level = 300;
	FIELD_PTR(CTL(modem), V17RXC_MTD2) = FPM_MTD_create(
		(struct fpm_mtd *)FIELD_PTR(CTL(modem), V17RXC_MTD2), &mtdcfg);

	/* The only init in the function given the flag that is about it. */
	FPM_AGC_init((struct fpm_agc *)(void *)FIELD(CTL(modem), V17RXC_AGC),
		     &AGCv17_CFG, ctl_fresh);

	AT_S(CTL(modem), V17RXC_OFFBAND) = 0;
	AT_S(CTL(modem), V17RXC_SHORT_002E) = 0;

	/*
	 * The bit rate to the four-value code, and the two DEAD STORES on the
	 * arm that does not recognise it: 0x097140 and 0x097144 report an
	 * unknown rate through `V17RX_OBJ_RESULT`, and 0x0977a5 further down
	 * this same function clears all four bytes of that word before any
	 * caller can see it.  Deviation D1220, reproduced.
	 */
	switch (AT_S(modem, V17RX_OBJ_RX_BPS)) {
	case 7200:
		AT_US(CTL(modem), V17RXC_RATE_CODE) = V17RX_RATE_7200;
		break;
	case 9600:
		AT_US(CTL(modem), V17RXC_RATE_CODE) = V17RX_RATE_9600;
		break;
	case 12000:
		AT_US(CTL(modem), V17RXC_RATE_CODE) = V17RX_RATE_12000;
		break;
	case 14400:
		AT_US(CTL(modem), V17RXC_RATE_CODE) = V17RX_RATE_14400;
		break;
	default:
		AT_US(CTL(modem), V17RXC_RATE_CODE) = V17RX_RATE_14400;
		AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_ERROR;
		AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_DEFAULT;
		break;
	}

	/* ---- the demodulator state ------------------------------------ */

	/*
	 * One value into three configurations' tail slots, which is F8651's
	 * shape in a second modem: `fpm_mrf_cfg::aux`, `fpm_sre_cfg` + 0x34
	 * and `fpm_fse_cfg::reserved34`.  What it MEANS is not established
	 * here either.
	 */
	aux = ((const struct v17rx_cfg *)modem)->ptr_0024;

	if (RXS(modem) == NULL) {
		FIELD_PTR(modem, V17RX_OBJ_STATE) = sysdep_malloc(0x4fbc);
		/*
		 * NOTHING IS CLEARED HERE.  20,412 bytes of allocator fill,
		 * and the writes below do not cover all of it -- see the
		 * tiling note in v17fax.h for which spans stay untouched.
		 */
		FIELD_PTR(RXS(modem), V17RXS_BUF_MRF) = sysdep_malloc(0x140);
		FIELD_PTR(RXS(modem), V17RXS_BUF_SRE) =
			sysdep_malloc(V17RXS_SRE_MAX * (int)sizeof(short));
	}

	/*
	 * 8000 -> 7200 Hz: nine branches, decimate ten, 360 taps over
	 * `MRFv17_COFFS`.  7200 is three samples a symbol at 2400 baud.
	 */
	mrfcfg = FPM_MRF_CFG;
	mrfcfg.branches = 9;
	mrfcfg.decimate = 10;
	mrfcfg.coeff = MRFv17_COFFS;
	mrfcfg.taps = 0x168;
	mrfcfg.aux = aux;
	FPM_MRF_init(RXS_MRF(RXS(modem)), &mrfcfg, owned);

	FPM_AGC_init(RXS_AGC(RXS(modem)), &AGCv17_CFG, owned);

	/* ---- symbol-timing recovery ----------------------------------- */

	srecfg = FPM_SRE_CFG;
	srecfg.clock_len = 3;
	srecfg.groups_acq = 3;
	srecfg.groups_trk = 0x10;
	srecfg.acc_down = 0x2000;
	srecfg.acc_up = 0x4000;
	srecfg.coeffs = 0xb4;
	srecfg.proto = SREv17_COFFS;
	srecfg.disc = SREv17_XB_COFFS;
	srecfg.xclock = SREv17_xCLOCK;
	srecfg.yclock = SREv17_yCLOCK;
	srecfg.pll_k2 = SREv17_PLL_K2;
	if (AT_I(CTL(modem), V17RXC_INT_0010) != 0) {
		srecfg.settle = 0x30;
		srecfg.pll_k1 = SREv17_PLL_K1_S;
	} else {
		srecfg.settle = 0x55;
		srecfg.pll_k1 = SREv17_PLL_K1;
	}
	srecfg.mag_hi = 2;
	srecfg.mag_lo = 1;
	srecfg.err_hi = 0x2666;
	srecfg.err_lo = 0xc8;
	/*
	 * The level gate's threshold is a SIXTH of the gain control's own
	 * reference, read back out of the AGC the call above has just
	 * configured -- `movswl 0xb4(%edx)` at 0x097334 is
	 * `V17RXS_AGC` + `offsetof(struct fpm_agc, cfg.ref_level)`, and the
	 * `imul $0x2aaaaaab` / `sub` pair is a signed divide by six.
	 */
	srecfg.rms_min = (short)(RXS_AGC(RXS(modem))->cfg.ref_level / 6);
	srecfg.rms_len = 9;
	/* fpm_sre_cfg + 0x34; see the head of this function and F9475. */
	memcpy(&srecfg.pad34, &aux,
	       sizeof srecfg.pad34 + sizeof srecfg.pad36);
	FPM_SRE_init(RXS_SRE(RXS(modem)), &srecfg, owned);

	/*
	 * The four ppm-meter parameters `FPM_SRE_init` never writes, which
	 * `fpm_sre.h` says a caller has to fill.  `ppm_scale` is ppm per
	 * slipped sample at this loop's own output rate: `clock_len` points
	 * per symbol at 9600 symbols a second, so 10^6 / 28800 = 34.
	 */
	RXS_SRE(RXS(modem))->ppm_step = 0x30;
	RXS_SRE(RXS(modem))->ppm_period = 0x2580;
	RXS_SRE(RXS(modem))->ppm_n_max = 0x68;
	RXS_SRE(RXS(modem))->ppm_scale =
		(short)(1000000 / (srecfg.clock_len * 0x2580));

	/* ---- the equaliser and its slicer ----------------------------- */

	fsecfg = FPM_FSE_CFG;
	fsecfg.block = 0x90;
	fsecfg.interp = 3;
	if (AT_I(CTL(modem), V17RXC_INT_0010) != 0) {
		fsecfg.icoff = (const short *)
			FIELD_PTR(modem, V17RX_OBJ_COEFSAVE0);
		fsecfg.qcoff = (const short *)
			FIELD_PTR(modem, V17RX_OBJ_COEFSAVE1);
		fsecfg.pll_k1 = CRRv17_PLL_K1_S;
		fsecfg.train_sym = 0x100;
	} else {
		fsecfg.icoff = FSEv17_ICOFF;
		fsecfg.qcoff = FSEv17_QCOFF;
		fsecfg.pll_k1 = CRRv17_PLL_K1;
		fsecfg.train_sym = 0x5dc;
	}
	fsecfg.taps = V17_COEF_N;
	fsecfg.mu[0] = 0;
	fsecfg.mu[1] = 0;
	fsecfg.clk = CRRv17_CLK;
	fsecfg.clk_mod = 4;
	fsecfg.clk_inc = 1;
	fsecfg.err_hi = 0x199a;
	fsecfg.err_lo = 0x666;
	fsecfg.pll_k2 = CRRv17_PLL_K2;
	fsecfg.owner = RXS_DEC(RXS(modem));
	/*
	 * THE SLICER IS `FAX_FSE_decision_AB` AND IT IS NOT PER RATE.  This
	 * function carries no relocation against `FSEv17_decision` at all --
	 * that table is reached only from `FSE_Bridge_det` and
	 * `FSE_decision_eqtrn`, which is the handshake handing over to the
	 * rate slicer once training ends.  What the constructor installs is
	 * the first segment of the handshake, unconditionally.  Finding F9473.
	 */
	fsecfg.decision = FAX_FSE_decision_AB;
	fsecfg.reserved34 = aux;
	FPM_FSE_init(RXS_FSE(RXS(modem)), &fsecfg, owned);

	/* ---- the slicers' own state ----------------------------------- */

	rate = AT_S(CTL(modem), V17RXC_RATE_CODE);
	RXS_DEC(RXS(modem))->sym_count = 0;
	RXS_DEC(RXS(modem))->short_0066 = 3;
	RXS_DEC(RXS(modem))->rate = rate;
	RXS_DEC(RXS(modem))->short_train = AT_I(CTL(modem), V17RXC_INT_0010);
	RXS_DEC(RXS(modem))->count = 0;
	RXS_DEC(RXS(modem))->scram = 0;
	RXS_DEC(RXS(modem))->ang_prev = 0;
	RXS_DEC(RXS(modem))->eqm_a = 0;
	RXS_DEC(RXS(modem))->eqm_b = 0;
	RXS_DEC(RXS(modem))->int_0050 = 0;
	/*
	 * `movl $0x0,0x54(%ebp)` -- four bytes of the six `v17dec.h` carries
	 * as `pad54`, so the two at +0x58 stay as the allocator left them.
	 * Written through the array because that header is not this pass's to
	 * edit and a wider member would be a claim about bytes the object
	 * does not touch.
	 */
	memset(RXS_DEC(RXS(modem))->pad54, 0, 4);
	/*
	 * `sym_i`, `sym_q`, `sym_i1`, `sym_q1`, `sym_i2`, `sym_q2` -- six
	 * contiguous shorts from +0x3e, cleared by one loop (0x097550,
	 * `cmp $0x5`) rather than one at a time.
	 */
	for (i = 0; (short)i <= 5; i++)
		(&RXS_DEC(RXS(modem))->sym_i)[i] = 0;

	/*
	 * `VTBv32_init`'s body, INLINED, with only the switch's case values
	 * changed: 0x097563..0x09760d is that function instruction for
	 * instruction (compare `src/pump/v32/v32vtb.c`, .text 0x07e700).  The
	 * survivor ring is allocated on `owned` -- the wrong flag again, and
	 * the reason the zeroing loop below can walk an uninitialised pointer.
	 * D1222.
	 */
	v = (struct vtb *)(void *)RXS_DEC(RXS(modem))->vtb;
	if (owned)
		v->paths = (struct vtb_path *)sysdep_malloc(
			16 * 8 * sizeof(struct vtb_path));
	for (i = 0; (short)i <= 0x7f; i++) {
		v->paths[i].surv = 0;
		v->paths[i].sym = 0;
	}

	v->ring = 0;
	v->prev = 0;
	v->depth = 0x10;

	switch (rate) {
	case V17RX_RATE_7200:
		v->nsub = 1;
		v->imap = VTBv17_IMAP16T;
		v->qmap = VTBv17_QMAP16T;
		v->bound = VTB_BOUND_7200;
		v->region = VTB_REGION_7200;
		v->grid = 2;
		v->mask = 0x7;
		break;
	case V17RX_RATE_9600:
		v->nsub = 2;
		v->imap = VTBv17_IMAP32;
		v->qmap = VTBv17_QMAP32;
		v->bound = VTB_BOUND_9600;
		v->region = VTB_REGION_9600;
		v->grid = 4;
		v->mask = 0xf;
		break;
	case V17RX_RATE_12000:
		v->nsub = 3;
		v->imap = VTBv17_IMAP64;
		v->qmap = VTBv17_QMAP64;
		v->bound = VTB_BOUND_12000;
		v->region = VTB_REGION_12000;
		v->grid = 6;
		v->mask = 0x1f;
		break;
	default:
		v->nsub = 4;
		v->imap = VTBv17_IMAP128;
		v->qmap = VTBv17_QMAP128;
		v->bound = VTB_BOUND_14400;
		v->region = VTB_REGION_14400;
		v->grid = 8;
		v->mask = 0x3f;
		break;
	}

	v->metric[0] = 0;
	v->shift = (short)v->nsub;

	for (i = 1; (short)i <= 7; i++)
		v->metric[i] = 0;

	/* ---- the training-sequence engine ----------------------------- */

	if (owned)
		FIELD_PTR(RXS(modem), V17RXS_SGD) = NULL;

	sgdcfg = SGD_CFG;
	sgdcfg.sym_bits = 2;
	sgdcfg.det.ref_margin = 0x2000;
	sgdcfg.det.pat_match = 0x111;
	sgdcfg.det.pat_mask = 0xffff;
	FIELD_PTR(RXS(modem), V17RXS_SGD) = SGD_create(
		(struct sgd *)FIELD_PTR(RXS(modem), V17RXS_SGD), &sgdcfg);

	/*
	 * The descrambler: V.17's own 1 + x^-18 + x^-23, over a word carrying
	 * one symbol's worth of bits.  Every one of `SDM_CFG`'s three shorts
	 * is overwritten, which is deviation D1224 and is what fixes the
	 * source's shape as a copy plus three assignments.
	 */
	sdmcfg = SDM_CFG;
	sdmcfg.nbits = (short)(AT_US(CTL(modem), V17RXC_RATE_CODE) + 3);
	sdmcfg.tap1 = 0x12;
	sdmcfg.tap2 = 0x17;
	SDM_init((struct fpm_sdm *)(void *)FIELD(RXS(modem), V17RXS_SDM),
		 &sdmcfg);

	/*
	 * 160 entries of each chained buffer.  `V17RXS_BUF_MRF` is exactly
	 * that long; `V17RXS_BUF_SRE` is `V17RXS_SRE_MAX` = 164, so its top
	 * four entries keep the allocator's fill.  Deviation D1225.
	 */
	mrfbuf = (short *)FIELD_PTR(RXS(modem), V17RXS_BUF_MRF);
	srebuf = (short *)FIELD_PTR(RXS(modem), V17RXS_BUF_SRE);
	for (i = 0; (short)i <= 0x9f; i++) {
		mrfbuf[i] = 0;
		srebuf[i] = 0;
	}

	AT_S(RXS(modem), V17RXS_QCOUNT) = 0;
	AT_S(RXS(modem), V17RXS_QAVG) = 0;
	AT_S(RXS(modem), V17RXS_SHORT_4FB2) = 0;

	/*
	 * The quality threshold `QualityDetectV17` judges its smoothed
	 * decoder error against on block 0x32, one value per rate.  There is
	 * no default arm: a code outside 0..3 leaves the field as the
	 * allocator left it, which `V17RXC_RATE_CODE`'s own writer above
	 * makes unreachable.
	 */
	switch (AT_S(CTL(modem), V17RXC_RATE_CODE)) {
	case V17RX_RATE_7200:
		AT_S(RXS(modem), V17RXS_SHORT_4FB0) = 0xa28;
		break;
	case V17RX_RATE_9600:
		AT_S(RXS(modem), V17RXS_SHORT_4FB0) = 0x514;
		break;
	case V17RX_RATE_12000:
		AT_S(RXS(modem), V17RXS_SHORT_4FB0) = 0x341;
		break;
	case V17RX_RATE_14400:
		AT_S(RXS(modem), V17RXS_SHORT_4FB0) = 0x1c2;
		break;
	}

	AT_I(RXS(modem), V17RXS_INT_0000) = 1;
	AT_I(RXS(modem), V17RXS_INT_0004) = 1;
	AT_I(RXS(modem), V17RXS_INT_0008) = 1;
	AT_I(RXS(modem), V17RXS_INT_000C) = 0;
	AT_I(RXS(modem), V17RXS_INT_0010) = 1;
	AT_I(RXS(modem), V17RXS_INT_0014) = 0;
	AT_I(RXS(modem), V17RXS_INT_0018) = 1;
	/*
	 * A FULL `int`, and that is what settles the width `v17fax.h` had to
	 * guess: `V17RX_status` reads bit 0 of the byte and this writes
	 * `movl $0x1` over all four.  Finding F9474.
	 */
	AT_I(RXS(modem), V17RXS_INT_001C) = 1;
	AT_I(RXS(modem), V17RXS_INT_0020) = 0;
	AT_US(RXS(modem), V17RXS_RATE_CODE) =
		AT_US(CTL(modem), V17RXC_RATE_CODE);
	AT_I(RXS(modem), V17RXS_INT_0028) = 0;
	AT_S(RXS(modem), V17RXS_SHORT_4FB4) = 1;
	AT_S(RXS(modem), V17RXS_RMS_REF) = 0;
	AT_S(RXS(modem), V17RXS_RMS_PHASE) = 0;

	/* ---- what the instance hands back to its caller --------------- */

	/*
	 * The result word, cleared and then rebuilt: this is where the two
	 * dead stores on the unrecognised-rate arm go, and where bits 4 and 6
	 * of `V17RX_OBJ_RESULT_B1` -- which nothing else in the object writes
	 * and nothing at all reads -- are set.  Finding F9473.
	 */
	AT_I(modem, V17RX_OBJ_RESULT) = 0;
	AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_BIT4 | V17RX_FLAG_BIT6;
	AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_START;

	/*
	 * Six handles copied out of the equaliser the call above has just
	 * built, and six fields cleared.  All six sources are `struct fpm_fse`
	 * members, which is what types them -- rank 2 and not usage
	 * inference.  The six zeroed at +0x44..+0x58 have no evidence of role
	 * anywhere and are left unnamed.  Finding F9476.
	 */
	FIELD_PTR(modem, V17RX_OBJ_OUT_I) = RXS_FSE(RXS(modem))->out_i;
	FIELD_PTR(modem, V17RX_OBJ_OUT_Q) = RXS_FSE(RXS(modem))->out_q;
	FIELD_PTR(modem, V17RX_OBJ_N_OUT) = &RXS_FSE(RXS(modem))->n_out;
	FIELD_PTR(modem, V17RX_OBJ_ICOEFF) = RXS_FSE(RXS(modem))->icoeff;
	FIELD_PTR(modem, V17RX_OBJ_QCOEFF) = RXS_FSE(RXS(modem))->qcoeff;
	AT_US(modem, V17RX_OBJ_TAPS) = (unsigned short)
		RXS_FSE(RXS(modem))->cfg.taps;

	AT_I(modem, V17RX_OBJ_INT_0044) = 0;
	AT_I(modem, V17RX_OBJ_INT_0048) = 0;
	AT_S(modem, V17RX_OBJ_SHORT_004C) = 0;
	AT_I(modem, V17RX_OBJ_INT_0050) = 0;
	AT_I(modem, V17RX_OBJ_INT_0054) = 0;
	AT_S(modem, V17RX_OBJ_SHORT_0058) = 0;

	return modem;
}

/* --------------------------------------------------------------------- */

/*
 * V17RX_delete -- .text 0x097b40, 251 bytes.  See v17fax.h for the order and
 * for why the object's literal 1 in the second argument slot is not here.
 */
void
V17RX_delete(void *modem)
{
	SGD_delete((struct sgd *)FIELD_PTR(RXS(modem), V17RXS_SGD));
	sysdep_free(FIELD_PTR(RXS(modem), V17RXS_PTR_0030));

	FPM_FSE_free(RXS_FSE(RXS(modem)));
	FPM_SRE_free(RXS_SRE(RXS(modem)));
	FPM_MRF_free(RXS_MRF(RXS(modem)));

	sysdep_free(FIELD_PTR(RXS(modem), V17RXS_BUF_SRE));
	sysdep_free(FIELD_PTR(RXS(modem), V17RXS_BUF_MRF));
	sysdep_free(RXS(modem));

	FPM_MTD_delete((struct fpm_mtd *)FIELD_PTR(CTL(modem), V17RXC_MTD));
	FPM_TONE_delete((struct fpm_tone *)FIELD_PTR(CTL(modem), V17RXC_TONE));
	sysdep_free(FIELD_PTR(CTL(modem), V17RXC_SCRATCH));
	sysdep_free(FIELD_PTR(CTL(modem), V17RXC_BUF2));
	FPM_MTD_delete((struct fpm_mtd *)FIELD_PTR(CTL(modem), V17RXC_MTD2));
	sysdep_free(CTL(modem));

	sysdep_free(modem);
}

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

/*
 * ---------------------------------------------------------------------------
 * V17TX_create -- .text 0x0989e0, 1,043 bytes.
 *
 * THE SHAPE IS `V29TX_create`'s AND `V21TX_create`'s, ONE CONFIG DWORD WIDER:
 * allocate-or-reuse the handle, copy the caller's config (or `V17TX_CFG`)
 * onto its first 0x20 bytes, allocate-or-reuse the parameter/half-duplex
 * block and build the FIFO and the SGD generator into it, derive
 * `V17TXP_MODE` from the caller's bit rate, seed the half-duplex machine at
 * `V17TX_STATE_START`, then allocate-or-reuse the private block and
 * initialise its ring, its scrambler, its symbol coder and its pulse shaper.
 * Two debug strings, "V.17 TX Create " (0x458e) then either
 * "New allocation\n" (0x459e) or "\n" (0x458c).
 *
 * `V17TXP_MODE` IS DERIVED HERE, keyed on the caller's `bitrate`:
 * 7200/9600/12000/14400 map to 0/1/2/3; anything else takes mode 3 too but
 * ALSO raises `V17TX_RESULT_B1_BIT1` and writes `V17TX_RESULT_BYTE_07`, the
 * same "recognised value, or the ceiling value plus an error flag" shape
 * `V29TX_create`'s own rate switch uses.
 *
 * `fresh` IS THE SAME STACK SLOT FROM ENTRY TO EXIT -- the object spills the
 * "did THIS call allocate the handle" flag at one local (0x18(%esp)) at entry
 * and reloads the identical slot at 0x98c5f to pass as `FPM_PPS_init`'s third
 * argument, over 700 bytes later.  `V29TX_create`'s own comment describes the
 * same carrier.
 *
 * THE TRANSMIT FIFO'S SIZE IS COMPUTED, `fifo_size_factor * 3 * 16` (`lea
 * (%eax,%eax,2),%esi; shl $0x4,%esi` at 0x98aa6/0x98aab), 48 for the default
 * config's `fifo_size_factor` of 1.  `word0` carries over from `FIFO_CFG`
 * unchanged, but `fill` IS FORCED TO A LITERAL ZERO (`mov %di,0xa4(%esp)` at
 * 0x98a9e, overwriting the `FIFO_CFG.fill` value the two preceding
 * instructions had just loaded into the same slot) -- NOT `V29TX_create`'s
 * own shape, which keeps `FIFO_CFG.fill` unchanged.  Measured from the two
 * writes' addresses, not assumed from the sibling.
 *
 * THE SGD GENERATOR TAKES `SGD_CFG` WITH ONLY `sym_bits` PATCHED, to 2
 * (V.29's own copy patches it to 4) -- the whole 13-dword template is copied
 * (`rep movsl`, 0x98ac8) and every other field survives.
 *
 * `V17TXP_NOCARRIER_SYM` (v17data.h, TxNoCarrierV17's own symbol index) IS
 * SEEDED TO 4 HERE (`movw $0x4,0x1e(%edx)` at 0x98b03) -- the one field of
 * the parameter block this constructor writes that TxNoCarrierV17, not
 * TxNextStateV17, later reads.
 *
 * `V17TXP_INT_000C` IS THE CALLER'S OWN `int_0018`, COPIED VERBATIM (`mov
 * 0x18(%ebp),%edi; mov %edi,0xc(%edx)` at 0x98b14/0x98b23) -- see its own
 * comment in v17fax.h for what little the object establishes about it.
 *
 * `RING.SYM` IS ALLOCATED ONLY ONCE, UNLIKE `V29TX_create`'s OWN RING --
 * `sysdep_malloc(0x64)` for `V17FP_PTR_0010` sits INSIDE the private block's
 * own fresh-allocation branch (guarded on `V17TX_OBJ_FP == NULL`) and is
 * skipped entirely when that block is reused, where `V29TX_create`
 * reallocates its ring's `sym` array unconditionally on every call.  Two
 * different objects, two different reuse disciplines; each reproduced as
 * measured.  `V17FP_PTR_0010` -- named in v17data.h before this function was
 * reconstructed -- IS `struct fpm_smc_ring`'s own `sym` field; `ring.i` and
 * `ring.q` stay NULL, because V.17's transmitter uses the MAPPED ring form
 * exclusively (v17data.h's own derivation from `TxNoCarrierV17`).
 *
 * `FPM_PPS_CFG.AUX` CARRIES THE CALLER'S OWN `int_001c`, read back at
 * 0x098b4e and stored through to the shaper's own configuration at 0x098be3
 * -- `V17TX_CFG`'s own `int_001c` is 0, so this is invisible on the default
 * config, the same `(void *)(long)` idiom D1250 records for `V29TX_create`'s
 * `int_0018`.
 *
 * THE THREE ENCODER TABLE ENTRIES ARE PLANTED IN THE OBJECT'S OWN ORDER --
 * `SMCv17_encoder_abs` (fp + 0x84), then `SMCv17_encoder_dif` (fp + 0x80),
 * then `SMCv17_encoder_tcm` (fp + 0x88) -- exactly `v17data.h`'s own
 * three-address note, confirmed a second time from this side of the call.
 *
 * Finding F9911.
 */
void *
V17TX_create(void *modem, const struct v17tx_cfg *params)
{
	void *prm;
	void *fp;
	void *existing;
	int fresh = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V.17 TX Create ");

	if (modem == 0) {
		modem = sysdep_malloc(0x2c);
		FIELD_PTR(modem, V17TX_OBJ_PARAMS) = 0;
		FIELD_PTR(modem, V17TX_OBJ_FP) = 0;
		fresh = 1;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("New allocation\n");
	} else {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\n");
	}

	if (params != 0)
		*(struct v17tx_cfg *)modem = *params;
	else
		*(struct v17tx_cfg *)modem = V17TX_CFG;

	AT_I(modem, V17TX_OBJ_RESULT) = 0;
	*FIELD(modem, V17TX_OBJ_RESULT_B1) |= 0x58;
	AT_B(modem, V17TX_OBJ_RESULT) = 1;

	/* ---- the parameter/half-duplex block, the FIFO and the SGD ------- */

	prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
	if (prm == 0) {
		prm = sysdep_malloc(0x20);
		FIELD_PTR(modem, V17TX_OBJ_PARAMS) = prm;
		FIELD_PTR(prm, V17TXP_FIFO) = 0;
		FIELD_PTR(prm, V17TXP_SGD) = 0;
	}

	{
		struct fifo_cfg fc;
		unsigned short n = (unsigned short)
			((struct v17tx_cfg *)modem)->fifo_size_factor;

		fc.word0 = FIFO_CFG.word0;
		fc.size = (short)(n * 3 * 16);
		fc.fill = 0;

		existing = FIELD_PTR(prm, V17TXP_FIFO);
		FIELD_PTR(prm, V17TXP_FIFO) =
			FIFO_create((struct fax_fifo *)existing, &fc);
	}

	{
		struct sgd_cfg gcfg = SGD_CFG;

		gcfg.sym_bits = 2;

		existing = FIELD_PTR(prm, V17TXP_SGD);
		FIELD_PTR(prm, V17TXP_SGD) =
			SGD_create((struct sgd *)existing, &gcfg);
	}

	/* ---- the half-duplex machine's own state ------------------------- */

	prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
	AT_S(prm, V17TXP_STATE) = V17TX_STATE_START;
	AT_S(prm, V17TXP_SHORT_001A) = 0;
	AT_S(prm, V17TXP_NOCARRIER_SYM) = 4;
	AT_I(prm, V17TXP_INT_0008) = 0;
	*(v17tx_process_fn *)(void *)FIELD(prm, V17TXP_PROCESS) = TxHdxStartV17;
	AT_I(prm, V17TXP_INT_000C) = ((struct v17tx_cfg *)modem)->int_0018;

	if (((struct v17tx_cfg *)modem)->bitrate == 9600) {
		AT_S(prm, V17TXP_MODE) = 1;
	} else if (((struct v17tx_cfg *)modem)->bitrate == 12000) {
		AT_S(prm, V17TXP_MODE) = 2;
	} else if (((struct v17tx_cfg *)modem)->bitrate == 7200) {
		AT_S(prm, V17TXP_MODE) = 0;
	} else if (((struct v17tx_cfg *)modem)->bitrate == 14400) {
		AT_S(prm, V17TXP_MODE) = 3;
	} else {
		AT_S(prm, V17TXP_MODE) = 3;
		*FIELD(modem, V17TX_OBJ_RESULT_B1) |= V17TX_RESULT_B1_BIT1;
		AT_B(modem, V17TX_OBJ_RESULT) = V17TX_RESULT_BYTE_07;
	}

	/* ---- the private block: the ring, the scrambler, the symbol coder
	 * and the pulse shaper ---------------------------------------------- */

	fp = FIELD_PTR(modem, V17TX_OBJ_FP);
	if (fp == 0) {
		fp = sysdep_malloc(0x90);
		FIELD_PTR(modem, V17TX_OBJ_FP) = fp;
		FIELD_PTR(fp, V17FP_PTR_0010) = sysdep_malloc(0x64);
	}

	{
		struct fpm_smc_ring *ring = (struct fpm_smc_ring *)(void *)
			FIELD(fp, V17FP_SMC_RING);
		short i;

		ring->i = 0;
		ring->q = 0;
		ring->widx = 0;
		ring->ridx = 0;
		ring->len = 0x32;

		ring->sym = (short *)FIELD_PTR(fp, V17FP_PTR_0010);
		for (i = 0; i <= 0x31; i++)
			ring->sym[i] = 0;
	}

	{
		struct fpm_sdm_cfg dcfg;

		dcfg.nbits = 2;
		dcfg.tap1 = 0x12;
		dcfg.tap2 = 0x17;

		SDM_init((struct fpm_sdm *)(void *)FIELD(fp, V17FP_SDM), &dcfg);
	}

	{
		short scfg[2];

		scfg[0] = SMCv17_CFG[0];
		scfg[1] = SMCv17_CFG[1];
		SMCv17_init(FIELD(fp, V17FP_SMC), scfg);
	}

	{
		struct fpm_pps_cfg pcfg = FPM_PPS_CFG;

		pcfg.phases = 10;
		pcfg.step = 3;
		pcfg.mapped = 1;
		pcfg.scale = V17TX_PPS_SCALE[AT_S(prm, V17TXP_MODE)];
		pcfg.step_adj = 0;
		pcfg.imap = SMCv17_IMAP4;
		pcfg.qmap = SMCv17_QMAP4;
		pcfg.coeff_i = PPSv17_ICOFFS;
		pcfg.coeff_q = PPSv17_QCOFFS;
		pcfg.coeffs = 120;
		pcfg.aux = (void *)(long)((struct v17tx_cfg *)modem)->int_001c;

		FPM_PPS_init((struct fpm_pps *)(void *)FIELD(fp, V17FP_PPS),
			     &pcfg, fresh);
	}

	FIELD_PTR(fp, V17FP_ENCODERS) = (void *)SMCv17_encoder_dif;
	FIELD_PTR(fp, V17FP_ENCODERS + 4) = (void *)SMCv17_encoder_abs;
	FIELD_PTR(fp, V17FP_ENCODERS + 8) = (void *)SMCv17_encoder_tcm;

	return modem;
}

/*
 * ---------------------------------------------------------------------------
 * V17TX_delete -- .text 0x098e00, 107 bytes.  See v17fax.h; the object's
 * literal 1 before `FPM_PPS_free` is F8876 again and is not reproduced.
 */
void
V17TX_delete(void *modem)
{
	FPM_PPS_free((struct fpm_pps *)(void *)
			FIELD(FIELD_PTR(modem, V17TX_OBJ_FP), V17FP_PPS));
	sysdep_free(FIELD_PTR(FIELD_PTR(modem, V17TX_OBJ_FP), V17FP_PTR_0010));
	sysdep_free(FIELD_PTR(modem, V17TX_OBJ_FP));

	SGD_delete((struct sgd *)
			FIELD_PTR(FIELD_PTR(modem, V17TX_OBJ_PARAMS),
				  V17TXP_SGD));
	FIFO_delete((struct fax_fifo *)
			FIELD_PTR(FIELD_PTR(modem, V17TX_OBJ_PARAMS),
				  V17TXP_FIFO));
	sysdep_free(FIELD_PTR(modem, V17TX_OBJ_PARAMS));

	sysdep_free(modem);
}

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

/* (widx + 1) mod len, exactly as the object spells it -- see v32smc.c. */
static short
smc_ring_advance(short widx, short len)
{
	short next = (short)(widx + 1);

	return (short)((next < len) ? next : 0);
}

/*
 * SMCv17_encoder_dif -- .text 0x09fcc0, 164 bytes.
 *
 * `state = (state + PMAP4[in[i] & 3]) & 3; point = (state + quad + 1) & 3`.
 * `in[i]` is read whole and unsigned (`movzwl`) and only its low two bits are
 * ever used, so nothing separates `short` from `unsigned short` here -- the
 * type follows `v17_encoder_fn`, not a forced reading.
 */
void
SMCv17_encoder_dif(void *smc, struct fpm_smc_ring *ring,
		   const unsigned short *data, unsigned short count)
{
	short *const sym = ring->sym;
	const short len = ring->len;
	short widx = ring->widx;
	int quad = SMC_QUAD(smc);
	int state = SMC_STATE(smc);
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned int sel = (unsigned int)data[i] & 3u;
		int point;

		quad = (quad + 3) & 3;
		state = (int)(state + SMCv17_PMAP4[sel]) & 3;
		point = (state + quad + 1) & 3;
		sym[widx] = (short)point;
		widx = smc_ring_advance(widx, len);
	}

	SMC_STATE(smc) = (short)state;
	SMC_QUAD(smc) = (short)quad;
	ring->widx = widx;
}

/*
 * SMCv17_encoder_abs -- .text 0x09fd70, 135 bytes.
 *
 * `point = (quad + ABS4[in[i] & 3]) & 3` -- no accumulator, the point comes
 * straight out of the table every symbol.
 */
void
SMCv17_encoder_abs(void *smc, struct fpm_smc_ring *ring,
		   const unsigned short *data, unsigned short count)
{
	short *const sym = ring->sym;
	const short len = ring->len;
	short widx = ring->widx;
	int quad = SMC_QUAD(smc);
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned int sel = (unsigned int)data[i] & 3u;
		int point;

		quad = (quad + 3) & 3;
		point = (int)(quad + SMCv17_ABS4[sel]) & 3;
		sym[widx] = (short)point;
		widx = smc_ring_advance(widx, len);
	}

	SMC_QUAD(smc) = (short)quad;
	ring->widx = widx;
}

/*
 * SMCv17_encoder_tcm -- .text 0x09fe00, 374 bytes.
 *
 * The trellis coder.  Three tables and four pieces of state (`quad`,
 * `trellis`, `prev`, `nbits`), and unlike its two siblings it WRITES BACK to
 * its input buffer -- see v17data.h for why `data` cannot be `const` here.
 *
 *     in[i] &= mask_all                        -- in place, before anything
 *     trellis = TrellisEncodeDifTable[trellis + (in[i] >> nbits) * 4]
 *     word    = (trellis << nbits) + (in[i] & mask_low)
 *     if (prev > 3) word = (word + (1 << (nbits + 2))) & 0xffff
 *     quad    = (quad + 3) & 3
 *     prev    = TrellisTransitionTable[trellis + prev * 4]
 *     rot     = (SMCv17_MOD[word >> nbits] >> (quad * 4)) & 7
 *     out     = ((rot << nbits) + (word & mask_low)) | (mode << 8)
 *
 * Exactly `SMCv32_encoder_tcm`'s algorithm (`v32smc.c`), reusing that file's
 * tables; only the state's offsets differ.  `mask_low`, `mask_all` and the
 * `1 << (nbits + 2)` constant are built once, before the loop, and all three
 * are truncated to 16 bits where the object builds them.
 *
 * `prev > 3` is a SIGNED 16-bit comparison (`cmpw $0x3` / `jle`) against the
 * value from the PREVIOUS iteration -- the update below it happens later in
 * the same body, exactly as in V.32's coder.
 */
void
SMCv17_encoder_tcm(void *smc, struct fpm_smc_ring *ring,
		   unsigned short *data, unsigned short count)
{
	short *const sym = ring->sym;
	const short len = ring->len;
	const int nbits = SMC_NBITS(smc);
	const unsigned short mask_low = (unsigned short)((1 << nbits) - 1);
	const unsigned short bit_hi = (unsigned short)(1 << (nbits + 2));
	const unsigned short mask_all = (unsigned short)(bit_hi - 1);
	const int tag = SMC_MODE(smc) << 8;
	short widx = ring->widx;
	int quad = SMC_QUAD(smc);
	int trellis = SMC_TRELLIS(smc);
	int prev = SMC_PREV(smc);
	unsigned int i;

	for (i = 0; i < count; i++) {
		int word;
		int rot;
		int masked;

		/* In place, and the caller sees it. */
		data[i] = (unsigned short)(data[i] & mask_all);
		masked = data[i];

		trellis = TrellisEncodeDifTable[trellis
						+ ((masked >> nbits) * 4)];
		word = (trellis << nbits) + (masked & mask_low);
		if (prev > 3)
			word = (int)(unsigned short)(word + bit_hi);

		quad = (quad + 3) & 3;
		prev = TrellisTransitionTable[trellis + prev * 4];

		rot = (SMCv17_MOD[(unsigned short)(word >> nbits)]
		       >> (quad * 4)) & 7;
		sym[widx] = (short)(((rot << nbits) + (word & mask_low))
				    | tag);
		widx = smc_ring_advance(widx, len);
	}

	SMC_QUAD(smc) = (short)quad;
	SMC_TRELLIS(smc) = (short)trellis;
	SMC_PREV(smc) = (short)prev;
	ring->widx = widx;
}

/* --------------------------------------------------------------------- */

int
V17RX_modem(void *modem, short *in, short *out, unsigned short *count)
{
	short total;
	short before;
	unsigned short left;

	*FIELD(modem, V17RX_OBJ_RESULT_B1) &=
		(unsigned char)~V17RX_FLAG_ERROR;

	total = 0;
	do {
		void *ctl;
		short got;

		/*
		 * `before` is signed and `left` is not, and both are what the
		 * object encodes: ONE 16-bit load feeds a `movswl` at the top
		 * of the loop and a `movzwl` after the call, because the
		 * compiler shared the read across the back edge.
		 */
		before = (short)*count;

		ctl = FIELD_PTR(modem, V17RX_OBJ_CTL);
		got = (*(v17rx_process_fn *)(void *)FIELD(ctl, V17RXC_PROCESS))
				(modem, in, out, count);

		left = *count;
		in += before - left;
		out += got;
		total = (short)(total + got);
	} while (left != 0);

	*count = (unsigned short)total;

	return AT_I(modem, V17RX_OBJ_RESULT);
}

/* --------------------------------------------------------------------- */

/*
 * RxHdxDataV17 -- .text 0x0a0000, 226 bytes.
 *
 * The DATA state of the receive machine.  See v17fax.h for the flag-bit
 * enumeration and for `V17RXC_INT_0008`.
 *
 * THE `out` CASTS ARE THE RECONSTRUCTION'S AND THE OBJECT CANNOT SEE THEM.
 * The state handlers share one signature (`v17rx_process_fn`) whose third
 * argument this tree already spells `short *`, while `DemodDataV17` and
 * `DescrambleDataV17` declare theirs `unsigned short *`.  Both pointers are
 * passed through untouched, so nothing in the object distinguishes the two
 * spellings and the casts cost no instruction.  Deviation D1141.
 *
 * THE RESULT IS A `?:` AND THE OBJECT SPELLS IT BRANCHLESSLY.  It emits
 * `cmp $0x2,%ax` / `setne %al` / `movzbl %al,%esi` / `neg %esi` /
 * `and %edi,%esi`, which is `n & -(q != 2)` -- the standard shape GCC folds a
 * two-armed conditional into when one arm is a constant zero and the guard is
 * already a flag.  The `?:` is what is written here, because it is the source
 * that expression is the compilation of and because writing the mask by hand
 * would be fitting the object rather than reading it.  `n` is `unsigned short`
 * and the object zero-extends it (`movzwl %ax,%edi`), which is the local's
 * declared type showing through (finding F7803); the final `movswl %si` is the
 * function's own `short` return.
 *
 * THE `andb $0x7f` SITS BETWEEN THE `setne` AND THE `and` in the object.  That
 * is the scheduler moving a store with no dependence on either, not a
 * statement order to reproduce: the clear of `V17RX_FLAG_LOW_SNR` belongs with
 * the `GetSNRV17` test it precedes.
 */
short
RxHdxDataV17(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short r;

	AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_CARRIER;
	AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_DATA;

	if (DataCarrierDetectV17(modem, in, *count) == 0
	    || AT_I(CTL(modem), V17RXC_INT_0008) != 0) {
		AT_B(modem, V17RX_OBJ_RESULT_B1) &=
			(unsigned char)~V17RX_FLAG_CARRIER;
		*count = 0;
		return 0;
	}

	n = DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV17(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	r = (short)(QualityDetectV17(modem) != V17_QUALITY_UNRELIABLE ? n : 0);

	AT_B(modem, V17RX_OBJ_RESULT_B1) &=
		(unsigned char)~V17RX_FLAG_LOW_SNR;
	if (GetSNRV17(modem) <= V17RX_SNR_THRESHOLD)
		AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_LOW_SNR;

	return r;
}

/* --------------------------------------------------------------------- */

/*
 * RxHdxErrorV17 -- .text 0x0a00f0, 59 bytes.
 *
 * The ERROR state: raise the flag, demodulate anyway so the filters keep their
 * history, and consume the block.  `DemodDataV17`'s return is DISCARDED, which
 * is the one thing separating this from a data handler that happened to fail.
 */
short
RxHdxErrorV17(void *modem, short *in, short *out, unsigned short *count)
{
	AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_ERROR;

	DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * RxNextStateV17 -- .text 0x0a0130, 730 bytes.
 *
 * The receive machine's state advance.  See `v17fax.h` for what each arm does
 * and for the eight state names, which are the author's own out of
 * `.rodata.str1.1`.
 *
 * THE TABLE IS THE COMPILER'S AND THERE IS NOTHING TO REPRODUCE.  The object
 * dispatches through `jmp *0xc2d0(,%eax,4)` after `cmp $0x6` / `ja`, which is
 * what GCC emits for a dense `switch` on 0..6 with a `default`.  The `ja` is
 * unsigned over a `movswl`-widened `short`, so a negative state takes the
 * default rather than indexing backwards, and `t_v17rxstate.c` drives -1 for
 * exactly that reason.
 *
 * EVERY ARM RE-READS `V17RX_OBJ_CTL` AFTER ITS DIAGNOSTIC AND THAT IS FORCED.
 * The object reloads `0x5c(%ebx)` at 0x0a0398, 0x0a0384, 0x0a035c, 0x0a03d1,
 * 0x0a03ac and 0x0a0370 -- once per arm that prints -- because the call to
 * `dsplibs_debug_printf` clobbers memory it cannot see through.  The `CTL()`
 * macro expands at each use, so the reloads come out of the C rather than
 * being imitated; the SCRAM arm has none because its own reload happens after
 * `FPM_AGC_Freeze` instead.
 *
 * THE EPOCH_DET ARM TESTS `V17RXC_INT_0010` TWICE AND THE SECOND TEST IS DEAD.
 * `Restore_rateV17` writes only `V17RXS_RATE` and `V17RXS_SHORT_01F8`, both in
 * the demodulator state, so it cannot change a control-block field -- but the
 * compiler does not know that, and the object's `mov 0x10(%edx),%ecx` /
 * `test` / `jne` at 0x0a0340 is the reload it is forced into.  On the arm
 * where the call did NOT happen it CSEs the two tests and jumps straight to
 * the 62, which is why 0x0a01d9's not-taken edge goes to 0x0a01df.  Written as
 * the two reads the source had; deviation D1212.
 */
void
RxNextStateV17(void *modem)
{
	switch (AT_S(CTL(modem), V17RXC_STATE)) {
	case V17RX_STATE_START:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_STATE_START\n");
		AT_S(CTL(modem), V17RXC_COUNTDOWN) = 5;
		CTL_PROCESS(modem) = RxHdxEpochDetV17;
		AT_S(CTL(modem), V17RXC_STATE) = V17RX_STATE_EPOCH_DET;
		AT_B(modem, V17RX_OBJ_RESULT_B2) &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		AT_B(modem, V17RX_OBJ_RESULT_B1) &=
			(unsigned char)~V17RX_FLAG_DATA;
		break;

	case V17RX_STATE_EPOCH_DET:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_STATE_EPOCH_DET\n");
		if (AT_I(CTL(modem), V17RXC_INT_0010) != 0)
			Restore_rateV17(modem);
		AT_S(CTL(modem), V17RXC_COUNTDOWN) = (short)
			(AT_I(CTL(modem), V17RXC_INT_0010) != 0 ? 1 : 62);
		CTL_PROCESS(modem) = RxHdxPrtcolV17;
		AT_S(CTL(modem), V17RXC_STATE) = V17RX_STATE_PROTOCOL;
		AT_B(modem, V17RX_OBJ_RESULT_B2) &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		AT_B(modem, V17RX_OBJ_RESULT_B1) &=
			(unsigned char)~V17RX_FLAG_DATA;
		/*
		 * Acquisition to tracking: one `short` along each of the two
		 * Q15 coefficient tables `AGCv17_CFG` points at, which the
		 * object spells `addl $0x2` because that is what `const short *`
		 * arithmetic compiles to.  See v17fax.h and D1216.
		 */
		RXS_AGC(RXS(modem))->cfg.alpha++;
		RXS_AGC(RXS(modem))->cfg.beta++;
		break;

	case V17RX_STATE_PROTOCOL:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_STATE_PROTOCOL\n");
		if (AT_I(CTL(modem), V17RXC_INT_0010) != 0) {
			AT_S(CTL(modem), V17RXC_COUNTDOWN) = 1;
			CTL_PROCESS(modem) = RxHdxScramV17;
			AT_S(CTL(modem), V17RXC_STATE) = V17RX_STATE_SCRAM;
			AT_B(modem, V17RX_OBJ_RESULT_B1) &=
				(unsigned char)~V17RX_FLAG_DATA;
			/* No write to V17RX_OBJ_RESULT_B2 here.  D1213. */
		} else {
			AT_S(CTL(modem), V17RXC_COUNTDOWN) = 1;
			CTL_PROCESS(modem) = RxHdxBridgeV17;
			AT_S(CTL(modem), V17RXC_STATE) = V17RX_STATE_BRIDGE;
			AT_B(modem, V17RX_OBJ_RESULT_B1) &=
				(unsigned char)~V17RX_FLAG_DATA;
			StoreCoefV17(modem);
			AT_B(modem, V17RX_OBJ_RESULT_B2) &=
				(unsigned char)~V17RX_RESULT_B2_BIT0;
		}
		break;

	case V17RX_STATE_BRIDGE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_STATE_BRIDGE\n");
		AT_S(CTL(modem), V17RXC_COUNTDOWN) = 1;
		CTL_PROCESS(modem) = RxHdxScramV17;
		AT_S(CTL(modem), V17RXC_STATE) = V17RX_STATE_SCRAM;
		AT_B(modem, V17RX_OBJ_RESULT_B2) &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		AT_B(modem, V17RX_OBJ_RESULT_B1) &=
			(unsigned char)~V17RX_FLAG_DATA;
		break;

	case V17RX_STATE_SCRAM:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_STATE_SCRAM\n");
		FPM_AGC_Freeze(RXS_AGC(RXS(modem)));
		AT_S(CTL(modem), V17RXC_COUNTDOWN) = 0;
		CTL_PROCESS(modem) = RxHdxDataV17;
		AT_S(CTL(modem), V17RXC_STATE) = V17RX_STATE_DATA;
		AT_B(modem, V17RX_OBJ_RESULT_B2) &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_DATA;
		break;

	case V17RX_STATE_DATA:
		/*
		 * UNREACHABLE IN THE OBJECT, AND WRITTEN ANYWAY.  Nothing that
		 * runs while the state is DATA calls this function --
		 * `RxHdxDataV17` and `RxHdxErrorV17` are the two handlers that
		 * never do -- so this arm is 52 bytes of code the machine
		 * cannot enter.  Deviation D1210.  The store order differs from
		 * every other arm's (handler, state, countdown, rather than
		 * countdown, handler, state) and is the object's.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_STATE_DATA\n");
		CTL_PROCESS(modem) = RxHdxIdleV17;
		AT_S(CTL(modem), V17RXC_STATE) = V17RX_STATE_IDLE;
		AT_S(CTL(modem), V17RXC_COUNTDOWN) = 0;
		AT_I(CTL(modem), V17RXC_INT_0008) = 0;
		AT_B(modem, V17RX_OBJ_RESULT_B2) |= V17RX_RESULT_B2_BIT0;
		AT_B(modem, V17RX_OBJ_RESULT_B1) &=
			(unsigned char)~V17RX_FLAG_DATA;
		break;

	case V17RX_STATE_IDLE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_STATE_IDLE\n");
		CTL_PROCESS(modem) = RxHdxDataV17;
		AT_S(CTL(modem), V17RXC_STATE) = V17RX_STATE_DATA;
		AT_B(modem, V17RX_OBJ_RESULT_B2) &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_DATA;
		/* The one arm that does not seed the countdown.  D1215. */
		switch (AT_US(CTL(modem), V17RXC_RATE_CODE)) {
		case V17RX_RATE_7200:
			AT_B(modem, V17RX_OBJ_RESULT) =
				V17RX_STATUS_RATE_7200;
			break;
		case V17RX_RATE_9600:
			AT_B(modem, V17RX_OBJ_RESULT) =
				V17RX_STATUS_RATE_9600;
			break;
		case V17RX_RATE_12000:
			AT_B(modem, V17RX_OBJ_RESULT) =
				V17RX_STATUS_RATE_12000;
			break;
		default:
			AT_B(modem, V17RX_OBJ_RESULT) =
				V17RX_STATUS_RATE_14400;
			break;
		}
		break;

	default:
		/*
		 * Reached by `V17RX_STATE_ERROR`, which has no arm of its own,
		 * and by any state outside 0..6.  D1211.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_DEFAULT: %d\n",
					     AT_S(CTL(modem), V17RXC_STATE));
		AT_B(modem, V17RX_OBJ_RESULT_B2) &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_DEFAULT;
		/*
		 * THE MASK IS 0xde AND NOT 0xdf: this arm clears CARRIER *and*
		 * `V17RX_FLAG_DATA`, where the four handlers' error arms clear
		 * CARRIER alone (`and $0xdf`).  That is consistent with every
		 * other arm of this function writing the DATA bit -- the
		 * default is not a transition, so it clears it -- and it is
		 * one of the two places the two masks differ by exactly that
		 * bit.  Read the bytes at 0x0a0168 and 0x0a0192.
		 */
		AT_B(modem, V17RX_OBJ_RESULT_B1) = (unsigned char)
			((AT_B(modem, V17RX_OBJ_RESULT_B1) | V17RX_FLAG_ERROR)
			 & ~(V17RX_FLAG_CARRIER | V17RX_FLAG_DATA));
		break;
	}
}

/* --------------------------------------------------------------------- */

/*
 * RxHdxIdleV17 -- .text 0x0a0410, 132 bytes.
 *
 * See v17fax.h.  The `out` cast is D1141's, as in `RxHdxDataV17`: the handler
 * signature spells the buffer `short *` and `DemodDataV17` spells it
 * `unsigned short *`, the pointer is passed through untouched, and the cast
 * costs no instruction.
 */
short
RxHdxIdleV17(void *modem, short *in, short *out, unsigned short *count)
{
	DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	AT_B(modem, V17RX_OBJ_RESULT_B1) &=
		(unsigned char)~V17RX_FLAG_CARRIER;
	AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_IDLE;

	if (CarrierDetectV17(modem) != 0)
		AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_CARRIER;

	if ((AT_B(modem, V17RX_OBJ_RESULT_B1) & V17RX_FLAG_CARRIER) != 0
	    && AT_S(RXS(modem), V17RXS_DEC_ERROR) <= V17RXS_DEC_ERROR_SMALL) {
		RxNextStateV17(modem);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"Decision error is small back to DATA mode !!!\n");
	}

	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * RxHdxScramV17 -- .text 0x0a04a0, 266 bytes.
 *
 * The rate ladder on the expiry path is the ONLY thing that separates this
 * from `RxHdxBridgeV17` and `RxHdxPrtcolV17` below, which are 210 bytes each
 * and byte-for-byte identical to one another.  See v17fax.h and F9444.
 *
 * THE COUNTDOWN IS LOADED UNSIGNED AND TESTED SIGNED, and both halves are the
 * object's; see `V17RXC_COUNTDOWN`.  The local is what carries the extension.
 */
short
RxHdxScramV17(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short left;

	n = DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV17(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	if (CarrierDetectV17(modem) == 0) {
		CTL_PROCESS(modem) = RxHdxErrorV17;
		AT_S(CTL(modem), V17RXC_STATE) = V17RX_STATE_ERROR;
		AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_ERROR;
		AT_B(modem, V17RX_OBJ_RESULT_B1) = (unsigned char)
			((AT_B(modem, V17RX_OBJ_RESULT_B1) | V17RX_FLAG_ERROR)
			 & ~V17RX_FLAG_CARRIER);
		return 0;
	}

	AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_CARRIER;
	AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_CARRIER;

	left = (short)(AT_US(CTL(modem), V17RXC_COUNTDOWN) - 1);
	AT_S(CTL(modem), V17RXC_COUNTDOWN) = left;
	if (left > 0)
		return 0;

	switch (AT_US(CTL(modem), V17RXC_RATE_CODE)) {
	case V17RX_RATE_7200:
		AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_RATE_7200;
		break;
	case V17RX_RATE_9600:
		AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_RATE_9600;
		break;
	case V17RX_RATE_12000:
		AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_RATE_12000;
		break;
	default:
		AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_RATE_14400;
		break;
	}

	/* SET and never cleared; only RxHdxDataV17 clears it.  D1217. */
	if (GetSNRV17(modem) <= V17RX_SNR_THRESHOLD)
		AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_LOW_SNR;

	RxNextStateV17(modem);

	return (short)n;
}

/* --------------------------------------------------------------------- */

/*
 * RxHdxBridgeV17 -- .text 0x0a05b0, 210 bytes.
 *
 * IDENTICAL TO `RxHdxPrtcolV17` BELOW, BYTE FOR BYTE, and the two bodies are
 * written out twice for that reason: a shared static helper would be one
 * symbol where the object has two, and would move the code generation of both.
 * Do not fold them.  Finding F9444.
 */
short
RxHdxBridgeV17(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short left;

	n = DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV17(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	if (CarrierDetectV17(modem) == 0) {
		CTL_PROCESS(modem) = RxHdxErrorV17;
		AT_S(CTL(modem), V17RXC_STATE) = V17RX_STATE_ERROR;
		AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_ERROR;
		AT_B(modem, V17RX_OBJ_RESULT_B1) = (unsigned char)
			((AT_B(modem, V17RX_OBJ_RESULT_B1) | V17RX_FLAG_ERROR)
			 & ~V17RX_FLAG_CARRIER);
		return 0;
	}

	AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_CARRIER;
	AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_CARRIER;

	left = (short)(AT_US(CTL(modem), V17RXC_COUNTDOWN) - 1);
	AT_S(CTL(modem), V17RXC_COUNTDOWN) = left;
	if (left > 0)
		return 0;

	if (GetSNRV17(modem) <= V17RX_SNR_THRESHOLD)
		AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_LOW_SNR;

	RxNextStateV17(modem);

	return (short)n;
}

/* --------------------------------------------------------------------- */

/*
 * RxHdxPrtcolV17 -- .text 0x0a0690, 210 bytes.  The other copy; see above.
 */
short
RxHdxPrtcolV17(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short left;

	n = DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV17(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	if (CarrierDetectV17(modem) == 0) {
		CTL_PROCESS(modem) = RxHdxErrorV17;
		AT_S(CTL(modem), V17RXC_STATE) = V17RX_STATE_ERROR;
		AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_ERROR;
		AT_B(modem, V17RX_OBJ_RESULT_B1) = (unsigned char)
			((AT_B(modem, V17RX_OBJ_RESULT_B1) | V17RX_FLAG_ERROR)
			 & ~V17RX_FLAG_CARRIER);
		return 0;
	}

	AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_CARRIER;
	AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_CARRIER;

	left = (short)(AT_US(CTL(modem), V17RXC_COUNTDOWN) - 1);
	AT_S(CTL(modem), V17RXC_COUNTDOWN) = left;
	if (left > 0)
		return 0;

	if (GetSNRV17(modem) <= V17RX_SNR_THRESHOLD)
		AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_LOW_SNR;

	RxNextStateV17(modem);

	return (short)n;
}

/* --------------------------------------------------------------------- */

/*
 * RxHdxEpochDetV17 -- .text 0x0a0770, 156 bytes.
 *
 * THE `||` IS SHORT-CIRCUIT AND THE OBJECT PROVES IT: `jle` at 0x0a07c4 jumps
 * over the `EpochDetectV17` call at 0x0a07c9 to the `RxNextStateV17` call at
 * 0x0a07d3.  An expired countdown advances the machine without asking about the
 * epoch, and `t_v17rxstate.c` drives that case with the epoch flag CLEAR so
 * that a non-short-circuiting reading would stay put.
 */
short
RxHdxEpochDetV17(void *modem, short *in, short *out, unsigned short *count)
{
	short left;

	DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	if (CarrierDetectV17(modem) == 0) {
		CTL_PROCESS(modem) = RxHdxErrorV17;
		AT_S(CTL(modem), V17RXC_STATE) = V17RX_STATE_ERROR;
		AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_ERROR;
		AT_B(modem, V17RX_OBJ_RESULT_B1) = (unsigned char)
			((AT_B(modem, V17RX_OBJ_RESULT_B1) | V17RX_FLAG_ERROR)
			 & ~V17RX_FLAG_CARRIER);
		return 0;
	}

	AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_CARRIER;
	AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_CARRIER;

	left = (short)(AT_US(CTL(modem), V17RXC_COUNTDOWN) - 1);
	AT_S(CTL(modem), V17RXC_COUNTDOWN) = left;
	if (left <= 0 || EpochDetectV17(modem) != 0)
		RxNextStateV17(modem);

	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * RxHdxStartV17 -- .text 0x0a0810, 105 bytes.
 *
 * The only handler with no error arm; see v17fax.h.  `*count = 0` is ONE
 * statement that the compiler tail-duplicated into both arms of the carrier
 * test, which is why the object stores it at 0x0a0850 and again at 0x0a086c.
 */
short
RxHdxStartV17(void *modem, short *in, short *out, unsigned short *count)
{
	AT_B(modem, V17RX_OBJ_RESULT_B1) &=
		(unsigned char)~V17RX_FLAG_CARRIER;
	AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_START;

	DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);

	if (CarrierDetectV17(modem) != 0) {
		AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_CARRIER;
		RxNextStateV17(modem);
	}

	*count = 0;

	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * V17RX_control -- .text 0x0a0880, 131 bytes.
 *
 * See v17fax.h for the derivation of `struct v17rx_ctl` and for why the
 * self-referential `V17RX_create(modem, modem)` call below is a legitimate
 * reinit-with-current-config and not the aliasing defect it first looks
 * like (finding F9470, the receive instance's head IS its own config
 * struct).
 *
 * THE MERGE IS BEHAVIOURAL, NOT A SIMPLIFICATION.  The object tests
 * `flags_0d`'s bit 4 twice -- once on each of the two paths through bit 1 --
 * and both paths converge on the same `flags_0c` tail (`jmp 0x0a08af`).
 * Written straight-line, that is exactly the order below: the `int_0008`
 * write, then the `V17RXC_INT_0008` write, then the conditional reinit,
 * then both `flags_0c` clears unconditionally.
 */
int
V17RX_control(void *modem, const struct v17rx_ctl *arg)
{
	struct v17rx_cfg *cfg = (struct v17rx_cfg *)modem;

	if (arg == NULL)
		return 0;

	cfg->int_0008 = arg->int_0004;

	AT_I(CTL(modem), V17RXC_INT_0008) =
		(arg->flags_0d & V17RXCTL_SET_CTL_INT_0008) != 0;

	if (arg->flags_0d & V17RXCTL_REINIT) {
		cfg->int_0014 = arg->int_0010;
		V17RX_create(modem, (const struct v17rx_cfg *)modem);
	}

	if (arg->flags_0c & V17RXCTL_CLEAR_STATE0)
		AT_I(RXS(modem), V17RXS_INT_0000) = 0;

	if (arg->flags_0c & V17RXCTL_CLEAR_STATE10)
		AT_I(RXS(modem), V17RXS_INT_0010) = 0;

	return 1;
}

/* --------------------------------------------------------------------- */

/*
 * V17RX_status -- .text 0x0a0910, 190 bytes.
 *
 * THE FLAGS BYTE IS FOUR STORES AND THE VALUE IT SETTLES ON IS DETERMINISTIC.
 * Reading the object's chain from the incoming byte `b`, with `x` for the
 * three state bits:
 *
 *     store 1  b & 0xfe
 *     store 2  ((b & 0xfc) | x1) & 0xfb          =  (b & 0xf8) | x1
 *     store 3  (((b & 0xf8) & 0xf3) | x3) | 0x10 = ((b & 0xf0) | x1 | x3 | 0x10)
 *     store 4  ((... & 0xdf) | x5 | 0x40) & 0x7f
 *
 * -- and the last line leaves `(b & 0x50) | x1 | x3 | x5 | 0x10 | 0x40`, in
 * which bits 4 and 6 of `b` are re-set by the two constants anyway.  So the
 * result is `0x50 | x1 | x3 | x5` and NOTHING of the caller's byte survives.
 * The three intermediate stores are the object's and are kept: each is
 * separated from the next by a load of `V17RX_OBJ_STATE`, which is reached
 * through a character type and may alias the status block, so a source that
 * assigned once could not have produced them.  They are observable only to a
 * caller that overlaps its two arguments, which is what deviation D1092
 * records.
 */
int
V17RX_status(void *modem, struct v17_status *status)
{
	unsigned char *rx;

	if (status == 0)
		return 0;

	/*
	 * `modem` stays a byte pointer for the same reason `V17TX_status`'s
	 * `params` does: it is an unmodelled block, and it is what keeps the
	 * stores above from being merged.
	 */
	rx = (unsigned char *)modem;

	status->protocol = (short)AT_US(rx, V17RX_OBJ_PROTOCOL);
	status->tx_bps = 0;
	status->rx_bps = (short)AT_US(rx, V17RX_OBJ_RX_BPS);
	status->snr_ok = (short)
		((rx[V17RX_OBJ_RESULT_B1] & V17RX_FLAG_LOW_SNR) == 0);
	status->snr = GetSNRV17(modem);
	status->short_0a = 0;
	status->short_0e = 0;
	status->short_10 = 0;
	status->short_12 = (short)AT_US(rx, V17RX_OBJ_RX_BPS);

	status->flags &= (unsigned char)~V17_STATUS_FLAG_01;
	status->flags = (unsigned char)
		((status->flags & ~V17_STATUS_FLAG_02)
		 | ((AT_B(RXS(modem), V17RXS_BYTE_001C) & V17RXS_001C_BIT0)
		    << 1));
	status->flags &= (unsigned char)~V17_STATUS_FLAG_04;
	status->flags = (unsigned char)
		((status->flags & ~V17_STATUS_FLAG_08)
		 | ((AT_I(RXS(modem), V17RXS_INT_0000) == 0) << 3));
	status->flags |= V17_STATUS_FLAG_10;
	status->flags1 &= (unsigned char)~V17_STATUS_FLAGS1_CLEAR;
	status->flags = (unsigned char)
		((status->flags & ~V17_STATUS_FLAG_20)
		 | ((AT_I(RXS(modem), V17RXS_INT_0010) == 0) << 5));
	status->flags |= V17_STATUS_FLAG_40;
	status->flags &= (unsigned char)~V17_STATUS_FLAG_80;

	return 1;
}

/* --------------------------------------------------------------------- */

void
ScrambleDataV17(void *modem, unsigned short *data, unsigned short count)
{
	void *fp;

	fp = FIELD_PTR(modem, V17TX_OBJ_FP);
	SDM_scrambler((struct fpm_sdm *)(void *)FIELD(fp, V17FP_SDM), data,
		      count);
}

/* --------------------------------------------------------------------- */

void
SeedScramblerV17(void *modem, unsigned int seed)
{
	void *fp;

	fp = FIELD_PTR(modem, V17TX_OBJ_FP);
	((struct fpm_sdm *)(void *)FIELD(fp, V17FP_SDM))->reg = seed;
}

/* --------------------------------------------------------------------- */

void
SetEncoderV17(void *modem, short which, short arg)
{
	void *fp;

	switch (which) {
	case V17_ENCODER_DIF:
		fp = FIELD_PTR(modem, V17TX_OBJ_FP);
		AT_S(fp, V17FP_ENCODER_SEL) = V17_ENCODER_DIF;
		AT_S(fp, V17FP_SMC_SHORT_06) = arg;
		break;
	case V17_ENCODER_ABS:
		/* No second value on this arm; see v17fax.h. */
		fp = FIELD_PTR(modem, V17TX_OBJ_FP);
		AT_S(fp, V17FP_ENCODER_SEL) = V17_ENCODER_ABS;
		break;
	case V17_ENCODER_TCM:
		fp = FIELD_PTR(modem, V17TX_OBJ_FP);
		AT_S(fp, V17FP_ENCODER_SEL) = V17_ENCODER_TCM;
		AT_S(fp, V17FP_SMC_SHORT_06) = arg;
		break;
	default:
		break;
	}
}

/* --------------------------------------------------------------------- */

/*
 * SMCv17_init -- .text 0x0a0a60, 87 bytes.  See v17data.h for what this
 * confirms about the five neutral fields it clears.
 */
void
SMCv17_init(void *smc, const short *cfg)
{
	if (cfg == NULL)
		cfg = SMCv17_CFG;

	memcpy(FIELD(smc, 0x00), cfg, sizeof(short[2]));
	SMC_QUAD(smc) = 0;
	SMC_STATE(smc) = 0;
	SMC_TRELLIS(smc) = 0;
	SMC_PREV(smc) = 0;
	AT_S(smc, V17FP_SMC_SHORT_10 - V17FP_SMC) = 0;
}

/* --------------------------------------------------------------------- */

/*
 * `SetTxModeV17`'s two `.rodata` tables.  See `v17data.h` for the evidence;
 * the bytes are the object's, `03 00 04 00 05 00 06 00` and `00 00 01 00`.
 */
const short V17TX_SYM_SIZE[4] = { 3, 4, 5, 6 };
const short SMCv17_CFG[2] = { 0, 1 };

/*
 * SetTxModeV17 -- .text 0x0a0ac0, 625 bytes.
 *
 * (Re)configures the transmitter's training-sequence detector, descrambler
 * and SMCv17 coder state for one of four symbol rates and returns nothing.
 * `mode` 0..3 select 16T/32/64/128-point constellations (V17TX_SYM_SIZE[mode]
 * bits/symbol: 3, 4, 5, 6) in that order; anything else writes the same
 * "unsupported" pair `V17TX_modem` writes on a short FIFO write, with one
 * extra bit -- see `V17TX_RESULT_BYTE_07` in v17fax.h.
 *
 * THE SHIFT REGISTER SURVIVES ITS OWN RE-INIT.  `SDM_init` clears
 * `struct fpm_sdm::reg` like every other field of its target, but this
 * function reads `V17FP_SDM`'s `reg` BEFORE calling it and writes the same
 * value back AFTER (`0xa0b51` / `0xa0baa`) -- so a caller re-arming the
 * transmitter for a rate change keeps the descrambler's running state rather
 * than restarting it at zero.  Reproduced, not second-guessed: nothing here
 * says why, and the object does it on every call, recognised mode or not.
 *
 * SGD_CREATE'S CFG IS SGD_CFG WITH ONLY `sym_bits` CHANGED -- no det.*
 * override the way `V17RX_create` gives its own training detector, so the
 * request threshold and pattern stay whatever `SGD_CFG` ships.  The existing
 * `struct sgd *` at `V17TXP_SGD` is reused if the caller already built one;
 * `SGD_create` allocates only when it is NULL.
 */
void
SetTxModeV17(void *modem, short mode)
{
	struct sgd_cfg sgdcfg;
	struct fpm_sdm_cfg sdmcfg;
	struct fpm_sdm *sdm;
	void *fp;
	void *prm;
	short sym_size;
	unsigned int saved_reg;

	sym_size = V17TX_SYM_SIZE[mode];

	sgdcfg = SGD_CFG;
	sgdcfg.sym_bits = sym_size;
	prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
	FIELD_PTR(prm, V17TXP_SGD) =
		SGD_create((struct sgd *)FIELD_PTR(prm, V17TXP_SGD), &sgdcfg);

	sdmcfg = SDM_CFG;
	sdmcfg.nbits = sym_size;
	sdmcfg.tap1 = 0x12;
	sdmcfg.tap2 = 0x17;

	fp = FIELD_PTR(modem, V17TX_OBJ_FP);
	sdm = (struct fpm_sdm *)(void *)FIELD(fp, V17FP_SDM);
	saved_reg = sdm->reg;
	SDM_init(sdm, &sdmcfg);
	sdm->reg = saved_reg;

	fp = FIELD_PTR(modem, V17TX_OBJ_FP);
	memcpy(FIELD(fp, V17FP_SMC), SMCv17_CFG, sizeof(SMCv17_CFG));
	AT_S(fp, V17FP_SMC_SHORT_08) = 0;
	AT_S(fp, V17FP_SMC_SHORT_06) = 0;
	AT_S(fp, V17FP_SMC_SHORT_0E) = 0;
	AT_S(fp, V17FP_SMC_SHORT_0C) = 0;
	AT_S(fp, V17FP_SMC_SHORT_10) = 0;
	AT_S(fp, V17FP_SMC_SHORT_02) = 2;
	AT_S(fp, V17FP_ENCODER_SEL) = 2;

	switch (mode) {
	case 0:
		AT_S(fp, V17FP_SMC_SHORT_12) = 1;
		AT_S(fp, V17FP_SMC) = 3;
		FIELD_PTR(fp, V17FP_SMC_IMAP) = (void *)VTBv17_IMAP16T;
		FIELD_PTR(fp, V17FP_SMC_QMAP) = (void *)VTBv17_QMAP16T;
		prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
		AT_US(prm, V17TXP_NOCARRIER_SYM) = 0x10;
		break;
	case 1:
		AT_S(fp, V17FP_SMC_SHORT_12) = 2;
		AT_S(fp, V17FP_SMC) = 2;
		FIELD_PTR(fp, V17FP_SMC_IMAP) = (void *)VTBv17_IMAP32;
		FIELD_PTR(fp, V17FP_SMC_QMAP) = (void *)VTBv17_QMAP32;
		prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
		AT_US(prm, V17TXP_NOCARRIER_SYM) = 0x20;
		break;
	case 2:
		AT_S(fp, V17FP_SMC_SHORT_12) = 3;
		AT_S(fp, V17FP_SMC) = 4;
		FIELD_PTR(fp, V17FP_SMC_IMAP) = (void *)VTBv17_IMAP64;
		FIELD_PTR(fp, V17FP_SMC_QMAP) = (void *)VTBv17_QMAP64;
		prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
		AT_US(prm, V17TXP_NOCARRIER_SYM) = 0x40;
		break;
	case 3:
		AT_S(fp, V17FP_SMC_SHORT_12) = 4;
		AT_S(fp, V17FP_SMC) = 5;
		FIELD_PTR(fp, V17FP_SMC_IMAP) = (void *)VTBv17_IMAP128;
		FIELD_PTR(fp, V17FP_SMC_QMAP) = (void *)VTBv17_QMAP128;
		prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
		AT_US(prm, V17TXP_NOCARRIER_SYM) = 0x80;
		break;
	default:
		AT_B(modem, V17TX_OBJ_RESULT) = V17TX_RESULT_BYTE_07;
		AT_B(modem, V17TX_OBJ_RESULT_B1) = (unsigned char)
			((AT_B(modem, V17TX_OBJ_RESULT_B1)
			  | V17TX_RESULT_B1_BIT1) & ~1);
		break;
	}
}

/* --------------------------------------------------------------------- */

/*
 * V17TX_modem -- .text 0x0a0e40, 182 bytes.  See v17fax.h for the two arms,
 * the budget and why `in` does not advance while `out` does.
 */
int
V17TX_modem(void *modem, unsigned short *in, short *out, unsigned short *count)
{
	void *prm;
	unsigned short taken;
	short budget;
	short total;

	prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);

	*FIELD(modem, V17TX_OBJ_RESULT_B1) &=
		(unsigned char)~V17TX_RESULT_B1_BIT1;

	if (AT_I(prm, V17TXP_INT_0008) == 0)
		taken = (unsigned short)FIFO_write(
				(struct fax_fifo *)
					FIELD_PTR(prm, V17TXP_FIFO),
				in, *count);
	else
		taken = *count;

	budget = V17TX_MODEM_BUDGET;
	total = 0;
	do {
		short got;

		prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
		got = (*(v17tx_process_fn *)(void *)
				FIELD(prm, V17TXP_PROCESS))
					(modem, in, out, &budget);

		out += got;
		total = (short)(total + got);
	} while (budget > 0);

	if (*count != taken) {
		*FIELD(modem, V17TX_OBJ_RESULT_B1) |= V17TX_RESULT_B1_BIT1;
		/*
		 * A BYTE store into the low byte of the int this function
		 * returns, which is what the object encodes
		 * (`movb $0x9,0x20(%edi)`) and is why it cannot be written
		 * through `AT_I`.
		 */
		*FIELD(modem, V17TX_OBJ_RESULT) = V17TX_RESULT_BYTE_09;
	}

	*count = (unsigned short)total;

	return AT_I(modem, V17TX_OBJ_RESULT);
}

/*
 * ---------------------------------------------------------------------------
 * TxNextStateV17 -- .text 0x0a0f00, 1,439 bytes.
 *
 * `jmp *table(,%eax,4)` on `V17TXP_STATE`, bounded `cmp $0xb`/`ja default`
 * -- a real jump table, twelve entries, `V17TX_STATE_START`..
 * `V17TX_STATE_IDLE`.  The case bodies and the state names are v17fax.h's
 * own derivation (rank 1, the twelve debug strings), reproduced here exactly
 * as `TxNextStateV29` reproduces its own seven.
 *
 * `TxHdxSCR1V17` IS INSTALLED BY TWO ARMS (`BRIDGE` and `DATA`) AND
 * `TxHdxSilenceV17` BY THREE (`START`, `TEP` and `SCR1_END`) -- neither
 * handler looks at which state led to it; both read only the SGD
 * configuration and the countdown this function seeds beside them.  See
 * v17fax.h's own state table for the full installs-what-next-STATE listing.
 *
 * THREE OF THE TWELVE ARMS RETURN DIRECTLY RATHER THAN FALLING TO THE SHARED
 * TAIL -- `SCR1`, `DATA` and `SCR1_END` clear `V17TX_OBJ_RESULT_B2`'s bit 0
 * and SET `V17TX_OBJ_RESULT_B1`'s bit 0 (`V17TX_RESULT_B1_BIT0`) with their
 * own inline code and `ret` directly; every other arm clears BOTH bits
 * through the tail every `break` reaches at the bottom of this function.
 * `QUIET_END` is the exception that still `break`s: it explicitly SETS
 * `V17TX_OBJ_RESULT_B2`'s bit and clears `V17TX_OBJ_RESULT_B1`'s -- which the
 * shared tail's own unconditional `&= ~V17TX_RESULT_B1_BIT0` reproduces for
 * free, so nothing extra is needed there.
 *
 * `V17TXP_INT_000C`'s TWO READERS ARE `ALT` (the training BUDGET: 0x26
 * against 0xba0) AND `EQCOND` (whether `BRIDGE` is installed on the way to
 * `SCR1`, or skipped) -- see the field's own comment in v17fax.h.
 *
 * `req.det` IS `SGD_CTL.det`, READ BACK OUT OF THE GLOBAL RATHER THAN
 * HARDCODED NULL, the shape F9700 established for this file's whole family
 * -- seven of the thirteen sites sgd.h's own comment counts.
 *
 * THE WRONG-COPY RITUAL (F134).  Swapping which handler `BRIDGE`'s own arm
 * installs (`TxHdxSCR1V17` for `TxHdxDataV17`) failed
 * `t_v17txcreate.c`'s `test_tx_cycle` immediately -- the DATA state was never
 * entered and the FIFO-fed cycle stalled in BRIDGE/SCR1.  Reverted; `make one
 * T=t_v17txcreate` green again.
 *
 * Finding F9912.
 */
void
TxNextStateV17(void *modem)
{
	void *prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
	short state = AT_S(prm, V17TXP_STATE);

	switch (state) {
	case V17TX_STATE_START:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_START\n");
		AT_S(prm, V17TXP_SHORT_001A) = 0x30;
		*(v17tx_process_fn *)(void *)FIELD(prm, V17TXP_PROCESS) =
			TxHdxSilenceV17;
		AT_S(prm, V17TXP_STATE) = V17TX_STATE_SILENCE;
		*FIELD(modem, V17TX_OBJ_RESULT_B2) &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_SILENCE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_SILENCE\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = 0;
			gen.word_syms = 2;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					FIELD_PTR(prm, V17TXP_SGD), &req);
		}
		SetEncoderV17(modem, 1, 0);
		prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
		AT_S(prm, V17TXP_SHORT_001A) = 0x1e0;
		*(v17tx_process_fn *)(void *)FIELD(prm, V17TXP_PROCESS) =
			TxHdxTEP_V17;
		AT_S(prm, V17TXP_STATE) = V17TX_STATE_TEP;
		*FIELD(modem, V17TX_OBJ_RESULT_B2) &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_TEP:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_TEP\n");
		AT_S(prm, V17TXP_SHORT_001A) = 0x30;
		*(v17tx_process_fn *)(void *)FIELD(prm, V17TXP_PROCESS) =
			TxHdxSilenceV17;
		AT_S(prm, V17TXP_STATE) = V17TX_STATE_QUIET;
		*FIELD(modem, V17TX_OBJ_RESULT_B2) &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_QUIET:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_QUIET\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = 0xe;
			gen.word_syms = 2;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					FIELD_PTR(prm, V17TXP_SGD), &req);
		}
		SetEncoderV17(modem, 1, 0);
		prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
		AT_S(prm, V17TXP_SHORT_001A) = 0x100;
		*(v17tx_process_fn *)(void *)FIELD(prm, V17TXP_PROCESS) =
			TxHdxABV17;
		AT_S(prm, V17TXP_STATE) = V17TX_STATE_ALT;
		*FIELD(modem, V17TX_OBJ_RESULT_B2) &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_ALT:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_ALT\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = 0xf;
			gen.word_syms = 2;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					FIELD_PTR(prm, V17TXP_SGD), &req);
		}
		prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
		AT_S(prm, V17TXP_SHORT_001A) = (short)
			((AT_I(prm, V17TXP_INT_000C) != 0) ? 0x26 : 0xba0);
		*(v17tx_process_fn *)(void *)FIELD(prm, V17TXP_PROCESS) =
			TxHdxEQCondV17;
		AT_S(prm, V17TXP_STATE) = V17TX_STATE_EQCOND;
		AT_S(prm, V17TXP_SHORT_001C) = 0;
		SeedScramblerV17(modem, 0x2ecdd5);
		*FIELD(modem, V17TX_OBJ_RESULT_B2) &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_EQCOND:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_EQCOND\n");
		if (AT_I(prm, V17TXP_INT_000C) != 0) {
			short mode = AT_S(prm, V17TXP_MODE);
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			SetTxModeV17(modem, mode);

			prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
			mode = AT_S(prm, V17TXP_MODE);
			gen.data_word =
				(unsigned short)V17TX_PATTERN_SCR1[mode];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					FIELD_PTR(prm, V17TXP_SGD), &req);

			SetEncoderV17(modem, 2, 3);
			prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
			AT_S(prm, V17TXP_SHORT_001A) = 0x30;
			*(v17tx_process_fn *)(void *)
				FIELD(prm, V17TXP_PROCESS) = TxHdxSCR1V17;
			AT_S(prm, V17TXP_STATE) = V17TX_STATE_SCR1;
		} else {
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = 0x111;
			gen.word_syms = 8;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					FIELD_PTR(prm, V17TXP_SGD), &req);

			SetEncoderV17(modem, 0, 3);
			prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
			AT_S(prm, V17TXP_SHORT_001A) = 0x40;
			*(v17tx_process_fn *)(void *)
				FIELD(prm, V17TXP_PROCESS) = TxHdxBridgeV17;
			AT_S(prm, V17TXP_STATE) = V17TX_STATE_BRIDGE;
		}
		*FIELD(modem, V17TX_OBJ_RESULT_B2) &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_BRIDGE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_BRIDGE\n");
		{
			short mode = AT_S(prm, V17TXP_MODE);
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			SetTxModeV17(modem, mode);

			prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
			mode = AT_S(prm, V17TXP_MODE);
			gen.data_word =
				(unsigned short)V17TX_PATTERN_SCR1[mode];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					FIELD_PTR(prm, V17TXP_SGD), &req);
		}
		SetEncoderV17(modem, 2, 0);
		prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
		AT_S(prm, V17TXP_SHORT_001A) = 0x30;
		*(v17tx_process_fn *)(void *)FIELD(prm, V17TXP_PROCESS) =
			TxHdxSCR1V17;
		AT_S(prm, V17TXP_STATE) = V17TX_STATE_SCR1;
		*FIELD(modem, V17TX_OBJ_RESULT_B2) &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_SCR1:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_SCR1\n");
		AT_S(prm, V17TXP_SHORT_001A) = 1;
		*(v17tx_process_fn *)(void *)FIELD(prm, V17TXP_PROCESS) =
			TxHdxDataV17;
		AT_S(prm, V17TXP_STATE) = V17TX_STATE_DATA;
		*FIELD(modem, V17TX_OBJ_RESULT_B2) &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		*FIELD(modem, V17TX_OBJ_RESULT_B1) |= V17TX_RESULT_B1_BIT0;
		return;

	case V17TX_STATE_DATA:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_DATA\n");
		{
			short mode = AT_S(prm, V17TXP_MODE);
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word =
				(unsigned short)V17TX_PATTERN_SCR1[mode];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					FIELD_PTR(prm, V17TXP_SGD), &req);
		}
		prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
		AT_S(prm, V17TXP_SHORT_001A) = 0x20;
		*(v17tx_process_fn *)(void *)FIELD(prm, V17TXP_PROCESS) =
			TxHdxSCR1V17;
		AT_S(prm, V17TXP_STATE) = V17TX_STATE_SCR1_END;
		*FIELD(modem, V17TX_OBJ_RESULT_B2) &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		*FIELD(modem, V17TX_OBJ_RESULT_B1) |= V17TX_RESULT_B1_BIT0;
		return;

	case V17TX_STATE_SCR1_END:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_SCR1_END\n");
		AT_S(prm, V17TXP_SHORT_001A) = 0x30;
		*(v17tx_process_fn *)(void *)FIELD(prm, V17TXP_PROCESS) =
			TxHdxSilenceV17;
		AT_S(prm, V17TXP_STATE) = V17TX_STATE_QUIET_END;
		*FIELD(modem, V17TX_OBJ_RESULT_B2) &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		*FIELD(modem, V17TX_OBJ_RESULT_B1) |= V17TX_RESULT_B1_BIT0;
		return;

	case V17TX_STATE_QUIET_END:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_QUIET_END\n");
		AT_S(prm, V17TXP_SHORT_001A) = 0;
		*(v17tx_process_fn *)(void *)FIELD(prm, V17TXP_PROCESS) =
			TxHdxIdleV17;
		AT_S(prm, V17TXP_STATE) = V17TX_STATE_IDLE;
		AT_I(prm, V17TXP_INT_0008) = 1;
		*FIELD(modem, V17TX_OBJ_RESULT_B2) |= V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_IDLE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_IDLE\n");
		AT_S(prm, V17TXP_SHORT_001A) = 0;
		*(v17tx_process_fn *)(void *)FIELD(prm, V17TXP_PROCESS) =
			TxHdxStartV17;
		AT_S(prm, V17TXP_STATE) = V17TX_STATE_START;
		*FIELD(modem, V17TX_OBJ_RESULT_B2) &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_DEFAULT, %d\n", state);
		*FIELD(modem, V17TX_OBJ_RESULT_B2) &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		AT_B(modem, V17TX_OBJ_RESULT) = V17TX_RESULT_BYTE_07;
		*FIELD(modem, V17TX_OBJ_RESULT_B1) = (unsigned char)
			((*FIELD(modem, V17TX_OBJ_RESULT_B1)
			  | V17TX_RESULT_B1_BIT1)
			 & ~V17TX_RESULT_B1_BIT0);
		break;
	}

	*FIELD(modem, V17TX_OBJ_RESULT_B1) &=
		(unsigned char)~V17TX_RESULT_B1_BIT0;
}

/*
 * TxHdxStartV17 -- .text 0x0a1b10, 21 bytes.  Nothing but the transition.
 */
short
TxHdxStartV17(void *modem, unsigned short *in, short *out, short *budget)
{
	TxNextStateV17(modem);
	return 0;
}

/*
 * TxHdxIdleV17 -- .text 0x0a14a0, 116 bytes.  Exactly `TxHdxIdleV21`'s and
 * `TxHdxIdleV29`'s shape: report `V17TX_STATUS_IDLE` unconditionally, then
 * spend the whole call's budget on `TxNoCarrierV17` while the FIFO is empty,
 * or hand off to `TxNextStateV17` the moment it is not.
 */
short
TxHdxIdleV17(void *modem, unsigned short *in, short *out, short *budget)
{
	void *prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
	struct fax_fifo *fifo =
		(struct fax_fifo *)FIELD_PTR(prm, V17TXP_FIFO);

	AT_B(modem, V17TX_OBJ_RESULT) = V17TX_STATUS_IDLE;

	if (fifo->count == 0) {
		unsigned short b = (unsigned short)*budget;
		short nsamples = (short)TxNoCarrierV17(modem, in, out, b);

		*budget = (short)((unsigned short)*budget - b);
		return nsamples;
	}

	TxNextStateV17(modem);
	return 0;
}

/*
 * TxHdxSilenceV17 -- .text 0x0a1a70, 158 bytes.  Installed by `START`, `TEP`
 * and `SCR1_END` alike (see `TxNextStateV17`'s own comment); every call
 * spends `min(remaining, *budget)` on `TxNoCarrierV17` and counts the
 * countdown down towards `TxNextStateV17`.
 *
 * IT DOES NOT WRITE `V17TX_OBJ_RESULT`, and neither does `TxHdxTEP_V17` --
 * MEASURED, not an omission: no `movb`/`orb`/`andb` touches the byte
 * anywhere in either function's disassembly, where every OTHER countdown
 * handler in this file (`TxHdxABV17`, `TxHdxEQCondV17`, `TxHdxBridgeV17`,
 * `TxHdxSCR1V17`) writes `V17TX_STATUS_TRAINING` on entry.  `TxHdxQuietV29`,
 * this file's closest V.29 analogue, writes its own status even on its
 * no-carrier arm -- so this is a genuine divergence from the sibling shape
 * and not a transcription slip.
 */
short
TxHdxSilenceV17(void *modem, unsigned short *in, short *out, short *budget)
{
	void *prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
	short remaining;
	unsigned short n;
	short nsamples;

	remaining = AT_S(prm, V17TXP_SHORT_001A);
	if (remaining <= 0) {
		TxNextStateV17(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	AT_S(prm, V17TXP_SHORT_001A) = (short)(remaining - n);

	nsamples = (short)TxNoCarrierV17(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/*
 * TxHdxTEP_V17 -- .text 0x0a19b0, 179 bytes.  Installed by `SILENCE`;
 * `SGD_symbol_gen` (word_syms=2, data_word=0, set by `TxNextStateV17`'s
 * SILENCE arm) straight into `ModDataV17` -- no `ScrambleDataV17`, and see
 * `TxHdxSilenceV17`'s own comment for the missing status write.
 */
short
TxHdxTEP_V17(void *modem, unsigned short *in, short *out, short *budget)
{
	void *prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
	short remaining;
	unsigned short n;
	short nsamples;

	remaining = AT_S(prm, V17TXP_SHORT_001A);
	if (remaining <= 0) {
		TxNextStateV17(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	AT_S(prm, V17TXP_SHORT_001A) = (short)(remaining - n);

	SGD_symbol_gen((struct sgd *)FIELD_PTR(prm, V17TXP_SGD), in, (short)n);
	nsamples = (short)ModDataV17(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/*
 * TxHdxABV17 -- .text 0x0a18f0, 190 bytes.  Function name AB, debug string
 * ALT -- the mismatch `TxHdxABV29`/`V29TX_STATE_ALT` already carries, one
 * modulation over.  `SGD_symbol_gen` (word_syms=2, data_word=0xe, set by
 * `TxNextStateV17`'s QUIET arm) straight into `ModDataV17`, no
 * `ScrambleDataV17` -- unlike `TxHdxSCR1V17` below, V.17's alternating
 * training dibit is unscrambled.
 */
short
TxHdxABV17(void *modem, unsigned short *in, short *out, short *budget)
{
	void *prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
	short remaining;
	unsigned short n;
	short nsamples;

	AT_B(modem, V17TX_OBJ_RESULT) = V17TX_STATUS_TRAINING;

	remaining = AT_S(prm, V17TXP_SHORT_001A);
	if (remaining <= 0) {
		TxNextStateV17(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	AT_S(prm, V17TXP_SHORT_001A) = (short)(remaining - n);

	SGD_symbol_gen((struct sgd *)FIELD_PTR(prm, V17TXP_SGD), in, (short)n);
	nsamples = (short)ModDataV17(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/*
 * TxHdxEQCondV17 -- .text 0x0a1820, 206 bytes.
 * TxHdxBridgeV17 -- .text 0x0a1750, 206 bytes.
 * TxHdxSCR1V17   -- .text 0x0a1680, 206 bytes.
 *
 * THE THREE ARE ONE BODY, COMPILED THREE TIMES -- byte for byte the same
 * instruction sequence at all three addresses (`dis.py` over each range),
 * the same shape `RxHdxBridgeV17`/`RxHdxPrtcolV17` already carry on the
 * receive side of this file: `SGD_symbol_gen` then `ScrambleDataV17` then
 * `ModDataV17`, driven by whichever `SGD_control` request the installing
 * arm of `TxNextStateV17` built immediately before.  What differs between
 * the three transitions is upstream, in `TxNextStateV17` itself; the
 * handler code does not look at which state led to it.
 */
short
TxHdxEQCondV17(void *modem, unsigned short *in, short *out, short *budget)
{
	void *prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
	short remaining;
	unsigned short n;
	short nsamples;

	AT_B(modem, V17TX_OBJ_RESULT) = V17TX_STATUS_TRAINING;

	remaining = AT_S(prm, V17TXP_SHORT_001A);
	if (remaining <= 0) {
		TxNextStateV17(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	AT_S(prm, V17TXP_SHORT_001A) = (short)(remaining - n);

	SGD_symbol_gen((struct sgd *)FIELD_PTR(prm, V17TXP_SGD), in, (short)n);
	ScrambleDataV17(modem, in, n);
	nsamples = (short)ModDataV17(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/* See TxHdxEQCondV17's own comment: the same body, a different symbol. */
short
TxHdxBridgeV17(void *modem, unsigned short *in, short *out, short *budget)
{
	void *prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
	short remaining;
	unsigned short n;
	short nsamples;

	AT_B(modem, V17TX_OBJ_RESULT) = V17TX_STATUS_TRAINING;

	remaining = AT_S(prm, V17TXP_SHORT_001A);
	if (remaining <= 0) {
		TxNextStateV17(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	AT_S(prm, V17TXP_SHORT_001A) = (short)(remaining - n);

	SGD_symbol_gen((struct sgd *)FIELD_PTR(prm, V17TXP_SGD), in, (short)n);
	ScrambleDataV17(modem, in, n);
	nsamples = (short)ModDataV17(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/*
 * See `TxHdxEQCondV17`'s own comment: the same body, a different symbol --
 * installed both by `BRIDGE` (pre-data) and by `DATA` (post-data), which is
 * `V17TX_STATE_SCR1`'s own comment in v17fax.h.
 */
short
TxHdxSCR1V17(void *modem, unsigned short *in, short *out, short *budget)
{
	void *prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
	short remaining;
	unsigned short n;
	short nsamples;

	AT_B(modem, V17TX_OBJ_RESULT) = V17TX_STATUS_TRAINING;

	remaining = AT_S(prm, V17TXP_SHORT_001A);
	if (remaining <= 0) {
		TxNextStateV17(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	AT_S(prm, V17TXP_SHORT_001A) = (short)(remaining - n);

	SGD_symbol_gen((struct sgd *)FIELD_PTR(prm, V17TXP_SGD), in, (short)n);
	ScrambleDataV17(modem, in, n);
	nsamples = (short)ModDataV17(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/*
 * TxHdxDataV17 -- .text 0x0a1520, 349 bytes.  `TxHdxDataV21`'s and
 * `TxHdxDataV29`'s shape one modulation over: an optional one-shot rate
 * report the first call after `TxHdxSCR1V17` installs it (`V17TXP_SHORT_001A`
 * seeded to 1), then the three-arm FIFO read -- satisfied and
 * underrun-with-`V17TXP_INT_0008`-clear both scramble+modulate exactly what
 * was taken (the underrun arm takes the FULL REQUESTED BUDGET rather than
 * what the FIFO gave, raising `V17TX_RESULT_B1_BIT1` and reporting
 * `V17TX_STATUS_UNDERRUN`); underrun-with-`V17TXP_INT_0008`-set modulates
 * only what the FIFO gave, LEAVES the remainder in `*budget`, and calls
 * `TxNextStateV17` before returning.
 */
short
TxHdxDataV17(void *modem, unsigned short *in, short *out, short *budget)
{
	void *prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
	unsigned short req;
	unsigned short taken;
	short nsamples;

	AT_B(modem, V17TX_OBJ_RESULT) = V17TX_STATUS_DATA;

	if (AT_S(prm, V17TXP_SHORT_001A) != 0) {
		short mode = AT_S(prm, V17TXP_MODE);

		AT_S(prm, V17TXP_SHORT_001A) = 0;

		if (mode == 1)
			AT_B(modem, V17TX_OBJ_RESULT) =
				V17TX_STATUS_DATA_RATE_9600;
		else if (mode == 0)
			AT_B(modem, V17TX_OBJ_RESULT) =
				V17TX_STATUS_DATA_RATE_7200;
		else if (mode == 2)
			AT_B(modem, V17TX_OBJ_RESULT) =
				V17TX_STATUS_DATA_RATE_12000;
		else if (mode == 3)
			AT_B(modem, V17TX_OBJ_RESULT) =
				V17TX_STATUS_DATA_RATE_14400;
		else
			AT_B(modem, V17TX_OBJ_RESULT) = V17TX_RESULT_BYTE_07;
	}

	req = (unsigned short)*budget;
	taken = (unsigned short)
		FIFO_read((struct fax_fifo *)FIELD_PTR(prm, V17TXP_FIFO),
			  in, req);

	if (req <= taken) {
		ScrambleDataV17(modem, in, taken);
		nsamples = (short)ModDataV17(modem, in, out, taken);
		*budget = (short)((unsigned short)*budget - taken);
		return nsamples;
	}

	if (AT_I(prm, V17TXP_INT_0008) != 0) {
		*budget = (short)(req - taken);
		ScrambleDataV17(modem, in, taken);
		nsamples = (short)ModDataV17(modem, in, out, taken);
		TxNextStateV17(modem);
		return nsamples;
	}

	*FIELD(modem, V17TX_OBJ_RESULT_B1) |= V17TX_RESULT_B1_BIT1;
	AT_B(modem, V17TX_OBJ_RESULT) = V17TX_STATUS_UNDERRUN;
	ScrambleDataV17(modem, in, req);
	nsamples = (short)ModDataV17(modem, in, out, req);
	*budget = (short)((unsigned short)*budget - req);

	return nsamples;
}

/*
 * ---------------------------------------------------------------------------
 * V17TX_control -- .text 0x0a1b30, 148 bytes.  See v17fax.h for the five
 * effects, the request type's derivation and the `params` identity this
 * function settles for `V17TX_status`.
 */
int
V17TX_control(void *fp, const struct v17tx_control_req *req)
{
	void *priv;
	void *block;
	struct fpm_pps *pps;
	short mode;

	if (req == 0)
		return 0;

	priv = FIELD_PTR(fp, V17TX_OBJ_PARAMS);
	block = FIELD_PTR(fp, V17TX_OBJ_FP);
	mode = AT_S(priv, V17TXP_MODE);

	pps = (struct fpm_pps *)(void *)FIELD(block, V17FP_PPS);
	pps->cfg.scale = req->scale_mul;
	pps->cfg.scale = V17TX_PPS_SCALE[mode] * req->scale_mul;

	((struct v17tx_cfg *)fp)->int_0018 = req->int_0010;
	((struct v17tx_cfg *)fp)->int_0008 = req->int_0004;

	if (req->ctl0 & V17TXCTL_CTL0_BIT2)
		AT_B(fp, 0x10) |= V17_STATUS_FLAG_04;	/* struct v17tx_cfg::
							 * int_0010's low byte */

	AT_I(priv, V17TXP_INT_0008) = 0;
	if (req->ctl1 & V17TXCTL_CTL1_BIT4)
		AT_I(priv, V17TXP_INT_0008) = 1;

	if (req->ctl1 & V17TXCTL_CTL1_BIT1) {
		V17TX_create(fp, fp);
		return 1;
	}

	return 1;
}

/* --------------------------------------------------------------------- */

int
V17TX_status(void *params, struct v17_status *status)
{
	unsigned char *p;

	if (status == 0)
		return 0;

	/*
	 * `params` stays a byte pointer: it is an unidentified block (see
	 * v17fax.h), and it is also what keeps the dead store below alive,
	 * since a character type may alias anything.
	 */
	p = (unsigned char *)params;

	status->protocol = (short)AT_US(p, 0x00);
	status->tx_bps = (short)AT_US(p, 0x02);
	status->rx_bps = 0;
	status->snr_ok = 0;
	status->snr = 0;
	status->short_0a = 0;
	status->short_0c = 0;
	status->short_10 = (short)AT_US(p, 0x02);
	status->short_12 = 0;

	/*
	 * The first of these two writes to `flags` is dead and is the
	 * object's; see v17fax.h and D1032.  It stays because the load of
	 * `p[0x10]` sits between them and may alias.
	 */
	status->flags &= (unsigned char)~V17_STATUS_FLAGS_CLEAR;
	status->flags1 &= (unsigned char)~V17_STATUS_FLAGS1_CLEAR;
	status->flags = (unsigned char)(p[0x10] & V17_STATUS_FLAG_04);

	status->int_18 = AT_I(p, 0x18);

	return 1;
}

/* --------------------------------------------------------------------- */

/*
 * DemodDataV17 -- .text 0x0a50a0, 415 bytes.  See v17fax.h for the shape, for
 * why the pre-pass copy does NOT halve where `DemodDataV29`'s does, and for
 * where `signal` comes from.
 *
 * THE STORE ORDER OF THE THREE EQUALISER ENABLES IS THE OBJECT'S.  `tilt_on`
 * is written first (0x0a51dd), then `pll_on` (0x0a51f0), then `lms_on`
 * (0x0a51fa) -- the same three fields in the same order as `DemodDataV29`.
 */
unsigned short
DemodDataV17(void *modem, short *in, unsigned short *bits, unsigned short count)
{
	int signal;
	unsigned short n;
	unsigned char *rxs;

	FPM_AGC_agc(RXS_AGC(RXS(modem)), in, count);
	/* Not the object's `%eax`; the same value.  D1091. */
	signal = RXS_AGC(RXS(modem))->signal;

	if (AT_S(CTL(modem), V17RXC_STATE) == V17RX_STATE_START) {
		short *buf = (short *)FIELD_PTR(CTL(modem), V17RXC_SCRATCH);
		unsigned short i;

		/* No `>> 1` here.  F9103. */
		for (i = 0; i < count; i++)
			buf[i] = in[i];

		FPM_TONE_kill((struct fpm_tone *)
				FIELD_PTR(CTL(modem), V17RXC_TONE),
			      (short *)FIELD_PTR(CTL(modem), V17RXC_SCRATCH),
			      (short)count);

		if (FPM_MTD_detect((struct fpm_mtd *)
					FIELD_PTR(CTL(modem), V17RXC_MTD),
				   (const short *)
					FIELD_PTR(CTL(modem), V17RXC_SCRATCH),
				   (short)count) != 0)
			return 0;
	}

	n = (unsigned short)FPM_MRF_filter(
			RXS_MRF(RXS(modem)),
			in,
			(short *)FIELD_PTR(RXS(modem), V17RXS_BUF_MRF),
			(short)count);

	rxs = RXS(modem);
	RXS_SRE(rxs)->adapt = signal & AT_I(rxs, V17RXS_INT_0004);

	n = FPM_SRE_recover(RXS_SRE(RXS(modem)),
			    (const short *)
				FIELD_PTR(RXS(modem), V17RXS_BUF_MRF),
			    (short *)FIELD_PTR(RXS(modem), V17RXS_BUF_SRE),
			    (short)n);

	if (n > V17RXS_SRE_MAX && DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("ERROR: SRE buffer violation!(%d)", n);

	rxs = RXS(modem);
	RXS_FSE(rxs)->tilt_on = 0;
	RXS_FSE(rxs)->pll_on = signal & AT_I(rxs, V17RXS_INT_0008);
	RXS_FSE(rxs)->lms_on = signal & AT_I(rxs, V17RXS_INT_0010);

	return FPM_FSE_receive(RXS_FSE(RXS(modem)),
			       (const short *)
				FIELD_PTR(RXS(modem), V17RXS_BUF_SRE),
			       bits, n);
}

/* --------------------------------------------------------------------- */

void
DescrambleDataV17(void *modem, unsigned short *data, unsigned short count)
{
	SDM_descrambler((struct fpm_sdm *)(void *)
				FIELD(RXS(modem), V17RXS_SDM),
			data, count);
}

/* --------------------------------------------------------------------- */

int
CarrierDetectV17(void *modem)
{
	unsigned char *rx;
	unsigned char *ctl;
	int r;

	rx = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_STATE);
	ctl = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_CTL);

	/* +0xd0 is read 32-bit HERE and 16-bit in QualityDetectV17. */
	r = AT_I(rx, V17RXS_AGC_SIGNAL) & AT_I(rx, V17RXS_INT_0120);

	if (AT_I(ctl, V17RXC_INT_0010) != 0
	    && AT_I(rx, V17RXS_EPOCH) != 0
	    && AT_S(rx, V17RXS_SHORT_0094) > V17RXS_0094_MIN) {
		if (AT_S(rx, V17RXS_DEC_ERROR) > V17RXS_DEC_ERROR_MAX)
			r = 0;
		else
			r &= 1;
		/*
		 * The object tests the same field twice, with the two arms
		 * merged in between; it is two `if`s in the source and not
		 * one, or the compare would have been shared.
		 */
		if (AT_S(rx, V17RXS_DEC_ERROR) > V17RXS_DEC_ERROR_MAX) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"V17 Decoder error too big..." " no carrier\n");
		}
	}

	return r;
}

/* --------------------------------------------------------------------- */

short
DataCarrierDetectV17(void *modem, const short *in, unsigned short count)
{
	unsigned char *rx;
	unsigned char *ctl;
	short r;

	rx = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_STATE);
	ctl = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_CTL);

	/* +0xd0 is read 16-bit HERE and 32-bit in CarrierDetectV17. */
	r = (short)(AT_S(rx, V17RXS_AGC_SIGNAL) & AT_I(rx, V17RXS_INT_0120));

	if (AT_S(ctl, V17RXC_SHORT_0020) == 0) {
		/*
		 * The same three gates and the same two arms as
		 * `CarrierDetectV17`, including its doubled test of the
		 * decoder error and its format string.
		 */
		if (AT_I(ctl, V17RXC_INT_0010) != 0
		    && AT_I(rx, V17RXS_EPOCH) != 0
		    && AT_S(rx, V17RXS_SHORT_0094) > V17RXS_0094_MIN) {
			if (AT_S(rx, V17RXS_DEC_ERROR) > V17RXS_DEC_ERROR_MAX)
				r = 0;
			else
				r &= 1;
			if (AT_S(rx, V17RXS_DEC_ERROR) > V17RXS_DEC_ERROR_MAX) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V17 Decoder error too big..." " no carrier\n");
			}
		}
	} else {
		if (AT_S(rx, V17RXS_DEC_ERROR) > V17RXS_DEC_ERROR_MAX
		    || (r & 1) == 0)
			AT_S(ctl, V17RXC_SHORT_002E) = 1;

		r = 1;
		if (AT_S(ctl, V17RXC_SHORT_002E) != 0) {
			short *buf;
			short i;

			buf = (short *)FIELD_PTR(ctl, V17RXC_BUF2);
			for (i = 0; i < (int)count; i++)
				buf[i] = (short)(unsigned short)in[i];

			/*
			 * The object passes FOUR arguments here, the fourth a
			 * constant 1 the callee never reads; see D1031.
			 */
			FPM_AGC_agc((struct fpm_agc *)(void *)
					FIELD(ctl, V17RXC_AGC),
				    (short *)FIELD_PTR(ctl, V17RXC_BUF2),
				    count);

			if (FPM_MTD_detect((struct fpm_mtd *)
						FIELD_PTR(ctl, V17RXC_MTD2),
					   (const short *)
						FIELD_PTR(ctl, V17RXC_BUF2),
					   (short)count) != 0)
				AT_S(ctl, V17RXC_OFFBAND) = 0;
			else
				AT_S(ctl, V17RXC_OFFBAND) = (short)
					(AT_US(ctl, V17RXC_OFFBAND) + count);

			if (AT_S(ctl, V17RXC_OFFBAND) > V17RXC_OFFBAND_MAX) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V17: V21 Carrier detected\n");
				r = 0;
			}
		}
	}

	if (AT_S(rx, V17RXS_SHORT_4FB4) != 0) {
		short rms;
		unsigned short phase;

		rms = FPM_rms(in, count);

		/*
		 * No rounding term on this one, unlike QualityDetectV17's
		 * smoothing: the object is `imul $0x32fe ; sar $0xf` and
		 * nothing else.
		 */
		if ((int)rms < ((int)AT_S(rx, V17RXS_RMS_REF)
				* V17RXS_RMS_DROP_Q15) >> 15) {
			r = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"sudden energy drop > 8[dB]," " no carrier");
		}

		phase = (unsigned short)(AT_US(rx, V17RXS_RMS_PHASE) + 1);
		if ((short)phase == V17RXS_RMS_PERIOD) {
			AT_S(rx, V17RXS_RMS_REF) = rms;
			AT_S(rx, V17RXS_RMS_PHASE) = 0;
		} else {
			AT_S(rx, V17RXS_RMS_PHASE) = (short)phase;
		}
	}

	return r;
}

/* --------------------------------------------------------------------- */

short
QualityDetectV17(void *modem)
{
	unsigned char *rx;
	short r;
	short err;
	short n;

	rx = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_STATE);

	/* +0xd0 is read 16-bit HERE and 32-bit in CarrierDetectV17. */
	r = (short)(AT_S(rx, V17RXS_AGC_SIGNAL) & AT_I(rx, V17RXS_INT_0120));

	/*
	 * Read BEFORE the diagnostic, because the object reads it before the
	 * call and no compiler may hoist a load across one.
	 */
	err = AT_S(rx, V17RXS_DEC_ERROR);

	if (r == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"V17 Dec error too big..." " unreliable data\n");
		r = V17_QUALITY_UNRELIABLE;
	}

	n = (short)AT_US(rx, V17RXS_QCOUNT);
	if (n == 0) {
		AT_S(rx, V17RXS_QAVG) = err;
		AT_S(rx, V17RXS_QCOUNT) = 1;
		return r;
	}

	if (n > V17RXS_QCOUNT_SETTLE) {
		if (n != V17RXS_QCOUNT_JUDGE)
			return r;
		if (AT_S(rx, V17RXS_QAVG)
		    <= (short)AT_US(rx, V17RXS_SHORT_4FB0))
			AT_S(rx, V17RXS_SHORT_4FB2) = 1;
	} else {
		AT_S(rx, V17RXS_QAVG) = (short)
			(((err * V17RXS_QWEIGHT_NEW + V17RXS_QROUND) >> 15)
			 + ((AT_S(rx, V17RXS_QAVG) * V17RXS_QWEIGHT_OLD
			     + V17RXS_QROUND) >> 15));
	}

	AT_S(rx, V17RXS_QCOUNT) = (short)(n + 1);
	return r;
}

/* --------------------------------------------------------------------- */

int
EpochDetectV17(void *modem)
{
	unsigned char *rx;

	rx = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_STATE);
	return AT_I(rx, V17RXS_EPOCH) != 0;
}

/* --------------------------------------------------------------------- */

short
GetSNRV17(void *modem)
{
	unsigned char *rx;

	rx = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_STATE);
	return (short)(13 - AT_US(rx, V17RXS_DEC_ERROR));
}

/* --------------------------------------------------------------------- */

void
StoreCoefV17(void *modem)
{
	unsigned char *rx;
	short *d0;
	short *d1;
	const unsigned short *s0;
	const unsigned short *s1;
	unsigned short i;

	rx = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_STATE);
	d0 = (short *)FIELD_PTR(modem, V17RX_OBJ_COEFSAVE0);
	d1 = (short *)FIELD_PTR(modem, V17RX_OBJ_COEFSAVE1);
	s0 = (const unsigned short *)FIELD_PTR(rx, V17RXS_COEF0);
	s1 = (const unsigned short *)FIELD_PTR(rx, V17RXS_COEF1);

	for (i = 0; i < V17_COEF_N; i++) {
		d0[i] = (short)s0[i];
		d1[i] = (short)s1[i];
	}

	*(short *)FIELD_PTR(modem, V17RX_OBJ_RATESAVE) =
		(short)AT_I(rx, V17RXS_RATE);
}

/* --------------------------------------------------------------------- */

void
Restore_rateV17(void *modem)
{
	unsigned char *rx;
	const short *saved;

	rx = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_STATE);
	saved = (const short *)FIELD_PTR(modem, V17RX_OBJ_RATESAVE);

	AT_I(rx, V17RXS_RATE) = *saved;
	AT_S(rx, V17RXS_SHORT_01F8) =
		(short)(AT_US(rx, V17RXS_USHORT_018C) + 5);
}

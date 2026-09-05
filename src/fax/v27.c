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
#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_S(obj, off)	(*(short *)(void *)FIELD((obj), (off)))
#define FIELD_US(obj, off)	(*(unsigned short *)(void *)FIELD((obj), (off)))
#define FIELD_I(obj, off)	(*(int *)(void *)FIELD((obj), (off)))
#define FIELD_BYTE(obj, off)	(*(unsigned char *)FIELD((obj), (off)))

/*
 * The rate index, re-read at every use because that is what the object does:
 * `V27RX_create` reloads `movswl 0x8(%reg)` seventeen times while it fills
 * five stack configurations, none of which the compiler can prove does not
 * alias the shared block.
 */
#define RATE(sh)	FIELD_S((sh), V27SH_RATE)

/* The half-duplex machine's current handler, at sh + V27SH_STATE. */
#define SH_HANDLER(sh)	(*(v27_rx_state_fn *)(void *)FIELD((sh), V27SH_STATE))

#define RX_AGC(rx)	((struct fpm_agc *)(void *)FIELD((rx), V27RX_AGC))
#define RX_MRF(rx)	((struct fpm_mrf *)(void *)FIELD((rx), V27RX_MRF))
#define RX_SRE(rx)	((struct fpm_sre *)(void *)FIELD((rx), V27RX_SRE))
#define RX_FSE(rx)	((struct fpm_fse *)(void *)FIELD((rx), V27RX_FSE))

/* ------------------------------------------------------------------ */

/*
 * V27RX_create .text 0x099660, 2210 bytes.
 *
 * The receiver's constructor.  It is three allocations, five module
 * configurations built on the stack from the library's own `*_CFG` templates,
 * and about forty scalar seeds.  `include/dsplib/v27fax.h` carries the layout
 * it settles -- in particular that the receive handle and the transmit handle
 * are two different objects (F9304) -- and `docs/findings.md` F9305 the four
 * things it says that nothing else in the object does.
 *
 * THE RATE IS RE-READ FROM THE SHARED BLOCK AT EVERY USE, seventeen times,
 * rather than kept in a local.  The object does that because the stack
 * configurations it is filling might alias the shared block as far as the
 * compiler can tell, and `RATE()` below is written the same way for the same
 * reason; every one of those reads is `movswl 0x8(%reg)`.
 *
 * THE `fresh` ARGUMENT IS TWO DIFFERENT FLAGS.  `FPM_AGC_init` on the SHARED
 * block's gain control gets "this call allocated the shared block"; the four
 * modules in the RECEIVE block get "this call allocated the handle".  They are
 * separate stack slots in the object (0x99773 reads one, 0x998b0 the other)
 * and they are not interchangeable -- see the header.
 *
 * THE SYMBOL RECOVERY'S CONTEXT POINTER GOES THROUGH A CAST, and that is the
 * one place this function cannot be spelled in the tree's own types.
 * `fpm_mrf_cfg` has `void *aux` at +0x0c and `fpm_fse_cfg` has
 * `void *reserved34` at +0x34, and `V27RX_create` gives both of them
 * `cfg->ptr_0018`.  It gives `fpm_sre_cfg` +0x34 the same pointer with the same
 * single `movl` -- but `fpm_sre.h` models that word as two `short`, `pad34`
 * and `pad36`, because nothing had ever been seen to write it.  Writing two
 * shorts would be a different instruction pair and an endianness assumption,
 * so the pointer is stored through a cast and `fpm_sre.h` is left alone: it is
 * another module's header and three sessions are live in this tree.  Whoever
 * owns it next should make those two fields one `void *aux`, at which point
 * the cast here becomes a plain assignment.  Finding F9306.
 */
void *
V27RX_create(void *modem, const struct v27rx_cfg *cfg)
{
	struct fpm_mtd_cfg mcfg;
	struct fpm_mrf_cfg rcfg;
	struct fpm_sre_cfg scfg;
	struct fpm_fse_cfg fcfg;
	struct sdmv27_cfg dcfg;
	struct fpm_fse *fse;
	struct fpm_sre *sre;
	void *sh;
	void *rx;
	void *aux;
	short *bufa;
	short *bufb;
	short rate;
	short period;
	short i;
	int fresh_sh = 0;
	int fresh_handle = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V.27 RX Create ");

	if (modem == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("New allocation\n");
		modem = sysdep_malloc(V27RXH_SIZE);
		FIELD_PTR(modem, V27_OBJ_SHARED) = 0;
		FIELD_PTR(modem, V27_OBJ_RX) = 0;
		fresh_handle = 1;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("\n");

	/* The handle's first 28 bytes ARE the configuration.  See v27fax.h. */
	if (cfg == 0)
		*(struct v27rx_cfg *)modem = V27RX_CFG;
	else
		*(struct v27rx_cfg *)modem = *cfg;

	/* ---- the shared block ---------------------------------------- */

	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	if (sh == 0) {
		sh = sysdep_malloc(V27SH_SIZE);
		FIELD_PTR(modem, V27_OBJ_SHARED) = sh;
		fresh_sh = 1;
		FIELD_PTR(sh, V27SH_MTD) = 0;
		FIELD_PTR(sh, V27SH_BUF) = sysdep_malloc(V27SH_BUF_BYTES);
		sh = FIELD_PTR(modem, V27_OBJ_SHARED);
		FIELD_PTR(sh, V27SH_MTD_V21) = 0;
	}

	FIELD_I(sh, V27SH_INT_0004) = 0;
	FIELD_S(sh, V27SH_RX_STATE) = V27RX_STATE_START;
	FIELD_US(sh, V27SH_COUNTDOWN) = 0;
	SH_HANDLER(sh) = RxHdxStartV27;
	FIELD_S(sh, V27SH_TRAIN_LONG) =
		(short)(((struct v27rx_cfg *)modem)->int_0014 == 0);

	/*
	 * The V.21 control-channel detector.  Note it is built and created
	 * BEFORE the rate is known, which is why its coefficients are the
	 * rate-independent V.21 ones.
	 */
	mcfg = FPM_MTD_CFG;
	mcfg.coeff = V21_CHAN2_MTD_COEFF;
	mcfg.tones = V27_MTD_V21_TONES;
	mcfg.ratio = V27_MTD_V21_RATIO;
	mcfg.min_level = V27_MTD_V21_MIN_LEVEL;
	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	FIELD_PTR(sh, V27SH_MTD_V21) = FPM_MTD_create(
		(struct fpm_mtd *)FIELD_PTR(sh, V27SH_MTD_V21), &mcfg);

	FPM_AGC_init((struct fpm_agc *)(void *)FIELD(
			FIELD_PTR(modem, V27_OBJ_SHARED), V27SH_AGC),
		     &AGCv27_CFG, fresh_sh);

	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	FIELD_US(sh, V27SH_V21_SAMPLES) = 0;
	FIELD_S(sh, V27SH_V21_ARMED) = 0;

	/*
	 * THE RATE, and the two numbers are V.27ter's own.  An unrecognised
	 * one is reported through the status word and then treated as 4800.
	 */
	if (((struct v27rx_cfg *)modem)->bit_rate == 2400) {
		FIELD_S(sh, V27SH_RATE) = V27SH_RATE_2400;
	} else if (((struct v27rx_cfg *)modem)->bit_rate == 4800) {
		FIELD_S(sh, V27SH_RATE) = V27SH_RATE_4800;
	} else {
		FIELD_S(sh, V27SH_RATE) = V27SH_RATE_4800;
		*FIELD(modem, V27_OBJ_STATUS_FLAGS) |= V27_STATUS_FLAG_ERROR;
		*FIELD(modem, V27_OBJ_STATUS) = V27_STATUS_DEFAULT;
	}

	/* The data-channel detector, which does depend on the rate. */
	mcfg = FPM_MTD_CFG;
	mcfg.coeff = RATE(sh) == V27SH_RATE_4800 ? V27_MTD_COEFF_4800
						 : V27_MTD_COEFF_2400;
	mcfg.tones = V27_MTD_TONES;
	mcfg.ratio = V27_MTD_RATIO;
	mcfg.min_level = V27_MTD_MIN_LEVEL;
	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	FIELD_PTR(sh, V27SH_MTD) = FPM_MTD_create(
		(struct fpm_mtd *)FIELD_PTR(sh, V27SH_MTD), &mcfg);

	/* ---- the receive block ---------------------------------------- */

	aux = ((struct v27rx_cfg *)modem)->ptr_0018;

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	if (rx == 0) {
		rx = sysdep_malloc(V27RX_BLOCK_SIZE);
		FIELD_PTR(modem, V27_OBJ_RX) = rx;
		FIELD_PTR(rx, V27RX_BUF_A) = sysdep_malloc(V27RX_BUF_A_BYTES);
		rx = FIELD_PTR(modem, V27_OBJ_RX);
		FIELD_PTR(rx, V27RX_BUF_B) = sysdep_malloc(V27RX_BUF_B_BYTES);
	}

	rcfg = FPM_MRF_CFG;
	rcfg.aux = aux;
	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	rcfg.branches = V27RX_MRF_UP[RATE(sh)];
	rcfg.decimate = V27RX_MRF_DOWN[RATE(sh)];
	rcfg.coeff = V27RX_MRF_FILT[RATE(sh)];
	rcfg.taps = V27RX_MRF_FILT_LEN[RATE(sh)];
	FPM_MRF_init(RX_MRF(FIELD_PTR(modem, V27_OBJ_RX)), &rcfg,
		     fresh_handle);

	FPM_AGC_init(RX_AGC(FIELD_PTR(modem, V27_OBJ_RX)), &AGCv27_CFG,
		     fresh_handle);

	/*
	 * The measurement block, overridden at 4800 only.  The object patches
	 * the LIVE gain control rather than the configuration it was just
	 * given, so it is a post-init fix-up and not a fifth stack config.
	 */
	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	if (RATE(sh) == V27SH_RATE_4800)
		RX_AGC(FIELD_PTR(modem, V27_OBJ_RX))->cfg.block_len =
							V27_AGC_BLOCK_4800;

	scfg = FPM_SRE_CFG;
	/*
	 * The context pointer, over `pad34`/`pad36`.  See the note above.
	 * `memcpy` rather than a pointer cast because the cast is a
	 * strict-aliasing violation the modern build warns about, and a
	 * four-byte `memcpy` of a constant size is one `movl` to both
	 * compilers -- which is what the object has (0x99919).
	 */
	memcpy(&scfg.pad34, &aux, sizeof aux);
	scfg.groups_acq = V27_SRE_GROUPS_ACQ;
	scfg.groups_trk = V27_SRE_GROUPS_TRK;
	scfg.settle = V27_SRE_SETTLE;
	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	scfg.clock_len = V27RX_SAMP_PER_BAUD[RATE(sh)];
	scfg.coeffs = V27RX_SRE_FILT_LEN[RATE(sh)];
	scfg.proto = V27RX_SRE_FILT[RATE(sh)];
	scfg.disc = V27RX_XB_COFFS[RATE(sh)];
	scfg.xclock = V27RX_XCLOCK[RATE(sh)];
	scfg.yclock = V27RX_YCLOCK[RATE(sh)];
	scfg.pll_k1 = V27RX_SRE_PLLK1[RATE(sh)];
	scfg.pll_k2 = V27RX_SRE_PLLK2[RATE(sh)];
	scfg.mag_hi = V27_SRE_MAG_HI;
	scfg.mag_lo = V27_SRE_MAG_LO;
	scfg.err_hi = V27_SRE_ERR_HI;
	scfg.err_lo = V27_SRE_ERR_LO;
	/* Read back out of the gain control initialised four lines up. */
	scfg.rms_min = (short)
		(RX_AGC(FIELD_PTR(modem, V27_OBJ_RX))->cfg.ref_level
		 / V27_SRE_RMS_MIN_DIV);
	scfg.rms_len = (short)(V27_SRE_RMS_LEN_SYMS
			       * V27RX_SAMP_PER_BAUD[RATE(sh)]);
	FPM_SRE_init(RX_SRE(FIELD_PTR(modem, V27_OBJ_RX)), &scfg,
		     fresh_handle);

	/*
	 * THE TIMING METER'S FOUR CALLER-SUPPLIED FIELDS, and `fpm_sre.h`
	 * already says they exist: "FOUR OF THESE ARE NEVER WRITTEN BY init --
	 * `ppm_step`, `ppm_scale`, `ppm_period` and `ppm_n_max` are read-only
	 * to both functions, so a caller has to fill them".  This is that
	 * caller, and it is the only one in the object.
	 *
	 * The product is narrowed to `short` BEFORE both divisions, which the
	 * object states with `movswl %di,%esi` at 0x99a67 between the multiply
	 * and the first `idiv`, and `clock_len` is read back out of the stack
	 * configuration rather than out of the object.
	 */
	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	sre = RX_SRE(FIELD_PTR(modem, V27_OBJ_RX));
	sre->ppm_step = (short)(RATE(sh) == V27SH_RATE_2400
				? V27_SRE_PPM_STEP_2400
				: V27_SRE_PPM_STEP_4800);
	period = (short)(sre->ppm_step * V27_SRE_PPM_UNIT);
	sre->ppm_period = period;
	sre->ppm_scale = (short)(V27_SRE_PPM_MILLION
				 / (period * scfg.clock_len));
	sre->ppm_n_max = (short)(V27_SRE_PPM_MILLION / period);

	fcfg = FPM_FSE_CFG;
	fcfg.reserved34 = aux;
	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	fcfg.block = (short)(RATE(sh) == V27SH_RATE_2400 ? V27_FSE_BLOCK_2400
							 : V27_FSE_BLOCK_4800);
	fcfg.interp = V27RX_SAMP_PER_BAUD[RATE(sh)];
	fcfg.icoff = V27RX_FSE_IFILT[RATE(sh)];
	fcfg.qcoff = V27RX_FSE_QFILT[RATE(sh)];
	fcfg.taps = V27RX_FSE_FILT_LEN[RATE(sh)];
	fcfg.mu[0] = V27RX_FSE_MU_TRAIN[RATE(sh)];
	fcfg.mu[1] = V27RX_FSE_MU_TRACK[RATE(sh)];
	fcfg.clk = V27RX_CRR_TABLE[RATE(sh)];
	fcfg.clk_mod = V27RX_CRR_TABLE_LEN[RATE(sh)];
	fcfg.train_sym = V27_FSE_TRAIN_SYM;
	fcfg.err_hi = V27_FSE_ERR_HI;
	fcfg.err_lo = V27_FSE_ERR_LO;
	fcfg.clk_inc = V27RX_CRR_ADJUST[RATE(sh)];
	fcfg.pll_k1 = V27RX_FSE_PLLK1[RATE(sh)];
	fcfg.pll_k2 = V27RX_FSE_PLLK2[RATE(sh)];
	fcfg.owner = FIELD(FIELD_PTR(modem, V27_OBJ_RX), V27RX_DEC);
	fcfg.decision = V27RX_epoch_det;
	FPM_FSE_init(RX_FSE(FIELD_PTR(modem, V27_OBJ_RX)), &fcfg,
		     fresh_handle);

	/* ---- the scratch buffers, the smoothers, the decoder ---------- */

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	bufa = (short *)FIELD_PTR(rx, V27RX_BUF_A);
	bufb = (short *)FIELD_PTR(rx, V27RX_BUF_B);
	/*
	 * 160 entries of each, with a `short` induction variable (`inc` then
	 * `cwtl` at 0x99bcc).  `V27RX_BUF_B` is four bytes longer than that
	 * and its last two entries are left as `sysdep_malloc` returned them.
	 */
	for (i = 0; i < V27RX_BUF_ZERO; i = (short)(i + 1)) {
		bufa[i] = 0;
		bufb[i] = 0;
	}

	FIELD_S(rx, V27RX_Q_FLAG) = 0;
	FIELD_S(rx, V27RX_Q_ACC) = 0;
	FIELD_US(rx, V27RX_Q_COUNT) = 0;

	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	rate = RATE(sh);
	if (rate == V27SH_RATE_2400)
		FIELD_US(rx, V27RX_Q_LIMIT) = V27RX_Q_LIMIT_2400;
	else if (rate == V27SH_RATE_4800)
		FIELD_US(rx, V27RX_Q_LIMIT) = V27RX_Q_LIMIT_4800;

	FIELD_US(FIELD(rx, V27RX_DEC), V27DEC_EPOCH_I0) = 0;
	FIELD_US(rx, V27RX_RMS_COUNT) = 0;
	FIELD_S(rx, V27RX_RMS_ON) = 1;
	FIELD_S(rx, V27RX_RMS_REF) = 0;
	FIELD_US(FIELD(rx, V27RX_DEC), V27DEC_EPOCH_Q0) = 0;
	FIELD_US(FIELD(rx, V27RX_DEC), V27DEC_EPOCH_I1) = 0;
	FIELD_US(FIELD(rx, V27RX_DEC), V27DEC_EPOCH_Q1) = 0;
	FIELD_US(FIELD(rx, V27RX_DEC), V27DEC_EPOCH_I2) = 0;
	FIELD_US(FIELD(rx, V27RX_DEC), V27DEC_EPOCH_Q2) = 0;

	FIELD_I(FIELD(rx, V27RX_DEC), V27DEC_EIGHT_PHASE) =
					RATE(sh) == V27SH_RATE_4800;
	FIELD_S(FIELD(rx, V27RX_DEC), V27DEC_LAST) = 0;
	FIELD_US(FIELD(rx, V27RX_DEC), V27DEC_PHASE_MASK) =
					(unsigned short)
					V27RX_DEC_PHS_MASK[RATE(sh)];
	FIELD_US(FIELD(rx, V27RX_DEC), V27DEC_TRAIN_COUNT) = 0;
	FIELD_S(FIELD(rx, V27RX_DEC), V27DEC_EPOCH_AVG) = V27DEC_MAG;
	FIELD_S(FIELD(rx, V27RX_DEC), V27DEC_ANGLE_PREV) = 0;
	FIELD_I(FIELD(rx, V27RX_DEC), V27DEC_TRAIN_SHORT) =
					FIELD_S(sh, V27SH_TRAIN_LONG) == 0;
	FIELD_US(FIELD(rx, V27RX_DEC), V27DEC_SYM_COUNT) = 0;
	FIELD_PTR(FIELD(rx, V27RX_DEC), V27DEC_PMAP) =
					V27RX_DEC_PMAP[RATE(sh)];
	FIELD_PTR(FIELD(rx, V27RX_DEC), V27DEC_ANGLES) =
					V27RX_DEC_LAST_PHASE[RATE(sh)];

	dcfg = SDMv27_CFG;
	dcfg.nbits = (unsigned short)(V27_SDM_NBITS_4800
				      - (RATE(sh) == V27SH_RATE_2400));
	SDMv27_init((struct sdmv27 *)(void *)
			FIELD(FIELD_PTR(modem, V27_OBJ_RX), V27RX_SDM),
		    &dcfg);

	/* ---- the enables, the status word and the equaliser view ------ */

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	FIELD_I(rx, 0x00) = 1;
	FIELD_I(rx, V27RX_EN_SRE_ADAPT) = 1;
	FIELD_I(rx, V27RX_EN_FSE_PLL) = 1;
	FIELD_I(rx, 0x0c) = 0;
	FIELD_I(rx, V27RX_EN_FSE_LMS) = 1;

	FIELD_I(modem, V27_OBJ_STATUS) = 0;
	*FIELD(modem, V27_OBJ_STATUS_FLAGS) |= V27_STATUS_FLAGS_SEED;
	*FIELD(modem, V27_OBJ_STATUS) = V27_STATUS_START;

	fse = RX_FSE(rx);
	FIELD_PTR(modem, V27RXH_EQ_OUT_I) = fse->out_i;
	FIELD_PTR(modem, V27RXH_EQ_OUT_Q) = fse->out_q;
	FIELD_PTR(modem, V27RXH_EQ_N_OUT) = &fse->n_out;
	FIELD_PTR(modem, V27RXH_EQ_ICOEFF) = fse->icoeff;
	FIELD_PTR(modem, V27RXH_EQ_QCOEFF) = fse->qcoeff;
	FIELD_US(modem, V27RXH_EQ_TAPS) = (unsigned short)fse->cfg.taps;

	FIELD_S(modem, V27RXH_ZERO_40) = 0;
	FIELD_S(modem, V27RXH_ZERO_4C) = 0;
	FIELD_I(modem, V27RXH_ZERO_38) = 0;
	FIELD_I(modem, V27RXH_ZERO_3C) = 0;
	FIELD_I(modem, V27RXH_ZERO_44) = 0;
	FIELD_I(modem, V27RXH_ZERO_48) = 0;

	return modem;
}

/* ------------------------------------------------------------------ */

/*
 * Release everything the receiver owns, in the object's order.
 *
 * The instance pointer is re-read before every call rather than kept in a
 * local; that is what the object encodes and it is not observable, because
 * nothing on this path writes the instance.
 */
void
V27RX_delete(void *modem)
{
	void *rx;
	void *sh;

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	FPM_FSE_free(RX_FSE(rx));

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	FPM_SRE_free(RX_SRE(rx));

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	FPM_MRF_free(RX_MRF(rx));

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	sysdep_free(FIELD_PTR(rx, V27RX_BUF_B));

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	sysdep_free(FIELD_PTR(rx, V27RX_BUF_A));

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	sysdep_free(rx);

	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	FPM_MTD_delete((struct fpm_mtd *)FIELD_PTR(sh, V27SH_MTD));

	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	sysdep_free(FIELD_PTR(sh, V27SH_BUF));

	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	FPM_MTD_delete((struct fpm_mtd *)FIELD_PTR(sh, V27SH_MTD_V21));

	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	sysdep_free(sh);

	sysdep_free(modem);
}

/* ------------------------------------------------------------------ */

/*
 * V27RX_epoch_det .text 0x099fe0, 303 bytes.
 *
 * The equaliser's FIRST slicer, and the one `V27RX_create` installs.  It
 * decides nothing: `angle` and `mag` are never read or written, the return is
 * 0xffff on every path, and what it does instead is watch the equaliser's own
 * output for a jump.
 *
 * THE ARITHMETIC, and every part of it is the object's:
 *
 *   d = (short)( (((short)(I1 - i))^2 + ((short)(Q1 - q))^2) >> 15
 *              + (((short)(I2 - I0))^2 + ((short)(Q2 - Q0))^2) >> 15 )
 *   e = (short)((i*i + q*q) >> 15)
 *
 * -- two squared distances ACROSS TWO SYMBOLS each (the new point against the
 * one two shifts back, and the two-back point against the one-back point),
 * plus the new point's own energy.  Then, once the counter has passed its
 * limit, `avg = (31*avg >> 5) + (e >> 5)` and the epoch is declared when
 * `d > 4*avg`.
 *
 * THE SHIFTS ARE SHIFTS AND NOT DIVISIONS, which the object settles: a signed
 * `/32` compiles to `test`/`add $31`/`sar`, and there is no such correction
 * anywhere in these 303 bytes.  `31*avg` is `shl $5` then `sub`, GCC's own
 * expansion of the multiply.
 *
 * THE SIX HISTORY SLOTS ARE READ `movzwl` AND THE TWO SAMPLES `movswl`, AND
 * ONLY ONE OF THOSE IS FORCED.  The samples are SQUARED, and (-1)^2 and
 * (65535)^2 are not the same 32-bit number, so their sign extension changes
 * the answer and is forced -- `t_v27fax.c` measures it.  The history's does
 * NOT: every slot is read and immediately differenced, and the difference is
 * narrowed straight back to `short`, so nothing above bit 15 survives.  That
 * is finding F614's free case exactly, and under finding F7803 the extension
 * follows the DECLARED TYPE of what is loaded -- which is why the slots are
 * `unsigned short` here.  The test asserts that variant separates NOTHING
 * rather than pretending to measure it.
 *
 * `n_out` IS READ SIGNED, and that IS forced: its 32-bit result indexes
 * `out_i[]` and `out_q[]`, which is CLAUDE.md's forced case exactly
 * (`movswl 0x5e(%esi),%edx` at 0x9a006).  `fpm_fse.h` models the field as
 * `unsigned short` from `FPM_FSE_receive`, so the two functions did not share
 * a declaration and the `short` local below is what the object encodes here.
 * A negative `n_out` therefore indexes BEFORE both arrays; `t_v27fax.c` drives
 * that deliberately rather than assuming it cannot happen (finding F8587's
 * rule: a subscript has to be planted, because a blob-against-blob run agrees
 * on any out-of-bounds neighbour it reads).
 *
 * THE COUNTER RESET IS `0xffff` FOLLOWED BY THE UNCONDITIONAL INCREMENT, so
 * the handover leaves `V27DEC_TRAIN_COUNT` at zero for `V27RX_eq_train` to
 * count up from.  Writing it as a plain `= 0` before the increment would be a
 * different instruction and a different value on the taken arm; the object's
 * spelling is kept.
 */
unsigned short
V27RX_epoch_det(struct fpm_fse *state, short *angle, short *mag)
{
	void *dec = state->cfg.owner;
	short n = state->n_out;
	short i, q;
	short di, dq, ei, eq;
	short avg;
	int limit;
	int d, e;

	(void)angle;
	(void)mag;

	FIELD_US(dec, V27DEC_SYM_COUNT) =
		(unsigned short)(FIELD_US(dec, V27DEC_SYM_COUNT) + 1);

	limit = FIELD_I(dec, V27DEC_TRAIN_SHORT) ? V27EPOCH_SYMS_SHORT
						 : V27EPOCH_SYMS_LONG;

	i = state->out_i[n];
	q = state->out_q[n];

	/*
	 * The new point against the one-back slot, and the two-back slot
	 * against the newest one -- both of which are two symbols apart once
	 * the shift below has happened.
	 */
	di = (short)(FIELD_US(dec, V27DEC_EPOCH_I1) - i);
	dq = (short)(FIELD_US(dec, V27DEC_EPOCH_Q1) - q);
	ei = (short)(FIELD_US(dec, V27DEC_EPOCH_I2)
		     - FIELD_US(dec, V27DEC_EPOCH_I0));
	eq = (short)(FIELD_US(dec, V27DEC_EPOCH_Q2)
		     - FIELD_US(dec, V27DEC_EPOCH_Q0));

	d = (short)(((di * di + dq * dq) >> 15)
		    + ((ei * ei + eq * eq) >> 15));

	FIELD_US(dec, V27DEC_EPOCH_I2) = FIELD_US(dec, V27DEC_EPOCH_I1);
	FIELD_US(dec, V27DEC_EPOCH_Q2) = FIELD_US(dec, V27DEC_EPOCH_Q1);
	FIELD_US(dec, V27DEC_EPOCH_I1) = FIELD_US(dec, V27DEC_EPOCH_I0);
	FIELD_US(dec, V27DEC_EPOCH_Q1) = FIELD_US(dec, V27DEC_EPOCH_Q0);
	FIELD_US(dec, V27DEC_EPOCH_I0) = (unsigned short)i;
	FIELD_US(dec, V27DEC_EPOCH_Q0) = (unsigned short)q;

	e = (short)((i * i + q * q) >> 15);

	if (FIELD_S(dec, V27DEC_TRAIN_COUNT) > limit) {
		avg = (short)(((FIELD_S(dec, V27DEC_EPOCH_AVG)
				* V27EPOCH_AVG_WEIGHT) >> V27EPOCH_AVG_SHIFT)
			      + (e >> V27EPOCH_AVG_SHIFT));
		FIELD_S(dec, V27DEC_EPOCH_AVG) = avg;

		if (d > avg * V27EPOCH_TRIGGER) {
			FIELD_US(dec, V27DEC_TRAIN_COUNT) = 0xffff;
			state->lms_force = 1;
			state->cfg.decision = V27RX_eq_train;
		}
	} else {
		FIELD_S(dec, V27DEC_EPOCH_AVG) = (short)e;
	}

	FIELD_US(dec, V27DEC_TRAIN_COUNT) =
		(unsigned short)(FIELD_US(dec, V27DEC_TRAIN_COUNT) + 1);

	return 0xffff;
}

/* ------------------------------------------------------------------ */

/*
 * Release the transmitter, in the object's order.
 *
 * The instance pointer is kept in a register across all seven calls -- it is
 * the only thing in this function that is not re-read -- while both BLOCK
 * pointers are loaded afresh before each use, exactly as `V27RX_delete` does.
 * Neither is observable, because nothing on this path writes the instance.
 *
 * `FPM_PPS_free` is given a second argument the object does not declare; see
 * `v27fax.h` and F8870.  Not reproduced.
 */
void
V27TX_delete(void *modem)
{
	void *tx;
	void *src;

	tx = FIELD_PTR(modem, V27_OBJ_TX);
	FPM_PPS_free((struct fpm_pps *)(void *)FIELD(tx, V27TX_PPS));

	/*
	 * The SYMBOL RING's buffer, not a scratch allocation of the
	 * transmitter's.  The object frees `*(tx + 0x10)`, and 0x10 is
	 * `V27TX_RING + offsetof(struct fpm_smc_ring, sym)`; see v27fax.h and
	 * F9121 for why there is no room for a separate field there.
	 */
	tx = FIELD_PTR(modem, V27_OBJ_TX);
	sysdep_free(((struct fpm_smc_ring *)(void *)
			FIELD(tx, V27TX_RING))->sym);

	tx = FIELD_PTR(modem, V27_OBJ_TX);
	sysdep_free(tx);

	src = FIELD_PTR(modem, V27_OBJ_TXDATA);
	FIFO_delete((struct fax_fifo *)FIELD_PTR(src, V27TXD_FIFO));

	src = FIELD_PTR(modem, V27_OBJ_TXDATA);
	SGD_delete((struct sgd *)FIELD_PTR(src, V27TXD_SGD));

	src = FIELD_PTR(modem, V27_OBJ_TXDATA);
	sysdep_free(src);

	sysdep_free(modem);
}

/* ------------------------------------------------------------------ */

/*
 * The equaliser's TRAINING slicer, and the handover to the running one.
 *
 * The decision it takes is a binary one: the training symbol alternates
 * between two constellation points half a revolution apart, so the only
 * question is whether the measured angle has crossed to the other side.  The
 * phase difference is folded into [-V27DEC_HALF_TURN, +V27DEC_HALF_TURN] --
 * NOT into [0, V27DEC_PHASE_FULL] as `V27RX_decision` folds it -- and the
 * reference index is advanced by half the constellation when what is left
 * exceeds a quarter of a revolution.
 *
 * THE FOLD'S TWO TESTS ARE ASYMMETRIC AND THAT IS THE OBJECT'S: the high side
 * is `> 0x4000` and the low side is `< -0x4000`, so exactly +0x4000 folds and
 * exactly -0x4000 does not.  Both comparisons are 16-bit and signed
 * (`cmp $0x4000,%dx` / `jle`, `cmp $0xc000,%dx` / `jge`), which is why `diff`
 * is a `short` and not an `int`; a 32-bit fold would not wrap the same way.
 *
 * THE EMPTY LOOP IS THE OBJECT'S TOO, AND WHAT THE AUTHOR PUT IN IT CANNOT BE
 * RECOVERED.  At 0x9a1ce the object loads `state->cfg.taps`, runs a counted
 * loop with a `short` induction variable (`inc %eax` then `cwtl`) and NO body
 * at all, and falls through.  Whatever was written there produced no
 * instructions, so there is no preimage to derive: the loop is reproduced for
 * its control flow and it is not observable.  Finding F9117.
 *
 * `mu_sel` IS CLEARED ON EVERY CALL and set to 1 only on the handover, so the
 * equaliser trains on `cfg.mu[0]` and runs on `cfg.mu[1]`.
 *
 * THE COUNTER IS RE-READ FROM MEMORY for the limit test rather than reused
 * from the increment (`mov %dx,0x1c(%ecx)` ... `cmp %di,0x1c(%ecx)`), and that
 * is forced: `state->mu_sel` is a `short` and the counter is an
 * `unsigned short`, so the store between them may alias and the compiler has
 * to reload.  Written as a re-read for that reason.
 */
unsigned short
V27RX_eq_train(struct fpm_fse *state, short *angle, short *mag)
{
	void *dec = state->cfg.owner;
	const short *tbl;
	unsigned short count;
	short step;
	short diff;
	short err;
	short a;
	short limit;
	short i;

	/* Saturating, and it restarts at half scale -- V27RX_decision's. */
	count = (unsigned short)(FIELD_US(dec, V27DEC_SYM_COUNT) + 1);
	if (count == V27DEC_PHASE_FULL)
		FIELD_US(dec, V27DEC_SYM_COUNT) = V27DEC_PHASE_FULL / 2;
	else
		FIELD_US(dec, V27DEC_SYM_COUNT) = count;

	/* Half the constellation: 4 of 8, or 2 of 4. */
	step = FIELD_I(dec, V27DEC_EIGHT_PHASE) ? 4 : 2;

	diff = (short)(*angle - FIELD_S(dec, V27DEC_ANGLE_PREV));
	if (diff > V27DEC_HALF_TURN)
		diff = (short)(diff + V27DEC_PHASE_FULL);
	if (diff < -V27DEC_HALF_TURN)
		diff = (short)(diff - V27DEC_PHASE_FULL);

	*mag = V27DEC_MAG;

	err = diff < 0 ? (short)-diff : diff;
	if (err > V27DEC_QUARTER_TURN)
		FIELD_S(dec, V27DEC_LAST) =
			(short)((FIELD_S(dec, V27DEC_LAST) + step)
				& FIELD_US(dec, V27DEC_PHASE_MASK));

	tbl = (const short *)FIELD_PTR(dec, V27DEC_ANGLES);
	a = tbl[FIELD_S(dec, V27DEC_LAST)];
	limit = FIELD_I(dec, V27DEC_TRAIN_SHORT) ? V27DEC_TRAIN_SYMS_SHORT
						 : V27DEC_TRAIN_SYMS_LONG;
	*angle = a;
	FIELD_S(dec, V27DEC_ANGLE_PREV) = a;
	FIELD_US(dec, V27DEC_TRAIN_COUNT) =
		(unsigned short)(FIELD_US(dec, V27DEC_TRAIN_COUNT) + 1);

	state->mu_sel = 0;

	if (FIELD_S(dec, V27DEC_TRAIN_COUNT) >= limit) {
		for (i = 0; i < state->cfg.taps; i = (short)(i + 1)) {
			/* No body in the object.  See the note above. */
		}
		state->lms_force = 0;
		state->mu_sel = 1;
		state->cfg.decision = V27RX_decision;
	}

	return 0xffff;
}

/* ------------------------------------------------------------------ */

/*
 * The slicer.
 *
 * THE INITIAL `best` IS THE SAME EXPRESSION AS THE LOOP'S, applied to a
 * virtual phase of half a revolution -- which is the largest distance any
 * legal phase can be at.  The object writes it out with the constant folded:
 * `lea 0x8000(%ebx),%eax` where the arithmetic says subtract, and
 * `mov $0xffff8000,%eax; sub %ebx,%eax` where it says 0x8000 - diff.  Both
 * are correct because the result is immediately narrowed to `short` and
 * 0x8000 is its own negation modulo 2^16, so the compiler was free to choose
 * either sign of the constant.  It is written here the way the arithmetic
 * reads.
 *
 * THE DISTANCE IS A `short` AND THAT IS FORCED: the object narrows every one
 * of them with `cwtl` before the comparison, so a table entry far enough from
 * `diff` wraps and compares as its own opposite.  A `short` local is the only
 * spelling that reproduces it, and `t_v27fax` reaches the wrap deliberately.
 *
 * The two extensions on the table load -- `movzwl` for the subtraction and
 * `movswl` for the comparison, from the same 16 bits -- are finding F614's
 * free case: the subtraction's result is narrowed, so only its low half is
 * ever read.  The COMPARISON is the one that is forced, and it is signed.
 */
unsigned short
V27RX_decision(struct fpm_fse *state, short *angle, short *mag)
{
	void *dec = state->cfg.owner;
	const short *tbl = (const short *)FIELD_PTR(dec, V27DEC_ANGLES);
	const short *pmap = (const short *)FIELD_PTR(dec, V27DEC_PMAP);
	short n = FIELD_I(dec, V27DEC_EIGHT_PHASE) ? 8 : 4;
	unsigned short count;
	short best, bi, k;
	int diff;

	diff = *angle - tbl[FIELD_S(dec, V27DEC_LAST)];

	/* Saturating, and it restarts at half scale rather than at zero. */
	count = (unsigned short)(FIELD_US(dec, V27DEC_SYM_COUNT) + 1);
	if (count == V27DEC_PHASE_FULL)
		FIELD_US(dec, V27DEC_SYM_COUNT) = V27DEC_PHASE_FULL / 2;
	else
		FIELD_US(dec, V27DEC_SYM_COUNT) = count;

	/* One revolution, taken as [0, V27DEC_PHASE_FULL]. */
	if (diff < 0)
		diff += V27DEC_PHASE_FULL;
	if (diff > V27DEC_PHASE_FULL)
		diff -= V27DEC_PHASE_FULL;

	best = diff >= V27DEC_PHASE_FULL
	     ? (short)(diff - V27DEC_PHASE_FULL)
	     : (short)(V27DEC_PHASE_FULL - diff);
	bi = 0;

	for (k = 0; k < n; k = (short)(k + 1)) {
		short d;

		d = diff >= tbl[k] ? (short)(diff - tbl[k])
				   : (short)(tbl[k] - diff);
		if (d < best) {
			bi = k;
			best = d;
		}
	}

	FIELD_S(dec, V27DEC_LAST) = (short)((FIELD_S(dec, V27DEC_LAST) + bi)
					    & FIELD_US(dec, V27DEC_PHASE_MASK));
	*mag = V27DEC_MAG;
	*angle = tbl[FIELD_S(dec, V27DEC_LAST)];

	return (unsigned short)pmap[bi];
}

/* ------------------------------------------------------------------ */

/*
 * Drive the half-duplex receive state handler until it stops asking for more.
 *
 * A DO-WHILE, not a while: the handler is entered once even when the caller
 * offers no samples at all, which is how a state that only has to emit gets
 * to run.  The accumulated output count is a `short` -- `add %edx,%eax` then
 * `cwtl` on every iteration -- and it is written back over the caller's input
 * count.
 */
int
V27RX_modem(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short total = 0;

	*FIELD(modem, V27_OBJ_STATUS_FLAGS) &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG_ERROR;

	n = *count;
	do {
		short before = (short)n;
		short got;

		got = (*(v27_rx_state_fn *)(void *)
			FIELD(FIELD_PTR(modem, V27_OBJ_SHARED), V27SH_STATE))
				(modem, in, out, count);
		n = *count;
		out += got;
		/*
		 * The samples the handler took.  `before` is signed and `n` is
		 * not, which the object states by extending the same sixteen
		 * bits two different ways -- `movswl %cx` at the top of the
		 * loop and `movzwl %cx` here.  Both are used at full width by
		 * the subtraction, so neither is free.
		 */
		in += before - n;
		total = (short)(total + got);
	} while (n != 0);

	*count = (unsigned short)total;

	return FIELD_I(modem, V27_OBJ_STATUS);
}

/* ------------------------------------------------------------------ */

/*
 * RxHdxDataV27 .text 0x0a2ce0, 226 bytes.
 *
 * The DATA state of the receive machine.  See v27fax.h for the flag-bit
 * enumeration and for `V27SH_INT_0004`.
 *
 * THE `out` CASTS ARE THE RECONSTRUCTION'S AND THE OBJECT CANNOT SEE THEM: the
 * handler family's signature (`v27_rx_state_fn`) spells the third argument
 * `short *` and both callees declare theirs `unsigned short *`, and the
 * pointer is passed through untouched either way.  Deviation D1141.
 *
 * THE RESULT IS A `?:` AND THE OBJECT SPELLS IT BRANCHLESSLY -- `cmp $0x2,%ax`
 * / `setne` / `movzbl` / `neg` / `and`, which is `n & -(q != 2)`.  The `?:` is
 * what is written, for the reason `src/fax/v17.c` gives at the identical site.
 *
 * ONE CODEGEN DIFFERENCE IS EXPECTED HERE AND IT IS DECLARED RATHER THAN
 * FITTED.  The object widens `n` for the `DescrambleDataV27` call with
 * `movzwl %ax,%edi`; `DescrambleDataV27`'s third parameter is `short` in this
 * tree (from its own `movswl` of that parameter, F9118), so the implicit
 * conversion below must widen with `movswl` instead.  Declaring that parameter
 * `unsigned short` and letting the SINGLE narrowing happen inside the callee
 * would reproduce both sites -- but it is a change to a written, tested
 * function's signature that cannot be checked without the period compiler, so
 * it is left for whoever has one.  The two spellings are behaviourally
 * identical for every `n`, because the conversion to `short` happens either
 * way before `SDMv27_descrambler` sees it.  Finding F9237, deviation D1140.
 */
short
RxHdxDataV27(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short r;

	*FIELD(modem, V27_OBJ_STATUS_FLAGS) |= V27_STATUS_FLAG_CARRIER;
	*FIELD(modem, V27_OBJ_STATUS) = V27_STATUS_DATA;

	if (DataCarrierDetectV27(modem, in, *count) == 0
	    || FIELD_I(FIELD_PTR(modem, V27_OBJ_SHARED), V27SH_INT_0004) != 0) {
		*FIELD(modem, V27_OBJ_STATUS_FLAGS) &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG_CARRIER;
		*count = 0;
		return 0;
	}

	n = DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV27(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	r = (short)(QualityDetectV27(modem) != V27_QUALITY_UNRELIABLE ? n : 0);

	*FIELD(modem, V27_OBJ_STATUS_FLAGS) &=
		(unsigned char)~(unsigned char)V27_STATUS_FLAG_LOW_SNR;
	if (GetSNRV27(modem) <= V27RX_SNR_THRESHOLD)
		*FIELD(modem, V27_OBJ_STATUS_FLAGS) |= V27_STATUS_FLAG_LOW_SNR;

	return r;
}

/* ------------------------------------------------------------------ */

/*
 * RxHdxErrorV27 .text 0x0a2dd0, 59 bytes.
 *
 * The ERROR state: raise the flag, demodulate anyway so the filters keep their
 * history, and consume the block.  `DemodDataV27`'s return is DISCARDED.
 */
short
RxHdxErrorV27(void *modem, short *in, short *out, unsigned short *count)
{
	*FIELD(modem, V27_OBJ_STATUS_FLAGS) |= V27_STATUS_FLAG_ERROR;

	DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	return 0;
}

/* ------------------------------------------------------------------ */

/*
 * RxNextStateV27 .text 0x0a2e10, 518 bytes.
 *
 * The transition table of the half-duplex receive machine, and the only thing
 * in the object that writes `V27SH_STATE`, `V27SH_RX_STATE` or
 * `V27SH_COUNTDOWN` outside `V27RX_create`.
 *
 * THE STATE NAMES ARE THE AUTHOR'S OWN AND SO ARE THEIR NUMBERS.  The switch
 * goes through the jump table at `.rodata:0xc31c`, whose five entries are
 * 0x0a2e7a, 0x0a2ea7, 0x0a2ed9, 0x0a2f17 and 0x0a2f4b in that order, and each
 * of those five arms opens by printing its own name out of `.rodata.str1.1`.
 * So the pairing of NAME to NUMBER is read off the table and the strings
 * together, not guessed from the order the handlers run in.  See v27fax.h at
 * `V27SH_RX_STATE`, and finding F9300.
 *
 * EVERY ARM WRITES BOTH FLAG BITS, ALWAYS OPPOSITE WAYS.  Four arms clear
 * `V27_STATUS_FLAG2_IDLE` and clear `V27_STATUS_FLAG_DATA`; the two that hand
 * over to `RxHdxDataV27` clear IDLE and SET DATA; the one that hands over to
 * `RxHdxIdleV27` does the reverse.  That is what makes the two bits a pair
 * rather than two independent flags.
 *
 * THE ONLY ARM THAT TOUCHES THE DSP IS EPOCH_DET -> PROTOCOL, and what it does
 * there is step the AGC's two smoother coefficients on by one `short` each:
 * `addl $0x2,0x74(%eax)` and `addl $0x2,0x78(%eax)`, where `%eax` is the
 * receive block, 0x74 is `V27RX_AGC + offsetof(struct fpm_agc_cfg, alpha)` and
 * 0x78 is the same for `beta`.  Both are POINTERS -- `fpm_agc.h` says so, and
 * `AGCv27_CFG` supplies them by address -- so 2 is one element and not a
 * magnitude.  The PROTOCOL -> DATA arm then freezes the gain outright.
 * Finding F9303.
 *
 * THE DEFAULT ARM INSTALLS NOTHING.  It leaves the handler and the state
 * number alone, so a machine that reaches `V27RX_STATE_ERROR` and is advanced
 * from there stays in `RxHdxErrorV27` for ever; all the arm does is report
 * `V27_STATUS_DEFAULT` and rewrite the flags.
 *
 * IT RETURNS NOTHING.  All four callers discard `%eax` and the arms leave
 * different values in it, so there is no return to reproduce.
 */
void
RxNextStateV27(void *modem)
{
	void *sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	void *rx;
	unsigned short blocks;
	unsigned char flags;
	int state = FIELD_S(sh, V27SH_RX_STATE);

	switch (state) {
	case V27RX_STATE_START:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27RX_STATE_START\n");
		FIELD_US(sh, V27SH_COUNTDOWN) = V27SH_EPOCH_DET_BLOCKS;
		SH_HANDLER(sh) = RxHdxEpochDetV27;
		FIELD_S(sh, V27SH_RX_STATE) = V27RX_STATE_EPOCH_DET;
		*FIELD(modem, V27_OBJ_STATUS_FLAGS2) &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG2_IDLE;
		*FIELD(modem, V27_OBJ_STATUS_FLAGS) &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG_DATA;
		break;

	case V27RX_STATE_EPOCH_DET:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27RX_STATE_EPOCH_DET\n");
		/*
		 * Four countdowns, one selector each.  The object spells the
		 * short pair branchlessly (`setne` on `rate == 1` then `inc`,
		 * 0x0a2f81) and the long pair as a two-constant `?:`
		 * (0x0a2ebf); both are GCC's own forms of what is written
		 * here, and the four values differ so the two spellings
		 * cannot be confused for one another.
		 */
		if (FIELD_S(sh, V27SH_TRAIN_LONG) == 0)
			blocks = FIELD_S(sh, V27SH_RATE) == V27SH_RATE_4800
			       ? V27SH_PROTOCOL_SHORT_4800
			       : V27SH_PROTOCOL_SHORT_2400;
		else
			blocks = FIELD_S(sh, V27SH_RATE) == V27SH_RATE_4800
			       ? V27SH_PROTOCOL_LONG_4800
			       : V27SH_PROTOCOL_LONG_2400;
		FIELD_US(sh, V27SH_COUNTDOWN) = blocks;

		rx = FIELD_PTR(modem, V27_OBJ_RX);
		SH_HANDLER(sh) = RxHdxPrtcolV27;
		FIELD_S(sh, V27SH_RX_STATE) = V27RX_STATE_PROTOCOL;
		*FIELD(modem, V27_OBJ_STATUS_FLAGS2) &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG2_IDLE;
		*FIELD(modem, V27_OBJ_STATUS_FLAGS) &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG_DATA;
		/* One `short` along each, not two of anything.  F9303. */
		RX_AGC(rx)->cfg.alpha++;
		RX_AGC(rx)->cfg.beta++;
		break;

	case V27RX_STATE_PROTOCOL:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27RX_STATE_PROTOCOL\n");
		FPM_AGC_Freeze(RX_AGC(FIELD_PTR(modem, V27_OBJ_RX)));
		/* Re-read across the call; the object does (0x0a2ef4). */
		sh = FIELD_PTR(modem, V27_OBJ_SHARED);
		FIELD_US(sh, V27SH_COUNTDOWN) = 0;
		SH_HANDLER(sh) = RxHdxDataV27;
		FIELD_S(sh, V27SH_RX_STATE) = V27RX_STATE_DATA;
		*FIELD(modem, V27_OBJ_STATUS_FLAGS) |= V27_STATUS_FLAG_DATA;
		*FIELD(modem, V27_OBJ_STATUS_FLAGS2) &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG2_IDLE;
		break;

	case V27RX_STATE_DATA:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27RX_STATE_DATA\n");
		SH_HANDLER(sh) = RxHdxIdleV27;
		FIELD_S(sh, V27SH_RX_STATE) = V27RX_STATE_IDLE;
		FIELD_US(sh, V27SH_COUNTDOWN) = 0;
		/*
		 * The gate `RxHdxDataV27` refuses to demodulate through.
		 * Nothing in the object SETS it, and this is the only thing
		 * that clears it -- exactly as `RxNextStateV21` clears
		 * `hdx->int_0000` on the same transition.
		 */
		FIELD_I(sh, V27SH_INT_0004) = 0;
		*FIELD(modem, V27_OBJ_STATUS_FLAGS2) |= V27_STATUS_FLAG2_IDLE;
		*FIELD(modem, V27_OBJ_STATUS_FLAGS) &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG_DATA;
		break;

	case V27RX_STATE_IDLE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27RX_STATE_IDLE\n");
		SH_HANDLER(sh) = RxHdxDataV27;
		FIELD_S(sh, V27SH_RX_STATE) = V27RX_STATE_DATA;
		*FIELD(modem, V27_OBJ_STATUS_FLAGS) |= V27_STATUS_FLAG_DATA;
		*FIELD(modem, V27_OBJ_STATUS_FLAGS2) &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG2_IDLE;
		*FIELD(modem, V27_OBJ_STATUS) = (unsigned char)
			(FIELD_S(sh, V27SH_RATE) == V27SH_RATE_2400
			 ? V27_STATUS_ENTER_DATA_2400
			 : V27_STATUS_ENTER_DATA_4800);
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27RX_DEFAULT, %d\n", state);
		flags = *FIELD(modem, V27_OBJ_STATUS_FLAGS);
		*FIELD(modem, V27_OBJ_STATUS_FLAGS2) &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG2_IDLE;
		*FIELD(modem, V27_OBJ_STATUS) = V27_STATUS_DEFAULT;
		*FIELD(modem, V27_OBJ_STATUS_FLAGS) = (unsigned char)
			((flags | V27_STATUS_FLAG_ERROR)
			 & (unsigned char)~(unsigned char)
				(V27_STATUS_FLAG_CARRIER | V27_STATUS_FLAG_DATA));
		break;
	}
}

/* ------------------------------------------------------------------ */

/*
 * RxHdxIdleV27 .text 0x0a3020, 132 bytes.
 *
 * The IDLE state.  It keeps demodulating -- the filters have to keep their
 * history -- reports `V27_STATUS_IDLE` every block, and goes back to DATA once
 * the carrier is up AND the equaliser's error has come back below
 * `V27RX_MSE_IDLE_OK`.
 *
 * THE RESTART TEST READS THE FLAG BYTE, NOT THE CALL.  The object clears
 * `V27_STATUS_FLAG_CARRIER`, calls `CarrierDetectV27`, raises the flag again
 * if it answered, and then tests the FLAG (`testb $0x20,0x1d(%esi)` at
 * 0x0a3069) rather than the value it just computed.  The two agree here
 * because nothing between them writes the byte, and the object's spelling is
 * what is reproduced.  `RxHdxIdleV21` does the clear-call-set half the same
 * way and has no second half.
 *
 * THE DEBUG PRINT IS AFTER THE TRANSITION, not before it, which is the reverse
 * of `RxNextStateV27`'s five arms -- and it is the author's own words for what
 * just happened: "Decision error is small back to DATA mode !!!\n", from
 * `.rodata.str1.4 + 0x12b9c`.
 */
short
RxHdxIdleV27(void *modem, short *in, short *out, unsigned short *count)
{
	DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	*FIELD(modem, V27_OBJ_STATUS_FLAGS) &=
		(unsigned char)~(unsigned char)V27_STATUS_FLAG_CARRIER;
	*FIELD(modem, V27_OBJ_STATUS) = V27_STATUS_IDLE;

	if (CarrierDetectV27(modem))
		*FIELD(modem, V27_OBJ_STATUS_FLAGS) |= V27_STATUS_FLAG_CARRIER;

	if ((*FIELD(modem, V27_OBJ_STATUS_FLAGS) & V27_STATUS_FLAG_CARRIER)
	    && RX_FSE(FIELD_PTR(modem, V27_OBJ_RX))->mse <= V27RX_MSE_IDLE_OK) {
		RxNextStateV27(modem);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Decision error is small back to" " DATA mode !!!\n");
	}

	return 0;
}

/* ------------------------------------------------------------------ */

/*
 * RxHdxPrtcolV27 .text 0x0a30b0, 224 bytes.
 *
 * The PROTOCOL state: the equaliser is trained, so the block is demodulated
 * AND descrambled, and the machine sits here for `V27SH_COUNTDOWN` blocks
 * before handing over to `RxHdxDataV27`.
 *
 * IT IS THE ONLY HANDLER OF THE FIVE THAT REPORTS A NON-ZERO SAMPLE COUNT, and
 * only on the block that hands over: `movswl %bp,%eax` at 0x0a3179 against a
 * literal zero on both other paths.  So everything it descrambled before the
 * countdown expired is thrown away by `V27RX_modem`, and the last block's
 * worth is not.  That asymmetry is the object's and is not obviously
 * intentional; it is reproduced because it is there.  Deviation D1160.
 *
 * THE COUNTDOWN IS DECREMENTED UNSIGNED AND TESTED SIGNED -- `movzwl`, `dec`,
 * `test %cx,%cx`, `jle` -- so a countdown of zero on entry wraps to 0xffff and
 * hands over immediately rather than counting 65535 blocks.  Both readings are
 * reproduced; see v27fax.h at `V27SH_COUNTDOWN`.
 *
 * THE `DescrambleDataV27` WIDENING IS THE SAME DECLARED DIFFERENCE
 * `RxHdxDataV27` CARRIES: the object widens `n` with `movzwl` and this tree's
 * `DescrambleDataV27` takes a `short`, so the implicit conversion widens with
 * `movswl` instead.  Behaviourally identical for every `n`.  F9237, D1140.
 */
short
RxHdxPrtcolV27(void *modem, short *in, short *out, unsigned short *count)
{
	void *sh;
	unsigned short n;
	unsigned short left;

	n = DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV27(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	if (CarrierDetectV27(modem) == 0) {
		unsigned char flags;

		sh = FIELD_PTR(modem, V27_OBJ_SHARED);
		SH_HANDLER(sh) = RxHdxErrorV27;
		FIELD_S(sh, V27SH_RX_STATE) = V27RX_STATE_ERROR;
		flags = *FIELD(modem, V27_OBJ_STATUS_FLAGS);
		*FIELD(modem, V27_OBJ_STATUS) = V27_STATUS_ERROR;
		*FIELD(modem, V27_OBJ_STATUS_FLAGS) = (unsigned char)
			((flags | V27_STATUS_FLAG_ERROR)
			 & (unsigned char)~(unsigned char)V27_STATUS_FLAG_CARRIER);
		return 0;
	}

	*FIELD(modem, V27_OBJ_STATUS_FLAGS) |= V27_STATUS_FLAG_CARRIER;
	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	*FIELD(modem, V27_OBJ_STATUS) = V27_STATUS_TRAINING;

	left = (unsigned short)(FIELD_US(sh, V27SH_COUNTDOWN) - 1);
	FIELD_US(sh, V27SH_COUNTDOWN) = left;
	if ((short)left > 0)
		return 0;

	*FIELD(modem, V27_OBJ_STATUS) = (unsigned char)
		(FIELD_S(sh, V27SH_RATE) == V27SH_RATE_2400
		 ? V27_STATUS_ENTER_DATA_2400
		 : V27_STATUS_ENTER_DATA_4800);
	RxNextStateV27(modem);

	return (short)n;
}

/* ------------------------------------------------------------------ */

/*
 * RxHdxEpochDetV27 .text 0x0a3190, 156 bytes.
 *
 * The EPOCH_DET state.  It is `RxHdxPrtcolV27` without the descramble, without
 * the sample count and without the status handover, plus one extra way out:
 * `EpochDetectV27` can advance the machine before the countdown does.
 *
 * THE TWO EXITS ARE `||`, NOT `&&`, AND THE ORDER MATTERS.  The object
 * decrements the countdown FIRST, unconditionally, and only calls
 * `EpochDetectV27` when what is left is still positive (0x0a31e4).  So the
 * detector is not consulted on the block that exhausts the countdown, and the
 * countdown advances the machine whether or not the epoch was ever found.
 *
 * `EpochDetectV27` IS TESTED IN SIXTEEN BITS (`test %ax,%ax` at 0x0a31ee)
 * although it is declared to return `int` here and returns 0 or 1.  The cast
 * below is what the object encodes and it cannot separate any value that
 * function can produce.
 */
short
RxHdxEpochDetV27(void *modem, short *in, short *out, unsigned short *count)
{
	void *sh;
	unsigned short left;

	DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	if (CarrierDetectV27(modem) == 0) {
		unsigned char flags;

		sh = FIELD_PTR(modem, V27_OBJ_SHARED);
		SH_HANDLER(sh) = RxHdxErrorV27;
		FIELD_S(sh, V27SH_RX_STATE) = V27RX_STATE_ERROR;
		flags = *FIELD(modem, V27_OBJ_STATUS_FLAGS);
		*FIELD(modem, V27_OBJ_STATUS) = V27_STATUS_ERROR;
		*FIELD(modem, V27_OBJ_STATUS_FLAGS) = (unsigned char)
			((flags | V27_STATUS_FLAG_ERROR)
			 & (unsigned char)~(unsigned char)V27_STATUS_FLAG_CARRIER);
		return 0;
	}

	*FIELD(modem, V27_OBJ_STATUS_FLAGS) |= V27_STATUS_FLAG_CARRIER;
	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	*FIELD(modem, V27_OBJ_STATUS) = V27_STATUS_TRAINING;

	left = (unsigned short)(FIELD_US(sh, V27SH_COUNTDOWN) - 1);
	FIELD_US(sh, V27SH_COUNTDOWN) = left;
	if ((short)left > 0 && (short)EpochDetectV27(modem) == 0)
		return 0;

	RxNextStateV27(modem);

	return 0;
}

/* ------------------------------------------------------------------ */

/*
 * RxHdxStartV27 .text 0x0a3230, 101 bytes.
 *
 * The START state, and the one `V27RX_create` installs (0x0996f9).  It is the
 * smallest of the five: lower the carrier flag, report `V27_STATUS_START`,
 * demodulate, and advance as soon as `CarrierDetectV27` answers.
 *
 * THE FLAG IS LOWERED AND NEVER RAISED HERE.  Unlike `RxHdxIdleV27`, which
 * clears it and then puts it back from the same call, this handler only ever
 * clears it -- the arm that finds a carrier goes straight to
 * `RxNextStateV27`, whose START arm does not touch that bit either.  So the
 * carrier flag stays down until `RxHdxEpochDetV27` raises it on the next
 * block.
 *
 * `*count` IS ZEROED AFTER THE BRANCH ON BOTH PATHS (0x0a3270 and 0x0a3288),
 * so `RxNextStateV27` runs while the caller's count still holds the block
 * length.  Nothing on that path reads it; the ordering is the object's.
 */
short
RxHdxStartV27(void *modem, short *in, short *out, unsigned short *count)
{
	*FIELD(modem, V27_OBJ_STATUS_FLAGS) &=
		(unsigned char)~(unsigned char)V27_STATUS_FLAG_CARRIER;
	*FIELD(modem, V27_OBJ_STATUS) = V27_STATUS_START;

	DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);

	if (CarrierDetectV27(modem))
		RxNextStateV27(modem);

	*count = 0;

	return 0;
}

/* ------------------------------------------------------------------ */

/*
 * Report whether the caller supplied a receive status block.
 *
 * The block is not filled: eleven bytes, and the first argument is not read.
 */
int
V27RX_status(void *rx, void *status)
{
	(void)rx;
	return status != 0;
}

/*
 * V27RX_control .text 0x0a32a0, 125 bytes.
 *
 * See v27fax.h for the request's own layout.  `int_0008` and the
 * `V27SH_INT_0004` reset are unconditional; the mask byte's two disables and
 * the flags byte's force/reinit are each gated on their own bit.
 */
int
V27RX_control(void *rx, void *req)
{
	void *sh;
	void *rxb;
	unsigned char flags;
	unsigned char mask;

	if (req == 0)
		return 0;

	((struct v27rx_cfg *)rx)->int_0008 = FIELD_I(req, V27RXCTL_INT_0004);
	sh = FIELD_PTR(rx, V27_OBJ_SHARED);
	FIELD_I(sh, V27SH_INT_0004) = 0;

	flags = FIELD_BYTE(req, V27RXCTL_FLAGS);
	if (flags & V27RXCTL_FLAGS_FORCE_NOCARRIER)
		FIELD_I(sh, V27SH_INT_0004) = 1;
	if (flags & V27RXCTL_FLAGS_REINIT)
		V27RX_create(rx, (const struct v27rx_cfg *)rx);

	mask = FIELD_BYTE(req, V27RXCTL_MASK);
	rxb = FIELD_PTR(rx, V27_OBJ_RX);
	if (mask & V27RXCTL_MASK_DISABLE_00)
		FIELD_I(rxb, V27RX_EN_00) = 0;
	if (mask & V27RXCTL_MASK_DISABLE_FSE_LMS)
		FIELD_I(rxb, V27RX_EN_FSE_LMS) = 0;

	return 1;
}

/*
 * V27TX_control .text 0x0a3e30, 148 bytes.
 *
 * See v27fax.h for the request's own layout.  The pulse shaper's gain and
 * `int_0008`/`int_0018` are unconditional; `V27TX_HANDLE_FLAGS`'s bit and
 * `int_0008`'s force/reinit are each gated on their own bit -- `V27RX_
 * control`'s own shape, transmit side.
 */
int
V27TX_control(void *modem, void *req)
{
	void *prm;
	struct fpm_pps *pps;
	short rate;
	unsigned char mask;
	unsigned char flags;

	if (req == 0)
		return 0;

	prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
	pps = (struct fpm_pps *)(void *)
		FIELD(FIELD_PTR(modem, V27_OBJ_TX), V27TX_PPS);
	rate = FIELD_S(prm, V27TXP_RATE);

	pps->cfg.scale = FIELD_I(req, V27TXCTL_SCALE_MUL) *
		V27TX_PPS_SCALE[rate];

	((struct v27tx_cfg *)modem)->int_0018 = FIELD_I(req, V27TXCTL_INT_0010);
	((struct v27tx_cfg *)modem)->int_0008 = FIELD_I(req, V27TXCTL_INT_0004);

	mask = FIELD_BYTE(req, V27TXCTL_MASK);
	if (mask & V27TXCTL_MASK_HANDLE_FLAG_04)
		*FIELD(modem, V27TX_HANDLE_FLAGS) |= 0x04;

	FIELD_I(prm, V27TXP_INT_0008) = 0;

	flags = FIELD_BYTE(req, V27TXCTL_FLAGS);
	if (flags & V27TXCTL_FLAGS_FORCE_INT_0008)
		FIELD_I(prm, V27TXP_INT_0008) = 1;
	if (flags & V27TXCTL_FLAGS_REINIT)
		V27TX_create(modem, (const struct v27tx_cfg *)modem);

	return 1;
}

/*
 * Fill the caller's status block from the transmitter's.
 *
 * THE TWO WRITES TO +0x14 ARE BOTH THE OBJECT'S, and the first is not dead.
 * The second reads the SOURCE's +0x10, and nothing tells the compiler the two
 * blocks do not overlap -- so the store has to happen first, and it is
 * observable exactly when they do.  See D1033.
 */
int
V27TX_status(const void *tx, void *status)
{
	unsigned char flags;

	if (status == 0)
		return 0;

	FIELD_US(status, V27STAT_PROTOCOL) = FIELD_US(tx, V27STAT_PROTOCOL);
	FIELD_US(status, V27STAT_TX_BPS) = FIELD_US(tx, V27STAT_TX_BPS);
	FIELD_US(status, V27STAT_RX_BPS) = 0;
	FIELD_US(status, V27STAT_QUALITY) = 0;
	FIELD_US(status, V27STAT_ZERO_08) = 0;
	FIELD_US(status, V27STAT_ZERO_0A) = 0;
	FIELD_US(status, V27STAT_ZERO_0C) = 0;
	/*
	 * The SOURCE IS READ AGAIN, not reused: `movzwl 0x2(%ebx),%eax` at
	 * a3f0b after the store at a3efb.  Observable only if the two blocks
	 * overlap, and what the compiler was forced to encode.
	 */
	FIELD_US(status, V27STAT_WORD_10) = FIELD_US(tx, V27STAT_TX_BPS);
	FIELD_US(status, V27STAT_ZERO_12) = 0;

	/*
	 * V.17, V.21 and V.29 spell this `flags &= ~(BIT0 | BIT1)`.  V.27ter
	 * SETS bit 0 instead of clearing it, which changes what the last line
	 * of the function produces.  Finding F8866, deviation D1033.
	 */
	flags = (unsigned char)(*FIELD(status, V27STAT_FLAGS)
				| V27STAT_FLAGS_BIT0);
	*FIELD(status, V27STAT_FLAGS) =
			(unsigned char)(flags & (unsigned char)~V27STAT_FLAGS_BIT1);
	*FIELD(status, V27STAT_FLAGS2) &= (unsigned char)~V27STAT_FLAGS2_BIT0;
	*FIELD(status, V27STAT_FLAGS) =
			(unsigned char)((flags & V27STAT_FLAGS_BIT0)
					| (*FIELD(tx, V27TX_HANDLE_FLAGS)
					   & V27STAT_FLAGS_FROM_TX));

	FIELD_I(status, V27STAT_WORD_18) = FIELD_I(tx, V27STAT_WORD_18);

	return 1;
}

/* ------------------------------------------------------------------ */

/*
 * One block through the receive chain.
 *
 * THE TONE TEST HAS NO COPY AND NO NOTCH, WHERE V.17 AND V.29 HAVE BOTH.
 * Both of those halve the caller's block into a scratch buffer, run
 * `FPM_TONE_kill` over the copy and hand the copy to the detector; V.27ter
 * hands `FPM_MTD_detect` the CALLER's buffer, which by then is the buffer
 * `FPM_AGC_agc` has rewritten in place.  There is no copy loop in the object's
 * 331 bytes -- no loop at all -- and no `FPM_TONE_kill` relocation.  Checked
 * against the bytes rather than inferred from the size, because it is the one
 * structural difference between the three demodulators.  Finding F9115.
 *
 * A DETECTION ABANDONS THE WHOLE CALL: it returns 0 having run the gain
 * control and nothing else, so the caller's samples are left gain-controlled
 * and the resampler, the recoverer and the equaliser do not advance.
 *
 * THE TWO WIDENINGS OF `count` ARE DIFFERENT AND BOTH ARE FORCED.  The gain
 * control takes an `unsigned short` and gets `movzwl`; the tone detector takes
 * a `short` and gets `movswl` (0x0a599f).  The resampler's is `movzwl` again
 * only because the register already held that value and the callee's parameter
 * is 16 bits wide -- F614's free case, not a third reading.
 *
 * THE THREE EQUALISER FLAGS ARE STORED IN THE OBJECT'S ORDER, which is
 * `tilt_on`, `lms_on`, `pll_on` (0x0a5a24, 0x0a5a2f, 0x0a5a37).  That is NOT
 * V.17's or V.29's order, and the difference is kept because a store order is
 * evidence about one function and does not carry to its siblings.
 */
unsigned short
DemodDataV27(void *modem, short *in, unsigned short *bits, unsigned short count)
{
	int signal;
	unsigned short n;
	unsigned short m;
	void *rx;
	void *sh;

	FPM_AGC_agc(RX_AGC(FIELD_PTR(modem, V27_OBJ_RX)), in, count);
	/* Not the object's `%eax`; the same value.  D1094. */
	signal = RX_AGC(FIELD_PTR(modem, V27_OBJ_RX))->signal;

	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	/*
	 * "the machine is still in START".  The object compares the field
	 * against zero in memory (`cmpw $0x0,0x10(%edx)` at 0x0a5994), so it
	 * does not say which extension the author's declaration carried and
	 * this site is unchanged by the rename.  See v27fax.h and F9300.
	 */
	if (FIELD_S(sh, V27SH_RX_STATE) == V27RX_STATE_START) {
		if (FPM_MTD_detect((struct fpm_mtd *)FIELD_PTR(sh, V27SH_MTD),
				   in, (short)count) != 0)
			return 0;
	}

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	n = (unsigned short)FPM_MRF_filter(RX_MRF(rx), in,
					   (short *)FIELD_PTR(rx, V27RX_BUF_A),
					   (short)count);

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	RX_SRE(rx)->adapt = signal & FIELD_I(rx, V27RX_EN_SRE_ADAPT);
	m = FPM_SRE_recover(RX_SRE(rx),
			    (const short *)FIELD_PTR(rx, V27RX_BUF_A),
			    (short *)FIELD_PTR(rx, V27RX_BUF_B),
			    (short)n);

	if (m > V27RX_SRE_MAX && DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("ERROR: SRE buffer violation(%d)", m);

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	RX_FSE(rx)->tilt_on = 0;
	RX_FSE(rx)->lms_on = signal & FIELD_I(rx, V27RX_EN_FSE_LMS);
	RX_FSE(rx)->pll_on = signal & FIELD_I(rx, V27RX_EN_FSE_PLL);

	return FPM_FSE_receive(RX_FSE(rx),
			       (const short *)FIELD_PTR(rx, V27RX_BUF_B),
			       bits, m);
}

/* ------------------------------------------------------------------ */

/*
 * The receiver's descrambler, and its sibling `ScrambleDataV27` at the foot of
 * this file.  One indirection, one addition and a tail call each; the only
 * work either does is sign-extending `count`.
 */
void
DescrambleDataV27(void *modem, unsigned short *data, short count)
{
	SDMv27_descrambler((struct sdmv27 *)(void *)
				FIELD(FIELD_PTR(modem, V27_OBJ_RX), V27RX_SDM),
			   data, count);
}

/* ------------------------------------------------------------------ */

/*
 * Carrier, and the two things that can take it away.
 *
 * The V.21 arm exists because a fax receiver that has lost the image carrier
 * must notice the sending end going back to the control channel.  It is armed
 * -- and stays armed -- the moment the equaliser's error goes bad or carrier
 * drops, and from then on every block is copied out, gain-controlled and run
 * past the V.21 tone detector.  0x4ff samples without a hit is what the
 * detector needs to be believed.
 */
short
DataCarrierDetectV27(void *modem, short *samples, unsigned short count)
{
	void *rx = FIELD_PTR(modem, V27_OBJ_RX);
	void *sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	void *dec = FIELD(rx, V27RX_DEC);
	short cd;

	cd = (short)(RX_AGC(rx)->signal & RX_SRE(rx)->active);

	if (FIELD_US(sh, V27SH_V21_WATCH) == 0) {
		if (FIELD_S(dec, V27DEC_SYM_COUNT) > V27RX_DEC_SETTLED) {
			if (RX_FSE(rx)->mse > V27RX_MSE_NO_CARRIER)
				cd = 0;
			else
				cd &= 1;
		}
		if (RX_FSE(rx)->mse > V27RX_MSE_NO_CARRIER && DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27 Decoder error too big..." " no carrier\n");
	} else {
		short i;

		if (RX_FSE(rx)->mse > V27RX_MSE_NO_CARRIER || (cd & 1) == 0)
			FIELD_S(sh, V27SH_V21_ARMED) = 1;

		cd = 1;
		if (FIELD_S(sh, V27SH_V21_ARMED) != 0) {
			short *buf = (short *)FIELD_PTR(sh, V27SH_BUF);

			for (i = 0; i < (int)count; i = (short)(i + 1))
				buf[i] = samples[i];

			FPM_AGC_agc((struct fpm_agc *)(void *)
					FIELD(sh, V27SH_AGC), buf, count);

			if (FPM_MTD_detect((struct fpm_mtd *)
						FIELD_PTR(sh, V27SH_MTD_V21),
					   buf, (short)count) != 0)
				FIELD_US(sh, V27SH_V21_SAMPLES) = 0;
			else
				FIELD_US(sh, V27SH_V21_SAMPLES) =
					(unsigned short)
					(FIELD_US(sh, V27SH_V21_SAMPLES)
					 + count);

			if (FIELD_S(sh, V27SH_V21_SAMPLES)
			    > V27SH_V21_TIMEOUT) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V27: V21 Carrier detected\n");
				cd = 0;
			}
		}
	}

	if (FIELD_S(rx, V27RX_RMS_ON) != 0) {
		short level = FPM_rms(samples, count);
		unsigned short n;

		if (level < (short)((FIELD_S(rx, V27RX_RMS_REF)
				     * V27RX_RMS_DROP_Q15) >> 15)) {
			cd = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("sudden energy drop >" " 8[dB], no carrier");
		}

		n = (unsigned short)(FIELD_US(rx, V27RX_RMS_COUNT) + 1);
		if (n == 2) {
			FIELD_S(rx, V27RX_RMS_REF) = level;
			FIELD_US(rx, V27RX_RMS_COUNT) = 0;
		} else {
			FIELD_US(rx, V27RX_RMS_COUNT) = n;
		}
	}

	return cd;
}

/* ------------------------------------------------------------------ */

/*
 * Grade the data, and keep the running error average the grade will be read
 * off later.
 *
 * The average runs for exactly 0x32 blocks and then stops; the block after it
 * -- and only that one -- compares the result against the limit and latches
 * the verdict.  The counter keeps incrementing past that, so the latch fires
 * once.
 */
short
QualityDetectV27(void *modem)
{
	void *rx = FIELD_PTR(modem, V27_OBJ_RX);
	short mse = RX_FSE(rx)->mse;
	short verdict;
	unsigned short n;

	verdict = (short)(RX_AGC(rx)->signal & RX_SRE(rx)->active);
	if (verdict == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27 Dec error too big..." " unreliable data\n");
		verdict = V27_QUALITY_UNRELIABLE;
	}

	n = FIELD_US(rx, V27RX_Q_COUNT);
	if (n == 0) {
		FIELD_S(rx, V27RX_Q_ACC) = mse;
		FIELD_US(rx, V27RX_Q_COUNT) = 1;
	} else if ((short)n <= 0x31) {
		FIELD_S(rx, V27RX_Q_ACC) = (short)
			(((FIELD_S(rx, V27RX_Q_ACC) * 0x7333 + 0x4000) >> 15)
			 + ((mse * 0xccd + 0x4000) >> 15));
		FIELD_US(rx, V27RX_Q_COUNT) = (unsigned short)(n + 1);
	} else if ((short)n == 0x32) {
		if (FIELD_S(rx, V27RX_Q_ACC) <= FIELD_S(rx, V27RX_Q_LIMIT))
			FIELD_S(rx, V27RX_Q_FLAG) = 1;
		FIELD_US(rx, V27RX_Q_COUNT) = (unsigned short)(n + 1);
	}

	return verdict;
}

/* ------------------------------------------------------------------ */

int
EpochDetectV27(void *modem)
{
	void *rx = FIELD_PTR(modem, V27_OBJ_RX);

	return RX_FSE(rx)->lms_force != 0;
}

int
CarrierDetectV27(void *modem)
{
	void *rx = FIELD_PTR(modem, V27_OBJ_RX);

	return RX_SRE(rx)->active & RX_AGC(rx)->signal;
}

short
GetSNRV27(void *modem)
{
	(void)modem;			/* never read; see v27fax.h */
	return 10;
}

/* ------------------------------------------------------------------ */

/*
 * One block through the transmit chain.
 *
 * `count` GOES TO BOTH CALLS UNCHANGED, and it is not the same unit in each:
 * `SMC_encoder` takes data words and `FPM_PPS_filter` takes symbols.  The
 * object holds the caller's count in `%ebx` across both and stores it into
 * `0xc(%esp)` twice, so there is no conversion to reproduce.
 *
 * The ring is loaded from `tx + 0x08` twice, once per call, and `tx` itself is
 * re-read from the instance in between -- the reload pattern of every function
 * in this file.
 */
unsigned short
ModDataV27(void *modem, const unsigned short *bits, short *samples,
	   unsigned short count)
{
	void *tx;

	tx = FIELD_PTR(modem, V27_OBJ_TX);
	SMC_encoder((struct fpm_smc *)(void *)FIELD(tx, V27TX_SMC),
		    (struct fpm_smc_ring *)(void *)FIELD(tx, V27TX_RING),
		    bits, count);

	tx = FIELD_PTR(modem, V27_OBJ_TX);
	return FPM_PPS_filter((struct fpm_pps *)(void *)FIELD(tx, V27TX_PPS),
			      (struct fpm_smc_ring *)(void *)
					FIELD(tx, V27TX_RING),
			      samples, count);
}

/* ------------------------------------------------------------------ */

/*
 * The transmitter's scrambler.  `DescrambleDataV27` above is the same shape
 * over a different block: the scrambler lives in the TRANSMITTER's, at
 * tx + V27TX_SDM, and the descrambler in the receiver's.
 */
void
ScrambleDataV27(void *modem, unsigned short *data, short count)
{
	SDMv27_scrambler((struct sdmv27 *)(void *)
				FIELD(FIELD_PTR(modem, V27_OBJ_TX), V27TX_SDM),
			 data, count);
}

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
	1,			/* int_000c -- the PPS gain multiplier        */
	0,			/* flags_0010                                */
	0,			/* short_0012                                */
	1,			/* int_0014 -- the FIFO's own size multiplier */
	0,			/* int_0018 -- V27TXP_TRAIN_LONG's source     */
	0,			/* int_001c -- FPM_PPS_CFG's aux              */
};

/*
 * SetScramblerV27 .text 0x0a5e90, 92 bytes.
 *
 * Reseed the scrambler for the rate `TxNextStateV27`'s EQCOND arm has just
 * settled on.  `sdmv27.h` already carries this derivation -- written before
 * this file reconstructed the function that needed it -- and this is that
 * derivation typed out: `reg` is saved across `SDMv27_init` and put back by
 * hand, because `SDMv27_init` has no separate reset and would otherwise drop
 * the shift register's running state on every rate-driven reseed.
 */
void
SetScramblerV27(void *modem)
{
	struct sdmv27_cfg cfg;
	void *prm;
	struct sdmv27 *sdm;
	short rate;
	unsigned short reg;

	cfg = SDMv27_CFG;

	prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
	rate = FIELD_S(prm, V27TXP_RATE);
	cfg.nbits = (unsigned short)V27TX_SDM_NUM_BITS[rate];

	sdm = (struct sdmv27 *)(void *)FIELD(FIELD_PTR(modem, V27_OBJ_TX),
					     V27TX_SDM);
	reg = sdm->reg;

	SDMv27_init(sdm, &cfg);

	sdm = (struct sdmv27 *)(void *)FIELD(FIELD_PTR(modem, V27_OBJ_TX),
					     V27TX_SDM);
	sdm->reg = reg;
}

/*
 * V27TX_create .text 0x09a330, 1165 bytes.
 *
 * The transmitter's constructor: the handle, the data-source block (a FIFO
 * and an `sgd`), and the private DSP block (the symbol ring, the scrambler,
 * the symbol coder and the pulse shaper).  Same four-part shape as
 * `V29TX_create`; see its own comment in v29.c for the codegen-level notes
 * (`fresh`'s two roles, the rate re-read seventeen times over, `aux`'s
 * `(void *)(long)` cast) that hold here without change.
 *
 * THE SYMBOL RING ALLOCATES ONLY `sym`.  Unlike V.29's private ring (its own
 * struct, `i`/`q` both `sysdep_malloc`'d), V.27ter's ring is a
 * `struct fpm_smc_ring` and `ModDataV27` runs the pulse shaper in MAPPED
 * mode, so `i`/`q` are zeroed and never allocated -- `V27TX_delete` frees
 * exactly the one buffer this function allocates.  Finding F9751.
 *
 * THE RING'S LENGTH IS COMPUTED, `V27TX_FRMSIZE[rate] + 2`, matching
 * `V29TX_create`'s own `+2` over its FRMSIZE-equivalent lookup.
 *
 * `V27TXP_TRAIN_LONG` IS SEEDED ONCE, `(params->int_0018 == 0)`, the same
 * `sete` idiom `V27RX_create` uses for `V27SH_TRAIN_LONG` one struct over.
 *
 * THE PPS `scale` FIELD IS NOT THE TABLE VALUE ALONE: `V27TX_PPS_SCALE[rate]`
 * is multiplied by the config's own `int_000c` (default 1, so invisible on
 * `V27TX_CFG` itself) -- V.27ter's own caller-adjustable output gain, which
 * V.29's `V29TX_create` does not have at the identical field.
 */
void *
V27TX_create(void *modem, const struct v27tx_cfg *params)
{
	void *prm;
	int fresh = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V.27 TX Create ");

	if (modem == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("New allocation\n");

		modem = sysdep_malloc(0x2c);
		FIELD_PTR(modem, V27_OBJ_TXDATA) = 0;
		FIELD_PTR(modem, V27_OBJ_TX) = 0;
		fresh = 1;
	} else {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\n");
	}

	if (params != 0)
		*(struct v27tx_cfg *)modem = *params;
	else
		*(struct v27tx_cfg *)modem = V27TX_CFG;

	FIELD_I(modem, V27TX_OBJ_RESULT) = 0;
	*FIELD(modem, V27TX_OBJ_RESULT_B1) |= 0x58;
	FIELD_BYTE(modem, V27TX_OBJ_RESULT) = 1;

	/* ---- the data-source block: the FIFO and the SGD ------------- */

	prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
	if (prm == 0) {
		prm = sysdep_malloc(V27TXDATA_SIZE);
		FIELD_PTR(modem, V27_OBJ_TXDATA) = prm;
		FIELD_PTR(prm, V27TXD_FIFO) = 0;
		FIELD_PTR(prm, V27TXD_SGD) = 0;
	}

	{
		struct sgd_cfg gcfg = SGD_CFG;
		void *existing;

		gcfg.sym_bits = 3;

		existing = FIELD_PTR(prm, V27TXD_SGD);
		FIELD_PTR(prm, V27TXD_SGD) =
			SGD_create((struct sgd *)existing, &gcfg);
	}

	/* ---- the half-duplex machine's own state ---------------------- */

	prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
	FIELD_I(prm, V27TXP_INT_0008) = 0;
	FIELD_S(prm, V27TXP_STATE) = V27TX_STATE_START;
	FIELD_S(prm, V27TXP_COUNTDOWN) = 0;
	*(v27tx_process_fn *)(void *)FIELD(prm, V27TXP_PROCESS) = TxHdxStartV27;
	FIELD_S(prm, V27TXP_TRAIN_LONG) = (short)
		(((struct v27tx_cfg *)modem)->int_0018 == 0);

	if (((struct v27tx_cfg *)modem)->bitrate == 2400) {
		FIELD_S(prm, V27TXP_RATE) = 0;
	} else if (((struct v27tx_cfg *)modem)->bitrate == 4800) {
		FIELD_S(prm, V27TXP_RATE) = 1;
	} else {
		FIELD_S(prm, V27TXP_RATE) = 1;
		*FIELD(modem, V27TX_OBJ_RESULT_B1) |= V27TX_RESULT_B1_BIT1;
		FIELD_BYTE(modem, V27TX_OBJ_RESULT) = V27TX_STATUS_DEFAULT;
	}

	/* ---- the FIFO --------------------------------------------------- */

	{
		struct fifo_cfg fc;
		unsigned short n = (unsigned short)
			((struct v27tx_cfg *)modem)->int_0014;
		void *existing;
		short rate;

		prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
		rate = FIELD_S(prm, V27TXP_RATE);

		fc.word0 = FIFO_CFG.word0;
		fc.fill = 0;
		fc.size = (short)(n * V27TX_FRMSIZE[rate]);

		existing = FIELD_PTR(prm, V27TXD_FIFO);
		FIELD_PTR(prm, V27TXD_FIFO) =
			FIFO_create((struct fax_fifo *)existing, &fc);
	}

	/* ---- the private block: the ring, the scrambler, the symbol coder
	 * and the pulse shaper ---------------------------------------------- */

	{
		void *tx;
		short rate;
		short ring_len;

		prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
		rate = FIELD_S(prm, V27TXP_RATE);
		ring_len = (short)(V27TX_FRMSIZE[rate] + 2);

		tx = FIELD_PTR(modem, V27_OBJ_TX);
		if (tx == 0) {
			struct fpm_smc_ring *ring;

			tx = sysdep_malloc(0x94);
			FIELD_PTR(modem, V27_OBJ_TX) = tx;
			ring = (struct fpm_smc_ring *)(void *)
					FIELD(tx, V27TX_RING);
			ring->sym = (short *)
				sysdep_malloc((unsigned)(ring_len * 2));
		}

		tx = FIELD_PTR(modem, V27_OBJ_TX);
		{
			struct fpm_smc_ring *ring = (struct fpm_smc_ring *)
					(void *)FIELD(tx, V27TX_RING);
			short i;

			ring->i = 0;
			ring->q = 0;
			ring->widx = 0;
			ring->ridx = 0;
			ring->len = ring_len;

			for (i = 0; i < ring_len; i++)
				ring->sym[i] = 0;
		}
	}

	{
		struct fpm_smc_cfg scfg = SMC_CFG;
		void *tx = FIELD_PTR(modem, V27_OBJ_TX);
		short rate = FIELD_S(FIELD_PTR(modem, V27_OBJ_TXDATA),
				     V27TXP_RATE);

		scfg.f00 = 1;
		scfg.direct = 0;
		scfg.rot_step = V27TX_SMC_CRR_ADJ[rate];
		scfg.rot_mod = V27TX_SMC_CRR_LEN[rate];
		scfg.qshift = 0;
		scfg.qmask = (unsigned short)V27TX_SMC_PHS_MASK[rate];
		scfg.amask = 0;
		scfg.pmask = (unsigned short)V27TX_SMC_PHS_MASK[rate];
		scfg.pmap = V27TX_SMC_PMAP[rate];

		SMC_init((struct fpm_smc *)(void *)FIELD(tx, V27TX_SMC), &scfg);
	}

	{
		struct fpm_pps_cfg pcfg = FPM_PPS_CFG;
		void *tx = FIELD_PTR(modem, V27_OBJ_TX);
		short rate = FIELD_S(FIELD_PTR(modem, V27_OBJ_TXDATA),
				     V27TXP_RATE);

		pcfg.phases = V27TX_PPS_UP_FACT[rate];
		pcfg.step = V27TX_PPS_DOWN_FACT[rate];
		pcfg.mapped = 1;
		pcfg.scale = V27TX_PPS_SCALE[rate] *
			((struct v27tx_cfg *)modem)->int_000c;
		pcfg.step_adj = 0;
		pcfg.imap = V27TX_PPS_IMAP[rate];
		pcfg.qmap = V27TX_PPS_QMAP[rate];
		pcfg.coeff_i = V27TX_PPS_IFILT[rate];
		pcfg.coeff_q = V27TX_PPS_QFILT[rate];
		pcfg.coeffs = V27TX_PPS_FILT_LEN[rate];
		pcfg.aux = (void *)(long)((struct v27tx_cfg *)modem)->int_001c;

		FPM_PPS_init((struct fpm_pps *)(void *)FIELD(tx, V27TX_PPS),
			    &pcfg, fresh);
	}

	{
		struct sdmv27_cfg dcfg;
		void *tx = FIELD_PTR(modem, V27_OBJ_TX);

		dcfg.nbits = 3;
		SDMv27_init((struct sdmv27 *)(void *)FIELD(tx, V27TX_SDM),
			   &dcfg);
	}

	return modem;
}

/*
 * V27TX_modem .text 0x0a3330, 192 bytes.
 *
 * `V29TX_modem`'s own shape: fill the FIFO from `in` unless
 * `V27TXP_INT_0008` is non-zero (in which case `*count` is already queued
 * elsewhere), then run the installed handler in a do/while seeded with
 * `V27TX_FRMSIZE[rate]` budget -- V.29's own fixed `V29TX_MODEM_BUDGET`
 * literal, here the same per-rate table every `TxHdx*V27` handler already
 * reads.  `in` is re-passed unchanged to every call in the loop, never
 * advanced; only `out` advances, by what each call returns.
 */
int
V27TX_modem(void *modem, unsigned short *in, short *out,
	   unsigned short *count)
{
	void *prm;
	unsigned short taken;
	short budget;
	short total;

	prm = FIELD_PTR(modem, V27_OBJ_TXDATA);

	*FIELD(modem, V27TX_OBJ_RESULT_B1) &=
		(unsigned char)~V27TX_RESULT_B1_BIT1;

	if (FIELD_I(prm, V27TXP_INT_0008) == 0)
		taken = (unsigned short)FIFO_write(
				(struct fax_fifo *)
					FIELD_PTR(prm, V27TXD_FIFO),
				in, *count);
	else
		taken = *count;

	budget = V27TX_FRMSIZE[FIELD_S(prm, V27TXP_RATE)];
	total = 0;
	do {
		short got;

		prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
		got = (*(v27tx_process_fn *)(void *)
				FIELD(prm, V27TXP_PROCESS))
					(modem, in, out, &budget);

		out += got;
		total = (short)(total + got);
	} while (budget > 0);

	if (*count != taken) {
		*FIELD(modem, V27TX_OBJ_RESULT_B1) |= V27TX_RESULT_B1_BIT1;
		FIELD_BYTE(modem, V27TX_OBJ_RESULT) = V27TX_RESULT_BYTE_07;
	}

	*count = (unsigned short)total;

	return FIELD_I(modem, V27TX_OBJ_RESULT);
}

/*
 * TxNextStateV27 .text 0x0a3480, 1148 bytes.
 *
 * `jmp *table(,%eax,4)` on `V27TXP_STATE`, bounded `cmp $0xa` / `ja default`
 * -- eleven arms, not V.29's seven, and each arm's printed string names the
 * state being LEFT (the object's own debug strings at .rodata.str1.1
 * 0x4c21..0x4cdc, CLAUDE.md's evidence rank 1):
 *
 *   V27TX_STATE_START    installs TxHdxQuietV27, budget FRMSIZE[rate]
 *   V27TX_STATE_QUIET    SGD_control(gen={PATTERN_CARR[rate],1}), installs
 *                        TxHdxAltV27, budget FRMSIZE[rate]*10
 *   V27TX_STATE_CARR     installs TxHdxQuietV27, budget FRMSIZE[rate]
 *   V27TX_STATE_NOCARR   SGD_control(gen={PATTERN_ALT[rate],1}), installs
 *                        TxHdxAltV27, budget ALT_COUNT[TRAIN_LONG]
 *   V27TX_STATE_ALT      installs TxHdxEQCondV27, budget
 *                        EQCOND_COUNT[TRAIN_LONG]
 *   V27TX_STATE_EQCOND   SGD_control(gen={PATTERN_SCR1[rate],1}), installs
 *                        TxHdxSCR1V27, budget 8, SetScramblerV27(modem),
 *                        RETURNS (bypasses the shared tail)
 *   V27TX_STATE_SCR1     installs TxHdxDataV27, budget 1 (no table lookup),
 *                        RESULT_B1_BIT0 SET, RETURNS
 *   V27TX_STATE_DATA     SGD_control(gen={PATTERN_SCR1[rate],1}), installs
 *                        TxHdxSCR1V27, budget FRMSIZE[rate] -- reached only
 *                        from TxHdxDataV27's own underrun-bypass arm, which
 *                        nothing reconstructed drives (see TxHdxDataV27)
 *   V27TX_STATE_TURNOFF  installs TxHdxQuietV27, budget FRMSIZE[rate]
 *   V27TX_STATE_NOENG    installs TxHdxIdleV27, budget 0, RESULT_B2_BIT0 SET
 *   V27TX_STATE_IDLE     installs TxHdxStartV27, budget 0 -- wraps to START
 *
 * EVERY ARM CLEARS RESULT_B2_BIT0 on its way out except NOENG's, which SETS
 * it, and every arm clears RESULT_B1_BIT0 except EQCOND's (cleared
 * explicitly, matching the shared tail) and SCR1's (SET, and returned
 * before the tail can clear it) -- `TxNextStateV29`'s own SCR1 asymmetry,
 * one state index later in this machine's numbering.
 *
 * `V27TX_STATE_NOCARR`/`_ALT` INDEX BY `V27TXP_TRAIN_LONG`, NOT
 * `V27TXP_RATE` -- the one place this machine differs from `V27TX_FRMSIZE`'s
 * own rate indexing, and it is what the object's `movswl 0xe(%ecx)` reads
 * (field 0x0e, not 0xc) at both sites.
 *
 * `req.det` IS `SGD_CTL.det`, read back out of the global exactly as
 * `TxNextStateV29`'s own two sites do -- this machine's four, matching
 * `sgd.h`'s own tally of "7+4+2 = 13" SGD_control sites across V.17, V.27ter
 * and V.29.
 */
void
TxNextStateV27(void *modem)
{
	void *prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
	short state = FIELD_S(prm, V27TXP_STATE);

	switch (state) {
	case V27TX_STATE_START:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_START\n");
		FIELD_S(prm, V27TXP_COUNTDOWN) =
			V27TX_FRMSIZE[FIELD_S(prm, V27TXP_RATE)];
		*(v27tx_process_fn *)(void *)FIELD(prm, V27TXP_PROCESS) =
			TxHdxQuietV27;
		FIELD_S(prm, V27TXP_STATE) = V27TX_STATE_QUIET;
		*FIELD(modem, V27TX_OBJ_RESULT_B2) &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	case V27TX_STATE_QUIET:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_QUIET\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = (unsigned short)
				V27TX_PATTERN_CARR[FIELD_S(prm, V27TXP_RATE)];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					FIELD_PTR(prm, V27TXD_SGD), &req);
		}
		prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
		FIELD_S(prm, V27TXP_COUNTDOWN) = (short)
			(V27TX_FRMSIZE[FIELD_S(prm, V27TXP_RATE)] * 10);
		*(v27tx_process_fn *)(void *)FIELD(prm, V27TXP_PROCESS) =
			TxHdxAltV27;
		FIELD_S(prm, V27TXP_STATE) = V27TX_STATE_CARR;
		*FIELD(modem, V27TX_OBJ_RESULT_B2) &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	case V27TX_STATE_CARR:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_CARR\n");
		FIELD_S(prm, V27TXP_COUNTDOWN) =
			V27TX_FRMSIZE[FIELD_S(prm, V27TXP_RATE)];
		*(v27tx_process_fn *)(void *)FIELD(prm, V27TXP_PROCESS) =
			TxHdxQuietV27;
		FIELD_S(prm, V27TXP_STATE) = V27TX_STATE_NOCARR;
		*FIELD(modem, V27TX_OBJ_RESULT_B2) &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	case V27TX_STATE_NOCARR:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_NOCARR\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = (unsigned short)
				V27TX_PATTERN_ALT[FIELD_S(prm, V27TXP_RATE)];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					FIELD_PTR(prm, V27TXD_SGD), &req);
		}
		prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
		FIELD_S(prm, V27TXP_COUNTDOWN) =
			V27TX_ALT_COUNT[FIELD_S(prm, V27TXP_TRAIN_LONG)];
		*(v27tx_process_fn *)(void *)FIELD(prm, V27TXP_PROCESS) =
			TxHdxAltV27;
		FIELD_S(prm, V27TXP_STATE) = V27TX_STATE_ALT;
		*FIELD(modem, V27TX_OBJ_RESULT_B2) &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	case V27TX_STATE_ALT:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_ALT\n");
		FIELD_S(prm, V27TXP_COUNTDOWN) =
			V27TX_EQCOND_COUNT[FIELD_S(prm, V27TXP_TRAIN_LONG)];
		*(v27tx_process_fn *)(void *)FIELD(prm, V27TXP_PROCESS) =
			TxHdxEQCondV27;
		FIELD_S(prm, V27TXP_STATE) = V27TX_STATE_EQCOND;
		*FIELD(modem, V27TX_OBJ_RESULT_B2) &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	case V27TX_STATE_EQCOND:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_EQCOND\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = (unsigned short)
				V27TX_PATTERN_SCR1[FIELD_S(prm, V27TXP_RATE)];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					FIELD_PTR(prm, V27TXD_SGD), &req);
		}
		prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
		FIELD_S(prm, V27TXP_COUNTDOWN) = 8;
		*(v27tx_process_fn *)(void *)FIELD(prm, V27TXP_PROCESS) =
			TxHdxSCR1V27;
		FIELD_S(prm, V27TXP_STATE) = V27TX_STATE_SCR1;
		*FIELD(modem, V27TX_OBJ_RESULT_B2) &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		*FIELD(modem, V27TX_OBJ_RESULT_B1) &=
			(unsigned char)~V27TX_RESULT_B1_BIT0;
		SetScramblerV27(modem);
		return;

	case V27TX_STATE_SCR1:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_SCR1\n");
		FIELD_S(prm, V27TXP_COUNTDOWN) = 1;
		*(v27tx_process_fn *)(void *)FIELD(prm, V27TXP_PROCESS) =
			TxHdxDataV27;
		FIELD_S(prm, V27TXP_STATE) = V27TX_STATE_DATA;
		*FIELD(modem, V27TX_OBJ_RESULT_B2) &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		*FIELD(modem, V27TX_OBJ_RESULT_B1) |= V27TX_RESULT_B1_BIT0;
		return;

	case V27TX_STATE_DATA:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_DATA\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = (unsigned short)
				V27TX_PATTERN_SCR1[FIELD_S(prm, V27TXP_RATE)];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					FIELD_PTR(prm, V27TXD_SGD), &req);
		}
		prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
		FIELD_S(prm, V27TXP_COUNTDOWN) =
			V27TX_FRMSIZE[FIELD_S(prm, V27TXP_RATE)];
		*(v27tx_process_fn *)(void *)FIELD(prm, V27TXP_PROCESS) =
			TxHdxSCR1V27;
		FIELD_S(prm, V27TXP_STATE) = V27TX_STATE_TURNOFF;
		*FIELD(modem, V27TX_OBJ_RESULT_B2) &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	case V27TX_STATE_TURNOFF:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_TURNOFF\n");
		FIELD_S(prm, V27TXP_COUNTDOWN) =
			V27TX_FRMSIZE[FIELD_S(prm, V27TXP_RATE)];
		*(v27tx_process_fn *)(void *)FIELD(prm, V27TXP_PROCESS) =
			TxHdxQuietV27;
		FIELD_S(prm, V27TXP_STATE) = V27TX_STATE_NOENG;
		*FIELD(modem, V27TX_OBJ_RESULT_B2) &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	case V27TX_STATE_NOENG:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_NOENG\n");
		FIELD_S(prm, V27TXP_COUNTDOWN) = 0;
		*(v27tx_process_fn *)(void *)FIELD(prm, V27TXP_PROCESS) =
			TxHdxIdleV27;
		FIELD_S(prm, V27TXP_STATE) = V27TX_STATE_IDLE;
		*FIELD(modem, V27TX_OBJ_RESULT_B2) |= V27TX_RESULT_B2_BIT0;
		*FIELD(modem, V27TX_OBJ_RESULT_B1) &=
			(unsigned char)~V27TX_RESULT_B1_BIT0;
		return;

	case V27TX_STATE_IDLE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_IDLE\n");
		FIELD_S(prm, V27TXP_COUNTDOWN) = 0;
		*(v27tx_process_fn *)(void *)FIELD(prm, V27TXP_PROCESS) =
			TxHdxStartV27;
		FIELD_S(prm, V27TXP_STATE) = V27TX_STATE_START;
		*FIELD(modem, V27TX_OBJ_RESULT_B2) &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_DEFAULT, %d\n", state);
		*FIELD(modem, V27TX_OBJ_RESULT_B2) &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		FIELD_BYTE(modem, V27TX_OBJ_RESULT) = V27TX_STATUS_DEFAULT;
		*FIELD(modem, V27TX_OBJ_RESULT_B1) = (unsigned char)
			((*FIELD(modem, V27TX_OBJ_RESULT_B1)
			  | V27TX_RESULT_B1_BIT1)
			 & (unsigned char)~V27TX_RESULT_B1_BIT0);
		break;
	}

	*FIELD(modem, V27TX_OBJ_RESULT_B1) &=
		(unsigned char)~V27TX_RESULT_B1_BIT0;
}

/*
 * TxHdxStartV27 .text 0x0a3e10, 21 bytes.  Nothing but the transition.
 */
short
TxHdxStartV27(void *modem, unsigned short *in, short *out, short *budget)
{
	(void)in;
	(void)out;
	(void)budget;
	TxNextStateV27(modem);
	return 0;
}

/*
 * TxHdxQuietV27 .text 0x0a3d60, 160 bytes.
 *
 * Spend `min(V27TXP_COUNTDOWN, *budget)` on `TxNoCarrierV27`, or transition
 * once the countdown reaches zero.  `TxHdxAltV27` is the identical shape
 * over `SGD_symbol_gen`+`ModDataV27` instead.
 */
short
TxHdxQuietV27(void *modem, unsigned short *in, short *out, short *budget)
{
	void *prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
	short countdown;
	short taken;
	short r;

	FIELD_BYTE(modem, V27TX_OBJ_RESULT) = V27TX_STATUS_TRAINING;

	countdown = FIELD_S(prm, V27TXP_COUNTDOWN);
	if (countdown <= 0) {
		TxNextStateV27(modem);
		return 0;
	}

	taken = (short)((countdown <= *budget) ? countdown : *budget);
	FIELD_S(prm, V27TXP_COUNTDOWN) = (short)(countdown - taken);

	r = TxNoCarrierV27(modem, in, out, (unsigned short)taken);
	*budget = (short)(*budget - taken);
	return r;
}

/*
 * TxHdxAltV27 .text 0x0a3ca0, 190 bytes.
 *
 * `SGD_symbol_gen` fills `in` with `taken` symbols using the pattern
 * `TxNextStateV27`'s NOCARR arm just installed into the SGD, then
 * `ModDataV27` modulates them -- `TxHdxQuietV27`'s shape with the source
 * swapped for a real pattern.
 */
short
TxHdxAltV27(void *modem, unsigned short *in, short *out, short *budget)
{
	void *prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
	short countdown;
	short taken;
	short r;

	FIELD_BYTE(modem, V27TX_OBJ_RESULT) = V27TX_STATUS_TRAINING;

	countdown = FIELD_S(prm, V27TXP_COUNTDOWN);
	if (countdown <= 0) {
		TxNextStateV27(modem);
		return 0;
	}

	taken = (short)((countdown <= *budget) ? countdown : *budget);
	FIELD_S(prm, V27TXP_COUNTDOWN) = (short)(countdown - taken);

	SGD_symbol_gen((struct sgd *)FIELD_PTR(prm, V27TXD_SGD), in, taken);
	r = (short)ModDataV27(modem, in, out, (unsigned short)taken);

	*budget = (short)(*budget - taken);
	return r;
}

/*
 * TxHdxEQCondV27 .text 0x0a3b90, 264 bytes.
 *
 * Equaliser conditioning: fill `in[0..taken)` with the literal 7, scramble
 * the whole run, then walk it choosing `V27TX_PATTERN_ALT[rate]` or
 * `V27TX_PATTERN_CARR[rate]` per element from bit 2 of the FOLLOWING
 * scrambled element -- `in[taken]`, one element past what was filled, on the
 * loop's last iteration.  Reproduced as `in[i + 1] & 0x04`, a full-word test
 * rather than the object's byte test on `((unsigned char *)&in[i+1])[0]`;
 * x86 is little-endian, so the two are the same value for every `in[i+1]`.
 */
short
TxHdxEQCondV27(void *modem, unsigned short *in, short *out, short *budget)
{
	void *prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
	short countdown;
	short taken;
	short r;
	short i;

	FIELD_BYTE(modem, V27TX_OBJ_RESULT) = V27TX_STATUS_TRAINING;

	countdown = FIELD_S(prm, V27TXP_COUNTDOWN);
	if (countdown <= 0) {
		TxNextStateV27(modem);
		return 0;
	}

	taken = (short)((countdown <= *budget) ? countdown : *budget);
	FIELD_S(prm, V27TXP_COUNTDOWN) = (short)(countdown - taken);

	for (i = 0; i < taken; i++)
		in[i] = 7;

	ScrambleDataV27(modem, in, taken);

	if (taken != 0) {
		short rate;

		prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
		rate = FIELD_S(prm, V27TXP_RATE);

		for (i = 0; i < taken; i++) {
			if (in[i + 1] & 0x04)
				in[i] = (unsigned short)V27TX_PATTERN_ALT[rate];
			else
				in[i] = (unsigned short)
					V27TX_PATTERN_CARR[rate];
		}
	}

	r = (short)ModDataV27(modem, in, out, (unsigned short)taken);
	*budget = (short)(*budget - taken);
	return r;
}

/*
 * TxHdxSCR1V27 .text 0x0a3ac0, 206 bytes.
 *
 * `SGD_symbol_gen`, `ScrambleDataV27`, `ModDataV27` -- the scrambled-1s
 * training pattern.  `TxNextStateV27`'s EQCOND arm seeds
 * `V27TXP_COUNTDOWN` to 8 for this handler's first calls and its OWN arm
 * (reached when that countdown hits zero) reseeds it to 1 and installs
 * `TxHdxDataV27`, so this handler's own countdown-exhausted transition is
 * what carries the machine into DATA.
 */
short
TxHdxSCR1V27(void *modem, unsigned short *in, short *out, short *budget)
{
	void *prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
	short countdown;
	short taken;
	short r;

	FIELD_BYTE(modem, V27TX_OBJ_RESULT) = V27TX_STATUS_TRAINING;

	countdown = FIELD_S(prm, V27TXP_COUNTDOWN);
	if (countdown <= 0) {
		TxNextStateV27(modem);
		return 0;
	}

	taken = (short)((countdown <= *budget) ? countdown : *budget);
	FIELD_S(prm, V27TXP_COUNTDOWN) = (short)(countdown - taken);

	SGD_symbol_gen((struct sgd *)FIELD_PTR(prm, V27TXD_SGD), in, taken);
	ScrambleDataV27(modem, in, taken);
	r = (short)ModDataV27(modem, in, out, (unsigned short)taken);

	*budget = (short)(*budget - taken);
	return r;
}

/*
 * TxHdxDataV27 .text 0x0a3980, 314 bytes.
 *
 * Drain the FIFO through `FIFO_read`, scramble, modulate.
 *
 * THE ONE-TIME ENTRY STATUS.  `V27TXP_COUNTDOWN` is nonzero exactly once, on
 * the call right after the SCR1->DATA transition -- SCR1's own arm seeds it
 * to 1 and never overwrites it with a table lookup the way every other arm
 * does -- so this is the only handler that reads it as anything but a
 * countdown, and it clears the field immediately after.
 *
 * `FIFO_read` NEVER RETURNS MORE THAN IT IS ASKED FOR (`faxfifo.h`'s own
 * contract), and this function asks for exactly `*budget`, so the object's
 * `got > *budget` branch is UNREACHABLE from `V27TX_modem`'s own loop --
 * `TxHdxDataV29`'s own `V29TXP_INT_0008` arm, one modulation over, and
 * `t_v29txcreate.c`'s own idiom for reaching it (`TxHdxDataV27` called
 * directly with `V27TXP_INT_0008` poked non-zero) is the only way in.
 */
short
TxHdxDataV27(void *modem, unsigned short *in, short *out, short *budget)
{
	void *prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
	short taken;
	short got;
	short r;

	FIELD_BYTE(modem, V27TX_OBJ_RESULT) = V27TX_STATUS_DATA;

	if (FIELD_S(prm, V27TXP_COUNTDOWN) != 0) {
		short rate = FIELD_S(prm, V27TXP_RATE);

		FIELD_S(prm, V27TXP_COUNTDOWN) = 0;

		if (rate == 0)
			FIELD_BYTE(modem, V27TX_OBJ_RESULT) =
				V27TX_STATUS_ENTER_DATA_2400;
		else if (rate == 1)
			FIELD_BYTE(modem, V27TX_OBJ_RESULT) =
				V27TX_STATUS_ENTER_DATA_4800;
		else
			FIELD_BYTE(modem, V27TX_OBJ_RESULT) =
				V27TX_STATUS_DEFAULT;
	}

	taken = *budget;
	got = (short)FIFO_read((struct fax_fifo *)
					FIELD_PTR(prm, V27TXD_FIFO),
			       in, (unsigned short)taken);

	if (*budget <= got) {
		ScrambleDataV27(modem, in, got);
		r = (short)ModDataV27(modem, in, out, (unsigned short)got);
		*budget = (short)(*budget - got);
		return r;
	}

	/* Underrun: FIFO_read returned fewer than asked for. */
	if (FIELD_I(prm, V27TXP_INT_0008) != 0) {
		short remaining = (short)(*budget - got);

		*budget = remaining;
		ScrambleDataV27(modem, in, got);
		r = (short)ModDataV27(modem, in, out, (unsigned short)got);
		TxNextStateV27(modem);
		return r;
	}

	*FIELD(modem, V27TX_OBJ_RESULT_B1) |= V27TX_RESULT_B1_BIT1;
	FIELD_BYTE(modem, V27TX_OBJ_RESULT) = V27TX_STATUS_UNDERRUN;
	taken = *budget;
	ScrambleDataV27(modem, in, taken);
	r = (short)ModDataV27(modem, in, out, (unsigned short)taken);
	*budget = (short)(*budget - taken);
	return r;
}

/*
 * TxHdxIdleV27 .text 0x0a3900, 116 bytes.
 *
 * `V27TX_STATUS_IDLE` is written UNCONDITIONALLY at entry, even on the
 * transition arm, which the object does not undo -- `TxHdxIdleV29`'s own
 * shape.  With the FIFO non-empty this does not modulate at all, just
 * transitions; with it empty this spends the WHOLE current `*budget` on
 * `TxNoCarrierV27` in one call.
 */
short
TxHdxIdleV27(void *modem, unsigned short *in, short *out, short *budget)
{
	void *prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
	struct fax_fifo *fifo;
	short taken;
	short r;

	FIELD_BYTE(modem, V27TX_OBJ_RESULT) = V27TX_STATUS_IDLE;

	fifo = (struct fax_fifo *)FIELD_PTR(prm, V27TXD_FIFO);
	if (fifo->count != 0) {
		TxNextStateV27(modem);
		return 0;
	}

	taken = *budget;
	r = TxNoCarrierV27(modem, in, out, (unsigned short)taken);
	*budget = (short)(*budget - taken);
	return r;
}

/*
 * TxNoCarrierV27 .text 0x0a5f50, 151 bytes.
 *
 * Fill `count` symbol-ring slots with `V27TX_NOCARR_SYMBOL[rate]` -- wrapping
 * `widx` against `struct fpm_smc_ring::len` by hand, one slot at a time,
 * rather than through `FPM_SMC_encoder` -- then run the pulse shaper over
 * `count` samples.  `sym` and `len` are read out of the ring ONCE, before the
 * loop, and only `rate` is re-read every iteration -- the object's own
 * register/reload discipline, and `in` is UNREAD, exactly as its callers'
 * own `in` buffers go untouched here.
 */
short
TxNoCarrierV27(void *modem, unsigned short *in, short *out,
	      unsigned short count)
{
	void *tx = FIELD_PTR(modem, V27_OBJ_TX);
	struct fpm_smc_ring *ring =
		(struct fpm_smc_ring *)(void *)FIELD(tx, V27TX_RING);
	short *sym = ring->sym;
	short len = ring->len;
	short widx = ring->widx;
	unsigned short i;
	short r;

	(void)in;

	for (i = 0; i < count; i++) {
		void *prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
		short rate = FIELD_S(prm, V27TXP_RATE);

		sym[widx] = V27TX_NOCARR_SYMBOL[rate];
		widx = (short)((widx + 1 < len) ? widx + 1 : 0);
	}

	r = (short)FPM_PPS_filter(
		(struct fpm_pps *)(void *)FIELD(tx, V27TX_PPS),
		(struct fpm_smc_ring *)(void *)FIELD(tx, V27TX_RING),
		out, count);

	tx = FIELD_PTR(modem, V27_OBJ_TX);
	((struct fpm_smc_ring *)(void *)FIELD(tx, V27TX_RING))->widx = widx;

	return r;
}

/*
 * GenEQTrnSequenceV27 .text 0x0a33f0, 134 bytes.
 *
 * `TxHdxEQCondV27`'s own fill/scramble/choose sequence, free-standing over a
 * caller's buffer and count instead of `*budget`.  No reconstructed caller
 * reaches it and, per the reverse-edge probe `docs/remaining.md` records,
 * neither does anything in the object -- exported API surface with no
 * internal caller, not a missing graph hop.
 */
void
GenEQTrnSequenceV27(void *modem, unsigned short *buf, unsigned short count)
{
	unsigned short i;

	for (i = 0; i < count; i++)
		buf[i] = 7;

	ScrambleDataV27(modem, buf, (short)count);

	if (count != 0) {
		void *prm = FIELD_PTR(modem, V27_OBJ_TXDATA);
		short rate = FIELD_S(prm, V27TXP_RATE);

		for (i = 0; i < count; i++) {
			if (buf[i + 1] & 0x04)
				buf[i] = (unsigned short)
					V27TX_PATTERN_ALT[rate];
			else
				buf[i] = (unsigned short)
					V27TX_PATTERN_CARR[rate];
		}
	}
}

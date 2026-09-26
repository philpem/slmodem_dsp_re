/*
 * V17rx.c -- split out of the merged v17.c so the definitions sit in
 * the translation unit the object's FILE order gives them.  Bodies
 * moved verbatim; no source text changed.  See finding F11390.
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
#include "dsplib/v32smc.h"
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
 *     +0x14 short_train -> `V17RXC_INT_0010` at 0x09709c
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
 * `FPM_TONE_CFG` IS THE CONFIGURATION STRUCTURE
 *
 * The object's `FPM_TONE_CFG` is the 36-byte structure itself (`R` at .rodata
 * 0xd000, `st_size` 0x24), and this function copies all nine of its dwords.
 * `src/dsp/fpm_tone_cfg.c` defines that structure under the same exported
 * name.  `t_v17rxcreate.c` compares `FPM_TONE_CFG` against `ref_FPM_TONE_CFG`
 * byte for byte, with the embedded prototype pointer followed separately.
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
 * recorded as a derivation: no format string names it, so the name
 * `short_train` is carried from the decoder field this constructor copies it
 * into, `v17_dec::short_train` (rank 2, Batch 25).  Finding F9477.
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
		RXROOT(modem)->ctl = NULL;
		RXROOT(modem)->state = NULL;
		owned = 1;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("\n");

	/* See the head of this function: 40 bytes, one struct assignment. */
	if (params != NULL)
		RXROOT(modem)->cfg = *params;
	else
		RXROOT(modem)->cfg = V17RX_CFG;

	/* ---- the control block ---------------------------------------- */

	ctl_fresh = 0;
	if (CTL(modem) == NULL) {
		RXROOT(modem)->ctl = (struct v17rx_priv *)sysdep_malloc(0x5c);
		/*
		 * Three handles and nothing else.  The other 0x4c bytes of the
		 * block keep whatever the allocator left until the code below
		 * writes them, which is why `t_v17rxcreate.c` compares the
		 * whole 0x5c under the harness's 0xa5 fill.
		 */
		RXCTL(modem)->mtd = NULL;
		RXCTL(modem)->tone = NULL;
		RXCTL(modem)->scratch = (short *)sysdep_malloc(0x140);
		RXCTL(modem)->buf2 = (short *)sysdep_malloc(0x140);
		RXCTL(modem)->mtd2 = NULL;
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
	RXCTL(modem)->mtd = FPM_MTD_create(RXCTL(modem)->mtd, &mtdcfg);

	/*
	 * The notch the demodulator's pre-pass runs, retuned from the built-in
	 * V.25 answer tone to V.17's own 1800 Hz carrier and otherwise copied
	 * whole.
	 */
	tonecfg = FPM_TONE_CFG;
	tonecfg.freq = 1800;
	RXCTL(modem)->tone = FPM_TONE_create(RXCTL(modem)->tone, &tonecfg);

	RXCTL(modem)->state = V17RX_STATE_START;
	RXCTL(modem)->countdown = 0;
	RXCTL(modem)->r08 = 0;
	CTL(modem)->process = RxHdxStartV17;
	RXCTL(modem)->short_train =
		RXROOT(modem)->cfg.short_train;

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
	RXCTL(modem)->mtd2 = FPM_MTD_create(RXCTL(modem)->mtd2, &mtdcfg);

	/* The only init in the function given the flag that is about it. */
	FPM_AGC_init(&RXCTL(modem)->agc, &AGCv17_CFG, ctl_fresh);

	RXCTL(modem)->offband = 0;
	RXCTL(modem)->offband_latch = 0;

	/*
	 * The bit rate to the four-value code, and the two DEAD STORES on the
	 * arm that does not recognise it: 0x097140 and 0x097144 report an
	 * unknown rate through `V17RX_OBJ_RESULT`, and 0x0977a5 further down
	 * this same function clears all four bytes of that word before any
	 * caller can see it.  Deviation D1220, reproduced.
	 */
	switch (RXROOT(modem)->cfg.bit_rate) {
	case 7200:
		RXCTL(modem)->rate_code = V17RX_RATE_7200;
		break;
	case 9600:
		RXCTL(modem)->rate_code = V17RX_RATE_9600;
		break;
	case 12000:
		RXCTL(modem)->rate_code = V17RX_RATE_12000;
		break;
	case 14400:
		RXCTL(modem)->rate_code = V17RX_RATE_14400;
		break;
	default:
		RXCTL(modem)->rate_code = V17RX_RATE_14400;
		RXROOT(modem)->result.byte.flags |= V17RX_FLAG_ERROR;
		RXROOT(modem)->result.byte.status = V17RX_STATUS_DEFAULT;
		break;
	}

	/* ---- the demodulator state ------------------------------------ */

	/*
	 * One value into three configurations' tail slots, which is F8651's
	 * shape in a second modem: `fpm_mrf_cfg::aux`, `fpm_sre_cfg` + 0x34
	 * and `fpm_fse_cfg::reserved34`.  What it MEANS is not established
	 * here either.
	 */
	aux = RXROOT(modem)->cfg.ptr_0024;

	if (RXS(modem) == NULL) {
		RXROOT(modem)->state =
			(struct v17rx_state *)sysdep_malloc(0x4fbc);
		/*
		 * NOTHING IS CLEARED HERE.  20,412 bytes of allocator fill,
		 * and the writes below do not cover all of it -- see the
		 * tiling note in v17fax.h for which spans stay untouched.
		 */
		RXSTATE(modem)->buf_mrf = (short *)sysdep_malloc(0x140);
		RXSTATE(modem)->buf_sre = (short *)
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
	FPM_MRF_init(&RXS(modem)->mrf, &mrfcfg, owned);

	FPM_AGC_init(&RXS(modem)->agc.value, &AGCv17_CFG, owned);

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
	if (RXCTL(modem)->short_train != 0) {
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
	srecfg.rms_min = (short)(RXS(modem)->agc.value.cfg.ref_level / 6);
	srecfg.rms_len = 9;
	/* fpm_sre_cfg + 0x34; see the head of this function and F9475. */
	memcpy(&srecfg.pad34, &aux,
	       sizeof srecfg.pad34 + sizeof srecfg.pad36);
	FPM_SRE_init(&RXS(modem)->sre, &srecfg, owned);

	/*
	 * The four ppm-meter parameters `FPM_SRE_init` never writes, which
	 * `fpm_sre.h` says a caller has to fill.  `ppm_scale` is ppm per
	 * slipped sample at this loop's own output rate: `clock_len` points
	 * per symbol at 9600 symbols a second, so 10^6 / 28800 = 34.
	 */
	RXS(modem)->sre.ppm_step = 0x30;
	RXS(modem)->sre.ppm_period = 0x2580;
	RXS(modem)->sre.ppm_n_max = 0x68;
	RXS(modem)->sre.ppm_scale =
		(short)(1000000 / (srecfg.clock_len * 0x2580));

	/* ---- the equaliser and its slicer ----------------------------- */

	fsecfg = FPM_FSE_CFG;
	fsecfg.block = 0x90;
	fsecfg.interp = 3;
	if (RXCTL(modem)->short_train != 0) {
		fsecfg.icoff = (const short *)
			RXROOT(modem)->cfg.coefsave0;
		fsecfg.qcoff = (const short *)
			RXROOT(modem)->cfg.coefsave1;
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
	fsecfg.owner = &RXS(modem)->dec;
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
	FPM_FSE_init(&RXS(modem)->fse, &fsecfg, owned);

	/* ---- the slicers' own state ----------------------------------- */

	rate = (short)RXCTL(modem)->rate_code;
	RXS(modem)->dec.sym_count = 0;
	RXS(modem)->dec.short_0066 = 3;
	RXS(modem)->dec.rate = rate;
	RXS(modem)->dec.short_train = RXCTL(modem)->short_train;
	RXS(modem)->dec.count = 0;
	RXS(modem)->dec.scram = 0;
	RXS(modem)->dec.ang_prev = 0;
	RXS(modem)->dec.eqm_a = 0;
	RXS(modem)->dec.eqm_b = 0;
	RXS(modem)->dec.int_0050 = 0;
	/*
	 * `movl $0x0,0x54(%ebp)` -- four bytes of the six `v17dec.h` carries
	 * as `pad54`, so the two at +0x58 stay as the allocator left them.
	 * Written through the array because that header is not this pass's to
	 * edit and a wider member would be a claim about bytes the object
	 * does not touch.
	 */
	memset(RXS(modem)->dec.pad54, 0, 4);
	/*
	 * `sym_i`, `sym_q`, `sym_i1`, `sym_q1`, `sym_i2`, `sym_q2` -- six
	 * contiguous shorts from +0x3e, cleared by one loop (0x097550,
	 * `cmp $0x5`) rather than one at a time.
	 */
	for (i = 0; (short)i <= 5; i++)
		(&RXS(modem)->dec.sym_i)[i] = 0;

	/*
	 * `VTBv32_init`'s body, INLINED, with only the switch's case values
	 * changed: 0x097563..0x09760d is that function instruction for
	 * instruction (compare `src/pump/v32/v32vtb.c`, .text 0x07e700).  The
	 * survivor ring is allocated on `owned` -- the wrong flag again, and
	 * the reason the zeroing loop below can walk an uninitialised pointer.
	 * D1222.
	 */
	v = &RXS(modem)->dec.vtb;
	if (owned)
		v->paths = sysdep_malloc(
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
		RXSTATE(modem)->dec.sgd = NULL;

	sgdcfg = SGD_CFG;
	sgdcfg.sym_bits = 2;
	sgdcfg.det.ref_margin = 0x2000;
	sgdcfg.det.pat_match = 0x111;
	sgdcfg.det.pat_mask = 0xffff;
	RXSTATE(modem)->dec.sgd = SGD_create(RXSTATE(modem)->dec.sgd,
						       &sgdcfg);

	/*
	 * The descrambler: V.17's own 1 + x^-18 + x^-23, over a word carrying
	 * one symbol's worth of bits.  Every one of `SDM_CFG`'s three shorts
	 * is overwritten, which is deviation D1224 and is what fixes the
	 * source's shape as a copy plus three assignments.
	 */
	sdmcfg = SDM_CFG;
	sdmcfg.nbits = (short)(RXCTL(modem)->rate_code + 3);
	sdmcfg.tap1 = 0x12;
	sdmcfg.tap2 = 0x17;
	SDM_init(&RXSTATE(modem)->sdm, &sdmcfg);

	/*
	 * 160 entries of each chained buffer.  `V17RXS_BUF_MRF` is exactly
	 * that long; `V17RXS_BUF_SRE` is `V17RXS_SRE_MAX` = 164, so its top
	 * four entries keep the allocator's fill.  Deviation D1225.
	 */
	mrfbuf = RXSTATE(modem)->buf_mrf;
	srebuf = RXSTATE(modem)->buf_sre;
	for (i = 0; (short)i <= 0x9f; i++) {
		mrfbuf[i] = 0;
		srebuf[i] = 0;
	}

	RXSTATE(modem)->qcount = 0;
	RXSTATE(modem)->qavg = 0;
	RXSTATE(modem)->r4fb2 = 0;

	/*
	 * The quality threshold `QualityDetectV17` judges its smoothed
	 * decoder error against on block 0x32, one value per rate.  There is
	 * no default arm: a code outside 0..3 leaves the field as the
	 * allocator left it, which `V17RXC_RATE_CODE`'s own writer above
	 * makes unreachable.
	 */
	switch ((short)RXCTL(modem)->rate_code) {
	case V17RX_RATE_7200:
		RXSTATE(modem)->quality_threshold = 0xa28;
		break;
	case V17RX_RATE_9600:
		RXSTATE(modem)->quality_threshold = 0x514;
		break;
	case V17RX_RATE_12000:
		RXSTATE(modem)->quality_threshold = 0x341;
		break;
	case V17RX_RATE_14400:
		RXSTATE(modem)->quality_threshold = 0x1c2;
		break;
	}

	RXSTATE(modem)->r00 = 1;
	RXSTATE(modem)->r04 = 1;
	RXSTATE(modem)->r08 = 1;
	RXSTATE(modem)->r0c = 0;
	RXSTATE(modem)->r10 = 1;
	RXSTATE(modem)->r14 = 0;
	RXSTATE(modem)->r18 = 1;
	/*
	 * A FULL `int`, and that is what settles the width `v17fax.h` had to
	 * guess: `V17RX_status` reads bit 0 of the byte and this writes
	 * `movl $0x1` over all four.  Finding F9474.
	 */
	RXSTATE(modem)->r1c = 1;
	RXSTATE(modem)->r20 = 0;
	RXSTATE(modem)->rate_code = RXCTL(modem)->rate_code;
	RXSTATE(modem)->r28 = 0;
	RXSTATE(modem)->energy_watch = 1;
	RXSTATE(modem)->rms_ref = 0;
	RXSTATE(modem)->rms_phase = 0;

	/* ---- what the instance hands back to its caller --------------- */

	/*
	 * The result word, cleared and then rebuilt: this is where the two
	 * dead stores on the unrecognised-rate arm go, and where bits 4 and 6
	 * of `V17RX_OBJ_RESULT_B1` -- which nothing else in the object writes
	 * and nothing at all reads -- are set.  Finding F9473.
	 */
	RXROOT(modem)->result.word = 0;
	RXROOT(modem)->result.byte.flags |= V17RX_FLAG_BIT4 | V17RX_FLAG_BIT6;
	RXROOT(modem)->result.byte.status = V17RX_STATUS_START;

	/*
	 * Six handles copied out of the equaliser the call above has just
	 * built, and six fields cleared.  All six sources are `struct fpm_fse`
	 * members, which is what types them -- rank 2 and not usage
	 * inference.  The six zeroed at +0x44..+0x58 have no evidence of role
	 * anywhere and are left unnamed.  Finding F9476.
	 */
	RXROOT(modem)->out_i = RXS(modem)->fse.out_i;
	RXROOT(modem)->out_q = RXS(modem)->fse.out_q;
	RXROOT(modem)->n_out = &RXS(modem)->fse.n_out;
	RXROOT(modem)->icoeff = RXS(modem)->fse.icoeff;
	RXROOT(modem)->qcoeff = RXS(modem)->fse.qcoeff;
	RXROOT(modem)->taps = (unsigned short)
		RXS(modem)->fse.cfg.taps;

	RXROOT(modem)->r44 = 0;
	RXROOT(modem)->r48 = 0;
	RXROOT(modem)->r4c = 0;
	RXROOT(modem)->r50 = 0;
	RXROOT(modem)->r54 = 0;
	RXROOT(modem)->r58 = 0;

	return modem;
}

/*
 * V17RX_delete -- .text 0x097b40, 251 bytes.  See v17fax.h for the order and
 * for why the object's literal 1 in the second argument slot is not here.
 */
void
V17RX_delete(void *modem)
{
	SGD_delete(RXSTATE(modem)->dec.sgd);
	sysdep_free(RXSTATE(modem)->dec.vtb.paths);

	FPM_FSE_free(&RXS(modem)->fse);
	FPM_SRE_free(&RXS(modem)->sre);
	FPM_MRF_free(&RXS(modem)->mrf);

	sysdep_free(RXSTATE(modem)->buf_sre);
	sysdep_free(RXSTATE(modem)->buf_mrf);
	sysdep_free(RXS(modem));

	FPM_MTD_delete(RXCTL(modem)->mtd);
	FPM_TONE_delete(RXCTL(modem)->tone);
	sysdep_free(RXCTL(modem)->scratch);
	sysdep_free(RXCTL(modem)->buf2);
	FPM_MTD_delete(RXCTL(modem)->mtd2);
	sysdep_free(CTL(modem));

	sysdep_free(modem);
}

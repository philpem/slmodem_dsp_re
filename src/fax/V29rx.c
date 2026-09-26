/*
 * V29rx.c -- split out of the merged v29.c / v29data.c so the definitions sit in
 * the translation unit the object's FILE order gives them.  Bodies moved
 * verbatim; no source text changed.  See finding F11390.
 */
#include <stddef.h>
#include <string.h>

#include "dsplib/v29fax.h"
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

/*
 * ---------------------------------------------------------------------------
 * V29RX_create -- .text 0x09ad40, 2,127 bytes.
 *
 * The constructor, and the function that lays out everything the rest of this
 * file reads through named offsets.  Three allocations, six FPM modules
 * initialised from five stack-built configurations, and then about sixty
 * literal seeds.
 *
 * IT IS RE-ENTRANT OVER AN EXISTING INSTANCE, at three independent levels: a
 * non-NULL `modem` is re-initialised in place, and the detection and receive
 * blocks are allocated only if their pointers are already clear.  That is what
 * makes the two `reset` flags below two flags and not one.
 *
 * THE TWO `reset` FLAGS ARE SEPARATE AND THE OBJECT KEEPS THEM IN SEPARATE
 * PLACES -- `%edi` for the detection block's and `0x1c(%esp)` for the handle's.
 * `FPM_AGC_init` on the V.21 gain control gets the first; `FPM_MRF_init`,
 * `FPM_AGC_init` on the input gain control, `FPM_SRE_init` and `FPM_FSE_init`
 * get the second.  A handle that already exists but whose receive block does
 * not therefore allocates that block and then hands those four modules a ZERO,
 * so they re-initialise over uninitialised memory.  D1174.
 *
 * THE CONFIGURATIONS ARE TEMPLATE-PLUS-PATCH, five times over, and GCC dead-
 * stored the parts of each template that are wholly overwritten -- which is
 * why the object copies only two of `FPM_MTD_CFG`'s three dwords and only one
 * of `SDM_CFG`'s two.  The source is a struct assignment followed by field
 * assignments in every case; what survives of each template is stated beside
 * it below.
 *
 * `fpm_sre_cfg`'s +0x34 IS A POINTER AND THE HEADER SPELLS IT `pad34`/`pad36`.
 * The object stores 32 bits there (`mov %edi,0xa4(%esp)`), which two shorts
 * cannot express, so this file uses the `memcpy` idiom
 * `src/pump/v32/v32fprecr.c` already established for exactly this field rather
 * than renaming a shared header from a V.29 pass.  The same pointer goes to
 * `fpm_mrf_cfg::aux` and `fpm_fse_cfg::reserved34`, which ARE declared as
 * pointers -- so all three modules are handed one `aux` out of the caller's
 * configuration, and that is what says the sre field is one too.  F9324.
 *
 * THE ONE FIELD WITH A DEFAULT-LESS SWITCH is `V29RX_DEC_ERROR_LIMIT`: rate 0
 * writes 0x320, rate 1 writes 0x2bc, and any other value writes NOTHING.  On a
 * freshly allocated block that leaves the allocator's contents in a field
 * `DataCarrierDetectV29` compares against.  D1173.
 *
 * WHAT THE SRE'S TIMING METER IS GIVEN.  `fpm_sre.h` records that
 * `ppm_step`, `ppm_scale`, `ppm_period` and `ppm_n_max` are read by
 * `FPM_SRE_recover` and written by neither it nor `FPM_SRE_init`, so "a caller
 * has to fill them".  THIS IS THAT CALLER, and it fills exactly those four and
 * no others -- 0x30, 1000000 / (clock_len * 9600), 9600 and 0x68 -- which
 * turns that paragraph's inference into a reading.  Finding F9325.
 */
void *
V29RX_create(void *modem, const struct v29rx_cfg *params)
{
	struct fpm_mtd_cfg mcfg;
	struct fpm_tone_cfg tcfg;
	struct fpm_mrf_cfg rcfg;
	struct fpm_sre_cfg scfg;
	struct fpm_fse_cfg fcfg;
	struct fpm_sdm_cfg dcfg;
	struct v29_rx_detector *det;
	struct v29_rx_block *rx;
	struct v29_rx_decoder *dec;
	void *aux;
	int new_handle = 0;
	int new_det = 0;
	short i;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V.29 RX Create ");

	if (modem == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("New allocation\n");

		modem = sysdep_malloc(V29_OBJ_SIZE);
		((struct v29_rx *)modem)->det = 0;
		((struct v29_rx *)modem)->rx = 0;
		new_handle = 1;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("\n");

	/*
	 * TWO STRUCT ASSIGNMENTS, not one with a conditional operand: the
	 * object has two unrolled six-dword copies whose tails are merged at
	 * 0x9ada5, which is what a compiler does to an if/else and not to a
	 * `?:` over one destination.
	 */
	if (params != 0)
		*&((struct v29_rx *)modem)->cfg = *params;
	else
		*&((struct v29_rx *)modem)->cfg = V29RX_CFG;

	/* ---- the detection block ---------------------------------------- */

	det = ((struct v29_rx *)modem)->det;
	if (det == 0) {
		det = sysdep_malloc(V29DET_SIZE);
		((struct v29_rx *)modem)->det = det;

		det->mtd = 0;
		det->tone = 0;
		det->buf = sysdep_malloc(V29DET_BUF_BYTES);
		((struct v29_rx *)modem)->det->v21_buf =
			sysdep_malloc(V29DET_V21_BUF_BYTES);
		((struct v29_rx *)modem)->det->v21_mtd = 0;
		new_det = 1;
	}

	/* The block detector.  Only `f0a` survives from the template. */
	mcfg = FPM_MTD_CFG;
	mcfg.coeff = V29_MTD_COEFF;
	mcfg.tones = V29RX_MTD_TONES;
	mcfg.ratio = V29RX_MTD_RATIO;
	mcfg.min_level = V29RX_MTD_MIN_LEVEL;
	det->mtd = FPM_MTD_create(
		(struct fpm_mtd *)((struct v29_rx *)modem)->det->mtd, &mcfg);

	/*
	 * The notch and tone detector.  Everything but `freq` survives, and
	 * the 1700 Hz that replaces it is V.29's own carrier -- where
	 * `FPM_TONE_CFG` carries V.25's 2100 Hz answer tone.
	 */
	tcfg = FPM_TONE_CFG;
	tcfg.freq = V29RX_TONE_HZ;
	det->tone = FPM_TONE_create(
		(struct fpm_tone *)((struct v29_rx *)modem)->det->tone, &tcfg);

	/* The receive machine starts in START, with START's handler. */
	det = ((struct v29_rx *)modem)->det;
	det->state = V29RX_STATE_START;
	det->state_count = 0;
	det->int_0008 = 0;
	det->handler = RxHdxStartV29;

	/* The V.21 channel-2 detector, from the same template again. */
	mcfg = FPM_MTD_CFG;
	mcfg.coeff = V21_CHAN2_MTD_COEFF;
	mcfg.tones = V29RX_V21_MTD_TONES;
	mcfg.ratio = V29RX_V21_MTD_RATIO;
	mcfg.min_level = V29RX_V21_MTD_MIN_LEVEL;
	det->v21_mtd = FPM_MTD_create(
		(struct fpm_mtd *)((struct v29_rx *)modem)->det->v21_mtd,
		&mcfg);

	FPM_AGC_init((struct fpm_agc *)(void *)&((struct v29_rx *)modem)->det->v21_agc,
		     &AGCv29_CFG, new_det);

	det = ((struct v29_rx *)modem)->det;
	det->v21_samples = 0;
	det->v21_enable = 0;

	/*
	 * The rate, as an index.  The DEFAULT arm and the 9600 arm write the
	 * same value and are still two arms in the object (0x9b565 and
	 * 0x9af7d), which is what says this was a `switch` and not an
	 * `== 7200` test.
	 */
	switch ((&((struct v29_rx *)modem)->cfg)->bit_rate) {
	case V29_BPS_7200:
		det->rate = V29_RATE_7200;
		break;
	case V29_BPS_9600:
		det->rate = V29_RATE_9600;
		break;
	default:
		det->rate = V29_RATE_9600;
		break;
	}

	/* ---- the receive block ------------------------------------------ */

	aux = (&((struct v29_rx *)modem)->cfg)->ptr_0014;

	/*
	 * Raised here and overwritten at the end of the function.  Both stores
	 * are real and a caller cannot see either, because nothing else runs
	 * in between.
	 */
	((struct v29_rx *)modem)->result.word |= V29_STATUS_ERROR;
	((struct v29_rx *)modem)->result.byte.status = V29RX_STATUS_DEFAULT;

	rx = ((struct v29_rx *)modem)->rx;
	if (rx == 0) {
		rx = sysdep_malloc(V29RX_SIZE);
		((struct v29_rx *)modem)->rx = rx;

		rx->buf_mrf =
			sysdep_malloc(V29RX_BUF_MRF_BYTES);
		((struct v29_rx *)modem)->rx->buf_sre =
			sysdep_malloc(V29RX_BUF_SRE_BYTES);
	}

	/* The resampler.  Only `pad0a` survives from the template. */
	rcfg = FPM_MRF_CFG;
	rcfg.branches = 9;
	rcfg.decimate = 10;
	rcfg.coeff = V29RX_MRF_FILT;
	rcfg.taps = 0x10e;
	rcfg.aux = aux;
	FPM_MRF_init(&((struct v29_rx *)modem)->rx->mrf, &rcfg, new_handle);

	FPM_AGC_init(&((struct v29_rx *)modem)->rx->agc, &AGCv29_CFG, new_handle);

	/*
	 * The symbol recoverer.  `pad0e` and `pad36` survive; everything else
	 * is V.29's.
	 *
	 * `rms_min` IS READ BACK OUT OF THE GAIN CONTROL the line above just
	 * initialised -- `movswl 0x64(%edx)` on the receive block is
	 * `agc.cfg.ref_level` -- and divided by six.  The object does it with
	 * the `imul $0x2aaaaaab` / `sar $0x1f` / `sub` magic sequence, which
	 * is GCC's signed division by 6 and not a shift.
	 */
	scfg = FPM_SRE_CFG;
	scfg.clock_len = 3;
	scfg.groups_acq = 3;
	scfg.groups_trk = 0x10;
	scfg.settle = 0x2b;
	scfg.acc_down = 0x2000;
	scfg.acc_up = 0x4000;
	scfg.coeffs = 0xb4;
	scfg.proto = V29RX_SRE_FILT;
	scfg.disc = V29RX_XB_COFFS;
	scfg.xclock = V29RX_XCLOCK;
	scfg.yclock = V29RX_YCLOCK;
	scfg.pll_k1 = V29RX_SRE_PLLK1;
	scfg.pll_k2 = V29RX_SRE_PLLK2;
	scfg.mag_hi = 2;
	scfg.mag_lo = 1;
	scfg.err_hi = 0x2666;
	scfg.err_lo = 0xc8;
	scfg.rms_min = (short)(((struct v29_rx *)modem)->rx->agc.cfg.ref_level / 6);
	scfg.rms_len = 9;
	memcpy(&scfg.pad34, &aux, sizeof aux);
	FPM_SRE_init(&((struct v29_rx *)modem)->rx->sre, &scfg, new_handle);

	/* The four timing-meter fields `FPM_SRE_init` leaves to its caller. */
	((struct v29_rx *)modem)->rx->sre.ppm_step = 0x30;
	((struct v29_rx *)modem)->rx->sre.ppm_period = V29_BPS_9600;
	((struct v29_rx *)modem)->rx->sre.ppm_n_max = 0x68;
	((struct v29_rx *)modem)->rx->sre.ppm_scale =
		(short)(1000000 / (scfg.clock_len * V29_BPS_9600));

	/*
	 * The equaliser.  `block`, `mu[2]` and `pad22` survive the template.
	 * `owner` is the decoder block and `decision` is the first stage of
	 * the slicer chain -- which is the whole of what installs it.
	 */
	fcfg = FPM_FSE_CFG;
	fcfg.interp = 3;
	fcfg.icoff = V29RX_FSE_IFILT;
	fcfg.qcoff = V29RX_FSE_QFILT;
	fcfg.taps = 0x31;
	fcfg.mu[0] = 0;
	fcfg.mu[1] = 0;
	fcfg.clk = V29RX_CRR_TABLE;
	fcfg.clk_mod = 0x48;
	fcfg.clk_inc = 0x11;
	fcfg.train_sym = 0x1c8;
	fcfg.err_hi = 0x199a;
	fcfg.err_lo = 0xccd;
	fcfg.pll_k1 = V29RX_FSE_PLLK1;
	fcfg.pll_k2 = V29RX_FSE_PLLK2;
	fcfg.owner = &((struct v29_rx *)modem)->rx->dec;
	fcfg.decision = V29RX_epoch_det;
	fcfg.reserved34 = aux;
	FPM_FSE_init(&((struct v29_rx *)modem)->rx->fse, &fcfg, new_handle);

	/*
	 * The descrambler.  Three bits a symbol at 7200 and four at 9600, over
	 * V.29's 2400 baud -- which is the second, independent statement that
	 * `V29DET_RATE` is the bit rate and which value is which.
	 */
	dcfg = SDM_CFG;
	dcfg.nbits = (short)(4 - (((struct v29_rx *)modem)->det->rate
				  == V29_RATE_7200));
	dcfg.tap1 = V29RX_SDM_TAP1;
	dcfg.tap2 = V29RX_SDM_TAP2;
	SDM_init((struct fpm_sdm *)(void *)&((struct v29_rx *)modem)->rx->sdm, &dcfg);

	/* ---- the seeds --------------------------------------------------- */

	rx = ((struct v29_rx *)modem)->rx;

	/*
	 * Only the FIRST V29RX_BUF_ZEROED entries, which is all of the MRF's
	 * buffer and all but four shorts of the SRE's.  The induction variable
	 * is a `short` -- `inc %eax` then `cwtl` -- which is the object's.
	 */
	for (i = 0; i < V29RX_BUF_ZEROED; i = (short)(i + 1)) {
		((short *)rx->buf_mrf)[i] = 0;
		((short *)rx->buf_sre)[i] = 0;
	}

	det = ((struct v29_rx *)modem)->det;
	rx->dec_error_avg = 0;
	rx->dec_error_n = 0;
	rx->short_4f62 = 0;

	/* NO DEFAULT ARM.  See the note above and D1173. */
	switch (det->rate) {
	case V29_RATE_7200:
		rx->dec_error_limit =
			V29RX_DEC_ERROR_LIMIT_7200;
		break;
	case V29_RATE_9600:
		rx->dec_error_limit =
			V29RX_DEC_ERROR_LIMIT_9600;
		break;
	default:
		break;
	}

	dec = &rx->dec;
	dec->mag_avg_far = 0;
	rx->short_4f64 = 1;
	rx->rms_n = 0;
	rx->rms_ref = 0;
	dec->mag_avg_near = 0;
	dec->i0 = 0;
	dec->q0 = 0;
	dec->i1 = 0;
	dec->q1 = 0;
	dec->i2 = 0;
	dec->q2 = 0;

	rx->int_0000 = 1;
	dec->last = 0;
	dec->train_lfsr = V29RX_TRAIN_LFSR_INIT;
	dec->sixteen_point = det->rate;
	dec->train_count = 0;
	dec->short_001a = 0;
	dec->angle_prev = 0;
	dec->sym_count = 0;
	rx->int_0004 = 1;
	rx->int_0008 = 1;
	rx->int_000c = 0;
	rx->int_0010 = 0;
	rx->int_0014 = 1;
	rx->flags_word_0018 = 1;
	rx->int_001c = 0;
	rx->int_0020 = 1;
	rx->int_0024 = 0;

	/* ---- the handle -------------------------------------------------- */

	rx = ((struct v29_rx *)modem)->rx;

	((struct v29_rx *)modem)->result.word = 0;
	((struct v29_rx *)modem)->result.word |= V29_STATUS_CREATE_BITS;
	((struct v29_rx *)modem)->result.byte.status = V29RX_STATUS_START;

	((struct v29_rx *)modem)->eq_out_i = rx->fse.out_i;
	((struct v29_rx *)modem)->eq_out_q = rx->fse.out_q;
	((struct v29_rx *)modem)->eq_n_out = &rx->fse.n_out;
	((struct v29_rx *)modem)->eq_icoeff = rx->fse.icoeff;
	((struct v29_rx *)modem)->eq_qcoeff = rx->fse.qcoeff;
	((struct v29_rx *)modem)->eq_taps = rx->fse.cfg.taps;

	((struct v29_rx *)modem)->short_003c = 0;
	((struct v29_rx *)modem)->short_0048 = 0;
	((struct v29_rx *)modem)->int_0034 = 0;
	((struct v29_rx *)modem)->int_0038 = 0;
	((struct v29_rx *)modem)->int_0040 = 0;
	((struct v29_rx *)modem)->int_0044 = 0;

	return modem;
}

/*
 * ---------------------------------------------------------------------------
 * V29RX_delete -- .text 0x09b590, 220 bytes.
 *
 * The object places a literal 1 in the second argument slot before
 * `FPM_FSE_free`, `FPM_SRE_free` and `FPM_MRF_free`.  All three take a single
 * argument -- none of them reads a frame slot past the first -- so it is dead
 * stack setup, presumably left from a version where they took a `fresh` flag
 * like their `_init` counterparts.  Not reproduced, because there is nothing
 * to reproduce; `B103FP_delete` records the identical pattern for the same
 * three-way reason.  Finding F8876.
 *
 * The final free is a sibling `jmp` and is unconditional.
 */
void
V29RX_delete(void *modem)
{
	FPM_FSE_free(&((struct v29_rx *)modem)->rx->fse);
	FPM_SRE_free(&((struct v29_rx *)modem)->rx->sre);
	FPM_MRF_free(&((struct v29_rx *)modem)->rx->mrf);

	sysdep_free(((struct v29_rx *)modem)->rx->buf_sre);
	sysdep_free(((struct v29_rx *)modem)->rx->buf_mrf);
	sysdep_free(((struct v29_rx *)modem)->rx);

	FPM_MTD_delete((struct fpm_mtd *)((struct v29_rx *)modem)->det->mtd);
	FPM_TONE_delete((struct fpm_tone *)((struct v29_rx *)modem)->det->tone);
	sysdep_free(((struct v29_rx *)modem)->det->buf);
	sysdep_free(((struct v29_rx *)modem)->det->v21_buf);
	FPM_MTD_delete((struct fpm_mtd *)((struct v29_rx *)modem)->det->v21_mtd);
	sysdep_free(((struct v29_rx *)modem)->det);

	sysdep_free(modem);
}

/*
 * ---------------------------------------------------------------------------
 * V29RX_epoch_det -- .text 0x09b670, 413 bytes.
 *
 * The FIRST slicer of three, the one `V29RX_create` installs, and -- like
 * `V27RX_epoch_det` -- it decides nothing.  It watches the equaliser's own
 * output for a jump, reports a fixed constellation angle back, and returns
 * 0xffff on every path.
 *
 * WHAT IT READS AND WHAT IT WRITES ARE NOT V.27ter's.  `V27RX_epoch_det`
 * ignores both of its `short *` arguments; this one READS `*mag` (as the
 * energy that feeds the leaky average) and WRITES `*angle` (as a constellation
 * angle chosen by the phase advance).  So the two functions have the same
 * shape and not the same interface, and a reader who assumes otherwise gets
 * the argument directions backwards.  Finding F9322.
 *
 * THE ARITHMETIC, and every part of it is the object's:
 *
 *   d = (short)( (((short)(I1 - i))^2 + ((short)(Q1 - q))^2) >> 15
 *              + (((short)(I2 - I0))^2 + ((short)(Q2 - Q0))^2) >> 15 )
 *
 * -- two squared distances ACROSS TWO SYMBOLS each, exactly as V.27ter forms
 * them.  Then the phase advance since the previous symbol selects one of two
 * leaky averages of `*mag`, and once the counter has passed `V29_EPOCH_SETTLE`
 * the epoch is declared when `d` exceeds twice `(a*a + b*b) >> 15` over BOTH
 * averages.  V.27ter compares against four times its ONE average; the trigger
 * constant and the number of averages both differ and neither is transferable.
 *
 * `n_out` IS READ SIGNED, and that IS forced: its 32-bit result indexes
 * `out_i[]` and `out_q[]` (`movswl 0x5e(%ecx),%edx` at 0x9b698), which is
 * CLAUDE.md's forced case.  `fpm_fse.h` models the field `unsigned short` from
 * `FPM_FSE_receive`, so the two functions did not share a declaration and a
 * negative `n_out` indexes BEFORE both arrays; F8587's rule says that has to be
 * PLANTED rather than assumed unreachable, and `t_v29hdx.c` drives it.
 *
 * THE COUNTER RESET IS `0xffff` FOLLOWED BY THE UNCONDITIONAL INCREMENT, so
 * the handover leaves `V29DEC_TRAIN_COUNT` at zero for `V29RX_eq_train` to
 * count up from.  A plain `= 0` before the increment would be a different
 * instruction and a different value on the taken arm; the object's spelling is
 * kept, as it is in `V27RX_epoch_det`.
 *
 * THE SYMBOL COUNTER IS ADVANCED WITHOUT THE SATURATION the other two slicers
 * apply to it.  That is the object (0x9b68c: `movzwl` / `inc` / store, and no
 * compare at all) and it is the one place the three disagree about a field
 * they share.  Reproduced; deviation D1170.
 */
unsigned short
V29RX_epoch_det(struct fpm_fse *state, short *angle, short *mag)
{
	struct v29_rx_decoder *dec = state->cfg.owner;
	short n = (short)state->n_out;
	short i, q;
	short di, dq, ei, eq;
	int d;
	unsigned short a;
	short diff;

	state->lms_on = 0;
	dec->sym_count =
		(unsigned short)(dec->sym_count + 1);

	i = state->out_i[n];
	q = state->out_q[n];

	di = (short)(dec->i1 - i);
	dq = (short)(dec->q1 - q);
	ei = (short)(dec->i2 - dec->i0);
	eq = (short)(dec->q2 - dec->q0);

	d = (short)(((di * di + dq * dq) >> 15)
		    + ((ei * ei + eq * eq) >> 15));

	dec->i2 = dec->i1;
	dec->q2 = dec->q1;
	dec->i1 = dec->i0;
	dec->q1 = dec->q0;
	dec->i0 = (unsigned short)i;
	dec->q0 = (unsigned short)q;

	/*
	 * The phase advance, folded into [0, V29DEC_PHASE_FULL) by SUBTRACTING
	 * a full turn -- which is what the object encodes and is the same
	 * sixteen bits as adding one, because the result is narrowed to
	 * `short` immediately.
	 */
	a = (unsigned short)*angle;
	diff = (short)(a - dec->angle_prev);
	dec->angle_prev = a;
	if (diff < 0)
		diff = (short)(diff - V29DEC_PHASE_FULL);

	/*
	 * TWO STATEMENTS PER AVERAGE, not one, and the double store is forced:
	 * `mag` is a `short *` parameter that may alias the field, so the
	 * compiler has to flush the intermediate before it loads `*mag`.
	 */
	if (diff > V29DEC_HALF_TURN) {
		dec->mag_avg_far =
			(short)((dec->mag_avg_far
				 * V29EPOCH_AVG_WEIGHT) >> V29EPOCH_AVG_SHIFT);
		dec->mag_avg_far =
			(short)(dec->mag_avg_far
				+ (*mag >> V29EPOCH_AVG_SHIFT));
		*angle = V29RX_DEC_ANGLE[V29_EPOCH_POINT_FAR];
	} else {
		dec->mag_avg_near =
			(short)((dec->mag_avg_near
				 * V29EPOCH_AVG_WEIGHT) >> V29EPOCH_AVG_SHIFT);
		dec->mag_avg_near =
			(short)(dec->mag_avg_near
				+ (*mag >> V29EPOCH_AVG_SHIFT));
		*angle = V29RX_DEC_ANGLE[V29_EPOCH_POINT_NEAR];
	}

	if ((short)dec->train_count > V29_EPOCH_SETTLE) {
		short af = dec->mag_avg_far;
		short an = dec->mag_avg_near;
		short avg = (short)((af * af + an * an) >> 15);

		if (d > avg * V29EPOCH_TRIGGER) {
			state->cfg.mu[0] = V29RX_MU_TRAIN;
			state->lms_force = 1;
			state->cfg.decision = V29RX_eq_train;

			dec->train_lfsr =
					V29_TRAIN_LFSR_SEED;
			dec->last = 0;
			dec->train_count = 0xffff;
		}
	}

	dec->train_count =
		(unsigned short)(dec->train_count + 1);

	return 0xffff;
}

/*
 * ---------------------------------------------------------------------------
 * V29RX_eq_train -- .text 0x09b810, 221 bytes.
 *
 * THE SECOND SLICER, AND IT IS A GENERATOR RATHER THAN A SLICER.  It does not
 * look at the equaliser's output at all -- `out_i`, `out_q` and `n_out` are
 * never read -- it runs `V29DEC_TRAIN_LFSR` on one step and hands the
 * equaliser back the constellation point that register's bit 0 selects.  So
 * during training the "decision" is the transmitter's own known sequence, and
 * the LMS loop adapts against it.  That is what `V27RX_eq_train` does too,
 * except that V.27ter's reference is the PREVIOUS decision advanced by half
 * the constellation and V.29's is a pseudo-random bit; the two are not the
 * same mechanism and neither derivation transfers.
 *
 * `mu_sel` IS CLEARED ON EVERY CALL and set to 1 only on the handover, so the
 * equaliser trains on `cfg.mu[0]` and runs on `cfg.mu[1]` -- and the handover
 * also OVERWRITES `cfg.mu[1]` with `V29RX_MU_DATA`, which the configuration in
 * `V29RX_CFG` had already set.  `mse` is parked at `V29RX_TRAIN_MSE` every
 * call, which is four times `V29RX_MSE_RECOVERED`: while this slicer is
 * installed, `RxHdxIdleV29` cannot leave IDLE.
 *
 * THE HANDOVER TEST IS `==`, NOT `>=` (`cmp $0x17e,%cx` / `je`), so it fires on
 * exactly one call and a counter that started above `V29_TRAIN_SYMS` would
 * train for ever.  `V29RX_epoch_det`'s 0xffff-then-increment reset is what
 * guarantees it starts at zero.
 *
 * THE FEEDBACK SHIFT IS LOGICAL AND THAT IS WHAT FIXES THE TYPES: `shr $0x1`
 * on a value the register was `or`ed into, so the intermediate is `unsigned`
 * while the field itself is read `movswl` and is `short`.
 */
unsigned short
V29RX_eq_train(struct fpm_fse *state, short *angle, short *mag)
{
	struct v29_rx_decoder *dec = state->cfg.owner;
	short st = dec->train_lfsr;
	unsigned int x = (unsigned int)st;
	unsigned int odd = x & 1u;
	unsigned short count;
	unsigned short tc;
	short idx;

	count = (unsigned short)(dec->sym_count + 1);
	if (count == V29DEC_SYM_COUNT_WRAP)
		dec->sym_count = V29DEC_SYM_COUNT_RESTART;
	else
		dec->sym_count = count;

	state->lms_on = 1;
	state->mu_sel = 0;
	state->mse = V29RX_TRAIN_MSE;

	x = ((((unsigned int)st << 6) & 0x80u) ^ (odd << 7)) | (unsigned int)st;
	x = (x >> 1) & V29_TRAIN_LFSR_MASK;

	idx = 0;
	if (odd != 0)
		idx = dec->sixteen_point
		    ? V29_TRAIN_POINT_16 : V29_TRAIN_POINT_8;

	*mag = V29RX_DEC_MAG[idx];
	*angle = V29RX_DEC_ANGLE[idx];
	dec->train_lfsr = (short)x;

	tc = (unsigned short)(dec->train_count + 1);
	dec->train_count = tc;
	if (tc == V29_TRAIN_SYMS) {
		state->cfg.mu[1] = V29RX_MU_DATA;
		state->mu_sel = 1;
		state->lms_force = 0;
		state->cfg.decision = V29RX_decision;
	}

	dec->last = (unsigned short)(idx & 7);

	return 0xffff;
}

/*
 * ---------------------------------------------------------------------------
 * V29RX_decision -- .text 0x09b8f0, 260 bytes.
 *
 * The running slicer: a nearest-point search over the constellation, followed
 * by the differential decode.  Where V.27ter searches an ANGLE table, V.29
 * searches the (I, Q) MAP -- a real two-dimensional QAM slicer -- and that is
 * the deepest difference between the two files.
 *
 * THE TWO SQUARED TERMS ARE SHIFTED BY DIFFERENT AMOUNTS, and this is the one
 * thing here that no amount of reading explains: `sar $0xf` on the I term
 * (0x9b977) and `sar $0x10` on the Q term (0x9b97a).  Fifteen and sixteen, in
 * consecutive instructions, on two halves of one Euclidean distance.  It is
 * not a compiler artefact -- the two shifts are on separate registers and both
 * results are added -- and it is not free, because it weights the quadrature
 * error at half the in-phase error and so tilts every decision.  Reproduced
 * exactly as encoded, WITHOUT a claim about what it is for; deviation D1171.
 *
 * THE DISTANCE IS A `short` AND THAT IS FORCED: the object narrows each one
 * with `movswl %dx,%eax` before the comparison, so a point far enough from the
 * sample wraps and compares as its own opposite.  `best` starts at 0x7fff --
 * the largest a `short` can hold, not a computed bound as V.27ter's is -- and
 * the comparison is `<`, so a tie keeps the LOWER index.
 *
 * THE TABLE LOADS ARE `movzwl` AND THE ENTRIES ARE NEGATIVE, which is F614's
 * free case: each load is immediately differenced and the difference narrowed
 * to `short`, so nothing above bit 15 survives.  `v29cfg.c` records the same
 * reading for the same tables from the other end.
 *
 * THE FOURTH BIT IS THE AMPLITUDE BIT.  `V29RX_DEC_PMAP[(cur - prev) & 7]`
 * carries the three phase bits and `bi & 8` -- which ring of the constellation
 * won -- is ORed on top, and ONLY when `V29DEC_SIXTEEN_POINT` is set.  At
 * eight points the search never reaches index 8, so the OR would be a no-op;
 * the object guards it anyway.
 */
unsigned short
V29RX_decision(struct fpm_fse *state, short *angle, short *mag)
{
	struct v29_rx_decoder *dec = state->cfg.owner;
	short n;
	short points;
	short i, q;
	short best, bi, k;
	unsigned short count;
	unsigned short prev;
	short cur;
	unsigned short sym;

	count = (unsigned short)(dec->sym_count + 1);
	if (count == V29DEC_SYM_COUNT_WRAP)
		dec->sym_count = V29DEC_SYM_COUNT_RESTART;
	else
		dec->sym_count = count;

	n = (short)state->n_out;
	points = dec->sixteen_point ? 16 : 8;
	i = state->out_i[n];
	q = state->out_q[n];

	best = 0x7fff;
	bi = 0;
	for (k = 0; k < points; k = (short)(k + 1)) {
		short di = (short)(i - V29RX_DEC_IMAP[k]);
		short dq = (short)(q - V29RX_DEC_QMAP[k]);
		short d = (short)(((di * di) >> 15) + ((dq * dq) >> 16));

		if (d < best) {
			best = d;
			bi = k;
		}
	}

	*mag = V29RX_DEC_MAG[bi];
	cur = (short)(bi & 7);
	*angle = V29RX_DEC_ANGLE[bi];

	prev = dec->last;
	dec->last = (unsigned short)cur;

	sym = (unsigned short)V29RX_DEC_PMAP[(cur - prev) & 7];
	if (dec->sixteen_point != 0)
		sym = (unsigned short)(sym | (bi & 8));

	return sym;
}

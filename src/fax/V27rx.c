/*
 * V27rx.c -- split out of the merged v27.c so the definitions sit in the
 * translation unit the object's FILE order gives them.  Bodies moved
 * verbatim; no source text changed.  See finding F11390.
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
 * compiler can tell, and `((struct v27_rx_shared *))->rate` below is written the same way for the same
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
	struct v27_rx_shared *sh;
	struct v27_rx_block *rx;
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
		((struct v27_rx *)modem)->shared = 0;
		((struct v27_rx *)modem)->rx = 0;
		fresh_handle = 1;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("\n");

	/* The handle's first 28 bytes ARE the configuration.  See v27fax.h. */
	if (cfg == 0)
		((struct v27_rx *)modem)->cfg = V27RX_CFG;
	else
		((struct v27_rx *)modem)->cfg = *cfg;

	/* ---- the shared block ---------------------------------------- */

	sh = ((struct v27_rx *)modem)->shared;
	if (sh == 0) {
		sh = sysdep_malloc(V27SH_SIZE);
		((struct v27_rx *)modem)->shared = sh;
		fresh_sh = 1;
		sh->mtd = 0;
		sh->buf = sysdep_malloc(V27SH_BUF_BYTES);
		sh = ((struct v27_rx *)modem)->shared;
		sh->mtd_v21 = 0;
	}

	sh->int_0004 = 0;
	sh->rx_state = V27RX_STATE_START;
	sh->countdown = 0;
	sh->handler = RxHdxStartV27;
	sh->train_long =
		(short)((&((struct v27_rx *)modem)->cfg)->short_train == 0);

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
	sh = ((struct v27_rx *)modem)->shared;
	sh->mtd_v21 = FPM_MTD_create(
		(struct fpm_mtd *)sh->mtd_v21, &mcfg);

	FPM_AGC_init(&((struct v27_rx *)modem)->shared->agc,
		     &AGCv27_CFG, fresh_sh);

	sh = ((struct v27_rx *)modem)->shared;
	sh->v21_samples = 0;
	sh->v21_armed = 0;

	/*
	 * THE RATE, and the two numbers are V.27ter's own.  An unrecognised
	 * one is reported through the status word and then treated as 4800.
	 */
	if ((&((struct v27_rx *)modem)->cfg)->bit_rate == 2400) {
		sh->rate = V27SH_RATE_2400;
	} else if ((&((struct v27_rx *)modem)->cfg)->bit_rate == 4800) {
		sh->rate = V27SH_RATE_4800;
	} else {
		sh->rate = V27SH_RATE_4800;
		((struct v27_rx *)modem)->result.byte.flags |= V27_STATUS_FLAG_ERROR;
		((struct v27_rx *)modem)->result.byte.status = V27_STATUS_DEFAULT;
	}

	/* The data-channel detector, which does depend on the rate. */
	mcfg = FPM_MTD_CFG;
	mcfg.coeff = sh->rate == V27SH_RATE_4800 ? V27_MTD_COEFF_4800
						 : V27_MTD_COEFF_2400;
	mcfg.tones = V27_MTD_TONES;
	mcfg.ratio = V27_MTD_RATIO;
	mcfg.min_level = V27_MTD_MIN_LEVEL;
	sh = ((struct v27_rx *)modem)->shared;
	sh->mtd = FPM_MTD_create(
		(struct fpm_mtd *)sh->mtd, &mcfg);

	/* ---- the receive block ---------------------------------------- */

	aux = (&((struct v27_rx *)modem)->cfg)->ptr_0018;

	rx = ((struct v27_rx *)modem)->rx;
	if (rx == 0) {
		rx = sysdep_malloc(V27RX_BLOCK_SIZE);
		((struct v27_rx *)modem)->rx = rx;
		rx->buf_a = sysdep_malloc(V27RX_BUF_A_BYTES);
		rx = ((struct v27_rx *)modem)->rx;
		rx->buf_b = sysdep_malloc(V27RX_BUF_B_BYTES);
	}

	rcfg = FPM_MRF_CFG;
	rcfg.aux = aux;
	sh = ((struct v27_rx *)modem)->shared;
	rcfg.branches = V27RX_MRF_UP[sh->rate];
	rcfg.decimate = V27RX_MRF_DOWN[sh->rate];
	rcfg.coeff = V27RX_MRF_FILT[sh->rate];
	rcfg.taps = V27RX_MRF_FILT_LEN[sh->rate];
	FPM_MRF_init(&((struct v27_rx *)modem)->rx->mrf, &rcfg,
		     fresh_handle);

	FPM_AGC_init(&((struct v27_rx *)modem)->rx->agc, &AGCv27_CFG,
		     fresh_handle);

	/*
	 * The measurement block, overridden at 4800 only.  The object patches
	 * the LIVE gain control rather than the configuration it was just
	 * given, so it is a post-init fix-up and not a fifth stack config.
	 */
	sh = ((struct v27_rx *)modem)->shared;
	if (sh->rate == V27SH_RATE_4800)
		((struct v27_rx *)modem)->rx->agc.cfg.block_len =
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
	sh = ((struct v27_rx *)modem)->shared;
	scfg.clock_len = V27RX_SAMP_PER_BAUD[sh->rate];
	scfg.coeffs = V27RX_SRE_FILT_LEN[sh->rate];
	scfg.proto = V27RX_SRE_FILT[sh->rate];
	scfg.disc = V27RX_XB_COFFS[sh->rate];
	scfg.xclock = V27RX_XCLOCK[sh->rate];
	scfg.yclock = V27RX_YCLOCK[sh->rate];
	scfg.pll_k1 = V27RX_SRE_PLLK1[sh->rate];
	scfg.pll_k2 = V27RX_SRE_PLLK2[sh->rate];
	scfg.mag_hi = V27_SRE_MAG_HI;
	scfg.mag_lo = V27_SRE_MAG_LO;
	scfg.err_hi = V27_SRE_ERR_HI;
	scfg.err_lo = V27_SRE_ERR_LO;
	/* Read back out of the gain control initialised four lines up. */
	scfg.rms_min = (short)
		(((struct v27_rx *)modem)->rx->agc.cfg.ref_level
		 / V27_SRE_RMS_MIN_DIV);
	scfg.rms_len = (short)(V27_SRE_RMS_LEN_SYMS
			       * V27RX_SAMP_PER_BAUD[sh->rate]);
	FPM_SRE_init(&((struct v27_rx *)modem)->rx->sre, &scfg,
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
	sh = ((struct v27_rx *)modem)->shared;
	sre = &((struct v27_rx *)modem)->rx->sre;
	sre->ppm_step = (short)(sh->rate == V27SH_RATE_2400
				? V27_SRE_PPM_STEP_2400
				: V27_SRE_PPM_STEP_4800);
	period = (short)(sre->ppm_step * V27_SRE_PPM_UNIT);
	sre->ppm_period = period;
	sre->ppm_scale = (short)(V27_SRE_PPM_MILLION
				 / (period * scfg.clock_len));
	sre->ppm_n_max = (short)(V27_SRE_PPM_MILLION / period);

	fcfg = FPM_FSE_CFG;
	fcfg.reserved34 = aux;
	sh = ((struct v27_rx *)modem)->shared;
	fcfg.block = (short)(sh->rate == V27SH_RATE_2400 ? V27_FSE_BLOCK_2400
							 : V27_FSE_BLOCK_4800);
	fcfg.interp = V27RX_SAMP_PER_BAUD[sh->rate];
	fcfg.icoff = V27RX_FSE_IFILT[sh->rate];
	fcfg.qcoff = V27RX_FSE_QFILT[sh->rate];
	fcfg.taps = V27RX_FSE_FILT_LEN[sh->rate];
	fcfg.mu[0] = V27RX_FSE_MU_TRAIN[sh->rate];
	fcfg.mu[1] = V27RX_FSE_MU_TRACK[sh->rate];
	fcfg.clk = V27RX_CRR_TABLE[sh->rate];
	fcfg.clk_mod = V27RX_CRR_TABLE_LEN[sh->rate];
	fcfg.train_sym = V27_FSE_TRAIN_SYM;
	fcfg.err_hi = V27_FSE_ERR_HI;
	fcfg.err_lo = V27_FSE_ERR_LO;
	fcfg.clk_inc = V27RX_CRR_ADJUST[sh->rate];
	fcfg.pll_k1 = V27RX_FSE_PLLK1[sh->rate];
	fcfg.pll_k2 = V27RX_FSE_PLLK2[sh->rate];
	fcfg.owner = &((struct v27_rx *)modem)->rx->dec;
	fcfg.decision = V27RX_epoch_det;
	FPM_FSE_init(&((struct v27_rx *)modem)->rx->fse, &fcfg,
		     fresh_handle);

	/* ---- the scratch buffers, the smoothers, the decoder ---------- */

	rx = ((struct v27_rx *)modem)->rx;
	bufa = (short *)rx->buf_a;
	bufb = (short *)rx->buf_b;
	/*
	 * 160 entries of each, with a `short` induction variable (`inc` then
	 * `cwtl` at 0x99bcc).  `V27RX_BUF_B` is four bytes longer than that
	 * and its last two entries are left as `sysdep_malloc` returned them.
	 */
	for (i = 0; i < V27RX_BUF_ZERO; i = (short)(i + 1)) {
		bufa[i] = 0;
		bufb[i] = 0;
	}

	rx->q_flag = 0;
	rx->q_acc = 0;
	rx->q_count = 0;

	sh = ((struct v27_rx *)modem)->shared;
	rate = sh->rate;
	if (rate == V27SH_RATE_2400)
		rx->q_limit = V27RX_Q_LIMIT_2400;
	else if (rate == V27SH_RATE_4800)
		rx->q_limit = V27RX_Q_LIMIT_4800;

	rx->dec.epoch_i0 = 0;
	rx->rms_count = 0;
	rx->rms_on = 1;
	rx->rms_ref = 0;
	rx->dec.epoch_q0 = 0;
	rx->dec.epoch_i1 = 0;
	rx->dec.epoch_q1 = 0;
	rx->dec.epoch_i2 = 0;
	rx->dec.epoch_q2 = 0;

	rx->dec.eight_phase =
					sh->rate == V27SH_RATE_4800;
	rx->dec.last = 0;
	rx->dec.phase_mask =
					(unsigned short)
					V27RX_DEC_PHS_MASK[sh->rate];
	rx->dec.train_count = 0;
	rx->dec.epoch_avg = V27DEC_MAG;
	rx->dec.angle_prev = 0;
	rx->dec.train_short =
					sh->train_long == 0;
	rx->dec.sym_count = 0;
	rx->dec.pmap =
					V27RX_DEC_PMAP[sh->rate];
	rx->dec.angles =
					V27RX_DEC_LAST_PHASE[sh->rate];

	dcfg = SDMv27_CFG;
	dcfg.nbits = (unsigned short)(V27_SDM_NBITS_4800
				      - (sh->rate == V27SH_RATE_2400));
	SDMv27_init((struct sdmv27 *)(void *)
			&((struct v27_rx *)modem)->rx->sdm,
		    &dcfg);

	/* ---- the enables, the status word and the equaliser view ------ */

	rx = ((struct v27_rx *)modem)->rx;
	rx->int_0000 = 1;
	rx->en_sre_adapt = 1;
	rx->en_fse_pll = 1;
	rx->int_000c = 0;
	rx->en_fse_lms = 1;

	((struct v27_rx *)modem)->result.word = 0;
	((struct v27_rx *)modem)->result.byte.flags |= V27_STATUS_FLAGS_SEED;
	((struct v27_rx *)modem)->result.byte.status = V27_STATUS_START;

	fse = (&rx->fse);
	((struct v27_rx *)modem)->eq_out_i = fse->out_i;
	((struct v27_rx *)modem)->eq_out_q = fse->out_q;
	((struct v27_rx *)modem)->eq_n_out = &fse->n_out;
	((struct v27_rx *)modem)->eq_icoeff = fse->icoeff;
	((struct v27_rx *)modem)->eq_qcoeff = fse->qcoeff;
	((struct v27_rx *)modem)->eq_taps = (unsigned short)fse->cfg.taps;

	((struct v27_rx *)modem)->short_0040 = 0;
	((struct v27_rx *)modem)->short_004c = 0;
	((struct v27_rx *)modem)->int_0038 = 0;
	((struct v27_rx *)modem)->int_003c = 0;
	((struct v27_rx *)modem)->int_0044 = 0;
	((struct v27_rx *)modem)->int_0048 = 0;

	return modem;
}

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
	struct v27_rx_block *rx;
	struct v27_rx_shared *sh;

	rx = ((struct v27_rx *)modem)->rx;
	FPM_FSE_free((&rx->fse));

	rx = ((struct v27_rx *)modem)->rx;
	FPM_SRE_free((&rx->sre));

	rx = ((struct v27_rx *)modem)->rx;
	FPM_MRF_free((&rx->mrf));

	rx = ((struct v27_rx *)modem)->rx;
	sysdep_free(rx->buf_b);

	rx = ((struct v27_rx *)modem)->rx;
	sysdep_free(rx->buf_a);

	rx = ((struct v27_rx *)modem)->rx;
	sysdep_free(rx);

	sh = ((struct v27_rx *)modem)->shared;
	FPM_MTD_delete((struct fpm_mtd *)sh->mtd);

	sh = ((struct v27_rx *)modem)->shared;
	sysdep_free(sh->buf);

	sh = ((struct v27_rx *)modem)->shared;
	FPM_MTD_delete((struct fpm_mtd *)sh->mtd_v21);

	sh = ((struct v27_rx *)modem)->shared;
	sysdep_free(sh);

	sysdep_free(modem);
}

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
	struct v27_rx_decoder *dec = state->cfg.owner;
	short n = (short)state->n_out;
	short i, q;
	short di, dq, ei, eq;
	short avg;
	int limit;
	int d, e;

	(void)angle;
	(void)mag;

	dec->sym_count =
		(unsigned short)(dec->sym_count + 1);

	limit = dec->train_short ? V27EPOCH_SYMS_SHORT
						 : V27EPOCH_SYMS_LONG;

	i = state->out_i[n];
	q = state->out_q[n];

	/*
	 * The new point against the one-back slot, and the two-back slot
	 * against the newest one -- both of which are two symbols apart once
	 * the shift below has happened.
	 */
	di = (short)(dec->epoch_i1 - i);
	dq = (short)(dec->epoch_q1 - q);
	ei = (short)(dec->epoch_i2
		     - dec->epoch_i0);
	eq = (short)(dec->epoch_q2
		     - dec->epoch_q0);

	d = (short)(((di * di + dq * dq) >> 15)
		    + ((ei * ei + eq * eq) >> 15));

	dec->epoch_i2 = dec->epoch_i1;
	dec->epoch_q2 = dec->epoch_q1;
	dec->epoch_i1 = dec->epoch_i0;
	dec->epoch_q1 = dec->epoch_q0;
	dec->epoch_i0 = (unsigned short)i;
	dec->epoch_q0 = (unsigned short)q;

	e = (short)((i * i + q * q) >> 15);

	if (((short)dec->train_count) > limit) {
		avg = (short)(((dec->epoch_avg
				* V27EPOCH_AVG_WEIGHT) >> V27EPOCH_AVG_SHIFT)
			      + (e >> V27EPOCH_AVG_SHIFT));
		dec->epoch_avg = avg;

		if (d > avg * V27EPOCH_TRIGGER) {
			dec->train_count = 0xffff;
			state->lms_force = 1;
			state->cfg.decision = V27RX_eq_train;
		}
	} else {
		dec->epoch_avg = (short)e;
	}

	dec->train_count =
		(unsigned short)(dec->train_count + 1);

	return 0xffff;
}

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
	struct v27_rx_decoder *dec = state->cfg.owner;
	const short *tbl;
	unsigned short count;
	short step;
	short diff;
	short err;
	short a;
	short limit;
	short i;

	/* Saturating, and it restarts at half scale -- V27RX_decision's. */
	count = (unsigned short)(dec->sym_count + 1);
	if (count == V27DEC_PHASE_FULL)
		dec->sym_count = V27DEC_PHASE_FULL / 2;
	else
		dec->sym_count = count;

	/* Half the constellation: 4 of 8, or 2 of 4. */
	step = dec->eight_phase ? 4 : 2;

	diff = (short)(*angle - dec->angle_prev);
	if (diff > V27DEC_HALF_TURN)
		diff = (short)(diff + V27DEC_PHASE_FULL);
	if (diff < -V27DEC_HALF_TURN)
		diff = (short)(diff - V27DEC_PHASE_FULL);

	*mag = V27DEC_MAG;

	err = (short)(diff < 0 ? (short)-diff : diff);
	if (err > V27DEC_QUARTER_TURN)
		dec->last =
			(short)((dec->last + step)
				& dec->phase_mask);

	tbl = (const short *)dec->angles;
	a = tbl[dec->last];
	limit = dec->train_short ? V27DEC_TRAIN_SYMS_SHORT
						 : V27DEC_TRAIN_SYMS_LONG;
	*angle = a;
	dec->angle_prev = a;
	dec->train_count =
		(unsigned short)(dec->train_count + 1);

	state->mu_sel = 0;

	if (((short)dec->train_count) >= limit) {
		for (i = 0; i < state->cfg.taps; i = (short)(i + 1)) {
			/* No body in the object.  See the note above. */
		}
		state->lms_force = 0;
		state->mu_sel = 1;
		state->cfg.decision = V27RX_decision;
	}

	return 0xffff;
}

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
	struct v27_rx_decoder *dec = state->cfg.owner;
	const short *tbl = (const short *)dec->angles;
	const short *pmap = (const short *)dec->pmap;
	short n = dec->eight_phase ? 8 : 4;
	unsigned short count;
	short best, bi, k;
	int diff;

	diff = *angle - tbl[dec->last];

	/* Saturating, and it restarts at half scale rather than at zero. */
	count = (unsigned short)(dec->sym_count + 1);
	if (count == V27DEC_PHASE_FULL)
		dec->sym_count = V27DEC_PHASE_FULL / 2;
	else
		dec->sym_count = count;

	/* One revolution, taken as [0, V27DEC_PHASE_FULL]. */
	if (diff < 0)
		diff += V27DEC_PHASE_FULL;
	if (diff > V27DEC_PHASE_FULL)
		diff -= V27DEC_PHASE_FULL;

	best = (short)(diff >= V27DEC_PHASE_FULL
	     ? (short)(diff - V27DEC_PHASE_FULL)
	     : (short)(V27DEC_PHASE_FULL - diff));
	bi = 0;

	for (k = 0; k < n; k = (short)(k + 1)) {
		short d;

		d = (short)(diff >= tbl[k] ? (short)(diff - tbl[k])
				   : (short)(tbl[k] - diff));
		if (d < best) {
			bi = k;
			best = d;
		}
	}

	dec->last = (short)((dec->last + bi)
					    & dec->phase_mask);
	*mag = V27DEC_MAG;
	*angle = tbl[dec->last];

	return (unsigned short)pmap[bi];
}

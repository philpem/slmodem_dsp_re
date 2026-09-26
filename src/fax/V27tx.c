/*
 * V27tx.c -- split out of the merged v27.c so the definitions sit in the
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
 * is multiplied by the config's own `scale_mul` (default 1, so invisible on
 * `V27TX_CFG` itself) -- V.27ter's own caller-adjustable output gain, which
 * V.29's `V29TX_create` does not have at the identical field.
 */
void *
V27TX_create(void *modem, const struct v27tx_cfg *params)
{
	struct v27_tx_source *prm;
	int fresh = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V.27 TX Create ");

	if (modem == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("New allocation\n");

		modem = sysdep_malloc(0x2c);
		((struct v27_tx *)modem)->source = 0;
		((struct v27_tx *)modem)->tx = 0;
		fresh = 1;
	} else {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\n");
	}

	if (params != 0)
		((struct v27_tx *)modem)->cfg = *params;
	else
		((struct v27_tx *)modem)->cfg = V27TX_CFG;

	((struct v27_tx *)modem)->result.word = 0;
	((struct v27_tx *)modem)->result.byte.flags |= 0x58;
	((struct v27_tx *)modem)->result.byte.status = 1;

	/* ---- the data-source block: the FIFO and the SGD ------------- */

	prm = ((struct v27_tx *)modem)->source;
	if (prm == 0) {
		prm = sysdep_malloc(V27TXDATA_SIZE);
		((struct v27_tx *)modem)->source = prm;
		prm->fifo = 0;
		prm->sgd = 0;
	}

	{
		struct sgd_cfg gcfg = SGD_CFG;
		struct sgd *existing;

		gcfg.sym_bits = 3;

		existing = prm->sgd;
		prm->sgd =
			SGD_create(existing, &gcfg);
	}

	/* ---- the half-duplex machine's own state ---------------------- */

	prm = ((struct v27_tx *)modem)->source;
	prm->int_0008 = 0;
	prm->state = V27TX_STATE_START;
	prm->countdown = 0;
	prm->handler = TxHdxStartV27;
	prm->train_long = (short)
		((&((struct v27_tx *)modem)->cfg)->int_0018 == 0);

	if ((&((struct v27_tx *)modem)->cfg)->bitrate == 2400) {
		prm->rate = 0;
	} else if ((&((struct v27_tx *)modem)->cfg)->bitrate == 4800) {
		prm->rate = 1;
	} else {
		prm->rate = 1;
		((struct v27_tx *)modem)->result.byte.flags |= V27TX_RESULT_B1_BIT1;
		((struct v27_tx *)modem)->result.byte.status = V27TX_STATUS_DEFAULT;
	}

	/* ---- the FIFO --------------------------------------------------- */

	{
		struct fifo_cfg fc;
		unsigned short n = (unsigned short)
			(&((struct v27_tx *)modem)->cfg)->fifo_size_factor;
		struct fax_fifo *existing;
		short rate;

		prm = ((struct v27_tx *)modem)->source;
		rate = prm->rate;

		fc.word0 = FIFO_CFG.word0;
		fc.fill = 0;
		fc.size = (short)(n * V27TX_FRMSIZE[rate]);

		existing = prm->fifo;
		prm->fifo =
			FIFO_create(existing, &fc);
	}

	/* ---- the private block: the ring, the scrambler, the symbol coder
	 * and the pulse shaper ---------------------------------------------- */

	{
		struct v27_tx_block *tx;
		short rate;
		short ring_len;

		prm = ((struct v27_tx *)modem)->source;
		rate = prm->rate;
		ring_len = (short)(V27TX_FRMSIZE[rate] + 2);

		tx = ((struct v27_tx *)modem)->tx;
		if (tx == 0) {
			struct fpm_smc_ring *ring;

			tx = sysdep_malloc(0x94);
			((struct v27_tx *)modem)->tx = tx;
			ring = (struct fpm_smc_ring *)(void *)
					&tx->ring;
			ring->sym = (short *)
				sysdep_malloc((unsigned)(ring_len * 2));
		}

		tx = ((struct v27_tx *)modem)->tx;
		{
			struct fpm_smc_ring *ring = (struct fpm_smc_ring *)
					(void *)&tx->ring;
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
		struct v27_tx_block *tx = ((struct v27_tx *)modem)->tx;
		short rate = ((struct v27_tx *)modem)->source->rate;

		scfg.f00 = 1;
		scfg.direct = 0;
		scfg.rot_step = V27TX_SMC_CRR_ADJ[rate];
		scfg.rot_mod = V27TX_SMC_CRR_LEN[rate];
		scfg.qshift = 0;
		scfg.qmask = (unsigned short)V27TX_SMC_PHS_MASK[rate];
		scfg.amask = 0;
		scfg.pmask = (unsigned short)V27TX_SMC_PHS_MASK[rate];
		scfg.pmap = V27TX_SMC_PMAP[rate];

		SMC_init(&tx->smc, &scfg);
	}

	{
		struct fpm_pps_cfg pcfg = FPM_PPS_CFG;
		struct v27_tx_block *tx = ((struct v27_tx *)modem)->tx;
		short rate = ((struct v27_tx *)modem)->source->rate;

		pcfg.phases = V27TX_PPS_UP_FACT[rate];
		pcfg.step = V27TX_PPS_DOWN_FACT[rate];
		pcfg.mapped = 1;
		pcfg.scale = V27TX_PPS_SCALE[rate] *
			(&((struct v27_tx *)modem)->cfg)->scale_mul;
		pcfg.step_adj = 0;
		pcfg.imap = V27TX_PPS_IMAP[rate];
		pcfg.qmap = V27TX_PPS_QMAP[rate];
		pcfg.coeff_i = V27TX_PPS_IFILT[rate];
		pcfg.coeff_q = V27TX_PPS_QFILT[rate];
		pcfg.coeffs = V27TX_PPS_FILT_LEN[rate];
		pcfg.aux = (void *)(long)(&((struct v27_tx *)modem)->cfg)->int_001c;

		FPM_PPS_init(&tx->pps,
			    &pcfg, fresh);
	}

	{
		struct sdmv27_cfg dcfg;
		struct v27_tx_block *tx = ((struct v27_tx *)modem)->tx;

		dcfg.nbits = 3;
		SDMv27_init(&tx->sdm,
			   &dcfg);
	}

	return modem;
}

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
	struct v27_tx_block *tx;
	struct v27_tx_source *src;

	tx = ((struct v27_tx *)modem)->tx;
	FPM_PPS_free(&tx->pps);

	/*
	 * The SYMBOL RING's buffer, not a scratch allocation of the
	 * transmitter's.  The object frees `*(tx + 0x10)`, and 0x10 is
	 * `V27TX_RING + offsetof(struct fpm_smc_ring, sym)`; see v27fax.h and
	 * F9121 for why there is no room for a separate field there.
	 */
	tx = ((struct v27_tx *)modem)->tx;
	sysdep_free(((struct fpm_smc_ring *)(void *)
			&tx->ring)->sym);

	tx = ((struct v27_tx *)modem)->tx;
	sysdep_free(tx);

	src = ((struct v27_tx *)modem)->source;
	FIFO_delete((struct fax_fifo *)src->fifo);

	src = ((struct v27_tx *)modem)->source;
	SGD_delete((struct sgd *)src->sgd);

	src = ((struct v27_tx *)modem)->source;
	sysdep_free(src);

	sysdep_free(modem);
}

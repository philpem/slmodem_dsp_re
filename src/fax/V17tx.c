/*
 * V17tx.c -- split out of the merged v17.c so the definitions sit in
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
	struct v17tx_priv *prm;
	struct v17tx_fp *fp;
	void *existing;
	int fresh = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V.17 TX Create ");

	if (modem == 0) {
		modem = sysdep_malloc(0x2c);
		TXROOT(modem)->priv = 0;
		TXROOT(modem)->fp = 0;
		fresh = 1;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("New allocation\n");
	} else {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\n");
	}

	if (params != 0)
		TXROOT(modem)->cfg = *params;
	else
		TXROOT(modem)->cfg = V17TX_CFG;

	TXROOT(modem)->result.word = 0;
	TXROOT(modem)->result.byte.flags |= 0x58;
	TXROOT(modem)->result.byte.status = 1;

	/* ---- the parameter/half-duplex block, the FIFO and the SGD ------- */

	prm = TXP(modem);
	if (prm == 0) {
		prm = sysdep_malloc(0x20);
		TXROOT(modem)->priv = (struct v17tx_priv *)prm;
		TXPRIV(modem)->fifo = 0;
		TXPRIV(modem)->sgd = 0;
	}

	{
		struct fifo_cfg fc;
		unsigned short n = (unsigned short)
			TXROOT(modem)->cfg.fifo_size_factor;

		fc.word0 = FIFO_CFG.word0;
		fc.size = (short)(n * 3 * 16);
		fc.fill = 0;

		existing = TXPRIV(modem)->fifo;
		TXPRIV(modem)->fifo =
			FIFO_create((struct fax_fifo *)existing, &fc);
	}

	{
		struct sgd_cfg gcfg = SGD_CFG;

		gcfg.sym_bits = 2;

		existing = TXPRIV(modem)->sgd;
		TXPRIV(modem)->sgd = SGD_create((struct sgd *)existing, &gcfg);
	}

	/* ---- the half-duplex machine's own state ------------------------- */

	prm = TXP(modem);
	TXPRIV(modem)->state = V17TX_STATE_START;
	TXPRIV(modem)->countdown = 0;
	TXPRIV(modem)->no_carrier_sym = 4;
	TXPRIV(modem)->r08 = 0;
	TXPRIV(modem)->process = TxHdxStartV17;
	((struct v17tx_priv *)prm)->r0c = TXROOT(modem)->cfg.int_0018;

	if (TXROOT(modem)->cfg.bitrate == 9600) {
		TXPRIV(modem)->mode = 1;
	} else if (TXROOT(modem)->cfg.bitrate == 12000) {
		TXPRIV(modem)->mode = 2;
	} else if (TXROOT(modem)->cfg.bitrate == 7200) {
		TXPRIV(modem)->mode = 0;
	} else if (TXROOT(modem)->cfg.bitrate == 14400) {
		TXPRIV(modem)->mode = 3;
	} else {
		TXPRIV(modem)->mode = 3;
		TXROOT(modem)->result.byte.flags |= V17TX_RESULT_B1_BIT1;
		TXROOT(modem)->result.byte.status = V17TX_RESULT_BYTE_07;
	}

	/* ---- the private block: the ring, the scrambler, the symbol coder
	 * and the pulse shaper ---------------------------------------------- */

	fp = TXFP(modem);
	if (fp == 0) {
		fp = sysdep_malloc(0x90);
		TXROOT(modem)->fp = fp;
		TXBLOCK(modem)->ring.sym = (short *)sysdep_malloc(0x64);
	}

	{
		struct fpm_smc_ring *ring = &TXBLOCK(modem)->ring;
		short i;

		ring->i = 0;
		ring->q = 0;
		ring->widx = 0;
		ring->ridx = 0;
		ring->len = 0x32;

		for (i = 0; i <= 0x31; i++)
			ring->sym[i] = 0;
	}

	{
		struct fpm_sdm_cfg dcfg;

		dcfg.nbits = 2;
		dcfg.tap1 = 0x12;
		dcfg.tap2 = 0x17;

		SDM_init(&TXBLOCK(modem)->sdm, &dcfg);
	}

	{
		short scfg[2];

		scfg[0] = SMCv17_CFG[0];
		scfg[1] = SMCv17_CFG[1];
		SMCv17_init(&TXBLOCK(modem)->smc, scfg);
	}

	{
		struct fpm_pps_cfg pcfg = FPM_PPS_CFG;

		pcfg.phases = 10;
		pcfg.step = 3;
		pcfg.mapped = 1;
		pcfg.scale = V17TX_PPS_SCALE[TXPRIV(modem)->mode];
		pcfg.step_adj = 0;
		pcfg.imap = SMCv17_IMAP4;
		pcfg.qmap = SMCv17_QMAP4;
		pcfg.coeff_i = PPSv17_ICOFFS;
		pcfg.coeff_q = PPSv17_QCOFFS;
		pcfg.coeffs = 120;
		pcfg.aux = (void *)(long)TXROOT(modem)->cfg.int_001c;

		FPM_PPS_init(&TXBLOCK(modem)->pps, &pcfg, fresh);
	}

	TXBLOCK(modem)->encoders[0] = SMCv17_encoder_dif;
	TXBLOCK(modem)->encoders[1] = SMCv17_encoder_abs;
	TXBLOCK(modem)->encoders[2] = (v17_encoder_fn)SMCv17_encoder_tcm;

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
	FPM_PPS_free(&TXBLOCK(modem)->pps);
	sysdep_free(TXBLOCK(modem)->ring.sym);
	sysdep_free(TXBLOCK(modem));

	SGD_delete(TXPRIV(modem)->sgd);
	FIFO_delete(TXPRIV(modem)->fifo);
	sysdep_free(TXPRIV(modem));

	sysdep_free(modem);
}

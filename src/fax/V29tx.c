/*
 * V29tx.c -- split out of the merged v29.c / v29data.c so the definitions sit in
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
 * V29TX_create -- .text 0x09ba00, 1,162 bytes.
 *
 * THE SHAPE IS `V21TX_create`'s, ONE MODULATION OVER: allocate-or-reuse the
 * handle, copy the caller's config (or `V29TX_CFG`) onto its first 28 bytes,
 * allocate-or-reuse the parameter/half-duplex block and build the FIFO and
 * the SGD generator into it, derive `V29TXP_RATE`, seed the half-duplex
 * machine at `V29TX_STATE_START`, then allocate-or-reuse `struct v29tx` and
 * initialise its three sub-objects.  Two debug strings, "V.29 TX Create "
 * (0x4889) then either "New allocation\n" (0x4899) or "\n" (0x4887),
 * exactly as `V21TX_create`'s three collapse to two here because there is
 * no third, unconditional message.
 *
 * `V29TXP_RATE` IS DERIVED HERE, NOT FROM ANY DEDICATED "SET RATE" CALL --
 * see its own comment in v29data.h.  The three-way compare
 * (`V29_BPS_7200`/`V29_BPS_9600`/anything else) is `V29RX_create`'s own for
 * `V29DET_RATE`, read one modulation over; the "anything else" arm ALSO
 * raises `V29TX_RESULT_B1_BIT1` and reports `V29TX_STATUS_DEFAULT`
 * (0x9bb5f/0x9bb63), which the two recognised arms (0x9be66, 0x9be71) skip
 * by jumping straight back into the shared tail.
 *
 * `fresh` IS THE SAME STACK SLOT FROM ENTRY TO EXIT -- the object spills the
 * "did THIS call allocate the handle" flag at one local (0x1c(%esp)) at
 * 0x9ba11/0x9be5d and reloads the identical slot at 0x9bd21 to pass as
 * `FPM_PPS_init`'s third argument, 160 bytes of unrelated code later.  Forced
 * by the register/stack allocator reusing one slot for the whole function,
 * exactly the carrier CLAUDE.md's codegen section describes; reproduced as
 * one C local rather than two, which is what makes the reuse fall out on its
 * own.
 *
 * THE TRANSMIT FIFO'S SIZE IS COMPUTED, NOT A LITERAL -- `int_0014 * 3 * 16`
 * (`lea (%eax,%eax,2),%esi; shl $0x4,%esi` at 0x9bac0/0x9bac5), which is 48
 * for the default config's `int_0014` of 1.  `word0` and `fill` come
 * unchanged from `FIFO_CFG`, matching `V21TX_create`'s own note that only
 * `word0`'s zero matters and the size is the caller's to set.
 *
 * THE SGD GENERATOR TAKES `SGD_CFG` WITH ONLY `sym_bits` PATCHED, to 4 --
 * the whole 13-dword template is copied (`rep movsl`, 0x9bae5) and every
 * other field survives, unlike the FIFO and PPS configs below, which patch
 * several fields each.
 *
 * `RING.SYM` IS REALLOCATED ON EVERY CALL, EVEN WHEN THE TX BLOCK IS REUSED
 * -- `sysdep_malloc(0x64)` at 0x9bb85 runs unconditionally after the
 * fresh/reuse branch converges, with no free of whatever `ring.sym`
 * previously held.  Reproduced as observed; a caller that repeatedly
 * re-initialises an existing transmit handle leaks one 100-byte buffer per
 * call, same as the object.
 *
 * SDM_init's `nbits` AND SMC's `amask` ARE ALSO KEYED ON `V29TXP_RATE`,
 * READ BACK OUT OF THE FIELD THIS SAME FUNCTION JUST WROTE (0x9bbfe and
 * 0x9bc78) -- not from a local kept in a register, so both reads are
 * reproduced as fresh field reads rather than of one cached value, matching
 * the object's own reload discipline elsewhere in this file.
 *
 * `PCFG.AUX` CARRIES THE CALLER'S OWN `int_0018`, ACROSS THE INT/POINTER
 * BOUNDARY -- `mov 0x18(%ebp),%ebx` at 0x9bb67 (the FULL 32 bits, unlike
 * `int_0014`'s 16-bit read above) is spilled to a stack slot and reloaded
 * at 0x9bcbf, 260 bytes later, to become `FPM_PPS_CFG`'s `aux` field
 * (0x9bccf).  `V29TX_CFG`'s own `int_0018` is 0, so this is invisible on
 * the default config; the `(void *)(long)` idiom is D1250's, for the same
 * reason.
 *
 * Finding F9701.
 */
void *
V29TX_create(void *modem, const struct v29tx_cfg *params)
{
	struct v29_tx_params *prm;
	void *existing;
	int fresh = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V.29 TX Create ");

	if (modem == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("New allocation\n");

		modem = sysdep_malloc(0x28);
		((struct v29_tx_root *)modem)->params = 0;
		((struct v29_tx_root *)modem)->tx = 0;
		fresh = 1;
	} else {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\n");
	}

	if (params != 0)
		*&((struct v29_tx_root *)modem)->cfg = *params;
	else
		*&((struct v29_tx_root *)modem)->cfg = V29TX_CFG;

	((struct v29_tx_root *)modem)->result.word = 0;
	((struct v29_tx_root *)modem)->result.byte.flags |= 0x58;
	((struct v29_tx_root *)modem)->result.byte.status = 1;

	/* ---- the parameter/half-duplex block, the FIFO and the SGD ------- */

	prm = ((struct v29_tx_root *)modem)->params;
	if (prm == 0) {
		prm = sysdep_malloc(0x1c);
		((struct v29_tx_root *)modem)->params = prm;
		prm->fifo = 0;
		prm->sgd = 0;
	}

	{
		struct fifo_cfg fc;
		unsigned short n = (unsigned short)
			(&((struct v29_tx_root *)modem)->cfg)->fifo_size_factor;

		fc.word0 = FIFO_CFG.word0;
		fc.size = (short)(n * 3 * 16);
		fc.fill = FIFO_CFG.fill;

		existing = prm->fifo;
		prm->fifo =
			FIFO_create((struct fax_fifo *)existing, &fc);
	}

	{
		struct sgd_cfg gcfg = SGD_CFG;

		gcfg.sym_bits = 4;

		existing = prm->sgd;
		prm->sgd =
			SGD_create((struct sgd *)existing, &gcfg);
	}

	/* ---- the half-duplex machine's own state ------------------------- */

	prm = ((struct v29_tx_root *)modem)->params;
	prm->state = V29TX_STATE_START;
	prm->countdown = 0;
	prm->scram_sr = 0x2a;

	prm->int_0008 = 0;
	prm->handler = TxHdxStartV29;

	if ((&((struct v29_tx_root *)modem)->cfg)->bitrate == V29_BPS_7200) {
		prm->rate = V29_RATE_7200;
	} else if ((&((struct v29_tx_root *)modem)->cfg)->bitrate == V29_BPS_9600) {
		prm->rate = V29_RATE_9600;
	} else {
		prm->rate = V29_RATE_9600;
		((struct v29_tx_root *)modem)->result.byte.flags |= V29TX_RESULT_B1_BIT1;
		((struct v29_tx_root *)modem)->result.byte.status = V29TX_STATUS_DEFAULT;
	}

	/* ---- the private block: the ring, the scrambler, the symbol coder
	 * and the pulse shaper ---------------------------------------------- */

	if (((struct v29_tx_root *)modem)->tx == 0) {
		struct v29tx *tx = sysdep_malloc(0x9c);

		((struct v29_tx_root *)modem)->tx = tx;
		tx->ring.i = sysdep_malloc(0x64);
		tx->ring.q = sysdep_malloc(0x64);
	}

	((struct v29_tx_root *)modem)->tx->ring.ridx = 0;
	((struct v29_tx_root *)modem)->tx->ring.widx = 0;
	((struct v29_tx_root *)modem)->tx->ring.sym = sysdep_malloc(0x64);
	((struct v29_tx_root *)modem)->tx->ring.len = 0x32;

	{
		short i;

		for (i = 0; i <= 0x31; i++) {
			((struct v29_tx_root *)modem)->tx->ring.i[i] = 0;
			((struct v29_tx_root *)modem)->tx->ring.q[i] = 0;
		}
	}

	{
		struct fpm_sdm_cfg dcfg = SDM_CFG;

		dcfg.nbits = (short)((prm->rate != 0)
				     ? 4 : 3);
		dcfg.tap1 = 0x12;
		dcfg.tap2 = 0x17;

		SDM_init(&((struct v29_tx_root *)modem)->tx->sdm, &dcfg);
	}

	{
		struct fpm_smc_cfg scfg = SMC_CFG;

		scfg.f00 = 0;
		scfg.direct = 1;
		scfg.rot_step = 0x11;
		scfg.rot_mod = 0x18;
		scfg.qshift = 0;
		scfg.qmask = 7;
		scfg.amask = (unsigned short)
			((prm->rate != 0) ? 8 : 0);
		scfg.pmask = 7;
		scfg.pmap = V29TX_SMC_PMAP;
		scfg.imap = V29TX_SMC_IMAP;
		scfg.qmap = V29TX_SMC_QMAP;
		scfg.cosine = V29TX_SMC_COSINE;
		scfg.sine = V29TX_SMC_SINE;

		SMC_init(&((struct v29_tx_root *)modem)->tx->smc, &scfg);
	}

	{
		struct fpm_pps_cfg pcfg = FPM_PPS_CFG;

		pcfg.phases = 10;
		pcfg.step = 3;
		pcfg.mapped = 0;
		pcfg.scale = V29TX_PPS_SCALE[prm->rate];
		pcfg.coeff_i = V29TX_PPS_IFILT;
		pcfg.coeff_q = V29TX_PPS_QFILT;
		pcfg.aux = (void *)(long)(&((struct v29_tx_root *)modem)->cfg)->int_0018;

		FPM_PPS_init(&((struct v29_tx_root *)modem)->tx->pps, &pcfg, fresh);
	}

	return modem;
}

/*
 * ---------------------------------------------------------------------------
 * V29TX_delete -- .text 0x09be90, 135 bytes.
 *
 * The transmitter's nine releases.  The private block is re-read from the
 * handle before every one -- five separate `mov 0x24(%ebx),%e?x` between
 * 0x09bea1 and 0x09bed9, then three of `0x20(%ebx)` -- so `((struct v29_tx_root *)modem)->tx`
 * expands at each use here as `RX()` and `DET()` do above.
 *
 * The object places a literal 1 in the second argument slot before
 * `FPM_PPS_free` (0x09be91), which takes a single argument and reads no frame
 * slot past the first.  Not reproduced; finding F8876, exactly as for
 * `V29RX_delete`.
 *
 * The final free is a sibling `jmp` and is unconditional, and nothing on the
 * way is guarded; D1150.
 */
void
V29TX_delete(void *modem)
{
	FPM_PPS_free(&((struct v29_tx_root *)modem)->tx->pps);

	sysdep_free(((struct v29_tx_root *)modem)->tx->ring.sym);
	sysdep_free(((struct v29_tx_root *)modem)->tx->ring.q);
	sysdep_free(((struct v29_tx_root *)modem)->tx->ring.i);
	sysdep_free(((struct v29_tx_root *)modem)->tx);

	SGD_delete((struct sgd *)
			((struct v29_tx_root *)modem)->params->sgd);
	FIFO_delete((struct fax_fifo *)
			((struct v29_tx_root *)modem)->params->fifo);
	sysdep_free(((struct v29_tx_root *)modem)->params);

	sysdep_free(modem);
}

/*
 * V21tx.c -- split out of the merged v21.c so the definitions sit in the
 * translation unit the object's FILE order gives them.  Bodies moved
 * verbatim; no source text changed.  See finding F11390.
 */
#include <string.h>

#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fsd.h"
#include "dsplib/fpm_fsm.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/sysdep.h"
#include "dsplib/v21fax.h"

/*
 * V21TX_create -- .text 0x0992f0, 757 bytes.
 *
 * The three debug strings are the author's own words, from .rodata.str1.1 at
 * 0x468d, 0x469d and 0x468b: "V.21 TX Create ", "New allocation\n" and "\n".
 * Unlike `V21RX_create`'s three (which compose in sequence), these are TWO
 * ALTERNATIVES after the first: "New allocation\n" when the handle was just
 * allocated, "\n" otherwise -- 0x099590's branch chooses between them, not
 * 0x099317's.
 *
 * WHAT THE FUNCTION IS.  Allocate-or-reuse the handle, copy the 28-byte
 * configuration onto its head, allocate-or-reuse the parameter/half-duplex
 * block and build the transmit FIFO into it, install `TxHdxStartV21` at
 * `V21TX_STATE_START`, allocate-or-reuse the DSP block, and initialise the
 * modulator and the rate converter.  `fresh` -- 1 only when this call
 * allocated the handle -- is the third argument to `FPM_MRF_init` alone;
 * `FPM_FSM_init` takes no such flag.
 *
 * THE TRANSMIT FIFO'S SIZE AND FILL ARE LITERALS, 6 AND 1, NOT `FIFO_CFG`'s
 * OWN 100 AND 0.  The object loads `FIFO_CFG`'s first dword (word0, size)
 * onto the stack at 0x099382 and then immediately overwrites the size half
 * with the literal 6 at 0x099386 -- so this constructor's own correctness
 * does not depend on `FIFO_CFG`'s VALUE, only on its `word0` field being 0,
 * which is true of BOTH of the blob's same-named copies (F9058) and is why
 * this call alone cannot tell the global 100-table from the local 300-one
 * apart.  `t_fifocreate.c` is what proves `FIFO_CFG` itself, through
 * `FIFO_create`'s own NULL-config default.
 *
 * THE THREE LITERAL FREQUENCY WRITES ARE DEAD.  `V21TX_CFG.protocol`
 * selects one of three short blocks (1180/980, 0/0, or 1850/1650 depending on
 * whether it is 0, anything else, or 1) that each store a freq pair onto the
 * stack -- and every one of the three is unconditionally overwritten by
 * `FPM_FSM_CFG`'s OWN freq pair four to nine instructions later
 * (0x099409), before `FPM_FSM_init` ever sees the local.  Reproduced as the
 * observable net effect (`fsm = FPM_FSM_CFG; fsm.scale = 0x1900;`) rather
 * than as dead stores; D1241.  The ONLY observable difference between the
 * three branches is that "neither 0 nor 1" also raises
 * `V21TX_RESULT_B1_BIT1` and reports `V21TX_STATUS_DEFAULT` before falling
 * into the shared tail -- reproduced below.
 *
 * `MRF.AUX` IS NEVER WRITTEN, unlike the receiver's.  The object loads only
 * TWO of `FPM_MRF_CFG`'s four dwords (offset 0 and offset 8) before patching
 * `branches`/`decimate`/`coeff`/`taps`; `coeff` and `aux` are never read out
 * of the library default at all, and `aux` is never written by anything
 * else either -- so the object's own local carries whatever was on the stack
 * before this call.  Spelled here as `mrf = FPM_MRF_CFG;` -- which DOES give
 * `aux` a defined value, the library default's own (typically NULL) -- and
 * not as an intentionally uninitialised local, because C makes reading an
 * uninitialised struct member undefined and nothing here needs to court
 * that; the field is untestable either way, since nothing reconstructed
 * reads `dsp->mrf.cfg.aux` back out, and `t_v21txcreate.c` excludes it from
 * comparison exactly as `t_v21create.c` excludes the receiver's.  D1242.
 */
void *
V21TX_create(void *modem, const struct v21tx_cfg *params)
{
	struct v21_tx *tx;
	struct fpm_fsm_cfg fsm;
	struct fpm_mrf_cfg mrf;
	struct fifo_cfg fc;
	struct v21_tx_dsp *dsp;
	struct v21_tx_hdx *hdx;
	struct fax_fifo *existing_fifo;
	short protocol;
	int fresh = 0;
	int zero = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V.21 TX Create ");

	if (modem == NULL) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("New allocation\n");
		modem = sysdep_malloc(V21TX_OBJ_SIZE);
		tx = (struct v21_tx *)modem;
		tx->hdx = NULL;
		fresh = 1;
		tx->dsp = NULL;
	} else {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\n");
	}
	tx = (struct v21_tx *)modem;

	/* The configuration IS the handle's first twenty-eight bytes. */
	if (params != NULL)
		tx->cfg = *params;
	else
		tx->cfg = V21TX_CFG;

	memcpy(&tx->result, &zero, sizeof zero);
	tx->result.byte.flags1 |= 0x58;

	hdx = tx->hdx;
	if (hdx == NULL) {
		hdx = sysdep_malloc(sizeof(struct v21_tx_hdx));
		tx->hdx = hdx;
		hdx->fifo = NULL;
	}

	/*
	 * The local FIFO configuration: `word0` from the real `FIFO_CFG`
	 * (0 either of the blob's two copies), `size` and `fill` the
	 * literals 6 and 1 -- see the function comment.
	 */
	fc.word0 = FIFO_CFG.word0;
	fc.size = 6;
	fc.fill = 1;
	existing_fifo = hdx->fifo;
	hdx->fifo =
		FIFO_create(existing_fifo, &fc);

	hdx->int_0004 = 0;
	hdx->handler = TxHdxStartV21;
	hdx->state = V21TX_STATE_START;
	hdx->short_000e = 0;

	dsp = tx->dsp;
	if (dsp == NULL) {
		dsp = (struct v21_tx_dsp *)
			sysdep_malloc(sizeof(struct v21_tx_dsp));
		sysdep_memset(dsp, 0, sizeof(struct v21_tx_dsp));
		tx->dsp = dsp;
		dsp->scratch = sysdep_malloc(V21TX_SCRATCH_BYTES);
		sysdep_memset(dsp->scratch, 0, V21TX_SCRATCH_BYTES);
	}

	/*
	 * `protocol` selects one of three (past the dead frequency writes,
	 * identical) paths; only the third has an observable side effect.
	 */
	memcpy(&protocol, modem, sizeof protocol);
	if (protocol == 0 || protocol == 1) {
		/* No observable effect; see the function comment and D1241. */
	} else {
		tx->result.byte.flags1 |= V21TX_RESULT_B1_BIT1;
		tx->result.byte.status = V21TX_STATUS_DEFAULT;
	}

	fsm = FPM_FSM_CFG;
	fsm.scale = 0x1900;
	FPM_FSM_init(&tx->dsp->fsm, &fsm);

	mrf = FPM_MRF_CFG;
	mrf.branches = 10;
	mrf.decimate = 9;
	mrf.coeff = V21_MRF_FILT;
	mrf.taps = 360;
	FPM_MRF_init(&tx->dsp->mrf, &mrf, fresh);

	return modem;
}

/*
 * V21TX_delete -- .text 0x0995f0, 104 bytes.
 *
 * The transmitter's seven releases.  The DSP block is re-read from the handle
 * before every one rather than cached, because the object re-reads it: four
 * separate `mov 0x24(%ebx),%e?x` between 0x0995f8 and 0x099628, and two more
 * of `0x20(%ebx)` after them.
 *
 * `FPM_FSM_delete`, `FPM_MRF_free` and the `sysdep_free` of +0x2c type the
 * three members of `struct v21_tx_dsp` a second time -- `ModDataV21` typed
 * them by what each is handed TO, this function by what releases each -- so
 * the block's layout has two independent statements behind it.  See the
 * header and finding F9252.
 *
 * THE LITERAL 1 IN THE SECOND ARGUMENT SLOT IS NOT REPRODUCED (0x099603,
 * before `FPM_MRF_free`).  Finding F8876; `V21RX_delete` above carries the
 * same note for the same reason.
 *
 * No NULL guard anywhere, and the handle goes unconditionally; D1150.
 */
void
V21TX_delete(void *modem)
{
	struct v21_tx *tx = (struct v21_tx *)modem;

	FPM_FSM_delete(&tx->dsp->fsm);
	FPM_MRF_free(&tx->dsp->mrf);
	sysdep_free(tx->dsp->scratch);
	sysdep_free(tx->dsp);

	FIFO_delete(tx->hdx->fifo);
	sysdep_free(tx->hdx);

	sysdep_free(modem);
}

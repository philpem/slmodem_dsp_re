/*
 * v21.c -- ITU-T V.21 (the fax control channel): the receive and transmit
 * primitives.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V21RX_create      .text 0x098e70  1011
 *   V21RX_delete      .text 0x099270   123
 *   V21TX_create      .text 0x0992f0   757
 *   V21TX_delete      .text 0x0995f0   104
 *   V21RX_modem       .text 0x0a1c40   127
 *   RxHdxErrorV21     .text 0x0a1cc0    59
 *   RxHdxIdleV21      .text 0x0a1d00    81
 *   RxNextStateV21    .text 0x0a1d60   290
 *   RxHdxStartV21     .text 0x0a1e90   522
 *   RxHdxWaitV21      .text 0x0a20a0   440
 *   RxHdxDataV21      .text 0x0a2260   418
 *   V21RX_control     .text 0x0a2410    75
 *   V21RX_status      .text 0x0a2460   132
 *   V21TX_modem       .text 0x0a24f0   182
 *   TxNextStateV21    .text 0x0a25b0   280
 *   TxHdxStartV21     .text 0x0a26d0   380
 *   TxHdxIdleV21      .text 0x0a2850   372
 *   TxHdxDataV21      .text 0x0a29d0   451
 *   V21TX_control     .text 0x0a2ba0    94
 *   V21TX_status      .text 0x0a2c00    96
 *   DemodDataV21      .text 0x0a5740   217
 *   CarrierDetectV21  .text 0x0a5820    16
 *   GetSNRV21         .text 0x0a5830    77
 *   ModDataV21        .text 0x0a5880    87
 *   TxNoCarrierV21    .text 0x0a58e0   103
 *
 * THESE ARE TWENTY-FIVE SYMBOLS OF THREE DIFFERENT CLUSTERS, not one author
 * file: 0x098e70..0x0995f0 sit with the constructors and destructors,
 * 0x0a1c40..0x0a2c00 with the half-duplex machines (receive and, since
 * F9500, transmit), and 0x0a5740 onward with the per-modulation data paths.
 * They are collected here because they are the V.21 work that is startable,
 * and `include/dsplib/v21fax.h` says what each one establishes.  The order
 * below is the object's own address order.
 *
 * THE TRANSMIT HALF-DUPLEX MACHINE IS A SECOND INDIVISIBLE UNIT, F9500's,
 * on the same F8492/F8493 ground the receive one already stood on: each of
 * `TxHdxStartV21`, `TxHdxIdleV21` and `TxHdxDataV21` installs at least one of
 * the other two as a STORED FUNCTION POINTER (a data reference, no `call`),
 * and `TxNextStateV21` -- the canonical, out-of-line copy of the switch all
 * three inline -- installs all three itself.  No proper subset links.
 *
 * THE FIVE RECEIVE-PATH SYMBOLS ARE ONE INDIVISIBLE UNIT and had to be
 * written together.  `DemodDataV21` carries an `R_386_32` against
 * `RxHdxDataV21` on the `cmpl` at 0x0a57a3 -- a DATA reference, not a call --
 * and `RxHdxErrorV21`, `RxHdxIdleV21`, `RxHdxWaitV21` and `RxHdxDataV21` all
 * call `DemodDataV21`.  Under findings F8492/F8493 a reference from `src/` to
 * a symbol this tree has not written is an undefined reference that fails
 * every test binary, so no proper subset of the five links.
 *
 * The functions are laid out in the object's address order, which is the one
 * lever this tree has on register allocation across a translation unit
 * (finding F7796) -- it is not a claim that the author had them in one file.
 *
 * Everything here takes a `void *` handle.  That was originally because
 * NEITHER constructor was reconstructed, so naming their fields would have
 * been guessing; both `V21RX_create` and `V21TX_create` are written now, and
 * each handle is STILL a `void *` because each constructor decided its own
 * layout without settling what most of it MEANS.  `V21RX_create` fixes the
 * receive handle's size at 0x54 and the width of every field, and `v21fax.h`
 * records both -- but +0x1c through +0x4b are written by that one function
 * and read by nothing else in the object, so they keep `type_NNNN` names and
 * a set of offset constants rather than becoming a struct whose members would
 * each be a claim.  `V21TX_create` does the same for the transmit handle at
 * 0x28 bytes: +0x00..+0x1b (`struct v21tx_cfg`, v21cfg.h) is likewise
 * write-only.  See the header for the ruling and for where every offset used
 * below comes from.
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
 * The TRANSMITTER handle is not modelled -- `V21TX_create` is not
 * reconstructed -- so the two transmit entry points below reach it through
 * offsets, exactly as `v17.c` and `v29.c` reach theirs.  The receive side
 * keeps its typed accessors from `v21fax.h`; the two halves are different
 * objects (see the header) and are spelled differently on purpose.
 */
/*
 * V21RX_create -- .text 0x098e70, 1,011 bytes.
 *
 * IT IS FIRST IN THE FILE BECAUSE IT IS FIRST IN THE OBJECT.  0x098e70 is
 * below `V21RX_delete`'s 0x099270, and a translation unit's emission order is
 * what register allocation follows (finding F7796), so the order here is the
 * blob's rather than the one the file grew in.
 *
 * The three debug strings are the author's own words, from .rodata.str1.1 at
 * 0x461b, 0x462d and 0x462b: "V.21 RX Create ", "New allocation\n" and "\n".
 * The first has a trailing space and no newline, so the three compose into one
 * line either way -- "V.21 RX Create New allocation" when the handle is
 * allocated here and "V.21 RX Create" when the caller supplied one.  That is
 * what fixes the order of the three tests.
 *
 * WHAT THE FUNCTION IS.  Four allocations, a configuration copy, four DSP
 * blocks initialised from the library built-ins with V.21's tables patched in,
 * and a tail of field initialisation.  `fresh` -- 1 only when this call
 * allocated the handle -- is the third argument to all three `*_init` calls,
 * which is how the blocks learn whether to allocate their own buffers or
 * re-initialise in place.  The object keeps it in `%ebp`, zeroed at entry by
 * `xor %ebp,%ebp` before anything else happens.
 *
 * THE HANDLE IS RE-READ RATHER THAN CACHED, exactly as in `V21RX_delete`
 * below and for the same reason: the object re-loads `0x50(%esi)` at 0x098f30,
 * 0x098f64, 0x098f72, 0x098ff2, 0x099056, 0x09905d and 0x099089 rather than
 * keeping the DSP pointer in a register across the calls.
 *
 * THE CONFIGURATION COPY IS WRITTEN AS TWO ARMS, not as a pointer fixup
 * followed by one copy.  The object carries the six-dword copy TWICE, at
 * 0x098ea8 from the caller's table and at 0x099190 from `V21RX_CFG`, and two
 * arms is the spelling that says so.  `B103FP_create` uses the other form
 * (`cfg = &B103_CFG_data;` then one assignment) and both are behaviourally
 * identical; which one the period compiler turns into the object's two copies
 * was NOT measured here, because this worktree has no period compiler.  If a
 * later pass measures it, this is the site.
 */
void *
V21RX_create(void *modem, const struct v21rx_cfg *params)
{
	struct v21_rx *rx;
	struct fpm_mrf_cfg mrf;
	struct fpm_fsd_cfg fsd;
	struct fpm_mtd_cfg mtd;
	struct v21_rx_hdx *hdx;
	struct v21_rx_dsp *dsp;
	const struct v21rx_cfg *cfg;
	unsigned long aux;
	int fresh = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V.21 RX Create ");

	if (modem == NULL) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("New allocation\n");
		modem = sysdep_malloc(V21RX_OBJ_SIZE);
		rx = (struct v21_rx *)modem;
		rx->hdx = NULL;
		fresh = 1;
		rx->dsp = NULL;
	}
	rx = (struct v21_rx *)modem;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("\n");

	/* The configuration IS the handle's first twenty-four bytes. */
	if (params != NULL)
		rx->cfg = *params;
	else
		rx->cfg = V21RX_CFG;
	cfg = &rx->cfg;

	/*
	 * `cfg->aux` reaches TWO configurations from here -- the resampler's
	 * `aux` and the FSK demodulator's trailing slot -- and the object
	 * loads it once, into `%edi` at 0x098ede, before either.
	 */
	aux = (unsigned long)cfg->aux;

	if (rx->hdx == NULL)
		rx->hdx = sysdep_malloc(sizeof(struct v21_rx_hdx));
	hdx = rx->hdx;

	hdx->int_0000 = 0;
	hdx->handler = RxHdxStartV21;
	hdx->state = V21RX_STATE_START;
	hdx->countdown = 0;
	hdx->ones_run = 0;
	hdx->mark_seq = 0;

	/*
	 * The DSP block and the shared intermediate buffer.  0x140 bytes is
	 * 160 shorts, which is `FPM_FSD_CFG.trace_len` -- the demodulator
	 * writes one trace word per input sample and `GetSNRV21` rectifies
	 * that trace into `mag`, so the two buffers are the same length and
	 * the count is written down twice in the object.
	 */
	if (rx->dsp == NULL) {
		rx->dsp =
			sysdep_malloc(sizeof(struct v21_rx_dsp));
		sysdep_memset(rx->dsp, 0,
			      sizeof(struct v21_rx_dsp));
		rx->dsp->mag = sysdep_malloc(V21RX_MAG_BYTES);
		sysdep_memset(rx->dsp->mag, 0, V21RX_MAG_BYTES);
		rx->dsp->mtd = NULL;
	}

	/*
	 * The receive rate converter: 8 kHz in, 7200 Hz out.  `branches` and
	 * `decimate` are written even though they already equal the built-in's
	 * -- the object stores both explicitly at 0x098f3c and 0x098f49.
	 */
	mrf = FPM_MRF_CFG;
	mrf.branches = 9;
	mrf.decimate = 10;
	mrf.coeff = V21_MRF_FILT;
	mrf.taps = 360;
	mrf.aux = (void *)aux;
	FPM_MRF_init(&rx->dsp->mrf, &mrf, fresh);

	FPM_AGC_init(&rx->dsp->agc, &AGCv21_CFG, fresh);

	dsp = rx->dsp;
	dsp->int_0000 = 1;
	dsp->int_0004 = 0;
	dsp->int_0008 = 0;

	/*
	 * The FSK demodulator.  `high_bit` is patched to ZERO, which inverts
	 * the demodulated data against the library default's 1 -- V.21's mark
	 * is the LOWER of each channel's pair, so the discriminator's sign
	 * runs the other way round from Bell 103's.
	 *
	 * `bit_samples` is 24 rather than the built-in's 8, which is 7200 Hz
	 * divided by 300 bit/s: one bit is twenty-four samples at the rate the
	 * resampler above delivers.  That is the arithmetic tying this call to
	 * the one before it.
	 */
	fsd = FPM_FSD_CFG;
	if (cfg->chan2) {
		fsd.delay = 3;
		fsd.fir = V21RX_CHAN2_INTRP;
	} else {
		fsd.delay = 5;
		fsd.fir = V21RX_CHAN1_INTRP;
	}
	fsd.fir_taps = 15;
	fsd.iir = V21RX_IIR_LPF;
	fsd.iir_len = 3;
	fsd.high_bit = 0;
	fsd.bit_samples = 24;
	/*
	 * The object stores `aux` over `f18` AND `pad1a` with ONE 32-bit `mov`
	 * at 0x098f75, which is what a `void *` member there would take and
	 * not what two `short` stores would -- so the original almost
	 * certainly had one four-byte slot here, the same trailing `aux` that
	 * `struct fpm_mrf_cfg` carries and that receives the same value nine
	 * instructions later.
	 *
	 * `fpm_fsd.h` cannot be retyped from this pass: `src/pump/v23/v23rx.c`
	 * names `.f18` in a designated initialiser and that file is outside
	 * this pass's scope.  Splitting the pointer by hand reproduces the
	 * stored BYTES exactly for every input on the 32-bit build this
	 * reconstruction targets, so nothing observable is given up -- only
	 * the shape of the two instructions.  Recorded as D1181.
	 */
	fsd.f18 = (short)(unsigned short)aux;
	fsd.pad1a = (short)(unsigned short)(aux >> 16);
	FPM_FSD_init(&rx->dsp->fsd, &fsd, fresh);

	/*
	 * The tone detector, listening for the mark and space of whichever
	 * channel `chan2` selected.  0x4ccd is 0.6 in Q15 and 300 is V.21's
	 * own bit rate reused as a level floor.
	 */
	mtd = FPM_MTD_CFG;
	mtd.coeff = cfg->chan2 ? V21_CHAN2_MTD_COEFF : V21_CHAN1_MTD_COEFF;
	mtd.tones = 2;
	mtd.ratio = 0x4ccd;
	mtd.min_level = 300;
	rx->dsp->mtd = FPM_MTD_create(rx->dsp->mtd, &mtd);

	/*
	 * The status word is zeroed as one 32-bit unit and then two of its
	 * bytes are written back -- `movl $0x0,0x18(%esi)`, `orb $0x50`,
	 * `movb $0x1`.  It is spelled with `memcpy` here for the reason
	 * `V21RX_modem` reads it with one: +0x18 leaves the library as a
	 * single word, so the four bytes are one object rather than four.
	 */
	{
		int zero = 0;

		memcpy(&rx->status, &zero, sizeof zero);
	}
	rx->status.byte.flags |= (unsigned char)(V21RX_FLAG_BIT4
					      | V21RX_FLAG_BIT6);
	rx->status.byte.status = V21RX_STATUS_START;

	/*
	 * The trace export and the three unmodelled groups.  See `v21fax.h`
	 * for what is known about each and what is not; the DSP pointer is
	 * re-read here because the object re-reads it at 0x099089.
	 */
	dsp = rx->dsp;
	rx->ptr_001c = dsp->fsd.trace;
	rx->int_0020 = 0;
	rx->ptr_0024 = &dsp->fsd.last_count;
	rx->int_0028 = 0;
	rx->int_002c = 0;
	rx->short_0030 = 0;
	rx->int_0034 = 0;
	rx->int_0038 = 0;
	rx->short_003c = 0;
	rx->int_0040 = 0;
	rx->int_0044 = 0;
	rx->short_0048 = 0;

	return modem;
}

/*
 * The handle is re-read from the caller's argument before every free rather
 * than cached in a local, because the object re-reads it: five separate
 * `mov 0x50(%ebx),%eax` between 0x099278 and 0x0992c8.
 *
 * THE LITERAL 1 IN THE SECOND ARGUMENT SLOT IS NOT REPRODUCED.  The object
 * puts one there before `FPM_FSD_free` and `FPM_MRF_free`, both of which take
 * a single argument and never load a second -- so it is dead stack setup,
 * presumably from a version where they took a `fresh` flag like their init
 * counterparts.  `B103FP_delete` has exactly the same three call sites and
 * the same note; there is nothing to reproduce, because an argument the
 * callee never loads has no observable effect.
 *
 * There is no NULL guard on anything, and the last free releases the handle
 * unconditionally even when the caller supplied it.  Both reproduced; see
 * docs/deviations.md D1039.
 */
void
V21RX_delete(void *modem)
{
	struct v21_rx *rx = (struct v21_rx *)modem;

	FPM_MTD_delete(rx->dsp->mtd);
	FPM_FSD_free(&rx->dsp->fsd);
	FPM_MRF_free(&rx->dsp->mrf);

	sysdep_free(rx->dsp->mag);
	sysdep_free(rx->dsp);
	sysdep_free(rx->hdx);
	sysdep_free(modem);
}

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

/*
 * One block of receive.
 *
 * `remaining` is the count the handler is still working through, and it is
 * held UNSIGNED while `before` is a `short`: the object sign-extends the
 * previous count into the subtraction (`movswl %cx,%ebx` at 0x0a1c70) and
 * zero-extends the new one (`movzwl %cx,%edx` at 0x0a1c91).  The second
 * extension feeds a 32-bit subtract, so it is FORCED and not the free kind
 * -- CLAUDE.md's rule for reading a codegen difference, applied the way round
 * it is meant to be.
 *
 * The loop is a do-while: `*count` of zero on entry still dispatches once.
 *
 * The return is the 32-bit word that starts at the status byte, taken with
 * `memcpy` rather than a cast for the reason `B103FP_modem` takes its
 * identically shaped one that way.
 */
int
V21RX_modem(void *modem, short *in, short *out, short *count)
{
	struct v21_rx *rx = (struct v21_rx *)modem;
	unsigned short remaining;
	short total = 0;
	int word;

	rx->status.byte.flags &= (unsigned char)~V21RX_FLAG_ERROR;

	remaining = (unsigned short)*count;
	do {
		short before = (short)remaining;
		short n;

		n = rx->hdx->handler(modem, in, out, count);
		remaining = (unsigned short)*count;

		out += n;
		in += before - remaining;
		total = (short)(total + n);
	} while (remaining != 0);

	*count = total;

	memcpy(&word, &rx->status, sizeof word);
	return word;
}

/*
 * The error state: raise the flag, run the block through the demodulator
 * anyway so the filters keep their history, and consume it.
 *
 * Nothing here advances the state, so once `RxHdxWaitV21` has installed this
 * handler the machine stays in it until something outside re-installs
 * another one.  The flag is a one-shot: `V21RX_modem` clears it at the top of
 * every block, so a caller that does not read the returned word each block
 * loses the event.
 */
short
RxHdxErrorV21(void *modem, short *in, short *out, short *count)
{
	struct v21_rx *rx = (struct v21_rx *)modem;
	rx->status.byte.flags |= V21RX_FLAG_ERROR;

	DemodDataV21(modem, in, out, (unsigned short)*count);
	*count = 0;

	return 0;
}

/*
 * The idle state: demodulate, report, and re-read the carrier.
 *
 * The carrier flag is cleared BEFORE `CarrierDetectV21` is called and set
 * again only if it answers, rather than being assigned from the answer --
 * `andb $0xdf` at 0x0a1d31 and `orb $0x20` at 0x0a1d45 with the call between
 * them.  The two spellings agree on the value and not on the instructions,
 * and this is the object's.
 */
short
RxHdxIdleV21(void *modem, short *in, short *out, short *count)
{
	struct v21_rx *rx = (struct v21_rx *)modem;
	DemodDataV21(modem, in, out, (unsigned short)*count);
	*count = 0;

	rx->status.byte.flags &= (unsigned char)~V21RX_FLAG_CARRIER;
	rx->status.byte.status = V21RX_STATUS_IDLE;

	if (CarrierDetectV21(modem))
		rx->status.byte.flags |= V21RX_FLAG_CARRIER;

	return 0;
}

/*
 * Advance the receive state machine one step.
 *
 * THIS IS `RxNextStateV21` (0x0a1d60, 290 bytes), AND IT IS NOW CLAIMED.
 * The object carries the block four times: once out of line under that name,
 * and three more times inlined into `RxHdxStartV21`, `RxHdxWaitV21` and
 * `RxHdxDataV21`.  All four copies are the same instructions in the same
 * order, which is what says the author wrote one function and the compiler
 * inlined it at `-O3`.  Finding F8898 left it `static v21rx_next_state`
 * because its third caller, `RxHdxStartV21`, was not reconstructed and a
 * `static` carrying a blob symbol's name would have been counted as written
 * by every tool that globs `build/repro`.  That caller is below, so the
 * symbol is global and named here; finding F9091.
 *
 * The four strings are the author's own words, out of .rodata.str1.1 at
 * 0x4b3a, 0x4b16, 0x4b28 and 0x4b03; `tools/relocscan.py` is what pairs them
 * with these sites, because the reference is an R_386_32 against the section
 * symbol with the offset as an inline addend (finding F604).
 *
 * The default arm is reached from state IDLE and state ERROR alike, and it
 * does not install a handler: it only resets the flags and reports
 * V21RX_STATUS_DEFAULT.
 */
void
RxNextStateV21(void *modem)
{
	struct v21_rx *rx = (struct v21_rx *)modem;
	struct v21_rx_hdx *hdx = rx->hdx;

	switch (hdx->state) {
	case V21RX_STATE_START:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21RX_STATE_START\n");
		hdx->countdown = 1;
		hdx->handler = RxHdxWaitV21;
		hdx->state = V21RX_STATE_WAIT;
		break;

	case V21RX_STATE_WAIT:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21RX_STATE_WAIT\n");
		hdx->countdown = 0;
		hdx->handler = RxHdxDataV21;
		hdx->state = V21RX_STATE_DATA;
		rx->status.byte.flags |= V21RX_FLAG_DATA;
		break;

	case V21RX_STATE_DATA:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21RX_STATE_DATA\n");
		hdx->handler = RxHdxIdleV21;
		hdx->state = V21RX_STATE_IDLE;
		hdx->countdown = 0;
		hdx->int_0000 = 0;
		rx->status.byte.flags1 |= V21RX_FLAG1_IDLE;
		rx->status.byte.flags &= (unsigned char)~V21RX_FLAG_DATA;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21RX_DEFAULT, %d\n", hdx->state);
		rx->status.byte.flags1 &= (unsigned char)~V21RX_FLAG1_IDLE;
		rx->status.byte.status = V21RX_STATUS_DEFAULT;
		rx->status.byte.flags = (unsigned char)
			((rx->status.byte.flags | V21RX_FLAG_ERROR)
			 & ~(V21RX_FLAG_DATA | V21RX_FLAG_CARRIER));
		break;
	}
}

/*
 * The start state, which is the one `V21RX_create` installs (0x098ee1).
 *
 * It demodulates the block like every other handler and then WALKS THE
 * DEMODULATED UNITS, which none of the other four does.  The walk keeps
 * `ones_run` as the length of the current run of non-zero units and bumps
 * `mark_seq` on each transition out of a run of exactly six; the state
 * advances once `mark_seq` has passed four and the carrier is up.
 *
 * THE TWO COUNTERS ARE UPDATED IN THE ORDER THE OBJECT UPDATES THEM, and the
 * ordering is observable: at `ones_run == 6` with a zero unit the object
 * increments `mark_seq` (0x0a1f9c) and then falls into the common store that
 * puts zero in `ones_run` (0x0a1efc), while at `ones_run == 6` with a
 * non-zero unit it jumps straight to the increment at 0x0a1ef7 and leaves
 * `mark_seq` alone.  The `? :` below is the same three-way outcome written
 * once; the object's two entries into the common store are the compiler's
 * tail-merge of it.
 *
 * BOTH LOOP VARIABLES ARE 16-BIT AND THAT IS FORCED.  `nbits` is decremented
 * with `lea -0x1(%esi),%eax` / `movzwl %ax,%esi` and `i` incremented with
 * `lea 0x1(%edi),%ebx` / `movzwl %bx,%edi` (0x0a1f00..0x0a1f0b), so a count of
 * 0x10000 would be a count of zero and the walk is skipped entirely; the
 * demodulator cannot return one, but the reproduction does not depend on
 * that.  The `> 4` test is `cmpw $0x4` with `jle`, so it is SIGNED over
 * sixteen bits -- the same shape `RxHdxWaitV21`'s countdown has and the same
 * cast is used for it.
 *
 * The return is a literal zero on every path (0x0a1f83), so the bits this
 * handler produced are reported to `V21RX_modem` as none.  That is the
 * object's; see docs/deviations.md D1090.
 */
short
RxHdxStartV21(void *modem, short *in, short *out, short *count)
{
	struct v21_rx *rx = (struct v21_rx *)modem;
	struct v21_rx_hdx *hdx;
	unsigned short nbits;
	unsigned short i;

	rx->status.byte.status = V21RX_STATUS_START;

	nbits = DemodDataV21(modem, in, out, (unsigned short)*count);
	*count = 0;

	hdx = rx->hdx;

	i = 0;
	while (nbits != 0) {
		if (hdx->ones_run == V21RX_MARK_RUN && out[i] == 0)
			hdx->mark_seq = (unsigned short)(hdx->mark_seq + 1);

		hdx->ones_run = (unsigned short)
			(out[i] != 0 ? hdx->ones_run + 1 : 0);

		nbits = (unsigned short)(nbits - 1);
		i = (unsigned short)(i + 1);
	}

	rx->status.byte.flags &= (unsigned char)~V21RX_FLAG_CARRIER;

	if (!CarrierDetectV21(modem))
		return 0;

	hdx = rx->hdx;
	if ((short)hdx->mark_seq <= V21RX_MARK_SEQ_THRESHOLD)
		return 0;

	rx->status.byte.flags |= V21RX_FLAG_CARRIER;
	RxNextStateV21(modem);

	return 0;
}

/*
 * The wait state: hold for `hdx->countdown` blocks with the carrier up, then
 * advance.
 *
 * Losing the carrier is fatal here and not merely reported: the error handler
 * is installed, the state goes to V21RX_STATE_ERROR and the return is zero
 * rather than the bit count, so the bits this block did produce are thrown
 * away.  That arm is the object's and is reproduced.
 *
 * The countdown is loaded `movzwl` and tested `jle` on the low sixteen bits
 * (0x0a20f5..0x0a2101), which is why the field is `unsigned short` and the
 * test narrows: the two readings agree over every value the field can hold,
 * and the load is what the object encodes.
 */
short
RxHdxWaitV21(void *modem, short *in, short *out, short *count)
{
	struct v21_rx *rx = (struct v21_rx *)modem;
	unsigned short nbits;

	nbits = DemodDataV21(modem, in, out, (unsigned short)*count);
	*count = 0;

	if (!CarrierDetectV21(modem)) {
		rx->hdx->handler = RxHdxErrorV21;
		rx->hdx->state = V21RX_STATE_ERROR;
		rx->status.byte.status = V21RX_STATUS_ERROR;
		rx->status.byte.flags = (unsigned char)
			((rx->status.byte.flags | V21RX_FLAG_ERROR)
			 & ~V21RX_FLAG_CARRIER);
		return 0;
	}

	rx->status.byte.flags |= V21RX_FLAG_CARRIER;
	rx->status.byte.status = V21RX_STATUS_WAIT;

	rx->hdx->countdown = (unsigned short)(rx->hdx->countdown - 1);
	if ((short)rx->hdx->countdown > 0)
		return 0;

	rx->status.byte.status = V21RX_STATUS_TIMEOUT;
	RxNextStateV21(modem);

	return (short)nbits;
}

/*
 * The data state: demodulate while the carrier is up, and grade the result.
 *
 * The carrier flag is raised UNCONDITIONALLY on entry and lowered again on
 * the arm where `CarrierDetectV21` says it has gone, which is not the same
 * as assigning it -- that is the object's order (0x0a2277 before the call,
 * 0x0a22eb after it) and it is what a caller reading the flag from a handler
 * that ran earlier in the same block would see.
 *
 * `hdx->int_0000` is the second gate and nothing reconstructed sets it, so it
 * is exercised in the test by planting it rather than by reaching it.
 *
 * `GetSNRV21` is compared 16 bits wide (`cmpw $0x5,%ax`), so the narrowing
 * below is the object's and not a convenience.  As reconstructed that
 * function returns a literal zero, so the flag is raised on every block the
 * data state demodulates; the comparison is reproduced because it is there.
 */
short
RxHdxDataV21(void *modem, short *in, short *out, short *count)
{
	struct v21_rx *rx = (struct v21_rx *)modem;
	unsigned short nbits;

	rx->status.byte.flags |= V21RX_FLAG_CARRIER;
	rx->status.byte.status = V21RX_STATUS_DATA;

	if (CarrierDetectV21(modem) && rx->hdx->int_0000 == 0) {
		nbits = DemodDataV21(modem, in, out, (unsigned short)*count);
		*count = 0;

		rx->status.byte.flags &= (unsigned char)~V21RX_FLAG_LOW_SNR;
		if ((short)GetSNRV21(modem) <= V21RX_SNR_THRESHOLD)
			rx->status.byte.flags |= V21RX_FLAG_LOW_SNR;

		return (short)nbits;
	}

	rx->status.byte.flags &= (unsigned char)~V21RX_FLAG_CARRIER;
	RxNextStateV21(modem);

	return 0;
}

/*
 * V21RX_control -- .text 0x0a2410, 75 bytes.
 *
 * `arg == NULL` returns 0 and touches nothing.  Otherwise: `cfg->int_0008`
 * (the receive handle's own head, `struct v21rx_cfg`) is set unconditionally
 * from `arg->int_0004`; `V21RX_HDX(modem)->int_0000` -- the SAME field
 * `RxHdxDataV21` gates demodulation on, above, and whose header comment used
 * to read "nothing written sets it" -- is set to 1 when
 * `V21RXCTL_SET_HDX_INT0000` is set in `arg->flags` and to 0 otherwise;
 * and `V21RXCTL_REINIT` (also in `flags`) calls `V21RX_create(modem,
 * modem)`, the same self-referential reinit `v17fax.h` documents for
 * `V17RX_control` (finding F9470/F9900: the receive handle's head, byte for
 * byte, IS a `struct v21rx_cfg`, so passing it as its own `params` re-copies
 * its current configuration onto itself and reruns construction).
 *
 * NOTHING IN THE OBJECT CALLS THIS DIRECTLY: like `V17RX_control`, its only
 * referrer is the lowercase adapter `v21rx_control` (faxadapt.c), which
 * forwards `arg` unchanged from its own caller -- so `struct v21rx_ctl`
 * reads what the two loads force and no further, `v22ctl.h`'s discipline.
 */
int
V21RX_control(void *modem, const struct v21rx_ctl *arg)
{
	struct v21_rx *rx = (struct v21_rx *)modem;
	struct v21rx_cfg *cfg = &rx->cfg;

	if (arg == NULL)
		return 0;

	cfg->int_0008 = arg->int_0004;

	rx->hdx->int_0000 =
		(arg->flags & V21RXCTL_SET_HDX_INT0000) != 0;

	if (arg->flags & V21RXCTL_REINIT)
		V21RX_create(modem, (const struct v21rx_cfg *)modem);

	return 1;
}

/*
 * Fill the caller's status block from the RECEIVER.
 *
 * NOT a mirror of `V21TX_status`.  The rate goes in `rx_bps` and not
 * `tx_bps`, `snr` carries what `GetSNRV21` answered rather than a literal
 * zero, `quality` is the COMPLEMENT of V21RX_FLAG_LOW_SNR (`testb $0x80`
 * followed by `sete`, 0x0a2487), +0x0e is zeroed where the transmit side
 * zeroes +0x0c, and the flags byte is stored as a literal 0 rather than
 * merged from the handle.
 *
 * `short_12` IS THE ONLY ARITHMETIC IN THE FUNCTION and it is a `cltd`/`idiv`
 * over two SIGNED shorts loaded `movswl` (0x0a24b3 and 0x0a24b7), then
 * `imul $0x12c`.  Both operands live in the fsd: +0x76 of the DSP block is
 * `fsd.f22` and +0x66 is `fsd.cfg.bit_samples`, and `FPM_FSD_init` sets the
 * first to half the second -- so for an even `bit_samples` the quotient is 1
 * and the field comes out at 300, which is `rx_bps` again by a different
 * route.  Finding F9132.
 *
 * THE DIVIDE HAS NO GUARD.  A receiver whose fsd was never configured has
 * `bit_samples` zero and this faults; see docs/deviations.md D1098.  It is
 * reproduced, and the test asserts the precondition on BOTH sides rather
 * than driving it.
 */
int
V21RX_status(void *modem, struct v21_status *st)
{
	struct v21_rx *rx = (struct v21_rx *)modem;
	short f22, bit_samples;

	if (st == NULL)
		return 0;

	st->protocol = (short)(unsigned short)rx->cfg.chan2;
	st->tx_bps = 0;
	st->rx_bps = V21_STATUS_BPS;
	st->quality = (short)
		((rx->status.byte.flags & V21RX_FLAG_LOW_SNR) == 0);
	st->snr = (short)GetSNRV21(modem);
	st->short_0a = 0;
	st->short_0e = 0;
	st->short_10 = 0;
	st->flags1 &= (unsigned char)~V21_STATUS1_BIT0;
	st->flags = 0;

	f22 = rx->dsp->fsd.f22;
	bit_samples = rx->dsp->fsd.cfg.bit_samples;
	st->short_12 = (short)((2 - 2 * (int)f22 / (int)bit_samples)
			       * V21_STATUS_BPS);

	return 1;
}

/*
 * V21TX_modem -- .text 0x0a24f0, 182 bytes.  See v21fax.h for the two arms,
 * the budget and why `in` does not advance while `out` does.
 *
 * IT IS `V17TX_modem` FOR A DIFFERENT MODULATION, and the object says so:
 * both are 182 bytes and the instruction sequences differ in four immediates
 * and nothing else -- the gate at params + 0x04 against V.17's + 0x08, the
 * dispatch slot at + 0x08 against + 0x14, the budget 6 against 0x30, and the
 * status byte 4 against 9.  `V29TX_modem` is the third copy.  Finding F9253.
 *
 * The parameter block is read ONCE before the gate and re-read at the top of
 * every loop iteration; the object hoists the first iteration's read out of
 * the loop (0x0a24ff and 0x0a259e reach the head at 0x0a252a, and the back
 * edge at 0x0a2527 reloads), which is loop rotation of exactly this source.
 */
int
V21TX_modem(void *modem, unsigned short *in, short *out, unsigned short *count)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	struct v21_tx_hdx *hdx;
	unsigned short taken;
	short budget;
	short total;

	hdx = tx->hdx;

	tx->result.byte.flags1 &=
		(unsigned char)~V21TX_RESULT_B1_BIT1;

	if (hdx->int_0004 == 0)
		taken = (unsigned short)FIFO_write(
				hdx->fifo,
				in, *count);
	else
		taken = *count;

	budget = V21TX_MODEM_BUDGET;
	total = 0;
	do {
		short got;

		hdx = tx->hdx;
		got = hdx->handler(modem, in, out, &budget);

		out += got;
		total = (short)(total + got);
	} while (budget > 0);

	if (*count != taken) {
		tx->result.byte.flags1 |= V21TX_RESULT_B1_BIT1;
		/*
		 * A BYTE store into the low byte of the int this function
		 * returns -- `movb $0x4,0x1c(%edi)` at 0x0a2564 -- which is
		 * why it cannot be written through `AT_I`.
		 */
		tx->result.byte.status = V21TX_RESULT_BYTE_04;
	}

	*count = (unsigned short)total;

	return tx->result.word;
}

/*
 * Advance the transmit state machine one step.
 *
 * THE OBJECT CARRIES THIS BLOCK FOUR TIMES, byte for byte: once out of line
 * as `TxNextStateV21` (0x0a25b0, 280 bytes) and three more times inlined,
 * once each into `TxHdxStartV21`, `TxHdxIdleV21` and `TxHdxDataV21` --
 * `RxNextStateV21`'s situation on the transmit side (F8898/F9091), except
 * every caller lands in this same commit, so there is no intermediate
 * `static` step to record.
 *
 * The four strings are the author's own words, out of .rodata.str1.1 at
 * 0x4b60, 0x4b72, 0x4b84 and 0x4b4d; `tools/relocscan.py` pairs them with
 * these sites (finding F604).  They name the CURRENT state at each arm --
 * "V21TX_STATE_DATA" is printed when state == V21TX_STATE_DATA, which then
 * installs `TxHdxIdleV21` and advances to V21TX_STATE_IDLE -- exactly as
 * `RxNextStateV21`'s strings name the state being LEFT, not the one entered.
 *
 * The default arm is reached for any state outside 0..2; it installs no
 * handler, only reports V21TX_STATUS_DEFAULT and clears
 * `V21TX_RESULT_B2_BIT0` while setting `V21TX_RESULT_B1_BIT1` and clearing
 * `V21TX_RESULT_B1_BIT0`.
 */
void
TxNextStateV21(void *modem)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	struct v21_tx_hdx *hdx = tx->hdx;
	short state = hdx->state;

	switch (state) {
	case V21TX_STATE_DATA:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21TX_STATE_DATA\n");
		hdx->handler = TxHdxIdleV21;
		hdx->state = V21TX_STATE_IDLE;
		tx->result.byte.flags2 |= V21TX_RESULT_B2_BIT0;
		tx->result.byte.flags1 &=
			(unsigned char)~V21TX_RESULT_B1_BIT0;
		break;

	case V21TX_STATE_IDLE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21TX_STATE_IDLE\n");
		hdx->short_000e = 0;
		hdx->handler = TxHdxStartV21;
		hdx->state = V21TX_STATE_START;
		tx->result.byte.flags2 &=
			(unsigned char)~V21TX_RESULT_B2_BIT0;
		tx->result.byte.flags1 |= V21TX_RESULT_B1_BIT0;
		break;

	case V21TX_STATE_START:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21TX_STATE_START\n");
		hdx->short_000e = 0;
		hdx->handler = TxHdxDataV21;
		hdx->state = V21TX_STATE_DATA;
		tx->result.byte.flags2 &=
			(unsigned char)~V21TX_RESULT_B2_BIT0;
		tx->result.byte.flags1 |= V21TX_RESULT_B1_BIT0;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21TX_DEFAULT, %d\n", state);
		tx->result.byte.flags2 &=
			(unsigned char)~V21TX_RESULT_B2_BIT0;
		tx->result.byte.status = V21TX_STATUS_DEFAULT;
		tx->result.byte.flags1 = (unsigned char)
			((tx->result.byte.flags1
			  | V21TX_RESULT_B1_BIT1)
			 & ~V21TX_RESULT_B1_BIT0);
		break;
	}
}

/*
 * The START state, installed by `V21TX_create` and by IDLE's own transition.
 *
 * Reads up to `*budget` elements out of the FIFO into `in`, modulates
 * whatever it got, decrements `*budget` by the amount actually taken, and
 * ALWAYS advances the state machine and reports V21TX_STATUS_START --
 * unconditionally, on every path, which is why the status write sits after
 * `TxNextStateV21` rather than inside any one of its arms: it overwrites
 * whatever that call itself wrote, including its own default-arm status.
 */
short
TxHdxStartV21(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	struct v21_tx_hdx *hdx = tx->hdx;
	unsigned short taken;
	short nsamples;

	taken = (unsigned short)
		FIFO_read(hdx->fifo,
			  in, (unsigned short)*budget);
	*budget = (short)((unsigned short)*budget - taken);

	nsamples = (short)ModDataV21(modem, in, out, taken);

	TxNextStateV21(modem);
	tx->result.byte.status = V21TX_STATUS_START;

	return nsamples;
}

/*
 * The IDLE state, installed only by `TxNextStateV21`'s own V21TX_STATE_DATA
 * arm.
 *
 * With the FIFO empty, asks `TxNoCarrierV21` for the WHOLE current budget in
 * one call -- not a per-block amount -- forces `*budget` to zero and reports
 * V21TX_STATUS_IDLE.  With the FIFO non-empty, does not modulate at all:
 * calls `TxNextStateV21` and returns 0, leaving `*budget` untouched for the
 * newly-installed handler to consume on the SAME caller block (`V21TX_modem`'s
 * `do`/`while` runs the dispatch slot again while `budget > 0`).
 */
short
TxHdxIdleV21(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	struct fax_fifo *fifo = tx->hdx->fifo;

	if (fifo->count == 0) {
		unsigned short b = (unsigned short)*budget;
		short nsamples = (short)TxNoCarrierV21(modem, in, out, b);

		*budget = (short)((unsigned short)*budget - b);
		tx->result.byte.status = V21TX_STATUS_IDLE;
		return nsamples;
	}

	TxNextStateV21(modem);
	return 0;
}

/*
 * The DATA state, installed only by `TxNextStateV21`'s own V21TX_STATE_START
 * arm.
 *
 * Reads up to `*budget` elements.  THREE ARMS:
 *
 *   - satisfied (the FIFO supplied the whole request): modulate `taken`,
 *     zero `*budget` (it equals `taken` by construction), report
 *     V21TX_STATUS_DATA.
 *   - underrun, `V21TXP_INT_0004 == 0`: modulate the FULL REQUESTED BUDGET
 *     regardless of what the FIFO actually supplied, force `*budget` to
 *     zero, and report V21TX_STATUS_DATA -- after MOMENTARILY reporting
 *     V21TX_STATUS_UNDERRUN, which the object writes and then immediately
 *     overwrites at the same call (D1240; reproduced because it is there and
 *     has no observable effect on any interface this file exposes).
 *   - underrun, `V21TXP_INT_0004 != 0`: modulate only `taken`, LEAVE the
 *     remainder in `*budget` (do not force it to zero), call
 *     `TxNextStateV21`, and report whatever that installed.
 */
short
TxHdxDataV21(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	struct v21_tx_hdx *hdx = tx->hdx;
	unsigned short req = (unsigned short)*budget;
	unsigned short taken;
	unsigned short nbits;
	short nsamples;

	taken = (unsigned short)
		FIFO_read(hdx->fifo,
			  in, req);

	if (req <= taken) {
		nbits = taken;
		nsamples = (short)ModDataV21(modem, in, out, nbits);
		*budget = (short)((unsigned short)*budget - nbits);
		tx->result.byte.status = V21TX_STATUS_DATA;
		return nsamples;
	}

	if (hdx->int_0004 != 0) {
		nbits = taken;
		nsamples = (short)ModDataV21(modem, in, out, nbits);
		*budget = (short)((unsigned short)*budget - taken);
		TxNextStateV21(modem);
		return nsamples;
	}

	tx->result.byte.flags1 |= V21TX_RESULT_B1_BIT1;
	nbits = req;
	tx->result.byte.status = V21TX_STATUS_UNDERRUN;
	nsamples = (short)ModDataV21(modem, in, out, nbits);
	*budget = (short)((unsigned short)*budget - nbits);
	tx->result.byte.status = V21TX_STATUS_DATA;

	return nsamples;
}

/*
 * V21TX_control -- .text 0x0a2ba0, 94 bytes.
 *
 * `arg == NULL` returns 0 and touches nothing.  Otherwise, in the object's
 * own order: `dsp->fsm.cfg.scale` is set from `arg->scale`, narrowed to
 * `short` as the object narrows it; `cfg->int_0008` (the transmit handle's
 * own head, `struct v21tx_cfg`) is set unconditionally from `arg->int_0004`;
 * `arg->mask`'s bit 2 ORs into `V21TX_FLAGS(modem)`; `arg->flags`'s
 * bit 4 sets `V21TXP_INT_0004` (of the params block at `V21TX_OBJ_PARAMS`)
 * to a boolean; and bit 1 of the same byte calls `V21TX_create(modem,
 * modem)` -- the self-referential reinit `V17RX_control`'s header comment
 * documents at length (finding F9900): the transmit handle's head IS its
 * own `struct v21tx_cfg`, so passing it as its own `params` re-copies its
 * current configuration onto itself and reruns construction.
 */
int
V21TX_control(void *modem, const struct v21tx_ctl *arg)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	struct v21tx_cfg *cfg = &tx->cfg;
	struct v21_tx_hdx *hdx = tx->hdx;

	if (arg == NULL)
		return 0;

	tx->dsp->fsm.cfg.scale = (short)arg->scale;
	cfg->int_0008 = arg->int_0004;

	if (arg->mask & V21TXCTL_SET_TXFLAGS_BIT2)
		((unsigned char *)(void *)&tx->cfg)[V21TX_OBJ_FLAGS] |=
			V21TXCTL_SET_TXFLAGS_BIT2;

	hdx->int_0004 =
		(arg->flags & V21TXCTL_SET_PARAMS_INT0004) != 0;

	if (arg->flags & V21TXCTL_REINIT)
		V21TX_create(modem, (const struct v21tx_cfg *)modem);

	return 1;
}

/*
 * Fill the caller's status block.
 *
 * The last statement ASSIGNS the flags byte rather than merging into it, so
 * the two bits cleared four statements earlier are cleared for nothing and
 * every other bit the caller had is lost.  That is the object's; see D1037.
 */
int
V21TX_status(void *modem, struct v21_status *st)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	unsigned char tx_flags;

	if (st == NULL)
		return 0;

	tx_flags = ((unsigned char *)(void *)&tx->cfg)[V21TX_OBJ_FLAGS];
	st->protocol = tx->cfg.protocol;
	st->tx_bps = V21_STATUS_BPS;
	st->rx_bps = 0;
	st->quality = 0;
	st->snr = 0;
	st->short_0a = 0;
	st->short_0c = 0;
	st->flags &= (unsigned char)~(V21_STATUS_BIT0 | V21_STATUS_BIT1);
	st->short_10 = 0;
	st->short_12 = 0;
	st->flags1 &= (unsigned char)~V21_STATUS1_BIT0;
	st->flags = (unsigned char)(tx_flags & V21_STATUS_BIT2);

	return 1;
}

/*
 * One block through the receive chain.
 *
 * The DSP block is re-read from the handle at every use rather than cached,
 * because the object re-reads it: four separate `mov 0x50(%edi),%e?x` at
 * 0x0a5764, 0x0a5772, 0x0a57a0/0x0a57c8 and 0x0a57f7, with `%edi` holding the
 * handle throughout.  A local pointer would have lived in a callee-saved
 * register across the calls instead.
 *
 * `FPM_AGC_agc` IS GIVEN A FOURTH ARGUMENT BY THE OBJECT and its return value
 * is used, and it has neither.  The extra argument is dead stack setup and is
 * not reproduced -- an argument the callee never loads has no observable
 * effect, which is `V21RX_delete`'s note above and `v22data.c`'s at its own
 * call site.  The value in %eax on return is `agc.signal`, the same quantity
 * the function's last store put in the state, so the field is read here
 * instead; `src/pump/v23/bwchdem.c` records that reading and is tested on it.
 *
 * The squelch loop indexes with an `unsigned short` and compares `jb`
 * (0x0a57be..0x0a57c4), so a count with the top bit set walks forward rather
 * than not at all.
 */
unsigned short
DemodDataV21(void *modem, short *in, short *bits, unsigned short count)
{
	struct v21_rx *rx = (struct v21_rx *)modem;
	short nsamples;

	FPM_AGC_agc(&rx->dsp->agc, in, count);

	rx->dsp->int_0004 = rx->dsp->agc.signal;
	rx->dsp->int_0008 = 1;

	if (FPM_MTD_detect(rx->dsp->mtd, in, (short)count)
	    != FPM_MTD_ABSENT) {
		rx->dsp->int_0008 = 0;

		if (rx->hdx->handler != RxHdxDataV21) {
			unsigned short i;

			for (i = 0; i < count; i++)
				in[i] = 0;
		}
	}

	nsamples = FPM_MRF_filter(&rx->dsp->mrf, in,
				  rx->dsp->mag, (short)count);

	return (unsigned short)
		FPM_FSD_demodulate(&rx->dsp->fsd,
				   rx->dsp->mag,
				   (unsigned short *)(void *)bits,
				   (unsigned short)nsamples);
}

/* Carrier present: the receive block's two words at once. */
int
CarrierDetectV21(void *modem)
{
	struct v21_rx_dsp *dsp = ((struct v21_rx *)modem)->dsp;

	return dsp->int_0004 & dsp->int_0008;
}

/*
 * Rectify the demodulator's trace into the block's own buffer.
 *
 * The three fields are lifted into locals before the loop because the object
 * lifts them: 0x74, 0x70 and 0x90 are all loaded at 0x0a583c..0x0a5843,
 * ahead of the first test.  Read through the struct each time they would not
 * be, since the store into `mag` may alias them.
 *
 * The absolute value is written out rather than called: C's `abs()` is
 * undefined at INT_MIN and this one is reached with -32768, which the object
 * turns into -32768 by truncating 32768 back to a short.  That corner is
 * driven by the differential test rather than reasoned about.
 *
 * THE SECOND LOOP HAS NO BODY IN THE OBJECT, and this is not an omission
 * here.  0x0a5868..0x0a5875 counts from zero to the same bound with nothing
 * between the increment and the test, and the return is a literal zero.  The
 * natural reading is an accumulation whose result became dead before the
 * compiler saw it, but the object does not say that and nothing here claims
 * it.  Reproduced because it is there; it has no observable effect.  D1038.
 */
int
GetSNRV21(void *modem)
{
	struct v21_rx_dsp *dsp = ((struct v21_rx *)modem)->dsp;
	short n = dsp->fsd.last_count;
	const short *trace = dsp->fsd.trace;
	short *mag = dsp->mag;
	short i;

	for (i = 0; i < n; i++) {
		int v = trace[i];

		mag[i] = (short)(v < 0 ? -v : v);
	}

	for (i = 0; i < n; i++)
		;

	return 0;
}

/*
 * Modulate, then resample.  The handle is re-read after the modulator
 * returns; see the header.
 */
unsigned short
ModDataV21(void *modem, const unsigned short *bits, short *out,
	   unsigned short nbits)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	unsigned short nsamples;

	nsamples = (unsigned short)FPM_FSM_modulate(&tx->dsp->fsm,
						    bits,
						    tx->dsp->scratch,
						    nbits);

	return (unsigned short)FPM_MRF_filter(&tx->dsp->mrf,
					      tx->dsp->scratch, out,
					      (short)nsamples);
}

/*
 * The same with the output scale forced to zero across the modulator, so the
 * modulator's phase and the converter's history advance exactly as they would
 * have and the carrier comes back in phase.  The converter runs with whatever
 * scale it was given, which by then is silence anyway.
 */
unsigned short
TxNoCarrierV21(void *modem, const unsigned short *bits, short *out,
	       unsigned short nbits)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	short saved_scale = tx->dsp->fsm.cfg.scale;
	unsigned short nsamples;

	tx->dsp->fsm.cfg.scale = 0;
	nsamples = (unsigned short)FPM_FSM_modulate(&tx->dsp->fsm,
						    bits,
						    tx->dsp->scratch,
						    nbits);
	tx->dsp->fsm.cfg.scale = saved_scale;

	return (unsigned short)FPM_MRF_filter(&tx->dsp->mrf,
					      tx->dsp->scratch, out,
					      (short)nsamples);
}

/*
 * The layout above is a claim about a 32-bit object and is asserted as one.
 * The guard is the tree's usual `__SIZEOF_POINTER__` one; `tools/assertlive.py`
 * is what keeps it from quietly reading `#if 0` under a compiler that does not
 * predefine it.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

/*
 * THE TYPEDEF NAME CARRIES `__LINE__`, AND THAT IS NOT DECORATION.  Naming it
 * after the FIELD alone collides the moment two structures here share a field
 * name, and two of them do: `mrf` is in both `v21_tx_dsp` and `v21_rx_dsp`.
 * GCC 14 accepts an identical typedef redefinition (C11 permits it) and said
 * nothing; GCC 3.4.2 rejects it outright, so `make period` -- the tier that
 * decides -- would not compile this file at all.  A discriminator that cannot
 * repeat is what keeps the next added field from bringing it back.
 */
#define V21_CAT2(a, b)	a##b
#define V21_CAT(a, b)	V21_CAT2(a, b)
#define V21_ASSERT_OFF(type, field, off) \
	typedef char V21_CAT(v21_off_line_, __LINE__)[ \
		((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

V21_ASSERT_OFF(struct v21_tx_dsp, fsm, 0x00);
V21_ASSERT_OFF(struct v21_tx_dsp, mrf, 0x10);
V21_ASSERT_OFF(struct v21_tx_dsp, scratch, 0x2c);
V21_ASSERT_OFF(struct v21_tx_hdx, fifo, 0x00);
V21_ASSERT_OFF(struct v21_tx_hdx, int_0004, 0x04);
V21_ASSERT_OFF(struct v21_tx_hdx, handler, 0x08);
V21_ASSERT_OFF(struct v21_tx_hdx, state, 0x0c);
V21_ASSERT_OFF(struct v21_tx_hdx, short_000e, 0x0e);
V21_ASSERT_OFF(struct v21_tx, result, 0x1c);
V21_ASSERT_OFF(struct v21_tx, hdx, 0x20);
V21_ASSERT_OFF(struct v21_tx, dsp, 0x24);

V21_ASSERT_OFF(struct v21_rx_dsp, int_0004, 0x04);
V21_ASSERT_OFF(struct v21_rx_dsp, int_0008, 0x08);
V21_ASSERT_OFF(struct v21_rx_dsp, agc, 0x0c);
V21_ASSERT_OFF(struct v21_rx_dsp, mrf, 0x38);
V21_ASSERT_OFF(struct v21_rx_dsp, fsd, 0x54);
V21_ASSERT_OFF(struct v21_rx_dsp, mtd, 0x8c);
V21_ASSERT_OFF(struct v21_rx_dsp, mag, 0x90);

V21_ASSERT_OFF(struct v21_rx_hdx, handler, 0x04);
V21_ASSERT_OFF(struct v21_rx_hdx, state, 0x08);
V21_ASSERT_OFF(struct v21_rx_hdx, countdown, 0x0a);
V21_ASSERT_OFF(struct v21_rx_hdx, ones_run, 0x0c);
V21_ASSERT_OFF(struct v21_rx_hdx, mark_seq, 0x0e);
V21_ASSERT_OFF(struct v21_rx, status, 0x18);
V21_ASSERT_OFF(struct v21_rx, ptr_001c, 0x1c);
V21_ASSERT_OFF(struct v21_rx, ptr_0024, 0x24);
V21_ASSERT_OFF(struct v21_rx, hdx, 0x4c);
V21_ASSERT_OFF(struct v21_rx, dsp, 0x50);

V21_ASSERT_OFF(struct v21_status, tx_bps, 0x02);
V21_ASSERT_OFF(struct v21_status, rx_bps, 0x04);
V21_ASSERT_OFF(struct v21_status, quality, 0x06);
V21_ASSERT_OFF(struct v21_status, snr, 0x08);
V21_ASSERT_OFF(struct v21_status, short_0a, 0x0a);
V21_ASSERT_OFF(struct v21_status, short_0c, 0x0c);
V21_ASSERT_OFF(struct v21_status, short_0e, 0x0e);
V21_ASSERT_OFF(struct v21_status, short_10, 0x10);
V21_ASSERT_OFF(struct v21_status, short_12, 0x12);
V21_ASSERT_OFF(struct v21_status, flags, 0x14);
V21_ASSERT_OFF(struct v21_status, flags1, 0x15);
V21_ASSERT_OFF(struct v21_status, int_18, 0x18);

/*
 * The two DSP blocks are gapless: every offset above abuts the next, which is
 * what makes the layout a reading of the object rather than a set of
 * independent guesses.  Asserting the sizes is what would catch a sub-struct
 * changing under us.
 */
typedef char v21_tx_dsp_size[(sizeof(struct v21_tx_dsp) == 0x30) ? 1 : -1];
typedef char v21_rx_dsp_size[(sizeof(struct v21_rx_dsp) == 0x94) ? 1 : -1];
typedef char v21_tx_hdx_size[(sizeof(struct v21_tx_hdx) == 0x10) ? 1 : -1];
typedef char v21_tx_result_size[(sizeof(union v21_tx_result) == 4) ? 1 : -1];
typedef char v21_rx_status_word_size[
	(sizeof(union v21_rx_status_word) == 4) ? 1 : -1];
typedef char v21_tx_size[(sizeof(struct v21_tx) == 0x28) ? 1 : -1];
typedef char v21_rx_size[(sizeof(struct v21_rx) == 0x54) ? 1 : -1];

/*
 * The transmit config table is what `V21TX_create` copies onto the handle's
 * head, whole; its size is the literal 28 `V21TX_create` itself carries
 * (0x099328..0x09934d's six-plus-one dword copy), and confirming it here
 * catches a struct-shape slip the same way `t_faxcfg.c`'s `offcheck.py` pass
 * caught one for `v29rx_cfg` (finding F9059).
 */
typedef char v21tx_cfg_size[(sizeof(struct v21tx_cfg) == 0x1c) ? 1 : -1];

#endif

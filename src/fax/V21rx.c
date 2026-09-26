/*
 * V21rx.c -- split out of the merged v21.c so the definitions sit in the
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
 * The V.21 struct-offset assertions, moved here verbatim from the deleted
 * ours-only v21.c so the TU set matches the object.  Compile-time only;
 * emits no code.
 */
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

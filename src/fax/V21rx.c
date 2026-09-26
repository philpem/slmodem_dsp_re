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


/*
 * The nine V.21 configuration/coefficient tables, moved here VERBATIM from
 * the deleted ours-only v21cfg.c: the blob defines them in the unit that
 * holds V21RX_create (F11413).  A data symbol byte does not depend on its
 * translation unit.
 */
#include "dsplib/v21cfg.h"
/*
 * The AGC smoother's coefficients, `R` at .rodata 0x0a100 and 0x0a0fc.
 *
 * GLOBAL and uniquely named, unlike the six-times-defined `AGC_DEF_ALPHA` and
 * `AGC_DEF_BETA` that F9144 had to reach through their consumer.  They are
 * declared in the header and compared BY NAME in the test.
 *
 * Element 0 and element 1 each sum to 32768, which is unity DC gain in Q15
 * for `y += alpha*y + beta*x`.  Only element 0 is ever selected.
 */
const short AGC_DEF_ALPHA_v21[2] = { 16384, 32604 };
const short AGC_DEF_BETA_v21[2] = { 16384,   164 };

/*
 * `R` at .rodata 0x0a0e4, 24 bytes, and the only table here with relocations
 * inside it: +0x0c and +0x10 hold the two arrays above.  Both targets had to
 * be written before this symbol could be, because the link constraint binds
 * on a STORED POINTER exactly as it does on a call (F8492/F8493).
 *
 * `f16` is 6553 -- 0.2 in Q15 -- which F1621 measured as the value V.32's two
 * configs carry and Bell 103's and V.23's leave at zero.  It is not padding.
 */
const struct fpm_agc_cfg AGCv21_CFG = {
	10000,			/* +0x00 ref_level; output settles at 5000  */
	2,			/* +0x02 acquire_level                      */
	32,			/* +0x04 squelch_level                      */
	1000,			/* +0x06 f06                                */
	1,			/* +0x08 f08                                */
	40,			/* +0x0a block_len                          */
	AGC_DEF_ALPHA_v21,	/* +0x0c alpha                              */
	AGC_DEF_BETA_v21,	/* +0x10 beta                               */
	158,			/* +0x14 f14                                */
	6553			/* +0x16 f16; 0.2 in Q15, see F1621         */
};

/*
 * `D` at .data 0x7ab4, 24 bytes.  The fourth member of `faxcfg.h`'s family
 * and a fourth TYPE -- see the header for the `cmpw` that forces the width of
 * `chan2` and separates it from `struct v29rx_cfg`.
 *
 * Writable in the object even though nothing in the 1.2 MB writes it, which
 * is what the other three tables do too.
 */
struct v21rx_cfg V21RX_CFG = {
	1,			/* +0x00 chan2: default to the answer side  */
	0,			/* +0x02 short_0002                         */
	300,			/* +0x04 bit_rate; V.21's only rate         */
	0,			/* +0x06 short_0006                         */
	60000,			/* +0x08 int_0008; 60000 in all four        */
	0,			/* +0x0c int_000c                           */
	0,			/* +0x10 int_0010                           */
	0			/* +0x14 aux                                */
};

/*
 * The FSK discriminator's lowpass, `R` at .rodata 0x0a088.  Three biquad
 * sections of five shorts: `V21RX_create` writes `fsd.iir_len = 3` at
 * 0x09900d, and 3 * 5 * 2 is the symbol's 30 bytes.  Shared by both channels
 * -- neither arm of the `chan2` test replaces it.
 */
const short V21RX_IIR_LPF[15] = {
	-12333,    445,  28281,   -454,    445, -14208,   4528,  29701,
	 -8164,   4528, -15782,   9514,  30961, -17829,   9514,
};
/*
 * The channel-2 discriminator FIR, `R` at .rodata 0x0a0a6.  Fifteen taps:
 * `V21RX_create` writes `fsd.fir_taps = 15` at 0x098ffc beside whichever of
 * these two it installed, and 15 * 2 is the symbol's 30 bytes.
 *
 * Selected when `V21RX_CFG.chan2` is non-zero, together with `fsd.delay = 3`.
 */
const short V21RX_CHAN2_INTRP[15] = {
	   229,   -274,    341,   -451,    667,  -1282,  16185,   1523,
	  -727,    478,   -356,    283,   -235,    201,   -176,
};
/*
 * The channel-1 discriminator FIR, `R` at .rodata 0x0a0c4.  The same fifteen
 * taps' worth of shape, selected when `chan2` is zero, with `fsd.delay = 5`.
 *
 * Its first and last entries are 0 and its third-from-last is 3, so it is a
 * shorter response padded into the same fifteen-tap slot as channel 2's --
 * which is consistent with channel 1 sitting lower in frequency.  Stated as
 * an observation about the bytes; nothing depends on it.
 */
const short V21RX_CHAN1_INTRP[15] = {
	     0,     -3,     26,   -133,    501,  -1604,   6016,  13751,
	 -3008,   1146,   -401,    112,    -23,      3,      0,
};
/*
 * The receive rate converter's prototype, `R` at .rodata 0x0c000 -- 720
 * bytes, the largest table here.
 *
 * 360 taps: `V21RX_create` writes `mrf.taps = 0x168` at 0x098f50, and 360 * 2
 * is the symbol's size.  It keeps `FPM_MRF_CFG`'s own 9 branches and
 * decimation of 10 rather than patching them, so the block runs 9/10 -- 8000
 * Hz in, 7200 Hz out -- at 40 taps a branch.  V.29's equivalent is the same
 * 9 branches at 30 taps (F9140), so the branch count is the library's and the
 * per-branch length is the modulation's.
 */
const short V21_MRF_FILT[360] = {
	  -149,    -29,    -25,    -16,      0,     22,     49,     82,    117,    154,
	   191,    226,    255,    277,    291,    293,    284,    262,    229,    186,
	   134,     77,     18,    -40,    -93,   -137,   -170,   -189,   -192,   -180,
	  -152,   -111,    -59,      0,     61,    120,    172,    213,    238,    246,
	   235,    205,    157,     95,     23,    -54,   -131,   -200,   -256,   -295,
	  -312,   -306,   -275,   -221,   -147,    -58,     39,    137,    229,    306,
	   363,    394,    395,    365,    304,    218,    110,    -11,   -136,   -255,
	  -360,   -440,   -490,   -502,   -476,   -411,   -311,   -182,    -33,    124,
	   278,    417,    528,    602,    631,    612,    544,    430,    277,     96,
	  -100,   -296,   -477,   -628,   -735,   -788,   -782,   -713,   -585,   -405,
	  -186,     58,    306,    541,    743,    895,    982,    995,    929,    787,
	   576,    311,      9,   -306,   -611,   -881,  -1093,  -1228,  -1271,  -1215,
	 -1060,   -814,   -491,   -114,    290,    690,   1054,   1353,   1558,   1649,
	  1613,   1447,   1157,    759,    279,   -249,   -786,  -1290,  -1719,  -2036,
	 -2208,  -2213,  -2041,  -1695,  -1190,   -557,    163,    919,   1654,   2308,
	  2825,   3152,   3250,   3093,   2671,   1996,   1097,     25,  -1153,  -2358,
	 -3498,  -4478,  -5204,  -5590,  -5563,  -5069,  -4075,  -2575,   -593,   1823,
	  4596,   7627,  10798,  13979,  17035,  19830,  22238,  24150,  25477,  26156,
	 26156,  25477,  24150,  22238,  19830,  17035,  13979,  10798,   7627,   4596,
	  1823,   -593,  -2575,  -4075,  -5069,  -5563,  -5590,  -5204,  -4478,  -3498,
	 -2358,  -1153,     25,   1097,   1996,   2671,   3093,   3250,   3152,   2825,
	  2308,   1654,    919,    163,   -557,  -1190,  -1695,  -2041,  -2213,  -2208,
	 -2036,  -1719,  -1290,   -786,   -249,    279,    759,   1157,   1447,   1613,
	  1649,   1558,   1353,   1054,    690,    290,   -114,   -491,   -814,  -1060,
	 -1215,  -1271,  -1228,  -1093,   -881,   -611,   -306,      9,    311,    576,
	   787,    929,    995,    982,    895,    743,    541,    306,     58,   -186,
	  -405,   -585,   -713,   -782,   -788,   -735,   -628,   -477,   -296,   -100,
	    96,    277,    430,    544,    612,    631,    602,    528,    417,    278,
	   124,    -33,   -182,   -311,   -411,   -476,   -502,   -490,   -440,   -360,
	  -255,   -136,    -11,    110,    218,    304,    365,    395,    394,    363,
	   306,    229,    137,     39,    -58,   -147,   -221,   -275,   -306,   -312,
	  -295,   -256,   -200,   -131,    -54,     23,     95,    157,    205,    235,
	   246,    238,    213,    172,    120,     61,      0,    -59,   -111,   -152,
	  -180,   -192,   -189,   -170,   -137,    -93,    -40,     18,     77,    134,
	   186,    229,    262,    284,    293,    291,    277,    255,    226,    191,
	   154,    117,     82,     49,     22,      0,    -16,    -25,    -29,   -149,
};
/*
 * The channel-1 tone detector's coefficients, `D` at .data 0x7a74.
 *
 * Two resonator sections of five shorts in Q14, at 980 Hz and 1180 Hz --
 * V.21 channel 1's mark and space.  All ten entries are reproduced exactly by
 * rounding { -r^2, 1.0, 2*r*cos(w), -2*cos(w), 1.0 } with r = 0.9 and
 * w = 2*pi*f/8000; see the header, and F9350 for the measurement over both
 * banks at once.
 *
 * `V21_CHAN2_MTD_COEFF` is its channel-2 twin and lives in `faxcfg.c`,
 * because all three fax receivers reference that one and only V.21 references
 * this one.
 */
short V21_CHAN1_MTD_COEFF[10] = {
	-13271,  16384,  21178, -23532,  16384,
	-13271,  16384,  17707, -19675,  16384,
};

/*
 * `D` at .data 0x07af8, 28 bytes.  `V21TX_create`'s own default; see the
 * header for the derivation and for why the SHORT/INT split is the
 * receiver's precedent rather than something this table's own instructions
 * force.
 */
struct v21tx_cfg V21TX_CFG = {
	1,		/* +0x00 protocol                           */
	300,		/* +0x02 bit_rate                           */
	0,		/* +0x04 short_0004                         */
	0,		/* +0x06 short_0006                         */
	60000,		/* +0x08 int_0008                           */
	3200,		/* +0x0c int_000c                           */
	0,		/* +0x10 flags                              */
	0,		/* +0x14 int_0014                           */
	0		/* +0x18 int_0018                           */
};

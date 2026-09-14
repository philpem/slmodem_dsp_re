/*
 * V32.c -- ITU-T V.32 / V.32bis: constructor and datapump recreation.
 *
 *   V32FP_recreate  .text 0x07e870  3733
 *
 * One function, and it is the whole V.32 datapump laid out: three heap
 * allocations, seven `FPM_*` sub-objects configured from seven templates,
 * two scramblers, two symbol coders, a Viterbi decoder, three tone detectors
 * and a multi-tone detector, and finally the half-duplex machine's opening
 * state.  Nothing here does arithmetic on a sample.
 *
 * ---------------------------------------------------------------------------
 * WHY IT IS `recreate` AND NOT `create`, AND WHAT `fresh` IS
 *
 * A NULL `modem` allocates the instance, the datapump block, the half-duplex
 * context and four buffers, and sets a local flag; a non-NULL one re-uses
 * everything that is already there.  That flag is then handed to every
 * `FPM_*_init` as its third argument and to `VTBv32_init` as its `alloc`, and
 * those functions' own headers already name it `fresh` -- non-zero means "the
 * buffers do not exist yet, allocate without inspecting them".  It is also
 * what decides whether the three `FPM_TONE_create` calls are given the
 * existing tone object or a NULL that makes them allocate a new one.
 *
 * So one function serves both the constructor and the reconfigure, which is
 * exactly what its two callers need: `V32FP_create` (0x7f710) calls it with a
 * NULL instance and a patched copy of `V32_CFG`, and `V32FP_control`
 * (0x84530) calls it as `V32FP_recreate(obj, obj, 0)` -- passing the instance
 * as BOTH arguments, which works because the instance's first 48 bytes ARE
 * the parameter block (v32fp.h, finding F8641).
 *
 * THE THIRD ARGUMENT IS NEVER READ.  Nothing in the 3733 bytes touches
 * 0x1a8(%esp).  `V32FP_create` passes its own second argument through to it
 * and `V32FP_control` passes zero, so it exists and is dead; it is `void *`
 * here because the two writers both move it as one dword and nothing types it
 * further.  Finding F8650.
 *
 * ---------------------------------------------------------------------------
 * THE INSTANCE IS NOT MODELLED AS A STRUCT
 *
 * v32data.h's ruling, and v32fpctl.h's after it: the parameter is `void *`
 * and the offsets are named constants.  What IS modelled is the parameter
 * block -- `struct v32fp_params`, v32fp.h's, which is the instance's first 48
 * bytes -- and every sub-object this function configures, each of which has a
 * header of its own.  The map, all of it forced by an instruction here:
 *
 *      obj + 0x00   struct v32fp_params    the parameter block
 *      obj + 0x30   status / flags         v32fpctl.h
 *      obj + 0x34   the diagnostic window  see below
 *      obj + 0x64   the half-duplex context, 0xb0 bytes
 *      obj + 0x68   the datapump block, 0x50dc bytes
 *      obj total    0x6c bytes
 *
 *      hdx + 0x00   struct fpm_agc         AGCv32Prc_CFG's
 *      hdx + 0x2c   three struct fpm_tone *
 *      hdx + 0x38   struct fpm_mtd *
 *      hdx + 0xa4   a 0x64-byte buffer
 *
 *      fp  + 0x00   ten int switches       (F8642; not named)
 *      fp  + 0x28   tx and rx rate index   v32seq.h
 *      fp  + 0x30   struct v32_sdm         the transmit scrambler
 *      fp  + 0x48   struct v32_smc         the transmit coder
 *      fp  + 0x60   struct fpm_pps         the pulse shaper
 *      fp  + 0x98   three v32_encoder_fn, then the selector at +0xa4
 *      fp  + 0xb0   struct v32_symout      the symbol ring
 *      fp  + 0xc4   struct fpm_mrf         the resampler
 *      fp  + 0xe0   struct fpm_ecc         the echo canceller
 *      fp  + 0x148  struct fpm_sre         the timing recovery
 *      fp  + 0x1d8  struct fpm_agc         AGCv32_CFG's
 *      fp  + 0x204  struct fpm_fse         the equaliser / slicer
 *      fp  + 0x5020 struct v32_dec         the decoder, with its vtb at +0x18
 *      fp  + 0x5098 struct v32_smc         the RECEIVE coder
 *      fp  + 0x50b0 struct v32_sdm         the descrambler
 *
 * Every one of those bases is pinned by a `sizeof` chain that closes exactly:
 * fpm_pps ends at 0x98, fpm_mrf at 0xe0, fpm_ecc at 0x148, fpm_sre at 0x1d8,
 * fpm_agc at 0x204 and fpm_fse at 0x501c.  Six sub-object sizes measured by
 * six other files, and the next base in this function lands on each.
 *
 * ---------------------------------------------------------------------------
 * `VTBv32_init` IS INLINED HERE IN THE OBJECT, AND IS CALLED HERE
 *
 * The object carries `VTBv32_init`'s whole body at 0x7ee15..0x7ee9f -- the
 * survivor-ring allocation, the 128-node clear, the four-armed rate switch and
 * the metric clear -- with no call.  `VTBv32_init` is at 0x7e700 and ends at
 * 0x7e863, twelve bytes before this function starts, so the two were in one
 * translation unit and `-finline-functions` took it.  This file calls it: the
 * behaviour is identical and our factoring is not the object's, which is
 * finding F605's case and not a defect.  Finding F8653.
 *
 * ---------------------------------------------------------------------------
 * WHAT +0x24 OF THE PARAMETER BLOCK IS FOR
 *
 * It is read once and written into SIX configuration structures -- the `aux`
 * of `fpm_pps_cfg`, `fpm_mrf_cfg` and `fpm_ecc_cfg`, `fpm_fse_cfg`'s
 * `reserved34`, `fpm_sre_cfg` +0x34 and `fpm_tone_cfg` +0x18 -- and by
 * nothing else.  Every one of those is a 32-bit slot at the tail of its
 * configuration that the module owning it records as copied and never read.
 * So the six are ONE field under six spellings, and this function is what
 * fills them; what the value MEANS is still not established and +0x24 keeps
 * its neutral name.  Finding F8651.
 *
 * Two of the six have no member to assign: `fpm_sre_cfg` spells the slot
 * `pad34`/`pad36` and `fpm_tone_cfg` spells it inside `r16[3]`.  Renaming
 * either is a change to a header two other datapumps initialise
 * positionally -- `src/pump/v22/v22rxtab.c` is one, and this pass does not own
 * it -- so those two are written through the slot's address with the object's
 * own single 32-bit store, and the finding records the rename as owed.
 */

#include <string.h>

#include "dsplib/v32fpctl.h"

#include "dsplib/v32cfg.h"
#include "dsplib/v32data.h"
#include "dsplib/v32dec.h"
#include "dsplib/v32fp.h"
#include "dsplib/v32fpstat.h"		/* V32FP_recreate's own prototype    */
#include "dsplib/v32fse.h"
#include "dsplib/v32hdx.h"
#include "dsplib/v32hdxst.h"
#include "dsplib/v32scram.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32smc.h"
#include "dsplib/vtb.h"

#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_ecc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sysdep.h"

/*
 * FILE-LOCAL IN THE OBJECT (`r`), and this file is its only consumer --
 * `V32FP_recreate` copies `[3]` into `params.disconnect_thresh`.  It moved out
 * of `v32fptab.c` and is `static` here; a test names it through the test
 * tier's globalized copies (tools/testvisible.py).
 */
static const short V32DiconnectThreshTable[8] = {
	75, 95, 119, 150, 168, 174, 212, 238
};


/* The instance is not modelled; these are v32fpctl.c's accessors. */
#define FIELD(obj, off)		((unsigned char *)(void *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_INT(obj, off)	(*(int *)(void *)FIELD((obj), (off)))
#define FIELD_S16(obj, off)	(*(short *)(void *)FIELD((obj), (off)))
#define FIELD_U16(obj, off)	(*(unsigned short *)(void *)FIELD((obj), (off)))
#define FIELD_U8(obj, off)	(*(unsigned char *)FIELD((obj), (off)))

#define HDX(m)			FIELD_PTR((m), V32_OBJ_HDX)
#define FP(m)			FIELD_PTR((m), V32_OBJ_FP)

#define SDM_TX(fp)	((struct v32_sdm *)(void *)FIELD((fp), V32FP_SCRAMBLER))
#define SDM_RX(fp)	((struct v32_sdm *)(void *)FIELD((fp), V32FP_DESCRAMBLER))
#define SMC_TX(fp)	((struct v32_smc *)(void *)FIELD((fp), V32FP_SMC))
#define PPS(fp)		((struct fpm_pps *)(void *)FIELD((fp), V32FP_PPS))
#define RING(fp)	((struct v32_symout *)(void *)FIELD((fp), V32FP_SYMOUT))
#define MRF(fp)		((struct fpm_mrf *)(void *)FIELD((fp), V32FP_MRF))
#define ECC(fp)		((struct fpm_ecc *)(void *)FIELD((fp), V32FP_ECC))
#define SRE(fp)		((struct fpm_sre *)(void *)FIELD((fp), V32FP_SRE))
#define FSE(fp)		((struct fpm_fse *)(void *)FIELD((fp), V32FP_FSE))

/*
 * Sizes, all three from a `sysdep_malloc` immediate in this function and
 * nowhere else, so they are the object's own and not a `sizeof` of ours.
 */
#define V32_OBJ_SIZE		0x6c
#define V32_HDX_SIZE		0xb0
#define V32FP_SIZE		0x50dc
#define V32_HDX_BUF_A4_SIZE	0x64	/* hdx + 0xa4, the state buffer      */
#define V32FP_CLEAN_SIZE	0x154	/* both fp + 0x50cc and fp + 0x50d0  */

/*
 * The three switches this function drives from `options`, continuing
 * v32fpctl.h's bank at fp + 0x00..0x18.  `V32FP_control` sets the same three
 * from bits 0, 1 and 2 of its request byte (finding F8642), which is where
 * their grouping comes from; nothing names them.
 */
#define V32FP_R1C		0x1c
#define V32FP_R20		0x20
#define V32FP_R24		0x24

/*
 * The RECEIVE symbol coder, and it is a `struct v32_smc` for the same reason
 * fp + 0x48 is: this function writes the SAME seven fields at both bases from
 * one template, +0x00, +0x04, +0x06, +0x08, +0x0a, +0x0e, +0x10 and +0x12.
 * v32fpctl.h's V32FP_SHORT_5098 and V32FP_SHORT_509C are this object's `mode`
 * and `shift`.  Finding F8652.
 */
#define V32FP_SMC_RX		0x5098
#define SMC_RX(fp)	((struct v32_smc *)(void *)FIELD((fp), V32FP_SMC_RX))

/* The decoder, which is also what `fpm_fse_cfg::owner` is pointed at. */
#define V32FP_DEC		0x5020
#define DEC(fp)		((struct v32_dec *)(void *)FIELD((fp), V32FP_DEC))

/* The second AGC, the one AGCv32_CFG configures; hdx + 0x00 is the other. */
#define V32FP_AGC		0x1d8

/*
 * MODELLED, UNNAMED.  Two `short *` in the datapump block that `SetTxModeV32`
 * has no counterpart for: this function seeds them with the sixteen-point
 * constellation and nothing reconstructed reads them back.
 */
#define V32FP_IMAP		0xa8
#define V32FP_QMAP		0xac

/*
 * The instance's diagnostic window, obj + 0x34 .. obj + 0x63.
 *
 * Nine pointers and three counts, all of them derived from the equaliser and
 * the echo canceller after those two are initialised, and NONE of them read
 * by anything reconstructed -- so what consumes the window is open and the
 * offsets stay offsets.  What IS established is the shape: four of the
 * pointers are the four coefficient banks inside one `fpm_ecc::coef[0]`
 * allocation, in the order fpm_ecc.h states (near-I, near-Q, far-I, far-Q),
 * with the near and far tap counts beside them.
 */
#define V32_OBJ_DIAG_OUT_I	0x34	/* fpm_fse::out_i                    */
#define V32_OBJ_DIAG_OUT_Q	0x38	/* fpm_fse::out_q                    */
#define V32_OBJ_DIAG_N_OUT	0x3c	/* &fpm_fse::n_out                   */
#define V32_OBJ_DIAG_ICOEFF	0x40	/* fpm_fse::icoeff                   */
#define V32_OBJ_DIAG_QCOEFF	0x44	/* fpm_fse::qcoeff                   */
#define V32_OBJ_DIAG_FSE_TAPS	0x48	/* short, the literal 0x31           */
#define V32_OBJ_DIAG_NEAR_I	0x4c	/* fpm_ecc::coef[0]                  */
#define V32_OBJ_DIAG_NEAR_Q	0x50
#define V32_OBJ_DIAG_NEAR_N	0x54	/* unsigned short                    */
#define V32_OBJ_DIAG_FAR_I	0x58
#define V32_OBJ_DIAG_FAR_Q	0x5c
#define V32_OBJ_DIAG_FAR_N	0x60	/* unsigned short                    */

/*
 * MODELLED, UNNAMED -- the half-duplex context's fields this function seeds
 * and no reconstructed file names.  +0x78 and +0x80 are both `int` and both
 * take the same computed timeout on the TxHdxNull path, which is what pairs
 * them; v32fpctl.h already records +0x7c as an accumulator against +0x80.
 */
#define V32HDX_TIMEOUT		0x78	/* int                               */
#define V32HDX_ELAPSED		0x7c	/* int; v32fpctl.h's accumulator     */
#define V32HDX_TIMEOUT_MAX	0x80	/* int                               */
#define V32HDX_SYMBOL_LEN2	0x84	/* short, a second V32_SYMBOL_LEN     */
#define V32HDX_INT_88		0x88
#define V32HDX_INT_90		0x90
#define V32HDX_SHORT_AC		0xac
#define V32HDX_SHORT_AE		0xae

/*
 * The scratch register bank is SEVEN shorts here and the accessors bound it
 * at FIVE.  `LoadReg`/`StoreReg` reject an index above 4 (v32seq.h), and this
 * function clears +0x3c through +0x48 -- seven -- with seven separate stores.
 * Recorded rather than reconciled: nothing says the two extra words belong to
 * the array, and nothing says they do not.  Finding F8654.
 */
#define V32HDX_REGS_CLEARED	7

/*
 * The five bit rates the two ladders recognise, and the rate index each one
 * maps to is v32seq.h's.  Written as the object writes them -- four 16-bit
 * compares in this order, with index 0 as the fall-through.
 */
#define V32_BPS_14400	14400
#define V32_BPS_12000	12000
#define V32_BPS_9600	9600
#define V32_BPS_7200	7200

/*
 * `options` bit 10 -- the byte at +0x11, bit 2.  It is the only bit of the
 * word this function tests, `V32FP_create` patches it from its caller's
 * configuration, and BOTH protocol arms below are skipped when it is set.
 * Named for the bit position and nothing more: what it selects is not
 * established.
 */
#define V32_OPT_BIT10		(1u << 10)

/* The three protocols obj + 0x00 selects, and the hdx mode each installs. */
#define V32_PROTOCOL_0		0
#define V32_PROTOCOL_1		1

/*
 * MODELLED, UNNAMED -- bit 6 of `V32_OBJ_FLAGS`.  This function is its only
 * writer in the object: it clears the whole dword at +0x30, sets this bit,
 * and then writes 1 into the status byte, all three at the point where the
 * datapump is finished.  Nothing reconstructed reads it, so it gets its bit
 * value and no meaning; `V32_FLAG_FAULT` (0x02) is the one bit of this byte
 * that IS named, and by two writers rather than a guess.
 */
#define V32_FLAG_BIT6		(1 << 6)

/*
 * Q15 scale factors this function applies, all of them bare immediates in the
 * object.  0x2666 is 0.3 and 0x4ccc is 2.4 in Q13; the rounding constant
 * differs between the echo canceller's delay (0x8000) and the two half-duplex
 * ones (0x4000) and that difference is the object's.
 */
#define V32_DELAY_SCALE		0x2666
#define V32_TIMEOUT_SCALE	0x4ccc

void *
V32FP_recreate(void *modem, const struct v32fp_params *param, void *arg2)
{
	struct v32fp_params *p;
	unsigned char *fp;
	unsigned char *hdx;
	struct v32_sdm *sdm;
	struct v32_smc_cfg smccfg;
	struct fpm_pps_cfg ppscfg;
	struct fpm_mrf_cfg mrfcfg;
	struct fpm_ecc_cfg ecccfg;
	struct fpm_sre_cfg srecfg;
	struct fpm_fse_cfg fsecfg;
	struct fpm_tone_cfg tonecfg;
	struct fpm_mtd_cfg mtdcfg;
	short sdmcfg[3];
	unsigned int mask;
	unsigned short rate;
	unsigned short prot;
	short *coef;
	short symlen;
	short sel;
	int near_n, far_n;
	int timeout;
	int delay;
	int shift;
	int fresh;
	int i;

	(void)arg2;

	fresh = 0;
	if (modem == 0) {
		modem = sysdep_malloc(V32_OBJ_SIZE);
		FIELD_PTR(modem, V32_OBJ_FP) = sysdep_malloc(V32FP_SIZE);
		hdx = sysdep_malloc(V32_HDX_SIZE);
		FIELD_PTR(modem, V32_OBJ_HDX) = hdx;
		FIELD_PTR(hdx, V32_HDX_TONE0) = 0;
		FIELD_PTR(hdx, V32_HDX_MTD) = 0;
		FIELD_PTR(hdx, V32_HDX_BUF_A4) =
			sysdep_malloc(V32_HDX_BUF_A4_SIZE);
		FIELD_PTR(FP(modem), V32FP_BUF_50CC) =
			sysdep_malloc(V32FP_CLEAN_SIZE);
		FIELD_PTR(FP(modem), V32FP_CLEAN_BUF) =
			sysdep_malloc(V32FP_CLEAN_SIZE);
		fresh = 1;
	}

	/*
	 * A NULL parameter block means "use V32_CFG".  Written as two struct
	 * assignments rather than as a reassignment of `param`, because the
	 * debug line at the bottom reads the CALLER'S pointer and not the
	 * copy -- the object reloads 0x1a4(%esp) there and never stores back
	 * to it, so a NULL block and debug level 2 dereference NULL in the
	 * object exactly as they do here.
	 */
	if (param == 0)
		*(struct v32fp_params *)modem = V32_CFG;
	else
		*(struct v32fp_params *)modem = *param;

	p = (struct v32fp_params *)modem;
	p->disconnect_thresh = V32DiconnectThreshTable[3];
	p->r2c = 0;

	fp = (unsigned char *)FP(modem);

	/* Three switches out of `options`, then seven more forced to 1. */
	FIELD_INT(fp, V32FP_R1C) = (int)(p->options & 1);
	FIELD_INT(fp, V32FP_R20) = (int)((p->options >> 1) & 1);
	FIELD_INT(fp, V32FP_R00) = 1;
	FIELD_INT(fp, V32FP_R04) = 1;
	FIELD_INT(fp, V32FP_R08) = 1;
	FIELD_INT(fp, V32FP_R24) = (int)((p->options >> 2) & 1);
	FIELD_INT(fp, V32FP_R0C) = 1;
	FIELD_INT(fp, V32FP_EQ_ADAPT) = 1;
	FIELD_INT(fp, V32FP_R14) = 1;
	FIELD_INT(fp, V32FP_R18) = 1;

	/*
	 * The two rate ladders.  Identical but for which field they read and
	 * which index they write; 9600 splits on `trellis` in both, and the
	 * fall-through is index 0 -- 4800, which `RATEv32` settles (F8640).
	 */
	rate = (unsigned short)p->tx_rate;
	if (rate == V32_BPS_14400)
		FIELD_S16(fp, V32FP_TX_RATE_INDEX) = V32_RATE_14400;
	else if (rate == V32_BPS_12000)
		FIELD_S16(fp, V32FP_TX_RATE_INDEX) = V32_RATE_12000;
	else if (rate == V32_BPS_9600)
		FIELD_S16(fp, V32FP_TX_RATE_INDEX) =
			(short)(V32_RATE_9600 - (p->trellis == 0));
	else if (rate == V32_BPS_7200)
		FIELD_S16(fp, V32FP_TX_RATE_INDEX) = V32_RATE_7200;
	else
		FIELD_S16(fp, V32FP_TX_RATE_INDEX) = V32_RATE_4800;

	rate = (unsigned short)p->rx_rate;
	if (rate == V32_BPS_14400)
		FIELD_S16(fp, V32FP_RX_RATE_INDEX) = V32_RATE_14400;
	else if (rate == V32_BPS_12000)
		FIELD_S16(fp, V32FP_RX_RATE_INDEX) = V32_RATE_12000;
	else if (rate == V32_BPS_9600)
		FIELD_S16(fp, V32FP_RX_RATE_INDEX) =
			(short)(V32_RATE_9600 - (p->trellis == 0));
	else if (rate == V32_BPS_7200)
		FIELD_S16(fp, V32FP_RX_RATE_INDEX) = V32_RATE_7200;
	else
		FIELD_S16(fp, V32FP_RX_RATE_INDEX) = V32_RATE_4800;

	/* Protocol 0, 1, or anything else -- three half-duplex modes. */
	hdx = (unsigned char *)HDX(modem);
	prot = (unsigned short)p->protocol;
	if (prot == V32_PROTOCOL_0)
		FIELD_S16(hdx, V32HDX_MODE) = 0;
	else if (prot == V32_PROTOCOL_1)
		FIELD_S16(hdx, V32HDX_MODE) = 1;
	else
		FIELD_S16(hdx, V32HDX_MODE) = 2;

	/*
	 * The two scrambler configurations are ONE local, patched twice: the
	 * group width and the second tap position are shared, and only the
	 * first tap position differs -- SDMv32_GPC for the transmitter,
	 * SDMv32_GPA for the receiver, both indexed by the half-duplex mode.
	 */
	sdmcfg[0] = SDMv32_CFG[0];
	sdmcfg[1] = SDMv32_CFG[1];
	sdmcfg[2] = SDMv32_CFG[2];
	sdmcfg[0] = (short)(FIELD_S16(fp, V32FP_TX_RATE_INDEX) != 0 ? 4 : 2);
	smccfg = SMCv32_CFG;
	sdmcfg[1] = SDMv32_GPC[FIELD_S16(hdx, V32HDX_MODE)];
	smccfg.mode = (short)(FIELD_S16(fp, V32FP_TX_RATE_INDEX) != 0);

	sdm = SDM_TX(fp);
	sdm->group = sdmcfg[0];
	FIELD_U16(sdm, V32_SDM_TAP1_POS) = (unsigned short)sdmcfg[1];
	sdm->reg = 0;
	FIELD_U16(sdm, V32_SDM_TAP2_POS) = (unsigned short)sdmcfg[2];
	shift = sdm->group;
	sdm->tap1 = (short)(FIELD_U16(sdm, V32_SDM_TAP1_POS) - shift);
	sdm->tap2 = (short)(FIELD_U16(sdm, V32_SDM_TAP2_POS) - shift);
	mask = (unsigned int)((1 << shift) - 1);
	sdm->outmask = mask;
	sdm->regmask = ~mask;

	/* The transmit symbol coder, from the four-byte template. */
	SMC_TX(fp)->mode = smccfg.mode;
	SMC_TX(fp)->pad02 = smccfg.pad02;
	for (i = 0; (short)i <= 1; i++)
		SMC_TX(fp)->state[i] = 0;
	SMC_TX(fp)->quad = 0;
	SMC_TX(fp)->f10 = 0;
	SMC_TX(fp)->f0e = 0;
	SMC_TX(fp)->pad12 = 0;
	SMC_TX(fp)->shift = (short)(2 * (SMC_TX(fp)->mode != 0));

	/* The pulse shaper. */
	ppscfg = PPSv32_CFG;
	ppscfg.aux = (void *)(long)p->r24;
	FPM_PPS_init(PPS(FP(modem)), &ppscfg, fresh);

	/* The resampler; the descrambler's tap comes out of the same load. */
	mrfcfg = MRFv32_CFG;
	sdmcfg[1] = SDMv32_GPA[FIELD_S16(HDX(modem), V32HDX_MODE)];
	mrfcfg.aux = (void *)(long)p->r24;
	FPM_MRF_init(MRF(FP(modem)), &mrfcfg, fresh);

	/*
	 * The echo canceller's two delays are INPUTS -- fpm_ecc.h records that
	 * init reads them and never writes them, so they are set here, before
	 * the call, and the far one is a bare 48.
	 */
	delay = ((int)(short)p->ec_near_delay * V32_DELAY_SCALE + 0x8000) >> 15;
	if (delay < 0)
		delay = 0;
	ECC(fp)->far_delay = 48;
	ECC(fp)->near_delay = (short)delay;

	ecccfg = ECCv32_CFG;
	ecccfg.aux = (void *)(long)p->r24;
	FPM_ECC_init(ECC(FP(modem)), &ecccfg, fresh);

	fp = (unsigned char *)FP(modem);
	ECC(fp)->line_len = 48;
	ECC(fp)->near_rd = 0;
	ECC(fp)->far_rd = 0;

	FPM_AGC_init((struct fpm_agc *)(void *)FIELD(FP(modem), V32FP_AGC),
		     &AGCv32_CFG, fresh);

	/* The timing recovery. */
	srecfg = SREv32_CFG;
	/*
	 * fpm_sre_cfg + 0x34, spelled `pad34`/`pad36`; see the head of file.
	 * `memcpy` and not a cast so that the one 32-bit store the object makes
	 * survives without a strict-aliasing pun.
	 */
	memcpy(&srecfg.pad34, &p->r24, sizeof p->r24);
	FPM_SRE_init(SRE(FP(modem)), &srecfg, fresh);

	/*
	 * The four ppm-meter parameters `FPM_SRE_init` deliberately leaves
	 * alone.  fpm_sre.h: "FOUR OF THESE ARE NEVER WRITTEN BY init --
	 * ppm_step, ppm_scale, ppm_period and ppm_n_max are read-only to both
	 * functions, so a caller has to fill them".  This is that caller, and
	 * it fills exactly those four.
	 */
	fp = (unsigned char *)FP(modem);
	SRE(fp)->ppm_step = 12;
	SRE(fp)->ppm_period = 9600;
	SRE(fp)->ppm_n_max = 0x68;
	SRE(fp)->ppm_scale =
		(short)(1000000 / ((int)srecfg.clock_len * 9600));

	/* The equaliser.  Its `owner` is the decoder, which follows it. */
	fsecfg = FSEv32_CFG;
	fsecfg.decision = FSE_decision_AB;
	fsecfg.reserved34 = (void *)(long)p->r24;
	fsecfg.owner = (void *)FIELD(FP(modem), V32FP_DEC);
	FPM_FSE_init(FSE(FP(modem)), &fsecfg, fresh);

	/* The decoder, cleared field by field, and its Viterbi state. */
	fp = (unsigned char *)FP(modem);
	sel = FIELD_S16(fp, V32FP_RX_RATE_INDEX);
	DEC(fp)->scram = 0;
	DEC(fp)->count = 0;
	DEC(fp)->ang_prev = 0;
	DEC(fp)->eqm = 0;
	DEC(fp)->eqm_b = 0;
	DEC(fp)->rate_change = 0;
	DEC(fp)->retrain = 0;
	*(int *)(void *)DEC(fp)->pad68 = 0;
	FIELD_S16(DEC(fp), V32_DEC_RETRAIN_N) = 0;
	for (i = 0; (short)i <= 5; i++)
		FIELD_S16(DEC(fp), 0x52 + 2 * i) = 0;
	for (i = 0; (short)i <= 1; i++)
		DEC(fp)->prev_sym[i] = 0;
	DEC(fp)->chan = 0;
	FIELD_S16(DEC(fp), V32_DEC_SHORT_04) = 0;
	FIELD_S16(DEC(fp), 0x06) = 0;
	VTBv32_init((struct vtb *)(void *)DEC(fp)->vtb, sel, fresh);

	/* The receive symbol coder, from the same template as the transmit. */
	SMC_RX(fp)->mode = smccfg.mode;
	SMC_RX(fp)->pad02 = smccfg.pad02;
	for (i = 0; (short)i <= 1; i++)
		SMC_RX(fp)->state[i] = 0;
	SMC_RX(fp)->quad = 0;
	SMC_RX(fp)->f10 = 0;
	SMC_RX(fp)->f0e = 0;
	SMC_RX(fp)->shift = (short)(2 * (SMC_RX(fp)->mode != 0));
	SMC_RX(fp)->pad12 = 0;

	/* The descrambler. */
	sdm = SDM_RX(fp);
	sdm->reg = 0;
	sdm->group = sdmcfg[0];
	FIELD_U16(sdm, V32_SDM_TAP1_POS) = (unsigned short)sdmcfg[1];
	FIELD_U16(sdm, V32_SDM_TAP2_POS) = (unsigned short)sdmcfg[2];
	shift = sdm->group;
	sdm->tap1 = (short)(FIELD_U16(sdm, V32_SDM_TAP1_POS) - shift);
	sdm->tap2 = (short)(FIELD_U16(sdm, V32_SDM_TAP2_POS) - shift);
	mask = (unsigned int)((1 << shift) - 1);
	sdm->outmask = mask;
	sdm->regmask = ~mask;

	/*
	 * The symbol ring, the three encoders and the constellation.
	 *
	 * THE RING'S BUFFER IS THE ECHO CANCELLER'S DELAY LINE -- one
	 * allocation serving both, which is why `line_len` and `limit` are set
	 * from the same table entry two statements later.
	 */
	RING(fp)->pad0e = 0;
	RING(fp)->widx = 0;
	RING(fp)->buf = ECC(fp)->line;
	((v32_encoder_fn *)(void *)FIELD(fp, V32FP_ENCODERS))[0] =
		(v32_encoder_fn)SMCv32_encoder_dif;
	((v32_encoder_fn *)(void *)FIELD(fp, V32FP_ENCODERS))[1] =
		(v32_encoder_fn)SMCv32_encoder_abs;
	((v32_encoder_fn *)(void *)FIELD(fp, V32FP_ENCODERS))[2] =
		(v32_encoder_fn)SMCv32_encoder_tcm;
	FIELD_S16(fp, V32FP_ENCODER_SEL) = 0;
	FIELD_PTR(fp, V32FP_IMAP) = (void *)SMCv32_IMAP16;
	FIELD_PTR(fp, V32FP_QMAP) = (void *)SMCv32_QMAP16;
	symlen = V32_SYMBOL_LEN[p->symlen_sel];
	RING(fp)->limit = symlen;
	ECC(fp)->line_len = symlen;

	/*
	 * The three tone detectors.  One configuration, patched between the
	 * calls: 2100 Hz, then 600 or 1800 by the half-duplex mode, then
	 * 3000 Hz.  `fresh` clears the slot first so that FPM_TONE_create
	 * allocates rather than re-using.
	 */
	tonecfg = FPM_TONE_CFG;
	tonecfg.freq = 2100;
	tonecfg.scale = 0x16a1;
	tonecfg.rev_period = 0;
	tonecfg.ratio = 0x747a;
	tonecfg.f08 = 0x27ae;
	tonecfg.min_level = 1;
	tonecfg.damp = 0x7c00;
	/* fpm_tone_cfg + 0x18, spelled inside `r16[3]`; see head of file. */
	memcpy(&tonecfg.r16[1], &p->r24, sizeof p->r24);
	tonecfg.rev_thresh = 0;
	tonecfg.rev_lag = 40;

	if (fresh)
		FIELD_PTR(HDX(modem), V32_HDX_TONE0) = 0;
	hdx = (unsigned char *)HDX(modem);
	FIELD_PTR(hdx, V32_HDX_TONE0) = FPM_TONE_create(
		(struct fpm_tone *)FIELD_PTR(HDX(modem), V32_HDX_TONE0),
		&tonecfg);

	hdx = (unsigned char *)HDX(modem);
	tonecfg.freq = (short)(FIELD_S16(hdx, V32HDX_MODE) != 0 ? 600 : 1800);
	if (fresh)
		FIELD_PTR(hdx, V32_HDX_TONE1) = 0;
	FIELD_PTR(hdx, V32_HDX_TONE1) = FPM_TONE_create(
		(struct fpm_tone *)FIELD_PTR(HDX(modem), V32_HDX_TONE1),
		&tonecfg);

	tonecfg.freq = 3000;
	if (fresh)
		FIELD_PTR(HDX(modem), V32_HDX_TONE2) = 0;
	hdx = (unsigned char *)HDX(modem);
	FIELD_PTR(hdx, V32_HDX_TONE2) = FPM_TONE_create(
		(struct fpm_tone *)FIELD_PTR(HDX(modem), V32_HDX_TONE2),
		&tonecfg);

	/*
	 * The half-duplex machine's opening state.  The three length tables
	 * are indexed by +0x16 here and by +0x18 in `V32FP_control`; they are
	 * two different selectors and v32fp.h says why.
	 */
	hdx = (unsigned char *)HDX(modem);
	timeout = (short)((p->timeout * V32_TIMEOUT_SCALE) >> 13);
	FIELD_S16(hdx, V32HDX_STATE) = 0;
	FIELD_INT(hdx, V32HDX_ELAPSED) = 0;
	FIELD_INT(hdx, V32HDX_TIMEOUT_MAX) = timeout;
	FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxTone;
	FIELD_S16(hdx, V32HDX_SYMBOL_LEN2) = V32_SYMBOL_LEN[p->r16];
	FIELD_S16(hdx, V32HDX_SYMBOL_LEN) = V32_SYMBOL_LEN[p->r16];
	FIELD_S16(hdx, V32HDX_SAMPLE_LEN) = V32_SAMPLE_LEN[p->r16];
	FIELD_S16(hdx, V32_HDX_SHORT_94) = V32_TURNAROUND_DLY[p->r16];
	FIELD_S16(hdx, V32_HDX_SHORT_9A) = 0;
	FIELD_S16(hdx, V32_HDX_SHORT_98) =
		(short)(V32_SYMBOL_LEN[p->r16] + 5);
	FIELD_S16(hdx, V32_HDX_SHORT_9A) =
		(short)((tonecfg.rev_lag * V32_DELAY_SCALE + 0x4000) >> 15);
	FIELD_S16(hdx, V32_HDX_SHORT_9C) =
		(short)(((int)(short)p->ec_near_delay * V32_DELAY_SCALE
			 + 0x4000) >> 15);
	if (FIELD_S16(hdx, V32HDX_MODE) != 0) {
		FIELD_PTR(hdx, V32HDX_TXSTATE) = (void *)TxHdxTone;
		FIELD_INT(hdx, V32HDX_TIMEOUT) = 0x1ef0;
	} else {
		FIELD_PTR(hdx, V32HDX_TXSTATE) = (void *)TxHdxNull;
		FIELD_INT(hdx, V32HDX_TIMEOUT) = timeout;
	}
	FIELD_S16(hdx, V32HDX_SHORT_AE) = 0;
	FIELD_S16(hdx, V32HDX_SHORT_AC) = 0;

	/* The five control-surface calls, in the object's order. */
	SetAdaptEcV32(modem, V32_ADAPTEC_OFF);
	SetAdaptEqV32(modem, V32_ADAPTEQ_OFF);
	SetRxLoopsV32(modem, 1);
	SetTxModeV32(modem, V32_MODE_ABS4);
	SetRxModeV32(modem, V32_MODE_DIF4);

	/*
	 * The TRN generator's shift is the DESCRAMBLER'S first tap, reached
	 * through the equaliser's `owner` rather than through fp + 0x5020 --
	 * the object loads fp + 0x230 here and the two are the same address.
	 */
	fp = (unsigned char *)FP(modem);
	((struct v32_dec *)FSE(fp)->cfg.owner)->scram_tap =
		(short)(unsigned short)SDM_RX(fp)->tap1;

	/* The scratch register bank, and the multi-tone detector. */
	hdx = (unsigned char *)HDX(modem);
	for (i = 0; i < V32HDX_REGS_CLEARED; i++)
		FIELD_S16(hdx, V32HDX_REGS + 2 * i) = 0;
	FIELD_INT(hdx, V32HDX_INT_88) = 0;

	/*
	 * FOUR FIELDS OF FIVE.  `mtdcfg.f0a` is left as whatever the stack
	 * held -- the object writes +0x00, +0x04, +0x06 and +0x08 and stops,
	 * and `FPM_MTD_create` copies all five into the object.  A C89
	 * initialiser would have zero-filled the fifth and the object emits no
	 * such store, so this is four assignments to an uninitialised local
	 * and the garbage is reproduced rather than tidied.  Deviation D960.
	 */
	mtdcfg.coeff = V32_S_DATA_COEF;
	mtdcfg.tones = 3;
	mtdcfg.ratio = 0x747a;
	mtdcfg.min_level = 1;
	FIELD_PTR(hdx, V32_HDX_MTD) = FPM_MTD_create(
		(struct fpm_mtd *)FIELD_PTR(HDX(modem), V32_HDX_MTD),
		&mtdcfg);

	FPM_AGC_init((struct fpm_agc *)HDX(modem), &AGCv32Prc_CFG, fresh);

	fp = (unsigned char *)FP(modem);
	FIELD_U16(fp, V32FP_CLEAN_N) = 0;
	FIELD_S16(fp, 0x50d6) = 0;
	FIELD_S16(fp, 0x50d8) = 0;

	/* The instance's own status byte, its flag, and the window. */
	FIELD_INT(modem, V32_OBJ_STATUS) = 0;
	FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_BIT6;
	FIELD_U8(modem, V32_OBJ_STATUS) = 1;

	FIELD_PTR(modem, V32_OBJ_DIAG_OUT_I) = FSE(fp)->out_i;
	FIELD_PTR(modem, V32_OBJ_DIAG_OUT_Q) = FSE(fp)->out_q;
	FIELD_PTR(modem, V32_OBJ_DIAG_ICOEFF) = FSE(fp)->icoeff;
	FIELD_PTR(modem, V32_OBJ_DIAG_QCOEFF) = FSE(fp)->qcoeff;
	near_n = ECC(fp)->cfg.near_taps;
	far_n = ECC(fp)->cfg.far_taps;
	coef = ECC(fp)->coef[0];
	FIELD_PTR(modem, V32_OBJ_DIAG_NEAR_I) = coef;
	FIELD_PTR(modem, V32_OBJ_DIAG_NEAR_Q) = coef + near_n;
	FIELD_S16(modem, V32_OBJ_DIAG_FSE_TAPS) = 0x31;
	FIELD_PTR(modem, V32_OBJ_DIAG_FAR_I) = coef + 2 * near_n;
	FIELD_PTR(modem, V32_OBJ_DIAG_N_OUT) = &FSE(fp)->n_out;
	FIELD_U16(modem, V32_OBJ_DIAG_NEAR_N) =
		(unsigned short)ECC(fp)->cfg.near_taps;
	FIELD_PTR(modem, V32_OBJ_DIAG_FAR_Q) = coef + 2 * near_n + far_n;
	FIELD_U16(modem, V32_OBJ_DIAG_FAR_N) =
		(unsigned short)ECC(fp)->cfg.far_taps;

	/*
	 * The two protocol arms.  Both are skipped when `options` bit 10 is
	 * set, and the object tests that bit SEPARATELY in each -- two `if`s
	 * and not one, which is what its two `testb $0x4,0x11(%ebp)` say.
	 */
	if ((unsigned short)p->protocol == V32_PROTOCOL_0
	    && !(p->options & V32_OPT_BIT10)) {
		hdx = (unsigned char *)HDX(modem);
		FIELD_S16(hdx, V32HDX_STATE) = 2;
		InitGenSequence(modem, 0, 4, 2);
		((struct fpm_tone *)FIELD_PTR(HDX(modem),
					      V32_HDX_TONE0))->cfg.f08 = 0x4000;
		SetToneDetect(modem, 600);
		hdx = (unsigned char *)HDX(modem);
		FIELD_INT(hdx, V32HDX_INT_90) = 0;
		FIELD_PTR(hdx, V32HDX_TXSTATE) = (void *)TxHdxCarrierState;
		FIELD_PTR(hdx, V32HDX_RXSTATE) = (void *)RxHdxPhsReversal;
		FIELD_INT(hdx, V32HDX_TIMEOUT) =
			FIELD_INT(hdx, V32HDX_TIMEOUT_MAX);
		FIELD_INT(hdx, V32HDX_ELAPSED) = 0;
	}
	if ((unsigned short)p->protocol == V32_PROTOCOL_1
	    && !(p->options & V32_OPT_BIT10)) {
		hdx = (unsigned char *)HDX(modem);
		FIELD_S16(hdx, V32HDX_STATE) = 1;
		/*
		 * The object computes both arms of this even though the `if`
		 * above has already settled the bit -- `sbb`/`and $-40`/`add
		 * $180`, which is 180 when the bit is set and 140 when it is
		 * not.  Written as the conditional it encodes rather than as
		 * the 140 it can only produce.
		 */
		FIELD_INT(hdx, V32HDX_TIMEOUT) =
			((p->options >> 10) & 1) ? 180 : 140;
		SetToneDetect(modem, 0);
	}

	/*
	 * Two separately gated debug lines, which is two `cmpl $0x1` in the
	 * object and not one.  The date and time are the ORIGINAL's __DATE__
	 * and __TIME__ as literals, following src/dsp/fpm_agc.c: reproducing
	 * the macros would stamp this build instead.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V32FP version: %s %s\n", "Sep 22 2005", "15:48:07");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"V32FP Config: protocol=%d,tx_rate=%d,rx_rate=%d,"
			"timeout=%d,energy_drop_time=%d,tx_scale=%d," "options=0x%x,trellis=%d\n",
			param->protocol, param->tx_rate, param->rx_rate,
			param->timeout, param->energy_drop_time,
			param->tx_scale, param->options, param->trellis);

	return modem;
}
void *
V32FP_create(const struct v32fp_cfg *cfg, void *arg1)
{
	struct v32fp_params params;

	params = V32_CFG;

	params.protocol = (short)(cfg->protocol != 0);
	params.tx_rate = (short)cfg->rate;
	params.rx_rate = (short)cfg->rate;
	params.timeout = cfg->timeout;
	params.options = (params.options & ~0x400u)
		| (unsigned int)((cfg->r10 & 1) << 10);
	params.ec_near_delay = (unsigned short)cfg->phys_delay;
	params.trellis = (cfg->rate > 0x1c1f);
	params.energy_drop_time = (short)cfg->energy_drop_time;

	return V32FP_recreate(0, &params, arg1);
}

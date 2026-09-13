/*
 * v22fp.c -- V.22 / V.22bis: the datapump object's constructor, destructor
 * and per-block entry point.
 *
 *   V22FP_create   .text 0x087990  2,449 bytes
 *   V22FP_delete   .text 0x088330    332 bytes
 *   V22FP_modem    .text 0x0887b0    346 bytes
 *   V22_PROTOCOL   .rodata 0x008544   28 bytes, LOCAL
 *
 * See include/dsplib/v22fp.h for the layout and where each field's name comes
 * from.  What is worth saying beside the code:
 *
 * THE SECOND ARGUMENT TO THE FOUR `*_free` CALLS IS DROPPED.  `V22FP_delete`
 * pushes a literal 1 as a second argument to `V22_PPS_free`, `V22_MRF_free`,
 * `V22_SRE_free` and `V22_FSE_free`, four times over with a fresh `mov $0x1`
 * each time, so the author declared those four with a second parameter.  None
 * of the four reads it -- each touches only `0x4(%esp)` or `0x10(%esp)`, which
 * is the first -- and this tree reconstructed all four from their own bodies
 * with one parameter apiece.  Adding a second would mean changing four
 * headers, one of which (v22_fse.h) another effort owns.  cdecl is
 * caller-cleaned and the callee never looks, so nothing observable turns on
 * it; recorded here so that a later reader finding the push is not left
 * thinking an argument went missing.
 *
 * CREATE DOES NOT CHECK ANY ALLOCATION.  Eleven `sysdep_malloc` calls, no
 * NULL test on any of them, and the very next instruction after the first
 * dereferences it.  `B103FP_create` in the same object does check.  Faithful,
 * and it is why `V22FP_create` cannot be handed a zeroed buffer either: the
 * `fp != NULL` path reads `fp->dsp` immediately.
 *
 * THE THROWAWAY OSCILLATOR.  Three of the object's coefficient sets are not
 * stored tables at all -- they are a stored PROTOTYPE multiplied sample by
 * sample by a carrier that create synthesises with an `fpm_tone` object built
 * for the purpose, retuned three times, and deleted before create returns.
 * That is why `FPM_TONE_generate2` appears in a constructor.
 */

#include <string.h>

#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sysdep.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22_iir.h"
#include "dsplib/v22_mrf.h"
#include "dsplib/v22_pps.h"
#include "dsplib/v22_sre.h"
#include "dsplib/v22ans.h"
#include "dsplib/v22dec.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22hdx.h"
#include "dsplib/v22loop.h"
#include "dsplib/v22org.h"
#include "dsplib/v22prc.h"
#include "dsplib/v22tab.h"
#include "dsplib/v22txtab.h"

/*
 * FILE-LOCAL IN THE OBJECT.  `nm ref/slmodemd/dsplibs.o` gives all three a
 * lower-case `d`/`r`, and the reference's FILE structure puts them in a
 * data-only unit; the three have exactly one consumer here (this file), so
 * they live here and are `static`, which is also what the note the tables
 * carried on the reference side always said.  Tests reach them through the
 * globalized test copy (tools/testvisible.py).
 */

/*
 * Eight rising thresholds, in .data rather than .rodata -- so the original
 * declared them without `const` -- and nothing in the object writes them.
 */
static short V22DiconnectThreshTable[V22_DISCONNECT_THRESHOLDS] = {
	75, 95, 119, 150, 168, 174, 212, 238,
};

/*
 * The tone configurations.  Byte-identical to each other, and NOT because
 * one is derived from the other: both are file-static in the original and
 * the author wrote the same 36 bytes twice under two names.
 *
 * `src` is NULL in both, which would have `FPM_TONE_create` read 53 words
 * from address zero.  `V22FP_create` is what closes that: it copies one of
 * these to the stack and assigns `FPM_TONE_CFG.src` -- the library's
 * shared 53-tap prototype -- into the copy before creating anything.  That
 * assignment is what types these as `struct fpm_tone_cfg`; it reads the
 * dword at the library config's +0x10, and the only thing there is `src`.
 *
 * `rev_thresh` and `rev_lag` HOLD 40 AND 0 HERE, not the library config's
 * 16384 and 40, and that is the object's -- `.rodata:0x84e0` words 14 and 15,
 * dumped and checked.  A lag of zero means `FPM_TONE_find_rev` would divide
 * its history modulo zero, so these two copies are not configured for the
 * reversal detector and nothing in V.22 calls it.  The pair was named from the
 * arithmetic in that function (finding F8171) and renamed here in F8321; the
 * values are carried unexplained because they are the object's and no reader
 * of them exists on this path.
 */
static const struct fpm_tone_cfg TONEv22_CFG = {
	2100, 11587, 0, 2981,		/* freq, scale, rev_period, ratio    */
	328, 1, 31457, 0,		/* f08, min_level, damp, pad0e       */
	0,				/* src -- patched by V22FP_create    */
	53, { 0, 0, 0 },		/* len, r16                          */
	40, 0,				/* rev_thresh, rev_lag -- see below  */
	0, 0				/* extra, pad22                      */
};

static const struct fpm_tone_cfg TONEv22INIT_CFG = {
	2100, 11587, 0, 2981,
	328, 1, 31457, 0,
	0,
	53, { 0, 0, 0 },
	40, 0,
	0, 0
};

/*
 * Build a V.22 datapump.
 *
 * Four movements: settle the parameter block, allocate (or not), configure
 * every DSP sub-object, and publish the handful of pointers a caller reads.
 * The coefficient loops in the middle are the interesting part -- see the
 * note on the throwaway oscillator at the top of this file.
 */
struct v22fp *
V22FP_create(struct v22fp *fp, const struct v22fp_cfg *cfg)
{
	struct v22fp_params p;
	struct fpm_tone_cfg tone;
	struct fpm_mtd_cfg mtd;
	struct fpm_sdm_cfg sdm;
	struct v22_pps_cfg pps;
	struct v22_mrf_cfg mrf;
	struct v22_fse_cfg fse;
	struct fpm_tone *gen;
	struct v22fp_hdx *hdx;
	struct v22fp_dsp *dsp;
	short cosine;
	short sine;
	int fresh = 0;
	int i;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("V22FP create, built %s %s\n",
				     "Sep 22 2005", "15:48:09");

	/*
	 * The template, then six patches.  Anything the caller asks for that
	 * is not in range leaves the template's value alone -- there is no
	 * range check and no default arm, which is why `mode` 5 produces an
	 * object identical to `mode` 1 rather than to `mode` 0.
	 */
	p = V22_CFG;

	switch (cfg->mode) {
	case 0:
		p.mode = 0;
		break;
	case 1:
		p.mode = 1;
		break;
	case 2:
		p.mode = 2;
		p.r14 = 1;
		break;
	default:
		break;
	}

	switch (cfg->rate) {
	case 0:
		p.bps = 2400;
		break;
	case 1:
	case 2:
		p.bps = 1200;
		break;
	default:
		break;
	}
	p.bps2 = p.bps;

	p.r08 = cfg->f08;
	/*
	 * Three bits, and only bit 0 of each source word.  A caller passing 2
	 * selects nothing -- the same shape as `SetAdaptEqV22`'s 16-bit mode.
	 * The object clears bits 10 and 11 together and bit 9 separately,
	 * which is visible only in the mask widths; bits 4 and 6 of the
	 * template survive and are read by nothing.
	 */
	p.flags = (p.flags & ~0x0e00u)
		| (unsigned int)((cfg->f0c & 1) << 10)
		| (unsigned int)((cfg->f14 & 1) << 11)
		| (unsigned int)((cfg->f18 & 1) << 9);
	p.carrier_loss_ms = (short)cfg->f10;

	if (fp == NULL) {
		/*
		 * Eleven allocations and not one of them checked, exactly as
		 * the object does it.  `fresh` is what the six `*_init` calls
		 * below use to decide whether to allocate their own buffers,
		 * so a re-initialisation reuses everything.
		 */
		fp = sysdep_malloc(sizeof(*fp));
		fp->dsp = sysdep_malloc(sizeof(*fp->dsp));
		fp->hdx = sysdep_malloc(sizeof(*fp->hdx));

		hdx = fp->hdx;
		hdx->tone = NULL;
		hdx->mtd = NULL;
		hdx->mtd_s1 = NULL;
		hdx->mtd2 = NULL;
		hdx->iir = sysdep_malloc(0x20);

		dsp = fp->dsp;
		dsp->rx_scratch = sysdep_malloc(0x154);
		dsp->ra8 = sysdep_malloc(0x18);
		dsp->pps_coff_i = sysdep_malloc(V22_PPS_COEFFS * 2);
		dsp->pps_coff_q = sysdep_malloc(V22_PPS_COEFFS * 2);
		dsp->mrf_coeff = sysdep_malloc(V22_MRF_COEFFS * 2);
		dsp->fse_coff_i = sysdep_malloc(V22_FSE_TAPS * 2);
		dsp->fse_coff_q = sysdep_malloc(V22_FSE_TAPS * 2);

		fresh = 1;
	}

	/* The parameter block IS the object's first 28 bytes. */
	fp->params = p;
	fp->params.disconnect_thresh = V22DiconnectThreshTable[3];

	hdx = fp->hdx;
	dsp = fp->dsp;

	dsp->r00 = 1;
	dsp->r04 = 1;
	dsp->r08 = 1;
	dsp->r0c = 1;
	dsp->eq_adapt = 1;
	dsp->r14 = 0;
	dsp->scrambler_on = (int)(fp->params.flags & 1);
	dsp->descrambler_on = (int)((fp->params.flags >> 1) & 1);
	dsp->r20 = (int)((fp->params.flags >> 2) & 1);

	dsp->r28 = (short)(fp->params.bps != 1200);
	dsp->r2a = (short)(fp->params.bps2 != 1200);

	switch (fp->params.mode) {
	case 0:
		hdx->protocol = 1;
		dsp->r2c = 1;
		dsp->r2e = 2;
		break;
	case 1:
		hdx->protocol = 2;
		dsp->r2c = 2;
		dsp->r2e = 1;
		break;
	default:
		hdx->protocol = 3;
		dsp->r2c = (short)fp->params.r14;
		dsp->r2e = (short)fp->params.r14;
		break;
	}
	/*
	 * And bit 11 overrides all three.  It is the one caller-supplied flag
	 * with a second, visible consequence, which is what pins it to this
	 * bit rather than a neighbour.
	 */
	if (fp->params.flags & 0x800)
		hdx->protocol = 0;

	hdx->gtimer = 0;
	hdx->r08 = 0;
	hdx->r0a = 0;
	hdx->connect_substate = 0;
	hdx->r28 = 0;
	hdx->r30 = 0x2454;
	hdx->r32 = 0;
	hdx->rx_shift = 0;
	hdx->node_deadline = fp->params.r08;
	hdx->trained = 0;
	hdx->r2c = 0;
	hdx->carrier_loss_blocks = 0;

	/*
	 * The four sub-objects.  Each `create` is handed the existing pointer,
	 * so on a re-initialisation they are reconfigured rather than rebuilt.
	 *
	 * `TONEv22_CFG` and `TONEv22INIT_CFG` are byte-identical, so which of
	 * the two feeds `hdx->tone` and which feeds the throwaway generator
	 * below is NOT decidable by any test.  Written as the object writes
	 * it -- two different symbols -- and recorded here so the choice is
	 * not mistaken for a measurement.
	 */
	tone = TONEv22_CFG;
	tone.src = FPM_TONE_CFG.src;
	hdx->tone = FPM_TONE_create(hdx->tone, &tone);

	/*
	 * One of the three MTD configurations is copied to the stack and the
	 * other two are passed straight from .rodata.  Nothing turns on it --
	 * `FPM_MTD_create` copies twelve bytes out of whichever it is given --
	 * but the copy is in the object and is reproduced.
	 */
	mtd = MTDs1_CFG;
	hdx->mtd_s1 = FPM_MTD_create(hdx->mtd_s1, &mtd);
	hdx->mtd = FPM_MTD_create(hdx->mtd, &MTDv22_CFG);
	hdx->mtd2 = FPM_MTD_create(hdx->mtd2, &MTDv22_CFG2);

	tone = TONEv22INIT_CFG;
	tone.src = FPM_TONE_CFG.src;
	gen = FPM_TONE_create(NULL, &tone);

	/* Two bits per symbol at 1200, four at 2400. */
	sdm = SDMv22_CFG;
	sdm.nbits = (short)(dsp->r28 != 0 ? 4 : 2);
	FPM_SDM_init(&dsp->sdm, &sdm);

	FPM_SMC_init(&dsp->smc, &SMCv22_CFG);

	/*
	 * The pulse shaper's kernel: the stored prototype multiplied by a
	 * quadrature carrier, one sample of it per tap.  Q14 throughout.
	 */
	gen->inc = (unsigned short)(dsp->r2c == 1 ? 0x666 : 0xccc);
	gen->cfg.scale = 0x4000;
	for (i = 0; i < V22_PPS_COEFFS; i++) {
		FPM_TONE_generate2(gen, &cosine, &sine, 1);
		dsp->pps_coff_i[i] =
			(short)((PPSv22_COFFS[i] * cosine) >> 14);
		dsp->pps_coff_q[i] =
			(short)((PPSv22_COFFS[i] * sine) >> 14);
	}

	pps = PPSv22_CFG;
	pps.coeff_i = dsp->pps_coff_i;
	pps.coeff_q = dsp->pps_coff_q;
	dsp->pps.imap = SMCv22_IMAP_1200BPS;
	dsp->pps.qmap = SMCv22_QMAP_1200BPS;
	V22_PPS_init(&dsp->pps, &pps, fresh);

	dsp->rac = 0;
	dsp->rae = 0;
	dsp->rb0 = 12;

	/*
	 * The channel filter, and the ONLY thing in this constructor that the
	 * mode gates: it runs for mode 0 and for nothing else.
	 */
	if (dsp->r2e == 2)
		V22IIRFilterInit(hdx->iir, IIR_b_coeff, IIR_a_coeff);

	/* The receive rate converter's kernel.  Cosine only, no quadrature. */
	gen->inc = 0x222;
	gen->phase = 0;
	for (i = 0; i < V22_MRF_COEFFS; i++) {
		FPM_TONE_generate2(gen, &cosine, &sine, 1);
		dsp->mrf_coeff[i] = (short)((MRFv22_COFFS[i] * cosine) >> 14);
	}

	/*
	 * `V22_MRF_init` PERMUTES the array it is given, in place, so this
	 * must be the freshly built copy and never `MRFv22_COFFS` itself --
	 * see v22_mrf.h.  That is also why the loop above regenerates it on
	 * every call rather than only when allocating.
	 */
	mrf = V22_MRF_CFG;
	mrf.coeff = dsp->mrf_coeff;
	V22_MRF_init(&dsp->mrf, &mrf, fresh);

	V22_SRE_init(&dsp->sre, fresh);

	FPM_AGC_init(&dsp->agc, &AGCv22_CFG, fresh);
	FPM_AGC_init(&dsp->agc2, &AGCv22_CFG2, fresh);

	/* And the equaliser's initial taps, quadrature again. */
	gen->inc = 0x2aaa;
	gen->phase = 0;
	fse = FSEv22_CFG;
	fse.icoff = dsp->fse_coff_i;
	fse.qcoff = dsp->fse_coff_q;
	for (i = 0; i < V22_FSE_TAPS; i++) {
		FPM_TONE_generate2(gen, &cosine, &sine, 1);
		dsp->fse_coff_i[i] =
			(short)((FSEv22_COFFS[i] * cosine) >> 14);
		dsp->fse_coff_q[i] =
			(short)((FSEv22_COFFS[i] * sine) >> 14);
	}

	/*
	 * Both written BEFORE init, and init leaves both alone.  `prev_quad`
	 * is the datapump's own two bytes; the slicer starts at the 1200 bit/s
	 * one whatever the rate, and `SetRxRate` is what moves it to
	 * `FSEv22_decision24`.
	 */
	dsp->fse.prev_quad = &dsp->prev_quad;
	dsp->fse.decision = FSEv22_decision12;
	V22_FSE_init(&dsp->fse, &fse, fresh);

	/* The descrambler, from the same configuration as the scrambler. */
	FPM_SDM_init(&dsp->sdm2, &sdm);

	FPM_TONE_delete(gen);

	/*
	 * The tail: five of the equaliser's fields lifted to the top of the
	 * object, and everything the datapump reports upward set to its
	 * starting value.  +0x1c is cleared as a word before two of its four
	 * bytes are set, so `r1e` is zero and is not left at the allocator's
	 * fill.
	 */
	fp->status = 0;
	fp->flags = 0;
	fp->r1e[0] = 0;
	fp->r1e[1] = 0;
	fp->flags |= 0x40;
	fp->status = 1;

	fp->out_i = dsp->fse.out_i;
	fp->out_q = dsp->fse.out_q;
	fp->n_out = &dsp->fse.n_out;
	fp->icoeff = dsp->fse.icoeff;
	fp->qcoeff = dsp->fse.qcoeff;

	fp->r34 = 0x31;
	fp->r38 = 0;
	fp->r3c = 0;
	fp->r40 = 0;
	fp->r44 = 0;
	fp->r48 = 0;
	fp->r4c = 0;

	return fp;
}

/*
 * Release the whole tree: the four embedded blocks' own buffers, the four
 * heap sub-objects, the seven raw buffers, then the two blocks and the object.
 *
 * The order is the object's, and it is not the reverse of create's.
 */
void
V22FP_delete(struct v22fp *fp)
{
	V22_PPS_free(&fp->dsp->pps);
	V22_MRF_free(&fp->dsp->mrf);
	V22_SRE_free(&fp->dsp->sre);
	V22_FSE_free(&fp->dsp->fse);

	FPM_TONE_delete(fp->hdx->tone);
	FPM_MTD_delete(fp->hdx->mtd);
	FPM_MTD_delete(fp->hdx->mtd_s1);
	FPM_MTD_delete(fp->hdx->mtd2);
	sysdep_free(fp->hdx->iir);

	sysdep_free(fp->dsp->ra8);
	sysdep_free(fp->dsp->pps_coff_i);
	sysdep_free(fp->dsp->pps_coff_q);
	sysdep_free(fp->dsp->mrf_coeff);
	sysdep_free(fp->dsp->fse_coff_i);
	sysdep_free(fp->dsp->fse_coff_q);
	sysdep_free(fp->dsp->rx_scratch);

	sysdep_free(fp->hdx);
	sysdep_free(fp->dsp);
	sysdep_free(fp);
}

/*
 * V22FP_GetDiagnostics lives in `v22ctl.c`.  It was reconstructed twice, in
 * two waves, with identical bodies and identical signatures; the banner here
 * also had its size wrong (33 against the object's 0x15 = 21), which is what
 * `tools/bannercheck.py` exists to catch.
 */

/*
 * ---------------------------------------------------------------------------
 * V22FP_modem -- .text 0x0887b0, 346 bytes.
 *
 * One block of the modulation, and the only caller of the seven-state
 * machine.  It is a marshalling layer with one piece of policy in it:
 *
 *   1. stage the caller's transmit words, int to short, into
 *      `tx_in_internal`;
 *   2. stage the caller's input samples, right-shifted by `hdx->rx_shift`, into
 *      `rx_in_internal`;
 *   3. dispatch through `V22_PROTOCOL[hdx->protocol]`;
 *   4. take the symbol count back, and -- THE POLICY -- move the machine to
 *      state 0 for five values of `fp->status`;
 *   5. copy `rx_out_internal` out to the caller's int array;
 *   6. scale exactly V22_TX_BLOCK transmit samples by `params.tx_gain` in Q15;
 *   7. report no symbols at all unless `fp->status` is zero.
 *
 * THE COUNTS CHANGE WIDTH ACROSS THE CALL.  The caller's are `int`; the
 * handlers' are `unsigned short` (finding F8534), and the two 16-bit slots
 * live side by side on this function's own stack.  Only the RECEIVE count is
 * written back -- the transmit count the handler leaves is dropped on the
 * floor, which is the object's and not an omission here.
 *
 * THE RETURN IS THE WHOLE 32-BIT WORD AT fp+0x1c, not `fp->status`: the
 * object emits `mov 0x1c(%ebp),%eax` where a byte field would force a
 * `movzbl`, and the same four bytes are read as a BYTE three times in the
 * lines above it.  `B103FP_modem` does exactly this, and this file follows
 * its spelling.  A draft argued that no test could separate the two readings
 * because `v22_process` only looks at the low byte; the test separated them
 * on the first call, 0 against 17664 (finding F8538).  Measure, do not argue.
 */

/*
 * The three staging buffers, with the original's own names, from .bss:0x3a0,
 * 0x480 and 0x560.  All three are file-static in the object, so two V.22
 * datapumps in one process share them -- reproduced as-is, exactly as
 * b103fp.c reproduces its pair.  The names COLLIDE across modules: `nm` finds
 * three `tx_in_internal` and three `rx_out_internal` in the 1.2 MB, so
 * `tools/symmap.py` can only alias the unique one, `rx_in_internal`.
 *
 * The element types are the handler signature's, which is what forces them:
 * the two the handler sees as symbols are `unsigned short` -- the copy-out
 * below is a `movzwl` -- and the sample buffer is `short`.
 */
#define V22FP_TX_IN_ENTRIES	100	/* .bss 0x3a0, 0xc8 bytes */
#define V22FP_RX_OUT_ENTRIES	100	/* .bss 0x480, 0xc8 bytes */
#define V22FP_RX_IN_ENTRIES	160	/* .bss 0x560, 0x140 bytes */

static unsigned short tx_in_internal[V22FP_TX_IN_ENTRIES];
static unsigned short rx_out_internal[V22FP_RX_OUT_ENTRIES];
static short rx_in_internal[V22FP_RX_IN_ENTRIES];

/*
 * The seven protocol states, in the object's own order -- `.rodata` + 0x8544,
 * 28 bytes, LOCAL (`nm` shows a lower-case `r`, so `static const`).  Each
 * entry carries a RELOCATION naming its handler, so the order below is read
 * off the object rather than inferred; `include/dsplib/v22status.h` records
 * the same seven against `V22_status`'s own parallel table.
 *
 * `hdx->protocol` indexes it, sign-extended and WITH NO BOUNDS CHECK.
 */
static void (* const V22_PROTOCOL[7])(struct v22fp *fp, unsigned short *txsym,
				      short *txout, short *rxin,
				      unsigned short *rxsym,
				      unsigned short *txcount,
				      unsigned short *rxcount) = {
	v22_data,		/* 0 */
	v22_originate,		/* 1 */
	v22_answer,		/* 2 */
	v22_local_loop,		/* 3 */
	v22_org_rmloop2,	/* 4 */
	v22_ans_rmloop2,	/* 5 */
	v22_retrain		/* 6 */
};

int
V22FP_modem(struct v22fp *fp, const int *tx_bits, short *tx_out,
	    const short *rx_in, int *rx_bits, int *n_tx, int *n_rx)
{
	/*
	 * The handler's own pair, sixteen bits wide and adjacent on the
	 * stack.  Both are seeded from the caller's before anything else
	 * happens, which matters: the second staging loop below reads
	 * `*n_rx` and the handler is handed the copy, not the original.
	 */
	unsigned short tx_syms = (unsigned short)*n_tx;
	unsigned short rx_syms = (unsigned short)*n_rx;
	int i;

	/*
	 * Three flag bits and one more in the next byte, cleared on entry to
	 * every block.  What they indicate is NOT established -- nothing
	 * reconstructed reads either byte -- so they stay masks with the
	 * instruction beside them rather than becoming names that would be
	 * believed.  `andb $0xf8,0x1d` and `andb $0xfd,0x1e`.
	 */
	fp->flags &= (unsigned char)~0x07;
	fp->r1e[0] &= (unsigned char)~0x02;

	/* Stage the transmit words, narrowing int to short. */
	for (i = 0; i < *n_tx; i++)
		tx_in_internal[i] = (unsigned short)tx_bits[i];

	/*
	 * Stage the input samples, right-shifted by the receive input shift.
	 * The load is `movswl` and the shift `sar`, so the arithmetic is
	 * signed; the shift count is `movzwl`, so `hdx->rx_shift` is read as an
	 * unsigned sixteen-bit field.
	 */
	for (i = 0; i < *n_rx; i++)
		rx_in_internal[i] =
			(short)((int)rx_in[i] >> fp->hdx->rx_shift);

	V22_PROTOCOL[fp->hdx->protocol](fp, tx_in_internal, tx_out, rx_in_internal,
				   rx_out_internal, &tx_syms, &rx_syms);

	*n_rx = rx_syms;

	/*
	 * THE STATE TRANSITION, which no handler carries: five status values
	 * put the machine back into state 0 -- `v22_data` -- with its
	 * sub-state cleared.  3 is V22_MSG_CONNECT_2400 and 4 is the 1200
	 * connect (finding F8538, from `v22_process`'s own jump table), so
	 * this reads as "any connect code enters the data state"; 6, 7 and 8
	 * are named by nothing and stay values.  Two separate range tests in
	 * the object, in this order, with the status byte re-read between
	 * them.
	 */
	if (fp->status == 3 || fp->status == 4) {
		fp->hdx->protocol = 0;
		fp->hdx->connect_substate = 0;
	}
	if (fp->status >= 6 && fp->status <= 8) {
		fp->hdx->protocol = 0;
		fp->hdx->connect_substate = 0;
	}

	/* Widen the recovered symbols back out to int. */
	for (i = 0; i < *n_rx; i++)
		rx_bits[i] = rx_out_internal[i];

	/*
	 * The transmit output gain, Q15, over exactly V22_TX_BLOCK samples --
	 * a literal 160 in the object and not `*n_tx` or a parameter, which is
	 * the same block length `TxNOP` emits.  `params.tx_gain` is 13014 in the
	 * template, which is 0.397.
	 */
	for (i = 0; i < V22_TX_BLOCK; i++)
		tx_out[i] = (short)(((int)tx_out[i] * fp->params.tx_gain) >> 15);

	/*
	 * Anything but status 0 reports NO received symbols, whatever the
	 * handler said -- and this happens AFTER the copy-out above, so the
	 * caller's array is still written and only the count is suppressed.
	 */
	if (fp->status != 0)
		*n_rx = 0;

	{
		int word;

		memcpy(&word, &fp->status, sizeof word);
		return word;
	}
}

/*
 * ---------------------------------------------------------------------------
 * The layout, held to the compiler.
 *
 * 32-BIT ONLY, and necessarily so: the reserved regions above are byte counts
 * measured from an object whose pointers are four bytes, so under any other
 * ABI the named fields land elsewhere and these are simply false rather than
 * violated.  Same argument as src/pump/b103/b103fp.c.
 *
 * The last block is the interesting one: it holds `v22prc.h`'s nine offset
 * constants -- written before this object was modelled, from nine other
 * functions' loads and stores -- against the struct.  Nine independent
 * addresses landing on nine plausible fields is the check that no amount of
 * differential testing of `create` alone could provide.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V22FP_ASSERT_OFF(tag, type, field, off) \
	typedef char v22fp_off_##tag[ \
		((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

V22FP_ASSERT_OFF(p_bps, struct v22fp_params, bps, 0x02);
V22FP_ASSERT_OFF(p_bps2, struct v22fp_params, bps2, 0x04);
V22FP_ASSERT_OFF(p_r08, struct v22fp_params, r08, 0x08);
V22FP_ASSERT_OFF(p_flags, struct v22fp_params, flags, 0x10);
V22FP_ASSERT_OFF(p_r14, struct v22fp_params, r14, 0x14);
V22FP_ASSERT_OFF(p_thresh, struct v22fp_params, disconnect_thresh, 0x16);
V22FP_ASSERT_OFF(p_r18, struct v22fp_params, carrier_loss_ms, 0x18);

V22FP_ASSERT_OFF(h_r04, struct v22fp_hdx, node_deadline, 0x04);
V22FP_ASSERT_OFF(h_r0e, struct v22fp_hdx, protocol, 0x0e);
V22FP_ASSERT_OFF(h_tone, struct v22fp_hdx, tone, 0x14);
V22FP_ASSERT_OFF(h_mtd, struct v22fp_hdx, mtd, 0x18);
V22FP_ASSERT_OFF(h_mtd_s1, struct v22fp_hdx, mtd_s1, 0x1c);
V22FP_ASSERT_OFF(h_mtd2, struct v22fp_hdx, mtd2, 0x20);
V22FP_ASSERT_OFF(h_iir, struct v22fp_hdx, iir, 0x24);
V22FP_ASSERT_OFF(h_r30, struct v22fp_hdx, r30, 0x30);
V22FP_ASSERT_OFF(h_r3c, struct v22fp_hdx, carrier_loss_blocks, 0x3c);

V22FP_ASSERT_OFF(d_r20, struct v22fp_dsp, r20, 0x20);
V22FP_ASSERT_OFF(d_r28, struct v22fp_dsp, r28, 0x28);
V22FP_ASSERT_OFF(d_r2e, struct v22fp_dsp, r2e, 0x2e);
V22FP_ASSERT_OFF(d_sdm, struct v22fp_dsp, sdm, 0x30);
V22FP_ASSERT_OFF(d_smc, struct v22fp_dsp, smc, 0x48);
V22FP_ASSERT_OFF(d_pps, struct v22fp_dsp, pps, 0x78);
V22FP_ASSERT_OFF(d_ra8, struct v22fp_dsp, ra8, 0xa8);
V22FP_ASSERT_OFF(d_rb0, struct v22fp_dsp, rb0, 0xb0);
V22FP_ASSERT_OFF(d_ppsi, struct v22fp_dsp, pps_coff_i, 0xb4);
V22FP_ASSERT_OFF(d_ppsq, struct v22fp_dsp, pps_coff_q, 0xb8);
V22FP_ASSERT_OFF(d_mrf, struct v22fp_dsp, mrf, 0xbc);
V22FP_ASSERT_OFF(d_agc, struct v22fp_dsp, agc, 0xd0);
V22FP_ASSERT_OFF(d_agc2, struct v22fp_dsp, agc2, 0xfc);
V22FP_ASSERT_OFF(d_sre, struct v22fp_dsp, sre, 0x128);
V22FP_ASSERT_OFF(d_fse, struct v22fp_dsp, fse, 0x164);
V22FP_ASSERT_OFF(d_quad, struct v22fp_dsp, prev_quad, 0x1c8);
V22FP_ASSERT_OFF(d_sdm2, struct v22fp_dsp, sdm2, 0x1cc);
V22FP_ASSERT_OFF(d_mrfc, struct v22fp_dsp, mrf_coeff, 0x1e4);
V22FP_ASSERT_OFF(d_fsei, struct v22fp_dsp, fse_coff_i, 0x1e8);
V22FP_ASSERT_OFF(d_fseq, struct v22fp_dsp, fse_coff_q, 0x1ec);
V22FP_ASSERT_OFF(d_scratch, struct v22fp_dsp, rx_scratch, 0x1f4);

V22FP_ASSERT_OFF(o_status, struct v22fp, status, 0x1c);
V22FP_ASSERT_OFF(o_flags, struct v22fp, flags, 0x1d);
V22FP_ASSERT_OFF(o_out_i, struct v22fp, out_i, 0x20);
V22FP_ASSERT_OFF(o_out_q, struct v22fp, out_q, 0x24);
V22FP_ASSERT_OFF(o_n_out, struct v22fp, n_out, 0x28);
V22FP_ASSERT_OFF(o_icoeff, struct v22fp, icoeff, 0x2c);
V22FP_ASSERT_OFF(o_qcoeff, struct v22fp, qcoeff, 0x30);
V22FP_ASSERT_OFF(o_r34, struct v22fp, r34, 0x34);
V22FP_ASSERT_OFF(o_r4c, struct v22fp, r4c, 0x4c);

typedef char v22fp_size[(sizeof(struct v22fp) == 0x5c) ? 1 : -1];
typedef char v22fp_hdx_size[(sizeof(struct v22fp_hdx) == 0x40) ? 1 : -1];
typedef char v22fp_dsp_size[(sizeof(struct v22fp_dsp) == 0x1f8) ? 1 : -1];
typedef char v22fp_params_size[(sizeof(struct v22fp_params) == 28) ? 1 : -1];
typedef char v22fp_cfg_size[(sizeof(struct v22fp_cfg) == 28) ? 1 : -1];

/*
 * v22prc.h's nine constants, resolved.  Each was derived from a load or a
 * store in a DIFFERENT function, none of which knew where the object's
 * sub-blocks began; that all nine land on a field is the corroboration.
 *
 * Three of them resolve INSIDE `struct v22_fse` and one inside `struct
 * v22_sre`, so the constants stay: retiring them would mean naming fields in
 * headers this task must not edit.  The mapping is recorded here instead.
 *
 *   V22_OBJ_GTIMER  0x50  -> struct v22fp::hdx, and hdx->gtimer beyond it
 *   V22_OBJ_FP      0x54  -> struct v22fp::dsp
 *   V22FP_EQ_ADAPT  0x10  -> dsp->eq_adapt      (create leaves 1)
 *   V22FP_TX_CLOCK  0x78  -> dsp->pps.cfg.step  (see the note below)
 *   V22FP_SIGNAL    0xec  -> dsp->agc.signal
 *   V22FP_BAUD     0x12a  -> dsp->sre.pll_acc   (see the note below)
 *   V22FP_CARRIER  0x130  -> dsp->sre.active
 *   V22FP_EQ_MODE  0x16c  -> dsp->fse.r08       (init 0)
 *   V22FP_EQ_EXTRA 0x180  -> dsp->fse.r1c       (init 1)
 *   V22FP_QUALITY  0x186  -> dsp->fse.r22       (init 0)
 *
 * TWO OF THE TEN ARE AN OPEN QUESTION AND ARE LEFT AS ONE.  `TxClockSync`
 * stores a short at the object's +0x54 +0x78, which is `v22_pps_cfg::step` --
 * a field `V22_PPS_init` copies in from the configuration and `V22_PPS_filter`
 * reads on every output.  `TxClockSync` therefore overwrites a configuration
 * word after init, which is either a deliberate rate correction or a soft
 * spot in one of the two readings.  `V22FP_BAUD` at +0x12a is
 * `v22_sre::pll_acc` and has the same shape.  Nothing here decides it, and
 * v22prc.h's names are NOT propagated inward on the strength of an offset
 * agreeing.
 */
typedef char v22fp_prc_gtimer[
	((int)__builtin_offsetof(struct v22fp, hdx) == V22_OBJ_GTIMER)
		? 1 : -1];
typedef char v22fp_prc_fp[
	((int)__builtin_offsetof(struct v22fp, dsp) == V22_OBJ_FP) ? 1 : -1];
typedef char v22fp_prc_adapt[
	((int)__builtin_offsetof(struct v22fp_dsp, eq_adapt)
		== V22FP_EQ_ADAPT) ? 1 : -1];
typedef char v22fp_prc_txclock[
	((int)__builtin_offsetof(struct v22fp_dsp, pps) == V22FP_TX_CLOCK)
		? 1 : -1];
typedef char v22fp_prc_signal[
	((int)__builtin_offsetof(struct v22fp_dsp, agc)
	 + (int)__builtin_offsetof(struct fpm_agc, signal)
		== V22FP_SIGNAL) ? 1 : -1];
typedef char v22fp_prc_baud[
	((int)__builtin_offsetof(struct v22fp_dsp, sre)
	 + (int)__builtin_offsetof(struct v22_sre, pll_acc)
		== V22FP_BAUD) ? 1 : -1];
typedef char v22fp_prc_carrier[
	((int)__builtin_offsetof(struct v22fp_dsp, sre)
	 + (int)__builtin_offsetof(struct v22_sre, active)
		== V22FP_CARRIER) ? 1 : -1];
/*
 * THE LAST THREE LAND ON FIELDS THAT WERE NAMED INDEPENDENTLY, AND THE
 * MEANINGS AGREE.  These assertions were written against `r08`, `r1c` and
 * `r22`, which is what `struct v22_fse` called those offsets when this file
 * was written; `V22_FSE_receive` has since named all three from its own
 * instructions, without reference to `v22prc.h` or to this file.  What the
 * three offsets resolve to is:
 *
 *   V22FP_EQ_MODE  0x16c -> fse.mu_sel   the LMS step-size selector
 *   V22FP_EQ_EXTRA 0x180 -> fse.lms_on   the gate on the tap update
 *   V22FP_QUALITY  0x186 -> fse.mse      the smoothed squared decision error
 *
 * and every one of them says the same thing the leaf function did.
 * `SetAdaptEqV22` mode 3 writes 1 to EQ_EXTRA -- it turns the equaliser's
 * adaptation ON.  Modes 2 and 3 write 0 and 1 to EQ_MODE -- they pick the
 * step size.  `GetSignalQuality` returns QUALITY -- and the mean squared
 * error IS the signal quality.  Three names derived from what a caller does
 * with a field, three derived from what the receive loop does with it,
 * agreeing on all three.  Finding F3510.
 */
typedef char v22fp_prc_eqmode[
	((int)__builtin_offsetof(struct v22fp_dsp, fse)
	 + (int)__builtin_offsetof(struct v22_fse, mu_sel)
		== V22FP_EQ_MODE) ? 1 : -1];
typedef char v22fp_prc_eqextra[
	((int)__builtin_offsetof(struct v22fp_dsp, fse)
	 + (int)__builtin_offsetof(struct v22_fse, lms_on)
		== V22FP_EQ_EXTRA) ? 1 : -1];
typedef char v22fp_prc_quality[
	((int)__builtin_offsetof(struct v22fp_dsp, fse)
	 + (int)__builtin_offsetof(struct v22_fse, mse)
		== V22FP_QUALITY) ? 1 : -1];

#endif /* 32-bit */

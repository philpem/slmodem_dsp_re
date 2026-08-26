/*
 * v32fpctl.c -- ITU-T V.32 / V.32bis: the datapump's control surface.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V32FP_delete             .text 0x07f7c0  293
 *   V32FP_GetDiagnostics     .text 0x07f8f0   21
 *   V32FP_GetCleanedSamples  .text 0x07f910   53
 *   SetTxModeV32             .text 0x081680  520
 *   SetRxModeV32             .text 0x081890  577
 *   SeedScramblerV32         .text 0x081ae0   15
 *   GetRateV32               .text 0x081af0   78
 *   ScrambleDataV32          .text 0x081b40   28
 *   DescrambleDataV32        .text 0x081b60   30
 *   SetAdaptEqV32            .text 0x081e60   79
 *   SetAdaptEcV32            .text 0x081eb0  319
 *   SetRxLoopsV32            .text 0x081ff0   83
 *   SetECRndTripDelayV32     .text 0x082050  233
 *   TxClockSyncV32           .text 0x082140    1
 *   EpochDetectV32           .text 0x082150   17
 *   RetrainDetectV32         .text 0x082170   53
 *   RenegotiateDetectV32     .text 0x0821b0   44
 *   RxClampV32               .text 0x0825f0   49
 *   v32_null_protocol        .text 0x082bd0    1   (file-local there)
 *   SetToneDetect            .text 0x083600  110
 *   CalcTurnAroundDelay      .text 0x083ae0   53
 *   V32_SYMBOL_LEN           .data 0x0076c4    4
 *
 * The functions are in the object's address order, which is what
 * `docs/method/refinement.md` lever 1 asks for: emission order is upstream of
 * register allocation.  Two of them sit 0x2000 below the rest and may belong
 * to a different translation unit; nothing in the object separates them and
 * they are kept here with the family they configure.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS FILE IS FOR, AND WHY IT IS ALMOST ALL STORES
 *
 * V.32 keeps one instance holding two pointers -- `V32_OBJ_HDX` to the
 * half-duplex handshake context and `V32_OBJ_FP` to the datapump block -- and
 * the datapump block is a tiling of sub-objects that other headers already
 * model.  Every function here reaches through those two pointers into one of
 * those sub-objects.  So the reconstruction risk is not arithmetic: it is
 * WHICH sub-object and WHICH FIELD, and that is what the differential test is
 * built to separate.
 *
 * The instance itself is NOT modelled as a struct; v32fpctl.h says why, and
 * lists which sub-object sits at which offset with the instruction that pins
 * each one.
 *
 * ---------------------------------------------------------------------------
 * THE TWO MODE SETTERS ARE ONE CONFIGURATOR WRITTEN TWICE
 *
 * `SetTxModeV32` writes seven fields at fp + 0x30..0x46; `SetRxModeV32` writes
 * seven fields at fp + 0x50b0..0x50c6.  Every pair differs by exactly 0x5080,
 * and `ScrambleDataV32` / `DescrambleDataV32` hand those two bases to
 * `SDMv32_scrambler` / `SDMv32_descrambler`.  So both clusters are
 * `struct v32_sdm` and the seven fields are that header's own -- `group`,
 * the two tap positions, `outmask`, `regmask`, `tap1`, `tap2`.
 *
 * The tail both setters share is therefore readable rather than arithmetic on
 * offsets:
 *
 *     shift    the bits taken out of the register per symbol
 *     outmask  (1 << shift) - 1
 *     regmask  ~outmask, so `reg << shift` keeps room for the new group
 *     tapN     the Recommendation's tap position, less the shift
 *
 * AND THE 14400 ARM'S SHIFT OF 3 IS NOT A DEFECT.  Every other mode's shift is
 * its bits per symbol -- 2, 2, 4, 4, 3, 5 -- and V32_MODE_128T carries six
 * bits and shifts by three.  v32scram.h records why from the consumer's side:
 * `SDMv32_scrambler` special-cases `group == 6` into two three-bit groups, most
 * significant first, and shifts by three.  The two readings were made
 * independently and meet exactly.  Finding F8214.
 */

#include "dsplib/v32fpctl.h"

#include "dsplib/v32data.h"		/* V32_SYMBOL_NOCARRIER, V32FP_*      */
#include "dsplib/v32dec.h"		/* struct v32_dec, FSE_decision_*     */
#include "dsplib/v32scram.h"
#include "dsplib/v32smc.h"
#include "dsplib/vtb.h"
#include "dsplib/fpm_ecc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sysdep.h"

/* The instance is not modelled; see v32fpctl.h.  These are the accessors. */
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
#define SMC(fp)		((struct v32_smc *)(void *)FIELD((fp), V32FP_SMC))
#define PPS(fp)		((struct fpm_pps *)(void *)FIELD((fp), V32FP_PPS))
#define RING(fp)	((struct v32_symout *)(void *)FIELD((fp), V32FP_SYMOUT))
#define ECC(fp)		((struct fpm_ecc *)(void *)FIELD((fp), V32FP_ECC))
#define FSE(fp)		((struct fpm_fse *)(void *)FIELD((fp), V32FP_FSE))
#define DEC(fp)		((struct v32_dec *)FSE(fp)->cfg.owner)

/*
 * The symbol ring's length, indexed by V32_OBJ_SYMLEN_SEL.
 *
 * `.data` in the object -- so not const, even though nothing writes it -- and
 * two entries, which is the symbol size and the section layout together.
 */
short V32_SYMBOL_LEN[2] = { 12, 48 };

/* --------------------------------------------------------------------- */

/*
 * Tear the whole datapump down.
 *
 * TWO THINGS ABOUT THIS FUNCTION ARE DELIBERATE.
 *
 * It re-reads V32_OBJ_HDX and V32_OBJ_FP before every single use -- thirteen
 * loads of two fields.  That is what the object does, and it is not an
 * accident of scheduling: `sysdep_free` is an external call, so the compiler
 * cannot keep either pointer live across one.  Caching them in a local emits
 * two loads and no reloads, which is a different function.
 *
 * THE FIVE `FPM_*_free` CALLS TAKE A SECOND ARGUMENT IN THE OBJECT AND NOT
 * HERE.  Before each of them the object stores a literal 1 into the outgoing
 * area's second slot -- `mov $0x1,%ecx; mov %ecx,0x4(%esp)` -- and the callees
 * read only the first.  GCC does not emit dead stores into the argument area,
 * so the author's declarations for these five had two parameters; `fresh`, as
 * on the matching `_init`, is the obvious candidate and is not claimed.  The
 * differential tier cannot see it either way.  D481 and finding F8215.
 */
void
V32FP_delete(void *modem)
{
	FPM_MTD_delete((struct fpm_mtd *)FIELD_PTR(HDX(modem), V32_HDX_MTD));
	FPM_TONE_delete((struct fpm_tone *)FIELD_PTR(HDX(modem),
						     V32_HDX_TONE2));
	FPM_TONE_delete((struct fpm_tone *)FIELD_PTR(HDX(modem),
						     V32_HDX_TONE1));
	FPM_TONE_delete((struct fpm_tone *)FIELD_PTR(HDX(modem),
						     V32_HDX_TONE0));

	sysdep_free(FIELD_PTR(FP(modem), V32FP_BUF_5038));
	FPM_FSE_free(FSE(FP(modem)));
	FPM_SRE_free((struct fpm_sre *)(void *)FIELD(FP(modem), V32FP_SRE));
	FPM_ECC_free(ECC(FP(modem)));
	FPM_MRF_free((struct fpm_mrf *)(void *)FIELD(FP(modem), V32FP_MRF));
	FPM_PPS_free(PPS(FP(modem)));
	sysdep_free(FIELD_PTR(FP(modem), V32FP_BUF_50CC));
	sysdep_free(FIELD_PTR(FP(modem), V32FP_CLEAN_BUF));

	sysdep_free(FIELD_PTR(HDX(modem), V32_HDX_BUF_A4));
	sysdep_free(HDX(modem));
	sysdep_free(FP(modem));
	sysdep_free(modem);
}

/*
 * Drain the equaliser's scatter log.  A pure forwarder: only the first
 * argument is rewritten, and it is a tail call in the object.
 */
int
V32FP_GetDiagnostics(void *modem, int which, struct fpm_fse_point *out,
		     int max)
{
	return FSE_getdiag(FSE(FP(modem)), which, out, max);
}

/*
 * The echo-cancelled input block, and how many samples are in it.
 *
 * A count above V32FP_CLEAN_MAX is reported as NONE rather than clamped, and
 * the buffer pointer comes back either way.  The comparison is unsigned and
 * the delivered count is sign-extended from sixteen bits, which agree over
 * every value that reaches the first branch.
 */
short *
V32FP_GetCleanedSamples(void *modem, int *n)
{
	unsigned short have;

	have = FIELD_U16(FP(modem), V32FP_CLEAN_N);
	if (have > V32FP_CLEAN_MAX)
		*n = 0;
	else
		*n = (short)have;
	return (short *)FIELD_PTR(FP(modem), V32FP_CLEAN_BUF);
}

/* --------------------------------------------------------------------- */

/*
 * Configure the transmitter.
 *
 * The dispatch sign-extends `mode` and then tests it UNSIGNED against 6, so a
 * negative mode reaches the default arm along with 7 and above.
 *
 * The default arm posts a reason code and still runs the scrambler tail, with
 * a shift of zero -- an out-of-range request leaves `outmask` at 0 and
 * `regmask` at all-ones, which is a scrambler that emits nothing.
 */
void
SetTxModeV32(void *modem, short mode)
{
	unsigned char *fp;
	struct v32_sdm *sdm;
	unsigned int mask;
	short sel;
	int shift;

	switch (mode) {
	case V32_MODE_ABS4:
	case V32_MODE_DIF4:
		/*
		 * One arm for both: the object tests `mode` a second time
		 * inside it and the only thing that differs is the encoder,
		 * absolute against differential.  `smc->pad02` takes the same
		 * value as the selector, which is the first evidence anywhere
		 * that anything writes that field at all -- v32smc.h has it as
		 * "not read by any encoder", which stays true.  Finding F8216.
		 */
		fp = FP(modem);
		sel = (short)(mode == V32_MODE_ABS4 ? 1 : 0);
		SMC(fp)->pad02 = sel;
		FIELD_S16(fp, V32FP_ENCODER_SEL) = sel;
		SDM_TX(fp)->group = 2;
		SMC(fp)->mode = 0;
		SMC(fp)->shift = 0;
		PPS(fp)->cfg.imap = SMCv32_IMAP16;
		PPS(fp)->cfg.qmap = SMCv32_QMAP16;
		FIELD_S16(fp, V32FP_SHORT_2C) = 0;
		shift = 2;
		break;

	case V32_MODE_16:
		fp = FP(modem);
		SDM_TX(fp)->group = 4;
		SMC(fp)->mode = 1;
		SMC(fp)->pad02 = 0;
		FIELD_S16(fp, V32FP_ENCODER_SEL) = 0;
		SMC(fp)->shift = 2;
		PPS(fp)->cfg.imap = SMCv32_IMAP16;
		PPS(fp)->cfg.qmap = SMCv32_QMAP16;
		FIELD_S16(fp, V32FP_SHORT_2C) = 1;
		shift = 4;
		break;

	case V32_MODE_32T:
		fp = FP(modem);
		SDM_TX(fp)->group = 4;
		SMC(fp)->f14 = 2;
		SMC(fp)->mode = 2;
		SMC(fp)->pad02 = 2;
		FIELD_S16(fp, V32FP_ENCODER_SEL) = 2;
		SMC(fp)->shift = 2;
		PPS(fp)->cfg.imap = VTBv32_IMAP32;
		PPS(fp)->cfg.qmap = VTBv32_QMAP32;
		FIELD_S16(fp, V32FP_SHORT_2C) = 1;
		shift = 4;
		break;

	case V32_MODE_16T:
		fp = FP(modem);
		SDM_TX(fp)->group = 3;
		SMC(fp)->f14 = 1;
		SMC(fp)->mode = 3;
		FIELD_S16(fp, V32FP_ENCODER_SEL) = 2;
		PPS(fp)->cfg.imap = VTBv32_IMAP16T;
		PPS(fp)->cfg.qmap = VTBv32_QMAP16T;
		FIELD_S16(fp, V32FP_SHORT_2C) = 3;
		shift = 3;
		break;

	case V32_MODE_64T:
		fp = FP(modem);
		SDM_TX(fp)->group = 5;
		SMC(fp)->f14 = 3;
		SMC(fp)->mode = 4;
		FIELD_S16(fp, V32FP_ENCODER_SEL) = 2;
		PPS(fp)->cfg.imap = VTBv32_IMAP64;
		PPS(fp)->cfg.qmap = VTBv32_QMAP64;
		FIELD_S16(fp, V32FP_SHORT_2C) = 4;
		shift = 5;
		break;

	case V32_MODE_128T:
		fp = FP(modem);
		SDM_TX(fp)->group = 6;
		SMC(fp)->f14 = 4;
		SMC(fp)->mode = 5;
		SMC(fp)->pad02 = 2;
		FIELD_S16(fp, V32FP_ENCODER_SEL) = 2;
		PPS(fp)->cfg.imap = VTBv32_IMAP128;
		PPS(fp)->cfg.qmap = VTBv32_QMAP128;
		FIELD_S16(fp, V32FP_SHORT_2C) = 5;
		shift = 3;		/* six bits, sent as two groups of 3 */
		break;

	default:
		FIELD_U8(modem, V32_OBJ_STATUS) = V32_STATUS_BAD_MODE;
		FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_FAULT;
		fp = FP(modem);
		shift = 0;
		break;
	}

	sdm = SDM_TX(fp);
	mask = (unsigned int)((1 << shift) - 1);
	sdm->outmask = mask;
	sdm->regmask = ~mask;
	sdm->tap1 = (short)(FIELD_U16(sdm, V32_SDM_TAP1_POS) - shift);
	sdm->tap2 = (short)(FIELD_U16(sdm, V32_SDM_TAP2_POS) - shift);
}

/*
 * Configure the receiver, mode for mode with the transmitter.
 *
 * Three things differ from the transmit side and all three are the object's:
 * the reason code and the fault flag are written the other way round (no
 * comparison can separate the two orders -- both leave the same two bytes);
 * the four trellis arms call `VTBv32_init` on the decoder's Viterbi state,
 * which is why the shift survives the call in a callee-saved register; and
 * the two four-point arms clear the decoder's differential context.
 */
void
SetRxModeV32(void *modem, short mode)
{
	unsigned char *fp;
	struct v32_dec *dec;
	struct v32_sdm *sdm;
	unsigned int mask;
	int shift;

	switch (mode) {
	case V32_MODE_ABS4:
	case V32_MODE_DIF4:
		fp = FP(modem);
		SDM_RX(fp)->group = 2;
		FIELD_S16(fp, V32FP_SHORT_509C) = 0;
		dec = DEC(fp);
		FIELD_S16(fp, V32FP_SHORT_5098) = 0;
		FIELD_S16(dec, V32_DEC_SHORT_04) = 0;
		dec->chan = 0;
		FIELD_S16(fp, V32FP_SHORT_2E) = 0;
		dec->count = 0;
		if (mode == V32_MODE_ABS4) {
			dec->rate_change = 0;
			FSE(fp)->cfg.decision = FSE_decision_AB;
		} else {
			FSE(fp)->cfg.decision = FSE_decision_4pt;
		}
		shift = 2;
		break;

	case V32_MODE_16:
		fp = FP(modem);
		SDM_RX(fp)->group = 4;
		dec = DEC(fp);
		FIELD_S16(fp, V32FP_SHORT_5098) = 1;
		FIELD_S16(fp, V32FP_SHORT_509C) = 2;
		dec->chan = 1;
		FIELD_S16(dec, V32_DEC_SHORT_04) = 2;
		FSE(fp)->cfg.decision = FSE_decision_16pt;
		FIELD_S16(fp, V32FP_SHORT_2E) = 1;
		shift = 4;
		break;

	case V32_MODE_32T:
		fp = FP(modem);
		FIELD_S16(fp, V32FP_SHORT_5098) = 1;
		dec = DEC(fp);
		SDM_RX(fp)->group = 4;
		FIELD_S16(fp, V32FP_SHORT_509C) = 2;
		dec->chan = 1;
		FIELD_S16(dec, V32_DEC_SHORT_04) = 2;
		FSE(fp)->cfg.decision = FSE_decision_32pt;
		FIELD_S16(fp, V32FP_SHORT_2E) = 1;
		VTBv32_init((struct vtb *)(void *)dec->vtb, 2, 0);
		shift = 4;
		break;

	case V32_MODE_16T:
		fp = FP(modem);
		SDM_RX(fp)->group = 3;
		FSE(fp)->cfg.decision = FSE_decision_16Tpt;
		FIELD_S16(fp, V32FP_SHORT_2E) = 3;
		VTBv32_init((struct vtb *)(void *)DEC(fp)->vtb, 3, 0);
		shift = 3;
		break;

	case V32_MODE_64T:
		fp = FP(modem);
		SDM_RX(fp)->group = 5;
		FSE(fp)->cfg.decision = FSE_decision_64pt;
		FIELD_S16(fp, V32FP_SHORT_2E) = 4;
		VTBv32_init((struct vtb *)(void *)DEC(fp)->vtb, 4, 0);
		shift = 5;
		break;

	case V32_MODE_128T:
		fp = FP(modem);
		SDM_RX(fp)->group = 6;
		FSE(fp)->cfg.decision = FSE_decision_128pt;
		FIELD_S16(fp, V32FP_SHORT_2E) = 5;
		VTBv32_init((struct vtb *)(void *)DEC(fp)->vtb, 5, 0);
		shift = 3;		/* see SetTxModeV32 */
		break;

	default:
		FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_FAULT;
		FIELD_U8(modem, V32_OBJ_STATUS) = V32_STATUS_BAD_MODE;
		shift = 0;
		break;
	}

	sdm = SDM_RX(FP(modem));
	mask = (unsigned int)((1 << shift) - 1);
	sdm->outmask = mask;
	sdm->regmask = ~mask;
	sdm->tap1 = (short)(FIELD_U16(sdm, V32_SDM_TAP1_POS) - shift);
	sdm->tap2 = (short)(FIELD_U16(sdm, V32_SDM_TAP2_POS) - shift);
}

/* Load the transmit scrambler's shift register. */
void
SeedScramblerV32(void *modem, unsigned int seed)
{
	SDM_TX(FP(modem))->reg = seed;
}

/*
 * The line rate, as a V32_RATE_*.
 *
 * 9600 splits on V32_OBJ_TRELLIS -- the two 9600 modes are the 32-point
 * trellis one and the 16-point uncoded one -- and an unrecognised bit rate
 * reports V32_RATE_INVALID rather than any of the five.
 */
int
GetRateV32(void *modem)
{
	unsigned short bps = FIELD_U16(modem, V32_OBJ_BPS);

	if (bps == 14400)
		return V32_RATE_14400;
	if (bps == 12000)
		return V32_RATE_12000;
	if (bps == 9600)
		return FIELD_INT(modem, V32_OBJ_TRELLIS) ? V32_RATE_9600
							 : V32_RATE_9600_NT;
	if (bps == 7200)
		return V32_RATE_7200;
	if (bps == 4800)
		return V32_RATE_4800;
	return V32_RATE_INVALID;
}

/* Scramble `count` words of `buf` in place.  A tail call in the object. */
void
ScrambleDataV32(void *modem, short *buf, unsigned short count)
{
	SDMv32_scrambler(SDM_TX(FP(modem)), buf, count);
}

/* And the receive direction, through the descrambler at fp + 0x50b0. */
void
DescrambleDataV32(void *modem, short *buf, unsigned short count)
{
	SDMv32_descrambler(SDM_RX(FP(modem)), buf, count);
}

/*
 * Equaliser adaptation.
 *
 * V32_ADAPTEQ_MU0 and _MU1 both switch adaptation on and differ only in which
 * of `fpm_fse_cfg::mu[]` the LMS update takes its step size from.  Any other
 * value does nothing at all -- not even the switch-off.
 */
void
SetAdaptEqV32(void *modem, unsigned short mode)
{
	unsigned char *fp;

	switch (mode) {
	case V32_ADAPTEQ_OFF:
		fp = FP(modem);
		FIELD_INT(fp, V32FP_EQ_ADAPT) = 0;
		break;
	case V32_ADAPTEQ_MU0:
		fp = FP(modem);
		FIELD_INT(fp, V32FP_EQ_ADAPT) = 1;
		FSE(fp)->mu_sel = 0;
		break;
	case V32_ADAPTEQ_MU1:
		fp = FP(modem);
		FIELD_INT(fp, V32FP_EQ_ADAPT) = 1;
		FSE(fp)->mu_sel = 1;
		break;
	default:
		break;
	}
}

/*
 * Echo canceller.
 *
 * V32_ADAPTEC_RESET re-initialises it in place -- note that the config it
 * hands `FPM_ECC_init` is the canceller's OWN, which works because
 * `struct fpm_ecc` opens with its `struct fpm_ecc_cfg` and init copies it
 * before touching anything.  The two delay inputs are set first because init
 * reads them and never writes them, and the three geometry fields afterwards
 * because init has just overwritten them.
 *
 * V32_ADAPTEC_ON clears the delay line before enabling the two update loops.
 * V32_ADAPTEC_SLOW divides the update gain by five and changes nothing else.
 */
void
SetAdaptEcV32(void *modem, unsigned short mode)
{
	unsigned char *fp;
	struct fpm_ecc *ecc;
	short i;

	switch (mode) {
	case V32_ADAPTEC_OFF:
		fp = FP(modem);
		ECC(fp)->adapt_near = 0;
		ECC(fp)->adapt_far = 0;
		FIELD_INT(fp, V32FP_R14) = 0;
		FIELD_INT(fp, V32FP_R18) = 0;
		break;

	case V32_ADAPTEC_ON:
		fp = FP(modem);
		ecc = ECC(fp);
		for (i = 0; i < ecc->line_len; i++)
			ecc->line[i] = 0;
		FIELD_INT(fp, V32FP_R14) = 1;
		ecc->unk1a = 0;
		ecc->adapt_near = 1;
		ecc->adapt_far = 1;
		FIELD_INT(fp, V32FP_R18) = 1;
		break;

	case V32_ADAPTEC_SLOW:
		ecc = ECC(FP(modem));
		ecc->mu = (short)(ecc->mu / 5);
		break;

	case V32_ADAPTEC_RESET:
		ecc = ECC(FP(modem));
		ecc->near_delay = (short)FIELD_U16(modem,
						   V32_OBJ_EC_NEAR_DELAY);
		ecc->far_delay = 0x30;
		FPM_ECC_init(ecc, (const struct fpm_ecc_cfg *)(void *)ecc, 0);
		ecc = ECC(FP(modem));
		ecc->line_len = 0x30;
		ecc->near_rd = 0;
		ecc->far_rd = 0;
		break;

	default:
		break;
	}
}

/*
 * The four switches at V32FP_R00..V32FP_R0C, as one group.
 *
 * The store order is +0x08, +0x0c, +0x00, +0x04 in both arms and is written
 * that way here; no comparison can separate it from any other order, since all
 * four end up holding the same value.
 */
void
SetRxLoopsV32(void *modem, unsigned short mode)
{
	unsigned char *fp;

	switch (mode) {
	case 1:
		fp = FP(modem);
		FIELD_INT(fp, V32FP_R08) = 0;
		FIELD_INT(fp, V32FP_R0C) = 0;
		FIELD_INT(fp, V32FP_R00) = 0;
		FIELD_INT(fp, V32FP_R04) = 0;
		break;
	case 2:
	case 3:
		fp = FP(modem);
		FIELD_INT(fp, V32FP_R08) = 1;
		FIELD_INT(fp, V32FP_R0C) = 1;
		FIELD_INT(fp, V32FP_R00) = 1;
		FIELD_INT(fp, V32FP_R04) = 1;
		break;
	default:
		break;
	}
}

/*
 * Place the echo canceller for a round-trip delay of `delay` symbols.
 *
 * The requested delay is raised to the far section's length, clamped at zero,
 * and then held down to the far lag; `fpm_ecc.h` has both of those as config
 * fields the canceller is built around, so the effect is "as much delay as the
 * geometry can express".  From that:
 *
 *     far_rd    = one symbol period
 *     line_len  = the context's own delay + the round trip + two symbols
 *     near_rd   = round trip + one symbol, wrapped into the line
 *     ring      = the same again with half a symbol added, and the ring's
 *                 length set to the line's
 *
 * and the line is preset to the no-carrier constellation index.  THAT LAST
 * IDENTIFICATION IS USAGE INFERENCE: the object writes a bare 0x10, and
 * v32data.h's V32_SYMBOL_NOCARRIER is the same literal reached the same way --
 * the seventeenth entry of a sixteen-point map -- but nothing states they are
 * one constant.
 *
 * Every wrap is `if (line_len <= x) x -= line_len`, which is a single
 * conditional subtraction and not a modulus; it is correct only while `x` is
 * below twice the line length, and every `x` here is.
 */
void
SetECRndTripDelayV32(void *modem, short delay)
{
	unsigned char *fp;
	struct fpm_ecc *ecc;
	struct v32_symout *ring;
	short lag, symlen, d, t, i;

	lag = FIELD_S16(HDX(modem), V32_HDX_SHORT_9C);
	symlen = V32_SYMBOL_LEN[FIELD_S16(modem, V32_OBJ_SYMLEN_SEL)];
	fp = FP(modem);
	ecc = ECC(fp);
	ring = RING(fp);

	d = ecc->cfg.far_taps;
	if (d < delay)
		d = delay;
	if (d < 0)
		d = 0;
	if (ecc->cfg.far_lag < d)
		d = ecc->cfg.far_lag;

	ecc->far_rd = symlen;
	ecc->line_len = (short)(lag + d + 2 * symlen);

	t = (short)(d + symlen);
	if (ecc->line_len <= t)
		t = (short)(t - ecc->line_len);
	ecc->near_rd = t;

	t = (short)(lag + (short)(symlen / 2 + ecc->near_rd));
	if (ecc->line_len <= t)
		t = (short)(t - ecc->line_len);
	ring->pad0e = t;
	ring->limit = ecc->line_len;
	ring->widx = ring->pad0e;

	for (i = 0; i < ecc->line_len; i++)
		ecc->line[i] = V32_SYMBOL_NOCARRIER;
}

/*
 * A `ret` and nothing else.
 *
 * Its one caller, `v32_data`, passes the instance and ignores the result, so
 * nothing about the signature is recoverable beyond "at most one argument is
 * read, and none is".  Either the transmit clock needs no correction in V.32 --
 * V.22's `TxClockSync` at 0x8e610 is 22 bytes and does real work -- or the
 * function was emptied.  Nothing here decides which.
 */
void
TxClockSyncV32(void *modem)
{
	(void)modem;
}

/* The decoder's rate-change report, straight through and not cleared. */
int
EpochDetectV32(void *modem)
{
	return DEC(FP(modem))->rate_change;
}

/*
 * Poll and clear the decoder's retrain request, and count it.
 *
 * v32dec.h has +0x62 as `retrain`, "1 or 2; a request, not a state -- nothing
 * here reads it back".  This is what reads it back, and the two pollers name
 * the two bits between them.  The phase counter is zeroed BEFORE the request
 * bit is cleared; both orders leave the same bytes, so no test can separate
 * them and the object's order is the one written.
 */
int
RetrainDetectV32(void *modem)
{
	struct v32_dec *dec = DEC(FP(modem));
	unsigned short req = (unsigned short)dec->retrain;

	if (!(req & V32_DEC_RETRAIN_REQ))
		return 0;

	dec->count = 0;
	dec->retrain = (short)(req & ~(unsigned short)V32_DEC_RETRAIN_REQ);
	FIELD_U16(dec, V32_DEC_RETRAIN_N) =
		(unsigned short)(FIELD_U16(dec, V32_DEC_RETRAIN_N) + 1);
	return 1;
}

/* The renegotiation request, bit 1 of the same word.  It is not counted. */
int
RenegotiateDetectV32(void *modem)
{
	struct v32_dec *dec = DEC(FP(modem));
	unsigned short req = (unsigned short)dec->retrain;

	if (!(req & V32_DEC_RENEG_REQ))
		return 0;

	dec->count = 0;
	dec->retrain = (short)(req & ~(unsigned short)V32_DEC_RENEG_REQ);
	return 1;
}

/*
 * Fill one block's worth of `out` with a clamped word and report the count.
 *
 * The counter is a SHORT and the loop tests it against -1, so a count of zero
 * writes nothing and a NEGATIVE count runs to 65535-ish writes rather than
 * none.  That is the object's loop and it is reproduced; D482 records it.
 *
 * `in` and `count` are never read.  Their types are `RxHdxNull`'s call and no
 * stronger -- the whole Rx state family takes the same four.
 */
unsigned short
RxClampV32(void *modem, short *in, short *out, unsigned short count)
{
	void *hdx = HDX(modem);
	short i;

	(void)in;
	(void)count;

	for (i = (short)(FIELD_S16(hdx, V32_HDX_SHORT_9E) - 1); i != -1; i--)
		*out++ = 0xff;

	return FIELD_U16(hdx, V32_HDX_SHORT_9E);
}

/*
 * FILE-LOCAL IN THE OBJECT and global here, which is the arrangement
 * `src/pump/v34/v34hshak.c` already uses for `getbit`: a `static` has no
 * symbol for the differential harness to compare against, and the blob's copy
 * is reached as `ref_v32_null_protocol` through `symmap.py --globals`.
 *
 * One byte of `ret`.  Nothing in `.text` references it, so its one reference is
 * from a table this batch does not write, and `void (void)` is a placeholder
 * for a signature that is not recoverable.
 */
void
v32_null_protocol(void)
{
}

/*
 * Retune the first tone detector.
 *
 * It rebuilds the object from a COPY OF ITS OWN CONFIGURATION with the
 * frequency replaced, which is why `struct fpm_tone_cfg` being exactly the
 * first 36 bytes of `struct fpm_tone` matters: the object copies nine dwords
 * off the front of the tone object onto the stack and hands that back.
 *
 * The tone pointer is loaded TWICE, once for the copy and once for the call.
 * That is the object's, and it is what a source that names the field at both
 * sites gives.
 */
void
SetToneDetect(void *modem, short hz)
{
	void *hdx = HDX(modem);
	struct fpm_tone_cfg cfg;

	cfg = ((struct fpm_tone *)FIELD_PTR(hdx, V32_HDX_TONE0))->cfg;
	cfg.freq = hz;
	FPM_TONE_create((struct fpm_tone *)FIELD_PTR(hdx, V32_HDX_TONE0), &cfg);
}

/*
 * What is left of the turnaround budget, clamped at zero.
 *
 * The subtraction is narrowed to sixteen bits BEFORE the clamp -- the object
 * does `cwtl` and then the branchless `x & ~(x >> 31)` -- so a budget that
 * underflows past 32768 comes back positive rather than clamped.  D483.
 *
 * The four fields are loaded `movzwl` here and +0x9c `movswl` in
 * `SetECRndTripDelayV32`.  Both extensions are DEAD -- every use is truncated
 * back to sixteen bits -- so the signedness is the compiler's free choice at
 * each site (finding F614) and each site is written the way the object has it.
 */
short
CalcTurnAroundDelay(void *modem)
{
	void *hdx = HDX(modem);
	short left;

	left = (short)(FIELD_U16(hdx, V32_HDX_SHORT_94)
		       - (FIELD_U16(hdx, V32_HDX_SHORT_98)
			  + FIELD_U16(hdx, V32_HDX_SHORT_9C)
			  + FIELD_U16(hdx, V32_HDX_SHORT_9A)));
	return left < 0 ? 0 : left;
}

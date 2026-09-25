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
 *   v32_null_protocol        .text 0x082bd0    1   (moved to v32fpdisp.c)
 *   SetToneDetect            .text 0x083600  110
 *   CalcTurnAroundDelay      .text 0x083ae0   53   (moved to V32rxhdx.c)
 *   V32_TURNAROUND_DLY       .data 0x0076c0    4
 *   V32_SYMBOL_LEN           .data 0x0076c4    4
 *   V32_SAMPLE_LEN           .data 0x0076c8    4
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
 * The owning allocations and their embedded sub-objects are modelled in
 * v32struct.h; this file retains the object's repeated owner-pointer reloads.
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

/* Raw access remains only for deliberately overlapping legacy subfields. */
#include "dsplib/v32struct.h"

#define HDX(m)			((m)->hdx)
#define FP(m)			((m)->fp)

#define SDM_TX(fp)	(&((struct v32_fp *)(fp))->scrambler)
#define SDM_RX(fp)	(&((struct v32_fp *)(fp))->descrambler)
#define SMC(fp)		(&((struct v32_fp *)(fp))->tx_smc)
#define PPS(fp)		(&((struct v32_fp *)(fp))->pps)
#define RING(fp)	(&((struct v32_fp *)(fp))->symout)
#define ECC(fp)		(&((struct v32_fp *)(fp))->ecc)
#define FSE(fp)		(&((struct v32_fp *)(fp))->fse)
#define DEC(fp)		((struct v32_dec *)FSE(fp)->cfg.owner)

/*
 * THE THREE LENGTH TABLES ARE ONE FAMILY AND THE OBJECT LAYS THEM OUT AS ONE.
 *
 *   V32_TURNAROUND_DLY  .data 0x0076c0   4   { 64, 360 }
 *   V32_SYMBOL_LEN      .data 0x0076c4   4   { 12,  48 }
 *   V32_SAMPLE_LEN      .data 0x0076c8   4   { 40, 160 }
 *
 * Twelve contiguous bytes, all three GLOBAL, all three two entries, and all
 * three indexed by the SAME selector at every site -- `V32FP_recreate` uses
 * obj + 0x16 for all three within twenty instructions (7f13d, 7f150, 7f165)
 * and `V32FP_control` uses obj + 0x18 for two of them (84611, 84624).  They
 * are declared here in the object's address order for that reason.
 *
 * The ratio between the two columns is 4:1 in samples (40 and 160), 4:1 in
 * symbols (12 and 48) and 45:8 in turnaround delay (64 and 360), so the third
 * is not simply the other two scaled and the selector is not a sample-rate
 * switch alone.  Nothing written establishes what the two columns ARE; what
 * is established is that one selector picks a column in all three.
 *
 * `.data` in the object -- so not const, even though nothing writes any of
 * them.
 */
short V32_TURNAROUND_DLY[2] = { 64, 360 };
short V32_SYMBOL_LEN[2] = { 12, 48 };
short V32_SAMPLE_LEN[2] = { 40, 160 };

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
V32FP_delete(struct v32_modem *modem)
{
	FPM_MTD_delete((struct fpm_mtd *)HDX(modem)->mtd);
	FPM_TONE_delete((struct fpm_tone *)HDX(modem)->tone2);
	FPM_TONE_delete((struct fpm_tone *)HDX(modem)->tone1);
	FPM_TONE_delete((struct fpm_tone *)HDX(modem)->tone0);

	sysdep_free(FP(modem)->decoder.vtb.paths);
	FPM_FSE_free(FSE(FP(modem)));
	FPM_SRE_free(&FP(modem)->sre);
	FPM_ECC_free(ECC(FP(modem)));
	FPM_MRF_free(&FP(modem)->mrf);
	FPM_PPS_free(PPS(FP(modem)));
	sysdep_free(FP(modem)->rx_buf);
	sysdep_free(FP(modem)->clean_buf);

	sysdep_free(HDX(modem)->buffer);
	sysdep_free(HDX(modem));
	sysdep_free(FP(modem));
	sysdep_free(modem);
}

/*
 * Drain the equaliser's scatter log.  A pure forwarder: only the first
 * argument is rewritten, and it is a tail call in the object.
 */
int
V32FP_GetDiagnostics(struct v32_modem *modem, int which, struct fpm_fse_point *out,
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
V32FP_GetCleanedSamples(struct v32_modem *modem, int *n)
{
	unsigned short have;

	have = FP(modem)->clean_n;
	if (have > V32FP_CLEAN_MAX)
		*n = 0;
	else
		*n = (short)have;
	return (short *)FP(modem)->clean_buf;
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
SetTxModeV32(struct v32_modem *modem, short mode)
{
	struct v32_fp *fp;
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
		fp->encoder_sel = sel;
		SDM_TX(fp)->group = 2;
		SMC(fp)->mode = 0;
		SMC(fp)->shift = 0;
		PPS(fp)->cfg.imap = SMCv32_IMAP16;
		PPS(fp)->cfg.qmap = SMCv32_QMAP16;
		fp->short_2c = 0;
		shift = 2;
		break;

	case V32_MODE_16:
		fp = FP(modem);
		SDM_TX(fp)->group = 4;
		SMC(fp)->mode = 1;
		SMC(fp)->pad02 = 0;
		fp->encoder_sel = 0;
		SMC(fp)->shift = 2;
		PPS(fp)->cfg.imap = SMCv32_IMAP16;
		PPS(fp)->cfg.qmap = SMCv32_QMAP16;
		fp->short_2c = 1;
		shift = 4;
		break;

	case V32_MODE_32T:
		fp = FP(modem);
		SDM_TX(fp)->group = 4;
		SMC(fp)->uncoded_bits = 2;
		SMC(fp)->mode = 2;
		SMC(fp)->pad02 = 2;
		fp->encoder_sel = 2;
		SMC(fp)->shift = 2;
		PPS(fp)->cfg.imap = VTBv32_IMAP32;
		PPS(fp)->cfg.qmap = VTBv32_QMAP32;
		fp->short_2c = 1;
		shift = 4;
		break;

	case V32_MODE_16T:
		fp = FP(modem);
		SDM_TX(fp)->group = 3;
		SMC(fp)->uncoded_bits = 1;
		SMC(fp)->mode = 3;
		fp->encoder_sel = 2;
		PPS(fp)->cfg.imap = VTBv32_IMAP16T;
		PPS(fp)->cfg.qmap = VTBv32_QMAP16T;
		fp->short_2c = 3;
		shift = 3;
		break;

	case V32_MODE_64T:
		fp = FP(modem);
		SDM_TX(fp)->group = 5;
		SMC(fp)->uncoded_bits = 3;
		SMC(fp)->mode = 4;
		fp->encoder_sel = 2;
		PPS(fp)->cfg.imap = VTBv32_IMAP64;
		PPS(fp)->cfg.qmap = VTBv32_QMAP64;
		fp->short_2c = 4;
		shift = 5;
		break;

	case V32_MODE_128T:
		fp = FP(modem);
		SDM_TX(fp)->group = 6;
		SMC(fp)->uncoded_bits = 4;
		SMC(fp)->mode = 5;
		SMC(fp)->pad02 = 2;
		fp->encoder_sel = 2;
		PPS(fp)->cfg.imap = VTBv32_IMAP128;
		PPS(fp)->cfg.qmap = VTBv32_QMAP128;
		fp->short_2c = 5;
		shift = 3;		/* six bits, sent as two groups of 3 */
		break;

	default:
		modem->status = V32_STATUS_BAD_MODE;
		modem->flags |= V32_FLAG_FAULT;
		fp = FP(modem);
		shift = 0;
		break;
	}

	sdm = SDM_TX(fp);
	mask = (unsigned int)((1 << shift) - 1);
	sdm->outmask = mask;
	sdm->regmask = ~mask;
	sdm->tap1 = (short)(sdm->tap1_pos - shift);
	sdm->tap2 = (short)(sdm->tap2_pos - shift);
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
SetRxModeV32(struct v32_modem *modem, short mode)
{
	struct v32_fp *fp;
	struct v32_dec *dec;
	struct v32_sdm *sdm;
	unsigned int mask;
	int shift;

	switch (mode) {
	case V32_MODE_ABS4:
	case V32_MODE_DIF4:
		fp = FP(modem);
		SDM_RX(fp)->group = 2;
		fp->rx_smc.shift = 0;
		dec = DEC(fp);
		fp->rx_smc.mode = 0;
		dec->short_04 = 0;
		dec->chan = 0;
		fp->short_2e = 0;
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
		fp->rx_smc.mode = 1;
		fp->rx_smc.shift = 2;
		dec->chan = 1;
		dec->short_04 = 2;
		FSE(fp)->cfg.decision = FSE_decision_16pt;
		fp->short_2e = 1;
		shift = 4;
		break;

	case V32_MODE_32T:
		fp = FP(modem);
		fp->rx_smc.mode = 1;
		dec = DEC(fp);
		SDM_RX(fp)->group = 4;
		fp->rx_smc.shift = 2;
		dec->chan = 1;
		dec->short_04 = 2;
		FSE(fp)->cfg.decision = FSE_decision_32pt;
		fp->short_2e = 1;
		VTBv32_init(&dec->vtb, 2, 0);
		shift = 4;
		break;

	case V32_MODE_16T:
		fp = FP(modem);
		SDM_RX(fp)->group = 3;
		FSE(fp)->cfg.decision = FSE_decision_16Tpt;
		fp->short_2e = 3;
		VTBv32_init(&DEC(fp)->vtb, 3, 0);
		shift = 3;
		break;

	case V32_MODE_64T:
		fp = FP(modem);
		SDM_RX(fp)->group = 5;
		FSE(fp)->cfg.decision = FSE_decision_64pt;
		fp->short_2e = 4;
		VTBv32_init(&DEC(fp)->vtb, 4, 0);
		shift = 5;
		break;

	case V32_MODE_128T:
		fp = FP(modem);
		SDM_RX(fp)->group = 6;
		FSE(fp)->cfg.decision = FSE_decision_128pt;
		fp->short_2e = 5;
		VTBv32_init(&DEC(fp)->vtb, 5, 0);
		shift = 3;		/* see SetTxModeV32 */
		break;

	default:
		modem->flags |= V32_FLAG_FAULT;
		modem->status = V32_STATUS_BAD_MODE;
		shift = 0;
		break;
	}

	sdm = SDM_RX(FP(modem));
	mask = (unsigned int)((1 << shift) - 1);
	sdm->outmask = mask;
	sdm->regmask = ~mask;
	sdm->tap1 = (short)(sdm->tap1_pos - shift);
	sdm->tap2 = (short)(sdm->tap2_pos - shift);
}

/* Load the transmit scrambler's shift register. */
void
SeedScramblerV32(struct v32_modem *modem, unsigned int seed)
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
GetRateV32(struct v32_modem *modem)
{
	unsigned short bps = modem->params.rx_rate;

	if (bps == 14400)
		return V32_RATE_14400;
	if (bps == 12000)
		return V32_RATE_12000;
	if (bps == 9600)
		return modem->params.trellis ? V32_RATE_9600
							 : V32_RATE_9600_NT;
	if (bps == 7200)
		return V32_RATE_7200;
	if (bps == 4800)
		return V32_RATE_4800;
	return V32_RATE_INVALID;
}

/* Scramble `count` words of `buf` in place.  A tail call in the object. */
void
ScrambleDataV32(struct v32_modem *modem, short *buf, unsigned short count)
{
	SDMv32_scrambler(SDM_TX(FP(modem)), buf, count);
}

/* And the receive direction, through the descrambler at fp + 0x50b0. */
void
DescrambleDataV32(struct v32_modem *modem, short *buf, unsigned short count)
{
	SDMv32_descrambler(SDM_RX(FP(modem)), buf, count);
}

/*
 * Equaliser adaptation.
 *
 * V32_ADAPTEQ_MU0 and _MU1 both switch adaptation on and differ only in which
 * of `fpm_fse_cfg::mu[]` the LMS update takes its step size from.  Any other
 * value does nothing at all -- not even the switch-off.
 * The MU1, OFF, MU0 case order recovers the period emission (F10227).
 */
void
SetAdaptEqV32(struct v32_modem *modem, unsigned short mode)
{
	struct v32_fp *fp;

	switch (mode) {
	case V32_ADAPTEQ_MU1:
		fp = FP(modem);
		fp->eq_adapt = 1;
		FSE(fp)->mu_sel = 1;
		break;
	case V32_ADAPTEQ_OFF:
		fp = FP(modem);
		fp->eq_adapt = 0;
		break;
	case V32_ADAPTEQ_MU0:
		fp = FP(modem);
		fp->eq_adapt = 1;
		FSE(fp)->mu_sel = 0;
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
 * V32_ADAPTEC_SLOW divides the update gain by TEN and changes nothing else.
 * The object writes that as the usual reciprocal multiply -- `imul
 * $0x66666667`, `sar $2` on the high half, less the sign -- and the shift is
 * what says ten rather than five: 0x66666667 / 2^32 is 0.4, and the extra two
 * places take it to 0.1.  Both readings compile; only one agrees.
 */
void
SetAdaptEcV32(struct v32_modem *modem, unsigned short mode)
{
	struct v32_fp *fp;
	struct fpm_ecc *ecc;
	short i;

	switch (mode) {
	case V32_ADAPTEC_OFF:
		fp = FP(modem);
		ECC(fp)->adapt_near = 0;
		ECC(fp)->adapt_far = 0;
		fp->int_14 = 0;
		fp->int_18 = 0;
		break;

	case V32_ADAPTEC_ON:
		fp = FP(modem);
		ecc = ECC(fp);
		for (i = 0; i < ecc->line_len; i++)
			ecc->line[i] = 0;
		fp->int_14 = 1;
		ecc->unk1a = 0;
		ecc->adapt_near = 1;
		ecc->adapt_far = 1;
		fp->int_18 = 1;
		break;

	case V32_ADAPTEC_SLOW:
		ecc = ECC(FP(modem));
		ecc->mu = (short)(ecc->mu / 10);
		break;

	case V32_ADAPTEC_RESET:
		ecc = ECC(FP(modem));
		ecc->near_delay = (short)modem->params.ec_near_delay;
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
SetRxLoopsV32(struct v32_modem *modem, unsigned short mode)
{
	struct v32_fp *fp;

	switch (mode) {
	case 1:
		fp = FP(modem);
		fp->int_08 = 0;
		fp->int_0c = 0;
		fp->int_00 = 0;
		fp->int_04 = 0;
		break;
	case 2:
	case 3:
		fp = FP(modem);
		fp->int_08 = 1;
		fp->int_0c = 1;
		fp->int_00 = 1;
		fp->int_04 = 1;
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
SetECRndTripDelayV32(struct v32_modem *modem, short delay)
{
	struct v32_fp *fp;
	struct fpm_ecc *ecc;
	struct v32_symout *ring;
	short lag, symlen, d, t, i;

	lag = HDX(modem)->short_9c;
	symlen = V32_SYMBOL_LEN[modem->params.symlen_sel];
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
TxClockSyncV32(struct v32_modem *modem)
{
	(void)modem;
}

/* The decoder's rate-change report, straight through and not cleared. */
int
EpochDetectV32(struct v32_modem *modem)
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
RetrainDetectV32(struct v32_modem *modem)
{
	struct v32_dec *dec = DEC(FP(modem));
	unsigned short req = (unsigned short)dec->retrain;

	if (!(req & V32_DEC_RETRAIN_REQ))
		return 0;

	dec->count = 0;
	dec->retrain = (short)(req & ~(unsigned short)V32_DEC_RETRAIN_REQ);
	dec->short_74 =
		(unsigned short)(dec->short_74 + 1);
	return 1;
}

/* The renegotiation request, bit 1 of the same word.  It is not counted. */
int
RenegotiateDetectV32(struct v32_modem *modem)
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
RxClampV32(struct v32_modem *modem, short *in, short *out, unsigned short count)
{
	struct v32_hdx *hdx = HDX(modem);
	short i;

	(void)in;
	(void)count;

	for (i = (short)(hdx->symbol_len - 1); i != -1; i--)
		*out++ = 0xff;

	return hdx->symbol_len;
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
SetToneDetect(struct v32_modem *modem, short hz)
{
	struct v32_hdx *hdx = HDX(modem);
	struct fpm_tone_cfg cfg;

	cfg = ((struct fpm_tone *)hdx->tone0)->cfg;
	cfg.freq = hz;
	FPM_TONE_create((struct fpm_tone *)hdx->tone0, &cfg);
}

/*
 * CalcTurnAroundDelay is V32rxhdx.c's and now lives in
 * src/pump/v32/V32rxhdx.c.  The object INLINES it into RxHdxPhsReversal
 * (83c29..83c55 is the same 53 bytes instruction for instruction), which is
 * only possible within one translation unit, so the move is what recovers
 * the object's call shape.  See V32rxhdx.c.
 */

/*
 * V22int.c -- V.22 / V.22bis: the datapump's integer entry points.
 *
 * TU RECONCILIATION (issue #6/#20/#67).  The object's FILE record 483 is
 * `V22int.c`, and it has NO local symbol of its own -- no LOCAL FUNC and no
 * LOCAL OBJECT.  Its extent is therefore recovered from the FILE order and
 * the neighbouring anchors, not from a local:
 *
 *   ... v22_sre.c (record 482) ... V22int.c (483) ... B103.c (484) ...
 *
 * `ld -r` concatenates .text in FILE order, so every V22int.c function sits
 * between the last v22_sre.c function and the first B103.c function:
 *
 *   V22_SRE_free   .text 0x08e0f0  46   (v22_sre.c's last)
 *   SetTxRate      .text 0x08e120 205   \
 *   SetRxRate      .text 0x08e1f0 211    |
 *   ScrambleDataV22 .text 0x08e2d0  28   |
 *   DescrambleDataV22 .text 0x08e2f0 30  |
 *   ModDataV22     .text 0x08e310  95    |  V22int.c
 *   DemodDataV22   .text 0x08e370 510    |  [0x08e120, 0x08e68b)
 *   ResetRx        .text 0x08e570  62    |
 *   SetAdaptEqV22  .text 0x08e5b0  95    |
 *   TxClockSync    .text 0x08e610  22    |
 *   CarrierDetect  .text 0x08e630  14    |
 *   SignalDetect   .text 0x08e640  14    |
 *   GetSignalQuality .text 0x08e650 25  |
 *   ScramblerOn    .text 0x08e670  11    |
 *   DescramblerOn  .text 0x08e680  11   /
 *   B103FP_create  .text 0x08e690 2151  (B103.c's first)
 *
 * All fourteen are GLOBAL entry points and none is called by any of the
 * others, so the object's `.text` order IS the unit's emission order and
 * therefore its source order -- the recovered order is the address order
 * above, and the functions are written in exactly that order.
 *
 * THE FUNCTIONS WERE SPLIT ACROSS FOUR RECONSTRUCTION FILES, and those four
 * are LAYERS, not translation units: `v22rate.c` held four of them,
 * `v22data.c` three, `v22prc.c` five and `v22ctl.c` two, each file mixing
 * this unit's functions with functions from other V.22 units.  They are
 * reunited here because `-O3` only inlines within a translation unit and the
 * unit's emission order is a register-allocation carrier, so the boundary is
 * not bookkeeping.
 *
 * WHAT WAS MOVED, and where each came from (bodies verbatim, no rewrite):
 *
 *   SetTxRate, SetRxRate, DemodDataV22, ResetRx          <- v22rate.c
 *   ScrambleDataV22, DescrambleDataV22, ModDataV22       <- v22data.c
 *   SetAdaptEqV22, TxClockSync, CarrierDetect,
 *   SignalDetect, GetSignalQuality                       <- v22prc.c
 *   ScramblerOn, DescramblerOn                           <- v22ctl.c
 *
 * v22rate.c is now empty of functions and is deleted; v22data.c, v22prc.c
 * and v22ctl.c keep the functions the object puts in OTHER V.22 units, and
 * those functions are all defined BEFORE the moved ones in their files, so
 * no remaining function's emission-order context changes.
 *
 * The per-function notes below are the originals and are kept where they
 * were function-specific.
 */

#include "dsplib/v22rate.h"
#include "dsplib/v22data.h"
#include "dsplib/v22prc.h"
#include "dsplib/v22ctl.h"
#include "dsplib/v22fp.h"

#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v22_iir.h"
#include "dsplib/v22_mrf.h"
#include "dsplib/v22_pps.h"
#include "dsplib/v22_sre.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22dec.h"
#include "dsplib/v22txtab.h"

void
SetTxRate(struct v22fp *fp, short rate)
{
	struct v22fp_dsp *dsp;

	switch (rate) {
	case V22_RATE_1200:
		dsp = fp->dsp;
		/* FPM_SDM_init's width arithmetic, open-coded. */
		dsp->sdm.cfg.nbits = V22_SDM_BITS_1200;
		dsp->sdm.mask = (1 << V22_SDM_BITS_1200) - 1;
		dsp->sdm.notmask = ~((1 << V22_SDM_BITS_1200) - 1);
		dsp->sdm.reg = 0;
		dsp->sdm.shift1 = (short)(dsp->sdm.cfg.tap1
					  - V22_SDM_BITS_1200);
		dsp->sdm.shift2 = (short)(dsp->sdm.cfg.tap2
					  - V22_SDM_BITS_1200);
		/* No amplitude bits at 1200, so no quadrant shift either. */
		dsp->smc.cfg.amask = 0;
		dsp->smc.cfg.qshift = 0;
		dsp->pps.imap = SMCv22_IMAP_1200BPS;
		dsp->pps.qmap = SMCv22_QMAP_1200BPS;
		dsp->r28 = 0;
		break;
	case V22_RATE_2400:
		dsp = fp->dsp;
		dsp->sdm.cfg.nbits = V22_SDM_BITS_2400;
		dsp->sdm.mask = (1 << V22_SDM_BITS_2400) - 1;
		dsp->sdm.notmask = ~((1 << V22_SDM_BITS_2400) - 1);
		dsp->sdm.reg = 0;
		dsp->sdm.shift1 = (short)(dsp->sdm.cfg.tap1
					  - V22_SDM_BITS_2400);
		dsp->sdm.shift2 = (short)(dsp->sdm.cfg.tap2
					  - V22_SDM_BITS_2400);
		/* Two amplitude bits below two quadrant bits. */
		dsp->smc.cfg.amask = 3;
		dsp->smc.cfg.qshift = 2;
		dsp->pps.imap = SMCv22_IMAP_2400BPS;
		dsp->pps.qmap = SMCv22_QMAP_2400BPS;
		dsp->r28 = 1;
		break;
	default:
		break;
	}
}

void
SetRxRate(struct v22fp *fp, short rate)
{
	struct v22fp_dsp *dsp;

	switch (rate) {
	case V22_RATE_1200:
		dsp = fp->dsp;
		dsp->sdm2.cfg.nbits = V22_SDM_BITS_1200;
		dsp->sdm2.mask = (1 << V22_SDM_BITS_1200) - 1;
		dsp->sdm2.notmask = ~((1 << V22_SDM_BITS_1200) - 1);
		dsp->sdm2.reg = 0;
		dsp->sdm2.shift1 = (short)(dsp->sdm2.cfg.tap1
					   - V22_SDM_BITS_1200);
		dsp->sdm2.shift2 = (short)(dsp->sdm2.cfg.tap2
					   - V22_SDM_BITS_1200);
		dsp->r2a = 0;
		dsp->fse.decision = FSEv22_decision12;
		break;
	case V22_RATE_2400:
		dsp = fp->dsp;
		dsp->sdm2.cfg.nbits = V22_SDM_BITS_2400;
		dsp->sdm2.mask = (1 << V22_SDM_BITS_2400) - 1;
		dsp->sdm2.notmask = ~((1 << V22_SDM_BITS_2400) - 1);
		dsp->sdm2.reg = 0;
		dsp->sdm2.shift1 = (short)(dsp->sdm2.cfg.tap1
					   - V22_SDM_BITS_2400);
		dsp->sdm2.shift2 = (short)(dsp->sdm2.cfg.tap2
					   - V22_SDM_BITS_2400);
		dsp->r2a = 1;
		dsp->fse.decision = FSEv22_decision24;
		break;
	default:
		break;
	}
}

/*
 * Both scramblers are a two-instruction wrapper and a tail jump.  The only
 * thing that distinguishes them is the offset -- +0x30 for the transmit
 * scrambler, +0x1cc for the receive descrambler -- and which of the two
 * FPM_SDM entry points they jump to.  `V22FP_create` initialises an
 * `fpm_sdm` at each of those two offsets, so the pairing is confirmed
 * independently of these functions' names.
 */
void
ScrambleDataV22(void *modem, unsigned short *data, unsigned short count)
{
	struct v22fp *v22 = (struct v22fp *)modem;
	/*
	 * NO INTERMEDIATE LOCAL, and that is measured rather than a style
	 * choice.  Seven spellings of these two wrappers were compiled --
	 * `void *` local, no local, a typed sub-object local, both locals,
	 * an `unsigned char *` local, a `const` local, and the sub-object
	 * computed through a char pointer -- and this is the ONLY one that
	 * reproduces either function.  It is a unique preimage over an
	 * exhausted domain and it closes BOTH.
	 *
	 * What the local costs is the register: with it, GCC puts the
	 * sub-object pointer in %edx and pays the 6-byte `add $imm32,%edx`;
	 * without it the pointer lands in %eax and takes the 5-byte
	 * `add $imm32,%eax` short form the object uses.  In DescrambleDataV22
	 * that one byte is the whole size difference.  Finding F8120.
	 */
	FPM_SDM_scrambler(&v22->dsp->sdm, data, count);
}

void
DescrambleDataV22(void *modem, unsigned short *data, unsigned short count)
{
	struct v22fp *v22 = (struct v22fp *)modem;

	/* No intermediate local, for the reason ScrambleDataV22 records. */
	FPM_SDM_descrambler(&v22->dsp->sdm2, data, count);
}

/*
 * Bits to samples, in two stages that share one ring.
 *
 * THE RING IS THE POINT.  Both calls are handed `fp + 0xa0` as their second
 * argument -- the object computes `lea 0xa0(%eax),%edx` twice, once before
 * each -- so the symbol indices `FPM_SMC_encoder` writes are exactly the ones
 * `V22_PPS_filter` reads back.  `fpm_smc.h` records that two sessions
 * modelled half of that ring each, the producer's cursor and the consumer's;
 * this function is where the two halves meet.
 *
 * `count` is in DATA WORDS for the encoder and in SYMBOLS for the filter, and
 * the object passes the same value to both because the encoder makes exactly
 * one symbol per word.
 */
unsigned short
ModDataV22(void *modem, const unsigned short *data, short *out,
	   unsigned short count)
{
	struct v22fp *v22 = (struct v22fp *)modem;
	struct v22fp_dsp *dsp;

	dsp = v22->dsp;
	FPM_SMC_encoder(&dsp->smc, &dsp->smc_ring, data, count);

	dsp = v22->dsp;
	return (unsigned short)V22_PPS_filter(
			&dsp->pps, &dsp->smc_ring, out, count);
}

unsigned short
DemodDataV22(struct v22fp *fp, short *in, unsigned short *sym,
	     unsigned short count)
{
	struct v22fp_hdx *hdx = fp->hdx;
	short *iir = hdx->iir;
	unsigned short n_rate;	/* samples the resampler produced      */
	unsigned short n_sym;	/* symbol-clock samples it then found  */
	int agc_state;		/* dsp->agc.f18, needed after a call    */
	int adapt;		/* the equaliser's enable for this block */

	/*
	 * The IIR front end, mode 0 only.  The tone generator fills a block
	 * of V22_IIR_BLOCK shorts -- the filter's own block length -- but is
	 * asked for only `count` of them, so a short block leaves the tail of
	 * the mixer at whatever the stack held.  The object's.
	 */
	if (fp->dsp->r2e == V22_FRONTEND_IIR) {
		short mix[V22_IIR_BLOCK];

		FPM_TONE_generate_demod(hdx->tone, mix, (short)count);
		V22_iir_filt_demod(in,
				   iir + V22_IIR_OFF_B, iir + V22_IIR_OFF_A,
				   iir + V22_IIR_OFF_X, iir + V22_IIR_OFF_Y,
				   mix);
	}

	n_rate = (unsigned short)V22_MRF_filter(&fp->dsp->mrf, in,
						fp->dsp->rx_scratch,
						(short)count);
	fp->dsp->rx_count = (short)n_rate;

	/*
	 * The disconnect check, skipped entirely when hdx->protocol is set.  Note
	 * the comparison is against `params.disconnect_thresh`, which
	 * `V22FP_create` loads from V22DiconnectThreshTable regardless of what
	 * the template held -- see v22fp.h.
	 */
	if (fp->hdx->protocol == 0
	    && fp->params.disconnect_thresh
	       > FPM_rms(fp->dsp->rx_scratch, n_rate)) {
		fp->dsp->sre.active = 0;
		return 0;
	}

	/*
	 * The object passes a FOURTH argument here, the constant 1, which
	 * FPM_AGC_agc does not have -- the same extra argument v22data.c's
	 * Detect_v22 and bwchdem.c record at their own call sites, and
	 * ignored in the same way.
	 */
	FPM_AGC_agc(&fp->dsp->agc, fp->dsp->rx_scratch, n_rate);

	/*
	 * `agc.f18` is read ONCE and used twice across the clock-recovery
	 * call, so it cannot be a re-read: the object keeps it in a
	 * callee-saved register.  fpm_agc.h has it as init's reset flag and
	 * nothing in fpm_agc writes it, so what it means here is the caller's
	 * business and it keeps its neutral name.
	 */
	agc_state = fp->dsp->agc.f18;
	fp->dsp->sre.adapt = fp->dsp->r04 & agc_state;

	n_sym = (unsigned short)V22_SRE_recover(&fp->dsp->sre,
						fp->dsp->rx_scratch, in,
						(short)n_rate);

	/*
	 * While the clock loop is still acquiring, the equaliser adapts on
	 * nothing; once it has settled, bit 0 of the AGC word decides.
	 */
	adapt = 0;
	if (fp->dsp->sre.acquiring == 0)
		adapt = agc_state & 1;

	fp->dsp->fse.pll_on = fp->dsp->r08;
	if (fp->dsp->sre.mode != 0) {
		fp->dsp->fse.r18 = fp->dsp->r0c & adapt;
		fp->dsp->fse.lms_on = adapt & fp->dsp->eq_adapt;
	} else {
		fp->dsp->fse.r18 = 0;
		fp->dsp->fse.lms_on = 0;
	}

	V22_FSE_receive(&fp->dsp->fse, in, sym, (short)n_sym);

	/* Not V22_FSE_receive's return, which the object throws away. */
	return (unsigned short)fp->dsp->fse.n_out;
}

void
ResetRx(struct v22fp *fp)
{
	V22_SRE_init(&fp->dsp->sre, 0);
	/* The equaliser as its own configuration; see v22rate.h. */
	V22_FSE_init(&fp->dsp->fse,
		     (const struct v22_fse_cfg *)&fp->dsp->fse, 0);
}

/*
 * Three modes, a `switch` in the object (compare, jg, dec, je -- GCC's shape
 * for a dense switch of three), and no default action.  Written as a switch
 * for the same reason.
 */
void
SetAdaptEqV22(void *modem, unsigned short mode)
{
	struct v22fp *v22 = (struct v22fp *)modem;
	struct v22fp_dsp *dsp;

	switch (mode) {
	case 1:
		dsp = v22->dsp;
		dsp->eq_adapt = 0;
		break;
	case 2:
		dsp = v22->dsp;
		dsp->eq_adapt = 1;
		dsp->fse.mu_sel = 0;
		break;
	case 3:
		dsp = v22->dsp;
		dsp->eq_adapt = 1;
		dsp->fse.mu_sel = 1;
		/* Set here and cleared by nothing -- mode 2 leaves it alone. */
		dsp->fse.lms_on = 1;
		break;
	default:
		break;
	}
}

void
TxClockSync(void *modem)
{
	struct v22fp *v22 = (struct v22fp *)modem;
	struct v22fp_dsp *dsp = v22->dsp;
	short baud = dsp->sre.pll_acc;

	dsp->pps.cfg.step = (unsigned short)(short)(baud * 3);
}

int
CarrierDetect(void *modem)
{
	struct v22fp *v22 = (struct v22fp *)modem;

	return v22->dsp->sre.active;
}

int
SignalDetect(void *modem)
{
	struct v22fp *v22 = (struct v22fp *)modem;

	return v22->dsp->agc.signal;
}
/*
 * Quality as a distance from the rail: 0x8000 minus the stored figure,
 * truncated to sixteen bits and returned unsigned.
 *
 * The object loads the constant as 0xffff8000 -- that is, -32768 in a 32-bit
 * register -- subtracts, and then zero-extends the low half.  So a stored
 * figure of 0 gives 32768 and one of 0x8000 gives 0.  The wraparound is real
 * and reachable: any stored figure above 0x8000 gives a LARGE answer, not a
 * negative one.  Preserved, and the differential test sweeps the whole
 * sixteen-bit domain rather than sampling it.
 */
unsigned short
GetSignalQuality(void *modem)
{
	struct v22fp *v22 = (struct v22fp *)modem;

	return (unsigned short)(-32768
		- (int)(unsigned short)v22->dsp->fse.mse);
}

int
ScramblerOn(struct v22fp *fp)
{
	return fp->dsp->scrambler_on;
}

int
DescramblerOn(struct v22fp *fp)
{
	return fp->dsp->descrambler_on;
}

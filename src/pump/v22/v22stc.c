/*
 * v22status.c -- V.22 / V.22bis: the status/control pair.
 *
 * The blob's v22stc.c translation unit.  Both bodies moved VERBATIM;
 * V22FP_control came out of the over-split v22ctl.c layer, and V22_status was
 * already here.  The emission order is the object's: V22FP_control at
 * 0x08c3b0, then V22_status at 0x08c450.
 *
 * The control body acquires the DSP owner before the first flag pair and
 * again after the HDX updates. The DSP-owner view preserves that native
 * alias boundary; each conditional pair captures its HDX owner once.
 */

#include "dsplib/v22status.h"

#include "dsplib/fpm_agc.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22ctl.h"
#include "dsplib/v22fp.h"

/*
 * LOCAL in the object, so `static` here.  Seven entries, indexed by
 * `hdx->protocol` with no bounds check; see the header on what the values are not
 * known to mean.
 */
static const short PROTOCOL[7] = {
	3, 0, 1, 2, 7, 8, 5
};

int
V22FP_control(struct v22fp *fp, const struct v22fp_ctl *ctl)
{
	unsigned char first_flags, late_flags, hdx_flags;
	int scrambler;
	struct v22fp_dsp *dsp;
	/*
	 * The two scrambler switches, from the two low bits.  Both are read
	 * out of one byte load, which is why they are taken in this order --
	 * the object shifts the loaded byte for bit 1 and masks the original
	 * for bit 0.
	 */
	first_flags = ctl->flags_0c;
	scrambler = first_flags & 1;
	dsp = fp->dsp;
	dsp->descrambler_on = (first_flags >> 1) & 1;
	/* Acquire the HDX request before publishing the scrambler result. */
	hdx_flags = ctl->flags_0d;
	dsp->scrambler_on = scrambler;

	/*
	 * The half-duplex pair, twice over.  The flag is tested first and the
	 * two-bit field second, so a control byte carrying both leaves the
	 * field's values in place; that ordering is the object's and is
	 * observable, which is why it is not tidied into an if/else.
	 */
	if (hdx_flags & V22_CTL_RETRAIN) {
		struct v22fp_hdx *hdx = fp->hdx;
		hdx->protocol = V22_PROTOCOL_RETRAIN;
		hdx->connect_substate = 1;
		/* Those stores may overlap the request; the other path reuses it. */
		hdx_flags = ctl->flags_0d;
	}
	if ((hdx_flags >> V22_CTL_HDX_SHIFT) == V22_CTL_HDX_ORG_RMLOOP2) {
		struct v22fp_hdx *hdx = fp->hdx;
		hdx->protocol = V22_PROTOCOL_ORG_RMLOOP2;
		hdx->connect_substate = 0;
	}

	late_flags = ctl->flags_0c;
	dsp = fp->dsp;
	dsp->r20 = (late_flags >> 2) & 1;
	/* Inverted; see V22_CTL_FREEZE_ADAPT. */
	dsp->agc.f18 = (late_flags & V22_CTL_FREEZE_ADAPT) ? 0 : 1;

	/* Native byte +0x11 write; the whole-word parameter view is retained. */
	fp->params.flag_bits.bit9 = late_flags >> 7;

	/* A literal on every path, not a status. */
	return 1;
}

int
V22_status(struct v22fp *fp, struct v22_status *st)
{
	int err;
	int protocol = fp->hdx->protocol;
	struct v22fp_dsp *dsp = fp->dsp;

	st->protocol = PROTOCOL[protocol];

	st->tx_bps = dsp->r28 == 0 ? V22_STATUS_BPS_1200
				       : V22_STATUS_BPS_2400;

	/*
	 * THE SCALE IS APPLIED AT 1200 AND NOT AT 2400, and the two arms share
	 * everything below.  See the header: this is the object's, it is not a
	 * transcription slip, and it is worth an `if` with a duplicated store
	 * rather than a conditional inside the expression because that is the
	 * shape the object has.
	 */
	if (dsp->r2a == 0) {
		st->rx_bps = V22_STATUS_BPS_1200;
		err = (dsp->fse.mse * V22_STATUS_QUALITY_SCALE_Q14) >> 14;
	} else {
		st->rx_bps = V22_STATUS_BPS_2400;
		err = dsp->fse.mse;
	}

	st->short_08 = 0;
	/* No clamp; the report goes negative.  32-bit, narrowed by the store. */
	st->quality = (short)(V22_STATUS_QUALITY_BASE - err);
	st->short_0a = 0;
	st->short_10 = 0;
	st->short_12 = 0;

	st->flag_bits.scrambler = dsp->scrambler_on;
	st->flag_bits.descrambler = fp->dsp->descrambler_on;
	st->flag_bits.r20 = fp->dsp->r20;
	st->flag_bits.r00_off = fp->dsp->r00 == 0;
	st->flag_bits.r0c_off = fp->dsp->r0c == 0;
	st->flag_bits.eq_frozen = fp->dsp->eq_adapt == 0;
	st->flag_bits.bit6 = 1;
	st->flag_bits.param_bit9 = fp->params.flag_bits.bit9;
	st->flags2_bits.param_bit10 = fp->params.flag_bits.bit10;

	/* A literal on every path, not a status. */
	return 1;
}

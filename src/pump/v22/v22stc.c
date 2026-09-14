/*
 * v22status.c -- V.22 / V.22bis: the connection status report.
 * See include/dsplib/v22status.h, which carries the evidence.
 *
 * THE INSTANCE POINTER IS READ AGAIN FOR EVERY FIELD, and it is not a style
 * choice: the object emits `mov 0x54(%esi),%ebx` five separate times inside
 * the flag sequence, with the instance itself live in a callee-saved register
 * throughout.  A local holding the sub-object pointer would have been kept in
 * that register instead.  src/pump/v22/v22data.c records the same shape for
 * the same author's neighbouring functions.
 */

#include "dsplib/v22status.h"

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
V22_status(struct v22fp *fp, struct v22_status *st)
{
	int err;

	st->protocol = PROTOCOL[fp->hdx->protocol];

	st->tx_bps = fp->dsp->r28 != 0 ? V22_STATUS_BPS_2400
				       : V22_STATUS_BPS_1200;

	/*
	 * THE SCALE IS APPLIED AT 1200 AND NOT AT 2400, and the two arms share
	 * everything below.  See the header: this is the object's, it is not a
	 * transcription slip, and it is worth an `if` with a duplicated store
	 * rather than a conditional inside the expression because that is the
	 * shape the object has.
	 */
	if (fp->dsp->r2a != 0) {
		st->rx_bps = V22_STATUS_BPS_2400;
		err = fp->dsp->fse.mse;
	} else {
		st->rx_bps = V22_STATUS_BPS_1200;
		err = (fp->dsp->fse.mse * V22_STATUS_QUALITY_SCALE_Q14) >> 14;
	}

	st->short_08 = 0;
	/* No clamp; the report goes negative.  32-bit, narrowed by the store. */
	st->quality = (short)(V22_STATUS_QUALITY_BASE - err);
	st->short_0a = 0;
	st->short_10 = 0;
	st->short_12 = 0;

	/* One bit at a time, whole byte stored each time; see the header. */
	st->flags = (unsigned char)((st->flags & ~V22_STATUS_SCRAMBLER)
				    | (fp->dsp->scrambler_on & 1));
	st->flags = (unsigned char)((st->flags & ~V22_STATUS_DESCRAMBLER)
				    | ((fp->dsp->descrambler_on & 1) << 1));
	st->flags = (unsigned char)((st->flags & ~V22_STATUS_R20)
				    | ((fp->dsp->r20 & 1) << 2));
	st->flags = (unsigned char)((st->flags & ~V22_STATUS_R00_OFF)
				    | ((fp->dsp->r00 == 0) << 3));
	st->flags = (unsigned char)((st->flags & ~V22_STATUS_R0C_OFF)
				    | ((fp->dsp->r0c == 0) << 4));
	st->flags = (unsigned char)((st->flags & ~V22_STATUS_EQ_FROZEN)
				    | ((fp->dsp->eq_adapt == 0) << 5));
	st->flags = (unsigned char)(st->flags | V22_STATUS_ALWAYS);
	st->flags = (unsigned char)((st->flags & ~V22_STATUS_PARAM_BIT9)
				    | (((fp->params.flags & V22_PARAMS_BIT9)
					!= 0) << 7));

	/* Only bit 0; the caller's other seven survive. */
	st->flags2 = (unsigned char)((st->flags2 & ~V22_STATUS2_PARAM_BIT10)
				     | ((fp->params.flags & V22_PARAMS_BIT10)
					!= 0));

	/* A literal on every path, not a status. */
	return 1;
}

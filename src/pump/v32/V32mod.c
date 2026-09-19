/* V32mod.c -- original V.32 protocol-handler translation unit. */
#include "v32fpdisp-common.h"
static unsigned short tx_in_internal[V32_BIT_BUFFER];
static unsigned short rx_out_internal[V32_BIT_BUFFER];

/*
 * V32FP_modem -- .text 0x082630.
 *
 * One block.  The two counts are in/out and they CROSS OVER: `nout` arrives as
 * a count of transmit BITS and leaves as a count of output SAMPLES, `nin`
 * arrives as input samples and leaves as receive bits.  Everything around the
 * dispatch is marshalling -- the caller's `int` arrays are narrowed into the
 * two `short` statics and widened back.
 *
 * THE RETURN IS A 32-BIT LOAD OF obj + 0x30 (82779), where every other access
 * to that byte in this tree is `movb` or `movzbl`.  Reproduced as a 32-bit
 * read: the status is the low byte and `v32.c` masks it, so `V32_OBJ_FLAGS`
 * and the two bytes above it reach the caller and are discarded.
 */
int
V32FP_modem(struct v32_modem *modem, const int *txbits, short *out, const short *in,
	    int *rxbits, int *nout, int *nin)
{
	struct v32_fp *fp;
	struct v32_hdx *hdx;
	short nsamples;
	unsigned short rxcount;
	int scale;
	int n;
	int i;

	nsamples = (short)*nout;
	rxcount = (unsigned short)*nin;

	fp = FP(modem);
	if (*nin > 0) {
		short *clean = (short *)fp->clean_buf;

		for (i = 0; i < *nin; i++)
			clean[i] = in[i];
	}
	fp->clean_n = (unsigned short)*nin;

	for (i = 0; i < *nout; i++)
		tx_in_internal[i] = (unsigned short)txbits[i];

	modem->flags &= (unsigned char)~V32_FLAG_FAULT;
	hdx = HDX(modem);
	V32_PROTOCOL[hdx->mode](modem, tx_in_internal, out,
						  (short *)in, rx_out_internal,
						  &nsamples, &rxcount);

	*nout = (int)(unsigned short)nsamples;
	*nin = (int)rxcount;

	for (i = 0; i < *nin; i++)
		rxbits[i] = (int)rx_out_internal[i];

	if ((modem->flags & V32_FLAG_DATA) != 0) {
		hdx = HDX(modem);
		hdx->mode = V32_PROTO_DATA;
		hdx->state = V32_STATE_DONT_CARE;
	}

	n = *nout;
	if (n > 0) {
		scale = PARAMS(modem)->tx_scale;
		for (i = 0; i < n; i++)
			out[i] = (short)((out[i] * scale) >> 15);
	}

	return modem->status_word;
}

/* ------------------------------------------------------------------------ */

/*
 * v32_data -- .text 0x0827a0.  `V32_PROTOCOL`'s data-mode handler, and its
 * argument list is `v32_handshake`'s because one table dispatches to both.
 *
 * ---------------------------------------------------------------------------
 * THE DEFERRED CONTROL REQUEST, AND THE BUG IN IT
 *
 * When `RetrainDetectV32` fires, this function builds a `struct v32fp_ctl`
 * from `V32_CTL` IN A STACK LOCAL, sets `Control_Flag`, and returns without
 * issuing it.  The call is made on the NEXT block, from the test at the top of
 * the function -- by which time the local it built is gone and the block
 * handed to `V32FP_control` is whatever is on the stack.  The renegotiation
 * arm two lines below does NOT defer and is correct.
 *
 * `docs/deviations.md` and finding F8645 have it; `V32FP_control`'s `Patch:`
 * debug line is the author's own workaround, putting the one bit that mattered
 * back from the status code.  **No differential test can drive that arm**,
 * because the two sides read two different stack frames.
 *
 * ---------------------------------------------------------------------------
 * THE CARRIER-LOSS TIMER IS FIVE MILLISECONDS A BLOCK
 *
 * hdx + 0xae counts blocks, and the comparison is `count * 5` against
 * `params.energy_drop_time`.  The format string is the author's --
 * `carrier_loss_time %d of %d ms` -- so the unit is MILLISECONDS and five of
 * them is one block, which is exactly `v32.c`'s 40 samples at 8000 Hz.  It is
 * also what makes `energy_drop_time`'s 700 a duration.  Finding F8648.
 *
 * LOCAL in the blob (`t`), so `static` here; `V32_PROTOCOL` holds its
 * address, so the convention stays the ordinary one and the differential
 * test declares it the ordinary way.
 */
static void
v32_data(struct v32_modem *modem, unsigned short *txdata, short *txout, short *rxin,
	 unsigned short *rxout, short *nsamples, unsigned short *rxcount)
{
	struct v32_status st;
	struct v32fp_ctl ctl;
	struct v32_hdx *hdx;
	int code;
	int i;

	V32FP_status(modem, &st);
	code = 0;

	if (Control_Flag == 1) {
		/* `ctl` is uninitialised here.  See the note above. */
		V32FP_control(modem, &ctl);
		Control_Flag = 0;
		return;
	}

	hdx = HDX(modem);
	if (hdx->symbol_len > 0) {
		short *buf = (short *)hdx->buffer;

		for (i = 0; i < hdx->symbol_len; i++)
			buf[i] = (short)txdata[i];
	}

	if ((st.flags & V32_STFLAG_SCRAMBLE) != 0)
		ScrambleDataV32(modem, (short *)hdx->buffer,
				(unsigned short)*nsamples);

	*rxcount = DemodDataV32(modem, rxin, rxout, *rxcount);
	*nsamples = (short)ModDataV32(modem,
				      (short *)HDX(modem)->buffer,
				      txout, (unsigned short)*nsamples);

	if ((st.flags & V32_STFLAG_DESCRAMBLE) != 0)
		DescrambleDataV32(modem, (short *)rxout, *rxcount);
	if ((st.flags & V32_STFLAG_TXCLOCK) != 0)
		TxClockSyncV32(modem);
	if ((st.flags & V32_STFLAG_RETRAIN_DET) != 0
	    && RetrainDetectV32(modem) != 0) {
		modem->flags &= (unsigned char)~V32_FLAG_DATA;
		ctl = V32_CTL;
		ctl.ctl0 = (unsigned char)
			(((((ctl.ctl0 & 0xfc) | (st.flags & 1)
			    | (st.flags & 2)) & 0xfb) | (st.flags & 4)) | 0x80);
		ctl.ctl1 |= V32_CTL1_RETRAIN;
		code = V32_MSG_RETRAIN_REQ;
		Control_Flag = 1;
	}

	if (RenegotiateDetectV32(modem) != 0) {
		ctl = V32_CTL;
		ctl.ctl0 = (unsigned char)
			((((ctl.ctl0 & 0xfc) | (st.flags & 1)
			   | (st.flags & 2)) & 0xfb) | (st.flags & 4));
		ctl.ctl1 |= V32_CTL1_RENEG;
		ctl.r18 = 0;
		ctl.r1c = 0;
		HDX(modem)->mode = V32_MODE_RING_RESP;
		V32FP_control(modem, &ctl);
		code = V32_MSG_RENEG;
	}

	if ((modem->flags & V32_FLAG_RETRAIN) != 0) {
		HDX(modem)->loss_blocks = 0;
	} else {
		short elapsed;

		hdx = HDX(modem);
		hdx->loss_blocks = (unsigned short)
			(hdx->loss_blocks + 1);
		elapsed = (short)((short)hdx->loss_blocks
				  * V32_BLOCK_MS);
		if (PARAMS(modem)->energy_drop_time >= elapsed) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"V32_MSG_NO_CARRIER won't be reported "
					"(carrier_loss_time %d of %d ms)\n",
					elapsed,
					PARAMS(modem)->energy_drop_time);
		} else {
			code = V32_MSG_NO_CARRIER;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V32_MSG_NO_CARRIER\n");
		}
	}

	/*
	 * Two bits at once, and NEITHER IS NAMED: the object writes the
	 * immediate 0x0c and nothing reconstructed reads either bit, so a
	 * name here would be an offset in disguise.  Recorded as the object's
	 * own constant.
	 */
	modem->flags |= (unsigned char)0x0c;
	modem->status = (unsigned char)code;
}

/*
 * ---------------------------------------------------------------------------
 * `v32_handshake` AND `v32_null_protocol`: the two protocol handlers that were
 * their own files (`v32hshake.c`, and a tail of `v32fpctl.c`).
 *
 * FILE-LOCAL IN THE OBJECT.  The reference groups both with `v32_data` and the
 * `V32_PROTOCOL` table in one unit (`V32mod.c`); the three are the table's
 * slots 0..8, so they are `static` here where the table is.  A test names them
 * through the test tier's globalized copies (tools/testvisible.py); their
 * addresses are taken by the table, so the calling convention is unchanged.
 *
 * `v32_handshake` -- .text 0x082b00, 203 bytes.  Seven interleaved arguments
 * whose names come from the two callees (grade 2, finding F8201): it clears
 * bit 0x01 of `obj + 0x31`, clears 0x04/0x08 as well only when `hdx->mode` is
 * `V32_MODE_RING_INIT`, copies the caller's transmit words into the context's
 * own buffer at hdx + 0xa4 (`symbol_len` words, a SIGNED 16-bit count whose
 * index wraps), runs the receive state, then RE-READS the context and
 * tail-calls the transmit driver -- so a receive state that replaced the whole
 * context sends the NEW context's buffer.  Findings F8239, F614 and F7803.
 *
 * `v32_null_protocol` -- one byte of `ret`.  Nothing in `.text` references it;
 * its one reference is `V32_PROTOCOL` slot 3, 7 and 8, and `void (void)` is a
 * placeholder for a signature that is not recoverable.
 */
#define V32_FLAG_01		0x01
#define V32_FLAG_04		0x04
#define V32_FLAG_08		0x08

static void
v32_handshake(struct v32_modem *modem, unsigned short *txdata, short *txout, short *rxin,
	      unsigned short *rxout, short *nsamples, unsigned short *rxcount)
{
	struct v32_hdx *hdx;
	unsigned char flags;
	short i;

	hdx = modem->hdx;

	flags = modem->flags;
	if (hdx->mode == V32_MODE_RING_INIT)
		flags = (unsigned char)(flags & ~(V32_FLAG_04 | V32_FLAG_08));
	modem->flags = (unsigned char)(flags & ~V32_FLAG_01);

	if (hdx->symbol_len > 0) {
		unsigned short *buf;

		buf = (unsigned short *)hdx->buffer;
		i = 0;
		do {
			buf[i] = txdata[i];
			i = (short)(i + 1);
		} while (hdx->symbol_len > i);
	}

	V32RxHdxModem(modem, rxin, rxout, rxcount);

	/*
	 * RE-READ, and it matters: the receive state may have replaced the
	 * whole context, exactly as `V32TxHdxModem`'s own loop allows.
	 */
	hdx = modem->hdx;
	V32TxHdxModem(modem, (short *)hdx->buffer, txout,
		      nsamples);
}

static void
v32_null_protocol(void)
{
}

/*
 * `V32_PROTOCOL`, .data 0x007700, GLOBAL.  Nine slots indexed by
 * `V32HDX_MODE`, resolved from the nine `R_386_32 .text` relocations and NOT
 * from the file's bytes, which are zero -- the trap `tools/dis.py` exists for.
 *
 * FIVE slots are the handshake, one is the data-mode handler and THREE are the
 * one-byte `ret`.  Finding F8649 corrects F8594, which read it as six and one
 * and did not count the null ones.
 */
v32_protocol_fn V32_PROTOCOL[9] = {
	v32_handshake,				/* 0  V32_MODE_ORIGINATE    */
	v32_handshake,				/* 1  V32_MODE_ANSWER       */
	v32_handshake,				/* 2  V32_MODE_LOCLOOP_2    */
	(v32_protocol_fn)v32_null_protocol,	/* 3  V32_MODE_LOCLOOP_3    */
	v32_handshake,				/* 4  V32_MODE_RING_INIT    */
	v32_handshake,				/* 5  V32_MODE_RING_RESP    */
	v32_data,				/* 6  V32_PROTO_DATA        */
	(v32_protocol_fn)v32_null_protocol,	/* 7                        */
	(v32_protocol_fn)v32_null_protocol	/* 8                        */
};

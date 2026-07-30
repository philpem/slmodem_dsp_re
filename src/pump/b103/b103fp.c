/*
 * b103fp.c -- Bell 103 / V.21 Fixed Point: the modulation proper.
 *
 * Reconstructed from dsplibs.o b103fp.c:
 *   ModDataB103        .text 0x08f9a0    99 bytes
 *   TxNoCarrierB103    .text 0x08fa10   124 bytes
 *   CarrierDetectB103  .text 0x08fcd0    16 bytes
 *
 * The transmitter is two calls and nothing else.  All the interesting
 * decisions were made in B103FP_create, which sized the FSK modulator at 24
 * samples per symbol -- 300 baud at 7200 Hz -- and configured the rate
 * converter 10:9 to lift that to the 8000 the datapump interface speaks.
 * See findings 17 and 24.
 */

#include "dsplib/b103fp.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_tone.h"

/*
 * Modulate, then resample.
 *
 * The intermediate lands in the shared scratch buffer at dsp->scratch, which
 * is 324 bytes -- 162 samples, or six bits' worth at 24 samples each.  Nothing
 * here checks that `nbits` fits; B103FP_modem is what keeps the request small
 * enough, and the original has no guard either.
 */
short
ModDataB103(struct b103fp *fp, const unsigned short *bits, short *out,
	    unsigned short nbits)
{
	struct b103_dsp *dsp = fp->dsp;
	unsigned short nsamples;

	nsamples = (unsigned short)FPM_FSM_modulate(&dsp->fsm, bits,
						    dsp->scratch, nbits);

	return (short)(unsigned short)FPM_MRF_filter(&dsp->tx_mrf,
						     dsp->scratch, out,
						     nsamples);
}

/*
 * Silence, with the transmitter still running.
 *
 * Identical to ModDataB103 except that the modulator's output scale is forced
 * to zero for the duration and restored afterwards.  That matters: the phase
 * accumulator, the symbol counter and the resampler history all advance
 * exactly as they would have, so when the carrier comes back it comes back in
 * phase and on the same symbol boundary.  Muting the output buffer instead
 * would leave the modulator's state behind by however long the gap was.
 *
 * Note the scale is saved and restored around the FSM call only -- the
 * resampler runs with whatever it was given, which by then is silence anyway.
 */
short
TxNoCarrierB103(struct b103fp *fp, const unsigned short *bits, short *out,
		unsigned short nbits)
{
	struct b103_dsp *dsp = fp->dsp;
	short saved_scale = dsp->fsm.scale;
	unsigned short nsamples;

	dsp->fsm.scale = 0;
	nsamples = (unsigned short)FPM_FSM_modulate(&dsp->fsm, bits,
						    dsp->scratch, nbits);
	dsp->fsm.scale = saved_scale;

	return (short)(unsigned short)FPM_MRF_filter(&dsp->tx_mrf,
						     dsp->scratch, out,
						     nsamples);
}

/*
 * Carrier present.
 *
 * A bitwise AND of the two receiver flags, not a logical one -- the original
 * is literally `and %ecx,%eax`.  Both are set to 0 or 1 by the receiver, so
 * the two agree in practice; reproduced as written because nothing guarantees
 * it stays that way.
 */
int
CarrierDetectB103(struct b103fp *fp)
{
	struct b103_dsp *dsp = fp->dsp;

	return dsp->rx_tone & dsp->rx_energy;
}

/*
 * The structs above are laid out by hand around a handful of known offsets.
 * If a reserved region is ever resized to name a new field, these fail at
 * compile time rather than silently moving the transmit path's fields.
 *
 * 32-BIT ONLY, and necessarily so.  Unlike the rest of this tree, these two
 * structs are not merely *tested* against a 32-bit object -- they describe
 * one.  Their reserved regions are byte counts measured from a build where
 * pointers are four bytes, so under any other ABI the named fields land
 * somewhere else and the assertions below are simply false rather than
 * violated.  `make check64` exists to prove the *code* compiles cleanly for a
 * 64-bit target, not that this layout survives one; it cannot, and nothing in
 * this tree needs it to.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define B103_ASSERT_OFF(type, field, off) \
	typedef char b103_off_##field[ \
		((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

B103_ASSERT_OFF(struct b103_dsp, rx_energy, 0x04);
B103_ASSERT_OFF(struct b103_dsp, rx_tone, 0x08);
B103_ASSERT_OFF(struct b103_dsp, agc, 0x0c);
B103_ASSERT_OFF(struct b103_dsp, det_agc, 0x38);
B103_ASSERT_OFF(struct b103_dsp, tx_mrf, 0x64);
B103_ASSERT_OFF(struct b103_dsp, rx_mrf, 0x80);
B103_ASSERT_OFF(struct b103_dsp, fsd, 0x9c);
B103_ASSERT_OFF(struct b103_dsp, fsm, 0xd4);
B103_ASSERT_OFF(struct b103_dsp, scratch, 0xe8);
B103_ASSERT_OFF(struct b103_dsp, rx_scratch, 0xec);
B103_ASSERT_OFF(struct b103_dsp, p_f0, 0xf0);
B103_ASSERT_OFF(struct b103_dsp, rx_state, 0xfc);
B103_ASSERT_OFF(struct b103_hdx, tx, 0x08);
B103_ASSERT_OFF(struct b103_hdx, rx, 0x10);
B103_ASSERT_OFF(struct b103_hdx, tone_detect, 0x1c);
B103_ASSERT_OFF(struct b103_hdx, tone_lo, 0x20);
B103_ASSERT_OFF(struct b103fp, is_answer, 0x04);
B103_ASSERT_OFF(struct b103fp, status, 0x1c);
B103_ASSERT_OFF(struct b103fp, flags, 0x1d);
B103_ASSERT_OFF(struct b103_hdx, tone_timeout, 0x02);
B103_ASSERT_OFF(struct b103_hdx, tx_blocks, 0x04);
B103_ASSERT_OFF(struct b103_hdx, rx_count, 0x0c);
B103_ASSERT_OFF(struct b103_hdx, substate, 0x14);
B103_ASSERT_OFF(struct b103fp, hdx, 0x50);
B103_ASSERT_OFF(struct b103fp, dsp, 0x54);

typedef char b103_dsp_size[(sizeof(struct b103_dsp) == 0x100) ? 1 : -1];
typedef char b103fp_size[(sizeof(struct b103fp) == 0x58) ? 1 : -1];
typedef char b103_hdx_size[(sizeof(struct b103_hdx) == 0x24) ? 1 : -1];

#endif /* 32-bit */

/*
 * ---------------------------------------------------------------------------
 * DemodDataB103 -- the receive chain.  .text 0x08fa90, 568 bytes.
 *
 * This is the function that explains the receiver's frequency plan, and it is
 * not what a first reading of the filters suggests.  The incoming signal is
 * **mixed down with a local oscillator** before anything else:
 *
 *     in[i] = (in[i] * lo[i]) >> 14
 *
 * where `lo` comes from FPM_TONE_generate_demod on the tone object at
 * hdx->tone_lo.  Everything downstream -- the 3:10 rate converter, the AGC,
 * the demodulator's discriminator -- therefore works on a *baseband* signal,
 * which is why measuring their response and reading it as a radio-frequency
 * passband gives an answer that fits no Bell 103 tone pair (finding 32).  The
 * oscillator frequency is what selects the received channel.
 *
 * ---------------------------------------------------------------------------
 * Two AGCs, and why
 *
 * There are two independent gain controls in the DSP block, and they see
 * different signals:
 *
 *   dsp->det_agc   runs on an untouched COPY of the input, before the mixer,
 *                  and feeds only the guard-tone detector.
 *   dsp->agc       runs after the mixer and the rate converter, and feeds the
 *                  demodulator.
 *
 * Keeping them apart is what lets the data path's gain be FROZEN at the moment
 * of acquisition without blinding the tone detector -- see the state machine.
 *
 * ---------------------------------------------------------------------------
 * The acquisition state machine, in dsp->rx_state
 *
 * What it is waiting for is the **2100 Hz answer tone**.  The detector at
 * hdx->tone_detect is built from FPM_TONE_CFG, which is the ITU-T V.25 ANSam
 * configuration, and measuring its response confirms it: a 2100 Hz input
 * reports FPM_TONE_PRESENT and 2000 or 2200 Hz do not (finding 33).  So this
 * is a calling modem listening for the answering modem to reply.
 *
 *      <= 14   listening.  Each block in which the 2100 Hz tone dominates
 *              advances the state by five; anything else -- other signal, or
 *              no signal at all -- resets it to zero.  Three consecutive
 *              detections therefore reach 15.
 *      == 15   freeze the data AGC, then demodulate.  The gain is locked at
 *              whatever acquisition settled on, which is the point of doing
 *              it on a steady tone.
 *      >= 15   demodulating; the state parks at 16 and the detector is never
 *              consulted again.
 */
short
DemodDataB103(struct b103fp *fp, short *in, unsigned short *bits_out,
	      unsigned short count)
{
	struct b103_dsp *dsp = fp->dsp;
	/*
	 * The local oscillator's samples.  The original's frame leaves room
	 * for 160 shorts here and bounds `count` against it nowhere; 160 is
	 * one 20 ms fragment at 8 kHz, which is what dp_wrapper delivers.
	 */
	short lo[160];
	int nbits = 0;
	int i;

	if (dsp->rx_state <= 14) {
		/*
		 * The detector gets its own copy, because the mixer below
		 * overwrites `in`.
		 */
		for (i = 0; i < (short)count; i++)
			dsp->rx_scratch[i] = in[i];

		FPM_AGC_agc(&dsp->det_agc, dsp->rx_scratch, count);

		dsp->rx_tone = 0;
		if (FPM_TONE_detect(fp->hdx->tone_detect, dsp->rx_scratch,
				    (short)count) != FPM_TONE_PRESENT) {
			dsp->rx_state = 0;
		} else {
			dsp->rx_tone = 1;
			dsp->rx_state = (short)(dsp->rx_state + 5);
		}
	}

	/* Mix to baseband, in place. */
	FPM_TONE_generate_demod(fp->hdx->tone_lo, lo, (short)count);
	for (i = 0; i < (short)count; i++)
		in[i] = (short)((in[i] * lo[i]) >> 14);

	count = (unsigned short)FPM_MRF_filter(&dsp->rx_mrf, in, dsp->scratch,
					       (short)count);

	dsp->agc.f18 = dsp->r00;
	FPM_AGC_agc(&dsp->agc, dsp->scratch, count);
	dsp->rx_energy = dsp->agc.signal;

	if (dsp->rx_state == 15)
		FPM_AGC_Freeze(&dsp->agc);

	if (dsp->rx_state > 14) {
		dsp->rx_tone = 1;
		dsp->rx_state = 16;
		nbits = FPM_FSD_demodulate(&dsp->fsd, dsp->scratch, bits_out,
					   count);
	}

	return (short)nbits;
}

/*
 * ---------------------------------------------------------------------------
 * The half-duplex state machines.
 *
 * Seven states, one shared signature, and one shared shape:
 *
 *     do this state's work
 *     zero the caller's count
 *     move a counter
 *     if the counter says so, dispatch B103NextState[hdx->mode](fp)
 *
 * The dispatch is what advances `hdx->state`.  None of these functions
 * chooses its own successor -- that is entirely the business of the three
 * NextState tables, which is why the same seven states serve originate,
 * answer and local loopback.
 *
 * `count` is in/out.  Every state zeroes it after use, so the caller knows
 * the input was consumed even when the state produced nothing.
 */

/* Transmit: nothing to send yet, just advance. */
short
TxHdxStartB103(struct b103fp *fp, short *in, short *out, short *count)
{
	(void)in;
	(void)out;
	(void)count;

	B103NextState[fp->hdx->mode](fp);
	return 0;
}

/* Transmit: one block of data. */
short
TxHdxDataB103(struct b103fp *fp, short *in, short *out, short *count)
{
	short n;

	n = ModDataB103(fp, (const unsigned short *)in, out,
			(unsigned short)*count);
	*count = 0;
	return n;
}

/*
 * Transmit: a block of continuous mark.
 *
 * The bit buffer is FILLED with ones here rather than supplied, so the caller
 * hands over an empty buffer and a length.
 *
 * The exit condition differs by direction, and that difference is the whole
 * handshake: an answering modem stops after `tx_blocks` regardless, while a
 * calling modem also waits for its own receiver to have acquired
 * (dsp->rx_state past 14).  So the caller holds mark until it hears the
 * answering modem, which is exactly what Bell 103 call setup requires.
 */
short
TxHdxMarksB103(struct b103fp *fp, short *in, short *out, short *count)
{
	unsigned short *bits = (unsigned short *)in;
	struct b103_hdx *hdx;
	unsigned short i;
	short n;

	for (i = 0; i < (unsigned short)*count; i++)
		bits[i] = 1;

	n = ModDataB103(fp, bits, out, (unsigned short)*count);
	*count = 0;

	hdx = fp->hdx;
	hdx->tx_blocks = (short)(hdx->tx_blocks - 1);

	if (fp->is_answer) {
		if (hdx->tx_blocks <= 0)
			B103NextState[hdx->mode](fp);
	} else if (hdx->tx_blocks <= 0 && fp->dsp->rx_state > 14) {
		B103NextState[hdx->mode](fp);
	}

	return n;
}

/* Transmit: a block of silence, with the modulator still running. */
short
TxHdxSilenceB103(struct b103fp *fp, short *in, short *out, short *count)
{
	struct b103_hdx *hdx;
	short n;

	n = TxNoCarrierB103(fp, (const unsigned short *)in, out,
			    (unsigned short)*count);
	*count = 0;

	hdx = fp->hdx;
	hdx->tx_blocks = (short)(hdx->tx_blocks - 1);
	if (hdx->tx_blocks <= 0)
		B103NextState[hdx->mode](fp);

	return n;
}

/*
 * Receive: wait for the answer tone.
 *
 * Runs the acquisition AGC and the tone detector directly rather than going
 * through DemodDataB103 -- there is no point mixing and demodulating while
 * still waiting.  Note it uses the same two objects DemodDataB103's
 * acquisition phase does, dsp->det_agc and hdx->tone_detect.
 *
 * Two exits: the tone arrives (advance, if the substate says to), or
 * `tone_timeout` blocks pass without it (advance anyway, and report 5).
 */
short
RxDetMarkB103(struct b103fp *fp, short *in, short *out, short *count)
{
	struct b103_hdx *hdx = fp->hdx;
	struct b103_dsp *dsp = fp->dsp;

	(void)out;

	hdx->rx_count = (short)(hdx->rx_count + 1);

	FPM_AGC_agc(&dsp->det_agc, in, (unsigned short)*count);

	if (FPM_TONE_detect(hdx->tone_detect, in, *count) == FPM_TONE_PRESENT) {
		dsp->rx_state = (short)(dsp->rx_state + 5);
		if (hdx->substate == 1)
			B103NextState[hdx->mode](fp);
	} else if (hdx->rx_count >= hdx->tone_timeout) {
		hdx->substate = 5;
		B103NextState[hdx->mode](fp);
		fp->flags |= B103_FLAG_TIMEOUT;
		fp->status = 5;
	}

	*count = 0;
	return 0;
}

/*
 * Receive: demodulate while waiting for carrier.
 *
 * Counts DOWN, unlike RxDetMark: `rx_count` blocks are allowed before giving
 * up.  Carrier appearing advances immediately; running out advances too, but
 * reports 5.
 */
short
RxHdxStartB103(struct b103fp *fp, short *in, short *out, short *count)
{
	struct b103_hdx *hdx;

	DemodDataB103(fp, in, (unsigned short *)out, (unsigned short)*count);
	*count = 0;

	if (CarrierDetectB103(fp)) {
		B103NextState[fp->hdx->mode](fp);
		return 0;
	}

	hdx = fp->hdx;
	hdx->rx_count = (short)(hdx->rx_count - 1);
	if (hdx->rx_count <= 0) {
		hdx->substate = 5;
		B103NextState[hdx->mode](fp);
		fp->flags |= B103_FLAG_TIMEOUT;
		fp->status = 5;
	}

	return 0;
}

/*
 * Receive: carry data, and watch for the carrier going away.
 *
 * `rx_count` is reset to zero on every block with carrier and incremented on
 * every block without, so it counts CONSECUTIVE losses.  Eight in a row ends
 * the call with status 6.  A single dropout does not.
 */
short
RxHdxDataB103(struct b103fp *fp, short *in, short *out, short *count)
{
	struct b103_hdx *hdx;
	short nbits;

	nbits = DemodDataB103(fp, in, (unsigned short *)out,
			      (unsigned short)*count);
	*count = 0;

	fp->flags &= (unsigned char)~B103_FLAG_CARRIER;

	if (CarrierDetectB103(fp)) {
		fp->flags |= B103_FLAG_CARRIER;
		fp->hdx->rx_count = 0;
		fp->flags &= (unsigned char)~B103_FLAG_80;
		return nbits;
	}

	hdx = fp->hdx;
	hdx->rx_count = (short)(hdx->rx_count + 1);
	if (hdx->rx_count <= 7) {
		fp->flags &= (unsigned char)~B103_FLAG_80;
		return nbits;
	}

	B103NextState[hdx->mode](fp);
	fp->flags &= (unsigned char)~B103_FLAG_80;
	fp->status = 6;
	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * The three state tables.
 *
 * Each is a switch on hdx->substate that installs the next pair of half-duplex
 * states and sets the status the layer above reads.  The seven Hdx functions
 * never choose their own successor -- they only decide *when* to ask -- so the
 * same seven serve all three call types.
 *
 * The substate names are the original author's, recovered from the debug
 * strings the blob still carries at .rodata.str1.1+0x3d5c onward.  They label
 * the case, not the state being entered:
 *
 *     0  B103_STATE_START      1  B103_STATE_CARRDET
 *     2  B103_STATE_WAIT1      3  B103_STATE_WAIT2
 *     4  connected -- no case, falls through to "default"
 *
 * Answer and loopback skip WAIT2 and jump from WAIT1 straight to 4.
 *
 * The `status` values (2, 3, 4, 7) and the `flags` bits are B103's own, not
 * the DPSTAT_* codes in dp.h -- 7 here means connected, where DPSTAT_BUSY is
 * 7.  Whatever maps them lives above this layer; B103FP_modem is the
 * candidate.  Left as literals rather than given invented names.
 */

/*
 * Local loopback: transmit mark to yourself and demodulate it back.  No tone
 * detection at all -- the receive state is never set to RxDetMarkB103, it goes
 * straight to RxHdxDataB103.
 */
void
B103LocLoopNextState(struct b103fp *fp)
{
	struct b103_hdx *hdx = fp->hdx;

	switch (hdx->substate) {
	case B103_STATE_START:
		hdx->tx = TxHdxMarksB103;
		hdx->substate = B103_STATE_CARRDET;
		hdx->tx_blocks = (short)(hdx->tone_timeout * 2);
		hdx->rx_count = hdx->tone_timeout;
		fp->flags |= 0x10;
		fp->status = 2;
		break;

	case B103_STATE_CARRDET:
		hdx->tx_blocks = 8;
		hdx->rx = RxHdxDataB103;
		hdx->rx_count = 0;
		hdx->substate = B103_STATE_WAIT1;
		fp->flags &= (unsigned char)~0x40;
		fp->status = 3;
		break;

	case B103_STATE_WAIT1:
		hdx->tx = TxHdxMarksB103;
		hdx->substate = B103_STATE_DATA;
		fp->flags |= 0x2d;
		fp->status = 7;
		break;

	default:
		break;
	}
}

/*
 * Originate.  The four-step Bell 103 calling sequence:
 *
 *   START    transmit silence, listen for the answer tone (RxDetMark),
 *            giving up after tone_timeout blocks
 *   CARRDET  the tone arrived -- hold for eight more blocks
 *   WAIT1    transmit 40 blocks of mark, so the answerer can train
 *   WAIT2    data both ways
 */
void
B103OriginateNextState(struct b103fp *fp)
{
	struct b103_hdx *hdx = fp->hdx;

	switch (hdx->substate) {
	case B103_STATE_START:
		hdx->tx = TxHdxSilenceB103;
		hdx->rx_count = 1;
		hdx->rx = RxDetMarkB103;
		hdx->tx_blocks = hdx->tone_timeout;
		hdx->substate = B103_STATE_CARRDET;
		fp->flags |= 0x10;
		fp->status = 2;
		break;

	case B103_STATE_CARRDET:
		hdx->tx_blocks = 8;
		hdx->rx_count = 0;
		hdx->substate = B103_STATE_WAIT1;
		fp->flags &= (unsigned char)~0x40;
		fp->status = 3;
		break;

	case B103_STATE_WAIT1:
		hdx->tx_blocks = 40;
		hdx->rx_count = 0;
		hdx->tx = TxHdxMarksB103;
		hdx->substate = B103_STATE_WAIT2;
		fp->flags |= 0x24;
		fp->status = 4;
		break;

	case B103_STATE_WAIT2:
		hdx->tx = TxHdxDataB103;
		hdx->rx = RxHdxDataB103;
		hdx->substate = B103_STATE_DATA;
		fp->flags |= 0x09;
		fp->status = 7;
		break;

	default:
		break;
	}
}

/*
 * Answer.  Three steps rather than four: the answerer transmits mark from the
 * start (that IS the answer tone the caller is listening for) and jumps from
 * WAIT1 straight to data without an equivalent of WAIT2.
 */
void
B103AnswerNextState(struct b103fp *fp)
{
	struct b103_hdx *hdx = fp->hdx;

	switch (hdx->substate) {
	case B103_STATE_START:
		hdx->tx = TxHdxMarksB103;
		hdx->rx = RxDetMarkB103;
		hdx->rx_count = 0;
		hdx->tx_blocks = hdx->tone_timeout;
		hdx->substate = B103_STATE_CARRDET;
		fp->flags |= 0x10;
		fp->status = 2;
		break;

	case B103_STATE_CARRDET:
		hdx->tx_blocks = 8;
		hdx->rx_count = 8;
		hdx->substate = B103_STATE_WAIT1;
		fp->flags &= (unsigned char)~0x40;
		fp->status = 3;
		break;

	case B103_STATE_WAIT1:
		hdx->tx = TxHdxDataB103;
		hdx->rx = RxHdxDataB103;
		hdx->substate = B103_STATE_DATA;
		fp->flags |= 0x2d;
		fp->status = 7;
		break;

	default:
		break;
	}
}

/* Indexed by hdx->mode.  The order is the original's, from .data:0x77e8. */
void (*const B103NextState[3])(struct b103fp *fp) = {
	B103LocLoopNextState,		/* 0 */
	B103OriginateNextState,		/* 1 */
	B103AnswerNextState		/* 2 */
};

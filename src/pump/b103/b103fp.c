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
 * See findings F17 and F24.
 */

#include <string.h>

#include "dsplib/debug.h"
#include "dsplib/b103fp.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sysdep.h"

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
						     (short)nsamples);
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
	short saved_scale = dsp->fsm.cfg.scale;
	unsigned short nsamples;

	dsp->fsm.cfg.scale = 0;
	nsamples = (unsigned short)FPM_FSM_modulate(&dsp->fsm, bits,
						    dsp->scratch, nbits);
	dsp->fsm.cfg.scale = saved_scale;

	return (short)(unsigned short)FPM_MRF_filter(&dsp->tx_mrf,
						     dsp->scratch, out,
						     (short)nsamples);
}

/*
 * Carrier present.
 *
 * A bitwise AND of the two receiver flags, not a logical one -- the original
 * is literally `and %ecx,%eax`.  Both are set to 0 or 1 by the receiver, so
 * the two agree in practice; reproduced as written because nothing guarantees
 * it stays that way.
 *
 * THE OPERAND ORDER IS THE OBJECT'S AND IT WAS RECOVERED, NOT GUESSED.  `&`
 * is commutative and both operands are plain loads, so `rx_tone & rx_energy`
 * and `rx_energy & rx_tone` compute the same answer and no differential test
 * can separate them -- but the object loads +0x8 before +0x4, and only one of
 * the two spellings makes GCC 3.4.2 do that.  Written the other way round
 * this function was `same size, bytes differ`; written this way it is
 * BYTE-IDENTICAL to the object.  That is finding F617's acceptance test --
 * full identity, operands included -- and it passes, so this is the source
 * the author wrote rather than a permutation that happened to fit.
 */
int
CarrierDetectB103(struct b103fp *fp)
{
	struct b103_dsp *dsp = fp->dsp;

	return dsp->rx_energy & dsp->rx_tone;
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
B103_ASSERT_OFF(struct b103_dsp, mtd, 0xe4);
B103_ASSERT_OFF(struct b103_dsp, bpf_hist, 0xf0);
B103_ASSERT_OFF(struct b103_dsp, bpf, 0xf4);
B103_ASSERT_OFF(struct b103_dsp, bpf_idx, 0xf8);
B103_ASSERT_OFF(struct b103_dsp, bpf_taps, 0xfa);
B103_ASSERT_OFF(struct b103_dsp, rx_state, 0xfc);
B103_ASSERT_OFF(struct b103_hdx, tx, 0x08);
B103_ASSERT_OFF(struct b103_hdx, rx, 0x10);
B103_ASSERT_OFF(struct b103_hdx, tone_detect, 0x1c);
B103_ASSERT_OFF(struct b103_hdx, tone_lo, 0x20);
B103_ASSERT_OFF(struct b103fp, trace, 0x20);
B103_ASSERT_OFF(struct b103fp, fsd_count, 0x28);
B103_ASSERT_OFF(struct b103_cfg, loop_high_channel, 0x08);
B103_ASSERT_OFF(struct b103_cfg, tone_timeout_ticks, 0x0c);
B103_ASSERT_OFF(struct b103_cfg, tx_scale, 0x18);
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
typedef char b103_cfg_size[(sizeof(struct b103_cfg) == 28) ? 1 : -1];

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
 * passband gives an answer that fits no Bell 103 tone pair (finding F32).  The
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
 * reports FPM_TONE_PRESENT and 2000 or 2200 Hz do not (finding F33).  So this
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
 * The exit condition differs by TONE PLAN -- see the note at the test below.
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

	/*
	 * The exit condition differs by tone plan, not by direction: under
	 * V.21 the block count alone ends the mark hold, while Bell 103 also
	 * requires this station's own receiver to have acquired.  The field
	 * is named `v21` rather than `is_answer` because that is what it
	 * selects everywhere else -- see the tone table in b103fp.h.
	 */
	if (fp->cfg.v21) {
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
		fp->flags &= (unsigned char)~0x80;
		return nbits;
	}

	hdx = fp->hdx;
	hdx->rx_count = (short)(hdx->rx_count + 1);
	if (hdx->rx_count <= 7) {
		fp->flags &= (unsigned char)~0x80;
		return nbits;
	}

	B103NextState[hdx->mode](fp);
	fp->flags &= (unsigned char)~0x80;
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

/*
 * ---------------------------------------------------------------------------
 * B103FP_modem -- .text 0x08f010, 775 bytes.  The driver.
 *
 * One call carries both directions.  It contains no calls of its own at all --
 * every relocation in it is a data reference -- because it reaches the DSP
 * entirely through hdx->tx and hdx->rx.  That is the clearest evidence for the
 * architecture: the state machine IS the datapump.
 *
 * `n_tx` and `n_rx` are both in/out, and both change units:
 *
 *     n_tx   in: bits to send        out: samples produced
 *     n_rx   in: samples received    out: bits recovered
 *
 * ---------------------------------------------------------------------------
 * The receive bandpass, at last
 *
 * The filter B103FP_create selects between B103_BPF_CALLER and
 * B103_BPF_ANSWER and parks at dsp[+0xf4] is applied HERE, before anything
 * else sees the signal -- which is why nothing reconstructed before this
 * touched it (finding F32).  It is a plain circular FIR, in place:
 *
 *     rx_in[i] = ((sum(h[j] * (rx_in[i-j] >> 2))) >>> 15) << 2
 *
 * The input is scaled down by four before the filter and the result back up
 * by four after, which buys two bits of headroom in the accumulator at the
 * cost of two bits at the bottom.  The shift is LOGICAL in the original;
 * arithmetic would give the same 16 bits after the << 2 and the truncation,
 * so nothing rests on it, but it is reproduced as written.
 *
 * ---------------------------------------------------------------------------
 * Two things that look like loops and are really state-machine hand-overs
 *
 * Both the transmit and receive loops repeat "while the count has not been
 * consumed".  Every Hdx state zeroes the count, so normally each runs once --
 * except TxHdxStartB103, which produces nothing and leaves the count alone,
 * so the loop immediately re-enters with whatever state it just installed.
 * That is how a state can advance without emitting a sample.
 *
 * ---------------------------------------------------------------------------
 * The transmit data is discarded whenever the link is not up
 *
 * If `fp->status` is non-zero the staged bits are overwritten with mark
 * before the modulator sees them.  A caller feeding data during call setup
 * therefore loses it silently; the layer above has to know not to.
 */

/*
 * The two staging buffers, with the original's own names, from .bss:0x6c0 and
 * 0x7a0.  100 shorts each, and file-static in the original -- so a second
 * B103FP instance would share them.  Reproduced as-is; nothing here is
 * re-entrant and the datapump is single-threaded.
 */
#define B103_INTERNAL_BITS 100

static short tx_in_internal[B103_INTERNAL_BITS];
static short rx_out_internal[B103_INTERNAL_BITS];

int
B103FP_modem(struct b103fp *fp, const int *tx_bits, short *tx_out,
	     short *rx_in, int *rx_bits, short *n_tx, short *n_rx)
{
	struct b103_dsp *dsp;
	int total;
	short count;
	int i;

	/* Stage the transmit bits, narrowing int to short. */
	for (i = 0; i < (short)*n_tx; i++)
		tx_in_internal[i] = (short)tx_bits[i];

	/* Not connected: send mark, whatever was offered. */
	if (fp->status != 0) {
		for (i = 0; i < (short)*n_tx; i++)
			tx_in_internal[i] = 1;
	}

	/* The channel bandpass, in place. */
	dsp = fp->dsp;
	for (i = 0; i < (short)*n_rx; i++) {
		short *hist = (short *)dsp->bpf_hist;
		const short *coeff = (const short *)dsp->bpf;
		int taps = dsp->bpf_taps;
		int widx = (unsigned short)dsp->bpf_idx;
		const short *c;
		short *p;
		int acc = 0;
		int k;

		widx = (taps > widx + 1) ? widx + 1 : 0;
		dsp->bpf_idx = (short)widx;

		hist[widx] = (short)(rx_in[i] >> 2);

		c = coeff;
		p = &hist[widx];
		for (k = widx; k >= 0; k--)
			acc += *p-- * *c++;
		p += taps;
		for (k = taps - 1; k > widx; k--)
			acc += *p-- * *c++;

		rx_in[i] = (short)(((unsigned)acc >> 15) << 2);
	}

	fp->flags &= (unsigned char)~B103_FLAG_TIMEOUT;
	if (fp->flags & B103_FLAG_CLEAR_STATUS)
		fp->status = 0;

	/* Pad the transmit bits out to a full six with mark. */
	for (i = (short)*n_tx; i <= 5; i++)
		tx_in_internal[i] = 1;

	/* Transmit.  Always six bits, however many were offered. */
	count = 6;
	total = 0;
	do {
		short n = fp->hdx->tx(fp, tx_in_internal, tx_out, &count);

		tx_out += n;
		total = (short)(total + n);
	} while (count > 0);
	*n_tx = (short)total;

	/* Receive. */
	{
		short *staging = rx_out_internal;
		short remaining;

		total = 0;
		do {
			short n;

			remaining = (short)*n_rx;
			n = fp->hdx->rx(fp, rx_in, staging, n_rx);
			rx_in += remaining - (unsigned short)*n_rx;
			staging += n;
			total = (short)(total + n);
		} while (*n_rx != 0);

		*n_rx = (short)total;
	}

	/* Widen the recovered bits back out to int. */
	for (i = 0; i < (unsigned short)*n_rx; i++)
		rx_bits[i] = (unsigned short)rx_out_internal[i];

	/*
	 * The switch is debug-only -- every arm returns the same thing -- and
	 * it was elided here until finding F2953.  `debug.h`'s policy is to
	 * carry the call sites: the gate is real control flow and the strings
	 * are the author's own words, and with the level at zero nothing else
	 * can tell the two versions apart (finding F134).
	 *
	 * The eight-entry jump table at `.rodata+0x8e14` sends 0..4, 6 and 7
	 * straight to the epilogue and 5 to the error message; anything above
	 * 7 fails the `cmp $0x7` before the table is reached and prints the
	 * other one.  Neither string has a trailing newline; that is the
	 * object's.
	 *
	 * The scrutinee is the BYTE at +0x1c -- `movzbl 0x1c(%edx),%eax` --
	 * while the return is the whole 32-bit word there, status in the low
	 * byte and flags in the next.  The same four bytes read two ways, and
	 * both readings are the original's.
	 */
	switch (fp->status) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 6:
	case 7:
		break;
	case 5:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "Bell103 internal error detected!");
		break;
	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "Bell103 unknown internal state!");
		break;
	}

	{
		int word;

		memcpy(&word, &fp->status, sizeof word);
		return word;
	}
}

/*
 * ---------------------------------------------------------------------------
 * B103FP_delete -- .text 0x08ef00, 272 bytes.
 *
 * Tears the tree down in the reverse order create built it.  The two tone
 * objects are guarded because loopback never creates the detector; nothing
 * else is.
 *
 * Note the original places a literal 1 in the second argument slot before
 * FPM_FSD_free and both FPM_MRF_free calls.  Neither function reads it --
 * both take a single argument -- so it is dead stack setup, presumably from
 * a version where they took a `fresh` flag like their init counterparts.
 * Not reproduced, because there is nothing to reproduce: an argument the
 * callee never loads has no observable effect.
 *
 * The final free is D8: it releases the object unconditionally, even when the
 * caller supplied it.  Reproduced -- see docs/deviations.md.
 */
void
B103FP_delete(struct b103fp *fp)
{
	struct b103_dsp *dsp = fp->dsp;
	struct b103_hdx *hdx;

	FPM_FSM_delete(&dsp->fsm);
	FPM_MTD_delete(dsp->mtd);
	FPM_FSD_free(&dsp->fsd);
	FPM_MRF_free(&dsp->rx_mrf);
	FPM_MRF_free(&dsp->tx_mrf);

	sysdep_free(dsp->scratch);
	sysdep_free(dsp->rx_scratch);
	sysdep_free(dsp->bpf_hist);
	sysdep_free(dsp);

	hdx = fp->hdx;
	if (hdx->tone_detect != NULL)
		FPM_TONE_delete(hdx->tone_detect);
	if (hdx->tone_lo != NULL)
		FPM_TONE_delete(hdx->tone_lo);
	sysdep_free(hdx);

	sysdep_free(fp);	/* D8 */
}

/*
 * ---------------------------------------------------------------------------
 * B103FP_create -- .text 0x08e690, 2151 bytes.
 *
 * Not 2151 bytes of logic: it is *copy a library default onto the stack,
 * patch the Bell 103 fields into it, call the init*, eight times over, with
 * a switch on the call type deciding the tone plan.
 *
 * ---------------------------------------------------------------------------
 * The "pass NULL to allocate" idiom, at three levels
 *
 *   state == NULL          allocate the 88-byte object, and NULL both of its
 *                          sub-pointers so the two below also fire
 *   state->hdx == NULL     allocate the 36-byte half-duplex context
 *   state->dsp == NULL     allocate the 256-byte DSP block, CLEAR it, and
 *                          allocate its three buffers.  Also sets `fresh`,
 *                          which is passed to every sub-object's init so
 *                          they allocate their own working buffers exactly
 *                          once.
 *
 * That last is the whole ownership mechanism: a caller supplying its own DSP
 * block is saying it supplied the sub-buffers too, and `fresh == 0` makes
 * every init reuse rather than allocate.
 *
 * ---------------------------------------------------------------------------
 * Two switches, not one
 *
 * The call type is tested twice, and they do different things.  The first,
 * right after the local oscillator is built, sets `hdx->mode` and the tone
 * detector.  The second, after the modulator and demodulator configs are
 * staged, installs the transmit tones and the channel bandpass.  Writing it
 * as one switch would be tidier and would reorder the allocations, which the
 * allocation-balance test would notice.
 */

/* Q13 magic for Hz -> phase increment at 8 kHz; see FPM_TONE_set_freq. */
#define B103_HZ_TO_INC(hz) ((short)(((int)(hz) * 0x8312 + 0x1000) >> 13))

struct b103fp *
B103FP_create(struct b103fp *fp, const struct b103_cfg *cfg)
{
	struct fpm_tone_cfg tone;
	struct fpm_mrf_cfg mrf;
	struct fpm_fsm_cfg fsm;
	struct fpm_fsd_cfg fsd;
	struct fpm_mtd_cfg mtd;
	struct b103_hdx *hdx;
	struct b103_dsp *dsp;
	int fresh = 0;
	int timeout;
	int i;

	if (fp == NULL) {
		fp = (struct b103fp *)sysdep_malloc(sizeof(*fp));
		if (fp == NULL)
			return NULL;
		fp->hdx = NULL;
		fp->dsp = NULL;
	}
	if (cfg == NULL)
		cfg = &B103_CFG_data;

	/* The config IS the object's first 28 bytes. */
	fp->cfg = *cfg;

	if (fp->hdx == NULL)
		fp->hdx = (struct b103_hdx *)sysdep_malloc(sizeof(*fp->hdx));
	hdx = fp->hdx;

	/*
	 * The answer-tone timeout, in blocks.
	 *
	 * The floor is applied with an UNSIGNED comparison against a signed
	 * divide, so a negative `tone_timeout_ticks` becomes a huge unsigned,
	 * passes the test the floor exists to catch, and is stored as a
	 * negative short.  Reproduced; nothing reachable sets it negative.
	 */
	timeout = fp->cfg.tone_timeout_ticks / 20;
	if ((unsigned)timeout < 700u)
		timeout = 700;
	hdx->tone_timeout = (short)timeout;

	hdx->tx_blocks = 0;
	hdx->rx_count = 0;
	hdx->tx = TxHdxStartB103;
	hdx->substate = 0;
	hdx->r18 = 0;
	hdx->tone_detect = NULL;
	hdx->tone_lo = NULL;

	/* The receive local oscillator: default frequency, then overridden. */
	tone = FPM_TONE_CFG_data;
	hdx->tone_lo = FPM_TONE_create(NULL, &tone);
	hdx->tone_lo->inc = 0x159a;		/* 1350.1 Hz */
	hdx->tone_lo->phase = 0;

	/*
	 * First switch: the mode and the tone detector.  The detector's
	 * config is the library default with its ratio, threshold and notch
	 * radius replaced -- the tone it listens for is the MARK of the pair
	 * this station receives.
	 */
	tone = FPM_TONE_CFG_data;
	tone.ratio = 0x55c3;
	tone.min_level = 6;
	tone.damp = 0x7c00;

	switch (fp->cfg.call_type) {
	case B103_CALL_ORIGINATE:
		hdx->mode = 1;
		tone.freq = fp->cfg.v21 ? 1650 : 2225;
		hdx->tone_detect = FPM_TONE_create(NULL, &tone);
		if (fp->cfg.f10 == 0) {
			hdx->tx_blocks = 40;
			hdx->tx = TxHdxMarksB103;
			hdx->rx = RxHdxDataB103;
			hdx->substate = B103_STATE_WAIT2;
			fp->status = 4;
			fp->flags = (unsigned char)((fp->flags & ~0x40) | 0x34);
		} else {
			hdx->r18 = 1;
			if (fp->cfg.v21)
				hdx->tone_lo->inc = 0xf9a;	/* 975.1 Hz */
		}
		break;

	case B103_CALL_ANSWER:
		hdx->mode = 2;
		hdx->r18 = 1;
		if (fp->cfg.v21) {
			tone.freq = 1180;
			hdx->tone_lo->inc = 0x4e2;	/* 305.2 Hz */
		} else {
			tone.freq = 1270;
			hdx->tone_lo->inc = 0x652;	/* 395.0 Hz */
		}
		hdx->tone_detect = FPM_TONE_create(NULL, &tone);
		break;

	default:
		hdx->mode = 0;
		break;
	}

	/* The DSP block and its buffers. */
	if (fp->dsp == NULL) {
		dsp = (struct b103_dsp *)sysdep_malloc(sizeof(*dsp));
		fp->dsp = dsp;
		sysdep_memset(dsp, 0, sizeof(*dsp));
		dsp->scratch = (short *)sysdep_malloc(0x144);
		dsp->rx_scratch = (short *)sysdep_malloc(0x144);
		/*
		 * The channel filter's history.  The answer side's filter is
		 * 50 taps to the caller's 40, so it gets a bigger buffer --
		 * 104 bytes rather than 84.  Both are cleared to exactly the
		 * filter's length by the switch below, not here.
		 */
		dsp->bpf_hist = (short *)sysdep_malloc(
			fp->cfg.call_type == B103_CALL_ANSWER ? 0x68 : 0x54);
		/* Only the transmit staging buffer is cleared, not both. */
		sysdep_memset(dsp->scratch, 0, 0x144);
		dsp->mtd = NULL;
		fresh = 1;
	}
	dsp = fp->dsp;

	/* Transmit rate conversion, 7200 -> 8000. */
	mrf = FPM_MRF_CFG;
	mrf.branches = 10;
	mrf.decimate = 9;
	mrf.coeff = B103_MRF_FILT_TX;
	mrf.taps = 270;
	FPM_MRF_init(&dsp->tx_mrf, &mrf, fresh);

	/* Receive rate conversion, 8000 -> 2400. */
	mrf.branches = 3;
	mrf.decimate = 10;
	mrf.coeff = B103_MRF_FILT_RX;
	mrf.taps = 90;
	FPM_MRF_init(&dsp->rx_mrf, &mrf, fresh);

	/*
	 * Both gain controls share one config; the acquisition one's block
	 * length is patched afterwards.  Reading the two init calls alone
	 * would say they were identical.
	 */
	FPM_AGC_init(&dsp->agc, &AGCb103_CFG_data, fresh);
	FPM_AGC_init(&dsp->det_agc, &AGCb103_CFG_data, fresh);
	dsp->det_agc.cfg.block_len = 40;

	dsp->r00 = 1;
	dsp->rx_energy = 0;
	dsp->rx_tone = 0;

	/* Stage the modulator and demodulator configs. */
	fsm = FPM_FSM_CFG_data;
	fsd = FPM_FSD_CFG_data;

	/*
	 * Second switch: the transmit tones and the channel bandpass.  Note
	 * `high_bit` -- the slicer's polarity -- moves with the standard,
	 * because Bell 103's mark is the higher tone of its pair and V.21's
	 * is the lower.
	 */
	switch (fp->cfg.call_type) {
	case B103_CALL_ORIGINATE:
		if (fp->cfg.v21) {
			fsm.freq[0] = 1180;
			fsm.freq[1] = 980;
			fsd.high_bit = 0;
		} else {
			fsm.freq[0] = 1070;
			fsm.freq[1] = 1270;
			fsd.high_bit = 1;
		}
		dsp->bpf = B103_BPF_CALLER;
		dsp->bpf_idx = 0;
		dsp->bpf_taps = 40;
		sysdep_memset(dsp->bpf_hist, 0, 80);
		break;

	case B103_CALL_ANSWER:
		fsm.freq[0] = 2025;
		fsm.freq[1] = 2225;
		dsp->bpf = B103_BPF_ANSWER;
		dsp->bpf_idx = 0;
		dsp->bpf_taps = 50;
		sysdep_memset(dsp->bpf_hist, 0, 100);
		break;

	default:
		/* Out of range is loopback, but flagged. */
		if (fp->cfg.call_type != B103_CALL_LOOPBACK) {
			fp->flags |= B103_FLAG_TIMEOUT;
			fp->status = 5;
		}
		if (fp->cfg.loop_high_channel) {
			fsm.freq[0] = 2025;
			fsm.freq[1] = 2225;
		} else {
			fsm.freq[0] = 1070;
			fsm.freq[1] = 1270;
		}
		break;
	}

	fsm.samples_per_sym = 24;
	fsm.scale = (short)fp->cfg.tx_scale;
	FPM_FSM_init(&dsp->fsm, &fsm);

	fsd.fir = B103_CHAN_INTRP;
	fsd.fir_taps = 15;
	fsd.delay = 4;
	fsd.iir = B103_IIR_LPF;
	fsd.iir_len = 3;
	FPM_FSD_init(&dsp->fsd, &fsd, fresh);

	mtd = FPM_MTD_CFG_data;
	mtd.coeff = MTDb103_COEF;
	mtd.tones = 2;
	mtd.ratio = 0x3666;
	mtd.min_level = 2;
	dsp->mtd = FPM_MTD_create(dsp->mtd, &mtd);

	dsp->rx_state = 0;

	/* The tail: everything the object reports upward starts here. */
	fp->status = 0;
	fp->flags = 0;
	fp->r1e[0] = 0;
	fp->r1e[1] = 0;
	fp->flags |= 0x40;
	fp->status = 1;

	fp->trace = dsp->fsd.trace;
	fp->fsd_count = &dsp->fsd.last_count;
	for (i = 0; i < (int)sizeof(fp->r24); i++)
		fp->r24[i] = 0;
	/*
	 * The reserved block at +0x2c is three twelve-byte entries, and only
	 * the first ten bytes of each are cleared -- the last word of every
	 * entry keeps whatever the allocator left.  Clearing all thirty-six
	 * bytes reads as tidier and is wrong; the difference was invisible
	 * until fresh allocations stopped arriving zeroed.
	 */
	for (i = 0; i < (int)sizeof(fp->r2c); i++)
		if ((i % 12) < 10)
			fp->r2c[i] = 0;

	return fp;
}

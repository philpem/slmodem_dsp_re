/*
 * B103int.c -- Bell 103 modulation, demodulation and carrier detection.
 *
 * Native callers retain calls to these four helpers. Their consecutive run
 * follows the half-duplex states; B103int.c is a later original FILE
 * record. The separation is witnessed by the call boundaries; the filename
 * attribution remains an inference because the helpers have no local anchor.
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
unsigned short
ModDataB103(struct b103fp *fp, const unsigned short *bits, short *out,
	    unsigned short nbits)
{
	unsigned short nsamples;

	nsamples = (unsigned short)FPM_FSM_modulate(&fp->dsp->fsm, bits,
						    fp->dsp->scratch, nbits);

	return FPM_MRF_filter(&fp->dsp->tx_mrf,
						     fp->dsp->scratch, out,
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
unsigned short
TxNoCarrierB103(struct b103fp *fp, const unsigned short *bits, short *out,
		unsigned short nbits)
{
	short saved_scale = fp->dsp->fsm.cfg.scale;
	unsigned short nsamples;

	fp->dsp->fsm.cfg.scale = 0;
	nsamples = (unsigned short)FPM_FSM_modulate(&fp->dsp->fsm, bits,
						    fp->dsp->scratch, nbits);
	fp->dsp->fsm.cfg.scale = saved_scale;

	return FPM_MRF_filter(&fp->dsp->tx_mrf,
						     fp->dsp->scratch, out,
						     nsamples);
}

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
int
DemodDataB103(struct b103fp *fp, short *in, unsigned short *bits_out,
	      unsigned short count)
{
	/*
	 * The local oscillator's samples.  The original's frame leaves room
	 * for 160 shorts here and bounds `count` against it nowhere; 160 is
	 * one 20 ms fragment at 8 kHz, which is what dp_wrapper delivers.
	 */
	short lo[160];
	int nbits = 0;
	short i;

	if (fp->dsp->rx_state <= 14) {
		/*
		 * The detector gets its own copy, because the mixer below
		 * overwrites `in`.
		 */
		for (i = 0; i < count; i++)
			fp->dsp->rx_scratch[i] = in[i];

		FPM_AGC_agc(&fp->dsp->det_agc, fp->dsp->rx_scratch, count, 1);

		fp->dsp->rx_tone = 0;
		if (FPM_TONE_detect(fp->hdx->tone_detect, fp->dsp->rx_scratch,
				    (short)count) == FPM_TONE_PRESENT) {
			fp->dsp->rx_tone = 1;
			fp->dsp->rx_state = (short)(fp->dsp->rx_state + 5);
		} else {
			fp->dsp->rx_state = 0;
		}
	}

	/* Mix to baseband, in place. */
	FPM_TONE_generate_demod(fp->hdx->tone_lo, lo, (short)count);
	for (i = 0; i < count; i++)
		in[i] = (short)((in[i] * lo[i]) >> 14);

	count = (unsigned short)FPM_MRF_filter(&fp->dsp->rx_mrf, in, fp->dsp->scratch,
					       count);

	fp->dsp->agc.f18 = fp->dsp->r00;
	FPM_AGC_agc(&fp->dsp->agc, fp->dsp->scratch, count, 1);
	fp->dsp->rx_energy = fp->dsp->agc.signal;

	if (fp->dsp->rx_state == 15)
		FPM_AGC_Freeze(&fp->dsp->agc);

	if (fp->dsp->rx_state > 14) {
		fp->dsp->rx_tone = 1;
		fp->dsp->rx_state = 16;
		nbits = (unsigned short)FPM_FSD_demodulate(&fp->dsp->fsd, fp->dsp->scratch, bits_out,
					   count);
	}

	return nbits;
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

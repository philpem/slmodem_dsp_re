/*
 * v22rate.h -- V.22 / V.22bis: rate selection, receiver reset, and the
 * receive chain.
 *
 * Reconstructed from dsplibs.o:
 *
 *   SetTxRate     .text 0x08e120  205 bytes
 *   SetRxRate     .text 0x08e1f0  211 bytes
 *   DemodDataV22  .text 0x08e370  510 bytes
 *   ResetRx       .text 0x08e570   62 bytes
 *
 * All four take `struct v22fp *` -- the object `V22FP_create` builds, which
 * `v22prc.h` reaches through `V22_OBJ_FP` and friends and which
 * `include/dsplib/v22fp.h` models.  They are the four functions of the
 * 0x8e120..0x8e669 block that src/pump/v22/v22data.c did not take, and they
 * are written here in the object's own order for the reason that file
 * records.
 *
 * ---------------------------------------------------------------------------
 * WHAT A RATE CHANGE ACTUALLY IS
 *
 * `SetTxRate` and `SetRxRate` do NOT call the scrambler's own `init`: they
 * re-derive its four width-dependent fields in place -- `nbits`, `mask`,
 * `notmask` and the two tap shifts -- and clear the shift register.  That is
 * `FPM_SDM_init`'s arithmetic open-coded, and it is what makes the rate
 * switchable without discarding the rest of the object.
 *
 * Beyond the scrambler, the transmit side moves the symbol coder's quadrant
 * shift and amplitude mask and repoints the pulse shaper's constellation maps
 * (`v22_pps.h`: `imap` and `qmap` are set by the caller and not by init), and
 * the receive side repoints the equaliser's slicer at
 * `FSEv22_decision12` or `FSEv22_decision24`.
 *
 * The two rate arms are written out separately rather than sharing a loop
 * over a table, which is what the object does: two near-identical blocks with
 * different immediates and a jump between them.
 */

#ifndef DSPLIB_V22RATE_H
#define DSPLIB_V22RATE_H

struct v22fp;

/*
 * The rate selector both setters take.  It is NOT `struct v22fp_cfg::rate`,
 * whose 0 means 2400 -- see v22fp.h -- and it is not the bit rate either.
 * Anything outside 0..1 does nothing at all, silently.
 *
 * What names the two is the arithmetic and not the caller: selector 1 gives
 * the scrambler four-bit words, the symbol coder a two-bit quadrant shift and
 * a two-bit amplitude mask, and the 2400 bit/s constellation maps, which is
 * exactly the split `fpm_smc.h` records for V.22bis at 2400.
 */
#define V22_RATE_1200		0
#define V22_RATE_2400		1

/* Scrambler word width at each rate, in bits. */
#define V22_SDM_BITS_1200	2
#define V22_SDM_BITS_2400	4

/*
 * `struct v22fp_dsp::r2e` selects the receiver front end, and the value 2 is
 * the one `V22FP_create` writes for mode 0 and the only one that makes it
 * allocate `hdx->iir`.  `DemodDataV22` runs the IIR path on exactly that
 * value.
 */
#define V22_FRONTEND_IIR	2

/**
 * @brief Select the V.22 transmit rate.
 *
 * Re-derives the scrambler's width-dependent fields in place (not a call
 * to `FPM_SDM_init`), moves the symbol coder's quadrant shift and
 * amplitude mask, and repoints the pulse shaper's constellation maps.
 *
 * @param fp    The V.22 datapump instance.
 * @param rate  V22_RATE_1200 or V22_RATE_2400; anything else is a no-op.
 */
void SetTxRate(struct v22fp *fp, short rate);

/**
 * @brief Select the V.22 receive rate.
 *
 * Re-derives the descrambler's width-dependent fields in place and
 * repoints the equaliser's slicer at FSEv22_decision12() or
 * FSEv22_decision24().
 *
 * @param fp    The V.22 datapump instance.
 * @param rate  V22_RATE_1200 or V22_RATE_2400; anything else is a no-op.
 */
void SetRxRate(struct v22fp *fp, short rate);

/**
 * @brief Re-initialise the V.22 symbol-rate recovery loop and the equaliser without reallocating either.
 *
 * `V22_FSE_init` is handed the equaliser as its own configuration. That is
 * not a transcription slip: `struct v22_fse_cfg` is the two words `icoff`
 * and `qcoff`, and `struct v22_fse` opens with the same two in the same
 * places, so passing the state re-seeds it from the coefficient tables it
 * is already pointing at. Both calls pass `fresh` = 0, so nothing is
 * allocated.
 *
 * @param fp  The V.22 datapump instance.
 */
void ResetRx(struct v22fp *fp);

/**
 * @brief Demodulate one received V.22 block: optional IIR front end, rate conversion, level check, AGC, symbol-clock recovery, equalisation.
 *
 * @p in is an input and two scratch buffers. The rate converter reads it
 * and writes `dsp->rx_scratch`; the clock recovery reads `rx_scratch` and
 * writes back over @p in; the equaliser then reads @p in and writes @p sym.
 * So @p in is destroyed, and it must be large enough for whatever the
 * resampler produces as well as for what the caller put there.
 *
 * Zero is also the disconnect answer: when the level check fails the
 * carrier flag is cleared and the function returns 0 without running any
 * of the rest.
 *
 * @param fp     The V.22 datapump instance.
 * @param in     Input samples; destroyed as scratch space (see above).
 * @param sym    Output for the demodulated symbols.
 * @param count  How many input samples.
 * @return The number of symbols now in @p sym, read back out of the
 *         equaliser's own counter rather than from `V22_FSE_receive`'s
 *         return, which the object discards.
 */
unsigned short DemodDataV22(struct v22fp *fp, short *in, unsigned short *sym,
			    unsigned short count);

#endif /* DSPLIB_V22RATE_H */

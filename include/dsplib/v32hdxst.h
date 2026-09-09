/*
 * v32hdxst.h -- ITU-T V.32 / V.32bis: the twenty half-duplex STATES and the
 *               five next-state dispatchers they transition through.
 *
 * `include/dsplib/v32hdx.h` is the CONTRACT -- what the two drivers
 * (`V32TxHdxModem`, `V32RxHdxModem`) require of a state.  This header is the
 * roster: the twenty states themselves, the five `V32*NextState` functions,
 * and the four `.data` tables that tie them together.
 *
 * ---------------------------------------------------------------------------
 * WHY THIS IS ONE HEADER AND NOT SIX
 *
 * The twenty-five functions are ONE strongly-connected component in the
 * object's relocation graph, and that is measured rather than assumed:
 *
 *     python3 tools/closure.py RxHdxNull --missing
 *
 * reports twenty-two of them from a 103-byte leaf, and adding the three the
 * walk does not reach from there (`RxHdxTone`, `TxHdxNull`, `TxHdxTone`)
 * closes it at twenty-five call symbols and six data symbols.  A state
 * installs its successor by STORING that successor's address into the
 * context, and a `movl $handler, field` is a relocation exactly as a `call`
 * is -- so no proper subset of the twenty-five can be linked.  They land
 * together or not at all, and one header is the honest shape of that.
 * Findings F8200 (which predicted the closure) and F8562 (which measured it).
 *
 * ---------------------------------------------------------------------------
 * THE OBJECT'S OWN FILE NAMES
 *
 * The blob's `FILE` symbols name the original translation units, and the
 * `.text` addresses group cleanly against them:
 *
 *     V32TXHDX.c   0x07fce0..0x080324   V32TxHdxModem + the eight TxHdx*
 *     V32rxhdx.c   0x0838f0..0x084509   V32RxHdxModem + the twelve RxHdx*
 *                                       (with CalcTurnAroundDelay local)
 *     V32org.c     0x082be0             V32OrgNextState
 *     V32ans.c     0x085ab0             V32AnsNextState
 *     V32RNG.c     0x084c80, 0x0852d0   V32RngInitNextState, V32RngRespNextState
 *     V32loop.c    0x0864a0             V32LocLoopNextState
 *
 * This tree does not mirror the blob's translation units -- `v32fpctl.c`
 * already carries functions from four of these spans -- so the reconstruction
 * splits by role instead.  The mapping is recorded here because it is the
 * only place the object's own grouping is written down, and because a tier-3
 * pass that wants emission order will need it.
 *
 * ---------------------------------------------------------------------------
 * THE MODE TABLE, AND A CORRECTION TO v32hdx.h
 *
 * A state transitions with `call *V32NextState[hdx->mode]` -- `TxHdxTone` at
 * 7fd9b is the worked example.  `v32hdx.h`'s prose lists that table's six
 * slots as
 *
 *     0 Org   1 Ans   2 LocLoop   3 LocLoop   4 RngInit   5 (no relocation)
 *
 * and THAT IS WRONG in two slots.  `tools/tabdump.py --sym V32NextState`
 * prints all six relocations:
 *
 *     0  V32OrgNextState        3  V32LocLoopNextState
 *     1  V32AnsNextState        4  V32RngInitNextState
 *     2  V32LocLoopNextState    5  V32RngRespNextState
 *
 * so slot 5 is relocated after all, and the pair of `V32LocLoopNextState`
 * entries is at 2 and 3 rather than 2 and 3-with-a-hole.  Finding F8560.
 */

#ifndef DSPLIB_V32HDXST_H
#define DSPLIB_V32HDXST_H

#include "dsplib/v32hdx.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The mode, which is hdx + V32HDX_MODE (0x76) and indexes V32NextState.
 * Named from the table's own relocations; see the correction above.
 *
 * THERE ARE TWO UNRELATED THINGS IN THIS TREE CALLED A V.32 "MODE" AND THEY
 * SHARE A PREFIX BY ACCIDENT OF THE OBJECT'S OWN VOCABULARY.  This one is the
 * HANDSHAKE mode -- which of the five state machines drives the half-duplex
 * exchange, six slots, held at hdx + 0x76.  `include/dsplib/v32fpctl.h`'s
 * `V32_MODE_ABS4` .. `V32_MODE_128T` are the CONSTELLATION mode -- which
 * signal set the modulator uses, seven values, a different field entirely.
 * No macro name collides; the concepts do.  Check which field a use reads
 * before reaching for either set.
 */
#define V32_MODE_ORIGINATE	0
#define V32_MODE_ANSWER		1
#define V32_MODE_LOCLOOP_2	2
#define V32_MODE_LOCLOOP_3	3
#define V32_MODE_RING_INIT	4
#define V32_MODE_RING_RESP	5
#define V32_NEXTSTATE_COUNT	6

/*
 * A next-state dispatcher.  One argument, the modem instance, and no value:
 * every call site is `push modem; call *V32NextState[mode]` with %eax dead
 * afterwards (7fd9b in `TxHdxTone` is the shortest).
 */
typedef void (*v32_nextstate_fn)(void *modem);

/*
 * `.data`, GLOBAL, and therefore NOT const -- section 143 in the object for
 * all four.  `V32_RX_MODE` and `V32_TX_MODE` are seven entries each, indexed
 * by the V.32 rate 0..6 of `v32seq.h`, and `V32_CONNECT` is the handshake
 * state each rate connects in.
 */
extern v32_nextstate_fn V32NextState[V32_NEXTSTATE_COUNT];
extern short V32_CONNECT[7];
extern short V32_RX_MODE[7];
extern short V32_TX_MODE[7];

/*
 * The S-tone detector's coefficient bank -- .rodata 0x006d60, 30 bytes,
 * GLOBAL, and therefore const.  It is `struct fpm_mtd_cfg::coeff`: fifteen
 * shorts, three biquad sections of five, and the type is settled by the
 * callee rather than by the table (`include/dsplib/fpm_mtd.h`, "+0x00 is a
 * POINTER to the coefficient bank, not a scalar").
 *
 * `V32OrgNextState` (830b8) and `V32AnsNextState` (860c3) store its address
 * into the MTD at hdx + V32_HDX_MTD before calling `FPM_MTD_create`, and
 * `RxHdxSTone` (842b3) tests `mtd->cfg.coeff == V32_S_DATA_COEF` to tell the
 * DATA-mode S tone's detector from the other bank the same field can hold.
 *
 * That other bank is `V32_S_COEF`, .rodata 0x006d7e, also fifteen shorts and
 * immediately after this one.  It is NOT defined here: nothing written or in
 * this batch's closure references it, and a table with no consumer cannot be
 * differentially tested.  Finding F8563.
 */
extern const short V32_S_DATA_COEF[15];

/* ---------------------------------------------------------------- transmit */

/**
 * @brief V.32 transmit state: emit the half-duplex tone.
 *
 * Fills the block with the tone at `hdx + V32_HDX_TONE0` and spends the
 * whole of `*left`. @p data is unused: the tone generator writes @p out
 * directly and no data path runs.
 *
 * @param modem  The V.32 datapump instance.
 * @param data   Unused.
 * @param out    Output for the tone samples.
 * @param left   In/out: symbol budget; zeroed unconditionally.
 * @return The block's sample count (re-read after any transition; see the file banner).
 */
short TxHdxTone(void *modem, short *data, short *out, unsigned short *left);

/**
 * @brief V.32 transmit state: emit the handshake's carrier-bearing sequence, unscrambled.
 *
 * `GenSequence` fills @p data from the pattern `InitGenSequence` armed and
 * `ModDataV32` shapes it.
 *
 * @param modem  The V.32 datapump instance.
 * @param data   Scratch buffer, filled by `GenSequence`.
 * @param out    Output for the modulated samples.
 * @param left   In/out: block symbol budget, clamped against the handshake countdown.
 * @return The number of samples written.
 */
short TxHdxCarrierState(void *modem, short *data, short *out,
			unsigned short *left);

/**
 * @brief V.32 transmit state: the same as TxHdxCarrierState(), with the scrambler in the path.
 * @param modem  The V.32 datapump instance.
 * @param data   Scratch buffer, filled by `GenSequence` and scrambled in place.
 * @param out    Output for the modulated samples.
 * @param left   In/out: block symbol budget, clamped against the handshake countdown.
 * @return The number of samples written.
 */
short TxHdxScrSequence(void *modem, short *data, short *out,
		       unsigned short *left);

/**
 * @brief V.32 transmit state: the equaliser training segment (TRN).
 *
 * Generates, scrambles, then folds each word onto the two-point TRN
 * alphabet {0, 3} before modulating it.
 *
 * @param modem  The V.32 datapump instance.
 * @param data   Scratch buffer: generated, scrambled, then folded onto the TRN alphabet.
 * @param out    Output for the modulated samples.
 * @param left   In/out: block symbol budget, clamped against the handshake countdown.
 * @return The number of samples written.
 */
short TxHdxTRN(void *modem, short *data, short *out, unsigned short *left);

/**
 * @brief V.32 transmit state: the connected data segment.
 *
 * Scrambles and modulates whatever the caller left in @p data. No
 * generator runs.
 *
 * @param modem  The V.32 datapump instance.
 * @param data   The caller's data, scrambled in place.
 * @param out    Output for the modulated samples.
 * @param left   In/out: block symbol budget, clamped against the handshake countdown.
 * @return The number of samples written.
 */
short TxHdxData(void *modem, short *data, short *out, unsigned short *left);

/**
 * @brief V.32 transmit state: drop the carrier.
 *
 * Runs the scrambler over @p data as usual, then shapes the no-carrier
 * constellation point instead of the data. `*left`'s own guard forces the
 * count to zero once the handshake countdown has expired, rather than
 * letting the truncation produce 0xffff (see the file banner).
 *
 * @param modem  The V.32 datapump instance.
 * @param data   The caller's data, scrambled in place (with a count of zero once expired).
 * @param out    Output for the shaped (no-carrier) samples.
 * @param left   In/out: block symbol budget, clamped against the handshake countdown.
 * @return The number of samples written.
 */
short TxHdxNoCarrier(void *modem, short *data, short *out,
		     unsigned short *left);

/**
 * @brief V.32 transmit state: finish the block with no carrier, then end the driver's loop.
 *
 * No clamp and no countdown guard: `*left` itself is the count, charged
 * in full against the countdown, and is zeroed at the end rather than
 * reduced -- so this is the only one of the eight transmit states that
 * always ends V32TxHdxModem()'s loop.
 *
 * @param modem  The V.32 datapump instance.
 * @param data   The caller's data, scrambled in place.
 * @param out    Output for the shaped (no-carrier) samples.
 * @param left   In/out: the whole remaining budget, charged in full and zeroed.
 * @return The number of samples written.
 */
short TxHdxFinishFrame(void *modem, short *data, short *out,
		       unsigned short *left);

/**
 * @brief V.32 transmit state: silence.
 *
 * Writes `hdx + V32HDX_SAMPLE_LEN` zero samples and spends the whole
 * block. @p data is unused.
 *
 * @param modem  The V.32 datapump instance.
 * @param data   Unused.
 * @param out    Output; filled with zero samples.
 * @param left   In/out: symbol budget; zeroed unconditionally.
 * @return The block's sample count (re-read after any transition; see the file banner).
 */
short TxHdxNull(void *modem, short *data, short *out, unsigned short *left);

/* ----------------------------------------------------------------- receive */

/*
 * Eleven of the twelve receive states below share one shape (see
 * V32rxhdx.c's file banner): charge one block's worth of symbols against
 * `hdx->timer`, do the state's own work (which may install a successor via
 * `V32NextState[hdx->mode]`), and post a fault reason at `V32_OBJ_STATUS`
 * if the timer has reached `hdx->limit`. `RxHdxError` alone does neither.
 */

/**
 * @brief V.32 receive state: run the answer/originate tone detector.
 *
 * Runs the AGC and, subject to a three-term gate (protocol, an options
 * bit, and a countdown), the tone detector; a lost tone transitions to
 * the next handshake step.
 *
 * @param modem  The V.32 datapump instance.
 * @param in     Input samples, gain-controlled in place.
 * @param out    Output; not filled with demodulated data by this state.
 * @param count  In/out sample count, clamped by `RxClampV32` on exit.
 */
void RxHdxTone(void *modem, short *in, unsigned short *out,
	       unsigned short *count);

/**
 * @brief V.32 receive state: wait for a detected tone with no signal yet decoded.
 *
 * Runs the AGC and the tone detector; once the tone has been seen for
 * more than 60 timer units it transitions to the next handshake step.
 *
 * @param modem  The V.32 datapump instance.
 * @param in     Input samples, gain-controlled in place.
 * @param out    Output; not filled with demodulated data by this state.
 * @param count  In/out sample count, clamped by `RxClampV32` on exit.
 */
void RxHdxNoSignal(void *modem, short *in, unsigned short *out,
		   unsigned short *count);

/**
 * @brief V.32 receive state: search for the handshake's phase reversal and measure round-trip delay.
 *
 * Runs the AGC and kills two tones; while armed (`V32HDX_INT_90`), searches
 * for the phase reversal and, on finding it, computes and stores the
 * round-trip delay (`V32HDX_RTD`) before transitioning. Otherwise runs the
 * tone detector and arms the reversal search after several consecutive
 * present-tone blocks. See the file banner for the reversal-scale
 * constants and the not-quite-`else` control flow.
 *
 * @param modem  The V.32 datapump instance.
 * @param in     Input samples, gain-controlled and tone-killed in place.
 * @param out    Output; not filled with demodulated data by this state.
 * @param count  In/out sample count, clamped by `RxClampV32` on exit.
 */
void RxHdxPhsReversal(void *modem, short *in, unsigned short *out,
		      unsigned short *count);

/**
 * @brief V.32 receive state: demodulate and look for the rate sequence, requiring two matching detections.
 *
 * Demodulates and descrambles the block, then requires `DetSequence` to
 * find a match AND the detector's high and low halves to agree before
 * transitioning -- i.e. the same rate signal must have arrived twice.
 *
 * @param modem  The V.32 datapump instance.
 * @param in     Input samples to demodulate.
 * @param out    Output for the demodulated (descrambled) symbols.
 * @param count  In/out sample count, clamped by `RxClampV32` on exit.
 */
void RxHdxRateSequence(void *modem, short *in, unsigned short *out,
		       unsigned short *count);

/**
 * @brief V.32 receive state: demodulate and look for the handshake sequence.
 *
 * Demodulates and descrambles the block; a single `DetSequence` match
 * transitions to the next handshake step.
 *
 * @param modem  The V.32 datapump instance.
 * @param in     Input samples to demodulate.
 * @param out    Output for the demodulated (descrambled) symbols.
 * @param count  In/out sample count, clamped by `RxClampV32` on exit.
 */
void RxHdxSequence(void *modem, short *in, unsigned short *out,
		   unsigned short *count);

/**
 * @brief V.32 receive state: demodulate, decode the negotiated rate sequence, and switch the receiver to it.
 *
 * Demodulates and descrambles the block. Once armed (`V32HDX_SHORT_48`
 * non-zero), just counts blocks and raises V32_FLAG_04 past a threshold.
 * Otherwise, on a `DetSequence` match, decodes the rate register via
 * `DecodeRateSeq`, switches the receiver to `RxHdxData` and to the decoded
 * `SetRxModeV32`, and arms the block counter.
 *
 * @param modem  The V.32 datapump instance.
 * @param in     Input samples to demodulate.
 * @param out    Output for the demodulated (descrambled) symbols.
 * @param count  In/out sample count, clamped by `RxClampV32` on exit.
 */
void RxHdxSequenceE(void *modem, short *in, unsigned short *out,
		    unsigned short *count);

/**
 * @brief V.32 receive state: the connected data segment.
 *
 * Demodulates and descrambles the block. No detector, no transition logic.
 *
 * @param modem  The V.32 datapump instance.
 * @param in     Input samples to demodulate.
 * @param out    Output for the demodulated (descrambled) data.
 * @param count  In/out sample count, clamped by `RxClampV32` on exit.
 */
void RxHdxData(void *modem, short *in, unsigned short *out,
	       unsigned short *count);

/**
 * @brief V.32 receive state: watch for the tone to end, then demodulate anyway.
 *
 * A lost tone transitions to the next handshake step (checked before the
 * timeout post, per the source's own note); the demodulation and
 * descrambling happen after the timeout check either way.
 *
 * @param modem  The V.32 datapump instance.
 * @param in     Input samples to demodulate.
 * @param out    Output for the demodulated (descrambled) data.
 * @param count  In/out sample count, clamped by `RxClampV32` on exit.
 */
void RxHdxToneData(void *modem, short *in, unsigned short *out,
		   unsigned short *count);

/**
 * @brief V.32 receive state: run the S-tone multi-tone detector, over whichever buffer its configured bank calls for.
 *
 * Configured with the DATA-mode S-tone coefficients (V32_S_DATA_COEF), it
 * demodulates the block first and detects over the datapump's own working
 * buffer; configured with the other bank, it detects over the caller's
 * raw input and does not demodulate at all. A detection transitions to
 * the next handshake step.
 *
 * @param modem  The V.32 datapump instance.
 * @param in     Input samples.
 * @param out    Output for demodulated data, when the DATA-mode bank is active.
 * @param count  In/out sample count, clamped by `RxClampV32` on exit.
 */
void RxHdxSTone(void *modem, short *in, unsigned short *out,
		unsigned short *count);

/**
 * @brief V.32 receive state: demodulate and watch for a decoder rate-change epoch.
 *
 * A positive EpochDetectV32() result transitions to the next handshake
 * step.
 *
 * @param modem  The V.32 datapump instance.
 * @param in     Input samples to demodulate.
 * @param out    Output for the demodulated data.
 * @param count  In/out sample count, clamped by `RxClampV32` on exit.
 */
void RxHdxEpoch(void *modem, short *in, unsigned short *out,
		unsigned short *count);

/**
 * @brief V.32 receive state: the terminal error state.
 *
 * The only one of the twelve that touches neither the context nor the
 * block counter. Raises the fault bit unconditionally -- without a
 * reason code, because whoever installed it has already posted one --
 * demodulates the block anyway, and reports no symbols.
 *
 * @param modem  The V.32 datapump instance.
 * @param in     Input samples, demodulated but discarded.
 * @param out    Scratch destination for the discarded demodulation.
 * @param count  Output: forced to 0.
 */
void RxHdxError(void *modem, short *in, unsigned short *out,
		unsigned short *count);

/**
 * @brief V.32 receive state: do nothing but watch the block counter.
 * @param modem  The V.32 datapump instance.
 * @param in     Input samples.
 * @param out    Output; not filled with demodulated data.
 * @param count  In/out sample count, clamped by `RxClampV32` on exit.
 */
void RxHdxNull(void *modem, short *in, unsigned short *out,
	       unsigned short *count);

/* --------------------------------------------------------- the dispatchers */

/*
 * The five dispatchers below are `V32NextState`'s slots (see the mode table
 * above): each advances the half-duplex handshake by one step when a spent
 * `TxHdx*`/`RxHdx*` state calls it, in some subset of: writing the next
 * handshake state to `hdx + V32HDX_STATE`, installing the next `TxHdx*`/
 * `RxHdx*` pair, reloading the step's duration, and reconfiguring the
 * datapump for what the new step needs. Each is a `switch` on the 0..33
 * handshake state (`v32state.h`) through a jump table, with an unsigned
 * out-of-range test that also catches negative states.
 */

/** @brief V.32 handshake step for the ORIGINATING side (`V32NextState[V32_MODE_ORIGINATE]`). @param modem The V.32 datapump instance. */
void V32OrgNextState(void *modem);

/** @brief V.32 handshake step for the ANSWERING side (`V32NextState[V32_MODE_ANSWER]`). @param modem The V.32 datapump instance. */
void V32AnsNextState(void *modem);

/** @brief V.32 handshake step for entering the ring/retrain sequence (`V32NextState[V32_MODE_RING_INIT]`). @param modem The V.32 datapump instance. */
void V32RngInitNextState(void *modem);

/** @brief V.32 handshake step for responding to a ring/retrain request (`V32NextState[V32_MODE_RING_RESP]`). @param modem The V.32 datapump instance. */
void V32RngRespNextState(void *modem);

/**
 * @brief V.32 handshake step for local loopback, occupying both `V32NextState[V32_MODE_LOCLOOP_2]` and `[V32_MODE_LOCLOOP_3]`.
 *
 * Occupies two slots of `V32NextState` (finding F8560) but never reads
 * `hdx + V32HDX_MODE` itself and cannot tell which slot invoked it; which
 * mode is in force is settled only by the six sites across the object
 * that write that field.
 *
 * @param modem  The V.32 datapump instance.
 */
void V32LocLoopNextState(void *modem);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32HDXST_H */

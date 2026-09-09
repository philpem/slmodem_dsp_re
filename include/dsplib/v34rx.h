/*
 * v34rx.h -- ITU-T V.34: the receiver and transmitter cores (V34RX.c/V34TX.c).
 *
 * Reconstructed under the fast pass; see docs/fastpass.md.  Structural
 * comments only, and the derivations are owed to task #47.
 */

#ifndef DSPLIB_V34RX_H
#define DSPLIB_V34RX_H

#include "dsplib/v34recv.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A sample queue: a count, two cursors, and a ring of ints of which only the
 * low half carries data.
 *
 * The ring's LENGTH is not in the object -- each function hardcodes its own
 * end address, so the receive queue's ring runs to +0x10c and the transmit
 * queue's to +0x3a4.  They are therefore different objects with the same
 * header, and the constants below are the only record of how long each is.
 */
#define V34_RXQ_END	0x10c		/* rxreadqueue's ring end   */
#define V34_TXQ_END	0x3a4		/* txwritequeue's ring end  */
#define V34_QUEUE_BURST	4		/* entries moved per call   */

/* Ring lengths in entries, from those ends: (END - 0x0c) / 4. */
#define V34_RXQ_RING	((V34_RXQ_END - 0x0c) / 4)	/*  64 */
#define V34_TXQ_RING	((V34_TXQ_END - 0x0c) / 4)	/* 230 */

struct v34_queue {
	short count;		/* +0x00  entries held, in samples       */
	int *rd;		/* +0x04  read cursor.  (+0x02..+0x04 is a
				 * pure alignment gap, not an unmodelled
				 * field -- see finding F10149.)
				 */
	int *wr;		/* +0x08  write cursor                   */
	/*
	 * +0x0c.  Declared as one entry because the two instances differ:
	 * the enclosing object carries the rest immediately after, and
	 * V34_RXQ_RING / V34_TXQ_RING say how many.
	 */
	int ring[1];
};

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
typedef char v34q_off_rd[
	((int)__builtin_offsetof(struct v34_queue, rd) == 0x04) ? 1 : -1];
typedef char v34q_off_ring[
	((int)__builtin_offsetof(struct v34_queue, ring) == 0x0c) ? 1 : -1];
#endif

/**
 * @brief Dequeue four samples from the receive queue.
 *
 * Takes four entries off the queue into the four shorts that sit
 * immediately after its ring, and drops the count by four.
 *
 * @param q  The receive queue.
 */
void rxreadqueue(struct v34_queue *q);

/**
 * @brief Enqueue four samples onto the transmit queue.
 *
 * Each entry's high half is zeroed.
 *
 * @param q    The transmit queue.
 * @param src  Four shorts to enqueue.
 */
void txwritequeue(struct v34_queue *q, const short *src);

/**
 * @brief Slice a demodulated point to the nearest constellation point.
 *
 * Finds the nearest of `npts` constellation points to the target and
 * records both the point and its index. Each point is one `int`, real in
 * the low half and imaginary in the high.
 *
 * @param d     The V.34 receiver; the decoder's state is a sub-object of it.
 * @param pts   The constellation, packed as described above.
 * @param npts  Number of points in @p pts.
 */
void decision(struct v34_receiver *d, const int *pts, short npts);

/**
 * @brief V.34's non-linear encoder for the outer constellation shells.
 *
 * Scales a complex point by a gain derived from its own magnitude, which is
 * how V.34 warps the outer shells of the constellation.
 *
 * @param in   The input complex point.
 * @param out  The scaled output complex point.
 */
void V34nlencoder(const short *in, short *out);

/**
 * @brief Recompute an adaptation step from an energy estimate.
 *
 * Replaces `*alpha` with `-(reciprocal(energy) * gain)`, then optionally
 * scales the result again by `decay`. Each stage is skipped independently:
 * a zero `energy` leaves the first alone, a zero `apply_decay` the second.
 *
 * @param alpha        The adaptation step to update.
 * @param energy       The energy estimate driving the reciprocal.
 * @param apply_decay  Non-zero to also apply the @p decay scaling.
 * @param gain         Gain applied to the reciprocal.
 * @param decay        Decay factor applied when @p apply_decay is set.
 * @param tag          Diagnostic label for this call site.
 */
void updateAlpha(short *alpha, int energy, int apply_decay, int gain,
		 int decay, const char *tag);

/** Bit 2 of `flags`: set selects the answerer's polynomial. */
#define V34_SCR_ANSWERER	0x0004

/**
 * @brief Descramble `nbits` bits, LSB first, returning them in the same order.
 *
 * V.34 gives the two ends different generators: 1 + x^-5 + x^-23 for the
 * caller, 1 + x^-18 + x^-23 for the answerer. The descrambler's state is a
 * sub-object of @p s.
 *
 * @param s      The V.34 receiver.
 * @param bits   The bits to descramble.
 * @param nbits  How many bits of @p bits are valid.
 * @return The descrambled bits.
 */
int V34descrambler(struct v34_receiver *s, short bits, short nbits);

/**
 * @brief Reset the V.34 transmit side.
 *
 * Resets both echo cancellers, both sample queues, the echo pre-filter's
 * history, and a handful of scalars.
 *
 * @param obj  The V.34 modem object. Declared `void *` because this header
 *             must not depend on v34fsk.h, which is where `struct
 *             v34_object` is declared.
 */
void txinit(void *obj);

/**
 * @brief Reset the receive timing chain and the receiver's scalars.
 * @param obj  The V.34 modem object.
 */
void rxtiminginit(void *obj);

/**
 * @brief Reset the V.34 receive side.
 *
 * Resets the equaliser, Hilbert state, AGC and scalars.
 *
 * @param obj  The V.34 modem object.
 */
void rxinit(void *obj);

/** The receive timing IIR's pole coefficients, Q14: 1.4001 and -0.9801, just inside the unit circle. */
#define V34_RXTIMING_IIR_A1	0x599b
#define V34_RXTIMING_IIR_A2	(-0x3eba)

/**
 * @brief Resample the receive signal onto the recovered clock.
 *
 * Produces `out_count` timing estimates. Takes the whole modem object
 * because it reaches both the receiver and the timing filters (at +0x50c).
 *
 * @param obj  The V.34 modem object.
 */
void rxtiming(void *obj);

/**
 * @brief Transmit one symbol.
 *
 * Modulates the symbol, enqueues it, pre-filters it, and feeds it to the
 * echo cancellers.
 *
 * @param obj  The V.34 modem object.
 */
void txmit(void *obj);

/* The AGC's freeze bit lives in v34recv.h: it is the detector-pending flag. */
#define V34_AGC_TARGET		0xfa0	/* 4000: the level it aims for   */
#define V34_AGC_DEADBAND	0x4b0	/* 1200: error ignored below this*/
#define V34_AGC_ACCUM_LIMIT	0x1f4	/*  500: integrator trip point   */
#define V34_AGC_GAIN_CEILING	0x6a00	/* gain is not raised past this  */
#define V34_AGC_SMOOTH		0x6ccd	/* 0.85 in Q15                   */
#define V34_AGC_GAIN_DOWN	0x390a	/* 0.883 in Q14                  */
#define V34_AGC_GAIN_UP		0x47cf	/* 1.122 in Q14                  */
#define V34_AGC_RMS_TAPS	36	/* the energy window, in samples */
#define V34_AGC_RMS_SCALE	0x38e	/* 910/32768 == 1/36.008         */
#define V34_AGC_RMS_FLOOR	0x1f	/* below this the AGC will not adapt */

/**
 * @brief One receive AGC adaptation step.
 * @param a  The V.34 receiver; the AGC's state is a sub-object of it.
 * @return Always 0; the effect is the updated AGC state.
 */
int agcadapt(struct v34_receiver *a);

/**
 * @brief Run the AGC over one four-sample burst from the receive queue.
 *
 * The handshake's entry point onto the AGC: runs the same measurement
 * chain as V34demodulate(), but over a whole burst and with no timing
 * recovery or down-mixing. Leaves the gained samples at +0x10c and points
 * `rx_samples` just past them.
 *
 * @param rx  The V.34 receiver.
 */
void V34agc(struct v34_receiver *rx);

/**
 * @brief One half-baud receive step: gain a sample pair, adapt, and mix it
 * down to baseband.
 *
 * Called only by rxtiming(); declared here (rather than kept file-static,
 * as it is in the object) so both sides of it can be driven directly in
 * tests, rather than only through the interpolator.
 *
 * @param rx  The V.34 receiver.
 */
void V34demodulate(struct v34_receiver *rx);

/**
 * @brief Build a twelve-short complex-multiply coefficient block.
 *
 * Takes three complex pairs from `src + 4` and emits them as conjugates,
 * then as swapped pairs.
 *
 * @param dst  Output: twelve shorts.
 * @param src  Input coefficients.
 */
void txrxdmainit(short *dst, const short *src);

/** Bit 2 of `tx_flags`: both echo cancellers have stopped adapting. */
#define V34_EC_FROZEN	0x0004
/** Bit 9 of `tx_flags`: feed the transmit sample through the cancellers at all. */
#define V34_EC_FEED	0x0200

/**
 * @brief Freeze both echo cancellers.
 *
 * Stops further adaptation and, if debugging is enabled, reports their
 * coefficients.
 *
 * @param obj  The V.34 modem object.
 */
void v34FreezeEcho(void *obj);

/**
 * @brief Scramble `nbits` bits, LSB first. The inverse of V34descrambler().
 * @param sr     The scrambler's shift register state.
 * @param mode   Non-zero selects the answerer's generator polynomial.
 * @param bits   The bits to scramble.
 * @param nbits  How many bits of @p bits are valid.
 * @return The scrambled bits.
 */
int V34scrambler(unsigned *sr, short mode, short bits, short nbits);

/**
 * @brief Install the demodulator's timing and carrier constants.
 *
 * Selects the timing constants for one of the six V.34 symbol rates and
 * the carrier table for one of the eight carriers. Unrecognised values for
 * either argument are silently ignored rather than rejected.
 *
 * @param obj      The V.34 modem object.
 * @param baud     Symbol rate selector.
 * @param carrier  Carrier frequency selector.
 */
void V34SetupDemodulator(void *obj, short baud, short carrier);

/**
 * @brief One echo-canceller step.
 *
 * Dequeues a transmit sample, filters it, subtracts it from the receive
 * path, and adapts the canceller on a schedule.
 *
 * @param obj  The V.34 modem object.
 * @return Always 0.
 */
int adaptecho(void *obj);

/**
 * @brief The per-symbol receive tick.
 *
 * Dequeues a transmit sample, cancels echo, forms a complex receive
 * sample, pushes it onto the receive queue, and adapts both echo
 * cancellers.
 *
 * @param obj  The V.34 modem object.
 * @return Always 0.
 */
int modem_serrint(void *obj);

/**
 * @brief Decode one demodulated point.
 *
 * Uses the 8D trellis decoder when all three of the 0x98 flag bits are
 * set, and a differentially-coded four-point slicer otherwise. This is
 * the handshake's decoder.
 *
 * @param obj  The V.34 modem object.
 */
void decoderv34(void *obj);

/**
 * @brief Evaluate `P(k) = -21k^2 + 837k - 354`, truncated to a short.
 * @param k  The input value.
 * @return The polynomial's value.
 */
int polyValue(short k);

/**
 * @brief Centre the interpolator's phase on the symbol.
 *
 * Derives the starting phase from the timing metric's zero crossing.
 *
 * @param obj  The V.34 modem object.
 */
void setInitialPhase(void *obj);

/**
 * @brief Install the timing loop's gains for the current state.
 * @param obj  The V.34 modem object; the state is `pllcnt`.
 */
void setTimingStateParameters(void *obj);

/**
 * @brief One step of the timing recovery loop: state machine, detector,
 * integrator.
 * @param obj  The V.34 modem object.
 */
void TimingV34(void *obj);

/**
 * @brief Reverse the low `nbits` bits of a value.
 * @param v      The value to reverse.
 * @param nbits  How many low bits to reverse.
 * @return The bit-reversed value.
 */
int bitreverse(unsigned short v, short nbits);

/**
 * @brief The per-symbol receive chain, end to end.
 *
 * Resamples onto the recovered clock, equalises, predicts, derotates,
 * decides, and closes the carrier and equaliser loops. The last, and
 * largest, function in `V34RX.c`.
 *
 * @param obj  The V.34 modem object.
 */
void receiver(void *obj);

/**
 * @brief V.34's PP training sequence.
 *
 * Forty-eight complex points, packed as (re, im), every one of magnitude
 * 6476 at a multiple of 60 degrees. Lives at `.rodata + 0x2c80` in the
 * blob, and is `extern` (rather than file-local) because it has two
 * readers with two different element widths: receiver() slices it as
 * ninety-six shorts, while `v34hstx1.cpp`'s table 1 `PPSEG` transmits it
 * as forty-eight four-byte points. Defined in V34RX.c.
 */
#define V34_VECTPP_POINTS	48
extern const short vectpp[2 * V34_VECTPP_POINTS];

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34RX_H */

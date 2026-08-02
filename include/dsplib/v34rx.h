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
	short pad_02;
	int *rd;		/* +0x04  read cursor                    */
	int *wr;		/* +0x08  write cursor                   */
	/*
	 * +0x0c.  Declared as one entry because the two instances differ:
	 * the enclosing object carries the rest immediately after, and
	 * V34_RXQ_RING / V34_TXQ_RING say how many.
	 */
	int ring[1];
};

/*
 * Take four entries off the receive queue into the four shorts that sit
 * immediately after its ring, and drop the count by four.
 */
void rxreadqueue(struct v34_queue *q);

/* Put four shorts onto the transmit queue, zeroing each entry's high half. */
void txwritequeue(struct v34_queue *q, const short *src);

/*
 * The decoder's state, mapped where `decision` touches it.  A sub-object of
 * the V.34 receiver; the pads are not a claim about their contents.
 */


/*
 * Slice: find the nearest of `npts` constellation points to the target, and
 * record both the point and its index.
 *
 * Each point is one int, real in the low half and imaginary in the high.
 */
void decision(struct v34_receiver *d, const int *pts, short npts);

/*
 * The non-linear encoder: scale a complex point by a gain derived from its
 * own magnitude, which is V.34's warping of the outer constellation shells.
 */
void V34nlencoder(const short *in, short *out);

/*
 * Recompute an adaptation step from an energy estimate.
 *
 * `*alpha` is replaced by -(reciprocal(energy) * gain), and then optionally
 * scaled again by `decay`.  Both stages are skipped independently: a zero
 * `energy` leaves the first alone and a zero `apply_decay` the second.
 */
void updateAlpha(short *alpha, int energy, int apply_decay, int gain,
		 int decay, const char *tag);

/*
 * The descrambler's state, mapped where V34descrambler touches it.
 */


/* Bit 2 of `flags`: set selects the answerer's polynomial. */
#define V34_SCR_ANSWERER	0x0004

/*
 * Descramble `nbits` bits, LSB first, returning them in the same order.
 *
 * V.34 gives the two ends different generators and this is both of them:
 * 1 + x^-5 + x^-23 for the caller, 1 + x^-18 + x^-23 for the answerer.
 */
int V34descrambler(struct v34_receiver *s, short bits, short nbits);

/*
 * Reset the transmit side: both echo cancellers, both sample queues, the
 * echo pre-filter's history, and a handful of scalars.
 *
 * Declared `void *` for the same reason the other whole-object functions
 * are -- v34rx.h must not depend on v34fsk.h, since the dependency runs the
 * other way.
 */
void txinit(void *obj);

/* Reset the receive timing chain and the receiver's scalars. */
void rxtiminginit(void *obj);

/* Reset the receive side: equaliser, Hilbert state, AGC and scalars. */
void rxinit(void *obj);

/* The timing IIR's poles, Q14: 1.4001 and -0.9801, just inside the circle. */
#define V34_RXTIMING_IIR_A1	0x599b
#define V34_RXTIMING_IIR_A2	(-0x3eba)

/*
 * Resample onto the recovered clock, producing f128 timing estimates.
 * Takes the whole object: it reaches both the receiver and the timing
 * filters at +0x50c.
 */
void rxtiming(void *obj);

/* Modulate one symbol, enqueue it, pre-filter it, feed the echo cancellers. */
void txmit(void *obj);

/*
 * The receive AGC's state, mapped where `agcadapt` touches it.
 */


/* The freeze bit lives in v34recv.h: it is the detector-pending flag. */
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

/*
 * One AGC step.  Always returns zero; the state is the output.
 */
int agcadapt(struct v34_receiver *a);

/*
 * Pull one burst from the receive queue, gain it in place, and run the AGC.
 *
 * The handshake's entry point: the same measurement chain V34demodulate runs
 * per sample-pair, but over a whole four-sample burst and with no timing
 * recovery or down-mixing.  It leaves the gained samples at +0x10c and points
 * `rx_samples` just past them.
 */
void V34agc(struct v34_receiver *rx);

/*
 * Build the twelve-short complex-multiply coefficient block: three pairs
 * from `src + 4`, emitted as conjugates then as swapped pairs.
 */
void txrxdmainit(short *dst, const short *src);

/* Bit 2 of f25c2: both echo cancellers have stopped adapting. */
#define V34_EC_FROZEN	0x0004

/* Freeze both cancellers, and report their coefficients if debugging. */
void v34FreezeEcho(void *obj);

/*
 * Scramble `nbits` bits, LSB first.  `mode` non-zero selects the answerer's
 * generator.  The inverse of V34descrambler, and the same two polynomials.
 */
int V34scrambler(unsigned *sr, short mode, short bits, short nbits);

/*
 * Install the timing constants for one of the six V.34 symbol rates and the
 * carrier table for one of the eight carriers.  Unrecognised values for
 * either are ignored rather than rejected.
 */
void V34SetupDemodulator(void *obj, short baud, short carrier);

/* Reverse the low `nbits` bits of `v`. */
int bitreverse(unsigned short v, short nbits);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34RX_H */

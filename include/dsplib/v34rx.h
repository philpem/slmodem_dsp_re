/*
 * v34rx.h -- ITU-T V.34: the receiver and transmitter cores (V34RX.c/V34TX.c).
 *
 * Reconstructed under the fast pass; see docs/fastpass.md.  Structural
 * comments only, and the derivations are owed to task #47.
 */

#ifndef DSPLIB_V34RX_H
#define DSPLIB_V34RX_H

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

struct v34_queue {
	short count;		/* +0x00  entries held, in samples       */
	short pad_02;
	int *rd;		/* +0x04  read cursor                    */
	int *wr;		/* +0x08  write cursor                   */
	int ring[1];		/* +0x0c  length is the caller's         */
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
struct v34_decoder {
	unsigned char unmapped_000[0x126];
	short best_index;	/* +0x126  index of the nearest point    */
	unsigned char unmapped_128[0x20c - 0x128];
	int best_point;		/* +0x20c  the point itself              */
	short target_re;	/* +0x210  what we are deciding on       */
	short target_im;	/* +0x212                                */
};

/*
 * Slice: find the nearest of `npts` constellation points to the target, and
 * record both the point and its index.
 *
 * Each point is one int, real in the low half and imaginary in the high.
 */
void decision(struct v34_decoder *d, const int *pts, short npts);

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
		 int decay, int tag);

/*
 * The descrambler's state, mapped where V34descrambler touches it.
 */
struct v34_scrambler {
	unsigned char unmapped_000[0x122];
	unsigned short flags;	/* +0x122  bit 2 picks the polynomial    */
	unsigned char unmapped_124[0x1a4 - 0x124];
	unsigned sr;		/* +0x1a4  the shift register            */
};

/* Bit 2 of `flags`: set selects the answerer's polynomial. */
#define V34_SCR_ANSWERER	0x0004

/*
 * Descramble `nbits` bits, LSB first, returning them in the same order.
 *
 * V.34 gives the two ends different generators and this is both of them:
 * 1 + x^-5 + x^-23 for the caller, 1 + x^-18 + x^-23 for the answerer.
 */
int V34descrambler(struct v34_scrambler *s, short bits, short nbits);

/* Reverse the low `nbits` bits of `v`. */
int bitreverse(unsigned short v, short nbits);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34RX_H */

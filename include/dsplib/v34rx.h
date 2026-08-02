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

/* Reverse the low `nbits` bits of `v`. */
int bitreverse(unsigned short v, short nbits);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34RX_H */

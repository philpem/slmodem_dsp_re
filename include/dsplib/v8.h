/*
 * v8.h -- V.8 handshake.
 *
 * V.8 is the negotiation that happens once a call is answered and before any
 * datapump starts: the answering modem sends ANSam, the calling modem offers
 * a menu of modulations in a CM sequence, and the answerer picks one in JM.
 * Everything above 2400 bit/s in this library is chosen here.
 *
 * The public surface is unusually cooperative for this object file -- every
 * entry point below is a global symbol, so all of it can be driven by name
 * in a differential test, unlike the call-progress code where most of the
 * work lived in file statics.
 *
 * Reconstruction order is bottom-up, because `V8Create` reaches the signal
 * layer through `v8handshakinit` and the signal layer reaches the arithmetic
 * leaves in v8util.c.
 */

#ifndef DSPLIB_V8_H
#define DSPLIB_V8_H

/* One cycle of cosine, in 256 steps. */
#define V8_COSTAB_SIZE	256

extern const short v8_costab[V8_COSTAB_SIZE];

/*
 * A bin of the handshake's DFT.  Sixteen bytes; `v8_dftenergy` reads the two
 * halves of the complex value and writes the magnitude squared beside them.
 */
struct v8_dft_bin {
	int	f00;			/* +0x00 */
	int	re;			/* +0x04 */
	int	im;			/* +0x08 */
	short	energy;			/* +0x0c */
	short	f0e;			/* +0x0e */
};

/*
 * The handshake state.  Only the CRC register is known so far -- the rest
 * arrives with `v8handshakinit`, which is what fills it.
 */
struct v8_handshake {
	unsigned char	pad[0x1e];	/* +0x00 */
	short		crc;		/* +0x1e */
};

short v8_mpyint(short a, short b);
short v8_absfn(short x);
short v8_cosread(unsigned char phase);
void v8_crc(struct v8_handshake *hs, int bit);
void v8_copycoeff(short *dst, const short *src, short n);
void v8_dftenergy(struct v8_dft_bin *bin, short n, short shift);

#endif /* DSPLIB_V8_H */

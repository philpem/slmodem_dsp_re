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

/*
 * The handshake object.  3780 bytes, allocated by `V8Create`.  Only the two
 * regions below are mapped so far -- the initialisers reveal them, and the
 * rest is named as later functions claim it.  The padding is explicit rather
 * than implied so that every offset stays checkable against the object.
 */
#define V8_STATE_BYTES		0xec4

/*
 * The phase-reversal detector, at +0xb40.  ANSam is a 2100 Hz tone whose
 * phase inverts every 450 ms, and this is what watches for the inversions:
 * three accumulators, a window of 64 samples, and a countdown.
 */
struct v8_phase_rev {
	int	f00;				/* +0x00 */
	int	f04;				/* +0x04 */
	int	f08;				/* +0x08 */
	short	f0c;				/* +0x0c */
	short	f0e;		/* set to 0x20   +0x0e */
	short	f10;				/* +0x10 */
	short	f12;				/* +0x12 */
	short	window[64];			/* +0x14 */
	unsigned char pad94[0xdc - 0x94];
	short	fdc;				/* +0xdc */
	unsigned char padde[0x114 - 0xde];
};

/*
 * The V.21 modem V.8 signals over, at +0xdd8.  `V8_setFilters` swaps the four
 * coefficient pointers -- which is how one modem serves both channels -- and
 * `V8_V21_reset` clears the delay line between uses.
 *
 * It starts at +0xdd8 rather than at the tone-queue fields just before it
 * because those are shorts and these are pointers: putting them in one struct
 * makes the compiler realign, and the offsets stop matching the object.  The
 * compile-time assertions in v8util.c caught exactly that.
 */
#define V8_V21_DELAY	40

struct v8_v21 {
	/* The four filter designs, swapped as a set. */
	const short	*a;			/* +0x00  obj +0xdd8 */
	const short	*b;			/* +0x04 */
	const short	*c;			/* +0x08 */
	const short	*d;			/* +0x0c */

	int	f10;				/* +0x10  obj +0xde8 */
	int	f14;				/* +0x14 */
	int	f18;				/* +0x18 */
	unsigned char pad1c[0x34 - 0x1c];
	short	delay[V8_V21_DELAY];		/* +0x34  obj +0xe0c */
};

struct v8 {
	unsigned char		pad000[0xb40];
	struct v8_phase_rev	phase_rev;	/* +0xb40 */
	unsigned char		padc54[0xdd2 - 0xc54];

	/* The tone queue, which v8_TONEq_init arms. */
	short			toneq_pending;	/* +0xdd2 */
	short			toneq_period;	/* +0xdd4 */
	unsigned char		paddd6[2];

	struct v8_v21		v21;		/* +0xdd8 */
	unsigned char		pade5c[V8_STATE_BYTES - 0xe5c];
};

/*
 * Point the V.21 modem at a set of filter designs.  Called from
 * `v8_V21_Init`, and again whenever the handshake changes direction.
 */
void V8_setFilters(struct v8 *v, const short *a, const short *b,
		   const short *c, const short *d);

/* Clear the V.21 delay line and its three accumulators. */
void V8_V21_reset(struct v8 *v);

/* Arm the tone queue. */
void v8_TONEq_init(struct v8 *v);

/* Arm the ANSam phase-reversal detector. */
void v8_phase_rev_init(struct v8_phase_rev *pr);

short v8_mpyint(short a, short b);
short v8_absfn(short x);
short v8_cosread(unsigned char phase);
void v8_crc(struct v8_handshake *hs, int bit);
void v8_copycoeff(short *dst, const short *src, short n);
void v8_dftenergy(struct v8_dft_bin *bin, short n, short shift);

#endif /* DSPLIB_V8_H */

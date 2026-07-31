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

/*
 * The receiver, at +0x1c.  The original works through a base pointer held in
 * a register rather than through the object, which is what marks it out as a
 * sub-object rather than a scattering of fields.
 */
struct v8_rx {
	unsigned char	pad00[0x10];
	short		*buf;			/* +0x10  -> v8.rx_scratch */
	int		f14;			/* +0x14 */
	unsigned char	pad18[2];
	short		f1a;			/* +0x1a */
	short		f1c;		/* 0x200     +0x1c */
	short		f1e;			/* +0x1e */
	short		f20;		/* 0x3333    +0x20 */
	short		hist[48];		/* +0x22 */
	short		f82;			/* +0x82 */
	short		f84;			/* +0x84 */
	short		f86;		/* 0x200     +0x86 */
	short		f88;			/* +0x88 */
	short		f8a;			/* +0x8a */
	unsigned char	pad8c[0xac - 0x8c];
	short		fac;			/* +0xac */
	unsigned char	padae[0xc2 - 0xae];
	short		fc2;		/* 0x50      +0xc2 */
	unsigned char	padc4[2];
	short		fc6;			/* +0xc6 */
	short		fc8;			/* +0xc8 */
	unsigned char	padca[0xd8 - 0xca];
	short		fd8;			/* +0xd8 */
	short		fda;			/* +0xda */
	unsigned char	paddc[0xdc - 0xdc];
};

/* Sizes of the buffers the transmitter and receiver clear. */
#define V8_TX_SYMBOLS	128		/* +0x11c */
#define V8_TX_RING	460		/* +0x228 */
#define V8_TX_SHAPE	140		/* +0x77c */
#define V8_RX_SCRATCH	160		/* +0x894 */
#define V8_RX_HIST	48		/* +0x22 of the receiver */
#define V8_TX_RING_HALF	64		/* where the write cursor starts */

/*
 * The receiver's scratch buffer carries one value that is not zero: element
 * 40 is set to 0x1000 straight after the clear, so the initialiser writes
 * over what it has just written.  Reproduced in that order.
 */
#define V8_RX_SCRATCH_SEED_INDEX	40
#define V8_RX_SCRATCH_SEED		0x1000

struct v8 {
	unsigned char		pad000[4];
	int			f004;		/* +0x004 */
	unsigned char		pad008[4];
	short			f00c;		/* +0x00c */
	unsigned char		pad00e[6];
	short			f014;		/* 1        +0x014 */
	unsigned char		pad016[2];
	short			f018;		/* +0x018 */
	unsigned char		pad01a[2];

	struct v8_rx		rx;		/* +0x01c */

	unsigned char		pad0f8[0x110 - 0xf8];
	short			f110;		/* +0x110 */
	unsigned char		pad112[2];
	short			*tx_sym_a;	/* +0x114 -> tx_symbols */
	short			*tx_sym_b;	/* +0x118 -> tx_symbols */
	short			tx_symbols[V8_TX_SYMBOLS];	/* +0x11c */
	short			f21c;		/* 0x20     +0x21c */
	unsigned char		pad21e[2];
	short			*tx_ring_base;	/* +0x220 -> tx_ring[0]  */
	short			*tx_ring_half;	/* +0x224 -> tx_ring[64] */
	short			tx_ring[V8_TX_RING];		/* +0x228 */
	unsigned char		pad5c0[8];
	short			rx_scratch2[(0x77c - 0x5c8) / 2];/* +0x5c8 */
	short			tx_shape[V8_TX_SHAPE];		/* +0x77c */
	short			rx_scratch[V8_RX_SCRATCH];	/* +0x894 */

	unsigned char		pad9d4[0xb40 - 0x9d4];
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

/* Arm the transmitter and the receiver.  Both always return 0. */
int v8_txinit(struct v8 *v);
int v8_rxinit(struct v8 *v);

#endif /* DSPLIB_V8_H */

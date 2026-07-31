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
	/*
	 * A phase accumulator per bin, stepped by its own increment: this is
	 * a sliding DFT, one oscillator per frequency of interest, not a
	 * transform over a block.
	 */
	short	phase;			/* +0x00 */
	short	step;			/* +0x02 */
	int	re;			/* +0x04 */
	int	im;			/* +0x08 */
	short	energy;			/* +0x0c */
	short	f0e;			/* +0x0e */
};

/*
 * The handshake state.  Only the CRC register is known so far -- the rest
 * arrives with `v8handshakinit`, which is what fills it.
 */
/*
 * What `v8_crc` is handed.  It is a `struct v8_tx_sequence` -- the CRC lives
 * at +0x1e of one -- kept as its own name only because v8_crc was
 * reconstructed before that was known.
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
 * The tone detector, at +0xad8.  Everything above the fixed fields is a set
 * of small accumulator arrays: two two-by-two, then two of three.
 */
struct v8_detector {
	/*
	 * A coefficient table in .rodata, not a number: v8handshakinit passes
	 * an address here.  Typed as an int until the caller was read, which
	 * a 32-bit differential test could never have caught -- both are four
	 * bytes and the value copies through either way.
	 */
	const short	*table;			/* +0x00 */
	short	f04;				/* +0x04 */
	short	f06;				/* +0x06 */
	short	f08;		/* the negated argument  +0x08 */
	short	f0a;				/* +0x0a */
	short	f0c;		/* 1                     +0x0c */
	short	f0e;				/* +0x0e */
	short	f10;				/* +0x10 */
	short	f12;				/* +0x12 */
	short	acc_a[4];			/* +0x14 */
	short	acc_b[4];			/* +0x1c */
	short	acc_c[3];			/* +0x24 */
	short	acc_d[3];			/* +0x2a */
	short	f30;				/* +0x30 */
	unsigned char pad32[0x68 - 0x32];
};

/* What v8_detectorinit sets in the receiver's flag word. */
#define V8_RX_DETECTOR_ARMED	0x200

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
	unsigned char padde[0xe0 - 0xde];
};

/*
 * The V.21 modem's working parameters, at +0xc20.  `v8_V21_Init` sets them
 * from two independent choices: which channel is being sent (which picks the
 * carrier constants and the 61-tap filter) and whether this modem answered
 * the call (which picks the four filter designs and two more constants).
 */
struct v8_v21_params {
	short	f00;				/* +0xc20 */
	short	carrier_a;	/* 0x62b or 0x3ef  +0xc22 */
	short	carrier_b;	/* 0x580 or 0x344  +0xc24 */
	short	f06;		/* 0x20            +0xc26 */
	short	f08;				/* +0xc28 */
	short	f0a;		/* scaled by v8_mpyint  +0xc2a */
	short	f0c;		/* 4 or 7          +0xc2c */
	short	f0e;		/* -100 or 0       +0xc2e */
	short	f10;				/* +0xc30 */
	short	f12;		/* 1               +0xc32 */
	short	f14;		/* 0x18            +0xc34 */
	short	f16;				/* +0xc36 */
	short	f18;				/* +0xc38 */
	short	f1a;				/* +0xc3a */
	short	f1c;				/* +0xc3c */
	short	f1e;				/* +0xc3e */
	short	f20;				/* +0xc40 */
	short	f22;				/* +0xc42 */
	short	f24;				/* +0xc44 */
	short	f26;				/* +0xc46 */
};

/*
 * A tone generator's working parameters.  Sixteen bytes, and the same shape
 * appears at +0xda4 with its own constants.
 */
struct v8_tone {
	short	f00;				/* +0x00 */
	short	f02;				/* +0x02 */
	short	f04;		/* 0x1a          +0x04 */
	short	f06;		/* 0xe00         +0x06 */
	short	f08;		/* scaled        +0x08 */
	short	f0a;				/* +0x0a */
	short	f0c;				/* +0x0c */
	short	f0e;		/* 1             +0x0e */
};

/*
 * The transmit sequence: the CM or JM about to go on the wire, as 10-bit
 * V.21 characters -- start bit, eight data bits least significant first, stop
 * bit -- terminated by 0xffff, followed by the transmitter's control block.
 *
 * `nbits` is the character count times ten, which is what confirms the
 * entries are ten bits each rather than bytes with framing added later.
 */
#define V8_TX_SEQ_WORDS	15

struct v8_tx_sequence {
	short	word[V8_TX_SEQ_WORDS];		/* +0x00 */
	/*
	 * CRC-16-CCITT, and 0xffff is its initial value rather than a
	 * terminator: `v8_getbit` folds each bit it hands out into this as it
	 * goes, and appends the finished register to the message.  It was
	 * called a terminator for as long as only the builder had been read.
	 */
	short	crc;				/* +0x1e */
	short	crc_enable;			/* +0x20 */
	short	nbits;		/* words * 10   +0x22 */
	short	bitpos;		/* bits handed out   +0x24 */
	short	wordbits;	/* 10               +0x26 */
	short	wordidx;			/* +0x28 */
	short	repeat;		/* 1                +0x2a */
	short	repeats;			/* +0x2c */
	short	f2e;				/* +0x2e */
	int	shifter;	/* the bits being handed out  +0x30 */
	short	nleft;		/* how many are still in it   +0x34 */
	short	f36;				/* +0x36 */
	int	shifter0;	/* both restored on repeat    +0x38 */
	short	nleft0;				/* +0x3c */
	short	f3e;				/* +0x3e */
};

/*
 * The call menu itself -- the bits V.8 is actually negotiating over.  Three
 * flag bytes and two optional four-byte extensions, which is what the
 * standard calls the country code and vendor-specific fields.
 */
struct v8_cm {
	unsigned char	b0;			/* +0x00 */
	unsigned char	b1;			/* +0x01 */
	unsigned char	b2;			/* +0x02 */
	unsigned char	b3;			/* +0x03 */
	unsigned char	pad04[0x10 - 4];
	/* The modulation list, read as one word when the JM is built. */
	int		menu;			/* +0x10 */
	unsigned char	pad14[0x18 - 0x14];
	unsigned char	ext1[4];		/* +0x18 */
	unsigned char	ext2[4];		/* +0x1c */
};

/* At most four characters are taken from each extension field. */
#define V8_CM_EXT_MAX	4

/* Bits of v8_cm.b2 saying whether each extension is present. */
#define V8_CM_EXT1_PRESENT	0x04
#define V8_CM_EXT2_PRESENT	0x08

/* The 61-tap filter v8_V21_Init copies in, chosen by channel. */
#define V8_V21_TAPS	61

/* What v8_V21_Init sets in the receiver's flag word. */
#define V8_RX_V21_ARMED	0x800

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
	unsigned char	pad00[0x0a];
	/* `v8_detectorinit` sets bit 9 here; the rest is not yet known. */
	unsigned short	flags;			/* +0x0a */
	unsigned char	pad0c[4];
	short		*buf;			/* +0x10  -> v8.rx_stage  */
	short		f14;			/* +0x14 */
	short		f16;			/* +0x16 */
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
	/*
	 * Two four-sample staging buffers.  `v8_txwritequeue` copies out of
	 * the first into the transmit ring; `v8_rxreadqueue` copies into the
	 * second out of the symbol buffer.  Four samples at a time is the
	 * handshake's block.
	 */
	short			tx_stage[4];	/* +0x5c0 */
	short			rx_stage[(0x77c - 0x5c8) / 2];	/* +0x5c8 */
	short			tx_shape[V8_TX_SHAPE];		/* +0x77c */
	short			rx_scratch[V8_RX_SCRATCH];	/* +0x894 */

	unsigned char		pad9d4[0x9d4 - 0x9d4];
	short			f9d4;		/* +0x9d4 */
	short			f9d6;		/* +0x9d6 */
	short			f9d8;		/* +0x9d8 */
	unsigned char		pad9da[0xa3e - 0x9da];

	short			fa3e;		/* 0x10     +0xa3e */
	short			fa40;		/* 0x200    +0xa40 */
	short			fa42;		/* +0xa42 */

	/*
	 * The configuration V8Create plants, which v8handshakinit reads back.
	 * `mode` picks between three whole shapes of handshake and is the
	 * first thing looked at; anything but 0 or 1 makes the function
	 * return having done only the common preamble.
	 */
	int			mode;		/* +0xa44 */
	int			fa48;		/* +0xa48 */
	int			timeout_a;	/* +0xa4c */
	int			timeout_b;	/* +0xa50 */
	int			fa54;		/* +0xa54 */

	struct v8_cm		*cm;		/* +0xa58 */
	short			v21_taps[V8_V21_TAPS];	/* +0xa5c */
	unsigned char		pada_d6[2];
	struct v8_detector	detector;	/* +0xad8 */
	struct v8_phase_rev	phase_rev;	/* +0xb40 */
	struct v8_v21_params	v21_params;	/* +0xc20 */

	/* Which sequence is being sent, and two more of the five. */
	struct v8_tx_sequence	*tx_seq;	/* +0xc48 */
	struct v8_tx_sequence	*seq_alt;	/* +0xc4c */
	struct v8_tx_sequence	*seq_spare;	/* +0xc50 */

	/*
	 * Five sequence buffers in a row.  That they are exactly five, and
	 * exactly 0x40 bytes each, is confirmed by the object rather than
	 * assumed: the hand-built one at +0xd14 puts its terminator at +0xd32
	 * and its length at +0xd36, which is where struct v8_tx_sequence puts
	 * them, and 60 bits is exactly the six words written above it.
	 */
	struct v8_tx_sequence	seq[5];		/* +0xc54 */

	short			fd94;		/* +0xd94 */
	short			fd96;		/* 0x1a     +0xd96 */
	int			fd98;		/* +0xd98 */
	int			fd9c;		/* +0xd9c */
	short			fda0;		/* +0xda0 */
	unsigned char		padda2[2];

	/*
	 * A tone generator, laid out like the V.21 parameters but with its own
	 * constants -- this is the one the answering side uses.
	 */
	struct v8_tone		tone;		/* +0xda4 */

	short			fdb4;		/* +0xdb4 */
	short			fdb6;		/* +0xdb6 */
	short			fdb8;		/* +0xdb8 */
	short			fdba;		/* +0xdba */
	unsigned char		paddbc[2];
	short			fdbe;		/* +0xdbe */
	short			fdc0;		/* +0xdc0 */
	unsigned char		paddc2[2];
	int			fdc4;		/* +0xdc4 */
	int			fdc8;		/* +0xdc8 */
	int			fdcc;		/* +0xdcc */
	short			fdd0;		/* +0xdd0 */

	short			toneq_pending;	/* +0xdd2 */
	short			toneq_period;	/* +0xdd4 */
	unsigned char		paddd6[2];
	struct v8_v21		v21;		/* +0xdd8 */

	/* The two timeouts, in samples, and a counter. */
	int			deadline_a;	/* +0xe5c */
	int			deadline_b;	/* +0xe60 */
	int			fe64;		/* +0xe64 */
	unsigned char		pade68[0xeb8 - 0xe68];
	int			feb8;		/* +0xeb8 */
	short			febc;		/* +0xebc */
	short			febe;		/* +0xebe */
	short			fec0;		/* +0xec0 */
	short			fec2;		/* +0xec2 */
	unsigned char		padec4[V8_STATE_BYTES - 0xec4];
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

/*
 * Reverse the eight bits of a byte.
 *
 * V.8 transmits its octets least significant bit first, so every byte of a
 * CM or JM sequence passes through here on its way out.  The original does it
 * as two table lookups on the nibbles, with the halves swapped -- reversing
 * each nibble and exchanging them is the same as reversing all eight bits.
 */
unsigned char charFlip(unsigned char b);

short v8_mpyint(short a, short b);
short v8_absfn(short x);
short v8_cosread(unsigned char phase);
void v8_crc(struct v8_handshake *hs, int bit);
void v8_copycoeff(short *dst, const short *src, short n);
void v8_dftenergy(struct v8_dft_bin *bin, short n, short shift);

/*
 * Arm the tone detector.  Eight arguments: the object (whose receiver gets a
 * flag set), the detector itself, and six configuration values.  `a5` is
 * stored negated, which is the only one that is not a straight copy.
 */
void v8_detectorinit(struct v8 *v, struct v8_detector *d, const short *table,
		     short a3, short a4, short a5, short a6, short a7);

/*
 * Bring up the V.21 modem V.8 signals over.  `channel` picks which of the two
 * V.21 channels this modem transmits on, and `answerer` whether it answered
 * the call; the two choices are independent and pick different things.
 */
void v8_V21_Init(struct v8 *v, short channel, short answerer);

/*
 * Build the CM or JM sequence about to be transmitted, from the call menu at
 * `v->cm` into the buffer at `v->tx_seq`.  Both are set by the caller, which
 * is how one function serves both messages.
 */
void initTxSequence(struct v8 *v);

/*
 * Lay out the handshake from the configuration V8Create planted.  Reads
 * `v->mode` and builds one of three shapes.
 */
void v8handshakinit(struct v8 *v);

/*
 * What V8Create is handed: six words the object keeps and reads back from
 * v8handshakinit onwards.
 */
struct v8_cfg {
	int		mode;			/* +0x00 -> v8.mode      */
	int		f04;			/* +0x04 */
	int		timeout_a;		/* +0x08 */
	int		timeout_b;		/* +0x0c */
	int		f10;			/* +0x10 */
	struct v8_cm	*cm;			/* +0x14 */
};

/*
 * Build a handshake.  Allocates 3780 bytes and does NOT zero them: only the
 * fields below and whatever v8handshakinit writes are defined afterwards.
 */
struct v8 *V8Create(const struct v8_cfg *cfg);

/* Free it.  Tolerates NULL. */
void V8Delete(struct v8 *v);

/*
 * Read back the message that was received, as octets.
 *
 * This is the inverse of what `initTxSequence` builds: each 10-bit character
 * has its framing shifted off and its bits put back in order, so a CM or JM
 * captured off the line becomes the bytes the standard describes.  It is the
 * decode half of V.8 and the one thing needed to watch a negotiation.
 *
 * `count` is in/out: the caller's capacity going in, the number of octets
 * written coming out.  Returns 0 normally, the full length when the message
 * did not fit (so the caller can tell truncation from a short message), and
 * -1 when there is nothing to read.
 */
#define V8_GET_EMPTY	(-1)

int V8GetMessage(struct v8 *v, unsigned char *out, int *count);

/*
 * Put a message of your own into one of the five buffers, as octets.  The
 * counterpart to V8GetMessage and the same framing: each octet is reversed,
 * shifted up one and given a low bit.
 *
 * `which` selects the buffer, and the mapping is not the order they sit in
 * memory -- 1 and 2 are swapped.  Returns 0 normally, V8_SET_TRUNCATED when
 * the message was longer than a buffer holds and only the first fifteen
 * octets went in, and -1 for an unknown selector or an empty message.
 */
#define V8_SET_CM	0
#define V8_SET_JM	1
#define V8_SET_CJ	2
#define V8_SET_CI	3

#define V8_SET_TRUNCATED	15
#define V8_SET_REJECTED		(-1)

int V8SetMessage(struct v8 *v, int which, const unsigned char *octets, int n);

/*
 * Hand out the next bit of a sequence, least significant first, folding each
 * into the CRC on the way.  Returns 0 or 1, or -1 when the sequence is
 * finished and not set to repeat.
 */
#define V8_GETBIT_END	(-1)

int v8_getbit(struct v8_tx_sequence *s);

/* Arm the transmitter and the receiver.  Both always return 0. */
int v8_txinit(struct v8 *v);
int v8_rxinit(struct v8 *v);

/* Arm the ANSam tone generator.  The same fields v8handshakinit sets inline. */
void v8_ansaminit(struct v8 *v);

/* Four samples of the queued tone, from the phase accumulator. */
void v8_TONEq_generate(struct v8 *v, short *out);

/* Move four samples between the rings and their staging buffers. */
int v8_rxreadqueue(struct v8 *v);
int v8_txwritequeue(struct v8 *v);

/* One sample through the 61-tap transmit shaping filter. */
short v8_fsktxfilter(struct v8 *v, short sample);

/*
 * Advance a sliding DFT.  Each bin has its own phase accumulator and step,
 * and takes `nsamples` samples into its running real and imaginary sums.
 */
void v8_dftupdate(struct v8_dft_bin *bins, short nbins, const short *samples,
		  short nsamples);

/*
 * Four samples of FSK.  `which` picks the mark or the space carrier; the
 * result goes through the shaping filter and straight into the transmit ring.
 */
int v8_fskmodulate(struct v8 *v, short which);

/* One step of the receive AGC. */
int v8_agcadapt(struct v8 *v);

/*
 * Four samples of ANSam: a carrier amplitude-modulated by a second, slower
 * oscillator, with the amplitude negated every V8_ANSAM_REVERSAL blocks --
 * which is the periodic phase reversal that distinguishes ANSam from a plain
 * answer tone.
 */
#define V8_ANSAM_REVERSAL	0x438
#define V8_ANSAM_DEPTH		0xccd	/* Q14: 0.05 */
#define V8_ANSAM_UNITY		0x4000	/* Q14: 1.0  */

void v8_ansamgenerate(struct v8 *v, short *out);

/*
 * Nudge the handshake from outside.  Three requests, each valid only from one
 * state; returns 0 when it was accepted and -1 when it was not, including for
 * an unknown request.
 */
#define V8_CONTROL_START	0
#define V8_CONTROL_ANSWER	1
#define V8_CONTROL_PROCEED	2

int V8Control(struct v8 *v, int what);

/* How many samples each queue operation moves. */
#define V8_QUEUE_BLOCK	4

/*
 * Where the transmit ring ends, in samples, and the top of the shaping
 * filter's delay line.  Both come out of the object rather than the array
 * sizes: the ring's wrap point is +0x5c0 and the filter's line starts at
 * +0x90c, which is the sixtieth sample of the receive scratch.
 */
#define V8_TX_RING_END	((0x5c0 - 0x228) / 2)
#define V8_FSK_TAP_TOP	((0x90c - 0x894) / 2)

#endif /* DSPLIB_V8_H */

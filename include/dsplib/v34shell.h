/*
 * v34shell.h -- ITU-T V.34: the shell mapper's index arithmetic.
 *
 * V.34 9.4 (shell mapping; 9.3 is the parser, per T-REC-V.34-199802) maps a
 * group of eight sub-indices onto one index into a shell of the
 * constellation, so that points of equal energy are equally likely -- the
 * shaping that buys V.34 its ~0.8 dB over a uniform mapping.  This is the
 * inverse direction, used by the receiver.
 *
 * THE OBJECT IS PARTIALLY MAPPED, and deliberately so: `shellDemapper` is
 * being reconstructed ahead of the rest of its module because it is a leaf
 * -- no calls, no relocations at all -- and therefore differentially
 * testable on its own, whereas `demapFrame` above it needs `decodeDepth`
 * and `putFrame` first.  Only the fields it reads are named; the pads are
 * not a claim about what else is in there.
 *
 * The three cumulative-count tables are ring buffers of partial sums, and
 * their lengths below are bounded by the next field whose offset is known,
 * not by anything the code proves.  Whoever fills in `demapFrame` should
 * pin them properly.
 */

#ifndef DSPLIB_V34SHELL_H
#define DSPLIB_V34SHELL_H

#ifdef __cplusplus
extern "C" {
#endif

#define V34_SHELL_SUBS		8	/* sub-indices per group          */

/*
 * decodeDepth's two tables.  See the notes in v34shell.c: kLookup is the
 * quadrant group as a 4x4 Latin square, grid is a 529-entry map indexed by
 * `(x + 0x408) >> 2`.
 */
extern const short kLookup[16];
extern const short grid[529];

/* The bit sink putFrame writes through: (context, value, bit count). */
typedef void (*v34_putbits_fn)(void *shell, int value, int nbits);

struct v34_shell {
	unsigned char pad_000[0xa00];
	short           fa00;			/* +0xa00 */
	unsigned char pad_a02[0xa04 - 0xa02];
	short           fa04;			/* +0xa04 */
	short           fa06;			/* +0xa06 */
	short           fa08;			/* +0xa08 an accumulator putFrame
						 *  advances and folds back */
	unsigned char pad_a0a[0xa0e - 0xa0a];
	short           fa0e;			/* +0xa0e width, one branch  */
	short           fa10;			/* +0xa10 width, the other   */
	/*
	 * One past the largest index the demapper will consider: every
	 * comparison in shellDemapper is against `count - 1`, and a count of
	 * 1 makes it return zero without reading anything else.
	 */
	short           count;			/* +0xa12 */
	short           fa14;			/* +0xa14 the repeated width */
	unsigned char pad_a16[0xa18 - 0xa16];
	/*
	 * decodeDepth's delay line: three COMPLEX taps, shifted a pair at a
	 * time.  hist[2..3] take hist[0..1] and hist[4..5] take hist[2..3];
	 * the new pair is written to hist[0..1].
	 */
	short           hist[6];		/* +0xa18 */
	unsigned char pad_a24_[0xa24 - 0xa24];
	/* Twelve coefficients as two rows of six, the second at +6. */
	const short *   coeff;			/* +0xa24 */
	unsigned char pad_a28[0xa38 - 0xa28];
	short           prev_k;			/* +0xa38 last quadrant   */
	unsigned char pad_a3a[0xa42 - 0xa3a];
	short           divisor;		/* +0xa42 zero means one  */
	unsigned char pad_a44[0xa46 - 0xa44];
	short           wrap;			/* +0xa46 sets the masks  */
	unsigned char pad_a48_[0xa48 - 0xa48];
	short           t1[0x80];		/* +0xa48 */
	short           t2[0x80];		/* +0xb48 */
	/*
	 * +0xc48.  Bounded above by putFrame's callback pointer at +0xe48,
	 * which makes it exactly 128 entries -- the same length as t1 and t2,
	 * which is the reassuring answer.  Before putFrame was read this had
	 * to be padded to the next known field and came out at 149.
	 */
	int             t3[0x80];		/* +0xc48 */
	v34_putbits_fn  put_bits;		/* +0xe48 */
	unsigned char pad_e4c[0xe50 - 0xe4c];
	/*
	 * The frame putFrame emits: one wide value, then four groups of
	 * (1 bit, a small width, and two of `fa14`).
	 */
	short           frame[18];		/* +0xe50 */
	unsigned char pad_e74[0xe9c - 0xe74];
	/*
	 * The eight sub-indices, read as two groups of four.  Alternate
	 * entries are taken signed and the others unsigned -- see the note
	 * in shellDemapper; that asymmetry is the object's, not a slip here.
	 */
	short           sub[V34_SHELL_SUBS];	/* +0xe9c */
	unsigned char pad_eac[0xecc - 0xeac];
	/*
	 * The trellis, 32 states of 16 branches.  Each entry is two bytes
	 * read separately and with different signedness: the LOW byte is an
	 * output code, taken unsigned, and the HIGH byte is the next branch,
	 * taken SIGNED.  decodeDepth walks it backwards 31 steps.
	 */
	unsigned short  trellis[32 * 16];	/* +0xecc */
	/*
	 * Per-state parameters, one six-short group each.  The second entry
	 * of every group is unread by decodeDepth.
	 */
	struct {
		short   seed;			/* +0x0 walk's first branch */
		short   unread_2;		/* +0x2 */
		short   a;			/* +0x4 */
		short   b;			/* +0x6 */
		short   c;			/* +0x8 */
		short   d;			/* +0xa */
	}               state[32];		/* +0x12cc */
	short           state_idx;		/* +0x144c */
};

/*
 * Combine the eight sub-indices into one shell index.
 *
 * Returns zero, without reading anything but `count`, when `count` is 1.
 */
int shellDemapper(void *shell);

/*
 * Emit one mapped frame through `put_bits`: a wide shell index followed by
 * four groups of (1 bit, a small field, and two `fa14`-wide fields).
 */
void putFrame(void *shell);

/*
 * Walk the trellis back 31 steps and decode one 8D frame.
 *
 * `quad` receives four shorts (two quadrant deltas and two masked grid
 * values) and `idx` two -- the grid values shifted down by `fa14`.
 */
void decodeDepth(void *shell, short *quad, short *idx);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34SHELL_H */

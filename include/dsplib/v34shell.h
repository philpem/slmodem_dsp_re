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
	unsigned char pad_a16[0xa48 - 0xa16];
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

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34SHELL_H */

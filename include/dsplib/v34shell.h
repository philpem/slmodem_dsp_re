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

struct v34_shell {
	unsigned char pad_000[0xa12];
	/*
	 * One past the largest index the demapper will consider: every
	 * comparison below is against `count - 1`, and a count of 1 makes
	 * the whole function return zero without reading anything else.
	 */
	short           count;			/* +0xa12 */
	unsigned char pad_a14[0xa48 - 0xa14];
	short           t1[0x80];		/* +0xa48 */
	short           t2[0x80];		/* +0xb48 */
	int             t3[(0xe9c - 0xc48) / 4];/* +0xc48 */
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

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34SHELL_H */

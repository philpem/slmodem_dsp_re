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
extern const short gInvertPat[16];
extern const short kkInvert[16];
extern const short kkNormal[16];
extern const short kTable[64];

/* The bit sink putFrame writes through: (context, value, bit count). */
typedef void (*v34_putbits_fn)(void *shell, int value, int nbits);

/*
 * The bit SOURCE getFrame refills through, at the same offset in the
 * transmit context.  Handed the OBJECT (not the context) and the current
 * bit position, and returns the new one.
 */
typedef int (*v34_getbits_fn)(void *obj, int pos);

/*
 * The signature `preinitdigital` installs at the same offset: the scrambler
 * pair.  See the union in `struct v34_shell`.
 */
typedef short (*v34_scramble_fn)(void *obj, short nbits);

/*
 * The transmit shell context sits this far past the receive one.  Every
 * offset getFrame uses lands on a field of this struct once the difference
 * is subtracted -- see finding 137.
 */
#define V34_SHELL_TX	0x1be0

/* lsbMask[n] == (1 << n) - 1, seventeen entries.  Emitted as data. */
extern const unsigned short lsbMask[17];

/*
 * The 64-entry table `preinitdigital` installs at `convolve`.  Global in the
 * object, with `Convolve32` and `Convolve64` beside it that nothing installs.
 * Sixteen distinct rows of four repeating with period 16, values only 0, 2,
 * 12 and 14 -- a two-bit quantity scaled by two, at a guess, and the
 * derivation is #47's.
 */
extern const short Convolve16[64];

struct v34_shell {
	unsigned char pad_000[0xa00];
	short           fa00;			/* +0xa00 */
	short           fa02;			/* +0xa02 sub-frame limit  */
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
	/* preinitdigital puts 0x18 here and nothing reconstructed reads it. */
	short           fa16;			/* +0xa16 */
	/*
	 * decodeDepth's delay line: three COMPLEX taps, shifted a pair at a
	 * time.  hist[2..3] take hist[0..1] and hist[4..5] take hist[2..3];
	 * the new pair is written to hist[0..1].
	 */
	short           hist[6];		/* +0xa18 */
	unsigned char pad_a24_[0xa24 - 0xa24];
	/* Twelve coefficients as two rows of six, the second at +6. */
	const short *   coeff;			/* +0xa24 */
	/*
	 * +0xa28.  `preinitdigital` installs `Convolve16` here, in both
	 * contexts; nothing reconstructed reads it yet.  The two siblings
	 * `Convolve32` and `Convolve64` sit beside it in .rodata and are
	 * installed by nothing at all so far.
	 */
	const short *   convolve;		/* +0xa28 */
	short           fa2c[6];		/* +0xa2c cleared, six of them,
						 * reaching exactly +0xa38 */
	short           prev_k;			/* +0xa38 last quadrant   */
	short           invert;			/* +0xa3a picks kkInvert  */
	short           fa3c;			/* +0xa3c sub-frame count */
	short           fa3e;			/* +0xa3e frame count     */
	short           fa40;			/* +0xa40 its limit       */
	short           divisor;		/* +0xa42 zero means one  */
	short           fa44;			/* +0xa44 cost shift      */
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
	/*
	 * +0xe48.  A sink in the receive context and a source in the
	 * transmit one -- same offset, two signatures, so a union rather
	 * than a cast.
	 */
	union {
		v34_putbits_fn	put_bits;	/* the receive context's sink */
		v34_getbits_fn	get_bits;	/* the transmit one's source  */
		/*
		 * AND WHAT `preinitdigital` ACTUALLY INSTALLS HERE is a
		 * scrambler in the transmit context and a descrambler in the
		 * receive one -- see finding 154.  That is the same pairing
		 * the two names above describe, which is the point: the
		 * shell's bit source IS the scrambler and its sink IS the
		 * descrambler.
		 *
		 * The widths do not match exactly -- `scrambleGPC` takes and
		 * returns a short where `v34_getbits_fn` uses int -- and
		 * `getFrame` is not reconstructed, so which typedef is the
		 * original's is not settled.  Both spellings are kept rather
		 * than one being chosen on a guess.
		 */
		v34_scramble_fn	scramble;
	};					/* +0xe48, anonymous so both
						 * spellings reach it directly
						 * and no caller has to change */
	short           latched;		/* +0xe4c */
	unsigned char pad_e4e[0xe50 - 0xe4e];
	/*
	 * The frame putFrame emits: one wide value, then four groups of
	 * (1 bit, a small width, and two of `fa14`).
	 */
	short           frame[18];		/* +0xe50 */
	unsigned char pad_e74[0xe80 - 0xe74];
	/*
	 * getFrame's bit window: a 32-bit buffer and the position within it.
	 * Refilled through `bits.get` whenever the position passes 15.  The
	 * receive context has no reader for either.
	 */
	int             bitbuf;			/* +0xe80 */
	short           bitpos;			/* +0xe84 */
	unsigned char pad_e86[0xe9c - 0xe86];
	/*
	 * The eight sub-indices, read as two groups of four.  Alternate
	 * entries are taken signed and the others unsigned -- see the note
	 * in shellDemapper; that asymmetry is the object's, not a slip here.
	 */
	short           sub[V34_SHELL_SUBS];	/* +0xe9c */
	/* Sixteen path costs, normalised against the best each sub-frame. */
	short           cost[16];		/* +0xeac */
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
 * The transmit-side counterpart of putFrame: unpack one frame from the bit
 * source into the TRANSMIT shell context at `obj + V34_SHELL_TX`.
 *
 * Takes the object, not the context -- the callback is handed the object too.
 */
void getFrame(void *obj);

/*
 * Walk the trellis back 31 steps and decode one 8D frame.
 *
 * `quad` receives four shorts (two quadrant deltas and two masked grid
 * values) and `idx` two -- the grid values shifted down by `fa14`.
 */
void decodeDepth(void *shell, short *quad, short *idx);

/*
 * One sub-frame of the 8D demapper.  `n` counts sub-frames; even ones run
 * decodeDepth, odd ones the trellis update, and the eighth emits a frame.
 * Returns zero on an even sub-frame and one on an odd one.
 */
int demapFrame(void *shell, void *a, void *b, short n);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34SHELL_H */

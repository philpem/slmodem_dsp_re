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
 * is subtracted -- see finding F137.
 */
#define V34_SHELL_TX	0x1be0

/* lsbMask[n] == (1 << n) - 1, seventeen entries.  Emitted as data. */
extern const unsigned short lsbMask[17];

/*
 * The three initialisers below are handed a pointer to the shell's OWN
 * fields, not to the object: every offset they use is 0xa00 less than the
 * matching one in `struct v34_shell`.  preinitdigital calls them with
 * `obj + 0xa00` and `obj + 0x25e0`, which are the receive context's fields
 * and the transmit context's -- the same 0x1be0 apart as everything else.
 *
 * The struct is left based on the object, because that is how every
 * function already written reaches it; this is the one place the two
 * spellings meet.
 */
#define V34_SHELL_FIELDS	0xa00

/*
 * V.34's three convolutional codes, 64 shorts each, selected by initV34's
 * `depth` argument and named for their state counts by the original.
 * modulatevector indexes them with exactly six bits, which is what pins the
 * length at 64 rather than at 32 ints of the same bytes.
 */
extern const short Convolve16[64];
extern const short Convolve32[64];
extern const short Convolve64[64];

/*
 * The ring-count tables, as a ragged array with its own index header:
 * `xyz[0..19]` are offsets, and the block `[xyz[n], xyz[n+1])` is the
 * cumulative shell count for a ring of n points.  See v34shell.c.
 */
extern const int xyz[945];

/* The ring size initV34 picks, by index; MMax when told to, MMin when not. */
extern const signed char MMaxTable[32];
extern const signed char MMinTable[32];

struct v34_shell {
	unsigned char pad_000[0xa00];
	short           span;			/* +0xa00 */
	short           subframe_limit;			/* +0xa02 sub-frame limit  */
	short           group_count;			/* +0xa04 */
	short           remainder;			/* +0xa06 */
	short           wide_accum;			/* +0xa08 an accumulator putFrame
						 *  advances and folds back */
	/*
	 * +0xa0a.  initV34 sets it to 15 minus the group size -- 8 for the
	 * two rates that use a group of 7, 7 for the five that use 8.
	 */
	short           short_a0a;			/* +0xa0a */
	unsigned char pad_a0c[0xa0e - 0xa0c];
	short           wide_bits;			/* +0xa0e width, one branch  */
	short           wide_bits_alt;			/* +0xa10 width, the other   */
	/*
	 * One past the largest index the demapper will consider: every
	 * comparison in shellDemapper is against `count - 1`, and a count of
	 * 1 makes it return zero without reading anything else.
	 */
	short           count;			/* +0xa12 */
	short           idx_width;			/* +0xa14 the repeated width */
	/*
	 * +0xa16.  The convolutional encoder's FEEDBACK MASK -- the generator
	 * polynomial, XORed back in when the bit shifted out is set.  24 out
	 * of preinitV34 for the 16-state code, then 32 and 64 out of initV34
	 * for the other two: single bits for the codes with one feedback tap
	 * and 0b11000 for the one with two.  It tracks `conv` because it IS
	 * `conv`'s recurrence.  modulatevector also compares it against 64 to
	 * pick a hand-unrolled form of the same step.  Retracted D47.
	 */
	short           feedback_mask;			/* +0xa16 */
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
	 * +0xa28.  V.34's convolutional code, as a table rather than as a
	 * function: preinitV34 installs the 16-state one and initV34 swaps in
	 * the 32- or 64-state code when asked for it.  Each is 64 shorts, and
	 * modulatevector indexes them with a six-bit code -- see the note on
	 * them in v34shell.c.
	 */
	const short *   conv;			/* +0xa28 */
	/* Six shorts preinitV34 clears and nothing read so far touches. */
	short           conv_sr[6];		/* +0xa2c */
	short           prev_k;			/* +0xa38 last quadrant   */
	short           invert;			/* +0xa3a picks kkInvert  */
	short           subframe_count;			/* +0xa3c sub-frame count */
	short           frame_count;			/* +0xa3e frame count     */
	short           frame_limit;			/* +0xa40 its limit       */
	short           divisor;		/* +0xa42 zero means one  */
	short           cost_shift;			/* +0xa44 cost shift      */
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
		 * receive one -- see finding F178.  That is the same pairing
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
	 * (1 bit, a small width, and two of `idx_width`).
	 *
	 * THE WIDE VALUE IS `frame[0..1]` AS ONE 32-BIT QUANTITY, and
	 * `getFrame` stores it that way -- `mov %eax,0x2a30(%esi)` at
	 * 0x57a68 for the value and `mov %edx,0x2a30(%esi)` at 0x57da5 for
	 * the explicit zero, with `%esi` the object and the transmit shell
	 * at +0x1be0, so 0x2a30 is this offset.  The SPLIT path stores the
	 * two halves separately instead -- `mov %dx,0x2a30(%esi)` at
	 * 0x57cac, 16 bits -- and `demapFrame` reaches `frame[2 + ...]` with
	 * `lea 0xe54(%ebx,%edx,8)` at 0x5965a, four shorts a group.
	 *
	 * So it is genuinely both, and the union says so.  `frame` keeps its
	 * name and its type; only the five `*(int *)&frame[0]` accesses
	 * become `frame_wide`.
	 */
	union {
		short           frame[18];	/* +0xe50 */
		int             frame_wide;	/* +0xe50, frame[0..1] as one */
	};
	/*
	 * +0xe74.  The scrambler's shift register, three words wide, shared
	 * by both contexts -- scrambleGP* drives it forwards and
	 * descrambleGP* backwards over the same three offsets.
	 */
	int             scr[3];			/* +0xe74 */
	/*
	 * +0xe80, AND THIS IS WHERE THE TWO CONTEXTS DIVERGE -- the one place
	 * they do.  The transmit context has a FOURTH scrambler word here,
	 * which is also getFrame's bit window: scrambleGP* leaves the
	 * scrambled 32 bits in it and getFrame reads them out, so the buffer
	 * and the register's last word are deliberately the same store.  Its
	 * bit position then sits at +0xe84.
	 *
	 * The receive context has no fourth word -- descrambleGP* folds the
	 * three down to sixteen bits and hands them off -- so it puts its bit
	 * position HERE instead, as a short, and +0xe84 is unused.
	 *
	 * preinitdigital confirms it from the other side: it clears four ints
	 * and sets a position at +0xe84 in the transmit context, and three
	 * ints and a short at +0xe80 in the receive one.  That looked like an
	 * asymmetry until the two callbacks were read; it is two field sets.
	 */
	union {
		int	bitbuf;			/* transmit: getFrame's window */
		short	rx_bitpos;		/* receive:  the bit position  */
	};
	short           bitpos;			/* +0xe84 transmit only */
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
	 *
	 * THE LAST FOUR WERE NEVER FOUR FIELDS.  `demapFrame` stores the
	 * caller's four bytes with ONE `movl` per pair -- `mov %edi,0x12d0
	 * (%ebx,%eax,4)` at 0x59673 on the even arm and `mov %edx,0x12d4
	 * (%ecx,%ebp,4)` at 0x59193 on the odd one -- and reads them back
	 * one at a time with `movswl (%edi)` at 0x5923d, off a pointer it
	 * steps by two.  So it is one four-short parameter group written as
	 * two 32-bit halves.
	 *
	 * The tree spelled the store `*(int *)&state[i].a`, which GCC 13
	 * warns about, and the walk `(&state[st].a)[i]` for i in 0..3, which
	 * warns NOWHERE and is the same defect: pointer arithmetic across
	 * four separately declared members.  The declaration retires both.
	 * `par` was `a`, `b`, `c`, `d`.
	 */
	struct {
		short   seed;			/* +0x0 walk's first branch */
		short   unread_2;		/* +0x2 */
		union {
			short   par[4];		/* +0x4 was a, b, c, d */
			int     pair[2];	/* +0x4 as demapFrame stores */
		};
	}               state[32];		/* +0x12cc */
	short           state_idx;		/* +0x144c */
};

/*
 * Point a context's bit callback somewhere.  Nothing in the object calls
 * it -- the stores it would make are inlined at all four sites -- so it
 * survives only as the out-of-line copy.  `fields`, not the object.
 */
void setScramble(void *fields, void *fn);

/*
 * Scale sixteen shorts -- eight complex points -- by `scale`/128, in place.
 * Also uncalled; see setScramble.
 */
void scaleVector(short *v, short scale);

/*
 * Clear one shell context to its power-on state: the three count tables,
 * the delay line, the 16-state code, and the scalars.  Leaves `count`
 * alone, so initV34 or initG248 must follow before the tables mean
 * anything.
 */
void preinitV34(void *fields);

/*
 * Rebuild the three count tables from `count` alone.
 *
 * t1 is the tent 1,2,..,count,..,2,1; t2 is t1 convolved with itself; and
 * t3 is copied out of `xyz`, which holds the eight-fold convolution
 * precomputed.  Uncalled -- initV34 carries the same three loops inline.
 */
void initG248(void *fields);

/*
 * Configure one shell context for a symbol rate and a trellis, and build
 * its count tables.  Always returns zero.
 *
 * `baud` is the V.34 symbol rate in units of 1/100 baud (2400, 2743, 2800,
 * 3000, 3200, 3429); `bitrate` the data rate; `use_max` picks MMaxTable
 * over MMinTable; `depth` selects the convolutional code; `coeff` and
 * `divisor` are stored as handed over.
 */
int initV34(void *fields, short baud, short bitrate, short use_max,
	    short depth, const short *coeff, short divisor);

/*
 * Reset BOTH shell contexts and the trellis decoder between them, and
 * install the pair of scrambler callbacks the station's role calls for.
 *
 * Takes the object.  The two contexts get the same treatment 0x1be0 apart,
 * with two asymmetries that are the original's -- see v34shell.c.
 */
void preinitdigital(void *obj);

/*
 * The four bit callbacks preinitdigital installs: a scrambler for the
 * transmit context's source and a descrambler for the receive context's
 * sink, in the caller's polynomial (GPC) and the answerer's (GPA).
 */
#include "dsplib/v34scram.h"

/*
 * modulatevector's two tables: `quarter` is 416 shorts of packed signed byte
 * pairs and `smIndex` sixteen, indexed by which band each coordinate is in.
 */
extern const short quarter[416];
extern const short smIndex[16];

/*
 * V.34's rate negotiation: unpack the negotiated INFO bits into the rate
 * config at +0xaa84, reconcile the two directions, and configure BOTH shell
 * contexts through initV34.  Takes the object.
 */
void initdigital(void *obj);

/*
 * Emit one modulated point, and refill all eight when the cursor wraps.
 *
 * The forward shell mapper: `shellDemapper` run backwards on the TRANSMIT
 * context, with `getFrame` as its bit source.  Ends in a tail call to
 * `txmit`, so this transmits rather than computes.  Takes the object.
 */
void modulatevector(void *obj);

/*
 * Combine the eight sub-indices into one shell index.
 *
 * Returns zero, without reading anything but `count`, when `count` is 1.
 */
int shellDemapper(void *shell);

/*
 * Emit one mapped frame through `put_bits`: a wide shell index followed by
 * four groups of (1 bit, a small field, and two `idx_width`-wide fields).
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
 * values) and `idx` two -- the grid values shifted down by `idx_width`.
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

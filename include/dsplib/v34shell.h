/*
 * v34shell.h -- ITU-T V.34: the shell mapper's index arithmetic.
 *
 * V.34 9.4 (shell mapping; 9.3 is the parser, per T-REC-V.34-199802) maps a
 * group of eight sub-indices onto one index into a shell of the
 * constellation, so that points of equal energy are equally likely -- the
 * shaping that buys V.34 its ~0.8 dB over a uniform mapping.  This is the
 * inverse direction, used by the receiver.
 *
 * The object is only partially mapped here, and deliberately so: `shellDemapper` is
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

/**
 * decodeDepth's two tables (see the notes in v34shell.c): kLookup is the
 * quadrant group as a 4x4 Latin square, grid a 529-entry map indexed by
 * `(x + 0x408) >> 2`.
 */
extern const short kLookup[16];
extern const short grid[529];
extern const short gInvertPat[16];
extern const short kkInvert[16];
extern const short kkNormal[16];
extern const short kTable[64];

/** The bit sink putFrame() writes through: (context, value, bit count). */
typedef void (*v34_putbits_fn)(void *shell, int value, int nbits);

/**
 * The bit source getFrame() refills through, at the same offset in the
 * transmit context. Handed the object (not the context) and the current
 * bit position, and returns the new one.
 */
typedef int (*v34_getbits_fn)(void *obj, int pos);

/**
 * The signature preinitdigital() installs at the same offset: the
 * scrambler pair. See the union in struct v34_shell.
 */
typedef short (*v34_scramble_fn)(void *obj, short nbits);

/**
 * How far past the receive shell context the transmit one sits. Every
 * offset getFrame() uses lands on a field of struct v34_shell once this
 * difference is subtracted (finding F137).
 */
#define V34_SHELL_TX	0x1be0

/** `lsbMask[n] == (1 << n) - 1`, seventeen entries. Emitted as literal data. */
extern const unsigned short lsbMask[17];

/**
 * Offset of a shell context's fields from the enclosing object. The three
 * initialisers below (setScramble(), preinitV34(), initG248()) take a
 * pointer to a context's own fields, not to the object -- every offset
 * they use is this much less than the matching one in struct v34_shell.
 * preinitdigital() calls them with `obj + V34_SHELL_FIELDS` and
 * `obj + V34_SHELL_FIELDS + V34_SHELL_TX`.
 */
#define V34_SHELL_FIELDS	0xa00

/**
 * V.34's three convolutional codes, 64 shorts each, selected by initV34()'s
 * `depth` argument and named for their state counts. modulatevector()
 * indexes them with exactly six bits, which is what pins the length at 64
 * rather than at 32 ints of the same bytes.
 */
extern const short Convolve16[64];
extern const short Convolve32[64];
extern const short Convolve64[64];

/**
 * The ring-count tables, as a ragged array with its own index header:
 * `xyz[0..19]` are offsets, and the block `[xyz[n], xyz[n+1])` is the
 * cumulative shell count for a ring of n points. See v34shell.c.
 */
extern const int xyz[945];

/** The ring size initV34() picks, by index; used when `use_max` is set. */
extern const signed char MMaxTable[32];
/** The ring size initV34() picks, by index; used when `use_max` is clear. */
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
	 * The convolutional encoder's feedback mask: the generator
	 * polynomial, XORed back in when the bit shifted out is set. Tracks
	 * `conv` because it is `conv`'s recurrence -- 24 for the 16-state
	 * code, 32 and 64 for the other two (single-tap codes get one bit,
	 * the two-tap code gets 0b11000). See deviation D47 (retracted).
	 */
	short           feedback_mask;			/* +0xa16 */
	/*
	 * decodeDepth's delay line: three complex taps, shifted a pair at a
	 * time. hist[2..3] take hist[0..1] and hist[4..5] take hist[2..3];
	 * the new pair is written to hist[0..1].
	 */
	short           hist[6];		/* +0xa18 */
	unsigned char pad_a24_[0xa24 - 0xa24];
	/* Twelve coefficients as two rows of six, the second at +6. */
	const short *   coeff;			/* +0xa24 */
	/*
	 * V.34's convolutional code, as a table rather than a function:
	 * preinitV34 installs the 16-state one, and initV34 swaps in the
	 * 32- or 64-state code when asked. Each is 64 shorts, indexed by
	 * modulatevector with a six-bit code.
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
	/* 128 entries, the same length as t1 and t2. */
	int             t3[0x80];		/* +0xc48 */
	/*
	 * A sink in the receive context and a source in the transmit one --
	 * same offset, two signatures, so a union rather than a cast. What
	 * `preinitdigital` actually installs here is a scrambler in the
	 * transmit context and a descrambler in the receive one (finding
	 * F178): the shell's bit source is the scrambler and its sink is
	 * the descrambler, which is the same pairing the two function-
	 * pointer types above describe.
	 *
	 * The widths do not match exactly -- `scrambleGPC` takes and
	 * returns a `short` where `v34_getbits_fn` uses `int` -- and
	 * `getFrame` is not yet reconstructed, so which typedef matches the
	 * original is not settled. Both spellings are kept rather than
	 * choosing one on a guess.
	 */
	union {
		v34_putbits_fn	put_bits;	/* the receive context's sink */
		v34_getbits_fn	get_bits;	/* the transmit one's source  */
		v34_scramble_fn	scramble;
	};					/* +0xe48, anonymous so both
						 * spellings reach it directly
						 * and no caller has to change */
	short           latched;		/* +0xe4c */
	/* (+0xe4e..+0xe50 is a pure alignment gap, not an unmodelled field
	 * -- see finding F10147.) */
	/*
	 * The frame putFrame() emits: one wide value, then four groups of
	 * (1 bit, a small width, and two of `idx_width`). `frame[0..1]` is
	 * genuinely both a 32-bit quantity (the wide value, and the
	 * explicit zero getFrame() also stores there) and two separate
	 * shorts (the split path that runs when the value needs more than
	 * 16 bits, and the four-shorts-at-a-time reads in demapFrame()) --
	 * hence the union. `frame` keeps its name and type; only the wide
	 * accesses go through `frame_wide` (finding F5304).
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
	 * +0xe80 is the one place the two contexts diverge. The transmit
	 * context has a fourth scrambler word here, which doubles as
	 * getFrame's bit window: scrambleGP* leaves the scrambled 32 bits in
	 * it and getFrame reads them back out, so the buffer and the
	 * register's last word are deliberately the same store; its bit
	 * position then sits at +0xe84. The receive context has no fourth
	 * word -- descrambleGP* folds the three down to sixteen bits and
	 * hands them off -- so it keeps its bit position here instead, as a
	 * short, leaving +0xe84 unused. preinitdigital's own clears confirm
	 * the split: four ints and a position at +0xe84 on the transmit
	 * side, three ints and a short at +0xe80 on the receive side.
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
	 * The trellis, 32 states of 16 branches. Each entry is two bytes
	 * read separately and with different signedness: the low byte is an
	 * output code, taken unsigned, and the high byte is the next branch,
	 * taken signed. decodeDepth walks it backwards 31 steps.
	 */
	unsigned short  trellis[32 * 16];	/* +0xecc */
	/*
	 * Per-state parameters, one six-short group each. The second entry
	 * of every group is unread by decodeDepth. The last four shorts of
	 * each group (`par`, formerly separate fields `a`..`d`) are written
	 * by demapFrame as two 32-bit halves rather than four shorts, hence
	 * the union with `pair` -- see finding F5304.
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
 * PAD-REGION AUDIT (finding F10147).  `pad_e4e[2]` was removed above as a
 * pure compiler-alignment artefact: `latched` ends on a 2-mod-4 byte
 * boundary and the union that follows needs 4-byte alignment for its `int`
 * member, so GCC's own default alignment inserts exactly this gap once the
 * pad member is gone -- no `#pragma pack` applies to this struct. Proven the
 * same two ways as every other removal this workstream made: the assertion
 * below holds the union at its original offset, and a disassembly search of
 * the whole object under both addressing conventions this struct's callers
 * use (the absolute struct offset, and the `V34_SHELL_FIELDS`-relative one
 * `preinitV34`/`initV34`/etc. receive) found no instruction anywhere that
 * reads or writes those two bytes.
 *
 * `pad_000` (the struct's opening 0xa00 bytes -- no preceding field to
 * derive alignment from), `pad_a0c` (`short_a0a` and `wide_bits` are both
 * already 2-aligned at +0xa0c; a 2-byte gap there is not what alignment
 * would add) and `pad_e86` (22 bytes where `bitpos`/`sub` need none) were
 * checked the same way and LEFT ALONE: their gaps do not match what natural
 * alignment would insert, so deleting them would not reproduce the object's
 * layout.  Not a claim their bytes are unread -- only that they stay
 * explicit per this workstream's decision rule.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
typedef char v34shell_off_frame[
	((int)__builtin_offsetof(struct v34_shell, frame) == 0xe50) ? 1 : -1];
typedef char v34shell_size[(sizeof(struct v34_shell) == 0x1450) ? 1 : -1];
#endif

/**
 * @brief Point a shell context's bit callback at a function.
 *
 * Nothing in the object calls this: the stores it would make are inlined
 * at all four call sites, so it survives in the object only as an
 * out-of-line copy nothing reaches.
 *
 * @param fields  A shell context's fields (see #V34_SHELL_FIELDS), not the object.
 * @param fn      The callback to install.
 */
void setScramble(void *fields, void *fn);

/**
 * @brief Scale eight complex points (sixteen shorts) by `scale`/128, in place.
 *
 * Uncalled in the object, like setScramble().
 *
 * @param v      Sixteen shorts (eight complex points) to scale.
 * @param scale  Scale factor, as a numerator over 128.
 */
void scaleVector(short *v, short scale);

/**
 * @brief Clear one shell context to its power-on state.
 *
 * Clears the three count tables, the delay line, the 16-state code, and
 * the scalars. Leaves `count` alone, so initV34() or initG248() must run
 * afterward before the tables mean anything.
 *
 * @param fields  A shell context's fields (see #V34_SHELL_FIELDS).
 */
void preinitV34(void *fields);

/**
 * @brief Rebuild a shell context's three count tables from `count` alone.
 *
 * `t1` is the tent `1, 2, .., count, .., 2, 1`; `t2` is `t1` convolved with
 * itself; `t3` is copied out of the precomputed eight-fold convolution in
 * `xyz`. Uncalled in the object -- initV34() carries the same three loops
 * inline instead of calling this.
 *
 * @param fields  A shell context's fields (see #V34_SHELL_FIELDS).
 */
void initG248(void *fields);

/**
 * @brief Configure one shell context for a symbol rate and trellis, and
 * build its count tables.
 *
 * @param fields   A shell context's fields (see #V34_SHELL_FIELDS).
 * @param baud     V.34 symbol rate, in units of 1/100 baud (2400, 2743,
 *                 2800, 3000, 3200 or 3429).
 * @param bitrate  Data rate.
 * @param use_max  Non-zero to use MMaxTable instead of MMinTable.
 * @param depth    Selects the convolutional code.
 * @param coeff    Stored as given.
 * @param divisor  Stored as given.
 * @return Always 0.
 */
int initV34(void *fields, short baud, short bitrate, short use_max,
	    short depth, const short *coeff, short divisor);

/**
 * @brief Reset both shell contexts and the trellis decoder, and install
 * the scrambler callback pair for the station's role.
 *
 * The two contexts get the same treatment #V34_SHELL_TX apart, with two
 * asymmetries that are the object's own (see v34shell.c).
 *
 * @param obj  The V.34 modem object.
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

/**
 * @brief V.34's rate negotiation.
 *
 * Unpacks the negotiated INFO bits into the rate configuration record, at
 * +0xaa84 in the object, reconciles the two directions, and configures
 * both shell contexts through initV34().
 *
 * @param obj  The V.34 modem object.
 */
void initdigital(void *obj);

/**
 * @brief Emit one modulated point.
 *
 * The forward shell mapper: shellDemapper() run backwards on the transmit
 * context, with getFrame() as its bit source. Refills all eight
 * sub-indices when the cursor wraps, and ends in a tail call to `txmit`,
 * so this transmits rather than merely computes.
 *
 * @param obj  The V.34 modem object.
 */
void modulatevector(void *obj);

/**
 * @brief Combine the eight sub-indices into one shell index.
 *
 * @param shell  A shell context.
 * @return The shell index; 0, without reading anything but `count`, if
 *         `count` is 1.
 */
int shellDemapper(void *shell);

/**
 * @brief Emit one mapped frame through a context's `put_bits` callback.
 *
 * A wide shell index followed by four groups of (1 bit, a small field,
 * and two `idx_width`-wide fields).
 *
 * @param shell  A shell context.
 */
void putFrame(void *shell);

/**
 * @brief The transmit-side counterpart of putFrame().
 *
 * Unpacks one frame from the bit source into the transmit shell context
 * at `obj + V34_SHELL_TX`.
 *
 * @param obj  The V.34 modem object, not the context -- the callback is
 *             handed the object too.
 */
void getFrame(void *obj);

/**
 * @brief Walk the trellis back 31 steps and decode one 8D frame.
 *
 * @param shell  A shell context.
 * @param quad   Output: four shorts -- two quadrant deltas and two masked
 *               grid values.
 * @param idx    Output: two shorts -- the grid values shifted down by
 *               `idx_width`.
 */
void decodeDepth(void *shell, short *quad, short *idx);

/**
 * @brief One sub-frame of the 8D demapper.
 *
 * Even sub-frames run decodeDepth(); odd ones run the trellis update; the
 * eighth also emits a frame.
 *
 * @param shell  A shell context.
 * @param a      Passed through to the sub-frame's processing.
 * @param b      Passed through to the sub-frame's processing.
 * @param n      Sub-frame counter.
 * @return 0 on an even sub-frame, 1 on an odd one.
 */
int demapFrame(void *shell, void *a, void *b, short n);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34SHELL_H */

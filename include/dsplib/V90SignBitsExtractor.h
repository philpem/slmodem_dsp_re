/**
 * @file V90SignBitsExtractor.h
 * @brief The V.90 sign-bit extractor: decodes one V.90 frame's sign-bit
 *        stream into data bits, embedded inside `V90Demapper`.
 *
 * One process() call consumes one V.90 frame of `width` sign bits. It picks
 * an inversion pattern (an ::ACTIONS value) from a small two-state machine
 * and the frame's first bit, applies that pattern into its own buffer,
 * differentially decodes the buffer (per-position, then serially over the
 * odd positions only), and hands the caller positions 1..width-1 -- so the
 * first bit is consumed as the state signal and `width - 1` bits come out.
 * `V90Demapper::process` relies on that off-by-one exactly, advancing its
 * output pointer by `width - 1` per group.
 *
 * Reconstructed from dsplibs.o; `.symtab`'s `V90SignBitsExtractor.cpp` FILE
 * entry says the original had its own translation unit, and this pair is
 * named after it. `reset()` is declared but not yet reconstructed.
 *
 * The object is 0x28 (40) bytes, derived from containment rather than a
 * displacement scan (finding F1107's rule, applied the other way up): there
 * is no `sysdep_malloc` to read, since every instance is embedded, but the
 * gap between this member and the next one in `V90Demapper` (+0x668 to
 * +0x690) is exactly 0x28, and the last member here (`decoder`, a 12-byte
 * `ParallelDifferentialDecoder<unsigned char>`) ending at +0x1c + 0xc = 0x28
 * agrees with no trailing padding. Finding F1173.
 *
 * Not polymorphic: `nm` gives `D1`/`D2` and no `D0`, and GCC emits a deleting
 * destructor only for a virtual class, so offset 0 is a real member and
 * there is no vptr (finding F228). The destructor's twenty-two bytes are
 * entirely the compiler's -- the implicit destruction of `decoder` at +0x1c,
 * with an empty body as the whole source -- and it exists only so that
 * `~V90Demapper` (which calls it at `+0x668`) is complete.
 */

#ifndef DSPLIB_V90SIGNBITSEXTRACTOR_H
#define DSPLIB_V90SIGNBITSEXTRACTOR_H

#include "dsplib/DiffCoder.h"

/*
 * The capacity the constructor fixes (`mov $0x6,%edx` at 0x31901) and, with
 * it, the most `width` can ever be -- so it is also the size of the frame
 * buffer at +0x08.  See both member comments.
 */
#define V90SBE_DECODER_SIZE	6u

class V90SignBitsExtractor {
public:
	/**
	 * @brief Construct the extractor. Value-initialises `oddDecoder`,
	 * fixes `decoder`'s capacity at #V90SBE_DECODER_SIZE, and sets
	 * `state` to 0. Every other field is left unseeded until reset()
	 * runs.
	 */
	V90SignBitsExtractor();

	/**
	 * @brief Destructor. Its only effect is destroying `decoder`.
	 */
	~V90SignBitsExtractor();

	/**
	 * @brief One of the four sign-inversion patterns applyFrameAction()
	 * applies, chosen per frame by process()'s two-state machine.
	 *
	 * `ACTIONS` is the original author's own enum name (from the
	 * mangling); the four enumerator spellings below are this tree's
	 * own, since the mangling carries no enumerator names. The four
	 * values and what each does are read off the object's own switch:
	 *
	 *     0   out[i] = in[i]                      pass every position
	 *     1   out[i] = (in[i] == 0)                invert every position
	 *     2   out[i] = (i & 1) ? in[i] : !in[i]    invert the even ones
	 *     3   out[i] = (i & 1) ? !in[i] : in[i]    invert the odd ones
	 *
	 * The switch is over a signed value, which is why this is an `enum`
	 * and not an `unsigned` parameter; a negative selector falls through
	 * to the default arm, which still runs the loop writing nothing
	 * (the object has the switch inside the loop, not outside it).
	 */
	enum ACTIONS {
		V90SBE_PASS_ALL		= 0,
		V90SBE_INVERT_ALL	= 1,
		V90SBE_INVERT_EVEN	= 2,
		V90SBE_INVERT_ODD	= 3
	};

	/**
	 * @brief Arm the extractor for a new sign-bit spacing.
	 *
	 * Derives `width` as `V90SBE_DECODER_SIZE / spacing` (0 if @p spacing
	 * is 0, guarded rather than dividing by zero), clears the odd
	 * decoder's history, re-arms the parallel decoder at the new width,
	 * and sets `state` to @p state.
	 * @param spacing  The sign-bit spacing; width = 6 / spacing.
	 * @param state    The two-state machine's initial state (see
	 *                 #state; only 0 and 1 are handled, docs/deviations.md
	 *                 D386).
	 */
	void reset(unsigned int spacing, unsigned int state);

	/**
	 * @brief Apply one inversion pattern across `width` positions.
	 * @param action  Which of the four patterns to apply.
	 * @param in      Input buffer, `width` bytes.
	 * @param out     Output buffer, `width` bytes; may alias @p in.
	 */
	void applyFrameAction(ACTIONS action, unsigned char *in, unsigned char *out);

	/**
	 * @brief Decode one V.90 frame of sign bits.
	 *
	 * Uses `in[0]` and the stored two-state machine to choose an
	 * ::ACTIONS, applies it into the internal frame buffer, runs the
	 * parallel differential decoder over every position and then the
	 * serial one over the odd positions only, and copies positions
	 * 1..width-1 to @p out.
	 * @param in   Input buffer, `width` sign bits (one V.90 frame).
	 * @param out  Output buffer, `width - 1` decoded bits.
	 */
	void process(unsigned char *in, unsigned char *out);

	/*
	 * Data members are public for the reason V90Jd.h gives: the original's
	 * access specifiers are not recoverable from the mangling, and a
	 * single access section is what lets the .cpp assert every offset
	 * below with __builtin_offsetof.
	 */

	/**
	 * @brief The sign-bit spacing passed to reset(); `width` is 6 / this.
	 * Stored unconditionally before anything branches on it, and read by
	 * nothing else in the object. An earlier version of this comment
	 * misread stack displacements in `process` as offsets into this
	 * field; corrected by finding F3531.
	 */
	unsigned int spacing;

	/**
	 * @brief The active frame width (`6 / spacing`), used as the
	 * unsigned loop bound in every method here and as the parallel
	 * decoder's active size.
	 */
	unsigned int width;

	/**
	 * @brief The frame under construction, `width` (at most
	 * #V90SBE_DECODER_SIZE) bytes of it. applyFrameAction()'s pattern is
	 * applied into this buffer in place, the parallel decoder then runs
	 * over it in place, and process() finally copies positions 1..width-1
	 * out to the caller. The trailing two bytes when width < 6 are
	 * ordinary alignment ahead of `state`, not a second field.
	 */
	unsigned char bits[V90SBE_DECODER_SIZE];
	/*
	 * +0x0e was `pad_0e[2]` -- REMOVED (finding F10151).  The old comment
	 * already called it correctly: "the two trailing bytes are the
	 * alignment of the word at +0x10" -- exactly the compiler-inserted
	 * gap a 6-byte array ending at +0x0e leaves ahead of the 4-byte-
	 * aligned `unsigned int state` below, already proved by
	 * `SBE_OFF(state, 0x10, state)` in the .cpp.  `dis.py` over every
	 * `V90SignBitsExtractor` method finds no access to offset 0x0e.
	 */

	/**
	 * @brief The two-state machine process() runs, seeded by reset()'s
	 * second argument. Chosen jointly with the frame's first bit:
	 *
	 *     state 0:  action = state = (in[0] != 0)
	 *     state 1:  in[0] == 0 -> action INVERT_ODD,  state 1
	 *               in[0] != 0 -> action INVERT_EVEN, state 0
	 *
	 * A third value is reachable (reset() stores its argument
	 * unfiltered) and the object does not handle it: process() falls
	 * through both tests with its action left undefined. docs/deviations.md
	 * D386.
	 */
	unsigned int state;

	/*
	 * +0x14  Not modelled -- nothing in the five reconstructed members
	 * touches it. An earlier version of this comment claimed
	 * `applyFrameAction` read it; that was `0x14(%esp)`, the action
	 * argument on the stack, misread as an object offset. Finding F3531.
	 */
	unsigned char pad_14[4];

	/**
	 * @brief The serial differential decoder run over the odd positions
	 * only, after the parallel decoder has run over every position. Its
	 * type is forced rather than inferred: process() passes `this + 0x18`
	 * as the `this` of `SerialDifferentialDecoder<unsigned char>::process`.
	 */
	SerialDifferentialDecoder<unsigned char> oddDecoder;
	/*
	 * +0x19 was `pad_19[3]` -- REMOVED (finding F10151): a 1-byte
	 * `oddDecoder` ending at +0x19 leaves exactly 3 bytes of compiler
	 * alignment ahead of `decoder`, a `ParallelDifferentialDecoder
	 * <unsigned char>` whose first member is a pointer and needs 4-byte
	 * alignment (DiffCoder.h).  `SBE_OFF(decoder, 0x1c, decoder)` in the
	 * .cpp already proves the target offset, and `dis.py` over every
	 * `V90SignBitsExtractor` method finds no access to 0x19/0x1a/0x1b.
	 */

	/**
	 * @brief The per-position parallel differential decoder, run over
	 * every position of the frame buffer. Its capacity is fixed at
	 * #V90SBE_DECODER_SIZE by the constructor. Last member; the object
	 * ends where it does.
	 */
	ParallelDifferentialDecoder<unsigned char> decoder;
};

#endif /* DSPLIB_V90SIGNBITSEXTRACTOR_H */

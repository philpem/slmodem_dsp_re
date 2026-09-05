/**
 * @file V90SpectralShaper.h
 * @brief V.90 transmit sign-bit spectral shaper: `V90SpectralShaper`.
 *
 * This is the transmit half of V.90's sign-bit spectral shaping, and
 * `V90SignBitsExtractor` is the receive half of the same trellis -- read
 * that header beside this one, since the two describe one mechanism from
 * its two ends.
 *
 * One process() call handles one V.90 frame of `blockLength` samples. The
 * frame's sign bits are differentially encoded (serially over the odd
 * positions, then per-position against the previous frame), applied as
 * signs to the samples, and pushed into a delay line that is `shaperId`
 * frames deep. Position 0 of every frame is not a payload bit: process()
 * forces it to zero and the trellis owns it instead -- the same off-by-one
 * `V90SignBitsExtractor::process` spends on the receive side.
 *
 * advanceTrellis() then searches, exhaustively, all 2^(shaperId+1) ways of
 * flipping the polarity of the frames still in the line. Each candidate is
 * one entry of `actionLookupTable`, scored by running the embedded shaping
 * filter over a scratch copy of the whole line, and the lowest score wins.
 * Only the winner's leading action -- the one belonging to the frame about
 * to leave the delay line -- is committed, and its low bit becomes `state`,
 * so the next call's search starts from the branch this one took. That is
 * what makes it a trellis rather than a per-frame decision.
 *
 * <b>`actionLookupTable` is a decimal encoding, and it is entirely
 * regular.</b> 128 signed ints, `.data 0x8e0`, indexed `[state + 2 *
 * shaperId][candidate]` -- eight rows of sixteen, of which row `2 *
 * shaperId + state` uses its first 2^(shaperId+1) entries and the rest are
 * zero. Each entry is a decimal number of `shaperId + 1` digits, each digit
 * in 1..4, one digit per frame in the line, most significant digit for the
 * oldest frame:
 *
 *      row 0 (shaperId 0, state 0)      1 2
 *      row 1 (shaperId 0, state 1)      3 4
 *      row 2 (shaperId 1, state 0)      11 12 23 24
 *      ...
 *      row 7 (shaperId 3, state 1)      3111 3112 ... 4443 4444
 *
 * Reading `candidate` as a bit vector b[shaperId..0], every digit is
 *
 *      digit = 1 + 2 * previous_bit + this_bit
 *
 * with `state` standing in as the previous bit for the leading digit. So a
 * digit carries a state transition, which is why `digit - 1` is an
 * `ACTIONS` and why `state` ends up as `action & 1`. The table is
 * reproduced literally in V90SpectralShaper.cpp; the rule above is stated
 * so a reader can check it, not because the object computes it.
 *
 * applyAction() walks those digits with a signed `% 10` / `/= 10`, which is
 * what makes the table `int` and not `unsigned` (0x32a4d). `pow10Table`
 * exists only so advanceTrellis() can divide the winner by 10^shaperId and
 * recover the leading digit in one step.
 *
 * The object is 108 bytes, and the bound comes from outside it as well as
 * from inside: `V90Mapper` embeds one at +0x68c -- its constructor does
 * `lea 0x68c(%ebx),%eax` and calls `V90SpectralShaperC1` on it -- and the
 * next thing `V90Mapper::reset` and `V90Mapper::process` touch above that
 * is +0x6f8, which is 0x68c + 0x6c exactly.
 *
 * <b>The two heap buffers are `short *`, and that is a callee's word, not
 * an inference.</b> `process` hands +0x28 straight to
 * `V90SpectralShapingFilter::progress(const short *)` and advanceTrellis()
 * hands +0x2c straight to `getMetric(const short *, unsigned)`, with no
 * conversion between; `applyAction`'s and `applyFrameAction`'s own
 * `short *` parameters are the same evidence a third and fourth time
 * (finding F5850).
 */

#ifndef DSPLIB_V90SPECTRALSHAPER_H
#define DSPLIB_V90SPECTRALSHAPER_H

#include "dsplib/DiffCoder.h"
#include "dsplib/V90SpectralShapingFilter.h"

/*
 * The capacity `ParallelDifferentialEncoder<unsigned char>` is constructed
 * with, and with it the most `blockLength` can ever be: six samples to a V.90
 * frame, so `6 / shaperSR` cannot exceed six.  It is therefore also the size
 * of the three sign-bit arrays at +0x0c, +0x12 and +0x18, none of which the
 * object bounds from the inside.
 */
#define V90SS_FRAME_BITS	6u

class V90SpectralShaper {
public:
	/**
	 * @brief Construct a shaper with an empty, unconfigured delay line.
	 *
	 * Allocates the 24-entry `delayLine` and `trialLine` buffers, sets
	 * `windowLength` to their allocated size and `writeIndex`/`state` to
	 * zero. reset() must be called before process() to size the line to
	 * the connection's actual `shaperId`/`blockLength`.
	 */
	V90SpectralShaper();

	/** @brief Free `delayLine` and `trialLine`. */
	~V90SpectralShaper();

	/**
	 * @brief One of the four sample-polarity patterns applyFrameAction()
	 * can apply to a frame.
	 *
	 * `ACTIONS` is the original author's own enum name (from the
	 * mangling of applyFrameAction()); the four enumerator spellings
	 * below are descriptive, since a mangled name does not preserve
	 * enumerator names. The values and their effect on sample `i`
	 * (counted from the start of the frame) are read off the object's
	 * switch:
	 *
	 *     0   dst[i] =  src[i]                       keep the frame
	 *     1   dst[i] = -src[i]                       negate the frame
	 *     2   dst[i] = (i & 1) ?  src[i] : -src[i]   negate the even ones
	 *     3   dst[i] = (i & 1) ? -src[i] :  src[i]   negate the odd ones
	 *
	 * These are the same four arms, in the same four positions, as
	 * `V90SignBitsExtractor::ACTIONS`, which inverts bits where this
	 * negates samples -- the receive side of this trellis.
	 */
	enum ACTIONS {
		V90SS_KEEP_ALL		= 0,
		V90SS_NEGATE_ALL	= 1,
		V90SS_NEGATE_EVEN	= 2,
		V90SS_NEGATE_ODD	= 3
	};

	/**
	 * @brief Set up the shaper for a connection.
	 *
	 * Derives `blockLength` as `6 / shaperSR` (0 if @p shaperSR is 0),
	 * resets the differential encoders, configures the embedded shaping
	 * filter with the four coefficients, sizes the active window to
	 * `(shaperId + 1) * blockLength`, clears the delay line, and arms
	 * `primeFrames` so the trellis search does not start until `shaperId`
	 * frames have been primed.
	 *
	 * @param shaperId  Depth of the trellis lookahead, in frames.
	 * @param shaperSR  Divides 6 to give the frame width in samples.
	 * @param a1,a2,b1,b2  The embedded shaping filter's coefficients.
	 */
	void reset(unsigned int shaperId, unsigned int shaperSR,
		   float a1, float a2, float b1, float b2);

	/**
	 * @brief Re-run the embedded shaping filter's own setup only.
	 *
	 * Touches no shaper state and does not recompute `blockLength`.
	 * @param a1,a2,b1,b2  The filter's coefficients.
	 */
	void resetSSFilter(float a1, float a2, float b1, float b2);

	/**
	 * @brief Apply one polarity pattern to one frame of the delay line.
	 * @param action  Which of the four patterns to apply.
	 * @param dst     Destination buffer.
	 * @param start   Index in `delayLine`/`dst` of the frame's first sample.
	 */
	void applyFrameAction(ACTIONS action, short *dst, int start);

	/**
	 * @brief Apply one whole `actionLookupTable` candidate to the delay
	 * line.
	 *
	 * @p action's decimal digits are consumed least-significant first;
	 * digit `m` selects the pattern for the frame at
	 * `(shaperId - m) * blockLength`, so the leading digit lands on the
	 * oldest frame.
	 *
	 * @param action  One `actionLookupTable` entry.
	 * @param dst     Destination buffer, one whole window.
	 */
	void applyAction(int action, short *dst);

	/**
	 * @brief Search all `2^(shaperId+1)` candidate polarity patterns for
	 * the frames in the delay line, and commit the best-scoring one's
	 * leading action.
	 *
	 * Scores each candidate by copying the line into `trialLine`,
	 * applying the candidate, and running it through the embedded shaping
	 * filter's getMetric(); the lowest-scoring candidate's leading action
	 * is applied to `delayLine` itself (the frame about to leave), and
	 * its low bit becomes the next call's `state`. See the file comment
	 * for what makes this a trellis rather than a per-frame decision.
	 */
	void advanceTrellis();

	/**
	 * @brief Shape one V.90 frame: encode its sign bits, push it into the
	 * delay line, and emit the frame leaving the far end.
	 *
	 * Position 0 of the frame is not a payload bit -- it is forced to
	 * zero and belongs to the trellis -- so @p bits supplies only
	 * `blockLength - 1` bits, landing at position 1 upward. The odd
	 * positions are serially differentially encoded and the even ones
	 * copied through; the resulting bits are per-position differentially
	 * encoded into sign bits, which negate `in`'s samples on a zero bit
	 * before they are pushed into the line. Runs advanceTrellis() once
	 * `primeFrames` reaches zero.
	 *
	 * @param in    One frame of `blockLength` samples to push into the line.
	 * @param bits  That frame's `blockLength - 1` payload sign bits.
	 * @param out   Receives the frame leaving the delay line, `shaperId`
	 *              frames after it entered.
	 */
	void process(short *in, unsigned char *bits, short *out);

	/**
	 * @brief The trellis's precomputed action table; see the file comment
	 * for the decimal encoding.
	 *
	 * A global (not `const`, not file-local) data symbol, `.data 0x8e0`,
	 * 512 bytes.
	 */
	static int actionLookupTable[8][16];

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	unsigned int	 shaperId;	/* +0x00 V90MappingParams::shaperId */
	unsigned int	 shaperSR;	/* +0x04 V90MappingParams::shaperSR */
	unsigned int	 blockLength;	/* +0x08 6 / shaperSR, or 0         */

	/*
	 * +0x0c  The frame's sign bits as `process` receives them, shifted up
	 * by one: position 0 is forced to zero (`movb $0x0,0xc(%edi)` at
	 * 0x32fd4) because the trellis owns it, and `bits[0..blockLength-2]`
	 * land in 1..blockLength-1.
	 */
	unsigned char	 frameBits[V90SS_FRAME_BITS];

	/*
	 * +0x12  After the serial differential encoder, which runs on the odd
	 * positions only; the even positions are copied through. This is
	 * `V90SignBitsExtractor`'s `oddDecoder` step seen from the transmit
	 * side.
	 */
	unsigned char	 codedBits[V90SS_FRAME_BITS];

	/*
	 * +0x18  After the per-position differential encoder, and what decides
	 * each sample's sign: `process` negates the sample where the bit is
	 * zero, so 1 is positive.
	 */
	unsigned char	 signBits[V90SS_FRAME_BITS];
	/*
	 * +0x1e was `pad_1e[2]` -- REMOVED (finding F10151).  Already
	 * correctly described as alignment of the word below; proved
	 * mechanically by the existing `V90SS_OFF(signBits, 0x18, ...)`/
	 * `V90SS_OFF(state, 0x20, ...)` and by `dis.py` over every
	 * `V90SpectralShaper` method finding no access to 0x1e/0x1f.
	 */

	/*
	 * +0x20  The trellis state, one bit wide although it is a full word.
	 * It selects which half of `actionLookupTable` the search runs over,
	 * and `advanceTrellis` leaves it holding the committed action's low
	 * bit. The same two-state machine is `V90SignBitsExtractor::state`.
	 */
	unsigned int	 state;

	/*
	 * +0x24  Frames still to be primed. `reset` seeds it with `shaperId`
	 * and `process` counts it down one per call, running the trellis only
	 * once it has reached zero -- exactly the number of calls it takes to
	 * fill the `shaperId` frames of lookahead the search needs. It is not
	 * a second copy of `shaperId`: nothing ever reads it as one (usage
	 * inference; finding F5851).
	 */
	unsigned int	 primeFrames;

	/*
	 * +0x28  The delay line, `windowLength` samples of it live, owned.
	 * +0x2c  The trellis's scratch copy of the line, owned, and never
	 *        read before it is written.
	 * Both 24 entries; both `short *` for the reason in the file comment.
	 */
	short		*delayLine;
	short		*trialLine;

	/*
	 * +0x30  Where `process` writes the incoming frame, invariant at
	 * `shaperId * blockLength`: the write loop advances it by
	 * `blockLength` and the tail subtracts the same amount back off after
	 * shifting the line down. `reset` seeds it, so the first `shaperId`
	 * frames of the line start out zero.
	 */
	unsigned int	 writeIndex;

	/*
	 * +0x34  The live length of the delay line, `(shaperId + 1) *
	 * blockLength` -- the frame in flight plus `shaperId` of lookahead.
	 * It bounds the copy into `trial` and the shift-down in `process`.
	 * The constructor's 24 is the allocated count and is overwritten by
	 * the first `reset`.
	 */
	unsigned int	 windowLength;

	/*
	 * +0x38  A `SerialDifferentialEncoder<unsigned char>`; the type is
	 * forced, since `process` hands its address to
	 * `SerialDifferentialEncoder<unsigned char>::process` directly. One
	 * byte, so the three that follow are padding.
	 */
	SerialDifferentialEncoder<unsigned char> oddEncoder;
	/*
	 * +0x39 was `pad_39[3]` -- REMOVED (finding F10151): a 1-byte
	 * `oddEncoder` ending at +0x39 leaves exactly 3 bytes of alignment
	 * ahead of `pde`, a `ParallelDifferentialEncoder<unsigned char>`
	 * needing 4-byte alignment.  Both ends already asserted
	 * (`V90SS_OFF(oddEncoder, 0x38, ...)`, `V90SS_OFF(pde, 0x3c, ...)`),
	 * and `dis.py` finds no access to 0x39/0x3a/0x3b.
	 */

	/* +0x3c, built with 6 -- one memory per position in the frame. */
	ParallelDifferentialEncoder<unsigned char> pde;

	V90SpectralShapingFilter ssf;			/* +0x48              */
};

#endif /* DSPLIB_V90SPECTRALSHAPER_H */

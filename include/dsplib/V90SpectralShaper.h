/*
 * V90SpectralShaper.h -- the V.90 spectral shaper's state.
 *
 * Reconstructed from dsplibs.o.  All eight members, 2,562 bytes of text, a
 * 512-byte `actionLookupTable` and the 20-byte `pow10Table` beside it.
 *
 * NOT POLYMORPHIC: `nm` lists `D1` and `D2` and no `D0`, and the constructor
 * stores no vptr, so offset 0 is a real member.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE CLASS DOES, now that `process` and `advanceTrellis` are read
 *
 * It is the TRANSMIT half of V.90's sign-bit spectral shaping, and
 * `V90SignBitsExtractor` is the receive half of the same trellis -- read that
 * header beside this one, because the two describe one mechanism from its two
 * ends.
 *
 * One `process` call is one V.90 frame of `blockLength` samples.  The frame's
 * sign bits are differentially encoded (serially over the odd positions, then
 * per-position against the previous frame), applied as signs to the samples,
 * and pushed into a delay line that is `shaperId` frames deep.  Position 0 of
 * every frame is NOT a payload bit: `process` forces it to zero and the
 * trellis owns it, which is the same off-by-one `V90SignBitsExtractor::process`
 * spends on the receive side.
 *
 * `advanceTrellis` then searches, exhaustively, all 2^(shaperId+1) ways of
 * flipping the polarity of the frames still in the line.  Each candidate is
 * one entry of `actionLookupTable`, scored by running the embedded shaping
 * filter over a scratch copy of the whole line, and the lowest score wins.
 * Only the winner's LEADING action -- the one belonging to the frame about to
 * leave the delay line -- is committed, and its low bit becomes `state`, so
 * the next call's search starts from the branch this one took.  That is what
 * makes it a trellis rather than a per-frame decision.
 *
 * ---------------------------------------------------------------------------
 * `actionLookupTable` IS A DECIMAL ENCODING, and it is entirely regular
 *
 * 128 signed ints, `.data 0x8e0`, indexed `[state + 2 * shaperId][candidate]`
 * -- eight rows of sixteen, of which row `2 * shaperId + state` uses its
 * first 2^(shaperId+1) entries and the rest are zero.  Each entry is a
 * DECIMAL number of `shaperId + 1` digits, each digit in 1..4, one digit per
 * frame in the line, most significant digit for the OLDEST frame:
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
 * with `state` standing in as the previous bit for the leading digit.  So a
 * digit carries a state TRANSITION, which is why `digit - 1` is an `ACTIONS`
 * and why `state` ends up as `action & 1`.  The table is reproduced literally
 * in V90SpectralShaper.cpp; the rule above is stated so a reader can check it,
 * not because the object computes it.
 *
 * `applyAction` walks those digits with a signed `% 10` / `/= 10`, which is
 * what makes the table `int` and not `unsigned` (0x32a4d).  `pow10Table`
 * exists only so `advanceTrellis` can divide the winner by 10^shaperId and
 * recover the LEADING digit in one step.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE CONSTRUCTOR AND DESTRUCTOR PIN:
 *
 *     +0x20  constructed 0.  `advanceTrellis` stores 0 or 1 into it.
 *     +0x28  `sysdep_malloc(0x30)`, freed by the destructor.  24 SHORTS --
 *     +0x2c  `sysdep_malloc(0x30)`, freed by the destructor.  see the note on
 *            the element type below, which is a callee's and not a guess.
 *     +0x30  constructed 0.
 *     +0x34  constructed 24.  Stored AFTER both allocations, so the
 *            allocation size is not computed from it.
 *     +0x38  constructed 0, one byte, BEFORE the +0x3c subobject's
 *            constructor runs -- a member-initialiser and not a body store.
 *     +0x3c  a `ParallelDifferentialEncoder<unsigned char>`, constructed with
 *            6 and destroyed by the destructor's last call.  12 bytes, which
 *            is what DiffCoder.h already pinned from this very constructor.
 *     +0x48  a `V90SpectralShapingFilter`, default-constructed.  The
 *            destructor does NOT call one for it, which is half the evidence
 *            that that class has no destructor at all.
 *
 * THE OBJECT IS 108 BYTES, and the bound comes from OUTSIDE it as well as
 * from inside.  `V90Mapper` embeds one at +0x68c -- its constructor does
 * `lea 0x68c(%ebx),%eax` and calls `V90SpectralShaperC1` on it -- and the
 * next thing `V90Mapper::reset` and `V90Mapper::process` touch above that is
 * +0x6f8, which is 0x68c + 0x6c exactly.
 *
 * ---------------------------------------------------------------------------
 * THE TWO HEAP BUFFERS ARE `short *`, AND THAT IS A CALLEE'S WORD
 *
 * They used to be `unsigned short *`, inferred from `movzwl (%reg,%edx,2)` at
 * every access.  That inference was finding F614's FREE case in its purest
 * form: every one of those loads has its upper half discarded by a 16-bit
 * store or by a `neg` feeding one, so the compiler could have used either
 * instruction and the reading was never forced.
 *
 * What IS forced is that `process` hands +0x28 straight to
 * `V90SpectralShapingFilter::progress(const short *)` (0x330bb) and
 * `advanceTrellis` hands +0x2c straight to
 * `getMetric(const short *, unsigned)` (0x32cd8), with no conversion between.
 * A mangled parameter type is evidence class 2 and beats an inference from a
 * free encoding, so both are `short *`.  `applyAction`'s and
 * `applyFrameAction`'s own `short *` parameters -- also the mangling's -- are
 * the same evidence a third and fourth time.  Finding F5850.
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
	/* Defined in src/pump/v90/V90SpectralShaper.cpp. */
	V90SpectralShaper();
	~V90SpectralShaper();

	/*
	 * `ACTIONS` is the original author's own enum name, out of
	 * `_ZN17V90SpectralShaper16applyFrameActionENS_7ACTIONSEPsi`.
	 * ITS ENUMERATORS ARE NOT RECOVERABLE FROM THE MANGLING, so the four
	 * spellings below are DESCRIPTIVE and not the author's -- what is the
	 * object's is the four VALUES and what each one does, read off the
	 * switch at 0x3291e..0x32935 and the `neg`s behind it, over samples
	 * `i` counted from the start of the frame:
	 *
	 *     0   dst[i] =  src[i]                       keep the frame
	 *     1   dst[i] = -src[i]                       negate the frame
	 *     2   dst[i] = (i & 1) ?  src[i] : -src[i]   negate the even ones
	 *     3   dst[i] = (i & 1) ? -src[i] :  src[i]   negate the odd ones
	 *
	 * The switch is over a SIGNED value -- `cmp $0x1,%esi; jle` at
	 * 0x32923 with the negative side falling into the default -- which is
	 * what an `enum` parameter compiles to and is why this is an enum and
	 * not an `unsigned`.
	 *
	 * They are the same four arms, in the same four positions, as
	 * `V90SignBitsExtractor::ACTIONS`, which inverts BITS where this
	 * negates SAMPLES.  That is the receive side of this trellis and the
	 * correspondence is the reason to trust the ordering.
	 */
	enum ACTIONS {
		V90SS_KEEP_ALL		= 0,
		V90SS_NEGATE_ALL	= 1,
		V90SS_NEGATE_EVEN	= 2,
		V90SS_NEGATE_ODD	= 3
	};

	/*
	 * Set up for a connection: `V90MappingParams`' six shaper words, as
	 * `V90Mapper::reset` passes them (0x3025c).  The `unsigned` pair is
	 * the mangling's (`Ejjffff`) and the four floats go straight through
	 * to the embedded filter's coefficients.
	 */
	void reset(unsigned int shaperId, unsigned int shaperSR,
		   float a1, float a2, float b1, float b2);

	/* Re-run the filter's own two-step setup and nothing else. */
	void resetSSFilter(float a1, float a2, float b1, float b2);

	/*
	 * One frame of `blockLength` samples starting at `dst[start]`, taken
	 * from `delay` and written to `dst`.  Signatures are the mangling's:
	 * `NS_7ACTIONSEPsi` and `EiPs`.
	 */
	void applyFrameAction(ACTIONS action, short *dst, int start);

	/*
	 * `action` is one `actionLookupTable` entry: its digits are consumed
	 * least-significant first, digit `m` driving the frame at
	 * `(shaperId - m) * blockLength`, so the leading digit lands on the
	 * OLDEST frame.
	 */
	void applyAction(int action, short *dst);

	/* Choose and commit one frame's polarity; see the file comment. */
	void advanceTrellis();

	/*
	 * `in` is one frame of `blockLength` samples, `bits` its
	 * `blockLength - 1` payload sign bits, `out` the frame leaving the
	 * delay line `shaperId` frames later.  Mangling `EPsPhS0_`.
	 */
	void process(short *in, unsigned char *bits, short *out);

	/*
	 * `.data 0x8e0`, 512 bytes, and a GLOBAL `D` symbol so it is neither
	 * `const` nor in an anonymous namespace.  See the file comment for
	 * what the numbers mean.
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
	 * +0x12  After the serial differential encoder, which runs on the ODD
	 * positions only (`test $0x1,%bl; je` at 0x33002); the even positions
	 * are copied through.  This is `V90SignBitsExtractor`'s `oddDecoder`
	 * step seen from the transmit side.
	 */
	unsigned char	 codedBits[V90SS_FRAME_BITS];

	/*
	 * +0x18  After the per-position differential encoder, and what decides
	 * each sample's sign: `process` NEGATES the sample where the bit is
	 * ZERO (0x3305d..0x3306a), so 1 is positive.
	 */
	unsigned char	 signBits[V90SS_FRAME_BITS];
	/*
	 * +0x1e was `pad_1e[2]` -- REMOVED (finding F10145).  Already
	 * correctly described as alignment of the word below; proved
	 * mechanically by the existing `V90SS_OFF(signBits, 0x18, ...)`/
	 * `V90SS_OFF(state, 0x20, ...)` and by `dis.py` over every
	 * `V90SpectralShaper` method finding no access to 0x1e/0x1f.
	 */

	/*
	 * +0x20  THE TRELLIS STATE, one bit wide although it is a full word
	 * (`movl $0x1` at 0x32eb2, `movl $0x0` at 0x32f71).  It selects which
	 * half of `actionLookupTable` the search runs over, and
	 * `advanceTrellis` leaves it holding the committed action's low bit.
	 * The same two-state machine is `V90SignBitsExtractor::state`.
	 */
	unsigned int	 state;

	/*
	 * +0x24  FRAMES STILL TO BE PRIMED.  `reset` seeds it with `shaperId`
	 * and `process` counts it down one per call, running the trellis only
	 * once it has reached zero (0x33080..0x3308c) -- which is exactly the
	 * number of calls it takes to fill the `shaperId` frames of lookahead
	 * the search needs.  It is NOT a second copy of `shaperId`: nothing
	 * ever reads it as one, and `advanceTrellis` reads `shaperId` itself
	 * at +0x00 three times over and never touches this word.  Usage
	 * inference; finding F5851.
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
	 * +0x30  Where `process` writes the incoming frame, and it is
	 * INVARIANT at `shaperId * blockLength`: the write loop advances it by
	 * `blockLength` and the tail subtracts the same amount back off after
	 * shifting the line down.  `reset` seeds it, so the first
	 * `shaperId` frames of the line start out zero.
	 */
	unsigned int	 writeIndex;

	/*
	 * +0x34  The live length of the delay line, `(shaperId + 1) *
	 * blockLength` -- the frame in flight plus `shaperId` of lookahead.
	 * It bounds the copy into `trial` and the shift-down in `process`.
	 * The constructor's 24 is the ALLOCATED count and is overwritten by
	 * the first `reset`.
	 */
	unsigned int	 windowLength;

	/*
	 * +0x38  A `SerialDifferentialEncoder<unsigned char>`, and the type is
	 * FORCED: `process` does `lea 0x38(%edi),%edx` and hands that as the
	 * `this` of `_ZN25SerialDifferentialEncoderIhE7processEh` at 0x33016.
	 * One byte, so the three that follow are padding.
	 */
	SerialDifferentialEncoder<unsigned char> oddEncoder;
	/*
	 * +0x39 was `pad_39[3]` -- REMOVED (finding F10145): a 1-byte
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

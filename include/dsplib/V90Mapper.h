/**
 * @file V90Mapper.h
 * @brief `V90Mapper`, the V.90 downstream constellation mapper: turns
 *        buffered payload bits into one six-sample V.90 frame at a time,
 *        via a modulus encoder, an optional spectral shaper and a serial
 *        differential (sign) encoder.
 *
 * Five members and 2,400 bytes of code, all five written: the constructor,
 * the destructor, both resets, and `process`.
 *
 * Not polymorphic: `~V90Mapper` is listed with `D1` and `D2` and no `D0`,
 * so offset 0 is a real member and there is no vptr.
 *
 * The size is 0x704, the original compiler's own `sizeof`: `V90BitsToSymbol`'s
 * constructor is
 *
 *     movl $0x704,(%esp) ; call sysdep_malloc ; ... ; call V90Mapper::C1
 *
 * -- the allocation is `sizeof(V90Mapper)` written by the compiler that laid
 * the class out, which is better evidence than any displacement (finding
 * F1246). +0x700 is a four-byte member and both resets store zero into it
 * with a `movl`, so the class ends exactly at 0x704 with no tail padding.
 * See `word_700`.
 *
 * The two embedded sub-objects are fixed points, not guesses. The
 * constructor does `lea 0x670(%ebx),%edx ; call ModulusEncoder::C1` and
 * `lea 0x68c(%ebx),%eax ; call V90SpectralShaper::C1` -- a `lea` and not a
 * load, so they are embedded and not pointed at. `sizeof(ModulusEncoder)`
 * is 0x1c (asserted in src/pump/v90/ModulusCoder.cpp) and 0x670 + 0x1c =
 * 0x68c, so they abut. `sizeof(V90SpectralShaper)` is 0x6c (asserted in
 * src/pump/v90/V90SpectralShaper.cpp, and derived independently from
 * inside that class) and 0x68c + 0x6c = 0x6f8, exactly where the next
 * field the constructor writes sits. Three sizes settled elsewhere and one
 * displacement here agree to the byte.
 *
 * Both resets confirm the first of them from the other side: they fill
 * +0x670..+0x688 with seven words -- the six constellation sizes and
 * `word_08` -- which is `ModulusEncoder`'s whole seven-member layout
 * written out by hand, in the same order and from the same sources as
 * `V90Demapper::reset` writes into its embedded `ModulusDecoder`. There is
 * no call: the blob has no `ModulusEncoder::reset` symbol
 * (`V92ModulusEncoder` does have one, and the V.90 pair do not), so the
 * stores are the author's own and not an inlined member.
 *
 * The destructor does not destroy the modulus encoder: it calls
 * `V90SpectralShaper::~V90SpectralShaper` and nothing else, which is what a
 * compiler emits when the other member is trivially destructible --
 * `ModulusEncoder` has a constructor and no destructor, and `nm` on the blob
 * shows no `_ZN14ModulusEncoderD*` symbol at all.
 *
 * ---------------------------------------------------------------------------
 * This class is `V90Demapper` seen from the transmit side, and that is where
 * the names at +0x04..+0x14 come from
 *
 * `V90Demapper::reset` computes five quantities out of the same
 * `V90MappingParams` and stores them at the same five offsets, by the same
 * arithmetic, and `include/dsplib/V90Demapper.h` already names all five:
 *
 *     +0x04   bitsPerFrame        = mp->word_0
 *     +0x08   word_08             = bitsPerFrame - signBitsPerFrame
 *     +0x0c   signBitsPerFrame    = 6 - mp->shaperSR
 *     +0x10   signBitGroups       = mp->shaperSR
 *     +0x14   signBitGroupSize    = 6 / mp->shaperSR
 *
 * The names are therefore this tree's own from the other class rather than
 * fresh inventions, and two of them are corroborated by what `process` does
 * with them here: it branches on `signBitGroups` being zero exactly as the
 * demapper does, and it strides the frame it hands `V90SpectralShaper::
 * process` by `signBitGroupSize` -- which is the shaper's own `blockLength`,
 * `6 / shaperSR`, computed a second time.  `word_08` keeps the demapper's
 * deliberately unnamed spelling for the reason that header gives at length:
 * calling it the modulus bit count assumes what `ModulusEncoder` does with
 * its seventh word, and that class's members are all `field_NN`.
 *
 * The one place the two classes DISAGREE is worth stating, because it is the
 * kind of difference a shared name hides.  With `shaperSR` zero the demapper
 * leaves `signBitGroupSize` STALE (`if (shaperSR != 0)` around the division);
 * the mapper stores zero (`movl $0x0,0x14(%ebp)` at 0x30206).  Both are
 * reproduced as written.
 *
 * ---------------------------------------------------------------------------
 * +0x020..+0x055 IS 54 BYTES, AND IT IS NOW FOUR NAMED ARRAYS
 *
 * It was `pad_020[0x36]` while `process` was unwritten, deliberately: finding
 * F7103 had already tiled it, and a field no differential test can fail on is
 * finding F3120's hazard rather than progress.  `process` is written now, it is
 * the only member that touches any of the four, and `t_v90modchain`'s
 * `V90Mapper::process` group compares all 54 bytes against the blob's after
 * every frame -- so the tiling has a test behind it and the names go in.
 *
 *     +0x20   `mov 0x20(%ebx,%edx,4),%ecx`   %edx 0..5   6 * 4 = 24 -> +0x38
 *     +0x38   `mov %si,0x38(%ebx,%edx,2)`    %edx 0..5   6 * 2 = 12 -> +0x44
 *     +0x44   `mov %al,0x44(%esi,%ebx,1)`    %esi 0..5   6 * 1 =  6 -> +0x4a
 *     +0x4a   `mov %dx,0x4a(%ebx,%esi,2)`    %esi 0..5   6 * 2 = 12 -> +0x56
 *
 * -- one entry per sample of a six-sample V.90 frame in each, four independent
 * bases and three exact meetings, which is the standard `V90Demapper.h` set
 * for its own four parallel arrays.
 *
 * THREE OF THE FOUR ARE TYPED BY A MANGLING and one is not, which is the
 * difference worth carrying (CLAUDE.md's evidence order; finding F7420):
 *
 *   - `codes` and `signs` are `V90Demapper`'s own two names, for the same two
 *     quantities, reached the same way.  `codes` is handed to
 *     `ModulusEncoder::progress(unsigned char *, unsigned int *)` as its
 *     second operand and `signs` holds what
 *     `SerialDifferentialEncoder<unsigned char>::process(unsigned char)`
 *     returned -- element types from the manglings, roles from the demapper's
 *     twin pair at +0x1c and +0x20 of that class.
 *   - `levels` is `V90SpectralShaper::process(short *in, ...)`'s first
 *     operand, so `short` is forced; the name is what `process` puts in it,
 *     one `constellation[k][codes[k]]` per sample.
 *   - `samples` is the ONE INFERENCE.  Both of the shaper's other operands are
 *     `short *` too, so the mangling cannot separate `out` from `in`, and the
 *     demapper has no twin for it.  What bounds it is that `process` writes it
 *     from three places and reads it from one: the shaper's `out`, and
 *     `levels[k]` with the sign from `signs[k]` applied on the other arm; and
 *     the only reader is the tail that copies it to the caller's `short *`.
 *     So it is the frame as it leaves this class.  Evidence class 3.
 *
 *     WHAT THE SHAPER ARM PUTS THERE IS NOW SETTLED, and it is class 2 rather
 *     than 3: `V90SpectralShaper::process` is reconstructed, and its body ends
 *     `for (i = 0; i < blockLength; i++) out[i] = delayLine[i];` -- so `out`
 *     receives the OLDEST frame in the line, one that entered
 *     `windowLength / blockLength` frames ago and whose polarity
 *     `advanceTrellis()` may since have flipped.  It is not this frame's
 *     signed levels.
 *
 *     THE NAME STAYS GENERIC ANYWAY, and deliberately: the OTHER arm writes
 *     `samples[k] = signs[k] ? levels[k] : -levels[k]`, which IS this frame's
 *     signed levels.  One field, two producers, two different things -- so a
 *     shaper-specific name like `shapedFrame` would be exactly wrong half the
 *     time.  This is the `history_2aa8_index` case from the V.34 anonymous-
 *     field review: where two paths fill one slot, the generic name is the
 *     accurate one and the specific name is a lie about the other path.
 *
 * ---------------------------------------------------------------------------
 * Data member names are invented unless said otherwise; the mangling never
 * carries one (finding F226).
 */

#ifndef DSPLIB_V90MAPPER_H
#define DSPLIB_V90MAPPER_H

#include "dsplib/DiffCoder.h"		/* SerialDifferentialEncoder<>      */
#include "dsplib/ModulusCoder.h"	/* ModulusEncoder, embedded at +0x670 */
#include "dsplib/V90Phase3Modulator.h"	/* for `PcmType`; see `reset` below */
#include "dsplib/V90SpectralShaper.h"	/* embedded at +0x68c               */

class V90Parameters;
class V90MappingParams;

/*
 * The shape of `constellation`.  Six because the RBS cycle is six frames
 * long and the fill loop's outer counter is `cmp $0x5,%edi; jbe`; 128 because
 * the tail-fill's is `cmp $0x7f,%edx; jbe`.  They are the same two dimensions
 * `V90MappingParams` uses for the tables this array is built from, and
 * separate macros for the reason `V90Demapper.h` gives: a reader who saw one
 * macro would not know the other use had been checked.
 */
#define V90MAPPER_CONSTELLATIONS	6
#define V90MAPPER_LEVELS		128

/*
 * The samples in one V.90 frame.  UNSIGNED, and that is measured rather than
 * stylistic: `reset` divides it by `shaperSR` with `f7 f1  div %ecx` at
 * 0x3009c and not `idiv`, so the numerator's type is unsigned even though
 * `V90MappingParams::shaperSR` is declared `int`.  This is the same reading
 * `V90MappingParams.h` records for `V90DEMAPPER_FRAME`, from the same
 * instruction in the demapper.
 */
#define V90MAPPER_FRAME			6u

class V90Mapper {
public:
	/**
	 * @brief Construct the mapper: allocate its bit buffer and zero its state.
	 * @param params  The V.90 parameter block; stored, not read yet.
	 */
	V90Mapper(V90Parameters *params);
	/** @brief Destroy the mapper: free `buf` (not null-checked, not nulled after) and the spectral shaper. */
	~V90Mapper();

	/**
	 * @brief Set up for a connection, with spectral shaping.
	 *
	 * Computes `bitsPerFrame`, `signBitGroups`, `signBitGroupSize`,
	 * `signBitsPerFrame` and `word_08` from @p mp; builds the six
	 * constellations by companding @p mp's tables per @p pcm; loads the
	 * modulus encoder's seven fields; resets the sign encoder; and, if
	 * `signBitGroups` is nonzero, resets the embedded spectral shaper
	 * and primes the +0x6f8 countdown from `mp->shaperId` (left at 0
	 * otherwise). `signBitGroupSize` is stored 0 when there is no
	 * shaper, where V90Demapper::reset() instead leaves the
	 * corresponding field stale.
	 *
	 * @param mp   The V.90 mapping parameters for this connection.
	 * @param pcm  Zero selects mu-law; any other value selects A-law
	 *             (tested for nonzero, never compared against a specific value).
	 */
	void reset(V90MappingParams *mp, PcmType pcm);
	/**
	 * @brief Set up for a connection, without spectral shaping.
	 *
	 * `reset()` without the shaper: leaves `signBitsPerFrame`,
	 * `signBitGroups`, `signBitGroupSize`, `bitsBuffered` and +0x6f8
	 * exactly as found, but still reads `signBitsPerFrame` to compute
	 * `word_08`.
	 *
	 * @param mp   The V.90 mapping parameters for this connection.
	 * @param pcm  Zero selects mu-law; any other value selects A-law.
	 */
	void resetNoSpectral(V90MappingParams *mp, PcmType pcm);

	/**
	 * @brief Buffer payload bits and emit whole V.90 frames as they fill.
	 *
	 * Appends each bit of @p bits to `buf` at `bitsBuffered`; once
	 * `bitsPerFrame` bits have accumulated, builds one frame (the
	 * modulus encoder turns the non-sign bits into six digits that
	 * select a level from each constellation; the sign bits either
	 * drive the spectral shaper in `signBitGroups` groups of
	 * `signBitGroupSize` samples, or, with no shaper, the serial
	 * differential encoder one bit at a time) and takes `bitsPerFrame`
	 * back off `bitsBuffered` rather than clearing it, so a
	 * part-filled frame carries over to the next call. The +0x6f8
	 * priming countdown suppresses whole frames and then part of one at
	 * the start of a connection, so @p nofSymbols is not simply
	 * `nofBits / bitsPerFrame * 6`.
	 *
	 * @param bits        One payload bit per byte.
	 * @param nofBits     How many bits of @p bits are valid.
	 * @param symbols     Receives the finished frames' samples.
	 * @param nofSymbols  Receives how many samples were written.
	 */
	void process(unsigned char *bits, unsigned int nofBits, short *symbols,
		     unsigned int &nofSymbols);

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	V90Parameters *params;			/* +0x000 the argument   */

	/* +0x004 .. +0x014  See the file comment for where these names are
	 * from.  All five are zeroed by the constructor. */
	unsigned int bitsPerFrame;		/* +0x004 mp->word_0     */
	unsigned int word_08;			/* +0x008                */
	unsigned int signBitsPerFrame;		/* +0x00c 6 - shaperSR   */
	unsigned int signBitGroups;		/* +0x010 mp->shaperSR   */
	unsigned int signBitGroupSize;		/* +0x014 6 / shaperSR   */

	void *buf;				/* +0x018 malloc(0x50)   */

	/*
	 * +0x01c  HOW MANY BITS ARE IN `buf`.  Zeroed by the constructor and
	 * again by `reset`, and NOT by `resetNoSpectral`.  It was
	 * `cleared_01c`, on the header's own terms -- "the name stays the
	 * offset's until that member is written" -- and `process` is that
	 * member: it indexes `buf` with this (`mov %al,(%ecx,%edi,1)`),
	 * increments it once per input byte, runs a frame when it reaches
	 * `bitsPerFrame`, and takes `bitsPerFrame` back off it afterwards
	 * (`mov 0x1c(%ebx),%eax ; sub 0x4(%ebx),%eax ; mov %eax,0x1c(%ebx)`)
	 * rather than clearing it, which is what carries a part-filled frame
	 * across calls.  That is mechanism and not inference.  Finding F7421.
	 */
	unsigned int bitsBuffered;

	/*
	 * +0x020 .. +0x055  ONE ENTRY PER SAMPLE OF THE SIX-SAMPLE FRAME, in
	 * four parallel arrays, and `process` is the only member that touches
	 * any of them.  See the file comment for which three are typed by a
	 * mangling and which one is not, and finding F7103 for the tiling.
	 */
	unsigned int  codes[V90MAPPER_FRAME];	/* +0x020 modulus digits */
	short	      levels[V90MAPPER_FRAME];	/* +0x038 the PCM levels */
	unsigned char signs[V90MAPPER_FRAME];	/* +0x044 1 is positive  */
	short	      samples[V90MAPPER_FRAME];	/* +0x04a the frame out  */

	/*
	 * +0x056  THE SIX CONSTELLATIONS, up to 128 linear PCM levels each,
	 * and the base and the shape are both read off the fill loop that
	 * both resets run: `mov %ax,0x56(%ebp,%ebx,2)` with
	 * `%ebx = 128 * k + i`, `k` counted 0..5 against `constellationSize[k]`
	 * and the stack slot holding the row base advanced by
	 * `subl $0xffffff80` -- add 128 -- once per `k`.  0x56 + 6 * 128 * 2 =
	 * 0x656, two bytes short of `constellationSize`, which is the pad
	 * below.
	 *
	 * `short` AND NOT `unsigned short`, and the encodings do not decide
	 * it.  The only load in the object is `movzwl 0x56(%ebx,%edi,2),%esi`
	 * in `process`, whose upper half is immediately discarded by
	 * `mov %si,0x38(%ebx,%edx,2)` -- finding F614's FREE case, so the
	 * compiler could have used either instruction.  What does decide it
	 * is the VALUES: every entry is an `alaw2linear`/`ulaw2linear` result,
	 * which is a signed 16-bit PCM level and is negative for half the
	 * codes.  `V90Demapper`'s twin array is `short` for a stronger reason
	 * -- a `movswl` whose 32-bit result is used -- and holds the same kind
	 * of level.
	 *
	 * THE FILL IS NOT BOUNDED AT 128.  `constellationSize[k]` above 128
	 * runs row `k` into row `k + 1`, exactly as `V90Demapper.h` records
	 * for its own array: there is a `shl $0x7` and an `add` and no test.
	 */
	short constellation[V90MAPPER_CONSTELLATIONS][V90MAPPER_LEVELS];
	unsigned char pad_656[2];		/* +0x656 alignment      */

	/*
	 * +0x658  The six constellation sizes, copied word for word out of
	 * `V90MappingParams::constellationSize` by both resets and used
	 * UNSIGNED (`cmp $0x0,%eax; jbe` and `cmp %esi,%eax; ja`).  The
	 * constructor zeroes all six in a rolled loop.  It used to be
	 * `cleared_658`, named for the constructor because nothing else was
	 * written; the resets name it now, and `V90Demapper` calls the field
	 * it copies the same six words into by the same name.
	 */
	unsigned int constellationSize[V90MAPPER_CONSTELLATIONS];

	ModulusEncoder modulusEncoder;		/* +0x670 0x1c bytes     */
	V90SpectralShaper spectralShaper;	/* +0x68c 0x6c bytes     */

	/*
	 * +0x6f8  Zeroed by the constructor; `reset` stores `mp->shaperId`
	 * when the shaper is running and zero when it is not, and
	 * `resetNoSpectral` does not touch it.  `process` reads it, tests it
	 * for nonzero and counts it down by `signBitGroups`, which is the
	 * shape of a priming countdown like `V90SpectralShaper::primeFrames`
	 * -- but that is one unwritten function's arithmetic and the name
	 * waits for it.  `type_NNNN` per CLAUDE.md: the width and the shape
	 * are known, the meaning is not.
	 */
	unsigned int uint_6f8;

	/*
	 * +0x6fc  A `SerialDifferentialEncoder<unsigned char>`, and the type
	 * is FORCED by a `this` rather than inferred from the store: `process`
	 * does `lea 0x6fc(%ebx),%edi` and hands that as the first stack
	 * argument of `_ZN25SerialDifferentialEncoderIhE7processEh` at
	 * 0x3057e.  A mangled parameter type is CLAUDE.md's second-strongest
	 * evidence, and this is the transmit twin of `V90Demapper`'s
	 * `SerialDifferentialDecoder<unsigned char> signDecoder` -- one byte,
	 * zeroed by the constructor and by both resets, driving the sign-bit
	 * path taken when `signBitGroups` is zero.  It was `cleared_6fc`.
	 */
	SerialDifferentialEncoder<unsigned char> signEncoder;
	unsigned char pad_6fd[3];		/* +0x6fd alignment      */

	/*
	 * +0x700  FOUR BYTES, WRITTEN BY BOTH RESETS AND READ BY NOTHING.
	 * Each stores the constant zero with a 32-bit `mov %edi,0x700(%ebp)`
	 * (0x301aa and 0x303d1), which forces the width and nothing else.
	 * The CONSTRUCTOR does not write it, which is unusual enough to say
	 * out loud: it is what lets a never-zeroed fixture see the store at
	 * all.
	 *
	 * A sweep of every `0x700(%` displacement in the object finds exactly
	 * those two stores plus three `movswl 0x700(%ebx,%esi,2)` inside
	 * `V90AutoDigitalImpDetector::studyUrefHandler`, which is a different
	 * class -- so "nothing reads it" is a measurement over the whole
	 * object and not an impression (finding F4342's rule).  A write-only
	 * slot has no meaning to take a name from; `V90MappingParams::word_61c`
	 * is the same case and the precedent for the spelling.  Finding F7102.
	 */
	unsigned int word_700;
};

#endif /* DSPLIB_V90MAPPER_H */

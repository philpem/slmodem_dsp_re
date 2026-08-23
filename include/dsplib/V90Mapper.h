/*
 * V90Mapper.h -- the V.90 downstream constellation mapper.
 *
 * Reconstructed from dsplibs.o.  Five members and 2,400 bytes of code, of
 * which the constructor, the destructor and both resets are written here;
 * `process` is not.
 *
 * NOT POLYMORPHIC: `~V90Mapper` is listed with `D1` and `D2` and no `D0`, so
 * offset 0 is a real member and there is no vptr.
 *
 * THE SIZE IS 0x704, AND IT IS THE ORIGINAL COMPILER'S OWN `sizeof`.
 * `V90BitsToSymbol`'s constructor is
 *
 *     movl $0x704,(%esp) ; call sysdep_malloc ; ... ; call V90Mapper::C1
 *
 * -- the allocation is `sizeof(V90Mapper)` written by the compiler that laid
 * the class out, which is better evidence than any displacement (finding
 * 1246).  The "one member of unknown width at 0x700 -- either reading is
 * consistent" sentence that used to stand here IS RETRACTED: +0x700 is a
 * four-byte member and both resets store zero into it with a `movl`, so the
 * class ends exactly at 0x704 with no tail padding at all.  See `word_700`.
 *
 * THE TWO EMBEDDED SUBOBJECTS ARE FIXED POINTS, NOT GUESSES.  The constructor
 * does `lea 0x670(%ebx),%edx ; call ModulusEncoder::C1` and `lea
 * 0x68c(%ebx),%eax ; call V90SpectralShaper::C1` -- a `lea` and not a load,
 * so they are embedded and not pointed at.  `sizeof(ModulusEncoder)` is 0x1c
 * (asserted in src/pump/v90/ModulusCoder.cpp) and 0x670 + 0x1c = 0x68c, so
 * they abut.  `sizeof(V90SpectralShaper)` is 0x6c (asserted in
 * src/pump/v90/V90SpectralShaper.cpp, and derived independently from inside
 * that class) and 0x68c + 0x6c = 0x6f8, which is exactly where the next field
 * the constructor writes sits.  Three sizes settled elsewhere and one
 * displacement here agree to the byte.
 *
 * AND BOTH RESETS CONFIRM THE FIRST OF THEM FROM THE OTHER SIDE.  They fill
 * +0x670..+0x688 with seven words -- the six constellation sizes and
 * `word_08` -- which is `ModulusEncoder`'s whole seven-member layout written
 * out by hand, in the same order and from the same sources as
 * `V90Demapper::reset` writes into its embedded `ModulusDecoder`.  There is
 * no call: the blob has no `ModulusEncoder::reset` symbol (`V92ModulusEncoder`
 * does have one, and the V.90 pair do not), so the stores are the author's
 * own and not an inlined member.
 *
 * THE DESTRUCTOR DOES NOT DESTROY THE MODULUS ENCODER.  It calls
 * `V90SpectralShaper::~V90SpectralShaper` and nothing else, which is what a
 * compiler emits when the other member is trivially destructible --
 * `ModulusEncoder` has a constructor and no destructor, and `nm` on the blob
 * shows no `_ZN14ModulusEncoderD*` symbol at all.
 *
 * ---------------------------------------------------------------------------
 * THIS CLASS IS `V90Demapper` SEEN FROM THE TRANSMIT SIDE, AND THAT IS WHERE
 * THE NAMES AT +0x04..+0x14 COME FROM
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
 * +0x020..+0x055 IS 54 BYTES AND IS *NOT* OPAQUE -- but it is left padded
 *
 * `V90Mapper::process` tiles all 54 of them, and the four bases meet exactly
 * at `constellation`:
 *
 *     +0x20   `mov 0x20(%ebx,%edx,4),%ecx`   %edx 0..5   6 * 4 = 24 -> +0x38
 *     +0x38   `mov %si,0x38(%ebx,%edx,2)`    %edx 0..5   6 * 2 = 12 -> +0x44
 *     +0x44   `mov %al,0x44(%esi,%ebx,1)`    %esi 0..5   6 * 1 =  6 -> +0x4a
 *     +0x4a   `mov %dx,0x4a(%ebx,%esi,2)`    %esi 0..5   6 * 2 = 12 -> +0x56
 *
 * -- one entry per sample of a six-sample V.90 frame in each.  That is the
 * same standard of evidence `V90Demapper.h` used for its own four parallel
 * arrays: four independent bases and three exact meetings.
 *
 * IT IS STILL `pad_020` HERE, DELIBERATELY.  Neither reset touches any of the
 * four, so nothing in this batch can put a differential test behind them, and
 * a field nobody can fail on is finding 3120's hazard rather than progress.
 * `process` is the member that reads and writes them and it is the batch that
 * should model them.  Finding 7103 carries the tiling so it is not re-derived.
 *
 * ---------------------------------------------------------------------------
 * Data member names are invented unless said otherwise; the mangling never
 * carries one (finding 226).
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
	/* Defined in src/pump/v90/V90Mapper.cpp. */
	V90Mapper(V90Parameters *params);
	~V90Mapper();

	/*
	 * Set up for a connection.  Signatures are the mangling's:
	 * `_ZN9V90Mapper5resetEP16V90MappingParams7PcmType` and
	 * `_ZN9V90Mapper15resetNoSpectralEP16V90MappingParams7PcmType`.
	 *
	 * `pcm` IS TESTED FOR NONZERO AND NEVER COMPARED AGAINST A VALUE --
	 * `test %ebx,%ebx ; jne` in both -- so zero is mu-law and ANYTHING
	 * else is A-law, which is what `PcmType` already records.
	 *
	 * `resetNoSpectral` is `reset` without the spectral shaper: it leaves
	 * +0x0c, +0x10, +0x14, +0x1c and +0x6f8 exactly as it found them and
	 * READS +0x0c to form `word_08`.  That is the same division of labour
	 * `V90Demapper` has between its own two.
	 */
	void reset(V90MappingParams *mp, PcmType pcm);
	void resetNoSpectral(V90MappingParams *mp, PcmType pcm);

	/*
	 * `process` is NOT declared here.  It is another batch's work, and a
	 * declaration would have to state a return type the object does not
	 * carry -- the mangling has none and the epilogue at 0x30558 leaves
	 * `%eax` holding whatever the last store computed.
	 */

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
	 * +0x01c  Zeroed by the constructor and again by `reset`, and NOT by
	 * `resetNoSpectral`.  `process` runs it as a bit account against
	 * `bitsPerFrame` (`mov 0x1c(%ebx),%eax ; sub 0x4(%ebx),%eax ; mov
	 * %eax,0x1c(%ebx)`), which bounds what it is without settling it, so
	 * the name stays the offset's until that member is written.
	 */
	unsigned int cleared_01c;

	/* +0x020  Four arrays of six; see the file comment and finding 7103. */
	unsigned char pad_020[0x36];

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
	 * `mov %si,0x38(%ebx,%edx,2)` -- finding 614's FREE case, so the
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
	 * object and not an impression (finding 4342's rule).  A write-only
	 * slot has no meaning to take a name from; `V90MappingParams::word_61c`
	 * is the same case and the precedent for the spelling.  Finding 7102.
	 */
	unsigned int word_700;
};

#endif /* DSPLIB_V90MAPPER_H */

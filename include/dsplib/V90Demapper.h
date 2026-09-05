/**
 * @file V90Demapper.h
 * @brief `V90Demapper`: the V.90 receive-side demapper -- turns decided
 *        constellation points into bits, and studies the mapping's accuracy
 *        along the way.
 *
 * Reconstructed from `dsplibs.o` (the class had no header or `.cpp` in this
 * tree before this file, though `.symtab` shows the original had its own
 * translation unit, `V90Demapper.cpp`).
 *
 * `sizeof(V90Demapper) == 0x1eb8` (7,864 bytes), taken from the object's own
 * allocation size at both of its two construction sites (finding F1170). It
 * is not polymorphic -- its destructor has no deleting variant, so offset 0
 * is a real member and there is no vptr (F228).
 *
 * The class carries four parallel `[6][128]` tables indexed by (RBS frame
 * position, PCM level): `constellation` (the demapper's own decision
 * levels), and `errorSum`/`errorCount` (the study's running per-level error
 * statistics). Their bases meet exactly -- `constellation` ends where
 * `constellationSize` begins, `errorSum` ends where `errorCount` begins --
 * which is how the shape was confirmed rather than assumed (F1170).
 * `constellation` is signed (`short`) because the diagnostic that prints it
 * sign-extends the load; `errorSum`/`errorCount` and their shared divide are
 * unsigned throughout.
 */

#ifndef DSPLIB_V90DEMAPPER_H
#define DSPLIB_V90DEMAPPER_H

#include "dsplib/ModulusCoder.h"
#include "dsplib/V90SignBitsExtractor.h"

/*
 * Pointers only, so a forward declaration is all this header needs.
 * `V90Demapper.cpp` includes the real `V90Parameters.h` and reads named
 * fields such as `DEBUG_DEMAPPER_ERROR_HISTOGRAM` (the author's own name
 * for the field the destructor branches on). Finding F1112 recorded a
 * second, incompatible `V90Parameters` definition that once forced this
 * choice; that duplication was retired at task #116 (finding F6402), so
 * this is now an ordinary include-cost forward declaration rather than a
 * one-definition-only workaround.
 */
class V90Parameters;
class V90AutoDigitalImpDetector;
class V90MappingParams;

/* The two dimensions of the four parallel arrays; see the file comment. */
#define V90DEMAPPER_CONSTELLATIONS	6
#define V90DEMAPPER_LEVELS		128

/*
 * The samples in one V.90 frame, and a SEPARATE constant from the count above
 * even though both are six.  `process` steps its cursor by it (`lea 0x6(%edx)`
 * at 0x31750) and refuses to run below it (`cmp $0x5,%ebx; jbe`); the six
 * above is how many constellations the RBS cycle holds.  They are the same
 * six for the same reason -- one sample per RBS position -- but a reader who
 * saw only one macro would not know that either use had been checked.
 */
#define V90DEMAPPER_FRAME		6u

class V90Demapper {
public:
	/**
	 * @brief Construct the demapper: allocate its two per-sample buffers and
	 *        derive its sign-bit split from `levels`.
	 * @param levels  Bits-per-frame budget the mapping targets; drives
	 *                `bitsPerFrame`/`signBitsPerFrame`/`signBitGroups`.
	 * @param params  The V.90 parameter block (not owned).
	 * @param adi     The automatic digital-impairment detector the linear
	 *                mapping study reports into (not owned).
	 */
	V90Demapper(unsigned int levels, V90Parameters *params,
		    V90AutoDigitalImpDetector *adi);

	/** @brief Free the two owned per-sample buffers (`codes`, `signs`). */
	~V90Demapper();

	/**
	 * @brief Turn one decided sample into a constellation index and error
	 *        contribution, appending it to the pending frame.
	 *
	 * Selects the constellation by the current RBS frame position,
	 * decides its nearest level, folds the result into the error
	 * histogram once the configured delay has elapsed, records the
	 * decision for linearMappingStudy(), and advances the RBS frame
	 * position.
	 *
	 * @param in  The received sample.
	 * @return The decided level, in the constellation's own units.
	 */
	short hardDecision(short in);

	/**
	 * @brief Drain whole frames of buffered samples into output bits.
	 *
	 * Consumes the buffered samples six at a time (one V.90 frame),
	 * splitting each frame's sign bits out through `signBits` or the
	 * embedded serial decoder before handing the remainder to
	 * `modulusDecoder`, and compacts any leftover partial frame back to
	 * the front of the buffer.
	 *
	 * @param out    Destination bit buffer.
	 * @param nbits  Running output-bit count, advanced by this call.
	 * @return Non-zero once at least one frame was processed.
	 */
	int process(unsigned char *out, unsigned int &nbits);

	/**
	 * @brief Arm the linear mapping study for `length` completed runs.
	 * @param length  Number of decisions the study should run before its
	 *                end-of-run pass fires.
	 */
	void resetLinearMappStudy(unsigned int length);

	/**
	 * @brief Reset the demapper's constellations and study state from a
	 *        non-spectral mapping.
	 * @param mapp  The mapping parameters to rebuild the constellations from.
	 */
	void resetNoSpectral(V90MappingParams *mapp);

	/** @brief Advance the RBS frame position by one, modulo six. */
	void incrementRBSFramePosition();

	/**
	 * @brief Accumulate one linear-mapping-study sample and, every second
	 *        completed run, raise the study's enable flag for the next stage.
	 * @param sample  The received sample that produced the decision.
	 * @param level   The decided level (constellation index).
	 */
	void linearMappingStudy(short sample, short level);

	/** @brief Recompute a constellation's decision levels from its current statistics. */
	void updateConstelation();

	/**
	 * @brief Reset the demapper to its just-constructed state for a new mapping.
	 * @param mapp  The mapping parameters to reset from.
	 */
	void reset(V90MappingParams *mapp);

	/**
	 * @brief Print the per-level error histogram for each constellation
	 *        (via `edprintf`, which gates on the debug level itself) and
	 *        clear it.
	 *
	 * Both the print and the underlying zeroing are skipped together if
	 * any one of the six constellations has zero levels (F1171); the call
	 * counter still advances either way.
	 */
	void printErrorHistogramAndReset();

	/*
	 * Data members are public for the reason V90Jd.h gives: the original's
	 * access specifiers are not recoverable from the mangling, and a
	 * single access section is what lets the .cpp assert every offset
	 * below with __builtin_offsetof.
	 */

	/*
	 * +0x00  The parameter block, not owned. The destructor's only
	 * dereference through it is `DEBUG_DEMAPPER_ERROR_HISTOGRAM` (+0x52c).
	 */
	V90Parameters *params;

	/*
	 * +0x04 .. +0x18  Six words the constructor zeroes with six
	 * consecutive `movl $0x0`.  Five of the six are named from `process`
	 * and `hardDecision`; the sixth is read by nothing this tree has.
	 *
	 * THE FRAME IS SIX SAMPLES AND THE ARITHMETIC BELOW ALL HANGS OFF
	 * THAT.  `process` consumes samples six at a time (`lea 0x6(%edx)`,
	 * loop guard `frameStart + 6 <= sampleCount`), and one such frame
	 * yields `bitsPerFrame` bits laid out as `signBitsPerFrame` sign bits
	 * followed by whatever `ModulusDecoder::progress` writes after them.
	 */

	/*
	 * +0x04  Bits produced per six-sample frame.  `process` adds it to its
	 * running total once per frame and returns that total through its
	 * reference argument, and it is also the stride of the output buffer.
	 */
	unsigned int bitsPerFrame;

	/*
	 * +0x08  Modelled, unnamed. `resetNoSpectral` both computes and reads
	 * it back:
	 *
	 *     30d09:  mov  0xc(%ecx),%ebp          <- signBitsPerFrame
	 *     30d0e:  mov  %ebx,0x4(%ecx)          <- bitsPerFrame = mapp->[0]
	 *     30d11:  sub  %ebp,%ebx
	 *     30d13:  mov  %ebx,0x8(%ecx)          <- here
	 *     ...
	 *     30e77:  mov  %esi,0x18(%eax)         <- modulusDecoder's +0x18
	 *
	 * so it is `bitsPerFrame - signBitsPerFrame`, and it is the seventh
	 * word handed to the embedded `ModulusDecoder`.
	 *
	 * Left unnamed on purpose. The arithmetic makes "the bits of a frame
	 * that are not sign bits" certain; calling it the modulus bit count
	 * would additionally assume what `ModulusDecoder` does with its
	 * seventh word, and that class's seven members are all `field_NN`
	 * because nothing in the object names them either. A wrong name is
	 * believed by every future reader and no test can fail on it
	 * (F3120), so the derivation stays here and the name waits for
	 * `ModulusDecoder::progress`.
	 */
	unsigned int word_08;

	/*
	 * +0x0c  How many sign bits a frame carries, and therefore where the
	 * modulus bytes start: `process` hands `ModulusDecoder::progress` the
	 * output pointer already advanced by it.  It is the loop bound of the
	 * serial-decoder path too, so the two readings agree.
	 */
	unsigned int signBitsPerFrame;

	/*
	 * +0x10  How many sign-bit GROUPS a frame is cut into, and the switch
	 * between this class's two sign-bit paths: zero means the whole frame
	 * goes through the serial decoder at +0x664, non-zero means it is cut
	 * into this many groups and each goes through `signBits`.
	 *
	 * It is `V90SignBitsExtractor::reset`'s `spacing`: that member sets the
	 * extractor's width to `6 / spacing`, and `groups * groupSize == 6`
	 * below is what makes the two consistent.
	 */
	unsigned int signBitGroups;

	/*
	 * +0x14  Samples per group -- the extractor's `width`.  Each group
	 * consumes this many samples of `signs` and produces `width - 1` bits,
	 * which is why `process` strides its input by it and its output by one
	 * less.
	 */
	unsigned int signBitGroupSize;

	/*
	 * +0x18  The RBS frame position, 0..5. `hardDecision` selects the
	 * constellation by it and advances it `(pos + 1) % 6` (an unsigned
	 * division), copying the pre-advance value to `decisionFramePosition`;
	 * `incrementRBSFramePosition` is the identical advance, repeated
	 * rather than called, for use by a caller outside this class.
	 */
	unsigned int rbsFramePosition;

	/*
	 * +0x1c and +0x20  The two heap blocks the destructor frees, sized
	 * from the constructor's `levels` argument: `codes` holds
	 * `sampleCapacity` elements of four bytes, `signs` of one. Element
	 * types are forced by the mangling of their two consumers --
	 * `ModulusDecoder::progress(unsigned char *, unsigned int *)` and
	 * `V90SignBitsExtractor::process(unsigned char *, unsigned char *)`.
	 *
	 * One entry per sample `hardDecision` has decided: `codes` the chosen
	 * constellation index ("code" is the author's own word for it, shared
	 * with `V90AutoDigitalImpDetector`), `signs` 1 for positive, 0 for
	 * negative. Both owned.
	 */
	unsigned int *codes;
	unsigned char *signs;

	/*
	 * +0x24  The element count both allocations were sized from, and the
	 * ceiling `hardDecision` refuses to write past.
	 */
	unsigned int sampleCapacity;

	/*
	 * +0x28  Where the frame `process` is working on starts.  Zero at
	 * construction, advanced by six per frame, and reset to zero when
	 * `process` compacts what is left down to the front.
	 */
	unsigned int frameStart;

	/*
	 * +0x2c  How many samples `codes` and `signs` hold.  `hardDecision`
	 * appends one and increments it; `process` consumes whole frames from
	 * `frameStart` and leaves the remainder here.
	 */
	unsigned int sampleCount;

	/*
	 * +0x30  The six constellations, up to 128 signed PCM levels each.
	 * `short` is forced by a sign-extending load in
	 * `printErrorHistogramAndReset`'s diagnostic; `updateConstelation`
	 * fills it via `fistps`.
	 *
	 * Two of its five users index it outside `[6][128]`, and both do so
	 * in the object rather than only in this reconstruction, so any
	 * re-shape of this array has to keep the same flat arithmetic:
	 * `updateConstelation` and `resetNoSpectral` form `i * 128 + j` with
	 * `j` bounded by twice the row length rather than 128, so a long
	 * enough row runs into the next one and the sixth runs past the array
	 * into `constellationSize` behind it; `linearMappingStudy` reads
	 * `[phase][code - 1]` with `code` at zero, two bytes below the row.
	 * `t_v90demap.cpp` reaches both on purpose.
	 */
	short constellation[V90DEMAPPER_CONSTELLATIONS][V90DEMAPPER_LEVELS];

	/*
	 * +0x630  How many levels each of the six constellations actually
	 * has. `printErrorHistogramAndReset` gates on all six being non-zero
	 * and uses them as its inner-loop bound, unsigned.
	 */
	unsigned int constellationSize[V90DEMAPPER_CONSTELLATIONS];

	/*
	 * +0x648 .. +0x663  An embedded `ModulusDecoder`, constructed in place
	 * and not destroyed by `~V90Demapper` (its destructor is trivial).
	 */
	ModulusDecoder modulusDecoder;

	/*
	 * +0x664  A `SerialDifferentialDecoder<unsigned char>`, one byte; the
	 * type is forced by a `this` passed to
	 * `SerialDifferentialDecoder<unsigned char>::process` in `process()`.
	 * Its declaration position between `modulusDecoder` and `signBits` is
	 * itself proven, not assumed: the constructor's value-initialising
	 * store to it sits between the two neighbours' constructor calls,
	 * which is where only a mem-initializer -- never a body statement --
	 * can land (finding F1302).
	 *
	 * What it is for: `process`'s other sign-bit path. With
	 * `signBitGroups` zero, no frame goes through `signBits` at all and
	 * every sign bit goes through this decoder instead.
	 */
	SerialDifferentialDecoder<unsigned char> signDecoder;
	/*
	 * +0x665 was `pad_665[3]` -- REMOVED (finding F10151): a 1-byte
	 * `signDecoder` ending at +0x665 leaves exactly 3 bytes of compiler
	 * alignment ahead of `signBits`, a `V90SignBitsExtractor` whose
	 * first member is a 4-byte `unsigned int` and needs 4-byte
	 * alignment.  Both ends already asserted in the .cpp
	 * (`DEM_OFF(signDecoder, 0x0664, signdec)`, `DEM_OFF(signBits,
	 * 0x0668, signbits)`), and `dis.py` over every `V90Demapper` method
	 * plus the `V90Equalizer`/`V90Phase4Demodulator` constructors that
	 * receive a `V90Demapper *` finds no access to 0x665/0x666/0x667.
	 */

	/*
	 * +0x668  The sign-bit extractor, embedded.  The constructor runs its
	 * constructor at `lea 0x668(%esi)` and the destructor ends with the
	 * matching `lea 0x668(%ebx)` and a call to
	 * `V90SignBitsExtractor::~V90SignBitsExtractor` -- which is what the
	 * compiler emits for us, in exactly that position, because it is the
	 * only member with a non-trivial destructor.
	 */
	V90SignBitsExtractor signBits;

	/*
	 * +0x690  Accumulated error per constellation level, and +0x1290 the
	 * number of samples that went into it (unsigned throughout, per the
	 * file comment). `hardDecision` fills both; `printErrorHistogramAndReset`
	 * divides one by the other to print, then empties both.
	 */
	unsigned int errorSum[V90DEMAPPER_CONSTELLATIONS][V90DEMAPPER_LEVELS];
	unsigned int errorCount[V90DEMAPPER_CONSTELLATIONS][V90DEMAPPER_LEVELS];

	/*
	 * +0x1e90  How many times the histogram has been asked for. Zeroed by
	 * the constructor, printed as "Error histogram #%d" before it is
	 * incremented, and incremented on every call whether anything was
	 * printed or not (F1171).
	 */
	unsigned int errorHistogramCount;

	/*
	 * +0x1e94  THE DELAY BEFORE THE HISTOGRAM STARTS FILLING, counted
	 * down.  `hardDecision` tests it and, while it is non-zero, decrements
	 * it and accumulates nothing; when it reaches zero every decision goes
	 * into the histogram.  The parameter block has the author's name for
	 * what seeds it -- `DEMAPPER_DELAY_BEFORE_ERROR_HISTOGRAM` at +0x530,
	 * beside the two this class does read -- and `reset` is what seeds it.
	 */
	int histogramDelay;

	/*
	 * +0x1e98  HOW LONG THE CURRENT HISTOGRAM HAS BEEN INTEGRATING.
	 * `hardDecision` compares it against
	 * `params->DEMAPPER_ERROR_HISTOGRAM_INTEGRATION_TIME` (+0x534) with a
	 * SIGNED `jge`, which is why both are `int`; on reaching it the
	 * histogram is printed, reset and this goes back to zero, otherwise it
	 * is incremented.
	 */
	int histogramIntegration;

	/*
	 * +0x1e9c  Modelled, unnamed. `resetLinearMappStudy` clears it with a
	 * 16-bit store; `linearMappingStudy` increments it once per completed
	 * run and compares the result against two (`movzwl 0x1e9c(%edx),%ebx;
	 * inc %ebx; cmp $0x2,%bx; mov %bx,0x1e9c(%edx); je`). The compare is
	 * an equality, so it carries no signedness evidence either.
	 *
	 * It is a count of completed study runs -- the increment is on the
	 * path where `uint_1eb0 + 1` reaches `uint_1ea8` -- and nothing in
	 * that function ever clears it; only `resetLinearMappStudy` and
	 * `reset` do. So the `== 2` test fires exactly once per study, on the
	 * second completed run, and what it does there is raise `short_1ea6`
	 * below.
	 */
	short short_1e9c;
	/*
	 * +0x1e9e was `pad_1e9e[2]` -- REMOVED (finding F10151): the same
	 * short-to-pointer-sized-field alignment gap as +0x665 above, ahead
	 * of `adiDetector` at +0x1ea0.  Both ends already asserted
	 * (`DEM_OFF(short_1e9c, 0x1e9c, short1e9c)`, `DEM_OFF(adiDetector,
	 * 0x1ea0, adi)`), and the same `dis.py` sweep finds no access to
	 * 0x1e9e/0x1e9f.
	 */

	/*
	 * +0x1ea0  The automatic digital-impairment detector, the
	 * constructor's fourth argument. Not owned. Reached through at
	 * +0x1000, +0x1c00 and +0x2800 by the study machinery below.
	 */
	V90AutoDigitalImpDetector *adiDetector;

	/*
	 * +0x1ea4 and +0x1ea6  Modelled, unnamed. Two claims that used to
	 * stand here are withdrawn: that nothing in the object loads either,
	 * and that the two stores at 0x315b6 and 0x3170f are "the two points
	 * where the end-of-run pass begins". Both are wrong.
	 *
	 * They ARE read, by `V90Equalizer::process`, and the identification is
	 * not a displacement coincidence -- the same function calls
	 * `linearMappingStudy` thirty bytes earlier:
	 *
	 *     3a3dd:  call  V90Demapper::linearMappingStudy(short, short)
	 *     ...
	 *     3a3fd:  mov   0x3054(%edx),%ecx
	 *     3a403:  cmpw  $0x0,0x1ea4(%ecx)     ; je  -> skip
	 *     ...
	 *     3a42c:  mov   0x3054(%edx),%eax
	 *     3a432:  cmpw  $0x0,0x1ea6(%eax)     ; je  -> skip
	 *
	 * so +0x3054 of the equaliser's argument is this demapper, each flag
	 * is tested against zero, and each gates a second `cmpw $0x0` on a
	 * flag at +0x144 / +0x146 of another object.  Finding F3531's rule is
	 * why this matters and finding F4342 records it: a claim that NOTHING
	 * reads a field is a claim about every function in the object, and
	 * the way to test it is a displacement grep -- which works here only
	 * because `1ea4` is a rare displacement and would prove nothing for,
	 * say, `0x08`.
	 *
	 * And the two stores are under different conditions: 0x315b6 sets
	 * +0x1ea4 on every completed run; 0x3170f sets +0x1ea6 only when
	 * `short_1e9c` reaches two, the second completed run, once per study.
	 * So one is "a run has finished" and the other "a second run has
	 * finished" -- bounded, but what the equaliser does with the
	 * distinction is in a function nobody has written, so the names stay
	 * neutral and this comment carries the derivation.
	 */
	short short_1ea4;
	short short_1ea6;

	/*
	 * +0x1ea8 and +0x1eb0  The linear-mapping study's length and its
	 * progress; the pair is what says which is which:
	 *
	 *     314e2:  mov  0x1eb0(%edx),%eax
	 *     314e8:  inc  %eax
	 *     314e9:  cmp  0x1ea8(%edx),%eax
	 *     314ef:  je   31578            -> the study's end-of-run pass
	 *     314f9:  mov  %eax,0x1eb0(%ebx)
	 *
	 * inside `linearMappingStudy`, against `resetLinearMappStudy` storing
	 * its one argument at +0x1ea8 and zeroing +0x1eb0. One is set once and
	 * only read; the other starts at zero, is incremented per call and is
	 * compared against the first. The comparison is an equality, so
	 * neither carries signedness evidence and both are `unsigned int` by
	 * the argument's type rather than by the branch.
	 *
	 * `linearMappingStudy`'s own behaviour confirms both names: the test
	 * is `uint_1eb0 + 1 == uint_1ea8` and the progress is stored only on
	 * the arm that fails it; the arm that passes rewinds it to zero and
	 * runs the end-of-run pass. A length that is set once and only read,
	 * against a progress that counts to it and restarts, is exactly what
	 * the pair was named for.
	 */
	unsigned int uint_1ea8;			/* the length  */

	/*
	 * +0x1eac and +0x1eae  The code the last `hardDecision` chose, and the
	 * RBS frame position it chose it in (the position BEFORE the advance,
	 * so it is recorded even on the over-capacity arm that decides
	 * nothing). Both written on every path, and both read by
	 * `linearMappingStudy` as the (phase, code) cell it accumulates the
	 * study's error into -- this pair is the hand-off from decision to
	 * study. A prior draft's "nothing reads this" claim was wrong; see
	 * F3531/F4342 for why that class of negative claim needs a
	 * whole-object displacement search rather than a look at the few
	 * functions in hand.
	 *
	 * Both signed: `decisionCode` decisively so, since `linearMappingStudy`
	 * sign-tests it and signed-compares `decisionCode + 1` against the row
	 * length, which an unsigned type promoted to `int` could never need --
	 * and the object pays for it, reading `constellation[phase][-1]` when
	 * code is 0 and the row length is 1.
	 */
	short decisionCode;
	short decisionFramePosition;

	unsigned int uint_1eb0;			/* the progress; see +0x1ea8 */

	/*
	 * +0x1eb4  The linear mapping study's enable flag. Three writers
	 * (`V90Demodulator::enterDataPhase`/`enterDataSteadyState`/`enterRRN`)
	 * store 1/0/0 beside format strings saying "enable"/"disable", and the
	 * reader (`V90Phase4Demodulator`'s decision members) gates
	 * `linearMappingStudy` on it being non-zero -- tier 1 and tier 3
	 * evidence agreeing (F4902). Stays a `short`, not a flag constant:
	 * every writer stores a whole halfword, none masks a bit.
	 */
	short linearMappStudyEnabled;
	/*
	 * +0x1eb6 was `pad_1eb6[2]`, the struct's LAST member -- REMOVED
	 * (finding F10151).  This one is trailing padding rather than a gap
	 * before a named field: `linearMappStudyEnabled` (0x1eb4, 2 bytes)
	 * ends at 0x1eb6, and the class's own alignment (forced to 4 by its
	 * many `int`/pointer members elsewhere) means the compiler rounds
	 * `sizeof` up to 0x1eb8 on its own with no member needed to name the
	 * gap. The existing size assertion
	 * (`typedef char v90dem_size[(sizeof(V90Demapper) == 0x1eb8) ? 1 :
	 * -1]`) is a hard compile-time proof of this, not just a spot check,
	 * and `dis.py` over every `V90Demapper` method plus the
	 * `V90Equalizer`/`V90Phase4Demodulator` constructors finds no access
	 * to 0x1eb6/0x1eb7.
	 */
};

#endif /* DSPLIB_V90DEMAPPER_H */

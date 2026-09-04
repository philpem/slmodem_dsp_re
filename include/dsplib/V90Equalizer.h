/*
 * V90Equalizer.h -- the V.90 receive equaliser, so far as three of its
 * twenty-seven members need it.
 *
 * Reconstructed from dsplibs.o.  `V90Equalizer` is NOT polymorphic --
 * tools/cppstruct.py lists its destructor with the `D1` and `D2` variants and
 * not the deleting `D0`, and GCC emits a deleting destructor only for a
 * virtual class -- so offset 0 is a real member and there is no vptr to shift every
 * field by four (finding F228).
 *
 * THE OBJECT IS 336 BYTES, AND THE 328 THIS HEADER USED TO CLAIM WAS A
 * DISPLACEMENT SCAN THAT COULD NOT SEE ITS OWN BLIND SPOT.  The largest
 * `this`-relative displacement across all twenty-nine V90Equalizer symbols in
 * the blob is +0x146, in `reset`:
 *
 *     3b58e:  66 89 bb 46 01 00 00    mov    %di,0x146(%ebx)
 *
 * a two-byte store, so those twenty-nine functions bound the object at
 * 0x148 = 328.  But `V90Demodulator::reset` writes FOUR BYTES at +0x148 of
 * the equaliser it holds at its own +0x1d8 -- a member of a different class,
 * which a scan restricted to `V90Equalizer` symbols cannot reach -- and the
 * allocation settles it outright:
 *
 *     1c584:  c7 04 24 50 01 00 00    movl   $0x150,(%esp)
 *     1c58b:  e8 ..                   call   sysdep_malloc
 *     1c590:  89 c6                   mov    %eax,%esi     <- the equaliser
 *
 * so sizeof is 0x150 = 336.  `test/harness/v90demfix.h` had already been
 * allocating 0x150 for its slot.  A displacement is not a size (finding F215,
 * and the V90Jd 0x8c -> 144 worked example in docs/v90cpp.md); a
 * displacement scan over one class is not a bound either.  Finding F1107.
 *
 * ONLY THE FIELDS THE WRITTEN METHODS TOUCH ARE NAMED.  Everything else is
 * `pad_*`, because a field this batch cannot see written is a field this
 * batch cannot claim (findings F223, F224 -- the harness fill makes untouched
 * memory compare equal on both sides, so a passing test says nothing about
 * where an untouched field lives).
 *
 * THE CONSTRUCTOR NAMED TWENTY-ONE MORE OF THEM (finding F1230).  It is the
 * one member that touches every allocation the object owns, so six argument
 * pointers, three fixed-size blocks and the twelve words of the six
 * raw/aligned/skew triples came out of it -- `pad_48`, `pad_98`, `pad_b4`,
 * `pad_dc`, `pad_11c` and most of `pad_f0` and `pad_130` are gone.  Nothing
 * else moved; the offsets the earlier batches asserted are unchanged and the
 * .cpp still asserts every one of them.
 *
 * THE MEMBER NAMES ARE THE AUTHOR'S; THE FIELD NAMES ARE NOT.  C++ mangling
 * preserves method names and signatures, so `setLinearEquBeta(float)` and
 * `enterPhase3()` are the original's own spelling.  Data members are not
 * mangled anywhere, so the names below are this reconstruction's, chosen from
 * what the instructions do with each slot; the derivation is given against
 * each one.
 */

#ifndef DSPLIB_V90EQUALIZER_H
#define DSPLIB_V90EQUALIZER_H

/*
 * BOTH ARE POINTERS HERE, so both are forward-declared and neither header is
 * included.  `V90Parameters` has two incompatible definitions in this tree
 * and no translation unit may include both (finding F1112); `V90Resampler.h`
 * pulls one of them in, and this header is included by `V90Demodulator.h`,
 * which pulls in the other.  The .cpp picks.
 */
class V90Parameters;
class V90Resampler;

/*
 * The constructor's other six object arguments, which it does nothing with
 * except store.  Forward declarations only, for the same reason: the class
 * holds a pointer to each and dereferences none of them.
 */
class V90Phase3Demodulator;
class V90Phase4Demodulator;
class V90Demapper;
class V90ConnectionEvaluator;
class V90SpectralVerifier;
class V90PreFilter;

/*
 * The constructor's last argument.  The name is the mangling's
 * (`20V90ComputationalMode`); the enumerators are not recoverable, so this is
 * an opaque enumeration with a fixed underlying type, the way
 * `V90PreFilter.h` spells `__tHardwareCodecTypes__`.  Only one of its values
 * is distinguished in this class:
 *
 *      3b767:  4f              dec    %edi
 *      3b768:  0f 84 c2 01 ..  je     3b930
 *
 * so the constructor asks "is the mode 1?" and nothing else.  What 1 is
 * called is not established here.
 *
 * SPELLED AS A DEFINITION BECAUSE C++98 HAS NO OPAQUE ENUM, and the author's
 * compiler was C++98 -- `enum X : int;` is C++11 and GCC 3.4.2 rejects it
 * outright, so it cannot be what was written here.  `_BASE_PIN` is OURS: the
 * object names no enumerator, and the pin's only job is to fix the underlying
 * type, which an empty enum does not -- it leaves the range 0..0 and the
 * casts this type exists for undefined.  A single negative enumerator makes
 * the base signed and every `int` representable, measured identical to the
 * C++11 spelling under both compilers.  docs/method/compilers.md, V2.
 *
 * THIS HEADER IS THE TYPE'S ONLY HOME.  It used to be declared here AND in
 * V90Modem.h, which an opaque declaration permits and a definition does not;
 * V90Modem.h now includes this file instead.  One definition, one place, and
 * the file's own include guard is the only guard needed.
 */
enum V90ComputationalMode { V90ComputationalMode_BASE_PIN = -0x7fffffff - 1 };

typedef char v90equ_compmode_is_signed[
    ((enum V90ComputationalMode)-1 < (enum V90ComputationalMode)0) ? 1 : -1];

#define V90EQU_COMP_MODE_1	1

/*
 * The values `state` (+0x60) takes.  Each of the `enter*` methods opens with
 * `cmpl $N,0x60(this); je <return>` and, having done its work, stores the
 * same N back -- so the field is a state and these are its values.  What the
 * state is CALLED in the original is still not established; `reset` stores 0.
 *
 * THREE IS NOW ACCOUNTED FOR, and the sentence that used to say "whether 0, 2
 * and 3 are used is not established here" is retracted for that one value.
 * `enterDataPhase` opens `cmpl $0x3,0x60(%ebx); je` at 0x37d4b and stores
 * `movl $0x3,0x60(%ebx)` at 0x37d61, exactly the family's shape, so the name
 * below is the SYMBOL's and not an inference.  Only 0 is still unwitnessed
 * outside `reset`.
 */
#define V90EQU_STATE_RESET		0	/* V90Equalizer::reset       */
#define V90EQU_STATE_PHASE3		1	/* enterPhase3()             */
#define V90EQU_STATE_PHASE4		2	/* enterPhase4()             */
#define V90EQU_STATE_DATA		3	/* enterDataPhase()          */
#define V90EQU_STATE_RRN		4	/* enterRRN()                */
#define V90EQU_STATE_FPE		5	/* enterFPE()                */
#define V90EQU_STATE_CHANNEL_VERIFY	6	/* enterChannelVerification()*/

/*
 * The length of the mean-error buffer at +0x98, in floats.  It is the 0x12c
 * `calcMeanErrorStatistics` uses when the buffer has wrapped, and
 * 300 * sizeof(float) is the 0x4b0 the constructor allocates -- the two
 * readings agree, which is what makes this a length and not a magic number.
 */
#define V90EQU_MEAN_ERROR_LEN		300

class V90Equalizer {
public:
	/*
	 * The three members this batch defines.  Their signatures are the
	 * mangling's, which makes them a specification and not a guess:
	 * _ZN12V90Equalizer16setLinearEquBetaEf,
	 * _ZN12V90Equalizer10setDfeBetaEf and
	 * _ZN12V90Equalizer11enterPhase3Ev.  A return type is not mangled;
	 * all three fall off the end without setting %eax, so all three are
	 * void.
	 */
	void setLinearEquBeta(float beta);
	void setDfeBeta(float beta);
	void enterPhase3();

	/*
	 * Added by task #88, the lifecycle batch.  `reset(unsigned)` is
	 * `_ZN12V90Equalizer5resetEj` and `enterChannelVerification()` is
	 * `_ZN12V90Equalizer24enterChannelVerificationEv`; both fall off the
	 * end without setting %eax, so both are void.  The argument to `reset`
	 * is `unsigned` from the mangling's `j`, and the object treats it as
	 * one: `cmp 0x14(%esp),%eax / jae` is the unsigned comparison.
	 */
	void reset(unsigned int cursor);
	void enterChannelVerification();

	/*
	 * The coefficient batch.  Every signature below is the mangling's:
	 * `_ZN12V90Equalizer17setLinearEquCoeffEPfj` is `(float *, unsigned)`,
	 * `_ZN12V90Equalizer29setLinearEquEdgesFadingParamsEff` is
	 * `(float, float)`, and `_ZNK12V90Equalizer16printCoefsToFileEv`
	 * carries the `K` that makes it `const`.  Return types are not
	 * mangled, so each one is read off the body: all of these fall off
	 * the end without setting %eax and are void, EXCEPT `getDfeBeta`,
	 * whose whole body is `flds 0x3c(%eax); ret` -- a float in st(0),
	 * which is the return value.
	 */
	/*
	 * The other two state entries.  These two return an `int` where every
	 * other `enter*` in the class returns void: %edi is zeroed at entry,
	 * set to 1 on the one path that takes the equaliser out of
	 * fixed-point mode, and moved to %eax at both returns.  Finding F2134.
	 */
	int enterRRN();
	int enterFPE();

	/*
	 * `_ZN12V90Equalizer14enterDataPhaseEv`, and an `int` for the same
	 * reason those two are (finding F2134): %esi is zeroed at entry, set to
	 * 1 on the one path where `convertEqualizerToMmx` leaves the equaliser
	 * in fixed-point mode, and moved to %eax at the single return.
	 */
	int enterDataPhase();

	/*
	 * `_ZN12V90Equalizer11enterPhase4Ev`, and void: it falls off the end
	 * without setting %eax, so it is the `enter*` family's usual shape and
	 * not `enterRRN`'s.
	 */
	void enterPhase4();

	/*
	 * The mean-error diagnostic.  It returns a float -- `flds 0x20(%esp);
	 * ret` off a stack slot `Std<float>` wrote -- and on its early exit
	 * that slot has not been written at all; see the .cpp and D324.
	 */
	float calcMeanErrorStatistics();

	/*
	 * The hub.  `_ZN12V90Equalizer7processEPfjPsS0_Rj` is
	 * `(float *, unsigned int, short *, float *, unsigned int &)` --
	 * every argument type is the mangling's, including the REFERENCE on
	 * the last one, which `Rj` spells and which no other reading of the
	 * object would have given.  Void: all three returns fall off the end
	 * without setting %eax.  docs/v90equprocess.md.
	 */
	void process(float *in, unsigned int n, short *outSym,
		     float *outFloat, unsigned int &nOut);

	float getDfeBeta();
	void setLinearEquCoeff(float *src, unsigned int n);
	void setDfeCoeff(float *src, unsigned int n);
	void zeroLinearEquCoefs();
	void zeroDfeCoefs();
	void resetMeanErrorEnergyDiagnostics();
	void setLinearEquEdgesFadingParams(float left, float right);
	void freeze();
	void restoreEqualizerToFloat();
	void linearEquFadeEdges();

	/*
	 * `_ZN12V90Equalizer21convertEqualizerToMmxEv`, and void: both exits
	 * fall off the end without setting %eax.  The other half of
	 * `restoreEqualizerToFloat`, and the class's own name for the state
	 * `mmxMode` selects.
	 */
	void convertEqualizerToMmx();

	/*
	 * THREE MEMBERS THAT ARE ONE `ret` EACH.  Not stubs and not missing:
	 * the object's copies are a single byte at 0x36b10, 0x36960 and
	 * 0x388b0, so whatever they did was compiled out -- the names say
	 * file I/O and a debug dump, which is what a shipping build drops.
	 * They are written empty because an empty body is what the object
	 * has, and they are still tested: a body that touched the object
	 * would show.
	 */
	void printCoefsToFile() const;
	void loadCoefsFromFile();
	void printEquStuff();

	/*
	 * The lifecycle pair.  The constructor's signature is the mangling's,
	 * argument for argument:
	 *
	 *   _ZN12V90EqualizerC1EjjP20V90Phase3DemodulatorP20V90Phase4Demodul\
	 *   atorP11V90DemapperP22V90ConnectionEvaluatorP19V90SpectralVerifie\
	 *   rP13V90ParametersP12V90ResamplerP12V90PreFilter20V90Computationa\
	 *   lMode
	 *
	 * and the two `j`s are unsigned because the mangling says so; the
	 * object treats both the same way, masking each to a multiple of four
	 * with `shr $2` followed by a scale, which is the LOGICAL shift.
	 *
	 * ALL ELEVEN ARGUMENTS ARE ACCOUNTED FOR AND SEVEN OF THEM NAME A
	 * FIELD.  The eighth (`V90Parameters *`) lands at +0xa8 and the ninth
	 * (`V90Resampler *`) at +0x00 -- two slots this header had already
	 * typed from `reset` and `enterChannelVerification`, which are other
	 * functions entirely.  That agreement is what makes the argument-to-
	 * offset mapping evidence rather than an ordering guess, and it is
	 * what licenses naming +0x48 .. +0x5c from the argument types.
	 */
	V90Equalizer(unsigned int linearEquLen, unsigned int dfeLen,
		     V90Phase3Demodulator *p3d, V90Phase4Demodulator *p4d,
		     V90Demapper *dem, V90ConnectionEvaluator *ce,
		     V90SpectralVerifier *sv, V90Parameters *parms,
		     V90Resampler *rs, V90PreFilter *pf,
		     V90ComputationalMode mode);
	~V90Equalizer();

	/*
	 * Data members are public for the reason V90Jd.h gives: the original's
	 * access specifiers are not recoverable (finding F226), and a single
	 * access section is what keeps the class POD, so the .cpp can assert
	 * every offset below with __builtin_offsetof and the test can put the
	 * object in a union with a byte array.
	 */

	/*
	 * +0x00  The resampler the equaliser steers.
	 * `enterChannelVerification` loads it with `mov (%esi),%eax` and hands
	 * it straight to `V90Resampler::setBllState`, which is what types it.
	 * Not owned: `V90Demodulator` holds the object itself at its own
	 * +0x94 and passes the address to this class's constructor.
	 */
	V90Resampler *resampler;	/* +0x00 */

	/*
	 * +0x04  THE RESAMPLER'S BLL STATE, SAVED ACROSS THE PHASE 4 FREEZE.
	 * `enterPhase4` reads `resampler->bllState` (+0x94 of that object) and
	 * stores it here in the instruction before it calls
	 * `setBllState(V90_BLL_FROZEN, 1)`; nothing else in the class touches
	 * the slot, so what restores it is in `process` or in a member this
	 * batch has not written.  It was `pad_04`.
	 *
	 * Spelled `int` and not `V90BllState`: this header deliberately does
	 * not include `V90Resampler.h` (see the note above the two forward
	 * declarations), and the object moves the word with a plain 32-bit
	 * `mov` either way.
	 */
	int savedBllState;		/* +0x04 */

	/*
	 * +0x08  Zeroed by `reset` with a `movw`, so two bytes and not four.
	 *
	 * NAMED FROM THE FORMAT STRING THAT PRINTS IT.  `process`'s DIL
	 * ("digital in-line", the German-PBX arm) state 10 prints
	 * `"V90Equalizer: DfeProtectionOnDil = %d \r\n"` off exactly this
	 * slot before using it as a divisor for `DFE_DIL_HIGH_UCODE_BETA`.
	 * Class-1 evidence.  Nothing in this class writes it; the one
	 * external writer is `V90Demodulator::progress`, which computes it
	 * from how far the AGC gain has fallen below a parameter threshold
	 * (`equalizer->dfeProtectionOnDil = (short)((1.0f - agc.gain /
	 * threshold) * 250.0f) + 1`).
	 */
	short dfeProtectionOnDil;	/* +0x08 */

	/*
	 * +0x0a WAS `pad_0a[2]`, REMOVED (2026-09-04, pad-audit).  A `short`
	 * ending at +0x0a followed by the `int` below needs exactly this
	 * 2-byte gap for natural alignment, no dis.py reader/writer touches
	 * +0x0a/+0x0b anywhere in the object, and `V90EQU_OFF(linearEquLength,
	 * 0x00c, ...)` below already asserts the next field's offset -- so the
	 * compiler's own padding reproduces the member being deleted.
	 */

	/*
	 * +0x0c  The linear equaliser's tap count.  `reset` clears
	 * `linearEquCoefs[0 .. this - 1]`, uses it as the ceiling the cursor
	 * is clamped to, converts it to floating point with the
	 * `push 0 / push it / fildll` zero-extending idiom -- so UNSIGNED --
	 * and `V90Demodulator::reset` halves it with `shr $1`, a logical
	 * shift, which says the same thing a second time.
	 */
	unsigned int linearEquLength;	/* +0x0c */

	/*
	 * The linear equaliser's LMS step size.  `setLinearEquBeta` compares
	 * its argument against this slot and then stores it there; `reset`
	 * plants 0x283424dc in it immediately before calling the setter with
	 * zero, which is what makes the setter's inequality fire.
	 */
	float linearEquBeta;		/* +0x10 */

	/*
	 * +0x14  The linear equaliser's coefficients, `linearEquLength` of
	 * them.  `reset` zeroes the whole array and then plants 1.0f at the
	 * cursor -- `movl $0x3f800000,(%ecx,%edx,4)` -- which is what makes
	 * this the coefficient vector and not a history.
	 */
	float *linearEquCoefs;		/* +0x14 */

	/*
	 * +0x18  Cleared in the SAME loop as `linearEquCoefs` and from the
	 * far end: the store is `movl $0x0,-0x4(%esi,%eax,4)` with
	 * `%eax = linearEquHistoryLength - i`, so index
	 * `linearEquHistoryLength - 1 - i` runs downwards while the first
	 * array runs up.  Two arrays walked in opposite directions by one
	 * counter; what the second holds is not established here, so it is
	 * offset-named.
	 *
	 * `process` IS WHAT SAYS WHAT IT IS: this is the linear equaliser's
	 * sample delay line, read at `array_18[historyIndex + i]` as the FIR
	 * sum's history and shifted down by `linearEquLength` samples every
	 * time `historyIndex` wraps.
	 */
	float *array_18;		/* +0x18  the linear delay line     */

	/*
	 * +0x1c  The bound the descending clear counts down from, and the
	 * length (plus eight) of the fixed-point array at +0xec.  Unsigned:
	 * `lea 0x8(%ebp),%eax / cmp %edx,%eax / ja` is the unsigned form.
	 *
	 * NAMED FROM ITS SOURCE.  The constructor computes it as
	 * `2 * (unsigned)(params->LINEAR_EQU_HISTORY_LENGTH / 2)` -- the
	 * parameter's own name, rounded down to an even count -- and this is
	 * the only field that value reaches.  Class-2/3 evidence (a named
	 * parameter, read over the whole constructor).
	 */
	unsigned int linearEquHistoryLength;	/* +0x1c */

	/*
	 * +0x20  Derived, not copied: `reset` computes
	 * `linearEquHistoryLength - linearEquLength - 1` and stores it here
	 * BEFORE the guard that skips the clearing loops, so it is written
	 * even when the equaliser has no taps.
	 *
	 * SIGNED, AND `process` IS WHAT SETTLES IT.  The cursor retreats by
	 * two per symbol and the second step is `dec %eax; js 3a2d4` at
	 * 0x3985f -- a test of the SIGN, which is the whole wrap condition.
	 * Written against an `unsigned` field that test folds to false and the
	 * wrap never runs, so the only two readings are an `int` field or a
	 * cast at the test; a cast is a claim that the declaration is wrong
	 * (docs/cleanup.md §3a) and the declaration is what this batch owns.
	 * `reset`'s `linearEquHistoryLength - linearEquLength - 1` still
	 * computes in unsigned and converts, which is why nothing else moved.
	 * Finding F6200.
	 *
	 * NAMED FROM READING `process` AS ONE ALGORITHM (usage inference,
	 * class-4): it is the write position into `array_18` (and, in fixed-
	 * point mode, `array_ecAligned`) that the linear FIR sum reads
	 * forward from, retreating by two samples per symbol and wrapping
	 * back to the value above when it goes negative -- a delay-line
	 * index, not a cursor in `reset`'s sense (that parameter seeds
	 * `linearEquCoefs`, a different array entirely).
	 */
	int historyIndex;		/* +0x20 */

	/*
	 * +0x24, +0x28  The two windows.  `reset` ends by calling
	 * `hamming<float>` on each with twice the matching length below, and
	 * the second call is a TAIL call -- the object's last act.
	 */
	float *linearEquWindow;		/* +0x24 */
	float *dfeWindow;		/* +0x28 */

	/*
	 * +0x2c, +0x30  Half the length of the window above each.  Both come
	 * out of `(unsigned)(ratio * (float)linearEquLength)` with the ratio
	 * clamped to [0, 0.5] -- the `fistpll` low word, which is the
	 * float-to-unsigned conversion and not a float-to-long-long one.
	 */
	unsigned int linearEquWindowHalf;	/* +0x2c */
	unsigned int dfeWindowHalf;		/* +0x30 */

	/*
	 * +0x34  Zeroed by `reset`.  `process` increments it once per call
	 * and compares it against `params->LINEAR_EQU_FADE_EDGES_CYCLE`,
	 * calling `linearEquFadeEdges()` and resetting to zero when it hits
	 * that count -- a call-count divider, named from reading `process`
	 * as one algorithm (usage inference).
	 */
	unsigned int fadeEdgesCounter;	/* +0x34 zeroed by reset          */

	/*
	 * +0x38  The decision-feedback filter's tap count, the same role
	 * `linearEquLength` plays for the linear half: it bounds the clear of
	 * +0x40 and +0x44 and, plus eight, the clear of +0x114, +0x118 and
	 * +0x12c.
	 */
	unsigned int dfeLength;		/* +0x38 */

	/*
	 * The decision-feedback filter's step size, the same way round:
	 * `setDfeBeta` stores here, and `getDfeBeta` is nothing but
	 * `flds 0x3c(%eax); ret`.
	 */
	float dfeBeta;			/* +0x3c */

	/* +0x40, +0x44  Two `dfeLength`-long float arrays, both zeroed. */
	float *dfeCoefs;		/* +0x40 */
	float *array_44;		/* +0x44 */

	/*
	 * +0x48 .. +0x5c  THE CONSTRUCTOR'S THIRD TO SEVENTH AND TENTH
	 * ARGUMENTS, in the order it is given them and stored nowhere else.
	 * Nothing in the class dereferences any of the six; they are held for
	 * members this batch does not write.  Not owned -- the destructor
	 * frees fifteen pointers and none of these is among them.
	 */
	V90Phase3Demodulator *phase3Demod;	/* +0x48 */
	V90Phase4Demodulator *phase4Demod;	/* +0x4c */
	V90Demapper *demapper;			/* +0x50 */
	V90ConnectionEvaluator *connEval;	/* +0x54 */
	V90SpectralVerifier *spectralVerifier;	/* +0x58 */
	V90PreFilter *preFilter;		/* +0x5c */

	int state;			/* +0x60 see V90EQU_STATE_* above */

	/*
	 * Zeroed by `enterPhase3`, by `enterChannelVerification` and by
	 * `reset`, always in the instruction after `state` is written.  What
	 * it counts is not established by anything in this batch.
	 */
	int stateCount;			/* +0x64 */

	/*
	 * +0x68 .. +0xa4  Eighteen consecutive slots `reset` writes, sixteen
	 * of them with zero and two from the parameter block.  Only the two
	 * copies say anything about what they hold, so those two carry the
	 * parameter's name and the rest are offset-named.  +0x98 is the one
	 * slot in the run `reset` does not touch.
	 */
	/*
	 * +0x68, +0x6c  THE HELD-OVER ODD SAMPLE AND ITS VALUE, and `process`
	 * is what says so: the epilogue sets the flag exactly when `n` came
	 * out odd and stores the sample the loop could not pair, and the
	 * prologue consumes both.  The VALUE is a `float`, because the float
	 * arm assigns it straight into `array_18[]` and the epilogue fills it
	 * from `*in` -- a raw 32-bit `mov` either way, which is exactly what
	 * GCC emits for a float copy that does no arithmetic.  `reset` writes
	 * zero, and a store of zero cannot tell an int from a float (the
	 * +0x80..+0x8c argument again).
	 *
	 * NOW NAMED, from the same reading -- usage inference over the whole
	 * of `process`'s prologue and epilogue, which is unambiguous even
	 * without a format string: this is the two-samples-per-symbol
	 * pairing's carry between calls.
	 */
	unsigned int holdoverPending;	/* +0x68 */
	float holdoverSample;		/* +0x6c */

	/*
	 * +0x70  Zeroed by `reset`, incremented by `nOut` at the end of every
	 * `process` call, and compared against `errorEnergyMeanBlockLen`;
	 * once it reaches that count the block's r.m.s. error is computed
	 * from +0x78 and both counters reset to zero.  A sample count within
	 * the current error-energy block (usage inference, class-4).
	 */
	unsigned int blockSampleCount;	/* +0x70 */

	/* +0x74  = params->ERROR_ENERGY_MEAN_BLOCK_LEN */
	int errorEnergyMeanBlockLen;	/* +0x74 */

	/*
	 * +0x78  The block's accumulated squared error, read UNSIGNED: the
	 * epilogue converts it with `push $0; push it; fildll`, the
	 * zero-extending idiom.
	 *
	 * +0x7c  The block's root-mean-square error, and a FLOAT: the object
	 * writes it with `fsts 0x7c(%ebp)` -- a four-byte x87 store, not an
	 * integer move -- and reads it back to append to `meanErrorEnergy[]`.
	 * `reset` writes zero, which is the same word either way.
	 *
	 * NOW NAMED, matching `blockSampleCount` above and the roles the
	 * surrounding comments already established (usage inference).
	 */
	unsigned int blockErrorEnergySum;	/* +0x78 */
	float blockErrorEnergyRms;		/* +0x7c */

	/*
	 * +0x80 .. +0x8c  FOUR FLOATS, AND THE AUTHOR'S OWN NAMES FOR THEM.
	 * `calcMeanErrorStatistics` writes +0x84 with `fstps` from
	 * `mean<float>` and tracks a running maximum in +0x8c and minimum in
	 * +0x88, then prints all four through format strings that name them:
	 * "meanErrorEnergy mean", "current meanErrorEnergy", "meanErrorEnergy
	 * min value" and "meanErrorEnergy max value".  +0x80 is printed and
	 * never written there, so `process` maintains it.
	 *
	 * They were `unsigned int word_8*` and `reset` sets all four to zero,
	 * which is the same word either way -- a store of zero cannot tell an
	 * int from a float, which is why the type only arrived with a member
	 * that does arithmetic on them.
	 */
	float meanErrorEnergyCurrent;	/* +0x80 */
	float meanErrorEnergyMean;	/* +0x84 */
	float meanErrorEnergyMin;	/* +0x88 */
	float meanErrorEnergyMax;	/* +0x8c */

	/*
	 * +0x90  = params->ERROR_ENERGY_MEAN_K.  Copied as a 32-bit word and
	 * never loaded into the x87 stack, so it is moved as a float and not
	 * converted -- the same shape as V90Phase3Demodulator's +0x418.
	 */
	float errorEnergyMeanK;		/* +0x90 */

	/*
	 * +0x94  A high-error burst counter, zeroed by `reset`.  `process`
	 * increments it (and zeroes `updateCoefs`, suspending adaptation)
	 * whenever a symbol's error exceeds 300 in a state past `RESET`; once
	 * it is above 2, coefficient updates stay suspended until FOUR clean
	 * symbols in a row bring it back to zero.  Usage inference over the
	 * whole of `process`'s error-handling arm (class-4); no format string
	 * or callee names the quantity.
	 */
	unsigned int highErrorCount;	/* +0x94 */

	/*
	 * +0x98  THREE HUNDRED FLOATS, AND THE ONLY BLOCK THE CONSTRUCTOR
	 * TAKES UNCONDITIONALLY.  `movl $0x4b0,(%esp); call sysdep_malloc` is
	 * the last thing it does before handing over to `reset`, and the
	 * destructor frees it under a null test like all the others.  0x4b0
	 * is 1,200 bytes, and `calcMeanErrorStatistics` hands the same pointer
	 * to `mean<float>`, `Std<float>` and `Var<float>` and indexes it with
	 * `flds (%edx,%ecx,4)` -- so 300 floats, and the length constant the
	 * function uses is 0x12c = 300.  It was `void *block_98` and before
	 * that `pad_98`.
	 *
	 * +0x9c, +0xa0  The two words `resetMeanErrorEnergyDiagnostics` zeroes
	 * and nothing else in the class touches.  `calcMeanErrorStatistics`
	 * reads both: it returns early when BOTH are zero, uses the whole 300
	 * when +0xa0 is set, and otherwise uses +0x9c as the count -- so +0x9c
	 * is how many of the 300 are filled and +0xa0 says the buffer has
	 * wrapped.  Named for what the length arithmetic does with them; what
	 * `process` calls them is not established.
	 */
	float *meanErrorEnergy;		/* +0x98  300 floats */
	unsigned int meanErrorCount;	/* +0x9c */
	unsigned int meanErrorFull;	/* +0xa0 */

	/*
	 * +0xa4  A gate on whether `process` appends the block r.m.s. error
	 * (+0x7c) to `meanErrorEnergy[]`.  `process`'s PHASE3 arm sets it once
	 * a fixed count of clean symbols has passed in state 4 and clears it
	 * on states 10 and 13; the block-boundary code at the epilogue only
	 * writes the buffer when it is set.  Usage inference (class-4).
	 */
	unsigned int meanErrorRecordEnable;	/* +0xa4 */

	/*
	 * +0xa8  The parameter block.  Not owned; the constructor is handed
	 * it as its seventh argument.
	 */
	V90Parameters *params;		/* +0xa8 */

	/*
	 * +0xac  Non-zero enables the three fixed-point clearing loops in
	 * `reset` -- the arrays at +0xd4, +0xd8, +0xec, +0x114, +0x118 and
	 * +0x12c.  It is NOT `mmxMode`: `reset` zeroes `mmxMode` a hundred
	 * instructions before it tests this one, so the two are separate
	 * fields with separate lives.  What sets it is not in this batch.
	 */
	int mmxArraysPresent;		/* +0xac */

	/*
	 * Non-zero selects the fixed-point coefficient representation.  Both
	 * setters skip their whole second half when it is zero; `reset` sets
	 * it to zero; `process`, `enterFPE`, `enterRRN`, `zeroDfeCoefs`,
	 * `zeroLinearEquCoefs` and `convertEqualizerToMmx` all branch on it.
	 * Named for `convertEqualizerToMmx` and `restoreEqualizerToFloat`,
	 * which are the class's own names for its two states.  A full 32-bit
	 * word: `mov 0xb0(%ebx),%edx; test %edx,%edx`.
	 */
	int mmxMode;			/* +0xb0 */

	/*
	 * +0xb4, +0xb8  Two fixed-size blocks the constructor takes only when
	 * `mmxArraysPresent` came out set, 0x400 and 0x200 bytes, allocated in
	 * that order (+0xb8 first).  The destructor frees both under the same
	 * flag.  Neither has a size that depends on any length in the object,
	 * and nothing this batch writes reads either of them.
	 */
	void *block_b4;			/* +0xb4  0x400 bytes */
	void *block_b8;			/* +0xb8  0x200 bytes */

	/*
	 * The four slots the linear equaliser's fixed-point step size is built
	 * from.  `setLinearEquBeta` reads the first two and writes the last
	 * two; `convertEqualizerToMmx` writes the same last two from the same
	 * arithmetic over a magnitude it has just measured, which is what says
	 * +0xbc holds a magnitude and not a step size.  `reset` zeroes +0xbc.
	 *
	 * +0xbc AND +0xc0 HAVE THE AUTHOR'S OWN NAMES, and `enterPhase4` is
	 * where they came from: it walks `linearEquCoefs`, tracks the largest
	 * and smallest `fabs` in these two slots, and prints them under
	 * "maxLeCoefValue  = %c%d.%06d" and "minLeCoefValue  = %c%d.%010d".
	 * They were `linearEquMmxRefLevel` and `word_c0`, which said what
	 * `setLinearEquBeta` does with +0xbc and nothing at all about +0xc0.
	 * Finding F2137.  BOTH ARE FLOATS; `reset` writes zero to each, which
	 * is the same word either way.
	 */
	float maxLeCoefValue;		/* +0xbc */
	float minLeCoefValue;		/* +0xc0 zeroed by reset          */

	/*
	 * +0xc4 AND +0xc8 ALSO HAVE THE AUTHOR'S OWN NAMES, and
	 * `convertEqualizerToMmx` is where they came from: it computes both,
	 * and prints each under a format string that names it --
	 * "linearEquMmxConversionFactor = %c%d.%05de8" reads +0xc4 and
	 * "linearEquMmxOutputConversionFactor = %c%d.%03d" reads +0xc8.
	 * +0xc4 was `linearEquMmxBetaScale`, which said what
	 * `setLinearEquBeta` does with the slot and nothing about what it is;
	 * +0xc8 was `pad_c8`, and it is a real field.  Finding F2145.
	 *
	 * The pair is one scaling and its inverse: +0xc4 is
	 * `2**30 / maxLeCoefValue`, which takes a float coefficient into the
	 * 32-bit fixed-point word, and +0xc8 is `2**14 / maxLeCoefValue` --
	 * the same factor times 2**-16 -- which is what takes the HIGH HALF
	 * of that word back out.  It is an `int` and not a float: the object
	 * writes it with `fistpl` and reads it back with `fildl`.
	 */
	float linearEquMmxConversionFactor;		/* +0xc4 */
	int linearEquMmxOutputConversionFactor;		/* +0xc8 */

	int linearEquMmxBeta;		/* +0xcc */
	int linearEquMmxShift;		/* +0xd0 */

	/*
	 * +0xd4, +0xd8  The linear half's fixed-point coefficient pair,
	 * `linearEquLength + 8` SHORTS each -- `movw $0x0,(%ecx,%eax,2)`, a
	 * two-byte store with a two-byte stride, which is what says 16-bit.
	 * Cleared only when `mmxArraysPresent` is set.
	 */
	/*
	 * EACH FIXED-POINT ARRAY IS THREE SLOTS, NOT ONE.  The constructor
	 * follows every one of the six `sysdep_malloc`s below with
	 *
	 *      lea    0x7(%p),%d ; and $0xfffffff8,%d ; sub %p,%d ; shr $1,%d
	 *      lea    (%p,%d,2),%a
	 *
	 * which is `skew = (align8(p) - p) / 2` and `aligned = p + 2 * skew`:
	 * the raw pointer kept for `sysdep_free`, the count of SHORTS that
	 * have to be stepped over to reach an eight-byte boundary, and the
	 * aligned pointer itself.  The division by two is what says the arrays
	 * are 16-bit -- the same thing `reset`'s `movw` stride says -- and the
	 * eight-byte target is what an MMX load wants.  Only the raw pointer
	 * is freed; the other two are interior and are never passed anywhere.
	 */
	short *linearEquMmxCoefs;	/* +0xd4 raw    */
	short *array_d8;		/* +0xd8 raw    */

	short *linearEquMmxCoefsAligned;	/* +0xdc */
	short *array_d8Aligned;			/* +0xe0 */
	unsigned int linearEquMmxCoefsSkew;	/* +0xe4 */
	unsigned int array_d8Skew;		/* +0xe8 */

	/*
	 * +0xec  `linearEquHistoryLength + 8` shorts, cleared under the same
	 * condition -- the fixed-point mode's own delay line, the counterpart
	 * of `array_18`.
	 */
	short *array_ec;		/* +0xec raw    */
	short *array_ecAligned;		/* +0xf0 */
	unsigned int array_ecSkew;	/* +0xf4 */

	/*
	 * +0xf8  WHERE `historyIndex` IS PARKED WHILE THE EQUALISER IS IN ITS
	 * FIXED-POINT MODE.  `convertEqualizerToMmx` copies +0x20 here
	 * (`mov 0x20(%ebp),%ebx; mov %ebx,0xf8(%ebp)`) and
	 * `restoreEqualizerToFloat` copies it straight back
	 * (`mov 0xf8(%ebx),%ecx; mov %ecx,0x20(%ebx)`); `process` reads and
	 * writes it in the fixed-point arms.  It used to be `pad_f8`.
	 */
	/* SIGNED, for the reason `historyIndex` above is: 0x39457 is the same
	 * `dec %eax; js` on this slot. */
	int historyIndexSaved;		/* +0xf8 */

	/*
	 * The same four for the decision-feedback filter, +0x40 further on --
	 * and the same two names, from "maxDfeCoefValue" and "minDfeCoefValue"
	 * in `enterPhase4`'s second half.
	 */
	float maxDfeCoefValue;		/* +0xfc */
	float minDfeCoefValue;		/* +0x100 zeroed by reset         */
	/*
	 * The same pair as +0xc4 and +0xc8, named by the same two prints --
	 * "dfeMmxConversionFactor = %c%d.%05de8" and
	 * "dfeMmxOutputConversionFactor = %c%d.%03d" -- and built from
	 * `maxDfeCoefValue` the way the linear half's are built from
	 * `maxLeCoefValue`.  +0x108 was `pad_108`.  Finding F2145.
	 */
	float dfeMmxConversionFactor;			/* +0x104 */
	int dfeMmxOutputConversionFactor;		/* +0x108 */

	int dfeMmxBeta;			/* +0x10c */
	int dfeMmxShift;		/* +0x110 */

	/*
	 * +0x114, +0x118, +0x12c  The decision-feedback half's fixed-point
	 * arrays, `dfeLength + 8` shorts each, all three cleared in one loop.
	 */
	short *dfeMmxCoefs;		/* +0x114 raw   */
	short *array_118;		/* +0x118 raw   */

	/* The same three-slot shape as +0xd4; see the note there. */
	short *dfeMmxCoefsAligned;	/* +0x11c */
	short *array_118Aligned;	/* +0x120 */
	unsigned int dfeMmxCoefsSkew;	/* +0x124 */
	unsigned int array_118Skew;	/* +0x128 */

	short *array_12c;		/* +0x12c raw   */
	short *array_12cAligned;	/* +0x130 */
	unsigned int array_12cSkew;	/* +0x134 */

	unsigned char pad_138[0x4];	/* +0x138 */

	/*
	 * +0x13c, +0x140  THE PHASE 4 MEAN-ERROR BEFORE/AFTER PAIR, and BOTH
	 * ARE FLOATS.  `process` copies `meanErrorEnergyMean` into +0x13c at
	 * 0x3aa09, before the statistics are recomputed, and divides the two
	 * into +0x140 at 0x3a485 afterwards -- a float divide, so both slots
	 * are floats and `reset`'s store of zero says nothing either way.
	 *
	 * +0x140 CARRIES THE AUTHOR'S OWN NAME.  The format string at
	 * .rodata 0x9540 prints exactly this slot as
	 * "ph4MeanErrorEnergyBeforeToAfterUpdateRatio", which is class-1
	 * evidence under CLAUDE.md's ordering.  +0x13c is the "Before" of
	 * that ratio and is named from the same string plus the arithmetic
	 * that feeds it; nothing prints it directly.
	 */
	float ph4MeanErrorEnergyBeforeUpdate;			/* +0x13c */
	float ph4MeanErrorEnergyBeforeToAfterUpdateRatio;	/* +0x140 */

	/*
	 * +0x144, +0x146  Two 16-bit flags `reset` sets to 1, not 0 -- the
	 * only two non-zero constants it plants that are not read out of the
	 * parameter block.  `mov %bp,0x144` and `mov %di,0x146`.
	 */
	short flag_144;			/* +0x144 */
	short flag_146;			/* +0x146 */

	/*
	 * +0x148  NOT WRITTEN BY ANY V90Equalizer MEMBER.  The only thing in
	 * the object that touches it is `V90Demodulator::reset`, which stores
	 * its own `quickConnect` argument here through the equaliser pointer
	 * it holds at +0x1d8.  It is the reason this class is 0x150 and not
	 * 0x148; see the file comment.
	 */
	unsigned int quickConnect;	/* +0x148 */

	/*
	 * +0x14c  NOT PADDING.  `process`'s PHASE3 arm reads it and hands it
	 * straight to `ResamplerTimingOffset::setTimingOffset(float)` at
	 * 0x3b1cb, which is what types it -- a callee's mangled signature,
	 * class-2 evidence.  It is the last four bytes of the 0x150 object.
	 * What WRITES it is not in this class.
	 */
	float timingOffset;		/* +0x14c */
};

#endif /* DSPLIB_V90EQUALIZER_H */

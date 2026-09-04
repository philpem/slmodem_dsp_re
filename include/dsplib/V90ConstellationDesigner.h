/**
 * @file V90ConstellationDesigner.h
 * @brief `V90ConstellationDesigner`: chooses V.90 downstream constellations.
 *
 * The class is closed: `process()` is the one entry point, running
 * `constellationDesign()`'s three steps (choose constellations for the
 * noise floor, then two optional refinements) in a loop of at most three
 * passes, forcing the rate to `minRate`/`maxRate` whenever the design lands
 * outside them. `adjustConstellationsPower()`, `adjustConstellationsToNewK()`,
 * `constellationDesign()` and `process()` all reach `V90ConstellationPower`
 * and are the only members with a caller inside the class; the rest
 * (`maxK`, `realK`, `findMinValueIndex`, `findConstelMaxValueIndex`,
 * `reconstructInitialConditions` and the small leaves below) are either
 * inlined into those four or have no caller anywhere in the object at all.
 *
 * Not polymorphic (no deleting destructor variant, so no vptr and offset 0
 * is a real member). The object is 84 bytes (`sizeof == 0x54`), asserted by
 * the .cpp. `minRate`/`maxRate` default to 28000/56000 -- the V.90
 * downstream rate ladder's ends -- set by the constructor and changeable via
 * setMinMaxRates(), whose own diagnostics print them in the opposite order
 * from their offsets: the first argument ("set min rate") lands at the
 * higher offset (+0x50), the second ("set max rate") at the lower (+0x4c).
 *
 * Everything the reconstructed members do not touch is `pad_`; the class's
 * other four defined-but-unwritten-here members write into that region.
 */

#ifndef DSPLIB_V90CONSTELLATIONDESIGNER_H
#define DSPLIB_V90CONSTELLATIONDESIGNER_H

/*
 * `spectralDesign`'s second parameter, and the type is part of its mangled
 * name, so this include is what makes the symbol come out right.  See that
 * header for why the type lives on its own.
 */
#include "dsplib/V90SpectralConditions.h"

/*
 * `process`' twelfth parameter, and the type is part of its mangled name
 * (`23__tHardwareCodecTypes__`), so this include is what makes the symbol come
 * out right.  That header holds the enum and nothing else, for exactly this.
 */
#include "dsplib/V90CodecType.h"

/*
 * A POINTER ONLY, so a forward declaration is what belongs here.  Two
 * different definitions of `V90Parameters` exist in this tree -- the 0x504
 * word block in `V90PreFilter.h` and the 0x558 named map in
 * `V90Parameters.h` -- and no translation unit may include both.  Declaring
 * the class here keeps this header compatible with either; the .cpp picks
 * one.  Finding F1112.
 */
class V90Parameters;

/*
 * The constructor's other two arguments, and pointers only, so forward
 * declarations are what belongs here for the same reason `V90Parameters` is
 * one: `V90PreFilter.h` is one of the two headers that DEFINE
 * `V90Parameters`, so including it here would decide for every translation
 * unit which of the two definitions it gets.
 */
class V90PreFilter;
class V90ConstellationPower;

/*
 * The constellation table five of the members below take by pointer, and the
 * type of the member at +0x04.  `V90MappingParams.h` defines it; a pointer is
 * all that is needed here.
 */
class V90MappingParams;

/*
 * `process`' second parameter -- and, as that member proves, what `+0x14`
 * points at.  A pointer is all the declaration needs; the .cpp includes the
 * definition, because `process` reads two of its fields.
 */
class V90AutoDigitalImpDetector;

class V90ConstellationDesigner {
public:
	/**
	 * @brief Construct a designer over its three collaborators.
	 *
	 * Straight-line stores only: keeps `params`, `preFilter` and `power`,
	 * and seeds the rate ladder's two ends (minRate/maxRate), `byte_08`
	 * (0) and `byte_38` (22, the default power-ladder index).
	 *
	 * @param params      The V.90 parameter block (not owned).
	 * @param preFilter   The pre-filter collaborator (not owned).
	 * @param power       The constellation-power collaborator (not owned).
	 */
	V90ConstellationDesigner(V90Parameters *params, V90PreFilter *preFilter,
				 V90ConstellationPower *power);
	/** @brief Destroy a designer. Empty body -- declared because the blob has the symbol, but writes nothing. */
	~V90ConstellationDesigner();

	/**
	 * @brief Set the downstream rate ladder's floor and ceiling.
	 * @param minRate  New minimum rate, stored at +0x50 (default 28000).
	 * @param maxRate  New maximum rate, stored at +0x4c (default 56000).
	 */
	void setMinMaxRates(unsigned int minRate, unsigned int maxRate);
	/** @brief Reset the seven fields reset() owns to their initial values, reading only `params`. */
	void reset();

	/**
	 * @brief Raise 6.0 to an integer power.
	 * @param exponent  The exponent.
	 * @return `6^exponent`.
	 */
	float pow6(short exponent);
	/**
	 * @brief Compute the constellation-shaping factor K for a candidate table.
	 * @param n      Element count.
	 * @param table  The candidate values.
	 * @return The computed K.
	 */
	float calcK(unsigned int n, float *table);
	/**
	 * @brief Compute the real (non-integer) K a mapping's constellations actually achieve.
	 * @param mappingParams  The mapping parameters to evaluate.
	 * @return The achieved K.
	 */
	float realK(V90MappingParams *mappingParams);
	/**
	 * @brief Find the largest integer K a mapping's constellations can support.
	 * @param mappingParams  The mapping parameters to evaluate.
	 * @return The maximum K.
	 */
	int maxK(V90MappingParams *mappingParams);
	/**
	 * @brief Find the point count M whose K is closest to a target K.
	 * @param targetK  The K value to match.
	 * @param k        A working K value.
	 * @return The matching point count M.
	 */
	int calcMtoMatchKtarget(float targetK, float k);
	/**
	 * @brief Find the index of the smallest value in a mapping's per-constellation table.
	 * @param mappingParams  The mapping parameters to search.
	 * @return The index of the minimum.
	 */
	int findMinValueIndex(V90MappingParams *mappingParams);
	/**
	 * @brief Find the constellation with the largest value in its table.
	 * @param mappingParams  The mapping parameters to search.
	 * @return The index of the maximum.
	 */
	int findConstelMaxValueIndex(V90MappingParams *mappingParams);
	/**
	 * @brief Look up a constellation's ucode for a given point.
	 * @param k  Constellation index.
	 * @param i  Point index within the constellation.
	 * @return The ucode at `(k << 7) + i` in `constelTable`.
	 */
	unsigned char constelBuild(short k, short i);
	/**
	 * @brief Design the transmit spectral-shaping filter for a given rate.
	 * @param rate        The data rate to shape for.
	 * @param conditions  The spectral conditions to design under.
	 */
	void spectralDesign(unsigned int rate, V90SpecialSpectralConditions conditions);
	/**
	 * @brief Rebuild a mapping's initial per-constellation ucode assignment.
	 * @param mappingParams  The mapping parameters to rebuild.
	 * @param ucodes         Per-constellation ucode buffer to fill.
	 */
	void reconstructInitialConditions(V90MappingParams *mappingParams, unsigned char *ucodes);
	/**
	 * @brief Choose the next ucode to add to a constellation.
	 * @param ucode           Per-constellation top ucode index (in/out is via the caller's own table).
	 * @param flags           Per-phase flags.
	 * @param table1          A flat `(k<<7)+i`-indexed value table.
	 * @param table2          A second flat `(k<<7)+i`-indexed value table.
	 * @param scratch         Per-constellation scratch values.
	 * @param flagTable       Flat `(k<<7)+i`-indexed flag table.
	 * @return The chosen ucode.
	 */
	int findNextUcodeToAdd(unsigned char *ucode, unsigned char flags,
			      short (*table1)[128], short (*table2)[128], short *scratch,
			      unsigned char (*flagTable)[128]);

	/**
	 * @brief Compute the minimum distance for a given RRN and log its up/down deltas.
	 * @param rrn  The RRN (robbed-bit-signalling ratio) to evaluate.
	 */
	void determineDminForRrn(unsigned int rrn);

	/**
	 * @brief Set each constellation's shape to match a target noise level.
	 * @param noiseEnergy  The noise energy to design against.
	 * @param table1       Flat `(k<<7)+i`-indexed value table.
	 * @param table2       Flat `(k<<7)+i`-indexed value table.
	 * @param scratch      Per-constellation scratch values, indexed 0..5.
	 * @param ucode        Per-constellation ucode, indexed 0..5.
	 * @param flagTable    Flat `(k<<7)+i`-indexed flag table.
	 */
	void setConstellationToNoise(float noiseEnergy, short (*table1)[128], short (*table2)[128],
				     short *scratch, unsigned char *ucode,
				     unsigned char (*flagTable)[128]);

	/**
	 * @brief setConstellationToNoise(), with the data rate forced rather than derived.
	 *
	 * Same design as setConstellationToNoise(), plus a per-phase top ucode
	 * index (`topUcode`) that this function writes through as it runs,
	 * and a per-phase one-bit flag (`phaseFlags`).
	 *
	 * @param noiseEnergy  The noise energy to design against.
	 * @param table1       Flat `(k<<7)+i`-indexed value table.
	 * @param table2       Flat `(k<<7)+i`-indexed value table.
	 * @param scratch      Per-constellation scratch values, indexed 0..5.
	 * @param ucode        Per-constellation ucode, indexed 0..5.
	 * @param topUcode     Per-phase top ucode index; incremented in place as phases are visited.
	 * @param flagTable    Flat `(k<<7)+i`-indexed flag table.
	 */
	void setConstellationToNoise_forceRate(float noiseEnergy, short (*table1)[128],
					       short (*table2)[128], short *scratch,
					       unsigned char *ucode,
					       unsigned char *topUcode,
					       unsigned char (*flagTable)[128]);

	/**
	 * @brief Shrink the constellations until the frame's average power fits the current power-ladder entry.
	 *
	 * Steps `byte_38` down the `V90ConstellationPower::averagePowerLimits`
	 * ladder until the frame's average power is at or under the selected
	 * limit, then restores one point if that took `mappingParams->word_0`
	 * (K) below 21.
	 */
	void adjustConstellationsPower();

	/**
	 * @brief Add or remove constellation points until K reaches the parameter block's `UP_ROUND_K` target.
	 * @param table1     Flat `(k<<7)+u`-indexed value table.
	 * @param table2     Flat `(k<<7)+u`-indexed value table.
	 * @param scratch    Per-constellation scratch values.
	 * @param flagTable  Flat `(k<<7)+u`-indexed flag table; unused (never read by the object).
	 */
	void adjustConstellationsToNewK(short (*table1)[128], short (*table2)[128], short *scratch,
					unsigned char (*flagTable)[128]);

	/**
	 * @brief Run one design pass: choose constellations for the noise, then apply the two optional refinements the parameter block gates.
	 *
	 * With `FORCE_RATE_ENABLE` clear, calls setConstellationToNoise()
	 * (dropping the fifth argument, `phaseFlags`, and passing `flagTable`
	 * in its place); with it set, calls setConstellationToNoise_forceRate()
	 * with all seven.
	 *
	 * @param noiseEnergy  The noise energy to design against.
	 * @param table1       Flat `(k<<7)+i`-indexed value table.
	 * @param table2       Flat `(k<<7)+i`-indexed value table.
	 * @param scratch      Per-constellation scratch values.
	 * @param ucode        Per-constellation ucode.
	 * @param phaseFlags   Per-phase one-bit flag; only used when `FORCE_RATE_ENABLE` is set.
	 * @param flagTable    Flat `(k<<7)+i`-indexed flag table.
	 */
	void constellationDesign(float noiseEnergy, short (*table1)[128], short (*table2)[128],
				 short *scratch, unsigned char *ucode, unsigned char *phaseFlags,
				 unsigned char (*flagTable)[128]);

	/**
	 * @brief The entry point: run constellationDesign() up to three times, forcing the rate to the ladder's ends as needed.
	 *
	 * The only member of the class with a caller inside it. Loops
	 * constellationDesign() at most three times, forcing `rate` to
	 * `minRate` or `maxRate` whenever the design lands outside them.
	 *
	 * @param rate          The initial data rate to design for.
	 * @param detector      The digital-impairment detector supplying constelTable's backing tables.
	 * @param noiseEnergy   The noise energy to design against.
	 * @param forceRate     Non-zero to force the rate rather than deriving it (selects constellationDesign()'s `FORCE_RATE_ENABLE` arm).
	 * @param mappingParams  The mapping parameters to fill.
	 * @param table1        Flat `(k<<7)+i`-indexed value table.
	 * @param table2        Flat `(k<<7)+i`-indexed value table.
	 * @param scratch       Per-constellation scratch values.
	 * @param ucode         Per-constellation ucode.
	 * @param phaseFlags    Per-phase one-bit flag; dropped on the same arm constellationDesign() drops it on.
	 * @param powerLadderIndex  Initial `byte_38` power-ladder index.
	 * @param codecType     The hardware codec type in use.
	 * @param word_40       Additional design parameter (unresolved role beyond storage).
	 * @param spectralConditions  Spectral conditions to design the shaping filter under.
	 * @return 1 if the chosen rate had to be forced up to the minimum ("D choosen is smaller than minimum"), 0 otherwise.
	 */
	int process(unsigned int rate, V90AutoDigitalImpDetector *detector, float noiseEnergy,
		    int forceRate, V90MappingParams *mappingParams, short (*table1)[128],
		    short (*table2)[128], short *scratch, unsigned char *ucode,
		    unsigned char *phaseFlags, unsigned char powerLadderIndex,
		    __tHardwareCodecTypes__ codecType, unsigned int word_40,
		    V90SpecialSpectralConditions spectralConditions);

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable from the mangling, and because a single access
	 * section is what keeps the class POD and __builtin_offsetof well
	 * defined -- the .cpp asserts the offsets below against what the
	 * compiler lays out.
	 */

	/* +0x00  The parameter block.  Not owned; `reset` reads +0x39c. */
	V90Parameters *params;

	/* +0x04  The constellation table; type forced by three independent readers (spectralDesign, constelBuild, findNextUcodeToAdd), written by process() as its fifth argument. See F3406. */
	V90MappingParams *mappingParams;	/* +0x04                    */

	/*
	 * +0x08  `movb $0x0,0x8(%eax)` in the constructor, a byte (encoding
	 * `c6 40 08 00`, no operand-size prefix, no 32-bit immediate). Nothing
	 * reconstructed here reads it, so it is offset-named; the constructor
	 * proves only the width and the initial value, not the meaning.
	 */
	unsigned char byte_08;		/* +0x08                            */

	unsigned char pad_09;		/* +0x09                            */

	/*
	 * +0x0a .. +0x10  Four consecutive 16-bit slots `reset` zeroes with
	 * four `movw $0x0`; offset-named, with the store width alone fixing
	 * them at two bytes rather than four.
	 *
	 * Two of the four have a reader and both readings are `movswl`, so
	 * they stay signed: `findNextUcodeToAdd` loads +0x0a and +0x10 and
	 * adds each to a sign-extended `short` from a caller's table before a
	 * signed comparison -- the forced kind of extension (finding F614),
	 * not the free kind.
	 *
	 * Three of the four have the author's own word for what they hold, out
	 * of `determineDminForRrn`'s format strings (the first reader or
	 * writer of +0x0c and +0x0e anywhere in the reconstruction, `reset`'s
	 * zeroing aside):
	 *
	 *     +0x0a  dMin         read as `filds 0xa(%edx)`, and copied into
	 *                         both of the others; still written by
	 *                         nothing this tree has reconstructed
	 *     +0x0c  rrnDownDmin  "V90ConstellationDesigner:: rrnDownDmin =
	 *                         %d" prints `movswl 0xc(%edx)`
	 *     +0x0e  rrnUpDmin    the same message and load, at +0x0e
	 *
	 * They keep their offset names: a name out of a diagnostic is the
	 * author's word for the quantity, not yet a claim about the slot the
	 * offset assertions pin, so the mapping is recorded here for a later
	 * batch to act on.
	 */
	short short_0a;			/* +0x0a  the author's `dMin`        */
	short short_0c;			/* +0x0c  the author's `rrnDownDmin` */
	short short_0e;			/* +0x0e  the author's `rrnUpDmin`   */
	short short_10;			/* +0x10 */

	unsigned char pad_12[2];	/* +0x12                            */

	/*
	 * +0x14  `constelBuild`'s table, its only reader. Two more independent
	 * displacements off the same pointer (a byte table at +0xd00, a
	 * per-phase flag array at +0x280c) are read by `determineDminForRrn`,
	 * and `process` writes the field from its second argument, a
	 * `V90AutoDigitalImpDetector *`. The three displacements land exactly
	 * on that class's `linMapp[6][128]`, `byte_0d00[6][128]` and
	 * `byte_280c[6]`, so the declared type stays `short (*)[128]` --
	 * `&detector->linMapp[0]` is this pointer, value and type alike. See
	 * finding F3406 (this retracts an earlier standalone-array reading of
	 * this field).
	 */
	short (*constelTable)[128];	/* +0x14 = &detector->linMapp[0]    */

	/*
	 * +0x18 .. +0x20  Three floats; `setConstellationToNoise` is the first
	 * reader or writer of any of them, writing all three (as `noiseEnergy
	 * * 0.45f`, `* 1.4125f`, and `* 2.0f or 4.0f` respectively) in three of
	 * its four `word_48` arms, and none in the KeepRate arm ("keep dMin
	 * and pdsnr thresh").
	 *
	 * The author's own words for them are in three unconditional
	 * `edprintf` sites that follow, in offset order: `pdSnrThreshForRateUp`,
	 * `pdSnrThreshForRateDown`, `pdSnrThreshForRetrain`. They keep their
	 * offset names for the reason `short_0c`/`short_0e` do: a name out of a
	 * diagnostic is the author's word for the quantity, not yet a claim
	 * about the slot itself, so the mapping is recorded here for a later
	 * batch to act on.
	 */
	float float_18;			/* +0x18  pdSnrThreshForRateUp      */
	float float_1c;			/* +0x1c  pdSnrThreshForRateDown    */
	float float_20;			/* +0x20  pdSnrThreshForRetrain     */

	/*
	 * +0x24  Seeded by `reset` from the parameter block's +0x39c, which
	 * `tools/vparse.py` reports as `unnamed_39c`: `setToDefault` writes it
	 * and `loadParams` never reads it, so the original has no name for it
	 * either (finding F878).
	 */
	unsigned int word_24;		/* +0x24 = params->w[0x39c / 4]     */

	/*
	 * +0x28  Four bytes because it is compared against `word_2c`, which is
	 * the only thing about it the object fixes: `setConstellationToNoise`
	 * loads `mov 0x2c(%ecx),%edx` and then `cmp 0x28(%ecx),%edx`, a 32-bit
	 * compare with no operand-size prefix, so the two slots are the same
	 * width. It used to be `pad_28[4]`.
	 *
	 * Both slots now have their writer, `process`, which copies them out
	 * of the detector it is handed:
	 *
	 *     mov 0xa95c(%edi),%eax ; mov %eax,0x28(%ebp)   pcmType
	 *     mov 0xa960(%edi),%esi ; mov %esi,0x2c(%ebp)   int_a960
	 *
	 * So the "nothing anywhere in the object writes it" sentence that used
	 * to stand here is retracted for both; `word_28` is the session's
	 * companding law as `V90AutoDigitalImpDetector::reset` stored it, and
	 * the equality test between the two asks whether the detector's second
	 * flag agrees with it. The types stay as the widths measure them: what
	 * the object forces is four bytes and a 32-bit compare, and naming
	 * `PcmType` here would make this header depend on the one that defines
	 * the enum for no measured gain.
	 */
	int word_28;			/* +0x28 = detector->pcmType        */

	/*
	 * +0x2c  The companding law, a four-byte load: `mov 0x2c(%edx),%esi ;
	 * test %esi,%esi` in `findNextUcodeToAdd`, whose zero arm calls
	 * `linear2ulaw` and complements the result and whose non-zero arm
	 * calls `linear2alaw` and XORs it with 0xd5. So non-zero is A-law,
	 * the same convention `PcmType` records for `V90Phase3Modulator`
	 * (that header's finding). It is typed `int` rather than `PcmType`
	 * because the only thing the object forces is the width and the
	 * `!= 0`, and naming the type would make this header depend on the
	 * one that defines the enum for no measured gain. It used to be
	 * inside `pad_28`.
	 *
	 * It is also what the three new members hand to `getPower` as its
	 * `PcmType` argument, the same reading from a second direction:
	 * `mov 0x2c(%ebp),%edx ; mov %edx,0xc(%esp)` ahead of every one of the
	 * five `V90ConstellationPower::getPower` calls. See `word_28` above
	 * for where `process` gets it from.
	 */
	int word_2c;			/* +0x2c = detector->int_a960       */

	/*
	 * +0x30 and +0x44  The constructor's third and second arguments
	 * respectively, stored and never read by anything reconstructed here;
	 * typed from the constructor's own mangling rather than the store
	 * width. Both used to be inside `pad_28`.
	 */
	V90ConstellationPower *power;	/* +0x30 = constructor argument 3   */

	unsigned char pad_34[4];	/* +0x34                            */

	/*
	 * +0x38  `movb $0x16,0x38(%eax)`, again a byte by its encoding
	 * (`c6 40 38 16`). 22 is not one of the rate-ladder constants and
	 * nothing here reads it, so it is offset-named too.
	 *
	 * It now has a writer and a reader: `process` stores its eleventh
	 * argument into it (`mov %dl,0x38(%ebp)`, an `unsigned char` by the
	 * mangling), and `adjustConstellationsPower` reads it as the index
	 * into `V90ConstellationPower::averagePowerLimits`, clamped by
	 * `cmp $0x15,%al; ja` to the constructor's own 22, which is why the
	 * seed is that value. So the field is a power-ladder index and the
	 * default is the ladder entry the design starts from; it keeps its
	 * offset name because nothing in the object names the quantity.
	 */
	unsigned char byte_38;		/* +0x38 a power-ladder index, 22   */

	unsigned char pad_39[3];	/* +0x39                            */

	/*
	 * +0x3c and +0x40  `process`' twelfth and thirteenth arguments, stored
	 * and read by nothing reconstructed here.  What types them is the
	 * mangling of the member that writes them and nothing else:
	 * `...h23__tHardwareCodecTypes__j...` makes argument 12 the codec enum
	 * and argument 13 an `unsigned int`, and the object puts 12 at +0x3c
	 * (`mov 0x90(%esp),%edx ; mov %edx,0x3c(%ebp)`) and 13 at +0x40 (`mov
	 * 0x94(%esp),%ecx ; mov %ecx,0x40(%ebp)`).  Both are `movl`, so both
	 * are four bytes.  They used to be inside `pad_39[11]`.
	 */
	__tHardwareCodecTypes__ codecType;	/* +0x3c                    */
	unsigned int word_40;			/* +0x40                    */

	V90PreFilter *preFilter;	/* +0x44 = constructor argument 2   */

	unsigned int word_48;		/* +0x48 zeroed by reset            */

	unsigned int maxRate;		/* +0x4c defaults to 56000          */
	unsigned int minRate;		/* +0x50 defaults to 28000          */
};

#endif /* DSPLIB_V90CONSTELLATIONDESIGNER_H */

/*
 * V90Equalizer.h -- the V.90 receive equaliser, so far as three of its
 * twenty-seven members need it.
 *
 * Reconstructed from dsplibs.o.  `V90Equalizer` is NOT polymorphic --
 * tools/cppstruct.py lists its destructor with the `D1` and `D2` variants and
 * not the deleting `D0`, and GCC emits a deleting destructor only for a
 * virtual class -- so offset 0 is a real member and there is no vptr to shift every
 * field by four (finding 228).
 *
 * THE OBJECT IS 328 BYTES.  The largest `this`-relative displacement across
 * all twenty-nine V90Equalizer symbols in the blob is +0x146, in `reset`:
 *
 *     3b58e:  66 89 bb 46 01 00 00    mov    %di,0x146(%ebx)
 *
 * a two-byte store, so the object runs to 0x148 = 328 and not to the 0x146
 * the displacement alone gives.  A displacement is not a size (finding 215,
 * and the V90Jd 0x8c -> 144 worked example in docs/v90cpp.md).  Every
 * four-byte slot from +0x00 to +0x144 is touched by some member except
 * +0xc8, +0x108 and +0x138.
 *
 * ONLY THE FIELDS THE THREE WRITTEN METHODS TOUCH ARE NAMED.  Everything else
 * is `pad_*`, because a field this batch cannot see written is a field this
 * batch cannot claim (findings 223, 224 -- the harness fill makes untouched
 * memory compare equal on both sides, so a passing test says nothing about
 * where an untouched field lives).
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
 * The values `state` (+0x60) takes.  Each of the four `enter*` methods opens
 * with `cmpl $N,0x60(this); je <return>` and, having done its work, stores
 * the same N back -- so the field is a state and these are four of its
 * values.  What the state is called in the original, and whether 0, 2 and 3
 * are used, is not established here; `reset` stores 0.
 */
#define V90EQU_STATE_RESET		0	/* V90Equalizer::reset       */
#define V90EQU_STATE_PHASE3		1	/* enterPhase3()             */
#define V90EQU_STATE_RRN		4	/* enterRRN()                */
#define V90EQU_STATE_FPE		5	/* enterFPE()                */
#define V90EQU_STATE_CHANNEL_VERIFY	6	/* enterChannelVerification()*/

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
	 * Data members are public for the reason V90Jd.h gives: the original's
	 * access specifiers are not recoverable (finding 226), and a single
	 * access section is what keeps the class POD, so the .cpp can assert
	 * every offset below with __builtin_offsetof and the test can put the
	 * object in a union with a byte array.
	 */

	unsigned char pad_00[0x10];	/* +0x00 */

	/*
	 * The linear equaliser's LMS step size.  `setLinearEquBeta` compares
	 * its argument against this slot and then stores it there; `reset`
	 * plants 0x283424dc in it immediately before calling the setter with
	 * zero, which is what makes the setter's inequality fire.
	 */
	float linearEquBeta;		/* +0x10 */

	unsigned char pad_14[0x28];	/* +0x14 */

	/*
	 * The decision-feedback filter's step size, the same way round:
	 * `setDfeBeta` stores here, and `getDfeBeta` is nothing but
	 * `flds 0x3c(%eax); ret`.
	 */
	float dfeBeta;			/* +0x3c */

	unsigned char pad_40[0x20];	/* +0x40 */

	int state;			/* +0x60 see V90EQU_STATE_* above */

	/*
	 * Zeroed by `enterPhase3`, by `enterChannelVerification` and by
	 * `reset`, always in the instruction after `state` is written.  What
	 * it counts is not established by anything in this batch.
	 */
	int stateCount;			/* +0x64 */

	unsigned char pad_68[0x48];	/* +0x68 */

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

	unsigned char pad_b4[0x8];	/* +0xb4 */

	/*
	 * The four slots the linear equaliser's fixed-point step size is built
	 * from.  `setLinearEquBeta` reads the first two and writes the last
	 * two; `convertEqualizerToMmx` writes the same last two from the same
	 * arithmetic over a magnitude it has just measured, which is what says
	 * +0xbc holds a magnitude and not a step size.  `reset` zeroes +0xbc.
	 */
	float linearEquMmxRefLevel;	/* +0xbc */
	unsigned char pad_c0[0x4];	/* +0xc0 (zeroed by reset)        */
	float linearEquMmxBetaScale;	/* +0xc4 */
	unsigned char pad_c8[0x4];	/* +0xc8 (no member touches it)   */
	int linearEquMmxBeta;		/* +0xcc */
	int linearEquMmxShift;		/* +0xd0 */

	unsigned char pad_d4[0x28];	/* +0xd4 */

	/* The same four for the decision-feedback filter, +0x40 further on. */
	float dfeMmxRefLevel;		/* +0xfc */
	unsigned char pad_100[0x4];	/* +0x100 (zeroed by reset)       */
	float dfeMmxBetaScale;		/* +0x104 */
	unsigned char pad_108[0x4];	/* +0x108 (no member touches it)  */
	int dfeMmxBeta;			/* +0x10c */
	int dfeMmxShift;		/* +0x110 */

	unsigned char pad_114[0x34];	/* +0x114 .. +0x147               */
};

#endif /* DSPLIB_V90EQUALIZER_H */

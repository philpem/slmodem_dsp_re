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
 * allocating 0x150 for its slot.  A displacement is not a size (finding 215,
 * and the V90Jd 0x8c -> 144 worked example in docs/v90cpp.md); a
 * displacement scan over one class is not a bound either.  Finding 1107.
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
 * BOTH ARE POINTERS HERE, so both are forward-declared and neither header is
 * included.  `V90Parameters` has two incompatible definitions in this tree
 * and no translation unit may include both (finding 1112); `V90Resampler.h`
 * pulls one of them in, and this header is included by `V90Demodulator.h`,
 * which pulls in the other.  The .cpp picks.
 */
class V90Parameters;
class V90Resampler;

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
	 * Data members are public for the reason V90Jd.h gives: the original's
	 * access specifiers are not recoverable (finding 226), and a single
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

	unsigned char pad_04[4];	/* +0x04 */

	/* +0x08  Zeroed by `reset` with a `movw`, so two bytes and not four. */
	short short_08;			/* +0x08 */

	unsigned char pad_0a[2];	/* +0x0a alignment                */

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
	 * `%eax = word_1c - i`, so index `word_1c - 1 - i` runs downwards
	 * while the first array runs up.  Two arrays walked in opposite
	 * directions by one counter; what the second holds is not established
	 * here, so it is offset-named.
	 */
	float *array_18;		/* +0x18 */

	/*
	 * +0x1c  The bound the descending clear counts down from, and the
	 * length (plus eight) of the fixed-point array at +0xec.  Unsigned:
	 * `lea 0x8(%ebp),%eax / cmp %edx,%eax / ja` is the unsigned form.
	 */
	unsigned int word_1c;		/* +0x1c */

	/*
	 * +0x20  Derived, not copied: `reset` computes `word_1c -
	 * linearEquLength - 1` and stores it here BEFORE the guard that skips
	 * the clearing loops, so it is written even when the equaliser has no
	 * taps.
	 */
	unsigned int word_20;		/* +0x20 */

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

	unsigned int word_34;		/* +0x34 zeroed by reset          */

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

	unsigned char pad_48[0x18];	/* +0x48 */

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
	unsigned int word_68;		/* +0x68 */
	unsigned int word_6c;		/* +0x6c */
	unsigned int word_70;		/* +0x70 */

	/* +0x74  = params->ERROR_ENERGY_MEAN_BLOCK_LEN */
	int errorEnergyMeanBlockLen;	/* +0x74 */

	unsigned int word_78;		/* +0x78 */
	unsigned int word_7c;		/* +0x7c */
	unsigned int word_80;		/* +0x80 */
	unsigned int word_84;		/* +0x84 */
	unsigned int word_88;		/* +0x88 */
	unsigned int word_8c;		/* +0x8c */

	/*
	 * +0x90  = params->ERROR_ENERGY_MEAN_K.  Copied as a 32-bit word and
	 * never loaded into the x87 stack, so it is moved as a float and not
	 * converted -- the same shape as V90Phase3Demodulator's +0x418.
	 */
	float errorEnergyMeanK;		/* +0x90 */

	unsigned int word_94;		/* +0x94 */
	unsigned char pad_98[4];	/* +0x98 reset does not reach it  */
	unsigned int word_9c;		/* +0x9c */
	unsigned int word_a0;		/* +0xa0 */
	unsigned int word_a4;		/* +0xa4 */

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

	unsigned char pad_b4[0x8];	/* +0xb4 */

	/*
	 * The four slots the linear equaliser's fixed-point step size is built
	 * from.  `setLinearEquBeta` reads the first two and writes the last
	 * two; `convertEqualizerToMmx` writes the same last two from the same
	 * arithmetic over a magnitude it has just measured, which is what says
	 * +0xbc holds a magnitude and not a step size.  `reset` zeroes +0xbc.
	 */
	float linearEquMmxRefLevel;	/* +0xbc */
	unsigned int word_c0;		/* +0xc0 zeroed by reset          */
	float linearEquMmxBetaScale;	/* +0xc4 */
	unsigned char pad_c8[0x4];	/* +0xc8 (no member touches it)   */
	int linearEquMmxBeta;		/* +0xcc */
	int linearEquMmxShift;		/* +0xd0 */

	/*
	 * +0xd4, +0xd8  The linear half's fixed-point coefficient pair,
	 * `linearEquLength + 8` SHORTS each -- `movw $0x0,(%ecx,%eax,2)`, a
	 * two-byte store with a two-byte stride, which is what says 16-bit.
	 * Cleared only when `mmxArraysPresent` is set.
	 */
	short *linearEquMmxCoefs;	/* +0xd4 */
	short *array_d8;		/* +0xd8 */

	unsigned char pad_dc[0x10];	/* +0xdc */

	/* +0xec  `word_1c + 8` shorts, cleared under the same condition. */
	short *array_ec;		/* +0xec */

	unsigned char pad_f0[0xc];	/* +0xf0 */

	/* The same four for the decision-feedback filter, +0x40 further on. */
	float dfeMmxRefLevel;		/* +0xfc */
	unsigned int word_100;		/* +0x100 zeroed by reset         */
	float dfeMmxBetaScale;		/* +0x104 */
	unsigned char pad_108[0x4];	/* +0x108 (no member touches it)  */
	int dfeMmxBeta;			/* +0x10c */
	int dfeMmxShift;		/* +0x110 */

	/*
	 * +0x114, +0x118, +0x12c  The decision-feedback half's fixed-point
	 * arrays, `dfeLength + 8` shorts each, all three cleared in one loop.
	 */
	short *dfeMmxCoefs;		/* +0x114 */
	short *array_118;		/* +0x118 */

	unsigned char pad_11c[0x10];	/* +0x11c */

	short *array_12c;		/* +0x12c */

	unsigned char pad_130[0xc];	/* +0x130 */

	unsigned int word_13c;		/* +0x13c zeroed by reset         */
	unsigned int word_140;		/* +0x140 zeroed by reset         */

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

	unsigned char pad_14c[4];	/* +0x14c .. +0x14f               */
};

#endif /* DSPLIB_V90EQUALIZER_H */

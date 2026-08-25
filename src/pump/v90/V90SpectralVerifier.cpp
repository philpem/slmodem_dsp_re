/*
 * V90SpectralVerifier.cpp -- clearing the spectrum accumulator.
 *
 * Reconstructed from dsplibs.o.  One of the class's twelve members:
 * `reset()`, which is the one `v34handshak` reaches.
 * `include/dsplib/V90SpectralVerifier.h` carries the object map.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
 *
 * THE DIAGNOSTIC IS NOT GATED.  Unlike `V90ConstellationDesigner`'s two, this
 * one is a bare `call edprintf` with no `dsplibs_debug_level` test in front
 * of it -- because `edprintf` does its own gating, and does it AFTER
 * formatting and encoding, so the call has an effect at every level (see
 * src/core/encode.c).  It comes first in the object and it comes first here.
 *
 * THE THREE STORES ARE INDEPENDENT.  The object emits them +0x24, +0x20,
 * +0x28; they are three zeroes into three distinct words of the same object,
 * so the order is the scheduler's and not the source's, and no order of the
 * three is distinguishable by any observer.  Written low-to-high here.
 */

#include <stddef.h>

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/sysdep.h"
#include "dsplib/Psd.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90SpectralVerifier.h"

/*
 * THE REPLACEMENT `operator delete` / `operator delete[]`, READ OFF THE
 * OBJECT.  The blob frees the owned `Psd` with `if (p) { Psd::~Psd(p);
 * sysdep_free(p); }` -- `delete p` over an inline wrapper -- and the two float
 * buffers with a plain guarded free.  At the destructor's LAST free the
 * delete-expression emits an ordinary `call sysdep_free` where the open-coded
 * form emits a sibling `jmp`.  refinement.md lever 7, findings 7786 and 7817.
 */
inline void operator delete(void *p) { sysdep_free(p); }

/*
 * AND THE SIZED FORM, FOR THE MODERN BUILD ONLY.  C++14 added
 * `operator delete(void *, size_t)`, and GCC 13 calls it for `delete p` on a
 * class with a destructor -- an undefined `_ZdlPvj` in a tree that links no
 * libstdc++, which is the link failure `dsplib/Resampler.h` documents.
 *
 * `__cplusplus >= 201402L` is FALSE under GCC 3.4.2 (199711L), so the compiler
 * that decides byte identity never sees this.  It is portability plumbing and
 * carries no claim about the object.
 */
#if defined(__cplusplus) && __cplusplus >= 201402L
inline void operator delete(void *p, __SIZE_TYPE__) { sysdep_free(p); }
#endif
inline void operator delete[](void *p) { sysdep_free(p); }

/*
 * `Psd::Psd` AND `Psd::~Psd` BY THEIR MANGLED NAMES, for the reason
 * V90Demodulator.cpp gives for `V90Resampler::reset`, plus one this class
 * has on its own: the object allocates the `Psd` with `sysdep_malloc` and
 * then runs its constructor over that storage, and C++ has no syntax for
 * that without `<new>`, which this tree builds `-nostdinc++` without.  The
 * symbols are the ones the object calls, `C1` and `D1`, and both take `this`
 * as their first stack argument like everything else here (finding 215).
 */
extern void psd_construct(void *self, unsigned int length, WindowType window,
			  unsigned int overlap)
	asm("_ZN3PsdC1Ej10WindowTypej");
/*
 * `psd_destruct` IS GONE: `delete psd` calls `Psd::~Psd` (D1) itself, which
 * is the same symbol this asm() label named.  Finding 7817.
 */

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90SV_OFF(field, off, tag) \
	typedef char v90sv_off_##tag[ \
	    ((int)__builtin_offsetof(V90SpectralVerifier, field) == (off)) \
	    ? 1 : -1]

V90SV_OFF(params,       0x00, params);
V90SV_OFF(psd,          0x04, psd);
V90SV_OFF(sampleFreq,   0x08, samplefreq);
V90SV_OFF(fftLength,    0x0c, fftlength);
V90SV_OFF(psdLength,    0x10, psdlength);
V90SV_OFF(binWidth,     0x14, binwidth);
V90SV_OFF(buf_18,       0x18, buf18);
V90SV_OFF(spectrum,     0x1c, spectrum);
V90SV_OFF(accumCount,   0x20, accumcount);
V90SV_OFF(accumulating, 0x24, accumulating);
V90SV_OFF(word_28,      0x28, word28);
typedef char v90sv_size[(sizeof(V90SpectralVerifier) == 0x2c) ? 1 : -1];

/* The parameter slots the constructor reads, held to the map (finding 230). */
#define V90SV_POFF(field, off, tag) \
	typedef char v90sv_poff_##tag[ \
	    ((int)__builtin_offsetof(V90Parameters, field) == (off)) ? 1 : -1]

V90SV_POFF(SPECTRAL_VERIFIER_SAMPLE_FREQ,      0x2ac, freq);
V90SV_POFF(SPECTRAL_VERIFIER_FFT_LEN,          0x2b0, fftlen);
V90SV_POFF(SPECTRAL_VERIFIER_FFT_WINDOW,       0x2b4, window);
V90SV_POFF(SPECTRAL_VERIFIER_PSD_LEN,          0x2b8, psdlen);
V90SV_POFF(SPECTRAL_VERIFIER_PSD_OVERLAP_LEN,  0x2bc, overlap);
#endif

/*
 * THREE ALLOCATIONS, NONE CHECKED, AND ONE OF THEM CONSTRUCTED.  The two
 * float buffers are left exactly as the allocator returned them -- nothing
 * here clears either -- and the third holds a `Psd` built from three
 * parameter slots.
 *
 * THE PARAMETER BLOCK IS RE-READ THROUGH THE OBJECT.  `SPECTRAL_VERIFIER_-
 * FFT_WINDOW` and `..._PSD_OVERLAP_LEN` are loaded via `(%ebx)`, the pointer
 * just stored at +0x00, and not via the incoming argument still live in a
 * register -- so the source names the member, and it is written that way.
 *
 * +0x20 AND +0x24 ARE NOT INITIALISED, which `reset()` clears and this does
 * not.  It is the object's, not an omission here; docs/deviations.md.
 */
V90SpectralVerifier::V90SpectralVerifier(V90Parameters *p)
{
	params = p;

	fftLength = p->SPECTRAL_VERIFIER_FFT_LEN;
	sampleFreq = p->SPECTRAL_VERIFIER_SAMPLE_FREQ;
	binWidth = sampleFreq / fftLength;
	psdLength = p->SPECTRAL_VERIFIER_PSD_LEN;

	buf_18 = (float *)sysdep_malloc(psdLength * sizeof(float));
	spectrum = (float *)sysdep_malloc((fftLength / 2) * sizeof(float));

	psd = (Psd *)sysdep_malloc(sizeof(Psd));
	psd_construct(psd, fftLength,
		      (WindowType)params->SPECTRAL_VERIFIER_FFT_WINDOW,
		      params->SPECTRAL_VERIFIER_PSD_OVERLAP_LEN);

	word_28 = 0;
}

/*
 * THE ORDER IS +0x18, +0x1c, +0x04, and the third is the only one that is
 * more than a free: the `Psd` is destroyed and then its storage released,
 * two calls on the same address.  Each pointer is tested first and none is
 * nulled afterwards.
 */
V90SpectralVerifier::~V90SpectralVerifier()
{
	delete[] buf_18;

	delete[] spectrum;

	delete psd;
}

void
V90SpectralVerifier::reset()
{
	edprintf("V90SpectralVerifier: Reset\r\n");

	accumulating = 0;
	accumCount = 0;
	word_28 = 0;
}

/*
 * ===========================================================================
 * The accumulation cycle and the five frequency accessors
 * ===========================================================================
 *
 * THE OBJECT'S TU ORDER IS `startAccumulation`, the three `freqTo*Bin`, the
 * two `getSpectrumOf*`, `printSpectrum`, `checkSpecialSpectralConditions`,
 * `process` (0x45cc0 .. 0x465b0).  This file follows it except for
 * `printSpectrum`, which is written after `checkSpecialSpectralConditions`
 * because it shares that section's `SV_PRINT_SIGN` and `sv_abs` helpers.
 * That is a text difference and not a code one: the only ordering that can
 * reach code generation is an inlining opportunity, nothing in this file
 * calls any of the five accessors, and `printSpectrum` still precedes its
 * one caller exactly as in the object.
 *
 * THE FIVE ACCESSORS HAVE NO CALLER ANYWHERE IN THE OBJECT.  Not one
 * `R_386_PC32` in 1.2 MB names any of them, so every use the original had was
 * inlined and these out-of-line bodies exist only because a non-static member
 * defined outside its class body is emitted whether or not it is called.  They
 * are written the same way here, and the codegen tier is what checks them.
 */

/*
 * `fistpll` AND THE LOW DWORD IS THE UNSIGNED CONVERSION, and it is what all
 * three roundings end in -- 0x45d2a, 0x45d64, 0x45da4 and 0x45eeb.  A signed
 * `(int)` cast compiles to a 32-bit `fistpl`; the 64-bit store with only
 * `(%esp)` read back is `fixuns_truncsfsi2`, so the destination type is a
 * 32-bit UNSIGNED one.
 *
 * `unsigned int` RATHER THAN THE `unsigned long` THE CLASS'S OWN ACCESSOR
 * TAKES.  `getSpectrumOfBin`'s mangling ends `Em`, so its parameter is
 * `unsigned long` and that is not ours to choose; the three converters'
 * return type is not mangled anywhere and is chosen the way `sv_bin_at`
 * already chose it in the next section -- on i386 the two types are the
 * same and both expand to the object's idiom, and `make check64` compiles
 * this file for a target where `unsigned long` is 64 bits, where the
 * conversion would stop agreeing above 2**32.
 *
 * THE ROUNDINGS ARE THREE AND THEY ARE NOT INTERCHANGEABLE: truncate, add a
 * half and truncate, truncate and add one.  `freqToRightBin`'s `inc %eax` at
 * 0x45db1 is on the INTEGER, after the conversion, so it is not
 * `(unsigned)(f / binWidth + 1.0f)` -- the two disagree for every operand
 * whose quotient is within one ulp below an integer.
 */
unsigned int
V90SpectralVerifier::freqToNearestBin(float freq) const
{
	return (unsigned int)(freq / binWidth + 0.5f);
}

unsigned int
V90SpectralVerifier::freqToLeftBin(float freq) const
{
	return (unsigned int)(freq / binWidth);
}

unsigned int
V90SpectralVerifier::freqToRightBin(float freq) const
{
	return (unsigned int)(freq / binWidth) + 1;
}

/*
 * FOUR INSTRUCTIONS AND NO BOUND.  `spectrum` is `fftLength / 2` floats long
 * and 0x45dc0 indexes it with the caller's argument unchecked; there is no
 * compare and no clamp in the object.  Reproduced, and recorded as D780 --
 * this reconstruction does not add a guard the original does not have, and
 * the fixture is what keeps the index in range so that no differential trial
 * reaches the undefined access (D561).
 */
float
V90SpectralVerifier::getSpectrumOfBin(unsigned long bin) const
{
	return spectrum[bin];
}

float
V90SpectralVerifier::getSpectrumOfNearestBin(float freq) const
{
	return getSpectrumOfBin(freqToNearestBin(freq));
}

/*
 * ARMING AN ACCUMULATION IS GATED ON A PARAMETER, NOT JUST ITS DIAGNOSTIC.
 * `SPECTRAL_VERIFIER_ENABLE` (+0x2a4) is tested at 0x45cd6 and the `jne`
 * carries the two state stores as well as the `edprintf`, so with the
 * parameter clear the object never leaves state 0 and `process` -- which
 * requires state 1 -- accumulates nothing for the rest of the connection.
 * That is the whole verifier turned off by one word, and it is why the test
 * drives the parameter both ways.
 *
 * THE `== 1` TEST IS FOR ALREADY-RUNNING AND NOT FOR ANYTHING ELSE.  State 2
 * (finished) falls THROUGH it, so a second call after a completed
 * accumulation re-arms; only state 1 is refused.
 */
void
V90SpectralVerifier::startAccumulation()
{
	if (accumulating == 1)
		return;

	if (params->SPECTRAL_VERIFIER_ENABLE == 0)
		return;

	edprintf("V90SpectralVerifier: start data accumulation\r\n");

	accumulating = 1;
	accumCount = 0;
}

/*
 * ===========================================================================
 * checkSpecialSpectralConditions -- 1,682 bytes, 0x45f10
 *
 * Three line conditions, tested in order, each overwriting the last:
 * a German ISDN NT1 box, a German PBX, and a severe codec.  Every threshold
 * and every probe frequency is a `V90Parameters` field the author named
 * himself (`SPECTRAL_VERIFIER_ISDN_LEFT_PEAK_FREQ` and its fourteen
 * neighbours at +0x2c4..+0x2f8), and every one of the nine diagnostics is
 * the author's own sentence.  So this function is unusually well anchored:
 * the parameter block says what is being measured and the format strings say
 * what the answer means.
 *
 * THE THREE TESTS ARE SEQUENTIAL `if`s AND NOT AN `else` CHAIN.  0x46588
 * stores 2 with no test of what is already in +0x28, and 0x46573 stores 3
 * the same way, so a line that trips two conditions is reported as the LATER
 * one.  Reproduced deliberately.
 * ===========================================================================
 */

/*
 * The magnitude of a signed 32-bit value; `cltd; xor %edx,%eax; sub
 * %edx,%eax` at 0x46015, 0x460a5, 0x4636b, 0x46407 and 0x464b1.  `inline`
 * for the reason `adid_abs` gives -- GCC 3.4.2 does not inline a plain
 * `static` here, and a call between the computation and the print would
 * spill an x87 value through a four-byte slot and change what is printed
 * (finding 1448).
 */
static inline int
sv_abs(int v)
{
	return v < 0 ? -v : v;
}

/*
 * A FLOAT IS PRINTED AS `%c%d.%02d`, exactly as in V90AutoDigitalImpDetector
 * and V90ConnectionEvaluator: the object has no floating-point formatting at
 * all and hands `edprintf` a sign character and two integers.
 *
 * THE SIGN TEST IS THE NEGATION OF THE ORDERED ONE, and that is measured,
 * not stylistic.  All five sites are
 *
 *	fldz / fcomps (or fcompp) / fnstsw %ax / sahf
 *	sbb %eax,%eax / and $0xfffffffe,%eax / add $0x2d,%eax
 *
 * -- 0x4601e, 0x460ae, 0x46374, 0x464ba and 0x4650c -- a branchless
 * `0x2d - 2*CF`, which is only encodable when the '+' arm is the CF arm, and
 * CF is "below OR unordered" with the zero in %st(0).  See the long note at
 * ADID_PRINT_SIGN for the ten spellings that were compiled to establish that.
 */
#define SV_PRINT_SIGN(v)	(!(0.0f >= (v)) ? '+' : '-')

/* `fabs` then a truncating `fistpl`. */
#define SV_PRINT_WHOLE(v)	((int)__builtin_fabsf(v))

/*
 * THE HUNDREDTHS ARE SPELLED TWO WAYS IN THE OBJECT AND NO TEST CAN TELL
 * THEM APART.  Both are `abs((int)(100 * <the fractional part>))`, and the
 * two differ only in which way round the subtraction goes:
 *
 *	0x45ff5   de ea   FSUBP ST(2),ST(0)   ST(2) = ST(0) - ST(2)
 *	0x46186   de eb   FSUBP ST(3),ST(0)   ST(3) = ST(0) - ST(3)
 *	    with %st(0) holding `(float)(int)v` and the deeper slot `v`,
 *	    so those two compute `(int)v - v`
 *
 *	0x46072   d8 6c 24 4c   FSUBR ST(0),m32   ST(0) = m32 - ST(0)
 *	0x4621a   d8 6c 24 50   the same
 *	0x46353 / 0x46497   d8 e9   FSUBR ST(0),ST(1)  ST(0) = ST(1) - ST(0)
 *	    so those four compute `v - (int)v`
 *
 * The encodings are forced -- reg-stack chooses which register dies and
 * therefore which mnemonic, but it can never flip the sign of a subtraction
 * -- and the two sites that come out reversed are the two LEFT peak deltas,
 * which is a pattern rather than a slip.  The `sv_abs` outside makes the two
 * agree for every value, so this is a codegen-tier claim only: it is written
 * the way the object encodes it, `compare.py` is what adjudicates it, and
 * the differential tier cannot and does not.  `ce_frac3`'s comment makes the
 * same declaration about a constant its mutation set records as equivalent.
 *
 * THE SCALE IS A `double` AND THAT IS NOT FREE.  0x4633f and 0x463ce load it
 * with `fldl` out of `.rodata.cst8+0x100`, and an eight-byte pool entry
 * cannot have been a `float` (finding 1384 makes the converse argument about
 * a four-byte one).  0x46425 materialises the same 0x4059000000000000 as two
 * integer stores rather than a load at all.  The two ISDN sites load `flds`
 * out of `.rodata.cst4+0x3c0` and then SPILL the register with `fstpl`,
 * eight bytes, which is the narrowed load of the same `double` and not a
 * `float`: a `float` in a register spills with `fstps`.
 */
#define SV_FRAC2(v)	sv_abs((int)(100.0 * ((v) - (float)(int)(v))))
#define SV_FRAC2_REV(v)	sv_abs((int)(100.0 * ((float)(int)(v) - (v))))

/*
 * `getSpectrumOfNearestBin(f)` is `spectrum[(unsigned)(f / binWidth + 0.5f)]`
 * -- 0x45ec0, four instructions of it -- and this function calls it seven
 * times with the divisor never changing.  IT DOES NOT DIVIDE SEVEN TIMES.
 * Each of the three blocks opens `fld1` / `fdivs 0x14(%edi)` once (0x45f37,
 * 0x460e1, 0x4629a) and then MULTIPLIES by that reciprocal at every probe,
 * so the source computed the reciprocal itself: GCC 3.4.2 turns `x / y` into
 * `x * (1/y)` only under `-funsafe-math-optimizations`, which
 * `tools/toolchain/build.sh` does not set, and the tree-ssa pass that would
 * do it for a repeated divisor is not in 3.4 at all.
 *
 * THAT IS AN OBSERVABLE DIFFERENCE AND NOT A TRANSCRIPTION CHOICE.  A
 * reciprocal multiply and a division disagree in the last place for most
 * operands, and the `+ 0.5f` and the truncation turn a last-place
 * disagreement into a bin index that is one out.  The differential test
 * decides it; it is stated here so that the next reader does not "tidy" it
 * back into `getSpectrumOfNearestBin`.
 *
 * THE INDEX IS `unsigned int` WHERE THE CLASS'S OWN ACCESSOR RETURNS
 * `unsigned long` (its mangling ends `Em`).  On i386 the two are the same
 * type and both expand through `fixuns_truncsfsi2` to the object's `fistpll`
 * plus the low dword; `unsigned int` is chosen because `make check64`
 * compiles this file for a target where `unsigned long` is 64 bits and the
 * conversion would stop agreeing above 2**32.
 */
static inline float
sv_bin_at(const float *spectrum, float freq, float binsPerHz)
{
	return spectrum[(unsigned int)(freq * binsPerHz + 0.5f)];
}

void
V90SpectralVerifier::checkSpecialSpectralConditions()
{
	float leftDelta, rightDelta, leftThr, rightThr;
	float ref, test1, test2;

	/*
	 * THE CLEAR IS OUTSIDE THE GUARD.  `cmpl $0x2,0x24(%edi)` at 0x45f1d
	 * and `movl $0x0,0x28(%edi)` at 0x45f21 are interleaved and the store
	 * is unconditional, so a call made before the accumulation finishes
	 * still forgets whatever was detected last time.
	 */
	word_28 = 0;

	if (accumulating != 2)
		return;

	/*
	 * --- German ISDN NT1 box.  Both deltas are measured against the same
	 * null bin, and the object reads that bin ONCE (0x45f6b computes it
	 * between the two peak bins and 0x45f97 keeps the sample in a slot),
	 * which is what a common subexpression looks like and what fixes the
	 * order the three bins are computed in: left, null, right.
	 */
	{
		float binsPerHz = 1.0f / binWidth;
		float nullBin = sv_bin_at(spectrum,
		    params->SPECTRAL_VERIFIER_ISDN_NULL_FREQ, binsPerHz);

		leftDelta = sv_bin_at(spectrum,
		    params->SPECTRAL_VERIFIER_ISDN_LEFT_PEAK_FREQ, binsPerHz)
		    - nullBin;
		rightDelta = sv_bin_at(spectrum,
		    params->SPECTRAL_VERIFIER_ISDN_RIGHT_PEAK_FREQ, binsPerHz)
		    - nullBin;
	}

	/*
	 * THE TWO THRESHOLDS ARE READ INTO LOCALS AND THAT IS NOT TIDINESS --
	 * DO NOT INLINE THEM BACK.  `leftDelta > params->SPECTRAL_VERIFIER_-
	 * ISDN_LEFT_PEAK_DELTA` is the obvious spelling, it is what stood here
	 * first, and `make period` fails on it.  GCC 3.4.2's
	 * `tree_swap_operands_p` puts a DECL last, so a plain local against a
	 * COMPONENT_REF gets SWAPPED: the THRESHOLD lands in %st(0), the
	 * branch becomes `jb`/`jae` -- "below OR UNORDERED" -- and
	 * `-mno-ieee-fp` licenses GCC not to add the parity test that would
	 * exclude the NaN.  The object branches `ja`/`jbe` with the DELTA in
	 * %st(0) at 0x460d5, 0x46553, 0x4627d and 0x4628c, so a NaN delta
	 * detects nothing there and detected all three conditions here.
	 *
	 * Making both operands DECLs stops the swap, and all six comparison
	 * sites then carry the object's own condition codes.  The modern build
	 * cannot see any of this -- GCC 13 honours IEEE for `>` whichever
	 * operand order it picks -- so the period tier is what holds it, and
	 * the mutation that inlines them back is pre-registered as
	 * uncatchable-here for exactly that reason.  Findings 3529, 2300, 1990.
	 */
	leftThr = params->SPECTRAL_VERIFIER_ISDN_LEFT_PEAK_DELTA;
	rightThr = params->SPECTRAL_VERIFIER_ISDN_RIGHT_PEAK_DELTA;

	edprintf("V90SpectralVerifier: German ISDN NT1 box: left peak "
		 "delta = %c%d.%02d\r\n", SV_PRINT_SIGN(leftDelta),
		 SV_PRINT_WHOLE(leftDelta), SV_FRAC2_REV(leftDelta));
	edprintf("V90SpectralVerifier: German ISDN NT1 box: right peak "
		 "delta = %c%d.%02d\r\n", SV_PRINT_SIGN(rightDelta),
		 SV_PRINT_WHOLE(rightDelta), SV_FRAC2(rightDelta));

	if (leftDelta > leftThr && rightDelta > rightThr) {
		word_28 = 1;
		edprintf("V90SpectralVerifier: German ISDN NT1 box conditions "
			 "detected!\r\n");
	}

	/* --- German PBX.  The same shape over the second set of frequencies. */
	{
		float binsPerHz = 1.0f / binWidth;
		float nullBin = sv_bin_at(spectrum,
		    params->SPECTRAL_VERIFIER_GERMAN_PBX_NULL_FREQ, binsPerHz);

		leftDelta = sv_bin_at(spectrum,
		    params->SPECTRAL_VERIFIER_GERMAN_PBX_LEFT_PEAK_FREQ,
		    binsPerHz) - nullBin;
		rightDelta = sv_bin_at(spectrum,
		    params->SPECTRAL_VERIFIER_GERMAN_PBX_RIGHT_PEAK_FREQ,
		    binsPerHz) - nullBin;
	}

	/* Locals for the reason the ISDN pair gives.  Do not inline. */
	leftThr = params->SPECTRAL_VERIFIER_GERMAN_PBX_LEFT_PEAK_DELTA;
	rightThr = params->SPECTRAL_VERIFIER_GERMAN_PBX_RIGHT_PEAK_DELTA;

	edprintf("V90SpectralVerifier: German PBX: left peak delta = "
		 "%c%d.%02d\r\n", SV_PRINT_SIGN(leftDelta),
		 SV_PRINT_WHOLE(leftDelta), SV_FRAC2_REV(leftDelta));
	edprintf("V90SpectralVerifier: German PBX: right peak delta = "
		 "%c%d.%02d\r\n", SV_PRINT_SIGN(rightDelta),
		 SV_PRINT_WHOLE(rightDelta), SV_FRAC2(rightDelta));

	if (leftDelta > leftThr && rightDelta > rightThr) {
		word_28 = 2;
		edprintf("V90SpectralVerifier: German PBX conditions "
			 "detected!\r\n");
	}

	/*
	 * --- Severe codec.  Three absolute levels rather than two deltas,
	 * and each diagnostic prints the probe frequency as well, truncated
	 * to an `int` (`flds` then `fistpl` at 0x46385, 0x4642a and 0x464cd).
	 */
	{
		float binsPerHz = 1.0f / binWidth;

		ref = sv_bin_at(spectrum,
		    params->SPECTRAL_VERIFIER_SEVERE_CODEC_REF_FREQ, binsPerHz);
		test1 = sv_bin_at(spectrum,
		    params->SPECTRAL_VERIFIER_SEVERE_CODEC_TEST_FREQ1,
		    binsPerHz);
		test2 = sv_bin_at(spectrum,
		    params->SPECTRAL_VERIFIER_SEVERE_CODEC_TEST_FREQ2,
		    binsPerHz);
	}

	edprintf("V90SpectralVerifier: Severe Codec: ref freq (%d)  = "
		 "%c%d.%02d\r\n",
		 (int)params->SPECTRAL_VERIFIER_SEVERE_CODEC_REF_FREQ,
		 SV_PRINT_SIGN(ref), SV_PRINT_WHOLE(ref), SV_FRAC2(ref));
	edprintf("V90SpectralVerifier: Severe Codec: test freq1 (%d)  = "
		 "%c%d.%02d\r\n",
		 (int)params->SPECTRAL_VERIFIER_SEVERE_CODEC_TEST_FREQ1,
		 SV_PRINT_SIGN(test1), SV_PRINT_WHOLE(test1),
		 SV_FRAC2(test1));
	edprintf("V90SpectralVerifier: Severe Codec: test freq2 (%d)  = "
		 "%c%d.%02d\r\n",
		 (int)params->SPECTRAL_VERIFIER_SEVERE_CODEC_TEST_FREQ2,
		 SV_PRINT_SIGN(test2), SV_PRINT_WHOLE(test2),
		 SV_FRAC2(test2));

	/*
	 * ONE THRESHOLD FOR BOTH ARMS.  0x464f5 loads
	 * `SEVERE_CODEC_DELTA` once and keeps it in %st across the second
	 * compare (`fcom %st(1)` then `fcompp`), which is why the first
	 * compare reads with its operands the other way round; that is a
	 * common subexpression and not a different test.
	 */
	if (ref - test1 > params->SPECTRAL_VERIFIER_SEVERE_CODEC_DELTA
	    && ref - test2 > params->SPECTRAL_VERIFIER_SEVERE_CODEC_DELTA) {
		word_28 = 3;
		edprintf("V90SpectralVerifier: Severe Codec conditions "
			 "detected!\r\n");
	}

	/*
	 * THE LAST DIAGNOSTIC IS THE ONLY GATED ONE AND IT IS A TAIL CALL.
	 * 0x46534 writes the string pointer over the incoming `this` slot at
	 * 0x90(%esp), unwinds, and `jmp`s to `dsplibs_debug_printf`, which
	 * GCC emits only for a sibling call in tail position -- so this must
	 * stay the last statement.  It goes through `dsplibs_debug_printf`
	 * behind `DSPLIB_DEBUG_ON()` where the other nine go through
	 * `edprintf`, which does its own gating after formatting; the file
	 * comment above has the reason.
	 */
	if (word_28 == 0 && DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90SpectralVerifier: No special "
				     "conditions\r\n");
}

/*
 * The same two decimals `SV_FRAC2` prints, with a `float` hundred: 0x45e25
 * is `fmuls .rodata.cst4+0x3b0`, a FOUR-byte pool entry, where the five sites
 * in `checkSpecialSpectralConditions` load eight bytes out of `.rodata.cst8`.
 * A four-byte entry does not distinguish `100`, `100.0f` and `100.0` on its
 * own (finding 1384), but an eight-byte one rules a `float` out -- so the two
 * really are different expressions in the original, and folding this into
 * `SV_FRAC2` would move the multiply's width at the site that has it narrow.
 *
 * The subtraction runs `v - (float)(int)v`: 0x45e23 is `d8 e9`, FSUBR
 * ST(0),ST(i), which is a D8 REGISTER form and therefore not one of the
 * encodings finding 245 warns about -- the swap is in the DE pop forms only.
 */
#define SV_FRAC2F(v)	sv_abs((int)(((v) - (float)(int)(v)) * 100.0f))

/*
 * THE RULE IS PRINTED TWICE FROM ONE STRING.  0x45ddb and 0x45ea4 both carry
 * `.rodata.str1.4+0xbcb4`, and the trailing one is a SIBLING CALL: 0x45ea9
 * writes the pointer over the incoming `this` slot, unwinds and `jmp`s to
 * `edprintf`, which GCC emits only in tail position -- so the closing rule
 * has to stay the last statement here.
 *
 * `Spectrum[%d]` IS A FREQUENCY IN Hz AND NOT THE BIN NUMBER.  The first
 * conversion is `(bin * binWidth) + 0.5f` truncated (0x45e70..0x45e8a), which
 * is the bin's centre frequency rounded to nearest; the loop counter itself
 * is never printed.  Read the log accordingly.
 *
 * `fftLength / 2` IS RE-READ EVERY ITERATION -- 0x45e97 loads +0x0c again --
 * because `edprintf` is an opaque call between the two uses, so the bound is
 * written in the condition rather than hoisted into a local.
 */
void
V90SpectralVerifier::printSpectrum() const
{
	unsigned int bin;

	edprintf("--------------------------------------------------------"
		 "\r\n");

	for (bin = 0; bin < fftLength / 2; bin++) {
		float v = spectrum[bin];

		edprintf("V90SpectralVerifier: Spectrum[%d] = %c%d.%02d\r\n",
			 (int)(bin * binWidth + 0.5f), SV_PRINT_SIGN(v),
			 SV_PRINT_WHOLE(v), SV_FRAC2F(v));
	}

	edprintf("--------------------------------------------------------"
		 "\r\n");
}

/*
 * ===========================================================================
 * process -- 268 bytes, 0x465b0
 *
 * Fill the accumulation buffer from the caller's block, and on the sample
 * that fills it run the periodogram and classify the line.  Returns 1 on
 * that one call and 0 on every other, including every call made outside
 * state 1.
 *
 * THE TWO LOOP CONDITIONS ARE IF-CONVERTED AND THAT IS THE COMPILER'S.
 * 0x465fb..0x46607 is `setb`/`setb`/`test`/`je` -- two compares with no
 * branch between them -- which is what GCC 3.4.2 does to a `&&` whose arms
 * are both cheap (finding 2411's class).  The source is an ordinary `&&`.
 *
 * THE PROGRESS COUNTER IS STORED ONLY ON THE PATH THAT LEAVES THE LOOP FROM
 * INSIDE IT (0x46647).  Entering with the buffer already full skips the
 * store, which is a loop rotation and unobservable: on that path the value
 * being stored is the one already there.
 *
 * THE STATE MOVES TO 2 BEFORE THE PERIODOGRAM RUNS (0x46659, ahead of the
 * call at 0x4667c), so a `Psd::process` that re-entered this object would
 * find it finished rather than running.  It does not, but the order is the
 * object's and is kept.
 * ===========================================================================
 */
int
V90SpectralVerifier::process(float *in, unsigned int count)
{
	unsigned int filled, i;

	if (accumulating != 1)
		return 0;

	filled = accumCount;
	for (i = 0; filled < psdLength && i < count; i++)
		buf_18[filled++] = in[i];
	accumCount = filled;

	if (filled < psdLength)
		return 0;

	accumulating = 2;

	psd->process(buf_18, psdLength, spectrum, Psd::OUTPUT_DB);

	edprintf("V90SpectralVerifier: Ready\r\n");

	/*
	 * `SPECTRAL_VERIFIER_PRINT_SPECTRUM` (+0x2c0) is read through the
	 * object's own `params` (0x46695 reloads it from `(%edx)`), not
	 * through a register still holding it, so the source names the
	 * member.
	 */
	if (params->SPECTRAL_VERIFIER_PRINT_SPECTRUM != 0)
		printSpectrum();

	checkSpecialSpectralConditions();

	return 1;
}

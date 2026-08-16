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
extern void psd_destruct(void *self) asm("_ZN3PsdD1Ev");

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
	if (buf_18 != 0)
		sysdep_free(buf_18);

	if (spectrum != 0)
		sysdep_free(spectrum);

	if (psd != 0) {
		psd_destruct(psd);
		sysdep_free(psd);
	}
}

void
V90SpectralVerifier::reset()
{
	edprintf("V90SpectralVerifier: Reset\r\n");

	accumCount = 0;
	accumulating = 0;
	word_28 = 0;
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
	float leftDelta, rightDelta;
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

	edprintf("V90SpectralVerifier: German ISDN NT1 box: left peak "
		 "delta = %c%d.%02d\r\n", SV_PRINT_SIGN(leftDelta),
		 SV_PRINT_WHOLE(leftDelta), SV_FRAC2_REV(leftDelta));
	edprintf("V90SpectralVerifier: German ISDN NT1 box: right peak "
		 "delta = %c%d.%02d\r\n", SV_PRINT_SIGN(rightDelta),
		 SV_PRINT_WHOLE(rightDelta), SV_FRAC2(rightDelta));

	if (leftDelta > params->SPECTRAL_VERIFIER_ISDN_LEFT_PEAK_DELTA
	    && rightDelta > params->SPECTRAL_VERIFIER_ISDN_RIGHT_PEAK_DELTA) {
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

	edprintf("V90SpectralVerifier: German PBX: left peak delta = "
		 "%c%d.%02d\r\n", SV_PRINT_SIGN(leftDelta),
		 SV_PRINT_WHOLE(leftDelta), SV_FRAC2_REV(leftDelta));
	edprintf("V90SpectralVerifier: German PBX: right peak delta = "
		 "%c%d.%02d\r\n", SV_PRINT_SIGN(rightDelta),
		 SV_PRINT_WHOLE(rightDelta), SV_FRAC2(rightDelta));

	if (leftDelta
	      > params->SPECTRAL_VERIFIER_GERMAN_PBX_LEFT_PEAK_DELTA
	    && rightDelta
	      > params->SPECTRAL_VERIFIER_GERMAN_PBX_RIGHT_PEAK_DELTA) {
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

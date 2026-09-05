/*
 * GenericToneDetector.cpp -- the constructor and the destructor.
 *
 * `include/dsplib/GenericToneDetector.h` carries the object map, the 60-byte
 * measurement, the eleven arguments and where each of them goes.  This file is
 * the two members and the three things about them that are not visible from
 * the field list.
 *
 * PLAIN CDECL with `this` as the first STACK argument (finding F215).  The
 * constructor's prologue is `sub $0x2c,%esp` after no push, then four
 * callee-saved registers written to the frame by hand -- `-fomit-frame-pointer`
 * and `-maccumulate-outgoing-args`, which is the toolchain the object was built
 * with (tools/toolchain/build.sh).  Nothing here needs a calling-convention
 * attribute, and the two `float` arguments occupy one four-byte stack slot each
 * because the call is prototyped.
 *
 * ---------------------------------------------------------------------------
 * THE FILTER IS ALLOCATED WITH `new` AND FREED WITH `delete`, and the reason
 * that is the reading rather than a hand-written malloc pair is set out at
 * length in `GenericIIR.h`, where the two allocation operators live.  In
 * summary: the constructor calls `sysdep_malloc` DIRECTLY with 0x34 -- which is
 * `sizeof(GenericIIR<float, double>)` -- and then the filter's `C1` with NO
 * null check between them, which is what a plain `operator new` gives; and the
 * destructor tests for null, calls `D1`, then calls `sysdep_free`, which is
 * what a delete-expression gives and a hand-written free would not.
 *
 * ---------------------------------------------------------------------------
 * THE CONSTRUCTOR'S TAIL IS `reset()` INLINED, and it is now spelled that way.
 * `_ZN19GenericToneDetector5resetEv` is in the blob at 0x10640, 68 bytes, and
 * its body is: call `GenericIIR<float,double>::reset` on +0x00, then zero
 * +0x2c, +0x0c, +0x10, +0x14, +0x18, +0x30, +0x24, +0x38.  The constructor
 * ends with a call to the SAME `GenericIIR` member and stores to exactly those
 * eight fields and no others -- in a different order, which is scheduling and
 * free (CLAUDE.md).  Set intersection over eight fields and one call is not a
 * coincidence, so the original's constructor said `reset();` and GCC inlined
 * it.
 *
 * It was written out here while `reset()` was a later batch.  That batch has
 * landed, the tail is the call, and the generated code is the same code.
 *
 * ---------------------------------------------------------------------------
 * ROUNDING UP IS A MULTIPLY, NOT A REMAINDER.  Both duration arguments are
 * converted with
 *
 *     div  %edi                ; n = samples / blockLen
 *     mov  %eax,%ecx
 *     imul %edi,%eax           ; n * blockLen
 *     cmp  %ebx,%eax
 *     jae  ...                 ; store n
 *     lea  0x1(%ecx),%ebx      ; else store n + 1
 *
 * -- and the `imul` is the point.  The remainder of that same division is
 * already sitting in %edx, so a source that had said `if (samples % blockLen)`
 * would have tested %edx and needed no multiply at all.  The object multiplies
 * the quotient back out, so the source multiplied.  The two forms agree over
 * every input, which is exactly why the instruction and not the behaviour is
 * what settles which one was written.
 *
 * The division is NOT guarded against a zero `blockLen`.  Reproduced; see
 * docs/deviations.md.
 */

#include <stddef.h>

#include "dsplib/GenericIIR.h"
#include "dsplib/GenericToneDetector.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but parses only `struct name {`, so a C++ class asserts its
 * own -- and this is the check that catches an object right in size and wrong
 * in its offsets.
 *
 * GUARDED ON THE POINTER WIDTH, because the object leads with one and
 * `make check64` compiles this file for the native target, where it is eight
 * bytes and every offset after it moves.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define GTD_OFF(field, off, tag) \
	typedef char gtd_off_##tag[ \
	    ((int)__builtin_offsetof(GenericToneDetector, field) \
	     == (off)) ? 1 : -1]

GTD_OFF(filter,      0x00, filter);
GTD_OFF(threshold,   0x04, threshold);
GTD_OFF(ratio,       0x08, ratio);
GTD_OFF(acc_0c,      0x0c, acc0c);
GTD_OFF(acc_10,      0x10, acc10);
GTD_OFF(acc_14,      0x14, acc14);
GTD_OFF(acc_18,      0x18, acc18);
GTD_OFF(blocks1,     0x1c, blocks1);
GTD_OFF(blocks2,     0x20, blocks2);
GTD_OFF(sampleCount, 0x24, samplecount);
GTD_OFF(blockLen,    0x28, blocklen);
GTD_OFF(count_2c,    0x2c, count2c);
GTD_OFF(count_30,    0x30, count30);
GTD_OFF(flag,        0x34, flag);
GTD_OFF(detected,    0x38, detected);
typedef char gtd_size[(sizeof(GenericToneDetector) == 0x3c) ? 1 : -1];
#endif

GenericToneDetector::GenericToneDetector(unsigned int nden, unsigned int nnum,
					 double *den, double *num,
					 unsigned int samples1,
					 unsigned int samples2,
					 float threshold_, unsigned int flag_,
					 float ratio_, unsigned int blockLen_,
					 unsigned int blockSize)
{
	unsigned int n;

	filter = new GenericIIR<float, double>(nden, nnum, den, num, blockSize);

	blockLen = blockLen_;
	threshold = threshold_;
	ratio = ratio_;

	n = samples1 / blockLen_;
	if (n * blockLen_ < samples1)
		n++;
	blocks1 = n;

	n = samples2 / blockLen_;
	if (n * blockLen_ < samples2)
		n++;
	blocks2 = n;

	flag = flag_;

	reset();
}

/*
 * ONE CALL AND EIGHT ZEROES, in the object's own order: +0x2c first, then the
 * four accumulators out of one zeroed register, then +0x30, +0x24, +0x38.
 * Storing an integer zero into a `float` is what the compiler does for `= 0`
 * and says nothing about the field's type; GenericToneDetector.h reads those
 * types off the `fadds`/`fsts` in `process` instead.
 *
 * What it does NOT touch is the whole of the configuration -- `threshold`,
 * `ratio`, `blocks1`, `blocks2`, `blockLen` and `flag` -- so a reset detector
 * is the same detector, and the test asserts that rather than only asserting
 * the eight zeroes.
 */
void GenericToneDetector::reset()
{
	filter->reset();

	count_2c = 0;
	acc_0c = 0;
	acc_10 = 0;
	acc_14 = 0;
	acc_18 = 0;
	count_30 = 0;
	sampleCount = 0;
	detected = 0;
}

/*
 * ONE SAMPLE.  0x10350, 318 bytes.
 *
 * The sample goes through the filter; the block accumulates the square of the
 * INPUT at +0x0c and of the OUTPUT at +0x10; and on the `blockLen`th sample
 * both are turned into means, smoothed into +0x14 and +0x18, and scored.
 *
 * ---------------------------------------------------------------------------
 * WHERE THE ROUNDING HAPPENS, because it is not uniform and the object is
 * explicit about it.  `-mfpmath=387` with GCC's default `-fexcess-precision=
 * fast`, so a value stays at 80 bits until something stores it.
 *
 *   - `in` and `out` are stored to +0x0c/+0x10 and reloaded next sample, so
 *     the accumulators round to `float` ONCE PER SAMPLE.  `fstps` on the
 *     not-a-boundary arm, `fadds` on the way back in.
 *   - on a boundary they are NOT stored -- the object writes zero to both --
 *     so the means are taken from the UNROUNDED 80-bit sums.  That is why
 *     they are locals here and not `acc_0c += ...`: the store has to be on
 *     one arm only, exactly as `fstps 0xc(%ebx)` is.
 *   - `acc_14` and `acc_18` are stored with `fsts`, which does NOT pop, and
 *     the ratio test that follows uses the register.  So the comparison is
 *     made at 80 bits against the value the store rounded.
 *
 * ---------------------------------------------------------------------------
 * THE RECIPROCAL IS A RECIPROCAL.  `d8 3d` is `D8 /7`, FDIVR against a
 * `float` in `.rodata.cst4` holding 1.0f: ST(0) = 1.0f / ST(0).  So the object
 * forms 1/blockLen once and MULTIPLIES by it twice; `in / sampleCount` would
 * be a different answer in the last place and is not what is written here.
 * (Read from the ModR/M byte and not from the mnemonic -- finding F245 is about
 * the popping forms, and this is the memory form, but the rule is the rule.)
 *
 * The count is converted with `push $0; push %ecx; fildll`, a 64-bit load of a
 * zero-extended `sampleCount`, which is what an UNSIGNED-to-float conversion
 * compiles to.  Keeping the operand `unsigned` is what keeps that encoding.
 *
 * ---------------------------------------------------------------------------
 * 0.7 AND 0.3 ARE `float` CONSTANTS at `.rodata.cst4` +0x60 and +0x64, loaded
 * with `flds`/`fmuls`.  The smoother is a one-pole low pass with a 0.3
 * coefficient, and the same pair is spelled again at +0x6c/+0x70 for the array
 * overload -- separate slots, which is one of the things saying the two bodies
 * are separate code.
 *
 * ---------------------------------------------------------------------------
 * THE SCORING, and the one shape in it that is not symmetric.  A block scores
 * as a hit when the mean output reaches `threshold` and either `flag` is clear
 * or the output smoother leads the input smoother by `ratio`.  A hit advances
 * +0x2c and clears +0x30; a miss advances +0x30, and +0x30 reaching `blocks2`
 * withdraws the answer.  `count_2c >= blocks1` then sets it -- but ONLY on the
 * arm the threshold passed.  The below-threshold arm jumps straight to the
 * per-block cleanup at 0x10420 and never loads `blocks1` at all.  Reproduced
 * as written; deviation D320 says what it costs.
 */
int GenericToneDetector::process(float sample)
{
	float y = filter->process(sample);
	float in = acc_0c + sample * sample;
	float out = acc_10 + y * y;

	if (++sampleCount != blockLen) {
		acc_0c = in;
		acc_10 = out;
		return (int)detected;
	}

	{
		float inv = 1.0f / (float)sampleCount;
		float meanIn = in * inv;
		float meanOut = out * inv;

		acc_14 = 0.7f * acc_14 + 0.3f * meanIn;
		acc_18 = 0.7f * acc_18 + 0.3f * meanOut;

		if (meanOut >= threshold) {
			if (flag == 0 || acc_18 > ratio * acc_14) {
				count_2c++;
				count_30 = 0;
			} else if (++count_30 >= blocks2) {
				count_2c = 0;
				detected = 0;
			}
			if (count_2c >= blocks1)
				detected = 1;
		} else if (++count_30 >= blocks2) {
			count_2c = 0;
			detected = 0;
		}

		sampleCount = 0;
		acc_0c = 0;
		acc_10 = 0;
	}

	return (int)detected;
}

/*
 * `n` SAMPLES.  0x10490, 422 bytes.
 *
 * Everything above about rounding, about the reciprocal and about the
 * bookkeeping holds here word for word -- the accumulation is the same, the
 * smoother is the same, the constants are the same values in different
 * literal-pool slots (`.rodata.cst4` +0x68/+0x6c/+0x70 against +0x5c/+0x60/
 * +0x64), and the hit and miss arms are the same arms.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS DIFFERENT IS A THIRD ARM, and it is why these are two functions and
 * not one called twice.  0x105a0..0x105d8:
 *
 *     fmuls  0x74(.rodata.cst4)     ; 0.5f * threshold
 *     fcomp  %st(3)                 ; against the block's mean output
 *     ja     ...                    ; short of half -- a miss
 *     mov    0x34(%ebx),%ecx        ; flag
 *     test   %ecx,%ecx
 *     je     ...                    ; clear -- a miss
 *     fmull  0x18(.rodata.cst8)     ; acc_14 * 0.85, a DOUBLE
 *     fcompp                        ; against acc_18
 *     jae    ...                    ; acc_18 no higher -- a miss
 *     ...                           ; otherwise a HIT
 *
 * So a block whose mean output fell short of `threshold` but reached HALF of
 * it still scores, provided `flag` is set and the output smoother stands more
 * than 0.85 of the input smoother.  `process(float)` has none of this: no
 * 0.5f, no 0.85, no third comparison.  0x105ba is the only `fmull` in the
 * class and 0.85 is the only double constant it uses.
 *
 * ---------------------------------------------------------------------------
 * THE THREE COMPARISONS ARE SPELLED THE WAY THE BRANCHES ARE, and NaN is the
 * reason.  `ja` after `fcomp` is false when the compare is unordered, so
 * `0.5f * threshold > meanOut` sending a block to the miss arm keeps a NaN
 * mean OUT of that arm; and `jae` on `acc_14 * 0.85 >= acc_18` likewise sends
 * a NaN to the HIT.  Writing the second as `acc_18 > acc_14 * 0.85` would read
 * identically over every ordered input and take the other arm on a NaN, which
 * an accumulator that has reached infinity produces.  The test drives it.
 *
 * ---------------------------------------------------------------------------
 * THE WEAK ARM'S MISS DOES NOT REACH `blocks1`, exactly as the below-threshold
 * arm in the other overload does not -- 0x105e4 falls into the per-block
 * cleanup at 0x10600 while the strong arm's miss at 0x10572 goes to 0x10625
 * and loads it.  Deviation D320.
 *
 * `n == 0` returns the answer at +0x38 without touching the filter: the object
 * tests it at 0x104a2 before anything else.
 */
int GenericToneDetector::process(float *samples, unsigned int n)
{
	unsigned int k;

	for (k = 0; k < n; k++) {
		float sample = *samples++;
		float y = filter->process(sample);
		float in = acc_0c + sample * sample;
		float out = acc_10 + y * y;

		if (++sampleCount != blockLen) {
			acc_0c = in;
			acc_10 = out;
			continue;
		}

		{
			float inv = 1.0f / (float)sampleCount;
			float meanIn = in * inv;
			float meanOut = out * inv;

			acc_14 = 0.7f * acc_14 + 0.3f * meanIn;
			acc_18 = 0.7f * acc_18 + 0.3f * meanOut;

			if (meanOut >= threshold) {
				if (flag == 0 || acc_18 > ratio * acc_14) {
					count_2c++;
					count_30 = 0;
				} else if (++count_30 >= blocks2) {
					count_2c = 0;
					detected = 0;
				}
				if (count_2c >= blocks1)
					detected = 1;
			} else if (threshold * 0.5f > meanOut || flag == 0 ||
				   acc_14 * 0.85 >= acc_18) {
				if (++count_30 >= blocks2) {
					count_2c = 0;
					detected = 0;
				}
			} else {
				count_30 = 0;
				count_2c++;
				if (count_2c >= blocks1)
					detected = 1;
			}

			sampleCount = 0;
			acc_0c = 0;
			acc_10 = 0;
		}
	}

	return (int)detected;
}

/*
 * One `delete`, and nothing else: the object is not written, so `filter` is
 * left dangling rather than nulled.  The test asserts that, because "the
 * destructor leaves the object alone" is a claim about forty bytes of code and
 * not an absence of one.
 */
GenericToneDetector::~GenericToneDetector()
{
	delete filter;
}

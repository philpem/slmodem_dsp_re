/*
 * GenericToneDetector.h -- an energy-in-band tone detector built on one
 * `GenericIIR<float, double>`.
 *
 * Reconstructed from dsplibs.o.  Five members, and ALL FIVE are now written:
 * the constructor (0x10690, 267 bytes), the destructor (0x102f0, 40 bytes),
 * `reset` (0x10640, 68 bytes), `process(float)` (0x10350, 318 bytes) and
 * `process(float *, unsigned)` (0x10490, 422 bytes).
 *
 * ---------------------------------------------------------------------------
 * WHAT THE CLASS DOES, now that the three that do it are read.  It is a
 * two-counter hysteresis machine over blocks of `blockLen` samples.  Each
 * sample is run through the filter; the block accumulates the mean square of
 * the INPUT at +0x0c and of the FILTER OUTPUT at +0x10.  At the end of a block
 * both are turned into means by multiplying by 1/blockLen, and each is fed
 * into a one-pole smoother -- `0.7f * old + 0.3f * mean` -- at +0x14 (input)
 * and +0x18 (output).  The block then scores as a HIT or a MISS; a hit
 * advances +0x2c and clears +0x30, a miss advances +0x30, and +0x30 reaching
 * +0x20 clears +0x2c and the answer.  +0x2c reaching +0x1c sets the answer.
 * So +0x1c is "how many good blocks in a row declare the tone" and +0x20 is
 * "how many bad blocks in a row withdraw it".
 *
 * ---------------------------------------------------------------------------
 * THE TWO `process` OVERLOADS ARE NOT THE SAME ALGORITHM, and that is the
 * single most important thing about this class.  They agree exactly on the
 * accumulation, on the smoother, on both constants and on the hit/miss
 * bookkeeping, and they differ in how a block SCORES:
 *
 *   process(float)          hit iff  mean_out >= threshold  AND
 *                                    (flag == 0 || acc_18 > ratio * acc_14)
 *
 *   process(float *, n)     the same, PLUS a second, weaker arm: a block
 *                           whose mean_out fell short of `threshold` but
 *                           reached HALF of it still scores as a hit when
 *                           `flag` is set and acc_18 exceeds 0.85 * acc_14.
 *
 * The weak arm is 0x105a0..0x105d8 and it exists in the array overload only:
 * `process(float)` has no `0.5f`, no `0.85` and no third comparison, and the
 * two functions reference SEPARATE literal-pool slots for the constants they
 * do share (0x5c/0x60/0x64 against 0x68/0x6c/0x70 in `.rodata.cst4`).  So
 * they are not one body called twice and must not be factored into one here.
 *
 * They also differ in one piece of bookkeeping, which is recorded as a
 * deviation rather than smoothed over: see docs/deviations.md.
 *
 * NOT POLYMORPHIC: the destructor appears with the `D1` and `D2` variants and
 * no `D0`, and GCC emits a deleting destructor only for a virtual one, so
 * offset 0 is a real member and there is no vptr.
 *
 * WHICH TRANSLATION UNIT THIS BELONGED TO IS NOT KNOWN.  In the blob the
 * class sits between `K56FlexFloModem`'s stubs and `ANSamToneDetector`, and
 * neither neighbour settles it; `src/dsp/` is where its one collaborator,
 * `GenericIIR`, lives, so that is where the .cpp went.  `compare.py`'s
 * per-object rollup will attribute it to whatever file holds it (finding
 * F610), so read that number knowing the placement is a choice and not a
 * measurement.
 *
 * ---------------------------------------------------------------------------
 * THE OBJECT IS 60 BYTES (0x3c), and all five members agree.  The largest
 * `this`-relative displacement any of them uses is +0x38 and every access
 * there is four bytes wide -- `movl $0x0,0x38(%ebx)` and `movl $0x1,...` in
 * both `process`es, `mov 0x38(%ebx),%eax` where the result is returned -- so
 * the object ends at 0x3c.  A displacement is not a size (finding F215); the
 * width of what sits at the bound is what turns one into the other.
 *
 * The FIELD TYPES below are read off the instructions that touch them, not
 * guessed from the names.  A slot the constructor only copies is typed from
 * the constructor's own argument list, which the mangling gives exactly:
 * `_ZN19GenericToneDetectorC1EjjPdS0_jjfjfjj` is
 * (unsigned, unsigned, double *, double *, unsigned, unsigned, float,
 *  unsigned, float, unsigned, unsigned).
 *
 * THE TWO FLOATS ARE COPIED, NOT COMPUTED WITH.  The constructor moves them
 * with `mov %ecx,0x4(%esi)` and `mov %edx,0x8(%esi)` -- 32-bit integer moves,
 * never the x87 stack -- so the constructor performs no rounding and cannot
 * turn a denormal into a zero or an inexact literal into a different one.
 * The test sweeps zero, both signs, a denormal, an exactly representable
 * value and one that is not; what that sweep proves is that the COPY is
 * bit-exact, and it is deliberately not evidence about floating-point
 * arithmetic anywhere in this class.  The `process`es do use the x87 stack on
 * +0x04 and +0x08, but they are not reconstructed here.
 *
 * THE CONSTRUCTOR DIVIDES BY ITS TENTH ARGUMENT AND DOES NOT GUARD IT.  Both
 * `div %edi` sites take `blockLen` straight from the argument, so a zero
 * traps.  That is the object's behaviour and it is reproduced, not repaired
 * (docs/deviations.md).  The test therefore sweeps `blockLen` over nonzero
 * values only, and says so rather than avoiding zero quietly.
 */

#ifndef DSPLIB_GENERICTONEDETECTOR_H
#define DSPLIB_GENERICTONEDETECTOR_H

#include "dsplib/GenericIIR.h"

class GenericToneDetector {
public:
	/**
	 * @brief Construct a tone detector over a newly built IIR filter.
	 *
	 * Eleven arguments, spent in three places: five go straight to the
	 * `GenericIIR<float, double>` filter this allocates, four are copied
	 * into the object as they stand, and two are divided by a fifth
	 * (rounded up) before being stored. Argument names describe what the
	 * object does with each, which for a store-only slot is all that is
	 * recoverable (finding F226: the mangling preserves method and type
	 * names, never a data member's or a parameter's).
	 *
	 * @param nden       Filter denominator order, passed through to
	 *                   GenericIIR.
	 * @param nnum       Filter numerator order, passed through.
	 * @param den        Filter denominator coefficients, passed through.
	 * @param num        Filter numerator coefficients, passed through.
	 * @param samples1   Hit-streak duration in samples; divided by
	 *                   @p blockLen (rounded up) and stored as `blocks1`.
	 * @param samples2   Miss-streak duration in samples; divided by
	 *                   @p blockLen (rounded up) and stored as `blocks2`.
	 * @param threshold  Output-energy threshold a block's mean must reach
	 *                   to score as a hit; stored as `threshold`.
	 * @param flag       Enables the ratio-based scoring arm; stored as
	 *                   `flag` and tested against zero by process().
	 * @param ratio      Input/output energy ratio used only on the path
	 *                   @p flag enables; stored as `ratio`.
	 * @param blockLen   Samples per scoring block; stored as `blockLen`
	 *                   and also used as the divisor for @p samples1 and
	 *                   @p samples2 -- not guarded against zero (a zero
	 *                   value traps in the divide, reproducing the
	 *                   object; see docs/deviations.md).
	 * @param blockSize  Passed to the filter as its block size; never
	 *                   stored in this object.
	 */
	GenericToneDetector(unsigned int nden, unsigned int nnum,
			    double *den, double *num,
			    unsigned int samples1, unsigned int samples2,
			    float threshold, unsigned int flag, float ratio,
			    unsigned int blockLen, unsigned int blockSize);

	/**
	 * @brief Destroy the detector and its owned filter.
	 *
	 * Tests `filter` for null, destroys it, and frees it -- exactly what
	 * a delete-expression compiles to for GenericIIR's member `operator
	 * delete`. Does not null `filter` afterwards.
	 */
	~GenericToneDetector();

	/**
	 * @brief Reset the filter and the detector's running state.
	 *
	 * Resets the owned filter (GenericIIR::reset()), then clears the
	 * four accumulators, the two block counters, the sample counter and
	 * the answer -- eight fields. Does NOT touch `blocks1`, `blocks2`,
	 * `blockLen`, `threshold`, `ratio` or `flag`, which are configuration
	 * and survive a reset.
	 */
	void reset();

	/**
	 * @brief Score one sample and report the current verdict.
	 *
	 * Accumulates @p sample into the current block, and at the end of a
	 * block scores it as a hit or miss (see the file comment above for
	 * the exact rule) and updates the hit/miss streak counters and the
	 * answer accordingly.
	 *
	 * @param sample  The next input sample.
	 * @return The detector's current answer (1 tone detected, 0 not),
	 *         as it stands after this sample.
	 */
	int process(float sample);

	/**
	 * @brief Score @p n samples and report the verdict after the last one.
	 *
	 * Same accumulation and bookkeeping as process(float), plus a second,
	 * weaker scoring arm enabled by `flag` (see the file comment above --
	 * the two overloads are NOT the same algorithm). `n == 0` returns the
	 * current answer without touching the filter at all.
	 *
	 * The only caller in the object is `VPcmV34Progress`, running the
	 * `ANSamToneDetector` embedded in `VPcmFloModem` on the modem-on-hold
	 * arm. `DSPLIB_GTD_UNWRITTEN` used to mark this overload's reference
	 * weak in that caller while it was undefined; it is defined here as
	 * nothing and kept only because `v34pcmmain.cpp` still spells it.
	 *
	 * @param samples  Input samples.
	 * @param n        Number of samples in @p samples.
	 * @return The detector's current answer (1 tone detected, 0 not).
	 */
#ifndef DSPLIB_GTD_UNWRITTEN
#define DSPLIB_GTD_UNWRITTEN
#endif
	int process(float *samples, unsigned int n) DSPLIB_GTD_UNWRITTEN;

	/*
	 * Public for `offsetof`; the original's access specifiers are not
	 * recoverable, and one access section is what keeps `offsetof`
	 * meaningful.  See V90ConstellationDesigner.h.
	 */

	/*
	 * +0x00  The filter, OWNED.  `sysdep_malloc(0x34)` in the constructor
	 * and `sysdep_free` in the destructor, and 0x34 is 52, which is
	 * `sizeof(GenericIIR<float, double>)` exactly.  `reset` and both
	 * `process`es dereference it with `mov (%ebx),%edx` and pass the
	 * result as the first argument of a `GenericIIR` member, which is what
	 * makes it that pointer type rather than four opaque bytes.
	 */
	GenericIIR<float, double> *filter;

	/*
	 * +0x04 and +0x08  The two float arguments, stored as they arrive.
	 * `process(float)` reads +0x04 with `fcomps 0x4(%ebx)` and +0x08 with
	 * `flds 0x8(%ebx)`, which is what types them `float` rather than
	 * `int`: a four-byte x87 load is not something the compiler chooses.
	 */
	float threshold;		/* +0x04 = argument 7  */
	float ratio;			/* +0x08 = argument 9  */

	/*
	 * +0x0c .. +0x18  Four accumulators the constructor and `reset` clear.
	 * The constructor writes them with `mov %ebp,` where %ebp is an
	 * integer zero, but `process` reads all four with `fadds`/`flds` and
	 * writes them with `fsts`/`fstps`, so they are floats whose cleared
	 * state is the all-zero-bits pattern -- which for IEEE 754 is +0.0f.
	 * Clearing a float by storing an integer zero is what the compiler
	 * does for `= 0`, so nothing here is unusual; it is recorded because
	 * the store instruction alone would suggest `int`.
	 */
	float acc_0c;			/* +0x0c */
	float acc_10;			/* +0x10 */
	float acc_14;			/* +0x14 */
	float acc_18;			/* +0x18 */

	/*
	 * +0x1c and +0x20  `samples1` and `samples2` divided by `blockLen` and
	 * rounded up.  `process` compares the counters at +0x2c and +0x30
	 * against them (`cmp 0x1c(%ebx),%edx`, `cmp 0x20(%ebx),%ecx`) and the
	 * comparisons are UNSIGNED -- `jb`, not `jl` -- which is the
	 * signedness the compiler was forced to encode.
	 */
	unsigned int blocks1;		/* +0x1c */
	unsigned int blocks2;		/* +0x20 */

	/*
	 * +0x24  The sample counter inside the current block.  `process`
	 * increments it and compares it against +0x28 for equality, then
	 * zeroes it; the constructor and `reset` zero it.
	 */
	unsigned int sampleCount;	/* +0x24 */

	/* +0x28  `blockLen`, stored as it arrives. */
	unsigned int blockLen;		/* +0x28 = argument 10 */

	/*
	 * +0x2c and +0x30  Two block counters, compared unsigned against
	 * +0x1c and +0x20 respectively.  Both cleared by the constructor and
	 * by `reset`.
	 */
	unsigned int count_2c;		/* +0x2c */
	unsigned int count_30;		/* +0x30 */

	/* +0x34  `flag`, stored as it arrives and only ever tested against 0. */
	unsigned int flag;		/* +0x34 = argument 8  */

	/*
	 * +0x38  The detector's answer.  Set to 1 or 0 by `process`, and it is
	 * what both `process` overloads RETURN -- `mov 0x38(%ebx),%eax`
	 * immediately before the epilogue in each.  Cleared by the constructor
	 * and by `reset`.
	 */
	unsigned int detected;		/* +0x38 */
};

#endif /* DSPLIB_GENERICTONEDETECTOR_H */

/*
 * GenericToneDetector.h -- an energy-in-band tone detector built on one
 * `GenericIIR<float, double>`.
 *
 * Reconstructed from dsplibs.o.  Five members; TWO are written here, the
 * constructor (0x10690, 267 bytes) and the destructor (0x102f0, 40 bytes).
 * `reset` and the two `process` overloads are read below for the object map
 * and are NOT reconstructed.
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
 * 610), so read that number knowing the placement is a choice and not a
 * measurement.
 *
 * ---------------------------------------------------------------------------
 * THE OBJECT IS 60 BYTES (0x3c), and all five members agree.  The largest
 * `this`-relative displacement any of them uses is +0x38 and every access
 * there is four bytes wide -- `movl $0x0,0x38(%ebx)` and `movl $0x1,...` in
 * both `process`es, `mov 0x38(%ebx),%eax` where the result is returned -- so
 * the object ends at 0x3c.  A displacement is not a size (finding 215); the
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
	/*
	 * ELEVEN ARGUMENTS, and the constructor spends them in three places.
	 * Five go straight to the filter it allocates, four are copied into
	 * the object as they stand, and two are divided by a fifth before
	 * being stored.  Argument names are descriptive of what the object
	 * DOES with each, which for a store-only slot is all that is
	 * recoverable (finding 226: the mangling preserves method and type
	 * names, never a data member's or a parameter's).
	 *
	 *   nden, nnum, den, num   the filter's denominator and numerator
	 *                          orders and coefficient arrays, passed
	 *                          through untouched to
	 *                          GenericIIR<float, double>.
	 *   samples1, samples2     divided by `blockLen`, rounded UP, and
	 *                          stored at +0x1c and +0x20.  So the caller
	 *                          states two durations in samples and the
	 *                          object keeps them in whole blocks.
	 *   threshold              float, copied to +0x04.
	 *   flag                   copied to +0x34; both `process`es test it
	 *                          with `test %eax,%eax; je` and take a
	 *                          shorter path when it is zero.
	 *   ratio                  float, copied to +0x08; used by `process`
	 *                          only on the path `flag` enables.
	 *   blockLen               copied to +0x28 AND used as the divisor
	 *                          for `samples1` and `samples2`.  Not
	 *                          guarded against zero -- see the file
	 *                          comment.
	 *   blockSize              passed to the filter as its block size and
	 *                          never stored in this object.
	 */
	GenericToneDetector(unsigned int nden, unsigned int nnum,
			    double *den, double *num,
			    unsigned int samples1, unsigned int samples2,
			    float threshold, unsigned int flag, float ratio,
			    unsigned int blockLen, unsigned int blockSize);

	/*
	 * FORTY BYTES, AND IT IS ONE `delete`.  Test `filter` for null, call
	 * `_ZN10GenericIIRIfdED1Ev`, call `sysdep_free` -- which is exactly
	 * what a delete-expression compiles to when `operator delete` is the
	 * member `GenericIIR.h` derives.  It writes nothing: `filter` is left
	 * dangling rather than nulled, and the test asserts that too.
	 */
	~GenericToneDetector();

	/*
	 * `_ZN19GenericToneDetector7processEPfj`, 422 bytes, NOT WRITTEN.
	 * `VPcmV34Progress` is the only caller in the object -- it runs the
	 * `ANSamToneDetector` embedded at `VPcmFloModem + 0x6f5c` over one
	 * block on the modem-on-hold arm -- so it is declared here and defined
	 * nowhere, weak in that one translation unit.  VPcmFloModem.h's block
	 * on its own four says why at length; the arrangement is identical.
	 *
	 * The result is the detector's answer at +0x38, which both overloads
	 * return; `int` for the reason the file comment gives for the other.
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

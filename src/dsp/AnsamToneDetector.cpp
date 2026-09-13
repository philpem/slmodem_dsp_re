/*
 * AnsamToneDetector.cpp -- reconstructed from dsplibs.o.  Both members use
 * the four IIR2100 coefficient tables they choose between.
 * `include/dsplib/ANSamToneDetector.h` carries the object map, the argument
 * that the class DERIVES from `GenericToneDetector` rather than containing
 * one, and the reading of the sixth argument as a selector.
 *
 * THE TABLES ARE `double *`, NOT `const double *`.  They live in .data, not
 * .rodata, and the base passes them straight to `GenericIIR<float, double>`,
 * whose coefficient parameters are non-const -- so a `const` here would not
 * compile and would not be the object's type either.  `vpcm_tables.c` defines
 * them in the object's own order: .data+0x160, +0x1c0, +0x240, +0x2a0.
 *
 * EVERY LITERAL IN `vpcm_tables.c` ROUND-TRIPS.  Each is the shortest decimal
 * that reproduces the blob's eight bytes exactly, and the test compares the
 * arrays the two sides' filters point at, in full, on every trial -- so a
 * transcription slip in any of the 48 values is a failing test rather than a
 * plausible-looking coefficient.  That comparison is the reason the two
 * `m_den`/`m_num` pointers are excluded from the filter comparison and not
 * merely skipped.
 *
 * THE 11-TAP NUMERATOR'S ZEROS ARE THE BLOB'S.  Every second coefficient is
 * exactly +0.0 and the magnitudes are ~1e-8; that is a real table, read out
 * of the object, and not a partially transcribed one.
 *
 * WHY `src/dsp/`: its base and the filter under that are both here, and
 * nothing in the object says which translation unit it belonged to -- the
 * class sits between `GenericToneDetector` and `K56FlexFloModem`'s stubs in
 * the text, which settles nothing.  `compare.py`'s per-object rollup will
 * attribute it to this file (finding F610), so read that number knowing the
 * placement is a choice.
 */

#include "dsplib/ANSamToneDetector.h"
#include "dsplib/vpcm_tables.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
typedef char ansam_size[(sizeof(ANSamToneDetector) == 0x3c) ? 1 : -1];
typedef char ansam_base_size[(sizeof(GenericToneDetector) == 0x3c) ? 1 : -1];
#endif

/*
 * ---------------------------------------------------------------------------
 * ANSamToneDetector's constructor: the 2100 Hz IIR, one design per sample
 * rate.
 *
 * WHICH PAIR IS USED IS CHOSEN BY THREE SEPARATE `cmp $0x1f40` TESTS on the
 * same value -- .text+0x108c7, +0x10905 and +0x1091d -- one for the B array,
 * one for the A array and one for the shared tap count.  0x1f40 is 8000, so
 * the `_8000` pair is the equality arm and the `_9600` pair is everything
 * else; the name is the design's rate and not a claim that 9600 is the only
 * other one.
 * ---------------------------------------------------------------------------
 */

/*
 * `IIR2100_Coef_A_8000`, .data+0x2a0 -- DENOMINATOR at 8 kHz, third argument
 * of `GenericToneDetector(13, 13, A, B, ...)`.  Thirteen doubles indexed by
 * tap, a[0] exactly 1.0.  A twelfth-order recursive section, and all thirteen
 * coefficients are positive, which is the signature of a lowpass-prototype
 * denominator rather than the alternating signs the 9600 design has.
 */
static double IIR2100_Coef_A_8000[IIR2100_TAPS_8000] = {
	1.0, 0.89541491035646703, 6.0172148546971167, 4.3198953361432411,
	14.81300311749296, 8.3172895657106665, 19.12198197948349,
	7.9878009229030278, 13.65601668082814, 3.8260430702025792,
	5.1123037260126232, 0.73105659448409177, 0.78247411963482927,
};

/*
 * `IIR2100_Coef_B_8000`, .data+0x1c0 -- NUMERATOR at 8 kHz, fourth argument of
 * the same call.  Thirteen doubles, all small and all positive.
 */
static double IIR2100_Coef_B_8000[IIR2100_TAPS_8000] = {
	0.0024082424508832588, 0.0019498558169405531, 0.011663689293966391,
	0.0076189528889452386, 0.02346701176517511, 0.012047109745466149,
	0.025043121085192872, 0.0096040039766794698, 0.01492339147837984,
	0.0038520411694815452, 0.0046978678998417017, 0.00062087677563193132,
	0.00060868589911538661,
};

/*
 * `IIR2100_Coef_A_9600`, .data+0x240 -- DENOMINATOR at the other rate.  Eleven
 * doubles, a[0] exactly 1.0, signs alternating.
 */
static double IIR2100_Coef_A_9600[IIR2100_TAPS_9600] = {
	1.0, -1.943566985186252, 6.4682469545778698, -8.2972076136613016,
	14.445775463170071, -12.64954539798627, 14.337539482872881,
	-8.1733225465473112, 6.3239307716147168, -1.8859534703639531,
	0.96308623821118655,
};

/*
 * `IIR2100_Coef_B_9600`, .data+0x160 -- NUMERATOR at the other rate.  Eleven
 * doubles, and the only table here with a structure worth naming: EVERY
 * ODD-INDEXED ENTRY IS EXACTLY ZERO and the five non-zero ones are the
 * binomial coefficients of (1 - z^-2)^5 scaled by 2.3122e-09.  A five-fold
 * zero pair, in other words -- five zeros at DC and five at Nyquist.
 *
 * That is what makes the table a real test rather than a formality: half its
 * entries are zero, so a test that only checked "not all the same" would pass
 * on a table that had lost the other half.  t_vpcmtabs.c checks the byte
 * pattern against the blob's and the zero positions separately.
 */
static double IIR2100_Coef_B_9600[IIR2100_TAPS_9600] = {
	2.312202029234315e-09, 0.0, -1.1561010146171571e-08, 0.0,
	2.3122020292343141e-08, 0.0, -2.3122020292343141e-08, 0.0,
	1.1561010146171571e-08, 0.0, -2.312202029234315e-09,
};

/* The rate that picks the 13-tap pair.  `cmp $0x1f40,%edx`, three times. */
#define ANSAM_RATE_13	8000u

/*
 * THE SELECTOR IS TESTED THREE TIMES AND THAT IS THE SOURCE'S SHAPE, NOT AN
 * ACCIDENT OF SCHEDULING.  The object compares against 8000 at +0x17, +0x55
 * and +0x6d and does not reuse the flag, which is what three separate
 * conditional expressions in one argument list compile to; a single
 * `if (sampleRate == 8000) { ... } else { ... }` over two calls would emit
 * one comparison and two call sites.  The tap count comes out as
 * `lea 0xb(%eax,%eax,1)` on a `sete`, which is `11 + 2 * (rate == 8000)` --
 * the compiler's spelling of the conditional, not the author's.
 *
 * The base takes `nden` and `nnum` separately and this class always passes
 * the same value for both; the two tables it hands over are that long each.
 */
ANSamToneDetector::ANSamToneDetector(unsigned int samples1,
				     unsigned int samples2, float threshold,
				     unsigned int flag, float ratio,
				     unsigned int sampleRate,
				     unsigned int blockLen,
				     unsigned int blockSize)
	: GenericToneDetector(sampleRate == ANSAM_RATE_13 ? 13u : 11u,
			      sampleRate == ANSAM_RATE_13 ? 13u : 11u,
			      sampleRate == ANSAM_RATE_13 ? IIR2100_Coef_A_8000
							  : IIR2100_Coef_A_9600,
			      sampleRate == ANSAM_RATE_13 ? IIR2100_Coef_B_8000
							  : IIR2100_Coef_B_9600,
			      samples1, samples2, threshold, flag, ratio,
			      blockLen, blockSize)
{
}

/*
 * Empty.  The base's destructor is what frees the filter, and the object
 * calls it and does nothing else.
 */
ANSamToneDetector::~ANSamToneDetector()
{
}

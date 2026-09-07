/*
 * ANSamToneDetector.cpp -- reconstructed from dsplibs.o.  Both members use
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

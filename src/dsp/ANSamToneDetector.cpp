/*
 * ANSamToneDetector.cpp -- reconstructed from dsplibs.o.  Both members, and
 * the four coefficient tables they choose between.
 * `include/dsplib/ANSamToneDetector.h` carries the object map, the argument
 * that the class DERIVES from `GenericToneDetector` rather than containing
 * one, and the reading of the sixth argument as a selector.
 *
 * THE TABLES ARE FILE-LOCAL AND THEY ARE `double *`, NOT `const double *`.
 * They live in .data, not .rodata, and the base passes them straight to
 * `GenericIIR<float, double>`, whose coefficient parameters are non-const --
 * so a `const` here would not compile and would not be the object's type
 * either.  They are laid out below in the object's own order: .data+0x160,
 * +0x1c0, +0x240, +0x2a0.
 *
 * EVERY LITERAL BELOW ROUND-TRIPS.  Each is the shortest decimal that
 * reproduces the blob's eight bytes exactly, and the test compares the arrays
 * the two sides' filters point at, in full, on every trial -- so a
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
 * attribute it to this file (finding 610), so read that number knowing the
 * placement is a choice.
 */

#include "dsplib/ANSamToneDetector.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
typedef char ansam_size[(sizeof(ANSamToneDetector) == 0x3c) ? 1 : -1];
typedef char ansam_base_size[(sizeof(GenericToneDetector) == 0x3c) ? 1 : -1];
#endif

/* The rate that picks the 13-tap pair.  `cmp $0x1f40,%edx`, three times. */
#define ANSAM_RATE_13	8000u

/* .data+0x160 -- the 11-tap numerator. */
static double ansamNum11[11] = {
	2.312202029234315e-09,
	0.0,
	-1.156101014617157e-08,
	0.0,
	2.312202029234314e-08,
	0.0,
	-2.312202029234314e-08,
	0.0,
	1.156101014617157e-08,
	0.0,
	-2.312202029234315e-09
};

/* .data+0x1c0 -- the 13-tap numerator. */
static double ansamNum13[13] = {
	0.002408242450883259,
	0.001949855816940553,
	0.01166368929396639,
	0.007618952888945239,
	0.02346701176517511,
	0.01204710974546615,
	0.02504312108519287,
	0.00960400397667947,
	0.01492339147837984,
	0.003852041169481545,
	0.004697867899841702,
	0.0006208767756319313,
	0.0006086858991153866
};

/* .data+0x240 -- the 11-tap denominator; [0] is the divisor and is 1. */
static double ansamDen11[11] = {
	1.0,
	-1.943566985186252,
	6.46824695457787,
	-8.297207613661302,
	14.44577546317007,
	-12.64954539798627,
	14.33753948287288,
	-8.173322546547311,
	6.323930771614717,
	-1.885953470363953,
	0.9630862382111866
};

/* .data+0x2a0 -- the 13-tap denominator; [0] is the divisor and is 1. */
static double ansamDen13[13] = {
	1.0,
	0.895414910356467,
	6.017214854697117,
	4.319895336143241,
	14.81300311749296,
	8.317289565710666,
	19.12198197948349,
	7.987800922903028,
	13.65601668082814,
	3.826043070202579,
	5.112303726012623,
	0.7310565944840918,
	0.7824741196348293
};

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
			      sampleRate == ANSAM_RATE_13 ? ansamDen13
							  : ansamDen11,
			      sampleRate == ANSAM_RATE_13 ? ansamNum13
							  : ansamNum11,
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

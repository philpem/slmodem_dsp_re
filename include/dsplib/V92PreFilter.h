/**
 * @file V92PreFilter.h
 * @brief The V.92 transmit pre-filter: a `FloatFIR` and a `FloatIIR` it
 *        owns, either of which may be switched off by a zero tap count.
 *
 * Five members in the blob, 550 bytes; this tree defines the constructor
 * and destructor and declares the rest.
 *
 * It is `V92Precoder`'s twin: the two constructors are 127 bytes each, the
 * two destructors 108, and the instruction sequences are the same one with
 * the offsets and second class name changed -- allocate 0x14, construct,
 * store, twice, then `if (p) { p->~T(); free(p); }` twice. Almost certainly
 * one piece of source written twice (finding F1246). Not polymorphic, and
 * +0x00 is unreferenced, for the reasons `V92Precoder.h` gives.
 *
 * The object is 0x14 bytes -- the same size as one of the filters it
 * holds, a coincidence worth not tripping over -- measured the same way as
 * `V92Precoder`'s: `V92Transmitter` allocates exactly `sizeof` bytes before
 * calling this constructor, agreeing with the furthest member access, +0x10.
 *
 * The second filter is a `FloatIIR` and not a second `FloatFIR`, and no
 * behavioral test can show it: the two classes have the same five fields in
 * the same order, round tap counts down to a multiple of four the same way,
 * allocate and zero the same buffer and leave the same write index -- a
 * reconstruction using two FIRs would produce byte-identical objects and
 * identical allocation counts. What settles it is the relocation: the
 * constructor's second call targets `_ZN8FloatIIRC1EjPfj` and the
 * destructor's second targets `_ZN8FloatIIRD1Ev`, direct evidence about the
 * source that `make similarity` keeps honest (a wrong callee is a wrong
 * call target there).
 */

#ifndef DSPLIB_V92PREFILTER_H
#define DSPLIB_V92PREFILTER_H

#include "dsplib/FloatFIR.h"
#include "dsplib/FloatIIR.h"

/* Both filters are built with 99 samples of slack, exactly as the precoder's
 * are: `movl $0x63,0xc(%esp)` before each constructor call. */
#define V92PREFILTER_BLOCK 99

/*
 * `process` moves twelve floats whatever path it takes -- the two filter
 * calls pass `$0xc` as their count and the both-off arm is a twelve-word copy
 * loop (`cmp $0xb,%edx; jle`).  So the caller's buffers are twelve samples
 * and this is the block the 99 words of slack are slack for.
 */
#define V92PREFILTER_SAMPLES 12

class V92PreFilter {
public:
	/**
	 * @brief Construct, allocating both owned filters.
	 * @param nTaps  Tap count passed through to both.
	 */
	V92PreFilter(unsigned int nTaps);
	/** @brief Destroy, freeing both owned filters. */
	~V92PreFilter();

	/** @brief Reset both owned filters. Forwards to `fir`/`iir`'s own
	 *  reset(). */
	void reset();
	/**
	 * @brief Store new coefficients and tap counts for both stages, and
	 *        gate which stages process() runs.
	 * @param coefFir   FIR coefficients.
	 * @param coefIir   IIR coefficients.
	 * @param tapsFir   FIR tap count; zero disables the FIR stage.
	 * @param tapsIir   IIR tap count; zero disables the IIR stage.
	 */
	void setCoefficients(float *coefFir, float *coefIir,
			     unsigned int tapsFir, unsigned int tapsIir);
	/**
	 * @brief Filter one block of samples through whichever of the FIR
	 *        and IIR stages setCoefficients() left enabled -- both,
	 *        chained through a stack buffer; either alone; or neither,
	 *        a straight copy.
	 * @param in   Input samples.
	 * @param out  Output samples.
	 */
	void process(float *in, float *out);

	/* +0x00  Not referenced by any of the five members and not written
	 * by the constructor -- a real member, not a vptr (see file
	 * comment). */
	unsigned int word_00;

	/* +0x04, +0x08  The two owned filters, in that order. */
	FloatFIR *fir;
	FloatIIR *iir;

	/* +0x0c, +0x10  setCoefficients()'s third and fourth arguments,
	 * stored unchanged. Zero means "skip that filter" to process(),
	 * which tests them and nothing else. */
	unsigned int tapsFir;
	unsigned int tapsIir;
};

#endif /* DSPLIB_V92PREFILTER_H */

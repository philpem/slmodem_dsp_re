/*
 * V92PreFilter.h -- the V.92 transmit pre-filter: a FloatFIR and a FloatIIR
 * it owns, either of which may be switched off by a zero tap count.
 *
 * Reconstructed from dsplibs.o.  Five members in the blob, 550 bytes; this
 * tree defines the constructor and the destructor and declares the rest.
 *
 * IT IS V92Precoder'S TWIN.  The two constructors are 127 bytes each and the
 * two destructors 108, and the instruction sequences are the same one with
 * the offsets and the second class name changed: allocate 0x14, construct,
 * store; allocate 0x14, construct, store; then `if (p) { p->~T(); free(p); }`
 * twice.  They are almost certainly one piece of source written twice
 * (finding F1246).  NOT POLYMORPHIC, and +0x00 unreferenced, for the reasons
 * V92Precoder.h gives.
 *
 * THE OBJECT IS 0x14 BYTES -- the same size as one of the filters it holds,
 * which is a coincidence worth not tripping over.  `V92Transmitter` builds
 * one with `movl $0x14,(%esp); call sysdep_malloc; mov $0x140,%ecx; call
 * V92PreFilter::V92PreFilter(unsigned)`, so the size is the original's
 * `sizeof` and not a bound; the largest displacement any member uses is
 * +0x10, which agrees.
 *
 * THE SECOND FILTER IS A FloatIIR AND NOT A SECOND FloatFIR, and no
 * behavioural test can show it.  The two classes have the same five fields in
 * the same order, round their tap counts down to a multiple of four the same
 * way, allocate and zero the same buffer and leave the same write index -- so
 * a reconstruction that built two FIRs would produce byte-identical objects
 * and identical `harness_alloc` counters.  What settles it is the relocation:
 * the constructor's second call is `R_386_PC32 _ZN8FloatIIRC1EjPfj` and the
 * destructor's second is `_ZN8FloatIIRD1Ev`.  That is direct evidence about
 * the source, and `make similarity` is the tier that keeps it honest, because
 * a wrong callee is a wrong call target there.
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
	/* Written. */
	V92PreFilter(unsigned int nTaps);
	~V92PreFilter();

	/*
	 * Written.  The argument types are the mangling's; a return type is
	 * never mangled and none of the three leaves anything meaningful in
	 * %eax, so all three are `void`.
	 *
	 * `reset` forwards to both filters' `reset`.  `setCoefficients` gives
	 * the FIR the first pair and the IIR the second, and then stores the
	 * two counts at +0x0c and +0x10 -- which is what `process` gates on.
	 * `process` runs the FIR if +0x0c is non-zero and the IIR if +0x10 is,
	 * chaining them through a stack buffer when both are on and copying
	 * the input straight through when neither is.
	 */
	void reset();
	void setCoefficients(float *coefFir, float *coefIir,
			     unsigned int tapsFir, unsigned int tapsIir);
	void process(float *in, float *out);

	/*
	 * Public for the reason V92Precoder.h gives: the original's access
	 * specifiers are not recoverable, and one access section keeps the
	 * class standard-layout for `__builtin_offsetof`.
	 */

	/*
	 * +0x00  Not referenced by any of the five members and not written by
	 * the constructor.  A real member, not a vptr -- the constructor of a
	 * polymorphic class would store one here and this one does not.
	 */
	unsigned int word_00;

	/* +0x04, +0x08  The two owned filters, in that order. */
	FloatFIR *fir;
	FloatIIR *iir;

	/*
	 * +0x0c, +0x10  `setCoefficients`' third and fourth arguments, stored
	 * unchanged; the mangling gives them as `unsigned int`.  Zero means
	 * "skip that filter" to `process`, which tests them and nothing else.
	 */
	unsigned int tapsFir;
	unsigned int tapsIir;
};

#endif /* DSPLIB_V92PREFILTER_H */

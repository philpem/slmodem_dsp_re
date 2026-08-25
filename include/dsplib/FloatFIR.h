/*
 * FloatFIR.h -- the float FIR the V.90 pre-filter is built out of.
 *
 * Reconstructed from dsplibs.o FloatFIR.cpp: six members, 723 bytes unique.
 * `nm` totals 858 because the constructor and destructor each appear twice,
 * byte-identical -- C1/C2 and D1/D2 -- which GCC emits from one definition.
 *
 * NOT POLYMORPHIC.  tools/cppstruct.py lists the destructor with the two
 * ordinary variants and not the deleting `D0`, and GCC emits a deleting
 * destructor only for a virtual one, so offset 0 is a real member and there
 * is no vptr (finding F228).
 *
 * THE HISTORY BUFFER FILLS DOWNWARD.  `index` is where the next input goes
 * and it *decreases*, so history[index] is the newest sample and
 * history[index + taps - 1] the oldest the convolution reaches.  The buffer
 * is over-allocated by the constructor's third argument so a whole block can
 * be processed between wraps; when `index` would go negative the last
 * `taps - 1` samples are copied to the top of the buffer and `index` restarts
 * at `bufferLength - taps`.  That copy is the only thing that makes the
 * falling index legal, and it is what `reset()` and the constructor set up by
 * putting `index` at `bufferLength - taps` to begin with.
 *
 * `taps` is always the constructor's or setCoefficients' count rounded DOWN
 * to a multiple of four (`and $0xfffffffc`), because the inner loop is
 * unrolled by four with a one-at-a-time tail that the object still emits --
 * so a count of 22 uses 20 taps and the tail loop never runs.
 */

#ifndef DSPLIB_FLOATFIR_H
#define DSPLIB_FLOATFIR_H

class FloatFIR {
public:
	/*
	 * The signatures are the mangling's, so this is a specification and
	 * not a guess.  A return type is never mangled: `process(float)`
	 * returns its result in st(0) so it is `float`; the block form leaves
	 * nothing meaningful in %eax and is `void`; `setCoefficients` returns
	 * 0 or -1 in %eax.
	 */
	FloatFIR(unsigned int nTaps, float *coef, unsigned int blockSize);
	~FloatFIR();

	void reset();
	int setCoefficients(float *coef, unsigned int nTaps);
	float process(float in);
	void process(const float *in, float *out, unsigned int count);

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable from the mangling, and because one access section is
	 * what keeps the class standard-layout so `__builtin_offsetof` is well
	 * defined -- the .cpp asserts every offset below.  The names are
	 * invented; the mangling never carries a data member's name.
	 */
	float *coefficients;		/* +0x00 */
	float *history;			/* +0x04 sysdep_malloc'd, bufferLength */
	unsigned int taps;		/* +0x08 always a multiple of four     */
	unsigned int bufferLength;	/* +0x0c taps + the constructor's slack */
	int index;			/* +0x10 write position, counts DOWN   */
};

#endif /* DSPLIB_FLOATFIR_H */

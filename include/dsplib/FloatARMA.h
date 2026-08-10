/*
 * FloatARMA.h -- direct-form-I recursive filter over float samples.
 *
 * Reconstructed from dsplibs.o.  Five members, 1,770 bytes by `nm` because
 * the constructor and destructor each appear twice, byte-identical -- C1/C2
 * and D1/D2 -- which GCC emits from one definition.
 *
 * NOT POLYMORPHIC.  tools/cppstruct.py lists the destructor with the two
 * ordinary variants and not the deleting `D0`, and GCC emits a deleting
 * destructor only for a virtual one, so offset 0 is a real member and there
 * is no vptr (the argument at include/dsplib/FloatFIR.h, finding 228).
 *
 * THE DIFFERENCE EQUATION, and which array is which.  The object settles it
 * and the mangling does not: the constructor divides BOTH coefficient arrays
 * by `den[0]` and then stores 0.0f over `m_a[0]`, which is the classic
 * normalisation of a recursive filter, and `process` subtracts the feedback
 * sum from the feed-forward one.  So the FIRST count/pointer pair is the
 * DENOMINATOR and the second the numerator:
 *
 *     y[n] = SUM(k = 0 .. nB-1) b[k] * x[n-k]
 *          - SUM(k = 1 .. nA-1) a[k] * y[n-k]
 *
 * with a[] and b[] both pre-divided by the caller's den[0] and a[0] forced to
 * zero so the k = 0 term of the feedback sum contributes nothing.  When
 * den[0] is exactly 1.0f the scaling is skipped entirely -- an `fcom`/`je`
 * around both loops -- and only the `m_a[0] = 0.0f` remains.
 *
 * THE TAP COUNTS ARE ROUNDED **UP** to a multiple of four, which is the
 * opposite of FloatFIR and FloatIIR: `and $0xfffffffc` followed by a
 * conditional `add $0x4`.  Asking for 5 denominator taps gets 8, and the
 * slots between the caller's count and the rounded one are zero-filled.  The
 * dot product is unrolled four ways with a one-at-a-time tail that therefore
 * never runs.
 *
 * BOTH HISTORY BUFFERS FILL DOWNWARD, as FloatFIR's does.  `m_xpos` and
 * `m_ypos` are where the next sample goes and they *decrease*; when one would
 * go negative the newest `taps - 1` samples are copied to the top of that
 * buffer and the position restarts at `len - taps`.  The two run in lockstep
 * -- both start at `blockSize` and are decremented once per sample -- so they
 * wrap on the same call and cannot be separated by timing, only by the number
 * of words each carries.
 *
 * `m_idx` IS A MEMBER, NOT A LOCAL.  Every fill loop in the constructor and
 * in `reset` uses it as its index and leaves it behind: after `reset` it
 * holds `m_ylen`, or 0 when `m_ylen` is 0.  See finding 874 for why the
 * constructor's earlier loops appear to leave `n - 1` and do not.
 */

#ifndef DSPLIB_FLOATARMA_H
#define DSPLIB_FLOATARMA_H

class FloatARMA {
public:
	/*
	 * The signature is the mangling's, so the shape is a specification;
	 * only the names are invented.  `nDen`/`den` and `nNum`/`num` are
	 * rounded up to a multiple of four independently, and `blockSize` is
	 * the slack above each tap count -- it decides how often a history is
	 * compacted and nothing about the response.
	 *
	 * A return type is never mangled: `process(float)` leaves its result
	 * in st(0) so it is `float`; the block form leaves nothing meaningful
	 * in %eax and is `void`.
	 *
	 * Four allocations, in the order a, b, x-history, y-history.  A
	 * failed allocation is not reported and the fill loops that follow
	 * would fault on it, exactly as FloatFIR's constructor does.
	 */
	FloatARMA(unsigned int nDen, unsigned int nNum, float *den, float *num,
		  unsigned int blockSize);
	~FloatARMA();

	/* Zero both histories and rewind both write positions. */
	void reset();

	float process(float in);
	void process(const float *in, float *out, unsigned int count);

	/*
	 * Data members are public because the original's access specifiers
	 * are not recoverable from the mangling, and because one access
	 * section is what keeps the class standard-layout so
	 * `__builtin_offsetof` is well defined -- the .cpp asserts every
	 * offset below.  The names are invented; the mangling never carries a
	 * data member's name.
	 *
	 * Note the crossing: the buffer at +0x08 is the INPUT history and its
	 * length +0x18 is built from the NUMERATOR tap count at +0x14, while
	 * +0x0c/+0x1c go with the denominator count at +0x10.  That is the
	 * object's own pairing, not a transcription slip.
	 */
	float *m_a;		/* +0x00 owned, m_nA entries, a[0] == 0.0f  */
	float *m_b;		/* +0x04 owned, m_nB entries                */
	float *m_xhist;		/* +0x08 owned, m_xlen entries              */
	float *m_yhist;		/* +0x0c owned, m_ylen entries              */
	unsigned int m_nA;	/* +0x10 denominator taps, multiple of four */
	unsigned int m_nB;	/* +0x14 numerator taps, multiple of four   */
	unsigned int m_xlen;	/* +0x18 m_nB + blockSize                   */
	unsigned int m_ylen;	/* +0x1c m_nA + blockSize                   */
	int m_xpos;		/* +0x20 input write index, counts DOWN     */
	int m_ypos;		/* +0x24 output write index, counts DOWN    */
	unsigned int m_idx;	/* +0x28 the fill loops' index, left behind */
	float m_fwd;		/* +0x2c the b.x sum, then the output       */
	float m_fbk;		/* +0x30 the a.y sum                        */
};				/* 0x34 bytes                               */

#endif /* DSPLIB_FLOATARMA_H */

/*
 * FloatARMA.cpp -- reconstructed from dsplibs.o.  All five members;
 * `include/dsplib/FloatARMA.h` carries the object map and the difference
 * equation.
 *
 * THE DEFINITION ORDER IS THE OBJECT'S AND IT IS LOAD-BEARING.  `process(const
 * float *, float *, unsigned)` comes FIRST, above the constructor, which is
 * why the emission order is process(Pf), reset, C2, C1, D2, D1, process(f) --
 * `reset` is hoisted above the constructor that calls it, and everything else
 * follows the source.  We had all five in the "natural" order and were 0 of 7
 * against the blob's `nm -n`; this arrangement is 7 of 7 and `reset` went from
 * nine differing bytes to exact.  All 5! = 120 orderings were compiled on the
 * period toolchain: three distinct emissions, 30 of them close `reset`, so
 * what is decoded is the FACT that `process(Pf)` precedes `reset`'s emission
 * slot and not a unique order -- the `nm -n` agreement is the tiebreaker.
 * FloatIIR.cpp turned out to have the identical shape.  Do not "tidy" this
 * back into declaration order; see docs/method/refinement.md lever 3.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x20(%esp),%ebx` after four pushes and a 12-byte frame --
 * not %ecx, so nothing here needs an attribute (finding F215).
 *
 * THE TWO ACCUMULATORS ARE THE ORIGINAL'S, NOT A CONVENIENCE, and the
 * argument is FloatFIR.cpp's: both dot products run with two independent x87
 * registers, taps 0 and 2 of each unrolled group folded into one and taps 1
 * and 3 into the other, the one-at-a-time tail into the same register as taps
 * 0 and 2, summed once at the end and rounded once by `fstps`.  A single
 * `float` accumulator would round every partial sum and diverge; one `long
 * double` would pair the products differently and diverge in the last bit.
 * `long double` is what an x87 register holds, so the two below are the
 * register file.
 *
 * All four dot products in this file -- two in each `process` -- use the same
 * helper.  The object assigns the x87 stack slots differently in each (loop
 * one opens `faddp %st,%st(1)`, loop two `faddp %st,%st(2)`), but that is
 * stack DEPTH, not different arithmetic: the pairing, the tail's destination
 * and the single final add are identical in all four.
 *
 * WHAT THE POPPING DIVIDES REALLY ARE.  The constructor's normalisation uses
 * `d8 f9` and `d8 37`, which are the D8 *register* and *memory* forms and are
 * printed correctly by objdump; there is no DE-form popping divide or
 * subtract anywhere in this file, so finding F245's swap does not bite here.
 * The one popping subtract, `d8 6d 2c` in the block form, is likewise a D8
 * memory form: `fsubrs m32` is st(0) = m32 - st(0), which is why `m_fwd`
 * comes out as forward minus feedback and not the other way round.
 */

#include "dsplib/FloatARMA.h"

extern "C" {
void *sysdep_malloc(unsigned size);
void sysdep_free(void *ptr);
}

/*
 * THE REPLACEMENT `operator delete[]`, AND IT IS READ OFF THE OBJECT.  The
 * blob makes an ordinary `call sysdep_free` at a destructor's LAST free where
 * an explicit `if (p) sysdep_free(p)` makes a sibling `jmp` -- one instruction
 * fewer at the same byte count.  Eight spellings were compiled and only
 * `delete[]` over an inline replacement reproduces the object's shape; see the
 * finding for the enumeration.  Behaviourally it is exactly the guard it
 * replaces: `float` has no destructor, so `delete[] p` is `if (p)
 * operator delete[](p)` with no array cookie.
 */
inline void operator delete[](void *p) { sysdep_free(p); }

/*
 * Hold the compiler to the map in the header.  tools/offcheck.py parses
 * `struct name {` out of include/dsplib and compiles it as C, so a class has
 * to assert its own (finding F230).
 */
#define FLOATARMA_OFF(field, off, tag) \
	typedef char floatarma_off_##tag[ \
	    ((int)__builtin_offsetof(FloatARMA, field) == (off)) ? 1 : -1]

#if __SIZEOF_POINTER__ == 4
FLOATARMA_OFF(m_a,     0x00, m_a);
FLOATARMA_OFF(m_b,     0x04, m_b);
FLOATARMA_OFF(m_xhist, 0x08, m_xhist);
FLOATARMA_OFF(m_yhist, 0x0c, m_yhist);
FLOATARMA_OFF(m_nA,    0x10, m_na);
FLOATARMA_OFF(m_nB,    0x14, m_nb);
FLOATARMA_OFF(m_xlen,  0x18, m_xlen);
FLOATARMA_OFF(m_ylen,  0x1c, m_ylen);
FLOATARMA_OFF(m_xpos,  0x20, m_xpos);
FLOATARMA_OFF(m_ypos,  0x24, m_ypos);
FLOATARMA_OFF(m_idx,   0x28, m_idx);
FLOATARMA_OFF(m_fwd,   0x2c, m_fwd);
FLOATARMA_OFF(m_fbk,   0x30, m_fbk);
typedef char floatarma_size[(sizeof(FloatARMA) == 0x34) ? 1 : -1];
#endif

/*
 * Round a tap count UP to a multiple of four.  This is the opposite of
 * FloatFIR and FloatIIR, which mask DOWN, and it is the object's own shape --
 * `and $0xfffffffc` then a conditional `add $0x4` -- rather than the
 * arithmetically equal `(n + 3) & ~3u`.
 */
static unsigned int
arma_round4(unsigned int n)
{
	unsigned int r = n & ~3u;

	if (r < n)
		r += 4;
	return r;
}

/*
 * The dot product, shared by all four call sites because the object emits the
 * same code at each.  `p` walks up from the history's write position and `c`
 * up from coefficient 0; `even` takes taps 0 and 2 of each unrolled group and
 * the whole of the tail, `odd` takes 1 and 3.  The tail cannot run: both tap
 * counts are rounded to a multiple of four everywhere they are set.
 */
static float
arma_convolve(const float *p, const float *c, unsigned int n)
{
	long double even = 0.0L;
	long double odd = 0.0L;

	while (n > 3) {
		even += (long double)p[0] * c[0];
		odd += (long double)p[1] * c[1];
		even += (long double)p[2] * c[2];
		odd += (long double)p[3] * c[3];
		p += 4;
		c += 4;
		n -= 4;
	}
	while (n != 0) {
		even += (long double)p[0] * c[0];
		p++;
		c++;
		n--;
	}

	return (float)(odd + even);
}

/*
 * Carry the tail of a history to the top of its buffer, so a run that has
 * just used index 0 can restart at `len - taps` and still see the `taps - 1`
 * samples behind it.
 *
 * The loop is the object's `dec %edx; jne`, so a tap count of one or zero
 * runs it 2**32 times.  Left as it is, for FloatFIR's reason: the counts are
 * rounded to a multiple of four everywhere they are set, so the only way in
 * is a zero-tap filter, and making the count safe here would be inventing
 * behaviour the blob does not have.
 */
static void
arma_carry_tail(float *hist, unsigned int taps, unsigned int len)
{
	float *src = hist + taps - 2;
	float *dst = hist + len - 1;
	unsigned int rem = taps - 1;

	do {
		*dst-- = *src--;
	} while (--rem != 0);
}

/*
 * The block form.  Both positions are written ONCE, after the last sample --
 * the running pair lives in registers for the whole run -- and a count of
 * zero returns before the object is read at all, so it does not even fault on
 * a filter with no history buffers.  `m_fwd` and `m_fbk` are still written
 * every sample, since they are what the arithmetic goes through.
 *
 * The output store reads `m_fwd` back out of the object rather than reusing
 * the register: `mov 0x2c(%ebp),%edx` then `mov %edx,(%ecx)`.
 */
void
FloatARMA::process(const float *in, float *out, unsigned int count)
{
	float *x;
	float *y;
	const float *a;
	const float *b;
	unsigned int nA, nB;
	int xp, yp;

	if (count == 0)
		return;

	x = m_xhist;
	y = m_yhist;
	a = m_a;
	b = m_b;
	nA = m_nA;
	nB = m_nB;
	xp = m_xpos;
	yp = m_ypos;

	do {
		x[xp] = *in++;
		m_fwd = arma_convolve(x + xp, b, nB);
		m_fbk = arma_convolve(y + yp, a, nA);

		m_fwd = m_fwd - m_fbk;
		y[yp] = m_fwd;

		yp--;
		if (yp < 0) {
			yp = (int)(m_ylen - nA);
			arma_carry_tail(y, nA, m_ylen);
		}

		*out++ = m_fwd;

		xp--;
		if (xp < 0) {
			xp = (int)(m_xlen - nB);
			arma_carry_tail(x, nB, m_xlen);
		}
	} while (--count != 0);

	m_xpos = xp;
	m_ypos = yp;
}
/*
 * The four zero stores before the first allocation are the object's and are
 * not dead: `sysdep_malloc` is an external call the compiler cannot prove
 * does not read `*this`, so all four have to be in the instruction stream
 * before it.
 *
 * The scaling loops are bounded by the CALLER's counts, not the rounded ones,
 * so the zero-filled slots above `nNum`/`nDen` stay zero; and `den[0]` is
 * re-read on every iteration, which the object also does -- the store to
 * `m_b[i]` may alias it, both being `float *`.
 */
FloatARMA::FloatARMA(unsigned int nDen, unsigned int nNum, float *den,
		     float *num, unsigned int blockSize)
{
	m_nA = arma_round4(nDen);
	m_nB = arma_round4(nNum);
	m_xlen = m_nB + blockSize;
	m_ylen = m_nA + blockSize;

	m_a = 0;
	m_b = 0;
	m_xhist = 0;
	m_yhist = 0;

	m_a = (float *)sysdep_malloc(m_nA * sizeof(float));
	m_b = (float *)sysdep_malloc(m_nB * sizeof(float));
	m_xhist = (float *)sysdep_malloc(m_xlen * sizeof(float));
	m_yhist = (float *)sysdep_malloc(m_ylen * sizeof(float));

	for (m_idx = 0; m_idx < nDen; m_idx++)
		m_a[m_idx] = den[m_idx];
	for (m_idx = nDen; m_idx < m_nA; m_idx++)
		m_a[m_idx] = 0.0f;

	for (m_idx = 0; m_idx < nNum; m_idx++)
		m_b[m_idx] = num[m_idx];
	for (m_idx = nNum; m_idx < m_nB; m_idx++)
		m_b[m_idx] = 0.0f;

	if (den[0] != 1.0f) {
		for (m_idx = 0; m_idx < nNum; m_idx++)
			m_b[m_idx] = 1.0f / den[0] * m_b[m_idx];
		for (m_idx = 1; m_idx < nDen; m_idx++)
			m_a[m_idx] = 1.0f / den[0] * m_a[m_idx];
	}
	m_a[0] = 0.0f;

	reset();
}

/*
 * Four frees, in the order the pointers are declared, each guarded.  The
 * pointers are NOT nulled, so a destroyed object still holds four dangling
 * addresses -- which is why a comparison of two destroyed objects compares
 * the two allocators and not the objects.
 */
FloatARMA::~FloatARMA()
{
	delete[] m_a;
	delete[] m_b;
	delete[] m_xhist;
	delete[] m_yhist;
}

/*
 * Unlike FloatFIR::reset this does not check the buffers for null; the length
 * is the only guard, and a filter with a failed allocation and a non-zero
 * length faults here exactly as the blob does.
 *
 * `m_idx` is left holding `m_ylen`, or 0 when `m_ylen` is 0.  The first
 * loop's exit value is never visible: the statement after it writes `m_idx`
 * again, which is why the object omits that loop's final store (finding F874).
 */
void
FloatARMA::reset()
{
	for (m_idx = 0; m_idx < m_xlen; m_idx++)
		m_xhist[m_idx] = 0.0f;
	for (m_idx = 0; m_idx < m_ylen; m_idx++)
		m_yhist[m_idx] = 0.0f;

	m_xpos = (int)(m_xlen - m_nB);
	m_ypos = (int)(m_ylen - m_nA);
}

/*
 * One sample.  Both `m_fwd` and `m_fbk` are real member stores -- the object
 * writes each with `fstps` and reads `m_fbk` and `m_fwd` straight back from
 * memory for the subtraction -- so they are not locals that happen to be
 * spilled: the round trip through 32 bits is part of the arithmetic.
 *
 * The output is written to `m_fwd` and then to the history from the same
 * register, and the return value is `m_fwd` read back, so all three are the
 * same 32-bit pattern.
 *
 * Both positions are written on every call, in both the wrapping and the
 * non-wrapping arm.
 */
float
FloatARMA::process(float in)
{
	float *x = m_xhist;
	float *y = m_yhist;
	int xp = m_xpos;
	int yp = m_ypos;

	x[xp] = in;
	m_fwd = arma_convolve(x + xp, m_b, m_nB);
	m_fbk = arma_convolve(y + yp, m_a, m_nA);

	m_fwd = m_fwd - m_fbk;
	y[yp] = m_fwd;

	if (yp - 1 < 0) {
		m_ypos = (int)(m_ylen - m_nA);
		arma_carry_tail(y, m_nA, m_ylen);
	} else {
		m_ypos = yp - 1;
	}

	if (xp - 1 < 0) {
		m_xpos = (int)(m_xlen - m_nB);
		arma_carry_tail(x, m_nB, m_xlen);
	} else {
		m_xpos = xp - 1;
	}

	return m_fwd;
}


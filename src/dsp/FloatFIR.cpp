/*
 * FloatFIR.cpp -- the float FIR used by V90PreFilter.
 *
 * Reconstructed from dsplibs.o FloatFIR.cpp.  All six members;
 * `include/dsplib/FloatFIR.h` carries the object map.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x8(%esp),%ebx` after one push -- not %ecx, so nothing
 * here needs an attribute (finding 215).
 *
 * THE DEFINITION ORDER IS THE OBJECT'S EMISSION ORDER, which is why the block
 * `process` comes first and the scalar one last: `nm -n` on the blob gives
 * process(const float*, float*, unsigned), reset, the constructor pair, the
 * destructor pair, setCoefficients, process(float), and all 8 emitted symbols
 * now sit at the blob's own index.  It was worth trying and it PAID NOTHING --
 * `FloatFIR::reset` is still grade 1, four differing bytes, a clean
 * `%edx`/`%ecx` swap on `taps`, and every other symbol's differing-byte count
 * is unchanged to the byte.  Recorded rather than reverted because an achieved
 * order is the only thing that makes a null result evidence: this file is a
 * measured negative for lever 3, not an untried one.  See
 * docs/method/refinement.md lever 3.
 *
 * THE TWO ACCUMULATORS ARE THE ORIGINAL'S, NOT A CONVENIENCE.  Both
 * `process` overloads run the multiply-accumulate with two independent x87
 * registers: `faddp %st,%st(1)` folds the even-indexed products into one and
 * `faddp %st,%st(2)` the odd into the other, and the one-at-a-time tail --
 * which only runs when the tap count is not a multiple of four, i.e. never,
 * since `taps` is masked -- goes into the even one.  They are summed once at
 * the end and rounded once, by `fstps`.  Writing that as a single `float`
 * accumulator would round every partial sum and diverge; writing it as one
 * `long double` would pair the products differently and diverge in the last
 * bit.  `long double` is what an x87 register holds, so the two `long
 * double`s below are the register file (see src/dsp/FloatIIR.cpp for the same
 * argument made the other way round, about -ffloat-store).
 */

#include "dsplib/FloatFIR.h"

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
 * to assert its own (finding 230).
 */
#define FLOATFIR_OFF(field, off, tag) \
	typedef char floatfir_off_##tag[ \
	    ((int)__builtin_offsetof(FloatFIR, field) == (off)) ? 1 : -1]

#if __SIZEOF_POINTER__ == 4
FLOATFIR_OFF(coefficients, 0x00, coefficients);
FLOATFIR_OFF(history,      0x04, history);
FLOATFIR_OFF(taps,         0x08, taps);
FLOATFIR_OFF(bufferLength, 0x0c, bufferlength);
FLOATFIR_OFF(index,        0x10, index);
typedef char floatfir_size[(sizeof(FloatFIR) == 0x14) ? 1 : -1];
#endif

/*
 * The convolution, shared by the two `process` overloads because the object
 * emits the same code twice.  `p` walks up from history[index] and `c` up
 * from coefficients[0]; `even` takes taps 0 and 2 of each unrolled group and
 * the whole of the tail, `odd` takes 1 and 3.
 */
static float
floatfir_convolve(const float *p, const float *c, unsigned int n)
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
 * Carry the tail of the history to the top of the buffer, so a run that has
 * just used history[0] can restart at history[bufferLength - taps] and still
 * see the `taps - 1` samples behind it.
 *
 * The loop is the object's `dec %edx; jne`, so a tap count of one or zero
 * runs it 2**32 times.  Left as it is: `taps` is masked to a multiple of four
 * everywhere it is set, so the only way in is a zero-tap filter, and making
 * the count safe here would be inventing behaviour the blob does not have.
 */
static void
floatfir_carry_tail(float *history, unsigned int taps, unsigned int bufferLength)
{
	float *src = history + taps - 2;
	float *dst = history + bufferLength - 1;
	unsigned int rem = taps - 1;

	do {
		*dst-- = *src--;
	} while (--rem != 0);
}

/*
 * The block form.  `index` is written ONCE, after the last sample -- the
 * running position lives in a register for the whole run -- and a count of
 * zero returns before the object is read at all, so it does not even fault on
 * a filter with no history buffer.
 */
void
FloatFIR::process(const float *in, float *out, unsigned int count)
{
	float *h;
	const float *c;
	unsigned int n;
	int i, next;

	if (count == 0)
		return;

	h = history;
	c = coefficients;
	n = taps;
	i = index;
	next = i;

	do {
		h[i] = *in++;
		next = i - 1;
		*out++ = floatfir_convolve(h + i, c, n);

		i = next;
		if (i < 0) {
			i = (int)(bufferLength - n);
			next = i;
			floatfir_carry_tail(h, n, bufferLength);
		}
	} while (--count != 0);

	index = next;
}

void
FloatFIR::reset()
{
	unsigned int i;

	if (history != 0)
		for (i = 0; i < bufferLength; i++)
			history[i] = 0.0f;

	index = (int)(bufferLength - taps);
}

FloatFIR::FloatFIR(unsigned int nTaps, float *coef, unsigned int blockSize)
{
	unsigned int i;

	taps = nTaps & ~3u;
	coefficients = coef;
	bufferLength = taps + blockSize;

	history = (float *)sysdep_malloc(bufferLength * sizeof(float));
	if (history != 0)
		for (i = 0; i < bufferLength; i++)
			history[i] = 0.0f;

	index = (int)(bufferLength - taps);
}

FloatFIR::~FloatFIR()
{
	delete[] history;
}

/*
 * Point the filter at a new coefficient set.  Returns -1 and changes nothing
 * if the buffer cannot hold that many taps, 0 otherwise.
 *
 * The write position is pulled back to the new run's start only if it is
 * already past it, and that comparison is signed -- so a filter whose index
 * is about to wrap keeps its negative position rather than being reset.
 */
int
FloatFIR::setCoefficients(float *coef, unsigned int nTaps)
{
	unsigned int want = nTaps & ~3u;
	int room;

	if (bufferLength <= want)
		return -1;

	coefficients = coef;
	if (taps == want)
		return 0;

	taps = want;
	room = (int)(bufferLength - want);
	if (index > room)
		index = room;

	return 0;
}

float
FloatFIR::process(float in)
{
	float *h = history;
	int i = index;
	int next = i - 1;
	float out;

	h[i] = in;
	out = floatfir_convolve(h + i, coefficients, taps);

	if (next >= 0) {
		index = next;
		return out;
	}

	index = (int)(bufferLength - taps);
	floatfir_carry_tail(h, taps, bufferLength);

	return out;
}

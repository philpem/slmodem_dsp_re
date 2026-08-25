/*
 * V90RDetector.cpp -- the constructor and destructor.
 *
 * `include/dsplib/V90RDetector.h` carries the object map, the 44-byte
 * measurement and the evidence for every field width.
 *
 * PLAIN CDECL with `this` as the first STACK argument -- `mov 0x4(%esp),%eax`
 * with no frame -- so nothing here needs a calling-convention attribute
 * (finding F215).
 */

#include <stddef.h>

#include "dsplib/V90RDetector.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but parses only `struct name {`, so a C++ class asserts
 * its own -- and this is the check that catches an object right in size and
 * wrong in its offsets.
 *
 * GUARDED ON THE POINTER WIDTH, because the class holds one at +0x28 and
 * `make check64` compiles this file for the native target, where it is eight
 * bytes and the size moves with it.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define RD_OFF(field, off, tag) \
	typedef char rd_off_##tag[ \
	    ((int)__builtin_offsetof(V90RDetector, field) == (off)) ? 1 : -1]

RD_OFF(int_00,    0x00, int00);
RD_OFF(int_04,    0x04, int04);
RD_OFF(int_08,    0x08, int08);
RD_OFF(int_0c,    0x0c, int0c);
RD_OFF(int_10,    0x10, int10);
RD_OFF(int_14,    0x14, int14);
RD_OFF(int_18,    0x18, int18);
RD_OFF(int_1c,    0x1c, int1c);
RD_OFF(ushort_20, 0x20, ushort20);
RD_OFF(int_24,    0x24, int24);
RD_OFF(params,    0x28, params);
typedef char rd_size[(sizeof(V90RDetector) == 0x2c) ? 1 : -1];
#endif

/*
 * One store.  Everything else in the object keeps whatever was there before,
 * which is why the test seeds both sides and compares the WHOLE object: the
 * claim is not only that +0x28 is written but that the other forty bytes are
 * not.
 */
V90RDetector::V90RDetector(V90Parameters *p)
{
	params = p;
}

/*
 * One byte, `ret`.  See the header: the symbol exists only because the
 * original declared the destructor.
 */
V90RDetector::~V90RDetector()
{
}

/*
 * `reset` clears the four counters, points the polarity at +1, empties the
 * sign register and turns its two SAMPLE COUNTS into four limits.
 *
 * THE FOUR LIMITS ARE THE ARGUMENTS ROUNDED DOWN, and that is the whole of
 * the arithmetic: `(n / 6) * 6` and `(n / 12) * 12`.  The object spells both
 * with the `mul $0xaaaaaaab` reciprocal -- a division by 3 whose quotient is
 * then shifted, once for six and twice for twelve, and multiplied straight
 * back up with `lea`.  It is a rounding, not an identity: the run counters
 * step by 6 and by 12 and are compared for EQUALITY, so a limit that was not
 * a multiple of the step would never be met and the detector would never
 * fire.  Written as the division it is; the compiler emits the same
 * reciprocal.
 *
 * The object computes the twelve-sample limits from the six-sample ones --
 * `(n / 6) / 2` for `n / 12` -- which is the same value for every unsigned n
 * and is not written that way here.
 */
void
V90RDetector::reset(unsigned int rSamples, unsigned int rNotSamples)
{
	int_00 = 0;
	int_14 = 0;
	int_18 = 0;
	int_1c = 0;
	int_24 = 1;
	ushort_20 = 0;

	int_04 = (int)(rSamples / 6u * 6u);
	int_08 = (int)(rNotSamples / 6u * 6u);
	int_0c = (int)(rSamples / 12u * 12u);
	int_10 = (int)(rNotSamples / 12u * 12u);
}

/*
 * THE FOUR DETECTORS ARE ONE FUNCTION FOUR TIMES, and the object emitted all
 * four in full rather than sharing anything: 164, 113, 164 and 117 bytes with
 * no call between them.  They are written out here for the same reason, and
 * the local names are deliberately different in each so that no mutation
 * anchor over one of them can match another (finding F1264).
 *
 * WHAT THEY SHARE.  Each takes one sample, shifts its SIGN into the register
 * at +0x20 -- `bits = bits + bits` then `|= 1` when the sample is strictly
 * positive, so zero counts as negative -- and returns 0 immediately unless
 * the sample completes a group.  A group is 6 samples for the R pair and 12
 * for the Rf pair.  On the sample that completes one, the register is matched
 * against a pattern, the run counter for that pattern is advanced by the
 * group size, and the answer is non-zero only when the run has reached the
 * limit `reset` computed.  Whatever happens, the group ends with the counter
 * and the register both cleared.
 *
 * THE PATTERNS ARE THE SIGN SEQUENCES.  R is 0x38 = 111000 and 0x07 = 000111,
 * six bits; Rf is 0xccc = 110011001100 and 0x333 = 001100110011, twelve.  The
 * comparison is against the 16-bit field reloaded from the object, so a
 * seventeenth sample cannot leave a stale high bit behind.
 */
int
V90RDetector::detectR(short sample)
{
	int found = 0;
	unsigned int bits = (unsigned int)ushort_20 * 2u;
	int taken;

	if (sample > 0)
		bits |= 1u;
	ushort_20 = (unsigned short)bits;

	taken = int_00 + 1;
	if (taken != 6) {
		int_00 = taken;
		return 0;
	}

	if (ushort_20 == 0x38) {
		int_18 = 0;
		int_14 += 6;
		if (int_14 == int_04) {
			int_24 = 1;
			found = 1;
		}
	} else if (ushort_20 == 0x07) {
		int_14 = 0;
		int_18 += 6;
		if (int_18 == int_04) {
			int_24 = -1;
			found = 1;
		}
	} else {
		int_14 = 0;
		int_18 = 0;
	}

	int_00 = 0;
	ushort_20 = 0;
	return found;
}

/*
 * The R detector's opposite number: it watches for the pattern to STOP.  The
 * pattern it looks for is chosen by the polarity at +0x24 -- 0x07 while that
 * is positive and 0x38 once it is not -- which is the one the plain detector
 * last confirmed, and the answer is -1 rather than +1.
 *
 * NOTE WHICH WAY THE MATCH RUNS.  A group that MATCHES advances +0x1c and a
 * group that does not clears it, exactly as in `detectR`; what differs is
 * that there is one run counter rather than two, and that its limit is
 * +0x08, from `reset`'s second argument.
 */
int
V90RDetector::detectRNot(short sample)
{
	int verdict = 0;
	unsigned int reg = (unsigned int)ushort_20 * 2u;
	int used;

	if (sample > 0)
		reg |= 1u;
	ushort_20 = (unsigned short)reg;

	used = int_00 + 1;
	if (used != 6) {
		int_00 = used;
		return 0;
	}

	if ((unsigned int)ushort_20 == (int_24 > 0 ? 0x07u : 0x38u)) {
		int_1c += 6;
		if (int_1c == int_08)
			verdict = -1;
	} else {
		int_1c = 0;
	}

	int_00 = 0;
	ushort_20 = 0;
	return verdict;
}

/*
 * `detectR` at twelve samples to the group, against the twelve-bit patterns
 * and the twelve-sample limit at +0x0c.  Everything else is the same
 * function, and the object repeats it in full.
 */
int
V90RDetector::detectRf(short sample)
{
	int hit = 0;
	unsigned int sr = (unsigned int)ushort_20 * 2u;
	int seen;

	if (sample > 0)
		sr |= 1u;
	ushort_20 = (unsigned short)sr;

	seen = int_00 + 1;
	if (seen != 12) {
		int_00 = seen;
		return 0;
	}

	if (ushort_20 == 0xccc) {
		int_18 = 0;
		int_14 += 12;
		if (int_14 == int_0c) {
			int_24 = 1;
			hit = 1;
		}
	} else if (ushort_20 == 0x333) {
		int_14 = 0;
		int_18 += 12;
		if (int_18 == int_0c) {
			int_24 = -1;
			hit = 1;
		}
	} else {
		int_14 = 0;
		int_18 = 0;
	}

	int_00 = 0;
	ushort_20 = 0;
	return hit;
}

/*
 * `detectRNot` at twelve samples to the group: polarity picks 0x333 or
 * 0xccc, the run counter steps by twelve, and the limit is +0x10.
 */
int
V90RDetector::detectRfNot(short sample)
{
	int answer = 0;
	unsigned int shifted = (unsigned int)ushort_20 * 2u;
	int count;

	if (sample > 0)
		shifted |= 1u;
	ushort_20 = (unsigned short)shifted;

	count = int_00 + 1;
	if (count != 12) {
		int_00 = count;
		return 0;
	}

	if ((unsigned int)ushort_20 == (int_24 > 0 ? 0x333u : 0xcccu)) {
		int_1c += 12;
		if (int_1c == int_10)
			answer = -1;
	} else {
		int_1c = 0;
	}

	int_00 = 0;
	ushort_20 = 0;
	return answer;
}

/*
 * V92Mapper.cpp -- the V.92 symbol mapper: bits in, one scaled constellation
 * point out.
 *
 * Reconstructed from dsplibs.o.  All four members and the static table;
 * `include/dsplib/V92Mapper.h` carries the object map and the 0x2c that
 * `V92Phase4Modulator::V92Phase4Modulator` measures.
 *
 * As with V92ConvolutionEncoder, the claim about the constructor and the
 * destructor is that the original DECLARED both: GCC emits an out-of-line
 * constructor or destructor only for a user-declared one, and the blob has
 * C1, C2, D1 and D2 at one byte each.  `reset(short, unsigned char)` is what
 * puts the object into a usable state, so there was nothing for the
 * constructor to do.
 */

#include <stddef.h>
#include <math.h>

#include "dsplib/V92Mapper.h"

/*
 * The reference uses bare fsqrt even for a negative power.  The C++ build's
 * -fno-math-errno suppresses the library fallback without enabling unsafe
 * arithmetic.  `power` is a float; the period compiler keeps the double
 * square-root result in its extended-precision register through the divide
 * and rounds at fistps.  See docs/issue19-inline-asm.md.
 */
static inline double
v92mapper_fsqrt(double x)
{
	return sqrt(x);
}

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V92MAP_OFF(field, off, tag) \
	typedef char v92map_off_##tag[ \
	    ((int)__builtin_offsetof(V92Mapper, field) == (off)) ? 1 : -1]

V92MAP_OFF(scale,  0x00, scale);
V92MAP_OFF(mode,   0x02, mode);
V92MAP_OFF(pad_03, 0x03, pad_03);
V92MAP_OFF(bits,   0x26, bits);
V92MAP_OFF(power,  0x28, power);
typedef char v92map_size[(sizeof(V92Mapper) == 0x2c) ? 1 : -1];
#endif

V92Mapper::V92Mapper()
{
}

V92Mapper::~V92Mapper()
{
}

/*
 * The constellation amplitudes, out of .data:0x6a80 and read back as sixteen
 * signed ints because `process` loads them with `fildl (,%ebx,4)` -- a 32-bit
 * INTEGER load with a four-byte scale, so neither floats nor shorts.
 *
 * Two rows of eight, selected by `mode`: row 0 is the four-point set with its
 * upper half unused, row 1 the eight-point one.  That agrees with `reset`,
 * which pairs row 0 with two bits and row 1 with three.
 *
 * It is a static data member and therefore a defined data symbol in the blob,
 * renamed `ref_*` like everything else, so it has to exist here or the whole
 * suite fails to link.  Non-const to match: the blob's symbol is `D`.
 */
int V92Mapper::constelAmplitudeTable[16] = {
	 1,  3, -1, -3,  0,  0,  0,  0,
	 1,  3,  5,  7, -1, -3, -5, -7
};

/*
 * The byte is emitted before the short although the recovered source order
 * assigns the short first, and `mode` is tested before either store.  The
 * zero-mode arm likewise writes `power` first in source but emits `bits`
 * first.  Those inversions are GCC's schedule, not observable store-order
 * requirements; finding F10238 records the finite source-order preimages.
 *
 * 0x40a00000 is 5.0f and 0x41a80000 is 21.0f; both are stored as 32-bit
 * integer immediates, which is how GCC spells a float constant with no
 * register to spare.
 */
void
V92Mapper::reset(short scaleArg, unsigned char modeArg)
{
	scale = scaleArg;
	mode = modeArg;

	if (modeArg == 0) {
		power = 5.0f;
		bits = 2;
	} else {
		bits = 3;
		power = 21.0f;
	}
}

/*
 * Pack `bits` bytes into a symbol, look its amplitude up, normalise it by the
 * root mean power and scale it.
 *
 * THE ACCUMULATOR IS SIXTEEN BITS WIDE, and that is visible rather than
 * assumed: the loop body ends `movswl %ax,%edx`, so every partial result is
 * truncated to a short and sign-extended again.  With the two tap counts
 * `reset` installs -- two and three -- nothing can overflow and the width
 * cannot matter; t_v92convmapper.cpp therefore drives `bits` well past three
 * so that it does, because a member whose width no test can see is a claim
 * nobody has checked.
 *
 * The input is read from the top down: `dec %ecx; jns` enters the body with
 * the index already one below the count, so bit `bits - 1` is the first one
 * shifted in and bit 0 the last.
 *
 * THE DIVIDE IS `de f1`, WHICH objdump PRINTS AS ITS OWN OPPOSITE.  It reads
 * `fdivp %st,%st(1)` and IS FDIVRP -- `st(1) = st(0)/st(1)` -- so the table
 * entry is the numerator and the square root the denominator, not the other
 * way round (finding F245, and the same reading the header gives).
 *
 * The square root is `v92mapper_fsqrt` above rather than a builtin, for the
 * reason given there.
 *
 * The result comes back through `fistps` under a control word forced to
 * round-toward-zero and is then sign-extended with `cwtl`, so it is a short's
 * worth of value returned as an int.
 */
int
V92Mapper::process(unsigned char *bitsIn)
{
	short acc;
	int i;

	acc = 0;
	for (i = bits - 1; i >= 0; i--)
		acc = (short)(acc * 2 | bitsIn[i]);

	return (short)(scale * (constelAmplitudeTable[acc + 8 * mode]
				/ v92mapper_fsqrt(power)));
}

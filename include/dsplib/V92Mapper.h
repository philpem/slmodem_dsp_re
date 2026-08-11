/*
 * V92Mapper.h -- the V.92 symbol mapper: bits in, one scaled constellation
 * point out.
 *
 * Reconstructed from dsplibs.o.  Four members and one static table, 229
 * bytes; this tree defines the constructor and the destructor -- one `ret`
 * each -- and declares `reset` and `process`.
 *
 * WHY AN EMPTY CONSTRUCTOR IS WORTH A SYMBOL.  GCC emits an out-of-line
 * constructor or destructor only for a user-declared one, so the blob's C1,
 * C2, D1 and D2 say the original declared both and left the bodies empty.
 * The object is initialised by `reset(short, unsigned char)` instead, which is
 * why the constructor has nothing to do.
 *
 * THE OBJECT IS 0x2c BYTES, measured rather than bounded:
 * `V92Phase4Modulator::V92Phase4Modulator` runs `movl $0x2c,(%esp); call
 * sysdep_malloc; call V92Mapper::V92Mapper()` and keeps the pointer at its own
 * +0x70, where its destructor finds it again.  The furthest displacement any
 * member uses is +0x28 and it is four bytes wide, so the two agree.
 */

#ifndef DSPLIB_V92MAPPER_H
#define DSPLIB_V92MAPPER_H

class V92Mapper {
public:
	/* Written.  Both bodies are empty; both symbols exist. */
	V92Mapper();
	~V92Mapper();

	/*
	 * Declared and deliberately not defined; the signatures are the
	 * mangling's, `Esh` being (short, unsigned char).
	 *
	 * `reset` stores its two arguments at +0x00 and +0x02 and then picks
	 * a pair from the second: zero gives +0x26 = 2 and +0x28 = 5.0f,
	 * anything else gives 3 and 21.0f.  `process` divides the table entry
	 * at `constelAmplitudeTable[bits + 8 * (+0x02)]` by the square root of
	 * +0x28, multiplies by +0x00 and rounds toward zero.
	 */
	void reset(short scale, unsigned char mode);
	void process(unsigned char *bits);

	/*
	 * DECLARED, NOT DEFINED.  `D` in the blob, so not const, and 0x40
	 * bytes.  `process` reads it with `fildl (,%ebx,4)` -- a 32-bit
	 * INTEGER load, not a float one -- so it is sixteen ints, and that is
	 * the one thing about it this file does claim.
	 */
	static int constelAmplitudeTable[16];

	/*
	 * Public for the usual reason.  The widths are the instructions':
	 * `mov %dx,(%ebx)` and `filds (%esi)` for the short at +0x00,
	 * `mov %al,0x2(%ebx)` and `movzbl` for the byte at +0x02, `movw` for
	 * the short at +0x26, and `flds`/a 32-bit store of 0x40a00000 for the
	 * float at +0x28.
	 */

	/* +0x00  `reset`'s first argument; `process` multiplies by it. */
	short scale;

	/* +0x02  `reset`'s second argument, and the table's row selector. */
	unsigned char mode;

	/*
	 * +0x03 .. +0x25  NOT REFERENCED by either of the two members that
	 * touch the object.  Thirty-five bytes whose contents are settled by
	 * neither the constructor nor `reset`, and which nothing here names.
	 */
	unsigned char pad_03[0x23];

	/* +0x26  2 when `mode` is zero, 3 otherwise: a bit count. */
	short bits;

	/* +0x28  5.0f when `mode` is zero, 21.0f otherwise.  `process` takes
	 * its square root, so it is a mean power and the divisor normalises
	 * the constellation. */
	float power;
};

#endif /* DSPLIB_V92MAPPER_H */

/*
 * V92ModulusEncoder.cpp -- the V.92 modulus encoder.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V92ModulusEncoder.h` carries
 * the object map, the 0x54 the caller's `sysdep_malloc` measures, and why the
 * initialised range starts at +0x18 rather than at zero.
 *
 * NO DESTRUCTOR is declared, because the blob has none.  The constructor
 * initialises thirteen members and no others; the six words below them and
 * the two above are `reset`'s to fill, and the test asserts they keep
 * whatever the storage held.
 *
 * WHAT THE CLASS DOES.  `reset` takes twelve moduli out of the parameter
 * block and forms their product, kept two ways: truncated to 64 bits in
 * `product`, and exactly in `productHi`:`productLo` as two limbs in base
 * 2^63.  `progress` reads `+0x48` bits out of a byte-per-bit array, one bit
 * per byte, as a 126-bit integer and writes its twelve digits to `out` --
 * digit k reduced modulo the k'th modulus.  That is a modulus (a.k.a.
 * "shell" or mixed-radix) encoder: the mapping V.90 and V.92 use to spend a
 * frame's bits over symbol positions whose alphabets are not powers of two.
 *
 * THE ARITHMETIC OVERFLOWS ON PURPOSE, which is the whole point of the loop
 * both members contain: it shifts one factor right until the product stops
 * wrapping into the sign bit.  This tree is not built `-fwrapv`, so every
 * 64-bit multiply and every left shift below goes through `unsigned long
 * long` and is cast back -- that is bit-for-bit what the object does and is
 * defined, where the same expression written signed is not.  Divide, modulo,
 * right shift and every comparison stay SIGNED, because the object's are:
 * `idiv` by way of `__divdi3`, `sar`, `shrd`, and the sign-corrected `>> 1`.
 */

#include <stddef.h>

#include "dsplib/V92ModulusEncoder.h"

/*
 * Finding 1321: the block `reset` is handed is the one this tree already
 * models as `struct V92ParamsInfo`, and finding 1325 is why a translation
 * unit that dereferences it includes the header rather than reaching through
 * a local declaration.  The thirteen words `reset` reads all fall inside the
 * region that header leaves as `pad_00`, so they are addressed through it by
 * offset; naming them there is the job of whoever reconstructs the unpacker.
 */
#include "dsplib/V92ParamsInfo.h"

/*
 * 2^63 as the object spells it: the literal 0x8000000000000000 loaded as the
 * high half of a signed 64-bit constant, which is LLONG_MIN.  Written the
 * long way round because this build has no <limits.h> (-nostdinc++) and
 * because the direct spelling is out of range for `long long`.
 */
#define V92ME_HALF	(-0x7fffffffffffffffLL - 1)
#define V92ME_MASK	0x7fffffffffffffffLL

/*
 * The three UB-free primitives described in the header comment.  All three
 * are one instruction pair wide and are always inlined; `nm` on the built
 * object shows no symbol for any of them.
 */
static inline long long
v92me_mul(long long x, long long y)
{
	return (long long)((unsigned long long)x * (unsigned long long)y);
}

static inline long long
v92me_shl(long long x, int s)
{
	return (long long)((unsigned long long)x << s);
}

static inline long long
v92me_add(long long x, long long y)
{
	return (long long)((unsigned long long)x + (unsigned long long)y);
}

/* One 32-bit load out of the parameter block, by offset. */
static inline unsigned int
v92me_param(const struct V92ParamsInfo *p, int off)
{
	unsigned int v;

	__builtin_memcpy(&v, &p->pad_00[off], sizeof v);
	return v;
}

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V92ME_OFF(field, off, tag) \
	typedef char v92me_off_##tag[ \
	    ((int)__builtin_offsetof(V92ModulusEncoder, field) == (off)) \
	    ? 1 : -1]

V92ME_OFF(product,   0x00, product);
V92ME_OFF(productHi, 0x08, productHi);
V92ME_OFF(productLo, 0x10, productLo);
V92ME_OFF(field_18, 0x18, field_18);
V92ME_OFF(field_1c, 0x1c, field_1c);
V92ME_OFF(field_20, 0x20, field_20);
V92ME_OFF(field_24, 0x24, field_24);
V92ME_OFF(field_28, 0x28, field_28);
V92ME_OFF(field_2c, 0x2c, field_2c);
V92ME_OFF(field_30, 0x30, field_30);
V92ME_OFF(field_34, 0x34, field_34);
V92ME_OFF(field_38, 0x38, field_38);
V92ME_OFF(field_3c, 0x3c, field_3c);
V92ME_OFF(field_40, 0x40, field_40);
V92ME_OFF(field_44, 0x44, field_44);
V92ME_OFF(field_48, 0x48, field_48);
V92ME_OFF(field_4c, 0x4c, field_4c);
V92ME_OFF(field_50, 0x50, field_50);
typedef char v92me_size[(sizeof(V92ModulusEncoder) == 0x54) ? 1 : -1];
#endif

/*
 * Thirteen scalars, one `movl $0x0` each in the object -- so a
 * member-initialiser list and not a loop, and not `memset(this, 0, ...)`,
 * which would also have cleared +0x00..+0x14 and +0x4c, +0x50.
 */
V92ModulusEncoder::V92ModulusEncoder()
	: field_18(0), field_1c(0), field_20(0), field_24(0), field_28(0),
	  field_2c(0), field_30(0), field_34(0), field_38(0), field_3c(0),
	  field_40(0), field_44(0), field_48(0)
{
}

/*
 * reset -- take the twelve moduli and the bit count out of the parameter
 * block and form the product three times over.
 *
 * WHY THE PRODUCT IS FORMED THREE TIMES.  The object multiplies all twelve
 * together, then the first six, then the last six, reloading every modulus
 * from `this` each time rather than reusing a subexpression.  That is not a
 * missed CSE: the stores to `product` and `productHi` in between are through
 * `this`, may alias the moduli as far as the compiler can tell, and kill
 * every load.  The same is true of the twelve-fold product written out a
 * second time in the `n == 8` arm below.
 *
 * THE LOOP is the normalisation: `a * (b >> n) > 0` is false exactly when
 * the product has run into or past the sign bit, so `n` is the number of low
 * bits of `b` that have to be given up for `a * b` to fit in 63.  `n` is
 * capped at 8, and reaching 8 means it never overflowed at all, in which case
 * the exact product IS the low limb and the high limb is zero.  Otherwise the
 * 64-bit `u` is redistributed: the top `62 - n` bits become the high limb and
 * the rest, shifted back up and masked to 63 bits, the low one.
 *
 * `a * (b >> n) > 0` is the object's `((x >> 63) - x) < 0` read off the sign
 * bit, which is that comparison and not an approximation of it: for x > 0 the
 * difference is -x and negative, for x <= 0 it is ~x or 0 and is not
 * (finding 1378).
 */
void
V92ModulusEncoder::reset(V92MappingParams *params)
{
	const struct V92ParamsInfo *p = (const struct V92ParamsInfo *)params;
	long long a, b, u;
	int n;

	field_48 = v92me_param(p, 0x00);
	field_18 = v92me_param(p, 0x1c);
	field_1c = v92me_param(p, 0x20);
	field_20 = v92me_param(p, 0x24);
	field_24 = v92me_param(p, 0x28);
	field_28 = v92me_param(p, 0x2c);
	field_2c = v92me_param(p, 0x30);
	field_30 = v92me_param(p, 0x34);
	field_34 = v92me_param(p, 0x38);
	field_38 = v92me_param(p, 0x3c);
	field_3c = v92me_param(p, 0x40);
	field_40 = v92me_param(p, 0x44);
	field_44 = v92me_param(p, 0x48);

	product = (long long)((unsigned long long)field_18 * field_1c *
			      field_20 * field_24 * field_28 * field_2c *
			      field_30 * field_34 * field_38 * field_3c *
			      field_40 * field_44);

	a = (long long)((unsigned long long)field_18 * field_1c * field_20 *
			field_24 * field_28 * field_2c);

	/*
	 * Dead: both arms below store over it.  The object does it too --
	 * GCC 3.4 does not eliminate a dead store through a pointer -- and it
	 * is reproduced rather than dropped.  docs/deviations.md D263.
	 */
	productHi = a;

	b = (long long)((unsigned long long)field_30 * field_34 * field_38 *
			field_3c * field_40 * field_44);

	n = 0;
	while (v92me_mul(a, b >> n) > 0 && n <= 7)
		n++;

	if (n == 8) {
		productHi = 0;
		productLo = (long long)((unsigned long long)field_18 *
					field_1c * field_20 * field_24 *
					field_28 * field_2c * field_30 *
					field_34 * field_38 * field_3c *
					field_40 * field_44);
	} else {
		u = v92me_mul(a, b >> (n + 1));
		productHi = u >> (62 - n);
		productLo = v92me_shl(u, n + 1) & V92ME_MASK;
	}

	field_4c = 0;
	field_50 = 0;
}

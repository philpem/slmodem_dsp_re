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
 * a local declaration.  The thirteen words `reset` reads used to fall inside
 * the region that header left as `pad_00` and were addressed by offset
 * through a `v92me_param(p, off)` helper; the unpacker has since named them,
 * so they are `p->K` and `p->m[0..11]` and the helper is gone.
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

static inline long long
v92me_sub(long long x, long long y)
{
	return (long long)((unsigned long long)x - (unsigned long long)y);
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

	field_48 = p->K;
	field_18 = p->m[0];
	field_1c = p->m[1];
	field_20 = p->m[2];
	field_24 = p->m[3];
	field_28 = p->m[4];
	field_2c = p->m[5];
	field_30 = p->m[6];
	field_34 = p->m[7];
	field_38 = p->m[8];
	field_3c = p->m[9];
	field_40 = p->m[10];
	field_44 = p->m[11];

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

/*
 * progress -- turn a frame of bits into twelve digits, or report the digits'
 * ranges.
 *
 * +0x50 SELECTS AMONG THREE THINGS, and anything else returns at once.  Cases
 * 1 and 2 do not look at the bits at all: they write `modulus - 1` -- the
 * largest digit each position can hold -- for six of the twelve and zero for
 * the other six, case 1 taking the first two of every group of four and case
 * 2 the last two.  Case 0 is the encoder proper.
 *
 * CASE 0 READS `+0x48` BITS, ONE PER BYTE, MOST SIGNIFICANT FIRST: `bytes[i]`
 * contributes its bit 0 and nothing else, and `i` counts DOWN from the bit
 * count to zero.  The result is a 126-bit integer in two limbs, `hi` taking
 * everything above bit 62 and `lo` the low 63 bits, which is the same base
 * 2^63 `reset` left the product in.
 *
 * THE MIDDLE OF IT IS A COMPLEMENT, and +0x4c is the state that decides.  The
 * value is compared against half the product -- `productHi / 2` over
 * `((productHi % 2) << 62) + (productLo - 1) / 2`, which is `(product - 1) / 2`
 * carried in the same two limbs -- and +0x4c is exclusive-ORed with the
 * answer.  When +0x4c was already set, the value used is `product - 1 - value`
 * instead of `value`.  So consecutive calls alternate between the number and
 * its complement whenever the number lands in the top half, which is a
 * one-bit running disparity of exactly the kind V.90 and V.92 use to keep a
 * transmitted sequence balanced.  ONE CALL CANNOT SEE IT: the fixture drives
 * several in a row against the same object and compares after each.
 *
 * THE DIVISION IS THE MIXED-RADIX CONVERSION, and the awkward part is that
 * the dividend is 126 bits and `__divdi3` is 64.  It is done with 2^63 held
 * as the negative literal 0x8000000000000000, so
 *
 *     value / m  =  lo / m  +  (2^63 / m) * hi  +  correction
 *
 * where the correction only matters when `hi` is big enough to need it.  Every
 * digit after the first two is a plain `v % m` then `v = (v - digit) / m`, and
 * THE SUBTRACTION READS THE DIGIT BACK OUT OF `out[]`, where it has been
 * truncated to 32 bits and is then widened UNSIGNED.  That is not the same as
 * subtracting the remainder: `v` is negative on the paths that go through
 * 2^63, so the remainder is negative too, and the round trip through
 * `unsigned int` is what the object does and what makes the digits come out
 * right.  A signed temporary gives different answers and the mutation set
 * proves the fixture sees the difference.
 *
 * THE NORMALISATION BLOCK from `reset` appears here twice more, once for the
 * first modulus and once for the second.  It cannot have been a function in
 * the original: its `n == 8` arm RE-ISSUES `__divdi3` and `__moddi3` for
 * values a function would have had in hand, and GCC cannot rematerialise a
 * call -- so the text really was repeated, and it is repeated here (finding
 * 1379).  The last modulus, +0x44, is never read by case 0 at all: `out[11]`
 * is the bare quotient left over.  docs/deviations.md D264.
 */
void
V92ModulusEncoder::progress(unsigned char *bytes, unsigned int *out)
{
	switch (field_50) {
	case 0: {
		long long hi = 0, lo = 0;
		long long a, b, hh, ll, c, d, v;
		int i, n, t, over;

		if (field_48 > 63) {
			for (i = (int)(field_48 - 1); i > 62; i--)
				hi = v92me_shl(hi, 1) | (bytes[i] & 1);
			i = 62;
		} else
			i = (int)(field_48 - 1);

		for (; i >= 0; i--)
			lo = v92me_shl(lo, 1) | (bytes[i] & 1);

		/* (product - 1) / 2, in the same two limbs. */
		a = productHi;
		b = productLo;
		hh = a / 2;
		ll = v92me_add(v92me_shl(a % 2, 62), v92me_sub(b, 1) / 2);

		over = (hi > hh) || (hi == hh && lo >= ll);

		d = hi;
		c = lo;
		t = (int)field_4c;
		if (t) {
			d = a ? v92me_sub(a, hi) : 0;
			c = v92me_sub(v92me_sub(b, lo), 1);
		}
		field_4c = (unsigned int)(t ^ over);

		/* The complement above can leave the low limb borrowing. */
		if (d > 0 && c < 0) {
			c = v92me_add(c, V92ME_HALF);
			d = v92me_sub(d, 1);
		}

		if (d > 0) {
			long long r = d % field_18;
			long long s = v92me_mul(r, field_18);
			long long q = V92ME_HALF / field_18;
			long long e = 0;

			if (s > 1) {
				long long x;

				n = 0;
				while (v92me_mul(s, q >> n) > 0 && n <= 7)
					n++;

				if (n == 8)
					x = v92me_mul(v92me_mul(
						V92ME_HALF / field_18,
						d % field_18), field_18);
				else {
					long long u = v92me_mul(s,
							        q >> (n + 1));

					x = v92me_shl(u, n + 1) & V92ME_MASK;
				}

				e = v92me_sub(V92ME_HALF, x) % field_18;
			}

			out[0] = (unsigned int)(v92me_add(e, c % field_18) %
						field_18);

			if (d < field_18) {
				v = v92me_add(c / field_18,
					      v92me_mul(d, V92ME_HALF /
							   field_18));
				out[1] = (unsigned int)(v % field_1c);
				v = v92me_sub(v, out[1]) / field_1c;
			} else {
				long long dq = d / field_18;

				v = v92me_add(c / field_18,
					      v92me_mul(V92ME_HALF / field_18,
							d % field_18));

				if (dq > 0) {
					long long r1 = dq % field_1c;
					long long s1 = v92me_mul(r1, field_1c);
					long long q1 = V92ME_HALF / field_1c;
					long long e1 = 0;

					if (s1 > 1) {
						long long x1;

						n = 0;
						while (v92me_mul(s1, q1 >> n) >
						       0 && n <= 7)
							n++;

						if (n == 8)
							x1 = v92me_mul(
							     v92me_mul(
							      V92ME_HALF /
							       field_1c,
							      dq % field_1c),
							     field_1c);
						else {
							long long u1 =
							  v92me_mul(s1, q1 >>
								    (n + 1));

							x1 = v92me_shl(u1,
								       n + 1) &
							     V92ME_MASK;
						}

						e1 = v92me_sub(V92ME_HALF, x1)
						     % field_1c;
					}

					out[1] = (unsigned int)(v92me_add(e1,
							v % field_1c) %
							field_1c);
					v = v92me_add(v / field_1c,
						      v92me_mul(V92ME_HALF /
								field_1c,
								dq %
								field_1c));
				} else {
					out[1] = (unsigned int)(v % field_1c);
					v = v92me_sub(v, out[1]) / field_1c;
				}
			}
		} else {
			out[0] = (unsigned int)(c % field_18);
			c = v92me_sub(c, out[0]);
			v = c / field_18;
			out[1] = (unsigned int)(v % field_1c);
			v = v92me_sub(v, out[1]) / field_1c;
		}

		out[2] = (unsigned int)(v % field_20);
		v = v92me_sub(v, out[2]) / field_20;
		out[3] = (unsigned int)(v % field_24);
		v = v92me_sub(v, out[3]) / field_24;
		out[4] = (unsigned int)(v % field_28);
		v = v92me_sub(v, out[4]) / field_28;
		out[5] = (unsigned int)(v % field_2c);
		v = v92me_sub(v, out[5]) / field_2c;
		out[6] = (unsigned int)(v % field_30);
		v = v92me_sub(v, out[6]) / field_30;
		out[7] = (unsigned int)(v % field_34);
		v = v92me_sub(v, out[7]) / field_34;
		out[8] = (unsigned int)(v % field_38);
		v = v92me_sub(v, out[8]) / field_38;
		out[9] = (unsigned int)(v % field_3c);
		v = v92me_sub(v, out[9]) / field_3c;
		out[10] = (unsigned int)(v % field_40);
		v = v92me_sub(v, out[10]) / field_40;
		out[11] = (unsigned int)v;
		break;
	}

	case 1:
		out[0] = field_18 - 1;
		out[1] = field_1c - 1;
		out[2] = 0;
		out[3] = 0;
		out[4] = field_28 - 1;
		out[5] = field_2c - 1;
		out[6] = 0;
		out[7] = 0;
		out[8] = field_38 - 1;
		out[9] = field_3c - 1;
		out[10] = 0;
		out[11] = 0;
		break;

	case 2:
		out[0] = 0;
		out[1] = 0;
		out[2] = field_20 - 1;
		out[3] = field_24 - 1;
		out[4] = 0;
		out[5] = 0;
		out[6] = field_30 - 1;
		out[7] = field_34 - 1;
		out[8] = 0;
		out[9] = 0;
		out[10] = field_40 - 1;
		out[11] = field_44 - 1;
		break;
	}
}

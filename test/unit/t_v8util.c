/*
 * t_v8util.c -- differential test of the V.8 arithmetic leaves.
 *
 * All six are global symbols, so both sides can be called by name and swept
 * exhaustively where the domain allows it: `v8_mpyint` and `v8_absfn` are
 * checked over every short, and `v8_cosread` over every index, so those three
 * are proved rather than sampled.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v8.h"

extern short ref_v8_mpyint(short a, short b);
extern short ref_v8_absfn(short x);
extern short ref_v8_cosread(unsigned char phase);
extern void ref_v8_crc(struct v8_handshake *hs, int bit);
extern void ref_v8_copycoeff(short *dst, const short *src, short n);
extern void ref_v8_dftenergy(struct v8_dft_bin *bin, short n, short shift);

static int
t_mpyint(void)
{
	long a, b;

	diff_begin("v8_mpyint: Q14 multiply");
	/*
	 * The full cross product is 2^32 pairs; step one operand coarsely and
	 * the other finely, and include the ends and the sign boundaries
	 * exactly.
	 */
	for (a = -32768; a <= 32767; a += 37) {
		for (b = -32768; b <= 32767; b += 1021)
			diff_eq_int("mpyint(%ld, .)",
				    v8_mpyint((short)a, (short)b),
				    ref_v8_mpyint((short)a, (short)b), a);
	}
	for (b = -32768; b <= 32767; b++) {
		diff_eq_int("mpyint(-32768, %ld)", v8_mpyint(-32768, (short)b),
			    ref_v8_mpyint(-32768, (short)b), b);
		diff_eq_int("mpyint(32767, %ld)", v8_mpyint(32767, (short)b),
			    ref_v8_mpyint(32767, (short)b), b);
		diff_eq_int("mpyint(-1, %ld)", v8_mpyint(-1, (short)b),
			    ref_v8_mpyint(-1, (short)b), b);
	}
	return diff_end();
}

static int
t_absfn(void)
{
	long x;

	diff_begin("v8_absfn: absolute value, exhaustive");
	for (x = -32768; x <= 32767; x++)
		diff_eq_int("absfn(%ld)", v8_absfn((short)x),
			    ref_v8_absfn((short)x), x);
	/* The corner that has no positive answer. */
	diff_eq_int("absfn(-32768) is still negative", v8_absfn(-32768) < 0, 1,
		    0);
	return diff_end();
}

static int
t_cosread(void)
{
	int i;
	int nonzero = 0;

	diff_begin("v8_cosread: the whole table");
	for (i = 0; i < 256; i++) {
		short v = v8_cosread((unsigned char)i);

		diff_eq_int("cosread(%ld)", v, ref_v8_cosread((unsigned char)i),
			    i);
		if (v != 0)
			nonzero++;
	}
	/* Shape, so a table of zeros could not agree with itself. */
	diff_eq_int("quarter cycle is zero", v8_cosread(64), 0, 0);
	diff_eq_int("peak is Q14 one", v8_cosread(0), 16384, 0);
	diff_eq_int("the table is not flat (%ld non-zero)", nonzero > 250, 1,
		    nonzero);
	return diff_end();
}

static int
t_crc(void)
{
	struct v8_handshake a, b;
	int i, bit;

	diff_begin("v8_crc: CRC-16-CCITT bit steps");

	/* Every starting register against both bit values. */
	for (i = -32768; i <= 32767; i += 7) {
		for (bit = 0; bit <= 1; bit++) {
			memset(&a, 0, sizeof(a));
			memset(&b, 0, sizeof(b));
			a.crc = b.crc = (short)i;
			ref_v8_crc(&a, bit);
			v8_crc(&b, bit);
			diff_eq_int("crc(%ld)", b.crc, a.crc, i);
		}
	}

	/*
	 * A whole message through both, so an error that cancels itself on a
	 * single step still shows up.
	 */
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	for (i = 0; i < 4096; i++) {
		bit = (i * 2654435761u) >> 28 & 1;
		ref_v8_crc(&a, bit);
		v8_crc(&b, bit);
		diff_eq_int("streamed bit %ld", b.crc, a.crc, i);
	}
	diff_eq_int("the register actually moved (%ld)", a.crc != 0, 1, a.crc);

	/*
	 * Only the low half of `bit` is looked at, so a value that is non-zero
	 * overall but zero in its low 16 bits counts as a zero bit.
	 */
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	a.crc = b.crc = 0x1234;
	ref_v8_crc(&a, 0x10000);
	v8_crc(&b, 0x10000);
	diff_eq_int("only the low half of the bit counts", b.crc, a.crc, 0);

	return diff_end();
}

static int
t_copycoeff(void)
{
	short src[64], dst_a[64], dst_b[64];
	int n, i;

	diff_begin("v8_copycoeff");
	for (i = 0; i < 64; i++)
		src[i] = (short)(i * 517 - 9000);

	for (n = 0; n <= 64; n++) {
		memset(dst_a, 0x5a, sizeof(dst_a));
		memset(dst_b, 0x5a, sizeof(dst_b));
		ref_v8_copycoeff(dst_a, src, (short)n);
		v8_copycoeff(dst_b, src, (short)n);
		for (i = 0; i < 64; i++)
			diff_eq_int("n=%ld", dst_b[i], dst_a[i], n);
	}

	/* A negative count copies nothing rather than running away. */
	memset(dst_a, 0x5a, sizeof(dst_a));
	memset(dst_b, 0x5a, sizeof(dst_b));
	ref_v8_copycoeff(dst_a, src, -3);
	v8_copycoeff(dst_b, src, -3);
	diff_eq_int("negative count copies nothing",
		    memcmp(dst_a, dst_b, sizeof(dst_a)) == 0, 1, 0);
	diff_eq_int("and really nothing", dst_b[0], (short)0x5a5a, 0);

	return diff_end();
}

static int
t_dftenergy(void)
{
	struct v8_dft_bin a[16], b[16];
	int shift, i, n;
	long nonzero = 0;

	diff_begin("v8_dftenergy");
	for (shift = 0; shift <= 15; shift++) {
		for (n = 0; n <= 16; n++) {
			for (i = 0; i < 16; i++) {
				a[i].f00 = b[i].f00 = i;
				a[i].re = b[i].re = (i - 8) * 0x01234567;
				a[i].im = b[i].im = (i * 7 - 40) * 0x00765432;
				a[i].energy = b[i].energy = 0x1234;
				a[i].f0e = b[i].f0e = 0x4321;
			}
			ref_v8_dftenergy(a, (short)n, (short)shift);
			v8_dftenergy(b, (short)n, (short)shift);
			for (i = 0; i < 16; i++) {
				diff_eq_int("shift=%ld energy", b[i].energy,
					    a[i].energy, shift);
				diff_eq_int("shift=%ld untouched", b[i].f0e,
					    a[i].f0e, shift);
				if (a[i].energy != 0)
					nonzero++;
			}
		}
	}
	diff_eq_int("energies were actually computed (%ld)", nonzero > 100, 1,
		    nonzero);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= t_mpyint();
	rc |= t_absfn();
	rc |= t_cosread();
	rc |= t_crc();
	rc |= t_copycoeff();
	rc |= t_dftenergy();
	return rc;
}

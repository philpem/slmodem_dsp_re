/*
 * v34shell.c -- ITU-T V.34: the shell mapper's index arithmetic.
 *
 * Reconstructed under the fast pass (docs/fastpass.md).  One function so
 * far; see the note in v34shell.h about why this one and not its callers.
 *
 * WHAT IT COMPUTES.  Three levels of the same step, each turning a pair of
 * running totals into a cumulative count:
 *
 *     level 1   sub[0..3]  -> c1, d1   against t1
 *     level 2   sub[4..7]  -> c2, d2   against t1
 *     level 3   d1 + d2               against t2, plus t3 at the end
 *
 * and each level is `table[c] * clamped + base`, plus a correlation of the
 * table against itself reversed:
 *
 *     sum = t[0]*t[d] + t[1]*t[d-1] + ... + t[c-1]*t[d-c+1]
 *
 * which is the convolution that counts how many ways the remaining energy
 * can be split -- the shell mapping's whole content.  Level 3 correlates
 * over `d1` terms rather than over its own `c`, which is not a typo here:
 * the loop counter is loaded from the first group's total.
 *
 * TWO CLAMPS PER LEVEL, and they are independent of each other:
 *
 *     if (d - c >= n)  the second addend becomes n - sub[3]  (or sub[7])
 *     if (c >= n)      the base becomes        n - sub[1]  (or sub[5])
 *
 * where n is `count - 1`.  Neither is an else-branch of the other, so both
 * can fire on the same group.
 *
 * MIXED SIGNEDNESS.  Within each group of four the first and third entries
 * are read signed and the second and fourth unsigned, and the running totals
 * are truncated to 16 bits between the two additions.  There is no reading
 * under which that is uniform, and it changes the result whenever a
 * sub-index has bit 15 set, so it is reproduced rather than tidied.
 */

#include "dsplib/v34shell.h"

/*
 * `t[0]*t[d] + t[1]*t[d-1] + ...`, `n` terms.
 *
 * The original spells this out three times; the two operands walk towards
 * each other from opposite ends and the counter is decremented as a short,
 * so a negative `n` runs zero times rather than wrapping.
 */
static int
shell_correlate(const short *t, int d, int n)
{
	int sum = 0;
	int lo = 0;
	short i;

	/*
	 * Two cursors moving towards each other.  Advancing the base pointer
	 * AND decrementing `d` would leave `t[d]` fixed, which is a different
	 * sum entirely and passes any test that only checks it runs.
	 */
	for (i = (short)n; i > 0; i = (short)(i - 1)) {
		sum += (int)(unsigned short)t[lo] * (unsigned short)t[d];
		lo++;
		d--;
	}

	return sum;
}

/*
 * One group of four: fold sub[0..3] into a cumulative count.
 *
 * `c` is the total after two entries and `d` after all four, both truncated
 * to 16 bits.  The two clamps described in the file comment are applied to
 * the third and first entries respectively.
 */
static int
shell_group(const struct v34_shell *s, const short *sub, int n, int *c_out,
	    int *d_out)
{
	int e0 = (unsigned short)sub[0];
	int e1 = (unsigned short)sub[1];
	int e2 = (short)sub[2];
	int e3 = (unsigned short)sub[3];
	int c, d;
	int scale, base;

	c = (unsigned short)(e0 + e1);
	d = (unsigned short)(e0 + e1 + e2 + e3);

	scale = e2;
	if (d - c >= n)
		scale = n - (short)e3;

	base = (short)e0;
	if ((unsigned short)c >= (unsigned)n)
		base = n - (short)e1;

	*c_out = c;
	*d_out = d;

	return (int)(unsigned short)s->t1[c] * scale + base
	       + shell_correlate(s->t1, d, (short)c);
}

int
shellDemapper(void *shellp)
{
	struct v34_shell *s = (struct v34_shell *)shellp;
	int n = (unsigned short)(s->count - 1);
	int c1, d1, c2, d2, dd;
	int a, b;

	/* A count of one leaves nothing to decide. */
	if (n == 0)
		return 0;

	a = shell_group(s, &s->sub[0], n, &c1, &d1);
	b = shell_group(s, &s->sub[4], n, &c2, &d2);

	dd = (unsigned short)(d1 + d2);

	/*
	 * Level three correlates `d1` terms, taken from the FIRST group --
	 * not `c` of its own, which is what the two levels below use.
	 */
	return (int)(unsigned short)s->t2[d1] * b + a
	       + shell_correlate(s->t2, dd, (short)d1)
	       + s->t3[dd];
}

/*
 * ---------------------------------------------------------------------------
 * putFrame -- emit one mapped frame through the object's bit sink.
 *
 * Seventeen or eighteen calls to a function pointer the object carries at
 * +0xe48, each handed a value and a bit count.  The shape is one wide field
 * followed by four identical groups:
 *
 *     wide                       the shell index
 *     (1, small, w, w) x 4       one group per 2D symbol of the 8D frame
 *
 * where `w` is `fa14` and `small` is normally 2.  Four groups of four is
 * V.34's 8D frame -- four 2D symbols -- so the single bit per group is the
 * differential quadrant bit and the two `w`-wide fields are the point.
 *
 * THREE WAYS TO SEND THE WIDE FIELD, and the third sends nothing:
 *
 *   nb > 16   split: sixteen bits from frame[0], then `nb & 15` from
 *             frame[1].  frame[1] is read on this path ONLY.
 *   nb > 0    all of it from frame[0].
 *   otherwise no call at all, and the group widths change instead: `small`
 *             becomes 2 minus whether fa04 is 8 or less, and the LAST
 *             group's width becomes that plus nb -- which is negative here,
 *             so the final group is narrower than the other three.
 *
 * That last branch is the only thing distinguishing the fourth group from
 * the first three; on the two normal paths all four are identical, and the
 * separate local the object keeps for it looks redundant until this path
 * uses it.
 */
void
putFrame(void *shellp)
{
	struct v34_shell *s = (struct v34_shell *)shellp;
	v34_putbits_fn put = s->put_bits;
	const short *v = s->frame;
	int w = s->fa14;
	int small = 2;
	int small_last = 2;
	int sum;
	int nb;
	int g;

	sum = (unsigned short)s->fa08 + (unsigned short)s->fa06;

	if ((unsigned short)s->fa00 > (unsigned short)sum) {
		s->fa08 = (short)sum;
		nb = s->fa10;
	} else {
		s->fa08 = (short)(sum - s->fa00);
		nb = s->fa0e;
	}

	if (nb > 16) {
		put(s, (unsigned short)v[0], 16);
		put(s, (unsigned short)v[1], nb & 15);
	} else if (nb > 0) {
		put(s, (unsigned short)v[0], nb);
	} else {
		small = 2 - ((unsigned short)s->fa04 <= 8 ? 1 : 0);
		small_last = (short)(nb + small);
	}

	for (g = 0; g < 4; g++) {
		const short *p = &v[2 + g * 4];

		put(s, (unsigned short)p[0], 1);
		put(s, (unsigned short)p[1], g == 3 ? small_last : small);
		put(s, (unsigned short)p[2], w);
		put(s, (unsigned short)p[3], w);
	}
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned to what shellDemapper and putFrame read.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34SH_ASSERT(field, off) \
	typedef char v34sh_off_##field[ \
		((int)__builtin_offsetof(struct v34_shell, field) == (off)) \
		? 1 : -1]

V34SH_ASSERT(count, 0xa12);
V34SH_ASSERT(t1, 0xa48);
V34SH_ASSERT(t2, 0xb48);
V34SH_ASSERT(t3, 0xc48);
V34SH_ASSERT(sub, 0xe9c);
V34SH_ASSERT(put_bits, 0xe48);
V34SH_ASSERT(frame, 0xe50);
V34SH_ASSERT(fa00, 0xa00);
V34SH_ASSERT(fa14, 0xa14);

#endif

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
 * Layout, pinned to what shellDemapper reads.
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

#endif

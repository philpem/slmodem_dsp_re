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
 * ---------------------------------------------------------------------------
 * The two tables decodeDepth reads.  Emitted here because they belong to the
 * mapper, and ahead of decodeDepth itself because they are pure data and can
 * be verified against the blob on their own.
 */

/*
 * kLookup -- 16 entries at .rodata+0xec0, indexed by a four-bit code the
 * decoder assembles from three separate pairs of bits.
 *
 * As a 4x4 it is a Latin square, every row and column a permutation of
 * 0..3:
 *
 *     0 1 3 2
 *     1 0 2 3
 *     2 3 1 0
 *     3 2 0 1
 *
 * which is the group operation on V.34's four quadrants -- not XOR, which
 * would give 0 1 2 3 on the first row.  Row 0 and column 0 are not the
 * identity either, so the table is a relabelling of the group as well as
 * the operation.  Which relabelling is a question for task #47.
 */
const short kLookup[16] = {
	    0,     1,     3,     2,     1,     0,     2,     3,
	    2,     3,     1,     0,     3,     2,     0,     1,
};

/*
 * grid -- 529 entries at .rodata+0x2060, in [0, 415] with 416 distinct
 * values, indexed by `(x + 0x408) >> 2`.
 *
 * The odd count is the object's: 0x422 bytes is 529 shorts, not a round
 * number, so either the last entry is padding or the table is genuinely
 * 23 x 23.  Emitted exactly as found rather than rounded to a guess.
 */
const short grid[529] = {
	    0,     0,     0,     0,     0,     0,     0,   411,
	  389,   374,   366,   364,   368,   381,   393,     0,
	    0,     0,     0,     0,     0,     0,     0,     0,
	    0,     0,     0,     0,   405,   370,   344,   321,
	  309,   301,   297,   305,   317,   334,   356,   385,
	    0,     0,     0,     0,     0,     0,     0,     0,
	    0,     0,   383,   346,   313,   286,   262,   252,
	  241,   239,   246,   256,   276,   295,   325,   363,
	  407,     0,     0,     0,     0,     0,     0,     0,
	  377,   333,   293,   260,   233,   211,   200,   192,
	  188,   196,   204,   223,   245,   278,   312,   352,
	  404,     0,     0,     0,     0,     0,   382,   332,
	  287,   250,   215,   184,   169,   153,   145,   143,
	  151,   159,   178,   202,   231,   264,   308,   358,
	  413,     0,     0,     0,   403,   345,   292,   249,
	  205,   176,   150,   130,   114,   107,   105,   109,
	  120,   136,   161,   191,   227,   268,   319,   373,
	    0,     0,     0,   369,   311,   259,   214,   175,
	  139,   116,    95,    82,    74,    70,    76,    86,
	  104,   129,   157,   195,   235,   285,   342,   399,
	    0,   410,   343,   284,   232,   183,   149,   115,
	   89,    68,    53,    46,    44,    51,    61,    78,
	   99,   132,   168,   209,   258,   315,   376,     0,
	  388,   320,   261,   210,   167,   128,    94,    67,
	   47,    34,    27,    23,    29,    40,    57,    81,
	  111,   147,   187,   237,   291,   351,     0,   372,
	  307,   251,   199,   152,   113,    80,    52,    33,
	   19,    12,    10,    14,    26,    42,    66,    97,
	  134,   174,   225,   280,   341,   409,   365,   300,
	  240,   190,   144,   106,    73,    45,    25,    11,
	    3,     2,     7,    18,    36,    59,    88,   124,
	  166,   217,   272,   331,   397,   362,   296,   238,
	  186,   142,   103,    69,    43,    22,     9,     1,
	    0,     5,    16,    32,    56,    85,   122,   163,
	  213,   267,   328,   395,   367,   304,   244,   194,
	  148,   108,    75,    50,    28,    13,     6,     4,
	    8,    21,    38,    63,    93,   127,   171,   219,
	  275,   336,   402,   380,   316,   255,   203,   158,
	  119,    84,    60,    39,    24,    17,    15,    20,
	   30,    49,    72,   101,   138,   182,   230,   283,
	  348,   415,   392,   330,   274,   222,   177,   135,
	  102,    77,    55,    41,    35,    31,    37,    48,
	   65,    91,   118,   155,   198,   248,   303,   361,
	    0,     0,   355,   294,   243,   201,   160,   126,
	   98,    79,    64,    58,    54,    62,    71,    90,
	  112,   141,   180,   221,   271,   323,   387,     0,
	    0,   384,   324,   277,   229,   189,   156,   131,
	  110,    96,    87,    83,    92,   100,   117,   140,
	  172,   208,   254,   299,   354,     0,     0,     0,
	    0,   360,   310,   263,   226,   193,   165,   146,
	  133,   123,   121,   125,   137,   154,   179,   207,
	  242,   289,   338,   391,     0,     0,     0,     0,
	  406,   350,   306,   266,   234,   206,   185,   173,
	  164,   162,   170,   181,   197,   220,   253,   288,
	  327,   379,     0,     0,     0,     0,     0,     0,
	  401,   357,   318,   282,   257,   236,   224,   216,
	  212,   218,   228,   247,   270,   298,   337,   378,
	    0,     0,     0,     0,     0,     0,     0,     0,
	  412,   371,   340,   314,   290,   279,   269,   265,
	  273,   281,   302,   322,   353,   390,     0,     0,
	    0,     0,     0,     0,     0,     0,     0,     0,
	    0,   398,   375,   349,   339,   329,   326,   335,
	  347,   359,   386,     0,     0,     0,     0,     0,
	    0,     0,     0,     0,     0,     0,     0,     0,
	    0,     0,     0,   408,   396,   394,   400,   414,
	    0,     0,     0,     0,     0,     0,     0,     0,
	    0,
};

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
 * decodeDepth -- walk the trellis back and decode one 8D frame.
 *
 * Three parts: a traceback, then the same 4D decode twice, then a pair of
 * grid lookups.
 *
 * THE TRACEBACK.  `state_idx` picks a starting state; the walk then takes 31
 * steps through a 32x16 table, at each step indexing by `(state << 4) +
 * branch`, taking the entry's HIGH byte as the next branch and stepping the
 * state down by one modulo 32.  The entry's LOW byte at the final position is
 * the decision, and it is read UNSIGNED where every intermediate high byte is
 * read SIGNED -- same address, two byte lanes, two signednesses.
 *
 * THE FOUR-WAY PREAMBLES.  `code >> 2` and `code & 3` each select how a pair
 * of per-state parameters is nudged: each may be pushed two counts up or two
 * down depending on how the state's stored value compares with `divisor`
 * times the parameter.  The mapping is not the bit pattern it looks like --
 *
 *     0   neither
 *     1   the second only
 *     2   BOTH
 *     3   the first only
 *
 * -- because case 2 falls through into case 1's block rather than jumping
 * past it, and case 3 jumps out.  Reading it as "bit 0 does B, bit 1 does A"
 * gives the wrong answer for exactly half the cases.
 *
 * THE DECODE, run twice.  Six complex taps against two coefficient rows,
 * both accumulators seeded with 0x1fff for rounding; each result rounded
 * toward zero, shifted down 14, folded against a mask derived from `wrap`,
 * then shifted down 7.  The delay line then shifts by one COMPLEX pair and
 * takes the new one.  A four-bit code assembled from three separate pairs of
 * bits indexes `kLookup` for the quadrant.
 *
 * The two halves are the same computation on different parameters, which is
 * why they are one function here; the object emits them twice with different
 * stack slots throughout.  See finding 132.  The one asymmetry worth naming:
 * the second accumulator's mask variable is AND-ed in place rather than
 * copied, in both halves -- harmless because neither reuses it, but it is
 * why the two look less alike than they are.
 */

/* Round toward zero, drop 14 bits, fold against the wrap, drop 7 more. */
static int
depth_quant(int acc, int lim, int maskhi, int mask2, short *q_out)
{
	unsigned v = (unsigned)(acc < 0 ? acc + 1 : acc);
	int q;
	int t;

	q = (short)(unsigned short)(v >> 14);
	*q_out = (short)(v >> 14);

	/*
	 * The fold test is SIXTEEN bits wide (`cmp 0x6c(%esp),%ax`), even
	 * though the AND that feeds it is 32.  Comparing the full words
	 * agrees only while the high half happens to match.
	 */
	t = (short)(((unsigned)(v >> 14) & (unsigned)maskhi)) == (short)lim
	    ? q : q + lim;

	return (int)(short)(((unsigned)t >> 7) & (unsigned)mask2);
}

/*
 * One 4D half: the dot products, the two quantisations, the delay-line
 * shift and the kLookup index.  `p`/`q` are the pair of parameters this
 * half was handed; `r0`/`r1` come back for the caller's arithmetic.
 */
static int
depth_half(struct v34_shell *s, int p, int q, int codebits, short *r0,
	   short *r1)
{
	const short *c = s->coeff;
	int acc0 = 0x1fff, acc1 = 0x1fff;
	int lim = (short)(s->wrap << 7);
	int maskhi = lim | (int)0xffff80ff;
	int mask2 = (short)(-(s->wrap * 2));
	short qa, qb;
	int ra, rb;
	int j;

	for (j = 0; j < 6; j++) {
		int h = s->hist[j];

		acc0 += h * c[j];
		acc1 += h * c[j + 6];
	}

	ra = depth_quant(acc0, lim, maskhi, mask2, &qa);
	rb = depth_quant(acc1, lim, maskhi, mask2, &qb);

	/* Shift by one complex pair, then take the new one. */
	s->hist[5] = s->hist[3];
	s->hist[4] = s->hist[2];
	s->hist[3] = s->hist[1];
	s->hist[2] = s->hist[0];
	s->hist[0] = (short)((p << 7) - (unsigned short)qa);
	s->hist[1] = (short)((q << 7) - (unsigned short)qb);

	*r0 = (short)ra;
	*r1 = (short)rb;

	return kLookup[(codebits & 0xc) | (ra & 2) | ((rb & 2) >> 1)];
}

/* Nudge `v` two counts toward the state's stored parameter. */
static int
depth_nudge(int v, int stored, int divisor)
{
	return stored > divisor * v ? v + 2 : v - 2;
}

/* Rotate a packed (lo, hi) pair by `k` quadrants and scale by 23. */
static int
depth_rotate(int packed, int k)
{
	int lo = (short)packed;
	int hi = packed >> 16;

	switch (k) {
	case 1:  return (short)(23 * lo - hi);
	case 2:  return (short)(-23 * hi - packed);
	case 3:  return (short)(-23 * lo + hi);
	default: return (short)(23 * hi + packed);
	}
}

void
decodeDepth(void *shellp, short *quad, short *idx)
{
	struct v34_shell *s = (struct v34_shell *)shellp;
	int div = s->divisor ? s->divisor : 1;
	int st = (unsigned short)(s->state_idx - 1) & 0x1f;
	int branch = s->state[st].seed;
	int code;
	int a, b, c, d;
	short r0, r1, r2, r3;
	int k1, k2;
	int packed, g;
	short prev;
	int n;

	/*
	 * Thirty-ONE steps back, high byte signed, state stepping down.  The
	 * counter is incremented before its `<= 30` test, so the body runs
	 * once more than the bound reads.
	 */
	for (n = 1; n <= 31; n++) {
		branch = (signed char)(s->trellis[(st << 4) + branch] >> 8);
		st = (st - 1) & 0x1f;
	}

	code = (unsigned char)s->trellis[(st << 4) + branch];

	/* The two parameter pairs, each rounded up to a multiple of 4 plus 1. */
	a = (((s->state[st].a / div) + (s->state[st].a > 0 ? 1 : 0)) & ~3) + 1;
	b = (((s->state[st].b / div) + (s->state[st].b > 0 ? 1 : 0)) & ~3) + 1;

	switch (code >> 2) {
	case 1:
		b = depth_nudge(b, s->state[st].b, div);
		break;
	case 2:
		a = depth_nudge(a, s->state[st].a, div);
		b = depth_nudge(b, s->state[st].b, div);
		break;
	case 3:
		a = depth_nudge(a, s->state[st].a, div);
		break;
	default:
		break;
	}

	k1 = depth_half(s, a, b, code, &r0, &r1);

	c = (((s->state[st].c / div) + (s->state[st].c > 0 ? 1 : 0)) & ~3) + 1;
	d = (((s->state[st].d / div) + (s->state[st].d > 0 ? 1 : 0)) & ~3) + 1;

	switch (code & 3) {
	case 1:
		d = depth_nudge(d, s->state[st].d, div);
		break;
	case 2:
		c = depth_nudge(c, s->state[st].c, div);
		d = depth_nudge(d, s->state[st].d, div);
		break;
	case 3:
		c = depth_nudge(c, s->state[st].c, div);
		break;
	default:
		break;
	}

	k2 = depth_half(s, c, d, code << 2, &r2, &r3);

	/* The quadrant pair, and the running quadrant folded back. */
	prev = s->prev_k;
	quad[0] = (short)(((k2 - k1) & 2) >> 1);
	quad[1] = (short)((k1 - (unsigned short)prev) & 3);
	s->prev_k = (short)k1;

	/* First grid lookup, from the first half's residues. */
	packed = (unsigned short)(a - r0)
	       | ((unsigned)(unsigned short)(b - r1) << 16);
	g = grid[(depth_rotate(packed, k1) + 0x408) >> 2];
	idx[0] = (short)((g >> (s->fa14 & 31)) & 0x1f);
	/* The mask is truncated to a short first, so fa14 == 16 gives -1. */
	quad[2] = (short)(g & (short)((1 << (s->fa14 & 31)) - 1));

	/* Second, from the second half's. */
	packed = (unsigned short)(c - r2)
	       | ((unsigned)(unsigned short)(d - r3) << 16);
	g = grid[(depth_rotate(packed, k2) + 0x408) >> 2];
	idx[1] = (short)((g >> (s->fa14 & 31)) & 0x1f);
	quad[3] = (short)(g & (short)((1 << (s->fa14 & 31)) - 1));
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned to what shellDemapper, putFrame and decodeDepth read.
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

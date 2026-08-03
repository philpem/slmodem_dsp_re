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

#include "dsplib/sysdep.h"	/* sysdep_memset: preinitdigital clears three blocks */
#include "dsplib/v34fsk.h"	/* struct v34_object: the scrambler callbacks take it */
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
 * demapFrame's four tables.
 *
 * kkNormal and kkInvert are alternative branch-code lists, selected by
 * `invert`; each pairs two candidate codes per position and the demapper
 * keeps whichever costs less.  kTable drives the 16-state add-compare-select,
 * four branches per state.  gInvertPat supplies the invert flag itself.
 */
const short gInvertPat[16] = {
	    0,     1,     1,     1,     0,     1,     1,     1,
	    1,     1,     1,     1,     1,     0,     1,     0,
};

const short kkInvert[16] = {
	    1,    11,     6,    12,     3,     9,     4,    14,
	    0,    10,     5,    15,     2,     8,     7,    13,
};

const short kkNormal[16] = {
	    0,    10,     5,    15,     2,     8,     7,    13,
	    1,    11,     6,    12,     3,     9,     4,    14,
};

const short kTable[64] = {
	    0,     1,     2,     3,     2,     3,     0,     1,
	    1,     0,     3,     2,     3,     2,     1,     0,
	    4,     5,     6,     7,     6,     7,     4,     5,
	    5,     4,     7,     6,     7,     6,     5,     4,
	    2,     3,     0,     1,     0,     1,     2,     3,
	    3,     2,     1,     0,     1,     0,     3,     2,
	    6,     7,     4,     5,     4,     5,     6,     7,
	    7,     6,     5,     4,     5,     4,     7,     6,
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
		small = 2 - ((unsigned short)s->fa04 < 9 ? 1 : 0);
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
 * demapFrame -- one sub-frame of the 8D demapper, and the frame it completes.
 *
 * This is the piece that makes the rest of the file make sense.  Eight
 * sub-frames make a frame, counted by `n`, and the two parities do entirely
 * different jobs:
 *
 *   n even   run decodeDepth, writing its four quadrant/point shorts into
 *            frame[2 + (n&7)/2 * 4] and its two shell sub-indices into
 *            sub[n & 7].  Four even sub-frames therefore fill exactly the
 *            sixteen shorts putFrame emits as four groups, and exactly the
 *            eight sub-indices shellDemapper consumes.  Returns 0.
 *
 *   n odd    build the branch costs and run a sixteen-state
 *            add-compare-select over the trellis.  Returns 1.
 *
 *   n & 7 == 7   additionally: shellDemapper into frame[0], then putFrame.
 *
 * So `frame[0]` is the shell index and `frame[2..17]` the four 2D symbols,
 * which is why putFrame sends one wide field and then four groups of four.
 *
 * THE ODD PATH, in order:
 *
 *   1. Store the caller's pair into this state's (c, d), and rebuild eight
 *      candidate values -- for each of the state's four parameters, the
 *      rounded value and that value two counts away, the direction chosen by
 *      which side of it the parameter falls.  This is decodeDepth's nudge
 *      run forwards.
 *   2. Square-distance every combination: four sums per parameter pair.
 *   3. For each of eight positions take the cheaper of two branch codes from
 *      kkNormal or kkInvert.
 *   4. Sixteen states x four branches, keeping the best cost and branch, and
 *      write each state's decision into the trellis as
 *      `(derived << 8) | code` -- the two byte lanes decodeDepth reads back
 *      with different signedness.
 *   5. Normalise all sixteen costs against the best, remember which state
 *      achieved it, and hand two values back through `b`.
 *
 * The two counters at +0xa3c and +0xa3e are sub-frame and frame indices with
 * their own limits; when the frame counter wraps it takes a new invert flag
 * from gInvertPat, which is what switches the branch-code table.
 */

/* Round a parameter to the grid, and give the neighbour two counts away. */
static void
demap_candidates(int p, int div, short *lo, short *hi)
{
	int q = p / div;

	q = ((p > 0 ? q + 1 : q) & ~3) + 1;
	q *= div;

	*lo = (short)q;
	*hi = (short)(p > (short)q ? q + 2 * div : q - 2 * div);
}

int
demapFrame(void *shellp, void *ap, void *bp, short n)
{
	struct v34_shell *s = (struct v34_shell *)shellp;
	int div = s->divisor ? s->divisor : 1;
	int sub = n & 7;
	short cand[8];
	short dist[8];
	short code[8];
	short cost[8];
	short best[16];
	int st = s->state_idx;
	int i, j;

	if (!(n & 1)) {
		/* Even: fill four frame shorts and two sub-indices. */
		decodeDepth(s, &s->frame[2 + (sub / 2) * 4], &s->sub[sub]);
		*(int *)&s->state[s->state_idx].a = *(int *)ap;
		return 0;
	}

	*(int *)&s->state[st].c = *(int *)ap;

	/* 1. Eight candidates, two per parameter. */
	for (i = 0; i < 4; i++)
		demap_candidates((&s->state[st].a)[i], div,
				 &cand[i * 2], &cand[i * 2 + 1]);

	/* 2. Squared distances, four combinations per parameter pair. */
	for (i = 0; i < 2; i++) {
		int p0 = (&s->state[st].a)[i * 2];
		int p1 = (&s->state[st].a)[i * 2 + 1];
		int sh = s->fa44 & 31;
		int d0 = ((p0 - cand[i * 4 + 0]) * (p0 - cand[i * 4 + 0])) >> sh;
		int d1 = ((p0 - cand[i * 4 + 1]) * (p0 - cand[i * 4 + 1])) >> sh;
		int e0 = ((p1 - cand[i * 4 + 2]) * (p1 - cand[i * 4 + 2])) >> sh;
		int e1 = ((p1 - cand[i * 4 + 3]) * (p1 - cand[i * 4 + 3])) >> sh;

		dist[i * 4 + 0] = (short)(d0 + e0);
		dist[i * 4 + 1] = (short)(d0 + e1);
		dist[i * 4 + 2] = (short)(d1 + e1);
		dist[i * 4 + 3] = (short)(d1 + e0);
	}

	/* 3. The cheaper of two branch codes at each of eight positions. */
	{
		const short *kk = s->invert ? kkInvert : kkNormal;

		for (i = 0; i < 8; i++) {
			int c0 = kk[i * 2];
			int c1 = kk[i * 2 + 1];
			int v0 = (unsigned short)dist[4 + (c0 & 3)]
			       + (unsigned short)dist[c0 >> 2];
			int v1 = (unsigned short)dist[c1 >> 2]
			       + (unsigned short)dist[4 + (c1 & 3)];

			code[i] = (short)c0;
			cost[i] = (short)v0;
			if ((short)v1 < (unsigned short)v0) {
				cost[i] = (short)v1;
				code[i] = (short)c1;
			}
		}
	}

	/* 4. Sixteen states, four branches each. */
	{
		int overall = 0x7fff;
		const short *kt = kTable;

		for (i = 15; i >= 0; i--) {
			int bestc = 0x7fff;
			int bestb = 3;
			const short *prior = &s->cost[3 - (i >> 2)];
			int hi;

			for (j = 3; j >= 0; j--) {
				int v = (unsigned short)cost[*kt]
				      + (unsigned short)prior[(3 - j) * 4];

				kt++;
				if ((unsigned short)v < (unsigned)bestc) {
					bestc = (unsigned short)v;
					bestb = j;
				}
			}

			best[15 - i] = (short)bestc;
			if ((unsigned)bestc < (unsigned)overall)
				overall = bestc;

			hi = (-(bestb * 4) - (i >> 2)) + 0xf;
			s->trellis[(st << 4) + (15 - i)] =
			    (unsigned short)((hi << 8)
					     + (unsigned short)code[kt[-1 - bestb]]);
		}

		/* 5. Normalise, and remember which state was best. */
		{
			int win = 0xf;

			for (i = 0; i < 16; i++) {
				s->cost[i] = (short)(best[i] - overall);
				if (s->cost[i] == 0)
					win = 15 - i;
			}
			s->state[st].seed = (short)(0xf - win);
		}
	}

	{
		int seed = s->state[st].seed;
		int t = (short)s->trellis[(st << 4) + seed];
		int k = (t >> 1) & 1;

		((short *)bp)[0] = cand[4 + k];
		k ^= t & 1;
		((short *)bp)[1] = cand[6 + k];
	}

	s->state_idx = (short)((s->state_idx + 1) & 0x1f);

	/* The two counters, and the invert flag the outer one refreshes. */
	if ((short)(s->fa3c + 1) < s->fa02) {
		s->fa3c = (short)(s->fa3c + 1);
		s->invert = 0;
	} else {
		s->fa3c = 0;
		if ((short)(s->fa3e + 1) < s->fa40) {
			s->fa3e = (short)(s->fa3e + 1);
			s->invert = gInvertPat[s->fa3e & 15];
		} else {
			s->fa3e = 0;
			s->invert = 0;
		}
	}

	if (sub != 7)
		return 1;

	/*
	 * The shell index is stored THIRTY-TWO bits wide, across frame[0]
	 * and frame[1] -- which is precisely the pair putFrame splits when
	 * its width exceeds 16, and the only path that reads frame[1].
	 */
	if (s->latched) {
		*(int *)&s->frame[0] = shellDemapper(s);
		putFrame(s);
		return 1;
	}

	if (n <= 0x40)
		return 1;

	*(int *)&s->frame[0] = shellDemapper(s);
	putFrame(s);

	if (n >= 0x40 + (unsigned short)s->fa00 * 8)
		s->latched = 1;

	return 1;
}


/*
 * lsbMask[n] == (1 << n) - 1, at .rodata+0x1f40.  Seventeen entries, so
 * fields up to sixteen bits wide.  Reproducible exactly, and emitted as data
 * anyway on the same principle as the others.
 */
const unsigned short lsbMask[17] = {
	     0,      1,      3,      7,     15,     31,     63,    127,
	   255,    511,   1023,   2047,   4095,   8191,  16383,  32767,
	 65535,
};

/*
 * ---------------------------------------------------------------------------
 * getFrame -- putFrame run backwards.
 *
 * Unpacks one frame from a bit stream into the TRANSMIT shell context, which
 * lives at `obj + V34_SHELL_TX` and is the same structure as the receive one
 * (finding 137).  The field layout is putFrame's exactly:
 *
 *     wide                  the shell index, into frame[0..1] as ONE 32-bit
 *                           store -- the pair putFrame splits above sixteen
 *                           bits, and the reason frame[1] exists
 *     (1, small, w, w) x 4  one group per 2D symbol, into frame[2..17]
 *
 * and the widths are chosen the same way, from `fa00` against `fa06 + fa08`.
 *
 * THE BIT WINDOW.  A 32-bit buffer at +0xe80 with a position at +0xe84;
 * fields come out as `(buf >> pos) & lsbMask[width]` and the position
 * advances by the width.  Whenever it passes 15 the callback is invoked to
 * refill, and the callback RETURNS the new position rather than taking a
 * pointer to it.  It is handed the object, not the context.
 */
void
getFrame(void *objp)
{
	struct v34_shell *s =
	    (struct v34_shell *)((char *)objp + V34_SHELL_TX);
	int w = s->fa14;
	int small = 2;
	int small_last = 2;
	int sum;
	int nb;
	int pos;
	int g;

	sum = (unsigned short)s->fa08 + (unsigned short)s->fa06;

	if ((unsigned short)s->fa00 > (unsigned short)sum) {
		s->fa08 = (short)sum;
		nb = s->fa10;
	} else {
		s->fa08 = (short)(sum - s->fa00);
		nb = s->fa0e;
	}

	pos = (unsigned short)s->bitpos;

	/* Refill before reading, and keep refilling while short. */
	while (pos > 15)
		pos = s->get_bits(objp, (short)pos);

	if (nb > 16) {
		/*
		 * Split, mirroring putFrame: sixteen bits, refill, then the
		 * remainder.  The first half is stored narrow and the second
		 * completes the 32-bit pair.
		 */
		s->frame[0] = (short)((unsigned)s->bitbuf >> (pos & 31));
		pos = s->get_bits(objp, 0);
		s->frame[1] = (short)(((unsigned)s->bitbuf >> (pos & 31))
				      & lsbMask[nb & 15]);
		pos += nb & 15;
	} else if (nb > 0) {
		*(int *)&s->frame[0] =
		    (int)(((unsigned)s->bitbuf >> (pos & 31))
			  & lsbMask[nb]);
		pos += nb;
	} else {
		/*
		 * No wide field: frame[0..1] is EXPLICITLY zeroed, as one
		 * 32-bit store, and the group widths change instead.
		 * putFrame simply emits nothing here, so the zeroing has no
		 * counterpart on that side and is easy to miss.
		 */
		*(int *)&s->frame[0] = 0;
		small = 2 - ((unsigned short)s->fa04 < 9 ? 1 : 0);
		small_last = (short)(nb + small);
	}

	for (g = 0; g < 4; g++) {
		short *p = &s->frame[2 + g * 4];
		int sw = (g == 3) ? small_last : small;
		unsigned v;

		if (pos > 15)
			pos = s->get_bits(objp, (short)pos);

		s->bitpos = (short)pos;
		v = (unsigned)s->bitbuf >> (pos & 31);

		/*
		 * `sw` is NOT bounds-checked, here or in the object.  On the
		 * no-wide-field path it is `nb + small`, so an `nb` below -2
		 * makes it negative and the object reads `.rodata` BEFORE
		 * lsbMask -- deterministic in that build, not reproducible
		 * here, and outside anything a real caller produces since a
		 * field width is never negative.  Same shape as finding 129;
		 * the fixture stays inside and says so.
		 */
		p[0] = (short)(v & 1);
		p[1] = (short)((v >> 1) & lsbMask[sw]);
		p[2] = (short)((v >> ((1 + sw) & 31)) & lsbMask[w & 31]);
		p[3] = (short)((v >> ((1 + sw + w) & 31)) & lsbMask[w & 31]);

		pos += 1 + sw + 2 * w;
		s->bitpos = (short)pos;
	}
}


/*
 * ---------------------------------------------------------------------------
 * The initialisers' tables.
 */

/*
 * V.34's three convolutional codes, one 128-byte table each and named for
 * their state counts by the original.  Thirty-two ints of two packed 16-bit
 * halves, every half in 0..15 -- a branch table, not coefficients.
 *
 * `conv` points at one of them: preinitV34 installs the 16-state code and
 * initV34 swaps in a wider one when its `depth` argument asks.
 */
const int Convolve16[32] = {
	0, 131074, 0, 131074, 786446, 917516, 786446, 917516,
	131074, 0, 131074, 0, 917516, 786446, 917516, 786446,
	0, 131074, 0, 131074, 786446, 917516, 786446, 917516,
	131074, 0, 131074, 0, 917516, 786446, 917516, 786446,
};
const int Convolve32[32] = {
	0, 524296, 262148, 786444, 1179674, 1703954, 1441822, 1966102,
	524296, 0, 786444, 262148, 1703954, 1179674, 1966102, 1441822,
	262148, 786444, 0, 524296, 1441822, 1966102, 1179674, 1703954,
	786444, 262148, 524296, 0, 1966102, 1441822, 1703954, 1179674,
};
const int Convolve64[32] = {
	0, 65537, 524296, 589833, 131075, 196610, 655371, 720906,
	327685, 262148, 851981, 786444, 458758, 393223, 983054, 917519,
	524296, 589833, 0, 65537, 655371, 720906, 131075, 196610,
	851981, 786444, 327685, 262148, 983054, 917519, 458758, 393223,
};

/*
 * xyz -- the shell counts, as a ragged array carrying its own index header.
 *
 * `xyz[0..19]` are offsets into xyz ITSELF, and the block between xyz[n] and
 * xyz[n+1] is the table for a ring of n points.  initG248 copies that block
 * straight into `t3`.
 *
 * WHAT THE BLOCKS ARE.  Block n is the cumulative count of the eight-fold
 * convolution of a rectangular window of length n: entry k is the number of
 * eight-tuples drawn from 0..n-1 whose sum is less than k.  That makes the
 * full length 8(n-1)+1 and the last entry n^8 - 1, and both hold exactly for
 * n up to 14.
 *
 * WHERE THEY STOP.  From n=15 on the block is SHORTER than 8(n-1)+1, and the
 * cut is not arbitrary: each block ends on the last entry that still fits a
 * signed 32-bit int.  69 entries for n=15, 58 for n=17, 56 for n=18 -- and
 * the entry after each is the first past INT_MAX.  Checked against a
 * recomputed convolution: every block is a prefix of the true sequence, and
 * every truncation point is exactly the overflow point.
 *
 * WHY n=16 IS EMPTY.  xyz[16] and xyz[17] are both 831, so a ring of 16 gets
 * no table at all.  That is not a hole: MMaxTable runs ... 14, 15, 17, 18 and
 * MMinTable stops at 15, so 16 is the one value initV34 cannot produce.  The
 * table and its only caller agree about which sizes exist.
 *
 * Emitted as reference bytes rather than as a generator, per docs/fastpass.md
 * -- the derivation above is recorded so task #47 does not have to find it
 * again, but a byte-exact copy is what the differential test proves.
 */
const int xyz[945] = {
	0, 20, 21, 30, 47, 72, 105, 146,
	195, 252, 317, 390, 471, 560, 657, 762,
	831, 831, 889, 945, 0, 0, 1, 9,
	37, 93, 163, 219, 247, 255, 0, 1,
	9, 45, 157, 423, 927, 1711, 2727, 3834,
	4850, 5634, 6138, 6404, 6516, 6552, 6560, 0,
	1, 9, 45, 165, 487, 1215, 2643, 5115,
	8938, 14266, 20994, 28722, 36814, 44542, 51270, 56598,
	60421, 62893, 64321, 65049, 65371, 65491, 65527, 65535,
	0, 1, 9, 45, 165, 495, 1279, 2931,
	6075, 11550, 20350, 33490, 51810, 75750, 105150, 139150,
	176230, 214395, 251475, 285475, 314875, 338815, 357135, 370275,
	379075, 384550, 387694, 389346, 390130, 390460, 390580, 390616,
	390624, 0, 1, 9, 45, 165, 495, 1287,
	2995, 6363, 12510, 22990, 39798, 65286, 101974, 152262,
	218070, 300454, 399267, 512955, 638543, 771831, 907785, 1041073,
	1166661, 1280349, 1379162, 1461546, 1527354, 1577642, 1614330, 1639818,
	1656626, 1667106, 1673253, 1676621, 1678329, 1679121, 1679451, 1679571,
	1679607, 1679615, 0, 1, 9, 45, 165, 495,
	1287, 3003, 6427, 12798, 23950, 42438, 71622, 115674,
	179466, 268318, 387606, 542251, 736131, 971479, 1248351, 1564269,
	1914109, 2290269, 2683117, 3081684, 3474532, 3850692, 4200532, 4516450,
	4793322, 5028670, 5222550, 5377195, 5496483, 5585335, 5649127, 5693179,
	5722363, 5740851, 5752003, 5758374, 5761798, 5763514, 5764306, 5764636,
	5764756, 5764792, 5764800, 0, 1, 9, 45, 165,
	495, 1287, 3003, 6435, 12862, 24238, 43398, 74262,
	122010, 193194, 295746, 438834, 632539, 887347, 1213471, 1620039,
	2114205, 2700261, 3378849, 4146393, 4994836, 5911732, 6880708, 7882276,
	8894940, 9896508, 10865484, 11782380, 12630823, 13398367, 14076955, 14663011,
	15157177, 15563745, 15889869, 16144677, 16338382, 16481470, 16584022, 16655206,
	16702954, 16733818, 16752978, 16764354, 16770781, 16774213, 16775929, 16776721,
	16777051, 16777171, 16777207, 16777215, 0, 1, 9, 45,
	165, 495, 1287, 3003, 6435, 12870, 24302, 43686,
	75222, 124650, 199530, 309474, 466290, 683991, 978615, 1367823,
	1870263, 2504709, 3289005, 4238865, 5366601, 6679872, 8180568, 9863964,
	11718244, 13724460, 15856956, 18084252, 20370348, 22676373, 24962469, 27189765,
	29322261, 31328477, 33182757, 34866153, 36366849, 37680120, 38807856, 39757716,
	40542012, 41176458, 41678898, 42068106, 42362730, 42580431, 42737247, 42847191,
	42922071, 42971499, 43003035, 43022419, 43033851, 43040286, 43043718, 43045434,
	43046226, 43046556, 43046676, 43046712, 43046720, 0, 1, 9,
	45, 165, 495, 1287, 3003, 6435, 12870, 24310,
	43750, 75510, 125610, 202170, 315810, 480018, 711447, 1030095,
	1459315, 2025595, 2758069, 3687741, 4846425, 6265425, 7974000, 9997680,
	12356520, 15063400, 18122500, 21528100, 25263820, 29302380, 33605925, 38126925,
	42809625, 47591985, 52408015, 57190375, 61873075, 66394075, 70697620, 74736180,
	78471900, 81877500, 84936600, 87643480, 90002320, 92026000, 93734575, 95153575,
	96312259, 97241931, 97974405, 98540685, 98969905, 99288553, 99519982, 99684190,
	99797830, 99874390, 99924490, 99956250, 99975690, 99987130, 99993565, 99996997,
	99998713, 99999505, 99999835, 99999955, 99999991, 99999999, 0, 1,
	9, 45, 165, 495, 1287, 3003, 6435, 12870,
	24310, 43758, 75574, 125898, 203130, 318450, 486354, 725175,
	1057551, 1510795, 2117115, 2913625, 3942081, 5248297, 6881217, 8891640,
	11330616, 14247552, 17688088, 21691824, 26290000, 31503252, 37339588, 43792749,
	50841085, 58447041, 66557313, 75103699, 84004635, 93167371, 102490707, 111868174,
	121191510, 130354246, 139255182, 147801568, 155911840, 163517796, 170566132, 177019293,
	182855629, 188068881, 192667057, 196670793, 200111329, 203028265, 205467241, 207477664,
	209110584, 210416800, 211445256, 212241766, 212848086, 213301330, 213633706, 213872527,
	214040431, 214155751, 214232983, 214283307, 214315123, 214334571, 214346011, 214352446,
	214355878, 214357594, 214358386, 214358716, 214358836, 214358872, 214358880, 0,
	1, 9, 45, 165, 495, 1287, 3003, 6435,
	12870, 24310, 43758, 75582, 125962, 203418, 319410, 488994,
	731511, 1071279, 1538251, 2168595, 3005145, 4097665, 5502861, 7284069,
	9510568, 12256488, 15599304, 19617928, 24390432, 29991456, 36489376, 43943328,
	52400205, 61891765, 72432009, 84015009, 96613331, 110177163, 124634223, 139890487,
	155831742, 172325934, 189226246, 206374806, 223606890, 240755450, 257655762, 274149954,
	290091209, 305347473, 319804533, 333368365, 345966687, 357549687, 368089931, 377581491,
	386038368, 393492320, 399990240, 405591264, 410363768, 414382392, 417725208, 420471128,
	422697627, 424478835, 425884031, 426976551, 427813101, 428443445, 428910417, 429250185,
	429492702, 429662286, 429778278, 429855734, 429906114, 429937938, 429957386, 429968826,
	429975261, 429978693, 429980409, 429981201, 429981531, 429981651, 429981687, 429981695,
	0, 1, 9, 45, 165, 495, 1287, 3003,
	6435, 12870, 24310, 43758, 75582, 125970, 203482, 319698,
	489954, 734151, 1077615, 1551979, 2196051, 3056625, 4189185, 5658445,
	7538661, 9913644, 12876396, 16528312, 20977912, 26339088, 32728872, 40264752,
	49061584, 59228169, 70863585, 84053385, 98865793, 115348051, 133523091, 153386727,
	174905527, 198015490, 222621618, 248598438, 275791494, 304019794, 333079162, 362746410,
	392784210, 422946511, 452984311, 482651559, 511710927, 539939227, 567132283, 593109103,
	617715231, 640825194, 662343994, 682207630, 700382670, 716864928, 731677336, 744867136,
	756502552, 766669137, 775465969, 783001849, 789391633, 794752809, 799202409, 802854325,
	805817077, 808192060, 810072276, 811541536, 812674096, 813534670, 814178742, 814653106,
	814996570, 815240767, 815411023, 815527239, 815604751, 815655139, 815686963, 815706411,
	815717851, 815724286, 815727718, 815729434, 815730226, 815730556, 815730676, 815730712,
	815730720, 0, 1, 9, 45, 165, 495, 1287,
	3003, 6435, 12870, 24310, 43758, 75582, 125970, 203490,
	319762, 490242, 735111, 1080255, 1558315, 2209779, 3084081, 4240665,
	5749965, 7694245, 10168236, 13279500, 17148444, 21907900, 27702208, 34685760,
	43020984, 52875768, 64420345, 77823681, 93249429, 110851533, 130769587, 153124075,
	178011639, 205500543, 235626522, 268389226, 303749434, 341627178, 381900882, 424407586,
	468944290, 515270418, 563111367, 612163071, 662097475, 712568779, 763220277, 813691581,
	863625985, 912677689, 960518638, 1006844766, 1051381470, 1093888174, 1134161878, 1172039622,
	1207399830, 1240162534, 1270288513, 1297777417, 1322664981, 1345019469, 1364937523, 1382539627,
	1397965375, 1411368711, 1422913288, 1432768072, 1441103296, 1448086848, 1453881156, 1458640612,
	1462509556, 1465620820, 1468094811, 1470039091, 1471548391, 1472704975, 1473579277, 1474230741,
	1474708801, 1475053945, 1475298814, 1475469294, 1475585566, 1475663086, 1475713474, 1475745298,
	1475764746, 1475776186, 1475782621, 1475786053, 1475787769, 1475788561, 1475788891, 1475789011,
	1475789047, 1475789055, 0, 1, 9, 45, 165, 495,
	1287, 3003, 6435, 12870, 24310, 43758, 75582, 125970,
	203490, 319770, 490306, 735399, 1081215, 1560955, 2216115, 3097809,
	4268121, 5801445, 7785765, 10323820, 13534092, 17551548, 22528060, 28632420,
	36049860, 44981008, 55640232, 68253345, 83054665, 100283445, 120179709, 142979551,
	168909975, 198183375, 230991775, 267500970, 307844730, 352119250, 400378050, 452627550,
	508823510, 568868490, 632610450, 699842575, 770304375, 843684075, 919622275, 997716825,
	1077528825, 1158589625, 1240408665, 1322481960, 1404301000, 1485361800, 1565173800, 1643268350,
	1719206550, 1792586250, 1863048050, 1930280175, 1994022135, 2054067115, 2110263075, 0,
	1, 9, 45, 165, 495, 1287, 3003, 6435,
	12870, 24310, 43758, 75582, 125970, 203490, 319770, 490314,
	735471, 1081567, 1562203, 2219715, 3106785, 4288185, 5842629, 7864701,
	10466820, 13781196, 17961724, 23185756, 29655684, 37600260, 47275572, 58965588,
	72982173, 89664477, 109377613, 132510565, 159473287, 190692975, 226609515, 267670131,
	314323278, 367011846, 426165762, 492194098, 565476814, 646356286, 735128790, 832036134,
	937257651, 1050902787, 1173004539, 1303513963, 1442295937, 1589126329, 1743690685, 1905584517,
	2074315236, 0, 1, 9, 45, 165, 495, 1287,
	3003, 6435, 12870, 24310, 43758, 75582, 125970, 203490,
	319770, 490314, 735471, 1081575, 1562267, 2220003, 3107745, 4290825,
	5848965, 7878429, 10494276, 13832676, 18053244, 23341340, 29910276, 38003364,
	47895732, 59895828, 74346525, 91625733, 112146417, 136355913, 164734455, 197792847,
	236069235, 280124955, 330539454, 387904302, 452816334, 525869982, 607648878, 698716830,
	799608294, 910818486, 1032793299, 1165919211, 1310513391, 1466814231, 1634972553, 1815043761,
	2006981173,
};

/*
 * The ring size, indexed by `fa0e`.  initV34 takes MMaxTable when its
 * `use_max` argument is non-zero and MMinTable when it is not, and the index
 * it uses is bounded to 0..31 by the loop that produces it -- so `count`
 * lands in 1..18 and nothing else, which is what keeps xyz's header read in
 * range.  Signed chars in the object, and read with a sign-extending load.
 */
const signed char MMaxTable[32] = {
	1, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5,
	5, 5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 14, 15, 17, 18,
};
const signed char MMinTable[32] = {
	1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,
	4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 10, 11, 12, 13, 14, 15,
};

/*
 * ---------------------------------------------------------------------------
 * The initialisers, and the four bit callbacks they install.
 *
 * These take a pointer to the shell's own fields rather than to the object --
 * see V34_SHELL_FIELDS in v34shell.h -- so each starts by winding it back to
 * the struct the rest of this file works in.  The two spellings meet here and
 * nowhere else.
 */
static struct v34_shell *
shell_of(void *fields)
{
	return (struct v34_shell *)((char *)fields - V34_SHELL_FIELDS);
}

/*
 * Point a context's bit callback somewhere.
 *
 * NOTHING IN THE OBJECT CALLS THIS.  All four sites that set the callback --
 * two in preinitV34's inlined copies and two in preinitdigital -- store it
 * directly, so this survives only as the out-of-line copy the compiler had to
 * emit for an extern function.  Same shape as finding 89; reconstructed
 * because it is there, not because anything needs it.
 */
void
setScramble(void *fields, void *fn)
{
	shell_of(fields)->put_bits = (v34_putbits_fn)fn;
}

/*
 * Scale eight complex points -- sixteen shorts -- by `scale`/128, in place.
 *
 * The product is formed at 32 bits and shifted arithmetically before it is
 * truncated, so a scale above 128 saturates by wrapping rather than by
 * clipping.  Also uncalled; see setScramble.
 */
void
scaleVector(short *v, short scale)
{
	short i;

	for (i = 0; i <= 7; i++) {
		v[0] = (short)((v[0] * scale) >> 7);
		v[1] = (short)((v[1] * scale) >> 7);
		v += 2;
	}
}

/*
 * Clear one shell context to its power-on state.
 *
 * The three count tables go to 0, 0 and -1 respectively -- t3's fill is the
 * only one that is not zero, and initG248 overwrites however much of it the
 * ring size calls for, so the -1s are what is left showing past the end.
 *
 * `count` is NOT set here, so the tables mean nothing until initV34 or
 * initG248 has run.  The 16-state convolutional code and the caller's
 * scrambler are the defaults; preinitdigital corrects the second of those for
 * the receive context and for the answering role.
 */
void
preinitV34(void *fields)
{
	struct v34_shell *s = shell_of(fields);
	short i;

	for (i = 0; i <= 0x7f; i++) {
		s->t1[i] = 0;
		s->t2[i] = 0;
		s->t3[i] = -1;
	}
	for (i = 0; i <= 5; i++) {
		s->fa2c[i] = 0;
		s->hist[i] = 0;
	}

	s->fa16 = 0x18;
	s->conv = Convolve16;
	s->fa3c = 0;
	s->prev_k = 0;
	s->latched = 0;
	s->fa08 = 0;
	s->get_bits = scrambleGPC;
}

/*
 * Rebuild the three count tables from `count` alone.
 *
 * t1 is the tent 1, 2, ... n, ... 2, 1 -- the number of ways one sub-index
 * pair can reach each total -- written from both ends at once, which is why
 * the loop stores twice per step.  t2 is t1 convolved with itself, mirrored
 * the same way.  t3 is the eight-fold convolution, which is not computed at
 * all: it is copied out of `xyz`, where it was precomputed.
 *
 * UNCALLED, like setScramble -- initV34 carries the identical three loops
 * inline.  Reconstructed as its own function anyway, because the inline copy
 * inside initV34 is then the same code and gets tested twice over.
 *
 * A `count` of zero would send the second loop round 2^32 times; nothing
 * reaches that, since the only thing that sets `count` is initV34 and both
 * of its tables bottom out at 1.
 */
void
initG248(void *fields)
{
	struct v34_shell *s = shell_of(fields);
	unsigned n = (unsigned short)s->count;
	unsigned top = 2 * (n - 1);
	unsigned i, j, k, cursor, len;

	for (i = 0; i < n; i++) {
		s->t1[top - i] = (short)(i + 1);
		s->t1[i] = (short)(i + 1);
	}

	for (j = 0; j <= top; j++) {
		const short *a = s->t1;
		const short *b = s->t1 + j;
		short cnt = (short)(j + 1);
		int acc = 0;

		while (cnt > 0) {
			acc += (unsigned short)*b * (unsigned short)*a;
			b--;
			a++;
			cnt = (short)(cnt - 1);
		}
		s->t2[2 * top - j] = (short)acc;
		s->t2[j] = (short)acc;
	}

	cursor = (unsigned)xyz[n];
	len = (unsigned)(xyz[n + 1] - xyz[n]);
	for (k = 0; k < len; k++)
		s->t3[k] = xyz[cursor++];
}

/*
 * Configure one shell context for a symbol rate and a trellis, and build its
 * count tables.  Always returns zero.
 *
 * `baud` is the V.34 symbol rate: 2400, 2743, 2800, 3000, 3200 or 3429.  Two
 * things come off it, and they partition the six rates differently:
 *
 *   the GROUP  is 8 for 2743 and 3429 and 7 for the other four -- i.e. 8 for
 *              the two rates whose baud is not a whole number of hundredths,
 *              which are exactly the two V.34 defines as 2400*7/8*... ratios
 *   the SPAN J is 12 below 2800, 14 at 2800, 16 at 3200 and 15 above 2800
 *              otherwise -- five distinct rates mapping onto four values
 *
 * `bitrate` is then divided down to a per-symbol bit count against both, and
 * `use_max` picks which of the two ring-size tables the result indexes.
 * `depth` selects the convolutional code, `coeff` and `divisor` are stored as
 * handed over, and the last of those sets a field width by binary search.
 */
int
initV34(void *fields, short baud, short bitrate, short use_max,
	short depth, const short *coeff, short divisor)
{
	struct v34_shell *s = shell_of(fields);
	unsigned rate = (unsigned short)baud;
	unsigned group = (unsigned short)(7 + (rate == 0xab7 || rate == 0xd65));
	unsigned span, q, m, u, idx, w;
	unsigned i, j, k, n, top, cursor, len;

	if (rate > 0xaf0)
		span = (unsigned short)(16 - (rate != 0xc80));
	else if (rate == 0xaf0)
		span = 14;
	else
		span = 12;

	s->fa00 = (short)span;
	s->fa02 = (short)(2 * span);
	s->coeff = coeff;

	/*
	 * The wider codes come with a wider field: 32 and Convolve32 for a
	 * depth of one, 64 and Convolve64 for anything above.  A depth of
	 * zero leaves preinitV34's 24 and the 16-state code alone.
	 */
	if (depth != 0) {
		s->fa16 = (short)(depth << 5);
		s->conv = (depth == 1) ? Convolve32 : Convolve64;
	}

	s->fa40 = (short)(2 * group);
	s->fa3e = (short)(2 * group - 2);
	s->invert = (short)(unsigned short)gInvertPat[(short)(2 * group - 2)];

	/*
	 * Three divisions, each rounding differently: /25 truncating, then
	 * /group truncating, then /span rounding UP.  `fa06` is what the
	 * rounding up left over, so the last pair is a quotient and its
	 * remainder spread across `fa04` groups.
	 *
	 * The 25 is worth pinning down, because the object does it as a
	 * reciprocal multiply and the constant is the one everybody reads as
	 * a divide by 100: `mul $0x51eb851f` then `shr $3`, i.e. >> 35, not
	 * the >> 37 that would make it 100.  Written as /100 it agrees with
	 * the object on nothing above 24.
	 */
	q = (unsigned)(unsigned short)bitrate / 25u;
	m = (unsigned short)((int)(q * 7) / (int)group);
	u = (unsigned)(int)((int)(m + span - 1) / (int)span);

	s->fa04 = (short)u;
	s->wrap = (short)(2 - ((unsigned short)u <= 0x37));
	s->fa0a = (short)(15 - group);
	s->fa06 = (short)(m - ((unsigned short)u - 1) * span);
	s->fa14 = 0;

	/*
	 * Bring `u` down into 12..43 in steps of eight, and record how many
	 * steps it took.  The two early exits are the same test unrolled: no
	 * steps needed at all, then one subtraction's worth.
	 */
	if ((unsigned short)u <= 12) {
		idx = 0;
	} else if ((unsigned short)(u - 12) <= 0x1f) {
		idx = u - 12;
	} else {
		j = 0;
		do {
			j++;
			idx = u - 8 * (unsigned short)j - 12;
		} while ((unsigned short)idx > 0x1f);
		s->fa14 = (short)j;
	}

	s->fa0e = (short)idx;
	s->fa10 = (short)(idx - 1);

	/*
	 * The ring size, and then initG248's three loops inline -- which is
	 * how the object has it, and the reason initG248 has no caller.
	 */
	s->count = use_max ? MMaxTable[(unsigned short)idx]
			   : MMinTable[(unsigned short)idx];

	n = (unsigned short)s->count;
	top = 2 * (n - 1);

	for (i = 0; i < n; i++) {
		s->t1[top - i] = (short)(i + 1);
		s->t1[i] = (short)(i + 1);
	}

	for (j = 0; j <= top; j++) {
		const short *a = s->t1;
		const short *b = s->t1 + j;
		short cnt = (short)(j + 1);
		int acc = 0;

		while (cnt > 0) {
			acc += (unsigned short)*b * (unsigned short)*a;
			b--;
			a++;
			cnt = (short)(cnt - 1);
		}
		s->t2[2 * top - j] = (short)acc;
		s->t2[j] = (short)acc;
	}

	cursor = (unsigned)xyz[n];
	len = (unsigned)(xyz[n + 1] - xyz[n]);
	for (k = 0; k < len; k++)
		s->t3[k] = xyz[cursor++];

	/*
	 * `fa44` is the field width `divisor` needs, found by walking powers
	 * of two up from 128 -- and then one more, so it is a width and not
	 * an exponent.  A divisor of 128 or less skips the search entirely
	 * and takes the 7 the search would have returned.
	 */
	s->divisor = divisor;
	s->fa44 = 7;
	if ((int)(unsigned)(unsigned short)divisor > 0x80) {
		w = 7;
		do {
			w++;
		} while ((int)(1u << (w & 31))
			 < (int)(unsigned)(unsigned short)divisor);
		s->fa44 = (short)w;
	}
	s->fa44 = (short)((unsigned short)s->fa44 + 1);

	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * The four bit callbacks.
 *
 * V.34 gives the two ends opposite generators -- 1 + x^-5 + x^-23 for the
 * calling station (GPC) and 1 + x^-18 + x^-23 for the answering one (GPA) --
 * and each end scrambles with its own and descrambles with the other's.
 * preinitdigital installs the pair, so the four appear only ever two at a
 * time.
 *
 * All four work SIXTEEN BITS AT A TIME rather than one, which is why none of
 * them looks like a shift register: the taps become shifts of a whole word
 * and the history lives in three (transmit: four) parallel words.  Which
 * shift stands for which tap is left to task #47; what is reproduced here is
 * the word arithmetic exactly as the object has it, ORs and all -- note that
 * `(w << 2) | half` genuinely overlaps in bits 2..15 and is not a
 * concatenation.
 *
 * The transmit pair take the OBJECT and a bit position, and return the
 * position moved back sixteen; the receive pair take the object, a value and
 * a width, and return nothing.  Both reach the shell context they belong to
 * from the object: the receive one is at +0, the transmit one at +0x1be0.
 */

/* The next sixteen bits to send: from `tx_data` if enabled, else idle ones. */
static unsigned
tx_bits(struct v34_object *o)
{
	if (o->data_enable != 0 && (unsigned)o->tx_rd < (unsigned)o->tx_n)
		return (unsigned short)o->tx_data[o->tx_rd++];
	return 0xffffu;
}

/* Hand sixteen recovered bits to `rx_data`, while there is room for them. */
static void
rx_bits(struct v34_object *o, unsigned bits)
{
	if (o->data_enable != 0 && (unsigned)o->rx_n <= 0x3f) {
		o->rx_data[o->rx_n] = (int)bits;
		o->rx_n++;
	}
}

int
scrambleGPC(void *obj, int pos)
{
	struct v34_shell *tx = (struct v34_shell *)((char *)obj + V34_SHELL_TX);
	unsigned in = tx_bits((struct v34_object *)obj);
	unsigned b = (unsigned)tx->bitbuf;
	unsigned n0, n1, n2;

	n0 = (in << 16) | ((unsigned)tx->scr[0] >> 16);
	n1 = (b << 2)   | ((unsigned)tx->scr[1] >> 16);
	n2 = (b << 7)   | ((unsigned)tx->scr[2] >> 16);

	tx->scr[0] = (int)n0;
	tx->scr[1] = (int)n1;
	tx->scr[2] = (int)n2;
	tx->bitbuf = (int)(n0 ^ n1 ^ n2);

	return (short)(pos - 16);
}

int
scrambleGPA(void *obj, int pos)
{
	struct v34_shell *tx = (struct v34_shell *)((char *)obj + V34_SHELL_TX);
	unsigned in = tx_bits((struct v34_object *)obj);
	unsigned b = (unsigned)tx->bitbuf;
	unsigned h1 = (unsigned)tx->scr[1] >> 16;
	unsigned n0, n2, t, a;

	n0 = (in << 16) | ((unsigned)tx->scr[0] >> 16);
	n2 = (b << 7)   | ((unsigned)tx->scr[2] >> 16);
	tx->scr[0] = (int)n0;
	tx->scr[2] = (int)n2;

	/*
	 * Four rounds of the same step rather than one, because this
	 * generator's taps are close enough together that a sixteen-bit
	 * block feeds back into itself: each round shifts five more bits in.
	 */
	t = n0 ^ n2;
	a = ((b >> 11) | h1) ^ t;
	a = ((a << 5) | h1) ^ t;
	a = ((a << 5) | h1) ^ t;
	a = ((a << 5) | h1) ^ t;

	tx->bitbuf = (int)a;
	tx->scr[1] = (int)(h1 | (a << 5));

	return (short)(pos - 16);
}

void
descrambleGPC(void *obj, int value, int nbits)
{
	struct v34_shell *rx = (struct v34_shell *)obj;
	unsigned v = (unsigned short)value;
	unsigned w = (unsigned short)nbits;
	unsigned pos = (unsigned short)rx->rx_bitpos;
	unsigned w0, w1, w2;

	w0 = (v << ((short)pos & 31)) | (unsigned)rx->scr[0];
	rx->scr[0] = (int)w0;

	pos += w;
	rx->rx_bitpos = (short)pos;
	if ((short)pos <= 31)
		return;

	w1 = (unsigned)rx->scr[1];
	w2 = (unsigned)rx->scr[2];
	rx_bits((struct v34_object *)obj, (unsigned short)(w0 ^ w1 ^ w2));

	rx->scr[1] = (int)((w0 << 2) | (w1 >> 16));
	rx->scr[2] = (int)((w0 << 7) | (w2 >> 16));

	pos = (unsigned short)rx->rx_bitpos - 16;
	rx->rx_bitpos = (short)pos;
	rx->scr[0] = (int)((v << (((short)pos - (int)w) & 31)) | (w0 >> 16));
}

void
descrambleGPA(void *obj, int value, int nbits)
{
	struct v34_shell *rx = (struct v34_shell *)obj;
	unsigned v = (unsigned short)value;
	unsigned w = (unsigned short)nbits;
	unsigned pos = (unsigned short)rx->rx_bitpos;
	unsigned w0, t, w2;

	w0 = (v << ((short)pos & 31)) | (unsigned)rx->scr[0];
	rx->scr[0] = (int)w0;

	pos += w;
	rx->rx_bitpos = (short)pos;
	if ((short)pos <= 31)
		return;

	t = (w0 << 5) | (unsigned)rx->scr[1];
	w2 = (unsigned)rx->scr[2];
	rx_bits((struct v34_object *)obj, (unsigned short)(w0 ^ t ^ w2));

	rx->scr[1] = (int)(t >> 16);
	rx->scr[2] = (int)((w0 << 7) | (w2 >> 16));

	pos = (unsigned short)rx->rx_bitpos - 16;
	rx->rx_bitpos = (short)pos;
	rx->scr[0] = (int)((v << (((short)pos - (int)w) & 31)) | (w0 >> 16));
}

/*
 * Reset both shell contexts and the trellis decoder between them, and install
 * the pair of scrambler callbacks the station's role calls for.
 *
 * The two contexts get preinitV34 apiece -- inlined twice in the object, and
 * called twice here, which is the same thing -- and then the receive one gets
 * the trellis machinery cleared, because only it decodes.
 *
 * THE TWO SCRAMBLER STATES ARE CLEARED DIFFERENTLY, and that is not an
 * oversight: the transmit context has a fourth register word where the
 * receive one keeps its bit position, so there are four ints and a position
 * on one side and three ints and a position on the other.  The note on the
 * union in v34shell.h has the whole of it.  The transmit position starts at
 * 32, which is past the fifteen getFrame checks against, so the first getFrame
 * refills before it reads.
 */
void
preinitdigital(void *obj)
{
	struct v34_object *o = (struct v34_object *)obj;
	struct v34_shell *rx = (struct v34_shell *)obj;
	struct v34_shell *tx = (struct v34_shell *)((char *)obj + V34_SHELL_TX);

	preinitV34((char *)obj + V34_SHELL_TX + V34_SHELL_FIELDS);
	preinitV34((char *)obj + V34_SHELL_FIELDS);

	sysdep_memset(rx->cost, 0, sizeof rx->cost);
	sysdep_memset(rx->trellis, 0, sizeof rx->trellis);
	sysdep_memset(rx->state, 0, sizeof rx->state);

	tx->scr[0] = 0;
	tx->scr[1] = 0;
	tx->scr[2] = 0;
	tx->bitbuf = 0;
	tx->bitpos = 32;

	rx->scr[0] = 0;
	rx->scr[1] = 0;
	rx->scr[2] = 0;
	rx->rx_bitpos = 0;
	rx->state_idx = 0;

	o->data_enable = 0;
	o->faa74 = 0;

	/*
	 * Scramble with this station's generator and descramble with the
	 * other's.  0x65 in f359c is the same flag that picks the second
	 * timing-parameter table, so the role is carried in one place.
	 */
	if (o->f359c == 0x65) {
		tx->get_bits = scrambleGPC;
		rx->put_bits = descrambleGPA;
	} else {
		tx->get_bits = scrambleGPA;
		rx->put_bits = descrambleGPC;
	}
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned to what every function above reads.
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
V34SH_ASSERT(bitbuf, 0xe80);
V34SH_ASSERT(bitpos, 0xe84);
V34SH_ASSERT(frame, 0xe50);
V34SH_ASSERT(fa00, 0xa00);
V34SH_ASSERT(fa14, 0xa14);

/* The fields the initialisers and the scrambler callbacks added. */
V34SH_ASSERT(fa0a, 0xa0a);
V34SH_ASSERT(fa16, 0xa16);
V34SH_ASSERT(hist, 0xa18);
V34SH_ASSERT(coeff, 0xa24);
V34SH_ASSERT(conv, 0xa28);
V34SH_ASSERT(fa2c, 0xa2c);
V34SH_ASSERT(prev_k, 0xa38);
V34SH_ASSERT(cost, 0xeac);
V34SH_ASSERT(trellis, 0xecc);
V34SH_ASSERT(state, 0x12cc);
V34SH_ASSERT(state_idx, 0x144c);
V34SH_ASSERT(scr, 0xe74);
V34SH_ASSERT(rx_bitpos, 0xe80);

/*
 * And the object's, which preinitdigital and the two data buffers pin.
 * `struct v34_object` is v34fsk.h's, so the macro above will not do.
 */
#define V34OB_ASSERT(field, off) \
	typedef char v34ob_off_##field[ \
		((int)__builtin_offsetof(struct v34_object, field) == (off)) \
		? 1 : -1]

V34OB_ASSERT(rx_data, 0x014);
V34OB_ASSERT(rx_n, 0x114);
V34OB_ASSERT(tx_data, 0x118);
V34OB_ASSERT(tx_n, 0x218);
V34OB_ASSERT(tx_rd, 0x21c);
V34OB_ASSERT(data_enable, 0x2214);
V34OB_ASSERT(faa74, 0xaa74);

/* The three memsets' lengths are the object's own, so pin those too. */
typedef char v34sh_len_cost[(sizeof(((struct v34_shell *)0)->cost)
			     == 0x20) ? 1 : -1];
typedef char v34sh_len_trellis[(sizeof(((struct v34_shell *)0)->trellis)
				== 0x400) ? 1 : -1];
typedef char v34sh_len_state[(sizeof(((struct v34_shell *)0)->state)
			      == 0x180) ? 1 : -1];

#endif

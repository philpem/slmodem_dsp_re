/*
 * t_v34shell.c -- differential test of the V.34 shell demapper.
 */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "dsplib/v34shell.h"
#include "dsplib/v34fsk.h"	/* struct v34_object: preinitdigital takes it */

extern int ref_shellDemapper(void *s);
extern void ref_putFrame(void *s);
extern const short ref_kLookup[16];
extern const short ref_grid[529];
extern const unsigned short ref_lsbMask[17];
extern void ref_getFrame(void *obj);

/*
 * getFrame's bit source.  Each side gets its own, for the same reason
 * putFrame's sinks are separate: one shared callback would interleave.
 * It refills the buffer from a deterministic stream and returns the new
 * bit position, which is the contract the object expects.
 */
/* --- the round-trip harness: a sink and a source over one bit stream --- */
static unsigned long long rt_bits;
static int rt_n, rt_rd;
static short rt_want[18];

static void
rt_sink(void *ctx, int value, int nbits)
{
	(void)ctx;
	if (nbits <= 0)
		return;
	rt_bits |= (unsigned long long)(value & ((1 << nbits) - 1)) << rt_n;
	rt_n += nbits;
}

/*
 * The contract getFrame expects: it calls this when the position has passed
 * 15, meaning the low sixteen bits of the window are spent.  So advance the
 * window by sixteen and hand back the position relative to the new one.
 * Returning a fixed 0 instead -- the obvious first guess -- silently drops
 * whatever was left above bit 15.
 */
static int
rt_source(void *o, int pos)
{
	struct v34_shell *t = (struct v34_shell *)((char *)o + V34_SHELL_TX);

	rt_rd += 16;
	t->bitbuf = (int)(unsigned)(rt_bits >> rt_rd);
	return pos - 16;
}

static unsigned src_words_a[64], src_words_b[64];
static int src_i_a, src_i_b;

static int
bitsrc_a(void *obj, int pos)
{
	struct v34_shell *s = (struct v34_shell *)((char *)obj + V34_SHELL_TX);

	s->bitbuf = (int)src_words_a[src_i_a & 63];
	src_i_a++;
	(void)pos;
	return 0;
}

static int
bitsrc_b(void *obj, int pos)
{
	struct v34_shell *s = (struct v34_shell *)((char *)obj + V34_SHELL_TX);

	s->bitbuf = (int)src_words_b[src_i_b & 63];
	src_i_b++;
	(void)pos;
	return 0;
}
extern void ref_decodeDepth(void *s, short *quad, short *idx);
extern int ref_demapFrame(void *s, void *a, void *b, short n);


/*
 * putFrame writes through a pointer the object carries, so each side gets
 * its OWN sink and its own log.  Installing one shared callback would
 * interleave the two call sequences into a single buffer and compare it
 * against itself -- which passes whatever either side does.
 */
#define LOGMAX 64
static struct { int val, nbits; } log_a[LOGMAX], log_b[LOGMAX];
static int nlog_a, nlog_b;

static void
sink_a(void *s, int value, int nbits)
{
	(void)s;
	if (nlog_a < LOGMAX) {
		log_a[nlog_a].val = value;
		log_a[nlog_a].nbits = nbits;
	}
	nlog_a++;
}

static void
sink_b(void *s, int value, int nbits)
{
	(void)s;
	if (nlog_b < LOGMAX) {
		log_b[nlog_b].val = value;
		log_b[nlog_b].nbits = nbits;
	}
	nlog_b++;
}


/*
 * ---------------------------------------------------------------------------
 * The initialisers, the tables they read, and the four bit callbacks.
 */

extern void ref_setScramble(void *fields, void *fn);
extern void ref_scaleVector(short *v, short scale);
extern void ref_preinitV34(void *fields);
extern void ref_initG248(void *fields);
extern int ref_initV34(void *fields, short baud, short bitrate, short use_max,
		       short depth, const short *coeff, short divisor);
extern void ref_preinitdigital(void *obj);
extern int ref_scrambleGPC(void *obj, int pos);
extern int ref_scrambleGPA(void *obj, int pos);
extern void ref_descrambleGPC(void *obj, int value, int nbits);
extern void ref_descrambleGPA(void *obj, int value, int nbits);

extern const int ref_xyz[945];
extern const int ref_Convolve16[32];
extern const int ref_Convolve32[32];
extern const int ref_Convolve64[32];
extern const signed char ref_MMaxTable[32];
extern const signed char ref_MMinTable[32];

/*
 * The two fields that hold POINTERS, and so can never match across the two
 * sides: `conv` at +0xa28 and the bit callback at +0xe48.  One side's
 * Convolve16 is at a different address from the other's, so a raw byte
 * compare over them proves nothing and fails always.  They are skipped here
 * and asserted by identity -- ours must be OUR table, theirs THEIR table --
 * which is the stronger check anyway, since it says which table was chosen.
 */
static int
shell_bytes_eq(const char *tag, const void *a, const void *b, long ctx)
{
	const unsigned char *pa = a, *pb = b;
	unsigned i;
	int bad = 0;

	for (i = 0; i < sizeof(struct v34_shell); i++) {
		if ((i >= 0xa28 && i < 0xa2c) || (i >= 0xe48 && i < 0xe4c))
			continue;
		if (pa[i] != pb[i] && bad++ < 8) {
			printf("  (%s, case %ld)\n", tag, ctx);
			diff_eq_int("shell byte at +0x%lx", pa[i], pb[i],
				    (long)i);
		}
	}
	return bad;
}

/*
 * The reachable range of `count`.  initV34 indexes MMaxTable or MMinTable
 * with a value the loop above it forces into 0..31, and neither table holds
 * anything outside 1..18 -- so every t3 block initG248 can be asked for is
 * one xyz actually has.  Asserted rather than assumed: finding 129 is about
 * these three tables having no bounds check of their own, and this is the
 * caller-side invariant it left open.
 */
#define V34_COUNT_MIN	1
#define V34_COUNT_MAX	18

static int
check_count_bound(void)
{
	int i, bad = 0;

	for (i = 0; i < 32; i++) {
		if (MMaxTable[i] < V34_COUNT_MIN || MMaxTable[i] > V34_COUNT_MAX
		    || MMinTable[i] < V34_COUNT_MIN
		    || MMinTable[i] > V34_COUNT_MAX) {
			printf("FIXTURE: ring table [%d] = %d/%d is outside "
			       "%d..%d\n", i, MMaxTable[i], MMinTable[i],
			       V34_COUNT_MIN, V34_COUNT_MAX);
			bad = 1;
		}
	}
	for (i = V34_COUNT_MIN; i <= V34_COUNT_MAX; i++) {
		int len = xyz[i + 1] - xyz[i];

		if (len < 0 || len > 0x80) {
			printf("FIXTURE: xyz block %d is %d entries, and t3 "
			       "holds 128\n", i, len);
			bad = 1;
		}
		if (xyz[i] < 0 || xyz[i + 1] > 945) {
			printf("FIXTURE: xyz block %d runs [%d,%d) outside "
			       "the table\n", i, xyz[i], xyz[i + 1]);
			bad = 1;
		}
	}
	return bad;
}

int
main(void)
{
	int rc = 0;

	/*
	 * The two clamps are the whole point: each fires on a comparison
	 * against `count - 1`, and neither is an else-branch of the other,
	 * so the sweep has to reach groups where one, both and neither
	 * apply.  Sub-indices therefore run from small to well past any
	 * plausible count, and include values with bit 15 set so the mixed
	 * signedness shows.
	 */
	diff_begin("v34 shellDemapper");
	{
		static struct v34_shell a, b;
		int cnt, k, v, i;

		for (cnt = 1; cnt <= 64; cnt += 7)
		for (v = 0; v < 24; v++) {
			memset(&a, HARNESS_MALLOC_FILL, sizeof(a));
			memset(&b, HARNESS_MALLOC_FILL, sizeof(b));

			for (k = 0; k < 0x80; k++) {
				a.t1[k] = b.t1[k] = (short)(k * 37 + v);
				a.t2[k] = b.t2[k] = (short)(k * 53 - 400 + v);
			}
			for (k = 0; k < (int)(sizeof(a.t3) / sizeof(a.t3[0]));
			     k++)
				a.t3[k] = b.t3[k] = k * 1013 - 5000;

			a.count = b.count = (short)cnt;
			for (k = 0; k < V34_SHELL_SUBS; k++) {
				int e = (v * 5 + k * 3) % 13;

				/*
				 * Only the SIGNED slots (2 and 6) may go
				 * negative -- the others are read unsigned,
				 * so a negative there becomes ~65500 and the
				 * running totals leave the tables entirely.
				 */
				if ((k & 3) == 2 && (v & 1))
					e = -(e % 5) - 1;
				a.sub[k] = b.sub[k] = (short)e;
			}

			/*
			 * NEITHER SIDE BOUNDS-CHECKS.  t1, t2 and t3 are
			 * indexed by the running totals with nothing
			 * clamping them, so a caller that lets a group sum
			 * past the table length reads whatever follows --
			 * identically on both sides only by luck, and off
			 * the end of the object soon after.  The sweep stays
			 * inside deliberately, and says so here rather than
			 * leaving a future widening to discover it with a
			 * segfault, which is how this fixture found it.
			 */
			{
				int s0 = 0, s1 = 0;

				for (k = 0; k < 4; k++)
					s0 += a.sub[k];
				for (k = 4; k < 8; k++)
					s1 += a.sub[k];
				if (s0 < 0 || s1 < 0
				    || s0 >= 0x80 || s1 >= 0x80
				    || s0 + s1 >= 0x80) {
					printf("FIXTURE: sub totals %d/%d "
					       "leave the tables\n", s0, s1);
					return 1;
				}
			}

			{
				int ra = shellDemapper(&a);
				int rb = ref_shellDemapper(&b);

				diff_eq_int("shell index", ra, rb,
					    (long)cnt * 1000 + v);
			}

			/* Nothing in the object may move. */
			for (i = 0; i < (int)sizeof(a); i++)
				diff_eq_int("shell object at %ld",
					    ((unsigned char *)&a)[i],
					    ((unsigned char *)&b)[i],
					    (long)cnt * 1000000 + v * 100000 + i);
		}
	}
	rc |= diff_end();

	/* count == 1 short-circuits before reading anything else. */
	diff_begin("v34 shellDemapper: count 1");
	{
		static struct v34_shell a, b;

		memset(&a, HARNESS_MALLOC_FILL, sizeof(a));
		memset(&b, HARNESS_MALLOC_FILL, sizeof(b));
		a.count = b.count = 1;
		diff_eq_int("count 1 returns 0", shellDemapper(&a),
			    ref_shellDemapper(&b), 0);
		diff_eq_int("count 1 returns 0 literally", shellDemapper(&a),
			    0, 0);
	}
	rc |= diff_end();

	/*
	 * putFrame: the three ways it sends the wide field, including the
	 * one that sends nothing and narrows the last group instead.  `nb`
	 * comes from fa0e or fa10 depending on how fa00 compares with the
	 * running sum, so both of those are swept as well as the widths.
	 */
	diff_begin("v34 putFrame");
	{
		static struct v34_shell a, b;
		int nb, wide, a04, k, i;

		for (nb = -6; nb <= 20; nb++)
		for (wide = 0; wide <= 1; wide++)
		for (a04 = 6; a04 <= 10; a04 += 2) {
			memset(&a, HARNESS_MALLOC_FILL, sizeof(a));
			memset(&b, HARNESS_MALLOC_FILL, sizeof(b));

			a.put_bits = sink_a;
			b.put_bits = sink_b;
			a.fa14 = b.fa14 = 3;
			a.fa04 = b.fa04 = (short)a04;
			a.fa06 = b.fa06 = 100;
			a.fa08 = b.fa08 = 200;
			/*
			 * `wide` picks which branch supplies nb, by making
			 * fa00 larger or smaller than fa06 + fa08.
			 */
			a.fa00 = b.fa00 = (short)(wide ? 1000 : 10);
			a.fa10 = b.fa10 = (short)nb;
			a.fa0e = b.fa0e = (short)nb;

			for (k = 0; k < 18; k++)
				a.frame[k] = b.frame[k] =
				    (short)(k * 4919 + nb * 31);

			nlog_a = nlog_b = 0;
			putFrame(&a);
			ref_putFrame(&b);

			diff_eq_int("putFrame call count", nlog_a, nlog_b,
				    (long)(nb + 6) * 100 + wide * 10 + a04);
			for (i = 0; i < nlog_a && i < LOGMAX; i++) {
				long tag = ((long)(nb + 6) * 100 + wide * 10
					    + a04) * 100 + i;

				diff_eq_int("putFrame value", log_a[i].val,
					    log_b[i].val, tag);
				diff_eq_int("putFrame nbits", log_a[i].nbits,
					    log_b[i].nbits, tag);
			}
			/* fa08 is folded back into the object. */
			diff_eq_int("putFrame fa08", a.fa08, b.fa08,
				    (long)(nb + 6) * 100 + wide * 10 + a04);
			for (i = 0; i < (int)sizeof(a); i++) {
				unsigned pb = __builtin_offsetof(
					struct v34_shell, put_bits);

				if (i >= (int)pb && i < (int)pb + 4)
					continue;	/* each side's sink */
				diff_eq_int("putFrame object at %ld",
					    ((unsigned char *)&a)[i],
					    ((unsigned char *)&b)[i],
					    ((long)(nb + 6) * 100 + wide * 10
					     + a04) * 100000 + i);
			}
		}
	}
	rc |= diff_end();

	/*
	 * The two tables, byte for byte.  Emitted ahead of decodeDepth, which
	 * is the only reader, precisely so they can be checked on their own:
	 * a table transcribed wrong is indistinguishable from a decoder
	 * written wrong once the two are compiled together.
	 */
	diff_begin("v34 mapper tables");
	{
		int i;

		for (i = 0; i < 16; i++)
			diff_eq_int("kLookup", kLookup[i], ref_kLookup[i], i);
		for (i = 0; i < 529; i++)
			diff_eq_int("grid", grid[i], ref_grid[i], i);

		/* kLookup is a Latin square: every row and column a
		 * permutation of 0..3.  A property the bytes alone do not
		 * state, and the thing that would break first if the table
		 * were ever regenerated rather than copied. */
		for (i = 0; i < 4; i++) {
			int rowseen = 0, colseen = 0, j;

			for (j = 0; j < 4; j++) {
				rowseen |= 1 << kLookup[i * 4 + j];
				colseen |= 1 << kLookup[j * 4 + i];
			}
			diff_eq_int("kLookup row is a permutation",
				    rowseen, 0xf, i);
			diff_eq_int("kLookup column is a permutation",
				    colseen, 0xf, i);
		}
	}
	rc |= diff_end();

	/*
	 * decodeDepth.  The sweep has to reach all sixteen combinations of
	 * the two four-way preambles (code >> 2 and code & 3), which means
	 * varying the trellis contents rather than just the inputs -- the
	 * code byte comes out of the traceback, not from a parameter.
	 * Filling the table so the walk lands on a chosen entry is the only
	 * way to drive them.
	 */
	diff_begin("v34 decodeDepth");
	{
		static struct v34_shell a, b;
		/*
		 * Small, because `grid` is indexed by (rotate + 0x408) >> 2
		 * into 529 entries with nothing clamping it -- finding 129's
		 * shape again.  Large coefficients push the residues, and
		 * hence the rotate, straight off the end of the table, where
		 * each side reads its own adjacent .rodata and the two
		 * disagree for reasons that are the fixture's, not the
		 * reconstruction's.
		 */
		static const short coeffs[12] = {
			  30,  -21,   15,   -9,    6,   -3,
			 -28,   19,  -14,    8,   -5,    2
		};
		short qa[4], qb[4], ia[2], ib[2];
		int cd, si, w, k, i;

		for (cd = 0; cd < 16; cd++)
		for (si = 0; si < 4; si++)
		for (w = 1; w <= 3; w++) {
			memset(&a, HARNESS_MALLOC_FILL, sizeof(a));
			memset(&b, HARNESS_MALLOC_FILL, sizeof(b));

			/*
			 * Every trellis entry carries the same code and a
			 * next-branch of zero, so wherever the walk goes it
			 * ends on the code under test.
			 */
			for (k = 0; k < 32 * 16; k++)
				a.trellis[k] = b.trellis[k] =
				    (unsigned short)cd;

			/*
			 * SMALL, and this is the constraint that matters:
			 * grid is indexed by (23*hi + lo + 0x408) >> 2 with
			 * no clamp, where hi is one of these parameters
			 * minus a residue.  A parameter of 200 gives an
			 * index near 1200 into a 529-entry table, and each
			 * side then reads its own adjacent .rodata.  See
			 * finding 129 -- this is the third table in this
			 * file with the same property.
			 */
			for (k = 0; k < 32; k++) {
				a.state[k].seed = b.state[k].seed = 0;
				a.state[k].a = b.state[k].a =
				    (short)((k % 9) * 4 - 16);
				a.state[k].b = b.state[k].b =
				    (short)((k % 7) * 4 - 12);
				a.state[k].c = b.state[k].c =
				    (short)((k % 11) * 4 - 20);
				a.state[k].d = b.state[k].d =
				    (short)((k % 5) * 4 - 8);
			}
			a.state_idx = b.state_idx = (short)(si * 7 + 1);
			a.divisor = b.divisor = (short)(si);   /* 0 -> 1 */
			a.wrap = b.wrap = (short)w;
			a.fa14 = b.fa14 = (short)(w + 1);
			a.prev_k = b.prev_k = (short)si;
			a.coeff = coeffs;
			b.coeff = coeffs;
			for (k = 0; k < 6; k++)
				a.hist[k] = b.hist[k] =
				    (short)(k * 41 - 90 + cd * 3);

			memset(qa, 0x5a, sizeof(qa));
			memset(qb, 0x5a, sizeof(qb));
			memset(ia, 0x5a, sizeof(ia));
			memset(ib, 0x5a, sizeof(ib));

			decodeDepth(&a, qa, ia);
			ref_decodeDepth(&b, qb, ib);


			for (k = 0; k < 4; k++)
				diff_eq_int("depth quad %ld", qa[k], qb[k],
					    ((long)cd * 100 + si * 10 + w)
					    * 10 + k);
			for (k = 0; k < 2; k++)
				diff_eq_int("depth idx %ld", ia[k], ib[k],
					    ((long)cd * 100 + si * 10 + w)
					    * 10 + k);
			for (i = 0; i < (int)sizeof(a); i++) {
				unsigned cp = __builtin_offsetof(
					struct v34_shell, coeff);

				if (i >= (int)cp && i < (int)cp + 4)
					continue;	/* each side's table */
				diff_eq_int("depth object at %ld",
					    ((unsigned char *)&a)[i],
					    ((unsigned char *)&b)[i],
					    ((long)cd * 100 + si * 10 + w)
					    * 100000 + i);
			}
		}
	}
	rc |= diff_end();

	/*
	 * demapFrame, driven eight sub-frames at a time so both parities and
	 * the frame-completing eighth are reached.  Parameters stay small for
	 * the same reason as decodeDepth's: grid is indexed unclamped.
	 */
	diff_begin("v34 demapFrame");
	{
		static struct v34_shell a, b;
		static const short coeffs[12] = {
			  30,  -21,   15,   -9,    6,   -3,
			 -28,   19,  -14,    8,   -5,    2
		};
		int inv, w, base, k, i, sf;
		int pa[2], pb[2], oa[2], ob[2];

		for (inv = 0; inv <= 1; inv++)
		for (w = 1; w <= 2; w++)
		for (base = 0; base < 3; base++) {
			memset(&a, HARNESS_MALLOC_FILL, sizeof(a));
			memset(&b, HARNESS_MALLOC_FILL, sizeof(b));

			for (k = 0; k < 32 * 16; k++)
				a.trellis[k] = b.trellis[k] = 0;
			for (k = 0; k < 32; k++) {
				a.state[k].seed = b.state[k].seed = 0;
				a.state[k].a = b.state[k].a =
				    (short)((k % 9) * 4 - 16);
				a.state[k].b = b.state[k].b =
				    (short)((k % 7) * 4 - 12);
				a.state[k].c = b.state[k].c =
				    (short)((k % 11) * 4 - 20);
				a.state[k].d = b.state[k].d =
				    (short)((k % 5) * 4 - 8);
			}
			for (k = 0; k < 16; k++)
				a.cost[k] = b.cost[k] = (short)(k * 3);
			for (k = 0; k < 18; k++)
				a.frame[k] = b.frame[k] = (short)(k * 7);
			for (k = 0; k < 8; k++)
				a.sub[k] = b.sub[k] = (short)(k + 1);
			for (k = 0; k < 6; k++)
				a.hist[k] = b.hist[k] =
				    (short)(k * 41 - 90 + base * 3);
			for (k = 0; k < 0x80; k++) {
				a.t1[k] = b.t1[k] = (short)(k * 3 + 1);
				a.t2[k] = b.t2[k] = (short)(k * 5 + 2);
			}
			for (k = 0; k < 0x80; k++)
				a.t3[k] = b.t3[k] = k * 11;

			a.coeff = coeffs; b.coeff = coeffs;
			a.put_bits = sink_a; b.put_bits = sink_b;
			a.state_idx = b.state_idx = (short)(base * 5);
			a.divisor = b.divisor = 1;
			a.wrap = b.wrap = (short)w;
			a.fa14 = b.fa14 = (short)(w + 1);
			a.fa44 = b.fa44 = 2;
			a.invert = b.invert = (short)inv;
			a.fa00 = b.fa00 = 3;
			a.fa02 = b.fa02 = 4;
			a.fa3c = b.fa3c = 0;
			a.fa3e = b.fa3e = 0;
			a.fa40 = b.fa40 = 3;
			a.latched = b.latched = 0;
			a.prev_k = b.prev_k = 0;
			a.count = b.count = 9;
			a.fa08 = b.fa08 = 0;
			a.fa06 = b.fa06 = 3;
			a.fa0e = b.fa0e = 8;
			a.fa10 = b.fa10 = 8;
			a.fa04 = b.fa04 = 7;

			nlog_a = nlog_b = 0;

			for (sf = 0; sf < 16; sf++) {
				long tag = ((long)inv * 100 + w * 10 + base)
					 * 100 + sf;
				int ra, rb2;

				pa[0] = pb[0] = (sf * 37) & 0xffff;
				pa[1] = pb[1] = (sf * 53) & 0xffff;
				memset(oa, 0x5a, sizeof(oa));
				memset(ob, 0x5a, sizeof(ob));

				ra  = demapFrame(&a, pa, oa,
						 (short)(sf + 0x41));
				rb2 = ref_demapFrame(&b, pb, ob,
						     (short)(sf + 0x41));

				diff_eq_int("demap ret", ra, rb2, tag);
				diff_eq_int("demap out0", ((short *)oa)[0],
					    ((short *)ob)[0], tag);
				diff_eq_int("demap out1", ((short *)oa)[1],
					    ((short *)ob)[1], tag);
			}

			diff_eq_int("demap emit count", nlog_a, nlog_b,
				    (long)inv * 100 + w * 10 + base);
			/*
			 * And it must actually have emitted: with n running
			 * 0x41..0x50 the eighth sub-frame falls twice, so a
			 * zero here would mean the frame-completing path
			 * never ran and the comparison above proved nothing.
			 */
			diff_eq_int("demap emitted at all", nlog_a > 0, 1,
				    (long)inv * 100 + w * 10 + base);
			for (i = 0; i < nlog_a && i < LOGMAX; i++) {
				long tag = ((long)inv * 100 + w * 10 + base)
					 * 100 + i;

				diff_eq_int("demap emit value", log_a[i].val,
					    log_b[i].val, tag);
				diff_eq_int("demap emit nbits",
					    log_a[i].nbits, log_b[i].nbits,
					    tag);
			}
			for (i = 0; i < (int)sizeof(a); i++) {
				unsigned cp = __builtin_offsetof(
					struct v34_shell, coeff);
				unsigned pb2 = __builtin_offsetof(
					struct v34_shell, put_bits);

				if ((i >= (int)cp && i < (int)cp + 4)
				    || (i >= (int)pb2 && i < (int)pb2 + 4))
					continue;
				diff_eq_int("demap object at %ld",
					    ((unsigned char *)&a)[i],
					    ((unsigned char *)&b)[i],
					    ((long)inv * 100 + w * 10 + base)
					    * 100000 + i);
			}
		}
	}
	rc |= diff_end();

	diff_begin("v34 lsbMask");
	{
		int i;

		for (i = 0; i < 17; i++) {
			diff_eq_int("lsbMask", lsbMask[i], ref_lsbMask[i], i);
			/* And the generator, which is exact. */
			diff_eq_int("lsbMask is (1<<n)-1", lsbMask[i],
				    (1 << i) - 1, i);
		}
	}
	rc |= diff_end();

	/*
	 * getFrame: the three wide-field paths and all four groups, driven
	 * through a bit source that each side owns.  The object is compared
	 * whole, so the transmit context at +V34_SHELL_TX is covered without
	 * naming its fields individually.
	 */
	diff_begin("v34 getFrame");
	{
		static unsigned char oa[V34_SHELL_TX + sizeof(struct v34_shell)];
		static unsigned char ob[V34_SHELL_TX + sizeof(struct v34_shell)];
		struct v34_shell *sa, *sb;
		int nb, wide, a04, k, i;

		/*
		 * nb from -1, not lower: below that `small + nb` goes
		 * negative and the object indexes lsbMask before the table.
		 * A field width is never negative in operation, so this is
		 * the same bound finding 129 describes, asserted rather than
		 * discovered.
		 */
		for (nb = -1; nb <= 20; nb++)
		for (wide = 0; wide <= 1; wide++)
		for (a04 = 6; a04 <= 10; a04 += 2) {
			memset(oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(ob, HARNESS_MALLOC_FILL, sizeof(ob));
			sa = (struct v34_shell *)(oa + V34_SHELL_TX);
			sb = (struct v34_shell *)(ob + V34_SHELL_TX);

			for (k = 0; k < 64; k++)
				src_words_a[k] = src_words_b[k] =
				    (unsigned)(k * 2654435761u
					       + (unsigned)nb * 40503u);
			src_i_a = src_i_b = 0;

			sa->get_bits = bitsrc_a;
			sb->get_bits = bitsrc_b;
			sa->fa14 = sb->fa14 = 3;
			sa->fa04 = sb->fa04 = (short)a04;
			sa->fa06 = sb->fa06 = 100;
			sa->fa08 = sb->fa08 = 200;
			sa->fa00 = sb->fa00 = (short)(wide ? 1000 : 10);
			sa->fa10 = sb->fa10 = (short)nb;
			sa->fa0e = sb->fa0e = (short)nb;
			sa->bitbuf = sb->bitbuf = (int)0x5a3c7e91;
			sa->bitpos = sb->bitpos = 0;
			for (k = 0; k < 18; k++)
				sa->frame[k] = sb->frame[k] = 0;

			getFrame(oa);
			ref_getFrame(ob);

			for (i = 0; i < (int)sizeof(oa); i++) {
				unsigned bp = V34_SHELL_TX
				    + __builtin_offsetof(struct v34_shell,
						     put_bits);

				if (i >= (int)bp && i < (int)bp + 4)
					continue;	/* each side's source */
				diff_eq_int("getFrame at %ld", oa[i], ob[i],
					    ((long)(nb + 6) * 100 + wide * 10
					     + a04) * 100000 + i);
			}
			diff_eq_int("getFrame refills", src_i_a, src_i_b,
				    (long)(nb + 6) * 100 + wide * 10 + a04);
		}
	}
	rc |= diff_end();

	/*
	 * ROUND TRIP.  putFrame emits a frame through a bit sink; getFrame
	 * reads one back from a bit source.  Feeding one into the other must
	 * return the frame unchanged, and that has to hold for reasons that
	 * have NOTHING to do with matching the blob -- which is the point.
	 * Both sides here are ours, so this is not a differential test and
	 * does not replace one; two functions sharing one wrong table would
	 * still round-trip.  It is an additional oracle, and it catches the
	 * class a differential test cannot: a field packed at the wrong
	 * offset in BOTH directions consistently.
	 */
	diff_begin("v34 putFrame/getFrame round trip");
	{
		static unsigned char obj[V34_SHELL_TX
					 + sizeof(struct v34_shell)];
		struct v34_shell *tx;
		int nb, a04, k, g;

		for (nb = 1; nb <= 16; nb++)
		for (a04 = 6; a04 <= 10; a04 += 2) {
			int w = 3;
			int small = 2;

			memset(obj, 0, sizeof(obj));
			tx = (struct v34_shell *)(obj + V34_SHELL_TX);

			tx->fa14 = (short)w;
			tx->fa04 = (short)a04;
			tx->fa06 = 100;
			tx->fa08 = 200;
			tx->fa00 = 10;		/* takes the fa0e branch */
			tx->fa0e = tx->fa10 = (short)nb;

			/* A frame whose every field is inside its width. */
			tx->frame[0] = (short)(((1 << nb) - 1) & 0x5a5a);
			tx->frame[1] = 0;
			for (g = 0; g < 4; g++) {
				short *p = &tx->frame[2 + g * 4];

				p[0] = (short)(g & 1);
				p[1] = (short)((g + 1) & ((1 << small) - 1));
				p[2] = (short)((g * 3 + 1) & ((1 << w) - 1));
				p[3] = (short)((g * 5 + 2) & ((1 << w) - 1));
			}

			/* Remember it, emit it, read it back. */
			memcpy(rt_want, tx->frame, sizeof(rt_want));
			rt_bits = 0;
			rt_n = 0;
			tx->put_bits = rt_sink;
			putFrame(tx);

			memset(tx->frame, 0, sizeof(tx->frame));
			tx->fa08 = 200;		/* putFrame advanced it */
			rt_rd = -16;
			tx->get_bits = rt_source;
			tx->bitbuf = 0;
			tx->bitpos = 16;	/* just past, so refill first */
			getFrame(obj);

			for (k = 0; k < 18; k++) {
				if (k == 1)
					continue;	/* unused below 17 bits */
				diff_eq_int("round trip frame[%ld]",
					    tx->frame[k], rt_want[k],
					    (long)nb * 1000 + a04 * 100 + k);
			}
		}
	}
	rc |= diff_end();

	/*
	 * The tables, against the object's own copies.  945 ints transcribed
	 * by hand is exactly the kind of thing that goes wrong in the middle
	 * and nowhere else, and a memcmp against ref_* costs nothing.
	 */
	diff_begin("v34 shell tables");
	{
		diff_eq_int("xyz", memcmp(xyz, ref_xyz, sizeof(xyz)), 0, 0);
		diff_eq_int("Convolve16",
			    memcmp(Convolve16, ref_Convolve16,
				   sizeof(Convolve16)), 0, 0);
		diff_eq_int("Convolve32",
			    memcmp(Convolve32, ref_Convolve32,
				   sizeof(Convolve32)), 0, 0);
		diff_eq_int("Convolve64",
			    memcmp(Convolve64, ref_Convolve64,
				   sizeof(Convolve64)), 0, 0);
		diff_eq_int("MMaxTable",
			    memcmp(MMaxTable, ref_MMaxTable,
				   sizeof(MMaxTable)), 0, 0);
		diff_eq_int("MMinTable",
			    memcmp(MMinTable, ref_MMinTable,
				   sizeof(MMinTable)), 0, 0);
		diff_eq_int("ring size stays in xyz", check_count_bound(), 0, 0);
	}
	rc |= diff_end();

	/* setScramble and scaleVector: the two nothing in the object calls. */
	diff_begin("v34 setScramble/scaleVector");
	{
		static struct v34_shell a, b;
		short va[16], vb[16];
		int k, sc;

		memset(&a, HARNESS_MALLOC_FILL, sizeof(a));
		memset(&b, HARNESS_MALLOC_FILL, sizeof(b));
		setScramble((char *)&a + V34_SHELL_FIELDS,
			    (void *)descrambleGPC);
		ref_setScramble((char *)&b + V34_SHELL_FIELDS,
				(void *)ref_descrambleGPC);
		diff_eq_int("setScramble object", shell_bytes_eq("setScramble",
			    &a, &b, 0), 0, 0);
		diff_eq_int("setScramble stored ours",
			    a.put_bits == descrambleGPC, 1, 0);
		diff_eq_int("setScramble stored theirs",
			    b.put_bits == (v34_putbits_fn)ref_descrambleGPC,
			    1, 0);

		for (sc = -300; sc <= 300; sc += 37) {
			for (k = 0; k < 16; k++)
				va[k] = vb[k] = (short)(k * 4001 - 20000);
			scaleVector(va, (short)sc);
			ref_scaleVector(vb, (short)sc);
			for (k = 0; k < 16; k++)
				diff_eq_int("scaleVector[%ld]", va[k], vb[k],
					    (long)sc * 100 + k);
		}
	}
	rc |= diff_end();

	/* preinitV34, on both contexts' worth of offsets. */
	diff_begin("v34 preinitV34");
	{
		static struct v34_shell a, b;
		int pass;

		for (pass = 0; pass < 2; pass++) {
			memset(&a, pass ? 0 : HARNESS_MALLOC_FILL, sizeof(a));
			memset(&b, pass ? 0 : HARNESS_MALLOC_FILL, sizeof(b));

			preinitV34((char *)&a + V34_SHELL_FIELDS);
			ref_preinitV34((char *)&b + V34_SHELL_FIELDS);

			diff_eq_int("preinitV34 object",
				    shell_bytes_eq("preinitV34 byte", &a, &b,
						   pass), 0, pass);
			diff_eq_int("preinitV34 conv ours",
				    a.conv == Convolve16, 1, pass);
			diff_eq_int("preinitV34 conv theirs",
				    b.conv == ref_Convolve16, 1, pass);
			diff_eq_int("preinitV34 callback ours",
				    a.get_bits == scrambleGPC, 1, pass);
			diff_eq_int("preinitV34 callback theirs",
				    b.get_bits ==
				    (v34_getbits_fn)ref_scrambleGPC, 1, pass);
		}
	}
	rc |= diff_end();

	/*
	 * initG248, over every ring size initV34 can ask for and no others.
	 * Sixteen is in the sweep on purpose: xyz has an EMPTY block for it,
	 * because MMaxTable runs 15, 17, 18 and never produces a 16, so the
	 * copy loop must do nothing at all and leave preinitV34's -1s showing.
	 */
	diff_begin("v34 initG248");
	{
		static struct v34_shell a, b;
		int n;

		for (n = V34_COUNT_MIN; n <= V34_COUNT_MAX; n++) {
			memset(&a, HARNESS_MALLOC_FILL, sizeof(a));
			memset(&b, HARNESS_MALLOC_FILL, sizeof(b));
			preinitV34((char *)&a + V34_SHELL_FIELDS);
			ref_preinitV34((char *)&b + V34_SHELL_FIELDS);
			a.count = b.count = (short)n;

			initG248((char *)&a + V34_SHELL_FIELDS);
			ref_initG248((char *)&b + V34_SHELL_FIELDS);

			diff_eq_int("initG248 object",
				    shell_bytes_eq("initG248 byte", &a, &b, n),
				    0, n);
		}
	}
	rc |= diff_end();

	/*
	 * initV34.  The six real V.34 symbol rates, both ring tables and all
	 * three trellis depths.
	 *
	 * `bitrate` is swept over the whole 16-bit range rather than over the
	 * data rates a modem would use, and that is deliberate: it is divided
	 * by 100 and then by the group and the span, and a realistic rate only
	 * ever reaches the bottom third of the ring-size table.  The wide
	 * sweep is what makes counts up to 18 -- and so xyz's truncated
	 * blocks, and its empty one -- reachable at all.  check_count_bound()
	 * above proves the sweep cannot walk any of the three tables off its
	 * end, which is the thing finding 129 warns about.
	 */
	diff_begin("v34 initV34");
	{
		static struct v34_shell a, b;
		static const short rates[6] = { 2400, 2743, 2800, 3000,
						3200, 3429 };
		static const short coeffs[12] = { 1, -2, 3, -4, 5, -6,
						  7, -8, 9, -10, 11, -12 };
		int r, mx, dep, br, dv, ra, rb;
		long ctx = 0;

		for (r = 0; r < 6; r++)
		for (mx = 0; mx < 2; mx++)
		for (dep = 0; dep < 3; dep++)
		for (br = 300; br < 65500; br += 1637)
		for (dv = 0; dv < 4; dv++) {
			short divisor = (short)(dv == 0 ? 0
					      : dv == 1 ? 0x80
					      : dv == 2 ? 0x81 : 30000);

			memset(&a, HARNESS_MALLOC_FILL, sizeof(a));
			memset(&b, HARNESS_MALLOC_FILL, sizeof(b));
			preinitV34((char *)&a + V34_SHELL_FIELDS);
			ref_preinitV34((char *)&b + V34_SHELL_FIELDS);

			ra = initV34((char *)&a + V34_SHELL_FIELDS, rates[r],
				     (short)br, (short)mx, (short)dep,
				     coeffs, divisor);
			rb = ref_initV34((char *)&b + V34_SHELL_FIELDS,
					 rates[r], (short)br, (short)mx,
					 (short)dep, coeffs, divisor);

			ctx++;
			diff_eq_int("initV34 return", ra, rb, ctx);
			diff_eq_int("initV34 object",
				    shell_bytes_eq("initV34 byte", &a, &b,
						   ctx), 0, ctx);

			/* The ring size it picked must be one xyz has. */
			if (a.count < V34_COUNT_MIN
			    || a.count > V34_COUNT_MAX) {
				printf("FIXTURE: initV34 produced count %d "
				       "at rate %d bitrate %d\n",
				       a.count, rates[r], br);
				return 1;
			}

			diff_eq_int("initV34 conv ours",
				    a.conv == (dep == 0 ? Convolve16
					     : dep == 1 ? Convolve32
					     : Convolve64), 1, ctx);
			diff_eq_int("initV34 conv theirs",
				    b.conv == (dep == 0 ? ref_Convolve16
					     : dep == 1 ? ref_Convolve32
					     : ref_Convolve64), 1, ctx);
		}
	}
	rc |= diff_end();

	/*
	 * The four bit callbacks, driven through the object they belong to.
	 *
	 * Each side gets its own object, because both read and WRITE the data
	 * buffers -- one shared object would have the second call see the
	 * first's cursor.
	 */
	diff_begin("v34 scrambler callbacks");
	{
		static unsigned char oa[sizeof(struct v34_object)];
		static unsigned char ob[sizeof(struct v34_object)];
		struct v34_object *ja = (struct v34_object *)oa;
		struct v34_object *jb = (struct v34_object *)ob;
		struct v34_shell *ta = (struct v34_shell *)(oa + V34_SHELL_TX);
		struct v34_shell *tb = (struct v34_shell *)(ob + V34_SHELL_TX);
		struct v34_shell *ra = (struct v34_shell *)oa;
		struct v34_shell *rb = (struct v34_shell *)ob;
		int en, gpa, k, step;
		long ctx = 0;

		for (en = 0; en < 2; en++)
		for (gpa = 0; gpa < 2; gpa++) {
			memset(oa, 0, sizeof(oa));
			memset(ob, 0, sizeof(ob));
			ja->data_enable = jb->data_enable = (short)en;
			ja->tx_n = jb->tx_n = 40;
			for (k = 0; k < 64; k++)
				ja->tx_data[k] = jb->tx_data[k] =
					k * 2357 + 11;

			/* The transmit side: sixteen bits per call. */
			for (step = 0; step < 24; step++) {
				int pa, pb;

				if (gpa) {
					pa = scrambleGPA(oa, 16 + step);
					pb = ref_scrambleGPA(ob, 16 + step);
				} else {
					pa = scrambleGPC(oa, 16 + step);
					pb = ref_scrambleGPC(ob, 16 + step);
				}
				ctx++;
				diff_eq_int("scramble pos", pa, pb, ctx);
				for (k = 0; k < 3; k++)
					diff_eq_int("scramble scr[%ld]",
						    ta->scr[k], tb->scr[k],
						    ctx * 10 + k);
				diff_eq_int("scramble bitbuf", ta->bitbuf,
					    tb->bitbuf, ctx);
				diff_eq_int("scramble tx_rd", ja->tx_rd,
					    jb->tx_rd, ctx);
			}

			/* The receive side: a width at a time, crossing 32. */
			for (step = 0; step < 60; step++) {
				int val = (step * 7919) & 0xffff;
				int nb = 1 + (step % 16);

				if (gpa) {
					descrambleGPA(oa, val, nb);
					ref_descrambleGPA(ob, val, nb);
				} else {
					descrambleGPC(oa, val, nb);
					ref_descrambleGPC(ob, val, nb);
				}
				ctx++;
				for (k = 0; k < 3; k++)
					diff_eq_int("descramble scr[%ld]",
						    ra->scr[k], rb->scr[k],
						    ctx * 10 + k);
				diff_eq_int("descramble bitpos",
					    ra->rx_bitpos, rb->rx_bitpos, ctx);
				diff_eq_int("descramble rx_n", ja->rx_n,
					    jb->rx_n, ctx);
				for (k = 0; k < 64; k++)
					diff_eq_int("descramble rx_data[%ld]",
						    ja->rx_data[k],
						    jb->rx_data[k],
						    ctx * 100 + k);
			}
		}
	}
	rc |= diff_end();

	/*
	 * preinitdigital: both contexts at once, and the role branch.
	 *
	 * Compared over the WHOLE object, not just the two shells, because
	 * the three memsets and the scalars past them are most of what it
	 * does -- and because the transmit context's scrambler words are
	 * cleared differently from the receive one's, which a shell-only
	 * compare would show as a pass either way.
	 */
	diff_begin("v34 preinitdigital");
	{
		static unsigned char oa[sizeof(struct v34_object)];
		static unsigned char ob[sizeof(struct v34_object)];
		struct v34_object *ja = (struct v34_object *)oa;
		struct v34_object *jb = (struct v34_object *)ob;
		struct v34_shell *ta = (struct v34_shell *)(oa + V34_SHELL_TX);
		struct v34_shell *tb = (struct v34_shell *)(ob + V34_SHELL_TX);
		struct v34_shell *ra = (struct v34_shell *)oa;
		struct v34_shell *rb = (struct v34_shell *)ob;
		int role, i, bad;

		for (role = 0; role < 2; role++) {
			memset(oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(ob, HARNESS_MALLOC_FILL, sizeof(ob));
			ja->f359c = jb->f359c = (short)(role ? 0x65 : 0x12);

			preinitdigital(oa);
			ref_preinitdigital(ob);

			bad = 0;
			for (i = 0; i < (int)sizeof(oa); i++) {
				/* The four pointer fields, as always. */
				if ((i >= 0xa28 && i < 0xa2c)
				    || (i >= 0xe48 && i < 0xe4c)
				    || (i >= 0x2608 && i < 0x260c)
				    || (i >= 0x2a28 && i < 0x2a2c))
					continue;
				if (oa[i] != ob[i] && bad++ < 8)
					diff_eq_int("preinitdigital byte",
						    oa[i], ob[i],
						    (long)role * 0x100000 + i);
			}
			diff_eq_int("preinitdigital object", bad, 0, role);

			diff_eq_int("preinitdigital tx conv",
				    ta->conv == Convolve16, 1, role);
			diff_eq_int("preinitdigital rx conv",
				    ra->conv == Convolve16, 1, role);
			diff_eq_int("preinitdigital tx conv theirs",
				    tb->conv == ref_Convolve16, 1, role);
			diff_eq_int("preinitdigital rx conv theirs",
				    rb->conv == ref_Convolve16, 1, role);

			diff_eq_int("preinitdigital scrambler",
				    ta->get_bits == (role ? scrambleGPC
							  : scrambleGPA),
				    1, role);
			diff_eq_int("preinitdigital descrambler",
				    ra->put_bits == (role ? descrambleGPA
							  : descrambleGPC),
				    1, role);
			diff_eq_int("preinitdigital scrambler theirs",
				    tb->get_bits ==
				    (v34_getbits_fn)(role ? ref_scrambleGPC
							  : ref_scrambleGPA),
				    1, role);
			diff_eq_int("preinitdigital descrambler theirs",
				    rb->put_bits ==
				    (v34_putbits_fn)(role ? ref_descrambleGPA
							  : ref_descrambleGPC),
				    1, role);

			/* The one place the two contexts genuinely differ. */
			diff_eq_int("tx bitpos starts past 15",
				    ta->bitpos, 32, role);
			diff_eq_int("rx bitpos starts at zero",
				    ra->rx_bitpos, 0, role);
		}
	}
	rc |= diff_end();


	return rc;
}

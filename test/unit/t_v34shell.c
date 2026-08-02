/*
 * t_v34shell.c -- differential test of the V.34 shell demapper.
 */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "dsplib/v34shell.h"

extern int ref_shellDemapper(void *s);
extern void ref_putFrame(void *s);
extern const short ref_kLookup[16];
extern const short ref_grid[529];
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
					    (long)cnt * 1000000 + v * 10000 + i);
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
					     + a04) * 10000 + i);
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

	return rc;
}

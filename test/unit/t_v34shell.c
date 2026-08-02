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

	return rc;
}

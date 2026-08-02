/*
 * t_v34shell.c -- differential test of the V.34 shell demapper.
 */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "dsplib/v34shell.h"

extern int ref_shellDemapper(void *s);

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

	return rc;
}

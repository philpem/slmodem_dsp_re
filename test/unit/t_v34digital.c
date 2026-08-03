/*
 * t_v34digital.c -- differential test of preinitdigital.
 *
 * It writes about 3.5 KB scattered across a 44 KB object in two contexts
 * 0x1be0 apart, so the comparison is the whole object byte for byte with
 * only the five installed POINTERS skipped -- and those are checked by which
 * function or table they select, which is the thing that matters.
 *
 * The pointer check is the point of the test, not an aside: `f359c` decides
 * which polynomial goes which way, and getting that backwards would swap the
 * two ends of the call while leaving every other byte identical.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v34digital.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34scram.h"
#include "dsplib/v34shell.h"

extern void ref_preinitdigital(void *obj);
extern short ref_scrambleGPC(void *obj, short n);
extern short ref_scrambleGPA(void *obj, short n);
extern int ref_descrambleGPC(void *obj, unsigned short b, unsigned short n);
extern int ref_descrambleGPA(void *obj, unsigned short b, unsigned short n);
extern const short ref_Convolve16[64];

static struct v34_object oa;
static unsigned char ob[sizeof(struct v34_object)];

/* The five pointer fields: two per context, plus each context's convolve. */
static const unsigned ptr_skip[] = {
	0x0a28, 0x0e48,				/* receive context  */
	0x0a28 + V34_SHELL_TX, 0x0e48 + V34_SHELL_TX	/* transmit         */
};
#define NPTR (sizeof(ptr_skip) / sizeof(ptr_skip[0]))

static int
skipped(unsigned off)
{
	unsigned k;

	for (k = 0; k < NPTR; k++)
		if (off >= ptr_skip[k] && off < ptr_skip[k] + 4)
			return 1;
	return 0;
}

static void
compare(const char *what, long tag)
{
	const unsigned char *p = (const unsigned char *)&oa;
	unsigned i;
	int bad = 0;

	for (i = 0; i < sizeof(oa); i++) {
		if (p[i] == ob[i] || skipped(i))
			continue;
		bad++;
		if (bad <= 5) {
			char m[160];

			snprintf(m, sizeof(m), "%s: byte at +0x%x (case %ld)",
				 what, i, tag);
			diff_eq_int(m, p[i], ob[i], (long)i);
		}
	}
	diff_eq_int(what, bad, 0, tag);
}

static void *
ptr_at(const void *base, unsigned off)
{
	void *p;

	memcpy(&p, (const unsigned char *)base + off, sizeof(p));
	return p;
}

int
main(void)
{
	int rc = 0;
	unsigned f;
	static const short flags[] = { 0x65, 0, 1, 0x64, 0x66, -1, 0x7fff };

	diff_begin("v34 digital: preinitdigital, both ends of the call");
	{
		for (f = 0; f < sizeof(flags) / sizeof(flags[0]); f++) {
			short v = flags[f];
			int orig = (v == 0x65);

			memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(ob, HARNESS_MALLOC_FILL, sizeof(ob));
			memcpy((unsigned char *)&oa + 0x359c, &v, sizeof(v));
			memcpy(ob + 0x359c, &v, sizeof(v));

			preinitdigital(&oa);
			ref_preinitdigital(ob);
			compare("preinitdigital", v);

			/*
			 * Each side must have installed ITS OWN copy of the
			 * right function -- compared by identity against the
			 * two sets, which is what an address comparison
			 * cannot do and what actually matters here.
			 */
			diff_eq_int("tx scrambler",
				    ptr_at(&oa, 0x0e48 + V34_SHELL_TX)
				    == (void *)(orig ? scrambleGPC
						     : scrambleGPA), 1, v);
			diff_eq_int("ref tx scrambler",
				    ptr_at(ob, 0x0e48 + V34_SHELL_TX)
				    == (void *)(orig ? ref_scrambleGPC
						     : ref_scrambleGPA), 1, v);
			diff_eq_int("rx descrambler",
				    ptr_at(&oa, 0x0e48)
				    == (void *)(orig ? descrambleGPA
						     : descrambleGPC), 1, v);
			diff_eq_int("ref rx descrambler",
				    ptr_at(ob, 0x0e48)
				    == (void *)(orig ? ref_descrambleGPA
						     : ref_descrambleGPC),
				    1, v);

			/* And both convolve pointers select the same table. */
			{
				const short *ca = ptr_at(&oa, 0x0a28);
				const short *cb = ptr_at(ob, 0x0a28);
				int k, diffs = 0;

				for (k = 0; k < 64; k++)
					if (ca[k] != cb[k])
						diffs++;
				diff_eq_int("convolve table", diffs, 0, v);
				diff_eq_int("and it is Convolve16",
					    ca == Convolve16
					    && cb == ref_Convolve16, 1, v);
			}
		}
	}
	rc |= diff_end();

	diff_begin("v34 digital: it is idempotent");
	{
		short v = 0x65;

		memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
		memset(ob, HARNESS_MALLOC_FILL, sizeof(ob));
		memcpy((unsigned char *)&oa + 0x359c, &v, sizeof(v));
		memcpy(ob + 0x359c, &v, sizeof(v));
		preinitdigital(&oa);
		ref_preinitdigital(ob);
		preinitdigital(&oa);
		ref_preinitdigital(ob);
		compare("preinitdigital twice", 0);

		/* t3 is set to -1, not zero, and that is the whole point. */
		{
			struct v34_shell *rx = (struct v34_shell *)&oa;

			diff_eq_int("t3 is -1", rx->t3[0], -1, 0);
			diff_eq_int("t3 is -1 throughout", rx->t3[0x7f], -1, 0);
			diff_eq_int("t1 is zero", rx->t1[0x7f], 0, 0);
			diff_eq_int("fa16 is 0x18", rx->fa16, 0x18, 0);
		}
	}
	rc |= diff_end();

	return rc;
}

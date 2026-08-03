/*
 * t_v34scram.c -- differential test of the V.34 scrambler pair.
 *
 * These four are pure state machines over a 44 KB object, so the comparison
 * is the whole object byte for byte -- there are no pointers to skip, which
 * makes this the strictest form in the tree.
 *
 * THE REGISTER IS THE OUTPUT.  Both descramblers return 0 unconditionally
 * and both scramblers return their argument minus 16, so nothing useful
 * comes back through the return value: what has to be compared is the shift
 * register itself, and the whole-object compare does that.  The capture
 * sink is driven as well, because it is the only path on which a recovered
 * word is visible at all.
 *
 * DRIVEN IN LONG RUNS, NOT SINGLE CALLS.  A 128-bit register stepped 16 bits
 * at a time takes eight calls to turn over once, and a feedback term that
 * was transcribed one bit out shows up only after the wrong bits have
 * propagated -- so each case runs 64 steps and compares after every one.
 * A single-call test passes on arithmetic that is badly wrong.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34scram.h"

extern short ref_scrambleGPC(void *obj, short nbits);
extern short ref_scrambleGPA(void *obj, short nbits);
extern int ref_descrambleGPC(void *obj, unsigned short b, unsigned short n);
extern int ref_descrambleGPA(void *obj, unsigned short b, unsigned short n);

static struct v34_object oa;
static unsigned char ob[sizeof(struct v34_object)];

static void
setup(void)
{
	memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
	memset(ob, HARNESS_MALLOC_FILL, sizeof(ob));
}

static void
compare(const char *what, long tag)
{
	const unsigned char *p = (const unsigned char *)&oa;
	unsigned i;
	int bad = 0;

	for (i = 0; i < sizeof(oa); i++) {
		if (p[i] == ob[i])
			continue;
		bad++;
		if (bad <= 4) {
			char m[160];

			snprintf(m, sizeof(m), "%s: byte at +0x%x (case %ld)",
				 what, i, tag);
			diff_eq_int(m, p[i], ob[i], (long)i);
		}
	}
	diff_eq_int(what, bad, 0, tag);
}

static void
poke_int(unsigned off, int v)
{
	memcpy((unsigned char *)&oa + off, &v, sizeof(v));
	memcpy(ob + off, &v, sizeof(v));
}

static void
poke_short(unsigned off, short v)
{
	memcpy((unsigned char *)&oa + off, &v, sizeof(v));
	memcpy(ob + off, &v, sizeof(v));
}

/* Put both registers in a known, identical, non-trivial state. */
static void
seed(unsigned seed_word, short count, int capture)
{
	int i;

	setup();
	for (i = 0; i < 4; i++)
		poke_int(0x2a54 + i * 4, (int)(seed_word * (unsigned)(i + 1)
					       + 0x9e3779b9u * (unsigned)i));
	poke_short(0x2a64, 0x20);
	for (i = 0; i < 3; i++)
		poke_int(0xe74 + i * 4, (int)(seed_word ^ (0x5bd1e995u
							   * (unsigned)i)));
	poke_short(0xe80, count);
	poke_short(0x2214, (short)capture);
	poke_int(0x114, 0);
	poke_int(0x21c, 0);
	poke_int(0x218, 0x40);
	for (i = 0; i < 0x40; i++)
		poke_int(0x118 + i * 4, (int)(seed_word + (unsigned)i * 7919u));
}

int
main(void)
{
	int rc = 0;
	unsigned s;
	int i;

	diff_begin("v34 scrambler: GPC and GPA over 64 steps");
	{
		static const unsigned seeds[] = {
			0, 0xffffffffu, 0x00000001u, 0x80000000u,
			0x12345678u, 0xdeadbeefu, 0xa5a5a5a5u
		};

		for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++)
		for (i = 0; i <= 1; i++) {
			int cap;

			for (cap = 0; cap <= 1; cap++) {
				int step;

				seed(seeds[s], 0, cap);
				for (step = 0; step < 64; step++) {
					short ga, gb;

					if (i == 0) {
						ga = scrambleGPC(&oa, 256);
						gb = ref_scrambleGPC(ob, 256);
					} else {
						ga = scrambleGPA(&oa, 256);
						gb = ref_scrambleGPA(ob, 256);
					}
					diff_eq_int("scramble return", ga, gb,
						    step);
					compare(i == 0 ? "scrambleGPC"
						       : "scrambleGPA",
						(long)s * 1000 + i * 100
						+ cap * 50 + step);
				}
			}
		}
	}
	rc |= diff_end();

	diff_begin("v34 scrambler: the source runs out mid-run");
	{
		/*
		 * Capture on with a short source: the first calls take a
		 * scripted word and the rest fall back to 0xffff, which is a
		 * separate test in the object rather than a stall.  A
		 * NEGATIVE length must exhaust it at once -- the comparison
		 * is unsigned.
		 */
		static const int lens[] = { 0, 1, 3, 0x40, -1, 0x7fffffff };
		unsigned li;

		for (li = 0; li < sizeof(lens) / sizeof(lens[0]); li++)
		for (i = 0; i <= 1; i++) {
			int step;

			seed(0x13579bdfu, 0, 1);
			poke_int(0x218, lens[li]);
			for (step = 0; step < 12; step++) {
				if (i == 0) {
					scrambleGPC(&oa, 64);
					ref_scrambleGPC(ob, 64);
				} else {
					scrambleGPA(&oa, 64);
					ref_scrambleGPA(ob, 64);
				}
				compare("scramble, source bounded",
					(long)li * 100 + i * 50 + step);
			}
		}
	}
	rc |= diff_end();

	diff_begin("v34 descrambler: across the 31-bit flush");
	{
		/*
		 * Nothing happens until the register passes 31 bits, so the
		 * runs have to cross that boundary repeatedly -- and with
		 * several `nbits`, because the tail's shift is computed from
		 * the count BEFORE this call's bits were added.
		 */
		static const unsigned short widths[] = { 1, 4, 8, 15, 16 };
		unsigned wi;

		for (wi = 0; wi < sizeof(widths) / sizeof(widths[0]); wi++)
		for (i = 0; i <= 1; i++) {
			int cap;

			for (cap = 0; cap <= 1; cap++) {
				int step;

				seed(0x2468ace0u, 0, cap);
				for (step = 0; step < 80; step++) {
					unsigned short b =
						(unsigned short)(step * 5779);
					int ra, rb;

					if (i == 0) {
						ra = descrambleGPC(&oa, b,
								   widths[wi]);
						rb = ref_descrambleGPC(ob, b,
								   widths[wi]);
					} else {
						ra = descrambleGPA(&oa, b,
								   widths[wi]);
						rb = ref_descrambleGPA(ob, b,
								   widths[wi]);
					}
					diff_eq_int("descramble return", ra,
						    rb, step);
					compare(i == 0 ? "descrambleGPC"
						       : "descrambleGPA",
						(long)wi * 1000 + i * 100
						+ cap * 50 + step);
				}
			}
		}

		/*
		 * The sink must actually have filled, or the capture path is
		 * being compared empty on both sides.
		 */
		diff_eq_int("the sink filled", oa.scram_sink_n > 0, 1, 0);
	}
	rc |= diff_end();

	diff_begin("v34 descrambler: the sink's 64-entry bound");
	{
		/*
		 * The bound is checked before the store, so a full sink drops
		 * the word.  Driven well past it.
		 */
		int step;

		seed(0x0f0f0f0fu, 0, 1);
		for (step = 0; step < 400; step++) {
			descrambleGPC(&oa, (unsigned short)(step * 37), 16);
			ref_descrambleGPC(ob, (unsigned short)(step * 37), 16);
			compare("sink bound", step);
		}
		diff_eq_int("the sink stopped at 64", oa.scram_sink_n, 0x40, 0);
	}
	rc |= diff_end();

	diff_begin("v34 scrambler: a starting count that is already large");
	{
		/*
		 * `count` is a short and the flush subtracts 16 from it, so a
		 * run started near the top exercises the arithmetic the
		 * ordinary runs never reach.
		 */
		static const short counts[] = { 0, 15, 16, 30, 31, 32, 100 };
		unsigned ci;

		for (ci = 0; ci < sizeof(counts) / sizeof(counts[0]); ci++)
		for (i = 0; i <= 1; i++) {
			int step;

			seed(0xcafebabeu, counts[ci], 1);
			for (step = 0; step < 24; step++) {
				if (i == 0) {
					descrambleGPC(&oa, 0xabcd, 8);
					ref_descrambleGPC(ob, 0xabcd, 8);
				} else {
					descrambleGPA(&oa, 0xabcd, 8);
					ref_descrambleGPA(ob, 0xabcd, 8);
				}
				compare("descramble, high count",
					(long)ci * 100 + i * 50 + step);
			}
		}
	}
	rc |= diff_end();

	return rc;
}

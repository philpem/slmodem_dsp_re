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
#include "dsplib/v34digital.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34scram.h"
#include "dsplib/v34shell.h"

extern short ref_scrambleGPC(void *obj, short nbits);
extern short ref_scrambleGPA(void *obj, short nbits);
extern int ref_descrambleGPC(void *obj, unsigned short b, unsigned short n);
extern int ref_descrambleGPA(void *obj, unsigned short b, unsigned short n);
extern void ref_preinitdigital(void *obj);

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

/*
 * ===========================================================================
 * ITU-T V.34 CLAUSE 7, independently of dsplibs.o.
 *
 * Equations 7-1 and 7-2 define the call-mode polynomial as taps (18, 23)
 * and the answer-mode polynomial as taps (5, 23).  The model below is the
 * Recommendation's bit-serial division/multiplication, deliberately not the
 * production code's four-word, sixteen-bit-at-once identities.  Input bit 0
 * is processed first; after each production call the newest sixteen quotient
 * coefficients are the high half of scrambler.w[3].
 *
 * Each implementation gets its own group.  The receiver is driven from an
 * independently chosen scrambled stream rather than from either production
 * scrambler, so matching defects in a scrambler/descrambler pair cannot close
 * into a false-positive round trip.
 * ===========================================================================
 */

typedef short (*std_scramble_fn)(void *, short);
typedef int (*std_descramble_fn)(void *, unsigned short, unsigned short);
typedef void (*std_init_fn)(void *);

struct std_subject {
	const char *group;
	void *obj;
	std_scramble_fn gpc;
	std_scramble_fn gpa;
	std_descramble_fn dgpc;
	std_descramble_fn dgpa;
	std_init_fn init;
};

struct serial_model {
	unsigned char history[128];
};

/* Divide when `scramble` is set; multiply when it is clear. */
static unsigned short
serial_word(struct serial_model *m, unsigned short word, unsigned near_tap,
	    int scramble)
{
	unsigned short out = 0;
	unsigned bit;

	for (bit = 0; bit < 16; bit++) {
		unsigned in = (word >> bit) & 1u;
		unsigned v = in ^ m->history[near_tap - 1u]
				^ m->history[23u - 1u];
		unsigned i;

		out |= (unsigned short)(v << bit);
		for (i = sizeof(m->history) - 1u; i > 0; i--)
			m->history[i] = m->history[i - 1u];
		m->history[0] = (unsigned char)(scramble ? v : in);
	}
	return out;
}

static unsigned short
standard_word(unsigned step)
{
	static const unsigned short edge[] = {
		0x0000, 0xffff, 0x0001, 0x8000, 0x1234, 0xa5a5
	};
	unsigned x;

	if (step < sizeof(edge) / sizeof(edge[0]))
		return edge[step];
	x = 0x9e3779b9u * (step + 1u) ^ 0x5bd1e995u;
	x ^= x >> 13;
	return (unsigned short)x;
}

/*
 * Fixed zero-history known-answer vectors.  These are transcribed constants,
 * not results from serial_word(): they independently pin the Recommendation's
 * tap numbering and least-significant-bit-first wire order.  The second input
 * is a single one bit followed by seven zero words.
 */
static const unsigned short standard_kat[2][2][8] = {
	{
		{ 0xffff, 0xff83, 0xc00f, 0xf83f,
		  0x00e0, 0x8c00, 0xcfff, 0x3fc7 },
		{ 0x0001, 0x0084, 0x4010, 0x0840,
		  0x0121, 0x9400, 0x5000, 0x4048 }
	},
	{
		{ 0x7c1f, 0x3e70, 0x2706, 0x7064,
		  0xf9a2, 0x95d8, 0xa5b1, 0xc701 },
		{ 0x8421, 0x4290, 0x690a, 0x90ac,
		  0x0ae6, 0xbe69, 0xeed2, 0x4902 }
	}
};

static int
run_standard_subject(const struct std_subject *s)
{
	struct v34_object *obj = (struct v34_object *)s->obj;
	unsigned poly;

	diff_begin(s->group);

	/* Clause-7 known answers, checked without consulting the scalar model. */
	for (poly = 0; poly < 2; poly++) {
		std_scramble_fn fn = poly ? s->gpa : s->gpc;
		unsigned pattern;

		for (pattern = 0; pattern < 2; pattern++) {
			unsigned step;

			memset(obj, 0, sizeof(*obj));
			obj->data_enable = 1;
			obj->tx_n = 8;
			for (step = 0; step < 8; step++)
				obj->tx_data[step] = pattern
					? (step == 0 ? 1 : 0) : 0xffff;

			for (step = 0; step < 8; step++) {
				long tag = (long)poly * 100L
					   + (long)pattern * 10L + (long)step;

				fn(obj, 16);
				diff_eq_int("fixed clause-7 known answer (%ld)",
					    (obj->scrambler.w[3] >> 16) & 0xffffu,
					    standard_kat[poly][pattern][step], tag);
			}
		}
	}

	/* Transmit: primary data divided by GPC or GPA, 16 bits per call. */
	for (poly = 0; poly < 2; poly++) {
		struct serial_model model;
		std_scramble_fn fn = poly ? s->gpa : s->gpc;
		unsigned near_tap = poly ? 5u : 18u;
		unsigned step;

		memset(obj, 0, sizeof(*obj));
		memset(&model, 0, sizeof(model));
		obj->data_enable = 1;
		obj->tx_n = 32;
		for (step = 0; step < 32; step++)
			obj->tx_data[step] = (int)standard_word(step);

		for (step = 0; step < 32; step++) {
			unsigned short expected = serial_word(&model,
				(unsigned short)obj->tx_data[step], near_tap, 1);
			short left = (short)(200 - (int)step);
			short got = fn(obj, left);
			long tag = (long)poly * 1000L + (long)step;

			diff_eq_int("clause 7 scrambler consumes sixteen bits (%ld)",
				    got, (short)(left - 16), tag);
			diff_eq_int("equation 7 scrambler quotient word (%ld)",
				    (long)((obj->scrambler.w[3] >> 16) & 0xffffu),
				    (long)expected, tag);
			diff_eq_int("standard stream advances once (%ld)",
				    obj->tx_rd, (long)step + 1L, tag);
		}
	}

	/*
	 * Receive: multiplication by the polynomial.  The word interface keeps
	 * one sixteen-bit word buffered, so call n emits call n-1; the final zero
	 * word flushes the 32nd independently modelled result.
	 */
	for (poly = 0; poly < 2; poly++) {
		struct serial_model model;
		unsigned short expected[32];
		std_descramble_fn fn = poly ? s->dgpa : s->dgpc;
		unsigned near_tap = poly ? 5u : 18u;
		unsigned step;

		memset(obj, 0, sizeof(*obj));
		memset(&model, 0, sizeof(model));
		obj->data_enable = 1;
		for (step = 0; step < 32; step++)
			expected[step] = serial_word(&model, standard_word(step),
						     near_tap, 0);

		for (step = 0; step <= 32; step++) {
			unsigned short in = step < 32 ? standard_word(step) : 0;
			long tag = 2000L + (long)poly * 1000L + (long)step;

			diff_eq_int("clause 7 descrambler returns zero (%ld)",
				    fn(obj, in, 16), 0, tag);
			diff_eq_int("descrambler emits after one buffered word (%ld)",
				    obj->rx_n, step == 0 ? 0 : (long)step, tag);
			if (step > 0)
				diff_eq_int("equation 7 descrambler product word (%ld)",
					    obj->rx_data[step - 1u],
					    expected[step - 1u], tag);
		}
	}

	/*
	 * Clause 7 assigns GPC to the call-mode transmission and GPA to the
	 * answer-mode transmission.  A station therefore receives with the
	 * OTHER role's polynomial: call is GPC-out/GPA-in, answer GPA-out/GPC-in.
	 */
	for (poly = 0; poly < 2; poly++) {
		int call_mode = poly == 0;
		struct v34_shell *rx;
		struct v34_shell *tx;
		long tag = 4000L + (long)poly;

		memset(obj, 0, sizeof(*obj));
		obj->role = (short)(call_mode ? 0x65 : 0x66);
		s->init(obj);
		rx = (struct v34_shell *)obj;
		tx = (struct v34_shell *)((unsigned char *)obj + V34_SHELL_TX);

		diff_eq_int("call/answer transmit polynomial pairing (%ld)",
			    tx->scramble == (call_mode ? s->gpc : s->gpa), 1,
			    tag);
		diff_eq_int("peer receive polynomial pairing (%ld)",
			    (const void *)rx->put_bits
			    == (const void *)(call_mode ? s->dgpa : s->dgpc), 1,
			    tag);
	}

	return diff_end();
}

static int
run_standards_oracle(void)
{
	static const struct std_subject subjects[] = {
		{
			"V.34 clause 7/reconstruction", &oa,
			scrambleGPC, scrambleGPA, descrambleGPC, descrambleGPA,
			preinitdigital
		},
		{
			"V.34 clause 7/blob", ob,
			ref_scrambleGPC, ref_scrambleGPA,
			ref_descrambleGPC, ref_descrambleGPA,
			ref_preinitdigital
		}
	};
	unsigned i;
	int rc = 0;

	for (i = 0; i < sizeof(subjects) / sizeof(subjects[0]); i++)
		rc |= run_standard_subject(&subjects[i]);
	return rc;
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
		diff_eq_int("the sink filled", oa.rx_n > 0, 1, 0);
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
		diff_eq_int("the sink stopped at 64", oa.rx_n, 0x40, 0);
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

	rc |= run_standards_oracle();

	return rc;
}

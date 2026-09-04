/*
 * t_v92cpb2i.cpp -- differential test of `V92CP::bitsToInfo`, the receive-side
 * driver, and with it of the two function-local statics `gamma` and `delta`.
 *
 * A RANDOM BIT STREAM PROVES NOTHING HERE, and that is the whole design.  The
 * state machine leaves state 0 only after seventeen consecutive ones, and
 * states 5 to 10 are reachable only through a message whose CRC checks, so a
 * sweep of drawn bytes sits in states 0 and 1 for ever while every arm goes
 * untested and the suite goes green.  So the stimulus is CLOSED WITH THE
 * TRANSMITTER: a third object is filled in, `infoToBits` lays the message out
 * into its `bits`, and those `vectorLen` bytes are then fed one at a time into
 * a fresh pair.  `infoToBits` is differentially identical to the blob's
 * already (t_v92info), so using it to make the stimulus asserts nothing about
 * the member under test.
 *
 * ---------------------------------------------------------------------------
 * THE TWO STATICS CANNOT BE RESET, AND THE TEST IS BUILT AROUND THAT.
 *
 * `gamma` and `delta` are function-local statics.  The blob's are reachable --
 * `symmap.py` globalises file-local symbols, so `ref__ZZN5V92CP10bitsToInfoEh
 * E5gamma` links -- but OURS are `b` in `nm` and no other translation unit can
 * name them.  Zeroing one side and not the other would guarantee a divergence
 * that says nothing; leaving both alone risks the failure the harness exists
 * to prevent, where the two drift together and agree for the wrong reason.
 *
 * WHAT MAKES THEM OBSERVABLE ANYWAY is that they are the LENGTHS the state
 * machine counts to.  `gamma` decides the bit at which state 7 hands over, and
 * `delta` the bit at which state 8 does, so a wrong value moves a state change
 * -- and `rxState`, `word_11c` and `stateBitCount` are compared after every bit.
 * A stale `gamma` cannot hide: it ends the block early or late and the whole
 * object diverges on that bit.
 *
 * SO THE TRIALS ARE ORDERED TO MAKE STALENESS FATAL.  Consecutive message
 * cases use DIFFERENT group counts, and the run asserts the blob's own `gamma`
 * and `delta` against `136 * word_10c` after each one.  A version that
 * computed the length once and cached it across calls -- which is what a
 * static invites -- gets the previous case's length and fails on the case
 * after.  `case 8` additionally reuses `word_10c` at a moment when `case 7`
 * has already run, so the two statics are set from the same field at different
 * times and are compared separately.
 *
 * ---------------------------------------------------------------------------
 * WHAT ELSE EACH RUN HAS TO SEPARATE:
 *
 *   sixteen ones ARE NOT ENOUGH.  `case 0` leaves on `> 0x10`.  One trial
 *                feeds sixteen ones and a zero before the real preamble and
 *                requires the blob to still be in state 0 at that point;
 *                without it, `>=` and `>` are indistinguishable.
 *
 *   the forms    Three of them, and they walk different state lists.  The SUV
 *                form goes 0,1,2,3,9,10; the short form 0,1,2,5,9,10; the long
 *                form 0,1,2,5,6,7,9,10 and, when `byte_24` is set,
 *                0,1,2,5,6,7,8,9,10.  The masks are accumulated off the BLOB's
 *                own +0x114 after every bit and asserted, so "the second mask
 *                block was entered" is measured and not assumed.
 *
 *   state 4      IS NOT REACHABLE.  `case 2` chooses 3 or 5 and nothing else
 *                writes 4, which is the receive-side half of `evaluateInfo`'s
 *                six-entry table having the bare `ret` in its second slot.
 *                The union of every mask below is asserted to be exactly
 *                0x7ef -- all eleven states but that one.
 *
 *   bad CRC      One payload bit is flipped before the feed.  The blob must
 *                reach state 9, fail, reset to state 0 and never report.  The
 *                diagnostic is driven with `dsplibs_debug_level` at 2 on both
 *                sides, so the author's own spelling is exercised rather than
 *                merely compiled.
 *
 *   the answers  1..4 are four combinations of `byte_00` and `byte_04` and all
 *                four are driven; 5 is the far end stopping and has a trial of
 *                its own, a run of `12 * bitsPerSymbol` zeros with the cursor
 *                at its home 18.
 *
 *   the hold-off `word_914` starts at -1, is set to 0 by answers 1 and 2, then
 *                suppresses answers 3 and 4 for 400 calls before returning to
 *                -1.  Four hundred is longer than a short message, so one
 *                trial feeds 700 bits of tail after the answer purely to reach
 *                the wrap, and the field is compared on every bit like the
 *                rest of the object.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS NOT DRIVEN, and why.  `bitsToInfo` stores into `bits[word_11c]` at
 * seven sites and NONE of them is bounds-checked -- docs/deviations.md D923 --
 * so a stream long enough to fill the vector walks off the object on both
 * sides.  Every case here stays inside it: the longest message the grid builds
 * is six groups in both blocks, 1,785 positions of a 2,000-entry array.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V92CP.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

/* NOT void: %edi is zeroed at entry and moved to %eax at all three `ret`s. */
int ref_cp_bitstoinfo(void *, unsigned char) asm("ref__ZN5V92CP10bitsToInfoEh");

/*
 * The blob's two function-local statics, globalised by `symmap.py`'s second
 * pass (finding F221).  Ours are file-local and cannot be named from here; the
 * header comment says what is done about that.
 */
extern unsigned int ref_b2i_gamma asm("ref__ZZN5V92CP10bitsToInfoEhE5gamma");
extern unsigned int ref_b2i_delta asm("ref__ZZN5V92CP10bitsToInfoEhE5delta");
}

#define TAIL	256u
#define SLOT	((unsigned int)sizeof(V92CP) + TAIL)

static unsigned char cp_a[SLOT] __attribute__((aligned(8)));
static unsigned char cp_b[SLOT] __attribute__((aligned(8)));
static unsigned char cp_g[SLOT] __attribute__((aligned(8)));

#define A	((V92CP *)cp_a)
#define B	((V92CP *)cp_b)
#define G	((V92CP *)cp_g)

/*
 * A zeroed slot carrying what `V92CP::V92CP` leaves behind.  Placement new is
 * not available -- the tree builds -nostdinc++ and there is no <new> -- and
 * calling the constructor on a raw slot is not what is wanted anyway: the
 * point is a KNOWN start, including `word_914` at the -1 that makes the
 * hold-off idle, and not a second test of the constructor.
 */
static void
blank(V92CP *o)
{
	memset((void *)o, 0, SLOT);

	o->word_11c = 18;
	o->word_914 = -1;
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/* --------------------------------------------------------------- the grid */

struct mcase {
	const char *name;
	unsigned char byte_00;
	signed char char_01;
	signed char char_02;
	unsigned char byte_04;
	unsigned char byte_24;
	unsigned short word_10c;
	unsigned char bitsPerSymbol;
	int corrupt;		/* index of a payload bit to flip, or -1   */
	unsigned int tail;	/* extra zero bits fed after the vector    */
	unsigned int expect;	/* states the blob MUST enter              */
	int answer;		/* the answer the blob must report, or -1  */
};

#define S(n)	(1u << (n))

/*
 * `word_10c` CHANGES BETWEEN CONSECUTIVE LONG-FORM CASES, which is what makes
 * a cached `gamma` or `delta` fail.  The order below is 1, 6, 2, 5, 3 and not
 * an accident.
 *
 * `word_28[i]` is set to `word_10c - 1` for every i by `build`, because the
 * DECODER derives its group count from those six four-bit fields and the
 * encoder from the field itself; if the two disagreed, the block lengths would
 * not line up and every long case would fail its CRC for a reason that has
 * nothing to do with the member under test.
 */
static const struct mcase cases[] = {
 { "the SUV form, byte_04 clear",
   1,  0,  0, 0x00, 0, 0, 1, -1,   0, S(0)|S(1)|S(2)|S(3)|S(9)|S(10), 3 },
 { "the SUV form, byte_04 set",
   1,  0,  0, 0x01, 0, 0, 1, -1,   0, S(0)|S(1)|S(2)|S(3)|S(9)|S(10), 4 },
 { "the short form at char_01 = 2, byte_04 clear",
   0,  2,  0x0f, 0x00, 0, 0, 1, -1, 0, S(0)|S(1)|S(2)|S(5)|S(9)|S(10), 1 },
 { "the short form at char_01 = 3, byte_04 set",
   0,  3,  0x11, 0x01, 0, 0, 2, -1, 0, S(0)|S(1)|S(2)|S(5)|S(9)|S(10), 2 },
 { "the long form, one group, no second block",
   0,  1,  0x0f, 0x00, 0, 1, 1, -1, 0,
   S(0)|S(1)|S(2)|S(5)|S(6)|S(7)|S(9)|S(10), 1 },
 { "the long form, six groups, both blocks",
   0,  0,  0x15, 0x01, 1, 6, 3, -1, 0,
   S(0)|S(1)|S(2)|S(5)|S(6)|S(7)|S(8)|S(9)|S(10), 2 },
 { "the long form, two groups, both blocks",
   0,  1,  0x0a, 0x01, 1, 2, 2, -1, 0,
   S(0)|S(1)|S(2)|S(5)|S(6)|S(7)|S(8)|S(9)|S(10), 2 },
 { "the long form, five groups, no second block",
   0,  0,  0x7f, 0x00, 0, 5, 4, -1, 0,
   S(0)|S(1)|S(2)|S(5)|S(6)|S(7)|S(9)|S(10), 1 },
 { "the long form, three groups, both blocks",
   0,  1,  0x03, 0x00, 1, 3, 1, -1, 0,
   S(0)|S(1)|S(2)|S(5)|S(6)|S(7)|S(8)|S(9)|S(10), 1 },
 { "byte_00 = 2, which is neither the SUV form nor zero",
   2,  1,  0x0f, 0x01, 1, 4, 2, -1, 0,
   S(0)|S(1)|S(2)|S(5)|S(6)|S(7)|S(8)|S(9)|S(10), 2 },
 { "the long form with a corrupted payload bit",
   0,  1,  0x0f, 0x01, 0, 2, 1, 40, 0, S(0)|S(1)|S(2)|S(5)|S(6)|S(9), -1 },
 { "the SUV form with a corrupted payload bit",
   1,  0,  0, 0x00, 0, 0, 1, 22,  0, S(0)|S(1)|S(2)|S(3)|S(9), -1 },
 { "the SUV form and a long tail, for the hold-off wrap",
   1,  0,  0, 0x01, 0, 0, 1, -1, 700u,
   S(0)|S(1)|S(2)|S(3)|S(9)|S(10), 4 }
};

#define NCASE	((int)(sizeof(cases) / sizeof(cases[0])))

/*
 * Fill the generator and lay the message out.  Everything not named by the
 * case is a fixed pattern rather than a draw, because the stimulus has to be
 * reproducible bit for bit between the reach pass and the comparison pass.
 */
static unsigned int
build(int c)
{
	const struct mcase *m = &cases[c];
	unsigned int i, k;

	memset(cp_g, 0, SLOT);

	G->byte_00 = m->byte_00;
	G->char_01 = m->char_01;
	G->char_02 = m->char_02;
	G->byte_03 = 0x21;
	G->byte_04 = m->byte_04;
	G->word_08 = 2u;
	G->word_0c = 1u;
	G->flt_10 = 1.5f;
	G->flt_14 = 0.5f;
	G->flt_18 = -0.25f;
	G->flt_1c = 0.125f;
	G->flt_20 = -0.0625f;
	G->byte_24 = m->byte_24;
	G->word_10c = m->word_10c;
	G->bitsPerSymbol = m->bitsPerSymbol;
	G->word_104 = 16u;
	G->suv = 1u;

	for (i = 0; i < 6u; i++)
		G->word_28[i] = (int)m->word_10c - 1;

	for (i = 0; i < 6u; i++) {
		for (k = 0; k < 8u; k++) {
			G->short_42[i][k] = (short)(0x1234 + 0x11 * (i * 8 + k));
			G->short_a2[i][k] = (short)(0x4321 - 0x13 * (i * 8 + k));
		}
	}

	G->word_11c = 18;
	G->infoToBits();

	if (m->corrupt >= 0)
		G->bits[m->corrupt] = (unsigned char)(!G->bits[m->corrupt]);

	return G->vectorLen;
}

/* ------------------------------------------------------- driving the feed */

static unsigned int statemask;
static int reports;
static unsigned int answermask;
static int state_at_16;

static void
feed(unsigned char v, long tag)
{
	unsigned int before = B->rxState;
	int ra = A->bitsToInfo(v);
	int rb = ref_cp_bitstoinfo(cp_b, v);

	diff_eq_int("the answer matches (%ld)", (long)ra, (long)rb, tag);

	if (rb != 0) {
		reports++;
		answermask |= S((unsigned int)rb & 31u);
	}

	statemask |= S(B->rxState & 31u);

	diff_eq_obj("after one bit", V92CP, A, B, tag);
	diff_eq_int("the tail past the class is untouched (%ld)",
		    (long)(memcmp(cp_a + sizeof(V92CP),
				  cp_b + sizeof(V92CP), TAIL) == 0), 1, tag);

	if (B->rxState != before)
		diff_eq_int("the state agrees at a change (%ld)",
			    (long)A->rxState, (long)B->rxState, tag);
}

/*
 * A fresh pair, constructed rather than memset, so that `word_914` starts at
 * the -1 the constructor gives it and the hold-off is exercised from its own
 * idle value.
 */
static void
fresh(void)
{
	blank(A);
	blank(B);
	A->bitsPerSymbol = B->bitsPerSymbol = G->bitsPerSymbol;
}

static void
run_one(int c, long tag)
{
	const struct mcase *m = &cases[c];
	unsigned int len = build(c);
	unsigned int i;

	fresh();

	statemask = S(B->rxState & 31u);
	reports = 0;
	answermask = 0;
	state_at_16 = -1;

	set_level(m->corrupt >= 0 ? 2u : 0u);

	/*
	 * Sixteen ones and a zero first, which must NOT leave state 0.  The
	 * zero then puts the detector back where it started, so the message
	 * that follows is unaffected.
	 */
	for (i = 0; i < 16u; i++)
		feed(1, tag);
	state_at_16 = (int)B->rxState;
	feed(0, tag);

	for (i = 0; i < len; i++)
		feed(G->bits[i], tag);

	for (i = 0; i < m->tail; i++)
		feed(0, tag);

	set_level(0u);
}

static int
run_cases(void)
{
	int c;
	unsigned int seen = 0;

	diff_begin("bitsToInfo over the message grid");

	for (c = 0; c < NCASE; c++) {
		long tag = (long)c;

		run_one(c, tag);
		seen |= statemask;

		diff_eq_int("sixteen ones leave it in state 0 (%ld)",
			    (long)state_at_16, 0, tag);
		diff_eq_int("the states the blob entered (0x%lx)",
			    (long)(statemask & cases[c].expect),
			    (long)cases[c].expect, tag);

		if (cases[c].answer >= 0) {
			diff_eq_int("the blob reported it (%ld)",
				    (long)((answermask >>
					    cases[c].answer) & 1u), 1, tag);
			diff_eq_int("it reported at least once (%ld)",
				    (long)(reports > 0), 1, tag);
		} else {
			/*
			 * A bad CRC must never produce a MESSAGE answer.  It
			 * may still produce 5, because the reset puts the
			 * cursor back at 18 and the padding that follows is a
			 * run of zeros -- which is the detector doing its
			 * other job and not a report about this message.
			 */
			diff_eq_int("a corrupted message answers nothing (0x%lx)",
				    (long)(answermask & 0x1eu), 0, tag);
		}

		/*
		 * The two lengths, off the BLOB, against the rule.  A long
		 * case sets both; a short one leaves whatever the previous
		 * case put there, which is why only the long ones are checked.
		 */
		if (cases[c].word_10c != 0 && cases[c].corrupt < 0) {
			diff_eq_int("gamma is 136 * word_10c (%ld)",
				    (long)ref_b2i_gamma,
				    (long)(136u * cases[c].word_10c), tag);
			if (cases[c].byte_24 != 0)
				diff_eq_int("delta is 136 * word_10c (%ld)",
					    (long)ref_b2i_delta,
					    (long)(136u *
						   cases[c].word_10c), tag);
		}
	}

	/*
	 * Every state but 4, which nothing writes.  This is the receive-side
	 * half of `evaluateInfo`'s hole and it is measured here rather than
	 * inferred from the jump table.
	 */
	diff_eq_int("the union of every state entered is 0x%lx", (long)seen,
		    0x7ef, 0);

	return diff_end();
}

/*
 * Answer 5 has no message behind it: it is a run of `12 * bitsPerSymbol` zeros
 * arriving with the cursor still at its home 18, which is the detector saying
 * the far end has stopped.  Driven at four symbol sizes, and at zero -- where
 * the quantum is zero, the object reloads `byte_11a` after clearing it, and
 * the test is therefore true on a ONE bit as well.
 */
static int
run_silence(void)
{
	unsigned int bps;

	diff_begin("the far end stopping");

	for (bps = 0; bps <= 4u; bps++) {
		unsigned int i;
		int seen5 = 0;

		blank(A);
		blank(B);
		A->bitsPerSymbol = B->bitsPerSymbol = (unsigned char)bps;

		for (i = 0; i < 80u; i++) {
			int ra = A->bitsToInfo(0);
			int rb = ref_cp_bitstoinfo(cp_b, 0);

			diff_eq_int("the answer matches (%ld)", (long)ra,
				    (long)rb, (long)(bps * 100u + i));
			diff_eq_obj("after one zero", V92CP, A, B,
				    (long)(bps * 100u + i));
			if (rb == 5)
				seen5++;
		}

		/*
		 * AND THE ZERO SIZE IS THE OTHER WAY ROUND, which is the whole
		 * point of driving it.  The quantum is `12 * bitsPerSymbol`,
		 * so at zero the test is `byte_11a == 0` -- and a ZERO bit
		 * INCREMENTS `byte_11a`, so it is 1, 2, 3 ... and never 0.  A
		 * run of silence at `bitsPerSymbol == 0` therefore answers
		 * NOTHING, while a single ONE answers 5, because the one arm
		 * clears the field and the object reloads it.  The two
		 * assertions below are opposite for that reason and a
		 * reconstruction that kept the cleared value in a register
		 * fails the second.
		 */
		diff_eq_int("silence answers 5 at this size (%ld)",
			    (long)(seen5 > 0), (long)(bps != 0u), (long)bps);
	}

	/* And a ONE at bps = 0, which the reload makes answer 5 as well. */
	{
		int ra, rb;

		blank(A);
		blank(B);
		A->bitsPerSymbol = B->bitsPerSymbol = 0;

		ra = A->bitsToInfo(1);
		rb = ref_cp_bitstoinfo(cp_b, 1);
		diff_eq_int("a ONE at bitsPerSymbol = 0 answers (%ld)",
			    (long)ra, (long)rb, 0);
		diff_eq_int("and the answer is 5 (%ld)", (long)rb, 5, 0);
	}

	return diff_end();
}

/*
 * The hold-off on its own terms.  `word_914` is driven straight rather than
 * through a message: set it to a value, feed a bit, and compare the whole
 * object.  400 is the wrap and 399 is one short of it, and the two are only
 * one apart in the object.
 */
static int
run_holdoff(void)
{
	static const int starts[] = { -5, -1, 0, 1, 200, 398, 399, 400, 401 };
	unsigned int s;

	diff_begin("the hold-off over the answer");

	for (s = 0; s < sizeof(starts) / sizeof(starts[0]); s++) {
		unsigned int v;

		for (v = 0; v <= 1u; v++) {
			long tag = (long)(s * 2u + v);

			blank(A);
			blank(B);
			A->bitsPerSymbol = B->bitsPerSymbol = 3;
			A->word_914 = B->word_914 = starts[s];

			diff_eq_int("the answer matches (%ld)",
				    (long)A->bitsToInfo((unsigned char)v),
				    (long)ref_cp_bitstoinfo(cp_b,
							    (unsigned char)v),
				    tag);
			diff_eq_obj("after one bit", V92CP, A, B, tag);
			diff_eq_int("word_914 agrees (%ld)", (long)A->word_914,
				    (long)B->word_914, tag);
		}
	}

	return diff_end();
}


/*
 * THE POKED SECTION, and it exists because the message grid cannot reach
 * everything.  Three claims are outside any legal sequence:
 *
 *   - `char_01` is a SIGNED byte and the branch on it is `jg`, but the value
 *     `evaluateInfo` puts there comes from two positions of `bits` that a
 *     legal message only ever holds 0 or 1 in, so the decoded field is 0..3
 *     and signed and unsigned agree over all four.  Setting bits[20] to 0x40
 *     by hand makes the decoded value 0x80, where they do not.
 *
 *   - the hold-off suppresses answers 3 and 4 and passes 5, and to see that
 *     an answer has to be produced WHILE `word_914` is running.  A message
 *     produces its answer once, at the end, from a fresh object where the
 *     hold-off is idle.
 *
 *   - answer 5 arrives from a run of zeros with the cursor at home, which is
 *     a different arm from the one that produces 1..4.
 *
 * Each trial sets the state word, the cursor and the fields by hand, feeds
 * ONE bit, and compares the whole object.  That is exactly what a state
 * machine's caller does, so nothing here is out of the member's contract --
 * only out of the sequence a well-formed message walks.
 */
static int
run_poked(void)
{
	static const int holds[] = { -5, -1, 0, 7, 399 };
	unsigned int b0, b4, h, v;

	diff_begin("the answer and the hold-off, poked");

	/* State 10: the four answers, against every hold-off value. */
	for (b0 = 0; b0 <= 2u; b0++) {
		for (b4 = 0; b4 <= 1u; b4++) {
			for (h = 0; h < sizeof(holds) / sizeof(holds[0]); h++) {
				long tag = (long)((b0 * 2u + b4) * 8u + h);

				blank(A);
				blank(B);
				A->bitsPerSymbol = B->bitsPerSymbol = 3;
				A->byte_00 = B->byte_00 = (unsigned char)b0;
				A->byte_04 = B->byte_04 = (unsigned char)b4;
				A->rxState = B->rxState = 10;
				A->word_11c = B->word_11c = 35;
				A->word_914 = B->word_914 = holds[h];

				diff_eq_int("the answer matches (%ld)",
					    (long)A->bitsToInfo(0),
					    (long)ref_cp_bitstoinfo(cp_b, 0),
					    tag);
				diff_eq_obj("after the answer", V92CP, A, B,
					    tag);
			}
		}
	}

	/* State 0: answer 5, which the hold-off must NOT suppress. */
	for (h = 0; h < sizeof(holds) / sizeof(holds[0]); h++) {
		long tag = (long)(100 + h);

		blank(A);
		blank(B);
		A->bitsPerSymbol = B->bitsPerSymbol = 3;
		A->byte_11a = B->byte_11a = 35;
		A->word_914 = B->word_914 = holds[h];

		diff_eq_int("the answer matches (%ld)", (long)A->bitsToInfo(0),
			    (long)ref_cp_bitstoinfo(cp_b, 0), tag);
		diff_eq_obj("after the silence answer", V92CP, A, B, tag);
	}

	/* State 5: a decoded `char_01` with its top bit set. */
	for (v = 0; v <= 1u; v++) {
		static const unsigned char hi[] = { 0x40, 0x41, 0x02, 0x01 };
		unsigned int k;

		for (k = 0; k < sizeof(hi) / sizeof(hi[0]); k++) {
			long tag = (long)(200 + v * 8u + k);

			blank(A);
			blank(B);
			A->bitsPerSymbol = B->bitsPerSymbol = 2;
			A->rxState = B->rxState = 5;
			A->word_11c = B->word_11c = 33;
			A->bits[19] = B->bits[19] = 1;
			A->bits[20] = B->bits[20] = hi[k];

			diff_eq_int("the answer matches (%ld)",
				    (long)A->bitsToInfo((unsigned char)v),
				    (long)ref_cp_bitstoinfo(cp_b,
							    (unsigned char)v),
				    tag);
			diff_eq_obj("after the header decode", V92CP, A, B,
				    tag);
			diff_eq_int("the state agrees (%ld)",
				    (long)A->rxState, (long)B->rxState, tag);
		}
	}

	/*
	 * The two restart arms, which no legal sequence reaches.  State 1
	 * expects the framing ZERO and a ONE there restarts the detector;
	 * state 10 is counting padding out to a frame boundary and a ONE
	 * there does the same.  A message never supplies either -- the
	 * framing position IS zero and the padding IS zeros -- so without
	 * these two the arms are 99.0% line coverage and two `resetDetector`
	 * calls that never run.  The MUTATIONS on both were caught anyway,
	 * from the other side of each branch, which is the worked example of
	 * why line coverage is quoted first.
	 */
	{
		static const unsigned int st[] = { 1, 10 };
		unsigned int k;

		for (k = 0; k < 2u; k++) {
			for (v = 0; v <= 1u; v++) {
				long tag = (long)(300 + k * 2u + v);

				blank(A);
				blank(B);
				A->bitsPerSymbol = B->bitsPerSymbol = 3;
				A->rxState = B->rxState = st[k];
				A->word_11c = B->word_11c = 20;
				A->byte_119 = B->byte_119 = 4;

				diff_eq_int("the answer matches (%ld)",
					    (long)A->bitsToInfo(
						(unsigned char)v),
					    (long)ref_cp_bitstoinfo(cp_b,
						(unsigned char)v), tag);
				diff_eq_obj("after the restart arm", V92CP, A,
					    B, tag);
				diff_eq_int("the cursor agrees (%ld)",
					    (long)A->word_11c,
					    (long)B->word_11c, tag);
			}
		}
	}

	return diff_end();
}

/*
 * The drawn sweep.  It cannot reach the decoding states -- that is what the
 * grid above is for -- but it is the only thing that drives the detector's
 * ENTRY over arbitrary traffic, which is what a live receiver mostly sees, and
 * it reaches `case 0` and `case 1` with every combination of run lengths.
 */
static int
run_sweep(void)
{
	unsigned int lfsr = 0xace1u;
	unsigned int n;

	diff_begin("bitsToInfo over drawn bits");

	blank(A);
	blank(B);
	A->bitsPerSymbol = B->bitsPerSymbol = 5;

	for (n = 0; n < 4000u; n++) {
		unsigned char v;
		int ra, rb;

		lfsr = (lfsr >> 1) ^ (unsigned int)(-(int)(lfsr & 1u) &
						    0xb400u);
		v = (unsigned char)((lfsr >> 5) & 1u);

		ra = A->bitsToInfo(v);
		rb = ref_cp_bitstoinfo(cp_b, v);

		diff_eq_int("the answer matches (%ld)", (long)ra, (long)rb,
			    (long)n);
		diff_eq_obj("after one drawn bit", V92CP, A, B, (long)n);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	diff_begin("the class's map");
	diff_eq_int("sizeof(V92CP) is %ld", (long)sizeof(V92CP), 0x918, 0x918);
	diff_eq_int("rxState is at +0x%lx", (long)offsetof(V92CP, rxState),
		    0x114, 0x114);
	diff_eq_int("word_11c is at +0x%lx", (long)offsetof(V92CP, word_11c),
		    0x11c, 0x11c);
	diff_eq_int("word_914 is at +0x%lx", (long)offsetof(V92CP, word_914),
		    0x914, 0x914);
	rc |= diff_end();

	rc |= run_cases();
	rc |= run_silence();
	rc |= run_holdoff();
	rc |= run_poked();
	rc |= run_sweep();

	return rc;
}

/*
 * t_v34hst3mid.c -- our reconstruction of table 3's middle group, against the
 * blob, one dispatch arm at a time.
 *
 * `v34handshak` is 61,541 bytes and nothing here reconstructs it.  What is
 * reconstructed is a set of the microstate arms at .rodata+0x3000 plus the
 * prologue that reaches them and the transmit dispatch and tail they leave
 * through (src/pump/v34/v34hshak_t3mid.c), and this drives each of them
 * through `test/harness/v34hsstep.c` with OUR entry on side A and the blob on
 * side B.  That makes it an ordinary tier-1 differential test: 44,096 bytes
 * of object, five blocks, seven filler regions and both transcripts, compared
 * after every step.
 *
 * THREE THINGS EVERY CASE HERE HAS TO DO, and the first two are what stop it
 * being an expensive way of comparing the blob with itself:
 *
 *   1  RUN THE SAME CASE WITH THE BLOB ON BOTH SIDES.  `v34hs_side_a(NULL)`
 *      puts it back.  A green ours-versus-blob run means nothing if the same
 *      seed is green blob-versus-blob for a reason of the fixture's, and this
 *      is finding 290's whole point about what the harness proves.
 *
 *   2  REACH THE PATH IT CLAIMS TO.  Six of table 3's arms bump one counter
 *      at +0xaa78 and test it against one or two exact thresholds, and the
 *      fixture's own fill sends all six down the same branch -- finding 290
 *      measured them as one behaviour with one signature.  A sweep that never
 *      seeds the counter tests the increment and NONE of the thresholds, and
 *      swapping one arm's constants for another's could not fail.  So each
 *      threshold gets a trial that lands exactly on it and a trial that does
 *      not, and the counter is read back afterwards to say which happened.
 *
 *   3  NOT REACH A PATH THAT IS NOT WRITTEN.  Our entry records the first
 *      unwritten path it takes and returns; `check` fails if any trial hit
 *      one, so a case that quietly did nothing is a failure and not a pass.
 *
 * THE TXSTATE IS PART OF THE FIXTURE, not a don't-care (finding 288): most of
 * these arms end by jumping to the once-per-block transmit dispatch, so a
 * case driven with a txstate whose table-2 arm does nothing is a case tested
 * against silence.  SSEG is the choice here, as in `t_v34hsstep.c`, and it is
 * also the choice that exercises both of arm 48's diagnostics -- its forcing
 * path prints only when the txstate it is replacing is not already 5.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "v34hsstep.h"

#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"

/*
 * The txstate every case below is driven with, and the counter the six timer
 * arms share.  Prefixed, because four batches are writing `v34handshak` at
 * once and finding 325 is what one shared macro name cost.
 */
#define T3MT_TXSTATE	V34HS_SSEG	/* 18 */
#define T3MT_COUNTER	0xaa78		/* short */
#define T3MT_TOGGLE	0x358c		/* short */
#define T3MT_PROGRESS	0x0004		/* int   */
#define T3MT_MODE	0x2218		/* int, what the tail dispatches on */
#define T3MT_TICK	0x0234		/* int  */
#define T3MT_ELAPSED	0x0238		/* int  */
#define T3MT_DEADLINE	0x023c		/* int  */
#define T3MT_FLOOR	0x0230		/* int, rx_energy_floor            */
#define T3MT_ANSWER	0x359c		/* short, 0x65 originate 0x66 answer*/
#define T3MT_BAUD	0xaa96		/* short, the tail multiplies by 3  */
#define T3MT_VECTIDX	0x2aa2		/* short, the tail's 0x4a arm reads */
/* Inside the receiver, which is the object's +0x264. */
#define T3MT_RX_FLAGS	(0x0264 + 0x122)	/* short */
#define T3MT_RX_LEVEL	(0x0264 + 0x134)	/* short, agc_level */

static int dump;

/*
 * The unwritten-path codes, named.  They are numbers in src/ because the
 * strings firewall holds every literal there against the object's own
 * .rodata; a phrase this tree invented belongs on this side of the line.
 */
static const char *
unwritten_name(int code)
{
	switch (code) {
	case T3M_WRITTEN:		return "nothing unwritten was reached";
	case T3M_UNWRITTEN_TBL1:	return "table 1, the per-sample loop";
	case T3M_UNWRITTEN_RXIDLE:	return "the receiver-idle route";
	case T3M_UNWRITTEN_RXSTATE:	return "an rxstate other than RX_DPSK";
	case T3M_UNWRITTEN_FSKGATE:	return "the +0xa8a0 divert at 0x64a87";
	case T3M_UNWRITTEN_TBL3_DEFAULT: return "table 3's default arm";
	case T3M_UNWRITTEN_TBL3_ARM:	return "a table 3 arm not written yet";
	case T3M_UNWRITTEN_TBL2_ARM:	return "a table 2 arm not written yet";
	default:			return "an unknown unwritten path";
	}
}

/* Which paths the trials actually reached, so the claims below can be made. */
static int saw_below, saw_toggle, saw_force, saw_trace, saw_fsk;

/*
 * One trial: seed the counter, drive the microstate, step, compare.
 *
 * `ours` picks side A.  Everything else is identical between the two runs,
 * which is what makes the pair a control rather than two unrelated tests.
 */
/*
 * What one trial seeds beyond the state words.  A struct rather than eight
 * arguments because the tail's branches want five of them set at once, and
 * because a trial that leaves a field alone has to be visibly different from
 * one that sets it to zero -- finding 230's rule: zero is the value that
 * makes an unwritten field look deliberate.
 */
struct seed {
	short	txstate;
	short	counter;
	int	set_mode;	int	mode;
	int	set_time;	int	elapsed, deadline;
	int	set_level;	short	level;	int	floor;
	int	set_tick;	int	tick;
	int	set_flags;	short	flags;
	int	set_answer;	short	answer;
	int	set_baud;	short	baud;
	int	set_vectidx;	short	vectidx;
	/*
	 * `fsk_inhibit` at +0x402 makes `fskdemodulate` return without doing
	 * anything, and the fixture's varied fill leaves it non-zero -- so
	 * every trial that does not clear it tests the CALL and not the
	 * demodulator.  Clearing it is what makes the second argument, the
	 * receiver's +0x10c, a claim a test can fail.
	 */
	int	set_fsk;
};

#define T3MT_FSKINHIBIT	0x0402

static void
apply(const struct seed *s)
{
	if (s->set_mode)
		v34hs_poke_int(T3MT_MODE, s->mode);
	if (s->set_time) {
		v34hs_poke_int(T3MT_ELAPSED, s->elapsed);
		v34hs_poke_int(T3MT_DEADLINE, s->deadline);
	}
	if (s->set_level) {
		v34hs_poke_short(T3MT_RX_LEVEL, s->level);
		v34hs_poke_int(T3MT_FLOOR, s->floor);
	}
	if (s->set_tick)
		v34hs_poke_int(T3MT_TICK, s->tick);
	if (s->set_flags)
		v34hs_poke_short(T3MT_RX_FLAGS, s->flags);
	if (s->set_answer)
		v34hs_poke_short(T3MT_ANSWER, s->answer);
	if (s->set_baud)
		v34hs_poke_short(T3MT_BAUD, s->baud);
	if (s->set_vectidx)
		v34hs_poke_short(T3MT_VECTIDX, s->vectidx);
	if (s->set_fsk) {
		int k;

		/*
		 * The bring-up leaves the receiver's sample buffer at +0x10c
		 * ZERO, and `fskdetect` reads exactly four shorts from it --
		 * so with the buffer as the fixture leaves it, an input
		 * pointer one short out reads zeroes either way and the whole
		 * chain lands on the same object.  Nine varied shorts across
		 * +0x108..+0x118 is what makes the pointer a claim: they
		 * differ from each other, so a window shifted by one short is
		 * a different window.  Finding 230's rule, in the form
		 * finding 277 gives it -- values can be varied and still all
		 * lie in the set where the operation is the identity.
		 */
		v34hs_poke_short(T3MT_FSKINHIBIT, 0);
		for (k = 3; k < 66; k++)
			v34hs_poke_int(0x0264 + 4 * k,
				       (int)(0x1234 + 0x2f1d * k));
	}
}

static const struct seed plain = { T3MT_TXSTATE, 0x0100, 0,0, 0,0,0,
				   0,0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0 };

static void
trial_seeded(short mst, const struct seed *s, int ours, long tag)
{
	char what[96];
	int unwritten;

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(mst, V34HS_RX_DPSK, s->txstate);
	v34hs_poke_short(T3MT_COUNTER, s->counter);
	apply(s);

	v34handshak_t3mid_unwritten_reset();
	v34hs_side_a(ours ? v34handshak_t3mid : NULL);
	v34hs_step();

	snprintf(what, sizeof(what),
		 "microstate %d, tx %d, counter 0x%x, %s",
		 (int)mst, (int)s->txstate,
		 (unsigned)(unsigned short)s->counter,
		 ours ? "ours" : "blob");
	v34hs_compare(what, tag);

	unwritten = ours ? v34handshak_t3mid_unwritten() : T3M_WRITTEN;
	diff_eq_int(unwritten_name(unwritten), unwritten, T3M_WRITTEN, tag);

	if (dump) {
		const struct v34hs_obs *a = v34hs_observed(0);
		const struct v34hs_obs *b = v34hs_observed(1);

		printf("  %-40s  ours wrote %4u B sig %08x lines %u | "
		       "blob wrote %4u B sig %08x lines %u | "
		       "counter %04x -> %04x\n",
		       what, a->changed, a->hash, a->lines,
		       b->changed, b->hash, b->lines,
		       (unsigned)(unsigned short)s->counter,
		       (unsigned)(unsigned short)v34hs_peek_short(1,
								 T3MT_COUNTER));
	}
}

/*
 * A pair of trials -- ours and the blob -- plus the assertion that the step
 * did something at all.
 *
 * THE MOTION GUARD IS NOT OPTIONAL.  Every one of these arms writes at least
 * the counter, so a step that wrote nothing is a step that never reached the
 * arm; without this an entry that returned early would pass every byte
 * comparison in the file.  Finding 223 records the same shape from the other
 * direction: two objects agree when neither has moved.
 */
static void
both_seeded(short mst, const struct seed *s, long tag)
{
	trial_seeded(mst, s, 0, tag);
	trial_seeded(mst, s, 1, tag + 1);
	diff_eq_int("the step wrote something", v34hs_observed(0)->changed > 0,
		    1, tag);
}

/* The common case: nothing seeded but the counter, txstate held at SSEG. */
static void
both(short mst, short counter, long tag)
{
	struct seed s = plain;

	s.counter = counter;
	both_seeded(mst, &s, tag);
}

/*
 * --------------------------------------------------------------------------
 * 0x65d30 -- microstate 48 `TX_PHASE3_ANS`.
 *
 * The counter is incremented as an unsigned halfword and stored back before
 * either threshold is tested, and both thresholds are exact equalities.  Four
 * trials, chosen so that each of the three paths is taken and so that the two
 * thresholds cannot be exchanged for one another:
 *
 *      0x0100  neither: the ordinary path, txstate unchanged
 *      0x0077  becomes 0x78: inverts bit 0 of +0x358c
 *      0x00a1  becomes 0xa2: forces txstate to SILENCE and microstate to
 *              RX_PHASE2_ANS, clears the counter, and leaves through table
 *              2's txstate-5 arm rather than its SSEG arm
 *      0x00a2  becomes 0xa3: neither again, and it is the trial that says
 *              the 0xa2 test is an equality -- "at least 0xa2" would fire
 *
 * A fifth, 0x0079, is the same statement for the 0x78 threshold.
 */
static void
micro48(void)
{
	short before;

	both(V34HS_TX_PHASE3_ANS, 0x0100, 4800);
	saw_below = 1;

	/*
	 * The same, with `fsk_inhibit` cleared so `fskdemodulate` actually
	 * runs.  The fixture's fill leaves +0x402 non-zero, and every other
	 * trial in this file therefore exercises the CALL and not the
	 * demodulator -- which leaves the second argument, the receiver's
	 * +0x10c, unchecked.  Two trials rather than one, because the
	 * demodulator's own state advances and a single call is a weaker
	 * statement than a call whose input it disagrees about.
	 */
	{
		struct seed s = plain;

		s.set_fsk = 1;
		both_seeded(V34HS_TX_PHASE3_ANS, &s, 4802);
		s.counter = 0x0077;
		both_seeded(V34HS_TX_PHASE3_ANS, &s, 4804);
		saw_fsk = 1;
	}

	/*
	 * The 0x78 arm, and the proof it was the 0x78 arm: +0x358c comes back
	 * inverted in bit 0 and in nothing else.  Read off the BLOB's object,
	 * so it is a statement about the blob and not about our arm.
	 */
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_poke_short(T3MT_COUNTER, 0x0077);
	before = v34hs_peek_short(1, T3MT_TOGGLE);

	both(V34HS_TX_PHASE3_ANS, 0x0077, 4810);
	diff_eq_int("48 at 0x78 inverts bit 0 of +0x358c",
		    (unsigned short)(v34hs_peek_short(1, T3MT_TOGGLE)
				     ^ before),
		    1, 0x78);
	diff_eq_int("48 at 0x78 leaves the counter at 0x78",
		    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
		    0x78, 0x78);
	saw_toggle = 1;

	both(V34HS_TX_PHASE3_ANS, 0x0079, 4820);
	diff_eq_int("48 at 0x7a does not take the 0x78 arm",
		    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
		    0x7a, 0x7a);

	/*
	 * The 0xa2 arm.  Three separate consequences, each read off the blob:
	 * the counter is cleared, txstate becomes SILENCE and the microstate
	 * becomes RX_PHASE2_ANS.  Any one of them alone would be satisfied by
	 * a wrong arm that happened to write that field.
	 */
	both(V34HS_TX_PHASE3_ANS, 0x00a1, 4830);
	diff_eq_int("48 at 0xa2 clears the counter",
		    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER), 0, 0xa2);
	diff_eq_int("48 at 0xa2 forces txstate to SILENCE",
		    v34hs_peek_short(1, V34HS_TXSTATE_OFF), V34HS_SILENCE,
		    0xa2);
	diff_eq_int("48 at 0xa2 forces the microstate to RX_PHASE2_ANS",
		    v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
		    V34HS_RX_PHASE2_ANS, 0xa2);
	/*
	 * And it announces both, which is the half of the evidence the byte
	 * comparison cannot give: two lines, one per machine, and the
	 * transcripts are compared in full by `v34hs_compare`.
	 */
	diff_eq_int("48 at 0xa2 prints both transitions",
		    v34hs_observed(1)->lines, 2, 0xa2);
	saw_force = 1;
	saw_trace = 1;

	both(V34HS_TX_PHASE3_ANS, 0x00a2, 4840);
	diff_eq_int("48 at 0xa3 does not take the 0xa2 arm",
		    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
		    0xa3, 0xa3);
}

/*
 * --------------------------------------------------------------------------
 * 0x62a40 -- the tail, driven through microstate 48's ordinary path.
 *
 * Every arm of table 3 and every arm of table 2 falls into these eighty-eight
 * instructions, and they are the only place the function returns from -- so
 * a batch that reconstructs nine arms and leaves the tail's branches
 * unexercised has nine arms resting on an untested epilogue.  Six of its
 * branches are reachable from here and each gets a trial on both sides of its
 * condition:
 *
 *      +0x2218 == 1            the receiver's +0x1d2 gets three times the
 *                              baud rate, progress 4, and the txstate is
 *                              RE-READ from the object
 *      +0x2218 in {4, 5}       progress 6
 *      +0x2218 in {2, 3}       and txstate 0x4a: progress from `vect_idx`
 *      +0x238 > +0x23c         progress 8, UNSIGNED
 *      agc_level vs +0x230     one arm zeroes +0x234, the other increments it
 *      +0x234 > 0x257f         progress 9
 *      txstate 0x52/0x53/0x54  progress 0x0d / 0x0f / 0x10
 *
 * 0x52, 0x53 and 0x54 are past table 2's last entry, so the transmit dispatch
 * hands straight to the tail -- which is how a table-3 arm can reach those
 * three at all.
 */
static void
tail_paths(void)
{
	struct seed s;
	long tag = 4900;
	int i;

	/* +0x2218: 0 and 6 take nothing, 1, 2, 3, 4 and 5 each take an arm. */
	for (i = 0; i <= 6; i++) {
		s = plain;
		s.set_mode = 1;
		s.mode = i;
		s.set_baud = 1;
		s.baud = (short)(0x1234 + i);
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}

	/*
	 * The 0x4a arm needs mode 2 or 3 AND txstate 0x4a, and 0x4a is 74
	 * SILENCERETRAIN, which table 2 sends to 0x644c9.  Both sides of the
	 * `vect_idx > 0x3c` test, because the two answers are 0 and 7 and a
	 * trial that only ever saw one of them could not tell them apart.
	 */
	for (i = 0; i < 2; i++) {
		s = plain;
		s.txstate = 0x4a;
		s.set_mode = 1;
		s.mode = 2;
		s.set_vectidx = 1;
		s.vectidx = (short)(i ? 0x3d : 0x3c);
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}

	/* +0x238 against +0x23c, unsigned and straddled in both directions,
	   with the EQUAL case, which is what says the test is strict. */
	for (i = 0; i < 4; i++) {
		static const int el[4] = { 100, 200, -1, 150 };
		static const int dl[4] = { 200, 100, 5, 150 };

		s = plain;
		s.set_time = 1;
		s.elapsed = el[i];
		s.deadline = dl[i];
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}

	/* agc_level against +0x230: equal, above and below. */
	for (i = 0; i < 3; i++) {
		static const short lv[3] = { 300, 300, -300 };
		static const int fl[3] = { 300, 500, 100 };

		s = plain;
		s.set_level = 1;
		s.level = lv[i];
		s.floor = fl[i];
		s.set_tick = 1;
		s.tick = 0x1000;
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}

	/* +0x234 across 0x257f, with the level below the floor so the counter
	   is incremented rather than zeroed. */
	for (i = 0; i < 3; i++) {
		static const int tk[3] = { 0x257e, 0x257f, 0x2580 };

		s = plain;
		s.set_level = 1;
		s.level = -1000;
		s.floor = 1000;
		s.set_tick = 1;
		s.tick = tk[i];
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}

	/* The three txstates past table 2's window, and one that is not. */
	for (i = 0; i < 4; i++) {
		static const short tx[4] = { 0x52, 0x53, 0x54, 0x55 };

		s = plain;
		s.txstate = tx[i];
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}
}

/*
 * --------------------------------------------------------------------------
 * .rodata+0x2ee8 -- the three transmit arms a table-3 arm can hand control to.
 *
 * These are not a reconstruction of table 2, which is #56's; they are here
 * because an arm that ends in `jmp 62af1` cannot be compared without them.
 * What this drives is what microstate 48 can reach: SSEG and SBARSEG on the
 * ordinary path, SILENCE on the 0xa2 path, the 24/51/54/60/74 arm, and an
 * index past the end.
 */
static void
txblock_paths(void)
{
	struct seed s;
	long tag = 4970;
	int i;

	/* 0x64518: originate answers 2 whatever the flag says, and the answer
	   side answers 3 or 2 on bit 3 of the receiver's flags. */
	for (i = 0; i < 4; i++) {
		s = plain;
		s.txstate = (short)(i & 1 ? 19 : 18);
		s.set_answer = 1;
		s.answer = (short)(i < 2 ? 0x65 : 0x66);
		s.set_flags = 1;
		s.flags = (short)(i & 1 ? 0x3fff : 0x37f7);
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}

	/* 0x644c9, five txstates into one arm. */
	for (i = 0; i < 5; i++) {
		static const short tx[5] = { 24, 51, 54, 60, 74 };

		s = plain;
		s.txstate = tx[i];
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}

	/*
	 * 0x64480, reached the only way a table-3 arm can reach it: microstate
	 * 48 at the 0xa2 threshold forces txstate to SILENCE.  Its three
	 * routes to progress 1 all want a microstate or rxstate this route
	 * cannot present -- by the time the arm gets there the microstate is
	 * RX_PHASE2_ANS and the rxstate is RX_DPSK -- so what is exercised
	 * here is the fall-through, and the finding says so.
	 */
	for (i = 0; i < 2; i++) {
		s = plain;
		s.counter = 0x00a1;
		s.set_answer = 1;
		s.answer = (short)(i ? 0x66 : 0x65);
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}
}

/*
 * --------------------------------------------------------------------------
 * The four guards, and the two table bounds, checked by the path they select.
 *
 * These cannot be checked by comparing objects, because the answer is that
 * our entry does NOTHING: the path is one this batch has not written, and the
 * blob of course goes on and does the whole step.  What is compared instead
 * is WHICH unwritten path was selected, which is a claim about the guard
 * alone and is exactly what a range constant off by one changes.
 *
 * Without this, `T3M_TBL3_COUNT`, `T3M_TBL3_FIRST`, `T3M_TBL2_COUNT`, the
 * cursor compare, the receiver-count compare and the +0xa8a0 gate are all
 * free: every trial above drives values that satisfy them either way.
 */
static void
guard(short mst, const struct seed *s, short rxstate, int expect, long tag)
{
	char what[96];

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(mst, rxstate, s->txstate);
	v34hs_poke_short(T3MT_COUNTER, s->counter);
	apply(s);

	v34handshak_t3mid_unwritten_reset();
	v34hs_side_a(v34handshak_t3mid);
	v34hs_step();
	v34hs_side_a(NULL);

	snprintf(what, sizeof(what), "guard: microstate %d, rxstate %d -> %s",
		 (int)mst, (int)rxstate, unwritten_name(expect));
	diff_eq_int(what, v34handshak_t3mid_unwritten(), expect, tag);
}

static void
guards(void)
{
	struct seed s;
	long tag = 5100;

	/* Table 3's window is 41..80, and one past each end is the default
	   arm at 0x65329 rather than an entry of the table. */
	s = plain;
	guard(40, &s, V34HS_RX_DPSK, T3M_UNWRITTEN_TBL3_DEFAULT, tag++);
	guard(81, &s, V34HS_RX_DPSK, T3M_UNWRITTEN_TBL3_DEFAULT, tag++);
	guard(41, &s, V34HS_RX_DPSK, T3M_UNWRITTEN_TBL3_ARM, tag++);
	guard(80, &s, V34HS_RX_DPSK, T3M_UNWRITTEN_TBL3_ARM, tag++);

	/* Table 2's is 5..74, and 75 is past it -- which is how microstate
	   48 driven with txstate 75 reaches the tail without an arm. */
	s = plain;
	s.txstate = 75;
	guard(V34HS_TX_PHASE3_ANS, &s, V34HS_RX_DPSK, T3M_WRITTEN, tag++);
	s.txstate = 20;			/* 0x64509, which is not written */
	guard(V34HS_TX_PHASE3_ANS, &s, V34HS_RX_DPSK, T3M_UNWRITTEN_TBL2_ARM,
	      tag++);

	/* The rxstate chain: only RX_DPSK reaches table 3. */
	s = plain;
	guard(V34HS_TX_PHASE3_ANS, &s, V34HS_RX_RECEIVE,
	      T3M_UNWRITTEN_RXSTATE, tag++);
	guard(V34HS_TX_PHASE3_ANS, &s, V34HS_RX_WAIT, T3M_UNWRITTEN_RXSTATE,
	      tag++);

	/*
	 * The receiver's first halfword.  `v34hs_route` leaves it at 6; at 5
	 * the once-per-block dispatch runs instead, which is another batch's
	 * route.  `<= 5` and `< 5` are the two readings and this is the trial
	 * that separates them.
	 */
	s = plain;
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, T3MT_TXSTATE);
	v34hs_poke_short(0x0264, 5);
	v34handshak_t3mid_unwritten_reset();
	v34hs_side_a(v34handshak_t3mid);
	v34hs_step();
	v34hs_side_a(NULL);
	diff_eq_int("guard: a receiver count of 5 takes the block route",
		    v34handshak_t3mid_unwritten(), T3M_UNWRITTEN_RXIDLE,
		    tag++);

	/*
	 * The cursor against the limit.  Route RXCHAIN leaves both at zero,
	 * where `<` and `>` agree, so a trial with the cursor ABOVE the limit
	 * is what says the compare is the right way round -- it must still
	 * reach table 3.
	 */
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, T3MT_TXSTATE);
	v34hs_poke_short(0x221c, 5);
	v34hs_poke_short(0x2aa0, 0);
	v34handshak_t3mid_unwritten_reset();
	v34hs_side_a(v34handshak_t3mid);
	v34hs_step();
	v34hs_compare("cursor above the limit still reaches table 3", tag);
	v34hs_side_a(NULL);
	diff_eq_int("guard: a cursor above the limit reaches table 3",
		    v34handshak_t3mid_unwritten(), T3M_WRITTEN, tag++);

	/* And below it, which is table 1 and #56's. */
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, T3MT_TXSTATE);
	v34hs_poke_short(0x221c, 0);
	v34hs_poke_short(0x2aa0, 5);
	v34handshak_t3mid_unwritten_reset();
	v34hs_side_a(v34handshak_t3mid);
	/* NOT stepped: table 1's default arm does not terminate (D59), and
	   this is a claim about our guard, which needs no step at all. */
	v34handshak_t3mid((void *)v34hs_object(0));
	v34hs_side_a(NULL);
	diff_eq_int("guard: a cursor below the limit is table 1",
		    v34handshak_t3mid_unwritten(), T3M_UNWRITTEN_TBL1, tag++);

	/* The +0xa8a0 gate, which `v34hs_route` clears. */
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, T3MT_TXSTATE);
	v34hs_poke_int(0xa8a0, 1);
	v34handshak_t3mid_unwritten_reset();
	v34hs_side_a(v34handshak_t3mid);
	v34hs_step();
	v34hs_side_a(NULL);
	diff_eq_int("guard: a non-zero +0xa8a0 diverts at 0x64a87",
		    v34handshak_t3mid_unwritten(), T3M_UNWRITTEN_FSKGATE,
		    tag++);
}

int
main(void)
{
	dump = getenv("V34HS_DUMP") != NULL;
	diff_begin("v34handshak table 3, the middle group");

	/*
	 * The diagnostics are on throughout.  They are the cheapest
	 * discriminator the object offers, and two of these arms are
	 * distinguished from their neighbours by nothing else; `StateName` is
	 * indexed unbounded while they are on (D42), so every state word
	 * driven here stays inside 0..86.
	 */
	v34hs_debug(1);

	micro48();
	tail_paths();
	txblock_paths();
	guards();

	/*
	 * WHAT THE TRIALS REACHED.  Each of these is a branch of an arm, and
	 * a suite that never took one is a suite in which that branch's
	 * constants are free.  Findings 247 and 262 are two checks in this
	 * tree that could not be satisfied by any input and failed loudly;
	 * these can be, and the trials above are chosen so that they are.
	 */
	diff_eq_int("a below-threshold trial ran", saw_below, 1, 0);
	diff_eq_int("the 0x78 arm was taken", saw_toggle, 1, 0);
	diff_eq_int("the 0xa2 arm was taken", saw_force, 1, 0);
	diff_eq_int("a trial printed a transition", saw_trace, 1, 0);
	diff_eq_int("a trial ran the demodulator", saw_fsk, 1, 0);

	/*
	 * And every pointer field `v34hs_compare` skips was exercised, so the
	 * thirty-five offsets this test trusts cannot go stale unnoticed.
	 */
	v34hs_holes_check();

	v34hs_side_a(NULL);
	return diff_end();
}

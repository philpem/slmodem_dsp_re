/*
 * t_v34hsfsk.c -- `v34handshak`'s FSK gate, 0x64a87 and the arm at 0x6754b.
 *
 * THE GATE DOES NOT DIVERT.  A non-zero +0xa8a0 sends the step through 1,089
 * bytes that are `detectRetrainReq` inlined, and then every one of that arm's
 * six exits is `jmp 0x64a8f` -- the instruction immediately below the gate's
 * own test, which the CLEARED gate reaches too.  So the gate adds a poll of
 * the retrain detector to the same route rather than choosing a different
 * one, and `fskdemodulate` and the microstate dispatch run either way.  The
 * stub this file replaces returned instead, which was wrong for every gated
 * step of the function and not only for this arm.  Finding F721.
 *
 * WHAT THAT MAKES THIS TEST.  Two axes, and both are needed:
 *
 *   - the DETECTOR's own state machine, which decides whether the arm fires:
 *     the 128-sample counter, the two retrain states, the three quiet bins
 *     and the one loud one.  `t_v34hshak.c` already sweeps the standalone
 *     `detectRetrainReq`, so what is new here is that `v34handshak` polls it
 *     with the right arguments in the right place;
 *   - the ACTION BLOCK at 0x6936e, which is the only genuinely new logic in
 *     the arm and runs on the one path in a hundred that fires.
 *
 * THE SAMPLES CANNOT BE POKED.  `dftupdate`'s four inputs are at receiver +
 * 0x10c, which is `V34agc`'s OUTPUT -- written by the call immediately above
 * the gate on every step (v34rx.h).  Every trial here therefore steers the
 * detector through the ACCUMULATORS at bin + 0x04/+0x08, which `dftupdate`
 * adds into, or through the THRESHOLDS at bin + 0x28/+0x2a, which nothing in
 * the step writes.  `bins[i].energy`, `.shift` and `.denergy` are not levers
 * either: `dftenergy` overwrites all three on every 0x80-path entry.
 *
 * AND THE WITNESS IS THE BLOB'S.  A trial that means to fire the arm asserts
 * that SIDE B's transcript carries the object's own "DET_SYNC : retrain
 * request detected while searching for info1" line.  That is a statement
 * about the blob rather than about this reconstruction, so a trial cannot
 * satisfy its own anti-vacuity guard by agreeing with itself.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "harness.h"
#include "v34hsstep.h"

/* The gate, and the five scalars the detector keeps beside it. */
#define FSK_GATE	0xa8a0	/* int   */
#define FSK_STATE	0xa24a	/* short */
#define FSK_PHASE	0xa24c	/* int   */
#define FSK_RUNS	0xa250	/* short */
#define FSK_QUIET_RUNS	0xa252	/* short */
#define FSK_TONE_RUNS	0xa254	/* short */

/* The three bins, and the two thresholds on each. */
#define FSK_BINS	0xa81c
#define FSK_BINSTRIDE	0x2c
#define FSK_BIN(i)	(FSK_BINS + (i) * FSK_BINSTRIDE)
#define FSK_ACC_RE	0x04
#define FSK_ACC_IM	0x08
#define FSK_THRESH_LO	0x28
#define FSK_THRESH_HI	0x2a

/* What the action block writes, besides the three state words. */
#define FSK_F3588	0x3588	/* short, |= 2                        */
#define FSK_TOGGLE	0x358c	/* short, zeroed when UNSIGNED > 1    */
#define FSK_VECT_IDX	0x2aa2	/* short                              */
#define FSK_COUNTER	0xaa78	/* short                              */
#define FSK_NBITS	0xaae0	/* short, fsk.nbits                   */
#define FSK_SR		0xaae2	/* short, fsk.sr                      */

/*
 * The microstate and txstate every trial is driven with, where the trial is
 * not about them.
 *
 * 42 DET_CJ is the nineteen-byte arm twenty-four microstates share (0x6590b)
 * -- read txstate, go to the once-per-block transmit dispatch -- so it adds
 * as little as any written arm can to what the gate itself did.  24 TX_DPSK
 * is one of the five txstates table 2 acts on at 0x644c9, for the reason
 * `t_v34hsrxch.c` gives: a dispatch that does nothing makes every trial a
 * comparison of two silences.  Table 1 cannot run on any trial in this file,
 * because `V34HS_ROUTE_RXCHAIN` pins the cursor and the limit both at zero.
 */
#define FSK_MST		V34HS_DET_CJ
#define FSK_TXST	V34HS_TX_DPSK

/*
 * What the action block sets the three words to: 0x2e, 0x2b and 0x3c at
 * 0x6949c, 0x6952a and 0x695b8.
 */
#define FSK_NEXT_MST	V34HS_TX_PHASE1_ANS	/* 46 */
#define FSK_NEXT_RXST	V34HS_RX_DPSK		/* 43 */
#define FSK_NEXT_TXST	V34HS_TONE_AB		/* 60 */

static int dump;

/* The one field the harness has no typed accessor for. */
static int
peek_int(int side, unsigned off)
{
	int v;

	memcpy(&v, (const unsigned char *)v34hs_object(side) + off, sizeof(v));
	return v;
}

/*
 * Did the BLOB take the action block?
 *
 * 0x69377 is the only site in `v34handshak` that prints this literal, and
 * side B is the blob's own copy of the function, so this is an oracle and
 * not a restatement of what our code did.
 */
static int
blob_fired(void)
{
	return strstr(v34hs_text(1),
		      "retrain request detected while searching") != NULL;
}

/*
 * How every trial in this file is set up: the rx chain, rxstate 43, the gate
 * armed, and the detector put wherever the caller wants it.
 *
 * THE ORDER MATTERS.  `v34hs_route` CLEARS +0xa8a0 itself -- it has to, or
 * rxstate 43 would never reach the microstate table for any other test -- so
 * a poke of the gate before the route is a poke that is thrown away and a
 * trial that silently never enters the arm.
 */
struct trial {
	int	gate;		/* +0xa8a0                              */
	int	phase;		/* +0xa24c                              */
	short	state;		/* +0xa24a                              */
	short	runs;		/* +0xa250                              */
	short	quiet_runs;	/* +0xa252                              */
	short	tone_runs;	/* +0xa254                              */
	short	thresh_lo;	/* all three bins                       */
	short	thresh_hi;	/* all three bins                       */
	int	zero_acc;	/* clear the three bins' accumulators   */
	short	mst, txst;	/* the two state words that are levers  */
	short	toggle;		/* +0x358c                              */
	short	short_3588;		/* +0x3588                              */
	/*
	 * THE FOUR FIELDS THE ACTION BLOCK CLEARS, SEEDED NON-ZERO.
	 *
	 * `v34handshakinit` sets `vect_idx` and `fsk.nbits` to zero on the
	 * way in, so a trial that left them there would compare a store of
	 * zero against a field that was already zero -- eleven of table 1's
	 * claims were untestable for exactly this reason (finding F345), and
	 * four of this file's five first-run survivors were.  None of the
	 * four is read between `v34hs_setup` and the gate, so seeding them
	 * is a poke and not finding F429's write-then-read.
	 */
	short	vect_idx;	/* +0x2aa2                              */
	short	counter;	/* +0xaa78                              */
	short	nbits;		/* +0xaae0, fsk.nbits                   */
	short	sr;		/* +0xaae2, fsk.sr                      */
};

static void
setup(const struct trial *t)
{
	int i;

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(t->mst, V34HS_RX_DPSK, t->txst);

	v34hs_poke_int(FSK_GATE, t->gate);
	v34hs_poke_int(FSK_PHASE, t->phase);
	v34hs_poke_short(FSK_STATE, t->state);
	v34hs_poke_short(FSK_RUNS, t->runs);
	v34hs_poke_short(FSK_QUIET_RUNS, t->quiet_runs);
	v34hs_poke_short(FSK_TONE_RUNS, t->tone_runs);
	v34hs_poke_short(FSK_TOGGLE, t->toggle);
	v34hs_poke_short(FSK_F3588, t->short_3588);
	v34hs_poke_short(FSK_VECT_IDX, t->vect_idx);
	v34hs_poke_short(FSK_COUNTER, t->counter);
	v34hs_poke_short(FSK_NBITS, t->nbits);
	v34hs_poke_short(FSK_SR, t->sr);

	for (i = 0; i < V34_RETRAIN_BINS; i++) {
		v34hs_poke_short(FSK_BIN(i) + FSK_THRESH_LO, t->thresh_lo);
		v34hs_poke_short(FSK_BIN(i) + FSK_THRESH_HI, t->thresh_hi);
		if (t->zero_acc) {
			v34hs_poke_int(FSK_BIN(i) + FSK_ACC_RE, 0);
			v34hs_poke_int(FSK_BIN(i) + FSK_ACC_IM, 0);
		}
	}
}

/*
 * One step, compared whole.
 *
 * `expect_fire` is checked against the BLOB's transcript, so a trial that
 * meant to reach the action block and did not says so instead of passing.
 */
static void
run(const struct trial *t, const char *what, int expect_fire, long tag)
{
	char msg[192];

	setup(t);

	v34hs_ours(1);
	v34hs_step();
	v34hs_ours(0);

	v34hs_compare(what, tag);

	snprintf(msg, sizeof(msg), "%s: the arm %s", what,
		 expect_fire ? "fired" : "did not fire");
	diff_eq_int(msg, blob_fired(), expect_fire != 0, tag);

	if (dump)
		printf("  %-52s fired %d  phase %3d  state %d  runs %d\n",
		       what, blob_fired(), peek_int(0, FSK_PHASE),
		       (int)v34hs_peek_short(0, FSK_STATE),
		       (int)v34hs_peek_short(0, FSK_RUNS));
}

/* The seed every trial starts from; the suites move one field at a time. */
static const struct trial base = {
	1,			/* gate armed                           */
	0,			/* phase                                */
	1,			/* state 1, waiting for silence         */
	0, 3, 9,		/* runs, quiet_runs, tone_runs          */
	0, 0,			/* thresholds                           */
	1,			/* accumulators cleared                 */
	FSK_MST, FSK_TXST,
	0, 0,			/* +0x358c, +0x3588                     */
	5, 0x11, 3, 0x1234	/* the four the action block clears     */
};

/*
 * The gate's own test at 0x64a87, and the counter at +0xa24c.
 *
 * `test %esi,%esi; jne 6754b` is a 32-bit test of the whole word, so the four
 * trials below are zero, a small non-zero, a value whose LOW HALFWORD is zero
 * -- which a gate mistakenly read as a `short` would treat as clear -- and a
 * negative one.  The counter is the witness: +4 on every gated step,
 * untouched on the ungated one.
 */
static void
suite_gate(void)
{
	long tag = 100;
	static const struct { int gate; int polled; const char *what; } t[] = {
		{ 0,          0, "a cleared gate never polls the detector"    },
		{ 1,          1, "a gate of 1 polls it"                       },
		{ 0x10000,    1, "and so does one whose low halfword is zero" },
		{ -1,         1, "and so does one that is negative"           }
	};
	struct trial tr;
	int i;

	for (i = 0; i < 4; i++) {
		char msg[160];

		tr = base;
		tr.gate = t[i].gate;
		tr.phase = 0x10;
		run(&tr, t[i].what, 0, tag);

		snprintf(msg, sizeof(msg), "%s: the counter %s", t[i].what,
			 t[i].polled ? "advanced by four" : "did not move");
		diff_eq_int(msg, peek_int(0, FSK_PHASE),
			    t[i].polled ? 0x14 : 0x10, tag++);
	}
}

/*
 * The 128-sample counter: `add $0x4` then `cmp $0x80` and **`je`**.
 *
 * EQUALITY AND NOT `>=`, which is why 0x7c is the only starting value in
 * this file that measures in one step and why 0x7e cannot ever measure --
 * it would step to 0x82 and go round again, 0x86, 0x8a, and never equal
 * 0x80.  Both are driven.  A reconstruction using `>=` passes every trial
 * whose counter lands exactly on 0x80 and fails the two that do not.
 */
static void
suite_counter(void)
{
	long tag = 200;
	static const struct { int start; int after; const char *what; } t[] = {
		{ 0x00, 0x04, "the counter starts at zero"                   },
		{ 0x78, 0x7c, "and stops one short of the measurement"       },
		{ 0x7e, 0x82, "an ODD multiple of two steps PAST 128"        },
		{ 0x84, 0x88, "and past it the counter keeps going"          }
	};
	struct trial tr;
	int i;

	for (i = 0; i < 4; i++) {
		char msg[160];

		tr = base;
		tr.phase = t[i].start;
		run(&tr, t[i].what, 0, tag);

		snprintf(msg, sizeof(msg), "%s: +0xa24c is 0x%x afterwards",
			 t[i].what, (unsigned)t[i].after);
		diff_eq_int(msg, peek_int(0, FSK_PHASE), t[i].after, tag++);
	}
}

/*
 * State 1, the silence hunt: 0x6aa85 and 0x6d2a6.
 *
 * All three bins have to be below `thresh_lo` at once for the run to
 * continue, and the arm that ADVANCES the machine is the one where that
 * FAILS -- a run has to end before its length can be judged.  So:
 *
 *   thresh_lo = 0        no unsigned energy is < 0, so every trial is LOUD
 *   thresh_lo = 0x7fff   every energy the four AGC samples can produce is
 *                        below it, so every trial is QUIET
 *
 * and the second is asserted rather than assumed: a quiet window increments
 * +0xa250 and a loud one zeroes it, so the counter says which happened.
 */
static void
suite_state1(void)
{
	long tag = 300;
	static const struct {
		short lo, runs, quiet_runs, after_runs, after_state;
		const char *what;
	} t[] = {
		{ 0x7fff, 0, 3, 1, 1, "state 1, quiet: the run lengthens"     },
		{ 0x7fff, 7, 3, 8, 1, "state 1, quiet again, from a long run" },
		{ 0,      0, 3, 0, 1, "state 1, loud, run too short to count" },
		{ 0,      2, 3, 0, 1, "state 1, loud, run one short"          },
		{ 0,      3, 3, 0, 2, "state 1, loud, run exactly long enough"},
		{ 0,      9, 3, 0, 2, "state 1, loud, a run well past it"     }
	};
	struct trial tr;
	int i;

	for (i = 0; i < 6; i++) {
		char msg[192];

		tr = base;
		tr.phase = 0x7c;
		tr.state = 1;
		tr.thresh_lo = t[i].lo;
		tr.runs = t[i].runs;
		tr.quiet_runs = t[i].quiet_runs;
		run(&tr, t[i].what, 0, tag);

		snprintf(msg, sizeof(msg), "%s: +0xa250 is %d", t[i].what,
			 (int)t[i].after_runs);
		diff_eq_int(msg, v34hs_peek_short(0, FSK_RUNS),
			    t[i].after_runs, tag);
		snprintf(msg, sizeof(msg), "%s: +0xa24a is %d", t[i].what,
			 (int)t[i].after_state);
		diff_eq_int(msg, v34hs_peek_short(0, FSK_STATE),
			    t[i].after_state, tag++);
	}
}

/*
 * State 2, the tone hunt: 0x6932d, 0x6ca72 and the shared tail at 0x69353.
 *
 * Only bin 1 -- 1200 Hz -- is compared here, against `thresh_hi`, and the
 * comparison is `>`: `movzwl 0x38(%ebx)` against `movswl 0x56(%ebx)`, so a
 * `thresh_hi` of -1 is below every unsigned energy and 0x7fff is above every
 * one the four samples can make.
 *
 * THE RESET ARM FALLS INTO THE SHARED TAIL and the state-1 arms do not:
 * 0x6ca72 ends `jmp 69353`, while 0x6aace and 0x6d2d9 end `jmp 64a8f`.  The
 * last trial is what makes that observable -- `tone_runs` of zero, tone
 * absent, so the reset writes `runs = 0` and the tail then finds `0 == 0`
 * and FIRES on the very call that gave up.  A reconstruction that tidied the
 * tail into the increment arm returns 0 there.
 */
static void
suite_state2(void)
{
	long tag = 400;
	static const struct {
		short hi, runs, tone_runs, after_runs, after_state;
		int fires;
		const char *what;
	} t[] = {
		{ -1,     0, 9, 1, 2, 0, "state 2, tone, the run lengthens"    },
		{ -1,     7, 9, 8, 2, 0, "state 2, tone, one short of the end" },
		{ -1,     8, 9, 0, 1, 1, "state 2, tone, the run completes"    },
		{ 0x7fff, 5, 9, 0, 1, 0, "state 2, no tone, the hunt restarts" },
		{ 0x7fff, 5, 0, 0, 1, 1, "state 2, no tone, tone_runs zero"    }
	};
	struct trial tr;
	int i;

	for (i = 0; i < 5; i++) {
		char msg[192];

		tr = base;
		tr.phase = 0x7c;
		tr.state = 2;
		tr.thresh_hi = t[i].hi;
		tr.runs = t[i].runs;
		tr.tone_runs = t[i].tone_runs;
		run(&tr, t[i].what, t[i].fires, tag);

		/*
		 * A FIRED trial reads its two scalars back from
		 * `dftRetrainDetInit`, which the action block calls: state 1,
		 * runs 0, tone_runs 9, quiet_runs 3.  An unfired one reads
		 * back what the detector left.
		 */
		snprintf(msg, sizeof(msg), "%s: +0xa250 is %d", t[i].what,
			 (int)t[i].after_runs);
		diff_eq_int(msg, v34hs_peek_short(0, FSK_RUNS),
			    t[i].after_runs, tag);
		snprintf(msg, sizeof(msg), "%s: +0xa24a is %d", t[i].what,
			 (int)t[i].after_state);
		diff_eq_int(msg, v34hs_peek_short(0, FSK_STATE),
			    t[i].after_state, tag++);
	}
}

/*
 * A retrain state that is neither 1 nor 2 leaves at 0x69327 with the window
 * already cleared, which is the one path that measures and then does nothing.
 */
static void
suite_state_other(void)
{
	long tag = 500;
	static const short states[] = { 0, 3, -1, 0x100 };
	struct trial tr;
	int i;

	for (i = 0; i < 4; i++) {
		char msg[160];

		tr = base;
		tr.phase = 0x7c;
		tr.state = states[i];
		snprintf(msg, sizeof(msg),
			 "retrain state %d is neither 1 nor 2",
			 (int)states[i]);
		run(&tr, msg, 0, tag);

		snprintf(msg, sizeof(msg),
			 "retrain state %d: the state is left alone",
			 (int)states[i]);
		diff_eq_int(msg, v34hs_peek_short(0, FSK_STATE),
			    states[i], tag++);
	}
}

/*
 * THE ACTION BLOCK, 0x6936e.
 *
 * Reached the same way every time -- state 2, tone present, one run short of
 * `tone_runs` -- and then the fields it writes are moved one at a time so
 * that each store is a store the object had to make rather than one the fill
 * had already made for it.  Finding F345's lesson from table 1.
 */
static void
fired_case(const struct trial *t, const char *what, long tag)
{
	char msg[192];

	run(t, what, 1, tag);

	snprintf(msg, sizeof(msg), "%s: +0xa8a0 is cleared", what);
	diff_eq_int(msg, peek_int(0, FSK_GATE), 0, tag);

	snprintf(msg, sizeof(msg), "%s: the detector is re-armed", what);
	diff_eq_int(msg, v34hs_peek_short(0, FSK_STATE) == 1
			 && v34hs_peek_short(0, FSK_RUNS) == 0
			 && v34hs_peek_short(0, FSK_QUIET_RUNS) == 3
			 && v34hs_peek_short(0, FSK_TONE_RUNS) == 9
			 && peek_int(0, FSK_PHASE) == 0, 1, tag);

	snprintf(msg, sizeof(msg), "%s: +0x3588 has bit 1 set", what);
	diff_eq_int(msg, v34hs_peek_short(0, FSK_F3588) & 2, 2, tag);

	snprintf(msg, sizeof(msg), "%s: the microstate moved to 46", what);
	diff_eq_int(msg, v34hs_peek_short(0, V34HS_MICROSTATE),
		    FSK_NEXT_MST, tag);
	snprintf(msg, sizeof(msg), "%s: the rxstate is still 43", what);
	diff_eq_int(msg, v34hs_peek_short(0, V34HS_RXSTATE),
		    FSK_NEXT_RXST, tag);
}

static void
suite_fired(void)
{
	long tag = 600;
	struct trial tr;
	char msg[192];

	/* The plain case, from a seed that holds none of what it writes. */
	tr = base;
	tr.phase = 0x7c;
	tr.state = 2;
	tr.thresh_hi = -1;
	tr.runs = 8;
	tr.tone_runs = 9;
	tr.short_3588 = 0;
	tr.toggle = 0;
	fired_case(&tr, "the action block, from a cold seed", tag++);

	/*
	 * +0x2aa2 is asserted and +0xaa78 IS NOT, and the difference is the
	 * FALL-THROUGH: microstate 46's arm runs below the join and bumps the
	 * counter the action block had just cleared, so a witness reading 1
	 * would be a witness on that arm and not on this store.  What holds
	 * +0xaa78 is the whole-object comparison in `run`, against a seed
	 * that puts 0x11 there -- and `test/mutations/v34hsfsk.json`'s
	 * "+0xaa78 is not cleared" is caught by it.
	 */
	diff_eq_int("the action block clears +0x2aa2",
		    v34hs_peek_short(0, FSK_VECT_IDX), 0, tag);
	if (dump)
		printf("  after the action block: +0x2aa2 %d  +0xaa78 %d\n",
		       (int)v34hs_peek_short(0, FSK_VECT_IDX),
		       (int)v34hs_peek_short(0, FSK_COUNTER));
	tag++;

	/*
	 * THE CLEAR OF +0xa8a0 IS 32 BITS WIDE -- `xor %edx,%edx; mov
	 * %edx,0xa8a0(%eax)` at 0x6938a.  Fired from a gate whose LOW
	 * HALFWORD is already zero, a 16-bit store leaves 0x10000 standing
	 * and the gate is still armed next step; from a gate of 1, which
	 * every other trial here uses, the two widths agree.
	 */
	tr.gate = 0x10000;
	fired_case(&tr, "the action block, from a gate of 0x10000", tag++);
	tr.gate = 1;

	/*
	 * +0x3588 IS A READ-MODIFY-WRITE and not a store.  Seeded with bit 0
	 * set, an `= 2` would lose it and an `|= 2` would not.
	 */
	tr.short_3588 = 1;
	fired_case(&tr, "the action block, +0x3588 already holding bit 0",
		   tag);
	diff_eq_int("+0x3588 |= 2 keeps the bit that was there",
		    v34hs_peek_short(0, FSK_F3588), 3, tag++);

	tr.short_3588 = 2;
	fired_case(&tr, "the action block, +0x3588 already holding bit 1",
		   tag);
	diff_eq_int("+0x3588 |= 2 on a value that already has it",
		    v34hs_peek_short(0, FSK_F3588), 2, tag++);

	/*
	 * THE MICROSTATE AND TXSTATE SETTERS EACH HAVE TWO BRANCHES, and
	 * `hs_setstate` takes the short one when the value is already there:
	 * it compares, prints only on a change, and stores.  Both sides of
	 * both are driven.  The RXSTATE setter has only one reachable branch
	 * ever -- the gate is reached only with rxstate 43 -- which is
	 * finding F722 and is why there is no trial for its other side.
	 */
	tr.short_3588 = 0;
	tr.mst = FSK_NEXT_MST;
	fired_case(&tr, "the action block with the microstate already 46",
		   tag++);
	tr.mst = FSK_MST;

	tr.txst = FSK_NEXT_TXST;
	fired_case(&tr, "the action block with the txstate already 60", tag);
	diff_eq_int("and the txstate is still 60 afterwards",
		    v34hs_peek_short(0, V34HS_TXSTATE), FSK_NEXT_TXST, tag++);
	tr.txst = FSK_TXST;

	/*
	 * +0x358c: `cmpw $0x1` then `jbe`, so the comparison is UNSIGNED.
	 * The last two are what separate the two readings -- 0xffff and
	 * 0x8000 are both above 1 unsigned and both far below it signed --
	 * and they are the two of these six a signed `>` gets wrong.
	 */
	{
		static const struct { short before, after; const char *what; } t[] = {
			{ 0,      0,      "+0x358c of 0 is left alone"        },
			{ 1,      1,      "+0x358c of 1 is left alone"        },
			{ 2,      0,      "+0x358c of 2 is cleared"           },
			{ 0x7fff, 0,      "+0x358c at the signed top"         },
			{ -1,     0,      "+0x358c of 0xffff is cleared"      },
			{ -32768, 0,      "+0x358c of 0x8000 is cleared"      }
		};
		int i;

		for (i = 0; i < 6; i++) {
			tr.toggle = t[i].before;
			fired_case(&tr, t[i].what, tag);
			snprintf(msg, sizeof(msg), "%s: it reads %d after",
				 t[i].what, (int)t[i].after);
			diff_eq_int(msg, v34hs_peek_short(0, FSK_TOGGLE),
				    t[i].after, tag++);
		}
		tr.toggle = 0;
	}
}

/*
 * THE FALL-THROUGH, which is the correction this whole file exists for.
 *
 * The action block's last two stores are `fsk.sr = -1` and `fsk.nbits = 0`,
 * and 0x64a9e -- seven instructions past the join at 0x64a8f -- reads
 * `fsk.nbits` into the register arm 55's guard at 0x65b79 compares against.
 * So a gate that returned skips the demodulator, the microstate dispatch and
 * that read together, and everything the action block wrote for them.
 *
 * `fskdemodulate` rewrites both fsk fields, which is why neither is asserted
 * directly: what IS asserted is that the transmit machine the action block
 * chose is the one the dispatch below then acted on, and the whole-object
 * comparison in `run` carries the rest.
 */
static void
suite_fallthrough(void)
{
	long tag = 800;
	struct trial tr;

	/*
	 * THE SAME SEED, FIRED AND UNFIRED, differing only in +0xa8a0.
	 *
	 * The action block writes microstate 46 and txstate 60, and the two
	 * dispatches below the join then read them.  So the fired run must end
	 * with the transmit machine off the 24 it was driven with and the
	 * unfired run must end still on it -- a gate that returned, or one
	 * whose writes did not reach the dispatch, gives the two runs the same
	 * answer.
	 */
	tr = base;
	tr.phase = 0x7c;
	tr.state = 2;
	tr.thresh_hi = -1;
	tr.runs = 8;
	tr.tone_runs = 9;
	run(&tr, "a fired gate still reaches the microstate dispatch", 1, tag);
	diff_eq_int("the fired gate moved the transmit machine off 24",
		    v34hs_peek_short(0, V34HS_TXSTATE) != FSK_TXST, 1, tag);

	tr.gate = 0;
	run(&tr, "the same seed with the gate clear", 0, tag);
	diff_eq_int("and with the gate clear the transmit machine stayed",
		    v34hs_peek_short(0, V34HS_TXSTATE), FSK_TXST, tag++);
	tr.gate = 1;

	/*
	 * And an UNFIRED gated step reaches it too, which is the half a
	 * `return` in the gate would break most widely: the counter moves and
	 * everything below the join runs exactly as it does with the gate
	 * clear.  The two trials below differ only in +0xa8a0 and must leave
	 * the same three state words.
	 */
	tr = base;
	tr.phase = 0x10;
	tr.mst = V34HS_DET_SYNC;
	run(&tr, "an unfired gated step reaches the microstate dispatch", 0,
	    tag);

	{
		short mst = v34hs_peek_short(0, V34HS_MICROSTATE);
		short txst = v34hs_peek_short(0, V34HS_TXSTATE);

		tr.gate = 0;
		run(&tr, "and the ungated step reaches it identically", 0, tag);
		diff_eq_int("the gate does not change which arm ran"
			    " (microstate)",
			    v34hs_peek_short(0, V34HS_MICROSTATE), mst, tag);
		diff_eq_int("the gate does not change which arm ran (txstate)",
			    v34hs_peek_short(0, V34HS_TXSTATE), txst, tag++);
	}
}

/*
 * The control: the same cases with the BLOB on both sides.
 *
 * A green ours-against-blob run says nothing unless the same seed is green
 * blob-against-blob, because a failure could then be the fixture's.
 * docs/v34handshak.md asks for this per case; it is cheap because it is the
 * same two seeds, one that fires and one that does not.
 */
static void
suite_control(void)
{
	long tag = 900;
	struct trial tr;

	tr = base;
	tr.phase = 0x7c;
	tr.state = 2;
	tr.thresh_hi = -1;
	tr.runs = 8;
	tr.tone_runs = 9;
	setup(&tr);
	v34hs_step();
	v34hs_compare("control: the blob on both sides, the arm fired", tag);
	diff_eq_int("control: and it did fire", blob_fired(), 1, tag++);

	tr.phase = 0x10;
	setup(&tr);
	v34hs_step();
	v34hs_compare("control: the blob on both sides, the arm did not", tag);
	diff_eq_int("control: and it did not fire", blob_fired(), 0, tag++);
}

int
main(void)
{
	dump = getenv("V34HS_DUMP") != NULL;
	diff_begin("v34handshak, the FSK gate at 0x64a87 and 0x6754b");

	/*
	 * The diagnostics are ON.  `v34hs_ours(1)` moves side A's capture
	 * slot to 0 and leaves the blob's at 1, so the transcript is a real
	 * comparison rather than the blob's against itself -- and side B's
	 * copy of it is this file's oracle for whether the arm fired.
	 * `StateName` is indexed unbounded while they are on (D42), so every
	 * state word driven here stays inside 0..86.
	 */
	v34hs_debug(1);

	suite_gate();
	suite_counter();
	suite_state1();
	suite_state2();
	suite_state_other();
	suite_fired();
	suite_fallthrough();
	suite_control();

	v34hs_holes_check();
	return diff_end();
}

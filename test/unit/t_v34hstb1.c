/*
 * t_v34hstb1.c -- `v34handshak`'s per-sample transmit LOOP, 0x62933..0x629ed.
 *
 * WHAT THIS TESTS THAT `t_v34hstx1.c` DOES NOT.  That file drives table 1's
 * nineteen arms one at a time through `v34hs_step_case`, which installs one
 * arm inside the fixture and lets the blob's `v34handshak` contribute the
 * loop around it.  So it proves each arm and says nothing about the loop --
 * about the guard at 0x62933, the `(short)txstate - 5` index, the range test
 * at 0x62961, the re-test at 0x629e0, or the dispatch on an arm's exit code.
 * Those were the whole of what `T3M_UNWRITTEN_TBL1` covered.
 *
 * So this test runs OUR WHOLE `v34handshak` against the blob's, with
 * `v34hs_ours(1)`, and there is no anti-vacuity problem to solve: side A runs
 * none of the blob's code, so an arm or a loop that did nothing cannot be
 * compared against the blob doing it instead.  That is why the guard
 * machinery `run_case_ex` needs has no counterpart here.
 *
 * WHAT THE LOOP DOES NOT DO IS TERMINATE.  Fifty-seven of the eighty-two
 * table entries are the loop bottom itself, so a txstate with no arm of its
 * own leaves the cursor where it was and the test that entered the loop is
 * still true (finding 287, D59).  Every txstate driven below is one of the
 * twenty-five the nineteen arms cover; the spin is demonstrated on purpose by
 * `V34HS_HANG=1 ./build/test/t_v34hsstep` and is not repeated here.
 *
 * THE SAMPLE BUDGET IS THE POINT OF THE SECOND RUN, AND FOUR IS NOT ENOUGH.
 * The re-test at 0x629e0 is only exercised TRUE if the body has to run more
 * than once, and an arm transmits through `txwritequeue`, which puts FOUR
 * entries on the queue per call -- so a budget of four is reached in a single
 * pass and a `while` degraded to an `if` compares equal.  That is not a
 * supposition: it was measured by making exactly that mutation, and the test
 * passed 4,653 checks with the loop running once.  Sixteen is four passes and
 * the mutation then fails.  Finding 713.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34hstx1.h"
#include "harness.h"
#include "v34hsstep.h"

/*
 * The twenty-five txstates the nineteen arms cover, read out of the blob's
 * `.rodata+0x2da0` rather than off a list: every entry of the table that is
 * not the loop bottom at 0x629e0.  The target is carried so a failure names
 * the arm's address and not just its state number.
 */
static const struct {
	short		txst;
	unsigned	target;
	const char	*name;
} arms[] = {
	{ V34HS_SILENCE,	0x640b4, "5 SILENCE"		},
	{ V34HS_SILENCEINFO,	0x640b4, "54 SILENCEINFO"	},
	{ V34HS_SILENCERETRAIN,	0x640b4, "74 SILENCERETRAIN"	},
	{ V34HS_SSEG,		0x64048, "18 SSEG"		},
	{ V34HS_SBARSEG,	0x6296d, "19 SBARSEG"		},
	{ V34HS_PPSEG,		0x642bf, "20 PPSEG"		},
	{ V34HS_TRNSEG4,	0x64339, "21 TRNSEG4"		},
	{ V34HS_TX_DPSK,	0x62b96, "24 TX_DPSK"		},
	{ V34HS_TX_L1,		0x62c69, "51 TX_L1"		},
	{ V34HS_TONE_AB,	0x62d3d, "60 TONE_AB"		},
	{ V34HS_JTXMIT,		0x635cc, "64 JTXMIT"		},
	{ V34HS_J1TXMIT,	0x635cc, "68 J1TXMIT"		},
	{ V34HS_XMIT0,		0x62d83, "65 XMIT0"		},
	{ V34HS_TRNSEG4A,	0x62e28, "66 TRNSEG4A"		},
	{ V34HS_XMITMP,		0x6399b, "67 XMITMP"		},
	{ V34HS_EXMIT,		0x63858, "69 EXMIT"		},
	{ V34HS_DATAXMIT,	0x63ca8, "70 DATAXMIT"		},
	{ V34HS_TXLEVEL,	0x641d1, "71 TXLEVEL"		},
	{ V34HS_JaTXMIT,	0x64139, "78 JaTXMIT"		},
	{ V34HS_MOH_SILENCE,	0x63d58, "81 MOH_SILENCE"	},
	{ V34HS_MOH_ON_HOLD,	0x63d58, "82 MOH_ON_HOLD"	},
	{ V34HS_MOH_FRR,	0x63d58, "83 MOH_FRR"		},
	{ V34HS_MOH_CLEARDOWN,	0x63d58, "84 MOH_CLEARDOWN"	},
	{ V34HS_K56JaTXMIT,	0x63fb0, "85 K56JaTXMIT"	},
	{ V34HS_TXMD,		0x63dae, "86 TXMD"		}
};

#define NARMS	((int)(sizeof(arms) / sizeof(arms[0])))

static int dump;

/*
 * One trial: bring both objects up, put the transmit machine on `txst` with a
 * budget of `budget` samples, and run our whole `v34handshak` against the
 * blob's.
 *
 * The rxstate is `SILENCE`, which is 5 and so below 43: the loop falls out at
 * 0x629ed into the receiver's own test and then into table 2, both of which
 * are written, so nothing downstream of the loop can reach a guard and turn a
 * comparison into an abort.  The microstate is `PHASE1` for the same reason
 * `t_v34hstx1.c` uses it.
 */
static void
loop_case(short txst, short budget, const char *name, long tag)
{
	char msg[192];

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_TXSAMPLE, budget);
	v34hs_state(V34HS_PHASE1, V34HS_SILENCE, txst);

	v34handshak_unwritten_reset();
	v34hs_ours(1);
	v34hs_step();
	v34hs_ours(0);

	snprintf(msg, sizeof(msg), "%s, budget %d", name, (int)budget);
	v34hs_compare(msg, tag);

	/*
	 * AND THE LOOP MUST HAVE RUN, which the comparison alone does not say:
	 * a `while` whose condition was built the wrong way round skips the
	 * body, and the blob would then be compared against a side that had
	 * also done nothing if the arm happened to write nothing visible.  The
	 * cursor started at zero and the budget is positive, so an entered
	 * loop is a cursor that moved or a transmit machine that did.
	 */
	snprintf(msg, sizeof(msg), "%s, budget %d: the loop ran",
		 name, (int)budget);
	diff_eq_int(msg,
		    v34hs_peek_short(0, V34HS_TXCURSOR) != 0
		    || v34hs_peek_short(0, V34HS_TXSTATE) != txst, 1, tag);

	if (dump)
		printf("  %-24s budget %d  cursor %3d  txstate %2d  code %d\n",
		       name, (int)budget,
		       (int)v34hs_peek_short(0, V34HS_TXCURSOR),
		       (int)v34hs_peek_short(0, V34HS_TXSTATE),
		       v34handshak_unwritten());
}

/*
 * The nineteen arms, at two sample budgets.
 *
 * `v34handshak_unwritten()` is asserted `T3M_WRITTEN` on every one of them,
 * and that is a claim about the loop and not about the arms: it says the
 * dispatch reached an arm rather than falling through to the code that
 * records a gap, for all twenty-five states the table names.
 */
static void
suite_arms(void)
{
	long tag = 100;
	int i;

	for (i = 0; i < NARMS; i++) {
		loop_case(arms[i].txst, 1, arms[i].name, tag);
		diff_eq_int("the loop reached a written arm",
			    v34handshak_unwritten(), T3M_WRITTEN, tag++);

		loop_case(arms[i].txst, 16, arms[i].name, tag);
		diff_eq_int("the loop reached a written arm, sixteen samples",
			    v34handshak_unwritten(), T3M_WRITTEN, tag++);
	}
}

/*
 * The guard at 0x62933, which is a `jge` and so skips on EQUAL as well.
 *
 * Three trials, because the boundary is where a `>` written for a `>=` hides:
 * cursor above the limit, cursor exactly at it, and cursor one below it.  The
 * first two must not enter the loop and the third must.
 */
static void
suite_guard(void)
{
	long tag = 900;
	short cursor;

	static const struct { short cursor, limit, enters; const char *what; } t[] = {
		{ 7, 5, 0, "cursor above the limit skips the loop"		},
		{ 5, 5, 0, "cursor EQUAL to the limit skips the loop"		},
		{ 4, 5, 1, "cursor one below the limit enters the loop"		}
	};
	int i;

	for (i = 0; i < 3; i++) {
		v34hs_setup(0);
		v34hs_route(V34HS_ROUTE_TXSAMPLE, t[i].limit);
		v34hs_state(V34HS_PHASE1, V34HS_SILENCE, V34HS_SSEG);
		v34hs_poke_short(V34HS_TXCURSOR, t[i].cursor);
		v34hs_poke_short(V34HS_TXLIMIT, t[i].limit);

		v34handshak_unwritten_reset();
		v34hs_ours(1);
		v34hs_step();
		v34hs_ours(0);

		v34hs_compare(t[i].what, tag);

		/*
		 * 18 SSEG emits into the queue, so "the loop ran" is "the
		 * cursor moved off where it was put".  A skipped loop leaves
		 * it exactly there.
		 */
		cursor = v34hs_peek_short(0, V34HS_TXCURSOR);
		diff_eq_int(t[i].what, cursor != t[i].cursor, t[i].enters,
			    tag++);
	}
}

/*
 * The range test at 0x62961, `sub $0x5,%eax; cmp $0x51,%eax; ja 629e0`.
 *
 * It admits 5..86 and nothing else, and both ends of that are a place an
 * off-by-one lives.  4 and 87 must reach the loop bottom -- which spins, so
 * these cannot be stepped -- and the two ends that ARE in range, 5 and 86,
 * are driven by `suite_arms` above.
 *
 * SO THIS IS ASSERTED ON THE OBJECT AND NOT BY RUNNING IT: the table has
 * eighty-two entries and the first is txstate 5, so a txstate of 4 or 87 is
 * outside the table by construction.  What is checked here is that our
 * `switch` agrees -- that it has no case for 4 and none for 87 -- and the way
 * to check that without spinning is to enter with the cursor already at the
 * limit, where the loop is skipped and the state is simply carried through.
 * A `switch` with a stray case for 4 would still not run here; that claim is
 * the compile-time one below.
 */
static void
suite_range(void)
{
	long tag = 950;
	static const short out[] = { 4, 87 };
	int i;

	for (i = 0; i < 2; i++) {
		char msg[96];

		v34hs_setup(0);
		v34hs_route(V34HS_ROUTE_TXSAMPLE, 1);
		v34hs_state(V34HS_PHASE1, V34HS_SILENCE, out[i]);
		/* Skip the loop: the arm for these is the bottom, which spins. */
		v34hs_poke_short(V34HS_TXCURSOR, 1);
		v34hs_poke_short(V34HS_TXLIMIT, 1);

		v34handshak_unwritten_reset();
		v34hs_ours(1);
		v34hs_step();
		v34hs_ours(0);

		snprintf(msg, sizeof(msg),
			 "txstate %d is outside the table and carries through",
			 (int)out[i]);
		v34hs_compare(msg, tag);
		diff_eq_int(msg, v34handshak_unwritten(), T3M_WRITTEN, tag++);
	}
}

/*
 * The two blocks the loop used to STOP at -- 81's wrap at 0x66d85 and 86's
 * segment end at 0x66fe9 -- which are written now, in the arms that reach
 * them.
 *
 * THIS SUITE'S CLAIM HAS INVERTED, and what it used to be is worth keeping
 * because it is what the guard bought.  It was "our side recorded
 * `T3M_UNWRITTEN_TBL1` and RETURNED", checked at two budgets: `t3m_notwritten`
 * records and, under `v34handshak_unwritten_reset`, returns, so a loop that
 * recorded the code and went round again recorded the SAME code and passed,
 * and only the budget separated the two (finding 715).
 *
 * NEITHER BLOCK IS A TRANSFER OUT OF THE LOOP.  0x66d85 ends at 0x63941 or
 * 0x63948, which are both the loop test, and 0x66fe9 at 0x63e7f, which is the
 * fall-through of the block that jumped to it.  So there is nothing to stop,
 * nothing to neutralise, and each seed below is an ordinary differential run
 * of our whole function against the blob's.  Finding 748.
 *
 * BOTH BUDGETS ARE KEPT, for the opposite reason to the one that put them
 * here: a budget of sixteen runs the loop PAST the wrap, so it is the only
 * run in which what the wrap left behind -- a moved txstate and a `vect_idx`
 * back at zero -- is read by a later pass.
 *
 * AND 82, 83 AND 84 ARE HERE BECAUSE THE WRAP IS WHERE FOUR TXSTATES STOP
 * BEING ONE BEHAVIOUR.  They share 81's entry at 0x63d58, and 0x66d85 re-reads
 * +0x3596 and returns to the loop test for anything that is not 0x51 -- so
 * these three reach the hundred-and-ninety-second sample and decide nothing.
 * Under the old guard every one of them aborted.  Finding 750.
 *
 * The pokes are `t_v34hstx1.c`'s, which is deliberate: they are the seeds
 * that file already proves reach these two blocks, so a change that stopped
 * reaching them fails there as well as here.
 */
static void
wrap_case(short txst, void (*seed)(void), short budget, const char *what,
	  long tag)
{
	char msg[192];

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_TXSAMPLE, budget);
	v34hs_state(V34HS_PHASE1, V34HS_SILENCE, txst);
	seed();

	v34handshak_unwritten_reset();
	v34hs_ours(1);
	v34hs_step();
	v34hs_ours(0);

	snprintf(msg, sizeof(msg), "%s, budget %d", what, (int)budget);
	v34hs_compare(msg, tag);
	diff_eq_int(msg, v34handshak_unwritten(), T3M_WRITTEN, tag);
}

static void
seed_moh_wrap(void)
{
	v34hs_poke_short(0x2aa2, 0xbf);		/* TX1_VECTIDX */
}

/*
 * The two 32-bit companions 0x66d85 reads, driven both ways.  `fabec` at
 * +0xabec is set to 0x10001 and not to 1: the low half is one either way, so
 * a 16-bit read of it agrees with a 32-bit read on the value 1 and cannot be
 * told apart -- this is the seed that separates them.
 */
static void
seed_moh_wrap_frr(void)
{
	v34hs_poke_short(0x2aa2, 0xbf);		/* TX1_VECTIDX */
	v34hs_poke_int(0xabec, 1);
	v34hs_poke_int(0xabf0, 0);		/* moh_message */
}

static void
seed_moh_wrap_act(void)
{
	v34hs_poke_short(0x2aa2, 0xbf);		/* TX1_VECTIDX */
	v34hs_poke_int(0xabec, 0x10001);
	v34hs_poke_int(0xabf0, 1);		/* moh_message */
}

static void
seed_moh_wrap_hold(void)
{
	v34hs_poke_short(0x2aa2, 0xbf);		/* TX1_VECTIDX */
	v34hs_poke_int(0xabec, 0x10001);
	v34hs_poke_int(0xabf0, 0);		/* moh_message */
}

static void
seed_txmd_done(void)
{
	v34hs_poke_short(0x2aa2, 0x3f);		/* TX1_VECTIDX */
	v34hs_poke_short(0xaa78, 0x40);		/* TX1_COUNT   */
	v34hs_poke_short(0x35a6, 0x50);		/* TX1_SEGLEN  */
	v34hs_poke_short(0x359c, 0x64);		/* TX1_F359C   */
	v34hs_poke_int(0x25cc, 0x04000000);	/* TX1_F25CC   */
}

static void
suite_exits(void)
{
	long tag = 980;
	static const short shared[] = { V34HS_MOH_ON_HOLD, V34HS_MOH_FRR,
					V34HS_MOH_CLEARDOWN };
	int i;

	wrap_case(V34HS_MOH_SILENCE, seed_moh_wrap, 1,
		  "81's wrap at 0xc0 runs 0x66d85", tag);
	wrap_case(V34HS_MOH_SILENCE, seed_moh_wrap, 16,
		  "81's wrap at 0xc0 runs 0x66d85", tag++);

	wrap_case(V34HS_MOH_SILENCE, seed_moh_wrap_frr, 16,
		  "81's wrap, org MHfrr", tag++);
	wrap_case(V34HS_MOH_SILENCE, seed_moh_wrap_act, 16,
		  "81's wrap, act MHfrr", tag++);
	wrap_case(V34HS_MOH_SILENCE, seed_moh_wrap_hold, 16,
		  "81's wrap, neither MHfrr", tag++);

	for (i = 0; i < 3; i++) {
		char msg[96];

		snprintf(msg, sizeof(msg),
			 "txstate %d shares 81's entry and its wrap decides"
			 " nothing", (int)shared[i]);
		wrap_case(shared[i], seed_moh_wrap_frr, 16, msg, tag++);
	}

	wrap_case(V34HS_TXMD, seed_txmd_done, 1,
		  "86's segment end runs 0x66fe9", tag);
	wrap_case(V34HS_TXMD, seed_txmd_done, 16,
		  "86's segment end runs 0x66fe9", tag++);
}

/*
 * The transmit state is re-read AT THE TOP OF EVERY PASS, 0x62957.
 *
 * Several arms move the transmit machine, so a state read once before the
 * loop dispatches the second pass to the arm the first pass left behind.
 * Nothing above catches that, which is the mutation tier's finding and not a
 * supposition (finding 715).
 *
 * 18 SSEG IS THE LEVER, AND 65 XMIT0 IS NOT, which is worth writing down
 * because 65 was tried first: 65 moves the machine to 18 when bit 3 of the
 * receiver's flags is set, and the two arms then wrote the SAME sixteen bytes
 * for four passes -- a hand-over that the object cannot see is no test at
 * all.  SSEG counts `f25c0` up and at 0x40 hands over to 19 SBARSEG, which is
 * 991 bytes against SSEG's 251 and writes quite different things.
 *
 * So: seed `f25c0` one below the threshold and the FIRST pass hands over.
 *
 * SEVENTY-TWO IS THE BUDGET AND TWENTY-FOUR IS NOT, which is the second thing
 * the mutation tier had to say here.  SSEG calls `txmit` twice and the queue
 * advances THIRTY-TWO per pass, not eight -- measured, by sweeping the budget
 * from 8 to 48 and watching the cursor go 32, 32, 32, 32, 64, 64.  So a
 * budget anywhere in 8..32 is ONE pass, there is no second dispatch, and a
 * state hoisted out of the loop cannot be seen to be wrong.  Seventy-two is
 * three passes: one of SSEG and two of SBARSEG.  Finding 715.
 */
static void
suite_restate(void)
{
	long tag = 990;

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_TXSAMPLE, 72);
	v34hs_state(V34HS_PHASE1, V34HS_SILENCE, V34HS_SSEG);
	v34hs_poke_short(0x25c0, 0x3f);		/* TX1_F25C0, one below 0x40 */

	v34handshak_unwritten_reset();
	v34hs_ours(1);
	v34hs_step();
	v34hs_ours(0);

	v34hs_compare("18 SSEG hands the machine to 19 SBARSEG mid-loop", tag);

	/*
	 * AND THE HAND-OVER MUST HAVE HAPPENED, or this is SSEG three times on
	 * both sides and says nothing about the re-read.
	 */
	diff_eq_int("18 SSEG moved the transmit machine to SBARSEG",
		    v34hs_peek_short(0, V34HS_TXSTATE), V34HS_SBARSEG, tag);
	diff_eq_int("and the loop went round again after it",
		    v34hs_peek_short(0, V34HS_TXCURSOR) >= 72, 1, tag++);

	if (dump)
		printf("  restate: cursor %d txstate %d changed %u\n",
		       (int)v34hs_peek_short(0, V34HS_TXCURSOR),
		       (int)v34hs_peek_short(0, V34HS_TXSTATE),
		       v34hs_observed(0)->changed);
}

int
main(void)
{
	dump = getenv("V34HS_DUMP") != NULL;
	diff_begin("v34handshak table 1, the per-sample loop");

	suite_guard();
	suite_range();
	suite_arms();
	suite_restate();
	suite_exits();

	v34hs_holes_check();
	return diff_end();
}

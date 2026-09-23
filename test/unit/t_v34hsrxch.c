/*
 * t_v34hsrxch.c -- `v34handshak`'s rxstate compare chain, 0x62a02 and 0x62b71.
 *
 * The chain has no table.  It is `cmp`/`je` against the rxstate at +0x3594,
 * and it has six exits: 43 RX_DPSK to the microstate machine, 4 RECEIVE to
 * 0x653e4, 35 WAIT to 0x6752c, 53 DET_AB to 0x65473, 72 RX_L1 to 0x650c6, and
 * -- for EVERYTHING ELSE, by two different doors -- the once-per-block
 * transmit dispatch at 0x62af1.
 *
 * WHAT THIS TEST IS FOR.  Two of those six are still unwritten, so the point
 * is not "the chain is finished": it is that the routing itself is now the
 * object's, and that the two "everything else" doors need no new code at all.
 * `t3c_txblock` has been written since table 2 landed, so every rxstate below
 * 43 except 4, and every rxstate above 43 except 72, is COMPLETE.
 *
 * TWO COUNTS, AND THE SWEEP BELOW ASSERTS THE SECOND.  82 of the eighty-seven
 * reach a written exit through one of the two DEFAULT doors -- all but 4 and
 * 72, which are guarded, and all but 43, 35 and 53, which have arms of their
 * own.  85 are written altogether, those 82 plus 43, 35 and 53.  Finding F717,
 * and finding F725 for 53's move from the guarded column to the written one.
 *
 * THE `jg` IS WHY THIS IS A SWEEP AND NOT FIVE TRIALS.  0x62a12 branches to
 * the second chain before the compares against 4 and 35 are reached, so a
 * chain rewritten with the compares in a different order behaves identically
 * on the five named states and differently on some unnamed one.  Driving
 * every rxstate 0..86 is what makes the ORDER testable rather than just the
 * five destinations.
 */

#include <stdio.h>
#include <stdlib.h>

#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "harness.h"
#include "v34hsstep.h"

/*
 * The txstate every trial is driven with.
 *
 * It has to be one TABLE 2 actually acts on, or the sweep is eighty-seven
 * comparisons of one dispatch doing nothing.  24 is one of the five that
 * share table 2's 0x644c9, which `t_v34hstbl2.c` already proves, and it is
 * not 6 -- table 2's default -- for exactly that reason.
 *
 * AND TABLE 1 CANNOT RUN ON ANY TRIAL IN THIS FILE, which is worth saying
 * because 24 is also table 1's `TX_DPSK` and `suite_wait_txsweep` below drives
 * all eighty-seven txstates -- fifty-seven of which are table 1's
 * non-terminating default (finding F287).  `V34HS_ROUTE_RXCHAIN` pins the
 * cursor AND the limit at zero, so the entry test at 0x62933 is `0 >= 0` and
 * the per-sample loop is skipped before any txstate is read.  Every txstate
 * here is therefore a table-2 index and never a table-1 one; nothing in this
 * file can spin.
 */
#define RXCH_TXSTATE	V34HS_TX_DPSK

static int dump;

/*
 * The four rxstates that leave the chain for somewhere of their own.
 *
 * 43 is the microstate machine, which landed long ago, and is here so that
 * the one state the chain treats specially and completely is asserted to be
 * complete, rather than being absent from the table and so indistinguishable
 * from an oversight.  4 RECEIVE, 53 DET_AB and 72 RX_L1 joined it when their
 * arms landed -- so all four run a written arm and are compared whole.
 */
static const struct {
	short		rxst;
	const char	*name;
	const char	*what;
} named[] = {
	{ V34HS_RX_DPSK, "43 RX_DPSK",
	  "reaches the microstate machine"	},
	{ V34HS_RECEIVE, "4 RECEIVE",
	  "runs the arm t_v34hsrx4.c owns"	},
	{ V34HS_DET_AB,	 "53 DET_AB",
	  "runs the arm t_v34hsrx53.c owns"	},
	{ V34HS_RX_L1,	 "72 RX_L1",
	  "runs the arm t_v34hsrx72.c owns"	}
};

#define NNAMED	((int)(sizeof(named) / sizeof(named[0])))

/*
 * Is this rxstate one the chain sends somewhere that is not written?
 *
 * NOTHING IS, ANY MORE.  35 WAIT never was -- its arm is four instructions
 * and is written below -- and 72 RX_L1 was the last, until `t72_rx_l1`
 * landed.  The predicate stays rather than being deleted with its last
 * member, because it is what the sweep below consults and a sweep with no
 * exclusion mechanism cannot be given one again cheaply; it now returns 0
 * for everything, which is the claim.
 */
static int
unwritten(short rxst)
{
	(void)rxst;
	return 0;
}

/*
 * Every rxstate that reaches a written exit, compared whole.
 *
 * 0..86 is `StateName`'s whole range, and the fixture requires every state
 * word to stay inside it while the diagnostics are on -- `StateName` is
 * indexed unbounded (D42), so a state outside it is a wild `char *` in
 * `v34handshakinit`'s own logging and the fixture faults rather than fails.
 */
static void
suite_sweep(void)
{
	long tag = 1000;
	short rxst;
	char msg[128];
	int written = 0, guarded = 0;

	for (rxst = 0; rxst <= 86; rxst++) {
		if (unwritten(rxst)) {
			guarded++;
			continue;
		}

		v34hs_setup(0);
		v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
		v34hs_state(V34HS_PHASE1, rxst, RXCH_TXSTATE);

		v34hs_ours(1);
		v34hs_step();
		v34hs_ours(0);

		snprintf(msg, sizeof(msg), "rxstate %d reaches a written exit",
			 (int)rxst);
		v34hs_compare(msg, tag++);
		written++;
	}

	/*
	 * AND THE TWO COUNTS ARE ASSERTED, because a sweep that silently
	 * skipped most of its range reads exactly like a sweep that passed.
	 * gates.md's rule 1: make the tool count what it examined.
	 */
	diff_eq_int("the sweep drove every rxstate 0..86 that is written",
		    written, 87, tag);
	diff_eq_int("and skipped none, because none is unwritten",
		    guarded, 0, tag++);

	if (dump)
		printf("  sweep: %d written, %d guarded\n", written, guarded);
}

/*
 * The four the chain names, and the code each must record.
 */
static void
suite_named(void)
{
	long tag = 1200;
	char msg[160];
	int i;

	for (i = 0; i < NNAMED; i++) {
		v34hs_setup(0);
		v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
		v34hs_state(V34HS_PHASE1, named[i].rxst, RXCH_TXSTATE);

		v34hs_ours(1);
		v34hs_step();
		v34hs_ours(0);

		snprintf(msg, sizeof(msg), "%s %s",
			 named[i].name, named[i].what);
		v34hs_compare(msg, tag++);
	}
}

/*
 * 35 WAIT, 0x6752c, and it is the whole arm.
 *
 *   6752c  mov  0x74(%esp),%ebp     ; the receiver, obj + 0x264
 *   67530  mov  %ebp,(%esp)
 *   67533  call rxreadqueue
 *   67538  mov  0xc0(%esp),%edx
 *   6753f  movzwl 0x3596(%edx),%ecx ; reload txstate
 *   67546  jmp  62af1               ; the transmit dispatch
 *
 * `rxreadqueue` takes four entries off the queue, so the receiver's count has
 * to be ABOVE four for the call to be visible -- and the route already puts
 * it at 6, because the chain is only reached when the count is above 5
 * (0x629f1).  The second trial drives it higher still so the arm is not
 * tested only at the one value the route happens to choose.
 */
static void
suite_wait(void)
{
	long tag = 1300;
	static const short counts[] = { 6, 8, 16, 64 };
	char msg[128];
	int i;

	for (i = 0; i < 4; i++) {
		short before, after;

		v34hs_setup(0);
		v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
		v34hs_state(V34HS_PHASE1, V34HS_WAIT, RXCH_TXSTATE);
		v34hs_poke_short(V34HS_RXCOUNT, counts[i]);
		before = counts[i];
		(void)before;

		v34hs_ours(1);
		v34hs_step();
		v34hs_ours(0);

		snprintf(msg, sizeof(msg),
			 "35 WAIT drains the receive queue, count %d",
			 (int)counts[i]);
		v34hs_compare(msg, tag);

		/*
		 * AND THE DRAIN MUST HAVE HAPPENED.  Without this the trial
		 * passes for an arm that only called `t3c_txblock`, which is
		 * what the state would do if it fell through to the chain's
		 * default -- the one wrong answer that is hardest to see,
		 * because the tail is the same on both routes.
		 */
		after = v34hs_peek_short(0, V34HS_RXCOUNT);
		snprintf(msg, sizeof(msg),
			 "35 WAIT took four off a queue of %d", (int)counts[i]);
		diff_eq_int(msg, after, (short)(before - 4), tag++);

		if (dump)
			printf("  35 WAIT: count %2d -> %2d\n",
			       (int)before, (int)after);
	}
}

/*
 * 35 WAIT against every txstate, because the ORDER of its two acts is only
 * visible to a transmit arm that reads what `rxreadqueue` wrote.
 *
 * The object drains the queue and THEN jumps to the transmit dispatch, and
 * `rxreadqueue` does not just move a cursor -- it lifts four entries into the
 * receiver's own shorts.  Whether swapping the two is observable therefore
 * depends entirely on which table-2 arm runs, so the honest test is all
 * eighty-seven rather than the one txstate the rest of this file uses.
 * Finding F718.
 */
static void
suite_wait_txsweep(void)
{
	long tag = 1400;
	short txst;
	char msg[128];

	for (txst = 0; txst <= 86; txst++) {
		v34hs_setup(0);
		v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
		v34hs_state(V34HS_PHASE1, V34HS_WAIT, txst);
		v34hs_poke_short(V34HS_RXCOUNT, 16);

		v34hs_ours(1);
		v34hs_step();
		v34hs_ours(0);

		snprintf(msg, sizeof(msg),
			 "35 WAIT then the transmit dispatch, txstate %d",
			 (int)txst);
		v34hs_compare(msg, tag++);
	}
}

int
main(void)
{
	dump = getenv("V34HS_DUMP") != NULL;
	diff_begin("v34handshak, the rxstate compare chain");

	suite_sweep();
	suite_named();
	suite_wait();
	suite_wait_txsweep();

	v34hs_holes_check();
	return diff_end();
}

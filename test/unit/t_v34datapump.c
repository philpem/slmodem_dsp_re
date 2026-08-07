/*
 * t_v34datapump.c -- `datapumpv34` and the two RRN counters, against the blob.
 *
 * `datapumpv34` is the datapump's per-block entry point and the last function
 * of V34hshak.c.  It is a supervisor rather than a signal path: three loops
 * that call `v34handshak`, `modulatevector` and `receiver`, and four
 * independent `if`s afterwards that retrain or renegotiate on the receiver's
 * consecutive-error counters.
 *
 * SIDE A RUNS `datapumpv34` AND SIDE B `ref_datapumpv34`, through
 * `v34hs_entry`.  That is a new door into test/harness/v34hsstep.c and the
 * reason it is worth cutting is findings 319-322: what an object step depends
 * on is the geometry of the five blocks the object points at, and this
 * function steps the same object `v34handshak` does.  A second fixture would
 * be that geometry built a second time.
 *
 * WHAT THIS TEST CANNOT DRIVE, AND WHY IT IS NOT A CHOICE.
 *
 * The +0x2218 > 1 branch calls `v34handshak` until the transmit block is full
 * AND the receive queue is drained -- and `v34handshak`'s own prologue reads
 * those same two fields to pick its dispatch.  So:
 *
 *   cursor < limit          -> table 1, the per-sample loop, whose arms live
 *                              in v34hstx1.cpp as separate entry points and
 *                              are not wired into `v34handshak`; it halts.
 *   cursor >= limit,
 *   receiver count > 5      -> the rxstate chain, and no arm anybody has
 *                              written lowers the receive count or raises the
 *                              cursor, so the loop cannot terminate.
 *
 * Every iteration of that loop therefore either halts in `t3c_unwritten` or
 * spins.  What IS drivable is the branch with the loop condition already
 * false, which is a real case -- the object reaches it whenever a block
 * completes -- and it is driven below with the tail's triggers armed, so a
 * reconstruction that took the wrong branch would retrain and be caught.
 * Finding 452 records the rest.
 *
 * THE OTHER TWO LOOPS DO RUN.  `modulatevector` advances the transmit cursor
 * four samples a call through `txmit`, and `receiver` drains the receive
 * queue through `V34demodulate`, so both terminate on their own.
 *
 * THE ORACLES ARE READ OFF THE DISASSEMBLY, not off our code: each case says
 * what the BLOB must leave in +0x2218, in the receiver's +0x25e, and in the
 * two RRN counters at +0xac0e and +0xac10.  Two arms that both did nothing
 * would agree perfectly, so agreement alone is not evidence that the right
 * arm ran.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "v34hsstep.h"

#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34pcmif.h"

extern void ref_datapumpv34(void *obj);
extern void ref_VPcmV34IndicateLocalRRN(void *obj);
extern void ref_VPcmV34IndicateRemoteRRN(void *obj);

/* --- the object, by offset, so the test reads as the disassembly does ------ */

#define O_PROGRESS	0x0004	/* int:   5 when the sample clock is stale  */
#define O_TIMER		0x0238	/* int:   the running sample count          */
#define O_MARK		0x0248	/* int:   the instant spans are measured from*/
#define O_MODE		0x2218	/* int:   above 1 is the handshake          */
#define O_TXCUR		0x221c	/* short: samples emitted this block        */
#define O_TXLIM		0x2aa0	/* short: how many the block wants          */
#define O_RXCNT		0x0264	/* short: the receive queue's count         */
#define O_FAA96		0xaa96	/* short: the block rate the thresholds scale*/
#define O_FAA98		0xaa98	/* short: copied into the receiver's +0x260 */
#define O_RRNLOC	0xac0e	/* short: VPcmV34IndicateLocalRRN's counter */
#define O_RRNREM	0xac10	/* short: ...and the remote one             */

/* The receiver, at +0x264, whose own offsets these are plus that base. */
#define O_FLAGS		(0x264 + 0x122)	/* bit 6 retrain, bit 5 remote RRN  */
#define O_BLOCKS	(0x264 + 0x124)	/* blocks received, capped          */
#define O_ERR		(0x264 + 0x21a)	/* the block's error measure        */
#define O_THR_A		(0x264 + 0x252)
#define O_THR_B		(0x264 + 0x254)
#define O_THR_C		(0x264 + 0x256)
#define O_BAD		(0x264 + 0x258)
#define O_BAD_LONG	(0x264 + 0x25a)
#define O_GOOD		(0x264 + 0x25c)
#define O_F21C		(0x264 + 0x21c)	/* receiver: wraps at 0x400, and the
					   wrap is what refreshes +0x21a    */
#define O_WHY		(0x264 + 0x25e)	/* 1 remote, 2 down, 3 up           */
#define O_RATE		(0x264 + 0x260)

/*
 * The three spans, and the two thresholds that are NOT a multiple of the
 * block rate.  Spelled out here as well as in the source so that a mutation
 * of either has to be made in two places to go unnoticed.
 */
#define SPAN_STALE	288000
#define SPAN_MID	144000
#define SPAN_LONG	1152000
#define BLOCK_CAP	0x752f

/* --- a case --------------------------------------------------------------- */

struct poke {
	unsigned	off;
	int		sz;	/* 2 or 4; 0 ends the list */
	int		v;
};

#define PK_END		{ 0, 0, 0 }
#define NOPOKE		((const struct poke *)NULL)

#define KEEP		0x7fffffff	/* the oracle: this field must not move */

struct oracle {
	int	mode;		/* +0x2218 afterwards, or KEEP            */
	int	why;		/* receiver +0x25e afterwards, or KEEP     */
	int	dloc;		/* how far +0xac0e moved                   */
	int	drem;		/* ...and +0xac10                          */
	int	progress;	/* +0x0004 afterwards, or KEEP             */
};

static int dump;
static int debug_on;

/* What each trial did, per diagnostics setting, for the claims at the end. */
struct rec {
	unsigned	changed, hash, lines;
	int		err, blocks;
	int		used;
};

static struct rec seen[2][64];
static int trial_now;

static int
peek_int(int side, unsigned off)
{
	int v;

	memcpy(&v, (char *)v34hs_object(side) + off, sizeof(v));
	return v;
}

/*
 * The base every case starts from: nothing armed, no loop entered, no span
 * elapsed, and the four fields the tail writes holding values no arm of it
 * could produce, so that "this arm did not run" is a claim the object can
 * carry rather than an absence.
 *
 * NOT ZEROES (finding 230): 0x5a5a in +0x25e and 0x3c3c in +0x260 are
 * distinguishable from every value the four blocks write, which are 1, 2, 3
 * and whatever +0xaa98 holds.
 */
static const struct poke base[] = {
	{ O_MODE,	4, 0 },
	{ O_TXCUR,	2, 12 },
	{ O_TXLIM,	2, 12 },
	{ O_RXCNT,	2, 5 },
	{ O_TIMER,	4, 0x00100000 },
	{ O_MARK,	4, 0x00100000 },
	{ O_PROGRESS,	4, 0x0badf00d },
	{ O_FLAGS,	2, 0x1a95 },	/* bits 6 and 5 clear, the rest varied */
	{ O_BLOCKS,	2, 0x0123 },
	{ O_ERR,	2, 0x2000 },
	{ O_THR_A,	2, 0x3000 },
	{ O_THR_B,	2, 0x3000 },
	{ O_THR_C,	2, 0x1000 },
	{ O_BAD,	2, 0 },
	{ O_BAD_LONG,	2, 0 },
	{ O_GOOD,	2, 0 },
	{ O_WHY,	2, 0x5a5a },
	{ O_RATE,	2, 0x3c3c },
	{ O_FAA96,	2, 100 },
	{ O_FAA98,	2, -1234 },
	{ O_RRNLOC,	2, 0x1111 },
	{ O_RRNREM,	2, 0x2222 },
	PK_END
};

static void
apply(const struct poke *p)
{
	for (; p != NULL && p->sz != 0; p++) {
		if (p->sz == 2)
			v34hs_poke_short(p->off, (short)p->v);
		else
			v34hs_poke_int(p->off, p->v);
	}
}

/*
 * Drive one case and check both axes: the whole object and its five blocks
 * byte for byte between the two sides, and the blob's own answer against the
 * oracle.
 */
static void
run(const char *name, const struct poke *pk, const struct oracle *ex, long tag)
{
	const struct v34hs_obs *o;
	int pre_loc, pre_rem;
	char msg[192];

	v34hs_setup(0);
	apply(base);
	apply(pk);

	pre_loc = (int)v34hs_peek_short(1, O_RRNLOC);
	pre_rem = (int)v34hs_peek_short(1, O_RRNREM);

	v34hs_step();
	v34hs_compare(name, tag);

	if (ex->mode != KEEP) {
		snprintf(msg, sizeof(msg),
			 "%s: the blob leaves +0x2218 at %d", name, ex->mode);
		diff_eq_int(msg, peek_int(1, O_MODE), ex->mode, tag);
	}
	if (ex->why != KEEP) {
		snprintf(msg, sizeof(msg),
			 "%s: the blob leaves the receiver's +0x25e at %d",
			 name, ex->why);
		diff_eq_int(msg, (int)v34hs_peek_short(1, O_WHY), ex->why,
			    tag);
	}
	if (ex->progress != KEEP) {
		snprintf(msg, sizeof(msg),
			 "%s: the blob leaves +0x0004 at %d", name,
			 ex->progress);
		diff_eq_int(msg, peek_int(1, O_PROGRESS), ex->progress, tag);
	}
	snprintf(msg, sizeof(msg), "%s: +0xac0e moved by %d", name, ex->dloc);
	diff_eq_int(msg, (int)v34hs_peek_short(1, O_RRNLOC) - pre_loc,
		    ex->dloc, tag);
	snprintf(msg, sizeof(msg), "%s: +0xac10 moved by %d", name, ex->drem);
	diff_eq_int(msg, (int)v34hs_peek_short(1, O_RRNREM) - pre_rem,
		    ex->drem, tag);

	o = v34hs_observed(0);
	seen[debug_on][trial_now].changed = o->changed;
	seen[debug_on][trial_now].hash = o->hash;
	seen[debug_on][trial_now].lines = o->lines;
	seen[debug_on][trial_now].err = (int)v34hs_peek_short(1, O_ERR);
	seen[debug_on][trial_now].blocks = (int)v34hs_peek_short(1, O_BLOCKS);
	seen[debug_on][trial_now].used = 1;

	if (dump)
		printf("  %-46s wrote %5u B  sig %08x  lines %u  "
		       "+0x2218 %d  +0x25e %d  +0x21a %d  "
		       "+0x124 %d  +0x258/a/c %d/%d/%d\n",
		       name, o->changed, o->hash, o->lines,
		       peek_int(1, O_MODE), (int)v34hs_peek_short(1, O_WHY),
		       (int)v34hs_peek_short(1, O_ERR),
		       (int)v34hs_peek_short(1, O_BLOCKS),
		       (int)v34hs_peek_short(1, O_BAD),
		       (int)v34hs_peek_short(1, O_BAD_LONG),
		       (int)v34hs_peek_short(1, O_GOOD));
}


/*
 * The same case with the BLOB ON BOTH SIDES.  A green ours-against-blob run
 * says nothing unless the same seed is green blob-against-blob, because then
 * the disagreement could be the fixture's; docs/v34handshak.md says so for
 * the per-case forms and it is no less true for this one.
 */
static void
control(const char *name, const struct poke *pk, long tag)
{
	char msg[192];

	v34hs_entry(ref_datapumpv34, ref_datapumpv34, 1);
	v34hs_setup(0);
	apply(base);
	apply(pk);
	v34hs_step();
	snprintf(msg, sizeof(msg), "control (blob both sides): %s", name);
	v34hs_compare(msg, tag);
	v34hs_entry(datapumpv34, ref_datapumpv34, 0);
}

/* --- the cases ------------------------------------------------------------ */

/*
 * THE HANDSHAKE BRANCH, with the loop condition already false.
 *
 * Every one of these arms the tail as hard as it can be armed -- bit 6 of
 * +0x122 set, the bad-block run far over half the block rate, both
 * renegotiation counters over their thresholds -- and then asserts that
 * NOTHING happened.  That is what makes it a test of the branch rather than
 * of silence: a reconstruction that fell through to the data path would
 * retrain, renegotiate twice and print, and every one of the five oracles
 * would move.
 */
static const struct poke pk_hs_armed[] = {
	{ O_MODE,	4, 2 },
	{ O_FLAGS,	2, 0x1af5 },		/* bits 6 and 5 both set */
	{ O_BAD,	2, 30000 },
	{ O_BAD_LONG,	2, 30000 },
	{ O_GOOD,	2, 30000 },
	PK_END
};

static const struct poke pk_hs_mode3[] = {
	{ O_MODE,	4, 3 },
	{ O_FLAGS,	2, 0x1af5 },
	{ O_BAD,	2, 30000 },
	PK_END
};

static const struct poke pk_hs_huge[] = {
	{ O_MODE,	4, -1 },		/* unsigned, so far above 1 */
	{ O_FLAGS,	2, 0x1af5 },
	{ O_BAD,	2, 30000 },
	PK_END
};

/*
 * And +0x2218 == 1 and 0, which are the SAME pokes down the other branch.
 * The pair is the whole content of the guard: one field, two values, two
 * completely different answers.
 */
static const struct poke pk_hs_one[] = {
	{ O_MODE,	4, 1 },
	{ O_BAD_LONG,	2, 201 },
	PK_END
};

static const struct poke pk_hs_zero[] = {
	{ O_MODE,	4, 0 },
	{ O_BAD_LONG,	2, 201 },
	PK_END
};

/* The same trigger at mode 2, which must do nothing at all. */
static const struct poke pk_hs_two[] = {
	{ O_MODE,	4, 2 },
	{ O_BAD_LONG,	2, 201 },
	PK_END
};

/* --- the stale-clock guard at the top ------------------------------------- */

static const struct poke pk_stale_under[] = {
	{ O_MODE,	4, 2 },
	{ O_TIMER,	4, 0x00100000 + SPAN_STALE },
	{ O_MARK,	4, 0x00100000 },
	PK_END
};

static const struct poke pk_stale_over[] = {
	{ O_MODE,	4, 2 },
	{ O_TIMER,	4, 0x00100000 + SPAN_STALE + 1 },
	{ O_MARK,	4, 0x00100000 },
	PK_END
};

/*
 * THE COMPARE IS UNSIGNED, and this is the case that says so: a mark AHEAD of
 * the count is a small negative difference, which reads as nearly four
 * billion.  A signed compare would leave +0x0004 alone.
 */
static const struct poke pk_stale_behind[] = {
	{ O_MODE,	4, 2 },
	{ O_TIMER,	4, 0x00100000 },
	{ O_MARK,	4, 0x00100000 + 8 },
	PK_END
};

/* --- the retrain block ---------------------------------------------------- */

static const struct poke pk_rt_flag[] = {
	{ O_FLAGS,	2, 0x1ad5 },	/* bit 6 set, bit 5 clear */
	{ O_BAD,	2, 0 },
	PK_END
};

static const struct poke pk_rt_run[] = {
	{ O_BAD,	2, 51 },	/* faa96 100, so half is 50 */
	PK_END
};

static const struct poke pk_rt_edge[] = {
	{ O_BAD,	2, 50 },	/* not greater: no retrain */
	PK_END
};

/*
 * Bit 6 set AND the run over half, which is the only way to reach the
 * retrain block and leave 2 behind: the mode it reports is the SAME test
 * again, taken after `v34handshakinit` has run.
 */
static const struct poke pk_rt_both[] = {
	{ O_FLAGS,	2, 0x1ad5 },
	{ O_BAD,	2, 51 },
	PK_END
};

/*
 * ARITHMETIC SHIFT, NOT DIVISION.  With +0xaa96 at -3 the halving gives -2
 * shifted and -1 divided, and a run of -1 is over the first and not the
 * second.  Nothing else in the object separates the two.
 *
 * A NEGATIVE BLOCK RATE ALSO MAKES THE FALL-THROUGH VISIBLE, which is why
 * the two long-run counters are pinned well below both negative thresholds:
 * whichever block runs first clears all three to ZERO, and zero is above
 * 2 * -3 and above 8 * -3, so the retrain is followed by the step down and
 * that by the step up.  A run that does NOT retrain leaves -100 in both and
 * neither follows.  So the two cases differ in five oracles rather than in
 * the object alone.
 */
/*
 * A run EQUAL to half the block rate, with bit 6 doing the entry: the report
 * is `<=` and not `<`, so this is the one value that separates 3 from 2 on a
 * path where the run itself did not fire the retrain.
 */
static const struct poke pk_rt_report_edge[] = {
	{ O_FLAGS,	2, 0x1ad5 },
	{ O_BAD,	2, 50 },
	PK_END
};

static const struct poke pk_rt_negshift[] = {
	{ O_FAA96,	2, -3 },
	{ O_BAD,	2, -1 },
	{ O_BAD_LONG,	2, -100 },
	{ O_GOOD,	2, -100 },
	PK_END
};

static const struct poke pk_rt_negedge[] = {
	{ O_FAA96,	2, -3 },
	{ O_BAD,	2, -2 },
	{ O_BAD_LONG,	2, -100 },
	{ O_GOOD,	2, -100 },
	PK_END
};

/* --- the remote renegotiation --------------------------------------------- */

static const struct poke pk_rrn_remote[] = {
	{ O_FLAGS,	2, 0x1ab5 },	/* bit 5 set, bit 6 clear */
	PK_END
};

/*
 * BOTH BITS, AND THE SECOND BLOCK DOES NOT RUN.  The flags are re-read after
 * the retrain, and `v34handshakinit` mode 1 reaches `setupreceiver`, which
 * ASSIGNS the receiver's +0x122 rather than masking it -- so bit 5 is gone by
 * the time the second `if` looks.  The mode therefore ends at 3 and not 4,
 * and +0x25e ends at 0 because `setupreceiver` clears that too and the
 * retrain block writes nothing there.
 *
 * That is a fact about `v34handshakinit`, not about this function, and it is
 * why the fall-through has to be shown with a negative block rate below
 * rather than with these two bits.
 */
static const struct poke pk_rrn_both[] = {
	{ O_FLAGS,	2, 0x1af5 },
	{ O_BAD,	2, 0 },
	PK_END
};

/*
 * THE REMOTE BLOCK CLEARS +0x25c, and this is what says so: a good-block run
 * already over eight times the block rate, with the long run left at zero so
 * the step down cannot run in between.  Cleared, the step up does not follow;
 * left alone, it does.
 */
static const struct poke pk_rrn_remote_good[] = {
	{ O_FLAGS,	2, 0x1ab5 },
	{ O_GOOD,	2, 801 },
	PK_END
};

/*
 * THE FALL-THROUGH, THREE BLOCKS IN ONE CALL.  Bit 5 with a block rate of -1:
 * the remote block runs, clears all three counters, and zero is above both
 * 2 * -1 and 8 * -1 -- so the step down runs, and then the step up.  Three
 * `v34handshakinit` calls, three different values through +0x2218, and
 * +0xac0e twice.  These are four independent `if`s and this is what says so.
 */
static const struct poke pk_fallthrough[] = {
	{ O_FLAGS,	2, 0x1ab5 },
	{ O_FAA96,	2, -1 },
	{ O_BAD,	2, -100 },	/* below -1 >> 1, so no retrain first */
	{ O_BAD_LONG,	2, -100 },
	{ O_GOOD,	2, -100 },
	PK_END
};

/* --- the two error renegotiations ----------------------------------------- */

static const struct poke pk_down[] = {
	{ O_BAD_LONG,	2, 201 },	/* 2 * 100 */
	PK_END
};

static const struct poke pk_down_edge[] = {
	{ O_BAD_LONG,	2, 200 },
	PK_END
};

static const struct poke pk_up[] = {
	{ O_GOOD,	2, 801 },	/* 8 * 100 */
	PK_END
};

static const struct poke pk_up_edge[] = {
	{ O_GOOD,	2, 800 },
	PK_END
};

/*
 * BOTH ARMED, AND ONLY ONE FIRES.  The step down clears +0x25c on its way
 * out, so the step up's own test then reads zero.  Ordering, made visible.
 */
static const struct poke pk_both_rrn[] = {
	{ O_BAD_LONG,	2, 201 },
	{ O_GOOD,	2, 801 },
	PK_END
};

/*
 * THIRTY-TWO BIT COMPARES.  2 * 20000 is 40000 and 8 * 5000 is 40000, both of
 * which are negative as a short -- so a counter of 1 is under the threshold in
 * 32 bits and over it in 16.  These two cases are the difference.
 */
static const struct poke pk_down_wide[] = {
	{ O_FAA96,	2, 20000 },
	{ O_BAD_LONG,	2, 1 },
	PK_END
};

static const struct poke pk_up_wide[] = {
	{ O_FAA96,	2, 5000 },
	{ O_GOOD,	2, 1 },
	PK_END
};

/*
 * And the rate index the two renegotiations record, which is +0xaa98 read
 * AFTER `v34handshakinit`.
 */
static const struct poke pk_up_rate[] = {
	{ O_GOOD,	2, 801 },
	{ O_FAA98,	2, 0x4d2 },
	PK_END
};

/* --- the two loops -------------------------------------------------------- */

/*
 * `modulatevector` until the cursor reaches the limit.  Four samples a call
 * through `txmit`, so a limit of 12 with the cursor at 0 is three calls; the
 * count is not asserted here because it belongs to `modulatevector` and not
 * to this function, but the object's agreement over three calls of it is.
 */
static const struct poke pk_mod_loop[] = {
	{ O_TXCUR,	2, 0 },
	{ O_TXLIM,	2, 12 },
	PK_END
};

static const struct poke pk_mod_one[] = {
	{ O_TXCUR,	2, 8 },
	{ O_TXLIM,	2, 12 },
	PK_END
};

/*
 * `receiver` until the queue is down to five.  Each pass bumps +0x124 unless
 * it has reached the cap, refreshes the error measure at +0x21a, and moves
 * the three consecutive-run counters.
 */
static const struct poke pk_rx_loop[] = {
	{ O_RXCNT,	2, 12 },
	PK_END
};

/* The cap on +0x124: the last value that still bumps, and the first that does
 * not.  A `<` where the object has `<=` moves the first of these by one. */
static const struct poke pk_rx_cap_last[] = {
	{ O_RXCNT,	2, 8 },
	{ O_BLOCKS,	2, BLOCK_CAP },
	PK_END
};

static const struct poke pk_rx_cap_over[] = {
	{ O_RXCNT,	2, 8 },
	{ O_BLOCKS,	2, BLOCK_CAP + 1 },
	PK_END
};

/*
 * The two timed counters inside the loop.  Below 144,000 elapsed neither
 * +0x25a nor +0x25c moves whatever the error does; above 1,152,000 both do.
 */
static const struct poke pk_rx_span_none[] = {
	{ O_RXCNT,	2, 12 },
	{ O_TIMER,	4, 0x00100000 + SPAN_MID },
	{ O_MARK,	4, 0x00100000 },
	{ O_BAD_LONG,	2, 7 },
	{ O_GOOD,	2, 9 },
	PK_END
};

static const struct poke pk_rx_span_mid[] = {
	{ O_RXCNT,	2, 12 },
	{ O_TIMER,	4, 0x00100000 + SPAN_MID + 1 },
	{ O_MARK,	4, 0x00100000 },
	{ O_BAD_LONG,	2, 7 },
	{ O_GOOD,	2, 9 },
	PK_END
};

static const struct poke pk_rx_span_long[] = {
	{ O_RXCNT,	2, 12 },
	{ O_TIMER,	4, 0x00100000 + SPAN_LONG + 1 },
	{ O_MARK,	4, 0x00100000 },
	{ O_BAD_LONG,	2, 7 },
	{ O_GOOD,	2, 9 },
	PK_END
};

/*
 * And the three thresholds themselves, driven both ways: an error measure
 * above +0x252 and +0x254 and below +0x256 bumps all three counters, and one
 * the other side of all three clears all three.  +0x256's compare runs the
 * OTHER WAY, which is what makes its counter the step up.
 */
static const struct poke pk_rx_bump[] = {
	{ O_RXCNT,	2, 8 },
	{ O_TIMER,	4, 0x00100000 + SPAN_LONG + 1 },
	{ O_MARK,	4, 0x00100000 },
	{ O_THR_A,	2, -30000 },
	{ O_THR_B,	2, -30000 },
	{ O_THR_C,	2,  30000 },
	{ O_BAD,	2, 11 },
	{ O_BAD_LONG,	2, 13 },
	{ O_GOOD,	2, 17 },
	PK_END
};

static const struct poke pk_rx_clear[] = {
	{ O_RXCNT,	2, 8 },
	{ O_TIMER,	4, 0x00100000 + SPAN_LONG + 1 },
	{ O_MARK,	4, 0x00100000 },
	{ O_THR_A,	2,  30000 },
	{ O_THR_B,	2,  30000 },
	{ O_THR_C,	2, -30000 },
	{ O_BAD,	2, 11 },
	{ O_BAD_LONG,	2, 13 },
	{ O_GOOD,	2, 17 },
	PK_END
};

/*
 * THE TWO PLAIN-RUN THRESHOLDS ARE DIFFERENT FIELDS.  An error measure
 * between +0x252 and +0x254 fails the first and passes the second, so the
 * plain run bumps while the long run clears -- which is the only way to tell
 * the two reads apart.
 */
static const struct poke pk_rx_thr_split[] = {
	{ O_RXCNT,	2, 8 },
	{ O_TIMER,	4, 0x00100000 + SPAN_MID + 1 },
	{ O_MARK,	4, 0x00100000 },
	{ O_THR_A,	2, 4000 },
	{ O_THR_B,	2, 20000 },
	{ O_BAD,	2, 11 },
	{ O_BAD_LONG,	2, 13 },
	PK_END
};

/* An error EQUAL to its threshold passes: the compare is `>` and not `>=`. */
static const struct poke pk_rx_thr_equal[] = {
	{ O_RXCNT,	2, 8 },
	{ O_THR_A,	2, 0x2000 },
	{ O_BAD,	2, 11 },
	PK_END
};

/* And the long span's own boundary, which +0x25c is on the wrong side of. */
static const struct poke pk_rx_span_exact[] = {
	{ O_RXCNT,	2, 8 },
	{ O_TIMER,	4, 0x00100000 + SPAN_LONG },
	{ O_MARK,	4, 0x00100000 },
	{ O_THR_C,	2, 30000 },
	{ O_GOOD,	2, 9 },
	PK_END
};

/*
 * THE ONE THING THE FIXTURE'S `receiver` DOES NOT DO, seeded as hard as it
 * can be from outside.
 *
 * `receiver` refreshes +0x21a only when the counter at +0x21c wraps through
 * 0x400, and it writes +0x124 only on the decoder's two paths -- and it
 * returns before either, because reaching them needs a symbol decision and
 * this fixture's receive queue carries the arena's fill rather than a signal.
 * So the two ORDERING claims about this loop -- read the measure after the
 * call, bump the count before it -- are not testable here, and
 * test/mutations/v34datapump.json carries both as named gaps.
 *
 * The case is kept, and the two fields are ASSERTED unchanged below, so that
 * the gap is a checked property rather than an assumption: a fixture that
 * later drives the decoder will fail this and say the mutations became
 * catchable.
 */
static const struct poke pk_rx_equerr[] = {
	{ O_RXCNT,	2, 12 },
	{ O_F21C,	2, 0x3ff },
	{ O_BAD,	2, 11 },
	PK_END
};

/* --- the two RRN counters, on their own ----------------------------------- */

/*
 * Twenty bytes each and nothing but a wrapping 16-bit increment, so what
 * matters is the wrap and the fact that each touches ONE of the two fields.
 * Driven directly rather than only through `datapumpv34`, because the caller
 * reaches each of them on one path only.
 */
static void
rrn_cases(void)
{
	static const short seeds[] = { 0, 1, -1, 0x7fff, (short)0x8000,
				       0x1234, (short)0xffff, 30000 };
	unsigned i;

	for (i = 0; i < sizeof(seeds) / sizeof(seeds[0]); i++) {
		static struct v34_object oa, ob;
		char msg[128];

		memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
		memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
		oa.rrn_local = ob.rrn_local = seeds[i];
		oa.rrn_remote = ob.rrn_remote =
			(short)(seeds[i] ^ 0x55aa);

		VPcmV34IndicateLocalRRN(&oa);
		ref_VPcmV34IndicateLocalRRN(&ob);
		snprintf(msg, sizeof(msg), "IndicateLocalRRN at %d",
			 (int)seeds[i]);
		diff_eq_obj(msg, struct v34_object, &oa, &ob, (long)seeds[i]);
		diff_eq_int("IndicateLocalRRN wraps in 16 bits",
			    (int)oa.rrn_local, (int)(short)(seeds[i] + 1),
			    (long)seeds[i]);

		VPcmV34IndicateRemoteRRN(&oa);
		ref_VPcmV34IndicateRemoteRRN(&ob);
		snprintf(msg, sizeof(msg), "IndicateRemoteRRN at %d",
			 (int)seeds[i]);
		diff_eq_obj(msg, struct v34_object, &oa, &ob,
			    0x10000L + seeds[i]);
		diff_eq_int("IndicateRemoteRRN wraps in 16 bits",
			    (int)oa.rrn_remote,
			    (int)(short)((seeds[i] ^ 0x55aa) + 1),
			    (long)seeds[i]);
	}
}

/* --- the sweep ------------------------------------------------------------ */

struct trial {
	const char		*name;
	const struct poke	*pk;
	struct oracle		 ex;
	long			 tag;
};

/*
 * `mode`, `why`, `dloc`, `drem`, `progress`.  KEEP means the field must hold
 * whatever the base poke left in it -- 0x0badf00d for +0x0004, 0x5a5a for
 * +0x25e -- which is a claim that no arm wrote it.
 */
#define BASE_MODE	0
#define BASE_WHY	0x5a5a
#define BASE_PROGRESS	0x0badf00d

static const struct trial trials[] = {
  /* The handshake branch: armed and inert. */
  { "handshake mode 2, loop already satisfied", pk_hs_armed,
    { 2, BASE_WHY, 0, 0, BASE_PROGRESS }, 100 },
  { "handshake mode 3", pk_hs_mode3,
    { 3, BASE_WHY, 0, 0, BASE_PROGRESS }, 101 },
  { "handshake mode 0xffffffff", pk_hs_huge,
    { -1, BASE_WHY, 0, 0, BASE_PROGRESS }, 102 },

  /*
   * The same pokes at mode 1 and 0, which take the data path: bit 6 and the
   * run both fire the retrain, bit 5 then fires the remote block, and the
   * mode ends at 4.  This is the guard, both ways, one field apart.
   */
  { "mode 1 takes the data path", pk_hs_one, { 5, 2, 1, 0, KEEP }, 110 },
  { "mode 0 takes the data path", pk_hs_zero, { 5, 2, 1, 0, KEEP }, 111 },
  { "the same trigger at mode 2 does nothing", pk_hs_two,
    { 2, BASE_WHY, 0, 0, BASE_PROGRESS }, 112 },

  /* The stale-clock guard. */
  { "span exactly 288000 is not stale", pk_stale_under,
    { 2, BASE_WHY, 0, 0, BASE_PROGRESS }, 120 },
  { "span 288001 is stale", pk_stale_over,
    { 2, BASE_WHY, 0, 0, 5 }, 121 },
  { "the mark ahead of the count is stale, unsigned", pk_stale_behind,
    { 2, BASE_WHY, 0, 0, 5 }, 122 },

  /* The retrain. */
  /*
   * `why` is 0 rather than the base value in every retrain case: the retrain
   * block writes +0x25e not at all, and `setupreceiver` -- which
   * `v34handshakinit` mode 1 reaches -- clears it.
   */
  { "retrain on bit 6, run at 0", pk_rt_flag,
    { 3, 0, 0, 0, KEEP }, 200 },
  { "retrain on a run of 51 over half of 100", pk_rt_run,
    { 2, 0, 0, 0, KEEP }, 201 },
  { "a run of exactly 50 does not retrain", pk_rt_edge,
    { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 202 },
  { "bit 6 with a run of exactly 50 reports 3", pk_rt_report_edge,
    { 3, 0, 0, 0, KEEP }, 206 },
  { "bit 6 and a run of 51 report 2", pk_rt_both,
    { 2, 0, 0, 0, KEEP }, 203 },
  { "the halving is an arithmetic shift", pk_rt_negshift,
    { 5, 3, 2, 0, KEEP }, 204 },
  { "a run of -2 against -3 halved does not retrain", pk_rt_negedge,
    { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 205 },

  /* The remote renegotiation. */
  { "remote renegotiation on bit 5", pk_rrn_remote,
    { 4, 1, 0, 1, KEEP }, 300 },
  { "the remote block clears +0x25c, so the step up cannot follow",
    pk_rrn_remote_good, { 4, 1, 0, 1, KEEP }, 303 },
  { "a retrain resets the flags, so bit 5 cannot follow", pk_rrn_both,
    { 3, 0, 0, 0, KEEP }, 301 },
  { "bit 5 then both error blocks: three inits in one call",
    pk_fallthrough, { 5, 3, 2, 1, KEEP }, 302 },

  /* The two error renegotiations. */
  { "step down on a long run of 201 over 2 x 100", pk_down,
    { 5, 2, 1, 0, KEEP }, 400 },
  { "a long run of exactly 200 does not step down", pk_down_edge,
    { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 401 },
  { "step up on a good run of 801 over 8 x 100", pk_up,
    { 5, 3, 1, 0, KEEP }, 402 },
  { "a good run of exactly 800 does not step up", pk_up_edge,
    { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 403 },
  { "down clears the good run, so up cannot follow", pk_both_rrn,
    { 5, 2, 1, 0, KEEP }, 404 },
  { "2 x 20000 is 40000 in 32 bits, not -25536", pk_down_wide,
    { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 405 },
  { "8 x 5000 is 40000 in 32 bits, not -25536", pk_up_wide,
    { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 406 },
  { "the step up records +0xaa98 as the rate", pk_up_rate,
    { 5, 3, 1, 0, KEEP }, 407 },

  /* The modulator loop. */
  { "modulatevector to a limit of 12 from 0", pk_mod_loop,
    { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 500 },
  { "modulatevector to a limit of 12 from 8", pk_mod_one,
    { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 501 },

  /* The receiver loop. */
  { "receiver drains a queue of 12", pk_rx_loop,
    { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 600 },
  { "+0x124 at 0x752f still bumps", pk_rx_cap_last,
    { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 601 },
  { "+0x124 at 0x7530 does not", pk_rx_cap_over,
    { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 602 },
  { "a span of exactly 144000 moves neither timed counter",
    pk_rx_span_none, { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 603 },
  { "a span of 144001 moves +0x25a only", pk_rx_span_mid,
    { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 604 },
  /*
   * The last three run a span past 1,152,000, which is also past the
   * stale-clock threshold at the top of the function, so +0x0004 is 5
   * before the loop is even entered.
   */
  { "a span past 1152000 moves both", pk_rx_span_long,
    { BASE_MODE, BASE_WHY, 0, 0, 5 }, 605 },
  { "an error over both thresholds and under the third", pk_rx_bump,
    { KEEP, BASE_WHY, 0, 0, 5 }, 606 },
  { "an error the other side of all three clears all three", pk_rx_clear,
    { BASE_MODE, BASE_WHY, 0, 0, 5 }, 607 },
  { "+0x252 and +0x254 are different fields", pk_rx_thr_split,
    { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 608 },
  { "an error equal to +0x252 clears the run", pk_rx_thr_equal,
    { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 609 },
  { "a span of exactly 1152000 leaves +0x25c alone", pk_rx_span_exact,
    { BASE_MODE, BASE_WHY, 0, 0, 5 }, 610 },
  { "+0x21c seeded at its wrap: `receiver` still writes neither field",
    pk_rx_equerr, { BASE_MODE, BASE_WHY, 0, 0, BASE_PROGRESS }, 611 }
};

#define NTRIALS	((int)(sizeof(trials) / sizeof(trials[0])))


/* --- the separation claims, and the anti-vacuity ones --------------------- */

static int
index_of(long tag)
{
	int i;

	for (i = 0; i < NTRIALS; i++)
		if (trials[i].tag == tag)
			return i;
	return -1;
}

/*
 * Two cases took different paths.  The comparison is over what side A WROTE
 * -- the byte count and the FNV hash of (offset, new byte) -- so it is a
 * claim about the object and not about the oracle, and a reconstruction that
 * collapsed the two would fail here even if both halves still matched the
 * blob byte for byte (which they could not, but the claim is worth stating
 * where the oracle cannot reach: `receiver` loop iterations have no oracle).
 */
static void
differ(const char *what, long a, long b)
{
	int i = index_of(a), j = index_of(b);
	char msg[192];

	snprintf(msg, sizeof(msg), "%s take different paths", what);
	diff_eq_int(msg, i >= 0 && j >= 0 && (seen[0][i].hash != seen[0][j].hash
			 || seen[0][i].changed != seen[0][j].changed), 1, a);
}

int
main(void)
{
	int i;

	dump = getenv("V34HS_DUMP") != NULL;
	diff_begin("datapumpv34 and the two rate-renegotiation counters");

	rrn_cases();

	for (debug_on = 0; debug_on <= 1; debug_on++) {
		v34hs_debug(debug_on);
		v34hs_entry(datapumpv34, ref_datapumpv34, 0);
		if (dump)
			printf("datapumpv34, diagnostics %s\n",
			       debug_on ? "on" : "off");
		for (i = 0; i < NTRIALS; i++) {
			trial_now = i;
			run(trials[i].name, trials[i].pk, &trials[i].ex,
			    trials[i].tag + debug_on * 10000);
			control(trials[i].name, trials[i].pk,
				trials[i].tag + debug_on * 10000 + 1000000);
		}
	}

	/*
	 * ANTI-VACUITY, three ways.
	 *
	 * First: every trial's record was filled.  An array written and never
	 * read, under a comment promising it cannot go stale, is what finding
	 * 290 records this fixture doing to `saw_hole`.
	 */
	for (i = 0; i < NTRIALS; i++) {
		char msg[192];

		snprintf(msg, sizeof(msg), "record for \"%s\" was filled",
			 trials[i].name);
		diff_eq_int(msg, seen[0][i].used + seen[1][i].used, 2,
			    trials[i].tag);
	}

	/*
	 * Second: how many trials wrote NOTHING AT ALL.  Eleven of the
	 * thirty-six are "this arm must not run" cases, and for those the
	 * differential comparison alone is a comparison of two silences; the
	 * count is what stops a change that quietly turned an armed case
	 * inert from still passing.  The other twenty-five must all have
	 * moved the object.
	 */
	{
		int inert = 0;

		for (i = 0; i < NTRIALS; i++)
			if (seen[0][i].changed == 0)
				inert++;
		diff_eq_int("trials that wrote nothing", inert, 11, 0);
	}

	/*
	 * Third: the diagnostics move the transcript and NOT the object.
	 * With them off nothing prints; with them on the object is written
	 * exactly the same way, which is what makes the transcript an
	 * independent axis rather than a second view of the bytes.
	 */
	for (i = 0; i < NTRIALS; i++) {
		char msg[192];

		snprintf(msg, sizeof(msg),
			 "\"%s\" prints nothing with the diagnostics off",
			 trials[i].name);
		diff_eq_int(msg, (int)seen[0][i].lines, 0, trials[i].tag);
		snprintf(msg, sizeof(msg),
			 "\"%s\" writes the same object either way",
			 trials[i].name);
		diff_eq_int(msg, (int)seen[1][i].changed,
			    (int)seen[0][i].changed, trials[i].tag);
	}

	/*
	 * And at least one trial must have printed, or the whole diagnostics
	 * pass is a comparison of two empty transcripts (finding 362 is that
	 * failure mode, named there rather than caught).
	 */
	{
		int traced = 0;

		for (i = 0; i < NTRIALS; i++)
			if (seen[1][i].lines != 0)
				traced++;
		diff_eq_int("trials that printed with the diagnostics on",
			    traced, 15, 0);
	}

	/*
	 * AND THE NAMED GAP, CHECKED.  Trial 611 seeds +0x21c at its wrap and
	 * drives two passes of `receiver`; if the measure at +0x21a is still
	 * the value the base poked and +0x124 has moved by exactly the two
	 * bumps this function made, then `receiver` wrote neither, and the
	 * two ordering mutations in the suite cannot be caught for that
	 * reason and no other.  Written as an assertion so that a fixture
	 * which later reaches the decoder fails here rather than leaving the
	 * gap recorded and stale.
	 */
	diff_eq_int("the fixture's `receiver` leaves +0x21a alone",
		    seen[0][index_of(611)].err, 0x2000, 611);
	diff_eq_int("the fixture's `receiver` leaves +0x124 alone",
		    seen[0][index_of(611)].blocks, 0x123 + 2, 611);

	/* --- the separations, each one a claim some mutation would break --- */

	differ("+0x124 at the cap and one past it", 601, 602);
	differ("a span at 144000 and one past it", 603, 604);
	differ("a span past 144000 and one past 1152000", 604, 605);
	differ("an error over the thresholds and under them", 606, 607);
	differ("the modulator loop from 0 and from 8", 500, 501);
	differ("the step down and the step up", 400, 402);
	differ("the step down alone and both armed", 400, 404);
	differ("the retrain reporting 3 and reporting 2", 200, 201);
	differ("the remote block and the retrain", 300, 200);
	differ("the fall-through and the remote block alone", 302, 300);
	differ("+0xaa98 at -1234 and at 0x4d2", 402, 407);

	v34hs_debug(0);
	v34hs_entry(NULL, NULL, 0);
	v34hs_holes_check();

	return diff_end();
}

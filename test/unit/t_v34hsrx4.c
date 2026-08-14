/*
 * t_v34hsrx4.c -- `v34handshak`'s rxstate 4 `RECEIVE`, 0x653e4.
 *
 * The largest of the rxstate chain's six exits: 4,881 bytes over
 * twenty-eight ranges, driven case by case against the blob through
 * test/harness/v34hsstep.c with `v34hs_ours(1)` putting this tree's
 * `v34handshak` on side A -- so every case here is an ordinary tier-1
 * differential comparison of the whole 44,096-byte object, the five blocks
 * it points at, the padding around them and both transcripts.
 *
 * WHAT THE ARM IS.  A retrain test at the entry, then four independent
 * pieces: the TRN2 shift registers (0x679bc), the MP bit packer (0x68bd8)
 * with its run-length half (0x6cb70), the far end's J (0x68e3b, which is
 * `v34setuptxmit` inlined) and the tone detector with what follows it.
 * FIFTEEN exits and every one of them is the once-per-block transmit
 * dispatch; TEN are the same nineteen-byte "read txstate, jump" idiom, so
 * "the arm ran" is not evidence and every suite below asserts what its path
 * WROTE.  All fifteen are driven, and TWO OF THEM ONLY WITH THE DIAGNOSTICS
 * OFF: 0x6546e and 0x6a4dc are the quiet twins of 0x690fb and 0x6d28e, and
 * `suite_quiet` is the only place either is reached.
 *
 * THE TWO CLAIMS NO ORDINARY VALUE CAN TEST, and the trials built for them:
 *
 *   0x65427 is a THIRTY-TWO-BIT signed compare and `7 * baud_rate` is not
 *   truncated.  Over every legal baud -- 7 * 3429 is 23,853 -- a sixteen-bit
 *   spelling agrees with the object on every input.  `suite_entry` drives
 *   baud_rate at 20,000, where `7 * baud_rate` is 140,000 and its low halfword is
 *   8,928, so the object takes the BODY and a truncating reading retrains;
 *   and at -20,000, where the object retrains and both a truncating and an
 *   unsigned reading decline.  Finding 724's shape in a second arm.
 *
 *   0x6cafd COMPARES with 0x7fff and STORES without it.  A reconstruction
 *   that stored the masked value agrees on every word whose bit 15 is clear,
 *   which is every word a fixture picks by accident.  `suite_packer` drives
 *   a word with bit 15 SET and asserts the table entry afterwards.
 *
 * FIVE THINGS THAT CANNOT BE POKED, four of them because the receive chain
 * runs first: the receiver's +0x10c.. (the burst the detector reads), its
 * +0x130 (`rx_samples`, the detector's end pointer), its +0x136 (the gain)
 * and its +0x122 -- `tone_detect` itself clears bit 9 of the flags at
 * 0x73869, so the value 0x67b56 reads back is not the one this file poked.
 * The fifth is +0x35a2, which 0x6a4c1 writes and 0x6a097 reads on a LATER
 * step.  The detector is steered through its own state at +0x3564 instead,
 * with its coefficients aimed at object +0x8000 -- finding 357's address,
 * because +0x500 is inside the receiver and produces a false alarm.
 *
 * WHICH FILLS THIS FILE IS GREEN AT, MEASURED RATHER THAN ASSUMED.  The
 * default and `V34HS_SEED` 1, 3, 5, 10 and 12 all pass; 7 and 20 do NOT, and
 * at both of them the CONTROL cases -- the blob on side A as well as side B
 * -- fail identically, so what disagrees is the fixture and not this arm.
 * The disagreement is confined to the receiver's +0x1ae and +0x1cc..+0x1e3
 * and appears on every case, including ones whose only call is `receiver`
 * itself; it survives V34HS_REFINIT, V34HS_EQPTR, V34HS_SKEW, V34HS_NOSCRUB
 * and V34HS_PADVARY, and `V34HS_PROBE` reports the two objects identical
 * after setup and side B deterministic at its own address.  This is the
 * first test to call `receiver` through this fixture at all.  Finding 736.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "v34hsstep.h"
#include "dsplib/v34hshak.h"

/* The five inputs of table 2's tail, which can erase what an arm decided. */
#define RX4_LVL_LIMIT	0x0230
#define RX4_LVL_COUNT	0x0234
#define RX4_TIMER_LO	0x0238
#define RX4_TIMER_HI	0x023c
#define RX4_MODE	0x2218

#define RX4_RECEIVER	0x0264
#define RX4_V90RX	0x024c	/* int, `v34setuptxmit`'s `pcm`            */
#define RX4_K56RX	0x0250	/* int, and its other half                 */

/* Within the receiver, as object offsets. */
#define RX4_RX_F11C	(RX4_RECEIVER + 0x11c)	/* the TRN2 shift register */
#define RX4_RX_F11E	(RX4_RECEIVER + 0x11e)	/* the one behind it       */
#define RX4_RX_F120	(RX4_RECEIVER + 0x120)
#define RX4_RX_FLAGS	(RX4_RECEIVER + 0x122)
#define RX4_RX_F124	(RX4_RECEIVER + 0x124)	/* the baud counter        */
#define RX4_RX_BESTIDX	(RX4_RECEIVER + 0x126)	/* two bits a block        */
#define RX4_RX_AGCGAIN	(RX4_RECEIVER + 0x136)	/* seedable on the J path */
#define RX4_RX_F1C0	(RX4_RECEIVER + 0x1c0)
#define RX4_RX_F1D0	(RX4_RECEIVER + 0x1d0)	/* the timing offset       */
#define RX4_RX_F218	(RX4_RECEIVER + 0x218)
#define RX4_RX_TIMERLO	(RX4_RECEIVER + 0x238)
#define RX4_RX_TIMERHI	(RX4_RECEIVER + 0x23c)
#define RX4_RX_F262	(RX4_RECEIVER + 0x262)
#define RX4_RX_F266	(RX4_RECEIVER + 0x266)

#define RX4_PLLCNT	0x25da	/* obj+0x221c+0x3be, the arm's own counter */
#define RX4_MPCOEF	0x2a68	/* twelve shorts                           */
#define RX4_VECTIDX	0x2aa2	/* short, every trace's [1]                */
#define RX4_DET		0x3564	/* struct v34_detector                     */
#define RX4_F356C	0x356c
#define RX4_F3570	0x3570	/* short, 3 selects the MD-over path        */
#define RX4_F3576	0x3576
#define RX4_F3598	0x3598	/* short, "initdigital has run"            */
#define RX4_F359C	0x359c	/* short, 0x65 opens the J path            */
#define RX4_MDLEN	0x35a2
#define RX4_REC_A9DC	0xa9dc	/* what `settxlevel` is handed             */
#define RX4_MPTBL	0xaa0c	/* ten shorts, the MP sequence             */
#define RX4_MPRUN	0xaa26
#define RX4_MPIDX	0xaa2a
#define RX4_MPACC	0xaa30	/* int, the bit accumulator                */
#define RX4_MPBITS	0xaa34
#define RX4_MPCAPS	0xaa3c	/* low byte tested by the run-length half  */
#define RX4_COUNT	0xaa78	/* short, every trace's [2]                */
#define RX4_FAA80	0xaa80
#define RX4_TXBAUD	0xaa84	/* struct v34_ratecfg, v34fsk.h            */
#define RX4_TXBITS	0xaa88
#define RX4_DEPTH	0xaa8c
#define RX4_USEMAX	0xaa8e
#define RX4_PREEMP	0xaa8a
#define RX4_TXCARRIER	0xaa94
#define RX4_FAA96	0xaa96	/* short, the receive baud rate            */
#define RX4_RXBITS	0xaa98
#define RX4_RXUSEMAX	0xaaa6
#define RX4_RXCARRIER	0xaaa8
#define RX4_RXCARRDESC	0xaab0	/* the detector coefficients               */
#define RX4_RTSCALE	0xaacc	/* int, SIGNED, scaled per baud            */

/* struct v34_detector at +0x3564, fields from v34det.h. */
#define RX4_DET_POL	(RX4_DET + 0x04)
#define RX4_DET_ARMED	(RX4_DET + 0x06)
#define RX4_DET_COUNT	(RX4_DET + 0x08)
#define RX4_DET_LIMIT	(RX4_DET + 0x0a)
#define RX4_DET_STATE	(RX4_DET + 0x0c)
#define RX4_DET_THI	(RX4_DET + 0x0e)
#define RX4_DET_TLO	(RX4_DET + 0x10)

/* Outside anything the step writes -- finding 357. */
#define RX4_DET_COEFF	0x8000

/*
 * The txstate every case that does not force its own is driven with.
 *
 * 81 MOH_SILENCE is above table 2's window and selects the once-per-block
 * dispatch's own default at 0x62a40, which is written.  The J path FORCES
 * 18 SSEG, which is table 2's 0x64518 and is landed -- `t_v34hstbl2.c`
 * drives it.
 */
#define RX4_TXSTATE	V34HS_MOH_SILENCE

/* A microstate that is neither 41 nor 44, so both transitions are changes. */
#define RX4_MST		V34HS_INFODONE

static int dump;
static int default_fill;

/*
 * Open a case: the route, the three state words, table 2's five inputs, and
 * every field this arm writes seeded to something it does NOT write.
 *
 * That last part is finding 345's failure mode turned on this arm.  Eleven
 * of the arm's stores are of a constant -- 0x1e, 3, 0, 1, 0x4000 -- and a
 * field the fill happened to leave holding that value makes the store
 * invisible and its mutation equivalent.  All of them are seeded here rather
 * than reasoned about.
 */
static void
begin(short mst, short txst)
{
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(mst, V34HS_RECEIVE, txst);

	v34hs_poke_int(RX4_MODE, 0);
	v34hs_poke_int(RX4_TIMER_LO, 1000);
	v34hs_poke_int(RX4_TIMER_HI, 2000);
	v34hs_poke_int(RX4_LVL_LIMIT, 0x7fffffff);
	v34hs_poke_int(RX4_LVL_COUNT, 0);

	/*
	 * NEITHER PCM RECEIVER RUNNING.  `v34setuptxmit` only prints what it
	 * computes from these, but a fill that leaves either non-zero makes
	 * the J path's `pcm` argument depend on the seed.
	 */
	v34hs_poke_int(RX4_V90RX, 0);
	v34hs_poke_int(RX4_K56RX, 0);

	/* The entry's two inputs, and the guards below them. */
	v34hs_poke_short(RX4_RX_FLAGS, 0);
	v34hs_poke_short(RX4_RX_F124, 0);
	v34hs_poke_short(RX4_FAA96, 100);
	v34hs_poke_short(RX4_F3570, 0);
	v34hs_poke_short(RX4_F359C, 0);
	v34hs_poke_short(RX4_MDLEN, 0);
	v34hs_poke_short(RX4_FAA80, 0);

	/* Everything the arm stores a constant into. */
	v34hs_poke_short(RX4_F356C, 0x0101);
	v34hs_poke_short(RX4_F3576, 0x0202);
	v34hs_poke_short(RX4_F3598, 0x0303);
	v34hs_poke_short(RX4_RX_F1C0, 0x0404);
	v34hs_poke_short(RX4_RX_F218, 0x0505);
	v34hs_poke_short(RX4_RX_F262, 0x0606);
	v34hs_poke_short(RX4_RX_F266, 0x0707);
	v34hs_poke_short(RX4_PLLCNT, 0x0808);
	v34hs_poke_short(RX4_COUNT, 0x09);
	v34hs_poke_short(RX4_VECTIDX, 0x0a);
	v34hs_poke_short(RX4_MPIDX, 0);
	v34hs_poke_short(RX4_MPRUN, 0);
	v34hs_poke_short(RX4_MPBITS, 0);
	v34hs_poke_int(RX4_MPACC, 0);
	v34hs_poke_byte(RX4_MPCAPS, 0);
	v34hs_poke_short(RX4_RX_F11C, 0x1111);
	v34hs_poke_short(RX4_RX_F11E, 0x2222);
	v34hs_poke_short(RX4_RX_F120, 0x3333);
	v34hs_poke_short(RX4_RX_BESTIDX, 0);
	v34hs_poke_short(RX4_RX_F1D0, 0);
	v34hs_poke_int(RX4_RTSCALE, 0x40000);

	/* The rate configuration `setupreceiver` and `v34setuptxmit` read. */
	v34hs_poke_short(RX4_TXBAUD, 0xd65);
	v34hs_poke_short(RX4_TXCARRIER, 0x780);
	v34hs_poke_short(RX4_PREEMP, 0);
	v34hs_poke_short(RX4_RXCARRIER, 0x780);
	v34hs_poke_self_ptr(RX4_RXCARRDESC, RX4_DET_COEFF);

	/*
	 * AND THE FOUR RATE FIELDS `initdigital` INDEXES A TABLE WITH, which
	 * is a fixture requirement and not a claim about this arm.  E's path
	 * calls `initdigital`, whose 0x59c0f reads
	 * `rx_divtab[rxbits + 14 * rx_use_max - 1]` -- so a fill that leaves
	 * +0xaaa6 holding a pseudorandom short indexes that table miles off
	 * its end and FAULTS INSIDE THE CALLEE ON BOTH SIDES.  Found by
	 * running the sweep at V34HS_SEED=3, where it segfaults in
	 * `ref_initdigital`; the default fill happens to leave values that
	 * land inside.  `t_v34shell.c` constrains the same four for the same
	 * reason and says so.  A fault has no offset in it, which is why this
	 * is seeded rather than discovered per case.
	 */
	v34hs_poke_short(RX4_TXBITS, 6);
	v34hs_poke_short(RX4_DEPTH, 0);
	v34hs_poke_short(RX4_USEMAX, 0);
	v34hs_poke_short(RX4_RXBITS, 6);
	v34hs_poke_short(RX4_RXUSEMAX, 0);
}

/*
 * Make the detector assert, or refuse to.  Verbatim from `t_v34hsrx53.c`,
 * which took it from `t_v34hst3m41.c`: refusing is done through the warm-up
 * counter, which returns before the level is consulted, so neither answer
 * depends on what the receive chain made of the samples.
 */
static void
detector(int assert_it)
{
	v34hs_poke_self_ptr(RX4_DET + 0x00, RX4_DET_COEFF);
	v34hs_poke_short(RX4_DET + 0x12, 300);		/* level   */
	v34hs_poke_short(RX4_DET + 0x14, 11);		/* x[0][0] */
	v34hs_poke_short(RX4_DET + 0x16, -22);
	v34hs_poke_short(RX4_DET + 0x18, 33);
	v34hs_poke_short(RX4_DET + 0x1a, -44);
	v34hs_poke_short(RX4_DET + 0x1c, 55);		/* y[0][0] */
	v34hs_poke_short(RX4_DET + 0x1e, -66);
	v34hs_poke_short(RX4_DET + 0x20, 77);
	v34hs_poke_short(RX4_DET + 0x22, -88);

	v34hs_poke_short(RX4_DET_POL, 0);		/* presence */
	v34hs_poke_short(RX4_DET_ARMED, 1);
	v34hs_poke_short(RX4_DET_THI, 0);
	v34hs_poke_short(RX4_DET_TLO, (short)0x8000);

	if (assert_it) {
		v34hs_poke_short(RX4_DET_STATE, 2);
		v34hs_poke_short(RX4_DET_COUNT, 0);
		v34hs_poke_short(RX4_DET_LIMIT, -1);
	} else {
		v34hs_poke_short(RX4_DET_STATE, 1);
		v34hs_poke_short(RX4_DET_COUNT, -1000);
		v34hs_poke_short(RX4_DET_LIMIT, 0x7fff);
	}
}

#define NOCHECK	((unsigned)-1)

static unsigned last_hash;

/*
 * Step, compare, and say what the step DID.
 *
 * The comparison is the differential check; the assertions after it are the
 * anti-vacuity half.  Ten of the fifteen exits write nothing of their own,
 * so a case that left one guard earlier than its name says still compares --
 * both sides left -- and without a claim about what was written this whole
 * file would be green and testing nothing.
 */
static void
step(const char *what, long tag, unsigned changed, unsigned lines,
     int mst, int rxst, int txst)
{
	const struct v34hs_obs *o;

	v34hs_ours(1);
	v34hs_step();
	v34hs_ours(0);
	v34hs_compare(what, tag);

	o = v34hs_observed(0);
	last_hash = o->hash;
	if (dump)
		printf("  %-46s changed %4u  lines %2u  mst %2d rx %2d tx %2d "
		       "hash %08x\n", what, o->changed, o->lines,
		       o->mst, o->rxst, o->txst, o->hash);

	/*
	 * `changed` and `lines` count against the fill, so both are asserted
	 * at the default fixture only -- finding 359 measured them moving by
	 * a byte or two across seeds.  The transcript itself is compared line
	 * for line at EVERY seed by `v34hs_compare` above; what is gated here
	 * is only the count.
	 */
	if (default_fill && changed != NOCHECK)
		diff_eq_int("object bytes the step wrote", o->changed, changed,
			    tag);
	if (default_fill && lines != NOCHECK)
		diff_eq_int("diagnostic lines printed", o->lines, lines, tag);
	if (mst != (int)NOCHECK)
		diff_eq_int("microstate afterwards", o->mst, mst, tag);
	if (rxst != (int)NOCHECK)
		diff_eq_int("rxstate afterwards", o->rxst, rxst, tag);
	if (txst != (int)NOCHECK)
		diff_eq_int("txstate afterwards", o->txst, txst, tag);
}

/* The same case with the BLOB on both sides -- the fixture's own control. */
static void
control(const char *what, long tag)
{
	v34hs_step();
	v34hs_compare(what, tag);
}

/* What side A holds afterwards, for a claim about one field. */
static short
after16(unsigned off)
{
	return v34hs_peek_short(0, off);
}

static int
after32(unsigned off)
{
	const unsigned char *m = (const unsigned char *)v34hs_object(0);
	int v;

	memcpy(&v, m + off, sizeof(v));
	return v;
}

/*
 * The distinct behaviours this file claims, so that a change collapsing two
 * of them is a failure rather than a silence (finding 290).
 */
#define NSIG	64
static unsigned sig[NSIG];
static const char *signame[NSIG];
static int nsig;

static void
record(const char *name)
{
	if (nsig < NSIG) {
		sig[nsig] = last_hash;
		signame[nsig] = name;
		nsig++;
	}
}

/* --- the entry, 0x65401..0x6543b --------------------------------------- */

/*
 * Three guards over two fields, and the width of the middle one is the
 * claim.
 *
 * 0x65401 tests bit 6 of the flags and goes STRAIGHT to the retrain.
 * 0x65427 compares the baud counter against seven times the baud rate, both
 * sign-extended into 32-bit registers, and `jle` sends less-or-equal to the
 * BODY.  0x6542f then declines the retrain anyway if bit 7 is up.
 *
 * `v34handshakinit` RUNS INSIDE THE STEP on the retrain path, so those cases
 * are what finding 359 says `V34HS_REFINIT=1` cannot be used against -- and
 * it is ONE of the fifteen exits, so the prohibition scopes to it and to
 * nothing else in this file.
 */
static void
suite_entry(void)
{
	long tag = 100;

	/* Bit 6: retrain whatever the counter says. */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_RX_FLAGS, 0x40);
	step("entry: flag 0x40 retrains", tag, 204, 9,
	     RX4_MST, V34HS_WAIT, V34HS_SILENCERETRAIN);
	record("entry retrain by flag");

	/* The counter over seven times the rate, both small. */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA96, 100);
	v34hs_poke_short(RX4_RX_F124, 1000);
	step("entry: counter over 7*baud retrains", tag + 1, 206, 9,
	     RX4_MST, V34HS_WAIT, V34HS_SILENCERETRAIN);

	/* And bit 7 declines it. */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA96, 100);
	v34hs_poke_short(RX4_RX_F124, 1000);
	v34hs_poke_short(RX4_RX_FLAGS, 0x80);
	step("entry: flag 0x80 declines the retrain", tag + 2, 192, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	record("entry declined by flag 0x80");

	/*
	 * THE WIDE PRODUCT.  7 * 20000 is 140,000, whose low halfword is
	 * 8,928 -- so a truncating spelling sees 10,000 > 8,928 and retrains
	 * where the object stays in the body.  Every legal baud is inside a
	 * short, which is why nothing smaller can separate the two.
	 */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA96, 20000);
	v34hs_poke_short(RX4_RX_F124, 10000);
	step("entry: 7*20000 does not wrap, so the body runs", tag + 3,
	     196, 0, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the body ran and stepped the counter",
		    (int)after16(RX4_RX_F124), 10001, tag + 3);
	record("wide product declines");

	/*
	 * And the other direction, which also fixes the SIGN of +0xaa96.
	 * 7 * -20000 is -140,000; its low halfword is 56,608 and an unsigned
	 * reading of the field gives 7 * 45,536 = 318,752.  The object
	 * retrains on the negative product and both wrong readings decline.
	 */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA96, -20000);
	v34hs_poke_short(RX4_RX_F124, 0);
	step("entry: 7*-20000 is negative, so it retrains", tag + 4,
	     204, 9, RX4_MST, V34HS_WAIT, V34HS_SILENCERETRAIN);
	/*
	 * NOT RECORDED as a distinct behaviour, and deliberately: it is the
	 * SAME retrain the first case reaches by the other guard, so the two
	 * signatures are identical and must be.  What separates the readings
	 * here is which BRANCH was taken, and the three state words above are
	 * where that shows.
	 */
	/*
	 * THE BOUNDARY, which is what fixes `>` against `>=`: at exactly
	 * seven times the rate the object stays in the body.
	 */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA96, 100);
	v34hs_poke_short(RX4_RX_F124, 700);
	step("entry: exactly 7*baud stays in the body", tag + 5, 192, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the body ran and stepped the counter",
		    (int)after16(RX4_RX_F124), 701, tag + 5);
	record("entry at the boundary");

	/* And between six and seven times it, which fixes the multiplier. */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA96, 100);
	v34hs_poke_short(RX4_RX_F124, 650);
	step("entry: between 6*baud and 7*baud", tag + 6, 192, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the body ran and stepped the counter",
		    (int)after16(RX4_RX_F124), 651, tag + 6);
	record("entry between six and seven");

	/*
	 * A NEGATIVE COUNTER, which fixes the sign of +0x124 in TWO places:
	 * here, where an unsigned reading would see 65,535 and retrain, and
	 * at 0x67a3a, where an unsigned reading would refuse to step it.
	 */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA96, 100);
	v34hs_poke_short(RX4_RX_F124, -1);
	step("entry: a negative counter is below the threshold", tag + 7,
	     199, 0, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("and the counter stepped to zero",
		    (int)after16(RX4_RX_F124), 0, tag + 7);
	record("entry with a negative counter");

	/* The cap itself: 0x61a7 still steps and 0x61a8 does not. */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA96, 3600);
	v34hs_poke_short(RX4_RX_F124, 0x61a7);
	step("entry: the counter at its cap still steps", tag + 8, 196, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the cap is 0x61a7 and not 0x61a6",
		    (int)after16(RX4_RX_F124), 0x61a8, tag + 8);
	record("entry at the counter cap");

}

/* --- the plain exits and the detector, 0x67a2f..0x67b90 ----------------- */

/*
 * The path with no message and no packer: bump the baud counter, freeze the
 * gain, ask the detector, and either leave or commit.
 *
 * FOUR OF THIS FILE'S EXITS WRITE ALMOST NOTHING, so each case here asserts
 * the one field that separates it.
 */
static void
suite_detect(void)
{
	long tag = 200;

	/* 0x686f1: bit 10 up and the arm leaves before the detector. */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_RX_FLAGS, 0x400);
	detector(1);
	step("detect: flag 0x400 leaves at 0x686f1", tag, 188, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the counter was stepped and nothing else",
		    (int)after16(RX4_RX_F124), 1, tag);
	diff_eq_int("the gain was NOT frozen",
		    (int)(unsigned short)after16(RX4_RX_FLAGS) & 0x100, 0, tag);
	record("plain exit at 0x686f1");

	/* 0x69b3c: the detector refuses, but 0x100 is up on the object. */
	begin(RX4_MST, RX4_TXSTATE);
	detector(0);
	step("detect: the detector refuses, 0x69b3c", tag + 1, 207,
	     0, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the gain was frozen before the detector ran",
		    (int)(unsigned short)after16(RX4_RX_FLAGS) & 0x100, 0x100,
		    tag + 1);
	record("detector refuses");

	/* +0xaa80 non-zero raises bit 9 as well. */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA80, 1);
	detector(0);
	step("detect: +0xaa80 raises bit 9 too", tag + 2, 207, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	record("detector refuses with 0xaa80");

	/*
	 * The detector asserts with no message descriptor and bit 4 up: the
	 * round-trip measurement is taken from the receiver's timer.
	 */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_RX_FLAGS, 0x10);
	v34hs_poke_int(RX4_RX_TIMERLO, 0x1234abcd);
	detector(1);
	step("detect: S detected, bit 4 up, rtd committed", tag + 3, 214,
	     1, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("+0x356c", (int)after16(RX4_F356C), 0x1e, tag + 3);
	diff_eq_int("+0x3570", (int)after16(RX4_F3570), 3, tag + 3);
	diff_eq_int("+0x3576", (int)after16(RX4_F3576), 0, tag + 3);
	diff_eq_int("the baud counter was cleared",
		    (int)after16(RX4_RX_F124), 0, tag + 3);
	diff_eq_int("the counter at +0x25da stepped",
		    (int)after16(RX4_PLLCNT), 0x0809, tag + 3);
	diff_eq_int("+0x1c0 raised", (int)after16(RX4_RX_F1C0), 1, tag + 3);
	diff_eq_int("the rtd is the receiver's timer",
		    after32(RX4_RTSCALE), 0x1234abcd, tag + 3);
	record("S detected, rtd committed");

	/*
	 * THE MESSAGE-DESCRIPTOR LENGTH, and the whole expression is 32 bits
	 * wide: +0xaa96 sign-extended, times 0x23d, arithmetic-shifted right
	 * 14, times the ZERO-extended +0x35a2, plus 0x1e, and only the store
	 * narrows.  At baud 3429 and a length of 0x4d that is
	 * ((3429 * 573) >> 14) * 77 + 30 = 119 * 77 + 30 = 9193.
	 */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA96, 3429);
	v34hs_poke_short(RX4_MDLEN, 0x4d);
	detector(1);
	step("detect: the MD length is computed 32 bits wide", tag + 4,
	     211, 2, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the new MD length", (int)after16(RX4_MDLEN), 9193,
		    tag + 4);
	record("MD length computed");

	/*
	 * +0xaa80 SET AND THE DETECTOR ASSERTING, which is the only way bit 9
	 * is up when 0x67b56 masks it off.
	 */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA80, 1);
	v34hs_poke_short(RX4_RX_FLAGS, 0x10);
	detector(1);
	step("detect: bit 9 raised, then masked off at the commit", tag + 5,
	     214, 1, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("bits 8 and 9 are both down afterwards",
		    (int)(unsigned short)after16(RX4_RX_FLAGS) & 0x300, 0,
		    tag + 5);
	record("bit 9 raised and masked");

	/*
	 * A NEGATIVE BAUD RATE, which fixes the sign of +0xaa96 in the MD
	 * computation.  The retrain at the entry has to be declined by bit 7,
	 * because seven times a negative rate is below every counter value a
	 * short can hold.
	 */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_RX_FLAGS, 0x80);
	v34hs_poke_short(RX4_FAA96, -20000);
	v34hs_poke_short(RX4_MDLEN, 0x4d);
	detector(1);
	step("detect: the MD length at a negative baud rate", tag + 6, 211, 2,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the new MD length", (int)after16(RX4_MDLEN),
		    (short)((((-20000 * 0x23d) >> 14) * 0x4d) + 0x1e), tag + 6);
	record("MD length at a negative rate");

	/*
	 * AND A DESCRIPTOR WITH ITS TOP BIT SET, which fixes +0x35a2's sign
	 * in the multiply and the sign of the number the trace prints.
	 */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA96, 3429);
	v34hs_poke_short(RX4_MDLEN, (short)0x8000);
	detector(1);
	step("detect: a descriptor with bit 15 set", tag + 7, 210, 2,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the descriptor is read UNSIGNED in the multiply",
		    (int)after16(RX4_MDLEN),
		    (short)((((3429 * 0x23d) >> 14) * 0x8000) + 0x1e), tag + 7);
	record("MD length from a wide descriptor");
}

/* --- the round-trip scaling, 0x6d111 ----------------------------------- */

/*
 * Four baud rates and no default, all of them a SIGNED 32-bit `sar $0x8` on
 * +0xaacc.  A seed is chosen whose scaled value is negative in three of the
 * four, because that is what an unsigned shift would get wrong; the value is
 * large enough that the scaling is visible at eight bits of precision.
 */
static void
scale_case(const char *what, long tag, short baud, int seed, int want)
{
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA96, baud);
	v34hs_poke_int(RX4_RTSCALE, seed);
	detector(1);
	step(what, tag, 213, 1, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the scaled rtd", after32(RX4_RTSCALE), want, tag);
	record(what);
}

static void
suite_scale(void)
{
	long tag = 300;

	scale_case("scale: 3429 baud, *0xb3>>8", tag, 0xd65, -1234567,
		   ((-1234567) * 0xb3) >> 8);
	scale_case("scale: 3200 baud, *0xc0>>8", tag + 1, 0xc80, -1234567,
		   ((-1234567) * 0xc0) >> 8);
	scale_case("scale: 3000 baud, *205>>8", tag + 2, 0xbb8, -1234567,
		   ((-1234567) * 205) >> 8);
	scale_case("scale: 2800 baud, *0xdb>>8", tag + 3, 0xaf0, -1234567,
		   ((-1234567) * 0xdb) >> 8);

	/* And a rate with no arm, which leaves it alone. */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA96, 0x960);		/* 2400, not an arm */
	v34hs_poke_int(RX4_RTSCALE, -1234567);
	detector(1);
	step("scale: 2400 baud has no arm", tag + 4, 210, 1,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the rtd was left alone", after32(RX4_RTSCALE), -1234567,
		    tag + 4);
	record("scale: no arm");
}

/* --- the message descriptor running out, 0x6a090 ----------------------- */

/*
 * +0x3570 at 3 diverts the whole arm: the descriptor's bauds are counted
 * down and, when they run out, `setupreceiver` reconfigures the demodulator
 * and a SECOND `detectorinit` re-arms the detector with 10/0x28 where the
 * callee used 8/10.
 */
static void
suite_mdover(void)
{
	long tag = 400;

	/* No descriptor: 0x6aad3, which writes only the counter. */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_F3570, 3);
	step("mdover: no descriptor, 0x6aad3", tag, 192, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the descriptor is still zero", (int)after16(RX4_MDLEN), 0,
		    tag);
	diff_eq_int("nothing past the guard ran", (int)after16(RX4_PLLCNT),
		    0x0808, tag);
	record("mdover: none");

	/* Not yet over: 0x6d293, and the counter is what says so. */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_F3570, 3);
	v34hs_poke_short(RX4_MDLEN, 500);
	v34hs_poke_short(RX4_RX_F124, 100);
	step("mdover: not yet, 0x6d293", tag + 1, 193, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the descriptor is untouched", (int)after16(RX4_MDLEN),
		    500, tag + 1);
	diff_eq_int("only the counter moved", (int)after16(RX4_RX_F124), 101,
		    tag + 1);
	record("mdover: not yet");

	/*
	 * OVER.  `setupreceiver` runs, the descriptor and the counter at
	 * +0x25da are cleared, and +0xaa80 with them.  The comparison at
	 * 0x6a0b2 is SIGNED and sixteen bits wide.
	 */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_F3570, 3);
	v34hs_poke_short(RX4_MDLEN, 100);
	v34hs_poke_short(RX4_RX_F124, 500);
	v34hs_poke_short(RX4_FAA96, 0xd65);
	v34hs_poke_short(RX4_RXCARRIER, 0x780);
	v34hs_poke_short(RX4_FAA80, 7);
	step("mdover: over, setupreceiver runs", tag + 2, 197, 3,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the descriptor was cleared", (int)after16(RX4_MDLEN), 0,
		    tag + 2);
	diff_eq_int("+0xaa80 was cleared", (int)after16(RX4_FAA80), 0,
		    tag + 2);
	diff_eq_int("+0x25da was cleared", (int)after16(RX4_PLLCNT), 0,
		    tag + 2);
	diff_eq_int("setupreceiver set the frame length",
		    (int)after16(RX4_RECEIVER + 0x128), 4, tag + 2);
	record("mdover: over");

	/*
	 * THE BOUNDARY, which fixes `<=` against `<`: with the counter
	 * exactly at the descriptor's length the object is NOT over.  The
	 * value compared is the one 0x67a3a has already stepped, so the poke
	 * is one below.
	 */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_F3570, 3);
	v34hs_poke_short(RX4_MDLEN, 100);
	v34hs_poke_short(RX4_RX_F124, 99);
	v34hs_poke_short(RX4_FAA96, 0xd65);
	step("mdover: the counter exactly at the length", tag + 3, 193, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the descriptor is untouched", (int)after16(RX4_MDLEN),
		    100, tag + 3);
	diff_eq_int("the counter reached the length", (int)after16(RX4_RX_F124),
		    100, tag + 3);
	record("mdover: at the boundary");

	/*
	 * AND THE COMPARISON IS SIGNED.  A descriptor with bit 15 set is
	 * NEGATIVE, so every counter value is above it and the object treats
	 * the descriptor as over; an unsigned reading sees 32,768 and waits.
	 */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_F3570, 3);
	v34hs_poke_short(RX4_MDLEN, (short)0x8000);
	v34hs_poke_short(RX4_RX_F124, 100);
	v34hs_poke_short(RX4_FAA96, 0xd65);
	v34hs_poke_short(RX4_FAA80, 7);
	step("mdover: a negative descriptor is always over", tag + 4, 196, 3,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the descriptor was cleared", (int)after16(RX4_MDLEN), 0,
		    tag + 4);
	record("mdover: a negative descriptor");
}

/* --- the far end's J, 0x68e3b ------------------------------------------ */

/*
 * `v34setuptxmit` INLINED, and it is the only path in this arm that moves a
 * state word the caller did not seed: rxstate to 35 WAIT and txstate to 18
 * SSEG.  Both transitions print, which is two of this file's ten
 * diagnostics.
 *
 * The gate is (flags & 0x98) == 0x08 AND +0x359c == 0x65, and the baud
 * counter is held at or below 0x bb8 so the head above does not run the TRN2
 * registers first and change the flags out from under it.
 */
static void
suite_j(void)
{
	long tag = 500;

	/* The marker refuses and the arm carries straight on. */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_RX_FLAGS, 0x08);
	v34hs_poke_short(RX4_F359C, 0);
	detector(0);
	step("J: +0x359c is not 0x65, so no modulator", tag, 207, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	/*
	 * NOT RECORDED: declining the marker leaves the arm on exactly the
	 * path `suite_detect`'s "the detector refuses" takes, so the two
	 * signatures are identical and must be.  What is claimed here is that
	 * `v34setuptxmit` did NOT run, and the two state words say so.
	 */
	diff_eq_int("the transmit machine did not move",
		    (int)v34hs_observed(0)->txst, RX4_TXSTATE, tag);

	/*
	 * And it accepts.  +0x136 is the receive chain's, so the gain
	 * estimate at 0x68f86 cannot be predicted here -- what IS asserted is
	 * that it changed from the seed and that the two state words moved.
	 */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_RX_FLAGS, 0x08);
	v34hs_poke_short(RX4_F359C, 0x65);
	detector(0);
	step("J: detected, v34setuptxmit runs", tag + 1, 1001, 11,
	     RX4_MST, V34HS_WAIT, V34HS_SSEG);
	diff_eq_int("the gain estimate was written",
		    (int)after16(RX4_RX_F262) != 0x0606, 1, tag + 1);
	diff_eq_int("+0x1c0 was cleared", (int)after16(RX4_RX_F1C0), 0,
		    tag + 1);
	diff_eq_int("bit 4 was raised",
		    (int)(unsigned short)after16(RX4_RX_FLAGS) & 0x10, 0x10,
		    tag + 1);
	diff_eq_int("the FIR flag was cleared",
		    (int)(unsigned short)after16(RX4_RX_FLAGS) & 0x800, 0,
		    tag + 1);
	record("J accepted");

	/*
	 * THE GAIN IS SEEDABLE HERE, which no other case in this file needs:
	 * the receive chain leaves +0x136 alone on this path, so the estimate
	 * at 0x68f86 can be driven at a value where 0x6666 and 0x6667 differ.
	 * 5,463 is the SMALLEST gain at which 0x6666 and 0x6667 round to
	 * different shorts -- 4,370 against 4,371 -- and there is no smaller
	 * one, which is why the ratio's last digit needed a sweep rather than
	 * a guess.
	 *
	 * AND THE TWO TIMERS ARE DRIVEN NEGATIVE, because the timing trace
	 * shifts each of them right by sixteen and only an UNSIGNED shift
	 * gives the object's answer for a value with bit 31 set.
	 */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_RX_FLAGS, 0x08);
	v34hs_poke_short(RX4_F359C, 0x65);
	v34hs_poke_short(RX4_RX_AGCGAIN, 5463);
	v34hs_poke_int(RX4_RX_TIMERLO, (int)0x89abcdefu);
	v34hs_poke_int(RX4_RX_TIMERHI, (int)0xfedcba98u);
	detector(0);
	step("J: the gain estimate and two negative timers", tag + 2, 1001, 11,
	     RX4_MST, V34HS_WAIT, V34HS_SSEG);
	diff_eq_int("the gain estimate", (int)after16(RX4_RX_F262),
		    (short)((5463 * 0x6666 + 0x4000) >> 15), tag + 2);
	record("J with a seeded gain");
}

/* --- the TRN2 shift registers, 0x679bc --------------------------------- */

/*
 * Two fourteen-bit registers fed two bits a block, and three sync words.
 * The head runs this only with bit 4 CLEAR and the baud counter above
 * 0xbb8, which is what separates it from the packer.
 */
static void
trn2_begin(void)
{
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_RX_F124, 0xbb9);
	v34hs_poke_short(RX4_FAA96, 3429);	/* 7*3429 is over the counter */
	detector(0);
}

static void
suite_trn2(void)
{
	long tag = 600;

	/* Bit 3 up: one register, and it does not match. */
	trn2_begin();
	v34hs_poke_short(RX4_RX_FLAGS, 0x08);
	v34hs_poke_short(RX4_RX_F11C, 0x1234);
	v34hs_poke_short(RX4_RX_BESTIDX, 1);
	step("trn2: late-TRN register, no match", tag, 203, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the register shifted",
		    (int)(unsigned short)after16(RX4_RX_F11C),
		    ((1 & 3) << 14) | (0x1234 >> 2), tag);
	record("trn2 late, no match");

	/*
	 * And it matches 0x899f.  0x899f is 0b10_0010_0110_0111_11, so the
	 * two new bits are 0b10 and the old register must hold
	 * (0x899f & 0x3fff) << 2 in its top fourteen bits.
	 */
	trn2_begin();
	v34hs_poke_short(RX4_RX_FLAGS, 0x08);
	v34hs_poke_short(RX4_RX_F11C, (short)((0x899f & 0x3fff) << 2));
	v34hs_poke_short(RX4_RX_BESTIDX, 2);
	step("trn2: late-TRN register matches 0x899f", tag + 1, 206,
	     0, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("+0x120 was cleared", (int)after16(RX4_RX_F120), 0,
		    tag + 1);
	diff_eq_int("bit 4 was raised",
		    (int)(unsigned short)after16(RX4_RX_FLAGS) & 0x10, 0x10,
		    tag + 1);
	record("trn2 late, matched");

	/* Bit 3 clear: both registers, and they disagree. */
	trn2_begin();
	v34hs_poke_short(RX4_RX_F11C, 0x1234);
	v34hs_poke_short(RX4_RX_F11E, 0x5678);
	v34hs_poke_short(RX4_RX_BESTIDX, 1);
	step("trn2: two registers, they disagree", tag + 2, 205, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the second register shifted",
		    (int)(unsigned short)after16(RX4_RX_F11E),
		    ((0x1234 & 3) << 14) | (0x5678 >> 2), tag + 2);
	diff_eq_int("the first register shifted",
		    (int)(unsigned short)after16(RX4_RX_F11C),
		    ((1 & 3) << 14) | (0x1234 >> 2), tag + 2);
	record("trn2 both, disagree");

	/*
	 * They agree, on a word that is not one of the two the arm reports.
	 * `b` is ((best & 3) << 14) | (f11c >> 2) and `d` is
	 * ((f11c & 3) << 14) | (f11e >> 2); seeding f11c and f11e to the same
	 * value with best equal to its low two bits makes them equal.
	 */
	trn2_begin();
	v34hs_poke_short(RX4_RX_F11C, 0x1234);
	v34hs_poke_short(RX4_RX_F11E, 0x1234);
	v34hs_poke_short(RX4_RX_BESTIDX, 0x1234 & 3);
	step("trn2: two registers agree, not a reported word", tag + 3,
	     205, 0, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the first register took the agreed word",
		    (int)(unsigned short)after16(RX4_RX_F11C),
		    ((0x1234 & 3) << 14) | (0x1234 >> 2), tag + 3);
	record("trn2 both, agree");

	/*
	 * And they agree on 0x8990, which is what reports the timing offset.
	 * `b` is ((best & 3) << 14) | (f11c >> 2) and `d` is
	 * ((f11c & 3) << 14) | (f11e >> 2), so BOTH come out at 0x8990 with
	 * best = 2, f11c = ((0x8990 & 0x3fff) << 2) | 2 -- its low two bits
	 * are what `d` takes for its top two -- and f11e = (0x8990 & 0x3fff)
	 * << 2.
	 */
	trn2_begin();
	v34hs_poke_short(RX4_RX_F11C, (short)(((0x8990 & 0x3fff) << 2) | 2));
	v34hs_poke_short(RX4_RX_F11E, (short)((0x8990 & 0x3fff) << 2));
	v34hs_poke_short(RX4_RX_BESTIDX, 2);
	v34hs_poke_short(RX4_RX_F1D0, 300);
	step("trn2: agreed on 0x8990, timing reported", tag + 4, 210,
	     0, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the register was cleared",
		    (int)after16(RX4_RX_F11C), 0, tag + 4);
	diff_eq_int("bit 3 was raised",
		    (int)(unsigned short)after16(RX4_RX_FLAGS) & 0x08, 0x08,
		    tag + 4);
	record("trn2 both, 0x8990");

	/* THE THRESHOLD: at exactly 0xbb8 the registers do NOT run. */
	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_RX_F124, 0xbb8);
	v34hs_poke_short(RX4_FAA96, 3429);
	detector(0);
	step("trn2: 0xbb8 exactly does not open it", tag + 5, 201, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the register was left alone",
		    (int)(unsigned short)after16(RX4_RX_F11C), 0x1111,
		    tag + 5);
	record("trn2 at the threshold");

	/* And the OTHER reported sync word, 0x89b0. */
	trn2_begin();
	v34hs_poke_short(RX4_RX_F11C, (short)(((0x89b0 & 0x3fff) << 2) | 2));
	v34hs_poke_short(RX4_RX_F11E, (short)((0x89b0 & 0x3fff) << 2));
	v34hs_poke_short(RX4_RX_BESTIDX, 2);
	v34hs_poke_short(RX4_RX_F1D0, -300);
	step("trn2: agreed on 0x89b0, timing reported", tag + 6, 210, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the register was cleared",
		    (int)after16(RX4_RX_F11C), 0, tag + 6);
	record("trn2 both, 0x89b0");
}

/* --- the MP bit packer, 0x68bd8 ---------------------------------------- */

/*
 * Bit 4 up, bit 10 up and (flags & 0x98) != 0x98 opens the packer; the
 * baud counter is held low so the head cannot take the TRN2 branch instead.
 * Bit 10 also makes the arm leave at 0x686f1 once the packer is done, so
 * every case here is "the packer and nothing else".
 */
static void
packer_begin(short mst)
{
	begin(mst, RX4_TXSTATE);
	v34hs_poke_short(RX4_RX_FLAGS, 0x410);
	detector(0);
}

static void
suite_packer(void)
{
	long tag = 700;
	unsigned acc;

	/* Under seventeen bits: shift in and store, nothing else. */
	packer_begin(RX4_MST);
	v34hs_poke_int(RX4_MPACC, 0);
	v34hs_poke_short(RX4_MPBITS, 4);
	v34hs_poke_short(RX4_RX_BESTIDX, 3);
	step("packer: two more bits, under the limit", tag, 190, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the accumulator took the bits", after32(RX4_MPACC),
		    3 << 4, tag);
	diff_eq_int("the bit count stepped", (int)after16(RX4_MPBITS), 6, tag);
	record("packer: under the limit");

	/*
	 * Seventeen bits with bit 16 SET: the sequence restarts.  The
	 * transition to 41 prints, and its "already there" branch at 0x6ff81
	 * can never fire because 41 is the run-length path's own gate.
	 */
	packer_begin(RX4_MST);
	v34hs_poke_int(RX4_MPACC, 0);
	v34hs_poke_short(RX4_MPBITS, 15);
	v34hs_poke_short(RX4_RX_BESTIDX, 3);
	v34hs_poke_short(RX4_MPIDX, 5);
	v34hs_poke_short(RX4_MPRUN, 6);
	step("packer: bit 16 set restarts the sequence", tag + 1, 194,
	     1, V34HS_DET_SYNC, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the index was cleared", (int)after16(RX4_MPIDX), 0,
		    tag + 1);
	diff_eq_int("the run was cleared", (int)after16(RX4_MPRUN), 0,
		    tag + 1);
	diff_eq_int("the accumulator was stored", after32(RX4_MPACC),
		    3 << 15, tag + 1);
	record("packer: restart");

	/*
	 * SEVENTEEN BITS WITH BIT 16 CLEAR: one word is committed, and THIS
	 * IS THE MASK CLAIM.  The word is 0xabcd, whose bit 15 is SET, and
	 * the table entry afterwards must be 0xabcd and not 0x2bcd -- the
	 * compare at 0x6cb0f masks with 0x7fff and the store at 0x6cb2a does
	 * not.
	 */
	/*
	 * THE SHIFT COUNT HAS TO BE 20 AND NOT 17.  0x68c17 masks the
	 * accumulator to its low `nbits` bits before OR-ing the new pair in,
	 * so a count of 15 destroys bit 15 and everything above it and the
	 * committed word can never have its top bit set.  At 20 the mask
	 * keeps bits 0..19, the new pair lands at 20 and 21, and bit 15 --
	 * the one the claim is about -- survives.
	 */
	acc = 0x8abcdu;
	packer_begin(RX4_MST);
	v34hs_poke_int(RX4_MPACC, (int)acc);
	v34hs_poke_short(RX4_MPBITS, 20);
	v34hs_poke_short(RX4_RX_BESTIDX, 0);
	v34hs_poke_short(RX4_MPIDX, 1);
	v34hs_poke_short(RX4_MPTBL + 2, 0x0000);
	step("packer: a word with bit 15 set is stored UNMASKED", tag + 2,
	     195, 0, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the word went in unmasked",
		    (int)(unsigned short)after16(RX4_MPTBL + 2), 0xabcd,
		    tag + 2);
	diff_eq_int("the accumulator shifted by seventeen",
		    after32(RX4_MPACC), (int)(acc >> 17), tag + 2);
	diff_eq_int("the index stepped", (int)after16(RX4_MPIDX), 2, tag + 2);
	diff_eq_int("the bit count dropped by seventeen",
		    (int)after16(RX4_MPBITS), 22 - 0x11, tag + 2);
	diff_eq_int("the repeat counter did NOT step",
		    (int)after16(RX4_COUNT), 9, tag + 2);
	record("packer: word committed");

	/*
	 * And the SAME word already in the table with its bit 15 CLEAR: the
	 * masked compare says they agree, so the repeat counter steps -- and
	 * the store still puts the unmasked word back.
	 */
	packer_begin(RX4_MST);
	v34hs_poke_int(RX4_MPACC, (int)acc);
	v34hs_poke_short(RX4_MPBITS, 20);
	v34hs_poke_short(RX4_RX_BESTIDX, 0);
	v34hs_poke_short(RX4_MPIDX, 1);
	v34hs_poke_short(RX4_MPTBL + 2, (short)0x2bcd);
	step("packer: the compare masks, so a repeat is seen", tag + 3,
	     195, 0, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the repeat counter stepped", (int)after16(RX4_COUNT), 10,
		    tag + 3);
	diff_eq_int("and the word still went in unmasked",
		    (int)(unsigned short)after16(RX4_MPTBL + 2), 0xabcd,
		    tag + 3);
	record("packer: repeat seen");

	/*
	 * THE HEAD'S MASK IS 0x98 AND NOT 0x90.  With 0x90 raised and bit 3
	 * clear the object still enters the head, so the packer runs; a
	 * reconstruction testing 0x90 would skip it.
	 */
	packer_begin(RX4_MST);
	v34hs_poke_short(RX4_RX_FLAGS, 0x490);
	v34hs_poke_int(RX4_MPACC, 0);
	v34hs_poke_short(RX4_MPBITS, 4);
	v34hs_poke_short(RX4_RX_BESTIDX, 3);
	step("packer: 0x90 raised still enters the head", tag + 4, 190, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the packer ran", after32(RX4_MPACC), 3 << 4, tag + 4);
	/*
	 * NOT RECORDED: the packer does exactly what it does with 0x10 alone,
	 * so the two signatures are identical and must be -- the flags that
	 * separate the cases are in the object BEFORE the step, which finding
	 * 290 says must never go into a signature.  What is claimed is the
	 * store above, which a reading that tested 0x90 would never make.
	 */

	/*
	 * AND THE TWO BRANCHES OF THE HEAD ARE EXCLUSIVE.  With bit 4 up and
	 * the counter above 0xbb8, the object runs the packer and NOT the
	 * TRN2 registers.
	 */
	packer_begin(RX4_MST);
	v34hs_poke_short(RX4_RX_F124, 0xbb9);
	v34hs_poke_short(RX4_FAA96, 3429);
	v34hs_poke_int(RX4_MPACC, 0);
	v34hs_poke_short(RX4_MPBITS, 4);
	v34hs_poke_short(RX4_RX_BESTIDX, 3);
	step("packer: the TRN2 registers do NOT run beside it", tag + 5,
	     218, 0, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the first register was left alone",
		    (int)(unsigned short)after16(RX4_RX_F11C), 0x1111,
		    tag + 5);
	diff_eq_int("and so was the second",
		    (int)(unsigned short)after16(RX4_RX_F11E), 0x2222,
		    tag + 5);
	record("packer: not beside TRN2");

	/*
	 * THE LIMIT IS COMPARED SIGNED.  A bit count whose next value has bit
	 * 15 set is below 0x10 as a short and above it as an unsigned, so the
	 * object stores the accumulator and leaves where an unsigned reading
	 * would go on to restart the sequence.  The shift count that comes
	 * with it is what the object does too -- `shl %cl` takes five bits.
	 */
	packer_begin(RX4_MST);
	v34hs_poke_int(RX4_MPACC, (int)0x00011234u);
	v34hs_poke_short(RX4_MPBITS, (short)0x7ffe);
	v34hs_poke_short(RX4_RX_BESTIDX, 0);
	step("packer: a bit count whose next value is negative", tag + 6,
	     190, 0, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	record("packer: the signed limit");

	/*
	 * THE REPEAT COMPARE MASKS FIFTEEN BITS, not twelve: two words that
	 * agree in their low twelve and differ above them are NOT a repeat.
	 */
	packer_begin(RX4_MST);
	v34hs_poke_int(RX4_MPACC, 0x0bcd);
	v34hs_poke_short(RX4_MPBITS, 20);
	v34hs_poke_short(RX4_RX_BESTIDX, 0);
	v34hs_poke_short(RX4_MPIDX, 1);
	v34hs_poke_short(RX4_MPTBL + 2, 0x1bcd);
	step("packer: agreeing in twelve bits is not a repeat", tag + 7,
	     193, 0, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the repeat counter did not step",
		    (int)after16(RX4_COUNT), 9, tag + 7);
	record("packer: twelve bits is not a repeat");

	/*
	 * AND THE ACCUMULATOR IS SHIFTED UNSIGNED.  Two bits at position 30
	 * put bit 31 up, and an arithmetic shift right by seventeen then
	 * fills with ones where the object fills with zeros.
	 */
	packer_begin(RX4_MST);
	v34hs_poke_int(RX4_MPACC, 0xabcd);
	v34hs_poke_short(RX4_MPBITS, 30);
	v34hs_poke_short(RX4_RX_BESTIDX, 3);
	v34hs_poke_short(RX4_MPIDX, 1);
	v34hs_poke_short(RX4_MPTBL + 2, 0x0000);
	step("packer: the accumulator shifts UNSIGNED", tag + 8, 194, 0,
	     RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the shifted accumulator", after32(RX4_MPACC),
		    (int)(0xc000abcdu >> 17), tag + 8);
	diff_eq_int("and the word went in unmasked",
		    (int)(unsigned short)after16(RX4_MPTBL + 2), 0xabcd,
		    tag + 8);
	record("packer: unsigned shift");
}

/* --- the ends of the sequence, 0x6ff3d --------------------------------- */

/*
 * Ten words if the first one's low bit is set, four if it is not; MP1 if its
 * SIGN bit is set and MP if it is not.  Both arms rejoin at the transition
 * to 41 `DET_SYNC`, and only the MP1 arm copies coefficients.
 */
static void
suite_sequence(void)
{
	long tag = 800;
	unsigned acc = 0x1234u | (1u << 17);
	int i;

	/* Four words, low bit clear, sign bit clear: MP. */
	packer_begin(RX4_MST);
	v34hs_poke_int(RX4_MPACC, (int)acc);
	v34hs_poke_short(RX4_MPBITS, 15);
	v34hs_poke_short(RX4_RX_BESTIDX, 0);
	v34hs_poke_short(RX4_MPIDX, 3);
	v34hs_poke_short(RX4_MPTBL, 0x0100);		/* low bit clear */
	/*
	 * BIT 3 RAISED, which is the only bit the mask at 0x6ff52 and
	 * 0x7028d decides: 0x10 and 0x90 put bits 4 and 7 back whatever the
	 * mask cleared, so a reconstruction clearing 0x90 instead of 0x98
	 * differs here and nowhere else.
	 */
	v34hs_poke_short(RX4_RX_FLAGS, 0x418);
	step("sequence: four words, MP", tag, 197, 2,
	     V34HS_DET_SYNC, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("bit 4 raised and 3 and 7 cleared",
		    (int)(unsigned short)after16(RX4_RX_FLAGS) & 0x98, 0x10,
		    tag);
	diff_eq_int("the index was cleared", (int)after16(RX4_MPIDX), 0, tag);
	record("sequence: MP over four");

	/* Ten words, low bit set: still MP, because the sign bit is clear. */
	packer_begin(RX4_MST);
	v34hs_poke_int(RX4_MPACC, (int)acc);
	v34hs_poke_short(RX4_MPBITS, 15);
	v34hs_poke_short(RX4_RX_BESTIDX, 0);
	v34hs_poke_short(RX4_MPIDX, 9);
	v34hs_poke_short(RX4_MPTBL, 0x0101);		/* low bit set   */
	step("sequence: ten words, MP", tag + 1, 196, 2,
	     V34HS_DET_SYNC, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the coefficients were NOT copied",
		    (int)after16(RX4_MPCOEF), 0, tag + 1);
	record("sequence: MP over ten");

	/*
	 * MP1: the first word's SIGN bit is set, so the flags take 0x90 and
	 * the twelve coefficients at +0x2a68 are filled from words 2..7.
	 */
	packer_begin(RX4_MST);
	v34hs_poke_int(RX4_MPACC, (int)acc);
	v34hs_poke_short(RX4_MPBITS, 15);
	v34hs_poke_short(RX4_RX_BESTIDX, 0);
	v34hs_poke_short(RX4_MPIDX, 9);
	v34hs_poke_short(RX4_RX_FLAGS, 0x418);		/* bit 3, see above */
	v34hs_poke_short(RX4_MPTBL, (short)0x8101);
	for (i = 2; i <= 7; i++)
		v34hs_poke_short(RX4_MPTBL + 2u * (unsigned)i,
				 (short)(0x1100 * i));
	for (i = 0; i < 12; i++)
		v34hs_poke_short(RX4_MPCOEF + 2u * (unsigned)i, 0x7777);
	step("sequence: ten words, MP1, coefficients copied", tag + 2,
	     220, 2, V34HS_DET_SYNC, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("bits 4 and 7 raised",
		    (int)(unsigned short)after16(RX4_RX_FLAGS) & 0x98, 0x90,
		    tag + 2);
	diff_eq_int("coef[0]", (int)after16(RX4_MPCOEF + 0), 0x2200, tag + 2);
	diff_eq_int("coef[7]", (int)after16(RX4_MPCOEF + 14), 0x2200, tag + 2);
	diff_eq_int("coef[1]", (int)after16(RX4_MPCOEF + 2), -0x3300, tag + 2);
	diff_eq_int("coef[6]", (int)after16(RX4_MPCOEF + 12), 0x3300, tag + 2);
	diff_eq_int("coef[2]", (int)after16(RX4_MPCOEF + 4), 0x4400, tag + 2);
	diff_eq_int("coef[9]", (int)after16(RX4_MPCOEF + 18), 0x4400, tag + 2);
	diff_eq_int("coef[3]", (int)after16(RX4_MPCOEF + 6), -0x5500, tag + 2);
	diff_eq_int("coef[8]", (int)after16(RX4_MPCOEF + 16), 0x5500, tag + 2);
	diff_eq_int("coef[4]", (int)after16(RX4_MPCOEF + 8), 0x6600, tag + 2);
	diff_eq_int("coef[11]", (int)after16(RX4_MPCOEF + 22), 0x6600,
		    tag + 2);
	diff_eq_int("coef[5]", (int)after16(RX4_MPCOEF + 10), -0x7700,
		    tag + 2);
	diff_eq_int("coef[10]", (int)after16(RX4_MPCOEF + 20), 0x7700,
		    tag + 2);
	record("sequence: MP1 over ten");

	/*
	 * AND THE TWO COUNTS ARE NOT INTERCHANGEABLE.  With the low bit set,
	 * an index reaching four does NOT end the sequence -- which is what
	 * says 0x6cb54 and 0x6d2de are two tests and not one.
	 */
	packer_begin(RX4_MST);
	v34hs_poke_int(RX4_MPACC, (int)acc);
	v34hs_poke_short(RX4_MPBITS, 15);
	v34hs_poke_short(RX4_RX_BESTIDX, 0);
	v34hs_poke_short(RX4_MPIDX, 3);
	v34hs_poke_short(RX4_MPTBL, 0x0101);
	step("sequence: low bit set, four is not the end", tag + 3, 195,
	     0, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the index simply stepped", (int)after16(RX4_MPIDX), 4,
		    tag + 3);
	record("sequence: four is not ten");

	/*
	 * MP1 IS CHOSEN ON THE SIGN BIT AND NOTHING ELSE.  0x8000 exactly is
	 * negative as a short, has its low bit CLEAR -- so the sequence is
	 * four words and the coefficients are not copied -- and is not
	 * greater than 0x8000 as an unsigned.  One word separates three
	 * readings.
	 */
	packer_begin(RX4_MST);
	v34hs_poke_int(RX4_MPACC, (int)acc);
	v34hs_poke_short(RX4_MPBITS, 15);
	v34hs_poke_short(RX4_RX_BESTIDX, 0);
	v34hs_poke_short(RX4_MPIDX, 3);
	v34hs_poke_short(RX4_MPTBL, (short)0x8000);
	for (i = 2; i <= 7; i++)
		v34hs_poke_short(RX4_MPTBL + 2u * (unsigned)i,
				 (short)(0x1100 * i));
	for (i = 0; i < 12; i++)
		v34hs_poke_short(RX4_MPCOEF + 2u * (unsigned)i, 0x7777);
	step("sequence: 0x8000 exactly is MP1 over four", tag + 4, 197, 2,
	     V34HS_DET_SYNC, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("bits 4 and 7 raised",
		    (int)(unsigned short)after16(RX4_RX_FLAGS) & 0x98, 0x90,
		    tag + 4);
	diff_eq_int("the coefficients were NOT copied",
		    (int)(unsigned short)after16(RX4_MPCOEF), 0x7777, tag + 4);
	record("sequence: MP1 on the sign alone");
}

/* --- the run-length half, 0x6cb70 -------------------------------------- */

/*
 * Microstate 41 sends the packer here instead: the accumulator is shifted a
 * bit at a time and runs of one bits are counted, looking for the seventeen
 * that end MP and the twenty that mark E.
 */
static void
suite_runlength(void)
{
	long tag = 900;

	/* A short run: the counter moves and nothing else. */
	packer_begin(V34HS_DET_SYNC);
	v34hs_poke_int(RX4_MPACC, 0);
	v34hs_poke_short(RX4_MPBITS, 4);
	v34hs_poke_short(RX4_RX_BESTIDX, 3);
	v34hs_poke_short(RX4_MPRUN, 0);
	step("runlength: six bits, two of them ones", tag, 190, 0,
	     V34HS_DET_SYNC, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the accumulator drained", after32(RX4_MPACC), 0, tag);
	diff_eq_int("the bit count drained", (int)after16(RX4_MPBITS), 0, tag);
	diff_eq_int("the run ended at two", (int)after16(RX4_MPRUN), 2, tag);
	record("runlength: short run");

	/*
	 * A run of ones longer than sixteen ENDED BY A ZERO: MP is over and
	 * the microstate goes to 44 `DET_INFO`.  Its "already there" branch
	 * at 0x71275 can never fire, because the microstate is 41 here.
	 */
	packer_begin(V34HS_DET_SYNC);
	v34hs_poke_int(RX4_MPACC, (int)0x0001ffffu);	/* 17 ones then zeros */
	v34hs_poke_short(RX4_MPBITS, 18);
	v34hs_poke_short(RX4_RX_BESTIDX, 0);
	v34hs_poke_short(RX4_MPRUN, 0);
	v34hs_poke_short(RX4_MPIDX, 5);
	step("runlength: seventeen ones then a zero ends MP", tag + 1,
	     195, 1, V34HS_DET_INFO, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the run counter was cleared", (int)after16(RX4_MPRUN), 0,
		    tag + 1);
	diff_eq_int("the index was cleared", (int)after16(RX4_MPIDX), 0,
		    tag + 1);
	diff_eq_int("the trace counter was cleared", (int)after16(RX4_COUNT),
		    0, tag + 1);
	record("runlength: MP over");

	/*
	 * A run of ones over nineteen with the E gate open: E has arrived.
	 * The gate is (flags & 0x98) == 0x90 OR the low bit of +0xaa3c, and
	 * this case uses the second, so the "received before MP'" diagnostic
	 * prints as well.
	 */
	packer_begin(V34HS_DET_SYNC);
	v34hs_poke_byte(RX4_MPCAPS, 1);
	v34hs_poke_int(RX4_MPACC, (int)0xffffffffu);
	v34hs_poke_short(RX4_MPBITS, 30);
	v34hs_poke_short(RX4_RX_BESTIDX, 3);
	v34hs_poke_short(RX4_MPRUN, 0);
	v34hs_poke_short(RX4_MPTBL, 0x0100);		/* low bit clear */
	v34hs_poke_short(RX4_RX_F1D0, 700);
	v34hs_poke_short(RX4_F3598, 0);
	step("runlength: twenty ones is E", tag + 2, 317, 6,
	     V34HS_DET_SYNC, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the flags took 0x98",
		    (int)(unsigned short)after16(RX4_RX_FLAGS) & 0x98, 0x98,
		    tag + 2);
	diff_eq_int("+0x266 was cleared", (int)after16(RX4_RX_F266), 0,
		    tag + 2);
	diff_eq_int("the trace counter was cleared", (int)after16(RX4_COUNT),
		    0, tag + 2);
	diff_eq_int("initdigital ran", (int)after16(RX4_F3598), 1, tag + 2);
	diff_eq_int("+0x218 took 0x4000", (int)after16(RX4_RX_F218), 0x4000,
		    tag + 2);
	record("runlength: E");

	/*
	 * E with the coefficients: the low bit of the first word is set, so
	 * the twelve at +0x2a68 are filled from the same six words the MP1
	 * arm uses -- one construct, two call sites.
	 */
	packer_begin(V34HS_DET_SYNC);
	v34hs_poke_byte(RX4_MPCAPS, 1);
	v34hs_poke_int(RX4_MPACC, (int)0xffffffffu);
	v34hs_poke_short(RX4_MPBITS, 30);
	v34hs_poke_short(RX4_RX_BESTIDX, 3);
	v34hs_poke_short(RX4_MPRUN, 0);
	v34hs_poke_short(RX4_MPTBL, 0x0101);		/* low bit set   */
	v34hs_poke_short(RX4_MPTBL + 4, 0x2200);
	v34hs_poke_short(RX4_MPTBL + 6, 0x3300);
	v34hs_poke_short(RX4_MPCOEF, 0x7777);
	v34hs_poke_short(RX4_MPCOEF + 2, 0x7777);
	step("runlength: E with coefficients", tag + 3, 223, 2,
	     V34HS_DET_SYNC, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("coef[0]", (int)after16(RX4_MPCOEF), 0x2200, tag + 3);
	diff_eq_int("coef[1]", (int)after16(RX4_MPCOEF + 2), -0x3300, tag + 3);
	record("runlength: E with coefficients");

	/*
	 * AND THE E GATE IS A GATE.  The same twenty ones with +0xaa3c clear
	 * and the flags not at 0x90 leaves the run counting past nineteen and
	 * nothing else happening -- so the two halves of 0x6cba6 are two
	 * conditions and not one.
	 */
	packer_begin(V34HS_DET_SYNC);
	v34hs_poke_byte(RX4_MPCAPS, 0);
	v34hs_poke_int(RX4_MPACC, (int)0xffffffffu);
	v34hs_poke_short(RX4_MPBITS, 30);
	v34hs_poke_short(RX4_RX_BESTIDX, 3);
	v34hs_poke_short(RX4_MPRUN, 0);
	step("runlength: the E gate is shut", tag + 4, 194, 0,
	     V34HS_DET_SYNC, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the run counted every bit", (int)after16(RX4_MPRUN), 32,
		    tag + 4);
	diff_eq_int("+0x218 was left alone", (int)after16(RX4_RX_F218), 0x0505,
		    tag + 4);
	record("runlength: E gate shut");

	/*
	 * MP ENDS ON A RUN OF SEVENTEEN, so a run of exactly sixteen followed
	 * by a zero resets the counter and does nothing else.
	 */
	packer_begin(V34HS_DET_SYNC);
	v34hs_poke_int(RX4_MPACC, 0);
	v34hs_poke_short(RX4_MPBITS, 0);
	v34hs_poke_short(RX4_RX_BESTIDX, 0);
	v34hs_poke_short(RX4_MPRUN, 0x10);
	step("runlength: a run of sixteen is not the end", tag + 5, 189, 0,
	     V34HS_DET_SYNC, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the run counter was reset", (int)after16(RX4_MPRUN), 0,
		    tag + 5);
	record("runlength: sixteen is not seventeen");

	/*
	 * AND E'S THRESHOLD IS COMPARED SIGNED.  A run counter whose next
	 * value has bit 15 set is below 0x13 as a short and far above it as
	 * an unsigned, so the object counts on where an unsigned reading
	 * would declare E.
	 */
	packer_begin(V34HS_DET_SYNC);
	v34hs_poke_byte(RX4_MPCAPS, 1);
	v34hs_poke_int(RX4_MPACC, (int)0xffffffffu);
	v34hs_poke_short(RX4_MPBITS, 1);
	v34hs_poke_short(RX4_RX_BESTIDX, 0);
	v34hs_poke_short(RX4_MPRUN, (short)0x7fff);
	step("runlength: a negative run is below the threshold", tag + 6,
	     195, 0, V34HS_DET_SYNC, V34HS_RECEIVE, RX4_TXSTATE);
	/*
	 * The counter reaches 0x8000 on the first bit and is reset by the two
	 * zeros behind it, so what is asserted is that E did NOT fire: an
	 * unsigned reading declares E on that first bit and brings the
	 * digital half up.
	 */
	diff_eq_int("E did not fire", (int)after16(RX4_RX_F218), 0x0505,
		    tag + 6);
	diff_eq_int("and the digital half did not come up",
		    (int)after16(RX4_F3598), 0x0303, tag + 6);
	record("runlength: a negative run");

	/*
	 * E WITH THE FLAGS ALREADY AT 0x90: the gate opens on the flags
	 * rather than on +0xaa3c, and the "received before MP'" report AND
	 * the coefficient copy are both skipped -- 0x710e4 jumps past both.
	 */
	packer_begin(V34HS_DET_SYNC);
	v34hs_poke_short(RX4_RX_FLAGS, 0x490);
	v34hs_poke_byte(RX4_MPCAPS, 0);
	v34hs_poke_int(RX4_MPACC, (int)0xffffffffu);
	v34hs_poke_short(RX4_MPBITS, 4);
	v34hs_poke_short(RX4_RX_BESTIDX, 3);
	v34hs_poke_short(RX4_MPRUN, 0x13);
	v34hs_poke_short(RX4_MPTBL, 0x0101);
	v34hs_poke_short(RX4_MPTBL + 4, 0x2200);
	v34hs_poke_short(RX4_MPCOEF, 0x7777);
	v34hs_poke_short(RX4_F3598, 1);
	step("runlength: E with the flags already at 0x90", tag + 7, 202, 1,
	     V34HS_DET_SYNC, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the coefficients were NOT copied",
		    (int)(unsigned short)after16(RX4_MPCOEF), 0x7777, tag + 7);
	diff_eq_int("the flags took 0x98",
		    (int)(unsigned short)after16(RX4_RX_FLAGS) & 0x98, 0x98,
		    tag + 7);
	record("runlength: E at 0x90");

	/*
	 * AND THE PRECODER FLAG IS RAISED ON BIT 14, not on bit 13.  Bit 14
	 * is seeded and bit 13 is not, so a reconstruction reading the wrong
	 * one raises nothing.
	 */
	packer_begin(V34HS_DET_SYNC);
	v34hs_poke_short(RX4_RX_FLAGS, 0x4410);
	v34hs_poke_byte(RX4_MPCAPS, 1);
	v34hs_poke_int(RX4_MPACC, (int)0xffffffffu);
	v34hs_poke_short(RX4_MPBITS, 4);
	v34hs_poke_short(RX4_RX_BESTIDX, 3);
	v34hs_poke_short(RX4_MPRUN, 0x13);
	v34hs_poke_short(RX4_MPTBL, 0x0100);
	v34hs_poke_short(RX4_F3598, 1);
	step("runlength: bit 14 raises the precoder flag", tag + 8, 203, 2,
	     V34HS_DET_SYNC, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("bit 13 was raised",
		    (int)(unsigned short)after16(RX4_RX_FLAGS) & 0x2000,
		    0x2000, tag + 8);
	record("runlength: the precoder flag");
}

/* --- the two exits that exist only with the diagnostics OFF ------------ */

/*
 * TWO OF THE FIFTEEN EXITS ARE THE DIAGNOSTICS-OFF TWINS OF TWO OTHERS.
 * 0x65453 tests `dsplibs_debug_level` and leaves through 0x690fb when it is
 * up and through 0x6546e when it is not; 0x6a4ba does the same for 0x6d28e
 * and 0x6a4dc.  Every other suite in this file runs with the diagnostics on,
 * so without these two cases thirteen of the fifteen would be driven and the
 * claim would be wrong by exactly the two nobody would look for.
 *
 * The transcript axis is worth nothing here -- both sides print nothing --
 * so each case asserts the field its path wrote instead.
 */
static void
suite_quiet(void)
{
	long tag = 1500;

	v34hs_debug(0);

	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_RX_FLAGS, 0x40);
	step("quiet: the retrain leaves through 0x6546e", tag, NOCHECK, 0,
	     RX4_MST, V34HS_WAIT, V34HS_SILENCERETRAIN);
	/*
	 * NEITHER OF THESE TWO IS RECORDED, and that is the point of them:
	 * the object each writes is byte for byte its diagnostics-on twin's,
	 * because the only difference between the two exits is the lines
	 * nobody printed.  A signature that separated them would be a
	 * signature over something other than the object.
	 */
	diff_eq_int("and it printed nothing",
		    (int)v34hs_observed(0)->lines, 0, tag);

	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA96, 3429);
	v34hs_poke_short(RX4_MDLEN, 0x4d);
	detector(1);
	step("quiet: the MD length leaves through 0x6a4dc", tag + 1, NOCHECK,
	     0, RX4_MST, V34HS_RECEIVE, RX4_TXSTATE);
	diff_eq_int("the new MD length", (int)after16(RX4_MDLEN), 9193,
		    tag + 1);
	diff_eq_int("and it printed nothing",
		    (int)v34hs_observed(0)->lines, 0, tag + 1);

	v34hs_debug(1);
}

/* --- the fixture's own control ----------------------------------------- */

/*
 * The same cases with the BLOB on both sides.
 *
 * A green ours-versus-blob run says nothing unless the same seed is green
 * blob-versus-blob, because then the disagreement could be the fixture's --
 * docs/v34handshak.md's rule for a per-case test, and it matters most for
 * the entry's retrain, where `v34handshakinit` runs inside the step and both
 * sides must be brought up by their own initialisers.
 */
static void
suite_control(void)
{
	long tag = 1600;

	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_RX_FLAGS, 0x40);
	control("control: the retrain, blob on both sides", tag);

	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_RX_FLAGS, 0x08);
	v34hs_poke_short(RX4_F359C, 0x65);
	detector(0);
	control("control: the J path, blob on both sides", tag + 1);

	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_F3570, 3);
	v34hs_poke_short(RX4_MDLEN, 100);
	v34hs_poke_short(RX4_RX_F124, 500);
	v34hs_poke_short(RX4_FAA96, 0xd65);
	control("control: the MD over, blob on both sides", tag + 2);

	packer_begin(V34HS_DET_SYNC);
	v34hs_poke_byte(RX4_MPCAPS, 1);
	v34hs_poke_int(RX4_MPACC, (int)0xffffffffu);
	v34hs_poke_short(RX4_MPBITS, 30);
	v34hs_poke_short(RX4_RX_BESTIDX, 3);
	control("control: E, blob on both sides", tag + 3);

	begin(RX4_MST, RX4_TXSTATE);
	v34hs_poke_short(RX4_FAA96, 20000);
	v34hs_poke_short(RX4_RX_F124, 10000);
	control("control: the wide product, blob on both sides", tag + 4);
}

int
main(void)
{
	int i, j;

	dump = getenv("V34HS_DUMP") != NULL;
	default_fill = getenv("V34HS_SEED") == NULL;
	diff_begin("v34handshak: rxstate 4 RECEIVE, 0x653e4");

	/*
	 * The diagnostics on.  Ten of the arm's own sites print, plus
	 * `hs_setstate`'s at every transition, and the transcripts are
	 * compared line for line -- which is this file's only axis on the
	 * four exits that write nothing.  `StateName` is indexed unbounded
	 * (D42), so every state word driven here stays inside 0..86.
	 */
	v34hs_debug(1);

	suite_entry();
	suite_detect();
	suite_scale();
	suite_mdover();
	suite_j();
	suite_trn2();
	suite_packer();
	suite_sequence();
	suite_runlength();
	suite_quiet();
	suite_control();

	/*
	 * The behaviours claimed distinct, pairwise.  A change collapsing two
	 * of them is a failure rather than a silence (finding 290), and the
	 * count is pinned because a `record()` deleted in an edit would
	 * shrink the check in silence.
	 */
	for (i = 0; i < nsig; i++)
		for (j = i + 1; j < nsig; j++) {
			char msg[160];

			snprintf(msg, sizeof(msg), "%s differs from %s",
				 signame[i], signame[j]);
			diff_eq_int(msg, sig[i] != sig[j], 1,
				    9000 + i * 100 + j);
		}

	diff_eq_int("behaviours recorded", nsig, 56, 0);

	v34hs_holes_check();
	return diff_end();
}

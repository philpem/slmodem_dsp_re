/*
 * t_v34hsrx53.c -- `v34handshak`'s rxstate 53 `DET_AB`, 0x65473.
 *
 * The fourth of the rxstate chain's six exits, driven case by case against
 * the blob through test/harness/v34hsstep.c with `v34hs_ours(1)` putting
 * this tree's `v34handshak` on side A -- so every case here is an ordinary
 * tier-1 differential comparison of the whole 44,096-byte object, the five
 * blocks it points at, the padding around them and both transcripts.
 *
 * WHAT THE ARM IS.  1,632 bytes over ten ranges, of which 861 are live: two
 * entry guards, the tone detector, a threshold on the round-trip delay, and
 * then two bodies chosen by the originate/answer flag at +0x359c.  Every one
 * of the exits is the once-per-block transmit dispatch.
 *
 * THE ONE CLAIM NO SMALL VALUE CAN TEST.  0x6af24 sign-extends BOTH
 * halfwords with `movswl`, adds 0x2418 in a 32-bit register and compares
 * there, because `rtd + 9240` does not fit a short.  Over the values a
 * fixture would naturally pick, a 16-bit spelling agrees with the object on
 * every one of them.  `suite_rtd` therefore drives rtd at 30000, where the
 * 32-bit reading leaves through 0x6af47 and the 16-bit reading -- (short)
 * 39240 is -26296 -- retrains instead, which runs `v34handshakinit` inside
 * the step and prints eight more lines.  Two signatures that could not be
 * further apart.  The same suite drives rtd NEGATIVE, where a `movzwl`
 * reading of it moves the threshold by 65,536.
 *
 * AND `v34handshakinit` RUNS INSIDE THE STEP on the retrain path, so those
 * cases are what finding 359 says `V34HS_REFINIT=1` cannot be used against:
 * side A installs our library tables and side B the blob's, and no address
 * comparison can settle two copies of one table.  `t_v34hst3m41.c` drives
 * microstate 41's retrain the same way.
 *
 * THREE THINGS THAT CANNOT BE POKED, because `V34agc` runs first and writes
 * them: the receiver's +0x10c..+0x113 (the burst the detector reads), its
 * +0x130 (`rx_samples`, the detector's end pointer) and its +0x136 (the
 * gain).  The detector is steered through its own state at +0x3564 instead,
 * verbatim from `t_v34hst3m41.c`, and the gain's restore is made observable
 * by poking the receiver's +0x264 -- object +0x4c8, an unmodelled halfword
 * this arm only reads.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "v34hsstep.h"
#include "dsplib/v34hshak.h"

/* The five inputs of table 2's tail, which can erase what an arm decided. */
#define RX53_LVL_LIMIT	0x0230
#define RX53_LVL_COUNT	0x0234
#define RX53_TIMER_LO	0x0238
#define RX53_TIMER_HI	0x023c
#define RX53_MODE	0x2218

#define RX53_RECEIVER	0x0264
#define RX53_V90RX	0x024c	/* int, `V34SetINFO1aBits`'s first door    */
#define RX53_K56RX	0x0250	/* int, and its second                     */
#define RX53_VECTIDX	0x2aa2	/* short, the trace's [1] and guard 2      */
#define RX53_DET	0x3564	/* struct v34_detector                     */
#define RX53_F358C	0x358c	/* short, zeroed by the INFO1c body        */
#define RX53_F359C	0x359c	/* short, 0x65 is the originating end      */
#define RX53_F35A4	0x35a4	/* short, what the callee folds into +0xa9ac */
#define RX53_BLK_A9AC	0xa9ac	/* the record the INFO1c body fills        */
#define RX53_BLK_A9DC	0xa9dc	/* the second record, INFO1a's length only */
#define RX53_PTR_AA6C	0xaa6c
#define RX53_PTR_AA70	0xaa70
#define RX53_COUNT	0xaa78	/* short, the trace's [2]                  */
#define RX53_RTD	0xaa7e	/* short, the round-trip delay             */
#define RX53_FSK_PHASE	0xaadc
#define RX53_FSK_NEXT	0xaade	/* the one the tail sets to SIX            */
#define RX53_FSK_NBITS	0xaae0
#define RX53_FSK_SR	0xaae2

#define RX53_RX_FLAGS	(RX53_RECEIVER + 0x122)
#define RX53_RX_F264	(RX53_RECEIVER + 0x264)	/* object +0x4c8 */

/* struct v34_detector at +0x3564, fields from v34det.h. */
#define RX53_DET_POL	(RX53_DET + 0x04)
#define RX53_DET_ARMED	(RX53_DET + 0x06)
#define RX53_DET_COUNT	(RX53_DET + 0x08)
#define RX53_DET_LIMIT	(RX53_DET + 0x0a)
#define RX53_DET_STATE	(RX53_DET + 0x0c)
#define RX53_DET_THI	(RX53_DET + 0x0e)
#define RX53_DET_TLO	(RX53_DET + 0x10)

/*
 * Where the two record pointers are aimed BEFORE the step.
 *
 * The INFO1c body re-aims both -- +0xaa6c at +0xa9ac and +0xaa70 at +0xa9dc
 * -- so seeding them somewhere else is what makes the re-aiming observable.
 * Both are interior pointers, which the harness compares by offset from each
 * side's own base; `v34hs_poke_self_ptr` aims each side at its OWN object
 * because one address written into both is precisely the asymmetry findings
 * 319-322 are about.
 */
#define RX53_REC_AA70	0x8100
#define RX53_REC_AA6C	0x8200
#define RX53_DET_COEFF	0x8000

/*
 * The txstate every case that does not force its own is driven with.
 *
 * 81 MOH_SILENCE is above table 2's window and selects the once-per-block
 * dispatch's own default at 0x62a40, which is written; the arm's two bodies
 * force 24 TX_DPSK and 60 TONE_AB, both of which select table 2's 0x644c9,
 * which `t_v34hstbl2.c` proves.  Nothing here can reach a table-2 arm the
 * partial `v34handshak` would halt on.
 */
#define RX53_TXSTATE	V34HS_MOH_SILENCE

static int dump;
static int default_fill;

/*
 * Open a case: the route, the three state words, table 2's five inputs, and
 * every field this arm writes seeded to something it does NOT write.
 *
 * That last part is finding 345's failure mode turned on this arm.  The tail
 * stores zero to six halfwords and SIX to +0xaade, and a field already
 * holding what a store writes makes the store invisible and its mutation
 * equivalent.  `+0xaade == 6` is the collision the fill could plausibly hand
 * us; all seven are seeded rather than reasoned about.
 */
static void
begin(short mst, short txst)
{
	unsigned off;

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(mst, V34HS_DET_AB, txst);

	v34hs_poke_int(RX53_MODE, 0);
	v34hs_poke_int(RX53_TIMER_LO, 1000);
	v34hs_poke_int(RX53_TIMER_HI, 2000);
	v34hs_poke_int(RX53_LVL_LIMIT, 0x7fffffff);
	v34hs_poke_int(RX53_LVL_COUNT, 0);

	/* The arm's four inputs, so no case inherits one from the fill. */
	v34hs_poke_short(RX53_VECTIDX, 100);	/* below guard 2's floor */
	v34hs_poke_short(RX53_RTD, 0);
	v34hs_poke_short(RX53_F359C, 0);

	/*
	 * NEITHER PCM RECEIVER RUNNING, and this is a fixture requirement
	 * rather than a choice about the arm.  `V34SetINFO1aBits` reads
	 * `*(int *)(obj->pac18 + 0x24)` inside its K56Flex branch, and +0xac18
	 * is not one of the thirty-five pointers `v34hs_setup` aims -- so a
	 * fill that leaves +0x250 non-zero faults inside the callee on BOTH
	 * sides.  With both counters clear the callee takes its "neither
	 * running" path, which writes the two halfwords below and returns.
	 */
	v34hs_poke_int(RX53_V90RX, 0);
	v34hs_poke_int(RX53_K56RX, 0);
	/*
	 * And the seven-bit field it folds into the record, seeded so that
	 * fold is a CHANGE: 0x7f reverses to 0x7f, which ORs 3 into the
	 * record's +0x00 and 0xf8 into its +0x02.  That is what makes the
	 * second argument of the call observable at all.
	 */
	v34hs_poke_short(RX53_F35A4, 0x7f);

	/* The seven the tail writes, and the one the INFO1c body writes. */
	v34hs_poke_short(RX53_FSK_SR, 0x1111);
	v34hs_poke_short(RX53_FSK_NBITS, 0x2222);
	v34hs_poke_short(RX53_FSK_PHASE, 0x3333);
	v34hs_poke_short(RX53_FSK_NEXT, 0x4444);
	v34hs_poke_short(RX53_COUNT, 0x55);
	v34hs_poke_short(RX53_F358C, 0x66);

	/*
	 * The receiver's flags with bit 9 CLEAR, so the tail's `|= 0x200` is
	 * a change rather than a no-op -- and +0x264 distinct from anything
	 * `V34agc` would leave in the gain it is copied into.
	 */
	v34hs_poke_short(RX53_RX_FLAGS, 0);
	v34hs_poke_short(RX53_RX_F264, 0x2a2a);

	/*
	 * The record at +0xa9ac, seeded so that each of the twelve stores is
	 * a change -- INCLUDING the two 32-bit ones, whose upper halfwords at
	 * +0x26 and +0x2e a `movw` spelling would leave alone.
	 */
	for (off = 0; off < 0x30; off += 2)
		v34hs_poke_short(RX53_BLK_A9AC + off,
				 (short)(0x7b00 + (int)off));
	v34hs_poke_short(RX53_BLK_A9DC + 0x18, 0x1234);

	v34hs_poke_self_ptr(RX53_PTR_AA70, RX53_REC_AA70);
	v34hs_poke_self_ptr(RX53_PTR_AA6C, RX53_REC_AA6C);
}

/*
 * Make the detector assert, or refuse to.  Verbatim from `t_v34hst3m41.c`:
 * refusing is done through the warm-up counter, which returns before the
 * level is consulted, so neither answer depends on what the filter made of
 * `V34agc`'s output.
 */
static void
detector(int assert_it)
{
	v34hs_poke_self_ptr(RX53_DET + 0x00, RX53_DET_COEFF);
	v34hs_poke_short(RX53_DET + 0x12, 300);		/* level   */
	v34hs_poke_short(RX53_DET + 0x14, 11);		/* x[0][0] */
	v34hs_poke_short(RX53_DET + 0x16, -22);
	v34hs_poke_short(RX53_DET + 0x18, 33);
	v34hs_poke_short(RX53_DET + 0x1a, -44);
	v34hs_poke_short(RX53_DET + 0x1c, 55);		/* y[0][0] */
	v34hs_poke_short(RX53_DET + 0x1e, -66);
	v34hs_poke_short(RX53_DET + 0x20, 77);
	v34hs_poke_short(RX53_DET + 0x22, -88);

	v34hs_poke_short(RX53_DET_POL, 0);		/* presence */
	v34hs_poke_short(RX53_DET_ARMED, 1);
	v34hs_poke_short(RX53_DET_THI, 0);
	v34hs_poke_short(RX53_DET_TLO, (short)0x8000);

	if (assert_it) {
		v34hs_poke_short(RX53_DET_STATE, 2);
		v34hs_poke_short(RX53_DET_COUNT, 0);
		v34hs_poke_short(RX53_DET_LIMIT, -1);
	} else {
		v34hs_poke_short(RX53_DET_STATE, 1);
		v34hs_poke_short(RX53_DET_COUNT, -1000);
		v34hs_poke_short(RX53_DET_LIMIT, 0x7fff);
	}
}

#define NOCHECK	((unsigned)-1)

static unsigned last_hash;
static unsigned last_changed;
static unsigned last_lines;

/*
 * Step, compare, and say what the step DID.
 *
 * The comparison is the differential check; the assertions after it are the
 * anti-vacuity half.  A case that left one guard earlier than the name says
 * still compares -- both sides left -- so without them a seed that stopped
 * reaching a body would leave this file green and testing nothing.
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
	last_changed = o->changed;
	last_lines = o->lines;
	if (dump)
		printf("  %-44s changed %4u  lines %2u  mst %2d rx %2d tx %2d "
		       "hash %08x\n", what, o->changed, o->lines,
		       o->mst, o->rxst, o->txst, o->hash);

	/*
	 * BOTH OF THESE ARE PROPERTIES OF THE FILL AS WELL AS OF THE ARM, so
	 * both are asserted at the default fixture only.  `changed` counts
	 * bytes differing from what the fill left, which finding 359 measured
	 * moving by a byte or two across seeds; and `lines` moves because
	 * `probeselect` -- which the INFO1c body calls -- prints a number of
	 * diagnostics that depends on the probe results.
	 *
	 * WHICH CASES, MEASURED AND NOT REASONED.  Exactly the five that
	 * reach the INFO1c body move: 13 lines at the default fill, 14 at
	 * seeds 3 and 10, 15 at seed 7.  The state-only and retrain cases do
	 * NOT move, which is what says the variation is `probeselect`'s and
	 * not `v34handshakinit`'s -- the retrain runs the latter and its nine
	 * lines are the same at every seed.  `V34SetINFO1aBits` is not a
	 * candidate either: with both PCM receivers clear it returns at
	 * v34info1a.cpp's `if (obj->v90_receiver == 0)` before reaching any
	 * debug site, so it prints nothing on this path.
	 *
	 * The transcript itself is compared LINE FOR LINE at every seed by
	 * `v34hs_compare` above; what is gated here is only the count.
	 *
	 * No conversion in these: `diff_eq_int` appends the input itself.
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

/*
 * The distinct behaviours this file claims, so that a change collapsing two
 * of them is a failure rather than a silence (finding 290).
 */
#define NSIG	16
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

/* --- the two entry guards ---------------------------------------------- */

/*
 * 0x65486 and 0x6845e.
 *
 * BOTH ARE DRIVEN AGAINST A BODY AND NOT AGAINST EACH OTHER.  The three
 * early exits do nothing an exit does not -- they run `V34agc` and go to the
 * transmit dispatch -- so they are byte for byte the same step and a pair of
 * trials on either side of one guard would separate nothing.  Each pair here
 * therefore has the rest of the arm seeded for the INFO1c body, so failing
 * the guard is the difference between five bytes and a hundred.
 */
static void
suite_guards(void)
{
	long tag = 1000;
	unsigned h_out;

	/* Microstate 52 is TX_L2.  51 is TX_L1, one away, and leaves. */
	begin(V34HS_TX_L1, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0x65);
	detector(1);
	step("guard 1: microstate 51, not 52", tag, 11, 0,
	     V34HS_TX_L1, V34HS_DET_AB, RX53_TXSTATE);
	h_out = last_hash;
	record("early exit");

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0x65);
	detector(1);
	step("guard 1: microstate 52 goes on", tag + 1, 149, 13,
	     V34HS_INFODONE, V34HS_RX_DPSK, V34HS_TX_DPSK);
	diff_eq_int("microstate 52 does NOT take the early exit",
		    last_hash != h_out, 1, tag + 1);

	/* 53 DET_AB is one above 52, and is the rxstate, not the microstate. */
	begin(V34HS_DET_AB, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0x65);
	detector(1);
	step("guard 1: microstate 53, not 52", tag + 2, 11, 0,
	     V34HS_DET_AB, V34HS_DET_AB, RX53_TXSTATE);
	diff_eq_int("microstate 53 leaves as 51 did", last_hash, h_out,
		    tag + 2);

	/* `cmpw $0x5db` + `jg`, so 0x5db stays and 0x5dc goes. */
	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5db);
	v34hs_poke_short(RX53_F359C, 0x65);
	detector(1);
	step("guard 2: vect_idx 0x5db, the boundary", tag + 3, 11, 0,
	     V34HS_TX_L2, V34HS_DET_AB, RX53_TXSTATE);
	diff_eq_int("0x5db leaves as a failed guard 1 does", last_hash, h_out,
		    tag + 3);

	/*
	 * SIGNED, sixteen bits.  -1 stays; an unsigned read would make it
	 * 65,535 and take the whole body.  Nothing else in the fixture
	 * separates the two spellings.
	 */
	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, -1);
	v34hs_poke_short(RX53_F359C, 0x65);
	detector(1);
	step("guard 2: vect_idx -1 is below the floor", tag + 4, 11, 0,
	     V34HS_TX_L2, V34HS_DET_AB, RX53_TXSTATE);
	diff_eq_int("-1 leaves as 0x5db does", last_hash, h_out, tag + 4);
}

/* --- the detector, and the round-trip-delay threshold ------------------ */

/*
 * 0x69815 and 0x6af24.
 *
 * A SILENT detector is the retrain half of the arm; an asserting one is the
 * end of L2.  That is the one lever separating the arm's two halves, and it
 * is the detector's own state machine rather than anything about the samples,
 * which `V34agc` writes on every step.
 */
static void
suite_rtd(void)
{
	long tag = 1100;
	unsigned h_quiet, h_retrain;

	/* Silent, and 20000 is not past 400 + 9240: the arm leaves. */
	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 9640);
	v34hs_poke_short(RX53_RTD, 400);
	detector(0);
	step("rtd: vect_idx == rtd + 0x2418, the boundary", tag, 25, 0,
	     V34HS_TX_L2, V34HS_DET_AB, RX53_TXSTATE);
	h_quiet = last_hash;
	record("silent, below the threshold");

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 9641);
	v34hs_poke_short(RX53_RTD, 400);
	detector(0);
	step("rtd: one past it retrains", tag + 1, 60, 9,
	     V34HS_TX_L2, V34HS_WAIT, V34HS_SILENCERETRAIN);
	h_retrain = last_hash;
	record("retrain");
	diff_eq_int("one past the threshold is not the same step",
		    h_retrain != h_quiet, 1, tag + 1);

	/*
	 * THE WIDTH OF THE SUM, and the reason this file exists in the shape
	 * it does.  30000 + 0x2418 is 39,240, which the object computes in a
	 * 32-bit register: 1600 is not past it and the arm leaves.  Truncated
	 * to a short the sum is -26,296, 1600 IS past it, and the step
	 * retrains -- runs `v34handshakinit`, moves three state words and
	 * prints eight more lines.  No small value can tell the two apart.
	 */
	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 1600);
	v34hs_poke_short(RX53_RTD, 30000);
	detector(0);
	step("rtd: 30000 + 0x2418 does not fit a short", tag + 2, 25, 0,
	     V34HS_TX_L2, V34HS_DET_AB, RX53_TXSTATE);
	diff_eq_int("and the sum is computed thirty-two bits wide",
		    last_hash, h_quiet, tag + 2);

	/*
	 * AND THE DELAY ITSELF IS SIGNED.  -1 puts the threshold at 9,239, so
	 * 9,240 retrains; read with `movzwl` the threshold would be 74,775
	 * and no vect_idx a short can hold could reach it.
	 */
	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 9240);
	v34hs_poke_short(RX53_RTD, -1);
	detector(0);
	step("rtd: -1 lowers the threshold by one", tag + 3, 60, 9,
	     V34HS_TX_L2, V34HS_WAIT, V34HS_SILENCERETRAIN);
	diff_eq_int("a negative delay retrains where an unsigned one could not",
		    last_hash != h_quiet, 1, tag + 3);

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 9239);
	v34hs_poke_short(RX53_RTD, -1);
	detector(0);
	step("rtd: -1 and one below it", tag + 4, 25, 0,
	     V34HS_TX_L2, V34HS_DET_AB, RX53_TXSTATE);
	diff_eq_int("one below the lowered threshold leaves",
		    last_hash, h_quiet, tag + 4);

	/* rtd 0 pins the constant on its own: 0x2418 exactly, and `>`. */
	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x2418);
	v34hs_poke_short(RX53_RTD, 0);
	detector(0);
	step("rtd: 0, vect_idx == 0x2418", tag + 5, 25, 0,
	     V34HS_TX_L2, V34HS_DET_AB, RX53_TXSTATE);
	diff_eq_int("0x2418 exactly leaves", last_hash, h_quiet, tag + 5);

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x2419);
	v34hs_poke_short(RX53_RTD, 0);
	detector(0);
	step("rtd: 0, vect_idx == 0x2419", tag + 6, 60, 9,
	     V34HS_TX_L2, V34HS_WAIT, V34HS_SILENCERETRAIN);
	diff_eq_int("0x2419 retrains", last_hash != h_quiet, 1, tag + 6);
}

/* --- the two bodies ---------------------------------------------------- */

/*
 * 0x69888 and 0x6b138, chosen by +0x359c.
 *
 * 0x65 is the originating end everywhere in this tree, and here it is the
 * end that builds INFO1c: the record at +0xa9ac gets length 0x4d, and the
 * 0x26 that IS INFO1a's length goes to a second record at +0xa9dc.
 */
static void
suite_bodies(void)
{
	long tag = 1200;
	unsigned h_info1c, h_states;
	unsigned l_info1c, l_states;

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0);
	detector(1);
	step("body: +0x359c not 0x65, three state words", tag, 46, 4,
	     V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, V34HS_TONE_AB);
	h_states = last_hash;
	l_states = last_lines;
	record("state-only");

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0x65);
	detector(1);
	step("body: +0x359c == 0x65, INFO1c", tag + 1, 149, 13,
	     V34HS_INFODONE, V34HS_RX_DPSK, V34HS_TX_DPSK);
	h_info1c = last_hash;
	l_info1c = last_lines;
	record("INFO1c");
	diff_eq_int("the two bodies are not one", h_info1c != h_states, 1,
		    tag + 1);
	diff_eq_int("and INFO1c writes more of the object",
		    last_changed > 40, 1, tag + 1);

	/*
	 * 0x66 is not 0x65.  The flag is compared for EQUALITY against one
	 * value, so the answering end is every value but one -- and 0x66 is
	 * the one the rest of this file's neighbours use for it.
	 */
	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0x66);
	detector(1);
	step("body: +0x359c == 0x66 is the answer end", tag + 2, 46, 4,
	     V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, V34HS_TONE_AB);
	diff_eq_int("0x66 writes what every non-0x65 value does",
		    last_hash, h_states, tag + 2);

	/*
	 * THE DECLINED TRANSITION.  `hs_setstate` prints only when the word
	 * moves, so entering with the txstate the body is about to set costs
	 * one line and changes two bytes fewer.  That is the only observable
	 * difference between the two runs, and it is a transcript one.
	 */
	begin(V34HS_TX_L2, V34HS_TX_DPSK);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0x65);
	detector(1);
	step("body: INFO1c entered already at TX_DPSK", tag + 3, 148, 12,
	     V34HS_INFODONE, V34HS_RX_DPSK, V34HS_TX_DPSK);
	record("INFO1c, txstate already there");
	/*
	 * ONE LINE FEWER, asserted as a DIFFERENCE rather than as a count.
	 * The absolute counts move with the fill -- see `step` -- but the
	 * declined transition is exactly one line whatever `probeselect`
	 * said, so this is the form of the claim that holds at every seed.
	 */
	diff_eq_int("the declined transition costs exactly one line",
		    l_info1c - last_lines, 1, tag + 3);

	begin(V34HS_TX_L2, V34HS_TONE_AB);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0);
	detector(1);
	step("body: state-only entered already at TONE_AB", tag + 4, 45,
	     3, V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, V34HS_TONE_AB);
	record("state-only, txstate already there");
	diff_eq_int("and the state-only body's declined transition too",
		    l_states - last_lines, 1, tag + 4);
}

/* --- the tail both bodies reach ---------------------------------------- */

/*
 * 0x69924, and every one of its eight stores is checked by hand as well as
 * by the whole-object comparison.
 *
 * The comparison is the check; these are the anti-vacuity half, and they say
 * WHICH byte moved rather than that some byte did.  Reading side A's object
 * directly is what makes a store landing at the wrong offset a named failure
 * instead of a hash that differs for no stated reason.
 */
static void
suite_tail(void)
{
	long tag = 1300;
	int i;

	static const struct {
		unsigned	off;
		short		want;
		const char	*name;
	} stores[] = {
		{ RX53_FSK_SR,		0,	"fsk.sr at +0xaae2"	},
		{ RX53_FSK_NBITS,	0,	"fsk.nbits at +0xaae0"	},
		{ RX53_FSK_PHASE,	0,	"fsk.phase at +0xaadc"	},
		{ RX53_FSK_NEXT,	6,	"fsk.next at +0xaade"	},
		{ RX53_VECTIDX,		0,	"vect_idx at +0x2aa2"	},
		{ RX53_COUNT,		0,	"the counter at +0xaa78" }
	};

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0);
	detector(1);
	step("tail: the six zero-stores and the six", tag, 46, 4,
	     V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, V34HS_TONE_AB);

	for (i = 0; i < (int)(sizeof(stores) / sizeof(stores[0])); i++)
		diff_eq_int(stores[i].name,
			    v34hs_peek_short(0, stores[i].off),
			    stores[i].want, tag);

	/*
	 * The receiver's flags get bit 9 SET and nothing else touched, and
	 * the gain is restored from the receiver's +0x264 -- which is why
	 * that halfword is seeded to a value `V34agc` would not leave.
	 */
	diff_eq_int("the receiver's flags have bit 9 set",
		    v34hs_peek_short(0, RX53_RX_FLAGS) & 0x200, 0x200, tag);
	diff_eq_int("and the gain came from the receiver's +0x264",
		    v34hs_peek_short(0, RX53_RECEIVER + 0x136), 0x2a2a, tag);

	/*
	 * +0x358c is the INFO1c body's, NOT the tail's: the state-only case
	 * above must leave it exactly as it was seeded.
	 */
	diff_eq_int("+0x358c is untouched by the state-only body",
		    v34hs_peek_short(0, RX53_F358C), 0x66, tag);

	/*
	 * AND THE FLAG IS ADDED, NOT ASSIGNED.  Every other case here enters
	 * with the receiver's flags at zero -- which is what makes the store
	 * visible at all -- and over that one seed `flags = 0x200` and
	 * `flags |= 0x200` are the same function.  `tools/mutate.py` said so
	 * before this trial existed.  0x4000 is a bit no `V34_RX_FLAG_*` in
	 * v34recv.h claims, so it changes nothing but its own survival.
	 */
	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0);
	v34hs_poke_short(RX53_RX_FLAGS, 0x4000);
	detector(1);
	step("tail: the flag is ORed into what was there", tag + 1, 46, 4,
	     V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, V34HS_TONE_AB);
	diff_eq_int("the unrelated flag bit survives the OR",
		    (unsigned short)v34hs_peek_short(0, RX53_RX_FLAGS),
		    0x4200, tag + 1);
}

/* --- the record the INFO1c body fills ---------------------------------- */

/*
 * 0x6b1fb..0x6b24c, twelve stores into the record at +0xa9ac.
 *
 * TWO OF THE TWELVE ARE `movl` AND NOT `movw`.  +0x24 and +0x2c take the
 * immediate 0x0000ff72, which is +65,394 and four bytes wide, so the
 * halfword above each is zeroed too.  The seed puts something else there and
 * both halves are read back.
 *
 * The length field is what says INFO1c rather than INFO1a: +0x18 of THIS
 * record gets 0x4d, and 0x26 -- INFO1a's length -- goes to +0x18 of a second
 * record at +0xa9dc, which the body also aims +0xaa70 at.
 */
static void
suite_record(void)
{
	long tag = 1400;
	int i;

	static const struct {
		unsigned	off;
		short		want;
		const char	*name;
	} rec[] = {
		{ 0x14, -1,	"record +0x14 = -1"		},
		{ 0x16,  1,	"record +0x16 = 1"		},
		{ 0x18,  0x4d,	"record +0x18 = 0x4d, INFO1c"	},
		{ 0x1a,  0,	"record +0x1a = 0"		},
		{ 0x1c,  8,	"record +0x1c = 8"		},
		{ 0x1e,  0,	"record +0x1e = 0"		},
		{ 0x20,  0,	"record +0x20 = 0"		},
		{ 0x22,  0,	"record +0x22 = 0"		},
		{ 0x24, (short)0xff72, "record +0x24 low half"	},
		{ 0x26,  0,	"record +0x26, the 32-bit store's upper half" },
		{ 0x28,  0x10,	"record +0x28 = 0x10"		},
		{ 0x2a,  0x10,	"record +0x2a = 0x10"		},
		{ 0x2c, (short)0xff72, "record +0x2c low half"	},
		{ 0x2e,  0,	"record +0x2e, the 32-bit store's upper half" }
	};

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0x65);
	detector(1);
	step("record: INFO1c built at +0xa9ac", tag, 149, 13,
	     V34HS_INFODONE, V34HS_RX_DPSK, V34HS_TX_DPSK);

	for (i = 0; i < (int)(sizeof(rec) / sizeof(rec[0])); i++)
		diff_eq_int(rec[i].name,
			    v34hs_peek_short(0, RX53_BLK_A9AC + rec[i].off),
			    rec[i].want, tag);

	/*
	 * AND THE SECOND ARGUMENT OF `V34SetINFO1aBits` IS THIS RECORD.  With
	 * neither PCM receiver running the callee ORs the reversed seven-bit
	 * field into `bits[0]` and `bits[1]`; seeded at 0x7f it reverses to
	 * 0x7f, which is 3 into the first and 0xf8 into the second.  A call
	 * handed +0xa9dc, or +0xaa6c's old target, leaves those bits at
	 * whatever `probeselect` put there.
	 *
	 * THE BITS AND NOT THE WORDS, because `probeselect` runs first and
	 * writes the same two halfwords from the probe results -- so the rest
	 * of each word is the fill's and moves with the seed, which is how
	 * this pair was found to be reading more than it could claim.
	 */
	diff_eq_int("the callee folded the field into record +0x00",
		    v34hs_peek_short(0, RX53_BLK_A9AC + 0) & 3, 3, tag);
	diff_eq_int("and into record +0x02",
		    v34hs_peek_short(0, RX53_BLK_A9AC + 2) & 0xf8, 0xf8, tag);

	diff_eq_int("+0xa9dc + 0x18 = 0x26, INFO1a's length",
		    v34hs_peek_short(0, RX53_BLK_A9DC + 0x18), 0x26, tag);
	diff_eq_int("+0x358c cleared by the INFO1c body",
		    v34hs_peek_short(0, RX53_F358C), 0, tag);
}

/* --- both sides of every debug guard ----------------------------------- */

/*
 * The cases above run with the diagnostics ON, which leaves the other half
 * of each `if (DSPLIB_DEBUG_ON())` undriven -- and a store moved inside one
 * of them undetectable.  `t_v34hst3m41.c` measured that rather than reasoned
 * it: the mutation was applied by hand and the file passed.  So every body
 * that prints is re-driven quiet and asserted to write the same bytes and no
 * lines.
 */
static void
suite_quiet(void)
{
	long tag = 1500;
	unsigned h_states, h_info1c, h_retrain;

	v34hs_debug(1);

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0);
	detector(1);
	step("loud: state-only", tag, 46, 4,
	     V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, V34HS_TONE_AB);
	h_states = last_hash;

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0x65);
	detector(1);
	step("loud: INFO1c", tag + 1, 149, 13,
	     V34HS_INFODONE, V34HS_RX_DPSK, V34HS_TX_DPSK);
	h_info1c = last_hash;

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 9641);
	v34hs_poke_short(RX53_RTD, 400);
	detector(0);
	step("loud: retrain", tag + 2, 60, 9,
	     V34HS_TX_L2, V34HS_WAIT, V34HS_SILENCERETRAIN);
	h_retrain = last_hash;

	v34hs_debug(0);

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0);
	detector(1);
	step("quiet: state-only", tag + 3, 46, 0,
	     V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, V34HS_TONE_AB);
	diff_eq_int("quiet state-only writes what the loud one did",
		    last_hash, h_states, tag + 3);

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0x65);
	detector(1);
	step("quiet: INFO1c", tag + 4, 149, 0,
	     V34HS_INFODONE, V34HS_RX_DPSK, V34HS_TX_DPSK);
	diff_eq_int("quiet INFO1c writes what the loud one did",
		    last_hash, h_info1c, tag + 4);

	/*
	 * The retrain is the strongest of the three: `v34handshakinit` runs
	 * inside the step either way and its own eight lines are what the
	 * loud twin printed.
	 */
	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 9641);
	v34hs_poke_short(RX53_RTD, 400);
	detector(0);
	step("quiet: retrain", tag + 5, 60, 0,
	     V34HS_TX_L2, V34HS_WAIT, V34HS_SILENCERETRAIN);
	diff_eq_int("quiet retrain writes what the loud one did",
		    last_hash, h_retrain, tag + 5);

	v34hs_debug(1);
}

/* --- the fixture's own control ----------------------------------------- */

/*
 * The same five cases with the BLOB ON BOTH SIDES.
 *
 * A green ours-versus-blob run says nothing unless the same seed is green
 * blob-versus-blob, because then the disagreement could be the fixture's --
 * docs/v34handshak.md's rule for a per-case test, and the retrain case is
 * the one it matters most for, since `v34handshakinit` runs inside the step
 * there and both sides must be brought up by their own initialisers.
 */
static void
suite_control(void)
{
	long tag = 1600;

	begin(V34HS_TX_L1, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	detector(1);
	control("control: the early exit, blob on both sides", tag);

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0);
	detector(1);
	control("control: state-only, blob on both sides", tag + 1);

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 0x5dc);
	v34hs_poke_short(RX53_F359C, 0x65);
	detector(1);
	control("control: INFO1c, blob on both sides", tag + 2);

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 9641);
	v34hs_poke_short(RX53_RTD, 400);
	detector(0);
	control("control: retrain, blob on both sides", tag + 3);

	begin(V34HS_TX_L2, RX53_TXSTATE);
	v34hs_poke_short(RX53_VECTIDX, 1600);
	v34hs_poke_short(RX53_RTD, 30000);
	detector(0);
	control("control: the wide sum, blob on both sides", tag + 4);
}

int
main(void)
{
	int i, j;

	dump = getenv("V34HS_DUMP") != NULL;
	default_fill = getenv("V34HS_SEED") == NULL;
	diff_begin("v34handshak: rxstate 53 DET_AB, 0x65473");

	/*
	 * The diagnostics on.  Nine of the arm's sites print -- six of them
	 * `hs_setstate`'s own -- and the transcripts are compared line for
	 * line, which is this file's check on `StateName` and on the three
	 * literals the arm owns.  `StateName` is indexed unbounded (D42), so
	 * every state word driven here stays inside 0..86.
	 */
	v34hs_debug(1);

	suite_guards();
	suite_rtd();
	suite_bodies();
	suite_tail();
	suite_record();
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
			diff_eq_int(msg, sig[i] != sig[j], 1, 9000 + i * 100 + j);
		}

	diff_eq_int("behaviours recorded", nsig, 7, 0);

	v34hs_holes_check();
	return diff_end();
}

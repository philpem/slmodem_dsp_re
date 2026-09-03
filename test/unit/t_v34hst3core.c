/*
 * t_v34hst3core.c -- `v34handshak`'s microstate table, its core arms.
 *
 * THIS IS THE FIRST TEST THAT PUTS THIS TREE'S `v34handshak` ON SIDE A.
 * `test/harness/v34hsstep.c` compares two objects at different addresses
 * brought up by different code; `v34hs_ours(1)` makes side A the
 * reconstruction, so every case below is an ordinary tier-1 differential
 * test of our code against the blob's, over the whole 44,096-byte object,
 * the five blocks it points at, the padding around them, and both
 * transcripts.  t_v34hsstep.c keeps proving the FIXTURE, blob against blob,
 * and this proves the arms.
 *
 * WHAT IS COVERED, from .rodata+0x3000 (finding F286):
 *
 *     0x6590b   the arm twenty-four of the forty microstates share
 *     0x65329   the default, which is the same three instructions
 *     0x65c7a   62 RX_PHASE3_CALL, both sides of its one guard
 *     0x657ca   79 MOH_TONE
 *     0x656e0   80 MOH_TONE_DROP
 *
 * THE TXSTATE IS PART OF THE FIXTURE, NOT A DON'T-CARE (finding F288).  Every
 * one of these arms leaves through the once-per-block transmit dispatch at
 * 0x62af1, so a microstate case is a microstate arm AND a transmit arm.  The
 * cases below are driven at MOH_SILENCE unless they say otherwise: 81 is
 * above table 2's window, so it selects the dispatch's own default at
 * 0x62a40 and pulls in none of table 2's seven arms, which are #56's.  Two
 * of the arms here force a txstate of their own and cannot avoid it: 79's
 * body sets TX_DPSK and 80's retrain leaves SILENCERETRAIN, and both of those
 * select table 2's arm at 0x644c9.
 *
 * ON THE TWENTY-FOUR.  Driving all twenty-four states through the shared arm
 * is ONE behavioural check repeated twenty-four times: they take the same
 * three instructions and there is nothing to tell apart.  It is twenty-four
 * INDEPENDENT checks of a different claim -- which states reach that arm --
 * because our dispatch is a switch and a state dropped from it falls to a
 * `default` that halts.  Both readings are asserted below, separately.
 *
 * SWEPT.  Twelve object fills (`V34HS_SEED=1..12`), five object skews, four
 * arena placements, eight neighbourhoods, and side B's object outside its
 * arena: no differential failure at any of them, and the only measurement
 * that moves is the absolute byte count, which `step` therefore asserts at
 * the default fill alone and says why.  That sweep is findings F319-322's
 * standard of evidence, applied here to a reconstruction rather than to the
 * fixture.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "v34hsstep.h"
#include "dsplib/v34hshak.h"

/* Offsets the cases seed.  Prefixed: four batches share this fixture. */
#define T3T_PROGRESS	0x0004
#define T3T_LVL_LIMIT	0x0230
#define T3T_LVL_COUNT	0x0234
#define T3T_TIMER_LO	0x0238
#define T3T_TIMER_HI	0x023c
#define T3T_RECEIVER	0x0264
#define T3T_MODE	0x2218
#define T3T_DET		0x3564
#define T3T_FAA96	0xaa96
#define T3T_COUNT	0xaa78
#define T3T_COUNT_SRC	0xaa7c
#define T3T_RTD		0xaa7e
#define T3T_FAAE2	0xaae2
#define T3T_F359C	0x359c
#define T3T_FABE2	0xabe2
#define T3T_FABE4	0xabe4
#define T3T_FABE6	0xabe6
#define T3T_FABF0	0xabf0
#define T3T_FABF8	0xabf8
#define T3T_FABF9	0xabf9
#define T3T_FABFC	0xabfc

#define T3T_RX_FLAGS	(T3T_RECEIVER + 0x122)
#define T3T_RX_LEVEL	(T3T_RECEIVER + 0x134)
#define T3T_RX_F1D2	(T3T_RECEIVER + 0x1d2)
#define T3T_RX_SAMPBUF	0x010c	/* where the detector and the FSK read */

/* struct v34_detector, at object +0x3564.  Fields from v34det.h. */
#define T3T_DET_POL	(T3T_DET + 0x04)
#define T3T_DET_ARMED	(T3T_DET + 0x06)
#define T3T_DET_COUNT	(T3T_DET + 0x08)
#define T3T_DET_LIMIT	(T3T_DET + 0x0a)
#define T3T_DET_STATE	(T3T_DET + 0x0c)
#define T3T_DET_THI	(T3T_DET + 0x0e)
#define T3T_DET_TLO	(T3T_DET + 0x10)

static int dump;

/*
 * Whether the object fill is the default one.
 *
 * `V34HS_SEED=n` refills both objects, and one of the four measurements
 * below is a property of that fill rather than of the arm: `changed` counts
 * bytes that DIFFER from what the fill left, so a write of the same value
 * the fill happened to hold is not counted and a counter that crosses a byte
 * boundary is counted twice.  Swept over seeds 1..12, every one of these
 * cases moves by one or two bytes and nothing else moves at all -- not the
 * comparison, not a line count, not a state word, not a progress code.
 *
 * So that one assertion is made at the default fixture and the rest always.
 * The differential comparison itself is made at every seed and is never
 * relaxed: this is about what a signature MEANS, not about a tolerance.
 * Finding F290 made the same distinction for the same reason.
 */
static int default_fill;

/*
 * Open a case: both objects built and brought up, the rxstate chain routed
 * to table 3, the three state words written, and the once-per-block tail's
 * own inputs pinned.
 *
 * The tail at 0x62a40 reads +0x2218, two timer words and the receiver's AGC
 * level and writes a progress code from them, so a microstate case run on a
 * pseudorandom fill is a microstate case plus whatever the fill happened to
 * make the tail do.  Pinning them is what makes the arm the only variable;
 * `tail_paths` below then varies them deliberately and holds the arm fixed.
 */
static void
begin(short mst, short tx)
{
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(mst, V34HS_RX_DPSK, tx);

	v34hs_poke_int(T3T_MODE, 0);
	v34hs_poke_int(T3T_TIMER_LO, 1000);
	v34hs_poke_int(T3T_TIMER_HI, 2000);
	v34hs_poke_int(T3T_LVL_LIMIT, 0x7fffffff);
	v34hs_poke_int(T3T_LVL_COUNT, 0);
}

/*
 * Make the detector at +0x3564 assert, or refuse to.
 *
 * Not "seed it and see": `tone_detect` is reconstructed (src/pump/v34/
 * detector.c) and its answer is a function of five fields, so both outcomes
 * are set rather than inherited from the fill.  Refusing is done through the
 * warm-up counter, which returns before the level is consulted at all, so
 * neither answer depends on what the filter made of the input.
 */
static void
detector(int assert_it)
{
	/*
	 * GIVE THE FILTER A STATE THAT EVOLVES.  The range it runs over is
	 * not the test's to choose: `V34agc` runs earlier on this route and
	 * ends with `rx->rx_samples = rx + 0x10c + 4 * 2`, so the detector
	 * always sees exactly the four samples the queue just delivered and a
	 * poke at +0x130 is overwritten before it is read.
	 *
	 * With the histories and the coefficients left as the fill leaves
	 * them the filter settles to zero within two samples, and then three
	 * samples and four give the same answer -- so the START of the range
	 * was untested, and the mutation that moves it two bytes was the one
	 * this file did not catch.  Seeded histories and coefficients that
	 * are really coefficients make each sample change the state, and the
	 * count of them observable.
	 */
	v34hs_poke_self_ptr(T3T_DET + 0x00, 0x8000);	/* eight coefficients */
	v34hs_poke_short(T3T_DET + 0x12, 300);		/* level              */
	v34hs_poke_short(T3T_DET + 0x14, 11);		/* x[0][0]            */
	v34hs_poke_short(T3T_DET + 0x16, -22);
	v34hs_poke_short(T3T_DET + 0x18, 33);
	v34hs_poke_short(T3T_DET + 0x1a, -44);
	v34hs_poke_short(T3T_DET + 0x1c, 55);		/* y[0][0]            */
	v34hs_poke_short(T3T_DET + 0x1e, -66);
	v34hs_poke_short(T3T_DET + 0x20, 77);
	v34hs_poke_short(T3T_DET + 0x22, -88);

	v34hs_poke_short(T3T_DET_POL, 0);		/* presence */
	v34hs_poke_short(T3T_DET_ARMED, 1);		/* already armed */
	v34hs_poke_short(T3T_DET_THI, 0);		/* absence only */
	v34hs_poke_short(T3T_DET_TLO, (short)0x8000);

	if (assert_it) {
		v34hs_poke_short(T3T_DET_STATE, 2);	/* running */
		v34hs_poke_short(T3T_DET_COUNT, 0);
		v34hs_poke_short(T3T_DET_LIMIT, -1);
	} else {
		v34hs_poke_short(T3T_DET_STATE, 1);	/* warm-up */
		v34hs_poke_short(T3T_DET_COUNT, -1000);
		v34hs_poke_short(T3T_DET_LIMIT, 0x7fff);
	}
}

/*
 * The detector's `coeff` pointer on one side, read as a pointer.
 *
 * NOT `v34hs_peek_short`: the field is a pointer and half of one is not an
 * identity.  And not a COMPARISON BETWEEN THE SIDES either -- side A holds
 * ours and side B the blob's copy of the same table, which is exactly the
 * case finding F324 says no address comparison can settle.  What is checked
 * with this is which of OUR OWN two descriptors the arm chose, which is an
 * absolute answer and not a difference: two runs that merely differ would
 * pass with the select inverted.
 */
static const short *
det_coeff(int side)
{
	return *(const short *const *)((const char *)v34hs_object(side)
				       + T3T_DET);
}

/*
 * Step, compare, and say what the step DID.
 *
 * The comparison is the differential check; the four `diff_eq_int`s after it
 * are the anti-vacuity half.  A case that took a different branch of its arm
 * from the one it is named for still compares -- both sides took it -- so
 * without them a seed that stopped reaching the body would leave the test
 * green and testing nothing.  `changed` is bytes written, which pins which
 * branch ran; `lines` is diagnostic lines, which pins the traces.
 */
static void
step(const char *what, long tag, unsigned changed, unsigned lines,
     short mst, short txst)
{
	const struct v34hs_obs *o;

	v34hs_step();
	v34hs_compare(what, tag);

	o = v34hs_observed(0);
	if (dump)
		printf("  %-28s changed %3u  lines %u  mst %2d tx %2d "
		       "hash %08x\n", what, o->changed, o->lines,
		       o->mst, o->txst, o->hash);

	/*
	 * No conversion in these: `diff_eq_int` appends the input when the
	 * format has none (finding F220), and a `%s` in one would be handed a
	 * long -- which segfaults rather than reporting.
	 */
	if (default_fill)
		diff_eq_int("object bytes the step wrote", o->changed, changed,
			    tag);
	diff_eq_int("diagnostic lines printed", o->lines, lines, tag);
	diff_eq_int("microstate afterwards", o->mst, mst, tag);
	diff_eq_int("txstate afterwards", o->txst, txst, tag);
}

/* The twenty-four entries of .rodata+0x3000 that hold 0x6590b. */
static const short shared_arm[] = {
	V34HS_DET_CJ, V34HS_RX_DPSK, V34HS_TONE_AB_ANS, V34HS_TX_L2,
	V34HS_DET_AB, V34HS_SILENCEINFO, V34HS_TX_PHASE3_CALL, V34HS_TONE_AB,
	V34HS_TONE_AB_CALL, V34HS_JTXMIT, V34HS_XMIT0, V34HS_TRNSEG4A,
	V34HS_XMITMP, V34HS_J1TXMIT, V34HS_EXMIT, V34HS_DATAXMIT,
	V34HS_TXLEVEL, V34HS_RX_L1, V34HS_RX_L2, V34HS_SILENCERETRAIN,
	V34HS_RX_RETRAIN_CALL, V34HS_RX_RETRAIN_ANSWER, V34HS_TX_RETRAIN_ANS,
	V34HS_JaTXMIT
};
#define NSHARED	((int)(sizeof(shared_arm) / sizeof(shared_arm[0])))

/*
 * The six microstates outside 41..80 the brief asks about, plus the two
 * inside it that the dispatch cannot reach any other way.  0x64ac6 subtracts
 * 41 and rejects above 0x27 unsigned, so both ends and the wrap are worth a
 * case.
 */
static const short out_of_window[] = { 0, 1, 40, 81, 86, -1 };
#define NOUT	((int)(sizeof(out_of_window) / sizeof(out_of_window[0])))

/* The txstates that select the transmit dispatch's own default at 0x62a40. */
static const short default_tx[] = {
	V34HS_RX_RETRAIN_CALL, V34HS_RX_RETRAIN_ANSWER, V34HS_TX_RETRAIN_ANS,
	V34HS_MOH_SILENCE, V34HS_K56JaTXMIT, V34HS_TXMD
};
#define NDEFTX	((int)(sizeof(default_tx) / sizeof(default_tx[0])))

int
main(void)
{
	int i;
	unsigned base_hash = 0;
	unsigned base_changed = 0;

	dump = getenv("V34HS_DUMP") != NULL;
	default_fill = getenv("V34HS_SEED") == NULL;
	diff_begin("v34handshak: table 3's shared arm, 62, 79 and 80");

	/*
	 * OURS ON SIDE A.  Everything below this line is a comparison of this
	 * tree's `v34handshak` against the blob's, and the moment it is
	 * removed every case becomes blob against blob and passes for a
	 * reason that has nothing to do with the reconstruction.
	 */
	v34hs_ours(1);

	/*
	 * The diagnostics on, at level 2, which is where `DSPLIB_DEBUG_ON()`
	 * is true.  Two of these arms print, one of them through
	 * `hs_setstate`, and the transcripts are compared line for line --
	 * which is the only check this tree has on `StateName`, a local
	 * symbol with no `ref_` alias.
	 */
	v34hs_debug(1);

	/* --- the arm twenty-four states share, 0x6590b -------------------- */

	/*
	 * ONE BEHAVIOUR, TWENTY-FOUR ENTRIES.  What is independent here is
	 * the table transcription: a state left out of our switch falls to a
	 * default that halts, and a state wrongly added takes a body it
	 * should not.  What is NOT independent is the behaviour -- all
	 * twenty-four run the same three instructions, so the byte counts
	 * below are one measurement made twenty-four times, and they are
	 * asserted EQUAL to each other for that reason.
	 */
	for (i = 0; i < NSHARED; i++) {
		char what[64];

		snprintf(what, sizeof(what), "shared arm, microstate %d",
			 shared_arm[i]);
		begin(shared_arm[i], V34HS_MOH_SILENCE);
		v34hs_step();
		v34hs_compare(what, 100 + shared_arm[i]);

		if (i == 0) {
			base_hash = v34hs_observed(0)->hash;
			base_changed = v34hs_observed(0)->changed;
		} else {
			diff_eq_int("shared arm: same signature as 42",
				    v34hs_observed(0)->hash, base_hash,
				    shared_arm[i]);
			diff_eq_int("shared arm: same bytes as 42",
				    v34hs_observed(0)->changed, base_changed,
				    shared_arm[i]);
		}
	}

	/*
	 * The default at 0x65329 is the same three instructions at a
	 * different address, so a microstate outside 41..80 must land on the
	 * same signature.  That is the claim, and it is why these are here
	 * rather than in the loop above: the two arms are asserted to AGREE,
	 * which is a fact about the object and would be a failure if the
	 * default ever grew a body.
	 */
	for (i = 0; i < NOUT; i++) {
		char what[64];

		snprintf(what, sizeof(what), "default arm, microstate %d",
			 out_of_window[i]);
		begin(out_of_window[i], V34HS_MOH_SILENCE);
		v34hs_step();
		v34hs_compare(what, 200 + i);
		diff_eq_int("default arm: agrees with the shared arm",
			    v34hs_observed(0)->hash, base_hash,
			    out_of_window[i]);
	}

	/*
	 * THE TXSTATE AXIS, AND IT DOES NOT SEPARATE EITHER.  The shared arm's
	 * whole body is "read the transmit state and jump", so the transmit
	 * state is the only thing that can distinguish two runs of it -- and
	 * six txstates above table 2's window all select the same default at
	 * 0x62a40 and come back with the SAME SIGNATURE.  That is asserted
	 * rather than glossed: a collision the object's own structure
	 * produces is evidence, and asserting it makes a later change that
	 * separates them a failure rather than a silence (finding F290).
	 *
	 * So these six are one behavioural check repeated six times, exactly
	 * as the twenty-four above are.  The txstate that IS independent is
	 * MOH_CLEARDOWN, which the tail compares against by name; it is
	 * driven below and it does separate.
	 */
	for (i = 0; i < NDEFTX; i++) {
		char what[64];

		snprintf(what, sizeof(what), "shared arm at txstate %d",
			 default_tx[i]);
		begin(V34HS_DET_CJ, default_tx[i]);
		step(what, 300 + default_tx[i], 12, 0, V34HS_DET_CJ,
		     default_tx[i]);
		diff_eq_int("txstate does not separate in the default group",
			    v34hs_observed(0)->hash, base_hash, default_tx[i]);
	}

	/* --- the once-per-block tail at 0x62a40 --------------------------- */

	/*
	 * Five branches of the tail every arm above leaves through, driven
	 * one at a time with the microstate held on the shared arm.  These
	 * are independent of each other and of everything above: each turns
	 * on a different field and each writes a different progress code.
	 */

	/* +0x2218 == 1: the block at 0x62b45, which also writes the receiver. */
	begin(V34HS_DET_CJ, V34HS_MOH_SILENCE);
	v34hs_poke_int(T3T_MODE, 1);
	v34hs_poke_short(T3T_FAA96, 7);
	step("tail, mode 1", 400, 18, 0, V34HS_DET_CJ, V34HS_MOH_SILENCE);
	diff_eq_int("tail, mode 1: receiver +0x1d2 tripled",
		    v34hs_peek_short(0, T3T_RX_F1D2), 21, 400);
	diff_eq_int("tail, mode 1: progress",
		    v34hs_observed(0)->progress, 4, 400);

	/* +0x2218 in {4,5}: progress 6. */
	begin(V34HS_DET_CJ, V34HS_MOH_SILENCE);
	v34hs_poke_int(T3T_MODE, 5);
	step("tail, mode 5", 401, 16, 0, V34HS_DET_CJ, V34HS_MOH_SILENCE);
	diff_eq_int("tail, mode 5: progress",
		    v34hs_observed(0)->progress, 6, 401);

	/* The timer compare, which is UNSIGNED: progress 8. */
	begin(V34HS_DET_CJ, V34HS_MOH_SILENCE);
	v34hs_poke_int(T3T_TIMER_LO, 2000);
	v34hs_poke_int(T3T_TIMER_HI, 1000);
	step("tail, timer past", 402, 16, 0, V34HS_DET_CJ, V34HS_MOH_SILENCE);
	diff_eq_int("tail, timer past: progress",
		    v34hs_observed(0)->progress, 8, 402);

	/*
	 * And the compare is UNSIGNED, which is not decoration: a low word
	 * that has wrapped past the high one reads as enormous rather than as
	 * past, so the arm fires.  A signed compare gets this case backwards.
	 */
	begin(V34HS_DET_CJ, V34HS_MOH_SILENCE);
	v34hs_poke_int(T3T_TIMER_LO, -1);
	v34hs_poke_int(T3T_TIMER_HI, 1000);
	step("tail, timer wrapped", 406, 16, 0, V34HS_DET_CJ,
	     V34HS_MOH_SILENCE);
	diff_eq_int("tail, timer wrapped: progress",
		    v34hs_observed(0)->progress, 8, 406);

	/*
	 * The AGC level exactly at the limit: `jge`, so it resets rather than
	 * counting.  Without this the comparison could be `>` and nothing
	 * would notice.
	 */
	begin(V34HS_DET_CJ, V34HS_MOH_SILENCE);
	v34hs_poke_short(T3T_RX_LEVEL, 100);
	v34hs_poke_int(T3T_LVL_LIMIT, 100);
	v34hs_poke_int(T3T_LVL_COUNT, 55);
	step("tail, level exactly at the limit", 407, 12, 0, V34HS_DET_CJ,
	     V34HS_MOH_SILENCE);
	diff_eq_int("tail, level at the limit: counter reset",
		    v34hs_peek_short(0, T3T_LVL_COUNT), 0, 407);

	/*
	 * The level counter, both ways.  Below the limit it counts up; at or
	 * above it, it resets -- and the reset arm does NOT also increment,
	 * which is the half a reading of the branch gets backwards.
	 */
	begin(V34HS_DET_CJ, V34HS_MOH_SILENCE);
	v34hs_poke_int(T3T_LVL_LIMIT, -0x8000);
	v34hs_poke_int(T3T_LVL_COUNT, 1234);
	step("tail, level at limit", 403, 13, 0, V34HS_DET_CJ,
	     V34HS_MOH_SILENCE);
	diff_eq_int("tail, level at limit: counter reset",
		    v34hs_peek_short(0, T3T_LVL_COUNT), 0, 403);

	/* And the cap the counter trips: progress 9. */
	begin(V34HS_DET_CJ, V34HS_MOH_SILENCE);
	v34hs_poke_int(T3T_LVL_COUNT, 0x257f);
	step("tail, level count capped", 404, 16, 0, V34HS_DET_CJ,
	     V34HS_MOH_SILENCE);
	diff_eq_int("tail, level count capped: progress",
		    v34hs_observed(0)->progress, 9, 404);

	/* MOH_CLEARDOWN is compared against the state, not the table index. */
	begin(V34HS_DET_CJ, V34HS_MOH_CLEARDOWN);
	step("tail, txstate MOH_CLEARDOWN", 405, 16, 0, V34HS_DET_CJ,
	     V34HS_MOH_CLEARDOWN);
	diff_eq_int("tail, MOH_CLEARDOWN: progress",
		    v34hs_observed(0)->progress, 0x10, 405);
	/* The one txstate above the window that is NOT the six above. */
	diff_eq_int("MOH_CLEARDOWN separates from the default group",
		    v34hs_observed(0)->hash != base_hash, 1, 405);

	/* --- 62 RX_PHASE3_CALL, 0x65c7a ----------------------------------- */

	/*
	 * Bit 0 of +0xaae2 clear: the arm is the shared one, and it must give
	 * the shared arm's signature exactly.  Asserted against `base_hash`
	 * so that a body accidentally run on this path is a failure.
	 */
	begin(V34HS_RX_PHASE3_CALL, V34HS_MOH_SILENCE);
	v34hs_poke_byte(T3T_FAAE2, 0x54);
	v34hs_step();
	v34hs_compare("62, guard clear", 500);
	diff_eq_int("62, guard clear: takes the shared arm",
		    v34hs_observed(0)->hash, base_hash, 500);

	/*
	 * Bit 0 set: the body.  It copies +0xaa7c over +0xaa78, moves the
	 * microstate to TX_PHASE2_CALL and raises the receiver's pending
	 * flag, and it prints twice -- the transition through `hs_setstate`
	 * and its own line.
	 */
	begin(V34HS_RX_PHASE3_CALL, V34HS_MOH_SILENCE);
	v34hs_poke_byte(T3T_FAAE2, 0x55);
	v34hs_poke_short(T3T_COUNT_SRC, 0x1234);
	v34hs_poke_short(T3T_COUNT, 0);
	v34hs_poke_short(T3T_RX_FLAGS, 0x0011);
	step("62, guard set", 501, 16, 2, V34HS_TX_PHASE2_CALL,
	     V34HS_MOH_SILENCE);
	diff_eq_int("62: counter copied from +0xaa7c",
		    v34hs_peek_short(0, T3T_COUNT), 0x1234, 501);
	diff_eq_int("62: receiver's pending flag raised",
		    v34hs_peek_short(0, T3T_RX_FLAGS), 0x0211, 501);

	/*
	 * And with the diagnostics off, so that the transcript is the only
	 * difference: the same bytes, no lines.  That separates what the arm
	 * WRITES from what it PRINTS, and it is the check that would catch a
	 * trace placed inside the assignment it reports.
	 */
	v34hs_debug(0);
	begin(V34HS_RX_PHASE3_CALL, V34HS_MOH_SILENCE);
	v34hs_poke_byte(T3T_FAAE2, 0x55);
	v34hs_poke_short(T3T_COUNT_SRC, 0x1234);
	v34hs_poke_short(T3T_COUNT, 0);
	v34hs_poke_short(T3T_RX_FLAGS, 0x0011);
	step("62, guard set, quiet", 502, 16, 0, V34HS_TX_PHASE2_CALL,
	     V34HS_MOH_SILENCE);
	v34hs_debug(1);

	/* --- 79 MOH_TONE, 0x657ca ----------------------------------------- */

	/* The detector silent: the counter moves and nothing else does. */
	begin(V34HS_MOH_TONE, V34HS_MOH_SILENCE);
	detector(0);
	v34hs_poke_short(T3T_COUNT, 100);
	step("79, detector silent", 600, 27, 0, V34HS_MOH_TONE,
	     V34HS_MOH_SILENCE);
	diff_eq_int("79, detector silent: counter stepped",
		    v34hs_peek_short(0, T3T_COUNT), 101, 600);

	/*
	 * The detector asserting but the counter short of +0xabfc: a
	 * DIFFERENT exit, at 0x6c701, with the same outcome.  Driven because
	 * the two paths are indistinguishable in the object and a
	 * reconstruction that merged them would pass everything else.
	 */
	begin(V34HS_MOH_TONE, V34HS_MOH_SILENCE);
	detector(1);
	v34hs_poke_short(T3T_COUNT, 100);
	v34hs_poke_short(T3T_FABFC, 4000);
	step("79, counter short", 601, 27, 0, V34HS_MOH_TONE,
	     V34HS_MOH_SILENCE);
	diff_eq_int("79, counter short: counter stepped",
		    v34hs_peek_short(0, T3T_COUNT), 101, 601);

	/*
	 * THE BODY.  It forces the transmit state to TX_DPSK, so this case
	 * cannot avoid table 2's arm at 0x644c9; that arm is three
	 * instructions and is written in src/pump/v34/v34hshak.c with a note
	 * saying it belongs to #56.  Nine fields are written, two of them
	 * pointers back into the object, and the microstate goes to DET_SYNC.
	 */
	begin(V34HS_MOH_TONE, V34HS_MOH_SILENCE);
	detector(1);
	v34hs_poke_short(T3T_COUNT, 100);
	v34hs_poke_short(T3T_FABFC, 50);
	v34hs_poke_int(T3T_FABF0, 0);
	step("79, body", 602, 37, 2, V34HS_DET_SYNC, V34HS_TX_DPSK);
	diff_eq_int("79, body: counter cleared",
		    v34hs_peek_short(0, T3T_COUNT), 0, 602);

	/*
	 * THE BOUNDARY, both sides of it.  The counter is compared against
	 * +0xabfc AFTER it has been stepped, and the comparison is `jl`: equal
	 * goes on into the body.  Without these two a `<=` reads the same.
	 */
	begin(V34HS_MOH_TONE, V34HS_MOH_SILENCE);
	detector(1);
	v34hs_poke_short(T3T_COUNT, 100);
	v34hs_poke_short(T3T_FABFC, 101);
	v34hs_poke_int(T3T_FABF0, 0);
	step("79, counter exactly at +0xabfc", 603, 37, 2, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);

	begin(V34HS_MOH_TONE, V34HS_MOH_SILENCE);
	detector(1);
	v34hs_poke_short(T3T_COUNT, 100);
	v34hs_poke_short(T3T_FABFC, 102);
	step("79, counter one short of +0xabfc", 604, 27, 0, V34HS_MOH_TONE,
	     V34HS_MOH_SILENCE);

	/*
	 * 0x6d57c, THE OTHER SIDE OF 79's ONE 32-BIT GUARD.  With
	 * `moh_message` at 1 MHfrr the body does not go to DET_SYNC: it
	 * re-aims the tone detector at +0x3564 and goes to MOH_TONE_DROP to
	 * wait for the far end's carrier to stop.  Everything below the guard
	 * -- the FSK reset, the two self-pointers, the counter -- is shared,
	 * and 0x6d57c's exit at 0x6589f joins it past the DET_SYNC store.
	 *
	 * THE DETECTOR IS ASSERTED FIELD BY FIELD AND NOT LEFT TO THE BYTE
	 * COMPARISON.  `limit` is the ONE constant that separates this call
	 * from microstate 44's at 0x6ebed -- 0xf0 here, 0x64 there, same
	 * coefficients, same polarity, same warm-up, same thresholds -- so a
	 * reconstruction that copied 44's line differs in two bytes of one
	 * halfword and in nothing else.  `count` is -`warmup`, which is what
	 * detectorinit does with it.
	 *
	 * AND THE CARRIER SELECT IS CHECKED BY DIFFERENCE, not by address.
	 * `coeff` points at a library table, so side A holds ours and side B
	 * the blob's and no address comparison can tell two copies from two
	 * tables (finding F324).  What CAN be checked on one side is that
	 * role 0x64 and role 0x65 select different tables at all -- which a
	 * reconstruction that always picked one would fail.
	 */
	begin(V34HS_MOH_TONE, V34HS_MOH_SILENCE);
	detector(1);
	v34hs_poke_short(T3T_COUNT, 100);
	v34hs_poke_short(T3T_FABFC, 50);
	v34hs_poke_int(T3T_FABF0, 1);
	v34hs_poke_short(T3T_F359C, 0x64);
	step("79, MHfrr, answering", 605, 45, 2, V34HS_MOH_TONE_DROP,
	     V34HS_TX_DPSK);
	diff_eq_int("79, MHfrr: the detector's limit is 0xf0 and not 0x64",
		    v34hs_peek_short(0, T3T_DET_LIMIT), 0xf0, 605);
	diff_eq_int("79, MHfrr: polarity 1, so the high threshold is read",
		    v34hs_peek_short(0, T3T_DET_POL), 1, 605);
	diff_eq_int("79, MHfrr: the warm-up is 0x32 calls",
		    v34hs_peek_short(0, T3T_DET_COUNT), -0x32, 605);
	diff_eq_int("79, MHfrr: thresholds 0x800 low and 0x400 high",
		    (v34hs_peek_short(0, T3T_DET_TLO) == 0x800
		     && v34hs_peek_short(0, T3T_DET_THI) == 0x400), 1, 605);
	diff_eq_int("79, MHfrr: the detector is armed at +0x356a",
		    v34hs_peek_short(0, T3T_DET_ARMED), 1, 605);
	diff_eq_int("79, MHfrr: role 0x64 selects the 1200 Hz descriptor",
		    det_coeff(0) == c1200_, 1, 605);

	begin(V34HS_MOH_TONE, V34HS_MOH_SILENCE);
	detector(1);
	v34hs_poke_short(T3T_COUNT, 100);
	v34hs_poke_short(T3T_FABFC, 50);
	v34hs_poke_int(T3T_FABF0, 1);
	v34hs_poke_short(T3T_F359C, 0x65);
	step("79, MHfrr, originating", 606, 45, 2, V34HS_MOH_TONE_DROP,
	     V34HS_TX_DPSK);
	diff_eq_int("79, MHfrr: role 0x65 selects the 2400 Hz descriptor",
		    det_coeff(0) == c2400_, 1, 606);

	/*
	 * AND THE GUARD IS `== 1` AND THIRTY-TWO BITS WIDE.  `moh_message` at
	 * 2 MHclrd takes the DET_SYNC side, which a test for non-zero would
	 * not; at 0x10001 it does too, which a sixteen-bit read would not.
	 * Both go to DET_SYNC, so they are 79's body case again with one
	 * field moved -- which is the point: the field is what is on trial.
	 */
	begin(V34HS_MOH_TONE, V34HS_MOH_SILENCE);
	detector(1);
	v34hs_poke_short(T3T_COUNT, 100);
	v34hs_poke_short(T3T_FABFC, 50);
	v34hs_poke_int(T3T_FABF0, 2);
	step("79, moh_message 2 is not MHfrr", 607, 37, 2, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);

	begin(V34HS_MOH_TONE, V34HS_MOH_SILENCE);
	detector(1);
	v34hs_poke_short(T3T_COUNT, 100);
	v34hs_poke_short(T3T_FABFC, 50);
	v34hs_poke_int(T3T_FABF0, 0x10001);
	step("79, moh_message 0x10001 is not MHfrr", 608, 37, 2,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);

	/* --- 80 MOH_TONE_DROP, 0x656e0 ------------------------------------ */

	/* Detector silent: the counter, and the drop is not reported. */
	begin(V34HS_MOH_TONE_DROP, V34HS_MOH_SILENCE);
	detector(0);
	v34hs_poke_short(T3T_COUNT, 100);
	v34hs_poke_short(T3T_RTD, 0);
	v34hs_poke_byte(T3T_FABF8, 0);
	step("80, detector silent", 700, 27, 0, V34HS_MOH_TONE_DROP,
	     V34HS_MOH_SILENCE);
	diff_eq_int("80, detector silent: drop not reported",
		    v34hs_peek_short(0, T3T_FABF8) & 0xff, 0, 700);

	/*
	 * Detector asserting with the drop unreported: one byte and one line.
	 * This is where 79 and 80 stop being the same case -- 79 tests the
	 * counter against +0xabfc here and 80 raises a flag.
	 */
	begin(V34HS_MOH_TONE_DROP, V34HS_MOH_SILENCE);
	detector(1);
	v34hs_poke_short(T3T_COUNT, 100);
	v34hs_poke_short(T3T_RTD, 0);
	v34hs_poke_byte(T3T_FABF8, 0);
	step("80, drop reported", 701, 28, 1, V34HS_MOH_TONE_DROP,
	     V34HS_MOH_SILENCE);
	diff_eq_int("80, drop reported: flag raised",
		    v34hs_peek_short(0, T3T_FABF8) & 0xff, 1, 701);

	/* And reported once: with the flag already up, nothing is printed. */
	begin(V34HS_MOH_TONE_DROP, V34HS_MOH_SILENCE);
	detector(1);
	v34hs_poke_short(T3T_COUNT, 100);
	v34hs_poke_short(T3T_RTD, 0);
	v34hs_poke_byte(T3T_FABF8, 1);
	step("80, drop already reported", 702, 27, 0, V34HS_MOH_TONE_DROP,
	     V34HS_MOH_SILENCE);

	/*
	 * THE THRESHOLD, both sides of it.  The counter is compared against
	 * (rtd >> 2) + 0x12c0 with the round-trip delay signed, and the
	 * counter has already been stepped when the comparison is made.
	 */
	begin(V34HS_MOH_TONE_DROP, V34HS_MOH_SILENCE);
	detector(0);
	v34hs_poke_short(T3T_RTD, 0);
	v34hs_poke_short(T3T_COUNT, 0x12be);
	step("80, one short of the threshold", 703, 27, 0,
	     V34HS_MOH_TONE_DROP, V34HS_MOH_SILENCE);

	/*
	 * At the threshold, with +0xabf9 clear: the retrain.  It calls
	 * `v34handshakinit` mode 1, which rewrites a large part of the object
	 * and leaves the transmit state at SILENCERETRAIN -- so this case,
	 * like 79's body, ends in table 2's arm at 0x644c9.
	 */
	begin(V34HS_MOH_TONE_DROP, V34HS_MOH_SILENCE);
	detector(0);
	v34hs_poke_short(T3T_RTD, 0);
	v34hs_poke_short(T3T_COUNT, 0x12bf);
	v34hs_poke_byte(T3T_FABF9, 0);
	/*
	 * The microstate is NOT moved: `v34handshakinit` mode 1 writes the
	 * transmit state and leaves +0x3592 where it found it, so 80 comes
	 * back out of the retrain still in MOH_TONE_DROP.  Nine diagnostic
	 * lines, one of them this arm's and eight the bring-up's.
	 */
	step("80, retrain", 704, 50, 9, V34HS_MOH_TONE_DROP,
	     V34HS_SILENCERETRAIN);
	diff_eq_int("80, retrain: +0xabe6 set",
		    v34hs_peek_short(0, T3T_FABE6), 1, 704);

	/*
	 * 0x6c8f8, THE OTHER SIDE OF THAT BYTE.  +0xabf9 non-zero means the
	 * far end never sent its MH sequence under MHfrr and the connection is
	 * given up rather than retrained: no `v34handshakinit`, four stores
	 * and one line.  The two paths are not symmetrical with 79's -- there
	 * the guard is a 32-bit field and here it is a BYTE -- so the read
	 * width is its own claim and gets its own trial below.
	 *
	 * THE FOUR STORES ARE SEEDED AWAY FIRST.  Two of them are state words
	 * and `hs_setstate` does nothing when the word already holds the
	 * value, so entering at MOH_CLEARDOWN or WAIT would make the stores
	 * invisible and cost the diagnostic line as well; +0xabe4 and +0xabe2
	 * are seeded non-one for finding F345's reason.
	 *
	 * AND THE BYTE IS SEEDED WITH ITS NEIGHBOUR CLEAR.  +0xabf9 is set to
	 * 1 and +0xabfa -- a declared field, so a real neighbour and not a pad
	 * -- to zero, and then to 0x100 with +0xabf9 clear.  A sixteen-bit
	 * read at +0xabf9 sees the neighbour, so the second seed is the one
	 * that separates `cmpb` from `cmpw`; a run with only the first cannot.
	 */
	begin(V34HS_MOH_TONE_DROP, V34HS_MOH_SILENCE);
	detector(0);
	v34hs_poke_short(T3T_RTD, 0);
	v34hs_poke_short(T3T_COUNT, 0x12bf);
	v34hs_poke_byte(T3T_FABF9, 1);
	v34hs_poke_byte(T3T_FABF9 + 1, 0);
	v34hs_poke_short(T3T_FABE4, 0x1234);
	v34hs_poke_short(T3T_FABE2, 0x5678);
	step("80, disconnect", 706, 37, 3, V34HS_MOH_TONE_DROP,
	     V34HS_MOH_CLEARDOWN);
	diff_eq_int("80, disconnect: +0xabe4 set",
		    v34hs_peek_short(0, T3T_FABE4), 1, 706);
	diff_eq_int("80, disconnect: +0xabe2 set",
		    v34hs_peek_short(0, T3T_FABE2), 1, 706);
	diff_eq_int("80, disconnect: the receive machine is at WAIT",
		    v34hs_peek_short(0, V34HS_RXSTATE_OFF), V34HS_WAIT, 706);
	diff_eq_int("80, disconnect: +0xabe6 is NOT the field it writes",
		    v34hs_peek_short(0, T3T_FABE6) != 1, 1, 706);

	/*
	 * The neighbour non-zero and the byte clear: this must RETRAIN.  A
	 * sixteen-bit read of +0xabf9 would see 0x100 and disconnect.
	 */
	begin(V34HS_MOH_TONE_DROP, V34HS_MOH_SILENCE);
	detector(0);
	v34hs_poke_short(T3T_RTD, 0);
	v34hs_poke_short(T3T_COUNT, 0x12bf);
	v34hs_poke_byte(T3T_FABF9, 0);
	v34hs_poke_byte(T3T_FABF9 + 1, 1);
	step("80, the byte clear and its neighbour set: retrain", 707, 50, 9,
	     V34HS_MOH_TONE_DROP, V34HS_SILENCERETRAIN);
	diff_eq_int("80, +0xabf9 is read as a byte: +0xabe6 set",
		    v34hs_peek_short(0, T3T_FABE6), 1, 707);

	/*
	 * THE ROUND-TRIP DELAY MOVES THE THRESHOLD, and it moves it by
	 * rtd >> 2.  Three cases pin the shift and its sign: at rtd 400 the
	 * threshold is 4900, so 4899 stops and 4900 goes on -- a shift of one
	 * or three puts the boundary somewhere else and one of the two fails
	 * -- and at rtd -400 it is 4700, which an unsigned shift could not
	 * produce.
	 */
	begin(V34HS_MOH_TONE_DROP, V34HS_MOH_SILENCE);
	detector(0);
	v34hs_poke_short(T3T_RTD, 400);
	v34hs_poke_short(T3T_COUNT, 4898);
	step("80, rtd 400, one short", 705, 27, 0, V34HS_MOH_TONE_DROP,
	     V34HS_MOH_SILENCE);

	begin(V34HS_MOH_TONE_DROP, V34HS_MOH_SILENCE);
	detector(0);
	v34hs_poke_short(T3T_RTD, 400);
	v34hs_poke_short(T3T_COUNT, 4899);
	v34hs_poke_byte(T3T_FABF9, 0);
	step("80, rtd 400, at the threshold", 706, 50, 9,
	     V34HS_MOH_TONE_DROP, V34HS_SILENCERETRAIN);

	begin(V34HS_MOH_TONE_DROP, V34HS_MOH_SILENCE);
	detector(0);
	v34hs_poke_short(T3T_RTD, -400);
	v34hs_poke_short(T3T_COUNT, 4699);
	v34hs_poke_byte(T3T_FABF9, 0);
	step("80, rtd -400, at the threshold", 707, 50, 9,
	     V34HS_MOH_TONE_DROP, V34HS_SILENCERETRAIN);

	/*
	 * THE OTHER ROUTE INTO THE SAME TAIL.  With the receiver's first
	 * halfword at 5 or below the prologue never reaches the rxstate chain
	 * at all and goes straight to the once-per-block transmit dispatch,
	 * so the same code is reached by a different guard.  Driven at
	 * MOH_SILENCE for the same reason as everything above, and with the
	 * microstate outside 41..80 so that it cannot contribute.
	 */
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_TXBLOCK, 0);
	v34hs_state(V34HS_PHASE1, V34HS_SILENCE, V34HS_MOH_SILENCE);
	v34hs_poke_int(T3T_MODE, 0);
	v34hs_poke_int(T3T_TIMER_LO, 1000);
	v34hs_poke_int(T3T_TIMER_HI, 2000);
	v34hs_poke_int(T3T_LVL_LIMIT, 0x7fffffff);
	v34hs_poke_int(T3T_LVL_COUNT, 0);
	/*
	 * One byte, where the microstate route writes twelve: the other
	 * eleven are `V34agc` and `fskdemodulate`, which this route does not
	 * reach.  That difference is the check that the guard chose.
	 */
	step("txblock route, MOH_SILENCE", 800, 1, 0, V34HS_PHASE1,
	     V34HS_MOH_SILENCE);

	/*
	 * THE POINTER HOLES.  `v34hs_compare` skips thirty-five pointer
	 * fields and compares each by offset from its own base; this asserts
	 * every one of them was reached, so the skip list cannot go stale
	 * while these cases run.  Finding F290.
	 */
	v34hs_holes_check();

	return diff_end();
}

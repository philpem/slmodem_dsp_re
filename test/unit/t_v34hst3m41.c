/*
 * t_v34hst3m41.c -- `v34handshak`'s microstate 41 `DET_SYNC`, 0x669a4.
 *
 * The third largest arm of .rodata+0x3000 (3,945 exclusive bytes, finding
 * 286) driven case by case against the blob through
 * test/harness/v34hsstep.c, with `v34hs_ours(1)` putting this tree's
 * `v34handshak` on side A -- so every case here is an ordinary tier-1
 * differential comparison of the whole 44,096-byte object, the five blocks
 * it points at, the padding around them and both transcripts.
 *
 * WHY THIS STATE NEEDED A FILE OF ITS OWN.  `docs/v34handshak.md` uses 41 as
 * its worked example of a case a seed does not reach: entered cold it is
 * indistinguishable from 44 and writes twenty-three bytes, because its first
 * guard sends it straight out.  Three companion fields decide which of six
 * bodies runs -- +0xaae2, +0xabe8 and +0x358a -- and every one of them is
 * written by the cases below rather than inherited from the fill.
 *
 * THE TXSTATE IS PART OF THE FIXTURE (finding 288).  Every path leaves
 * through the once-per-block transmit dispatch.  The cases run at
 * MOH_SILENCE (81), which is above table 2's window and selects the
 * dispatch's own default at 0x62a40, except where the arm forces its own:
 * three paths set TX_DPSK (24), one SILENCERETRAIN (74) and one TONE_AB
 * (60), and all three of those select table 2's arm at 0x644c9, which
 * finding 354 put in the tree.
 *
 * WHAT IS NOT DRIVEN, and so is not claimed: nothing.  Every leaf of the arm
 * has a case below; the arm contains no `t3c_unwritten` and no path that
 * halts.  What the arm CANNOT reach from microstate 41 is the `%si == 44`
 * early-out at 0x6c862 and the `%si == 58` one at 0x6e044, both of which are
 * `hs_setstate`'s own "already there" guard on a cached copy of +0x3592;
 * they are not separate branches and are not written as any.
 *
 * BOTH SIDES OF EVERY DEBUG GUARD.  The cases run with the diagnostics on,
 * which leaves the other half of each `if (DSPLIB_DEBUG_ON())` undriven and
 * a store moved inside one of them undetectable -- measured, not reasoned:
 * that mutation was applied by hand and this file passed.  So the last block
 * of `main` re-drives every body that prints with `v34hs_debug(0)` and
 * asserts the same bytes and no lines.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "v34hsstep.h"
#include "dsplib/v34hshak.h"

/* Offsets these cases seed.  `M41_`, because three batches share this file. */
#define M41_PROGRESS	0x0004
#define M41_LVL_LIMIT	0x0230
#define M41_LVL_COUNT	0x0234
#define M41_TIMER_LO	0x0238
#define M41_TIMER_HI	0x023c
#define M41_V90RX	0x024c
#define M41_RECEIVER	0x0264
#define M41_MODE	0x2218
#define M41_TRACE_1	0x2aa2
#define M41_DET		0x3564
#define M41_F3588	0x3588
#define M41_F358A	0x358a
#define M41_F358C	0x358c
#define M41_F359C	0x359c
#define M41_F35A0	0x35a0
#define M41_FA24A	0xa24a
#define M41_BLK_A94C	0xa94c
#define M41_BLK_A9AC	0xa9ac
#define M41_PTR_AA6C	0xaa6c
#define M41_PTR_AA70	0xaa70
#define M41_COUNT	0xaa78
#define M41_FAA7A	0xaa7a
#define M41_RTD		0xaa7e
#define M41_FAAE0	0xaae0
#define M41_FAAE2	0xaae2
#define M41_ABAE	0xabae
#define M41_FABC2	0xabc2
#define M41_FABCA	0xabca
#define M41_FABCC	0xabcc
#define M41_FABE8	0xabe8
#define M41_FABF0	0xabf0
#define M41_FABF8	0xabf8

#define M41_RX_FLAGS	(M41_RECEIVER + 0x122)

/* struct v34_detector at +0x3564, fields from v34det.h. */
#define M41_DET_POL	(M41_DET + 0x04)
#define M41_DET_ARMED	(M41_DET + 0x06)
#define M41_DET_COUNT	(M41_DET + 0x08)
#define M41_DET_LIMIT	(M41_DET + 0x0a)
#define M41_DET_STATE	(M41_DET + 0x0c)
#define M41_DET_THI	(M41_DET + 0x0e)
#define M41_DET_TLO	(M41_DET + 0x10)

/*
 * Where the two records this arm reaches through are put.
 *
 * Both are interior pointers, so the harness compares them by offset from
 * each side's own base and `v34hs_poke_self_ptr` aims each side at its OWN
 * object -- one address written into both is precisely the asymmetry
 * findings 319-322 are about.  The offsets are away from anything this route
 * writes, which is finding 357's caution: a pointer aimed into a region the
 * step itself modifies is a fixture fault presenting as a reconstruction
 * fault.
 */
#define M41_REC_AA70	0x8100
#define M41_REC_AA6C	0x8200
#define M41_DET_COEFF	0x8000

static int dump;

/*
 * `changed` counts bytes DIFFERING from what the fill left, so it is a
 * property of the fill as well as of the arm; finding 359 measured that it
 * moves by one or two bytes across seeds while nothing else moves at all.
 * Asserted at the default fixture only, everything else always.
 */
static int default_fill;

/*
 * Open a case.
 *
 * The tail at 0x62a40 overwrites +0x0004 on four conditions no arm reads,
 * and at two of twenty-four fills it does so on every case (finding 364).
 * Pinning its five inputs is what makes each body's answer a property of the
 * object rather than of the seed.
 *
 * +0xaa7a is seeded NON-ZERO on every case because the arm's one
 * unconditional store puts a zero there: a field that already holds what the
 * store writes makes the store invisible and its mutation equivalent, which
 * is finding 345's failure mode.
 */
static void
begin(short tx)
{
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(V34HS_DET_SYNC, V34HS_RX_DPSK, tx);

	v34hs_poke_int(M41_MODE, 0);
	v34hs_poke_int(M41_TIMER_LO, 1000);
	v34hs_poke_int(M41_TIMER_HI, 2000);
	v34hs_poke_int(M41_LVL_LIMIT, 0x7fffffff);
	v34hs_poke_int(M41_LVL_COUNT, 0);

	v34hs_poke_short(M41_FAA7A, 0x5a5a);
	/*
	 * The trace's `[1]`, seeded non-zero for the same reason as +0xaa7a:
	 * two of the six bodies clear it, and the bring-up leaves it at zero,
	 * so the store would be invisible and its mutation equivalent.
	 */
	v34hs_poke_short(M41_TRACE_1, 0x33);

	/* The arm's four inputs, so that no case inherits one from the fill. */
	v34hs_poke_short(M41_FAAE2, 0x1234);	/* low byte is not 0x72 */
	v34hs_poke_byte(M41_FABE8, 0);
	v34hs_poke_short(M41_F358A, 0);
	v34hs_poke_short(M41_FAAE0, 50);

	v34hs_poke_self_ptr(M41_PTR_AA70, M41_REC_AA70);
	v34hs_poke_self_ptr(M41_PTR_AA6C, M41_REC_AA6C);
}

/*
 * Make the detector assert, or refuse to.
 *
 * Refusing is done through the warm-up counter, which returns before the
 * level is consulted, so neither answer depends on what the filter made of
 * the input.  The histories and coefficients are seeded for finding 357's
 * reason: left as the fill leaves them the filter settles to zero within two
 * samples and the number of samples read stops being observable.
 */
static void
detector(int assert_it)
{
	v34hs_poke_self_ptr(M41_DET + 0x00, M41_DET_COEFF);
	v34hs_poke_short(M41_DET + 0x12, 300);		/* level   */
	v34hs_poke_short(M41_DET + 0x14, 11);		/* x[0][0] */
	v34hs_poke_short(M41_DET + 0x16, -22);
	v34hs_poke_short(M41_DET + 0x18, 33);
	v34hs_poke_short(M41_DET + 0x1a, -44);
	v34hs_poke_short(M41_DET + 0x1c, 55);		/* y[0][0] */
	v34hs_poke_short(M41_DET + 0x1e, -66);
	v34hs_poke_short(M41_DET + 0x20, 77);
	v34hs_poke_short(M41_DET + 0x22, -88);

	v34hs_poke_short(M41_DET_POL, 0);		/* presence */
	v34hs_poke_short(M41_DET_ARMED, 1);
	v34hs_poke_short(M41_DET_THI, 0);
	v34hs_poke_short(M41_DET_TLO, (short)0x8000);

	if (assert_it) {
		v34hs_poke_short(M41_DET_STATE, 2);
		v34hs_poke_short(M41_DET_COUNT, 0);
		v34hs_poke_short(M41_DET_LIMIT, -1);
	} else {
		v34hs_poke_short(M41_DET_STATE, 1);
		v34hs_poke_short(M41_DET_COUNT, -1000);
		v34hs_poke_short(M41_DET_LIMIT, 0x7fff);
	}
}

#define NOCHECK	((unsigned)-1)

static unsigned last_hash;

/*
 * Step, compare, and say what the step DID.
 *
 * The comparison is the differential check; the assertions after it are the
 * anti-vacuity half.  A case that fell out of its arm one guard earlier than
 * it is named for still compares -- both sides fell out -- so without them a
 * seed that stopped reaching a body would leave this file green and testing
 * nothing.
 */
static void
step(const char *what, long tag, unsigned changed, unsigned lines,
     short mst, short txst)
{
	const struct v34hs_obs *o;

	v34hs_step();
	v34hs_compare(what, tag);

	o = v34hs_observed(0);
	last_hash = o->hash;
	if (dump)
		printf("  %-34s changed %3u  lines %u  mst %2d rx %2d tx %2d "
		       "hash %08x\n", what, o->changed, o->lines,
		       o->mst, o->rxst, o->txst, o->hash);

	/* No conversion in these: `diff_eq_int` appends the input itself. */
	if (default_fill && changed != NOCHECK)
		diff_eq_int("object bytes the step wrote", o->changed, changed,
			    tag);
	if (lines != NOCHECK)
		diff_eq_int("diagnostic lines printed", o->lines, lines, tag);
	diff_eq_int("microstate afterwards", o->mst, mst, tag);
	diff_eq_int("txstate afterwards", o->txst, txst, tag);
}

/*
 * The distinct behaviours this file claims, so that a change collapsing two
 * of them is a failure rather than a silence (finding 290).
 */
#define NSIG	40
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

int
main(void)
{
	int i, j;
	unsigned h_cold, h_wu11, h_step0, h_reset;
	unsigned h_detinfo, h_search, h_frr, h_tone1, h_infodone;
	unsigned h_info1a, h_retrain, h_tx1ans, h_rx1call;

	dump = getenv("V34HS_DUMP") != NULL;
	default_fill = getenv("V34HS_SEED") == NULL;
	diff_begin("v34handshak: microstate 41 DET_SYNC, 0x669a4");

	/*
	 * OURS ON SIDE A.  Remove this and every case below becomes blob
	 * against blob and passes for a reason that has nothing to do with
	 * the reconstruction.
	 */
	v34hs_ours(1);

	/*
	 * The diagnostics on.  Six of this arm's paths print, four of them
	 * through `hs_setstate`, and the transcripts are compared line for
	 * line -- which is the tree's only check on `StateName` (a local
	 * symbol with no `ref_` alias) and on the seven literal messages.
	 */
	v34hs_debug(1);

	/* --- the entry guards, cold ------------------------------------- */

	/*
	 * Nothing set: the arm stores one halfword and leaves.  This is the
	 * behaviour docs/v34handshak.md records as indistinguishable from 44,
	 * and it is the baseline every case below has to differ from.
	 */
	begin(V34HS_MOH_SILENCE);
	step("cold: all four guards false", 100, 14, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	record("cold");
	h_cold = last_hash;

	/*
	 * The +0xaae0 guard is `cmpw $0x64; jg`, so 100 stays and 101 goes.
	 * Both sides of it, and the boundary is the point of the pair.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_FAAE0, 100);
	step("aae0 == 100, the boundary", 101, 14, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);

	/*
	 * The block count is SIGNED at this guard: 0xffff is -1 and stays,
	 * where an unsigned read would make it 65,535 and go.  Nothing else
	 * in the fixture distinguishes the two spellings.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_FAAE0, -1);
	step("aae0 -1 is below the floor", 102, 14, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	diff_eq_int("aae0 -1 == the cold path", last_hash, h_cold, 102);

	/* --- 0x709e1, the tone search ----------------------------------- */

	/*
	 * Past 100 blocks the detector is consulted, and a silent one before
	 * block 900 leaves -- but the detector-pending flag has already been
	 * lowered, which is what separates this from the cold case.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_FAAE0, 101);
	v34hs_poke_short(M41_RX_FLAGS, 0x0200);
	detector(0);
	step("tone search: silent, early", 110, 29, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	record("search silent early");

	/* 0x384 is `jle`, so 900 leaves and 901 goes on. */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_FAAE0, 0x384);
	v34hs_poke_short(M41_RX_FLAGS, 0x0200);
	detector(0);
	step("tone search: silent, aae0 900", 111, 29, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_FAAE0, 0x385);
	v34hs_poke_short(M41_RX_FLAGS, 0x0200);
	v34hs_poke_short(M41_F359C, 0);
	detector(0);
	step("tone search: silent, aae0 901", 112, 92, 2,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);
	record("search body, warmup 0x11");

	/*
	 * The detector asserting reaches the same body from the other side of
	 * the `||`, at a block count the silent case would have left at.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_FAAE0, 101);
	v34hs_poke_short(M41_F359C, 0);
	detector(1);
	step("tone search: detected, early", 113, 91, 2,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);
	h_search = last_hash;
	h_wu11 = last_hash;

	/*
	 * The longer warm-up needs BOTH the marker and the V.90 receiver, and
	 * the object reads the second through `obj + 4`, so it is +0x24c.
	 * Three cases, because two of them are the two ways to miss it.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_FAAE0, 101);
	v34hs_poke_short(M41_F359C, 0x65);
	v34hs_poke_int(M41_V90RX, 7);
	detector(1);
	step("tone search: 359c 0x65, v90 set", 114, 91, 2,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);
	record("search body, warmup 0x1e");

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_FAAE0, 101);
	v34hs_poke_short(M41_F359C, 0x65);
	v34hs_poke_int(M41_V90RX, 0);
	detector(1);
	step("tone search: 359c 0x65, v90 clear", 115, 91, 2,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);
	/*
	 * THE MARKER ALONE IS NOT ENOUGH.  0x70d35 tests +0x359c and THEN
	 * +0x24c, so the two ways to miss the long warm-up must agree with
	 * each other and differ from the case that takes it.  Asserted both
	 * ways round, which is what makes the `&&` a check rather than a
	 * reading.
	 */
	diff_eq_int("359c 0x65 with no V.90 receiver == no marker",
		    last_hash, h_wu11, 115);

	/*
	 * Driven at TX_DPSK, the transmit transition is the one `hs_setstate`
	 * declines to make, so this run prints one line fewer and takes table
	 * 2's arm at 0x644c9 by the door the others reach through a write.
	 */
	begin(V34HS_TX_DPSK);
	v34hs_poke_short(M41_FAAE0, 101);
	v34hs_poke_short(M41_F359C, 0);
	detector(1);
	step("tone search: already TX_DPSK", 116, 90, 1,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);
	record("search body, tx already 24");

	/* --- 0x6c862, the low byte of +0xaae2 ---------------------------- */

	/*
	 * The entry test is on the BYTE, so a halfword whose top half is
	 * anything at all takes it.  0x1172 and 0x0072 must both arrive.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_FAAE2, 0x0072);
	v34hs_poke_short(M41_RX_FLAGS, 0x0200);
	step("aae2 0x0072 -> DET_INFO", 120, 19, 2,
	     V34HS_DET_INFO, V34HS_MOH_SILENCE);
	h_detinfo = last_hash;
	record("to DET_INFO, flag cleared");

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_FAAE2, 0x1172);
	v34hs_poke_short(M41_RX_FLAGS, 0x0200);
	step("aae2 0x1172 -> DET_INFO", 121, 19, 2,
	     V34HS_DET_INFO, V34HS_MOH_SILENCE);

	/*
	 * The flag is lowered only when +0x358a says no sub-state is running,
	 * and the byte test happens FIRST -- so this reaches DET_INFO with
	 * +0x358a set to a value that would otherwise have chosen a body.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_FAAE2, 0x0072);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RX_FLAGS, 0x0200);
	step("aae2 0x72 with 358a 2", 122, 18, 2,
	     V34HS_DET_INFO, V34HS_MOH_SILENCE);
	record("to DET_INFO, flag kept");

	/*
	 * The message prints the AGC gain SIGN-extended (`movswl` at
	 * 0x710be), so a negative gain is the only thing that separates the
	 * two spellings and the fill does not supply one.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_FAAE2, 0x0072);
	v34hs_poke_short(M41_RECEIVER + 0x136, -3);
	step("aae2 0x72 with a negative gain", 123, 19, 2,
	     V34HS_DET_INFO, V34HS_MOH_SILENCE);
	record("to DET_INFO, negative gain");

	/* --- 0x6ab33, the junction's two cheap exits --------------------- */

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 3);
	step("358a 3, abe8 clear", 130, 14, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	/*
	 * ASSERTED AS A COLLISION, not recorded as a behaviour.  0x6ab35 and
	 * 0x669fa are two exits that do the same nothing, and so is the cold
	 * path -- three routes, one answer.  Finding 351's distinction: what
	 * is independent here is WHICH route was taken, which the object
	 * cannot show and the mutation suite has to.
	 */
	diff_eq_int("junction with abe8 clear == the cold path",
		    last_hash, h_cold, 130);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_byte(M41_FABE8, 1);
	v34hs_poke_int(M41_FABF0, 0);
	step("abe8 set, abf0 not 1", 131, 14, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	diff_eq_int("junction with abf0 not 1 == the cold path",
		    last_hash, h_cold, 131);

	/*
	 * +0xabf0 is tested for EXACTLY 1 and it is an int.  Two cases: a
	 * non-zero value that is not 1, and a value whose low halfword is 1
	 * and whose upper half is not.  Both must decline, and the case just
	 * below with the same three companions set must not.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_byte(M41_FABE8, 1);
	v34hs_poke_int(M41_FABF0, 2);
	v34hs_poke_short(M41_F35A0, 0x32);
	v34hs_poke_short(M41_FAAE2, 0);
	v34hs_poke_byte(M41_FABF8, 0);
	step("abf0 2 is not 1", 132, 14, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	diff_eq_int("abf0 2 == the cold path", last_hash, h_cold, 132);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_byte(M41_FABE8, 1);
	v34hs_poke_int(M41_FABF0, 0x00010001);
	v34hs_poke_short(M41_F35A0, 0x32);
	v34hs_poke_short(M41_FAAE2, 0);
	v34hs_poke_byte(M41_FABF8, 0);
	step("abf0 0x10001 is not 1", 133, 14, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	diff_eq_int("abf0 0x10001 == the cold path", last_hash, h_cold, 133);

	/* --- 0x6dca7, the FRR NACK report -------------------------------- */

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_byte(M41_FABE8, 1);
	v34hs_poke_int(M41_FABF0, 1);
	v34hs_poke_short(M41_F35A0, 0x31);
	v34hs_poke_short(M41_FAAE2, 0);
	v34hs_poke_byte(M41_FABF8, 0);
	step("frr nack: 35a0 0x31, the boundary", 140, 14, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);

	/*
	 * The +0xaae2 test HERE is sixteen bits wide where the arm's entry
	 * test on the same field was eight, so a halfword whose low byte is
	 * zero and whose top half is not must still be rejected.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_byte(M41_FABE8, 1);
	v34hs_poke_int(M41_FABF0, 1);
	v34hs_poke_short(M41_F35A0, 0x32);
	v34hs_poke_short(M41_FAAE2, 0x1100);
	step("frr nack: aae2 0x1100 rejected", 141, 14, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_byte(M41_FABE8, 1);
	v34hs_poke_int(M41_FABF0, 1);
	v34hs_poke_short(M41_F35A0, 0x32);
	v34hs_poke_short(M41_FAAE2, 0);
	v34hs_poke_byte(M41_FABF8, 1);
	step("frr nack: already reported", 142, 14, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_byte(M41_FABE8, 1);
	v34hs_poke_int(M41_FABF0, 1);
	v34hs_poke_short(M41_F35A0, 0x32);
	v34hs_poke_short(M41_FAAE2, 0);
	v34hs_poke_byte(M41_FABF8, 0);
	step("frr nack: reported here", 143, 15, 1,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	h_frr = last_hash;
	record("frr nack reported");

	/* --- 0x6ce8b, +0x358a == 2, the info marks ----------------------- */

	/*
	 * Every threshold in this body is (rtd >> 4) plus a constant, and the
	 * shift is ARITHMETIC: rtd 400 puts the base at 25, and rtd -400 puts
	 * it at -25, which an unsigned shift could not produce.  The three
	 * constants are 100, 400 and 490.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 125);
	step("marks: aae0 125, the first floor", 150, 14, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 126);
	v34hs_poke_short(M41_F35A0, 0x40);
	v34hs_poke_short(M41_FAAE2, 0);
	step("marks: aae0 126, counter reset", 151, 15, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	record("marks, counter reset");
	h_reset = last_hash;

	/*
	 * rtd negative, so the base is negative: at -400 the first floor is
	 * 75 and a block count of 76 goes on where 400's would not have.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, -400);
	v34hs_poke_short(M41_FAAE0, 75);
	step("marks: rtd -400, floor is 75", 152, 14, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, -400);
	v34hs_poke_short(M41_FAAE0, 76);
	v34hs_poke_short(M41_F35A0, 0x40);
	v34hs_poke_short(M41_FAAE2, 0);
	step("marks: rtd -400, aae0 76", 153, 15, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);

	/*
	 * Past 400 the counter is not reset, and it is only STEPPED when
	 * +0xaae2 is one of its two sentinels.  Three cases: stepped, not
	 * stepped, and zeroed because the transmit state is TONE_AB.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 426);
	v34hs_poke_short(M41_F35A0, 0x10);
	v34hs_poke_short(M41_FAAE2, 0);
	step("marks: counter stepped, aae2 0", 154, 15, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	record("marks, counter stepped");
	h_step0 = last_hash;

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 426);
	v34hs_poke_short(M41_F35A0, 0x10);
	v34hs_poke_short(M41_FAAE2, -1);
	step("marks: counter stepped, aae2 -1", 155, 15, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	/*
	 * The step is `sete` on zero ORed with `sete` on 0xffff, so both
	 * sentinels must give the same answer and anything else a different
	 * one.  Two assertions, because one of them alone would pass with the
	 * test written as a single comparison.
	 */
	diff_eq_int("aae2 -1 steps the counter as aae2 0 does",
		    last_hash, h_step0, 155);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 426);
	v34hs_poke_short(M41_F35A0, 0x10);
	v34hs_poke_short(M41_FAAE2, 5);
	step("marks: not a sentinel, counter 0", 156, 15, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	record("marks, counter cleared");
	diff_eq_int("aae2 5 does not step the counter",
		    last_hash != h_step0, 1, 156);

	begin(V34HS_TONE_AB);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 426);
	v34hs_poke_short(M41_F35A0, 0x40);
	v34hs_poke_short(M41_FAAE2, 0);
	step("marks: txstate TONE_AB zeroes it", 157, 19, 0,
	     V34HS_DET_SYNC, V34HS_TONE_AB);
	record("marks, TONE_AB zeroes counter");

	/*
	 * The reset's own floor is `<=`, and 425 is the one block count that
	 * separates it: the counter comes out at 1 rather than at 0x41.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 425);
	v34hs_poke_short(M41_F35A0, 0x40);
	v34hs_poke_short(M41_FAAE2, 0);
	step("marks: aae0 425 still resets", 158, 15, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	diff_eq_int("aae0 425 resets the counter as 126 does",
		    last_hash, h_reset, 158);

	/*
	 * The counter floor is `<=` too, and 0x31 is where it shows: the arm
	 * takes the late half instead of answering.  0x30 stepped once is the
	 * only value that reaches it, because the step is what makes 0x31.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 426);
	v34hs_poke_short(M41_F35A0, 0x30);
	v34hs_poke_short(M41_FAAE2, 0);
	v34hs_poke_short(M41_F359C, 0);
	step("marks: counter 0x31 takes the late half", 159, 15, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	record("marks, counter 0x31");

	/* --- the info marks' three answers ------------------------------- */

	/*
	 * +0xaae2 == 0: the transmit machine is dropped into SILENCERETRAIN
	 * and the receive machine into WAIT, which is two `hs_setstate`
	 * traces and one message of the body's own.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 426);
	v34hs_poke_short(M41_F35A0, 0x40);
	v34hs_poke_short(M41_FAAE2, 0);
	step("marks: aae2 0, tone during info1", 160, 29, 3,
	     V34HS_DET_SYNC, V34HS_SILENCERETRAIN);
	h_tone1 = last_hash;
	record("marks answer: retrain silence");

	/*
	 * +0xaae2 == -1: the record at +0xa9ac is configured, +0xaa6c is
	 * re-aimed at it -- an interior pointer the harness compares by
	 * offset from each side's own base -- and the state moves to
	 * INFODONE.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 426);
	v34hs_poke_short(M41_F35A0, 0x40);
	v34hs_poke_short(M41_FAAE2, -1);
	step("marks: aae2 -1, reinit Info1c", 161, 51, 3,
	     V34HS_INFODONE, V34HS_TX_DPSK);
	h_infodone = last_hash;
	record("marks answer: INFODONE");

	/* The same body with the transmit state already there. */
	begin(V34HS_TX_DPSK);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 426);
	v34hs_poke_short(M41_F35A0, 0x40);
	v34hs_poke_short(M41_FAAE2, -1);
	step("marks: INFODONE, tx already 24", 162, 50, 2,
	     V34HS_INFODONE, V34HS_TX_DPSK);

	/* --- 0x6dfc2, the late half ------------------------------------- */

	/*
	 * A counter at or below 0x31 skips the two answers above and goes
	 * straight to the third threshold, (rtd >> 4) + 490.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 426);
	v34hs_poke_short(M41_F35A0, 0x10);
	v34hs_poke_short(M41_FAAE2, 0);
	v34hs_poke_short(M41_F359C, 0);
	step("late: below the third floor", 170, 15, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);

	/* 515 = 25 + 490, and the test is `jl`, so 514 leaves and 515 goes. */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 514);
	v34hs_poke_short(M41_F35A0, 0x10);
	v34hs_poke_short(M41_FAAE2, 0);
	v34hs_poke_short(M41_F359C, 0);
	v34hs_poke_short(M41_FA24A, 1);
	step("late: aae0 514, the boundary", 171, 15, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	/*
	 * With +0xa24a already 1 the ONLY thing between this case and the
	 * retrain below is the block count, so the third threshold is pinned
	 * by a pair rather than by one run that could have left for any of
	 * three reasons.
	 */
	diff_eq_int("aae0 514 leaves without retraining",
		    last_hash, h_step0, 171);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 515);
	v34hs_poke_short(M41_F35A0, 0x10);
	v34hs_poke_short(M41_FAAE2, 0);
	v34hs_poke_short(M41_F359C, 0);
	v34hs_poke_short(M41_FA24A, 0);
	step("late: aae0 515, a24a not 1", 172, 15, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	diff_eq_int("a24a not 1 leaves as the short count does",
		    last_hash, h_step0, 172);

	/*
	 * The retrain.  `v34handshakinit(obj, 1)` runs INSIDE the step, so
	 * this is the case finding 359 says `V34HS_REFINIT=1` cannot be used
	 * against: side A installs our library tables and side B the blob's,
	 * and no address comparison can settle two copies of one table.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 515);
	v34hs_poke_short(M41_F35A0, 0x10);
	v34hs_poke_short(M41_FAAE2, 0);
	v34hs_poke_short(M41_F359C, 0);
	v34hs_poke_short(M41_FA24A, 1);
	step("late: retrain for info1c", 173, 34, 9,
	     V34HS_DET_SYNC, V34HS_SILENCERETRAIN);
	h_retrain = last_hash;
	record("late, retrain");

	/*
	 * The marker at +0x359c takes the whole thing somewhere else: three
	 * state machines move, which is three traces and one message.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 515);
	v34hs_poke_short(M41_F35A0, 0x10);
	v34hs_poke_short(M41_FAAE2, 0);
	v34hs_poke_short(M41_F359C, 0x65);
	/*
	 * Three traces and one message, not four traces: the receive machine
	 * is already RX_DPSK -- the route into table 3 requires it -- so
	 * `hs_setstate` declines that one.
	 */
	step("late: 359c 0x65, search info1a", 174, 29, 3,
	     V34HS_RX_PHASE1_CALL, V34HS_TONE_AB);
	h_info1a = last_hash;
	record("late, info1a");

	/* --- 0x6da9c, +0x358a == 1, the tone --------------------------- */

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 1);
	detector(0);
	step("tone: detector silent", 180, 28, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	record("tone, silent");

	/*
	 * A detector that asserts is not enough: the record at +0xaa6c has to
	 * have bit 7 of its byte at +0x04, and that byte is a BYTE.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 1);
	v34hs_poke_byte(M41_REC_AA6C + 4, 0x7f);
	detector(1);
	step("tone: record bit 7 clear", 181, 28, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	record("tone, record rejects");

	/*
	 * Bit 7 set, +0x359c not 0x65: the copy runs, +0xaae2 is invalidated
	 * and the state moves to TX_PHASE1_ANS.  +0xabc2 is the LAST index
	 * copied, so 3 copies four words.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 1);
	v34hs_poke_byte(M41_REC_AA6C + 4, (unsigned char)0x80);
	v34hs_poke_short(M41_F359C, 0);
	v34hs_poke_short(M41_FABC2, 3);
	v34hs_poke_short(M41_ABAE + 0, 0x1111);
	v34hs_poke_short(M41_ABAE + 2, 0x2222);
	v34hs_poke_short(M41_ABAE + 4, 0x3333);
	v34hs_poke_short(M41_ABAE + 6, 0x4444);
	detector(1);
	step("tone: to TX_PHASE1_ANS, copy 4", 182, 41, 4,
	     V34HS_TX_PHASE1_ANS, V34HS_MOH_SILENCE);
	h_tx1ans = last_hash;
	record("tone, TX_PHASE1_ANS");

	/* +0xabc2 negative copies nothing at all: `js`, not a zero test. */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 1);
	v34hs_poke_byte(M41_REC_AA6C + 4, (unsigned char)0x80);
	v34hs_poke_short(M41_F359C, 0);
	v34hs_poke_short(M41_FABC2, -1);
	detector(1);
	step("tone: abc2 -1 copies nothing", 183, 33, 4,
	     V34HS_TX_PHASE1_ANS, V34HS_MOH_SILENCE);
	record("tone, no copy");

	/* And 0 copies exactly one, which is the other side of `<=`. */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 1);
	v34hs_poke_byte(M41_REC_AA6C + 4, (unsigned char)0x80);
	v34hs_poke_short(M41_F359C, 0);
	v34hs_poke_short(M41_FABC2, 0);
	v34hs_poke_short(M41_ABAE + 0, 0x1111);
	detector(1);
	step("tone: abc2 0 copies one", 184, 35, 4,
	     V34HS_TX_PHASE1_ANS, V34HS_MOH_SILENCE);
	record("tone, copy one");

	/*
	 * +0x359c == 0x65 goes to RX_PHASE1_CALL instead, prints a different
	 * message, and does NOT invalidate +0xaae2 -- which is the one thing
	 * that separates the two answers apart from the state they leave.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 1);
	v34hs_poke_byte(M41_REC_AA6C + 4, (unsigned char)0x80);
	v34hs_poke_short(M41_F359C, 0x65);
	v34hs_poke_short(M41_FABC2, 3);
	detector(1);
	step("tone: to RX_PHASE1_CALL", 185, 39, 4,
	     V34HS_RX_PHASE1_CALL, V34HS_MOH_SILENCE);
	h_rx1call = last_hash;
	record("tone, RX_PHASE1_CALL");

	/* --- the behaviours are distinct -------------------------------- */

	/*
	 * Every signature recorded above is asserted DIFFERENT from every
	 * other.  Finding 290's rule applies: the signature holds what the
	 * step wrote and not the state it was entered with, so this is a
	 * claim about the bodies and not about the seeds.  A change that
	 * collapsed two of these into one would otherwise pass in silence.
	 */
	for (i = 0; i < nsig; i++)
		for (j = i + 1; j < nsig; j++)
			diff_eq_int(signame[i], sig[i] != sig[j], 1,
				    (long)(i * 100 + j));

	if (dump)
		printf("  %d distinct behaviours\n", nsig);

	/* --- the diagnostics off ---------------------------------------- */

	/*
	 * EVERY CASE ABOVE RUNS WITH THE DIAGNOSTICS ON, so the other half of
	 * each of this arm's nine `if (DSPLIB_DEBUG_ON())` blocks is not
	 * driven by any of them -- and a store moved INSIDE one of those
	 * guards would survive every check in this file.  Measured rather
	 * than reasoned: that mutation was applied by hand before these cases
	 * existed and the file passed.
	 *
	 * So each body that prints is re-driven with the diagnostics off, and
	 * asserted to write the SAME BYTES and print none.  The byte counts
	 * are the debug-on ones because no out-of-line debug block in this
	 * arm stores to the object; a count that moved would be a finding and
	 * not a fixture wobble.  Finding 358 is the model.
	 */
	v34hs_debug(0);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_FAAE2, 0x0072);
	v34hs_poke_short(M41_RX_FLAGS, 0x0200);
	step("quiet: aae2 0x72 -> DET_INFO", 220, 19, 0,
	     V34HS_DET_INFO, V34HS_MOH_SILENCE);
	diff_eq_int("quiet DET_INFO writes what the loud one did",
		    last_hash, h_detinfo, 220);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_FAAE0, 101);
	v34hs_poke_short(M41_F359C, 0);
	detector(1);
	step("quiet: tone search body", 221, 91, 0,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);
	diff_eq_int("quiet tone search writes what the loud one did",
		    last_hash, h_search, 221);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_byte(M41_FABE8, 1);
	v34hs_poke_int(M41_FABF0, 1);
	v34hs_poke_short(M41_F35A0, 0x32);
	v34hs_poke_short(M41_FAAE2, 0);
	v34hs_poke_byte(M41_FABF8, 0);
	step("quiet: frr nack reported", 222, 15, 0,
	     V34HS_DET_SYNC, V34HS_MOH_SILENCE);
	diff_eq_int("quiet FRR report writes what the loud one did",
		    last_hash, h_frr, 222);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 426);
	v34hs_poke_short(M41_F35A0, 0x40);
	v34hs_poke_short(M41_FAAE2, 0);
	step("quiet: tone during info1", 223, 29, 0,
	     V34HS_DET_SYNC, V34HS_SILENCERETRAIN);
	diff_eq_int("quiet tone-during-info1 writes what the loud one did",
		    last_hash, h_tone1, 223);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 426);
	v34hs_poke_short(M41_F35A0, 0x40);
	v34hs_poke_short(M41_FAAE2, -1);
	step("quiet: reinit Info1c", 224, 51, 0,
	     V34HS_INFODONE, V34HS_TX_DPSK);
	diff_eq_int("quiet Info1c writes what the loud one did",
		    last_hash, h_infodone, 224);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 515);
	v34hs_poke_short(M41_F35A0, 0x10);
	v34hs_poke_short(M41_FAAE2, 0);
	v34hs_poke_short(M41_F359C, 0x65);
	step("quiet: search info1a", 225, 29, 0,
	     V34HS_RX_PHASE1_CALL, V34HS_TONE_AB);
	diff_eq_int("quiet info1a writes what the loud one did",
		    last_hash, h_info1a, 225);

	/*
	 * The retrain is the one whose loud twin printed nine lines, eight of
	 * them `v34handshakinit`'s own, so it is also the strongest of these:
	 * the bring-up runs inside the step either way.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 2);
	v34hs_poke_short(M41_RTD, 400);
	v34hs_poke_short(M41_FAAE0, 515);
	v34hs_poke_short(M41_F35A0, 0x10);
	v34hs_poke_short(M41_FAAE2, 0);
	v34hs_poke_short(M41_F359C, 0);
	v34hs_poke_short(M41_FA24A, 1);
	step("quiet: retrain for info1c", 226, 34, 0,
	     V34HS_DET_SYNC, V34HS_SILENCERETRAIN);
	diff_eq_int("quiet retrain writes what the loud one did",
		    last_hash, h_retrain, 226);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 1);
	v34hs_poke_byte(M41_REC_AA6C + 4, (unsigned char)0x80);
	v34hs_poke_short(M41_F359C, 0);
	v34hs_poke_short(M41_FABC2, 3);
	v34hs_poke_short(M41_ABAE + 0, 0x1111);
	v34hs_poke_short(M41_ABAE + 2, 0x2222);
	v34hs_poke_short(M41_ABAE + 4, 0x3333);
	v34hs_poke_short(M41_ABAE + 6, 0x4444);
	detector(1);
	step("quiet: to TX_PHASE1_ANS", 227, 41, 0,
	     V34HS_TX_PHASE1_ANS, V34HS_MOH_SILENCE);
	diff_eq_int("quiet TX_PHASE1_ANS writes what the loud one did",
		    last_hash, h_tx1ans, 227);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(M41_F358A, 1);
	v34hs_poke_byte(M41_REC_AA6C + 4, (unsigned char)0x80);
	v34hs_poke_short(M41_F359C, 0x65);
	v34hs_poke_short(M41_FABC2, 3);
	detector(1);
	step("quiet: to RX_PHASE1_CALL", 228, 39, 0,
	     V34HS_RX_PHASE1_CALL, V34HS_MOH_SILENCE);
	diff_eq_int("quiet RX_PHASE1_CALL writes what the loud one did",
		    last_hash, h_rx1call, 228);

	/*
	 * The pairwise loop above is only as strong as the number of
	 * signatures fed to it, and a `record()` deleted in an edit would
	 * shrink it in silence -- which is finding 395's hazard turned on
	 * this file.  Pinned.
	 */
	diff_eq_int("behaviours recorded", nsig, 24, 0);

	v34hs_holes_check();
	return diff_end();
}

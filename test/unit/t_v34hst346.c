/*
 * t_v34hst346.c -- `v34handshak`'s microstate 46, `TX_PHASE1_ANS`, 0x65d6d.
 *
 * 3,198 bytes exclusive to one dispatch entry (finding 288's table), driven
 * through test/harness/v34hsstep.c with `v34hs_ours(1)`, so every case below
 * is this tree's `v34handshak` against the blob's over the whole 44,096-byte
 * object, the five blocks it points at, the padding around them and both
 * transcripts.
 *
 * WHAT THE ARM IS.  Two transmit states are special-cased at the head, and
 * then a chain on +0x3588 chooses between four bodies.  The four bodies are
 * one reset with four preludes and four messages:
 *
 *     0x65df4   the counter past 1200 and +0xaae2 non-zero
 *     0x6abc1   the counter past 199 and +0xaae2's low ten bits 0x372
 *     0x6c459   +0x3588 == 2 and +0xaa7a counted past twelve
 *     0x6f90c   txstate TONE_AB with +0xac00 set -- the only one that calls
 *               `V34SetINFO0aBits`, and `V34SetINFO0dBits` when +0x359c is
 *               0x66
 *
 * WHAT IS INDEPENDENT HERE AND WHAT IS NOT.  The shared reset is ONE
 * behavioural check made four times: all four bodies clear the same eleven
 * shorts, move txstate to TX_DPSK and the microstate to DET_SYNC, write
 * +0xaae2 and +0xaae0, and rearm the record at +0xa94c.  That is asserted as
 * a collision -- `record_is_armed` is called after each -- rather than
 * counted four times.  What IS independent is the four entry conditions, the
 * four preludes (`+0x3588 = 4` against `|= 4`, `+0xaa7a = 0`, the two pointer
 * installs, the INFO0 build) and the four distinct message strings.
 *
 * SEEDED, NOT INHERITED (finding 345).  Almost everything the reset writes is
 * a zero, so on a fill that happened to leave one of them zero a mutation
 * deleting that store would be equivalent rather than uncaught.  `begin`
 * therefore puts a distinct non-zero value in every field the arm writes,
 * including all eleven message shorts and all twelve fields of the record.
 *
 * THE TXSTATE IS PART OF THE FIXTURE (finding 288), and here it is more than
 * that: two of its values ARE two of the cases.  MOH_SILENCE (81) is the
 * neutral one -- above table 2's window, so it takes the once-per-block
 * dispatch's own default -- and TX_DPSK (24) and TONE_AB (60) are driven
 * because the arm names them.  Every body ends by setting TX_DPSK, which
 * selects table 2's arm at 0x644c9.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "v34hsstep.h"
#include "dsplib/v34hshak.h"

/* Offsets this case seeds or reads.  Prefixed: four batches share the file. */
#define T46T_PROGRESS	0x0004
#define T46T_LVL_LIMIT	0x0230
#define T46T_LVL_COUNT	0x0234
#define T46T_TIMER_LO	0x0238
#define T46T_TIMER_HI	0x023c
#define T46T_V90RX	0x024c
#define T46T_MODE	0x2218
#define T46T_F3588	0x3588
#define T46T_F358A	0x358a
#define T46T_F358C	0x358c
#define T46T_F359C	0x359c
#define T46T_SESSION	0x3548
#define T46T_REC	0xa94c	/* the record +0xaa6c is aimed at            */
#define T46T_INFO0D	0xa97c	/* the record +0xaa70 is aimed at            */
#define T46T_PTR_AA6C	0xaa6c
#define T46T_PTR_AA70	0xaa70
#define T46T_COUNT	0xaa78
#define T46T_COUNT3	0xaa7a
#define T46T_FAAE0	0xaae0
#define T46T_FAAE2	0xaae2
#define T46T_MSG	0xabae	/* eleven shorts, 0xabae..0xabc2             */
#define T46T_MSG_N	11
#define T46T_LOCAL_SH	0xabca
#define T46T_IS_SHORT	0xabcc
#define T46T_BULKDELAY	0xac02
#define T46T_RETRAIN	0xac00

/* The session block's two fields `V34SetINFO0aBits` reads. */
#define T46T_SESS_VARIANT	0x6120
#define T46T_SESS_CAPS		0x612c

static int dump;
static int default_fill;

/*
 * A capability block for `V34SetINFO0aBits` to read.
 *
 * ONE BUFFER FOR BOTH SIDES, and deliberately so.  The session block is
 * filled with pseudorandom bytes, so its capability pointer is wild and the
 * INFO0 body would fault before it could fail.  Aiming it at each side's own
 * session block instead would put two different addresses in two blocks the
 * comparison reads byte for byte, which is precisely the asymmetry findings
 * 319-322 are about.  A single buffer keeps the two blocks identical AND is
 * safe, because everything downstream only READS through the pointer:
 * `V34SetINFO0aBits` takes one byte at +0x11 of it and writes nothing.
 */
static unsigned char caps[64];

static void
poke_session_int(unsigned off, int v)
{
	int side;

	for (side = 0; side < 2; side++) {
		unsigned char *o = (unsigned char *)v34hs_object(side);
		unsigned char *s = *(unsigned char **)(o + T46T_SESSION);

		memcpy(s + off, &v, sizeof(v));
	}
}

static void
poke_session_caps(void)
{
	const void *p = caps;
	int side;

	for (side = 0; side < 2; side++) {
		unsigned char *o = (unsigned char *)v34hs_object(side);
		unsigned char *s = *(unsigned char **)(o + T46T_SESSION);

		memcpy(s + T46T_SESS_CAPS, &p, sizeof(p));
	}
}

/*
 * Open a case, with every field the arm writes holding a distinct non-zero
 * value.
 *
 * The tail at 0x62a40 reads +0x2218, two timer words and the receiver's AGC
 * level and writes a progress code from them, so those are pinned for the
 * same reason t_v34hst3core pins them: otherwise a microstate case is a
 * microstate case plus whatever the fill made the tail do.
 */
static void
begin(short tx)
{
	int i;

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(V34HS_TX_PHASE1_ANS, V34HS_RX_DPSK, tx);

	v34hs_poke_int(T46T_MODE, 0);
	v34hs_poke_int(T46T_TIMER_LO, 1000);
	v34hs_poke_int(T46T_TIMER_HI, 2000);
	v34hs_poke_int(T46T_LVL_LIMIT, 0x7fffffff);
	v34hs_poke_int(T46T_LVL_COUNT, 0);

	/* The message buffer the reset clears, all eleven shorts. */
	for (i = 0; i < T46T_MSG_N; i++)
		v34hs_poke_short(T46T_MSG + 2 * i, (short)(0x0141 + 0x111 * i));

	/* The two the reset clears beside it, and the two state bytes. */
	v34hs_poke_short(T46T_LOCAL_SH, 0x0333);
	v34hs_poke_short(T46T_IS_SHORT, 0x0444);
	v34hs_poke_short(T46T_F358A, 0x0555);
	v34hs_poke_short(T46T_F358C, 0x0666);
	v34hs_poke_short(T46T_FAAE0, 0x0777);

	/*
	 * The record at +0xa94c.  Every one of the twelve stores lands on a
	 * value it differs from, so deleting any of them is observable.
	 */
	v34hs_poke_short(T46T_REC + 0x14, 0x1111);
	v34hs_poke_short(T46T_REC + 0x16, 0x2222);
	v34hs_poke_short(T46T_REC + 0x18, 0x3333);
	v34hs_poke_short(T46T_REC + 0x1a, 0x4444);
	v34hs_poke_short(T46T_REC + 0x1c, 0x5555);
	v34hs_poke_short(T46T_REC + 0x1e, 0x6666);
	v34hs_poke_short(T46T_REC + 0x20, 0x7777);
	v34hs_poke_short(T46T_REC + 0x22, 0x0788);
	v34hs_poke_int(T46T_REC + 0x24, 0x11223344);
	v34hs_poke_short(T46T_REC + 0x28, 0x0abc);
	v34hs_poke_short(T46T_REC + 0x2a, 0x0def);
	v34hs_poke_int(T46T_REC + 0x2c, 0x55667788);

	/*
	 * AIM +0xaa6c INTO THE OBJECT.  The fixture pre-aims it at a dummy
	 * block, so the TX_DPSK guard's two reads would come from the fill and
	 * the branch would be whichever the fill picked.  Pointed at the
	 * record, `v34hs_poke_short` reaches both of them -- and the object
	 * itself aims it there in two of its bodies.
	 */
	v34hs_poke_self_ptr(T46T_PTR_AA6C, T46T_REC);

	/* Defaults the individual cases override. */
	v34hs_poke_short(T46T_F3588, 0);
	v34hs_poke_short(T46T_COUNT, 0);
	v34hs_poke_short(T46T_COUNT3, 0);
	v34hs_poke_short(T46T_FAAE2, 0);
	v34hs_poke_short(T46T_F359C, 0x0010);
	v34hs_poke_int(T46T_V90RX, 0);
	v34hs_poke_byte(T46T_RETRAIN, 0);
}

/*
 * Step, compare, and say what the step DID.
 *
 * The comparison is the differential check; the assertions after it are the
 * anti-vacuity half.  A case that took a branch other than the one it is
 * named for still compares -- both sides took it -- so without them a seed
 * that stopped reaching a body would leave the test green and testing
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
	if (dump)
		printf("  %-36s changed %3u  lines %u  mst %2d tx %2d "
		       "hash %08x\n", what, o->changed, o->lines,
		       o->mst, o->txst, o->hash);

	if (default_fill)
		diff_eq_int("object bytes the step wrote", o->changed, changed,
			    tag);
	diff_eq_int("diagnostic lines printed", o->lines, lines, tag);
	diff_eq_int("microstate afterwards", o->mst, mst, tag);
	diff_eq_int("txstate afterwards", o->txst, txst, tag);
}

/*
 * The reset all four bodies share, asserted once per body.
 *
 * THIS IS ONE CHECK MADE FOUR TIMES and is written that way on purpose: the
 * four bodies reach the same code, so four passes of it are four observations
 * of one claim.  What separates the bodies is asserted at each call site.
 */
static void
record_is_armed(const char *what, long tag, short plus18)
{
	int i;
	char msg[96];

	snprintf(msg, sizeof(msg), "%s: record +0x14", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_REC + 0x14), -1, tag);
	snprintf(msg, sizeof(msg), "%s: record +0x16", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_REC + 0x16), 1, tag);
	snprintf(msg, sizeof(msg), "%s: record +0x18", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_REC + 0x18), plus18, tag);
	snprintf(msg, sizeof(msg), "%s: record +0x1a", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_REC + 0x1a), 0, tag);
	snprintf(msg, sizeof(msg), "%s: record +0x1c", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_REC + 0x1c), 8, tag);
	snprintf(msg, sizeof(msg), "%s: record +0x1e", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_REC + 0x1e), 0, tag);
	snprintf(msg, sizeof(msg), "%s: record +0x20", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_REC + 0x20), 1, tag);
	snprintf(msg, sizeof(msg), "%s: record +0x22", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_REC + 0x22), 0, tag);
	snprintf(msg, sizeof(msg), "%s: record +0x24", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_REC + 0x24), 0xf72, tag);
	snprintf(msg, sizeof(msg), "%s: record +0x28", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_REC + 0x28), 0xc, tag);
	snprintf(msg, sizeof(msg), "%s: record +0x2a", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_REC + 0x2a), 0xc, tag);
	snprintf(msg, sizeof(msg), "%s: record +0x2c", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_REC + 0x2c), 0xf72, tag);

	for (i = 0; i < T46T_MSG_N; i++) {
		snprintf(msg, sizeof(msg), "%s: message short %d", what, i);
		diff_eq_int(msg, v34hs_peek_short(0, T46T_MSG + 2 * i), 0, tag);
	}

	snprintf(msg, sizeof(msg), "%s: +0xabca cleared", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_LOCAL_SH), 0, tag);
	snprintf(msg, sizeof(msg), "%s: +0xabcc cleared", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_IS_SHORT), 0, tag);
	snprintf(msg, sizeof(msg), "%s: +0x358a set to one", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_F358A), 1, tag);
	snprintf(msg, sizeof(msg), "%s: +0xaae2 set to -1", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_FAAE2), -1, tag);
	snprintf(msg, sizeof(msg), "%s: +0xaae0 cleared", what);
	diff_eq_int(msg, v34hs_peek_short(0, T46T_FAAE0), 0, tag);
}

int
main(void)
{
	unsigned quiet_changed;

	dump = getenv("V34HS_DUMP") != NULL;
	default_fill = getenv("V34HS_SEED") == NULL;
	memset(caps, 0, sizeof(caps));
	caps[0x11] = 0;			/* no V.92 indication unless asked */

	diff_begin("v34handshak: microstate 46, TX_PHASE1_ANS");

	/*
	 * OURS ON SIDE A.  Every case below is a comparison of this tree's
	 * `v34handshak` against the blob's, and the moment this is removed
	 * they all become blob against blob and pass for a reason that has
	 * nothing to do with the reconstruction.
	 */
	v34hs_ours(1);
	v34hs_debug(1);

	/* --- the chain on +0x3588, and the counter it guards -------------- */

	/*
	 * +0x3588 non-zero and not 2: straight out through the transmit
	 * dispatch.  Nothing of the arm runs, which is the point -- the two
	 * bodies below are reached only because this one is not.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_F3588, 1);
	v34hs_poke_short(T46T_COUNT, 5000);
	v34hs_poke_short(T46T_FAAE2, 0x0f72);
	step("+0x3588 == 1, straight out", 100, 12, 0, V34HS_TX_PHASE1_ANS,
	     V34HS_MOH_SILENCE);

	/*
	 * +0x3588 clear and the counter exactly 199: NOT past it, so the
	 * 0x372 test is never made, and 199 is also under 1200.  Both
	 * boundaries are `jle`, and each has its other side below.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_COUNT, 199);
	v34hs_poke_short(T46T_FAAE2, 0x0372);
	step("counter 199, under both limits", 101, 12, 0,
	     V34HS_TX_PHASE1_ANS, V34HS_MOH_SILENCE);

	/*
	 * 1200 exactly: past 199 but the 0x372 test fails, and 1200 is not
	 * past 1200 either.  The far side of the second boundary is case 110.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_COUNT, 1200);
	v34hs_poke_short(T46T_FAAE2, 0x0373);
	step("counter 1200, exactly at the limit", 102, 12, 0,
	     V34HS_TX_PHASE1_ANS, V34HS_MOH_SILENCE);

	/* Past 1200 but +0xaae2 zero: 0x6bd7a, and no body. */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_COUNT, 1201);
	v34hs_poke_short(T46T_FAAE2, 0);
	step("counter past 1200, +0xaae2 clear", 103, 12, 0,
	     V34HS_TX_PHASE1_ANS, V34HS_MOH_SILENCE);

	/* --- 0x65df4, the body for a counter past 1200 -------------------- */

	/*
	 * +0xaae2 is 1, which is neither 0x372 in ten bits nor 0xf72 in
	 * twelve, so this reaches the LATE body and not the other two.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_COUNT, 1201);
	v34hs_poke_short(T46T_FAAE2, 1);
	step("body 0x65df4, past 1200", 110, 79, 3, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	record_is_armed("body 0x65df4", 110, 0x11);
	diff_eq_int("body 0x65df4: +0x3588 stored as four",
		    v34hs_peek_short(0, T46T_F3588), 4, 110);

	/*
	 * THE RECORD'S +0x18, all three ways.  It is 0x1e only when +0x359c
	 * is 0x65 AND a V.90 receiver is running, and both halves are pinned:
	 * the second case is the only thing in this arm that reads +0x24c,
	 * so if the base were four out the field would not move.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_COUNT, 1201);
	v34hs_poke_short(T46T_FAAE2, 1);
	v34hs_poke_short(T46T_F359C, 0x65);
	v34hs_poke_int(T46T_V90RX, 0);
	step("body 0x65df4, +0x359c 0x65 and no V.90", 111, 79, 3,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);
	record_is_armed("+0x359c 0x65, no V.90", 111, 0x11);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_COUNT, 1201);
	v34hs_poke_short(T46T_FAAE2, 1);
	v34hs_poke_short(T46T_F359C, 0x65);
	v34hs_poke_int(T46T_V90RX, 7);
	step("body 0x65df4, +0x359c 0x65 with V.90", 112, 79, 3,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);
	record_is_armed("+0x359c 0x65, V.90", 112, 0x1e);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_COUNT, 1201);
	v34hs_poke_short(T46T_FAAE2, 1);
	v34hs_poke_short(T46T_F359C, 0x64);
	v34hs_poke_int(T46T_V90RX, 7);
	step("body 0x65df4, +0x359c 0x64 with V.90", 113, 79, 3,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);
	record_is_armed("+0x359c 0x64, V.90", 113, 0x11);

	/*
	 * THE SAME BODY WITH THE TRANSMIT STATE ALREADY AT TX_DPSK.  The
	 * transition then prints nothing, so the line count drops by one and
	 * every byte the body writes is unchanged.  That separates what the
	 * arm WRITES from what it PRINTS.
	 *
	 * +0x20 of the record is cleared first so that the TX_DPSK head guard
	 * does NOT take its pre-step; case 130 takes it.
	 */
	begin(V34HS_TX_DPSK);
	v34hs_poke_short(T46T_REC + 0x20, 0);
	v34hs_poke_short(T46T_COUNT, 1201);
	v34hs_poke_short(T46T_FAAE2, 1);
	step("body 0x65df4 entered at TX_DPSK", 114, 77, 2, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	record_is_armed("entered at TX_DPSK", 114, 0x11);

	/* And with the diagnostics off: the same bytes, no lines. */
	v34hs_debug(0);
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_COUNT, 1201);
	v34hs_poke_short(T46T_FAAE2, 1);
	v34hs_step();
	v34hs_compare("body 0x65df4, quiet", 115);
	quiet_changed = v34hs_observed(0)->changed;
	diff_eq_int("body 0x65df4, quiet: no lines",
		    v34hs_observed(0)->lines, 0, 115);
	if (default_fill)
		diff_eq_int("body 0x65df4, quiet: the same bytes as loud",
			    quiet_changed, 79, 115);
	v34hs_debug(1);

	/* --- 0x6abc1, the body for the ten-bit 0x372 ---------------------- */

	/*
	 * THE MASK IS TEN BITS AND THE TEST IS FOR 0x372.  0x0372 fires it
	 * and so does 0x0f72, whose top nibble the mask drops -- which is
	 * what a mask widened to twelve bits would get wrong.  0x0373 does
	 * not fire it (case 102 above).
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_COUNT, 200);
	v34hs_poke_short(T46T_FAAE2, 0x0372);
	step("body 0x6abc1, +0xaae2 0x0372", 120, 79, 3, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	record_is_armed("body 0x6abc1", 120, 0x11);
	diff_eq_int("body 0x6abc1: +0x3588 stored as four",
		    v34hs_peek_short(0, T46T_F3588), 4, 120);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_COUNT, 200);
	v34hs_poke_short(T46T_FAAE2, 0x0f72);
	step("body 0x6abc1, +0xaae2 0x0f72", 121, 79, 3, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	record_is_armed("body 0x6abc1, ten bits", 121, 0x11);

	/* --- the head guard at TX_DPSK, 0x6b5b0 --------------------------- */

	/*
	 * BOTH FIELDS MUST BE SET.  +0x20 alone and +0x22 alone leave the
	 * counter where it was; only both together restart it and clear
	 * +0x20.  Three cases because one of them cannot tell `&&` from `||`.
	 */
	begin(V34HS_TX_DPSK);
	v34hs_poke_short(T46T_REC + 0x20, 0);
	v34hs_poke_short(T46T_REC + 0x22, 0x0788);
	v34hs_poke_short(T46T_COUNT, 5000);
	step("TX_DPSK guard, +0x20 clear", 130, 16, 0, V34HS_TX_PHASE1_ANS,
	     V34HS_TX_DPSK);
	diff_eq_int("TX_DPSK guard, +0x20 clear: counter untouched",
		    v34hs_peek_short(0, T46T_COUNT), 5000, 130);

	begin(V34HS_TX_DPSK);
	v34hs_poke_short(T46T_REC + 0x20, 0x7777);
	v34hs_poke_short(T46T_REC + 0x22, 0);
	v34hs_poke_short(T46T_COUNT, 5000);
	step("TX_DPSK guard, +0x22 clear", 131, 16, 0, V34HS_TX_PHASE1_ANS,
	     V34HS_TX_DPSK);
	diff_eq_int("TX_DPSK guard, +0x22 clear: counter untouched",
		    v34hs_peek_short(0, T46T_COUNT), 5000, 131);

	/*
	 * Both set: the counter is restarted and +0x20 cleared, and the arm
	 * then continues with a counter of zero -- which is under 1200, so it
	 * leaves through 0x6c120 rather than through a body.  Driving it with
	 * 5000 and +0xaae2 non-zero makes that visible: without the pre-step
	 * this case would reach 0x65df4.
	 */
	begin(V34HS_TX_DPSK);
	v34hs_poke_short(T46T_COUNT, 5000);
	v34hs_poke_short(T46T_FAAE2, 1);
	step("TX_DPSK guard, both set", 132, 20, 0, V34HS_TX_PHASE1_ANS,
	     V34HS_TX_DPSK);
	diff_eq_int("TX_DPSK guard: counter restarted",
		    v34hs_peek_short(0, T46T_COUNT), 0, 132);
	diff_eq_int("TX_DPSK guard: record +0x20 cleared",
		    v34hs_peek_short(0, T46T_REC + 0x20), 0, 132);
	diff_eq_int("TX_DPSK guard: record +0x22 left alone",
		    v34hs_peek_short(0, T46T_REC + 0x22), 0x0788, 132);

	/* --- the head guard at TONE_AB, 0x6b4ba --------------------------- */

	/*
	 * THE COUNTER IS STEPPED BEFORE IT IS COMPARED, and the comparison is
	 * `jle` against 399.  398 -> 399 stops here; 399 -> 400 does not.
	 */
	begin(V34HS_TONE_AB);
	v34hs_poke_short(T46T_COUNT, 398);
	step("TONE_AB, counter reaches 399", 140, 17, 0, V34HS_TX_PHASE1_ANS,
	     V34HS_TONE_AB);
	diff_eq_int("TONE_AB, 399: counter stepped",
		    v34hs_peek_short(0, T46T_COUNT), 399, 140);

	/* Past 399 with +0xaae2 non-zero: stepped, and nothing else. */
	begin(V34HS_TONE_AB);
	v34hs_poke_short(T46T_COUNT, 399);
	v34hs_poke_short(T46T_FAAE2, 1);
	step("TONE_AB, past 399 with +0xaae2 set", 141, 17, 0,
	     V34HS_TX_PHASE1_ANS, V34HS_TONE_AB);
	diff_eq_int("TONE_AB, past 399: counter stepped",
		    v34hs_peek_short(0, T46T_COUNT), 400, 141);

	/*
	 * Past 399, +0xaae2 clear and +0xac00 clear: 0x6b4ee.  The counter is
	 * RESTARTED rather than stepped, bit 0 of +0x358c is toggled, the
	 * microstate goes to RX_PHASE1_ANS and +0x358a takes 2 rather than
	 * the 1 every body writes.
	 *
	 * BOTH POLARITIES OF THE TOGGLE.  One run cannot tell `^ 1` from
	 * `| 1` or from `& ~1`; two can.
	 */
	begin(V34HS_TONE_AB);
	v34hs_poke_short(T46T_COUNT, 399);
	v34hs_poke_short(T46T_F358C, 0x0666);
	step("TONE_AB, hand over to RX_PHASE1_ANS", 142, 22, 1,
	     V34HS_RX_PHASE1_ANS, V34HS_TONE_AB);
	diff_eq_int("TONE_AB hand-over: counter restarted",
		    v34hs_peek_short(0, T46T_COUNT), 0, 142);
	diff_eq_int("TONE_AB hand-over: +0x358c bit 0 set",
		    v34hs_peek_short(0, T46T_F358C), 0x0667, 142);
	diff_eq_int("TONE_AB hand-over: +0x358a takes two",
		    v34hs_peek_short(0, T46T_F358A), 2, 142);

	begin(V34HS_TONE_AB);
	v34hs_poke_short(T46T_COUNT, 399);
	v34hs_poke_short(T46T_F358C, 0x0667);
	step("TONE_AB, hand over, +0x358c bit already set", 143, 22, 1,
	     V34HS_RX_PHASE1_ANS, V34HS_TONE_AB);
	diff_eq_int("TONE_AB hand-over: +0x358c bit 0 cleared",
		    v34hs_peek_short(0, T46T_F358C), 0x0666, 143);

	/* --- 0x6f90c, the INFO0 body -------------------------------------- */

	/*
	 * +0xac00 set, no V.90 receiver: `V34SetINFO0aBits` takes one of its
	 * two shallow branches, `V34SetINFO0dBits` is not called at all
	 * (+0x359c is not 0x66), the two records are aimed and +0xac00 is
	 * cleared on the way out.
	 */
	begin(V34HS_TONE_AB);
	v34hs_poke_short(T46T_COUNT, 399);
	v34hs_poke_byte(T46T_RETRAIN, 3);
	poke_session_int(T46T_SESS_VARIANT, 0);
	step("body 0x6f90c, no V.90 receiver", 150, 87, 4, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	record_is_armed("body 0x6f90c", 150, 0x11);
	diff_eq_int("body 0x6f90c: counter stepped, not restarted",
		    v34hs_peek_short(0, T46T_COUNT), 400, 150);
	diff_eq_int("body 0x6f90c: +0xac00 cleared",
		    v34hs_peek_short(0, T46T_RETRAIN) & 0xff, 0, 150);
	diff_eq_int("body 0x6f90c: +0x3588 has bit 2 set",
		    v34hs_peek_short(0, T46T_F3588), 4, 150);
	diff_eq_int("body 0x6f90c: INFO0a wrote index 0",
		    v34hs_peek_short(0, T46T_REC + 0), 0xff, 150);
	diff_eq_int("body 0x6f90c: INFO0a wrote index 1",
		    v34hs_peek_short(0, T46T_REC + 2), 0x84, 150);

	/*
	 * +0x3588 IS OR-ED HERE AND STORED IN THE OTHER THREE.  Seeded with
	 * bit 0 set, the body leaves 5 and not 4, which no `= 4` can produce.
	 */
	begin(V34HS_TONE_AB);
	v34hs_poke_short(T46T_COUNT, 399);
	v34hs_poke_byte(T46T_RETRAIN, 3);
	v34hs_poke_short(T46T_F3588, 1);
	poke_session_int(T46T_SESS_VARIANT, 0);
	step("body 0x6f90c, +0x3588 or-ed not stored", 151, 87, 4,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);
	diff_eq_int("body 0x6f90c: +0x3588 or-ed with four",
		    v34hs_peek_short(0, T46T_F3588), 5, 151);

	/*
	 * WITH A V.90 RECEIVER, so that `V34SetINFO0dBits` has something to
	 * do: it writes 30 into index 12 of the record at +0xa97c, which the
	 * body aims +0xaa70 at and then reads BACK rather than reusing.
	 * +0x359c is 0x66 and not 0x65, so the record's +0x18 is still 0x11 --
	 * the two tests on +0x359c are different tests.
	 */
	begin(V34HS_TONE_AB);
	v34hs_poke_short(T46T_COUNT, 399);
	v34hs_poke_byte(T46T_RETRAIN, 3);
	v34hs_poke_int(T46T_V90RX, 5);
	v34hs_poke_short(T46T_F359C, 0x66);
	v34hs_poke_short(T46T_INFO0D + 24, 0x1234);
	v34hs_poke_short(T46T_LOCAL_SH, 0);
	poke_session_int(T46T_SESS_VARIANT, 1);
	poke_session_caps();
	step("body 0x6f90c, +0x359c 0x66", 152, 87, 5, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	record_is_armed("body 0x6f90c with V.90", 152, 0x11);
	diff_eq_int("body 0x6f90c: INFO0d wrote index 12",
		    v34hs_peek_short(0, T46T_INFO0D + 24), 30, 152);

	/* And 0x67, where it is not called: index 12 is left alone. */
	begin(V34HS_TONE_AB);
	v34hs_poke_short(T46T_COUNT, 399);
	v34hs_poke_byte(T46T_RETRAIN, 3);
	v34hs_poke_int(T46T_V90RX, 5);
	v34hs_poke_short(T46T_F359C, 0x67);
	v34hs_poke_short(T46T_INFO0D + 24, 0x1234);
	v34hs_poke_short(T46T_LOCAL_SH, 0);
	v34hs_poke_short(T46T_BULKDELAY, 0x0999);
	poke_session_int(T46T_SESS_VARIANT, 1);
	poke_session_caps();
	step("body 0x6f90c, +0x359c 0x67", 153, 85, 4, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	diff_eq_int("body 0x6f90c: INFO0d not called",
		    v34hs_peek_short(0, T46T_INFO0D + 24), 0x1234, 153);
	diff_eq_int("body 0x6f90c: bulk delay left alone",
		    v34hs_peek_short(0, T46T_BULKDELAY), 0x0999, 153);

	/*
	 * AND THE ORDER: `V34SetINFO0aBits` reads +0xabca BEFORE the reset
	 * clears it.  Seeded non-zero it asks for a short phase 2, which is
	 * two more diagnostic lines and a write to +0xac02; a reset moved
	 * ahead of the call would lose all three.  The only difference from
	 * case 153 is that one seed.
	 */
	begin(V34HS_TONE_AB);
	v34hs_poke_short(T46T_COUNT, 399);
	v34hs_poke_byte(T46T_RETRAIN, 3);
	v34hs_poke_int(T46T_V90RX, 5);
	v34hs_poke_short(T46T_F359C, 0x67);
	v34hs_poke_short(T46T_INFO0D + 24, 0x1234);
	v34hs_poke_short(T46T_LOCAL_SH, 1);
	v34hs_poke_short(T46T_BULKDELAY, 0x0999);
	poke_session_int(T46T_SESS_VARIANT, 1);
	poke_session_caps();
	step("body 0x6f90c, +0xabca read before it is cleared", 154, 88, 6,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);
	diff_eq_int("body 0x6f90c: bulk delay set by INFO0a",
		    v34hs_peek_short(0, T46T_BULKDELAY), 0x14, 154);
	diff_eq_int("body 0x6f90c: +0xabca cleared afterwards",
		    v34hs_peek_short(0, T46T_LOCAL_SH), 0, 154);

	/* --- +0x3588 == 2, the counting path at 0x6c3f3 -------------------- */

	/* Counter not past 199: out, and +0xaa7a is not touched. */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_F3588, 2);
	v34hs_poke_short(T46T_COUNT, 199);
	v34hs_poke_short(T46T_FAAE2, 0x0f72);
	v34hs_poke_short(T46T_COUNT3, 5);
	step("+0x3588 == 2, counter 199", 160, 12, 0, V34HS_TX_PHASE1_ANS,
	     V34HS_MOH_SILENCE);
	diff_eq_int("+0x3588 == 2, counter 199: +0xaa7a untouched",
		    v34hs_peek_short(0, T46T_COUNT3), 5, 160);

	/*
	 * THE MASK HERE IS TWELVE BITS AND THE TEST IS FOR 0xf72.  0x1f72
	 * fires it -- the mask drops the bit above -- and 0x0f71 does not.
	 * A mask widened to sixteen gets the first wrong.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_F3588, 2);
	v34hs_poke_short(T46T_COUNT, 200);
	v34hs_poke_short(T46T_FAAE2, 0x0f71);
	v34hs_poke_short(T46T_COUNT3, 5);
	step("+0x3588 == 2, +0xaae2 0x0f71", 161, 12, 0, V34HS_TX_PHASE1_ANS,
	     V34HS_MOH_SILENCE);
	diff_eq_int("+0xaae2 0x0f71: +0xaa7a untouched",
		    v34hs_peek_short(0, T46T_COUNT3), 5, 161);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_F3588, 2);
	v34hs_poke_short(T46T_COUNT, 200);
	v34hs_poke_short(T46T_FAAE2, 0x1f72);
	v34hs_poke_short(T46T_COUNT3, 5);
	step("+0x3588 == 2, +0xaae2 0x1f72", 162, 13, 1, V34HS_TX_PHASE1_ANS,
	     V34HS_MOH_SILENCE);
	diff_eq_int("+0xaae2 0x1f72: +0xaa7a stepped",
		    v34hs_peek_short(0, T46T_COUNT3), 6, 162);

	/*
	 * +0xaa7a IS STEPPED BEFORE IT IS COMPARED, `jle` against twelve, and
	 * the trace prints the value BEFORE the step.  11 -> 12 stops here.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_F3588, 2);
	v34hs_poke_short(T46T_COUNT, 200);
	v34hs_poke_short(T46T_FAAE2, 0x0f72);
	v34hs_poke_short(T46T_COUNT3, 11);
	step("+0xaa7a reaches twelve", 163, 13, 1, V34HS_TX_PHASE1_ANS,
	     V34HS_MOH_SILENCE);
	diff_eq_int("+0xaa7a reaches twelve: stepped",
		    v34hs_peek_short(0, T46T_COUNT3), 12, 163);

	/* --- 0x6c459, the body +0xaa7a running out reaches ---------------- */

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_F3588, 2);
	v34hs_poke_short(T46T_COUNT, 200);
	v34hs_poke_short(T46T_FAAE2, 0x0f72);
	v34hs_poke_short(T46T_COUNT3, 12);
	step("body 0x6c459, +0xaa7a past twelve", 170, 80, 4, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	record_is_armed("body 0x6c459", 170, 0x11);
	diff_eq_int("body 0x6c459: +0xaa7a restarted",
		    v34hs_peek_short(0, T46T_COUNT3), 0, 170);
	diff_eq_int("body 0x6c459: +0x3588 stored as four",
		    v34hs_peek_short(0, T46T_F3588), 4, 170);

	/* --- the three blocks each body owns a copy of --------------------- */

	/*
	 * EACH BODY HAS ITS OWN COPY of the record's 0x1e variant (0x716b8,
	 * 0x6fed8, 0x71027, 0x6fb81) and its own copy of the block that skips
	 * the transmit trace when the state is already TX_DPSK (0x6e4c3,
	 * 0x6feae, 0x70c75, 0x6fbb6).  `t46_init_record` and `hs_setstate`
	 * collapse all four of each into one, which is right -- the four
	 * copies are the same instructions -- but a collapse nothing drives
	 * is an unasserted claim.  Body 0x65df4's two are cases 112 and 114;
	 * these are the other five, and they are ASSERTED TO AGREE rather
	 * than merely to pass.
	 *
	 * The eighth, 0x6fbb6, is unreachable: body 0x6f90c is entered only
	 * from the TONE_AB guard and nothing between there and its compare
	 * writes +0x3596, so the transmit state is always 60 there.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_COUNT, 200);
	v34hs_poke_short(T46T_FAAE2, 0x0372);
	v34hs_poke_short(T46T_F359C, 0x65);
	v34hs_poke_int(T46T_V90RX, 7);
	step("body 0x6abc1, +0x359c 0x65 with V.90", 180, 79, 3,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);
	record_is_armed("body 0x6abc1, 0x1e", 180, 0x1e);

	begin(V34HS_MOH_SILENCE);
	v34hs_poke_short(T46T_F3588, 2);
	v34hs_poke_short(T46T_COUNT, 200);
	v34hs_poke_short(T46T_FAAE2, 0x0f72);
	v34hs_poke_short(T46T_COUNT3, 12);
	v34hs_poke_short(T46T_F359C, 0x65);
	v34hs_poke_int(T46T_V90RX, 7);
	step("body 0x6c459, +0x359c 0x65 with V.90", 181, 80, 4,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);
	record_is_armed("body 0x6c459, 0x1e", 181, 0x1e);

	/*
	 * And 0x6f90c's, which also re-separates its two tests on +0x359c:
	 * 0x65 selects the record's +0x18 and leaves `V34SetINFO0dBits`
	 * alone, where 0x66 in case 152 did the opposite.
	 */
	begin(V34HS_TONE_AB);
	v34hs_poke_short(T46T_COUNT, 399);
	v34hs_poke_byte(T46T_RETRAIN, 3);
	v34hs_poke_int(T46T_V90RX, 5);
	v34hs_poke_short(T46T_F359C, 0x65);
	v34hs_poke_short(T46T_INFO0D + 24, 0x1234);
	v34hs_poke_short(T46T_LOCAL_SH, 0);
	poke_session_int(T46T_SESS_VARIANT, 1);
	poke_session_caps();
	step("body 0x6f90c, +0x359c 0x65 with V.90", 182, 85, 4,
	     V34HS_DET_SYNC, V34HS_TX_DPSK);
	record_is_armed("body 0x6f90c, 0x1e", 182, 0x1e);
	diff_eq_int("body 0x6f90c: 0x65 does not call INFO0d",
		    v34hs_peek_short(0, T46T_INFO0D + 24), 0x1234, 182);

	/* The other two bodies entered with the transmit state already there. */
	begin(V34HS_TX_DPSK);
	v34hs_poke_short(T46T_REC + 0x20, 0);
	v34hs_poke_short(T46T_COUNT, 200);
	v34hs_poke_short(T46T_FAAE2, 0x0372);
	step("body 0x6abc1 entered at TX_DPSK", 183, 77, 2, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	record_is_armed("body 0x6abc1 at TX_DPSK", 183, 0x11);

	begin(V34HS_TX_DPSK);
	v34hs_poke_short(T46T_REC + 0x20, 0);
	v34hs_poke_short(T46T_F3588, 2);
	v34hs_poke_short(T46T_COUNT, 200);
	v34hs_poke_short(T46T_FAAE2, 0x0f72);
	v34hs_poke_short(T46T_COUNT3, 12);
	step("body 0x6c459 entered at TX_DPSK", 184, 78, 3, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	record_is_armed("body 0x6c459 at TX_DPSK", 184, 0x11);

	/*
	 * THE POINTER HOLES.  `v34hs_compare` skips thirty-five pointer
	 * fields and compares each by offset from its own base; this asserts
	 * every one of them was reached.  Two of the arm's bodies write two
	 * of them, so the check is live here rather than inherited.
	 */
	v34hs_holes_check();

	if (dump)
		printf("  quiet body wrote %u bytes\n", quiet_changed);

	return diff_end();
}

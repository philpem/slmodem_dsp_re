/*
 * t_v34hsmst44.c -- `v34handshak`'s microstate 44, `DET_INFO`.
 *
 * OURS ON SIDE A.  `v34hs_ours(1)` puts this tree's `v34handshak` on side A
 * of test/harness/v34hsstep.c, so every case below is an ordinary tier-1
 * differential comparison of our arm against the blob's over the whole
 * 44,096-byte object, the five blocks it points at, the padding around them
 * and both transcripts.  t_v34hsstep.c goes on proving the FIXTURE, blob
 * against blob; this proves one arm.
 *
 * WHAT 44 IS.  `fskdemodulate` runs earlier on this route and leaves the bits
 * it recovered in `obj->fsk`; 44 is the clock that consumes them, ONE PER
 * CALL.  It steps a CRC-16 register in the record at +0xaa70 while the
 * message is arriving, stores a byte every eighth bit, and when the counter
 * reaches the message length plus the sixteen CRC bits it compares the
 * register against what arrived and either accepts the message or restarts
 * the exchange.
 *
 * WHAT IS COVERED, and what is not:
 *
 *     0x668c0   the bit clock, all five of its exits
 *     0x6c133   the CRC step without the xor
 *     0x6bda0   the restart a failed CRC takes, and its three sub-branches
 *     0x718fc   f359c == 0x66, the answering side
 *     0x71920   f359c == 0x65 with a V.90 receiver
 *     0x6e534   NOT WRITTEN -- the accept path, 4,744 bytes.  `t3c_unwritten`
 *               halts there, so no case below may reach it and every case
 *               that could is arranged so that the register and the received
 *               bits DISAGREE.
 *
 * THE TXSTATE IS PART OF THE FIXTURE (finding 288).  Every arm of table 3
 * leaves through the once-per-block transmit dispatch, so a microstate case
 * is a microstate arm AND a transmit arm.  MOH_SILENCE (81) throughout, which
 * is above table 2's window and selects the dispatch's own default at 0x62a40
 * -- except that the restart FORCES TX_DPSK on its way out, which is table
 * 2's arm at 0x644c9.  That arm exists (finding 354 put it there for
 * microstate 79) and this is why the restart can be driven at all.
 *
 * THE FSK RECEIVER IS HELD STILL.  `obj->fsk_inhibit` non-zero makes
 * `fskdemodulate` return before it touches `nbits` or `sr` (dpsk.c), so the
 * two fields this arm reads are the test's to choose rather than whatever the
 * fill's coefficients made of four samples.  One case at the end leaves it
 * clear, so the arm is also driven downstream of the real demodulator.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "v34hsstep.h"
#include "dsplib/v34hshak.h"

/* The tail's own inputs, pinned so that the arm is the only variable. */
#define T44T_PROGRESS	0x0004
#define T44T_LVL_LIMIT	0x0230
#define T44T_LVL_COUNT	0x0234
#define T44T_TIMER_LO	0x0238
#define T44T_TIMER_HI	0x023c
#define T44T_MODE	0x2218

#define T44T_V90RECV	0x024c	/* int:   `v90_receiver`                   */
#define T44T_INHIBIT	0x0402	/* short: `fsk_inhibit`                    */
#define T44T_F3588	0x3588
#define T44T_F358A	0x358a
#define T44T_F359C	0x359c	/* short: 0x65 originate, 0x66 answer      */
#define T44T_PTR_AA6C	0xaa6c
#define T44T_PTR_AA70	0xaa70
#define T44T_COUNT	0xaa78	/* short: bits taken so far                */
#define T44T_FAA7A	0xaa7a	/* short: cleared on entry, every call     */
#define T44T_NBITS	0xaae0	/* short: `fsk.nbits`                      */
#define T44T_SR		0xaae2	/* short: `fsk.sr`                         */
#define T44T_FABAE	0xabae
#define T44T_FABC2	0xabc2
#define T44T_LOCALSHORT	0xabca
#define T44T_ISSHORT	0xabcc

/*
 * The two records.  `v34handshakinit` mode 0 aims +0xaa70 at +0xa97c, and the
 * restart installs +0xa94c at +0xaa6c; the first is ASSERTED below rather
 * than assumed, because every offset this file seeds is relative to it.
 */
#define T44T_REC	0xa97c
#define T44T_BLK	0xa94c

#define T44T_R_CRC	0x14
#define T44T_R_F16	0x16
#define T44T_R_NBITS	0x18
#define T44T_R_F1A	0x1a
#define T44T_R_F1C	0x1c
#define T44T_R_F1E	0x1e
#define T44T_R_F20	0x20
#define T44T_R_F22	0x22
#define T44T_R_F24	0x24
#define T44T_R_F28	0x28
#define T44T_R_F2A	0x2a
#define T44T_R_F2C	0x2c

static int dump;

/*
 * Whether the object fill is the default one.  `changed` counts bytes that
 * DIFFER from what the fill left, so a write of the value already there is
 * not counted; that one measurement is asserted at the default fill and
 * everything else at every fill.  The differential comparison itself is never
 * relaxed at any knob.  Finding 359 made the same distinction.
 */
static int default_fill;

/*
 * Open a case: both objects built and brought up, the rxstate chain routed to
 * table 3, the three state words written, the tail's five inputs pinned, the
 * FSK receiver switched off, and every field this arm can write seeded to
 * something OTHER than what the arm will store.
 *
 * That last part is finding 345's warning: a claim about a value written is
 * untestable against a field that already holds it.
 */
static void
begin(short tx)
{
	int i;

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(V34HS_DET_INFO, V34HS_RX_DPSK, tx);

	v34hs_poke_int(T44T_MODE, 0);
	v34hs_poke_int(T44T_TIMER_LO, 1000);
	v34hs_poke_int(T44T_TIMER_HI, 2000);
	v34hs_poke_int(T44T_LVL_LIMIT, 0x7fffffff);
	v34hs_poke_int(T44T_LVL_COUNT, 0);

	v34hs_poke_short(T44T_INHIBIT, 1);

	/* Non-zero, so the unconditional clear at 0x668c9 is observable. */
	v34hs_poke_short(T44T_FAA7A, 0x5a5a);

	/* The ten byte slots, distinct and none of them a value stored. */
	for (i = 0; i < 10; i++)
		v34hs_poke_short(T44T_REC + 2 * i, (short)(0x0301 + 0x111 * i));

	/* What the restart clears or sets, none of it already right. */
	v34hs_poke_short(T44T_F3588, 0x0a02);	/* bit 2 clear, others set */
	v34hs_poke_short(T44T_F358A, 0x7abc);
	v34hs_poke_short(T44T_LOCALSHORT, 0x4d4d);
	v34hs_poke_short(T44T_ISSHORT, 0x5e5e);
	for (i = 0; i <= 10; i++)
		v34hs_poke_short(T44T_FABAE + 2 * i, (short)(0x2001 + 7 * i));

	/* The record the restart installs and fills in. */
	v34hs_poke_short(T44T_BLK + T44T_R_CRC, 0x1234);
	v34hs_poke_short(T44T_BLK + T44T_R_F16, 0x2345);
	v34hs_poke_short(T44T_BLK + T44T_R_NBITS, 0x3456);
	v34hs_poke_short(T44T_BLK + T44T_R_F1A, 0x4567);
	v34hs_poke_short(T44T_BLK + T44T_R_F1C, 0x5678);
	v34hs_poke_short(T44T_BLK + T44T_R_F1E, 0x6789);
	v34hs_poke_short(T44T_BLK + T44T_R_F20, 0x789a);
	v34hs_poke_short(T44T_BLK + T44T_R_F22, 0x1a2b);
	v34hs_poke_int(T44T_BLK + T44T_R_F24, 0x2b3c4d);
	v34hs_poke_short(T44T_BLK + T44T_R_F28, 0x3c4d);
	v34hs_poke_short(T44T_BLK + T44T_R_F2A, 0x4d5e);
	v34hs_poke_int(T44T_BLK + T44T_R_F2C, 0x5e6f70);
}

/* One bit's worth of state: what is waiting, and where the clock is. */
static void
bits(short nbits, short sr, short count, short len, short crc)
{
	v34hs_poke_short(T44T_NBITS, nbits);
	v34hs_poke_short(T44T_SR, sr);
	v34hs_poke_short(T44T_COUNT, count);
	v34hs_poke_short(T44T_REC + T44T_R_NBITS, len);
	v34hs_poke_short(T44T_REC + T44T_R_CRC, crc);
}

/*
 * Step, compare, and say what the step DID.
 *
 * The comparison is the differential check; the four `diff_eq_int`s are the
 * anti-vacuity half.  A case that took a different branch from the one it is
 * named for still compares -- both sides took it -- so without them a seed
 * that stopped reaching the branch would leave the test green and testing
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
		printf("  %-34s changed %3u  lines %u  mst %2d tx %2d\n",
		       what, o->changed, o->lines, o->mst, o->txst);

	/*
	 * No conversion in these: `diff_eq_int` appends the input when the
	 * format has none (finding 220), and a `%s` would be handed a long.
	 */
	if (default_fill)
		diff_eq_int("object bytes the step wrote", o->changed, changed,
			    tag);
	diff_eq_int("diagnostic lines printed", o->lines, lines, tag);
	diff_eq_int("microstate afterwards", o->mst, mst, tag);
	diff_eq_int("txstate afterwards", o->txst, txst, tag);
}

/* What the restart leaves behind, checked once per sub-branch. */
static void
check_restart(long tag, short rec_len, short blk_len)
{
	const char *base = (const char *)v34hs_object(0);

	diff_eq_int("restart: the CRC register is reset",
		    v34hs_peek_short(0, T44T_REC + T44T_R_CRC), -1, tag);
	diff_eq_int("restart: the message length",
		    v34hs_peek_short(0, T44T_REC + T44T_R_NBITS), rec_len, tag);
	diff_eq_int("restart: +0x358a", v34hs_peek_short(0, T44T_F358A), 1,
		    tag);
	diff_eq_int("restart: bit 2 of +0x3588",
		    v34hs_peek_short(0, T44T_F3588), 0x0a06, tag);
	diff_eq_int("restart: local_short cleared",
		    v34hs_peek_short(0, T44T_LOCALSHORT), 0, tag);
	diff_eq_int("restart: is_short cleared",
		    v34hs_peek_short(0, T44T_ISSHORT), 0, tag);
	diff_eq_int("restart: +0xabc2 cleared",
		    v34hs_peek_short(0, T44T_FABC2), 0, tag);
	/*
	 * TEN SHORTS FROM +0xabae, AND THE FIRST OF THEM IS INDEX ZERO.
	 * 0x6bf1f loads 1 into %eax for the +0x358a store and 0x6bf3e zeroes
	 * it again before the loop, so a reading that starts the loop at one
	 * is exactly the plausible mistake -- and it leaves +0xabae holding
	 * the seed, which this catches.  The far end is +0xabc0.
	 */
	diff_eq_int("restart: +0xabae, the first", v34hs_peek_short(0, 0xabae),
		    0, tag);
	diff_eq_int("restart: +0xabc0, the tenth", v34hs_peek_short(0, 0xabc0),
		    0, tag);

	/* The installed record. */
	diff_eq_int("restart: +0xaa6c aimed at +0xa94c",
		    (int)(*(char *const *)(base + T44T_PTR_AA6C) - base),
		    T44T_BLK, tag);
	diff_eq_int("restart: the installed record's length",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_NBITS), blk_len, tag);
	diff_eq_int("restart: installed +0x14",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_CRC), -1, tag);
	diff_eq_int("restart: installed +0x16",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F16), 1, tag);
	diff_eq_int("restart: installed +0x1a",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F1A), 0, tag);
	diff_eq_int("restart: installed +0x1c",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F1C), 8, tag);
	diff_eq_int("restart: installed +0x1e",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F1E), 0, tag);
	diff_eq_int("restart: installed +0x20",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F20), 1, tag);
	diff_eq_int("restart: installed +0x22",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F22), 0, tag);
	diff_eq_int("restart: installed +0x28",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F28), 0xc, tag);
	diff_eq_int("restart: installed +0x2a",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F2A), 0xc, tag);
	diff_eq_int("restart: installed +0x24 low",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F24), 0xf72, tag);
	diff_eq_int("restart: installed +0x24 high",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F24 + 2), 0, tag);
	diff_eq_int("restart: installed +0x2c low",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F2C), 0xf72, tag);
	diff_eq_int("restart: installed +0x2c high",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F2C + 2), 0, tag);
}

int
main(void)
{
	const char *base;

	dump = getenv("V34HS_DUMP") != NULL;
	default_fill = getenv("V34HS_SEED") == NULL;
	diff_begin("v34handshak: microstate 44, DET_INFO");

	v34hs_ours(1);
	v34hs_debug(1);

	/*
	 * WHERE +0xaa70 POINTS, asserted and not assumed.  Every offset this
	 * file seeds into the record is relative to +0xa97c, so a bring-up
	 * that aimed the field somewhere else would leave every seed landing
	 * in the wrong place and every case below passing for nothing.
	 */
	begin(V34HS_MOH_SILENCE);
	base = (const char *)v34hs_object(0);
	diff_eq_int("+0xaa70 points at the record at +0xa97c",
		    (int)(*(char *const *)(base + T44T_PTR_AA70) - base),
		    T44T_REC, 1);

	/* --- the outermost gate ------------------------------------------ */

	/*
	 * NO BIT WAITING.  `fsk.nbits` zero and the arm is over at 0x668da:
	 * it clears +0xaa7a and leaves through the transmit dispatch without
	 * touching the counter, the register or the byte array.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(0, 0x1234, 100, 200, 0x4321);
	step("no bit waiting", 100, 13, 0, V34HS_DET_INFO, V34HS_MOH_SILENCE);
	diff_eq_int("no bit: the counter did not move",
		    v34hs_peek_short(0, T44T_COUNT), 100, 100);
	diff_eq_int("no bit: the register did not move",
		    v34hs_peek_short(0, T44T_REC + T44T_R_CRC), 0x4321, 100);
	diff_eq_int("no bit: +0xaa7a cleared",
		    v34hs_peek_short(0, T44T_FAA7A), 0, 100);

	/*
	 * ONE BIT WAITING, AND EXACTLY ONE IS TAKEN.  `nbits` goes 5 -> 4,
	 * not 5 -> 0: the arm consumes one bit per call however many are
	 * queued, which is what makes the counter and `nbits` two different
	 * quantities.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 0, 64, 0x0001);
	step("one bit taken", 101, 16, 0, V34HS_DET_INFO, V34HS_MOH_SILENCE);
	diff_eq_int("one bit: nbits decremented once",
		    v34hs_peek_short(0, T44T_NBITS), 4, 101);
	diff_eq_int("one bit: the counter stepped",
		    v34hs_peek_short(0, T44T_COUNT), 1, 101);

	/* --- the CRC register: the feedback bit's whole truth table ------- */

	/*
	 * FOUR INDEPENDENT CASES, one per row.  The feedback is the register's
	 * bit 15 XOR the incoming bit, and 0x1021 is xored in only when it is
	 * one, so each row fails on its own for its own reason and none of
	 * them can be got right by getting another wrong.
	 *
	 *   bit15   sr&1   feedback   register after
	 *     0      0        0       0x0001 << 1            = 0x0002
	 *     1      0        1       0x8000 << 1 ^ 0x1021   = 0x1021
	 *     0      1        1       0x0001 << 1 ^ 0x1021   = 0x1023
	 *     1      1        0       0x8001 << 1            = 0x0002
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 0, 64, 0x0001);
	step("CRC: 0,0 -> no xor", 110, 16, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("CRC 0,0", v34hs_peek_short(0, T44T_REC + T44T_R_CRC),
		    0x0002, 110);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 0, 64, (short)0x8000);
	step("CRC: 1,0 -> xor", 111, 17, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("CRC 1,0", v34hs_peek_short(0, T44T_REC + T44T_R_CRC),
		    0x1021, 111);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1235, 0, 64, 0x0001);
	step("CRC: 0,1 -> xor", 112, 17, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("CRC 0,1", v34hs_peek_short(0, T44T_REC + T44T_R_CRC),
		    0x1023, 112);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1235, 0, 64, (short)0x8001);
	step("CRC: 1,1 -> no xor", 113, 17, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("CRC 1,1", v34hs_peek_short(0, T44T_REC + T44T_R_CRC),
		    0x0002, 113);

	/*
	 * THE REGISTER STOPS AT THE MESSAGE LENGTH, and the boundary is `<`.
	 * Two cases one apart: at 63 of 64 the register steps, at 64 of 64 it
	 * does not, because the sixteen bits that follow the message ARE the
	 * CRC and are compared rather than accumulated.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 63, 64, 0x0001);
	step("CRC: one bit short of the length", 114, 18, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("the register stepped at 63 of 64",
		    v34hs_peek_short(0, T44T_REC + T44T_R_CRC), 0x0002, 114);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 64, 64, 0x0001);
	step("CRC: at the length", 115, 15, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("the register held at 64 of 64",
		    v34hs_peek_short(0, T44T_REC + T44T_R_CRC), 0x0001, 115);

	/* --- the byte store, and the three exits that skip it ------------- */

	/*
	 * EVERY EIGHTH BIT COMPLETES A BYTE.  The counter reaching 8 stores
	 * the low byte of `sr`, zero extended, into slot 0; a counter of 9
	 * stores nothing.  Two cases, so the mask is pinned rather than the
	 * store.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x12ab, 7, 200, 0x0001);
	step("byte 0 stored", 120, 19, 0, V34HS_DET_INFO, V34HS_MOH_SILENCE);
	diff_eq_int("the low byte of sr, widened",
		    v34hs_peek_short(0, T44T_REC + 0), 0x00ab, 120);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x12ab, 8, 200, 0x0001);
	step("no byte at nine bits", 121, 17, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("slot 0 untouched at nine bits",
		    v34hs_peek_short(0, T44T_REC + 0), 0x0301, 121);

	/*
	 * THE TENTH BYTE AND THE ELEVENTH.  Slot 9 is the last: at 80 bits it
	 * is written and at 88 the arm leaves through 0x71857 with nothing
	 * stored.  The bound is `> 9` on the index, which is
	 * (counter >> 3) - 1, so these two also pin the -1.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x12ab, 79, 200, 0x0001);
	step("byte 9, the last slot", 122, 19, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("slot 9 written", v34hs_peek_short(0, T44T_REC + 18),
		    0x00ab, 122);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x12ab, 87, 200, 0x0001);
	step("one byte too many", 123, 17, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("nothing past slot 9",
		    v34hs_peek_short(0, T44T_REC + 18),
		    (short)(0x0301 + 0x111 * 9), 123);

	/*
	 * A COUNTER THAT IS NOT POSITIVE takes 0x7186a instead, and the test
	 * is `<= 0` and not `== 0`: at -8 the eighth-bit mask is satisfied
	 * -- -8 & 7 is zero -- and the arm still stores nothing.  Two cases,
	 * because one of them alone cannot tell `jle` from `je`.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x12ab, -1, 200, 0x0001);
	step("the counter reaches zero", 124, 18, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("the counter is zero",
		    v34hs_peek_short(0, T44T_COUNT), 0, 124);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x12ab, -9, 200, 0x0001);
	step("a negative counter, byte aligned", 125, 17, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("still nothing stored",
		    v34hs_peek_short(0, T44T_REC + 0), 0x0301, 125);

	/* --- the restart, 0x6bda0 ---------------------------------------- */

	/*
	 * THE MESSAGE IS COMPLETE AND ITS CRC IS WRONG.  The threshold is the
	 * message length plus sixteen: with a length of 24 the counter
	 * reaching 40 ends the message, and the register (0x4321) against
	 * what arrived (0x1234) disagrees, so the exchange restarts.
	 *
	 * THE REGISTER MUST DISAGREE IN EVERY CASE HERE.  Agreement is
	 * 0x6e534, which nobody has written and which halts.
	 *
	 * 40 is also a multiple of eight, so the restart is followed by the
	 * byte store -- on the 0xffff the restart itself left in `sr`, which
	 * is how the two halves are shown to be one arm.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0050);
	step("restart, neither side", 130, 82, 3, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	check_restart(130, 24, 0x11);
	diff_eq_int("restart: sr reset",
		    v34hs_peek_short(0, T44T_SR), -1, 130);
	diff_eq_int("restart: nbits cleared",
		    v34hs_peek_short(0, T44T_NBITS), 0, 130);
	diff_eq_int("restart: byte 4 stored from the reset sr",
		    v34hs_peek_short(0, T44T_REC + 8), 0x00ff, 130);

	/*
	 * ONE SHORT OF THE THRESHOLD, so the restart does NOT run.  This is
	 * what makes the +0x10 a measurement rather than a transcription: at
	 * 39 of 24+16 the arm is an ordinary bit and the state words do not
	 * move.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 38, 24, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0050);
	step("one short of the threshold", 131, 15, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("no restart: +0x358a untouched",
		    v34hs_peek_short(0, T44T_F358A), 0x7abc, 131);

	/*
	 * THE ANSWERING SIDE, `f359c == 0x66`.  One extra store -- the
	 * record's length becomes 0x1e -- and one extra diagnostic line.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0066);
	step("restart, answering side", 132, 83, 4, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	check_restart(132, 0x1e, 0x11);

	/*
	 * THE ORIGINATING SIDE, `f359c == 0x65`, and the V.90 receiver
	 * decides ONE FIELD: the installed record's length is 0x1e when there
	 * is one and 0x11 when there is not.  Both are driven, so the
	 * condition is pinned from both sides.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0065);
	v34hs_poke_int(T44T_V90RECV, 3);
	step("restart, originating with V.90", 133, 82, 3, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	check_restart(133, 24, 0x1e);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0065);
	v34hs_poke_int(T44T_V90RECV, 0);
	step("restart, originating without V.90", 134, 82, 3, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	check_restart(134, 24, 0x11);

	/*
	 * AND `f359c == 0x66` DOES NOT REACH THE SECOND BRANCH.  The two
	 * tests are 0x66 for the record's length and 0x65 for the installed
	 * one, and a reconstruction that used one value for both would pass
	 * every case above; this is the case that separates them, because it
	 * has a V.90 receiver AND the answering side's value.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0066);
	v34hs_poke_int(T44T_V90RECV, 3);
	step("restart, answering side with V.90", 135, 83, 4, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	check_restart(135, 0x1e, 0x11);

	/*
	 * A RESTART THAT IS NOT BYTE ALIGNED.  A length of 17 puts the
	 * threshold at 33, so the restart runs and the arm then leaves
	 * through 0x6bd8d with no byte stored.  Same restart, different tail.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 32, 17, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0050);
	step("restart, not byte aligned", 136, 80, 3, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	check_restart(136, 17, 0x11);
	diff_eq_int("no byte stored on a 33-bit restart",
		    v34hs_peek_short(0, T44T_REC + 0), 0x0301, 136);

	/* --- downstream of the real demodulator --------------------------- */

	/*
	 * `fsk_inhibit` CLEAR, so `fskdemodulate` really runs and the bits
	 * this arm consumes are the ones it produced.  Everything above holds
	 * the FSK receiver still to make `nbits` and `sr` the test's to
	 * choose; this one case gives that up in exchange for driving the arm
	 * where the object drives it.
	 *
	 * The branch is still pinned: `nbits` is seeded high enough that the
	 * demodulator cannot bring it to zero, and the counter reaching 1 is
	 * neither byte aligned nor at any threshold, so the exit is 0x6bd8d
	 * whatever four samples the queue delivered.  `changed` is not
	 * asserted here -- it counts what the demodulator wrote too.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(200, 0x1234, 0, 500, 0x0001);
	v34hs_poke_short(T44T_INHIBIT, 0);
	v34hs_step();
	v34hs_compare("with the demodulator running", 140);
	diff_eq_int("the demodulator ran and the arm still took one bit",
		    v34hs_peek_short(0, T44T_COUNT), 1, 140);
	diff_eq_int("and stepped the register",
		    v34hs_peek_short(0, T44T_REC + T44T_R_CRC),
		    (v34hs_peek_short(0, T44T_SR) & 1) != 0 ? 0x1023 : 0x0002,
		    140);

	/* --- the transmit state is part of the case ----------------------- */

	/*
	 * MOH_CLEARDOWN as the transmit state.  84 is above table 2's window
	 * like MOH_SILENCE, but the tail compares against it by name and
	 * writes a progress code of its own (finding 358), so this is a
	 * different tail from every case above with the same arm in front of
	 * it.
	 */
	begin(V34HS_MOH_CLEARDOWN);
	bits(5, 0x12ab, 7, 200, 0x0001);
	step("byte 0 stored, MOH_CLEARDOWN", 150, 23, 0, V34HS_DET_INFO,
	     V34HS_MOH_CLEARDOWN);
	diff_eq_int("the tail's own progress code",
		    (int)((const int *)v34hs_object(0))[1], 0x10, 150);

	/*
	 * THE POINTER HOLES.  `v34hs_compare` skips thirty-five pointer
	 * fields and compares each by offset from its own base; this asserts
	 * every one of them was reached, so the skip list cannot go stale
	 * while these cases run.  Finding 290.
	 */
	v34hs_holes_check();

	return diff_end();
}

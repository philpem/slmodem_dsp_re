/*
 * v34hstx1.cpp -- seventeen arms of `v34handshak`'s per-sample transmit
 * dispatch.
 *
 * IT IS A `.cpp` WHERE `v34handshak` IS C, which is finding F217's rule rather
 * than a choice.  78 `JaTXMIT` tail-calls `v90Phase34` and 85 `K56JaTXMIT`
 * tail-calls `k56FlexPhase34`; both live in the V.90/V.92 C++ half, and the
 * five SpanDSP interop binaries link `$(SRC)` -- every `.c` under src/ and no
 * C++ at all.  A `.c` file naming either symbol leaves them undefined and
 * takes those five down with it, which is what v34info1a.cpp, v34k56.cpp and
 * v34pcmmain.cpp are `.cpp` for.  Measured here rather than assumed: as a
 * `.c` this file broke all five, and `make phase` was how it said so.
 * Finding F344.
 *
 * The table is `.rodata+0x2da0`, indexed `txstate - 5`, read at 0x62966
 * inside the loop that runs while the transmit queue's count at +0x221c is
 * below the block's sample limit at +0x2aa0.  See include/dsplib/v34hstx1.h
 * for what an arm is and docs/v34handshak.md for the harness.
 *
 * ---------------------------------------------------------------------------
 * THE ONE REGISTER CONVENTION THE WHOLE TABLE IS WRITTEN IN, because reading
 * an arm without it gives every field the wrong offset:
 *
 *     0xc0(%esp)  the object
 *     0x4c(%esp)  the object + 0x221c   -- the transmit queue
 *     0x74(%esp)  the object + 0x264    -- the receiver
 *
 * so `0x3a6(%esi)` with `esi` from 0x4c(%esp) is +0x25c2 in the object, not
 * +0x3a6.  The six fields that region carries are `seg_symcount`, `tx_flags`, `prev_quadrant`,
 * `cur_quadrant`, `tx_scr_sr` and `txpoint`, and they are named in `struct v34_object`.
 *
 * ---------------------------------------------------------------------------
 * TWO PAIRS THAT LOOK LIKE ONE ARM AND ARE NOT.
 *
 * 78 `JaTXMIT` and 85 `K56JaTXMIT` are instruction-for-instruction identical
 * -- the same counter and the same `v34FreezeEcho` -- up to the tail call,
 * which is `v90Phase34` for one and
 * `k56FlexPhase34` for the other.  They are two arms and the object gives
 * them two table entries; finding F340 measures them apart.
 *
 * 71 `TXLEVEL` and 86 `TXMD` share the scrambler step and differ in what they
 * transmit: 71 always sends `vect4[0]` and 86 sends `vect4[q]`, the point the
 * scrambler just chose.  71 advances the register and throws the result away
 * except as `cur_quadrant`.
 *
 * ---------------------------------------------------------------------------
 * AND ONE PAIR THAT AGREES COLD AND IS STILL NOT ONE ARM.
 *
 * Finding F323 measured 24 `TX_DPSK` and 60 `TONE_AB` writing the same 69
 * bytes with the same progress code, the table's one collision.  They are two
 * entries at two addresses -- 24's is 0x62b96 and 60's is 0x62d3d, and 60's
 * whole body is thirty-four bytes where 24's is 2,220 -- so the agreement is
 * a property of ONE object fill and of nothing else.  BOTH are written here
 * now, and the agreement is stated where 24 is: cold, the reader hands 24 a
 * zero bit and the Modem-on-Hold flag is clear, so 24 sends the tone 60
 * sends and writes nothing else the object can see.  It is agreement on one
 * path, not identity.
 *
 * WHICH PATH 323 MEASURED, since 60 has two.  The default fill leaves
 * +0x358c at 0xb7eb, which is ODD, so the cold run sent `vect4[2]`; and
 * finding F323's 69 bytes for 60 is what `t_v34hstx1.c`'s odd case still
 * writes, where its even case writes 65.  The collision was measured on one
 * of the two paths and the fill decides which -- seed 1 leaves 0xb06e and
 * would have measured the other.  So the agreement is narrower than the
 * table entry, which is the reason both values are poked here rather than
 * one of them being reached by luck.
 */

#include <string.h>

/*
 * `V34EchoReportCoeff` is no longer CALLED here -- `v34FreezeEcho` makes both
 * of its calls (finding F572) -- but nine mutations in `v34hstx1.json` inline
 * the freeze back out to ask whether the factoring is right, and a mutant
 * that fails to compile is reported CAUGHT for the wrong reason.  The
 * declaration stays for them.
 */
#include "dsplib/debug.h"	/* dsplibs_debug_level, dsplibs_debug_printf */
#include "dsplib/v34filt.h"	/* V34EchoReportCoeff, V34SetupModulator */
#include "dsplib/v34fsk.h"	/* struct v34_object, struct v34_ratecfg */
#include "dsplib/v34hshak.h"	/* vect4, v90Phase34, k56FlexPhase34      */
#include "dsplib/v34hstx1.h"
#include "dsplib/v34info.h"	/* V34SetINFO0aBits                       */
#include "dsplib/v34pcmif.h"	/* VPcmV34Report*OfEchoAdapt              */
#include "dsplib/v34pcmif.h"	/* VPcmV34GetMaxUpstreamRateIndex         */
#include "dsplib/v34recv.h"	/* struct v34_receiver                    */
#include "dsplib/v34rx.h"	/* txmit, txwritequeue, V34scrambler      */
#include "dsplib/v34shell.h"	/* modulatevector                         */
#include "dsplib/sysdep.h"	/* sysdep_memset                          */

/*
 * Bit 4 of `tx_flags`, established as `PROG_TXBIT_DATA` beside its other
 * reader in v34pcmmain.cpp and catalogued in v34fsk.h's own comment on the
 * field; re-declared here, file-local, for this file's own reader below.
 */
#define PROG_TXBIT_DATA		0x10

/* The receiver sub-object; `0x74(%esp)` above. */
#define TX1_RECEIVER	0x264

/* The transmit state machine's own word (finding F213). */
#define TX1_TXSTATE	0x3596

/*
 * +0xaa78, the counter three of these six read.  v34hshak.c calls it
 * `HS_TRACE_2` because every handshake diagnostic prints it as `[2]`;
 * `v34handshakinit` zeroes it and the arms here count it DOWN to zero (78,
 * 85) or count `vect_idx` UP to it (86).  It is inside `unmapped_aa78`, so
 * it is reached by offset exactly as v34hshak.c:421 reaches it.
 */
#define TX1_COUNT	0xaa78

/*
 * +0x35a6.  86 compares `vect_idx` against it to decide when the modulator is
 * reconfigured.  Nothing else in the tree reads it; it is `unmapped_35a6`.
 */
#define TX1_SEGLEN	0x35a6

/* The modulator, where V34SetupModulator writes it -- v34hshak.c:302. */
#define TX1_MODULATOR	0x1450

/*
 * +0x358c.  60 masks it with one to choose between two of `vect4`'s four
 * points.  It is inside `unmapped_3564`; the only other site in the tree is
 * v34hshak.c:1480, where `v34handshakinit` clears it.
 */
#define TX1_F358C	0x358c

/* +0x3594, the RXSTATE (finding F213).  74's retrain is its only writer here. */
#define TX1_RXSTATE	0x3594

/*
 * +0x3592, the MICROSTATE (finding F213).  51 is the one arm here that reads
 * a state word belonging to another machine: it selects between two copies of
 * its own loop on `microstate == TX_L1`, having been dispatched on
 * `txstate == TX_L1`.  The two 51s are the same number in two machines and
 * `StateName` is one table for all three (docs/v34handshak.md).
 */
#define TX1_MICROSTATE	0x3592

/*
 * +0x2218, an int.  Table 2's tail reads it to choose four of its arms
 * (v34hstxblock.c's `TB_F2218`); 70 is a writer of it.
 *
 * Named `hs_mode` in `struct v34_object` (`v34fsk.h`), on `v34hshak.c`'s own
 * `DP_MODE`/`T3C_MODE` names for the same int: `datapumpv34`'s recovery
 * supervisor reads it as "handshake above 1", and 70 setting it to 1 here is
 * exactly that transition.  Kept as a raw-offset macro in this file rather
 * than reached as `o->hs_mode`, matching every other field here that a
 * second file also reaches by offset (`v34hstxblock.c`'s own `TB_F2218`) --
 * see `TX1_FAAE0`/`TX1_FAA3C` above for why that convention is deliberate.
 */
#define TX1_HS_MODE	0x2218

/*
 * +0x358e and +0x35a4, two more words of `unmapped_3564`.
 *
 * +0x358e is a SEGMENT COUNTER shared by 19 and 20: 20 counts it up to six
 * and 19 clears it, both on the pass that changes `txstate`.  +0x35a4 is the
 * length 19 scales by 0x53 into +0x35a6, and zero there is what sends 19 to
 * PPSEG instead of TXMD.  Neither has another reader in this tree.
 */
#define TX1_F358E	0x358e
#define TX1_F35A4	0x35a4

/*
 * +0xaa7a, +0xaa7c and +0xaa86, the three words of the `unmapped_aa78` /
 * `unmapped_aa80` region these arms touch besides the counter at +0xaa78.
 * 5/54/74 clears +0xaa7a before it writes the queue; 20 adds +0xaa7c into the
 * counter and accumulates +0xaa86.  `rtd` at +0xaa7e is named in
 * `struct v34_object` and is reached as a field.
 */
#define TX1_FAA7A	0xaa7a
#define TX1_FAA7C	0xaa7c
#define TX1_FAA86	0xaa86

/*
 * +0xaae0 and +0xaae2.  `struct v34_object` names them `fsk.nbits` and
 * `fsk.sr` because the FSK demodulator reaches them that way; the handshake
 * uses them as two words of its own -- v34hshak.c's table-3 core reaches them
 * by offset as `T3C_FAAE0` and `T3C_FAAE2` for the same reason, and 74's
 * retrain writes -1 and 0 into them.  Reached by offset here so that the
 * name of the other reader is not asserted to be the meaning here.
 */
#define TX1_FAAE0	0xaae0
#define TX1_FAAE2	0xaae2

/*
 * +0xabe8, a BYTE.  `v34handshakinit`'s Modem-on-Hold bring-up sets it to one
 * (v34hshak.c:1441) and 74's retrain tests it to choose which microstate the
 * handshake restarts in.  The object reads it with `cmpb`.
 *
 * Named `moh_active` in `struct v34_object` (`v34fsk.h`) -- kept as a
 * raw-offset macro here rather than `o->moh_active`, matching this file's own
 * convention for fields also reached elsewhere by offset.
 */
#define TX1_MOH_ACTIVE	0xabe8

/*
 * +0x25d6, +0x25d8 and +0x25da -- the three halfwords of `unmapped_25d6` that
 * 64/68 owns, reached off `0x4c(%esp)` as 0x3ba, 0x3bc and 0x3be.
 *
 * +0x25d6 is the BIT SOURCE: 64/68 shifts it right by twice `vect_idx` and
 * hands the bottom two bits to the scrambler, so it is sixteen bits read as
 * eight dibits, and the end of 64's segment reloads it with 0x899f.  +0x25d8
 * counts one per pass and ends the segment past 0x80.  +0x25da is a mode
 * word: 64 goes no further unless it holds 2, and the `role == 0x66` path
 * declines to move on when it holds 0.  No other site in this tree reads any
 * of the three.
 */
#define TX1_F25D6	0x25d6
#define TX1_F25D8	0x25d8
#define TX1_F25DA	0x25da

/*
 * +0x35a2, one halfword of `unmapped_359e`, and +0x382, which is the
 * RECEIVER's +0x11e -- `0x74(%esp)` plus 0x11e -- and lands in that
 * structure's `pad_000`, so neither is a named field anywhere in this tree.
 * 64/68 tests +0x35a2 against zero on its `role == 0x66` path and 69 tests
 * +0x382 against 0x89b0 to choose between four points and sixteen.
 */
#define TX1_F35A2	0x35a2
#define TX1_F382	0x382

/*
 * +0x3590, +0x3598 and +0x359e -- three halfwords 67 owns, and no other site
 * in this tree reads any of them.
 *
 * +0x3590 is the NEXT STUFF POINT: 67 compares `vect_idx` against it and, when
 * they meet, pushes one more bit into the reader and advances it by 0x11.
 * +0x359e counts the sequences 67 has sent, and +0x3598 is the once-only flag
 * that stops `initdigital` being called twice.  All three are inside
 * `unmapped_3564` and `unmapped_359e`.
 */
#define TX1_F3590	0x3590
#define TX1_F3598	0x3598
#define TX1_F359E	0x359e

/*
 * +0xaa3c, read as a BYTE.  `struct v34_object` names the halfword there
 * `info_caps` because the handshake reads the INFO capability nibbles out of
 * it, and v34hshak.h records that `getMPrecvdBits` stores obj+0xaa3c into
 * +0xaa6c -- so in the object's own configuration this byte and the reader's
 * `word[0]` are the same two bytes, and 0x30 bytes of record fit exactly
 * between +0xaa3c and the self-pointer at +0xaa6c.
 *
 * 67 READS BOTH SPELLINGS AND THEY ARE NOT THE SAME EXPRESSION.  0x645de
 * tests bit 0 at obj+0xaa3c; 0x647db tests bit 0 of `*(+0xaa6c)` and 0x647ef
 * sets it there.  Reached by offset here rather than as `info_caps` so that
 * the other reader's name is not asserted to be the meaning, exactly as
 * TX1_FAAE0 and TX1_FAAE2 are.
 */
#define TX1_FAA3C	0xaa3c

/*
 * +0xa244, an INT, and +0xaa80, a halfword.
 *
 * 21 reads +0xa244 three ways in one compare chain -- against its half, its
 * whole and nothing else -- and no other site in this tree reads it; it is
 * inside `unmapped_a242`.  +0xaa80 is the first halfword of `unmapped_aa80`,
 * the region whose +4 and +6 are the rate configuration's baud and 20's
 * +0xaa86; 21 stores one into it and nothing here reads it back.
 */
#define TX1_FA244	0xa244
#define TX1_FAA80	0xaa80

/*
 * +0xaa0c, the second of the five 0x30-byte message records that run from
 * +0xa94c to +0xaa3c.  `struct v34_object` names its first halfword
 * `info_rates`; v34hshak.c:1406 blanks the SAME twelve fields at +0x14
 * through +0x2c that 21's completion blanks, which is what says the two are
 * one record and not two overlapping readings of one region.  Reached by
 * offset here for the reason TX1_FAAE0 and TX1_FAA3C are.
 */
#define TX1_INFOREC	0xaa0c

/*
 * +0xaa6c and +0xa94c: the self-pointer and the record it is aimed at.
 * `v34handshakinit` aims the same pair on its mode-4 path (v34hshak.c:1451),
 * and 54's completion aims it again before handing the record to
 * `V34SetINFO0aBits`.  The harness excludes +0xaa6c from the byte comparison
 * and checks it by offset instead (finding F324).
 */
#define TX1_PTR_AA6C	0xaa6c
#define TX1_BLK_A94C	0xa94c

/*
 * +0x238 and +0x248, two of the four words of the sample-clock timer in
 * `unmapped_0234`.  `datapumpv34` reads the running count at +0x238 against
 * the mark at +0x248 and reports a stall when the span passes 287,488; 70
 * copies one onto the other, which restarts the span.  v34fsk.h's note on
 * that region is the other half of this reading.
 */
#define TX1_TIMER	0x238
#define TX1_TIMER_MARK	0x248

/*
 * +0xaa98, a short: the negotiated rate INDEX rather than a bit rate --
 * `VPcmV34GetCurrentRxBitRate` multiplies the same units by 2400.  70 copies
 * it, sign-extended, into `rate_now` and `rate_want`; v34fsk.h names those
 * two off this site and off 0x63395's read-back.
 */
#define TX1_RATEIDX	0xaa98

/*
 * The transmitted point.  `txpoint` is two shorts and every arm that sends a
 * constellation point writes them with ONE 32-bit store, which is what
 * `vect4` holds -- v34pcmmain.cpp and v34k56.cpp spell it the same way.
 *
 * This used to be a `memcpy`, which was a workaround for the C front end
 * warning about `*(int *)&o->txpoint` (then still bare, `f25d0`) where the
 * C++ one did not.  The
 * declaration carries it now: `txpoint` is a union, so the wide store has a
 * member of its own and there is nothing left to work around.
 */
static void
tx1_put_point(struct v34_object *o, int point)
{
	o->txpoint.word = point;
}

static short
tx1_get(const void *objp, unsigned off)
{
	return *(const short *)((const char *)objp + off);
}

static void
tx1_put(void *objp, unsigned off, short v)
{
	*(short *)((char *)objp + off) = v;
}

static int
tx1_get_int(const void *objp, unsigned off)
{
	return *(const int *)((const char *)objp + off);
}

static void
tx1_put_int(void *objp, unsigned off, int v)
{
	*(int *)((char *)objp + off) = v;
}

/*
 * ---------------------------------------------------------------------------
 * 65 `XMIT0`, 0x62d83.
 *
 * Send one silent symbol -- `txpoint.c[0]` and `txpoint.c[1]` are the two
 * halves of the point `txmit` transmits, and both are cleared before the
 * call -- and then,
 * if the receiver is holding bit 3 of its flags word, arm the segment:
 * raise 0x2000 in `tx_flags`, move the transmit machine to SSEG, and clear the
 * three fields the next segment counts in.
 *
 * BIT 3 IS THE ONLY THING THAT SEPARATES THE TWO PATHS and the fixture's
 * cold fill leaves it clear, so the whole body below the `if` is reached
 * only by a test that sets it.  t_v34hstx1.c drives both.
 *
 * `V34_RX_FLAG_LATE_TRN` is the receiver's own name for the bit, from
 * v34recv.h; the handshake reads the same bit and the receiver's reading of
 * it says nothing about what it means here.
 *
 * THE `txstate != SSEG` COMPARE CANNOT BE FALSE.  The arm is reached only
 * through table 1 at `txstate == 65`, `txmit` does not write +0x3596, so the
 * guard is always true and the object's branch at 0x62de3 is not reachable
 * from this entry.  It is written as the object writes it; deleting it is an
 * equivalent mutation and is recorded as one (finding F342).
 */
int
v34tx1_xmit0(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx =
		(struct v34_receiver *)((char *)objp + TX1_RECEIVER);

	o->txpoint.c[0] = 0;
	o->txpoint.c[1] = 0;
	txmit(o);

	if (rx->flags & V34_RX_FLAG_LATE_TRN) {
		o->tx_flags = (short)((unsigned short)o->tx_flags | V34_TXFLAG_SEG4A);
		hs_setstate(o, TX1_TXSTATE, V34HS_SSEG);
		o->prev_quadrant = 0;
		o->seg_symcount = 0;
		o->tx_scr_sr = 0;
	}
	return V34TX1_LOOP;
}

/*
 * ---------------------------------------------------------------------------
 * The scrambler step 71 and 86 share, 0x641de..0x6424e and 0x63db5..0x63e2e.
 *
 * Both arms carry the loop twice -- once with the 0x04000000 tap and once
 * with 0x00002000 -- and choose between the copies with `role == 0x65`.
 * That is `V34scrambler`'s `mode` argument exactly: v34rx.c hoists the same
 * branch out of the same loop, over the same register at +0x25cc, with the
 * same two generators.  `bits` is the literal 3 and `nbits` is 2, so two
 * scrambled bits come out and the register advances twice.
 *
 * WHY THE RETURN VALUE IS USABLE THOUGH THE OBJECT DOES NOT MASK.  The arms
 * store `reg >> 29` into `cur_quadrant` unmasked where `V34scrambler` returns
 * `(reg >> 29) & 3`.  The two agree for every input: the loop's last act is
 * `reg >>= 1`, so bit 31 is clear on exit and `reg >> 29` is already 0..3.
 * The masked form is used here because it is the published function; nothing
 * downstream can tell them apart.
 */
static short
tx1_scramble2(struct v34_object *o)
{
	return (short)V34scrambler((unsigned *)&o->tx_scr_sr,
				   (short)(o->role == 0x65), 3, 2);
}

/*
 * ---------------------------------------------------------------------------
 * 71 `TXLEVEL`, 0x641d1.
 *
 * One scrambler step, and then a symbol that is ALWAYS `vect4[0]`: the load
 * at 0x64257 has no index register, where 86's at 0x63e40 scales the
 * scrambler's output by four.  The quadrant still lands in `cur_quadrant`, so the
 * register and the quadrant advance while the transmitted point does not --
 * which is what a level-measurement segment wants.
 */
int
v34tx1_txlevel(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;

	o->cur_quadrant = tx1_scramble2(o);
	tx1_put_point(o, vect4[0]);
	txmit(o);
	return V34TX1_LOOP;
}

/*
 * ---------------------------------------------------------------------------
 * 78 `JaTXMIT`, 0x64139, and 85 `K56JaTXMIT`, 0x63fb0.
 *
 * Count +0xaa78 down.  On the step that reaches zero -- not before, and not
 * again afterwards, because zero is also the "do nothing" test at the top --
 * raise bit 2 of `tx_flags` and report both echo cancellers' coefficients.
 * Then hand the sample itself to the phase 3/4 half, which is where the
 * transmitting is done: neither arm calls `txmit`.
 *
 * The two differ in that call and in nothing else.  They are written as one
 * body and two entry points so that the identity is a property of the file
 * rather than a claim in a comment; finding F340 is the measurement that they
 * are still two behaviours, because the two callees are.
 *
 * AND IN THE MESSAGE, which is a third caller's evidence rather than a
 * qualification of that.  The completion block prints one line naming the
 * txstate it completed in -- `JaTXMIT`, `K56JaTXMIT` and `J1TXMIT` at
 * 0x6821d, 0x68282 and 0x682d6 -- and 64 shares this countdown too
 * (0x635cc, whose completion at 0x6533c falls back into the body).  Three
 * identical blocks with one string each is what a caller-supplied message
 * looks like from the outside, so it is passed rather than branched on.
 *
 * THE COUNTER IS SIXTEEN BITS.  The object loads it with `movzwl`, tests
 * `%ax`, and stores `%ax` back, so a negative `short` counts down through
 * 0x8000 rather than through zero.  Spelled with an `unsigned short` here
 * for that reason.
 */
static int
tx1_ja_common(struct v34_object *o, const char *msg)
{
	unsigned short c = (unsigned short)tx1_get(o, TX1_COUNT);

	if (c == 0)
		return 0;

	c = (unsigned short)(c - 1);
	tx1_put(o, TX1_COUNT, (short)c);
	if (c != 0)
		return 0;

	/*
	 * The WHOLE message, not a name spliced into one format: the three
	 * sites hold three complete literals in `.rodata.str1.4` and push one
	 * argument each, which is what constant propagation into an inlined
	 * static leaves behind and a `%s` would not.
	 */
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf(msg);
	v34FreezeEcho(o);
	return 1;
}

int
v34tx1_jatxmit(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;

	(void)tx1_ja_common(o, "V34Hshak: on JaTXMIT - time to freeze"
				   " echo...\r\n");		/* 0x6821d */
	v90Phase34(o);
	return V34TX1_LOOP;
}

int
v34tx1_k56jatxmit(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;

	(void)tx1_ja_common(o, "V34Hshak: on K56JaTXMIT - time to freeze"
				   " echo...\r\n");		/* 0x68282 */
	(void)k56FlexPhase34(o);
	return V34TX1_LOOP;
}

/*
 * ---------------------------------------------------------------------------
 * 81 `MOH_SILENCE`, 0x63d58 -- the table gives the same target to txstates
 * 81, 82, 83 and 84.
 *
 * Four zero samples into the transmit queue and one tick of `vect_idx`.
 * `txwritequeue` takes four shorts and the object hands it eight bytes of
 * stack it has just zeroed as two `int`s, which is the same four samples.
 *
 * At 0xc0 the arm leaves for 0x66d85, and that block is written here now.
 * The count is compared as sixteen bits, after the store.
 *
 * THE `txstate == 81` COMPARE AT 0x66d8e IS WHAT MAKES FOUR TXSTATES TWO
 * BEHAVIOURS, AND IT IS LIVE.  Everything above the wrap is one body under
 * four indices; 0x66d85 re-reads +0x3596 and returns to the loop test at
 * 0x63941 for anything that is not 0x51, so 82, 83 and 84 reach the
 * hundred-and-ninety-second sample and simply carry on, and only 81 decides
 * anything.  That is finding F423's shape at 0x635cc -- most passes are one
 * body under several indices and the wrap is two behaviours -- and it is why
 * docs/v34handshak.md's "0x63d58 ... really is one behaviour" was wrong.
 *
 * WHAT 81 DECIDES.  The silence is over, and the transmit machine moves to
 * MOH_FRR when either the Modem-on-Hold message this end originally asked
 * for (+0xabec) or the one it is building now (`moh_message`) is 1 MHfrr,
 * and to MOH_ON_HOLD otherwise.  BOTH ARE READ 32 BITS WIDE -- `cmpl $0x1`
 * at 0x66dac and 0x66db9 -- and both are printed with `%d` by the
 * diagnostic in front of them, which is where +0xabec's name comes from.
 *
 * The compares against 0x52 at 0x66dc6 and 0x53 at 0x68ae3 are
 * `hs_setstate`'s own "already there" early return inlined, not a fifth
 * state test: `jne 63941` has established that the halfword holds 0x51, so
 * neither can be true.  Findings F722 and F730's shape.
 *
 * Both of the block's ways out are the loop test -- 0x63941 loads the object
 * first and 0x63948 does not, and both fall into 0x629e7 -- so the wrap is
 * `V34TX1_LOOP` on either side of the guard and nothing leaves the dispatch.
 */
int
v34tx1_moh_silence(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	short quiet[4];

	quiet[0] = 0;
	quiet[1] = 0;
	quiet[2] = 0;
	quiet[3] = 0;
	txwritequeue(&o->txq, quiet);

	o->vect_idx = (short)(o->vect_idx + 1);
	if ((unsigned short)o->vect_idx != 0xc0u)
		return V34TX1_LOOP;			/* 0x63da2 */

	/* 0x66d85 */
	if (tx1_get(o, TX1_TXSTATE) != V34HS_MOH_SILENCE)
		return V34TX1_LOOP;			/* 0x63941 */

	if (dsplibs_debug_level > 1)			/* 0x66d98 */
		dsplibs_debug_printf(
			"V34F MOH: After 192 silence, org = %d ," " act = %d\r\n",
			o->moh_org, o->moh_message);

	/* 0x66dac */
	hs_setstate(o, TX1_TXSTATE,
		    (o->moh_org == 1 || o->moh_message == 1)
		    ? V34HS_MOH_FRR : V34HS_MOH_ON_HOLD);

	o->vect_idx = 0;				/* 0x66dec */
	return V34TX1_LOOP;				/* 0x63948 */
}

/*
 * ---------------------------------------------------------------------------
 * 86 `TXMD`, 0x63dae.
 *
 * Scramble two bits, transmit the point they select, and count the symbol.
 * Then two comparisons of `vect_idx`, in this order:
 *
 *   - against +0xaa78: the MD segment is over, echo adaptation is enabled,
 *     and the arm falls straight into the second comparison;
 *   - against +0x35a6: the modulator is reconfigured for the negotiated rate
 *     and the transmit machine moves to SSEG.
 *
 * THE TWO ARE NOT EXCLUSIVE AND THE FIRST IS NOT AN EXIT.  0x66fe9's block
 * ends `jmp 63e7f`, and 0x63e7f is the fall-through of the very block that
 * jumped to it -- the +0x35a6 test.  So the object runs both bodies when
 * +0xaa78 and +0x35a6 hold the same value, and the arm has ONE way out.
 * This file used to report 0x66fe9 as a transfer (`V34TX1_TXMD_DONE`) and
 * stop; the transfer does not exist.
 *
 * WHAT 0x66fe9 DOES is clear bit 2 of `tx_flags` and zero `echo_calls`, `echo_alpha` and
 * `far_echo_alpha` -- the four that gate and seed `adaptecho`'s slow path.  The same
 * four are cleared at 0x67613 in 21 `TRNSEG4`, which ALSO zeroes `echo_energy`
 * and calls `VPcmV34ReportStartOfEchoAdapt`; the two are near-twins and not
 * one body, so they are written twice rather than factored.  The
 * reconstruction's own diagnostic names what it is for: "On MD - enabling
 * echo adaptation".
 *
 * `V34SetupModulator`'s fifth argument is one when EITHER of the two PCM
 * receivers at +0x24c and +0x250 is non-zero -- the object computes it as
 * two tests jumping to one `mov $0x1,%eax` at 0x65696 -- and its sixth is the
 * literal zero.  The three rate fields are the transmit half of the rate
 * configuration at +0xaa84: `baud`, `carrier` and the pre-emphasis index at
 * +0x06, which is the same triple v34hshak.c:522 reads.
 *
 * THE ARM ENDS AT 0x6430c, WHICH IS SHARED, and its one instruction --
 * `seg_symcount += 1` -- is written here rather than in the caller because it is
 * the last thing this arm does on both of its in-loop paths.  Another table-1
 * arm reaching 0x6430c will want the same line; the eventual `v34handshak`
 * may factor it.
 */
int
v34tx1_txmd(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	short q = tx1_scramble2(o);

	o->cur_quadrant = q;
	tx1_put_point(o, vect4[q]);
	txmit(o);

	o->vect_idx = (short)(o->vect_idx + 1);
	if ((unsigned short)o->vect_idx
	    == (unsigned short)tx1_get(o, TX1_COUNT)) {
		/* 0x66fe9 */
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
				"On MD - enabling echo adaptation...\r\n");

		o->echo_calls = 0;				/* 0x6700e */
		o->tx_flags =				/* 0x67017 */
			(short)((unsigned short)o->tx_flags & ~V34_EC_FROZEN);
		o->echo_alpha = 0;				/* 0x6701e */
		o->far_echo_alpha = 0;				/* 0x67025 */
	}

	if ((unsigned short)o->vect_idx
	    == (unsigned short)tx1_get(o, TX1_SEGLEN)) {
		struct v34_ratecfg *cfg =
			(struct v34_ratecfg *)((char *)o + V34_RATECFG);
		int pcm = (o->v90_receiver != 0 || o->k56flex_receiver != 0);

		if (dsplibs_debug_level > 1)		/* 0x6800d */
			dsplibs_debug_printf(
				"TX: Done with MD, moving to S/Sbar" " again...\r\n");

		V34SetupModulator((struct v34_modulator *)
				  ((char *)o + TX1_MODULATOR),
				  cfg->baud, cfg->carrier, cfg->preemp, pcm, 0);

		hs_setstate(o, TX1_TXSTATE, V34HS_SSEG);
		o->seg_symcount = 0;
		o->tx_scr_sr = 0;
		o->tx_flags = (short)((unsigned short)o->tx_flags
				     | (V34_TXFLAG_PPSEG | V34_EC_FROZEN));
	}

	/* 0x6430c */
	o->seg_symcount = (short)(o->seg_symcount + 1);
	return V34TX1_LOOP;
}

/*
 * ---------------------------------------------------------------------------
 * 60 `TONE_AB`, 0x62d3d.  Thirty-four bytes, and the smallest arm the table
 * has: one point, and the shared `txmit` at 0x62d5f that it falls into.
 *
 * THE INDEX IS SCALED BY EIGHT OVER A TABLE OF FOUR-BYTE ENTRIES, so it
 * selects entry 0 or entry 2 and not 0 or 1:
 *
 *     62d48  movswl 0x358c(%ebx),%edi
 *     62d4f  and    $0x1,%edi
 *     62d52  mov    0x0(,%edi,8),%edx      <== R_386_32 vect4
 *
 * `vect4` is in the order (+,+) (+,-) (-,-) (-,+) -- clockwise, v34hshak.c --
 * so entries 0 and 2 are the two ENDS of a diagonal and the two points this
 * sends are exact negations of one another.  A tone alternating between them
 * is A or B depending on which the flag picks, which is the state's name.
 *
 * The load is `movswl` and the mask is one bit, so the sign extension cannot
 * change the answer; reading it as `movzwl` is an equivalent mutation and is
 * recorded as one.
 */
int
v34tx1_tone_ab(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	int sel = tx1_get(o, TX1_F358C) & 1;

	tx1_put_point(o, vect4[2 * sel]);
	txmit(o);
	return V34TX1_LOOP;
}

/*
 * ---------------------------------------------------------------------------
 * 18 `SSEG`, 0x64048, continuing at 0x66d11.
 *
 * TWO symbols per pass of the loop, not one: `vect4[0]` then `vect4[3]`, each
 * with its own `txmit`, which is the only arm in this file that transmits
 * twice.  Then one tick of `seg_symcount`, the segment's symbol count, and at 0x40
 * the segment is over.
 *
 * `vect4[3]` is the (-,+) point and `vect4[0]` is (+,+), so the pair is one
 * step anticlockwise; alternating the two is a half-rate square wave on the
 * imaginary axis, which is what the S segment is.
 *
 * THE COUNT IS COMPARED AS SIXTEEN BITS AFTER THE STORE.  The object loads it
 * with `movzwl`, increments the 32-bit register, compares `%bp` against 0x40
 * and stores `%bp` back, so the wrap is at 0x10000 and the comparison sees
 * the stored value.
 *
 * THE `txstate != SBARSEG` COMPARE AT 0x66d1f CANNOT BE FALSE, for finding
 * F342's reason at 65: this arm is reached only through table 1 at
 * `txstate == 18`, and `txmit` does not write +0x3596 (finding F285's sweep).
 * It is written as the object writes it and deleting it is an equivalent
 * mutation.
 *
 * 0x66d11 IS NOT SHARED.  It is reached from 0x64094 and from nowhere else,
 * and it rejoins the loop at 0x6409a like the counting path, so this arm has
 * one exit and no transfer out.
 */
int
v34tx1_sseg(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	unsigned short n;

	tx1_put_point(o, vect4[0]);
	txmit(o);
	tx1_put_point(o, vect4[3]);
	txmit(o);

	n = (unsigned short)((unsigned short)o->seg_symcount + 1);
	o->seg_symcount = (short)n;
	if (n == 0x40) {
		/* 0x66d11 */
		hs_setstate(o, TX1_TXSTATE, V34HS_SBARSEG);
		o->seg_symcount = 0;
	}
	return V34TX1_LOOP;
}

/*
 * ---------------------------------------------------------------------------
 * 70 `DATAXMIT`, 0x63ca8.
 *
 * `modulatevector` does all of the transmitting -- it maps one point out of
 * the vector it holds and tail-calls `txmit` -- and the rest of the arm is
 * conditional on bit 4 of `tx_flags`.
 *
 * That bit is the mapper's own "the data path is on" flag: v34shell.c raises
 * it inside `modulatevector`, on the pass that regenerates the vector, when
 * the training-to-data counter reaches the span the transmit shell carries.
 * Nothing here lowers it, so the body below runs on every pass once it is up.
 * The object does not need it to be one-shot: four of the five words it
 * writes are idempotent and the fifth, +0x2218, is what the once-per-block
 * half reads to move on.
 *
 * WHAT IT WRITES, with the register convention applied -- `0x74(%esp)` is the
 * receiver at +0x264 and `0x78(%esp)` is the object PLUS FOUR, so `0x224(%ecx)`
 * is +0x228 and not +0x224:
 *
 *   receiver +0x220 = 0, +0x21c = 0   the receiver's own two counters
 *   +0x248 = +0x238                   restart the sample-clock span
 *   rate_now = rate_want = +0xaa98    the negotiated rate index, SIGN
 *                                     EXTENDED from a short into two ints
 *   +0x2218 = 1
 *
 * The sign extension is the one thing here a fill can hide: +0xaa98 is a rate
 * index and small, so a `movzwl` reading of it agrees on every non-negative
 * value.  `t_v34hstx1.c` drives it negative for that reason.
 *
 * The int at +0x2218 is read at 0x63d01 as well as written, but only to skip
 * the diagnostic at 0x63d1a: at `dsplibs_debug_level` 0 the whole block is
 * dead and the store of 1 happens either way.  Finding F341's gap, again.
 */
int
v34tx1_dataxmit(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx =
		(struct v34_receiver *)((char *)objp + TX1_RECEIVER);
	int idx;

	modulatevector(o);

	if (!(o->tx_flags & PROG_TXBIT_DATA))
		return V34TX1_LOOP;

	rx->equerr_accum = 0;
	rx->err_symcount = 0;
	tx1_put_int(o, TX1_TIMER_MARK, tx1_get_int(o, TX1_TIMER));

	idx = tx1_get(o, TX1_RATEIDX);
	o->rate_now = idx;
	o->rate_want = idx;

	tx1_put_int(o, TX1_HS_MODE, 1);
	return V34TX1_LOOP;
}

/*
 * ---------------------------------------------------------------------------
 * 51 `TX_L1`, 0x62c69, with the second copy of its loop at 0x65290.
 *
 * Four samples of the line probe into the transmit queue, and then -- but
 * only when the MICROSTATE is also 51 -- the end of the segment.
 *
 * THE OBJECT CARRIES ITS LOOP TWICE AND CHOOSES ON `microstate == TX_L1`,
 * which is the same shape as 71 and 86's two scrambler generators.  The two
 * copies differ in one instruction:
 *
 *     62cab  movswl probe[i],%eax          ; and then imul, sar
 *     652ae  movswl probe[i],%eax
 *     652be  add    %eax,%eax              ; L1 sends it at twice the level
 *     652c0  cwtl                          ;  ... through a short
 *
 * so the second copy doubles the sample.  `cwtl` truncates the doubled value
 * to sixteen bits before the multiply, and for THIS table it can never bite:
 * the largest magnitude in `probe` is 13,317 and twice that is 26,634, which
 * a short holds.  So `(short)(2 * v)` and `2 * v` are indistinguishable here
 * and a mutation of the truncation is equivalent -- a property of the table's
 * values, not of the code.  It is written as the object writes it.
 *
 * THE SCALE IS SIGNED AND THE SHIFT IS ARITHMETIC.  `tx_scale` comes in through
 * `movswl` and the product is closed with `sar $0xe`; `probe` is half
 * negative, so both readings are exercised by any run at all -- which is why
 * neither needs a case of its own and a `movzwl` or `shr` mutation dies on
 * the first sample.
 *
 * `vect_idx` IS RE-READ FROM THE OBJECT EVERY ITERATION, incremented, and
 * stored back before the next read.  Written that way rather than as a local
 * because that is what the object does; nothing here aliases it, so the two
 * are equivalent, and the equivalence is the sort of thing that stops being
 * true when somebody puts a call inside the loop.
 *
 * AND THE SEGMENT'S END IS GUARDED TWICE.  The microstate is tested again
 * after `txwritequeue` -- 0x62cf5, the identical compare -- so an arm that
 * ran the un-doubled loop cannot reach the end at all.  The end itself is
 * `vect_idx == 0x600` exactly, tested as sixteen bits after the four
 * increments; it moves the microstate to `TX_L2` and zeroes `vect_idx`
 * through 0x62d32, which is shared with other arms of this table.
 */
int
v34tx1_tx_l1(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	int doubled = tx1_get(o, TX1_MICROSTATE) == V34HS_TX_L1;
	short buf[4];
	int i;

	for (i = 0; i < 4; i++) {
		unsigned idx = (unsigned short)o->vect_idx;
		int v = probe[idx & 0x3f];

		if (doubled)
			v = (short)(2 * v);
		buf[i] = (short)((v * o->tx_scale) >> 14);
		o->vect_idx = (short)(idx + 1);
	}

	txwritequeue(&o->txq, buf);

	if (tx1_get(o, TX1_MICROSTATE) != V34HS_TX_L1)
		return V34TX1_LOOP;
	if ((unsigned short)o->vect_idx != 0x600u)
		return V34TX1_LOOP;

	hs_setstate(o, TX1_MICROSTATE, V34HS_TX_L2);
	/* 0x62d32 */
	o->vect_idx = 0;
	return V34TX1_LOOP;
}

/*
 * ---------------------------------------------------------------------------
 * 19 `SBARSEG`, 0x6296d, with the segment's end at 0x671f0 and its four ways
 * out at 0x67236, 0x68375, 0x69eca and 0x6783e.
 *
 * TWO SYMBOLS PER PASS, `vect4[2]` then `vect4[1]`, each with its own `txmit`
 * -- 18 `SSEG`'s shape with a different pair.  `vect4` is (+,+) (+,-) (-,-)
 * (-,+), so 18 sends (+,+) then (-,+) and 19 sends (-,-) then (+,-): the S
 * segment and the S-bar segment are the same alternation with both points
 * negated, which is what the bar in the name is.
 *
 * The segment is eight symbol PAIRS long, `seg_symcount` counting them, and the
 * count is compared as sixteen bits BEFORE the store -- the object stores
 * `%ax` only on the path that does not reach eight, and the completing path
 * clears the field at 0x671f2 instead.  So a reconstruction that stored first
 * and then compared would leave 8 behind where the object leaves 0.
 *
 * FOUR WAYS OUT OF THE COMPLETION, tested in this order and no other:
 *
 *   tx_flags & 0x2000      -> TRNSEG4A                          (0x67209)
 *   short_35a4 == 0          -> PPSEG                             (0x68375)
 *   tx_flags & 0x8000      -> PPSEG, and seg_symcount = TX1_SEGLEN first
 *                          (0x69eca)
 *   otherwise           -> TXMD, after TX1_SEGLEN = short_35a4 * 0x53,
 *                          the modulator and the counter     (0x6783e)
 *
 * and all four converge on 0x67236, which zeroes `vect_idx` and +0x358e.
 * The first test is `test $0x20,%dh` on the halfword loaded at 0x671f9, which
 * is bit 13 and not bit 5; the third is `test %dx,%dx` / `js` on the SAME
 * register, so it is bit 15 of `tx_flags` read as a sign.
 *
 * THE MODULATOR'S SIX ARGUMENTS ARE ALL LITERALS -- 4800 baud, 2400 carrier,
 * no pre-emphasis, `v90` zero, no reset (0x67890..0x678be).  There is no rate
 * configuration to seed and finding F216's 3200-baud discipline has nothing to
 * bite on here: `v90` is a constant zero, so the one rate that reads it is not
 * the rate this arm asks for.  4800 is a real case in `V34SetupModulator` --
 * `txAllPass` at 0x20 taps -- and it is the one case that leaves `prem` NULL,
 * so `m->ec_prem` is not written.
 *
 * THE COUNTER AT +0xaa78 IS A TIME, and the object computes it in fixed point
 * without touching the FPU:
 *
 *     67878..678db   esi = (0x5e8 - good_run) << 14
 *     678dd..678e7   the 0x1b4e81b5 / `sar $0xa` / `sub` sequence, which is
 *                    signed division by 9600 truncating toward zero
 *     678e9          movswl %dx  -- the quotient is TRUNCATED TO A SHORT
 *     678ec..67909   * 0x960, >> 14, + 0x96
 *
 * so it is `(1512 - good_run) * 2400 / 9600 + 150` carried through a sixteen-bit
 * intermediate.  1512 and 9600 are sample counts at 9600 Hz; the truncation is
 * reachable, because the quotient leaves a short once `1512 - good_run` passes
 * 19,200, and `t_v34hstx1.c` drives it there.
 *
 * ONE STORE IS NOT MODELLED AND CANNOT BE.  0x6784d spills 0x53 to
 * `0x48(%esp)`, a slot `v34handshak` reuses in nineteen other places and that
 * nothing on this path reads again.  It is stack rather than object state, so
 * no comparison here can see it.
 */
int
v34tx1_sbarseg(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	unsigned short n;

	tx1_put_point(o, vect4[2]);
	txmit(o);
	tx1_put_point(o, vect4[1]);
	txmit(o);

	n = (unsigned short)((unsigned short)o->seg_symcount + 1);
	if (n != 8) {
		o->seg_symcount = (short)n;
		return V34TX1_LOOP;		/* 0x629c8 */
	}

	/* 0x671f0 */
	o->seg_symcount = 0;
	if ((unsigned short)o->tx_flags & V34_TXFLAG_SEG4A) {
		hs_setstate(o, TX1_TXSTATE, V34HS_TRNSEG4A);
	} else if (tx1_get(o, TX1_F35A4) == 0) {
		/* 0x68375 */
		hs_setstate(o, TX1_TXSTATE, V34HS_PPSEG);
	} else if ((unsigned short)o->tx_flags & V34_TXFLAG_PPSEG) {
		/* 0x69eca */
		o->seg_symcount = tx1_get(o, TX1_SEGLEN);
		hs_setstate(o, TX1_TXSTATE, V34HS_PPSEG);
	} else {
		/* 0x6783e */
		int span;
		short q;

		tx1_put(o, TX1_SEGLEN,
			(short)((unsigned short)tx1_get(o, TX1_F35A4) * 0x53));
		hs_setstate(o, TX1_TXSTATE, V34HS_TXMD);

		/* 0x67885 */
		V34SetupModulator((struct v34_modulator *)
				  ((char *)o + TX1_MODULATOR),
				  4800, 2400, 0, 0, 0);

		/*
		 * `(0x5e8 - good_run) << 14` with the subtraction done in an int.
		 * Spelled through `unsigned` because the left shift of a
		 * negative int is undefined in C and the object's `shl` is
		 * not; the bits are the same either way.
		 */
		span = (int)((unsigned)(0x5e8 - o->dmadelay) << 14);
		q = (short)(span / 9600);
		tx1_put(o, TX1_COUNT, (short)((((int)q * 0x960) >> 14) + 0x96));
		if (dsplibs_debug_level > 1)		/* 0x67916 */
			dsplibs_debug_printf(
				"Moving to TX MD, would take %d symbols ,"
				" echo start delay is %d...\r\n",
				tx1_get(o, TX1_SEGLEN),
				tx1_get(o, TX1_COUNT));
	}

	/* 0x67236 */
	o->vect_idx = 0;
	tx1_put(o, TX1_F358E, 0);
	return V34TX1_LOOP;			/* 0x63948 */
}

/*
 * ---------------------------------------------------------------------------
 * 20 `PPSEG`, 0x642bf, continuing at 0x66d57 and 0x680ac.
 *
 * ONE POINT OF `vectpp` PER PASS, and the index is `vect_idx` itself rather
 * than a mask: `movswl 0x2aa2(%ebp),%esi` then `mov 0x0(,%esi,4),%edx`, so the
 * table is read as FORTY-EIGHT FOUR-BYTE ENTRIES where v34rx.c reads the same
 * bytes as ninety-six shorts.  That is the same (re, im)-in-one-int packing
 * `vect4` has, and it is why `vectpp` is global in the object.
 *
 * The index is SIGN-EXTENDED and not bounded, so the arm is only safe for
 * `vect_idx` in 0..47; the object gets there by clearing the field at 0x66d76
 * every time it reaches 48, and nothing else in table 1 leaves PPSEG entered
 * with a larger one.
 *
 * THREE NESTED COUNTERS, and the innermost is the one the loop rejoins with:
 *
 *   vect_idx  0..0x30    one pass of the PP sequence
 *   +0x358e   0..6       six passes make the segment
 *   seg_symcount                bumped at 0x6430c on the two paths that do NOT end
 *                        the segment, and NOT on the one that does
 *
 * That last asymmetry is the arm's shape.  0x6430c is the shared block 86
 * also rejoins through (finding F340), and the segment's end leaves through
 * 0x63da2 instead, which writes nothing -- so a reconstruction that bumped
 * seg_symcount on every path is wrong on exactly one of the three.
 *
 * WHAT THE SEGMENT'S END DOES, at 0x680ac.  `vect_idx` is reloaded from `rtd`
 * plus 0x90 -- not cleared -- and then +0xaa86 is set from a SCALED COPY of
 * that same value, chosen by the rate configuration's baud at +0xaa84:
 *
 *     2400  >> 2          3000  * 0x1400 >> 14      3429  * 0x16dc >> 14
 *     2800  * 0x12ab >> 14        3200  * 0x1555 >> 14
 *
 * which are `baud / 9600` in Q14 to within a count: 0x1000, 0x12ab, 0x1400,
 * 0x1555 and 0x16dc over 0x4000 are 0.25, 0.2917, 0.3125, 0.3333 and 0.3572
 * against 2400, 2800, 3000, 3200 and 3429 over 9600.  Any other baud leaves
 * +0xaa86 alone, and that is a real arm and not an oversight: the field is
 * still read three instructions later.
 *
 * THE COUNTER AT +0xaa78 IS 19'S ARITHMETIC WITH THE BAUD IN PLACE OF THE
 * SHIFT.  19 computes `(0x5e8 - good_run) << 14` and this computes
 * `(0x5e8 - good_run) * baud`, both divided by 9600 through the same
 * 0x1b4e81b5 / `sar $0xa` sequence -- signed, truncating toward zero.  Then
 * three terms are added: seg_symcount, +0xaa7c, and a literal one, plus `rtd` again
 * when `role == 0x65`.  The quotient is NOT truncated to a short here, where
 * 19 truncates it; the object stores it through `%dx` but keeps the 32-bit
 * value in `%edx` for the sum, which is the difference.
 *
 * ONE STORE IS OVERWRITTEN ON BOTH PATHS.  0x6818d puts the raw quotient in
 * +0xaa78 and 0x681bf or 0x69277 replaces it before anything reads it.  It is
 * written here as the object writes it and a mutation deleting it is
 * equivalent, recorded as one.
 */
int
v34tx1_ppseg(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	int i = o->vect_idx;			/* movswl 0x2aa2 */
	int point;

	memcpy(&point, &vectpp[2 * i], sizeof(point));
	o->vect_idx = (short)(i + 1);
	tx1_put_point(o, point);
	txmit(o);

	if ((unsigned short)o->vect_idx == (unsigned short)V34_VECTPP_POINTS) {
		/* 0x66d57 */
		unsigned short n = (unsigned short)
			((unsigned short)tx1_get(o, TX1_F358E) + 1);

		if (n != 6) {
			tx1_put(o, TX1_F358E, (short)n);
			o->vect_idx = 0;
		} else {
			/* 0x680ac */
			struct v34_ratecfg *cfg = (struct v34_ratecfg *)
						  ((char *)o + V34_RATECFG);
			short v = (short)((unsigned short)o->rtd + 0x90);
			short baud = cfg->baud;
			int span, q, acc;

			o->vect_idx = v;
			switch ((unsigned short)baud) {
			case 2400:
				tx1_put(o, TX1_FAA86, (short)(v >> 2));
				break;
			case 2800:
				tx1_put(o, TX1_FAA86,
					(short)(((int)v * 0x12ab) >> 14));
				break;
			case 3000:
				tx1_put(o, TX1_FAA86,
					(short)(((int)v * 0x1400) >> 14));
				break;
			case 3200:
				tx1_put(o, TX1_FAA86,
					(short)(((int)v * 0x1555) >> 14));
				break;
			case 3429:
				tx1_put(o, TX1_FAA86,
					(short)(((int)v * 0x16dc) >> 14));
				break;
			default:
				break;
			}

			/* 0x680fd */
			tx1_put(o, TX1_F358E, 0);
			tx1_put(o, TX1_FAA86,
				(short)((unsigned short)tx1_get(o, TX1_FAA86)
					+ 0x120));
			hs_setstate(o, TX1_TXSTATE, V34HS_TRNSEG4);

			/* 0x68154 */
			span = (0x5e8 - o->dmadelay) * (int)baud;
			q = span / 9600;
			tx1_put(o, TX1_COUNT, (short)q);	/* 0x6818d */

			acc = q + (unsigned short)tx1_get(o, TX1_FAA7C)
				+ (unsigned short)o->seg_symcount + 1;
			if (o->role == 0x65)			/* 0x6926d */
				acc += (unsigned short)o->rtd;
			tx1_put(o, TX1_COUNT, (short)acc);

			/* 0x681c6 */
			tx1_put(o, TX1_FAA86,
				(short)((unsigned short)tx1_get(o, TX1_FAA86)
					+ (unsigned short)tx1_get(o, TX1_COUNT)));
			if (dsplibs_debug_level > 1)	/* 0x681ef */
				dsplibs_debug_printf(
					"V34Hshak: echo start wait time would" " be: NEC %d symbols, FEC %d"
					" symbols...\r\n",
					tx1_get(o, TX1_COUNT),
					tx1_get(o, TX1_FAA86));
			return V34TX1_LOOP;		/* 0x63da2 */
		}
	}

	/* 0x6430c */
	o->seg_symcount = (short)((unsigned short)o->seg_symcount + 1);
	return V34TX1_LOOP;			/* 0x6431f */
}

/*
 * ---------------------------------------------------------------------------
 * 5 `SILENCE`, 54 `SILENCEINFO` and 74 `SILENCERETRAIN`, 0x640b4.
 *
 * ONE TABLE ENTRY AND THREE BEHAVIOURS, which is not what one entry usually
 * means here.  `.rodata+0x2da0` gives indices 0, 49 and 69 -- txstates 5, 54
 * and 74 -- the identical address, and the shared prologue then RE-READS
 * `txstate` at 0x640f4 and branches on it:
 *
 *     0x4a  74  -> 0x66b87, the retrain
 *     0x36  54  -> 0x6410b, the INFO0a countdown
 *     else   5  -> 0x6409a, the loop, having done the prologue and no more
 *
 * So the three are one entry, one prologue and three tails, and a test that
 * drove only one of them would be testing a third of the arm.  The shared
 * prologue is four zero samples into the transmit queue and one clear of
 * +0xaa7a -- 81 `MOH_SILENCE`'s shape, with the extra clear and without the
 * `vect_idx` tick.
 *
 * They are written as one function with the read inside it, rather than as
 * three entry points, because the read IS the object: 78 and 85 above are two
 * entries with one body and this is one entry with one body, and the
 * difference between those two shapes is worth keeping visible.
 *
 * ---------------------------------------------------------------------------
 * 54, at 0x6410b.  `vect_idx` counts ten silent blocks and then, at 0x6858f:
 * clear it, move the transmit machine to TX_DPSK, aim the self-pointer at
 * +0xa94c, and hand that record to `V34SetINFO0aBits` -- which fills its
 * first three shorts -- before writing eleven more fields of it directly.
 *
 * ONE OF THOSE ELEVEN IS CONDITIONAL and the rest are not: +0x18 is 0x1e when
 * `role == 0x65` AND `v90_receiver` is non-zero (0x69fcf), and 0x11
 * otherwise (0x685eb).  The two blocks are otherwise the same twelve stores
 * in the same order and rejoin at 0x68616, which is why they are one body
 * here.  `v90_receiver` is read as `*(int *)(obj+0x24c)` off `0x78(%esp)`,
 * which is the OBJECT PLUS FOUR -- it is that field alone and not the
 * `+0x24c || +0x250` pair 86 computes.
 *
 * ---------------------------------------------------------------------------
 * 74, at 0x66b87.  `vect_idx` counts 0xb4 silent blocks -- and is STORED
 * BEFORE the comparison, where 19's count is stored only on the path that
 * does not complete -- and then restarts the handshake: microstate, rxstate
 * and txstate all move, `vect_idx`, +0x358c and +0xaa78 are cleared, and
 * +0xaae2 and +0xaae0 are set to -1 and 0.
 *
 * WHICH MICROSTATE IT RESTARTS IN IS THE ONE THREE-WAY CHOICE.  +0xabe8
 * non-zero -- `v34handshakinit`'s Modem-on-Hold flag -- gives MOH_TONE;
 * otherwise the object computes it arithmetically at 0x68a99:
 *
 *     sete %bl ; movzbl %bl,%eax ; dec %eax ; and $0xfffffff4,%eax
 *     lea 0x3a(%eax),%ebx
 *
 * from `role == 0x65`, which is 0x3a when the compare held and 0x2e when it
 * did not: RX_PHASE1_CALL for the originating side and TX_PHASE1_ANS for the
 * answering one, which is the same `role` reading probeselect uses.
 *
 * THE txstate COMPARE AT 0x66c10 CANNOT BE FALSE and is written as the object
 * writes it: it tests the value read at 0x640f4, which is 0x4a on this path
 * by construction, against 0x3c.  Nothing between the two writes +0x3596, so
 * comparing the saved value and re-reading the field are the same thing here;
 * deleting the compare is an equivalent mutation and is recorded as one.
 */
int
v34tx1_silence(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx =
		(struct v34_receiver *)((char *)objp + TX1_RECEIVER);
	short quiet[4];
	short txst;
	unsigned short n;

	quiet[0] = 0;
	quiet[1] = 0;
	quiet[2] = 0;
	quiet[3] = 0;
	tx1_put(o, TX1_FAA7A, 0);
	txwritequeue(&o->txq, quiet);

	txst = tx1_get(o, TX1_TXSTATE);

	if (txst == V34HS_SILENCERETRAIN) {
		/* 0x66b87 */
		short want;

		n = (unsigned short)((unsigned short)o->vect_idx + 1);
		o->vect_idx = (short)n;
		if (n != 0xb4)
			return V34TX1_LOOP;		/* 0x63da2 */

		/* 0x66ba1 */
		if (*((unsigned char *)o + TX1_MOH_ACTIVE) != 0)
			want = V34HS_MOH_TONE;
		else					/* 0x68a7c */
			want = o->role == 0x65 ? V34HS_RX_PHASE1_CALL
						: V34HS_TX_PHASE1_ANS;
		hs_setstate(o, TX1_MICROSTATE, want);

		/* 0x66be0 */
		hs_setstate(o, TX1_RXSTATE, V34HS_RX_DPSK);
		/* 0x66c10 */
		hs_setstate(o, TX1_TXSTATE, V34HS_TONE_AB);

		/* 0x66c32 */
		tx1_put(o, TX1_F358C, 0);
		o->vect_idx = 0;
		tx1_put(o, TX1_COUNT, 0);
		tx1_put(o, TX1_FAAE2, -1);
		tx1_put(o, TX1_FAAE0, 0);
		if (dsplibs_debug_level > 1)		/* 0x66c72 */
			dsplibs_debug_printf(
				"V34RETRAIN, SILENCERETRAIN finished," " rx->rxflgs,=0x%x,rx->gain=0x%x,"
				"gainestimate=0x%x\n",
				rx->flags, rx->agc_gain, rx->agc_start_gain);
		return V34TX1_LOOP;			/* 0x63941 */
	}

	if (txst != V34HS_SILENCEINFO)
		return V34TX1_LOOP;			/* 0x6409a */

	/*
	 * 0x6410b.  THE COUNT IS STORED ON BOTH PATHS, and the transcript is
	 * what says so: the blob's `SILENCEINFO=>TX_DPSK` line prints `[1]10`,
	 * and `[1]` is `vect_idx` read at the moment of the print.  Zeroing
	 * before the state change -- which is how this was written until
	 * finding F573 -- prints `[1]0` instead.  Both orders leave the same
	 * bytes behind, so no byte comparison can separate them and none did
	 * for six batches; only turning the diagnostics on does.  74 stores
	 * its count before its own comparison in exactly this shape, and
	 * mutation 181 has asserted that since it landed.
	 */
	n = (unsigned short)((unsigned short)o->vect_idx + 1);
	o->vect_idx = (short)n;
	if (n != 0xa)
		return V34TX1_LOOP;			/* 0x629cf */

	/* 0x6858f */
	hs_setstate(o, TX1_TXSTATE, V34HS_TX_DPSK);
	o->vect_idx = 0;
	*(short **)((char *)o + TX1_PTR_AA6C) =
		(short *)((char *)o + TX1_BLK_A94C);
	V34SetINFO0aBits(o, (short *)((char *)o + TX1_BLK_A94C));

	{
		/*
		 * RE-READ, not the pointer just stored, because that is what
		 * the object does at 0x685f2 and 0x69fe1 -- the same reading
		 * v34hshak.c:1459 records for `v34handshakinit`'s copy of
		 * this sequence.  `V34SetINFO0aBits` does not write +0xaa6c,
		 * so nothing observable turns on it.
		 */
		char *r = (char *)*(short **)((char *)o + TX1_PTR_AA6C);
		short lead = (o->role == 0x65 && o->v90_receiver != 0)
			     ? 0x1e : 0x11;

		*(short *)(r + 0x14) = -1;
		*(short *)(r + 0x1a) = 0;
		*(short *)(r + 0x1e) = 0;
		*(short *)(r + 0x22) = 0;
		*(short *)(r + 0x18) = lead;
		/* 0x68616 */
		*(short *)(r + 0x1c) = 8;
		*(short *)(r + 0x16) = 1;
		*(short *)(r + 0x28) = 0x10;
		*(short *)(r + 0x2a) = 0x10;
		*(short *)(r + 0x20) = 0;
		*(int *)(r + 0x24) = 0xff72;
		*(int *)(r + 0x2c) = 0xff72;
	}

	tx1_put(o, TX1_F358C, 0);
	return V34TX1_LOOP;				/* 0x640a1 */
}

/*
 * ---------------------------------------------------------------------------
 * The differential quadrant step 69 and 64/68 share, 0x638eb..0x6390d and
 * 0x63697..0x636b9.
 *
 * The scrambler's two bits are not transmitted directly: they are ADDED to
 * the quadrant last sent, modulo four, and the sum is both the point's index
 * and the new "last".  `cur_quadrant` is the quadrant chosen and `prev_quadrant` the one
 * carried forward, and the object writes them with two stores of the same
 * register either side of the table load.
 *
 * The two sites are instruction for instruction the same -- the same three
 * fields, the same mask, the same order -- so this is one static rather than
 * two copies, for the reason 78 and 85 share `tx1_ja_common`.  `prev_quadrant` is
 * read with `movzwl` and the sum is masked to two bits, so a fixture value
 * outside 0..3 cannot change the answer.
 */
static short
tx1_dpsk4(struct v34_object *o, short q)
{
	short k = (short)((q + (unsigned short)o->prev_quadrant) & 3);
	int point = vect4[k];

	o->cur_quadrant = k;
	o->prev_quadrant = k;
	tx1_put_point(o, point);
	return k;
}

/*
 * ---------------------------------------------------------------------------
 * 69 `EXMIT`, 0x63858, with the sixteen-point half at 0x67031 and the
 * segment's end at 0x66e01.
 *
 * ONE HALFWORD OF THE RECEIVER PICKS THE CONSTELLATION.  +0x11e against
 * 0x89b0 -- a sixteen-bit compare, and the arm's first instruction -- sends
 * the pass either through `vect4`, two samples of `vect_idx` at a time, or
 * through `vect16`, four at a time.  Everything else about the two halves is
 * the same shape: scramble, add to the quadrant last sent, transmit, count.
 *
 * THE GENERATOR SELECT IS `tx_flags & 1` AND NOT `role == 0x65`.  71 and 86
 * choose their scrambler polynomial on the state word (finding F340); this arm
 * chooses on bit 0 of the transmitter's flags, and the sense is inverted --
 * the bit SET takes the 0x04000000 tap, which is `V34scrambler`'s `mode` 0.
 * The object reads the flags word once (0x67048) and tests it twice on the
 * sixteen-point path, so one `mode` for both calls is what it does.
 *
 * THE SIXTEEN-POINT HALF SCRAMBLES TWICE AND THE FIRST CALL IS PASSED 0xf
 * WHERE THE SECOND IS PASSED 3 (0x67035 against 0x670c7).  For `nbits == 2`
 * the two are the same argument: only bits 0 and 1 of `bits` are consumed and
 * both are set either way.  It is written as the object writes it and the
 * mutation that makes them equal is recorded as equivalent, with that proof.
 *
 * The first call's quadrant goes into `cur_quadrant` and is READ BACK FROM THERE
 * (0x67131) to index `vect16` as the row, with the second call's two bits as
 * the column -- `vect16[q2 + 4 * k]`, the same packing v34k56.cpp and
 * v34pcmmain.cpp use.  So the sixteen-point symbol carries four bits and the
 * four-point one carries two, which is why one advances `vect_idx` by four
 * and the other by two.
 *
 * THE SEGMENT ENDS AT `vect_idx == 0x14`, tested as sixteen bits after the
 * advance and reached from either half -- ten passes of the four-point one or
 * five of the sixteen-point one, both from zero.  0x66e01 then reloads
 * `vect_idx` with 8 and moves the transmit machine to DATAXMIT.  Its compare
 * against DATAXMIT cannot be false, for finding F342's reason at 65: the arm
 * is reached only through table 1 at `txstate == 69` and `txmit` does not
 * write +0x3596.
 */
int
v34tx1_exmit(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	short mode = (short)((o->tx_flags & V34_TXFLAG_CALLER) == 0);
	short q;

	if ((unsigned short)tx1_get(o, TX1_F382) == 0x89b0u) {
		/* 0x67031 */
		short k;
		int point;

		q = (short)V34scrambler((unsigned *)&o->tx_scr_sr, mode, 0xf, 2);
		o->cur_quadrant = (short)((q + (unsigned short)o->prev_quadrant) & 3);

		q = (short)V34scrambler((unsigned *)&o->tx_scr_sr, mode, 3, 2);
		k = o->cur_quadrant;			/* re-read, 0x67131 */
		o->prev_quadrant = k;
		point = vect16[q + 4 * k];
		tx1_put_point(o, point);
		txmit(o);
		o->vect_idx = (short)((unsigned short)o->vect_idx + 4);
	} else {
		/* 0x6386b */
		q = (short)V34scrambler((unsigned *)&o->tx_scr_sr, mode, 3, 2);
		(void)tx1_dpsk4(o, q);
		txmit(o);
		o->vect_idx = (short)((unsigned short)o->vect_idx + 2);
	}

	/* 0x6392c */
	if ((unsigned short)o->vect_idx == 0x14u) {
		/* 0x66e01 */
		short txst = tx1_get(o, TX1_TXSTATE);

		o->vect_idx = 8;
		hs_setstate(o, TX1_TXSTATE, V34HS_DATAXMIT);
		if (dsplibs_debug_level > 1)		/* 0x66e48 */
			dsplibs_debug_printf("V34MP- E transmit completed\n");
		return V34TX1_LOOP;			/* 0x6409a, 0x6431f */
	}
	return V34TX1_LOOP;				/* 0x63941 */
}

/*
 * ---------------------------------------------------------------------------
 * 64 `JTXMIT` and 68 `J1TXMIT`, 0x635cc, with 68's tail at 0x65653, 64's at
 * 0x6372e and the `role == 0x66` path at 0x66ca8.
 *
 * ONE TABLE ENTRY, ONE BODY, TWO TAILS -- a third shape beside the file's
 * other two.  `.rodata+0x2da0` gives indices 59 and 63 the identical address
 * and, unlike 5/54/74, the prologue does NOT re-read `txstate`: the body runs
 * the same for both txstates and only the pass on which `vect_idx` wraps to
 * zero reaches the compare at 0x636ff.  So driving 68 through a pass that
 * does not wrap is 64's check under a different index, and driving it through
 * one that does is a behaviour of its own; `t_v34hstx1.c` says which is
 * which.
 *
 * THE BODY, in the object's order:
 *
 *   - the countdown at +0xaa78, which is 78 and 85's exactly -- `tx1_ja_common`
 *     -- except that its completion block at 0x6533c FALLS BACK INTO the body
 *     at 0x635f0 rather than tail-calling anything.  The return value is
 *     therefore ignored here, and that is the whole difference;
 *   - +0x25d8 counts one per pass;
 *   - TWO BITS OUT OF +0x25d6, selected by `vect_idx`: the halfword is loaded
 *     zero-extended and shifted right by TWICE the index, so the field is
 *     eight dibits and the eight passes of a wrap send all of them.  The
 *     shift count is `%cl` and the hardware masks it to five bits, which is
 *     why it is written `& 31` here;
 *   - the scrambler, on `tx_flags & 1` as 69's is;
 *   - the differential quadrant and one `vect4` point;
 *   - `vect_idx = (vect_idx + 1) & 7`, stored before anything branches.
 *
 * THEN THREE WAYS OUT, tested in this order and no other:
 *
 *   role == 0x66       -> 0x66ca8, from EITHER txstate
 *   vect_idx != 0       -> the loop, which is most passes
 *   txstate == 68       -> 0x65653, the segment's end
 *   otherwise (64)      -> 0x6370d, +0x25da and +0x25d8
 *
 * 0x66ca8 IS A THREE-TEST CONJUNCTION AND ONLY ITS LAST ARM WRITES.  The
 * receiver's `V34_RX_FLAG_DATA` short-circuits the other two -- the object
 * spells it `testb $0x4,0x123(%ebp)`, bit 2 of the flags word's high byte --
 * and without it BOTH +0x35a2 and +0x25da must be non-zero.  The arm then
 * moves the transmit machine to XMIT0 and clears `vect_idx` through 0x62d32,
 * which 51 also rejoins.  The two declining exits leave `vect_idx` at the
 * value the body stored, so the clear is the observable difference.
 *
 * 64's TAIL IS A SECOND COUNTER.  +0x25da must be 2 or the pass simply ends;
 * +0x25d8 must be PAST 0x80, tested SIGNED (`jle`), so a sixteen-bit counter
 * that has wrapped negative does not end the segment.  Past it, +0x25d6 is
 * reloaded with 0x899f -- a fresh sixteen dibits -- and the transmit machine
 * becomes 68, which is how 64 hands over.  The compare guarding that store
 * cannot be TRUE: this path is reached only because 0x636ff found `txstate`
 * was not 68 and nothing between the two writes it.
 *
 * AND THEN THE SECOND FREEZE.  If +0xaa78 is still non-zero the arm raises
 * bit 2 of `tx_flags`, reports both cancellers and ZEROES the counter -- the
 * same flag and the same pair of reports as the countdown's own completion,
 * on a different condition.  The two are mutually exclusive on one pass:
 * reaching zero at the top leaves nothing for this to do.  That flag-then-
 * report-both triple IS `v34FreezeEcho`, and it is now called rather than
 * spelled out; finding F572 is the transcript that proves the call, since the
 * blob prints `V34HSHAK: Freeze EC` and both report headers here and this
 * file emitted none of the three while it inlined the pair.
 *
 * 68's TAIL, 0x65653, CLEARS RATHER THAN COUNTS: `tx_scr_sr` (a 32-bit store),
 * `prev_quadrant` and `seg_symcount` all go to zero and the transmit machine moves to
 * TRNSEG4A.  THE OBJECT DOES NOT COMPARE FIRST here -- there is no
 * `if (txstate != ...)` guard the way every other state change in this file
 * has one.  It is written as `hs_setstate` anyway, because the enclosing
 * `txstate == J1TXMIT` test has already established the value and
 * `hs_setstate`'s compare is therefore provably dead: same stores, same
 * transcript, and one copy of the three format strings instead of two.  The
 * blob does print the `J1TXMIT=>TRNSEG4A` line, so the diagnostic is not
 * optional even though the compare is.
 */
int
v34tx1_jtxmit(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx =
		(struct v34_receiver *)((char *)objp + TX1_RECEIVER);
	unsigned shift;
	unsigned short idx;
	short bits, mode, q;

	/* 0x635cc, and the completion at 0x6533c falls back into 0x635f0 */
	(void)tx1_ja_common(o, "V34Hshak: on JTXMIT - time to freeze"
				   " echo...\r\n");		/* 0x682d6 */

	/* 0x635f0 */
	tx1_put(o, TX1_F25D8,
		(short)((unsigned short)tx1_get(o, TX1_F25D8) + 1));

	shift = (unsigned)(2 * (int)o->vect_idx) & 31u;
	bits = (short)((unsigned)(unsigned short)tx1_get(o, TX1_F25D6) >> shift);

	mode = (short)((o->tx_flags & V34_TXFLAG_CALLER) == 0);
	q = (short)V34scrambler((unsigned *)&o->tx_scr_sr, mode, bits, 2);
	(void)tx1_dpsk4(o, q);
	txmit(o);

	idx = (unsigned short)(((unsigned short)o->vect_idx + 1) & 7);
	o->vect_idx = (short)idx;

	if (o->role == 0x66) {
		/* 0x66ca8 */
		if (!(rx->flags & V34_RX_FLAG_DATA)) {
			if (tx1_get(o, TX1_F35A2) == 0)
				return V34TX1_LOOP;	/* 0x629c8 */
			if (tx1_get(o, TX1_F25DA) == 0)
				return V34TX1_LOOP;	/* 0x63da2 */
		}
		/* 0x66cd1 */
		hs_setstate(o, TX1_TXSTATE, V34HS_XMIT0);
		o->vect_idx = 0;			/* 0x62d32 */
		return V34TX1_LOOP;			/* 0x62d70 */
	}

	/* 0x636f0 */
	if (idx != 0)
		return V34TX1_LOOP;			/* 0x63941 */

	if (tx1_get(o, TX1_TXSTATE) == V34HS_J1TXMIT) {
		/* 0x65653 */
		o->tx_scr_sr = 0;
		hs_setstate(o, TX1_TXSTATE, V34HS_TRNSEG4A);
		o->prev_quadrant = 0;
		o->seg_symcount = 0;
		return V34TX1_LOOP;			/* 0x640a1 */
	}

	/* 0x6370d */
	if (tx1_get(o, TX1_F25DA) != 2)
		return V34TX1_LOOP;			/* 0x6409a */
	if (tx1_get(o, TX1_F25D8) <= 0x80)
		return V34TX1_LOOP;			/* 0x6431f */

	/* 0x6372e */
	tx1_put(o, TX1_F25D6, (short)0x899f);
	hs_setstate(o, TX1_TXSTATE, V34HS_J1TXMIT);

	/* 0x637c8 */
	if (tx1_get(o, TX1_COUNT) != 0) {
		if (dsplibs_debug_level > 1)		/* 0x6c725 */
			dsplibs_debug_printf(
				"V34Hshak: on J1TXMIT - forced freeze echo" " (count2 = %d)...\r\n",
				tx1_get(o, TX1_COUNT));
		v34FreezeEcho(o);
		tx1_put(o, TX1_COUNT, 0);
		return V34TX1_LOOP;			/* 0x64326 */
	}
	return V34TX1_LOOP;				/* 0x629c8 */
}

/*
 * ---------------------------------------------------------------------------
 * 67 `XMITMP`, 0x6399b -- the message reader, and the two symbol mappers it
 * feeds.
 *
 * THE WHOLE OF 0x639db..0x63aae IS `getbit` INLINED, and that is a
 * measurement rather than a resemblance: every arm of the reader
 * v34hshak.c:2513 carries is here, in the same order and on the same fields
 * -- the "still have bits" short cut at 0x64588, the whole-word refill at
 * 0x63a1d, the PART-word refill at 0x646f6 that does not advance `idx`, the
 * CRC-16 flush at 0x64781, the "overrun by exactly sixteen" arm at 0x64857
 * that loads four ones, and the exhausted-with-`repeat` restart at 0x64811 --
 * which does not even inline: it reloads the reader and CALLS `getbit` at
 * 0x6484c for the first bit of the repeat, the same recursion 0x5ec47 makes.
 * So this arm is written as one call, the way 78 and 85 are written over
 * `tx1_ja_common`.
 *
 * WHAT THAT COSTS, named rather than implied: no mutation in
 * `test/mutations/v34hstx1.json` then tests the reader.  `tools/mutate.py`
 * anchors on source text and the reader's text is in `v34hshak.c`, which is
 * `t_v34hshak.c`'s suite and not this one.  What the runs here DO test is
 * that the call is the right one on the right record, and every branch the
 * arm takes around it.
 *
 * ---------------------------------------------------------------------------
 * THE SHAPE.  Two or four bits are collected into `cur_quadrant`, lowest first, and
 * then ONE symbol is mapped out of them:
 *
 *     cur_quadrant = 0
 *     n = 0
 *     while (n <= (receiver +0x11e == 0x89b0 ? 3 : 1)) {
 *             cur_quadrant |= getbit(*(obj+0xaa6c)) << n;
 *             vect_idx += 1;
 *             ... the three checkpoints ...
 *             n += 1;
 *     }
 *     one symbol, four points or sixteen, then `txmit`
 *
 * THE SELECTOR IS 69 `EXMIT`'s, THE SAME HALFWORD AGAINST THE SAME CONSTANT
 * (0x639b9 here, 0x6385c there), and it does the same thing: sixteen points
 * carry four bits a symbol and four points carry two.  It is RE-READ at the
 * top of every pass (0x63b46) and again on the way out of 0x648bf (0x6490e);
 * nothing this arm calls is known to write it, so the three readings agree,
 * and the last one is the one the mapper uses.
 *
 * ---------------------------------------------------------------------------
 * THE THREE CHECKPOINTS, after each bit, tested in this order and no other.
 * Each is a pair of constants selected by bit 5 of the receiver's flags word
 * -- `V34_RX_FLAG_RENEG` to the receiver, which says nothing about what it
 * means here; the same bit picks the message's length below.
 *
 *   vect_idx == 0x55 (bit set) or 0xbb (clear)
 *              push THREE zero bits onto the end of the reader's accumulator,
 *              or ONE.  `acc <<= 3` with `avail += 3` leaves the bits already
 *              in `acc` coming out in the same order and three zeros after
 *              them, which is what a fill sequence is.  0x645a4 and 0x64750,
 *              and they are NOT the same width;
 *   vect_idx == 0x58 (bit set) or 0xbc (clear)
 *              the sequence is over -- 0x645d0, below;
 *   vect_idx == +0x3590
 *              push ONE zero bit and move the stuff point on by 0x11.
 *              0x64727.
 *
 * The first two are exclusive by construction and the object still tests them
 * in series: 0x6459b jumps straight to the loop bottom, so a pass that stuffs
 * neither ends the sequence nor reaches +0x3590.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS NOT RECONSTRUCTED, and it is finding F341's gap again.  Three blocks
 * here are entered only when `dsplibs_debug_level > 1` -- 0x646b5, 0x67254 and
 * 0x6b410 -- and none is written.  Every one is a diagnostic on the path where
 * the reader is reloaded or the transmit machine moves.
 */

/*
 * The message reader, at whatever +0xaa6c holds.  The object re-reads the
 * pointer at every site (0x639e2, 0x645ab, 0x64757, 0x6472d, 0x647d5,
 * 0x6462f) and nothing in this arm writes it, so the reads all give the same
 * record; they are written out one per site anyway, because that is the
 * object's shape and the equivalence stops being true the moment something
 * aims it.
 */
static struct v34_bitsource *
tx1_bitsource(struct v34_object *o)
{
	return *(struct v34_bitsource **)((char *)o + TX1_PTR_AA6C);
}

/*
 * 0x64635 -- re-arm the reader for the next sequence, and the counters with
 * it.  `nbits` is the ONE field bit 5 of the flags word chooses: 0x30 with
 * the bit set and 0x90 with it clear, computed in the object as
 * `sbb`/`and $0x60`/`add $0x30` rather than branched (0x64656..0x6466c).
 *
 * `repeat` goes to zero, so the reader will return -1 rather than restart
 * once this message runs out; `acc0`/`avail0` are still loaded, which is what
 * makes that a decision of the caller's rather than of the reader's.
 */
static void
tx1_mp_reload(struct v34_object *o, struct v34_bitsource *b,
	      unsigned short flags)
{
	b->crc = (short)0xffff;
	b->pos = 0;
	b->idx = 0;
	b->repeats = 0;
	b->wordbits = 0x10;
	b->nbits = (short)((flags & 0x20u) ? 0x30 : 0x90);
	b->crc_on = 1;
	b->avail = 0x12;
	b->avail0 = 0x12;
	b->repeat = 0;
	b->acc = 0x3fffe;
	b->acc0 = 0x3fffe;

	/* 0x646a1 */
	o->vect_idx = 0;
	tx1_put(o, TX1_F3590, 0x22);

	/*
	 * 0x646b5.  `flags` IS the receiver's word and the object re-reads
	 * it here rather than using the copy it was handed; nothing between
	 * the two writes it, so the argument is the parameter.
	 */
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf(
			"V34MP, Starting txmit MP again(%d)," " rxflgs=0x%x,txflags=0x%x\n",
			tx1_get(o, TX1_F359E), flags, o->tx_flags);
}

/*
 * 0x645d0 -- the sequence is over.  Count it in +0x359e, and then one of
 * three things.  Returns non-zero when the arm LEAVES the bit loop, which is
 * the only exit that is not "collect the next bit".
 *
 * THE FIRST TEST IS AT obj+0xaa3c AND NOT AT THE RECORD, and the two are the
 * same halfword only because `getMPrecvdBits` aims +0xaa6c at +0xaa3c.  The
 * object reads the object-relative one here (0x645de) and the record-relative
 * one twelve instructions later (0x647db), so a reconstruction using one for
 * both is wrong wherever the pointer is aimed anywhere else.
 *
 * WHERE THE THREE GO:
 *
 *   +0xaa3c bit 0 set, flags & 0x90 == 0x90, and this the FOURTH sequence
 *              or later: `initdigital` once (guarded by +0x3598), the
 *              transmit machine to 69 EXMIT, `vect_idx` cleared and bit 5 of
 *              the flags word dropped.  0x648a7 and 0x648bf, and it is the
 *              one path that leaves the loop;
 *   flags & 0x18 == 0x10 and +0x359e past one:
 *              raise bit 0 of the record's own first halfword, and clear
 *              +0x359e if it was NOT already up.  0x647be and 0x647d3;
 *   otherwise  straight to the reload.
 *
 * THE COMPARE AT 0x648ab IS SIGNED (`jle`) and so is 0x647cd's, which is why
 * both are spelled through `short`.
 */
static int
tx1_mp_sequence_end(struct v34_object *o, struct v34_receiver *rx)
{
	unsigned short flags;
	short seq;
	int stamp;

	seq = (short)((unsigned short)tx1_get(o, TX1_F359E) + 1);
	tx1_put(o, TX1_F359E, seq);

	if ((*((unsigned char *)o + TX1_FAA3C) & 1) != 0) {
		flags = rx->flags;			/* 0x645f9 */
		if ((flags & 0x90u) == 0x90u && seq > 3) {
			/* 0x648b1 */
			if (tx1_get(o, TX1_F3598) == 0) {
				initdigital(o);
				tx1_put(o, TX1_F3598, 1);
			}
			/* 0x648bf */
			hs_setstate(o, TX1_TXSTATE, V34HS_EXMIT);
			o->vect_idx = 0;
			rx->flags = (unsigned short)(rx->flags & ~V34_RX_FLAG_RENEG);
			return 1;
		}
		stamp = (flags & (V34_RX_FLAG_TRN_WATCH | V34_RX_FLAG_LATE_TRN))
			== V34_RX_FLAG_TRN_WATCH;		/* 0x64614 */
	} else {
		flags = rx->flags;			/* 0x647a9 */
		stamp = (flags & 0x18u) == 0x10u;	/* 0x647b0 */
	}

	/*
	 * 0x647c5 RE-READS +0x359e rather than using the value just stored.
	 * Nothing between the two writes it, so the two are the same number;
	 * it is written as the object writes it.
	 */
	if (stamp && tx1_get(o, TX1_F359E) > 1) {
		/* 0x647d3 */
		struct v34_bitsource *b = tx1_bitsource(o);

		if ((b->word[0] & 1) == 0)
			tx1_put(o, TX1_F359E, 0);
		b->word[0] = (short)((unsigned short)b->word[0] | 1u);
		if (dsplibs_debug_level > 1)		/* 0x67254 */
			dsplibs_debug_printf(
				"V34MP, MP detected, starting MP' txmit");
		flags = rx->flags;			/* 0x64805 */
	}

	tx1_mp_reload(o, tx1_bitsource(o), flags);
	return 0;
}

/*
 * 0x64929 -- two bits, one differential quadrant, one `vect4` point.  This is
 * 69's four-point half with the scrambler's `bits` coming out of `cur_quadrant`
 * instead of a literal three, so `tx1_dpsk4` carries the rest of it.
 */
static void
tx1_mp4(struct v34_object *o, short src)
{
	short pick = (short)((o->tx_flags & V34_TXFLAG_CALLER) == 0);
	short q = (short)V34scrambler((unsigned *)&o->tx_scr_sr, pick, src, 2);

	(void)tx1_dpsk4(o, q);
}

/*
 * 0x63b62 -- four bits, two scrambler steps, one `vect16` point.  Instruction
 * for instruction this is 69's sixteen-point block at 0x67031 with two
 * differences and no others: the two `bits` arguments are the low and the high
 * dibit of `cur_quadrant` where 69 passes the literals 0xf and 3, and this one does
 * not advance `vect_idx` (the bit loop already did, once per bit).  It is
 * written out rather than shared with 69 because sharing would have to rewrite
 * 69's body, and the mutations already aimed at that text are what say 69 is
 * right.
 *
 * `cur_quadrant` IS READ SIGNED (`movswl`, 0x63b83) AND IT CANNOT MATTER.  0x639ab
 * clears the field before the loop and the loop only ORs bits in, so the two
 * readings can differ only when bit 15 is set -- which needs the reader's
 * exhausted arm, whose -1 fills every bit.  Even then the scrambler consumes
 * bits 0 and 1 alone, and after the arithmetic `sar $2` bits 2 and 3, and all
 * four are ones under either reading.  So it is a proof and not a run.
 *
 * The second call takes the SAVED halfword shifted down by two, not the value
 * `cur_quadrant` now holds: 0x63c03 restores the register before 0x63c12 shifts it,
 * and 0x63c05 has already overwritten the field with the first quadrant.
 */
static void
tx1_mp16(struct v34_object *o, short src)
{
	short pick = (short)((o->tx_flags & V34_TXFLAG_CALLER) == 0);
	short k, q;
	int point;

	q = (short)V34scrambler((unsigned *)&o->tx_scr_sr, pick, src, 2);
	o->cur_quadrant = (short)((q + (unsigned short)o->prev_quadrant) & 3);

	q = (short)V34scrambler((unsigned *)&o->tx_scr_sr, pick,
				(short)(src >> 2), 2);
	k = o->cur_quadrant;					/* re-read, 0x63c72 */
	o->prev_quadrant = k;
	point = vect16[q + 4 * k];
	tx1_put_point(o, point);
}

int
v34tx1_xmitmp(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx =
		(struct v34_receiver *)((char *)objp + TX1_RECEIVER);
	int wide = 0;
	int n = 0;

	o->cur_quadrant = 0;					/* 0x639ab */

	for (;;) {
		struct v34_bitsource *b;
		unsigned short flags;
		short bit, idx;

		wide = (unsigned short)tx1_get(o, TX1_F382) == 0x89b0u;
		if (n > (wide ? 3 : 1))
			break;

		/* 0x639db, and 0x6484c where it does not inline */
		b = tx1_bitsource(o);
		bit = getbit(b);

		/*
		 * 0x63ab1.  `n` reaches the shift through `movzbl`, and the
		 * bit reaches it sign-extended -- so the reader's -1 fills
		 * `cur_quadrant` from bit `n` up rather than setting one bit.
		 */
		o->cur_quadrant = (short)((unsigned short)o->cur_quadrant
				   | (unsigned short)
				     ((unsigned)(int)bit
				      << ((unsigned)(unsigned char)n & 31u)));

		/* 0x63ad8 */
		o->vect_idx = (short)((unsigned short)o->vect_idx + 1);
		idx = o->vect_idx;
		flags = rx->flags;

		if (idx == ((flags & 0x20u) ? 0x55 : 0xbb)) {
			/* 0x6459b */
			b = tx1_bitsource(o);
			if (flags & 0x20u) {
				b->acc = (int)((unsigned)b->acc << 3);
				b->avail = (short)
					   ((unsigned short)b->avail + 3);
			} else {
				b->acc = (int)((unsigned)b->acc << 1);
				b->avail = (short)
					   ((unsigned short)b->avail + 1);
			}
		} else if (idx == ((flags & 0x20u) ? 0x58 : 0xbc)) {
			if (tx1_mp_sequence_end(o, rx)) {
				/* 0x6490e */
				wide = (unsigned short)
				       tx1_get(o, TX1_F382) == 0x89b0u;
				break;
			}
		} else if ((unsigned short)o->vect_idx
			   == (unsigned short)tx1_get(o, TX1_F3590)) {
			/* 0x64727 */
			b = tx1_bitsource(o);
			b->acc = (int)((unsigned)b->acc << 1);
			b->avail = (short)((unsigned short)b->avail + 1);
			tx1_put(o, TX1_F3590,
				(short)((unsigned short)
					tx1_get(o, TX1_F3590) + 0x11));
		}

		n = (short)(n + 1);			/* 0x63b38 */
	}

	if (wide)
		tx1_mp16(o, o->cur_quadrant);
	else
		tx1_mp4(o, o->cur_quadrant);

	txmit(o);				/* 0x62d5f and 0x64a42 */
	return V34TX1_LOOP;			/* 0x62d70 and 0x640a1 */
}

/*
 * ---------------------------------------------------------------------------
 * 24 `TX_DPSK`, 0x62b96 -- one bit of a message as one tone, and the whole of
 * Modem-on-Hold's clear-down when the message runs out.
 *
 * THE 2,220 BYTES ARE `getbit` INLINED TWICE, which is finding F424's result
 * at 67 for a second time and is why this arm is short.  0x62bb5..0x62c64
 * with 0x649f1, 0x654a7, 0x67b95, 0x68480 and 0x6876a is the reader
 * v34hshak.c:2513 carries, arm for arm and field for field: the "still have
 * bits" short cut at 0x62bc9, the whole-word refill at 0x654a7, the PART-word
 * refill at 0x62bfc that does not advance `idx`, the CRC-16 fold at 0x62c35,
 * the flush at 0x67ba8, the "overrun by exactly sixteen" arm at 0x6876a, and
 * the exhausted-with-`repeat` restart at 0x68490 -- which does not inline
 * either: it CALLS `getbit` at 0x684bb for the first bit of the repeat, the
 * same recursion 0x5ec47 makes.  0x67c4e..0x67d04 with 0x6887f, 0x69165,
 * 0x69a49, 0x6a842, 0x6a94b, 0x6c996, 0x6c9be and 0x70683 is the SAME reader
 * a second time, over a record the arm has just re-armed by hand.
 *
 * So the arm is two calls, the way 78 and 85 are written over
 * `tx1_ja_common` and 67 over `getbit`.  What it costs, named rather than
 * implied, is finding F424's cost unchanged: `tools/mutate.py` anchors on
 * source text and the reader's text is in `v34hshak.c`, which is
 * `t_v34hshak.c`'s suite.  What the runs here test is that each call is the
 * right one on the right record and every branch the arm takes around them.
 *
 * ---------------------------------------------------------------------------
 * THE SHAPE.  One bit, one tone, and three ways for the message to end:
 *
 *     if (moh_active)  vect_idx += 1        the Modem-on-Hold clock
 *     bit = getbit(*(obj + 0xaa6c))
 *     if (bit >= 0)  {  short_358c ^= bit; vect4[2 * (short_358c & 1)]; txmit;  }
 *     else if (!moh_active)      txstate = 60 TONE_AB and nothing else
 *     else if (!moh_msg_pending)      re-arm the reader BY HAND, take one more bit,
 *                           send it, and fall into the hold tail
 *     else                  the moh_message dispatch, then the hold tail
 *
 * THE TONE IS 60 `TONE_AB`'s, THE SAME TABLE ENTRY SCALED THE SAME WAY --
 * 0x64a35 here and 0x62d52 there, `mov 0x0(,%reg,8)` over four-byte entries,
 * so it selects `vect4[0]` or `vect4[2]`, the two ends of a diagonal.  What
 * 24 adds is that the message bit is XORed INTO +0x358c first, so the tone
 * alternates on a one and holds on a zero.  That is differential phase-shift
 * keying at one bit a symbol, which is the state's name.
 *
 * FINDING F323'S ONE COLLISION IS THIS, AND IT IS NOT IDENTITY.  Entered cold
 * 24 and 60 write the same 69 bytes with the same signature, and the reason
 * is narrow: the fixture's fill leaves +0xabe8 clear, so no `vect_idx` tick;
 * the reader hands back a ZERO bit, so the store into +0x358c writes the
 * value already there and the tone selected is the one 60 selects; and the
 * reader's own advance lands OUTSIDE the object, where finding F323's byte
 * count does not look.  Any one of those three moving separates them, and
 * `t_v34hstx1.c` moves all three.  They are two entries at two addresses --
 * 0x62b96 against 0x62d3d, 2,220 bytes against thirty-four -- and the file
 * asserts that too.
 *
 * ---------------------------------------------------------------------------
 * THE HOLD TAIL, 0x64fec, is where both Modem-on-Hold paths end, and its
 * first test is a TIME rather than a state: `vect_idx` against
 * `(rtd >> 4) + 1200`, both sign-extended from shorts and the shift
 * arithmetic.  Below it the arm simply leaves; at or above it the hold is
 * over and one of three things happens.
 *
 * WHAT IS NOT RECONSTRUCTED, and it is finding F341's gap again.  Fourteen
 * blocks here are entered only when `dsplibs_debug_level > 1` -- 0x68704,
 * 0x68b12, 0x6c771, 0x6c7db, 0x6a87e, 0x6b06c, 0x6c760 and the seven
 * `cmpl $0x1` sites that guard them -- and none is written.  Every one is a
 * diagnostic on a state transition.
 *
 * AND ONE PATH THAT WRITES ONE BYTE.  `moh_message` OUTSIDE 0..3 makes the
 * dispatch do NOTHING but clear its own one-shot at +0xabf8, and below the
 * threshold the hold tail leaves at 0x6431f -- so that run neither transmits
 * nor moves a state word, and `t_v34hstx1.c` guards it on the one-shot
 * instead.  It has to be that run and not one past the threshold: it is the
 * only place a clear-down that should not have happened is visible, because
 * BOTH of the tail's ways out leave exactly what a spurious clear-down leaves
 * -- the retrain's own `v34handshakinit` clears +0xabe4 (v34hshak.c:1307) and
 * rewrites both state words, and the tail's clear-down IS the clear-down.
 */

/* +0xabe4 and +0xabe6, two halfwords of `unmapped_abe4`; +0xabe8 is
 * TX1_MOH_ACTIVE above and is the same region's Modem-on-Hold flag.  The
 * clear-down raises +0xabe4 and the two `v34handshakinit` paths raise
 * +0xabe6; no other site in this tree reads either. */
#define TX1_FABE4	0xabe4
#define TX1_FABE6	0xabe6

/*
 * +0xabf8 and +0xabf9, two BYTES of `unmapped_abf8`, read with `cmpb`.
 * Named `moh_msg_pending` and `moh_path_sel` in `struct v34_object`
 * (`v34fsk.h`) -- kept as raw-offset macros here, matching this file's own
 * convention for fields also reached elsewhere by offset.
 *
 * +0xabf8 is a one-shot: while it is up the arm runs the message dispatch and
 * clears it, and once it is down the arm re-arms the reader instead.  +0xabf9
 * chooses twice -- which message is built at 0x6a3f3, and which of the tail's
 * three ways out is taken at 0x65012.  Neither has another reader here.
 */
#define TX1_MOH_MSG_PENDING	0xabf8
#define TX1_MOH_PATH_SEL	0xabf9

/*
 * 0x64a13 and 0x67d07, the same nine instructions twice.  The message bit is
 * XORed into +0x358c as a 32-bit value and the field stored back as sixteen,
 * then bit 0 of it selects `vect4[0]` or `vect4[2]`.
 *
 * THE BIT REACHES THE XOR SIGN-EXTENDED (`cwtl` at 0x64a05 and 0x67d0e), so
 * the reader's -1 COMPLEMENTS the whole halfword rather than flipping its low
 * bit.  Only the low bit is then read, so the tone is the same either way and
 * the field is not: a run with an exhausted reader is what makes that
 * visible, and `t_v34hstx1.c` has one.
 */
static void
tx1_dpsk_tone(struct v34_object *o, short bit)
{
	short sel = (short)((unsigned short)tx1_get(o, TX1_F358C)
			    ^ (unsigned short)bit);

	tx1_put(o, TX1_F358C, sel);
	tx1_put_point(o, vect4[2 * (sel & 1)]);
}

/*
 * 0x64eaf, 0x65046 and 0x688b4 -- the clear-down, written three times in the
 * object and once here.  All three are the same three compare-and-store
 * pairs in the same order, and the third adds one store of its own.
 *
 * THE txstate COMPARE AT 0x64eb6 CANNOT BE FALSE, for finding F342's reason at
 * 65: that site is reached only from the dispatch at `txstate == 24` and
 * nothing between them writes +0x3596.  The OTHER TWO CAN, because the hold
 * tail runs after the dispatch has already moved the machine -- which is why
 * this is one function and not one inlined body.
 */
static void
tx1_moh_cleardown(struct v34_object *o)
{
	hs_setstate(o, TX1_TXSTATE, V34HS_MOH_CLEARDOWN);
	hs_setstate(o, TX1_RXSTATE, V34HS_WAIT);
	tx1_put(o, TX1_FABE4, 1);
}

/*
 * The two 0x69041 sites share one literal; it is named so that the
 * sharing is visible in the source rather than only in the object.
 */
#define MOH_ILLEGAL	"MOH: Illegal MH sequence under MHreq,"	\
			" initiating retrain\r\n"

/*
 * 0x689f6 -- put the modem on hold: MOH_SILENCE, WAIT, and the two counters
 * the silence arm at 0x63d58 then runs on.  `t_v34hstx1.c` drives it through
 * both of its two entries, `moh_message == 1` and `moh_recvd` at 0 or 4.
 */
static void
tx1_moh_on_hold(struct v34_object *o)
{
	hs_setstate(o, TX1_TXSTATE, V34HS_MOH_SILENCE);
	hs_setstate(o, TX1_RXSTATE, V34HS_WAIT);
	tx1_put(o, TX1_COUNT, 0);
	o->vect_idx = 0;
}

/*
 * 0x69041 and 0x6923d -- the retrain entry, twice over and identical.
 *
 * THE MESSAGE IS THE CALLER'S, for tx1_ja_common's reason: 0x69041 and
 * 0x69227 are two guards in front of two copies of these two stores, and
 * the only difference between the copies is which literal they push.  The
 * dispatch's TWO calls share one copy because they share one message.
 *
 * `v34handshakinit(obj, 1)` RE-ARMS THE LOOP THAT IS DISPATCHING THIS ARM:
 * `v34modeminit` sets the block's sample limit at +0x2aa0 to six and clears
 * `vect_idx`, and it moves the transmit machine to SILENCERETRAIN, so the
 * pass after this one is 5/54/74's four silent samples and the loop then ends
 * on its own.  Nothing here depends on that; it is recorded because a callee
 * rewriting the bound of the loop that called it is not what anybody expects
 * to find.
 */
static void
tx1_moh_reinit(struct v34_object *o, const char *msg)
{
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf(msg);
	v34handshakinit(o, 1);
	tx1_put(o, TX1_FABE6, 1);
}

/*
 * 0x6a3cf -- build the next Modem-on-Hold message and install a fresh reader
 * for it.
 *
 * THE CALL TAKES THE OLD RECORD AND THE STORE INSTALLS THE NEW ONE.  0x6a42e
 * reads +0xaa6c before the call and 0x6a448 writes it after, so
 * `VPcmV34SetMohMessageBits` fills `word[0]` of WHATEVER THE READER WAS
 * POINTING AT and the twelve stores below re-arm the record at +0xa94c.  In
 * the object's own configuration those are the same record --
 * `v34handshakinit`'s mode 4 aims +0xaa6c at +0xa94c (v34hshak.c:1451) -- so
 * only a poke separates them, and `t_v34hstx1.c` aims the pointer elsewhere
 * for exactly one run.
 *
 * The twelve stores are `v34handshakinit`'s mode-4 record, value for value:
 * an eight-bit message with an eight-bit word, its CRC armed, twelve bits of
 * 0xf72 in hand and no repeat.  They are written in the object's order here,
 * which is not that one's.
 */
static void
tx1_moh_send(struct v34_object *o)
{
	struct v34_bitsource *b;

	if (o->short_abe2 != 3)
		o->short_abe2 = 1;
	if (*((unsigned char *)o + TX1_MOH_PATH_SEL) == 0) {
		if (dsplibs_debug_level > 1)	/* 0x7041b */
			dsplibs_debug_printf(
				"MOH: MHnack received for MHreq," " sending MHfrr\r\n");
		o->moh_message = 1;		/* 0x70430 */
	} else {
		if (dsplibs_debug_level > 1)	/* 0x6a400 */
			dsplibs_debug_printf(
				"MOH: MHnack received for MHreq," " sending MHcda\r\n");
		o->moh_message = 3;		/* 0x6a415 */
	}

	/* 0x6a427 */
	VPcmV34SetMohMessageBits(o, (short *)tx1_bitsource(o));
	*(short **)((char *)o + TX1_PTR_AA6C) =
		(short *)((char *)o + TX1_BLK_A94C);

	/* 0x6a44e */
	b = (struct v34_bitsource *)((char *)o + TX1_BLK_A94C);
	b->crc = (short)0xffff;
	b->pos = 0;
	b->idx = 0;
	b->repeats = 0;
	b->nbits = 8;
	b->wordbits = 8;
	b->crc_on = 1;
	b->acc = 0xf72;
	b->acc0 = 0xf72;
	b->avail = 0xc;
	b->avail0 = 0xc;
	b->repeat = 0;
}

/*
 * 0x64fec -- the hold tail, which both Modem-on-Hold paths fall into.
 *
 * `rtd` is a short and reaches the compare through `movswl` and an ARITHMETIC
 * `sar $0x4` (0x64ff3, 0x65001), so a negative round-trip delay pulls the
 * threshold below zero rather than above four hundred million.  That is the
 * one reading a positive `rtd` cannot tell apart, and `t_v34hstx1.c` drives
 * it negative.
 *
 * The three ways out are tested in this order and no other: +0xabf9, then
 * `moh_message` inside 2..3 as ONE unsigned compare of `moh_message - 2`
 * against one (0x65025), then everything else.
 */
static int
tx1_moh_hold(struct v34_object *o)
{
	if ((int)o->vect_idx < ((int)o->rtd >> 4) + 0x4b0)
		return V34TX1_LOOP;			/* 0x6431f */

	if (*((unsigned char *)o + TX1_MOH_PATH_SEL) != 0) {
		if (dsplibs_debug_level > 1)		/* 0x6889f */
			dsplibs_debug_printf(
				"MOH: Timeout waiting for MH sequence under"
				" MHreq, disconnecting...\r\n");
		/* 0x688b4 */
		tx1_moh_cleardown(o);
		o->short_abe2 = 1;
		return V34TX1_LOOP;			/* 0x629cf */
	}

	if ((unsigned)(o->moh_message - 2) > 1u) {
		/* 0x6923d */
		tx1_moh_reinit(o,
			       "MOH: Timeout waiting for MH sequence under"
			       " MHreq, initiating retrain\r\n");
		return V34TX1_LOOP;			/* 0x629cf */
	}

	if (dsplibs_debug_level > 1)			/* 0x65031 */
		dsplibs_debug_printf(
			"MOH: Timeout waiting for MH sequence under"
			" cleardown, terminating connection without" " acknowledge\r\n");

	/* 0x65046 */
	tx1_moh_cleardown(o);
	return V34TX1_LOOP;				/* 0x64326 */
}

int
v34tx1_tx_dpsk(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	short bit;

	/* 0x62b9d */
	if (*((unsigned char *)o + TX1_MOH_ACTIVE) != 0)
		o->vect_idx = (short)((unsigned short)o->vect_idx + 1);

	/* 0x62bb5, and 0x684bb where it does not inline */
	bit = getbit(tx1_bitsource(o));
	if (bit >= 0) {
		/* 0x64a13 */
		tx1_dpsk_tone(o, bit);
		txmit(o);				/* 0x64a42 */
		return V34TX1_LOOP;			/* 0x640a1 */
	}

	/* 0x64e5c: the message is over.  The flag is RE-READ (0x64e63) */
	if (*((unsigned char *)o + TX1_MOH_ACTIVE) == 0) {
		/*
		 * 0x654dd.  The compare against TONE_AB cannot be false, for
		 * finding F342's reason at 65: the arm is reached only through
		 * table 1 at `txstate == 24` and nothing between the dispatch
		 * and here writes +0x3596 -- `getbit` does not.  It is
		 * written as the object writes it.
		 */
		hs_setstate(o, TX1_TXSTATE, V34HS_TONE_AB);
		return V34TX1_LOOP;			/* 0x63948 */
	}

	if (dsplibs_debug_level > 1)			/* 0x68704 */
		dsplibs_debug_printf(
			"End of current MOH msg: isterm=%d, count1(%d)," " pktcount(%d)...\r\n",
			*((signed char *)o + TX1_MOH_MSG_PENDING), o->vect_idx,
			tx1_bitsource(o)->repeats);

	if (*((unsigned char *)o + TX1_MOH_MSG_PENDING) == 0) {
		/*
		 * 0x67c4e.  The reader is re-armed BY HAND and read again --
		 * `getbit`'s own restart arm with the `repeat` test taken out,
		 * so a reader that declines to repeat is restarted anyway.
		 * The stores are the object's, in the object's order; the
		 * compiler sank the rest of them into the successors, which is
		 * why 0x6a855 and 0x6c9c8 store 0xffff into `crc` again.
		 */
		struct v34_bitsource *b = tx1_bitsource(o);

		b->repeats = (short)((unsigned short)b->repeats + 1);
		b->crc = (short)0xffff;
		b->pos = 0;
		b->idx = 0;
		b->acc = b->acc0;
		b->avail = b->avail0;

		tx1_dpsk_tone(o, getbit(b));
		txmit(o);				/* 0x67d3e */
		return tx1_moh_hold(o);			/* 0x64fec */
	}

	/*
	 * 0x64e91.  Both selectors are read as ints and compared UNSIGNED --
	 * `cmp $0x1 ; je ; jb` picks zero alone and `cmp $0x3 ; ja` sends a
	 * negative one to the same place a large one goes.
	 */
	if (o->moh_message == 1) {
		tx1_moh_on_hold(o);			/* 0x689f6 */
	} else if (o->moh_message == 0) {
		int got = o->moh_recvd;			/* 0x6901d */

		if (got == 4)
			tx1_moh_on_hold(o);
		else if ((unsigned)got > 4u) {
			/* 0x6a3c6 */
			if (got != 5)
				tx1_moh_reinit(o, MOH_ILLEGAL);	/* 0x69041 */
			else
				tx1_moh_send(o);	/* 0x6a3cf */
		} else if (got == 0)
			tx1_moh_on_hold(o);
		else
			tx1_moh_reinit(o, MOH_ILLEGAL);	/* 0x69041 */
	} else if ((unsigned)o->moh_message <= 3u) {
		tx1_moh_cleardown(o);			/* 0x64eaf */
	}

	/* 0x64fde */
	*((unsigned char *)o + TX1_MOH_MSG_PENDING) = 0;
	return tx1_moh_hold(o);				/* 0x64fec */
}

/*
 * ---------------------------------------------------------------------------
 * 66 `TRNSEG4A`, 0x62e28, with the sixteen-point half at 0x66e59 and the
 * segment's completion at 0x62f22.
 *
 * THE SYMBOL IS 71's AND 86's, NOT 69's.  The generator is chosen on
 * `role == 0x65` -- the object carries the scrambler loop twice, at 0x62e70
 * with the 0x04000000 tap and at 0x6358c with 0x00002000, and picks between
 * the copies exactly as 71 and 86 do -- so `tx1_scramble2` is the same call
 * here.  69 chooses on bit 0 of `tx_flags` instead (finding F340 against 423);
 * one halfword apart and it is the whole difference between the two shapes.
 *
 * THE CONSTELLATION IS 69's.  The receiver's +0x11e against 0x89b0 is the
 * arm's first instruction, and the sixteen-point half scrambles TWICE: the
 * first call's two bits go into `cur_quadrant` and are READ BACK FROM THERE
 * (0x66f4d) as `vect16`'s row, with the second call's as the column.  Where
 * 69 differs is that neither half here differentially encodes and neither
 * writes `prev_quadrant` -- the quadrant this arm carries is the raw scrambler
 * output, and `prev_quadrant` is written once, at the completion, out of `cur_quadrant`.
 *
 * AND NEITHER HALF ADVANCES `vect_idx`.  This arm counts in `seg_symcount`, and it
 * counts BEFORE the segment-end compare rather than after it: 0x62edc reads
 * +0x25c0, increments, stores, and the STORED value is what 0x62f09 tests.
 * 86 increments last, at the shared 0x6430c; do not read the two as one
 * shape.
 *
 * THE SEGMENT ENDS ON THREE CONDITIONS AND NOT ONE.  With `n` the count just
 * stored and `lim` the length `baud + baud/2 + period` out of the rate
 * configuration at +0xaa84:
 *
 *     n == lim                                        the segment is over
 *     n  > lim  and  hs_mode > 3                        likewise
 *     n  < lim  and  hs_mode > 3  and  n >= baud+period
 *               and  rx->equerr <= rx+0x250 + 10        likewise (0x6559c)
 *
 * so a run that has passed the nominal length finishes at once, and a run
 * that has passed the SHORTER threshold finishes early when the equaliser
 * error at +0x21a has come down to within ten of the mark at +0x250.
 * `hs_mode` (`TX1_HS_MODE`, +0x2218) is the same int table 2's tail reads and
 * the compare is UNSIGNED (`cmpl $0x3 ; jbe`), so a negative value is a large
 * one here.
 *
 * `baud >> 1` IS AN ARITHMETIC SHIFT (0x62f02 is `sar`), not a divide: with
 * a negative `baud` the two differ, and the fixture's fill makes them differ.
 *
 * THE FOUR REJOINS ARE ALL THE SAME BLOCK.  0x63941, 0x6409a, 0x6431f and
 * 0x62d70 each reload the object, re-test the queue count against +0x2aa0
 * and jump to 0x629e7; the addresses are kept in the comments because the
 * blocks are distinct in the object, not because the exits differ.
 *
 * ---------------------------------------------------------------------------
 * THE COMPLETION AT 0x62f22 IS TWO THIRDS OF THE ARM and it is one job: it
 * settles the RECEIVE half of the rate configuration at +0xaa84 and publishes
 * the answer in the INFO capability record at +0xaa3c.  `tx1_ts_snapshot` is
 * its first half and `tx1_ts_rates` its second.
 *
 * `cfg->rx_divtab` IS `initdigital`'s TABLE, INDEXED `initdigital`'s WAY.
 * v34shell.c:1751 reads `rx_divtab[(short)bits + 14 * (short)umax - 1]` and
 * the four sites here read the same expression at -2, -1 and 0 -- so the
 * ladder below walks the same per-rate divisor list the receive context is
 * eventually brought up with, one rate at a time from the top.
 *
 * THE MULTIPLIER IS ONE PREDICATE WRITTEN TWO WAYS.  0x6308b and 0x6316f test
 * `rate >= 0xd` with `setge`; 0x631f6 and 0x6327a test `rate <= 0xc` with
 * `jle`.  They are the same question and `tx1_ts_scale` asks it once.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE ARM REQUIRES OF THE OBJECT, and it is not a style note.
 *
 * `cfg->rx_baud` MUST BE ONE OF 0x960, 0xaf0, 0xbb8, 0xc80 OR 0xd65.  The
 * five-way at 0x62f9c is the ONLY writer of the two locals the whole
 * completion then runs on -- the rate and its floor -- and it has no default:
 * on any other baud the object falls into 0x62fce reading two uninitialised
 * stack slots, and there is nothing in `v34handshak`'s prologue (0x628f0,
 * which writes 0x74, 0x4c and 0x78 and no more) that ever set them.  So that
 * is not a behaviour to reconstruct, it is a precondition the caller meets;
 * `t_v34hstx1.c` pins the field on every run and says so.  A default written
 * here would be a line no test could disagree with.
 *
 * AND THE FLOOR IS ALWAYS BELOW THE RATE, so the ladder always turns at least
 * once: 9 against 1 and 10..13 against 2, whichever way 0xa97e bit 2 goes.
 * That is what stops the term the ladder computes being read before it is
 * written at 0x631a1, and it is a property of those five pairs rather than of
 * the fixture.
 */

/*
 * +0x4b4, which is the RECEIVER's +0x250 -- `0x74(%esp)` plus 0x250 -- and
 * lands in that structure's `pad_250`, so it is not a named field anywhere in
 * this tree.  The completion writes it out of `preerr` or `equerr`; the early
 * finish above reads it as the mark the equaliser error is measured against,
 * and the ladder reads it as the level a rate has to stay under.
 */
#define TX1_RX250	0x4b4

/*
 * +0x359a, one more halfword of `unmapped_3564`.  v34pcmmain.cpp calls it
 * `OB_FORCE_LOW_BAUD` after `probeselect`'s use of it; here it is the first
 * of four conditions deciding whether the receiver's predictor is kept, and
 * this arm neither writes it nor asserts that meaning.
 */
#define TX1_F359A	0x359a

/*
 * +0xa97e, read as a BYTE and tested for bit 2.  It gates the one adjustment
 * the completion makes to the rate the baud implies.  Inside `unmapped_a948`;
 * no other site in this tree reads it.
 */
#define TX1_FA97E	0xa97e

/*
 * +0xe84, twelve shorts of `unmapped_0e84`.  `initdigital` hands exactly this
 * address to `initV34` as the RECEIVE context's coefficient array
 * (v34shell.c:1767), and the snapshot below fills it from the receiver's
 * predictor -- six taps as a conjugate pair and then straight, which is the
 * complex product `(b + ja)` written out.  Finding F424 is the other reader:
 * `initV34` stores an interior self-pointer to it at +0x0a24.
 */
#define TX1_RXCOEFF	0xe84

/*
 * The receiver's three-tap complex predictor at +0x288, read as ONE RUN OF
 * SIX rather than as `pred_b[3]` and `pred_a[3]`: the sum at 0x672de indexes
 * it with a single counter from zero to five across both halves, so the two
 * arrays' adjacency is what that loop is written on.  Reached by offset for
 * that reason -- `pred_b[4]` would be out of bounds and would be a claim the
 * object does not make.
 */
#define TX1_RX_PRED	0x288

/* Two 32-bit constants of the record, at +0xaa60 and +0xaa68. */
#define TX1_FAA60	0xaa60
#define TX1_FAA68	0xaa68

static short
tx1_ts_scale(const struct v34_ratecfg *cfg, int rate, int off)
{
	int x = cfg->rx_divtab[rate + 14 * (int)cfg->rx_use_max + off];

	x >>= 5;
	x *= x;
	x = (short)x;
	x *= (rate >= 0xd) ? 0x4268 : 0x3a98;
	return (short)(x >> 14);
}

/*
 * 0x672c7: what becomes of the receiver's predictor.  (0x62f5f, bit 5 of the
 * flags word, is the short way out and is written in the arm itself.)
 *
 * FOUR CONDITIONS DECIDE WHETHER THE PREDICTOR IS KEPT, in this order and all
 * of them 16-bit: +0x359a clear, the equaliser error at +0x21a no more than
 * 0x1ff, the predictor error at +0x224 strictly below it, and the sum of the
 * six coefficients' magnitudes no more than 0x3fff.  The sum is FORCED to
 * 0x5000 first when it is already past 0x1f40 and the error is both large and
 * improving, which is that last test's real trigger: the object computes a
 * number and then replaces it with one that cannot pass.
 *
 * KEPT means three things -- the six coefficients go into the record at
 * +0xaa40 in the order (b2, a2, b1, a1, b0, a0) and are then bit-reversed
 * over SIXTEEN bits in place, the same six fill the receive context's
 * coefficient array as a conjugate pair, and the predictor's two histories
 * are cleared.  THROWN AWAY means those six record words go to zero.  Either
 * way bit 12 of the flags comes down and the mark at +0x250 moves; only the
 * kept path raises bit 14, and the two branches move the mark to two
 * different fields.
 */
static void
tx1_ts_snapshot(struct v34_object *o, struct v34_receiver *rx, short *rec)
{
	short *co = (short *)((char *)o + TX1_RXCOEFF);
	int sum = 0, i, t;

	rec[0] = (short)0x8000;
	for (i = 0; i <= 5; i++) {
		int v = tx1_get(rx, TX1_RX_PRED + 2u * (unsigned)i) >> 2;

		sum += (v < 0) ? -v : v;
	}

	t = (short)((unsigned short)rx->equerr - (unsigned short)rx->preerr);
	if (t <= 0)
		t = rx->preerr;				/* 0x6874c */
	t = (short)(t - (rx->equerr >> 2));
	if (sum > 0x1f40 && rx->equerr > 0xc8 && t > 0)
		sum = 0x5000;

	if (tx1_get(o, TX1_F359A) == 0 && rx->equerr <= 0x1ff
	    && rx->preerr < rx->equerr && sum <= 0x3fff) {
		/* 0x6738c */
		if (dsplibs_debug_level > 1)		/* 0x6adf6 */
			dsplibs_debug_printf(
				"V34DATARATE, precoefs [%d,%d,%d][%d,%d,%d]\n",
				tx1_get(rx, TX1_RX_PRED + 0),
				tx1_get(rx, TX1_RX_PRED + 2),
				tx1_get(rx, TX1_RX_PRED + 4),
				tx1_get(rx, TX1_RX_PRED + 6),
				tx1_get(rx, TX1_RX_PRED + 8),
				tx1_get(rx, TX1_RX_PRED + 10));

		rec[2] = rx->pred_b[2];
		rec[3] = rx->pred_a[2];
		rec[4] = rx->pred_b[1];
		rec[5] = rx->pred_a[1];
		rec[6] = rx->pred_b[0];
		rec[7] = rx->pred_a[0];

		co[7] = rec[2];
		co[0] = rec[2];
		co[1] = (short)-rec[3];
		co[6] = rec[3];
		co[9] = rec[4];
		co[2] = rec[4];
		co[3] = (short)-rec[5];
		co[8] = rec[5];
		co[11] = rec[6];
		co[4] = rec[6];
		co[5] = (short)-rec[7];
		co[10] = rec[7];

		for (i = 0; i <= 5; i++)
			rec[2 + i] = (short)bitreverse(
					(unsigned short)rec[2 + i], 0x10);

		rx->flags = (unsigned short)(rx->flags | 0x4000u);
		sysdep_memset(&rx->pred_i[0], 0, 0x10);
		tx1_put(o, TX1_RX250, rx->preerr);
	} else {
		/* 0x674e5 */
		if (dsplibs_debug_level > 1)		/* 0x683b2 */
			dsplibs_debug_printf(
				"V34DATARATE, precoefs=0, 0, 0, 0, 0, 0\n");

		/*
		 * Rolled, this is:
		 *     for (i = 2; i <= 7; i++)
		 *             rec[i] = 0;
		 * -- the six zeros the debug line above names as precoefs.
		 */
		rec[2] = 0;
		rec[3] = 0;
		rec[4] = 0;
		rec[5] = 0;
		rec[6] = 0;
		rec[7] = 0;
		tx1_put(o, TX1_RX250, rx->equerr);
	}

	/* 0x674c8 */
	rx->flags = (unsigned short)(rx->flags & ~V34_RX_FLAG_PREDICT);
}

/*
 * 0x62f82 to 0x63587: the receive rate, settled and published.
 *
 * The baud gives a starting rate and a floor.  The ladder then walks the rate
 * DOWN while the divisor table's term for it is still no more than the mark
 * at +0x250, so it stops at the highest rate the line is measured to carry;
 * the receiver's +0x25e and +0x260 clamp it from either side, and a stale
 * sample clock -- the running count at +0x238 within 96,000 of the mark at
 * +0x248, with `hs_mode` (+0x2218) low -- takes two more off it.  What comes out lands in
 * `cfg->rxbits`, is clamped again to the object's own `rate_min` and
 * `rate_max`, is overridden outright by `rate_want`, and is finally
 * bit-reversed into the capability word beside the transmit rate.
 *
 * THE TWO NIBBLES SWAP BY ROLE, exactly as `initdigital` swaps them
 * (v34shell.c:1568): `role == 0x65` puts the receive rate at bit 6 and the
 * transmit rate at bit 10, and any other value the other way round.  The
 * object open-codes `bitreverse(..., 4)` at 0x6346b and 0x677a8 as a chain of
 * shifts and adds; the two forms agree over all 65,536 inputs and the call is
 * written here, which is finding F424's treatment of `getbit` at 67.
 *
 * AND THE UPSTREAM CAP IS APPLIED LAST.  If the nibble just written asks for
 * more than `VPcmV34GetMaxUpstreamRateIndex` allows, the four bits are
 * replaced one at a time -- `mask` walking 0x40, 0x80, 0x100, 0x200 or the
 * same four shifted up by four -- from `(cap * 7) >> 14`, which is
 * v34pcmmain.cpp:318's divide by 2340 rather than by 2400.
 *
 * `rec[k]` IS THE OBJECT'S OWN ADDRESSING: one base register at +0xaa3c and a
 * byte displacement, so `rec[8]` is +0xaa4c and `rec[21]` is +0xaa66.
 *
 * THE TXSTATE COMPARE AT 0x634b3 CANNOT BE FALSE, for finding F342's reason at
 * 65: the arm is reached only through table 1 at `txstate == 66`, and neither
 * `txmit` nor anything else between the dispatch and here writes +0x3596.  It
 * is written as the object writes it.
 */
static int
tx1_ts_rates(struct v34_object *o, struct v34_receiver *rx,
	     struct v34_ratecfg *cfg, short *rec)
{
	int rate = 0, ratemin = 0, term = 0, cap, v, mask, k;
	short baud;

	/* 0x62f82 */
	cfg->rx_use_max = 1;
	baud = cfg->rx_baud;
	switch ((unsigned short)baud) {
	case 0x960: ratemin = 1; rate = 9;  break;
	case 0xaf0: ratemin = 2; rate = 11; break;
	case 0xbb8: ratemin = 2; rate = 12; break;
	case 0xc80: ratemin = 2; rate = 13; break;
	case 0xd65: ratemin = 2; rate = 14; break;
	}

	/* 0x62fce */
	if (cfg->rx_use_max != 0)
		rec[0] = (short)((unsigned short)rec[0] | 2u);

	if ((*((unsigned char *)o + TX1_FA97E) & 4) == 0) {
		/* 0x62ff8 */
		if (baud == 0x0c80 || baud == 0x0d65)
			rate = 0xc;
		else if (baud == 0x0bb8 || baud == 0x0af0)
			rate = (short)(rate - 1);	/* 0x67984 */

		/*
		 * 0x63025, and NOT part of the 3200/3429 arm: 0x6797e and
		 * 0x67994 both jump back to the guard, so every baud that
		 * reaches this block reports, including the ones that changed
		 * nothing.  Placing it inside the `rate = 0xc` branch left
		 * four cases short -- 2400, 2800, 3000 and the ladder's
		 * floor -- which is how the difference was found.
		 */
		if (dsplibs_debug_level > 1)		/* 0x6834b */
			dsplibs_debug_printf(
				"V34INFO, V.34bis is not possible \n");
	}

	/* 0x6302e */
	cfg->txbits = (short)rate;
	while ((short)rate > (short)ratemin) {
		term = tx1_ts_scale(cfg, rate, -1);
		if (dsplibs_debug_level > 1)		/* 0x66b61 */
			dsplibs_debug_printf(
				"V34DATARATE,threshold for data rate" " %d = %d\n", rate, term);
		if (tx1_get(o, TX1_RX250) < (short)term)
			break;
		rate = (short)(rate - 1);
	}

	/* 0x630e9 */
	if (rx->rrn_local_dir > 1) {
		int d = rx->baud_copy;

		if (rx->rrn_local_dir == 2 && rate > d - 1) {
			rate = (short)(d - 1);		/* 0x68308 */
			if (dsplibs_debug_level > 1)	/* 0x68334 */
				dsplibs_debug_printf(
					" TRNSEG4A : returning from local rrn" " down => forcing rate down\n");
		} else if (rate < d + 1) {
			rate = (short)(d + 1);		/* 0x63120 */
			if (dsplibs_debug_level > 1)	/* 0x69100 */
				dsplibs_debug_printf(
					" TRNSEG4A : returning from local rrn" " up => forcing rate up\n");
		}
		term = tx1_ts_scale(cfg, rate, -1);	/* 0x63134 */
	}

	/* 0x63198 */
	if (dsplibs_debug_level > 1)			/* 0x6759e */
		dsplibs_debug_printf(
			"V34DATARATE, ethresh data rate = %d,ethreh=%d," "rate2 = 0x%x,data=%d\n",
			rate, tx1_get(o, TX1_RX250),
			(unsigned short)tx1_get(o, TX1_F382), term);
	if (dsplibs_debug_level > 1)			/* 0x675e6 */
		dsplibs_debug_printf("V34DATARATE, equerr = %d,preerr=%d\n",
				     rx->equerr, rx->preerr);

	rx->bad_thresh = (short)(2 * term);
	if ((short)rate > (short)ratemin) {
		rx->bad_long_thresh = tx1_ts_scale(cfg, rate, -2);
		rx->bad_long_thresh = (short)((rx->bad_long_thresh + term) >> 1);
	} else {
		/* 0x672ad */
		rx->bad_long_thresh = (short)(tx1_get(o, TX1_RX250) << 3);
	}

	/* 0x63234 */
	if ((short)cfg->txbits > (short)rate)
		rx->good_thresh = tx1_ts_scale(cfg, rate, 0);
	else
		rx->good_thresh = 0;				/* 0x6729b */

	/* 0x63299 */
	rx->bad_long_run = 0;
	rx->good_run = 0;
	rx->bad_run = 0;
	rec[1] = (short)0xfffd;
	if ((unsigned)tx1_get_int(o, TX1_TIMER)
	    < (unsigned)(tx1_get_int(o, TX1_TIMER_MARK) + 0x17700)
	    && (unsigned)tx1_get_int(o, TX1_HS_MODE) <= 3u) {
		rate = (short)(rate - 2);
		if ((short)rate < (short)ratemin)
			rate = ratemin;
		cfg->txbits = (short)rate;
		rec[1] = (short)((unsigned short)rec[1] & ~1u);
	}

	/* 0x6332c */
	cfg->rxbits = (short)rate;
	if (dsplibs_debug_level > 1)			/* 0x67741 */
		dsplibs_debug_printf(
			"V34DATARATE, automatic: %d, min %d, max %d\n",
			0x960 * rate, 0x960 * o->rate_min,
			0x960 * o->rate_max);

	/* 0x6334a */
	v = cfg->rxbits;
	if (v < o->rate_min) {
		cfg->rxbits = (short)o->rate_min;
		v = (short)o->rate_min;
	}
	if (v > o->rate_max)
		cfg->rxbits = (short)o->rate_max;
	if (o->rate_want >= 0 && o->rate_want != o->rate_now)
		cfg->rxbits = (short)o->rate_want;

	/*
	 * Bench-only rate cap.  This is deliberately a compile-time switch: the
	 * normal reconstruction, including the differential build, retains the
	 * blob's exact outgoing-MP choice.  It lets the interop harness distinguish
	 * an early data-phase failure caused by the rate proposed to the peer from
	 * one which occurs independently of that proposal.
	 */
#ifdef DSPLIB_V34_TEST_TX_RATE_CAP
	if (cfg->txbits > DSPLIB_V34_TEST_TX_RATE_CAP)
		cfg->txbits = DSPLIB_V34_TEST_TX_RATE_CAP;
#endif

	/* 0x633b5 */
	rate = (short)bitreverse((unsigned short)cfg->rxbits, 4);
	tx1_put(o, TX1_F358C,
		(short)bitreverse((unsigned short)cfg->txbits, 4));
	if (dsplibs_debug_level > 1)			/* 0x676f3 */
		dsplibs_debug_printf(
			"V34DATARATE, Final choice data rate = %d,"
			" retrainThresh = %d, renegDownthresh = %d," " renegUpthresh = %d\n",
			cfg->rxbits, rx->bad_thresh, rx->bad_long_thresh, rx->good_thresh);

	/* 0x6340f */
	if (o->role == 0x65)
		rec[0] = (short)((unsigned)(unsigned short)rec[0]
				 | ((unsigned)tx1_get(o, TX1_F358C) << 10)
				 | ((unsigned)rate << 6));
	else {
		rec[0] = (short)((unsigned)(unsigned short)rec[0]
				 | ((unsigned)rate << 10));
		rec[0] = (short)((unsigned)(unsigned short)rec[0]
				 | ((unsigned)tx1_get(o, TX1_F358C) << 6));
	}

	/* 0x63453 */
	cap = VPcmV34GetMaxUpstreamRateIndex(o);
	if (o->role == 0x65) {
		/* 0x677a8 */
		v = bitreverse((unsigned short)
			       (((unsigned)(unsigned short)rec[0] >> 10) & 0xf),
			       4);
		mask = ~0x400;
	} else {
		/* 0x6346b */
		v = bitreverse((unsigned short)
			       (((unsigned)(unsigned short)rec[0] >> 6) & 0xf),
			       4);
		mask = ~0x40;
	}
	if (v * 0x960 > cap) {
		/* 0x6768b */
		int rev = (short)bitreverse((unsigned short)((cap * 7) >> 14),
					    4);

		for (k = 0; k <= 3; k++) {
			unsigned short w = (unsigned short)rec[0];

			if ((rev >> k) & 1)
				w = (unsigned short)(w | (unsigned)~mask);
			else
				w = (unsigned short)(w & (unsigned)mask);
			rec[0] = (short)w;
			mask = (short)(mask * 2 + 1);
		}
	}

	/* 0x634a6 */
	rec[8] = 0;					/* +0xaa4c */
	hs_setstate(o, TX1_TXSTATE, V34HS_XMITMP);
	o->vect_idx = 0;
	tx1_put(o, TX1_F3598, 0);
	tx1_put(o, TX1_F359E, 0);
	if (dsplibs_debug_level > 1)			/* 0x67653 */
		dsplibs_debug_printf(
			"V34DATARATE, txmp bits 0x%x,0x%x,0x%x,0x%x,0x%x\n",
			(unsigned short)rec[0], (unsigned short)rec[1],
			(unsigned short)rec[2], (unsigned short)rec[3],
			(unsigned short)rec[4]);

	/* 0x63510 */
	tx1_put_int(o, TX1_FAA60, 0x3fffe);
	tx1_put_int(o, TX1_FAA68, 0x3fffe);
	rec[10] = -1;					/* +0xaa50 */
	rec[13] = 0;					/* +0xaa56 */
	rec[15] = 0;					/* +0xaa5a */
	rec[17] = 0;					/* +0xaa5e */
	rec[14] = 0x10;					/* +0xaa58 */
	rec[12] = (short)((rx->flags & V34_RX_FLAG_RENEG) ? 0x30 : 0x90);
	rec[11] = 1;					/* +0xaa52 */
	rec[20] = 0x12;					/* +0xaa64 */
	rec[21] = 0x12;					/* +0xaa66 */
	rec[16] = 0;					/* +0xaa5c */
	tx1_put(o, TX1_F3590, 0x22);
	return V34TX1_LOOP;				/* 0x62d70 */
}

int
v34tx1_trnseg4a(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx =
		(struct v34_receiver *)((char *)objp + TX1_RECEIVER);
	struct v34_ratecfg *cfg =
		(struct v34_ratecfg *)((char *)objp + V34_RATECFG);
	int n, lim, baud, period;
	short *rec;

	if ((unsigned short)tx1_get(o, TX1_F382) == 0x89b0u) {
		/* 0x66e59 */
		short k = tx1_scramble2(o);
		short q;

		o->cur_quadrant = k;
		q = tx1_scramble2(o);
		tx1_put_point(o, vect16[q + 4 * k]);
	} else {
		/* 0x62e3b */
		short q = tx1_scramble2(o);

		o->cur_quadrant = q;
		tx1_put_point(o, vect4[q]);
	}

	/* 0x62ecd */
	txmit(o);

	/* 0x62edc */
	o->seg_symcount = (short)((unsigned short)o->seg_symcount + 1);
	n = o->seg_symcount;
	baud = cfg->baud;
	period = cfg->period;
	lim = baud + (baud >> 1) + period;

	if (n != lim) {
		if ((unsigned)tx1_get_int(o, TX1_HS_MODE) <= 3u)
			return V34TX1_LOOP;		/* 0x63941 */
		if (n < lim) {
			/* 0x6559c */
			if (n < baud + period)
				return V34TX1_LOOP;	/* 0x6409a */
			if ((int)rx->equerr > (int)tx1_get(o, TX1_RX250) + 10)
				return V34TX1_LOOP;	/* 0x6431f */
		}
	}

	/* 0x62f22 */
	rec = (short *)((char *)objp + TX1_FAA3C);
	o->prev_quadrant = (short)(unsigned short)o->cur_quadrant;
	memcpy((char *)objp + TX1_PTR_AA6C, &rec, sizeof(rec));

	if (rx->flags & V34_RX_FLAG_RENEG) {
		/* 0x62f5f */
		rec[2] = 0;
		rec[0] = 0;
		tx1_put(o, TX1_RX250, rx->equerr);
	} else {
		tx1_ts_snapshot(o, rx, rec);
	}

	return tx1_ts_rates(o, rx, cfg, rec);
}

/*
 * ---------------------------------------------------------------------------
 * 21 `TRNSEG4`, 0x64339, with the three-way compare at 0x64ad9, the
 * echo-adapt clear-down at 0x67613, the counter's two forms at 0x6801e and
 * 0x64b96, and the two Ja tails at 0x64de7 and 0x67f80.
 *
 * ONE SCRAMBLED SYMBOL PER PASS AND A COUNTER READ FOUR WAYS.  The head is
 * 71 `TXLEVEL`'s and 86 `TXMD`'s: two bits of the literal 3 through the
 * shift register at +0x25cc, the quadrant into `cur_quadrant`, one `vect4` point
 * out.  The object carries the loop twice -- 0x6436e with the 0x04000000 tap
 * and 0x64440 with 0x00002000 -- and picks between the copies on
 * `role == 0x65`, which is `V34scrambler`'s `mode` exactly, so it is the
 * same `tx1_scramble2` the other two use.
 *
 * Then `seg_symcount` is incremented and compared against FOUR different things, in
 * this order, and only the last of them ends the segment:
 *
 *     +0xaa78            the echo-adapt START point -- clear four fields
 *                        and report, then CARRY ON down the chain
 *     +0xaa86            the "period", which only raises a flag
 *     +0xa244 / 2        the echo-adapt MIDDLE point -- report and leave
 *     baud + baud/2      raise 0x400 and clear bit 15, and leave
 *     +0xa244            the segment is over: the completion below
 *
 * THE FIRST IS NOT AN EXIT AND THE OTHER FOUR ARE, which is the shape a
 * reconstruction can get wrong without any test noticing: 0x67613 ends in a
 * `jmp 0x643f8`, back into the chain, where 0x67278 and 0x67734 end in loop
 * rejoins.  `+0xa244` is read as an INT and `seg_symcount` is sign-extended before
 * every one of the three compares against it, so the arithmetic is 32-bit
 * even though both fields are written as halfwords.
 *
 * `baud + baud/2` IS THE RATE CONFIGURATION'S BAUD AT +0xaa84 and not the
 * receive baud at +0xaa96 that the completion switches on.  They are two
 * fields and this arm reads both, which is why they are seeded apart in the
 * test rather than left to agree.
 *
 * ---------------------------------------------------------------------------
 * THE COMPLETION IS `setupreceiver` INLINED, and recognising that is what
 * turns 0x64c04..0x64d4a into one call.  Store for store it is that function:
 * `rxinit`, `out_count = 4`, the six-way switch on +0xaa96 that sets phase_wrap, phase_inc,
 * symbol_period and phase_frac, the eight-way switch on +0xaaa8 that sets `carrier` and
 * half_len, `agc_step = 0x2000`, `agc_gain = agc_start_gain`, `flags &= 0xf0ff`,
 * `detectorinit(obj+0x3564, *(obj+0xaab0), 0, 8, 10, 0x600, 0)` and
 * `flags |= 0x200` -- the same literals, the same two diagnostics, and both
 * switches without a default.  So the eight `hsine*` tables this arm reaches
 * are reached through `setupreceiver` and no table had to be extracted for
 * it; they are already global in v34filters.c.
 *
 * The +0xaa0c record is blanked next, and it is v34hshak.c:1406's TWELVE
 * FIELDS in a different order -- +0x14 to -1 and the other eleven to zero.
 * Two of them are 32-bit stores and ten are halfwords; written in the object's
 * order here, which is not the record's.
 *
 * ---------------------------------------------------------------------------
 * WHICH Ja TAIL, AND THE TWO TESTS ARE NOT THE SAME TEST.  The counter above
 * takes `rtd` whole when EITHER PCM receiver is non-zero and halves it when
 * neither is; the tail takes the V.90 side when +0x24c is above one and the
 * K56flex side when +0x250 is.  So +0x24c == 1 is a real state: the counter
 * is not halved and neither Ja tail is taken.
 *
 * AND THE ARM'S TESTS ARE UNSIGNED WHERE `indicateJaTransmission`'S ARE
 * SIGNED.  0x64de1 and 0x67f8b are `cmpl $1 ; jbe`; the callee's two at
 * 0xa3f2 and 0xa401 are `cmpl $1 ; jg` (v34pcmmain.cpp says so).  A negative
 * `v90_receiver` therefore reaches `indicateJaTransmission` through this arm
 * and does nothing when it gets there.  Reproduced, not smoothed over.
 *
 * ---------------------------------------------------------------------------
 * FIVE COMPARES HERE CANNOT BE FALSE, for finding F342's reason at 65.  The
 * arm is reached only through table 1 at `txstate == 21` and nothing between
 * the dispatch and 0x64b32 writes +0x3596, so `txstate != 0x40` holds; and
 * 0x64de7's `!= 0x4e`, 0x67f98's `!= 0x55` and 0x64e1b's `!= 0x23` test a
 * word this arm has just written with a different value.  0x64bde's
 * `rxstate != 4` is false only if the caller entered in RECEIVE, which the
 * fixture cannot drive without changing which dispatch the tail takes.  All
 * five are written as the object writes them and each is an equivalent
 * mutation.
 *
 * WHAT IS NOT MODELLED, and it is finding F341's gap and not a new one: the
 * thirteen trace blocks at 0x68d07, 0x68d6f, 0x687f4, 0x68754, 0x68ca4,
 * 0x691b9, 0x691a8, 0x6916d, 0x6917e, 0x69dfa, 0x69b6f, 0x6a998 and 0x6a8e1,
 * and the two `VPcmV34Report*OfEchoAdapt` calls that only print.  The boundary
 * arm's own pair of echo reports IS modelled now: it is `v34FreezeEcho`, and
 * finding F572 is the transcript that proves the call.
 */
int
v34tx1_trnseg4(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_ratecfg *cfg =
		(struct v34_ratecfg *)((char *)objp + V34_RATECFG);
	short q;
	int n, span, baud;

	/* 0x64339: the scrambler, the quadrant and one sample. */
	q = tx1_scramble2(o);
	o->cur_quadrant = q;
	tx1_put_point(o, vect4[q]);
	txmit(o);

	/* 0x643dc */
	o->seg_symcount = (short)((unsigned short)o->seg_symcount + 1);

	if (tx1_get(o, TX1_COUNT) == o->seg_symcount) {
		/* 0x67613, and this one FALLS BACK INTO THE CHAIN */
		o->echo_calls = 0;
		o->tx_flags = (short)((unsigned short)o->tx_flags & ~V34_EC_FROZEN);
		o->echo_alpha = 0;
		o->far_echo_alpha = 0;
		o->echo_energy = 0;
		VPcmV34ReportStartOfEchoAdapt(o);
	}

	/* 0x643f8 */
	if (((unsigned short)o->tx_flags & 0x100u) == 0
	    && o->seg_symcount >= tx1_get(o, TX1_FAA86))
		o->tx_flags = (short)((unsigned short)o->tx_flags | 0x100u);

	/* 0x64ae4 */
	n = o->seg_symcount;
	span = tx1_get_int(o, TX1_FA244);

	if (n == (span >> 1)) {
		VPcmV34ReportMiddleOfEchoAdapt(o);	/* 0x67734 */
		return V34TX1_LOOP;			/* 0x640a1 */
	}

	baud = cfg->baud;
	if (n == baud + (baud >> 1)) {
		/* 0x67278 */
		o->tx_flags = (short)(((unsigned short)o->tx_flags & ~V34_TXFLAG_PPSEG)
				   | 0x400u);
		return V34TX1_LOOP;			/* 0x63da2 */
	}

	if (n != span)
		return V34TX1_LOOP;			/* 0x629c8 */

	/* 0x64b24: the segment is over. */
	hs_setstate(o, TX1_TXSTATE, V34HS_JTXMIT);

	if (o->rtd <= 2) {
		/* 0x6801e */
		v34FreezeEcho(o);
		tx1_put(o, TX1_COUNT, 0);
	} else {
		if (o->v90_receiver != 0 || o->k56flex_receiver != 0) {
			/* 0x68ad0 and 0x69de7, two copies of one store */
			tx1_put(o, TX1_COUNT, o->rtd);
		} else {
			/* 0x64b96 */
			tx1_put(o, TX1_COUNT, (short)(o->rtd >> 1));
		}

		/*
		 * 0x64ba9, and the nesting is what the object says: BOTH
		 * stores jump here (0x68ade and 0x69df5) and the `rtd <= 2`
		 * arm does not -- it reports the freeze instead.  An
		 * else-if chain cannot express a guard shared by two of its
		 * three arms.
		 */
		if (dsplibs_debug_level > 1)		/* 0x6917e */
			dsplibs_debug_printf(
				"V34Hshak: On J TX start, would freeze EC" " after bulk delay (%d samples,"
				" bulk=%d)\r\n",
				tx1_get(o, TX1_COUNT), o->rtd);
	}

	/* 0x64bb2 */
	tx1_put(o, TX1_F25D6, (short)0x8990);
	o->prev_quadrant = o->cur_quadrant;
	hs_setstate(o, TX1_RXSTATE, V34HS_RECEIVE);
	setupreceiver(o);				/* 0x64c04 */

	/* 0x64d37: the +0xaa0c record, in the object's order */
	{
		unsigned char *r = (unsigned char *)objp + TX1_INFOREC;

		*(int *)(r + 0x24) = 0;
		*(int *)(r + 0x2c) = 0;
		*(short *)(r + 0x14) = -1;
		*(short *)(r + 0x1a) = 0;
		*(short *)(r + 0x1e) = 0;
		*(short *)(r + 0x22) = 0;
		*(short *)(r + 0x18) = 0;
		*(short *)(r + 0x1c) = 0;
		*(short *)(r + 0x16) = 0;
		*(short *)(r + 0x28) = 0;
		*(short *)(r + 0x2a) = 0;
		*(short *)(r + 0x20) = 0;
	}

	/* 0x64d8d */
	hs_setstate(o, TX1_MICROSTATE, V34HS_DET_SYNC);

	/* 0x64dba */
	o->vect_idx = 0;
	tx1_put(o, TX1_FAA80, 1);

	if ((unsigned)o->v90_receiver > 1u) {
		/* 0x64de7 */
		hs_setstate(o, TX1_TXSTATE, V34HS_JaTXMIT);
		hs_setstate(o, TX1_RXSTATE, V34HS_WAIT);
		indicateJaTransmission(o);
		return V34TX1_LOOP;			/* 0x629c8 */
	}

	if ((unsigned)o->k56flex_receiver > 1u) {
		/* 0x67f91 */
		hs_setstate(o, TX1_TXSTATE, V34HS_K56JaTXMIT);
		hs_setstate(o, TX1_RXSTATE, V34HS_WAIT);
		indicateJaTransmission(o);
		return V34TX1_LOOP;			/* 0x63948 */
	}

	return V34TX1_LOOP;				/* 0x63da2 */
}

/*
 * v34hstx1.cpp -- seventeen arms of `v34handshak`'s per-sample transmit
 * dispatch.
 *
 * IT IS A `.cpp` WHERE `v34handshak` IS C, which is finding 217's rule rather
 * than a choice.  78 `JaTXMIT` tail-calls `v90Phase34` and 85 `K56JaTXMIT`
 * tail-calls `k56FlexPhase34`; both live in the V.90/V.92 C++ half, and the
 * five SpanDSP interop binaries link `$(SRC)` -- every `.c` under src/ and no
 * C++ at all.  A `.c` file naming either symbol leaves them undefined and
 * takes those five down with it, which is what v34info1a.cpp, v34k56.cpp and
 * v34pcmmain.cpp are `.cpp` for.  Measured here rather than assumed: as a
 * `.c` this file broke all five, and `make phase` was how it said so.
 * Finding 344.
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
 * +0x3a6.  The six fields that region carries are `f25c0`, `f25c2`, `f25c6`,
 * `f25c8`, `f25cc` and `f25d0`, and they are named in `struct v34_object`.
 *
 * ---------------------------------------------------------------------------
 * TWO PAIRS THAT LOOK LIKE ONE ARM AND ARE NOT.
 *
 * 78 `JaTXMIT` and 85 `K56JaTXMIT` are instruction-for-instruction identical
 * -- the same counter, the same flag, the same two `V34EchoReportCoeff`
 * calls -- up to the tail call, which is `v90Phase34` for one and
 * `k56FlexPhase34` for the other.  They are two arms and the object gives
 * them two table entries; finding 340 measures them apart.
 *
 * 71 `TXLEVEL` and 86 `TXMD` share the scrambler step and differ in what they
 * transmit: 71 always sends `vect4[0]` and 86 sends `vect4[q]`, the point the
 * scrambler just chose.  71 advances the register and throws the result away
 * except as `f25c8`.
 *
 * ---------------------------------------------------------------------------
 * AND ONE PAIR THAT AGREES COLD AND IS STILL NOT ONE ARM.
 *
 * Finding 323 measured 24 `TX_DPSK` and 60 `TONE_AB` writing the same 69
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
 * finding 323's 69 bytes for 60 is what `t_v34hstx1.c`'s odd case still
 * writes, where its even case writes 65.  The collision was measured on one
 * of the two paths and the fill decides which -- seed 1 leaves 0xb06e and
 * would have measured the other.  So the agreement is narrower than the
 * table entry, which is the reason both values are poked here rather than
 * one of them being reached by luck.
 */

#include <string.h>

#include "dsplib/v34filt.h"	/* V34EchoReportCoeff, V34SetupModulator */
#include "dsplib/v34fsk.h"	/* struct v34_object, struct v34_ratecfg */
#include "dsplib/v34hshak.h"	/* vect4, v90Phase34, k56FlexPhase34      */
#include "dsplib/v34hstx1.h"
#include "dsplib/v34info.h"	/* V34SetINFO0aBits                       */
#include "dsplib/v34recv.h"	/* struct v34_receiver                    */
#include "dsplib/v34rx.h"	/* txmit, txwritequeue, V34scrambler      */
#include "dsplib/v34shell.h"	/* modulatevector                         */

/* The receiver sub-object; `0x74(%esp)` above. */
#define TX1_RECEIVER	0x264

/* The transmit state machine's own word (finding 213). */
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

/* +0x3594, the RXSTATE (finding 213).  74's retrain is its only writer here. */
#define TX1_RXSTATE	0x3594

/*
 * +0x3592, the MICROSTATE (finding 213).  51 is the one arm here that reads
 * a state word belonging to another machine: it selects between two copies of
 * its own loop on `microstate == TX_L1`, having been dispatched on
 * `txstate == TX_L1`.  The two 51s are the same number in two machines and
 * `StateName` is one table for all three (docs/v34handshak.md).
 */
#define TX1_MICROSTATE	0x3592

/*
 * +0x2218, an int.  Table 2's tail reads it to choose four of its arms
 * (v34hstxblock.c's `TB_F2218`); 70 is a writer of it.
 */
#define TX1_F2218	0x2218

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
 */
#define TX1_FABE8	0xabe8

/*
 * +0x25d6, +0x25d8 and +0x25da -- the three halfwords of `unmapped_25d6` that
 * 64/68 owns, reached off `0x4c(%esp)` as 0x3ba, 0x3bc and 0x3be.
 *
 * +0x25d6 is the BIT SOURCE: 64/68 shifts it right by twice `vect_idx` and
 * hands the bottom two bits to the scrambler, so it is sixteen bits read as
 * eight dibits, and the end of 64's segment reloads it with 0x899f.  +0x25d8
 * counts one per pass and ends the segment past 0x80.  +0x25da is a mode
 * word: 64 goes no further unless it holds 2, and the `f359c == 0x66` path
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
 * 64/68 tests +0x35a2 against zero on its `f359c == 0x66` path and 69 tests
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
 * +0xaa6c and +0xa94c: the self-pointer and the record it is aimed at.
 * `v34handshakinit` aims the same pair on its mode-4 path (v34hshak.c:1451),
 * and 54's completion aims it again before handing the record to
 * `V34SetINFO0aBits`.  The harness excludes +0xaa6c from the byte comparison
 * and checks it by offset instead (finding 324).
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
 * The transmitted point.  `f25d0` and `f25d2` are two shorts and every arm
 * that sends a constellation point writes them with ONE 32-bit store, which
 * is what `vect4` holds -- v34pcmmain.cpp and v34k56.cpp spell it
 * `*(int *)&o->f25d0`.  Spelled with `memcpy` here only because the C front
 * end warns about the type pun where the C++ one does not; it is the same
 * store.
 */
static void
tx1_put_point(struct v34_object *o, int point)
{
	memcpy(&o->f25d0, &point, sizeof(point));
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
 * Send one silent symbol -- `f25d0` and `f25d2` are the two halves of the
 * point `txmit` transmits, and both are cleared before the call -- and then,
 * if the receiver is holding bit 3 of its flags word, arm the segment:
 * raise 0x2000 in `f25c2`, move the transmit machine to SSEG, and clear the
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
 * equivalent mutation and is recorded as one (finding 342).
 */
int
v34tx1_xmit0(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx =
		(struct v34_receiver *)((char *)objp + TX1_RECEIVER);

	o->f25d0 = 0;
	o->f25d2 = 0;
	txmit(o);

	if (rx->flags & V34_RX_FLAG_LATE_TRN) {
		o->f25c2 = (short)((unsigned short)o->f25c2 | 0x2000u);
		if (tx1_get(o, TX1_TXSTATE) != V34HS_SSEG)
			tx1_put(o, TX1_TXSTATE, V34HS_SSEG);
		o->f25c6 = 0;
		o->f25c0 = 0;
		o->f25cc = 0;
	}
	return V34TX1_LOOP;
}

/*
 * ---------------------------------------------------------------------------
 * The scrambler step 71 and 86 share, 0x641de..0x6424e and 0x63db5..0x63e2e.
 *
 * Both arms carry the loop twice -- once with the 0x04000000 tap and once
 * with 0x00002000 -- and choose between the copies with `f359c == 0x65`.
 * That is `V34scrambler`'s `mode` argument exactly: v34rx.c hoists the same
 * branch out of the same loop, over the same register at +0x25cc, with the
 * same two generators.  `bits` is the literal 3 and `nbits` is 2, so two
 * scrambled bits come out and the register advances twice.
 *
 * WHY THE RETURN VALUE IS USABLE THOUGH THE OBJECT DOES NOT MASK.  The arms
 * store `reg >> 29` into `f25c8` unmasked where `V34scrambler` returns
 * `(reg >> 29) & 3`.  The two agree for every input: the loop's last act is
 * `reg >>= 1`, so bit 31 is clear on exit and `reg >> 29` is already 0..3.
 * The masked form is used here because it is the published function; nothing
 * downstream can tell them apart.
 */
static short
tx1_scramble2(struct v34_object *o)
{
	return (short)V34scrambler((unsigned *)&o->f25cc,
				   (short)(o->f359c == 0x65), 3, 2);
}

/*
 * ---------------------------------------------------------------------------
 * 71 `TXLEVEL`, 0x641d1.
 *
 * One scrambler step, and then a symbol that is ALWAYS `vect4[0]`: the load
 * at 0x64257 has no index register, where 86's at 0x63e40 scales the
 * scrambler's output by four.  The quadrant still lands in `f25c8`, so the
 * register and the quadrant advance while the transmitted point does not --
 * which is what a level-measurement segment wants.
 */
int
v34tx1_txlevel(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;

	o->f25c8 = tx1_scramble2(o);
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
 * raise bit 2 of `f25c2` and report both echo cancellers' coefficients.
 * Then hand the sample itself to the phase 3/4 half, which is where the
 * transmitting is done: neither arm calls `txmit`.
 *
 * The two differ in that call and in nothing else.  They are written as one
 * body and two entry points so that the identity is a property of the file
 * rather than a claim in a comment; finding 340 is the measurement that they
 * are still two behaviours, because the two callees are.
 *
 * THE COUNTER IS SIXTEEN BITS.  The object loads it with `movzwl`, tests
 * `%ax`, and stores `%ax` back, so a negative `short` counts down through
 * 0x8000 rather than through zero.  Spelled with an `unsigned short` here
 * for that reason.
 */
static int
tx1_ja_common(struct v34_object *o)
{
	unsigned short c = (unsigned short)tx1_get(o, TX1_COUNT);

	if (c == 0)
		return 0;

	c = (unsigned short)(c - 1);
	tx1_put(o, TX1_COUNT, (short)c);
	if (c != 0)
		return 0;

	o->f25c2 = (short)((unsigned short)o->f25c2 | 4u);
	V34EchoReportCoeff(&o->echo0);
	V34EchoReportCoeff(&o->echo1);
	return 1;
}

int
v34tx1_jatxmit(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;

	(void)tx1_ja_common(o);
	v90Phase34(o);
	return V34TX1_LOOP;
}

int
v34tx1_k56jatxmit(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;

	(void)tx1_ja_common(o);
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
 * At 0xc0 the arm leaves for 0x66d85, which re-reads `txstate` and is not
 * reconstructed here, so the wrap is REPORTED and not followed.  The count
 * is compared as sixteen bits, after the store.
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
	if ((unsigned short)o->vect_idx == 0xc0u)
		return V34TX1_MOH_WRAP;
	return V34TX1_LOOP;
}

/*
 * ---------------------------------------------------------------------------
 * 86 `TXMD`, 0x63dae.
 *
 * Scramble two bits, transmit the point they select, and count the symbol.
 * Then two comparisons of `vect_idx`, in this order:
 *
 *   - against +0xaa78: the segment is over, and the arm leaves for 0x66fe9,
 *     which is not reconstructed here;
 *   - against +0x35a6: the modulator is reconfigured for the negotiated rate
 *     and the transmit machine moves to SSEG.
 *
 * `V34SetupModulator`'s fifth argument is one when EITHER of the two PCM
 * receivers at +0x24c and +0x250 is non-zero -- the object computes it as
 * two tests jumping to one `mov $0x1,%eax` at 0x65696 -- and its sixth is the
 * literal zero.  The three rate fields are the transmit half of the rate
 * configuration at +0xaa84: `baud`, `carrier` and the pre-emphasis index at
 * +0x06, which is the same triple v34hshak.c:522 reads.
 *
 * THE ARM ENDS AT 0x6430c, WHICH IS SHARED, and its one instruction --
 * `f25c0 += 1` -- is written here rather than in the caller because it is
 * the last thing this arm does on both of its in-loop paths.  Another table-1
 * arm reaching 0x6430c will want the same line; the eventual `v34handshak`
 * may factor it.
 */
int
v34tx1_txmd(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	short q = tx1_scramble2(o);

	o->f25c8 = q;
	tx1_put_point(o, vect4[q]);
	txmit(o);

	o->vect_idx = (short)(o->vect_idx + 1);
	if ((unsigned short)o->vect_idx == (unsigned short)tx1_get(o, TX1_COUNT))
		return V34TX1_TXMD_DONE;

	if ((unsigned short)o->vect_idx
	    == (unsigned short)tx1_get(o, TX1_SEGLEN)) {
		struct v34_ratecfg *cfg =
			(struct v34_ratecfg *)((char *)o + V34_RATECFG);
		int pcm = (o->v90_receiver != 0 || o->k56flex_receiver != 0);

		V34SetupModulator((struct v34_modulator *)
				  ((char *)o + TX1_MODULATOR),
				  cfg->baud, cfg->carrier, cfg->f06, pcm, 0);

		if (tx1_get(o, TX1_TXSTATE) != V34HS_SSEG)
			tx1_put(o, TX1_TXSTATE, V34HS_SSEG);
		o->f25c0 = 0;
		o->f25cc = 0;
		o->f25c2 = (short)((unsigned short)o->f25c2 | 0x8004u);
	}

	/* 0x6430c */
	o->f25c0 = (short)(o->f25c0 + 1);
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
 * twice.  Then one tick of `f25c0`, the segment's symbol count, and at 0x40
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
 * 342's reason at 65: this arm is reached only through table 1 at
 * `txstate == 18`, and `txmit` does not write +0x3596 (finding 285's sweep).
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

	n = (unsigned short)((unsigned short)o->f25c0 + 1);
	o->f25c0 = (short)n;
	if (n == 0x40) {
		/* 0x66d11 */
		if (tx1_get(o, TX1_TXSTATE) != V34HS_SBARSEG)
			tx1_put(o, TX1_TXSTATE, V34HS_SBARSEG);
		o->f25c0 = 0;
	}
	return V34TX1_LOOP;
}

/*
 * ---------------------------------------------------------------------------
 * 70 `DATAXMIT`, 0x63ca8.
 *
 * `modulatevector` does all of the transmitting -- it maps one point out of
 * the vector it holds and tail-calls `txmit` -- and the rest of the arm is
 * conditional on bit 4 of `f25c2`.
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
 * dead and the store of 1 happens either way.  Finding 341's gap, again.
 */
int
v34tx1_dataxmit(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx =
		(struct v34_receiver *)((char *)objp + TX1_RECEIVER);
	int idx;

	modulatevector(o);

	if (!(o->f25c2 & 0x10))
		return V34TX1_LOOP;

	rx->f220 = 0;
	rx->f21c = 0;
	tx1_put_int(o, TX1_TIMER_MARK, tx1_get_int(o, TX1_TIMER));

	idx = tx1_get(o, TX1_RATEIDX);
	o->rate_now = idx;
	o->rate_want = idx;

	tx1_put_int(o, TX1_F2218, 1);
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
 * THE SCALE IS SIGNED AND THE SHIFT IS ARITHMETIC.  `f25d4` comes in through
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
		buf[i] = (short)((v * o->f25d4) >> 14);
		o->vect_idx = (short)(idx + 1);
	}

	txwritequeue(&o->txq, buf);

	if (tx1_get(o, TX1_MICROSTATE) != V34HS_TX_L1)
		return V34TX1_LOOP;
	if ((unsigned short)o->vect_idx != 0x600u)
		return V34TX1_LOOP;

	tx1_put(o, TX1_MICROSTATE, V34HS_TX_L2);
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
 * The segment is eight symbol PAIRS long, `f25c0` counting them, and the
 * count is compared as sixteen bits BEFORE the store -- the object stores
 * `%ax` only on the path that does not reach eight, and the completing path
 * clears the field at 0x671f2 instead.  So a reconstruction that stored first
 * and then compared would leave 8 behind where the object leaves 0.
 *
 * FOUR WAYS OUT OF THE COMPLETION, tested in this order and no other:
 *
 *   f25c2 & 0x2000      -> TRNSEG4A                          (0x67209)
 *   f35a4 == 0          -> PPSEG                             (0x68375)
 *   f25c2 & 0x8000      -> PPSEG, and f25c0 = f35a6 first    (0x69eca)
 *   otherwise           -> TXMD, after f35a6 = f35a4 * 0x53,
 *                          the modulator and the counter     (0x6783e)
 *
 * and all four converge on 0x67236, which zeroes `vect_idx` and +0x358e.
 * The first test is `test $0x20,%dh` on the halfword loaded at 0x671f9, which
 * is bit 13 and not bit 5; the third is `test %dx,%dx` / `js` on the SAME
 * register, so it is bit 15 of `f25c2` read as a sign.
 *
 * THE MODULATOR'S SIX ARGUMENTS ARE ALL LITERALS -- 4800 baud, 2400 carrier,
 * no pre-emphasis, `v90` zero, no reset (0x67890..0x678be).  There is no rate
 * configuration to seed and finding 216's 3200-baud discipline has nothing to
 * bite on here: `v90` is a constant zero, so the one rate that reads it is not
 * the rate this arm asks for.  4800 is a real case in `V34SetupModulator` --
 * `txAllPass` at 0x20 taps -- and it is the one case that leaves `prem` NULL,
 * so `m->ec_prem` is not written.
 *
 * THE COUNTER AT +0xaa78 IS A TIME, and the object computes it in fixed point
 * without touching the FPU:
 *
 *     67878..678db   esi = (0x5e8 - f25c) << 14
 *     678dd..678e7   the 0x1b4e81b5 / `sar $0xa` / `sub` sequence, which is
 *                    signed division by 9600 truncating toward zero
 *     678e9          movswl %dx  -- the quotient is TRUNCATED TO A SHORT
 *     678ec..67909   * 0x960, >> 14, + 0x96
 *
 * so it is `(1512 - f25c) * 2400 / 9600 + 150` carried through a sixteen-bit
 * intermediate.  1512 and 9600 are sample counts at 9600 Hz; the truncation is
 * reachable, because the quotient leaves a short once `1512 - f25c` passes
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

	n = (unsigned short)((unsigned short)o->f25c0 + 1);
	if (n != 8) {
		o->f25c0 = (short)n;
		return V34TX1_LOOP;		/* 0x629c8 */
	}

	/* 0x671f0 */
	o->f25c0 = 0;
	if ((unsigned short)o->f25c2 & 0x2000u) {
		if (tx1_get(o, TX1_TXSTATE) != V34HS_TRNSEG4A)
			tx1_put(o, TX1_TXSTATE, V34HS_TRNSEG4A);
	} else if (tx1_get(o, TX1_F35A4) == 0) {
		/* 0x68375 */
		if (tx1_get(o, TX1_TXSTATE) != V34HS_PPSEG)
			tx1_put(o, TX1_TXSTATE, V34HS_PPSEG);
	} else if ((unsigned short)o->f25c2 & 0x8000u) {
		/* 0x69eca */
		o->f25c0 = tx1_get(o, TX1_SEGLEN);
		if (tx1_get(o, TX1_TXSTATE) != V34HS_PPSEG)
			tx1_put(o, TX1_TXSTATE, V34HS_PPSEG);
	} else {
		/* 0x6783e */
		int span;
		short q;

		tx1_put(o, TX1_SEGLEN,
			(short)((unsigned short)tx1_get(o, TX1_F35A4) * 0x53));
		if (tx1_get(o, TX1_TXSTATE) != V34HS_TXMD)
			tx1_put(o, TX1_TXSTATE, V34HS_TXMD);

		/* 0x67885 */
		V34SetupModulator((struct v34_modulator *)
				  ((char *)o + TX1_MODULATOR),
				  4800, 2400, 0, 0, 0);

		/*
		 * `(0x5e8 - f25c) << 14` with the subtraction done in an int.
		 * Spelled through `unsigned` because the left shift of a
		 * negative int is undefined in C and the object's `shl` is
		 * not; the bits are the same either way.
		 */
		span = (int)((unsigned)(0x5e8 - o->f25c) << 14);
		q = (short)(span / 9600);
		tx1_put(o, TX1_COUNT, (short)((((int)q * 0x960) >> 14) + 0x96));
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
 *   f25c0                bumped at 0x6430c on the two paths that do NOT end
 *                        the segment, and NOT on the one that does
 *
 * That last asymmetry is the arm's shape.  0x6430c is the shared block 86
 * also rejoins through (finding 340), and the segment's end leaves through
 * 0x63da2 instead, which writes nothing -- so a reconstruction that bumped
 * f25c0 on every path is wrong on exactly one of the three.
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
 * SHIFT.  19 computes `(0x5e8 - f25c) << 14` and this computes
 * `(0x5e8 - f25c) * baud`, both divided by 9600 through the same
 * 0x1b4e81b5 / `sar $0xa` sequence -- signed, truncating toward zero.  Then
 * three terms are added: f25c0, +0xaa7c, and a literal one, plus `rtd` again
 * when `f359c == 0x65`.  The quotient is NOT truncated to a short here, where
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
			if (tx1_get(o, TX1_TXSTATE) != V34HS_TRNSEG4)
				tx1_put(o, TX1_TXSTATE, V34HS_TRNSEG4);

			/* 0x68154 */
			span = (0x5e8 - o->f25c) * (int)baud;
			q = span / 9600;
			tx1_put(o, TX1_COUNT, (short)q);	/* 0x6818d */

			acc = q + (unsigned short)tx1_get(o, TX1_FAA7C)
				+ (unsigned short)o->f25c0 + 1;
			if (o->f359c == 0x65)			/* 0x6926d */
				acc += (unsigned short)o->rtd;
			tx1_put(o, TX1_COUNT, (short)acc);

			/* 0x681c6 */
			tx1_put(o, TX1_FAA86,
				(short)((unsigned short)tx1_get(o, TX1_FAA86)
					+ (unsigned short)tx1_get(o, TX1_COUNT)));
			return V34TX1_LOOP;		/* 0x63da2 */
		}
	}

	/* 0x6430c */
	o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
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
 * `f359c == 0x65` AND `v90_receiver` is non-zero (0x69fcf), and 0x11
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
 * from `f359c == 0x65`, which is 0x3a when the compare held and 0x2e when it
 * did not: RX_PHASE1_CALL for the originating side and TX_PHASE1_ANS for the
 * answering one, which is the same `f359c` reading probeselect uses.
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
		if (*((unsigned char *)o + TX1_FABE8) != 0)
			want = V34HS_MOH_TONE;
		else					/* 0x68a7c */
			want = o->f359c == 0x65 ? V34HS_RX_PHASE1_CALL
						: V34HS_TX_PHASE1_ANS;
		if (tx1_get(o, TX1_MICROSTATE) != want)
			tx1_put(o, TX1_MICROSTATE, want);

		/* 0x66be0 */
		if (tx1_get(o, TX1_RXSTATE) != V34HS_RX_DPSK)
			tx1_put(o, TX1_RXSTATE, V34HS_RX_DPSK);
		/* 0x66c10 */
		if (txst != V34HS_TONE_AB)
			tx1_put(o, TX1_TXSTATE, V34HS_TONE_AB);

		/* 0x66c32 */
		tx1_put(o, TX1_F358C, 0);
		o->vect_idx = 0;
		tx1_put(o, TX1_COUNT, 0);
		tx1_put(o, TX1_FAAE2, -1);
		tx1_put(o, TX1_FAAE0, 0);
		return V34TX1_LOOP;			/* 0x63941 */
	}

	if (txst != V34HS_SILENCEINFO)
		return V34TX1_LOOP;			/* 0x6409a */

	/* 0x6410b */
	n = (unsigned short)((unsigned short)o->vect_idx + 1);
	if (n != 0xa) {
		o->vect_idx = (short)n;
		return V34TX1_LOOP;			/* 0x629cf */
	}

	/* 0x6858f */
	o->vect_idx = 0;
	tx1_put(o, TX1_TXSTATE, V34HS_TX_DPSK);
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
		short lead = (o->f359c == 0x65 && o->v90_receiver != 0)
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
 * and the new "last".  `f25c8` is the quadrant chosen and `f25c6` the one
 * carried forward, and the object writes them with two stores of the same
 * register either side of the table load.
 *
 * The two sites are instruction for instruction the same -- the same three
 * fields, the same mask, the same order -- so this is one static rather than
 * two copies, for the reason 78 and 85 share `tx1_ja_common`.  `f25c6` is
 * read with `movzwl` and the sum is masked to two bits, so a fixture value
 * outside 0..3 cannot change the answer.
 */
static short
tx1_dpsk4(struct v34_object *o, short q)
{
	short k = (short)((q + (unsigned short)o->f25c6) & 3);
	int point = vect4[k];

	o->f25c8 = k;
	o->f25c6 = k;
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
 * THE GENERATOR SELECT IS `f25c2 & 1` AND NOT `f359c == 0x65`.  71 and 86
 * choose their scrambler polynomial on the state word (finding 340); this arm
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
 * The first call's quadrant goes into `f25c8` and is READ BACK FROM THERE
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
 * against DATAXMIT cannot be false, for finding 342's reason at 65: the arm
 * is reached only through table 1 at `txstate == 69` and `txmit` does not
 * write +0x3596.
 */
int
v34tx1_exmit(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	short mode = (short)((o->f25c2 & 1) == 0);
	short q;

	if ((unsigned short)tx1_get(o, TX1_F382) == 0x89b0u) {
		/* 0x67031 */
		short k;
		int point;

		q = (short)V34scrambler((unsigned *)&o->f25cc, mode, 0xf, 2);
		o->f25c8 = (short)((q + (unsigned short)o->f25c6) & 3);

		q = (short)V34scrambler((unsigned *)&o->f25cc, mode, 3, 2);
		k = o->f25c8;			/* re-read, 0x67131 */
		o->f25c6 = k;
		point = vect16[q + 4 * k];
		tx1_put_point(o, point);
		txmit(o);
		o->vect_idx = (short)((unsigned short)o->vect_idx + 4);
	} else {
		/* 0x6386b */
		q = (short)V34scrambler((unsigned *)&o->f25cc, mode, 3, 2);
		(void)tx1_dpsk4(o, q);
		txmit(o);
		o->vect_idx = (short)((unsigned short)o->vect_idx + 2);
	}

	/* 0x6392c */
	if ((unsigned short)o->vect_idx == 0x14u) {
		/* 0x66e01 */
		short txst = tx1_get(o, TX1_TXSTATE);

		o->vect_idx = 8;
		if (txst != V34HS_DATAXMIT)
			tx1_put(o, TX1_TXSTATE, V34HS_DATAXMIT);
		return V34TX1_LOOP;			/* 0x6409a */
	}
	return V34TX1_LOOP;				/* 0x63941 */
}

/*
 * ---------------------------------------------------------------------------
 * 64 `JTXMIT` and 68 `J1TXMIT`, 0x635cc, with 68's tail at 0x65653, 64's at
 * 0x6372e and the `f359c == 0x66` path at 0x66ca8.
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
 *   - the scrambler, on `f25c2 & 1` as 69's is;
 *   - the differential quadrant and one `vect4` point;
 *   - `vect_idx = (vect_idx + 1) & 7`, stored before anything branches.
 *
 * THEN THREE WAYS OUT, tested in this order and no other:
 *
 *   f359c == 0x66       -> 0x66ca8, from EITHER txstate
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
 * AND THEN THE SECOND PAIR OF ECHO REPORTS.  If +0xaa78 is still non-zero the
 * arm raises bit 2 of `f25c2`, reports both cancellers and ZEROES the counter
 * -- the same two calls and the same flag as the countdown's own completion,
 * on a different condition.  The two are mutually exclusive on one pass:
 * reaching zero at the top leaves nothing for this to do.  `V34EchoReportCoeff`
 * only prints (finding 341), so of the four call sites in this arm nothing is
 * observable at debug level 0; the flag and the zeroing are.
 *
 * 68's TAIL, 0x65653, CLEARS RATHER THAN COUNTS: `f25cc` (a 32-bit store),
 * `f25c6` and `f25c0` all go to zero and the transmit machine moves to
 * TRNSEG4A.  IT DOES NOT COMPARE FIRST -- there is no `if (txstate != ...)`
 * guard the way every other state change in this file has one -- and it is
 * written that way because that is what the object does.
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
	(void)tx1_ja_common(o);

	/* 0x635f0 */
	tx1_put(o, TX1_F25D8,
		(short)((unsigned short)tx1_get(o, TX1_F25D8) + 1));

	shift = (unsigned)(2 * (int)o->vect_idx) & 31u;
	bits = (short)((unsigned)(unsigned short)tx1_get(o, TX1_F25D6) >> shift);

	mode = (short)((o->f25c2 & 1) == 0);
	q = (short)V34scrambler((unsigned *)&o->f25cc, mode, bits, 2);
	(void)tx1_dpsk4(o, q);
	txmit(o);

	idx = (unsigned short)(((unsigned short)o->vect_idx + 1) & 7);
	o->vect_idx = (short)idx;

	if (o->f359c == 0x66) {
		/* 0x66ca8 */
		if (!(rx->flags & V34_RX_FLAG_DATA)) {
			if (tx1_get(o, TX1_F35A2) == 0)
				return V34TX1_LOOP;	/* 0x629c8 */
			if (tx1_get(o, TX1_F25DA) == 0)
				return V34TX1_LOOP;	/* 0x63da2 */
		}
		/* 0x66cd1 */
		if (tx1_get(o, TX1_TXSTATE) != V34HS_XMIT0)
			tx1_put(o, TX1_TXSTATE, V34HS_XMIT0);
		o->vect_idx = 0;			/* 0x62d32 */
		return V34TX1_LOOP;			/* 0x62d70 */
	}

	/* 0x636f0 */
	if (idx != 0)
		return V34TX1_LOOP;			/* 0x63941 */

	if (tx1_get(o, TX1_TXSTATE) == V34HS_J1TXMIT) {
		/* 0x65653 */
		o->f25cc = 0;
		tx1_put(o, TX1_TXSTATE, V34HS_TRNSEG4A);
		o->f25c6 = 0;
		o->f25c0 = 0;
		return V34TX1_LOOP;			/* 0x640a1 */
	}

	/* 0x6370d */
	if (tx1_get(o, TX1_F25DA) != 2)
		return V34TX1_LOOP;			/* 0x6409a */
	if (tx1_get(o, TX1_F25D8) <= 0x80)
		return V34TX1_LOOP;			/* 0x6431f */

	/* 0x6372e */
	tx1_put(o, TX1_F25D6, (short)0x899f);
	if (tx1_get(o, TX1_TXSTATE) != V34HS_J1TXMIT)
		tx1_put(o, TX1_TXSTATE, V34HS_J1TXMIT);

	/* 0x637c8 */
	if (tx1_get(o, TX1_COUNT) != 0) {
		o->f25c2 = (short)((unsigned short)o->f25c2 | 4u);
		V34EchoReportCoeff(&o->echo0);
		V34EchoReportCoeff(&o->echo1);
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
 * THE SHAPE.  Two or four bits are collected into `f25c8`, lowest first, and
 * then ONE symbol is mapped out of them:
 *
 *     f25c8 = 0
 *     n = 0
 *     while (n <= (receiver +0x11e == 0x89b0 ? 3 : 1)) {
 *             f25c8 |= getbit(*(obj+0xaa6c)) << n;
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
 * WHAT IS NOT RECONSTRUCTED, and it is finding 341's gap again.  Three blocks
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
			if (tx1_get(o, TX1_TXSTATE) != V34HS_EXMIT)
				tx1_put(o, TX1_TXSTATE, V34HS_EXMIT);
			o->vect_idx = 0;
			rx->flags = (unsigned short)(rx->flags & ~0x20u);
			return 1;
		}
		stamp = (flags & 0x18u) == 0x10u;	/* 0x64614 */
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
		flags = rx->flags;			/* 0x64805 */
	}

	tx1_mp_reload(o, tx1_bitsource(o), flags);
	return 0;
}

/*
 * 0x64929 -- two bits, one differential quadrant, one `vect4` point.  This is
 * 69's four-point half with the scrambler's `bits` coming out of `f25c8`
 * instead of a literal three, so `tx1_dpsk4` carries the rest of it.
 */
static void
tx1_mp4(struct v34_object *o, short src)
{
	short pick = (short)((o->f25c2 & 1) == 0);
	short q = (short)V34scrambler((unsigned *)&o->f25cc, pick, src, 2);

	(void)tx1_dpsk4(o, q);
}

/*
 * 0x63b62 -- four bits, two scrambler steps, one `vect16` point.  Instruction
 * for instruction this is 69's sixteen-point block at 0x67031 with two
 * differences and no others: the two `bits` arguments are the low and the high
 * dibit of `f25c8` where 69 passes the literals 0xf and 3, and this one does
 * not advance `vect_idx` (the bit loop already did, once per bit).  It is
 * written out rather than shared with 69 because sharing would have to rewrite
 * 69's body, and the mutations already aimed at that text are what say 69 is
 * right.
 *
 * `f25c8` IS READ SIGNED (`movswl`, 0x63b83) AND IT CANNOT MATTER.  0x639ab
 * clears the field before the loop and the loop only ORs bits in, so the two
 * readings can differ only when bit 15 is set -- which needs the reader's
 * exhausted arm, whose -1 fills every bit.  Even then the scrambler consumes
 * bits 0 and 1 alone, and after the arithmetic `sar $2` bits 2 and 3, and all
 * four are ones under either reading.  So it is a proof and not a run.
 *
 * The second call takes the SAVED halfword shifted down by two, not the value
 * `f25c8` now holds: 0x63c03 restores the register before 0x63c12 shifts it,
 * and 0x63c05 has already overwritten the field with the first quadrant.
 */
static void
tx1_mp16(struct v34_object *o, short src)
{
	short pick = (short)((o->f25c2 & 1) == 0);
	short k, q;
	int point;

	q = (short)V34scrambler((unsigned *)&o->f25cc, pick, src, 2);
	o->f25c8 = (short)((q + (unsigned short)o->f25c6) & 3);

	q = (short)V34scrambler((unsigned *)&o->f25cc, pick,
				(short)(src >> 2), 2);
	k = o->f25c8;					/* re-read, 0x63c72 */
	o->f25c6 = k;
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

	o->f25c8 = 0;					/* 0x639ab */

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
		 * `f25c8` from bit `n` up rather than setting one bit.
		 */
		o->f25c8 = (short)((unsigned short)o->f25c8
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
		tx1_mp16(o, o->f25c8);
	else
		tx1_mp4(o, o->f25c8);

	txmit(o);				/* 0x62d5f and 0x64a42 */
	return V34TX1_LOOP;			/* 0x62d70 and 0x640a1 */
}

/*
 * ---------------------------------------------------------------------------
 * 24 `TX_DPSK`, 0x62b96 -- one bit of a message as one tone, and the whole of
 * Modem-on-Hold's clear-down when the message runs out.
 *
 * THE 2,220 BYTES ARE `getbit` INLINED TWICE, which is finding 424's result
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
 * implied, is finding 424's cost unchanged: `tools/mutate.py` anchors on
 * source text and the reader's text is in `v34hshak.c`, which is
 * `t_v34hshak.c`'s suite.  What the runs here test is that each call is the
 * right one on the right record and every branch the arm takes around them.
 *
 * ---------------------------------------------------------------------------
 * THE SHAPE.  One bit, one tone, and three ways for the message to end:
 *
 *     if (fabe8)  vect_idx += 1        the Modem-on-Hold clock
 *     bit = getbit(*(obj + 0xaa6c))
 *     if (bit >= 0)  {  f358c ^= bit; vect4[2 * (f358c & 1)]; txmit;  }
 *     else if (!fabe8)      txstate = 60 TONE_AB and nothing else
 *     else if (!fabf8)      re-arm the reader BY HAND, take one more bit,
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
 * FINDING 323'S ONE COLLISION IS THIS, AND IT IS NOT IDENTITY.  Entered cold
 * 24 and 60 write the same 69 bytes with the same signature, and the reason
 * is narrow: the fixture's fill leaves +0xabe8 clear, so no `vect_idx` tick;
 * the reader hands back a ZERO bit, so the store into +0x358c writes the
 * value already there and the tone selected is the one 60 selects; and the
 * reader's own advance lands OUTSIDE the object, where finding 323's byte
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
 * WHAT IS NOT RECONSTRUCTED, and it is finding 341's gap again.  Fourteen
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

/* +0xabe4 and +0xabe6, two halfwords of `unmapped_abe4`; +0xabe8 is TX1_FABE8
 * above and is the same region's Modem-on-Hold flag.  The clear-down raises
 * +0xabe4 and the two `v34handshakinit` paths raise +0xabe6; no other site in
 * this tree reads either. */
#define TX1_FABE4	0xabe4
#define TX1_FABE6	0xabe6

/*
 * +0xabf8 and +0xabf9, two BYTES of `unmapped_abf8`, read with `cmpb`.
 *
 * +0xabf8 is a one-shot: while it is up the arm runs the message dispatch and
 * clears it, and once it is down the arm re-arms the reader instead.  +0xabf9
 * chooses twice -- which message is built at 0x6a3f3, and which of the tail's
 * three ways out is taken at 0x65012.  Neither has another reader here.
 */
#define TX1_FABF8	0xabf8
#define TX1_FABF9	0xabf9

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
 * THE txstate COMPARE AT 0x64eb6 CANNOT BE FALSE, for finding 342's reason at
 * 65: that site is reached only from the dispatch at `txstate == 24` and
 * nothing between them writes +0x3596.  The OTHER TWO CAN, because the hold
 * tail runs after the dispatch has already moved the machine -- which is why
 * this is one function and not one inlined body.
 */
static void
tx1_moh_cleardown(struct v34_object *o)
{
	if (tx1_get(o, TX1_TXSTATE) != V34HS_MOH_CLEARDOWN)
		tx1_put(o, TX1_TXSTATE, V34HS_MOH_CLEARDOWN);
	if (tx1_get(o, TX1_RXSTATE) != V34HS_WAIT)
		tx1_put(o, TX1_RXSTATE, V34HS_WAIT);
	tx1_put(o, TX1_FABE4, 1);
}

/*
 * 0x689f6 -- put the modem on hold: MOH_SILENCE, WAIT, and the two counters
 * the silence arm at 0x63d58 then runs on.  `t_v34hstx1.c` drives it through
 * both of its two entries, `moh_message == 1` and `moh_recvd` at 0 or 4.
 */
static void
tx1_moh_on_hold(struct v34_object *o)
{
	if (tx1_get(o, TX1_TXSTATE) != V34HS_MOH_SILENCE)
		tx1_put(o, TX1_TXSTATE, V34HS_MOH_SILENCE);
	if (tx1_get(o, TX1_RXSTATE) != V34HS_WAIT)
		tx1_put(o, TX1_RXSTATE, V34HS_WAIT);
	tx1_put(o, TX1_COUNT, 0);
	o->vect_idx = 0;
}

/*
 * 0x69041 and 0x6923d -- the retrain entry, twice over and identical.
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
tx1_moh_reinit(struct v34_object *o)
{
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

	if (o->fabe2 != 3)
		o->fabe2 = 1;
	if (*((unsigned char *)o + TX1_FABF9) == 0)
		o->moh_message = 1;		/* 0x70430 */
	else
		o->moh_message = 3;		/* 0x6a415 */

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

	if (*((unsigned char *)o + TX1_FABF9) != 0) {
		/* 0x688b4 */
		tx1_moh_cleardown(o);
		o->fabe2 = 1;
		return V34TX1_LOOP;			/* 0x629cf */
	}

	if ((unsigned)(o->moh_message - 2) > 1u) {
		/* 0x6923d */
		tx1_moh_reinit(o);
		return V34TX1_LOOP;			/* 0x629cf */
	}

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
	if (*((unsigned char *)o + TX1_FABE8) != 0)
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
	if (*((unsigned char *)o + TX1_FABE8) == 0) {
		/*
		 * 0x654dd.  The compare against TONE_AB cannot be false, for
		 * finding 342's reason at 65: the arm is reached only through
		 * table 1 at `txstate == 24` and nothing between the dispatch
		 * and here writes +0x3596 -- `getbit` does not.  It is
		 * written as the object writes it.
		 */
		if (tx1_get(o, TX1_TXSTATE) != V34HS_TONE_AB)
			tx1_put(o, TX1_TXSTATE, V34HS_TONE_AB);
		return V34TX1_LOOP;			/* 0x63948 */
	}

	if (*((unsigned char *)o + TX1_FABF8) == 0) {
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
				tx1_moh_reinit(o);	/* 0x69041 */
			else
				tx1_moh_send(o);	/* 0x6a3cf */
		} else if (got == 0)
			tx1_moh_on_hold(o);
		else
			tx1_moh_reinit(o);		/* 0x69041 */
	} else if ((unsigned)o->moh_message <= 3u) {
		tx1_moh_cleardown(o);			/* 0x64eaf */
	}

	/* 0x64fde */
	*((unsigned char *)o + TX1_FABF8) = 0;
	return tx1_moh_hold(o);				/* 0x64fec */
}

/*
 * ---------------------------------------------------------------------------
 * 66 `TRNSEG4A`, 0x62e28, with the sixteen-point half at 0x66e59 and the
 * segment's completion at 0x62f22.
 *
 * THE SYMBOL IS 71's AND 86's, NOT 69's.  The generator is chosen on
 * `f359c == 0x65` -- the object carries the scrambler loop twice, at 0x62e70
 * with the 0x04000000 tap and at 0x6358c with 0x00002000, and picks between
 * the copies exactly as 71 and 86 do -- so `tx1_scramble2` is the same call
 * here.  69 chooses on bit 0 of `f25c2` instead (finding 340 against 423);
 * one halfword apart and it is the whole difference between the two shapes.
 *
 * THE CONSTELLATION IS 69's.  The receiver's +0x11e against 0x89b0 is the
 * arm's first instruction, and the sixteen-point half scrambles TWICE: the
 * first call's two bits go into `f25c8` and are READ BACK FROM THERE
 * (0x66f4d) as `vect16`'s row, with the second call's as the column.  Where
 * 69 differs is that neither half here differentially encodes and neither
 * writes `f25c6` -- the quadrant this arm carries is the raw scrambler
 * output, and `f25c6` is written once, at the completion, out of `f25c8`.
 *
 * AND NEITHER HALF ADVANCES `vect_idx`.  This arm counts in `f25c0`, and it
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
 *     n  > lim  and  f2218 > 3                        likewise
 *     n  < lim  and  f2218 > 3  and  n >= baud+period
 *               and  rx->f21a <= rx+0x250 + 10        likewise (0x6559c)
 *
 * so a run that has passed the nominal length finishes at once, and a run
 * that has passed the SHORTER threshold finishes early when the equaliser
 * error at +0x21a has come down to within ten of the mark at +0x250.
 * `f2218` is the same int table 2's tail reads and the compare is UNSIGNED
 * (`cmpl $0x3 ; jbe`), so a negative value is a large one here.
 *
 * `baud >> 1` IS AN ARITHMETIC SHIFT (0x62f02 is `sar`), not a divide: with
 * a negative `baud` the two differ, and the fixture's fill makes them differ.
 *
 * THE FOUR REJOINS ARE ALL THE SAME BLOCK.  0x63941, 0x6409a, 0x6431f and
 * 0x62d70 each reload the object, re-test the queue count against +0x2aa0
 * and jump to 0x629e7; the addresses are kept in the comments because the
 * blocks are distinct in the object, not because the exits differ.
 *
 * WHAT IS NOT HERE YET.  The completion at 0x62f22 is 2.5 KB of its own --
 * it rebuilds the receive half of the rate configuration, snapshots the
 * receiver's predictor into the INFO record at +0xaa3c, and moves the
 * transmit machine to 0x43.  It returns `V34TX1_TRNSEG4A_SEGEND` rather than
 * doing something plausible, for the reason 81 and 86 return theirs
 * (finding 343): a path that is not settled is a path that says so.
 */

/*
 * +0x4b4, which is the RECEIVER's +0x250 -- `0x74(%esp)` plus 0x250 -- and
 * lands in that structure's `pad_250`, so it is not a named field anywhere in
 * this tree.  The completion writes it out of `f224` or `f21a`; the early
 * finish above reads it as the mark the equaliser error is measured against.
 */
#define TX1_RX250	0x4b4

int
v34tx1_trnseg4a(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx =
		(struct v34_receiver *)((char *)objp + TX1_RECEIVER);
	struct v34_ratecfg *cfg =
		(struct v34_ratecfg *)((char *)objp + V34_RATECFG);
	int n, lim, baud, period;

	if ((unsigned short)tx1_get(o, TX1_F382) == 0x89b0u) {
		/* 0x66e59 */
		short k = tx1_scramble2(o);
		short q;

		o->f25c8 = k;
		q = tx1_scramble2(o);
		tx1_put_point(o, vect16[q + 4 * k]);
	} else {
		/* 0x62e3b */
		short q = tx1_scramble2(o);

		o->f25c8 = q;
		tx1_put_point(o, vect4[q]);
	}

	/* 0x62ecd */
	txmit(o);

	/* 0x62edc */
	o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
	n = o->f25c0;
	baud = cfg->baud;
	period = cfg->period;
	lim = baud + (baud >> 1) + period;

	if (n != lim) {
		if ((unsigned)tx1_get_int(o, TX1_F2218) <= 3u)
			return V34TX1_LOOP;		/* 0x63941 */
		if (n < lim) {
			/* 0x6559c */
			if (n < baud + period)
				return V34TX1_LOOP;	/* 0x6409a */
			if ((int)rx->f21a > (int)tx1_get(o, TX1_RX250) + 10)
				return V34TX1_LOOP;	/* 0x6431f */
		}
	}

	/* 0x62f22 */
	return V34TX1_TRNSEG4A_SEGEND;
}

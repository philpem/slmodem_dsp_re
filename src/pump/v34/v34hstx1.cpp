/*
 * v34hstx1.cpp -- thirteen arms of `v34handshak`'s per-sample transmit
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
 * a property of ONE object fill and of nothing else.  60 is written here; 24
 * is not, and this file says nothing about what it does.
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

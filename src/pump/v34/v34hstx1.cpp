/*
 * v34hstx1.cpp -- ten arms of `v34handshak`'s per-sample transmit dispatch.
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

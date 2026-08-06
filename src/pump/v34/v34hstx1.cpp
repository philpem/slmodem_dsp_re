/*
 * v34hstx1.cpp -- six arms of `v34handshak`'s per-sample transmit dispatch.
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
 */

#include <string.h>

#include "dsplib/v34filt.h"	/* V34EchoReportCoeff, V34SetupModulator */
#include "dsplib/v34fsk.h"	/* struct v34_object, struct v34_ratecfg */
#include "dsplib/v34hshak.h"	/* vect4, v90Phase34, k56FlexPhase34      */
#include "dsplib/v34hstx1.h"
#include "dsplib/v34recv.h"	/* struct v34_receiver                    */
#include "dsplib/v34rx.h"	/* txmit, txwritequeue, V34scrambler      */

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

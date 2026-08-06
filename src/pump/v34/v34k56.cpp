/*
 * v34k56.cpp -- `k56FlexPhase34`, the K56flex handshake's phase 3/4
 * transmitter.
 *
 * WHY THIS IS A .cpp AND WHY THE SYMBOL IS UNMANGLED.  The object exports
 * `k56FlexPhase34` with no mangling, so it was declared `extern "C"`; but two
 * of its calls are relocations against
 *
 *     _ZN15K56FlexFloModem16getK56FlexJaBitsEPs
 *     _ZN15K56FlexFloModem16getK56FlexMpBitsEPs
 *
 * which a C translation unit cannot name.  So the translation unit is C++ and
 * the declaration lives inside `v34hshak.h`'s `extern "C"` block -- the same
 * arrangement `v34pcmmain.cpp` uses from the other side, and for the mirror
 * reason.  The stem is `v34k56` rather than a per-object name because
 * `docs/attribution.md` puts .text+0xa790 in the `V34.c|GenericToneDetector.cpp`
 * block and marks it AMBIGUOUS: the object does not say which file this came
 * from, so neither does the file name.
 *
 * WHAT IT DOES.  One call emits one handshake symbol, and which symbol
 * depends on where the K56flex phase-3/4 sequence has got to.  Two things
 * select the arm: `V34_RX_FLAG_DATA` in the receiver's flags word, and the
 * int at +0x250 that `v34fsk.h` calls `k56flex_receiver`.
 *
 *     flag clear          transmit the next Ja dibit
 *     flag set, +0x250=3  shift the word at +0x25d6 out, two bits at a time
 *     flag set, +0x250=4  transmit a scrambled idle symbol
 *     flag set, +0x250=5  transmit the next MP dibit or quadbit
 *     flag set, anything  do nothing
 *     else
 *
 * and the object advances +0x250 3 -> 4 itself.  Nothing here moves it to 5;
 * whatever does is outside the 721 bytes.
 *
 * ---------------------------------------------------------------------------
 * THE IDLE SYMBOL IS NOT `txmitdibit`, AND THAT IS THE WHOLE POINT OF THE
 * FUNCTION'S MIDDLE.
 *
 * Case 4 looks exactly like a `txmitdibit(obj, 3)` and is not one, in three
 * ways that all move the constellation point:
 *
 *   - `V34scrambler`'s mode argument is the LITERAL 1.  `txmitdibit` passes
 *     `tx_scrambler_mode(o)`, which reads bit 0 of `f25c2`; nothing in these
 *     721 bytes loads +0x25c2 at all, so the generator here does not follow
 *     the calling/answering flag the rest of the transmitter obeys.
 *   - there is NO differential encoding.  `txmitdibit` forms
 *     `(d + f25c6) & 3`; this stores the scrambler's two bits straight into
 *     `f25c8` and indexes `vect4` with them.
 *   - `f25c6` is not written, so the quadrant the rest of the handshake
 *     carries does not advance.
 *
 * The quadbit arm differs the same way: two two-bit scrambler requests, the
 * first into `f25c8` and the second selecting within it, with the sum
 * `d2 + q * 4` -- which is `txmitquadbit`'s index expression -- but again
 * with no differential add and no `f25c6` write.  A reconstruction that
 * called the two published emitters would compile, link, and be wrong; the
 * mutation suite's first two entries are exactly that substitution.
 *
 * `vect4[q]` and `vect16[d2 + q * 4]` are indexed UNMASKED, as the object
 * does.  `V34scrambler` with `nbits == 2` returns 0..3, so a `& 3` would be
 * unobservable -- but only while that contract holds, which is why it is not
 * written here.
 *
 * ---------------------------------------------------------------------------
 * TWO BLOCKS OF THIS FUNCTION ARE DEAD IN THIS OBJECT, and they are marked
 * below.  Both are reached only when a `K56FlexFloModem` bit source reports
 * that its sequence has finished, and both of those members are three bytes
 * of `xor %eax,%eax; ret` -- see `include/dsplib/K56FlexFloModem.h`, which
 * measured them.  So the completion arms cannot be entered from either side
 * of a differential test, by construction and not by omission.  They are
 * transcribed from the disassembly and are NOT covered by the test; finding
 * 281 gives the measurement and lists the mutations that go uncaught as a
 * result.
 */

#include "dsplib/K56FlexFloModem.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34recv.h"
#include "dsplib/v34rx.h"
#include "dsplib/v34shell.h"

/* The receiver sub-object, whose flags word carries the phase gate. */
#define OB_RECEIVER	0x264

/*
 * +0x25d6.  A sixteen-bit pattern shifted out as eight dibits, LSB first,
 * and the ONLY thing case 3 transmits.  `v34handshakinit` seeds it with
 * 0x8990 and the Ja completion arm below replaces it with 0x899f; both are
 * whole-word stores of a constant, and nothing in the tree reads it as
 * anything but this shift register.  Reached by offset because the region is
 * `unmapped_25d6` in `struct v34_object` -- v34hshak.c:424 writes it the same
 * way.
 */
#define OB_TXBITS	0x25d6

/* The handshake's transmit state machine; see v34hshak.c. */
#define OB_TXSTATE	0x3596

/*
 * The constellation-size discriminator at +0x382, which
 * `VPcmV34SetV90RateReneg` sets to 0x89b0 or 0x8990.  0x89b0 selects the
 * sixteen-point map and four bits a symbol; anything else selects four
 * points and two bits.  Both readers here compare for equality with 0x89b0,
 * so 0x8990 is not privileged over any other value.
 */
#define OB_CONSTEL_16	((short)0x89b0)

int
k56FlexPhase34(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)(m + OB_RECEIVER);
	K56FlexFloModem *k56 = (K56FlexFloModem *)o->pac18;

	if (!(rx->flags & V34_RX_FLAG_DATA)) {
		/*
		 * Ja.  The bit source writes the dibit into `f25c8` -- the
		 * same field the emitters use as their quadrant register --
		 * and returns non-zero on the symbol that ends the sequence.
		 * The dibit is transmitted either way, so the last one is
		 * sent and then acted on.
		 */
		int done = (short)k56->getK56FlexJaBits(&o->f25c8);

		txmitdibit(o, o->f25c8);
		if (done == 0)
			return 0;

		/* DEAD: `getK56FlexJaBits` is a stub returning 0. */
		rx->flags |= V34_RX_FLAG_DATA;
		*(short *)(m + OB_TXBITS) = (short)0x899f;
		o->vect_idx = 0;
		o->k56flex_receiver = 3;
		return 0;
	}

	switch (o->k56flex_receiver) {
	case 3: {
		/*
		 * Eight dibits out of one word, `vect_idx` counting them.
		 *
		 * The index is read TWICE and the second read is after
		 * `txmitdibit`, which reaches `txmit` and thence
		 * `modulatevector` -- the other writer of `vect_idx`.  So the
		 * increment is applied to whatever the transmit chain left,
		 * not to the value the shift used.
		 *
		 * The shift count comes from a SIGN-extended `vect_idx` and
		 * the increment from a zero-extended one.  `& 31` is the x86
		 * shift-count mask made explicit, so that an out-of-range
		 * index -- which only the caller can produce, since the store
		 * below masks to 0..7 -- shifts by the same amount here as in
		 * the object instead of being undefined.
		 */
		int idx = o->vect_idx;
		unsigned short w = (unsigned short)*(short *)(m + OB_TXBITS);
		unsigned nidx;

		txmitdibit(o, (short)(w >> ((2 * idx) & 31)));

		nidx = ((unsigned)(unsigned short)o->vect_idx + 1) & 7;
		o->vect_idx = (short)nidx;
		if (nidx != 0)
			return 0;

		/* The word is spent: reset the transmitter and advance. */
		o->f25c6 = 0;
		o->f25c0 = 0;
		o->f25cc = 0;
		o->k56flex_receiver = 4;
		return 0;
	}

	case 4: {
		/* The idle symbol.  See the note at the top of this file. */
		int q;

		if (o->f382 == OB_CONSTEL_16) {
			int d;

			q = (short)V34scrambler((unsigned *)&o->f25cc,
						1, 3, 2);
			o->f25c8 = (short)q;
			d = (short)V34scrambler((unsigned *)&o->f25cc,
						1, 3, 2);
			q = o->f25c8;
			*(int *)&o->f25d0 = vect16[d + q * 4];
		} else {
			q = (short)V34scrambler((unsigned *)&o->f25cc,
						1, 3, 2);
			o->f25c8 = (short)q;
			*(int *)&o->f25d0 = vect4[q];
		}

		txmit(o);
		/* Re-read: `txmit` is between the load and the store. */
		o->f25c0 = (short)((unsigned)(unsigned short)o->f25c0 + 1);
		return 0;
	}

	case 5: {
		/*
		 * MP, and the only arm that uses the full emitters: the bits
		 * are the far end's message rather than a fixed pattern, so
		 * they are scrambled and differentially encoded the way the
		 * rest of the handshake is.
		 */
		int done = (short)k56->getK56FlexMpBits(&o->f25c8);

		if (o->f382 == OB_CONSTEL_16)
			txmitquadbit(o, o->f25c8);
		else
			txmitdibit(o, o->f25c8);

		if (done == 0)
			return 0;

		/*
		 * DEAD: `getK56FlexMpBits` is a stub returning 0.
		 *
		 * The state store is a compare-then-store in the object and
		 * is written as one here.  It is indistinguishable from a
		 * plain store by any test -- the value written is the value
		 * compared against -- and the mutation that removes the
		 * compare is listed as equivalent rather than as a gap.
		 */
		getMPrecvdBits((struct tagV34Object *)objp);
		initdigital(o);
		if (*(short *)(m + OB_TXSTATE) != V34HS_EXMIT)
			*(short *)(m + OB_TXSTATE) = V34HS_EXMIT;
		o->k56flex_receiver = 2;
		return 0;
	}
	}

	return 0;
}

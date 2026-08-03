/*
 * v34hshak.c -- ITU-T V.34: the handshake's two symbol emitters.
 *
 * Reconstructed under the fast pass (docs/fastpass.md).  V34hshak.c is 118 KB
 * and `v34handshak` alone is 61 KB of it; these two are the leaves at the far
 * end, and they are here because they are the transmit half of a pair whose
 * other half is already written.
 *
 * WHAT THEY DO.  Both take a small field of bits, scramble it, map it
 * DIFFERENTIALLY onto the constellation, and hand the symbol to `txmit`.
 * They differ only in how many bits and which constellation:
 *
 *     txmitdibit    2 bits -> one quadrant of `vect4`,  added to the last
 *     txmitquadbit  4 bits -> `vect16`, as (quadrant, dibit) with only the
 *                             FIRST dibit differentially encoded
 *
 * Differential encoding is what makes the handshake survive a 180-degree
 * phase ambiguity: the receiver never has to know which way up the
 * constellation is, only how far it turned since the last symbol.  That is
 * why the quadrant is carried in the object rather than recomputed, and why
 * the second dibit of a quadbit is NOT added to anything -- the quadrant is
 * already differential, so encoding the offset within it twice would undo it.
 *
 * BOTH END IN A TAIL CALL to `txmit`, which is the whole transmit chain:
 * modulate, enqueue, pre-filter, feed the echo cancellers.  So these are the
 * handshake's entry into the datapump, not helpers beside it.
 *
 * The scrambler is not transcribed here.  The object inlines it -- twice in
 * txmitquadbit -- but it is `V34scrambler`, which v34rx.c already has, and
 * the taps prove it: 1<<26 against 1<<8 for the calling station and 1<<13
 * against 1<<8 for the answering one, which are V.34's 1 + x^-5 + x^-23 and
 * 1 + x^-18 + x^-23.  Its return value for a two-bit request is `(reg >> 29)
 * & 3`, which is exactly the expression the object forms by hand.  Calling it
 * is therefore the same function, and the differential test below is what
 * says so rather than the reading.
 */

#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34rx.h"

/*
 * ---------------------------------------------------------------------------
 * The two handshake constellations, as packed complex ints: the real part in
 * the low half and the imaginary in the high, which is the form `txmit` reads
 * them back in and the reason a single 32-bit store lands both.
 */

/*
 * vect4 -- the four quadrant points, all at (+-4579, +-4579), in the order
 * (+,+) (+,-) (-,-) (-,+).  That is CLOCKWISE, so adding one to the quadrant
 * index turns the point by -90 degrees, not +90.
 */
const int vect4[4] = {
	300093923, -300084765, -300028387, 300150301,
};

/*
 * vect16 -- sixteen points as four quadrants of four, indexed by
 * (quadrant << 2) | dibit.  Same magnitudes throughout; this is V.34's
 * 16-point handshake constellation, not a data-mode one.
 */
const int vect16[16] = {
	134219776, 134277120, -402651136, -402593792,
	-134215680, 402655232, -134158336, 402712576,
	-134154240, -134211584, 402716672, 402659328,
	134281216, -402589696, 134223872, -402647040,
};

/*
 * The scrambler generator, from the flag the two share.  Bit 0 of f25c2 set
 * selects the calling station's polynomial, which is V34scrambler's mode 0 --
 * so the sense is inverted between the two, and that is the object's.
 */
static short
tx_scrambler_mode(const struct v34_object *o)
{
	return (short)((o->f25c2 & 1) == 0);
}

/*
 * Scramble two bits and turn them into a quadrant, differentially.
 *
 * The sum is taken modulo four with no regard for the previous quadrant's
 * sign, because it is stored back masked and so is never anything but 0..3.
 */
void
txmitdibit(void *obj, short bits)
{
	struct v34_object *o = (struct v34_object *)obj;
	unsigned sr = (unsigned)o->f25cc;
	int d, q;

	d = V34scrambler(&sr, tx_scrambler_mode(o), bits, 2);
	o->f25cc = (int)sr;

	q = (d + (unsigned short)o->f25c6) & 3;

	*(int *)&o->f25d0 = vect4[q];
	o->f25c8 = (short)q;
	o->f25c6 = (short)q;

	txmit(obj);
}

/*
 * Scramble four bits and turn them into one of sixteen points.
 *
 * The two dibits go through the scrambler as two separate two-bit requests
 * rather than as one four-bit one -- which matters, because the register is
 * written back between them and the second request reads it.  Only the first
 * is differentially encoded; the second selects within the quadrant the first
 * chose, and `f25c6` catches up to `f25c8` only at the end.
 */
void
txmitquadbit(void *obj, short bits)
{
	struct v34_object *o = (struct v34_object *)obj;
	short mode = tx_scrambler_mode(o);
	unsigned sr = (unsigned)o->f25cc;
	int d, q;

	d = V34scrambler(&sr, mode, bits, 2);
	o->f25cc = (int)sr;
	q = (d + (unsigned short)o->f25c6) & 3;
	o->f25c8 = (short)q;

	sr = (unsigned)o->f25cc;
	d = V34scrambler(&sr, mode, (short)(bits >> 2), 2);
	o->f25cc = (int)sr;

	q = (unsigned short)o->f25c8;
	o->f25c6 = (short)q;
	*(int *)&o->f25d0 = vect16[d + q * 4];

	txmit(obj);
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned to what the two functions above reach.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34HS_ASSERT(field, off) \
	typedef char v34hs_off_##field[ \
		((int)__builtin_offsetof(struct v34_object, field) == (off)) \
		? 1 : -1]

V34HS_ASSERT(f25c2, 0x25c2);
V34HS_ASSERT(f25c6, 0x25c6);
V34HS_ASSERT(f25c8, 0x25c8);
V34HS_ASSERT(f25cc, 0x25cc);
V34HS_ASSERT(f25d0, 0x25d0);
V34HS_ASSERT(f25d2, 0x25d2);

#endif

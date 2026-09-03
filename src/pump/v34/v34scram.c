/*
 * v34scram.c -- ITU-T V.34: the scrambler and descrambler, both polynomials.
 *
 * Four functions, 783 bytes, and `preinitdigital` installs one scrambler and
 * one descrambler by ADDRESS according to which end of the call this is --
 * `role == 0x65` picks GPC to send and GPA to receive, anything else the
 * other way round (finding F177).  The two ends of a V.34 call must use
 * opposite polynomials, which is what identifies that field.
 *
 * WHERE THEY LIVE.  Between `FloatIIR.cpp`'s methods, which end at 0x5785a,
 * and `getFrame` at 0x579c0; `descrambleGPC` and `descrambleGPA` sit either
 * side of `putFrame` at 0x57fb0.  No local symbol anchors the translation
 * unit, so the four are grouped here by what they are rather than by an
 * extent anyone can prove -- and being interleaved with the frame packers
 * suggests the original had them in one file with those, which this
 * reconstruction does not yet.
 *
 * SIXTEEN BITS AT A TIME, NOT ONE.  Both directions hold a 128-bit shift
 * register in four 32-bit words and advance it by a whole 16-bit step, which
 * is why the code looks nothing like the recommendation's bit-serial
 * definition.  The consequence is the difference between the two
 * polynomials:
 *
 *   GPC's near tap is 18 bits back -- further than one step -- so the
 *   feedback for all sixteen new bits can be taken from words that are
 *   already settled, and one pass does it.
 *
 *   GPA's near tap is 5 bits back, CLOSER than one step, so bits produced
 *   within this step feed bits later in the same step.  `scrambleGPA` folds
 *   the feedback in four times, 5 bits at a time, to cover the sixteen.
 *
 * Neither function derives its polynomial from anything; both are the
 * object's arithmetic transcribed.  Task #47 owns the derivation.
 *
 * THE SCRIPTED-BITS HARNESS.  With `data_enable` set, the scramblers draw
 * their input word from `tx_data` rather than using 0xffff, and the
 * descramblers append each recovered word to `rx_data` until 64 are in.
 * That is a loopback the original could drive its own scrambler pair
 * through, and it is the only path on which a descrambler's output is
 * visible at all: both return 0 unconditionally, so with capture off the
 * recovered bits go nowhere a caller can see and only the register moves.
 */

#include "dsplib/v34fsk.h"
#include "dsplib/v34scram.h"

/*
 * The word the scramblers feed in.
 *
 * 0xffff unless the harness is on and has a word left, and the two
 * conditions are separate tests in the object: capture set but the source
 * exhausted gives 0xffff, not a stall.  The comparison against the length is
 * UNSIGNED, so a negative length exhausts it immediately.
 */
static unsigned
scram_input(struct v34_object *obj)
{
	if (obj->data_enable != 0
	    && (unsigned)obj->tx_rd < (unsigned)obj->tx_n) {
		unsigned v = (unsigned short)obj->tx_data[obj->tx_rd];

		obj->tx_rd++;
		return v;
	}
	return 0xffff;
}

/*
 * And the word the descramblers hand back.
 *
 * Bounded at 64 entries by `cmp $0x3f; ja`, and the bound is checked BEFORE
 * the store, so a full sink drops the word rather than overrunning.  Note
 * the index is also the count and nothing here resets it.
 */
static void
scram_output(struct v34_object *obj, unsigned word)
{
	if (obj->data_enable != 0 && (unsigned)obj->rx_n <= 0x3f) {
		obj->rx_data[obj->rx_n] = (int)word;
		obj->rx_n++;
	}
}

/*
 * Scramble sixteen bits with GPC.
 *
 * One pass: every feedback term reaches back past the step, so the three
 * words can be formed from the old register and the new word alone.
 *
 *     w0' = (in << 16) | (w0 >> 16)
 *     w1' = (w3 << 2)  | (w1 >> 16)
 *     w2' = (w3 << 7)  | (w2 >> 16)
 *     w3' = w0' ^ w1' ^ w2'
 *
 * The `>> 16` are the object's `movzwl` of each word's high half, which is
 * the same thing said the other way.
 *
 * `nbits` is passed and only ever decremented: the return is `nbits - 16`,
 * the caller's count of what is left.  Nothing else reads it.
 */
short
scrambleGPC(void *objp, short nbits)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_scrambler *s = &obj->scrambler;
	unsigned in = scram_input(obj);
	unsigned w0, w1, w2, w3 = s->w[3];

	w0 = (in << 16) | (s->w[0] >> 16);
	s->w[0] = w0;
	w1 = (w3 << 2) | (s->w[1] >> 16);
	s->w[1] = w1;
	w2 = (w3 << 7) | (s->w[2] >> 16);
	s->w[2] = w2;
	s->w[3] = w0 ^ w1 ^ w2;

	return (short)(nbits - 16);
}

/*
 * Scramble sixteen bits with GPA.
 *
 * FOUR FOLDS, because the near tap is inside the step.  `w2'` and `w0'` are
 * formed as in GPC, and then the feedback is applied five bits at a time:
 *
 *     t   = w0' ^ w2'
 *     a   = ((w3 >> 11) | h1) ^ t
 *     a   = ((a << 5)   | h1) ^ t      x3
 *     w3' = a
 *     w1' = h1 | (w3' << 5)
 *
 * where `h1` is the old `w1`'s high half.  `w3 >> 11` is LOGICAL in the
 * object -- `shr`, not `sar` -- and the register is unsigned to match.
 *
 * THAT CHOICE IS NOT OBSERVABLE, which is worth knowing before anyone
 * "simplifies" it: the bits an arithmetic shift would differ in are 31..21
 * of the first fold's result, and the three `<< 5` that follow push all of
 * them out of the word before `w3'` or `w1'` is formed.  Changing `shr` to
 * `sar` here passes the differential test.  It is kept because it is what
 * the object does, not because anything can tell.
 *
 * The four are not a loop in the object and are not one here: writing them
 * as `for (i = 0; i < 3; i++)` hides that the first fold takes its input
 * from a different place than the other three.
 */
short
scrambleGPA(void *objp, short nbits)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_scrambler *s = &obj->scrambler;
	unsigned in = scram_input(obj);
	unsigned w3 = s->w[3];
	unsigned h1 = (unsigned short)(s->w[1] >> 16);
	unsigned w0, w2, t, a;

	w0 = (in << 16) | (s->w[0] >> 16);
	s->w[0] = w0;
	w2 = (w3 << 7) | (s->w[2] >> 16);
	s->w[2] = w2;

	t = w0 ^ w2;
	a = ((w3 >> 11) | h1) ^ t;
	a = ((a << 5) | h1) ^ t;
	a = ((a << 5) | h1) ^ t;
	a = ((a << 5) | h1) ^ t;

	s->w[3] = a;
	s->w[1] = h1 | (a << 5);

	return (short)(nbits - 16);
}

/*
 * The descramblers' common tail: shift the register down by a step and put
 * the caller's leftover bits back at the top.
 *
 * `count` is a bit position, not a word count, and the object keeps it as a
 * short.  The final shift is by `count - 16` where `count` is the value
 * BEFORE this call's bits were added -- the object computes it as
 * `(count + nbits - 16) - nbits`, which is the same number by a longer road.
 *
 * MASKED TO FIVE BITS.  That shift is negative if a caller passes more than
 * sixteen bits at once against a nearly empty register, and `shl %cl` on
 * x86 masks the count to five bits in exactly that case.  Spelling the mask
 * out keeps the C defined and does not change what happens; see D41.
 */
static void
descram_tail(struct v34_descrambler *d, unsigned w1, unsigned w2,
	     unsigned bits, unsigned nbits)
{
	unsigned w0 = d->w[0];
	short count;

	d->w[1] = w1;
	d->w[2] = w2;

	count = (short)(d->count - 16);
	d->count = count;
	d->w[0] = (w0 >> 16)
		  | (bits << (((unsigned)(count - (short)nbits)) & 31));
}

/*
 * Descramble with GPC.  `bits` is `nbits` fresh bits; nothing comes out
 * until the register holds more than 31.
 *
 * Returns 0 always.  The recovered word is only observable through the
 * capture sink -- see the file header.
 */
int
descrambleGPC(void *objp, unsigned short bits, unsigned short nbits)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_descrambler *d = &obj->descrambler;
	unsigned w0, w1, w2;

	w0 = d->w[0] | ((unsigned)bits << (unsigned short)d->count);
	d->w[0] = w0;
	d->count = (short)(d->count + nbits);

	if ((short)d->count <= 0x1f)
		return 0;

	w1 = d->w[1];
	w2 = d->w[2];
	scram_output(obj, (unsigned short)(w0 ^ w1 ^ w2));

	descram_tail(d, (w0 << 2) | (w1 >> 16), (w0 << 7) | (w2 >> 16),
		     bits, nbits);
	return 0;
}

/*
 * Descramble with GPA.
 *
 * The same shape, and the same asymmetry as the transmit side: the near tap
 * is inside the step, so `w1'` is formed from a term that has already had
 * the feedback applied rather than from the old register directly.
 */
int
descrambleGPA(void *objp, unsigned short bits, unsigned short nbits)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_descrambler *d = &obj->descrambler;
	unsigned w0, w1, w2, t;

	w0 = d->w[0] | ((unsigned)bits << (unsigned short)d->count);
	d->w[0] = w0;
	d->count = (short)(d->count + nbits);

	if ((short)d->count <= 0x1f)
		return 0;

	w1 = d->w[1];
	w2 = d->w[2];
	t = (w0 << 5) | w1;
	scram_output(obj, (unsigned short)(w0 ^ t ^ w2));

	descram_tail(d, t >> 16, (w0 << 7) | (w2 >> 16), bits, nbits);
	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34SCRAM_ASSERT(name, field, off) \
	typedef char v34scram_off_##name[ \
		((int)__builtin_offsetof(struct v34_object, field) == (off)) \
		? 1 : -1]

V34SCRAM_ASSERT(sink,    rx_data,    0x0014);
V34SCRAM_ASSERT(sink_n,  rx_n,  0x0114);
V34SCRAM_ASSERT(src,     tx_data,     0x0118);
V34SCRAM_ASSERT(src_len, tx_n, 0x0218);
V34SCRAM_ASSERT(src_n,   tx_rd,   0x021c);
V34SCRAM_ASSERT(dscr,    descrambler,   0x0e74);
V34SCRAM_ASSERT(cap,     data_enable, 0x2214);
V34SCRAM_ASSERT(scr,     scrambler,     0x2a54);

/* The two capture arrays tile exactly onto their index words. */
typedef char v34scram_tiles[
	((0x14 + 0x40 * 4 == 0x114) && (0x118 + 0x40 * 4 == 0x218)) ? 1 : -1];

#endif

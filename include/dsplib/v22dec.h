/*
 * v22dec.h -- V.22 / V.22bis: the two receive slicers.
 *
 * The equaliser block calls one of these once per symbol through
 * `struct v22_fse`'s +0x60 (`call *0x60(%edx)` inside `V22_FSE_receive`), and
 * `SetRxRate` is what installs the right one: the 1200 bit/s slicer for the
 * four-point constellation and the 2400 bit/s one for the sixteen-point one.
 *
 * Reconstructed from dsplibs.o:
 *   FSEv22_decision24 .text 0x0884a0  467 bytes
 *   FSEv22_decision12 .text 0x088680  294 bytes
 *
 * WHAT EACH ONE RETURNS.  Both end the same way, and it is the V.22
 * differential quadrant encoding read backwards: the constellation index they
 * settled on is masked to its two QUADRANT bits, the previously received
 * quadrant is subtracted, the difference is reduced modulo sixteen and indexed
 * into `SMCv22_PMAP` -- the same table `FPM_SMC_encoder` uses to go the other
 * way.  1200 returns that alone, shifted down two, which is the two dibits of
 * a four-point symbol.  2400 returns it unshifted and ORs in the index's low
 * two bits, which are the amplitude pair.  So both return values are in the
 * numbering `SMCv22_*MAP_*BPS` is indexed by, and the low half of the 2400 one
 * is NOT differential.
 *
 * THE PREVIOUS QUADRANT LIVES OUTSIDE THE EQUALISER.  Both slicers reach it
 * through `state->prev_quad`, a pointer nothing in the equaliser initialises,
 * so the datapump owns the storage.
 */

#ifndef DSPLIB_V22DEC_H
#define DSPLIB_V22DEC_H

#include "dsplib/v22_fse.h"

/*
 * The four-point decision tables, one entry per constellation point.
 *
 * `DECv22_ANGL12` is the ideal carrier phase the PLL runs its error against;
 * `DECv22_IMAP12` and `DECv22_QMAP12` are the ideal coordinates, at twice the
 * scale of `SMCv22_*MAP_1200BPS` -- the search that matches a decision back to
 * a transmit index halves the transmit table rather than doubling this one.
 */
extern const short DECv22_ANGL12[4];
extern const short DECv22_QMAP12[4];
extern const short DECv22_IMAP12[4];

/* The sixteen-point set, laid out quadrant-major: see the note in v22dec.c. */
extern const short DECv22_ANGL24[16];
extern const short DECv22_QMAP24[16];
extern const short DECv22_IMAP24[16];

/*
 * The three ring magnitudes of the sixteen-point constellation, indexed by
 * `(|i| + |q|) >> 12` minus one.  The middle entry, 12953, is the constant the
 * 1200 slicer reports unconditionally -- its four points are all on that ring.
 */
extern const short DECv22_MAG24[3];

/*
 * THE CONSTELLATION INDEX IS A TWO-FIELD WORD, in both alphabets, and both
 * slicers take it apart.  These are MULTI-BIT fields, not flags, so they are
 * named as a width and a shift.
 *
 * Evidence, and it is not the author's own words -- the slicers reference no
 * format string, `relocscan.py --range .text:0x884a0-0x887a6` finds nothing.
 * It is the callee plus the table layout:
 *
 *   - `SMCv22_PMAP` holds { 4, 0, 8, 12 }, which is a quadrant already shifted
 *     into bits 3:2, and `FPM_SMC_encoder` consumes that same numbering going
 *     the other way.  So bits 3:2 of an index are the quadrant, and the shift
 *     that turns the field into 0..3 is two.
 *   - `DECv22_IMAP24` and `DECv22_QMAP24` change sign exactly with bits 3 and
 *     2 of their own index -- see the layout note in v22dec.c -- which is the
 *     same statement read off the tables.
 *   - bits 1:0 survive `FSEv22_decision24`'s return untouched, ORed in after
 *     the differential step, which is what makes them the amplitude pair
 *     rather than part of the phase.
 *
 * The four-bit `V22_SYM_MODULO` is the alphabet size and not a third field:
 * the difference it reduces is already a multiple of four, so its low two bits
 * are always clear and masking four bits or two would give the same answer.
 * Four is what the object encodes and four is what is written.
 */
#define V22_SYM_QUAD		0x0c	/* bits 3:2, the quadrant           */
#define V22_SYM_QUAD_SHIFT	2	/* ...down to 0..3                  */
#define V22_SYM_AMP		0x03	/* bits 1:0, the amplitude pair     */
#define V22_SYM_MODULO		0x0f	/* the differential wraps over 16   */

/*
 * How far a constellation LEVEL is shifted to become a coordinate.  The two
 * amplitudes are 1 and 3, and the tables hold 4096 and 12288.
 */
#define V22_DEC_LEVEL_SHIFT	12

unsigned short FSEv22_decision12(struct v22_fse *state, short *angle,
				 short *mag);
unsigned short FSEv22_decision24(struct v22_fse *state, short *angle,
				 short *mag);

#endif /* DSPLIB_V22DEC_H */

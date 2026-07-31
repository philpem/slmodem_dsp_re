/*
 * v23fp.h -- ITU-T V.23 Fixed Point: the 1200/75 bps modem.
 *
 * V.23 is asymmetric.  The forward channel carries 1200 bps FSK (1300 Hz for
 * a mark, 2100 Hz for a space) and the backward channel carries 75 bps in the
 * other direction (390 Hz mark, 450 Hz space), so a V.23 modem is not one
 * modem run twice but two different ones sharing a line.  That is why the
 * object is built from three independent halves rather than a symmetric pair:
 *
 *     CreateV23Modem              the composite, in v23modem.c
 *       -> v23FP_rx_create        1200 bps receiver, v23rx.c
 *       -> v23FP_tx_create        1200 bps transmitter, v23tx.c
 *       -> BwChDem_Create         75 bps backward channel, bwchdem.c
 *
 * The coefficient tables split the same way: each half owns its own static
 * configuration, and only the four filters below are global.  Two of the
 * three halves define statics of the SAME NAMES (`AGCv23_CFG`,
 * `TONEv23_CFG`, `V23_AGC_DEF_ALPHA`, `V23_AGC_DEF_BETA`) at different
 * addresses with different contents, so those must stay file-local in the
 * file that owns them.  Hoisting either into this header would silently merge
 * two different filters into one.
 *
 * STATUS: partial.  V23filt.c (the tables below) is reconstructed; the three
 * halves and the composite are not yet.
 */

#ifndef DSPLIB_V23FP_H
#define DSPLIB_V23FP_H

#include "dsplib/fpm_tone.h"

/*
 * ---------------------------------------------------------------------------
 * V23filt.c -- the four global coefficient tables.
 *
 * That file contributes nothing else: it has no .text at all and no local
 * symbols, so these four objects ARE the translation unit.  Its position is
 * pinned by the STT_FILE order -- its entry sits between v23tx.c's and
 * bwchdem.c's with nothing of its own between them, which is also why the
 * only tables it can own are the global ones.
 *
 * Which filter is which was read off v23FP_rx_create, not guessed from the
 * names: it passes _V23_MRF_FILT to FPM_MRF_init, _V23RX_ANSWER_INTRP and
 * _V23RX_IIR_LPF to FPM_FSD_init inside one config struct, and stores
 * V23_IIR_FILT in the object at +0xb4 with a section count of 4 for
 * FPM_iir_filt_II to use later.
 *
 * NAME COLLISION.  The object also defines `V23_MRF_FILT` -- no leading
 * underscore, 180 bytes, file-local -- and it is NOT V.23's.  The STT_FILE
 * order puts it in Rxcid.c, the caller-ID receiver.  Only the underscored
 * name below belongs here.
 */

/*
 * 48 taps, symmetric, for the multirate filter.  MRFv23_CFG asks for 48 of
 * them, which is the check that this is the right table and the right length.
 */
extern const short _V23_MRF_FILT[48];

/*
 * 15 taps, the FSK demodulator's input interpolator.  Same shape as Bell
 * 103's B103_CHAN_INTRP: one dominant tap off centre with a decaying
 * alternating tail, which is a fractional-delay design rather than a
 * frequency-shaping one.
 */
extern const short _V23RX_ANSWER_INTRP[15];

/*
 * 3 biquads for the demodulator's discriminator lowpass, reached as the
 * FPM_FSD config's `iir` and therefore filtered by FPM_iir_filt -- direct
 * form II, coefficients { -a1, b1, -a2, b2, b0 } per section, Q14.
 */
extern const short _V23RX_IIR_LPF[15];

/*
 * 4 biquads for the receiver's channel filter, filtered by FPM_iir_filt_II --
 * direct form I, coefficients { b0, b2, b1, a2, a1 } per section, Q14.
 *
 * b0 == b2 in all four sections, which is what a symmetric numerator looks
 * like under that ordering and confirms it is this ordering and not the other
 * one.  Under FPM_iir_filt's order the same bytes would read as two feedback
 * terms that happen to be equal, which is not a thing filters do by accident.
 */
extern const short V23_IIR_FILT[20];

/*
 * ---------------------------------------------------------------------------
 * v23tx.c -- the transmitter, 32 bytes.
 *
 * One object serves either channel.  What makes it the 1200 bps forward
 * transmitter or the 75 bps backward one is entirely in the arguments
 * v23FP_tx_create is given, so the frequencies and the bit-period table are
 * fields rather than constants.
 *
 * 32-BIT LAYOUT: `period` and `tone` are pointers, so the offsets in the
 * comments hold only where they are four bytes wide.  The assertions in
 * src/pump/v23/v23tx.c are compiled only under that ABI.
 */
struct v23tx {
	short		period_index;	/* +0x00 where in `period` we are    */
	short		period_len;	/* +0x02 its length                  */
	const short	*period;	/* +0x04 samples per bit, cyclic:
					 *       { 7, 7, 6 } forward,
					 *       { 107, 107, 106 } backward  */
	unsigned short	remaining;	/* +0x08 samples left in this bit    */
	short		pad0a;
	int		held;		/* +0x0c the bit being sent, kept
					 *       across a call boundary.  NOT
					 *       initialised by create -- see
					 *       the note there              */
	int		resume;		/* +0x10 `held` is mid-transmission  */
	int		mute;		/* +0x14 one-shot: emit silence for
					 *       one block, then clear       */
	short		space;		/* +0x18 Hz for a 0 bit: 2100 forward,
					 *       450 backward                */
	short		mark;		/* +0x1a Hz for a 1 bit: 1300 forward,
					 *       390 backward                */
	struct fpm_tone	*tone;		/* +0x1c the generator               */
};

/*
 * Build a transmitter.  NULL `state` allocates one.  `period` is not copied
 * -- the object keeps the caller's pointer, so it must outlive the object.
 */
struct v23tx *v23FP_tx_create(struct v23tx *tx, short mark, short space,
			      short period_len, const short *period, int mute);

/* Tear one down, freeing the tone generator and then the object itself. */
void v23FP_tx_delete(struct v23tx *tx);

/*
 * Generate `count` samples from `bits`, one int per bit, writing how many
 * bits that finished through `consumed`.
 *
 * The original returns whatever happens to be in %eax and no caller uses it,
 * so this is declared void rather than inventing a return value.
 */
void v23FP_tx_progress(struct v23tx *tx, short *out, int count,
		       const int *bits, int *consumed);

#endif /* DSPLIB_V23FP_H */

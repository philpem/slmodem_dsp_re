/*
 * b103fp.h -- Bell 103 / V.21 fixed-point datapump.
 *
 * `B103FP` is the modulation proper: the object `b103_create` builds and the
 * half-duplex state machines drive.  It is two allocations deep --
 *
 *     struct b103fp        88 bytes, the object b103_create holds
 *       +0x50 -> hdx       36 bytes, the transmit state machine's context
 *       +0x54 -> dsp      256 bytes, every DSP block and buffer
 *
 * -- and everything the signal path touches lives in the second one.  See
 * docs/findings.md section 20 for the allocation tree and its ownership
 * rules.
 *
 * 32-BIT LAYOUT.  These two structs describe the memory of a 32-bit object,
 * so their reserved regions are byte counts that only hold when pointers are
 * four bytes wide.  The offset assertions in src/pump/b103/b103fp.c are
 * compiled only under that ABI, and say so.
 *
 * The DSP block is now almost fully accounted for.  Its five sub-objects sit
 * end to end with no slack at all -- AGC at +0x0c, the two rate converters at
 * +0x64 and +0x80, the demodulator at +0x9c and the modulator at +0xd4, each
 * starting exactly where the previous one ends -- which is a strong check on
 * all five of their sizes at once.  Only +0x38..+0x63 and a few odd words
 * remain unattributed.
 *
 * STATUS: partial.  The transmit path (ModDataB103, TxNoCarrierB103) and
 * CarrierDetectB103 are reconstructed.  The compile-time checks at the bottom
 * of src/pump/b103/b103fp.c pin every named offset.
 */

#ifndef DSPLIB_B103FP_H
#define DSPLIB_B103FP_H

#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fsd.h"
#include "dsplib/fpm_fsm.h"
#include "dsplib/fpm_mrf.h"

/*
 * The DSP block, `struct b103fp`'s +0x54.  256 bytes.
 *
 * Reserved regions are named for the offset they start at, so a field added
 * later can be sited without recounting.
 */
struct b103_dsp {
	int r00;		/* +0x00 copied into agc.f18 before each
				 *       FPM_AGC_agc call, which never
				 *       reads it -- purpose unknown       */
	int rx_energy;		/* +0x04 <- agc.signal after each block    */
	int rx_tone;		/* +0x08 set by the receive state machine  */
	struct fpm_agc agc;	/* +0x0c                                   */
	unsigned char r38[0x2c];/* +0x38 .. +0x63                          */
	struct fpm_mrf tx_mrf;	/* +0x64 7200 -> 8000, the transmit side   */
	struct fpm_mrf rx_mrf;	/* +0x80 8000 -> 2400, the receive side    */
	struct fpm_fsd fsd;	/* +0x9c the FSK demodulator               */
	struct fpm_fsm fsm;	/* +0xd4 the FSK modulator                 */
	unsigned char re4[4];	/* +0xe4 .. +0xe7                          */
	short *scratch;		/* +0xe8 324 bytes, shared by both paths   */
	short *rx_scratch;	/* +0xec 324 bytes                         */
	void *p_f0;		/* +0xf0 84 bytes                          */
	unsigned char rf4[8];	/* +0xf4 .. +0xfb                          */
	short rx_state;		/* +0xfc receive state machine             */
	short rfe;		/* +0xfe                                   */
};

/* The object itself, 88 bytes. */
struct b103fp {
	unsigned char r00[0x50];/* +0x00 .. +0x4f config and timing         */
	void *hdx;		/* +0x50 transmit state machine context     */
	struct b103_dsp *dsp;	/* +0x54                                    */
};

/*
 * Modulate `nbits` bits into `out` and return the number of 8 kHz samples
 * produced.
 *
 * Two stages: FPM_FSM_modulate writes 24 samples per bit into the scratch
 * buffer at 7200 Hz, then FPM_MRF_filter lifts that to 8000.  The bit count
 * and the sample count are therefore different numbers, and the return is the
 * second one.
 */
short ModDataB103(struct b103fp *fp, const unsigned short *bits, short *out,
		  unsigned short nbits);

/*
 * The same, with the carrier off: identical timing and identical sample
 * count, but silence.  Used to keep the transmitter running through a gap
 * without a discontinuity.
 */
short TxNoCarrierB103(struct b103fp *fp, const unsigned short *bits,
		      short *out, unsigned short nbits);

/* Carrier present: both receiver flags at once. */
int CarrierDetectB103(struct b103fp *fp);

#endif /* DSPLIB_B103FP_H */

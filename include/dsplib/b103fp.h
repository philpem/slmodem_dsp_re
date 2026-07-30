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
 * STATUS: partial.  The transmit path (ModDataB103, TxNoCarrierB103) and
 * CarrierDetectB103 are reconstructed.  The structs below therefore name only
 * the fields those need; everything else is reserved, sized so the offsets
 * that ARE known land where the original puts them.  The compile-time checks
 * at the bottom of src/pump/b103/b103fp.c enforce that.
 */

#ifndef DSPLIB_B103FP_H
#define DSPLIB_B103FP_H

#include "dsplib/fpm_fsm.h"
#include "dsplib/fpm_mrf.h"

/*
 * The DSP block, `struct b103fp`'s +0x54.  256 bytes.
 *
 * Reserved regions are named for the offset they start at, so a field added
 * later can be sited without recounting.
 */
struct b103_dsp {
	int r00;		/* +0x00                                    */
	int rx_energy;		/* +0x04 set by the receiver                */
	int rx_tone;		/* +0x08 set by the receiver                */
	unsigned char r0c[0x58];/* +0x0c .. +0x63                           */
	struct fpm_mrf tx_mrf;	/* +0x64 7200 -> 8000, the transmit side    */
	unsigned char r80[0x54];/* +0x80 .. +0xd3                           */
	struct fpm_fsm fsm;	/* +0xd4 the FSK modulator                  */
	unsigned char re4[4];	/* +0xe4 .. +0xe7                           */
	short *scratch;		/* +0xe8 324 bytes: the 7200 Hz staging buf */
	short *rx_scratch;	/* +0xec 324 bytes                          */
	void *p_f0;		/* +0xf0 84 bytes                           */
	unsigned char rf4[12];	/* +0xf4 .. +0xff                           */
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

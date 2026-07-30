/*
 * b103fp.h -- Bell 103 / V.21 Fixed Point: the modulation proper.
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

struct b103fp;

/*
 * The half-duplex states are all one type, so B103FP_modem can hold the
 * current one as a pointer.  For transmit `in` is the bit stream and `out`
 * the samples; for receive it is the other way round.  `count` is in/out:
 * the caller sets it, the state consumes it and zeroes it.  The return is
 * however many units were produced in the other direction.
 */
typedef short (*b103_hdx_fn)(struct b103fp *fp, short *in, short *out,
			     short *count);

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
	struct fpm_agc agc;	/* +0x0c the data path's gain control      */
	struct fpm_agc det_agc;	/* +0x38 the acquisition path's, run on an
				 *       untouched copy of the input       */
	struct fpm_mrf tx_mrf;	/* +0x64 7200 -> 8000, the transmit side   */
	struct fpm_mrf rx_mrf;	/* +0x80 8000 -> 2400, the receive side    */
	struct fpm_fsd fsd;	/* +0x9c the FSK demodulator               */
	struct fpm_fsm fsm;	/* +0xd4 the FSK modulator                 */
	unsigned char re4[4];	/* +0xe4 .. +0xe7                          */
	short *scratch;		/* +0xe8 324 bytes, shared by both paths   */
	short *rx_scratch;	/* +0xec 324 bytes                         */
	short *bpf_hist;	/* +0xf0 84 bytes: the channel filter's
				 *       circular history                  */
	const short *bpf;	/* +0xf4 B103_BPF_CALLER or _ANSWER, chosen
				 *       by B103FP_create on is_answer     */
	short bpf_idx;		/* +0xf8 its write position                */
	short bpf_taps;		/* +0xfa 40 for the caller, 50 for answer  */
	short rx_state;		/* +0xfc receive state machine             */
	short rfe;		/* +0xfe                                   */
};

/*
 * The half-duplex context, `struct b103fp`'s +0x50.  36 bytes.
 *
 * Despite the name it carries both directions' odds and ends: the transmit
 * state machine's entry point, and the two tone objects the receiver uses.
 */
struct b103_hdx {
	short mode;		/* +0x00 index into B103NextState:
				 *       0 loopback, 1 originate, 2 answer */
	short tone_timeout;	/* +0x02 blocks to wait for the answer tone;
				 *       cfg[0x0c]/20, floored at 700      */
	short tx_blocks;	/* +0x04 transmit countdown                */
	short pad06;
	b103_hdx_fn tx;		/* +0x08 the current transmit state        */
	short rx_count;		/* +0x0c receive-side counter: blocks
				 *       waited, or blocks since carrier   */
	short pad0e;
	b103_hdx_fn rx;		/* +0x10 the current receive state         */
	short substate;		/* +0x14 1 = advance on detection          */
	short pad16;
	int r18;		/* +0x18                                   */
	void *tone_detect;	/* +0x1c FPM_TONE object: the answer tone  */
	void *tone_lo;		/* +0x20 FPM_TONE object: the receive local
				 *       oscillator -- see DemodDataB103   */
};

/* The object itself, 88 bytes. */
struct b103fp {
	int r00;		/* +0x00                                    */
	int is_answer;		/* +0x04 zero selects the caller side       */
	unsigned char r08[0x14];/* +0x08 .. +0x1b config and timing         */
	unsigned char status;	/* +0x1c reported upward; 5 = timed out
				 *       waiting, 6 = carrier lost          */
	unsigned char flags;	/* +0x1d see B103_FLAG_* below              */
	unsigned char r1e[2];	/* +0x1e .. +0x1f                           */
	unsigned char r20[0x30];/* +0x20 .. +0x4f                           */
	struct b103_hdx *hdx;	/* +0x50                                    */
	struct b103_dsp *dsp;	/* +0x54                                    */
};

/*
 * Bits in `flags`.  Named for what sets and clears them; the meaning the
 * layer above attaches to them is not yet established.
 */
#define B103_FLAG_TIMEOUT  0x02	/* set when a wait expires               */
#define B103_FLAG_CARRIER  0x20	/* tracks CarrierDetectB103 each block   */
#define B103_FLAG_80       0x80	/* cleared on every receive-data block   */

/*
 * `hdx->substate`.  These are the ORIGINAL AUTHOR'S names, recovered from the
 * debug strings the blob still carries at .rodata.str1.1+0x3d5c onward -- the
 * NextState tables print them, so they label the case rather than being our
 * invention.  There is no name for 4: reaching it means connected, and asking
 * for a next state from there prints "default".
 */
#define B103_STATE_START    0
#define B103_STATE_CARRDET  1
#define B103_STATE_WAIT1    2
#define B103_STATE_WAIT2    3
#define B103_STATE_DATA     4

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

/*
 * The receive chain.  `in` is modified IN PLACE -- it is mixed with the local
 * oscillator before anything else touches it -- and `count` samples at 8 kHz
 * become at most a handful of bits, which is the return value.
 */
short DemodDataB103(struct b103fp *fp, short *in, unsigned short *bits_out,
		    unsigned short count);

/*
 * The half-duplex state machines.  Each ends by dispatching through
 * B103NextState[hdx->mode] when its condition is met, which is what advances
 * `hdx->state` to the next one.
 */
short TxHdxStartB103(struct b103fp *fp, short *in, short *out, short *count);
short TxHdxDataB103(struct b103fp *fp, short *in, short *out, short *count);
short TxHdxMarksB103(struct b103fp *fp, short *in, short *out, short *count);
short TxHdxSilenceB103(struct b103fp *fp, short *in, short *out, short *count);
short RxDetMarkB103(struct b103fp *fp, short *in, short *out, short *count);
short RxHdxStartB103(struct b103fp *fp, short *in, short *out, short *count);
short RxHdxDataB103(struct b103fp *fp, short *in, short *out, short *count);

/* Indexed by hdx->mode; entries are the three B103*NextState functions. */
extern void (*const B103NextState[3])(struct b103fp *fp);

/*
 * One call of the datapump, both directions.  `n_tx` and `n_rx` are in/out and
 * change units: n_tx takes bits and returns samples, n_rx takes samples and
 * returns bits.  `rx_in` is filtered IN PLACE.  Returns the 32-bit word at
 * fp+0x1c -- status in the low byte, flags in the next.
 */
int B103FP_modem(struct b103fp *fp, const int *tx_bits, short *tx_out,
		 short *rx_in, int *rx_bits, short *n_tx, short *n_rx);

void B103LocLoopNextState(struct b103fp *fp);
void B103OriginateNextState(struct b103fp *fp);
void B103AnswerNextState(struct b103fp *fp);

#endif /* DSPLIB_B103FP_H */

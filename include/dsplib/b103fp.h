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
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_tone.h"

struct b103fp;

/*
 * ---------------------------------------------------------------------------
 * The datapump configuration: 28 bytes, copied wholesale into the first 28
 * bytes of the object by B103FP_create.
 *
 * Field meanings were established by sweeping each word and observing the
 * resulting object, not by reading B103FP_create.  See src/pump/b103/b103_cfg.c
 * for the evidence and findings F32 and F35 for why that distinction matters.
 */
struct b103_cfg {
	int call_type;		/* +0x00 B103_CALL_*; see below.  THE field --
				 *       the only one that changes the object's
				 *       shape, and the only one that decides
				 *       whether a call can complete at all. */
	int v21;		/* +0x04 selects the V.21 tone plan instead of
				 *       Bell 103's.  NOT a caller/answer flag,
				 *       despite where it is read -- see the
				 *       tone table below.  Also read by
				 *       TxHdxMarksB103, which drops the
				 *       requirement to have acquired before
				 *       ending the mark hold.               */
	int loop_high_channel;	/* +0x08 LOOPBACK ONLY: non-zero transmits
				 *       2025/2225 instead of 1070/1270.
				 *       Ignored for originate and answer,
				 *       which take their tones from call_type */
	int tone_timeout_ticks;	/* +0x0c hdx->tone_timeout = max(this/20, 700),
				 *       in blocks.  14000 gives exactly the
				 *       700 floor, so the clamp is a no-op for
				 *       the built-in config and only bites if
				 *       a caller lowers it */
	int f10;		/* +0x10 gates a branch in B103FP_create --
				 *       non-zero skips the block that would
				 *       pre-advance the half-duplex state.
				 *       No effect on any field observed so
				 *       far, but it is NOT inert.           */
	int f14;		/* +0x14 no observed effect                  */
	int tx_scale;		/* +0x18 modulator output gain, straight into
				 *       fsm.scale.  3200 built in            */
};

/*
 * `call_type` and `v21` together choose the tone plan.  Anything other than 0
 * or 1 for call_type is loopback -- B103FP_create has no range check, it
 * simply falls through to the default arm.
 *
 * call_type | v21 | transmits | detects | local osc | bandpass       | plan
 * ----------|-----|-----------|---------|-----------|----------------|------
 *     0     |  0  | 1070/1270 |  2225   |  1350.1   | CALLER, 40     | Bell 103 originate
 *     0     |  1  | 1180/ 980 |  1650   |   975.1   | CALLER, 40     | V.21 channel 1
 *     1     |  0  | 2025/2225 |  1270   |   395.0   | ANSWER, 50     | Bell 103 answer
 *     1     |  1  | 2025/2225 |  1180   |   305.2   | ANSWER, 50     | see the note
 *   else    |  -  | see loop_high_channel | none | 1350.1 | none     | loopback
 *
 * The oscillator IS the frequency plan.  Every configuration mixes the pair
 * it *receives* down to 675/875 Hz, straddling the demodulator's 775 Hz
 * discriminator null, so one demodulator design serves every case and only
 * the oscillator differs:
 *
 *     originate  2025/2225 - 1350 = 675/875      V.21 ch1  1650/1850 - 975
 *     answer     1070/1270 -  395 = 675/875      V.21 ch2  980/1180  - 305
 *
 * NOTE the asymmetry in the last row.  With call_type 1 and v21 set, the
 * receive side is configured for V.21 channel 1 correctly -- 305 Hz brings
 * 980/1180 to 675/875 -- but the TRANSMIT tones stay at Bell 103's 2025/2225
 * rather than moving to V.21 channel 2's 1650/1850.  Measured, not inferred.
 * Whether that is a defect or a combination `b103_create` never asks for is
 * open until `b103_create` is decoded; it is recorded rather than filed as a
 * deviation for that reason.
 *
 * NOTE the built-in B103_CFG_data is LOOPBACK.  It installs no bandpass and no
 * tone detector, so an object built from it cannot complete a call; the
 * measured bit error rate for such a station is 0.485.  `b103_create` is
 * expected to build its own copy with call_type set from its caller argument.
 */
#define B103_CALL_ORIGINATE 0
#define B103_CALL_ANSWER    1
#define B103_CALL_LOOPBACK  2

/* `v21` values, for readability at call sites. */
#define B103_TONES_BELL103  0
#define B103_TONES_V21      1

extern const struct b103_cfg B103_CFG_data;

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
	struct fpm_mtd *mtd;	/* +0xe4 the multi-tone detector           */
	short *scratch;		/* +0xe8 324 bytes, shared by both paths   */
	short *rx_scratch;	/* +0xec 324 bytes                         */
	short *bpf_hist;	/* +0xf0 84 bytes: the channel filter's
				 *       circular history                  */
	const short *bpf;	/* +0xf4 B103_BPF_CALLER or _ANSWER, chosen
				 *       by B103FP_create on call_type     */
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
	/*
	 * UNSIGNED, and the object says so: `TxHdxStartB103` loads it with
	 * `movzwl` where a signed `short` gives `movswl`.  Nothing observable
	 * turns on it while the value stays in 0..0x7fff -- which is why 1,104
	 * differential tests never saw it -- but it is the type the original
	 * declared, and the instruction-level comparison found it.
	 * Finding F613.
	 */
	unsigned short mode;	/* +0x00 index into B103NextState:
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
	struct fpm_tone *tone_detect;	/* +0x1c the tone this station is
					 *       waiting to hear          */
	struct fpm_tone *tone_lo;	/* +0x20 the receive local oscillator;
					 *       see DemodDataB103        */
};

/*
 * The object itself, 88 bytes.
 *
 * Its first 28 bytes ARE the configuration -- B103FP_create copies the whole
 * struct in -- so it is embedded rather than duplicated field by field.
 */
struct b103fp {
	struct b103_cfg cfg;	/* +0x00 .. +0x1b                           */
	unsigned char status;	/* +0x1c reported upward; 5 = timed out
				 *       waiting, 6 = carrier lost          */
	unsigned char flags;	/* +0x1d see B103_FLAG_* below              */
	unsigned char r1e[2];	/* +0x1e .. +0x1f                           */
	short *trace;		/* +0x20 <- dsp->fsd.trace, for the caller  */
	unsigned char r24[4];	/* +0x24 .. +0x27                           */
	short *fsd_count;	/* +0x28 <- &dsp->fsd.last_count            */
	unsigned char r2c[0x24];/* +0x2c .. +0x4f                           */
	struct b103_hdx *hdx;	/* +0x50                                    */
	struct b103_dsp *dsp;	/* +0x54                                    */
};

/*
 * Bits in `flags`.  RESOLVED -- see finding F38.
 *
 * Every set and clear site was enumerated, and so was every *read*.  Within
 * dsplibs.o only TWO of the eight bits are ever tested:
 *
 *   0x01  read by B103FP_modem, which clears `status` when it is set.
 *         Set only by B103OriginateNextState's WAIT2 arm.
 *   0x02  read by nothing -- but cleared by B103FP_modem at the top of every
 *         call and set by every timeout path, so it is a ONE-SHOT event bit
 *         that a caller must read each block or lose.
 *
 * The other six are written and never read.  Not by dsplibs, and not outside
 * it either: `b103_process` masks B103FP_modem's return with 0xff, so the
 * flags byte never leaves the library at all.
 *
 *  bit  | set by                                   | ever tested?
 * ------|------------------------------------------|--------------
 *  0x01 | Originate WAIT2                          | YES, by B103FP_modem
 *  0x02 | every timeout path                       | no, but consumed
 *  0x04 | Originate/LocLoop/Answer WAIT1, create   | no
 *  0x08 | Originate WAIT2, LocLoop/Answer WAIT1    | no
 *  0x10 | START, all three tables                  | no
 *  0x20 | RxHdxData, tracking carrier              | no
 *  0x40 | B103FP_create; cleared at CARRDET        | no
 *  0x80 | nothing; cleared by RxHdxData            | no
 *
 * The six are still written faithfully -- they cost nothing and a future
 * caller may want them -- but they are NOT given invented names.  A name
 * implies a meaning, and the meaning of a bit nothing reads is unknowable.
 */
#define B103_FLAG_CLEAR_STATUS 0x01	/* B103FP_modem zeroes status if set */
#define B103_FLAG_TIMEOUT      0x02	/* one-shot; see above               */
#define B103_FLAG_CARRIER      0x20	/* tracks CarrierDetectB103          */

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

/**
 * @brief Modulate bits into Bell 103 / V.21 audio.
 *
 * Two stages: FPM_FSM_modulate() writes 24 samples per bit into the
 * scratch buffer at 7200 Hz, then FPM_MRF_filter() lifts that to 8000 --
 * so the bit count and the sample count are different numbers.
 *
 * @param fp     The datapump.
 * @param bits   @p nbits bits, one per word.
 * @param out    Output samples at 8 kHz.
 * @param nbits  Number of bits to modulate.
 * @return The number of 8 kHz samples produced.
 */
short ModDataB103(struct b103fp *fp, const unsigned short *bits, short *out,
		  unsigned short nbits);

/**
 * @brief Same timing as ModDataB103(), with the carrier off.
 *
 * Identical sample count, but silence. Used to keep the transmitter
 * running through a gap without a discontinuity.
 *
 * @param fp     The datapump.
 * @param bits   Ignored (kept for signature symmetry with ModDataB103()).
 * @param out    Output samples, silence.
 * @param nbits  Number of (silent) bit periods.
 * @return The number of 8 kHz samples produced.
 */
short TxNoCarrierB103(struct b103fp *fp, const unsigned short *bits,
		      short *out, unsigned short nbits);

/**
 * @brief Test whether carrier is currently present.
 * @param fp  The datapump.
 * @return Both receiver flags at once (carrier present).
 */
int CarrierDetectB103(struct b103fp *fp);

/**
 * @brief The receive chain: mix, filter and demodulate.
 *
 * @param fp        The datapump.
 * @param in        Modified IN PLACE -- mixed with the local oscillator
 *                  before anything else touches it.
 * @param bits_out  Demodulated bits.
 * @param count     Samples at 8 kHz.
 * @return The number of bits written to @p bits_out -- at most a
 *         handful per call.
 */
short DemodDataB103(struct b103fp *fp, short *in, unsigned short *bits_out,
		    unsigned short count);

/*
 * The half-duplex state machines.  Each ends by dispatching through
 * B103NextState[hdx->mode] when its condition is met, which is what advances
 * `hdx->state`; none of them chooses its own successor, which is why the same
 * seven states serve originate, answer and local loopback.  `count` is in/out
 * and every state zeroes it after use, so the caller knows the input was
 * consumed even when the state produced nothing.
 */

/** @brief Transmit: nothing to send yet, just advance. */
short TxHdxStartB103(struct b103fp *fp, short *in, short *out, short *count);
/** @brief Transmit: one block of data, via ModDataB103(). */
short TxHdxDataB103(struct b103fp *fp, short *in, short *out, short *count);
/**
 * @brief Transmit: a block of continuous mark.
 *
 * The bit buffer is FILLED with ones here rather than supplied, so the
 * caller hands over an empty buffer and a length. The exit condition
 * differs by tone plan, not direction: V.21 ends the mark hold on the
 * block count alone, Bell 103 also requires this station's own receiver
 * to have acquired.
 */
short TxHdxMarksB103(struct b103fp *fp, short *in, short *out, short *count);
/** @brief Transmit: a block of silence, with the modulator still running (TxNoCarrierB103()). */
short TxHdxSilenceB103(struct b103fp *fp, short *in, short *out, short *count);
/**
 * @brief Receive: wait for the answer tone.
 *
 * Runs the acquisition AGC and the tone detector directly rather than
 * going through DemodDataB103() -- there is no point mixing and
 * demodulating while still waiting. Two exits: the tone arrives (advance,
 * if the substate says to), or `tone_timeout` blocks pass without it
 * (advance anyway, and report status 5).
 */
short RxDetMarkB103(struct b103fp *fp, short *in, short *out, short *count);
/**
 * @brief Receive: demodulate while waiting for carrier.
 *
 * Counts DOWN, unlike RxDetMarkB103(): `rx_count` blocks are allowed
 * before giving up. Carrier appearing advances immediately; running out
 * advances too, but reports status 5.
 */
short RxHdxStartB103(struct b103fp *fp, short *in, short *out, short *count);
/**
 * @brief Receive: carry data, and watch for the carrier going away.
 *
 * `rx_count` is reset to zero on every block with carrier and
 * incremented on every block without, so it counts CONSECUTIVE losses.
 * Eight in a row ends the call with status 6; a single dropout does not.
 */
short RxHdxDataB103(struct b103fp *fp, short *in, short *out, short *count);

/** @brief Indexed by `hdx->mode`; entries are the three `B103*NextState` functions. */
extern void (*const B103NextState[3])(struct b103fp *fp);

/**
 * @brief One call of the datapump, both directions.
 *
 * @param fp       The datapump, updated in place.
 * @param tx_bits  Bits to transmit.
 * @param tx_out   Modulated output samples.
 * @param rx_in    Input samples, filtered IN PLACE.
 * @param rx_bits  Demodulated bits.
 * @param n_tx     In/out, changes units: in, bits available in @p tx_bits;
 *                 out, samples produced in @p tx_out.
 * @param n_rx     In/out, changes units: in, samples available in
 *                 @p rx_in; out, bits produced in @p rx_bits.
 * @return The 32-bit word at `fp+0x1c` -- status in the low byte, flags
 *         in the next.
 */
int B103FP_modem(struct b103fp *fp, const int *tx_bits, short *tx_out,
		 short *rx_in, int *rx_bits, short *n_tx, short *n_rx);

/**
 * @brief Build a Bell 103 / V.21 modulation object.
 *
 * A caller supplying its own @p state must ZERO it first: the sub-object
 * pointers at +0x50 and +0x54 are tested for NULL to decide whether to
 * allocate, so uninitialised memory is read as a tree that already exists.
 *
 * @param state  NULL allocates one.
 * @param cfg    NULL uses #B103_CFG_data, which is loopback and will not
 *               complete a call.
 * @return The object, or NULL on allocation failure.
 */
struct b103fp *B103FP_create(struct b103fp *state, const struct b103_cfg *cfg);

/**
 * @brief Tear a modulation object down.
 *
 * Frees the object itself unconditionally, even when the caller supplied
 * it -- see D8 in docs/deviations.md before passing anything this
 * function did not allocate.
 *
 * @param fp  The object to free.
 */
void B103FP_delete(struct b103fp *fp);

/*
 * The coefficient tables, in src/pump/b103/b103_tables.c.  Reference bytes;
 * regenerating them from the filters' design parameters is task 8.
 */
extern const short B103_MRF_FILT_TX[270];	/* 10:9, 7200 -> 8000       */
extern const short B103_MRF_FILT_RX[90];	/* 3:10, 8000 -> 2400       */
extern const short B103_BPF_CALLER[40];		/* originate channel filter */
extern const short B103_BPF_ANSWER[50];		/* answer channel filter    */
extern const short B103_CHAN_INTRP[15];		/* demodulator input FIR    */
extern const short B103_IIR_LPF[15];		/* 3 biquads: the lowpass   */
extern const short MTDb103_COEF[10];		/* 2 biquads: the detector  */

/* Bell 103's gain-control configuration, in b103_agc_cfg.c. */
extern const struct fpm_agc_cfg AGCb103_CFG_data;

/**
 * @brief Advance the local-loopback half-duplex machine.
 *
 * A switch on `hdx->substate` that installs the next pair of half-duplex
 * states and sets the status the layer above reads (see `B103_STATE_*`
 * for what each substate means). Transmits mark to itself and
 * demodulates it back with no tone detection at all -- the receive
 * state is never set to RxDetMarkB103(), it goes straight to
 * RxHdxDataB103().
 *
 * @param fp  The datapump, whose `hdx->substate` selects the transition.
 */
void B103LocLoopNextState(struct b103fp *fp);

/**
 * @brief Advance the originate half-duplex machine.
 *
 * The four-step Bell 103 calling sequence: START transmits silence and
 * listens for the answer tone (giving up after `tone_timeout` blocks),
 * CARRDET holds for eight more blocks once the tone arrives, WAIT1
 * transmits 40 blocks of mark so the answerer can train, WAIT2 is data
 * both ways.
 *
 * @param fp  The datapump, whose `hdx->substate` selects the transition.
 */
void B103OriginateNextState(struct b103fp *fp);

/**
 * @brief Advance the answer half-duplex machine.
 *
 * Three steps rather than four: the answerer transmits mark from the
 * start (that IS the answer tone the caller is listening for) and jumps
 * from WAIT1 straight to data without an equivalent of WAIT2.
 *
 * @param fp  The datapump, whose `hdx->substate` selects the transition.
 */
void B103AnswerNextState(struct b103fp *fp);

#endif /* DSPLIB_B103FP_H */

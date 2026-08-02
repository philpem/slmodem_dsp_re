/*
 * v23fp.h -- ITU-T V.23 Fixed Point: the 1200/75 bps modem.
 *
 * V.23 is asymmetric.  The forward channel carries 1200 bps FSK (1300 Hz for
 * a mark, 2100 Hz for a space) and the backward channel carries 75 bps in the
 * other direction (390 Hz mark, 450 Hz space), so a V.23 modem is not one
 * modem run twice but two different ones sharing a line.  That is why the
 * object is built from three independent parts rather than a symmetric pair:
 *
 *     CreateV23Modem              the composite, in v23modem.c
 *       -> v23FP_rx_create        1200 bps receiver, v23rx.c
 *       -> v23FP_tx_create        1200 bps transmitter, v23tx.c
 *       -> BwChDem_Create         75 bps backward channel, bwchdem.c
 *
 * The coefficient tables split the same way: each part owns its own static
 * configuration, and only the four filters below are global.  Two of the
 * three parts define statics of the SAME NAMES (`AGCv23_CFG`,
 * `TONEv23_CFG`, `V23_AGC_DEF_ALPHA`, `V23_AGC_DEF_BETA`) at different
 * addresses with different contents, so those must stay file-local in the
 * file that owns them.  Hoisting either into this header would silently merge
 * two different filters into one.
 *
 * STATUS: complete.  All five modules below are reconstructed and driven
 * against the blob, as is the datapump glue that owns the composite -- that
 * lives in v23.h because it is the modem core's interface rather than V.23's.
 */

#ifndef DSPLIB_V23FP_H
#define DSPLIB_V23FP_H

#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fsd.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_tone.h"

/*
 * ---------------------------------------------------------------------------
 * The configuration CreateV23Modem is handed and passes down to the parts.
 * Three fields, and every one of them is read.
 */
struct v23_cfg {
	/*
	 * Two jobs, one byte.  It arms the 2100 Hz answer tone in the
	 * composite -- which is why the modem starts in state 0 rather than
	 * straight in data -- and it is ALSO handed to v23FP_tx_create as the
	 * transmitter's one-shot mute, so the data transmitter stays silent
	 * for its first block while the tone plays.  See D19.
	 */
	unsigned char	answer_tone;	/* +0x00 */
	unsigned char	r01[3];
	/*
	 * Samples per second, 8000 in every configuration this library builds.
	 * The composite turns it into the two durations of the answer-tone
	 * sequence: three seconds of tone and 50 ms of silence.
	 */
	int		sample_rate;	/* +0x04 */
	int		silence_limit;	/* +0x08 give up after this much quiet,
					 *       in milliseconds              */
};

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
 * bits that FINISHED through `consumed`.
 *
 * `bits[0]` must be the next bit that has never been handed over.  A bit left
 * in flight at the end of a call has already been taken from the buffer and
 * is NOT counted in `consumed`, so refill from index zero with `consumed`
 * fresh bits -- do not advance a cursor into a longer array by it, or the
 * held bit goes out twice.  See the note in src/pump/v23/v23tx.c.
 *
 * The original returns whatever happens to be in %eax and no caller uses it,
 * so this is declared void rather than inventing a return value.
 */
void v23FP_tx_progress(struct v23tx *tx, short *out, int count,
		       const int *bits, int *consumed);

/*
 * ---------------------------------------------------------------------------
 * v23rx.c -- the 1200 bps forward-channel receiver, 576 bytes.
 *
 * A conventional FSK receive chain, and structurally the twin of Bell 103's:
 * gain control, a channel filter, resampling, then a delay-line discriminator
 * with its own slicer and bit clock.  What is V.23-specific is the rates.  The
 * datapump speaks 8 kHz; the multirate filter runs 3:4, which lands on 6 kHz;
 * and the demodulator is configured for 5 samples per bit.  6000 / 5 is 1200
 * exactly, so unlike Bell 103 -- which resamples to 7200 to get 24 samples per
 * 300 baud symbol -- nothing here is approximate.
 *
 * The object carries two gain controls, and they are not interchangeable: one
 * runs on a private copy of the input for the carrier detector, the other on
 * the demodulator's own signal after resampling and gets frozen once carrier
 * is up.  See src/pump/v23/v23rx.c.
 */
struct v23rx {
	short		rx_state;	/* +0x00 the acquisition gate: 0, 5,
					 *       10, then 11 for ever        */
	short		pad02;		/* +0x02 never written by anything   */
	struct fpm_tone	*tone;		/* +0x04 the 1300 Hz carrier detector */
	struct fpm_agc	agc;		/* +0x08 the data path's, frozen once
					 *       carrier is up               */
	struct fpm_agc	det_agc;	/* +0x34 the detector's, never frozen */
	struct fpm_mrf	mrf;		/* +0x60 8 kHz -> 6 kHz, 3:4         */
	struct fpm_fsd	fsd;		/* +0x7c the discriminator            */
	const short	*iir_coeff;	/* +0xb4 V23_IIR_FILT                */
	short		*iir_state;	/* +0xb8 16 words, heap-allocated    */
	short		iir_sections;	/* +0xbc 4                           */
	short		padbe;
	/*
	 * The carrier detector's private copy of the block.  160 shorts is one
	 * 20 ms frame at 8 kHz and is the whole of the gap to `bits`; nothing
	 * in the original bounds the copy against it.  See the note in
	 * v23rx.c.
	 */
	short		det_buf[160];	/* +0xc0 .. +0x1ff                   */
	/*
	 * Where FPM_FSD_demodulate writes.  26 is FSDv23_CFG's max_bits of 24
	 * plus the two the demodulator is allowed to overshoot by.
	 */
	unsigned short	bits[26];	/* +0x200 .. +0x233                  */
	short		nbits;		/* +0x234 how many the last call made */
	unsigned short	silence;	/* +0x236 ms of dead line since the
					 *       last block with signal      */
	unsigned short	silence_limit;	/* +0x238 from the configuration     */
	unsigned short	acquire;	/* +0x23a ms spent waiting for the
					 *       1300 Hz carrier             */
	unsigned short	acquire_limit;	/* +0x23c 60000, i.e. one minute     */
	short		status;		/* +0x23e the last return value      */
};

/* Build one.  NULL `state` allocates it; `cfg` supplies the silence timeout. */
struct v23rx *v23FP_rx_create(struct v23rx *rx, const struct v23_cfg *cfg);

/* Tear one down: the tone detector, both filters and the IIR state. */
void v23FP_rx_delete(struct v23rx *rx);

/*
 * Demodulate one block IN PLACE -- `samples` is filtered, resampled and
 * gain-controlled where it lies, so on return it holds 3/4 as many samples as
 * it did.  Returns 0 once carrier is up, 1 while acquiring it, 2 after giving
 * up on it.
 *
 * `bits` and `nbits` are written only on a 0 return, and not even then if the
 * line has gone quiet; see the note in v23rx.c.  A caller must therefore
 * initialise `*nbits` itself, which is the opposite of BwChDem_Progress.
 */
short v23FP_rx_progress(struct v23rx *rx, short *samples, int count,
			int *bits, int *nbits);

/*
 * ---------------------------------------------------------------------------
 * bwchdem.c -- the 75 bps backward-channel demodulator, 104 bytes.
 *
 * Two resonators and an energy comparison rather than a discriminator; see
 * the file comment in src/pump/v23/bwchdem.c for why that is enough at this
 * rate and how the bit timing falls out of it.
 */
struct bwchdem {
	short		mark_state[2];	/* +0x00 the 390 Hz resonator        */
	short		space_state[2];	/* +0x04 the 450 Hz one              */
	short		mark_energy;	/* +0x08 accumulated over one block  */
	short		space_energy;	/* +0x0a                             */
	short		r0c;		/* +0x0c zeroed by create, read by
					 *       nothing                     */
	short		blocks;		/* +0x0e blocks since create, against
					 *       the carrier timeout         */
	int		remaining;	/* +0x10 samples left in this block  */
	int		block_index;	/* +0x14 where in block_size_table   */
	int		settle;		/* +0x18 blocks to skip before the
					 *       next decision is emitted    */
	int		last_bit;	/* +0x1c to notice a transition      */
	short		silence;	/* +0x20 charged 20 per quiet block  */
	short		pad22;
	int		silence_limit;	/* +0x24 from the configuration      */
	short		carrier_blocks;	/* +0x28 consecutive blocks of 390 Hz */
	short		pad2a;
	struct fpm_tone	*tone;		/* +0x2c the carrier detector        */
	struct fpm_agc	agc;		/* +0x30 .. +0x5b                    */
	const short	*iir_coeff;	/* +0x5c the 3-biquad channel filter */
	short		*iir_state;	/* +0x60 12 words, heap-allocated    */
	short		iir_sections;	/* +0x64 3                           */
	short		status;		/* +0x66 the last return value       */
};

/* Build one.  NULL `state` allocates it; `cfg` supplies the silence timeout. */
struct bwchdem *BwChDem_Create(struct bwchdem *bw, const struct v23_cfg *cfg);

/* Tear one down, including the tone detector and the filter state. */
void BwChDem_Delete(struct bwchdem *bw);

/*
 * Demodulate one block IN PLACE -- `samples` is filtered and gain-controlled
 * where it lies.  Returns 0 once carrier is up, 1 while acquiring it, 2 after
 * giving up on it.
 */
short BwChDem_Progress(struct bwchdem *bw, short *samples, short count,
		       int *bits, int *nbits);

/*
 * ---------------------------------------------------------------------------
 * v23modem.c -- the composite, 40 bytes.
 *
 * Two of the three parts above, chosen by `mode`, plus a V.25 answer-tone
 * generator and the three-state sequence that plays it.  A V.23 modem is
 * asymmetric, so the two ends run different code rather than the same code
 * with the frequencies swapped:
 *
 *              transmits                 receives
 *   mode 0     75 bps backward channel   1200 bps forward channel (v23rx)
 *   mode != 0  1200 bps forward channel  75 bps backward channel (bwchdem)
 *
 * Mode 0 is the terminal end of a Viewdata call -- it types at 75 bps and
 * reads pages at 1200 -- and any other mode is the host end.
 */
struct v23modem {
	short		mode;		/* +0x00 0 = terminal, else host     */
	short		pad02;
	int		state;		/* +0x04 0 answer tone, 1 the silence
					 *       after it, 2 data            */
	int		reported;	/* +0x08 the state the debug line last
					 *       announced                   */
	int		elapsed;	/* +0x0c samples spent in this state  */
	int		tone_samples;	/* +0x10 sample_rate * 3, i.e. 3 s   */
	int		silence_samples;/* +0x14 sample_rate / 20, i.e. 50 ms  */
	int		sample_rate;	/* +0x18 kept; nothing in this module
					 *       reads it back               */
	struct fpm_tone	*answer_tone;	/* +0x1c NULL unless it was armed    */
	struct v23tx	*tx;		/* +0x20 whichever channel this end
					 *       transmits on                */
	/*
	 * And whichever it receives on: a `struct v23rx *` when `mode` is
	 * zero and a `struct bwchdem *` otherwise.  One slot, two types, and
	 * `mode` is the only thing that says which -- so every use of it in
	 * v23modem.c is guarded by the same test.
	 */
	void		*rx;		/* +0x24 */
};

/*
 * Build a V.23 modem.
 *
 * `state` MUST be NULL.  A non-null one is not an error and not a crash: the
 * original skips the whole of the construction and fills in only the timing
 * fields, leaving `mode`, `tx` and `rx` as it found them.  See D22.
 */
struct v23modem *CreateV23Modem(struct v23modem *m, int mode,
				const struct v23_cfg *cfg);

/* Tear one down, and everything it built. */
void DeleteV23Modem(struct v23modem *m);

/*
 * One block, both directions.
 *
 * `tx_nbits` is in/out -- in: bits available at `tx_bits`, out: bits the
 * transmitter finished -- exactly as Bell 103's B103FP_modem does it.
 *
 * Returns 1 throughout the answer-tone sequence, and after that whatever the
 * receiver returns: 0 carrier up, 1 acquiring, 2 given up.
 */
short V23ModemMain(struct v23modem *m, int *tx_bits, int *tx_nbits,
		   short *tx_out, int tx_count, short *rx_in, int rx_count,
		   int *rx_bits, int *rx_nbits);

#endif /* DSPLIB_V23FP_H */

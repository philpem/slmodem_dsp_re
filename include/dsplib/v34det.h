/*
 * v34det.h -- ITU-T V.34: the three detector translation units.
 *
 * V.34's handshake spends most of its time waiting for something to appear on
 * the line, and it has three different ways of looking.  They are small, they
 * are leaves, and they are the first V.34 code this reconstruction covers:
 *
 *     detector.c   a two-section IIR band-pass with a level integrator and a
 *                  hysteresis counter, for "is this tone present"
 *     DFTC.c       a bank of single-bin sliding DFTs, for "how much energy is
 *                  at each of these frequencies"
 *     DPSK.c       a delay-and-multiply FSK discriminator -- declared in
 *                  v34fsk.h, because it needs a partial map of the enclosing
 *                  V.34 object and nothing here does
 *
 * The file names come from the object's STT_FILE entries; `detector.c` and
 * `DFTC.c` have no local symbols at all, so the boundary between those two
 * rests on the function names and on link order, not on evidence.  See
 * docs/findings.md.
 *
 * What no test here can tell you yet: SpanDSP has no V.34, so at the time
 * of writing every V.34 claim rests on tier-1 differential testing alone --
 * which proves the reconstruction matches `dsplibs.o` and says nothing about
 * whether `dsplibs.o` is right.  Where that distinction has bitten before it
 * took an independent peer to find it (D4, and finding F87).  So read every
 * "confirmed" in this subtree as "confirmed identical to the original", never
 * as "confirmed correct", until docs/interop.md says otherwise.
 *
 * That is a current gap with a planned closure, not a permanent condition: a
 * real multi-standard modem is to be brought in as a hardware peer, reached
 * over a SIP ATA for audio and a serial port for control.  Design tests in
 * this subtree so that peer can drive them -- bit stream in, bit stream out,
 * at the datapump boundary -- rather than needing a debugger.
 */

#ifndef DSPLIB_V34DET_H
#define DSPLIB_V34DET_H

#include "dsplib/v34recv.h"	/* struct v34_receiver: one map, not four */

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------
 * detector.c -- tone presence and tone absence
 */

/**
 * The level a presence detector must reach before it starts counting.
 *
 * A literal in the original, and the only threshold in this module that
 * does not come from the caller.
 */
#define V34_DET_ARM_LEVEL		0x100

/** `state` value written by detectorinit(). */
#define V34_DET_STATE_WARMUP		1
/** `state` value once warm-up is over. */
#define V34_DET_STATE_RUNNING		2

/**
 * The leaky integrator's decay, 15565/16384 = 0.94998... Applied once per
 * input sample, so at V.34's 9600 Hz host rate the level falls to 1/e in
 * about 2.1 ms.
 */
#define V34_DET_DECAY			0x3ccd

/**
 * @brief One tone presence/absence detector: a two-section IIR band-pass,
 * a level integrator, and a warm-up counter.
 *
 * `coeff` addresses eight shorts making two second-order sections, grouped
 * by role rather than by section:
 *
 *     coeff[0..1]   section 1 feed-forward     (b)
 *     coeff[2..3]   section 2 feed-forward
 *     coeff[4..5]   section 1 feedback         (a, subtracted)
 *     coeff[6..7]   section 2 feedback
 *
 * There is no b0: the input enters each section already scaled by 1/16,
 * which is the only headroom management here and the reason the histories
 * are comfortable in 16 bits.
 */
struct v34_detector {
	const short *coeff;	/* +0x00  eight shorts, see above        */
	short polarity;		/* +0x04  0 = detect presence, else absence */
	short armed;		/* +0x06  presence mode: level has been seen */
	short count;		/* +0x08  starts negative, see `warmup`  */
	short limit;		/* +0x0a  assert once count exceeds this */
	short state;		/* +0x0c  V34_DET_STATE_*                */
	short thresh_hi;	/* +0x0e  absence mode only              */
	short thresh_lo;	/* +0x10                                 */
	short level;		/* +0x12  integrated |output|            */
	/*
	 * Input and output history, [section][tap].
	 *
	 * Two dimensions rather than four named fields because that is what
	 * detectorinit's clearing loop is: a nested pair over section and tap
	 * computing `section * 2 + tap`, which only addresses both sections
	 * as one run if the source had a 2-D array for the compiler to
	 * flatten.
	 */
	short x[2][2];		/* +0x14                                 */
	short y[2][2];		/* +0x1c                                 */
};

/**
 * @brief Configure one tone detector.
 *
 * `warmup` is in calls of tone_detect(), not samples: `count` is seeded to
 * `-warmup` and stepped once per call until it reaches zero, so a detector
 * cannot assert for the first `warmup` blocks however long each block is.
 * `thresh_hi` is used only when `polarity` is non-zero (absence mode).
 *
 * @param d          The detector to configure.
 * @param coeff      Eight-short filter coefficient block (see struct v34_detector).
 * @param polarity   0 to detect tone presence, non-zero for absence.
 * @param limit      Consecutive-hit count that makes the detector assert.
 * @param warmup     Calls to tone_detect() to ignore before it can assert.
 * @param thresh_lo  Lower level threshold (absence mode).
 * @param thresh_hi  Upper level threshold; used only in absence mode.
 */
void detectorinit(struct v34_detector *d, const short *coeff, short polarity,
		  short limit, short warmup, short thresh_lo, short thresh_hi);

/**
 * @brief Run the detector's filter over one block of samples.
 *
 * @param rx     The enclosing V.34 receiver. Only used to clear
 *               `V34_RX_FLAG_DET_PENDING`, and only on the call that
 *               first arms a presence detector.
 * @param d      The detector to run.
 * @param start  First sample of the block.
 * @param end    One past the last sample of the block.
 * @return 1 if the detector is asserting, 0 otherwise.
 */
int tone_detect(struct v34_receiver *rx, struct v34_detector *d, const short *start,
		const short *end);

/* ------------------------------------------------------------------------
 * DFTC.c -- a bank of single-bin sliding DFTs
 */

/**
 * @brief One sliding-DFT frequency bin, 0x2c bytes.
 *
 * Each bin carries the same correlation twice, in two different number
 * systems, and nothing in this module reconciles them: `acc_re`/`acc_im`
 * are 32-bit integers accumulating `(cos * x) >> 6`, from which `energy`
 * is derived with a caller-chosen shift, while `sum_re`/`sum_im` are
 * doubles accumulating the same products unshifted, from which `denergy`
 * is derived. The integer path is what the handshake reads; the double
 * path is reproduced faithfully (it costs real x87 time per bin per
 * sample) even though this reconstruction has found no reader for
 * `denergy` yet, since dropping it would risk hiding something that
 * matters once `V34hshak.c` is translated.
 *
 * `thresh_lo`/`thresh_hi` are not set by anything in this module -- they
 * belong to whoever owns the bank. The only owner reconstructed so far is
 * V34hshak.c's retrain detector, which sets them to 80 and 3000 and
 * compares `energy` (widened unsigned) against them (read signed) on two
 * different arms (finding F212).
 */
struct v34_dftbin {
	short phase;		/* +0x00  14-bit phase accumulator       */
	short inc;		/* +0x02  phase step per sample          */
	int acc_re;		/* +0x04                                 */
	int acc_im;		/* +0x08                                 */
	short energy;		/* +0x0c  written by dftenergy           */
	short shift;		/* +0x0e  written by dftenergy           */
	double sum_re;		/* +0x10                                 */
	double sum_im;		/* +0x18                                 */
	double denergy;		/* +0x20  written by dftenergy           */
	short thresh_lo;	/* +0x28  the bank's owner sets these    */
	short thresh_hi;	/* +0x2a                                 */
};

/* The phase accumulator is 14 bits and the table has 256 entries. */
#define V34_DFT_PHASE_MASK	0x3fff
#define V34_DFT_PHASE_SHIFT	6
#define V34_DFT_QUARTER		0x40	/* costbl index step for 90 degrees */

/**
 * The shared quarter-wave-symmetric cosine table, 256 entries in Q14.
 *
 * Global (rather than file-static) because it is shared with V34RX.c.
 * Emitted as literal data rather than generated, because it is not exactly
 * reproducible from its own generator: see docs/coefficients.md for the
 * one entry that disagrees.
 */
extern const short costbl[256];

/**
 * @brief Read the shared cosine table, wrapping the index.
 * @param idx  Table index, taken modulo the table length.
 * @return `costbl[idx % 256]`.
 */
short cosread(unsigned char idx);

/**
 * @brief Correlate a block of samples against every bin in a bank.
 *
 * The loops are nested sample-outer, bin-inner: one pass touches every
 * bin's accumulator for every sample.
 *
 * @param bins      The bin bank to update.
 * @param nbins     Number of bins in @p bins.
 * @param samples   The input samples.
 * @param nsamples  Number of samples in @p samples.
 */
void dftupdate(struct v34_dftbin *bins, short nbins, const short *samples,
	       short nsamples);

/**
 * @brief Reduce each bin's accumulators to `energy`, `shift` and `denergy`.
 *
 * @param bins   The bin bank to reduce.
 * @param nbins  Number of bins in @p bins.
 * @param scale  Left shift applied to both accumulators before squaring,
 *               letting the caller set where the integer path saturates.
 *               Used as a byte (the object loads it with `movzbl`), so a
 *               scale of 256 shifts by zero rather than by an absurd amount.
 */
void dftenergy(struct v34_dftbin *bins, short nbins, short scale);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34DET_H */

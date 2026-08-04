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
 * WHAT NO TEST HERE CAN TELL YOU *YET*.  SpanDSP has no V.34, so at the time
 * of writing every V.34 claim rests on tier-1 differential testing alone --
 * which proves the reconstruction matches `dsplibs.o` and says nothing about
 * whether `dsplibs.o` is right.  Where that distinction has bitten before it
 * took an independent peer to find it (D4, and finding 87).  So read every
 * "confirmed" in this subtree as "confirmed identical to the original", never
 * as "confirmed correct", until docs/interop.md says otherwise.
 *
 * That is a CURRENT GAP WITH A PLANNED CLOSURE, not a permanent condition: a
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

/*
 * The caller's receiver object, mapped only as far as this module needs.
 *
 * `tone_detect` touches exactly one field of the object it is handed: a flags
 * word it clears a bit in when the detector first sees signal.  The rest of
 * that object belongs to V34hshak.c and V34RX.c and is not reconstructed
 * yet, so it is a pad here rather than a guess.  The pad is NOT the object's
 * real size -- `datapumpv34` reaches +0x260 and the detector itself lives at
 * +0x3564 of the enclosing V.34 object -- and nothing should allocate one of
 * these expecting to have allocated a receiver.
 *
 * The offset is pinned by an assertion at the bottom of detector.c, so when
 * the real struct arrives it cannot silently move.
 *
 * THE OTHER PARTIAL MAP.  v34fsk.h declares `struct v34_object`, which is the
 * same object from a DIFFERENT BASE: this one is the sub-object at
 * V34object+0x264, so `flags` here is V34object+0x386.  Two partial maps of
 * one object is already one more than anybody wants; a third would be a mess.
 * When the next V.34 file needs a field, extend one of these two and say
 * which -- do not start a third.
 */


/*
 * Bit 9 of that word.  Set by the handshake state that arms a detector (for
 * example at 0x6f0e9, an `or $0x200`), cleared by `tone_detect` the first
 * time the integrated level crosses V34_DET_ARM_LEVEL.  So it reads as "a
 * detector has been armed and has not yet seen anything".
 */


/*
 * The level a presence detector must reach before it will start counting.
 * A literal in the original, and the only threshold in this module that does
 * not come from the caller.
 */
#define V34_DET_ARM_LEVEL		0x100

/* `state` values.  1 is what detectorinit writes; 2 is "warm-up over". */
#define V34_DET_STATE_WARMUP		1
#define V34_DET_STATE_RUNNING		2

/*
 * The leaky integrator's decay, 15565/16384 = 0.94998...  Applied once per
 * input sample, so at V.34's 9600 Hz host rate the level falls to 1/e in
 * about 2.1 ms.
 */
#define V34_DET_DECAY			0x3ccd

/*
 * The detector object, 0x24 bytes.
 *
 * `coeff` addresses eight shorts making two second-order sections, grouped by
 * role rather than by section:
 *
 *     coeff[0..1]   section 1 feed-forward     (b)
 *     coeff[2..3]   section 2 feed-forward
 *     coeff[4..5]   section 1 feedback         (a, subtracted)
 *     coeff[6..7]   section 2 feedback
 *
 * There is no b0: the input enters each section already scaled by 1/16, which
 * is the only headroom management in here and the reason the histories are
 * comfortable in 16 bits.
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

/*
 * Configure one detector.  `warmup` is in CALLS of tone_detect, not samples:
 * `count` is seeded to -warmup and stepped once per call until it reaches
 * zero, so a detector cannot assert for the first `warmup` blocks however
 * long each block is.
 *
 * `thresh_hi` is read only when `polarity` is non-zero.  Every call site in
 * the object that passes polarity 0 also passes 0 here, which is consistent
 * with it being unused rather than accidentally zero.
 */
void detectorinit(struct v34_detector *d, const short *coeff, short polarity,
		  short limit, short warmup, short thresh_lo, short thresh_hi);

/*
 * Run the filter over [start, end) and return 1 if the detector is asserting.
 *
 * `rx` is only ever used to clear V34_RX_FLAG_DET_PENDING, and only on the
 * one call that arms a presence detector.
 */
int tone_detect(struct v34_receiver *rx, struct v34_detector *d, const short *start,
		const short *end);

/* ------------------------------------------------------------------------
 * DFTC.c -- a bank of single-bin sliding DFTs
 */

/*
 * One frequency bin, 0x2c bytes.
 *
 * Each bin carries the same correlation twice, in two different number
 * systems, and nothing in this module reconciles them:
 *
 *   - `acc_re` / `acc_im` are 32-bit integers accumulating (cos * x) >> 6,
 *     and `energy` is derived from them with a shift the caller chooses.
 *   - `sum_re` / `sum_im` are doubles accumulating the same products with no
 *     shift at all, and `denergy` is derived from those.
 *
 * The integer path is what the handshake reads.  The double path costs an
 * x87 load, add and store per bin per sample and its result is written to a
 * field this reconstruction has found no reader for -- see docs/findings.md.
 * It is reproduced because dropping it would change nothing observable and
 * hide something that might matter once V34hshak.c is translated.
 *
 * THE FOUR BYTES AT +0x28 ARE TWO THRESHOLDS, and until V34hshak.c's
 * `dftRetrainDetInit` and `detectRetrainReq` were read they were one `int
 * reserved` with no writer and no reader.  Neither function here touches
 * them, which is the point: they belong to whoever owns the bank, and the
 * only owner reconstructed so far is the retrain detector, which writes 80
 * into one and 3000 into the other and then compares `energy` against them
 * on two different arms.  Finding 204.
 *
 * They are read back the way the object reads them, which is not the same
 * way on both sides of the comparison: `energy` is widened UNSIGNED and the
 * threshold SIGNED.  See `detectRetrainReq`.
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

/*
 * The shared quarter-wave-symmetric cosine table, 256 entries in Q14.
 *
 * Global in the object and shared with V34RX.c, which is the only reason it
 * is declared in a header rather than kept static.  It is emitted as literal
 * data because it is NOT reproducible from its own generator: see
 * docs/coefficients.md for the one entry that disagrees.
 */
extern const short costbl[256];

/* costbl[idx], with idx taken modulo the table length.  See dftc.c. */
short cosread(unsigned char idx);

/*
 * Correlate `nsamples` samples against every one of `nbins` bins.
 *
 * The loops are nested sample-outer, bin-inner, so one pass touches every
 * bin's accumulator for every sample.
 */
void dftupdate(struct v34_dftbin *bins, short nbins, const short *samples,
	       short nsamples);

/*
 * Reduce each bin's accumulators to `energy`, `shift` and `denergy`.
 *
 * `scale` is a left shift applied to both accumulators before squaring, so
 * the caller sets the point at which the integer path saturates.  It is used
 * as a byte -- the original loads it with movzbl -- so a scale of 256 shifts
 * by zero rather than by an absurd amount.
 */
void dftenergy(struct v34_dftbin *bins, short nbins, short scale);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34DET_H */

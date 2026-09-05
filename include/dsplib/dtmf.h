/*
 * dtmf.h -- the DTMF receiver's object and entry points.
 *
 * Reconstructed from dsplibs.o `Dtmf.c` (dtmf_test, dtmf_detect,
 * dtmf_progress, dtmf_set_easy) and `Notch.c`.  The object is 152 bytes;
 * `create_dtmf` (0x0adef0, not reconstructed here -- it is reached from
 * `detector_create`, not from this closure) allocates 0x98 of them.
 *
 * HOW THE DETECTOR WORKS, because it is not the usual Goertzel bank.
 *
 * Every tone gets a NOTCH, not a bandpass, and the detector looks for the
 * SMALLEST output rather than the largest: the tone that is present is the
 * one its own notch removes.  `energy[i]` is the running sum of squares of
 * notch i's output over a block, `total` the same for the unfiltered input,
 * so `total - energy[i]` is how much notch i took out.  `dtmf_test` works on
 * exactly those eight numbers plus `total`.
 *
 * The whole thing runs at 4000 Hz.  `dtmf_detect` is called at 8000 and
 * processes every second sample (`phase`); every coefficient bank in the TU
 * -- eur_coef, us_coef and biascoef -- solves to a DTMF frequency, or to
 * 50 Hz for the bias notch, only at fs = 4000.  Finding F1412.
 */

#ifndef DSPLIB_DTMF_H
#define DSPLIB_DTMF_H

/* Digits are reported as an index; 0..15 is the usual 1 2 3 A / 4 5 6 B ... */
#define DTMF_NO_DIGIT	(-1)	/* nothing decided this block             */
#define DTMF_NOT_YET	(-2)	/* this call did not complete a block     */

/* Low group first, then high; `dtmf_test` returns low + 4 * high. */
#define DTMF_TONES	8

/*
 * The receiver.  0x98 bytes.
 *
 * `hist` is the only part with a subtlety: entry 0 is the newest and the
 * whole array shifts up one slot per block, so the "same digit twice, and
 * not before that" test that decides a digit is a fixed pattern of
 * comparisons against hist[1..6] (or hist[1..3] in easy mode).
 */
struct dtmf {
	float notch_state[DTMF_TONES][2];
				/* +0x00 two words per tone notch         */
	float energy[DTMF_TONES];
				/* +0x40 sum of squares of each notch's
				 *       output over the current block     */
	float total;		/* +0x60 sum of squares of the input      */
	short hist[DTMF_TONES];	/* +0x64 last eight block verdicts,
				 *       hist[0] newest                    */
	short count;		/* +0x74 samples in the current block     */
	short phase;		/* +0x76 8000 -> 4000 decimation phase    */
	float bias_state[2];	/* +0x78 the 50 Hz notch's state          */
	unsigned char pad_80[0x90 - 0x80];
				/* +0x80 create_dtmf does not touch these  */
	short held;		/* +0x90 a digit is being reported        */
	short digit;		/* +0x92 which one                        */
	short easy;		/* +0x94 relaxed validity rule            */
	/*
	 * +0x96 was `pad_96[2]`, the struct's LAST member -- REMOVED
	 * (finding F10151).  Trailing padding: `easy` ends at +0x96 and the
	 * struct's own alignment (forced to 4 by the leading `float`
	 * members) rounds `sizeof` up to +0x98 on its own.  Stronger proof
	 * than usual here -- `src/service/dtmf.c`'s existing
	 * `dtmf_size_check[sizeof(struct dtmf) == 0x98 ? 1 : -1]` is a hard
	 * compile-time assertion, and 0x98 is also the literal
	 * `sysdep_malloc(sizeof(struct dtmf))` allocation size in
	 * `create_dtmf`, not adjacency alone.  `dis.py` over every `dtmf`-
	 * touching function (`create_dtmf`, `reset_dtmf`, `dtmf_detect`,
	 * `dtmf_progress`, `dtmf_set_easy`, `dtmf_test`, `dtmf_modem`,
	 * `create_cid_dtmf`, `beepgen_start_dtmf`) finds no access to
	 * offset 0x96/0x97.
	 */
};

/*
 * `mode` picks the coefficient bank AND the block length.  1 is the European
 * plan: eur_coef, a 50 Hz notch ahead of the bank, and 45 decimated samples
 * per block.  Anything else is us_coef, no bias notch, and 40.
 */
#define DTMF_MODE_EUR	1

/**
 * @brief Feed one 8 kHz sample to the notch-bank DTMF detector.
 * @param x     The new sample.
 * @param d     The receiver, updated in place.
 * @param mode  #DTMF_MODE_EUR or any other value (see the struct comment).
 * @return #DTMF_NOT_YET on samples that do not close a block,
 *         #DTMF_NO_DIGIT while a digit is being held, or the digit index
 *         once -- on the block in which the tone STOPS, not the one in
 *         which it starts.
 */
int dtmf_detect(float x, struct dtmf *d, short mode);

/**
 * @brief Run a block of samples through dtmf_detect().
 * @param d        The receiver, updated in place.
 * @param samples  Input samples.
 * @param count    Number of samples.
 * @param mode     Passed through to dtmf_detect().
 * @return The last digit reported that was neither #DTMF_NO_DIGIT nor
 *         #DTMF_NOT_YET.
 */
short dtmf_progress(struct dtmf *d, const float *samples, short count,
		    short mode);

/**
 * @brief The block decision: which notch (if any) took out the most energy.
 *
 * Exported separately from dtmf_detect() because it is the whole of the
 * detector's judgement and has no state of its own.
 *
 * @param e      Eight notch energies.
 * @param total  The block's unfiltered energy.
 * @param mode   Passed through from dtmf_detect().
 * @return A digit index, or -1.
 */
int dtmf_test(const float *e, float total, short mode);

/**
 * @brief Switch to the shorter validity rule. There is no way back.
 * @param d  The receiver to relax.
 */
void dtmf_set_easy(struct dtmf *d);

/**
 * @brief Allocate (when @p d is NULL) and initialise one notch-bank receiver.
 *
 * Every state word zero, `hist` all -1, `digit` -1. Reconstructed in
 * src/service/beepgen.c, which is the file this tree gives the
 * `Beepgen.c` span's leftovers and where 0xadef0 falls in address order;
 * the prototype belongs here.
 *
 * @param d  NULL to allocate a new receiver, or caller-owned storage.
 * @return @p d, or the newly allocated receiver.
 */
struct dtmf *create_dtmf(struct dtmf *d);

/*
 * The notch banks.  Four floats per tone, eight tones, in the order
 * 697 770 852 941 1209 1336 1477 1633 Hz -- see notch.h for what the four
 * mean and docs/coefficients.md for the design.
 */
extern const float eur_coef[4 * DTMF_TONES];
extern const float us_coef[4 * DTMF_TONES];
extern const float biascoef[4];

#endif /* DSPLIB_DTMF_H */

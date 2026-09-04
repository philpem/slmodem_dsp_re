/*
 * dualtone.h -- Dual Tone Detector: answer-tone detection.
 *
 * Despite living in the call-progress module, this is not a call-progress
 * tone detector.  Dial tone, ringback and busy are cadence.c's job.  What
 * this decides is how the far end answered:
 *
 *   a pure tone at 2100 Hz          a modem, announcing itself with the
 *                                   V.25 answer tone or V.8 ANSam
 *   a tone near 1800 or 2250 Hz     a modem, answering with an FSK carrier
 *                                   (V.21 channel 2 at 1650/1850, Bell 103
 *                                   answer at 2025/2225)
 *   energy but no dominant tone     a person
 *   no energy                       nobody
 *
 * which is exactly the distinction CALLPROG_MODEM_ANSWER,
 * CALLPROG_V8BIS_MODEM_ANSWER and CALLPROG_VOICE_ANSWER need.
 *
 * HOW IT MEASURES
 *
 * The same trick FPM_TONE_detect uses, and the opposite of what the name
 * "detect" suggests: the tone is found by *removing* it.  Each candidate
 * frequency has a notch -- a zero on the unit circle with a pole just inside
 * at radius 0.9 -- and the energy that disappears when the signal passes
 * through it is the energy that was at that frequency.  So a large difference
 * means the tone is present, and reading the sign backwards inverts every
 * verdict while still producing plausible-looking numbers.
 *
 * Three notches, at 0.2625, 0.225 and 0.28125 of the sample rate: 2100, 1800
 * and 2250 Hz at the fixed 8000 Hz this module runs at (finding F41).  Notch A
 * alone gives tone A; notches B and C are cascaded and give tone B together,
 * so "tone B" means "somewhere in the FSK answer band", not one frequency.
 *
 * Everything is preceded by a three-section bandpass centred on 2100 Hz,
 * which is what keeps speech and call-progress tones out of the measurement.
 */

#ifndef DSPLIB_DUALTONE_H
#define DSPLIB_DUALTONE_H

/*
 * Verdicts.  The short/long pairs differ only in whether the tone has been
 * continuously present for DUAL_TONE_HOLD samples yet; the caller is expected
 * to act on the confirmed ones and treat the others as "still deciding".
 */
#define DUAL_TONE_NOSIGNAL	0	/* below the energy floor           */
#define DUAL_TONE_OTHER		1	/* energy, but no tone dominates    */
#define DUAL_TONE_A		2	/* 2100 Hz, not yet held long enough */
#define DUAL_TONE_A_CONFIRMED	3	/* 2100 Hz, held                    */
#define DUAL_TONE_B		4	/* FSK band, not yet held long enough */
#define DUAL_TONE_B_CONFIRMED	5	/* FSK band, held                   */

/* Samples a tone must persist before it is confirmed: 1280, or 160 ms. */
#define DUAL_TONE_HOLD		0x500

/* Sections in the input bandpass.  Fixed; the loop is hand-unrolled. */
#define DUAL_TONE_BP_SECTIONS	3

struct dual_tone {
	/*
	 * Leaky energy estimates, all sharing the same one-pole smoother
	 * (alpha 205/256, beta 51/256).  Unsigned in effect: the original
	 * shifts them right logically.
	 */
	int	energy_a;		/* +0x00  at notch A, 2100 Hz     */
	int	energy_b;		/* +0x04  at notches B and C      */
	int	energy;			/* +0x08  total, after bandpass   */

	/*
	 * Fraction of the total energy a tone must carry to win, Q8.  226/256
	 * is 88.3%, a demanding threshold that a tone plus noise will not
	 * meet -- which is the point, since anything less pure should read as
	 * a voice.
	 */
	short	ratio;			/* +0x0c  226                     */

	/*
	 * Total energy below which no decision is made.  Dual_TONE_create
	 * sets it to 1.  Not a typo and not a scale error: the "no signal"
	 * branch is very nearly unreachable, and a detector fed anything at
	 * all reports DUAL_TONE_OTHER rather than DUAL_TONE_NOSIGNAL.
	 */
	short	min_energy;		/* +0x0e  1                       */

	/*
	 * Notch histories, { x[n-1], x[n-2], y[n-1], y[n-2] } each.  Note the
	 * order in memory: A, then C, then B.  B feeds C, so the two that are
	 * cascaded are not adjacent.
	 */
	short	notch_a[4];		/* +0x10  2100 Hz                 */
	short	notch_c[4];		/* +0x18  2250 Hz, fed from B     */
	short	notch_b[4];		/* +0x20  1800 Hz                 */

	/* Input bandpass, four words per section. */
	short	bp[4 * DUAL_TONE_BP_SECTIONS];	/* +0x28                  */

	/*
	 * How long each tone has been continuously present, in samples.  Both
	 * advance by the block size on every call and are cleared whenever
	 * their tone is not the winner.
	 */
	short	hold_a;			/* +0x40                          */
	short	hold_b;			/* +0x42                          */
};

/**
 * @brief Allocate and initialise a dual-tone detector.
 *
 * Takes no arguments -- there is nothing to configure.
 *
 * @return A new detector, or NULL if the allocation fails.
 */
struct dual_tone *Dual_TONE_create(void);

/**
 * @brief Free a detector. Unconditional; `sysdep_free` tolerates NULL.
 * @param st  The detector to free.
 */
void Dual_TONE_delete(struct dual_tone *st);

/**
 * @brief Examine a block of samples for an answer tone.
 *
 * @param st       The detector, updated in place.
 * @param samples  Input samples, not modified.
 * @param count    Number of samples. A call with @p count <= 0 still
 *                 returns a verdict, from the energies retained since the
 *                 last call.
 * @return One of the `DUAL_TONE_*` verdicts.
 */
int Dual_TONE_detect(struct dual_tone *st, const short *samples, int count);

/**
 * @brief Q14 cosine, 2048 points per cycle, from a 513-entry quarter-wave
 * table.
 *
 * Lives in this file because that is where the original put it, not
 * because the detector uses it -- the callers are the dialler and the
 * calling-tone generator.
 *
 * @param phase  Phase, masked to 11 bits -- any input is valid.
 * @return Q14 cosine of the phase.
 */
short TONE_read(short phase);

/**
 * @brief Amplitude threshold for a call-progress detection level.
 * @param level  Detection level, #DETECTION_THRESHOLD_MIN_LEVEL to
 *               #DETECTION_THRESHOLD_MAX_LEVEL; anything else reads past
 *               the 16-entry table. `cadence_create` is the only caller.
 * @return The threshold for @p level, from a table indexed as `45 - level`.
 */
short Get_Detection_Threshold_Table(short level);

#define DETECTION_THRESHOLD_MIN_LEVEL	30
#define DETECTION_THRESHOLD_MAX_LEVEL	45

#endif /* DSPLIB_DUALTONE_H */

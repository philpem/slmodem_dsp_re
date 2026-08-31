/*
 * ringdet.h -- the software ring detector: its configuration, its state, and
 * the four-function `RD_*` wrapper slmodemd actually calls (blob span
 * `voice.c#3`, addresses 0x2130-0x2b55).
 *
 * WHERE THE NAMES COME FROM, tier by tier (CLAUDE.md's evidence order).
 *
 * TIER 1 -- the author's own words, from two format strings.
 *
 *   `RingDetector_Reset`/`_Create` print
 *      "\n Reset Soft Ring: Threshold = %d, Fs = %d, MinFreq = %d,
 *       MaxFreq =%d \n\tminOnDur = %d, minOffDur = %d \n"
 *   which names the six configuration fields and the five copies of them the
 *   detector keeps.
 *
 *   `RD_process` prints "RD: RD: freq = %d, duration = %d\n" with exactly the
 *   two values `RingDetector_GetLastRing` hands back, so +0x2e is the ring
 *   FREQUENCY and +0x24 is the ring DURATION.  Those two are named from the
 *   object's own text and from nothing weaker.
 *
 * TIER 2 -- the caller's types.  slmodemd's `modem.c` declares
 *
 *      extern void RD_ring_details(void *obj, long *freq, long *duration);
 *
 *   and uses the pair as `ring_count = duration * freq / 1000`, with
 *   `freq == 0` meaning "ring starting" and `freq > 0` meaning "ring
 *   finishing".  That fixes the units -- Hz and milliseconds -- and it fixes
 *   what +0x3a/+0x3c are for, because +0x2e is zeroed on the edge that
 *   reports a start and filled from +0x2c on the edge that reports an end.
 *
 * TIER 3 -- USAGE INFERENCE, and it is most of the rest of this struct.  The
 *   detector is a hysteretic comparator whose two levels swap sign each half
 *   cycle; everything below that is a counter, a run length or a level is
 *   named for the arithmetic it takes part in and NOTHING stronger.  Each
 *   such field says in its comment which expression named it, so a future
 *   reader can check the naming rather than believe it.  No field here is
 *   named from a decompiler.
 */

#ifndef DSPLIB_RINGDET_H
#define DSPLIB_RINGDET_H

#ifdef __cplusplus
extern "C" {
#endif

struct ring_detector_cfg {
	int	fs;		/* +0x00 printed as "Fs"         */
	int	min_freq;	/* +0x04 printed as "MinFreq"    */
	int	max_freq;	/* +0x08 printed as "MaxFreq"    */
	int	min_on_dur;	/* +0x0c printed as "minOnDur"   */
	int	min_off_dur;	/* +0x10 printed as "minOffDur"  */
	int	threshold;	/* +0x14 printed as "Threshold";
				 *       sign selects a mode      */
};

/*
 * The three values of +0x3e.  SEARCH is what Reset and Create leave behind
 * and what a lost carrier falls back to; HIGH and LOW are the two half cycles
 * of the comparator, and they differ only in the sign of the two levels.
 *
 * Named from the transitions, tier 3.
 */
#define RD_STATE_SEARCH	1
#define RD_STATE_HIGH	2
#define RD_STATE_LOW	3

struct ring_detector {
	/* --- configuration, clamped by Reset.  Never written afterwards. --- */
	int	min_freq;	/* +0x00 cfg, clamped >= 14        */
	int	max_freq;	/* +0x04 cfg, clamped <= 100       */
	int	min_on_dur;	/* +0x08 cfg, clamped >= 40, ms    */
	int	min_off_dur;	/* +0x0c cfg, clamped >= 120, ms   */
	int	fs;		/* +0x10 sample rate               */

	/*
	 * +0x14 samples since the last edge Process reported.  The reported
	 * duration is `report_samples * 1000 / fs` plus a correction, so this
	 * counts the length of the state being left.
	 */
	int	report_samples;

	/*
	 * +0x18 samples in the current half cycle.  The frequency estimate is
	 * `fs / (half_samples + 1)`, which is what makes this a half-cycle
	 * count and not something else.
	 */
	int	half_samples;

	/*
	 * +0x1c samples spent in SEARCH, i.e. with no ring energy.  Seeded to
	 * a whole period (or to guard_limit) when the detector gives up on a
	 * carrier, and tested against min_off_dur in samples.
	 */
	int	idle_samples;

	/*
	 * +0x20 samples since the last half-cycle crossing, tested against
	 * min_off_dur in samples.  Tracks +0x4c exactly -- the object keeps
	 * both, one 32-bit and one 16-bit, and tests them against different
	 * limits.
	 */
	int	band_samples;

	int	last_duration;	/* +0x24 ms; RD_process prints it as
				 *       "duration" (tier 1)       */

	int	freq_n;		/* +0x28 how many frequency measurements
				 *       freq_avg is the mean of    */

	short	freq_avg;	/* +0x2c running mean ring frequency in Hz;
				 *       copied into last_freq at the end
				 *       of a ring                  */
	short	last_freq;	/* +0x2e Hz; RD_process prints it as "freq"
				 *       (tier 1).  0 while a ring is
				 *       starting, per modem.c      */

	/*
	 * +0x30 the run length a level has to hold before SEARCH will accept
	 * a crossing: |threshold| / (fs / 80).  Also the value +0x4e/+0x50
	 * are reset to, and the marker the half-cycle code compares against
	 * to decide which crossing of the pair completes a cycle.
	 */
	short	idle_debounce;

	/*
	 * +0x32 the same run length once locked -- 2, or 0 when the
	 * configured threshold was negative.
	 */
	short	lock_debounce;

	/*
	 * +0x34 the crossing level once locked -- 100, or 200 when the
	 * configured threshold was negative.  Small compared to threshold,
	 * so the comparator has wide hysteresis.
	 */
	short	lock_level;

	short	threshold;	/* +0x36 |cfg threshold|            */

	/*
	 * +0x38 three quarters of a period at min_freq: 3 * fs / (4 *
	 * min_freq).  A signal that stays inside the guard band for longer
	 * than this is not a ring, and the detector drops back to SEARCH.
	 */
	short	guard_limit;

	short	ring_active;	/* +0x3a the detector's current verdict  */
	short	ring_reported;	/* +0x3c the verdict last handed to the
				 *       caller; a difference is what makes
				 *       Process return 1          */
	short	state;		/* +0x3e RD_STATE_*                 */

	short	above_run;	/* +0x40 consecutive samples above the
				 *       crossing level             */
	short	below_run;	/* +0x42 consecutive samples below it */

	/*
	 * +0x44 / +0x46 the comparator's two levels, upper first, and they
	 * swap ROLES between the half cycles rather than merely sign:
	 *
	 *   HIGH:  upper = +threshold  (full amplitude)
	 *          lower = +lock_level (the crossing that ends this half)
	 *   LOW:   upper = -lock_level (the crossing that ends this half)
	 *          lower = -threshold  (full amplitude)
	 *
	 * In both, a sample between the two is in the guard band.
	 */
	short	upper_level;
	short	lower_level;

	short	guard_run;	/* +0x48 consecutive samples in the guard
				 *       band, tested against guard_limit */
	short	cycles;		/* +0x4a completed cycles; `cycles * 1000 /
				 *       freq_avg` is the ring's on-time in
				 *       ms, which is what names it  */
	short	cross_samples;	/* +0x4c samples since the last crossing,
				 *       tested against one period at
				 *       min_freq.  See band_samples */
	short	above_need;	/* +0x4e run length above_run must beat  */
	short	below_need;	/* +0x50 run length below_run must beat  */
};

/*
 * The object slmodemd holds, and all of it: eight bytes, allocated and zeroed
 * by RD_create.  `modem` is only ever handed back to `modem_get_param`.
 */
struct rd {
	void			*modem;	/* +0x00 */
	struct ring_detector	*det;	/* +0x04 */
};

void RingDetector_Reset(struct ring_detector *s, struct ring_detector_cfg *c);
struct ring_detector *RingDetector_Create(struct ring_detector_cfg *c);
void RingDetector_Delete(struct ring_detector *s);
void RingDetector_GetLastRing(struct ring_detector *s, int *freq, int *dur);
int RingDetector_Process(struct ring_detector *s, short *in,
			 unsigned int count);

void *RD_create(void *modem, unsigned int rate);
void RD_delete(void *obj);
int RD_process(void *obj, void *in, int count);
void RD_ring_details(void *obj, int *freq, int *duration);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_RINGDET_H */

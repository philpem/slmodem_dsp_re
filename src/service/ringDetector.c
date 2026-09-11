/*
 * ringDetector.c -- the software ring detector proper (blob span
 * `ringDetector.c`, .text 0x002360-0x002740, five symbols in the object's
 * own order: RingDetector_Delete, RingDetector_Reset, RingDetector_Create,
 * RingDetector_GetLastRing, RingDetector_Process).
 *
 * A TU of its own in the blob, distinct from `rd.c` (the four slmodemd
 * wrappers) and from `voice.c`.  Keeping it separate is what preserves the
 * cross-TU call boundaries the reference has: rd.c calls these entry points
 * rather than inlining them.
 *
 * TWO STATIC HELPERS have no symbol in the blob: `rd_guard` (the shared
 * guard-band arm, reached from both half cycles by cross-jumping) and
 * `rd_measure` (the per-cycle frequency fold).  Their bytes count against
 * neither side -- CLAUDE.md's inlining-boundary trap.
 */

#include "dsplib/ringdet.h"
#include "dsplib/debug.h"
#include "dsplib/sysdep.h"

void
RingDetector_Delete(struct ring_detector *s)
{
	if (s)
		sysdep_free(s);
}

/*
 * Program the soft ring detector from a configuration block.
 *
 * The clamps and the derived shorts are the object's, including two things
 * that look odd and are faithful:
 *   - min_off_dur is clamped to >= 120 TWICE when the threshold is
 *     negative -- once with everything else and once again at the end;
 *   - idle_debounce divides by fs/80, so an fs below 80 divides by zero.  The
 *     object does exactly that; callers evidently never hand it one.
 *
 * A negative threshold selects the second mode (lock_debounce/lock_level =
 * 0/200 rather than 2/100) and is used through its absolute value
 * everywhere else.
 */
void
RingDetector_Reset(struct ring_detector *s, struct ring_detector_cfg *c)
{
	int thr = c->threshold;
	int athr = thr < 0 ? -thr : thr;

	s->ring_active = 0;
	s->idle_debounce = (short)(athr / (c->fs / 80));
	s->fs = c->fs;
	s->guard_limit = (short)(3 * c->fs / (4 * c->min_freq));
	s->min_freq = c->min_freq;
	s->max_freq = c->max_freq;
	s->min_on_dur = c->min_on_dur;
	s->min_off_dur = c->min_off_dur;
	if (c->min_freq <= 13)
		s->min_freq = 14;
	if (s->max_freq > 100)
		s->max_freq = 100;
	if (s->min_on_dur <= 39)
		s->min_on_dur = 40;
	if (s->min_off_dur <= 119)
		s->min_off_dur = 120;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "\n Reset Soft Ring: Threshold = %d, Fs = %d, MinFreq = %d, MaxFreq =%d \n\tminOnDur = %d, minOffDur = %d \n",
		    c->threshold, s->fs, s->min_freq, s->max_freq,
		    s->min_on_dur, s->min_off_dur);

	s->report_samples = 0;
	s->half_samples = 0;
	s->idle_samples = 0;
	s->freq_avg = 0;
	s->last_freq = 0;
	s->freq_n = 0;
	s->ring_reported = 0;
	s->state = RD_STATE_SEARCH;
	s->above_run = 0;
	s->below_run = 0;
	athr = c->threshold < 0 ? -c->threshold : c->threshold;
	s->threshold = (short)athr;
	s->upper_level = (short)athr;
	s->lower_level = (short)-athr;
	s->guard_run = 0;
	s->cycles = 0;
	s->cross_samples = 0;
	s->band_samples = 0;
	s->above_need = s->idle_debounce;
	s->below_need = s->idle_debounce;
	if (c->threshold < 0) {
		s->lock_debounce = 0;
		s->lock_level = 200;
		if (s->min_off_dur <= 119)
			s->min_off_dur = 120;
	} else {
		s->lock_debounce = 2;
		s->lock_level = 100;
	}
}

/*
 * No NULL check on the allocation: the object dereferences the returned
 * pointer on the very next instruction (`movw $0x0,0x3a(%esi)`), so a failed
 * malloc faults here rather than being reported.  Reproduced, not repaired --
 * `RD_create`'s test of the result is therefore unreachable in the object too.
 */
struct ring_detector *
RingDetector_Create(struct ring_detector_cfg *c)
{
	struct ring_detector *s;

	s = sysdep_malloc(sizeof *s);
	RingDetector_Reset(s, c);
	return s;
}

void
RingDetector_GetLastRing(struct ring_detector *s, int *freq, int *dur)
{
	*freq = s->last_freq;
	*dur = s->last_duration;
}

/*
 * One completed cycle: measure its frequency, fold it into the running mean
 * if it is inside [min_freq, max_freq], and count it.
 *
 * `half` is the half-cycle length in samples PLUS ONE, which is what the
 * object divides by -- so the estimate is fs/(half_samples+1) and never
 * divides by zero.
 */
static void
rd_measure(struct ring_detector *s, int half, int min_freq)
{
	int f = s->fs / half;

	if (f < s->max_freq + 1 && f >= min_freq) {
		int n = s->freq_n;

		s->freq_n = n + 1;
		s->freq_avg = (short)((s->freq_avg * n + f) / (n + 1));
	}
	s->cycles++;
	s->half_samples = 0;
}

/*
 * A sample inside the comparator's guard band -- past the crossing level but
 * short of full amplitude.  Tolerated for up to three quarters of a period at
 * min_freq; beyond that the tone has gone and the detector starts again.
 */
static void
rd_guard(struct ring_detector *s, int band)
{
	s->above_run = 0;
	s->below_run = 0;
	if (++s->guard_run > s->guard_limit) {
		s->cycles = 0;
		s->state = RD_STATE_SEARCH;
		s->idle_samples = s->guard_limit;
		s->guard_run = 0;
		s->above_need = s->idle_debounce;
		s->below_need = s->idle_debounce;
		s->cross_samples = 0;
		s->band_samples = 0;
	} else {
		s->band_samples = band;
	}
}

int
RingDetector_Process(struct ring_detector *s, short *in, unsigned int count)
{
	int min_freq = s->min_freq;
	int off_samples = s->fs * s->min_off_dur / 1000;
	unsigned int i;
	int ret = 0;

	for (i = 0; i < count; i++) {
		int period;

		s->report_samples++;

		if (s->state == RD_STATE_SEARCH) {
			short thr = s->threshold;
			int idle = s->idle_samples + 1;
			short sample;

			s->half_samples = 0;
			sample = in[i];
			if (sample > thr) {
				s->below_run = 0;
				if (++s->above_run > s->above_need) {
					s->state = RD_STATE_HIGH;
					s->upper_level = thr;
					s->below_need = s->lock_debounce;
					s->lower_level = s->lock_level;
					s->idle_samples = 0;
				} else {
					s->idle_samples = idle;
				}
			} else if (sample < -thr) {
				s->above_run = 0;
				if (++s->below_run > s->below_need) {
					s->state = RD_STATE_LOW;
					s->upper_level = (short)-s->lock_level;
					s->above_need = s->lock_debounce;
					s->lower_level = (short)-thr;
					s->idle_samples = 0;
				} else {
					s->idle_samples = idle;
				}
			} else {
				s->idle_samples = idle;
				s->above_run = 0;
				s->below_run = 0;
			}
		} else if (s->state == RD_STATE_HIGH) {
			int half = s->half_samples + 1;
			int band = s->band_samples + 1;
			short sample;

			s->cross_samples++;
			sample = in[i];
			if (sample >= s->lower_level) {
				s->half_samples = half;
				if (sample >= s->upper_level) {
					s->band_samples = band;
					s->above_run = 0;
					s->below_run = 0;
				} else {
					rd_guard(s, band);
				}
			} else {
				s->above_run = 0;
				if (++s->below_run > s->below_need) {
					if (s->below_need == s->idle_debounce)
						rd_measure(s, half, min_freq);
					else
						s->half_samples = half;
					s->state = RD_STATE_LOW;
					s->guard_run = 0;
					s->cross_samples = 0;
					s->band_samples = 0;
				} else {
					s->band_samples = band;
					s->half_samples = half;
				}
			}
		} else if (s->state == RD_STATE_LOW) {
			int half = s->half_samples + 1;
			int band = s->band_samples + 1;
			short sample;

			s->cross_samples++;
			sample = in[i];
			if (sample <= s->upper_level) {
				s->half_samples = half;
				if (sample <= s->lower_level) {
					s->band_samples = band;
					s->above_run = 0;
					s->below_run = 0;
				} else {
					rd_guard(s, band);
				}
			} else {
				s->below_run = 0;
				if (++s->above_run > s->above_need) {
					if (s->above_need == s->idle_debounce)
						rd_measure(s, half, min_freq);
					else
						s->half_samples = half;
					s->state = RD_STATE_HIGH;
					s->guard_run = 0;
					s->cross_samples = 0;
					s->band_samples = 0;
				} else {
					s->band_samples = band;
					s->half_samples = half;
				}
			}
		}

		/*
		 * A whole period at min_freq with no crossing means the tone
		 * has gone: back to SEARCH, with the idle counter seeded to
		 * that period so the "ring has ended" test below sees it.
		 */
		period = s->fs / min_freq;
		if (s->cross_samples > period) {
			s->idle_samples = period;
			s->cycles = 0;
			s->guard_run = 0;
			s->cross_samples = 0;
			s->state = RD_STATE_SEARCH;
			s->above_need = s->idle_debounce;
			s->below_need = s->idle_debounce;
		}

		/*
		 * cycles/freq_avg is the tone's length in seconds.  Once it
		 * passes minOnDur this is a ring, and the reported frequency
		 * is zeroed so the caller reads the edge as a ring STARTING.
		 */
		if (s->freq_avg != 0 && s->cycles > 0 &&
		    s->cycles * 1000 / s->freq_avg > s->min_on_dur) {
			s->ring_active = 1;
			s->last_freq = 0;
		}

		/*
		 * minOffDur of silence, measured either in SEARCH or between
		 * crossings, ends the ring.  The measured frequency is
		 * published on the way out, which is what makes the closing
		 * edge report a positive one.
		 */
		if (s->idle_samples > off_samples ||
		    s->band_samples > off_samples) {
			if (s->freq_avg != 0)
				s->last_freq = s->freq_avg;
			s->freq_avg = 0;
			s->freq_n = 0;
			s->ring_active = 0;
		}
	}

	/*
	 * The verdict is only handed out when it changes, and the duration is
	 * the time spent in the state being left -- corrected by the
	 * difference between the two minimum durations and 20 ms, then held
	 * at the minimum for the state being left.
	 */
	if (s->ring_reported != s->ring_active) {
		int ms;

		if (s->ring_reported == 1) {
			ms = s->min_on_dur - s->min_off_dur + 20
			     + s->report_samples * 1000 / s->fs;
			if (ms < s->min_on_dur)
				ms = s->min_on_dur;
		} else {
			ms = s->min_off_dur - s->min_on_dur - 20
			     + s->report_samples * 1000 / s->fs;
			if (ms < s->min_off_dur)
				ms = s->min_off_dur;
		}
		s->last_duration = ms;
		s->report_samples = 0;
		ret = 1;
	}
	s->ring_reported = s->ring_active;
	return ret;
}


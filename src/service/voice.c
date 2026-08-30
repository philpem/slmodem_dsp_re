/*
 * voice.c -- the voice service's TU (blob spans `voice.c#1..#3`).  Only the
 * ring detector's reset is reconstructed so far; the rest of the TU's
 * symbols (VOICE_*, the detector itself) join it here as they are written.
 */

#include "dsplib/ringdet.h"
#include "dsplib/debug.h"

/*
 * Program the soft ring detector from a configuration block.
 *
 * The clamps and the derived shorts are the object's, including two things
 * that look odd and are faithful:
 *   - min_off_dur is clamped to >= 120 TWICE when the threshold is
 *     negative -- once with everything else and once again at the end;
 *   - short_30 divides by fs/80, so an fs below 80 divides by zero.  The
 *     object does exactly that; callers evidently never hand it one.
 *
 * A negative threshold selects the second mode (short_32/short_34 =
 * 0/200 rather than 2/100) and is used through its absolute value
 * everywhere else.
 */
void
RingDetector_Reset(struct ring_detector *s, struct ring_detector_cfg *c)
{
	int thr = c->threshold;
	int athr = thr < 0 ? -thr : thr;

	s->short_3a = 0;
	s->short_30 = athr / (c->fs / 80);
	s->fs = c->fs;
	s->short_38 = 3 * c->fs / (4 * c->min_freq);
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

	s->int_14 = 0;
	s->int_18 = 0;
	s->int_1c = 0;
	s->short_2c = 0;
	s->short_2e = 0;
	s->int_28 = 0;
	s->short_3c = 0;
	s->short_3e = 1;
	s->short_40 = 0;
	s->short_42 = 0;
	athr = c->threshold < 0 ? -c->threshold : c->threshold;
	s->short_36 = athr;
	s->short_44 = athr;
	s->short_46 = -athr;
	s->short_48 = 0;
	s->short_4a = 0;
	s->short_4c = 0;
	s->int_20 = 0;
	s->short_4e = s->short_30;
	s->short_50 = s->short_30;
	if (c->threshold < 0) {
		s->short_32 = 0;
		s->short_34 = 200;
		if (s->min_off_dur <= 119)
			s->min_off_dur = 120;
	} else {
		s->short_32 = 2;
		s->short_34 = 100;
	}
}

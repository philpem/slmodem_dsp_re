/*
 * voice.c -- the voice service's TU (blob spans `voice.c#1..#3`).  What is
 * reconstructed so far is the SOFTWARE RING DETECTOR, which is all nine
 * symbols of it, in the object's own emission order:
 *
 *     0x2130 RD_create           0x2380 RingDetector_Reset
 *     0x2260 RD_delete           0x2550 RingDetector_Create
 *     0x22c0 RD_process          0x2720 RingDetector_GetLastRing
 *     0x2350 RD_ring_details     0x2740 RingDetector_Process
 *     0x2360 RingDetector_Delete
 *
 * ...and, at the END of the file, four more of the span's symbols:
 *
 *     0x0600 vce_hook_on        0x0660 vce_get_sreg
 *     0x0630 vce_hook_off       0x13b0 STRM_VCE_GetFDSPEnvironmentalParams
 *
 * THEY ARE OUT OF EMISSION ORDER ON PURPOSE, and that is a deviation from
 * this tree's usual rule, so it is written down rather than left to be
 * discovered.  All four precede `RD_create` in the object -- the first three
 * are the span's first three symbols -- so faithful order would put them at
 * the top of this file.  They are appended instead because emission order is
 * a register-allocation carrier (CLAUDE.md's lever 2), the ring detector
 * above was measured in its current position, and this session has no period
 * compiler with which to re-measure it after a move.  Whoever next runs
 * `byteident.py` over this file should try the faithful order and keep it if
 * nothing above regresses.  Finding F8773.
 *
 * The rest of the TU (`VOICE_*`) joins it here as it is written; it is
 * blocked today on the FDSP kernel and the beep generator.
 *
 * WHAT THE DETECTOR IS.  A hysteretic zero-crossing counter run over the
 * incoming 16-bit samples.  `RD_create` picks a threshold from the codec type
 * and hands `RingDetector_Create` a configuration block; `RingDetector_Reset`
 * clamps it and derives three working values from it; `RingDetector_Process`
 * runs a three-state comparator per sample, measures the frequency once per
 * cycle, averages it, and declares a ring once the measured tone has lasted
 * `minOnDur` milliseconds.  It returns 1 on the sample where its verdict
 * CHANGES, and slmodemd then asks `RD_ring_details` for the frequency and
 * duration -- a frequency of 0 meaning "ring starting" and a positive one
 * meaning "ring finishing" (`modem.c`, `modem_ring_detector_process`).
 *
 * TWO FACTORING NOTES, so a per-function byte count is read correctly.
 *
 *   - `RingDetector_Create` in the object is `malloc` followed by the WHOLE
 *     BODY of `RingDetector_Reset`, which GCC 3.4 at -O3 inlines while still
 *     emitting the out-of-line copy the rest of the object calls.  It is
 *     written here as the call it must have been.
 *   - `rd_guard` and `rd_measure` are static helpers with no symbol in the
 *     blob: the object holds one shared copy of the guard-band arm (reached
 *     from both half cycles by cross-jumping) and two identical copies of the
 *     measurement.  Their bytes count against neither side -- CLAUDE.md's
 *     inlining-boundary trap.
 */

#include "dsplib/ringdet.h"
#include "dsplib/vce.h"
#include "dsplib/debug.h"
#include "dsplib/sysdep.h"
#include "dsplib/modem_params.h"

typedef char ring_detector_size_check[
    sizeof(struct ring_detector) == 0x54 ? 1 : -1];
/*
 * `struct rd` is 8 bytes in the object and two POINTERS wide, which is not
 * the same claim off ILP32 -- so the portable half is asserted here and the
 * literal 8 is held by t_ringdet, which compares `harness_alloc_reqsize` on
 * the 32-bit differential build.  A `__SIZEOF_POINTER__` guard would not do:
 * that predefine is GCC 4.6+, so under the period compiler the guard reads
 * `#if 0` and the assertion silently disappears (docs/method/compilers.md).
 */
typedef char rd_size_check[
    sizeof(struct rd) == 2 * sizeof(void *) ? 1 : -1];

/*
 * The two sample rates the detector is written for.  `RD_create` refuses
 * anything else before it allocates.
 */
#define RD_RATE_8000	8000
#define RD_RATE_9600	9600

/*
 * The comparator threshold, chosen from `MDMPRM_CODECTYPE`.  The object
 * names no codec constant anywhere -- the mangling of the V.90 code records
 * only that `__tHardwareCodecTypes__` exists, not its enumerators (see
 * V90CodecType.h) -- so these stay as the numbers the switch tests.
 *
 * The default is NEGATIVE, and that is not a sentinel: Reset takes the
 * threshold's absolute value everywhere and uses its SIGN to pick the second
 * set of debounce constants (0/200 rather than 2/100).
 */
#define RD_THRESHOLD_DEFAULT	(-3000)

void *
RD_create(void *modem, unsigned int rate)
{
	struct rd *rd;
	struct ring_detector_cfg cfg;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("RD: create...\n");

	if (rate != RD_RATE_8000 && rate != RD_RATE_9600)
		return 0;

	rd = (struct rd *)sysdep_malloc(sizeof *rd);
	if (!rd)
		return 0;
	sysdep_memset(rd, 0, sizeof *rd);
	rd->modem = modem;

	cfg.fs = (int)rate;
	cfg.min_freq = 15;
	cfg.max_freq = 80;
	cfg.min_on_dur = 120;
	cfg.min_off_dur = 120;

	switch ((int)modem_get_param(modem, MDMPRM_CODECTYPE)) {
	case 4:
	case 12:
		cfg.threshold = 1000;
		break;
	case 13:
	case 15:
		cfg.threshold = 650;
		break;
	case 14:
		cfg.threshold = 850;
		break;
	default:
		cfg.threshold = RD_THRESHOLD_DEFAULT;
		break;
	}

	rd->det = RingDetector_Create(&cfg);
	if (!rd->det) {
		sysdep_free(rd);
		return 0;
	}
	return rd;
}

void
RD_delete(void *obj)
{
	struct rd *rd = (struct rd *)obj;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("RD: delete...\n");
	RingDetector_Delete(rd->det);
	sysdep_free(rd);
}

int
RD_process(void *obj, void *in, int count)
{
	struct rd *rd = (struct rd *)obj;
	int ret;

	ret = RingDetector_Process(rd->det, (short *)in, (unsigned int)count);
	if (ret) {
		int freq, duration;

		RingDetector_GetLastRing(rd->det, &freq, &duration);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("RD: RD: freq = %d, duration = %d\n",
					     freq, duration);
	}
	return ret;
}

/*
 * slmodemd declares the two out-parameters `long *`; the object stores 32-bit
 * words through them, which is the same thing on the ILP32 target it was
 * built for and not the same thing anywhere else.  `int *` is what the
 * instructions say, so `int *` is what is written -- the same call this tree
 * already made for `dsp_info::clock_deviation`.
 */
void
RD_ring_details(void *obj, int *freq, int *duration)
{
	struct rd *rd = (struct rd *)obj;

	RingDetector_GetLastRing(rd->det, freq, duration);
}

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
	s->idle_debounce = athr / (c->fs / 80);
	s->fs = c->fs;
	s->guard_limit = 3 * c->fs / (4 * c->min_freq);
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
	s->threshold = athr;
	s->upper_level = athr;
	s->lower_level = -athr;
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

	s = (struct ring_detector *)sysdep_malloc(sizeof *s);
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

/* ------------------------------------------------------------------ *
 * The `vce_*` / `STRM_VCE_*` group.  See the note at the top of the   *
 * file about why these sit here and not before `RD_create`.           *
 * ------------------------------------------------------------------ */

/*
 * Off-hook and on-hook notifications.  Both are pure diagnostics in this
 * object: the whole body is the `> 1` gate and one printf, and the argument
 * is printed with `%p` and otherwise untouched, so nothing observable happens
 * at level 0 or 1.  Kept for the reason debug.h gives -- the call site is the
 * author's annotation, and a reconstruction that dropped it would differ in
 * control flow from the object even where the output agreed.
 */
void
vce_hook_on(void *p)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("voice: vce_hook_on (%p)...\n", p);
}

void
vce_hook_off(void *p)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("voice: vce_hook_off (%p)...\n", p);
}

/*
 * The voice service's own S-register reader.
 *
 * slmodemd has a `modem_get_sreg`, and this is NOT a call into it: the object
 * fetches `struct voice_info` under MDMPRM_VOICEINFO and answers seven
 * register numbers out of that block and out of three built-in constants.
 * Everything else reads back 0, including every register the host would have
 * had a value for.
 *
 * The seven, and where each answer comes from:
 *
 *   S24  flash timer                     20, constant
 *   S72  handset gain                    19, constant
 *   S73  voice dial-tone detect delay     3, constant (seconds)
 *   S82  #VSS silence sensitivity        voice_info.silence_detect_sensitivity,
 *                                        reduced to a 0..3 level
 *   S83  #VSP silence period             voice_info.silence_detect_period
 *   S138 mic gain                        voice_info.rx_gain
 *   S139 line record gain                voice_info.rx_gain
 *
 * The block is fetched BEFORE the switch, unconditionally, so the three
 * constant answers still cost a `modem_get_param` call -- which is visible in
 * the object (the call is the first thing the function does) and is worth
 * preserving because a caller's parameter log can see it.
 */
int
vce_get_sreg(void *modem, unsigned int num)
{
	struct voice_info *vi;
	unsigned int level;

	vi = (struct voice_info *)modem_get_param(modem, MDMPRM_VOICEINFO);

	switch (num) {
	case SREG_FLASH_TIMER:
		return VCE_FLASH_TIMER;
	case SREG_HANDSET_GANE:
		return VCE_HANDSET_GAIN;
	case SREG_VOICE_DIALTONE_DETECT_DELAY:
		return VCE_DIALTONE_DETECT_DELAY;
	case SREG_SILENCE_DETECT_SENSITIVITY:
		level = vi->silence_detect_sensitivity
			>> VCE_SILENCE_LEVEL_SHIFT;
		if (level == 0)
			return vi->silence_detect_sensitivity != 0;
		if (level > VCE_SILENCE_LEVEL_MAX)
			return VCE_SILENCE_LEVEL_MAX;
		return level;
	case SREG_SILENCE_DETECT_DURATION:
		return vi->silence_detect_period;
	case SREG_MIC_GAIN:
	case SREG_LINE_RECORD_GAIN:
		return vi->rx_gain;
	}
	return 0;
}

/*
 * The FDSP environment the voice stream runs in: two echo delays, in
 * samples, WRITTEN not read.  Both are constants in this object -- 51 and
 * 369 -- and the incoming values are only ever printed, which is what makes
 * the two debug lines ("old:" before, "new:" after) the whole evidence for
 * the argument names.  A caller therefore cannot influence the answer.
 */
void
STRM_VCE_GetFDSPEnvironmentalParams(short *psFarEchoDelay,
				    short *psNearEchoDelay)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "voice: StrmVCE old: *psFarEchoDelay %d ,*psNearEchoDelay %d \n",
		    *psFarEchoDelay, *psNearEchoDelay);

	*psFarEchoDelay = STRM_VCE_FAR_ECHO_DELAY;
	*psNearEchoDelay = STRM_VCE_NEAR_ECHO_DELAY;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "voice: StrmVCE new: *psFarEchoDelay %d ,*psNearEchoDelay %d \n",
		    *psFarEchoDelay, *psNearEchoDelay);
}

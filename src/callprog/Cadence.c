/*
 * Cadence.c -- Cadence: recognising a call-progress tone by its rhythm.
 *
 * Reconstructed from dsplibs.o Cadence.c:
 *
 *   cadence_delete    .text 0x07d3f0
 *   cadence_progress  .text 0x07cd80
 *   cadence_create    .text 0x07d430
 *   cadence_reset     .text 0x07dff0
 *
 * See cadence.h for what the detector is for.  This file is about the three
 * ways it decides a pattern has matched, which are not variations on a theme:
 * they are three separately written pieces of code that share their inputs.
 */

#include "dsplib/cadence.h"
#include "dsplib/debug.h"
#include "dsplib/modem_params.h"
#include "dsplib/sysdep.h"
#include "dsplib/cpfiltrs.h"
#include "dsplib/elliptic.h"
#include "dsplib/fp_math.h"
#include "dsplib/dualtone.h"


void
cadence_delete(struct cadence *c)
{
	if (c->filter != 0)
		toneiir_delete(c->filter);
	sysdep_free(c);
}

void
cadence_reset(struct cadence *c)
{
	toneiir_reset(c->filter);
	c->state = CADENCE_IN_SILENCE;
	c->run = 0;
	c->n = 0;
	c->failures = 0;
}

/*
 * |a - b|, spelled the way the original does it -- cltd, xor, sub -- which is
 * the usual branchless absolute value and matters only in that it does not
 * special-case INT_MIN.
 */
static int
adiff(int a, int b)
{
	int v = a - b;

	return v < 0 ? -v : v;
}

/*
 * The matching tolerance, in 62.5 ms units.  `GetBusyToneDiffTime` is a
 * country parameter, and whatever it says the detector will not work with
 * less than three.
 */
static int
match_tolerance(struct cadence *c)
{
	int tol = modem_get_param(c->modem, GetBusyToneDiffTime);

	if (tol <= 2)
		tol = 3;
	return tol;
}

/*
 * Form 1: match a fixed pattern.
 *
 * `on[0]`, `off[1]`, `on[1]`, `off[2]` against the four configured values.
 * Note which four: the first tone, the silence after it, the second tone and
 * the silence after that.  `off[0]` is skipped because it is however long the
 * detector was listening before anything happened.
 */
static int
match_fixed(struct cadence *c, int last, int tol)
{
	/*
	 * Announced on entry, before anything is compared -- so at level 2 a
	 * fixed-pattern detector emits this banner every cycle whether or not
	 * anything matches.  "SERIRES" is the author's.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    " CADENCE SERIRES COMPARISON ========================>\n");

	if (c->pattern_min_cycles > last)
		return 0;
	if (adiff(c->on[0], c->pattern[0]) > tol)
		return 0;
	if (adiff(c->off[1], c->pattern[1]) > tol)
		return 0;
	if (adiff(c->on[1], c->pattern[2]) > tol)
		return 0;
	if (adiff(c->off[2], c->pattern[3]) > tol)
		return 0;

	/* The verdict is a level-3 message; the banner above is level 2. */
	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf(
		    "CADENCE %s: CONDITION -- SERIES --- SATISFIED\n",
		    c->name);
	return 1;
}

/*
 * Form 2: a loop over `cycles` periods.
 *
 * The most recent period must fall inside the timing windows, every earlier
 * period back to `cycles` ago must agree with it, and then two comparisons
 * are made against the period `cycles` back that are NOT symmetrical: the
 * silence is a one-sided test (`off[last] > off[first + 1]` fails) while the
 * tone is a two-sided one.  Reproduced as written.
 */
static int
match_looped(struct cadence *c, int last, int tol)
{
	int cycles = c->cycles;
	int off_last = c->off[last];
	int first;
	int k;
	int ok;

	if (cycles - 1 > last)
		return 0;

	ok = 0;
	if (off_last >= c->min_off && off_last <= c->max_off
	    && c->on[last] >= c->min_on && c->on[last] <= c->max_on)
		ok = 1;

	if (cycles > 2) {
		for (k = 1; cycles - 1 > k; k++)
			if (adiff(off_last, c->off[last - k]) >= tol)
				ok = 0;
	}

	first = last - cycles;
	if (off_last > c->off[first + 1])
		return 0;
	if (adiff(c->on[last], c->on[first + 1]) >= tol)
		return 0;

	if (ok != 1)
		return 0;
	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf("CADENCE %s: CONDITION C SATISFIED\n",
				     c->name);
	return 1;
}

/*
 * Form 3: the hand-unrolled match, used when `looped_match` is clear.
 *
 * Two independent patterns are tried and either will do:
 *
 *   a one-period cadence -- the last three tones alike and the last three
 *   silences alike
 *
 *   a two-period cadence -- alternating, so the tone matches the one two and
 *   four cycles back rather than the one immediately before.  That is what a
 *   double ring is, and a one-period test rejects it.
 *
 * The two-period test needs six cycles of history and is skipped below that.
 */
static int
match_unrolled(struct cadence *c, int last, int tol)
{
	int off_last, on_last, off_prev;
	int one_period, two_period;

	if (c->cycles - 1 > last || last <= 1)
		return 0;

	off_last = c->off[last];
	if (off_last < c->min_off || off_last > c->max_off)
		return 0;

	on_last = c->on[last];
	if (on_last < c->min_on || on_last > c->max_on)
		return 0;

	off_prev = c->off[last - 1];

	one_period = 0;
	if (adiff(off_last, off_prev) < tol
	    && adiff(off_last, c->off[last - 2]) < tol
	    && adiff(on_last, c->on[last - 1]) < tol
	    && adiff(on_last, c->on[last - 2]) < tol)
		one_period = 1;

	two_period = 0;
	if (last > 5
	    && adiff(off_last, c->off[last - 2]) < tol
	    && adiff(off_last, c->off[last - 4]) < tol
	    && adiff(on_last, c->on[last - 2]) < tol
	    && adiff(on_last, c->on[last - 4]) < tol
	    && adiff(off_prev, c->off[last - 3]) < tol
	    && adiff(off_prev, c->off[last - 5]) < tol)
		two_period = 1;

	if ((one_period | two_period) == 0)
		return 0;
	/*
	 * The three success messages name the forms: this unrolled test is
	 * the author's CONDITION B, the looped one CONDITION C, the fixed
	 * pattern "-- SERIES --".  No CONDITION A message survives anywhere
	 * in the object.
	 */
	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf("CADENCE %s: CONDITION B SATISFIED\n",
				     c->name);
	return 1;
}

static int
match(struct cadence *c, int last)
{
	int tol = match_tolerance(c);

	if (c->fixed_pattern != 0)
		return match_fixed(c, last, tol);
	if (c->looped_match != 0)
		return match_looped(c, last, tol);
	return match_unrolled(c, last, tol);
}

int
cadence_progress(struct cadence *c, short sample)
{
	int verdict = toneiir_progress(c->filter, sample);
	int result = CADENCE_NOTHING;
	int matched = 0;

	if (verdict == TONEIIR_PRESENT) {
		if (c->continuous != 0)
			result = CADENCE_DETECTED;

		if (c->state == CADENCE_IN_TONE) {
			int run = c->run + 1;

			/*
			 * A detector with neither a pattern nor an upper
			 * bound on the tone has nothing to measure, so it
			 * throws the cycle away and reports at once.  That is
			 * the continuous case again, reached from the other
			 * side.
			 */
			if (c->fixed_pattern == 0 && c->max_on == 0) {
				c->state = CADENCE_IN_SILENCE;
				c->run = 0;
				c->n = 0;
				result = CADENCE_DETECTED;
			} else {
				c->run = run;
			}
		} else if (c->state == CADENCE_IN_SILENCE) {
			c->state = CADENCE_IN_TONE;
			c->off[c->n] = c->run;
			c->run = 0;
		}
	} else if (verdict == TONEIIR_ABSENT) {
		if (c->state == CADENCE_IN_SILENCE) {
			/*
			 * Still silent.  A silence longer than the configured
			 * limit means whatever was being measured is over, so
			 * the recorded cycles are dropped -- but `run` keeps
			 * counting, so the next tone still records how long
			 * the gap was.
			 */
			c->run++;
			if (c->run > c->max_silence)
				c->n = 0;
		} else if (c->state == CADENCE_IN_TONE) {
			int last = c->n;

			c->on[last] = c->run;
			c->n = last + 1;

			/*
			 * A cycle that ends at index zero is the first one
			 * since a reset, and there is nothing to compare it
			 * with.  Counted as a failure: enough of them and the
			 * detector concludes there is no tone here at all.
			 */
			if (last == 0) {
				c->failures++;

				/* After the increment: 0x7d087 stores, then
				 * 0x7d08d branches to the print. */
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"CYCLES_COUNTER= %d\n",
						c->failures);
			}
			else if (c->n >= c->cycles)
				matched = match(c, c->n - 1);

			if (c->n > CADENCE_MAX_PERIODS - 1)
				c->n = 0;
			c->run = 0;
			c->state = CADENCE_IN_SILENCE;

			if (matched) {
				/*
				 * "BUSY" regardless of which tone this
				 * detector is for -- the string is fixed, not
				 * `c->name`, unlike the three CONDITION
				 * messages.  The author's, not a slip of
				 * ours.
				 */
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"BUSY cadence recognized\n");

				toneiir_reset(c->filter);
				c->state = CADENCE_IN_SILENCE;
				c->run = 0;
				c->n = 0;
				result = CADENCE_DETECTED;
			}
		}
	}

	if (c->failures > CADENCE_MAX_FAILURES) {
		/*
		 * "NO ANSWER" is the caller's word for it: CALLPROG_Progress
		 * turns this verdict into CPTD_BUSY_GIVE_UP and prints "no
		 * answer detected by cadence" (finding F154).  Two names for
		 * one event, both the original author's.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("NO ANSWER state recognized\n");

		toneiir_reset(c->filter);
		c->state = CADENCE_IN_SILENCE;
		c->run = 0;
		c->n = 0;
		result = CADENCE_RESTART;
	}

	return result;
}

/*
 * ---------------------------------------------------------------------------
 * cadence_create
 *
 * Four tones, four near-identical blocks, one shared tail.  The parameters
 * each block reads are what name it -- GetMinBusyCadenceOnTime and friends --
 * and the four blocks differ in only three respects: which parameters, how
 * often the underlying toneiir should return a verdict, and whether the tone
 * has a cadence at all.
 */

/*
 * The debug names, .rodata+0x6208, indexed by the clamped tone.  Reproduced
 * because `name` is part of the object and a differential test compares it.
 */
static const char *const cadence_tone_names[CADENCE_TONE_INVALID + 1] = {
	"BUSY", "DIAL", "CONG", "RING", "INVALID"
};

/*
 * Milliseconds-times-ten to toneiir intervals.  `GetFP_Value(1, buflen)` is
 * `ceil(16384 / buflen)`, so this is `t * 80 / buflen` -- and see
 * docs/parameters.md for why that means the country table is in units of
 * 10 ms.  The intermediate `* 80` is spelled `(x * 5) << 4` in the original.
 */
static int
to_intervals(int fp, int t)
{
	return ((fp * t) * 5 << 4) >> 14;
}

/*
 * The filter-bank dispatch.  Five of the eight indices select a bank of seven
 * designs and need a subindex in 1..7; the rest are single designs.  Each
 * bank falls back to its OWN nearest CP_* design rather than a common one,
 * which is why this is a table of three pointers rather than one.
 */
struct cadence_bank {
	const short	*a, *b, *scales;	/* the bank, or NULL         */
	const short	*fb_a, *fb_b, *fb_scales;	/* its fallback      */
};

static void
select_filter(struct cadence *c, int index, int sub)
{
	static const struct cadence_bank banks[8] = {
		{ Filter_350_500_a, Filter_350_500_b, Filter_350_500_scales,
		  CP_350_600_a, CP_350_600_b, CP_350_600_scales },
		{ Filter_100_550_a, Filter_100_550_b, Filter_100_550_scales,
		  CP_350_600_a, CP_350_600_b, CP_350_600_scales },
		{ Filter_350_500_a, Filter_350_500_b, Filter_350_500_scales,
		  CP_350_600_a, CP_350_600_b, CP_350_600_scales },
		{ Filter_276_504_a, Filter_276_504_b, Filter_276_504_scales,
		  CP_276_504_a, CP_276_504_b, CP_276_504_scales },
		{ Filter_100_550_a, Filter_100_550_b, Filter_100_550_scales,
		  CP_350_600_a, CP_350_600_b, CP_350_600_scales },
		{ Filter_100_550_a, Filter_100_550_b, Filter_100_550_scales,
		  CP_350_600_a, CP_350_600_b, CP_350_600_scales },
		{ 0, 0, 0, CP_450_630_a, CP_450_630_b, CP_450_630_scales },
		{ 0, 0, 0, CP_100_550_a, CP_100_550_b, CP_100_550_scales }
	};
	const struct cadence_bank *bank;

	c->sel_n_a = IIR_FILTER_COEFF;
	c->sel_n_b = IIR_FILTER_COEFF;

	if ((unsigned)index > 7) {
		c->sel_a = CP_350_600_a;
		c->sel_b = CP_350_600_b;
		c->sel_scales = CP_350_600_scales;
		return;
	}

	bank = &banks[index];

	/*
	 * One-based, and checked unsigned, so a subindex of 0 -- which is what
	 * slmodemd supplies, always -- lands here rather than reading behind
	 * the table.
	 */
	if (bank->a == 0 || (unsigned)(sub - 1) > 6) {
		c->sel_a = bank->fb_a;
		c->sel_b = bank->fb_b;
		c->sel_scales = bank->fb_scales;
		return;
	}

	c->sel_a = bank->a + IIR_FILTER_COEFF * (sub - 1);
	c->sel_b = bank->b + IIR_FILTER_COEFF * (sub - 1);
	c->sel_scales = bank->scales + IIR_FILTER_SCALES * (sub - 1);
}

/* Windows the busy and ringback cases fall back on, in 10 ms units. */
#define CADENCE_DEFAULT_MAX	55
#define CADENCE_DEFAULT_MIN	20

static void
default_windows(struct cadence *c)
{
	c->max_on = CADENCE_DEFAULT_MAX;
	c->min_on = CADENCE_DEFAULT_MIN;
	c->max_off = CADENCE_DEFAULT_MAX;
	c->min_off = CADENCE_DEFAULT_MIN;
}

static int
windows_are_set(const struct cadence *c)
{
	return c->max_on != 0 && c->min_on != 0
	    && c->max_off != 0 && c->min_off != 0;
}

struct cadence *
cadence_create(struct cadence *c, struct cadence_setup *s, int extra,
	       void *modem)
{
	struct toneiir_cfg cfg;
	int filter_index = 0;
	/*
	 * Zero unless dial tone supplies one, and zero is out of range -- so
	 * every bank selection for busy, congestion and ringback falls back to
	 * that bank's own single design.  See finding F49.
	 */
	int subindex = 0;
	int silence_mult = 4;
	int usable = 1;
	int tone, fp, name;

	/*
	 * Before the object is even known to exist -- the original fetches the
	 * template first and checks its argument second.
	 */
	toneiir_get_default_configuration(&cfg);

	if (c == 0) {
		c = (struct cadence *)sysdep_malloc(sizeof(*c));
		if (c == 0)
			return 0;
		sysdep_memset(c, 0, sizeof(*c));
		c->filter = 0;
	}

	c->modem = modem;
	c->use_allpass = 0;
	c->continuous = 0;
	tone = s->tone;

	/*
	 * Fetched and discarded -- the result is overwritten before it is
	 * read.  Reproduced because the harness counts parameter calls and a
	 * missing one is a difference.
	 */
	(void)modem_get_param(modem, GetDialToneCallProgressFilterIndex);

	{
		short level_fix = (short)modem_get_param(
			modem, GetDialToneDetectionThreshold);

		c->threshold = Get_Detection_Threshold_Table(level_fix);

		/* Raw parameter first, table result second -- 0x4(%esp)
		 * then 0x8(%esp) at 0x7d9a4.  Note the author's spelling
		 * and the \r\n, which the rest of the file does not use. */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"Detection Thresholds: levle_fix=%d," "--> LEVEL_THRESHOLD=%d\r\n",
				level_fix, c->threshold);
	}

	c->cycles = 100;
	c->looped_match = 0;

	if (tone == CADENCE_TONE_BUSY) {
		filter_index = modem_get_param(modem,
					       GetBusyToneCallProgressFilterIndex);

		/*
		 * The same line as the two below with its label rubbed out.
		 * Placed here because its block returns to the
		 * GetMaxBusyCadenceOnTime read at 0x7da0a -- so it is after
		 * the filter index and before the windows.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("============> %d\n",
					     filter_index);

		/*
		 * Busy tolerates a shorter silence than the others before
		 * dropping what it has measured -- three times the maximum
		 * off period rather than four.  This is NOT the filter
		 * subindex, which stays zero here: only dial tone ever sets
		 * one, and conflating the two selects a bank design where the
		 * original falls back.
		 */
		silence_mult = 3;
		c->buflen = 160;
		c->continuous = 0;
		c->validation = 0;
		c->max_on = modem_get_param(modem, GetMaxBusyCadenceOnTime);
		c->min_on = modem_get_param(modem, GetMinBusyCadenceOnTime);
		c->min_off = modem_get_param(modem, GetMinBusyCadenceOffTime);
		c->max_off = modem_get_param(modem, GetMaxBusyCadenceOffTime);
		c->cycles = modem_get_param(modem, GetBusyDetectionCyclesNumber);
		c->looped_match = modem_get_param(modem,
						  GetBusyToneLooseDetectionEnabled);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Cadence: Busy Tone loose " "detection is %d\r\n",
					     c->looped_match);

		if (!windows_are_set(c))
			default_windows(c);
	} else if (tone == CADENCE_TONE_CONG) {
		filter_index = modem_get_param(modem,
					       GetCongestionToneCallProgressFilterIndex);

		/*
		 * "Ringback" in the CONGESTION branch -- the author's
		 * copy-paste, not ours, and the reason the string appears
		 * twice in .rodata.  Which block is which is read from the
		 * return targets: this one lands on the
		 * GetMaxCongestionCadenceOnTime read at 0x7dc12, the other on
		 * the ringback one at 0x7de94.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Ringback index====> %d\n",
					     filter_index);

		c->buflen = 160;
		c->continuous = 0;
		c->validation = 0;
		c->max_on = modem_get_param(modem, GetMaxCongestionCadenceOnTime);
		c->min_on = modem_get_param(modem, GetMinCongestionCadenceOnTime);
		c->min_off = modem_get_param(modem, GetMinCongestionCadenceOffTime);
		c->max_off = modem_get_param(modem, GetMaxCongestionCadenceOffTime);
		c->cycles = modem_get_param(modem,
					    GetCongestionDetectionCyclesNumber);
		if (!windows_are_set(c)) {
			if (DSPLIB_DEBUG_VERBOSE())
				dsplibs_debug_printf(
					"Disable CONGESTION detector\n");

			usable = 0;
			default_windows(c);
		}
	} else if (tone == CADENCE_TONE_RING) {
		filter_index = modem_get_param(modem,
					       GetRingbackToneCallProgressFilterIndex);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Ringback index====> %d\n",
					     filter_index);

		c->buflen = 160;
		c->continuous = 0;
		c->validation = 0;
		c->max_on = modem_get_param(modem, GetMaxRingbackCadenceOnTime);
		c->min_on = modem_get_param(modem, GetMinRingbackCadenceOnTime);
		c->min_off = modem_get_param(modem, GetMinRingbackCadenceOffTime);
		c->max_off = modem_get_param(modem, GetMaxRingbackCadenceOffTime);
		c->cycles = modem_get_param(modem,
					    GetRingbackDetectionCyclesNumber);
		/*
		 * Congestion and ringback both give up if any window is zero.
		 * Busy meets the same condition by substituting defaults and
		 * carrying on -- it is the tone the modem most needs to hear,
		 * so it is the one that refuses to be switched off by a gap in
		 * the country table.
		 */
		if (!windows_are_set(c)) {
			if (DSPLIB_DEBUG_VERBOSE())
				dsplibs_debug_printf(
					"Disable RINGBACK detector\n");

			usable = 0;
			default_windows(c);
		}
	} else {
		/* DIAL, and anything that is not 0, 2 or 3. */
		filter_index = modem_get_param(modem,
					       GetDialToneCallProgressFilterIndex);
		subindex = modem_get_param(modem, GetDialToneFilterSubindex);
		c->buflen = modem_get_param(modem,
					    GetCallProgressSamplesBufferLength);
		if (c->buflen == 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"BUFFER LENGTH is INVALID!\n");

			c->buflen = 666;
		}
		c->continuous = 1;
		c->validation = 100 * modem_get_param(modem,
						      GetDialToneValidationTime);
		c->max_off = 0;
		c->max_on = 0;
	}

	select_filter(c, filter_index, subindex);

	/*
	 * Convert every window from the country table's 10 ms units into
	 * toneiir intervals.  GetFP_Value is called once per window in the
	 * original rather than hoisted, and it is deterministic, so calling it
	 * once here would change nothing except the harness's call count --
	 * which the differential test checks.
	 */
	fp = GetFP_Value(1, (short)c->buflen);
	c->max_on = to_intervals(fp, c->max_on);
	fp = GetFP_Value(1, (short)c->buflen);
	c->min_on = to_intervals(fp, c->min_on);
	fp = GetFP_Value(1, (short)c->buflen);
	c->max_off = to_intervals(fp, c->max_off);
	fp = GetFP_Value(1, (short)c->buflen);
	c->min_off = to_intervals(fp, c->min_off);

	/* Computed from the already-converted max_off. */
	c->max_silence = silence_mult * c->max_off;

	if (extra > 0)
		c->continuous = 0;

	c->int_2a4 = s->w6;

	/*
	 * The clamp is for the NAME only, and it is written back into the
	 * caller's descriptor.  Behaviour was already decided above, so a
	 * tone of 7 is configured as dial tone and labelled INVALID.
	 */
	name = s->tone;
	if ((unsigned)name > CADENCE_TONE_INVALID)
		name = CADENCE_TONE_INVALID;
	s->tone = name;
	c->name = cadence_tone_names[name];

	/*
	 * Nine consecutive gated prints in the object (0x7d87e onward), the
	 * whole configuration in one block.  It comes after the name is
	 * resolved because the first line needs it, and after the window
	 * conversion because every time it prints is in intervals, not in the
	 * country table's units.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("TYPE %s\n", c->name);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Filter index %d\n", filter_index);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Filter SubIndex %d\n", subindex);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("MAX_ON_TIME %d Buffers     "
				     "MIN_ON_TIME %d Buffers\n",
				     c->max_on, c->min_on);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("MAX_OFF_TIME %d Buffers    "
				     "MIN_OFF_TIME %d Buffers\n",
				     c->max_off, c->min_off);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("OFF_TIME_THAT_RESETS_CYCLE %d\n",
				     c->max_silence);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("BUFFER LENGTH %d samples.\n",
				     c->buflen);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("INTEGRATION_LENGTH %d[ms]\n",
				     c->validation);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("LEVEL %d\n", c->threshold);

	/*
	 * THE REPORT COMES FIRST, then the refusal.  An unusable window is
	 * announced in full -- type, filter, both cadence windows, buffer and
	 * level -- and only then freed, which is the opposite of the order
	 * this had.  The transcript is what says so: a zero-window RING
	 * printed two lines where the object printed eleven, because we
	 * returned before the report and it returns after.  Invisible to
	 * every other check, since both sides return NULL either way.
	 * Finding F201.
	 */
	if (!usable) {
		if (c->filter != 0)
			toneiir_delete(c->filter);
		sysdep_free(c);
		return 0;
	}

	cfg.a = c->sel_a;
	cfg.b = c->sel_b;
	cfg.n_a = c->sel_n_a;
	cfg.n_b = c->sel_n_b;
	cfg.interval = (short)c->buflen;
	cfg.threshold = (cfg.threshold & ~0xffff) | (unsigned short)c->threshold;
	/*
	 * AFTER the report above, which is how the transcript places it: the
	 * object prints INTEGRATION_LENGTH as the full validation time and
	 * only then takes the interval off.  Nothing else can see the
	 * difference -- the value that reaches `cfg` is the same either way,
	 * so the whole-object comparison agrees with the subtraction in either
	 * position.  Finding F194.
	 */
	if (c->validation > 100)
		c->validation -= 100;

	cfg.duration_ms = (extra + 1) * c->validation;
	cfg.keep_on_gap = c->continuous;
	cfg.scales = c->sel_scales;

	c->filter = toneiir_create(c->filter, &cfg);

	c->n = 0;
	c->state = CADENCE_IN_SILENCE;
	c->run = 0;

	/*
	 * Two more fields put through the same conversion, and nothing ever
	 * assigns them -- so on a fresh object they are zero going in and
	 * zero coming out.  Reproduced because they are part of the object.
	 */
	fp = GetFP_Value(1, (short)c->buflen);
	c->int_278 = to_intervals(fp, c->int_278);
	fp = GetFP_Value(1, (short)c->buflen);
	c->int_274 = to_intervals(fp, c->int_274);

	c->fixed_pattern = 0;
	c->int_27c = s->w3;

	return c;
}

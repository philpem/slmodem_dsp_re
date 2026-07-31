/*
 * cadence.c -- Cadence: recognising a call-progress tone by its rhythm.
 *
 * Reconstructed from dsplibs.o Cadence.c:
 *
 *   cadence_delete    .text 0x07d3f0
 *   cadence_progress  .text 0x07cd80
 *   cadence_create    .text 0x07d430   (not yet reconstructed)
 *   cadence_reset     .text 0x07dff0
 *
 * See cadence.h for what the detector is for.  This file is about the three
 * ways it decides a pattern has matched, which are not variations on a theme:
 * they are three separately written pieces of code that share their inputs.
 */

#include "dsplib/cadence.h"
#include "dsplib/modem_params.h"
#include "dsplib/sysdep.h"

extern int modem_get_param(void *modem, int name);

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

	return ok == 1;
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

	return (one_period | two_period) != 0;
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
			if (last == 0)
				c->failures++;
			else if (c->n >= c->cycles)
				matched = match(c, c->n - 1);

			if (c->n > CADENCE_MAX_PERIODS - 1)
				c->n = 0;
			c->run = 0;
			c->state = CADENCE_IN_SILENCE;

			if (matched) {
				toneiir_reset(c->filter);
				c->state = CADENCE_IN_SILENCE;
				c->run = 0;
				c->n = 0;
				result = CADENCE_DETECTED;
			}
		}
	}

	if (c->failures > CADENCE_MAX_FAILURES) {
		toneiir_reset(c->filter);
		c->state = CADENCE_IN_SILENCE;
		c->run = 0;
		c->n = 0;
		result = CADENCE_RESTART;
	}

	return result;
}

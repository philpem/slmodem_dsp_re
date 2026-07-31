/*
 * cadence.h -- Cadence: recognising a call-progress tone by its rhythm.
 *
 * Dial tone, ringback, busy and congestion are all built from the same few
 * frequencies; what tells them apart is timing.  Ringback is roughly a second
 * on and two off, busy is half a second each way, congestion is faster still,
 * and dial tone never stops.  So a cadence detector does not ask "which
 * frequency is this" -- toneiir already answered that -- but "how long has it
 * been on, how long off, and does that repeat".
 *
 * One of these exists per tone the supervisor is listening for, each with its
 * own filter and its own timing windows, and all of the windows come from the
 * country's homologation table (see docs/parameters.md).  A British busy tone
 * and an American one are the same detector with different numbers.
 *
 * HOW IT WORKS
 *
 * `cadence_progress` takes one sample, passes it to its `toneiir`, and does
 * nothing at all until that returns a verdict -- which is once every 500
 * samples, so everything below is measured in 62.5 ms units, not samples.
 *
 * On each verdict it tracks how long the current period has lasted and, at
 * every transition, records it:
 *
 *     silence -> tone   off[n] = run
 *     tone -> silence   on[n]  = run, then n++
 *
 * `off[0]` is therefore however long the detector happened to be listening
 * before the first tone, which is meaningless, and the comparisons below skip
 * it.  Once `n` reaches the configured cycle count the recorded periods are
 * matched against the expected pattern, in one of three forms.
 */

#ifndef DSPLIB_CADENCE_H
#define DSPLIB_CADENCE_H

#include "dsplib/toneiir.h"

/*
 * Recorded periods.  Two arrays of sixty; `n` wraps to zero rather than
 * stopping, so a tone that never matches is measured forever.
 */
#define CADENCE_MAX_PERIODS	60

/*
 * Consecutive failures to see any tone at all before the detector gives up,
 * resets its filter and reports CADENCE_RESTART.
 */
#define CADENCE_MAX_FAILURES	9

/* Returned by cadence_progress. */
#define CADENCE_NOTHING		0	/* no verdict this sample         */
#define CADENCE_DETECTED	1	/* the pattern matched            */
#define CADENCE_RESTART		7	/* gave up and reset              */

/*
 * `state` is which half of the cycle the detector is in.  Only 0 and 1 are
 * ever stored, but the code tests for them separately and falls through on
 * anything else, so they are named rather than treated as a boolean.
 */
#define CADENCE_IN_TONE		0
#define CADENCE_IN_SILENCE	1

struct cadence {
	struct toneiir	*filter;			/* +0x000 */

	int	state;					/* +0x004 */
	int	run;		/* periods elapsed in this half   +0x008 */

	int	on[CADENCE_MAX_PERIODS];		/* +0x00c */
	int	off[CADENCE_MAX_PERIODS];		/* +0x0fc */

	int	n;		/* complete cycles recorded       +0x1ec */
	int	failures;	/* consecutive silent cycles      +0x1f0 */

	/*
	 * A hundred bytes neither cadence_create nor cadence_progress ever
	 * touches -- twenty-five words between the counters and the
	 * configuration.  Kept so the object is the 732 bytes the original
	 * allocates; the most likely explanation is a third array that a
	 * removed feature used.
	 */
	int	reserved[25];				/* +0x1f4 */

	/*
	 * Timing windows, in 62.5 ms units, from the country table:
	 * GetMin/MaxBusyCadenceOnTime and friends.
	 */
	int	max_on;					/* +0x258 */
	int	max_off;				/* +0x25c */
	int	min_on;					/* +0x260 */
	int	min_off;				/* +0x264 */

	/* GetBusyDetectionCyclesNumber and its ringback/congestion twins. */
	int	cycles;					/* +0x268 */

	/*
	 * When set, every interval in which the tone is present reports
	 * CADENCE_DETECTED rather than waiting for a pattern.  That is how
	 * dial tone is detected: it has no cadence to match.
	 */
	int	continuous;				/* +0x26c */

	/* Longest silence tolerated before the recorded cycles are dropped. */
	int	max_silence;				/* +0x270 */

	int	f274, f278, f27c;			/* +0x274 */

	/*
	 * The filter design cadence_create selected, staged here before being
	 * copied into the toneiir configuration.
	 */
	int		sel_n_a;			/* +0x280 */
	int		sel_n_b;			/* +0x284 */
	const short	*sel_a;				/* +0x288 */
	const short	*sel_b;				/* +0x28c */
	const short	*sel_scales;			/* +0x290 */

	int	f294, f298, f29c;			/* +0x294 */
	const char *name;	/* for debug output               +0x2a0 */
	int	f2a4;					/* +0x2a4 */

	/*
	 * Always zero.  cadence_create clears it at entry and never sets it,
	 * so the branch that would build the filter from
	 * `toneiir_configuration_allpass` -- whose scales pointer is NULL --
	 * cannot be taken.  See finding 46.
	 */
	int	use_allpass;				/* +0x2a8 */

	/*
	 * Selects between two spellings of the same windowed match: a loop
	 * over `cycles` periods when set, and a hand-unrolled check of the
	 * last two and last three when clear.  They are not equivalent -- see
	 * src/callprog/cadence.c.
	 */
	int	looped_match;				/* +0x2ac */

	/*
	 * The exact pattern to match when `fixed_pattern` is set, as
	 * { on[0], off[1], on[1], off[2] }.
	 */
	int	pattern[4];				/* +0x2b0 */

	int	f2c0, f2c4, f2c8, f2cc;			/* +0x2c0 */

	/* When set, match `pattern` exactly instead of the timing windows. */
	int	fixed_pattern;				/* +0x2d0 */

	/* Cycles required before `pattern` is even looked at. */
	int	pattern_min_cycles;			/* +0x2d4 */

	void	*modem;		/* for modem_get_param            +0x2d8 */
};

void cadence_delete(struct cadence *c);

/* Start over: reset the filter, drop every recorded period. */
void cadence_reset(struct cadence *c);

/*
 * Feed one sample.  Returns CADENCE_NOTHING almost always -- a verdict is
 * possible only on the samples where the underlying toneiir reaches the end
 * of an interval.
 */
int cadence_progress(struct cadence *c, short sample);

#endif /* DSPLIB_CADENCE_H */

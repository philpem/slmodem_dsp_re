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
 * nothing at all until that returns a verdict.  How often that is depends on
 * the tone: busy, congestion and ringback use 160 samples, which is 20 ms at
 * the fixed 8000 Hz this runs at, and dial tone uses
 * `GetCallProgressSamplesBufferLength` (666 by default, so 83.25 ms).  EVERY
 * duration below is a count of those intervals, never samples and never
 * milliseconds.
 *
 * The country table speaks in units of 10 ms and `cadence_create` does the
 * conversion; see docs/parameters.md.
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
	 * Timing windows, from the country table
	 * (GetMin/MaxBusyCadenceOnTime and friends), converted by
	 * cadence_create from the table's units of 10 ms into a count of
	 * toneiir intervals:
	 *
	 *     intervals = time * 80 / buflen
	 *
	 * where buflen is 160 samples for these three tones, so an interval is
	 * 20 ms and a 500 ms busy tone is 50 in the table and 25 here.  See
	 * docs/parameters.md for why the table's unit has to be 10 ms -- the
	 * derivation does not depend on buflen, which cancels.
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

	/*
	 * The toneiir envelope floor, from
	 * Get_Detection_Threshold_Table(GetDialToneDetectionThreshold).  A
	 * short in an int-sized slot: cadence_create writes only the low
	 * half.
	 */
	short	threshold;				/* +0x294 */
	short	pad296;

	/*
	 * Dial tone only: 100 times GetDialToneValidationTime, less 100 if
	 * that exceeds 100.  Zero for the other three.
	 */
	int	validation;				/* +0x298 */

	/*
	 * Samples between toneiir verdicts.  160 for busy, congestion and
	 * ringback; GetCallProgressSamplesBufferLength (or 666) for dial.
	 */
	int	buflen;					/* +0x29c */
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

/*
 * Which tone a detector is for.  The names are the object's own -- it carries
 * them as debug strings at .rodata+0x6208 and indexes that table with this
 * value, clamped to CADENCE_TONE_INVALID.
 */
#define CADENCE_TONE_BUSY	0
#define CADENCE_TONE_DIAL	1
#define CADENCE_TONE_CONG	2
#define CADENCE_TONE_RING	3
#define CADENCE_TONE_INVALID	4

/*
 * What the caller hands to cadence_create.  Seven words on its stack; only
 * three of them are read, and one is written back.
 */
struct cadence_setup {
	int	w0;		/* +0x00  read into nothing            */
	int	w1;		/* +0x04                               */
	int	w2;		/* +0x08                               */
	int	w3;		/* +0x0c  -> c->f27c                   */

	/*
	 * The tone.  cadence_create clamps this to CADENCE_TONE_INVALID and
	 * WRITES THE CLAMPED VALUE BACK here before using it to pick the
	 * debug name -- so a caller that reuses one setup across several
	 * creates sees its own field change under it.
	 *
	 * Note the clamp affects only the name.  Any value that is not 0, 2
	 * or 3 behaves as DIAL, so a tone of 7 is configured as dial tone and
	 * labelled "INVALID".
	 */
	int	tone;		/* +0x10 */

	int	w5;		/* +0x14                               */
	int	w6;		/* +0x18  -> c->f2a4                   */
};

/*
 * Build a detector.  Pass NULL for `c` to allocate one; returns NULL if the
 * allocation fails, or if the tone's timing windows are unusable (only the
 * ringback case can decide that, and it frees what it built first).
 *
 * `extra` does two things: any positive value clears `continuous`, and
 * `extra + 1` scales the tone duration handed to the underlying toneiir.
 */
struct cadence *cadence_create(struct cadence *c, struct cadence_setup *s,
			       int extra, void *modem);

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

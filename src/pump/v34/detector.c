/*
 * detector.c -- ITU-T V.34: the tone presence/absence detector.
 *
 * Two functions and a 0x24-byte object.  The handshake creates one of these
 * whenever it needs to wait for a particular tone, points it at a pair of
 * biquad sections chosen for that tone's frequency, and then asks once per
 * received block whether the tone is there yet.
 *
 * The object is a member of the enclosing V.34 object -- every call site
 * forms its argument with `lea 0x3564(%reg)` -- but it is passed by pointer
 * and touches nothing outside itself except one flag word, so it is declared
 * standalone here and the parent will embed it when V34hshak.c arrives.
 *
 * THE SIGNAL PATH is two second-order sections in direct form I, followed by
 * a leaky integrator of the rectified output.  What makes it worth reading
 * closely is not the filter but the scaling, which is asymmetric in a way
 * that no filter design would ask for and that has to be reproduced exactly:
 *
 *   - the input enters section 1 already divided by 16;
 *   - section 1's output is stored to its own history at full width, but
 *     what reaches section 2 is `(short)(acc >> 4)` -- shifted while still
 *     32 bits wide, then truncated;
 *   - section 2's output is stored at full width too, but what reaches the
 *     integrator is `(short)acc >> 4` -- truncated FIRST, then shifted.
 *
 * Those last two differ whenever the accumulator leaves 16 bits, which is
 * exactly when the detector is being driven hard.  There is no reading under
 * which both are intended, so one of them is a slip; docs/deviations.md
 * records it as reproduced rather than repaired, because which one was meant
 * cannot be recovered and either choice would change the detector's
 * behaviour at the levels it actually has to discriminate.
 *
 * Every product is also truncated to 16 bits after its Q14 shift, before it
 * joins the accumulator.  That is ordinary for this codebase and unremarkable
 * except that it means the sections are not linear.
 *
 * THE DECISION is a counter with two polarities.  See tone_detect.
 *
 * No tier-2 peer exists for any of this -- see the note in v34det.h.
 */

#include "dsplib/v34det.h"

/*
 * One second-order section, accumulating into `acc`.
 *
 * `b` and `a` are two entries each and are NOT adjacent in the coefficient
 * block: the eight shorts are grouped by role, so section 2's feed-forward
 * pair sits between section 1's two pairs.  See v34det.h.
 *
 * The original spells this out twice rather than calling anything, and
 * interleaves the two terms within each tap.  Both orders sum the same
 * values, because every term is truncated to 16 bits before it is added and
 * the additions themselves are plain 32-bit ones.
 */
static int
det_section(int acc, const short *b, const short *a, const short *x,
	    const short *y)
{
	int k;

	for (k = 0; k < 2; k++) {
		acc += (short)((x[k] * b[k]) >> 14);
		acc -= (short)((y[k] * a[k]) >> 14);
	}
	return acc;
}

void
detectorinit(struct v34_detector *d, const short *coeff, short polarity,
	     short limit, short warmup, short thresh_lo, short thresh_hi)
{
	int section, tap;

	/* The original's nested pair, which is where the 2-D history came from. */
	for (section = 0; section < 2; section++)
		for (tap = 0; tap < 2; tap++) {
			d->x[section][tap] = 0;
			d->y[section][tap] = 0;
		}

	d->coeff = coeff;
	d->polarity = polarity;
	d->armed = 0;
	d->count = (short)-warmup;
	d->limit = limit;
	d->state = V34_DET_STATE_WARMUP;
	d->thresh_hi = thresh_hi;
	d->thresh_lo = thresh_lo;
	d->level = 0;
}

int
tone_detect(struct v34_rx *rx, struct v34_detector *d, const short *start,
	    const short *end)
{
	const short *p;

	for (p = start; p < end; p++) {
		int x = *p;
		int acc;
		short old;
		short mid;

		/* --- section 1, input pre-scaled by 1/16 --- */
		acc = det_section(x >> 4, &d->coeff[0], &d->coeff[4],
				  d->x[0], d->y[0]);

		/*
		 * Shift the 32-bit accumulator, THEN truncate.  Section 2
		 * does the opposite; see the file comment.
		 */
		mid = (short)(acc >> 4);

		old = d->y[0][0];
		d->y[0][0] = (short)acc;
		d->y[0][1] = old;
		old = d->x[0][0];
		d->x[0][0] = (short)(x >> 4);
		d->x[0][1] = old;

		/* --- section 2 --- */
		acc = det_section(mid, &d->coeff[2], &d->coeff[6],
				  d->x[1], d->y[1]);

		old = d->x[1][0];
		d->x[1][0] = mid;
		d->x[1][1] = old;
		old = d->y[1][0];
		d->y[1][0] = (short)acc;
		d->y[1][1] = old;

		/* --- rectify and integrate.  Truncate, THEN shift. --- */
		{
			int out = (short)acc >> 4;

			if (out < 0)
				out = -out;
			d->level = (short)(out
					   + ((d->level * V34_DET_DECAY) >> 14));
		}
	}

	/*
	 * Warm-up.  `count` was seeded negative and is stepped once per call,
	 * so this holds the detector off for a fixed number of CALLS -- the
	 * block length never enters into it.  The step is a 16-bit one: the
	 * original increments the zero-extended field and tests the low word,
	 * so a count that wrapped would escape the warm-up rather than stick.
	 */
	if (d->state == V34_DET_STATE_WARMUP) {
		d->count = (short)(d->count + 1);
		if (d->count < 0)
			return 0;
		d->state = V34_DET_STATE_RUNNING;
	}

	if (d->polarity != 0) {
		/*
		 * Absence.  Count blocks whose level is below `thresh_hi`,
		 * and reset on any block above `thresh_lo`.
		 *
		 * Both tests are made, in that order, on every call -- they
		 * are not an if/else in the original.  With thresh_lo below
		 * thresh_hi a level between the two therefore increments and
		 * then immediately resets, so the band behaves as though only
		 * the reset applied.  Written the same way here.
		 */
		if (d->level < d->thresh_hi)
			d->count = (short)(d->count + 1);
		if (d->level > d->thresh_lo)
			d->count = 0;
	} else if (d->armed) {
		/* Presence.  Count blocks above `thresh_lo`, reset below. */
		if (d->level > d->thresh_lo)
			d->count = (short)(d->count + 1);
		else
			d->count = 0;
	} else {
		/*
		 * Presence, before anything has been heard.  A presence
		 * detector will not start counting until the level has once
		 * crossed a fixed floor, whatever `thresh_lo` says; arming
		 * also tells the caller, by clearing its pending flag, and
		 * restarts the count from zero.
		 */
		if (d->level <= V34_DET_ARM_LEVEL)
			return 0;

		d->armed = 1;
		rx->flags = (unsigned short)(rx->flags
					     & ~V34_RX_FLAG_DET_PENDING);
		d->count = 0;
		return 0;
	}

	return d->count > d->limit;
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.
 *
 * Read off detectorinit's stores and tone_detect's indexed loads.  Guarded to
 * a 32-bit ABI because `coeff` is a pointer; see the same note in b103fp.c.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34DET_ASSERT_OFF(field, off) \
	typedef char v34det_off_##field[ \
		((int)__builtin_offsetof(struct v34_detector, field) == (off)) \
		? 1 : -1]

V34DET_ASSERT_OFF(coeff, 0x00);
V34DET_ASSERT_OFF(polarity, 0x04);
V34DET_ASSERT_OFF(armed, 0x06);
V34DET_ASSERT_OFF(count, 0x08);
V34DET_ASSERT_OFF(limit, 0x0a);
V34DET_ASSERT_OFF(state, 0x0c);
V34DET_ASSERT_OFF(thresh_hi, 0x0e);
V34DET_ASSERT_OFF(thresh_lo, 0x10);
V34DET_ASSERT_OFF(level, 0x12);
V34DET_ASSERT_OFF(x, 0x14);
V34DET_ASSERT_OFF(y, 0x1c);

typedef char v34det_size[(sizeof(struct v34_detector) == 0x24) ? 1 : -1];

/*
 * And the one field of the caller's object this module reaches into.  The
 * struct is a stub, so this assertion is the only thing keeping the offset
 * honest until V34hshak.c fills the rest in.
 */
typedef char v34det_rx_flags[
	((int)__builtin_offsetof(struct v34_rx, flags) == 0x122) ? 1 : -1];

#endif

/*
 * t_v34timing.c -- differential test of the two timing accessors,
 * `getTimingOffset` (0x71b0) and `getTimingPhase` (0x71c0).
 *
 * Eleven bytes each: one load at +0x49c / +0x4a0 of the argument.  No caller
 * exists anywhere in the object, so there is no behaviour beyond the load --
 * the test plants distinct patterns over the whole word (sign bit included,
 * because a `mov` that became a `movswl` through a mistyped field would pass
 * any small-positive probe) and asks both sides.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34pcmif.h"

extern int ref_getTimingOffset(void *objp);
extern int ref_getTimingPhase(void *objp);

static const int patterns[] = {
	0, 1, -1, 0x7fffffff, (int)0x80000000, 0x12345678,
	(int)0xdeadbeef, 0x49c, 0x4a0, -32768, 32767, (int)0xffff8000
};

int
main(void)
{
	static struct v34_object obj;	/* static: it is 43K+ */
	unsigned i;

	diff_begin("v34 timing accessors");

	memset(&obj, 0x5a, sizeof(obj));
	for (i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
		obj.timing_offset = patterns[i];
		obj.timing_phase = ~patterns[i];

		diff_eq_int("getTimingOffset(pattern %ld)",
			    getTimingOffset(&obj), ref_getTimingOffset(&obj),
			    i);
		diff_eq_int("getTimingPhase(pattern %ld)",
			    getTimingPhase(&obj), ref_getTimingPhase(&obj),
			    i);
		diff_eq_int("getTimingOffset reads +0x49c exactly (%ld)",
			    getTimingOffset(&obj), patterns[i], i);
		diff_eq_int("getTimingPhase reads +0x4a0 exactly (%ld)",
			    getTimingPhase(&obj), ~patterns[i], i);
	}

	return diff_end();
}

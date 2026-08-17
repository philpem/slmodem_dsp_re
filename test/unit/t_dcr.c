/*
 * t_dcr.c -- differential test of the DC remover against the blob.
 *
 * Four entry points, and every one of them is compared: the allocation and
 * its initial 32 bytes, the three-store reset, the free, and every arc
 * through `dcr_process`.
 *
 * WHAT COUNTS AS AN OBSERVABLE RESULT HERE.  Four things, and each check
 * below names which it is looking at:
 *
 *   - the return value, which is the "there is more DC on this line than we
 *     are happy about" verdict;
 *   - the caller's buffer, which `dcr_process` corrects in place;
 *   - all 32 bytes of the object, through `diff_eq_obj` -- the phase, the
 *     estimate and both accumulators at once, so a mutation that gets the
 *     right answer by the wrong route still fails;
 *   - the diagnostic transcript, swept over debug levels 0..3 because there
 *     is exactly one gated site in the file and a run at level 2 alone
 *     cannot separate `> 1` from `> 0` (debug.h, finding 150).
 *
 * HOW THE PHASES ARE REACHED.  `dcr_create` sets the three interval fields to
 * 5760, 9600 and 19200 samples, so walking the state machine honestly would
 * cost 35 KB of audio per scenario.  The intervals are FIELDS, and they are
 * read by exactly one phase each and written by nobody but `dcr_create`, so
 * every scenario pokes all three small -- identically through `struct dcr *`
 * on both sides, which is itself a check on the layout claim, because a
 * wrong offset would poke a different field on one of them and the object
 * comparison would say so on the very first block.
 *
 * WHAT NO TEST HERE CAN HOLD.  `dcr.c` reads `dcr->flags` at each use site
 * rather than caching it in a local, on the evidence of the reload after the
 * printf (0x2f6).  Caching it changes nothing observable -- the only call in
 * the function is `dsplibs_debug_printf`, which does not write the struct --
 * so this is a codegen-tier claim held by `compare.py` and not by anything
 * here.  It is registered as a surviving mutation in `test/mutations/dcr.json`
 * with that reason rather than dressed up as one this file kills.
 */

#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "dsplib/dcr.h"
#include "dsplib/debug.h"

extern void *ref_dcr_create(void);
extern void ref_dcr_delete(void *dcr);
extern void ref_dcr_reset(void *dcr);
extern int ref_dcr_process(void *dcr, short *buf, int len);
extern unsigned int ref_dsplibs_debug_level;

#define MAXBLK 512

struct pair {
	struct dcr *ours;
	struct dcr *ref;
};

static void
pair_new(struct pair *p, long tag)
{
	p->ours = dcr_create();
	p->ref = (struct dcr *)ref_dcr_create();
	diff_eq_int("create returned an object (%ld)",
		    p->ours != 0 && p->ref != 0, 1, tag);
	diff_eq_obj("fresh object", struct dcr, p->ours, p->ref, tag);
}

static void
pair_free(struct pair *p)
{
	dcr_delete(p->ours);
	ref_dcr_delete(p->ref);
	p->ours = p->ref = 0;
}

static void
pair_intervals(struct pair *p, int settle, int evaluate, int track)
{
	p->ours->settle_samples = p->ref->settle_samples = settle;
	p->ours->evaluate_samples = p->ref->evaluate_samples = evaluate;
	p->ours->track_samples = p->ref->track_samples = track;
}

/*
 * One block through both sides, on private copies of the same samples, with
 * all four observables compared.  Returns our verdict so a caller can assert
 * on it directly as well.
 */
static int
pair_block(struct pair *p, const short *src, int len, long tag)
{
	short a[MAXBLK], b[MAXBLK];
	int ra, rb;

	memcpy(a, src, (size_t)len * sizeof a[0]);
	memcpy(b, src, (size_t)len * sizeof b[0]);

	rb = ref_dcr_process(p->ref, b, len);
	ra = dcr_process(p->ours, a, len);

	diff_eq_int("verdict (%ld)", ra, rb, tag);
	diff_eq_int("corrected buffer (%ld)",
		    memcmp(a, b, (size_t)len * sizeof a[0]) == 0, 1, tag);
	diff_eq_obj("object after the block", struct dcr, p->ours, p->ref, tag);
	return ra;
}

static void
fill(short *buf, int len, int value)
{
	int i;

	for (i = 0; i < len; i++)
		buf[i] = (short)value;
}

/* ------------------------------------------------------------------ */

static int
t_lifecycle(void)
{
	struct pair p;
	short buf[MAXBLK];

	diff_begin("dcr: create, delete, reset and the two refusals");

	pair_new(&p, 0);
	/*
	 * The initial 32 bytes are already compared by pair_new; these say
	 * what they are, so a change to dcr_create that both sides happened
	 * to share would still be visible in the record.
	 */
	diff_eq_int("flags start at 7", p.ref->flags, DCR_ACTIVE | DCR_SUBTRACT
					| DCR_TRACK, 0);
	diff_eq_int("phase starts at SETTLE", (int)p.ref->state,
		    DCR_STATE_SETTLE, 0);
	diff_eq_int("threshold is 3000", p.ref->threshold, 3000, 0);
	diff_eq_int("settle is 5760", p.ref->settle_samples, 5760, 0);
	diff_eq_int("evaluate is 9600", p.ref->evaluate_samples, 9600, 0);
	diff_eq_int("track is 19200", p.ref->track_samples, 19200, 0);

	/* reset clears three fields and leaves the phase and the flags. */
	p.ours->dc_level = p.ref->dc_level = -1234;
	p.ours->sum = p.ref->sum = 987654;
	p.ours->count = p.ref->count = 4321;
	p.ours->state = p.ref->state = DCR_STATE_TRACK;
	p.ours->flags = p.ref->flags = DCR_ACTIVE;
	ref_dcr_reset(p.ref);
	dcr_reset(p.ours);
	diff_eq_obj("after reset", struct dcr, p.ours, p.ref, 0);
	diff_eq_int("reset left the phase alone", (int)p.ref->state,
		    DCR_STATE_TRACK, 0);
	diff_eq_int("reset left the flags alone", p.ref->flags, DCR_ACTIVE, 0);
	pair_free(&p);

	/*
	 * Both deletes accept NULL, and the guard is the observable thing:
	 * `sysdep_free` counts the null frees that reach it, so a delete that
	 * dropped the `if (dcr)` would be visible here even though free(NULL)
	 * is otherwise harmless.
	 */
	{
		int nulls = harness_alloc.free_null;

		ref_dcr_delete(0);
		dcr_delete(0);
		diff_eq_int("delete(NULL) reaches no free",
			    harness_alloc.free_null, nulls, 0);
	}

	/* A NULL object is refused before anything is read. */
	fill(buf, 64, 100);
	diff_eq_int("process(NULL) is 0", dcr_process(0, buf, 64),
		    ref_dcr_process(0, buf, 64), 0);

	/* DCR_ACTIVE clear: nothing read, nothing written, verdict 0. */
	pair_new(&p, 1);
	p.ours->flags = p.ref->flags = 0;
	p.ours->dc_level = p.ref->dc_level = 20000;	/* over threshold */
	fill(buf, 64, 100);
	diff_eq_int("inactive verdict is 0", pair_block(&p, buf, 64, 1), 0, 1);
	diff_eq_int("inactive left the counters alone", p.ref->count, 0, 1);
	pair_free(&p);

	/* DCR_SUBTRACT clear: measured, judged, but the block is untouched. */
	pair_new(&p, 2);
	p.ours->flags = p.ref->flags = DCR_ACTIVE;
	p.ours->state = p.ref->state = DCR_STATE_HOLD;
	p.ours->dc_level = p.ref->dc_level = 500;
	fill(buf, 64, 1000);
	pair_block(&p, buf, 64, 2);
	pair_free(&p);

	return diff_end();
}

/*
 * The whole state machine, walked with intervals small enough to reach every
 * transition, both ways round on DCR_TRACK.
 */
static int
t_phases(void)
{
	struct pair p;
	short buf[MAXBLK];
	int track, blk, i;

	diff_begin("dcr: the four phases, and the DCR_TRACK fork");

	for (track = 0; track < 2; track++) {
		for (blk = 1; blk <= 16; blk *= 4) {
			long tag = (long)track * 100 + blk;

			pair_new(&p, tag);
			if (!track)
				p.ours->flags = p.ref->flags =
				    DCR_ACTIVE | DCR_SUBTRACT;
			pair_intervals(&p, 8, 16, 24);

			/*
			 * A constant offset, so the mean is exactly it and a
			 * wrong divisor or a wrong accumulator shows up in
			 * dc_level rather than being averaged away.
			 */
			fill(buf, blk, 700);
			for (i = 0; i < 64; i++)
				pair_block(&p, buf, blk, tag * 1000 + i);

			diff_eq_int("phase reached (%ld)", (int)p.ref->state,
				    track ? DCR_STATE_TRACK : DCR_STATE_HOLD,
				    tag);
			pair_free(&p);
		}
	}

	/*
	 * SETTLE does not accumulate: park the object in phase 0 with a
	 * poisoned `sum` and check the poison survives until the phase ends.
	 */
	pair_new(&p, 900);
	pair_intervals(&p, 40, 16, 24);
	p.ours->sum = p.ref->sum = 0x5a5a5a;
	fill(buf, 8, -3000);
	for (i = 0; i < 4; i++)
		pair_block(&p, buf, 8, 900 + i);
	diff_eq_int("SETTLE left sum untouched", p.ref->sum, 0x5a5a5a, 900);
	pair_free(&p);

	return diff_end();
}

/*
 * The arithmetic: the plain mean in phase 1, the 29491/3277 blend in phase 2,
 * and both with negative inputs -- `idiv` truncates towards zero, so a
 * negative offset is a different code path in every sense that matters.
 */
static int
t_arithmetic(void)
{
	struct pair p;
	short buf[MAXBLK];
	static const int level[] = { 0, 1, -1, 7, -7, 300, -300, 3000, -3000,
				     12345, -12345, 32767, -32768 };
	static const int start[] = { 0, 1, -1, 5000, -5000, 32767, -32768 };
	unsigned li, si;

	diff_begin("dcr: the mean, the blend, and the signs");

	for (li = 0; li < sizeof level / sizeof level[0]; li++) {
		long tag = level[li];

		/* Phase 1: the plain mean of a constant block. */
		pair_new(&p, tag);
		pair_intervals(&p, 1, 32, 64);
		p.ours->state = p.ref->state = DCR_STATE_EVALUATE;
		fill(buf, 33, level[li]);
		pair_block(&p, buf, 33, tag);
		pair_free(&p);

		/* Phase 2's blend, from every starting estimate. */
		for (si = 0; si < sizeof start / sizeof start[0]; si++) {
			long t2 = tag * 100000L + start[si];

			pair_new(&p, t2);
			pair_intervals(&p, 1, 8, 32);
			p.ours->state = p.ref->state = DCR_STATE_TRACK;
			p.ours->dc_level = p.ref->dc_level = (short)start[si];
			/*
			 * DCR_SUBTRACT off: the correction would otherwise
			 * change the samples the NEXT block averages, which
			 * is realistic but makes the blend hard to read.
			 */
			p.ours->flags = p.ref->flags = DCR_ACTIVE | DCR_TRACK;
			fill(buf, 33, level[li]);
			pair_block(&p, buf, 33, t2);
			pair_free(&p);
		}
	}

	/*
	 * A block whose sum does not divide exactly, both signs, so the
	 * truncation direction is observable rather than incidental.
	 */
	pair_new(&p, 7777);
	pair_intervals(&p, 1, 7, 64);
	p.ours->state = p.ref->state = DCR_STATE_EVALUATE;
	buf[0] = 10; buf[1] = 10; buf[2] = 10; buf[3] = 11;
	buf[4] = -3; buf[5] = -3; buf[6] = -3;
	pair_block(&p, buf, 7, 7777);
	pair_free(&p);

	pair_new(&p, 7778);
	pair_intervals(&p, 1, 7, 64);
	p.ours->state = p.ref->state = DCR_STATE_EVALUATE;
	buf[0] = -10; buf[1] = -10; buf[2] = -10; buf[3] = -11;
	buf[4] = 3; buf[5] = 3; buf[6] = 3;
	pair_block(&p, buf, 7, 7778);
	pair_free(&p);

	return diff_end();
}

/*
 * The verdict, which is `|dc_level| >= threshold` and nothing else -- taken
 * in HOLD, where no other field can move and the comparison is the only
 * thing under test.
 */
static int
t_verdict(void)
{
	struct pair p;
	short buf[MAXBLK];
	static const int dc[] = { 0, 1, -1, 2999, 3000, 3001, -2999, -3000,
				  -3001, 32767, -32768 };
	static const int th[] = { 0, 1, 3000, 32767, -1, -32768 };
	unsigned di, ti;

	diff_begin("dcr: |dc_level| against the threshold");

	for (di = 0; di < sizeof dc / sizeof dc[0]; di++)
		for (ti = 0; ti < sizeof th / sizeof th[0]; ti++) {
			long tag = (long)dc[di] * 100000L + th[ti];

			pair_new(&p, tag);
			pair_intervals(&p, 1, 1, 1 << 30);
			p.ours->state = p.ref->state = DCR_STATE_HOLD;
			p.ours->dc_level = p.ref->dc_level = (short)dc[di];
			p.ours->threshold = p.ref->threshold = (short)th[ti];
			/*
			 * A non-zero block: HOLD discards an all-zero one
			 * before the verdict is ever computed, which is the
			 * next test's subject.
			 */
			fill(buf, 4, 1);
			pair_block(&p, buf, 4, tag);
			pair_free(&p);
		}

	/*
	 * -32768 is the case an `abs` that negates in place gets wrong, and
	 * it is reachable: the blend can land there from a saturated input.
	 */
	pair_new(&p, 32768);
	pair_intervals(&p, 1, 1, 1 << 30);
	p.ours->state = p.ref->state = DCR_STATE_HOLD;
	p.ours->dc_level = p.ref->dc_level = -32768;
	p.ours->threshold = p.ref->threshold = 32767;
	fill(buf, 4, 1);
	diff_eq_int("|-32768| clears any threshold", pair_block(&p, buf, 4,
		    32768), 1, 32768);
	pair_free(&p);

	return diff_end();
}

/*
 * The silence gate, which only phases 2 and 3 take, and the zero-length
 * block, which reaches its `i == len` with i == 0 and returns through it.
 */
static int
t_silence(void)
{
	struct pair p;
	short buf[MAXBLK];
	int st;

	diff_begin("dcr: all-zero blocks, and zero-length ones");

	for (st = DCR_STATE_SETTLE; st <= DCR_STATE_HOLD; st++) {
		pair_new(&p, st);
		pair_intervals(&p, 4, 4, 4);
		p.ours->state = p.ref->state = (unsigned)st;
		p.ours->dc_level = p.ref->dc_level = 9000;
		p.ours->count = p.ref->count = 3;
		p.ours->sum = p.ref->sum = 111;

		fill(buf, 16, 0);
		pair_block(&p, buf, 16, st);
		/*
		 * The gate is an EARLY RETURN, so the object must be exactly
		 * as it was left: `count` was primed to 3 above and the two
		 * measuring phases both consume it.  Asserting on the verdict
		 * instead would not separate the two, because phase 1 ends
		 * here and recomputes `dc_level` down under the threshold --
		 * which is a verdict of 0 reached the long way round.
		 */
		diff_eq_int("silence gate taken iff TRACK or HOLD (%ld)",
			    p.ref->count == 3,
			    (st == DCR_STATE_TRACK || st == DCR_STATE_HOLD),
			    st);
		pair_free(&p);

		/* Zero length, every phase. */
		pair_new(&p, 100 + st);
		pair_intervals(&p, 4, 4, 4);
		p.ours->state = p.ref->state = (unsigned)st;
		p.ours->dc_level = p.ref->dc_level = 9000;
		pair_block(&p, buf, 0, 100 + st);
		pair_free(&p);

		/*
		 * One non-zero sample at the end of an otherwise silent
		 * block: the gate must scan the whole buffer, not sample 0.
		 */
		pair_new(&p, 200 + st);
		pair_intervals(&p, 4, 4, 4);
		p.ours->state = p.ref->state = (unsigned)st;
		p.ours->dc_level = p.ref->dc_level = 9000;
		fill(buf, 16, 0);
		buf[15] = 1;
		pair_block(&p, buf, 16, 200 + st);
		pair_free(&p);

		/* And one at the start, which the loop must stop on. */
		pair_new(&p, 300 + st);
		pair_intervals(&p, 4, 4, 4);
		p.ours->state = p.ref->state = (unsigned)st;
		p.ours->dc_level = p.ref->dc_level = 9000;
		fill(buf, 16, 0);
		buf[0] = -1;
		pair_block(&p, buf, 16, 300 + st);
		pair_free(&p);
	}

	return diff_end();
}

/*
 * The subtraction: no clamp anywhere in the object, so a sample near the rail
 * with an offset of the opposite sign wraps, and that is the behaviour to
 * reproduce rather than to improve on.
 */
static int
t_subtract(void)
{
	struct pair p;
	short buf[MAXBLK];
	static const int dc[] = { 1, -1, 1000, -1000, 32767, -32768 };
	unsigned di;
	int i;

	diff_begin("dcr: the in-place correction wraps rather than clamps");

	for (di = 0; di < sizeof dc / sizeof dc[0]; di++) {
		long tag = dc[di];

		pair_new(&p, tag);
		pair_intervals(&p, 1, 1, 1 << 30);
		p.ours->state = p.ref->state = DCR_STATE_HOLD;
		p.ours->dc_level = p.ref->dc_level = (short)dc[di];

		for (i = 0; i < 16; i++)
			buf[i] = (short)(32767 - i);
		for (i = 0; i < 16; i++)
			buf[16 + i] = (short)(-32768 + i);
		buf[32] = 0; buf[33] = 1; buf[34] = -1;
		pair_block(&p, buf, 35, tag);
		pair_free(&p);
	}

	return diff_end();
}

/*
 * The transcript.  One gated site, at `> 1`, reached only when phase 1 ends;
 * swept over 0..3 because a run at 2 alone cannot tell `> 1` from `> 0`, and
 * checked for the right AMOUNT of output as well as the right text -- a
 * transcript comparison between two silent sides passes vacuously.
 */
static int
t_transcript(void)
{
	struct pair p;
	short buf[MAXBLK];
	unsigned lvl;
	int track;

	diff_begin("dcr: the initial-evaluation diagnostic, levels 0..3");

	for (track = 0; track < 2; track++)
		for (lvl = 0; lvl <= 3; lvl++) {
			long tag = (long)track * 10 + lvl;

			pair_new(&p, tag);
			pair_intervals(&p, 1, 16, 1 << 30);
			p.ours->state = p.ref->state = DCR_STATE_EVALUATE;
			if (!track)
				p.ours->flags = p.ref->flags =
				    DCR_ACTIVE | DCR_SUBTRACT;
			fill(buf, 16, track ? -777 : 1234);

			dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();
			pair_block(&p, buf, 16, tag);
			dsplib_debug_capture_on = 0;
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;

			diff_eq_int("transcripts agree, level %ld",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			/*
			 * The anti-vacuity check: the blob must have printed
			 * exactly when the gate says it should, so a silent
			 * pair cannot pass the line above by saying nothing.
			 */
			diff_eq_int("blob printed iff level > 1 (%ld)",
				    dsplib_debug_capture_lines(1) == 1,
				    lvl > 1, tag);
			diff_eq_int("we printed as often as the blob (%ld)",
				    dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1), tag);
			pair_free(&p);
		}

	/*
	 * THE PHASE-2 RE-ESTIMATION IS SILENT, and that needs saying with a
	 * check rather than by omission: the sentence begins "initial", it
	 * fires once per object, and a copy of it in the tracking arm would
	 * be invisible to every test above -- all of which either leave the
	 * object in HOLD or never raise the level while it is in TRACK.
	 */
	for (lvl = 2; lvl <= 3; lvl++) {
		pair_new(&p, 500 + lvl);
		pair_intervals(&p, 1, 1, 8);
		p.ours->state = p.ref->state = DCR_STATE_TRACK;
		p.ours->dc_level = p.ref->dc_level = 1000;
		fill(buf, 16, 2000);

		dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		pair_block(&p, buf, 16, 500 + lvl);
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;

		diff_eq_int("the blob's TRACK pass prints nothing (%ld)",
			    dsplib_debug_capture_lines(1), 0, (long)lvl);
		diff_eq_int("and neither do we (%ld)",
			    dsplib_debug_capture_lines(0), 0, (long)lvl);
		diff_eq_int("the estimate did move (%ld)",
			    p.ref->dc_level != 1000, 1, (long)lvl);
		pair_free(&p);
	}

	/*
	 * And the text itself at level 2, so the format string and its two
	 * arguments are pinned by content and not only by agreement.
	 */
	pair_new(&p, 999);
	pair_intervals(&p, 1, 10, 1 << 30);
	p.ours->state = p.ref->state = DCR_STATE_EVALUATE;
	fill(buf, 10, 4242);
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;
	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();
	pair_block(&p, buf, 10, 999);
	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	diff_eq_int("the sentence is the object's own",
		    strcmp(dsplib_debug_capture_text(1),
			   "DCR: initial DC Evaluation done, "
			   "DC level 4242, enabled\n") == 0, 1, 999);
	pair_free(&p);

	pair_new(&p, 998);
	pair_intervals(&p, 1, 10, 1 << 30);
	p.ours->state = p.ref->state = DCR_STATE_EVALUATE;
	p.ours->flags = p.ref->flags = DCR_ACTIVE | DCR_SUBTRACT;
	fill(buf, 10, -4242);
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;
	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();
	pair_block(&p, buf, 10, 998);
	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	diff_eq_int("DCR_TRACK clear prints \"disabled\"",
		    strcmp(dsplib_debug_capture_text(1),
			   "DCR: initial DC Evaluation done, "
			   "DC level -4242, disabled\n") == 0, 1, 998);
	pair_free(&p);

	return diff_end();
}

/*
 * A long run at the created intervals, which is the only thing here that
 * exercises the module as `modem_process` actually drives it: 160-sample
 * blocks, a DC offset that drifts, and the whole 0 -> 1 -> 2 -> 2 -> 2 walk
 * at 5760 / 9600 / 19200.
 */
static int
t_longrun(void)
{
	struct pair p;
	short buf[MAXBLK];
	int b, i;

	diff_begin("dcr: 90 seconds of 160-sample blocks at the real intervals");

	pair_new(&p, 0);
	for (b = 0; b < 5400; b++) {
		int dc = 400 + (b / 600) * 250;

		for (i = 0; i < 160; i++) {
			int s = dc + ((b * 160 + i) % 37) * 91
				   - ((b * 160 + i) % 13) * 130;
			buf[i] = (short)s;
		}
		pair_block(&p, buf, 160, b);
	}
	diff_eq_int("ended in TRACK", (int)p.ref->state, DCR_STATE_TRACK, 0);
	diff_eq_int("the estimate moved off zero", p.ref->dc_level != 0, 1, 0);
	pair_free(&p);

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= t_lifecycle();
	bad |= t_phases();
	bad |= t_arithmetic();
	bad |= t_verdict();
	bad |= t_silence();
	bad |= t_subtract();
	bad |= t_transcript();
	bad |= t_longrun();
	return bad;
}

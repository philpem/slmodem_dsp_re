/*
 * t_v23tx.c -- differential test of the V.23 transmitter.
 *
 * The hard part of this object is not the modulation -- FPM_TONE does that,
 * and t_fpm_tone already proves it.  It is the bookkeeping around the
 * modulation: a bit rate that is not a whole number of samples, and a caller
 * that asks for samples while supplying bits, so nearly every call ends part
 * way through a bit and has to leave enough behind to finish it next time.
 *
 * That bookkeeping fails in ways a naive test cannot see.  Ask for a whole
 * number of bit periods every time and the carry-over path never runs.  Use
 * one fixed block size and the { 7, 7, 6 } period table never gets out of
 * phase with it.  Send all ones and a transmitter that ignored the bit stream
 * entirely would pass.  So the sweep below varies the block size against the
 * period cycle deliberately, and the counters at the bottom refuse to let the
 * test pass unless each path was actually reached.
 *
 * Two objects, not one: both sides mutate their own tone generator, and a
 * shared object would have each side comparing against the other's writes.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v23fp.h"

extern void *ref_v23FP_tx_create(void *tx, short mark, short space,
				 short period_len, const short *period,
				 int mute);
extern void ref_v23FP_tx_delete(void *tx);
extern void ref_v23FP_tx_progress(void *tx, short *out, int count,
				  const int *bits, int *consumed);

/* The two channels, with the tables and frequencies CreateV23Modem uses. */
static const short fw_period[3] = { 7, 7, 6 };		/* 1200 bps at 8 kHz */
static const short bw_period[3] = { 107, 107, 106 };	/*   75 bps at 8 kHz */

#define FW_MARK		1300
#define FW_SPACE	2100
#define BW_MARK		390
#define BW_SPACE	450

/* Coverage: every path this test claims to reach. */
static int saw_carry;		/* a call ended part way through a bit    */
static int saw_boundary;	/* a call ended exactly on a bit boundary */
static int saw_signal;		/* output that is not silence             */
static int saw_wrap;		/* the period index wrapped               */

/*
 * Zero crossings per thousand samples, for the all-ones and all-zeros runs.
 * A transmitter that generated a tone but ignored which bit it was sending
 * would pass every comparison in this file -- the reference would be wrong in
 * exactly the same way only if the bug were in FPM_TONE, which it is not,
 * because both sides call a DIFFERENT FPM_TONE.  These two numbers are what
 * proves the bit actually selects a frequency.
 */
static int rate_mark;
static int rate_space;

#define MAXOUT 512

/*
 * Compare two transmitter objects word by word, skipping the tone pointer.
 *
 * Everything else must match exactly, including `held` -- which create does
 * not initialise, so under the harness's fill both sides hold the same 0xa5
 * pattern and a difference means one of them wrote to it when the other did
 * not.  That is a stronger check than skipping the field would be.
 */
static void
compare_obj(const char *what, const struct v23tx *ours, const void *refp)
{
	const unsigned char *a = (const unsigned char *)ours;
	const unsigned char *b = (const unsigned char *)refp;
	unsigned i;

	for (i = 0; i < sizeof(struct v23tx); i++) {
		if (i >= __builtin_offsetof(struct v23tx, tone)
		    && i < __builtin_offsetof(struct v23tx, tone)
			   + sizeof(void *))
			continue;	/* two heap objects, two addresses */
		diff_eq_int(what, a[i], b[i], i);
	}
}

/*
 * One channel, one bit pattern, one block size, run to `calls` blocks.
 *
 * `block` is deliberately allowed to be anything: the point of the sweep is
 * that it does not divide the period cycle.
 */
static int
run(const char *what, short mark, short space, const short *period,
    const int *bits, int nbits, int block, int calls)
{
	int crossings = 0;
	int samples = 0;
	short last = 0;
	struct v23tx *ours;
	void *ref;
	short out_a[MAXOUT], out_b[MAXOUT];
	int at = 0;
	int call;

	ours = v23FP_tx_create(NULL, mark, space, 3, period, 0);
	ref = ref_v23FP_tx_create(NULL, mark, space, 3, period, 0);
	compare_obj("after create", ours, ref);

	for (call = 0; call < calls; call++) {
		int used_a = -1, used_b = -1;
		int prev_index = ours->period_index;
		int i;
		int nonzero = 0;

		/*
		 * Fill both buffers with a pattern first, so a transmitter
		 * that wrote fewer samples than it claimed is caught rather
		 * than comparing two identically stale buffers.
		 */
		memset(out_a, 0x5a, sizeof(out_a));
		memset(out_b, 0x5a, sizeof(out_b));

		v23FP_tx_progress(ours, out_a, block, bits + at, &used_a);
		ref_v23FP_tx_progress(ref, out_b, block, bits + at, &used_b);

		for (i = 0; i < block; i++) {
			diff_eq_int(what, out_a[i], out_b[i], i);
			if (out_a[i] != 0)
				nonzero++;
			if ((out_a[i] < 0) != (last < 0))
				crossings++;
			last = out_a[i];
			samples++;
		}
		/* And that neither wrote past the block it was given. */
		diff_eq_int("wrote past the end", out_a[block], 0x5a5a, 0);

		diff_eq_int("bits consumed", used_a, used_b, call);
		compare_obj("object after a block", ours, ref);

		if (nonzero > block / 2)
			saw_signal++;
		if (ours->resume)
			saw_carry++;
		else
			saw_boundary++;
		if (ours->period_index < prev_index)
			saw_wrap++;

		at += used_a;
		if (at + 64 > nbits)
			at = 0;		/* wrap the pattern, not the object */
	}

	v23FP_tx_delete(ours);
	ref_v23FP_tx_delete(ref);

	return samples ? crossings * 1000 / samples : 0;
}

int
main(void)
{
	static int alt[256], ones[256], zeros[256], mixed[256];
	int rc = 0;
	int i;

	for (i = 0; i < 256; i++) {
		alt[i] = i & 1;
		ones[i] = 1;
		zeros[i] = 0;
		/* A pattern with runs of both, so neither tone dominates. */
		mixed[i] = (i / 3) & 1;
	}

	diff_begin("v23FP_tx: the 1200 bps forward channel");
	/*
	 * Block sizes chosen against the period cycle, which is 20 samples per
	 * 3 bits.  20 lands on a boundary every time; 7 and 13 never do; 160
	 * is one 8 kHz frame, which is what the datapump actually asks for.
	 */
	run("forward, alternating, block 7[%ld]", FW_MARK, FW_SPACE, fw_period,
	    alt, 256, 7, 24);
	run("forward, alternating, block 13[%ld]", FW_MARK, FW_SPACE, fw_period,
	    alt, 256, 13, 24);
	run("forward, alternating, block 20[%ld]", FW_MARK, FW_SPACE, fw_period,
	    alt, 256, 20, 24);
	run("forward, mixed runs, block 160[%ld]", FW_MARK, FW_SPACE, fw_period,
	    mixed, 256, 160, 8);
	rate_mark = run("forward, all ones, block 33[%ld]", FW_MARK, FW_SPACE,
			fw_period, ones, 256, 33, 12);
	rate_space = run("forward, all zeros, block 33[%ld]", FW_MARK, FW_SPACE,
			 fw_period, zeros, 256, 33, 12);
	/* One sample at a time: every call ends mid-bit. */
	run("forward, one sample per call[%ld]", FW_MARK, FW_SPACE, fw_period,
	    alt, 256, 1, 40);

	rc |= diff_end();

	diff_begin("v23FP_tx: the 75 bps backward channel");
	/*
	 * 320 samples per 3 bits here, so a 160-sample block never lands on a
	 * boundary and the carry-over path runs on every call -- the opposite
	 * bias to the forward channel's, which is the reason to test both.
	 */
	run("backward, alternating, block 160[%ld]", BW_MARK, BW_SPACE,
	    bw_period, alt, 256, 160, 12);
	run("backward, alternating, block 107[%ld]", BW_MARK, BW_SPACE,
	    bw_period, alt, 256, 107, 12);
	run("backward, mixed runs, block 320[%ld]", BW_MARK, BW_SPACE,
	    bw_period, mixed, 256, 320, 6);

	rc |= diff_end();

	diff_begin("v23FP_tx: the one-shot mute");
	{
		struct v23tx *ours;
		void *ref;
		short a[64], b[64];
		int ua = -1, ub = -1;
		int call;

		ours = v23FP_tx_create(NULL, FW_MARK, FW_SPACE, 3, fw_period, 1);
		ref = ref_v23FP_tx_create(NULL, FW_MARK, FW_SPACE, 3, fw_period,
					  1);
		compare_obj("after create with mute set", ours, ref);

		/*
		 * Twice: the first call must be silent and clear the flag, and
		 * the second must be a real signal.  Testing only the first
		 * would pass on a transmitter that muted for ever.
		 */
		for (call = 0; call < 2; call++) {
			int i;
			int nonzero = 0;

			memset(a, 0x5a, sizeof(a));
			memset(b, 0x5a, sizeof(b));
			v23FP_tx_progress(ours, a, 64, alt, &ua);
			ref_v23FP_tx_progress(ref, b, 64, alt, &ub);

			for (i = 0; i < 64; i++) {
				diff_eq_int("mute output[%ld]", a[i], b[i], i);
				if (a[i] != 0)
					nonzero++;
			}
			diff_eq_int("mute consumed", ua, ub, call);
			compare_obj("object after the mute block", ours, ref);
			diff_eq_int("call %ld: silent iff muting",
				    nonzero == 0, call == 0, call);
		}

		v23FP_tx_delete(ours);
		ref_v23FP_tx_delete(ref);
	}

	rc |= diff_end();

	diff_begin("v23FP_tx: coverage");
	diff_eq_int("a call ended part way through a bit", saw_carry > 0, 1, 0);
	diff_eq_int("a call ended on a bit boundary", saw_boundary > 0, 1, 0);
	diff_eq_int("the period index wrapped", saw_wrap > 0, 1, 0);
	/*
	 * 1300 Hz and 2100 Hz at 8 kHz are 325 and 525 crossings per thousand
	 * samples.  Checked as a wide band rather than an exact figure: the
	 * point is that the two are far apart and in the right order, which a
	 * transmitter stuck on one tone could not manage.
	 */
	printf("  all ones %d crossings/1000, all zeros %d\n", rate_mark,
	       rate_space);
	diff_eq_int("a mark is near 1300 Hz", rate_mark > 250 && rate_mark < 400,
		    1, rate_mark);
	diff_eq_int("a space is near 2100 Hz",
		    rate_space > 450 && rate_space < 600, 1, rate_space);
	diff_eq_int("the bit selects the frequency", rate_space > rate_mark, 1,
		    0);
	diff_eq_int("the output was a signal, not silence", saw_signal > 0, 1,
		    0);

	rc |= diff_end();

	return rc;
}

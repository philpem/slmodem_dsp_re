/*
 * t_callingtone.c -- differential test of the calling-tone generator.
 *
 * Two jobs, in this order.
 *
 * First, prove the reconstruction is the blob: every sample and every field
 * of the state after every call, across block sizes that do and do not
 * straddle a cadence transition, and across the whole range of the level
 * argument.  This is what makes the second job trustworthy.
 *
 * Second, measure what the blob actually emits, and assert it.  Three things
 * are wrong with this module and all three are the original's; asserting them
 * is what stops a later edit from "fixing" one without anyone noticing that
 * the output changed.  Each measurement below is a claim about the ORIGINAL,
 * verified by the first job.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "harness.h"
#include "dsplib/callingtone.h"

extern void ref_ResetCallingTone(struct calling_tone *ct, char level);
extern void ref_GenerateCallingTone(struct calling_tone *ct, short *buf,
				    int count);

#define FS 8000.0

static void
compare_state(const struct calling_tone *b, const struct calling_tone *a,
	      long n)
{
	diff_eq_int("call %ld: phase", b->phase, a->phase, n);
	diff_eq_int("call %ld: on", b->on, a->on, n);
	diff_eq_int("call %ld: remaining", b->remaining, a->remaining, n);
	diff_eq_int("call %ld: amplitude", b->amplitude, a->amplitude, n);
}

/*
 * ---------------------------------------------------------------------------
 * 1. equivalence
 */
static int
run_reset(void)
{
	int level;

	diff_begin("ResetCallingTone");

	for (level = -128; level <= 127; level++) {
		struct calling_tone a, b;

		memset(&a, 0xA5, sizeof(a));
		memset(&b, 0xA5, sizeof(b));
		ref_ResetCallingTone(&a, (char)level);
		ResetCallingTone(&b, (char)level);
		compare_state(&b, &a, level);
	}

	return diff_end();
}

static int
run_generate(const char *label, int level, int block, int calls)
{
	struct calling_tone a, b;
	short ba[512], bb[512];
	int n, i;
	int transitions = 0;
	int last_on;

	diff_begin(label);

	ref_ResetCallingTone(&a, (char)level);
	ResetCallingTone(&b, (char)level);
	last_on = a.on;

	for (n = 0; n < calls; n++) {
		memset(ba, 0x5A, sizeof(ba));
		memset(bb, 0x5A, sizeof(bb));

		ref_GenerateCallingTone(&a, ba, block);
		GenerateCallingTone(&b, bb, block);

		for (i = 0; i < block; i++)
			diff_eq_int("call %ld: sample", bb[i], ba[i], n);
		/*
		 * Past `block` neither should have written anything, so the
		 * fill pattern must survive.  Catches an off-by-one that the
		 * sample comparison alone would not.
		 */
		for (i = block; i < block + 4 && i < 512; i++)
			diff_eq_int("call %ld: past the end", bb[i],
				    (short)0x5A5A, n);

		compare_state(&b, &a, n);
		if (a.on != last_on) {
			transitions++;
			last_on = a.on;
		}
	}

	/*
	 * Anti-vacuity: a run that never leaves the first tone burst has not
	 * tested the cadence at all.  5760 on and 16800 off, so reaching a
	 * transition needs enough calls to cover 5760 samples.
	 */
	diff_eq_int("cadence transitions occurred (%ld)", transitions > 0, 1,
		    transitions);

	return diff_end();
}

/*
 * ---------------------------------------------------------------------------
 * 2. what it emits
 */

/*
 * D11: the phase-to-table shift is 6 where it should be 3, so only the first
 * eighth of the cosine is swept and the output never goes negative.  A
 * working generator would be symmetric about zero; this one has a mean of
 * about 0.9 of its peak, and the part that would survive a transformer is
 * under a tenth of what it spends its range on.
 *
 * Measured at level -12, which is the largest level that does NOT also trip
 * the amplitude overflow -- that is measured separately below, so that each
 * defect is asserted on its own.
 */
static int
run_waveform(void)
{
	struct calling_tone ct;
	short buf[4096];
	double mean = 0.0, ac = 0.0;
	int i, negative = 0;
	int lo = 32767, hi = -32768;
	char msg[128];

	diff_begin("calling tone: the waveform");

	ResetCallingTone(&ct, (char)-12);
	GenerateCallingTone(&ct, buf, 4096);

	for (i = 0; i < 4096; i++) {
		mean += buf[i];
		if (buf[i] < 0)
			negative++;
		if (buf[i] < lo)
			lo = buf[i];
		if (buf[i] > hi)
			hi = buf[i];
	}
	mean /= 4096.0;
	for (i = 0; i < 4096; i++)
		ac += (buf[i] - mean) * (buf[i] - mean);
	ac = sqrt(ac / 4096.0);

	snprintf(msg, sizeof(msg), "mean is %.0f, not near zero", mean);
	diff_eq_int(msg, mean > 20000.0, 1, 0);

	snprintf(msg, sizeof(msg), "negative samples: %d of 4096", negative);
	diff_eq_int(msg, negative == 0, 1, 0);

	snprintf(msg, sizeof(msg),
		 "range is %d..%d -- an eighth of a cosine", lo, hi);
	diff_eq_int(msg, lo > 20000 && hi > 30000, 1, 0);

	snprintf(msg, sizeof(msg),
		 "AC rms is %.0f against a mean of %.0f", ac, mean);
	diff_eq_int(msg, ac < 0.15 * mean, 1, 0);

	return diff_end();
}

/*
 * The amplitude overflow.  `(amplitude * TONE_read) >> 13` doubles, which
 * would be right if the level control produced a proper fraction -- but it
 * does not (D13), so the amplitude is always near 16384 and the product
 * reaches 32768, which wraps to -32768 in the 16-bit store.
 *
 * Level 0 is enough to trip it.  This is the one visible consequence of the
 * broken level control, and worth asserting separately from it.
 */
static int
run_overflow(void)
{
	struct calling_tone ct;
	short buf[4096];
	int i, wrapped = 0;
	char msg[128];

	diff_begin("calling tone: amplitude overflow");

	ResetCallingTone(&ct, 0);
	GenerateCallingTone(&ct, buf, 4096);

	for (i = 0; i < 4096; i++)
		if (buf[i] < 0)
			wrapped++;

	snprintf(msg, sizeof(msg),
		 "at level 0, %d of 4096 samples wrap to negative", wrapped);
	diff_eq_int(msg, wrapped > 0, 1, 0);

	/* And level -12 does not, which is what makes it an overflow. */
	ResetCallingTone(&ct, (char)-12);
	GenerateCallingTone(&ct, buf, 4096);
	wrapped = 0;
	for (i = 0; i < 4096; i++)
		if (buf[i] < 0)
			wrapped++;
	diff_eq_int("level -12 does not wrap", wrapped, 0, -12);

	return diff_end();
}

/*
 * The repetition rate is right even though the waveform is not: the phase
 * accumulator still wraps at the correct interval.  2219/16384 of 8000 is
 * 1083.7 Hz -- and of 9600, 1300.2, which is the V.25 calling tone exactly.
 * That is the evidence that the module was written for 9600.
 */
static int
run_repetition_rate(void)
{
	struct calling_tone ct;
	short buf[4096];
	int i, crossings = 0;
	double mean = 0.0, rate;
	char msg[128];

	diff_begin("calling tone: repetition rate");

	ResetCallingTone(&ct, 0);
	GenerateCallingTone(&ct, buf, 4096);

	for (i = 0; i < 4096; i++)
		mean += buf[i];
	mean /= 4096.0;

	/* Rising crossings of the mean, one per cycle of a sawtooth. */
	for (i = 1; i < 4096; i++)
		if (buf[i - 1] < mean && buf[i] >= mean)
			crossings++;

	rate = crossings * FS / 4096.0;

	snprintf(msg, sizeof(msg),
		 "%.0f Hz at 8000 (would be %.0f at 9600)", rate,
		 rate * 9600.0 / 8000.0);
	diff_eq_int(msg, rate > 1050.0 && rate < 1120.0, 1, 0);

	return diff_end();
}

/*
 * D13: the level argument barely changes the amplitude.  Over the whole range
 * of the signed char it takes, the output moves by about 1.4 dB, and the tone
 * is emitted at essentially full scale whatever is asked for.
 */
static int
run_level(void)
{
	short quietest, loudest, nominal;
	char msg[128];

	diff_begin("calling tone: the level argument");

	{
		struct calling_tone ct;

		ResetCallingTone(&ct, (char)-128);
		quietest = ct.amplitude;
		ResetCallingTone(&ct, (char)127);
		loudest = ct.amplitude;
		ResetCallingTone(&ct, (char)-12);
		nominal = ct.amplitude;
	}

	snprintf(msg, sizeof(msg),
		 "level -128 gives %d and level 127 gives %d, %.2f dB apart",
		 quietest, loudest,
		 20.0 * log10((double)loudest / quietest));
	diff_eq_int(msg,
		    20.0 * log10((double)loudest / quietest) < 2.0, 1, 0);

	/*
	 * -12 is a plausible dBm level to ask for and should give about a
	 * quarter of full scale.  It gives 0.99 of it.
	 */
	snprintf(msg, sizeof(msg),
		 "level -12 gives %d of 16384, not the 4115 that -12 dB means",
		 nominal);
	diff_eq_int(msg, nominal > 16000, 1, 0);

	return diff_end();
}

/*
 * D12: `end` is clamped against the block size rather than against the
 * samples left in the block, and the period counter is then decremented by
 * `end` rather than by the samples actually written.  So when a period ends
 * part-way through a block, the samples already emitted in that block are
 * subtracted from the NEXT period as well.
 *
 * The burst that ends is unaffected -- it gets its full length.  It is the
 * one that follows which comes up short, by exactly the number of samples
 * emitted before the transition.
 */
static int
off_burst_length(int block)
{
	struct calling_tone ct;
	short buf[512];
	int zeros = 0;
	int been_off = 0;
	int n;

	/*
	 * Every zero from reset until the tone comes back is silence: at level
	 * -12 the burst spans 23007 to 32540 and never passes through zero.
	 * Counting across the whole run rather than only over blocks that
	 * begin in the off state is the point -- the transition block holds
	 * some of each, and skipping it is what made the first version of this
	 * measurement report 40 samples too few.
	 */
	ResetCallingTone(&ct, (char)-12);

	for (n = 0; n < 2000; n++) {
		int i;

		GenerateCallingTone(&ct, buf, block);
		for (i = 0; i < block; i++)
			if (buf[i] == 0)
				zeros++;

		if (!ct.on)
			been_off = 1;
		else if (been_off)
			break;
	}
	return zeros;
}

static int
run_period_accounting(void)
{
	int aligned = off_burst_length(64);	/* 5760 = 90 * 64, exact  */
	int ragged = off_burst_length(50);	/* 5760 = 115.2 * 50      */
	char msg[128];

	diff_begin("calling tone: period accounting");

	/*
	 * With a block that divides the tone burst exactly, the transition
	 * lands on a block boundary, nothing has been emitted yet when the
	 * next period starts, and the silence gets its full length.
	 */
	snprintf(msg, sizeof(msg),
		 "block 64: silence is %d samples", aligned);
	diff_eq_int(msg, aligned, CALLING_TONE_OFF, 64);

	/*
	 * With 50, the burst ends 10 samples into a block, and the silence is
	 * short by exactly those 10.
	 */
	snprintf(msg, sizeof(msg),
		 "block 50: silence is %d, short of %d by %d",
		 ragged, CALLING_TONE_OFF, CALLING_TONE_OFF - ragged);
	diff_eq_int(msg, ragged, CALLING_TONE_OFF - 10, 50);
	/*
	 * Ten is exactly how far into a block the burst ended: 5760 is
	 * 115 blocks of 50 plus 10.
	 */
	diff_eq_int("the shortfall equals the burst's overhang",
		    CALLING_TONE_OFF - ragged, CALLING_TONE_ON % 50, 50);

	return diff_end();
}

/*
 * ---------------------------------------------------------------------------
 */
int
main(void)
{
	int rc = 0;

	rc |= run_reset();

	/*
	 * 5760 samples of tone, so 160 calls of 48 reach the first transition
	 * with room to spare, and 400 of 48 reach the second.
	 */
	rc |= run_generate("GenerateCallingTone: 48-sample blocks",
			   -12, 48, 400);
	rc |= run_generate("GenerateCallingTone: 40-sample blocks",
			   -12, 40, 480);
	rc |= run_generate("GenerateCallingTone: one sample at a time",
			   0, 1, 6000);
	rc |= run_generate("GenerateCallingTone: 50, which divides neither",
			   -43, 50, 400);
	rc |= run_generate("GenerateCallingTone: 512, longer than a burst",
			   0, 512, 60);

	rc |= run_waveform();
	rc |= run_overflow();
	rc |= run_repetition_rate();
	rc |= run_level();
	rc |= run_period_accounting();

	return rc;
}

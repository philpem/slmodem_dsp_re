/*
 * t_cid_fsd.c -- differential test of the Caller ID FSK demodulator.
 *
 * The demodulator is four stages deep and almost all of it is gated behind
 * state that takes hundreds of samples to build up: `high_level` needs 128
 * samples above threshold before the low end starts tracking at all, and the
 * threshold is not re-derived until 16 negatives after that and then every
 * 128.  A test of a few blocks reaches the discriminator and the low pass and
 * nothing else, so the long run here is not decoration -- it is the only way
 * the level tracking is executed.
 *
 * FIVE THINGS, all counted on the REFERENCE side and asserted at the end:
 *
 *   - bits come out, and both 0 and 1 do
 *   - both emission paths run: a whole bit of AGREEMENT, and half a bit of
 *     DISAGREEMENT, which is the path that re-times the receiver
 *   - `high_level` gets set
 *   - the low end is seeded at 16 negatives
 *   - and re-derived at a multiple of 128, which is the byte-wide test in
 *     the object
 *
 * A demodulator that emitted a constant, or one whose level tracking never
 * armed, would fail one of those with every differential comparison still
 * green.
 *
 * THE SIGNAL IS GENERATED AT 7200 Hz FOR THE 8000 CASE.  `cid->rate` says
 * 8000, the coefficient table is called 7200, and the bit length is six
 * samples: 1200 baud at 7200 Hz.  See src/service/Cidfsd.c.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/cid.h"

extern short ref_CID_FSD_demodulate(const short *samples, short *bits,
				    short count, void *cid);

extern const short ref_fix_LPF[];
extern const short ref_AUTOCOR_COEF_7200[];
extern const short ref_AUTOCOR_COEF_9600[];

#define GUARD	32
#define NSIG	2400

struct box {
	struct cid cid;
	unsigned char guard[GUARD];
};

/* Wrapped so the whole buffer is one diff_eq_obj check rather than 2400. */
struct bitbuf {
	short bit[NSIG + 8];
};

static short signal[NSIG];
static struct box boxa, boxb;
static struct bitbuf bufa, bufb;

static unsigned long seed = 1200130022UL;

static unsigned long
rnd(void)
{
	seed = seed * 1103515245UL + 12345UL;
	return (seed >> 8) & 0xffffffUL;
}

static void
fill_bytes(void *p, size_t n)
{
	unsigned char *b = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++)
		b[i] = (unsigned char)(rnd() & 0xff);
}

/* Anti-vacuity counters, all read off the reference object. */
static int seen_bit[2];
static int seen_run_path;
static int seen_opp_path;
static int seen_high_level;
static int seen_low_seed;
static int seen_low_recompute;
static int seen_dead_reset;

/* ------------------------------------------------------------------ */

/*
 * Phase-continuous 1200/2200 Hz FSK.  `spb` samples per bit; bit i of
 * `pattern` selects the mark.  Amplitude 0 gives silence with the phase
 * still advancing, which is what the dead-zone cases want.
 */
static void
fsk(short *out, int n, double fs, int spb, unsigned long pattern, double amp,
    double *ph)
{
	int i;

	for (i = 0; i < n; i++) {
		int b = (int)((pattern >> ((i / spb) & 31)) & 1UL);
		double f = b ? 1200.0 : 2200.0;
		double v = amp * sin(*ph);

		*ph += 2.0 * 3.14159265358979323846 * f / fs;
		if (v > 32767.0)
			v = 32767.0;
		if (v < -32768.0)
			v = -32768.0;
		out[i] = (short)v;
	}
}

static void
fresh(short rate)
{
	memset(&boxa, 0, sizeof(boxa));
	boxa.cid.rate = rate;
	memcpy(&boxb, &boxa, sizeof(boxa));
}

static void
seeded(short rate)
{
	fill_bytes(&boxa, sizeof(boxa));
	boxa.cid.rate = rate;
	/* The two circular indices are the only fields that INDEX anything. */
	boxa.cid.ac_idx = (short)(rnd() % 5UL);
	boxa.cid.lpf_idx = (short)(rnd() % 17UL);
	/* Keep the accumulators away from the edge of the int. */
	boxa.cid.thresh = (int)(rnd() % 4001UL) - 2000;
	boxa.cid.high_level = (int)(rnd() % 4001UL) - 2000;
	boxa.cid.low_sum = -(int)(rnd() % 40001UL);
	boxa.cid.high_sum = (int)(rnd() % 400001UL);
	boxa.cid.high_count = (short)(rnd() % 200UL);
	boxa.cid.low_count = (short)(rnd() % 300UL);
	boxa.cid.run = (short)((long)(rnd() % 20UL) - 9L);
	boxa.cid.opp = (short)((long)(rnd() % 20UL) - 9L);
	boxa.cid.dead = (short)(rnd() % 40UL);
	boxa.cid.last_bit = (short)(rnd() % 2UL);
	memcpy(&boxb, &boxa, sizeof(boxa));
}

/*
 * One call each, same inputs, then everything compared: the return, the bit
 * buffer including the part neither side should have touched, the object and
 * the guard.
 */
static short
run(const char *what, const short *in, int n, long tag)
{
	char buf[128];
	short ra, rb;

	memset(&bufa, 0x5a, sizeof(bufa));
	memset(&bufb, 0x5a, sizeof(bufb));

	ra = ref_CID_FSD_demodulate(in, bufa.bit, (short)n, &boxa.cid);
	rb = CID_FSD_demodulate(in, bufb.bit, (short)n, &boxb.cid);

	snprintf(buf, sizeof(buf), "%s: bits returned (%%ld)", what);
	diff_eq_int(buf, rb, ra, tag);
	snprintf(buf, sizeof(buf), "%s: bit buffer after %%ld", what);
	diff_eq_obj(buf, struct bitbuf, &bufb, &bufa, tag);
	snprintf(buf, sizeof(buf), "%s: object after %%ld", what);
	diff_eq_obj(buf, struct cid, &boxb.cid, &boxa.cid, tag);
	snprintf(buf, sizeof(buf), "%s: guard after %%ld", what);
	diff_eq_int(buf, memcmp(boxa.guard, boxb.guard, GUARD), 0, tag);

	return ra;
}

/*
 * The same, one sample at a time, watching the reference object between
 * samples.  Everything the counters record is a state transition that no
 * return value exposes.
 */
static void
run_sampled(const char *what, const short *in, int n, int baud, long tag)
{
	int i;

	for (i = 0; i < n; i++) {
		struct cid before = boxa.cid;

		if (run(what, in + i, 1, tag + i) != 0) {
			/*
			 * A bit came out.  Which path emitted it is exactly
			 * whether the last bit changed: the agreement path
			 * emits the bit already held, the disagreement path
			 * replaces it.
			 */
			if (boxa.cid.last_bit != before.last_bit)
				seen_opp_path++;
			else
				seen_run_path++;
			if (bufa.bit[0] == 0 || bufa.bit[0] == 1)
				seen_bit[bufa.bit[0]]++;
		}

		if (before.high_level == 0 && boxa.cid.high_level != 0)
			seen_high_level++;

		if (boxa.cid.low_count != before.low_count) {
			if (boxa.cid.low_count == 16)
				seen_low_seed++;
			if ((unsigned char)boxa.cid.low_count == 0x80 ||
			    (unsigned char)boxa.cid.low_count == 0)
				seen_low_recompute++;
		}

		if (boxa.cid.run == -baud && boxa.cid.opp == -baud &&
		    before.run != -baud)
			seen_dead_reset++;
	}
}

/* ------------------------------------------------------------------ */

static int
run_rate(short rate, double fs, int spb)
{
	char label[64];
	double ph = 0.0;
	int i;

	snprintf(label, sizeof(label), "CID_FSD_demodulate at %d",
		 (int)fs);
	diff_begin(label);

	/* Nothing at all, and the degenerate counts. */
	fresh(rate);
	memset(signal, 0, sizeof(signal));
	run("count 0", signal, 0, 0);
	run("count 1", signal, 1, 0);

	/*
	 * Silence long enough for the dead-zone timer to give up, which is
	 * the only thing that writes -baud into the two counters.
	 */
	fresh(rate);
	run("silence", signal, 200, 0);

	/*
	 * The same again from a CLEAN object, because the transition is what
	 * is being watched: once the timer has tripped, `run` stays at -baud
	 * and re-tripping is invisible.  Driving the sampled pass on the
	 * object the block above already tripped is how this counter read
	 * zero while every differential comparison passed.
	 */
	fresh(rate);
	run_sampled("silence sampled", signal, 120, spb, 0);

	/*
	 * The long one.  400 samples of unbroken mark builds `high_level`,
	 * then 2000 of alternating data drives the level tracking through
	 * its seed at 16 negatives and its re-derivation every 128.
	 */
	fresh(rate);
	ph = 0.0;
	fsk(signal, 400, fs, spb, 0xffffffffUL, 12000.0, &ph);
	fsk(signal + 400, NSIG - 400, fs, spb, 0xaaaaaaaaUL, 12000.0, &ph);
	run_sampled("mark then data", signal, NSIG, spb, 0);

	/* The same signal in blocks, so the bit buffer is exercised too. */
	fresh(rate);
	for (i = 0; i < NSIG; i += 300)
		run("blocked", signal + i, 300, i);

	/* A quiet signal, right down in the dead zone. */
	fresh(rate);
	ph = 0.0;
	fsk(signal, NSIG, fs, spb, 0xccccccccUL, 60.0, &ph);
	run("quiet data", signal, NSIG, 0);

	/* Full scale, and a square wave that never sits in the dead zone. */
	fresh(rate);
	ph = 0.0;
	fsk(signal, NSIG, fs, spb, 0x0f0f0f0fUL, 32767.0, &ph);
	run("full scale", signal, NSIG, 0);

	fresh(rate);
	for (i = 0; i < NSIG; i++)
		signal[i] = (short)((i / spb) & 1 ? 32767 : -32768);
	run("square", signal, NSIG, 0);

	/* Noise, and noise on top of a seeded object. */
	for (i = 0; i < NSIG; i++)
		signal[i] = (short)((long)(rnd() % 65536UL) - 32768L);
	fresh(rate);
	run("noise", signal, NSIG, 0);
	for (i = 0; i < 6; i++) {
		seeded(rate);
		run("noise from seeded", signal, NSIG, i);
	}

	/* And a seeded object down a real signal, a sample at a time. */
	seeded(rate);
	ph = 0.0;
	fsk(signal, NSIG, fs, spb, 0x36db6db7UL, 9000.0, &ph);
	run_sampled("seeded, sampled", signal, 800, spb, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;
	int i;

	diff_begin("FSD tables against the blob");
	for (i = 0; i < 17; i++)
		diff_eq_int("fix_LPF[%ld]", fix_LPF[i], ref_fix_LPF[i], i);
	for (i = 0; i < 5; i++) {
		diff_eq_int("AUTOCOR_COEF_7200[%ld]", AUTOCOR_COEF_7200[i],
			    ref_AUTOCOR_COEF_7200[i], i);
		diff_eq_int("AUTOCOR_COEF_9600[%ld]", AUTOCOR_COEF_9600[i],
			    ref_AUTOCOR_COEF_9600[i], i);
	}
	rc |= diff_end();

	/* 8000 on the object, 7200 on the wire, six samples to the bit. */
	rc |= run_rate(CID_RATE_8000, 7200.0, 6);
	rc |= run_rate(CID_RATE_9600, 9600.0, 8);

	diff_begin("the object's demodulator actually demodulated");
	diff_eq_int("zero bits emitted (%ld)", seen_bit[0] > 0, 1,
		    seen_bit[0]);
	diff_eq_int("one bits emitted (%ld)", seen_bit[1] > 0, 1,
		    seen_bit[1]);
	diff_eq_int("emitted after a whole bit of agreement (%ld)",
		    seen_run_path > 0, 1, seen_run_path);
	diff_eq_int("emitted after half a bit of disagreement (%ld)",
		    seen_opp_path > 0, 1, seen_opp_path);
	diff_eq_int("high_level derived (%ld)", seen_high_level > 0, 1,
		    seen_high_level);
	diff_eq_int("low end seeded at 16 (%ld)", seen_low_seed > 0, 1,
		    seen_low_seed);
	diff_eq_int("low end re-derived at a multiple of 128 (%ld)",
		    seen_low_recompute > 0, 1, seen_low_recompute);
	diff_eq_int("dead-zone timer gave up (%ld)", seen_dead_reset > 0, 1,
		    seen_dead_reset);
	rc |= diff_end();

	return rc;
}

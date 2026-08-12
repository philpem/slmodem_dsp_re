/*
 * t_cid_mtd.c -- differential test of the Caller ID mark-tone detector.
 *
 * Three things have to be shown, and the third is the one a detector test
 * usually skips:
 *
 *   - the four coefficient tables are the object's, byte for byte.  They are
 *     file-static there and here, so the comparison goes through
 *     CID_MTD_coeff rather than by name.
 *   - the two biquad states carry across calls, so the detector is driven a
 *     block at a time AND a sample at a time from a seeded object.
 *   - IT SAYS NO.  A detector exercised only on the signal it detects passes
 *     with the threshold deleted, so silence, out-of-band tones, noise and
 *     under-threshold tones are all here, and the reference side is asserted
 *     to have answered both ways.
 *
 * THE 150 IS PINNED INDEPENDENTLY.  `wide` is computed here from the input,
 * without going anywhere near the coefficients, and the reference is required
 * to answer "not detected" whenever it is 150 or below -- whatever the notch
 * residual does.  That is the energy gate itself under test, rather than a
 * sweep that hopes to cross it.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/cid.h"

extern short ref_CID_MTD_detect(const short *samples, short count, void *cid);

extern const short ref_MTD_COEF_1_8000[], ref_MTD_COEF_1_9600[];
extern const short ref_MTD_COEF_2_8000[], ref_MTD_COEF_2_9600[];

#define GUARD	32
#define NSAMP	300

struct box {
	struct cid cid;
	unsigned char guard[GUARD];
};

static unsigned long seed = 20260812UL;

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

/* Anti-vacuity, all measured on the REFERENCE side. */
static int seen_detect;
static int seen_reject;
static int seen_gate;		/* rejected with the energy gate closed */

/* ------------------------------------------------------------------ */

static void
tone(short *out, int n, double fs, double f, double amp, double *ph, int noise)
{
	int i;

	for (i = 0; i < n; i++) {
		double v;

		if (noise)
			v = ((double)(long)(rnd() % 20001UL) - 10000.0)
			    / 10000.0 * amp;
		else
			v = amp * sin(*ph);
		*ph += 2.0 * 3.14159265358979323846 * f / fs;
		if (v > 32767.0)
			v = 32767.0;
		if (v < -32768.0)
			v = -32768.0;
		out[i] = (short)v;
	}
}

/* The energy the detector accumulates, derived from the input alone. */
static unsigned int
wideband(const short *in, int n)
{
	unsigned int w = 0;
	int i;

	for (i = 0; i < n; i++)
		w += (unsigned int)(((int)in[i] * in[i] + 32) >> 6);
	return w;
}

static void
run(const char *what, struct box *a, struct box *b, const short *in, int n,
    long tag)
{
	char buf[128];
	short ra, rb;

	ra = ref_CID_MTD_detect(in, (short)n, &a->cid);
	rb = CID_MTD_detect(in, (short)n, &b->cid);

	snprintf(buf, sizeof(buf), "%s: return (%%ld)", what);
	diff_eq_int(buf, rb, ra, tag);
	snprintf(buf, sizeof(buf), "%s: object after %%ld", what);
	diff_eq_obj(buf, struct cid, &b->cid, &a->cid, tag);
	snprintf(buf, sizeof(buf), "%s: guard after %%ld", what);
	diff_eq_int(buf, memcmp(a->guard, b->guard, GUARD), 0, tag);

	if (ra == 0)
		seen_detect++;
	else {
		seen_reject++;
		if (wideband(in, n) <= 150)
			seen_gate++;
	}

	/*
	 * The energy gate, pinned on the reference without reference to the
	 * filters: at or below 150 the answer cannot be "detected".
	 */
	if (wideband(in, n) <= 150) {
		snprintf(buf, sizeof(buf),
			 "%s: energy <= 150 cannot detect (%%ld)", what);
		diff_eq_int(buf, ra, 1, tag);
	}
}

static void
fresh(struct box *a, struct box *b, short rate)
{
	memset(a, 0, sizeof(*a));
	a->cid.rate = rate;
	memcpy(b, a, sizeof(*a));
}

static void
seeded(struct box *a, struct box *b, short rate)
{
	fill_bytes(a, sizeof(*a));
	a->cid.rate = rate;
	memcpy(b, a, sizeof(*a));
}

/* ------------------------------------------------------------------ */

static int
run_rate(short rate, double fs)
{
	struct box a, b;
	short in[NSAMP];
	double ph;
	char label[64];
	int amp, i;
	int rc;

	snprintf(label, sizeof(label), "CID_MTD_detect at %d", (int)rate);
	diff_begin(label);

	/* Silence, and the degenerate lengths. */
	fresh(&a, &b, rate);
	memset(in, 0, sizeof(in));
	run("silence", &a, &b, in, 200, 0);
	run("count 0", &a, &b, in, 0, 0);
	run("count 1", &a, &b, in, 1, 0);

	/*
	 * The two tones it is built for, then two it is not, each from a
	 * clean object so the notches start settled the same way.
	 */
	{
		static const double hz[6] = { 1200.0, 1300.0, 1250.0, 2200.0,
					      400.0, 3000.0 };

		for (i = 0; i < 6; i++) {
			fresh(&a, &b, rate);
			ph = 0.0;
			tone(in, NSAMP, fs, hz[i], 8000.0, &ph, 0);
			run("tone", &a, &b, in, NSAMP, (long)hz[i]);
			/* A second block, so the states have to carry. */
			tone(in, NSAMP, fs, hz[i], 8000.0, &ph, 0);
			run("tone again", &a, &b, in, NSAMP, (long)hz[i]);
		}
	}

	/*
	 * An amplitude sweep across the energy gate.  64 samples of 1200 Hz
	 * puts `wide` at roughly 64*A*A/128, so the interesting amplitudes
	 * are the low ones and the sweep steps through 150 sample by sample.
	 */
	for (amp = 0; amp <= 40; amp++) {
		fresh(&a, &b, rate);
		ph = 0.0;
		tone(in, 64, fs, 1200.0, (double)amp, &ph, 0);
		run("gate sweep", &a, &b, in, 64, amp);
	}

	/* Full scale, both signs, and a DC block -- the accumulator's top. */
	fresh(&a, &b, rate);
	for (i = 0; i < NSAMP; i++)
		in[i] = (short)(i & 1 ? 32767 : -32768);
	run("full-scale alternation", &a, &b, in, NSAMP, 0);

	fresh(&a, &b, rate);
	for (i = 0; i < NSAMP; i++)
		in[i] = 32767;
	run("full-scale DC", &a, &b, in, NSAMP, 0);

	/*
	 * 257 full-scale samples wrap the 32-bit energy accumulator, which is
	 * D306.  Driven at 256 and at 300 so both sides are compared either
	 * side of the wrap rather than only inside it.
	 */
	fresh(&a, &b, rate);
	for (i = 0; i < NSAMP; i++)
		in[i] = (short)(i & 1 ? 32767 : -32768);
	run("energy accumulator under the wrap", &a, &b, in, 256, 0);
	fresh(&a, &b, rate);
	run("energy accumulator over the wrap", &a, &b, in, 300, 0);

	/* Noise, from a seeded object rather than a clean one. */
	for (i = 0; i < 4; i++) {
		seeded(&a, &b, rate);
		ph = 0.0;
		tone(in, NSAMP, fs, 0.0, 6000.0, &ph, 1);
		run("noise from seeded", &a, &b, in, NSAMP, i);
	}

	/*
	 * A sample at a time through the same tone, which is the only way the
	 * two biquad states get compared 200 times over instead of once.
	 */
	seeded(&a, &b, rate);
	ph = 0.0;
	tone(in, NSAMP, fs, 1200.0, 12000.0, &ph, 0);
	for (i = 0; i < NSAMP; i++)
		run("one sample", &a, &b, in + i, 1, i);

	rc = diff_end();
	return rc;
}

int
main(void)
{
	int rc = 0;
	int i;

	/*
	 * The tables.  Ours are static, so they are reached through the
	 * accessor; the blob's copies are ordinary globals once renamed.
	 */
	diff_begin("MTD_COEF tables against the blob");
	for (i = 0; i < 5; i++) {
		diff_eq_int("MTD_COEF_1_8000[%ld]",
			    CID_MTD_coeff(1, CID_RATE_8000)[i],
			    ref_MTD_COEF_1_8000[i], i);
		diff_eq_int("MTD_COEF_2_8000[%ld]",
			    CID_MTD_coeff(2, CID_RATE_8000)[i],
			    ref_MTD_COEF_2_8000[i], i);
		diff_eq_int("MTD_COEF_1_9600[%ld]",
			    CID_MTD_coeff(1, CID_RATE_9600)[i],
			    ref_MTD_COEF_1_9600[i], i);
		diff_eq_int("MTD_COEF_2_9600[%ld]",
			    CID_MTD_coeff(2, CID_RATE_9600)[i],
			    ref_MTD_COEF_2_9600[i], i);
	}
	diff_eq_int("no third table (%ld)",
		    CID_MTD_coeff(3, CID_RATE_8000) == 0, 1, 0);
	diff_eq_int("no third rate (%ld)", CID_MTD_coeff(1, 7200) == 0, 1, 0);
	rc |= diff_end();

	/* 8000 is the line rate here, not 7200: this half is not resampled. */
	rc |= run_rate(CID_RATE_8000, 8000.0);
	rc |= run_rate(CID_RATE_9600, 9600.0);

	diff_begin("CID_MTD_detect said both things");
	diff_eq_int("blocks the object called a mark tone (%ld)",
		    seen_detect > 0, 1, seen_detect);
	diff_eq_int("blocks it rejected (%ld)", seen_reject > 0, 1,
		    seen_reject);
	diff_eq_int("rejections with the energy gate closed (%ld)",
		    seen_gate > 0, 1, seen_gate);
	rc |= diff_end();

	return rc;
}

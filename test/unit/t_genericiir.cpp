/*
 * t_genericiir.cpp -- differential test of GenericIIR<float, double>.
 *
 * This is the first float module through the rig, so it also settles the
 * open question of whether x87 bit-exactness is achievable (docs/deviations.md
 * Q2).  If the two agree to the last bit across thousands of samples of a
 * recursive filter -- where any rounding difference compounds rather than
 * cancels -- the answer is yes and later float modules can be held to the
 * same standard.
 *
 * The originals live in .gnu.linkonce sections with mangled names; after
 * tools/symmap.py they are reachable as ref_<mangled>.  asm() labels bind to
 * them without needing a matching C++ declaration.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/GenericIIR.h"

/* The original object is 52 bytes; give it room and alignment to spare. */
union ref_obj {
	double align;
	char raw[64];
};

extern "C" {
void ref_ctor(void *self, unsigned nden, unsigned nnum, double *den,
	      double *num, unsigned blockSize)
	asm("ref__ZN10GenericIIRIfdEC1EjjPdS1_j");
void ref_dtor(void *self) asm("ref__ZN10GenericIIRIfdED1Ev");
void ref_reset(void *self) asm("ref__ZN10GenericIIRIfdE5resetEv");
float ref_process(void *self, float x) asm("ref__ZN10GenericIIRIfdE7processEf");
void ref_process_block(void *self, const float *in, float *out, unsigned n)
	asm("ref__ZN10GenericIIRIfdE7processEPKfPfj");
}

/* Compare two floats by bit pattern: -0.0 and NaN must match exactly too. */
static long
bits(float f)
{
	long v = 0;
	memcpy(&v, &f, sizeof(f));
	return v;
}

#define NSAMP 3000

static float input[NSAMP];

static void
make_input(void)
{
	unsigned lfsr = 0x1234u;

	for (int i = 0; i < NSAMP; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		/* A tone plus noise, scaled to a realistic sample range. */
		input[i] = (float)((int)(lfsr & 0x7fff) - 0x4000) / 32768.0f
			   + 0.4f * (float)((i % 7) - 3);
	}
}

static int
run(const char *label, unsigned nden, unsigned nnum, double *den, double *num,
    unsigned blockSize)
{
	union ref_obj ro;
	GenericIIR<float, double> ours(nden, nnum, den, num, blockSize);

	memset(&ro, 0, sizeof(ro));
	ref_ctor(&ro, nden, nnum, den, num, blockSize);

	diff_begin(label);
	for (int i = 0; i < NSAMP; i++) {
		float a = ref_process(&ro, input[i]);
		float b = ours.process(input[i]);
		diff_eq_int("sample %ld", bits(b), bits(a), i);
	}

	/* reset() must restore identical state, not merely similar. */
	ref_reset(&ro);
	ours.reset();
	for (int i = 0; i < 200; i++) {
		float a = ref_process(&ro, input[i]);
		float b = ours.process(input[i]);
		diff_eq_int("post-reset sample %ld", bits(b), bits(a), i);
	}

	int rc = diff_end();
	ref_dtor(&ro);
	return rc;
}

static int
run_block(const char *label, unsigned nden, unsigned nnum, double *den,
	  double *num, unsigned blockSize, unsigned chunk)
{
	union ref_obj ro;
	GenericIIR<float, double> ours(nden, nnum, den, num, blockSize);
	static float oa[NSAMP], ob[NSAMP];

	memset(&ro, 0, sizeof(ro));
	ref_ctor(&ro, nden, nnum, den, num, blockSize);

	diff_begin(label);
	for (unsigned pos = 0; pos < NSAMP; pos += chunk) {
		unsigned n = (pos + chunk > NSAMP) ? NSAMP - pos : chunk;
		ref_process_block(&ro, input + pos, oa + pos, n);
		ours.process(input + pos, ob + pos, n);
		for (unsigned k = 0; k < n; k++)
			diff_eq_int("block sample %ld", bits(ob[pos + k]),
				    bits(oa[pos + k]), pos + k);
	}
	int rc = diff_end();
	ref_dtor(&ro);
	return rc;
}

int
main(void)
{
	int rc = 0;

	/* A 2nd-order lowpass biquad: the shape most of dsplibs' filters take. */
	static double bq_den[3] = { 1.0, -1.5610180758, 0.6413515381 };
	static double bq_num[3] = { 0.0200833656, 0.0401667312, 0.0200833656 };

	/* den[0] == 0: the "already normalised, skip the divide" convention. */
	static double nodiv_den[3] = { 0.0, -1.5610180758, 0.6413515381 };

	/* den[0] != 1: exercises the division path with a real divisor. */
	static double scaled_den[3] = { 2.5, -1.5610180758, 0.6413515381 };

	/* Pure FIR: no feedback beyond den[0]. */
	static double fir_den[1] = { 1.0 };
	static double fir_num[8] = { 0.05, 0.1, 0.15, 0.2, 0.2, 0.15, 0.1, 0.05 };

	/* Higher order, to push the recursion harder. */
	static double h_den[5] = { 1.0, -3.1806, 3.8612, -2.1122, 0.4383 };
	static double h_num[5] = { 0.0009, 0.0036, 0.0055, 0.0036, 0.0009 };

	make_input();

	rc |= run("iir biquad", 3, 3, bq_den, bq_num, 64);
	rc |= run("iir den[0]==0", 3, 3, nodiv_den, bq_num, 64);
	rc |= run("iir den[0]==2.5", 3, 3, scaled_den, bq_num, 64);
	rc |= run("iir pure FIR", 1, 8, fir_den, fir_num, 64);
	rc |= run("iir 4th order", 5, 5, h_den, h_num, 64);

	/* Small blockSize forces frequent history compaction. */
	rc |= run("iir tight buffer", 3, 3, bq_den, bq_num, 1);
	rc |= run("iir 4th order tight", 5, 5, h_den, h_num, 2);

	rc |= run_block("iir block x64", 3, 3, bq_den, bq_num, 64, 64);
	rc |= run_block("iir block ragged", 5, 5, h_den, h_num, 64, 17);
	rc |= run_block("iir block x1", 3, 3, bq_den, bq_num, 64, 1);

	return rc;
}

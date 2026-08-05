/*
 * t_floatfir.cpp -- differential test of all six FloatFIR members.
 *
 * The fixture is t_v90jd.cpp's, with three differences forced by this class.
 *
 * THE OBJECT CANNOT LIVE IN A UNION.  FloatFIR declares a constructor and a
 * destructor -- they are two of the six symbols under test -- which makes it
 * non-trivial and deletes the default members of any union holding one
 * (finding 232).  So the slot is a plain aligned byte array and the object is
 * reached through a cast.
 *
 * THE CONSTRUCTOR AND DESTRUCTOR ARE CALLED THROUGH asm() LABELS, on our side
 * as well as the blob's.  Two reasons: our side has no other way to run a
 * constructor over existing storage without placement new (and there is no
 * <new> here -- the build is -nostdinc++), and it is a direct check that the
 * emitted symbol name is exactly the blob's.  Both variants of each are
 * called, because GCC emits C1/C2 and D1/D2 from one definition and this test
 * fails to link if it does not.
 *
 * ONE WORD OF THE OBJECT CAN NEVER COMPARE EQUAL.  `history` is a
 * sysdep_malloc return and the two sides allocate separately.  It is not
 * skipped: the snapshot below replaces it on each side with whether THAT
 * side's pointer is null, which is the only property of a heap address the
 * two runs can share (finding 224), and the buffer it points at is compared
 * in full separately.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/FloatFIR.h"

extern "C" {
void our_ctor(void *self, unsigned n, float *c, unsigned b)
	asm("_ZN8FloatFIRC1EjPfj");
void our_ctor2(void *self, unsigned n, float *c, unsigned b)
	asm("_ZN8FloatFIRC2EjPfj");
void our_dtor(void *self) asm("_ZN8FloatFIRD1Ev");
void our_dtor2(void *self) asm("_ZN8FloatFIRD2Ev");

void ref_ctor(void *self, unsigned n, float *c, unsigned b)
	asm("ref__ZN8FloatFIRC1EjPfj");
void ref_ctor2(void *self, unsigned n, float *c, unsigned b)
	asm("ref__ZN8FloatFIRC2EjPfj");
void ref_dtor(void *self) asm("ref__ZN8FloatFIRD1Ev");
void ref_dtor2(void *self) asm("ref__ZN8FloatFIRD2Ev");
void ref_reset(void *self) asm("ref__ZN8FloatFIR5resetEv");
int ref_setcoef(void *self, float *c, unsigned n)
	asm("ref__ZN8FloatFIR15setCoefficientsEPfj");
float ref_process1(void *self, float in) asm("ref__ZN8FloatFIR7processEf");
void ref_processn(void *self, const float *in, float *out, unsigned n)
	asm("ref__ZN8FloatFIR7processEPKfPfj");
}

/* The object, plus room past its end to catch a store that overruns it. */
#define SLOT 64
#define MAXBUF 512

static unsigned char ours_raw[SLOT] __attribute__((aligned(8)));
static unsigned char theirs_raw[SLOT] __attribute__((aligned(8)));

/* The history buffer, lifted out of each side's heap so it can be compared. */
struct fir_hist {
	unsigned int w[MAXBUF];
};

static FloatFIR *
O(void)
{
	return (FloatFIR *)ours_raw;
}

static FloatFIR *
T(void)
{
	return (FloatFIR *)theirs_raw;
}

/*
 * Seeds.  Never zero: a zero fill would let the constructor's clear loop stop
 * a word short and still compare equal, and would leave the convolution
 * summing zeros where a mispaired accumulator is invisible (findings 223,
 * 224).  `mode` varies what reaches the exponent field of the coefficients.
 */
static unsigned lfsr_state;

static unsigned
lfsr(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return lfsr_state;
}

static void
seed_slots(int trial)
{
	int i;

	lfsr_state = 0x2c1bu + 0x9e37u * (unsigned)trial;
	for (i = 0; i < SLOT; i++) {
		unsigned char v = (unsigned char)(lfsr() >> 3);

		ours_raw[i] = v;
		theirs_raw[i] = v;
	}
}

static float coefs[MAXBUF];

static void
seed_coefs(int trial, int mode)
{
	int i;

	lfsr_state = 0x5311u + 0x4e6du * (unsigned)trial;
	for (i = 0; i < MAXBUF; i++) {
		int m = (int)(lfsr() & 0x7fffu) - 0x4000;

		switch (mode) {
		case 0:
			coefs[i] = (float)m * 1.0e-4f;
			break;
		case 1:
			coefs[i] = (float)m;
			break;
		case 2:
			/* Alternating signs: the two accumulators see very
			 * different partial sums, so a swapped pairing shows. */
			coefs[i] = ((i & 1) ? -1.0f : 1.0f) * (float)m * 3.0e-3f;
			break;
		default:
			coefs[i] = (float)m * 7.3e-7f;
			break;
		}
	}
}

/*
 * Two coefficient sets designed against the x87, and the only reason the
 * claim "two extended-precision accumulators, paired even/odd" is testable at
 * all.  Ordinary coefficients do not distinguish it: 40 products of similar
 * magnitude summed in any order agree to far more than the 24 bits the result
 * is rounded to.  Both are driven with an input of exactly 1.0, so every
 * product is the coefficient itself and the sums below are what the object
 * computes, independent of where the window sits.
 *
 *   PAIRING   even taps alternate +2**70 and -2**70, odd taps are 1.  Two
 *             accumulators: the huge terms cancel in one and the ones survive
 *             in the other, giving 20 for a 40-tap filter.  One accumulator:
 *             each 1 is lost against the 2**70 sitting in the sum, giving 1.
 *
 *   PRECISION even taps run +2**60, 1, -2**60, 1; odd taps are 1.  Sixty-one
 *             significant bits, which an x87 register holds and a `double`
 *             does not: extended gives 30 for a 40-tap filter and double 25.
 *
 * 2**70 and 2**60 are exact as floats and as every wider type, so nothing
 * here depends on how a decimal literal rounds.
 */
#define TWO_P70 1180591620717411303424.0f
#define TWO_P60 1152921504606846976.0f

static void
seed_coefs_x87(int pairing)
{
	int i;

	for (i = 0; i < MAXBUF; i++) {
		if (i & 1)
			coefs[i] = 1.0f;
		else if (pairing)
			coefs[i] = (i & 2) ? -TWO_P70 : TWO_P70;
		else
			coefs[i] = ((i & 2) ? 1.0f :
				    ((i & 4) ? -TWO_P60 : TWO_P60));
	}
}

/*
 * A comparable copy of the object: everything as it stands, except that the
 * heap pointer becomes each side's own answer to "is it null".
 */
static void
snapshot(void *dst, const FloatFIR *src)
{
	memcpy(dst, src, sizeof(FloatFIR));
	((FloatFIR *)dst)->history = (float *)(src->history != 0 ? 1 : 0);
}

static void
cmp_obj(const char *what, int trial)
{
	unsigned char sa[sizeof(FloatFIR)], sb[sizeof(FloatFIR)];

	snapshot(sa, O());
	snapshot(sb, T());
	diff_eq_obj_(__FILE__, __LINE__, what, "FloatFIR", sa, sb,
		     sizeof(FloatFIR), (long)trial);
	diff_eq_int("no store past the object (trial %ld)",
		    memcmp(ours_raw + sizeof(FloatFIR),
			   theirs_raw + sizeof(FloatFIR),
			   SLOT - sizeof(FloatFIR)) == 0, 1, trial);
}

static void
cmp_hist(const char *what, int trial)
{
	struct fir_hist ha, hb;
	unsigned int n = O()->bufferLength;

	if (n > MAXBUF)
		n = MAXBUF;
	memset(&ha, 0, sizeof(ha));
	memset(&hb, 0, sizeof(hb));
	if (O()->history != 0)
		memcpy(&ha, O()->history, n * sizeof(float));
	if (T()->history != 0)
		memcpy(&hb, T()->history, n * sizeof(float));
	diff_eq_obj_(__FILE__, __LINE__, what, "struct fir_hist", &ha, &hb,
		     sizeof(struct fir_hist), (long)trial);
}

/* Bit patterns, not values: a float compared as a float hides a NaN and a
 * signed zero, and this test has no tolerance to widen. */
static unsigned
bits(float f)
{
	unsigned u;

	memcpy(&u, &f, sizeof(u));
	return u;
}

/* nTaps, blockSize -- the constructor's own pair is (0x28, 0x63). */
static const unsigned int shapes[][2] = {
	{ 40, 99 }, { 4, 1 }, { 4, 0 }, { 22, 30 }, { 20, 1 },
	{ 8, 64 }, { 39, 7 }, { 64, 128 }, { 12, 3 },
};
#define NSHAPE ((int)(sizeof(shapes) / sizeof(shapes[0])))

static void
build(int shape, int trial)
{
	seed_slots(trial);
	our_ctor(ours_raw, shapes[shape][0], coefs, shapes[shape][1]);
	ref_ctor(theirs_raw, shapes[shape][0], coefs, shapes[shape][1]);
}

static void
teardown(void)
{
	our_dtor(ours_raw);
	ref_dtor(theirs_raw);
}

static int
run_ctor(void)
{
	int shape, trial;

	diff_begin("FloatFIR::FloatFIR / ~FloatFIR");

	for (shape = 0; shape < NSHAPE; shape++) {
		for (trial = 0; trial < 2; trial++) {
			seed_coefs(trial, trial);

			/* C1 on one pass, C2 on the other: both variants are
			 * emitted from one definition and both must exist. */
			seed_slots(trial);
			if (trial == 0) {
				our_ctor(ours_raw, shapes[shape][0], coefs,
					 shapes[shape][1]);
				ref_ctor(theirs_raw, shapes[shape][0], coefs,
					 shapes[shape][1]);
			} else {
				our_ctor2(ours_raw, shapes[shape][0], coefs,
					  shapes[shape][1]);
				ref_ctor2(theirs_raw, shapes[shape][0], coefs,
					  shapes[shape][1]);
			}

			cmp_obj("after construction", shape);
			cmp_hist("history after construction", shape);
			diff_eq_int("taps is masked to a multiple of 4 (%ld)",
				    O()->taps, shapes[shape][0] & ~3u, shape);
			diff_eq_int("the buffer was allocated (shape %ld)",
				    O()->history != 0, 1, shape);

			if (trial == 0) {
				our_dtor(ours_raw);
				ref_dtor(theirs_raw);
			} else {
				our_dtor2(ours_raw);
				ref_dtor2(theirs_raw);
			}
			cmp_obj("after destruction", shape);
		}
	}

	/* The destructor's null arm, which no successful malloc reaches. */
	seed_slots(7);
	O()->history = 0;
	T()->history = 0;
	our_dtor(ours_raw);
	ref_dtor(theirs_raw);
	cmp_obj("after destruction with no buffer", 0);

	return diff_end();
}

static int
run_reset(void)
{
	int shape, trial;

	diff_begin("FloatFIR::reset");

	for (shape = 0; shape < NSHAPE; shape++) {
		seed_coefs(shape, shape % 4);
		build(shape, shape);

		for (trial = 0; trial < 4; trial++) {
			unsigned int k;

			/* Dirty the history and move the index off its
			 * starting value, so a reset that does nothing shows. */
			for (k = 0; k < O()->bufferLength; k++) {
				float v = (float)(int)(lfsr() & 0xffffu);

				O()->history[k] = v;
				T()->history[k] = v;
			}
			O()->index = T()->index = trial - 1;

			O()->reset();
			ref_reset(theirs_raw);

			cmp_obj("after reset", shape);
			cmp_hist("history after reset", shape);
			diff_eq_int("reset put the index at bufferLength - taps"
				    " (shape %ld)", O()->index,
				    (int)(O()->bufferLength - O()->taps),
				    shape);
		}

		/* reset() with no buffer: the clear is skipped, the index is
		 * still moved. */
		{
			float *ha = O()->history, *hb = T()->history;

			O()->history = 0;
			T()->history = 0;
			O()->index = T()->index = -12345;
			O()->reset();
			ref_reset(theirs_raw);
			cmp_obj("after reset with no buffer", shape);
			O()->history = ha;
			T()->history = hb;
		}

		teardown();
	}

	return diff_end();
}

static int
run_setcoefficients(void)
{
	static float other[MAXBUF];
	int shape, i, saw_reject = 0, saw_pull = 0, saw_same = 0;

	for (i = 0; i < MAXBUF; i++)
		other[i] = (float)i * 0.125f - 3.0f;

	diff_begin("FloatFIR::setCoefficients");

	for (shape = 0; shape < NSHAPE; shape++) {
		unsigned int want;

		seed_coefs(shape, shape % 4);

		/* Every count from 0 to a little past the buffer, and the
		 * index seeded above, below and at the boundary each time. */
		for (want = 0; want <= shapes[shape][0] + shapes[shape][1] + 8;
		     want++) {
			int seedidx;

			for (seedidx = 0; seedidx < 4; seedidx++) {
				int idx = (int)(shapes[shape][1]) - seedidx * 3;
				int ra, rb;

				build(shape, (int)want + seedidx);
				O()->index = T()->index = idx;

				ra = O()->setCoefficients(other, want);
				rb = ref_setcoef(theirs_raw, other, want);

				diff_eq_int("setCoefficients(%ld) return",
					    ra, rb, (long)want);
				cmp_obj("after setCoefficients", (int)want);
				cmp_hist("history after setCoefficients",
					 (int)want);

				if (ra == -1)
					saw_reject = 1;
				else if (O()->taps == (want & ~3u) &&
					 O()->index != idx)
					saw_pull = 1;
				if (ra == 0 && O()->coefficients == other &&
				    O()->index == idx)
					saw_same = 1;

				teardown();
			}
		}
	}

	diff_eq_int("some count was rejected", saw_reject, 1, 0);
	diff_eq_int("some call pulled the index back", saw_pull, 1, 0);
	diff_eq_int("some call left the index alone", saw_same, 1, 0);

	return diff_end();
}

static int
run_process_one(void)
{
	int shape, trial, wrapped = 0, distinct = 0;
	unsigned first = 0;

	diff_begin("FloatFIR::process(float)");

	for (shape = 0; shape < NSHAPE; shape++) {
		for (trial = 0; trial < 4; trial++) {
			int k, nsym;

			seed_coefs(trial, trial);
			build(shape, shape * 8 + trial);
			nsym = (int)(O()->bufferLength) * 3 + 5;

			for (k = 0; k < nsym; k++) {
				float in = (float)((int)(lfsr() & 0x1fffu) -
						   0x1000) * 0.01f;
				float a, b;
				int before = O()->index;

				a = O()->process(in);
				b = ref_process1(theirs_raw, in);

				diff_eq_int("process(float) output bits"
					    " (sample %ld)",
					    bits(a), bits(b), k);
				cmp_obj("after process(float)", k);
				cmp_hist("history after process(float)", k);

				if (O()->index > before)
					wrapped = 1;
				if (k == 0 && shape == 0 && trial == 0)
					first = bits(a);
				else if (bits(a) != first)
					distinct = 1;
			}

			teardown();
		}
	}

	diff_eq_int("the index wrapped at least once", wrapped, 1, 0);
	diff_eq_int("the output is not constant", distinct, 1, 0);

	return diff_end();
}

static int
run_process_block(void)
{
	static float in[MAXBUF];
	static float outa[MAXBUF], outb[MAXBUF];
	int shape, trial, wrapped = 0;

	diff_begin("FloatFIR::process(block)");

	for (shape = 0; shape < NSHAPE; shape++) {
		for (trial = 0; trial < 4; trial++) {
			int pass;
			unsigned int counts[6];

			seed_coefs(trial, trial);
			build(shape, shape * 16 + trial);

			counts[0] = 0;
			counts[1] = 1;
			counts[2] = shapes[shape][1];
			counts[3] = O()->bufferLength;
			counts[4] = 2;
			counts[5] = O()->bufferLength + 7;

			for (pass = 0; pass < 6; pass++) {
				unsigned int n = counts[pass], k;
				int before;

				if (n > MAXBUF)
					n = MAXBUF;
				for (k = 0; k < MAXBUF; k++)
					in[k] = (float)((int)(lfsr() & 0x1fffu)
							- 0x1000) * 0.01f;
				memset(outa, 0x5a, sizeof(outa));
				memset(outb, 0x5a, sizeof(outb));

				before = O()->index;
				O()->process(in, outa, n);
				ref_processn(theirs_raw, in, outb, n);

				diff_eq_int("process(block %ld) output",
					    memcmp(outa, outb,
						   sizeof(outa)) == 0, 1,
					    (long)n);
				cmp_obj("after process(block)", pass);
				cmp_hist("history after process(block)", pass);

				if (n == 0)
					diff_eq_int("count 0 left the index"
						    " alone (shape %ld)",
						    O()->index, before, shape);
				if (O()->index > before)
					wrapped = 1;
			}

			teardown();
		}
	}

	diff_eq_int("a block run wrapped at least once", wrapped, 1, 0);

	return diff_end();
}

/*
 * The x87 run: constant 1.0 input against the two coefficient sets above, in
 * both the scalar and the block form.  What it asserts beyond agreement with
 * the blob is that the run REACHED the discriminating values -- a filter
 * whose history is still full of zeros produces 0.0 on both sides and proves
 * nothing.
 */
static int
run_process_x87(void)
{
	static float in[MAXBUF], outa[MAXBUF], outb[MAXBUF];
	int shape, pairing, saw_pair = 0, saw_prec = 0;

	diff_begin("FloatFIR::process x87 accumulators");

	for (pairing = 0; pairing < 2; pairing++) {
		seed_coefs_x87(pairing);

		for (shape = 0; shape < NSHAPE; shape++) {
			unsigned int k, n;
			float want;

			build(shape, shape);
			n = O()->bufferLength * 2 + 8;
			if (n > MAXBUF)
				n = MAXBUF;

			for (k = 0; k < MAXBUF; k++)
				in[k] = 1.0f;

			/* Scalar form, one sample at a time. */
			for (k = 0; k < n; k++) {
				float a = O()->process(1.0f);
				float b = ref_process1(theirs_raw, 1.0f);

				diff_eq_int("x87 process(float) bits"
					    " (sample %ld)",
					    bits(a), bits(b), (long)k);
				cmp_obj("after x87 process(float)", shape);
				cmp_hist("history after x87 process(float)",
					 shape);
			}

			/* With the window full of 1.0, the answer is fixed. */
			want = pairing ? (float)(O()->taps / 2)
				       : (float)(O()->taps / 2 +
						 (O()->taps / 8) * 2);
			if (O()->process(1.0f) == want) {
				if (pairing)
					saw_pair = 1;
				else
					saw_prec = 1;
			}

			teardown();

			/* Block form over the same input. */
			build(shape, shape + 64);
			memset(outa, 0x5a, sizeof(outa));
			memset(outb, 0x5a, sizeof(outb));
			O()->process(in, outa, n);
			ref_processn(theirs_raw, in, outb, n);
			diff_eq_int("x87 process(block) output (shape %ld)",
				    memcmp(outa, outb, sizeof(outa)) == 0, 1,
				    shape);
			cmp_obj("after x87 process(block)", shape);
			cmp_hist("history after x87 process(block)", shape);
			teardown();
		}
	}

	diff_eq_int("the pairing set reached its steady value", saw_pair, 1, 0);
	diff_eq_int("the precision set reached its steady value",
		    saw_prec, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_ctor();
	rc |= run_reset();
	rc |= run_setcoefficients();
	rc |= run_process_one();
	rc |= run_process_block();
	rc |= run_process_x87();

	return rc;
}

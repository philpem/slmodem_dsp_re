/*
 * t_floatarma.cpp -- differential test of all five FloatARMA members.
 *
 * The fixture is t_floatfir.cpp's, with the differences this class forces.
 *
 * THE OBJECT CANNOT LIVE IN A UNION.  FloatARMA declares a constructor and a
 * destructor -- two of the five symbols under test -- which makes it
 * non-trivial and deletes the default members of any union holding one
 * (finding F232).  So the slot is a plain aligned byte array reached through a
 * cast.
 *
 * THE CONSTRUCTOR AND DESTRUCTOR ARE CALLED THROUGH asm() LABELS on both
 * sides.  Ours has no other way to run a constructor over existing storage
 * without placement new (there is no <new> here -- the build is -nostdinc++),
 * and it is a direct check that the emitted symbol name is exactly the
 * blob's.  Both variants of each are called, because GCC emits C1/C2 and
 * D1/D2 from one definition and this test fails to link if it does not.
 *
 * FOUR WORDS OF THE OBJECT CAN NEVER COMPARE EQUAL.  All four buffers are
 * sysdep_malloc returns and the two sides allocate separately.  They are not
 * skipped: the snapshot replaces each with whether THAT side's pointer is
 * null, which is the only property of a heap address the two runs can share
 * (finding F224), and all four buffers are compared in full separately.
 *
 * AND COMPARING TWO DESTROYED OBJECTS COMPARES THE ALLOCATORS.  The
 * destructor does not null what it frees, so after it runs the two objects
 * hold four dangling addresses that will never be equal and that the snapshot
 * flattens to "non-null" on both sides -- which is exactly the vacuous
 * comparison this project has been caught by three times.  The destructor is
 * therefore checked through `harness_alloc` instead: four frees, no bad
 * frees, and nothing left live.
 *
 * NOTHING HERE IS DRIVEN FROM A ZERO STATE.  An ARMA filter over a zeroed
 * history and a zero input is zero whatever its coefficients are, so both
 * histories are seeded with a live pattern before every `process` run and the
 * input is never zero.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/FloatARMA.h"

extern "C" {
void *sysdep_malloc(unsigned size);
void sysdep_free(void *ptr);

void our_ctor(void *self, unsigned nd, unsigned nn, float *d, float *n,
	      unsigned b) asm("_ZN9FloatARMAC1EjjPfS0_j");
void our_ctor2(void *self, unsigned nd, unsigned nn, float *d, float *n,
	       unsigned b) asm("_ZN9FloatARMAC2EjjPfS0_j");
void our_dtor(void *self) asm("_ZN9FloatARMAD1Ev");
void our_dtor2(void *self) asm("_ZN9FloatARMAD2Ev");

void ref_ctor(void *self, unsigned nd, unsigned nn, float *d, float *n,
	      unsigned b) asm("ref__ZN9FloatARMAC1EjjPfS0_j");
void ref_ctor2(void *self, unsigned nd, unsigned nn, float *d, float *n,
	       unsigned b) asm("ref__ZN9FloatARMAC2EjjPfS0_j");
void ref_dtor(void *self) asm("ref__ZN9FloatARMAD1Ev");
void ref_dtor2(void *self) asm("ref__ZN9FloatARMAD2Ev");
void ref_reset(void *self) asm("ref__ZN9FloatARMA5resetEv");
float ref_process1(void *self, float in) asm("ref__ZN9FloatARMA7processEf");
void ref_processn(void *self, const float *in, float *out, unsigned n)
	asm("ref__ZN9FloatARMA7processEPKfPfj");
}

/* The object, plus room past its end to catch a store that overruns it. */
#define SLOT 80
#define MAXBUF 256
#define MAXRUN 64

static unsigned char ours_raw[SLOT] __attribute__((aligned(8)));
static unsigned char theirs_raw[SLOT] __attribute__((aligned(8)));

struct arma_buf {
	unsigned int w[MAXBUF];
};

struct arma_run {
	unsigned int w[MAXRUN];
};

static FloatARMA *
O(void)
{
	return (FloatARMA *)ours_raw;
}

static FloatARMA *
T(void)
{
	return (FloatARMA *)theirs_raw;
}

/*
 * Seeds.  Never zero: a zero fill would let a clear loop stop a word short
 * and still compare equal, and would leave both dot products summing zeros
 * where a mispaired accumulator is invisible (findings F223, F224).
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

static float den[MAXBUF];
static float num[MAXBUF];

/*
 * `den[0]` is the normalisation divisor and the constructor compares it
 * against exactly 1.0f to decide whether to scale at all, so every mode says
 * explicitly which arm it is exercising.  Mode 3 is the `== 1.0f` arm.
 */
static void
seed_coefs(int trial, int mode)
{
	int i;

	lfsr_state = 0x5311u + 0x4e6du * (unsigned)trial;
	for (i = 0; i < MAXBUF; i++) {
		int m = (int)(lfsr() & 0x7fffu) - 0x4000;

		switch (mode) {
		case 0:
			den[i] = (float)m * 1.0e-4f;
			num[i] = (float)(m ^ 0x1234) * 3.0e-5f;
			break;
		case 1:
			den[i] = (float)m;
			num[i] = (float)(m ^ 0x0555);
			break;
		case 2:
			/* Alternating signs: the two accumulators see very
			 * different partial sums, so a swapped pairing shows. */
			den[i] = ((i & 1) ? -1.0f : 1.0f) * (float)m * 3.0e-3f;
			num[i] = ((i & 1) ? 1.0f : -1.0f) * (float)m * 5.0e-3f;
			break;
		default:
			den[i] = (float)m * 7.3e-7f;
			num[i] = (float)m * 2.9e-6f;
			break;
		}
	}
	if (mode == 3)
		den[0] = 1.0f;		/* the `no scaling` arm */
	else if (den[0] == 1.0f || den[0] == 0.0f)
		den[0] = 2.5f;		/* and never it by accident */
}

/*
 * Two coefficient sets designed against the x87, and the only reason the
 * claim "two extended-precision accumulators, paired even/odd" is testable at
 * all.  Ordinary coefficients do not distinguish it: products of similar
 * magnitude summed in any order agree to far more than the 24 bits the result
 * is rounded to.  The argument is t_floatfir.cpp's; what is different here is
 * that BOTH dot products have to be reached, so the histories are seeded
 * directly rather than being driven up from zero.
 *
 *   PAIRING   even taps alternate +2**70 and -2**70, odd taps are 1.  Two
 *             accumulators: the huge terms cancel in one and the ones survive
 *             in the other.  One accumulator: each 1 is lost against the
 *             2**70 sitting in the sum.
 *
 *   PRECISION even taps run +2**60, 1, -2**60, 1; odd taps are 1.  Sixty-one
 *             significant bits, which an x87 register holds and a `double`
 *             does not.
 *
 * `den[0]` is 1.0f throughout so the constructor does not scale the pattern
 * away; the constructor still writes 0.0f over `m_a[0]`, which is why the
 * feedback sum's first product is absent by design.
 */
#define TWO_P70 1180591620717411303424.0f
#define TWO_P60 1152921504606846976.0f

static void
seed_coefs_x87(int pairing)
{
	int i;

	for (i = 0; i < MAXBUF; i++) {
		float v;

		if (i & 1)
			v = 1.0f;
		else if (pairing)
			v = (i & 2) ? -TWO_P70 : TWO_P70;
		else
			v = ((i & 2) ? 1.0f : ((i & 4) ? -TWO_P60 : TWO_P60));
		den[i] = v;
		num[i] = v;
	}
	den[0] = 1.0f;
}

/*
 * A comparable copy of the object: everything as it stands, except that the
 * four heap pointers become each side's own answer to "is it null".
 */
static void
snapshot(void *dst, const FloatARMA *src)
{
	FloatARMA *d = (FloatARMA *)dst;

	memcpy(dst, src, sizeof(FloatARMA));
	d->m_a = (float *)(src->m_a != 0 ? 1 : 0);
	d->m_b = (float *)(src->m_b != 0 ? 1 : 0);
	d->m_xhist = (float *)(src->m_xhist != 0 ? 1 : 0);
	d->m_yhist = (float *)(src->m_yhist != 0 ? 1 : 0);
}

static void
cmp_obj(const char *what, int trial)
{
	unsigned char sa[sizeof(FloatARMA)], sb[sizeof(FloatARMA)];

	snapshot(sa, O());
	snapshot(sb, T());
	diff_eq_obj_(__FILE__, __LINE__, what, "FloatARMA", sa, sb,
		     sizeof(FloatARMA), (long)trial);
	diff_eq_int("no store past the object (trial %ld)",
		    memcmp(ours_raw + sizeof(FloatARMA),
			   theirs_raw + sizeof(FloatARMA),
			   SLOT - sizeof(FloatARMA)) == 0, 1, trial);
}

static void
cmp_buf(const char *what, const float *a, const float *b, unsigned int n,
	int trial)
{
	struct arma_buf ba, bb;

	if (n > MAXBUF)
		n = MAXBUF;
	memset(&ba, 0, sizeof(ba));
	memset(&bb, 0, sizeof(bb));
	if (a != 0)
		memcpy(&ba, a, n * sizeof(float));
	if (b != 0)
		memcpy(&bb, b, n * sizeof(float));
	diff_eq_obj_(__FILE__, __LINE__, what, "struct arma_buf", &ba, &bb,
		     sizeof(struct arma_buf), (long)trial);
}

static void
cmp_all(const char *what, int trial)
{
	cmp_obj(what, trial);
	cmp_buf("m_a", O()->m_a, T()->m_a, O()->m_nA, trial);
	cmp_buf("m_b", O()->m_b, T()->m_b, O()->m_nB, trial);
	cmp_buf("m_xhist", O()->m_xhist, T()->m_xhist, O()->m_xlen, trial);
	cmp_buf("m_yhist", O()->m_yhist, T()->m_yhist, O()->m_ylen, trial);
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

/*
 * nDen, nNum, blockSize.  Chosen so that neither tap count is already a
 * multiple of four in most rows -- the two pad loops are bounded by the
 * CALLER's counts, so a row of multiples leaves both of them dead -- and so
 * that m_nA != m_nB, which is what makes the two carry-tail loops copy
 * different numbers of words and a swapped pair separable.  Row 3 has
 * blockSize 0, so both positions start at 0 and wrap on the very first
 * sample.
 */
static const unsigned int shapes[][3] = {
	{ 5, 9, 7 },	/* m_nA 8, m_nB 12 -- both pad loops run       */
	{ 4, 4, 1 },	/* already multiples: both pad loops are dead  */
	{ 1, 1, 3 },	/* nDen 1: the a-scaling loop never runs       */
	{ 8, 4, 0 },	/* blockSize 0: wraps on every sample          */
	{ 3, 7, 16 },
	{ 12, 5, 2 },
	{ 2, 15, 5 },
	{ 9, 2, 9 },
};
#define NSHAPE ((int)(sizeof(shapes) / sizeof(shapes[0])))

static void
build(int shape, int trial)
{
	seed_slots(trial);
	our_ctor(ours_raw, shapes[shape][0], shapes[shape][1], den, num,
		 shapes[shape][2]);
	ref_ctor(theirs_raw, shapes[shape][0], shapes[shape][1], den, num,
		 shapes[shape][2]);
}

static void
teardown(void)
{
	our_dtor(ours_raw);
	ref_dtor(theirs_raw);
}

/*
 * Put a live, side-identical pattern into both histories on both sides.
 *
 * `flat` fills them with exactly 1.0f instead, and it is what makes the x87
 * coefficient sets work: every product is then the coefficient itself, so the
 * +2**70 and -2**70 terms CANCEL inside one accumulator and the ones survive
 * in the other.  With a random history they do not cancel, both accumulators
 * carry terms of order 2**77, and the ones are lost in every arrangement --
 * which is exactly what happened: the mutations `one accumulator, not two`
 * and `the taps pair the other way round` were both NOT CAUGHT until this
 * argument existed.
 */
static void
dirty_hist(int trial, int flat)
{
	unsigned int k;

	lfsr_state = 0x7a19u + 0x2b3du * (unsigned)trial;
	for (k = 0; k < O()->m_xlen; k++) {
		float v = flat ? 1.0f :
		    (float)((int)(lfsr() & 0x7fffu) - 0x4000) * 1.0e-2f;

		O()->m_xhist[k] = v;
		T()->m_xhist[k] = v;
	}
	for (k = 0; k < O()->m_ylen; k++) {
		float v = flat ? 1.0f :
		    (float)((int)(lfsr() & 0x7fffu) - 0x4000) * 3.0e-2f;

		O()->m_yhist[k] = v;
		T()->m_yhist[k] = v;
	}
}

static int
run_ctor(void)
{
	int shape, trial;

	diff_begin("FloatARMA::FloatARMA / ~FloatARMA");

	for (shape = 0; shape < NSHAPE; shape++) {
		for (trial = 0; trial < 4; trial++) {
			unsigned int nd = shapes[shape][0];
			unsigned int nn = shapes[shape][1];

			seed_coefs(trial, trial);
			harness_alloc_reset();

			/* C1 on two passes, C2 on the other two: both
			 * variants come from one definition and both must
			 * exist. */
			seed_slots(trial);
			if (trial & 1) {
				our_ctor2(ours_raw, nd, nn, den, num,
					  shapes[shape][2]);
				ref_ctor2(theirs_raw, nd, nn, den, num,
					  shapes[shape][2]);
			} else {
				our_ctor(ours_raw, nd, nn, den, num,
					 shapes[shape][2]);
				ref_ctor(theirs_raw, nd, nn, den, num,
					 shapes[shape][2]);
			}

			cmp_all("after construction", shape);

			diff_eq_int("m_nA is nDen rounded UP to a multiple of"
				    " four (shape %ld)", O()->m_nA,
				    (nd + 3u) & ~3u, shape);
			diff_eq_int("m_nB is nNum rounded UP to a multiple of"
				    " four (shape %ld)", O()->m_nB,
				    (nn + 3u) & ~3u, shape);
			diff_eq_int("m_xlen is m_nB + blockSize (shape %ld)",
				    O()->m_xlen, O()->m_nB + shapes[shape][2],
				    shape);
			diff_eq_int("m_ylen is m_nA + blockSize (shape %ld)",
				    O()->m_ylen, O()->m_nA + shapes[shape][2],
				    shape);
			diff_eq_int("m_a[0] is zeroed after normalisation"
				    " (shape %ld)", bits(O()->m_a[0]), 0u,
				    shape);
			diff_eq_int("four allocations (shape %ld)",
				    harness_alloc.allocs, 8, shape);
			diff_eq_int("and they are the four lengths in the map"
				    " (shape %ld)", harness_alloc.bytes,
				    2 * (O()->m_nA + O()->m_nB + O()->m_xlen +
					 O()->m_ylen) * sizeof(float), shape);

			/*
			 * The constructor ends by resetting: both histories
			 * cleared, both positions rewound, and m_idx left on
			 * the SECOND clear loop's count (finding F874).
			 */
			diff_eq_int("the constructor rewound m_xpos"
				    " (shape %ld)", O()->m_xpos,
				    (int)(O()->m_xlen - O()->m_nB), shape);
			diff_eq_int("the constructor rewound m_ypos"
				    " (shape %ld)", O()->m_ypos,
				    (int)(O()->m_ylen - O()->m_nA), shape);
			diff_eq_int("the constructor left m_idx at m_ylen"
				    " (shape %ld)", O()->m_idx, O()->m_ylen,
				    shape);
			{
				unsigned int k;
				int cleared = 1;

				for (k = 0; k < O()->m_xlen; k++)
					if (bits(O()->m_xhist[k]) != 0u)
						cleared = 0;
				for (k = 0; k < O()->m_ylen; k++)
					if (bits(O()->m_yhist[k]) != 0u)
						cleared = 0;
				diff_eq_int("both histories were cleared"
					    " (shape %ld)", cleared, 1, shape);
			}

			/*
			 * The scaling arm, asserted against arithmetic this
			 * test does rather than against the object: with
			 * den[0] != 1 every b coefficient below nNum is the
			 * caller's divided by it.  Guarded on the value being
			 * exactly representable so the check is bit-exact.
			 */
			if (trial != 3 && nn > 1) {
				float want = 1.0f / den[0] * num[1];

				diff_eq_int("m_b[1] was scaled by 1/den[0]"
					    " (shape %ld)", bits(O()->m_b[1]),
					    bits(want), shape);
			}
			if (trial == 3 && nn > 1)
				diff_eq_int("den[0] == 1.0f leaves m_b alone"
					    " (shape %ld)", bits(O()->m_b[1]),
					    bits(num[1]), shape);

			if (trial & 1) {
				our_dtor2(ours_raw);
				ref_dtor2(theirs_raw);
			} else {
				our_dtor(ours_raw);
				ref_dtor(theirs_raw);
			}

			diff_eq_int("the destructor freed all four (shape %ld)",
				    harness_alloc.frees, 8, shape);
			diff_eq_int("nothing left live (shape %ld)",
				    harness_alloc.live, 0, shape);
			diff_eq_int("no bad free (shape %ld)",
				    harness_alloc.bad_free, 0, shape);
		}
	}

	/*
	 * The destructor's null arms, which no successful malloc reaches.
	 * One pointer at a time, so a destructor that frees the wrong member
	 * shows up as a bad free rather than as the same total.
	 */
	{
		int which;

		for (which = 0; which < 4; which++) {
			float **ours, **theirs;

			seed_coefs(1, 0);
			harness_alloc_reset();
			build(0, which);

			switch (which) {
			case 0:
				ours = &O()->m_a; theirs = &T()->m_a; break;
			case 1:
				ours = &O()->m_b; theirs = &T()->m_b; break;
			case 2:
				ours = &O()->m_xhist;
				theirs = &T()->m_xhist; break;
			default:
				ours = &O()->m_yhist;
				theirs = &T()->m_yhist; break;
			}
			sysdep_free(*ours);
			sysdep_free(*theirs);
			*ours = 0;
			*theirs = 0;

			our_dtor(ours_raw);
			ref_dtor(theirs_raw);
			diff_eq_int("member %ld freed by hand, the other three"
				    " by the destructor",
				    harness_alloc.frees, 8, which);
			diff_eq_int("still no bad free (member %ld)",
				    harness_alloc.bad_free, 0, which);
			diff_eq_int("two null frees counted (member %ld)",
				    harness_alloc.free_null, 0, which);
			diff_eq_int("nothing left live (member %ld)",
				    harness_alloc.live, 0, which);
		}
	}

	/*
	 * nDen == 0.  The constructor still writes 0.0f over m_a[0], through
	 * a zero-length allocation -- see finding F875.  Construct and destroy
	 * only: with m_nA == 0 the carry-tail loop's count underflows and
	 * `process` would not return.
	 */
	{
		seed_coefs(2, 0);
		harness_alloc_reset();
		seed_slots(11);
		our_ctor(ours_raw, 0, 8, den, num, 4);
		ref_ctor(theirs_raw, 0, 8, den, num, 4);
		cmp_all("after construction with no denominator", 0);
		diff_eq_int("m_nA is zero (%ld)", O()->m_nA, 0, 0);
		diff_eq_int("m_ylen is blockSize (%ld)", O()->m_ylen, 4, 0);
		our_dtor(ours_raw);
		ref_dtor(theirs_raw);
		diff_eq_int("nothing left live (%ld)", harness_alloc.live, 0,
			    0);
	}

	/*
	 * den[0] == 0.0f.  The divisor is compared against 1.0f and against
	 * nothing else, so this arm SCALES -- by 1/0 -- and every coefficient
	 * below the caller's count comes out infinite or indefinite.  It is
	 * the only fixture that can separate `!= 1.0f` from `!= 0.0f`: at
	 * exactly 1.0f the two arms are numerically identical, because
	 * multiplying by 1.0f/1.0f changes nothing, so the mutation that
	 * swaps the constant was NOT CAUGHT until this case existed.
	 */
	{
		seed_coefs(4, 0);
		den[0] = 0.0f;
		harness_alloc_reset();
		seed_slots(13);
		our_ctor(ours_raw, 5, 9, den, num, 6);
		ref_ctor(theirs_raw, 5, 9, den, num, 6);
		cmp_all("after construction with a zero divisor", 0);
		diff_eq_int("m_b[1] is not finite (%ld)",
			    (bits(O()->m_b[1]) & 0x7f800000u) == 0x7f800000u,
			    1, 0);
		diff_eq_int("m_a[0] is still zeroed (%ld)",
			    bits(O()->m_a[0]), 0u, 0);
		our_dtor(ours_raw);
		ref_dtor(theirs_raw);
		diff_eq_int("nothing left live (%ld)", harness_alloc.live, 0,
			    0);
	}

	return diff_end();
}

static int
run_reset(void)
{
	int shape, trial;

	diff_begin("FloatARMA::reset");

	for (shape = 0; shape < NSHAPE; shape++) {
		seed_coefs(shape, shape % 4);
		build(shape, shape);

		for (trial = 0; trial < 4; trial++) {
			dirty_hist(trial, 0);
			O()->m_xpos = T()->m_xpos = trial - 1;
			O()->m_ypos = T()->m_ypos = 3 - trial;
			O()->m_idx = T()->m_idx = 0xdeadbeefu;

			O()->reset();
			ref_reset(theirs_raw);

			cmp_all("after reset", shape);
			diff_eq_int("reset put m_xpos at m_xlen - m_nB"
				    " (shape %ld)", O()->m_xpos,
				    (int)(O()->m_xlen - O()->m_nB), shape);
			diff_eq_int("reset put m_ypos at m_ylen - m_nA"
				    " (shape %ld)", O()->m_ypos,
				    (int)(O()->m_ylen - O()->m_nA), shape);
			/*
			 * m_idx is a member and the fill loops leave it
			 * behind; the value it ends on is m_ylen, and it is
			 * the SECOND loop's, not the first's (finding F874).
			 */
			diff_eq_int("reset left m_idx at m_ylen (shape %ld)",
				    O()->m_idx, O()->m_ylen, shape);
		}

		teardown();
	}

	return diff_end();
}

static int
run_process1(void)
{
	int shape, mode;

	diff_begin("FloatARMA::process(float)");

	for (shape = 0; shape < NSHAPE; shape++) {
		for (mode = 0; mode < 6; mode++) {
			int i;

			if (mode < 4)
				seed_coefs(shape + mode, mode);
			else
				seed_coefs_x87(mode == 4);
			build(shape, shape * 8 + mode);
			dirty_hist(shape + mode, mode >= 4);

			lfsr_state = 0x1357u + 0x71c3u * (unsigned)mode;
			for (i = 0; i < MAXRUN; i++) {
				float in;
				float ra, rb;

				if (mode >= 4)
					in = 1.0f;
				else
					in = (float)((int)(lfsr() & 0x7fffu)
						     - 0x4000) * 1.0e-3f;
				/* Never zero: a zero drive lets a wrong
				 * history index survive unnoticed. */
				if (in == 0.0f)
					in = 0.5f;

				ra = O()->process(in);
				rb = ref_process1(theirs_raw, in);

				diff_eq_int("process(float) return, sample %ld",
					    bits(ra), bits(rb), i);
				cmp_all("after process(float)", i);
			}

			teardown();
		}
	}

	return diff_end();
}

static int
run_processn(void)
{
	static float in[MAXRUN];
	static float oa[MAXRUN], ob[MAXRUN];
	int shape, mode;

	diff_begin("FloatARMA::process(const float *, float *, unsigned)");

	for (shape = 0; shape < NSHAPE; shape++) {
		for (mode = 0; mode < 6; mode++) {
			int i;
			unsigned int run;

			if (mode < 4)
				seed_coefs(shape + mode, mode);
			else
				seed_coefs_x87(mode == 4);

			lfsr_state = 0x24b1u + 0x51a7u * (unsigned)mode;
			for (i = 0; i < MAXRUN; i++) {
				if (mode >= 4)
					in[i] = 1.0f;
				else
					in[i] = (float)((int)(lfsr() & 0x7fffu)
							- 0x4000) * 1.0e-3f;
				if (in[i] == 0.0f)
					in[i] = 0.5f;
			}

			/* Several run lengths, including 1 and 0: a count of
			 * zero returns before the object is read at all. */
			static const unsigned int runs[] = {
				0, 1, 2, 3, 7, 16, MAXRUN
			};
			unsigned int r;

			for (r = 0; r < sizeof(runs) / sizeof(runs[0]); r++) {
				run = runs[r];
				build(shape, shape * 16 + mode);
				dirty_hist(shape + mode + (int)run, mode >= 4);

				memset(oa, 0x5a, sizeof(oa));
				memset(ob, 0x5a, sizeof(ob));

				O()->process(in, oa, run);
				ref_processn(theirs_raw, in, ob, run);

				diff_eq_obj_(__FILE__, __LINE__,
					     "block output", "struct arma_run",
					     oa, ob, sizeof(struct arma_run),
					     (long)run);
				cmp_all("after block process", (int)run);

				teardown();
			}
		}
	}

	/*
	 * The block form and the single-sample form must agree sample for
	 * sample: they are the same arithmetic, and only the block form keeps
	 * the positions in registers across the run.  This is the check that
	 * separates "the block form writes m_xpos once" from "it writes it
	 * wrongly once".
	 */
	for (shape = 0; shape < NSHAPE; shape++) {
		int i;

		seed_coefs(shape, shape % 4);

		for (i = 0; i < MAXRUN; i++)
			in[i] = (float)((i * 37) % 251 - 125) * 1.0e-2f;

		build(shape, shape);
		dirty_hist(shape, 0);
		O()->process(in, oa, MAXRUN);
		teardown();

		build(shape, shape);
		dirty_hist(shape, 0);
		for (i = 0; i < MAXRUN; i++)
			ob[i] = O()->process(in[i]);
		teardown();

		for (i = 0; i < MAXRUN; i++)
			diff_eq_int("block and single agree at sample %ld",
				    bits(oa[i]), bits(ob[i]), i);
	}

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_ctor();
	bad |= run_reset();
	bad |= run_process1();
	bad |= run_processn();

	return bad;
}

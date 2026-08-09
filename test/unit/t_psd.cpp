/*
 * t_psd.cpp -- differential test of five of Psd's six members.
 *
 * `Psd::process` is not reconstructed and is not exercised here; the two
 * reasons are in include/dsplib/Psd.h and in finding 876.
 *
 * THE OBJECT CANNOT LIVE IN A UNION and the constructor and destructor are
 * called through asm() labels on both sides, for t_floatfir.cpp's reasons.
 *
 * TWO WORDS CAN NEVER COMPARE EQUAL -- both buffers are sysdep_malloc returns
 * -- so the snapshot replaces each with that side's own answer to "is it
 * null", and the buffers are compared in full separately.  `m_fft` is never
 * written by anything this file calls, and it is still compared: the harness
 * fills every fresh allocation with 0xa5 (HARNESS_MALLOC_FILL), so a
 * constructor that wrote into it would show up on one side and not the other.
 *
 * AND THE FLAT WINDOW IS THE VACUOUS CASE.  `WINDOW_BOXCAR` fills the array
 * with 1.0f, so a `setWindowType` that ignored its argument and always chose
 * boxcar would compare equal to a boxcar reference for ever.  Every window
 * type is therefore checked against the OTHERS as well as against the blob:
 * `windows_differ` asserts that the four shapes are four shapes.
 *
 * COMPARING TWO DESTROYED OBJECTS COMPARES THE ALLOCATORS -- the destructor
 * does not null what it frees -- so the destructor is checked through
 * `harness_alloc`, not through the object.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/Psd.h"

extern "C" {
void *sysdep_malloc(unsigned size);
void sysdep_free(void *ptr);

void our_ctor(void *self, unsigned n, int w, unsigned o)
	asm("_ZN3PsdC1Ej10WindowTypej");
void our_ctor2(void *self, unsigned n, int w, unsigned o)
	asm("_ZN3PsdC2Ej10WindowTypej");
void our_dtor(void *self) asm("_ZN3PsdD1Ev");
void our_dtor2(void *self) asm("_ZN3PsdD2Ev");

void ref_ctor(void *self, unsigned n, int w, unsigned o)
	asm("ref__ZN3PsdC1Ej10WindowTypej");
void ref_ctor2(void *self, unsigned n, int w, unsigned o)
	asm("ref__ZN3PsdC2Ej10WindowTypej");
void ref_dtor(void *self) asm("ref__ZN3PsdD1Ev");
void ref_dtor2(void *self) asm("ref__ZN3PsdD2Ev");
void ref_setoverlap(void *self, unsigned o)
	asm("ref__ZN3Psd16setOverlapLengthEj");
void ref_setwindow(void *self, int w) asm("ref__ZN3Psd13setWindowTypeE10WindowType");
void ref_getfreq(const void *self, float *out, float rate)
	asm("ref__ZNK3Psd14getFrequenciesEPff");
}

/*
 * The `WindowType` argument is an `int` in the declarations above for
 * t_lowpassfir.cpp's reason: an enum passed by value through a C linkage
 * declaration is promoted anyway, and writing `int` keeps the two sides'
 * declarations identical.
 */

#define SLOT 32
#define MAXLEN 128

static unsigned char ours_raw[SLOT] __attribute__((aligned(8)));
static unsigned char theirs_raw[SLOT] __attribute__((aligned(8)));

struct psd_buf {
	unsigned int w[MAXLEN + 1];
};

static Psd *
O(void)
{
	return (Psd *)ours_raw;
}

static Psd *
T(void)
{
	return (Psd *)theirs_raw;
}

static unsigned lfsr_state;

static unsigned
lfsr(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return lfsr_state;
}

/* Never zero: a zero fill makes a constructor that writes nothing look right. */
static void
seed_slots(int trial)
{
	int i;

	lfsr_state = 0x63d1u + 0x9e37u * (unsigned)trial;
	for (i = 0; i < SLOT; i++) {
		unsigned char v = (unsigned char)(lfsr() >> 3);

		ours_raw[i] = v;
		theirs_raw[i] = v;
	}
}

static void
snapshot(void *dst, const Psd *src)
{
	Psd *d = (Psd *)dst;

	memcpy(dst, src, sizeof(Psd));
	d->m_window = (float *)(src->m_window != 0 ? 1 : 0);
	d->m_fft = (float *)(src->m_fft != 0 ? 1 : 0);
}

static void
cmp_obj(const char *what, int trial)
{
	unsigned char sa[sizeof(Psd)], sb[sizeof(Psd)];

	snapshot(sa, O());
	snapshot(sb, T());
	diff_eq_obj_(__FILE__, __LINE__, what, "Psd", sa, sb, sizeof(Psd),
		     (long)trial);
	diff_eq_int("no store past the object (trial %ld)",
		    memcmp(ours_raw + sizeof(Psd), theirs_raw + sizeof(Psd),
			   SLOT - sizeof(Psd)) == 0, 1, trial);
}

static void
cmp_buf(const char *what, const float *a, const float *b, unsigned int n,
	int trial)
{
	struct psd_buf ba, bb;

	if (n > MAXLEN + 1)
		n = MAXLEN + 1;
	memset(&ba, 0, sizeof(ba));
	memset(&bb, 0, sizeof(bb));
	if (a != 0)
		memcpy(&ba, a, n * sizeof(float));
	if (b != 0)
		memcpy(&bb, b, n * sizeof(float));
	diff_eq_obj_(__FILE__, __LINE__, what, "struct psd_buf", &ba, &bb,
		     sizeof(struct psd_buf), (long)trial);
}

static void
cmp_all(const char *what, int trial)
{
	cmp_obj(what, trial);
	cmp_buf("m_window", O()->m_window, T()->m_window, O()->m_length,
		trial);
	cmp_buf("m_fft", O()->m_fft, T()->m_fft, O()->m_length + 1, trial);
}

static unsigned
bits(float f)
{
	unsigned u;

	memcpy(&u, &f, sizeof(u));
	return u;
}

/* length, overlap */
/*
 * length, overlap.  The last row is not arbitrary: `getFrequencies` forms
 * (i * rate) * (1 / length) and NOT i * (rate / length), and the two agree to
 * the last bit over every ordinary length and sample rate.  A search over
 * lengths to 512 and eight rates found the first separating triple at
 * length 104, rate 1234.5678, bin 39 -- 462.962891 against 462.962921 -- so
 * that length and that rate are both here on purpose.  Without them the
 * association is claimed by the comment and checked by nothing (the mutation
 * `the product is associated the other way` was NOT CAUGHT until they went
 * in).
 */
static const unsigned int shapes[][2] = {
	{ 64, 32 }, { 16, 0 }, { 128, 96 }, { 5, 2 }, { 1, 0 },
	{ 2, 1 }, { 33, 7 }, { 8, 8 }, { 104, 40 },
};
#define NSHAPE ((int)(sizeof(shapes) / sizeof(shapes[0])))

static const int wtypes[] = {
	WINDOW_BOXCAR, WINDOW_HANNING, WINDOW_HAMMING, WINDOW_BLACKMAN
};
#define NWTYPE ((int)(sizeof(wtypes) / sizeof(wtypes[0])))

/*
 * The anti-vacuity check for every window assertion in this file: four
 * distinct shapes, held side by side.  Boxcar is all ones, so without this a
 * `setWindowType` that ignored its argument would agree with a boxcar
 * reference for ever and disagree with nothing.
 */
static int
windows_differ(void)
{
	static float w[NWTYPE][MAXLEN];
	int i, j, k, distinct = 0;
	unsigned int n = 64;

	diff_begin("Psd: the four window shapes are four shapes");

	for (i = 0; i < NWTYPE; i++)
		designWindow((WindowType)wtypes[i], w[i], n);

	for (i = 0; i < NWTYPE; i++) {
		for (j = i + 1; j < NWTYPE; j++) {
			int same = 1;

			for (k = 0; k < (int)n; k++)
				if (bits(w[i][k]) != bits(w[j][k]))
					same = 0;
			distinct += !same;
			diff_eq_int("window %ld differs from the next",
				    same, 0, i * NWTYPE + j);
		}
	}
	diff_eq_int("all six pairs differ (%ld)", distinct, 6, 0);

	return diff_end();
}

static void
build(int shape, int wt, int trial)
{
	seed_slots(trial);
	our_ctor(ours_raw, shapes[shape][0], wtypes[wt], shapes[shape][1]);
	ref_ctor(theirs_raw, shapes[shape][0], wtypes[wt], shapes[shape][1]);
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
	int shape, wt;

	diff_begin("Psd::Psd / ~Psd");

	for (shape = 0; shape < NSHAPE; shape++) {
		for (wt = 0; wt < NWTYPE; wt++) {
			unsigned int n = shapes[shape][0];
			unsigned int ov = shapes[shape][1];

			harness_alloc_reset();
			seed_slots(shape * NWTYPE + wt);

			/* C1 and C2 alternately: both are emitted from one
			 * definition and this test fails to link if not. */
			if ((shape + wt) & 1) {
				our_ctor2(ours_raw, n, wtypes[wt], ov);
				ref_ctor2(theirs_raw, n, wtypes[wt], ov);
			} else {
				our_ctor(ours_raw, n, wtypes[wt], ov);
				ref_ctor(theirs_raw, n, wtypes[wt], ov);
			}

			cmp_all("after construction", shape * NWTYPE + wt);
			diff_eq_int("m_length is the argument (%ld)",
				    O()->m_length, n, shape);
			diff_eq_int("m_overlap is the argument (%ld)",
				    O()->m_overlap, ov, shape);
			diff_eq_int("two allocations (shape %ld)",
				    harness_alloc.allocs, 4, shape);
			diff_eq_int("m_fft holds %ld + 1 floats",
				    harness_alloc.bytes,
				    2 * (n + n + 1) * sizeof(float), n);

			/*
			 * The window really is the one asked for: compared
			 * against a freshly designed copy, not only against
			 * the blob, so that both sides choosing the same
			 * WRONG window would still fail.
			 */
			{
				static float want[MAXLEN];
				unsigned int k;
				int same = 1;

				designWindow((WindowType)wtypes[wt], want, n);
				for (k = 0; k < n; k++)
					if (bits(O()->m_window[k]) !=
					    bits(want[k]))
						same = 0;
				diff_eq_int("m_window is designWindow(%ld)",
					    same, 1, wtypes[wt]);
			}

			/* m_fft is untouched: still the allocator's fill. */
			{
				unsigned int k;
				int untouched = 1;

				for (k = 0; k < n + 1; k++)
					if (bits(O()->m_fft[k]) != 0xa5a5a5a5u)
						untouched = 0;
				diff_eq_int("m_fft is left as allocated"
					    " (shape %ld)", untouched, 1,
					    shape);
			}

			if ((shape + wt) & 1) {
				our_dtor2(ours_raw);
				ref_dtor2(theirs_raw);
			} else {
				our_dtor(ours_raw);
				ref_dtor(theirs_raw);
			}

			diff_eq_int("the destructor freed both (shape %ld)",
				    harness_alloc.frees, 4, shape);
			diff_eq_int("nothing left live (shape %ld)",
				    harness_alloc.live, 0, shape);
			diff_eq_int("no bad free (shape %ld)",
				    harness_alloc.bad_free, 0, shape);
		}
	}

	/* The destructor's null arms, one member at a time. */
	{
		int which;

		for (which = 0; which < 2; which++) {
			harness_alloc_reset();
			build(0, 1, which);

			if (which == 0) {
				sysdep_free(O()->m_window);
				sysdep_free(T()->m_window);
				O()->m_window = 0;
				T()->m_window = 0;
			} else {
				sysdep_free(O()->m_fft);
				sysdep_free(T()->m_fft);
				O()->m_fft = 0;
				T()->m_fft = 0;
			}

			our_dtor(ours_raw);
			ref_dtor(theirs_raw);
			diff_eq_int("member %ld freed by hand, the other by"
				    " the destructor", harness_alloc.frees, 4,
				    which);
			diff_eq_int("no bad free (member %ld)",
				    harness_alloc.bad_free, 0, which);
			diff_eq_int("no free(NULL) (member %ld)",
				    harness_alloc.free_null, 0, which);
			diff_eq_int("nothing left live (member %ld)",
				    harness_alloc.live, 0, which);
		}
	}

	return diff_end();
}

static int
run_setters(void)
{
	int shape, wt, i;

	diff_begin("Psd::setOverlapLength / Psd::setWindowType");

	for (shape = 0; shape < NSHAPE; shape++) {
		build(shape, 0, shape);

		for (i = 0; i < 4; i++) {
			unsigned int ov = (unsigned int)(i * 7 + 1);

			O()->setOverlapLength(ov);
			ref_setoverlap(theirs_raw, ov);
			cmp_all("after setOverlapLength", shape);
			diff_eq_int("m_overlap took the argument (%ld)",
				    O()->m_overlap, ov, ov);
		}

		/*
		 * Every window type in turn over ONE object, so each call has
		 * to overwrite the previous shape rather than agreeing with
		 * whatever was already there.
		 */
		for (wt = 0; wt < NWTYPE; wt++) {
			O()->setWindowType((WindowType)wtypes[wt]);
			ref_setwindow(theirs_raw, wtypes[wt]);
			cmp_all("after setWindowType", shape * NWTYPE + wt);
			diff_eq_int("setWindowType stores nothing (%ld)",
				    O()->m_overlap, T()->m_overlap, wt);
		}

		teardown();
	}

	return diff_end();
}

static int
run_getfrequencies(void)
{
	static const float rates[] = {
		8000.0f, 7200.0f, 1.0f, 0.0f, -8000.0f, 3.14159f,
		1234.5678f	/* separates the two associations; see shapes[] */
	};
	static float fa[MAXLEN], fb[MAXLEN];
	int shape, r;

	diff_begin("Psd::getFrequencies");

	for (shape = 0; shape < NSHAPE; shape++) {
		build(shape, 2, shape);

		for (r = 0; r < (int)(sizeof(rates) / sizeof(rates[0])); r++) {
			unsigned int k;
			unsigned int bins = O()->m_length >> 1;

			memset(fa, 0x3c, sizeof(fa));
			memset(fb, 0x3c, sizeof(fb));

			O()->getFrequencies(fa, rates[r]);
			ref_getfreq(theirs_raw, fb, rates[r]);

			for (k = 0; k < MAXLEN; k++)
				diff_eq_int("getFrequencies bin %ld",
					    bits(fa[k]), bits(fb[k]), k);

			/*
			 * Only length/2 bins are written, and the last one is
			 * not zero for any non-zero rate -- otherwise a loop
			 * that ran no iterations would pass on the fill alone.
			 */
			diff_eq_int("bin %ld past the half is untouched",
				    bits(fa[bins]), 0x3c3c3c3cu, bins);
			if (bins > 1 && rates[r] != 0.0f)
				diff_eq_int("the top bin is not zero (%ld)",
					    bits(fa[bins - 1]) != 0u, 1,
					    shape);
		}

		teardown();
	}

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= windows_differ();
	bad |= run_ctor();
	bad |= run_setters();
	bad |= run_getfrequencies();

	return bad;
}

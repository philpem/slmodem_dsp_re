/*
 * t_resampler.cpp -- the four `Resampler` classes against the blob's own.
 *
 *     Resampler  <-  ResamplerTimingOffset  <-  ResamplerTiming
 *                                            <-  V90Resampler
 *
 * Both sides' constructors run on raw buffers of our own, each side calling
 * its own `sysdep_malloc`, and every comparison is against `ref_*`.
 *
 * ---------------------------------------------------------------------------
 * THREE THINGS THIS FILE IS CAREFUL ABOUT, EACH FOR A RECORDED REASON
 *
 * TWO EMPTY THINGS COMPARE EQUAL.  Every constructor here zeroes its history
 * buffer, so a comparison made straight after construction compares two runs
 * of zeros and proves nothing about the resampler.  Everything that can be
 * seeded IS seeded -- the coefficient banks, the parameter block, the fields
 * `reset` is supposed to clear -- and `seeded_ok` asserts at the end of the
 * run that the seeds actually reached the object.
 *
 * COMPARING TWO OBJECTS AFTER A DESTRUCTOR COMPARES THE ALLOCATORS.  The
 * destructors are checked on the allocation LOG -- how many blocks each side
 * released and whether the borrowed bank survived -- and not by looking at
 * the corpse.
 *
 * THE THREE POINTER FIELDS ALWAYS DIFFER and always will: `coeffs`,
 * `history` and `timingHistory` hold two different heap addresses, and the
 * vptr at +0x00 holds `&_ZTV...[2]` on one side and `&ref__ZTV...[2]` on the
 * other.  `cmp_obj` blanks exactly those four words and compares everything
 * else; what the buffers CONTAIN is then compared separately through the
 * pointers, which is the part that carries the information.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/debug.h"
#include "dsplib/sysdep.h"
#include "dsplib/V90Resampler.h"

/*
 * Each side's own entry points.  Ours get an `asm` label too, so both sides
 * are reached the same way and neither is a C++ call the compiler could
 * inline or devirtualise out from under the test.
 */
extern "C" {
extern unsigned int ref_dsplibs_debug_level;

/* Resampler */
void rs_ctor(void *s, unsigned p, float k, unsigned t, float c, unsigned m)
	asm("_ZN9ResamplerC1Ejfjfj");
void ref_rs_ctor(void *s, unsigned p, float k, unsigned t, float c, unsigned m)
	asm("ref__ZN9ResamplerC1Ejfjfj");
void rs_ctor_b(void *s, unsigned p, float k, unsigned t, float *b, unsigned m)
	asm("_ZN9ResamplerC1EjfjPfj");
void ref_rs_ctor_b(void *s, unsigned p, float k, unsigned t, float *b,
		   unsigned m) asm("ref__ZN9ResamplerC1EjfjPfj");
void rs_dtor(void *s) asm("_ZN9ResamplerD1Ev");
void ref_rs_dtor(void *s) asm("ref__ZN9ResamplerD1Ev");
void rs_reset(void *s) asm("_ZN9Resampler5resetEv");
void ref_rs_reset(void *s) asm("ref__ZN9Resampler5resetEv");
void rs_setnp(void *s, float p) asm("_ZN9Resampler18setNormalizedPhaseEf");
void ref_rs_setnp(void *s, float p)
	asm("ref__ZN9Resampler18setNormalizedPhaseEf");
float rs_getnp(const void *s) asm("_ZNK9Resampler18getNormalizedPhaseEv");
float ref_rs_getnp(const void *s)
	asm("ref__ZNK9Resampler18getNormalizedPhaseEv");
void rs_resample(void *s, const float *in, unsigned n, float *out,
		 unsigned *nOut) asm("_ZN9Resampler8resampleEPKfjPfRj");
void ref_rs_resample(void *s, const float *in, unsigned n, float *out,
		     unsigned *nOut) asm("ref__ZN9Resampler8resampleEPKfjPfRj");
void vr_resample(void *s, const float *in, unsigned n, float *out,
		 unsigned *nOut) asm("_ZN12V90Resampler8resampleEPKfjPfRj");
void ref_vr_resample(void *s, const float *in, unsigned n, float *out,
		     unsigned *nOut)
	asm("ref__ZN12V90Resampler8resampleEPKfjPfRj");
void rs_copytail(void *s) asm("_ZN9Resampler15copyHistoryTailEv");
void ref_rs_copytail(void *s) asm("ref__ZN9Resampler15copyHistoryTailEv");
void rs_resetidx(void *s) asm("_ZN9Resampler17resetHistoryIndexEv");
void ref_rs_resetidx(void *s) asm("ref__ZN9Resampler17resetHistoryIndexEv");

/* ResamplerTimingOffset */
void rto_ctor(void *s, unsigned p, float k, unsigned t, float c, float ppm,
	      unsigned m) asm("_ZN21ResamplerTimingOffsetC1Ejfjffj");
void ref_rto_ctor(void *s, unsigned p, float k, unsigned t, float c, float ppm,
		  unsigned m) asm("ref__ZN21ResamplerTimingOffsetC1Ejfjffj");
void rto_dtor(void *s) asm("_ZN21ResamplerTimingOffsetD1Ev");
void ref_rto_dtor(void *s) asm("ref__ZN21ResamplerTimingOffsetD1Ev");
void rto_reset(void *s) asm("_ZN21ResamplerTimingOffset5resetEv");
void ref_rto_reset(void *s) asm("ref__ZN21ResamplerTimingOffset5resetEv");
void rto_set(void *s, float ppm)
	asm("_ZN21ResamplerTimingOffset15setTimingOffsetEf");
void ref_rto_set(void *s, float ppm)
	asm("ref__ZN21ResamplerTimingOffset15setTimingOffsetEf");
float rto_getppm(const void *s)
	asm("_ZNK21ResamplerTimingOffset18getTimingOffsetPPMEv");
float ref_rto_getppm(const void *s)
	asm("ref__ZNK21ResamplerTimingOffset18getTimingOffsetPPMEv");

/* ResamplerTiming */
void rt_ctor(void *s, unsigned p, float k, unsigned t, float c, float ppm,
	     unsigned m) asm("_ZN15ResamplerTimingC1Ejfjffj");
void ref_rt_ctor(void *s, unsigned p, float k, unsigned t, float c, float ppm,
		 unsigned m) asm("ref__ZN15ResamplerTimingC1Ejfjffj");
void rt_dtor(void *s) asm("_ZN15ResamplerTimingD1Ev");
void ref_rt_dtor(void *s) asm("ref__ZN15ResamplerTimingD1Ev");
void rt_reset(void *s, unsigned n) asm("_ZN15ResamplerTiming5resetEj");
void ref_rt_reset(void *s, unsigned n) asm("ref__ZN15ResamplerTiming5resetEj");
/* The C2 / D2 / D0 variants and the adopting overloads -- see case_variants. */
void rs_ctor2(void *s, unsigned p, float k, unsigned t, float c, unsigned m)
	asm("_ZN9ResamplerC2Ejfjfj");
void ref_rs_ctor2(void *s, unsigned p, float k, unsigned t, float c,
		  unsigned m) asm("ref__ZN9ResamplerC2Ejfjfj");
void rs_ctor2_b(void *s, unsigned p, float k, unsigned t, float *b, unsigned m)
	asm("_ZN9ResamplerC2EjfjPfj");
void ref_rs_ctor2_b(void *s, unsigned p, float k, unsigned t, float *b,
		    unsigned m) asm("ref__ZN9ResamplerC2EjfjPfj");
void rs_dtor2(void *s) asm("_ZN9ResamplerD2Ev");
void ref_rs_dtor2(void *s) asm("ref__ZN9ResamplerD2Ev");
void rs_dtor0(void *s) asm("_ZN9ResamplerD0Ev");
void ref_rs_dtor0(void *s) asm("ref__ZN9ResamplerD0Ev");
void rs_tc(void *s, float v) asm("_ZN9Resampler16timingCorrectionEf");
void ref_rs_tc(void *s, float v) asm("ref__ZN9Resampler16timingCorrectionEf");

void rto_ctor_b(void *s, unsigned p, float k, unsigned t, float *b, float ppm,
		unsigned m) asm("_ZN21ResamplerTimingOffsetC1EjfjPffj");
void ref_rto_ctor_b(void *s, unsigned p, float k, unsigned t, float *b,
		    float ppm, unsigned m)
	asm("ref__ZN21ResamplerTimingOffsetC1EjfjPffj");
void rto_ctor2(void *s, unsigned p, float k, unsigned t, float c, float ppm,
	       unsigned m) asm("_ZN21ResamplerTimingOffsetC2Ejfjffj");
void ref_rto_ctor2(void *s, unsigned p, float k, unsigned t, float c,
		   float ppm, unsigned m)
	asm("ref__ZN21ResamplerTimingOffsetC2Ejfjffj");
void rto_ctor2_b(void *s, unsigned p, float k, unsigned t, float *b, float ppm,
		 unsigned m) asm("_ZN21ResamplerTimingOffsetC2EjfjPffj");
void ref_rto_ctor2_b(void *s, unsigned p, float k, unsigned t, float *b,
		     float ppm, unsigned m)
	asm("ref__ZN21ResamplerTimingOffsetC2EjfjPffj");
void rto_dtor2(void *s) asm("_ZN21ResamplerTimingOffsetD2Ev");
void ref_rto_dtor2(void *s) asm("ref__ZN21ResamplerTimingOffsetD2Ev");
void rto_dtor0(void *s) asm("_ZN21ResamplerTimingOffsetD0Ev");
void ref_rto_dtor0(void *s) asm("ref__ZN21ResamplerTimingOffsetD0Ev");
void rto_tc(void *s, float v)
	asm("_ZN21ResamplerTimingOffset16timingCorrectionEf");
void ref_rto_tc(void *s, float v)
	asm("ref__ZN21ResamplerTimingOffset16timingCorrectionEf");

void rt_ctor_b(void *s, unsigned p, float k, unsigned t, float *b, float ppm,
	       unsigned m) asm("_ZN15ResamplerTimingC1EjfjPffj");
void ref_rt_ctor_b(void *s, unsigned p, float k, unsigned t, float *b,
		   float ppm, unsigned m)
	asm("ref__ZN15ResamplerTimingC1EjfjPffj");
void rt_ctor2(void *s, unsigned p, float k, unsigned t, float c, float ppm,
	      unsigned m) asm("_ZN15ResamplerTimingC2Ejfjffj");
void ref_rt_ctor2(void *s, unsigned p, float k, unsigned t, float c, float ppm,
		  unsigned m) asm("ref__ZN15ResamplerTimingC2Ejfjffj");
void rt_ctor2_b(void *s, unsigned p, float k, unsigned t, float *b, float ppm,
		unsigned m) asm("_ZN15ResamplerTimingC2EjfjPffj");
void ref_rt_ctor2_b(void *s, unsigned p, float k, unsigned t, float *b,
		    float ppm, unsigned m)
	asm("ref__ZN15ResamplerTimingC2EjfjPffj");
void rt_dtor2(void *s) asm("_ZN15ResamplerTimingD2Ev");
void ref_rt_dtor2(void *s) asm("ref__ZN15ResamplerTimingD2Ev");
void rt_dtor0(void *s) asm("_ZN15ResamplerTimingD0Ev");
void ref_rt_dtor0(void *s) asm("ref__ZN15ResamplerTimingD0Ev");

void vr_ctor_b(void *s, unsigned p, float k, unsigned t, float *b, void *par,
	       float ppm, unsigned m)
	asm("_ZN12V90ResamplerC1EjfjPfP13V90Parametersfj");
void ref_vr_ctor_b(void *s, unsigned p, float k, unsigned t, float *b,
		   void *par, float ppm, unsigned m)
	asm("ref__ZN12V90ResamplerC1EjfjPfP13V90Parametersfj");
void vr_ctor2(void *s, unsigned p, float k, unsigned t, float c, void *par,
	      float ppm, unsigned m)
	asm("_ZN12V90ResamplerC2EjfjfP13V90Parametersfj");
void ref_vr_ctor2(void *s, unsigned p, float k, unsigned t, float c, void *par,
		  float ppm, unsigned m)
	asm("ref__ZN12V90ResamplerC2EjfjfP13V90Parametersfj");
void vr_ctor2_b(void *s, unsigned p, float k, unsigned t, float *b, void *par,
		float ppm, unsigned m)
	asm("_ZN12V90ResamplerC2EjfjPfP13V90Parametersfj");
void ref_vr_ctor2_b(void *s, unsigned p, float k, unsigned t, float *b,
		    void *par, float ppm, unsigned m)
	asm("ref__ZN12V90ResamplerC2EjfjPfP13V90Parametersfj");
void vr_dtor2(void *s) asm("_ZN12V90ResamplerD2Ev");
void ref_vr_dtor2(void *s) asm("ref__ZN12V90ResamplerD2Ev");
void vr_dtor0(void *s) asm("_ZN12V90ResamplerD0Ev");
void ref_vr_dtor0(void *s) asm("ref__ZN12V90ResamplerD0Ev");

void rt_dft(void *s, float v) asm("_ZN15ResamplerTiming13SdHalfBaudDftEf");
void ref_rt_dft(void *s, float v)
	asm("ref__ZN15ResamplerTiming13SdHalfBaudDftEf");
void rt_gain(void *s, float v)
	asm("_ZN15ResamplerTiming21adjustHalfBaudBpfGainEf");
void ref_rt_gain(void *s, float v)
	asm("ref__ZN15ResamplerTiming21adjustHalfBaudBpfGainEf");
void rt_tc(void *s, float v) asm("_ZN15ResamplerTiming16timingCorrectionEf");
void ref_rt_tc(void *s, float v)
	asm("ref__ZN15ResamplerTiming16timingCorrectionEf");
void rt_addphase(void *s, float v) asm("_ZN15ResamplerTiming8addPhaseEf");
void ref_rt_addphase(void *s, float v)
	asm("ref__ZN15ResamplerTiming8addPhaseEf");
void rt_invphase(void *s) asm("_ZN15ResamplerTiming11invertPhaseEv");
void ref_rt_invphase(void *s) asm("ref__ZN15ResamplerTiming11invertPhaseEv");
void rt_resetdft(void *s) asm("_ZN15ResamplerTiming18resetSdHalfBaudDftEv");
void ref_rt_resetdft(void *s)
	asm("ref__ZN15ResamplerTiming18resetSdHalfBaudDftEv");

/* V90Resampler */
void vr_ctor(void *s, unsigned p, float k, unsigned t, float c, void *par,
	     float ppm, unsigned m)
	asm("_ZN12V90ResamplerC1EjfjfP13V90Parametersfj");
void ref_vr_ctor(void *s, unsigned p, float k, unsigned t, float c, void *par,
		 float ppm, unsigned m)
	asm("ref__ZN12V90ResamplerC1EjfjfP13V90Parametersfj");
void vr_dtor(void *s) asm("_ZN12V90ResamplerD1Ev");
void ref_vr_dtor(void *s) asm("ref__ZN12V90ResamplerD1Ev");
void vr_reset(void *s) asm("_ZN12V90Resampler5resetEv");
void ref_vr_reset(void *s) asm("ref__ZN12V90Resampler5resetEv");
void vr_setbll(void *s, int st, unsigned c)
	asm("_ZN12V90Resampler11setBllStateE11V90BllStatej");
void ref_vr_setbll(void *s, int st, unsigned c)
	asm("ref__ZN12V90Resampler11setBllStateE11V90BllStatej");
float vr_mean(void *s) asm("_ZN12V90Resampler20getTimingHistoryMeanEv");
float ref_vr_mean(void *s) asm("ref__ZN12V90Resampler20getTimingHistoryMeanEv");
float vr_std(void *s) asm("_ZN12V90Resampler19getTimingHistoryStdEv");
float ref_vr_std(void *s) asm("ref__ZN12V90Resampler19getTimingHistoryStdEv");
}

/* ------------------------------------------------------------ scratch */

#define OBJ_SLOT	(0xb4 + 64)
#define PARM_SLOT	0x560
#define BANK		256u

static unsigned char obj[2][OBJ_SLOT] __attribute__((aligned(8)));
static unsigned char parm[2][PARM_SLOT] __attribute__((aligned(8)));
static float bank[2][BANK];
static unsigned char scratch[2][OBJ_SLOT] __attribute__((aligned(8)));

static int seeded_params;		/* the parameter block was non-zero  */
static int seeded_history;		/* a history buffer was non-zero     */
static int seeded_coeffs;		/* a designed bank was non-zero      */
static int seen_borrowed_free;		/* the borrow arm of ~Resampler ran  */
static int seen_owned_free;		/* the owning arm did too            */
static int seen_negative_var;		/* Var came out below zero           */

static volatile float fsink;

/*
 * The EXACT bits of a float, forced through memory.
 *
 * This is not fussiness.  A float returned on the x87 stack is 80 bits wide
 * until something stores it, and in `a = f(); b = g();` the compiler must
 * spill `a` across the call to `g` -- rounding it -- while `b` stays in
 * st(0) unrounded.  That asymmetry is in the TEST, not in either side, and it
 * made eleven of these comparisons differ by one in the last place before it
 * was found.  Both sides go through `fsink`, so both round exactly once.
 */
static unsigned
fbits(float f)
{
	unsigned u;

	fsink = f;
	__builtin_memcpy(&u, (const void *)&fsink, sizeof u);
	return u;
}

static unsigned lfsr;

static unsigned
next_word(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xd0000001u);
	return lfsr;
}

/* A float in roughly [-2, 2), never a denormal and never zero. */
static float
next_float(void)
{
	unsigned u = next_word();

	return (float)((int)(u & 0xffffu) - 32768) / 16384.0f + 1.0f / 4096.0f;
}

/*
 * The floating-point fields of the Resampler chain, named as typed spans so
 * the modern tier's rounding-level tolerance reaches them and every other
 * byte stays exact.  The classes are nested, so the offsets hold for all four
 * sizes this file compares (0x48, 0x4c, 0x94, 0xb4); the helper clips a span
 * that runs past the object it is given.
 */
static const struct diff_float_span rs_float_spans[] = {
	{ 0x0c, 1, 8 },		/* double phase                     */
	{ 0x14, 5, 4 },		/* pending[5]                       */
	{ 0x2c, 1, 4 },		/* ppmScale                         */
	{ 0x48, 1, 4 },		/* timingOffset                     */
	{ 0x4c, 2, 4 },		/* bllK1, bllK2                     */
	{ 0x54, 3, 4 },		/* lastHalfBaudErr, unnamed_58, lastPhaseAdj */
	{ 0x68, 5, 4 },		/* bpfSq1, bpfSq2, bpfZ1, bpfZ2, errZ1 */
	{ 0x80, 3, 4 },		/* dftMag, dftRe, dftIm             */
	{ 0x90, 1, 4 },		/* normBPFhBaudB0coef               */
};

/*
 * Compare two objects of `n` bytes, blanking the four-byte words listed in
 * `skip` (terminated by ~0u) because they hold heap addresses or the vptr, and
 * treating the floating-point fields above as floats.  Reports through the
 * harness so a difference names a field.
 */
static void
cmp_obj(const char *what, unsigned n, const unsigned *skip, long input)
{
	unsigned i;

	memcpy(scratch[0], obj[0], n);
	memcpy(scratch[1], obj[1], n);
	for (i = 0; skip[i] != ~0u; i++) {
		memset(scratch[0] + skip[i], 0, 4);
		memset(scratch[1] + skip[i], 0, 4);
	}
	diff_eq_obj_float_(__FILE__, __LINE__, what, "V90Resampler",
			   scratch[0], scratch[1], n, rs_float_spans,
			   sizeof rs_float_spans / sizeof rs_float_spans[0],
			   input);
}

static const unsigned skip_rs[] = { 0x00, 0x04, 0x08, ~0u };
static const unsigned skip_vr[] = { 0x00, 0x04, 0x08, 0xa0, 0xa4, ~0u };

/* Read a pointer field out of one side's object. */
static float *
ptr_at(int side, unsigned off)
{
	float *p;

	memcpy(&p, obj[side] + off, sizeof p);
	return p;
}

static unsigned
u32_at(int side, unsigned off)
{
	unsigned v;

	memcpy(&v, obj[side] + off, sizeof v);
	return v;
}

/*
 * One entry of a constructed object's vtable, reached through its own vptr.
 * The two sides point at two different tables -- ours and `ref__ZTV...` --
 * so calling the same slot on each and comparing what it did is the only way
 * to test the tables that does not privilege either one.
 */
typedef void (*slot_void)(void *);
typedef void (*slot_float)(void *, float);
typedef void (*slot_uint)(void *, unsigned);

static void *
slot(int side, unsigned k)
{
	void **vptr;

	memcpy(&vptr, obj[side], sizeof vptr);
	return vptr[k];
}

/*
 * Compare the two sides' heap buffers.  `n` comes out of the object, so a
 * disagreement about the LENGTH shows up in cmp_obj rather than here.
 */
static void
cmp_buf(const char *what, unsigned off, unsigned len, int *sawNonZero,
	long input)
{
	const float *a = ptr_at(0, off);
	const float *b = ptr_at(1, off);
	unsigned i;

	if (!a || !b) {
		diff_eq_int("%s: one side allocated and the other did not",
			    a != 0, b != 0, input);
		return;
	}
	for (i = 0; i < len; i++) {
		if (sawNonZero && a[i] != 0)
			*sawNonZero = 1;
		diff_eq_float(what, a[i], b[i], (long)i);
	}
}

/* ------------------------------------------------------- the parameters */

/*
 * A seeded V90Parameters.  The BLL block at +0x088..+0x0f4 must be sixteen
 * DISTINCT pairs, or `setBllState` picking the wrong one would compare equal.
 */
static void
fill_params(unsigned trial)
{
	unsigned i;

	lfsr = 0x2f6f2ffdu + 0x9e3779b9u * trial;
	for (i = 0; i < PARM_SLOT; i += 4) {
		float f = next_float() * (float)(i + 1);
		memcpy(parm[0] + i, &f, 4);
		memcpy(parm[1] + i, &f, 4);
		if (f != 0)
			seeded_params = 1;
	}

	/* TIMING_HISTORY_EVALUATION_{ENABLED,BUFFER_LENGTH,PERIOD}. */
	{
		int on = 1, len = 12, per = 5;
		memcpy(parm[0] + 0x160, &on, 4);  memcpy(parm[1] + 0x160, &on, 4);
		memcpy(parm[0] + 0x164, &len, 4); memcpy(parm[1] + 0x164, &len, 4);
		memcpy(parm[0] + 0x168, &per, 4); memcpy(parm[1] + 0x168, &per, 4);
	}
}

/* ---------------------------------------------------------------- cases */

/*
 * The designing constructor.  `taps` is deliberately not always a multiple of
 * four, because the object rounds it down and the adopting constructor does
 * not; `minHistory` straddles `10 * taps` so both arms of the historyLen
 * choice are taken.
 */
struct shape {
	unsigned	phases;
	float		ppmScale;
	unsigned	taps;
	float		cutoff;
	unsigned	minHistory;
};

static const struct shape shapes[] = {
	{  3, 1.0f,	 8,  0.45f,	  0 },	/* 10*taps wins            */
	{  3, 1.0f,	 8,  0.45f,	500 },	/* minHistory wins         */
	{  1, 2.5f,	 4,  0.25f,	 40 },	/* exactly 10*taps         */
	{  4, 0.5f,	 9,  0.40f,	  7 },	/* taps rounds 9 -> 8      */
	{  5, 1.25f,	10,  0.30f,	  1 },	/* taps rounds 10 -> 8     */
	{  2, 7.0f,	16,  0.10f,	300 },
	{  6, 0.125f,	12,  0.49f,	  0 },
	{  1, 1.0f,	 0,  0.20f,	 16 },	/* taps 0: no bank at all  */
};

#define NSHAPE	((int)(sizeof shapes / sizeof shapes[0]))

static int
case_designing_ctor(void)
{
	int rc = 0;

	int i;

	diff_begin("Resampler(phases, ppmScale, taps, cutoff, minHistory)");
	for (i = 0; i < NSHAPE; i++) {
		const struct shape *s = &shapes[i];
		int a0, a1, base;

		memset(obj[0], 0xa5, sizeof obj[0]);
		memset(obj[1], 0xa5, sizeof obj[1]);

		base = harness_alloc.allocs;
		rs_ctor(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			s->minHistory);
		a0 = harness_alloc.allocs - base;
		ref_rs_ctor(obj[1], s->phases, s->ppmScale, s->taps, s->cutoff,
			    s->minHistory);
		a1 = harness_alloc.allocs - base - a0;

		diff_eq_int("shape %ld: allocations", a0, a1, i);
		cmp_obj("Resampler after construction", 0x48, skip_rs, i);

		/*
		 * The bank is the whole point of this constructor: it is a
		 * transposed windowed-sinc and it is NOT all zeros, which is
		 * what `seeded_coeffs` records.
		 */
		if (u32_at(0, 0x34) && u32_at(0, 0x30))
			cmp_buf("shape %ld: coeffs[]", 0x04,
				u32_at(0, 0x34) * u32_at(0, 0x30),
				&seeded_coeffs, i);
		cmp_buf("shape %ld: history[]", 0x08, u32_at(0, 0x38), 0, i);

		rs_dtor(obj[0]);
		ref_rs_dtor(obj[1]);
	}
	rc |= diff_end();
	return rc;
}

static int
case_adopting_ctor(void)
{
	int rc = 0;

	int i;

	diff_begin("Resampler(phases, ppmScale, taps, float *, minHistory)");
	for (i = 0; i < NSHAPE; i++) {
		const struct shape *s = &shapes[i];
		unsigned j;
		int f0, f1, base, b0, b1, badbase;

		lfsr = 0x13572468u + (unsigned)i;
		for (j = 0; j < BANK; j++)
			bank[0][j] = bank[1][j] = next_float();

		memset(obj[0], 0x5a, sizeof obj[0]);
		memset(obj[1], 0x5a, sizeof obj[1]);

		rs_ctor_b(obj[0], s->phases, s->ppmScale, s->taps, bank[0],
			  s->minHistory);
		ref_rs_ctor_b(obj[1], s->phases, s->ppmScale, s->taps, bank[1],
			      s->minHistory);

		/*
		 * `coeffsBorrowed` is what this case exists for: without it
		 * the destructor's skip-the-free arm is never entered.
		 */
		diff_eq_int("shape %ld: coeffsBorrowed", u32_at(0, 0x44),
			    u32_at(1, 0x44), i);
		diff_eq_int("shape %ld: coeffsBorrowed is 1", u32_at(0, 0x44),
			    1, i);
		cmp_obj("Resampler after adopting construction", 0x48,
			skip_rs, i);
		cmp_buf("shape %ld: history[]", 0x08, u32_at(0, 0x38), 0, i);

		/*
		 * Exactly one block per side is released -- the history --
		 * and the caller's bank is untouched.
		 */
		base = harness_alloc.frees;
		badbase = harness_alloc.bad_free;
		rs_dtor(obj[0]);
		f0 = harness_alloc.frees - base;
		b0 = harness_alloc.bad_free - badbase;
		ref_rs_dtor(obj[1]);
		f1 = harness_alloc.frees - base - f0;
		b1 = harness_alloc.bad_free - badbase - b0;
		diff_eq_int("shape %ld: frees from ~Resampler", f0, f1, i);
		/*
		 * THE `frees` COUNT CANNOT SEE THIS ONE.  `bank` is a static
		 * array, so a destructor that frees it hands the allocator a
		 * pointer it never issued; the harness counts that as
		 * `bad_free` and deliberately does NOT pass it on, which
		 * leaves `frees` identical either way.  Watching `bad_free`
		 * is what makes "the borrowed bank is not freed" a claim with
		 * a test behind it.
		 */
		diff_eq_int("shape %ld: bad frees from ~Resampler", b0, b1, i);
		diff_eq_int("shape %ld: nothing unknown was freed", b0, 0, i);
		diff_eq_int("shape %ld: the borrowed bank is not freed", f0,
			    u32_at(0, 0x38) ? 1 : 0, i);
		if (f0 == 1)
			seen_borrowed_free = 1;
		for (j = 0; j < BANK; j++)
			diff_eq_int("shape %ld: bank survives",
				    (long)fbits(bank[0][j]),
				    (long)fbits(bank[1][j]), j);
	}
	rc |= diff_end();
	return rc;
}

/*
 * `reset` has to be shown clearing something, so the object is dirtied first
 * -- identically on both sides -- and the history is filled with a pattern
 * that is not zero.  Two zeroed buffers compare equal whatever reset does.
 */
static int
case_reset(void)
{
	int rc = 0;

	int i;

	diff_begin("Resampler::reset");
	for (i = 0; i < NSHAPE; i++) {
		const struct shape *s = &shapes[i];
		unsigned j, n;
		float *h0, *h1;

		rs_ctor(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			s->minHistory);
		ref_rs_ctor(obj[1], s->phases, s->ppmScale, s->taps, s->cutoff,
			    s->minHistory);

		n = u32_at(0, 0x38);
		h0 = ptr_at(0, 0x08);
		h1 = ptr_at(1, 0x08);
		lfsr = 0x0badf00du + (unsigned)i;
		for (j = 0; j < n; j++) {
			float f = next_float();
			h0[j] = h1[j] = f;
			if (f != 0)
				seeded_history = 1;
		}

		/* Dirty every field reset is meant to clear. */
		{
			double ph = 3.25 + i;
			unsigned k = 4u;
			float p[5];

			memcpy(obj[0] + 0x0c, &ph, 8);
			memcpy(obj[1] + 0x0c, &ph, 8);
			memcpy(obj[0] + 0x28, &k, 4);
			memcpy(obj[1] + 0x28, &k, 4);
			memcpy(obj[0] + 0x3c, &k, 4);
			memcpy(obj[1] + 0x3c, &k, 4);
			memcpy(obj[0] + 0x40, &k, 4);
			memcpy(obj[1] + 0x40, &k, 4);
			for (j = 0; j < 5; j++)
				p[j] = next_float();
			memcpy(obj[0] + 0x14, p, sizeof p);
			memcpy(obj[1] + 0x14, p, sizeof p);
		}

		rs_reset(obj[0]);
		ref_rs_reset(obj[1]);
		cmp_obj("Resampler after reset", 0x48, skip_rs, i);
		cmp_buf("shape %ld: history[] after reset", 0x08, n, 0, i);
		diff_eq_int("shape %ld: reset put historyIndex at taps",
			    u32_at(0, 0x3c), u32_at(0, 0x34), i);

		rs_dtor(obj[0]);
		ref_rs_dtor(obj[1]);
	}
	rc |= diff_end();
	return rc;
}

static int
case_phase(void)
{
	int rc = 0;

	int i, k;

	diff_begin("Resampler::setNormalizedPhase / getNormalizedPhase");
	for (i = 0; i < NSHAPE; i++) {
		const struct shape *s = &shapes[i];

		rs_ctor(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			s->minHistory);
		ref_rs_ctor(obj[1], s->phases, s->ppmScale, s->taps, s->cutoff,
			    s->minHistory);

		for (k = -4; k < 68; k++) {
			/* -0.0625 .. 1.0, so both rejection arms are taken. */
			float p = (float)k / 64.0f;
			unsigned d0w[2], d1w[2];
			float g0, g1;

			rs_setnp(obj[0], p);
			ref_rs_setnp(obj[1], p);
			memcpy(d0w, obj[0] + 0x0c, 8);
			memcpy(d1w, obj[1] + 0x0c, 8);
			diff_eq_int("setNormalizedPhase(%ld/64) low",
				    (long)(unsigned)d0w[0], (long)(unsigned)d1w[0],
				    k);
			diff_eq_int("setNormalizedPhase(%ld/64) high",
				    (long)(unsigned)d0w[1], (long)(unsigned)d1w[1],
				    k);

			g0 = rs_getnp(obj[0]);
			g1 = ref_rs_getnp(obj[1]);
			diff_eq_int("getNormalizedPhase after %ld/64",
				    (long)fbits(g0), (long)fbits(g1), k);
		}
		{
			unsigned nan_bits = 0x7fc00000u;
			unsigned d0w[2], d1w[2];
			float p;

			memcpy(&p, &nan_bits, sizeof p);
			rs_setnp(obj[0], p);
			ref_rs_setnp(obj[1], p);
			memcpy(d0w, obj[0] + 0x0c, 8);
			memcpy(d1w, obj[1] + 0x0c, 8);
			diff_eq_int("shape %ld: a NaN phase is rejected (low)",
				    (long)d0w[0], (long)d1w[0], i);
			diff_eq_int("shape %ld: a NaN phase is rejected (high)",
				    (long)d0w[1], (long)d1w[1], i);
			diff_eq_int("shape %ld: rejected NaN stores zero (low)",
				    (long)d1w[0], 0, i);
			diff_eq_int("shape %ld: rejected NaN stores zero (high)",
				    (long)d1w[1], 0, i);
		}

		rs_dtor(obj[0]);
		ref_rs_dtor(obj[1]);
	}
	rc |= diff_end();
	return rc;
}

static int
case_history_helpers(void)
{
	int rc = 0;

	int i;

	diff_begin("Resampler::copyHistoryTail / resetHistoryIndex");
	for (i = 0; i < NSHAPE; i++) {
		const struct shape *s = &shapes[i];
		unsigned j, n;
		float *h0, *h1;

		rs_ctor(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			s->minHistory);
		ref_rs_ctor(obj[1], s->phases, s->ppmScale, s->taps, s->cutoff,
			    s->minHistory);

		n = u32_at(0, 0x38);
		h0 = ptr_at(0, 0x08);
		h1 = ptr_at(1, 0x08);
		lfsr = 0xfeedbeefu + (unsigned)i;
		for (j = 0; j < n; j++) {
			float f = next_float();
			h0[j] = h1[j] = f;
			if (f != 0)
				seeded_history = 1;
		}

		rs_copytail(obj[0]);
		ref_rs_copytail(obj[1]);
		cmp_buf("shape %ld: history[] after copyHistoryTail", 0x08, n,
			0, i);

		{
			unsigned k = 0xdeadu;

			memcpy(obj[0] + 0x3c, &k, 4);
			memcpy(obj[1] + 0x3c, &k, 4);
		}
		rs_resetidx(obj[0]);
		ref_rs_resetidx(obj[1]);
		diff_eq_int("shape %ld: historyIndex after reset",
			    u32_at(0, 0x3c), u32_at(1, 0x3c), i);
		diff_eq_int("shape %ld: historyIndex is taps",
			    u32_at(0, 0x3c), u32_at(0, 0x34), i);

		rs_dtor(obj[0]);
		ref_rs_dtor(obj[1]);
	}
	rc |= diff_end();
	return rc;
}

/* ------------------------------------------------- ResamplerTimingOffset */

static int
case_rto(void)
{
	int rc = 0;

	int i, k;

	diff_begin("ResamplerTimingOffset");
	for (i = 0; i < NSHAPE; i++) {
		const struct shape *s = &shapes[i];
		float ppm = 12.5f * (float)(i + 1) - 40.0f;

		memset(obj[0], 0x33, sizeof obj[0]);
		memset(obj[1], 0x33, sizeof obj[1]);
		rto_ctor(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			 ppm, s->minHistory);
		ref_rto_ctor(obj[1], s->phases, s->ppmScale, s->taps,
			     s->cutoff, ppm, s->minHistory);
		cmp_obj("ResamplerTimingOffset after construction", 0x4c,
			skip_rs, i);

		for (k = -8; k <= 8; k++) {
			float p = (float)k * 6.25f;
			float g0, g1;

			rto_set(obj[0], p);
			ref_rto_set(obj[1], p);
			cmp_obj("after setTimingOffset", 0x4c, skip_rs, k);
			g0 = rto_getppm(obj[0]);
			g1 = ref_rto_getppm(obj[1]);
			diff_eq_int("getTimingOffsetPPM after %ld*6.25",
				    (long)fbits(g0), (long)fbits(g1), k);
		}

		/*
		 * `reset` must clear timingOffset as well as the base's
		 * fields, so it is set to something non-zero first.
		 */
		rto_set(obj[0], 137.0f);
		ref_rto_set(obj[1], 137.0f);
		rto_reset(obj[0]);
		ref_rto_reset(obj[1]);
		cmp_obj("ResamplerTimingOffset after reset", 0x4c, skip_rs, i);
		diff_eq_int("shape %ld: reset cleared timingOffset",
			    u32_at(0, 0x48), 0, i);

		rto_dtor(obj[0]);
		ref_rto_dtor(obj[1]);
	}
	rc |= diff_end();
	return rc;
}

/* ------------------------------------------------------- ResamplerTiming */

static int
case_rt(void)
{
	int rc = 0;

	int i;

	diff_begin("ResamplerTiming");
	for (i = 0; i < NSHAPE; i++) {
		const struct shape *s = &shapes[i];
		float ppm = -18.75f + 9.5f * (float)i;
		unsigned j;

		memset(obj[0], 0xc3, sizeof obj[0]);
		memset(obj[1], 0xc3, sizeof obj[1]);
		rt_ctor(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			ppm, s->minHistory);
		ref_rt_ctor(obj[1], s->phases, s->ppmScale, s->taps, s->cutoff,
			    ppm, s->minHistory);
		cmp_obj("ResamplerTiming after construction", 0x94, skip_rs, i);

		/* Dirty everything reset(unsigned) is meant to clear. */
		lfsr = 0x5eed0000u + (unsigned)i;
		for (j = 0x4c; j < 0x94; j += 4) {
			unsigned w = next_word();

			memcpy(obj[0] + j, &w, 4);
			memcpy(obj[1] + j, &w, 4);
		}
		rt_reset(obj[0], (unsigned)i);
		ref_rt_reset(obj[1], (unsigned)i);
		cmp_obj("ResamplerTiming after reset", 0x94, skip_rs, i);
		diff_eq_int("shape %ld: reset left bpfGain at 0x3d230fd0",
			    u32_at(0, 0x90), 0x3d230fd0, i);

		/*
		 * `reset(unsigned)` IGNORES its argument.  Two different
		 * arguments must leave the same object -- and this is a
		 * claim about our side and the blob's alike.
		 */
		rt_reset(obj[0], 0u);
		ref_rt_reset(obj[1], 0xffffffffu);
		cmp_obj("reset(0) == reset(0xffffffff)", 0x94, skip_rs, i);

		/* resetSdHalfBaudDft touches +0x7c..+0x88 and nothing else. */
		for (j = 0x4c; j < 0x94; j += 4) {
			unsigned w = next_word();

			memcpy(obj[0] + j, &w, 4);
			memcpy(obj[1] + j, &w, 4);
		}
		rt_resetdft(obj[0]);
		ref_rt_resetdft(obj[1]);
		cmp_obj("after resetSdHalfBaudDft", 0x94, skip_rs, i);
		diff_eq_int("shape %ld: resetSdHalfBaudDft left bpfGain alone",
			    u32_at(0, 0x90) != 0, 1, i);

		rt_dtor(obj[0]);
		ref_rt_dtor(obj[1]);
	}
	rc |= diff_end();
	return rc;
}

/* ---------------------------------------------------------- V90Resampler */

static int
case_v90(void)
{
	int rc = 0;

	int i;

	diff_begin("V90Resampler");
	for (i = 0; i < NSHAPE; i++) {
		const struct shape *s = &shapes[i];
		float ppm = 3.5f * (float)i - 7.0f;
		int st;
		int f0, f1, base;

		fill_params((unsigned)i);
		memset(obj[0], 0x77, sizeof obj[0]);
		memset(obj[1], 0x77, sizeof obj[1]);
		vr_ctor(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			parm[0], ppm, s->minHistory);
		ref_vr_ctor(obj[1], s->phases, s->ppmScale, s->taps, s->cutoff,
			    parm[1], ppm, s->minHistory);

		cmp_obj("V90Resampler after construction", 0xb4, skip_vr, i);
		cmp_buf("shape %ld: timingHistory[]", 0xa4, u32_at(0, 0xa8),
			0, i);

		/*
		 * All sixteen states, then each again (the early return),
		 * then one out of range.
		 */
		for (st = 0; st < 18; st++) {
			int state = (st < 16) ? st : (st == 16 ? 15 : 99);

			/*
			 * `stateSamples` is zero out of `reset` and only
			 * `resample` moves it, so without this the store that
			 * restarts it is invisible.
			 */
			{
				unsigned n = 0x1234u + (unsigned)st;

				memcpy(obj[0] + 0x98, &n, 4);
				memcpy(obj[1] + 0x98, &n, 4);
			}
			dsplib_debug_capture_reset();
			vr_setbll(obj[0], state, (unsigned)st + 1u);
			ref_vr_setbll(obj[1], state, (unsigned)st + 1u);
			cmp_obj("after setBllState", 0xb4, skip_vr, state);
			diff_eq_int("setBllState(%ld) diagnostics",
				    (long)dsplib_debug_capture_lines(0),
				    (long)dsplib_debug_capture_lines(1),
				    state);
			diff_eq_int("setBllState(%ld) transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)),
				    0, state);
		}

		/*
		 * FROZEN and SECOND_ORDER_FROZEN zero one gain each and are
		 * defined by WHICH.  Reached in numerical order they are
		 * always entered from a state that has already zeroed both --
		 * the constructor's -- so the difference is invisible.  Each
		 * is therefore entered again from a state that sets real
		 * gains, and the gains are checked to have been non-zero
		 * first.
		 */
		for (st = 0; st < 2; st++) {
			vr_setbll(obj[0], V90_BLL_STEADY_STATE, 1);
			ref_vr_setbll(obj[1], V90_BLL_STEADY_STATE, 1);
			diff_eq_int("both gains are non-zero before %ld",
				    (u32_at(0, 0x4c) != 0)
				    && (u32_at(0, 0x50) != 0), 1, st);
			vr_setbll(obj[0], (V90BllState)st, 1);
			ref_vr_setbll(obj[1], (V90BllState)st, 1);
			cmp_obj("entering FROZEN from a live state", 0xb4,
				skip_vr, st);
			diff_eq_int("state %ld left K1 as it should",
				    u32_at(0, 0x4c) == 0, st == 0, st);
			diff_eq_int("state %ld cleared K2", u32_at(0, 0x50),
				    0, st);
		}

		/*
		 * The timing-history ring is filled by hand, because
		 * `Resampler::resample` -- which is what fills it in service
		 * -- is not in this batch.  mean and Var over a ring of zeros
		 * are both zero and would prove nothing.
		 */
		{
			float *h0 = ptr_at(0, 0xa4);
			float *h1 = ptr_at(1, 0xa4);
			unsigned n = u32_at(0, 0xa8);
			unsigned j;

			lfsr = 0x24681357u + (unsigned)i;
			for (j = 0; j < n; j++) {
				float f = next_float();
				h0[j] = h1[j] = f;
			}
			{
				float m0 = vr_mean(obj[0]);
				float m1 = ref_vr_mean(obj[1]);
				float s0 = vr_std(obj[0]);
				float s1 = ref_vr_std(obj[1]);

				diff_eq_int("shape %ld: getTimingHistoryMean",
					    (long)fbits(m0), (long)fbits(m1),
					    i);
				diff_eq_int("shape %ld: getTimingHistoryStd",
					    (long)fbits(s0), (long)fbits(s1),
					    i);
			}
		}

		/*
		 * `Var` is `E[x^2] - E[x]^2` computed as two separate rounded
		 * quantities, so over a nearly constant ring it can come out
		 * NEGATIVE -- and that is the only input that separates
		 * `sqrt(|v|)`, which the object computes, from `sqrt(v)`.
		 * A spread of magnitudes is swept until one of them does it;
		 * `seen_negative_var` records that one did.
		 */
		{
			static const float flat[] = {
				1.0f, 3.0f, 1e3f, 1e4f, 1e5f, 1e6f, 1e7f,
				12345.678f, 0.1f, 7.7f
			};
			float *h0 = ptr_at(0, 0xa4);
			float *h1 = ptr_at(1, 0xa4);
			unsigned n = u32_at(0, 0xa8);
			unsigned c, j;

			for (c = 0; c < sizeof flat / sizeof flat[0]; c++) {
				float d0, d1;

				for (j = 0; j < n; j++)
					h0[j] = h1[j] = flat[c];
				d0 = vr_std(obj[0]);
				d1 = ref_vr_std(obj[1]);
				diff_eq_int("shape %ld: Std over a flat ring",
					    (long)fbits(d0), (long)fbits(d1),
					    i * 100 + (long)c);
				if (fbits(d0) != 0 && fbits(d0) != 0x80000000u)
					seen_negative_var = 1;
			}
		}

		/* reset again, from a thoroughly dirty object. */
		vr_reset(obj[0]);
		ref_vr_reset(obj[1]);
		cmp_obj("V90Resampler after reset", 0xb4, skip_vr, i);

		/* Two blocks each: the history and the timing history. */
		base = harness_alloc.frees;
		vr_dtor(obj[0]);
		f0 = harness_alloc.frees - base;
		ref_vr_dtor(obj[1]);
		f1 = harness_alloc.frees - base - f0;
		diff_eq_int("shape %ld: frees from ~V90Resampler", f0, f1, i);
		if (f0 >= 2)
			seen_owned_free = 1;
	}
	rc |= diff_end();
	return rc;
}


/* ------------------------------------------------------------- resample */

/*
 * `Resampler::resample`, and the four things that make a run of it non-vacuous.
 *
 *   - A ZEROED HISTORY MAKES THE WHOLE FILTER INVISIBLE.  Straight out of
 *     `reset()` the ring is zeros, every output is zero whatever the
 *     coefficients are, and the comparison proves nothing.  Every
 *     configuration below therefore pushes samples in and checks, at the end
 *     of the run, that a non-zero output was actually seen.
 *   - THE LOOK-AHEAD ARM needs `(int)phase + 1 == phases`, which a fresh
 *     object never reaches on its first output.  `setNormalizedPhase` is used
 *     to put it there deliberately, and `seen_lookahead` records that it was.
 *   - THE RING WRAP needs `historyLen - taps` samples consumed, so the
 *     configurations are small enough to reach it.
 *   - THE RATIO decides how many outputs come out per input, and 1:1 is the
 *     boring case: `ppmScale` is swept from a quarter of `phases` to four
 *     times it, which also drives the `credit > avail` early exit and the
 *     multi-iteration phase-unwrap loop.
 *
 * `in[]` IS PADDED BY ONE and the pad initialised, because the object reads
 * `in[n]` on several paths.  That is the original's bug, reproduced
 * deliberately; the pad is what stops the test disagreeing about
 * uninitialised memory rather than about the resampler.
 */
#define RS_IN	16u
#define RS_OUT	512u

static float rin[RS_IN + 1];
static float rout[2][RS_OUT];

static int seen_output;			/* a non-zero output sample     */
static int seen_lookahead;		/* the ph+1 == phases arm       */
static int seen_wrap;			/* copyHistoryTail fired        */
static int seen_multi;			/* more outputs than inputs     */
static int seen_carry;			/* pendingCount left non-zero   */

struct rshape {
	unsigned	phases;
	unsigned	taps;
	unsigned	minHistory;
	float		ratio;		/* ppmScale, in units of phases */
	unsigned	nPerCall;
	float		startPhase;
};

static const struct rshape rshapes[] = {
	{ 8,  8, 0,   1.00f,  8, -1.0f },	/* 1:1                      */
	{ 8,  8, 0,   0.25f,  8, -1.0f },	/* 4 outputs per input      */
	{ 8,  8, 0,   2.50f,  8, -1.0f },	/* credit 2 or 3, early exit */
	{ 8,  8, 0,   0.75f,  1, -1.0f },	/* one sample per call      */
	{ 4,  4, 40,  1.00f,  6, -1.0f },	/* short ring: wraps early  */
	{ 16, 8, 0,   1.00f,  8, 0.99f },	/* starts in the look-ahead */
	{ 32, 8, 0,   4.00f,  3, 0.50f },	/* credit > avail every pass */
	{ 3,  4, 0,   0.50f,  5, 0.60f }
};

#define NRSHAPE ((int)(sizeof rshapes / sizeof rshapes[0]))

static int
case_resample(void)
{
	int rc = 0;

	int i;

	diff_begin("Resampler::resample");
	for (i = 0; i < NRSHAPE; i++) {
		const struct rshape *r = &rshapes[i];
		float scale = r->ratio * (float)r->phases;
		unsigned call, j;

		rs_ctor(obj[0], r->phases, scale, r->taps, 0.45f,
			r->minHistory);
		ref_rs_ctor(obj[1], r->phases, scale, r->taps, 0.45f,
			    r->minHistory);
		if (r->startPhase >= 0.0f) {
			rs_setnp(obj[0], r->startPhase);
			ref_rs_setnp(obj[1], r->startPhase);
			seen_lookahead = 1;
		}

		lfsr = 0x0c0ffee0u + (unsigned)i;
		for (call = 0; call < 24; call++) {
			unsigned n0 = 0xdeadu, n1 = 0xbeefu;
			unsigned idx0, idx1;

			for (j = 0; j < RS_IN + 1; j++)
				rin[j] = next_float();
			memset(rout[0], 0, sizeof rout[0]);
			memset(rout[1], 0, sizeof rout[1]);

			idx0 = u32_at(0, 0x3c);
			rs_resample(obj[0], rin, r->nPerCall, rout[0], &n0);
			ref_rs_resample(obj[1], rin, r->nPerCall, rout[1],
					&n1);
			idx1 = u32_at(0, 0x3c);
			if (idx1 < idx0)
				seen_wrap = 1;

			diff_eq_int("shape %ld: nOut", (long)n0, (long)n1,
				    i * 100 + (long)call);
			if (n0 > r->nPerCall)
				seen_multi = 1;
			if (n0 > RS_OUT)
				n0 = RS_OUT;
			for (j = 0; j < n0; j++) {
				if (rout[0][j] != 0)
					seen_output = 1;
				diff_eq_float("shape %ld: out[]", rout[0][j],
					      rout[1][j],
					      i * 10000 + (long)(call * 100 + j));
			}
			cmp_obj("Resampler after resample", 0x48, skip_rs,
				i * 100 + (long)call);
			cmp_buf("shape %ld: history[] after resample", 0x08,
				u32_at(0, 0x38), 0, i * 100 + (long)call);
			if (u32_at(0, 0x28) != 0)
				seen_carry = 1;
		}

		rs_dtor(obj[0]);
		ref_rs_dtor(obj[1]);
	}
	rc |= diff_end();
	return rc;
}

/*
 * `V90Resampler::resample` on top of it: the same drive, plus the timing
 * history ring, which only advances when TIMING_HISTORY_EVALUATION_ENABLED is
 * set and only every TIMING_HISTORY_EVALUATION_PERIOD samples.  Half the
 * configurations turn the evaluation off, so the arm that skips the ring
 * entirely is covered too.
 */
static int
case_v90_resample(void)
{
	int rc = 0;

	int i;

	diff_begin("V90Resampler::resample");
	for (i = 0; i < NRSHAPE; i++) {
		const struct rshape *r = &rshapes[i];
		float scale = r->ratio * (float)r->phases;
		unsigned call, j;

		fill_params((unsigned)i + 128u);
		if (i & 1) {
			int off = 0;		/* the disabled arm too */

			memcpy(parm[0] + 0x160, &off, 4);
			memcpy(parm[1] + 0x160, &off, 4);
		}
		vr_ctor(obj[0], r->phases, scale, r->taps, 0.45f, parm[0],
			11.0f, r->minHistory);
		ref_vr_ctor(obj[1], r->phases, scale, r->taps, 0.45f, parm[1],
			    11.0f, r->minHistory);

		lfsr = 0x51ced00du + (unsigned)i;
		for (call = 0; call < 24; call++) {
			unsigned n0 = 0xdeadu, n1 = 0xbeefu;

			for (j = 0; j < RS_IN + 1; j++)
				rin[j] = next_float();
			memset(rout[0], 0, sizeof rout[0]);
			memset(rout[1], 0, sizeof rout[1]);

			vr_resample(obj[0], rin, r->nPerCall, rout[0], &n0);
			ref_vr_resample(obj[1], rin, r->nPerCall, rout[1],
					&n1);

			diff_eq_int("shape %ld: nOut", (long)n0, (long)n1,
				    i * 100 + (long)call);
			if (n0 > RS_OUT)
				n0 = RS_OUT;
			for (j = 0; j < n0; j++)
				diff_eq_float("shape %ld: out[]", rout[0][j],
					      rout[1][j],
					      i * 10000 + (long)(call * 100 + j));
			cmp_obj("V90Resampler after resample", 0xb4, skip_vr,
				i * 100 + (long)call);
			cmp_buf("shape %ld: timingHistory[]", 0xa4,
				u32_at(0, 0xa8), 0, i * 100 + (long)call);
		}

		vr_dtor(obj[0]);
		ref_vr_dtor(obj[1]);
	}
	rc |= diff_end();
	return rc;
}



/* ------------------------------- the C2, D2, D0 and adopting variants */

static int seen_d0_freed;		/* a deleting destructor released `this` */

/*
 * Sixteen entry points the cases above never name.
 *
 * THE ADOPTING OVERLOADS matter for a reason beyond coverage: they are the
 * only way `coeffsBorrowed` is ever 1 on a `ResamplerTiming` or a
 * `V90Resampler`, and therefore the only way the destructor chain's
 * skip-the-free arm runs below `Resampler`.
 *
 * THE DELETING DESTRUCTORS (`D0`) matter more.  They are where `operator
 * delete` ends up, and the whole reason that operator is a member calling
 * `sysdep_free` is that the default one would leave an undefined `_ZdlPvj`
 * and break every test binary in the tree.  Nothing else here calls a D0, so
 * without this case the claim is untested; `this` is allocated from
 * `sysdep_malloc` so the free is a real one and the allocation log can see
 * it.
 *
 * `C2` is the base-object constructor and `D2` the base-object destructor.
 * In our build each is an alias of its `C1`/`D1`, and in the blob each is a
 * separate body -- so calling them by name compares two things that are not
 * the same code, which is exactly the comparison worth making.
 */
static int
case_variants(void)
{
	int rc = 0;

	int i;

	diff_begin("the C2, D2, D0 and adopting variants");
	for (i = 0; i < NSHAPE; i++) {
		const struct shape *s = &shapes[i];
		float ppm = 6.25f * (float)i - 12.5f;
		unsigned j;
		int f0, f1, base;

		lfsr = 0x0f1e2d3cu + (unsigned)i;
		for (j = 0; j < BANK; j++)
			bank[0][j] = bank[1][j] = next_float();
		fill_params((unsigned)i + 192u);

		/* --- Resampler: C2, then the base-object destructor. */
		rs_ctor2(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			 s->minHistory);
		ref_rs_ctor2(obj[1], s->phases, s->ppmScale, s->taps,
			     s->cutoff, s->minHistory);
		cmp_obj("Resampler C2", 0x48, skip_rs, i);
		{
			double ph = 2.5;

			memcpy(obj[0] + 0x0c, &ph, 8);
			memcpy(obj[1] + 0x0c, &ph, 8);
			rs_tc(obj[0], 7.5f);
			ref_rs_tc(obj[1], 7.5f);
			cmp_obj("Resampler::timingCorrection is empty", 0x48,
				skip_rs, i);
			diff_eq_int("shape %ld: the base hook stored nothing",
				    memcmp(obj[0] + 0x0c, &ph, 8), 0, i);
		}
		rs_dtor2(obj[0]);
		ref_rs_dtor2(obj[1]);

		rs_ctor2_b(obj[0], s->phases, s->ppmScale, s->taps, bank[0],
			   s->minHistory);
		ref_rs_ctor2_b(obj[1], s->phases, s->ppmScale, s->taps,
			       bank[1], s->minHistory);
		cmp_obj("Resampler C2 adopting", 0x48, skip_rs, i);
		rs_dtor2(obj[0]);
		ref_rs_dtor2(obj[1]);

		/* --- ResamplerTimingOffset: C2, adopting C1/C2, D2, hook. */
		rto_ctor2(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			  ppm, s->minHistory);
		ref_rto_ctor2(obj[1], s->phases, s->ppmScale, s->taps,
			      s->cutoff, ppm, s->minHistory);
		cmp_obj("ResamplerTimingOffset C2", 0x4c, skip_rs, i);
		{
			double ph = -3.75;

			memcpy(obj[0] + 0x0c, &ph, 8);
			memcpy(obj[1] + 0x0c, &ph, 8);
			rto_tc(obj[0], 1234.0f);
			ref_rto_tc(obj[1], 1234.0f);
			cmp_obj("RTO::timingCorrection", 0x4c, skip_rs, i);
		}
		rto_dtor2(obj[0]);
		ref_rto_dtor2(obj[1]);

		rto_ctor_b(obj[0], s->phases, s->ppmScale, s->taps, bank[0],
			   ppm, s->minHistory);
		ref_rto_ctor_b(obj[1], s->phases, s->ppmScale, s->taps,
			       bank[1], ppm, s->minHistory);
		cmp_obj("ResamplerTimingOffset C1 adopting", 0x4c, skip_rs, i);
		rto_dtor2(obj[0]);
		ref_rto_dtor2(obj[1]);

		rto_ctor2_b(obj[0], s->phases, s->ppmScale, s->taps, bank[0],
			    ppm, s->minHistory);
		ref_rto_ctor2_b(obj[1], s->phases, s->ppmScale, s->taps,
				bank[1], ppm, s->minHistory);
		cmp_obj("ResamplerTimingOffset C2 adopting", 0x4c, skip_rs, i);
		rto_dtor2(obj[0]);
		ref_rto_dtor2(obj[1]);

		/* --- ResamplerTiming: C2, adopting C1/C2, D2. */
		rt_ctor2(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			 ppm, s->minHistory);
		ref_rt_ctor2(obj[1], s->phases, s->ppmScale, s->taps,
			     s->cutoff, ppm, s->minHistory);
		cmp_obj("ResamplerTiming C2", 0x94, skip_rs, i);
		rt_dtor2(obj[0]);
		ref_rt_dtor2(obj[1]);

		rt_ctor_b(obj[0], s->phases, s->ppmScale, s->taps, bank[0],
			  ppm, s->minHistory);
		ref_rt_ctor_b(obj[1], s->phases, s->ppmScale, s->taps, bank[1],
			      ppm, s->minHistory);
		cmp_obj("ResamplerTiming C1 adopting", 0x94, skip_rs, i);
		rt_dtor2(obj[0]);
		ref_rt_dtor2(obj[1]);

		rt_ctor2_b(obj[0], s->phases, s->ppmScale, s->taps, bank[0],
			   ppm, s->minHistory);
		ref_rt_ctor2_b(obj[1], s->phases, s->ppmScale, s->taps,
			       bank[1], ppm, s->minHistory);
		cmp_obj("ResamplerTiming C2 adopting", 0x94, skip_rs, i);
		rt_dtor2(obj[0]);
		ref_rt_dtor2(obj[1]);

		/* --- V90Resampler: C2, adopting C1/C2, D2. */
		vr_ctor2(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			 parm[0], ppm, s->minHistory);
		ref_vr_ctor2(obj[1], s->phases, s->ppmScale, s->taps,
			     s->cutoff, parm[1], ppm, s->minHistory);
		cmp_obj("V90Resampler C2", 0xb4, skip_vr, i);
		vr_dtor2(obj[0]);
		ref_vr_dtor2(obj[1]);

		vr_ctor_b(obj[0], s->phases, s->ppmScale, s->taps, bank[0],
			  parm[0], ppm, s->minHistory);
		ref_vr_ctor_b(obj[1], s->phases, s->ppmScale, s->taps, bank[1],
			      parm[1], ppm, s->minHistory);
		cmp_obj("V90Resampler C1 adopting", 0xb4, skip_vr, i);
		vr_dtor2(obj[0]);
		ref_vr_dtor2(obj[1]);

		vr_ctor2_b(obj[0], s->phases, s->ppmScale, s->taps, bank[0],
			   parm[0], ppm, s->minHistory);
		ref_vr_ctor2_b(obj[1], s->phases, s->ppmScale, s->taps,
			       bank[1], parm[1], ppm, s->minHistory);
		cmp_obj("V90Resampler C2 adopting", 0xb4, skip_vr, i);
		vr_dtor2(obj[0]);
		ref_vr_dtor2(obj[1]);

		/*
		 * --- The four deleting destructors, on heap objects.
		 *
		 * The count is what is checked, not the corpse: after a D0 the
		 * storage is gone and comparing it would be comparing the
		 * allocators.  Each side must release the same number of
		 * blocks, and `this` must be one of them.
		 */
		{
			void *a = sysdep_malloc(OBJ_SLOT);
			void *b = sysdep_malloc(OBJ_SLOT);

			rs_ctor(a, s->phases, s->ppmScale, s->taps, s->cutoff,
				s->minHistory);
			ref_rs_ctor(b, s->phases, s->ppmScale, s->taps,
				    s->cutoff, s->minHistory);
			base = harness_alloc.frees;
			rs_dtor0(a);
			f0 = harness_alloc.frees - base;
			ref_rs_dtor0(b);
			f1 = harness_alloc.frees - base - f0;
			diff_eq_int("shape %ld: ~Resampler D0 frees", f0, f1,
				    i);
			diff_eq_int("shape %ld: D0 released `this` too",
				    f0 >= 2, 1, i);
			if (f0 >= 2)
				seen_d0_freed = 1;
		}
		{
			void *a = sysdep_malloc(OBJ_SLOT);
			void *b = sysdep_malloc(OBJ_SLOT);

			rto_ctor(a, s->phases, s->ppmScale, s->taps, s->cutoff,
				 ppm, s->minHistory);
			ref_rto_ctor(b, s->phases, s->ppmScale, s->taps,
				     s->cutoff, ppm, s->minHistory);
			base = harness_alloc.frees;
			rto_dtor0(a);
			f0 = harness_alloc.frees - base;
			ref_rto_dtor0(b);
			f1 = harness_alloc.frees - base - f0;
			diff_eq_int("shape %ld: ~RTO D0 frees", f0, f1, i);
		}
		{
			void *a = sysdep_malloc(OBJ_SLOT);
			void *b = sysdep_malloc(OBJ_SLOT);

			rt_ctor(a, s->phases, s->ppmScale, s->taps, s->cutoff,
				ppm, s->minHistory);
			ref_rt_ctor(b, s->phases, s->ppmScale, s->taps,
				    s->cutoff, ppm, s->minHistory);
			base = harness_alloc.frees;
			rt_dtor0(a);
			f0 = harness_alloc.frees - base;
			ref_rt_dtor0(b);
			f1 = harness_alloc.frees - base - f0;
			diff_eq_int("shape %ld: ~ResamplerTiming D0 frees", f0,
				    f1, i);
		}
		{
			void *a = sysdep_malloc(OBJ_SLOT);
			void *b = sysdep_malloc(OBJ_SLOT);

			vr_ctor(a, s->phases, s->ppmScale, s->taps, s->cutoff,
				parm[0], ppm, s->minHistory);
			ref_vr_ctor(b, s->phases, s->ppmScale, s->taps,
				    s->cutoff, parm[1], ppm, s->minHistory);
			base = harness_alloc.frees;
			vr_dtor0(a);
			f0 = harness_alloc.frees - base;
			ref_vr_dtor0(b);
			f1 = harness_alloc.frees - base - f0;
			diff_eq_int("shape %ld: ~V90Resampler D0 frees", f0,
				    f1, i);
			/*
			 * The cross-side count above is the claim; this is
			 * only the floor that says the run was not vacuous.
			 * The exact number varies with the shape -- a taps=0
			 * configuration allocates a zero-length bank -- so an
			 * equality here would be a claim about the fixture
			 * rather than about the object.
			 */
			diff_eq_int("shape %ld: at least three blocks go",
				    f0 >= 3, 1, i);
		}
	}
	rc |= diff_end();
	return rc;
}

/* ------------------------------------------- ResamplerTiming, the loop */

static int seen_dft_done;		/* the 256-sample latch fired    */
static int seen_gain_moved;		/* normBPFhBaudB0coef changed    */
static int seen_pi;			/* the PI arm ran with real gains */
static int seen_addphase_wrap;		/* addPhase's unwrap loop ran    */

/*
 * `timingCorrection`, `SdHalfBaudDft` and `adjustHalfBaudBpfGain`, which are
 * where the timing loop actually is -- and where two separate vacuity traps
 * live.
 *
 * TRAP ONE: `adjustHalfBaudBpfGain` RETURNS IMMEDIATELY unless `dftDone` is
 * set, and only 256 calls to `SdHalfBaudDft` set it.  A comparison over two
 * freshly constructed objects proves that both returned early and nothing
 * else, so the DFT is driven to completion first and `seen_dft_done` records
 * that it was.
 *
 * TRAP TWO: `reset` leaves `bllK1` and `bllK2` at zero, and
 * `V90Resampler::reset` then enters FROZEN, which is defined as both gains
 * zero.  The whole PI update multiplies by zero in that state -- the object
 * still moves `lastHalfBaudErr`, `lastPhaseAdj` and `errZ1`, so a test looks
 * live while the loop it claims to cover is entirely untested.  The gains are
 * therefore poked to real values on both sides, and `seen_pi` records it.
 *
 * AND `SdHalfBaudDft` MUST NOT BE FED A CONSTANT.  The quadrature is
 * +v, -v, -v, +v across four calls, so any DC input cancels to
 * `dftRe == dftIm == 0` exactly, `dftMag` comes out 0, and only the
 * `dftMag == 0` arm of the gain adjustment is ever reached.
 */
static int
case_rt_timing(void)
{
	int rc = 0;

	static const float mags[] = {
		0.0f,		/* the m == 0 arm                        */
		50000.0f,	/* 350000/m == 7    -> the >4 reject     */
		87500.0f,	/* == 4 exactly     -> `ja` NOT taken    */
		100000.0f,	/* == 3.5           -> the >2 clamp      */
		175000.0f,	/* == 2 exactly     -> `jbe` taken       */
		200000.0f,	/* == 1.75          -> pass through      */
		350000.0f,	/* == 1 exactly     -> the floor's edge  */
		400000.0f	/* == 0.875         -> the <1 floor      */
	};
	int i, k;

	diff_begin("ResamplerTiming::timingCorrection / SdHalfBaudDft");
	for (i = 0; i < NSHAPE; i++) {
		const struct shape *s = &shapes[i];
		unsigned j;

		rt_ctor(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			13.0f, s->minHistory);
		ref_rt_ctor(obj[1], s->phases, s->ppmScale, s->taps, s->cutoff,
			    13.0f, s->minHistory);

		/* Real loop gains, or the PI arm is a multiply by zero. */
		{
			float k1 = 0.05f + 0.01f * (float)i;
			float k2 = 0.002f - 0.0001f * (float)i;

			memcpy(obj[0] + 0x4c, &k1, 4);
			memcpy(obj[1] + 0x4c, &k1, 4);
			memcpy(obj[0] + 0x50, &k2, 4);
			memcpy(obj[1] + 0x50, &k2, 4);
			seen_pi = 1;
		}

		/*
		 * An even number of calls, so both arms of the half-baud
		 * alternation run, and enough of them to fill both delay
		 * lines twice over.
		 */
		lfsr = 0x77aa5500u + (unsigned)i;
		for (j = 0; j < 64; j++) {
			float v = next_float();

			rt_tc(obj[0], v);
			ref_rt_tc(obj[1], v);
			cmp_obj("after timingCorrection", 0x94, skip_rs,
				i * 100 + (long)j);
		}

		/* Through the vtable as well: slot 3 must be THIS one. */
		{
			float v = next_float();

			((slot_float)slot(0, 3))(obj[0], v);
			((slot_float)slot(1, 3))(obj[1], v);
			cmp_obj("ResamplerTiming slot 3 (timingCorrection)",
				0x94, skip_rs, i);
			diff_eq_int("shape %ld: slot 3 moved the resonator",
				    u32_at(0, 0x70) != 0, 1, i);
		}

		/*
		 * 256 samples with real energy at Fs/4.  A repeating
		 * four-value pattern, not a constant: a constant cancels.
		 */
		for (j = 0; j < 256; j++) {
			float v = 1.0f + 0.25f * (float)(j & 3)
				  + 0.001f * (float)j;

			rt_dft(obj[0], v);
			ref_rt_dft(obj[1], v);
		}
		cmp_obj("after 256 x SdHalfBaudDft", 0x94, skip_rs, i);
		diff_eq_int("shape %ld: the DFT latched", u32_at(0, 0x8c) & 1,
			    1, i);
		diff_eq_int("shape %ld: the magnitude is not zero",
			    u32_at(0, 0x80) != 0, 1, i);
		if ((u32_at(0, 0x8c) & 1) != 0)
			seen_dft_done = 1;

		/* Past the latch it must ignore everything. */
		for (j = 0; j < 4; j++) {
			rt_dft(obj[0], 3.5f);
			ref_rt_dft(obj[1], 3.5f);
		}
		cmp_obj("SdHalfBaudDft after the latch", 0x94, skip_rs, i);

		/*
		 * resetSdHalfBaudDft restarts the accumulation but must NOT
		 * clear the latch -- that asymmetry is easy to get wrong.
		 */
		rt_resetdft(obj[0]);
		ref_rt_resetdft(obj[1]);
		cmp_obj("resetSdHalfBaudDft after the latch", 0x94, skip_rs,
			i);
		diff_eq_int("shape %ld: the latch survives resetSdHalfBaudDft",
			    u32_at(0, 0x8c) & 1, 1, i);

		rt_dtor(obj[0]);
		ref_rt_dtor(obj[1]);
	}
	rc |= diff_end();

	diff_begin("ResamplerTiming::adjustHalfBaudBpfGain");
	for (k = 0; k < (int)(sizeof mags / sizeof mags[0]); k++) {
		int lvl;

		for (lvl = 0; lvl <= 3; lvl++) {
			unsigned before;
			unsigned one = 1u;

			rt_ctor(obj[0], 8, 1.0f, 8, 0.45f, 0.0f, 0);
			ref_rt_ctor(obj[1], 8, 1.0f, 8, 0.45f, 0.0f, 0);

			/*
			 * The early return comes first: with the latch clear,
			 * nothing at all may happen.
			 */
			memcpy(obj[0] + 0x80, &mags[k], 4);
			memcpy(obj[1] + 0x80, &mags[k], 4);
			rt_gain(obj[0], 1.0f + 0.5f * (float)lvl);
			ref_rt_gain(obj[1], 1.0f + 0.5f * (float)lvl);
			cmp_obj("adjustHalfBaudBpfGain with the latch clear",
				0x94, skip_rs, k);
			diff_eq_int("mag %ld: nothing happened without the "
				    "latch", u32_at(0, 0x90), 0x3d230fd0, k);

			memcpy(obj[0] + 0x8c, &one, 1);
			memcpy(obj[1] + 0x8c, &one, 1);
			before = u32_at(0, 0x90);

			dsplibs_debug_level = (unsigned)lvl;
			ref_dsplibs_debug_level = (unsigned)lvl;
			dsplib_debug_capture_reset();
			/*
			 * THE ARGUMENT MUST BE SWEPT.  With `v` pinned at 1.0
			 * the `dftMag = v * dftMag` at the top of the function
			 * is an identity and dropping it changes nothing --
			 * which is what the mutation sweep reported before
			 * this line moved off the constant.
			 */
			rt_gain(obj[0], 0.25f + 0.75f * (float)lvl);
			ref_rt_gain(obj[1], 0.25f + 0.75f * (float)lvl);
			cmp_obj("after adjustHalfBaudBpfGain", 0x94, skip_rs,
				k * 10 + lvl);
			diff_eq_int("mag %ld: diagnostic lines",
				    (long)dsplib_debug_capture_lines(0),
				    (long)dsplib_debug_capture_lines(1),
				    k * 10 + lvl);
			diff_eq_int("mag %ld: transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)),
				    0, k * 10 + lvl);
			if (u32_at(0, 0x90) != before)
				seen_gain_moved = 1;

			rt_dtor(obj[0]);
			ref_rt_dtor(obj[1]);
		}
	}
	dsplibs_debug_level = 3;
	ref_dsplibs_debug_level = 3;
	rc |= diff_end();

	/*
	 * `addPhase` and `invertPhase`.  Neither has a caller in the object,
	 * so this is the only thing that has ever run them.  What is checked
	 * is the STORE and not only the value: `inputCredit` is written only
	 * inside the unwrap loop, so a version that assigns it unconditionally
	 * gives the right number and the wrong object.
	 */
	diff_begin("ResamplerTiming::addPhase / invertPhase");
	for (i = 0; i < NSHAPE; i++) {
		const struct shape *s = &shapes[i];
		static const float args[] = {
			0.0f, -1.0f, -0.0f, 1.0f, 45.0f, 90.0f, 180.0f,
			360.0f, 1e6f, 0.001f
		};
		unsigned a;

		for (a = 0; a < sizeof args / sizeof args[0]; a++) {
			unsigned credit = 0x5a5au;
			double ph;

			rt_ctor(obj[0], s->phases, s->ppmScale, s->taps,
				s->cutoff, 0.0f, s->minHistory);
			ref_rt_ctor(obj[1], s->phases, s->ppmScale, s->taps,
				    s->cutoff, 0.0f, s->minHistory);

			/* A sentinel, so an unconditional store is visible. */
			memcpy(obj[0] + 0x40, &credit, 4);
			memcpy(obj[1] + 0x40, &credit, 4);
			/*
			 * EXACTLY ON THE BOUNDARY.  With the phase below it, a
			 * zero or negative argument leaves the same object
			 * whether the guard is `> 0` or `>= 0`; sitting on it,
			 * the mutant enters the loop and banks a credit.
			 */
			ph = (double)s->phases;
			memcpy(obj[0] + 0x0c, &ph, 8);
			memcpy(obj[1] + 0x0c, &ph, 8);

			rt_addphase(obj[0], args[a]);
			ref_rt_addphase(obj[1], args[a]);
			cmp_obj("after addPhase", 0x94, skip_rs,
				i * 100 + (long)a);
			if (u32_at(0, 0x40) != credit)
				seen_addphase_wrap = 1;

			/* Exactly on the boundary: `>=` must wrap. */
			ph = (double)s->phases;
			memcpy(obj[0] + 0x0c, &ph, 8);
			memcpy(obj[1] + 0x0c, &ph, 8);
			rt_invphase(obj[0]);
			ref_rt_invphase(obj[1]);
			cmp_obj("after invertPhase on the boundary", 0x94,
				skip_rs, i * 100 + (long)a);

			rt_dtor(obj[0]);
			ref_rt_dtor(obj[1]);
		}
	}
	rc |= diff_end();
	return rc;
}

/* --------------------------------------------------------- the vtables */

/*
 * The four vtables, compared through DISPATCH rather than by reading them.
 *
 * This is the one part of the batch a byte comparison of a constructed object
 * cannot reach: the vptr is blanked everywhere above, precisely because the
 * two sides point at two different tables.  What matters is that the tables
 * AGREE about which body each slot names, and the way to test that with no
 * privileged knowledge of either table is to call each slot on each side and
 * compare what it did.
 *
 * The slot that earns this is +0x10 on `ResamplerTiming`: it is
 * `ResamplerTimingOffset::reset`, NOT a `ResamplerTiming` override.  A
 * reconstruction that gave `ResamplerTiming` its own `reset()` would pass
 * every other assertion in this file and fail here, because dispatching
 * through the base would then clear +0x4c..+0x93 as well.
 */
static int
case_vtables(void)
{
	int rc = 0;

	int i;

	diff_begin("the four vtables, through dispatch");
	for (i = 0; i < NSHAPE; i++) {
		const struct shape *s = &shapes[i];
		unsigned j;

		/* --- Resampler: slot 2 reset(), slot 3 timingCorrection() */
		rs_ctor(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			s->minHistory);
		ref_rs_ctor(obj[1], s->phases, s->ppmScale, s->taps, s->cutoff,
			    s->minHistory);
		{
			double ph = 9.5;

			memcpy(obj[0] + 0x0c, &ph, 8);
			memcpy(obj[1] + 0x0c, &ph, 8);
			((slot_float)slot(0, 3))(obj[0], 0.25f);
			((slot_float)slot(1, 3))(obj[1], 0.25f);
			cmp_obj("Resampler slot 3 (empty base hook)", 0x48,
				skip_rs, i);
			diff_eq_int("shape %ld: the base hook changed nothing",
				    memcmp(obj[0] + 0x0c, &ph, 8), 0, i);
			((slot_void)slot(0, 2))(obj[0]);
			((slot_void)slot(1, 2))(obj[1]);
			cmp_obj("Resampler slot 2 (reset)", 0x48, skip_rs, i);
		}
		rs_dtor(obj[0]);
		ref_rs_dtor(obj[1]);

		/* --- ResamplerTimingOffset */
		rto_ctor(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			 21.0f, s->minHistory);
		ref_rto_ctor(obj[1], s->phases, s->ppmScale, s->taps,
			     s->cutoff, 21.0f, s->minHistory);
		{
			double ph = 4.0;

			memcpy(obj[0] + 0x0c, &ph, 8);
			memcpy(obj[1] + 0x0c, &ph, 8);
			((slot_float)slot(0, 3))(obj[0], 1000.0f);
			((slot_float)slot(1, 3))(obj[1], 1000.0f);
			cmp_obj("RTO slot 3 (timingCorrection)", 0x4c,
				skip_rs, i);
			diff_eq_int("shape %ld: RTO slot 3 moved the phase",
				    memcmp(obj[0] + 0x0c, &ph, 8) != 0, 1, i);
			((slot_void)slot(0, 2))(obj[0]);
			((slot_void)slot(1, 2))(obj[1]);
			cmp_obj("RTO slot 2 (reset)", 0x4c, skip_rs, i);
		}
		rto_dtor(obj[0]);
		ref_rto_dtor(obj[1]);

		/* --- ResamplerTiming: slot 2 is the BASE's reset */
		rt_ctor(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			33.0f, s->minHistory);
		ref_rt_ctor(obj[1], s->phases, s->ppmScale, s->taps, s->cutoff,
			    33.0f, s->minHistory);
		lfsr = 0xa5a50000u + (unsigned)i;
		for (j = 0x4c; j < 0x94; j += 4) {
			unsigned w = next_word();

			memcpy(obj[0] + j, &w, 4);
			memcpy(obj[1] + j, &w, 4);
		}
		{
			unsigned before0 = u32_at(0, 0x90);

			((slot_void)slot(0, 2))(obj[0]);
			((slot_void)slot(1, 2))(obj[1]);
			cmp_obj("ResamplerTiming slot 2", 0x94, skip_rs, i);
			/*
			 * THE DISCRIMINATING ASSERTION.  Slot 2 is
			 * ResamplerTimingOffset::reset, so +0x90 keeps the
			 * junk it was given; a ResamplerTiming override would
			 * have put 0x3d230fd0 there.
			 */
			diff_eq_int("shape %ld: slot 2 left +0x90 alone",
				    u32_at(0, 0x90), before0, i);
			diff_eq_int("shape %ld: slot 2 is not RT::reset",
				    u32_at(1, 0x90), before0, i);

			((slot_uint)slot(0, 4))(obj[0], 7u);
			((slot_uint)slot(1, 4))(obj[1], 7u);
			cmp_obj("ResamplerTiming slot 4", 0x94, skip_rs, i);
			diff_eq_int("shape %ld: slot 4 IS RT::reset(unsigned)",
				    u32_at(0, 0x90), 0x3d230fd0, i);
		}
		rt_dtor(obj[0]);
		ref_rt_dtor(obj[1]);

		/* --- V90Resampler: slot 2 IS overridden, 3 and 4 are not */
		fill_params((unsigned)i + 64u);
		vr_ctor(obj[0], s->phases, s->ppmScale, s->taps, s->cutoff,
			parm[0], 5.0f, s->minHistory);
		ref_vr_ctor(obj[1], s->phases, s->ppmScale, s->taps, s->cutoff,
			    parm[1], 5.0f, s->minHistory);
		{
			unsigned zero = 0;

			memcpy(obj[0] + 0x94, &zero, 4);
			memcpy(obj[1] + 0x94, &zero, 4);
			((slot_void)slot(0, 2))(obj[0]);
			((slot_void)slot(1, 2))(obj[1]);
			cmp_obj("V90Resampler slot 2 (reset)", 0xb4, skip_vr,
				i);
			((slot_uint)slot(0, 4))(obj[0], 3u);
			((slot_uint)slot(1, 4))(obj[1], 3u);
			cmp_obj("V90Resampler slot 4 is still RT's", 0xb4,
				skip_vr, i);
		}
		vr_dtor(obj[0]);
		ref_vr_dtor(obj[1]);
	}
	rc |= diff_end();
	return rc;
}

/* ------------------------------------------------------------------ main */

int
main(void)
{
	int rc = 0;

	/*
	 * This fixture's sinc/FIR float sites diverge from the blob more than
	 * a rounding eps on the modern compiler (F11363: `sinc<float>`'s
	 * return narrowing) and the tier's 1e-6 is not wide enough.  Measured
	 * with DSPLIB_MAX_REPORT=0 over every failing float check:
	 *
	 *     out[] after resample   max |diff|/max|.| = 8.27e-3
	 *     V90Resampler spans                       4.64e-3
	 *     coeffs[] (sinc bank)                     1.23e-3
	 *
	 * 1.5e-2 is just above the measured max with headroom.  The budget is
	 * a no-op under `make period`, so that tier stays bit-exact, and it
	 * does not reach the NaN-phase rejection decisions at all (F11365).
	 */
	harness_float_tol_fixture(1.5e-2);

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = 3;
	ref_dsplibs_debug_level = 3;

	rc |= case_designing_ctor();
	rc |= case_adopting_ctor();
	rc |= case_reset();
	rc |= case_phase();
	rc |= case_history_helpers();
	rc |= case_rto();
	rc |= case_rt();
	rc |= case_v90();
	rc |= case_variants();
	rc |= case_rt_timing();
	rc |= case_resample();
	rc |= case_v90_resample();
	rc |= case_vtables();

	/*
	 * The anti-vacuity gate.  Every one of these says "a comparison this
	 * file makes really did have something in it".  Without them a run
	 * over all-zero buffers reports the same 0 failures.
	 */
	diff_begin("the seeds reached the objects");
	diff_eq_int("a designed coefficient bank was non-zero", seeded_coeffs,
		    1, 0);
	diff_eq_int("a history buffer was seeded non-zero", seeded_history, 1,
		    0);
	diff_eq_int("the parameter block was seeded non-zero", seeded_params,
		    1, 0);
	diff_eq_int("the borrowed-bank destructor arm ran",
		    seen_borrowed_free, 1, 0);
	diff_eq_int("the owning destructor arm ran", seen_owned_free, 1, 0);
	diff_eq_int("resample produced a non-zero sample", seen_output, 1, 0);
	diff_eq_int("the look-ahead arm was entered", seen_lookahead, 1, 0);
	diff_eq_int("the history ring wrapped", seen_wrap, 1, 0);
	diff_eq_int("more outputs than inputs came out of one call",
		    seen_multi, 1, 0);
	diff_eq_int("a sample was carried in `pending`", seen_carry, 1, 0);
	diff_eq_int("the half-baud DFT reached its 256th sample",
		    seen_dft_done, 1, 0);
	diff_eq_int("adjustHalfBaudBpfGain moved the coefficient",
		    seen_gain_moved, 1, 0);
	diff_eq_int("the PI arm ran with non-zero gains", seen_pi, 1, 0);
	diff_eq_int("addPhase's unwrap loop ran", seen_addphase_wrap, 1, 0);
	diff_eq_int("a deleting destructor released its own storage",
		    seen_d0_freed, 1, 0);
	diff_eq_int("a flat ring drove Var below zero", seen_negative_var, 1,
		    0);
	rc |= diff_end();

	/*
	 * `diff_end()` reports only the section it closes -- `diff_begin`
	 * clears the failure count -- so `return diff_end()` alone would exit
	 * 0 with fourteen of the fifteen sections red.  Every section's result
	 * is OR-ed into `rc`.  This file exited 0 through a mutation sweep in
	 * which 101 of 115 mutations were reported NOT CAUGHT before that was
	 * noticed, which is gates.md's "a result indistinguishable from
	 * success" with the test itself as the detector that had died.
	 */
	return rc;
}

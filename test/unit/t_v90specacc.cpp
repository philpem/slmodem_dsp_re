/*
 * t_v90specacc.cpp -- differential test of the V.90 spectral verifier's
 * accumulation cycle and its five frequency accessors:
 *
 *     V90SpectralVerifier::startAccumulation()
 *     V90SpectralVerifier::printSpectrum() const
 *     V90SpectralVerifier::freqToLeftBin(float) const
 *     V90SpectralVerifier::freqToRightBin(float) const
 *     V90SpectralVerifier::freqToNearestBin(float) const
 *     V90SpectralVerifier::getSpectrumOfBin(unsigned long) const
 *     V90SpectralVerifier::getSpectrumOfNearestBin(float) const
 *
 * 799 bytes at 0x45cc0..0x45f00 and 0x465b0.  `checkSpecialSpectralConditions`
 * is NOT retested here -- t_v90specialcond.cpp owns it -- but `process` calls
 * it, so it runs on both sides and its nine diagnostics are inside the
 * transcripts this file compares.
 *
 * THREE OF THE EIGHT HAVE NO OBSERVABLE EXCEPT THEIR RETURN VALUE and two
 * have no observable except their TRANSCRIPT, so this file works at two
 * tiers at once and says which is which per suite:
 *
 *   - the five accessors return a number and write nothing.  Both sides'
 *     objects are compared afterwards anyway, because "writes nothing" is a
 *     claim about the reconstruction that only a comparison can hold.
 *   - `printSpectrum` returns void, writes nothing and touches no memory the
 *     caller can see.  Its ENTIRE output is text, so the transcript is not a
 *     supplementary check here, it is the only one -- t_printtitle.cpp's
 *     position, and this file follows its shape including the anti-vacuity
 *     guards.
 *   - `startAccumulation` and `process` have both.
 *
 * FLOATS CROSS THE BOUNDARY AS BIT PATTERNS WHERE THEY ARE ARGUMENTS, for
 * t_v90spectral.cpp's reason: a `float` parameter declared as such would be
 * loaded and stored by the CALLER, rounding a denormal on both sides at once
 * and proving nothing.  RETURNED floats cannot be treated that way -- the
 * value comes back in %st(0) -- so they are stored to a `float` on each side
 * and compared as PATTERNS rather than as numbers, which is what makes a
 * differing NaN payload or a signed zero a failure instead of a pass.
 *
 * NO TRIAL IS ALLOWED TO REACH THE UNGUARDED INDEX.  `getSpectrumOfBin` and
 * `getSpectrumOfNearestBin` index `spectrum` with no bound (D780), and a
 * float-to-unsigned conversion is undefined in C outside [0, UINT_MAX] --
 * which on x87 is where the object's `fistpll` yields 0x8000000000000000 and
 * the low dword reads 0.  So every frequency driven through the two
 * `getSpectrumOf*` entry points has a quotient this file has computed to land
 * inside the fixture's array, and the out-of-range conversions are driven
 * through the three `freqTo*Bin` functions ONLY, which return the number and
 * do not index with it.  D561: a trial that reaches undefined behaviour in
 * the reconstruction is not a differential trial.
 *
 * THE SWEEP IS BUILT AROUND BIN BOUNDARIES BECAUSE THAT IS WHAT SEPARATES
 * THE THREE ROUNDINGS.  `freqToLeftBin` truncates, `freqToNearestBin` adds a
 * half first and `freqToRightBin` adds one AFTER the truncation; on a
 * quotient of 3.0 they give 3, 3 and 4, on 3.5 they give 3, 4 and 4, and on
 * 2.9999998 they give 2, 3 and 3.  A sweep of round quotients would let all
 * three be spelled `(unsigned)(f / binWidth)` plus a constant and pass.
 *
 * NOTHING IS EVER ZEROED (finding F230) and every object is followed by a
 * guard region compared separately.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/DspMath.h"
#include "dsplib/Psd.h"
#include "dsplib/sysdep.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90SpectralVerifier.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

/*
 * The blob's eight, by asm() label.  Plain cdecl with `this` as the first
 * stack argument (finding F215).  Every `float` PARAMETER is declared
 * `unsigned` so the caller hands over the bit pattern; every `float` RESULT
 * is declared `float` because there is no other way to collect %st(0).
 */
void ref_sv_start(void *self) asm("ref__ZN19V90SpectralVerifier17startAccumulationEv");
int ref_sv_process(void *self, float *in, unsigned int count)
	asm("ref__ZN19V90SpectralVerifier7processEPfj");
void ref_sv_printspec(const void *self)
	asm("ref__ZNK19V90SpectralVerifier13printSpectrumEv");
unsigned ref_sv_left(const void *self, unsigned freq)
	asm("ref__ZNK19V90SpectralVerifier13freqToLeftBinEf");
unsigned ref_sv_right(const void *self, unsigned freq)
	asm("ref__ZNK19V90SpectralVerifier14freqToRightBinEf");
unsigned ref_sv_near(const void *self, unsigned freq)
	asm("ref__ZNK19V90SpectralVerifier16freqToNearestBinEf");
float ref_sv_bin(const void *self, unsigned long bin)
	asm("ref__ZNK19V90SpectralVerifier16getSpectrumOfBinEm");
float ref_sv_nearbin(const void *self, unsigned freq)
	asm("ref__ZNK19V90SpectralVerifier23getSpectrumOfNearestBinEf");

/* Ours, reached the same way, so the two calls are exactly symmetric. */
unsigned our_sv_left(const void *self, unsigned freq)
	asm("_ZNK19V90SpectralVerifier13freqToLeftBinEf");
unsigned our_sv_right(const void *self, unsigned freq)
	asm("_ZNK19V90SpectralVerifier14freqToRightBinEf");
unsigned our_sv_near(const void *self, unsigned freq)
	asm("_ZNK19V90SpectralVerifier16freqToNearestBinEf");
float our_sv_bin(const void *self, unsigned long bin)
	asm("_ZNK19V90SpectralVerifier16getSpectrumOfBinEm");
float our_sv_nearbin(const void *self, unsigned freq)
	asm("_ZNK19V90SpectralVerifier23getSpectrumOfNearestBinEf");

/* The `Psd` `process` runs, built over storage this file owns. */
void psd_construct(void *self, unsigned int length, WindowType window,
		   unsigned int overlap) asm("_ZN3PsdC1Ej10WindowTypej");
void psd_destruct(void *self) asm("_ZN3PsdD1Ev");
}

/* ------------------------------------------------------------- the fixture */

#define SV_SLOT		96u	/* 44 of object, the rest a guard  */
#define SPEC_MAX	64u	/* floats in each side's spectrum  */
#define PSD_LEN		128u	/* SPECTRAL_VERIFIER_PSD_LEN       */
#define FFT_LEN		64u	/* SPECTRAL_VERIFIER_FFT_LEN       */
#define FFT_OVERLAP	32u
#define PARM_SLOT	0x600u
#define TEXT_MAX	16384u

union sv_slot {
	double align;
	unsigned char raw[SV_SLOT];
};

union psd_slot {
	double align;
	unsigned char raw[64];
};

static union sv_slot sv_a, sv_b;
static union psd_slot psd_a, psd_b;
static unsigned char parm[PARM_SLOT] __attribute__((aligned(8)));
static float spec_a[SPEC_MAX], spec_b[SPEC_MAX];
static float accum_a[PSD_LEN + 8], accum_b[PSD_LEN + 8];
static float feed[PSD_LEN + 8];

#define SV_A (*(V90SpectralVerifier *)sv_a.raw)
#define SV_B (*(V90SpectralVerifier *)sv_b.raw)

static unsigned lfsr;

static unsigned char
next_byte(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)(lfsr >> 3);
}

/* The same varied bytes into both sides.  Never zeros -- finding F230. */
static void
fill_pair(void *a, void *b, unsigned n, int trial)
{
	unsigned char *pa = (unsigned char *)a;
	unsigned char *pb = (unsigned char *)b;
	unsigned i;

	lfsr = 0x2f19u + 0x9e37u * (unsigned)trial;
	for (i = 0; i < n; i++)
		pa[i] = pb[i] = next_byte();
}

static unsigned
bits_of(float f)
{
	union {
		float f;
		unsigned u;
	} c;

	c.f = f;
	return c.u;
}

static float
float_of(unsigned u)
{
	union {
		float f;
		unsigned u;
	} c;

	c.u = u;
	return c.f;
}

static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/*
 * Both sides' objects agree everywhere except the two heap-ish pointers this
 * fixture deliberately makes different, so those are swapped for a token
 * before the whole-object compare and what they point at is compared on its
 * own.  `diff_eq_obj` is what reports the first differing FIELD.
 */
static void
snapshot(void *dst, const V90SpectralVerifier *src)
{
	V90SpectralVerifier *d = (V90SpectralVerifier *)dst;

	memcpy(dst, src, sizeof(V90SpectralVerifier));
	d->params = (V90Parameters *)(long)(src->params != 0);
	d->psd = (Psd *)(long)(src->psd != 0);
	d->buf_18 = (float *)(long)(src->buf_18 != 0);
	d->spectrum = (float *)(long)(src->spectrum != 0);
}

static void
compare_objects(const char *what, long tag)
{
	unsigned char sa[sizeof(V90SpectralVerifier)];
	unsigned char sb[sizeof(V90SpectralVerifier)];

	snapshot(sa, &SV_A);
	snapshot(sb, &SV_B);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90SpectralVerifier", sa, sb,
		     sizeof(V90SpectralVerifier), tag);
	diff_eq_int("no store past the object (%ld)",
		    memcmp(sv_a.raw + sizeof(SV_A), sv_b.raw + sizeof(SV_B),
			   SV_SLOT - sizeof(SV_A)) == 0, 1, tag);
}

/*
 * The parameter block, seeded and then given the eleven slots the three
 * functions under test read.  The probe frequencies are all under
 * SPEC_BINS * binWidth so that `checkSpecialSpectralConditions`, which
 * `process` tail-calls, cannot reach past the fixture's spectrum either.
 */
static void
set_params(float binWidth, int enable, int printSpectrum)
{
	V90Parameters *p = (V90Parameters *)parm;
	unsigned i;

	lfsr = 0x7a51u;
	for (i = 0; i < PARM_SLOT; i++)
		parm[i] = next_byte();

	p->SPECTRAL_VERIFIER_ENABLE = enable;
	p->SPECTRAL_VERIFIER_PRINT_SPECTRUM = printSpectrum;
	p->SPECTRAL_VERIFIER_FFT_LEN = (int)FFT_LEN;
	p->SPECTRAL_VERIFIER_PSD_LEN = (int)PSD_LEN;
	p->SPECTRAL_VERIFIER_PSD_OVERLAP_LEN = (int)FFT_OVERLAP;
	p->SPECTRAL_VERIFIER_FFT_WINDOW = (int)WINDOW_HANNING;
	p->SPECTRAL_VERIFIER_SAMPLE_FREQ = binWidth * (float)FFT_LEN;

	p->SPECTRAL_VERIFIER_ISDN_LEFT_PEAK_FREQ = 4.0f * binWidth;
	p->SPECTRAL_VERIFIER_ISDN_NULL_FREQ = 6.0f * binWidth;
	p->SPECTRAL_VERIFIER_ISDN_RIGHT_PEAK_FREQ = 8.0f * binWidth;
	p->SPECTRAL_VERIFIER_GERMAN_PBX_LEFT_PEAK_FREQ = 10.0f * binWidth;
	p->SPECTRAL_VERIFIER_GERMAN_PBX_NULL_FREQ = 12.0f * binWidth;
	p->SPECTRAL_VERIFIER_GERMAN_PBX_RIGHT_PEAK_FREQ = 14.0f * binWidth;
	p->SPECTRAL_VERIFIER_SEVERE_CODEC_REF_FREQ = 16.0f * binWidth;
	p->SPECTRAL_VERIFIER_SEVERE_CODEC_TEST_FREQ1 = 18.0f * binWidth;
	p->SPECTRAL_VERIFIER_SEVERE_CODEC_TEST_FREQ2 = 20.0f * binWidth;

	p->SPECTRAL_VERIFIER_ISDN_LEFT_PEAK_DELTA = 1.0f;
	p->SPECTRAL_VERIFIER_ISDN_RIGHT_PEAK_DELTA = 1.0f;
	p->SPECTRAL_VERIFIER_GERMAN_PBX_LEFT_PEAK_DELTA = 1.0f;
	p->SPECTRAL_VERIFIER_GERMAN_PBX_RIGHT_PEAK_DELTA = 1.0f;
	p->SPECTRAL_VERIFIER_SEVERE_CODEC_DELTA = 1.0f;
}

/* ================================================== the five accessors ==== */

/*
 * Quotients, not frequencies: each row is what `freq / binWidth` comes to,
 * and the frequency is formed from it so that the three roundings can be
 * predicted and so that `indexable` says whether the two `getSpectrumOf*`
 * entry points may be driven with it.
 *
 * `indexable` is 0 for every row whose nearest bin lands outside
 * [0, SPEC_MAX) or whose conversion is undefined -- the negative rows and
 * the two huge ones.  Those rows still exercise all three `freqTo*Bin`
 * functions, which return the number without indexing anything.
 */
struct qcase {
	const char *what;
	float quotient;
	int indexable;
};

static const struct qcase qcases[] = {
	{ "zero",		0.0f,		1 },
	{ "just above zero",	0.25f,		1 },
	{ "a half",		0.5f,		1 },
	{ "just under a half",	0.49999997f,	1 },
	{ "just over a half",	0.50000006f,	1 },
	{ "one",		1.0f,		1 },
	{ "just under one",	0.99999994f,	1 },
	{ "three",		3.0f,		1 },
	{ "three and a half",	3.5f,		1 },
	{ "just under 3.5",	3.4999998f,	1 },
	{ "just over 3.5",	3.5000002f,	1 },
	{ "just under four",	3.9999998f,	1 },
	{ "seventeen",		17.0f,		1 },
	{ "17.5 exactly",	17.5f,		1 },
	{ "the last bin",	(float)(SPEC_MAX - 1),		1 },
	{ "half short of last",	(float)(SPEC_MAX - 1) - 0.5f,	1 },
	{ "one past the array",	(float)SPEC_MAX,		0 },
	{ "a big quotient",	1.0e6f,				0 },
	{ "just under 2**31",	2147483520.0f,			0 },
	{ "minus a half",	-0.5f,				0 },
	{ "minus one",		-1.0f,				0 },
	{ "minus a lot",	-1.0e6f,			0 }
};

#define NQ (sizeof(qcases) / sizeof(qcases[0]))

/*
 * A NEGATIVE OR HUGE QUOTIENT IS UNDEFINED IN C AND THE OBJECT STILL DOES
 * SOMETHING WITH IT, so those rows are driven only where the answer is
 * RETURNED and never where it is used as a subscript -- and even then the
 * conversion itself is undefined, so the comparison is declared for what it
 * is: it holds our expansion of `fixuns_truncsfsi2` to the object's, over
 * inputs the C standard does not define, on one target with one compiler.
 * It is worth having because a reconstruction that used `(int)` instead of
 * `(unsigned)` agrees over every DEFINED input and differs here; it is not
 * worth generalising from.
 */
static int
run_accessors(void)
{
	unsigned i, k;
	static const float widths[] = { 1.0f, 125.0f, 0.5f, 62.5f, 3.0f };

	diff_begin("V90SpectralVerifier accessors");

	for (k = 0; k < sizeof(widths) / sizeof(widths[0]); k++) {
		float bw = widths[k];

		for (i = 0; i < NQ; i++) {
			long tag = (long)(k * 100 + i);
			float freq = qcases[i].quotient * bw;
			unsigned fa;
			unsigned la, lb, ra, rb, na, nb;

			set_params(bw, 1, 0);
			fill_pair(sv_a.raw, sv_b.raw, SV_SLOT, (int)tag);

			lfsr = 0x0f0fu + (unsigned)tag;
			for (fa = 0; fa < SPEC_MAX; fa++) {
				spec_a[fa] = spec_b[fa] =
				    (float)((int)next_byte() - 128) * 0.375f;
			}

			SV_A.params = SV_B.params = (V90Parameters *)parm;
			SV_A.binWidth = SV_B.binWidth = bw;
			SV_A.fftLength = SV_B.fftLength = FFT_LEN;
			SV_A.spectrum = spec_a;
			SV_B.spectrum = spec_b;

			la = our_sv_left(&SV_A, bits_of(freq));
			lb = ref_sv_left(&SV_B, bits_of(freq));
			ra = our_sv_right(&SV_A, bits_of(freq));
			rb = ref_sv_right(&SV_B, bits_of(freq));
			na = our_sv_near(&SV_A, bits_of(freq));
			nb = ref_sv_near(&SV_B, bits_of(freq));

			diff_eq_int("freqToLeftBin (%ld)", (long)la, (long)lb,
				    tag);
			diff_eq_int("freqToRightBin (%ld)", (long)ra,
				    (long)rb, tag);
			diff_eq_int("freqToNearestBin (%ld)", (long)na,
				    (long)nb, tag);

			/*
			 * THE RELATION IS ASSERTED TOO, and not because the
			 * blob's answer needs corroborating: it is what makes
			 * a fixture that quietly stopped varying the input
			 * visible.  `right` is `left + 1` for every operand
			 * because the object increments the INTEGER.
			 */
			diff_eq_int("right is left plus one (%ld)",
				    (long)(unsigned)(la + 1u), (long)ra, tag);

			if (qcases[i].indexable) {
				float ba, bb, ca, cb;

				ba = our_sv_bin(&SV_A, (unsigned long)la);
				bb = ref_sv_bin(&SV_B, (unsigned long)lb);
				ca = our_sv_nearbin(&SV_A, bits_of(freq));
				cb = ref_sv_nearbin(&SV_B, bits_of(freq));

				diff_eq_int("getSpectrumOfBin pattern (%ld)",
					    (long)bits_of(ba),
					    (long)bits_of(bb), tag);
				diff_eq_int("getSpectrumOfNearestBin "
					    "pattern (%ld)", (long)bits_of(ca),
					    (long)bits_of(cb), tag);

				/*
				 * And that it really is the NEAREST bin's
				 * entry: a `getSpectrumOfNearestBin` that
				 * rounded like `freqToLeftBin` would agree
				 * with the blob only if the blob did too.
				 */
				diff_eq_int("nearest bin is the one indexed "
					    "(%ld)", (long)bits_of(ca),
					    (long)bits_of(spec_a[na]), tag);
			}

			compare_objects("after the accessors", tag);
		}
	}

	/*
	 * THE SEPARATING TRIALS, and each counts a pair whose RESULTS differ
	 * rather than a pair that took a different path (findings F3509,
	 * F3403).  Three rounding modes over one operand: 3.4999998 and
	 * 3.5000002 differ in `freqToNearestBin` and agree in the other two,
	 * which is what says the `+ 0.5f` is real and is in exactly one of
	 * the three.
	 */
	{
		int sep_near = 0, sep_left = 0, agree_left = 0;
		unsigned lo = bits_of(3.4999998f), hi = bits_of(3.5000002f);
		unsigned a, b;

		set_params(1.0f, 1, 0);
		fill_pair(sv_a.raw, sv_b.raw, SV_SLOT, 9001);
		SV_A.params = SV_B.params = (V90Parameters *)parm;
		SV_A.binWidth = SV_B.binWidth = 1.0f;
		SV_A.spectrum = spec_a;
		SV_B.spectrum = spec_b;

		a = our_sv_near(&SV_A, lo);
		b = our_sv_near(&SV_A, hi);
		if (a != b)
			sep_near++;

		a = our_sv_left(&SV_A, lo);
		b = our_sv_left(&SV_A, hi);
		if (a == b)
			agree_left++;

		a = our_sv_left(&SV_A, bits_of(2.9999998f));
		b = our_sv_left(&SV_A, bits_of(3.0000002f));
		if (a != b)
			sep_left++;

		diff_eq_int("the half separates freqToNearestBin",
			    sep_near, 1, 0);
		diff_eq_int("the half does NOT separate freqToLeftBin",
			    agree_left, 1, 0);
		diff_eq_int("the integer boundary separates freqToLeftBin",
			    sep_left, 1, 0);
	}

	return diff_end();
}

/* ================================================== startAccumulation ===== */

/*
 * Two gates and three states.  The parameter gate is swept because it covers
 * the STATE STORES as well as the diagnostic (see the .cpp), so a
 * reconstruction that gated only the `edprintf` passes every trial with the
 * parameter set and fails every trial without it.
 */
static int
run_start(void)
{
	static const unsigned states[] = { 0u, 1u, 2u, 3u, 0xffffffffu };
	unsigned s, e, lvl;
	long tag = 0;

	diff_begin("V90SpectralVerifier::startAccumulation");

	for (lvl = 0; lvl <= 2; lvl += 2) {
		for (e = 0; e <= 1; e++) {
			for (s = 0; s < sizeof(states) / sizeof(states[0]);
			     s++) {
				int expect_print;

				tag++;
				set_level(lvl);
				set_params(125.0f, (int)e, 0);
				fill_pair(sv_a.raw, sv_b.raw, SV_SLOT,
					  (int)tag);

				SV_A.params = SV_B.params =
				    (V90Parameters *)parm;
				SV_A.accumulating = SV_B.accumulating =
				    states[s];

				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();

				SV_A.startAccumulation();
				ref_sv_start(&SV_B);

				dsplib_debug_capture_on = 0;

				compare_objects("after startAccumulation",
						tag);
				diff_eq_int("transcript matches (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag);
				diff_eq_int("line counts match (%ld)",
					    (int)dsplib_debug_capture_lines(0),
					    (int)dsplib_debug_capture_lines(1),
					    tag);

				/*
				 * The anti-vacuity guard: exactly which
				 * combinations must say something.  Without
				 * it every check above is satisfied by two
				 * functions that both do nothing, which is
				 * what both DO whenever the parameter is
				 * clear.
				 */
				expect_print = (lvl > 1 && e == 1
						&& states[s] != 1u);
				diff_eq_int("printed exactly when it should, "
					    "case %ld",
					    dsplib_debug_capture_lines(1) > 0,
					    expect_print, tag);

				/*
				 * And the state itself, at its absolute
				 * offset, against what the trial asked for --
				 * side-against-side cannot see two sides that
				 * both failed to store.
				 */
				diff_eq_int("the blob's state, case %ld",
					    (long)SV_B.accumulating,
					    (long)(e == 1 && states[s] != 1u
						   ? 1u : states[s]), tag);
			}
		}
	}

	set_level(0);
	return diff_end();
}

/* ======================================================= printSpectrum ==== */

/*
 * `printSpectrum` HAS NO OBSERVABLE BUT THE TEXT.  Everything below is a
 * transcript comparison, and the pair checks at the end are what stop two
 * empty strings from agreeing: each pair differs in ONE printed quantity and
 * is asserted to produce a DIFFERENT transcript from its partner.
 */
static int
run_print(void)
{
	static const unsigned lengths[] = { 2u, 4u, 8u, 64u, 128u };
	static char text[8][TEXT_MAX];
	unsigned k, i, lvl;
	long tag = 0;
	int slot = 0;

	diff_begin("V90SpectralVerifier::printSpectrum");

	for (lvl = 0; lvl <= 2; lvl += 2) {
		for (k = 0; k < sizeof(lengths) / sizeof(lengths[0]); k++) {
			tag++;
			set_level(lvl);
			set_params(125.0f, 1, 0);
			fill_pair(sv_a.raw, sv_b.raw, SV_SLOT, (int)tag);

			/*
			 * The values are chosen so that every printed field
			 * varies: a negative and a positive, an exact
			 * integer, a value whose hundredths are non-zero, a
			 * value under one so the whole part is 0, and a
			 * negative zero -- which the object prints '-' for,
			 * because the sign test is `!(0.0f >= v)`.
			 */
			lfsr = 0x51a3u + (unsigned)tag;
			for (i = 0; i < SPEC_MAX; i++) {
				int n = (int)next_byte();

				spec_a[i] = spec_b[i] =
				    (float)(n - 128) * 0.0703125f;
			}
			spec_a[0] = spec_b[0] = 0.0f;
			spec_a[1] = spec_b[1] = float_of(0x80000000u);
			if (SPEC_MAX > 5) {
				spec_a[2] = spec_b[2] = 12.0f;
				spec_a[3] = spec_b[3] = -12.34f;
				spec_a[4] = spec_b[4] = 0.99f;
				spec_a[5] = spec_b[5] = -0.01f;
			}

			SV_A.params = SV_B.params = (V90Parameters *)parm;
			SV_A.binWidth = SV_B.binWidth = 125.0f;
			SV_A.fftLength = SV_B.fftLength = lengths[k];
			SV_A.spectrum = spec_a;
			SV_B.spectrum = spec_b;

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			SV_A.printSpectrum();
			ref_sv_printspec(&SV_B);

			dsplib_debug_capture_on = 0;

			compare_objects("after printSpectrum", tag);
			diff_eq_int("transcript matches (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("line counts match (%ld)",
				    (int)dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1), tag);
			diff_eq_int("printed exactly when it should (%ld)",
				    dsplib_debug_capture_lines(1) > 0,
				    lvl > 1, tag);

			/*
			 * THE LINE COUNT IS ASSERTED ABSOLUTELY, not just
			 * side against side: two rules and one line per bin.
			 * A loop bound of `fftLength` rather than
			 * `fftLength / 2` agrees with the blob on nothing,
			 * but a loop that ran zero times would agree with a
			 * blob that also printed only the two rules, and this
			 * is what separates those.
			 */
			if (lvl > 1) {
				diff_eq_int("two rules and one line per bin "
					    "(%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    (int)(lengths[k] / 2u + 2u), tag);
			}

			if (lvl > 1 && slot < 8) {
				strncpy(text[slot], dsplib_debug_capture_text(1),
					TEXT_MAX - 1);
				text[slot][TEXT_MAX - 1] = '\0';
				slot++;
			}
		}
	}

	/*
	 * THE SEPARATING TRIALS.  Four pairs, each differing in exactly one
	 * printed quantity, each asserted to give a DIFFERENT transcript:
	 * the sign, the whole part, the hundredths and the printed frequency.
	 * A counter that only recorded a different code path would be
	 * satisfied by a `printSpectrum` that printed a constant, which is
	 * finding F3509's failure exactly.
	 */
	{
		struct {
			const char *what;
			float bin3;
			float binWidth;
		} pair[8];
		char a[TEXT_MAX], b[TEXT_MAX];
		int p, differed = 0;

		pair[0].what = "sign";
		pair[0].bin3 = 2.5f;		pair[0].binWidth = 125.0f;
		pair[1].what = "sign";
		pair[1].bin3 = -2.5f;		pair[1].binWidth = 125.0f;
		pair[2].what = "whole";
		pair[2].bin3 = 2.5f;		pair[2].binWidth = 125.0f;
		pair[3].what = "whole";
		pair[3].bin3 = 3.5f;		pair[3].binWidth = 125.0f;
		pair[4].what = "hundredths";
		pair[4].bin3 = 2.5f;		pair[4].binWidth = 125.0f;
		pair[5].what = "hundredths";
		pair[5].bin3 = 2.75f;		pair[5].binWidth = 125.0f;
		pair[6].what = "frequency";
		pair[6].bin3 = 2.5f;		pair[6].binWidth = 125.0f;
		pair[7].what = "frequency";
		pair[7].bin3 = 2.5f;		pair[7].binWidth = 130.0f;

		set_level(2);
		set_params(125.0f, 1, 0);

		for (p = 0; p < 8; p += 2) {
			int q;

			for (q = 0; q < 2; q++) {
				for (i = 0; i < SPEC_MAX; i++)
					spec_a[i] = 1.0f;
				spec_a[3] = pair[p + q].bin3;

				fill_pair(sv_a.raw, sv_b.raw, SV_SLOT, 7000 + p);
				SV_A.params = (V90Parameters *)parm;
				SV_A.binWidth = pair[p + q].binWidth;
				SV_A.fftLength = 16u;
				SV_A.spectrum = spec_a;

				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();
				SV_A.printSpectrum();
				dsplib_debug_capture_on = 0;

				strncpy(q == 0 ? a : b,
					dsplib_debug_capture_text(0),
					TEXT_MAX - 1);
				(q == 0 ? a : b)[TEXT_MAX - 1] = '\0';
			}

			if (strcmp(a, b) != 0)
				differed++;
			/*
			 * The tag is the pair's index in `pair[]` -- 0 the
			 * sign, 2 the whole part, 4 the hundredths, 6 the
			 * printed frequency.  It is a number and not the
			 * name because `diff_eq_int`'s format takes the
			 * INPUT as its only conversion (finding F220); a `%s`
			 * here reads the tag as a pointer.
			 */
			diff_eq_int("printed quantity %ld changes the "
				    "transcript", strcmp(a, b) != 0, 1,
				    (long)p);
		}

		diff_eq_int("all four printed quantities separate",
			    differed, 4, 0);
	}

	set_level(0);
	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_accessors();
	bad |= run_start();
	bad |= run_print();

	return bad;
}

/*
 * t_v90specproc.cpp -- differential test of
 *
 *     V90SpectralVerifier::process(float *, unsigned int)
 *
 * 268 bytes at 0x465b0: fill the accumulation buffer from the caller's block
 * and, on the sample that fills it, run the periodogram, say so, optionally
 * dump the spectrum and classify the line.
 *
 * IT IS ITS OWN BINARY AND THAT IS NOT A STYLE CHOICE.  `process` calls
 * `Psd::process`, which carries a DECLARED divergence on the modern compiler
 * -- `tools/gccdiverge.json`, finding F1453: x87 excess precision in
 * `four1`/`realfft` reaching the decibel scaling, up to 0.043 dB and not
 * closable with a tolerance.  Every call that completes an accumulation
 * therefore inherits it, so this binary is excused on GCC 13 and green on the
 * period compiler, which is the tier that decides.
 *
 * An excused binary exits non-zero, and `tools/mutate.py` judges a mutant
 * caught by a non-zero exit, so a red binary cannot score a mutation set and
 * refuses.  t_v90p4dnan's entry in that register records the same problem and
 * the same answer: split the excused check into a binary of its own.  Which
 * is why the other seven members of this class are in `t_v90specacc.cpp`,
 * green under BOTH compilers and mutation-testable, and only `process` is
 * here.  Finding F5804.
 *
 * WHAT IS STILL EXACT ON BOTH TIERS is everything `process` itself owns: the
 * state machine, the return value, the copy loop and its counter, the
 * argument marshalling, and the transcripts.  What diverges on GCC 13 is the
 * CONTENT of the spectrum `Psd::process` wrote, and it diverges in the last
 * bit or two.
 *
 * THIS IS AN INTEGRATION TEST AND SAYS SO.  `process` calls `Psd::process`
 * and `checkSpecialSpectralConditions`, and on each side those are that
 * side's own -- so a failure here is a failure in one of the three and this
 * file cannot say which.  It is worth running that way round: both callees
 * have suites of their own (t_psd.cpp, t_v90specialcond.cpp), so a green
 * there and a red here points at `process` itself.
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
 * The blob's `process`, by asm() label: plain cdecl with `this` as the first
 * stack argument (finding F215).
 */
int ref_sv_process(void *self, float *in, unsigned int count)
	asm("ref__ZN19V90SpectralVerifier7processEPfj");


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


/*
 * The whole accumulation cycle, with a real `Psd` on each side.  Each side
 * gets its OWN accumulation buffer, spectrum and `Psd` because `process`
 * writes all three; the parameter block is shared because nothing writes it.
 *
 * THIS IS AN INTEGRATION TEST AND SAYS SO.  `process` calls `Psd::process`
 * and `checkSpecialSpectralConditions`, and on each side those are that
 * side's own -- so a failure here is a failure in one of the three and this
 * file cannot say which.  It is worth running that way round: the two
 * callees have their own suites, so a green there and a red here points at
 * `process` itself.
 */
static int
run_process(void)
{
	static const unsigned chunks[] = { PSD_LEN, 1u, 7u, 64u, 127u,
					   PSD_LEN + 8u };
	unsigned c, s, i;
	long tag = 0;

	diff_begin("V90SpectralVerifier::process");

	for (c = 0; c < sizeof(chunks) / sizeof(chunks[0]); c++) {
		for (s = 0; s <= 2; s++) {
			unsigned fed = 0;
			int pass;

			tag++;
			set_level(2);
			set_params(125.0f, 1, (int)(c & 1u));

			psd_construct(psd_a.raw, FFT_LEN, WINDOW_HANNING,
				      FFT_OVERLAP);
			psd_construct(psd_b.raw, FFT_LEN, WINDOW_HANNING,
				      FFT_OVERLAP);

			fill_pair(sv_a.raw, sv_b.raw, SV_SLOT, (int)tag);
			fill_pair(accum_a, accum_b,
				  sizeof(accum_a), (int)tag + 1);
			for (i = 0; i < SPEC_MAX; i++)
				spec_a[i] = spec_b[i] = 1.0e30f;

			lfsr = 0x31d7u + (unsigned)tag;
			for (i = 0; i < PSD_LEN + 8u; i++)
				feed[i] = (float)((int)next_byte() - 128)
				    * 0.0078125f;

			SV_A.params = SV_B.params = (V90Parameters *)parm;
			SV_A.psd = (Psd *)psd_a.raw;
			SV_B.psd = (Psd *)psd_b.raw;
			SV_A.buf_18 = accum_a;
			SV_B.buf_18 = accum_b;
			SV_A.spectrum = spec_a;
			SV_B.spectrum = spec_b;
			SV_A.fftLength = SV_B.fftLength = FFT_LEN;
			SV_A.psdLength = SV_B.psdLength = PSD_LEN;
			SV_A.binWidth = SV_B.binWidth = 125.0f;
			SV_A.accumCount = SV_B.accumCount = 0u;
			SV_A.accumulating = SV_B.accumulating = s;
			SV_A.word_28 = SV_B.word_28 = 0xdeadbeefu;

			/*
			 * Feed the block in `chunks[c]`-sized pieces until
			 * either the buffer is full or the feed runs out.
			 * Every call is compared, not just the one that
			 * completes: the ones before it return 0 and move the
			 * counter, and a reconstruction that copied the wrong
			 * number of samples shows up there first.
			 */
			for (pass = 0; pass < 200 && fed < PSD_LEN + 8u;
			     pass++) {
				unsigned n = chunks[c];
				int ra, rb;

				if (fed + n > PSD_LEN + 8u)
					n = PSD_LEN + 8u - fed;

				dsplib_debug_capture_on = 1;
				dsplib_debug_capture_reset();

				ra = SV_A.process(feed + fed, n);
				rb = ref_sv_process(&SV_B, feed + fed, n);

				dsplib_debug_capture_on = 0;
				fed += n;

				diff_eq_int("return value (%ld)", (long)ra,
					    (long)rb, tag * 1000 + pass);
				compare_objects("after process",
						tag * 1000 + pass);
				diff_eq_int("accumulation buffer (%ld)",
					    memcmp(accum_a, accum_b,
						   sizeof(accum_a)) == 0, 1,
					    tag * 1000 + pass);
				for (i = 0; i < SPEC_MAX; i++) {
					diff_eq_int("spectrum bin (%ld)",
						    (long)bits_of(spec_a[i]),
						    (long)bits_of(spec_b[i]),
						    tag * 1000 + pass);
				}
				diff_eq_int("transcript matches (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag * 1000 + pass);
				diff_eq_int("line counts match (%ld)",
					    (int)dsplib_debug_capture_lines(0),
					    (int)dsplib_debug_capture_lines(1),
					    tag * 1000 + pass);

				/*
				 * The anti-vacuity guard for the state
				 * machine: outside state 1 nothing may
				 * happen at all, and the counter must not
				 * move.
				 */
				if (s != 1u) {
					diff_eq_int("idle state returns 0 "
						    "(%ld)", (long)rb, 0,
						    tag * 1000 + pass);
					diff_eq_int("idle state is silent "
						    "(%ld)",
						    (int)dsplib_debug_capture_lines(1),
						    0, tag * 1000 + pass);
					diff_eq_int("idle state does not "
						    "count (%ld)",
						    (long)SV_B.accumCount, 0,
						    tag * 1000 + pass);
				}
			}

			/*
			 * AND THE CYCLE COMPLETED AT LEAST ONCE where it
			 * should have.  Without this the whole suite passes
			 * on two `process` implementations that both return
			 * early for ever.
			 */
			if (s == 1u) {
				diff_eq_int("the accumulation finished (%ld)",
					    (long)SV_B.accumulating, 2, tag);
				diff_eq_int("the counter reached the target "
					    "(%ld)", (long)SV_B.accumCount,
					    (long)PSD_LEN, tag);
				diff_eq_int("the spectrum was written (%ld)",
					    spec_b[0] != 1.0e30f, 1, tag);
				diff_eq_int("a verdict was reached (%ld)",
					    SV_B.word_28 != 0xdeadbeefu, 1,
					    tag);
			}

			psd_destruct(psd_a.raw);
			psd_destruct(psd_b.raw);
		}
	}

	/*
	 * THE SEPARATING TRIAL FOR `process`: two runs differing only in
	 * `SPECTRAL_VERIFIER_PRINT_SPECTRUM` must differ in the TRANSCRIPT,
	 * which is what says the parameter is read and the call is made.  A
	 * counter that watched the return value or the spectrum would count
	 * nothing here -- both are identical between the two runs -- and that
	 * is the distinction findings F3509 and F3403 are about.
	 */
	{
		static char t0[TEXT_MAX];
		int p;

		set_level(2);
		for (p = 0; p <= 1; p++) {
			set_params(125.0f, 1, p);
			psd_construct(psd_a.raw, FFT_LEN, WINDOW_HANNING,
				      FFT_OVERLAP);
			fill_pair(sv_a.raw, sv_b.raw, SV_SLOT, 8000 + p);

			lfsr = 0x99a1u;
			for (i = 0; i < PSD_LEN; i++)
				feed[i] = (float)((int)next_byte() - 128)
				    * 0.0078125f;

			SV_A.params = (V90Parameters *)parm;
			SV_A.psd = (Psd *)psd_a.raw;
			SV_A.buf_18 = accum_a;
			SV_A.spectrum = spec_a;
			SV_A.fftLength = FFT_LEN;
			SV_A.psdLength = PSD_LEN;
			SV_A.binWidth = 125.0f;
			SV_A.accumCount = 0u;
			SV_A.accumulating = 1u;

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();
			SV_A.process(feed, PSD_LEN);
			dsplib_debug_capture_on = 0;

			if (p == 0) {
				strncpy(t0, dsplib_debug_capture_text(0),
					TEXT_MAX - 1);
				t0[TEXT_MAX - 1] = '\0';
			} else {
				diff_eq_int("PRINT_SPECTRUM changes the "
					    "transcript",
					    strcmp(t0,
						   dsplib_debug_capture_text(0))
					    != 0, 1, 0);
				diff_eq_int("and it is the LONGER one that "
					    "printed the spectrum",
					    (int)strlen(
						dsplib_debug_capture_text(0))
					    > (int)strlen(t0), 1, 0);
			}

			psd_destruct(psd_a.raw);
		}
	}

	set_level(0);
	return diff_end();
}

int
main(void)
{
	return run_process();
}

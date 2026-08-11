/*
 * t_v90spectral.cpp -- differential test of the spectral group's lifecycles.
 *
 *     V90SpectralShapingFilter::V90SpectralShapingFilter()
 *     V90SdDetector::V90SdDetector(float, float, float, unsigned) / ~
 *     V90SdDetector::reset()
 *     V90SpectralVerifier::V90SpectralVerifier(V90Parameters *) / ~
 *     V90SpectralVerifier::reset()
 *     V90SpectralShaper::V90SpectralShaper() / ~
 *
 * `reset()` is here as well as in t_v90leaves.cpp so that each of the four
 * sources this file's mutation suites name is covered ENTIRELY by this
 * binary: a suite whose binary cannot reach half its source reads NOT CAUGHT
 * for that half, which is the verdict an untested claim gives too.
 *
 * BOTH SIDES ARE CALLED BY SYMBOL, ours as well as the blob's.  C++ has no
 * syntax for running a constructor over storage that already exists, and the
 * tree builds `-nostdinc++` with no `<new>`; naming the symbol is what the
 * ABI does anyway and it keeps the two sides exactly symmetric.  C1 and C2
 * are alternated, and D1 and D2 with them: GCC emits each pair from one
 * definition and this file fails to LINK if it did not.
 *
 * THE THREE FLOAT ARGUMENTS ARE DECLARED `unsigned int`, on both sides.
 * They occupy one four-byte stack slot each in cdecl whichever way they are
 * declared, so the bytes the callee sees are identical -- and passing the
 * BIT PATTERN means a denormal reaches the constructor intact instead of
 * being rounded by an `flds`/`fstps` pair in the CALLER, where it would be
 * rounded on both sides at once and prove nothing.  The stored words are read
 * back with memcpy for the same reason.  What separates a copy from a
 * conversion is one pattern, it is not in the sweep, and why is at `sd_pat`.
 *
 * ARGUMENTS 3 AND 4 ARE STORED OUT OF ORDER (+0x10 and +0x04), so every
 * trial passes three DISTINCT float patterns: a pair of equal arguments
 * makes a swapped store invisible to both sides at once.
 *
 * THE FOURTH ARGUMENT IS SWEPT AROUND 12 -- 0, 1, 11, 12, 13, 0xffffffff --
 * because `historyLength` and the allocation size are the CONSTANTS 12 and
 * 0x30 whatever it is.  A reconstruction sizing either from the argument
 * would pass every trial where the argument happened to be 12.
 *
 * NOTHING IS EVER ZEROED (finding 230): both sides get the same varied
 * pseudorandom bytes before every call and are reseeded every trial, so a
 * store that fails to happen is visible.  Every object is followed by a
 * guard region that is compared separately, so a store past the end shows up
 * as a failure rather than as silence.  Where a constructor's whole write set
 * is CONSTANT, side-against-side is not enough on its own -- so each value is
 * also asserted at its ABSOLUTE OFFSET, against the seed it replaced, with no
 * reference to a field name (findings 223 and 224).
 *
 * TWO POINTERS ARE NEVER EQUAL -- every `sysdep_malloc` return is a different
 * address on the two sides -- so the snapshot replaces each with that side's
 * own answer to "is it null", and what they point at is compared separately.
 * The harness fills every fresh allocation with HARNESS_MALLOC_FILL, so a
 * buffer the constructor does NOT clear is asserted to be still 0xa5: that is
 * what tells "left as allocated" apart from "cleared by both sides".
 */

#include <string.h>

#include "harness.h"
#include "dsplib/DspMath.h"
#include "dsplib/Psd.h"
#include "dsplib/sysdep.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90SdDetector.h"
#include "dsplib/V90SpectralShaper.h"
#include "dsplib/V90SpectralShapingFilter.h"
#include "dsplib/V90SpectralVerifier.h"

extern "C" {
void our_ssf_ctor(void *self) asm("_ZN24V90SpectralShapingFilterC1Ev");
void our_ssf_ctor2(void *self) asm("_ZN24V90SpectralShapingFilterC2Ev");
void ref_ssf_ctor(void *self) asm("ref__ZN24V90SpectralShapingFilterC1Ev");
void ref_ssf_ctor2(void *self) asm("ref__ZN24V90SpectralShapingFilterC2Ev");

void our_sd_ctor(void *self, unsigned a, unsigned b, unsigned c, unsigned n)
	asm("_ZN13V90SdDetectorC1Efffj");
void our_sd_ctor2(void *self, unsigned a, unsigned b, unsigned c, unsigned n)
	asm("_ZN13V90SdDetectorC2Efffj");
void ref_sd_ctor(void *self, unsigned a, unsigned b, unsigned c, unsigned n)
	asm("ref__ZN13V90SdDetectorC1Efffj");
void ref_sd_ctor2(void *self, unsigned a, unsigned b, unsigned c, unsigned n)
	asm("ref__ZN13V90SdDetectorC2Efffj");
void our_sd_dtor(void *self) asm("_ZN13V90SdDetectorD1Ev");
void our_sd_dtor2(void *self) asm("_ZN13V90SdDetectorD2Ev");
void ref_sd_dtor(void *self) asm("ref__ZN13V90SdDetectorD1Ev");
void ref_sd_dtor2(void *self) asm("ref__ZN13V90SdDetectorD2Ev");
void ref_sd_reset(void *self) asm("ref__ZN13V90SdDetector5resetEv");

void our_sv_ctor(void *self, void *params)
	asm("_ZN19V90SpectralVerifierC1EP13V90Parameters");
void our_sv_ctor2(void *self, void *params)
	asm("_ZN19V90SpectralVerifierC2EP13V90Parameters");
void ref_sv_ctor(void *self, void *params)
	asm("ref__ZN19V90SpectralVerifierC1EP13V90Parameters");
void ref_sv_ctor2(void *self, void *params)
	asm("ref__ZN19V90SpectralVerifierC2EP13V90Parameters");
void our_sv_dtor(void *self) asm("_ZN19V90SpectralVerifierD1Ev");
void our_sv_dtor2(void *self) asm("_ZN19V90SpectralVerifierD2Ev");
void ref_sv_dtor(void *self) asm("ref__ZN19V90SpectralVerifierD1Ev");
void ref_sv_dtor2(void *self) asm("ref__ZN19V90SpectralVerifierD2Ev");
void ref_sv_reset(void *self) asm("ref__ZN19V90SpectralVerifier5resetEv");

void our_ss_ctor(void *self) asm("_ZN17V90SpectralShaperC1Ev");
void our_ss_ctor2(void *self) asm("_ZN17V90SpectralShaperC2Ev");
void ref_ss_ctor(void *self) asm("ref__ZN17V90SpectralShaperC1Ev");
void ref_ss_ctor2(void *self) asm("ref__ZN17V90SpectralShaperC2Ev");
void our_ss_dtor(void *self) asm("_ZN17V90SpectralShaperD1Ev");
void our_ss_dtor2(void *self) asm("_ZN17V90SpectralShaperD2Ev");
void ref_ss_dtor(void *self) asm("ref__ZN17V90SpectralShaperD1Ev");
void ref_ss_dtor2(void *self) asm("ref__ZN17V90SpectralShaperD2Ev");
}

/* ------------------------------------------------------------ plumbing */

static unsigned lfsr_state;

static unsigned
lfsr(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return lfsr_state;
}

/* Never zero: a zero fill makes a constructor that writes nothing look right. */
static void
seed_pair(unsigned char *a, unsigned char *b, int n, int trial)
{
	int i;

	lfsr_state = 0x4d7bu + 0x9e37u * (unsigned)trial;
	for (i = 0; i < n; i++) {
		unsigned char v = (unsigned char)((lfsr() >> 3) | 1u);

		a[i] = v;
		b[i] = v;
	}
}

static unsigned
w32(const void *p, int off)
{
	unsigned u;

	memcpy(&u, (const unsigned char *)p + off, sizeof(u));
	return u;
}

static unsigned
fbits(float f)
{
	unsigned u;

	memcpy(&u, &f, sizeof(u));
	return u;
}

static float
fromhex(unsigned u)
{
	float f;

	memcpy(&f, &u, sizeof(f));
	return f;
}

/* The bytes past an object must be the ones it was seeded with. */
static void
guard_intact(const unsigned char *a, const unsigned char *b, int from, int to,
	     int trial)
{
	diff_eq_int("nothing stored past the object (trial %ld)",
		    memcmp(a + from, b + from, to - from) == 0, 1, trial);
}

/* Every byte of a buffer is still the allocator's fill. */
static int
all_fill(const void *p, unsigned n)
{
	const unsigned char *q = (const unsigned char *)p;
	unsigned i;

	for (i = 0; i < n; i++)
		if (q[i] != HARNESS_MALLOC_FILL)
			return 0;
	return 1;
}

static int
all_zero(const void *p, unsigned n)
{
	const unsigned char *q = (const unsigned char *)p;
	unsigned i;

	for (i = 0; i < n; i++)
		if (q[i] != 0)
			return 0;
	return 1;
}

/* ------------------------------------ V90SpectralShapingFilter (36 bytes) */

#define SSF_SLOT 64

static unsigned char ssf_a[SSF_SLOT] __attribute__((aligned(8)));
static unsigned char ssf_b[SSF_SLOT] __attribute__((aligned(8)));

static int
run_ssf(void)
{
	int trial;

	diff_begin("V90SpectralShapingFilter::V90SpectralShapingFilter");

	for (trial = 0; trial < 8; trial++) {
		int i;

		seed_pair(ssf_a, ssf_b, SSF_SLOT, trial);

		if (trial & 1) {
			our_ssf_ctor2(ssf_a);
			ref_ssf_ctor2(ssf_b);
		} else {
			our_ssf_ctor(ssf_a);
			ref_ssf_ctor(ssf_b);
		}

		diff_eq_obj("after construction", V90SpectralShapingFilter,
			    ssf_a, ssf_b, trial);
		guard_intact(ssf_a, ssf_b, sizeof(V90SpectralShapingFilter),
			     SSF_SLOT, trial);

		/*
		 * The write set by absolute offset, on the BLOB's object as
		 * well as ours: eight zeroes and one 2, over a seed that was
		 * neither.
		 */
		for (i = 0; i < 8; i++) {
			diff_eq_int("blob: word +0x%02lx is zero",
				    w32(ssf_b, i * 4), 0u, i * 4);
			diff_eq_int("ours: word +0x%02lx is zero",
				    w32(ssf_a, i * 4), 0u, i * 4);
		}
		diff_eq_int("blob: +0x20 is 2 (trial %ld)", w32(ssf_b, 0x20),
			    2, trial);
		diff_eq_int("ours: +0x20 is 2 (trial %ld)", w32(ssf_a, 0x20),
			    2, trial);
	}

	return diff_end();
}

/* --------------------------------------------- V90SdDetector (28 bytes) */

#define SD_SLOT		64
#define SD_HIST		12u
#define SD_HIST_BYTES	(SD_HIST * sizeof(float))

static unsigned char sd_a[SD_SLOT] __attribute__((aligned(8)));
static unsigned char sd_b[SD_SLOT] __attribute__((aligned(8)));

/*
 * Distinct bit patterns, and every one of them is here for a reason: both
 * zeroes, both signs, a denormal of each sign, a quiet NaN, FLT_MAX and two
 * ordinary values.  Every one of these survives BOTH a bit copy and an x87
 * round trip unchanged, which is what makes them usable.
 *
 * A SIGNALLING NaN IS NOT HERE, AND ITS ABSENCE IS THE MEASUREMENT.
 * 0x7fa00000 is the one pattern the two spellings disagree on, and it does
 * disagree: the blob's constructor is three `mov`s and keeps the payload,
 * ours is three `flds`/`fstps` pairs -- the modern toolchain's SFmode move
 * under `-mfpmath=387` -- and quietens it to 0x7fe00000.  The source is not
 * what differs; a bit-copy spelling that forced `mov` would be fitting the
 * compiler, which this tree does not do.  Finding 1242 has the evidence and
 * the bound: no other pattern separates the two, so nothing an audio path
 * can carry does.
 */
static const unsigned sd_pat[] = {
	0x00000000u,	/* +0                        */
	0x80000000u,	/* -0                        */
	0x3f800000u,	/* 1.0f                      */
	0xbf800000u,	/* -1.0f                     */
	0x00000001u,	/* smallest positive denormal */
	0x807fffffu,	/* largest negative denormal  */
	0x7f7fffffu,	/* FLT_MAX                   */
	0x7fc00000u,	/* quiet NaN                 */
	0x3dcccccdu,	/* 0.1f                      */
	0x4b800001u	/* 16777218.0f               */
};
#define SD_NPAT ((int)(sizeof(sd_pat) / sizeof(sd_pat[0])))

static const unsigned sd_lim[] = {
	0u, 1u, 11u, 12u, 13u, 0xffffffffu, 0x80000000u
};
#define SD_NLIM ((int)(sizeof(sd_lim) / sizeof(sd_lim[0])))

static void
sd_snapshot(void *dst, const unsigned char *src)
{
	V90SdDetector *d = (V90SdDetector *)dst;

	memcpy(dst, src, sizeof(V90SdDetector));
	d->history = (float *)(long)(((const V90SdDetector *)src)->history
				     != 0);
}

static int
run_sd_ctor(void)
{
	int trial;

	diff_begin("V90SdDetector::V90SdDetector / ~V90SdDetector");

	for (trial = 0; trial < SD_NPAT * SD_NLIM; trial++) {
		unsigned a = sd_pat[trial % SD_NPAT];
		unsigned b = sd_pat[(trial + 1) % SD_NPAT];
		unsigned c = sd_pat[(trial + 2) % SD_NPAT];
		unsigned n = sd_lim[trial % SD_NLIM];
		unsigned char sa[sizeof(V90SdDetector)];
		unsigned char sb[sizeof(V90SdDetector)];
		const float *ha, *hb;

		harness_alloc_reset();
		seed_pair(sd_a, sd_b, SD_SLOT, trial);

		if (trial & 1) {
			our_sd_ctor2(sd_a, a, b, c, n);
			ref_sd_ctor2(sd_b, a, b, c, n);
		} else {
			our_sd_ctor(sd_a, a, b, c, n);
			ref_sd_ctor(sd_b, a, b, c, n);
		}

		sd_snapshot(sa, sd_a);
		sd_snapshot(sb, sd_b);
		diff_eq_obj_(__FILE__, __LINE__, "after construction",
			     "V90SdDetector", sa, sb, sizeof(V90SdDetector),
			     (long)trial);
		guard_intact(sd_a, sd_b, sizeof(V90SdDetector), SD_SLOT,
			     trial);

		/*
		 * Each argument at the offset it belongs at, on the BLOB's
		 * object as well as ours, as a bit pattern: this is what
		 * pins arguments 3 and 4 to +0x10 and +0x04 rather than to
		 * each other's slots.
		 */
		diff_eq_int("blob: +0x08 is argument 1 (%#lx)",
			    w32(sd_b, 0x08), a, a);
		diff_eq_int("blob: +0x0c is argument 2 (%#lx)",
			    w32(sd_b, 0x0c), b, b);
		diff_eq_int("blob: +0x10 is argument 3 (%#lx)",
			    w32(sd_b, 0x10), c, c);
		diff_eq_int("blob: +0x04 is argument 4 (%#lx)",
			    w32(sd_b, 0x04), n, n);
		diff_eq_int("ours: +0x08 is argument 1 (%#lx)",
			    w32(sd_a, 0x08), a, a);
		diff_eq_int("ours: +0x0c is argument 2 (%#lx)",
			    w32(sd_a, 0x0c), b, b);
		diff_eq_int("ours: +0x10 is argument 3 (%#lx)",
			    w32(sd_a, 0x10), c, c);
		diff_eq_int("ours: +0x04 is argument 4 (%#lx)",
			    w32(sd_a, 0x04), n, n);

		/* The length and the allocation are 12 and 0x30 regardless. */
		diff_eq_int("blob: historyLength is 12, argument 4 is %#lx",
			    w32(sd_b, 0x18), SD_HIST, n);
		diff_eq_int("ours: historyLength is 12, argument 4 is %#lx",
			    w32(sd_a, 0x18), SD_HIST, n);
		diff_eq_int("one allocation each, argument 4 is %#lx",
			    harness_alloc.allocs, 2, n);
		diff_eq_int("0x30 bytes each, argument 4 is %#lx",
			    harness_alloc.bytes, 2 * SD_HIST_BYTES, n);

		ha = ((const V90SdDetector *)sd_a)->history;
		hb = ((const V90SdDetector *)sd_b)->history;
		diff_eq_int("blob: the history is cleared (trial %ld)",
			    all_zero(hb, SD_HIST_BYTES), 1, trial);
		diff_eq_int("ours: the history is cleared (trial %ld)",
			    all_zero(ha, SD_HIST_BYTES), 1, trial);

		if (trial & 1) {
			our_sd_dtor2(sd_a);
			ref_sd_dtor2(sd_b);
		} else {
			our_sd_dtor(sd_a);
			ref_sd_dtor(sd_b);
		}

		diff_eq_int("the destructor freed both (trial %ld)",
			    harness_alloc.frees, 2, trial);
		diff_eq_int("nothing left live (trial %ld)",
			    harness_alloc.live, 0, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);
		/*
		 * "and the destructor does not null what it freed" is NOT
		 * asserted here.  A store to a member in a destructor is
		 * dead by construction and `-flifetime-dse` deletes it, so
		 * the check could not fail whatever the source said; the
		 * measurement is in test/mutations/v90sddet.json.
		 */
	}

	/* The destructor's null arm, which no constructed object reaches. */
	{
		harness_alloc_reset();
		seed_pair(sd_a, sd_b, SD_SLOT, 99);
		our_sd_ctor(sd_a, sd_pat[0], sd_pat[1], sd_pat[2], 4u);
		ref_sd_ctor(sd_b, sd_pat[0], sd_pat[1], sd_pat[2], 4u);
		sysdep_free(((V90SdDetector *)sd_a)->history);
		sysdep_free(((V90SdDetector *)sd_b)->history);
		((V90SdDetector *)sd_a)->history = 0;
		((V90SdDetector *)sd_b)->history = 0;
		our_sd_dtor(sd_a);
		ref_sd_dtor(sd_b);
		diff_eq_int("a null history is not freed (%ld)",
			    harness_alloc.frees, 2, 0);
		diff_eq_int("no free(NULL) (%ld)", harness_alloc.free_null, 0,
			    0);
		diff_eq_int("no bad free (%ld)", harness_alloc.bad_free, 0, 0);
	}

	return diff_end();
}

/*
 * `reset()` over a CONSTRUCTED object, with the loop bound moved off 12 so
 * that the bound really is read from the field.
 */
static int
run_sd_reset(void)
{
	static const unsigned lens[] = { 0u, 1u, 5u, 12u };
	int trial;

	diff_begin("V90SdDetector::reset after construction");

	for (trial = 0; trial < 4 * 4; trial++) {
		unsigned len = lens[trial % 4];
		unsigned char sa[sizeof(V90SdDetector)];
		unsigned char sb[sizeof(V90SdDetector)];
		float *ha, *hb;
		unsigned i;

		harness_alloc_reset();
		seed_pair(sd_a, sd_b, SD_SLOT, trial + 200);
		our_sd_ctor(sd_a, sd_pat[trial % SD_NPAT],
			    sd_pat[(trial + 3) % SD_NPAT],
			    sd_pat[(trial + 5) % SD_NPAT], 7u);
		ref_sd_ctor(sd_b, sd_pat[trial % SD_NPAT],
			    sd_pat[(trial + 3) % SD_NPAT],
			    sd_pat[(trial + 5) % SD_NPAT], 7u);

		ha = ((V90SdDetector *)sd_a)->history;
		hb = ((V90SdDetector *)sd_b)->history;
		for (i = 0; i < SD_HIST; i++) {
			unsigned v = 0x40000000u + (i << 8) + (unsigned)trial;

			ha[i] = fromhex(v);
			hb[i] = fromhex(v);
		}
		((V90SdDetector *)sd_a)->historyLength = len;
		((V90SdDetector *)sd_b)->historyLength = len;
		((V90SdDetector *)sd_a)->count = 0x5a5au + (unsigned)trial;
		((V90SdDetector *)sd_b)->count = 0x5a5au + (unsigned)trial;

		((V90SdDetector *)sd_a)->reset();
		ref_sd_reset(sd_b);

		sd_snapshot(sa, sd_a);
		sd_snapshot(sb, sd_b);
		diff_eq_obj_(__FILE__, __LINE__, "after reset",
			     "V90SdDetector", sa, sb, sizeof(V90SdDetector),
			     (long)trial);
		diff_eq_obj_(__FILE__, __LINE__, "history after reset",
			     "float", ha, hb, SD_HIST_BYTES, (long)trial);
		diff_eq_int("blob: %ld words cleared and no more",
			    all_zero(hb, len * sizeof(float))
			    && (len == SD_HIST
				|| !all_zero(hb + len,
					     (SD_HIST - len) * sizeof(float))),
			    1, len);

		our_sd_dtor(sd_a);
		ref_sd_dtor(sd_b);
	}

	return diff_end();
}

/* --------------------------------------- V90SpectralVerifier (44 bytes) */

#define SV_SLOT		96
#define PARM_SLOT	0x600

static unsigned char sv_a[SV_SLOT] __attribute__((aligned(8)));
static unsigned char sv_b[SV_SLOT] __attribute__((aligned(8)));

/*
 * ONE parameter block, shared: two separately seeded blocks would agree
 * whatever the constructor read out of them.
 */
static unsigned char parm[PARM_SLOT] __attribute__((aligned(8)));

/* fftLen, psdLen, window, overlap; no two equal, so no slot can stand in
 * for another. */
static const unsigned sv_shape[][4] = {
	{ 8u,   4u,  0u, 3u },
	{ 16u,  7u,  1u, 5u },
	{ 33u,  40u, 2u, 9u },
	{ 64u,  20u, 3u, 17u },
	{ 128u, 6u,  1u, 61u },
	{ 2u,   1u,  2u, 44u }
};
#define SV_NSHAPE ((int)(sizeof(sv_shape) / sizeof(sv_shape[0])))

static const unsigned sv_freq[] = {
	0x45fa0000u,	/* 8000.0f  */
	0x45e10000u,	/* 7200.0f  */
	0x46160000u,	/* 9600.0f  */
	0x3f800000u,	/* 1.0f     */
	0xc61c4000u	/* -10000.0f */
};
#define SV_NFREQ ((int)(sizeof(sv_freq) / sizeof(sv_freq[0])))

static void
sv_snapshot(void *dst, const unsigned char *src)
{
	const V90SpectralVerifier *s = (const V90SpectralVerifier *)src;
	V90SpectralVerifier *d = (V90SpectralVerifier *)dst;

	memcpy(dst, src, sizeof(V90SpectralVerifier));
	d->psd = (Psd *)(long)(s->psd != 0);
	d->buf_18 = (float *)(long)(s->buf_18 != 0);
	d->spectrum = (float *)(long)(s->spectrum != 0);
}

static void
psd_snapshot(void *dst, const Psd *src)
{
	Psd *d = (Psd *)dst;

	memcpy(dst, src, sizeof(Psd));
	d->m_window = (float *)(long)(src->m_window != 0);
	d->m_fft = (float *)(long)(src->m_fft != 0);
}

static int
run_sv(void)
{
	int trial;

	diff_begin("V90SpectralVerifier::V90SpectralVerifier / ~ / reset");

	for (trial = 0; trial < SV_NSHAPE * SV_NFREQ; trial++) {
		const unsigned *sh = sv_shape[trial % SV_NSHAPE];
		unsigned fftLen = sh[0], psdLen = sh[1];
		unsigned wtype = sh[2], overlap = sh[3];
		unsigned freq = sv_freq[trial % SV_NFREQ];
		V90Parameters *p = (V90Parameters *)parm;
		unsigned char sa[sizeof(V90SpectralVerifier)];
		unsigned char sb[sizeof(V90SpectralVerifier)];
		unsigned char pa[sizeof(Psd)], pb[sizeof(Psd)];
		const V90SpectralVerifier *va, *vb;
		unsigned seed20, seed24, want;
		float q;

		harness_alloc_reset();
		seed_pair(parm, parm, PARM_SLOT, trial + 400);
		p->SPECTRAL_VERIFIER_SAMPLE_FREQ = fromhex(freq);
		p->SPECTRAL_VERIFIER_FFT_LEN = (int)fftLen;
		p->SPECTRAL_VERIFIER_FFT_WINDOW = (int)wtype;
		p->SPECTRAL_VERIFIER_PSD_LEN = (int)psdLen;
		p->SPECTRAL_VERIFIER_PSD_OVERLAP_LEN = (int)overlap;

		seed_pair(sv_a, sv_b, SV_SLOT, trial);
		seed20 = w32(sv_a, 0x20);
		seed24 = w32(sv_a, 0x24);

		if (trial & 1) {
			our_sv_ctor2(sv_a, parm);
			ref_sv_ctor2(sv_b, parm);
		} else {
			our_sv_ctor(sv_a, parm);
			ref_sv_ctor(sv_b, parm);
		}

		va = (const V90SpectralVerifier *)sv_a;
		vb = (const V90SpectralVerifier *)sv_b;

		sv_snapshot(sa, sv_a);
		sv_snapshot(sb, sv_b);
		diff_eq_obj_(__FILE__, __LINE__, "after construction",
			     "V90SpectralVerifier", sa, sb,
			     sizeof(V90SpectralVerifier), (long)trial);
		guard_intact(sv_a, sv_b, sizeof(V90SpectralVerifier), SV_SLOT,
			     trial);

		/* Every word the constructor writes, by absolute offset. */
		diff_eq_int("blob: +0x00 is the argument (trial %ld)",
			    w32(sv_b, 0x00) == (unsigned)(long)parm, 1, trial);
		diff_eq_int("blob: +0x08 is SAMPLE_FREQ (%#lx)",
			    w32(sv_b, 0x08), freq, freq);
		diff_eq_int("blob: +0x0c is FFT_LEN (%ld)", w32(sv_b, 0x0c),
			    fftLen, fftLen);
		diff_eq_int("blob: +0x10 is PSD_LEN (%ld)", w32(sv_b, 0x10),
			    psdLen, psdLen);
		diff_eq_int("ours: +0x08 is SAMPLE_FREQ (%#lx)",
			    w32(sv_a, 0x08), freq, freq);
		diff_eq_int("ours: +0x0c is FFT_LEN (%ld)", w32(sv_a, 0x0c),
			    fftLen, fftLen);
		diff_eq_int("ours: +0x10 is PSD_LEN (%ld)", w32(sv_a, 0x10),
			    psdLen, psdLen);

		/*
		 * The quotient, against a value this file computes itself:
		 * side-against-side cannot tell a divide from its reciprocal
		 * and the object's `de f9` is the one place finding 245's
		 * mnemonic trap bites.
		 */
		q = fromhex(freq) / (float)fftLen;
		diff_eq_int("blob: +0x14 is SAMPLE_FREQ / FFT_LEN (%#lx)",
			    w32(sv_b, 0x14), fbits(q), fbits(q));
		diff_eq_int("ours: +0x14 is SAMPLE_FREQ / FFT_LEN (%#lx)",
			    w32(sv_a, 0x14), fbits(q), fbits(q));

		/* The two words the constructor does NOT write. */
		diff_eq_int("blob: +0x20 is untouched (trial %ld)",
			    w32(sv_b, 0x20), seed20, trial);
		diff_eq_int("blob: +0x24 is untouched (trial %ld)",
			    w32(sv_b, 0x24), seed24, trial);
		diff_eq_int("blob: +0x28 is zero (trial %ld)",
			    w32(sv_b, 0x28), 0u, trial);
		diff_eq_int("ours: +0x28 is zero (trial %ld)",
			    w32(sv_a, 0x28), 0u, trial);

		/* Five allocations a side, and every size is pinned. */
		diff_eq_int("five allocations a side (trial %ld)",
			    harness_alloc.allocs, 10, trial);
		want = (unsigned)(psdLen * sizeof(float)
				  + (fftLen / 2) * sizeof(float)
				  + sizeof(Psd)
				  + fftLen * sizeof(float)
				  + (fftLen + 1) * sizeof(float));
		diff_eq_int("the bytes are pinned (trial %ld)",
			    harness_alloc.bytes, 2 * want, trial);

		/* Neither buffer is cleared: still the allocator's fill. */
		diff_eq_int("blob: +0x18 is left as allocated (trial %ld)",
			    all_fill(vb->buf_18,
				     (unsigned)(psdLen * sizeof(float))), 1,
			    trial);
		diff_eq_int("blob: the spectrum is left as allocated (%ld)",
			    all_fill(vb->spectrum,
				     (unsigned)((fftLen / 2) * sizeof(float))),
			    1, trial);
		diff_eq_int("ours: +0x18 is left as allocated (trial %ld)",
			    all_fill(va->buf_18,
				     (unsigned)(psdLen * sizeof(float))), 1,
			    trial);
		diff_eq_int("ours: the spectrum is left as allocated (%ld)",
			    all_fill(va->spectrum,
				     (unsigned)((fftLen / 2) * sizeof(float))),
			    1, trial);

		/* The Psd, and the three arguments it was given. */
		psd_snapshot(pa, va->psd);
		psd_snapshot(pb, vb->psd);
		diff_eq_obj_(__FILE__, __LINE__, "the Psd", "Psd", pa, pb,
			     sizeof(Psd), (long)trial);
		diff_eq_int("blob: the Psd's length is FFT_LEN (%ld)",
			    vb->psd->m_length, fftLen, fftLen);
		diff_eq_int("blob: the Psd's overlap is OVERLAP_LEN (%ld)",
			    vb->psd->m_overlap, overlap, overlap);
		diff_eq_obj_(__FILE__, __LINE__, "the Psd's window", "float",
			     va->psd->m_window, vb->psd->m_window,
			     fftLen * sizeof(float), (long)trial);

		/*
		 * The window really is the type the parameter block asked
		 * for, checked against a freshly designed one: two sides
		 * passing the same WRONG slot would agree for ever.
		 */
		{
			static float want_w[130];
			unsigned k;
			int same = 1;

			designWindow((WindowType)wtype, want_w, fftLen);
			for (k = 0; k < fftLen; k++)
				if (fbits(vb->psd->m_window[k])
				    != fbits(want_w[k]))
					same = 0;
			diff_eq_int("blob: the window is FFT_WINDOW (%ld)",
				    same, 1, wtype);
		}

		/* reset(), over the object the constructor just built. */
		((V90SpectralVerifier *)sv_a)->reset();
		ref_sv_reset(sv_b);
		sv_snapshot(sa, sv_a);
		sv_snapshot(sb, sv_b);
		diff_eq_obj_(__FILE__, __LINE__, "after reset",
			     "V90SpectralVerifier", sa, sb,
			     sizeof(V90SpectralVerifier), (long)trial);
		diff_eq_int("blob: reset cleared +0x20 (trial %ld)",
			    w32(sv_b, 0x20), 0u, trial);
		diff_eq_int("blob: reset cleared +0x24 (trial %ld)",
			    w32(sv_b, 0x24), 0u, trial);

		if (trial & 1) {
			our_sv_dtor2(sv_a);
			ref_sv_dtor2(sv_b);
		} else {
			our_sv_dtor(sv_a);
			ref_sv_dtor(sv_b);
		}

		diff_eq_int("the destructor freed all five (trial %ld)",
			    harness_alloc.frees, 10, trial);
		diff_eq_int("nothing left live (trial %ld)",
			    harness_alloc.live, 0, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);
	}

	/* The destructor's three null arms, one at a time. */
	{
		int which;

		for (which = 0; which < 3; which++) {
			V90Parameters *p = (V90Parameters *)parm;
			V90SpectralVerifier *x = (V90SpectralVerifier *)sv_a;
			V90SpectralVerifier *y = (V90SpectralVerifier *)sv_b;

			harness_alloc_reset();
			seed_pair(parm, parm, PARM_SLOT, 900 + which);
			p->SPECTRAL_VERIFIER_SAMPLE_FREQ = 8000.0f;
			p->SPECTRAL_VERIFIER_FFT_LEN = 16;
			p->SPECTRAL_VERIFIER_FFT_WINDOW = 1;
			p->SPECTRAL_VERIFIER_PSD_LEN = 5;
			p->SPECTRAL_VERIFIER_PSD_OVERLAP_LEN = 3;
			seed_pair(sv_a, sv_b, SV_SLOT, 900 + which);
			our_sv_ctor(sv_a, parm);
			ref_sv_ctor(sv_b, parm);

			if (which == 0) {
				sysdep_free(x->buf_18);
				sysdep_free(y->buf_18);
				x->buf_18 = 0;
				y->buf_18 = 0;
			} else if (which == 1) {
				sysdep_free(x->spectrum);
				sysdep_free(y->spectrum);
				x->spectrum = 0;
				y->spectrum = 0;
			} else {
				sysdep_free(x->psd->m_window);
				sysdep_free(y->psd->m_window);
				sysdep_free(x->psd->m_fft);
				sysdep_free(y->psd->m_fft);
				sysdep_free(x->psd);
				sysdep_free(y->psd);
				x->psd = 0;
				y->psd = 0;
			}

			our_sv_dtor(sv_a);
			ref_sv_dtor(sv_b);
			diff_eq_int("member %ld freed by hand, the rest by"
				    " the destructor", harness_alloc.live, 0,
				    which);
			diff_eq_int("no bad free (member %ld)",
				    harness_alloc.bad_free, 0, which);
			diff_eq_int("no free(NULL) (member %ld)",
				    harness_alloc.free_null, 0, which);
		}
	}

	return diff_end();
}

/* --------------------------------------- V90SpectralShaper (108 bytes) */

#define SS_SLOT		160
#define SS_BUF_BYTES	48u
#define SS_PDE_SIZE	6u

static unsigned char ss_a[SS_SLOT] __attribute__((aligned(8)));
static unsigned char ss_b[SS_SLOT] __attribute__((aligned(8)));

static void
ss_snapshot(void *dst, const unsigned char *src)
{
	const V90SpectralShaper *s = (const V90SpectralShaper *)src;
	V90SpectralShaper *d = (V90SpectralShaper *)dst;

	memcpy(dst, src, sizeof(V90SpectralShaper));
	d->buf_28 = (unsigned short *)(long)(s->buf_28 != 0);
	d->buf_2c = (unsigned short *)(long)(s->buf_2c != 0);
	d->pde.state_ = (unsigned char *)(long)(s->pde.state_ != 0);
}

static int
run_ss(void)
{
	int trial;

	diff_begin("V90SpectralShaper::V90SpectralShaper / ~");

	for (trial = 0; trial < 8; trial++) {
		unsigned char sa[sizeof(V90SpectralShaper)];
		unsigned char sb[sizeof(V90SpectralShaper)];
		const V90SpectralShaper *xa, *xb;
		int i;

		harness_alloc_reset();
		seed_pair(ss_a, ss_b, SS_SLOT, trial + 600);

		if (trial & 1) {
			our_ss_ctor2(ss_a);
			ref_ss_ctor2(ss_b);
		} else {
			our_ss_ctor(ss_a);
			ref_ss_ctor(ss_b);
		}

		xa = (const V90SpectralShaper *)ss_a;
		xb = (const V90SpectralShaper *)ss_b;

		ss_snapshot(sa, ss_a);
		ss_snapshot(sb, ss_b);
		diff_eq_obj_(__FILE__, __LINE__, "after construction",
			     "V90SpectralShaper", sa, sb,
			     sizeof(V90SpectralShaper), (long)trial);
		guard_intact(ss_a, ss_b, sizeof(V90SpectralShaper), SS_SLOT,
			     trial);

		/* The write set by absolute offset, blob and ours. */
		diff_eq_int("blob: +0x20 is zero (trial %ld)",
			    w32(ss_b, 0x20), 0u, trial);
		diff_eq_int("blob: +0x30 is zero (trial %ld)",
			    w32(ss_b, 0x30), 0u, trial);
		diff_eq_int("blob: +0x34 is 24 (trial %ld)", w32(ss_b, 0x34),
			    24u, trial);
		diff_eq_int("blob: +0x38 is zero (trial %ld)",
			    ss_b[0x38], 0, trial);
		diff_eq_int("ours: +0x20 is zero (trial %ld)",
			    w32(ss_a, 0x20), 0u, trial);
		diff_eq_int("ours: +0x30 is zero (trial %ld)",
			    w32(ss_a, 0x30), 0u, trial);
		diff_eq_int("ours: +0x34 is 24 (trial %ld)", w32(ss_a, 0x34),
			    24u, trial);
		diff_eq_int("ours: +0x38 is zero (trial %ld)",
			    ss_a[0x38], 0, trial);

		/* The encoder at +0x3c, built with 6. */
		diff_eq_int("blob: the encoder's capacity is 6 (trial %ld)",
			    w32(ss_b, 0x40), SS_PDE_SIZE, trial);
		diff_eq_int("blob: the encoder's size is 0 (trial %ld)",
			    w32(ss_b, 0x44), 0u, trial);
		diff_eq_int("ours: the encoder's capacity is 6 (trial %ld)",
			    w32(ss_a, 0x40), SS_PDE_SIZE, trial);
		diff_eq_int("ours: the encoder's size is 0 (trial %ld)",
			    w32(ss_a, 0x44), 0u, trial);
		diff_eq_int("blob: the encoder's state is cleared (%ld)",
			    all_zero(xb->pde.state_, SS_PDE_SIZE), 1, trial);

		/* The filter at +0x48: eight zeroes and a 2. */
		for (i = 0; i < 8; i++) {
			diff_eq_int("blob: the filter's +0x%02lx is zero",
				    w32(ss_b, 0x48 + i * 4), 0u, i * 4);
			diff_eq_int("ours: the filter's +0x%02lx is zero",
				    w32(ss_a, 0x48 + i * 4), 0u, i * 4);
		}
		diff_eq_int("blob: the filter's +0x20 is 2 (trial %ld)",
			    w32(ss_b, 0x68), 2u, trial);
		diff_eq_int("ours: the filter's +0x20 is 2 (trial %ld)",
			    w32(ss_a, 0x68), 2u, trial);

		/* Three allocations a side, and neither buffer is cleared. */
		diff_eq_int("three allocations a side (trial %ld)",
			    harness_alloc.allocs, 6, trial);
		diff_eq_int("the bytes are pinned (trial %ld)",
			    harness_alloc.bytes,
			    2 * (2 * SS_BUF_BYTES + SS_PDE_SIZE), trial);
		diff_eq_int("blob: +0x28 is left as allocated (trial %ld)",
			    all_fill(xb->buf_28, SS_BUF_BYTES), 1, trial);
		diff_eq_int("blob: +0x2c is left as allocated (trial %ld)",
			    all_fill(xb->buf_2c, SS_BUF_BYTES), 1, trial);
		diff_eq_int("ours: +0x28 is left as allocated (trial %ld)",
			    all_fill(xa->buf_28, SS_BUF_BYTES), 1, trial);
		diff_eq_int("ours: +0x2c is left as allocated (trial %ld)",
			    all_fill(xa->buf_2c, SS_BUF_BYTES), 1, trial);

		if (trial & 1) {
			our_ss_dtor2(ss_a);
			ref_ss_dtor2(ss_b);
		} else {
			our_ss_dtor(ss_a);
			ref_ss_dtor(ss_b);
		}

		diff_eq_int("the destructor freed all three (trial %ld)",
			    harness_alloc.frees, 6, trial);
		diff_eq_int("nothing left live (trial %ld)",
			    harness_alloc.live, 0, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);
	}

	/* The destructor's two null arms. */
	{
		int which;

		for (which = 0; which < 2; which++) {
			V90SpectralShaper *x = (V90SpectralShaper *)ss_a;
			V90SpectralShaper *y = (V90SpectralShaper *)ss_b;

			harness_alloc_reset();
			seed_pair(ss_a, ss_b, SS_SLOT, 700 + which);
			our_ss_ctor(ss_a);
			ref_ss_ctor(ss_b);

			if (which == 0) {
				sysdep_free(x->buf_28);
				sysdep_free(y->buf_28);
				x->buf_28 = 0;
				y->buf_28 = 0;
			} else {
				sysdep_free(x->buf_2c);
				sysdep_free(y->buf_2c);
				x->buf_2c = 0;
				y->buf_2c = 0;
			}

			our_ss_dtor(ss_a);
			ref_ss_dtor(ss_b);
			diff_eq_int("member %ld freed by hand, the rest by"
				    " the destructor", harness_alloc.live, 0,
				    which);
			diff_eq_int("no bad free (member %ld)",
				    harness_alloc.bad_free, 0, which);
			diff_eq_int("no free(NULL) (member %ld)",
				    harness_alloc.free_null, 0, which);
		}
	}

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_ssf();
	bad |= run_sd_ctor();
	bad |= run_sd_reset();
	bad |= run_sv();
	bad |= run_ss();

	return bad;
}

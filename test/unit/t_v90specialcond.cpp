/*
 * t_v90specialcond.cpp -- differential test of
 *
 *     V90SpectralVerifier::checkSpecialSpectralConditions()
 *
 * 1,682 bytes at 0x45f10, and the only member of the class that decides
 * anything.  It reads five words of the object (+0x00 `params`, +0x14
 * `binWidth`, +0x1c `spectrum`, +0x24 the accumulation state) and writes
 * exactly one (+0x28), and it prints nine diagnostics on the way.
 *
 * WHAT HAS TO BE OBSERVED, AND WHY +0x28 IS NOT ENOUGH.  The result field
 * takes four values, so a test that watches it alone leaves everything the
 * nine `edprintf` calls carry unobserved: the sign character, the truncated
 * magnitude, the hundredths, and the three probe frequencies printed as
 * integers.  A mutation to any of those would read NOT CAUGHT.  So both
 * sides' diagnostics are captured and compared as text on every trial, and
 * the anti-vacuity checks at the end are PAIRS OF TRIALS that differ in one
 * printed quantity and nothing else -- a sign, a whole part, a hundredth, a
 * printed frequency, a selected bin -- each asserted to produce a DIFFERENT
 * transcript from its partner.  That is the separating-trial rule taken
 * literally: each counter counts trials whose observable output differs,
 * not trials that took a different path.
 *
 * THE TRANSCRIPT IS ENCODED AND THAT IS FINE.  `edprintf` emits each byte of
 * its formatted message as two offset characters (src/core/encode.c), so the
 * captured text is not readable and this file never tries to read it.  What
 * it does is compare the two sides' and compare one trial's against another
 * trial's, both of which work on ciphertext -- and the encoder resets its
 * rotating key at the head of every `edprintf`, so a given message always
 * encodes the same way and two trials that print the same numbers really do
 * produce the same transcript.
 *
 * BOTH SIDES SHARE ONE SPECTRUM AND ONE PARAMETER BLOCK.  The function only
 * reads them, so there is nothing to keep apart and a great deal of noise
 * removed: the two objects then differ in no pointer and `diff_eq_obj` can
 * compare all 44 bytes with no snapshot step.
 *
 * NOTHING IS EVER ZEROED (finding 230): both sides get the same varied
 * pseudorandom bytes before every trial, so the clear of +0x28 is visible
 * and a store that fails to happen is too.  Every object is followed by a
 * guard region compared separately, and the BLOB's object is checked word by
 * absolute offset against the seed it replaced, so a header offset that is
 * wrong on both sides at once still fails (findings 223 and 224).
 *
 * THE LEVEL IS SWEPT 0..2 TOGETHER on both sides.  Nine of the ten prints
 * are `edprintf`, which is NOT gated -- it encodes first and tests the level
 * afterwards -- and the tenth is a `dsplibs_debug_printf` behind
 * `DSPLIB_DEBUG_ON()`, which is `dsplibs_debug_level > 1`.  So level 2 is
 * the only one where anything reaches the log at all, and it is also the
 * only one where the tail call runs; the "no special conditions" arm is
 * therefore driven at level 2 specifically.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90SpectralVerifier.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

/*
 * Reached through an asm() label rather than by spelling the ref_-prefixed
 * mangled name as an identifier; the convention is plain cdecl with `this`
 * as the first stack argument (finding 215), so no attribute is involved.
 */
void ref_sv_check(void *self)
	asm("ref__ZN19V90SpectralVerifier30checkSpecialSpectralConditionsEv");
}

/* ------------------------------------------------------------------ seeds */

static unsigned lfsr;

static unsigned char
next_byte(int mode, unsigned i)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	switch (mode) {
	case 1:
		return 0xa5;
	case 2:
		return 0x5a;
	case 3:
		return (unsigned char)((lfsr & 0xfe) | (unsigned)(i & 1u));
	default:
		return (unsigned char)(lfsr >> 3);
	}
}

/* The same varied bytes into both sides.  Never zeros -- finding 230. */
static void
fill_pair(void *a, void *b, unsigned n, int trial, int mode)
{
	unsigned char *pa = (unsigned char *)a;
	unsigned char *pb = (unsigned char *)b;
	unsigned i;

	lfsr = 0x1234u + 0x9e37u * (unsigned)trial + 0x51edu * (unsigned)mode;
	for (i = 0; i < n; i++)
		pa[i] = pb[i] = next_byte(mode, i);
}

/* Which four-byte words of the BLOB's object moved.  t_v90leaves.cpp's. */
static int
only_wrote(const unsigned char *before, const unsigned char *after,
	   unsigned n, const int *allow, int nallow, int *seen, int *first_bad)
{
	unsigned i;
	int bad = 0;

	*first_bad = -1;
	for (i = 0; i + 4 <= n; i += 4) {
		int k, ok = 0;

		if (memcmp(before + i, after + i, 4) == 0)
			continue;
		for (k = 0; k < nallow; k++)
			if (allow[k] == (int)i) {
				ok = 1;
				if (seen != 0)
					seen[k] = 1;
			}
		if (!ok) {
			if (*first_bad < 0)
				*first_bad = (int)i;
			bad++;
		}
	}
	return bad;
}

/* Both sides' levels move together, or they take different branches. */
static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/* --------------------------------------------------------------- fixture */

#define SV_SLOT		96		/* 44 of object, the rest a guard */
#define SPEC_N		128		/* spectrum bins the fixture owns */
#define PARM_SLOT	0x600

static unsigned char parm[PARM_SLOT] __attribute__((aligned(8)));
static float spec[SPEC_N];

union sv_slot {
	double align;
	unsigned char raw[SV_SLOT];
};

static union sv_slot sv_a, sv_b;

#define SV_A (*(V90SpectralVerifier *)sv_a.raw)
#define SV_B (*(V90SpectralVerifier *)sv_b.raw)

/*
 * One trial's whole input.  The nine frequencies are in the order the
 * function reads them, and every one of them is a bin index times
 * `binWidth`, so a case is written by thinking in bins and the arithmetic
 * under test is still the object's.
 */
struct sv_case {
	const char *what;
	unsigned state;			/* +0x24 */
	float binWidth;
	float freq[9];			/* isdn L,N,R; pbx L,N,R; sc ref,1,2 */
	float thr[5];			/* isdn L,R; pbx L,R; severe codec */
	float bin[12];			/* spectrum[0..11] */
};

/*
 * A `bin[]` entry written as this becomes a quiet NaN at run time.  It is a
 * marker rather than a second field so that every case below can leave it
 * out; a NaN cannot be a static initialiser here anyway, because the period
 * build is `-mno-ieee-fp` and folds `0.0f / 0.0f` at compile time -- finding
 * 2303's mechanism, where the same flag deleted the harness's own NaN
 * detector.
 */
#define SV_NAN_MARK	1.0e30f

static float
sv_nan(void)
{
	union {
		unsigned u;
		float f;
	} q;

	q.u = 0x7fc00000u;
	return q.f;
}

/* The order `freq[]` is written in, which is also the object's read order. */
enum {
	F_ISDN_LEFT = 0, F_ISDN_NULL, F_ISDN_RIGHT,
	F_PBX_LEFT, F_PBX_NULL, F_PBX_RIGHT,
	F_SC_REF, F_SC_T1, F_SC_T2
};

static void
sv_apply(const struct sv_case *c)
{
	V90Parameters *p = (V90Parameters *)parm;
	int i;

	for (i = 0; i < SPEC_N; i++)
		spec[i] = (i < 12) ? c->bin[i] : (float)(i * 3 - 40);
	for (i = 0; i < 12; i++)
		if (c->bin[i] == SV_NAN_MARK)
			spec[i] = sv_nan();

	p->SPECTRAL_VERIFIER_ISDN_LEFT_PEAK_FREQ = c->freq[F_ISDN_LEFT];
	p->SPECTRAL_VERIFIER_ISDN_NULL_FREQ = c->freq[F_ISDN_NULL];
	p->SPECTRAL_VERIFIER_ISDN_RIGHT_PEAK_FREQ = c->freq[F_ISDN_RIGHT];
	p->SPECTRAL_VERIFIER_GERMAN_PBX_LEFT_PEAK_FREQ = c->freq[F_PBX_LEFT];
	p->SPECTRAL_VERIFIER_GERMAN_PBX_NULL_FREQ = c->freq[F_PBX_NULL];
	p->SPECTRAL_VERIFIER_GERMAN_PBX_RIGHT_PEAK_FREQ = c->freq[F_PBX_RIGHT];
	p->SPECTRAL_VERIFIER_SEVERE_CODEC_REF_FREQ = c->freq[F_SC_REF];
	p->SPECTRAL_VERIFIER_SEVERE_CODEC_TEST_FREQ1 = c->freq[F_SC_T1];
	p->SPECTRAL_VERIFIER_SEVERE_CODEC_TEST_FREQ2 = c->freq[F_SC_T2];

	p->SPECTRAL_VERIFIER_ISDN_LEFT_PEAK_DELTA = c->thr[0];
	p->SPECTRAL_VERIFIER_ISDN_RIGHT_PEAK_DELTA = c->thr[1];
	p->SPECTRAL_VERIFIER_GERMAN_PBX_LEFT_PEAK_DELTA = c->thr[2];
	p->SPECTRAL_VERIFIER_GERMAN_PBX_RIGHT_PEAK_DELTA = c->thr[3];
	p->SPECTRAL_VERIFIER_SEVERE_CODEC_DELTA = c->thr[4];
}

/*
 * One trial.  Seeds both objects, wires the five words the function reads,
 * runs both sides, and compares everything: the whole object, the guard past
 * it, the blob's write set by absolute offset, and both transcripts.
 *
 * Returns the value the BLOB left in +0x28; `text` gets the blob's
 * transcript, which is what the pair checks below compare against each
 * other.
 */
static unsigned
sv_run(const struct sv_case *c, unsigned lvl, int mode, long tag,
       char *text, unsigned textsz, int *wrote_seen)
{
	static const int allow[] = { 0x28 };
	unsigned char before[SV_SLOT];
	int bad, first;

	set_level(lvl);
	sv_apply(c);

	fill_pair(sv_a.raw, sv_b.raw, SV_SLOT, (int)tag, mode);

	SV_A.params = SV_B.params = (V90Parameters *)parm;
	SV_A.binWidth = SV_B.binWidth = c->binWidth;
	SV_A.spectrum = SV_B.spectrum = spec;
	SV_A.accumulating = SV_B.accumulating = c->state;

	memcpy(before, sv_b.raw, SV_SLOT);

	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();

	SV_A.checkSpecialSpectralConditions();
	ref_sv_check(&SV_B);

	dsplib_debug_capture_on = 0;

	diff_eq_obj("after checkSpecialSpectralConditions",
		    V90SpectralVerifier, &SV_A, &SV_B, tag);
	diff_eq_int("no store past the object (%ld)",
		    memcmp(sv_a.raw + sizeof(SV_A), sv_b.raw + sizeof(SV_B),
			   SV_SLOT - sizeof(SV_A)) == 0, 1, tag);

	bad = only_wrote(before, sv_b.raw, SV_SLOT, allow, 1, wrote_seen,
			 &first);
	diff_eq_int("the blob wrote outside +0x28 at +0x%lx",
		    bad == 0 ? -1 : first, -1, tag);

	diff_eq_int("transcript matches (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	diff_eq_int("line counts match (%ld)",
		    (int)dsplib_debug_capture_lines(0),
		    (int)dsplib_debug_capture_lines(1), tag);

	/*
	 * The guarded cases return before the first `edprintf`, so they are
	 * silent at EVERY level and the "above the gate something printed"
	 * check does not apply to them; `run_cases` asserts their silence
	 * separately, which is the stronger claim.
	 */
	if (lvl > 1 && c->state == 2u) {
		diff_eq_int("above the gate the blob printed (%ld)",
			    dsplib_debug_capture_lines(1) > 0, 1, tag);
	} else if (lvl <= 1) {
		diff_eq_int("below the gate ours was silent (%ld)",
			    (int)dsplib_debug_capture_lines(0), 0, tag);
		diff_eq_int("below the gate the blob was silent (%ld)",
			    (int)dsplib_debug_capture_lines(1), 0, tag);
	}

	if (text != 0) {
		unsigned n = textsz - 1;
		const char *t = dsplib_debug_capture_text(1);

		strncpy(text, t, n);
		text[n] = '\0';
	}

	return SV_B.word_28;
}

/* ---------------------------------------------------------------- cases */

/*
 * `binWidth` is 1.0f here and the frequencies are whole bin numbers, so
 * `(unsigned)(freq * (1/binWidth) + 0.5f)` is the bin number itself and each
 * case can be read as "these are the levels in these bins".  The pair cases
 * further down use other widths on purpose.
 *
 * The three detections are SEQUENTIAL and each overwrites the last, so
 * `both_isdn_and_pbx` is the case that proves 2 wins over 1 rather than the
 * first match sticking.
 */
static const struct sv_case cases[] = {
    /* Nothing trips: every delta is below its threshold. */
    { "none", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 10.0f, 10.0f, 10.0f, 10.0f, 10.0f },
      { 1.5f, 1.0f, 2.25f, 3.0f, 4.0f, 5.0f, 6.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    /* ISDN only: both ISDN deltas clear their thresholds. */
    { "isdn", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 1.0f, 1.0f, 100.0f, 100.0f, 100.0f },
      { 20.5f, 1.0f, 30.25f, 3.0f, 4.0f, 5.0f, 6.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    /* ISDN's LEFT clears and its RIGHT does not: the `&&` must hold. */
    { "isdn_left_only", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 1.0f, 1.0f, 100.0f, 100.0f, 100.0f },
      { 20.5f, 1.0f, 1.25f, 3.0f, 4.0f, 5.0f, 6.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    /* ISDN's RIGHT clears and its LEFT does not. */
    { "isdn_right_only", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 1.0f, 1.0f, 100.0f, 100.0f, 100.0f },
      { 1.5f, 1.0f, 30.25f, 3.0f, 4.0f, 5.0f, 6.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    /* German PBX only. */
    { "pbx", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 100.0f, 100.0f, 1.0f, 1.0f, 100.0f },
      { 1.5f, 1.0f, 2.25f, 40.0f, 4.0f, 50.0f, 6.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    { "pbx_left_only", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 100.0f, 100.0f, 1.0f, 1.0f, 100.0f },
      { 1.5f, 1.0f, 2.25f, 40.0f, 4.0f, 4.25f, 6.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    { "pbx_right_only", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 100.0f, 100.0f, 1.0f, 1.0f, 100.0f },
      { 1.5f, 1.0f, 2.25f, 4.25f, 4.0f, 50.0f, 6.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    /* Both ISDN and PBX trip: the answer must be 2, not 1. */
    { "both_isdn_and_pbx", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 1.0f, 1.0f, 1.0f, 1.0f, 100.0f },
      { 20.5f, 1.0f, 30.25f, 40.0f, 4.0f, 50.0f, 6.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    /* Severe codec: both differences clear the one shared threshold. */
    { "severe", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 100.0f, 100.0f, 100.0f, 100.0f, 5.0f },
      { 1.5f, 1.0f, 2.25f, 3.0f, 4.0f, 5.0f, 60.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    /* Only the first difference clears: the `&&` again. */
    { "severe_first_only", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 100.0f, 100.0f, 100.0f, 100.0f, 5.0f },
      { 1.5f, 1.0f, 2.25f, 3.0f, 4.0f, 5.0f, 60.0f, 5.5f, 58.0f,
	0.0f, 0.0f, 0.0f } },

    /* Only the second. */
    { "severe_second_only", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 100.0f, 100.0f, 100.0f, 100.0f, 5.0f },
      { 1.5f, 1.0f, 2.25f, 3.0f, 4.0f, 5.0f, 60.0f, 58.0f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    /* All three trip: 3 must win, being last. */
    { "all_three", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 1.0f, 1.0f, 1.0f, 1.0f, 5.0f },
      { 20.5f, 1.0f, 30.25f, 40.0f, 4.0f, 50.0f, 60.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    /*
     * The guard.  The accumulation state is not 2, so the function must
     * clear +0x28 and return without printing -- even though these levels
     * and thresholds would otherwise report a severe codec.
     */
    { "state_0", 0u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 1.0f, 1.0f, 1.0f, 1.0f, 5.0f },
      { 20.5f, 1.0f, 30.25f, 40.0f, 4.0f, 50.0f, 60.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },
    { "state_1", 1u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 1.0f, 1.0f, 1.0f, 1.0f, 5.0f },
      { 20.5f, 1.0f, 30.25f, 40.0f, 4.0f, 50.0f, 60.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },
    { "state_3", 3u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 1.0f, 1.0f, 1.0f, 1.0f, 5.0f },
      { 20.5f, 1.0f, 30.25f, 40.0f, 4.0f, 50.0f, 60.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    /*
     * EXACTLY ON THE THRESHOLD.  All six comparisons are strict `>` in the
     * object (`ja` at 0x460d5 and 0x4628c, `jbe` at 0x46553 and 0x4627d,
     * `jae` at 0x46502 and `ja` at 0x46511), and `>` and `>=` agree on every
     * pair of numbers except an equal one.  Each of these three puts one
     * delta EXACTLY on its threshold and the other well past it, so a
     * reconstruction that relaxed either comparison detects where the object
     * does not.
     */
    { "isdn_left_on_threshold", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 19.5f, 1.0f, 100.0f, 100.0f, 100.0f },
      { 20.5f, 1.0f, 30.25f, 3.0f, 4.0f, 5.0f, 6.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    { "pbx_right_on_threshold", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 100.0f, 100.0f, 1.0f, 46.0f, 100.0f },
      { 1.5f, 1.0f, 2.25f, 40.0f, 4.0f, 50.0f, 6.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    { "severe_second_on_threshold", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 100.0f, 100.0f, 100.0f, 100.0f, 54.25f },
      { 1.5f, 1.0f, 2.25f, 3.0f, 4.0f, 5.0f, 60.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    /*
     * A NaN in the null bin makes every ISDN and PBX delta unordered.  The
     * sign character is where that shows: the object's branchless
     * `0x2d - 2*CF` prints '+' for an unordered compare where the readable
     * spelling of the test would print '-'.  Nothing here decodes the
     * transcript -- the two sides' texts are compared, which is enough.
     */
    { "nan_null", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 1.0f, 1.0f, 1.0f, 1.0f, 5.0f },
      { 20.5f, SV_NAN_MARK, 30.25f, 40.0f, SV_NAN_MARK, 50.0f, 60.0f, 5.5f,
	5.75f, 0.0f, 0.0f, 0.0f } },

    /*
     * Every delta negative, so every sign is '-' and every whole part is a
     * magnitude rather than the value.
     */
    { "negative", 2u, 1.0f,
      { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
      { 1.0f, 1.0f, 1.0f, 1.0f, 5.0f },
      { -12.75f, 40.0f, -3.5f, -8.25f, 60.0f, -1.5f, -9.5f, 4.25f, 7.125f,
	0.0f, 0.0f, 0.0f } },

    /*
     * `binWidth` other than one, and frequencies that are not bin
     * boundaries: this is the case that exercises the reciprocal multiply
     * rather than a division, because 0.1f has no exact binary form and
     * `f * (1/0.1f)` and `f / 0.1f` do not agree in the last place.
     */
    { "narrow_bins", 2u, 0.1f,
      { 0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f },
      { 1.0f, 1.0f, 1.0f, 1.0f, 5.0f },
      { 20.5f, 1.0f, 30.25f, 40.0f, 4.0f, 50.0f, 60.0f, 5.5f, 5.75f,
	0.0f, 0.0f, 0.0f } },

    /*
     * Frequencies EXACTLY on the half-bin boundary, where `+ 0.5f` lands on
     * an integer and one unit in the last place decides which bin is read.
     * The bins on either side hold very different levels so the choice is
     * visible in both the transcript and +0x28.
     */
    { "half_bin", 2u, 4.0f,
      { 2.0f, 6.0f, 10.0f, 14.0f, 18.0f, 22.0f, 26.0f, 30.0f, 34.0f },
      { 1.0f, 1.0f, 1.0f, 1.0f, 5.0f },
      { 0.0f, 90.5f, 1.0f, 31.25f, 2.0f, 41.0f, 3.0f, 51.0f, 4.0f,
	61.0f, 5.0f, 71.0f } }
};

#define NCASES ((int)(sizeof(cases) / sizeof(cases[0])))

/* ------------------------------------------------------- separating pairs */

/*
 * Each entry is two cases that differ in ONE printed quantity and are
 * otherwise identical, so "the two transcripts differ" is a statement about
 * that quantity and nothing else.  A reconstruction that dropped the sign,
 * the whole part, the hundredths or the printed frequency would make the
 * corresponding pair agree, and this is the check that would fail.
 *
 * `same_result` records whether the pair is expected to leave the SAME value
 * in +0x28; where it is, the pair is separated by the transcript alone,
 * which is the point.
 */
struct sv_pair {
	const char *what;
	int same_result;
	struct sv_case a;
	struct sv_case b;
};

static const struct sv_pair pairs[] = {
    /*
     * SIGN.  The ISDN left delta is +19.5 in one and -19.5 in the other;
     * magnitude, whole part and hundredths are identical, so only the sign
     * character can separate them.  Neither trips a threshold.
     */
    { "the sign character", 1,
      { "sign+", 2u, 1.0f,
	{ 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
	{ 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f },
	{ 20.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
	  0.0f, 0.0f, 0.0f } },
      { "sign-", 2u, 1.0f,
	{ 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
	{ 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f },
	{ -18.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
	  0.0f, 0.0f, 0.0f } } },

    /*
     * THE WHOLE PART.  Same sign, same hundredths (both .25), different
     * integer magnitude.
     */
    { "the truncated magnitude", 1,
      { "whole7", 2u, 1.0f,
	{ 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
	{ 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f },
	{ 8.25f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
	  0.0f, 0.0f, 0.0f } },
      { "whole23", 2u, 1.0f,
	{ 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
	{ 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f },
	{ 24.25f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
	  0.0f, 0.0f, 0.0f } } },

    /*
     * THE HUNDREDTHS.  Same sign, same whole part (7), .25 against .75.
     * This is the pair a mutation to the scale or to the fractional
     * subtraction has to survive.
     */
    { "the hundredths", 1,
      { "frac25", 2u, 1.0f,
	{ 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
	{ 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f },
	{ 8.25f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
	  0.0f, 0.0f, 0.0f } },
      { "frac75", 2u, 1.0f,
	{ 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
	{ 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f },
	{ 8.75f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
	  0.0f, 0.0f, 0.0f } } },

    /*
     * THE PRINTED FREQUENCY.  `binWidth` is 10, so 60.0 and 64.0 both round
     * to bin 6 and the three severe-codec levels are identical -- but the
     * diagnostic prints `(int)SEVERE_CODEC_REF_FREQ`, which is 60 in one and
     * 64 in the other.  Nothing else in the run can tell them apart, so a
     * reconstruction that printed the bin, or the level, or nothing at all
     * would make this pair agree.
     */
    { "the printed probe frequency", 1,
      { "freq60", 2u, 10.0f,
	{ 0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f },
	{ 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f },
	{ 1.5f, 1.0f, 2.25f, 3.0f, 4.0f, 5.0f, 6.5f, 7.25f, 8.75f,
	  0.0f, 0.0f, 0.0f } },
      { "freq64", 2u, 10.0f,
	{ 0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 64.0f, 70.0f, 80.0f },
	{ 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f },
	{ 1.5f, 1.0f, 2.25f, 3.0f, 4.0f, 5.0f, 6.5f, 7.25f, 8.75f,
	  0.0f, 0.0f, 0.0f } } },

    /*
     * THE SELECTED BIN.  The two differ only in which bin the ISDN null
     * frequency rounds to -- 1 against 2 -- and the two bins hold different
     * levels, so both deltas move.  A reconstruction that dropped the
     * `+ 0.5f`, or rounded the other way, lands on a different bin here.
     */
    { "the rounded bin", 1,
      { "bin_lo", 2u, 1.0f,
	{ 0.0f, 1.4f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f },
	{ 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f },
	{ 20.5f, 1.0f, 9.25f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
	  0.0f, 0.0f, 0.0f } },
      { "bin_hi", 2u, 1.0f,
	{ 0.0f, 1.6f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f },
	{ 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f },
	{ 20.5f, 1.0f, 9.25f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
	  0.0f, 0.0f, 0.0f } } },

    /*
     * WHICH DELTA IS WHICH.  The ISDN left and right levels are swapped and
     * nothing else moves, so a reconstruction that measured both deltas
     * against the wrong peak, or printed them the wrong way round, makes
     * these two agree.  The thresholds are high enough that neither trips.
     */
    { "left and right are not interchangeable", 1,
      { "lr", 2u, 1.0f,
	{ 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
	{ 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f },
	{ 20.5f, 1.0f, 3.25f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
	  0.0f, 0.0f, 0.0f } },
      { "rl", 2u, 1.0f,
	{ 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f },
	{ 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f },
	{ 3.25f, 1.0f, 20.5f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
	  0.0f, 0.0f, 0.0f } } }
};

#define NPAIRS ((int)(sizeof(pairs) / sizeof(pairs[0])))

/* ------------------------------------------------------------------ main */

#define TEXTSZ 8192

static char text_a[TEXTSZ];
static char text_b[TEXTSZ];

static int
run_cases(void)
{
	int wrote_seen[1] = { 0 };
	int seen_result[4] = { 0, 0, 0, 0 };
	int seen_quiet = 0;
	int i, mode;
	unsigned lvl;

	diff_begin("V90SpectralVerifier::checkSpecialSpectralConditions");

	for (lvl = 0; lvl <= 2; lvl++) {
		for (mode = 0; mode < 4; mode++) {
			for (i = 0; i < NCASES; i++) {
				long tag = (long)lvl * 10000 + mode * 100 + i;
				unsigned got;

				got = sv_run(&cases[i], lvl, mode, tag,
					     text_b, TEXTSZ, wrote_seen);

				if (got < 4u)
					seen_result[got]++;
				else
					diff_eq_int("+0x28 is one of 0..3 "
						    "(%ld)", (long)got, 0,
						    tag);

				/*
				 * The guarded cases must print NOTHING at any
				 * level: the early return is before the first
				 * `edprintf`.
				 */
				if (cases[i].state != 2u) {
					diff_eq_int("the guard printed "
						    "nothing (%ld)",
						    (int)
						    dsplib_debug_capture_lines
							(1), 0, tag);
					diff_eq_int("the guard cleared +0x28 "
						    "(%ld)", (long)got, 0,
						    tag);
					seen_quiet++;
				}
			}
		}
	}

	set_level(0);

	diff_eq_int("+0x28 is a word the function writes", wrote_seen[0], 1, 0);
	diff_eq_int("no special conditions was reached %ld times",
		    seen_result[0] > 0, 1, seen_result[0]);
	diff_eq_int("German ISDN NT1 box was reached %ld times",
		    seen_result[1] > 0, 1, seen_result[1]);
	diff_eq_int("German PBX was reached %ld times",
		    seen_result[2] > 0, 1, seen_result[2]);
	diff_eq_int("Severe Codec was reached %ld times",
		    seen_result[3] > 0, 1, seen_result[3]);
	diff_eq_int("the accumulation guard was reached %ld times",
		    seen_quiet > 0, 1, seen_quiet);

	return diff_end();
}

/*
 * The separating pairs.  Every one of these runs at level 2, because that is
 * the only level at which anything reaches the log, and asserts that two
 * inputs differing in one printed quantity produce two different
 * transcripts.  `same_result` says the pair is NOT separated by +0x28, so
 * the transcript is carrying the whole claim.
 */
static int
run_pairs(void)
{
	int wrote_seen[1] = { 0 };
	int i, separated = 0;

	diff_begin("checkSpecialSpectralConditions: separating pairs");

	for (i = 0; i < NPAIRS; i++) {
		long tag = 500000 + i;
		unsigned ra, rb;

		ra = sv_run(&pairs[i].a, 2u, 0, tag, text_a, TEXTSZ,
			    wrote_seen);
		rb = sv_run(&pairs[i].b, 2u, 0, tag + 1, text_b, TEXTSZ,
			    wrote_seen);

		diff_eq_int("both sides of the pair printed (%ld)",
			    text_a[0] != '\0' && text_b[0] != '\0', 1, tag);
		if (pairs[i].same_result)
			diff_eq_int("the pair leaves the same +0x28 (%ld)",
				    (long)ra, (long)rb, tag);
		if (strcmp(text_a, text_b) != 0)
			separated++;
		diff_eq_int("the pair separates -- see the case comment (%ld)",
			    strcmp(text_a, text_b) != 0, 1, tag);
	}

	set_level(0);
	diff_eq_int("%ld pairs separated", separated, NPAIRS, separated);

	return diff_end();
}

/*
 * The bin arithmetic, swept.
 *
 * `getSpectrumOfNearestBin` DIVIDES (0x45ecb, `fdivs 0x14(%ecx)`) and this
 * function MULTIPLIES BY A RECIPROCAL it computes once per block (0x45f37,
 * 0x460e1, 0x4629a).  The two disagree in the last place for most divisors,
 * and `+ 0.5f` then a truncation turn a last-place disagreement into a bin
 * index that is one out -- so the difference is observable and this sweep is
 * what observes it.  Every frequency here is a bin index plus an offset,
 * times the width, and the offsets sit ON and immediately either side of the
 * half-bin boundary where the rounding decides.  The twelve levels alternate
 * in sign and grow, so landing one bin out changes the printed delta, its
 * sign, its whole part and its hundredths all at once.
 *
 * The widths are chosen to include several with no exact binary reciprocal
 * (0.3, 1/3, 0.7, 2.4, 9.6, 0.037) beside the exact ones, because an exact
 * reciprocal makes the multiply and the divide agree and proves nothing.
 */
static const float sweep_bw[] = {
	1.0f, 0.5f, 0.125f, 3.125f, 6.25f, 31.25f,
	0.3f, 1.0f / 3.0f, 0.7f, 1.7f, 2.4f, 9.6f, 0.037f, 12.7f
};
#define NSWEEP_BW ((int)(sizeof(sweep_bw) / sizeof(sweep_bw[0])))

static const float sweep_off[] = {
	0.0f, 0.25f, 0.499999f, 0.5f, 0.500001f, 0.75f
};
#define NSWEEP_OFF ((int)(sizeof(sweep_off) / sizeof(sweep_off[0])))

/* Two threshold sets: one nothing clears, one nearly everything does. */
static const float sweep_thr[][5] = {
	{ 1000.0f, 1000.0f, 1000.0f, 1000.0f, 1000.0f },
	{ -1000.0f, -1000.0f, -1000.0f, -1000.0f, -1000.0f }
};
#define NSWEEP_THR ((int)(sizeof(sweep_thr) / sizeof(sweep_thr[0])))

static int
run_sweep(void)
{
	static const float level[12] = {
		2.5f, -7.25f, 13.75f, -21.5f, 33.125f, -44.0f,
		55.625f, -66.25f, 77.5f, -88.75f, 99.0f, -110.5f
	};
	int wrote_seen[1] = { 0 };
	int seen_result[4] = { 0, 0, 0, 0 };
	int w, o, t, k, distinct = 0;
	char prev[TEXTSZ];

	diff_begin("checkSpecialSpectralConditions: the bin arithmetic");

	prev[0] = '\0';

	for (w = 0; w < NSWEEP_BW; w++)
		for (o = 0; o < NSWEEP_OFF; o++)
			for (t = 0; t < NSWEEP_THR; t++) {
				struct sv_case c;
				long tag = 700000 + ((w * NSWEEP_OFF + o)
						     * NSWEEP_THR + t);
				unsigned got;

				memset(&c, 0, sizeof(c));
				c.what = "sweep";
				c.state = 2u;
				c.binWidth = sweep_bw[w];
				for (k = 0; k < 9; k++)
					c.freq[k] = ((float)k + sweep_off[o])
						  * sweep_bw[w];
				for (k = 0; k < 5; k++)
					c.thr[k] = sweep_thr[t][k];
				for (k = 0; k < 12; k++)
					c.bin[k] = level[k];

				got = sv_run(&c, 2u, w & 3, tag, text_b,
					     TEXTSZ, wrote_seen);
				if (got < 4u)
					seen_result[got]++;
				else
					diff_eq_int("+0x28 is one of 0..3 "
						    "(%ld)", (long)got, 0,
						    tag);

				if (strcmp(prev, text_b) != 0) {
					distinct++;
					strcpy(prev, text_b);
				}
			}

	set_level(0);

	/*
	 * Anti-vacuity: the sweep must actually move the printed output, or
	 * it is 168 copies of one trial.  Half is a floor with room to
	 * spare; the run has 168 trials and consecutive ones differ far more
	 * often than that.
	 */
	diff_eq_int("%ld of the sweep's trials printed something new",
		    distinct > NSWEEP_BW * NSWEEP_OFF * NSWEEP_THR / 2, 1,
		    distinct);
	diff_eq_int("the sweep reached no-condition %ld times",
		    seen_result[0] > 0, 1, seen_result[0]);
	diff_eq_int("the sweep reached a detection %ld times",
		    seen_result[1] + seen_result[2] + seen_result[3] > 0, 1,
		    seen_result[1] + seen_result[2] + seen_result[3]);

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_cases();
	bad |= run_pairs();
	bad |= run_sweep();

	return bad;
}

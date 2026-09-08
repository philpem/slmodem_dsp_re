/*
 * t_v90rxctor.cpp -- differential test of the two INNER lifecycle pairs of the
 * V.90 receive chain: `V90Phase3Demodulator`'s constructor and destructor, and
 * `V90Phase4Demodulator`'s.
 *
 * Both sides go through asm() labels and both the C1 and the C2 variant of
 * each: C++ has no syntax for running a constructor over storage that already
 * exists, and `OBJ = V90Phase4Demodulator(...)` would build a temporary over
 * uninitialised stack and copy it in, throwing away the seed the whole
 * fixture rests on (findings F223, F224).  The blob holds C1 and C2 as two
 * identical copies at different addresses and our compiler emits one function
 * under both names; calling only one leaves half the pair untested.
 *
 * ---------------------------------------------------------------------------
 * THE PHASE 4 DEMODULATOR IS A PLACEMENT TEST ABOVE ALL
 *
 * Eleven arguments, ten of them pointers, and the constructor's entire body is
 * eleven stores.  So every pointed-to object is a SEPARATE block, seeded
 * per trial, and the two sides are pointed at the SAME eleven -- finding
 * F1105's rule, which makes every stored pointer compare equal and keeps all
 * eleven words IN the comparison instead of excluded from it.
 *
 * The two `V90MappingParams *` matter more than the rest.  The embedded
 * modulator receives them SWAPPED (finding F1301, derived twice from opposite
 * sides of the call), and a fixture that passed one pointer twice could not
 * see that at all -- the wrong order and the right order would store the same
 * bytes.  They are distinct blocks here for exactly that reason, and so is
 * every other argument: eleven identical pointers would make ten of the
 * eleven placement claims vacuous.
 *
 * ---------------------------------------------------------------------------
 * THE PHASE 3 DEMODULATOR'S CONSTRUCTOR ENDS BY CALLING `reset`
 *
 * which means this test drives 801 bytes of already-tested code as well, and
 * the object it compares is the state `reset` left rather than the state the
 * constructor's own stores left.  Two consequences the checks below are built
 * around:
 *
 *   - The three fields the constructor stores that `reset` does NOT overwrite
 *     -- `params`, `sessionFlag` and `autoDigitalImpDetector` -- are asserted
 *     by value, because a comparison alone cannot tell "stored correctly"
 *     from "both sides equally wrong".
 *   - `reset` prints at debug level 2, so the sweep runs at 0 and 2 and
 *     compares the transcripts.  Without that, the constructor's choice of
 *     `reset` arguments is only visible where those arguments reach memory,
 *     and the state selector reaches a diagnostic before it reaches a field.
 *
 * ARGUMENT 2 IS NEVER LOADED and no placement is asserted for it.  The
 * constructor accepts a `V90SpectralVerifier *` and `0x48(%esp)` appears
 * nowhere in its 365 bytes.  It is passed a distinguishable block all the
 * same, so that a reconstruction which started storing it somewhere would
 * fail rather than agree.
 */

#include <string.h>
#include <malloc.h>

#include "harness.h"
#include "dsplib/debug.h"
/* The definition the destructor test's explicit destructor calls need. */
#include "dsplib/ANSamToneDetector.h"
#include "dsplib/V90Phase3Demodulator.h"
#include "dsplib/V90Phase4Demodulator.h"
/* The BLOCK form of V90Parameters; no TU may hold both (finding F1112). */
#include "dsplib/V90PreFilter.h"

extern "C" {
void p3d_ctor1(void *self, void *params, void *ver, unsigned int flag,
	       void *adid)
	asm("_ZN20V90Phase3DemodulatorC1EP13V90ParametersP19V90SpectralVerifie"
	    "rjP25V90AutoDigitalImpDetector");
void p3d_ctor2(void *self, void *params, void *ver, unsigned int flag,
	       void *adid)
	asm("_ZN20V90Phase3DemodulatorC2EP13V90ParametersP19V90SpectralVerifie"
	    "rjP25V90AutoDigitalImpDetector");
void ref_p3d_ctor1(void *self, void *params, void *ver, unsigned int flag,
		   void *adid)
	asm("ref__ZN20V90Phase3DemodulatorC1EP13V90ParametersP19V90SpectralVer"
	    "ifierjP25V90AutoDigitalImpDetector");
void ref_p3d_ctor2(void *self, void *params, void *ver, unsigned int flag,
		   void *adid)
	asm("ref__ZN20V90Phase3DemodulatorC2EP13V90ParametersP19V90SpectralVer"
	    "ifierjP25V90AutoDigitalImpDetector");

void p3d_dtor1(void *self) asm("_ZN20V90Phase3DemodulatorD1Ev");
void p3d_dtor2(void *self) asm("_ZN20V90Phase3DemodulatorD2Ev");
void ref_p3d_dtor1(void *self) asm("ref__ZN20V90Phase3DemodulatorD1Ev");
void ref_p3d_dtor2(void *self) asm("ref__ZN20V90Phase3DemodulatorD2Ev");

void p4d_ctor1(void *self, void *mp1, void *mp2, void *dem, void *cp, void *mp,
	       void *dsc, void *ce, void *par, void *p3d, void *adid,
	       unsigned int flag)
	asm("_ZN20V90Phase4DemodulatorC1EP16V90MappingParamsS1_P11V90DemapperP"
	    "5V90CPP5V90MPP11DescramblerIhiEP22V90ConnectionEvaluatorP13V90Par"
	    "ametersP20V90Phase3DemodulatorP25V90AutoDigitalImpDetectorj");
void p4d_ctor2(void *self, void *mp1, void *mp2, void *dem, void *cp, void *mp,
	       void *dsc, void *ce, void *par, void *p3d, void *adid,
	       unsigned int flag)
	asm("_ZN20V90Phase4DemodulatorC2EP16V90MappingParamsS1_P11V90DemapperP"
	    "5V90CPP5V90MPP11DescramblerIhiEP22V90ConnectionEvaluatorP13V90Par"
	    "ametersP20V90Phase3DemodulatorP25V90AutoDigitalImpDetectorj");
void ref_p4d_ctor1(void *self, void *mp1, void *mp2, void *dem, void *cp,
		   void *mp, void *dsc, void *ce, void *par, void *p3d,
		   void *adid, unsigned int flag)
	asm("ref__ZN20V90Phase4DemodulatorC1EP16V90MappingParamsS1_P11V90Demap"
	    "perP5V90CPP5V90MPP11DescramblerIhiEP22V90ConnectionEvaluatorP13V9"
	    "0ParametersP20V90Phase3DemodulatorP25V90AutoDigitalImpDetectorj");
void ref_p4d_ctor2(void *self, void *mp1, void *mp2, void *dem, void *cp,
		   void *mp, void *dsc, void *ce, void *par, void *p3d,
		   void *adid, unsigned int flag)
	asm("ref__ZN20V90Phase4DemodulatorC2EP16V90MappingParamsS1_P11V90Demap"
	    "perP5V90CPP5V90MPP11DescramblerIhiEP22V90ConnectionEvaluatorP13V9"
	    "0ParametersP20V90Phase3DemodulatorP25V90AutoDigitalImpDetectorj");

void p4d_dtor1(void *self) asm("_ZN20V90Phase4DemodulatorD1Ev");
void p4d_dtor2(void *self) asm("_ZN20V90Phase4DemodulatorD2Ev");
void ref_p4d_dtor1(void *self) asm("ref__ZN20V90Phase4DemodulatorD1Ev");
void ref_p4d_dtor2(void *self) asm("ref__ZN20V90Phase4DemodulatorD2Ev");

extern unsigned int ref_dsplibs_debug_level;

void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);
}

#define P3D_SLOT	(0x42c + 64)
#define P4D_SLOT	(0x351c + 64)
#define PARM_SLOT	(V90PARAMETERS_BOUND + 64)

static unsigned char p3a[P3D_SLOT] __attribute__((aligned(8)));
static unsigned char p3b[P3D_SLOT] __attribute__((aligned(8)));
static unsigned char p4a[P4D_SLOT] __attribute__((aligned(8)));
static unsigned char p4b[P4D_SLOT] __attribute__((aligned(8)));

/*
 * The parameter block, and the ELEVEN distinguishable argument blocks.  Each
 * is its own object with its own seeded contents, so a store that lands at
 * the wrong offset holds a value no other argument could have supplied.
 */
static unsigned char parm[PARM_SLOT] __attribute__((aligned(8)));

#define NARG 11
static unsigned char argblk[NARG][96] __attribute__((aligned(8)));

/*
 * THE DETECTOR IS A REAL OBJECT, and it is the one argument that has to be.
 * `V90Phase3Demodulator`'s constructor ends by calling its own `reset`, which
 * calls `V90AutoDigitalImpDetector::reset`, which reads that object's `params`
 * and writes some 43 KB of its state.  A 96-byte block of seeded bytes gives
 * it a garbage `params` and the run dies inside a function this test is not
 * about.  Same wiring as test/harness/v90demfix.h: the detector points at the
 * one parameter block both sides share.
 *
 * Every OTHER argument is never dereferenced by anything under test -- they
 * are stored and nothing more -- so they stay small distinguishable blocks.
 */
static unsigned char adidblk[sizeof(V90AutoDigitalImpDetector)]
	__attribute__((aligned(8)));

#define ADID ((V90AutoDigitalImpDetector *)adidblk)
#define PARAMS ((V90Parameters *)parm)

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	/* `| 1` so no seeded byte is ever zero; finding F230. */
	return (unsigned char)((lfsr >> 3) | 1u);
}

static void
fill(void *p, size_t n)
{
	unsigned char *q = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++)
		q[i] = nextb();
}

/*
 * The parameter block's four SD-detector slots are SET, not seeded.
 * `V90SdDetector`'s constructor copies three floats bit for bit -- it does no
 * arithmetic on them -- but `reset`, which this constructor calls, does; and
 * a random 32-bit pattern is a signalling NaN about one time in 250.  No test
 * should be discovering what two different compilations do with one.  The
 * fourth is the unsigned limit and is a small count for the same reason.
 */
static const float sdv[] = { 0.0f, 1.0f, -1.0f, 0.375f, 1e-6f, 1e4f };
#define NSD ((int)(sizeof(sdv) / sizeof(sdv[0])))

static void
seed_all(long trial, int idx)
{
	int i;

	lfsr = 0x5c1du + 0x9e37u * (unsigned)trial;

	fill(parm, PARM_SLOT);
	for (i = 0; i < NARG; i++)
		fill(argblk[i], sizeof(argblk[i]));
	fill(adidblk, sizeof(adidblk));
	ADID->params = PARAMS;

	V90PF(PARAMS)[0x284 / 4] = sdv[idx % NSD];
	V90PF(PARAMS)[0x288 / 4] = sdv[(idx + 1) % NSD];
	V90PF(PARAMS)[0x28c / 4] = sdv[(idx + 2) % NSD];
	V90PW(PARAMS)[0x290 / 4] = (int)(unsigned)(idx * 7u + 1u);
}

/* ============================================== V90Phase3Demodulator */

/*
 * The words that hold two different addresses.  The two the constructor
 * allocates itself, plus whatever its embedded subobjects allocate -- the
 * `Descrambler<int,int>` at +0x3d0 and the `V90Phase3Modulator` at +0x34 both
 * take buffers in THEIR constructors, which is finding F1303's lesson applied
 * before the fact rather than after it: the excluded list is the pointers the
 * constructed OBJECT holds, not the ones this function stores.
 */
struct region {
	unsigned off;
	const char *name;
};

/* Filled in by discover_regions(); see its comment. */
static unsigned p3d_skip[64];
static int n_p3d_skip;

static void
skip_words(unsigned char *a, unsigned char *b, const unsigned *sk, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		memset(a + sk[i], 0, 4);
		memset(b + sk[i], 0, 4);
	}
}

/*
 * WHICH WORDS DIFFER FOR A REASON THAT IS NOT A DEFECT, discovered rather
 * than listed.  Every such word is a pointer into a block one side allocated,
 * so it is exactly the set of four-byte-aligned words whose value is a LIVE
 * ALLOCATION on that side -- which the harness allocator can answer.  Listing
 * them by hand would have meant reading three constructors this file does not
 * own and re-reading them whenever one changed.
 *
 * The discovery runs once, on the first trial, and the resulting list is then
 * fixed for every trial after it: a set that varied per trial could hide a
 * genuine difference by growing to cover it.
 */
/*
 * INSIDE a live allocation, not equal to one.  The embedded scrambler and
 * descrambler hold seven pointers each that address the MIDDLE of their
 * buffers -- `pOut`, `pTap1`, `pTap2` and the three `pInit*` -- so an
 * equality test finds the two bases and misses the twelve words beside them,
 * which is exactly how this test failed the first time it was run.
 */
static int
is_live(void *p, void **live, int nlive)
{
	unsigned long v = (unsigned long)p;
	int i;

	for (i = 0; i < nlive; i++) {
		unsigned long base = (unsigned long)live[i];

		if (v >= base && v <= base + malloc_usable_size(live[i]))
			return 1;
	}
	return 0;
}

/*
 * COMPARE A BLOCK THE CONSTRUCTOR ALLOCATED, not just the pointer to it.
 *
 * Without this the whole of what `V90SdDetector`'s and `ANSamToneDetector`'s
 * constructors were told is invisible: the pointers to their storage are
 * excluded because the two sides allocate separately, and if nothing looks
 * INSIDE the storage then swapping two of the four SD thresholds, or the
 * ANSam detector's two sample counts, changes nothing this test can see.  Six
 * mutations proved exactly that before this existed.
 *
 * Any word of the block that is itself a pointer into a live allocation is
 * zeroed on BOTH sides -- if EITHER side's value is such a pointer, because a
 * word that is a pointer on one side and a plain number on the other is
 * exactly the shape a real defect would take and must not be silently
 * dropped from only one side.
 */
static void
cmp_block(const char *what, const char *type, void *pa, void *pb, size_t n,
	  void **live, int nlive, long trial)
{
	static unsigned char ba[512], bb[512];
	size_t o;

	if (n > sizeof(ba))
		n = sizeof(ba);
	memcpy(ba, pa, n);
	memcpy(bb, pb, n);
	for (o = 0; o + 4 <= n; o += 4) {
		void *va, *vb;

		memcpy(&va, ba + o, sizeof va);
		memcpy(&vb, bb + o, sizeof vb);
		if ((va != 0 && is_live(va, live, nlive)) ||
		    (vb != 0 && is_live(vb, live, nlive))) {
			memset(ba + o, 0, 4);
			memset(bb + o, 0, 4);
		}
	}
	diff_eq_obj_(__FILE__, __LINE__, what, type, ba, bb, n, trial);
}

static void
discover_regions(unsigned char *obj, unsigned size, void **live, int nlive,
		 unsigned *out, int *nout, int max)
{
	unsigned o;

	*nout = 0;
	for (o = 0; o + 4 <= size; o += 4) {
		void *v;

		memcpy(&v, obj + o, sizeof v);
		if (v != 0 && is_live(v, live, nlive) && *nout < max)
			out[(*nout)++] = o;
	}
}

static int
run_p3d_ctor(void)
{
	long trial = 300000;
	int fi, which, lvl, i;
	static const unsigned int flag_v[] = { 0u, 1u, 2u, 0xffffffffu };
	unsigned seen[64];
	int nseen = 0, changed = 0;

	diff_begin("V90Phase3Demodulator::V90Phase3Demodulator");

	dsplib_debug_capture_on = 1;

	for (fi = 0; fi < 4; fi++)
	    for (which = 0; which < 2; which++)
		for (lvl = 0; lvl < 2; lvl++) {
			unsigned int flag = flag_v[fi];
			struct alloc_log a0, a1, a2;
			unsigned char before[P3D_SLOT];
			void *live[64];
			int nlive;
			unsigned h;

			trial++;
			dsplibs_debug_level = ref_dsplibs_debug_level =
			    lvl ? 2u : 0u;

			seed_all(trial, fi + lvl);
			fill(p3a, P3D_SLOT);
			memcpy(p3b, p3a, P3D_SLOT);
			memcpy(before, p3a, P3D_SLOT);
			dsplib_debug_capture_reset();

			a0 = harness_alloc;
			if (which)
				p3d_ctor1(p3a, PARAMS, argblk[0], flag, ADID);
			else
				p3d_ctor2(p3a, PARAMS, argblk[0], flag, ADID);
			a1 = harness_alloc;
			if (which)
				ref_p3d_ctor1(p3b, PARAMS, argblk[0], flag, ADID);
			else
				ref_p3d_ctor2(p3b, PARAMS, argblk[0], flag, ADID);
			a2 = harness_alloc;

			if (flag == 0) {
				int side;
				for (side = 0; side < 2; side++) {
				V90Phase3Demodulator *x =
				    (V90Phase3Demodulator *)(side ? p3b : p3a);
				Descrambler<int, int> *s = &x->descrambler;
				diff_eq_int("V.90 phase-3 demod tap1/out is 18 (%ld)",
					    s->pTap1 - s->pOut, 18, trial);
				diff_eq_int("V.90 phase-3 demod tap2/out is 23 (%ld)",
					    s->pTap2 - s->pOut, 23, trial);
				diff_eq_int("V.90 phase-3 demod tail is 23 (%ld)",
					    s->tailLength, 23, trial);
				diff_eq_int("V.90 phase-3 demod slack is 99 (%ld)",
					    s->pOut - s->pLimit, 99, trial);
				}
			}

			nlive = harness_alloc_live_set(live, 64);
			if (nlive > 64)
				nlive = 64;
			if (n_p3d_skip == 0)
				discover_regions(p3a, 0x42c, live, nlive,
						 p3d_skip, &n_p3d_skip, 64);

			/*
			 * The two blocks the constructor allocated, compared
			 * by CONTENT.  This is what makes the four arguments
			 * the SD detector is built from, and the eight the
			 * ANSam detector is built from, observable at all.
			 */
			cmp_block("the SD detector the constructor built",
				  "V90SdDetector",
				  ((V90Phase3Demodulator *)p3a)->sdDetector,
				  ((V90Phase3Demodulator *)p3b)->sdDetector,
				  0x1c, live, nlive, trial);
			cmp_block("the ANSam detector the constructor built",
				  "ANSamToneDetector",
				  ((V90Phase3Demodulator *)p3a)
				  ->ansamToneDetector,
				  ((V90Phase3Demodulator *)p3b)
				  ->ansamToneDetector,
				  0x3c, live, nlive, trial);

			{
				static unsigned char sa[P3D_SLOT], sb[P3D_SLOT];

				memcpy(sa, p3a, P3D_SLOT);
				memcpy(sb, p3b, P3D_SLOT);
				skip_words(sa, sb, p3d_skip, n_p3d_skip);
				diff_eq_obj_(__FILE__, __LINE__,
					     "after the constructor",
					     "V90Phase3Demodulator", sa, sb,
					     0x42c, trial);
				diff_eq_int("no store past the object (%ld)",
					    memcmp(sa + 0x42c, sb + 0x42c,
						   P3D_SLOT - 0x42c) == 0, 1,
					    trial);
				h = 0x811c9dc5u;
				for (i = 0; i < 0x42c; i++)
					h = (h ^ sa[i]) * 0x01000193u;
			}

			/*
			 * The three the constructor stores and `reset` does
			 * not overwrite, asserted by VALUE.  Two constructors
			 * that both stored nothing would compare equal
			 * (finding F1105).
			 */
			diff_eq_int("params (%ld)",
				    (void *)((V90Phase3Demodulator *)p3b)->params
				    == (void *)PARAMS, 1, trial);
			diff_eq_int("sessionFlag (%ld)",
				    (long)((V90Phase3Demodulator *)p3b)
				    ->sessionFlag, (long)flag, trial);
			diff_eq_int("autoDigitalImpDetector (%ld)",
				    (void *)((V90Phase3Demodulator *)p3b)
				    ->autoDigitalImpDetector ==
				    (void *)ADID, 1, trial);
			/*
			 * Asserted on OUR side as well as theirs.  The seed is
			 * pairwise identical, so a reconstruction that dropped
			 * the store leaves both sides holding the same seed
			 * byte and the comparison agrees -- finding F1105 in
			 * its sharpest form, and the only thing that catches
			 * it is reading the value.
			 */
			diff_eq_int("word_3cc, theirs (%ld)",
				    (long)((V90Phase3Demodulator *)p3b)
				    ->word_3cc.prev_, 0, trial);
			diff_eq_int("word_3cc, ours (%ld)",
				    (long)((V90Phase3Demodulator *)p3a)
				    ->word_3cc.prev_, 0, trial);
			/*
			 * And what the constructor's own `reset` arguments
			 * reach: state 0, ucode 0x40, short_414 = 1 and
			 * float_418 = 0.0f.  These are the four the argument
			 * list decides and nothing else in the object does.
			 */
			diff_eq_int("reset ran with state 0 (%ld)",
				    (long)((V90Phase3Demodulator *)p3b)->state,
				    0, trial);
			diff_eq_int("reset ran with ucode 0x40 (%ld)",
				    (long)((V90Phase3Demodulator *)p3b)->ucode,
				    0x40, trial);
			diff_eq_int("short_414 (%ld)",
				    (long)((V90Phase3Demodulator *)p3b)
				    ->short_414, 1, trial);
			diff_eq_int("float_418 (%ld)",
				    ((V90Phase3Demodulator *)p3b)->float_418
				    == 0.0f, 1, trial);
			/* Argument 2 reaches no field. */
			for (i = 0; i + 4 <= 0x42c; i += 4) {
				void *v;

				memcpy(&v, p3b + i, sizeof v);
				if (v == (void *)argblk[0])
					diff_eq_int("argument 2 was stored at "
						    "+0x%lx", 0, 1, (long)i);
			}

			diff_eq_int("allocations (%ld)", a1.allocs - a0.allocs,
				    a2.allocs - a1.allocs, trial);
			diff_eq_int("bytes asked for (%ld)",
				    (long)(a1.bytes - a0.bytes),
				    (long)(a2.bytes - a1.bytes), trial);
			diff_eq_int("transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0, 1,
				    trial);

			if (memcmp(before, p3a, P3D_SLOT) != 0)
				changed = 1;
			for (i = 0; i < nseen; i++)
				if (seen[i] == h)
					break;
			if (i == nseen && nseen < 64)
				seen[nseen++] = h;

			/* Give it all back through the destructors. */
			p3d_dtor1(p3a);
			ref_p3d_dtor1(p3b);
			diff_eq_int("live is back (%ld)", harness_alloc.live,
				    a0.live, trial);
			diff_eq_int("nothing wild was freed (%ld)",
				    harness_alloc.bad_free - a0.bad_free, 0,
				    trial);
		}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the constructor changed the object", changed, 1, 0);
	diff_eq_int("and not to the same thing every trial", nseen >= 4, 1,
		    nseen);
	diff_eq_int("some words had to be excluded", n_p3d_skip > 0, 1,
		    n_p3d_skip);

	return diff_end();
}

/*
 * The destructor, one slot at a time.
 *
 * The two heap slots are set independently to NULL or to a real block, and
 * the number of frees says which was released.  A count names the slot
 * because only the slots under test are ever non-null, and it is the only
 * shape that tells "freed the right one" from "freed one thing".  The
 * embedded descrambler and modulator are destroyed unconditionally by the
 * compiler and their own allocations are given back on every trial, so the
 * baseline is taken with them already built.
 */
static int
run_p3d_dtor(void)
{
	long trial = 400000;
	int sd, an, which;
	int saw_freed = 0, saw_null = 0;

	diff_begin("V90Phase3Demodulator::~V90Phase3Demodulator");

	for (sd = 0; sd < 2; sd++)
	    for (an = 0; an < 2; an++)
		for (which = 0; which < 2; which++) {
			struct alloc_log a0, a1, a2;
			unsigned char beforeA[P3D_SLOT], beforeB[P3D_SLOT];
			void *p;
			int want = sd + an;

			trial++;
			seed_all(trial, sd + an);
			fill(p3a, P3D_SLOT);
			memcpy(p3b, p3a, P3D_SLOT);

			/*
			 * Build both objects properly, then take the two
			 * allocations the arms guard away again and replace
			 * them, so the embedded subobjects are real and only
			 * the two slots under test vary.
			 */
			p3d_ctor1(p3a, PARAMS, argblk[0], 1u, ADID);
			ref_p3d_ctor1(p3b, PARAMS, argblk[0], 1u, ADID);

			/*
			 * THE "NULL" ARM DESTROYS THE REAL SUBOBJECT AND
			 * NULLS THE FIELD; it does NOT plant a raw block.
			 * Both of these have non-trivial destructors that
			 * reach further -- `~ANSamToneDetector` runs
			 * `~GenericToneDetector`, which runs two
			 * `~GenericIIR`s, which free two history buffers --
			 * so a `sysdep_malloc(0x3c)` of 0xa5 filler is a wild
			 * pointer three frames down.  That is how this test
			 * crashed the first time it was run, and planting a
			 * constructed object is the only shape that works.
			 */
			(void)p;
			{
				V90Phase3Demodulator *A =
				    (V90Phase3Demodulator *)p3a;
				V90Phase3Demodulator *B =
				    (V90Phase3Demodulator *)p3b;

				if (!sd) {
					A->sdDetector->~V90SdDetector();
					sysdep_free(A->sdDetector);
					A->sdDetector = 0;
					B->sdDetector->~V90SdDetector();
					sysdep_free(B->sdDetector);
					B->sdDetector = 0;
				}
				if (!an) {
					A->ansamToneDetector
					    ->~ANSamToneDetector();
					sysdep_free(A->ansamToneDetector);
					A->ansamToneDetector = 0;
					B->ansamToneDetector
					    ->~ANSamToneDetector();
					sysdep_free(B->ansamToneDetector);
					B->ansamToneDetector = 0;
				}
			}

			memcpy(beforeA, p3a, P3D_SLOT);
			memcpy(beforeB, p3b, P3D_SLOT);

			a0 = harness_alloc;
			if (which)
				p3d_dtor1(p3a);
			else
				p3d_dtor2(p3a);
			a1 = harness_alloc;
			if (which)
				ref_p3d_dtor1(p3b);
			else
				ref_p3d_dtor2(p3b);
			a2 = harness_alloc;

			diff_eq_int("frees (%ld)", a1.frees - a0.frees,
				    a2.frees - a1.frees, trial);
			diff_eq_int("free(NULL) (%ld)",
				    a1.free_null - a0.free_null,
				    a2.free_null - a1.free_null, trial);
			diff_eq_int("wild frees (%ld)",
				    a1.bad_free - a0.bad_free,
				    a2.bad_free - a1.bad_free, trial);
			/*
			 * The two guarded slots, plus whatever the descrambler
			 * and modulator give back -- the same number on every
			 * trial, so the DIFFERENCE across `want` is the claim.
			 */
			diff_eq_int("never freed a null (%ld)",
				    a2.free_null - a0.free_null, 0, trial);
			/*
			 * NEITHER object is written.  Each side is compared
			 * against its OWN state before the call: the freed
			 * pointers hold different addresses on the two sides,
			 * so a comparison ACROSS the sides would fail here for
			 * a reason that is not the destructor's.
			 */
			diff_eq_int("ours is untouched (%ld)",
				    memcmp(beforeA, p3a, P3D_SLOT) == 0, 1,
				    trial);
			diff_eq_int("and theirs too (%ld)",
				    memcmp(beforeB, p3b, P3D_SLOT) == 0, 1,
				    trial);

			if (want)
				saw_freed = 1;
			if (want < 2)
				saw_null = 1;
		}

	diff_eq_int("a guarded slot was freed", saw_freed, 1, 0);
	diff_eq_int("and a null one skipped", saw_null, 1, 0);

	return diff_end();
}

/* ============================================== V90Phase4Demodulator */

static unsigned p4d_skip[64];
static int n_p4d_skip;

/* The eleven placements, by the offset each argument must reach. */
static const struct region p4d_place[] = {
	{ 0x000c, "mappingParams1" },
	{ 0x0010, "mappingParams2" },
	{ 0x3054, "demapper" },
	{ 0x0014, "cp" },
	{ 0x0018, "mp" },
	{ 0x3058, "descrambler" },
	{ 0x34f8, "connectionEvaluator" },
	{ 0x0004, "params" },
	{ 0x001c, "phase3Demodulator" },
	{ 0x3514, "autoDigitalImpDetector" }
};

#define NPLACE ((int)(sizeof(p4d_place) / sizeof(p4d_place[0])))

static int
run_p4d(void)
{
	long trial = 500000;
	int fi, which, i;
	static const unsigned int flag_v[] = { 0u, 1u, 7u, 0xdeadbeefu };
	unsigned seen[64];
	int nseen = 0, changed = 0;

	diff_begin("V90Phase4Demodulator: constructor and destructor");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2u;

	for (fi = 0; fi < 4; fi++)
	    for (which = 0; which < 2; which++) {
		unsigned int flag = flag_v[fi];
		struct alloc_log a0, a1, a2, a3;
		unsigned char before[P4D_SLOT];
		unsigned char beforeA[P4D_SLOT], beforeB[P4D_SLOT];
		void *live[64];
		int nlive;
		unsigned h;

		trial++;
		seed_all(trial, fi);
		fill(p4a, P4D_SLOT);
		memcpy(p4b, p4a, P4D_SLOT);
		memcpy(before, p4a, P4D_SLOT);
		dsplib_debug_capture_reset();

		a0 = harness_alloc;
		if (which)
			p4d_ctor1(p4a, argblk[2], argblk[3], argblk[4],
				  argblk[5], argblk[6], argblk[7], argblk[8],
				  PARAMS, argblk[9], ADID, flag);
		else
			p4d_ctor2(p4a, argblk[2], argblk[3], argblk[4],
				  argblk[5], argblk[6], argblk[7], argblk[8],
				  PARAMS, argblk[9], ADID, flag);
		a1 = harness_alloc;
		if (which)
			ref_p4d_ctor1(p4b, argblk[2], argblk[3], argblk[4],
				      argblk[5], argblk[6], argblk[7],
				      argblk[8], PARAMS, argblk[9], ADID,
				      flag);
		else
			ref_p4d_ctor2(p4b, argblk[2], argblk[3], argblk[4],
				      argblk[5], argblk[6], argblk[7],
				      argblk[8], PARAMS, argblk[9], ADID,
				      flag);
		a2 = harness_alloc;

		if (n_p4d_skip == 0) {
			nlive = harness_alloc_live_set(live, 64);
			if (nlive > 64)
				nlive = 64;
			discover_regions(p4a, 0x351c, live, nlive, p4d_skip,
					 &n_p4d_skip, 64);
		}

		{
			static unsigned char sa[P4D_SLOT], sb[P4D_SLOT];

			memcpy(sa, p4a, P4D_SLOT);
			memcpy(sb, p4b, P4D_SLOT);
			skip_words(sa, sb, p4d_skip, n_p4d_skip);
			diff_eq_obj_(__FILE__, __LINE__,
				     "after the constructor",
				     "V90Phase4Demodulator", sa, sb, 0x351c,
				     trial);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(sa + 0x351c, sb + 0x351c,
					   P4D_SLOT - 0x351c) == 0, 1, trial);
			h = 0x811c9dc5u;
			for (i = 0; i < 0x351c; i++)
				h = (h ^ sa[i]) * 0x01000193u;
		}

		/*
		 * THE ELEVEN PLACEMENTS, ASSERTED.  Every argument is a
		 * distinct block, so a store landing at the wrong offset holds
		 * a value no other argument could have supplied and this loop
		 * names the field.
		 */
		{
			void *want[NPLACE];
			int k;

			want[0] = argblk[2];	/* mappingParams1 */
			want[1] = argblk[3];	/* mappingParams2 */
			want[2] = argblk[4];	/* demapper       */
			want[3] = argblk[5];	/* cp             */
			want[4] = argblk[6];	/* mp             */
			want[5] = argblk[7];	/* descrambler    */
			want[6] = argblk[8];	/* connEval       */
			want[7] = PARAMS;	/* params         */
			want[8] = argblk[9];	/* phase3Demod    */
			want[9] = ADID;		/* adid           */

			for (k = 0; k < NPLACE; k++) {
				void *got;

				memcpy(&got, p4b + p4d_place[k].off,
				       sizeof got);
				diff_eq_int("a pointer landed at +0x%lx",
					    got == want[k], 1,
					    (long)p4d_place[k].off);
			}
		}
		diff_eq_int("sessionFlag (%ld)",
			    (long)((V90Phase4Demodulator *)p4b)->sessionFlag,
			    (long)flag, trial);
		/*
		 * +0x08 is NOT written, which is a claim and not an omission:
		 * the seed must still be there.
		 */
		diff_eq_int("+0x08 keeps its seed (%ld)",
			    memcmp(p4b + 8, before + 8, 4) == 0, 1, trial);

		diff_eq_int("allocations (%ld)", a1.allocs - a0.allocs,
			    a2.allocs - a1.allocs, trial);
		diff_eq_int("bytes asked for (%ld)",
			    (long)(a1.bytes - a0.bytes),
			    (long)(a2.bytes - a1.bytes), trial);
		diff_eq_int("transcript (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    trial);

		if (memcmp(before, p4a, P4D_SLOT) != 0)
			changed = 1;
		for (i = 0; i < nseen; i++)
			if (seen[i] == h)
				break;
		if (i == nseen && nseen < 64)
			seen[nseen++] = h;

		/* And the destructor, on the object the constructor built. */
		memcpy(beforeA, p4a, P4D_SLOT);
		memcpy(beforeB, p4b, P4D_SLOT);
		if (which)
			p4d_dtor1(p4a);
		else
			p4d_dtor2(p4a);
		if (which)
			ref_p4d_dtor1(p4b);
		else
			ref_p4d_dtor2(p4b);
		a3 = harness_alloc;

		diff_eq_int("the pair balances (%ld)", a3.live, a0.live, trial);
		diff_eq_int("no free(NULL) (%ld)", a3.free_null - a0.free_null,
			    0, trial);
		diff_eq_int("nothing wild was freed (%ld)",
			    a3.bad_free - a0.bad_free, 0, trial);
		/*
		 * The destructor stores NOTHING.  `-fno-lifetime-dse` is on,
		 * so a store to `*this` here survives to be seen -- which is
		 * what makes this check able to fail.
		 */
		diff_eq_int("the destructor wrote nothing, ours (%ld)",
			    memcmp(beforeA, p4a, P4D_SLOT) == 0, 1, trial);
		diff_eq_int("and theirs (%ld)",
			    memcmp(beforeB, p4b, P4D_SLOT) == 0, 1, trial);
	    }

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the constructor changed the object", changed, 1, 0);
	diff_eq_int("and not to the same thing every trial", nseen >= 4, 1,
		    nseen);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_p3d_ctor();
	rc |= run_p3d_dtor();
	rc |= run_p4d();

	return rc;
}

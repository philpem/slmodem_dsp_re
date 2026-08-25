/*
 * t_v90demapctor.cpp -- differential test of `V90Demapper::V90Demapper` and,
 * around it, the constructor/destructor ROUND TRIP that no test of either half
 * alone can see.
 *
 * WHY THIS IS ITS OWN FILE and not another suite inside t_v90leaves.cpp:
 * finding F1264.  A mutation suite is a (source file, test binary) pair, and
 * two suites may share a source file -- `v90cd` and `v90cdctor` already do,
 * both naming V90ConstellationDesigner.cpp against different binaries.  What
 * must NOT be shared is anchor text: a block repeated between two functions
 * of one file matches twice, `mutate.py` calls that UNUSABLE, and UNUSABLE
 * does not fail a run.  So the constructor gets its own binary and its own
 * suite, and its anchors are taken from text that appears once.
 *
 * WHAT THE FIXTURE HAS TO DO THAT AN ORDINARY ONE WOULD NOT
 *
 * C++ has no syntax for running a constructor over storage that already
 * exists, so both sides go through asm() labels -- `_ZN11V90DemapperC1E...`
 * and the matching `ref_`-prefixed alias -- and BOTH the C1 and the C2
 * variant are driven.  The blob holds them as two identical 193-byte copies
 * at 0x30640 and 0x30710 and our compiler emits one function under both
 * names; calling only one leaves half the pair untested.  Writing
 * `OURS = V90Demapper(...)` instead would build a temporary over
 * uninitialised stack and copy it in, throwing away the seed the whole test
 * rests on (findings F223, F224).
 *
 * TWO WORDS CANNOT BE COMPARED AND ARE NOT.  `codes` and `signs` are
 * separate `sysdep_malloc`s on the two sides and hold different addresses for
 * ever.  What is compared instead is what those addresses stand for -- the
 * CONTENTS of both blocks over the exact length `sampleCapacity` implies, their
 * SIZES from `malloc_usable_size`, and the number of allocations and the
 * exact number of BYTES ASKED FOR.  That last one is the only check that can
 * see an allocation of the wrong size in a way the contents cannot: a block
 * allocated four times too large is never read past its first quarter.
 *
 * EVERY OTHER BYTE OF 7,864 IS COMPARED, INCLUDING THE TWO EMBEDDED OBJECTS.
 * The seed is varied and never zero, and both `ModulusDecoder::ModulusDecoder`
 * and `V90SignBitsExtractor::V90SignBitsExtractor` write zeros over part of
 * what they own -- so a reconstruction that dropped either call would leave
 * seeded bytes where the blob leaves zeros, at +0x648 and +0x668, and the
 * object comparison names the offset.  That is what makes the two member
 * constructions observable at all.
 */

#include <string.h>
#include <malloc.h>

#include "harness.h"
#include "dsplib/debug.h"
/*
 * The NAMED 0x558 `V90Parameters` map, the one V90Demapper.cpp compiles
 * against.  Two incompatible definitions exist and no translation unit may
 * include both; finding F1112.
 */
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Demapper.h"

extern "C" {
void dem_ctor1(void *self, unsigned int levels, void *params, void *adi)
	asm("_ZN11V90DemapperC1EjP13V90ParametersP25V90AutoDigitalImpDetector");
void dem_ctor2(void *self, unsigned int levels, void *params, void *adi)
	asm("_ZN11V90DemapperC2EjP13V90ParametersP25V90AutoDigitalImpDetector");
void ref_dem_ctor1(void *self, unsigned int levels, void *params, void *adi)
	asm("ref__ZN11V90DemapperC1EjP13V90ParametersP25V90AutoDigitalImpDetec"
	    "tor");
void ref_dem_ctor2(void *self, unsigned int levels, void *params, void *adi)
	asm("ref__ZN11V90DemapperC2EjP13V90ParametersP25V90AutoDigitalImpDetec"
	    "tor");

void dem_dtor1(void *self) asm("_ZN11V90DemapperD1Ev");
void dem_dtor2(void *self) asm("_ZN11V90DemapperD2Ev");
void ref_dem_dtor1(void *self) asm("ref__ZN11V90DemapperD1Ev");
void ref_dem_dtor2(void *self) asm("ref__ZN11V90DemapperD2Ev");

extern unsigned int ref_dsplibs_debug_level;

void sysdep_free(void *mem);
}

/* The object is 0x1eb8; the slot is larger so an overrunning store shows up. */
#define SLOT		(0x1eb8 + 64)
#define PARM_SLOT	(sizeof(V90Parameters) + 64)

static unsigned char ours[SLOT] __attribute__((aligned(8)));
static unsigned char theirs[SLOT] __attribute__((aligned(8)));

#define OURS	(*(V90Demapper *)ours)
#define THEIRS	(*(V90Demapper *)theirs)

/*
 * ONE parameter block and ONE detector, shared by the two sides.  Finding
 * F1105's rule: identical argument pointers give identical stored pointers, so
 * the two words the constructor copies compare equal and stay IN the object
 * comparison instead of being blanked out of it.  A per-side block would have
 * cost two more excluded words for nothing.
 */
static unsigned char parm[PARM_SLOT] __attribute__((aligned(8)));
static unsigned char adi_obj[64] __attribute__((aligned(8)));

#define PARAMS ((V90Parameters *)parm)

static void
seed(long trial)
{
	unsigned lfsr = 0x71b3u + 0x9e37u * (unsigned)trial;
	int i;

	for (i = 0; i < SLOT; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		/*
		 * `| 1` so no seeded byte is ever zero.  A zero in the seed is
		 * indistinguishable from a zero the constructor wrote, which
		 * is finding F230's whole point.
		 */
		v = (unsigned char)((lfsr >> 3) | 1u);
		ours[i] = v;
		theirs[i] = v;
	}
	for (i = 0; i < (int)PARM_SLOT; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		parm[i] = (unsigned char)((lfsr >> 3) | 1u);
	}
	for (i = 0; i < (int)sizeof(adi_obj); i++)
		adi_obj[i] = (unsigned char)(0x30 + i);
}

/*
 * The THREE words that hold three different addresses, and nothing else.
 *
 * The third is not the constructor's: `signBits` sits at +0x668 and its
 * `ParallelDifferentialDecoder<unsigned char>` at +0x1c inside it, so the
 * decoder's own six-byte buffer is a pointer at +0x684 that
 * `V90SignBitsExtractor::V90SignBitsExtractor` takes and
 * `~V90SignBitsExtractor` gives back.  It is a THIRD allocation this
 * constructor causes without naming, which is why the block count below is
 * three and the byte total carries a `+ V90SBE_DECODER_SIZE`.
 */
#define SBE_DECODER_PTR	0x684u

static const unsigned skip_v[] = { 0x1c, 0x20, SBE_DECODER_PTR, ~0u };

static unsigned char scratch[2][SLOT];

static void *
ptr_at(const unsigned char *o, unsigned off)
{
	void *p;

	memcpy(&p, o + off, sizeof p);
	return p;
}

static void
cmp_dem(const char *what, long trial)
{
	int i;

	memcpy(scratch[0], ours, SLOT);
	memcpy(scratch[1], theirs, SLOT);
	for (i = 0; skip_v[i] != ~0u; i++) {
		memset(scratch[0] + skip_v[i], 0, 4);
		memset(scratch[1] + skip_v[i], 0, 4);
	}
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Demapper", scratch[0],
		     scratch[1], sizeof(V90Demapper), trial);
	diff_eq_int("no store past the object (%ld)",
		    memcmp(scratch[0] + sizeof(V90Demapper),
			   scratch[1] + sizeof(V90Demapper),
			   SLOT - sizeof(V90Demapper)) == 0, 1, trial);
}

/*
 * A cheap digest of one side's object, so the sweep can assert that the
 * constructor did not write the SAME thing every trial.  Two constructors
 * that both ignore their arguments agree with each other perfectly, which is
 * finding F224's failure: the comparison passes and proves nothing.
 */
static unsigned
digest(const unsigned char *o)
{
	unsigned h = 0x811c9dc5u;
	int i;

	for (i = 0; i < (int)sizeof(V90Demapper); i++) {
		if (i >= 0x1c && i < 0x24)
			continue;		/* the two addresses */
		if (i >= (int)SBE_DECODER_PTR && i < (int)SBE_DECODER_PTR + 4)
			continue;		/* and the decoder's     */
		h = (h ^ o[i]) * 0x01000193u;
	}
	return h;
}

static const unsigned int level_v[] = { 0u, 1u, 2u, 3u, 7u, 16u, 64u, 128u };
#define NLEVEL ((int)(sizeof(level_v) / sizeof(level_v[0])))

static int
run_ctor(void)
{
	long trial = 100000;
	int li, which, lvl, i;
	unsigned seen[64];
	int nseen = 0;
	int changed = 0;

	diff_begin("V90Demapper::V90Demapper");

	dsplib_debug_capture_on = 1;

	for (li = 0; li < NLEVEL; li++)
	    for (which = 0; which < 2; which++)
		for (lvl = 0; lvl < 2; lvl++) {
			unsigned int levels = level_v[li];
			struct alloc_log a0, a1, a2;
			unsigned char before[SLOT];
			void *pa, *pb, *qa, *qb, *ra, *rb;
			unsigned d;

			trial++;
			dsplibs_debug_level = ref_dsplibs_debug_level =
			    lvl ? 2u : 0u;

			seed(trial);
			memcpy(before, ours, SLOT);
			dsplib_debug_capture_reset();

			a0 = harness_alloc;
			if (which)
				dem_ctor1(ours, levels, PARAMS, adi_obj);
			else
				dem_ctor2(ours, levels, PARAMS, adi_obj);
			a1 = harness_alloc;
			if (which)
				ref_dem_ctor1(theirs, levels, PARAMS, adi_obj);
			else
				ref_dem_ctor2(theirs, levels, PARAMS, adi_obj);
			a2 = harness_alloc;

			cmp_dem("after the constructor", trial);

			/*
			 * THE FILL IS GONE, so the values the object must hold
			 * are asserted and not only compared: two constructors
			 * that both did nothing would agree (finding F1105).
			 */
			diff_eq_int("sampleCapacity (%ld)", (long)THEIRS.sampleCapacity,
				    (long)levels, trial);
			diff_eq_int("params (%ld)",
				    (void *)THEIRS.params == (void *)PARAMS, 1,
				    trial);
			diff_eq_int("adiDetector (%ld)",
				    (void *)THEIRS.adiDetector ==
				    (void *)adi_obj, 1, trial);
			diff_eq_int("signDecoder (%ld)",
				    (long)THEIRS.signDecoder.prev_, 0,
				    trial);
			diff_eq_int("errorHistogramCount (%ld)",
				    (long)THEIRS.errorHistogramCount, 0, trial);
			diff_eq_int("bitsPerFrame (%ld)", (long)THEIRS.bitsPerFrame, 0,
				    trial);
			diff_eq_int("word_08 (%ld)", (long)THEIRS.word_08, 0,
				    trial);
			diff_eq_int("signBitsPerFrame (%ld)",
				    (long)THEIRS.signBitsPerFrame, 0,
				    trial);
			diff_eq_int("signBitGroups (%ld)",
				    (long)THEIRS.signBitGroups, 0,
				    trial);
			diff_eq_int("signBitGroupSize (%ld)",
				    (long)THEIRS.signBitGroupSize, 0,
				    trial);
			diff_eq_int("rbsFramePosition (%ld)",
				    (long)THEIRS.rbsFramePosition, 0,
				    trial);
			diff_eq_int("frameStart (%ld)", (long)THEIRS.frameStart, 0,
				    trial);
			diff_eq_int("sampleCount (%ld)", (long)THEIRS.sampleCount, 0,
				    trial);
			for (i = 0; i < V90DEMAPPER_CONSTELLATIONS; i++)
				diff_eq_int("constellationSize[%ld]",
					    (long)THEIRS.constellationSize[i], 0,
					    i);

			/*
			 * THE ARRAYS AT +0x30, +0x690 AND +0x1290 ARE NOT
			 * TOUCHED, and that is a claim worth making rather than
			 * leaving to the byte comparison: the seed is still
			 * there.  A constructor that helpfully cleared the
			 * histogram would pass every check above and this one
			 * catches it.
			 */
			diff_eq_int("the constellation is still the seed (%ld)",
				    memcmp(theirs + 0x30, before + 0x30,
					   0x630 - 0x30) == 0, 1, trial);
			diff_eq_int("the histograms are still the seed (%ld)",
				    memcmp(theirs + 0x690, before + 0x690,
					   0x1e90 - 0x690) == 0, 1, trial);

			/* The allocator's view, and the two blocks. */
			diff_eq_int("allocations (%ld)", a1.allocs - a0.allocs,
				    a2.allocs - a1.allocs, trial);
			diff_eq_int("bytes asked for (%ld)",
				    (long)(a1.bytes - a0.bytes),
				    (long)(a2.bytes - a1.bytes), trial);
			diff_eq_int("exactly three blocks (%ld)",
				    a1.allocs - a0.allocs, 3, trial);

			pa = ptr_at(ours, 0x1c);
			pb = ptr_at(theirs, 0x1c);
			qa = ptr_at(ours, 0x20);
			qb = ptr_at(theirs, 0x20);
			ra = ptr_at(ours, SBE_DECODER_PTR);
			rb = ptr_at(theirs, SBE_DECODER_PTR);
			diff_eq_int("both blocks exist (%ld)",
				    pa != 0 && pb != 0 && qa != 0 && qb != 0, 1,
				    trial);
			diff_eq_int("the decoder's buffer exists (%ld)",
				    ra != 0 && rb != 0, 1, trial);
			if (ra && rb) {
				diff_eq_int("decoder buffer contents (%ld)",
					    memcmp(ra, rb, V90SBE_DECODER_SIZE)
					    == 0, 1, trial);
				diff_eq_int("decoder buffer size (%ld)",
					    (long)malloc_usable_size(ra),
					    (long)malloc_usable_size(rb), trial);
			}
			if (pa && pb) {
				diff_eq_int("codes contents (%ld)",
					    memcmp(pa, pb, levels * 4) == 0, 1,
					    trial);
				diff_eq_int("codes size (%ld)",
					    (long)malloc_usable_size(pa),
					    (long)malloc_usable_size(pb), trial);
			}
			if (qa && qb) {
				diff_eq_int("signs contents (%ld)",
					    memcmp(qa, qb, levels) == 0, 1,
					    trial);
				diff_eq_int("signs size (%ld)",
					    (long)malloc_usable_size(qa),
					    (long)malloc_usable_size(qb), trial);
			}
			/*
			 * THE TWO WIDTHS ARE FOUR AND ONE, asserted against the
			 * byte total rather than only compared.  `levels * 4`
			 * and `levels` are the only pair whose sum is this, and
			 * a reconstruction that allocated four bytes for both
			 * would agree with the blob on every content check --
			 * the extra bytes are never read.  The constant is the
			 * sign-bit extractor's decoder buffer, which the
			 * constructor causes without naming.
			 */
			diff_eq_int("bytes = levels * 5 + 6 (%ld)",
				    (long)(a1.bytes - a0.bytes),
				    (long)(levels * 5u + V90SBE_DECODER_SIZE),
				    trial);

			diff_eq_int("transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0, 1,
				    trial);

			if (memcmp(before, ours, SLOT) != 0)
				changed = 1;
			d = digest(theirs);
			for (i = 0; i < nseen; i++)
				if (seen[i] == d)
					break;
			if (i == nseen && nseen < (int)(sizeof(seen) /
							sizeof(seen[0])))
				seen[nseen++] = d;

			sysdep_free(pa);
			sysdep_free(pb);
			sysdep_free(qa);
			sysdep_free(qb);
			sysdep_free(ra);
			sysdep_free(rb);
			diff_eq_int("nothing wild was freed (%ld)",
				    harness_alloc.bad_free - a0.bad_free, 0,
				    trial);
			diff_eq_int("live is back (%ld)", harness_alloc.live,
				    a0.live, trial);
		}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	/* Anti-vacuity: findings F223 and F224. */
	diff_eq_int("the constructor changed the object", changed, 1, 0);
	diff_eq_int("and not to the same thing every trial", nseen >= 8, 1,
		    nseen);

	return diff_end();
}

/*
 * THE ROUND TRIP.  Neither half alone can see this: the constructor test
 * frees the two blocks itself and the destructor test in t_v90leaves.cpp
 * plants blocks of its own.  What is left over is whether the pair BALANCES
 * -- whether the destructor frees exactly what the constructor took, on an
 * object neither test built by hand.
 *
 * `DEBUG_DEMAPPER_ERROR_HISTOGRAM` runs both ways, because the destructor's
 * first act is gated on it and the printing arm increments a field.  Both
 * arms end in the same two frees, and neither may free a NULL: this tree's
 * `sysdep_free` tolerates one, so dropping the destructor's two `if`s changes
 * no byte anywhere and moves only `harness_alloc.free_null`.
 */
static int
run_round_trip(void)
{
	long trial = 200000;
	int li, which, dbg;
	int saw_print = 0, saw_quiet = 0;

	diff_begin("V90Demapper: construct then destroy");

	dsplib_debug_capture_on = 1;

	for (li = 0; li < NLEVEL; li++)
	    for (which = 0; which < 2; which++)
		for (dbg = 0; dbg < 2; dbg++) {
			unsigned int levels = level_v[li];
			struct alloc_log a0, a1, a2, a3;

			trial++;
			dsplibs_debug_level = ref_dsplibs_debug_level = 2u;

			seed(trial);
			PARAMS->DEBUG_DEMAPPER_ERROR_HISTOGRAM = dbg;
			dsplib_debug_capture_reset();

			a0 = harness_alloc;
			if (which) {
				dem_ctor1(ours, levels, PARAMS, adi_obj);
				ref_dem_ctor1(theirs, levels, PARAMS, adi_obj);
			} else {
				dem_ctor2(ours, levels, PARAMS, adi_obj);
				ref_dem_ctor2(theirs, levels, PARAMS, adi_obj);
			}
			a1 = harness_alloc;

			if (which)
				dem_dtor1(ours);
			else
				dem_dtor2(ours);
			a2 = harness_alloc;
			if (which)
				ref_dem_dtor1(theirs);
			else
				ref_dem_dtor2(theirs);
			a3 = harness_alloc;

			diff_eq_int("frees (%ld)", a2.frees - a1.frees,
				    a3.frees - a2.frees, trial);
			diff_eq_int("the pair balances (%ld)",
				    a3.live, a0.live, trial);
			diff_eq_int("freed exactly what it took (%ld)",
				    a2.frees - a1.frees,
				    (a1.allocs - a0.allocs) / 2, trial);
			diff_eq_int("no free(NULL) (%ld)",
				    a3.free_null - a0.free_null, 0, trial);
			diff_eq_int("nothing wild was freed (%ld)",
				    a3.bad_free - a0.bad_free, 0, trial);
			/*
			 * Both sides' objects are compared AFTER the
			 * destructor too: it does not clear the pointers it
			 * frees, so everything except those two words must
			 * still agree, and the histogram counter must have
			 * moved on exactly the arm that prints.
			 */
			cmp_dem("after the destructor", trial);
			diff_eq_int("the histogram counter (%ld)",
				    (long)THEIRS.errorHistogramCount,
				    dbg ? 1 : 0, trial);
			diff_eq_int("transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0, 1,
				    trial);

			if (dbg)
				saw_print = 1;
			else
				saw_quiet = 1;
		}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the histogram arm was taken", saw_print, 1, 0);
	diff_eq_int("and skipped", saw_quiet, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_ctor();
	rc |= run_round_trip();

	return rc;
}

/*
 * t_cevalleaves.cpp -- differential test of the two V90ConnectionEvaluator
 * leaves claimed by the VPcmV34Main leaf pass:
 *
 *     evaluateMeanErrorStdPhase3(float)   0x3f9d0  234 B
 *     printStatus() const                 0x40140  177 B
 *
 * Both callerless exported API, driven through the `ref_` aliases (F7000).
 *
 * `evaluateMeanErrorStdPhase3` is one gated comparison: parameter enable off
 * and on, the standard deviation below, exactly at, and above the parameter
 * threshold, both signs -- the `%c%d.%04d` in the diagnostic needs the
 * negative-std trial or the sign selection is never exercised.  The RETURN
 * is compared (0 or the verdict 5), the diagnostic transcript is compared,
 * and the three counters it clears are pre-seeded non-zero so a body that
 * stopped clearing one could not pass on seed agreeing with seed (F223).
 *
 * NO NaN REACHES THE COMPARISON.  The object's `fcomp` is the NaN-blind
 * ordered kind and GCC 13's `<=` is not -- the class's other members carry
 * exactly that divergence (t_v90p4dnan and friends), and this leaf declines
 * to buy a divergence entry for an input no caller can produce.
 *
 * `printStatus` is a transcript test: nine `edprintf` lines, level swept,
 * the counters seeded differently per trial so the transcript must vary.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90ConnectionEvaluator.h"
#include "dsplib/V90Parameters.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

int ref_ce_evalstd3(void *self, float std)
	asm("ref__ZN22V90ConnectionEvaluator26evaluateMeanErrorStdPhase3Ef");
void ref_ce_printstatus(const void *self)
	asm("ref__ZNK22V90ConnectionEvaluator11printStatusEv");
}

#define GUARD	64
#define CE_SLOT	((unsigned)sizeof(V90ConnectionEvaluator) + GUARD)

static unsigned char ce[2][CE_SLOT] __attribute__((aligned(8)));
static unsigned char par[sizeof(V90Parameters)] __attribute__((aligned(8)));
static unsigned char before[CE_SLOT];

#define CE(s)	((V90ConnectionEvaluator *)(void *)ce[s])
#define PAR	((V90Parameters *)(void *)par)

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)((lfsr >> 3) | 1u);
}

static void
seed(long trial)
{
	unsigned i;

	lfsr = 0x40c1u ^ (unsigned)trial * 0x9e37u;
	for (i = 0; i < sizeof par; i++)
		par[i] = nextb();
	for (i = 0; i < CE_SLOT; i++)
		ce[0][i] = nextb();
	memcpy(ce[1], ce[0], CE_SLOT);
	CE(0)->params = CE(1)->params = PAR;
	memcpy(before, ce[0], CE_SLOT);
}

static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

static int
run_evalstd3(void)
{
	static const float threshes[] = { 0.75f, -2.5f, 100.0f };
	long tag = 0;
	int en, ti, d, lvl;
	int fired = 0, spared = 0;

	diff_begin("V90ConnectionEvaluator::evaluateMeanErrorStdPhase3");

	for (en = 0; en < 2; en++)
	    for (ti = 0; ti < (int)(sizeof threshes / sizeof threshes[0]);
		 ti++)
		for (d = -1; d <= 1; d++)
		    for (lvl = 0; lvl < 2; lvl++) {
			float thresh = threshes[ti];
			float std = thresh + (float)d * 0.125f;
			int r0, r1;

			seed(tag);
			PAR->TRN1D_MEAN_ERROR_STD_EVALUATION_ENABLE = en;
			PAR->TRN1D_MAX_MEAN_ERROR_STD_IN_PHASE3 = thresh;

			set_level(lvl ? 2u : 1u);
			dsplib_debug_capture_reset();
			dsplib_debug_capture_on = 1;
			r0 = CE(0)->evaluateMeanErrorStdPhase3(std);
			r1 = ref_ce_evalstd3(ce[1], std);
			dsplib_debug_capture_on = 0;
			set_level(0);

			diff_eq_int("the verdict (%ld)", r0, r1, tag);
			diff_eq_obj("after the evaluation",
				    V90ConnectionEvaluator, CE(0), CE(1),
				    tag);
			diff_eq_int("the guard held (%ld)",
				    memcmp(ce[0]
					   + sizeof(V90ConnectionEvaluator),
					   before
					   + sizeof(V90ConnectionEvaluator),
					   GUARD) == 0, 1, tag);
			diff_eq_int("the transcripts agree (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1))
				    == 0, 1, tag);

			if (en && d >= 0) {
				diff_eq_int("the verdict fired (%ld)", r0, 5,
					    tag);
				diff_eq_int("and cleared the counters (%ld)",
					    CE(0)->nofV90Retrains == 0
					    && CE(0)->word_1c == 0
					    && CE(0)->word_90 == 0, 1, tag);
				fired++;
			} else {
				diff_eq_int("the verdict did not fire (%ld)",
					    r0, 0, tag);
				diff_eq_int("and the seed survived (%ld)",
					    memcmp(before, ce[0], CE_SLOT)
					    == 0, 1, tag);
				spared++;
			}
			tag++;
		    }

	diff_eq_int("the firing arm was reached", fired > 0, 1, 0);
	diff_eq_int("the quiet arm was reached", spared > 0, 1, 0);

	return diff_end();
}

static int
run_printstatus(void)
{
	long tag = 1000;
	int trial, distinct = 0, nonEmpty = 0;
	char first[256];

	diff_begin("V90ConnectionEvaluator::printStatus");

	first[0] = 0;
	for (trial = 0; trial < 12; trial++) {
		unsigned lvl = (unsigned)(trial % 3);

		seed(tag);

		set_level(lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		CE(0)->printStatus();
		ref_ce_printstatus(ce[1]);
		dsplib_debug_capture_on = 0;
		set_level(0);

		diff_eq_int("the transcripts agree (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    tag);
		diff_eq_obj("printStatus left the object alone",
			    V90ConnectionEvaluator, CE(0), CE(1), tag);
		diff_eq_int("and really left it alone (%ld)",
			    memcmp(before, ce[0], CE_SLOT) == 0, 1, tag);

		if (lvl > 0 && strlen(dsplib_debug_capture_text(1)) > 0) {
			nonEmpty = 1;
			if (first[0] == 0) {
				strncpy(first, dsplib_debug_capture_text(1),
					255);
				first[255] = 0;
			} else if (strncmp(first,
					   dsplib_debug_capture_text(1),
					   255) != 0) {
				distinct = 1;
			}
		}
		tag++;
	}

	diff_eq_int("something was printed", nonEmpty, 1, 0);
	diff_eq_int("the counters reach the output", distinct, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_evalstd3();
	rc |= run_printstatus();

	return rc;
}

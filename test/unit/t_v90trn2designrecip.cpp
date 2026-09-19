/*
 * t_v90trn2designrecip.cpp -- the reciprocal and the retry cap, in their own
 * binary on purpose.
 *
 * `V90TRN2Designer::V90TRN2Design` seeds its iterative search with
 * `x * (1.0f / (nof - 0.5f))`.  The object computes that as a RECIPROCAL in
 * x87 extended precision and then a multiply, so it rounds twice.  At a few
 * inputs the two roundings land on opposite sides of the `(short)` truncation
 * and the design itself diverges -- a different `dMin` takes a different
 * constellation, and the retry cap's round count crosses 199.
 *
 * THE OBJECT'S OWN INSTRUCTION SEQUENCE at 0x3cece..0x3cf30 is
 *
 *     fld1
 *     filds  ucode                  ; the ucode value
 *     fildl  nof                    ; the sequence count
 *     fsub   <0.5f>
 *     fdivr  %st(3),%st             ; 1 / (nof - 0.5), in extended
 *     fmulp  %st,%st(2)             ; ucode * that, rounded once here
 *     fistps 0xb6(%esp)
 *
 * -- the reciprocal is never stored, so the only rounding before the
 * `fistps` is the subtract that forms `nof - 0.5`.  The modern build
 * (`build/repro/pump/v90/V90TRN2dDesigner.o` at 0x454..0x48c) is
 *
 *     filds 0x8(%esp)               ; ucode
 *     fildl 0x8(%esp)               ; nof
 *     fsubs <0.5f>
 *     fdivrp %st,%st(1)             ; ucode / (nof - 0.5), ONE rounding
 *     fistps 0x34(%esp)
 *
 * GCC 14's `-ffast-math` reassociates `x * (1.0f / y)` into `x / y`.  That
 * is one rounding where the object rounds twice, and the result crosses the
 * `(short)` truncation.  Finding F11369 records the search that found the
 * separating input (N = 21, level = 41: the reciprocal gives dMin 1 and the
 * divide gives 2); `make period` passes every check in this file with the
 * object's own compiler, which is the tier that decides.
 *
 * WHY IT IS NOT IN `t_v90trn2design`, and this is the whole reason the file
 * exists.  These four checks made the whole BINARY exit non-zero on the
 * modern build, and `tools/mutate.py` judges a mutant caught by a non-zero
 * exit -- so it cannot score a mutation set against a baseline that is
 * already red, and it refuses.  `t_v90trn2design` carries 23 mutations.
 * Splitting the divergent checks into their own binary is what that refusal
 * asks for; `t_v90p4dnan`, `t_v92ecnan` and `t_v90adidnan` are the worked
 * precedents (findings F2157, F3002, F6000-F6002).  The split-off binary has
 * no mutation suite.
 *
 * THE RECIPROCAL TRIAL IS DESIGNED, NOT SEARCHED.  The table is linear and
 * fully permitted so that both spellings SUCCEED and the comparison is
 * between two designs rather than between two failures, which
 * `setTrn2DummyConstel` would make identical again.
 *
 * THE CAP SWEEP IS APPROACHED FROM BELOW.  The iterative arm gives up after
 * 199 rounds, and a cap only shows where a design SUCCEEDS later than the
 * mutated cap would and no later than the real one.  Raising the scan's
 * lower bound starves the search a little at a time, so the round count
 * rises smoothly with it; sweeping the bound across its whole range is what
 * puts some trial in that window.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/V90TRN2Designer.h"
#include "dsplib/V90ConstellationPower.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Parameters.h"

extern "C" {

short our_trn2_design(void *, void *, void *, void *, void *, void *,
		      int, int, int, void *, unsigned int, int, int)
	asm("_ZN15V90TRN2Designer13V90TRN2DesignEP16V90MappingParamsPA128_sS3_"
	    "PA128_hPs7PcmTypeS7_sPhjh28V90SpecialSpectralConditions");
short ref_trn2_design(void *, void *, void *, void *, void *, void *,
		      int, int, int, void *, unsigned int, int, int)
	asm("ref__ZN15V90TRN2Designer13V90TRN2DesignEP16V90MappingParamsPA128_"
	    "sS3_PA128_hPs7PcmTypeS7_sPhjh28V90SpecialSpectralConditions");
extern unsigned int dsplibs_debug_level;

}

#define GUARD	64

static unsigned char designer[8 + GUARD] __attribute__((aligned(8)));

static unsigned char paramsOurs[0x558 + GUARD] __attribute__((aligned(8)));
static unsigned char paramsTheirs[0x558 + GUARD] __attribute__((aligned(8)));

#define MP_BYTES	((int)sizeof(V90MappingParams))

static unsigned char mp2Ours[MP_BYTES + GUARD] __attribute__((aligned(8)));
static unsigned char mp2Theirs[MP_BYTES + GUARD] __attribute__((aligned(8)));

static unsigned char powerOurs[256] __attribute__((aligned(8)));
static unsigned char powerTheirs[256] __attribute__((aligned(8)));

static unsigned lfsr;

static unsigned char
next_byte(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)(lfsr >> 3);
}

/*
 * Neither side is ever zeroed: both blocks get the same varied fill and the
 * seed moves with the trial, so "equal afterwards" is a statement about
 * varied content rather than about zeros agreeing with themselves (findings
 * F223, F224).
 */
static void
fill_pair(unsigned char *a, unsigned char *b, int n, int trial)
{
	int i;

	lfsr = 0x51a3u + 0x9e37u * (unsigned)trial + 1u;
	for (i = 0; i < n; i++) {
		unsigned char v = next_byte();

		a[i] = v;
		b[i] = v;
	}
}

static void
set_designer(void *params)
{
	int i;

	for (i = 8; i < 8 + GUARD; i++)
		designer[i] = (unsigned char)(0x5a + i);
	((V90TRN2Designer *)designer)->params = (V90Parameters *)params;
	((V90TRN2Designer *)designer)->power = 0;
}

#define ROWS		6
#define ROWLEN		128
#define SLACK		512
#define TABLE		(ROWS * ROWLEN + SLACK)

static short tabUcode[TABLE];
static short tabAlt[TABLE];
static unsigned char tabAllow[TABLE];
static short tabDmin[ROWS];
static unsigned char tabTop[ROWS];

/*
 * THE RECIPROCAL, SEPARATED.  `x * (1.0f / (N - 0.5f))` and `x / (N - 0.5f)`
 * agree over almost every input and differ in the last bits, which only shows
 * once the `(short)` truncation lands on the far side of an integer.  Searched
 * rather than guessed, in 80-bit arithmetic over every `(N, level)` up to
 * 32 x 32768: the first pair that separates is N = 21, level = 41, where the
 * reciprocal gives dMin 1 and the divide gives 2.
 */
static int
run_reciprocal(void)
{
	V90Parameters *pp = (V90Parameters *)paramsOurs;
	int k, i;
	short a, b;

	diff_begin("V90TRN2Designer::V90TRN2Design: the reciprocal, two "
		   "roundings against one");

	for (i = 0; i < TABLE; i++) {
		tabUcode[i] = 0;
		tabAlt[i] = 0;
		tabAllow[i] = 1;
	}
	for (k = 0; k < ROWS; k++) {
		for (i = 0; i < ROWLEN; i++) {
			tabUcode[k * ROWLEN + i] = (short)i;
			tabAlt[k * ROWLEN + i] = (short)i;
		}
		tabTop[k] = 41;
		tabDmin[k] = 1;
	}
	fill_pair(paramsOurs, paramsTheirs, 0x558 + GUARD, 11);
	pp->nofUcodesInTrn2 = 21;
	pp->maxUcode = 92;
	pp->unnamed_360 = 0;
	pp->SPECTRAL_SHAPER_SR = 3;
	pp->SPECTRAL_SHAPER_ID = 5;
	pp->SPECTRAL_SHAPER_A1 = 1.0f;
	pp->SPECTRAL_SHAPER_A2 = 0.0f;
	pp->SPECTRAL_SHAPER_B1 = 0.0f;
	pp->SPECTRAL_SHAPER_B2 = 0.0f;

	fill_pair(mp2Ours, mp2Theirs, MP_BYTES + GUARD, 11);
	fill_pair(powerOurs, powerTheirs, 256, 12);
	dsplibs_debug_level = 0;
	set_designer(paramsOurs);
	((V90TRN2Designer *)designer)->power = (V90ConstellationPower *)powerOurs;
	a = our_trn2_design(designer, mp2Ours, tabUcode, tabAlt, tabAllow,
			    tabDmin, 1, 1, 0, tabTop, 9u, 7, 0);
	((V90TRN2Designer *)designer)->power = (V90ConstellationPower *)powerTheirs;
	b = ref_trn2_design(designer, mp2Theirs, tabUcode, tabAlt, tabAllow,
			    tabDmin, 1, 1, 0, tabTop, 9u, 7, 0);

	diff_eq_int("the reciprocal case returned", a, b, 0);
	diff_eq_obj("the reciprocal case designed", V90MappingParams,
		    mp2Ours, mp2Theirs, 0);
	/*
	 * The trial is only a separator if it DESIGNED: a failure on both
	 * sides is `setTrn2DummyConstel`'s output either way and separates
	 * nothing.  Assert the outcome the search predicted.
	 */
	diff_eq_int("the reciprocal case designed rather than failed", a, 1,
		    0);
	diff_eq_int("and it took consecutive ucodes, so dMin was 1",
		    ((V90MappingParams *)mp2Ours)->constellation[0][1], 40, 0);

	return diff_end();
}

/*
 * THE RETRY CAP, approached from below.  A cap only shows in behaviour where
 * a design SUCCEEDS later than the mutated cap and no later than the real
 * one.  The starting dMin is `top / (N - 0.5)` and the spacing the ramp can
 * actually supply is 1, so the step sets how far dMin has to fall and
 * therefore how many rounds the retry takes -- from one at the bottom of the
 * sweep to past the cap at the top.
 */
static int
run_cap(void)
{
	V90Parameters *pp = (V90Parameters *)paramsOurs;
	int k, i, lo;
	int sawDesign = 0, sawGiveUp = 0;

	diff_begin("V90TRN2Designer::V90TRN2Design: the retry cap approached "
		   "from below");

	for (i = 0; i < TABLE; i++) {
		tabUcode[i] = 0;
		tabAlt[i] = 0;
		tabAllow[i] = 1;
	}
	for (lo = 120; lo < 6000; lo += 11) {
		short a, b;

		/*
		 * A UNIT RAMP WITH A STEP ON TOP, and the step is what is
		 * swept.
		 */
		for (k = 0; k < ROWS; k++) {
			for (i = 0; i < ROWLEN; i++) {
				tabUcode[k * ROWLEN + i] =
					(short)(i == 120 ? lo : i);
				tabAlt[k * ROWLEN + i] =
					(short)(i == 120 ? lo : i);
			}
			tabTop[k] = 120;
			tabDmin[k] = 1;
		}
		fill_pair(paramsOurs, paramsTheirs, 0x558 + GUARD, lo);
		pp->nofUcodesInTrn2 = 21;
		pp->maxUcode = 200;
		pp->unnamed_360 = 0;
		pp->SPECTRAL_SHAPER_SR = 3;
		pp->SPECTRAL_SHAPER_ID = 5;
		pp->SPECTRAL_SHAPER_A1 = 1.0f;
		pp->SPECTRAL_SHAPER_A2 = 0.0f;
		pp->SPECTRAL_SHAPER_B1 = 0.0f;
		pp->SPECTRAL_SHAPER_B2 = 0.0f;

		fill_pair(mp2Ours, mp2Theirs, MP_BYTES + GUARD, lo);
		fill_pair(powerOurs, powerTheirs, 256, lo + 1);
		dsplibs_debug_level = 0;
		set_designer(paramsOurs);
		((V90TRN2Designer *)designer)->power =
			(V90ConstellationPower *)powerOurs;
		a = our_trn2_design(designer, mp2Ours, tabUcode, tabAlt,
				    tabAllow, tabDmin, 1, 1, 0, tabTop, 9u,
				    7, 0);
		((V90TRN2Designer *)designer)->power =
			(V90ConstellationPower *)powerTheirs;
		b = ref_trn2_design(designer, mp2Theirs, tabUcode, tabAlt,
				    tabAllow, tabDmin, 1, 1, 0, tabTop, 9u,
				    7, 0);
		diff_eq_int("the cap sweep returned at step %ld", a, b, lo);
		diff_eq_obj("the cap sweep designed", V90MappingParams,
			    mp2Ours, mp2Theirs, lo);
		if (a)
			sawDesign++;
		else
			sawGiveUp++;
	}
	diff_eq_int("the cap sweep designed on %ld steps", sawDesign > 0, 1,
		    sawDesign);
	diff_eq_int("the cap sweep gave up on %ld steps", sawGiveUp > 0, 1,
		    sawGiveUp);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_reciprocal();
	rc |= run_cap();

	return rc;
}
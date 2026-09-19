/*
 * t_v90adidrecip.cpp -- the three accumulator-to-mapping methods, in their own
 * binary on purpose.
 *
 * `updateLinMappMeanAndVar` and `updateUrefAlt` multiply by a reciprocal and
 * `updateLinMappMeanAndVarAlt` divides directly.  The object's arithmetic is
 * visible at 0x41199 in `updateUrefAlt`:
 *
 *     flds 0x33c                 ; 1.0f, kept on the x87 stack across the loop
 *     ...
 *     fildll count
 *     fdivr %st(2),%st           ; 1 / count, in x87 extended
 *     fmuls 0x9d18(%esi,%ebx,4)  ; * the sum, rounded once here
 *     fistps 0x600(%esi,%ecx,2)
 *
 * and the modern build (`build/repro/pump/v90/V90AutoDigitalImpDetector.o` at
 * 0x1120..0x118c) is
 *
 *     fildll count
 *     fdivrs 0x9d18(%edx,%eax,4) ; sum / count, ONE divide
 *     fadds 0x2c                 ; + 0.5f
 *     fistps
 *
 * GCC 14's `-ffast-math` reassociates `sum * (1.0f / count)` into
 * `sum / count`, which rounds once where the object rounds twice, and the
 * result crosses the `(short)` truncation at count 41, sum 143.5:
 * 143.5 / 41 is exactly 3.5 and stores 4, while 143.5 * (1/41) is a hair under
 * 3.5 and stores 3.  It is `updateUrefAlt` ALONE: `updateLinMappMeanAndVar`
 * uses a named `float inv` in two expressions and GCC keeps the reciprocal
 * there, so its checks pass.  Finding F1366 found the witness; F11369 left
 * this site's next test as the disassembly above.
 *
 * A SOURCE HOIST WAS TRIED AND FAILED.  Writing `float inv = 1.0f / count;`
 * and multiplying in `updateUrefAlt` -- the spelling `updateLinMappMeanAndVar`
 * already uses -- makes no difference: with a SINGLE use GCC still folds it to
 * the `fdivrs`.  So the source is the object's and `make period` proves it,
 * while the modern build cannot reproduce the object from any spelling tried.
 *
 * WHY IT IS NOT IN `t_v90adid`, and this is the whole reason the file exists.
 * These 16 checks made the whole BINARY exit non-zero on the modern build, and
 * `tools/mutate.py` judges a mutant caught by a non-zero exit -- so it cannot
 * score a mutation set against a baseline that is already red, and it refuses.
 * `t_v90adid` carries the `v90adid` and `v90dil` suites, 497 mutations and the
 * largest suite in the tree.  Splitting the divergent group into its own
 * binary is what that refusal asks for; `t_v90p4dnan` is the worked precedent
 * (findings F2157, F3002, F6000-F6002).  The parent keeps every other group,
 * including `run_qcmapping`'s own reciprocal witness at `setQcLinearMapping`,
 * which passes on both tiers.  This split-off binary has no mutation suite.
 *
 * `make period` has no allow-list and passes this file with the object's own
 * compiler, which is the tier that decides.  Finding F11370.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/V90AutoDigitalImpDetector.h"

extern "C" {
void ref_updateLinMappMeanAndVar(void *self, int phase, int code)
	asm("ref__ZN25V90AutoDigitalImpDetector23updateLinMappMeanAndVarEss");
void ref_updateLinMappMeanAndVarAlt(void *self, int phase, int code)
	asm("ref__ZN25V90AutoDigitalImpDetector26updateLinMappMeanAndVarAltEss");
void ref_updateUrefAlt(void *self)
	asm("ref__ZN25V90AutoDigitalImpDetector13updateUrefAltEv");
}

/* The object, plus room past its end to catch a store that overruns it. */
#define OBJ_BYTES	0xa9b0
#define SLOT		(OBJ_BYTES + 128)

/*
 * `porcessSecondStudy` reads `linMapp[unSuspectedPhase][code - 2]` at codes 0
 * and 1, which for an unsuspected phase of 0 is four bytes IN FRONT of `this`
 * (D287).  Two static objects have two different sets of bytes in front of
 * them, so a guard is seeded in front on both sides and compared.
 */
#define PRE	16

struct adid_slot {
	unsigned char pre[PRE];
	unsigned char raw[SLOT];
} __attribute__((aligned(8)));

static struct adid_slot ours, theirs;

#define ours_o		(*(V90AutoDigitalImpDetector *)ours.raw)
#define theirs_o	(*(V90AutoDigitalImpDetector *)theirs.raw)

#define PARAMS_BYTES	0x504

static unsigned char params_block[PARAMS_BYTES];

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/*
 * `mode` picks how varied the fill is.  The seed is `t_v90adid`'s, so the
 * object bytes this file plants on are the same ones its parent would have.
 */
static void
seed(int trial, int mode)
{
	int i;

	lfsr_state = 0x1234u + 0x9e37u * (unsigned)trial + (unsigned)mode;

	for (i = 0; i < SLOT; i++) {
		unsigned char v;

		switch (mode) {
		case 1:
			v = 0xa5;
			break;
		case 2:
			v = 0x00;
			break;
		case 3:
			v = (unsigned char)((next_byte() & 0xfe) |
					    (unsigned)(i & 1));
			break;
		default:
			v = next_byte();
			break;
		}
		ours.raw[i] = v;
		theirs.raw[i] = v;
	}

	for (i = 0; i < PARAMS_BYTES; i++)
		params_block[i] = next_byte();

	for (i = 0; i < PRE; i++) {
		unsigned char v = (unsigned char)(0x5au ^ (unsigned)(i * 31)
						  ^ (unsigned)(trial * 13)
						  ^ (unsigned)(mode * 7));

		ours.pre[i] = theirs.pre[i] = v;
	}

	ours_o.params = (V90Parameters *)params_block;
	theirs_o.params = (V90Parameters *)params_block;
}

static int
guard_equal(void)
{
	return memcmp(ours.raw + OBJ_BYTES, theirs.raw + OBJ_BYTES,
		      SLOT - OBJ_BYTES) == 0
	    && memcmp(ours.pre, theirs.pre, PRE) == 0;
}

#define NPHASE	V90ADID_PHASES
#define NTRIAL	64

#define BOTH(field, value) \
	do { ours_o.field = theirs_o.field = (value); } while (0)

/*
 * The three that turn accumulators into a mapping.
 *
 * Each has an arm that does nothing -- a zero count for the two
 * `*MeanAndVar*` methods, an unflagged or empty phase for `updateUrefAlt` --
 * and a seeded count is nonzero with probability one, so the zero is forced
 * on every fourth trial and the run asserts that both arms were reached.
 * Finding F149: a method that always takes the same branch passes a whole
 * sweep of that branch perfectly.
 */
static int
run_means(void)
{
	int trial, moved = 0, distinct = 0;
	int zerocount = 0, nonzerocount = 0, altflag = 0, altnoflag = 0;
	short first = 0;

	diff_begin("V90AutoDigitalImpDetector::update*MeanAndVar*");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		short phase = (short)(trial % NPHASE);
		short code = (short)((trial * 13) & 0x7f);
		int p;

		seed(trial, trial % 4);

		if ((trial & 3) == 0) {
			BOTH(magnitudeCount[phase][code], 0u);
			BOTH(altMagnitudeCount[phase], 0u);
			zerocount = 1;
		} else if ((trial & 3) == 2) {
			/*
			 * THE ONE CASE THAT SEPARATES A DIVISION FROM A
			 * MULTIPLICATION BY A RECIPROCAL, and without it the
			 * two spellings agree over everything else this file
			 * offers -- measured, not assumed: the mutation that
			 * swaps them was NOT CAUGHT until this arm existed.
			 *
			 * 143.5 / 41 is exactly 3.5, so the object's `+ 0.5f`
			 * lands exactly on 4.0 and its truncating `fistp`
			 * stores 4.  1/41 is not representable and rounds the
			 * wrong way, so 143.5 * (1/41) is a hair under 3.5,
			 * the sum is a hair under 4.0, and the same
			 * truncation stores 3.  One code apart, from one bit.
			 *
			 * THE PAIR WAS FOUND, NOT GUESSED, and the search had
			 * to mirror the method's WHOLE body to find it: a
			 * probe with only the mean in it says n = 25 and
			 * sum = 12.5 disagree, and in the real method they do
			 * not, because the variance line either side changes
			 * which x87 register the mean lives in and therefore
			 * whether it is rounded.  n = 3 and n = 25 both agree
			 * here; 41 is the first that does not.  Finding F1366.
			 *
			 * `updateLinMappMeanAndVar` and `updateUrefAlt` take
			 * the reciprocal and `updateLinMappMeanAndVarAlt`
			 * divides, so this arm has to seed BOTH the per-code
			 * and the per-phase accumulators to pin all three.
			 */
			int q;

			BOTH(magnitudeCount[phase][code], 41u);
			BOTH(magnitudeSum[phase][code], 143.5f);
			BOTH(magnitudeSqSum[phase][code], 600.0f);
			for (q = 0; q < NPHASE; q++) {
				BOTH(altMagnitudeCount[q], 41u);
				BOTH(altMagnitudeSum[q], 143.5f);
			}
			nonzerocount = 1;
		} else {
			BOTH(magnitudeCount[phase][code],
			     (unsigned)(trial * 7 + 1));
			BOTH(altMagnitudeCount[phase], (unsigned)(trial + 1));
			nonzerocount = 1;
		}

		/*
		 * The float accumulators are left as the seed made them for
		 * three trials in four, and given tame values on the fourth,
		 * so that both a wild bit pattern and an ordinary mean are
		 * exercised through the same `fistp`.
		 */
		if ((trial & 3) == 1) {
			BOTH(magnitudeSum[phase][code], 1234.5f);
			BOTH(magnitudeSqSum[phase][code], 4000000.0f);
			BOTH(altMagnitudeSum[phase], -987.25f);
		}

		for (p = 0; p < NPHASE; p++) {
			short flag = (short)(((trial >> p) & 1) ? p + 1 : 0);

			BOTH(altRbsFlag[p], flag);
			if (flag != 0)
				altflag = 1;
			else
				altnoflag = 1;
		}

		memcpy(before, ours.raw, SLOT);

		ours_o.updateLinMappMeanAndVar(phase, code);
		ref_updateLinMappMeanAndVar(&theirs_o, phase, code);
		diff_eq_obj("after updateLinMappMeanAndVar",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);

		ours_o.updateLinMappMeanAndVarAlt(phase, code);
		ref_updateLinMappMeanAndVarAlt(&theirs_o, phase, code);
		diff_eq_obj("after updateLinMappMeanAndVarAlt",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o,
			    trial);

		ours_o.updateUrefAlt();
		ref_updateUrefAlt(&theirs_o);
		diff_eq_obj("after updateUrefAlt", V90AutoDigitalImpDetector,
			    &ours_o, &theirs_o, trial);

		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			first = ours_o.linMapp[phase][code];
		else if (ours_o.linMapp[phase][code] != first)
			distinct = 1;
	}

	diff_eq_int("the mean methods changed the object", moved, 1, 0);
	diff_eq_int("the mapping entry is not the same on every trial",
		    distinct, 1, 0);
	diff_eq_int("a zero count was exercised", zerocount, 1, 0);
	diff_eq_int("a nonzero count was exercised", nonzerocount, 1, 0);
	diff_eq_int("a flagged phase was exercised", altflag, 1, 0);
	diff_eq_int("an unflagged phase was exercised", altnoflag, 1, 0);

	return diff_end();
}

int
main(void)
{
	return run_means();
}
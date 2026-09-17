/*
 * t_v90adidnan.cpp -- ONE check, and it is in its own binary on purpose.
 *
 * `V90AutoDigitalImpDetector::determineMaxUcode` scans a five-entry window of
 * variances and counts the ones that are small.  An entry that is ZERO is
 * skipped, and the object's zero test is one ordered compare against a zero it
 * has kept on the x87 stack since 0x4443c:
 *
 *     444d1:  d9 04 98        flds   (%eax,%ebx,4)     ; the variance
 *     444d4:  d8 d1           fcom   %st(1)            ; against the kept 0.0
 *     444d6:  df e0           fnstsw %ax
 *     444d8:  9e              sahf
 *     444d9:  74 ..           je     <next entry>
 *
 * -- ONE `fcom` and no parity test.  FCOM sets C3 for an UNORDERED result
 * exactly as it does for an equal one, so `sahf` raises ZF either way and a
 * NaN entry is skipped along with a zero one.  Two small entries beside three
 * NaNs therefore give a count of 2, the window does NOT qualify, and the
 * answer falls through to the floor; a reading that counted the NaNs would
 * make it 5, qualify, and move the answer.  Finding F1436 for the window, 2300
 * for the site.
 *
 * WHY IT IS NOT IN `t_v90adid`, and this is the whole reason the file exists.
 * IEEE C's `==` is false for a NaN and GCC 13 emits the parity test whatever
 * it is told -- `-mno-ieee-fp` is accepted and does nothing, and
 * `-ffinite-math-only` withdraws NaN semantics from the whole translation unit
 * and breaks eleven sites that depend on them (finding F2304).  So the modern
 * build keeps the three entries the object drops and the answer moves.
 *
 * In `t_v90adid` that was 4 of `determineMaxUcode`'s 380 checks -- three from
 * this window row and one from a trial whose seeded variance array happened to
 * hold a NaN -- and it made the whole BINARY exit non-zero on the modern
 * build.  `tools/mutate.py` judges a mutant caught by a non-zero exit, so it
 * cannot score a mutation set against a baseline that is already red, and it
 * refuses.  `t_v90adid` carries the `v90adid` and `v90dil` suites, 497
 * mutations and the largest suite in the tree, and both were unscoreable for
 * this one arm.  Findings F2157 and F3002.
 *
 * Splitting the divergent check into its own binary is what that refusal asks
 * for; `t_v90p4dnan` is the worked precedent.  The accidental half went with
 * it: `mu_finite_variances` in `t_v90adid` now turns any seeded word whose
 * exponent field is all ones into the largest finite exponent, keeping its
 * sign and significand, because `linearMappingVar` holds VARIANCES and the object's
 * own writer cannot put a NaN there.  The deliberate half is this file.
 *
 * `make period` has no allow-list and passes this file with the object's own
 * compiler, which is the tier that decides.  Findings F6001, F2300 and F2304.
 *
 * THE FIXTURE IS THE WINDOW GRID AND NOTHING ELSE.  Everything outside the
 * planted window is 1.0e9f, so only the planted window can ever qualify and
 * the answer is a function of the five entries alone.  0x4f000000 is 2^31,
 * which is over the threshold the twenty entries at 40..59 produce and is not
 * a NaN.
 *
 * ANTI-VACUITY IS AN OBSERVABLE, NOT A PATH.  Rows 0 and 1 are the two
 * qualifying controls: three genuinely small entries in the same window, whose
 * answers are 0x5a and 0x59.  If the grid stopped reaching the scan at all,
 * every row would fall through to the floor together and the NaN rows would
 * "pass" for the wrong reason -- these two are what fails instead.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90AutoDigitalImpDetector.h"

extern "C" {
void ref_determineMaxUcode(void *self, int maxCode)
	asm("ref__ZN25V90AutoDigitalImpDetector17determineMaxUcodeEs");

extern unsigned int ref_dsplibs_debug_level;
}

/* `t_v90adid`'s slot, guard in front included: D287 reads in front of `this`. */
#define OBJ_BYTES	0xa9b0
#define SLOT		(OBJ_BYTES + 128)
#define PRE		16
#define NPHASE		V90ADID_PHASES

/* The last `linearMappingVar` entry whose four bytes are still inside the object. */
#define ADID_VAR_LAST	793

#define PARAMS_BYTES	0x504

struct adid_slot {
	unsigned char pre[PRE];
	unsigned char raw[SLOT];
} __attribute__((aligned(8)));

static struct adid_slot ours, theirs;

#define ours_o		(*(V90AutoDigitalImpDetector *)ours.raw)
#define theirs_o	(*(V90AutoDigitalImpDetector *)theirs.raw)

static unsigned char params_block[PARAMS_BYTES];

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/* `t_v90adid`'s seed, mode 0.  Never zero -- findings F223, F224, F230. */
static void
seed(int trial)
{
	int i;

	lfsr_state = 0x1234u + 0x9e37u * (unsigned)trial;

	for (i = 0; i < SLOT; i++) {
		unsigned char v = next_byte();

		ours.raw[i] = v;
		theirs.raw[i] = v;
	}
	for (i = 0; i < PARAMS_BYTES; i++)
		params_block[i] = next_byte();
	for (i = 0; i < PRE; i++) {
		unsigned char v = (unsigned char)(0x5au ^ (unsigned)(i * 31)
						  ^ (unsigned)(trial * 13));

		ours.pre[i] = theirs.pre[i] = v;
	}

	ours_o.params = (V90Parameters *)params_block;
	theirs_o.params = (V90Parameters *)params_block;
}

/*
 * THE SEED'S OWN NaNs ARE REMOVED HERE TOO, and for the reason `t_v90adid`
 * gives at `mu_finite_variances`: this file is about ONE window of PLANTED
 * NaNs, and a second, accidental one somewhere else in the array would make
 * the failing check set depend on the seed rather than on the plant.  Only
 * words whose exponent field is all ones are touched, and through the BITS
 * rather than `v != v` -- the period build's `-mno-ieee-fp` folds a
 * self-comparison to zero (finding F2303).
 */
static void
finite_variances(void)
{
	float *a = &ours_o.linearMappingVar[0][0];
	float *b = &theirs_o.linearMappingVar[0][0];
	int i;

	for (i = 0; i <= ADID_VAR_LAST; i++) {
		unsigned int u;

		memcpy(&u, &a[i], sizeof u);
		if ((u & 0x7f800000u) != 0x7f800000u)
			continue;
		u &= ~0x00800000u;
		memcpy(&a[i], &u, sizeof u);
		memcpy(&b[i], &u, sizeof u);
	}
}

#define BOTH(field, value) \
	do { ours_o.field = theirs_o.field = (value); } while (0)

/* One flag per phase from a bit pattern, the same on both sides. */
static void
adid_set_2800(int pattern)
{
	int p;

	for (p = 0; p < NPHASE; p++)
		ours_o.altRbsFlag[p] = theirs_o.altRbsFlag[p] =
		    (short)((pattern >> p) & 1);
}

static int
guard_equal(void)
{
	return memcmp(ours.raw + OBJ_BYTES, theirs.raw + OBJ_BYTES,
		      SLOT - OBJ_BYTES) == 0
	    && memcmp(ours.pre, theirs.pre, PRE) == 0;
}

/*
 * Row `w` is the five variances at codes 0x5a down to 0x56, top first.  Rows 0
 * and 1 qualify and are the controls; rows 2, 3 and 4 plant NaNs where a small
 * entry would otherwise be, in the three arrangements the count can see -- at
 * the top of the window, at its foot, and interleaved.
 */
static const unsigned int win[5][5] = {
	/* CONTROL: three small, the first at the top: qualifies at 0x5a */
	{ 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x7f7fffffu, 0x7f7fffffu },
	/* CONTROL: three small, the first one entry down: qualifies at 0x59 */
	{ 0x7f7fffffu, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x7f7fffffu },
	/* two small and three NaN, interleaved: must NOT qualify */
	{ 0x7fc00000u, 0x3f800000u, 0x7fc00000u, 0x3f800000u, 0x7fc00000u },
	/* two small and three NaN, the NaNs at the top */
	{ 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x3f800000u, 0x3f800000u },
	/*
	 * Two small and three NaN in three DIFFERENT encodings: a signalling
	 * NaN, a negative quiet NaN, and all-ones.  All three set C3 on
	 * `fcom` and all three are false under IEEE `==`, so the arm has to be
	 * taken for every one of them or the reading is right about quiet NaNs
	 * and wrong about the encoding.
	 */
	{ 0x7f800001u, 0x3f800000u, 0xffc00000u, 0x3f800000u, 0xffffffffu }
};
#define NWIN	((int)(sizeof(win) / sizeof(win[0])))

int
main(void)
{
	int w;
	unsigned char seen[NWIN];

	diff_begin("V90AutoDigitalImpDetector::determineMaxUcode: the "
		   "unordered entry in the window scan");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2u;

	for (w = 0; w < NWIN; w++) {
		int i;

		seed(760 + w);
		finite_variances();
		BOTH(unSuspectedPhase, 2);
		BOTH(minMaxUcode, 0x30);
		BOTH(float_a980, 1.0f);
		BOTH(ucode, 0x41);
		adid_set_2800(0x15);

		for (i = 0; i < V90ADID_CODES; i++)
			ours_o.linearMappingVar[2][i] =
			    theirs_o.linearMappingVar[2][i] = 1.0e9f;
		for (i = 0; i < 5; i++) {
			float v;

			memcpy(&v, &win[w][i], sizeof v);
			ours_o.linearMappingVar[2][0x5a - i] =
			    theirs_o.linearMappingVar[2][0x5a - i] = v;
		}

		dsplib_debug_capture_reset();
		ours_o.determineMaxUcode(0x5a);
		ref_determineMaxUcode(&theirs_o, 0x5a);

		diff_eq_obj("maxucode: the NaN window grid",
			    V90AutoDigitalImpDetector, &ours_o, &theirs_o, w);
		diff_eq_int("no store past the object (%ld)", guard_equal(), 1,
			    w);
		diff_eq_int("the NaN window grid's report matched (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, w);
		diff_eq_int("both sides printed the same number of lines (%ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), w);
		seen[w] = theirs_o.originalMaxUcode;
	}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0u;

	/*
	 * THE BLOB'S OWN ANSWERS, spelled without reference to our source.
	 * The two controls say the grid reached the scan; the three NaN rows
	 * say the unordered entry was SKIPPED, which is the whole claim.
	 */
	diff_eq_int("the blob qualified the control at its top", seen[0], 0x5a,
		    0);
	diff_eq_int("and the control one entry down", seen[1], 0x59, 0);
	diff_eq_int("interleaved NaNs do not make a window qualify",
		    seen[2] != 0x5a && seen[2] != 0x59, 1, 0);
	diff_eq_int("NaNs at the top do not either", seen[3] != 0x5a
		    && seen[3] != 0x59, 1, 0);
	diff_eq_int("nor do three different NaN encodings",
		    seen[4] != 0x5a && seen[4] != 0x59, 1, 0);

	return diff_end();
}

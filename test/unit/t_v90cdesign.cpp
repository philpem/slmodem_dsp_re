/*
 * t_v90cdesign.cpp -- differential test of V90ConstellationDesigner's eleven
 * small members, each against the blob's own copy.
 *
 * NOTHING IN THE OBJECT CALLS ANY OF THEM.  A sweep of every `R_386_PC32`
 * relocation in `.text` finds no caller for any of the eleven, so there is no
 * call site to drive them through and no call site to take argument types
 * from.  Each is therefore called directly by symbol on both sides -- ours by
 * its mangled name, the blob's by the `ref_` alias -- through an `asm()`
 * label, which is also how the ctor/dtor test reaches a member with no
 * spellable C++ form and how finding F225's double mangling is sidestepped.
 * The convention is plain cdecl with `this` as the first stack argument
 * (finding F215), and a `short` parameter occupies a whole slot, so the
 * declarations below widen those to `int` deliberately.
 *
 * WHAT A RANDOM FILL WOULD NOT REACH, and what is forced here instead:
 *
 *   TIES.  `findMinValueIndex` and `findConstelMaxValueIndex` are three-way
 *   -- greater, equal, less -- and the equal arm is where the two are
 *   asymmetric: both break a tie on the LARGER size, so the minimum function
 *   is min-value/max-size.  Random bytes essentially never make two
 *   `constellation[k][0]` equal, so the equal arm would go untested.  The
 *   sweep below makes every subset of the six rows equal in turn and, among
 *   the equal ones, varies the sizes both ways.
 *
 *   EVERY WINNER.  A sweep that only varies values at random can pass while
 *   two arms are transposed.  Each of the six rows is made the sole winner in
 *   turn, and each of the six is made the tie-break winner in turn.
 *
 *   THE ZERO PRODUCT.  `realK` and `maxK` answer a zero product with a
 *   constant, and a product of six random 32-bit sizes is never zero, so a
 *   zero size is planted in each of the six positions in turn.
 *
 *   BOTH COMPANDING ARMS and BOTH WALKS of `findNextUcodeToAdd`: the arm is
 *   chosen by `dmin[which] != 0` and the codec by the object's +0x2c, so all
 *   four combinations are driven, at starts that straddle both of the two
 *   different bounds (0x71 unsigned, 0x7f signed).
 *
 * WHAT IS DELIBERATELY NOT DRIVEN, because both sides would hang identically
 * and a hang is not a diagnostic (docs/deviations.md D-entries):
 *
 *   `calcMtoMatchKtarget` with kTarget below log2(m).  The truncation goes
 *   negative and the doubling loop then runs about 2^32 times.
 *
 *   `reconstructInitialConditions` with a ucode value absent from its row.
 *   The search is a bare `jne` with no bound and walks off the row.
 *
 * THE TWO MUTATING MEMBERS ARE COMPARED AS WHOLE OBJECTS with `diff_eq_obj`
 * over the 0x650-byte `V90MappingParams`, not by checking the fields this
 * session predicted: `spectralDesign` and `reconstructInitialConditions`
 * write six dwords and two byte tables respectively, and a field-by-field
 * check would pass over anything else either of them touched.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/debug.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90ConstellationDesigner.h"

extern "C" {

float our_pow6(void *, int) asm("_ZN24V90ConstellationDesigner4pow6Es");
float ref_pow6(void *, int) asm("ref__ZN24V90ConstellationDesigner4pow6Es");

float our_calcK(void *, unsigned, float *)
	asm("_ZN24V90ConstellationDesigner5calcKEjPf");
float ref_calcK(void *, unsigned, float *)
	asm("ref__ZN24V90ConstellationDesigner5calcKEjPf");

float our_realK(void *, void *)
	asm("_ZN24V90ConstellationDesigner5realKEP16V90MappingParams");
float ref_realK(void *, void *)
	asm("ref__ZN24V90ConstellationDesigner5realKEP16V90MappingParams");

int our_maxK(void *, void *)
	asm("_ZN24V90ConstellationDesigner4maxKEP16V90MappingParams");
int ref_maxK(void *, void *)
	asm("ref__ZN24V90ConstellationDesigner4maxKEP16V90MappingParams");

int our_calcM(void *, float, float)
	asm("_ZN24V90ConstellationDesigner19calcMtoMatchKtargetEff");
int ref_calcM(void *, float, float)
	asm("ref__ZN24V90ConstellationDesigner19calcMtoMatchKtargetEff");

int our_findMin(void *, void *)
	asm("_ZN24V90ConstellationDesigner17findMinValueIndexEP16V90MappingParams");
int ref_findMin(void *, void *)
	asm("ref__ZN24V90ConstellationDesigner17findMinValueIndexEP16V90MappingParams");

int our_findMax(void *, void *)
	asm("_ZN24V90ConstellationDesigner24findConstelMaxValueIndexEP16V90MappingParams");
int ref_findMax(void *, void *)
	asm("ref__ZN24V90ConstellationDesigner24findConstelMaxValueIndexEP16V90MappingParams");

int our_constelBuild(void *, int, int)
	asm("_ZN24V90ConstellationDesigner12constelBuildEss");
int ref_constelBuild(void *, int, int)
	asm("ref__ZN24V90ConstellationDesigner12constelBuildEss");

void our_spectral(void *, unsigned, int)
	asm("_ZN24V90ConstellationDesigner14spectralDesignEj28V90SpecialSpectralConditions");
void ref_spectral(void *, unsigned, int)
	asm("ref__ZN24V90ConstellationDesigner14spectralDesignEj28V90SpecialSpectralConditions");

void our_reconstruct(void *, void *, unsigned char *)
	asm("_ZN24V90ConstellationDesigner28reconstructInitialConditionsEP16V90MappingParamsPh");
void ref_reconstruct(void *, void *, unsigned char *)
	asm("ref__ZN24V90ConstellationDesigner28reconstructInitialConditionsEP16V90MappingParamsPh");

int our_findNext(void *, unsigned char *, int, void *, void *, short *, void *)
	asm("_ZN24V90ConstellationDesigner18findNextUcodeToAddEPhhPA128_sS2_PsPA128_h");
int ref_findNext(void *, unsigned char *, int, void *, void *, short *, void *)
	asm("ref__ZN24V90ConstellationDesigner18findNextUcodeToAddEPhhPA128_sS2_PsPA128_h");

void our_dmin(void *, unsigned)
	asm("_ZN24V90ConstellationDesigner19determineDminForRrnEj");
void ref_dmin(void *, unsigned)
	asm("ref__ZN24V90ConstellationDesigner19determineDminForRrnEj");

extern unsigned int ref_dsplibs_debug_level;

}

/*
 * A cheap varied fill.  The same generator the ctor/dtor test uses, so a
 * value that happens to be a constant the code stores is not mistaken for a
 * store.
 */
static unsigned lfsr;

static void
reseed(unsigned s)
{
	lfsr = s | 1u;
}

static unsigned
nextrand(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	lfsr = lfsr * 1103515245u + 12345u;
	return lfsr;
}

/*
 * The two mapping-parameter blocks, one per side, and the designer object
 * that points at nothing until each case wires it up.  They are file scope
 * because 0x650 bytes twice is more than a stack frame wants and because the
 * period compiler's frame limits are not worth discovering here.
 */
static V90MappingParams mpA;
static V90MappingParams mpB;

/*
 * `V90Parameters` and `V90ConstellationDesigner` both declare a constructor
 * and neither declares a default one, so neither can be a plain static
 * object.  They get raw storage and a pointer, which is also closer to what
 * the test wants: every field is set explicitly rather than by a constructor
 * whose stores would be part of what is under test elsewhere.
 */
static unsigned char parAbuf[sizeof(V90Parameters)] __attribute__((aligned(8)));
static unsigned char parBbuf[sizeof(V90Parameters)] __attribute__((aligned(8)));
static unsigned char cdAbuf[sizeof(V90ConstellationDesigner)]
	__attribute__((aligned(8)));
static unsigned char cdBbuf[sizeof(V90ConstellationDesigner)]
	__attribute__((aligned(8)));
static V90Parameters *parA;
static V90Parameters *parB;
static V90ConstellationDesigner *cdA;
static V90ConstellationDesigner *cdB;

/*
 * constelBuild's table: 0xd00 of shorts and then the byte table past it.
 *
 * IT REACHES FURTHER THAN `constelBuild` DOES, and 12 KB is not slack.
 * `determineDminForRrn` tests six bytes at +0x280c off the same pointer, so a
 * table sized for `constelBuild` alone would be read past its end -- and the
 * failure would not be a difference: six non-zero bytes there leave the search
 * with no candidate, `maxSize` zero, `1.0f / maxSize` an infinity and the
 * doubling loop running 2^31 times on BOTH sides identically.  A hang is not a
 * diagnostic, so the size is chosen for the furthest reader.
 */
#define TBLBYTES	12288
static unsigned char tblA[TBLBYTES] __attribute__((aligned(8)));
static unsigned char tblB[TBLBYTES] __attribute__((aligned(8)));

static void
wire(void)
{
	parA = (V90Parameters *)parAbuf;
	parB = (V90Parameters *)parBbuf;
	cdA = (V90ConstellationDesigner *)cdAbuf;
	cdB = (V90ConstellationDesigner *)cdBbuf;
	memset((void *)cdA, 0, sizeof(V90ConstellationDesigner));
	memset((void *)cdB, 0, sizeof(V90ConstellationDesigner));
	cdA->params = parA;
	cdB->params = parB;
	cdA->mappingParams = &mpA;
	cdB->mappingParams = &mpB;
	cdA->constelTable = (short (*)[128])tblA;
	cdB->constelTable = (short (*)[128])tblB;
}

/*
 * ALL ELEVEN ARE READ-ONLY ON `this`, and that is a negative claim, so it is
 * asserted rather than assumed.  The two sides cannot be compared against
 * each other -- they hold different collaborator addresses on purpose -- so
 * each is compared against its OWN state before the call.
 */
static unsigned char snapA[sizeof(V90ConstellationDesigner)];
static unsigned char snapB[sizeof(V90ConstellationDesigner)];

static void
snap_this(void)
{
	memcpy(snapA, cdAbuf, sizeof(snapA));
	memcpy(snapB, cdBbuf, sizeof(snapB));
}

static void
check_this(long input)
{
	diff_eq_obj("ours leaves `this` alone", V90ConstellationDesigner,
		    cdAbuf, snapA, input);
	diff_eq_obj("the blob leaves `this` alone", V90ConstellationDesigner,
		    cdBbuf, snapB, input);
}

/* Fill both mapping blocks identically with varied bytes. */
static void
fill_mp(unsigned s)
{
	unsigned char *a = (unsigned char *)&mpA;
	unsigned char *b = (unsigned char *)&mpB;
	unsigned i;

	reseed(s);
	for (i = 0; i < sizeof(mpA); i++) {
		unsigned char v = (unsigned char)(nextrand() >> 13);

		a[i] = v;
		b[i] = v;
	}
}

/*
 * ===========================================================================
 * pow6, calcK, realK, maxK, calcMtoMatchKtarget -- the arithmetic five
 * ===========================================================================
 */
static int
run_arith(void)
{
	static const short pw[] = {
		0, 1, -1, 2, -2, 3, 7, -7, 10, -10, 25, 100, -100,
		181, -181, 255, 256, 1000, -1000, 4096, 12345, -12345,
		32767, -32768
	};
	int i;
	int j;
	int k;
	int seenZeroK = 0;
	int seenNonZeroK = 0;
	int seenExact = 0;
	int seenShort = 0;

	diff_begin("V90ConstellationDesigner arithmetic leaves");
	wire();
	snap_this();

	/* pow6: x^6 as a float, over the whole short range's interesting end. */
	for (i = 0; i < (int)(sizeof(pw) / sizeof(pw[0])); i++) {
		float g = our_pow6(cdA, pw[i]);
		float r = ref_pow6(cdB, pw[i]);

		diff_eq_float("pow6(%ld)", g, r, pw[i]);
	}

	/*
	 * calcK: the product of six scaled sizes, then log2 of it.  The
	 * multiplier is unsigned and the object converts it 64-bit-wise, so
	 * values above 2^31 are part of the sweep and not an afterthought.
	 */
	{
		static const unsigned mm[] = {
			1u, 2u, 3u, 17u, 128u, 4096u, 65535u, 1000000u,
			0x7fffffffu, 0x80000000u, 0xfffffffeu
		};
		float f[6];

		for (i = 0; i < (int)(sizeof(mm) / sizeof(mm[0])); i++) {
			for (j = 0; j < 8; j++) {
				float g;
				float r;

				reseed(0x51ed + 37u * (unsigned)(i * 8 + j));
				for (k = 0; k < 6; k++)
					f[k] = (float)(nextrand() % 4096u)
					     / 512.0f + 0.03125f;

				g = our_calcK(cdA, mm[i], f);
				r = ref_calcK(cdB, mm[i], f);
				diff_eq_float("calcK(%ld, ...)", g, r,
					      (long)mm[i]);
			}
		}
	}

	/*
	 * realK and maxK share a product and differ in their answer's type and
	 * in the fudge added before the truncation.  Every position is given a
	 * zero in turn, because a random product is never zero and the zero
	 * arm is a different return.
	 */
	for (i = 0; i < 64; i++) {
		float gf;
		float rf;
		int gi;
		int ri;

		fill_mp(0x9e37u + 101u * (unsigned)i);
		reseed(0x1234u + 7u * (unsigned)i);
		for (k = 0; k < 6; k++) {
			unsigned v = nextrand() % 200u + 1u;

			mpA.constellationSize[k] = v;
			mpB.constellationSize[k] = v;
		}
		if (i < 6) {
			mpA.constellationSize[i] = 0;
			mpB.constellationSize[i] = 0;
		}
		if (i >= 6 && i < 12) {
			/* A huge size, so the product leaves float range. */
			mpA.constellationSize[i - 6] = 0xfff00000u;
			mpB.constellationSize[i - 6] = 0xfff00000u;
		}

		gf = our_realK(cdA, &mpA);
		rf = ref_realK(cdB, &mpB);
		diff_eq_float("realK trial %ld", gf, rf, i);

		gi = our_maxK(cdA, &mpA);
		ri = ref_maxK(cdB, &mpB);
		diff_eq_int("maxK trial %ld", gi, ri, i);
		if (gi == 0)
			seenZeroK++;
		else
			seenNonZeroK++;

		diff_eq_obj("realK/maxK read only", V90MappingParams,
			    &mpA, &mpB, i);
	}

	/*
	 * calcMtoMatchKtarget: 2^((kTarget - log2 m)/6).  kTarget is kept
	 * above log2(m) on purpose -- see the file header -- and the sweep
	 * straddles the integer boundaries of the doubling loop, which is
	 * where the split into 2^n and (2^0.01)^frac can disagree.
	 */
	for (i = 0; i < 40; i++) {
		static const float ms[] = { 1.0f, 2.0f, 6.0f, 64.0f, 128.0f };
		float m = ms[i % 5];
		float target = (float)(i / 5) * 6.0f + (float)(i % 5) * 0.37f
			     + 1.0f;
		int g;
		int r;

		g = our_calcM(cdA, target, m);
		r = ref_calcM(cdB, target, m);
		diff_eq_int("calcMtoMatchKtarget trial %ld", g, r, i);
	}

	/*
	 * THE TWO FUDGES, MADE VISIBLE.  `realK` adds 1e-9f and `maxK` adds
	 * 1e-6f before truncating, and neither can change an answer over the
	 * sweep above: at K near 40 an addition of 1e-9 rounds away entirely,
	 * and 1e-6 only matters when the quotient lands just under an integer.
	 *
	 * All six sizes 1 makes the product 1, log2 of it 0, and `realK`'s
	 * 1e-9f the WHOLE return value.  Products that are exact powers of two
	 * are where `maxK`'s quotient lands on an integer boundary, which is
	 * the only place its 1e-6f can decide anything, so 2^1 through 2^48 are
	 * swept with the exponent spread over the six sizes.
	 */
	fill_mp(0x7777u);
	for (k = 0; k < 6; k++) {
		mpA.constellationSize[k] = 1u;
		mpB.constellationSize[k] = 1u;
	}
	diff_eq_float("realK of a unit product", our_realK(cdA, &mpA),
		      ref_realK(cdB, &mpB), 1);
	diff_eq_int("maxK of a unit product", our_maxK(cdA, &mpA),
		    ref_maxK(cdB, &mpB), 1);

	for (i = 1; i <= 48; i++) {
		int left = i;
		int gi;

		fill_mp(0x2222u + (unsigned)i);
		for (k = 0; k < 6; k++) {
			int part = left / (6 - k);

			left -= part;
			mpA.constellationSize[k] = 1u << part;
			mpB.constellationSize[k] = mpA.constellationSize[k];
		}
		diff_eq_float("realK of 2^%ld", our_realK(cdA, &mpA),
			      ref_realK(cdB, &mpB), i);
		gi = our_maxK(cdA, &mpA);
		diff_eq_int("maxK of 2^%ld", gi, ref_maxK(cdB, &mpB), i);
		/*
		 * AND THE ANSWER ITSELF, because two sides agreeing on a wrong
		 * exponent would still pass.  A product of exactly 2^i has
		 * log2 exactly i, and the object returns i up to 21 and i-1
		 * from 22 on: the 1e-6f is an ABSOLUTE correction applied to a
		 * quotient whose error grows with the quotient, because the
		 * divisor is log10(2) rounded to a float first.  D329.  What is
		 * asserted here is that reading, not an intent -- and that both
		 * regimes were reached, so the boundary is under test.
		 */
		diff_eq_int("maxK of 2^%ld is the exponent or one below",
			    gi == i || gi == i - 1, 1, i);
		if (gi == i)
			seenExact++;
		else
			seenShort++;
		seenNonZeroK++;
	}

	diff_eq_int("maxK returned the exponent %ld times", seenExact > 0, 1,
		    seenExact);
	diff_eq_int("maxK returned one below it %ld times", seenShort > 0, 1,
		    seenShort);

	/*
	 * Both arms of the zero test were reached, or the sweep proves only
	 * that two sides agree about one of them (findings F149, F223).
	 */
	diff_eq_int("the zero-product arm was reached %ld times",
		    seenZeroK > 0, 1, seenZeroK);
	diff_eq_int("the logarithm arm was reached %ld times",
		    seenNonZeroK > 0, 1, seenNonZeroK);
	check_this(0);

	return diff_end();
}

/*
 * ===========================================================================
 * findMinValueIndex and findConstelMaxValueIndex
 * ===========================================================================
 *
 * The sweep is exhaustive over a small alphabet rather than random: six rows
 * whose first bytes take every combination of three values, with the sizes
 * taking every combination of two.  3^6 * 2^6 is 46,656 shapes and it covers
 * every tie pattern, every winner and every tie-break winner.
 */
static int
run_findindex(void)
{
	int shape;
	int i;
	int seenMin[6];
	int seenMax[6];

	diff_begin("V90ConstellationDesigner::find{Min,ConstelMax}ValueIndex");
	wire();
	snap_this();
	fill_mp(0x0d0eu);
	for (i = 0; i < 6; i++) {
		seenMin[i] = 0;
		seenMax[i] = 0;
	}

	for (shape = 0; shape < 46656; shape++) {
		int v = shape % 729;
		int s = shape / 729;
		int g;
		int r;

		for (i = 0; i < 6; i++) {
			static const unsigned char vals[3] = { 7, 8, 9 };
			static const unsigned int lens[2] = { 100u, 200u };

			mpA.constellation[i][0] = vals[(v / 1) % 3];
			mpB.constellation[i][0] = mpA.constellation[i][0];
			mpA.constellationSize[i] = lens[s & 1];
			mpB.constellationSize[i] = mpA.constellationSize[i];
			v /= 3;
			s >>= 1;
		}

		g = our_findMin(cdA, &mpA);
		r = ref_findMin(cdB, &mpB);
		diff_eq_int("findMinValueIndex shape %ld", g, r, shape);
		if (g >= 0 && g < 6)
			seenMin[g]++;

		g = our_findMax(cdA, &mpA);
		r = ref_findMax(cdB, &mpB);
		diff_eq_int("findConstelMaxValueIndex shape %ld", g, r, shape);
		if (g >= 0 && g < 6)
			seenMax[g]++;
	}

	/*
	 * And the wide values, because the alphabet above is all below 0x80
	 * and the byte is loaded `movzbl`: a signed reading would agree over
	 * every value used so far and disagree here.
	 */
	for (shape = 0; shape < 512; shape++) {
		int g;
		int r;

		reseed(0x77u + 13u * (unsigned)shape);
		for (i = 0; i < 6; i++) {
			unsigned char v = (unsigned char)(nextrand() >> 11);
			unsigned int n = nextrand();

			mpA.constellation[i][0] = v;
			mpB.constellation[i][0] = v;
			mpA.constellationSize[i] = n;
			mpB.constellationSize[i] = n;
		}

		g = our_findMin(cdA, &mpA);
		r = ref_findMin(cdB, &mpB);
		diff_eq_int("findMinValueIndex wide %ld", g, r, shape);

		g = our_findMax(cdA, &mpA);
		r = ref_findMax(cdB, &mpB);
		diff_eq_int("findConstelMaxValueIndex wide %ld", g, r, shape);
	}

	/*
	 * EVERY ROW WON AT LEAST ONCE, in each direction.  Without this the
	 * sweep would prove only that two sides agree, and two transposed arms
	 * agree with each other perfectly well (findings F149, F223).
	 */
	for (i = 0; i < 6; i++) {
		diff_eq_int("row %ld won the minimum", seenMin[i] > 0, 1, i);
		diff_eq_int("row %ld won the maximum", seenMax[i] > 0, 1, i);
	}
	check_this(0);

	return diff_end();
}

/*
 * ===========================================================================
 * spectralDesign
 * ===========================================================================
 *
 * ONE INPUT IS EXCLUDED AND IT IS EXCLUDED PRECISELY: a SIGNALLING NaN in one
 * of the eight shaper floats.  This is not a tolerance; it is the one input on
 * which the two BUILDS disagree while the source is right, and the exclusion
 * is the exact encoding rather than a range.
 *
 * The object copies each shaper float with a 32-bit integer `mov`, which is
 * what GCC 3.4.2 emits for a float assignment.  GCC 13 emits `flds`/`fstps`
 * instead -- seen in `build/src/pump/v90/V90ConstellationDesigner.o` -- and
 * loading a signalling NaN into an x87 register and storing it back sets the
 * quiet bit, so the byte at +0x636 comes out 0xef where the blob leaves 0xaf.
 * `make period`, the tier that decides, has no such difference.
 *
 * Writing the copy as a `memcpy` would make both builds agree and would be
 * papering over a compiler divergence in `src/`, which is exactly what
 * CLAUDE.md says not to do.  So the source keeps the float assignment the
 * author wrote and the test declines to feed it a value the parameters cannot
 * hold: every one of these comes from `Vparser_read_float` over a
 * configuration file.  docs/deviations.md carries the entry.
 */
static void
quieten_snan(float *f)
{
	unsigned int bits;

	memcpy(&bits, f, sizeof(bits));
	if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0)
		bits |= 0x00400000u;
	memcpy(f, &bits, sizeof(bits));
}

static int
run_spectral(void)
{
	static const unsigned rates[] = {
		0u, 1u, 27999u, 28000u, 28001u, 31999u, 32000u, 32001u,
		56000u, 0x7fffffffu, 0x80000000u, 0xffffffffu
	};
	int trial;
	unsigned i;

	diff_begin("V90ConstellationDesigner::spectralDesign");
	wire();
	snap_this();

	for (trial = 0; trial < 48; trial++) {
		unsigned char *pa = (unsigned char *)parA;
		unsigned char *pb = (unsigned char *)parB;
		int cond = trial % 4;

		reseed(0xabc0u + 61u * (unsigned)trial);
		for (i = 0; i < sizeof(V90Parameters); i++) {
			unsigned char v = (unsigned char)(nextrand() >> 9);

			pa[i] = v;
			pb[i] = v;
		}
		/*
		 * The identifier is what the rate limits, so it is planted
		 * one below, at, and one above each rate rather than left to
		 * the fill.
		 */
		/* See quieten_snan above for why these eight are touched. */
		quieten_snan(&parA->SPECTRAL_SHAPER_A1);
		quieten_snan(&parA->SPECTRAL_SHAPER_A2);
		quieten_snan(&parA->SPECTRAL_SHAPER_B1);
		quieten_snan(&parA->SPECTRAL_SHAPER_B2);
		quieten_snan(&parA->GERMAN_PBX_SPECTRAL_SHAPER_A1);
		quieten_snan(&parA->GERMAN_PBX_SPECTRAL_SHAPER_A2);
		quieten_snan(&parA->GERMAN_PBX_SPECTRAL_SHAPER_B1);
		quieten_snan(&parA->GERMAN_PBX_SPECTRAL_SHAPER_B2);
		memcpy((void *)parB, (const void *)parA, sizeof(V90Parameters));

		parA->SPECTRAL_SHAPER_ID = (int)rates[trial % 12]
					+ (trial % 3) - 1;
		parB->SPECTRAL_SHAPER_ID = parA->SPECTRAL_SHAPER_ID;
		parA->GERMAN_PBX_SPECTRAL_SHAPER_ID = parA->SPECTRAL_SHAPER_ID;
		parB->GERMAN_PBX_SPECTRAL_SHAPER_ID = parA->SPECTRAL_SHAPER_ID;

		fill_mp(0x5150u + 29u * (unsigned)trial);

		our_spectral(cdA, rates[trial % 12], cond);
		ref_spectral(cdB, rates[trial % 12], cond);

		diff_eq_obj("spectralDesign", V90MappingParams,
			    &mpA, &mpB, trial);
		diff_eq_obj("spectralDesign leaves the parameters alone",
			    V90Parameters, parA, parB, trial);
	}

	/*
	 * And the far side of the enumerator, because the object tests
	 * `== 2` and every other value takes one arm: the sweep sets the
	 * condition one below and one above the only value that branches.
	 */
	for (trial = 0; trial < 8; trial++) {
		static const int conds[8] = { -1, 0, 1, 2, 3, 4, 100, -2 };

		fill_mp(0x6161u + 17u * (unsigned)trial);
		our_spectral(cdA, 33600u, conds[trial]);
		ref_spectral(cdB, 33600u, conds[trial]);
		diff_eq_obj("spectralDesign condition sweep", V90MappingParams,
			    &mpA, &mpB, conds[trial]);
	}

	/*
	 * The limit fired and did not fire.  `shaperId` is the only computed
	 * field, so a sweep in which the rate never bit would be testing five
	 * copies and a constant.
	 */
	{
		int clamped = 0;
		int unclamped = 0;

		fill_mp(0x1010u);
		parA->SPECTRAL_SHAPER_ID = 40000;
		parB->SPECTRAL_SHAPER_ID = 40000;
		our_spectral(cdA, 30000u, 0);
		ref_spectral(cdB, 30000u, 0);
		diff_eq_obj("spectralDesign clamped", V90MappingParams,
			    &mpA, &mpB, 30000);
		if (mpA.shaperId == 30000u)
			clamped++;
		our_spectral(cdA, 50000u, 0);
		ref_spectral(cdB, 50000u, 0);
		diff_eq_obj("spectralDesign unclamped", V90MappingParams,
			    &mpA, &mpB, 50000);
		if (mpA.shaperId == 40000u)
			unclamped++;
		diff_eq_int("the rate limit fired", clamped, 1, 0);
		diff_eq_int("the rate limit did not fire", unclamped, 1, 0);
	}
	check_this(0);

	return diff_end();
}

/*
 * ===========================================================================
 * reconstructInitialConditions
 * ===========================================================================
 *
 * The ucode value is always PRESENT in its row, because the search has no
 * bound and an absent value walks off the row on both sides alike.  Which
 * position it sits at is the sweep: 0 (nothing to drop), the last entry
 * inside the length, and one past the length -- which is legal for the
 * search and makes the shift run more times than the row is long.
 */
static int
run_reconstruct(void)
{
	int trial;
	int k;
	int i;
	int seenDropped = 0;
	int seenUntouched = 0;

	diff_begin("V90ConstellationDesigner::reconstructInitialConditions");
	wire();
	snap_this();

	for (trial = 0; trial < 64; trial++) {
		unsigned char ucodeA[6];
		unsigned char ucodeB[6];

		fill_mp(0x3141u + 53u * (unsigned)trial);
		reseed(0x2718u + 11u * (unsigned)trial);

		for (k = 0; k < 6; k++) {
			unsigned int n = nextrand() % 24u + 1u;
			int pick;

			mpA.constellationSize[k] = n;
			mpB.constellationSize[k] = n;
			/*
			 * Distinct entries, so the first match is at a known
			 * index and a wrong search would land elsewhere
			 * rather than on an equal neighbour.
			 */
			for (i = 0; i < 64; i++) {
				unsigned char v =
				    (unsigned char)(k * 64 + i + 1);

				mpA.constellation[k][i] = v;
				mpB.constellation[k][i] = v;
				mpA.codecConstellation[k][i] =
				    (unsigned char)(255 - v);
				mpB.codecConstellation[k][i] =
				    mpA.codecConstellation[k][i];
			}
			pick = (int)(nextrand() % (n + 2u));
			if (trial % 8 == 0)
				pick = 0;
			ucodeA[k] = mpA.constellation[k][pick];
			ucodeB[k] = ucodeA[k];
		}

		{
			unsigned int before[6];

			for (k = 0; k < 6; k++)
				before[k] = mpA.constellationSize[k];

			our_reconstruct(cdA, &mpA, ucodeA);
			ref_reconstruct(cdB, &mpB, ucodeB);

			for (k = 0; k < 6; k++) {
				if (mpA.constellationSize[k] != before[k])
					seenDropped++;
				else
					seenUntouched++;
			}
		}

		diff_eq_obj("reconstructInitialConditions", V90MappingParams,
			    &mpA, &mpB, trial);
		diff_eq_obj("the ucode is not written", unsigned char[6],
			    ucodeA, ucodeB, trial);
	}

	/*
	 * Both outcomes.  A sweep in which nothing was ever dropped would be
	 * testing an empty loop on both sides and agreeing about it.
	 */
	diff_eq_int("a row was shortened %ld times", seenDropped > 0, 1,
		    seenDropped);
	diff_eq_int("a row was left alone %ld times", seenUntouched > 0, 1,
		    seenUntouched);
	check_this(0);

	return diff_end();
}

/*
 * ===========================================================================
 * constelBuild
 * ===========================================================================
 */
static int
run_constelbuild(void)
{
	int trial;
	int which;
	unsigned i;
	int seenZero = 0;
	int seenCounted = 0;

	diff_begin("V90ConstellationDesigner::constelBuild");
	wire();
	snap_this();

	for (trial = 0; trial < 96; trial++) {
		reseed(0x4d2u + 97u * (unsigned)trial);
		for (i = 0; i < TBLBYTES; i++) {
			unsigned char v = (unsigned char)(nextrand() >> 15);

			/*
			 * The byte table is a flag the object only tests
			 * against zero, so a quarter of it is made zero
			 * rather than one in 256.
			 */
			if (i >= 0xd00 && (v & 3) == 0)
				v = 0;
			tblA[i] = v;
			tblB[i] = v;
		}
		fill_mp(0xbeefu + 41u * (unsigned)trial);

		parA->unnamed_360 = (int)(nextrand() % 8u);
		parB->unnamed_360 = parA->unnamed_360;

		for (which = 0; which < 6; which++) {
			static const short steps[8] = {
				0, 1, -1, 2, 16, -16, 1024, -1024
			};
			unsigned char len =
			    (unsigned char)(nextrand() % 200u);
			int g;
			int r;
			int s;

			mpA.constellation[which][0] = len;
			mpB.constellation[which][0] = len;

			for (s = 0; s < 8; s++) {
				g = our_constelBuild(cdA, steps[s], which);
				r = ref_constelBuild(cdB, steps[s], which);
				diff_eq_int("constelBuild(step %ld)", g, r,
					    steps[s]);
				if (g == 0)
					seenZero++;
				else
					seenCounted++;
			}
		}

		diff_eq_obj("constelBuild leaves the table alone",
			    unsigned char[TBLBYTES], tblA, tblB, trial);
		diff_eq_obj("constelBuild leaves the mapping alone",
			    V90MappingParams, &mpA, &mpB, trial);
	}

	diff_eq_int("constelBuild counted something %ld times",
		    seenCounted > 0, 1, seenCounted);
	diff_eq_int("constelBuild counted nothing %ld times",
		    seenZero > 0, 1, seenZero);
	check_this(0);

	return diff_end();
}

/*
 * ===========================================================================
 * findNextUcodeToAdd
 * ===========================================================================
 *
 * The rows are seven deep and not six: the second walk's bound is
 * `(signed char)i >= 0`, so the index reaches 0x80 and the object then reads
 * the row after the one it was given.  Six rows would be a read past the end
 * of the array on both sides and the test would be measuring the allocator.
 */
static int
run_findnext(void)
{
	static short ucodeA[7][128];
	static short ucodeB[7][128];
	static short altA[7][128];
	static short altB[7][128];
	static unsigned char extraA[7][128];
	static unsigned char extraB[7][128];
	int trial;
	int which;
	int i;
	int j;
	int seenFound = 0;
	int seenRanOff = 0;
	int seenAlaw = 0;
	int seenUlaw = 0;
	int seenWalked = 0;

	diff_begin("V90ConstellationDesigner::findNextUcodeToAdd");
	wire();

	for (trial = 0; trial < 96; trial++) {
		short dminA[6];
		short dminB[6];

		reseed(0xfeedu + 71u * (unsigned)trial);
		for (i = 0; i < 7; i++) {
			for (j = 0; j < 128; j++) {
				/*
				 * A rising row with noise on it, so that the
				 * walk stops somewhere in the middle rather
				 * than at the first or the last entry.
				 */
				short u = (short)(j * 61
					  + (int)(nextrand() % 64u) - 32);
				short a = (short)(u + (int)(nextrand() % 512u)
					  - 256);

				ucodeA[i][j] = u;
				ucodeB[i][j] = u;
				altA[i][j] = a;
				altB[i][j] = a;
				extraA[i][j] = (unsigned char)nextrand();
				extraB[i][j] = extraA[i][j];
			}
		}
		fill_mp(0xc0deu + 23u * (unsigned)trial);

		cdA->dMin = (short)(nextrand() % 4096u) - 2048;
		cdB->dMin = cdA->dMin;
		cdA->short_10 = (short)(nextrand() % 4096u) - 2048;
		cdB->short_10 = cdA->short_10;
		/* Both companding arms, alternating with the trial. */
		cdA->compandingLaw = (trial & 1);
		cdB->compandingLaw = cdA->compandingLaw;
		/*
		 * After the fields this trial sets, not before: the claim is
		 * that the MEMBER leaves `this` alone, not that the test does.
		 */
		snap_this();

		for (which = 0; which < 6; which++) {
			unsigned char outA[4];
			unsigned char outB[4];
			int g;
			int r;
			int startcase;

			/* Both walks: zero dmin and non-zero dmin. */
			for (i = 0; i < 6; i++) {
				dminA[i] = (short)((trial >> 1) & 1);
				dminB[i] = dminA[i];
			}

			/*
			 * The start is the row's first byte, and the two
			 * walks have two different bounds -- 0x71 read
			 * unsigned and 0x7f read signed -- so the sweep sets
			 * it one below, at, and one above each.
			 */
			for (startcase = 0; startcase < 10; startcase++) {
				static const unsigned char starts[10] = {
					0, 1, 0x70, 0x71, 0x72, 0x7e, 0x7f,
					0x80, 0x81, 0xff
				};

				mpA.constellation[which][0] =
				    starts[startcase];
				mpB.constellation[which][0] =
				    starts[startcase];

				memset(outA, 0x5a, sizeof(outA));
				memset(outB, 0x5a, sizeof(outB));

				g = our_findNext(cdA, outA, which, ucodeA,
						 altA, dminA, extraA);
				r = ref_findNext(cdB, outB, which, ucodeB,
						 altB, dminB, extraB);

				diff_eq_int("findNextUcodeToAdd returns"
					    " (start %ld)", g, r,
					    starts[startcase]);
				diff_eq_obj("findNextUcodeToAdd out",
					    unsigned char[4], outA, outB,
					    starts[startcase]);
				if (g)
					seenFound++;
				else
					seenRanOff++;
				if (cdA->compandingLaw != 0)
					seenAlaw++;
				else
					seenUlaw++;
				if (outA[0] != starts[startcase])
					seenWalked++;
			}
		}

		diff_eq_obj("findNextUcodeToAdd leaves ucode alone",
			    short[7][128], ucodeA, ucodeB, trial);
		diff_eq_obj("findNextUcodeToAdd leaves alt alone",
			    short[7][128], altA, altB, trial);
		diff_eq_obj("findNextUcodeToAdd leaves the sixth alone",
			    unsigned char[7][128], extraA, extraB, trial);
		diff_eq_obj("findNextUcodeToAdd leaves the mapping alone",
			    V90MappingParams, &mpA, &mpB, trial);
	}

	/*
	 * Every outcome, or this is a sweep in which both sides said no.  The
	 * return is `(signed char)i >= 0`, so a run that never went off the end
	 * would never see 0, and one that always did would never see 1.
	 */
	diff_eq_int("the walk stopped inside the row %ld times",
		    seenFound > 0, 1, seenFound);
	diff_eq_int("the walk ran off the row %ld times",
		    seenRanOff > 0, 1, seenRanOff);
	diff_eq_int("the A-law arm ran %ld times", seenAlaw > 0, 1, seenAlaw);
	diff_eq_int("the u-law arm ran %ld times", seenUlaw > 0, 1, seenUlaw);
	diff_eq_int("the walk actually advanced %ld times",
		    seenWalked > 0, 1, seenWalked);
	check_this(0);

	return diff_end();
}

/*
 * ===========================================================================
 * determineDminForRrn
 * ===========================================================================
 *
 * THE HAZARDS ARE HANGS, NOT DIFFERENCES, so the fixture excludes them by
 * construction rather than by tolerance.  The function inlines D324's doubling
 * loop TWICE, and each runs about 2^32 times when its truncation goes
 * negative or lands on 0x80000000:
 *
 *   `maxSize` zero.  If none of the six `constellationSize` entries has a zero
 *   byte at `constelTable + 0x280c`, nothing wins the search, the reciprocal
 *   is an infinity and every `mm[k]` is a NaN.  One of the six is always
 *   planted zero below.
 *
 *   A zero `constellationSize`.  All six are in the product, so one zero makes
 *   it zero, `log10(0)` is minus infinity and the target goes to infinity.
 *   Every size below is at least 1.
 *
 *   `rrn` zero.  Every `mm[k]` is at most 1, so `log2(m)` is at most 0 and the
 *   rate-down target is `(rrn - 0.75 - log2 m) / 6`; at rrn 0 with a uniform
 *   constellation that is negative.  The sweep starts at 1.
 *
 * A SIZE ABOVE 255 IS ALSO EXCLUDED, and for a different reason: the object
 * truncates it to a byte for `nofUcodes`, so a size of 256 enters the
 * rate-down loop as 0, the loop does not run once, and `prevNofUcodes` and
 * `prevDmin` are then READ HAVING NEVER BEEN WRITTEN (D330).  Two sides
 * reading two different stack frames would disagree for a reason that is not
 * about the reconstruction.  D332 records the truncation itself.
 *
 * WHAT THE TRANSCRIPT IS FOR.  Seventeen diagnostics name every arm, and
 * several of the arms are invisible from outside: `prevNofUcodes == maxM` and
 * the two halves of the `||` all write `rrnDownDmin` and print nothing of
 * their own.  So the loud pass below does two things a quiet one cannot --
 * compares the two sides' transcripts, and reads the BLOB's own numbers back
 * out of its transcript to classify which arm ran, which is what turns "the
 * two agree" into "each arm was reached".
 */
static long
dmin_num(const char *t, const char *key, long dflt)
{
	const char *p = strstr(t, key);
	long v;

	if (p == NULL || sscanf(p + strlen(key), "%ld", &v) != 1)
		return dflt;
	return v;
}

static int
dmin_count(const char *t, const char *key)
{
	const char *p = t;
	size_t k = strlen(key);
	int n = 0;

	while ((p = strstr(p, key)) != NULL) {
		n++;
		p += k;
	}
	return n;
}

/* The seventeen, by a substring that matches that site and no other. */
static const char *const dmin_sites[17] = {
	"pParams->m[1..6] = ",
	"rrn down maxM too low",
	"increment maxM for one rate down",
	" rrn down maxM = ",
	"maxM>pParams->m[phase]",
	"rrn down - nofUcodes = ",
	"rrn down - tempDmin  = ",
	":: rrnDownDmin = ",
	"maxM too high",
	"increment maxM for one rate up",
	" rrn up maxM = ",
	"maxM<pParams->m[phase]",
	"rrn up nofUcodes = ",
	"rrn up tempDmin  = ",
	"failed to find rrnUpDmin",
	"tempDmin might cause 2 rates up",
	":: rrnUpDmin = "
};

#define DMIN_RRN	72
#define DMIN_PATTERNS	10
#define DMIN_TRIALS	(DMIN_RRN * DMIN_PATTERNS)

/*
 * Plant one trial's inputs on both sides.  Returns the trial's `dMin`.
 *
 * THE 16-BIT TABLE IS A RAMP AND NOT NOISE, and that is what makes the two
 * searches test anything.  `constelBuild` counts an entry only when it clears a
 * threshold that jumps to `step + value` after every hit, so over uniformly
 * random 16-bit values the count is a RECORD count -- about ln(rowlen), five
 * or so, whatever `step` is.  A count that does not move with `step` makes
 * both loops terminate on their first turn and leaves every arm that depends
 * on where the count crosses `maxM` unreached.  Against a ramp of slope
 * `ramp` the count is about `rowlen * ramp / (step + ramp)`, which is smooth,
 * monotone and tunable -- so the sweep can put the crossing anywhere.
 */
static short
dmin_fixture(int trial)
{
	unsigned i;
	unsigned ramp;
	int k;
	int pattern = trial / DMIN_RRN;
	short dmin;

	reseed(0x5a5au + 131u * (unsigned)trial);
	ramp = 1u + (unsigned)(trial % 29);

	for (i = 0; i < 0xd00 / 2; i++) {
		short v = (short)((i % 128) * ramp
				  + nextrand() % (2u * ramp));

		((short *)tblA)[i] = v;
		((short *)tblB)[i] = v;
	}
	for (i = 0xd00; i < TBLBYTES; i++) {
		unsigned char v = (unsigned char)(nextrand() >> 15);

		/*
		 * The byte table is only ever tested against zero, so an
		 * eighth of it is made zero rather than one in 256 -- and
		 * every twenty-fourth trial makes ALL of it zero, which is
		 * what drives `constelBuild` to answer 0 for ever and both
		 * loops to run out on their 100-iteration cap.
		 */
		if ((v & 7) == 0 || trial % 24 == 0)
			v = 0;
		tblA[i] = v;
		tblB[i] = v;
	}
	for (k = 0; k < 6; k++) {
		unsigned char mk = (unsigned char)((trial >> k) & 1);

		tblA[0x280c + k] = mk;
		tblB[0x280c + k] = mk;
	}
	tblA[0x280c + trial % 6] = 0;
	tblB[0x280c + trial % 6] = 0;

	fill_mp(0x6d61u + 17u * (unsigned)trial);
	for (k = 0; k < 6; k++) {
		unsigned int n;

		switch (pattern) {
		case 0:
			n = 100u;		/* uniform: log2(m) is 0    */
			break;
		case 1:
			n = nextrand() % 200u + 8u;
			break;
		case 2:
			n = (k == trial % 6) ? 250u : 12u;
			break;
		case 3:
			n = nextrand() % 255u + 1u;
			break;
		case 4:
			n = 2u + (unsigned)(trial % 3);	/* tiny */
			break;
		case 5:
		case 8:
			n = 100u;
			break;
		case 6:
			n = 40u + (unsigned)(trial % 7);
			break;
		case 9:
			n = 30u + (unsigned)(trial % 11);
			break;
		default:
			n = 200u + (unsigned)(trial % 40);
			break;
		}
		mpA.constellationSize[k] = n;
		mpB.constellationSize[k] = n;
		/*
		 * The row length bounds the count, so it is swept wide.  A row
		 * shorter than `maxM` leaves the count unable to reach it and
		 * the 100-iteration cap the only exit -- which is a real arm,
		 * and also the state in which `tempDmin` has been multiplied
		 * down to zero and stopped carrying information.  Patterns 5
		 * to 7 use LONG rows and a LARGE `dMin` for the opposite case:
		 * the loop exits on the count, a few turns in, with `tempDmin`
		 * still large enough that a change to the 0.95f or the 1.02f
		 * moves it.  Without them the 0.95f could be mutated to 0.96f
		 * and every check still passed.
		 */
		mpA.constellation[k][0] = (unsigned char)
		    (pattern >= 8 ? nextrand() % 56u + 200u
		   : pattern >= 4 ? nextrand() % 128u + 100u
				  : nextrand() % 120u + 6u);
		mpB.constellation[k][0] = mpA.constellation[k][0];
	}

	parA->unnamed_360 = (int)(nextrand() % 4u);
	parB->unnamed_360 = parA->unnamed_360;

	dmin = (short)(pattern >= 8 ? nextrand() % 140u + 60u
		     : pattern >= 4 ? nextrand() % 12u + 1u
				    : nextrand() % 160u + 1u);
	cdA->dMin = dmin;
	cdB->dMin = dmin;
	cdA->rrnDownDmin = 0x1234;
	cdB->rrnDownDmin = 0x1234;
	cdA->rrnUpDmin = 0x5678;
	cdB->rrnUpDmin = 0x5678;
	return dmin;
}

static int
run_dmin_quiet(void)
{
	int trial;

	diff_begin("V90ConstellationDesigner::determineDminForRrn, quiet");
	wire();

	for (trial = 0; trial < DMIN_TRIALS; trial++) {
		unsigned rrn = 1u + (unsigned)(trial % 72);

		dmin_fixture(trial);
		snap_this();

		our_dmin(cdA, rrn);
		ref_dmin(cdB, rrn);

		diff_eq_int("rrnDownDmin (trial %ld)", cdA->rrnDownDmin,
			    cdB->rrnDownDmin, trial);
		diff_eq_int("rrnUpDmin (trial %ld)", cdA->rrnUpDmin,
			    cdB->rrnUpDmin, trial);
		diff_eq_obj("determineDminForRrn leaves the mapping alone",
			    V90MappingParams, &mpA, &mpB, trial);
		diff_eq_obj("determineDminForRrn leaves the table alone",
			    unsigned char[TBLBYTES], tblA, tblB, trial);

		/*
		 * ONLY THOSE TWO FIELDS ARE WRITTEN, and that is a negative
		 * claim, so it is asserted: put them back and the object must
		 * be what it was.
		 */
		cdA->rrnDownDmin = 0x1234;
		cdB->rrnDownDmin = 0x1234;
		cdA->rrnUpDmin = 0x5678;
		cdB->rrnUpDmin = 0x5678;
		check_this(trial);
	}

	return diff_end();
}

static int dmin_seen[17];
static int dmin_armPrevEqMax;
static int dmin_armTemp;
static int dmin_armPrev;
static int dmin_downLoop;
static int dmin_downArb;
static int dmin_upLoop;
static int dmin_upArb;
static int dmin_upFailed;
static int dmin_capped;
static int dmin_printed;

static int
run_dmin_loud(void)
{
	int trial;
	int i;

	diff_begin("determineDminForRrn's diagnostics, both sides talking");
	wire();

	for (trial = 0; trial < DMIN_TRIALS; trial++) {
		unsigned rrn = 1u + (unsigned)(trial % 72);
		const char *t;
		short dmin;

		dmin = dmin_fixture(trial);
		snap_this();

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;

		our_dmin(cdA, rrn);
		ref_dmin(cdB, rrn);

		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
		dsplib_debug_capture_on = 0;

		diff_eq_int("the transcripts agree (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0 ? 1 : 0,
			    1, trial);
		diff_eq_int("rrnDownDmin, loud (trial %ld)", cdA->rrnDownDmin,
			    cdB->rrnDownDmin, trial);
		diff_eq_int("rrnUpDmin, loud (trial %ld)", cdA->rrnUpDmin,
			    cdB->rrnUpDmin, trial);

		if (dsplib_debug_capture_lines(1) > 0)
			dmin_printed = 1;

		/*
		 * Everything below reads the BLOB's transcript, never ours:
		 * the arm coverage is a claim about what the OBJECT did.
		 */
		t = dsplib_debug_capture_text(1);
		for (i = 0; i < 17; i++)
			if (dmin_count(t, dmin_sites[i]) > 0)
				dmin_seen[i]++;

		if (strstr(t, "rrn down - nofUcodes = ") != NULL) {
			long maxM = dmin_num(t, " rrn down maxM = ", -1);
			long prevNof = dmin_num(t, "prevNofUcodes = ", -2);
			long tempD = dmin_num(t, "rrn down - tempDmin  = ", -1);
			long prevD = dmin_num(t, "prevDmin      = ", -2);
			long got = dmin_num(t, ":: rrnDownDmin = ", -3);

			dmin_downLoop++;
			if (prevNof == maxM) {
				dmin_armPrevEqMax++;
				diff_eq_int("the prevNofUcodes == maxM arm"
					    " answers prevDmin (%ld)",
					    got, prevD, trial);
			} else if (got == tempD && tempD != prevD) {
				dmin_armTemp++;
			} else if (got == prevD && tempD != prevD) {
				dmin_armPrev++;
			}
		} else {
			dmin_downArb++;
		}

		if (strstr(t, "failed to find rrnUpDmin") != NULL)
			dmin_upFailed++;
		else if (strstr(t, "rrn up nofUcodes = ") != NULL)
			dmin_upLoop++;
		else
			dmin_upArb++;

		if (trial % 24 == 0)
			dmin_capped++;

		cdA->rrnDownDmin = 0x1234;
		cdB->rrnDownDmin = 0x1234;
		cdA->rrnUpDmin = 0x5678;
		cdB->rrnUpDmin = 0x5678;
		check_this(trial);

		(void)dmin;
	}

	return diff_end();
}

static int
run_dmin_outcomes(void)
{
	int i;

	diff_begin("determineDminForRrn reached every arm");

	diff_eq_int("the blob printed something (%ld)", dmin_printed, 1, 0);
	for (i = 0; i < 17; i++)
		diff_eq_int("diagnostic %ld fired", dmin_seen[i] > 0, 1, i);

	diff_eq_int("the rate-down search ran %ld times",
		    dmin_downLoop > 0, 1, dmin_downLoop);
	diff_eq_int("the rate-down arbitrary arm ran %ld times",
		    dmin_downArb > 0, 1, dmin_downArb);
	diff_eq_int("the prevNofUcodes == maxM arm ran %ld times",
		    dmin_armPrevEqMax > 0, 1, dmin_armPrevEqMax);
	diff_eq_int("the rate-down `||` answered tempDmin %ld times",
		    dmin_armTemp > 0, 1, dmin_armTemp);
	diff_eq_int("the rate-down `||` answered prevDmin %ld times",
		    dmin_armPrev > 0, 1, dmin_armPrev);
	diff_eq_int("the rate-up search ran %ld times",
		    dmin_upLoop > 0, 1, dmin_upLoop);
	diff_eq_int("the rate-up arbitrary arm ran %ld times",
		    dmin_upArb > 0, 1, dmin_upArb);
	diff_eq_int("the rate-up failure arm ran %ld times",
		    dmin_upFailed > 0, 1, dmin_upFailed);
	diff_eq_int("the 100-iteration cap was driven %ld times",
		    dmin_capped > 0, 1, dmin_capped);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_arith();
	rc |= run_findindex();
	rc |= run_spectral();
	rc |= run_reconstruct();
	rc |= run_constelbuild();
	rc |= run_findnext();
	rc |= run_dmin_quiet();
	rc |= run_dmin_loud();
	rc |= run_dmin_outcomes();

	return rc;
}

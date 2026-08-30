/*
 * t_p2echoleaves.cpp -- differential test of seven leaves claimed by the
 * VPcmV34Main leaf pass:
 *
 *     V90Phase2Info::setToDefault()          0x2a950   49 B
 *     V92Phase2Info::setToDefault()          0x15f10   87 B
 *     V92Phase2Info::printInfo() const       0x16030  572 B
 *     V92EchoCanceller::zeroEchoCoeff()      0x10f60   45 B
 *     V92EchoCanceller::resetEchoHistory()   0x10f90   62 B
 *     print_echo_coeffs(float *, unsigned)   0x10a30  179 B
 *     V92Precoder::reset()                   0x56d80   87 B
 *
 * All callerless exported API, driven through the `ref_` aliases (F7000).
 *
 * WHAT IS SHARED AND WHAT IS SPLIT, per t_v90unpck's rule: parameter blocks
 * and `L2` are INPUT, one copy pointed at by both sides, so the pointers
 * compare equal and stay in the object comparison.  The echo canceller's two
 * arrays and the two FloatFIR histories are WRITTEN, one per side, seeded
 * identically and compared as content; the pointer fields that hold them are
 * normalised to small tags before the object comparison, after being checked
 * unchanged.
 *
 * `printInfo` and `print_echo_coeffs` are transcript tests in the
 * t_v90p4dleaf mould: level swept, both sides captured, the transcripts
 * compared byte for byte -- and required NON-EMPTY where the gating says
 * something must print, so a silent pair cannot pass as agreement.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/FloatFIR.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Phase2Info.h"
#include "dsplib/V92EchoCanceller.h"
#include "dsplib/V92Parameters.h"
#include "dsplib/V92Phase2Info.h"
#include "dsplib/V92Precoder.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void ref_p90_default(void *self)
	asm("ref__ZN13V90Phase2Info12setToDefaultEv");
void ref_p92_default(void *self)
	asm("ref__ZN13V92Phase2Info12setToDefaultEv");
void ref_p92_print(const void *self)
	asm("ref__ZNK13V92Phase2Info9printInfoEv");
void ref_ec_zerocoeff(void *self)
	asm("ref__ZN16V92EchoCanceller13zeroEchoCoeffEv");
void ref_ec_resethist(void *self)
	asm("ref__ZN16V92EchoCanceller16resetEchoHistoryEv");
void ref_print_echo(float *coeffs, unsigned int n)
	asm("ref__Z17print_echo_coeffsPfj");
void ref_prec_reset(void *self) asm("ref__ZN11V92Precoder5resetEv");
}

/* ------------------------------------------------------------- storage */

#define GUARD	64

#define P90_SLOT	((unsigned)sizeof(V90Phase2Info) + GUARD)
#define P92_SLOT	((unsigned)sizeof(V92Phase2Info) + GUARD)
#define EC_SLOT		((unsigned)sizeof(V92EchoCanceller) + GUARD)
#define PREC_SLOT	((unsigned)sizeof(V92Precoder) + GUARD)
#define FIR_SLOT	((unsigned)sizeof(FloatFIR) + GUARD)

static unsigned char p90[2][P90_SLOT] __attribute__((aligned(8)));
static unsigned char p92[2][P92_SLOT] __attribute__((aligned(8)));
static unsigned char ec[2][EC_SLOT] __attribute__((aligned(8)));
static unsigned char prec[2][PREC_SLOT] __attribute__((aligned(8)));
static unsigned char fir[2][2][FIR_SLOT] __attribute__((aligned(8)));

static unsigned char par90[sizeof(V90Parameters)] __attribute__((aligned(8)));
static unsigned char par92[sizeof(V92Parameters)] __attribute__((aligned(8)));
static float l2shared[24];

/* The written arrays: [side], guarded. */
#define NCOEFF	48u
#define NHIST	160u
static float coeff[2][NCOEFF + 8];
static float hist[2][NHIST + 8];
#define NFIRHIST 32u
static float firhist[2][2][NFIRHIST + 4];

#define P90_(s)	((V90Phase2Info *)(void *)p90[s])
#define P92_(s)	((V92Phase2Info *)(void *)p92[s])
#define EC_(s)	((V92EchoCanceller *)(void *)ec[s])
#define PREC_(s) ((V92Precoder *)(void *)prec[s])
#define FIR_(s, k) ((FloatFIR *)(void *)fir[s][k])

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)((lfsr >> 3) | 1u);
}

static void
fill_pair(unsigned char *a, unsigned char *b, unsigned n)
{
	unsigned i;

	for (i = 0; i < n; i++)
		a[i] = nextb();
	if (b != 0)
		memcpy(b, a, n);
}

static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

#define NTRIAL	16

/* -------------------------------------------------- the two setToDefault */

static int
run_p90_default(void)
{
	int trial, moved = 0;

	diff_begin("V90Phase2Info::setToDefault");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[P90_SLOT];

		lfsr = 0x3101u + 0x9e37u * (unsigned)trial;
		fill_pair(par90, 0, (unsigned)sizeof par90);
		fill_pair(p90[0], p90[1], P90_SLOT);
		P90_(0)->params = P90_(1)->params = (V90Parameters *)par90;
		memcpy(before, p90[0], P90_SLOT);

		P90_(0)->setToDefault();
		ref_p90_default(p90[1]);

		diff_eq_obj("after setToDefault", V90Phase2Info, P90_(0),
			    P90_(1), trial);
		diff_eq_int("the guard held (%ld)",
			    memcmp(p90[0] + sizeof(V90Phase2Info),
				   before + sizeof(V90Phase2Info),
				   GUARD) == 0, 1, trial);
		diff_eq_int("rtd was copied (%ld)", P90_(0)->rtd,
			    ((V90Parameters *)(void *)par90)->PHASE2_INFO_RTD,
			    trial);
		if (memcmp(before, p90[0], P90_SLOT) != 0)
			moved = 1;
	}

	diff_eq_int("setToDefault changed the object", moved, 1, 0);

	return diff_end();
}

static int
run_p92_default(void)
{
	int trial, moved = 0;

	diff_begin("V92Phase2Info::setToDefault");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[P92_SLOT];

		lfsr = 0x3202u + 0x9e37u * (unsigned)trial;
		fill_pair(par92, 0, (unsigned)sizeof par92);
		fill_pair(p92[0], p92[1], P92_SLOT);
		P92_(0)->params = P92_(1)->params = (V92Parameters *)par92;
		memcpy(before, p92[0], P92_SLOT);

		P92_(0)->setToDefault();
		ref_p92_default(p92[1]);

		diff_eq_obj("after setToDefault", V92Phase2Info, P92_(0),
			    P92_(1), trial);
		diff_eq_int("the guard held (%ld)",
			    memcmp(p92[0] + sizeof(V92Phase2Info),
				   before + sizeof(V92Phase2Info),
				   GUARD) == 0, 1, trial);
		diff_eq_int("the local capability byte was set (%ld)",
			    P92_(0)->v92CapabilitiesLocal, 1, trial);
		diff_eq_int("the filter geometry was copied (%ld)",
			    P92_(0)->nofFilterSections,
			    (unsigned char)((V92Parameters *)(void *)par92)
				->V92_NOF_FILTER_SECTIONS, trial);
		if (memcmp(before, p92[0], P92_SLOT) != 0)
			moved = 1;
	}

	diff_eq_int("setToDefault changed the object", moved, 1, 0);

	return diff_end();
}

/* ----------------------------------------------------------- printInfo */

static int
run_p92_print(void)
{
	int trial, printedGated = 0, distinct = 0;
	char first[128];

	diff_begin("V92Phase2Info::printInfo");

	first[0] = 0;
	for (trial = 0; trial < NTRIAL * 2; trial++) {
		unsigned char before[P92_SLOT];
		unsigned lvl = (unsigned)(trial % 4);
		int i;

		lfsr = 0x3303u + 0x9e37u * (unsigned)trial;
		fill_pair(p92[0], p92[1], P92_SLOT);
		for (i = 0; i < 24; i++)
			l2shared[i] = (float)(i - 11)
			    * (1.75f + 0.125f * (float)trial)
			    + ((trial & 1) ? 0.0009765625f : 0.0f);
		P92_(0)->L2 = P92_(1)->L2 = l2shared;
		/* Both %s selectors, both values and an out-of-range one. */
		P92_(0)->pcmType = P92_(1)->pcmType = trial % 3;
		P92_(0)->txPowerMeasurementPoint =
		    P92_(1)->txPowerMeasurementPoint = (trial / 3) % 3;
		memcpy(before, p92[0], P92_SLOT);

		set_level(lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		P92_(0)->printInfo();
		ref_p92_print(p92[1]);
		dsplib_debug_capture_on = 0;
		set_level(0);

		diff_eq_int("the transcripts agree (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    trial);
		diff_eq_int("the line counts agree (%ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), trial);
		diff_eq_obj("printInfo left the object alone", V92Phase2Info,
			    P92_(0), P92_(1), trial);
		diff_eq_int("and really left it alone (%ld)",
			    memcmp(before, p92[0], P92_SLOT) == 0, 1, trial);

		if (lvl > 1) {
			/* The gated lines printed: 4 + 21 through the
			 * captured dsplibs_debug_printf.  edprintf's output
			 * lands there too, encoded; non-empty is the claim. */
			diff_eq_int("something printed at level %ld",
				    dsplib_debug_capture_lines(1) > 0, 1,
				    (long)lvl);
			printedGated++;
			if (first[0] == 0) {
				strncpy(first, dsplib_debug_capture_text(1),
					127);
				first[127] = 0;
			} else if (strncmp(first,
					   dsplib_debug_capture_text(1),
					   127) != 0) {
				distinct = 1;
			}
		}
	}

	diff_eq_int("the gated arm was reached", printedGated > 0, 1, 0);
	diff_eq_int("the transcript varies with the record", distinct, 1, 0);

	return diff_end();
}

/* ------------------------------------------------------ the echo leaves */

static int
run_ec(void)
{
	int trial, moved = 0;

	diff_begin("V92EchoCanceller::{zeroEchoCoeff,resetEchoHistory}");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[EC_SLOT];
		float fbefore[NHIST + 8];
		unsigned int flen = 8u + 4u * (unsigned)(trial % 9);
		unsigned int delay = (unsigned)(trial % 13);
		int s;

		lfsr = 0x3404u + 0x9e37u * (unsigned)trial;
		fill_pair(par92, 0, (unsigned)sizeof par92);
		/* Keep echoLength inside the history buffer. */
		((V92Parameters *)(void *)par92)->V92_ECHO_DELAY_OFFSET =
		    (int)(trial % 5);
		fill_pair(ec[0], ec[1], EC_SLOT);
		fill_pair((unsigned char *)coeff[0],
			  (unsigned char *)coeff[1], sizeof coeff[0]);
		fill_pair((unsigned char *)hist[0],
			  (unsigned char *)hist[1], sizeof hist[0]);
		memcpy(fbefore, hist[0], sizeof fbefore);

		for (s = 0; s < 2; s++) {
			EC_(s)->params = (V92Parameters *)par92;
			EC_(s)->filterLength = flen;
			EC_(s)->echoDelay = delay;
			EC_(s)->echoCoeff = coeff[s];
			EC_(s)->echoHistory = hist[s];
		}
		memcpy(before, ec[0], EC_SLOT);

		EC_(0)->zeroEchoCoeff();
		ref_ec_zerocoeff(ec[1]);
		EC_(0)->resetEchoHistory();
		ref_ec_resethist(ec[1]);

		/* The two written arrays, guards included. */
		diff_eq_int("the coefficient banks agree (%ld)",
			    memcmp(coeff[0], coeff[1], sizeof coeff[0]) == 0,
			    1, trial);
		diff_eq_int("the histories agree (%ld)",
			    memcmp(hist[0], hist[1], sizeof hist[0]) == 0, 1,
			    trial);
		diff_eq_int("the coefficients really were zeroed (%ld)",
			    coeff[0][0] == 0.0f && coeff[0][flen - 1] == 0.0f,
			    1, trial);
		diff_eq_int("the history guard held (%ld)",
			    memcmp(hist[0] + EC_(0)->echoLength,
				   fbefore + EC_(0)->echoLength,
				   (NHIST + 8 - EC_(0)->echoLength)
				   * sizeof(float)) == 0, 1, trial);

		/* The object: the two per-side pointers checked, then
		 * normalised out of the whole-object comparison. */
		for (s = 0; s < 2; s++) {
			diff_eq_int("echoCoeff was not moved (%ld)",
				    EC_(s)->echoCoeff == coeff[s], 1, trial);
			diff_eq_int("echoHistory was not moved (%ld)",
				    EC_(s)->echoHistory == hist[s], 1, trial);
			EC_(s)->echoCoeff = (float *)0x11;
			EC_(s)->echoHistory = (float *)0x22;
		}
		diff_eq_obj("after the pair", V92EchoCanceller, EC_(0),
			    EC_(1), trial);
		diff_eq_int("the guard held (%ld)",
			    memcmp(ec[0] + sizeof(V92EchoCanceller),
				   before + sizeof(V92EchoCanceller),
				   GUARD) == 0, 1, trial);
		diff_eq_int("the rebuilt length is the three-term sum (%ld)",
			    (long)EC_(0)->echoLength,
			    (long)((flen >> 1) + delay
				   + (unsigned)(trial % 5)), trial);
		if (memcmp(before, ec[0], EC_SLOT) != 0)
			moved = 1;
	}

	diff_eq_int("the pair changed the object", moved, 1, 0);

	return diff_end();
}

static int
run_print_echo(void)
{
	int trial, nonEmpty = 0;

	diff_begin("print_echo_coeffs");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned int n = (unsigned)(trial % 7) * 3u;
		unsigned lvl = (unsigned)(trial % 3);
		unsigned int i;

		for (i = 0; i < NCOEFF; i++)
			coeff[0][i] = ((float)i - 7.5f)
			    * (0.375f + 0.0625f * (float)trial)
			    + ((trial & 1) ? 0.0000152587890625f : 0.0f);

		set_level(lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		print_echo_coeffs(coeff[0], n);
		ref_print_echo(coeff[0], n);
		dsplib_debug_capture_on = 0;
		set_level(0);

		diff_eq_int("the transcripts agree (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    trial);
		if (lvl > 0 && strlen(dsplib_debug_capture_text(1)) > 0)
			nonEmpty = 1;
	}

	/*
	 * The banner is unconditional in the FUNCTION; `edprintf` still
	 * self-gates one level down (encode.h), which is why the level is
	 * swept above and non-emptiness is demanded only where the gate is
	 * open.  An all-empty run would mean the agreement was vacuous.
	 */
	diff_eq_int("something was printed", nonEmpty, 1, 0);

	return diff_end();
}

/* ------------------------------------------------------ V92Precoder::reset */

static int
run_prec_reset(void)
{
	int trial, moved = 0;

	diff_begin("V92Precoder::reset()");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[PREC_SLOT];
		unsigned lvl = (trial & 1) ? 2u : 0u;
		int s, k;

		lfsr = 0x3505u + 0x9e37u * (unsigned)trial;
		fill_pair(prec[0], prec[1], PREC_SLOT);
		fill_pair(fir[0][0], fir[1][0], FIR_SLOT);
		fill_pair(fir[0][1], fir[1][1], FIR_SLOT);
		fill_pair((unsigned char *)firhist[0][0],
			  (unsigned char *)firhist[1][0],
			  sizeof firhist[0][0]);
		fill_pair((unsigned char *)firhist[0][1],
			  (unsigned char *)firhist[1][1],
			  sizeof firhist[0][1]);

		for (s = 0; s < 2; s++) {
			for (k = 0; k < 2; k++) {
				FloatFIR *f = FIR_(s, k);

				f->history = firhist[s][k];
				f->taps = 8u + 4u * (unsigned)(trial % 3);
				f->bufferLength = NFIRHIST;
				f->coefficients = 0;
			}
			PREC_(s)->fir1 = FIR_(s, 0);
			PREC_(s)->fir2 = FIR_(s, 1);
		}
		memcpy(before, prec[0], PREC_SLOT);

		set_level(lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		PREC_(0)->reset();
		ref_prec_reset(prec[1]);
		dsplib_debug_capture_on = 0;
		set_level(0);

		diff_eq_int("the transcripts agree (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    trial);
		if (lvl > 1)
			diff_eq_int("the level-2 line printed (%ld)",
				    dsplib_debug_capture_lines(1) > 0, 1,
				    trial);

		/* Both filters on both sides: history zeroed, index reset. */
		for (s = 0; s < 2; s++)
			for (k = 0; k < 2; k++) {
				FloatFIR *f = FIR_(s, k);

				diff_eq_int("the history was zeroed (%ld)",
					    firhist[s][k][0] == 0.0f
					    && firhist[s][k][NFIRHIST - 1]
					       == 0.0f, 1,
					    trial * 4 + s * 2 + k);
				diff_eq_int("the index was reset (%ld)",
					    (long)f->index,
					    (long)(f->bufferLength - f->taps),
					    trial * 4 + s * 2 + k);
			}
		diff_eq_int("the two sides' histories agree (%ld)",
			    memcmp(firhist[0][0], firhist[1][0],
				   sizeof firhist[0][0]) == 0
			    && memcmp(firhist[0][1], firhist[1][1],
				      sizeof firhist[0][1]) == 0, 1, trial);

		/* The precoder object itself: nothing but the two filters is
		 * touched, and the pointers are per-side, so normalise. */
		for (s = 0; s < 2; s++) {
			PREC_(s)->fir1 = (FloatFIR *)0x11;
			PREC_(s)->fir2 = (FloatFIR *)0x22;
		}
		diff_eq_obj("after reset", V92Precoder, PREC_(0), PREC_(1),
			    trial);
		diff_eq_int("the guard held (%ld)",
			    memcmp(prec[0] + sizeof(V92Precoder),
				   before + sizeof(V92Precoder), GUARD) == 0,
			    1, trial);
		if (firhist[0][0][0] == 0.0f)
			moved = 1;
	}

	diff_eq_int("reset really reset something", moved, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_p90_default();
	rc |= run_p92_default();
	rc |= run_p92_print();
	rc |= run_ec();
	rc |= run_print_echo();
	rc |= run_prec_reset();

	return rc;
}

/*
 * t_v90spectrellis.cpp -- differential test of the V.90 spectral shaper's
 * trellis search and its frame pump:
 *
 *     V90SpectralShaper::advanceTrellis()                          0x32ba0
 *     V90SpectralShaper::process(short *, unsigned char *, short *) 0x32fc0
 *
 * WHAT MAKES THIS ONE HARD TO TEST HONESTLY.  `advanceTrellis` is a discrete
 * argmax over float metrics: it scores 2^(shaperId+1) candidates with
 * `V90SpectralShapingFilter::getMetric` and keeps the smallest.  Two failure
 * modes follow from that shape and the suite is built around both.
 *
 * ONE -- A DEGENERATE FILTER MAKES THE SEARCH UNTESTABLE WHILE THE TEST PASSES.
 * If the coefficients leave every candidate scoring alike, candidate 0 always
 * wins, the committed action is always the same, and a reconstruction that had
 * the digit order backwards or the metric comparison reversed would still
 * agree with the blob on every trial.  That is findings F3509 and F3403 exactly.
 * So the suite does not merely count trials: `run_trellis` recovers WHICH
 * ACTION was committed on each trial by comparing the delay line's head
 * against the four polarity patterns computed here, and asserts that across
 * the trial set **all four actions occur**, both `state` values occur, and the
 * committed action is not a function of `shaperId` alone.  If any of those
 * stops holding the suite fails rather than quietly measuring one arm.
 *
 * TWO -- A NEAR-TIE TURNS A ONE-ULP METRIC DIFFERENCE INTO A DIFFERENT ANSWER.
 * That is a property of the object and not a defect, but it makes a failure
 * here point at `getMetric`'s x87 path before it points at this file's control
 * flow.  The delay lines below are deliberately asymmetric and large-valued so
 * the winning margins are wide; `diff_end` reporting a handful of trials out of
 * hundreds would mean a tie, and all-or-nothing means a structural difference.
 * Exact ties go to the EARLIER candidate, because the object's test is a strict
 * `metric < best`.
 *
 * `process` IS TESTED AS A SEQUENCE, NOT AS A CALL.  `primeFrames` holds the
 * trellis off for the first `shaperId` calls and the delay line fills over
 * `shaperId + 1` of them, so a single call exercises neither the search nor the
 * shift-down.  Each case runs twelve consecutive frames and compares after
 * every one.
 *
 * `writeIndex` IS INVARIANT ACROSS EVERY `process` PATH -- the write loop
 * advances it by `blockLength` and the tail subtracts the same amount back --
 * so the check that compares it is passing trivially and is not evidence.  It
 * is kept because the field is compared as part of the object anyway, and said
 * out loud here so nobody counts it as coverage.  What is NOT trivial is the
 * delay line's CONTENT, which the shift-down moves every call.
 *
 * IN RANGE THROUGHOUT, which is D561's rule.  `shaperId <= 3` because
 * `actionLookupTable` has eight rows and the index is `2 * shaperId + state`;
 * `blockLength >= 1` because `process` computes its first loop bound as an
 * UNSIGNED `blockLength - 1` and would run four billion iterations at zero
 * (D930); `(shaperId + 1) * blockLength <= 24` because that is what both heap
 * buffers hold; and `state` is only ever 0 or 1 because nothing else is a row.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/DiffCoder.h"
#include "dsplib/V90SpectralShaper.h"
#include "dsplib/V90SpectralShapingFilter.h"

extern "C" {
void our_ss_ctor(void *self) asm("_ZN17V90SpectralShaperC1Ev");
void ref_ss_ctor(void *self) asm("ref__ZN17V90SpectralShaperC1Ev");
void our_ss_dtor(void *self) asm("_ZN17V90SpectralShaperD1Ev");
void ref_ss_dtor(void *self) asm("ref__ZN17V90SpectralShaperD1Ev");

void our_ss_reset(void *self, unsigned id, unsigned sr, unsigned a1,
		  unsigned a2, unsigned b1, unsigned b2)
	asm("_ZN17V90SpectralShaper5resetEjjffff");
void ref_ss_reset(void *self, unsigned id, unsigned sr, unsigned a1,
		  unsigned a2, unsigned b1, unsigned b2)
	asm("ref__ZN17V90SpectralShaper5resetEjjffff");

void our_ss_advance(void *self) asm("_ZN17V90SpectralShaper14advanceTrellisEv");
void ref_ss_advance(void *self)
	asm("ref__ZN17V90SpectralShaper14advanceTrellisEv");

void our_ss_process(void *self, short *in, unsigned char *bits, short *out)
	asm("_ZN17V90SpectralShaper7processEPsPhS0_");
void ref_ss_process(void *self, short *in, unsigned char *bits, short *out)
	asm("ref__ZN17V90SpectralShaper7processEPsPhS0_");

extern unsigned int ref_dsplibs_debug_level;
}

/* ------------------------------------------------------------- the fixture */

#define SS_SLOT		0x90u	/* 0x6c of object, the rest a guard */
#define SS_BUF		24u	/* entries in each of the two buffers */
#define OUT_ENTS	16u	/* 6 usable, the rest a guard         */
#define OUT_FILL	0x5a

union big_slot {
	double align;
	unsigned char raw[SS_SLOT];
};

static union big_slot ss_a, ss_b;

#define SS_A (*(V90SpectralShaper *)ss_a.raw)
#define SS_B (*(V90SpectralShaper *)ss_b.raw)

static short line[SS_BUF];
static short out_a[OUT_ENTS], out_b[OUT_ENTS];

static unsigned lfsr;

static unsigned char
next_byte(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)(lfsr >> 3);
}

static void
fill_pair(void *a, void *b, unsigned n, int trial)
{
	unsigned char *pa = (unsigned char *)a;
	unsigned char *pb = (unsigned char *)b;
	unsigned i;

	lfsr = 0x6d1bu + 0x9e37u * (unsigned)trial;
	for (i = 0; i < n; i++)
		pa[i] = pb[i] = next_byte();
}

/*
 * A REAL FILTER AND NOT A FLAT ONE.  These are the two cascaded first-order
 * sections V90SpectralShapingFilter.h describes -- two poles at 0.75 and 0.5,
 * two zeros at -0.25 and 0.4 -- chosen so the accumulated energy genuinely
 * depends on the polarity pattern.  A zero row would make every candidate
 * score identically and is the degenerate case this file exists to avoid; the
 * "all four actions occur" assertion below is what proves these are not it.
 */
#define C_A1	0x3f400000u	/* 0.75f  */
#define C_A2	0x3f000000u	/* 0.5f   */
#define C_B1	0xbe800000u	/* -0.25f */
#define C_B2	0x3ecccccdu	/* 0.4f   */

/*
 * Asymmetric and large, so the winning margin is wide and the argmax is not
 * decided by a last-bit difference in `getMetric`.  Never zero -- finding F230
 * -- and bounded well inside `short`, so that every sample's negation is a
 * DIFFERENT short: 0 and -32768 are each their own negation and either one
 * would make two of the four polarity patterns coincide, which is exactly
 * what `committed_action` must not have happen.
 */
static void
seed_line(int trial)
{
	unsigned i;

	lfsr = 0x1357u + 0x4f1bu * (unsigned)trial;
	for (i = 0; i < SS_BUF; i++) {
		int v = (int)((unsigned)next_byte() << 6)
		    + (int)next_byte() + 0x400;

		if ((i & 3u) == 3u)
			v = -v - 0x137;
		line[i] = (short)v;
	}
}

static short *ptr_pre_a_d, *ptr_pre_a_t, *ptr_pre_b_d, *ptr_pre_b_t;
static unsigned char *ptr_pre_a_p, *ptr_pre_b_p;

static void
ptr_save(void)
{
	ptr_pre_a_d = SS_A.delayLine;
	ptr_pre_a_t = SS_A.trialLine;
	ptr_pre_a_p = SS_A.pde.state_;
	ptr_pre_b_d = SS_B.delayLine;
	ptr_pre_b_t = SS_B.trialLine;
	ptr_pre_b_p = SS_B.pde.state_;
}

/*
 * The three heap pointers are SKIPPED in the object comparison because the two
 * sides allocate their own and always will.  Skipping is not free, so they get
 * their own assertion: unmoved against that side's OWN pre-call values,
 * non-null, and distinct.  Neither member under test may move them.
 */
static void
ptr_check(long tag)
{
	diff_eq_int("our three pointers are unmoved, non-null and distinct "
		    "(%ld)",
		    SS_A.delayLine == ptr_pre_a_d
		    && SS_A.trialLine == ptr_pre_a_t
		    && SS_A.pde.state_ == ptr_pre_a_p
		    && SS_A.delayLine != 0 && SS_A.trialLine != 0
		    && SS_A.pde.state_ != 0
		    && SS_A.delayLine != SS_A.trialLine, 1, tag);
	diff_eq_int("the blob's three pointers likewise (%ld)",
		    SS_B.delayLine == ptr_pre_b_d
		    && SS_B.trialLine == ptr_pre_b_t
		    && SS_B.pde.state_ == ptr_pre_b_p
		    && SS_B.delayLine != 0 && SS_B.trialLine != 0
		    && SS_B.pde.state_ != 0
		    && SS_B.delayLine != SS_B.trialLine, 1, tag);
}

static void
ss_snapshot(void *dst, const unsigned char *src)
{
	const V90SpectralShaper *s = (const V90SpectralShaper *)src;
	V90SpectralShaper *d = (V90SpectralShaper *)dst;

	memcpy(dst, src, sizeof(V90SpectralShaper));
	d->delayLine = (short *)(long)(s->delayLine != 0);
	d->trialLine = (short *)(long)(s->trialLine != 0);
	d->pde.state_ = (unsigned char *)(long)(s->pde.state_ != 0);
}

static void
same_state(const char *what, long tag)
{
	unsigned char sa[sizeof(V90SpectralShaper)];
	unsigned char sb[sizeof(V90SpectralShaper)];

	ss_snapshot(sa, ss_a.raw);
	ss_snapshot(sb, ss_b.raw);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90SpectralShaper", sa, sb,
		     sizeof(V90SpectralShaper), tag);
	ptr_check(tag);
	diff_eq_int("the delay lines agree (%ld)",
		    memcmp(SS_A.delayLine, SS_B.delayLine,
			   SS_BUF * sizeof(short)) == 0, 1, tag);
	diff_eq_int("the trial lines agree (%ld)",
		    memcmp(SS_A.trialLine, SS_B.trialLine,
			   SS_BUF * sizeof(short)) == 0, 1, tag);
	diff_eq_int("the parallel encoder's state agrees (%ld)",
		    memcmp(SS_A.pde.state_, SS_B.pde.state_,
			   SS_A.pde.capacity_) == 0, 1, tag);
	diff_eq_int("no store past the object (%ld)",
		    memcmp(ss_a.raw + sizeof(V90SpectralShaper),
			   ss_b.raw + sizeof(V90SpectralShaper),
			   SS_SLOT - sizeof(V90SpectralShaper)) == 0, 1, tag);
}

static unsigned
setup(unsigned id, unsigned sr, unsigned st, int trial)
{
	harness_alloc_reset();
	fill_pair(ss_a.raw, ss_b.raw, SS_SLOT, trial);

	our_ss_ctor(ss_a.raw);
	ref_ss_ctor(ss_b.raw);
	our_ss_reset(ss_a.raw, id, sr, C_A1, C_A2, C_B1, C_B2);
	ref_ss_reset(ss_b.raw, id, sr, C_A1, C_A2, C_B1, C_B2);

	SS_A.state = st;
	SS_B.state = st;

	ptr_save();
	return SS_B.blockLength;
}

static void
teardown(long tag)
{
	our_ss_dtor(ss_a.raw);
	ref_ss_dtor(ss_b.raw);
	diff_eq_int("no leak, no bad free (%ld)",
		    harness_alloc.live == 0 && harness_alloc.bad_free == 0, 1,
		    tag);
}

struct ss_case {
	unsigned id;
	unsigned sr;
};

static const struct ss_case ss_cases[] = {
	{ 0u, 1u }, { 1u, 1u }, { 2u, 1u }, { 3u, 1u },
	{ 0u, 2u }, { 1u, 2u }, { 2u, 2u }, { 3u, 2u },
	{ 0u, 3u }, { 1u, 3u }, { 2u, 3u }, { 3u, 3u },
	{ 0u, 6u }, { 1u, 6u }, { 2u, 6u }, { 3u, 6u }
};

#define NSS (sizeof(ss_cases) / sizeof(ss_cases[0]))

/* ==================================== V90SpectralShaper::advanceTrellis === */

/*
 * Which of the four polarity patterns was committed to the head frame, read
 * back from the delay line.  -1 if it matches none of them, which is itself a
 * result worth failing on.  This is the OBSERVABLE the separation counters
 * below are built on: not a path, not a constant, but which arm the search
 * actually chose (findings F3509, F3403).
 */
/*
 * AT `blockLength` 1 THIS CANNOT WORK AND MUST NOT PRETEND TO.  A one-sample
 * frame has no even/odd structure: KEEP_ALL and NEGATE_ODD both leave sample 0
 * alone, NEGATE_ALL and NEGATE_EVEN both negate it, so two pairs of actions
 * produce identical delay lines and no observer can separate them.  The
 * caller skips the absolute checks in that case rather than reading the first
 * match and calling it the answer -- which is what the first version of this
 * file did, and it reported the blob as wrong on 40 trials that were the
 * test's own ambiguity.  The DIFFERENTIAL comparison still runs there; it is
 * only the inference that is unavailable.
 */
static int
committed_action(const short *after, const short *before, unsigned bl)
{
	int a;

	if (bl < 2u)
		return -1;

	for (a = 0; a < 4; a++) {
		unsigned i;
		int ok = 1;

		for (i = 0; i < bl; i++) {
			int v = before[i];

			if (a == 1
			    || (a == 2 && (i & 1u) == 0u)
			    || (a == 3 && (i & 1u) != 0u))
				v = -v;
			if (after[i] != (short)v)
				ok = 0;
		}
		if (ok)
			return a;
	}
	return -1;
}

static int
run_trellis(void)
{
	unsigned c, st, k;
	long tag = 0;
	int seen_action[4];
	int seen_state[2];
	int id_varies = 0;
	int a;

	diff_begin("V90SpectralShaper::advanceTrellis");

	for (a = 0; a < 4; a++)
		seen_action[a] = 0;
	seen_state[0] = seen_state[1] = 0;

	for (c = 0; c < NSS; c++) {
		int first_for_case = -2;

		for (st = 0; st < 2u; st++) {
			for (k = 0; k < 8u; k++) {
				unsigned bl;
				int act;

				tag++;
				bl = setup(ss_cases[c].id, ss_cases[c].sr, st,
					   (int)tag);

				seed_line((int)tag);
				memcpy(SS_A.delayLine, line,
				       SS_BUF * sizeof(short));
				memcpy(SS_B.delayLine, line,
				       SS_BUF * sizeof(short));

				our_ss_advance(ss_a.raw);
				ref_ss_advance(ss_b.raw);

				same_state("after advanceTrellis", tag);

				/*
				 * ABSOLUTE.  Two sides that both failed to
				 * store compare equal (findings F223, F224), so
				 * the blob's own delay line is checked against
				 * the four patterns computed here, and its
				 * state word against the committed action's
				 * low bit -- which is the trellis relation the
				 * header derives and the one thing that ties
				 * `actionLookupTable`'s digit encoding to what
				 * the object actually does.
				 */
				act = committed_action(SS_B.delayLine, line,
						       bl);
				if (bl >= 2u)
					diff_eq_int("the head frame is one of "
						    "the four patterns (%ld)",
						    act >= 0, 1, tag);
				if (act >= 0) {
					seen_action[act] = 1;
					diff_eq_int("state is the committed "
						    "action's low bit (%ld)",
						    (long)SS_B.state,
						    (long)(act & 1), tag);
					if (first_for_case == -2)
						first_for_case = act;
					else if (first_for_case != act)
						id_varies = 1;
				}
				diff_eq_int("state is 0 or 1 (%ld)",
					    SS_B.state <= 1u, 1, tag);
				seen_state[SS_B.state & 1u] = 1;

				/*
				 * The tail of the line past `blockLength` is
				 * untouched by the commit: `applyFrameAction`
				 * writes exactly one frame at offset zero.
				 */
				diff_eq_int("only the head frame is committed "
					    "(%ld)",
					    memcmp(SS_B.delayLine + bl,
						   line + bl,
						   (SS_BUF - bl)
						   * sizeof(short)) == 0, 1,
					    tag);

				teardown(tag);
			}
		}
	}

	/*
	 * THE SEARCH IS NOT DEGENERATE.  All four actions must be reachable
	 * across the trial set, both states must occur, and within at least
	 * one (shaperId, blockLength) case the committed action must vary --
	 * otherwise the answer is a function of the configuration and the
	 * metric is doing no work.  The MUTATION suite adjudicates; these are
	 * the guards that stop a green run being vacuous.
	 */
	diff_eq_int("all four actions are committed somewhere",
		    seen_action[0] && seen_action[1] && seen_action[2]
		    && seen_action[3], 1, 0);
	diff_eq_int("both trellis states occur",
		    seen_state[0] && seen_state[1], 1, 0);
	diff_eq_int("the metric, not the configuration, picks the action",
		    id_varies, 1, 0);

	return diff_end();
}

/* =========================================== V90SpectralShaper::process === */

#define NFRAMES		12u

static int
run_process(void)
{
	unsigned c, f;
	long tag = 0;
	int out_moved = 0;

	diff_begin("V90SpectralShaper::process");

	for (c = 0; c < NSS; c++) {
		unsigned bl;
		short prev_out[OUT_ENTS];
		int have_prev = 0;

		tag++;
		bl = setup(ss_cases[c].id, ss_cases[c].sr, 0u, (int)tag);

		/*
		 * `reset` leaves the delay line zeroed, which is the object's
		 * own starting state and is the one place a zero seed is
		 * correct rather than finding F230's mistake.
		 */
		for (f = 0; f < NFRAMES; f++) {
			short in[8];
			unsigned char bits[8];
			unsigned i;

			tag++;
			lfsr = 0x2a41u + 0x71c3u * (unsigned)tag;
			for (i = 0; i < 8u; i++) {
				int v = (int)((unsigned)next_byte() << 7)
				    + (int)next_byte() + 0x300;

				in[i] = (short)((i & 1u) ? -v : v);
				bits[i] = (unsigned char)(next_byte() & 1u);
			}
			memset(out_a, OUT_FILL, sizeof(out_a));
			memset(out_b, OUT_FILL, sizeof(out_b));

			our_ss_process(ss_a.raw, in, bits, out_a);
			ref_ss_process(ss_b.raw, in, bits, out_b);

			same_state("after process", tag);
			diff_eq_int("the output frame (%ld)",
				    memcmp(out_a, out_b, sizeof(out_a)) == 0,
				    1, tag);

			/*
			 * ABSOLUTE: nothing past `blockLength` is written, and
			 * `writeIndex` is back where `reset` put it.  The
			 * second of those is INVARIANT on every path -- see
			 * the file comment -- and is checked because it is
			 * cheap, not because it is evidence.
			 */
			for (i = bl; i < OUT_ENTS; i++)
				if (out_b[i] != (short)0x5a5a)
					diff_eq_int("no store past the output "
						    "frame (%ld)", 0, 1, tag);
			diff_eq_int("the blob's writeIndex is unmoved (%ld)",
				    (long)SS_B.writeIndex,
				    (long)(ss_cases[c].id * bl), tag);
			diff_eq_int("the blob's windowLength is unmoved (%ld)",
				    (long)SS_B.windowLength,
				    (long)((ss_cases[c].id + 1u) * bl), tag);

			/*
			 * `primeFrames` counts down once per call and stops at
			 * zero, so it is `shaperId - min(f + 1, shaperId)`.
			 * Computed here rather than copied from the object.
			 */
			diff_eq_int("the blob's primeFrames countdown (%ld)",
				    (long)SS_B.primeFrames,
				    (long)(ss_cases[c].id
					   > (f + 1u)
					   ? ss_cases[c].id - (f + 1u) : 0u),
				    tag);

			/*
			 * SEPARATION on an observable: the output frame moves
			 * from call to call.  A pump that returned the same
			 * thing every time -- a delay line that never shifted,
			 * or a write index that never advanced -- would fail
			 * this even though both sides agreed.
			 */
			if (have_prev) {
				if (memcmp(prev_out, out_b, sizeof(out_b))
				    != 0)
					out_moved++;
			}
			memcpy(prev_out, out_b, sizeof(out_b));
			have_prev = 1;
		}
		teardown(tag);
	}

	diff_eq_int("the output frame moves between calls (%ld of 176)",
		    out_moved > 150, 1, (long)out_moved);

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	dsplibs_debug_level = 0u;
	ref_dsplibs_debug_level = 0u;

	bad |= run_trellis();
	bad |= run_process();

	return bad;
}

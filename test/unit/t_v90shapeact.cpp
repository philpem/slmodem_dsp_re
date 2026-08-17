/*
 * t_v90shapeact.cpp -- differential test of the V.90 spectral shaper's two
 * polarity primitives and the two data tables they are driven from:
 *
 *     V90SpectralShaper::applyFrameAction(ACTIONS, short *, int)   0x328f0
 *     V90SpectralShaper::applyAction(int, short *)                 0x32a10
 *     V90SpectralShaper::actionLookupTable                    .data 0x8e0
 *     pow10Table                                              .data 0xae0
 *
 * WHY THESE TWO ARE TESTED DIRECTLY AND NOT THROUGH `advanceTrellis`.  Both
 * are global `T` symbols with `ref_*` twins, and both are the primitives the
 * trellis search is built out of.  Reached only through the search they are
 * nearly unseparable -- the search runs every candidate and keeps one, so a
 * wrong arm in one of sixteen candidates changes the chosen action only if it
 * changes which candidate wins the metric.  Called directly, four ACTIONS
 * against four digit values with `start` at zero and away from zero separate
 * in one comparison each.  Findings 3509 and 3403 are the reason to insist on
 * that.
 *
 * THE TWO TABLES ARE COMPARED BYTE FOR BYTE against the blob's own objects,
 * which is the only sharp check on 532 bytes of `.data`.  An eyeball against
 * `tabdump.py` proves the transcription of what someone chose to print.
 *
 * WHAT EACH SIDE READS AND WRITES.  Both members take their SOURCE from the
 * object's own `delayLine` and write to a caller-supplied `dst`, so a trial is
 * only meaningful if both sides' delay lines hold the same 24 shorts.  The
 * fixture builds a real shaper on each side, resets it -- `reset` is verified
 * by t_v90shapereset.cpp -- and then writes the SAME varied pattern into both
 * heap buffers.  Never zeros, finding 230: a zero delay line makes every
 * negating arm agree with every non-negating one.
 *
 * EVERY TRIAL STAYS IN RANGE, which is D561's rule.  `applyAction` writes
 * `[0, (shaperId + 1) * blockLength)` and `applyFrameAction`
 * `[start, start + blockLength)`, both buffers hold 24 entries, and the case
 * table below keeps `(shaperId + 1) * blockLength <= 24` and
 * `start + blockLength <= 24`.  `shaperId` is capped at 3 for a second
 * reason: `actionLookupTable` has eight rows and `advanceTrellis` indexes it
 * `2 * shaperId + state`, so 4 would read past the table in the blob as well
 * as here.
 *
 * `dst` IS A GUARDED 32-ENTRY ARRAY pre-filled with a pattern that is not the
 * delay line's, so an arm that writes the right values to the wrong indices,
 * or writes nothing where it should write, is visible as a surviving fill
 * rather than as a coincidence.  The bytes past entry 24 are compared
 * separately.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/DiffCoder.h"
#include "dsplib/V90SpectralShaper.h"
#include "dsplib/V90SpectralShapingFilter.h"

extern "C" {
/* Both sides by asm() label: cdecl, `this` first on the stack (finding 215). */
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

/*
 * The `ACTIONS` argument crosses as `int`: an unscoped enum is passed as a
 * promoted `int` on the stack, so the declaration cannot change what is
 * pushed and this way the test can push a value the enum does not name.
 */
void our_ss_frameact(void *self, int action, short *dst, int start)
	asm("_ZN17V90SpectralShaper16applyFrameActionENS_7ACTIONSEPsi");
void ref_ss_frameact(void *self, int action, short *dst, int start)
	asm("ref__ZN17V90SpectralShaper16applyFrameActionENS_7ACTIONSEPsi");

void our_ss_action(void *self, int action, short *dst)
	asm("_ZN17V90SpectralShaper11applyActionEiPs");
void ref_ss_action(void *self, int action, short *dst)
	asm("ref__ZN17V90SpectralShaper11applyActionEiPs");

extern unsigned int ref_pow10Table[5];
extern unsigned int ref_dsplibs_debug_level;
}

extern int ref_action_table[8][16]
	asm("ref__ZN17V90SpectralShaper17actionLookupTableE");

/*
 * `pow10Table` is a plain global with C++ linkage and no header declares it --
 * nothing outside V90SpectralShaper.cpp uses it, and giving it a header would
 * be inventing an interface the object does not have.  Declared here, with the
 * same linkage as the definition, so the two names agree.
 */
extern unsigned int pow10Table[5];

/* ------------------------------------------------------------- the fixture */

#define SS_SLOT		0x90u	/* 0x6c of object, the rest a guard */
#define SS_BUF		24u	/* entries in each of the two buffers */
#define DST_ENTS	32u	/* 24 usable, the last 8 a guard        */
#define DST_FILL	0x5a	/* not HARNESS_MALLOC_FILL, not zero    */

union big_slot {
	double align;
	unsigned char raw[SS_SLOT];
};

static union big_slot ss_a, ss_b;

#define SS_A (*(V90SpectralShaper *)ss_a.raw)
#define SS_B (*(V90SpectralShaper *)ss_b.raw)

static short dst_a[DST_ENTS], dst_b[DST_ENTS];
static short line[SS_BUF];

static unsigned lfsr;

static unsigned char
next_byte(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)(lfsr >> 3);
}

/* The same varied bytes into both sides.  Never zeros -- finding 230. */
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
 * Four coefficients that are a real filter rather than zeros.  Neither member
 * under test reads them -- `applyAction` and `applyFrameAction` touch only
 * `shaperId`, `blockLength` and `delayLine` -- but `reset` stores them, and a
 * zero row would make the object compare equal for the wrong reason if that
 * ever stopped being true.
 */
#define C_A1	0x3f400000u	/* 0.75f  */
#define C_A2	0xbe800000u	/* -0.25f */
#define C_B1	0x3f000000u	/* 0.5f   */
#define C_B2	0x3ecccccdu	/* 0.4f   */

/*
 * A delay line with no zero in it, no repeated value, both signs, and both
 * 0x8000 and 0x7fff -- so a negate that saturated or that dropped the top bit
 * would be visible, and every arm's index pattern is distinguishable from
 * every other's.
 */
static void
seed_line(int trial)
{
	unsigned i;

	lfsr = 0x1357u + 0x4f1bu * (unsigned)trial;
	for (i = 0; i < SS_BUF; i++) {
		int v = (int)((unsigned)next_byte() << 8) | (int)next_byte();

		if (v == 0)
			v = 0x1234 + (int)i;
		line[i] = (short)v;
	}
	line[3] = (short)0x8000;	/* -32768, whose negation overflows */
	line[7] = 0x7fff;
	line[11] = -1;
	line[13] = 1;
}

/*
 * THE THREE POINTERS ARE SKIPPED, AND SKIPPING THEM IS NOT FREE.  Both sides
 * allocate their own buffers and always will, so +0x28, +0x2c and +0x3c differ
 * in every trial and a raw comparison of the two objects reports the first of
 * them and stops -- `diff_eq_obj` caps its output on the ground that everything
 * after the first difference is consequence, so ONE report is consistent with
 * three differing fields and is not evidence of one.
 *
 * CLAUDE.md licenses the skip ("two heap pointers hold two different addresses
 * and always will").  What it does not license is losing the property: reduced
 * to "is it null", a pointer that had been overwritten with the OTHER buffer's
 * address still compares equal, and the check has quietly stopped measuring.
 * So `ptr_check` below asserts, per side and against that side's OWN pre-call
 * values, that all three are byte-for-byte unchanged, non-null and distinct.
 * Neither member under test may touch them at all, so unchanged is the whole
 * property and there is nothing weaker to fall back on.
 */
struct ptr_set {
	short		*delay;
	short		*trial;
	unsigned char	*pde;
};

static struct ptr_set ptr_pre_a, ptr_pre_b;

static void
ptr_save(void)
{
	ptr_pre_a.delay = SS_A.delayLine;
	ptr_pre_a.trial = SS_A.trialLine;
	ptr_pre_a.pde = SS_A.pde.state_;
	ptr_pre_b.delay = SS_B.delayLine;
	ptr_pre_b.trial = SS_B.trialLine;
	ptr_pre_b.pde = SS_B.pde.state_;
}

static void
ptr_check(long tag)
{
	diff_eq_int("our three pointers are unmoved, non-null and distinct "
		    "(%ld)",
		    SS_A.delayLine == ptr_pre_a.delay
		    && SS_A.trialLine == ptr_pre_a.trial
		    && SS_A.pde.state_ == ptr_pre_a.pde
		    && SS_A.delayLine != 0 && SS_A.trialLine != 0
		    && SS_A.pde.state_ != 0
		    && SS_A.delayLine != SS_A.trialLine, 1, tag);
	diff_eq_int("the blob's three pointers likewise (%ld)",
		    SS_B.delayLine == ptr_pre_b.delay
		    && SS_B.trialLine == ptr_pre_b.trial
		    && SS_B.pde.state_ == ptr_pre_b.pde
		    && SS_B.delayLine != 0 && SS_B.trialLine != 0
		    && SS_B.pde.state_ != 0
		    && SS_B.delayLine != SS_B.trialLine, 1, tag);
}

/*
 * Build both shapers, reset them with the same arguments, and give both the
 * same delay line and the same destination fill.  Returns `blockLength`.
 */
static unsigned
setup(unsigned id, unsigned sr, int trial)
{
	harness_alloc_reset();
	fill_pair(ss_a.raw, ss_b.raw, SS_SLOT, trial);

	our_ss_ctor(ss_a.raw);
	ref_ss_ctor(ss_b.raw);
	our_ss_reset(ss_a.raw, id, sr, C_A1, C_A2, C_B1, C_B2);
	ref_ss_reset(ss_b.raw, id, sr, C_A1, C_A2, C_B1, C_B2);

	seed_line(trial);
	memcpy(SS_A.delayLine, line, SS_BUF * sizeof(short));
	memcpy(SS_B.delayLine, line, SS_BUF * sizeof(short));

	memset(dst_a, DST_FILL, sizeof(dst_a));
	memset(dst_b, DST_FILL, sizeof(dst_b));

	ptr_save();
	return SS_B.blockLength;
}

/*
 * Everything but those three, compared field for field.  The pointer slots are
 * reduced to a flag only so `diff_eq_obj` can compare the rest in one call;
 * `ptr_check` is what carries their evidence.
 */
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

/* Both objects, pointers normalised, compared field for field. */
static void
same_object(const char *what, long tag)
{
	unsigned char sa[sizeof(V90SpectralShaper)];
	unsigned char sb[sizeof(V90SpectralShaper)];

	ss_snapshot(sa, ss_a.raw);
	ss_snapshot(sb, ss_b.raw);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90SpectralShaper", sa, sb,
		     sizeof(V90SpectralShaper), tag);
}

static void
teardown(void)
{
	our_ss_dtor(ss_a.raw);
	ref_ss_dtor(ss_b.raw);
}

/* What one arm is supposed to do to one sample -- the absolute reference. */
static short
expect(int action, unsigned i, unsigned start)
{
	int v = line[i];

	switch (action) {
	case V90SpectralShaper::V90SS_KEEP_ALL:
		break;
	case V90SpectralShaper::V90SS_NEGATE_ALL:
		v = -v;
		break;
	case V90SpectralShaper::V90SS_NEGATE_EVEN:
		if (((i - start) & 1u) == 0u)
			v = -v;
		break;
	case V90SpectralShaper::V90SS_NEGATE_ODD:
		if (((i - start) & 1u) != 0u)
			v = -v;
		break;
	default:
		return (short)0x5a5a;	/* the fill: nothing written */
	}
	return (short)v;
}

/* --------------------------------------------------------- the case tables */

struct ss_case {
	unsigned id;
	unsigned sr;
};

/*
 * `blockLength` is `6 / sr`, so these give widths 6, 3, 2 and 1 against
 * `shaperId` 0..3 -- every combination the object can be in with
 * `(id + 1) * blockLength <= 24`, which is all sixteen of them.
 */
static const struct ss_case ss_cases[] = {
	{ 0u, 1u }, { 1u, 1u }, { 2u, 1u }, { 3u, 1u },
	{ 0u, 2u }, { 1u, 2u }, { 2u, 2u }, { 3u, 2u },
	{ 0u, 3u }, { 1u, 3u }, { 2u, 3u }, { 3u, 3u },
	{ 0u, 6u }, { 1u, 6u }, { 2u, 6u }, { 3u, 6u }
};

#define NSS (sizeof(ss_cases) / sizeof(ss_cases[0]))

/*
 * The four the enum names, then four it does not.  0 is `KEEP_ALL` and is in
 * the list twice over; -1 and 5 fall into the default arm from opposite sides
 * of the comparison tree (`jle` at 0x32923 takes the negative one), and 0x100
 * is there because the switch tests the whole word and not its low bits.
 */
static const int frame_actions[] = { 0, 1, 2, 3, -1, 5, 0x100, 0x7fffffff };

#define NFA (sizeof(frame_actions) / sizeof(frame_actions[0]))

/* ================================ V90SpectralShaper::applyFrameAction ==== */

static int
run_frameact(void)
{
	unsigned c, a, s;
	long tag = 0;
	int sep_action = 0, sep_start = 0;

	diff_begin("V90SpectralShaper::applyFrameAction");

	for (c = 0; c < NSS; c++) {
		for (a = 0; a < NFA; a++) {
			/*
			 * `start` at zero is the case `advanceTrellis` uses,
			 * where the `i - start` subtraction folds away; the
			 * odd values are what prove the parity is counted
			 * from the FRAME and not from the buffer, which is
			 * the one thing arms 2 and 3 could get wrong without
			 * start-zero trials noticing.
			 */
			static const unsigned starts[] = { 0u, 1u, 2u, 3u, 7u };
			short first[DST_ENTS];
			int have_first = 0;

			for (s = 0; s < sizeof(starts) / sizeof(starts[0]);
			     s++) {
				unsigned bl, start, i;
				int all_ok = 1;

				tag++;
				bl = setup(ss_cases[c].id, ss_cases[c].sr,
					   (int)tag);
				start = starts[s];
				if (start + bl > SS_BUF) {
					teardown();
					continue;
				}

				our_ss_frameact(ss_a.raw, frame_actions[a],
						dst_a, (int)start);
				ref_ss_frameact(ss_b.raw, frame_actions[a],
						dst_b, (int)start);

				diff_eq_int("destination (%ld)",
					    memcmp(dst_a, dst_b,
						   sizeof(dst_a)) == 0, 1,
					    tag);
				same_object("the object is untouched", tag);
				ptr_check(tag);
				diff_eq_int("no store past the object (%ld)",
					    memcmp(ss_a.raw + SS_SLOT / 2,
						   ss_b.raw + SS_SLOT / 2,
						   SS_SLOT - SS_SLOT / 2)
					    == 0, 1, tag);
				diff_eq_int("the delay line is read-only "
					    "(%ld)",
					    memcmp(SS_B.delayLine, line,
						   SS_BUF * sizeof(short))
					    == 0, 1, tag);

				/*
				 * ABSOLUTE, because two sides that both wrote
				 * nothing compare equal (findings 223, 224).
				 * Inside the frame the value is the arm's;
				 * outside it, and past entry 24, the fill.
				 */
				for (i = 0; i < DST_ENTS; i++) {
					short want;

					if (i >= start && i < start + bl)
						want = expect(frame_actions[a],
							      i, start);
					else
						want = (short)0x5a5a;
					if (dst_b[i] != want)
						all_ok = 0;
				}
				diff_eq_int("the blob writes the frame and "
					    "nothing else (%ld)", all_ok, 1,
					    tag);

				/*
				 * SEPARATION, and it is counted on an
				 * OBSERVABLE: the destination buffer differing
				 * between two `start` values under one action.
				 */
				if (!have_first) {
					memcpy(first, dst_b, sizeof(first));
					have_first = 1;
				} else if (memcmp(first, dst_b,
						  sizeof(first)) != 0) {
					sep_start++;
				}

				teardown();
				diff_eq_int("no leak, no bad free (%ld)",
					    harness_alloc.live == 0
					    && harness_alloc.bad_free == 0, 1,
					    tag);
			}
		}
	}

	/*
	 * And the action argument itself: for one case and one start, do the
	 * four named arms give four different buffers?  If they do not, every
	 * trial above was measuring the same arm four times.
	 */
	{
		short seen[4][DST_ENTS];
		unsigned a2, b2;

		for (a2 = 0; a2 < 4u; a2++) {
			setup(3u, 1u, 4242);
			ref_ss_frameact(ss_b.raw, (int)a2, dst_b, 0);
			memcpy(seen[a2], dst_b, sizeof(dst_b));
			teardown();
		}
		for (a2 = 0; a2 < 4u; a2++)
			for (b2 = a2 + 1u; b2 < 4u; b2++)
				if (memcmp(seen[a2], seen[b2],
					   sizeof(seen[0])) != 0)
					sep_action++;
	}

	diff_eq_int("the four arms are four different results", sep_action, 6,
		    0);
	diff_eq_int("`start` moves the result", sep_start > 0, 1, 0);

	return diff_end();
}

/* ===================================== V90SpectralShaper::applyAction ==== */

/*
 * `applyAction`'s digits are consumed least significant first, digit `m`
 * driving the frame at `(shaperId - m) * blockLength`.  This is the same
 * arithmetic written out independently, so a reconstruction that walked the
 * digits the other way round -- which is the single most likely way to get
 * this function wrong -- disagrees with it.
 */
static void
expect_action(int action, unsigned id, unsigned bl, short *want)
{
	unsigned m, i;

	for (i = 0; i < DST_ENTS; i++)
		want[i] = (short)0x5a5a;

	for (m = 0; m <= id; m++) {
		int digit = action % 10;
		unsigned start = (id - m) * bl;

		action /= 10;
		if (digit < 1 || digit > 4)
			continue;
		for (i = start; i < start + bl; i++)
			want[i] = expect(digit - 1, i, start);
	}
}

static int
run_action(void)
{
	unsigned c, k;
	long tag = 0;
	int sep_cand = 0;

	diff_begin("V90SpectralShaper::applyAction");

	for (c = 0; c < NSS; c++) {
		unsigned row = 2u * ss_cases[c].id;
		short first[DST_ENTS];
		int have_first = 0;

		/*
		 * Sixteen candidates from the table's own row, then five the
		 * table never holds: 0 (every digit the default arm), a code
		 * with an interior zero digit, a code with a 5 in it, a
		 * NEGATIVE code -- where `% 10` is negative and the default
		 * arm takes every digit -- and a code with more digits than
		 * `shaperId + 1`, whose extra digits are never reached.
		 */
		for (k = 0; k < 16u + 5u; k++) {
			unsigned bl, i;
			int action;
			short want[DST_ENTS];
			int all_ok = 1;

			tag++;
			bl = setup(ss_cases[c].id, ss_cases[c].sr, (int)tag);

			if (k < 16u)
				action = ref_action_table[row][k];
			else if (k == 16u)
				action = 0;
			else if (k == 17u)
				action = 1041;
			else if (k == 18u)
				action = 5253;
			else if (k == 19u)
				action = -1234;
			else
				action = 12341234;

			our_ss_action(ss_a.raw, action, dst_a);
			ref_ss_action(ss_b.raw, action, dst_b);

			diff_eq_int("destination (%ld)",
				    memcmp(dst_a, dst_b, sizeof(dst_a)) == 0,
				    1, tag);
			same_object("the object is untouched", tag);
			ptr_check(tag);
			diff_eq_int("no store past the object (%ld)",
				    memcmp(ss_a.raw + SS_SLOT / 2,
					   ss_b.raw + SS_SLOT / 2,
					   SS_SLOT - SS_SLOT / 2) == 0, 1,
				    tag);
			diff_eq_int("the delay line is read-only (%ld)",
				    memcmp(SS_B.delayLine, line,
					   SS_BUF * sizeof(short)) == 0, 1,
				    tag);

			expect_action(action, ss_cases[c].id, bl, want);
			for (i = 0; i < DST_ENTS; i++)
				if (dst_b[i] != want[i])
					all_ok = 0;
			diff_eq_int("the blob's digit order and frame offsets "
				    "(%ld)", all_ok, 1, tag);

			if (!have_first) {
				memcpy(first, dst_b, sizeof(first));
				have_first = 1;
			} else if (memcmp(first, dst_b, sizeof(first)) != 0) {
				sep_cand++;
			}

			teardown();
			diff_eq_int("no leak, no bad free (%ld)",
				    harness_alloc.live == 0
				    && harness_alloc.bad_free == 0, 1, tag);
		}
	}

	/*
	 * 16 cases, 20 comparisons against the first candidate each; the
	 * count is only meaningful as "the candidate changes the output most
	 * of the time", so the bound is deliberately loose and the MUTATION
	 * is what adjudicates (finding 3509).
	 */
	diff_eq_int("the candidate moves the output (%ld)", sep_cand > 200, 1,
		    (long)sep_cand);

	return diff_end();
}

/* ================================================== the two data tables === */

static int
run_tables(void)
{
	unsigned r, k;
	int nonzero = 0;

	diff_begin("V90SpectralShaper's two data tables");

	diff_eq_int("actionLookupTable, all 512 bytes",
		    memcmp(V90SpectralShaper::actionLookupTable,
			   ref_action_table,
			   sizeof(ref_action_table)) == 0, 1, 0);
	diff_eq_int("pow10Table, all 20 bytes",
		    memcmp(pow10Table, ref_pow10Table,
			   sizeof(ref_pow10Table)) == 0, 1, 0);

	/*
	 * ABSOLUTE, and against the rule the header states rather than against
	 * the bytes: row `2 * id + state` holds 2^(id+1) candidates whose
	 * digits are `1 + 2 * previous + current`, and zeros after them.  A
	 * transcription error that a memcmp of two copies of the same mistake
	 * would miss cannot survive this.
	 */
	for (r = 0; r < 8u; r++) {
		unsigned id = r / 2u;
		unsigned st = r & 1u;
		unsigned n = 1u << (id + 1u);

		for (k = 0; k < 16u; k++) {
			int want = 0;

			if (k < n) {
				unsigned prev = st;
				unsigned d;

				for (d = 0; d <= id; d++) {
					unsigned bit =
					    (k >> (id - d)) & 1u;

					want = want * 10
					    + (int)(1u + 2u * prev + bit);
					prev = bit;
				}
			}
			if (want != 0)
				nonzero++;
			diff_eq_int("actionLookupTable rule (%ld)",
				    (long)ref_action_table[r][k], (long)want,
				    (long)(r * 16u + k));
		}
	}
	diff_eq_int("the rule reproduced %ld non-zero entries", nonzero, 60,
		    (long)nonzero);

	for (k = 0; k < 5u; k++) {
		unsigned want = 1u;
		unsigned d;

		for (d = 0; d < k; d++)
			want *= 10u;
		diff_eq_int("pow10Table[%ld]", (long)ref_pow10Table[k],
			    (long)want, (long)k);
	}

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	dsplibs_debug_level = 0u;
	ref_dsplibs_debug_level = 0u;

	bad |= run_tables();
	bad |= run_frameact();
	bad |= run_action();

	return bad;
}

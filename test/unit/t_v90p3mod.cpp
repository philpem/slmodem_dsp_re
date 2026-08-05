/*
 * t_v90p3mod.cpp -- differential test of V90Phase3Modulator.
 *
 * The fixture is t_v90jd.cpp's, with the two differences this class forces:
 *
 * THE OBJECT IS SEEDED WITH VARIED BYTES, NEVER ZEROED.  Both sides get the
 * same pseudorandom fill before every call, reseeded each trial, so a clear
 * loop that stops one element short is visible and a field neither side
 * writes cannot pass by accident (findings 223, 224).
 *
 * BUT `pcmType` IS ALWAYS FORCED TO 0 OR 1.  `resetDILGenerator` indexes
 * `codeSegmentsBoundriesLookupTable` at `8 * pcmType`, and the table is
 * sixteen ints, so a random 32-bit `pcmType` would read wildly out of bounds
 * -- out of OUR table on our side and out of the REFERENCE table on theirs,
 * which are different objects at different addresses.  That is not a test of
 * anything.  Both values are exercised, and each is exercised against every
 * seed mode.
 *
 * THE OBJECT IS COMPARED WHOLE, AND SO IS A GUARD PAST ITS END.
 * `diff_eq_obj` covers `sizeof(V90Phase3Modulator)` = 920; the bytes from
 * there to the end of an over-large slot are compared separately, so a store
 * that overruns the object fails rather than passing in silence.  920 is the
 * largest displacement, +0x394, plus the width of the byte stored there,
 * rounded up for the four-byte members -- finding 229's rule.
 *
 * `resetDILGenerator` touches no pointer field, which is why the whole-object
 * comparison works here without the skip-and-compare-offsets form that
 * `reset` needs.
 *
 * The `ref_` aliases are reached through asm() labels rather than by spelling
 * the alias as an identifier, which sidesteps finding 225 entirely.  The
 * convention is plain cdecl with `this` as the first stack argument (finding
 * 215); both symbols are `T` in the blob, so no regparm is involved.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V90Phase3Modulator.h"

extern "C" {
void ref_setSessionFlag(void *self, unsigned int flag)
	asm("ref__ZN18V90Phase3Modulator14setSessionFlagEj");
void ref_resetDILGenerator(void *self, const void *d)
	asm("ref__ZN18V90Phase3Modulator17resetDILGeneratorEPK19tagV90DILdescriptor");

/*
 * The return type is not mangled and the header derives it as `int` from the
 * object's own widening; declaring the alias `int` here is what makes the
 * comparison cover all thirty-two bits rather than the low sixteen.
 */
int ref_generateV90Symbol(void *self)
	asm("ref__ZN18V90Phase3Modulator17generateV90SymbolEv");
int ref_generateV92Symbol(void *self)
	asm("ref__ZN18V90Phase3Modulator17generateV92SymbolEv");

/*
 * The four weak Scrambler members.  They are `W` in the blob, not `T`, and
 * symmap.py renames them anyway -- checked with
 * `nm build/dsplibs_ref.o | grep ScramblerIhiE` -- so they can be driven
 * directly rather than only through the modulator.  `process` is declared
 * returning `int` for the same reason as above: the header says the return
 * type is `T`, one byte, and that is a claim this test checks.
 */
int ref_scr_process(void *self, unsigned char in)
	asm("ref__ZN9ScramblerIhiE7processEh");
void ref_scr_reset(void *self, unsigned char v)
	asm("ref__ZN9ScramblerIhiE5resetEh");
void ref_scr_resetHistoryIndexes(void *self)
	asm("ref__ZN9ScramblerIhiE19resetHistoryIndexesEv");
void ref_scr_copyHistoryTail(void *self)
	asm("ref__ZN9ScramblerIhiE15copyHistoryTailEv");

extern unsigned int ref_dsplibs_debug_level;
}

#include "dsplib/debug.h"

/* The object, plus room past its end to catch a store that overruns it. */
#define SLOT 1024

union mod_slot {
	V90Phase3Modulator o;
	unsigned char raw[SLOT];
};

static union mod_slot ours, theirs;
static tagV90DILdescriptor desc;

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/*
 * `mode` picks how varied the fill is.  The DIL expansion masks every code
 * with 0x7f before companding, so a fill whose high bit is constant and one
 * whose low seven bits are constant probe different halves of the path; the
 * all-0xa5 case is the harness's own malloc fill, which is what a field
 * neither side writes would look like if the object were not seeded at all.
 */
static void
seed(int trial, int mode)
{
	int i;

	lfsr_state = 0x1234u + 0x9e37u * (unsigned)trial + (unsigned)mode;

	for (i = 0; i < SLOT; i++) {
		unsigned char v;

		switch (mode) {
		case 0:
			v = next_byte();
			break;
		case 1:
			v = 0xa5;
			break;
		case 2:
			v = (unsigned char)(next_byte() | 0x80);
			break;
		default:
			v = (unsigned char)(next_byte() & 0x7f);
			break;
		}
		ours.raw[i] = v;
		theirs.raw[i] = v;
	}
}

static void
seed_descriptor(int trial, int mode)
{
	unsigned char *p = (unsigned char *)&desc;
	unsigned int i;

	lfsr_state = 0x5eedu + 0x4f1bu * (unsigned)trial + (unsigned)mode;

	for (i = 0; i < sizeof(desc); i++) {
		switch (mode) {
		case 1:
			p[i] = 0;
			break;
		case 2:
			p[i] = 0xff;
			break;
		default:
			p[i] = next_byte();
			break;
		}
	}

	/*
	 * The two sequence lengths are kept inside their 128-byte
	 * destinations.  The object clamps neither, so a larger value would
	 * still compare equal on both sides -- but it would be testing the
	 * overrun rather than the copy, and `dilCount` below is the field that
	 * exercises a long loop.
	 */
	desc.seq1Length = (unsigned char)((trial * 13u + 1u) % 129u);
	desc.seq2Length = (unsigned char)((trial * 29u + 7u) % 129u);
	desc.dilCount = (unsigned char)(trial * 37u + 1u);
}

static int
guard_equal(void)
{
	return memcmp(ours.raw + sizeof(V90Phase3Modulator),
		      theirs.raw + sizeof(V90Phase3Modulator),
		      SLOT - sizeof(V90Phase3Modulator)) == 0;
}

#define NTRIAL 32

static int
run_setsessionflag(void)
{
	int trial, moved = 0;

	diff_begin("V90Phase3Modulator::setSessionFlag");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned int flag = 0x51a70000u + (unsigned)trial;
		unsigned char before[SLOT];

		seed(trial, trial % 4);
		memcpy(before, ours.raw, SLOT);

		ours.o.setSessionFlag(flag);
		ref_setSessionFlag(&theirs.o, flag);

		diff_eq_obj("after setSessionFlag", V90Phase3Modulator,
			    &ours.o, &theirs.o, trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("setSessionFlag stored the flag (trial %ld)",
			    ours.o.sessionFlag, (long)flag, trial);
		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
	}

	diff_eq_int("setSessionFlag changed the object", moved, 1, 0);

	return diff_end();
}

static int
run_resetdilgenerator(void)
{
	unsigned char first[SLOT];
	int trial, moved = 0, distinct = 0;
	int saw_index7 = 0, saw_index8 = 0, saw_negative = 0;

	diff_begin("V90Phase3Modulator::resetDILGenerator");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		PcmType law = (trial & 1) ? PCM_TYPE_A_LAW : PCM_TYPE_MU_LAW;

		seed(trial, trial % 4);
		seed_descriptor(trial, trial % 3);

		/* See the file comment: this one field cannot be random. */
		ours.o.pcmType = theirs.o.pcmType = law;
		memcpy(before, ours.raw, SLOT);

		ours.o.resetDILGenerator(&desc);
		ref_resetDILGenerator(&theirs.o, &desc);

		diff_eq_obj("after resetDILGenerator", V90Phase3Modulator,
			    &ours.o, &theirs.o, trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("resetDILGenerator copied dilCount (trial %ld)",
			    ours.o.dilCount, desc.dilCount, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, ours.raw, SLOT);
		else if (memcmp(first, ours.raw, SLOT) != 0)
			distinct = 1;
	}

	/*
	 * Every PCM code, both laws, one DIL entry.  The random descriptors
	 * above leave two things untested, and mutation showed it: the
	 * segment search never reached its last row entry, and it never saw a
	 * level the object reads back as more than 0x7fff.  Sweeping the code
	 * that produces `dilLevel[0]` reaches both -- the level is a signed
	 * short and the search zero-extends it, so half the sweep is above
	 * every boundary and lands on the index one past the end of the row.
	 */
	for (trial = 0; trial < 512; trial++) {
		PcmType law = (trial & 0x100) ? PCM_TYPE_A_LAW
					      : PCM_TYPE_MU_LAW;

		seed(trial, trial % 4);
		seed_descriptor(trial, 0);
		ours.o.pcmType = theirs.o.pcmType = law;
		desc.dilCount = 1;
		desc.dilCode[0] = (unsigned char)(trial & 0xff);

		ours.o.resetDILGenerator(&desc);
		ref_resetDILGenerator(&theirs.o, &desc);

		diff_eq_obj("after resetDILGenerator, code sweep",
			    V90Phase3Modulator, &ours.o, &theirs.o, trial);
		diff_eq_int("no store past the object, sweep (trial %ld)",
			    guard_equal(), 1, trial);
		if (ours.o.segmentIndex == 8)
			saw_index8 = 1;
		if (ours.o.segmentIndex == 7)
			saw_index7 = 1;
		if (ours.o.dilLevel[0] < 0)
			saw_negative = 1;
	}

	/*
	 * `dilCount` = 0, with `dilLevel[0]` set directly.  The sweep above
	 * cannot make the segment search see a large or negative level,
	 * because every code the expansion produces has its top bit set and
	 * this library's companding puts that half above zero -- so
	 * `dilLevel[0]` after a non-empty expansion is always in 0..0x7fff and
	 * always inside the first seven boundaries.
	 *
	 * With no DIL entries the field is not written, and the search reads
	 * back whatever was already there.  That is the only path on which the
	 * zero-extension is observable, and it is the path that reaches the
	 * index one past the end of the row.  Both sides get the same value,
	 * so this is still a comparison and not a fixture of our own making.
	 */
	for (trial = 0; trial < 2 * 34; trial++) {
		static const unsigned short probe[34] = {
			0, 1, 123, 124, 125, 255, 256, 257, 379, 380, 381,
			511, 512, 513, 891, 892, 893, 1915, 1916, 1917,
			3963, 3964, 3965, 8059, 8060, 8061, 16251, 16252,
			16253, 32635, 32636, 32637, 32768, 65535
		};
		PcmType law = (trial >= 34) ? PCM_TYPE_A_LAW
					    : PCM_TYPE_MU_LAW;
		short level = (short)probe[trial % 34];

		seed(trial, trial % 4);
		seed_descriptor(trial, 0);
		ours.o.pcmType = theirs.o.pcmType = law;
		desc.dilCount = 0;
		ours.o.dilLevel[0] = theirs.o.dilLevel[0] = level;

		ours.o.resetDILGenerator(&desc);
		ref_resetDILGenerator(&theirs.o, &desc);

		diff_eq_obj("after resetDILGenerator, level probe",
			    V90Phase3Modulator, &ours.o, &theirs.o, trial);
		diff_eq_int("no store past the object, probe (trial %ld)",
			    guard_equal(), 1, trial);
		if (ours.o.segmentIndex == 8)
			saw_index8 = 1;
		if (ours.o.segmentIndex == 7)
			saw_index7 = 1;
		if (ours.o.dilLevel[0] < 0)
			saw_negative = 1;
	}

	/* The null descriptor path, which is the whole of the error handling. */
	for (trial = 0; trial < 4; trial++) {
		seed(trial + NTRIAL, trial % 4);
		ours.o.pcmType = theirs.o.pcmType =
		    (trial & 1) ? PCM_TYPE_A_LAW : PCM_TYPE_MU_LAW;
		ours.o.dilCount = theirs.o.dilCount =
		    (unsigned char)(trial | 0x40);

		ours.o.resetDILGenerator(NULL);
		ref_resetDILGenerator(&theirs.o, NULL);

		diff_eq_obj("after resetDILGenerator(NULL)",
			    V90Phase3Modulator, &ours.o, &theirs.o, trial);
		diff_eq_int("no store past the object, NULL (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("resetDILGenerator(NULL) cleared dilCount "
			    "(trial %ld)", ours.o.dilCount, 0, trial);
	}

	diff_eq_int("resetDILGenerator changed the object", moved, 1, 0);
	diff_eq_int("resetDILGenerator is not the same on every trial",
		    distinct, 1, 0);

	/*
	 * Anti-vacuity for the segment search specifically: the sweep
	 * must actually have reached the last row entry, the index one
	 * past the end, and a level the object reads back above 0x7fff.
	 * Without these three the two mutations "row bound 7 -> 6" and
	 * "read the level signed" both survive.
	 */
	diff_eq_int("the sweep reached segment index 7", saw_index7, 1, 0);
	diff_eq_int("the sweep reached segment index 8", saw_index8, 1, 0);
	diff_eq_int("the sweep produced a negative level", saw_negative,
		    1, 0);

	return diff_end();
}

/*
 * The static table.  It is a defined data symbol in the blob and therefore
 * renamed, so both copies exist and can be compared element by element -- the
 * one thing in this file that is a comparison of data rather than behaviour,
 * and the reason the table above is an extraction rather than a generator.
 */
extern "C" int ref_codeSegmentsBoundries[2][8]
	asm("ref__ZN18V90Phase3Modulator32codeSegmentsBoundriesLookupTableE");

static int
run_table(void)
{
	int law, seg;

	diff_begin("V90Phase3Modulator::codeSegmentsBoundriesLookupTable");

	for (law = 0; law < 2; law++)
		for (seg = 0; seg < 8; seg++)
			diff_eq_int("table[%ld]",
			    V90Phase3Modulator::
				codeSegmentsBoundriesLookupTable[law][seg],
			    ref_codeSegmentsBoundries[law][seg],
			    law * 8 + seg);

	return diff_end();
}

/*
 * ===========================================================================
 * Scrambler<unsigned char, int>
 *
 * Four weak template members that nothing had ever called.  They are a
 * subobject of the modulator at +0x20 and both `generate*Symbol` drive them,
 * so they could be checked only through those -- but driving them directly is
 * sharper and costs one fixture, so it is done both ways.
 *
 * THE SEVEN POINTERS MUST BE VALID AND CONGRUENT.  They are dereferenced;
 * random bytes there segfault.  Each side gets its own buffer of the same
 * size with the same relative layout, so `pOut - buf` is comparable even
 * though `pOut` never is.  The layout below is a 64-byte buffer with the
 * output cursor starting 40 bytes above the limit and taps 5 and 23 bytes
 * above the cursor: 41 symbols reach the restart, so a run of 100 crosses it
 * twice and `copyHistoryTail` -- a third of the class, and invisible
 * otherwise -- actually executes.
 * ===========================================================================
 */

#define SCR_BUF		64u
#define SCR_OUT		40u
#define SCR_TAP1	45u
#define SCR_TAP2	63u
#define SCR_TAIL	23u

typedef Scrambler<unsigned char, int> ScramblerHI;

static unsigned char scr_ours[SCR_BUF], scr_theirs[SCR_BUF];

static void
scr_place(ScramblerHI *s, unsigned char *buf, unsigned int out)
{
	s->pLimit = buf;
	s->pInitOut = buf + SCR_OUT;
	s->pInitTap1 = buf + SCR_TAP1;
	s->pInitTap2 = buf + SCR_TAP2;
	s->pOut = buf + out;
	s->pTap1 = buf + out + (SCR_TAP1 - SCR_OUT);
	s->pTap2 = buf + out + (SCR_TAP2 - SCR_OUT);
	s->tailLength = SCR_TAIL;
}

/*
 * Every field of both objects, as each side's own offset into its own buffer,
 * plus the buffers themselves.  The raw pointers are never compared and never
 * merely checked non-null: two heap-free static arrays at two addresses would
 * pass that and prove nothing (finding 224).
 */
static void
scr_compare(const ScramblerHI *a, const ScramblerHI *b, long input)
{
	diff_eq_int("scrambler pLimit offset (case %ld)",
		    a->pLimit - scr_ours, b->pLimit - scr_theirs, input);
	diff_eq_int("scrambler pInitOut offset (case %ld)",
		    a->pInitOut - scr_ours, b->pInitOut - scr_theirs, input);
	diff_eq_int("scrambler pInitTap1 offset (case %ld)",
		    a->pInitTap1 - scr_ours, b->pInitTap1 - scr_theirs, input);
	diff_eq_int("scrambler pInitTap2 offset (case %ld)",
		    a->pInitTap2 - scr_ours, b->pInitTap2 - scr_theirs, input);
	diff_eq_int("scrambler pOut offset (case %ld)",
		    a->pOut - scr_ours, b->pOut - scr_theirs, input);
	diff_eq_int("scrambler pTap1 offset (case %ld)",
		    a->pTap1 - scr_ours, b->pTap1 - scr_theirs, input);
	diff_eq_int("scrambler pTap2 offset (case %ld)",
		    a->pTap2 - scr_ours, b->pTap2 - scr_theirs, input);
	diff_eq_int("scrambler tailLength (case %ld)",
		    a->tailLength, b->tailLength, input);
	diff_eq_int("scrambler buffer (case %ld)",
		    memcmp(scr_ours, scr_theirs, SCR_BUF) == 0, 1, input);
}

static void
scr_seed(int trial, int mode)
{
	unsigned int i;

	lfsr_state = 0x7c1bu + 0x2545u * (unsigned)trial + (unsigned)mode;
	for (i = 0; i < SCR_BUF; i++) {
		unsigned char v;

		switch (mode) {
		case 0:  v = next_byte();			break;
		case 1:  v = 0;					break;
		case 2:  v = 1;					break;
		default: v = (unsigned char)(next_byte() & 1u);	break;
		}
		scr_ours[i] = scr_theirs[i] = v;
	}
}

static int
run_scrambler(void)
{
	ScramblerHI a, b;
	int trial, mode, i, restarts = 0, differed = 0, wide = 0;

	diff_begin("Scrambler<unsigned char, int>");

	/* process(), run long enough to cross the restart twice. */
	for (mode = 0; mode < 4; mode++) {
		for (trial = 0; trial < 8; trial++) {
			unsigned int start = (unsigned)trial * 5u;

			scr_seed(trial, mode);
			scr_place(&a, scr_ours, start);
			scr_place(&b, scr_theirs, start);

			for (i = 0; i < 100; i++) {
				unsigned char in = (unsigned char)next_byte();
				unsigned char *before = a.pOut;
				int ra, rb;

				ra = a.process(in);
				rb = ref_scr_process(&b, in);

				diff_eq_int("process returned (case %ld)",
					    ra, rb,
					    (long)(mode * 10000 + trial * 100
						   + i));
				diff_eq_int("process returned a byte "
					    "(case %ld)", (rb & ~0xff) == 0, 1,
					    (long)(mode * 10000 + trial * 100
						   + i));
				scr_compare(&a, &b,
				    (long)(mode * 10000 + trial * 100 + i));

				if (before == scr_ours &&
				    a.pOut == a.pInitOut)
					restarts++;
				if (ra != 0)
					differed = 1;
				if ((rb & ~0xff) != 0)
					wide = 1;
			}
		}
	}

	/* reset(), whose masking of everything but bit 0 is the whole claim. */
	for (mode = 0; mode < 4; mode++) {
		for (trial = 0; trial < 256; trial++) {
			scr_seed(trial, mode);
			scr_place(&a, scr_ours, (unsigned)trial % 41u);
			scr_place(&b, scr_theirs, (unsigned)trial % 41u);

			a.reset((unsigned char)trial);
			ref_scr_reset(&b, (unsigned char)trial);

			scr_compare(&a, &b, (long)(mode * 1000 + trial));
		}
	}

	/* The other two, called on their own so neither hides in the third. */
	for (trial = 0; trial < 16; trial++) {
		scr_seed(trial, trial % 4);
		scr_place(&a, scr_ours, (unsigned)trial % 41u);
		scr_place(&b, scr_theirs, (unsigned)trial % 41u);
		a.resetHistoryIndexes();
		ref_scr_resetHistoryIndexes(&b);
		scr_compare(&a, &b, trial);

		scr_seed(trial, trial % 4);
		scr_place(&a, scr_ours, (unsigned)trial % 41u);
		scr_place(&b, scr_theirs, (unsigned)trial % 41u);
		a.tailLength = b.tailLength = (unsigned)trial;
		a.copyHistoryTail();
		ref_scr_copyHistoryTail(&b);
		scr_compare(&a, &b, trial);
	}

	/*
	 * Anti-vacuity.  Without the first, a `process` that never restarted
	 * would pass and `copyHistoryTail` would be untested; without the
	 * second, a `process` that always returned zero would pass.  The third
	 * is the check that the return type really is one byte.
	 */
	diff_eq_int("process crossed the restart", restarts > 0, 1, 0);
	diff_eq_int("process returned something nonzero", differed, 1, 0);
	diff_eq_int("the reference process never returned past a byte",
		    wide, 0, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * generateV90Symbol and generateV92Symbol
 *
 * The fixture the two need beyond `resetDILGenerator`'s:
 *
 *   - ten pointers, seven inside the Scrambler and three bit vectors, which
 *     hold different addresses on the two sides and always will.  They are
 *     blanked in a copy before the whole-object comparison and then compared
 *     as each side's own offset into each side's own buffer.
 *   - `jdBits`, `jdV92Bits` and `jdV92PhaseBits` overwritten on EVERY trial.
 *     Five states dereference them with no null check, so the pseudorandom
 *     fill would segfault.  Null is legitimate only in TRN1d, which tests it.
 *   - `state` forced to the arm under test, and driven out of range as well:
 *     the object's bound is `cmp $0xf; ja`, unsigned.
 *   - `dsplibs_debug_level` and `ref_dsplibs_debug_level` pinned together,
 *     at 0 and at 2, because both functions branch on it.
 *
 * `input` on every check below is `state * 10000 + trial`, or the loop's own
 * case number where a loop drives several states.
 * ===========================================================================
 */

#define JDBUF	96u

static unsigned char jd_ours[3][JDBUF], jd_theirs[3][JDBUF];

/* Coverage, all asserted at the end of run_generate(). */
static int cov_state[16], cov_illegal;
static int cov_exit[16], cov_stay[16];
static int cov_trn1d_null, cov_jdnot_null;
static int cov_dil_wrap, cov_dil_nowrap;
static int cov_dil_segment, cov_dil_entry, cov_dil_negated;
static int cov_seq1_wrap, cov_seq2_wrap, cov_dilindex_wrap;
static int cov_segindex8, cov_scr_restart;
static int cov_sample_neg, cov_sample_pos;

static void
prepare(int trial, int mode, unsigned int st)
{
	unsigned int i, j;

	seed(trial, mode);

	ours.o.pcmType = theirs.o.pcmType =
	    (trial & 1) ? PCM_TYPE_A_LAW : PCM_TYPE_MU_LAW;
	ours.o.state = theirs.o.state = (Phase3ModulatorState)st;

	for (i = 0; i < 3; i++)
		for (j = 0; j < JDBUF; j++)
			jd_ours[i][j] = jd_theirs[i][j] =
			    (unsigned char)(next_byte() & 1u);
	ours.o.jdBits = jd_ours[0];
	ours.o.jdV92Bits = jd_ours[1];
	ours.o.jdV92PhaseBits = jd_ours[2];
	theirs.o.jdBits = jd_theirs[0];
	theirs.o.jdV92Bits = jd_theirs[1];
	theirs.o.jdV92PhaseBits = jd_theirs[2];

	for (i = 0; i < SCR_BUF; i++)
		scr_ours[i] = scr_theirs[i] =
		    (unsigned char)(next_byte() & 1u);
	scr_place(&ours.o.scrambler, scr_ours, (unsigned)trial % 41u);
	scr_place(&theirs.o.scrambler, scr_theirs, (unsigned)trial % 41u);

	/*
	 * The two byte sequences must contain zeros: `seq2[seq2Index] == 0`
	 * is what makes a DIL symbol carry the segment level instead of the
	 * entry level, and `seq1[seq1Index] == 0` is what negates it.  A
	 * pseudorandom fill hits zero once in 256 and one of the seed modes
	 * never does, so both are planted at a fixed density.
	 */
	for (i = 0; i < 128; i++) {
		unsigned char v = next_byte();

		if (((i + (unsigned)trial) % 3u) == 0u)
			v = 0;
		else if (v == 0)
			v = 1;
		ours.o.seq1[i] = theirs.o.seq1[i] = v;

		v = next_byte();
		if (((i + 2u * (unsigned)trial) % 4u) == 0u)
			v = 0;
		else if (v == 0)
			v = 1;
		ours.o.seq2[i] = theirs.o.seq2[i] = v;
	}

	/* The segment lengths the object's own resetDILGenerator produces. */
	for (i = 0; i < 8; i++)
		ours.o.segmentLength[i] = theirs.o.segmentLength[i] =
		    6u * (i + 1u) + 6u;

	ours.o.segmentIndex = theirs.o.segmentIndex =
	    (unsigned char)(trial % 9);
	ours.o.segmentPos = theirs.o.segmentPos = 0;
	ours.o.dilCount = theirs.o.dilCount = (unsigned char)(1 + trial % 5);
	ours.o.dilIndex = theirs.o.dilIndex = (unsigned char)(trial % 6);
	ours.o.seq1Length = theirs.o.seq1Length =
	    (unsigned char)(1 + trial % 7);
	ours.o.seq2Length = theirs.o.seq2Length =
	    (unsigned char)(1 + trial % 5);
	ours.o.seq1Index = theirs.o.seq1Index = (unsigned char)(trial % 8);
	ours.o.seq2Index = theirs.o.seq2Index = (unsigned char)(trial % 6);

	ours.o.symbolCount = theirs.o.symbolCount = 0;
	ours.o.timeoutBase = theirs.o.timeoutBase = 0;
	ours.o.polarity = theirs.o.polarity = (unsigned)trial & 1u;

	/*
	 * NOT zero.  Every arm of both functions writes `eventCode`, and a
	 * dropped write is invisible if the field was already zero going in --
	 * mutation showed it: "eventCode not cleared in the DIL arm" survived
	 * until this line stopped clearing it.
	 */
	ours.o.eventCode = theirs.o.eventCode =
	    0x5a5a0000u + (unsigned int)trial;
}

/*
 * One call on each side, everything compared, coverage recorded.  Returns
 * nothing: what the test asserts about the outcome, it asserts from the
 * object afterwards.
 */
static void
drive(int v92, long input)
{
	static union mod_slot ca, cb;
	unsigned int st = (unsigned int)ours.o.state;
	const unsigned char *before = ours.o.scrambler.pOut;
	unsigned int pos_before = ours.o.segmentPos;
	unsigned char seq1i = ours.o.seq1Index, seq2i = ours.o.seq2Index;
	unsigned char dili = ours.o.dilIndex;
	int a, b;

	a = v92 ? ours.o.generateV92Symbol() : ours.o.generateV90Symbol();
	b = v92 ? ref_generateV92Symbol(&theirs.o)
		: ref_generateV90Symbol(&theirs.o);

	diff_eq_int("generateSymbol returned (case %ld)", a, b, input);

	memcpy(&ca, &ours, sizeof(ca));
	memcpy(&cb, &theirs, sizeof(cb));
	memset(ca.raw + 0x20, 0, 0x1c);		/* the Scrambler's seven */
	memset(cb.raw + 0x20, 0, 0x1c);
	memset(ca.raw + 0x44, 0, 0x0c);		/* the three bit vectors  */
	memset(cb.raw + 0x44, 0, 0x0c);
	diff_eq_obj("after generateSymbol", V90Phase3Modulator,
		    &ca.o, &cb.o, input);

	scr_compare(&ours.o.scrambler, &theirs.o.scrambler, input);

	diff_eq_int("jdBits offset (case %ld)",
		    ours.o.jdBits ? ours.o.jdBits - jd_ours[0] : -1,
		    theirs.o.jdBits ? theirs.o.jdBits - jd_theirs[0] : -1,
		    input);
	diff_eq_int("jdV92Bits offset (case %ld)",
		    ours.o.jdV92Bits ? ours.o.jdV92Bits - jd_ours[1] : -1,
		    theirs.o.jdV92Bits ? theirs.o.jdV92Bits - jd_theirs[1] : -1,
		    input);
	diff_eq_int("jdV92PhaseBits offset (case %ld)",
		    ours.o.jdV92PhaseBits
			? ours.o.jdV92PhaseBits - jd_ours[2] : -1,
		    theirs.o.jdV92PhaseBits
			? theirs.o.jdV92PhaseBits - jd_theirs[2] : -1,
		    input);
	diff_eq_int("the bit vectors are untouched (case %ld)",
		    memcmp(jd_ours, jd_theirs, sizeof(jd_ours)) == 0, 1, input);
	diff_eq_int("no store past the object (case %ld)", guard_equal(), 1,
		    input);

	if (st < 16) {
		cov_state[st] = 1;
		if ((unsigned int)ours.o.state != st)
			cov_exit[st] = 1;
		else
			cov_stay[st] = 1;
	} else {
		cov_illegal = 1;
	}
	if (before == scr_ours &&
	    ours.o.scrambler.pOut == ours.o.scrambler.pInitOut)
		cov_scr_restart = 1;
	if (st == P3M_STATE_DIL || st == P3M_STATE_DIL_END) {
		int wrapped = (ours.o.segmentPos == 0 && pos_before + 1u != 0u);

		if (wrapped)
			cov_dil_wrap = 1;
		else
			cov_dil_nowrap = 1;
		if (ours.o.usingSegmentLevel)
			cov_dil_segment = 1;
		else
			cov_dil_entry = 1;
		if (ours.o.seq1[seq1i] == 0)
			cov_dil_negated = 1;
		/*
		 * The segment wrap zeroes all three cursors, so a cursor
		 * wrapping ON ITS OWN LENGTH is only observable when the
		 * segment did not end -- and `dilIndex` only ever moves when
		 * it did.
		 */
		if (!wrapped && ours.o.seq1Index == 0 && seq1i != 0)
			cov_seq1_wrap = 1;
		if (!wrapped && ours.o.seq2Index == 0 && seq2i != 0)
			cov_seq2_wrap = 1;
		if (wrapped && ours.o.dilIndex == 0 && dili != 0)
			cov_dilindex_wrap = 1;
		if (ours.o.segmentIndex == 8)
			cov_segindex8 = 1;
	}
	if (a < 0)
		cov_sample_neg = 1;
	if (a > 0)
		cov_sample_pos = 1;
}

static void
cov_reset(void)
{
	int i;

	for (i = 0; i < 16; i++)
		cov_state[i] = cov_exit[i] = cov_stay[i] = 0;
	cov_illegal = cov_trn1d_null = cov_jdnot_null = 0;
	cov_dil_wrap = cov_dil_nowrap = 0;
	cov_dil_segment = cov_dil_entry = cov_dil_negated = 0;
	cov_seq1_wrap = cov_seq2_wrap = cov_dilindex_wrap = 0;
	cov_segindex8 = cov_scr_restart = 0;
	cov_sample_neg = cov_sample_pos = 0;
}

static int
run_generate(int v92)
{
	int st, trial, lvl, k;

	diff_begin(v92 ? "V90Phase3Modulator::generateV92Symbol"
		       : "V90Phase3Modulator::generateV90Symbol");
	cov_reset();

	/*
	 * Every arm, every seed mode, both debug levels.  Nothing here forces
	 * a transition; this is the sixteen states plus the out-of-range one
	 * running in their steady state.
	 */
	for (lvl = 0; lvl < 2; lvl++) {
		dsplibs_debug_level = ref_dsplibs_debug_level =
		    lvl ? 2u : 0u;
		for (st = 0; st < 17; st++) {
			for (trial = 0; trial < 8; trial++) {
				unsigned int s = (st < 16)
				    ? (unsigned int)st
				    : 0x51a70000u + (unsigned int)trial;

				prepare(trial, trial % 4, s);
				ours.o.symbolCount = theirs.o.symbolCount =
				    (unsigned int)(trial * 7 + 1);
				drive(v92, (long)(st * 10000 + trial));
			}
		}
	}
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	/*
	 * Every exit, forced.  Each is `symbolCount == K`, so random seeding
	 * reaches none of them; each is driven at K and at K-1 so the
	 * comparison sees the branch both ways.
	 */
	for (trial = 0; trial < 24; trial++) {
		int mo = trial % 4;
		unsigned int base = 0x1000u * (unsigned int)(trial % 3);

		/* Sd, 384 symbols. */
		for (k = 0; k < 2; k++) {
			prepare(trial, mo, P3M_STATE_SD);
			ours.o.symbolCount = theirs.o.symbolCount =
			    0x180u - 1u - (unsigned int)k;
			drive(v92, (long)(200000 + trial * 10 + k));
		}

		/* SdNot, 48 symbols, and the scrambler reset it ends on. */
		for (k = 0; k < 2; k++) {
			prepare(trial, mo, P3M_STATE_SD_NOT);
			ours.o.symbolCount = theirs.o.symbolCount =
			    0x30u - 1u - (unsigned int)k;
			drive(v92, (long)(210000 + trial * 10 + k));
		}

		/* TRN1d, 0x3e7c symbols, with and without the bit vector. */
		for (k = 0; k < 4; k++) {
			prepare(trial, mo, P3M_STATE_TRN1D);
			ours.o.symbolCount = theirs.o.symbolCount =
			    0x3e7cu - 1u - (unsigned int)(k & 1);
			if (k & 2) {
				if (v92)
					ours.o.jdV92Bits =
					    theirs.o.jdV92Bits = NULL;
				else
					ours.o.jdBits = theirs.o.jdBits = NULL;
			}
			dsplibs_debug_level = ref_dsplibs_debug_level =
			    (k & 2) ? 2u : 0u;
			drive(v92, (long)(220000 + trial * 10 + k));
			if (k == 2 && ours.o.state == P3M_STATE_ERROR)
				cov_trn1d_null = 1;
		}
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;

		/*
		 * Jd, at `timeoutBase + 24804` -- and the one input that
		 * separates the object's x87 comparison from the integer one
		 * it is NOT.  0xfffffff0 + 24804 wraps to 24788 in thirty-two
		 * bits, so a reconstruction that added in integers would fire
		 * here.  The object adds in the x87, where nothing wraps, and
		 * does not.
		 */
		for (k = 0; k < 2; k++) {
			prepare(trial, mo, P3M_STATE_JD);
			ours.o.timeoutBase = theirs.o.timeoutBase = base;
			ours.o.symbolCount = theirs.o.symbolCount =
			    base + 24803u - (unsigned int)k;
			drive(v92, (long)(230000 + trial * 10 + k));
		}
		prepare(trial, mo, P3M_STATE_JD);
		ours.o.timeoutBase = theirs.o.timeoutBase = 0xfffffff0u;
		ours.o.symbolCount = theirs.o.symbolCount = 24787u;
		drive(v92, (long)(230100 + trial));
		diff_eq_int("the Jd timeout does not wrap at 2**32 "
			    "(trial %ld)", (long)ours.o.state,
			    (long)P3M_STATE_JD, trial);

		/* The 72-boundary states: 7 under V.90, 4 and 6 under V.92. */
		for (k = 0; k < 4; k++) {
			unsigned int s;

			if (v92)
				s = (k < 2) ? P3M_STATE_V92JD_END
					    : P3M_STATE_JD_PHASE_END;
			else
				s = P3M_STATE_JD_END;

			prepare(trial, mo, s);
			ours.o.symbolCount = theirs.o.symbolCount =
			    71u - (unsigned int)(k & 1);
			drive(v92, (long)(240000 + trial * 10 + k));
		}
		/*
		 * ... and the wrap of the counter itself: 0xffffffff
		 * increments to 0, which IS a multiple of 72, so the exit
		 * fires from the far end of the range.
		 */
		for (k = 0; k < 2; k++) {
			unsigned int s = v92
			    ? (k ? P3M_STATE_JD_PHASE_END : P3M_STATE_V92JD_END)
			    : P3M_STATE_JD_END;

			prepare(trial, mo, s);
			ours.o.symbolCount = theirs.o.symbolCount = 0xffffffffu;
			drive(v92, (long)(240100 + trial * 10 + k));
		}

		/*
		 * JdPhase, V.92 only, and its timeout is an ABSOLUTE 24804
		 * with no `timeoutBase` added -- which is only visible with
		 * `timeoutBase` set to something.
		 */
		if (v92) {
			for (k = 0; k < 2; k++) {
				prepare(trial, mo, P3M_STATE_JD_PHASE);
				ours.o.timeoutBase = theirs.o.timeoutBase =
				    base + 0x40000u;
				ours.o.symbolCount = theirs.o.symbolCount =
				    24803u - (unsigned int)k;
				drive(v92, (long)(250000 + trial * 10 + k));
			}
			diff_eq_int("the JdPhase timeout ignores timeoutBase "
				    "(trial %ld)", (long)ours.o.state,
				    (long)P3M_STATE_JD_PHASE, trial);
			prepare(trial, mo, P3M_STATE_JD_PHASE);
			ours.o.timeoutBase = theirs.o.timeoutBase =
			    base + 0x40000u;
			ours.o.symbolCount = theirs.o.symbolCount = 24803u;
			drive(v92, (long)(250100 + trial));
			diff_eq_int("the JdPhase timeout fires at 24804 "
				    "(trial %ld)", (long)ours.o.state,
				    (long)P3M_STATE_JD_PHASE_TIMEOUT, trial);
		}

		/* JdNot, 12 symbols, with and without a DIL sequence. */
		for (k = 0; k < 4; k++) {
			prepare(trial, mo, P3M_STATE_JD_NOT);
			ours.o.symbolCount = theirs.o.symbolCount =
			    12u - 1u - (unsigned int)(k & 1);
			if (k & 2)
				ours.o.dilCount = theirs.o.dilCount = 0;
			drive(v92, (long)(260000 + trial * 10 + k));
			if (k == 2 && ours.o.state == P3M_STATE_ERROR)
				cov_jdnot_null = 1;
		}

		/*
		 * DIL, at `timeoutBase + 40000` -- the mirror of the Jd case
		 * above.  This one IS integer: 0xfffffff0 + 40000 wraps to
		 * 39984 and the object fires there.
		 */
		for (k = 0; k < 2; k++) {
			prepare(trial, mo, P3M_STATE_DIL);
			ours.o.timeoutBase = theirs.o.timeoutBase = base;
			ours.o.symbolCount = theirs.o.symbolCount =
			    base + 39999u - (unsigned int)k;
			drive(v92, (long)(270000 + trial * 10 + k));
		}
		prepare(trial, mo, P3M_STATE_DIL);
		ours.o.timeoutBase = theirs.o.timeoutBase = 0xfffffff0u;
		ours.o.symbolCount = theirs.o.symbolCount = 39983u;
		drive(v92, (long)(270100 + trial));
		diff_eq_int("the DIL timeout does wrap at 2**32 (trial %ld)",
			    (long)ours.o.state, (long)P3M_STATE_DIL_TIMEOUT,
			    trial);

		/*
		 * Both DIL states at the segment boundary, which clears three
		 * cursors, advances `dilIndex` modulo `dilCount` and
		 * recomputes `segmentIndex` -- and, in the DIL_END state, is
		 * the whole of the terminating condition.
		 */
		for (k = 0; k < 4; k++) {
			unsigned int s = (k & 2) ? P3M_STATE_DIL_END
						 : P3M_STATE_DIL;

			prepare(trial, mo, s);
			ours.o.segmentIndex = theirs.o.segmentIndex =
			    (unsigned char)(trial % 8);
			ours.o.segmentPos = theirs.o.segmentPos =
			    ours.o.segmentLength[ours.o.segmentIndex] - 1u -
			    (unsigned int)(k & 1);
			ours.o.symbolCount = theirs.o.symbolCount =
			    100u + (unsigned int)trial;
			drive(v92, (long)(280000 + trial * 10 + k));
		}

		/*
		 * Both sequence cursors one step from wrapping on their own
		 * length, with the segment deliberately NOT ending -- the
		 * segment wrap zeroes all three anyway, so it would hide the
		 * modulus.
		 */
		for (k = 0; k < 2; k++) {
			prepare(trial, mo, k ? P3M_STATE_DIL_END
					     : P3M_STATE_DIL);
			ours.o.seq1Index = theirs.o.seq1Index =
			    (unsigned char)(ours.o.seq1Length - 1);
			ours.o.seq2Index = theirs.o.seq2Index =
			    (unsigned char)(ours.o.seq2Length - 1);
			ours.o.segmentIndex = theirs.o.segmentIndex =
			    (unsigned char)(trial % 8);
			ours.o.segmentPos = theirs.o.segmentPos = 0;
			ours.o.symbolCount = theirs.o.symbolCount =
			    100u + (unsigned int)trial;
			drive(v92, (long)(290000 + trial * 10 + k));
		}

		/* ... and `dilIndex` one step from wrapping on `dilCount`,
		 * which needs the segment to end in the same symbol. */
		for (k = 0; k < 2; k++) {
			prepare(trial, mo, k ? P3M_STATE_DIL_END
					     : P3M_STATE_DIL);
			ours.o.dilIndex = theirs.o.dilIndex =
			    (unsigned char)(ours.o.dilCount - 1);
			ours.o.segmentIndex = theirs.o.segmentIndex =
			    (unsigned char)(trial % 8);
			ours.o.segmentPos = theirs.o.segmentPos =
			    ours.o.segmentLength[ours.o.segmentIndex] - 1u;
			ours.o.symbolCount = theirs.o.symbolCount =
			    100u + (unsigned int)trial;
			drive(v92, (long)(295000 + trial * 10 + k));
		}

		/*
		 * The scrambler's restart, in every state that calls
		 * `process`.  `pOut` is put on `pLimit`, so the single call
		 * this symbol makes takes it below and the restart path --
		 * `resetHistoryIndexes` then `copyHistoryTail` -- runs.
		 */
		for (k = 0; k < 5; k++) {
			static const unsigned int scr_states_v90[5] = {
				P3M_STATE_TRN1D, P3M_STATE_JD,
				P3M_STATE_JD_END, P3M_STATE_JD_NOT,
				P3M_STATE_JD
			};
			static const unsigned int scr_states_v92[5] = {
				P3M_STATE_TRN1D, P3M_STATE_JD,
				P3M_STATE_V92JD_END, P3M_STATE_JD_PHASE,
				P3M_STATE_JD_PHASE_END
			};
			unsigned int s = v92 ? scr_states_v92[k]
					     : scr_states_v90[k];

			prepare(trial, mo, s);
			scr_place(&ours.o.scrambler, scr_ours, 0);
			scr_place(&theirs.o.scrambler, scr_theirs, 0);
			ours.o.symbolCount = theirs.o.symbolCount =
			    3u + (unsigned int)trial;
			drive(v92, (long)(300000 + trial * 10 + k));
		}
	}

	/* -- anti-vacuity ------------------------------------------------ */
	for (st = 0; st < 16; st++) {
		int expected_exit = 1;

		/*
		 * The five terminal states and the three each protocol does
		 * not own never leave; everything else must have been seen
		 * both leaving and staying.
		 */
		if (st >= P3M_STATE_TERMINATED)
			expected_exit = 0;
		if (v92 && st == P3M_STATE_JD_END)
			expected_exit = 0;
		if (!v92 && (st == P3M_STATE_V92JD_END ||
			     st == P3M_STATE_JD_PHASE ||
			     st == P3M_STATE_JD_PHASE_END))
			expected_exit = 0;

		diff_eq_int("state %ld was driven", cov_state[st], 1,
			    (long)st);
		diff_eq_int("state %ld was seen not transitioning",
			    cov_stay[st], 1, (long)st);
		diff_eq_int("state %ld transitioned iff it can", cov_exit[st],
			    expected_exit, (long)st);
	}
	diff_eq_int("an out-of-range state was driven", cov_illegal, 1, 0);
	diff_eq_int("TRN1d saw a null bit vector", cov_trn1d_null, 1, 0);
	diff_eq_int("JdNot saw an empty DIL sequence", cov_jdnot_null, 1, 0);
	diff_eq_int("a DIL segment ended", cov_dil_wrap, 1, 0);
	diff_eq_int("a DIL segment did not end", cov_dil_nowrap, 1, 0);
	diff_eq_int("a DIL symbol took the segment level", cov_dil_segment,
		    1, 0);
	diff_eq_int("a DIL symbol took the entry level", cov_dil_entry, 1, 0);
	diff_eq_int("a DIL symbol was negated by seq1", cov_dil_negated, 1, 0);
	diff_eq_int("seq1Index wrapped", cov_seq1_wrap, 1, 0);
	diff_eq_int("seq2Index wrapped", cov_seq2_wrap, 1, 0);
	diff_eq_int("dilIndex wrapped", cov_dilindex_wrap, 1, 0);
	diff_eq_int("the segment search reached index 8", cov_segindex8, 1, 0);
	diff_eq_int("the scrambler restarted", cov_scr_restart, 1, 0);
	diff_eq_int("a symbol came out negative", cov_sample_neg, 1, 0);
	diff_eq_int("a symbol came out positive", cov_sample_pos, 1, 0);

	return diff_end();
}

/*
 * The whole machine, in sequence, from state 0.  Every loop above drives one
 * symbol from a fabricated state; this one lets the object walk its own path
 * and compares every symbol along it, which is the only thing that checks the
 * states compose.  The three transitions the object does NOT make for itself
 * -- Jd to its 72-boundary state, and DIL to its terminating one, which
 * `exitJd`, `exitJdPhase` and `exitDIL` make and which are not in this batch
 * -- are made here by writing the state on both sides, which is still a
 * comparison because both sides are written identically.
 */
static long
run_until(int v92, unsigned int st, int limit, long id)
{
	int i;

	for (i = 0; i < limit && (unsigned int)ours.o.state == st; i++)
		drive(v92, id++);
	return id;
}

static int
run_sequence(int v92)
{
	long id = 0;

	diff_begin(v92 ? "V90Phase3Modulator::generateV92Symbol in sequence"
		       : "V90Phase3Modulator::generateV90Symbol in sequence");

	prepare(3, 0, P3M_STATE_SD);
	ours.o.timeoutBase = theirs.o.timeoutBase = 0;

	/* Sd for 384, then SdNot for 48, then into TRN1d.  Both natural. */
	id = run_until(v92, P3M_STATE_SD, 500, id);
	diff_eq_int("the sequence reached SdNot", (long)ours.o.state,
		    (long)P3M_STATE_SD_NOT, 0);
	id = run_until(v92, P3M_STATE_SD_NOT, 100, id);
	diff_eq_int("the sequence reached TRN1d", (long)ours.o.state,
		    (long)P3M_STATE_TRN1D, 0);

	/* TRN1d ends at 0x3e7c; skip most of it and take the boundary. */
	ours.o.symbolCount = theirs.o.symbolCount = 0x3e7cu - 20u;
	id = run_until(v92, P3M_STATE_TRN1D, 40, id);
	diff_eq_int("the sequence reached Jd", (long)ours.o.state,
		    (long)P3M_STATE_JD, 0);
	diff_eq_int("entering Jd was reported", (long)ours.o.eventCode, 2, 0);

	id = run_until(v92, P3M_STATE_JD, 200, id);
	diff_eq_int("Jd did not time out early", (long)ours.o.state,
		    (long)P3M_STATE_JD, 0);

	/* What exitJd does, spelled out: on to the 72-boundary state. */
	ours.o.state = theirs.o.state =
	    v92 ? P3M_STATE_V92JD_END : P3M_STATE_JD_END;
	id = run_until(v92, (unsigned int)ours.o.state, 100, id);

	if (v92) {
		diff_eq_int("the sequence reached JdPhase", (long)ours.o.state,
			    (long)P3M_STATE_JD_PHASE, 0);
		id = run_until(v92, P3M_STATE_JD_PHASE, 100, id);
		/* ... and what exitJdPhase does. */
		ours.o.state = theirs.o.state = P3M_STATE_JD_PHASE_END;
		id = run_until(v92, P3M_STATE_JD_PHASE_END, 100, id);
	}

	diff_eq_int("the sequence reached JdNot", (long)ours.o.state,
		    (long)P3M_STATE_JD_NOT, 0);
	id = run_until(v92, P3M_STATE_JD_NOT, 40, id);
	diff_eq_int("the sequence reached DIL", (long)ours.o.state,
		    (long)P3M_STATE_DIL, 0);

	id = run_until(v92, P3M_STATE_DIL, 400, id);
	diff_eq_int("DIL did not time out early", (long)ours.o.state,
		    (long)P3M_STATE_DIL, 0);

	/*
	 * What exitDIL does: on to the state that stops at a segment end.
	 *
	 * The segment search leaves `segmentIndex` at 8 when no boundary
	 * matches, and `segmentLength[8]` is one past the array -- it reads
	 * the first two `segmentLevel` entries as a 32-bit length, which the
	 * seed makes enormous, so the segment would not end inside any
	 * reasonable run.  That aliasing is the object's and is compared like
	 * everything else above; here the cursor is simply put back inside the
	 * table, on both sides, so the terminating path is reachable.
	 */
	if (ours.o.segmentIndex > 7) {
		ours.o.segmentIndex = theirs.o.segmentIndex = 0;
		ours.o.segmentPos = theirs.o.segmentPos = 0;
	}
	ours.o.state = theirs.o.state = P3M_STATE_DIL_END;
	id = run_until(v92, P3M_STATE_DIL_END, 200, id);
	diff_eq_int("the sequence terminated", (long)ours.o.state,
		    (long)P3M_STATE_TERMINATED, 0);
	diff_eq_int("termination reported itself", (long)ours.o.eventCode,
		    6, 0);

	id = run_until(v92, P3M_STATE_TERMINATED, 20, id);
	diff_eq_int("the terminated state is quiet", id > 0, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * The three GATED diagnostic sites, which no object comparison can reach.
 *
 * "Illegal state" separates the five quiet states from the illegal ones, and
 * that is the ONLY thing separating them: both arms write `eventCode = 0` and
 * return zero, so mutating state 13 from quiet to illegal under V.92 -- or
 * the reverse under V.90 -- leaves the object identical and survives
 * everything above.  The two "ERROR: Null ... @ end of TRN1d" sites are gated
 * as well.
 *
 * The harness's debug capture reaches them.  It works HERE and not everywhere
 * because the states driven below reach no `edprintf` site: `edprintf` is a
 * defined symbol in the blob, so each side runs its own copy with its own
 * encoder counter and the two produce different text for the same message.
 * `dsplibs_debug_printf` is imported, so a gated site goes straight to the
 * harness on both sides and the transcripts compare directly, text and all.
 * ===========================================================================
 */
static int
run_diagnostics(int v92)
{
	int trial, st, lvl, ours_printed = 0, theirs_printed = 0;

	diff_begin(v92 ? "generateV92Symbol gated diagnostics"
		       : "generateV90Symbol gated diagnostics");

	dsplib_debug_capture_on = 1;

	/*
	 * THE LEVEL IS SWEPT 0 TO 3, not just raised.  The gate is `> 1`, so
	 * 0 and 1 must produce nothing and 2 and 3 must produce the message;
	 * a site with the gate dropped, or set at the wrong threshold, is
	 * identical to the object at one level and differs at another.  That
	 * is finding 150's point and it is what a single level misses --
	 * mutation showed it, with "the illegal arm's gate dropped" surviving
	 * a level-2-only sweep.
	 */
	for (lvl = 0; lvl < 4; lvl++) {
		dsplibs_debug_level = ref_dsplibs_debug_level =
		    (unsigned int)lvl;

		/*
		 * The states each protocol does not own, the five quiet ones,
		 * and one out of range.  Every other state reaches an
		 * `edprintf` site and is deliberately not driven here.
		 */
		for (st = 4; st < 17; st++) {
			if (st > 7 && st < 11)
				continue;
			for (trial = 0; trial < 4; trial++) {
				unsigned int s = (st < 16)
				    ? (unsigned int)st
				    : 0x51a70000u + (unsigned int)trial;

				prepare(trial, trial % 4, s);
				ours.o.symbolCount =
				    theirs.o.symbolCount =
				    (unsigned int)(trial * 7 + 1);
				dsplib_debug_capture_reset();
				drive(v92, (long)(400000 + lvl * 1000 +
						  st * 10 + trial));

				diff_eq_int("gated diagnostic lines "
					    "(case %ld)",
				    (long)dsplib_debug_capture_lines(0),
				    (long)dsplib_debug_capture_lines(1),
				    (long)(lvl * 100 + st));
				diff_eq_int("gated diagnostic text (case %ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, (long)(lvl * 100 + st));
				ours_printed +=
				    (int)dsplib_debug_capture_lines(0);
				theirs_printed +=
				    (int)dsplib_debug_capture_lines(1);
			}
		}

		/* The null bit vector at the end of TRN1d, gated the same. */
		for (trial = 0; trial < 4; trial++) {
			prepare(trial, trial % 4, P3M_STATE_TRN1D);
			ours.o.symbolCount = theirs.o.symbolCount =
			    0x3e7cu - 1u;
			if (v92)
				ours.o.jdV92Bits = theirs.o.jdV92Bits = NULL;
			else
				ours.o.jdBits = theirs.o.jdBits = NULL;
			dsplib_debug_capture_reset();
			drive(v92, (long)(410000 + lvl * 100 + trial));

			diff_eq_int("null bit vector diagnostic lines "
				    "(case %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1),
			    (long)(lvl * 100 + trial));
			diff_eq_int("null bit vector diagnostic text "
				    "(case %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    (long)(lvl * 100 + trial));
			ours_printed += (int)dsplib_debug_capture_lines(0);
			theirs_printed += (int)dsplib_debug_capture_lines(1);
		}
	}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	/*
	 * Anti-vacuity: two empty captures agree about nothing at all, which
	 * is the shape finding 149 warns about.
	 */
	diff_eq_int("our gated sites printed something", ours_printed > 0, 1,
		    0);
	diff_eq_int("the blob's gated sites printed something",
		    theirs_printed > 0, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_table();
	rc |= run_setsessionflag();
	rc |= run_resetdilgenerator();
	rc |= run_scrambler();
	rc |= run_generate(0);
	rc |= run_generate(1);
	rc |= run_sequence(0);
	rc |= run_sequence(1);
	rc |= run_diagnostics(0);
	rc |= run_diagnostics(1);

	return rc;
}

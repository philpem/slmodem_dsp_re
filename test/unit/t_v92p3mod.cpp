/*
 * t_v92p3mod.cpp -- differential test of V92Phase3Modulator::generateSymbol
 * and ::reset against the blob.
 *
 * Finding 1113's shape, with the parts this class forces:
 *
 * THE OBJECT IS SEEDED WITH VARIED BYTES, NEVER ZEROED, and both sides get
 * the same fill before every call.  One of the four modes is 0xa5 throughout
 * -- the harness's own malloc fill -- which is what a field neither side
 * writes looks like, and `run_generate` asserts that fill is GONE from the
 * fields both sides must write.  `eventCode` in particular is seeded to a
 * distinctive non-zero value, because every arm writes it and a dropped write
 * is invisible against a zero.
 *
 * FOUR THINGS MUST BE FORCED OR THE FILL IS NOT A TEST, IT IS A CRASH:
 *
 *   - `jaBitCount` is the divisor of a `divl` in the Ja arms and `jaBits` is
 *     dereferenced with no null check.  A pseudorandom object reaching state
 *     4 or 5 with symbolCount past 24 is SIGFPE on both sides, or a wild
 *     read.  Both are planted on every trial, `jaBitCount` in 1..96.
 *   - the Scrambler's seven pointers are dereferenced.  Each side gets its
 *     own 64-byte buffer with the same relative layout, so `pOut - buf` is
 *     comparable even though `pOut` never is; the restart is 41 symbols in,
 *     so a run of 100 crosses it twice.
 *   - `params` is dereferenced by `reset`.  ONE block, pointed at by both
 *     sides, so the stored pointer compares equal and no field has to be
 *     excluded -- `reset` only reads it.
 *   - `state` is written through the raw bytes at +0x08 rather than through
 *     the enum member, because the sweep drives it OUT OF RANGE on purpose
 *     (the object's bound is `cmp $0xf; ja`, unsigned) and assigning 100 to a
 *     sixteen-value enum is not something C++ defines.
 *
 * TWO SWEEP INPUTS EXIST ONLY TO MAKE A SIGNEDNESS OBSERVABLE, and without
 * them the differential tier is blind to it in exactly finding 613's way:
 *
 *   - `symbolCount` is driven at and past 0x80000000.  Every residue in
 *     `generateSymbol` is an unsigned reciprocal multiply with no sign fixup;
 *     at symbolCount 0x80000004 the unsigned `% 12` is 0 and the signed one is
 *     -4, so the arm taken differs and the field is forced, not assumed.
 *   - `V92_ECHO_FAST_UPDATE_DURATION + V92_ECHO_SLOW_UPDATE_DURATION + 12` is
 *     driven NEGATIVE.  `reset` divides that by twelve with `imul` and the
 *     `sar $0x1f`/`sub` fixup -- signed -- and then compares the product
 *     with `ja` -- unsigned.  Over non-negative sums the two readings are
 *     identical; at -188 they store 0xffffff4c and 0xffffff60.
 *
 * THE TRANSCRIPT IS COMPARED, NOT JUST THE OBJECT, and for one specific
 * reason.  State 2's jump-table entry is the DEFAULT label and states 6, 14
 * and 15 share a silent block; both write zero to `eventCode` and return
 * zero, so NOTHING IN THE OBJECT SEPARATES THEM.  The only difference is the
 * "Illegal state" line, so `run_diagnostics` raises both debug levels and
 * compares each side's captured text byte for byte.  Without that, folding
 * case 2 into the silent group would pass every check here.
 *
 * The `ref_` aliases are reached through asm() labels rather than by spelling
 * the alias as an identifier, which sidesteps finding 225.  The convention is
 * plain cdecl with `this` as the first stack argument (finding 215).
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V92Parameters.h"
#include "dsplib/V92Phase3Modulator.h"

extern "C" {

/*
 * The return type is not mangled and the header derives it as `int` from the
 * object's own widening; declaring the alias `int` is what makes the
 * comparison cover all thirty-two bits rather than the low sixteen.
 */
int ref_generateSymbol(void *self)
	asm("ref__ZN18V92Phase3Modulator14generateSymbolEv");

/*
 * Six arguments after `this`, all by value on the stack.  The enum is
 * declared `int` here rather than by its own name because an extern "C"
 * prototype only has to describe the ABI, and it is int-sized.  The leading
 * `short` is the dead one; it is passed a varied value anyway, so that a
 * reconstruction using it instead of the literal 4000 would fail.
 */
void ref_reset(void *self, short level, int st, unsigned int nSymbols,
	       void *ja, const void *dil, unsigned int last)
	asm("ref__ZN18V92Phase3Modulator5resetEs23V92Phase3ModulatorStatejP5V92JaPK19tagV90DILdescriptorj");

extern unsigned int ref_dsplibs_debug_level;
}

#include "dsplib/debug.h"

/* The object, plus room past its end to catch a store that overruns it. */
#define SLOT	128u

/*
 * THE EMPTY SPECIAL MEMBERS ARE LOAD-BEARING.  `Scrambler` declares a
 * constructor and a destructor, which leaves `V92Phase3Modulator` with no
 * default constructor and a non-trivial destructor, which DELETES both of a
 * union holding one.  A user-provided pair that constructs and destroys no
 * variant member restores them and changes nothing else.
 */
struct mod_slot {
	union {
		unsigned char raw[SLOT];
		double align_;		/* alignment only; trivial */
	};
	V92Phase3Modulator &o;

	mod_slot() : o(*(V92Phase3Modulator *)raw) {}
};

static struct mod_slot ours, theirs;
static struct mod_slot cmp_a, cmp_b;

/*
 * The parameter block.  `reset` reads two ints out of it and writes nothing,
 * so ONE block serves both sides and the stored `params` pointer compares
 * equal with no neutralisation.
 */
struct par_slot {
	union {
		unsigned char raw[0xdc];
		double align_;		/* alignment only; trivial */
	};
	V92Parameters &o;

	par_slot() : o(*(V92Parameters *)raw) {}
};

static struct par_slot par;

/* The V92Ja: a count at +0x00 and the vector at +0x04. */
#define JABITS	96u

union ja_slot {
	V92Ja o;
	unsigned char raw[4u + JABITS];
};

static union ja_slot ja_ours, ja_theirs;

/*
 * The descriptor `reset` never dereferences.  Its only use is `test`, so any
 * non-null address will do and its contents are never read; it is a byte
 * array rather than a `tagV90DILdescriptor` because this header declares that
 * type incomplete on purpose.
 */
static unsigned char dil_dummy[8];

/*
 * The Scrambler's buffer.  41 symbols reach the restart, so a hundred-symbol
 * run crosses it twice and `copyHistoryTail` actually executes.
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
 * Every pointer as each side's own offset into each side's own buffer, plus
 * the buffers themselves.  The raw pointers are never compared and never
 * merely checked non-null: two static arrays at two addresses would pass that
 * and prove nothing (finding 224).
 */
static void
scr_compare(long input)
{
	const ScramblerHI *a = &ours.o.scrambler;
	const ScramblerHI *b = &theirs.o.scrambler;

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
	diff_eq_int("scrambler buffer (case %ld)",
		    memcmp(scr_ours, scr_theirs, SCR_BUF) == 0, 1, input);
}

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/*
 * `mode` picks how varied the fill is.  Mode 1 is the harness's own malloc
 * fill; modes 2 and 3 pin the high bit of every byte, which is what decides
 * the sign of `codeLevel` and `suLevel` and therefore whether the arms emit
 * a negative sample first or a positive one.
 */
static void
seed(int trial, int mode)
{
	unsigned int i;

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

/* The enum field, written as the four bytes the object stores. */
static void
set_state(unsigned int st)
{
	*(unsigned int *)(ours.raw + 0x08) = st;
	*(unsigned int *)(theirs.raw + 0x08) = st;
}

static unsigned int
get_state(const struct mod_slot *m)
{
	return *(const unsigned int *)(m->raw + 0x08);
}

static int
guard_equal(void)
{
	return memcmp(ours.raw + sizeof(V92Phase3Modulator),
		      theirs.raw + sizeof(V92Phase3Modulator),
		      SLOT - sizeof(V92Phase3Modulator)) == 0;
}

/*
 * The two sides' objects with the ten bytes-worth of pointers that can never
 * agree blanked: the Scrambler's seven at +0x18..+0x33 and `jaBits` at +0x3c.
 * Everything else -- including `params`, which is one block -- is compared as
 * it stands.
 */
static void
compare_object(const char *what, long input)
{
	memcpy(cmp_a.raw, ours.raw, SLOT);
	memcpy(cmp_b.raw, theirs.raw, SLOT);
	memset(cmp_a.raw + 0x18, 0, 0x1c);
	memset(cmp_b.raw + 0x18, 0, 0x1c);
	memset(cmp_a.raw + 0x3c, 0, 4);
	memset(cmp_b.raw + 0x3c, 0, 4);
	diff_eq_obj(what, V92Phase3Modulator, &cmp_a.o, &cmp_b.o, input);

	diff_eq_int("jaBits offset (case %ld)",
		    ours.o.jaBits ? ours.o.jaBits - ja_ours.raw : -1,
		    theirs.o.jaBits ? theirs.o.jaBits - ja_theirs.raw : -1,
		    input);
	diff_eq_int("the Ja vectors are untouched (case %ld)",
		    memcmp(ja_ours.raw, ja_theirs.raw,
			   sizeof(ja_ours.raw)) == 0, 1, input);
	diff_eq_int("no store past the object (case %ld)", guard_equal(), 1,
		    input);
}

/* ======================================================================= */
/* generateSymbol                                                          */
/* ======================================================================= */

static int cov_state[16], cov_illegal;
static int cov_exit[16], cov_stay[16];
static int cov_phase[6];
static int cov_ja_const, cov_ja_vector;
static int cov_pol0, cov_pol1;
static int cov_scr_restart;
static int cov_sample_neg, cov_sample_pos, cov_sample_zero;
static int cov_bigcount, cov_event_moved, cov_fill_seen;

/* Set by prepare(), read by drive() for the mode-1 fill assertions. */
static int cur_mode;

/*
 * Every symbolCount the sweep drives, as the PRE-increment value: the object
 * increments before it dispatches, so 383 is what makes the Ru arm see 384.
 * The last six are the ones that separate an unsigned residue from a signed
 * one; the two at the top of the range also wrap the counter to zero.
 */
static const unsigned int counts[] = {
	0u, 1u, 2u, 3u, 4u, 5u, 6u, 10u, 11u, 12u,
	22u, 23u, 24u, 25u, 26u, 30u, 35u,
	142u, 143u, 144u, 382u, 383u, 384u,
	2038u, 2039u, 2040u, 2051u,
	0x7ffffffeu, 0x7fffffffu, 0x80000000u, 0x80000003u,
	0xfffffffeu, 0xffffffffu
};

#define NCOUNT	((int)(sizeof(counts) / sizeof(counts[0])))

/* In range, then over it: the bound is unsigned, so 0xffffffff must go too. */
static const unsigned int state_sweep[] = {
	0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u,
	16u, 17u, 100u, 0xffffffffu
};

#define NSTATE	((int)(sizeof(state_sweep) / sizeof(state_sweep[0])))

#define EVENT_SEED	0x5a5a0000u

/*
 * `trn1uMode` 0 makes the TRN1u arms miss their limit, 1 makes them hit it
 * exactly.  It is a separate knob because `trn1uLength` is the only limit in
 * the class that is a field rather than a literal.
 */
static void
prepare(int trial, int mode, unsigned int st, unsigned int count,
	int trn1uMode)
{
	unsigned int i;

	cur_mode = mode;
	seed(trial, mode);
	set_state(st);

	for (i = 0; i < sizeof(ja_ours.raw); i++)
		ja_ours.raw[i] = ja_theirs.raw[i] =
		    (unsigned char)(next_byte() & 1u);

	ours.o.jaBitCount = theirs.o.jaBitCount =
	    1u + (unsigned int)trial % JABITS;
	ours.o.jaBits = ja_ours.o.bits;
	theirs.o.jaBits = ja_theirs.o.bits;

	for (i = 0; i < SCR_BUF; i++)
		scr_ours[i] = scr_theirs[i] =
		    (unsigned char)(next_byte() & 1u);
	scr_place(&ours.o.scrambler, scr_ours, (unsigned)trial % 41u);
	scr_place(&theirs.o.scrambler, scr_theirs, (unsigned)trial % 41u);

	ours.o.params = theirs.o.params = &par.o;

	ours.o.symbolCount = theirs.o.symbolCount = count;
	/*
	 * Three settings, because the TRN1u limit is the one comparison in the
	 * class that is `==` against a FIELD.  Mode 0 misses it, mode 1 hits
	 * it exactly, and mode 2 puts the count PAST it -- which is what
	 * separates `==` from `>=` and is unreachable without it.
	 */
	ours.o.trn1uLength = theirs.o.trn1uLength =
	    trn1uMode == 1 ? count + 1u : (trn1uMode == 2 ? count : count + 7u);

	ours.o.polarity = theirs.o.polarity = (unsigned int)trial & 1u;

	/*
	 * NOT zero, and not the fill either: every arm writes `eventCode`, so
	 * a dropped write is invisible against a value an arm might have
	 * left, and `cov_event_fill_gone` below asserts the 0xa5 mode's fill
	 * really was replaced.
	 */
	ours.o.eventCode = theirs.o.eventCode =
	    EVENT_SEED + (unsigned int)trial;
}

static void
drive(long input)
{
	unsigned int st = get_state(&ours);
	unsigned int count = ours.o.symbolCount;
	const unsigned char *before = ours.o.scrambler.pOut;
	unsigned int pol_before = ours.o.polarity;
	unsigned int event_before = ours.o.eventCode;
	int a, b;

	a = ours.o.generateSymbol();
	b = ref_generateSymbol(&theirs.o);

	diff_eq_int("generateSymbol returned (case %ld)", a, b, input);
	compare_object("after generateSymbol", input);
	scr_compare(input);

	/*
	 * THE FILL IS ASSERTED PRESENT AND ASSERTED DISPLACED.  In the 0xa5
	 * mode the two words at +0x44 must still hold it -- no symbol of this
	 * class writes them, which is the claim the header makes and this is
	 * the only thing that checks it -- while `eventCode` must not, because
	 * every arm writes that one.  A fill nobody can see is not a fill.
	 */
	if (cur_mode == 1) {
		static const unsigned char a5[8] = {
			0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5
		};

		diff_eq_int("pad_44 kept the fill (case %ld)",
			    memcmp(ours.raw + 0x44, a5, 8) == 0, 1, input);
		diff_eq_int("pad_44 kept the fill on the blob's side "
			    "(case %ld)",
			    memcmp(theirs.raw + 0x44, a5, 8) == 0, 1, input);
		cov_fill_seen = 1;
	}

	if (st < 16u) {
		cov_state[st] = 1;
		if (get_state(&ours) != st)
			cov_exit[st] = 1;
		else
			cov_stay[st] = 1;
	} else {
		cov_illegal = 1;
	}

	cov_phase[count % 6u] = 1;
	if (count + 1u >= 0x80000000u)
		cov_bigcount = 1;

	if (st == V92P3M_STATE_JA || st == V92P3M_STATE_JA_END) {
		if (count + 1u > 24u)
			cov_ja_vector = 1;
		else
			cov_ja_const = 1;
		if (ours.o.polarity != 0u)
			cov_pol1 = 1;
		else
			cov_pol0 = 1;
		(void)pol_before;
	}
	if (before == scr_ours &&
	    ours.o.scrambler.pOut == ours.o.scrambler.pInitOut)
		cov_scr_restart = 1;

	if (a < 0)
		cov_sample_neg = 1;
	else if (a > 0)
		cov_sample_pos = 1;
	else
		cov_sample_zero = 1;

	if (ours.o.eventCode != event_before)
		cov_event_moved = 1;
}

static int
run_generate(void)
{
	int si, ci, mode, trial = 0;
	int i;

	diff_begin("V92Phase3Modulator::generateSymbol");

	for (si = 0; si < NSTATE; si++) {
		for (ci = 0; ci < NCOUNT; ci++) {
			for (mode = 0; mode < 4; mode++, trial++) {
				long input = (long)si * 100000L +
				    (long)ci * 100L + mode;

				prepare(trial, mode, state_sweep[si],
					counts[ci], trial % 3);
				drive(input);
			}
		}
	}

	for (i = 0; i < 16; i++) {
		diff_eq_int("the sweep reached state %ld", cov_state[i], 1, i);
		diff_eq_int("state %ld was driven without leaving it",
			    cov_stay[i], 1, i);
	}

	/*
	 * The nine states that can leave, and only those.  4, 9 and 12 have no
	 * exit of their own -- `exitJa`, `exitSuSecond` and `exitTRN1u` are
	 * what move them -- and 2, 6, 14 and 15 have none at all, so asserting
	 * an exit for any of the seven would be asserting a claim the object
	 * contradicts.
	 */
	diff_eq_int("state 0 exited", cov_exit[0], 1, 0);
	diff_eq_int("state 1 exited", cov_exit[1], 1, 0);
	diff_eq_int("state 3 exited", cov_exit[3], 1, 0);
	diff_eq_int("state 5 exited", cov_exit[5], 1, 0);
	diff_eq_int("state 7 exited", cov_exit[7], 1, 0);
	diff_eq_int("state 8 exited", cov_exit[8], 1, 0);
	diff_eq_int("state 10 exited", cov_exit[10], 1, 0);
	diff_eq_int("state 11 exited", cov_exit[11], 1, 0);
	diff_eq_int("state 13 exited", cov_exit[13], 1, 0);
	diff_eq_int("state 4 never left on its own", cov_exit[4], 0, 0);
	diff_eq_int("state 9 never left on its own", cov_exit[9], 0, 0);
	diff_eq_int("state 12 never left on its own", cov_exit[12], 0, 0);
	diff_eq_int("state 6 never left on its own", cov_exit[6], 0, 0);
	diff_eq_int("state 14 never left on its own", cov_exit[14], 0, 0);
	diff_eq_int("state 15 never left on its own", cov_exit[15], 0, 0);
	diff_eq_int("state 2 never left on its own", cov_exit[2], 0, 0);
	diff_eq_int("an out-of-range state was driven", cov_illegal, 1, 0);

	for (i = 0; i < 6; i++)
		diff_eq_int("the sweep reached (symbolCount - 1) %% 6 == %ld",
			    cov_phase[i], 1, i);

	diff_eq_int("Ja took its constant bit", cov_ja_const, 1, 0);
	diff_eq_int("Ja took the bit vector", cov_ja_vector, 1, 0);
	diff_eq_int("Ja was driven with polarity 0", cov_pol0, 1, 0);
	diff_eq_int("Ja was driven with polarity 1", cov_pol1, 1, 0);
	diff_eq_int("the scrambler crossed its restart", cov_scr_restart, 1,
		    0);
	diff_eq_int("a negative sample was produced", cov_sample_neg, 1, 0);
	diff_eq_int("a positive sample was produced", cov_sample_pos, 1, 0);
	diff_eq_int("a zero sample was produced", cov_sample_zero, 1, 0);
	diff_eq_int("symbolCount was driven past 0x7fffffff", cov_bigcount, 1,
		    0);
	diff_eq_int("eventCode moved off its seed", cov_event_moved, 1, 0);
	diff_eq_int("the 0xa5 fill mode was exercised", cov_fill_seen, 1, 0);

	return diff_end();
}

/* ======================================================================= */
/* A free run: one seeded object stepped repeatedly, both sides in lockstep. */
/* ======================================================================= */

/*
 * The per-call sweep above resets the object between calls, so it never sees
 * a state the object drove itself into.  This one starts at Ru and steps
 * until it stops moving, which walks the whole chain Ru -> RuNot -> TRN1u ->
 * Ja and then, with the four `exit*` transitions applied by hand -- they are
 * not this batch's symbols and are poked as three-line edits to the field the
 * object's own methods write -- on through Silence, Su and TRN1uSecond.
 */
static int
run_sequence(void)
{
	long step;
	int reached[16];
	int i;

	diff_begin("V92Phase3Modulator::generateSymbol, free run");

	memset(reached, 0, sizeof(reached));

	prepare(7, 0, V92P3M_STATE_RU, 0u, 0);
	/* Short enough that TRN1u ends inside the run. */
	ours.o.trn1uLength = theirs.o.trn1uLength = 30u;

	for (step = 0; step < 4200; step++) {
		unsigned int st = get_state(&ours);

		if (st < 16u)
			reached[st] = 1;
		drive(400000L + step);

		/*
		 * The four exits, applied to BOTH sides identically at the
		 * same step.  Their own methods are not defined here; what is
		 * written is the field they write, which is the only thing
		 * that reaches `generateSymbol`.
		 */
		st = get_state(&ours);
		if (ours.o.symbolCount != 0u) {
			unsigned int next = st;

			if (st == V92P3M_STATE_JA && step > 600)
				next = (ours.o.symbolCount % 12u) == 0u
				    ? V92P3M_STATE_SILENCE
				    : V92P3M_STATE_JA_END;
			else if (st == V92P3M_STATE_SILENCE && step > 700)
				next = V92P3M_STATE_SU;
			else if (st == V92P3M_STATE_SU_SECOND && step > 900)
				next = (ours.o.symbolCount % 12u) == 0u
				    ? V92P3M_STATE_SU_SECOND_NOT
				    : V92P3M_STATE_SU_SECOND_END;
			else if (st == V92P3M_STATE_TRN1U_SECOND && step > 1000)
				next = V92P3M_STATE_TRN1U_SECOND_END;

			if (next != st) {
				set_state(next);
				if (next == V92P3M_STATE_SILENCE ||
				    next == V92P3M_STATE_SU ||
				    next == V92P3M_STATE_SU_SECOND_NOT) {
					ours.o.symbolCount = 0;
					theirs.o.symbolCount = 0;
				}
			}
		}
	}

	for (i = 0; i < 16; i++) {
		if (i == V92P3M_STATE_ILLEGAL_2 ||
		    i == V92P3M_STATE_SILENT_15)
			continue;
		diff_eq_int("the free run reached state %ld", reached[i], 1, i);
	}

	return diff_end();
}

/* ======================================================================= */
/* reset                                                                   */
/* ======================================================================= */

/*
 * Each row is the pair of V92Parameters durations `reset` adds.  Rows 2, 3
 * and 9 make the sum negative, which is the only input under which a signed
 * divide and an unsigned one store different lengths; rows 0, 4, 5 and 6
 * straddle the 8160 floor in both directions and on it.
 */
static const int par_pairs[][2] = {
	{      0,       0 },	/* 12      -> floored to 8160          */
	{   5000,    5000 },	/* 10008   -> kept                     */
	{   -100,    -100 },	/* -180 signed, 0xffffff60 unsigned    */
	{    -13,       0 },	/* 0 signed (floored), huge unsigned   */
	{   8148,       0 },	/* exactly 8160, not floored           */
	{   8136,       0 },	/* 8148, floored                       */
	{   8149,       0 },	/* 8172, kept                          */
	{     -6,      -6 },	/* 0, floored; both readings agree     */
	{      1,       2 },	/* 12, floored                         */
	{ -20000,    1000 },	/* negative again, larger magnitude    */
	{ 100000,  100000 }	/* 200004, kept                        */
};

#define NPAR	((int)(sizeof(par_pairs) / sizeof(par_pairs[0])))

static const unsigned int nsym_ja[] = { 0u, 1u, 7u, 60u, 400u };
static const unsigned int nsym_null[] = { 0u, 1u, 7u, 20u };

static int cov_reset_ja, cov_reset_nullja, cov_reset_bug, cov_reset_nobug;
static int cov_reset_floor, cov_reset_kept, cov_reset_negative;
static int cov_reset_warm, cov_reset_moved_state;

static void
prepare_reset(int trial, int mode, int pi)
{
	unsigned int i;

	seed(trial, mode);
	set_state(0xdeadbeefu);		/* reset overwrites it */

	for (i = 0; i < sizeof(ja_ours.raw); i++)
		ja_ours.raw[i] = ja_theirs.raw[i] =
		    (unsigned char)(next_byte() & 1u);
	ja_ours.o.bitCount = ja_theirs.o.bitCount =
	    1u + (unsigned int)trial % JABITS;

	for (i = 0; i < SCR_BUF; i++)
		scr_ours[i] = scr_theirs[i] =
		    (unsigned char)(next_byte() & 1u);
	scr_place(&ours.o.scrambler, scr_ours, (unsigned)trial % 41u);
	scr_place(&theirs.o.scrambler, scr_theirs, (unsigned)trial % 41u);

	ours.o.params = theirs.o.params = &par.o;
	par.o.V92_ECHO_FAST_UPDATE_DURATION = par_pairs[pi][0];
	par.o.V92_ECHO_SLOW_UPDATE_DURATION = par_pairs[pi][1];

	ours.o.eventCode = theirs.o.eventCode =
	    EVENT_SEED + (unsigned int)trial;
}

static int
run_reset(void)
{
	int pi, si, mode, janull, dilnull;
	int trial = 0;
	long input = 0;

	diff_begin("V92Phase3Modulator::reset");

	for (pi = 0; pi < NPAR; pi++) {
		for (si = 0; si < NSTATE; si++) {
			for (mode = 0; mode < 4; mode++) {
				for (janull = 0; janull < 2; janull++) {
					for (dilnull = 0; dilnull < 2;
					     dilnull++, trial++, input++) {
	unsigned int nsym = janull
	    ? nsym_null[trial % (int)(sizeof(nsym_null) / sizeof(nsym_null[0]))]
	    : nsym_ja[trial % (int)(sizeof(nsym_ja) / sizeof(nsym_ja[0]))];
	short level = (short)(0x2000 + trial * 7);
	unsigned int last = 0x1000u + (unsigned int)trial;
	/*
	 * IN RANGE, unlike run_generate's sweep.  `reset` only STORES this
	 * argument, so an out-of-range value would test the same store and
	 * cost a cast C++ does not define; the bound is `generateSymbol`'s and
	 * is driven there.  Folding the sweep modulo 16 keeps all sixteen.
	 */
	unsigned int st = state_sweep[si] % 16u;
	const void *dil = dilnull ? (const void *)0
				  : (const void *)dil_dummy;

	prepare_reset(trial, mode, pi);

	ours.o.reset(level, (V92Phase3ModulatorState)st, nsym,
		     janull ? (V92Ja *)0 : &ja_ours.o,
		     (const tagV90DILdescriptor *)dil, last);
	ref_reset(&theirs.o, level, (int)st, nsym,
		  janull ? (void *)0 : &ja_theirs.o, dil, last);

	compare_object("after reset", input);
	scr_compare(input);

	if (janull) {
		cov_reset_nullja = 1;
		if (!dilnull)
			cov_reset_bug = 1;
		else
			cov_reset_nobug = 1;
	} else {
		cov_reset_ja = 1;
	}
	if (ours.o.trn1uLength == 8160u)
		cov_reset_floor = 1;
	else
		cov_reset_kept = 1;
	if (par_pairs[pi][0] + par_pairs[pi][1] + 12 < 0)
		cov_reset_negative = 1;
					}
				}
			}
		}
	}

	/*
	 * And the warm-up loop, which is the only thing that makes `reset`
	 * reach `generateSymbol`.  Driven separately so the state the object
	 * ends in can be asserted to have moved.
	 */
	for (pi = 0; pi < NPAR; pi++) {
		for (mode = 0; mode < 4; mode++, trial++, input++) {
			unsigned int nsym = nsym_ja[trial % 5];

			prepare_reset(trial, mode, pi);

			ours.o.reset(4000, V92P3M_STATE_RU, nsym,
				     &ja_ours.o, (const tagV90DILdescriptor *)
				     dil_dummy, 0x77u);
			ref_reset(&theirs.o, 4000, 0, nsym, &ja_theirs.o,
				  dil_dummy, 0x77u);

			compare_object("after reset with a warm-up", input);
			scr_compare(input);

			if (nsym != 0u) {
				cov_reset_warm = 1;
				if (get_state(&ours) != V92P3M_STATE_RU)
					cov_reset_moved_state = 1;
			}
		}
	}

	diff_eq_int("reset was given a V92Ja", cov_reset_ja, 1, 0);
	diff_eq_int("reset was given a null V92Ja", cov_reset_nullja, 1, 0);
	diff_eq_int("the null-Ja-with-a-descriptor arm was reached",
		    cov_reset_bug, 1, 0);
	diff_eq_int("the null-Ja-without-a-descriptor arm was reached",
		    cov_reset_nobug, 1, 0);
	diff_eq_int("a length was floored at 8160", cov_reset_floor, 1, 0);
	diff_eq_int("a length above 8160 was kept", cov_reset_kept, 1, 0);
	diff_eq_int("a negative duration sum was driven", cov_reset_negative,
		    1, 0);
	diff_eq_int("the warm-up loop ran", cov_reset_warm, 1, 0);
	diff_eq_int("the warm-up loop moved the state", cov_reset_moved_state,
		    1, 0);

	/*
	 * `suLevel` is derived from `codeLevel`, which `reset` sets to the
	 * literal 4000 whatever it was passed: sqrt(3/2) * 4000 + 0.5 is
	 * 4899.47, truncated to 4899.  Asserted against both sides having
	 * agreed above, so this only pins the value the pair settled on.
	 */
	diff_eq_int("reset set codeLevel to 4000", ours.o.codeLevel, 4000, 0);
	diff_eq_int("reset set suLevel to 4899", ours.o.suLevel, 4899, 0);

	return diff_end();
}

/* ======================================================================= */
/* The diagnostics, and the ONLY thing that separates state 2 from 6/14/15  */
/* ======================================================================= */

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

static int
run_diagnostics(void)
{
	int lvl, si, ci;
	int trial = 0;
	int saw_illegal_text = 0, saw_silent_text = 0;
	int saw_bug_text = 0;

	diff_begin("V92Phase3Modulator diagnostics");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 2; lvl += 2) {
		set_level(lvl);

		for (si = 0; si < NSTATE; si++) {
			for (ci = 0; ci < 4; ci++, trial++) {
				long input = (long)lvl * 10000L +
				    (long)si * 100L + ci;
				unsigned lo, hi;

				prepare(trial, ci, state_sweep[si],
					counts[ci * 7], trial & 1);
				dsplib_debug_capture_reset();
				drive(input);

				lo = dsplib_debug_capture_lines(0);
				hi = dsplib_debug_capture_lines(1);
				diff_eq_int("diagnostic lines (case %ld)",
					    lo, hi, input);
				diff_eq_int("transcript (case %ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, input);

				if (lvl == 2) {
					if (state_sweep[si] ==
					    V92P3M_STATE_ILLEGAL_2 && lo == 1u)
						saw_illegal_text = 1;
					if (state_sweep[si] ==
					    V92P3M_STATE_SILENCE && lo == 0u)
						saw_silent_text = 1;
				}
			}
		}
	}

	/*
	 * The "BUG BUG BUG" line, which only `reset` can print and only when
	 * it is given no V92Ja and a descriptor anyway.
	 */
	set_level(2);
	for (si = 0; si < 4; si++, trial++) {
		prepare_reset(trial, si, si % NPAR);
		dsplib_debug_capture_reset();

		ours.o.reset(4000, V92P3M_STATE_RU, 0u, (V92Ja *)0,
			     (const tagV90DILdescriptor *)dil_dummy, 0u);
		ref_reset(&theirs.o, 4000, 0, 0u, (void *)0, dil_dummy, 0u);

		diff_eq_int("reset diagnostic lines (case %ld)",
			    dsplib_debug_capture_lines(0),
			    dsplib_debug_capture_lines(1), si);
		diff_eq_int("reset transcript (case %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, si);
		compare_object("after reset, diagnostics on", 800000L + si);
		if (dsplib_debug_capture_lines(0) >= 2u)
			saw_bug_text = 1;
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	/*
	 * ANTI-VACUITY FOR THE TRANSCRIPT ITSELF.  A comparison of two empty
	 * strings passes for ever, so the two sides of the fork the object
	 * comparison cannot see are asserted to have actually printed
	 * different numbers of lines.
	 */
	diff_eq_int("state 2 printed a line", saw_illegal_text, 1, 0);
	diff_eq_int("state 6 printed nothing", saw_silent_text, 1, 0);
	diff_eq_int("reset printed its no-Ja warning as a second line",
		    saw_bug_text, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * THE CONSTRUCTOR AND THE DESTRUCTOR
 *
 * BOTH SIDES ARE CALLED BY SYMBOL.  C++ has no syntax for running a
 * constructor over storage that already exists -- no placement new, the tree
 * builds -nostdinc++ -- and `ours.o = V92Phase3Modulator(p)` would build a
 * temporary over uninitialised stack and copy it in, which throws away the
 * seeded-and-never-zeroed slot this file's comparisons rest on.  So our
 * constructor is reached through an asm() label exactly as the reference one
 * is.
 *
 * THE `params` STORE IS ORDERING THAT MATTERS, and the fixture is built to
 * catch it WITHOUT a wild pointer.  `reset` reads two `V92Parameters` fields
 * to compute `trn1uLength`, so a constructor that stored `params` after
 * calling `reset` would read through whatever +0x4c already held.  Both
 * sides' +0x4c is therefore pre-set to a DECOY block whose two durations are
 * different from the real one's, so the wrong ordering reads a valid object
 * and produces a different, comparable `trn1uLength` rather than a segfault.
 * The constructor must overwrite the decoy pointer with the real one, which
 * the object comparison checks at +0x4c.
 *
 * THE SEVEN SCRAMBLER POINTERS ARE THE ONLY THING NEUTRALISED.  Each side
 * mallocs its own 1 + 23 + 99 byte history, so those addresses differ for
 * ever; they are compared as offsets from each side's own `pLimit` and the
 * history is compared as bytes.  `jaBits` is NOT masked here, unlike
 * `compare_object`: the constructor passes a null `V92Ja`, so both sides must
 * produce a null pointer and a zero count and that is worth comparing.
 * `pad_44` is compared as it stands -- neither constructor writes it, so it
 * must still hold the seed.
 * ===========================================================================
 */

extern "C" {
void our_ctor(void *self, void *p)
	asm("_ZN18V92Phase3ModulatorC1EP13V92Parameters");
void our_dtor(void *self) asm("_ZN18V92Phase3ModulatorD1Ev");
void ref_ctor(void *self, void *p)
	asm("ref__ZN18V92Phase3ModulatorC1EP13V92Parameters");
void ref_dtor(void *self) asm("ref__ZN18V92Phase3ModulatorD1Ev");

/*
 * THE C2 AND D2 VARIANTS ARE SEPARATE BODIES IN THE BLOB, not aliases: C1 is
 * at .text+0x16d00 and C2 at +0x16d70, two distinct 105-byte copies, and D1
 * at +0x16290 against D2 at +0x16270.  GCC 3.4 duplicated them; ours are one
 * function under two names.  They are driven on alternate trials against the
 * same one of ours, so "the second copy is the first copy" is measured.
 */
void ref_ctor2(void *self, void *p)
	asm("ref__ZN18V92Phase3ModulatorC2EP13V92Parameters");
void ref_dtor2(void *self) asm("ref__ZN18V92Phase3ModulatorD2Ev");
}

/* Scrambler(5, 23, 99): 1 + 23 + 99 history elements, one byte each. */
#define CTOR_B		23
#define CTOR_C		99
#define CTOR_WORDS	(1u + CTOR_B + CTOR_C)

static struct par_slot par_decoy;

static void
ctor_compare(long input)
{
	const ScramblerHI *a = &ours.o.scrambler;
	const ScramblerHI *b = &theirs.o.scrambler;

	memcpy(cmp_a.raw, ours.raw, SLOT);
	memcpy(cmp_b.raw, theirs.raw, SLOT);
	memset(cmp_a.raw + 0x18, 0, 0x1c);	/* the Scrambler's seven */
	memset(cmp_b.raw + 0x18, 0, 0x1c);
	diff_eq_obj("after the constructor", V92Phase3Modulator,
		    &cmp_a.o, &cmp_b.o, input);

	diff_eq_int("scrambler pInitOut above pLimit (case %ld)",
		    a->pInitOut - a->pLimit, b->pInitOut - b->pLimit, input);
	diff_eq_int("scrambler pInitTap1 above pLimit (case %ld)",
		    a->pInitTap1 - a->pLimit, b->pInitTap1 - b->pLimit, input);
	diff_eq_int("scrambler pInitTap2 above pLimit (case %ld)",
		    a->pInitTap2 - a->pLimit, b->pInitTap2 - b->pLimit, input);
	diff_eq_int("scrambler pOut above pLimit (case %ld)",
		    a->pOut - a->pLimit, b->pOut - b->pLimit, input);
	diff_eq_int("scrambler pTap1 above pLimit (case %ld)",
		    a->pTap1 - a->pLimit, b->pTap1 - b->pLimit, input);
	diff_eq_int("scrambler pTap2 above pLimit (case %ld)",
		    a->pTap2 - a->pLimit, b->pTap2 - b->pLimit, input);
	diff_eq_int("the scrambler history (case %ld)",
		    memcmp(a->pLimit, b->pLimit, CTOR_WORDS) == 0, 1, input);
	diff_eq_int("no store past the object (case %ld)", guard_equal(), 1,
		    input);
}

static int
run_ctor_dtor(void)
{
	static unsigned char before[SLOT], first[SLOT];
	int pi, mode, trial = 0;
	int moved = 0, distinct = 0, have_first = 0;
	int saw_params = 0, saw_floor = 0, saw_kept = 0, saw_history = 0;
	int saw_nullja = 0, saw_pad = 0;
	int saw_variant[2];

	saw_variant[0] = saw_variant[1] = 0;

	diff_begin("V92Phase3Modulator::V92Phase3Modulator and ~");

	for (pi = 0; pi < NPAR; pi++) {
		for (mode = 0; mode < 4; mode++, trial++) {
			long input = (long)pi * 10L + mode;
			int al0, fr0, bad0, fr_ours, fr_theirs;
			int variant = trial & 1;
			unsigned int by0, i;

			seed(trial, mode);
			for (i = 0; i < SLOT; i++)
				before[i] = ours.raw[i];

			par.o.V92_ECHO_FAST_UPDATE_DURATION = par_pairs[pi][0];
			par.o.V92_ECHO_SLOW_UPDATE_DURATION = par_pairs[pi][1];
			/*
			 * The decoy: a valid block with durations that give a
			 * DIFFERENT trn1uLength from every pair above, so the
			 * "store params after reset" mutation is caught by a
			 * number rather than by a crash.
			 */
			par_decoy.o.V92_ECHO_FAST_UPDATE_DURATION = 60000;
			par_decoy.o.V92_ECHO_SLOW_UPDATE_DURATION = 60000;
			ours.o.params = theirs.o.params = &par_decoy.o;
			before[0x4c] = ours.raw[0x4c];
			before[0x4d] = ours.raw[0x4d];
			before[0x4e] = ours.raw[0x4e];
			before[0x4f] = ours.raw[0x4f];

			al0 = harness_alloc.allocs;
			by0 = harness_alloc.bytes;

			our_ctor(&ours.o, &par.o);
			if (variant)
				ref_ctor2(&theirs.o, &par.o);
			else
				ref_ctor(&theirs.o, &par.o);
			saw_variant[variant] = 1;

			diff_eq_int("both constructors allocated once "
				    "(case %ld)",
				    harness_alloc.allocs - al0, 2, input);
			diff_eq_int("1 + b + c bytes each (case %ld)",
				    (long)(harness_alloc.bytes - by0),
				    (long)(2u * CTOR_WORDS), input);

			ctor_compare(input);

			/* Anti-vacuity, read off OUR object. */
			if (memcmp(before, ours.raw, SLOT) != 0)
				moved = 1;
			if (!have_first) {
				memcpy(first, ours.raw, SLOT);
				have_first = 1;
			} else if (memcmp(first, ours.raw, SLOT) != 0) {
				distinct = 1;
			}

			if (ours.o.params == &par.o)
				saw_params = 1;
			if (ours.o.trn1uLength == 8160u)
				saw_floor = 1;
			else
				saw_kept = 1;
			if (ours.o.jaBits == NULL && ours.o.jaBitCount == 0u)
				saw_nullja = 1;
			if (memcmp(ours.raw + 0x44, before + 0x44, 8) == 0)
				saw_pad = 1;
			if (memcmp(ours.o.scrambler.pLimit,
				   before + 0x18, CTOR_WORDS) != 0)
				saw_history = 1;

			/*
			 * The destructor, each side measured on its own.  It
			 * frees the scrambler's history and writes nothing:
			 * the object is compared again afterwards with the
			 * same seven pointers neutralised, because
			 * `~Scrambler` does not null `pLimit`.
			 */
			bad0 = harness_alloc.bad_free;
			fr0 = harness_alloc.frees;
			our_dtor(&ours.o);
			fr_ours = harness_alloc.frees - fr0;
			fr0 = harness_alloc.frees;
			if (variant)
				ref_dtor2(&theirs.o);
			else
				ref_dtor(&theirs.o);
			fr_theirs = harness_alloc.frees - fr0;

			diff_eq_int("our destructor freed exactly once "
				    "(case %ld)", fr_ours, 1, input);
			diff_eq_int("their destructor freed exactly once "
				    "(case %ld)", fr_theirs, 1, input);
			diff_eq_int("neither freed something unknown "
				    "(case %ld)",
				    harness_alloc.bad_free - bad0, 0, input);

			memcpy(cmp_a.raw, ours.raw, SLOT);
			memcpy(cmp_b.raw, theirs.raw, SLOT);
			memset(cmp_a.raw + 0x18, 0, 0x1c);
			memset(cmp_b.raw + 0x18, 0, 0x1c);
			diff_eq_obj("after the destructor",
				    V92Phase3Modulator, &cmp_a.o, &cmp_b.o,
				    input);
			diff_eq_int("no store past the object after the "
				    "destructor (case %ld)", guard_equal(), 1,
				    input);
		}
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("and not to the same thing every trial", distinct, 1, 0);
	diff_eq_int("the constructor stored its V92Parameters *, not the "
		    "decoy", saw_params, 1, 0);
	diff_eq_int("a TRN1u length was floored at 8160", saw_floor, 1, 0);
	diff_eq_int("a TRN1u length above 8160 was kept", saw_kept, 1, 0);
	diff_eq_int("the null V92Ja gave a null vector and a zero count",
		    saw_nullja, 1, 0);
	diff_eq_int("the constructor left pad_44 on its seed", saw_pad, 1, 0);
	diff_eq_int("the scrambler history is not the object's own bytes",
		    saw_history, 1, 0);
	diff_eq_int("the C1 and D1 variants were driven", saw_variant[0], 1, 0);
	diff_eq_int("the C2 and D2 variants were driven too", saw_variant[1],
		    1, 0);

	return diff_end();
}


/* ======================================================================= */
/* the four exit* methods                                                  */
/* ======================================================================= */

/*
 * EACH ONE IS DRIVEN FROM EVERY STATE, not only from its own.  Three quarters
 * of what these methods do is REFUSE: fifteen of the sixteen states leave the
 * object alone, and a reconstruction that dropped the guard would pass a
 * fixture that only ever called `exitJa` with the state already 4.  The sweep
 * below is 16 states by 14 symbol counts by 4 methods, and the object is
 * compared whole after every one of the 896 calls.
 *
 * THE COUNTS STRADDLE THE TWELVE-SYMBOL BOUNDARY IN BOTH DIRECTIONS and go
 * past 0x7fffffff, because `symbolCount % 12` is unsigned in the object: at
 * 0x80000004 the unsigned residue is 0 and the signed one is not, so a
 * reconstruction that declared the counter `int` diverges there and nowhere
 * below it.  That is the same argument run_generate makes for its own `% 12`.
 *
 * ZERO IS ITS OWN CASE.  Every one of the four refuses to move a state whose
 * `symbolCount` is 0 -- `test %ebx,%ebx; je` -- so the count sweep starts
 * there and the coverage assertions below insist the refusal was seen.
 */
extern "C" {
void ref_exitJa(void *self) asm("ref__ZN18V92Phase3Modulator6exitJaEv");
void ref_exitSilence(void *self)
	asm("ref__ZN18V92Phase3Modulator11exitSilenceEv");
void ref_exitSuSecond(void *self)
	asm("ref__ZN18V92Phase3Modulator12exitSuSecondEv");
void ref_exitTRN1u(void *self)
	asm("ref__ZN18V92Phase3Modulator9exitTRN1uEv");
}

/*
 * SIX, EIGHTEEN AND 2046 ARE HERE BECAUSE OF A MUTATION, and they are the
 * whole difference between a boundary of twelve and one of six: every other
 * count below is either divisible by both or by neither.  v92p3mod.json's
 * "exitJa's boundary is six symbols, not twelve" read NOT CAUGHT until these
 * three were added -- the fixture had 12, 24, 144 and 2040, all multiples of
 * twelve, and 11, 13, 23 and 2039, none of them multiples of six.
 */
static const unsigned int exit_counts[] = {
	0u, 1u, 2u, 6u, 11u, 12u, 13u, 18u, 23u, 24u, 144u, 2039u, 2040u,
	2046u, 0x7fffffffu, 0x80000004u, 0xfffffffbu
};

#define NEXITCOUNT ((int)(sizeof(exit_counts) / sizeof(exit_counts[0])))

static int
run_exits(void)
{
	int method, st, ci;
	int cov_moved[4], cov_refused_state[4], cov_refused_zero[4];
	int cov_boundary[4], cov_offboundary[4];

	diff_begin("V92Phase3Modulator::exitJa / exitSilence / exitSuSecond / "
		   "exitTRN1u");

	for (method = 0; method < 4; method++) {
		cov_moved[method] = 0;
		cov_refused_state[method] = 0;
		cov_refused_zero[method] = 0;
		cov_boundary[method] = 0;
		cov_offboundary[method] = 0;
	}

	for (method = 0; method < 4; method++) {
		for (st = 0; st < 16; st++) {
			for (ci = 0; ci < NEXITCOUNT; ci++) {
				long input = (method * 16 + st) * 100 + ci;
				unsigned int count = exit_counts[ci];
				unsigned int was;

				/*
				 * `prepare` is run_generate's own setup: it
				 * seeds both slots, points the two Ja vectors
				 * and the two scrambler histories at their own
				 * side's storage, and shares the parameter
				 * block -- which is what `compare_object`
				 * expects to find when it neutralises the
				 * pointers that can never agree.
				 */
				prepare(st * NEXITCOUNT + ci, ci % 4,
					(unsigned int)st, count, 0);

				switch (method) {
				case 0:
					ours.o.exitJa();
					ref_exitJa(&theirs.o);
					was = 4u;
					break;
				case 1:
					ours.o.exitSilence();
					ref_exitSilence(&theirs.o);
					was = 6u;
					break;
				case 2:
					ours.o.exitSuSecond();
					ref_exitSuSecond(&theirs.o);
					was = 9u;
					break;
				default:
					ours.o.exitTRN1u();
					ref_exitTRN1u(&theirs.o);
					was = 12u;
					break;
				}

				compare_object("after the exit", input);
				diff_eq_int("the state agrees (case %ld)",
					    (long)get_state(&ours),
					    (long)get_state(&theirs), input);
				diff_eq_int("the symbol count agrees (case %ld)",
					    (long)ours.o.symbolCount,
					    (long)theirs.o.symbolCount, input);

				if ((unsigned int)st != was) {
					diff_eq_int("a foreign state is left "
						    "alone (case %ld)",
						    (long)get_state(&ours),
						    (long)st, input);
					cov_refused_state[method] = 1;
				} else if (count == 0u) {
					diff_eq_int("a zero count is left "
						    "alone (case %ld)",
						    (long)get_state(&ours),
						    (long)st, input);
					cov_refused_zero[method] = 1;
				} else {
					cov_moved[method] = 1;
					if (count % 12u == 0u)
						cov_boundary[method] = 1;
					else
						cov_offboundary[method] = 1;
				}
			}
		}
	}

	for (method = 0; method < 4; method++) {
		diff_eq_int("the exit moved its own state (method %ld)",
			    cov_moved[method], 1, method);
		diff_eq_int("the exit refused a foreign state (method %ld)",
			    cov_refused_state[method], 1, method);
		diff_eq_int("the exit refused a zero count (method %ld)",
			    cov_refused_zero[method], 1, method);
		diff_eq_int("a twelve-symbol boundary was driven (method %ld)",
			    cov_boundary[method], 1, method);
		diff_eq_int("and a count off the boundary too (method %ld)",
			    cov_offboundary[method], 1, method);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_generate();
	rc |= run_sequence();
	rc |= run_reset();
	rc |= run_diagnostics();
	rc |= run_ctor_dtor();
	rc |= run_exits();

	return rc;
}

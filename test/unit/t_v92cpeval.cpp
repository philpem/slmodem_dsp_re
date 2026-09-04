/*
 * t_v92cpeval.cpp -- differential test of `V92CP::evaluateInfo` and of
 * `binaryTable`, the weight table it is the only user of.
 *
 * SEPARATE FROM t_v92cpb2i.cpp, which drives the same member through the
 * state machine that calls it.  That test reaches every arm too, but only
 * along the paths a well-formed message takes; this one sets `rxState`,
 * `word_124` and `word_10c` by hand and hands the arm a bit vector it would
 * never see in service.  The two answer different questions and a defect that
 * only shows on a malformed vector is visible to this one alone.
 *
 * ---------------------------------------------------------------------------
 * THE TABLE IS COMPARED DIRECTLY, and that is what makes `1 << i` a tested
 * rule rather than a transcription anybody has to trust.  `binaryTable` is a
 * GLOBAL `D` symbol, so the blob's own copy links in beside ours as
 * `ref_binaryTable` and the two are compared word for word.  A generator that
 * got the direction backwards, or dropped an entry the way `fltTable_2` drops
 * 2^-8, fails here before any arm runs.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE GRID HAS TO SEPARATE, because two objects agreeing proves nothing
 * about a body no input reaches:
 *
 *   bit order   Every arm but the two mask ones reads its field MOST
 *               significant last; the mask arms read theirs MOST significant
 *               FIRST.  A vector of all ones or all zeros cannot tell the two
 *               apart, so the fills below include `i & 1`, `(i >> 1) & 1` and
 *               `i & 7`, each of which gives a different answer under each
 *               reading.
 *
 *   entries     `bits` is one byte per bit and the object never masks the
 *               byte before multiplying it -- `imul` against the whole
 *               `movzbl`.  A fill of 0xff therefore makes every weight 255
 *               times what a masked reading would give, which is the trial
 *               that separates `bits[p] * binaryTable[w]` from
 *               `(bits[p] & 1) * binaryTable[w]`.
 *
 *   char_01     `case 5` skips `word_08` when `char_01 > 1`, and the field it
 *               tests is one it decoded four instructions earlier.  Fill 5
 *               puts 9 there and fills 0..4 put 0, 1, 1, 1 and -1, so both
 *               sides of that branch are driven.
 *
 *   the maximum `case 6` sets `word_10c` from the LARGEST of the six counts.
 *               Fill 0 makes them all zero, so the running maximum never
 *               moves and the answer is 1; fill 5 makes them differ, so it
 *               moves several times.  A version that took the last rather
 *               than the largest passes the first and fails the second.
 *
 *   empty       `case 7` and `case 8` are `for (k = 0; k < word_10c; k++)`,
 *               so a zero group count must do NOTHING -- not one framing
 *               position of cursor movement.  Two cases carry it.
 *
 *   aliasing    `word_10c` is unbounded (docs/deviations.md D570) and the
 *               destination walks out of `short_42` the moment it passes six.
 *               THE TWO ARMS HAVE DIFFERENT CEILINGS, because their
 *               destinations start 0x60 apart.  `short_42[k][j]` is at
 *               0x42 + 16k + 2j and `short_a2[k][j]` at 0xa2 + 16k + 2j, and
 *               what each reaches first is:
 *
 *                 case 7, word_10c   7..12  short_a2, and at [12][5] `word_10c`
 *                                    13     `rxState`, `word_11c`, `stateBitCount`
 *                                    14     the top half of `stateBitCount`
 *                                    15     `word_124` -- THE CURSOR
 *
 *                 case 8, word_10c   1..7   `word_104`, `suv`, `word_10c`,
 *                                           `rxState`..`stateBitCount`
 *                                    8      `word_124` at [8][1], and `bits`
 *                                           itself at [8][4]
 *
 *               So the grid goes to fourteen for `case 7` and to eight for
 *               `case 8`, and both stop one short of the cursor.  The object
 *               reloads `word_124` from memory on every one of the sixteen
 *               iterations, so a store that lands on it moves the cursor
 *               mid-word and the loop then reads wherever the decoded mask
 *               value points -- 22,899 on one trial here, which is off the
 *               end of any allocation.  That is not a domain a test can
 *               drive; docs/deviations.md D922 records it and says what our
 *               source does there instead.
 *
 *               Everything BELOW it is driven, and is worth driving: at
 *               thirteen groups the arm overwrites the state word and both
 *               cursors while it runs, and the object carries on regardless
 *               because its own loop bound and indices are locals.
 *
 *   the holes   `evaluateInfo` decodes 3, 5, 6, 7 and 8.  State 4 is a hole
 *               in the case list, and everything outside 3..8 falls through
 *               to the same `ret`.  Nine states outside the case list are
 *               driven and must leave all 0x918 bytes untouched.
 *
 * ---------------------------------------------------------------------------
 * THE SLOT IS 4 KB LONGER THAN THE CLASS and every byte of it is compared.
 * At fourteen groups the mask arms read 1,904 positions from the cursor, which
 * runs past `bits` and off the end of the object; the tail is there so both
 * sides read and write the same defined bytes rather than whatever follows the
 * array, and so that an overrun is a difference rather than a crash.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/V92CP.h"

extern "C" {
void ref_cp_evalinfo(void *) asm("ref__ZN5V92CP12evaluateInfoEv");

extern int ref_binaryTable[16];
}

extern int binaryTable[16];

#define TAIL	4096u
#define SLOT	((unsigned int)sizeof(V92CP) + TAIL)

static unsigned char cp[2][SLOT] __attribute__((aligned(8)));

static unsigned int lfsr;

static unsigned char
nextbyte(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned int)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)(lfsr >> 3);
}

static V92CP *
C(int s)
{
	return (V92CP *)cp[s];
}

/*
 * The six bit-vector fills.  Which one a case uses is the whole of its
 * discriminating power: 0 and 1 are the two that cannot see a bit order, 2
 * and 3 are the two that can, 4 drives the unmasked multiply and 5 puts
 * values above one under every field at once.
 */
#define FILL_ZERO	0
#define FILL_ONE	1
#define FILL_ALT	2
#define FILL_ALT2	3
#define FILL_FF		4
#define FILL_MOD8	5
#define NFILL		6

static unsigned char
bitval(int fill, unsigned int i)
{
	switch (fill) {
	case FILL_ZERO:	return 0;
	case FILL_ONE:	return 1;
	case FILL_ALT:	return (unsigned char)(i & 1u);
	case FILL_ALT2:	return (unsigned char)((i >> 1) & 1u);
	case FILL_FF:	return 0xff;
	default:	return (unsigned char)(i & 7u);
	}
}

struct ecase {
	const char *name;
	unsigned int state;
	int cursor;
	unsigned short w10c;
};

/*
 * The cursor matters only to the three arms that move it.  `case 6` seeds it
 * itself (52), and `case 3` and `case 5` never touch it, so the value below
 * is there to prove they do not.
 */
static const struct ecase cases[] = {
	{ "state 3, the short form",		3,   0x111,	3 },
	{ "state 3, cursor already high",	3,   1500,	0 },
	{ "state 5, the header",		5,   0x111,	3 },
	{ "state 5, cursor already high",	5,   1900,	6 },
	{ "state 6, the fixed part",		6,   0,		0 },
	{ "state 6, cursor seeded elsewhere",	6,   1234,	6 },
	{ "state 7, no groups at all",		7,   136,	0 },
	{ "state 7, one group",			7,   136,	1 },
	{ "state 7, two groups",		7,   136,	2 },
	{ "state 7, the six the class holds",	7,   136,	6 },
	{ "state 7, seven, one past the class's own", 7, 136,	7 },
	{ "state 7, thirteen, which lands on word_10c", 7, 136,	13 },
	{ "state 7, fourteen, which lands on the state word", 7, 136, 14 },
	{ "state 7, a cursor that is not 136",	7,   17,	4 },
	{ "state 8, no groups at all",		8,   136,	0 },
	{ "state 8, one group",			8,   136,	1 },
	{ "state 8, the six the class holds",	8,   136,	6 },
	{ "state 8, seven",			8,   136,	7 },
	{ "state 8, eight, one short of the cursor", 8, 136,	8 },
	{ "state 8, a cursor that is not 136",	8,   400,	5 },
	{ "state 0, outside the case list",	0,   136,	4 },
	{ "state 1, outside the case list",	1,   136,	4 },
	{ "state 2, outside the case list",	2,   136,	4 },
	{ "state 4, the hole",			4,   136,	4 },
	{ "state 9, outside the case list",	9,   136,	4 },
	{ "state 10, outside the case list",	10,  136,	4 },
	{ "state 11, one past the last arm",	11,  136,	4 },
	{ "state 255, far outside",		255, 136,	4 },
	{ "state 0xffffffff, which sub $3 wraps", 0xffffffffu, 136, 4 }
};

#define NCASE	((int)(sizeof(cases) / sizeof(cases[0])))

static void
setup(int c, int fill, unsigned int seed)
{
	unsigned int i;
	V92CP *o;

	lfsr = seed | 1u;
	for (i = 0; i < SLOT; i++)
		cp[0][i] = nextbyte();

	o = C(0);

	for (i = 0; i < (unsigned int)V92CP_BITS; i++)
		o->bits[i] = bitval(fill, i);

	/*
	 * The mask arms read past `bits` at the larger group counts, so the
	 * tail carries the same fill rather than the random seed -- otherwise
	 * the reading and the reader disagree about what a position holds and
	 * the trial measures the seed instead of the arm.
	 */
	for (i = (unsigned int)V92CP_BITS;
	     i + offsetof(V92CP, bits) < SLOT; i++)
		cp[0][offsetof(V92CP, bits) + i] = bitval(fill, i);

	o->rxState = cases[c].state;
	o->word_124 = cases[c].cursor;
	o->word_10c = cases[c].w10c;

	memcpy(cp[1], cp[0], SLOT);
}

/*
 * The class first, because `diff_eq_obj` names the field a difference lands
 * in, and then the tail as one run -- an overrun past the object has no field
 * to name and would otherwise be reported as 4,096 separate bytes.
 */
static void
compare_slot(const char *what, long tag)
{
	diff_eq_obj(what, V92CP, C(0), C(1), tag);
	diff_eq_int("the tail past the class is untouched (%ld)",
		    (long)(memcmp(cp[0] + sizeof(V92CP),
				  cp[1] + sizeof(V92CP), TAIL) == 0), 1, tag);
}

static int
run_table(void)
{
	int i;

	diff_begin("binaryTable against the blob's own copy");

	for (i = 0; i < 16; i++)
		diff_eq_int("binaryTable[%ld]", (long)binaryTable[i],
			    (long)ref_binaryTable[i], (long)i);

	/* And the rule, stated so a reader can check it by hand. */
	for (i = 0; i < 16; i++)
		diff_eq_int("binaryTable[%ld] is 1 << i", (long)binaryTable[i],
			    (long)(1 << i), (long)i);

	diff_eq_int("the table is 64 bytes (%ld)", (long)sizeof(binaryTable),
		    64, 0);

	return diff_end();
}

/*
 * What the grid actually reached, counted off the BLOB rather than off our
 * copy.  A green run with any of these at zero would be measuring nothing,
 * which is CLAUDE.md's dead detector; the denominator is printed with them.
 */
static int
run_reach(void)
{
	int c, f;
	int moved = 0;
	int untouched = 0;
	int cursor_moved = 0;
	int w10c_changed = 0;
	int mask_written = 0;
	int trials = 0;

	for (c = 0; c < NCASE; c++) {
		for (f = 0; f < NFILL; f++) {
			unsigned char before[SLOT];
			V92CP *o;

			setup(c, f, 0x2468u + (unsigned int)(c * NFILL + f));
			memcpy(before, cp[1], SLOT);

			ref_cp_evalinfo(C(1));
			trials++;

			o = C(1);
			if (memcmp(before, cp[1], SLOT) != 0)
				moved++;
			else
				untouched++;

			if (o->word_124 !=
			    ((const V92CP *)before)->word_124)
				cursor_moved++;
			if (o->word_10c !=
			    ((const V92CP *)before)->word_10c)
				w10c_changed++;
			if (memcmp(before + offsetof(V92CP, short_42),
				   cp[1] + offsetof(V92CP, short_42),
				   2u * 6u * 8u) != 0)
				mask_written++;
		}
	}

	diff_begin("what the grid reaches");
	diff_eq_int("trials run (%ld)", (long)trials, NCASE * NFILL, 0);
	diff_eq_int("the blob changed the object somewhere (%ld)",
		    (long)(moved > 0), 1, 0);
	diff_eq_int("the blob left it alone somewhere (%ld)",
		    (long)(untouched > 0), 1, 0);
	diff_eq_int("the read cursor moved somewhere (%ld)",
		    (long)(cursor_moved > 0), 1, 0);
	diff_eq_int("the group count was recomputed somewhere (%ld)",
		    (long)(w10c_changed > 0), 1, 0);
	diff_eq_int("a mask block was written somewhere (%ld)",
		    (long)(mask_written > 0), 1, 0);

	return diff_end();
}

static int
run_cases(void)
{
	int c, f;

	diff_begin("evaluateInfo over the case grid");

	for (c = 0; c < NCASE; c++) {
		for (f = 0; f < NFILL; f++) {
			long tag = (long)(c * NFILL + f);

			setup(c, f, 0x1357u + (unsigned int)tag);

			C(0)->evaluateInfo();
			ref_cp_evalinfo(C(1));

			compare_slot(cases[c].name, tag);
			diff_eq_int("the read cursor agrees (%ld)",
				    (long)C(0)->word_124,
				    (long)C(1)->word_124, tag);
			diff_eq_int("the group count agrees (%ld)",
				    (long)C(0)->word_10c,
				    (long)C(1)->word_10c, tag);
		}
	}

	return diff_end();
}

/*
 * The sweep drives the five decoding arms over a bit vector of drawn bytes,
 * which is the one stimulus the fills above cannot make: every position an
 * independent value in 0..255, so the unmasked multiply, the float `!= 0`
 * test and the running maximum all see a fresh distribution on every trial.
 */
static int
run_sweep(void)
{
	static const unsigned int states[] = { 3, 5, 6, 7, 8 };
	unsigned int n;

	diff_begin("evaluateInfo over drawn bit vectors");

	for (n = 0; n < 600u; n++) {
		unsigned int i;
		unsigned int s = states[n % 5u];
		V92CP *o;

		lfsr = 0x9e37u + n * 3u;
		for (i = 0; i < SLOT; i++)
			cp[0][i] = nextbyte();

		o = C(0);
		o->rxState = s;
		o->word_124 = (int)(100u + (n % 37u));
		o->word_10c = (unsigned short)(n % 9u);	/* see D922 */

		memcpy(cp[1], cp[0], SLOT);

		C(0)->evaluateInfo();
		ref_cp_evalinfo(C(1));

		compare_slot("after a drawn vector", (long)n);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	diff_begin("the class's map");
	diff_eq_int("sizeof(V92CP) is %ld", (long)sizeof(V92CP), 0x918, 0x918);
	diff_eq_int("rxState is at +0x%lx", (long)offsetof(V92CP, rxState),
		    0x114, 0x114);
	diff_eq_int("word_124 is at +0x%lx", (long)offsetof(V92CP, word_124),
		    0x124, 0x124);
	diff_eq_int("bits is at +0x%lx", (long)offsetof(V92CP, bits), 0x129,
		    0x129);
	rc |= diff_end();

	rc |= run_table();
	rc |= run_reach();
	rc |= run_cases();
	rc |= run_sweep();

	return rc;
}

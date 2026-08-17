/*
 * t_v90sbereset.cpp -- differential test of `V90SignBitsExtractor::reset`.
 *
 * IT IS ITS OWN FILE AND NOT AN ADDITION TO `t_v90demap.cpp`, which already
 * carries this class's `process` and `applyFrameAction` and has the seeding
 * machinery this file repeats.  The reason is contention and not taste: the
 * demapper's four processing members were being written in a parallel
 * worktree while this landed, and `t_v90demap.cpp` is where they belong.
 * `t_v90demap.cpp`'s `sbe_setup` still says "`reset` is not written, so the
 * three words it would set are planted directly" -- that comment is stale
 * from this commit and is left for whoever merges the two branches, because
 * editing it here would collide with work in flight.
 *
 * THE ONE THING THAT CANNOT AGREE is the parallel decoder's `state_` pointer
 * at +0x1c: each side gets its own six-byte buffer, so the word holds two
 * different addresses for ever.  It is zeroed in a copy before the objects
 * are compared and the two buffers are compared separately -- both matter,
 * because `reset` re-arms the decoder and a run that compared only the
 * objects would pass while re-arming nothing.
 *
 * WHAT THE TRIALS HAVE TO SEPARATE, because comparing two objects cannot see
 * a body no input reaches:
 *
 *   THE ZERO ARM IS A GUARD, AND THE MUTATION IS WHAT SAYS SO.  A spacing of
 *   0 takes the arm that stores a width of zero without dividing, and
 *   "divide unconditionally" is caught by that trial raising #DE on our side
 *   and not on the blob's.  `saw_guard` records only that the trial ran; it
 *   proves nothing on its own (findings 3509, 3403).
 *
 *   WHAT THIS FILE CANNOT SEPARATE, and an earlier version of this comment
 *   wrongly claimed it could: whether the guard is `spacing != 0` or
 *   `spacing != 0 && spacing <= 6`.  Those two are behaviourally identical
 *   over all 2^32 spacings -- 6/spacing is zero for every spacing above six,
 *   which is the same zero the else arm stores -- so NO input separates them
 *   and no differential test ever will.  The mutation is registered and
 *   marked `equivalent` with that arithmetic as its reason rather than
 *   deleted, because a reader needs to know the claim was tested and found
 *   untestable here.  The codegen tier is what rules the range test out: the
 *   object has one `test %edx,%edx` / `je` where a second condition would be
 *   a second compare.  Finding 4304.
 *
 *   THE WIDTH IS `6 / spacing` AND NOT `spacing`.  Spacings of 1, 2, 3 and 4
 *   give four DIFFERENT widths (6, 3, 2, 1), so a body that stored the
 *   spacing, or the reciprocal the other way round, disagrees on at least
 *   three of them.
 *
 *   THE PARALLEL DECODER IS RE-ARMED WITH THE WIDTH.  Its `size_` is seeded
 *   to a value no trial computes and read back, and its state buffer is
 *   seeded non-zero and required to come back cleared over `width` entries
 *   and UNTOUCHED past them -- which is what makes "re-arm with the capacity
 *   instead" a catchable mistake rather than an invisible one.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/DiffCoder.h"
#include "dsplib/V90SignBitsExtractor.h"

extern "C" {
void ref_sbe_reset(void *, unsigned int, unsigned int)
	asm("ref__ZN20V90SignBitsExtractor5resetEjj");
}

/* ----------------------------------------------------------- the storage */

#define SBE_SLOT	(0x28 + 32)
#define DEC_PTR		0x1cu		/* the decoder's state_, two values */

static unsigned char sbe_s[2][SBE_SLOT] __attribute__((aligned(8)));
static unsigned char sbe_seed[2][SBE_SLOT];
static unsigned char dstate[2][V90SBE_DECODER_SIZE];
static unsigned char dstate_seed[V90SBE_DECODER_SIZE];

#define SBE(s)	(*(V90SignBitsExtractor *)sbe_s[s])

/* Varied, never zero, never the same twice: findings 223, 224, 230. */
static unsigned
fill(unsigned char *p, int n, unsigned lfsr)
{
	int i;

	for (i = 0; i < n; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		p[i] = (unsigned char)((lfsr >> 3) | 1u);
	}
	return lfsr;
}

/*
 * Both sides identical except for the one pointer that cannot be.  The
 * decoder's own bookkeeping is seeded to values `reset` must overwrite:
 * `size_` to something no trial computes, the state buffer to non-zero.
 */
static void
setup(unsigned lfsr)
{
	int s;

	fill(dstate_seed, V90SBE_DECODER_SIZE, lfsr ^ 0x77u);
	for (s = 0; s < 2; s++) {
		V90SignBitsExtractor *e = &SBE(s);

		fill(sbe_s[s], SBE_SLOT, lfsr);
		memcpy(sbe_seed[s], sbe_s[s], SBE_SLOT);
		memcpy(dstate[s], dstate_seed, V90SBE_DECODER_SIZE);

		e->decoder.state_ = dstate[s];
		e->decoder.capacity_ = V90SBE_DECODER_SIZE;
		e->decoder.size_ = 0x5au;
	}
}

static void
compare(const char *what, long tag)
{
	unsigned char a[SBE_SLOT], b[SBE_SLOT];
	unsigned n = (unsigned)sizeof(V90SignBitsExtractor);

	memcpy(a, sbe_s[0], SBE_SLOT);
	memcpy(b, sbe_s[1], SBE_SLOT);
	memset(a + DEC_PTR, 0, 4);
	memset(b + DEC_PTR, 0, 4);

	diff_eq_obj_(__FILE__, __LINE__, what, "V90SignBitsExtractor", a, b, n,
		     tag);
	diff_eq_int("nothing past the extractor (%ld)",
		    memcmp(a + n, b + n, SBE_SLOT - n) == 0, 1, tag);
	diff_eq_int("the decoder's own state agrees (%ld)",
		    memcmp(dstate[0], dstate[1], V90SBE_DECODER_SIZE) == 0, 1,
		    tag);
	diff_eq_int("the state_ pointer was not moved (%ld)",
		    SBE(0).decoder.state_ == dstate[0] &&
		    SBE(1).decoder.state_ == dstate[1], 1, tag);
}

/* ------------------------------------------------------------------ reset */

static int
run_sbe_reset(void)
{
	/*
	 * 0 is the guard.  1..6 are the divides that give a non-zero width;
	 * 7 and 13 are divides that give ZERO without touching the guard,
	 * which is the pair the guard has to be separated by.  0xffffffff is
	 * there because the divide is unsigned and a signed one would not
	 * give zero for it.
	 */
	static const unsigned int spacings[] = {
		0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 13u, 0x80000000u, 0xffffffffu
	};
	static const unsigned int states[] = { 0u, 1u, 2u, 0xdeadbeefu };
	int trial, i, k;
	int saw_guard = 0, saw_divide_to_zero = 0, saw_wide = 0;
	unsigned int widths_seen = 0;

	diff_begin("V90SignBitsExtractor::reset");

	for (trial = 0; trial < 44; trial++) {
		unsigned int sp =
		    spacings[(unsigned)trial % (sizeof(spacings) /
						sizeof(spacings[0]))];
		unsigned int st =
		    states[((unsigned)trial / 11u) % (sizeof(states) /
						      sizeof(states[0]))];
		long tag = 100 + trial;
		unsigned int want = sp ? V90SBE_DECODER_SIZE / sp : 0u;

		setup(0x1f35u + 0x9e37u * (unsigned)trial);

		SBE(0).reset(sp, st);
		ref_sbe_reset(sbe_s[1], sp, st);

		compare("after reset", tag);

		/* Everything below is read off the BLOB's object, side 1. */
		diff_eq_int("the spacing was taken (%ld)",
			    (long)SBE(1).spacing, (long)sp, tag);
		diff_eq_int("the width is 6/spacing (%ld)",
			    (long)SBE(1).width, (long)want, tag);
		diff_eq_int("the state was taken (%ld)",
			    (long)SBE(1).state, (long)st, tag);
		diff_eq_int("the odd decoder was cleared (%ld)",
			    (long)SBE(1).oddDecoder.prev_, 0, tag);
		diff_eq_int("the parallel decoder was re-armed (%ld)",
			    (long)SBE(1).decoder.size_, (long)want, tag);

		/*
		 * The decoder's buffer: cleared over `width` entries and
		 * untouched past them.  The second half is what separates
		 * "re-arm with the width" from "re-arm with the capacity".
		 */
		for (i = 0; i < (int)want; i++)
			if (dstate[1][i] != 0)
				break;
		diff_eq_int("the decoder's buffer was cleared to width (%ld)",
			    i, (int)want, tag);
		for (k = (int)want; k < (int)V90SBE_DECODER_SIZE; k++)
			if (dstate[1][k] != dstate_seed[k])
				break;
		diff_eq_int("and nothing past the width was touched (%ld)",
			    k, (int)V90SBE_DECODER_SIZE, tag);

		if (sp == 0u)
			saw_guard = 1;
		else if (want == 0u)
			saw_divide_to_zero = 1;	/* a divide, not the guard */
		if (want == V90SBE_DECODER_SIZE)
			saw_wide = 1;
		if (want < 32u)
			widths_seen |= 1u << want;
	}

	/*
	 * THE GUARD AND THE DIVIDE BOTH REACH WIDTH ZERO, and both were run.
	 * Neither counter proves anything by itself -- the mutation "divide
	 * unconditionally" is what adjudicates, and at a spacing of zero it
	 * raises #DE on our side and not on the blob's.
	 */
	diff_eq_int("the zero-spacing guard was taken", saw_guard, 1, 0);
	diff_eq_int("a divide that yields zero was taken", saw_divide_to_zero,
		    1, 0);
	diff_eq_int("the full six-wide case was taken", saw_wide, 1, 0);

	/* 6, 3, 2, 1 and 0 -- five distinct widths, so the map is exercised. */
	diff_eq_int("five distinct widths were produced",
		    (widths_seen & ((1u << 0) | (1u << 1) | (1u << 2) |
				    (1u << 3) | (1u << 6))) ==
		    ((1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 6)),
		    1, 0);

	return diff_end();
}

int
main(void)
{
	return run_sbe_reset();
}

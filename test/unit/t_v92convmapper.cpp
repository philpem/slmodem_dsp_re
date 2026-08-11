/*
 * t_v92convmapper.cpp -- differential test of V92ConvolutionEncoder and
 * V92Mapper: the two empty constructor and destructor pairs, V92Mapper's
 * table, `reset` and `process`, and V92ConvolutionEncoder's two static
 * tables, `makeStateTtransitionTable`, `reset`, `inverseMap` and `process`.
 *
 * THREE THINGS THE SECOND HALF OF THIS FILE IS ORGANISED AROUND.
 *
 * The builder's arms are compared as WHOLE 0x2008 OBJECTS, per mode, which is
 * the strongest assertion available: 244, 504 and 1,024 written entries of
 * each array for modes 0, 1 and 2, plus the untouched remainder, which is a
 * real check that nothing was cleared and a deliberate no-op otherwise.
 *
 * `process` IS A CODER, so it is driven as a sequence of 240 symbols with the
 * whole object compared after every one -- a divergence at symbol 40 that
 * self-corrects by symbol 60 is still a defect, and a comparison of the final
 * state would miss it.
 *
 * AND `inverseMap` IS SWEPT EXHAUSTIVELY over what it can see: its result
 * depends on its four inputs only through `((x + 2) % 4 + 4) % 4`, so 256
 * residue combinations is the whole domain, and each is driven at thirteen
 * multiples of four either side of zero because the negative arms of that
 * idiom are the only place it could differ from a plain `%`.
 *
 * WHAT IS BEING CLAIMED.  Each of these four symbols is one byte in the blob
 * -- a `ret` -- and the reason they exist at all is that GCC emits an
 * out-of-line constructor or destructor only for a USER-DECLARED one.  So the
 * claim this file carries is not "they do X" but "the original declared them,
 * and they do NOTHING", and the check is the exact converse of every other
 * fixture here: not that the call changed something, but that it changed
 * nothing at all, over the whole object and a guard past its end, on both
 * sides.
 *
 * FINDINGS 223 AND 224 STILL APPLY, and they are why the seeds matter more
 * here rather than less.  A comparison of memory neither side wrote proves
 * nothing, so the slots are filled with varied non-zero bytes that differ on
 * every trial, and the test asserts that the fill did vary -- a fixture that
 * silently stopped seeding would go on passing for the wrong reason.  With
 * live bytes underneath, a constructor that wrote a single zero anywhere in
 * V92ConvolutionEncoder's 8,200 fails.
 *
 * NEITHER ALLOCATES AND NEITHER FREES: `harness_alloc` is asserted empty
 * across the pair, which is what a destructor quietly releasing something --
 * or a constructor acquiring it -- would break.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V92ConvolutionEncoder.h"
#include "dsplib/V92Mapper.h"

extern "C" {
void our_conv_ctor(void *self) asm("_ZN21V92ConvolutionEncoderC1Ev");
void our_conv_ctor2(void *self) asm("_ZN21V92ConvolutionEncoderC2Ev");
void our_conv_dtor(void *self) asm("_ZN21V92ConvolutionEncoderD1Ev");
void our_conv_dtor2(void *self) asm("_ZN21V92ConvolutionEncoderD2Ev");
void ref_conv_ctor(void *self) asm("ref__ZN21V92ConvolutionEncoderC1Ev");
void ref_conv_ctor2(void *self) asm("ref__ZN21V92ConvolutionEncoderC2Ev");
void ref_conv_dtor(void *self) asm("ref__ZN21V92ConvolutionEncoderD1Ev");
void ref_conv_dtor2(void *self) asm("ref__ZN21V92ConvolutionEncoderD2Ev");

void our_map_ctor(void *self) asm("_ZN9V92MapperC1Ev");
void our_map_ctor2(void *self) asm("_ZN9V92MapperC2Ev");
void our_map_dtor(void *self) asm("_ZN9V92MapperD1Ev");
void our_map_dtor2(void *self) asm("_ZN9V92MapperD2Ev");
void ref_map_ctor(void *self) asm("ref__ZN9V92MapperC1Ev");
void ref_map_ctor2(void *self) asm("ref__ZN9V92MapperC2Ev");
void ref_map_dtor(void *self) asm("ref__ZN9V92MapperD1Ev");
void ref_map_dtor2(void *self) asm("ref__ZN9V92MapperD2Ev");

/*
 * `this` is the first STACK argument -- these are plain cdecl and not
 * thiscall (finding 215) -- so a free function of the right shape reaches
 * them, and the return type `process` actually has is what the fixture
 * declares: the object ends `fistps` into a 16-bit slot and sign-extends
 * with `cwtl`, so the whole of %eax is meaningful and is compared.
 */
void our_map_reset(void *self, short scale, unsigned char mode)
	asm("_ZN9V92Mapper5resetEsh");
void ref_map_reset(void *self, short scale, unsigned char mode)
	asm("ref__ZN9V92Mapper5resetEsh");
int our_map_process(void *self, unsigned char *bits)
	asm("_ZN9V92Mapper7processEPh");
int ref_map_process(void *self, unsigned char *bits)
	asm("ref__ZN9V92Mapper7processEPh");

/*
 * V92ConvolutionEncoder's four processing members, reached the same way: all
 * four take `this` as the first stack argument.
 */
void our_conv_maketable(void *self)
	asm("_ZN21V92ConvolutionEncoder25makeStateTtransitionTableEv");
void ref_conv_maketable(void *self)
	asm("ref__ZN21V92ConvolutionEncoder25makeStateTtransitionTableEv");
void our_conv_reset(void *self, int mode)
	asm("_ZN21V92ConvolutionEncoder5resetEi");
void ref_conv_reset(void *self, int mode)
	asm("ref__ZN21V92ConvolutionEncoder5resetEi");
int our_conv_inversemap(void *self, int *in)
	asm("_ZN21V92ConvolutionEncoder10inverseMapEPi");
int ref_conv_inversemap(void *self, int *in)
	asm("ref__ZN21V92ConvolutionEncoder10inverseMapEPi");
int our_conv_process(void *self, int *in)
	asm("_ZN21V92ConvolutionEncoder7processEPi");
int ref_conv_process(void *self, int *in)
	asm("ref__ZN21V92ConvolutionEncoder7processEPi");

/* The blob's own copy of the static table, for a word-for-word comparison. */
extern int ref_map_table[16] asm("ref__ZN9V92Mapper21constelAmplitudeTableE");
extern int ref_conv_coset[64]
	asm("ref__ZN21V92ConvolutionEncoder14cosetMapping4DE");
extern int ref_conv_subset[16]
	asm("ref__ZN21V92ConvolutionEncoder16subsetLabelTableE");
}

/* The sizes are the ones the callers' sysdep_malloc measures; see the two
 * headers.  The slots add a guard past the end. */
#define CONV_SIZE	0x2008
#define CONV_SLOT	0x2018
#define MAP_SIZE	0x2c
#define MAP_SLOT	0x3c

typedef char conv_size_check[(sizeof(V92ConvolutionEncoder) == CONV_SIZE)
			      ? 1 : -1];
typedef char map_size_check[(sizeof(V92Mapper) == MAP_SIZE) ? 1 : -1];

static unsigned char ours[CONV_SLOT] __attribute__((aligned(8)));
static unsigned char theirs[CONV_SLOT] __attribute__((aligned(8)));
static unsigned char before[CONV_SLOT];

#define NTRIAL 6

static void
seed(int slot, int trial)
{
	unsigned lfsr = 0x51a3u + 0x6d2fu * (unsigned)trial;
	int i;

	for (i = 0; i < slot; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		v = (unsigned char)((lfsr >> 2) | 0x41u);
		ours[i] = v;
		theirs[i] = v;
		before[i] = v;
	}
}

/*
 * One trial: seed, construct, check nothing moved, destroy, check nothing
 * moved again.  `first` is the seed of the previous trial's first byte, so
 * the run can assert the fixture is still varying.
 */
static void
trial_run(const char *what, int slot, int size, int trial,
	  void (*ctor)(void *), void (*rctor)(void *),
	  void (*dtor)(void *), void (*rdtor)(void *))
{
	harness_alloc_reset();

	ctor(ours);
	rctor(theirs);

	diff_eq_int("the constructor wrote nothing, ours (trial %ld)",
		    memcmp(ours, before, slot) == 0, 1, trial);
	diff_eq_int("the constructor wrote nothing, the blob's (trial %ld)",
		    memcmp(theirs, before, slot) == 0, 1, trial);
	diff_eq_obj_(__FILE__, __LINE__, what, "object", ours, theirs, size,
		     (long)trial);
	diff_eq_int("nor past the object (trial %ld)",
		    memcmp(ours + size, theirs + size, slot - size) == 0, 1,
		    trial);

	dtor(ours);
	rdtor(theirs);

	diff_eq_int("the destructor wrote nothing, ours (trial %ld)",
		    memcmp(ours, before, slot) == 0, 1, trial);
	diff_eq_int("the destructor wrote nothing, the blob's (trial %ld)",
		    memcmp(theirs, before, slot) == 0, 1, trial);

	diff_eq_int("nothing was allocated (trial %ld)",
		    harness_alloc.allocs, 0, trial);
	diff_eq_int("nothing was freed (trial %ld)", harness_alloc.frees, 0,
		    trial);
	diff_eq_int("and no null free either (trial %ld)",
		    harness_alloc.free_null, 0, trial);
}

static int
run_conv(void)
{
	unsigned char first[NTRIAL];
	int trial, varied = 0;

	diff_begin("V92ConvolutionEncoder::V92ConvolutionEncoder / ~");

	for (trial = 0; trial < NTRIAL; trial++) {
		seed(CONV_SLOT, trial);
		first[trial] = before[0];
		if (trial > 0 && first[trial] != first[trial - 1])
			varied = 1;

		if (trial & 1)
			trial_run("after construction", CONV_SLOT, CONV_SIZE,
				  trial, our_conv_ctor2, ref_conv_ctor2,
				  our_conv_dtor2, ref_conv_dtor2);
		else
			trial_run("after construction", CONV_SLOT, CONV_SIZE,
				  trial, our_conv_ctor, ref_conv_ctor,
				  our_conv_dtor, ref_conv_dtor);
	}

	/* The fixture is still seeding, so "nothing changed" is a statement
	 * about live bytes and not about a constant fill. */
	diff_eq_int("the seed varies across trials (%ld)", varied, 1, 0);

	return diff_end();
}

static int
run_mapper(void)
{
	int trial, varied = 0;
	unsigned char first[NTRIAL];

	diff_begin("V92Mapper::V92Mapper / ~V92Mapper");

	for (trial = 0; trial < NTRIAL; trial++) {
		seed(MAP_SLOT, trial + 16);
		first[trial] = before[0];
		if (trial > 0 && first[trial] != first[trial - 1])
			varied = 1;

		if (trial & 1)
			trial_run("after construction", MAP_SLOT, MAP_SIZE,
				  trial, our_map_ctor2, ref_map_ctor2,
				  our_map_dtor2, ref_map_dtor2);
		else
			trial_run("after construction", MAP_SLOT, MAP_SIZE,
				  trial, our_map_ctor, ref_map_ctor,
				  our_map_dtor, ref_map_dtor);
	}

	diff_eq_int("the seed varies across trials (%ld)", varied, 1, 0);

	return diff_end();
}

/*
 * The static table, word for word against the blob's own copy.
 *
 * Nothing else here can see it: `process` reads at most sixteen of its
 * entries and only through an index the caller cannot make arbitrary, so a
 * wrong entry outside the reachable set would survive every call below.  The
 * blob's array is a defined data symbol with a `ref_` alias, so the honest
 * check is the direct one.
 */
static int
run_mapper_table(void)
{
	int i;

	diff_begin("V92Mapper::constelAmplitudeTable");

	for (i = 0; i < 16; i++)
		diff_eq_int("constelAmplitudeTable[%ld]",
			    V92Mapper::constelAmplitudeTable[i],
			    ref_map_table[i], i);

	/* Not a constant array: two entries differing is enough to say the
	 * comparison above is looking at something. */
	diff_eq_int("the table is not uniform (%ld)",
		    V92Mapper::constelAmplitudeTable[1]
		    != V92Mapper::constelAmplitudeTable[2], 1, 0);

	return diff_end();
}

/*
 * V92Mapper::reset -- both arguments swept, including `mode` values no caller
 * produces, because the object's test is `test %al,%al` and every non-zero
 * byte takes the same arm.
 */
static int
run_mapper_reset(void)
{
	static const short scales[] = { 0, 1, -1, 300, 4096, 32767, -32768 };
	static const unsigned char modes[] = { 0, 1, 2, 7, 128, 255 };
	unsigned sig[42];
	int si, mi, trial = 0, varied = 0;

	diff_begin("V92Mapper::reset(short, unsigned char)");

	for (si = 0; si < (int)(sizeof(scales) / sizeof(scales[0])); si++) {
		for (mi = 0; mi < (int)(sizeof(modes) / sizeof(modes[0]));
		     mi++) {
			int i;
			unsigned s = 0;

			seed(MAP_SLOT, trial + 64);

			our_map_reset(ours, scales[si], modes[mi]);
			ref_map_reset(theirs, scales[si], modes[mi]);

			diff_eq_obj_(__FILE__, __LINE__, "after reset",
				     "V92Mapper", ours, theirs, MAP_SIZE,
				     (long)trial);
			diff_eq_int("reset wrote nothing past the object "
				    "(trial %ld)",
				    memcmp(ours + MAP_SIZE, theirs + MAP_SIZE,
					   MAP_SLOT - MAP_SIZE) == 0, 1, trial);
			diff_eq_int("reset changed the object (trial %ld)",
				    memcmp(ours, before, MAP_SIZE) != 0, 1,
				    trial);

			for (i = 0; i < MAP_SIZE; i++)
				s = s * 33u + ours[i];
			sig[trial] = s;
			if (trial > 0 && sig[trial] != sig[trial - 1])
				varied = 1;
			trial++;
		}
	}

	/* Not the same object every time -- findings 223 and 224. */
	diff_eq_int("reset does not produce one fixed object (%ld)", varied, 1,
		    0);

	return diff_end();
}

/*
 * V92Mapper::process -- the bit packer, the table lookup and the x87 chain.
 *
 * THE CASES ARE CHOSEN SO THE INDEX STAYS INSIDE THE TABLE, because the two
 * sides read two different copies of it and an out-of-range index would
 * compare whatever happens to follow each.  That is also what makes the
 * sixteen-bit accumulator testable: `bits` = 17 with only bit 16 set gives an
 * index of zero once truncated and 65,536 without, so the correct code is in
 * range and a wider accumulator is not.
 *
 * `power` is driven negative and zero on purpose.  The object's `fsqrt` has
 * no branch in front of it, so a reconstruction calling libm would take an
 * errno path and could return a different NaN; here both sides run the
 * instruction and both end at the same `fistps` result.
 */
#define MAP_IN 32

enum {
	PAT_ZEROS = 0,
	PAT_ONES,
	PAT_ALTERNATING,
	PAT_PSEUDORANDOM,
	PAT_TOPBIT,		/* only index `bits` - 1 set */
	PAT_TOPBIT_AND_LOW	/* index `bits` - 1 and index 0 */
};

struct mapcase {
	short scale;
	unsigned char mode;
	float power;
	short bits;
	int pattern;
};

static const struct mapcase mapcases[] = {
	/* The two configurations `reset` installs, over every input. */
	{     1, 0,   5.0f,  2, PAT_ZEROS },
	{     1, 0,   5.0f,  2, PAT_ONES },
	{     1, 0,   5.0f,  2, PAT_ALTERNATING },
	{     1, 0,   5.0f,  2, PAT_PSEUDORANDOM },
	{     1, 1,  21.0f,  3, PAT_ZEROS },
	{     1, 1,  21.0f,  3, PAT_ONES },
	{     1, 1,  21.0f,  3, PAT_ALTERNATING },
	{     1, 1,  21.0f,  3, PAT_PSEUDORANDOM },

	/* The scale, which is where the result's sign and magnitude come
	 * from; 32767 with the widest quotient overflows the `fistps`. */
	{   100, 1,  21.0f,  3, PAT_ONES },
	{  -100, 1,  21.0f,  3, PAT_ONES },
	{ 32767, 1,  21.0f,  3, PAT_ONES },
	{-32768, 1,  21.0f,  3, PAT_ALTERNATING },
	{     0, 1,  21.0f,  3, PAT_ONES },

	/* Powers no `reset` produces. */
	{   100, 0,   1.0f,  2, PAT_ONES },
	{   100, 0,   0.0f,  2, PAT_ONES },
	{   100, 0,  -4.0f,  2, PAT_ONES },
	{   100, 1, 1e-30f,  3, PAT_ONES },
	{   100, 1,  1e30f,  3, PAT_ONES },

	/* Tap counts no `reset` produces: none at all, one, and four that
	 * overflow the accumulator. */
	{   100, 0,   5.0f,  0, PAT_ONES },
	{   100, 0,   5.0f, -1, PAT_ONES },
	{   100, 0,   5.0f,  1, PAT_ONES },
	{   100, 1,  21.0f, 17, PAT_TOPBIT },
	{   100, 1,  21.0f, 18, PAT_TOPBIT_AND_LOW },
	{   100, 0,   5.0f, 20, PAT_TOPBIT },
	{   100, 0,   5.0f, 24, PAT_TOPBIT_AND_LOW }
};

static void
fill_input(unsigned char *buf, const struct mapcase *c, int trial)
{
	unsigned lfsr = 0x2f81u + 0x1d0fu * (unsigned)trial;
	int i;
	int n = c->bits > 0 ? c->bits : 0;

	/* The bytes the call cannot reach are seeded too, so that reading one
	 * of them shows up rather than reading a zero. */
	for (i = 0; i < MAP_IN; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		buf[i] = (unsigned char)(lfsr >> 3);
	}

	for (i = 0; i < n && i < MAP_IN; i++) {
		switch (c->pattern) {
		case PAT_ZEROS:
			buf[i] = 0;
			break;
		case PAT_ONES:
			buf[i] = 1;
			break;
		case PAT_ALTERNATING:
			buf[i] = (unsigned char)(i & 1);
			break;
		case PAT_PSEUDORANDOM:
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
			buf[i] = (unsigned char)((lfsr >> 5) & 1u);
			break;
		case PAT_TOPBIT:
			buf[i] = (unsigned char)(i == n - 1);
			break;
		case PAT_TOPBIT_AND_LOW:
			buf[i] = (unsigned char)(i == n - 1 || i == 0);
			break;
		}
	}
}

static int
run_mapper_process(void)
{
	unsigned char ourin[MAP_IN], theirin[MAP_IN];
	unsigned char snap[MAP_SLOT];
	int ncase = (int)(sizeof(mapcases) / sizeof(mapcases[0]));
	int trial, varied = 0, nonzero = 0;
	int prev = 0;

	diff_begin("V92Mapper::process(unsigned char *)");

	for (trial = 0; trial < ncase; trial++) {
		const struct mapcase *c = &mapcases[trial];
		V92Mapper *a = (V92Mapper *)(void *)ours;
		V92Mapper *b = (V92Mapper *)(void *)theirs;
		int ra, rb;

		seed(MAP_SLOT, trial + 128);

		/*
		 * Everything else in the object keeps its seed: `process`
		 * reads four fields and the thirty-five bytes at +0x03 are
		 * not among them.
		 */
		a->scale = b->scale = c->scale;
		a->mode = b->mode = c->mode;
		a->power = b->power = c->power;
		a->bits = b->bits = c->bits;
		memcpy(snap, ours, MAP_SLOT);

		fill_input(ourin, c, trial);
		memcpy(theirin, ourin, MAP_IN);

		ra = our_map_process(ours, ourin);
		rb = ref_map_process(theirs, theirin);

		diff_eq_int("process returned the same (trial %ld)", ra, rb,
			    trial);
		diff_eq_obj_(__FILE__, __LINE__, "after process", "V92Mapper",
			     ours, theirs, MAP_SIZE, (long)trial);
		diff_eq_int("process wrote nothing to the object (trial %ld)",
			    memcmp(ours, snap, MAP_SLOT) == 0, 1, trial);
		diff_eq_int("nor to the blob's object (trial %ld)",
			    memcmp(theirs, snap, MAP_SLOT) == 0, 1, trial);
		diff_eq_int("process wrote nothing to its input (trial %ld)",
			    memcmp(ourin, theirin, MAP_IN) == 0, 1, trial);

		if (ra != 0)
			nonzero = 1;
		if (trial > 0 && ra != prev)
			varied = 1;
		prev = ra;
	}

	/* Neither always zero nor always the same -- findings 223 and 224. */
	diff_eq_int("process returns something (%ld)", nonzero, 1, 0);
	diff_eq_int("process does not return one fixed value (%ld)", varied, 1,
		    0);

	return diff_end();
}

/* ---------------------------------------------------------------------- *
 *  V92ConvolutionEncoder: the table builder, reset, inverseMap, process.
 * ---------------------------------------------------------------------- */

/*
 * The three modes both switches name, and four they do not.  An unnamed mode
 * is a real case and not a curiosity: the builder's switch has no arm for it
 * and writes nothing, which is checkable, whereas `process`'s switch leaves
 * its index uninitialised, which is NOT -- see the note above
 * `run_conv_process` and docs/deviations.md D262.
 */
static const int conv_modes[3] = { 0, 1, 2 };
static const int conv_unnamed[4] = { 3, -1, 7, 0x10000 };

/* A snapshot taken after `mode` is planted, so "changed" excludes the plant. */
static unsigned char conv_snap[CONV_SLOT];

static unsigned
conv_sig(const unsigned char *p, int n)
{
	unsigned s = 0;
	int i;

	for (i = 0; i < n; i++)
		s = s * 33u + p[i];
	return s;
}

/*
 * The two static tables, word for word against the blob's own copies.
 *
 * The sweep in `run_conv_inversemap` does reach every entry of both -- the
 * four residues are 0..3 so all sixteen `subsetLabelTable` indices occur, and
 * its values cover 0..7 so all sixty-four `8 * a + b` do too -- but that is an
 * argument, and the direct comparison is a measurement.  Both are here.
 */
static int
run_conv_tables(void)
{
	int i;

	diff_begin("V92ConvolutionEncoder::cosetMapping4D / subsetLabelTable");

	for (i = 0; i < 64; i++)
		diff_eq_int("cosetMapping4D[%ld]",
			    V92ConvolutionEncoder::cosetMapping4D[i],
			    ref_conv_coset[i], i);
	for (i = 0; i < 16; i++)
		diff_eq_int("subsetLabelTable[%ld]",
			    V92ConvolutionEncoder::subsetLabelTable[i],
			    ref_conv_subset[i], i);

	/* Neither is a constant fill, so the loops above are looking at
	 * something -- findings 223 and 224 applied to a table. */
	diff_eq_int("cosetMapping4D is not uniform (%ld)",
		    V92ConvolutionEncoder::cosetMapping4D[2]
		    != V92ConvolutionEncoder::cosetMapping4D[4], 1, 0);
	diff_eq_int("subsetLabelTable is not uniform (%ld)",
		    V92ConvolutionEncoder::subsetLabelTable[0]
		    != V92ConvolutionEncoder::subsetLabelTable[1], 1, 0);

	return diff_end();
}

/*
 * The table builder, and the strongest assertion in this file: the whole
 * 0x2008 object diffed against the blob's after every call.
 *
 * WHAT THAT DOES AND DOES NOT COVER.  Nothing in the object clears either
 * array, and modes 0 and 1 write only the first four and eight columns of
 * each sixteen-wide row, so the compare is a real check over the written
 * subset -- 244, 504 and 1,024 entries of each array for modes 0, 1 and 2 --
 * and a vacuous one over the rest, where both sides still hold the identical
 * seed.  It is stated that way rather than as "8 KB of coverage".  The seed
 * is what makes even the vacuous part worth having: a reconstruction that
 * cleared the arrays, or wrote one slot the object leaves alone, fails here.
 *
 * Each mode is run at several seeds and the seeds are compared across trials,
 * and the three modes are compared against each other at a fixed seed, so
 * neither "the fixture stopped seeding" nor "the mode is being ignored" can
 * pass.
 */
static int
run_conv_maketable(void)
{
	unsigned sig[3][NTRIAL];
	int m, trial, varied_seed = 0, varied_mode = 1;

	diff_begin("V92ConvolutionEncoder::makeStateTtransitionTable()");

	for (trial = 0; trial < NTRIAL; trial++) {
		for (m = 0; m < 3; m++) {
			V92ConvolutionEncoder *a =
				(V92ConvolutionEncoder *)(void *)ours;
			V92ConvolutionEncoder *b =
				(V92ConvolutionEncoder *)(void *)theirs;
			long mode = conv_modes[m];

			/* The same seed for all three modes of a trial, so a
			 * difference between them is the mode's and not the
			 * seed's. */
			seed(CONV_SLOT, 200 + trial);
			a->mode = b->mode = conv_modes[m];
			memcpy(conv_snap, ours, CONV_SLOT);

			our_conv_maketable(ours);
			ref_conv_maketable(theirs);

			diff_eq_obj_(__FILE__, __LINE__,
				     "after makeStateTtransitionTable",
				     "V92ConvolutionEncoder", ours, theirs,
				     CONV_SIZE, mode);
			diff_eq_int("nor past the object (mode %ld)",
				    memcmp(ours + CONV_SIZE, theirs + CONV_SIZE,
					   CONV_SLOT - CONV_SIZE) == 0, 1, mode);
			diff_eq_int("the builder changed the object (mode %ld)",
				    memcmp(ours, conv_snap, CONV_SIZE) != 0, 1,
				    mode);
			diff_eq_int("and touched neither mode nor state "
				    "(mode %ld)",
				    memcmp(ours, conv_snap, 8) == 0, 1, mode);

			sig[m][trial] = conv_sig(ours, CONV_SIZE);
		}

		if (sig[0][trial] == sig[1][trial]
		    || sig[1][trial] == sig[2][trial]
		    || sig[0][trial] == sig[2][trial])
			varied_mode = 0;
		if (trial > 0 && sig[0][trial] != sig[0][trial - 1])
			varied_seed = 1;
	}

	diff_eq_int("the three modes build three different tables (%ld)",
		    varied_mode, 1, 0);
	diff_eq_int("and the untouched bytes are still live (%ld)", varied_seed,
		    1, 0);

	/*
	 * A mode the switch does not name.  The builder has no arm for it, so
	 * it writes NOTHING -- not the arrays, not `mode`, not `state` -- and
	 * the seeded slot must come back byte for byte on BOTH sides.  With
	 * live bytes underneath that is a real check and not a vacuous one.
	 */
	for (m = 0; m < 4; m++) {
		V92ConvolutionEncoder *a = (V92ConvolutionEncoder *)(void *)ours;
		V92ConvolutionEncoder *b =
			(V92ConvolutionEncoder *)(void *)theirs;
		long mode = conv_unnamed[m];

		seed(CONV_SLOT, 300 + m);
		a->mode = b->mode = conv_unnamed[m];
		memcpy(conv_snap, ours, CONV_SLOT);

		our_conv_maketable(ours);
		ref_conv_maketable(theirs);

		diff_eq_obj_(__FILE__, __LINE__, "after an unnamed mode",
			     "V92ConvolutionEncoder", ours, theirs, CONV_SIZE,
			     mode);
		diff_eq_int("an unnamed mode writes nothing, ours (mode %ld)",
			    memcmp(ours, conv_snap, CONV_SLOT) == 0, 1, mode);
		diff_eq_int("nor the blob's (mode %ld)",
			    memcmp(theirs, conv_snap, CONV_SLOT) == 0, 1, mode);
	}

	return diff_end();
}

/*
 * `reset` -- three statements whose ORDER is the claim.  Two of the three are
 * observable from outside: `mode` must be stored before the builder runs, or
 * the tables come out of the previous mode, and that is what the per-mode
 * table comparison below catches.  `state = 0` after the call rather than
 * before is NOT observable -- the builder never reads +0x04 -- and is
 * recorded as such in test/mutations/v92convenc.json rather than claimed
 * here.
 */
static int
run_conv_reset(void)
{
	int m, trial, varied = 0;
	unsigned prev = 0;

	diff_begin("V92ConvolutionEncoder::reset(int)");

	for (trial = 0; trial < NTRIAL; trial++) {
		for (m = 0; m < 7; m++) {
			V92ConvolutionEncoder *a =
				(V92ConvolutionEncoder *)(void *)ours;
			long mode = m < 3 ? conv_modes[m] : conv_unnamed[m - 3];
			unsigned s;

			seed(CONV_SLOT, 400 + trial * 8 + m);
			memcpy(conv_snap, ours, CONV_SLOT);

			our_conv_reset(ours, (int)mode);
			ref_conv_reset(theirs, (int)mode);

			diff_eq_obj_(__FILE__, __LINE__, "after reset",
				     "V92ConvolutionEncoder", ours, theirs,
				     CONV_SIZE, mode);
			diff_eq_int("nor past the object (mode %ld)",
				    memcmp(ours + CONV_SIZE, theirs + CONV_SIZE,
					   CONV_SLOT - CONV_SIZE) == 0, 1, mode);
			diff_eq_int("reset stored the mode (%ld)", a->mode, mode,
				    mode);
			diff_eq_int("reset zeroed the state (mode %ld)",
				    a->state, 0, mode);

			if (m < 3)
				diff_eq_int("reset rebuilt the tables "
					    "(mode %ld)",
					    memcmp(ours + 8, conv_snap + 8,
						   CONV_SIZE - 8) != 0, 1, mode);
			else
				diff_eq_int("an unnamed mode leaves both tables "
					    "alone (mode %ld)",
					    memcmp(ours + 8, conv_snap + 8,
						   CONV_SIZE - 8) == 0, 1, mode);

			s = conv_sig(ours, CONV_SIZE);
			if ((trial || m) && s != prev)
				varied = 1;
			prev = s;
		}
	}

	diff_eq_int("reset does not produce one fixed object (%ld)", varied, 1,
		    0);

	return diff_end();
}

/*
 * `inverseMap` -- four positive-modulo reductions and two table lookups.
 *
 * THE SWEEP IS EXHAUSTIVE OVER WHAT THE FUNCTION CAN SEE.  The result depends
 * on the four inputs only through `((x + 2) % 4 + 4) % 4`, so 256 residue
 * combinations is the whole domain; each is driven at thirteen offsets of
 * four, positive and negative, which is what makes the two `js` arms of each
 * reduction -- the only place where the idiom and a plain `%` differ -- carry
 * the same 256 answers as the positive path.
 *
 * NOTHING NEAR INT_MAX.  `in[k] + 2` overflowing is undefined in the
 * reconstruction and merely wraps in the blob, so a value the original could
 * never see could legitimately disagree; +-400,002 is as large as this goes
 * and the widest offset used is 100,000.
 */
#define CONV_NOFF 13

static int
run_conv_inversemap(void)
{
	static const int offs[CONV_NOFF] = {
		0, 1, -1, 2, -2, 3, -3, 250, -250, 1000, -1000, 100000, -100000
	};
	unsigned lfsr = 0x3d97u;
	int seen[16];
	int oi, c, i, k, ndistinct = 0, trial = 0;

	diff_begin("V92ConvolutionEncoder::inverseMap(int *)");

	for (i = 0; i < 16; i++)
		seen[i] = 0;

	/* The object is seeded once and must survive every call untouched:
	 * `inverseMap` reads two statics and its argument and nothing else. */
	seed(CONV_SLOT, 500);
	memcpy(conv_snap, ours, CONV_SLOT);

	for (oi = 0; oi < CONV_NOFF; oi++) {
		for (c = 0; c < 256; c++) {
			int ourin[4], theirin[4], ra, rb;

			for (k = 0; k < 4; k++) {
				ourin[k] = ((c >> (2 * k)) & 3) - 2
					   + 4 * offs[oi];
				theirin[k] = ourin[k];
			}

			ra = our_conv_inversemap(ours, ourin);
			rb = ref_conv_inversemap(theirs, theirin);

			diff_eq_int("inverseMap returned the same (case %ld)",
				    ra, rb, trial);
			diff_eq_int("inverseMap wrote nothing to its input "
				    "(case %ld)",
				    memcmp(ourin, theirin, sizeof ourin) == 0, 1,
				    trial);

			if (ra >= 0 && ra < 16 && !seen[ra]) {
				seen[ra] = 1;
				ndistinct++;
			}
			trial++;
		}
	}

	/* Values no residue sweep produces on its own: mixed magnitudes, both
	 * signs, and a pseudorandom block over a safe range. */
	for (c = 0; c < 512; c++) {
		int ourin[4], theirin[4], ra, rb;

		for (k = 0; k < 4; k++) {
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
			ourin[k] = (int)(lfsr & 0xffffu) - 0x8000;
			if ((c & 3) == k)
				ourin[k] = (c & 4) ? 0x3fffffff : -0x3fffffff;
			theirin[k] = ourin[k];
		}

		ra = our_conv_inversemap(ours, ourin);
		rb = ref_conv_inversemap(theirs, theirin);

		diff_eq_int("inverseMap returned the same, wide (case %ld)", ra,
			    rb, c);
		diff_eq_int("inverseMap wrote nothing to its input, wide "
			    "(case %ld)",
			    memcmp(ourin, theirin, sizeof ourin) == 0, 1, c);

		if (ra >= 0 && ra < 16 && !seen[ra]) {
			seen[ra] = 1;
			ndistinct++;
		}
	}

	diff_eq_int("inverseMap wrote nothing to the object (%ld)",
		    memcmp(ours, conv_snap, CONV_SLOT) == 0, 1, 0);
	diff_eq_int("nor to the blob's object (%ld)",
		    memcmp(theirs, conv_snap, CONV_SLOT) == 0, 1, 0);

	/* All sixteen values cosetMapping4D holds came back, so the sweep
	 * reached the whole table and not one corner of it. */
	diff_eq_int("every coset label was returned (%ld)", ndistinct, 16, 0);

	return diff_end();
}

/*
 * `process` -- and this is the one that carries `state`, so it is driven as a
 * sequence and compared after EVERY call.  A divergence at symbol 40 that
 * self-corrects by symbol 60 is still a defect, and a comparison of the final
 * object would miss it.
 *
 * EVERY CALL IS ON A `reset`-BUILT OBJECT AND AT A MODE THE SWITCH NAMES, and
 * both are load-bearing.  On a merely seeded object `nextState[...]` returns
 * an arbitrary int and `(ns << 4) + i` leaves the array entirely, so the two
 * sides would read two unrelated pieces of memory; after `reset` the reachable
 * set is closed, because `i` is 0..3, 0..7 or 0..15 exactly as wide as the
 * mode's arm of the builder filled, and every `nextState` value it wrote is a
 * state that arm also filled a row for.  An UNNAMED mode is not driven here at
 * all: the object's default arm stores and returns two registers nothing
 * wrote (D262), so the two sides would be comparing two different pieces of
 * stack -- a difference the fixture created.
 */
#define CONV_SYMS 240

static int
run_conv_process(void)
{
	int m, run, sym, varied = 0, nonzero = 0, states = 0;
	int seenstate[64];
	int prev = -1;

	diff_begin("V92ConvolutionEncoder::process(int *)");

	for (m = 0; m < 3; m++) {
		for (run = 0; run < 4; run++) {
			unsigned lfsr = 0x7c1bu
					+ 0x2b9du * (unsigned)(m * 4 + run);
			V92ConvolutionEncoder *a =
				(V92ConvolutionEncoder *)(void *)ours;

			for (sym = 0; sym < 64; sym++)
				seenstate[sym] = 0;
			states = 0;

			seed(CONV_SLOT, 600 + m * 4 + run);
			our_conv_reset(ours, conv_modes[m]);
			ref_conv_reset(theirs, conv_modes[m]);
			diff_eq_obj_(__FILE__, __LINE__, "after reset",
				     "V92ConvolutionEncoder", ours, theirs,
				     CONV_SIZE, (long)conv_modes[m]);

			/*
			 * A sequence is only worth driving from a state the
			 * two sides agree on.  If `reset` already diverged the
			 * failure is reported above, and going on would index
			 * a table one side never built -- a wild read this
			 * fixture would have created, reported as a
			 * difference.  Stop instead.
			 */
			if (memcmp(ours, theirs, CONV_SIZE) != 0)
				continue;

			for (sym = 0; sym < CONV_SYMS; sym++) {
				int ourin[4], theirin[4], k, ra, rb;

				for (k = 0; k < 4; k++) {
					lfsr = (lfsr >> 1)
					       ^ (-(int)(lfsr & 1u) & 0xb400u);
					/* -3..4: both signs of the modulo
					 * idiom, every residue. */
					ourin[k] = (int)((lfsr >> 3) & 7u) - 3;
					if (run == 1)
						ourin[k] = 0;
					else if (run == 2)
						ourin[k] = (sym + k) & 3;
					theirin[k] = ourin[k];
				}

				ra = our_conv_process(ours, ourin);
				rb = ref_conv_process(theirs, theirin);

				diff_eq_int("process returned the same "
					    "(symbol %ld)", ra, rb, sym);
				diff_eq_obj_(__FILE__, __LINE__,
					     "after process",
					     "V92ConvolutionEncoder", ours,
					     theirs, CONV_SIZE, (long)sym);
				diff_eq_int("nor past the object (symbol %ld)",
					    memcmp(ours + CONV_SIZE,
						   theirs + CONV_SIZE,
						   CONV_SLOT - CONV_SIZE) == 0,
					    1, sym);
				diff_eq_int("process wrote nothing to its "
					    "input (symbol %ld)",
					    memcmp(ourin, theirin,
						   sizeof ourin) == 0, 1, sym);

				if (ra != 0)
					nonzero = 1;
				if (prev >= 0 && ra != prev)
					varied = 1;
				prev = ra;

				if (a->state >= 0 && a->state < 64
				    && !seenstate[a->state]) {
					seenstate[a->state] = 1;
					states++;
				}

				/* Reported at the symbol it happened on; a
				 * diverged coder is not driven further. */
				if (memcmp(ours, theirs, CONV_SIZE) != 0)
					break;
			}

			/* The trellis is being walked, not sat in: a `process`
			 * that never updated +0x04 would show one state. */
			if (run != 1)
				diff_eq_int("the state moved (mode %ld)",
					    states > 4, 1, conv_modes[m]);
		}
	}

	/* Neither always zero nor always the same -- findings 223 and 224.
	 * `process` returns one bit out of `output`, so this is the whole of
	 * its range. */
	diff_eq_int("process returns something (%ld)", nonzero, 1, 0);
	diff_eq_int("process does not return one fixed value (%ld)", varied, 1,
		    0);

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_conv();
	bad |= run_mapper();
	bad |= run_mapper_table();
	bad |= run_mapper_reset();
	bad |= run_mapper_process();

	bad |= run_conv_tables();
	bad |= run_conv_maketable();
	bad |= run_conv_reset();
	bad |= run_conv_inversemap();
	bad |= run_conv_process();

	return bad;
}

/*
 * t_v92convmapper.cpp -- differential test of the two empty constructor and
 * destructor pairs: V92ConvolutionEncoder and V92Mapper.
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

/* The blob's own copy of the static table, for a word-for-word comparison. */
extern int ref_map_table[16] asm("ref__ZN9V92Mapper21constelAmplitudeTableE");
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

int
main(void)
{
	int bad = 0;

	bad |= run_conv();
	bad |= run_mapper();
	bad |= run_mapper_table();
	bad |= run_mapper_reset();
	bad |= run_mapper_process();

	return bad;
}

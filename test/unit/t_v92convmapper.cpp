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

int
main(void)
{
	int bad = 0;

	bad |= run_conv();
	bad |= run_mapper();

	return bad;
}

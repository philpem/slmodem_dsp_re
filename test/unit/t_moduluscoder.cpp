/*
 * t_moduluscoder.cpp -- differential test of the three modulus-coder default
 * constructors against the blob: ModulusEncoder, ModulusDecoder and
 * V92ModulusEncoder.
 *
 * All three do the same kind of thing -- store zeros over part of an object
 * and touch nothing else -- and that is exactly the shape of claim a zeroed
 * fixture cannot check.  So:
 *
 * THE OBJECTS ARE NEVER ZEROED, and the seed is different on every trial.
 * Every byte of every slot is non-zero going in, so "the constructor cleared
 * these thirteen words" and "it left those six alone" are both statements
 * about bytes that were something else beforehand.  A zero fill would make a
 * clear loop that stops a word short, or one that runs a word too far, pass
 * (findings 223, 224).
 *
 * V92ModulusEncoder IS THE ONE WITH A HOLE IN IT.  It zeroes +0x18 through
 * +0x48 and leaves +0x00..+0x14 and +0x4c, +0x50 as it found them, so the
 * test checks the seed on BOTH sides of the cleared range as well as the
 * range itself -- a `memset(this, 0, sizeof *this)` would pass every equality
 * against the reference only if the reference did the same, and it does not.
 *
 * NONE OF THE THREE ALLOCATES.  `harness_alloc.allocs` is asserted at zero,
 * which is what a constructor quietly acquiring something would break.
 *
 * The constructors are called through asm() labels on both sides: C++ has no
 * syntax for running one over storage that already exists, and this build has
 * no <new>.  Both the C1 and the C2 variant are called, because GCC emits the
 * pair from one definition and this file fails to link if it does not.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/ModulusCoder.h"
#include "dsplib/V92ModulusEncoder.h"

extern "C" {
void our_menc(void *self) asm("_ZN14ModulusEncoderC1Ev");
void our_menc2(void *self) asm("_ZN14ModulusEncoderC2Ev");
void ref_menc(void *self) asm("ref__ZN14ModulusEncoderC1Ev");
void ref_menc2(void *self) asm("ref__ZN14ModulusEncoderC2Ev");

void our_mdec(void *self) asm("_ZN14ModulusDecoderC1Ev");
void our_mdec2(void *self) asm("_ZN14ModulusDecoderC2Ev");
void ref_mdec(void *self) asm("ref__ZN14ModulusDecoderC1Ev");
void ref_mdec2(void *self) asm("ref__ZN14ModulusDecoderC2Ev");

void our_v92me(void *self) asm("_ZN17V92ModulusEncoderC1Ev");
void our_v92me2(void *self) asm("_ZN17V92ModulusEncoderC2Ev");
void ref_v92me(void *self) asm("ref__ZN17V92ModulusEncoderC1Ev");
void ref_v92me2(void *self) asm("ref__ZN17V92ModulusEncoderC2Ev");

void our_v92me_reset(void *self, void *params)
	asm("_ZN17V92ModulusEncoder5resetEP16V92MappingParams");
void ref_v92me_reset(void *self, void *params)
	asm("ref__ZN17V92ModulusEncoder5resetEP16V92MappingParams");
}

#define MOD_SIZE	0x1c
#define MOD_SLOT	0x2c
#define V92ME_SIZE	0x54
#define V92ME_SLOT	0x64
#define MAXSLOT		V92ME_SLOT

static unsigned char ours[MAXSLOT] __attribute__((aligned(8)));
static unsigned char theirs[MAXSLOT] __attribute__((aligned(8)));
static unsigned char before[MAXSLOT];

#define NTRIAL 8

/* Never zero, and never the same twice. */
static void
seed(int trial)
{
	unsigned lfsr = 0x2f19u + 0x7c5bu * (unsigned)trial;
	int i;

	for (i = 0; i < MAXSLOT; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		v = (unsigned char)((lfsr >> 5) | 0x81u);
		ours[i] = v;
		theirs[i] = v;
		before[i] = v;
	}
}

/* Every word in [lo, hi) is zero on our side. */
static int
all_zero(int lo, int hi)
{
	int i;

	for (i = lo; i < hi; i++)
		if (ours[i] != 0)
			return 0;
	return 1;
}

static int
run_modulus(const char *name, void (*our1)(void *), void (*our2)(void *),
	    void (*ref1)(void *), void (*ref2)(void *))
{
	int trial;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		seed(trial);
		harness_alloc_reset();

		/* The seed has to be non-zero where the constructor clears,
		 * or the clear is not being tested at all. */
		diff_eq_int("the seed is not already clear (trial %ld)",
			    all_zero(0, MOD_SIZE), 0, trial);

		if (trial & 1) {
			our2(ours);
			ref2(theirs);
		} else {
			our1(ours);
			ref1(theirs);
		}

		diff_eq_obj_(__FILE__, __LINE__, "after construction",
			     "ModulusEncoder", ours, theirs, MOD_SIZE,
			     (long)trial);
		diff_eq_int("no store past the object (trial %ld)",
			    memcmp(ours + MOD_SIZE, theirs + MOD_SIZE,
				   MOD_SLOT - MOD_SIZE) == 0, 1, trial);
		diff_eq_int("and the bytes past it keep their seed"
			    " (trial %ld)",
			    memcmp(ours + MOD_SIZE, before + MOD_SIZE,
				   MOD_SLOT - MOD_SIZE) == 0, 1, trial);
		diff_eq_int("all seven words are zero (trial %ld)",
			    all_zero(0, MOD_SIZE), 1, trial);
		diff_eq_int("it allocated nothing (trial %ld)",
			    harness_alloc.allocs, 0, trial);
	}

	return diff_end();
}

static int
run_v92me(void)
{
	int trial;

	diff_begin("V92ModulusEncoder::V92ModulusEncoder");

	for (trial = 0; trial < NTRIAL; trial++) {
		seed(trial + 32);
		harness_alloc_reset();

		diff_eq_int("the seed is not already clear (trial %ld)",
			    all_zero(0x18, 0x4c), 0, trial);

		if (trial & 1) {
			our_v92me2(ours);
			ref_v92me2(theirs);
		} else {
			our_v92me(ours);
			ref_v92me(theirs);
		}

		diff_eq_obj_(__FILE__, __LINE__, "after construction",
			     "V92ModulusEncoder", ours, theirs, V92ME_SIZE,
			     (long)trial);
		diff_eq_int("no store past the object (trial %ld)",
			    memcmp(ours + V92ME_SIZE, theirs + V92ME_SIZE,
				   V92ME_SLOT - V92ME_SIZE) == 0, 1, trial);

		/* The cleared range, and the two regions either side of it
		 * that the constructor must not have touched. */
		diff_eq_int("+0x18 .. +0x48 is clear (trial %ld)",
			    all_zero(0x18, 0x4c), 1, trial);
		diff_eq_int("+0x00 .. +0x14 keeps its seed (trial %ld)",
			    memcmp(ours, before, 0x18) == 0, 1, trial);
		diff_eq_int("+0x4c and +0x50 keep their seed (trial %ld)",
			    memcmp(ours + 0x4c, before + 0x4c,
				   V92ME_SLOT - 0x4c) == 0, 1, trial);
		diff_eq_int("it allocated nothing (trial %ld)",
			    harness_alloc.allocs, 0, trial);
	}

	return diff_end();
}

/*
 * V92ModulusEncoder::reset -- the parameter block in, the product out.
 *
 * WHAT THE SWEEP HAS TO REACH.  `reset` multiplies twelve moduli together in
 * 64 bits and then decides, from whether that product ran into the sign bit,
 * how to split it across two 63-bit limbs.  So the interesting axis is not
 * the individual moduli but the SIZE of their product, and the sets below
 * walk it from 1 (no product at all) through a product that fits in 63 bits
 * with room to spare, one that fits exactly, one that overflows by a little
 * and several that overflow by a lot.  A set of random 32-bit words reaches
 * only the last of those, which is why the table is written out rather than
 * generated.
 *
 * A ZERO MODULUS IS INCLUDED and is safe: `reset` only multiplies.  It is
 * `progress` that divides, and its fixture keeps every modulus at 1 or more.
 *
 * THE PARAMETER BLOCK IS SEEDED NON-ZERO EVERYWHERE, and the thirteen words
 * `reset` is supposed to read are then written over that seed, so a copy
 * from the wrong offset lands on a seed byte and shows up as a difference
 * rather than as a zero that happens to match.  It is also compared before
 * and after, because `reset` must not write to it.
 */

#define PBLOCK_SIZE	0xb4

static unsigned char pblock[PBLOCK_SIZE] __attribute__((aligned(8)));
static unsigned char pblock_before[PBLOCK_SIZE];

static const unsigned int modsets[][12] = {
	/* product 1: the loop gives up at once and both limbs come out 0. */
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	/* 2^12, and 4^12: comfortably inside 63 bits. */
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 },
	/* 2^60 and 2^62: inside 63 bits, but only just. */
	{ 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 },
	{ 64, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 64 },
	/* 2^63 and 2^64 exactly: the first bit into and past the sign. */
	{ 64, 64, 32, 32, 32, 32, 32, 32, 32, 32, 32, 64 },
	{ 64, 64, 64, 32, 32, 32, 32, 32, 32, 32, 32, 64 },
	/* Uneven, and reaching over 63 bits by a factor of a few. */
	{ 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29 },
	{ 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60 },
	{ 12, 12, 14, 14, 16, 16, 18, 18, 20, 20, 22, 22 },
	/* Lopsided: all of the magnitude in one half of the twelve. */
	{ 1, 1, 1, 1, 1, 1, 1000, 1000, 1000, 1000, 1000, 1000 },
	{ 1000, 1000, 1000, 1000, 1000, 1000, 1, 1, 1, 1, 1, 1 },
	/* A zero, and a lone huge word. */
	{ 6, 6, 6, 0, 6, 6, 6, 6, 6, 6, 6, 6 },
	{ 0xffffffffu, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
	/* Wide and pseudorandom: the product wraps several times over. */
	{ 0x9e3779b9u, 0x85ebca6bu, 0xc2b2ae35u, 0x27d4eb2fu, 0x165667b1u,
	  0x1b873593u, 0xcc9e2d51u, 0x2545f491u, 0x61c88647u, 0x3b9aca07u,
	  0x7feb352du, 0x846ca68bu },
};

#define NMODSET	((int)(sizeof modsets / sizeof modsets[0]))

/* The bit counts swept alongside them; reset only copies this one. */
static const unsigned int nbits_set[] = { 0, 1, 62, 63, 64, 65, 80, 0xffffffffu };

#define NNBITS	((int)(sizeof nbits_set / sizeof nbits_set[0]))

static void
put32(unsigned char *p, int off, unsigned int v)
{
	memcpy(p + off, &v, sizeof v);
}

/* Seed every byte, then lay the thirteen words reset reads over the top. */
static void
fill_params(int trial)
{
	unsigned lfsr = 0x1234u + 0x51edu * (unsigned)trial;
	int i;

	for (i = 0; i < PBLOCK_SIZE; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		pblock[i] = (unsigned char)((lfsr >> 3) | 0x41u);
	}

	put32(pblock, 0x00, nbits_set[trial % NNBITS]);
	for (i = 0; i < 12; i++)
		put32(pblock, 0x1c + 4 * i, modsets[trial % NMODSET][i]);

	memcpy(pblock_before, pblock, PBLOCK_SIZE);
}

static int
run_v92me_reset(void)
{
	unsigned char first[V92ME_SIZE];
	int differed = 0;
	int trial;

	diff_begin("V92ModulusEncoder::reset");

	/* NMODSET and NNBITS are coprime, so this walks every pairing. */
	for (trial = 0; trial < NMODSET * NNBITS; trial++) {
		seed(trial + 64);
		fill_params(trial);
		harness_alloc_reset();

		our_v92me_reset(ours, pblock);
		ref_v92me_reset(theirs, pblock);

		diff_eq_obj_(__FILE__, __LINE__, "after reset",
			     "V92ModulusEncoder", ours, theirs, V92ME_SIZE,
			     (long)trial);
		diff_eq_int("no store past the object (trial %ld)",
			    memcmp(ours + V92ME_SIZE, theirs + V92ME_SIZE,
				   V92ME_SLOT - V92ME_SIZE) == 0, 1, trial);
		diff_eq_int("the bytes past it keep their seed (trial %ld)",
			    memcmp(ours + V92ME_SIZE, before + V92ME_SIZE,
				   V92ME_SLOT - V92ME_SIZE) == 0, 1, trial);
		diff_eq_int("reset wrote something (trial %ld)",
			    memcmp(ours, before, V92ME_SIZE) != 0, 1, trial);
		diff_eq_int("the parameter block is untouched (trial %ld)",
			    memcmp(pblock, pblock_before, PBLOCK_SIZE) == 0,
			    1, trial);
		diff_eq_int("it allocated nothing (trial %ld)",
			    harness_alloc.allocs, 0, trial);

		if (trial == 0)
			memcpy(first, ours, V92ME_SIZE);
		else if (memcmp(first, ours, V92ME_SIZE) != 0)
			differed = 1;
	}

	/* Not the same answer every time, which a stub would also give. */
	diff_eq_int("reset does not produce one fixed object", differed, 1, 0);

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_modulus("ModulusEncoder::ModulusEncoder", our_menc,
			   our_menc2, ref_menc, ref_menc2);
	bad |= run_modulus("ModulusDecoder::ModulusDecoder", our_mdec,
			   our_mdec2, ref_mdec, ref_mdec2);
	bad |= run_v92me();
	bad |= run_v92me_reset();

	return bad;
}

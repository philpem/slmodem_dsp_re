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

int
main(void)
{
	int bad = 0;

	bad |= run_modulus("ModulusEncoder::ModulusEncoder", our_menc,
			   our_menc2, ref_menc, ref_menc2);
	bad |= run_modulus("ModulusDecoder::ModulusDecoder", our_mdec,
			   our_mdec2, ref_mdec, ref_mdec2);
	bad |= run_v92me();

	return bad;
}

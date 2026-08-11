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

void our_menc_prog(void *self, unsigned char *bytes, unsigned int *out)
	asm("_ZN14ModulusEncoder8progressEPhPj");
void ref_menc_prog(void *self, unsigned char *bytes, unsigned int *out)
	asm("ref__ZN14ModulusEncoder8progressEPhPj");
void our_mdec_prog(void *self, unsigned char *bytes, unsigned int *in)
	asm("_ZN14ModulusDecoder8progressEPhPj");
void ref_mdec_prog(void *self, unsigned char *bytes, unsigned int *in)
	asm("ref__ZN14ModulusDecoder8progressEPhPj");

void our_v92me_reset(void *self, void *params)
	asm("_ZN17V92ModulusEncoder5resetEP16V92MappingParams");
void ref_v92me_reset(void *self, void *params)
	asm("ref__ZN17V92ModulusEncoder5resetEP16V92MappingParams");

void our_v92me_progress(void *self, unsigned char *bytes, unsigned int *out)
	asm("_ZN17V92ModulusEncoder8progressEPhPj");
void ref_v92me_progress(void *self, unsigned char *bytes, unsigned int *out)
	asm("ref__ZN17V92ModulusEncoder8progressEPhPj");
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


/* --------------------------------------------- the two progress members */

/*
 * The coder proper: a bit string in, six mixed-radix digits out, and back.
 *
 * NO MODULUS IS EVER ZERO.  The encoder divides by five of the seven members
 * with no guard at all, so a zero one is a division by zero and a signal --
 * not a difference either side could report.  That is a real property of the
 * object and it is recorded in the source rather than driven here.
 *
 * THE LENGTHS GO PAST 63, on purpose.  The accumulator is SIGNED: the
 * divisions are `__divdi3`/`__moddi3` and the decoder shifts with `sar`, so a
 * 64-bit string with its top bit set is a NEGATIVE number, its first
 * remainder comes out negative, and an unsigned reconstruction would disagree
 * on every digit.  A sweep that stopped at 32 bits would call that untested.
 *
 * THE MODULI GO PAST 2^31 for the same reason from the other side: the
 * divisors are zero-extended into the 64-bit helper, so a modulus of
 * 0x80000000 is a large POSITIVE divisor.  Sign-extending it would make it
 * negative and change every quotient.
 */
#define MC_BITS		72
#define MC_GUARD	16

static unsigned char mc_bytes_a[MC_BITS + MC_GUARD];
static unsigned char mc_bytes_b[MC_BITS + MC_GUARD];
static unsigned int mc_words_a[6 + 4];
static unsigned int mc_words_b[6 + 4];

/* field_00 .. field_10: five moduli, none of them zero. */
static const unsigned int mc_mod[][5] = {
	{ 2u, 2u, 2u, 2u, 2u },
	{ 3u, 5u, 7u, 11u, 13u },
	{ 128u, 64u, 32u, 16u, 8u },
	{ 1u, 1u, 1u, 1u, 1u },
	{ 1u, 2u, 1u, 3u, 1u },
	{ 0x80000000u, 2u, 3u, 2u, 5u },
	{ 0xffffffffu, 1u, 1u, 1u, 1u },
	{ 6u, 6u, 6u, 6u, 6u },
	/*
	 * A modulus above 2^31 in the LAST slot as well as the first.  Each
	 * divisor is zero-extended into the 64-bit helper, so these are large
	 * positive numbers; a reconstruction that sign-extended one would make
	 * it negative and change that digit and every one after it, and only a
	 * set with a big value in that slot can see it.
	 */
	{ 2u, 3u, 5u, 7u, 0x80000000u },
	{ 2u, 2u, 2u, 2u, 0xffffffffu }
};
#define MC_NMOD ((int)(sizeof(mc_mod) / sizeof(mc_mod[0])))

static const unsigned int mc_len[] = { 0u, 1u, 2u, 7u, 8u, 31u, 32u, 33u,
				       63u, 64u, 65u, 72u };
#define MC_NLEN ((int)(sizeof(mc_len) / sizeof(mc_len[0])))

static void
mc_setup(void *obj, const unsigned int *mod, unsigned int len,
	 unsigned int spare)
{
	unsigned int v[7];
	int i;

	for (i = 0; i < 5; i++)
		v[i] = mod[i];
	v[5] = spare;			/* +0x14, which neither member reads */
	v[6] = len;			/* +0x18 */
	memcpy(obj, v, sizeof(v));
}

static void
mc_fill_bytes(int trial)
{
	int i;

	unsigned lf = 0x51a7u + 0x9e37u * (unsigned)trial + 1u;

	for (i = 0; i < MC_BITS + MC_GUARD; i++) {
		unsigned char v;

		lf = (lf >> 1) ^ (-(int)(lf & 1u) & 0xb400u);

		switch (trial % 5) {
		case 0:  v = 1;					break;
		case 1:  v = 0;					break;
		case 2:  v = (unsigned char)(i & 1);		break;
		/*
		 * The high bits of the byte must not matter -- the object
		 * masks with 1 -- so two of the five kinds set them.
		 */
		case 3:  v = (unsigned char)(0xfe | (i & 1));	break;
		default: v = (unsigned char)((lf >> 5) | (i & 1));	break;
		}
		mc_bytes_a[i] = mc_bytes_b[i] = v;
	}
}

static int
run_progress(void)
{
	int m, l, trial = 0;
	int seenNegDigit = 0, seenBigDigit = 0, seenCarry = 0, seenRound = 0;
	unsigned char obj_a[MOD_SLOT], obj_b[MOD_SLOT];
	unsigned char decoded[MC_BITS];

	diff_begin("ModulusEncoder::progress / ModulusDecoder::progress");

	for (m = 0; m < MC_NMOD; m++) {
		for (l = 0; l < MC_NLEN; l++) {
			unsigned int len = mc_len[l];
			unsigned int i;
			int roundTrip;

			trial++;
			seed(trial);
			memcpy(obj_a, ours, MOD_SLOT);
			memcpy(obj_b, theirs, MOD_SLOT);
			mc_setup(obj_a, mc_mod[m], len, 0xa5a5a5a5u);
			mc_setup(obj_b, mc_mod[m], len, 0xa5a5a5a5u);
			mc_fill_bytes(trial);

			for (i = 0; i < 6 + 4; i++)
				mc_words_a[i] = mc_words_b[i] =
					0xdeadbe00u + i;

			our_menc_prog(obj_a, mc_bytes_a, mc_words_a);
			ref_menc_prog(obj_b, mc_bytes_b, mc_words_b);

			diff_eq_obj_(__FILE__, __LINE__, "encoder object",
				     "ModulusEncoder", obj_a, obj_b, MOD_SLOT,
				     (long)trial);
			diff_eq_int("the six digits (trial %ld)",
				    memcmp(mc_words_a, mc_words_b,
					   6 * sizeof(unsigned int)) == 0, 1,
				    trial);
			diff_eq_int("nothing past the six (trial %ld)",
				    memcmp(mc_words_a + 6, mc_words_b + 6,
					   4 * sizeof(unsigned int)) == 0
				    && mc_words_b[6] == 0xdeadbe06u, 1, trial);
			diff_eq_int("the encoder did not touch the bits"
				    " (trial %ld)",
				    memcmp(mc_bytes_a, mc_bytes_b,
					   MC_BITS + MC_GUARD) == 0
				    && mc_bytes_b[MC_BITS] == mc_bytes_a[MC_BITS],
				    1, trial);

			for (i = 0; i < 6; i++) {
				if (mc_words_b[i] & 0x80000000u)
					seenNegDigit++;
				if (mc_words_b[i] > 1u
				    && mc_words_b[i] != 0xdeadbe00u + i)
					seenBigDigit++;
			}
			if (mc_words_b[5] != 0u)
				seenCarry++;

			/*
			 * And back.  The decoder is the encoder's inverse
			 * where the digits fit their moduli, so a round trip
			 * that recovers the bit string is a check on both at
			 * once -- and one that does NOT recover it is still
			 * compared side against side, which is what matters.
			 */
			for (i = 0; i < MC_BITS + MC_GUARD; i++)
				mc_bytes_a[i] = mc_bytes_b[i] = 0x5au;

			our_mdec_prog(obj_a, mc_bytes_a, mc_words_a);
			ref_mdec_prog(obj_b, mc_bytes_b, mc_words_b);

			diff_eq_obj_(__FILE__, __LINE__, "decoder object",
				     "ModulusDecoder", obj_a, obj_b, MOD_SLOT,
				     (long)trial);
			diff_eq_int("the bits back out (trial %ld)",
				    memcmp(mc_bytes_a, mc_bytes_b,
					   MC_BITS + MC_GUARD) == 0, 1, trial);
			diff_eq_int("the decoder wrote no further (trial %ld)",
				    mc_bytes_b[len] == 0x5au, 1, trial);
			diff_eq_int("the decoder did not touch the digits"
				    " (trial %ld)",
				    memcmp(mc_words_a, mc_words_b,
					   10 * sizeof(unsigned int)) == 0, 1,
				    trial);

			/*
			 * The decoded bits have to be kept before the
			 * original ones are regenerated over them, or the
			 * comparison is of one array against itself.
			 */
			memcpy(decoded, mc_bytes_b, MC_BITS);
			roundTrip = 1;
			mc_fill_bytes(trial);
			for (i = 0; i < len; i++)
				if ((decoded[i] & 1u) != (mc_bytes_a[i] & 1u))
					roundTrip = 0;
			if (roundTrip && len != 0)
				seenRound++;
		}
	}

	/*
	 * What the sweep is claiming to have reached.  A negative digit is
	 * the signed accumulator showing; a round trip is the pair really
	 * being inverses.
	 */
	diff_eq_int("a digit came out negative %ld times", seenNegDigit > 0,
		    1, seenNegDigit);
	diff_eq_int("a digit came out above one %ld times", seenBigDigit > 0,
		    1, seenBigDigit);
	diff_eq_int("the sixth word carried something %ld times",
		    seenCarry > 0, 1, seenCarry);
	diff_eq_int("a round trip recovered the bits %ld times", seenRound > 0,
		    1, seenRound);

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

/*
 * V92ModulusEncoder::progress -- bits in, twelve digits out.
 *
 * IT IS A CODER AND IT CARRIES STATE, so it is driven eight times in a row
 * over the same pair of objects and BOTH the twelve output words and the whole
 * object are compared after EVERY call.  +0x4c is one bit of running
 * disparity, exclusive-ORed with "did this value land in the top half of the
 * product", and it decides whether the next call encodes the value or its
 * complement.  A single call cannot see it and a comparison of final state
 * cannot either: a divergence at call three that lands back on the same bit by
 * call five would leave the last object identical.
 *
 * THE FIXTURE IS BUILT BY `reset` RATHER THAN POKED IN.  The two limbs at
 * +0x08 and +0x10 are not independent -- they are one 126-bit product split at
 * bit 63 -- and a fabricated pair sends both sides down whichever arm the
 * garbage picks rather than down the arms the encoder actually has.  So each
 * case runs `reset` on both sides first (proved identical by the run above)
 * and only +0x50 and +0x4c are written directly, because those are inputs
 * `reset` always leaves at zero.
 *
 * EVERY MODULUS IS AT LEAST TWO.  `progress` divides by eleven of the twelve
 * through `__divdi3`, and a zero divisor is SIGFPE on both sides at once,
 * which is not a comparison of anything.
 *
 * THE BIT ARRAY IS ONE BIT PER BYTE AND THE OTHER SEVEN BITS ARE NOISE, fresh
 * every call, so a reconstruction that took the whole byte instead of bit 0
 * diverges immediately.  The array is 160 bytes and the largest bit count
 * swept is 96, so a read past the end lands on seeded bytes rather than on
 * whatever follows.
 *
 * WHAT THE SWEEP IS AIMING AT, since case 0 is a tree of eight branches: the
 * bit count above and below 63 (whether the high limb exists at all); a
 * product below and above 2^63 (whether `reset` left a high limb to compare
 * against); the value below and above half the product (which way the
 * disparity bit goes); +0x4c set and clear on entry (value or complement); and
 * within the divide, the high half zero, smaller than the first modulus, and
 * larger than it.  The all-ones and top-half-only patterns are what reach the
 * last of those, and the all-zeros pattern is what reaches the first.
 *
 * FIVE THINGS THE RANDOM SWEEP CANNOT REACH, and `run_v92me_chosen` below
 * reaches them with values worked out rather than stumbled on.  Each one was
 * found by a mutation that the sweep did not catch, which is the only honest
 * way to discover that a branch is not being driven.
 */

#define BITBUF		160
#define OUTN		12
#define OUTSLOT		16

static unsigned char bitbuf[BITBUF];
static unsigned char bitcopy[BITBUF];
static unsigned int out_ours[OUTSLOT];
static unsigned int out_theirs[OUTSLOT];
static unsigned int out_before[OUTSLOT];

struct pcase {
	unsigned int nbits;
	unsigned int m[12];
};

/*
 * Paired bit counts and moduli.  The bit count is not always the one that
 * matches the product: feeding more bits than the product can hold is legal
 * input to both sides and drives the top of the range.
 */
static const struct pcase pcases[] = {
	/* 2^12, and the same moduli asked for far more bits than they hold. */
	{ 12, { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
	{ 63, { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
	/* A high limb of bits over a product that has no high limb at all:
	 * the one shape that reaches the guard on `productHi` being zero. */
	{ 70, { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
	/* 2^60: high limb zero, low limb nearly full. */
	{ 60, { 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 } },
	{ 63, { 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 } },
	/* 2^63 and 2^64: the first products with a high limb. */
	{ 63, { 64, 64, 32, 32, 32, 32, 32, 32, 32, 32, 32, 64 } },
	{ 64, { 64, 64, 64, 32, 32, 32, 32, 32, 32, 32, 32, 64 } },
	/* ~2^71 and ~2^78: a high limb of real size. */
	{ 71, { 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60 } },
	{ 78, { 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 } },
	/* Uneven moduli, which is what the real thing has. */
	{ 55, { 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29 } },
	{ 76, { 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46 } },
	/* Lopsided: all of the magnitude in one half of the twelve. */
	{ 60, { 2, 2, 2, 2, 2, 2, 1000, 1000, 1000, 1000, 1000, 1000 } },
	{ 60, { 1000, 1000, 1000, 1000, 1000, 1000, 2, 2, 2, 2, 2, 2 } },
	/* A modulus of one, which divides but never leaves a digit. */
	{ 64, { 1, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 1 } },
	/* A first modulus big enough to make the high half smaller than it. */
	{ 70, { 0x40000000u, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 } },
	/* Wide and pseudorandom: the product wraps several times over. */
	{ 96, { 0x9e3779b9u, 0x85ebca6bu, 0xc2b2ae35u, 0x27d4eb2fu,
		0x165667b1u, 0x1b873593u, 0xcc9e2d51u, 0x2545f491u,
		0x61c88647u, 0x3b9aca07u, 0x7feb352du, 0x846ca68bu } },
};

#define NPCASE	((int)(sizeof pcases / sizeof pcases[0]))
#define NCALL	8

static void
poke32(unsigned char *obj, int off, unsigned int v)
{
	memcpy(obj + off, &v, sizeof v);
}

static unsigned int
peek32(const unsigned char *obj, int off)
{
	unsigned int v;

	memcpy(&v, obj + off, sizeof v);
	return v;
}

/* The parameter block for one case: the seed everywhere, the case on top. */
static void
fill_pcase(int which)
{
	unsigned lfsr = 0x4d2bu + 0x39a7u * (unsigned)which;
	int i;

	for (i = 0; i < PBLOCK_SIZE; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		pblock[i] = (unsigned char)((lfsr >> 3) | 0x41u);
	}

	put32(pblock, 0x00, pcases[which].nbits);
	for (i = 0; i < 12; i++)
		put32(pblock, 0x1c + 4 * i, pcases[which].m[i]);

	memcpy(pblock_before, pblock, PBLOCK_SIZE);
}

/* One bit per byte in bit 0; the other seven are noise and must be ignored. */
static void
fill_bits(int which, int call)
{
	unsigned lfsr = 0x7a11u + 0x2c9du * (unsigned)(which * NCALL + call);
	int i;

	for (i = 0; i < BITBUF; i++) {
		unsigned bit;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);

		switch (call) {
		case 0:	bit = 0u; break;			/* nothing */
		case 1:	bit = 1u; break;			/* everything */
		case 2:	bit = (unsigned)(i & 1); break;		/* 0101 */
		case 3:	bit = (unsigned)((i + 1) & 1); break;	/* 1010 */
		case 4:	bit = (unsigned)(i < 63); break;	/* low limb only */
		case 5:	bit = (unsigned)(i >= 63); break;	/* high limb only */
		default: bit = (lfsr >> 7) & 1u; break;		/* noise */
		}

		bitbuf[i] = (unsigned char)((lfsr & 0xfeu) | bit);
	}

	memcpy(bitcopy, bitbuf, BITBUF);
}

static void
seed_out(int which, int call)
{
	unsigned lfsr = 0x11c7u + 0x6d31u * (unsigned)(which * NCALL + call);
	int i;

	for (i = 0; i < OUTSLOT; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		out_ours[i] = 0xa5000000u | lfsr;
		out_theirs[i] = out_ours[i];
		out_before[i] = out_ours[i];
	}
}

static int
run_v92me_progress(void)
{
	unsigned int firstout[OUTN];
	int have_first = 0, differed = 0, toggled = 0, wrote = 0;
	int which, sel, call;

	diff_begin("V92ModulusEncoder::progress");

	for (which = 0; which < NPCASE; which++) {
		/* 0, 1 and 2 are the three conversions; 3 is "none of them". */
		for (sel = 0; sel <= 3; sel++) {
			unsigned int f4c = (unsigned int)(which & 1);
			int input = which * 64 + sel * 16;

			seed(which + 128);
			fill_pcase(which);
			harness_alloc_reset();

			our_v92me_reset(ours, pblock);
			ref_v92me_reset(theirs, pblock);

			/* The two inputs reset always leaves at zero. */
			poke32(ours, 0x50, (unsigned int)sel);
			poke32(theirs, 0x50, (unsigned int)sel);
			poke32(ours, 0x4c, f4c);
			poke32(theirs, 0x4c, f4c);

			for (call = 0; call < NCALL; call++) {
				unsigned int was;

				fill_bits(which, call);
				seed_out(which, call);
				was = peek32(ours, 0x4c);

				our_v92me_progress(ours, bitbuf, out_ours);
				ref_v92me_progress(theirs, bitbuf, out_theirs);

				diff_eq_obj_(__FILE__, __LINE__,
					     "the object after progress",
					     "V92ModulusEncoder", ours, theirs,
					     V92ME_SIZE, (long)(input + call));
				diff_eq_int("the twelve digits (case %ld)",
					    memcmp(out_ours, out_theirs,
						   OUTN * sizeof out_ours[0])
					    == 0, 1, input + call);
				diff_eq_int("no store past out[11]"
					    " (case %ld)",
					    memcmp(out_ours + OUTN,
						   out_before + OUTN,
						   (OUTSLOT - OUTN) *
						   sizeof out_ours[0]) == 0,
					    1, input + call);
				diff_eq_int("no store past the object"
					    " (case %ld)",
					    memcmp(ours + V92ME_SIZE,
						   before + V92ME_SIZE,
						   V92ME_SLOT - V92ME_SIZE)
					    == 0, 1, input + call);
				diff_eq_int("the bit array is not written"
					    " (case %ld)",
					    memcmp(bitbuf, bitcopy,
						   BITBUF) == 0, 1,
					    input + call);
				diff_eq_int("it allocated nothing (case %ld)",
					    harness_alloc.allocs, 0,
					    input + call);

				if (sel == 3) {
					/* No case matches: nothing at all. */
					diff_eq_int("selector 3 writes no"
						    " digit (case %ld)",
						    memcmp(out_ours,
							   out_before,
							   sizeof out_ours)
						    == 0, 1, input + call);
					continue;
				}

				if (memcmp(out_ours, out_before,
					   OUTN * sizeof out_ours[0]) != 0)
					wrote = 1;
				if (peek32(ours, 0x4c) != was)
					toggled = 1;
				if (!have_first) {
					memcpy(firstout, out_ours,
					       sizeof firstout);
					have_first = 1;
				} else if (memcmp(firstout, out_ours,
						  sizeof firstout) != 0)
					differed = 1;
			}
		}
	}

	/* A stub that wrote nothing, or wrote one fixed answer, would pass
	 * every comparison above; these are what it would not pass. */
	diff_eq_int("progress writes digits", wrote, 1, 0);
	diff_eq_int("and not the same twelve every time", differed, 1, 0);
	diff_eq_int("and the disparity bit at +0x4c does change", toggled,
		    1, 0);

	return diff_end();
}

/*
 * V92ModulusEncoder::progress over CHOSEN values.
 *
 * Five branches of case 0 are not reachable by sweeping bit patterns, and
 * mutation is what proved it: five entries in the set went uncaught with only
 * `run_v92me_progress` running.  Each is reached here by an input worked out
 * on paper, and the workings are given because an input whose reason is lost
 * is an input the next person deletes.
 *
 * THE FIXTURE CHOOSES THE INPUT AND STILL DOES NOT JUDGE THE OUTPUT.  Two of
 * the cases below need the halfway point of the product, which means the
 * fixture computes it the same way the code does.  That is input SELECTION:
 * the check is still that the two implementations agree, and if the workings
 * here are wrong the case merely lands somewhere less interesting.
 *
 *  1, 2  THE NORMALISATION LOOP REACHING `n == 8`, once for each of the two
 *        moduli it runs for.  `2^63 / m` is negative for every modulus, so
 *        `s * (q >> n)` is only positive when it has WRAPPED, and it has to
 *        wrap the right way eight times running.  A search over m and over
 *        `d % m` finds that m = 170 with a remainder of 128 does: the pair is
 *        put in as the first modulus in case 1 and as the second in case 2,
 *        with the high limb set to 128 and to 256 respectively.
 *
 *  3     THE HIGH LIMB NEGATIVE while the product has no high limb at all.
 *        `d = productHi ? productHi - hi : 0` differs from `productHi - hi`
 *        only when `productHi` is zero and `hi` is not, AND the difference
 *        only survives if `-hi` is positive -- so `hi` has to have its top
 *        bit set, which takes 127 bits of input over a 12-bit product.  The
 *        bit count is a field of the parameter block and 127 is as legal a
 *        value as any; both sides read the same 127 bytes.
 *
 *  4     THE FIRST MODULUS DIVIDING THE HIGH LIMB EXACTLY, so that the
 *        correction is skipped.  It has to be an ODD modulus: the correction
 *        that would wrongly be computed is `2^63 % m`, which is zero for
 *        every even m and would agree with the right answer by accident.
 *        m = 3 with a high limb of 3.
 *
 *  5-7   THE HALFWAY POINT ITSELF, at `ll - 1`, `ll` and `ll + 1`.  The
 *        comparison that sets the disparity bit is against `(product - 1) / 2`
 *        and that is one less than `product / 2` for every even product, so a
 *        sweep only tells the two apart if a value lands exactly between.  The
 *        chance of that by accident is 2^-63 per call.
 */

struct ptarget {
	unsigned int nbits;
	unsigned int f4c;
	unsigned long long hi;		/* bits 63 and up */
	unsigned long long lo;		/* bits 0 .. 62 */
	int halfway;			/* 0, or 1/2/3 for ll-1, ll, ll+1 */
	unsigned int m[12];
};

static const struct ptarget ptargets[] = {
	{ 80, 0, 128, 0x123456789abcdULL, 0,
	  { 170, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 } },
	{ 80, 0, 256, 0x2468aceULL, 0,
	  { 2, 170, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 } },
	{ 127, 1, ~0ULL, 0x555ULL, 0,
	  { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
	{ 70, 0, 3, 0x3ffULL, 0,
	  { 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41 } },
	{ 63, 0, 0, 0, 1, { 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 } },
	{ 63, 0, 0, 0, 2, { 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 } },
	{ 63, 0, 0, 0, 3, { 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 } },
	/* And the same three over a product with a high limb of its own. */
	{ 71, 0, 0, 0, 1, { 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60 } },
	{ 71, 0, 0, 0, 2, { 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60 } },
	{ 71, 0, 0, 0, 3, { 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60 } },
};

#define NPTARGET	((int)(sizeof ptargets / sizeof ptargets[0]))

static unsigned long long
peek64(const unsigned char *obj, int off)
{
	return ((unsigned long long)peek32(obj, off + 4) << 32) |
	       peek32(obj, off);
}

/* Lay a chosen value out one bit per byte, noise in the other seven. */
static void
put_value(unsigned long long hi, unsigned long long lo, unsigned int nbits)
{
	unsigned lfsr = 0x3c5au;
	int i;

	for (i = 0; i < BITBUF; i++) {
		unsigned bit;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);

		if (i < 63)
			bit = (unsigned)((lo >> i) & 1u);
		else if (i < (int)nbits && i - 63 < 64)
			bit = (unsigned)((hi >> (i - 63)) & 1u);
		else
			bit = (lfsr >> 5) & 1u;	/* past the end: never read */

		bitbuf[i] = (unsigned char)((lfsr & 0xfeu) | bit);
	}

	memcpy(bitcopy, bitbuf, BITBUF);
}

static int
run_v92me_chosen(void)
{
	int which;

	diff_begin("V92ModulusEncoder::progress, chosen values");

	for (which = 0; which < NPTARGET; which++) {
		const struct ptarget *tg = &ptargets[which];
		unsigned long long hi = tg->hi, lo = tg->lo;
		int i;

		seed(which + 200);
		harness_alloc_reset();

		{
			unsigned lfsr = 0x6ba1u + 0x1d7fu * (unsigned)which;

			for (i = 0; i < PBLOCK_SIZE; i++) {
				lfsr = (lfsr >> 1) ^
				       (-(int)(lfsr & 1u) & 0xb400u);
				pblock[i] = (unsigned char)((lfsr >> 3) |
							    0x41u);
			}
		}
		put32(pblock, 0x00, tg->nbits);
		for (i = 0; i < 12; i++)
			put32(pblock, 0x1c + 4 * i, tg->m[i]);
		memcpy(pblock_before, pblock, PBLOCK_SIZE);

		our_v92me_reset(ours, pblock);
		ref_v92me_reset(theirs, pblock);

		/* The two limbs come from the REFERENCE object, so the
		 * halfway point is computed over the blob's own answer. */
		if (tg->halfway) {
			long long a = (long long)peek64(theirs, 0x08);
			long long b = (long long)peek64(theirs, 0x10);
			unsigned long long ll;

			ll = ((unsigned long long)(a % 2) << 62) +
			     (unsigned long long)((b - 1) / 2);
			hi = (unsigned long long)(a / 2);
			lo = ll + (unsigned long long)(tg->halfway - 2);
		}

		poke32(ours, 0x50, 0);
		poke32(theirs, 0x50, 0);
		poke32(ours, 0x4c, tg->f4c);
		poke32(theirs, 0x4c, tg->f4c);

		put_value(hi, lo, tg->nbits);
		seed_out(which, 0);

		our_v92me_progress(ours, bitbuf, out_ours);
		ref_v92me_progress(theirs, bitbuf, out_theirs);

		diff_eq_obj_(__FILE__, __LINE__, "the object after progress",
			     "V92ModulusEncoder", ours, theirs, V92ME_SIZE,
			     (long)which);
		diff_eq_int("the twelve digits (target %ld)",
			    memcmp(out_ours, out_theirs,
				   OUTN * sizeof out_ours[0]) == 0, 1, which);
		diff_eq_int("no store past out[11] (target %ld)",
			    memcmp(out_ours + OUTN, out_before + OUTN,
				   (OUTSLOT - OUTN) * sizeof out_ours[0]) == 0,
			    1, which);
		diff_eq_int("no store past the object (target %ld)",
			    memcmp(ours + V92ME_SIZE, before + V92ME_SIZE,
				   V92ME_SLOT - V92ME_SIZE) == 0, 1, which);
		diff_eq_int("the bit array is not written (target %ld)",
			    memcmp(bitbuf, bitcopy, BITBUF) == 0, 1, which);
		diff_eq_int("it allocated nothing (target %ld)",
			    harness_alloc.allocs, 0, which);
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
	bad |= run_progress();
	bad |= run_v92me_reset();
	bad |= run_v92me_progress();
	bad |= run_v92me_chosen();

	return bad;
}

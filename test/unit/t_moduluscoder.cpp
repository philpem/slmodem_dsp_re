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

	return bad;
}

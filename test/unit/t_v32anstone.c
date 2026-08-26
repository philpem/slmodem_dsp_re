/*
 * t_v32anstone.c -- differential test of GenerateAnsTone against the blob.
 *
 * The context is not modelled as a struct on either side, so the fixture is a
 * raw buffer with the pointer to a `struct fpm_tone` planted at +0x1c.  Each
 * side gets its OWN tone object -- see the note on `fx_build` for why a byte
 * copy of one into the other is wrong -- and both are restored from a snapshot
 * before every case, so the two generators start bit-identical each time and
 * any divergence in the samples is this function's and not the tone's.
 *
 * What is compared after every call: the samples, the whole context byte for
 * byte, the whole tone object byte for byte, and the return.  The tone object
 * matters -- the tone phase advances the generator's phase accumulator and the
 * silence phase must not, which no comparison of the OUTPUT can see once the
 * output is all zeros.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v32anstone.h"
#include "dsplib/fpm_tone.h"

extern int ref_GenerateAnsTone(void *ctx, short *out, int count);
extern struct fpm_tone *ref_FPM_TONE_create(struct fpm_tone *state,
					    const struct fpm_tone_cfg *cfg);

#define CTX_LEN		0x40
#define OUT_LEN		256

static unsigned char ctx_a[CTX_LEN], ctx_b[CTX_LEN];
static short out_a[OUT_LEN], out_b[OUT_LEN];
static int failed;

/*
 * EACH SIDE BUILDS ITS OWN TONE OBJECT, and it has to.
 *
 * `FPM_TONE_create(state, cfg)` with a caller-supplied `state` does NOT
 * allocate the object's buffers -- fpm_tone.h says so and a zeroed buffer
 * therefore leaves `kernel`, `history`, `rev_block` and `rev_acc` null, which
 * is a segfault the moment the generator runs.  The first version of this file
 * did exactly that.  So both sides get `create(NULL, ...)`, which allocates
 * object and buffers together, and the two are then INDEPENDENT: a byte copy
 * of one into the other would have made both sides write through one set of
 * heap buffers.
 *
 * BOTH ARE GIVEN OUR CONFIGURATION, deliberately.  `cfg` is copied wholesale
 * into the object, so passing each side its own copy would leave `cfg.src`
 * pointing at a different table on each and the comparison would report the
 * pointer for ever.  One table, two objects, and only the four heap pointers
 * `create` itself fills are skipped below.
 *
 * The pristine copies are what makes each case independent: `create` is called
 * once and the two objects are restored from a snapshot, so no case inherits
 * the phase accumulator the previous one left.
 */
static struct fpm_tone *tone_a, *tone_b;
static unsigned char pristine_a[FPM_TONE_STATE_SIZE];
static unsigned char pristine_b[FPM_TONE_STATE_SIZE];

/* create's own heap pointers: kernel, history, rev_block, rev_acc, iir_self. */
static int
tone_ptr_field(unsigned int off)
{
	return (off >= 0x2c && off < 0x34) || (off >= 0xf4 && off < 0x100);
}

static void
fx_build(void)
{
	tone_a = FPM_TONE_create((struct fpm_tone *)0, &FPM_TONE_CFG_data);
	tone_b = ref_FPM_TONE_create((struct fpm_tone *)0, &FPM_TONE_CFG_data);
	memcpy(pristine_a, tone_a, FPM_TONE_STATE_SIZE);
	memcpy(pristine_b, tone_b, FPM_TONE_STATE_SIZE);
}

static void
fx_init(unsigned int seed, int phase, int tone_len, int silence_len,
	int elapsed)
{
	unsigned int s = seed | 1u;
	unsigned int i;

	memcpy(tone_a, pristine_a, FPM_TONE_STATE_SIZE);
	memcpy(tone_b, pristine_b, FPM_TONE_STATE_SIZE);

	for (i = 0; i < CTX_LEN; i++) {
		s = s * 1103515245u + 12345u;
		ctx_a[i] = ctx_b[i] = (unsigned char)(s >> 16);
	}

	*(int *)(void *)(ctx_a + V32ANS_PHASE) = phase;
	*(int *)(void *)(ctx_b + V32ANS_PHASE) = phase;
	*(int *)(void *)(ctx_a + V32ANS_ELAPSED) = elapsed;
	*(int *)(void *)(ctx_b + V32ANS_ELAPSED) = elapsed;
	*(int *)(void *)(ctx_a + V32ANS_TONE_LEN) = tone_len;
	*(int *)(void *)(ctx_b + V32ANS_TONE_LEN) = tone_len;
	*(int *)(void *)(ctx_a + V32ANS_SILENCE_LEN) = silence_len;
	*(int *)(void *)(ctx_b + V32ANS_SILENCE_LEN) = silence_len;
	*(struct fpm_tone **)(void *)(ctx_a + V32ANS_TONE) = tone_a;
	*(struct fpm_tone **)(void *)(ctx_b + V32ANS_TONE) = tone_b;

	memset(out_a, 0x5a, sizeof(out_a));
	memset(out_b, 0x5a, sizeof(out_b));
}

/* Everything both sides own, except the two tone pointers, which differ. */
static void
fx_same(int input)
{
	unsigned int i;
	int off = -1;

	for (i = 0; i < CTX_LEN; i++) {
		if (i >= V32ANS_TONE && i < V32ANS_TONE + 4)
			continue;	/* the two heap pointers, for ever */
		if (ctx_a[i] != ctx_b[i]) {
			off = (int)i;
			break;
		}
	}
	diff_eq_int("context differs at offset %ld", off, -1, off);

	off = -1;
	for (i = 0; i < FPM_TONE_STATE_SIZE; i++) {
		if (tone_ptr_field(i))
			continue;	/* create's own heap buffers */
		if (((unsigned char *)tone_a)[i] != ((unsigned char *)tone_b)[i]) {
			off = (int)i;
			break;
		}
	}
	diff_eq_int("tone object differs at offset %ld", off, -1, off);

	off = -1;
	for (i = 0; i < OUT_LEN; i++)
		if (out_a[i] != out_b[i]) {
			off = (int)i;
			break;
		}
	diff_eq_int("output differs at word %ld", off, -1, off);
	(void)input;
}

static void
one(unsigned int seed, int phase, int tone_len, int silence_len, int elapsed,
    int count, int input)
{
	fx_init(seed, phase, tone_len, silence_len, elapsed);
	diff_eq_int("return, case %ld", GenerateAnsTone(ctx_a, out_a, count),
		    ref_GenerateAnsTone(ctx_b, out_b, count), input);
	fx_same(input);
}

/*
 * A whole cadence, driven block by block from one context, so the phase
 * transitions are exercised in sequence rather than one at a time.
 */
static void
cadence(int tone_len, int silence_len, int block, int blocks, int input)
{
	int i;

	fx_init(0x77770000u + (unsigned int)input, V32ANS_PHASE_TONE,
		tone_len, silence_len, 0);
	for (i = 0; i < blocks; i++) {
		memset(out_a, 0x5a, sizeof(out_a));
		memset(out_b, 0x5a, sizeof(out_b));
		diff_eq_int("cadence block %ld",
			    GenerateAnsTone(ctx_a, out_a, block),
			    ref_GenerateAnsTone(ctx_b, out_b, block),
			    input * 1000 + i);
		fx_same(input * 1000 + i);
	}
	/* The cadence must actually have finished, or the blocks prove little. */
	diff_eq_int("the cadence reaches the done phase (%ld)",
		    *(int *)(void *)(ctx_a + V32ANS_PHASE) >= V32ANS_PHASE_DONE,
		    1, input);
}

int
main(void)
{
	int n;

	fx_build();

	diff_begin("GenerateAnsTone: one call in each phase");
	/* The tone phase, ending and not ending. */
	one(0x1001u, V32ANS_PHASE_TONE, 160, 160, 0, 40, 1);
	one(0x1002u, V32ANS_PHASE_TONE, 160, 160, 120, 40, 2);   /* exactly */
	one(0x1003u, V32ANS_PHASE_TONE, 160, 160, 121, 40, 3);   /* past    */
	one(0x1004u, V32ANS_PHASE_TONE, 160, 160, 119, 40, 4);   /* short   */
	/* The silence phase, ending and not ending -- note the OTHER compare. */
	one(0x1005u, V32ANS_PHASE_SILENCE, 160, 160, 0, 40, 5);
	one(0x1006u, V32ANS_PHASE_SILENCE, 160, 160, 120, 40, 6); /* exactly:
								   * silence
								   * does NOT
								   * end here */
	one(0x1007u, V32ANS_PHASE_SILENCE, 160, 160, 121, 40, 7); /* past    */
	one(0x1008u, V32ANS_PHASE_SILENCE, 160, 160, 119, 40, 8);
	/*
	 * The done phase, and everything past it.
	 *
	 * ELAPSED IS NON-ZERO IN THREE OF THESE ON PURPOSE.  The done arm
	 * returns without touching the counter, and a body that fell through
	 * to the store instead would write whatever it had -- which is
	 * invisible if the counter was already zero.  With `elapsed` zero
	 * everywhere the mutation that does exactly that came back NOT CAUGHT.
	 */
	one(0x1009u, V32ANS_PHASE_DONE, 160, 160, 0, 40, 9);
	one(0x100au, 3, 160, 160, 77, 40, 10);
	one(0x100bu, -1, 160, 160, -9, 40, 11);
	one(0x100cu, 0x7fffffff, 160, 160, 12345, 40, 12);
	one(0x1013u, V32ANS_PHASE_DONE, 160, 160, 77, 40, 19);
	/* Degenerate counts and lengths. */
	one(0x100du, V32ANS_PHASE_TONE, 160, 160, 0, 0, 13);
	one(0x100eu, V32ANS_PHASE_SILENCE, 160, 160, 0, 0, 14);
	one(0x100fu, V32ANS_PHASE_SILENCE, 160, 160, 0, -8, 15);
	one(0x1010u, V32ANS_PHASE_TONE, 0, 0, 0, 40, 16);
	one(0x1011u, V32ANS_PHASE_SILENCE, 0, 0, 0, 40, 17);
	one(0x1012u, V32ANS_PHASE_TONE, -1, -1, 0, 40, 18);
	failed |= diff_end();

	diff_begin("GenerateAnsTone: the exact-length boundary, both phases");
	/*
	 * F8163.  The two phases END on different comparisons -- `>=` for the
	 * tone and `>` for the silence -- and a test that only ever lands
	 * short of or well past the boundary cannot tell one from the other.
	 * These land ON it, and assert the two phases disagree there.
	 */
	fx_init(0x2001u, V32ANS_PHASE_TONE, 80, 80, 40);
	(void)GenerateAnsTone(ctx_a, out_a, 40);
	diff_eq_int("a tone of exactly TONE_LEN has ended",
		    *(int *)(void *)(ctx_a + V32ANS_PHASE),
		    V32ANS_PHASE_SILENCE, 0);
	fx_init(0x2002u, V32ANS_PHASE_SILENCE, 80, 80, 40);
	(void)GenerateAnsTone(ctx_a, out_a, 40);
	diff_eq_int("a silence of exactly SILENCE_LEN has NOT ended",
		    *(int *)(void *)(ctx_a + V32ANS_PHASE),
		    V32ANS_PHASE_SILENCE, 0);
	/* And the blob agrees with both, which is what makes it a derivation. */
	fx_init(0x2003u, V32ANS_PHASE_TONE, 80, 80, 40);
	(void)GenerateAnsTone(ctx_a, out_a, 40);
	(void)ref_GenerateAnsTone(ctx_b, out_b, 40);
	fx_same(0);
	fx_init(0x2004u, V32ANS_PHASE_SILENCE, 80, 80, 40);
	(void)GenerateAnsTone(ctx_a, out_a, 40);
	(void)ref_GenerateAnsTone(ctx_b, out_b, 40);
	fx_same(1);
	failed |= diff_end();

	diff_begin("GenerateAnsTone: whole cadences, block by block");
	cadence(160, 160, 40, 12, 1);	/* both lengths a whole number of
					 * blocks */
	cadence(150, 150, 40, 12, 2);	/* neither is */
	cadence(40, 40, 40, 8, 3);	/* one block each */
	cadence(1, 1, 40, 6, 4);		/* the first block ends both */
	cadence(600, 40, 64, 16, 5);	/* a long tone, a short silence */
	cadence(64, 600, 64, 16, 6);	/* and the other way round */
	failed |= diff_end();

	diff_begin("GenerateAnsTone: the tone advances, the silence does not");
	/*
	 * F8163 again, on the OTHER observable.  A body that ran the generator
	 * in the silence phase would still write zeros over its output -- the
	 * loop follows -- so only the tone object can see it.  This asserts
	 * the tone object MOVES in the tone phase and does NOT in the silence
	 * phase, both against the blob and against itself.
	 */
	fx_init(0x3001u, V32ANS_PHASE_TONE, 10000, 10000, 0);
	{
		unsigned char before[FPM_TONE_STATE_SIZE];

		memcpy(before, tone_a, sizeof(before));
		(void)GenerateAnsTone(ctx_a, out_a, 40);
		(void)ref_GenerateAnsTone(ctx_b, out_b, 40);
		diff_eq_int("the tone phase moves the generator",
			    memcmp(before, tone_a, sizeof(before)) != 0, 1, 0);
		fx_same(0);

		fx_init(0x3002u, V32ANS_PHASE_SILENCE, 10000, 10000, 0);
		memcpy(before, tone_a, sizeof(before));
		(void)GenerateAnsTone(ctx_a, out_a, 40);
		(void)ref_GenerateAnsTone(ctx_b, out_b, 40);
		diff_eq_int("the silence phase does not",
			    memcmp(before, tone_a, sizeof(before)) == 0, 1, 0);
		fx_same(1);
	}
	/* And the silence really is silence, not merely equal to the blob's. */
	for (n = 0; n < 40; n++)
		diff_eq_int("silence word %ld is zero", out_a[n], 0, n);
	diff_eq_int("and the block beyond it is untouched", out_a[40],
		    0x5a5a, 40);
	failed |= diff_end();

	return failed != 0;
}

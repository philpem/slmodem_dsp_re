/* Constructor-created TONE timer history at a float rounding boundary.
 * Configured duration is finite and positive; all sample counts fit short.
 * The elapsed clock is advanced only by TONE_generate, never planted.
 * This tests the tone component, not a public modem call. */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "dsplib/fdspkrnl.h"

extern struct fdsp_tone *ref_TONE_create(struct fdsp_tone *,
                                      const struct fdsp_tone_cfg *);
extern void ref_TONE_generate(struct fdsp_tone *, float *, short);
extern void ref_TONE_delete(struct fdsp_tone *);

static float ours[32767], theirs[32767];

static void step(struct fdsp_tone *a, struct fdsp_tone *b, short count, long tag)
{
    ref_TONE_generate(b, theirs, count);
    TONE_generate(a, ours, count);
    diff_eq_obj_(__FILE__, __LINE__, "generated samples", "float[]",
                 ours, theirs, count * sizeof(float), tag);
    diff_eq_obj_(__FILE__, __LINE__, "phase and elapsed", "float[3]",
                 &a->phase, &b->phase, 3 * sizeof(float), tag);
}

int main(void)
{
    struct fdsp_tone_cfg cfg = TONE_CFG;
    struct fdsp_tone *a, *b;
    int i, failed;
    cfg.freq = 0.0f;
    cfg.duration = 2097152.5f;
    a = TONE_create(0, &cfg);
    b = ref_TONE_create(0, &cfg);
    diff_begin("constructed TONE elapsed rounding boundary");
    for (i = 0; i < 512; ++i)
        step(a, b, 32767, i);
    step(a, b, 514, 512);
    diff_eq_float("producer elapsed", b->elapsed, 2097152.25f, 0);
    step(a, b, 1, 513);
    diff_eq_float("reference expiry", b->elapsed, 0.0f, 0);
    step(a, b, 1, 514);
    failed = diff_end();
    printf("TONE history: 16777220 generated samples per side, 515 calls\n");
    TONE_delete(a);
    ref_TONE_delete(b);
    return failed;
}

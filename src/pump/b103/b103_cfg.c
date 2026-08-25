/*
 * b103_cfg.c -- Bell 103 / V.21: the datapump configuration.
 *
 * Extracted from dsplibs.o .data:0x77cc (`B103_CFG`), 28 bytes, copied
 * wholesale into the first 28 bytes of the `b103fp` object by
 * `B103FP_create`.
 *
 * Every field's meaning below was established by **measurement**, not by
 * reading `B103FP_create`'s 2151 bytes: build an object for each value of
 * each word and print what came out.  That is worth saying because the one
 * conclusion drawn from reading rather than measuring -- that `+0x04`
 * selected caller versus answer -- was wrong (finding F35).
 *
 * ---------------------------------------------------------------------------
 * `call_type` is the field that matters
 *
 * It is the only one that changes the shape of the object, and the built-in
 * `B103_CFG` sets it to **2, which is loopback**.  A station built straight
 * from `B103_CFG` therefore has no channel bandpass, no tone detector, and
 * cannot complete a call -- which is exactly what it measured before this was
 * understood.
 *
 *  value | mode | bandpass          | tone detector | transmits  | local osc
 * -------|------|-------------------|---------------|------------|----------
 *    0   |  1   | B103_BPF_CALLER   | yes           | 1070/1270  | 1350.1 Hz
 *    1   |  2   | B103_BPF_ANSWER   | yes           | 2025/2225  |  395.0 Hz
 *  else  |  0   | none              | no            | see below  | 1350.1 Hz
 *
 * The two oscillators are the whole frequency plan: each side mixes the pair
 * it *receives* down to 675/875 Hz, either side of the demodulator's 775 Hz
 * discriminator null.  One demodulator design serves both directions and only
 * the oscillator differs.  See findings F32 and F35.
 */

#include "dsplib/b103fp.h"

/*
 * The built-in configuration: LOOPBACK, on the low channel, with a 700-block
 * answer-tone timeout and the modulator at scale 3200.
 *
 * `b103_create` is expected to build its own copy with `call_type` set from
 * the caller/answer argument it is handed; this one on its own does not link.
 */
const struct b103_cfg B103_CFG_data = {
	.call_type = B103_CALL_LOOPBACK,
	.v21 = B103_TONES_BELL103,
	.tone_timeout_ticks = 14000,	/* -> the 700-block floor exactly */
	.f10 = 1,			/* gates a branch in create       */
	.tx_scale = 3200
	/* loop_high_channel and f14 are zero */
};

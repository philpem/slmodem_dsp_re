/*
 * v32anstone.h -- ITU-T V.32: the answer tone's cadence.
 *
 * One function.  `GenerateAnsTone` fills a block with tone, then with silence,
 * then stops, and the two lengths and the phase live in a small context the
 * caller owns.
 *
 * ---------------------------------------------------------------------------
 * IT HAS NO CALLER IN THE OBJECT, AND THAT BOUNDS WHAT CAN BE NAMED HERE
 *
 * Nothing in `dsplibs.o` references `GenerateAnsTone`: `objdump -dr` over the
 * whole 1.2 MB finds the symbol's own definition and its own internal branches
 * and nothing else.  So the strongest evidence class this tree recognises -- a
 * caller that types the argument -- does not exist for it, and neither does
 * the second-strongest, since the only callee is `FPM_TONE_generate` and it
 * types exactly one field.
 *
 * The context is therefore NOT modelled as a struct and its fields are NOT
 * given meanings beyond what the five instructions that touch each of them
 * establish.  That is `include/dsplib/v32data.h`'s ruling applied to a case
 * where it is forced rather than chosen.
 *
 * The offsets it never touches -- +0x00, +0x08, +0x18, and anything past
 * +0x1f -- are not named at all, because a callerless function cannot bound
 * the size of the thing it is handed.
 */

#ifndef DSPLIB_V32ANSTONE_H
#define DSPLIB_V32ANSTONE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The context's four fields, and what each is read or written by.
 *
 * `PHASE` drives a three-way branch and is advanced by this function alone,
 * 0 -> 1 -> 2, so the sequence is tone, then silence, then nothing for ever.
 * There is no way back to 0 from inside here; a caller wanting a second tone
 * has to zero it itself.
 */
#define V32ANS_PHASE		0x04	/* int: 0 tone, 1 silence, else done  */
#define V32ANS_ELAPSED		0x0c	/* int: samples emitted in this phase */
#define V32ANS_TONE_LEN		0x10	/* int: PHASE 0 ends at or past this  */
#define V32ANS_SILENCE_LEN	0x14	/* int: PHASE 1 ends PAST this        */
#define V32ANS_TONE		0x1c	/* struct fpm_tone *                  */

#define V32ANS_PHASE_TONE	0
#define V32ANS_PHASE_SILENCE	1
#define V32ANS_PHASE_DONE	2

/*
 * Emit `count` samples of whatever the current phase calls for and advance the
 * cadence.  Returns 1 -- always, on every path, including the one that emits
 * nothing.
 *
 * `out` IS NOT WRITTEN in the done phase, and is not written at all when
 * `count` is zero or negative in the silence phase; the tone phase hands
 * `count` to `FPM_TONE_generate` whatever its sign, and that function's own
 * loop decides.
 *
 * THE TWO PHASE ENDS USE DIFFERENT COMPARISONS and the object is explicit
 * about it: tone ends when elapsed + count is at or past `TONE_LEN`
 * (`cmp; jl` to continue), silence ends when it is strictly past
 * `SILENCE_LEN` (`cmp; jle` to continue).  So a silence of exactly
 * `SILENCE_LEN` samples is one block short of ending and a tone of exactly
 * `TONE_LEN` is not.  Reproduced, and recorded at D405 because a reader will
 * assume the two match.
 */
int GenerateAnsTone(void *ctx, short *out, int count);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32ANSTONE_H */

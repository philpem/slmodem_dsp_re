/*
 * v32anstone.c -- ITU-T V.32: the answer tone's cadence.
 *
 * Reconstructed from dsplibs.o:
 *   GenerateAnsTone  .text 0x0867c0  199 bytes
 *
 * `include/dsplib/v32anstone.h` carries the context layout and why so little
 * of it is named.  What is worth stating beside the code:
 *
 * THE FUNCTION ALWAYS RETURNS 1.  Every one of its five exits is preceded by
 * `mov $0x1,%eax` -- 0x867e5, 0x8681c, 0x8685b, 0x8687a and the done path's
 * -- so the return carries no information and no caller can branch on it.  It
 * is written as a literal for that reason rather than as a status a future
 * reader might think means something.
 *
 * THE PHASE ADVANCES BEFORE THE COUNTER IS STORED, AND ONLY ON THE ARM THAT
 * ENDS THE PHASE.  On the arm that does not end it, `elapsed` is stored and
 * the phase is untouched; on the arm that does, `elapsed` is ZEROED and the
 * phase incremented, so the accumulated `elapsed + count` is discarded rather
 * than carried into the next phase.  A block that overruns the tone by 40
 * samples therefore starts the silence at 0 and not at 40, and the overrun is
 * lost.  D406.
 *
 * THE COUNT IS AN `int` AND IS PASSED TO THE TONE GENERATOR AS A `short`.
 * `test %esi,%esi; jle` at 0x867f2 is a 32-bit signed test, and
 * `movswl %si,%ecx` at 0x86838 is the narrowing the callee's prototype forces
 * -- `FPM_TONE_generate`'s third parameter is a `short` (fpm_tone.h).  Both
 * are the object's, and the pair is what says the parameter is 32 bits wide.
 */

#include "dsplib/v32anstone.h"

#include "dsplib/fpm_tone.h"

int
GenerateAnsTone(void *ctx, short *out, int count)
{
	struct v32_ans_tone *ans = (struct v32_ans_tone *)ctx;
	int phase = ans->phase;
	int elapsed;

	if (phase == V32ANS_PHASE_TONE) {
		FPM_TONE_generate(ans->tone, out, (short)count);

		elapsed = ans->elapsed + count;
		if (elapsed >= ans->tone_len) {
			ans->elapsed = 0;	/* D406 */
			ans->phase = V32ANS_PHASE_SILENCE;
			return 1;
		}
	} else if (phase == V32ANS_PHASE_SILENCE) {
		int i;

		for (i = 0; i < count; i++)
			out[i] = 0;

		elapsed = ans->elapsed + count;
		if (elapsed > ans->silence_len) {   /* D405 */
			ans->elapsed = 0;	/* D406 */
			ans->phase = V32ANS_PHASE_DONE;
			return 1;
		}
	} else {
		return 1;
	}

	ans->elapsed = elapsed;

	return 1;
}

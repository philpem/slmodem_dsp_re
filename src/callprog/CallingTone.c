/*
 * CallingTone.c -- Calling Tone: the tone the caller sends while waiting.
 *
 * Reconstructed from dsplibs.o CallingTone.c:
 *
 *   GenerateCallingTone   .text 0x07e030
 *   ResetCallingTone      .text 0x07e110
 *
 * See callingtone.h for what it produces, which is not what it was meant to.
 * This file reproduces the original exactly; the three defects are recorded
 * in docs/deviations.md and asserted by t_callingtone, so they cannot be
 * quietly tidied away by a later edit.
 */

#include "dsplib/callingtone.h"
#include "dsplib/dualtone.h"		/* TONE_read */
#include "dsplib/fp_math.h"		/* FP_Pow    */

/*
 * Level to amplitude.  The intent is legible -- FP_Pow is exp(), so a
 * multiply by ln(10)/20 before it would turn decibels into a linear ratio --
 * but the constant is about 200 times too small once the shift is applied,
 * so what comes out is exp(level / 1734) rather than 10^(level/20).
 *
 * ln(10)/20 in Q14 is 1886, and passing `level * 1886` straight to FP_Pow
 * would have been correct.  What is here is (level * 154791) >> 14, which is
 * level * 9.45.  See D13.
 */
#define CALLING_TONE_LEVEL_SCALE	0x25ca7

void
GenerateCallingTone(struct calling_tone *ct, short *buf, int count)
{
	int i = 0;

	while (i < count) {
		int remaining = ct->remaining;
		int end;

		/*
		 * `end` is an index bound, not a length -- it is clamped
		 * against `count`, not against `count - i`.  That is the
		 * original's, and it is why the period accounting below
		 * over-counts after a transition inside a block.  See D12.
		 */
		end = remaining < count ? remaining : count;

		if (ct->on == 0) {
			while (i < end)
				buf[i++] = 0;
		} else {
			while (i < end) {
				short v;

				/*
				 * The phase accumulator holds one cycle in
				 * 14 bits and TONE_read wants 11, so this
				 * shift should be 3.  At 6 only the first
				 * eighth of the cosine is ever reached and
				 * the output never goes negative.  See D11.
				 *
				 * The +32 is a round-to-nearest on the
				 * discarded bits, and is correct for either
				 * shift.
				 */
				v = TONE_read((short)((ct->phase + 32) >> 6));

				/*
				 * Q14 times Q14 shifted by 13, so the scale
				 * is doubled: a full-scale amplitude and a
				 * peak table entry give 32768, which wraps
				 * to -32768 in the store.
				 */
				buf[i] = (short)((ct->amplitude * v) >> 13);
				i++;

				ct->phase = (short)((ct->phase
						     + CALLING_TONE_STEP)
						    & CALLING_TONE_PHASE_MASK);
			}
			remaining = ct->remaining;
		}

		remaining -= end;
		if (remaining != 0) {
			ct->remaining = remaining;
			continue;
		}

		if (ct->on != 0) {
			ct->on = 0;
			ct->remaining = CALLING_TONE_OFF;
		} else {
			ct->on = 1;
			ct->remaining = CALLING_TONE_ON;
		}
	}
}

void
ResetCallingTone(struct calling_tone *ct, char level)
{
	ct->phase = 0;
	ct->on = 1;
	ct->remaining = CALLING_TONE_ON;
	ct->amplitude = (short)FP_Pow((level * CALLING_TONE_LEVEL_SCALE) >> 14);
}

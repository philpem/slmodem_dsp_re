/*
 * fpm_fsm.c -- FSK modulator.
 *
 * Reconstructed from dsplibs.o fpm_fsm.c, FPM_FSM_modulate at .text 0x0a8940.
 * FPM_FSM_init and FPM_FSM_delete are pending -- init builds the FPM_TONE
 * object, which is not reconstructed yet.
 */

#include "dsplib/fpm_fsm.h"
#include "dsplib/fpm_tone.h"

short
FPM_FSM_modulate(struct fpm_fsm *state, const unsigned short *bits,
		 short *out, unsigned short nbits)
{
	int total = 0;
	unsigned i;

	for (i = 0; i < nbits; i++) {
		int bit = bits[i] & 1;
		int k;

		/*
		 * Retune per bit.  The frequency is the pre-scaled one, so
		 * FPM_TONE's 8 kHz assumption lands on the right phase
		 * increment for a 7200 Hz stream.
		 */
		FPM_TONE_set_freq(state->tone, state->scaled[bit]);
		FPM_TONE_set_scale(state->tone, state->scale);

		total += state->samples_per_sym;

		/*
		 * One sample per call, not one block call.  That matters: the
		 * tone generator's phase-reversal counter advances by
		 * count >> 3, so a count of 1 never advances it.  Driven this
		 * way the modulator's reversal logic is inert -- which is
		 * correct for data, where a 180 degree hop mid-symbol would
		 * corrupt the bit.
		 */
		for (k = 0; k < state->samples_per_sym; k++)
			FPM_TONE_generate(state->tone, out++, 1);
	}

	return (short)total;
}

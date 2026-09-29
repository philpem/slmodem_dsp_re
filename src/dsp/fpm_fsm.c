/*
 * fpm_fsm.c -- Fixed Point Modem: Frequency Shift Modulator.
 *
 * Reconstructed from dsplibs.o fpm_fsm.c:
 *   FPM_FSM_init      .text 0x0a8880
 *   FPM_FSM_modulate  .text 0x0a8940
 *   FPM_FSM_delete    .text 0x0a8a30
 */

#include <string.h>

#include "dsplib/fpm_fsm.h"
#include "dsplib/fpm_tone.h"

/*
 * Frequencies are pre-scaled by 10/9 = 8000/7200 before reaching FPM_TONE,
 * which assumes 8 kHz.  The product is a phase increment correct for the
 * 7200 Hz the modulator actually runs at.  0x471c is 10/9 in Q14.
 */
#define FPM_FSM_RATE_SCALE 0x471c

void
FPM_FSM_init(struct fpm_fsm *state, const struct fpm_fsm_cfg *cfg)
{
	struct fpm_tone_cfg tone;

	/* The first four shorts: both frequencies, symbol length and scale. */
	state->cfg = *cfg;

	state->scaled[0] =
		(short)(((int)state->cfg.freq[0] * FPM_FSM_RATE_SCALE) >> 14);
	state->scaled[1] =
		(short)(((int)state->cfg.freq[1] * FPM_FSM_RATE_SCALE) >> 14);

	/*
	 * Start from the built-in tone configuration and override only the
	 * output scale.  The frequency is not set here -- modulate retunes per
	 * bit, so whatever the default carries is immediately replaced.
	 */
	tone = FPM_TONE_CFG;
	tone.scale = state->cfg.scale;

	/*
	 * Passing the existing pointer means a re-init reuses the object;
	 * on a zeroed state it is NULL and FPM_TONE_create allocates.
	 */
	state->tone = FPM_TONE_create(state->tone, &tone);
}
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
		FPM_TONE_set_scale(state->tone, state->cfg.scale);

		total += state->cfg.samples_per_sym;

		/*
		 * One sample per call, not one block call.  That matters: the
		 * tone generator's phase-reversal counter advances by
		 * count >> 3, so a count of 1 never advances it.  Driven this
		 * way the modulator's reversal logic is inert -- which is
		 * correct for data, where a 180 degree hop mid-symbol would
		 * corrupt the bit.
		 */
		for (k = 0; k < state->cfg.samples_per_sym; k++)
			FPM_TONE_generate(state->tone, out++, 1);
	}

	return (short)total;
}




void
FPM_FSM_delete(struct fpm_fsm *state)
{
	if (state != 0)
		FPM_TONE_delete(state->tone);
}

/*
 * The library default, `D` at .data:0x8198, 8 bytes -- the object's own name
 * for it.  V.21 channel 2 at full scale; note freq[0] is the HIGHER tone
 * here, because V.21's mark is the lower one.  B103FP_create overwrites both
 * frequencies and the scale for every configuration it builds, so this is
 * only what an unpatched caller gets.
 *
 * A `FPM_FSM_CFG_data` stub used to sit here with the same bytes under a
 * `const` name, written before anything referenced the blob symbol;
 * `V21TX_create` references `FPM_FSM_CFG` directly (two loads at 0x0993e2 and
 * 0x0993e8) and the B.103 processor read the stub.  The duplicate was D1230,
 * D1180's shape a third time; it is removed here and its one reader points at
 * `FPM_FSM_CFG`.  Finding F9356.
 */


/*
 * `D` in the object -- global and writable -- hence not `const`.
 *
 * V.21 channel 2's mark and space (see `v21cfg.h`'s `V21_CHAN2_MARK_HZ` /
 * `V21_CHAN2_SPACE_HZ`), full symbol length for 300 baud at 7200 Hz, and full
 * scale.  `V21TX_create` overrides `scale` to 0x1900 (6400) after copying
 * this in; every other caller gets it unpatched.
 */
struct fpm_fsm_cfg FPM_FSM_CFG = {
	{ 1850, 1650 },	/* +0x00 freq: space, mark                  */
	24,		/* +0x04 samples_per_sym                    */
	32767		/* +0x06 scale                               */
};

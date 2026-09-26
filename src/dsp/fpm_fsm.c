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
 * The library default, from .data:0x8198.  V.21 channel 2 at full scale --
 * note freq[0] is the HIGHER tone here, because V.21's mark is the lower one.
 * B103FP_create overwrites both frequencies and the scale for every
 * configuration it builds, so this is only what an unpatched caller gets.
 */
const struct fpm_fsm_cfg FPM_FSM_CFG_data = {
	.freq = { 1850, 1650 },	/* space, mark -- V.21's mark is the lower */
	.samples_per_sym = 24,	/* 300 baud at 7200 Hz */
	.scale = 32767
};

/*
 * fpm_fsm_cfg.c -- Fixed Point Modem: the FSK Modulator's library built-in
 *                  configuration, `D` at .data:0x8198, 8 bytes.
 *
 * THE OBJECT'S OWN `FPM_FSM_CFG` IS HERE, AND IT IS NOT `FPM_FSM_CFG_data`.
 * This is `fpm_fsd_cfg.c`'s situation a third time (that file's own header
 * names it "FPM_MTD_CFG's situation a second time"): when `src/dsp/fpm_fsm.c`
 * was written the blob symbol had no caller that needed it, so a stub was
 * given its own `_data` name and `FPM_FSM_CFG` was left unwritten.
 * `V21TX_create` (0x0992f0) references `FPM_FSM_CFG` directly -- two loads at
 * 0x0993e2 and 0x0993e8 that copy it onto the stack before `FPM_FSM_init` --
 * and a `src/` reference to an unwritten blob symbol cannot link (F8492).  So
 * it has to exist under the object's name.  Finding F9356.
 *
 * THE TWO ARE THE SAME BYTES.  The object's eight bytes at .data:0x8198 are
 * `3a 07 72 06 18 00 ff 7f`, which is `{freq: {1850, 1650}, samples_per_sym:
 * 24, scale: 32767}` -- exactly what `src/dsp/fpm_fsm.c`'s `FPM_FSM_CFG_data`
 * already carries, and its own comment already read the value off the same
 * bytes ("V.21 channel 2 at full scale").  `t_v21txcreate.c` asserts the two
 * are `memcmp`-identical rather than assuming it.
 *
 * What remains is a storage class and a duplicate symbol: the object's is
 * `D`, global and writable; the stub is `const`, in `.rodata`.
 *
 * NOT FIXED HERE, AND THE FIX IS ONE LINE THIS PASS CANNOT REACH.  Delete the
 * stub and its declaration in `include/dsplib/fpm_fsm.h`, and point its one
 * reader -- `src/pump/b103/b103fp.c:1092`, `fsm = FPM_FSM_CFG_data;` -- at
 * `FPM_FSM_CFG`.  That file is `src/pump/**`, fenced from this pass exactly as
 * it was from `fpm_fsd_cfg.c`'s.  Recorded as D1180's shape, D1230.
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

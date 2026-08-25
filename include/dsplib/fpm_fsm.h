/*
 * fpm_fsm.h -- Fixed Point Modem: Frequency Shift Modulator.
 *
 * Turns a bit stream into audio by switching a tone generator between two
 * frequencies, holding each for a fixed number of samples.
 *
 * Bell 103 / V.21 use 24 samples per symbol, which at 300 baud means the
 * modulator runs at 7200 Hz; FPM_MRF then lifts its output to the 8000 the
 * datapump interface uses.  The stored frequencies are pre-scaled by 10/9 so
 * that FPM_TONE, which assumes 8 kHz, produces phase increments correct for
 * 7200.  See findings F17 and F24.
 */

#ifndef DSPLIB_FPM_FSM_H
#define DSPLIB_FPM_FSM_H

/*
 * The config is the first 8 bytes of the state and nothing more -- init reads
 * exactly these four fields and derives the rest.  Declared separately so a
 * caller building one does not have to supply a whole state's worth.
 */
struct fpm_fsm_cfg {
	short freq[2];		/* +0x00 indexed by the bit: [0] space,
				 *       [1] mark.  Which is the HIGHER of
				 *       the two differs by standard -- Bell
				 *       103's mark is higher, V.21's lower. */
	short samples_per_sym;	/* +0x04 24 for 300 baud at 7200 Hz     */
	short scale;		/* +0x06 output gain, Q15               */
};

struct fpm_fsm {
	struct fpm_fsm_cfg cfg;	/* +0x00 copied wholesale by init       */
	short scaled[2];	/* +0x08 freq * 10/9, what the tone sees */
	struct fpm_tone *tone;	/* +0x0c                                */
};

/* The library default: V.21 channel 2, full scale. */
extern const struct fpm_fsm_cfg FPM_FSM_CFG_data;

/*
 * Build a modulator.  `cfg` supplies the two frequencies, the symbol length
 * in samples and the output scale; the frequencies are stored both as given
 * and pre-scaled for the tone generator.
 */
void FPM_FSM_init(struct fpm_fsm *state, const struct fpm_fsm_cfg *cfg);
void FPM_FSM_delete(struct fpm_fsm *state);

/*
 * Modulate `nbits` bits, one 16-bit word each, using only bit 0.  Returns the
 * number of samples written: nbits * samples_per_sym.
 */
short FPM_FSM_modulate(struct fpm_fsm *state, const unsigned short *bits,
		       short *out, unsigned short nbits);

#endif /* DSPLIB_FPM_FSM_H */

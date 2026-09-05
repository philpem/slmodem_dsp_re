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
 * The object's OWN name for the same table, `D` at .data:0x8198 -- see
 * `src/dsp/fpm_fsm_cfg.c` for the derivation and D1230 for why both still
 * exist.
 */
extern struct fpm_fsm_cfg FPM_FSM_CFG;

/**
 * @brief Build a frequency-shift modulator.
 *
 * Copies @p cfg wholesale and derives the tone generator's pre-scaled
 * frequencies (`freq * 10/9`) from it.
 *
 * @param state  The modulator to initialize.
 * @param cfg    Supplies the two frequencies, the symbol length in samples
 *               and the output scale.
 */
void FPM_FSM_init(struct fpm_fsm *state, const struct fpm_fsm_cfg *cfg);

/**
 * @brief Release a modulator built by FPM_FSM_init().
 * @param state  The modulator to tear down.
 */
void FPM_FSM_delete(struct fpm_fsm *state);

/**
 * @brief Modulate a run of bits into audio.
 *
 * Each bit switches the tone generator between the config's two
 * frequencies for `samples_per_sym` samples.
 *
 * @param state  The modulator.
 * @param bits   @p nbits words, one bit per symbol; only bit 0 of each is read.
 * @param out    Output buffer, `nbits * samples_per_sym` samples.
 * @param nbits  Number of bits to modulate.
 * @return The number of samples written (`nbits * samples_per_sym`).
 */
short FPM_FSM_modulate(struct fpm_fsm *state, const unsigned short *bits,
		       short *out, unsigned short nbits);

#endif /* DSPLIB_FPM_FSM_H */

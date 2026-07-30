/*
 * fpm_fsm.h -- FSK modulator.
 *
 * Turns a bit stream into audio by switching a tone generator between two
 * frequencies, holding each for a fixed number of samples.
 *
 * Bell 103 / V.21 use 24 samples per symbol, which at 300 baud means the
 * modulator runs at 7200 Hz; FPM_MRF then lifts its output to the 8000 the
 * datapump interface uses.  The stored frequencies are pre-scaled by 10/9 so
 * that FPM_TONE, which assumes 8 kHz, produces phase increments correct for
 * 7200.  See findings 17 and 24.
 */

#ifndef DSPLIB_FPM_FSM_H
#define DSPLIB_FPM_FSM_H

struct fpm_fsm {
	short freq[2];		/* +0x00 mark and space, in Hz          */
	short samples_per_sym;	/* +0x04                                */
	short scale;		/* +0x06 output gain                    */
	short scaled[2];	/* +0x08 freq * 10/9, what the tone sees */
	void *tone;		/* +0x0c FPM_TONE object                */
};

/*
 * Modulate `nbits` bits, one 16-bit word each, using only bit 0.  Returns the
 * number of samples written: nbits * samples_per_sym.
 */
short FPM_FSM_modulate(struct fpm_fsm *state, const unsigned short *bits,
		       short *out, unsigned short nbits);

#endif /* DSPLIB_FPM_FSM_H */

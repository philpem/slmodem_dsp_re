/*
 * fpm_fsd.h -- Fixed Point Modem: Frequency Shift Demodulator.
 *
 * Bell 103 receives at 2400 Hz (FPM_MRF brings 8000 down to it), which is 8
 * samples per symbol at 300 baud.  The chain is:
 *
 *     input FIR  ->  delay-line discriminator  ->  IIR lowpass
 *             ->  Schmitt slicer  ->  edge-resynchronised bit clock
 *
 * NOTE the config is a mixed struct with POINTERS at +0x00 and +0x08 --
 * dumping it as int16 hides them behind plausible-looking scalars.
 */

#ifndef DSPLIB_FPM_FSD_H
#define DSPLIB_FPM_FSD_H

/* 28 bytes, copied wholesale by init.  Bell 103's values in the comments. */
struct fpm_fsd_cfg {
	const short *fir;	/* +0x00 input FIR coefficients          */
	short fir_taps;		/* +0x04 15                              */
	short delay;		/* +0x06 discriminator delay, samples: 4 */
	const short *iir;	/* +0x08 IIR lowpass coefficients        */
	short iir_len;		/* +0x0c sections: 3                     */
	short slice_level;	/* +0x0e Schmitt threshold, +/-: 10      */
	short high_bit;		/* +0x10 bit value for a positive slice
				 *       result; 1 - it for a negative.
				 *       Setting it to 0 inverts the data. */
	short bit_samples;	/* +0x12 samples per bit: 8              */
	short max_bits;		/* +0x14 output cap; see the note below  */
	short trace_len;	/* +0x16 length of the trace buffer: 160 */
	short f18;		/* +0x18 not read by demodulate          */
	short pad1a;
};

struct fpm_fsd {
	struct fpm_fsd_cfg cfg;	/* +0x00 .. +0x1a */
	short *trace;		/* +0x1c trace_len entries: the lowpass
				 *       output, one word per input sample.
				 *       Rewritten from the start on every
				 *       call and never read back.        */
	short last_count;	/* +0x20 the `count` of the last call     */
	short f22;		/* +0x22 bit_samples / 2, set by init and
				 *       then never read -- demodulate
				 *       recomputes it.                   */
	short *fir_hist;	/* +0x24 fir_taps entries, circular       */
	short hist_idx;		/* +0x28 its write position               */
	short pad2a;
	short *iir_hist;	/* +0x2c 2 * iir_len entries              */
	short bit;		/* +0x30 the committed bit                */
	short since_bit;	/* +0x32 samples since the last one       */
	short disagreements;	/* +0x34 samples sliced against `bit`     */
	short pad36;
};

/**
 * @brief Initialise an FSK demodulator.
 * @param state  The demodulator to initialise.
 * @param cfg    Filter and slicer configuration, copied wholesale.
 * @param fresh  Nonzero allocates the trace/FIR-history/IIR-history
 *               buffers; zero re-initialises in place, reusing whatever
 *               @p state already points at.
 */
void FPM_FSD_init(struct fpm_fsd *state, const struct fpm_fsd_cfg *cfg,
		  int fresh);

/**
 * @brief Free the buffers allocated by FPM_FSD_init() with `fresh != 0`.
 * @param state  The demodulator to tear down.
 */
void FPM_FSD_free(struct fpm_fsd *state);

/*
 * The library default: no filters, but Bell 103's scalars throughout.
 *
 * TWO SYMBOLS, ONE TABLE, AND ONLY THE SECOND IS THE OBJECT'S.  `FPM_FSD_CFG`
 * is the blob's own name for this, `D` at .data:0x812c; `FPM_FSD_CFG_data` is
 * a stub written before anything referenced the real one, and its only reader
 * is `src/pump/b103/b103fp.c`.  The two hold identical values -- `t_v21cfg.c`
 * checks that field for field -- and differ only in storage class.  Deleting
 * the stub and pointing its one reader here is D1180's fix; see
 * `src/dsp/fpm_fsd_cfg.c` for why this pass did not make it.
 */
extern const struct fpm_fsd_cfg FPM_FSD_CFG_data;
extern struct fpm_fsd_cfg FPM_FSD_CFG;

/**
 * @brief Demodulate a run of samples through FIR, discriminator, IIR
 * lowpass and Schmitt slicer, appending completed bits to @p bits_out.
 *
 * @param state     The demodulator, updated in place.
 * @param samples   Input samples.
 * @param bits_out  Completed bits are appended here, `cfg.high_bit`-coded.
 * @param count     Number of input samples. At most `cfg.max_bits + 2`
 *                  bits are written in one call, and any remaining input
 *                  beyond that is DISCARDED rather than held over -- a
 *                  caller feeding more than that many bits' worth of
 *                  samples silently loses data. See src/dsp/fpm_fsd.c.
 * @return The number of bits written -- usually far fewer than @p count,
 *         since one bit takes `cfg.bit_samples` samples.
 */
short FPM_FSD_demodulate(struct fpm_fsd *state, const short *samples,
			 unsigned short *bits_out, unsigned short count);

#endif /* DSPLIB_FPM_FSD_H */

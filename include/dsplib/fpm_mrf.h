/*
 * fpm_mrf.h -- Fixed Point Modem: Multi-Rate Filter (polyphase resampler).
 *
 * Despite the abbreviation, MRF is *multi-rate*, not "matched root filter".
 * It is the library's second polyphase resampler, alongside FixedRC -- and
 * the one the datapumps use internally, where FixedRC bridges the host.
 *
 * Bell 103 runs two: 10:9 to lift the 7200 Hz FSK modulator output to the
 * 8000 the datapump interface uses, and 3:10 to bring received 8000 down to
 * 2400 for demodulation (8 samples per symbol at 300 baud).  See finding F24.
 */

#ifndef DSPLIB_FPM_MRF_H
#define DSPLIB_FPM_MRF_H

/*
 * Configuration.  The coefficient array holds `taps` entries laid out as
 * `branches` polyphase phases of `taps / branches` each.
 */
struct fpm_mrf_cfg {
	short branches;		/* +0x00 interpolation factor            */
	short decimate;		/* +0x02 decimation factor               */
	const short *coeff;	/* +0x04                                 */
	short taps;		/* +0x08 total, across all phases        */
	/*
	 * Named, not implied.  Callers build one of these on the stack by
	 * copying a static and patching `coeff`, and init then copies the
	 * whole thing into the object -- so the two bytes here end up in a
	 * differential comparison.  The original's copy is dword-wise and
	 * carries the static's zero; a struct assignment over unnamed padding
	 * is free to leave stack garbage there instead.  Naming it makes it a
	 * member, which both the assignment and the designated initialisers
	 * below have to honour.
	 */
	short pad0a;
	void *aux;		/* +0x0c                                 */
};

struct fpm_mrf {
	struct fpm_mrf_cfg cfg;	/* +0x00 copied wholesale by init        */
	short need;		/* +0x10 inputs owed before the next out */
	short phase;		/* +0x12 polyphase position, 0 .. L-1    */
	short widx;		/* +0x14 circular history write index    */
	short history_len;	/* +0x16 taps / branches                 */
	short *history;		/* +0x18 history_len entries             */
};

/**
 * @brief Initialise (or re-initialise) an MRF resampler's state.
 *
 * Copies @p cfg into @p state and resets `need`/`phase`/`widx` to a fresh
 * run's start values, then sizes and clears the history buffer.
 *
 * @param state  The resampler state to initialise.
 * @param cfg    Configuration to copy in (branches, decimate, coefficients).
 * @param fresh  Non-zero if @p state is uninitialised memory: the existing
 *               `history` pointer is not inspected or freed, only replaced.
 *               Zero re-initialises in place, reusing the existing buffer
 *               when it is already large enough for the new tap count and
 *               reallocating (freeing the old one first) when it is not.
 *               Calling with `fresh` set on an already-initialised state
 *               leaks the old buffer -- reproduced from the original.
 */
void FPM_MRF_init(struct fpm_mrf *state, const struct fpm_mrf_cfg *cfg,
		  int fresh);

/**
 * @brief Free an MRF resampler's history buffer.
 * @param state The resampler state to tear down.
 */
void FPM_MRF_free(struct fpm_mrf *state);

/** @brief Library default configuration: 9:10, no coefficients. A template
 *  for callers to copy and patch `coeff`, not a usable filter on its own. */
extern const struct fpm_mrf_cfg FPM_MRF_CFG;

/**
 * @brief Resample @p count input samples through one MRF filter.
 *
 * `phase`, `widx` and `need` persist in @p state across calls, so a stream
 * may be fed in arbitrary fragments -- including fragments shorter than one
 * output needs.
 *
 * @param state  Resampler state (holds the circular history and position).
 * @param in     Input samples, @p count of them.
 * @param out    Output buffer; receives the produced samples.
 * @param count  Number of input samples to consume.
 * @return Number of output samples produced, roughly `count * branches /
 *         decimate`.
 */
short FPM_MRF_filter(struct fpm_mrf *state, const short *in, short *out,
		     short count);

#endif /* DSPLIB_FPM_MRF_H */

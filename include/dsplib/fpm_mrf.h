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

/*
 * `fresh` non-zero means the state is uninitialised: allocate without
 * inspecting the existing buffer.  Zero means re-init, reusing the buffer if
 * it is already large enough.
 */
void FPM_MRF_init(struct fpm_mrf *state, const struct fpm_mrf_cfg *cfg,
		  int fresh);
void FPM_MRF_free(struct fpm_mrf *state);

/* The library default: 9:10, no coefficients.  A template, not a filter. */
extern const struct fpm_mrf_cfg FPM_MRF_CFG;

/*
 * Resample `count` input samples.  Returns the number of outputs produced,
 * which is roughly count * branches / decimate.
 *
 * `phase`, `widx` and `need` persist across calls, so a stream may be fed in
 * arbitrary fragments -- including fragments shorter than one output needs.
 */
short FPM_MRF_filter(struct fpm_mrf *state, const short *in, short *out,
		     short count);

#endif /* DSPLIB_FPM_MRF_H */

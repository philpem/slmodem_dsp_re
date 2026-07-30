/*
 * fpm_mrf.h -- multi-rate (polyphase resampling) filter.
 *
 * Despite the abbreviation, MRF is *multi-rate*, not "matched root filter".
 * It is the library's second polyphase resampler, alongside FixedRC -- and
 * the one the datapumps use internally, where FixedRC bridges the host.
 *
 * Bell 103 runs two: 10:9 to lift the 7200 Hz FSK modulator output to the
 * 8000 the datapump interface uses, and 3:10 to bring received 8000 down to
 * 2400 for demodulation (8 samples per symbol at 300 baud).  See finding 24.
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
	void *aux;		/* +0x0c                                 */
};

struct fpm_mrf {
	struct fpm_mrf_cfg cfg;	/* +0x00 copied wholesale by init        */
	unsigned short f10;	/* +0x10 set to 1 by init                */
	unsigned short f12;	/* +0x12 cleared                         */
	unsigned short f14;	/* +0x14 cleared                         */
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

#endif /* DSPLIB_FPM_MRF_H */

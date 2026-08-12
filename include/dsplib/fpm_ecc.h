/*
 * fpm_ecc.h -- Fixed Point Modem: Echo Canceller.
 *
 * A two-section adaptive canceller for a passband data modem working
 * full-duplex over a two-wire loop: a NEAR section covering the hybrid's own
 * leakage and a FAR section covering the round-trip echo off the far end of
 * the line.  Both are driven not by the transmitted *waveform* but by the
 * transmitted *symbols*, replayed out of one delay line at two taps whose
 * separation is the extra round-trip delay -- so the canceller carries only
 * `near_taps + far_taps` complex coefficients rather than one per sample over
 * the whole echo span.
 *
 * V.32 sends 2400 symbols/s at 7200 samples/s, and the object bears that out:
 * three coefficient sets, `coef[0..2]`, one per sample phase, cycled by
 * `phase`, with a new symbol pulled off the delay line whenever it wraps.
 *
 * Symbols are stored in the delay line PACKED: the high byte selects one of
 * the six constellation maps in `cfg.imap` / `cfg.qmap` and the low byte is
 * the point within it.  `ECCv32_CFG` presets the line to 16, which reaches
 * `SMCv32_IMAP16[16]` -- that table is 0x22 bytes, i.e. 17 entries, so the
 * preset is the one point past a 16-point constellation.
 *
 * Reconstructed from dsplibs.o:
 *   FPM_ECC_cancel  .text 0x0a6e00
 *   FPM_ECC_init    .text 0x0a7610
 *   FPM_ECC_free    .text 0x0a7870
 */

#ifndef DSPLIB_FPM_ECC_H
#define DSPLIB_FPM_ECC_H

/*
 * Configuration, 24 bytes, copied wholesale into the state by init.
 *
 * `pad06` and `pad12` are named rather than implied for the same reason
 * fpm_mrf_cfg's `pad0a` is: init copies the config six dwords at a time, so
 * the holes reach a differential comparison and must be members that a struct
 * assignment is obliged to carry.
 */
struct fpm_ecc_cfg {
	short far_lag;		/* +0x00 symbols the far tap lags the near   */
	short near_taps;	/* +0x02 near section length, and the stride
				 *       between coefficient blocks           */
	short far_taps;		/* +0x04 far section length, and its stride   */
	short pad06;
	const short *const *imap;	/* +0x08 six in-phase maps            */
	const short *const *qmap;	/* +0x0c six quadrature maps          */
	short fill;		/* +0x10 packed symbol the delay line is
				 *       preset to                            */
	short pad12;
	void *aux;		/* +0x14 unread by any of the three; a
				 * pointer only by analogy with fpm_mrf_cfg   */
};

/*
 * State.  0x68 bytes is a FLOOR measured from init, free and cancel -- those
 * three touch nothing above 0x67 -- and not a proven size.
 *
 * `near_delay` and `far_delay` are INPUTS: init reads them and never writes
 * them, so whoever owns this object sets them beforehand.  They size the
 * delay line, `line_len = far_lag + near_delay + far_delay`, and place the
 * two read taps in it.
 */
struct fpm_ecc {
	struct fpm_ecc_cfg cfg;	/* +0x00 */
	short hold_power;	/* +0x18 nonzero: leave both power meters
				 *       alone                                */
	short unk1a;		/* +0x1a zeroed by init, read by nothing here */
	int enabled;		/* +0x1c set to 1 by init, read by nothing
				 *       here                                 */
	short pwr_in;		/* +0x20 leaky mean square of the input       */
	short pwr_out;		/* +0x22 leaky mean square of the residual    */
	short freeze;		/* +0x24 nonzero: no coefficient update at all */
	short pad26;
	int adapt_near;		/* +0x28 == 1 enables the near update         */
	int adapt_far;		/* +0x2c == 1 enables the far update          */
	short near_rd;		/* +0x30 near tap into `line`                 */
	short far_rd;		/* +0x32 far tap into `line`                  */
	short line_len;		/* +0x34 far_lag + near_delay + far_delay     */
	short pad36;
	short *line;		/* +0x38 line_len packed symbols              */
	short near_idx;		/* +0x3c newest entry in near_i / near_q      */
	short near_len;		/* +0x3e                                      */
	short *near_i;		/* +0x40 near_len entries                     */
	short *near_q;		/* +0x44 near_len entries                     */
	short far_idx;		/* +0x48 newest entry in far_i / far_q        */
	short far_len;		/* +0x4a                                      */
	short *far_i;		/* +0x4c far_len entries                      */
	short *far_q;		/* +0x50 far_len entries                      */
	short *coef[3];		/* +0x54 one per sample phase, each
				 * 2*(near_taps+far_taps) entries laid out as
				 * near-I, near-Q, far-I, far-Q               */
	short mu;		/* +0x60 update gain; init sets 0x29          */
	unsigned short phase;	/* +0x62 sample within the symbol, 0..2       */
	short near_delay;	/* +0x64 INPUT, see above                     */
	short far_delay;	/* +0x66 INPUT, see above                     */
};

/*
 * `fresh` non-zero means the state is uninitialised: allocate the eight
 * buffers.  Zero means re-init in place, keeping them.  A NULL `cfg` means
 * "use the library default", `ECC_CFG`.
 *
 * far_lag + near_delay + far_delay must be non-zero: init divides by it.
 */
void FPM_ECC_init(struct fpm_ecc *state, const struct fpm_ecc_cfg *cfg,
		  int fresh);
void FPM_ECC_free(struct fpm_ecc *state);

/*
 * Subtract the echo estimate from `count` samples IN PLACE and adapt.
 * Returns the number of symbols consumed off the delay line.
 *
 * The return type is unverified: the value is a zero-extended 16-bit count in
 * eax and nothing in the object distinguishes short from int there.
 */
short FPM_ECC_cancel(struct fpm_ecc *state, short *buf, unsigned short count);

/*
 * The library default, from .data:0x8114.  NOT const: it lives in .data,
 * where a const object would have been placed in .rodata -- the same evidence
 * that puts ECCv32_CFG in .rodata.  No maps, so it is a template rather than
 * a usable configuration, exactly as FPM_MRF_CFG_data is for the resampler.
 */
extern struct fpm_ecc_cfg ECC_CFG;

#endif /* DSPLIB_FPM_ECC_H */

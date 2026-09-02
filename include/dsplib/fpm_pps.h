/*
 * fpm_pps.h -- Fixed Point Modem: the generic transmit pulse shaper.
 *
 * A phase interpolator that turns symbols into passband samples: each output
 * takes the symbol history through two separate `taps`-tap filters, one per
 * rail, and returns their DIFFERENCE -- I*cos - Q*sin with the carrier folded
 * into the two coefficient sets, so the block modulates and interpolates in
 * one pass.  `v22_pps.c` is the author's own specialisation of it, and
 * `include/dsplib/v22_pps.h` states the block at length.
 *
 * WHAT DIFFERS FROM THE V.22 SPECIALISATION, and the first two are the same
 * two differences the SRE pair has:
 *
 *   1. THE HISTORY IS CIRCULAR.  V.22 keeps `2 * taps` entries and slides;
 *      this keeps exactly `taps` and wraps `widx`, so each rail's dot product
 *      is two runs -- hist[widx] down to hist[0], then hist[taps-1] down to
 *      hist[widx+1].
 *   2. THE COEFFICIENTS ARE NOT PERMUTED.  V22_PPS_init de-interleaves them
 *      into phase-major order; here phase `p` is selected by starting at
 *      `coeff[p]` and striding `phases`.  The stride is `2 * phases` bytes,
 *      recomputed into four stack slots at entry, which is what makes the
 *      four inner loops read as they do.
 *   3. THE SYMBOL SOURCE HAS TWO FORMS.  With `cfg.mapped` set the ring's
 *      entry is an INDEX -- its low byte, `movzbl` at stride two -- into
 *      `cfg.imap` and `cfg.qmap`; with it clear the ring's own `i` and `q`
 *      arrays are read directly.  V.22 has only the mapped form.
 *   4. EVERYTHING IS CONFIGURED.  V.22's 40 phases, 3 taps and nominal step
 *      of 3 are literals; here they are `cfg.phases`, `cfg.coeffs / phases`
 *      and `cfg.step`, and there is an output gain as well.
 *
 * Reconstructed from dsplibs.o:
 *   FPM_PPS_filter  .text   0x0a9590  753 B
 *   FPM_PPS_init    .text   0x0a98c0  259 B
 *   FPM_PPS_free    .text   0x0a9890   35 B
 *   FPM_PPS_CFG     .rodata 0x00c4a0   40 B
 */

#ifndef DSPLIB_FPM_PPS_H
#define DSPLIB_FPM_PPS_H

struct fpm_smc_ring;

/*
 * Configuration, 40 bytes, copied wholesale into the state by init as ten
 * dword moves.  The built-in `FPM_PPS_CFG` is quoted below for scale only:
 * its four pointers are null and a caller patches them, exactly as for
 * `fpm_sre_cfg` and `fpm_fse_cfg`.
 */
struct fpm_pps_cfg {
	short phases;		/* +0x00 10; also the coefficient stride     */
	/*
	 * The nominal phase step.  `FPM_PPS_init` also seeds `phase` from it,
	 * which is the one thing here with no counterpart in V.22 -- there the
	 * nominal step is the literal 3 and cannot be seeded from.
	 */
	short step;		/* +0x02  3                                  */
	/*
	 * Non-zero: the ring carries constellation INDICES and `imap`/`qmap`
	 * turn them into I and Q.  Zero: the ring carries I and Q directly.
	 * A 32-bit load and test, so it is an int and not a short.
	 */
	int mapped;		/* +0x04  1                                  */
	int scale;		/* +0x08 32767; Q15 gain on the output       */
	/*
	 * Added to `step` on every output.  This is V.22's `cfg.step` -- the
	 * field its `TxClockSync` writes a timing correction into (finding
	 * F3505) -- with the nominal part split out into `step` above.  Zero in
	 * the built-in configuration.
	 *
	 * Read `movzwl`, but the sum is truncated back to 16 bits, so the
	 * signedness is FREE and follows V.22's reading rather than being
	 * measured here.
	 */
	unsigned short step_adj;	/* +0x0c  0                          */
	short pad0e;		/* +0x0e  0 in the built-in instance         */
	const short *imap;	/* +0x10 indexed by the ring entry's low byte */
	const short *qmap;	/* +0x14                                     */
	const short *coeff_i;	/* +0x18 `coeffs` entries, phase-strided     */
	const short *coeff_q;	/* +0x1c                                     */
	short coeffs;		/* +0x20 120; taps = coeffs / phases         */
	short pad22;		/* +0x22  0 in the built-in instance         */
	/*
	 * Copied wholesale by init and read by nothing.  One dword, always
	 * zero -- the same note `v22_pps.h` and `v22_mrf.h` carry.
	 */
	void *aux;		/* +0x24                                     */
};

struct fpm_pps {
	struct fpm_pps_cfg cfg;	/* +0x00 copied wholesale by init            */
	short need;		/* +0x28 1 if the next output takes a new
				 *       symbol first, else 0.  INIT SETS 0,
				 *       where FPM_SRE_init sets its own
				 *       `need` to 1                         */
	/*
	 * +0x2a.  0 .. phases-1 once the filter has run, but INIT SEEDS IT
	 * FROM `cfg.step` rather than from zero and does not reduce it, so a
	 * configuration whose step is not below `phases` starts outside that
	 * range.
	 */
	short phase;
	short widx;		/* +0x2c newest entry of both histories      */
	short taps;		/* +0x2e cfg.coeffs / cfg.phases             */
	short *hist_i;		/* +0x30 `taps` entries, CIRCULAR            */
	short *hist_q;		/* +0x34                                     */
};

/*
 * `fresh` non-zero means the two history buffers do not exist yet: allocate
 * without inspecting them.
 *
 * Zero means re-initialise, and there is a REUSE PATH of the same shape as
 * `FPM_SRE_init`'s: the two buffers are freed and reallocated only if the
 * existing `taps` is smaller than `cfg.coeffs / cfg.phases` now asks for.
 * Every scalar is reset either way and both histories are cleared either way.
 *
 * UNLIKE ITS SIBLING THE TEST GUARDS EXACTLY WHAT IT SIZES -- see deviation
 * D400, and the note at the top of `src/dsp/fpm_pps.c`.  A re-init that
 * raises `cfg.coeffs` without raising the quotient keeps its buffers.
 *
 * `cfg.phases` of zero divides by zero.  The object has no guard.
 */
void FPM_PPS_init(struct fpm_pps *state, const struct fpm_pps_cfg *cfg,
		  int fresh);

/* Releases both histories, `hist_q` first.  Does not clear the pointers. */
void FPM_PPS_free(struct fpm_pps *state);

/*
 * Consume up to `count` SYMBOLS from `src` and write one output sample per
 * phase step, returning how many that was.  `need`, `phase` and `widx`
 * persist and `src->ridx` is advanced and wrapped, so a symbol stream may be
 * handed over in any grouping.
 *
 * `count` and the return are both `unsigned short`, and both are forced:
 * the count is decremented through `movzwl %ax`, and so is the counter.
 */
unsigned short FPM_PPS_filter(struct fpm_pps *state, struct fpm_smc_ring *src,
			      short *out, unsigned short count);

/*
 * The library's own built-in configuration, .rodata 0x00c4a0, 40 bytes,
 * quoted piecemeal in the field comments above; read directly from
 * `.rodata` here rather than reassembled from them.  Its four pointers are
 * NULL with no relocation at any of the four offsets -- a caller patches
 * `imap`/`qmap`/`coeff_i`/`coeff_q` before using it, exactly as the field
 * comments already say.  V.17's transmitter is the first reconstructed
 * consumer (`V17TX_create`, not yet written); nothing here is V.17-specific.
 */
extern const struct fpm_pps_cfg FPM_PPS_CFG;

#endif /* DSPLIB_FPM_PPS_H */

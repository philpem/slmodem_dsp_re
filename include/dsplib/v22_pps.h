/*
 * v22_pps.h -- V.22/V.22bis transmit pulse shaper: a 40-phase interpolator
 * that turns 600-baud symbols into 8000 samples/s of passband signal.
 *
 * `ModDataV22` (blob 0x8e310) runs the pair: `FPM_SMC_encoder` maps bits to
 * constellation points in a symbol ring, then `V22_PPS_filter` walks that
 * ring and interpolates.  Each output takes the symbol's I and Q through two
 * separate three-tap filters and returns their DIFFERENCE, scaled by four --
 * which is I*cos - Q*sin with the carrier folded into the two coefficient
 * sets, so the shaper modulates and interpolates in one pass, exactly as
 * `v22_mrf` demodulates and resamples in one pass on the way back.
 *
 * ---------------------------------------------------------------------------
 * THE RATE, AND WHY THE STEP IS THREE
 *
 * Forty phases, and each output advances the phase by `cfg.step + 3`.  A
 * wrap past 39 is what consumes a symbol, so the outputs per symbol are
 * 40 / (step + 3).  `PPSv22_CFG` is sixteen bytes of zero, so `step` starts
 * at zero and the ratio starts at 40 / 3 = 13.33 outputs per symbol -- 600
 * baud at 8000 samples/s.
 *
 * **IT DOES NOT STAY THERE, AND THE SENTENCE THAT USED TO BE HERE -- THAT
 * NOTHING IN THE OBJECT EVER GIVES `step` A VALUE -- IS WRONG.**
 * `V22FP_create` calls `V22_PPS_init` on `fp + 0x78` (0x87ea1: `mov
 * 0x54(%ebp),%ebx ; add $0x78,%ebx`), and `TxClockSync` stores three times
 * the field at `fp + 0x12a` to that same address.  The state begins with the
 * configuration, so that store lands on `cfg.step`.  Finding F3505.
 *
 * Nothing measured changes: `V22_PPS_STEP` is still the constant 3 the phase
 * update adds, and no test drives `TxClockSync` and the filter together.  What
 * changes is the standing of the 600-baud ratio -- it holds while `fp + 0x12a`
 * is zero and is a derivation about one state of the modem rather than about
 * the block.
 *
 * ---------------------------------------------------------------------------
 * THE SAME FAMILY SHAPE AS `fpm_mrf` AND `v22_mrf`
 *
 * Configuration, then `need`, `phase`, `widx`, `history_len` in that order,
 * then the buffers.  As in `v22_mrf` the history is a SLIDING buffer of twice
 * `history_len` entries rather than a ring, and `V22_PPS_init` permutes the
 * coefficient arrays in place, by the same rule with 40 phases and 3 taps in
 * place of 9 and 30:
 *
 *      after[p * 3 + t]  =  before[40 * (2 - t) + p]      p 0..39, t 0..2
 *
 * so both arrays must be writable and neither can be `PPSv22_COFFS` itself.
 * `V22FP_create` patches `coeff_i` and `coeff_q` in its stack copy of
 * `PPSv22_CFG` with two heap buffers before calling init.
 *
 * Two fields beyond the family shape: +0x20 and +0x24 hold the constellation
 * maps, and INIT DOES NOT SET THEM.  `V22FP_create` writes them directly at
 * 0x87e8d and 0x87e93 -- `SMCv22_IMAP_1200BPS` and `SMCv22_QMAP_1200BPS` --
 * so a caller that only calls init leaves two null pointers the filter will
 * dereference on its first symbol.
 */

#ifndef DSPLIB_V22_PPS_H
#define DSPLIB_V22_PPS_H

struct fpm_smc_ring;

/*
 * Literals in the object, not configured values -- see `v22_mrf.h` for the
 * same note.  `V22_PPS_STEP` is the constant 3 added to `cfg.step`.
 */
#define V22_PPS_PHASES		40	/* phases per symbol             */
#define V22_PPS_STEP		3	/* nominal phase step per output */
#define V22_PPS_TAPS		3	/* taps per phase                */
#define V22_PPS_COEFFS		(V22_PPS_PHASES * V22_PPS_TAPS)	  /* 120 */
#define V22_PPS_HISTORY		(2 * V22_PPS_TAPS)		  /*   6 */

struct v22_pps_cfg {
	/*
	 * +0x00 is added to the nominal step of 3 in the phase update, read
	 * fresh from the state on every output (`movzwl 0x0(%ebp),%esi`), and
	 * unsigned.  It is zero in `PPSv22_CFG` and nothing in the object
	 * assigns it, so no call the object can make ever runs at any other
	 * rate.  The name describes its use in the arithmetic; whether it was
	 * meant as a timing correction or as a rate selector is not
	 * established.
	 */
	unsigned short step;	/* +0x00                                 */
	/*
	 * Named, not implied -- the trap `fpm_mrf.h` documents, and it is
	 * live here.  `V22FP_create` copies `PPSv22_CFG` to the stack a dword
	 * at a time, patches only the two pointers, and init copies all
	 * sixteen bytes into the state, so these two bytes reach a
	 * differential comparison carrying the static's zero.  A struct
	 * assignment over unnamed padding may leave stack garbage instead.
	 */
	short pad02;		/* +0x02                                 */
	short *coeff_i;		/* +0x04 V22_PPS_COEFFS entries, permuted */
	short *coeff_q;		/* +0x08 V22_PPS_COEFFS entries, permuted */
	/*
	 * +0x0c is copied wholesale by init and read by nothing.  Its type is
	 * not established -- only that it is one dword and always zero.  See
	 * the same note on `struct v22_mrf_cfg`.
	 */
	void *aux;		/* +0x0c                                 */
};

struct v22_pps {
	struct v22_pps_cfg cfg;	/* +0x00 copied wholesale by init        */
	short need;		/* +0x10 1 if the next output takes a
				 *       new symbol first, else 0        */
	short phase;		/* +0x12 0 .. 39                         */
	short widx;		/* +0x14 slot the newest symbol is in    */
	short history_len;	/* +0x16 always V22_PPS_TAPS             */
	short *hist_i;		/* +0x18 V22_PPS_HISTORY entries         */
	short *hist_q;		/* +0x1c V22_PPS_HISTORY entries         */
	const short *imap;	/* +0x20 set by the caller, NOT by init  */
	const short *qmap;	/* +0x24 set by the caller, NOT by init  */
};

/*
 * `fresh` non-zero allocates both history buffers; zero adopts whatever the
 * state already holds, with no size check and no free.  Either way the first
 * `history_len` entries of each are zeroed and both coefficient arrays are
 * permuted.  `imap` and `qmap` are untouched.
 */
void V22_PPS_init(struct v22_pps *state, const struct v22_pps_cfg *cfg,
		  int fresh);
void V22_PPS_free(struct v22_pps *state);

/*
 * Consume `count` SYMBOLS from `src`, writing about 13.33 output samples per
 * symbol.  The return is the number of samples written, and `src->ridx` is
 * advanced and wrapped.  `need`, `phase` and `widx` persist, so a symbol
 * stream may be handed over in any grouping.
 */
short V22_PPS_filter(struct v22_pps *state, struct fpm_smc_ring *src,
		     short *out, unsigned short count);

/* The template: sixteen bytes of zero, `const`, hence `.rodata`. */
extern const struct v22_pps_cfg PPSv22_CFG;

/* The 120-tap prototype, in natural order -- see the permutation note. */
extern const short PPSv22_COFFS[V22_PPS_COEFFS];

#endif /* DSPLIB_V22_PPS_H */

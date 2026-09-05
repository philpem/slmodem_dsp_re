/*
 * fpm_smc.h -- Fixed Point Modem: SyMbol Coder.
 *
 * Turns scrambled data words into constellation-point indices.  For V.22bis
 * that is ITU-T V.22bis section 2.4: the first two bits of each symbol select
 * a quadrant CHANGE relative to the previous symbol, and the remaining bits
 * (two of them at 2400 bit/s, none at 1200) select a point within the new
 * quadrant.
 *
 *     quadrant = (quadrant + pmap[(word >> qshift) & qmask]) & pmask
 *     symbol   = quadrant | (word & amask)
 *
 * `pmap` holds the increment for each dibit and V.22's entries are multiples
 * of four, so the quadrant occupies bits 2..3 of a 4-bit index and the
 * amplitude bits occupy bits 0..1.  The index addresses a 16-entry map --
 * SMCv22_IMAP_1200BPS and its three companions -- which fpm_smc.c does NOT
 * read; the pulse shaping filter downstream does.  See
 * src/pump/v22/v22txtab.c.
 *
 * A carrier rotation is then added on top:
 *
 *     out[widx] = (acc + symbol) mod rot_mod        <- one conditional
 *     acc       = (acc + rot_step) mod rot_mod         subtract, not %
 *
 * so a modulator whose carrier advances by a whole number of constellation
 * steps per symbol can be folded into the same index.  V.22 does not use it:
 * SMCv22_CFG sets `rot_step` to 0, leaving `acc` at whatever it was
 * initialised to, and `rot_mod` to 16 -- which still wraps the sum.
 *
 * Because it is one conditional subtract and not a modulo, an input outside
 * [0, 2*rot_mod) comes out unreduced.  That is the object's behaviour.
 */

#ifndef DSPLIB_FPM_SMC_H
#define DSPLIB_FPM_SMC_H

/*
 * 44 bytes, copied wholesale by init (`rep movsl`, 11 dwords).
 *
 * MIXED STRUCT: +0x14, +0x18 and +0x1c are POINTERS (R_386_32 into .rodata).
 * An int16 dump shows them as three pairs of innocuous zeroes; run
 * tools/relocscan.py before believing one.
 */
struct fpm_smc_cfg {
	/*
	 * +0x00 is 1 in SMCv22_CFG and is not read by any fpm_smc function.
	 * Whether it is one int or two shorts is not decidable from this TU:
	 * nothing loads it.
	 *
	 * IT IS DECIDABLE FROM ANOTHER ONE.  `SMC_encoder` loads it whole,
	 * `mov (%edx),%eax` at 0x9faf6, so it is one int; non-zero selects the
	 * symbol-index output V.22 uses and zero selects the complex output
	 * V.29 uses.  Not renamed here only because `src/pump/v22/v22txtab.c`
	 * spells `f00` and that file was outside the pass that measured this.
	 * See include/dsplib/smc.h and finding F8903.
	 */
	int f00;		/* +0x00                                     */
	/*
	 * Non-zero: take the quadrant straight out of the data word instead
	 * of accumulating pmap increments.  Zero for V.22bis, which is
	 * differentially encoded.  Loaded once, outside the loop.
	 */
	int direct;		/* +0x04                                     */
	short rot_step;		/* +0x08 carrier steps added per symbol      */
	short rot_mod;		/* +0x0a modulus of the index, signed        */
	unsigned short qshift;	/* +0x0c right shift selecting the quadrant
				 *       bits: 0 at 1200 bit/s, 2 at 2400    */
	unsigned short qmask;	/* +0x0e mask applied after that shift       */
	unsigned short amask;	/* +0x10 mask selecting the amplitude bits:
				 *       0 at 1200 bit/s, 3 at 2400          */
	unsigned short pmask;	/* +0x12 mask applied to the running quadrant */
	const unsigned short *pmap;	/* +0x14 dibit -> quadrant increment */
	/*
	 * The constellation maps.  NOT read by any fpm_smc function -- they
	 * are carried here for the consumer of the symbol indices, and for
	 * V.22 that consumer keeps its own copy of the pointers, so the pair
	 * below stays on 1200 bit/s for the life of the object however the
	 * rate changes.  Finding F1522.
	 */
	const short *imap;	/* +0x18                                     */
	const short *qmap;	/* +0x1c                                     */
	/*
	 * The carrier phasor, indexed by `acc`.  Zero in SMCv22_CFG and never
	 * read by an `FPM_SMC_*` function -- but read by `SMC_encoder`, the
	 * fax pumps' copy, which multiplies a constellation point by
	 * `cosine[acc] + j*sine[acc]` when `f00` is clear.  Relocated
	 * POINTERS, not ints -- an int16 dump reads them as zeroes, and
	 * modelling them as `int` cannot survive a 64-bit build.  Named
	 * from V29TX_create's own symbols (F8903); see include/dsplib/smc.h.
	 */
	const short *cosine;	/* +0x20                                     */
	const short *sine;	/* +0x24                                     */
	int f28;		/* +0x28 zero in SMCv22_CFG, never read here */
};

struct fpm_smc {
	struct fpm_smc_cfg cfg;	/* +0x00 .. +0x2b copied wholesale by init   */
	short quad;		/* +0x2c running quadrant, cleared by init   */
	short acc;		/* +0x2e running carrier index, ditto        */
};

/*
 * Where the encoder puts its symbol indices: a circular buffer shared with
 * the stage that reads them (for V.22, V22_PPS_filter, which is handed the
 * same pointer immediately afterwards by ModDataV22).
 *
 * TWO SESSIONS EACH MODELLED HALF OF THIS AND THE HALVES FIT.  The one that
 * wrote `fpm_smc.c` saw the encoder's WRITE cursor at +0x0c and read +0x0e as
 * padding; the one that wrote `v22_pps.c` saw the filter's READ cursor at
 * +0x0e and read +0x0c as padding.  Neither was wrong -- it is a ring buffer
 * with a producer and a consumer, they advance independently, and +0x10
 * bounds both.  Merged here rather than kept as two struct tags for one
 * object, which `onedef.py` would have permitted and which would have been a
 * silent second definition in everything but name.
 *
 * +0x00..+0x07 remain unestablished.  The real object is at least 0x12 bytes
 * and its true extent is not known from either TU, so nothing here may be
 * used to size an allocation.
 */
struct fpm_smc_ring {
	/*
	 * The DIRECT rails: with `fpm_pps_cfg.mapped` clear, `FPM_PPS_filter`
	 * reads I and Q straight out of these two instead of through
	 * `imap`/`qmap`.  Only the mapped form is reachable from V.22, which
	 * is why they read as padding until the generic shaper was read
	 * (F3576).
	 */
	short *i;		/* +0x00 `len` I values, direct form         */
	short *q;		/* +0x04 `len` Q values, direct form         */
	short *sym;		/* +0x08 `len` symbol indices, read by
				 *       V22_PPS_filter (stride two, low
				 *       byte used)                          */
	short widx;		/* +0x0c WRITE cursor, advanced per symbol by
				 *       FPM_SMC_encoder                     */
	short ridx;		/* +0x0e READ cursor, advanced and wrapped
				 *       per symbol by V22_PPS_filter        */
	short len;		/* +0x10 wrap point for both cursors (F1546) */
};

/**
 * @brief Load a symbol coder config and clear the quadrant and carrier
 *        accumulators.
 * @param smc  State to initialise.
 * @param cfg  Configuration (mapping mode, masks/shifts, optional carrier).
 */
void FPM_SMC_init(struct fpm_smc *smc, const struct fpm_smc_cfg *cfg);

/**
 * @brief Encode @p count data words into constellation-point indices.
 *
 * Maps each word to a symbol index (quadrant plus amplitude bits, per
 * `cfg.direct`), optionally rotates it by the running carrier index, and
 * writes it into @p ring, advancing `ring->widx` (wrapped at `ring->len`)
 * once per symbol. `quad`, `acc` and `widx` are written back to @p smc /
 * @p ring even when @p count is zero.
 *
 * A `widx` seeded at or past `ring->len` is not brought into range before
 * use: the wrap is a single `widx + 1 < len` test made AFTER the store, so
 * the first symbol goes to the out-of-range slot and the index snaps back
 * to 0 only from the second symbol on. One write past the end, not a run
 * of them -- the object's own behaviour, reproduced here.
 *
 * @param smc    Symbol coder state.
 * @param ring   Output ring buffer; `ring->sym` and `ring->widx` are
 *               written.
 * @param data   Data words to encode, `count` of them.
 * @param count  Number of words in @p data.
 */
void FPM_SMC_encoder(struct fpm_smc *smc, struct fpm_smc_ring *ring,
		     const unsigned short *data, unsigned short count);

#endif /* DSPLIB_FPM_SMC_H */

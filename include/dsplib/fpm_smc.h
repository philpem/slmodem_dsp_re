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
	 * rate changes.  Finding 1522.
	 */
	const short *imap;	/* +0x18                                     */
	const short *qmap;	/* +0x1c                                     */
	int f20;		/* +0x20 zero in SMCv22_CFG, never read here */
	int f24;		/* +0x24 zero in SMCv22_CFG, never read here */
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
 * ONLY the three fields the encoder touches are modelled.  The real object is
 * at least 0x12 bytes and its true extent is not known from this TU, so
 * nothing here may be used to size an allocation.
 */
struct fpm_smc_ring {
	unsigned char pad00[8];	/* +0x00 not read by any fpm_smc function    */
	short *sym;		/* +0x08 `len` symbol indices                */
	short widx;		/* +0x0c write position, advanced per symbol */
	short pad0e;		/* +0x0e not read by any fpm_smc function    */
	short len;		/* +0x10 wrap point for widx                 */
};

/* Load a config and clear the quadrant and carrier accumulators. */
void FPM_SMC_init(struct fpm_smc *smc, const struct fpm_smc_cfg *cfg);

/*
 * Encode `count` data words into `ring`, advancing `ring->widx` (and wrapping
 * it at `ring->len`) once per symbol.  `quad`, `acc` and `widx` are written
 * back even when `count` is zero.
 *
 * A `widx` seeded at or past `len` is not brought into range before it is
 * used: the wrap is a single `widx + 1 < len` test made AFTER the store, so
 * the first symbol goes to the out-of-range slot and the index snaps to 0
 * only for the second.  One write past the end, not a run of them.
 */
void FPM_SMC_encoder(struct fpm_smc *smc, struct fpm_smc_ring *ring,
		     const unsigned short *data, unsigned short count);

#endif /* DSPLIB_FPM_SMC_H */

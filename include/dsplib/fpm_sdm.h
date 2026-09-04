/*
 * fpm_sdm.h -- Fixed Point Modem: Scrambler/Descrambler Module.
 *
 * A self-synchronising scrambler of the shape ITU-T V.22bis section 2.5
 * describes,
 *
 *     out[k] = in[k] ^ out[k - tap1] ^ out[k - tap2]
 *
 * with the descrambler the exact feed-forward inverse
 *
 *     out[k] = in[k] ^ in[k - tap1] ^ in[k - tap2].
 *
 * ---------------------------------------------------------------------------
 * Why the taps are stored as `tap - nbits`
 *
 * This is a WORD-AT-A-TIME implementation: one call to the scrambler consumes
 * an array of 16-bit words, each carrying `nbits` bits of the stream, and
 * scrambles all `nbits` at once from a single pre-update copy of the shift
 * register.  Number the stream bits b[0], b[1], ... in transmission order;
 * word n then carries b[n*nbits .. n*nbits + nbits - 1], and the register
 * holds them so that
 *
 *     reg bit p  ==  b[n * nbits - 1 - p]
 *
 * i.e. bit 0 is the most recent bit and the word's own bits sit reversed in
 * the low `nbits`.  Bit j of a word is therefore b[n*nbits - 1 - j], the
 * MSB of a word being the EARLIEST of its bits in time.  Substituting into
 * the recurrence, the tap-1 term for every j at once is
 *
 *     (reg >> (tap1 - nbits))   bit j
 *
 * which is why init stores the biased shifts and the inner loop is three
 * shifts and two XORs with no per-bit loop at all.
 *
 * The bias is also the implementation's only constraint: `tap >= nbits`.
 * A tap shorter than a word would need a bit of the word being formed, which
 * a parallel update cannot supply.  For V.22bis's 4 bits per symbol against
 * taps of 14 and 17 there is plenty of room.
 *
 * ---------------------------------------------------------------------------
 * The register
 *
 * `reg` is a 32-bit word of which only the low `tap2` bits are ever read, and
 * it is never truncated: bits shifted past 31 are simply lost.  `notmask` is
 * stored, and applied to `reg << nbits` -- where it is a no-op, because that
 * shift has already cleared the low `nbits` bits.  It is reproduced because
 * the object computes and applies it.
 */

#ifndef DSPLIB_FPM_SDM_H
#define DSPLIB_FPM_SDM_H

/*
 * 6 bytes, copied wholesale by init.
 *
 * Callers build one of these on the stack from a static and patch `nbits`
 * before calling init -- V22FP_create does exactly that, taking 2 or 4 from
 * the connection's rate -- so the copy is dword-plus-word and the struct has
 * no tail padding of its own.
 */
struct fpm_sdm_cfg {
	short nbits;		/* +0x00 bits carried by each data word     */
	short tap1;		/* +0x02 first  feedback tap, in bits       */
	short tap2;		/* +0x04 second feedback tap, in bits       */
};

struct fpm_sdm {
	struct fpm_sdm_cfg cfg;	/* +0x00 .. +0x05                           */
	/*
	 * Named, not implied: init copies the 6-byte config in and never
	 * touches these two bytes, so they reach a differential comparison
	 * carrying whatever the object was allocated with.  Making it a
	 * member is what keeps a struct assignment from leaving stack garbage
	 * where the original left the caller's zero.  Same argument as
	 * fpm_mrf_cfg::pad0a.
	 */
	short pad06;		/* +0x06                                    */
	int mask;		/* +0x08 (1 << nbits) - 1                   */
	int notmask;		/* +0x0c ~mask                              */
	unsigned int reg;	/* +0x10 shift register; init clears it     */
	short shift1;		/* +0x14 tap1 - nbits                       */
	short shift2;		/* +0x16 tap2 - nbits                       */
};

/**
 * @brief Load a scrambler/descrambler config and clear the shift register.
 *
 * There is no separate reset entry point: re-running init over a live
 * object is how the register is cleared, which is what a rate change does
 * (see `SetTxRate`, finding F1522).
 *
 * @param sdm  State to initialise.
 * @param cfg  Configuration (`nbits`, and the two taps biased by it).
 */
void FPM_SDM_init(struct fpm_sdm *sdm, const struct fpm_sdm_cfg *cfg);

/**
 * @brief Scramble @p count words in place with the self-synchronising
 *        scrambler.
 *
 * Each word carries `nbits` bits in its low end; the output is masked down
 * to those bits before it both replaces the input word and feeds the shift
 * register.
 *
 * @param sdm    Scrambler state (persists the shift register across calls).
 * @param data   Words to scramble in place, `count` of them.
 * @param count  Number of words in @p data. Counted in 16 bits: a value of
 *               0x10000 (reachable only via a cast past the declared
 *               `unsigned short`) processes nothing.
 */
void FPM_SDM_scrambler(struct fpm_sdm *sdm, unsigned short *data,
		       unsigned short count);

/**
 * @brief Descramble @p count words in place with the feed-forward inverse.
 *
 * The RECEIVED word is fed into the shift register unmasked, so a word with
 * bits set above `nbits` corrupts the register for `tap2 / nbits` symbols
 * afterwards -- the object's own behaviour, reproduced here.
 *
 * @param sdm    Descrambler state (persists the shift register across
 *               calls).
 * @param data   Words to descramble in place, `count` of them.
 * @param count  Number of words in @p data; see FPM_SDM_scrambler() for the
 *               16-bit counting note.
 */
void FPM_SDM_descrambler(struct fpm_sdm *sdm, unsigned short *data,
			 unsigned short count);

#endif /* DSPLIB_FPM_SDM_H */

/*
 * smc.h -- the fax pumps' SyMbol Coder.
 *
 * `SMC_init` is BYTE-FOR-BYTE `FPM_SMC_init` (53 bytes, every byte equal), and
 * `SMC_encoder` is `FPM_SMC_encoder` plus one extra output form.  Both operate
 * on `struct fpm_smc`, `struct fpm_smc_cfg` and `struct fpm_smc_ring` --
 * proved rather than assumed, by every field offset the two encoders load
 * agreeing and by V29TX_create filling the same 44 bytes for both.  One type,
 * one home: those live in `include/dsplib/fpm_smc.h`.
 *
 * ---------------------------------------------------------------------------
 * The extra output form, which is what the 242 extra bytes buy
 *
 * `FPM_SMC_encoder` always writes a symbol INDEX into `ring->sym`, with the
 * carrier rotation folded into the index.  `SMC_encoder` reads `cfg.f00`
 * first and does one of two things per symbol:
 *
 *   f00 != 0   index form.  Identical to FPM_SMC_encoder: the rotation is an
 *              addition into the index and a conditional subtract, and the
 *              result goes to `ring->sym[widx]`.  V.22bis uses this.
 *
 *   f00 == 0   COMPLEX form.  The index addresses `cfg.imap` and `cfg.qmap`
 *              for a constellation point, the accumulator `acc` addresses
 *              `cfg.f20` and `cfg.f24` for a carrier phasor, and the product
 *              goes to `ring->i[widx]` and `ring->q[widx]`:
 *
 *                  i = (imap[k]*cos[a] >> 15) - (qmap[k]*sin[a] >> 15)
 *                  q = (qmap[k]*cos[a] >> 15) + (imap[k]*sin[a] >> 15)
 *
 *              -- one Q15 complex multiply.  `ring->sym` is not written and
 *              the rotated index is not even computed.  V.29 uses this.
 *
 * Everything after that is common: `acc` advances by `rot_step` modulo
 * `rot_mod` (one conditional subtract, not a `%`), and `widx` advances and
 * wraps at `ring->len`.
 *
 * ---------------------------------------------------------------------------
 * What `f00`, `f20` and `f24` are, and why they keep those names
 *
 * `fpm_smc.h` records the three as unread, which was true of the translation
 * unit it was written from.  They are read here, and V29TX_create names two of
 * them in the author's own words -- the four table pointers it stores into its
 * 44-byte config are, in order,
 *
 *     +0x14 V29TX_SMC_PMAP   +0x18 V29TX_SMC_IMAP   +0x1c V29TX_SMC_QMAP
 *     +0x20 V29TX_SMC_COSINE +0x24 V29TX_SMC_SINE
 *
 * (0x9bc33 through 0x9bcae), so `f20` is a cosine table and `f24` is a sine
 * table, indexed by `acc`, and `imap`/`qmap` were already named correctly.
 * `f00` selects between the two output forms above; V.22 sets it to 1 and its
 * shaper reads `sym`, V.29 sets it to 0 and its shaper reads `i` and `q`,
 * which lines up with `fpm_pps_cfg`'s `mapped` -- inference, not a name from
 * the object.
 *
 * THE FIELDS ARE NOT RENAMED HERE.  `f00` is spelled in
 * `src/pump/v22/v22txtab.c`, which this pass may not edit, and a rename that
 * touches one of two definitions is worse than no rename.  The meanings are
 * recorded in finding F8903 and above; the rename is a separate, mechanical
 * change.
 */

#ifndef DSPLIB_SMC_H
#define DSPLIB_SMC_H

#include "dsplib/fpm_smc.h"

/*
 * 44 bytes in `.rodata`, so it cannot be patched in place: V29TX_create copies
 * it to the stack with `rep movsl` and fills the copy in.  Nine of its eleven
 * dwords are zero.
 */
extern const struct fpm_smc_cfg SMC_CFG;

void SMC_init(struct fpm_smc *smc, const struct fpm_smc_cfg *cfg);
void SMC_encoder(struct fpm_smc *smc, struct fpm_smc_ring *ring,
		 const unsigned short *data, unsigned short count);

#endif /* DSPLIB_SMC_H */

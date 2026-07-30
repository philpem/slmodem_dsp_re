/*
 * fpm_agc.c -- Fixed Point Modem: automatic gain control.
 *
 * Reconstructed from dsplibs.o fpm_agc.c:
 *   FPM_AGC_Freeze   .text 0x0a66c0    59 bytes
 *   FPM_AGC_Release  .text 0x0a6700    65 bytes
 *   FPM_AGC_agc      .text 0x0a6750   566 bytes
 *   FPM_AGC_init     .text 0x0a6990   178 bytes
 *
 * ---------------------------------------------------------------------------
 * How the gain is formed
 *
 * The pieces look unrelated until they are composed.  FPM_div normalises the
 * level estimate to `mant` in [0x8000, 0xffff] after `norm` left shifts and
 * returns recip ~= 2^30 / mant, so
 *
 *     recip  ~=  2^30 / (level * 2^norm)
 *     mult    =  (recip * ref_level) >> 15
 *     y       =  ((x * mult) >> 15) << (norm - 1)
 *
 * which collapses to
 *
 *     y  =  x * ref_level / (2 * level).
 *
 * So the loop settles at an output RMS of ref_level/2 -- 8192 for Bell 103's
 * ref_level of 16384, a quarter of full scale.  The factor of two is the
 * `norm - 1`; it is easy to read that as an off-by-one and it is not.
 *
 * Sanity check with the numbers: level = 8192 gives norm = 2, recip = 32768,
 * mult = 16384 and shift = 1, i.e. a gain of exactly 1.0 -- which is what
 * "settled" should mean when the target is 8192.
 *
 * ---------------------------------------------------------------------------
 * Block partitioning, including the part that surprises
 *
 * `count` is divided into blocks of `block_len`.  A remainder shorter than
 * half a block is folded into the last block rather than measured on its own,
 * which stops a stray few samples producing a wild RMS and yanking the gain:
 *
 *     blocks = count / block_len;  tail = count % block_len;
 *     if (block_len/2 <= tail)  blocks++;      // tail is its own block
 *     else                      tail += block_len;
 *
 * Two consequences worth knowing about, both faithfully reproduced:
 *
 *   - `count` below block_len/2 leaves `blocks` at zero, so the samples pass
 *     through COMPLETELY UNTOUCHED -- not gated, not scaled.  For Bell 103
 *     that is any call of 1..17 samples.  `signal` is still written (as 0).
 *
 *   - the folded block can be up to block_len + block_len/2 - 1 = 53 samples,
 *     and FPM_rms is only dimensioned for 36.  At 53 samples of ~82% full
 *     scale the true RMS passes 32767, so the level is simply wrong from
 *     there on -- but it is wrong in a survivable direction, because
 *     FPM_sqrt_dp saturates at 32703 rather than wrapping.  The gain is
 *     under-estimated for very loud blocks; nothing goes negative.
 *
 * ---------------------------------------------------------------------------
 * Why `shift` is never negative here
 *
 * `shift` is `norm - 1`, so it is negative exactly when FPM_div sees a
 * denominator with its top bit already set, i.e. level >= 0x8000.  That
 * cannot happen with this configuration:
 *
 *   - FPM_rms returns FPM_sqrt_dp(...), which CLAMPS its table index and so
 *     never exceeds 32703 -- not even when the accumulator overflows (D2).
 *     So `level` is in [0, 32703], and never negative.
 *
 *   - the smoother keeps `level_est` inside that range too, because
 *     alpha[0] + beta[0] == 32768 exactly in every shipped configuration.
 *
 * It is only the *second* of those that a different config could break: an
 * alpha/beta pair summing to more than 1.0 would let level_est climb until
 * the truncation to short made it negative, and then `shift` would go to -1
 * and the apply loop would shift left by 31.  Such a pair exists in the blob
 * but is never selected -- see D6 in docs/deviations.md.
 */

#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"

void
FPM_AGC_Freeze(struct fpm_agc *agc)
{
	agc->freeze = 1;
}

void
FPM_AGC_Release(struct fpm_agc *agc)
{
	agc->freeze = 0;
	agc->level = 0;	/* the level estimate, but deliberately not the gain */
}

void
FPM_AGC_init(struct fpm_agc *agc, const struct fpm_agc_cfg *cfg, int reset)
{
	agc->signal = 0;
	agc->cfg = *cfg;

	if (reset) {
		agc->f18 = 1;
		agc->mult = 0;
		agc->shift = 0;
		agc->level = 0;
	}

	/* Unconditional, and in this order -- the same pair Release writes. */
	agc->freeze = 0;
	agc->level = 0;
}

void
FPM_AGC_agc(struct fpm_agc *agc, short *samples, unsigned short count)
{
	const unsigned block_len = agc->cfg.block_len;
	const int acquire_level = agc->cfg.acquire_level;
	const int squelch_level = agc->cfg.squelch_level;
	const int ref_level = agc->cfg.ref_level;

	int level_est = agc->level;
	int mult = agc->mult;
	int shift = agc->shift;
	int adjusted = 0;		/* blocks that took the gain path */
	unsigned blocks, tail;
	int i;

	blocks = (unsigned short)(count / block_len);
	tail = (unsigned short)(count % block_len);

	if ((unsigned short)(block_len >> 1) <= (unsigned short)tail)
		blocks = (unsigned short)(blocks + 1);
	else
		tail = (unsigned short)(tail + block_len);

	/*
	 * Counted down, and the *last* block is the one carrying `tail`.
	 * The index is 16-bit in the original; blocks == 0 gives -1 and the
	 * loop does not run at all.
	 */
	for (i = (short)(blocks - 1); i >= 0; i--) {
		unsigned short len = (unsigned short)(i ? block_len : tail);
		int level = FPM_rms(samples, len);
		int k;

		/*
		 * The gate.  Which threshold applies depends on whether a gain
		 * has ever been computed: `mult` is zero only until the first
		 * successful update after a reset.
		 */
		if ((level < acquire_level && mult == 0) ||
		    (level < squelch_level && mult != 0)) {
			for (k = 0; k < (int)len; k++)
				samples[k] = 0;
			samples += len;
			continue;
		}

		adjusted = (short)(adjusted + 1);

		/*
		 * First-order smoother.  Note the asymmetry: the alpha term is
		 * left at full width and only the *sum* is truncated back to
		 * 16 bits.  Both coefficients are 16384 for every configuration
		 * in the blob, i.e. level = (level + measured) / 2.
		 */
		level_est = (agc->cfg.alpha[0] * level_est) >> 15;
		level_est = (short)(level_est +
				    ((agc->cfg.beta[0] * level) >> 15));

		if (agc->freeze == 0 && level_est != 0) {
			unsigned short recip, norm;

			FPM_div((unsigned short)level_est, &recip, &norm);
			mult = (short)(((int)recip * ref_level) >> 15);
			shift = (short)(norm - 1);	/* the /2; see above */
		}

		for (k = 0; k < (int)len; k++) {
			int v = (short)((samples[k] * mult) >> 15);

			/*
			 * The original loads `shift` with movzbl and shifts
			 * with `shl %cl`, so the count is masked to five bits.
			 * That only matters if `shift` is negative, which no
			 * shipped configuration can produce -- see the header
			 * comment.  Masked here so the two still agree if one
			 * ever does.
			 */
			v = (int)((unsigned)v << (shift & 31));

			if (v > 32767)
				samples[k] = 32767;
			else if (v < -32768)
				samples[k] = -32768;	/* not -32767; cf. FPM_iir_filt */
			else
				samples[k] = (short)v;
		}

		samples += len;
	}

	agc->level = (short)level_est;
	agc->mult = (short)mult;
	agc->shift = (short)shift;

	/* "Most of this buffer was above the gate." */
	agc->signal = adjusted > (int)(blocks >> 1);
}

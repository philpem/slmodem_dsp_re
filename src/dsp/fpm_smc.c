/*
 * fpm_smc.c -- Fixed Point Modem: SyMbol Coder (constellation mapper).
 *
 * Reconstructed from dsplibs.o fpm_smc.c:
 *   FPM_SMC_encoder  .text 0x0a9bc0   367 bytes
 *   FPM_SMC_init     .text 0x0a9d30    53 bytes
 *
 * What the mapping is, and what the config fields mean, is in
 * include/dsplib/fpm_smc.h.  Three things about the arithmetic are worth
 * having in front of you while reading this:
 *
 *   - The quadrant is masked as a 32-bit value and only THEN truncated to a
 *     short for the next symbol.  The symbol index is formed from the
 *     untruncated one, so the two can disagree above bit 15.
 *
 *   - Both wraps are a single conditional subtract, so a value at or above
 *     2 * rot_mod comes out unreduced, and a negative `rot_mod` makes the
 *     test always true.  Neither is a modulo and neither is clamped.
 *
 *   - The write index wraps on `widx + 1 < len`, tested after the increment.
 *     An index seeded at or beyond `len` is never brought back.
 *
 * Each is reachable only from a config V.22 does not use, which is why the
 * differential test builds synthetic ones.
 */

#include "dsplib/fpm_smc.h"

void
FPM_SMC_init(struct fpm_smc *smc, const struct fpm_smc_cfg *cfg)
{
	smc->cfg = *cfg;
	smc->quad = 0;
	smc->acc = 0;
}

void
FPM_SMC_encoder(struct fpm_smc *smc, struct fpm_smc_ring *ring,
		const unsigned short *data, unsigned short count)
{
	const unsigned short *pmap = smc->cfg.pmap;
	const int direct = smc->cfg.direct;
	const int qshift = smc->cfg.qshift;
	const int qmask = smc->cfg.qmask;
	const int amask = smc->cfg.amask;
	const int pmask = smc->cfg.pmask;
	const int rot_step = smc->cfg.rot_step;
	const int rot_mod = smc->cfg.rot_mod;
	short *sym = ring->sym;
	const int len = ring->len;
	short quad = smc->quad;
	short acc = smc->acc;
	int widx = ring->widx;

	while (count--) {
		unsigned short word = *data++;
		int q;
		int index;
		int out;

		if (direct) {
			/* Absolute: the quadrant is in the word as it stands. */
			q = word & qmask;
		} else {
			/*
			 * Differential: the selected bits index a table of
			 * quadrant CHANGES, which accumulate.
			 */
			q = (quad + pmap[(word >> qshift) & qmask]) & pmask;
		}
		quad = (short)q;
		index = q | (word & amask);

		/* Carrier rotation folded into the same index. */
		out = (unsigned short)(acc + index);
		if (out >= rot_mod)
			out = (unsigned short)(out - rot_mod);
		sym[widx] = (short)out;

		acc = (short)(acc + rot_step);
		if (acc >= rot_mod)
			acc = (short)(acc - rot_mod);

		widx = (short)(widx + 1);
		if (widx >= len)
			widx = 0;
	}

	smc->acc = acc;
	smc->quad = quad;
	ring->widx = (short)widx;
}

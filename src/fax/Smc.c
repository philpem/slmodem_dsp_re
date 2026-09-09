/*
 * Smc.c -- the fax pumps' SyMbol Coder.
 *
 * Reconstructed from dsplibs.o:
 *   SMC_encoder  .text 0x09fa10   609 bytes
 *   SMC_init     .text 0x09fc80    53 bytes
 *   SMC_CFG      .rodata 0x00bb40   44 bytes
 *
 * `SMC_init` is BYTE-FOR-BYTE `FPM_SMC_init` -- all 53 bytes equal -- so it is
 * the same source in a second translation unit.  `SMC_encoder` is
 * `FPM_SMC_encoder` with one more output form; what the form is and how the
 * config fields were named is in `include/dsplib/smc.h`, and what the mapping
 * and the two wraps mean is in `include/dsplib/fpm_smc.h`.
 *
 * Three things about the arithmetic are worth having in front of you, and all
 * three are inherited from the generic encoder rather than new here:
 *
 *   - the quadrant is masked as a 32-bit value and only THEN truncated to a
 *     short for the next symbol, so the index and the stored state can
 *     disagree above bit 15;
 *
 *   - both wraps are a single conditional subtract, so a value at or above
 *     2 * rot_mod comes out unreduced and a negative `rot_mod` makes the test
 *     always true;
 *
 *   - the write index wraps on `widx + 1 < len`, tested after the increment,
 *     so an index seeded at or past `len` writes out of range once.
 *
 * What IS new is the complex form's indices: `imap`/`qmap` are addressed by
 * the symbol index and `cosine`/`sine` by the carrier accumulator as it stood
 * BEFORE this symbol's step, and neither is bounds-checked against anything.
 */

#include "dsplib/smc.h"

/*
 * The object's table.  Nine of its eleven dwords are zero and every caller
 * copies it and fills the rest in: V29TX_create takes `direct`, `rot_step`
 * 0x11, `rot_mod` 0x18 and the five table pointers from its own V29TX_SMC_*
 * symbols, and clears `f00` to select the complex form.
 */
const struct fpm_smc_cfg SMC_CFG = {
	1,			/* f00      */
	1,			/* direct   */
	3,			/* rot_step */
	8,			/* rot_mod  */
	0,			/* qshift   */
	2,			/* qmask    */
	0,			/* amask    */
	0,			/* pmask    */
	0,			/* pmap     */
	0,			/* imap     */
	0,			/* qmap     */
	0,			/* cosine   */
	0,			/* sine     */
	0			/* f28      */
};

void
SMC_init(struct fpm_smc *smc, const struct fpm_smc_cfg *cfg)
{
	smc->cfg = *cfg;
	smc->quad = 0;
	smc->acc = 0;
}

void
SMC_encoder(struct fpm_smc *smc, struct fpm_smc_ring *ring,
	    const unsigned short *data, unsigned short count)
{
	const unsigned short *pmap = smc->cfg.pmap;
	const short *imap = smc->cfg.imap;
	const short *qmap = smc->cfg.qmap;
	const short *cosine = smc->cfg.cosine;
	const short *sine = smc->cfg.sine;
	const int qshift = smc->cfg.qshift;
	const int qmask = smc->cfg.qmask;
	const int amask = smc->cfg.amask;
	const int pmask = smc->cfg.pmask;
	const int rot_step = smc->cfg.rot_step;
	const int rot_mod = smc->cfg.rot_mod;
	short *rail_i = ring->i;
	short *rail_q = ring->q;
	short *sym = ring->sym;
	const int len = ring->len;
	short quad = smc->quad;
	short acc = smc->acc;
	int widx = ring->widx;
	/*
	 * Both selectors are loaded ONCE and outside the loop, so a config
	 * edited under a running encoder is not seen until the next call.
	 * `f00` picks the output form; `direct` picks the quadrant rule.
	 */
	const int mapped = smc->cfg.f00;
	const int direct = smc->cfg.direct;

	while (count--) {
		unsigned short word = *data++;
		int q;
		int index;

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

		if (mapped) {
			/* Carrier rotation folded into the same index. */
			int out = (unsigned short)(acc + index);

			if (out >= rot_mod)
				out = (unsigned short)(out - rot_mod);
			sym[widx] = (short)out;
		} else {
			/*
			 * Carrier rotation done as a Q15 complex multiply of
			 * the constellation point by the phasor at `acc`.
			 * `acc` here is the value BEFORE this symbol's step.
			 */
			int c = cosine[acc];
			int s = sine[acc];

			rail_i[widx] = (short)((imap[index] * c >> 15)
					       - (qmap[index] * s >> 15));
			rail_q[widx] = (short)((qmap[index] * c >> 15)
					       + (imap[index] * s >> 15));
		}

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

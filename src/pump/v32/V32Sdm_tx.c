/*
 * V32Sdm_tx.c -- ITU-T V.32/V.32bis: the self-synchronising SCRAMBLER.
 *
 * Reconstructed from dsplibs.o:
 *   SDMv32_scrambler    .text 0x085950   349
 *
 * Split out of v32scram.c.  The object names V32Sdm_rx.c and V32Sdm_tx.c as
 * consecutive FILE records (V32RNG.c, V32Sdm_rx.c, V32Sdm_tx.c, V32ans.c), and
 * the address run [SDMv32_descrambler 0x857f0, SDMv32_scrambler 0x85950) is
 * the receiver half while [0x85950, V32AnsNextState 0x85ab0) is this one.  The
 * two functions differ in one `or` -- the scrambler feeds back what it
 * produced, the descrambler what it was given; the shared analysis (the
 * sixteen-bit countdown, the walking local, the residual spill placement) is
 * in V32Sdm_rx.c beside the descrambler.
 */

#include "dsplib/v32scram.h"

#define SDM_SPLIT_GROUP		6
#define SDM_SPLIT_SHIFT		3

void
SDMv32_scrambler(struct v32_sdm *sdm, short *buf, unsigned short count)
{
	const unsigned int outmask = sdm->outmask;
	const unsigned int regmask = sdm->regmask;
	unsigned int reg = sdm->reg;
	int shift = sdm->group;
	short *p = buf;

	if (sdm->group == SDM_SPLIT_GROUP)
		shift = SDM_SPLIT_SHIFT;

	while (count--) {
		unsigned int in = (unsigned short)p[0];
		unsigned int low = in & 7;
		unsigned int out;

		if (sdm->group == SDM_SPLIT_GROUP)
			in = (in & 0x38) >> 3;

		out = (in ^ (reg >> sdm->tap1) ^ (reg >> sdm->tap2)) & 0xffffu;
		out &= outmask;
		reg = ((reg << shift) & regmask) | out;

		if (sdm->group == SDM_SPLIT_GROUP) {
			unsigned int out2;

			out2 = (low ^ (reg >> sdm->tap1)
				^ (reg >> sdm->tap2)) & 0xffffu;
			out2 &= outmask;
			reg = ((reg << shift) & regmask) | out2;
			p[0] = (short)((out << 3) | out2);
		} else {
			p[0] = (short)out;
		}
		p++;
	}

	sdm->reg = reg;
}

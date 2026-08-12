/*
 * v32scram.c -- ITU-T V.32/V.32bis: the self-synchronising scrambler pair.
 *
 * Reconstructed from dsplibs.o:
 *   SDMv32_descrambler  .text 0x0857f0   349
 *   SDMv32_scrambler    .text 0x085950   349
 *
 * Both are one loop over `count` words with a two-tap feedback register, and
 * they differ in one `or`: the scrambler feeds back what it produced, the
 * descrambler what it was given.  See v32scram.h for the two recurrences.
 *
 * WHAT THE OBJECT FORCES, and each of these is visible in the encoding rather
 * than inferred:
 *
 *  - `group` is re-read from memory on every iteration (`movzwl (%eax),%edi`
 *    at the top of the loop) but the SHIFT derived from it is computed once
 *    before it (`0xc(%esp)`).  Nothing writes `group`, so the two cannot
 *    disagree; it is mirrored anyway because it is what the code does.
 *  - the result is truncated to 16 bits (`movzwl %ax,%eax`) BEFORE `outmask`
 *    is applied, not after.
 *  - in the 6-bit case the second group's taps are read from the register
 *    that the FIRST group has already updated, so the two halves of a word
 *    are not independent.
 *  - the not-6 case feeds the WHOLE 16-bit input word into the xor and, in
 *    the descrambler, into the register -- it is not masked to `group` bits
 *    first.  For a caller that hands over only `group` significant bits the
 *    two are the same; the object does not assume it.
 *
 * The taps are right shifts of a register whose length `regmask` fixes, so a
 * tap value of n selects the bit n places old.  V.32's polynomials are
 * GPA = 1 + x^-18 + x^-23 and GPC = 1 + x^-5 + x^-23, and `SDMv32_GPA` and
 * `SDMv32_GPC` in v32scram_tables.c hold 5 and 18 in the two orders -- but
 * nothing in the object reads either of them, so which field of the state
 * they are meant to reach is not established here.
 */

#include "dsplib/v32scram.h"

/*
 * The 6-bit word is two 3-bit groups and the shift is 3.  Both constants are
 * literals in the object (`cmp $0x6`, `mov $0x3`), so this is a special case
 * the author wrote and not `group / 2` in disguise.
 */
#define SDM_SPLIT_GROUP		6
#define SDM_SPLIT_SHIFT		3

void
SDMv32_scrambler(struct v32_sdm *sdm, short *buf, unsigned short count)
{
	const unsigned int outmask = sdm->outmask;
	const unsigned int regmask = sdm->regmask;
	unsigned int reg = sdm->reg;
	int shift = sdm->group;
	unsigned int i;

	if (sdm->group == SDM_SPLIT_GROUP)
		shift = SDM_SPLIT_SHIFT;

	for (i = 0; i < count; i++) {
		unsigned int in = (unsigned short)buf[i];
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
			buf[i] = (short)((out << 3) | out2);
		} else {
			buf[i] = (short)out;
		}
	}

	sdm->reg = reg;
}

void
SDMv32_descrambler(struct v32_sdm *sdm, short *buf, unsigned short count)
{
	const unsigned int outmask = sdm->outmask;
	const unsigned int regmask = sdm->regmask;
	unsigned int reg = sdm->reg;
	int shift = sdm->group;
	unsigned int i;

	if (sdm->group == SDM_SPLIT_GROUP)
		shift = SDM_SPLIT_SHIFT;

	for (i = 0; i < count; i++) {
		unsigned int in = (unsigned short)buf[i];
		unsigned int low = in & 7;
		unsigned int out;

		if (sdm->group == SDM_SPLIT_GROUP)
			in = (in & 0x38) >> 3;

		out = (in ^ (reg >> sdm->tap1) ^ (reg >> sdm->tap2)) & 0xffffu;
		out &= outmask;
		reg = ((reg << shift) & regmask) | in;

		if (sdm->group == SDM_SPLIT_GROUP) {
			unsigned int out2;

			out2 = (low ^ (reg >> sdm->tap1)
				^ (reg >> sdm->tap2)) & 0xffffu;
			out2 &= outmask;
			reg = ((reg << shift) & regmask) | low;
			buf[i] = (short)((out << 3) | out2);
		} else {
			buf[i] = (short)out;
		}
	}

	sdm->reg = reg;
}

/*
 * V32Sdm_rx.c -- ITU-T V.32/V.32bis: the self-synchronising DESCRAMBLER.
 *
 * Reconstructed from dsplibs.o:
 *   SDMv32_descrambler  .text 0x0857f0   349
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
SDMv32_descrambler(struct v32_sdm *sdm, short *buf, unsigned short count)
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
		reg = ((reg << shift) & regmask) | in;

		if (sdm->group == SDM_SPLIT_GROUP) {
			unsigned int out2;

			out2 = (low ^ (reg >> sdm->tap1)
				^ (reg >> sdm->tap2)) & 0xffffu;
			out2 &= outmask;
			reg = ((reg << shift) & regmask) | low;
			p[0] = (short)((out << 3) | out2);
		} else {
			p[0] = (short)out;
		}
		p++;
	}

	sdm->reg = reg;
}
/*
 * THE LOOP COUNTER IS A SIXTEEN-BIT COUNTDOWN, and that is read off the
 * object rather than chosen.  Both functions open
 *
 *     dec %eax ; movzwl %ax,%ecx ; inc %ax ; mov %ecx,0x10(%esp) ; jne
 *
 * and repeat the same four-instruction motif at the foot of every iteration.
 * The truncation to sixteen bits on EVERY pass is the counter's declared
 * type; it is not something strength reduction makes of a 32-bit induction
 * variable, which is the distinction finding F7941 draws.  Twenty-two cells
 * were compiled over {counter type} x {direction} x {subscript, walking
 * pointer} and the motif appears in the eight `while (n--)` cells and in no
 * other -- `for (i = 0; i < count; i++)` with `i` either 16 or 32 bits does
 * not produce it at any buffer spelling.  `while (count--)` and a local
 * `unsigned short n = count` are indistinguishable here, so the parameter is
 * used directly.
 *
 * AND THE BUFFER IS WALKED THROUGH A LOCAL, WHICH IS A SECOND FORCED
 * OBSERVABLE AND NOT A TIDYING.  The object walks `buf` with `add $0x2,%esi`
 * -- the pointer lives in a register.  Only a local `short *p = buf`
 * reproduces that; walking the PARAMETER gives `addl $0x2,0x30(%esp)`, a
 * read-modify-write on its own incoming slot, and subscripting gives a scaled
 * index and no walk at all.  Counted as (register walk, memory walk):
 *
 *                            scrambler   descrambler
 *     the object               (2, 0)      (2, 0)
 *     a local `short *p`       (2, 0)      (0, 2)
 *     the parameter, `buf++`   (0, 2)      (0, 2)
 *     `buf[i]`                 (0, 0)      (0, 0)
 *
 * -- so the local buys the object's own form in the SCRAMBLER and nothing in
 * the descrambler, where no cell of the twenty-two gets the pointer into a
 * register at all.  Do not read the local as fixing both.  The `buf[i]` row is
 * why F7941 does not bite here: 3.4.2 does NOT strength-reduce this subscript
 * into a walking pointer, so writing one is not handing the compiler back what
 * it would have derived.
 *
 * It does not CLOSE either function: the scrambler goes 368 bytes to 352
 * against the object's 349 and the descrambler stays at 365, and what is left
 * is spill placement -- the object keeps `reg` in a register and spills the
 * three shift amounts to stack slots, and we do the exact opposite.  Findings
 * F8242 and F8243.
 */

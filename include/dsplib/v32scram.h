/*
 * v32scram.h -- ITU-T V.32/V.32bis: the self-synchronising scrambler pair.
 *
 * `SDM` is the author's prefix.  One state object serves both directions;
 * `SDMv32_scrambler` and `SDMv32_descrambler` are the two halves of the
 * standard self-synchronising arrangement:
 *
 *     scrambler     out = (in ^ (reg >> tap1) ^ (reg >> tap2)) & outmask
 *                   reg = ((reg << shift) & regmask) | out
 *
 *     descrambler   out = (in ^ (reg >> tap1) ^ (reg >> tap2)) & outmask
 *                   reg = ((reg << shift) & regmask) | in
 *
 * -- identical except for WHICH value is fed back into the register, which is
 * what makes the pair self-synchronising: the descrambler's register is
 * driven by the line and needs no agreement on an initial state.
 *
 * `group` is bits per symbol and also the shift, EXCEPT at 6, where the
 * object special-cases a 6-bit word into two 3-bit groups, most significant
 * first, and shifts by 3.  The check is against the literal 6 and the shift
 * is the literal 3, so this is a special case in the original and not an
 * arithmetic rule that happens to fit.
 */

#ifndef DSPLIB_V32SCRAM_H
#define DSPLIB_V32SCRAM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 24 bytes.  +0x02 through +0x07 are read by neither function; they are left
 * as named padding rather than guessed at, since no writer of this object is
 * reconstructed yet.
 */
struct v32_sdm {
	short group;		/* +0x00 bits per symbol; 6 means 3 + 3     */
	short pad02;		/* +0x02 not read here                      */
	int pad04;		/* +0x04 not read here                      */
	unsigned int outmask;	/* +0x08 applied to every scrambled group   */
	unsigned int regmask;	/* +0x0c register length, as a bit mask     */
	unsigned int reg;	/* +0x10 the shift register                 */
	short tap1;		/* +0x14 first  feedback tap, a right shift */
	short tap2;		/* +0x16 second feedback tap, a right shift */
};

/* Both work in place over `count` 16-bit words and update `reg`. */
void SDMv32_scrambler(struct v32_sdm *sdm, short *buf, unsigned short count);
void SDMv32_descrambler(struct v32_sdm *sdm, short *buf, unsigned short count);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32SCRAM_H */

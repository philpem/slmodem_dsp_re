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
	short tap1_pos;	/* +0x02 absolute first tap position         */
	short tap2_pos;	/* +0x04 absolute second tap position        */
	short pad06;		/* +0x06                                    */
	unsigned int outmask;	/* +0x08 applied to every scrambled group   */
	unsigned int regmask;	/* +0x0c register length, as a bit mask     */
	unsigned int reg;	/* +0x10 the shift register                 */
	short tap1;		/* +0x14 first  feedback tap, a right shift */
	short tap2;		/* +0x16 second feedback tap, a right shift */
};

/*
 * The three tables `V32FP_recreate` seeds a `struct v32_sdm` from, and THE
 * WAY IT USES THEM IS WHAT SETTLES THEIR SHAPE.  `src/pump/v32/
 * v32scram_tables.c` used to say all three were unreferenced and that
 * therefore only their element WIDTH could be established; that was wrong,
 * and V32FP_recreate is the referrer (finding F8655).
 *
 *   SDMv32_CFG   a three-word TEMPLATE for the first six bytes of a
 *                `struct v32_sdm` -- `group`, then the two ABSOLUTE tap
 *                positions v32fpctl.h names V32_SDM_TAP1_POS and
 *                V32_SDM_TAP2_POS.  Entries 0 and 1 are overwritten on every
 *                path; only entry 2, the 23, survives, and it lands in
 *                `tap2_pos`.
 *   SDMv32_GPC   the TRANSMIT first tap position, indexed by the half-duplex
 *   SDMv32_GPA   mode; the RECEIVE one, indexed the same way.  0, 1 and 2 are
 *                the three modes `V32FP_recreate` can install, so at least
 *                three of the four entries are reachable.
 */
extern const short SDMv32_GPA[4];
extern const short SDMv32_GPC[4];
extern const short SDMv32_CFG[3];

/**
 * @brief Scramble `count` V.32 words in place with the self-synchronising scrambler.
 * @param sdm    The scrambler state; `reg` is updated in place.
 * @param buf    The words to scramble, in place.
 * @param count  How many words.
 */
void SDMv32_scrambler(struct v32_sdm *sdm, short *buf, unsigned short count);

/**
 * @brief Descramble `count` V.32 words in place with the self-synchronising descrambler.
 * @param sdm    The descrambler state; `reg` is updated in place.
 * @param buf    The words to descramble, in place.
 * @param count  How many words.
 */
void SDMv32_descrambler(struct v32_sdm *sdm, short *buf, unsigned short count);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32SCRAM_H */

/*
 * v32dec.h -- V.32's slicers, their constellation tables, and the part of
 * the datapump object they touch.
 *
 * `FPM_FSE_receive` calls `cfg.decision(state, &angle, &mag)` once per
 * symbol.  The slicer reads the symbol the equaliser just wrote to
 * `state->out_i[n_out]` / `out_q[n_out]`, decides which constellation point
 * it is, writes that point's ideal angle back over `angle` -- which is what
 * gives the carrier PLL its error -- writes an ideal magnitude to `mag`, and
 * returns the decoded bits.
 *
 * FIVE of the nine slicers are NOT here.  `FSE_decision_16Tpt`, `_32pt`,
 * `_64pt` and `_128pt` all call `VTB_decoder`, which is 1,773 bytes of the
 * Viterbi block plus about 10 KB of trellis tables and belongs to a different
 * batch (finding 1602); `FSE_decision_16pt` indexes `DECv32_MAG9600` out of
 * bounds and cannot be reproduced across builds (finding 1603, D300).
 * Neither group is prototyped here: a declaration with no definition would be
 * a claim this tree cannot honour.
 */

#ifndef DSPLIB_V32DEC_H
#define DSPLIB_V32DEC_H

#include "dsplib/fpm_fse.h"

/*
 * THE DATAPUMP OBJECT, AS SEEN THROUGH THE SLICERS.
 *
 * This is `fpm_fse_cfg::owner`: the V.32 datapump's own object, which nothing
 * in the tree constructs yet.  Only the fields the four slicers here touch
 * are named; everything else is padding sized so the named ones land where
 * the disassembly puts them.  WHEN `v32_create` IS RECONSTRUCTED THIS TYPE
 * MUST BECOME THAT OBJECT'S TYPE rather than a second declaration of it.
 *
 * `vtb` is not a guess about the Viterbi decoder's size: `FSE_decision_16Tpt`
 * passes `owner + 0x18` to `VTB_decoder`, and `owner + 0x50` is in use here,
 * so whatever the decoder keeps there is at most 56 bytes.
 */
struct v32_dec {
	unsigned short chan;		/* +0x00 selects prev_sym[]          */
	unsigned char pad02[6];		/* +0x02                             */
	short prev_sym[8];		/* +0x08 last absolute quadrant pair */
	unsigned char vtb[0x38];	/* +0x18 VTB_decoder's state         */
	short ang_prev;			/* +0x50 previous angle (AB only)    */
	short sym_i;			/* +0x52 the symbol just decided     */
	short sym_q;			/* +0x54                             */
	short sym_i1;			/* +0x56 one symbol back             */
	short sym_q1;			/* +0x58                             */
	short sym_i2;			/* +0x5a two back (AB only)          */
	short sym_q2;			/* +0x5c                             */
	short eqm;			/* +0x5e leaky decision-error measure*/
	short eqm_b;			/* +0x60 AB's second accumulator     */
	short retrain;			/* +0x62 1 or 2; a request, not a
					 *       state -- nothing here reads
					 *       it back                     */
	int rate_change;		/* +0x64 set by trn and by AB        */
	unsigned char pad68[4];		/* +0x68                             */
	short scram_tap;		/* +0x6c shift for the TRN generator */
	unsigned short count;		/* +0x6e symbols in the current phase*/
	unsigned int scram;		/* +0x70 TRN generator's register    */
};

unsigned short FSE_decision_4pt(struct fpm_fse *state, short *angle,
				short *mag);
unsigned short FSE_decision_trn(struct fpm_fse *state, short *angle,
				short *mag);
unsigned short FSE_decision_CD(struct fpm_fse *state, short *angle,
			       short *mag);
unsigned short FSE_decision_AB(struct fpm_fse *state, short *angle,
			       short *mag);

/*
 * The constellations, as (I, Q) pairs at +-4096 and +-12288, with the ideal
 * angle and magnitude of each point beside them.  `.data` in the object, so
 * not declared const.
 */
extern short DECv32_ANGL1200[4];
extern short DECv32_MAP_TRN[4];
extern short DECv32_IMAP4[4];
extern short DECv32_QMAP4[4];
extern short DECv32_MAG9600[3];
extern short DECv32_ANGL9600[16];
extern short DECv32_IMAP16[16];
extern short DECv32_QMAP16[16];

/*
 * NOT PART OF THIS BATCH.  These three belong to the trellis/differential
 * encoder block (`SMCv32_*`) and are defined alongside the DEC tables only
 * because the slicers index them and nothing else in the tree defines them.
 * The two 17-entry ones are 0x22 bytes in the object; only 0..15 is ever
 * indexed, and the seventeenth entry is zero.
 */
extern const short SMCv32_IMAP16[17];
extern const short SMCv32_QMAP16[17];
extern const short SMCv32_PMAP16[4];

#endif /* DSPLIB_V32DEC_H */

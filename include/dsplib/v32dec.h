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
 * ALL NINE ARE HERE NOW.  `FSE_decision_16pt` was the last one out: it
 * indexes `DECv32_MAG9600` thousands of entries past the end (finding 1603,
 * D302) and only ONE of its three reachable indices reads a byte the blob
 * carries with it into a link, so that store is fixed behind
 * `DSPLIB_REPRODUCE_BUGS` and only that ring's `*mag` is compared.
 * Everything else about the function is ordinary and is compared on every
 * trial -- findings 3800 and 3801.  The four trellis ones -- `_16Tpt`,
 * `_32pt`, `_64pt` and `_128pt` -- were blocked on `VTB_decoder` (finding
 * 1602) until finding 3210's Viterbi batch landed it.
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
 * so whatever the decoder keeps there is at most 56 bytes.  Finding 3210
 * measured `sizeof(struct vtb)` at exactly 0x38, so the bound is now met
 * exactly -- but the field stays a byte array and the four trellis slicers
 * cast it, because `struct vtb` holds four POINTERS: declaring it as the
 * struct would make `struct v32_dec` a different size in the 64-bit build and
 * every offset after +0x18 in this comment false.  The 32-bit layout is the
 * one the object fixes, and the cast is where the two meet.
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
 * 9600 bit/s with no trellis: a sixteen-point decision reported as four
 * differentially encoded bits.  It installs no successor.
 */
unsigned short FSE_decision_16pt(struct fpm_fse *state, short *angle,
				 short *mag);

/*
 * THE FOUR TRELLIS SLICERS.  Each decides a point, writes its ideal angle and
 * magnitude, and then hands the UNROTATED received symbol to `VTB_decoder`,
 * whose output through a stack local is the return value.  None of them
 * installs a successor: the datapump leaves a trellis rate by another route.
 */
unsigned short FSE_decision_16Tpt(struct fpm_fse *state, short *angle,
				  short *mag);
unsigned short FSE_decision_32pt(struct fpm_fse *state, short *angle,
				 short *mag);
unsigned short FSE_decision_64pt(struct fpm_fse *state, short *angle,
				 short *mag);
unsigned short FSE_decision_128pt(struct fpm_fse *state, short *angle,
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
 * The trellis rates' tables.  The five `.data` ones first, then the eight
 * `.rodata` ones, and the split is which section the object defines each in.
 *
 * `ANA_QMAP` is the 32-point slicer's whole constellation: that decision is
 * one-dimensional, a search in Q alone over at most three candidates, and the
 * only I coordinates the function ever names are the two literals in its
 * tie-break.  `ANA_{I,Q}MAP128` is the 128-point one folded into the octant
 * the rotation maps everything into, which is why 32 entries serve 128 points.
 */
extern short DECv32_ANA_QMAP[8];
extern short DECv32_COS_ROT_ANGLE[4];
extern short DECv32_SIN_ROT_ANGLE[4];
extern short DECv32_MAG9600T[32];
extern short DECv32_ANGL9600T[32];

extern const short DECv32_IMAP64[64];
extern const short DECv32_QMAP64[64];
extern const short DECv32_MAG12000[64];
extern const short DECv32_ANGL12000[64];
extern const short DECv32_ANA_IMAP128[32];
extern const short DECv32_ANA_QMAP128[32];
extern const short DECv32_MAG14400[128];
extern const short DECv32_ANGL14400[128];

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

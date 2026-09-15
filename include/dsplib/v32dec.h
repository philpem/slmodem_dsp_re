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
 * indexes `DECv32_MAG9600` thousands of entries past the end (finding F1603,
 * D302) and only ONE of its three reachable indices reads a byte the blob
 * carries with it into a link, so that store is fixed behind
 * `DSPLIB_REPRODUCE_BUGS` and only that ring's `*mag` is compared.
 * Everything else about the function is ordinary and is compared on every
 * trial -- findings F3800 and F3801.  The four trellis ones -- `_16Tpt`,
 * `_32pt`, `_64pt` and `_128pt` -- were blocked on `VTB_decoder` (finding
 * F1602) until finding F3210's Viterbi batch landed it.
 */

#ifndef DSPLIB_V32DEC_H
#define DSPLIB_V32DEC_H

#include "dsplib/fpm_fse.h"
#include "dsplib/vtb.h"

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
 * so whatever the decoder keeps there is at most 56 bytes.  Finding F3210
 * measured `sizeof(struct vtb)` at exactly 0x38, so the bound is now met
 * exactly.  The period-i386 ABI is the one the object fixes, so the embedded
 * field is the existing shared decoder state rather than an untyped byte
 * reservation.
 */
struct v32_dec {
	unsigned short chan;		/* +0x00 selects prev_sym[]          */
	short pad02;			/* +0x02                             */
	short short_04;		/* +0x04                             */
	short short_06;		/* +0x06                             */
	short prev_sym[8];		/* +0x08 last absolute quadrant pair */
	struct vtb vtb;		/* +0x18 VTB_decoder's state         */
	short ang_prev;			/* +0x50 previous angle (AB only)    */
	union {
		struct {
			short sym_i;		/* +0x52 the symbol just decided */
			short sym_q;		/* +0x54                         */
			short sym_i1;		/* +0x56 one symbol back         */
			short sym_q1;		/* +0x58                         */
			short sym_i2;		/* +0x5a two back (AB only)      */
			short sym_q2;		/* +0x5c                         */
		};
		short sym[6];
	};
	short eqm;			/* +0x5e leaky decision-error measure*/
	short eqm_b;			/* +0x60 AB's second accumulator     */
	short retrain;			/* +0x62 1 or 2; a request, not a
					 *       state -- nothing here reads
					 *       it back                     */
	int rate_change;		/* +0x64 set by trn and by AB        */
	int int_68;			/* +0x68                             */
	short scram_tap;		/* +0x6c shift for the TRN generator */
	unsigned short count;		/* +0x6e symbols in the current phase*/
	unsigned int scram;		/* +0x70 TRN generator's register    */
	unsigned short short_74;	/* +0x74                             */
	short pad76;			/* +0x76                             */
};

#if !defined(__SIZEOF_POINTER__) || __SIZEOF_POINTER__ == 4
typedef char v32_dec_vtb_size[(sizeof(struct vtb) == 0x38) ? 1 : -1];
typedef char v32_dec_vtb_offset[
	(__builtin_offsetof(struct v32_dec, vtb) == 0x18) ? 1 : -1];
typedef char v32_dec_post_vtb_offset[
	(__builtin_offsetof(struct v32_dec, ang_prev) == 0x50) ? 1 : -1];
typedef char v32_dec_short74_offset[
	(__builtin_offsetof(struct v32_dec, short_74) == 0x74) ? 1 : -1];
typedef char v32_dec_size[(sizeof(struct v32_dec) == 0x78) ? 1 : -1];
#endif

/**
 * @brief V.32 slicer for the 4-point (1200 baud training) constellation.
 * @param state  The equaliser state; its decision is at `state->out_i[n_out]`/`out_q[n_out]`.
 * @param angle  In: measured angle; out: the decided point's ideal angle (the carrier PLL's error).
 * @param mag    Output: the decided point's ideal magnitude.
 * @return The decoded bits.
 */
unsigned short FSE_decision_4pt(struct fpm_fse *state, short *angle,
				short *mag);

/** @brief V.32 slicer for the TRN training sequence's constellation. @param state The equaliser state. @param angle In/out: measured then ideal angle. @param mag Output: ideal magnitude. @return The decoded bits. */
unsigned short FSE_decision_trn(struct fpm_fse *state, short *angle,
				short *mag);

/** @brief V.32 slicer for constellations C and D. @param state The equaliser state. @param angle In/out: measured then ideal angle. @param mag Output: ideal magnitude. @return The decoded bits. */
unsigned short FSE_decision_CD(struct fpm_fse *state, short *angle,
			       short *mag);

/** @brief V.32 slicer for constellations A and B. @param state The equaliser state. @param angle In/out: measured then ideal angle. @param mag Output: ideal magnitude. @return The decoded bits. */
unsigned short FSE_decision_AB(struct fpm_fse *state, short *angle,
			       short *mag);

/**
 * @brief V.32 slicer for 9600 bit/s with no trellis coding.
 *
 * A sixteen-point decision reported as four differentially encoded bits.
 * Installs no successor.
 *
 * @param state  The equaliser state.
 * @param angle  In/out: measured then ideal angle.
 * @param mag    Output: ideal magnitude.
 * @return The decoded bits.
 */
unsigned short FSE_decision_16pt(struct fpm_fse *state, short *angle,
				 short *mag);

/*
 * The four trellis slicers below each decide a point, write its ideal angle
 * and magnitude, and then hand the UNROTATED received symbol to
 * `VTB_decoder`, whose output through a stack local is the return value.
 * None of them installs a successor: the datapump leaves a trellis rate by
 * another route.
 */

/** @brief V.32 trellis slicer, 16 points (9600 bit/s trellis-coded). @param state The equaliser state. @param angle In/out: measured then ideal angle. @param mag Output: ideal magnitude. @return VTB_decoder()'s decoded bits. */
unsigned short FSE_decision_16Tpt(struct fpm_fse *state, short *angle,
				  short *mag);

/** @brief V.32bis trellis slicer, 32 points (12000 bit/s). @param state The equaliser state. @param angle In/out: measured then ideal angle. @param mag Output: ideal magnitude. @return VTB_decoder()'s decoded bits. */
unsigned short FSE_decision_32pt(struct fpm_fse *state, short *angle,
				 short *mag);

/** @brief V.32bis trellis slicer, 64 points (14400 bit/s). @param state The equaliser state. @param angle In/out: measured then ideal angle. @param mag Output: ideal magnitude. @return VTB_decoder()'s decoded bits. */
unsigned short FSE_decision_64pt(struct fpm_fse *state, short *angle,
				 short *mag);

/** @brief V.32bis trellis slicer, 128 points. @param state The equaliser state. @param angle In/out: measured then ideal angle. @param mag Output: ideal magnitude. @return VTB_decoder()'s decoded bits. */
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

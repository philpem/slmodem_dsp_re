/*
 * v17dec.h -- the V.17 fax receiver's slicer tables and its four
 *             constellation decision functions.
 *
 * WHAT THIS IS.  `V17RX_create` installs a slicer into the fractionally
 * spaced equaliser as `fse.decision`, and which one it installs depends on
 * the negotiated bit rate.  The four rate-specific slicers and the two
 * handshake ones live in `src/fax/v17dec.c`; the constellation, magnitude and
 * angle tables they index live in `src/fax/v17dec_tables.c`.
 *
 * WHY THE TABLES ARE `short`, TWICE OVER.
 *
 *   1. THE LOAD IS SIXTEEN BITS AND THE INDEX SCALE IS ONE.  Every reference
 *      in the object is `movzwl 0x0(%reg,%reg,1)` or `movswl 0x0(%reg,%reg,1)`
 *      -- the index register appears twice with scale 1, which is `2*i`, and
 *      the load is a word.  That is forced encoding in the sense of finding
 *      F613: the compiler had no freedom about the width.  Both extensions
 *      appear on the SAME table (`DECv17_IMAP16` is read `movzwl` at 0x98470
 *      and `movswl` at 0x984ba), which is finding F7803 -- the extension
 *      follows the declared type of the LOCAL, not of the array -- and not a
 *      disagreement about the element type.
 *
 *   2. NINETEEN OF THE TWENTY-TWO ARE BYTE-IDENTICAL TO V.32bis' OWN, which
 *      this tree already reconstructed and tests, as `short`, in
 *      `src/pump/v32/v32dec_tables.c`.  That is a second, independent
 *      extraction of the same numbers agreeing with the first.
 *
 * AND THE THREE THAT ARE NOT IDENTICAL ARE THE POINT OF SAYING SO.  A
 * cross-check that came out 22 of 22 would be reporting that V.17 and V.32bis
 * share a translation unit, which they do not: the two sets are separate
 * symbols in separate sections at separate addresses.  The three that differ
 * are where V.17's own Recommendation departs from V.32bis':
 *
 *   DECv17_MAP_TRN     { 3, 0, 2, 1 }  vs V.32's { 1, 2, 0, 3 }
 *   DECv17_ANGL4800    { 9870, 18062, 26254, 1678 }
 *                      vs V.32's DECv32_ANGL1200 { 9869, 18061, 26253, 1678 }
 *   DECv17_MAP_BRIDGE  { 1, 0, 2, 3 }  -- V.32bis has no such table
 *
 * `DECv17_ANGL4800`'s first three entries are each exactly ONE more than
 * V.32's and the fourth is equal.  That is recorded as an observation, not
 * explained: nothing in the object says why, and a rounding story that fits
 * three of four values is not evidence.  The values are copied from the
 * object's bytes, which is what a byte-exact reconstruction requires whatever
 * the reason.
 *
 * NAMING.  V.17 names these tables by its own BIT RATES where V.32bis names
 * them by its: V.17's 7200 bit/s uses the sixteen-point constellation
 * V.32bis calls 9600, and V.17's 4800 bit/s handshake constellation is
 * V.32's 1200.  The 9600T, 12000 and 14400 names coincide between the two.
 * This is the object's own spelling in both cases, not a convention chosen
 * here.
 */

#ifndef DSPLIB_V17DEC_H
#define DSPLIB_V17DEC_H

#include "dsplib/fpm_fse.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * THE TABLES.  Element counts are `st_size / 2` and every one of them is
 * corroborated by the consumer, either by a loop bound the slicer compares
 * against or by the constellation size the rate implies.  See
 * `src/fax/v17dec_tables.c` for the per-table derivation.
 */

/* The four-point handshake constellation, and the two symbol remappings. */
extern const short DECv17_MAP_BRIDGE[4];
extern const short DECv17_MAP_TRN[4];
extern const short DECv17_ANGL4800[4];
extern const short DECv17_QMAP4[4];
extern const short DECv17_IMAP4[4];

/* 7200 bit/s: sixteen points, three L2 magnitudes. */
extern const short DECv17_MAG7200[3];
extern const short DECv17_ANGL7200[16];
extern const short DECv17_QMAP16[16];
extern const short DECv17_IMAP16[16];

/* 9600 bit/s trellis: eight points by four rotations. */
extern const short DECv17_ANGL9600T[32];
extern const short DECv17_MAG9600T[32];

/* The 45-degree rotation applied before the 32- and 128-point slicers. */
extern const short DECv17_SIN_ROT_ANGLE[4];
extern const short DECv17_COS_ROT_ANGLE[4];

/* The analytic quadrant maps the 32- and 128-point slicers rail against. */
extern const short DECv17_ANA_QMAP[8];
extern const short DECv17_ANA_QMAP128[32];
extern const short DECv17_ANA_IMAP128[32];

/* 12000 bit/s trellis: sixty-four points. */
extern const short DECv17_QMAP64[64];
extern const short DECv17_IMAP64[64];
extern const short DECv17_ANGL12000[64];
extern const short DECv17_MAG12000[64];

/* 14400 bit/s trellis: one hundred and twenty-eight points. */
extern const short DECv17_ANGL14400[128];
extern const short DECv17_MAG14400[128];

/* ------------------------------------------------------------------------ */
/* THE PART OF THE V.17 RECEIVER THE SLICERS SEE                            */
/* ------------------------------------------------------------------------ */

/*
 * This is `fpm_fse_cfg::owner`, and WHERE it is comes out of `V17RX_create`
 * rather than out of a guess.  At .text 0x974c7 that function computes
 * `rx_state + 0x170` for `FPM_FSE_init`'s first argument -- which is
 * `V17RXS_FSE`, `v17fax.h`'s own offset for the equaliser -- and at 0x974a1
 * and 0x974ff it computes `rx_state + 0x2c` and stores it as `cfg.owner` and
 * then writes +0x5a, +0x60, +0x64, +0x66 and +0x68 through it.  So `owner` is
 * the receiver state's own bytes 0x2c..0x98, and the region ENDS exactly where
 * `V17RXS_MRF` (0x98) begins.
 *
 * `V17RX_create` IS NOT RECONSTRUCTED, so this is not that object's type and
 * must become it when it is -- the same ruling `v32dec.h` makes about
 * `struct v32_dec`, for the same reason.  Only the fields the seven functions
 * in `src/fax/v17dec.c` touch are named; the rest is padding sized so the
 * named ones land where the disassembly puts them.
 *
 * THE TWO OFFSETS `v17fax.h` ALREADY NAMES LINE UP AND DO NOT CONTRADICT
 * THIS.  `V17RXS_SGD` is 0x2c, which is `ptr_0000` here; `V17RXS_PTR_0030` is
 * 0x30, which is `struct vtb`'s first member `paths` -- a pointer the state
 * owns and which `V17RX_delete` hands to `sysdep_free`, exactly as that header
 * says.  A `struct vtb` is 0x38 bytes (finding F3210), so it runs 0x30..0x67
 * and `ang_prev` at 0x68 is the next thing the object writes.  The fit is
 * exact at both ends and that is the corroboration for the whole layout.
 *
 * `vtb` is a byte array and the four rate slicers cast it, for the reason
 * `v32dec.h` gives: `struct vtb` holds four pointers, so naming it as the
 * struct would make this type a different size in the 64-bit build and every
 * offset after +0x04 in this comment false.
 */
struct v17_dec {
	unsigned char ptr_0000[4];	/* +0x00 rx state + 0x2c; V17RXS_SGD */
	unsigned char vtb[0x38];	/* +0x04 VTB_decoder's state         */
	short ang_prev;			/* +0x3c previous angle (AB only)    */
	short sym_i;			/* +0x3e the symbol just decided     */
	short sym_q;			/* +0x40                             */
	short sym_i1;			/* +0x42 one symbol back             */
	short sym_q1;			/* +0x44                             */
	short sym_i2;			/* +0x46 two back (AB only)          */
	short sym_q2;			/* +0x48                             */
	/*
	 * AB's two leaky magnitude accumulators, one per phase of the
	 * alternation.  Which one a symbol feeds is decided by the SIGN OF THE
	 * PHASE STEP and nothing else, which is what makes them "A" and "B".
	 */
	short eqm_a;			/* +0x4a                             */
	short eqm_b;			/* +0x4c                             */
	unsigned char pad4e[2];		/* +0x4e                             */
	/*
	 * MODELLED, UNNAMED.  `FAX_FSE_decision_AB` sets it to 1 as it hands
	 * over to `FSE_decision_eqtrn` and `FSE_decision_eqtrn` clears it when
	 * the training segment ends, so it is set for exactly the span of the
	 * equaliser-training phase -- but nothing reconstructed READS it, so
	 * what it tells the reader of it is not established.
	 */
	int int_0050;			/* +0x50                             */
	unsigned char pad54[6];		/* +0x54                             */
	/*
	 * Symbols in the CURRENT handshake segment.  All three handshake
	 * functions increment it, compare it against that segment's length,
	 * and zero it at the handover -- 0xc8 in `FAX_FSE_decision_AB`, the
	 * training limit in `FSE_decision_eqtrn`, 0x3e in `FSE_Bridge_det`.
	 * Signed: every compare against it is `cmpw`/`jle` or `movswl`/`jl`.
	 */
	short count;			/* +0x5a                             */
	/*
	 * The TRN generator's shift register, and it is the same generator
	 * V.32bis' `FSE_decision_trn` runs (`src/pump/v32/v32fse.c`) with the
	 * tap fixed at 16 rather than carried in a field.  `FAX_FSE_decision_AB`
	 * seeds it with 0xbb3754 at the handover.
	 *
	 * SIGNED, and that is forced: the object reads it `sar $0x10` at
	 * 0x98902 for the tap and `shr $0x15` at 0x98908 for the fixed one, and
	 * only a signed operand makes the first of those an arithmetic shift.
	 */
	int scram;			/* +0x5c                             */
	/*
	 * Non-zero selects the SHORT training path, and that is read off the
	 * two branches it gates rather than inferred from a name:
	 * `FSE_decision_eqtrn` trains for 0x25 symbols when it is set and
	 * 0xb9f when it is clear, and on the short path it hands straight to
	 * the rate slicer where on the long one it hands to `FSE_Bridge_det`
	 * first.  It also gates `FAX_FSE_decision_AB`'s early `pll_on`.
	 * `V17RX_create` copies it in from the control block at +0x10.
	 */
	int short_train;		/* +0x60                             */
	/*
	 * Which of `FSEv17_decision`'s four the handshake hands over to.
	 * `V17RX_create` sets it from the control block at +0x0c; both
	 * `FSE_Bridge_det` and `FSE_decision_eqtrn` index the table with it and
	 * with nothing else.
	 */
	short rate;			/* +0x64                             */
	short short_0066;		/* +0x66 set to 3 by V17RX_create    */
	/*
	 * Symbols since the receiver started, and NOT per segment: every one
	 * of the six slicers increments it and none of them ever zeroes it.
	 *
	 * IT DOES NOT WRAP TO ZERO.  Five of the six replace 0x8000 with
	 * 0x4000 on the way past, so after the first 32768 symbols it cycles
	 * through the top half of the range only.  `FAX_FSE_decision_AB` is
	 * the exception and increments without the test -- which cannot matter,
	 * because AB is the first segment and 0x8000 symbols of it is longer
	 * than its own 0xc8-symbol exit.
	 */
	unsigned short sym_count;	/* +0x68                             */
	unsigned char pad6a[2];		/* +0x6a                             */
};

/* ------------------------------------------------------------------------ */
/* THE SLICERS                                                              */
/* ------------------------------------------------------------------------ */

/*
 * The four rate slicers.  Each decides a constellation point, writes that
 * point's ideal angle and magnitude back over the equaliser's measured ones,
 * and hands the UNROTATED received symbol to `VTB_decoder`, whose output
 * through a stack local is the return value.  None of them installs a
 * successor.
 */
unsigned short FAX_FSE_decision_16pt(struct fpm_fse *state, short *angle,
				     short *mag);
unsigned short FAX_FSE_decision_32pt(struct fpm_fse *state, short *angle,
				     short *mag);
unsigned short FAX_FSE_decision_64pt(struct fpm_fse *state, short *angle,
				     short *mag);
unsigned short FAX_FSE_decision_128pt(struct fpm_fse *state, short *angle,
				      short *mag);

/*
 * The three handshake slicers, and they are a state machine expressed as
 * function pointers:
 *
 *     AB      --(the phase alternation stops)-->   eqtrn
 *     eqtrn   --(short training, 0x25 symbols)-->  FSEv17_decision[rate]
 *     eqtrn   --(long training, 0xb9f symbols)-->  Bridge_det
 *     Bridge  --(0x3e symbols)-->                  FSEv17_decision[rate]
 *
 * `FSE_Bridge_det` is the odd one: it is not installed by `V17RX_create` and
 * is reached only through `FSE_decision_eqtrn`'s long-training arm.
 */
unsigned short FAX_FSE_decision_AB(struct fpm_fse *state, short *angle,
				   short *mag);
unsigned short FSE_decision_eqtrn(struct fpm_fse *state, short *angle,
				  short *mag);
unsigned short FSE_Bridge_det(struct fpm_fse *state, short *angle,
			      short *mag);

/*
 * `.rodata` 0x9864, 16 bytes, and it is FOUR FUNCTION POINTERS rather than
 * the `short[8]` a value dump of those bytes would suggest.  The object
 * carries an `R_386_32` against `FAX_FSE_decision_16pt`, `_32pt`, `_64pt` and
 * `_128pt` at +0x0, +0x4, +0x8 and +0xc; read as data it would give eight
 * plausible small integers, which is the trap `tools/dis.py` exists for.
 *
 * The order is the rate order: 7200, 9600, 12000, 14400 bit/s.
 */
extern const fpm_fse_decision FSEv17_decision[4];

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V17DEC_H */

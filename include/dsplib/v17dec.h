/**
 * @file v17dec.h
 * @brief The V.17 fax receiver's slicer tables and its four constellation
 *        decision functions.
 *
 * Six slicers for the fractionally spaced equaliser's `fse.decision` slot --
 * four rate-specific and two for the handshake -- in `src/fax/v17dec.c`, and
 * the constellation, magnitude and angle tables they index, in
 * `src/fax/v17dec_tables.c`.
 *
 * `V17RX_create` installs `FAX_FSE_decision_AB` unconditionally and does not
 * choose by bit rate: its one store into `fpm_fse_cfg::decision` is outside
 * every rate arm.  The rate-specific four are reached only later, once the
 * handshake hands over through `FSEv17_decision[rate]` -- what
 * `V17RX_create` does switch on the rate is the Viterbi decoder's
 * constellation (`struct vtb` at state +0x30) and one quality threshold
 * (finding F9473, correcting an earlier reading written before
 * `V17RX_create` could be read).
 *
 * The tables are `short`, on two independent grounds: every reference in the
 * object is a sixteen-bit load at index scale 1, which is forced encoding
 * (both extensions appear on the same table, `DECv17_IMAP16` -- finding
 * F7803, the extension follows the declared type of the LOCAL rather than
 * the array); and nineteen of the twenty-two tables are byte-identical to
 * V.32bis' own, already reconstructed and tested as `short` in
 * `src/pump/v32/v32dec_tables.c` (finding F9170).
 *
 * The three that are NOT identical are where V.17's own Recommendation
 * departs from V.32bis': `DECv17_MAP_TRN`, `DECv17_ANGL4800` (whose first
 * three entries are each exactly one more than V.32's `DECv32_ANGL1200`,
 * recorded as an observation and not explained) and `DECv17_MAP_BRIDGE`,
 * which V.32bis has no equivalent of.
 *
 * V.17 names these tables by its own bit rates where V.32bis names them by
 * its -- V.17's 7200 bit/s uses the sixteen-point constellation V.32bis
 * calls 9600, and V.17's 4800 bit/s handshake constellation is V.32's 1200 --
 * which is the object's own spelling in both cases, not a convention chosen
 * here.
 */

#ifndef DSPLIB_V17DEC_H
#define DSPLIB_V17DEC_H

#include "dsplib/fpm_fse.h"
#include "dsplib/vtb.h"

struct sgd;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The tables.  Element counts are `st_size / 2` and every one of them is
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
/* The part of the V.17 receiver the slicers see                            */
/* ------------------------------------------------------------------------ */

/*
 * This is `fpm_fse_cfg::owner`: the receiver state's own bytes 0x2c..0x98
 * (ending exactly where `V17RXS_MRF`, 0x98, begins), established from
 * `V17RX_create`'s own computation of `cfg.owner` at `rx_state + 0x2c` and
 * its writes through it at +0x5a, +0x60, +0x64, +0x66 and +0x68.
 *
 * `V17RX_create` is not reconstructed, so this is not that object's type and
 * must become it when it is -- the same ruling `v32dec.h` makes about
 * `struct v32_dec`, for the same reason.  Only the fields the seven
 * functions in `src/fax/v17dec.c` touch are named; the rest is padding sized
 * so the named ones land where the disassembly puts them.
 *
 * The two offsets `v17fax.h` already names line up and do not contradict
 * this: `V17RXS_SGD` is 0x2c, which is `sgd` here, and
 * `V17RXS_PTR_0030` is 0x30, `struct vtb`'s first member `paths`.  A
 * `struct vtb` is 0x38 bytes (finding F3210), so it runs 0x30..0x67 and
 * `ang_prev` at 0x68 is the next thing the object writes -- an exact fit at
 * both ends, which corroborates the whole layout.
 *
 * `vtb` is the existing decoder type in `vtb.h`; on the period i386 ABI its
 * measured 0x38-byte extent preserves the receiver-state layout exactly.
 */
struct v17_dec {
	struct sgd *sgd;			/* +0x00 rx state + 0x2c; V17RXS_SGD */
	struct vtb vtb;			/* +0x04 VTB_decoder's state, F3210  */
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
	 * Modelled, unnamed.  `FAX_FSE_decision_AB` sets it to 1 as it hands
	 * over to `FSE_decision_eqtrn` and `FSE_decision_eqtrn` clears it when
	 * the training segment ends, so it is set for exactly the span of the
	 * equaliser-training phase -- but nothing reconstructed reads it, so
	 * what it tells the reader of it is not established.
	 */
	int int_0050;			/* +0x50                             */
	unsigned char pad54[6];		/* +0x54                             */
	/*
	 * Symbols in the current handshake segment.  All three handshake
	 * functions increment it, compare it against that segment's length,
	 * and zero it at the handover -- 0xc8 in `FAX_FSE_decision_AB`, the
	 * training limit in `FSE_decision_eqtrn`, 0x3e in `FSE_Bridge_det`.
	 * Signed: every compare against it is a signed word compare.
	 */
	short count;			/* +0x5a                             */
	/*
	 * The TRN generator's shift register, and it is the same generator
	 * V.32bis' `FSE_decision_trn` runs (`src/pump/v32/v32fse.c`) with the
	 * tap fixed at 16 rather than carried in a field.  `FAX_FSE_decision_AB`
	 * seeds it with 0xbb3754 at the handover.
	 *
	 * Signed, and that is forced: the object reads it with one arithmetic
	 * and one logical shift, and only a signed operand makes the first of
	 * those an arithmetic shift.
	 */
	int scram;			/* +0x5c                             */
	/*
	 * Non-zero selects the short-training path, read off the two branches
	 * it gates rather than inferred from a name: `FSE_decision_eqtrn`
	 * trains for 0x25 symbols when it is set and 0xb9f when it is clear,
	 * and on the short path it hands straight to the rate slicer where on
	 * the long one it hands to `FSE_Bridge_det` first.  It also gates
	 * `FAX_FSE_decision_AB`'s early `pll_on`.  `V17RX_create` copies it in
	 * from the control block at +0x10.
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
	 * Symbols since the receiver started, and not per segment: every one
	 * of the six slicers increments it and none of them ever zeroes it.
	 *
	 * It does not wrap to zero.  Five of the six replace 0x8000 with
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
/* The slicers                                                              */
/* ------------------------------------------------------------------------ */

/**
 * @brief 7200 bit/s slicer: sixteen-point trellis constellation, searched
 *        exhaustively (no region tree).
 *
 * The only one of the four rate slicers with no region tree; it is in fact
 * V.32bis' `_16Tpt`, not its `_16pt` (which this function's own name would
 * suggest) -- V.32's `_16pt` has deviation D302's out-of-bounds table read,
 * and this one shifts by 13 rather than 1 and does not.
 *
 * @param state  The equaliser state; `state->cfg.owner` is a `struct
 *               v17_dec`.
 * @param angle  Output: the decided point's ideal angle, replacing the
 *               equaliser's measured one (the carrier PLL's error term).
 * @param mag    Output: the decided point's ideal magnitude.
 * @return The decoded symbol, from `VTB_decoder` on the unrotated point.
 */
unsigned short FAX_FSE_decision_16pt(struct fpm_fse *state, short *angle,
				     short *mag);
/**
 * @brief 9600 bit/s trellis slicer: thirty-two points, folded by a
 *        45-degree rotation into a one-dimensional search over three
 *        candidates (two at the ends of the fold, resolved by a real
 *        squared distance).
 *
 * V.32bis' `_32pt` unchanged, thresholds included.
 *
 * @param state  The equaliser state; `state->cfg.owner` is a `struct
 *               v17_dec`.
 * @param angle  Output: the decided point's ideal angle.
 * @param mag    Output: the decided point's ideal magnitude.
 * @return The decoded symbol, from `VTB_decoder` on the unrotated point.
 */
unsigned short FAX_FSE_decision_32pt(struct fpm_fse *state, short *angle,
				     short *mag);
/**
 * @brief 12000 bit/s trellis slicer: sixty-four points on a plain
 *        four-by-four grid, no rotation.
 *
 * V.32bis' region tree unchanged; the two axes deliberately split at
 * different points on their outer bands (asymmetric by one count, not a
 * misreading -- see `src/fax/v17dec.c`).
 *
 * @param state  The equaliser state; `state->cfg.owner` is a `struct
 *               v17_dec`.
 * @param angle  Output: the decided point's ideal angle.
 * @param mag    Output: the decided point's ideal magnitude.
 * @return The decoded symbol, from `VTB_decoder` on the unrotated point.
 */
unsigned short FAX_FSE_decision_64pt(struct fpm_fse *state, short *angle,
				     short *mag);
/**
 * @brief 14400 bit/s trellis slicer: one hundred and twenty-eight points,
 *        folded by the 45-degree rotation and cut by a ten-leaf region
 *        tree, with two leaves resolved by a squared distance between one
 *        specific pair of points rather than by searching.
 *
 * Four of the region tree's cuts are one value wider than V.32bis' own
 * (measured, not carried across); reproduces deviations D1200 (one literal
 * coordinate one off its table entry) and D370 (a tie-break favouring the
 * farther point); initialises the index `best`, which the object leaves
 * uninitialised on a path the region tree makes unreachable (D1201).
 *
 * @param state  The equaliser state; `state->cfg.owner` is a `struct
 *               v17_dec`.
 * @param angle  Output: the decided point's ideal angle.
 * @param mag    Output: the decided point's ideal magnitude.
 * @return The decoded symbol, from `VTB_decoder` on the unrotated point.
 */
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

/**
 * @brief The AB handshake segment, where the receiver starts.
 *
 * Two alternating phases 180 degrees apart; decides by the sign of the
 * phase step rather than by distance, and each phase keeps its own leaky
 * smoothed magnitude.  Exits on a signal-quality test (the last three
 * symbols moved apart by more than 4/3 of the two magnitudes' mean square,
 * after at least 0xc8 symbols), at which point it seeds the TRN generator
 * and installs `FSE_decision_eqtrn` as the equaliser's next decision
 * function.  Increments the symbol counter without the wrap test its five
 * siblings apply (deviation D1202, harmless here since AB always exits
 * well under 0x8000 symbols).
 *
 * @param state  The equaliser state; `state->cfg.owner` is a `struct
 *               v17_dec`.
 * @param angle  Output: one of two fixed handshake angles.
 * @param mag    Output: the fixed handshake magnitude.
 * @return 3 for an A symbol, 2 for a B symbol, 0 on the segment's last call
 *         (the handover to `FSE_decision_eqtrn`).
 */
unsigned short FAX_FSE_decision_AB(struct fpm_fse *state, short *angle,
				   short *mag);
/**
 * @brief The equaliser-training (TRN) handshake segment.
 *
 * Does not look at the received symbol at all: regenerates V.32bis' own TRN
 * sequence from its own shift register (tap fixed at 16) and reports that
 * as the decision, so the equaliser adapts against a known reference.
 * Trains for 0x25 symbols on the short path or 0xb9f on the long one (see
 * `short_train`), then hands over to `FSEv17_decision[rate]` directly on
 * the short path or to `FSE_Bridge_det` on the long one.
 *
 * @param state  The equaliser state; `state->cfg.owner` is a `struct
 *               v17_dec`.
 * @param angle  Output: one of the four handshake angles, from the
 *               regenerated sequence.
 * @param mag    Output: the fixed handshake magnitude.
 * @return The regenerated TRN symbol (0..3), not the received one.
 */
unsigned short FSE_decision_eqtrn(struct fpm_fse *state, short *angle,
				  short *mag);
/**
 * @brief The long-training bridge segment.
 *
 * Decides the four-point handshake constellation with a deliberately
 * lopsided metric (deviation D301's shape) for 0x3e symbols, then hands
 * over to `FSEv17_decision[rate]` -- tested at the top of the call, so the
 * handover fires one symbol late (the object's own ordering, not a slip).
 * Reached only through `FSE_decision_eqtrn`'s long-training arm; not
 * installed directly by `V17RX_create`.
 *
 * @param state  The equaliser state; `state->cfg.owner` is a `struct
 *               v17_dec`.
 * @param angle  Output: one of the four handshake angles.
 * @param mag    Output: the fixed handshake magnitude.
 * @return Always 0: the decided point reaches the caller only through
 *         `*angle`, never as data.
 */
unsigned short FSE_Bridge_det(struct fpm_fse *state, short *angle,
			      short *mag);

/*
 * `.rodata` 0x9864, 16 bytes, and it is four function pointers rather than
 * the `short[8]` a value dump of those bytes would suggest -- the object
 * carries a relocation against each of `FAX_FSE_decision_16pt`, `_32pt`,
 * `_64pt` and `_128pt`, which is the trap `tools/dis.py` exists for.
 *
 * The order is the rate order: 7200, 9600, 12000, 14400 bit/s.
 */
extern const fpm_fse_decision FSEv17_decision[4];

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V17DEC_H */

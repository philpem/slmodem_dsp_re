/*
 * v22hdx.h -- V.22 / V.22bis: two of the seven V22_PROTOCOL state handlers.
 *
 * Reconstructed from dsplibs.o:
 *
 *   v22_retrain      .text 0x0892c0  2,284 bytes
 *   v22_org_rmloop2  .text 0x089bb0  1,050 bytes
 *
 * WHAT A HANDLER IS.  `.rodata` holds a table of seven function pointers at
 * 0x8544 -- `v22_data`, `v22_originate`, `v22_answer`, `v22_local_loop`,
 * `v22_org_rmloop2`, `v22_ans_rmloop2`, `v22_retrain` -- and these two are the
 * last and the fifth of it.  Each takes the datapump and six buffer arguments,
 * runs ONE block of transmit and ONE block of receive, and leaves the modem in
 * whatever state that block moved it to.  Nothing loops here: the caller calls
 * again for the next block.
 *
 * THE SEVEN ARGUMENTS ARE THE SAME SEVEN, IN THE SAME ORDER, IN BOTH.  That is
 * the strongest single check on this reading -- two functions 2,284 and 1,050
 * bytes apart agree on all seven stack slots -- and it is what makes the
 * argument list a property of the TABLE rather than of either function:
 *
 *     fp        the datapump                       (struct v22fp *)
 *     txdata    symbols the modulator will send    MakeTxData writes it
 *     txout     samples the modulator produced     ModDataV22 writes it
 *     rxin      samples from the line              DemodDataV22 reads it
 *     rxsym     symbols the demodulator recovered  DemodDataV22 writes it
 *     txcount   symbols in, then samples out       both directions
 *     rxcount   samples in, then symbols out       both directions
 *
 * THE TWO COUNTS CHANGE UNITS ACROSS THE CALL, and that is not a slip in this
 * comment.  `*txcount` is a SYMBOL count when `MakeTxData` reads it and a
 * SAMPLE count after `ModDataV22` has stored its return into it; `*rxcount` is
 * a SAMPLE count when `DemodDataV22` reads it and a SYMBOL count after.  The
 * caller therefore has to reset both between blocks, and `RxClampV22` -- which
 * every path here ends with -- overwrites `rxsym` with V22_CLAMP_BLOCK copies
 * of V22_CLAMP_VALUE and sets `*rxcount` to V22_CLAMP_BLOCK on the way out.
 *
 * THE POINTER TYPES ARE THE CALLEES', NOT THIS HEADER'S.  `MakeTxData` takes
 * `short *` for the same buffer `ScrambleDataV22` and `ModDataV22` take
 * `unsigned short *` for, and `RxClampV22` takes `short *` for the buffer
 * `DemodDataV22` and `DescrambleDataV22` take `unsigned short *` for.  Every
 * one of those readings is established in its own header from its own
 * function's instructions, and this object cannot arbitrate between them --
 * the extension is dead at all four sites in finding F614's sense.  The
 * signature below picks the majority spelling per buffer and the source casts
 * at the minority call sites; the casts carry no claim.
 *
 * NEITHER FUNCTION PRINTS ANYTHING.  Between them they carry 97 relocations
 * and exactly one is not a call: `v22_retrain`'s eight-entry jump table.  So
 * there is no format string to read a field's meaning off, and every name
 * below that is not forced by a callee's signature stays at its offset.
 */

#ifndef DSPLIB_V22HDX_H
#define DSPLIB_V22HDX_H

#include "dsplib/v22fp.h"

/*
 * The rate `struct v22fp_params::bps2` is compared against to pick between
 * V22_TXDATA_ONES_1200 and V22_TXDATA_ONES_2400.  Note it is +0x04 and not
 * +0x02: v22fp.h records that `V22FP_create` copies +0x02 to +0x04
 * unconditionally, so the two are always equal and this is which one the
 * object's `cmpw` names.
 */
#define V22_HDX_BPS_1200	1200

/*
 * The tick.  `ReadGTimer` advances the shared timer by 20 ms per block, and 20
 * is also what both functions add to `hdx->r08` for a block that saw what it
 * was looking for.  So `r08` is a duration in milliseconds and the thresholds
 * below are milliseconds too.
 */
#define V22_HDX_TICK_MS		20

/*
 * `v22_org_rmloop2`'s two thresholds.
 *
 * The first is on `hdx->r08`, which accumulates what `Detect_Rmloop2_ACK` and
 * `Detect_1s` return -- v22det.h establishes those returns as milliseconds --
 * so 231 ms of the pattern advances the state.  THE COMPARISON IS UNSIGNED
 * (`cmpw`/`ja` on the 16-bit field), which is why the source casts.
 *
 * The second is on the shared timer and is the same 1,300 ms `v22_retrain`
 * uses for its own two long waits.
 */
#define V22_RMLOOP2_PATTERN_MS	231	/* strictly more than this */
#define V22_RMLOOP2_TIMEOUT_MS	1300	/* strictly more than this */

/*
 * `Detect_1s`'s threshold in `v22_org_rmloop2` substate 2, Q15: 27852/32768 =
 * 0.85.  `Detect_Retrain` and `Detect_Rmloop2_ACK` carry their own built-in
 * V22_DET_THRESH_Q15 (0.99); `Detect_1s` takes one from its caller and this is
 * the only caller reconstructed so far that supplies one.
 */
#define V22_RMLOOP2_ONES_Q15	0x6ccc

/*
 * `v22_retrain`'s five timeouts, one per substate that has one, in the
 * milliseconds `ReadGTimer` counts.  Each is a `ja` against the timer's value
 * AFTER the tick, so the wait ends on the first block whose timer exceeds it.
 */
#define V22_RETRAIN_T_S1	  99	/* substate 2, strictly more than this */
#define V22_RETRAIN_T_ONES1200	 449	/* substate 4                          */
#define V22_RETRAIN_T_DESCR1200	 599	/* substate 5                          */
#define V22_RETRAIN_T_ONES2400	 799	/* substate 6                          */
#define V22_RETRAIN_T_SILENCE	1300	/* substates 0 and 3                   */
#define V22_RETRAIN_T_TRAIN2400	1999	/* substate 7                          */

/*
 * A 16-bit field at `struct v22fp_hdx` + 0x38.  v22fp.h models +0x36..+0x3b as
 * `r36[6]` -- six bytes with no shape, because `V22FP_create` never writes any
 * of them -- and `v22_retrain` substate 2 is the first thing reconstructed
 * that reads inside that region.  It compares the halfword against 1 to choose
 * between substates 3 and 4 and does nothing else with it; nothing
 * reconstructed writes it.  So the shape is settled (a `short` at +0x38) and
 * the meaning is not, and it is reached through its offset rather than named.
 */
#define V22_HDX_R38(hdx)	(*(const short *)(const void *)&(hdx)->r36[2])

/*
 * Retrain.  Eight substates in `hdx->connect_substate`, dispatched through a jump table, and
 * a value outside 0..7 -- including a negative one, because the range test is
 * unsigned -- does nothing at all beyond stamping `fp->status`.
 *
 * The substates run in order and each hands on to the next: 0 waits for the
 * far end to stop, 1 resets the receiver and drops both directions to 1200, 2
 * sends unscrambled S1 and branches on +0x38, 3 waits again, 4, 5 and 6 walk
 * the rate back up, and 7 waits for `RxTrained2400`.
 */
void v22_retrain(struct v22fp *fp, unsigned short *txdata, short *txout,
		 short *rxin, unsigned short *rxsym, unsigned short *txcount,
		 unsigned short *rxcount);

/*
 * The originating end of the remote-loopback-2 exchange.  Three substates in
 * the same `hdx->connect_substate`, and any other value returns after stamping `fp->status`
 * and setting bit 1 of `fp->r1e[0]`.
 */
void v22_org_rmloop2(struct v22fp *fp, unsigned short *txdata, short *txout,
		     short *rxin, unsigned short *rxsym,
		     unsigned short *txcount, unsigned short *rxcount);

#endif /* DSPLIB_V22HDX_H */

/*
 * v22ans.h -- V.22 / V.22bis: two of the V22_PROTOCOL state handlers.
 *
 * Reconstructed from dsplibs.o:
 *   v22_data          .text 0x088910    946 bytes
 *   v22_ans_rmloop2   .text 0x089fd0  1,160 bytes
 *
 * THE HANDLER SHAPE.  Seven arguments, and it is the same seven for both:
 *
 *     v22_XXX(fp, txsym, txout, rxin, rxsym, txcount, rxcount)
 *
 * The object settles every one of them by what it is handed to.  `txsym` goes
 * to `ScrambleDataV22`'s `unsigned short *data` and `ModDataV22`'s
 * `const unsigned short *data`; `txout` goes to `ModDataV22`'s `short *out`;
 * `rxin` and `rxsym` go to `DemodDataV22`'s `short *in` and
 * `unsigned short *sym`; `txcount` and `rxcount` are read through with
 * `movzwl` and written back with a 16-bit store, which is what makes them
 * `unsigned short *` rather than by-value counts.
 *
 * `MakeTxData` and `RxClampV22` are declared over `short *` where the rest of
 * the chain is `unsigned short *`, so the calls carry casts.  Nothing in the
 * object distinguishes the two spellings -- every load is `movzwl` and every
 * store is 16 bits -- so the parameter types here follow the MAJORITY of the
 * callees and the two casts are ours, not the author's.
 *
 * WHAT THE FORMAT STRINGS SETTLE, and it is more than usual.  `v22_data`
 * carries five of the author's own sentences, and two of them name fields:
 *
 *     "V22_MSG_NO_CARRIER won't be reported (carrier_loss_time %d of %d ms)"
 *     "V22_MSG_NO_CARRIER2"
 *     "V.22: Retrain request detected."
 *     "Signal quality < Retrain level. Retrain initiated."
 *     "Carrier back during carrier_loss_time (v22_data). Retrain initiated."
 *
 * The first prints `hdx->carrier_loss_blocks * 20` and `params.carrier_loss_ms`, in that order, as the two
 * halves of "carrier_loss_time %d of %d ms".  So `hdx->carrier_loss_blocks` counts 20 ms
 * blocks of lost carrier and `params.carrier_loss_ms` is the limit, in milliseconds, at
 * which the loss is reported -- which is exactly the 700 `v22_create` puts
 * there.  v22fp.h names neither, and this header does not rename them; the
 * derivation is recorded here and in the report, and the code below uses the
 * existing spellings.
 *
 * `params.r08` is compared against the same `hdx->carrier_loss_blocks * 20` product on the
 * OTHER arm -- the one `params.flags` bit 11 selects -- so it is a limit of
 * the same kind and the same unit, 60000 ms from `v22_create`.  Again not
 * renamed here.
 *
 * TWO OF THE OBJECT'S TYPES DISAGREE WITH THE HEADERS WE ALREADY HAVE, and
 * both are visible as a dead `cwtl`:
 *
 *   - `Detect_1s` and `Detect_Retrain` are declared `int` in v22det.h, and
 *     every one of the four call sites here narrows the return to sixteen bits
 *     before using it -- `cwtl` then a 16-bit `test`, or `cwtl` then a store
 *     into an `int` slot.  That is what a `short`-returning callee looks like.
 *     The casts below reproduce it exactly; v22det.h is left alone.
 *   - `ReadGTimer` is declared `int` and both comparisons against it are
 *     `jbe`, i.e. UNSIGNED.  Same treatment: the cast is here, the header is
 *     not touched.
 *
 * `hdx->r08` is `short` in v22fp.h and every read of it in these two functions
 * is `movzwl` feeding an unsigned compare, so the casts below make it read
 * unsigned at each site rather than changing the declaration.
 *
 * THE OFFSET THAT IS NOT A FIELD YET.  Both retrain paths in `v22_data` store
 * a 16-bit 1 -- and the retrain-request path a 16-bit 0 -- at `hdx + 0x38`,
 * which falls inside v22fp.h's `unsigned char r36[6]`, the region create never
 * writes.  Reached below as `*(short *)(hdx->r36 + 2)` rather than by widening
 * that array, because a modelling change to a shared header is not this
 * file's to make.
 */

#ifndef DSPLIB_V22ANS_H
#define DSPLIB_V22ANS_H

struct v22fp;

/*
 * `params.flags` bit 11.  v22fp.h records it as "cfg.f14 bit 0 -> flags bit
 * 11, which forces hdx.protocol to 0 whatever the mode selected"; `v22_data` is the
 * first reconstructed reader of it, and what it selects there is a receiver
 * that never runs -- no demodulation, no carrier test, just the timer.
 * v22status.h spells bits 9 and 10 the same way.
 */
#define V22_PARAMS_BIT11	(1u << 11)

/*
 * The status codes these two write into `fp->status` (+0x1c).
 *
 * THREE OF THEM ARE NAMED BY THE AUTHOR'S OWN MESSAGES, and the strongest of
 * the three is corroborated from outside V.22: v32demod.h records that the
 * same author's `RxHdxNull` writes 0x10 to the V.32 object's status byte
 * "beside the string V32_MSG_NO_CARRIER".  Same value, same role, other
 * datapump.
 *
 * The remaining four have no string and keep their value in their name --
 * CLAUDE.md's "modelled, unnamed" tier.  0x0a is written unconditionally at
 * `v22_ans_rmloop2`'s entry and overridden by every transition it can make,
 * which reads like the handler's own id, but nothing in the object forces
 * that and it is not claimed here.
 */
#define V22_ST_NO_CARRIER	0x10	/* "V22_MSG_NO_CARRIER2"            */
#define V22_ST_RETRAIN		0x0b	/* both "Retrain initiated." sites  */
#define V22_ST_RETRAIN_REQ	0x09	/* "Retrain request detected."      */
#define V22_ST_ANS_RMLOOP2	0x0a	/* written at entry, then overridden */
#define V22_ST_05		0x05
#define V22_ST_07		0x07
#define V22_ST_08		0x08

/*
 * `fp->flags` (+0x1d) and `fp->r1e[0]` (+0x1e).  Each function ORs a constant
 * into one of the two and neither reads either back, so the bits are named by
 * value only.  Note that they are DIFFERENT bytes: `v22_data` reaches +0x1d
 * and `v22_ans_rmloop2` reaches +0x1e.
 */
#define V22_DATA_FLAGS_SET	0x05	/* fp->flags   |= this, v22_data     */
#define V22_ANS_R1E_SET		0x02	/* fp->r1e[0]  |= this, ans_rmloop2  */

/*
 * `hdx->connect_substate` selects the sub-state inside `v22_ans_rmloop2`.  Four live
 * values and a silent default: anything outside 0..3 does nothing at all
 * except the two stores at the top.
 */
#define V22_RMLOOP2_START	0	/* arm the timer, then fall to 1     */
#define V22_RMLOOP2_DETECT	1	/* accumulate Detect_1s into r08     */
#define V22_RMLOOP2_ANSWER	2	/* transmit symbol 2 / symbol 10     */
#define V22_RMLOOP2_LOOP	3	/* echo the received symbols back    */

/*
 * The two milliseconds-since-entry limits `ReadGTimer` is tested against,
 * one per sub-state.  Both share a single tail in the object -- the arm for
 * sub-state 1 jumps into the middle of the arm for sub-state 2, four
 * instructions past its own comparison -- which is finding F8528's shape and
 * the reason the test below drives the two independently.
 */
#define V22_RMLOOP2_T1_MS	0x514	/* sub-state 1, 1300 ms              */
#define V22_RMLOOP2_T2_MS	0x1388	/* sub-state 2, 5000 ms              */

/* `hdx->r08` limits.  See the header comment on why each is read unsigned. */
#define V22_RMLOOP2_DETECT_MAX	0x9a	/* sub-state 1, a Detect_1s total    */
#define V22_RMLOOP2_LOSS_MAX	0x27	/* sub-state 3, 39 ms of no carrier  */
#define V22_DATA_QUALITY_MAX	0xbb8	/* v22_data, 3000 ms of poor signal  */

/* `hdx->r08` steps by one block, 20 ms, on both of the millisecond uses. */
#define V22_BLOCK_MS		20

/*
 * `GetSignalQuality` at or below this starts the retrain timer; above it, the
 * timer is cleared.  The message the expiry prints is "Signal quality <
 * Retrain level", which is what says the comparison is a quality floor; the
 * number itself is a literal and is named for its value.
 */
#define V22_DATA_RETRAIN_LEVEL	0x7f37

/*
 * `hdx->carrier_loss_blocks` above this, with the carrier back and the receiver at 2400,
 * retrains.  Fifteen blocks, 300 ms, and the message calls the elapsed
 * quantity `carrier_loss_time`.
 */
#define V22_DATA_CARRIER_BACK	14	/* strictly more than this */

/*
 * The half-duplex answer state's transmit pattern: V22_TXDATA_SYMBOL_2 at
 * 1200 and V22_TXDATA_SYMBOL_10 at 2400, chosen by `params.bps != 1200`.
 */
#define V22_RMLOOP2_BPS_1200	1200

/*
 * Data state.  One block in, one block out: scramble and modulate the
 * transmit symbols, demodulate and descramble the receive samples, and watch
 * three things that can end the connection -- a retrain request in the
 * received symbol stream, the signal quality falling below the retrain level
 * for three seconds, and the carrier going away for longer than
 * `params.carrier_loss_ms` milliseconds.
 */
void v22_data(struct v22fp *fp, unsigned short *txsym, short *txout,
	      short *rxin, unsigned short *rxsym, unsigned short *txcount,
	      unsigned short *rxcount);

/*
 * The answering station's remote-loopback state, a four-way machine on
 * `hdx->connect_substate`.  Sub-state 3 is the loop itself: the demodulated symbols are
 * scrambled and modulated straight back out, which is what the name says and
 * what the argument flow shows -- `rxsym` is handed to `ScrambleDataV22` and
 * then to `ModDataV22`, and `txsym` is not read at all on that arm.
 */
void v22_ans_rmloop2(struct v22fp *fp, unsigned short *txsym, short *txout,
		     short *rxin, unsigned short *rxsym,
		     unsigned short *txcount, unsigned short *rxcount);

#endif /* DSPLIB_V22ANS_H */

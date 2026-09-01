/*
 * v17fax.h -- ITU-T V.17 (fax): the receiver's primitives, and three
 *             transmit-side setters that sit beside them.
 *
 * `include/dsplib/v17data.h` already carries the V.17 TRANSMITTER's two data
 * leaves and the offsets inside the transmitter instance.  This header is the
 * other side: the functions the fax state machine calls to drive and
 * interrogate the V.17 RECEIVER, plus `SeedScramblerV17`, `SetEncoderV17` and
 * `V17TX_status`, which are transmit-side and are here because they are the
 * remainder of the same batch rather than because they belong with the
 * receiver.
 *
 * `tools/service.py` puts every one of them on the FAX side.
 *
 * ---------------------------------------------------------------------------
 * THE TWO INSTANCES ARE NOT MODELLED, AND THERE ARE TWO OF THEM
 *
 * This follows `v17data.h`'s ruling verbatim: `V17RX_create` (3,201 bytes) and
 * `V17TX_create` (1,043) are not reconstructed, so naming their fields would
 * be guessing.  Every parameter is `void *` and every offset is a named
 * constant below with the evidence beside it.
 *
 * That there are TWO instance types, and not one, is MEASURED rather than
 * assumed, and the measurement is a contradiction that only two types resolve:
 *
 *   - `SeedScramblerV17` and `SetEncoderV17` dereference `obj + 0x28` as a
 *     POINTER and write through it -- the transmitter's private block, which
 *     `v17data.h` establishes twice over as `V17TX_OBJ_FP`;
 *   - `V17RX_modem` clears bit 1 of the BYTE at `obj + 0x29` and then RETURNS
 *     the int at `obj + 0x28`.
 *
 * Byte 0x29 is inside the 4 bytes at 0x28.  Clearing a bit of a live pointer
 * and then returning it is not something one object can survive, so the two
 * functions are not looking at the same struct.  The split is corroborated by
 * every other offset: the transmit pair touch 0x24 and 0x28 only, and the ten
 * receive-side functions touch 0x18, 0x1c, 0x20, 0x28, 0x29, 0x5c and 0x60
 * and never dereference 0x28.  Finding F8850.
 *
 * ---------------------------------------------------------------------------
 * THE AUTHOR'S OWN WORDS
 *
 * Five format strings, which is the strongest evidence class this tree
 * recognises, and they name what the fields below are for:
 *
 *   .rodata.str1.4 0x12bfc  "ERROR: SRE buffer violation!(%d)"
 *   .rodata.str1.4 0x12c20  "V17 Decoder error too big... no carrier\n"
 *   .rodata.str1.4 0x12c4c  "sudden energy drop > 8[dB], no carrier"
 *   .rodata.str1.4 0x12c74  "V17 Dec error too big... unreliable data\n"
 *   .rodata.str1.1 0x04dfb  "V17: V21 Carrier detected\n"
 *
 * `0x12c20` is referenced from BOTH `CarrierDetectV17` and
 * `DataCarrierDetectV17`, and in both it is reached only when the short at
 * receiver state + 0x1c2 exceeds 0x3fff -- so that field is the decoder error
 * the message names, and `V17RXS_DEC_ERROR` is the author's word and not a
 * guess.  `GetSNRV17` returning `13 - error` from the same field is the third
 * independent use.
 */

#ifndef DSPLIB_V17FAX_H
#define DSPLIB_V17FAX_H

#include "dsplib/v17data.h"	/* V17TX_OBJ_FP, V17FP_SMC, V17FP_ENCODER_SEL */

struct fax_fifo;
struct fpm_pps;
struct fpm_sdm;
struct sgd;
struct v17rx_cfg;	/* faxcfg.h -- and the receive instance's own head */

/* ------------------------------------------------------------------------ */
/* The status block                                                         */

/*
 * What `V17TX_status` fills, and it is a SHARED block: `v22status.h`'s
 * `struct v22_status` and `v32fpstat.h`'s `struct v32_status` model the same
 * layout for their own datapumps, field for field, and both were derived from
 * their own disassembly with nothing to do with V.17.
 *
 * THE V.17 WRITE SET MATCHES THAT LAYOUT EXACTLY, INCLUDING WHAT IT SKIPS.
 * `V17TX_status` writes +0x00, +0x02, +0x04, +0x06, +0x08, +0x0a, +0x0c,
 * +0x10, +0x12, +0x14, +0x15 and +0x18, and leaves +0x0e and +0x16 alone --
 * and +0x16 is the one field `v32_status` itself annotates "not written". A
 * reading that walked the block in even steps would have written +0x0e; the
 * object steps over it.
 *
 * +0x02 IS A BIT RATE, AND THAT IS MEASURED HERE RATHER THAN BORROWED.
 * `V21TX_status` (0xa2c00) stores the literal `$0x12c` -- 300 -- into +0x02,
 * and V.21 is a 300 bit/s modem. `v22_status` independently calls the same
 * offset 1200-or-2400 and `v32_status` calls it `RATEv32[...] bit/s`. Three
 * modules, three derivations, one meaning.
 *
 * THREE SPELLINGS OF ONE BLOCK IS A PROBLEM AND THIS ADDS A FOURTH, KNOWINGLY.
 * "One type, one home" is about a type having one DEFINITION, and these are
 * four differently-named types, so no gate fires -- but they are one thing and
 * they should end up as one. Unifying them is not this batch's to do: it
 * touches two headers this batch does not own and a third module's tests. The
 * names below are deliberately the ones `v22_status` and `v32_status` already
 * use wherever the two agree, so that a later unification is a rename and not
 * a re-derivation. Where they DISAGREE (+0x06, +0x08, +0x0c, +0x10, +0x12) the
 * neutral offset name is kept, because V.17 writes a constant zero to every
 * one of them and so has no evidence of its own to break the tie.
 *
 * The field widths are the object's: +0x14 and +0x15 are BYTES, written with
 * `movzbl`/`andb`/`mov %al`, and +0x18 is an `int` copied 32 bits at a time.
 */
struct v17_status {
	short protocol;		/* +0x00 <- params + 0x00                    */
	short tx_bps;		/* +0x02 <- params + 0x02; see above         */
	short rx_bps;		/* +0x04 always 0 here                       */
	short short_06;		/* +0x06 0 from V17TX_status; see below      */
	/*
	 * +0x08 IS THE SNR, AND `V17RX_status` IS WHAT ESTABLISHES IT.  It
	 * stores `GetSNRV17`'s return here and nothing else, and `GetSNRV17`
	 * is the author's own function name for `13 - V17RXS_DEC_ERROR`.
	 * `v32_status` calls the same offset `snr` from its own disassembly,
	 * so two modules agree and neither derivation used the other.  It was
	 * `short_08` while `V17TX_status`, which writes a constant zero here,
	 * was the only V.17 writer known.  Finding F9100.
	 */
	short snr;		/* +0x08 <- GetSNRV17; 0 from V17TX_status  */
	short short_0a;		/* +0x0a always 0 here                       */
	short short_0c;		/* +0x0c always 0 here                       */
	short short_0e;		/* +0x0e NOT WRITTEN -- the object steps over
				 *       it, and v32_status zeroes it        */
	short short_10;		/* +0x10 <- params + 0x02, read a second time */
	short short_12;		/* +0x12 always 0 here                       */
	unsigned char flags;	/* +0x14 ASSIGNED, not merged; see D1032     */
	unsigned char flags1;	/* +0x15 bit 0 cleared, bits 1..7 preserved  */
	short short_16;		/* +0x16 NOT WRITTEN                         */
	int int_18;		/* +0x18 <- params + 0x18                    */
};

/*
 * The one bit of `params + 0x10` that reaches `flags`.  Named by its VALUE per
 * CLAUDE.md; what it INDICATES is not established, and neither `v22_status`
 * nor `v32_status` names their equivalent either.
 */
#define V17_STATUS_FLAG_04	0x04

/* The two bits `flags` is masked of before being overwritten anyway. */
#define V17_STATUS_FLAGS_CLEAR	0x03

/* The one bit `flags1` is cleared of, and which is genuinely a mask. */
#define V17_STATUS_FLAGS1_CLEAR	0x01

/*
 * The rest of `flags`, from `V17RX_status`, which is the ONLY function in this
 * batch that builds the whole byte rather than assigning it.
 *
 * NAMED BY VALUE AND NOTHING MORE, per CLAUDE.md.  What each bit indicates is
 * NOT established: three of the six are read-modify-writes of state fields
 * whose own meaning is unknown (`V17RXS_BYTE_001C` bit 0, and whether
 * `V17RXS_INT_0000` and `V17RXS_INT_0010` are zero), and the other three --
 * 0x10 set, 0x40 set, 0x80 cleared -- are unconditional constants with nothing
 * behind them to name.  `V17_STATUS_FLAG_04` above is the one bit both this
 * function and `V17TX_status` touch, and even there the two disagree about
 * what it should end up as.
 *
 * THE WHOLE BYTE IS DETERMINISTIC AND THAT IS MEASURED, not deduced from the
 * masks looking exhaustive: the object's chain leaves
 * `0x50 | bit1 | bit3 | bit5`, so the caller's incoming bits 4 and 6 survive
 * only by being re-set and every other incoming bit is overwritten.  See the
 * derivation in `src/fax/v17.c`.  Finding F9101.
 */
#define V17_STATUS_FLAG_01	0x01
#define V17_STATUS_FLAG_02	0x02
#define V17_STATUS_FLAG_08	0x08
#define V17_STATUS_FLAG_10	0x10
#define V17_STATUS_FLAG_20	0x20
#define V17_STATUS_FLAG_40	0x40
#define V17_STATUS_FLAG_80	0x80

/* ------------------------------------------------------------------------ */
/* The TRANSMIT instance -- offsets shared with v17data.h                    */

/*
 * The scrambler, in the transmitter's private block.
 *
 * `v17data.h` records that `V17TX_create` builds an `SDM` at fp + 0x1c and
 * that neither of its two functions touches it.  `SeedScramblerV17` is what
 * touches it: it stores its second argument at fp + 0x2c, which is
 * `struct fpm_sdm` + 0x10 -- `reg`, the shift register, the ONLY 32-bit field
 * in that struct and the only one a "seed" could mean.  `fpm_sdm.h` pins the
 * layout independently of anything here, so the pairing of the function's
 * name to the field is not inference from the offset alone.  Finding F8851.
 */
#define V17FP_SDM		0x1c

/*
 * The short `SetEncoderV17` writes for encoder modes 0 and 2 and NOT for
 * mode 1.
 *
 * NEUTRAL NAME AND DELIBERATELY SO.  It is `V17FP_SMC` + 6, inside the SMCv17
 * coder state, which nothing reconstructed models.  What is established is
 * only that the differential and trellis encoders are given this value and
 * the absolute encoder is not; what it means is not.
 */
#define V17FP_SMC_SHORT_06	0x3a

/*
 * A pointer the private block owns, released by `V17TX_delete` between the
 * shaper and the block itself.  NEUTRAL: `sysdep_free` types nothing, and
 * nothing else read here touches it.
 */
#define V17FP_PTR_0010		0x10

/*
 * The int `V17TX_modem` returns, and the flag byte inside it.
 *
 * IT IS THE SAME SHAPE AS THE RECEIVER'S `V17RX_OBJ_RESULT` / `_B1` PAIR, byte
 * for byte: the object clears one bit of the byte at +0x21 on entry, sets that
 * same bit and stores a literal 9 into the BYTE at +0x20 on one condition, and
 * returns the INT at +0x20.  So +0x21 is byte 1 of the four bytes at +0x20 and
 * the function both modifies and returns the same word, which is exactly what
 * `V17RX_modem` does 0x28 into the other instance.
 *
 * Neutral, like its receive-side twin.  What the bit indicates is not
 * established; what the 9 means is not established either, and it is spelled
 * as the object spells it -- a BYTE store, which is why it cannot be written
 * through the int.  Named by VALUE per CLAUDE.md.
 *
 * The CONDITION is established: the flag and the 9 are written only when the
 * word count the caller asked for differs from what `FIFO_write` accepted, so
 * they report a transmit queue that would not take the whole block.  On the
 * non-FIFO arm the two are equal by construction and neither is ever written.
 */
#define V17TX_OBJ_RESULT	0x20
#define V17TX_OBJ_RESULT_B1	0x21
#define V17TX_RESULT_B1_BIT1	0x02
#define V17TX_RESULT_BYTE_09	9

/*
 * `SetTxModeV17`'s OWN unrecognised-`mode` arm writes the same two fields --
 * `V17TX_OBJ_RESULT` gets a literal 7 and `V17TX_OBJ_RESULT_B1` gets
 * `V17TX_RESULT_B1_BIT1` set -- but ALSO clears bit 0 of `_B1`, which
 * `V17TX_modem`'s arm never touches (`0xa0bd4`..`0xa0bee`).  Same shape,
 * different literal, one more bit: named separately rather than reusing
 * `V17TX_RESULT_BYTE_09` and pretending the two arms agree on the value.
 */
#define V17TX_RESULT_BYTE_07	7

/* ------------------------------------------------------------------------ */
/* Inside the block at V17TX_OBJ_PARAMS                                     */

/*
 * `v17data.h` describes this block as "a parameter block the instance points
 * at rather than owns", which is what `V17TX_create` alone could say.
 * `V17TX_delete` SETTLES THE OWNERSHIP THE OTHER WAY: it releases the block's
 * `SGD`, its `fax_fifo` and then the block itself, so the transmitter owns it.
 * The name in `v17data.h` is left alone -- renaming it is a change to a header
 * this batch does not own -- and the correction is recorded here and in
 * finding F9106 rather than by two headers disagreeing.
 *
 * `V17TXP_FIFO` and `V17TXP_SGD` are TYPED BY THEIR CALLEES, which is rank 2:
 * +0x00 is `FIFO_write`'s and `FIFO_delete`'s first argument and +0x04 is
 * `SGD_delete`'s.  The other two are neutral -- +0x08 is an int `V17TX_modem`
 * tests for zero to choose between queueing the caller's block and passing it
 * straight through, and +0x14 is a dispatch slot planted at construction
 * (`call *0x14(%edx)` carries no relocation, so nothing here can say which
 * function lands in it).
 */
#define V17TXP_FIFO		0x00
#define V17TXP_SGD		0x04
#define V17TXP_INT_0008		0x08
#define V17TXP_PROCESS		0x14

/*
 * What `V17TX_modem` initialises its inner loop's budget to, ONCE, before the
 * loop rather than per iteration.  From the object's `movw $0x30,0x1a(%esp)`.
 * The dispatch slot is what decrements it, and the loop runs while it is
 * STRICTLY POSITIVE as a signed short -- `cmpw $0x0` with `jg`, so a slot that
 * overshot into negative territory stops the loop rather than wrapping it.
 */
#define V17TX_MODEM_BUDGET	0x30

/*
 * The three values `SetEncoderV17` will accept, which is what BOUNDS the
 * selector `v17data.h` describes.  That header records that nothing it read
 * writes `V17FP_ENCODER_SEL` and that nothing bounds the index; this function
 * is the writer, and it writes 0, 1 or 2 and ignores every other argument.
 * The order matches `V17TX_create`'s table exactly -- dif at fp + 0x80, abs at
 * fp + 0x84, tcm at fp + 0x88 -- so the argument selects the encoder by the
 * same numbering.  Finding F8852.
 */
#define V17_ENCODER_DIF		0
#define V17_ENCODER_ABS		1
#define V17_ENCODER_TCM		2

/* ------------------------------------------------------------------------ */
/* The RECEIVE instance                                                     */

/*
 * Two `short *` the receiver does not own, and the coefficient store they are
 * the destination of.
 *
 * `StoreCoefV17` copies 49 entries from each of the receiver state's two
 * coefficient pointers into the arrays at +0x18 and +0x1c.  The pairing of
 * the two is the author's: the function's own name is the only word for what
 * is being stored.  WHICH of the two arrays is which rail is NOT established
 * -- nothing traced distinguishes them -- so they are numbered, not named.
 */
#define V17RX_OBJ_COEFSAVE0	0x18
#define V17RX_OBJ_COEFSAVE1	0x1c

/*
 * A `short *` that holds a saved copy of the receiver's rate.
 *
 * ESTABLISHED BY A MATCHED PAIR, which is why this one carries a real name
 * where its two neighbours do not: `StoreCoefV17` writes
 * `*p = (short)rx[V17RXS_RATE]` and `Restore_rateV17` writes
 * `rx[V17RXS_RATE] = *p`.  The direction, the width and the field at the far
 * end all agree, and "rate" is the author's word from `Restore_rateV17`.
 */
#define V17RX_OBJ_RATESAVE	0x20

/*
 * The int `V17RX_modem` returns, the STATUS BYTE at its low end, and the flag
 * byte above that.
 *
 * See the two-instances note above: 0x29 is byte 1 of the 4 bytes at 0x28, so
 * the object both modifies and returns the same word, and 0x28 is byte 0 of
 * it.  `V17RX_modem` clears one bit of 0x29 on entry and hands the whole word
 * back.
 */
#define V17RX_OBJ_RESULT	0x28
#define V17RX_OBJ_RESULT_B1	0x29

/*
 * THE THREE NAMED BITS OF 0x29, AND THE NAMES ARE NEW.
 *
 * `V17RX_RESULT_B1_BIT1` (0x02) and `V17RX_RESULT_B1_BIT7` (0x80) are RETIRED
 * here, and the retirement is a change to this header's own record, so it is
 * written down rather than done quietly.  Both were named BY VALUE when
 * `V17RX_modem` and `V17RX_status` were the only functions in the tree that
 * touched the byte -- one clearing a bit and one testing another, with nothing
 * on the other end of either.  `RxHdxDataV17` and `RxHdxErrorV17` are the
 * other end, and with them the WHOLE OBJECT can be enumerated: every `orb`,
 * `andb`, `testb` and read-modify-write of `obj + 0x29` in all 44 V.17
 * symbols, which is 34 sites.  Findings F9230 and F9231.
 *
 * The enumeration is this modem's own, taken from `dis.py`.  V.21's byte at
 * `rx + 0x19` has the same three bits in the same three roles (`v21fax.h`,
 * finding F8896) and Bell 103's does too -- that is a CORROBORATION and it is
 * not the derivation, exactly as `v21fax.h` says of its own.
 *
 * ERROR (0x02) -- SET by `orb $0x2` at 0x0a00fc inside `RxHdxErrorV17`, by the
 *   DEFAULT arm of `RxNextStateV17` (0x0a0165 and 0x0a0192, the two copies of
 *   one arm either side of the debug print, beside the "V17RX_DEFAULT" status
 *   3), and by the four transitions that install `RxHdxErrorV17` --
 *   `RxHdxScramV17` 0x0a0548, `RxHdxBridgeV17`, `RxHdxPrtcolV17`,
 *   `RxHdxEpochDetV17`.  CLEARED by `V17RX_modem` alone, `andb $0xfd` at
 *   0x09ff9d, at the top of every block.
 *   Nothing in the object READS it: it leaves through the returned word, and
 *   a caller that does not read that word each block loses the event.
 *
 *   IT IS NOT DELIVERED SET, AND THE LINE THAT SAID SO IS WITHDRAWN.  This
 *   paragraph used to end "SEEDED set by `V17RX_create`", which read the
 *   `orb $0x2,0x29(%ebp)` at 0x097140 and stopped there.  Further down its own
 *   fall-through `V17RX_create` clears the WHOLE WORD -- `movl $0x0,0x28(%ebp)`
 *   at 0x0977a4 -- and then finishes `orb $0x50,0x29(%ebp)` at 0x0977ab and
 *   `movb $0x2,0x28(%ebp)` at 0x0977b5.  So on any path that reaches the
 *   second group the instance is delivered with 0x28 = 2, 0x29 = 0x50 and
 *   0x2a = 0, and ERROR is CLEAR.  Finding F9442.
 *
 * CARRIER (0x20) -- CLEARED and then SET AGAIN if and only if
 *   `CarrierDetectV17` answers non-zero, in `RxHdxIdleV17` (`andb $0xdf` at
 *   0x0a0441, the call, `orb $0x20` at 0x0a0455) and in `RxHdxStartV17`
 *   (0x0a081d / 0x0a0860).  `RxHdxDataV17` sets it on entry and clears it on
 *   the arm where `DataCarrierDetectV17` says the carrier has gone.  READ by
 *   `testb $0x20,0x29(%esi)` at 0x0a0459 in `RxHdxIdleV17`, which is what
 *   gates that function's look at the decoder error.  Both ends measured, and
 *   the SET is gated on a function whose name is the author's own.
 *
 * LOW_SNR (0x80) -- CLEARED by `andb $0x7f` at 0x0a00b1 in `RxHdxDataV17` and
 *   SET at 0x0a00c7 if and only if `GetSNRV17` came back at or below
 *   `V17RX_SNR_THRESHOLD`; the same `cmpw $0x8` / `jg` / `orb $0x80` shape
 *   appears in `RxHdxScramV17` (0x0a0566), `RxHdxBridgeV17` and
 *   `RxHdxPrtcolV17`, and those four are exactly the four handlers that call
 *   `GetSNRV17`.  READ by `testb $0x80` at 0x0a093d in `V17RX_status`, where
 *   a SET bit makes the reported +0x06 zero.  Both ends measured.
 *
 * Bits 0x04 and 0x08 are touched by nothing in the object.  BITS 0x10 AND
 * 0x40 ARE, and the line that put 0x40 in the untouched set is withdrawn:
 * `V17RX_create` sets both, once, in the `orb $0x50,0x29(%ebp)` at 0x0977ab
 * that follows its clearing of the whole word.  Nothing anywhere READS
 * either, and nothing else writes them, so they are named BY VALUE and
 * nothing more -- see `V17RX_FLAG_BIT4` below.  Finding F9473.
 *
 * THE TWO ERROR MASKS ARE NOT THE SAME, AND THE DIFFERENCE IS THE DATA BIT.
 *   `RxNextStateV17`'s default arm is `or $0x2` then `and $0xde`, which clears
 *   CARRIER *and* `V17RX_FLAG_DATA`; the four handlers' error arms are
 *   `or $0x2` then `and $0xdf`, which clears CARRIER alone.  One instruction
 *   apart and they read alike; `src/fax/v17.c` spells each site the way the
 *   object spells it, and F9440 records what it cost to notice.
 *
 * DATA (0x01) -- NAMED NOW, AND IT WAS LEFT UNNAMED BECAUSE THE ONLY FUNCTION
 *   THAT TOUCHES IT WAS UNWRITTEN.  `RxNextStateV17` is its only writer in the
 *   whole object and it is written here, so the pattern can be enumerated
 *   rather than sampled: the bit is SET on exactly the two transitions that
 *   install `RxHdxDataV17` (`orb $0x1,0x29` at 0x0a02a8 leaving state SCRAM
 *   and at 0x0a02ff leaving state IDLE) and CLEARED on every other transition
 *   and by the default arm.  Set if and only if the handler just installed is
 *   the DATA handler, which is what the name says and nothing more.  NOTHING
 *   READS IT anywhere in the object; like ERROR it leaves through the returned
 *   word.  `V21RX_FLAG_DATA` is the same bit of the same shape in `v21fax.h`,
 *   which is a CORROBORATION and not the derivation.  Finding F9440.
 */
#define V17RX_FLAG_ERROR	(1 << 1)
#define V17RX_FLAG_CARRIER	(1 << 5)
#define V17RX_FLAG_LOW_SNR	(1 << 7)
#define V17RX_FLAG_DATA		(1 << 0)

/*
 * The two `V17RX_create` sets and no reader has.  NAMED BY VALUE, per
 * CLAUDE.md, and deliberately: their only write is one `orb` immediate that
 * carries no evidence about what either indicates, and the whole object
 * contains no read.  Do not promote them to a meaning.
 */
#define V17RX_FLAG_BIT4		(1 << 4)
#define V17RX_FLAG_BIT6		(1 << 6)

/*
 * BYTE 2 OF THE SAME WORD, AND ITS ONE BIT.  Both are named BY VALUE and the
 * role of NEITHER is established -- which is the whole point of writing them
 * down this way.
 *
 * `RxNextStateV17` is the only function in the object that touches +0x2a.  It
 * clears bit 0 on six of its seven transitions and on the default arm, SETS it
 * on exactly one (state DATA -> state IDLE, `orb $0x1,0x2a` at 0x0a02d8), and
 * skips it altogether on the SCRAM arm of state PROTOCOL.  Nothing anywhere
 * reads it, and nothing writes any other bit of it, so there is no second end
 * to measure against and no name to give it beyond its offset and its bit.
 * The near-inverse of `V17RX_FLAG_DATA` is a TEMPTING reading and is declined:
 * the two disagree on the PROTOCOL/SCRAM arm, which writes one and not the
 * other, so they are not one flag spelled twice.  Findings F9441 and D1213.
 */
#define V17RX_OBJ_RESULT_B2	0x2a
#define V17RX_RESULT_B2_BIT0	(1 << 0)

/*
 * `RxHdxDataV17` raises `V17RX_FLAG_LOW_SNR` when `GetSNRV17` comes back at or
 * below this.  The compare is 16 bits wide and SIGNED -- `cmpw $0x8,%ax` then
 * `jg` -- which is what `GetSNRV17`'s own `short` return gives, so nothing
 * narrows it here.  `GetSNRV17` is `13 - V17RXS_DEC_ERROR`, so the flag is
 * raised once the decoder error reaches 5.
 */
#define V17RX_SNR_THRESHOLD	8

/*
 * The STATUS BYTE at `V17RX_OBJ_RESULT`, and all nine values it takes.
 *
 * NOTHING IN THE OBJECT READS ANY OF THEM -- the byte leaves through the word
 * `V17RX_modem` returns and nothing else -- so a name here can only be the
 * SITE that writes it, which is `v21fax.h`'s ruling for the same field of the
 * same shape and is what the comments below give.  The write set is now
 * COMPLETE, because every writer is reconstructed.
 *
 * THE LAST FOUR ARE THE RATE LADDER, AND THAT IS RANK-2 EVIDENCE RATHER THAN
 * A GUESS.  `RxHdxScramV17` (0x0a0551) and `RxNextStateV17`'s IDLE arm
 * (0x0a0303) both read `V17RXC_RATE_CODE` and write 9, 8, 7 or 6 from it, and
 * `V17RX_create` is what puts the code there: it switches on
 * `V17RX_OBJ_RX_BPS` at 0x097113 and stores 0 for 7200, 1 for 9600, 2 for
 * 12000 and 3 for 14400 (and 3 for anything else).  So the ladder maps a bit
 * rate to a status byte, one to one, and the names say which rate.  What the
 * VALUES mean to a reader of the word is still not established -- see
 * `V17RX_RATE_7200` below and finding F9443.
 *
 * `V17RX_STATUS_RATE_14400` is also the ladder's `else` arm (`cmp $0x2` /
 * `sete` / `add $0x6`), so a rate code the object never writes lands there
 * too.
 */
#define V17RX_STATUS_DATA	0	/* RxHdxDataV17, every block          */
#define V17RX_STATUS_CARRIER	1	/* Scram/Bridge/Prtcol/EpochDet, the
					 * carrier-present arm                */
#define V17RX_STATUS_START	2	/* RxHdxStartV17; also V17RX_create   */
#define V17RX_STATUS_DEFAULT	3	/* RxNextStateV17's default arm       */
#define V17RX_STATUS_ERROR	4	/* the same four handlers' error arm  */
#define V17RX_STATUS_IDLE	5	/* RxHdxIdleV17, every block          */
#define V17RX_STATUS_RATE_14400	6
#define V17RX_STATUS_RATE_12000	7
#define V17RX_STATUS_RATE_9600	8
#define V17RX_STATUS_RATE_7200	9

/*
 * The two shorts `V17RX_status` copies out of the receive instance, and this
 * is the only function that reads either.
 *
 * NAMED BY THEIR DESTINATION, WHICH IS CLAUDE.md's RANK 2 AND NOT USAGE
 * INFERENCE: +0x00 lands in `struct v17_status::protocol` and +0x04 in
 * `::rx_bps`, and both of those fields carry their names from `v22_status` and
 * `v32_status`, each derived from its own disassembly with nothing to do with
 * V.17.  `V17TX_status` puts the TRANSMIT rate in +0x02 and zero in +0x04;
 * this function does the mirror image -- zero in +0x02 and a rate in +0x04 --
 * which is exactly what a receiver-side reporter should do and is the second
 * thing that fits.
 *
 * +0x04 IS ALSO WRITTEN TO THE STATUS BLOCK'S +0x12, a second time and
 * unchanged.  That offset stays neutral: two modules disagree about it and
 * nothing here breaks the tie.
 */
#define V17RX_OBJ_PROTOCOL	0x00
#define V17RX_OBJ_RX_BPS	0x04

/*
 * TWELVE FIELDS `V17RX_create` WRITES LAST, SIX OF THEM OUT OF THE EQUALISER.
 *
 * The constructor's final block (0x0977a2..0x097815) copies six handles from
 * the `struct fpm_fse` it has just built and zeroes six more slots.  The six
 * copies are TYPED BY THE STRUCT THEY COME OUT OF, which is CLAUDE.md's
 * rank 2 and not usage inference -- `fpm_fse.h` models every one of them
 * independently of anything here:
 *
 *     +0x2c <- fse.out_i        short *,  one entry per symbol per call
 *     +0x30 <- fse.out_q        short *
 *     +0x34 <- &fse.n_out       unsigned short *, the count itself
 *     +0x38 <- fse.icoeff       short *,  and it is `V17RXS_COEF0`
 *     +0x3c <- fse.qcoeff       short *,  and it is `V17RXS_COEF1`
 *     +0x40 <- fse.cfg.taps     unsigned short, 49 = `V17_COEF_N`
 *
 * So the instance publishes the equaliser's output buffers, their length and
 * its two live coefficient arrays to whatever holds the instance.  The
 * `movzwl` on the last is a dead extension and follows the local's declared
 * type (finding F7803), not the `short` field's.
 *
 * THE OTHER SIX HAVE NO EVIDENCE OF ROLE AND ARE NOT NAMED.  Every one is a
 * constant zero written once and read by nothing in the 1.2 MB.  The widths
 * are the object's: `movl` at +0x44, +0x48, +0x50 and +0x54, `movw` at +0x4c
 * and +0x58.  +0x42, +0x4e and +0x5a are never written at all.  Finding F9476.
 */
#define V17RX_OBJ_OUT_I		0x2c
#define V17RX_OBJ_OUT_Q		0x30
#define V17RX_OBJ_N_OUT		0x34
#define V17RX_OBJ_ICOEFF	0x38
#define V17RX_OBJ_QCOEFF	0x3c
#define V17RX_OBJ_TAPS		0x40
#define V17RX_OBJ_INT_0044	0x44
#define V17RX_OBJ_INT_0048	0x48
#define V17RX_OBJ_SHORT_004C	0x4c
#define V17RX_OBJ_INT_0050	0x50
#define V17RX_OBJ_INT_0054	0x54
#define V17RX_OBJ_SHORT_0058	0x58

/*
 * The receiver's two sub-blocks.  Both are POINTERS the instance holds.
 *
 *   +0x5c  the control block: two detector objects, a scratch buffer, a
 *          dispatch slot, and a second detector chain of its own
 *   +0x60  the demodulator state: every FPM object the receive chain runs,
 *          and about 20 KB of it -- `DemodDataV17` reaches +0x4fa8
 */
#define V17RX_OBJ_CTL		0x5c
#define V17RX_OBJ_STATE		0x60

/* ------------------------------------------------------------------------ */
/* Inside the control block at V17RX_OBJ_CTL                                */

/*
 * `struct fpm_mtd *` and `struct fpm_tone *`, TYPED BY THEIR CALLEES:
 * `DemodDataV17` passes +0x00 to `FPM_MTD_detect` and +0x04 to
 * `FPM_TONE_kill`, which is CLAUDE.md's rank-2 evidence and not usage
 * inference.
 */
#define V17RXC_MTD		0x00
#define V17RXC_TONE		0x04

/*
 * An int `RxHdxDataV17` requires to be ZERO before it will demodulate, and the
 * SECOND half of its gate: the carrier must be up AND this must be clear.
 *
 * NEUTRAL.  It is an `int`: `mov 0x8(%ecx),%edx` then `test %edx,%edx`, 32
 * bits at both ends.
 *
 * IT HAS THREE WRITERS, AND THE LINE THAT SAID IT HAD NONE IS WITHDRAWN.  This
 * comment used to call it "the one field in this header with exactly one
 * reader and no writer anywhere in the object", which was measured over the
 * functions this header had at the time and reported as if it were measured
 * over the object.  The writers are `RxNextStateV17`'s DATA arm
 * (`movl $0x0,0x8(%edx)` at 0x0a02d1, on the transition into IDLE) and
 * `V17RX_control`, which writes 0 at 0x0a089c and 1 at 0x0a08e0 from two bits
 * of its own argument.  So the gate is cleared whenever the machine leaves
 * DATA and is driven from outside by the control entry point; what it
 * INDICATES is still not established, and the name stays neutral.  Finding
 * F9442.
 *
 * IT IS NOT `V17RXS_INT_0008`, WHICH IS A DIFFERENT BLOCK.  `DemodDataV17`'s
 * `mov 0x8(%ebp),%eax` at 0x0a51e3 reads +0x08 of the DEMODULATOR STATE
 * (`V17RX_OBJ_STATE`, one of the three enables); this is +0x08 of the CONTROL
 * block (`V17RX_OBJ_CTL`).  The two offsets are equal and the two fields are
 * not.  Finding F9232.
 */
#define V17RXC_INT_0008		0x08

/*
 * THE RECEIVE BIT RATE, AS A FOUR-VALUE CODE.
 *
 * `V17RX_create` is what establishes it, and the derivation is rank 2 -- a
 * writer that types the field -- rather than usage inference.  At 0x097113 it
 * loads `V17RX_OBJ_RX_BPS` (`movswl 0x4(%esi),%eax`, the same field
 * `V17RX_status` reports as `struct v17_status::rx_bps`) and switches on it:
 *
 *     0x1c20   7200 -> 0     (0x097b15)
 *     0x2580   9600 -> 1     (0x097a08)
 *     0x2ee0  12000 -> 2     (0x097b0a)
 *     0x3840  14400 -> 3     (0x097837)
 *     anything else -> 3     (0x097133)
 *
 * It has exactly two readers, `RxNextStateV17`'s IDLE arm and
 * `RxHdxScramV17`'s expiry path, and both do nothing with it but pick one of
 * `V17RX_STATUS_RATE_*`.  Both read it `movzwl`, so it is an `unsigned short`.
 * Finding F9443.
 */
#define V17RXC_RATE_CODE	0x0c

#define V17RX_RATE_7200		0
#define V17RX_RATE_9600		1
#define V17RX_RATE_12000	2
#define V17RX_RATE_14400	3

/*
 * An int `CarrierDetectV17` and `DataCarrierDetectV17` both require to be
 * non-zero before they will look at the decoder error at all.  Neutral: what
 * it indicates is not established, only that it gates the carrier verdict.
 *
 * `RxNextStateV17` reads it too, twice on one arm, and chooses BOTH the
 * countdown seed and whether to call `Restore_rateV17` from it.  That does not
 * type it either; see D1212 for why the second of the two reads is dead.
 */
#define V17RXC_INT_0010		0x10

/*
 * A dispatch slot: `V17RX_modem` calls `*(fn *)(ctl + 0x14)` with its own
 * four arguments unchanged.  `call *0x14(%eax)` carries no relocation, so
 * this is a table entry planted at construction and NOT a symbol reference.
 *
 * THE SEED IS `RxHdxStartV17`, AND THE LINE THAT SAID NOTHING COULD SAY SO IS
 * WITHDRAWN.  `V17RX_create` is the planter: `movl $RxHdxStartV17,0x14(%ebx)`
 * at 0x097083, carrying an `R_386_32` against that symbol, beside the
 * `V17RX_STATE_START` it writes into `V17RXC_STATE` fifteen bytes earlier.
 * `RxNextStateV17` is the only other writer and it agrees -- state and handler
 * are installed together on every one of its arms.  Finding F9471.
 */
#define V17RXC_PROCESS		0x14

/*
 * THE RECEIVE STATE NUMBER, and the rename that F9235 deferred has happened.
 *
 * That finding recorded the derivation and declined to act on it "because this
 * batch writes none of its writers"; `RxNextStateV17` is below, so every
 * writer is now reconstructed and the enumeration is complete.  The machine
 * stores 1, 2, 3, 4, 5 and 6 into it in `RxNextStateV17`'s six transition
 * arms, the four handlers that install `RxHdxErrorV17` store
 * `V17RX_STATE_ERROR` beside that store, and `V17RX_create` stores
 * `V17RX_STATE_START` at 0x097068.  `DemodDataV17`'s own test of it -- the one
 * that used to be all this header had -- is therefore "the machine has left
 * START", and it skips the tone-kill and tone-detect front end once it has.
 *
 * IT IS A SIGNED `short` AND THAT IS FORCED.  `RxNextStateV17` loads it
 * `movswl` at 0x0a013b and feeds the 32-bit result to `cmp $0x6` / `ja`, which
 * is UNSIGNED -- so a negative state takes the default arm rather than
 * indexing the jump table backwards.  An `unsigned short` cannot produce that
 * `movswl`; do not respell it.  `V27SH_SKIP_TONE` in `v27fax.h` is the same
 * field of the same machine with the same evidence.  Findings F9235 and F9440.
 *
 * `V17RXC_SHORT_0018` IS KEPT AS AN ALIAS, and only because `t_v17fax.c` still
 * spells it that way and is not this batch's file to edit.  New code uses
 * `V17RXC_STATE`.
 */
#define V17RXC_STATE		0x18
#define V17RXC_SHORT_0018	V17RXC_STATE

/*
 * BLOCKS REMAINING IN THIS STATE, and that is the whole of what is
 * established.
 *
 * `RxNextStateV17` seeds it on every transition but one (5, 1, 62, 1, 1, 0 and
 * 0; the IDLE arm does not write it at all, which is D1215), and each of
 * `RxHdxScramV17`, `RxHdxBridgeV17`, `RxHdxPrtcolV17` and `RxHdxEpochDetV17`
 * decrements it once per block and advances the machine when the decremented
 * value is at or below zero.  So it counts BLOCKS and it counts DOWN; what any
 * particular seed is FOR -- why the protocol state gets 62 blocks when the
 * rate was not restored and 1 when it was -- is not established and no name
 * here claims it.
 *
 * THE LOAD IS UNSIGNED AND THE TEST IS SIGNED, and both are the object's:
 * `movzwl 0x1a(%edx),%ecx` / `dec %ecx` / `test %cx,%cx` / `mov %cx,0x1a(%edx)`
 * / `jle`, at 0x0a0509 and its three copies.  The extension is dead -- only
 * the low half is stored and tested -- so it follows the declared type of the
 * LOCAL (finding F7803), and the `jle` is a signed 16-bit compare, so a seed of
 * 0x8000 expires immediately rather than running for 32768 blocks.
 * `v21fax.h`'s `struct v21_rx_hdx::countdown` is the same field of the same
 * shape in a sibling modem, which corroborates and is not the derivation.
 */
#define V17RXC_COUNTDOWN	0x1a

/*
 * THE EIGHT STATES, AND SEVEN OF THE NAMES ARE THE AUTHOR'S OWN WORDS.
 *
 * `RxNextStateV17` prints the name of the state it is LEAVING at the top of
 * each arm, gated on `dsplibs_debug_level > 1`, and the arms are the seven
 * entries of a compiler-generated jump table at `.rodata` 0xc2d0, so the
 * pairing of a value to a string is the table's and not a reading of it:
 *
 *     0  0x049bc  "V17RX_STATE_START\n"       arm at 0x0a019a
 *     1  0x049a5  "V17RX_STATE_EPOCH_DET\n"   arm at 0x0a01c7
 *     2  0x0497d  "V17RX_STATE_PROTOCOL\n"    arm at 0x0a0213
 *     3  0x049f4  "V17RX_STATE_BRIDGE\n"      arm at 0x0a0247
 *     4  0x049e1  "V17RX_STATE_SCRAM\n"       arm at 0x0a0270
 *     5  0x049cf  "V17RX_STATE_DATA\n"        arm at 0x0a02b1
 *     6  0x04993  "V17RX_STATE_IDLE\n"        arm at 0x0a02e5
 *
 * -- and the mapping is FORCED TWICE OVER, because each arm also installs the
 * handler whose own blob symbol name matches the state it writes: state 1 goes
 * with `RxHdxEpochDetV17`, 2 with `RxHdxPrtcolV17`, 3 with `RxHdxBridgeV17`, 4
 * with `RxHdxScramV17`, 5 with `RxHdxDataV17` and 6 with `RxHdxIdleV17`.
 *
 * `V17RX_STATE_ERROR` IS THE ONE WITHOUT A STRING.  It has no case, so it
 * falls to the default arm (D1211), and its name comes from the handler the
 * four training states install beside it, `RxHdxErrorV17`.  Finding F9440.
 */
#define V17RX_STATE_START	0
#define V17RX_STATE_EPOCH_DET	1
#define V17RX_STATE_PROTOCOL	2
#define V17RX_STATE_BRIDGE	3
#define V17RX_STATE_SCRAM	4
#define V17RX_STATE_DATA	5
#define V17RX_STATE_IDLE	6
#define V17RX_STATE_ERROR	7

/* The scratch buffer `DemodDataV17` copies its input into.  `short *`. */
#define V17RXC_SCRATCH		0x1c

/*
 * The short that chooses which of `DataCarrierDetectV17`'s two completely
 * different bodies runs: zero takes the same path as `CarrierDetectV17`,
 * non-zero takes the V.21 tone watch.  Neutral -- nothing traced writes it,
 * so what selects the mode is outside what has been read.
 */
#define V17RXC_SHORT_0020	0x20

/*
 * The SECOND detector chain, which is `DataCarrierDetectV17`'s alone: a
 * `struct fpm_mtd *` at +0x24, its own `short *` buffer at +0x28, a sample
 * counter at +0x2c, a latch at +0x2e and a `struct fpm_agc` at +0x30.
 *
 * `V17RXC_MTD2` and `V17RXC_BUF2` are typed by their callees exactly as the
 * first pair are.  `V17RXC_AGC` is `FPM_AGC_agc`'s first argument, so it is
 * `struct fpm_agc` by the same rank-2 rule.
 *
 * THE COUNTER COUNTS `FPM_MTD_ABSENT`, WHICH IS NOT SILENCE.  It is the field
 * the "V17: V21 Carrier detected" message hangs off, and the temptation is to
 * read it as a silence timer -- but the object accumulates while
 * `FPM_MTD_detect` returns ZERO and clears on anything else, and `fpm_mtd.h`
 * defines `FPM_MTD_ABSENT` as 0 and glosses it "signal present, but not in
 * band".  `FPM_MTD_PRESENT` is 1 and `FPM_MTD_NOSIGNAL` is 2, so both a
 * detected tone AND a dead line clear it.
 *
 * So what crossing 0x4ff means is 1,280 consecutive samples -- 160 ms at
 * 8 kHz -- of energy on the line that is NOT the band this detector watches,
 * which is a coherent reason to conclude something else has taken the
 * channel.
 *
 * AND THE BAND IS V.21 CHANNEL 2, WHICH SETTLES THE QUESTION THIS COMMENT
 * LEFT OPEN.  `V17RX_create` is reconstructed and is what configures this
 * detector: at 0x09709f it loads `V21_CHAN2_MTD_COEFF` -- `faxcfg.h`'s
 * two-section bank, shared by all three fax receiver constructors -- into
 * `fpm_mtd_cfg::coeff`, with `min_level` 300 where the FIRST detector, the
 * V.17 one at `V17RXC_MTD`, gets `V17_MTD_COEFF` and 100.  So the message the
 * counter hangs off ("V17: V21 Carrier detected") names the same band the
 * configuration does, from two independent directions.  The neutral field
 * name is kept because what the counter counts is still absence and not the
 * tone.  Finding F9471.
 */
#define V17RXC_MTD2		0x24
#define V17RXC_BUF2		0x28
#define V17RXC_OFFBAND		0x2c
#define V17RXC_SHORT_002E	0x2e
#define V17RXC_AGC		0x30

/* The threshold the counter is compared against, from the object. */
#define V17RXC_OFFBAND_MAX	0x4ff

/* ------------------------------------------------------------------------ */
/* Inside the demodulator state at V17RX_OBJ_STATE                          */

/*
 * Three ints ANDed with the AGC's signal flag and stored elsewhere in the
 * same block, and the three destinations.
 *
 * THE THREE SOURCES ARE NEUTRAL: what is established is the plumbing, not the
 * meaning.  They sit immediately above `V17RXS_INT_0000`, which is consistent
 * with a small int array -- but nothing read here proves it is one, so they
 * are spelled as separate fields.
 *
 * THE FOUR DESTINATIONS ARE NOT NEUTRAL ANY MORE, and the tiling in F8854 is
 * why.  `V17RXS_SRE` is 0xe0 and `V17RXS_FSE` is 0x170, so
 *
 *     0x128 - 0xe0  = 0x48   struct fpm_sre::adapt
 *     0x1b4 - 0x170 = 0x44   struct fpm_fse::pll_on
 *     0x1b8 - 0x170 = 0x48   struct fpm_fse::tilt_on
 *     0x1bc - 0x170 = 0x4c   struct fpm_fse::lms_on
 *
 * -- four fields of two structs `fpm_sre.h` and `fpm_fse.h` model
 * independently, all four of which those headers describe as the CALLER's
 * enable for a stage of the loop.  `DemodDataV17` is that caller and this is
 * what it writes them with, so `src/fax/v17.c` reaches them as struct members
 * and not through these offsets.  The offsets stay for the test, which has to
 * find the same bytes without the struct.  Finding F9102.
 */
#define V17RXS_INT_0004		0x04
#define V17RXS_INT_0008		0x08
#define V17RXS_INT_0010		0x10
#define V17RXS_SRE_ADAPT	0x128	/* struct fpm_sre + 0x48             */
#define V17RXS_FSE_PLL_ON	0x1b4	/* struct fpm_fse + 0x44             */
#define V17RXS_FSE_TILT_ON	0x1b8	/* struct fpm_fse + 0x48             */
#define V17RXS_FSE_LMS_ON	0x1bc	/* struct fpm_fse + 0x4c             */

/*
 * The three state fields `V17RX_status` reports, and nothing else in this
 * batch touches any of them.  All three are NEUTRAL -- the function turns each
 * into one bit of a flags byte whose bits are themselves unnamed, so nothing
 * establishes what any of them indicates.
 *
 * The two ints are reported INVERTED (`sete` on a 32-bit test), the byte is
 * reported straight and only its bit 0 is read.
 *
 * `V17RXS_BYTE_001C` IS AN `int`, AND `V17RX_create` IS WHAT SETTLES IT.  The
 * name came from `V17RX_status`, which reads bit 0 through a `movzbl` -- an
 * unforced narrowing, since only one bit of the result survives.  The
 * constructor writes the field with `movl $0x1,0x1c(%edx)` at 0x097786, all
 * four bytes, exactly as it writes its six int neighbours.  `V17RXS_INT_001C`
 * is the field and the old name is kept as an ALIAS because `t_v17fax.c`
 * spells it that way; `V17RXS_001C_BIT0` is still the bit the reader takes.
 * Finding F9474.
 */
#define V17RXS_INT_0000		0x00
#define V17RXS_INT_001C		0x1c
#define V17RXS_BYTE_001C	V17RXS_INT_001C
#define V17RXS_001C_BIT0	0x01

/*
 * THE REST OF THE HEAD, AND EVERY ONE OF THEM IS `V17RX_create`'s ALONE.
 *
 * The constructor writes eleven fields at 0x097730..0x09779b and nothing
 * reconstructed reads any of the six below.  All six are NEUTRAL: what is
 * established is the width, the constant and the writer, and nothing else.
 * The widths are the object's own -- five `movl` and one `movw`.
 *
 *     +0x0c  int             0
 *     +0x14  int             0
 *     +0x18  int             1
 *     +0x20  int             0
 *     +0x24  unsigned short  a copy of V17RXC_RATE_CODE
 *     +0x28  int             0
 *
 * `V17RXS_RATE_CODE` IS THE ONE THAT CARRIES A REAL NAME, and it is rank 2
 * rather than usage inference: the constructor loads `V17RXC_RATE_CODE`
 * `movzwl` at 0x097754 and stores its low half here, so the field IS that
 * code and the name says only that.  What a reader of it would do with it is
 * not established, because there is no reader.
 */
#define V17RXS_INT_000C		0x0c
#define V17RXS_INT_0014		0x14
#define V17RXS_INT_0018		0x18
#define V17RXS_INT_0020		0x20
#define V17RXS_RATE_CODE	0x24
#define V17RXS_INT_0028		0x28

/*
 * Two more pointers the demodulator state owns, both from `V17RX_delete` and
 * both reached by nothing else read here.
 *
 * `V17RXS_SGD` is `struct sgd *`, TYPED BY ITS CALLEE -- it is `SGD_delete`'s
 * only argument, which is CLAUDE.md's rank 2.  `V17RXS_PTR_0030` goes to
 * `sysdep_free` and so is neutral in every way except that it is a pointer the
 * state owns; `sysdep_free` types nothing.
 */
#define V17RXS_SGD		0x2c
#define V17RXS_PTR_0030		0x30

/*
 * The descrambler, `DescrambleDataV17`'s only subject.
 *
 * `struct fpm_sdm *`, typed by its callee: the function adds 0x4f8c to the
 * state pointer and tail-jumps to `SDM_descrambler`, whose first argument is
 * that type.
 *
 * AND IT TILES, WHICH IS A SECOND CONFIRMATION OF THE OFFSET.
 * `sizeof(struct fpm_sdm)` is 0x18 -- from `fpm_sdm.h`, derived from the SDM
 * functions and nothing to do with V.17 -- and 0x4f8c + 0x18 is 0x4fa4, which
 * is `V17RXS_BUF_MRF` exactly.  The equaliser ends at 0x170 + 0x4e18 = 0x4f88,
 * so the four bytes at 0x4f88 are the only gap in the whole tail and they are
 * BELOW this object, not above it.
 *
 * THE SCRAMBLER IS NOT HERE.  `ScrambleDataV17` reaches `V17FP_SDM` in the
 * TRANSMITTER's private block instead, which is the two-instances split of
 * F8850 showing up a third time.
 */
#define V17RXS_SDM		0x4f8c

/*
 * The recoverer's output bound, from the object's own `cmp $0xa4` / `jbe`, and
 * the count `DemodDataV17` reports through the author's own
 * "ERROR: SRE buffer violation!(%d)" when it is EXCEEDED.
 *
 * IT IS THE BUFFER SIZE, AND THAT IS NOW CONFIRMED RATHER THAN BOUNDED.
 * `V17RX_create` allocates `V17RXS_BUF_SRE` with `sysdep_malloc(0x148)` at
 * 0x0978d1 -- 328 bytes, exactly 0xa4 `short` -- so the guard and the
 * allocation are the same number read two ways.  The constructor then zeroes
 * only the first 160 of the 164, which is deviation D1225.  Finding F9472.
 */
#define V17RXS_SRE_MAX		0xa4

/*
 * The four FPM objects the receive chain runs, every one typed by the
 * function it is handed to: `DemodDataV17` computes each of these four
 * addresses and passes it to exactly one module, which is CLAUDE.md's rank-2
 * evidence.
 *
 * AND THE FOUR TILE THE BLOCK EXACTLY, WHICH IS A SECOND, INDEPENDENT
 * CONFIRMATION OF ALL FOUR OFFSETS AT ONCE.  Every boundary is the previous
 * object's own `sizeof`, taken from the four headers that model them and not
 * from anything read here:
 *
 *     0x098 + sizeof(struct fpm_mrf) 0x001c = 0x0b4   V17RXS_AGC
 *     0x0b4 + sizeof(struct fpm_agc) 0x002c = 0x0e0   V17RXS_SRE
 *     0x0e0 + sizeof(struct fpm_sre) 0x0090 = 0x170   V17RXS_FSE
 *     0x170 + sizeof(struct fpm_fse) 0x4e18 = 0x4f88  (below the buffers)
 *
 * Four `add $imm` in one function agreeing with four independently derived
 * struct sizes, with no gap anywhere, is not a coincidence that survives one
 * offset being wrong.  It also explains the block's size: the equaliser alone
 * is 19.5 KB of it.  Finding F8854.
 */
#define V17RXS_MRF		0x98	/* struct fpm_mrf  -> FPM_MRF_filter  */
#define V17RXS_AGC		0xb4	/* struct fpm_agc  -> FPM_AGC_agc     */
#define V17RXS_SRE		0xe0	/* struct fpm_sre  -> FPM_SRE_recover */
#define V17RXS_FSE		0x170	/* struct fpm_fse  -> FPM_FSE_receive */

/*
 * The two halves of the carrier verdict, ANDed together by all three of
 * `CarrierDetectV17`, `QualityDetectV17` and `DataCarrierDetectV17`.
 *
 * THE FIRST HALF IS THE AGC's OWN `signal` FLAG, and the tiling above is what
 * says so: `V17RXS_AGC` is 0xb4, `offsetof(struct fpm_agc, signal)` is 0x1c,
 * and 0xb4 + 0x1c is 0xd0.  `fpm_agc.h` describes that field as "more than
 * half the blocks in the last call were above the gate", which is exactly
 * what a carrier verdict wants, and `FPM_AGC_agc` is the only writer.  So
 * this is not an unnamed offset that happens to be read three times; it is a
 * field of a modelled struct, reached the long way round because the
 * enclosing block is not modelled.  Finding F8854.
 *
 * THE WIDTH IS NOT THE SAME AT EVERY SITE, and that is the object's and not
 * ours.  `CarrierDetectV17` reads it with a 32-bit `mov`, which is what the
 * `int` field calls for; `QualityDetectV17` and `DataCarrierDetectV17` read
 * it with `movswl`.  Both instructions were forced -- an `int` cannot produce
 * `movswl` and a `short` cannot produce a 32-bit load -- so the three
 * functions did not share a declaration, and `src/fax/v17.c` spells each site
 * the way the object does rather than picking one.
 *
 * IT IS OBSERVABLE IN `CarrierDetectV17` ALONE.  The other two narrow the AND
 * back to a `short`, and `(short)(x & m)` depends on nothing above bit 15.
 * And on any state a real receiver can reach it is observable NOWHERE, since
 * `signal` only ever holds 0 or 1 -- so `t_v17fax.c` separates it with a
 * synthetic +0xd2, and says so.  Finding F8853.
 */
#define V17RXS_AGC_SIGNAL	(V17RXS_AGC + 0x1c)
#define V17RXS_INT_0120		0x120

/*
 * A short compared against 999, and required to EXCEED it, before either
 * carrier detector will look at the decoder error.  Neutral.
 */
#define V17RXS_SHORT_0094	0x94
#define V17RXS_0094_MIN		0x3e7

/*
 * The epoch flag, named from `EpochDetectV17`, which is the author's own
 * function name and does nothing except report this int as 0 or 1.  Both
 * carrier detectors require it non-zero on the same path.
 */
#define V17RXS_EPOCH		0x1b0

/* The rate, from the Store/Restore pair above.  An `int` in the state. */
#define V17RXS_RATE		0x1ac

/*
 * The decoder error metric.  Named from the two format strings above, which
 * are the only two sites that report it and are both gated on it exceeding
 * 0x3fff.  `GetSNRV17` reads the same field.
 *
 * IT IS READ UNSIGNED IN ONE PLACE.  `GetSNRV17` loads it `movzwl` where
 * every other site loads it as a signed short or compares it 16-bit.  The
 * extension is FREE there -- the difference is 65536 and the result is
 * truncated back to a short -- so it follows the declared type of the local
 * and is reproduced rather than reasoned about.
 */
#define V17RXS_DEC_ERROR	0x1c2
#define V17RXS_DEC_ERROR_MAX	0x3fff

/*
 * The OTHER threshold on the same field, and the author names this one too.
 *
 * `RxHdxIdleV17` compares it `cmpw $0x1fff,0x1c2(%ebx)` / `jle` at 0x0a0462 --
 * SIGNED and 16 bits wide -- and on the at-or-below arm it advances the machine
 * and prints "Decision error is small back to DATA mode !!!"
 * (`.rodata.str1.4` 0x12b6c).  So "small" is the author's word for this side of
 * this constant, exactly as "too big" is his word for the other side of
 * `V17RXS_DEC_ERROR_MAX`.  The two are not independent: 0x1fff is one less than
 * half 0x3fff + 1, which is recorded as an observation and NOT used to name
 * anything.
 */
#define V17RXS_DEC_ERROR_SMALL	0x1fff

/*
 * `Restore_rateV17`'s other pair: it writes the short at +0x1f8 with the
 * unsigned short at +0x18c plus five.  Both neutral -- nothing else read in
 * this batch touches either.
 */
#define V17RXS_USHORT_018C	0x18c
#define V17RXS_SHORT_01F8	0x1f8

/*
 * The two coefficient arrays `StoreCoefV17` reads, and how many entries it
 * takes from each.  The count is the object's loop bound, `cmp $0x30` with
 * `jbe`, so 49 and not 48.
 */
#define V17RXS_COEF0		0x1d0
#define V17RXS_COEF1		0x1d4
#define V17_COEF_N		49

/*
 * `DemodDataV17`'s two intermediate buffers, chained resampler -> recoverer
 * -> equaliser.  `+0x4fa4` is the resampler's output and the recoverer's
 * input; `+0x4fa8` is the recoverer's output and the equaliser's input.  Both
 * are typed by the callees at each end.
 */
#define V17RXS_BUF_MRF		0x4fa4
#define V17RXS_BUF_SRE		0x4fa8

/*
 * The energy-drop watchdog `DataCarrierDetectV17` runs, gated on the short at
 * +0x4fb4.
 *
 * The reference level at +0x4fb6 is refreshed every SECOND block, and the
 * block RMS is required to stay at or above 0x32fe/32768 of it -- 0.3984,
 * which is -8.00 dB, and the message says "8[dB]".  So the constant and the
 * author's own text agree, and the naming here rests on that agreement rather
 * than on the arithmetic alone.
 */
#define V17RXS_SHORT_4FB4	0x4fb4
#define V17RXS_RMS_REF		0x4fb6
#define V17RXS_RMS_PHASE	0x4fb8
#define V17RXS_RMS_DROP_Q15	0x32fe
#define V17RXS_RMS_PERIOD	2

/*
 * `QualityDetectV17`'s block of four shorts.
 *
 * +0x4fac is a smoothed decoder error: on the first block it is seeded with
 * the raw value, and on each block after it is
 * `0.1 * error + 0.9 * average` in Q15 with round-to-nearest, which are the
 * two constants 0xccd and 0x7333.  +0x4fae counts blocks.  On block 0x32 the
 * average is compared against +0x4fb0 and +0x4fb2 is latched to 1 if the
 * average has NOT stayed above it.
 *
 * THE THRESHOLD AND THE LATCH ARE NEUTRAL because nothing traced reads them
 * back; only the direction of the test is established.
 */
#define V17RXS_QAVG		0x4fac
#define V17RXS_QCOUNT		0x4fae
#define V17RXS_SHORT_4FB0	0x4fb0
#define V17RXS_SHORT_4FB2	0x4fb2
#define V17RXS_QWEIGHT_NEW	0xccd	/* 3277/32768  = 0.1000            */
#define V17RXS_QWEIGHT_OLD	0x7333	/* 29491/32768 = 0.8999            */
#define V17RXS_QROUND		0x4000	/* 0.5 before the >> 15            */
#define V17RXS_QCOUNT_SETTLE	0x31	/* the last block that only smooths */
#define V17RXS_QCOUNT_JUDGE	0x32	/* the one block that judges        */

/*
 * `QualityDetectV17`'s own return values, from the object: it returns the
 * carrier verdict unchanged, except that a ZERO verdict is reported as 2 and
 * annotated with the "unreliable data" message.
 */
#define V17_QUALITY_UNRELIABLE	2

/* ------------------------------------------------------------------------ */
/* The dispatch slot's type                                                 */

/*
 * `V17RX_modem`'s inner call, spelled from the object's own argument set:
 * four slots written, the instance first and the caller's three arguments
 * unchanged after it.  The result is sign-extended with `cwtl` before it is
 * added to the running total, so it is `short` and that is forced.
 */
typedef short (*v17rx_process_fn)(void *modem, short *in, short *out,
				  unsigned short *count);

/*
 * `V17TX_modem`'s inner call, and it is NOT the same signature.
 *
 * Four slots written, as on the receive side, and the first three are the
 * instance and the caller's two buffers unchanged -- but the FOURTH is
 * `lea 0x1a(%esp)`, the address of a `short` LOCAL, not the caller's count.
 * So the transmitter's slot is handed a per-call budget the caller never sees,
 * and the caller's `count` is read once before the loop and written once
 * after it.
 *
 * `in` IS NOT ADVANCED between iterations and `out` IS.  Both are the
 * object's: `0x34(%esp)` is reloaded unchanged every time round, while the
 * output pointer accumulates `2 * got`.  The result is sign-extended with
 * `cwtl` before it is added to the running total, so it is `short` and that is
 * forced.
 *
 * `in` is `unsigned short *` because `FIFO_write` -- which the other arm hands
 * the very same pointer to -- declares its source that way.  Rank 2, a callee
 * that types it, and not usage inference.
 */
typedef short (*v17tx_process_fn)(void *modem, unsigned short *in, short *out,
				  short *budget);

/* ------------------------------------------------------------------------ */
/* The functions                                                            */

/*
 * Drive the receiver until it stops consuming, and report what the dispatch
 * slot left in the instance.
 *
 * `count` is IN/OUT and changes meaning across the call: on entry it is the
 * number of input samples, on return the total number of outputs produced.
 * The loop is the object's own -- it re-reads `*count` after each inner call,
 * advances `in` by what was consumed and `out` by what was returned, and
 * stops when `*count` reaches zero.
 *
 * THE RUNNING TOTAL IS A `short`, AND THAT IS FORCED: the object sign-extends
 * it with `cwtl` on every iteration, so a total above 32767 wraps.  It is
 * reproduced, not corrected.
 *
 * The one thing the loop does before any of that is clear
 * `V17RX_FLAG_ERROR` in the word it will later return.
 */
int V17RX_modem(void *modem, short *in, short *out, unsigned short *count);

/*
 * The ERROR state: raise `V17RX_FLAG_ERROR`, run the block through the
 * demodulator anyway so the filters keep their history, and consume it.
 *
 * Nothing here advances the state, so the machine stays in it until something
 * outside re-installs another handler.  The flag is a one-shot -- `V17RX_modem`
 * clears it at the top of every block.  `RxHdxErrorV21` and `RxHdxErrorV29`
 * are the same eleven instructions over a different flag-byte offset.
 */
short RxHdxErrorV17(void *modem, short *in, short *out, unsigned short *count);

/*
 * The DATA state: demodulate while the carrier is up, descramble, and grade
 * what came out.
 *
 * IT IS NOT `RxHdxDataV21`'s SHAPE, and the differences are the object's: it
 * descrambles, it grades with `QualityDetectV17`, and it does NOT advance the
 * state on the carrier-gone arm -- where V.21's calls `RxNextStateV21`, this
 * one just clears `V17RX_FLAG_CARRIER` and returns.
 *
 * The carrier flag is raised UNCONDITIONALLY on entry and lowered again on the
 * deny arm, which is not the same as assigning it: a caller reading the byte
 * between two handlers in one block sees the raised bit.
 *
 * THE GATE IS TWO TERMS AND THE SECOND IS `V17RXC_INT_0008`, which nothing in
 * the object writes -- so the demodulating arm is reached only when something
 * outside has left that field zero.  The test plants it rather than reaching
 * it.
 *
 * THE RESULT IS `n` OR ZERO, and the object computes it branchlessly:
 * `cmp $0x2,%ax` / `setne` / `movzbl` / `neg` / `and`, which is
 * `n & -(quality != V17_QUALITY_UNRELIABLE)`.  It is written as the `?:` that
 * expression came from; see the derivation in `src/fax/v17.c`.
 */
short RxHdxDataV17(void *modem, short *in, short *out, unsigned short *count);

/*
 * Build the receive instance, its control block and its 20 KB demodulator
 * state, and return the instance.
 *
 * `modem` NULL allocates the 0x64-byte instance; a caller-supplied one is
 * reused, and so are its two sub-blocks if their pointers are non-NULL.
 * `params` NULL takes the library's `V17RX_CFG`.
 *
 * THE SECOND ARGUMENT IS `struct v17rx_cfg *` BECAUSE THE OBJECT COPIES ONE.
 * At 0x096ef5 and 0x097948 it copies forty bytes -- `sizeof(struct
 * v17rx_cfg)`, and the same forty `init_vmi_v17rx` allocates -- over the head
 * of the instance, as an interleaved load/store struct assignment.  Every
 * field this function then reads back lines up: +0x04 is `bit_rate` and is
 * `V17RX_OBJ_RX_BPS`, +0x18 and +0x1c are the two `sysdep_malloc(0x62)`
 * pointers and are `V17RX_OBJ_COEFSAVE0`/`_1`, +0x20 is the
 * `sysdep_malloc(2)` one and is `V17RX_OBJ_RATESAVE`.  So the instance's own
 * head IS that struct, and this signature is the object's rather than ours.
 * Finding F9470.
 *
 * IT CANNOT FAIL AND IT CANNOT REPORT FAILURE: eight allocations, none
 * checked, one `ret`, and the return is always the instance.  D1223.
 */
void *V17RX_create(void *modem, const struct v17rx_cfg *params);

/*
 * Tear the receive instance down.
 *
 * FIFTEEN RELEASES IN ONE FUNCTION, and the order is the object's: the
 * demodulator state's own sub-objects first (an `SGD`, a pointer at
 * `V17RXS_PTR_0030`, the equaliser, the recoverer, the resampler, the two
 * chained buffers and then the state block itself), then the control block's
 * (both tone detectors, the notch, both scratch buffers and then the control
 * block), and the instance last as a sibling `jmp`.
 *
 * THE LITERAL 1 IN THE SECOND ARGUMENT SLOT IS NOT REPRODUCED.  The object
 * plants one before `FPM_FSE_free`, `FPM_SRE_free` and `FPM_MRF_free`, all
 * three of which take a single argument and none of which reads a frame slot
 * past the first.  `V29RX_delete` and `B103FP_delete` carry the identical
 * pattern for the identical reason -- finding F8876 -- so there is nothing to
 * reproduce.
 */
void V17RX_delete(void *modem);

/*
 * Tear the transmit instance down.  Seven releases, in the object's order: the
 * shaper and the two things the private block owns, then the `SGD` and the
 * `fax_fifo` the block at `V17TX_OBJ_PARAMS` owns and that block itself, then
 * the instance as a sibling `jmp`.
 *
 * The object plants a literal 1 in the second argument slot before
 * `FPM_PPS_free`, which takes one argument and reads no frame slot past the
 * first.  Not reproduced, for F8876's reason and no other.
 */
void V17TX_delete(void *modem);

/*
 * Drive the transmitter for one caller block, and report what the instance's
 * result word says.
 *
 * TWO ARMS ON THE WAY IN, chosen by `V17TXP_INT_0008`.  Zero queues the
 * caller's `count` words through `FIFO_write` and remembers how many it took;
 * non-zero remembers `count` itself and touches the FIFO not at all.  What is
 * remembered is compared against `*count` AFTER the loop, and a mismatch is
 * what sets `V17TX_RESULT_B1_BIT1` and writes `V17TX_RESULT_BYTE_09` -- so on
 * the second arm the comparison is between a value and itself and neither is
 * ever written.  That is the object's, not a simplification.
 *
 * THE LOOP IS A `do`/`while` ON A LOCAL, NOT ON THE CALLER'S COUNT.  See
 * `v17tx_process_fn`: the budget starts at `V17TX_MODEM_BUDGET`, is set ONCE
 * before the loop, and the slot decrements it.  `count` is IN/OUT and changes
 * meaning across the call exactly as `V17RX_modem`'s does -- on entry the
 * number of input words, on return the total the slot produced -- and the
 * total is a `short` that wraps, which the object forces with `cwtl` on every
 * iteration.
 */
int V17TX_modem(void *modem, unsigned short *in, short *out,
		unsigned short *count);

/*
 * Fill a status block from the RECEIVE instance, or report that there was
 * nothing to fill.
 *
 * The same NULL guard and the same return convention as `V17TX_status`, and
 * the same block -- but a different write set and a different second half.
 * It writes +0x00, +0x02, +0x04, +0x06, +0x08, +0x0a, +0x0e, +0x10, +0x12,
 * +0x14 and +0x15, and leaves +0x0c, +0x16 and +0x18 alone; `V17TX_status`
 * writes +0x0c and +0x18 and skips +0x0e.  The two agree on skipping +0x16,
 * which is the field `v32_status` itself annotates "not written", so a third
 * derivation lands on the same gap.
 *
 * WHERE `V17TX_status` ASSIGNS `flags`, THIS BUILDS IT, one bit at a time,
 * over four stores.  Each store is separated from the next by a load of
 * `V17RX_OBJ_STATE` that may alias it, which is why the object emits four
 * rather than one -- see the derivation in `src/fax/v17.c`.  The result is
 * still deterministic, so the intermediate stores are observable only through
 * an aliasing caller and this batch does not claim them.
 */
int V17RX_status(void *modem, struct v17_status *status);

/*
 * Scramble `count` words in place, through the TRANSMITTER's scrambler.
 *
 * A two-instruction adapter and a tail `jmp` to `SDM_scrambler`: it replaces
 * its own first argument with `V17TX_OBJ_FP` + `V17FP_SDM` and re-writes its
 * third with the same value zero-extended, then falls into the callee.  The
 * data pointer is passed straight through.
 */
void ScrambleDataV17(void *modem, unsigned short *data, unsigned short count);

/*
 * The mirror image, through the RECEIVER's descrambler at `V17RXS_SDM`.
 *
 * NOT THE SAME OBJECT AS THE SCRAMBLER'S, and not on the same instance: this
 * one reaches `V17RX_OBJ_STATE` + 0x4f8c and the other reaches
 * `V17TX_OBJ_FP` + 0x1c.  See F8850 for why that is two instances and not one.
 */
void DescrambleDataV17(void *modem, unsigned short *data,
		       unsigned short count);

/*
 * One block through the receive chain: gain control, an optional tone
 * pre-pass, resample, recover the symbol timing, equalise and slice.
 *
 * THE PRE-PASS ABANDONS THE WHOLE CALL, exactly as `DemodDataV29`'s does.
 * While `V17RXC_SHORT_0018` is zero the input is copied into
 * `V17RXC_SCRATCH`, a tone is notched out of the copy and the tone detector is
 * asked whether it fired; if it did, the function returns zero without
 * touching the resampler, the recoverer or the equaliser.  The gain control
 * has already run over the CALLER's buffer by then and its effect stands.
 *
 * AND THE COPY DOES NOT HALVE.  `DemodDataV29`'s equivalent loop is
 * `buf[i] = in[i] >> 1`; this one is a plain 16-bit move, `movzwl` into `%bx`
 * and `mov %bx` out, with no shift anywhere in the block.  The two functions
 * are otherwise the same shape, which is exactly why this is written down.
 * Finding F9103.
 *
 * `signal` IS THE AGC's OWN FIELD, NOT ITS `%eax`.  The object calls
 * `FPM_AGC_agc` -- which is `void` -- and then uses `%eax`, which holds
 * `agc->signal` because that function's last store before its single `ret` is
 * to that field.  This is the same site shape as `DemodDataV29`'s; F8875 and
 * D1035 carry the argument, `t_v17fax.c` measures the identity rather than
 * believing it, and D1091 records it for this function.
 */
unsigned short DemodDataV17(void *modem, short *in, unsigned short *bits,
			    unsigned short count);

/*
 * Load the scrambler's shift register.  See `V17FP_SDM`: this is
 * `struct fpm_sdm::reg` in the transmitter's private block, and nothing else
 * in the function.
 */
void SeedScramblerV17(void *modem, unsigned int seed);

/*
 * Select one of the transmitter's three encoders, and for two of them set the
 * short at `V17FP_SMC_SHORT_06`.
 *
 * MODE 1 DOES NOT WRITE THE SECOND VALUE, and that asymmetry is the object's:
 * the arm for `V17_ENCODER_ABS` stores the selector and returns.  Any `which`
 * outside 0..2 does nothing at all -- there is no default arm, just a `ret`.
 *
 * Both arguments are `short` and both are FORCED.  `which` is loaded
 * `movswl` and its 32-bit result drives a signed comparison chain, which is
 * CLAUDE.md's forced case exactly; `arg` is loaded `movswl` and only `%cx` is
 * stored, so its extension is dead and follows the local's declared type.
 */
void SetEncoderV17(void *modem, short which, short arg);

/*
 * Fill a status block, or report that there was nothing to fill.
 *
 * Returns 1 when `status` is non-NULL and 0 when it is NULL, and the NULL
 * case does nothing else -- so this is the object's own guard, not ours.
 *
 * THE SECOND BLOCK IS `struct v17_status`, above.  THE FIRST IS NOT
 * IDENTIFIED: `params` is read at +0x00, +0x02, +0x10 and +0x18, and the two
 * candidates -- the transmitter's private block at `V17TX_OBJ_FP` and the
 * parameter block at `V17TX_OBJ_PARAMS` -- both have room for all four and
 * neither is contradicted.  `v22status.h` declares its equivalent as taking
 * the DATAPUMP (`struct v22fp *`), which is a hint and not a derivation, so
 * the parameter stays `void *`.
 *
 * It is worth recording that +0x10 of `params` is the same offset
 * `V17TX_create` takes a rate index from (`v17data.h`, `V17TX_OBJ_PARAMS`),
 * and that this function reads only the LOW BYTE of it and only bit 2;
 * whether the two are the same block is NOT established and the coincidence is
 * recorded rather than acted on.
 *
 * THE DEAD STORE IS THE OBJECT'S.  `status + 0x14` is cleared of its low two
 * bits and then overwritten outright a few instructions later.  It survives
 * in the object because the load of `params + 0x10` sits between the two and
 * may alias, and it survives here for the same reason -- both blocks are
 * `unsigned char *`.  Removing it would be tidier and would stop being the
 * object.  The consequence -- the caller's bits 2..7 of that byte are
 * DESTROYED, while +0x15 four instructions away carefully preserves its own --
 * is deviation D1032, and three sibling functions do the same thing.
 */
int V17TX_status(void *params, struct v17_status *status);

/*
 * Report whether a carrier is present.
 *
 * The verdict starts as the AND of the state's two carrier words and is
 * masked to bit 0, or forced to zero, only when the receiver is far enough
 * along to have a decoder error worth believing: the control block's
 * `V17RXC_INT_0010` set, the epoch found, and `V17RXS_SHORT_0094` past 999.
 *
 * RETURNS `int`, AND NOTHING NARROWS IT.  The verdict lives in a register the
 * object never truncates, so unlike `QualityDetectV17` there is no `short`
 * here to reproduce.
 */
int CarrierDetectV17(void *modem);

/*
 * Report the carrier verdict for a block of `count` samples, and run the two
 * watchdogs behind it.
 *
 * It has the SAME HEAD as `CarrierDetectV17` -- the same three gates, the same
 * two arms, the same doubled test of the decoder error and the same format
 * string -- but only while the control block's `V17RXC_SHORT_0020` is zero.
 * When it is not, the function takes a completely different path: it copies
 * the block into its own buffer, gain-controls it, runs the SECOND tone
 * detector over it, and accumulates the sample count for as long as that
 * detector answers `FPM_MTD_ABSENT` -- see `V17RXC_OFFBAND`, which is not the
 * silence timer it looks like.  Crossing `V17RXC_OFFBAND_MAX` prints "V17: V21
 * Carrier detected" and drops the verdict.
 *
 * The energy watchdog runs on BOTH paths and is the one the "-8 dB" string
 * belongs to; see `V17RXS_SHORT_4FB4`.
 *
 * `in` IS NOT MODIFIED.  The gain control runs over the private copy at
 * `V17RXC_BUF2`, not over the caller's block -- which is the one place a
 * careless reading of the two `FPM_AGC_agc` call sites in this file could go
 * wrong, since `DemodDataV17`'s runs over the caller's.
 */
short DataCarrierDetectV17(void *modem, const short *in, unsigned short count);

/*
 * Report the carrier verdict, and maintain the smoothed decoder error behind
 * it.
 *
 * The verdict is the same AND as `CarrierDetectV17`'s, except that +0xd0 is
 * read as a `short` here (see `V17RXS_00D0`) and the result is narrowed to a
 * `short`, which the object forces with `movswl %ax`.  A zero verdict is
 * reported as `V17_QUALITY_UNRELIABLE` and annotated.
 *
 * The maintenance runs on EVERY call and is what makes this function
 * stateful: block 0 seeds the average, blocks 1..0x31 smooth it, block 0x32
 * judges it once, and every block after 0x32 leaves it alone.
 */
short QualityDetectV17(void *modem);

/* Non-zero once the receiver has found its epoch.  0 or 1, never anything else. */
int EpochDetectV17(void *modem);

/*
 * `13 - error`, narrowed to a short.  See `V17RXS_DEC_ERROR` for why the load
 * is unsigned and why that does not change the answer.
 */
short GetSNRV17(void *modem);

/*
 * Copy the receiver's two coefficient arrays and its rate out into the
 * instance's three save pointers.  `V17_COEF_N` entries from each array.
 */
void StoreCoefV17(void *modem);

/*
 * Put the saved rate back, and recompute the short at `V17RXS_SHORT_01F8`
 * from `V17RXS_USHORT_018C`.  It is NOT the inverse of `StoreCoefV17` -- it
 * restores the rate and not the coefficients, which is what its name says.
 */
void Restore_rateV17(void *modem);

/* ------------------------------------------------------------------------ */
/* The half-duplex receive machine                                          */

/*
 * Advance the receive machine one state, and re-point the dispatch slot.
 *
 * A SEVEN-WAY SWITCH ON `V17RXC_STATE` WITH A COMPILER-GENERATED JUMP TABLE
 * (`.rodata` 0xc2d0), so there is no table to reproduce: the arms are 0..6 and
 * everything else -- including `V17RX_STATE_ERROR`, which has no arm of its own
 * -- lands on the default.  Each arm prints the name of the state it is
 * leaving, writes the next state, installs that state's handler in
 * `V17RXC_PROCESS` and seeds `V17RXC_COUNTDOWN`.
 *
 * WHAT EACH ARM DOES BESIDES THAT, because none of it is uniform:
 *
 *   START      -> EPOCH_DET, 5 blocks.
 *   EPOCH_DET  -> PROTOCOL.  Calls `Restore_rateV17` when `V17RXC_INT_0010` is
 *                 set and takes 1 block if it did and 62 if it did not, and
 *                 then STEPS THE GAIN CONTROL'S LEVEL SMOOTHER: `cfg.alpha`
 *                 and `cfg.beta` are both advanced by one `short`
 *                 (`addl $0x2` at 0x0a0200 and 0x0a0207), which takes
 *                 `AGCv17_CFG`'s pair from {0x4000, 0x4000} to
 *                 {0x7333, 0x0ccd} -- 0.5/0.5 to 0.9/0.1 in Q15, acquisition
 *                 to tracking.  See D1216 for what a second visit would do.
 *   PROTOCOL   -> SCRAM when `V17RXC_INT_0010` is set, else BRIDGE with a call
 *                 to `StoreCoefV17`.  Both take 1 block.  The SCRAM arm is the
 *                 one transition of the seven that does not touch
 *                 `V17RX_OBJ_RESULT_B2` (D1213).
 *   BRIDGE     -> SCRAM, 1 block.
 *   SCRAM      -> DATA, 0 blocks, after `FPM_AGC_Freeze`.
 *   DATA       -> IDLE, 0 blocks, and clears `V17RXC_INT_0008`.  UNREACHABLE
 *                 in the object: see D1210.
 *   IDLE       -> DATA, and it is the one arm that does NOT seed the countdown
 *                 (D1215).  It reports the rate ladder instead.
 *   default      Reports `V17RX_STATUS_DEFAULT` with the state as a `%d`,
 *                installs nothing and does not change the state.
 *
 * `V17RX_FLAG_DATA` is set by exactly the two arms that install
 * `RxHdxDataV17` and cleared by every other one, which is what names it.
 */
void RxNextStateV17(void *modem);

/*
 * The IDLE state: demodulate, drop the carrier flag, re-test it, and go back
 * to DATA once the decoder error is small again.
 *
 * `DemodDataV17`'s RETURN IS DISCARDED and the function returns a literal zero
 * on every path, so a block spent here produces no output words at all even
 * though the equaliser ran.  The carrier flag is CLEARED and then set again if
 * and only if `CarrierDetectV17` agrees, which is the same clear-then-set
 * `RxHdxStartV17` does and is not what the three training handlers do.
 *
 * THE SECOND TEST RE-READS THE FLAG BYTE FROM MEMORY (`testb $0x20,0x29` at
 * 0x0a0459) rather than reusing the verdict, and that is the compiler's, not a
 * second question: the byte was just stored through a character type.
 */
short RxHdxIdleV17(void *modem, short *in, short *out, unsigned short *count);

/*
 * The three TRAINING states, which are one function compiled three times and
 * then one of the three with a tail on it.
 *
 * `RxHdxBridgeV17` AND `RxHdxPrtcolV17` ARE BYTE-FOR-BYTE IDENTICAL -- 210
 * bytes each, the same six relocation targets in the same order, and a
 * byte-by-byte compare of 0x0a05b0 and 0x0a0690 reports no differing offset at
 * all.  They are one body the author wrote twice or a macro he expanded twice;
 * nothing in the object distinguishes them and this header does not pretend
 * otherwise.  Finding F9444.
 *
 * `RxHdxScramV17` IS THE SAME BODY PLUS ONE THING: on the expiry path, before
 * the SNR test, it reads `V17RXC_RATE_CODE` and reports the matching
 * `V17RX_STATUS_RATE_*` (0x0a0551..0x0a05a8).  The other two leave the status
 * byte at `V17RX_STATUS_CARRIER`.  That is the ONLY difference and it is the
 * only thing a test can use to tell the three apart, which is why
 * `t_v17rxstate.c` drives a Bridge and a Prtcol expiry with a rate code Scram
 * would have reacted to.
 *
 * All three: demodulate, descramble, consume the block, and then either
 * install `RxHdxErrorV17` because the carrier has gone or count one block off
 * `V17RXC_COUNTDOWN`.  THE RETURN IS `n` ONLY ON THE TRANSITION.  Every other
 * path -- carrier lost, and countdown not yet expired -- returns a literal
 * zero, so the words the descrambler just wrote are reported to `V17RX_modem`
 * as none unless this was the last block of the state.  That is the object's
 * (`xor %eax,%eax` at 0x0a051b against `movswl %bp,%eax` at 0x0a0578) and it
 * is deviation D1214.
 *
 * `V17RX_FLAG_LOW_SNR` is SET here and never cleared; only `RxHdxDataV17`
 * clears it.  D1217.
 */
short RxHdxScramV17(void *modem, short *in, short *out, unsigned short *count);
short RxHdxBridgeV17(void *modem, short *in, short *out, unsigned short *count);
short RxHdxPrtcolV17(void *modem, short *in, short *out, unsigned short *count);

/*
 * The EPOCH_DET state: demodulate and wait for the epoch, or for the clock.
 *
 * The same head as the three above -- demodulate, consume, carrier or error --
 * and then a SHORT-CIRCUIT OR that the object makes visible: `jle` at 0x0a07c4
 * jumps PAST the `EpochDetectV17` call at 0x0a07c9, so an expired countdown
 * advances the machine WITHOUT asking whether the epoch was found.  It does not
 * descramble, it discards `DemodDataV17`'s return, it never touches
 * `V17RX_FLAG_LOW_SNR` and it returns zero on every path.
 *
 * `EpochDetectV17`'S RETURN IS TESTED SIXTEEN BITS WIDE HERE (`test %ax,%ax` at
 * 0x0a07ce) where `CarrierDetectV17`'s is tested at thirty-two, so this
 * translation unit's prototype for it returned a `short`.  The two readings
 * agree over 0 and 1, which is all that function can return, and this header's
 * single `int` declaration is kept rather than split.  Finding F9445.
 */
short RxHdxEpochDetV17(void *modem, short *in, short *out,
		       unsigned short *count);

/*
 * The START state, which is what `V17RX_create` installs (0x097083).
 *
 * THE ONLY HANDLER WITH NO ERROR ARM -- three relocations where the others have
 * five or six.  It clears the carrier flag, reports `V17RX_STATUS_START`,
 * demodulates the block and discards the result, and advances the machine if
 * and only if a carrier appeared.  Nothing here can reach `RxHdxErrorV17`, so a
 * receiver that never hears a carrier sits in START for ever.
 *
 * `*count = 0` IS EMITTED TWICE (0x0a0850 and 0x0a086c) and is one statement:
 * the compiler tail-duplicated the trailing store into both arms of the
 * carrier test.
 */
short RxHdxStartV17(void *modem, short *in, short *out, unsigned short *count);

#endif /* DSPLIB_V17FAX_H */

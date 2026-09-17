#include "dsplib/period_byte_layout.h"
/**
 * @file v17fax.h
 * @brief ITU-T V.17 (fax): the receiver's primitives, and three
 *        transmit-side setters that sit beside them.
 *
 * `include/dsplib/v17data.h` already carries the V.17 transmitter's two
 * data leaves and the offsets inside the transmitter instance.  This
 * header is the other side: the functions the fax state machine calls to
 * drive and interrogate the V.17 receiver, plus `SeedScramblerV17`,
 * `SetEncoderV17` and `V17TX_status`, which are transmit-side and are
 * here because they are the remainder of the same batch rather than
 * because they belong with the receiver.  `tools/service.py` puts every
 * one of them on the FAX side.
 *
 * The two instances are not modelled, and there are two of them.  This
 * follows `v17data.h`'s ruling verbatim: `V17RX_create` (3,201 bytes) and
 * `V17TX_create` (1,043) are not reconstructed, so naming their fields
 * would be guessing.  Every parameter is `void *` and every offset is a
 * named constant below with the evidence beside it.
 *
 * That there are two instance types, and not one, is measured rather
 * than assumed, from a contradiction that only two types resolve:
 * `SeedScramblerV17` and `SetEncoderV17` dereference `obj + 0x28` as a
 * pointer and write through it (the transmitter's private block,
 * `V17TX_OBJ_FP`), while `V17RX_modem` clears bit 1 of the byte at
 * `obj + 0x29` -- inside those same four bytes -- and then returns the
 * int at `obj + 0x28`.  Clearing a bit of a live pointer and then
 * returning it is not something one object can survive, so the two
 * functions are not looking at the same struct; the split is
 * corroborated by every other offset the transmit and receive functions
 * touch.  Finding F8850.
 *
 * The author's own words: five format strings, the strongest evidence
 * class this tree recognises, name what the fields below are for --
 * "ERROR: SRE buffer violation!(%d)", "V17 Decoder error too big... no
 * carrier", "sudden energy drop > 8[dB], no carrier", "V17 Dec error too
 * big... unreliable data" and "V17: V21 Carrier detected".  The "no
 * carrier" message is referenced from both `CarrierDetectV17` and
 * `DataCarrierDetectV17`, and in both it is reached only when the short
 * at receiver state + 0x1c2 exceeds 0x3fff -- so that field is the
 * decoder error the message names, and `V17RXS_DEC_ERROR` is the
 * author's word and not a guess.  `GetSNRV17` returning `13 - error`
 * from the same field is a third, independent use.
 */

#ifndef DSPLIB_V17FAX_H
#define DSPLIB_V17FAX_H

#include "dsplib/v17data.h"	/* V17TX_OBJ_FP, V17FP_SMC, V17FP_ENCODER_SEL */
#include "dsplib/faxcfg.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/v17dec.h"

struct fax_fifo;
struct fpm_mtd;
struct fpm_pps;
struct fpm_sdm;
struct fpm_tone;
struct sgd;
struct v17rx_cfg;	/* faxcfg.h -- and the receive instance's own head */

/*
 * `V17RX_control`'s second argument.  Reads four fields and nothing past
 * +0x10, so the struct stops there -- the same "read what the loads
 * force, no further" rule `v22ctl.h`'s `struct v22fp_ctl` states for the
 * identical shape.  Nothing in the object calls `V17RX_control` directly:
 * its only referrer is the lowercase adapter `v17rx_control`
 * (`faxadapt.c`), which forwards this argument unchanged from its own
 * caller, so nothing reconstructed corroborates the type further than
 * these four loads do.
 *
 * `int_0004` and `short_train` are `int`, both loaded and stored whole
 * (`mov`/`mov`, no narrowing).  `flags_0c` and `flags_0d` are `unsigned
 * char`, each read once with `movzbl` and tested bit by bit.
 *
 * `short_train` is named by its destination (rank 2): `V17RX_control`
 * copies it into `cfg->short_train` on the REINIT path, and that field is
 * what `V17RX_create` hands to the decoder as `v17_dec::short_train`.
 * `int_0004` has the same shape but its destination `cfg->int_0008` is
 * read by nothing, so it stays an offset.  Batch 25.
 */
struct v17rx_ctl {
	unsigned char	unmapped_0000[0x04];
	/* +0x04 -> cfg->int_0008, which nothing reads back; retained
	 * neutral (Batch 25). */
	int		int_0004;
	unsigned char	unmapped_0008[0x04];
	unsigned char	flags_0c;	/* +0x0c                             */
	unsigned char	flags_0d;	/* +0x0d                             */
	unsigned char	unmapped_000e[0x02];
	/* +0x10 -> cfg->short_train, REINIT only; rank 2 (Batch 25). */
	int		short_train;
};

/*
 * `flags_0c`.  Both bits clear a field of the demodulator state
 * (`V17RXS_INT_0000`/`V17RXS_INT_0010`) and neither pairs with a reader that
 * would type its MEANING, so the names state the forced effect and nothing
 * more -- the same discipline `v22ctl.h` uses for its own unmodelled byte.
 */
#define V17RXCTL_CLEAR_STATE0		(1 << 3)	/* 0x08 */
#define V17RXCTL_CLEAR_STATE10		(1 << 5)	/* 0x20 */

/*
 * `flags_0d`.  Bit 1 gates a call back into `V17RX_create(modem, modem)` --
 * the self-referential reinit `V17RX_create`'s own header documents (finding
 * F9470: the instance's head IS a `struct v17rx_cfg`, so passing the handle
 * as its own `params` re-copies its current configuration onto itself and
 * reruns construction).  Bit 4 sets `V17RXC_INT_0008`; nothing pairs that
 * field with a reader either, so the name states only which bit reaches it.
 */
#define V17RXCTL_SET_CTL_INT_0008	(1 << 4)	/* 0x10 */
#define V17RXCTL_REINIT			(1 << 1)	/* 0x02 */

/* ------------------------------------------------------------------------ */
/* The status block                                                         */

/*
 * What `V17TX_status` fills, and it is a shared block: `v22status.h`'s
 * `struct v22_status` and `v32fpstat.h`'s `struct v32_status` model the
 * same layout for their own datapumps, field for field, both derived from
 * their own disassembly with nothing to do with V.17.  The V.17 write set
 * matches that layout exactly, including what it skips: `V17TX_status`
 * writes +0x00, +0x02, +0x04, +0x06, +0x08, +0x0a, +0x0c, +0x10, +0x12,
 * +0x14, +0x15 and +0x18, and leaves +0x0e and +0x16 alone -- and +0x16 is
 * the one field `v32_status` itself annotates "not written".
 *
 * +0x02 is a bit rate, measured here rather than borrowed: `V21TX_status`
 * stores the literal 300 into +0x02 and V.21 is a 300 bit/s modem;
 * `v22_status` independently calls the same offset 1200-or-2400 and
 * `v32_status` calls it `RATEv32[...] bit/s`.  Three modules, three
 * derivations, one meaning.
 *
 * Three spellings of one block is a problem and this adds a fourth,
 * knowingly: "one type, one home" is about a type having one definition,
 * and these are four differently-named types, so no gate fires -- but
 * they are one thing and should end up as one.  Unifying them is not this
 * batch's to do, since it touches two headers this batch does not own and
 * a third module's tests.  The names below are deliberately the ones
 * `v22_status` and `v32_status` already use wherever the two agree, so
 * that a later unification is a rename and not a re-derivation.  Where
 * they disagree (+0x0c, +0x10, +0x12) the neutral offset name is kept,
 * because V.17 writes a constant zero to every one of them and so has no
 * evidence of its own to break the tie.
 *
 * The field widths are the object's: +0x14 and +0x15 are bytes, and +0x18
 * is an `int` copied 32 bits at a time.
 */
struct v17_status {
	short protocol;		/* +0x00 <- params + 0x00                    */
	short tx_bps;		/* +0x02 <- params + 0x02; see above         */
	short rx_bps;		/* +0x04 always 0 here                       */
	/*
	 * `V17RX_status` establishes it: the inverse of `V17RX_FLAG_LOW_SNR`.
	 * `V17TX_status` writes a constant zero here, having no SNR of its
	 * own to report.  Finding F10144.
	 */
	short snr_ok;		/* +0x06 <- !V17RX_FLAG_LOW_SNR; 0 from TX   */
	/*
	 * `V17RX_status` establishes it: `GetSNRV17`'s return.  `v32_status`
	 * independently calls the same offset `snr`.  Finding F9100.
	 */
	short snr;		/* +0x08 <- GetSNRV17; 0 from V17TX_status  */
	/* +0x0a: written 0 by both fillers and read by nothing, and
	 * `v22_status` leaves this same offset unnamed -- retained neutral
	 * (Batch 28). */
	short short_0a;		/* +0x0a always 0 here                       */
	/* +0x0c: written 0 by `V17TX_status` alone and read by nothing,
	 * `v22_status` leaves the offset unmodelled and `v32_status` names
	 * its own `r0c` from a source V.17 does not have -- retained neutral
	 * (Batch 28). */
	short short_0c;		/* +0x0c always 0 here                       */
	short short_0e;		/* +0x0e NOT WRITTEN -- the object steps over
				 *       it, and v32_status zeroes it        */
	short short_10;		/* +0x10 <- params + 0x02, read a second time */
	/* +0x12: `V17TX_status` writes 0 and `V17RX_status` writes the
	 * receive bit rate; `v22_status` leaves it unnamed and `v32_status`'s
	 * `r12` comes from an unrelated source -- two modules disagree, so
	 * retained neutral (Batch 28). */
	short short_12;		/* +0x12 always 0 here                       */
	unsigned char flags;	/* +0x14 ASSIGNED, not merged; see D1032     */
	unsigned char flags1;	/* +0x15 bit 0 cleared, bits 1..7 preserved  */
	short short_16;		/* +0x16 NOT WRITTEN                         */
	int int_18;		/* +0x18 <- params + 0x18                    */
};

/*
 * The one bit of `params + 0x10` that reaches `flags`.  Named by its
 * value per CLAUDE.md; what it indicates is not established, and neither
 * `v22_status` nor `v32_status` names their equivalent either.
 *
 * `params` is the top-level TX handle (settled by `V17TX_control`), so
 * this is the same bit `V17TX_control`'s own `ctl0` bit 2 sets in `struct
 * v17tx_cfg::int_0010`'s low byte -- a writer for a bit this header had
 * previously recorded only as read.  Finding F10107.
 */
#define V17_STATUS_FLAG_04	0x04

/* The two bits `flags` is masked of before being overwritten anyway. */
#define V17_STATUS_FLAGS_CLEAR	0x03

/* The one bit `flags1` is cleared of, and which is genuinely a mask. */
#define V17_STATUS_FLAGS1_CLEAR	0x01

/*
 * The rest of `flags`, from `V17RX_status`, which is the only function in
 * this batch that builds the whole byte rather than assigning it.
 *
 * Named by value and nothing more, per CLAUDE.md.  What each bit
 * indicates is not established: three of the six are read-modify-writes
 * of state fields whose own meaning is unknown (`V17RXS_BYTE_001C` bit 0,
 * and whether `V17RXS_INT_0000` and `V17RXS_INT_0010` are zero), and the
 * other three -- 0x10 set, 0x40 set, 0x80 cleared -- are unconditional
 * constants with nothing behind them to name.  `V17_STATUS_FLAG_04` above
 * is the one bit both this function and `V17TX_status` touch, and even
 * there the two disagree about what it should end up as.
 *
 * The whole byte is deterministic, and that is measured rather than
 * deduced from the masks looking exhaustive: the object's chain leaves
 * `0x50 | bit1 | bit3 | bit5`, so the caller's incoming bits 4 and 6
 * survive only by being re-set and every other incoming bit is
 * overwritten.  See the derivation in `src/fax/v17.c`.  Finding F9101.
 */
#define V17_STATUS_FLAG_01	0x01
#define V17_STATUS_FLAG_02	0x02
#define V17_STATUS_FLAG_08	0x08
#define V17_STATUS_FLAG_10	0x10
#define V17_STATUS_FLAG_20	0x20
#define V17_STATUS_FLAG_40	0x40
#define V17_STATUS_FLAG_80	0x80

/* ------------------------------------------------------------------------ */
/* The transmit instance -- offsets shared with v17data.h                    */

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
 * The short `SetEncoderV17` writes for encoder modes 0 and 2 and not for
 * mode 1.  Neutral name, deliberately: it is `V17FP_SMC` + 6, inside the
 * SMCv17 coder state, which nothing reconstructed models.  What is
 * established is only that the differential and trellis encoders are
 * given this value and the absolute encoder is not; what it means is not.
 */
#define V17FP_SMC_SHORT_06	0x3a

/*
 * A pointer the private block owns, released by `V17TX_delete` between
 * the shaper and the block itself.  Neutral: `sysdep_free` types nothing.
 */
#define V17FP_PTR_0010		0x10

/*
 * The int `V17TX_modem` returns, and the flag byte inside it.
 *
 * The same shape as the receiver's `V17RX_OBJ_RESULT`/`_B1` pair, byte for
 * byte: the object clears one bit of the byte at +0x21 on entry, sets
 * that same bit and stores a literal 9 into the byte at +0x20 on one
 * condition, and returns the int at +0x20 -- so the function both
 * modifies and returns the same word, exactly what `V17RX_modem` does
 * 0x28 into the other instance.
 *
 * Neutral, like its receive-side twin: what the bit indicates and what
 * the 9 means are not established, named by value per CLAUDE.md.  The
 * condition IS established: the flag and the 9 are written only when the
 * word count the caller asked for differs from what `FIFO_write`
 * accepted, reporting a transmit queue that would not take the whole
 * block.  On the non-FIFO arm the two are equal by construction and
 * neither is ever written.
 */
#define V17TX_OBJ_RESULT	0x20
#define V17TX_OBJ_RESULT_B1	0x21
#define V17TX_RESULT_B1_BIT1	0x02
#define V17TX_RESULT_BYTE_09	9

/*
 * `SetTxModeV17`'s own unrecognised-`mode` arm writes the same two fields
 * -- `V17TX_OBJ_RESULT` gets a literal 7 and `V17TX_OBJ_RESULT_B1` gets
 * `V17TX_RESULT_B1_BIT1` set -- but also clears bit 0 of `_B1`, which
 * `V17TX_modem`'s arm never touches.  Same shape, different literal, one
 * more bit: named separately rather than reusing `V17TX_RESULT_BYTE_09`
 * and pretending the two arms agree on the value.
 */
#define V17TX_RESULT_BYTE_07	7

/* ------------------------------------------------------------------------ */
/* Inside the block at V17TX_OBJ_PARAMS                                     */

/*
 * `v17data.h` describes this block as "a parameter block the instance
 * points at rather than owns", which is what `V17TX_create` alone could
 * say.  `V17TX_delete` settles the ownership the other way: it releases
 * the block's `SGD`, its `fax_fifo` and then the block itself, so the
 * transmitter owns it.  The name in `v17data.h` is left alone -- renaming
 * it is a change to a header this batch does not own -- and the
 * correction is recorded here and in finding F9106 rather than by two
 * headers disagreeing.
 *
 * `V17TXP_FIFO` and `V17TXP_SGD` are typed by their callees (rank 2):
 * +0x00 is `FIFO_write`'s and `FIFO_delete`'s first argument and +0x04 is
 * `SGD_delete`'s.  The other two are neutral -- +0x08 is an int
 * `V17TX_modem` tests for zero to choose between queueing the caller's
 * block and passing it straight through, and +0x14 is a dispatch slot
 * planted at construction (the call carries no relocation, so nothing
 * here can say which function lands in it).  +0x08 is the field
 * `v17tx_priv::r08`, which Batch 28 retained neutral on that ground.
 */
#define V17TXP_FIFO		0x00
#define V17TXP_SGD		0x04
#define V17TXP_INT_0008		0x08
#define V17TXP_PROCESS		0x14

/*
 * What `V17TX_modem` initialises its inner loop's budget to, once, before
 * the loop rather than per iteration.  The dispatch slot decrements it,
 * and the loop runs while it is strictly positive as a signed short, so a
 * slot that overshot into negative territory stops the loop rather than
 * wrapping it.
 */
#define V17TX_MODEM_BUDGET	0x30

/*
 * The three values `SetEncoderV17` will accept, which is what bounds the
 * selector `v17data.h` describes.  That header records that nothing it
 * read writes `V17FP_ENCODER_SEL` and that nothing bounds the index; this
 * function is the writer, and it writes 0, 1 or 2 and ignores every other
 * argument.  The order matches `V17TX_create`'s table exactly -- dif at
 * fp + 0x80, abs at fp + 0x84, tcm at fp + 0x88 -- so the argument
 * selects the encoder by the same numbering.  Finding F8852.
 */
#define V17_ENCODER_DIF		0
#define V17_ENCODER_ABS		1
#define V17_ENCODER_TCM		2

/*
 * A byte alongside `V17TX_OBJ_RESULT`/`_B1`, the same shape
 * `V29TX_OBJ_RESULT_B2` and `V21TX_OBJ_RESULT_B2` already carry for their
 * own transmit instances.  `TxNextStateV17` establishes it: every one of
 * its twelve arms clears bit 0 except `V17TX_STATE_QUIET_END`, which sets
 * it, and every arm touches this byte and none of the others at
 * `V17TX_OBJ_RESULT`+2/+3.  Nothing reconstructed reads it back; like its
 * V.21/V.29 siblings it leaves only through the word `V17TX_modem`
 * returns.
 */
#define V17TX_OBJ_RESULT_B2	0x22
#define V17TX_RESULT_B1_BIT0	(1 << 0)
#define V17TX_RESULT_B2_BIT0	(1 << 0)

/*
 * The rest of the parameter/half-duplex block, all four from
 * `TxNextStateV17` and `V17TX_create` together.
 *
 * `V17TXP_MODE` is the transmit constellation index (0..3), typed by
 * `SetTxModeV17`'s own parameter (rank 2): every site that reads this
 * field either passes it straight to `SetTxModeV17` or uses it to index
 * `V17TX_PATTERN_SCR1`/`V17TX_PPS_SCALE`, both of which `SetTxModeV17`'s
 * own mode-indexed tables (`V17TX_SYM_SIZE`) corroborate.  `V17TX_create`
 * derives it from the caller's bit rate (7200/9600/12000/14400 -> 0/1/2/3,
 * anything else -> 3 with the error flags below set) and writes it here;
 * nothing downstream ever changes it.
 *
 * `V17TXP_INT_000C` is usage inference, the weakest tier, stated as such:
 * it is copied verbatim from the caller's config at construction
 * (`V17TX_OBJ_INT_0018` below) and read back only by `TxNextStateV17`'s
 * ALT and EQCOND arms, where a non-zero value shortens ALT's training
 * budget
 * (0x26 samples instead of 0xba0) and steers EQCOND straight to
 * `V17TX_STATE_SCR1` instead of by way of `V17TX_STATE_BRIDGE`. That reads as
 * a short-versus-long training request, the same role V.17 fax's "short
 * training" option plays in the ITU-T text, but nothing in the object types
 * it that way, so the name stays neutral.  `v17tx_priv::r0c` is this value
 * copied at construction and is retained neutral for the same reason
 * (Batch 28).
 *
 * `V17TXP_STATE` is the half-duplex machine's own state number, `V17TX_STATE_*`
 * below -- typed by `TxNextStateV17`'s own `jmp *table(,%eax,4)` (a real jump
 * table, `cmp $0xb`/`ja default` bounding it at 0..11) and by the twelve
 * debug strings the object prints for it, `.rodata.str1.1` 0x4a08..0x4af0
 * (CLAUDE.md's evidence rank 1 -- the author's own words).
 *
 * `V17TXP_SHORT_001A` is the per-state countdown/budget every `TxHdx*V17`
 * handler decrements towards zero before calling `TxNextStateV17` -- the same
 * role `V29TXP_SHORT_0016` plays for V.29's own machine, kept to a neutral
 * name for the same reason that one is.
 */
#define V17TXP_INT_000C		0x0c
#define V17TXP_MODE		0x10
#define V17TXP_STATE		0x18
#define V17TXP_SHORT_001A	0x1a

/*
 * Cleared to 0 by `TxNextStateV17`'s ALT arm alone, immediately before it
 * seeds the equaliser-conditioning LFSR with `SeedScramblerV17`. NEUTRAL:
 * nothing else in this closure reads or writes it, so no role is established
 * beyond "the ALT transition clears it".  Batch 28 retained the field
 * `v17tx_priv::r1c` on this ground.
 */
#define V17TXP_SHORT_001C	0x1c

/*
 * The twelve states, and eleven of the twelve names are the author's own
 * words: `TxNextStateV17` prints the name of the state it is leaving at
 * the top of each arm, paired to a jump-table index the same two ways
 * `RxNextStateV17`'s own table is (CLAUDE.md rank 1, plus each arm
 * installing the handler whose blob symbol name matches):
 *
 *    0  0x4a7f  "V17TX_STATE_START\n"        installs TxHdxSilenceV17
 *    1  0x4a59  "V17TX_STATE_SILENCE\n"      installs TxHdxTEP_V17
 *    2  0x4acd  "V17TX_STATE_TEP\n"          installs TxHdxSilenceV17
 *    3  0x4af0  "V17TX_STATE_QUIET\n"        installs TxHdxABV17
 *    4  0x4a6e  "V17TX_STATE_ALT\n"          installs TxHdxEQCondV17
 *    5  0x4a45  "V17TX_STATE_EQCOND\n"       installs TxHdxBridgeV17 or
 *                                             TxHdxSCR1V17 (V17TXP_INT_000C)
 *    6  0x4a1b  "V17TX_STATE_BRIDGE\n"       installs TxHdxSCR1V17
 *    7  0x4ade  "V17TX_STATE_SCR1\n"         installs TxHdxDataV17
 *    8  0x4abb  "V17TX_STATE_DATA\n"         installs TxHdxSCR1V17
 *    9  0x4a2f  "V17TX_STATE_SCR1_END\n"     installs TxHdxSilenceV17
 *   10  0x4aa4  "V17TX_STATE_QUIET_END\n"    installs TxHdxIdleV17
 *   11  0x4a92  "V17TX_STATE_IDLE\n"         installs TxHdxStartV17, wraps
 *
 * The function name `TxHdxABV17` and the state name `V17TX_STATE_ALT` are
 * both the author's and disagree, the identical mismatch
 * `TxHdxABV29`/`V29TX_STATE_ALT` already carries in `v29fax.h` -- not
 * reconciled, for the same reason.
 *
 * `TxHdxSCR1V17` is installed twice, once by `BRIDGE` (state 6, budget
 * 0x30) and once by `DATA` (state 8, budget 0x20): the same handler
 * drives both the pre-data and the post-data scrambled-training segments,
 * distinguished only by whatever `SGD_control` request the installing arm
 * built immediately before -- the handler itself reads the SGD object's
 * own configuration and does not look at which state value led to it.
 * Likewise `TxHdxSilenceV17` is installed three times (by START, TEP and
 * SCR1_END) and is the same "spend TxNoCarrierV17 for N samples" handler
 * each time.
 *
 * The default arm has no string because it has no jump-table entry:
 * "V17TX_DEFAULT, %d\n" is the thirteenth string and prints for any state
 * outside 0..11, the same "no case, falls to default" shape
 * `V17RX_STATE_ERROR` has on the receive side.
 */
#define V17TX_STATE_START	0
#define V17TX_STATE_SILENCE	1
#define V17TX_STATE_TEP		2
#define V17TX_STATE_QUIET	3
#define V17TX_STATE_ALT		4
#define V17TX_STATE_EQCOND	5
#define V17TX_STATE_BRIDGE	6
#define V17TX_STATE_SCR1	7
#define V17TX_STATE_DATA	8
#define V17TX_STATE_SCR1_END	9
#define V17TX_STATE_QUIET_END	10
#define V17TX_STATE_IDLE	11

/*
 * `V17TX_OBJ_RESULT`'s remaining values, from the handlers and from
 * `TxNextStateV17`'s default arm (which reuses `V17TX_RESULT_BYTE_07`,
 * already named above, rather than a fresh constant -- the two default arms,
 * this function's and `V17TX_modem`'s own unrecognised-mode arm, write the
 * identical byte). `TxHdxDataV17`'s own one-shot rate report (fired once,
 * the first time it runs after `TxHdxSCR1V17` installs it with
 * `V17TXP_SHORT_001A` == 1) is keyed on `V17TXP_MODE` the same way
 * `V17RX_STATUS_RATE_*` is keyed on the receive side's rate code -- rank 2,
 * a field this same function types by switching on it.
 */
#define V17TX_STATUS_DATA		0	/* TxHdxDataV17, steady state */
#define V17TX_STATUS_TRAINING		1	/* AB/SCR1/Bridge/EQCond entry */
#define V17TX_STATUS_DATA_RATE_14400	2
#define V17TX_STATUS_DATA_RATE_12000	3
#define V17TX_STATUS_DATA_RATE_9600	4
#define V17TX_STATUS_DATA_RATE_7200	5
#define V17TX_STATUS_IDLE		6	/* TxHdxIdleV17               */
#define V17TX_STATUS_UNDERRUN		8	/* TxHdxDataV17, no-flag arm  */

/*
 * ------------------------------------------------------------------------
 * `V17TX_create`'s own configuration -- the handle's first 0x20 bytes,
 * copied in from the caller (or from `V17TX_CFG`) by one struct
 * assignment.  `struct v17_status::protocol` and `::tx_bps` above already
 * established `+0x00`/`+0x02` from `V17TX_status`'s own reads; the rest
 * are `V17TX_create`'s alone.
 *
 * `fifo_size_factor` is the transmit FIFO's size factor, read back by
 * `V17TX_create` itself to compute the FIFO's capacity as
 * `fifo_size_factor * 3 * 16` -- 48 elements at the default value of 1.
 * `V29TX_CFG` carries an identical field at the same offset with an
 * identical role and default, still spelled `int_0014` there -- V.29's
 * own copy is left as a candidate for whoever visits it.  Finding F10144.
 *
 * `int_0018` is copied, unchanged, to `V17TXP_INT_000C` -- see that
 * constant's own comment for what little is established about what it
 * steers.
 *
 * `int_001c` becomes `FPM_PPS_CFG.aux`, read back and stored through to
 * the shaper's own configuration -- the same "aux carries the caller's
 * own field, across the `(void *)(long)` idiom" shape D1250 already names
 * for `V29TX_create`'s `int_0018`, one field over because V.17's config
 * carries one more dword than V.29's (`int_0010`, below, which nothing
 * traced reads at all).
 *
 * `short_0004`, `short_0006` and `int_000c` are copied in and never read
 * back by anything in this closure -- neutral, the same ground
 * `V29TX_CFG`'s own untouched fields stand on.
 *
 * `int_0008` gets a second writer: `V17TX_control` (`v17tx_control_req`
 * above) copies its own request's `+0x04` straight into this field,
 * independently of the constructor's whole-struct copy -- the same shape
 * `V29RX_control` gave `V29_OBJ_INT_0008` (finding F10103).  Still
 * nothing reconstructed reads it back, so it stays neutral in that sense;
 * it is not neutral in the sense of "only ever written once".  Finding
 * F10107.
 *
 * `int_0010` is read, at byte granularity, by `V17TX_status` (`p[0x10] &
 * V17_STATUS_FLAG_04`, not a full 32-bit read) and gets an independent
 * writer of that same bit from `V17TX_control`'s own `ctl0` bit 2.
 * Finding F10107.
 */
struct v17tx_cfg {
	short		protocol;	/* +0x00 <- V17TX_status's own read  */
	short		bitrate;	/* +0x02 <- V17TX_status's own read  */
	short		short_0004;	/* +0x04 copied, never read here     */
	short		short_0006;	/* +0x06 copied, never read here     */
	int		int_0008;	/* +0x08 <- V17TX_control's own +0x04;
					 * never read back                   */
	int		int_000c;	/* +0x0c copied, never read here     */
	int		int_0010;	/* +0x10 bit 2 <-> V17_STATUS_FLAG_04,
					 * V17TX_status's own read and
					 * V17TX_control's own ctl0 bit 2 --
					 * the dword V.29's own cfg lacks    */
	int		fifo_size_factor; /* +0x14 -> capacity, * 3 * 16     */
	int		int_0018;	/* +0x18 -> V17TXP_INT_000C          */
	int		int_001c;	/* +0x1c -> FPM_PPS_CFG.aux          */
};

union v17_result {
	int word;
	struct {
		unsigned char status;
		unsigned char flags;
		unsigned char flags2;
		unsigned char r3;
	} byte;
};

struct v17tx_priv {
	struct fax_fifo *fifo;	/* +0x00 */
	struct sgd *sgd;	/* +0x04 */
	/* +0x08 is the V17TXP_INT_0008 FIFO-bypass gate: `V17TX_modem`
	 * queues the caller's block when it is zero and passes it through
	 * when non-zero, and `V17TX_control` sets it 0/1 from `ctl1` bit 4.
	 * The bit's meaning beyond the gate is unstated -- retained neutral
	 * (Batch 28). */
	int r08;		/* +0x08 */
	/* +0x0c is the value of `cfg.int_0018` (V17TXP_INT_000C) copied at
	 * construction and read only by the ALT and EQCOND arms; it selects
	 * a budget and bypasses BRIDGE, which reads as short-vs-long
	 * training but is not typed by the object -- retained neutral
	 * (Batch 28). */
	int r0c;		/* +0x0c */
	short mode;		/* +0x10 */
	/* +0x12 is touched by nothing reconstructed, reader or writer --
	 * retained neutral (Batch 28). */
	short r12;		/* +0x12 */
	/**
	 * @brief Run the active transmit state.
	 * @param modem Owning v17tx, not this private block.
	 * @param in Input-word buffer; unused by some states.
	 * @param[out] out Shaped output samples.
	 * @param[in,out] budget Remaining symbols, not samples.
	 * @return Samples written; a state-only transition may return zero.
	 */
	short (*process)(void *modem, unsigned short *in, short *out,
			 short *budget); /* +0x14 */
	short state;		/* +0x18 */
	short countdown;	/* +0x1a */
	/* +0x1c is V17TXP_SHORT_001C: cleared by the ALT arm alone and read
	 * by nothing -- retained neutral (Batch 28). */
	short r1c;		/* +0x1c */
	unsigned short no_carrier_sym; /* +0x1e */
};

struct v17tx {
	struct v17tx_cfg cfg;	/* +0x00 .. +0x1f */
	union v17_result result; /* +0x20 */
	struct v17tx_priv *priv; /* +0x24 */
	struct v17tx_fp *fp;	/* +0x28 */
};

struct v17rx_priv {
	struct fpm_mtd *mtd;	/* +0x00 */
	struct fpm_tone *tone;	/* +0x04 */
	/* +0x08 gate `RxHdxDataV17` must see as zero before it
	 * demodulates; three writers, no established meaning -- retained
	 * neutral (V17RXC_INT_0008, F9442, Batch 25). */
	int r08;
	unsigned short rate_code; /* +0x0c */
	short r0e;		/* +0x0e unmodelled; retained neutral  */
	/* +0x10 copied from `cfg` +0x14; rank 2, the decoder's own field
	 * of this name (`v17_dec::short_train`), Batch 25. */
	int short_train;
	/**
	 * @brief Run the active receive state.
	 * @param modem Owning v17rx, not this private block.
	 * @param in Input samples.
	 * @param[out] out Decoded data words.
	 * @param[in,out] count Available samples on entry, unconsumed on return.
	 * @return Data words written. The dispatcher accumulates this separately.
	 */
	short (*process)(void *modem, short *in, short *out,
			 unsigned short *count); /* +0x14 */
	short state;		/* +0x18 */
	short countdown;	/* +0x1a */
	short *scratch;		/* +0x1c */
	/* +0x20 selects DataCarrierDetectV17's body; nothing traced writes
	 * it, so its role is unstated -- retained neutral. */
	short r20;
	short r22;		/* +0x22 unmodelled; retained neutral  */
	struct fpm_mtd *mtd2;	/* +0x24 */
	short *buf2;		/* +0x28 */
	short offband;		/* +0x2c */
	/* +0x2e set on a data-carrier failure and gates the V.21 offband
	 * watch; usage inference, single role (Batch 25). */
	short offband_latch;
	struct fpm_agc agc;	/* +0x30 */
};

union v17rx_agc {
	struct fpm_agc value;
	struct {
		unsigned char r00[0x1c];
		short signal;
		short r1e;
		unsigned char tail[0x0c];
	} narrow;
};

/*
 * Batch 25 dispositioned every offset-only member of this struct.  None
 * carries a single established role: the head is either a constant the
 * constructor writes and nothing reads, a state field reported through an
 * unnamed status bit, or one of the three AGC-gated enables whose only
 * evidence is the plumbing they drive (F9102).  `quality_threshold` is the
 * one exception and is named from `QualityDetectV17`'s own comparison.
 */
struct v17rx_state {
	/* +0x00 cleared by `V17RX_control`'s CLEAR_STATE0 bit and reported
	 * as `V17RX_status` flags bit 3; multi-role, retained neutral. */
	int r00;
	/* +0x04 enables SRE adapt; plumbing only, retained neutral
	 * (F9102). */
	int r04;
	/* +0x08 enables FSE PLL; plumbing only, retained neutral
	 * (F9102). */
	int r08;
	int r0c;		/* +0x0c written 0, read by nothing; retained */
	/* +0x10 enables FSE LMS; plumbing only, retained neutral
	 * (F9102). */
	int r10;
	int r14;		/* +0x14 written 0, read by nothing; retained */
	int r18;		/* +0x18 written 1, read by nothing; retained */
	/* +0x1c bit 0 -> status flags bit 1; meaning unstated, retained
	 * neutral (F9474). */
	int r1c;
	int r20;		/* +0x20 written 0, read by nothing; retained */
	unsigned short rate_code;
	short r26;		/* +0x26 read by nothing; retained neutral */
	int r28;		/* +0x28 written 0, read by nothing; retained */
	struct v17_dec dec;		/* +0x2c */
	struct fpm_mrf mrf;		/* +0x98 */
	union v17rx_agc agc;		/* +0xb4 */
	struct fpm_sre sre;		/* +0xe0 */
	struct fpm_fse fse;		/* +0x170 */
	/* +0x4f88 four-byte gap below the equaliser, untouched; retained. */
	unsigned char r4f88[4];
	struct fpm_sdm sdm;		/* +0x4f8c */
	short *buf_mrf;			/* +0x4fa4 */
	short *buf_sre;			/* +0x4fa8 */
	short qavg;			/* +0x4fac */
	short qcount;			/* +0x4fae */
	/* +0x4fb0 per-rate threshold `QualityDetectV17` judges `qavg`
	 * against; usage inference (Batch 25). */
	short quality_threshold;
	/* +0x4fb2 write-only quality verdict; no reader, retained
	 * neutral. */
	short r4fb2;
	short energy_watch;		/* +0x4fb4 */
	short rms_ref;			/* +0x4fb6 */
	short rms_phase;		/* +0x4fb8 */
	short r4fba;			/* allocation tail, unmodelled */
};

struct v17rx {
	struct v17rx_cfg cfg;		/* +0x00 .. +0x27 */
	union v17_result result;	/* +0x28 */
	short *out_i;			/* +0x2c */
	short *out_q;			/* +0x30 */
	unsigned short *n_out;		/* +0x34 */
	short *icoeff;			/* +0x38 */
	short *qcoeff;			/* +0x3c */
	unsigned short taps;		/* +0x40 */
	short r42;			/* +0x42 never written; retained */
	/* +0x44 written 0, read by nothing; retained neutral. */
	int r44;
	/* +0x48 written 0, read by nothing; retained neutral. */
	int r48;
	/* +0x4c written 0, read by nothing; retained neutral. */
	short r4c;
	short r4e;			/* +0x4e never written; retained */
	/* +0x50 written 0, read by nothing; retained neutral. */
	int r50;
	/* +0x54 written 0, read by nothing; retained neutral. */
	int r54;
	/* +0x58 written 0, read by nothing; retained neutral. */
	short r58;
	short r5a;			/* +0x5a never written; retained */
	struct v17rx_priv *ctl;		/* +0x5c */
	struct v17rx_state *state;	/* +0x60 */
};

extern struct v17tx_cfg V17TX_CFG;

/**
 * @brief Build the V.17 transmit instance, or reinitialise the one the
 *        caller already has.
 *
 * See `src/fax/v17.c` for the derivation and `V21TX_create`/`V29TX_create`
 * for the shape this follows.
 *
 * @param modem   NULL to allocate a new instance, or an existing one to
 *                reinitialise.
 * @param params  Configuration, or NULL for the library's #V17TX_CFG.
 * @return The transmit instance.
 */
void *V17TX_create(void *modem, const struct v17tx_cfg *params);

/*
 * The half-duplex machine's own dispatcher and its twelve installed states
 * -- `TxNextStateV17` and the nine `TxHdx*V17` handlers (`TxHdxSCR1V17` and
 * `TxHdxSilenceV17` each cover more than one `V17TX_STATE_*` value; see
 * above).  All ten link only together (finding F9600); see `src/fax/v17.c`.
 */

/**
 * @brief Advance the transmit half-duplex machine to its next state.
 *
 * A twelve-entry jump table on #V17TXP_STATE (see the `V17TX_STATE_*`
 * table above); each arm seeds the next state's countdown and SGD request,
 * installs that state's handler at #V17TXP_PROCESS, and updates the two
 * result-word status bits.  `SCR1`, `DATA` and `SCR1_END` return directly
 * with `V17TX_RESULT_B1_BIT0` set rather than falling to the shared tail
 * that clears it; every other arm falls through to that tail.  Finding
 * F9912.
 *
 * @param modem  The V.17 modem object.
 */
void TxNextStateV17(void *modem);
/**
 * @brief `V17TX_STATE_START`'s handler: nothing but the transition.
 * @param modem   The V.17 modem object.
 * @param in      Unused.
 * @param out     Unused.
 * @param budget  Unused.
 * @return Always 0.
 */
short TxHdxStartV17(void *modem, unsigned short *in, short *out,
		    short *budget);
/**
 * @brief Installed for `START`, `TEP` and `SCR1_END` alike.
 *
 * Spends `min(remaining, *budget)` on `TxNoCarrierV17` and counts the
 * per-state countdown down towards `TxNextStateV17`.  Does not write
 * `V17TX_OBJ_RESULT` -- measured, not an omission, and a genuine divergence
 * from every other countdown handler in this file, which all report
 * `V17TX_STATUS_TRAINING` on entry.
 *
 * @param modem   The V.17 modem object.
 * @param in      Scratch buffer for the generated symbols.
 * @param out     Destination for the shaped output samples.
 * @param budget  In/out: samples remaining in this call, decremented by
 *                what was spent.
 * @return The number of samples written to `out`.
 */
short TxHdxSilenceV17(void *modem, unsigned short *in, short *out,
		      short *budget);
/**
 * @brief `V17TX_STATE_SILENCE`'s handler: the TEP training-echo-protection
 *        tone.
 *
 * Spends `min(remaining, *budget)` on `ModDataV17` (no `ScrambleDataV17`);
 * installed by `SILENCE`'s SGD request (word_syms=2, data_word=0).
 *
 * @param modem   The V.17 modem object.
 * @param in      Scratch buffer for the generated symbols.
 * @param out     Destination for the shaped output samples.
 * @param budget  In/out: samples remaining in this call.
 * @return The number of samples written to `out`.
 */
short TxHdxTEP_V17(void *modem, unsigned short *in, short *out,
		   short *budget);
/**
 * @brief `V17TX_STATE_ALT`'s handler: the alternating training dibit
 *        (function name AB, debug string ALT -- the same mismatch
 *        `TxHdxABV29`/`V29TX_STATE_ALT` already carries).
 *
 * Spends `min(remaining, *budget)` on `ModDataV17`, unscrambled, driven by
 * `ALT`'s SGD request (word_syms=2, data_word=0xf).
 *
 * @param modem   The V.17 modem object.
 * @param in      Scratch buffer for the generated symbols.
 * @param out     Destination for the shaped output samples.
 * @param budget  In/out: samples remaining in this call.
 * @return The number of samples written to `out`.
 */
short TxHdxABV17(void *modem, unsigned short *in, short *out, short *budget);
/**
 * @brief `V17TX_STATE_EQCOND`'s handler: equaliser-conditioning scrambled
 *        training.
 *
 * `TxHdxEQCondV17`, `TxHdxBridgeV17` and `TxHdxSCR1V17` are one body
 * compiled three times, byte for byte identical: `SGD_symbol_gen` then
 * `ScrambleDataV17` then `ModDataV17`, driven by whichever SGD request the
 * installing arm of `TxNextStateV17` built beforehand.  What differs
 * between the three transitions is upstream, in `TxNextStateV17` itself;
 * the handler code does not look at which state led to it.
 *
 * @param modem   The V.17 modem object.
 * @param in      Scratch buffer for the generated symbols.
 * @param out     Destination for the shaped output samples.
 * @param budget  In/out: samples remaining in this call.
 * @return The number of samples written to `out`.
 */
short TxHdxEQCondV17(void *modem, unsigned short *in, short *out,
		     short *budget);
/**
 * @brief `V17TX_STATE_BRIDGE`'s handler: see `TxHdxEQCondV17`'s own
 *        comment -- the same body, a different symbol.
 * @param modem   The V.17 modem object.
 * @param in      Scratch buffer for the generated symbols.
 * @param out     Destination for the shaped output samples.
 * @param budget  In/out: samples remaining in this call.
 * @return The number of samples written to `out`.
 */
short TxHdxBridgeV17(void *modem, unsigned short *in, short *out,
		     short *budget);
/**
 * @brief `V17TX_STATE_SCR1`'s handler: see `TxHdxEQCondV17`'s own comment
 *        -- the same body, a different symbol, installed both by `BRIDGE`
 *        (pre-data) and by `DATA` (post-data).
 * @param modem   The V.17 modem object.
 * @param in      Scratch buffer for the generated symbols.
 * @param out     Destination for the shaped output samples.
 * @param budget  In/out: samples remaining in this call.
 * @return The number of samples written to `out`.
 */
short TxHdxSCR1V17(void *modem, unsigned short *in, short *out,
		   short *budget);
/**
 * @brief `V17TX_STATE_DATA`'s handler: the steady-state data path.
 *
 * `TxHdxDataV21`'s and `TxHdxDataV29`'s shape one modulation over: an
 * optional one-shot rate report on the first call after `TxHdxSCR1V17`
 * installs it, then a three-arm FIFO read -- satisfied and
 * underrun-with-`V17TXP_INT_0008`-clear both scramble+modulate exactly what
 * was taken (the underrun arm takes the full requested budget, raising
 * `V17TX_RESULT_B1_BIT1` and reporting `V17TX_STATUS_UNDERRUN`);
 * underrun-with-`V17TXP_INT_0008`-set modulates only what the FIFO gave,
 * leaves the remainder in `*budget`, and calls `TxNextStateV17`.
 *
 * @param modem   The V.17 modem object.
 * @param in      Scratch buffer for the generated symbols.
 * @param out     Destination for the shaped output samples.
 * @param budget  In/out: samples remaining in this call.
 * @return The number of samples written to `out`.
 */
short TxHdxDataV17(void *modem, unsigned short *in, short *out,
		   short *budget);
/**
 * @brief `V17TX_STATE_IDLE`'s handler.
 *
 * Exactly `TxHdxIdleV21`'s and `TxHdxIdleV29`'s shape: reports
 * `V17TX_STATUS_IDLE` unconditionally, then spends the whole call's budget
 * on `TxNoCarrierV17` while the FIFO is empty, or hands off to
 * `TxNextStateV17` the moment it is not.
 *
 * @param modem   The V.17 modem object.
 * @param in      Scratch buffer for the generated symbols.
 * @param out     Destination for the shaped output samples.
 * @param budget  In/out: samples remaining in this call.
 * @return The number of samples written to `out`.
 */
short TxHdxIdleV17(void *modem, unsigned short *in, short *out,
		   short *budget);

/* ------------------------------------------------------------------------ */
/* The receive instance                                                     */

/*
 * Two `short *` the receiver does not own, and the coefficient store they
 * are the destination of.  `StoreCoefV17` copies 49 entries from each of
 * the receiver state's two coefficient pointers into the arrays here.
 * The pairing of the two is the author's, from the function's own name;
 * which of the two arrays is which rail is not established, so they are
 * numbered, not named.
 */
#define V17RX_OBJ_COEFSAVE0	0x18
#define V17RX_OBJ_COEFSAVE1	0x1c

/*
 * A `short *` that holds a saved copy of the receiver's rate.  Established
 * by a matched pair, which is why this one carries a real name where its
 * two neighbours do not: `StoreCoefV17` writes it from `V17RXS_RATE` and
 * `Restore_rateV17` writes `V17RXS_RATE` back from it.  The direction, the
 * width and the field at the far end all agree, and "rate" is the
 * author's word from `Restore_rateV17`.
 */
#define V17RX_OBJ_RATESAVE	0x20

/*
 * The int `V17RX_modem` returns, the status byte at its low end, and the
 * flag byte above that.  See the two-instances note above: 0x29 is byte 1
 * of the 4 bytes at 0x28, so the object both modifies and returns the
 * same word, and 0x28 is byte 0 of it.  `V17RX_modem` clears one bit of
 * 0x29 on entry and hands the whole word back.
 */
#define V17RX_OBJ_RESULT	0x28
#define V17RX_OBJ_RESULT_B1	0x29

/*
 * The three named bits of 0x29.  `RxHdxDataV17` and `RxHdxErrorV17`,
 * together with `V17RX_modem`/`V17RX_status`, let the whole object be
 * enumerated: every read-modify-write of `obj + 0x29` in all 44 V.17
 * symbols, 34 sites (findings F9230/F9231).  V.21's byte at `rx + 0x19`
 * has the same three bits in the same three roles (`v21fax.h`, finding
 * F8896) and Bell 103's does too, which corroborates and is not the
 * derivation.
 *
 * ERROR (0x02) -- set by `RxHdxErrorV17`, by `RxNextStateV17`'s default
 *   arm, and by the four transitions that install `RxHdxErrorV17`
 *   (`RxHdxScramV17`/`BridgeV17`/`PrtcolV17`/`EpochDetV17`); cleared by
 *   `V17RX_modem` alone, at the top of every block.  Nothing reads it: it
 *   leaves only through the returned word.  It is NOT delivered set --
 *   `V17RX_create` clears the whole word before finishing construction
 *   with 0x28 = 2, 0x29 = 0x50, 0x2a = 0, an earlier reading that stopped
 *   at its own `orb` and missed the later clear (finding F9442).
 *
 * CARRIER (0x20) -- cleared and then set again if and only if
 *   `CarrierDetectV17` answers non-zero, in `RxHdxIdleV17` and
 *   `RxHdxStartV17`.  `RxHdxDataV17` sets it on entry and clears it when
 *   `DataCarrierDetectV17` says the carrier has gone.  Read by
 *   `RxHdxIdleV17`, gating its look at the decoder error.  Both ends
 *   measured.
 *
 * LOW_SNR (0x80) -- cleared by `RxHdxDataV17` and set there (and in the
 *   three other handlers that call `GetSNRV17`) when `GetSNRV17` comes
 *   back at or below `V17RX_SNR_THRESHOLD`.  Read by `V17RX_status`,
 *   where a set bit makes the reported +0x06 zero.  Both ends measured.
 *
 * Bits 0x04 and 0x08 are touched by nothing in the object.  Bits 0x10 and
 * 0x40 are: `V17RX_create` sets both, once, in the same clear-then-`orb`
 * that resolves ERROR above.  Nothing reads either and nothing else
 * writes them, so they are named by value and nothing more (finding
 * F9473; see `V17RX_FLAG_BIT4` below).
 *
 * The two error masks are not the same, and the difference is the DATA
 * bit: `RxNextStateV17`'s default arm clears CARRIER and
 * `V17RX_FLAG_DATA` together, where the four handlers' error arms clear
 * CARRIER alone -- one instruction apart and easy to misread as the same
 * mask (finding F9440).
 *
 * DATA (0x01) -- `RxNextStateV17` is its only writer in the whole object:
 *   set on exactly the two transitions that install `RxHdxDataV17`
 *   (leaving SCRAM and leaving IDLE) and cleared on every other
 *   transition and by the default arm -- set if and only if the handler
 *   just installed is the DATA handler, which is what the name says and
 *   nothing more.  Nothing reads it anywhere in the object; like ERROR it
 *   leaves through the returned word.  `V21RX_FLAG_DATA` is the same bit
 *   of the same shape in `v21fax.h`, a corroboration and not the
 *   derivation.  Finding F9440.
 */
#define V17RX_FLAG_ERROR	(1 << 1)
#define V17RX_FLAG_CARRIER	(1 << 5)
#define V17RX_FLAG_LOW_SNR	(1 << 7)
#define V17RX_FLAG_DATA		(1 << 0)

/*
 * The two bits `V17RX_create` sets and no reader has.  Named by value,
 * per CLAUDE.md, and deliberately: their only write is one `orb`
 * immediate that carries no evidence about what either indicates, and
 * the whole object contains no read.  Do not promote them to a meaning.
 */
#define V17RX_FLAG_BIT4		(1 << 4)
#define V17RX_FLAG_BIT6		(1 << 6)

/*
 * Byte 2 of the same word, and its one bit.  Both are named by value and
 * the role of neither is established -- which is the whole point of
 * writing them down this way.  `RxNextStateV17` is the only function in
 * the object that touches +0x2a: it clears bit 0 on six of its seven
 * transitions and on the default arm, sets it on exactly one (state DATA
 * -> IDLE), and skips it altogether on the SCRAM arm of state PROTOCOL.
 * Nothing reads it and nothing writes any other bit of it, so there is
 * no second end to measure against.  The near-inverse of
 * `V17RX_FLAG_DATA` is a tempting reading and is declined: the two
 * disagree on the PROTOCOL/SCRAM arm, so they are not one flag spelled
 * twice.  Findings F9441 and D1213.
 */
#define V17RX_OBJ_RESULT_B2	0x2a
#define V17RX_RESULT_B2_BIT0	(1 << 0)

/*
 * `RxHdxDataV17` raises `V17RX_FLAG_LOW_SNR` when `GetSNRV17` comes back
 * at or below this.  The compare is 16 bits wide and signed, which is
 * what `GetSNRV17`'s own `short` return gives, so nothing narrows it
 * here.  `GetSNRV17` is `13 - V17RXS_DEC_ERROR`, so the flag is raised
 * once the decoder error reaches 5.
 */
#define V17RX_SNR_THRESHOLD	8

/*
 * The status byte at `V17RX_OBJ_RESULT`, and all nine values it takes.
 * Nothing in the object reads any of them -- the byte leaves through the
 * word `V17RX_modem` returns -- so a name here can only be the site that
 * writes it (`v21fax.h`'s ruling for the same field of the same shape).
 * The write set is now complete, because every writer is reconstructed.
 *
 * The last four are the rate ladder, rank-2 evidence rather than a
 * guess: `RxHdxScramV17` and `RxNextStateV17`'s IDLE arm both read
 * `V17RXC_RATE_CODE` and write 9, 8, 7 or 6 from it, and `V17RX_create`
 * is what puts the code there, switching on `V17RX_OBJ_RX_BPS` to store
 * 0/1/2/3 for 7200/9600/12000/14400 bit/s.  So the ladder maps a bit rate
 * to a status byte, one to one; what the values mean to a reader of the
 * word is still not established (finding F9443).
 * `V17RX_STATUS_RATE_14400` is also the ladder's `else` arm, so a rate
 * code the object never writes lands there too.
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
 * Named by their destination (CLAUDE.md rank 2, not usage inference):
 * +0x00 lands in `struct v17_status::protocol` and +0x04 in `::rx_bps`,
 * both carrying their names from `v22_status` and `v32_status`, each
 * derived from its own disassembly with nothing to do with V.17.
 * `V17TX_status` puts the transmit rate in +0x02 and zero in +0x04; this
 * function does the mirror image -- zero in +0x02 and a rate in +0x04 --
 * which is exactly what a receiver-side reporter should do.
 *
 * +0x04 is also written to the status block's +0x12, a second time and
 * unchanged.  That offset stays neutral: two modules disagree about it
 * and nothing here breaks the tie.
 */
#define V17RX_OBJ_PROTOCOL	0x00
#define V17RX_OBJ_RX_BPS	0x04

/*
 * Twelve fields `V17RX_create` writes last, six of them out of the
 * equaliser.
 *
 * The constructor's final block copies six handles out of the `struct
 * fpm_fse` it has just built and zeroes six more slots.  The six copies are
 * typed by the struct they come out of (CLAUDE.md rank 2, not usage
 * inference) -- `fpm_fse.h` models every one of them independently:
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
 * The other six have no evidence of role and are not named: every one is a
 * constant zero written once and read by nothing in the 1.2 MB.  The widths
 * are the object's: `int` at +0x44, +0x48, +0x50 and +0x54, `short` at +0x4c
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
 * The receiver's two sub-blocks.  Both are pointers the instance holds.
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
 * `struct fpm_mtd *` and `struct fpm_tone *`, typed by their callees:
 * `DemodDataV17` passes +0x00 to `FPM_MTD_detect` and +0x04 to
 * `FPM_TONE_kill`, which is CLAUDE.md's rank-2 evidence and not usage
 * inference.
 */
#define V17RXC_MTD		0x00
#define V17RXC_TONE		0x04

/*
 * An int `RxHdxDataV17` requires to be zero before it will demodulate: the
 * second half of its gate, alongside the carrier being up.  Neutral: it is
 * a plain `int` test, and what it indicates is not established.
 *
 * It has three writers, not zero as an earlier reading (measured only over
 * the functions this header had at the time) said: `RxNextStateV17`'s DATA
 * arm clears it on the transition into IDLE, and `V17RX_control` writes 0
 * or 1 from two bits of its own argument.  So the gate is cleared whenever
 * the machine leaves DATA and is also driven from outside by the control
 * entry point (finding F9442).
 *
 * It is NOT `V17RXS_INT_0008`, a different block at the same offset:
 * `DemodDataV17` reads its own +0x08 off the demodulator state
 * (`V17RX_OBJ_STATE`), while this is +0x08 of the control block
 * (`V17RX_OBJ_CTL`) -- the two offsets coincide and the two fields do not
 * (finding F9232).
 */
#define V17RXC_INT_0008		0x08

/*
 * The receive bit rate, as a four-value code.  `V17RX_create` establishes
 * it (rank 2: it switches on `V17RX_OBJ_RX_BPS`, the same field
 * `V17RX_status` reports as `rx_bps`, and stores 0/1/2/3 for
 * 7200/9600/12000/14400, defaulting to 3).  Its two readers --
 * `RxNextStateV17`'s IDLE arm and `RxHdxScramV17`'s expiry path -- both use
 * it only to pick one of `V17RX_STATUS_RATE_*`, and both read it as
 * `unsigned short`.  Finding F9443.
 */
#define V17RXC_RATE_CODE	0x0c

#define V17RX_RATE_7200		0
#define V17RX_RATE_9600		1
#define V17RX_RATE_12000	2
#define V17RX_RATE_14400	3

/*
 * `struct v17rx_priv::short_train` (Batch 25): an int `CarrierDetectV17` and
 * `DataCarrierDetectV17` both require to be non-zero before they will look
 * at the decoder error at all, and `RxNextStateV17` reads it twice on one
 * arm to choose BOTH the countdown seed and whether to call
 * `Restore_rateV17` (see D1212 for why the second read is dead).  The name
 * is rank-2 and not an author string: `V17RX_create` copies this field into
 * `v17_dec::short_train`, the decoder's own field of that name, which
 * selects the short-vs-long training path.  The offset constant keeps its
 * old spelling because the tests reach the field through it.
 */
#define V17RXC_INT_0010		0x10

/*
 * A dispatch slot: `V17RX_modem` calls `*(fn *)(ctl + 0x14)` with its own
 * four arguments unchanged -- a table entry planted at construction, not a
 * direct symbol reference (the call carries no relocation).  `V17RX_create`
 * seeds it with `RxHdxStartV17`, beside the `V17RX_STATE_START` it writes
 * into `V17RXC_STATE` fifteen bytes earlier; `RxNextStateV17` is the only
 * other writer and installs state and handler together on every one of its
 * arms.  Finding F9471.
 */
#define V17RXC_PROCESS		0x14

/*
 * The receive state number.  `RxNextStateV17` stores 1..6 into it across
 * its six transition arms, the four handlers that install `RxHdxErrorV17`
 * store `V17RX_STATE_ERROR` beside that store, and `V17RX_create` seeds
 * `V17RX_STATE_START`.  `DemodDataV17`'s own test of it is "the machine has
 * left START", skipping the tone-kill/tone-detect front end once it has.
 *
 * Signed `short`, and that is forced: `RxNextStateV17` sign-extends it and
 * bounds it against the jump table with an unsigned compare, so a negative
 * state takes the default arm rather than indexing backwards -- an
 * `unsigned short` cannot produce that load.  `V27SH_SKIP_TONE` in
 * `v27fax.h` is the same field of the same machine with the same evidence.
 * Findings F9235 and F9440.
 *
 * `V17RXC_SHORT_0018` is kept as an alias because `t_v17fax.c` still spells
 * it that way and is not this batch's file to edit; new code uses
 * `V17RXC_STATE`.
 */
#define V17RXC_STATE		0x18
#define V17RXC_SHORT_0018	V17RXC_STATE

/*
 * Blocks remaining in this state, and that is the whole of what is
 * established.  `RxNextStateV17` seeds it on every transition but one (the
 * IDLE arm does not write it at all, deviation D1215), and each of
 * `RxHdxScramV17`, `RxHdxBridgeV17`, `RxHdxPrtcolV17` and `RxHdxEpochDetV17`
 * decrements it once per block and advances the machine once the
 * decremented value is at or below zero.  What any particular seed is for
 * -- why the protocol state gets 62 blocks when the rate was not restored
 * and 1 when it was -- is not established and no name here claims it.
 *
 * The load is unsigned and the test is signed, both the object's own: the
 * extension is dead (only the low half is stored and tested, so it follows
 * the declared type of the local, finding F7803), and the signed 16-bit
 * compare means a seed of 0x8000 expires immediately rather than running
 * for 32768 blocks.  `v21fax.h`'s `struct v21_rx_hdx::countdown` is the
 * same field of the same shape in a sibling modem, which corroborates and
 * is not the derivation.
 */
#define V17RXC_COUNTDOWN	0x1a

/*
 * The eight states, and seven of the names are the author's own words:
 * `RxNextStateV17` prints the name of the state it is leaving at the top of
 * each arm, and the arms are the seven entries of a compiler-generated jump
 * table, so the pairing of a value to a string is the table's own and not a
 * reading of it.  The mapping is forced twice over, because each arm also
 * installs the handler whose own blob symbol name matches the state it
 * writes: 1 with `RxHdxEpochDetV17`, 2 with `RxHdxPrtcolV17`, 3 with
 * `RxHdxBridgeV17`, 4 with `RxHdxScramV17`, 5 with `RxHdxDataV17` and 6 with
 * `RxHdxIdleV17`.
 *
 * `V17RX_STATE_ERROR` is the one without a string: it has no case, so it
 * falls to the default arm (deviation D1211), and its name comes from the
 * handler the four training states install beside it, `RxHdxErrorV17`.
 * Finding F9440.
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
 * The second detector chain, which is `DataCarrierDetectV17`'s alone: a
 * `struct fpm_mtd *` at +0x24, its own `short *` buffer at +0x28, a sample
 * counter at +0x2c, a latch at +0x2e (`offband_latch`, Batch 25) and a
 * `struct fpm_agc` at +0x30.
 * `V17RXC_MTD2`, `V17RXC_BUF2` and `V17RXC_AGC` are all typed by their
 * callees (CLAUDE.md rank 2), exactly as `V17RXC_MTD`/`V17RXC_TONE` are.
 *
 * The counter counts `FPM_MTD_ABSENT`, which is not silence: the object
 * accumulates while `FPM_MTD_detect` returns 0 ("signal present, but not in
 * band", per `fpm_mtd.h`) and clears on either a detected tone
 * (`FPM_MTD_PRESENT`) or a dead line (`FPM_MTD_NOSIGNAL`).  So crossing
 * 0x4ff means 1,280 consecutive samples (160 ms at 8 kHz) of energy that is
 * NOT the band this detector watches -- a coherent reason to conclude
 * something else has taken the channel.
 *
 * The band is V.21 channel 2: `V17RX_create` configures this detector with
 * `V21_CHAN2_MTD_COEFF` (`faxcfg.h`'s bank, shared by all three fax
 * receiver constructors) and `min_level` 300, where the first detector
 * (`V17RXC_MTD`) gets `V17_MTD_COEFF` and 100 -- matching the "V17: V21
 * Carrier detected" message this counter hangs off, from two independent
 * directions.  The field name stays neutral because what the counter
 * counts is still absence and not the tone.  Finding F9471.
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
 * The three sources are neutral: what is established is the plumbing, not
 * the meaning.  They sit immediately above `V17RXS_INT_0000`, consistent
 * with a small int array, but nothing read here proves it is one.
 *
 * The four destinations are not neutral: the tiling in finding F8854 places
 * them at `struct fpm_sre::adapt` (`V17RXS_SRE` + 0x48) and `struct
 * fpm_fse::pll_on`/`tilt_on`/`lms_on` (`V17RXS_FSE` + 0x44/0x48/0x4c) --
 * four fields of two structs `fpm_sre.h` and `fpm_fse.h` model
 * independently, each described there as the caller's enable for a stage of
 * the loop.  `DemodDataV17` is that caller, so `src/fax/v17.c` reaches them
 * as struct members and not through these offsets; the offsets stay for the
 * test, which has to find the same bytes without the struct.  Finding
 * F9102.
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
 * batch touches any of them.  All three are neutral -- the function turns
 * each into one bit of a flags byte whose bits are themselves unnamed, so
 * nothing establishes what any of them indicates.
 *
 * The two ints are reported inverted (`sete` on a 32-bit test), the byte is
 * reported straight and only its bit 0 is read.
 *
 * `V17RXS_BYTE_001C` is an `int`, and `V17RX_create` is what settles it: the
 * old name came from `V17RX_status`'s `movzbl` read of bit 0 (an unforced
 * narrowing, since only one bit survives), but the constructor writes all
 * four bytes of the field, exactly as it writes its six int neighbours.
 * `V17RXS_INT_001C` is the field and the old name is kept as an alias
 * because `t_v17fax.c` spells it that way; `V17RXS_001C_BIT0` is still the
 * bit the reader takes.  Finding F9474.
 */
#define V17RXS_INT_0000		0x00
#define V17RXS_INT_001C		0x1c
#define V17RXS_BYTE_001C	V17RXS_INT_001C
#define V17RXS_001C_BIT0	0x01

/*
 * The rest of the head, and every one of them is `V17RX_create`'s alone.
 * The constructor writes eleven fields and nothing reconstructed reads any
 * of the six below.  All six are neutral: what is established is the
 * width, the constant and the writer, and nothing else.
 *
 *     +0x0c  int             0
 *     +0x14  int             0
 *     +0x18  int             1
 *     +0x20  int             0
 *     +0x24  unsigned short  a copy of V17RXC_RATE_CODE
 *     +0x28  int             0
 *
 * `V17RXS_RATE_CODE` is the one that carries a real name, and it is rank 2
 * rather than usage inference: the constructor loads `V17RXC_RATE_CODE` and
 * stores its low half here, so the field IS that code and the name says
 * only that.  What a reader of it would do with it is not established,
 * because there is no reader.
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
 * `V17RXS_SGD` is `struct sgd *`, typed by its callee -- it is `SGD_delete`'s
 * only argument, which is CLAUDE.md's rank 2.  `V17RXS_PTR_0030` goes to
 * `sysdep_free` and so is neutral in every way except that it is a pointer the
 * state owns; `sysdep_free` types nothing.
 */
#define V17RXS_SGD		0x2c
#define V17RXS_PTR_0030		0x30

/*
 * The descrambler, `DescrambleDataV17`'s only subject.  `struct fpm_sdm *`,
 * typed by its callee: the function adds 0x4f8c to the state pointer and
 * tail-jumps to `SDM_descrambler`, whose first argument is that type.
 *
 * It tiles, which is a second confirmation of the offset: `sizeof(struct
 * fpm_sdm)` is 0x18 (from `fpm_sdm.h`, unrelated to V.17), and 0x4f8c +
 * 0x18 is 0x4fa4, exactly `V17RXS_BUF_MRF`.  The equaliser ends at 0x4f88,
 * so the four bytes there are the only gap in the whole tail, below this
 * object rather than above it.
 *
 * The scrambler is not here: `ScrambleDataV17` reaches `V17FP_SDM` in the
 * transmitter's private block instead, the two-instances split of F8850
 * showing up a third time.
 */
#define V17RXS_SDM		0x4f8c

/*
 * The recoverer's output bound, from the object's own loop guard, and the
 * count `DemodDataV17` reports through the author's own "ERROR: SRE buffer
 * violation!(%d)" when it is exceeded.
 *
 * It is the buffer size, confirmed rather than merely bounded:
 * `V17RX_create` allocates `V17RXS_BUF_SRE` with 328 bytes, exactly 0xa4
 * `short`, so the guard and the allocation are the same number read two
 * ways.  The constructor then zeroes only the first 160 of the 164
 * (deviation D1225).  Finding F9472.
 */
#define V17RXS_SRE_MAX		0xa4

/*
 * The four FPM objects the receive chain runs, every one typed by the
 * function it is handed to: `DemodDataV17` computes each of these four
 * addresses and passes it to exactly one module (CLAUDE.md rank 2).
 *
 * And the four tile the block exactly, a second, independent confirmation
 * of all four offsets at once: each boundary is the previous object's own
 * `sizeof`, taken from the four headers that model them independently --
 * `V17RXS_AGC` = `V17RXS_MRF` + `sizeof(struct fpm_mrf)`, and so on through
 * `V17RXS_SRE` and `V17RXS_FSE`, ending exactly at the equaliser's own
 * `sizeof` below the buffers.  Four additions in one function agreeing with
 * four independently derived struct sizes, with no gap anywhere, is not a
 * coincidence that survives one offset being wrong -- and it explains the
 * block's size: the equaliser alone is 19.5 KB of it.  Finding F8854.
 */
#define V17RXS_MRF		0x98	/* struct fpm_mrf  -> FPM_MRF_filter  */
#define V17RXS_AGC		0xb4	/* struct fpm_agc  -> FPM_AGC_agc     */
#define V17RXS_SRE		0xe0	/* struct fpm_sre  -> FPM_SRE_recover */
#define V17RXS_FSE		0x170	/* struct fpm_fse  -> FPM_FSE_receive */

/*
 * The two halves of the carrier verdict, ANDed together by all three of
 * `CarrierDetectV17`, `QualityDetectV17` and `DataCarrierDetectV17`.
 *
 * The first half is the AGC's own `signal` flag: the tiling above places it
 * at `V17RXS_AGC` + `offsetof(struct fpm_agc, signal)`, which `fpm_agc.h`
 * describes as "more than half the blocks in the last call were above the
 * gate" -- exactly what a carrier verdict wants, and `FPM_AGC_agc` is the
 * only writer.  So this is a field of a modelled struct, reached the long
 * way round because the enclosing block is not modelled (finding F8854).
 *
 * The width is not the same at every site, and that is the object's and
 * not ours: `CarrierDetectV17` reads it with a 32-bit load (what the `int`
 * field calls for) while the other two read it `movswl`.  Both are
 * forced -- an `int` cannot produce `movswl` and a `short` cannot produce a
 * 32-bit load -- so the three functions did not share a declaration, and
 * `src/fax/v17.c` spells each site the way the object does rather than
 * picking one.
 *
 * It is observable in `CarrierDetectV17` alone: the other two narrow the
 * AND back to a `short`, which depends on nothing above bit 15, and on any
 * state a real receiver can reach it is observable nowhere at all, since
 * `signal` only ever holds 0 or 1 -- so `t_v17fax.c` separates it with a
 * synthetic value and says so.  Finding F8853.
 */
#define V17RXS_AGC_SIGNAL	(V17RXS_AGC + 0x1c)
#define V17RXS_INT_0120		0x120

/*
 * A short compared against 999, and required to exceed it, before either
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
 * 0x3fff.  `GetSNRV17` reads the same field, unsigned where every other
 * site reads it signed -- a free extension there (the difference is 65536
 * and the result is truncated back to a short), reproduced as the local's
 * declared type rather than reasoned about.
 */
#define V17RXS_DEC_ERROR	0x1c2
#define V17RXS_DEC_ERROR_MAX	0x3fff

/*
 * The other threshold on the same field, and the author names this one
 * too: `RxHdxIdleV17` compares it signed, and on the at-or-below arm
 * advances the machine and prints "Decision error is small back to DATA
 * mode !!!" -- so "small" is the author's word for this side of the
 * constant, exactly as "too big" is his word for the other side of
 * `V17RXS_DEC_ERROR_MAX`.  The two are not independent: 0x1fff is one less
 * than half `V17RXS_DEC_ERROR_MAX` + 1, recorded as an observation and not
 * used to name anything.
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
 * The threshold +0x4fb0 is read by `QualityDetectV17`'s comparison and is
 * named `struct v17rx_state::quality_threshold` (Batch 25); the latch
 * +0x4fb2 is written by that comparison and read by nothing in the object,
 * so it stays neutral.  The offset constants keep their spelling because the
 * tests and `class1.c` reach the fields through them.
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
 * `V17TX_modem`'s inner call, and it is not the same signature.
 *
 * Four slots written, as on the receive side, and the first three are the
 * instance and the caller's two buffers unchanged -- but the fourth is the
 * address of a `short` local, not the caller's count.  So the
 * transmitter's slot is handed a per-call budget the caller never sees,
 * and the caller's `count` is read once before the loop and written once
 * after it.
 *
 * `in` is not advanced between iterations and `out` is: both are the
 * object's, the input pointer reloaded unchanged every time round while
 * the output pointer accumulates `2 * got`.  The result is sign-extended
 * before it is added to the running total, so it is `short` and that is
 * forced.
 *
 * `in` is `unsigned short *` because `FIFO_write` -- which the other arm
 * hands the very same pointer to -- declares its source that way.  Rank
 * 2, a callee that types it, and not usage inference.
 */
typedef short (*v17tx_process_fn)(void *modem, unsigned short *in, short *out,
				  short *budget);

/* ------------------------------------------------------------------------ */
/* The functions                                                            */

/**
 * @brief Drive the V.17 receiver until it stops consuming.
 *
 * `count` is in/out and changes meaning across the call: on entry it is the
 * number of input samples, on return the total number of outputs produced.
 * The loop re-reads `*count` after each inner call through the dispatch
 * slot, advances `in` by what was consumed and `out` by what was returned,
 * and stops when `*count` reaches zero.  The running total is a `short`
 * and wraps above 32767, which the object forces and this reproduces.
 * Clears `V17RX_FLAG_ERROR` in the result word before the loop starts.
 *
 * @param modem  The V.17 modem object.
 * @param in     Input samples.
 * @param out    Destination for demodulated bits (as the dispatch slot
 *               writes them).
 * @param count  In: number of input samples. Out: total outputs produced.
 * @return The receive instance's result word (see #V17RX_OBJ_RESULT).
 */
int V17RX_modem(void *modem, short *in, short *out, unsigned short *count);

/**
 * @brief The `V17RX_STATE_ERROR` handler.
 *
 * Raises `V17RX_FLAG_ERROR`, runs the block through the demodulator anyway
 * so the filters keep their history, and consumes it.  Nothing here
 * advances the state, so the machine stays in it until something outside
 * re-installs another handler.  The flag is a one-shot: `V17RX_modem`
 * clears it at the top of every block.  `RxHdxErrorV21` and
 * `RxHdxErrorV29` are the same shape over a different flag-byte offset.
 *
 * @param modem  The V.17 modem object.
 * @param in     Input samples.
 * @param out    Destination for demodulated bits.
 * @param count  In/out sample count, as `V17RX_modem` uses it.
 * @return The number of outputs produced.
 */
short RxHdxErrorV17(void *modem, short *in, short *out, unsigned short *count);

/**
 * @brief The `V17RX_STATE_DATA` handler: demodulate while the carrier is
 *        up, descramble, and grade what came out.
 *
 * Not `RxHdxDataV21`'s shape: it descrambles, grades with
 * `QualityDetectV17`, and does not advance the state on the carrier-gone
 * arm -- it just clears `V17RX_FLAG_CARRIER` and returns, where V.21's
 * calls `RxNextStateV21`.  The carrier flag is raised unconditionally on
 * entry and lowered again on the deny arm, so a caller reading the result
 * byte between two handlers in one block sees the raised bit.  The
 * demodulating arm's gate has a second term, `V17RXC_INT_0008`, which
 * nothing in the object writes -- so it is reached only when something
 * outside has left that field zero.  Returns `n` or zero, branchlessly:
 * `n & -(quality != V17_QUALITY_UNRELIABLE)`.
 *
 * @param modem  The V.17 modem object.
 * @param in     Input samples.
 * @param out    Destination for demodulated bits.
 * @param count  In/out sample count, as `V17RX_modem` uses it.
 * @return The number of outputs produced, or 0 if the quality grade was
 *         unreliable.
 */
short RxHdxDataV17(void *modem, short *in, short *out, unsigned short *count);

/**
 * @brief Build the V.17 receive instance, its control block and its 20 KB
 *        demodulator state.
 *
 * `modem` NULL allocates a new instance; a caller-supplied one is reused,
 * and so are its two sub-blocks if their pointers are non-NULL.  `params`
 * NULL takes the library's `V17RX_CFG`.  `params` is `struct v17rx_cfg *`
 * because the object copies one onto the instance's own head byte for
 * byte -- every field this function reads back afterward lines up with
 * that struct (finding F9470).  Cannot fail and cannot report failure:
 * eight allocations, none checked (deviation D1223).
 *
 * @param modem   NULL to allocate a new instance, or an existing one to
 *                reinitialise.
 * @param params  Configuration, or NULL for the library's `V17RX_CFG`.
 * @return The receive instance.
 */
void *V17RX_create(void *modem, const struct v17rx_cfg *params);

/**
 * @brief Tear the V.17 receive instance down.
 *
 * Fifteen releases in the object's order: the demodulator state's own
 * sub-objects first (an `SGD`, `V17RXS_PTR_0030`, the equaliser, the
 * recoverer, the resampler, the two chained buffers, then the state block
 * itself), then the control block's (both tone detectors, the notch, both
 * scratch buffers, then the control block), and the instance last as a
 * sibling `jmp`.
 *
 * @param modem  The V.17 modem object.
 */
void V17RX_delete(void *modem);

/**
 * @brief Tear the V.17 transmit instance down.
 *
 * Seven releases, in the object's order: the shaper and the two things the
 * private block owns, then the `SGD` and the `fax_fifo` the block at
 * `V17TX_OBJ_PARAMS` owns and that block itself, then the instance as a
 * sibling `jmp`.
 *
 * @param modem  The V.17 modem object.
 */
void V17TX_delete(void *modem);

/**
 * @brief Drive the V.17 transmitter for one caller block.
 *
 * Two arms on the way in, chosen by `V17TXP_INT_0008`: zero queues the
 * caller's `count` words through `FIFO_write` and remembers how many it
 * took; non-zero remembers `count` itself and touches the FIFO not at
 * all.  What is remembered is compared against `*count` after the loop,
 * and a mismatch sets `V17TX_RESULT_B1_BIT1` and writes
 * `V17TX_RESULT_BYTE_09`.  The loop is a `do`/`while` on a local budget
 * (see #v17tx_process_fn) starting at `V17TX_MODEM_BUDGET`, not on the
 * caller's count; `count` is in/out exactly as `V17RX_modem`'s is, and the
 * running total is a `short` that wraps, reproduced as the object forces
 * it.
 *
 * @param modem  The V.17 modem object.
 * @param in     Input data words to transmit.
 * @param out    Destination for the shaped output samples.
 * @param count  In: number of input words. Out: total samples produced.
 * @return The transmit instance's result word (see #V17TX_OBJ_RESULT).
 */
int V17TX_modem(void *modem, unsigned short *in, short *out,
		unsigned short *count);

/**
 * @brief Reconfigure the V.17 receive instance in place.
 *
 * `arg == NULL` returns 0 and touches nothing.  Otherwise: `cfg->int_0008`
 * (the instance's own head, `struct v17rx_cfg`) is unconditionally set
 * from `arg->int_0004`; `V17RXC_INT_0008` is set from
 * `V17RXCTL_SET_CTL_INT_0008` in `arg->flags_0d`; `V17RXCTL_REINIT` (also
 * in `flags_0d`) copies `arg->short_train` into `cfg->short_train` and then
 * calls
 * `V17RX_create(modem, modem)`; and regardless of which of those branches
 * ran, `arg->flags_0c` clears `V17RXS_INT_0000`/`V17RXS_INT_0010` per
 * `V17RXCTL_CLEAR_STATE0`/`_STATE10`.
 *
 * The `V17RX_create(modem, modem)` call is a deliberate self-referential
 * reinit, not an aliasing accident: `V17RX_create`'s own header (finding
 * F9470) establishes that the receive instance's head, byte for byte, IS a
 * `struct v17rx_cfg`, so this reads the instance's own current
 * configuration as its "new" one (an identity copy except for the two
 * fields this function just updated) and reruns the rest of construction.
 *
 * @param modem  The V.17 modem object.
 * @param arg    The reconfiguration request, or NULL to do nothing.
 * @return 0 if `arg` is NULL (nothing done), 1 otherwise.
 */
int V17RX_control(void *modem, const struct v17rx_ctl *arg);

/**
 * @brief Fill a status block from the V.17 receive instance.
 *
 * Same NULL guard and return convention as `V17TX_status`, and the same
 * block, but a different write set: writes +0x00, +0x02, +0x04, +0x06,
 * +0x08, +0x0a, +0x0e, +0x10, +0x12, +0x14 and +0x15, leaving +0x0c, +0x16
 * and +0x18 alone (`V17TX_status` writes +0x0c and +0x18 and skips +0x0e;
 * both skip +0x16, the field `v32_status` itself annotates "not written").
 * Where `V17TX_status` assigns `flags` outright, this builds it one bit at
 * a time over four stores, each separated by an aliasing load of
 * `V17RX_OBJ_STATE` -- deterministic in the end, so the intermediate
 * stores are not claimed as individually observable.
 *
 * @param modem   The V.17 modem object.
 * @param status  Destination status block, or NULL to do nothing.
 * @return 0 if `status` is NULL (nothing filled), 1 otherwise.
 */
int V17RX_status(void *modem, struct v17_status *status);

/**
 * @brief Scramble `count` words in place, through the transmitter's
 *        scrambler.
 *
 * A two-instruction adapter and a tail call to `SDM_scrambler`, retargeted
 * at `V17TX_OBJ_FP` + `V17FP_SDM`.  The data pointer is passed straight
 * through.
 *
 * @param modem  The V.17 modem object.
 * @param data   `count` words to scramble in place.
 * @param count  Number of words.
 */
void ScrambleDataV17(void *modem, unsigned short *data, unsigned short count);

/**
 * @brief The mirror image of ScrambleDataV17(), through the receiver's
 *        descrambler at `V17RXS_SDM`.
 *
 * Not the same object as the scrambler's, and not on the same instance:
 * this one reaches `V17RX_OBJ_STATE` + 0x4f8c and the other reaches
 * `V17TX_OBJ_FP` + 0x1c (finding F8850, the two-instances split).
 *
 * @param modem  The V.17 modem object.
 * @param data   `count` words to descramble in place.
 * @param count  Number of words.
 */
void DescrambleDataV17(void *modem, unsigned short *data,
		       unsigned short count);

/**
 * @brief One block through the V.17 receive chain: gain control, an
 *        optional tone pre-pass, resample, recover the symbol timing,
 *        equalise and slice.
 *
 * The pre-pass can abandon the whole call, exactly as `DemodDataV29`'s
 * does: while `V17RXC_SHORT_0018` is zero the input is copied into
 * `V17RXC_SCRATCH`, a tone is notched out of the copy, and if the tone
 * detector fires the function returns zero without touching the
 * resampler, the recoverer or the equaliser (the gain control has already
 * run over the caller's buffer by then, and its effect stands).  Unlike
 * `DemodDataV29`'s equivalent loop, this copy does not halve -- a plain
 * 16-bit move with no shift (finding F9103).  `signal` is the AGC's own
 * field, read out of `%eax` after the `void` `FPM_AGC_agc` call because
 * that function's last store before its `ret` is to that field (findings
 * F8875/D1035/D1091).
 *
 * @param modem  The V.17 modem object.
 * @param in     Input samples for this block.
 * @param bits   Destination for the decoded bits.
 * @param count  Number of input samples.
 * @return The number of bits produced, or 0 on the pre-pass's early exit.
 */
unsigned short DemodDataV17(void *modem, short *in, unsigned short *bits,
			    unsigned short count);

/**
 * @brief Load the transmit scrambler's shift register.
 *
 * See `V17FP_SDM`: this is `struct fpm_sdm::reg` in the transmitter's
 * private block, and nothing else in the function.
 *
 * @param modem  The V.17 modem object.
 * @param seed   The value to load into the shift register.
 */
void SeedScramblerV17(void *modem, unsigned int seed);

/**
 * @brief Select one of the V.17 transmitter's three encoders.
 *
 * For `V17_ENCODER_DIF`/`V17_ENCODER_TCM` also sets the short at
 * `V17FP_SMC_SHORT_06` from `arg`; `V17_ENCODER_ABS` stores the selector
 * alone and returns (the object's own asymmetry).  Any `which` outside
 * 0..2 does nothing at all -- there is no default arm, just a return.
 *
 * @param modem  The V.17 modem object.
 * @param which  One of #V17_ENCODER_DIF, #V17_ENCODER_ABS,
 *               #V17_ENCODER_TCM; anything else is ignored.
 * @param arg    Value for `V17FP_SMC_SHORT_06`, used for DIF and TCM only.
 */
void SetEncoderV17(void *modem, short which, short arg);

/**
 * @brief Fill a status block from the V.17 transmit instance.
 *
 * `params` is not identified as a specific type: it is read at +0x00,
 * +0x02, +0x10 and +0x18, and both candidates -- the transmitter's private
 * block (`V17TX_OBJ_FP`) and the parameter block (`V17TX_OBJ_PARAMS`) --
 * have room for all four without contradiction, so the parameter stays
 * `void *`.  `status + 0x14` is cleared of its low two bits and then
 * overwritten outright a few instructions later, a dead store the object
 * itself makes (because the intervening load of `params + 0x10` may alias
 * it) and reproduces here for the same reason; the consequence -- the
 * caller's bits 2..7 of that byte are destroyed -- is deviation D1032.
 *
 * @param params  The V.17 transmit instance (identity not established
 *                further than `void *`).
 * @param status  Destination status block, or NULL to do nothing.
 * @return 0 if `status` is NULL (nothing filled), 1 otherwise.
 */
int V17TX_status(void *params, struct v17_status *status);

/*
 * `V17TX_control` (below) settles what the note above left open: `params`
 * is the top-level TX handle, the same object `V17TX_create`/
 * `V17TX_control` take as their own first argument -- neither
 * `V17TX_OBJ_FP`'s private block nor `V17TX_OBJ_PARAMS`'s parameter block.
 * `V17TX_control` writes the same four offsets this function reads,
 * directly on its own first argument: +0x00/+0x02 are `struct
 * v17tx_cfg::protocol`/`::bitrate`; +0x10's bit 2 is the same
 * `V17_STATUS_FLAG_04` this function reads out, set by `V17TX_control`'s
 * own `ctl0` bit 2; +0x18 is `struct v17tx_cfg::int_0018`, written from
 * `V17TX_control`'s own `int_0010` request field.  Finding F10107.
 */

/*
 * `V17TX_control` (below) applies this request in five effects, none
 * exclusive of the others:
 * - `int_0004` is copied into the handle's `struct v17tx_cfg::int_0008`
 *   (the same shape `V29RX_control`'s `+0x04` -> `V29_OBJ_INT_0008` copy
 *   takes, finding F10103);
 * - `scale_mul`, multiplied by `V17TX_PPS_SCALE[mode]` (the transmit rate
 *   index, read back from `V17TXP_MODE`), becomes the pulse shaper's
 *   `scale` -- a runtime gain override on top of the constructor's own
 *   per-rate table lookup.  The object writes `scale` twice, the caller's
 *   raw value first and the scaled product second; the first write is
 *   dead and reproduced rather than simplified away (the same
 *   provably-dead-but-aliased-store shape D1032/F8878 record for the two
 *   sibling `*TX_status` functions);
 * - `int_0010` is copied into the handle's `struct v17tx_cfg::int_0018`
 *   -- the same field `V17TX_create` reads back into `V17TXP_INT_000C`
 *   on (re)initialisation, so setting it here and then triggering the
 *   reinit bit below is how a caller changes it;
 * - `ctl0` bit 2 (`V17TXCTL_CTL0_BIT2`) sets `V17_STATUS_FLAG_04` in the
 *   handle's own +0x10, the identical bit `V17TX_status` reads back as
 *   `flags` bit 2;
 * - `ctl1` bit 4 (`V17TXCTL_CTL1_BIT4`) sets the parameter block's
 *   `V17TXP_INT_0008` to 0 or 1; bit 1 (`V17TXCTL_CTL1_BIT1`), if set,
 *   calls `V17TX_create(fp, fp)` -- the same self-referential reinit
 *   shape `V29RX_control`/`V29TX_control` use.
 *
 * The request type is new, on the same footing as `V29RX_control`'s:
 * nothing else reconstructed reads it, so every field below is
 * established from that function alone.  Finding F10107.
 *
 * `scale_mul` (+0x08) takes its name from the sibling that already has
 * one: `v27fax.h`'s `struct v27tx_ctl::scale_mul` is the same field of
 * the same five-effect request shape (`class1tx.c`'s own comment on
 * `V17TX_CTL`/`V27TX_CTL`/`V29TX_CTL` says the three types are matched
 * field-by-field), so this is the established name for the role.  Rank
 * 2, a typed destination, per the `scale` derivation above.
 * `v29fax.h`'s own copy of this shape, `struct
 * v29tx_control_req::int_0008`, carries the identical comment and is not
 * yet renamed to match -- V.29 has not had a dedicated field-naming pass
 * in this project phase, and its own copy is a candidate for whoever
 * visits it next.  Finding F10144.
 */
struct v17tx_control_req {
	unsigned char	pad_0000[0x04];
	int		int_0004;	/* +0x04 -> handle's v17tx_cfg::int_0008 */
	int		scale_mul;	/* +0x08 -> fpm_pps_cfg::scale, * table  */
	unsigned char	ctl0;		/* +0x0c */
	unsigned char	ctl1;		/* +0x0d */
	int		int_0010;	/* +0x10 -> handle's v17tx_cfg::int_0018.
					 * The 2-byte gap `ctl1` leaves ahead of
					 * this 4-byte-aligned field used to be
					 * a named `pad_000e[0x02]`; removed by
					 * the pad-region removal audit
					 * (F10145) -- unlike `pad_0000` above
					 * (a real per-modulation bit-rate
					 * placeholder, `class1tx.c`'s own
					 * `V17TX_CTL`), nothing establishes any
					 * content for these two bytes and
					 * V17TX_control itself never reads
					 * them, so the compiler's own
					 * alignment reproduces the gap exactly.
					 * See V17TX_CTLREQ_ASSERT_OFF below   */
};

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V17TX_CTLREQ_ASSERT_OFF(field, off) \
	typedef char v17tx_control_req_off_##field[ \
		((int)__builtin_offsetof(struct v17tx_control_req, field) \
			== (off)) ? 1 : -1]
V17TX_CTLREQ_ASSERT_OFF(int_0010, 0x10);
typedef char v17tx_control_req_size[
	(sizeof(struct v17tx_control_req) == 0x14) ? 1 : -1];
#endif

/* Bits of `ctl0`, named by value; see V17TX_control's own derivation. */
#define V17TXCTL_CTL0_BIT2	(1 << 2)	/* -> V17_STATUS_FLAG_04 of the
						   handle's own +0x10, read back
						   by V17TX_status            */

/* Bits of `ctl1`, named by value. */
#define V17TXCTL_CTL1_BIT1	(1 << 1)	/* re-runs V17TX_create        */
#define V17TXCTL_CTL1_BIT4	(1 << 4)	/* -> V17TXP_INT_0008 (0 or 1) */

/**
 * @brief Apply a live reconfiguration request to the V.17 transmitter.
 *
 * See `struct v17tx_control_req`'s own comment above for the five
 * effects a non-NULL request has.
 *
 * @param fp   The V.17 transmit handle.
 * @param req  The reconfiguration request, or NULL to do nothing.
 * @return 0 if `req` is NULL (nothing done), 1 otherwise.
 */
int V17TX_control(void *fp, const struct v17tx_control_req *req);

/**
 * @brief Report whether a carrier is present.
 *
 * The verdict starts as the AND of the state's two carrier words and is
 * masked to bit 0, or forced to zero, only when the receiver is far enough
 * along to have a decoder error worth believing: the control block's
 * `V17RXC_INT_0010` set, the epoch found, and `V17RXS_SHORT_0094` past
 * 999.  Returns `int`, and nothing narrows it -- unlike `QualityDetectV17`
 * there is no `short` here to reproduce.
 *
 * @param modem  The V.17 modem object.
 * @return Non-zero if a carrier is present.
 */
int CarrierDetectV17(void *modem);

/**
 * @brief Report the carrier verdict for a block of `count` samples, and
 *        run the two watchdogs behind it.
 *
 * Has the same head as `CarrierDetectV17` -- the same three gates, arms
 * and doubled decoder-error test -- but only while the control block's
 * `V17RXC_SHORT_0020` is zero.  When it is not, the function takes a
 * completely different path: it copies the block into its own buffer,
 * gain-controls it, runs the second tone detector over it, and
 * accumulates the sample count for as long as that detector answers
 * `FPM_MTD_ABSENT` (see `V17RXC_OFFBAND`, not the silence timer it looks
 * like) -- crossing `V17RXC_OFFBAND_MAX` prints "V17: V21 Carrier
 * detected" and drops the verdict.  The energy watchdog (the "-8 dB"
 * message, `V17RXS_SHORT_4FB4`) runs on both paths.  `in` is not
 * modified: the gain control runs over the private copy at
 * `V17RXC_BUF2`, unlike `DemodDataV17`'s, which runs over the caller's.
 *
 * @param modem  The V.17 modem object.
 * @param in     The block's samples.
 * @param count  Number of samples.
 * @return The carrier verdict.
 */
short DataCarrierDetectV17(void *modem, const short *in, unsigned short count);

/**
 * @brief Report the carrier verdict, and maintain the smoothed decoder
 *        error behind it.
 *
 * The verdict is the same AND as `CarrierDetectV17`'s, narrowed to a
 * `short` (forced by the object).  A zero verdict is reported as
 * `V17_QUALITY_UNRELIABLE` and annotated.  The maintenance runs on every
 * call and is what makes this function stateful: block 0 seeds the
 * average, blocks 1..0x31 smooth it, block 0x32 judges it once, and every
 * block after 0x32 leaves it alone.
 *
 * @param modem  The V.17 modem object.
 * @return The carrier verdict, or #V17_QUALITY_UNRELIABLE.
 */
short QualityDetectV17(void *modem);

/**
 * @brief Report whether the receiver has found its epoch.
 * @param modem  The V.17 modem object.
 * @return 0 or 1, never anything else.
 */
int EpochDetectV17(void *modem);

/**
 * @brief Report the receiver's SNR.
 *
 * `13 - V17RXS_DEC_ERROR`, narrowed to a `short`.  See `V17RXS_DEC_ERROR`
 * for why the load is unsigned there and why that does not change the
 * answer.
 *
 * @param modem  The V.17 modem object.
 * @return The SNR estimate.
 */
short GetSNRV17(void *modem);

/**
 * @brief Copy the receiver's two coefficient arrays and its rate out into
 *        the instance's three save pointers.
 *
 * `V17_COEF_N` entries from each coefficient array.
 *
 * @param modem  The V.17 modem object.
 */
void StoreCoefV17(void *modem);

/**
 * @brief Put the saved rate back, and recompute `V17RXS_SHORT_01F8` from
 *        `V17RXS_USHORT_018C`.
 *
 * Not the inverse of `StoreCoefV17`: it restores the rate and not the
 * coefficients, which is what its name says.
 *
 * @param modem  The V.17 modem object.
 */
void Restore_rateV17(void *modem);

/* ------------------------------------------------------------------------ */
/* The half-duplex receive machine                                          */

/**
 * @brief Advance the V.17 receive machine one state, and re-point the
 *        dispatch slot.
 *
 * A seven-way switch on `V17RXC_STATE` with a compiler-generated jump
 * table, so there is no table to reproduce: the arms are 0..6 and
 * everything else -- including `V17RX_STATE_ERROR`, which has no arm of
 * its own -- lands on the default.  Each arm prints the name of the state
 * it is leaving, writes the next state, installs that state's handler in
 * `V17RXC_PROCESS` and seeds `V17RXC_COUNTDOWN`.
 *
 * What each arm does besides that, because none of it is uniform:
 *
 *   START      -> EPOCH_DET, 5 blocks.
 *   EPOCH_DET  -> PROTOCOL.  Calls `Restore_rateV17` when `V17RXC_INT_0010`
 *                 is set and takes 1 block if it did and 62 if it did not,
 *                 and then steps the gain control's level smoother:
 *                 `cfg.alpha` and `cfg.beta` are both advanced by one
 *                 `short`, taking `AGCv17_CFG`'s pair from {0x4000, 0x4000}
 *                 to {0x7333, 0x0ccd} -- 0.5/0.5 to 0.9/0.1 in Q15,
 *                 acquisition to tracking.  See deviation D1216 for what a
 *                 second visit would do.
 *   PROTOCOL   -> SCRAM when `V17RXC_INT_0010` is set, else BRIDGE with a
 *                 call to `StoreCoefV17`.  Both take 1 block.  The SCRAM
 *                 arm is the one transition of the seven that does not
 *                 touch `V17RX_OBJ_RESULT_B2` (deviation D1213).
 *   BRIDGE     -> SCRAM, 1 block.
 *   SCRAM      -> DATA, 0 blocks, after `FPM_AGC_Freeze`.
 *   DATA       -> IDLE, 0 blocks, and clears `V17RXC_INT_0008`.  Unreachable
 *                 in the object: see deviation D1210.
 *   IDLE       -> DATA, and it is the one arm that does not seed the
 *                 countdown (deviation D1215).  It reports the rate ladder
 *                 instead.
 *   default      Reports `V17RX_STATUS_DEFAULT` with the state as a `%d`,
 *                installs nothing and does not change the state.
 *
 * `V17RX_FLAG_DATA` is set by exactly the two arms that install
 * `RxHdxDataV17` and cleared by every other one, which is what names it.
 *
 * @param modem  The V.17 modem object.
 */
void RxNextStateV17(void *modem);

/**
 * @brief The `V17RX_STATE_IDLE` handler: demodulate, drop the carrier
 *        flag, re-test it, and go back to DATA once the decoder error is
 *        small again.
 *
 * `DemodDataV17`'s return is discarded and the function returns a literal
 * zero on every path, so a block spent here produces no output words at
 * all even though the equaliser ran.  The carrier flag is cleared and
 * then set again if and only if `CarrierDetectV17` agrees, the same
 * clear-then-set `RxHdxStartV17` does (and not what the three training
 * handlers do).  The second carrier test re-reads the flag byte from
 * memory rather than reusing the verdict, which is the compiler's rather
 * than a second question: the byte was just stored through a character
 * type.
 *
 * @param modem  The V.17 modem object.
 * @param in     Input samples.
 * @param out    Destination for demodulated bits.
 * @param count  In/out sample count, as `V17RX_modem` uses it.
 * @return Always 0.
 */
short RxHdxIdleV17(void *modem, short *in, short *out, unsigned short *count);

/*
 * The three training states, which are one function compiled three times
 * and then one of the three with a tail on it.
 *
 * `RxHdxBridgeV17` and `RxHdxPrtcolV17` are byte-for-byte identical -- 210
 * bytes each, the same six relocation targets in the same order, and no
 * differing offset at all.  They are one body the author wrote twice or a
 * macro he expanded twice; nothing in the object distinguishes them and
 * this header does not pretend otherwise (finding F9444).  `RxHdxScramV17`
 * is the same body plus one thing: on the expiry path, before the SNR
 * test, it reads `V17RXC_RATE_CODE` and reports the matching
 * `V17RX_STATUS_RATE_*`, where the other two leave the status byte at
 * `V17RX_STATUS_CARRIER` -- the only difference, and the only thing a
 * test can use to tell the three apart.
 *
 * All three: demodulate, descramble, consume the block, and then either
 * install `RxHdxErrorV17` because the carrier has gone or count one block
 * off `V17RXC_COUNTDOWN`.  The return is `n` only on the transition; every
 * other path returns a literal zero, so the words the descrambler just
 * wrote are reported to `V17RX_modem` as none unless this was the last
 * block of the state (deviation D1214).  `V17RX_FLAG_LOW_SNR` is set here
 * and never cleared; only `RxHdxDataV17` clears it (deviation D1217).
 */

/**
 * @brief The `V17RX_STATE_SCRAM` handler.  See the training-states note
 *        above: reports the rate ladder on expiry, unlike its two
 *        byte-identical siblings.
 * @param modem  The V.17 modem object.
 * @param in     Input samples.
 * @param out    Destination for demodulated bits.
 * @param count  In/out sample count, as `V17RX_modem` uses it.
 * @return `n` on the transition to the next state, 0 otherwise.
 */
short RxHdxScramV17(void *modem, short *in, short *out, unsigned short *count);
/**
 * @brief The `V17RX_STATE_BRIDGE` handler.  Byte-for-byte identical to
 *        `RxHdxPrtcolV17`; see the training-states note above.
 * @param modem  The V.17 modem object.
 * @param in     Input samples.
 * @param out    Destination for demodulated bits.
 * @param count  In/out sample count, as `V17RX_modem` uses it.
 * @return `n` on the transition to the next state, 0 otherwise.
 */
short RxHdxBridgeV17(void *modem, short *in, short *out, unsigned short *count);
/**
 * @brief The `V17RX_STATE_PROTOCOL` handler.  Byte-for-byte identical to
 *        `RxHdxBridgeV17`; see the training-states note above.
 * @param modem  The V.17 modem object.
 * @param in     Input samples.
 * @param out    Destination for demodulated bits.
 * @param count  In/out sample count, as `V17RX_modem` uses it.
 * @return `n` on the transition to the next state, 0 otherwise.
 */
short RxHdxPrtcolV17(void *modem, short *in, short *out, unsigned short *count);

/**
 * @brief The `V17RX_STATE_EPOCH_DET` handler: demodulate and wait for the
 *        epoch, or for the clock.
 *
 * The same head as the three training handlers -- demodulate, consume,
 * carrier or error -- and then a short-circuit OR the object makes
 * visible: an expired countdown advances the machine without asking
 * whether the epoch was found.  Does not descramble, discards
 * `DemodDataV17`'s return, never touches `V17RX_FLAG_LOW_SNR`, and
 * returns zero on every path.  `EpochDetectV17`'s return is tested
 * sixteen bits wide here where `CarrierDetectV17`'s is tested at
 * thirty-two, so this translation unit's prototype for it returned a
 * `short` -- the two readings agree over 0 and 1, which is all that
 * function can return, so this header's single `int` declaration is kept
 * rather than split (finding F9445).
 *
 * @param modem  The V.17 modem object.
 * @param in     Input samples.
 * @param out    Destination for demodulated bits.
 * @param count  In/out sample count, as `V17RX_modem` uses it.
 * @return Always 0.
 */
short RxHdxEpochDetV17(void *modem, short *in, short *out,
		       unsigned short *count);

/**
 * @brief The `V17RX_STATE_START` handler, installed by `V17RX_create`.
 *
 * The only handler with no error arm -- three relocations where the
 * others have five or six.  Clears the carrier flag, reports
 * `V17RX_STATUS_START`, demodulates the block and discards the result,
 * and advances the machine if and only if a carrier appeared.  Nothing
 * here can reach `RxHdxErrorV17`, so a receiver that never hears a
 * carrier sits in START forever.
 *
 * @param modem  The V.17 modem object.
 * @param in     Input samples.
 * @param out    Destination for demodulated bits.
 * @param count  In/out sample count, as `V17RX_modem` uses it; always set
 *               to 0.
 * @return Always 0.
 */
short RxHdxStartV17(void *modem, short *in, short *out, unsigned short *count);

#endif /* DSPLIB_V17FAX_H */

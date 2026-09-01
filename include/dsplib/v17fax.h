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

struct fpm_sdm;

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
 * The int `V17RX_modem` returns, and the flag byte inside it.
 *
 * NEUTRAL, and see the two-instances note above: 0x29 is byte 1 of the 4
 * bytes at 0x28, so the object both modifies and returns the same word.  What
 * `V17RX_modem` does is clear one bit of it on entry and hand the rest back;
 * nothing traced writes it, so what it reports is unknown.  The bit is named
 * by its VALUE, per CLAUDE.md, and the value is the one the object encodes:
 * `andb $0xfd,0x29(%eax)`.
 */
#define V17RX_OBJ_RESULT	0x28
#define V17RX_OBJ_RESULT_B1	0x29
#define V17RX_RESULT_B1_BIT1	0x02

/*
 * A second bit of the same byte, and the only OTHER thing in this batch that
 * reads it: `V17RX_status` reports `(byte & 0x80) == 0` -- inverted -- into the
 * status block's +0x06.  Named by value, like its neighbour; the INVERSION is
 * the object's `sete` after a `testb`, so the field is true when the bit is
 * CLEAR and that is not a transcription slip.
 */
#define V17RX_RESULT_B1_BIT7	0x80

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
 * An int `CarrierDetectV17` and `DataCarrierDetectV17` both require to be
 * non-zero before they will look at the decoder error at all.  Neutral: what
 * it indicates is not established, only that it gates the carrier verdict.
 */
#define V17RXC_INT_0010		0x10

/*
 * A dispatch slot: `V17RX_modem` calls `*(fn *)(ctl + 0x14)` with its own
 * four arguments unchanged.  `call *0x14(%eax)` carries no relocation, so
 * this is a table entry planted at construction and NOT a symbol reference --
 * nothing in this batch can say which function lands here.
 */
#define V17RXC_PROCESS		0x14

/*
 * A short `DemodDataV17` tests: non-zero skips the whole tone-kill and
 * tone-detect front end and goes straight to the resampler.  Neutral.
 */
#define V17RXC_SHORT_0018	0x18

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
 * channel.  Whether that band is V.17's or V.21's depends on the config
 * `V17RX_create` gives the detector, and `V17RX_create` is not reconstructed,
 * so the name stays with what the verdict says rather than with the message.
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
 */
#define V17RXS_INT_0000		0x00
#define V17RXS_BYTE_001C	0x1c
#define V17RXS_001C_BIT0	0x01

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
 * "ERROR: SRE buffer violation!(%d)" when it is EXCEEDED.  Not a buffer size
 * this batch can confirm -- `V17RX_create` is not reconstructed -- only the
 * number the object compares against.
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
 * `V17RX_RESULT_B1_BIT1` in the word it will later return.
 */
int V17RX_modem(void *modem, short *in, short *out, unsigned short *count);

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

#endif /* DSPLIB_V17FAX_H */

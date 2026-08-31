/*
 * v29fax.h -- ITU-T V.29 (fax): the receiver's entry points and the two
 *             transmitter accessors that sit beside them.
 *
 * Eleven exported functions that do arithmetic of their own only in three
 * places.  The rest reach into a modem instance, pick a sub-object out of it,
 * and hand it to the module that owns it -- so what can be WRONG here is
 * almost always WHICH offset, and `test/unit/t_v29fax.c` is built around that.
 *
 * `tools/service.py` puts the whole file on the FAX side.
 *
 * ---------------------------------------------------------------------------
 * THE INSTANCE IS NOT MODELLED, AND MUST NOT BE
 *
 * `V29RX_create` (0x09b8b0, 2,127 bytes) is not reconstructed -- it is blocked
 * on 45 unwritten tables -- so nothing lays the receiver's block out and
 * naming its fields as a struct would be guessing.  This header follows the
 * ruling `include/dsplib/v22data.h` and `v17data.h` set out: the parameter is
 * `void *` and every offset is a named constant with the evidence beside it.
 *
 * FIVE SUB-OBJECTS ARE TYPED, because a callee types them and that is evidence
 * rank 2 -- and between them they account for EIGHT of the eleven fields this
 * file first wrote down as anonymous ints:
 *
 *     rx + 0x48   struct fpm_mrf     FPM_MRF_filter / FPM_MRF_free
 *     rx + 0x64   struct fpm_agc     FPM_AGC_agc          0x2c bytes
 *     rx + 0x90   struct fpm_sre     FPM_SRE_recover / FPM_SRE_free   0x90
 *     rx + 0x120  struct fpm_fse     FPM_FSE_receive / FPM_FSE_free   0x4e18
 *     det + 0x2c  struct fpm_agc     FPM_AGC_agc
 *
 * ---------------------------------------------------------------------------
 * THE SIZES ARE WHAT MAKES THE REST OF THE BLOCK READABLE, AND THIS IS THE
 * FINDING THAT MATTERS MOST HERE (F8874)
 *
 * Each of those structures has a MEASURED size and MEASURED field offsets --
 * `src/dsp/fpm_sre.c` asserts 0x90, `fpm_fse.h` puts its second scatter log's
 * count at +0x4e14 -- so a field of the receiver's block that lands inside one
 * of them is not an anonymous int at all.  Eight do:
 *
 *     rx + 0x80  = agc + 0x1c  = fpm_agc::signal      "more than half the
 *                                                      blocks in the last call
 *                                                      were above the gate"
 *     rx + 0xd0  = sre + 0x40  = fpm_sre::active      "the squelch let the PLL
 *                                                      run"
 *     rx + 0xd8  = sre + 0x48  = fpm_sre::adapt       "caller's enable for the
 *                                                      phase update; set by
 *                                                      init, never written by
 *                                                      recover"
 *     rx + 0x160 = fse + 0x40  = fpm_fse::lms_force
 *     rx + 0x164 = fse + 0x44  = fpm_fse::pll_on
 *     rx + 0x168 = fse + 0x48  = fpm_fse::tilt_on
 *     rx + 0x16c = fse + 0x4c  = fpm_fse::lms_on
 *     rx + 0x172 = fse + 0x52  = fpm_fse::mse         "smoothed squared
 *                                                      decision error"
 *
 * and every one of them reads back as the function that touches it:
 *
 *   - `CarrierDetectV29` is `sre.active & agc.signal` -- the symbol-clock PLL
 *     is running AND the gain control sees energy.  Two independent modules'
 *     own verdicts, ANDed.
 *   - `DemodDataV29` writes `sre.adapt`, `fse.pll_on` and `fse.lms_on` from
 *     that same carrier bit ANDed with three configuration flags, and clears
 *     `fse.tilt_on`.  `fpm_sre.h` says of `adapt` that it is "written here,
 *     read only by the caller" -- this IS that caller, and it is the only one
 *     reconstructed.
 *   - `mse` is what the author's own messages call the decoder error:
 *     `"V29 Decoder error too big... no carrier\n"` and `"V29 Dec error too
 *     big... unreliable data\n"`, both gated on it exceeding 0x3fff.  So the
 *     equaliser's mean squared error and the "decoder error" are one field,
 *     which is evidence rank 1 meeting evidence rank 2 on the same offset.
 *   - `GetSNRV29` is `14 - mse`, i.e. an SNR index read off the equaliser's
 *     error rather than off any measurement of its own.
 *
 * The constants below still state all eight offsets, because `t_v29fax.c`
 * pokes raw bytes and must not agree with `src/` by construction -- the same
 * argument `v29data.h` makes for keeping its own constants after modelling.
 * `v29.c` asserts the two statements against each other.
 *
 * ---------------------------------------------------------------------------
 * `V29RX_delete` IS THE BEST WITNESS TO THE LAYOUT AND IS TREATED AS ONE
 *
 * A delete function frees exactly the sub-objects the instance owns, in order,
 * so every offset below that appears in it is confirmed twice -- once by the
 * function that USES the sub-object and once by the function that RELEASES it:
 *
 *     rx + 0x120   FPM_FSE_free    <-> FPM_FSE_receive   in DemodDataV29
 *     rx + 0x90    FPM_SRE_free    <-> FPM_SRE_recover   in DemodDataV29
 *     rx + 0x48    FPM_MRF_free    <-> FPM_MRF_filter    in DemodDataV29
 *     rx + 0x4f58  sysdep_free     <-> the SRE's output / the FSE's input
 *     rx + 0x4f54  sysdep_free     <-> the MRF's output / the SRE's input
 *     det + 0x00   FPM_MTD_delete  <-> FPM_MTD_detect    in DemodDataV29
 *     det + 0x04   FPM_TONE_delete <-> FPM_TONE_kill     in DemodDataV29
 *     det + 0x18   sysdep_free     <-> that pair's scratch buffer
 *     det + 0x24   sysdep_free     <-> the V.21 detector's scratch buffer
 *     det + 0x20   FPM_MTD_delete  <-> FPM_MTD_detect    in DataCarrierDetectV29
 *
 * `det + 0x2c` is NOT freed, which is how we know the AGC there is embedded
 * rather than pointed at.  Neither is `rx + 0x64`.
 */

#ifndef DSPLIB_V29FAX_H
#define DSPLIB_V29FAX_H

struct fpm_agc;
struct fpm_fse;
struct fpm_mrf;
struct fpm_mtd;
struct fpm_sre;
struct fpm_tone;

/* ------------------------------------------------------------------------ */
/* The handles.                                                             */

/*
 * THERE ARE PROBABLY TWO OF THEM, AND NOTHING HERE ESTABLISHES THAT THERE ARE
 * NOT THREE.
 *
 * `V29TX_create` and `V29RX_create` are separate constructors, and no function
 * in this file touches both +0x24 and +0x50 -- so the transmit accessors and
 * the receive entry points may well be reading two different objects that
 * happen to be spelled the same way at the call site.  A parallel pass over
 * the V.21 side reports that `faxvmi.h` gives `v29tx` and `v29rx` separate
 * FAXVMI slots with a handle each, which is consistent with that.
 *
 * SO THE CONSTANTS BELOW ARE GROUPED BY WHICH FUNCTION READS THEM AND NOT
 * COLLECTED INTO A STRUCT.  Two of them would be wrong the moment the handles
 * turn out to be distinct, and no test in this tree could fail on it.
 */

/* --- the receive handle: V29RX_modem, V29RX_delete, and the detectors ---- */

/*
 * A status word.  `V29RX_modem` clears bit 9 of it on entry -- `andb $0xfd,
 * 0x19(%eax)`, which is GCC's narrowing of `&= ~0x200` on the int at +0x18 --
 * and returns the whole int after the demodulation loop.  NEUTRAL NAME on
 * purpose: nothing reconstructed writes any other bit of it, and no string in
 * `.rodata` names it, so what bit 9 MEANS is not established.
 */
#define V29_OBJ_STATUS		0x18
#define V29_STATUS_0200		0x0200

/* --- the transmit handle: SeedScramblerV29 and SetEncoderV29 ------------- */

/*
 * The transmitter's private block.  The same field `v29data.h` calls
 * `V29TX_OBJ_FP` and reaches from `TxNoCarrierV29`; the two readings are
 * independent and agree, which is also the reason to think this handle is the
 * TRANSMIT one and not the object the paragraph above's other constants sit
 * in.
 */
#define V29_OBJ_TX		0x24

/*
 * The carrier/tone detection block.  `V29RX_modem` calls through its +0x10,
 * `DemodDataV29` and `DataCarrierDetectV29` read its sub-objects, and
 * `V29RX_delete` releases five of them and then the block itself.
 */
#define V29_OBJ_DET		0x4c

/* The receiver's block.  Every V29RX_* offset below is relative to this. */
#define V29_OBJ_RX		0x50

/* ------------------------------------------------------------------------ */
/* The detection block, obj + 0x4c.                                         */

#define V29DET_MTD		0x00	/* struct fpm_mtd *                  */
#define V29DET_TONE		0x04	/* struct fpm_tone *                 */

/*
 * The gate on `DemodDataV29`'s pre-pass.  While it is ZERO the demodulator
 * first halves the block into the scratch buffer, notches it and runs the
 * tone detector, and abandons the whole call if that detector fires.  Neutral
 * name: what the flag records is not established, only what it gates.
 */
#define V29DET_GATE_14		0x14	/* short                             */

/* `count` shorts.  Written by DemodDataV29, read by TONE_kill and MTD_detect. */
#define V29DET_BUF		0x18	/* short *                           */

/*
 * The gate on `DataCarrierDetectV29`'s V.21 scan, in the same neutral sense as
 * V29DET_GATE_14: zero takes the plain "report the carrier bit" path.
 */
#define V29DET_GATE_1C		0x1c	/* short                             */

/*
 * The V.21 carrier detector, named from the author's own message -- the whole
 * chain below exists to produce `"V29: V21 Carrier detected\n"` (.rodata.str1.1
 * + 0x4e31) and nothing else reads any of it.  Evidence rank 1.
 */
#define V29DET_V21_MTD		0x20	/* struct fpm_mtd *                  */
#define V29DET_V21_BUF		0x24	/* short *, `count` shorts           */
#define V29DET_V21_SAMPLES	0x28	/* short: samples accumulated        */
/*
 * Latched to 1 the first time the receiver has no carrier, and never cleared
 * by anything reconstructed; while it is set every call runs the V.21 scan.
 */
#define V29DET_V21_ENABLE	0x2a	/* short                             */
#define V29DET_V21_AGC		0x2c	/* struct fpm_agc, EMBEDDED          */

/* `V29: V21 Carrier detected` fires once the accumulator passes this. */
#define V29DET_V21_THRESHOLD	0x4ff

/* ------------------------------------------------------------------------ */
/* The receiver's block, obj + 0x50.                                        */

/*
 * Three int flags the caller ANDs with the carrier bit before enabling three
 * adaptive loops.  THE DESTINATIONS ARE NAMED and the SOURCES ARE NOT: each
 * destination is a field of a modelled sub-object, each source is somewhere in
 * the block's first 0x48 bytes that nothing reconstructed writes.
 *
 *     sre.adapt   = signal & rx->f0004      the symbol-clock phase update
 *     fse.pll_on  = signal & rx->f0008      carrier recovery
 *     fse.lms_on  = signal & rx->f0020      coefficient adaptation
 */
#define V29RX_INT_0004		0x0004
#define V29RX_INT_0008		0x0008
#define V29RX_INT_0020		0x0020
#define V29RX_SRE_ADAPT		0x00d8	/* struct fpm_sre + 0x48             */
#define V29RX_FSE_PLL_ON	0x0164	/* struct fpm_fse + 0x44             */
#define V29RX_FSE_LMS_ON	0x016c	/* struct fpm_fse + 0x4c             */

/*
 * Cleared to zero by every `DemodDataV29` call: the 4-tap tilt filter is off
 * for the whole of V.29.  `fpm_fse.h` records that the filter is inert until
 * a datapump sets its coefficients, and this datapump does not -- it turns the
 * stage off instead, every block.
 */
#define V29RX_FSE_TILT_ON	0x0168	/* struct fpm_fse + 0x48             */

/*
 * Compared against 999 by `DataCarrierDetectV29` and by nothing else.  A
 * `short`: the object compares 16 bits and branches signed.
 */
#define V29RX_SHORT_0046	0x0046

#define V29RX_MRF		0x0048	/* struct fpm_mrf                    */

/*
 * The input AGC.  `signal` (+0x1c of `struct fpm_agc`, so rx + 0x80) is the
 * carrier bit every detector in this file reads; see the header note.
 */
#define V29RX_AGC		0x0064
#define V29RX_AGC_SIGNAL	0x0080

#define V29RX_SRE		0x0090	/* struct fpm_sre                    */

/*
 * `struct fpm_sre`'s `active` -- "the squelch let the PLL run", set by
 * `FPM_SRE_recover` itself.  `CarrierDetectV29` returns it ANDed with the
 * AGC's `signal`, and `QualityDetectV29` and `DataCarrierDetectV29` form the
 * same product: two modules' own verdicts and no third opinion.
 */
#define V29RX_SRE_ACTIVE	0x00d0	/* struct fpm_sre + 0x40             */

#define V29RX_FSE		0x0120	/* struct fpm_fse                    */

/*
 * `struct fpm_fse`'s `lms_force` -- "adapt even when the gate below says no".
 * `EpochDetectV29` reports whether it is non-zero, as 0 or 1, and is the only
 * thing reconstructed that touches it.
 *
 * WHY THE FUNCTION IS CALLED `Epoch` IS NOT ESTABLISHED.  The offset is: the
 * FSE starts at rx + 0x120 and `lms_force` is its +0x40.  What is NOT claimed
 * is that "epoch" and "forced adaptation" are the same idea; the name is the
 * author's, the field is `fpm_fse.h`'s, and nothing here reconciles them.
 */
#define V29RX_FSE_LMS_FORCE	0x0160	/* struct fpm_fse + 0x40             */

/*
 * `struct fpm_fse`'s `mse`, the equaliser's smoothed squared decision error --
 * and the field the author's own messages call the DECODER ERROR.  The two,
 * both gated on it exceeding 0x3fff, are `"V29 Decoder error too big... no
 * carrier\n"` (.rodata.str1.4 + 0x12d88) and `"V29 Dec error too big...
 * unreliable data\n"` (+ 0x12db4).  Evidence rank 1 and rank 2 agreeing on
 * one offset, which is as settled as anything in this file gets.
 *
 * It is a SIGNED short and that is forced, not chosen: `QualityDetectV29`
 * loads it `movswl` and multiplies the 32-bit result by 0xccd.  `GetSNRV29`
 * loads it `movzwl`, which is the free case -- its result is truncated to 16
 * bits by a `cwtl` before it leaves the function.
 */
#define V29RX_FSE_MSE		0x0172	/* struct fpm_fse + 0x52             */

/* The MRF's output and the SRE's input; `count` shorts, freed by delete. */
#define V29RX_BUF_MRF		0x4f54	/* short *                           */
/* The SRE's output and the FSE's input; likewise. */
#define V29RX_BUF_SRE		0x4f58	/* short *                           */

/*
 * The smoothed decoder error and the count of blocks that have gone into it.
 * `QualityDetectV29` is the only writer:  avg = 0.9 * avg + 0.1 * error, in
 * Q15 with round-to-nearest, for blocks 1 through 0x31.
 */
#define V29RX_DEC_ERROR_AVG	0x4f5c	/* short                             */
#define V29RX_DEC_ERROR_N	0x4f5e	/* short                             */

/*
 * At block 0x32 the average is compared against this, once, and +0x4f62 is
 * set to 1 if it did NOT come out higher.  Both keep cautious names: nothing
 * reconstructed writes the limit or reads the verdict, so the polarity is
 * recorded here and not asserted in a name.
 */
#define V29RX_DEC_ERROR_LIMIT	0x4f60	/* short                             */
#define V29RX_SHORT_4F62	0x4f62	/* short                             */

/*
 * The gate on `DataCarrierDetectV29`'s energy-drop check.  Neutral, as above.
 */
#define V29RX_SHORT_4F64	0x4f64	/* short                             */

/*
 * The energy-drop detector, named from `"sudden energy drop > 8[dB], no
 * carrier"` (.rodata.str1.4 + 0x12d60):  the block's RMS is compared against
 * 0x32fe/32768 = 0.39984 of the reference, which is -7.97 dB, and the
 * reference is replaced by the RMS every second block.
 */
#define V29RX_RMS_REF		0x4f66	/* short                             */
#define V29RX_RMS_N		0x4f68	/* short                             */
#define V29RX_RMS_DROP_Q15	0x32fe
#define V29RX_RMS_SHIFT		15
/* The decoder error above which the receiver declares no carrier. */
#define V29RX_DEC_ERROR_MAX	0x3fff

/* ------------------------------------------------------------------------ */
/* The transmitter's private block, obj + 0x24.  See v29data.h for its shape. */

/*
 * `SeedScramblerV29` stores its second argument here as a full int.  It lands
 * in `struct v29tx`'s `pad_1c`, which is why it is spelled as an offset rather
 * than a field: v29data.h leaves +0x1c..+0x33 unmodelled because nothing it
 * read touched them, and one 32-bit store is not enough to model 24 bytes.
 * The name is the author's own, from the function.
 */
#define V29TXFP_SCRAMBLER_SEED	0x2c

/* ------------------------------------------------------------------------ */
/* The status report `V29TX_status` fills in.                               */

/*
 * THE SHAPE IS ESTABLISHED THREE TIMES OVER AND SIX OF THE FIELDS ARE NAMED.
 *
 * What this function writes, field for field:
 *
 *     +0x00  short  = handle->f00
 *     +0x02  short  = handle->f02
 *     +0x04 .. +0x0d  five shorts, all zeroed
 *     +0x0e  NOT WRITTEN -- the one gap in an otherwise solid run
 *     +0x10  short  = handle->f02, AGAIN, from a second load
 *     +0x12  short  = 0
 *     +0x14  byte   = handle->f10 & 0x04    (assigned, not merged; D1035)
 *     +0x15  byte  &= ~0x01
 *
 * `V17TX_status` (0x0a1bd0, 106 bytes) writes byte for byte the same sequence
 * plus one extra `int` copy at +0x18, which settles the SHAPE without either
 * reading being derived from the other.
 *
 * THE NAMES COME FROM `include/dsplib/v22status.h`, which is the same block
 * modelled from a different function -- `V22_status` -- and is CLAUDE.md's
 * evidence rank 2.  `struct v22_status` has `protocol` at +0x00, `tx_bps` at
 * +0x02, `rx_bps` at +0x04, `quality` at +0x06, two unnamed shorts, an
 * unmodelled +0x0c..+0x0f, `short_10`, `short_12`, `flags` at +0x14 and
 * `flags2` at +0x15.  Every offset this function touches lines up.
 *
 * AND +0x02 IS INDEPENDENTLY CONFIRMED BY A LITERAL: `V21TX_status` (0x0a2c00)
 * writes 0x12c -- 300 -- into it unconditionally, and V.21 is a 300 bit/s
 * modulation.  V.29's handle carries its own rate and this function copies it.
 *
 * NO `struct v29_status` IS DEFINED HERE, and that is a decision rather than
 * an omission.  `v22status.h` and `v32fpstat.h` already spell this block under
 * two tags; a third would make the drift worse, and unifying them is a change
 * to two files this pass does not own.  What is taken instead is the NAMING,
 * which costs nothing and cannot drift: the constants stay offsets against a
 * `void *`, as the rest of this header does.
 *
 * WHERE V.29 DIVERGES FROM V.22, AND IT IS WORTH KNOWING: `v22status.h`
 * records +0x10 as "written 0, read by nothing".  V.29 writes the BIT RATE
 * there.  So the field is not zero-by-definition, and it keeps a neutral name
 * on this side because two writers now disagree about what it carries.
 */
#define V29STAT_PROTOCOL	0x00	/* v22_status::protocol              */
#define V29STAT_TX_BPS		0x02	/* v22_status::tx_bps, in bit/s      */
#define V29STAT_ZERO_LO		0x04	/* +0x04 .. +0x0c, five shorts:
					 * rx_bps, quality and three more    */
#define V29STAT_ZERO_HI		0x0c
/* The bit rate again, from a second load of the same word.  See above. */
#define V29STAT_SHORT_10	0x10
#define V29STAT_SHORT_12	0x12	/* v22_status::short_12              */
#define V29STAT_FLAGS		0x14	/* v22_status::flags                 */
#define V29STAT_FLAGS2		0x15	/* v22_status::flags2                */
#define V29STAT_FLAGS_LOW2	0x03	/* the two bits +0x14 is cleared of  */
#define V29STAT_FLAGS2_BIT0	0x01	/* the one bit  +0x15 is cleared of  */

/*
 * The handle `v29tx_status` passes: the modem's +0x14, NOT its +0x24 -- so it
 * is not the transmitter's private block either, and it is a third thing this
 * file reaches without being able to say what owns it.
 */
#define V29TXS_PROTOCOL		0x00
#define V29TXS_BITRATE		0x02	/* copied to the report's +0x02 AND
					 * its +0x10; see V29STAT_TX_BPS     */
#define V29TXS_FLAGS_10		0x10
#define V29TXS_10_BIT2		0x04	/* the one bit that reaches the report */

/* ------------------------------------------------------------------------ */
/* The demodulator slot `V29RX_modem` calls through.                        */

/*
 * `call *0x10(%eax)` with `%eax` = the detection block.  A function pointer
 * inside a block whose other members are sub-objects, so it is spelled as an
 * offset and not as a vtable: nothing establishes that +0x00..+0x0c are
 * pointers of the same kind, and V29RX_delete proves they are not (they are
 * an MTD and a TONE).
 *
 * The signature is forced by the caller: four arguments pushed, the return
 * sign-extended from 16 bits with `cwtl`, the third argument advanced by the
 * return and the second by what the fourth was decremented by.
 */
#define V29DET_DEMOD		0x10

typedef short (*v29_demod_fn)(void *modem, short *in, short *out,
			      unsigned short *count);

/* ------------------------------------------------------------------------ */
/* The functions.                                                           */

/*
 * Report whether a carrier is up: the AGC's own signal bit ANDed with the
 * receiver's gate.  Both are `int` and the object loads both 32 bits wide.
 */
int CarrierDetectV29(void *modem);

/* Report whether the receiver's +0x160 is non-zero, as 0 or 1. */
int EpochDetectV29(void *modem);

/*
 * 14 minus the decoder's error metric, truncated to 16 bits.
 *
 * THE CONSTANT IS 14 AND V.17'S IS 13; that is the whole difference between
 * the two functions, and it is stated here because a reader who assumes a
 * shared implementation will get it wrong.
 */
short GetSNRV29(void *modem);

/* Store `seed` in the transmitter's block as a full int. */
void SeedScramblerV29(void *modem, int seed);

/*
 * Select the transmitter's encoder: 0 differential, 1 absolute.
 *
 * ANY OTHER VALUE WRITES NOTHING AND THE FUNCTION RETURNS HAVING DONE
 * NOTHING -- it is not clamped and it is not an error, the object simply
 * falls off the end of a two-armed comparison.
 *
 * The argument is loaded `movswl` and the 32-bit result is used, so `short`
 * is FORCED.  What it writes is `fpm_smc_cfg`'s `direct` -- the transmitter's
 * block puts `struct fpm_smc` at +0x34 and `direct` is that structure's +0x04
 * -- which is the second, independent statement that this is the differential
 * / absolute switch and not an index.
 */
void SetEncoderV29(void *modem, short which);

/*
 * Fill a status report from the transmitter object, and say whether there was
 * one to fill: 0 for a null pointer, 1 otherwise.  Nothing else is checked.
 */
int V29TX_status(void *tx, void *status);

/*
 * Release the receiver: nine sub-objects, in the order above, then the
 * detection block, then the instance itself.
 *
 * THE INSTANCE IS FREED UNCONDITIONALLY and by a sibling `jmp`, so a caller
 * that supplied the storage does not get it back.
 */
void V29RX_delete(void *modem);

/*
 * Demodulate until the receiver stops asking for input.
 *
 * `count` is IN AND OUT: on entry the number of input samples available, on
 * exit the number of output samples produced.  The loop calls the detection
 * block's +0x10 slot, advances `in` by what the slot consumed (the drop in
 * `*count`) and `out` by what it produced (the slot's return), and stops when
 * `*count` reaches zero.
 *
 * IT IS A do-while: the slot is called at least once even when `*count` is
 * zero on entry.  There is no test above the loop -- the object's only
 * instructions between the flag clear and the loop head are eleven bytes of
 * `-falign-loops` padding.
 *
 * The running total is a `short` and is re-truncated with `cwtl` on every
 * iteration, so a stream producing more than 32,767 samples in one call wraps.
 * Returns the instance's status word.
 */
int V29RX_modem(void *modem, short *in, short *out, unsigned short *count);

/*
 * `DemodDataV29` (0x0a5ff0, 398 bytes) IS NOT DECLARED HERE, because it is not
 * defined in `src/fax/v29.c`.  It is decoded -- finding F8883 carries the call
 * sequence, the argument types and the two forced type decisions -- and left
 * out for want of a fixture, not for want of a reading.  Its signature, when
 * it lands, is
 *
 *     unsigned short DemodDataV29(void *modem, short *in, unsigned short *out,
 *                                 unsigned short count);
 *
 * with `in` modified IN PLACE by the AGC and `out` typed by
 * `FPM_FSE_receive`'s own second parameter.
 */

/*
 * Decide whether a data carrier is present in this block.
 *
 * Three answers come out of one `int`: the carrier bit itself on the quiet
 * path, 0 when the V.21 detector or the energy-drop check says the carrier is
 * gone, and 1 when the V.21 scan ran and found nothing.
 */
int DataCarrierDetectV29(void *modem, short *in, unsigned short count);

/*
 * Fold this block's decoder error into the running average and report the
 * carrier bit -- or 2, if the carrier bit is clear.
 *
 * The average is first-order, Q15, round-to-nearest:
 *
 *     avg = (avg * 0x7333 + 0x4000) >> 15  +  (error * 0xccd + 0x4000) >> 15
 *
 * 0x7333/32768 is 0.89999 and 0xccd/32768 is 0.10001.  THE TWO WEIGHTS ARE
 * NOT INTERCHANGEABLE and swapping them is invisible to every codegen check
 * and to any single-block test -- finding F8790's shape -- so t_v29fax.c
 * drives it over many blocks and counts the trials that separate the swap.
 */
int QualityDetectV29(void *modem);

#define V29Q_AVG_KEEP		0x7333	/* weight on the running average     */
#define V29Q_AVG_NEW		0x0ccd	/* weight on this block's error      */
#define V29Q_AVG_ROUND		0x4000	/* the Q15 rounding term             */
#define V29Q_AVG_SHIFT		15
#define V29Q_AVG_BLOCKS		0x31	/* the last block folded in          */
#define V29Q_VERDICT_BLOCK	0x32	/* the block the limit is tested on  */
#define V29Q_NO_CARRIER		2	/* what a clear carrier bit reports  */

/* Above this many SRE outputs the receiver logs a buffer violation. */
#define V29RX_SRE_MAX		0xa4

#endif /* DSPLIB_V29FAX_H */

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
 * A status word: a status BYTE at +0x18 with a flags byte above it at +0x19,
 * read out together as one `int` and returned by `V29RX_modem`.  That is
 * `v21fax.h`'s +0x18/+0x19 shape and `v17fax.h`'s +0x20/+0x21 shape, and it
 * is why every flag below is spelled as a bit of the INT: the object reaches
 * them with `andb`/`orb`/`testb` on 0x19, which is GCC's own narrowing of a
 * 16-bit mask on the int (`andb $0xfd,0x19` for `&= ~0x200`).
 *
 * THE THREE NAMED BITS ARE NAMED FROM AN ENUMERATION over every one of the
 * 28 `*V29` functions in the object, not from V.21's identically placed byte;
 * finding F9254 carries the enumeration and finding F8889 the rule.  What it
 * found, at 0x19 and nowhere else:
 *
 *   ERROR (0x0200) -- set by `orb $0x2` at 0x0a40cc inside `RxHdxErrorV29`
 *     and by nothing else; cleared by `V29RX_modem` at the top of every block
 *     (0x0a3f5e).  An event a caller must read each block or lose.  The
 *     setter is the ERROR state handler by the author's own symbol name.
 *
 *   CARRIER (0x2000) -- cleared and then set again if and only if a carrier
 *     detector answers, in `RxHdxIdleV29` (0x0a4311 / 0x0a4325, and the ONE
 *     site in the object that READS it, `testb $0x20` at 0x0a4329) and in
 *     `RxHdxStartV29` (0x0a451d / 0x0a4560); set on entry by `RxHdxDataV29`
 *     (0x0a3ff3) and cleared again on the arm where `DataCarrierDetectV29`
 *     says it has gone (0x0a401d); set by `RxHdxPrtcolV29` and
 *     `RxHdxEpochDetV29`.  Six functions, one role.
 *
 *   LOW_SNR (0x8000) -- cleared at 0x0a4081 and set at 0x0a4097 if and only
 *     if `GetSNRV29` came back at or below `V29RX_SNR_THRESHOLD`, and set by
 *     `RxHdxPrtcolV29` at 0x0a4441.  The one bit anything READS outside its
 *     own handler: `V29RX_status` tests it as `testb $0x80,0x19(%esi)` at
 *     0x0a461d and reports its COMPLEMENT as the report's `quality`, so a set
 *     bit is quality zero.  BOTH ENDS ARE NOW MEASURED, which is what this
 *     header previously did not have and why the bit was neutral until now.
 *
 * Bit 0x0100 is set and cleared by `RxNextStateV29` alone and is not named.
 */
#define V29_OBJ_STATUS		0x18
#define V29_STATUS_ERROR	0x0200
#define V29_STATUS_CARRIER	0x2000
#define V29_STATUS_LOW_SNR	0x8000

/*
 * The STATUS BYTE, which is byte 0 of that same int.  Seven values are
 * written across the receive handlers and NOTHING IN THE OBJECT READS ANY OF
 * THEM, so a name here is the site that writes it and no more than that --
 * `v21fax.h`'s ruling on the identically shaped byte, and its numbering turns
 * out to be the same one.  Only the value `RxHdxDataV29` writes is needed by
 * anything reconstructed, so only that one is named.
 *
 *   0  RxHdxDataV29, every block          <- V29RX_STATUS_DATA
 *   1  RxHdxPrtcolV29, RxHdxEpochDetV29
 *   2  RxHdxStartV29
 *   3  RxNextStateV29's default arm
 *   4  RxHdxPrtcolV29, RxHdxEpochDetV29
 *   5  RxHdxIdleV29
 *
 * The author's own words for the STATES beside them are in .rodata.str1.1:
 * "V29RX_STATE_START", "V29RX_STATE_IDLE", "V29RX_STATE_DATA",
 * "V29RX_STATE_PROTOCOL", "V29RX_STATE_EPOCH_DET" and "V29RX_DEFAULT, %d"
 * (0x4d03, 0x4d16, 0x4d28, 0x4d3a, 0x4d50 and 0x4cf0), all printed by
 * `RxNextStateV29`.  Which value goes with which string is NOT read off here,
 * because that needs the jump table at .rodata + 0xc35c and this pass did not
 * need it; DATA is named from its writer, as above.
 */
#define V29_OBJ_STATUS_B0	0x18
#define V29RX_STATUS_DATA	0

/*
 * `RxHdxDataV29` raises V29_STATUS_LOW_SNR when `GetSNRV29` comes back at or
 * below this.  The compare in the object is 16 bits wide (`cmp $0x8,%ax` at
 * 0x0a4091) and taken with `jg`, so it is SIGNED and NARROW; `GetSNRV29`
 * already returns `short`, so nothing has to narrow at the call site.
 * V.21's threshold is 5.
 */
#define V29RX_SNR_THRESHOLD	8

/*
 * Two shorts at the very front of the handle, both read by `V29RX_status`
 * only.  +0x00 goes to the report's `protocol` and +0x04 goes to BOTH its
 * +0x04 and its +0x12 -- from two separate loads, 0x0a4615 and 0x0a4646, so
 * they are two statements and not one value used twice.  `V29TX_status` does
 * the same thing with the transmitter's `V29TXS_BITRATE`, which is why +0x04
 * is read as a bit rate here.
 */
#define V29_OBJ_PROTOCOL	0x00
#define V29_OBJ_BITRATE		0x04

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
 * ...AND THE HANDLE IT SITS IN IS NOW REACHED BY THREE MORE FUNCTIONS, which
 * is what turns the paragraph above from a guess into a reading.
 * `V29TX_delete`, `V29TX_modem` and `ModDataV29` all take a handle whose
 * +0x24 is `struct v29tx` -- `V29TX_delete` releases exactly the three ring
 * buffers and the shaper that `v29data.h` says live in it -- and `V29TX_delete`
 * and `V29TX_modem` both reach the SAME parameter block at +0x20.  So the
 * transmit handle has +0x1c, +0x20 and +0x24, and none of the receive
 * constants above belongs to it.
 *
 * The int `V29TX_modem` returns, and the flag byte inside it: the object
 * clears one bit of the BYTE at +0x1d on entry (`andb $0xfd`, 0x0a46c2), sets
 * that same bit and stores a literal 7 into the BYTE at +0x1c on one
 * condition (0x0a4720, 0x0a4724), and returns the INT at +0x1c (0x0a472f).
 * `v17fax.h`'s `V17TX_OBJ_RESULT` / `_B1` pair byte for byte, and
 * `v21fax.h`'s.  NEUTRAL: nothing reconstructed reads either, so what the bit
 * indicates and what the 7 means are not established.  Named by VALUE.
 */
#define V29TX_OBJ_RESULT	0x1c
#define V29TX_OBJ_RESULT_B1	0x1d
#define V29TX_RESULT_B1_BIT1	0x02
#define V29TX_RESULT_BYTE_07	7

/*
 * The parameter block the transmitter owns, and the four fields reached.
 *
 * `V29TX_delete` releases the block's `sgd`, its `fax_fifo` and then the block
 * itself, so the transmitter owns it -- the same correction `v17fax.h` records
 * for V.17's parameter block.
 *
 * +0x00 and +0x04 are TYPED BY THEIR CALLEES, rank 2: +0x00 is `FIFO_write`'s
 * first argument at 0x0a4752 and `FIFO_delete`'s at 0x09befa, and +0x04 is
 * `SGD_delete`'s at 0x09beed.
 *
 * +0x08 and +0x10 are TYPED BY THE CONSTRUCTOR, the same rank: `V29TX_create`
 * writes `movl $0x0,0x8(%eax)` at 0x09bb33 and `movl $TxHdxStartV29,0x10(%eax)`
 * at 0x09bb3a -- an `R_386_32` against a function, so the slot holds a function
 * pointer and the word beside it is an `int` seeded to zero.  `V29TX_modem`
 * reads the first to choose an arm and calls through the second.  What the int
 * MEANS is not established, so it keeps its offset name.
 */
#define V29TX_OBJ_PARAMS	0x20

#define V29TXP_FIFO		0x00	/* struct fax_fifo *                 */
#define V29TXP_SGD		0x04	/* struct sgd *                      */
#define V29TXP_INT_0008		0x08	/* int: zero selects the FIFO arm    */
#define V29TXP_PROCESS		0x10	/* the dispatch slot                 */

/*
 * What `V29TX_modem` initialises its inner loop's budget to, ONCE, before the
 * loop -- `movw $0x30,0x1a(%esp)` at 0x0a46d8.  The dispatch slot decrements
 * it and the loop runs while it is STRICTLY POSITIVE as a signed short
 * (`cmpw $0x0` with `jg`), so a slot that overshot into negative territory
 * stops the loop rather than wrapping it.  V.17's is 0x30 too; V.21's is 6.
 */
#define V29TX_MODEM_BUDGET	0x30

/*
 * `V29TX_modem`'s inner call, and it is NOT `v29_demod_fn`.
 *
 * Four slots written: the instance, the caller's two buffers UNCHANGED, and
 * fourth `lea 0x1a(%esp)` -- the address of a `short` LOCAL, not the caller's
 * count.  `in` is NOT advanced between iterations (0x34(%esp) is reloaded
 * unchanged at 0x0a46ee) and `out` IS (it accumulates `2 * got` at 0x0a470a).
 * The result is sign-extended with `cwtl` before it joins the running total,
 * so it is `short` and that is forced.
 *
 * `in` is `unsigned short *` because `FIFO_write` -- handed the very same
 * pointer on the other arm -- declares its source that way.  Rank 2.
 */
typedef short (*v29tx_process_fn)(void *modem, unsigned short *in, short *out,
				  short *budget);

/*
 * The transmit block's scrambler, `struct fpm_sdm`.  `ScrambleDataV29` is
 * `add $0x1c,%eax` on the block pointer and a `jmp SDM_scrambler`, so the
 * offset is the whole of what that function establishes.
 */
#define V29TX_SDM		0x1c

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
 * The SECOND gate on `RxHdxDataV29`: it demodulates only while the carrier is
 * up AND this int is zero (`mov 0x8(%ecx),%edx; test %edx,%edx` at 0x0a4016).
 * Loaded 32 bits wide, so it is an `int` and that is forced.
 *
 * NEUTRAL, AND IT HAS TO BE.  Nothing reconstructed writes it -- `V29RX_delete`
 * does not free it, `V29TX_create` does not reach this block, and no other
 * `*V29` function touches +0x08 of the detection block at all -- so what it
 * records is not established, only what it gates.  `v21fax.h` says the same of
 * `struct v21_rx_hdx`'s `int_0000`, which is the identical field in the
 * identical position of V.21's own half-duplex context.
 */
#define V29DET_INT_0008		0x08	/* int                               */

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

/*
 * Two more of the receive block's own words, reached only by `V29RX_status`.
 *
 * +0x0000 is loaded 32 bits wide (`mov (%ecx),%edx` at 0x0a4675) and tested
 * against zero; +0x0018 is loaded as a BYTE (`movzbl 0x18(%edx),%ecx` at
 * 0x0a465f) and only its bit 0 is used.  The two widths are what say these
 * are two different fields and not one, and neither has a name anywhere in
 * the object.
 */
#define V29RX_INT_0000		0x0000
#define V29RX_FLAGS_0018	0x0018
#define V29RX_0018_BIT0		0x01
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
/*
 * The receive block's descrambler, `struct fpm_sdm`, from
 * `DescrambleDataV29`'s `add $0x4f3c,%eax`.  It sits BELOW the two buffer
 * pointers, which is what says the 0x4f00 region is a run of members of this
 * block rather than a separate allocation.
 */
#define V29RX_SDM		0x4f3c

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
 * A tenth short, written 0 by `V29RX_status` (0x0a463a) and by nothing else.
 * `V29TX_status` writes +0x0c and leaves this one alone, and `V29RX_status`
 * writes this one and leaves +0x0c alone, so the two are separate fields and
 * both exist.  `v21fax.h` says the same of the identically laid-out block.
 */
#define V29STAT_SHORT_0E	0x0e

/*
 * The report's flags byte, bit by bit, NEUTRALLY NAMED.
 *
 * ALL EIGHT are determined by `V29RX_status`, so its read of the byte carries
 * no value into the result; see D1097.
 *
 * `V29RX_status` touches seven of the eight and the object gives no name to
 * any of them -- no format string prints them and no other function in the
 * 1.2 MB reads them -- so these are bit positions and nothing more.  Two are
 * set unconditionally (4 and 6), two are cleared unconditionally (2 and 7),
 * one is cleared and not set again (0), and two are assigned from a field
 * (1 from the receive block's +0x0018 bit 0, 5 from `+0x0020 == 0`).  Naming
 * any of them from that would be usage inference over a single site, which
 * CLAUDE.md ranks below leaving it neutral.  `v21fax.h`'s
 * `V21_STATUS_BIT0..2` is the same decision on the same block.
 */
#define V29STAT_BIT0		0x01
#define V29STAT_BIT1		0x02
#define V29STAT_BIT2		0x04
#define V29STAT_BIT3		0x08
#define V29STAT_BIT4		0x10
#define V29STAT_BIT5		0x20
#define V29STAT_BIT6		0x40
#define V29STAT_BIT7		0x80

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
 * The receiver's half of the same report, and NOT a mirror of it.
 *
 * It reads the RECEIVE handle rather than the transmit one, puts the bit rate
 * in +0x04 and +0x12 where the transmit side puts it in +0x02 and +0x10,
 * writes +0x0e where the transmit side writes +0x0c, and rewrites the flags
 * byte four times where the transmit side rewrites it twice.  Same return
 * convention: 0 for a null report, 1 otherwise, and nothing else is checked.
 *
 * THE FOUR STORES TO +0x14 ARE FORCED AND NOT AN ARTEFACT.  Each is flushed
 * before the next load through the receive handle, at 0x0a4657, 0x0a466f,
 * 0x0a4685 and 0x0a46a2, because `status` and `modem` are unrelated
 * parameters and may overlap -- the same argument `V29TX_status` records for
 * its two, and finding F8878's.  The byte's PRIOR VALUE is dead: all eight
 * bits are determined before the function returns.  See F9130 and D1097.
 */
int V29RX_status(void *modem, void *status);

/*
 * The scrambler pair, two instructions and a tail jump each.
 *
 * The transmit one takes `struct fpm_sdm` at the TRANSMIT block's +0x1c
 * (`obj + 0x24`, `V29_OBJ_TX`) and the receive one takes the one at the
 * RECEIVE block's +0x4f3c (`obj + 0x50`, `V29_OBJ_RX`), so the pairing is
 * confirmed by which block each reaches and not by the names.  They jump to
 * `SDM_scrambler` and `SDM_descrambler` -- `src/fax/sdm.c`'s copies, byte for
 * byte the same code as the `FPM_SDM_*` pair -- and not to the `FPM_` ones.
 */
void ScrambleDataV29(void *modem, unsigned short *data, unsigned short count);
void DescrambleDataV29(void *modem, unsigned short *data, unsigned short count);

/*
 * Release the receiver: nine sub-objects, in the order above, then the
 * detection block, then the instance itself.
 *
 * THE INSTANCE IS FREED UNCONDITIONALLY and by a sibling `jmp`, so a caller
 * that supplied the storage does not get it back.
 */
void V29RX_delete(void *modem);

/*
 * Release the TRANSMITTER: nine releases, in the object's order (0x09bea1
 * through the sibling `jmp` at 0x09bf12).  The shaper at `V29FP_PPS`, then
 * the ring's `sym`, `q` and `i` buffers, then the private block itself, then
 * the `sgd` and the `fax_fifo` the parameter block owns and that block, and
 * the handle last.
 *
 * IT IS ALSO THE SECOND, INDEPENDENT STATEMENT OF `struct v29tx`.
 * `v29data.h` derives that layout from `TxNoCarrierV29` and `V29TX_create`;
 * this function derives +0x08, +0x0c, +0x10 and +0x64 again from what
 * RELEASES each, and the two readings agree.  Finding F9255.
 *
 * THE LITERAL 1 IN THE SECOND ARGUMENT SLOT IS NOT REPRODUCED (0x09be91,
 * before `FPM_PPS_free`).  Finding F8876, as for `V29RX_delete`.
 *
 * NO NULL GUARD ANYWHERE and the handle goes unconditionally, so a caller
 * that supplied the storage does not get it back.  Reproduced; D1150.
 */
void V29TX_delete(void *modem);

/*
 * Drive the transmitter for one caller block, and report what the handle's
 * result word says.
 *
 * TWO ARMS ON THE WAY IN, chosen by `V29TXP_INT_0008`.  Zero queues the
 * caller's `count` words through `FIFO_write` and remembers how many it took;
 * non-zero remembers `count` itself and touches the FIFO not at all.  What is
 * remembered is compared against `*count` AFTER the loop, and a mismatch sets
 * `V29TX_RESULT_B1_BIT1` and writes `V29TX_RESULT_BYTE_07` -- so on the second
 * arm the comparison is between a value and itself and neither is ever
 * written.
 *
 * THE LOOP IS A `do`/`while` ON A LOCAL, NOT ON THE CALLER'S COUNT: the budget
 * starts at `V29TX_MODEM_BUDGET`, is set once before the loop, and the slot
 * decrements it.  See `v29tx_process_fn`.
 *
 * `count` IS IN/OUT AND CHANGES UNITS across the call, and NOTHING CLAMPS what
 * the slot writes through `out` -- deviation D956's shape.  A caller's output
 * buffer must be sized from what the slot can produce over the whole budget
 * and not from the input count.
 *
 * The running total is a `short` re-narrowed with `cwtl` every iteration, so a
 * block producing more than 32,767 units wraps.  Reproduced; D1148.
 */
int V29TX_modem(void *modem, unsigned short *in, short *out,
		unsigned short *count);

/*
 * Two of V.29's half-duplex receive states, and the only two the object
 * reaches through a slot this tree has written.
 *
 * THEY ARE NOT AN INDIVISIBLE UNIT WITH THE REST OF THE RECEIVE PATH, and
 * that is measured rather than assumed: `RxHdxDataV29` is the target of
 * exactly two `R_386_32` relocations, both inside `RxNextStateV29`, and
 * `RxHdxErrorV29` of two more, in `RxHdxPrtcolV29` and `RxHdxEpochDetV29`.
 * `DemodDataV29` carries NO relocation against either.  V.21's five were one
 * unit precisely because `DemodDataV21` DOES compare against `RxHdxDataV21`
 * (findings F8492/F8493); V.29's demodulator makes no such comparison, so
 * each of these two is independently writable.  Finding F9256.
 *
 * ERROR   demodulates the block anyway so the filters keep their history,
 *         raises V29_STATUS_ERROR and returns 0.  It does not advance the
 *         state, so the machine stays here.
 * DATA    raises V29_STATUS_CARRIER and reports V29RX_STATUS_DATA, then
 *         demodulates and descrambles the block ONLY while
 *         `DataCarrierDetectV29` answers and the detection block's
 *         `V29DET_INT_0008` is clear.  Otherwise it lowers the carrier bit,
 *         consumes the block and returns 0 WITHOUT advancing the state --
 *         which is where it differs from `RxHdxDataV21`, whose else arm calls
 *         the state advance.
 *
 * `RxHdxDataV29` REPORTS ZERO UNITS WHEN THE QUALITY VERDICT IS
 * `V29Q_NO_CARRIER`, having already written them to `out` and advanced every
 * filter.  The object computes it `setne`/`movzbl`/`neg`/`and`, so the two
 * outcomes are `n` and 0 and there is no third.  See docs/deviations.md D1149.
 */
short RxHdxErrorV29(void *modem, short *in, short *out, unsigned short *count);
short RxHdxDataV29(void *modem, short *in, short *out, unsigned short *count);

/*
 * Encode `count` data words into the transmit ring and shape them into
 * `samples`, returning the number of samples written.
 *
 * `count` GOES TO BOTH CALLS UNCHANGED and is not the same unit in each:
 * `SMC_encoder` takes data words and `FPM_PPS_filter` takes symbols.  The
 * object holds it in `%ebx` across both and stores it into `0xc(%esp)` twice,
 * so there is no conversion to reproduce.  `ModDataV27` is the same function
 * over V.27ter's block and carries the same note.
 *
 * THE BLOCK IS RE-READ from the handle between the two calls -- `mov
 * 0x24(%esi),%eax` at 0x0a65da and again at 0x0a65fb -- which is the reload
 * pattern of every function in this file.
 *
 * RETURNS `unsigned short`: the object zero-extends with `movzwl %ax,%eax`
 * before the epilogue and `FPM_PPS_filter` already returns `unsigned short`.
 */
unsigned short ModDataV29(void *modem, const unsigned short *bits,
			  short *samples, unsigned short count);

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
 * One block through the receiver: AGC, an optional tone pre-pass, resample,
 * symbol recovery, equalise and slice.  Returns the number of data words
 * written to `out`, or ZERO if the tone pre-pass fired -- in which case
 * nothing after the pre-pass ran at all.
 *
 * `in` IS MODIFIED IN PLACE by the AGC, which is why it is not `const`.
 * `out` is `unsigned short *` because `FPM_FSE_receive`'s second buffer is.
 */
unsigned short DemodDataV29(void *modem, short *in, unsigned short *out,
			    unsigned short count);

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

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
struct v29rx_cfg;
struct v29tx_cfg;

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
 * ---------------------------------------------------------------------------
 * TWO MORE BITS ARE NAMED NOW, AND THE STATUS WORD IS THREE BYTES WIDE
 *
 * This header used to say "bit 0x0100 is set and cleared by `RxNextStateV29`
 * alone and is not named", and did not record 0x010000 at all -- because both
 * are written only by `RxNextStateV29`, which was unwritten.  Reading it gives
 * both, and they are exact complements of one another:
 *
 *   DATA (0x0100), bit 0 of the byte at +0x19 -- SET by the two arms that
 *     install `RxHdxDataV29` (PROTOCOL -> DATA at 0x0a4204 and IDLE -> DATA at
 *     0x0a425b) and CLEARED by every other arm including the default.
 *
 *   IDLE (0x010000), bit 0 of the byte at +0x1a -- SET by the ONE arm that
 *     installs `RxHdxIdleV29` (DATA -> IDLE at 0x0a4234) and CLEARED by all
 *     five others.
 *
 * So every arm writes both and never the same way: the one entering IDLE sets
 * IDLE and clears DATA, the two entering DATA do the reverse, and the rest
 * clear both.  Nothing in the object reads either, so what they are FOR is a
 * caller's business -- what is established is that together they report which
 * of the two steady states the machine is in, or neither.
 *
 * `V21RX_FLAG_DATA` / `V21RX_FLAG1_IDLE` in `v21fax.h` and
 * `V27_STATUS_FLAG_DATA` / `V27_STATUS_FLAG2_IDLE` in `v27fax.h` are the same
 * two bits of the same two bytes of the same machine, derived from V.21's and
 * V.27ter's own state machines.  Three modulations agreeing is corroboration;
 * the derivation here is V.29's six arms.
 */
#define V29_OBJ_STATUS		0x18
#define V29_STATUS_DATA		0x000100
#define V29_STATUS_ERROR	0x000200
#define V29_STATUS_CARRIER	0x002000
#define V29_STATUS_LOW_SNR	0x008000
#define V29_STATUS_IDLE		0x010000

/*
 * The STATUS BYTE, which is byte 0 of that same int.  NINE values are written
 * across the receive handlers and NOTHING IN THE OBJECT READS ANY OF THEM, so
 * a name here is the site that writes it and no more than that -- `v21fax.h`'s
 * ruling on the identically shaped byte.
 *
 *   0  RxHdxDataV29, every block          <- V29RX_STATUS_DATA
 *   1  RxHdxPrtcolV29, RxHdxEpochDetV29, on the carrier-present arm
 *   2  RxHdxStartV29
 *   3  RxNextStateV29's default arm
 *   4  RxHdxPrtcolV29, RxHdxEpochDetV29, on the carrier-lost arm
 *   5  RxHdxIdleV29
 *   6  RxNextStateV29's IDLE arm and RxHdxPrtcolV29, when V29DET_RATE says
 *      9600
 *   7  the same two sites, when it says 7200
 *
 * 6 AND 7 ARE NEW IN THIS PASS and the paragraph above used to say "seven
 * values"; the two extra ones come from the two writers this header could not
 * see until `RxNextStateV29` and `RxHdxPrtcolV29` were read.  Both sites spell
 * the choice the same way -- `cmp $0x1,%si` / `sbb` / `not %bl` / `add $0x7`,
 * GCC's branchless two-constant conditional -- so they are one expression
 * written twice and not two coincidences.
 *
 * ---------------------------------------------------------------------------
 * THE STATE NUMBERING IS NOW READ OFF, FROM THE JUMP TABLE AT .rodata + 0xc35c
 *
 * The author's own words for the STATES are in .rodata.str1.1 and every one of
 * them is printed by `RxNextStateV29`, from a five-armed switch on
 * `V29DET_STATE`.  The table's five entries, in index order, are the five arm
 * addresses, and each arm prints ONE string -- so the string names the state
 * the machine is LEAVING and the index is that state's number:
 *
 *     0  0x4d03 "V29RX_STATE_START\n"       -> installs RxHdxEpochDetV29, 1
 *     1  0x4d50 "V29RX_STATE_EPOCH_DET\n"   -> installs RxHdxPrtcolV29,   2
 *     2  0x4d3a "V29RX_STATE_PROTOCOL\n"    -> installs RxHdxDataV29,     3
 *     3  0x4d28 "V29RX_STATE_DATA\n"        -> installs RxHdxIdleV29,     4
 *     4  0x4d16 "V29RX_STATE_IDLE\n"        -> installs RxHdxDataV29,     3
 *
 * and anything above 4 takes the default arm, which prints 0x4cf0
 * "V29RX_DEFAULT, %d\n" with the value.  The consistency check is that each
 * arm's INSTALLED HANDLER matches the number it stores -- state 1 gets
 * `RxHdxEpochDetV29`, state 2 gets `RxHdxPrtcolV29`, state 3 gets
 * `RxHdxDataV29`, state 4 gets `RxHdxIdleV29` -- which it does at all five,
 * and which a mapping read the other way round would fail at every one.
 * Evidence rank 1.  Finding F9320.
 *
 * State 5 has no arm of its own: `RxHdxPrtcolV29` and `RxHdxEpochDetV29` store
 * it beside the store that installs `RxHdxErrorV29`, so it is the ERROR state
 * and it is a trap -- `RxNextStateV29` would take the default arm on it, and
 * `RxHdxErrorV29` never calls `RxNextStateV29`.
 */
#define V29_OBJ_STATUS_B0	0x18
#define V29RX_STATUS_DATA	0
#define V29RX_STATUS_TRAIN	1	/* PROTOCOL and EPOCH_DET, carrier up */
#define V29RX_STATUS_START	2
#define V29RX_STATUS_DEFAULT	3	/* RxNextStateV29's default arm       */
#define V29RX_STATUS_LOST	4	/* PROTOCOL and EPOCH_DET, carrier gone */
#define V29RX_STATUS_IDLE	5
/*
 * 6 and 7 report the RATE the machine is about to carry data at, and both
 * sites write one of them at the moment they install `RxHdxDataV29`.  The
 * field they read is `V29DET_RATE`, which `V29RX_create` fills from the
 * configuration's `bit_rate`; see there.  Nothing in the object READS either
 * byte, so this names the value's source and not its purpose.
 */
#define V29RX_STATUS_DATA_9600	6
#define V29RX_STATUS_DATA_7200	7

/* The five states the jump table numbers, and the sixth the two error arms
 * store.  See the derivation above. */
#define V29RX_STATE_START	0
#define V29RX_STATE_EPOCH_DET	1
#define V29RX_STATE_PROTOCOL	2
#define V29RX_STATE_DATA	3
#define V29RX_STATE_IDLE	4
#define V29RX_STATE_ERROR	5
#define V29RX_STATE_MAX		4	/* `cmp $0x4` / `ja default`          */

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

/*
 * The one field `V29RX_control` writes on the HANDLE itself rather than on
 * either sub-object -- `mov 0x4(%ebx),%edx` / `mov %edx,0x8(%esi)` at
 * 0x0a4593/0x0a459b, a straight 32-bit copy of the request's own `+0x04`.
 * NEUTRAL: nothing else reconstructed reads or writes handle+0x08, so what it
 * records is not established.
 */
#define V29_OBJ_INT_0008	0x08

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
 * A THIRD BYTE OF THE SAME INT, TYPED BY `TxNextStateV29`, AND ONE MORE BIT
 * OF THE SECOND.  `V29TX_OBJ_RESULT` is a 32-bit store, so it has four
 * bytes; `TxNextStateV29`'s IDLE-installing arm sets bit 0 of the byte at
 * +0x1e (`orb $0x1,0x1e(%ebx)` at 0xa49e2) and every other arm clears it --
 * `v21fax.h`'s `V21TX_OBJ_RESULT_B2`/`V21TX_RESULT_B2_BIT0` are the same
 * byte and the same bit of the identically-shaped machine one modulation
 * over, named on that precedent.  `V29TX_RESULT_B1_BIT0` is the DATA-
 * installing arm's own bit of +0x1d (`orb $0x1,0x1d(%ebx)` at 0xa49bd),
 * paired the same way with the already-named BIT1.  NEUTRAL, like BIT1:
 * nothing reconstructed reads either.
 */
#define V29TX_OBJ_RESULT_B2	0x1e
#define V29TX_RESULT_B1_BIT0	0x01
#define V29TX_RESULT_B2_BIT0	0x01

/*
 * THE STATUS BYTE `V29TX_OBJ_RESULT` HOLDS (its low byte, +0x1c itself),
 * NAMED BY WHICH SITE WRITES IT -- `v29fax.h`'s own ruling on the
 * identically-shaped receive-side byte, `V29RX_STATUS_*` above.
 *
 *   0  TxHdxDataV29, unconditional entry write (0xa4b42)
 *   1  TxHdxQuietV29/TxHdxABV29/TxHdxEQCondV29/TxHdxSCR1V29, unconditional
 *      entry write -- the four states between START and DATA
 *   2  TxHdxDataV29, on the one-shot arm, when V29TXP_RATE == 1 (0xa4c51)
 *   3  TxHdxDataV29, on the one-shot arm, when V29TXP_RATE == 0 (0xa4bf0)
 *   4  TxHdxIdleV29, unconditional entry write (0xa4ab6)
 *   5  TxNextStateV29's default arm, AND TxHdxDataV29's one-shot arm when
 *      V29TXP_RATE is neither 0 nor 1 (0xa4811/0xa4b66)
 *   6  TxHdxDataV29, the FIFO-underrun arm with V29TXP_INT_0008 == 0
 *      (0xa4ba1) -- the same shape and the same value as
 *      `V21TX_STATUS_UNDERRUN`
 *
 * 2 and 3 track `V29TXP_RATE` the same way `SetEncoderV29`'s argument does
 * (0 selects one report, 1 the other), which is why they are named for the
 * rate rather than left as bare numbers -- but nothing reconstructed reads
 * either value back, so this is usage inference over a correlation, not a
 * callee's own type.
 */
#define V29TX_STATUS_DATA_RATE_9600	2
#define V29TX_STATUS_DATA_RATE_7200	3
#define V29TX_STATUS_IDLE		4
#define V29TX_STATUS_DEFAULT		5
#define V29TX_STATUS_UNDERRUN		6

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
 * THE TRANSMIT HALF-DUPLEX MACHINE'S OWN FIELDS, all still inside the
 * `V29TX_OBJ_PARAMS` block -- `TxNextStateV29` reaches every one of these
 * through the same pointer `V29TXP_FIFO`/`V29TXP_SGD`/`V29TXP_INT_0008`/
 * `V29TXP_PROCESS` already name, so the block is one struct, not two.
 *
 * `V29TXP_STATE` is typed by the dispatcher itself: `TxNextStateV29` reads
 * it with `movswl 0x14(%ecx),%eax`, bounds it `cmp $0x6` / `ja default`, and
 * indexes a 7-entry jump table with it -- the same shape `V29DET_STATE`
 * has on the receive side.  Evidence rank 1: every one of the seven cases'
 * own debug string (`.rodata.str1.1` 0x4d67..0x4de8, read with
 * `tools/relocscan.py --at`) names the state being LEFT when it executes,
 * exactly as `TxNextStateV21`'s four do, and every string's name matches
 * which state the arm's OWN transition installs next -- `V29TX_STATE_START`
 * installs `TxHdxQuietV29` and advances to QUIET, `V29TX_STATE_QUIET`
 * installs `TxHdxABV29` and advances to ALT, and so on around the cycle
 * below.  Finding F9701.
 */
#define V29TXP_STATE		0x14	/* short                             */

#define V29TX_STATE_START	0
#define V29TX_STATE_QUIET	1
#define V29TX_STATE_ALT		2	/* installs TxHdxABV29 -- the object's
					 * own function name reads "AB", its
					 * own debug string reads "ALT"; both
					 * are the author's, not reconciled  */
#define V29TX_STATE_EQCOND	3
#define V29TX_STATE_SCR1	4
#define V29TX_STATE_DATA	5
#define V29TX_STATE_IDLE	6
#define V29TX_STATE_MAX		6	/* `cmp $0x6` / `ja default`         */

/*
 * A per-state budget, in whatever unit that state's own handler counts in
 * (blocks for QUIET/ALT/EQCOND/SCR1, a one-shot flag for DATA -- see
 * `TxHdxDataV29` below).  `TxNextStateV29` seeds it on every transition --
 * 0x30 entering QUIET, 0x80 entering ALT, 0x180 entering EQCOND, 0x30
 * entering SCR1, 1 entering DATA, 0 entering IDLE and entering START -- and
 * `V29TX_create` also seeds it 0 at construction (state starts at START).
 * USAGE INFERENCE: no format string names it and no function outside this
 * cycle touches it.
 */
#define V29TXP_SHORT_0016	0x16	/* short                             */

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
 * IT HAS ONE WRITER, AND THE LINE THAT SAID IT HAD NONE IS WITHDRAWN.
 * `V29RX_control` sets it from bit 4 of its own request's second control
 * byte -- `movzbl 0xd(%ebx),%edx` / `test $0x10,%dl` / `setne %al` / `mov
 * %eax,0x8(%ecx)` at 0x0a459e..0x0a45a8 -- so a caller can gate the
 * demodulator on or off through this entry point; `V29RX_delete` still does
 * not free it and `V29TX_create` still does not reach this block.  What the
 * bit itself indicates beyond that gate is not established.  `v21fax.h` says
 * the same of `struct v21_rx_hdx`'s `int_0000`, which is the identical field
 * in the identical position of V.21's own half-duplex context.  Finding
 * F10103.
 */
#define V29DET_INT_0008		0x08	/* int                               */

/*
 * THE NEGOTIATED BIT RATE, as an index rather than a rate: 0 is 7200 and 1 is
 * 9600.  This was `V29DET_SHORT_000C` and neutral until `V29RX_create` was
 * read, because until then it had two readers and no writer.
 *
 * `V29RX_create` fills it from the configuration's own `bit_rate`:
 * `cmp $0x1c20` stores 0, `cmp $0x2580` stores 1, and ANYTHING ELSE ALSO
 * STORES 1 -- 0x1c20 is 7200 and 0x2580 is 9600, which are the only two rates
 * V.29 defines.  `faxcfg.h` derives `v29rx_cfg::bit_rate` independently, from
 * the same two constants seen from the table's side.
 *
 * AND THE OBJECT CONFIRMS IT ONE MODULE FURTHER ON: `V29RX_create` sets the
 * descrambler's `nbits` to `4 - (rate == 0)`, so 3 bits a symbol at rate 0 and
 * 4 at rate 1 -- and V.29 is 2400 baud, so 3 x 2400 = 7200 and 4 x 2400 =
 * 9600.  Two independent readings of the same field agreeing on which value
 * is which.  It also chooses `V29DEC_SIXTEEN_POINT`, which is the same fact a
 * third time.  Finding F9323.
 *
 * The two readers pick a status byte from it, which is why those are named for
 * the rate they report.
 */
#define V29DET_RATE		0x0c	/* short: 0 = 7200, 1 = 9600         */
#define V29_BPS_7200		0x1c20
#define V29_BPS_9600		0x2580
#define V29_RATE_7200		0
#define V29_RATE_9600		1

/*
 * THE RECEIVE STATE NUMBER.  Upgraded from `V29DET_GATE_14`, which was neutral
 * because this pass's writers were unwritten.
 *
 * `RxNextStateV29` switches on it (`movswl 0x14(%edx),%eax` / `cmp $0x4` /
 * `jmp *0xc35c(,%eax,4)`) and every arm stores the next state back into it;
 * `RxHdxPrtcolV29` and `RxHdxEpochDetV29` store `V29RX_STATE_ERROR` into it
 * beside the store that installs `RxHdxErrorV29`; and `V29RX_create` seeds it.
 * The five values it takes are exactly the five the jump table indexes, and
 * each one arrives paired with its own handler in `V29DET_HANDLER`.
 *
 * `DemodDataV29`'s pre-pass gate is therefore "the machine is still in START":
 * while this is zero the demodulator halves the block into the scratch buffer,
 * notches it and runs the tone detector, and abandons the whole call if that
 * detector fires.  `v27fax.h` records the identical field of the identical
 * machine at `V27SH_SKIP_TONE` and says it is "ALMOST CERTAINLY THE RECEIVE
 * STATE NUMBER" (finding F9235); this is that guess measured.  Finding F9320.
 *
 * It is read `movswl` and the 32-bit result is the switch's index, so SIGNED
 * is FORCED -- CLAUDE.md's forced case exactly.  A negative value takes the
 * default arm, because the bound test is `cmp $0x4` with an UNSIGNED `ja`.
 */
#define V29DET_STATE		0x14	/* short                             */

/*
 * How many more blocks the current state will sit in before it advances, and
 * it is a COUNTDOWN rather than a count.
 *
 * `RxNextStateV29` seeds it on every transition -- 3 entering EPOCH_DET, 8
 * entering PROTOCOL, 0 entering DATA -- and `RxHdxEpochDetV29` and
 * `RxHdxPrtcolV29`, the two handlers of the two states that get a non-zero
 * seed, each decrement it once per block and advance the machine when what is
 * left is `<= 0`.  The IDLE arm does not write it and `RxHdxIdleV29` does not
 * read it.
 *
 * USAGE INFERENCE, and this header says so: no format string names it and no
 * other function in the object touches it.  What is MEASURED is the seed, the
 * decrement and the `jle`; "blocks" is the unit only because the handlers are
 * called once per block.
 *
 * The decrement is 16-bit and the test signed (`dec %cx` / `test %cx,%cx` /
 * `jle`), so a seed of 0 goes to -1 and advances on the first block.
 */
#define V29DET_STATE_COUNT	0x16	/* short                             */
#define V29DET_COUNT_EPOCH_DET	3	/* seeded entering EPOCH_DET         */
#define V29DET_COUNT_PROTOCOL	8	/* ... and entering PROTOCOL         */

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
 * TEN INTS IN A ROW, rx + 0x00 .. rx + 0x27, ALL SEEDED BY `V29RX_create` and
 * all seeded with 0 or 1.  Three of them are the enables above and two more
 * are read by `V29RX_status`; the other five have no reader anywhere in the
 * object, so they keep offset names.  The run is a solid block of `movl $0` /
 * `movl $1` at 0x9b337 through 0x9b39c, which is what says they are ten ints
 * rather than a mixture -- and it is why the sizes here are not guesses.
 *
 *     +0x00  1   V29RX_INT_0000, read by V29RX_status
 *     +0x04  1   V29RX_INT_0004, the SRE's phase-update enable
 *     +0x08  1   V29RX_INT_0008, the FSE's carrier-recovery enable
 *     +0x0c  0
 *     +0x10  0
 *     +0x14  1
 *     +0x18  1   V29RX_FLAGS_0018, read as a BYTE by V29RX_status
 *     +0x1c  0
 *     +0x20  1   V29RX_INT_0020, the FSE's coefficient-adaptation enable
 *     +0x24  0
 *
 * TWO OF THEM GET A SECOND WRITER.  `V29RX_control` clears +0x00 and +0x20 to
 * zero, each gated on its own bit of the request's FIRST control byte --
 * `movzbl 0xc(%ebx),%edx` then `test $0x8,%dl` for +0x00 (`movl
 * $0x0,(%ebx)` at 0x0a45d6) and `test $0x20,%dl` for +0x20 (`movl
 * $0x0,0x20(%ecx)` at 0x0a45c1).  `V29RX_create` is still the only seed to 1;
 * this is the only place either is turned back off.  Finding F10103.
 */
#define V29RX_INT_000C		0x000c
#define V29RX_INT_0010		0x0010
#define V29RX_INT_0014		0x0014
#define V29RX_INT_001C		0x001c
#define V29RX_INT_0024		0x0024

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
 * Compared against 999 by `DataCarrierDetectV29`.  A `short`: the object
 * compares 16 bits and branches signed.
 *
 * IT IS THE DECODER'S SYMBOL COUNTER, and that is now read off rather than
 * guessed: `V29RX_create` sets the equaliser's `cfg.owner` to `rx + 0x28`
 * (0x9b1c6: `mov 0x50(%ebp),%esi` then `add $0x28,%esi` at 0x9b1d6, stored
 * into the on-stack `fpm_fse_cfg`'s +0x2c), and `V29DEC_SYM_COUNT` is that
 * block's +0x1e -- so this offset and `V29RX_DEC + V29DEC_SYM_COUNT` are the
 * same sixteen bits.  All three slicers advance it and this is its only
 * reader, which is `V27RX_DEC_SETTLED`'s shape exactly.  The constant is kept
 * because `t_v29fax.c` pokes raw bytes and must not agree with `src/` by
 * construction.  Finding F9321.
 */
#define V29RX_SHORT_0046	0x0046

/*
 * THE DECODER BLOCK, rx + 0x28 .. rx + 0x47, and the equaliser's `cfg.owner`.
 *
 * It ends exactly where `V29RX_MRF` begins, so the thirty-two bytes below tile
 * it with nothing over.  Every offset in the `V29DEC_*` group is relative to
 * THIS, not to the receive block.
 */
#define V29RX_DEC		0x0028

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
/* The decoder block, at rx + V29RX_DEC and at fse->cfg.owner.               */

/*
 * IT IS NOT V.27ter's BLOCK WITH THE OFFSETS MOVED, and anyone reading
 * `v27fax.h` beside this will assume it is.  V.27ter puts the three-point
 * history at dec + 0x00 and its ONE leaky average at dec + 0x1e; V.29 puts TWO
 * leaky averages at dec + 0x00 and dec + 0x02 and the history above them, and
 * has no `eight_phase`/`train_short` pair, no `phase_mask`, no `pmap` or
 * `angles` POINTER (both tables are named directly), and no `angle_prev`
 * separate from the phase index.  Five of V.27's twelve fields have no
 * counterpart here at all.  Finding F9322.
 *
 * ---------------------------------------------------------------------------
 * THE TWO LEAKY AVERAGES, dec + 0x00 and dec + 0x02
 *
 * `V29RX_epoch_det` keeps two running averages of the MAGNITUDE the equaliser
 * hands it, and updates exactly ONE of them per symbol -- the first when the
 * phase advance since the previous symbol exceeds half a turn, the second when
 * it does not.  Both are then read together, as `(a*a + b*b) >> 15`, to form
 * the threshold the epoch test compares against.
 *
 * NEUTRALLY NAMED BY WHICH BRANCH FILLS THEM.  What is measured is the update,
 * the selector and the joint read; nothing in the object names either, and
 * "the average for the far half of the circle" is a description of the code
 * and not a claim about what the author thought it meant.
 *
 * The update is `avg = (31 * avg) >> 5; avg = avg + (*mag >> 5);` and it is
 * TWO STATEMENTS, not one: the object stores the intermediate to memory before
 * it loads `*mag` (0x9b74b then 0x9b74f, and 0x9b7e6 then 0x9b7ea).  It has to
 * -- `mag` is a `short *` parameter and may alias the field -- so the double
 * store is forced by the source's shape and is reproduced.  The shifts are
 * SHIFTS AND NOT DIVISIONS: a signed `/32` compiles to `test`/`add $31`/`sar`
 * and there is no such correction in these 413 bytes.
 */
#define V29DEC_MAG_AVG_FAR	0x00	/* short: diff  >  V29DEC_HALF_TURN  */
#define V29DEC_MAG_AVG_NEAR	0x02	/* short: diff <=  V29DEC_HALF_TURN  */

#define V29EPOCH_AVG_SHIFT	5
#define V29EPOCH_AVG_WEIGHT	31	/* (1 << V29EPOCH_AVG_SHIFT) - 1     */

/*
 * THE EPOCH DETECTOR'S THREE-POINT HISTORY, dec + 0x04 .. dec + 0x0f.
 *
 * `V29RX_epoch_det` is the only thing in the object that touches any of the
 * six, and it uses them exactly as `V27RX_epoch_det` uses its own: the newest
 * pair at +0x04/+0x06, one symbol back at +0x08/+0x0a and two symbols back at
 * +0x0c/+0x0e, shifted along by one every call.
 *
 * THAT THE PAIRS ARE (I, Q) IS RANK 2: the values written into +0x04 and +0x06
 * are `state->out_i[n]` and `state->out_q[n]`, and those two fields are
 * `fpm_fse.h`'s, named there from `FPM_FSE_receive`.  WHICH pair is "one back"
 * follows from the shift and from nothing else.
 *
 * ALL SIX ARE READ `movzwl` AND EVERY DIFFERENCE IS NARROWED BACK TO `short`,
 * so the extension is F614's free case and the `unsigned short` here is what
 * F7803's rule reads off the object -- the declared type of what is loaded --
 * and not something a test can measure.  The two SAMPLES differenced against
 * them are a different matter: they are read `movswl` and then SQUARED, so
 * their sign extension is forced and IS measured.
 */
#define V29DEC_I0		0x04	/* unsigned short, the newest        */
#define V29DEC_Q0		0x06
#define V29DEC_I1		0x08	/* one symbol back                   */
#define V29DEC_Q1		0x0a
#define V29DEC_I2		0x0c	/* two symbols back                  */
#define V29DEC_Q2		0x0e

/*
 * Sixteen constellation points or eight, and the same field chooses both which
 * half of the training alternation is used and whether the fourth bit reaches
 * the output.
 *
 * `V29RX_decision` computes `n = dec->sixteen_point ? 16 : 8` -- `cmp $0x1,%ebp;
 * sbb %esi,%esi; and $0xfffffff8,%esi; lea 0x10(%esi),%ebp`, GCC's branchless
 * two-constant conditional, which sets the mask on `%ebp == 0` alone -- and it
 * gates the `| (bi & 8)` that puts the amplitude bit into the returned symbol.
 * `V29RX_eq_train` uses it to choose between `V29_TRAIN_POINT_8` and
 * `V29_TRAIN_POINT_16`.
 *
 * `src/fax/v29cfg.c` derives the same split from the tables: the first eight
 * entries of `V29RX_DEC_IMAP`/`_QMAP` are V.29's inner ring, so eight points is
 * three bits a symbol at 2400 baud (7200 bit/s) and sixteen is four (9600).
 * An `int`: loaded 32 bits wide and tested 32 bits wide at all three sites.
 */
#define V29DEC_SIXTEEN_POINT	0x10	/* int                               */

/*
 * The previous symbol's phase index, 0..7, and the DPSK reference.
 *
 * `V29RX_decision` reads it, subtracts it from the index it just chose and
 * indexes `V29RX_DEC_PMAP` with the difference; both slicers write the low
 * three bits of their own chosen index back into it.  `V29RX_epoch_det` clears
 * it at the handover.  Read `movzwl` and the difference is masked to three
 * bits, so the extension is free and the type is F7803's reading.
 */
#define V29DEC_LAST		0x14	/* unsigned short                    */

/*
 * THE TRAINING SEQUENCE'S SHIFT REGISTER, and the field that makes
 * `V29RX_eq_train` a generator rather than a slicer.
 *
 * Seven bits, updated once per symbol as
 *
 *     x    = (unsigned)s
 *     x    = (((x << 6) & 0x80) ^ ((x & 1) << 7)) | x
 *     s'   = (x >> 1) & 0x7f
 *
 * -- a right-shifting linear feedback register whose incoming bit is the XOR
 * of the two it is about to lose.  That reading is the instructions and not an
 * interpretation: `shl $0x6` / `and $0x80` places bit 1 at bit 7, `and $0x1` /
 * `shl $0x7` places bit 0 there too, `xor` combines them, `or` re-attaches the
 * body, `shr $0x1` (LOGICAL, which is what makes the intermediate unsigned)
 * shifts and `and $0x7f` trims.
 *
 * ITS BIT 0 IS THE WHOLE OUTPUT: the training symbol is constellation point 0
 * when the bit is clear and `V29_TRAIN_POINT_8`/`_16` when it is set, so the
 * slicer emits a pseudo-random two-point alternation.  `V29RX_epoch_det` seeds
 * it with `V29_TRAIN_LFSR_SEED` at the handover.
 *
 * Read `movswl` -- SIGNED, and the 32-bit result is `or`ed into the register
 * before the logical shift, so a value with bit 15 set would change the answer.
 * Nothing writes one, but the extension is what the object encodes.
 */
#define V29DEC_TRAIN_LFSR	0x16	/* short                             */
#define V29_TRAIN_LFSR_SEED	0x55
#define V29_TRAIN_LFSR_MASK	0x7f
#define V29_TRAIN_POINT_8	3	/* the odd point at eight points     */
#define V29_TRAIN_POINT_16	11	/* ... and at sixteen                */

/*
 * Symbols the CURRENT slicer has taken, and IT IS SHARED BY TWO OF THEM WITH
 * ONE RESET IN BETWEEN -- exactly `V27DEC_TRAIN_COUNT`'s shape.
 *
 * `V29RX_epoch_det` increments it every call and will not judge until it has
 * passed `V29_EPOCH_SETTLE`; on the call where it hands over it stores 0xffff
 * and then FALLS INTO the same unconditional increment, so the counter reaches
 * `V29RX_eq_train` at zero.  `V29RX_eq_train` then counts up to
 * `V29_TRAIN_SYMS` and hands over in turn.
 *
 * READ SIGNED AND WRITTEN UNSIGNED, by both functions: `cmpw $0x80,0x18(%ebp)`
 * with `jle` against `movzwl` / `inc` / 16-bit store.  F614 is why that is not
 * a contradiction -- the increment's extension is dead and the compare's is
 * not -- and both spellings are reproduced.
 */
#define V29DEC_TRAIN_COUNT	0x18	/* unsigned short                    */

/*
 * Zeroed by `V29RX_create` and touched by NOTHING ELSE in the object -- not by
 * any of the three slicers and not by any `*V29` function.  Two bytes, from
 * its neighbours: `V29DEC_TRAIN_COUNT` is a `short` above it and
 * `V29DEC_ANGLE_PREV` a `short` below.
 */
#define V29DEC_SHORT_001A	0x1a	/* short                             */

/*
 * How many symbols `V29RX_epoch_det` waits before it will judge, and how many
 * `V29RX_eq_train` trains for.  Both are literals in their own function and
 * neither is selected by anything, which is where V.29 differs from V.27ter --
 * `V27DEC_TRAIN_SHORT` picks between two of each there and has no counterpart
 * here.
 *
 * The epoch test is `count > V29_EPOCH_SETTLE`, strictly, from `jle` on the
 * fall-through.  The training test is `count == V29_TRAIN_SYMS`, from `je`,
 * so it fires on exactly one call and a counter that started above the limit
 * would never hand over.
 */
#define V29_EPOCH_SETTLE	0x80
#define V29_TRAIN_SYMS		0x17e

/*
 * The epoch trigger's multiplier: the summed squared jump must EXCEED twice
 * the joint average, `add %edx,%edx` then `cmp`/`jle`.  V.27ter's is four.
 */
#define V29EPOCH_TRIGGER	2

/*
 * The previous symbol's measured angle, the reference `V29RX_epoch_det`
 * differences against.  Read and written `movzwl`, difference narrowed to
 * `short`: F614's free case again.
 *
 * IT IS NOT `V29DEC_LAST` SPELLED TWICE.  `V29DEC_LAST` is a constellation
 * INDEX in 0..7 and this is the raw angle in `FPM_atan`'s units, and the two
 * are written by different slicers -- `V29RX_epoch_det` never touches
 * `V29DEC_LAST` except to clear it once.
 */
#define V29DEC_ANGLE_PREV	0x1c	/* unsigned short                    */

/*
 * Symbols decided, saturating, and the one field of this block anything
 * outside the slicers reads (`DataCarrierDetectV29`, as `V29RX_SHORT_0046`).
 *
 * `V29RX_decision` and `V29RX_eq_train` increment it and, when the increment
 * would reach `V29DEC_SYM_COUNT_WRAP`, store `V29DEC_SYM_COUNT_RESTART`
 * instead -- so it never goes negative and never stops moving.
 * `V29RX_epoch_det` increments it WITHOUT the saturation, which is the one
 * place the three slicers disagree about a shared field and is reproduced as
 * written.
 */
#define V29DEC_SYM_COUNT	0x1e	/* unsigned short                    */
#define V29DEC_SYM_COUNT_WRAP	0x8000
#define V29DEC_SYM_COUNT_RESTART 0x4000

/*
 * The full circle in the units `fpm_fse` hands the slicer, and half of it.
 * `V29RX_epoch_det` folds the phase difference into [0, 0x7fff] by SUBTRACTING
 * a full turn when it is negative -- which is what the object encodes
 * (`sub $0x8000,%eax` at 0x9b802) and is the same sixteen bits as adding one --
 * and then splits on the half turn.  `V29RX_DEC_ANGLE`'s own step of 4096 per
 * 45 degrees says the same thing: eight steps to the turn.
 */
#define V29DEC_PHASE_FULL	0x8000
#define V29DEC_HALF_TURN	(V29DEC_PHASE_FULL / 2)

/* The two constellation indices `V29RX_epoch_det` reports, one per half. */
#define V29_EPOCH_POINT_FAR	4	/* V29RX_DEC_ANGLE + 8  = 180 degrees */
#define V29_EPOCH_POINT_NEAR	7	/* V29RX_DEC_ANGLE + 14 = 315 degrees */

/*
 * What the training slicer parks the equaliser's `mse` at on EVERY call, and
 * the two LMS step sizes the chain installs.
 *
 * NEUTRAL.  Nothing reconstructed reads `cfg.mu[0]` or `cfg.mu[1]` -- they
 * belong to `FPM_FSE_receive` -- so what is established is only which slicer
 * writes which, and that `V29RX_MU_TRAIN` is exactly twice `V29RX_MU_DATA`.
 * The 0x4000 matters more than it looks: `RxHdxIdleV29` will only leave IDLE
 * once `mse` has come back down to `V29RX_MSE_RECOVERED` or below, so a
 * training slicer that is still running holds the machine there.
 */
#define V29RX_TRAIN_MSE		0x4000
#define V29RX_MU_TRAIN		0x14e6	/* cfg.mu[0], by V29RX_epoch_det     */
#define V29RX_MU_DATA		0x0a73	/* cfg.mu[1], by V29RX_eq_train      */

/*
 * The `fpm_fse::mse` below which `RxHdxIdleV29` will hand the machine back to
 * DATA, compared with `>` and branched `jle`, 16-bit and signed.  V.29's
 * no-carrier threshold on the same field is `V29RX_DEC_ERROR_MAX`, four times
 * larger.
 */
#define V29RX_MSE_RECOVERED	0x1fff

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
/* The slot `V29RX_modem` calls through: THE CURRENT RECEIVE STATE HANDLER.  */

/*
 * `call *0x10(%eax)` with `%eax` = the detection block.  A function pointer
 * inside a block whose other members are sub-objects, so it is spelled as an
 * offset and not as a vtable: nothing establishes that +0x00..+0x0c are
 * pointers of the same kind, and V29RX_delete proves they are not (they are
 * an MTD and a TONE).
 *
 * IT WAS `V29DET_DEMOD` / `v29_demod_fn` AND IT IS NOT A DEMODULATOR SLOT.
 * Every store into it in the whole object is a `RxHdx*V29` state handler --
 * five in `RxNextStateV29`, one each in `RxHdxPrtcolV29` and
 * `RxHdxEpochDetV29`, and the seed in `V29RX_create` -- and every one of them
 * is paired with a store of the matching number into `V29DET_STATE`.  So the
 * slot is the machine's current state and `V29RX_modem` is its driver, which
 * is `V27SH_STATE`'s shape one modulation over.  Renamed in this pass; the old
 * name carried no claim that survived reading its writers.  Finding F9320.
 *
 * The signature is forced by the caller: four arguments pushed, the return
 * sign-extended from 16 bits with `cwtl`, the third argument advanced by the
 * return and the second by what the fourth was decremented by.
 */
#define V29DET_HANDLER		0x10

typedef short (*v29_rx_state_fn)(void *modem, short *in, short *out,
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
 * ---------------------------------------------------------------------------
 * V29TX_control -- .text 0x0a4fb0, 126 bytes.
 *
 * A NULL request does nothing and returns 0; anything else is applied and the
 * function returns 1.  Five effects, none of them exclusive of the others --
 * `V17TX_control`'s own shape, one level of indirection different
 * (`V29TX_OBJ_PARAMS` = 0x20 supplies the rate at +0xc, matching
 * `V29TXP_RATE`; `V29TX_OBJ_FP`/`V29_OBJ_TX` = 0x24 is the private block; the
 * request's own `+0x08` plays `V17TX_control`'s `+0x08`, with NO `+0x10`
 * field touched):
 *
 *   - the request's `+0x04` is copied straight into the HANDLE's own
 *     `struct v29tx_cfg::int_0008` (neutral there; see v29data.h) -- the same
 *     shape `V29RX_control`'s own `+0x04` -> `V29_OBJ_INT_0008` copy takes
 *     (F10103);
 *   - the request's `+0x08`, multiplied by `V29TX_PPS_SCALE[rate]`, becomes
 *     the pulse shaper's `scale` (`V29FP_PPS`'s own `struct fpm_pps_cfg::
 *     scale`, at `V29TX(fp)->pps.cfg.scale`, the private block's +0x6c) -- a
 *     runtime GAIN OVERRIDE on top of the constructor's own per-rate table
 *     lookup, the same role `V17TX_control` gives its own `+0x08`.  THE
 *     OBJECT WRITES `scale` TWICE here too (`mov %eax,0x6c(%ecx)` at
 *     0x0a4fda, then again at 0x0a4fe5 after the `imul`) -- the first write
 *     is dead, the object's own, reproduced on the same D1032/F8878 ground as
 *     `V17TX_control`'s;
 *   - the request's FIRST control byte, bit 2, sets `V29TXS_10_BIT2` in the
 *     handle's own `V29TXS_FLAGS_10` -- the identical bit `V29TX_status`
 *     already reads back out (v29data.h), so this is a confirmed field and
 *     not a new one;
 *   - the request's SECOND control byte: bit 4 sets the parameter block's
 *     `V29TXP_INT_0008` (0 or 1, from `setne`, matching `V29RX_control`'s own
 *     style for the equivalent bit -- UNLIKE `V17TX_control`'s literal-store
 *     version of the same field); bit 1, if set, calls `V29TX_create(fp,
 *     fp)` -- BOTH ARGUMENTS THE SAME REGISTER (`mov %esi,0x4(%esp)` then
 *     `mov %esi,(%esp)` at 0x0a5020/0x0a5024), the same self-referential
 *     reinit shape `V29RX_control`/`V17TX_control` use.
 *
 * THE REQUEST TYPE IS NEW, the same footing as `V29RX_control`'s and
 * `V17TX_control`'s: nothing else reconstructed reads this argument, so every
 * field below is established from this function alone.  `+0x04` and `+0x08`
 * are each a 32-bit load; the two control bytes at `+0x0c`/`+0x0d` are each
 * `movzbl`.  Nothing here establishes what precedes `+0x04` or separates the
 * fields from each other, so the gaps stay padding.  Finding F10107.
 */
struct v29tx_control_req {
	unsigned char	pad_0000[0x04];
	int		int_0004;	/* +0x04 -> handle's v29tx_cfg::int_0008 */
	int		int_0008;	/* +0x08 scales the PPS shaper's gain    */
	unsigned char	ctl0;		/* +0x0c */
	unsigned char	ctl1;		/* +0x0d */
};

/* Bits of `ctl0`, named by value; see V29TX_control's own derivation. */
#define V29TXCTL_CTL0_BIT2	(1 << 2)	/* -> V29TXS_10_BIT2 of the
						   handle's own V29TXS_FLAGS_10,
						   read back by V29TX_status  */

/* Bits of `ctl1`, named by value. */
#define V29TXCTL_CTL1_BIT1	(1 << 1)	/* re-runs V29TX_create        */
#define V29TXCTL_CTL1_BIT4	(1 << 4)	/* -> V29TXP_INT_0008 (0 or 1) */

int V29TX_control(void *fp, const struct v29tx_control_req *req);

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
 * ---------------------------------------------------------------------------
 * V29RX_control -- .text 0x0a4580, 110 bytes.
 *
 * A NULL request does nothing and returns 0; anything else is applied and the
 * function returns 1.  Four independent effects, none of them exclusive of
 * the others:
 *
 *   - the request's `+0x04` is copied straight into the HANDLE's own
 *     `V29_OBJ_INT_0008` (neutral; see there);
 *   - the request's second control byte, bit 4, becomes the demodulator gate
 *     `V29DET_INT_0008` (0 or 1, from `setne`, not the bit itself);
 *   - that same byte's bit 1, if set, calls `V29RX_create(modem, modem)` --
 *     BOTH ARGUMENTS ARE THE SAME REGISTER (`mov %esi,0x4(%esp)` then `mov
 *     %esi,(%esp)` at 0x0a45e0/0x0a45e4), so this re-runs the constructor's
 *     "reuse an existing handle" path over the live instance rather than
 *     allocating a second one -- `V29RX_create` takes `(modem, params)` and
 *     treats a non-NULL `params` as `*(struct v29rx_cfg *)modem =
 *     *(struct v29rx_cfg *)modem`, a self-copy of the config block that
 *     precedes everything else the constructor does;
 *   - the request's FIRST control byte, bits 3 and 5, each independently
 *     clear one of `V29RX_INT_0000` / `V29RX_INT_0020` back to zero -- see
 *     the correction above.
 *
 * THE REQUEST TYPE IS NEW: nothing else reconstructed reads this argument, so
 * every field below is established from this function alone.  `+0x04` is a
 * 32-bit load (`mov 0x4(%ebx),%edx`) and forced to be `int`.  The two control
 * bytes at `+0x0c`/`+0x0d` are each loaded once with `movzbl` and match
 * `v32fp.h`'s `struct v32fp_ctl::ctl0`/`ctl1` in shape and position --
 * corroboration, not derivation, since that struct belongs to an unrelated
 * modem family.  Nothing here establishes what precedes `+0x04` or separates
 * it from `+0x0c`, so both gaps stay padding.  Finding F10103.
 */
struct v29rx_control_req {
	unsigned char pad_0000[0x04];
	int int_0004;			/* +0x04 -> handle's V29_OBJ_INT_0008 */
	unsigned char pad_0008[0x04];
	unsigned char ctl0;		/* +0x0c */
	unsigned char ctl1;		/* +0x0d */
};

/* Bits of `ctl0`, named by value; see V29RX_control's own derivation. */
#define V29RXCTL_CTL0_BIT3	(1 << 3)	/* clears V29RX_INT_0000      */
#define V29RXCTL_CTL0_BIT5	(1 << 5)	/* clears V29RX_INT_0020      */

/* Bits of `ctl1`, named by value. */
#define V29RXCTL_CTL1_BIT1	(1 << 1)	/* re-runs V29RX_create        */
#define V29RXCTL_CTL1_BIT4	(1 << 4)	/* -> V29DET_INT_0008 (0 or 1) */

int V29RX_control(void *modem, const struct v29rx_control_req *req);

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
 * THE TRANSMIT HALF-DUPLEX MACHINE: seven states in a cycle, and it does not
 * decompose -- `TxNextStateV29` stores all seven `TxHdx*V29` handlers and
 * each of the seven calls it back (confirmed with `objdump -dr`: seven
 * `R_386_PC32 TxNextStateV29` relocations, one per state), the F8492/F8493
 * shape.  `V29TX_create` installs only `TxHdxStartV29` directly and never
 * references `TxNextStateV29` itself, so all eight symbols link only
 * together.
 *
 * START    does nothing but advance -- no FIFO read, no budget spent.
 * QUIET    unmodulated silence (`TxNoCarrierV29`) for V29TXP_SHORT_0016
 *          blocks, then advances; the transition also seeds the scrambler's
 *          shift register (V29SCRAM_SR, the SAME field GenEQTrnSequenceV29
 *          uses, since V29TX_OBJ_SCRAM and V29TX_OBJ_PARAMS are one block --
 *          see the note at V29SCRAM_SR in v29data.h) to 42 and installs
 *          TxHdxABV29.
 * ALT      SGD_symbol_gen's data-word form (word_syms=2, data_word=0x4f)
 *          shaped directly, no scrambling -- V.29's alternating training
 *          dibit.  The QUIET->ALT transition also calls SetEncoderV29(1).
 * EQCOND   the equaliser-training LFSR, inlined (see below) rather than
 *          calling the separately-exported GenEQTrnSequenceV29 -- the two
 *          are independent copies of the same recurrence, not caller and
 *          callee; GenEQTrnSequenceV29 has no internal referrer at all
 *          (CLAUDE.md's fax-scope note).  The ALT->EQCOND transition
 *          re-seeds the same LFSR field to 42.
 * SCR1     SGD_symbol_gen (word_syms=1, data_word = V29TX_PATTERN_SCR1
 *          [V29TXP_RATE]) THEN ScrambleDataV29 -- the first state that
 *          scrambles.  The EQCOND->SCR1 transition seeds the scrambler
 *          (SeedScramblerV29(modem, 0)) and the encoder
 *          (SetEncoderV29(modem, 0)) fresh.
 * DATA     see TxHdxDataV29.
 * IDLE     see TxHdxIdleV29; wraps to START once the FIFO has data again.
 *
 * Every arm installs its OWN next handler and seeds V29TXP_SHORT_0016 with
 * that next state's own budget (0x30/0x80/0x180/0x30/1/0/0 for
 * QUIET/ALT/EQCOND/SCR1/DATA/IDLE/START) -- see V29TXP_SHORT_0016's own
 * comment.  Finding F9701.
 */
void TxNextStateV29(void *modem);

/* The START state: nothing but the transition.  0xa4f90, 21 bytes. */
short TxHdxStartV29(void *modem, unsigned short *in, short *out,
		    short *budget);

/*
 * The QUIET state.  Reports V29TX_STATUS_DEFAULT's sibling status 1 (shared
 * with EQCOND/SCR1/ALT, see V29TX_OBJ_RESULT's status-byte comment) at
 * entry, then TxNoCarrierV29 over V29TXP_SHORT_0016 blocks, decrementing it
 * by min(remaining, *budget) each call.  0xa4ee0, 163 bytes.
 */
short TxHdxQuietV29(void *modem, unsigned short *in, short *out,
		    short *budget);

/* The ALT state (function name AB, debug string ALT).  0xa4e20, 190 bytes. */
short TxHdxABV29(void *modem, unsigned short *in, short *out, short *budget);

/*
 * The EQCOND state.  0xa4d30, 237 bytes.  The LFSR recurrence is
 * GenEQTrnSequenceV29's own (v29data.h) written out again rather than
 * called -- see TxNextStateV29's comment above.
 */
short TxHdxEQCondV29(void *modem, unsigned short *in, short *out,
		     short *budget);

/* The SCR1 state, the first to scramble.  0xa4c60, 206 bytes. */
short TxHdxSCR1V29(void *modem, unsigned short *in, short *out,
		   short *budget);

/*
 * The DATA state.  0xa4b20, 314 bytes.
 *
 * ON THE ONE-SHOT CALL AFTER SCR1 INSTALLS IT (V29TXP_SHORT_0016 != 0, which
 * SCR1's own installer seeds to 1), reports a status that tracks
 * V29TXP_RATE and clears the field so no later call repeats it -- see
 * V29TX_STATUS_DATA_RATE_9600/_7200/_DEFAULT's own comments.
 *
 * THREE ARMS ON THE FIFO READ, all sharing TxHdxDataV21's shape one
 * modulation over: satisfied (FIFO supplied the whole request) and
 * underrun-with-V29TXP_INT_0008-set both scramble+modulate exactly what the
 * FIFO gave and zero the remaining budget; underrun-with-INT_0008-clear
 * scrambles+modulates the FULL REQUESTED BUDGET regardless of what the FIFO
 * supplied (raising V29TX_RESULT_B1_BIT1 and reporting
 * V29TX_STATUS_UNDERRUN); underrun-with-INT_0008-set instead modulates only
 * what the FIFO gave, LEAVES the remainder in `*budget`, and calls
 * TxNextStateV29 before returning.
 */
short TxHdxDataV29(void *modem, unsigned short *in, short *out,
		   short *budget);

/*
 * The IDLE state, installed by the DATA arm.  Unconditionally reports
 * V29TX_STATUS_IDLE (4) at entry -- unlike QUIET/ALT/EQCOND/SCR1, it has no
 * V29TXP_SHORT_0016 countdown of its own: with the FIFO empty it asks
 * TxNoCarrierV29 for the WHOLE current budget in one call and zeroes it;
 * with the FIFO non-empty it does not modulate at all and calls
 * TxNextStateV29 immediately, matching TxHdxIdleV21's shape.  0xa4aa0, 116
 * bytes.
 */
short TxHdxIdleV29(void *modem, unsigned short *in, short *out,
		   short *budget);

/*
 * Allocate-or-reuse the handle, copy the 28-byte configuration onto its
 * head (`params`, or `V29TX_CFG` when NULL), allocate-or-reuse the
 * parameter/half-duplex block and build the transmit FIFO and SGD into it,
 * derive `V29TXP_RATE` from the just-copied config's own `bitrate` (see its
 * own comment in v29data.h -- the same three-way compare `V29RX_create`
 * runs for `V29DET_RATE`, including the same "anything else" default arm
 * that raises `V29TX_RESULT_B1_BIT1` and reports `V29TX_STATUS_DEFAULT`),
 * seed the scrambler LFSR (`V29SCRAM_SR`) and install `TxHdxStartV29` at
 * `V29TX_STATE_START`, allocate-or-reuse `struct v29tx` (v29data.h) and
 * initialise its ring, its descrambler (`SDM_init`, `nbits` from
 * `V29TXP_RATE`), its symbol coder (`SMC_init`, direct/complex form,
 * `V29TX_SMC_*` tables, `amask` from `V29TXP_RATE`) and its pulse shaper
 * (`FPM_PPS_init`, `V29TX_PPS_IFILT`/`QFILT`, scale from
 * `V29TX_PPS_SCALE[V29TXP_RATE]`).  0x09ba00, 1,162 bytes.
 *
 * THE MODEM HANDLE IS NOT MODELLED AS A STRUCT, on this file's own
 * established convention: nothing traced needs its total size, only the
 * individual offsets already named (V29TX_OBJ_RESULT/_B1/_B2,
 * V29TX_OBJ_PARAMS, V29TX_OBJ_TX and V29TX_CFG's own 28 bytes).  Finding
 * F9701.
 */
void *V29TX_create(void *modem, const struct v29tx_cfg *params);

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

/* ------------------------------------------------------------------------ */
/* What `V29RX_create` lays down.                                           */

/*
 * THE THREE ALLOCATIONS ARE MEASURED, from `sysdep_malloc`'s own argument at
 * each of the six call sites.  The receive block's 0x4f6c is what makes the
 * 0x4f00 region members of it rather than a separate object, which this header
 * had inferred from `DescrambleDataV29`'s `add $0x4f3c` and can now state.
 */
#define V29_OBJ_SIZE		0x54
#define V29DET_SIZE		0x58
#define V29RX_SIZE		0x4f6c

/*
 * The four scratch buffers, in BYTES as the object allocates them.  Three are
 * 0x140 and the fourth is 0x148, and the difference is not a typo: the SRE's
 * output feeds the equaliser, which reads one entry past what it was given.
 * `V29RX_create` then zeroes the FIRST V29RX_BUF_ZEROED entries of the MRF and
 * SRE buffers -- 160 of the MRF's 160 and 160 of the SRE's 164 -- so the SRE's
 * last four shorts keep whatever the allocator left.
 */
#define V29DET_BUF_BYTES	0x140
#define V29DET_V21_BUF_BYTES	0x140
#define V29RX_BUF_MRF_BYTES	0x140
#define V29RX_BUF_SRE_BYTES	0x148
#define V29RX_BUF_ZEROED	0xa0

/*
 * THE EQUALISER'S WORKING STATE, REPUBLISHED ON THE HANDLE.  Six fields that
 * `V29RX_create` copies out of `struct fpm_fse` once, at the end, and that
 * nothing reconstructed reads: five pointers straight into the equaliser and
 * its tap count.  They are TYPED BY WHERE THEY COME FROM -- `fpm_fse.h` names
 * every one -- which is evidence rank 2, and what a caller DOES with them is
 * not established.  A diagnostic window is the obvious guess and is left as
 * one.
 *
 * `V29_OBJ_EQ_NOUT` is the ADDRESS of the equaliser's `n_out`, not its value:
 * the object computes `lea 0x17e(%edx)` on the receive block, and 0x17e is
 * `V29RX_FSE + offsetof(struct fpm_fse, n_out)`.
 */
#define V29_OBJ_EQ_OUT_I	0x1c	/* short *                           */
#define V29_OBJ_EQ_OUT_Q	0x20	/* short *                           */
#define V29_OBJ_EQ_NOUT		0x24	/* unsigned short *                  */
#define V29_OBJ_EQ_ICOEFF	0x28	/* short *                           */
#define V29_OBJ_EQ_QCOEFF	0x2c	/* short *                           */
#define V29_OBJ_EQ_TAPS		0x30	/* short                             */

/*
 * Six more the constructor zeroes and nothing reads.  The widths are the
 * object's own stores: 0x34, 0x38, 0x40 and 0x44 with `movl`, 0x3c and 0x48
 * with `movw`.  The gap at 0x4a..0x53 is never written by anything.
 */
#define V29_OBJ_INT_0034	0x34
#define V29_OBJ_INT_0038	0x38
#define V29_OBJ_SHORT_003C	0x3c
#define V29_OBJ_INT_0040	0x40
#define V29_OBJ_INT_0044	0x44
#define V29_OBJ_SHORT_0048	0x48

/* The two flags `V29RX_create` raises in the status word and nothing reads. */
#define V29_STATUS_CREATE_BITS	0x5000

/*
 * The constants the constructor plants that are not any module's own.
 *
 * `V29RX_TONE_HZ` is 1700, which is V.29's carrier frequency, and it replaces
 * `FPM_TONE_CFG_data`'s 2100 (V.25's answer tone) in the copy the detector
 * gets.  `V29RX_MTD_*` and `V29RX_V21_MTD_*` are the two tone detectors'
 * thresholds; the V.21 one runs on 300 bit/s channel 2 and takes
 * `V21_CHAN2_MTD_COEFF`.
 *
 * NEUTRAL WHERE THE OBJECT IS: what `ratio`, `min_level` and `tones` mean is
 * `fpm_mtd.h`'s business, and these are just the values V.29 asks for.
 */
#define V29RX_TONE_HZ		0x6a4	/* fpm_tone_cfg::freq, 1700 Hz       */
#define V29RX_MTD_TONES		2
#define V29RX_MTD_RATIO		0x199a
#define V29RX_MTD_MIN_LEVEL	0x32
#define V29RX_V21_MTD_TONES	2
#define V29RX_V21_MTD_RATIO	0x4ccd
#define V29RX_V21_MTD_MIN_LEVEL	0x12c

/*
 * The descrambler's two taps.  `nbits` is `4 - (rate == V29_RATE_7200)` and is
 * derived at `V29DET_RATE`; these two are literals.
 */
#define V29RX_SDM_TAP1		0x12
#define V29RX_SDM_TAP2		0x17

/*
 * The decoder-error limit, which is the ONE field of the receive block whose
 * seed depends on the rate and whose default arm writes NOTHING AT ALL.  Rate
 * 0 gets 0x320 and rate 1 gets 0x2bc; any other value leaves the field holding
 * whatever `sysdep_malloc` returned, which on a reused instance is the
 * previous session's.  Reproduced; deviation D1173.
 */
#define V29RX_DEC_ERROR_LIMIT_7200	0x320
#define V29RX_DEC_ERROR_LIMIT_9600	0x2bc

/* The seed `V29RX_create` gives the training sequence's shift register. */
#define V29RX_TRAIN_LFSR_INIT	0x6a

/*
 * Create or re-initialise a V.29 receiver.
 *
 * `modem` NULL allocates the handle; anything else is re-initialised IN PLACE,
 * and so are the detection and receive blocks if their pointers are already
 * set.  `params` NULL takes `V29RX_CFG`.  Returns the handle, which is the one
 * passed in unless it was NULL.
 *
 * THE `reset` FLAG THE FPM MODULES GET IS "I JUST ALLOCATED THIS", and it is
 * TWO different flags: the detection block's sub-objects get one that is set
 * only when the DETECTION BLOCK was freshly allocated, and the receive block's
 * get one set only when the HANDLE was.  Re-initialising an existing handle
 * whose receive block was NULL therefore allocates that block and then hands
 * `FPM_MRF_init`, `FPM_AGC_init`, `FPM_SRE_init` and `FPM_FSE_init` a zero --
 * so those four modules re-init over uninitialised memory.  It is the object's
 * and is not a path any reconstructed caller takes; deviation D1174.
 */
void *V29RX_create(void *modem, const struct v29rx_cfg *params);

/* ------------------------------------------------------------------------ */
/*
 * THE SLICER CHAIN: three stages in a line, each one installing the next.
 *
 * `V29RX_create` installs `V29RX_epoch_det`, which installs `V29RX_eq_train`,
 * which installs `V29RX_decision`.  Nothing installs a stage backwards and no
 * stage installs itself, so the chain runs once per carrier and the object
 * carries exactly three `R_386_32` relocations to say so.  V.27ter's chain is
 * the same three stages one modulation over; `v27fax.h` and `src/fax/v27.c`
 * carry that reading and the differences are set out at `V29RX_DEC` above.
 *
 * All three take `fpm_fse`'s slicer signature, return 0xffff except
 * `V29RX_decision`, and reach their state through `state->cfg.owner`.
 */
unsigned short V29RX_epoch_det(struct fpm_fse *state, short *angle, short *mag);
unsigned short V29RX_eq_train(struct fpm_fse *state, short *angle, short *mag);
unsigned short V29RX_decision(struct fpm_fse *state, short *angle, short *mag);

/*
 * THE HALF-DUPLEX RECEIVE MACHINE: five states in a cycle, and it does not
 * decompose.
 *
 * `RxNextStateV29` stores four of the handlers and each of the other four
 * calls it, so no proper subset of the five links -- `RxHdxDataV29` and
 * `RxHdxErrorV29` were already written, which is the only reason the unit is
 * five and not seven.  Finding F9256 measured that `DemodDataV29` carries NO
 * relocation against any `RxHdx*V29`, so V.29 is not V.21's shape here.
 *
 * EVERY HANDLER RETURNS 0 EXCEPT `RxHdxDataV29` AND ONE ARM OF
 * `RxHdxPrtcolV29`, and every one of them zeroes `*count` -- so a block is
 * always fully consumed and `V29RX_modem`'s loop always terminates after one
 * pass, whatever state it is in.
 *
 * START      demodulate, and advance as soon as a carrier appears.
 * EPOCH_DET  demodulate, drop to ERROR if the carrier goes, and advance when
 *            either the state's block budget runs out or `EpochDetectV29`
 *            reports the equaliser has been forced to adapt.
 * PROTOCOL   as EPOCH_DET, but it also DESCRAMBLES the block and returns what
 *            the demodulator produced -- and only on the call that advances.
 *            The blocks before that one are demodulated, descrambled and then
 *            reported as zero units.
 * DATA       see `RxHdxDataV29`.
 * IDLE       demodulate, and go back to DATA once the equaliser's `mse` has
 *            fallen to `V29RX_MSE_RECOVERED` -- the author's own words for
 *            that transition are "Decision error is small back to DATA mode".
 */
void RxNextStateV29(void *modem);

short RxHdxStartV29(void *modem, short *in, short *out, unsigned short *count);
short RxHdxIdleV29(void *modem, short *in, short *out, unsigned short *count);
short RxHdxPrtcolV29(void *modem, short *in, short *out, unsigned short *count);
short RxHdxEpochDetV29(void *modem, short *in, short *out,
		       unsigned short *count);

#endif /* DSPLIB_V29FAX_H */

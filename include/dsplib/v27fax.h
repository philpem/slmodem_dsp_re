/*
 * v27fax.h -- ITU-T V.27ter (fax): the receiver's primitives, the two
 * transmit-chain drivers, and both lifecycles.
 *
 * Sixteen functions sit directly on the FPM layer here.  Most of them reach
 * into the V.27ter modem instance, pick a sub-object out of it, and either
 * hand that sub-object to the module that owns it or read one field back out;
 * two are equaliser slicers, two drive a whole chain end to end, one is a
 * constant, and two are one-line wrappers around the V.27ter scrambler
 * module.
 *
 *   GetSNRV27             a constant
 *   V27RX_status          "did the caller supply somewhere to write"
 *   EpochDetectV27        one flag out of the equaliser
 *   CarrierDetectV27      the AGC's gate AND the symbol recovery's
 *   V27TX_status          fill a status block from the transmitter's
 *   V27RX_modem           drive the half-duplex receive state handler
 *   V27RX_delete          release the receiver
 *   V27TX_delete          release the transmitter
 *   ModDataV27            encode to symbols, then pulse-shape to samples
 *   V27RX_decision        the equaliser's slicer: nearest DPSK phase
 *   V27RX_eq_train        the equaliser's TRAINING slicer, and the handover
 *   QualityDetectV27      smooth the equaliser's MSE and grade it
 *   DataCarrierDetectV27  carrier up/down, and the V.21 escape
 *   DemodDataV27          AGC -> resample -> symbol recovery -> equalise
 *   ScrambleDataV27       the transmitter's scrambler, by one indirection
 *   DescrambleDataV27     the receiver's descrambler, by one indirection
 *
 * `tools/service.py` puts all of them on the FAX side.
 *
 * AND THE HALF-DUPLEX RECEIVE MACHINE ITSELF, which is six symbols and one
 * cycle:
 *
 *   RxNextStateV27        the transition table -- six states, five arms
 *   RxHdxStartV27         wait for a carrier
 *   RxHdxEpochDetV27      wait for the equaliser's epoch
 *   RxHdxPrtcolV27        train, descramble, hand over to DATA
 *   RxHdxIdleV27          carrier lost; wait for the error to come back down
 *   RxHdxErrorV27         demodulate and complain
 *
 * `RxNextStateV27` stores the addresses of four of them and each of the four
 * calls it back, so no proper subset of the five links -- see the note at the
 * top of `src/fax/v27.c`.
 *
 * ---------------------------------------------------------------------------
 * "THE INSTANCE" WAS TWO OBJECTS ALL ALONG, AND `V27RX_create` IS WHAT SHOWS IT
 *
 * This header used to say the instance was not modelled because `V27RX_create`
 * -- 2,210 bytes that lay it out -- was not reconstructed.  It is now, and the
 * first thing it settles is that there is no single instance: the RECEIVE
 * handle and the TRANSMIT handle are different allocations with different
 * layouts, and reading them as one would have been silently wrong.
 *
 *   - `V27RX_create` allocates 0x58 bytes, stores the shared block at +0x50
 *     and the receive block at +0x54, and fills +0x20..+0x34 with pointers
 *     INTO the equaliser.  `V27RX_delete` frees +0x50, +0x54 and the handle.
 *   - `V27TX_delete` frees `*(h + 0x28) + 0x5c` through `FPM_PPS_free` and
 *     `*(h + 0x24)`'s FIFO and `sgd`; `ModDataV27` and `ScrambleDataV27` reach
 *     the transmit chain through `*(h + 0x28)`.
 *
 * They cannot be the same handle.  On the receive one, +0x28 holds
 * `&fse->n_out` and +0x24 holds `fse->out_q`, so `FPM_PPS_free` would be given
 * a pointer into the middle of the equaliser -- and both deletes free the
 * handle itself, which would be a double free.  So `V27_OBJ_TX`,
 * `V27_OBJ_TXDATA` and the `V27TX_*` offsets below belong to the TRANSMIT
 * handle, and `V27_OBJ_SHARED`, `V27_OBJ_RX`, `V27_OBJ_STATUS` and the
 * `V27RXH_*` offsets to the RECEIVE one.  Finding F9304.
 *
 * THE RECEIVE HANDLE'S FIRST 28 BYTES ARE A `struct v27rx_cfg`.  `V27RX_create`
 * copies the caller's config there wholesale -- seven dwords, or `V27RX_CFG`
 * when the caller passes none -- and then reads three of its fields back:
 * `bit_rate` decides `V27SH_RATE`, `int_0014` decides `V27SH_TRAIN_LONG`, and
 * `ptr_0018` is handed to the resampler, the symbol recovery and the equaliser
 * as their context pointer.  `faxcfg.h` had already modelled that structure
 * from `init_vmi_v27rx`, which fills it and stores it in the VMI slot; this is
 * the other end of the same object.
 *
 * The accessors below stay `void *` plus named offsets rather than becoming a
 * `struct`, because every reconstructed V.27ter function already spells them
 * that way and a rewrite would move code generation everywhere at once for no
 * evidence.
 *
 * WHAT IS DIFFERENT HERE, AND IT IS THE STRONGEST EVIDENCE IN THIS FILE: the
 * receiver block is not opaque all the way down.  Four of the library's own
 * modules are EMBEDDED in it at fixed offsets, and `V27RX_delete` names three
 * of them by handing their addresses to `FPM_MRF_free`, `FPM_SRE_free` and
 * `FPM_FSE_free` -- so those regions are `struct fpm_mrf`, `struct fpm_sre`
 * and `struct fpm_fse` because a callee types them, not because a layout
 * looked plausible.  The fourth, the AGC, is named the same way by
 * `DemodDataV27`'s `FPM_AGC_agc(rx + 0x68, ...)`.
 *
 * That closes the block arithmetically, which is why the four offsets can be
 * trusted rather than merely recorded:
 *
 *     0x4c + sizeof(struct fpm_mrf) == 0x4c + 0x1c == 0x68   the AGC
 *     0x68 + sizeof(struct fpm_agc) == 0x68 + 0x2c == 0x94   the SRE
 *     0x94 + sizeof(struct fpm_sre) == 0x94 + 0x90 == 0x124  the FSE
 *     0x124 + sizeof(struct fpm_fse) == 0x124 + 0x4e18 == 0x4f3c
 *
 * and 0x4f3c is where the two scratch pointers `V27RX_delete` frees begin,
 * rounded up to 0x4f40.  Four independent modules tile the block with no gap
 * and no overlap.  Everything the five reading functions touch between 0x4c
 * and 0x4f3c therefore lands on a NAMED FIELD of one of those four structures,
 * and this header states which:
 *
 *     rx + 0x84   fpm_agc::signal     (0x68 + 0x1c)
 *     rx + 0xd4   fpm_sre::active     (0x94 + 0x40)
 *     rx + 0xdc   fpm_sre::adapt      (0x94 + 0x48)
 *     rx + 0x164  fpm_fse::lms_force  (0x124 + 0x40)
 *     rx + 0x168  fpm_fse::pll_on     (0x124 + 0x44)
 *     rx + 0x16c  fpm_fse::tilt_on    (0x124 + 0x48)
 *     rx + 0x170  fpm_fse::lms_on     (0x124 + 0x4c)
 *     rx + 0x176  fpm_fse::mse        (0x124 + 0x52)
 *
 * The last one is corroborated by the author's own words: `DataCarrierDetectV27`
 * compares rx + 0x176 against 0x3fff and prints "V27 Decoder error too big...
 * no carrier" when it is larger, and `fpm_fse.h` already calls +0x52 the
 * smoothed squared decision error.  Evidence class 1 and class 2 agreeing
 * without either being derived from the other.
 *
 * ---------------------------------------------------------------------------
 * THE DECODER BLOCK IS CONFIRMED TWICE TOO
 *
 * `V27RX_decision` reads `state->cfg.owner` and works relative to that.
 * `V27RX_create` sets `cfg.owner` to rx + 0x14 (99b65: `mov 0x54(%ebp),%edi`
 * then `add $0x14,%edi`, stored at the stack cfg's +0x2c) and fills the same
 * fields at rx-relative addresses, so every offset below is confirmed by the
 * function that reads it AND by the one that writes it (addresses are into
 * dsplibs.o):
 *
 *   99c54  rx + 0x20 = (params->f8 == 1)          -> dec + 0x0c
 *   99c6c  rx + 0x28 = V27RX_DEC_PHS_MASK[rate]   -> dec + 0x14
 *   99c2b  rx + 0x2a = 0                          -> dec + 0x16
 *   99ca4  rx + 0x2c = V27RX_DEC_PMAP[rate]       -> dec + 0x18
 *   99c7d  rx + 0x32 = 0x3299                     -> dec + 0x1e
 *   99cbf  rx + 0x34 = V27RX_DEC_LAST_PHASE[rate] -> dec + 0x20
 *   99c97  rx + 0x3a = 0                          -> dec + 0x26
 *
 * `V27RX_DEC_PMAP` and `V27RX_DEC_LAST_PHASE` are each two pointers indexed by
 * the rate, and the tables they point at are 4 entries for 2400 and 8 for
 * 4800 -- which is exactly the 4-or-8 `V27RX_decision` selects on dec + 0x0c.
 */

#ifndef DSPLIB_V27FAX_H
#define DSPLIB_V27FAX_H

struct fpm_fse;
struct v27rx_cfg;

/*
 * The transmit handle's first 32 bytes, `V27TX_create` copies wholesale from
 * the caller's `params` or, when NULL, from `V27TX_CFG` -- eight dwords read
 * off its own copy loop (0x9a375..0x9a3af), one more than `struct v29tx_cfg`
 * (0x1c) at the identical role, so this is NOT that struct one field short:
 * V.27ter's `int_001c` is its own field and V.29's `int_0018` fills the same
 * ROLE (FPM_PPS_CFG's `aux`) one field earlier.
 *
 * `protocol`/`bitrate` are RANK 2: `V27TX_status` reads the transmit handle's
 * own +0x00/+0x02 back as `V27STAT_PROTOCOL`/`V27STAT_TX_BPS`, and
 * `V27TX_create`'s only branch on the config tests +0x02 against 2400 and
 * 4800 in decimal.  `flags_0010`/`short_0012` split the fifth dword because
 * `V27TX_status` reads the handle's own +0x10 back as a SHORT
 * (`V27TX_HANDLE_FLAGS`); nothing splits the others, so they stay one `int`
 * each.  Usage inference for the rest, flagged per CLAUDE.md.
 */
struct v27tx_cfg {
	short	protocol;	/* +0x00                                     */
	short	bitrate;	/* +0x02  2400 or 4800                       */
	int	int_0004;	/* +0x04                                     */
	int	int_0008;	/* +0x08  60000, `v27rx_cfg`'s own value      */
	int	int_000c;	/* +0x0c  1; multiplies into the PPS gain     */
	short	flags_0010;	/* +0x10  `V27TX_HANDLE_FLAGS`                */
	short	short_0012;	/* +0x12                                     */
	int	int_0014;	/* +0x14  the transmit FIFO's element count is
					this * `V27TX_FRMSIZE[rate]`         */
	int	int_0018;	/* +0x18  `== 0` seeds `V27TXP_TRAIN_LONG`    */
	int	int_001c;	/* +0x1c  `FPM_PPS_CFG`'s `aux`, across the
					`(void *)(long)` idiom D1250 names   */
};

extern struct v27tx_cfg V27TX_CFG;

/* ------------------------------------------------------------------ */
/* The modem instance                                                  */

/*
 * The two sub-objects the instance owns, both pointers.  `V27RX_delete`
 * releases exactly these two and then the instance, which is the best single
 * witness to the layout there is without `V27RX_create`.
 */
#define V27_OBJ_SHARED		0x50	/* the half-duplex/detect block  */
#define V27_OBJ_RX		0x54	/* the receiver block            */

/*
 * The transmitter block.
 *
 * NAMED BY WHAT ITS CONTENTS ARE, not by position.  `V27TX_delete` releases
 * `*(modem + 0x28) + 0x5c` through `FPM_PPS_free` and `ModDataV27` hands
 * `*(modem + 0x28) + 0x2c` to `SMC_encoder` and `*(modem + 0x28) + 0x5c` to
 * `FPM_PPS_filter` -- a symbol-mapping encoder and a pulse-shaping filter,
 * which is the transmit chain and nothing else.  `ScrambleDataV27` takes its
 * scrambler out of the same block; the DEscrambler comes out of V27_OBJ_RX.
 */
#define V27_OBJ_TX		0x28	/* the transmitter block         */

/*
 * The block that holds the transmit data source: a byte FIFO and an `sgd`.
 *
 * TYPED BY ITS TWO MEMBERS AND BY NOTHING ELSE.  `V27TX_delete` hands +0x00
 * to `FIFO_delete` and +0x04 to `SGD_delete`, so those two fields are
 * `struct fax_fifo *` and `struct sgd *` because the functions that read them
 * say so.  That the BLOCK belongs to the transmit side is inference from the
 * only reconstructed function that reaches it being `V27TX_delete`;
 * `V27RX_delete` does not free it, so the two deletes do not overlap.  Its
 * extent is not established -- nothing reconstructed reads past +0x07.
 */
#define V27_OBJ_TXDATA		0x24
#define V27TXD_FIFO		0x00	/* struct fax_fifo * */
#define V27TXD_SGD		0x04	/* struct sgd *      */

/*
 * ITS EXTENT IS ESTABLISHED NOW: `V27TX_create` allocates it at 0x18 (24)
 * bytes and the half-duplex machine (`TxNextStateV27`, `SetScramblerV27` and
 * every `TxHdx*V27`) fills every byte of it.  Six more fields, all found by
 * `dis.py` against those functions rather than assumed from V.29's layout at
 * the same offsets -- V.27ter's own field WIDTHS and roles differ, most
 * visibly the two-int underrun/rate pair where V.29 has one.
 *
 *   V27TXP_INT_0008    int    zeroed by `V27TX_create`; `TxHdxDataV27` takes
 *                             a different underrun arm when non-zero and
 *                             nothing reconstructed ever sets it -- V.29's
 *                             `V29TXP_INT_0008` again, same shape, same dead
 *                             arm.
 *   V27TXP_RATE        short  `V27TX_SH_RATE_*`-style index (0 = 2400,
 *                             1 = 4800), re-read at every `V27TX_*` table
 *                             lookup rather than cached, `V27SH_RATE`'s own
 *                             discipline one modem over.
 *   V27TXP_TRAIN_LONG  short  `(cfg->int_0018 == 0)`, set once by
 *                             `V27TX_create` -- the transmit-side echo of
 *                             `V27SH_TRAIN_LONG`, indexing
 *                             `V27TX_ALT_COUNT`/`V27TX_EQCOND_COUNT`.
 *   V27TXP_PROCESS     v27tx_process_fn  the installed half-duplex handler.
 *   V27TXP_STATE       short  `TxNextStateV27`'s own dispatch index,
 *                             `V27TX_STATE_*` below.
 *   V27TXP_COUNTDOWN   short  the current handler's remaining budget for
 *                             this state; every `TxHdx*V27` decrements it
 *                             and calls `TxNextStateV27` at zero.
 */
#define V27TXP_INT_0008		0x08
#define V27TXP_RATE		0x0c
#define V27TXP_TRAIN_LONG	0x0e
#define V27TXP_PROCESS		0x10
#define V27TXP_STATE		0x14
#define V27TXP_COUNTDOWN	0x16
#define V27TXDATA_SIZE		0x18

/*
 * `TxNextStateV27`'s own dispatch index, at `V27TXP_STATE` -- an 11-entry
 * jump table (`ja default` bounds it at 0xa), read off the object's own
 * debug strings at .rodata.str1.1 0x4c21..0x4cdc, each naming the state
 * being LEFT (CLAUDE.md's evidence rank 1, same technique F9320/F9701 used
 * for V.29's four-entry machine).  The FUNCTION names and these STATE names
 * disagree in places exactly as V29TX_STATE_ALT/TxHdxABV29 do; not
 * reconciled, see TxNextStateV27's own comment in v27.c.
 */
#define V27TX_STATE_START	0
#define V27TX_STATE_QUIET	1
#define V27TX_STATE_CARR	2
#define V27TX_STATE_NOCARR	3
#define V27TX_STATE_ALT		4
#define V27TX_STATE_EQCOND	5
#define V27TX_STATE_SCR1	6
#define V27TX_STATE_DATA	7
#define V27TX_STATE_TURNOFF	8
#define V27TX_STATE_NOENG	9
#define V27TX_STATE_IDLE	10

typedef short (*v27tx_process_fn)(void *modem, unsigned short *in,
				  short *out, short *budget);

/*
 * The result word `V27TX_modem` would return (unwritten; nothing reconstructed
 * calls it yet), and the two flag bytes beside it -- `V27TX_create` zeroes
 * all three with one `movl $0x0,0x20(%ebp)` and the half-duplex machine reads
 * and writes them as bytes throughout, the same ONE-WORD-NOT-THREE-FIELDS
 * shape `V27_OBJ_STATUS` has on the receive side.
 */
#define V27TX_OBJ_RESULT	0x20	/* int; byte 0 is the status code   */
#define V27TX_OBJ_RESULT_B1	0x21	/* byte                              */
#define V27TX_OBJ_RESULT_B2	0x22	/* byte                              */

#define V27TX_RESULT_B1_BIT0	(1 << 0)
#define V27TX_RESULT_B1_BIT1	(1 << 1)
#define V27TX_RESULT_B2_BIT0	(1 << 0)

/*
 * The status codes `V27TX_OBJ_RESULT`'s low byte carries.  Usage inference --
 * each is what the naming FUNCTION writes into it, weakest of the three
 * evidence ranks, flagged per CLAUDE.md.  `V27TX_STATUS_DEFAULT` is the
 * exception: `TxNextStateV27`'s out-of-range arm and `V27TX_create`'s
 * neither-2400-nor-4800 arm both write the literal 5, matching the "keeps
 * whatever handler it had, reports DEFAULT" shape V29's own default arm has.
 */
#define V27TX_STATUS_DATA		0
#define V27TX_STATUS_TRAINING		1	/* Quiet/Alt/EQCond/SCR1 all
						   write this same value    */
#define V27TX_STATUS_ENTER_DATA_2400	2
#define V27TX_STATUS_ENTER_DATA_4800	3
#define V27TX_STATUS_IDLE		4
#define V27TX_STATUS_DEFAULT		5
#define V27TX_STATUS_UNDERRUN		6

/*
 * `V27TX_modem`'s own report when the FIFO could not take the whole block --
 * `V29TX_modem`'s identical `V29TX_RESULT_BYTE_07`, bare-named there too
 * because nothing but the literal byte value is established.
 */
#define V27TX_RESULT_BYTE_07		7

/*
 * The status word `V27RX_modem` returns, and the flags byte inside it.
 *
 * ONE 32-BIT WORD, NOT TWO FIELDS: `V27RX_create` zeroes all four bytes at
 * +0x1c with a single `movl $0x0,0x1c(%ebp)`, then ORs 0x50 into the byte at
 * +0x1d and stores 2 into the byte at +0x1c -- so +0x1d is byte 1 of the word
 * at +0x1c.  `V27RX_modem` clears one bit of that byte and returns the whole
 * word.
 */
#define V27_OBJ_STATUS		0x1c	/* int, returned by V27RX_modem  */
#define V27_OBJ_STATUS_FLAGS	0x1d	/* byte 1 of the word above      */

/*
 * BYTE 2 OF THE SAME WORD, and it is new here.
 *
 * It was not modelled while `RxNextStateV27` was unwritten because nothing
 * else in the object touches it: every one of its six sites is in the state
 * machine.  `V27RX_create`'s single `movl $0x0,0x1c(%ebp)` at 99d0d zeroes it
 * along with the rest of the word, which is what puts it inside the word
 * rather than beside it -- the same argument `V27_OBJ_STATUS_FLAGS` rests on.
 */
#define V27_OBJ_STATUS_FLAGS2	0x1e	/* byte 2 of the word above      */

/*
 * THE THREE NAMED BITS OF `V27_OBJ_STATUS_FLAGS`, AND THE NAMES ARE NEW.
 *
 * `V27_STATUS_FLAG_02` is RETIRED here.  It was named neutrally and correctly
 * when `V27RX_modem` was the only function in the tree that touched the byte:
 * "nothing traced sets it, so what it announces is not known".  `RxHdxErrorV27`
 * and `RxHdxDataV27` are what sets it, and with them the whole object can be
 * enumerated -- every `orb`, `andb`, `testb` and read-modify-write of
 * `obj + 0x1d` across all 40 V.27ter symbols, which is 23 sites.  Findings
 * F9230 and F9231.  A macro rename is a compile-time substitution, so nothing
 * in the codegen tier may move for it.
 *
 * The enumeration is this modem's own.  V.17's byte at `obj + 0x29` and V.21's
 * at `rx + 0x19` carry the same three bits in the same three roles; that is a
 * corroboration and not the derivation.
 *
 * ERROR (0x02) -- SET by `orb $0x2` at 0x0a2ddc inside `RxHdxErrorV27`, by the
 *   DEFAULT arm of `RxNextStateV27` (0x0a2e45 and its copy the other side of
 *   the debug print, beside status 3), and by the two transitions that install
 *   `RxHdxErrorV27` (`RxHdxPrtcolV27` 0x0a3158, `RxHdxEpochDetV27`).  CLEARED
 *   by `V27RX_modem` alone, `andb $0xfd` at 0x0a2c7d, at the top of every
 *   block.  SEEDED set by `V27RX_create` at 0x997b4.  Nothing in the object
 *   reads it: it leaves through the word `V27RX_modem` returns.
 *
 * CARRIER (0x20) -- CLEARED and then SET AGAIN if and only if
 *   `CarrierDetectV27` answers non-zero, in `RxHdxIdleV27` (`andb $0xdf` at
 *   0x0a3051, the call, `orb $0x20` at 0x0a3065); also cleared by
 *   `RxHdxStartV27` and set by `RxHdxPrtcolV27` and `RxHdxEpochDetV27`.
 *   `RxHdxDataV27` sets it on entry and clears it on the arm where
 *   `DataCarrierDetectV27` says the carrier has gone.  READ by
 *   `testb $0x20,0x1d(%esi)` at 0x0a3069 in `RxHdxIdleV27`, which gates that
 *   function's look at the equaliser's mse.  Both ends measured.
 *
 * LOW_SNR (0x80) -- CLEARED by `andb $0x7f` at 0x0a2d91 in `RxHdxDataV27` and
 *   SET at 0x0a2da7 if and only if `GetSNRV27` came back at or below
 *   `V27RX_SNR_THRESHOLD`.  `RxHdxDataV27` is the ONLY function in the object
 *   that touches this bit and NOTHING READS IT, so unlike V.17's the read end
 *   is not measured -- what the name rests on is the setter's own condition,
 *   which is complete and unambiguous.
 *
 *   AND THE SETTER IS UNREACHABLE.  `GetSNRV27` is `mov $0xa,%eax; ret`, so
 *   `GetSNRV27() <= 8` is false for every input the object can present and the
 *   bit is never raised.  The comparison is reproduced because it is there;
 *   `t_v27fax.c` records the arm as NOT reached rather than pretending to
 *   cover it.  Finding F9233.
 *
 * `V27RX_create`'s `orb $0x50` sets bits 4 and 6, which nothing else touches.
 */
/*
 * AND THE FOURTH BIT, WHICH THE STATE MACHINE OWNS ENTIRELY.
 *
 * DATA (0x01) -- SET by the two transitions that install `RxHdxDataV27`
 *   (`RxNextStateV27`'s PROTOCOL arm at 0x0a2f0a and its IDLE arm at
 *   0x0a2f65, both `orb $0x1,0x1d`).  CLEARED by every other arm of that
 *   function -- START, EPOCH_DET, DATA and the default -- and by nothing
 *   outside it.  So it is raised exactly while the machine is in
 *   V27RX_STATE_DATA.  NOTHING IN THE OBJECT READS IT: like the other three
 *   it leaves through the word `V27RX_modem` returns.
 *
 *   `V21RX_FLAG_DATA` in `v21fax.h` is bit 0 of the same byte of the same
 *   machine, set and cleared by the same two transitions, and V.21's IS read
 *   -- `V21RX_status` reports `quality` off it.  That is a corroboration of
 *   the name and not its derivation, which is the enumeration above.
 */
#define V27_STATUS_FLAG_DATA	(1 << 0)
#define V27_STATUS_FLAG_ERROR	(1 << 1)
#define V27_STATUS_FLAG_CARRIER	(1 << 5)
#define V27_STATUS_FLAG_LOW_SNR	(1 << 7)

/*
 * The one bit of `V27_OBJ_STATUS_FLAGS2` anything touches, and it is the exact
 * complement of `V27_STATUS_FLAG_DATA`: every arm of `RxNextStateV27` writes
 * both, and never the same way.  The DATA -> IDLE transition sets this and
 * clears that (0x0a2f3e / 0x0a2f42); all five other arms do the reverse.  So
 * it is raised exactly while the machine is in V27RX_STATE_IDLE.
 *
 * `V21RX_FLAG1_IDLE` is bit 0 of V.21's second flags byte with the same six
 * sites and the same pairing.  Nothing reads either.
 */
#define V27_STATUS_FLAG2_IDLE	(1 << 0)

/* See V27_STATUS_FLAG_LOW_SNR: a signed 16-bit `cmpw $0x8,%ax` then `jg`. */
#define V27RX_SNR_THRESHOLD	8

/*
 * THE STATUS BYTE at `V27_OBJ_STATUS`, ALL EIGHT VALUES, AND THE WRITE SET IS
 * NOW COMPLETE.
 *
 * Nothing in the object reads any of them -- the byte leaves through the word
 * `V27RX_modem` returns -- so a name can only be the site that writes it, and
 * that is what every name below is.  The previous revision of this paragraph
 * listed six values from the handlers it could see; writing the state machine
 * added 6 and 7 and settled what distinguishes them.  Finding F9302.
 *
 *   0  `RxHdxDataV27`, every block
 *   1  `RxHdxPrtcolV27` and `RxHdxEpochDetV27`, carrier-present arm
 *   2  `RxHdxStartV27`, every block
 *   3  `RxNextStateV27`'s default arm; `V27RX_create` on an unknown rate
 *   4  the same two handlers' carrier-lost arm
 *   5  `RxHdxIdleV27`, every block
 *   6  the two handovers into `RxHdxDataV27`, when `V27SH_RATE` is 2400
 *   7  the same two handovers, when it is 4800
 *
 * 6 AND 7 ARE ONE SITE-PAIR AND ONE SELECTOR.  `RxHdxPrtcolV27`'s
 * countdown-expiry arm (0x0a3161) and `RxNextStateV27`'s IDLE arm (0x0a2f6d)
 * both compute `V27SH_RATE < 1 ? 6 : 7` with the same branchless idiom
 * (`cmp $0x1,%cx` / `sbb` / `add $0x7`), and both are immediately followed by
 * the transition that installs `RxHdxDataV27`.  So the pair reports the rate
 * the machine is about to carry data at; `V27SH_RATE` is where the 2400 and
 * the 4800 come from, and they are the author's own numbers.
 *
 * V.21's byte carries 0, 3, 4 and 5 in exactly these four roles (`v21fax.h`),
 * which corroborates those four and says nothing about 1, 2, 6 or 7 -- the two
 * modems do not have the same states.
 */
#define V27_STATUS_DATA			0
#define V27_STATUS_TRAINING		1
#define V27_STATUS_START		2
#define V27_STATUS_DEFAULT		3
#define V27_STATUS_ERROR		4
#define V27_STATUS_IDLE			5
#define V27_STATUS_ENTER_DATA_2400	6
#define V27_STATUS_ENTER_DATA_4800	7

/*
 * THE RECEIVE HANDLE, +0x20 .. +0x54, and every one of these is written by
 * `V27RX_create` and by nothing else in the object.
 *
 * FIVE OF THEM ARE A VIEW OF THE EQUALISER, taken once at the end of
 * construction and never refreshed: `FPM_FSE_init` has just allocated those
 * four buffers, so the pointers stay valid for the life of the object, and
 * `n_out` is handed over BY ADDRESS rather than by value (`lea 0x182(%ebx)`
 * at 0x99ce5, which is `V27RX_FSE + offsetof(struct fpm_fse, n_out)`).  What
 * reads them is outside this object -- nothing reconstructed does -- so the
 * name says which field of `struct fpm_fse` each one is and no more than that.
 */
#define V27RXH_EQ_OUT_I		0x20	/* fse->out_i                      */
#define V27RXH_EQ_OUT_Q		0x24	/* fse->out_q                      */
#define V27RXH_EQ_N_OUT		0x28	/* &fse->n_out, the ADDRESS        */
#define V27RXH_EQ_ICOEFF	0x2c	/* fse->icoeff                     */
#define V27RXH_EQ_QCOEFF	0x30	/* fse->qcoeff                     */
#define V27RXH_EQ_TAPS		0x34	/* short: fse->cfg.taps            */

/*
 * The six `V27RX_create` zeroes and nothing else in the object touches.  Their
 * WIDTHS are the object's -- three `movl` and two `movw` -- and their meanings
 * are not established, so they are named by offset.
 */
#define V27RXH_ZERO_38		0x38	/* int   */
#define V27RXH_ZERO_3C		0x3c	/* int   */
#define V27RXH_ZERO_40		0x40	/* short */
#define V27RXH_ZERO_44		0x44	/* int   */
#define V27RXH_ZERO_48		0x48	/* int   */
#define V27RXH_ZERO_4C		0x4c	/* short */

/*
 * `sysdep_malloc`'s three arguments, which are the only statement in the
 * object about how big any of these are.  The handle's 0x58 covers +0x54 and
 * its pointer with nothing over; the two scratch buffers are 160 and 164
 * `short`, and `V27RX_create` zeroes the first 160 entries of each.
 */
#define V27RXH_SIZE		0x58
#define V27RX_BLOCK_SIZE	0x4f58
#define V27RX_BUF_A_BYTES	0x140
#define V27RX_BUF_B_BYTES	0x148
#define V27RX_BUF_ZERO		160	/* entries cleared in EACH buffer  */
#define V27SH_SIZE		0x50
#define V27SH_BUF_BYTES		0x140

/*
 * The two bits `V27RX_create`'s `orb $0x50,0x1d(%ebp)` raises (0x99d14).
 * Nothing else in the object sets, clears or reads either, so this is the
 * seed and not a flag pair -- naming the bits would be inventing two meanings
 * out of one store.
 */
#define V27_STATUS_FLAGS_SEED	0x50

/* ------------------------------------------------------------------ */
/* The receiver block, at *(void **)(obj + V27_OBJ_RX)                  */

/*
 * Three caller-owned enables, each ANDed with the AGC's signal gate by
 * `DemodDataV27` and stored into one named flag of one sub-object.  The role
 * is not inferred from the name -- there is none -- it is read off the
 * destination:
 *
 *     rx + 0x04 & signal  ->  fpm_sre::adapt     phase update
 *     rx + 0x08 & signal  ->  fpm_fse::pll_on    carrier recovery
 *     rx + 0x10 & signal  ->  fpm_fse::lms_on    coefficient adaptation
 *
 * `V27RX_create` sets all three to 1 at 99ceb..99d06, together with a fourth
 * at +0x0c set to 0 which nothing in this file reads.
 */
#define V27RX_EN_SRE_ADAPT	0x04
#define V27RX_EN_FSE_PLL	0x08
#define V27RX_EN_FSE_LMS	0x10

/*
 * A FOURTH caller-owned enable, at +0x00 -- `V27RX_create` sets it to 1
 * alongside the three above (`FIELD_I(rx, 0x00) = 1;`, this file's own
 * `V27RX_create`), and `V27RX_control` is the only function that clears it,
 * gated on its own request's mask byte.  Neither function types what it
 * selects; usage inference only.
 */
#define V27RX_EN_00		0x00

/* The decoder block; see the header comment. */
#define V27RX_DEC		0x14

/*
 * The receiver's `struct sdmv27`, and the transmitter's.
 *
 * TYPED BY THE CALLEE and by nothing else: `DescrambleDataV27` hands
 * `rx + 0x3c` to `SDMv27_descrambler` and `ScrambleDataV27` hands
 * `tx + 0x1c` to `SDMv27_scrambler`, so both regions are `struct sdmv27`
 * because the function that reads them says so.  `sizeof(struct sdmv27)` is
 * 14 and the decoder block's last field ends at rx + 0x3a, so the receiver's
 * copy tiles 0x3c..0x4a and leaves nothing over before `V27RX_MRF` at 0x4c.
 */
#define V27RX_SDM		0x3c	/* struct sdmv27, the descrambler  */
#define V27TX_SDM		0x1c	/* struct sdmv27, the scrambler    */

/*
 * THE TRANSMITTER BLOCK TILES EXACTLY, the same way the receiver's does, and
 * that is what closes it rather than a plausible-looking list of offsets.
 * Three of the four regions are TYPED BY A CALLEE -- `ModDataV27` hands +0x2c
 * to `SMC_encoder` and +0x5c to `FPM_PPS_filter`, `V27TX_delete` hands +0x5c
 * to `FPM_PPS_free`, and both of `ModDataV27`'s calls take +0x08 as their
 * `struct fpm_smc_ring *` (the encoder's destination and the shaper's source
 * in one call pair).  `ScrambleDataV27` types the fourth.  Then:
 *
 *     0x08 + sizeof(struct fpm_smc_ring) == 0x08 + 0x14 == 0x1c   the sdmv27
 *     0x1c + sizeof(struct sdmv27)       == 0x1c + 0x0e == 0x2a -> 0x2c  smc
 *     0x2c + sizeof(struct fpm_smc)      == 0x2c + 0x30 == 0x5c   the shaper
 *     0x5c + sizeof(struct fpm_pps)      == 0x5c + 0x38 == 0x94
 *
 * -- four modules with one two-byte alignment gap and no overlap.  Finding
 * F9121.
 *
 * THAT ARITHMETIC IS WHAT IDENTIFIES tx + 0x10, and the first reading of it
 * was WRONG.  `V27TX_delete` frees `*(int *)(tx + 0x10)` and the obvious
 * reading is a scratch allocation of the transmitter's own -- but 0x10 is
 * 0x08 + 0x08, which is `fpm_smc_ring::sym`, the symbol-index buffer the
 * encoder writes and the shaper reads.  There is no room for a field of its
 * own there.  So the delete releases the RING's buffer, and the reconstruction
 * spells it that way.
 */
#define V27TX_RING		0x08	/* struct fpm_smc_ring */
#define V27TX_SMC		0x2c	/* struct fpm_smc      */
#define V27TX_PPS		0x5c	/* struct fpm_pps      */

/* The four embedded FPM modules.  They tile 0x4c..0x4f3c exactly. */
#define V27RX_MRF		0x4c	/* struct fpm_mrf   */
#define V27RX_AGC		0x68	/* struct fpm_agc   */
#define V27RX_SRE		0x94	/* struct fpm_sre   */
#define V27RX_FSE		0x124	/* struct fpm_fse   */

/*
 * The two scratch buffers, allocated by `V27RX_create` and released by
 * `V27RX_delete`.  `DemodDataV27` uses A for the resampler's output and the
 * symbol recovery's input, and B for the symbol recovery's output and the
 * equaliser's input -- so B is the one the equaliser reads and A is the one
 * everything upstream shares.
 */
#define V27RX_BUF_A		0x4f40	/* short *  */
#define V27RX_BUF_B		0x4f44	/* short *  */

/*
 * `QualityDetectV27`'s own state, and it is the only reader or writer of all
 * four.  Their ROLES are measured -- what each one does is below -- and their
 * MEANINGS are usage inference, so the names say what happens rather than what
 * it is for.
 *
 * `q_acc` is a first-order smoother over `fpm_fse::mse` at 0.9/0.1 in Q15
 * with rounding; `q_count` counts the blocks that have gone into it and stops
 * the smoother at 0x31; at exactly 0x32 the accumulated value is compared
 * against `q_limit` and `q_flag` is set if it did NOT exceed it.
 */
#define V27RX_Q_ACC		0x4f48	/* short            */
#define V27RX_Q_COUNT		0x4f4a	/* unsigned short   */
#define V27RX_Q_LIMIT		0x4f4c	/* unsigned short   */
#define V27RX_Q_FLAG		0x4f4e	/* short            */

/*
 * The energy-drop detector's state, read and written only by
 * `DataCarrierDetectV27`, and named from the author's own words: the branch
 * that clears carrier prints "sudden energy drop > 8[dB], no carrier", and the
 * comparison it prints on is `FPM_rms(block) < (ref * 0x32fe) >> 15`.
 * 0x32fe / 32768 is 0.3984, and 20*log10(0.3984) is -8.0 dB -- so the constant
 * and the string agree to the digit the string quotes.
 *
 * `V27RX_create` sets `enable` and `ref` together at 99c1d.
 */
#define V27RX_RMS_ON		0x4f50	/* short: run the detector at all */
#define V27RX_RMS_REF		0x4f52	/* short: the level to fall from  */
#define V27RX_RMS_COUNT		0x4f54	/* unsigned short: blocks since
					 * `ref` was last republished     */

/* The -8 dB threshold, Q15.  See V27RX_RMS_REF. */
#define V27RX_RMS_DROP_Q15	0x32fe

/*
 * The two `fpm_fse::mse` thresholds, both compared with `>`.
 *
 * 0x3fff is the carrier test and the author names it twice over: above it
 * `DataCarrierDetectV27` prints "V27 Decoder error too big... no carrier".
 */
#define V27RX_MSE_NO_CARRIER	0x3fff

/*
 * The THIRD `fpm_fse::mse` threshold, and the only one compared with `<=`.
 *
 * `RxHdxIdleV27` restarts the machine when the carrier is up AND the error has
 * come back down to this (`cmpw $0x1fff,0x176(%ebx)` / `jle` at 0x0a3072), and
 * the author names the branch: `.rodata.str1.4 + 0x12b9c` is "Decision error
 * is small back to DATA mode !!!\n" and is printed on it and nowhere else.  So
 * the constant, the comparison and the string agree, and the string is what
 * makes this the "small enough" threshold rather than merely a number.  It is
 * exactly half `V27RX_MSE_NO_CARRIER`; nothing says the two are related beyond
 * that and this does not claim they are.
 */
#define V27RX_MSE_IDLE_OK	0x1fff

/*
 * The one value `QualityDetectV27` returns that is a CODE rather than the
 * carrier term itself, and the one value `RxHdxDataV27` tests it against
 * (`cmp $0x2,%ax` / `setne`).  Two functions, one number, and the second is
 * what makes it a shared constant rather than a literal in the first.
 * `V17_QUALITY_UNRELIABLE` in `v17fax.h` is the same code in the same place.
 */
#define V27_QUALITY_UNRELIABLE	2

/*
 * How many decisions must have been taken before the mse test is allowed to
 * drop carrier, compared against the decoder's own symbol counter.
 *
 * NEUTRAL ON PURPOSE: what is established is that `DataCarrierDetectV27`
 * gates on `dec->sym_count > 0x5db` and that `V27RX_decision` is the only
 * thing that advances that counter.  Whether 1500 symbols is a training
 * length, a timeout or something else is not read off anything.
 */
#define V27RX_DEC_SETTLED	0x5db

/*
 * The largest symbol count `DemodDataV27` will accept from the symbol
 * recoverer without complaining, and the author's own words for what a larger
 * one is: `.rodata.str1.4 + 0x12ca0` is "ERROR: SRE buffer violation(%d)".
 *
 * The test is `> 0xa4`, unsigned and 16-bit (`cmp $0xa4,%bx` / `ja`).  That
 * the buffer named is `V27RX_BUF_B` is INFERENCE and not measured: it is the
 * destination `FPM_SRE_recover` was given, and nothing reconstructed
 * allocates it.  V.29's demodulator carries the identical
 * check against the identical string, which is why the constant is stated per
 * modem rather than shared: nothing establishes that the two buffers are the
 * same size, only that the two messages are the same message.
 */
#define V27RX_SRE_MAX		0xa4

/* ------------------------------------------------------------------ */
/* The decoder block, at rx + V27RX_DEC (and at fse->cfg.owner)          */

/*
 * THE EPOCH DETECTOR'S SIX-SHORT HISTORY, dec + 0x00 .. dec + 0x0b.
 *
 * `V27RX_epoch_det` is the only thing in the object that touches any of the
 * six, and it uses them as three consecutive constellation points: the newest
 * pair at +0x00/+0x02, one symbol back at +0x04/+0x06 and two symbols back at
 * +0x08/+0x0a.  Every call shifts the pairs along by one and drops the oldest.
 *
 * THAT THE PAIRS ARE (I, Q) IS RANK 2 AND NOT A GUESS: the values written into
 * +0x00 and +0x02 are `state->out_i[n]` and `state->out_q[n]`, and those two
 * fields are `fpm_fse.h`'s, named there from `FPM_FSE_receive` with nothing to
 * do with V.27ter.  WHICH PAIR IS "one back" and which is "two back" follows
 * from the shift the function performs and from nothing else.
 *
 * ALL SIX ARE READ `movzwl` AND EVERY DIFFERENCE IS NARROWED BACK TO `short`,
 * so the extension is FREE at every one of those sites (finding F614) and the
 * `unsigned short` here is what finding F7803's rule reads off the object --
 * the declared type of what is loaded -- and not something a test can measure.
 * `t_v27fax.c` asserts the signed-reading variant separates nothing, which is
 * the honest form of that claim.  The two SAMPLES differenced against them are
 * a different matter: they are squared, so their `movswl` IS forced and is
 * measured.  Finding F9234.
 *
 * The block is 12 bytes and `V27DEC_EIGHT_PHASE` is at 0x0c, so the six tile
 * the whole gap below it with nothing over.
 */
#define V27DEC_EPOCH_I0		0x00	/* unsigned short, the newest      */
#define V27DEC_EPOCH_Q0		0x02
#define V27DEC_EPOCH_I1		0x04	/* one symbol back                 */
#define V27DEC_EPOCH_Q1		0x06
#define V27DEC_EPOCH_I2		0x08	/* two symbols back                */
#define V27DEC_EPOCH_Q2		0x0a

/*
 * Four phases or eight.
 *
 * `V27RX_decision` computes `n = dec->eight_phase ? 8 : 4` -- `cmp $0x1,%ebp;
 * sbb %ebx,%ebx; and $0xfffffffc,%ebx; add $0x8,%ebx`, which is GCC's
 * branchless form of a two-constant conditional and sets the mask on
 * `%ebp == 0` alone.  `V27RX_create` fills it with `params->f8 == 1`, and the
 * same `params->f8` indexes `V27RX_DEC_PMAP` and `V27RX_DEC_LAST_PHASE`,
 * whose entry 1 points at eight-entry tables and whose entry 0 points at
 * four-entry ones.  So the field is the rate and the count follows it.
 */
#define V27DEC_EIGHT_PHASE	0x0c	/* int    */

/*
 * Which of two equaliser training lengths `V27RX_eq_train` waits out.
 *
 * NON-ZERO SELECTS THE SHORT ONE.  The object computes
 * `cmp $0x1,%esi; sbb %edi,%edi; and $0x3b6,%edi; add $0x32,%edi` -- GCC's
 * branchless two-constant conditional again -- so the limit is 0x32 when the
 * field is non-zero and 0x32 + 0x3b6 == 0x3e8 when it is zero.
 *
 * `V27RX_create` fills it at 99c8d with `sete %dl` on `cmpw $0x0,0xa(%ebx)`,
 * a 32-bit store, which is what makes it an `int` rather than a flag byte.
 * WHAT `params->f0a` MEANS IS NOT ESTABLISHED and this header does not guess:
 * what is established is that one value of it costs fifty symbols of training
 * and the other a thousand.  The word "training" is the AUTHOR'S -- the
 * function reading this field is called `V27RX_eq_train` -- and not ours.
 */
#define V27DEC_TRAIN_SHORT	0x10	/* int    */

/* The two limits above, in symbols. */
#define V27DEC_TRAIN_SYMS_SHORT	0x32
#define V27DEC_TRAIN_SYMS_LONG	0x3e8

/*
 * The phase index mask, `V27RX_DEC_PHS_MASK[rate]`.  3 or 7 for the two
 * table lengths above; the object never assumes that and neither does this.
 */
#define V27DEC_PHASE_MASK	0x14	/* unsigned short */

/* The previous symbol's phase index, the DPSK reference. */
#define V27DEC_LAST		0x16	/* short */

/* `V27RX_DEC_PMAP[rate]`: the bits each phase STEP carries. */
#define V27DEC_PMAP		0x18	/* const short * */

/*
 * Symbols the CURRENT slicer has taken, and it is a SEPARATE counter from
 * `V27DEC_SYM_COUNT` below: this one is zeroed by `V27RX_create` (99c77,
 * `movw $0x0,0x30(%ecx)`, which is dec + 0x1c), while `V27DEC_SYM_COUNT` is
 * advanced by every slicer and saturates.
 *
 * IT IS SHARED BETWEEN TWO SLICERS AND ONE OF THEM RESETS IT, which the name
 * "train count" alone does not say.  `V27RX_epoch_det` -- the slicer
 * `V27RX_create` installs FIRST -- increments it on every call and compares it
 * against `V27EPOCH_SYMS_SHORT`/`_LONG`; on the call where it hands over to
 * `V27RX_eq_train` it stores 0xffff and then falls into the same unconditional
 * increment, so the counter comes out at ZERO and the training slicer starts
 * from a clean count.  `V27RX_eq_train` then increments it in turn and
 * compares it against `V27DEC_TRAIN_SYMS_SHORT`/`_LONG`.  So the object has a
 * three-stage slicer chain -- epoch detect, train, run -- and this one counter
 * times the first two.  Finding F9234.
 *
 * READ SIGNED AND WRITTEN UNSIGNED, BY BOTH FUNCTIONS.  The limit compare is
 * `movswl 0x1c(%ebp),%eax` and the increment is `movzwl` / `inc` / 16-bit
 * store, and finding F614 is why that is not a contradiction: the increment's
 * extension is dead, the compare's is not.  Both spellings are reproduced.
 */
#define V27DEC_TRAIN_COUNT	0x1c	/* unsigned short */

/*
 * The epoch detector's leaky energy average, and the only field of the decoder
 * block that `V27RX_create` seeds with something other than zero or a table
 * entry.
 *
 * `V27RX_create` writes 0x3299 here at 99c7d (rx + 0x32).  Until
 * `V27RX_epoch_det` was read this was recorded under `V27DEC_MAG` as a second,
 * unread occurrence of that constant; it is not a second occurrence of
 * anything, it is this field's seed, and the coincidence with `V27DEC_MAG`'s
 * value is exactly that.  See the note at `V27DEC_MAG`.
 *
 * `V27RX_epoch_det` updates it as `(31 * avg) >> 5 + (energy >> 5)` -- an
 * arithmetic SHIFT and not a division, which the object settles by emitting
 * `shl $5` / `sub` / `sar $5` with no rounding correction anywhere.
 */
#define V27DEC_EPOCH_AVG	0x1e	/* short */

/*
 * How many symbols `V27RX_epoch_det` waits before it will judge, chosen by the
 * SAME `V27DEC_TRAIN_SHORT` that chooses `V27RX_eq_train`'s two lengths and by
 * the same branchless idiom: `cmp $0x1,%ecx` / `sbb %ebx,%ebx` /
 * `and $0x1e,%ebx` / `add $0xa,%ebx`, which is 0x0a when the field is non-zero
 * and 0x0a + 0x1e == 0x28 when it is zero.  Non-zero selects the short one, as
 * it does there.
 *
 * The comparison is `count > limit`, strictly, from `jle` on the fall-through.
 */
#define V27EPOCH_SYMS_SHORT	10
#define V27EPOCH_SYMS_LONG	40

/*
 * The two constants of the leaky average, and the trigger.
 *
 * The average is `(31 * avg) / 32 + energy / 32` written as shifts, so
 * `V27EPOCH_AVG_SHIFT` is both the divisor's log2 and the multiplier's
 * complement.  The epoch is declared when the summed squared difference
 * EXCEEDS `V27EPOCH_TRIGGER` times the freshly updated average -- `shl $2`
 * then `cmp` / `jle`, so strictly greater.
 */
#define V27EPOCH_AVG_SHIFT	5
#define V27EPOCH_AVG_WEIGHT	31	/* (1 << V27EPOCH_AVG_SHIFT) - 1   */
#define V27EPOCH_TRIGGER	4

/* `V27RX_DEC_LAST_PHASE[rate]`: the phase angle of each index. */
#define V27DEC_ANGLES		0x20	/* const short * */

/*
 * The constellation angle the previous decision produced -- `angles[last]`,
 * stored by `V27RX_eq_train` on its way out and subtracted from the next
 * measured angle on its way in.
 *
 * IT IS NOT `V27DEC_LAST` SPELLED TWICE.  `V27DEC_LAST` is the phase INDEX
 * and this is the ANGLE that index selects; the training slicer writes both
 * on the same pass, from the same table load, to two different offsets.
 * `V27RX_create` zeroes it at 99c83 (`movw $0x0,0x38(%ecx)`, dec + 0x24).
 *
 * `V27RX_decision` does not read or write it -- it subtracts
 * `angles[dec->last]` afresh -- so the two slicers do not share this state.
 */
#define V27DEC_ANGLE_PREV	0x24	/* short */

/*
 * Decisions taken, saturating.  `V27RX_decision` increments it and, when the
 * increment would reach 0x8000, stores 0x4000 instead -- so it never goes
 * negative and never stops moving.  `DataCarrierDetectV27` is its only reader.
 */
#define V27DEC_SYM_COUNT	0x26	/* unsigned short */

/*
 * The magnitude every decision reports, a literal in `V27RX_decision`.
 *
 * `V27RX_create` also writes 0x3299 into dec + 0x1e.  That paragraph used to
 * read "nothing reconstructed reads that field, so the two are independent
 * occurrences of one number"; the first half has expired -- dec + 0x1e is
 * `V27DEC_EPOCH_AVG` and `V27RX_epoch_det` both reads and writes it -- and the
 * second half survives unchanged and is now better supported.  The two ARE
 * independent occurrences: one is a slicer's constant magnitude and the other
 * is an energy average's seed, so 0x3299 appearing twice is a coincidence of
 * the disassembly after all, and neither use is evidence for the other.
 */
#define V27DEC_MAG		0x3299

/*
 * The full circle, in the units `fpm_fse` hands the slicer.
 *
 * `FPM_atan`'s output convention, restated by this function three times: the
 * phase difference is folded into [0, 0x8000] by adding 0x8000 when it is
 * negative and subtracting 0x8000 when it exceeds 0x8000.
 */
#define V27DEC_PHASE_FULL	0x8000

/*
 * Half and a quarter of that, both used by `V27RX_eq_train` alone.
 *
 * It folds the phase difference into [-0x4000, +0x4000] -- half a revolution
 * either side, where `V27RX_decision` folds into [0, 0x8000] -- and then
 * advances the reference only when what is left EXCEEDS a quarter of a
 * revolution.  Written as fractions of `V27DEC_PHASE_FULL` because that is
 * what they are; the object holds them folded to 0x4000 and 0x2000.
 */
#define V27DEC_HALF_TURN	(V27DEC_PHASE_FULL / 2)
#define V27DEC_QUARTER_TURN	(V27DEC_PHASE_FULL / 4)

/* ------------------------------------------------------------------ */
/* The shared block, at *(void **)(obj + V27_OBJ_SHARED)                */

/*
 * The half-duplex receive machine and the tone detectors live together here.
 * `V27RX_delete` releases +0x00, +0x18 and +0x1c and then the block itself,
 * which fixes what it owns; `V27RX_modem` calls through +0x0c; and
 * `V27RX_create` stores `RxHdxStartV27` into +0x0c at 996f9, which is what
 * makes +0x0c the CURRENT STATE HANDLER rather than a vtable slot.
 */
#define V27SH_MTD		0x00	/* struct fpm_mtd *, deleted first */
#define V27SH_STATE		0x0c	/* v27_rx_state_fn                 */

/*
 * An int `RxHdxDataV27` requires to be ZERO before it will demodulate, and the
 * SECOND half of its gate: the carrier must be up AND this must be clear.
 *
 * NEUTRAL.  It has exactly one reader in the whole object and no writer at
 * all, so what sets it is outside what has been read.  It is an `int`:
 * `mov 0x4(%ecx),%edx` then `test %edx,%edx`, 32 bits at both ends.
 *
 * IT IS NOT `V27RX_EN_SRE_ADAPT`, WHICH IS A DIFFERENT BLOCK.  `DemodDataV27`'s
 * `mov 0x4(%edx),%ecx` at 0x0a59e1 reads +0x04 of the RECEIVER block
 * (`V27_OBJ_RX`); this is +0x04 of the SHARED block (`V27_OBJ_SHARED`).  The
 * two offsets are equal and the two fields are not.  Finding F9232.
 */
#define V27SH_INT_0004		0x04	/* int */

/*
 * THE RECEIVE STATE NUMBER.  `V27SH_SKIP_TONE` IS RETIRED.
 *
 * F9235 recorded this field as "almost certainly the receive state number",
 * named it for the one thing that READ it -- `DemodDataV27` skips its tone
 * test while it is zero -- and said the rename belonged with the pass that
 * wrote `RxNextStateV27`.  That pass is done and the rename is taken.
 *
 * IT IS THE AUTHOR'S OWN WORD, RANK 1.  `RxNextStateV27` switches on this
 * field through the jump table at `.rodata:0xc31c` and each of the five arms
 * begins by printing the state's name from `.rodata.str1.1`:
 *
 *     index 0  ->  0x0a2e7a  "V27RX_STATE_START\n"       (0x4bc1)
 *     index 1  ->  0x0a2ea7  "V27RX_STATE_EPOCH_DET\n"   (0x4baa)
 *     index 2  ->  0x0a2ed9  "V27RX_STATE_PROTOCOL\n"    (0x4bf8)
 *     index 3  ->  0x0a2f17  "V27RX_STATE_DATA\n"        (0x4be6)
 *     index 4  ->  0x0a2f4b  "V27RX_STATE_IDLE\n"        (0x4bd4)
 *     default  ->  0x0a2e30  "V27RX_DEFAULT, %d\n"       (0x4b97)
 *
 * so the five names AND their five numbers come off the object's own text.
 * `tools/relocscan.py --at .rodata.str1.1:0xNNNN` is what pairs a string with
 * its site, because the reference is an `R_386_32` against the section symbol
 * with the offset as an inline addend (finding F604).
 *
 * Each arm also stores the NEXT state's number and the NEXT state's handler
 * together, and the two agree 1:1 across all five, which is the second
 * derivation: 1 goes with `RxHdxEpochDetV27`, 2 with `RxHdxPrtcolV27`, 3 with
 * `RxHdxDataV27` and 4 with `RxHdxIdleV27`.  `V27RX_create` stores 0 with
 * `RxHdxStartV27` (0x996ed and 0x996f9).  Finding F9300.
 *
 * READ SIGNED: `movswl 0x10(%edx),%eax` before the range check, so the switch
 * is over `short` and a value above 4 -- or below 0 -- takes the default arm.
 */
#define V27SH_RX_STATE		0x10	/* short: V27RX_STATE_*            */

/*
 * BLOCKS LEFT IN THE CURRENT STATE.
 *
 * `RxHdxPrtcolV27` and `RxHdxEpochDetV27` are the only two handlers that count
 * it: each decrements it once per block on its carrier-present arm and hands
 * over to `RxNextStateV27` when the result is at or below zero.  Every arm of
 * `RxNextStateV27` seeds it for the state it is entering, and `V27RX_create`
 * zeroes it at 0x996f3 beside the state number.
 *
 * WRITTEN AND COMPARED IN SIXTEEN BITS, and the two readings are not the same:
 * the decrement loads `movzwl` and the exhaustion test is `test %cx,%cx` /
 * `jle`, so the count is unsigned in memory and the test is signed.  Both
 * spellings are reproduced.  `struct v21_rx_hdx::countdown` is the same field
 * of the same machine, read the same two ways.
 */
#define V27SH_COUNTDOWN		0x12	/* unsigned short */

/*
 * THE BIT RATE, AS AN INDEX, AND THE TWO NUMBERS ARE THE AUTHOR'S.
 *
 * `V27RX_create` reads the caller's `modem + 0x04` signed and compares it
 * against two literals before it writes this field (0x99794..0x997ae):
 *
 *     0x960  == 2400  ->  0   (0x99ecd)
 *     0x12c0 == 4800  ->  1   (0x99ed8)
 *     anything else   ->  1, and the status byte goes to V27_STATUS_DEFAULT
 *                            with V27_STATUS_FLAG_ERROR raised
 *
 * 2400 and 4800 bit/s are V.27ter's two rates to the digit, so the field is
 * the rate and the encoding is which of the two.  Everything else that reads
 * it agrees: it indexes `V27_MTD_COEFF_2400`/`_4800`, `V27RX_MRF_*`,
 * `V27RX_SRE_*`, `V27RX_FSE_*`, `V27RX_DEC_PMAP`, `V27RX_DEC_LAST_PHASE` and
 * `V27RX_DEC_PHS_MASK`, and `V27DEC_EIGHT_PHASE` is `rate == 1` -- eight
 * phases at 4800 and four at 2400, which is the recommendation's own
 * constellation.  Finding F9301.
 *
 * `V27RX_create` reads it `movswl` at nine sites and `movzwl` at none, which
 * is why it is a `short` here; the state machine's own two reads are
 * `movzwl` / 16-bit compare, where the extension is dead (finding F614).
 */
#define V27SH_RATE		0x08	/* short: V27SH_RATE_*             */
#define V27SH_RATE_2400		0
#define V27SH_RATE_4800		1

/*
 * LONG TRAINING RATHER THAN SHORT, and it is one flag with two readers.
 *
 * `V27RX_create` sets it from the caller's `modem + 0x14` at 0x996e9 --
 * `cmpl $0x0,0x14(%ebp)` / `sete` -- so it is raised when that word is ZERO.
 * The two things that read it are:
 *
 *   - `V27RX_create` itself at 0x99c61, where `V27DEC_TRAIN_SHORT` is set to
 *     `this field == 0`.  So this field non-zero means the equaliser takes
 *     `V27DEC_TRAIN_SYMS_LONG` (1000 symbols) rather than `..._SHORT` (50),
 *     and `V27RX_epoch_det` waits `V27EPOCH_SYMS_LONG` rather than `_SHORT`.
 *   - `RxNextStateV27`'s EPOCH_DET arm, where it chooses the PROTOCOL state's
 *     countdown: 1 or 2 blocks when it is clear, 33 or 45 when it is set.
 *
 * Both readers make the same choice between a short timing set and a long one,
 * and the word "training" is the AUTHOR'S -- it is what `V27RX_eq_train` and
 * `V27DEC_TRAIN_SHORT`'s own derivation are named from.  What `modem + 0x14`
 * MEANS to the caller is still not established and this does not guess.
 * Finding F9301.
 */
#define V27SH_TRAIN_LONG	0x0a	/* short */

/*
 * The PROTOCOL state's countdown, in blocks, by rate and by training length.
 *
 * `RxNextStateV27`'s EPOCH_DET arm and nothing else.  The short pair is
 * computed as `(rate != 1) + 1` and the long pair is selected by `rate == 1`,
 * which is why the two are spelled as two different idioms below.
 */
/*
 * The EPOCH_DET state's countdown, which is a literal 2 whatever the rate and
 * whatever the training length -- `movw $0x2,0x12(%edx)` at 0x0a2e87, the only
 * seed in the function that is not selected by anything.
 */
#define V27SH_EPOCH_DET_BLOCKS		2

#define V27SH_PROTOCOL_SHORT_4800	1
#define V27SH_PROTOCOL_SHORT_2400	2
#define V27SH_PROTOCOL_LONG_4800	0x21
#define V27SH_PROTOCOL_LONG_2400	0x2d

/*
 * The state numbers.  0..4 are the author's own words; see `V27SH_RX_STATE`.
 *
 * 5 IS NOT.  `RxHdxPrtcolV27` and `RxHdxEpochDetV27` store it beside the store
 * that installs `RxHdxErrorV27` (0x0a3146/0x0a314a and 0x0a3206/0x0a320d), and
 * `RxNextStateV27` has no arm for it -- so a machine that reaches it and is
 * then advanced takes the default arm.  The name is the handler's and nothing
 * more.
 */
#define V27RX_STATE_START	0
#define V27RX_STATE_EPOCH_DET	1
#define V27RX_STATE_PROTOCOL	2
#define V27RX_STATE_DATA	3
#define V27RX_STATE_IDLE	4
#define V27RX_STATE_ERROR	5

/*
 * A guard compared against zero as a 16-bit value and not written by anything
 * reconstructed.  Named for what it GATES, which is all that is established:
 * it selects which half of `DataCarrierDetectV27` runs.
 */
#define V27SH_V21_WATCH		0x14	/* unsigned short */

/*
 * The second tone detector and the buffer `DataCarrierDetectV27` copies into
 * before gain-controlling it.  The detector is the one whose success prints
 * "V27: V21 Carrier detected", so it is the V.21 (fax control channel) one;
 * `V27SH_MTD` above is the other and `DemodDataV27` runs it on the raw input.
 */
#define V27SH_MTD_V21		0x18	/* struct fpm_mtd * */
#define V27SH_BUF		0x1c	/* short *          */

/*
 * How many samples the V.21 detector has seen without a hit, and the latch
 * that starts it.  0x4ff samples at 8 kHz is 160 ms.
 */
#define V27SH_V21_SAMPLES	0x20	/* unsigned short */
#define V27SH_V21_ARMED		0x22	/* short          */
#define V27SH_V21_TIMEOUT	0x4ff

/* The gain control the V.21 detector runs behind, `struct fpm_agc`. */
#define V27SH_AGC		0x24

/*
 * The state handler `V27RX_modem` drives.
 *
 * `count` is IN/OUT: the handler is given the samples still to consume and
 * leaves behind how many are still to consume after it, and its RETURN is how
 * many output samples it produced.  `V27RX_modem` uses the difference for the
 * input pointer and the return for the output pointer, which is what types
 * both.
 */
typedef short (*v27_rx_state_fn)(void *modem, short *in, short *out,
				 unsigned short *count);

/* ------------------------------------------------------------------ */
/* What `V27RX_create` puts in the five configurations                  */
/*
 * EVERY ONE OF THESE IS A LITERAL IN THE OBJECT and none of them is read
 * anywhere else, so the names below say which FIELD of which module's config
 * the literal lands in -- which is measured, because `V27RX_create` copies the
 * library's own `*_CFG` onto the stack and then patches named offsets of it.
 * What the VALUES mean is the module's business and its header's; this one
 * only records where V.27ter puts them.
 *
 * The rate-dependent ones are named `_2400` / `_4800` after `V27SH_RATE`.
 */

/* The V.21 control-channel detector, `V27SH_MTD_V21`. */
#define V27_MTD_V21_TONES	2
#define V27_MTD_V21_RATIO	0x4ccd
#define V27_MTD_V21_MIN_LEVEL	300

/* The data-channel detector, `V27SH_MTD`. */
#define V27_MTD_TONES		2
#define V27_MTD_RATIO		0x199a
#define V27_MTD_MIN_LEVEL	100

/*
 * The gain control's measurement block at 4800 bit/s -- AND IT IS A NO-OP.
 *
 * `V27RX_create` stores it into the LIVE `fpm_agc_cfg::block_len` on the 4800
 * arm only, four instructions after `FPM_AGC_init` copied the configuration in,
 * and `AGCv27_CFG.block_len` is 40, which IS 0x28.  So the store puts back the
 * value that is already there and both rates run on a 40-sample block.
 * `t_v27fax.c` asserts that the variant doing it on BOTH arms separates
 * nothing, which is what makes the deadness measured rather than argued.
 * Finding F9307, deviation D1162.
 */
#define V27_AGC_BLOCK_4800	0x28

/* `struct fpm_sre_cfg`, the scalars V.27ter overrides. */
#define V27_SRE_GROUPS_ACQ	1
#define V27_SRE_GROUPS_TRK	8
#define V27_SRE_SETTLE		0x40
#define V27_SRE_MAG_HI		2
#define V27_SRE_MAG_LO		1
#define V27_SRE_ERR_HI		0x3333
#define V27_SRE_ERR_LO		0x199a
/*
 * `rms_min` is not a literal: it is `AGCv27_CFG.ref_level / 6`, read back out
 * of the gain control this function has just initialised (`movswl 0x68(%edx)`
 * at 0x999ee against the magic multiply 0x2aaaaaab, which is GCC's division by
 * six).  `rms_len` is `3 * V27RX_SAMP_PER_BAUD[rate]`, three symbols' worth.
 */
#define V27_SRE_RMS_MIN_DIV	6
#define V27_SRE_RMS_LEN_SYMS	3

/* `struct fpm_sre_cfg`, the timing meter's four caller-supplied fields. */
#define V27_SRE_PPM_STEP_2400	0x18
#define V27_SRE_PPM_STEP_4800	0x20
#define V27_SRE_PPM_UNIT	200	/* ppm_period = ppm_step * this    */
#define V27_SRE_PPM_MILLION	1000000	/* ppm_scale and ppm_n_max divide it */

/* `struct fpm_fse_cfg`, the scalars V.27ter overrides. */
#define V27_FSE_BLOCK_2400	0x90
#define V27_FSE_BLOCK_4800	0xa0
#define V27_FSE_TRAIN_SYM	0x3e8
#define V27_FSE_ERR_HI		0x2666
#define V27_FSE_ERR_LO		0x8f6

/* `V27RX_Q_LIMIT`, the only field of the quality smoother that depends on the
 * rate.  Set on both arms and on NEITHER for a rate that is not 0 or 1 --
 * which `V27SH_RATE`'s own two arms make unreachable. */
#define V27RX_Q_LIMIT_2400	0x1b9e
#define V27RX_Q_LIMIT_4800	0x0d27

/*
 * `struct sdmv27_cfg::nbits`, and it is the recommendation's own number: two
 * bits per symbol on the four-phase 2400 bit/s constellation and three on the
 * eight-phase 4800 one.  The object writes `3 - (rate == 0)`.
 */
#define V27_SDM_NBITS_2400	2
#define V27_SDM_NBITS_4800	3

/* ------------------------------------------------------------------ */
/* The functions                                                       */

/*
 * The signal-to-noise ratio, in whatever units the caller's family uses.
 *
 * `mov $0xa,%eax; ret` -- six bytes, no argument touched.  THE ARITY IS NOT
 * SETTLED BY THE OBJECT and this is the least claim compatible with the
 * family: `GetSNRV17` and `GetSNRV29` are the same accessor written out,
 * `mov 0x4(%esp),%ecx` then a subtraction from 13 and 14 respectively, and
 * both narrow the result with `cwtl`.  So the family takes the modem and
 * returns `short`; V.27ter's answer happens not to depend on either.
 */
short GetSNRV27(void *modem);

/*
 * Fill a receive status block -- or rather, report whether there was one.
 *
 * `xor %eax,%eax; cmpl $0x0,0x8(%esp); setne %al` and nothing else: the
 * SECOND argument decides the answer and the first is not read at all.  The
 * first is still declared, because `v27rx_status` loads `dp + 0x14` into it
 * before tail-jumping here exactly as `v27tx_status` does for
 * `V27TX_status`, which does read it.
 */
int V27RX_status(void *rx, void *status);

/*
 * `V27RX_control`'s own request, byte offsets only -- CLAUDE.md's "the
 * instance is not modelled" convention, because nothing establishes the
 * request's size or any field before +0x04.  Usage inference throughout:
 * every field's role is read off what `V27RX_control` DOES with it, not off
 * a name or a typed caller/callee.
 *
 *   +0x04  int     copied straight into the handle's own `int_0008`
 *                   (`struct v27rx_cfg`'s field of that name)
 *   +0x0c  byte    a MASK, tested bit by bit against the receive block's
 *                  enables
 *   +0x0d  byte    FLAGS: bit 0x10 forces `V27SH_INT_0004` (the field
 *                  `RxHdxDataV27`'s own comment calls "planted from
 *                  outside"); bit 0x02 re-runs `V27RX_create(rx, rx)` --
 *                  the handle's own first 28 bytes ARE its config, so this
 *                  reinitialises from whatever is already there
 */
#define V27RXCTL_INT_0004		0x04
#define V27RXCTL_MASK			0x0c
#define V27RXCTL_FLAGS			0x0d

#define V27RXCTL_MASK_DISABLE_00	(1 << 3)
#define V27RXCTL_MASK_DISABLE_FSE_LMS	(1 << 5)
#define V27RXCTL_FLAGS_FORCE_NOCARRIER	(1 << 4)
#define V27RXCTL_FLAGS_REINIT		(1 << 1)

/*
 * Plant `V27SH_INT_0004` and/or two of the receive block's enables from the
 * caller's request, and optionally re-run `V27RX_create` over the handle's
 * own current config.  Returns 0 if `req` is NULL, else 1.
 *
 * THE TWO GATES THE OBJECT ENCODES AS ONE NESTED BRANCH COLLAPSE TO TWO
 * INDEPENDENT TESTS: `V27SH_INT_0004 = (flags & FORCE_NOCARRIER) ? 1 : 0`
 * and `if (flags & REINIT) V27RX_create(...)` fire the identical
 * `V27RX_create` call on the SAME `flags & REINIT` bit whichever way the
 * object's `jne`/`else` split reads it, so the collapse changes no
 * behaviour for any input.  `int_0004` and the two enable-disables are
 * unconditional on the request's other fields.
 */
int V27RX_control(void *rx, void *req);

/*
 * `V27TX_control`'s own request -- byte offsets only, `V27RX_control`'s own
 * convention, and again entirely usage inference:
 *
 *   +0x04  int     copied straight into the handle's own `int_0008`
 *   +0x08  int     multiplied by `V27TX_PPS_SCALE[rate]` into the pulse
 *                  shaper's live `cfg.scale` -- the SAME table
 *                  `V27TX_create` seeds `scale` from at construction time,
 *                  here driven by the request instead of the handle's own
 *                  `int_000c`
 *   +0x0c  byte    a MASK, bit 0x04 tested against `V27TX_HANDLE_FLAGS`
 *   +0x0d  byte    FLAGS: bit 0x10 forces `V27TXP_INT_0008`; bit 0x02
 *                  re-runs `V27TX_create(modem, modem)` -- the transmit
 *                  handle's own first 32 bytes ARE its config, the same
 *                  self-reinit idiom `V27RX_control` uses
 *   +0x10  int     copied straight into the handle's own `int_0018`
 */
#define V27TXCTL_INT_0004		0x04
#define V27TXCTL_SCALE_MUL		0x08
#define V27TXCTL_MASK			0x0c
#define V27TXCTL_FLAGS			0x0d
#define V27TXCTL_INT_0010		0x10

#define V27TXCTL_MASK_HANDLE_FLAG_04	(1 << 2)
#define V27TXCTL_FLAGS_FORCE_INT_0008	(1 << 4)
#define V27TXCTL_FLAGS_REINIT		(1 << 1)

/*
 * Retune the pulse shaper's live gain, plant `int_0008`/`int_0018` from the
 * request, and optionally re-run `V27TX_create` over the handle's own
 * current config.  Returns 0 if `req` is NULL, else 1.  `V27RX_control`'s
 * own shape, transmit side.
 */
int V27TX_control(void *modem, void *req);

/*
 * Has the equaliser been told to adapt regardless of its own gate?
 *
 * Reads `fpm_fse::lms_force` and returns it as 0 or 1.  The NAME is the
 * author's and the FIELD is `fpm_fse.h`'s; what connects "epoch" to
 * `lms_force` is not established, so nothing here claims it does.
 */
int EpochDetectV27(void *modem);

/*
 * Is there a carrier: `fpm_agc::signal & fpm_sre::active`.
 *
 * Both are 32-bit and the AND is 32-bit, so the result is not reduced to 0/1
 * -- unlike `EpochDetectV27` immediately above it in the object, which is the
 * same shape and does reduce.  That difference is the object's.
 */
int CarrierDetectV27(void *modem);

/* ------------------------------------------------------------------ */
/* The status block `V27TX_status` fills                                */

/*
 * NOT the modem instance: this is the caller's own block, and it is ALREADY
 * MODELLED TWICE in this tree -- `struct v22_status` in
 * `include/dsplib/v22status.h` and `struct v32_status` in
 * `include/dsplib/v32fpstat.h`, both from datapumps that fill the same block
 * with the same fields at the same offsets.  Those are evidence class 2 for
 * the four names below and this header takes them.
 *
 * IT DOES NOT DEFINE A THIRD STRUCTURE, deliberately.  V.27ter writes +0x0c
 * and +0x18, both of which are outside what `struct v22_status` models, so a
 * `struct v27_status` would assert an extent nothing here can bound -- the
 * block is the caller's and no reconstructed function allocates it.  Named
 * constants say exactly what is known and no more.  A parallel V.21 pass has
 * reached the same block from the other side; whoever merges the two should
 * reconcile the spellings rather than let a fourth accumulate.  See finding
 * F8872.
 *
 * THE RATE FIELD IS RANK 1 AS WELL AS RANK 2.  `V21TX_status` does not copy
 * +0x02 from anywhere -- it stores the literal `movw $0x12c,0x2(%edx)` at
 * 0xa2c11, and 0x12c is 300, which is V.21's bit rate to the digit.  V.17,
 * V.27ter and V.29 fill the same slot from their own handle's +0x02 instead,
 * which is what a rate-selectable modem does with a field a fixed-rate one
 * can write as a constant.  `v22status.h` calls it `tx_bps` independently.
 *
 * AND V.27ter'S ZEROES ARE INFORMATIVE.  It zeroes `rx_bps` and `quality`
 * while filling `tx_bps`, which is what a HALF-DUPLEX TRANSMITTER's status
 * would carry: there is no receive rate to report and no equaliser to grade.
 * V.22's full-duplex `V22_status` fills all four.
 *
 * +0x10 IS BOUNDED AND NOT NAMED.  V.17, V.27ter and V.29 give it the same
 * value they gave +0x02 -- the object reads the source's +0x02 a SECOND time
 * rather than reusing the first read -- and V.21 gives it zero where it gave
 * +0x02 its 300.  So it is not simply a copy of the first, since the one
 * modem that knows its rate statically writes two different numbers into
 * them.  `v22status.h` leaves it unnamed too.
 */
#define V27STAT_PROTOCOL	0x00	/* copied from the handle's +0x00  */
#define V27STAT_TX_BPS		0x02	/* bit/s; V21TX_status writes 300  */
#define V27STAT_RX_BPS		0x04	/* zeroed: no receive side here    */
#define V27STAT_QUALITY		0x06	/* zeroed: no equaliser here       */
#define V27STAT_ZERO_08		0x08
#define V27STAT_ZERO_0A		0x0a
#define V27STAT_ZERO_0C		0x0c	/* outside struct v22_status       */
#define V27STAT_WORD_10		0x10	/* see the note above              */
#define V27STAT_ZERO_12		0x12
#define V27STAT_FLAGS		0x14	/* byte; the asymmetry lives here  */
#define V27STAT_FLAGS2		0x15	/* byte                            */
#define V27STAT_WORD_18		0x18	/* int; outside struct v22_status  */

/*
 * The two bits of `V27STAT_FLAGS` this function decides, and the one bit of
 * `V27STAT_FLAGS2` it clears.
 *
 * NAMED BY BIT VALUE AND NOT BY MEANING, because V.22's assignment of these
 * bits is V.22's: `v22status.h` reads bit 0 as its scrambler enable off five
 * datapump words this modem does not have.  What IS established here is that
 * bit 0 comes out set for V.27ter and clear for the other three, that bit 1
 * is cleared by all four, and that bit 2 is copied from the transmitter
 * handle's own +0x10.
 */
#define V27STAT_FLAGS_BIT0	0x01
#define V27STAT_FLAGS_BIT1	0x02
#define V27STAT_FLAGS_FROM_TX	0x04	/* the bit taken from tx + 0x10    */
#define V27STAT_FLAGS2_BIT0	0x01

/*
 * The byte of the transmitter's handle that supplies V27STAT_FLAGS_FROM_TX.
 */
#define V27TX_HANDLE_FLAGS	0x10

/*
 * Copy the transmitter's status into the caller's block, reporting 1, or 0 if
 * there is no block.
 *
 * THE ODD ONE OF FOUR, AND THE ASYMMETRY IS REAL.  `V17TX_status`,
 * `V29TX_status` and this function are otherwise the same code; where the
 * other two clear bits 0 and 1 of the destination's byte at +0x14
 * (`and $0xfc,%al`), this one SETS bit 0 and then clears bit 1
 * (`or $0x1,%al` ... `and $0xfd,%dl`).  Finding F8866 records it and the
 * deviation register carries it as D1033, because the store it makes is
 * dead unless the two blocks overlap.
 */
int V27TX_status(const void *tx, void *status);

/*
 * Run the receive state machine until it has consumed `*count` samples,
 * then report the status word.
 *
 * `count` is IN/OUT in a second sense: on the way in it is the number of
 * input samples, on the way out it is the number of OUTPUT samples produced,
 * accumulated as a `short`.  The loop is a do-while -- the handler is called
 * once even for a count of zero -- and `in` advances by what the handler
 * consumed while `out` advances by what it returned.
 */
int V27RX_modem(void *modem, short *in, short *out, unsigned short *count);

/*
 * Release the receiver.
 *
 * Eleven calls in one fixed order: the equaliser, the symbol recovery and the
 * resampler give their buffers back through their own `_free`; the two scratch
 * buffers and the receive block go to `sysdep_free`; then the shared block's
 * two tone detectors, its own scratch buffer and itself; then the instance.
 *
 * THE THREE `FPM_*_free` CALLS ARE PASSED A SECOND ARGUMENT, the constant 1,
 * and none of the three functions has one -- the same extra argument
 * `v22data.c` and `bwchdem.c` record at their `FPM_AGC_agc` sites.  It is
 * ignored, cdecl makes it harmless, and it is not reproduced here.
 */
void V27RX_delete(void *modem);

/*
 * Build the receiver: the handle, the shared block, the receive block, five
 * module configurations and the decoder's own state.  Returns the handle.
 *
 * BOTH ARGUMENTS MAY BE NULL AND THE TWO NULLS MEAN DIFFERENT THINGS.  A null
 * `modem` is allocated here (`V27RXH_SIZE`) with its two block pointers
 * cleared, which is what makes the three "allocate if absent" tests below
 * fire; a null `cfg` means `V27RX_CFG`, the library's own defaults.  Neither
 * `sysdep_malloc` result is checked, here or in `V27RX_delete`.
 *
 * `fresh` IS TWO DIFFERENT FLAGS, and which one each module gets is the
 * object's.  The AGC inside the SHARED block is initialised with "the shared
 * block was allocated by this call"; the resampler, the AGC inside the RECEIVE
 * block, the symbol recovery and the equaliser are all initialised with "the
 * HANDLE was allocated by this call".  So re-creating over a live handle whose
 * receive block was allocated elsewhere reuses those four modules' buffers,
 * and the two flags are not interchangeable.
 *
 * IT DOES NOT CLEAR THE RECEIVE BLOCK.  Everything the reconstructed functions
 * read is written here -- the three enables, the decoder's fourteen fields, the
 * quality smoother, the energy-drop detector -- but the block comes from
 * `sysdep_malloc` and the parts no module claims keep whatever was in the heap.
 */
void *V27RX_create(void *modem, const struct v27rx_cfg *cfg);

/*
 * The equaliser's slicer: `fpm_fse_decision` for V.27ter.
 *
 * Both arguments are in/out.  It is handed the measured angle, subtracts the
 * previous symbol's phase, folds the difference into one revolution, finds the
 * nearest of the four or eight legal phase steps, advances the DPSK reference
 * by that step, and writes the constellation point back: the angle becomes the
 * new reference phase and the magnitude becomes the constant V27DEC_MAG.  The
 * return is the bit pattern the step carries.
 */
unsigned short V27RX_decision(struct fpm_fse *state, short *angle, short *mag);

/*
 * Grade the data: 1 while carrier holds, 2 when it does not.
 *
 * Also drives the mse smoother described at V27RX_Q_ACC, which runs whether
 * or not carrier is up, and whose verdict is left in V27RX_Q_FLAG for someone
 * else to read.
 *
 * RETURNS `short`: the carrier term is `(short)(signal & active)` and the
 * function returns that when it is non-zero, so the value is not reduced to a
 * code.  2 is a code and is returned only for the zero case.
 */
short QualityDetectV27(void *modem);

/*
 * Is the far end still there?
 *
 * Three tests, and which of them run depends on V27SH_V21_WATCH and
 * V27RX_RMS_ON: the equaliser's mse against V27RX_MSE_NO_CARRIER, a V.21 tone
 * detector run over a gain-controlled copy of the block, and the energy-drop
 * detector at V27RX_RMS_REF.  Any of them may clear the answer; none of them
 * sets it.
 *
 * RETURNS `short`, and NOT a code: the carrier term is
 * `(short)(signal & active)` and three of the six paths out return it
 * unchanged, so the value is whatever those two 32-bit flags AND to in their
 * low half.  Only the paths that DENY carrier produce a constant.
 */
short DataCarrierDetectV27(void *modem, short *samples, unsigned short count);

/*
 * The equaliser's TRAINING slicer, and the only thing that installs the
 * running one.
 *
 * Same signature as `V27RX_decision` because it fills the same slot -- it is
 * an `fpm_fse_decision`, and the object proves it by storing `V27RX_decision`
 * into `state->cfg.decision` from inside it.  What it decides is much less:
 * the transmitted training symbol alternates between two constellation points
 * half a revolution apart, so all it has to do is notice when the measured
 * angle is more than a quarter of a revolution from the reference and advance
 * the reference by half the constellation.  It returns 0xffff on every path,
 * which is not a symbol; whoever reads the equaliser's output during training
 * is expected to discard it.
 *
 * IT ALSO DRIVES THE HANDOVER.  Every call clears `fpm_fse::mu_sel`, and the
 * call on which its own symbol counter reaches `V27DEC_TRAIN_SYMS_SHORT` or
 * `..._LONG` clears `fpm_fse::lms_force`, sets `mu_sel` to 1 -- the next LMS
 * step size in `cfg.mu[]` -- and replaces itself in `cfg.decision` with
 * `V27RX_decision`.  So the equaliser trains with one gain and runs with
 * another, and this function is what switches it.
 */
unsigned short V27RX_eq_train(struct fpm_fse *state, short *angle, short *mag);

/*
 * The equaliser's FIRST slicer, and the one `V27RX_create` installs.
 *
 * It is an `fpm_fse_decision` and the object proves it the same way it proves
 * `V27RX_eq_train` is one: `V27RX_create` stores this function's address into
 * the stack `struct fpm_fse_cfg`'s +0x30 at 0x99b6c, four instructions before
 * handing that cfg to `FPM_FSE_init`.  So the chain is epoch detect ->
 * `V27RX_eq_train` -> `V27RX_decision`, each stage installing the next.
 *
 * IT DECIDES NOTHING.  Neither `angle` nor `mag` is read or written and the
 * return is 0xffff on every path, exactly as `V27RX_eq_train`'s is; whatever
 * reads the equaliser's output before the epoch is found is expected to
 * discard it.  What it does instead is watch the equaliser's own output pair
 * `out_i[n_out]` / `out_q[n_out]` for a discontinuity: it keeps three
 * consecutive points (`V27DEC_EPOCH_I0`..`_Q2`), sums the squared distance
 * from the newest point to the one two symbols back with the squared distance
 * across the other pair, and compares that against `V27EPOCH_TRIGGER` times a
 * leaky average of the point's own energy.
 *
 * THE HANDOVER SETS `lms_force`, WHERE `V27RX_eq_train`'s CLEARS IT.  Together
 * the two bracket the training run: this one forces the equaliser to adapt and
 * installs the training slicer, and the training slicer withdraws the force
 * and installs the running one.
 */
unsigned short V27RX_epoch_det(struct fpm_fse *state, short *angle,
			       short *mag);

/*
 * The ERROR state: raise `V27_STATUS_FLAG_ERROR`, run the block through the
 * demodulator anyway so the filters keep their history, and consume it.
 *
 * Nothing here advances the state.  The flag is a one-shot -- `V27RX_modem`
 * clears it at the top of every block.  `RxHdxErrorV17` and `RxHdxErrorV29`
 * are the same eleven instructions over a different flag-byte offset.
 */
short RxHdxErrorV27(void *modem, short *in, short *out, unsigned short *count);

/*
 * The DATA state: demodulate while the carrier is up, descramble, and grade
 * what came out.  `RxHdxDataV17` and `RxHdxDataV29` are the same function over
 * three different offsets and three different sets of callees.
 *
 * The carrier flag is raised UNCONDITIONALLY on entry and lowered again on the
 * deny arm, which is not the same as assigning it.  Nothing advances the state
 * on either arm.
 *
 * THE GATE IS TWO TERMS AND THE SECOND IS `V27SH_INT_0004`, which nothing in
 * the object writes -- so the demodulating arm is reached only when something
 * outside has left that field zero.  The test plants it rather than reaching
 * it.
 */
short RxHdxDataV27(void *modem, short *in, short *out, unsigned short *count);

/*
 * Advance the receive machine one step, from whatever `V27SH_RX_STATE` says.
 *
 * Each arm installs the NEXT state's handler, writes the next state's number
 * and seeds `V27SH_COUNTDOWN` for it; the two flag bits the machine owns
 * (`V27_STATUS_FLAG_DATA` and `V27_STATUS_FLAG2_IDLE`) are written by every
 * arm, always the opposite way round.  The default arm installs NOTHING: it
 * reports `V27_STATUS_DEFAULT`, raises `V27_STATUS_FLAG_ERROR` and clears the
 * carrier, so the machine keeps whatever handler it had.
 *
 * ONLY THE EPOCH_DET ARM TOUCHES THE DSP.  It steps the AGC's two smoother
 * coefficient POINTERS on by one `short` each, and the PROTOCOL arm freezes
 * the AGC's gain outright.  Neither happens anywhere else in the object.
 *
 * IT RETURNS NOTHING.  All four callers ignore `%eax`, and the arms leave
 * different things in it (`RxHdxIdleV27`'s caller then loads a fresh zero), so
 * there is no return value to reproduce.
 */
void RxNextStateV27(void *modem);

/*
 * The START state, which is the one `V27RX_create` installs (0x0996f9).
 *
 * It lowers the carrier flag, reports `V27_STATUS_START`, demodulates the
 * block and advances the machine as soon as `CarrierDetectV27` answers.  The
 * demodulator's return is DISCARDED and this handler always reports zero
 * output samples, so nothing it produced reaches `V27RX_modem`'s caller.
 */
short RxHdxStartV27(void *modem, short *in, short *out, unsigned short *count);

/*
 * The EPOCH_DET state: wait for the equaliser to find its epoch.
 *
 * Carrier gone -> install `RxHdxErrorV27`, state `V27RX_STATE_ERROR`, report
 * `V27_STATUS_ERROR` and raise `V27_STATUS_FLAG_ERROR`.  Carrier up -> count
 * `V27SH_COUNTDOWN` down and advance either when it is exhausted OR when
 * `EpochDetectV27` answers, whichever comes first.  The demodulator's return
 * is discarded and the handler always reports zero output samples.
 */
short RxHdxEpochDetV27(void *modem, short *in, short *out,
		       unsigned short *count);

/*
 * The PROTOCOL state: the only handler besides `RxHdxDataV27` that
 * descrambles, and the only one that reports a non-zero sample count.
 *
 * It is `RxHdxEpochDetV27`'s carrier arm with two additions -- the descramble,
 * and `V27_STATUS_ENTER_DATA_*` written on the way out -- and one subtraction:
 * there is no `EpochDetectV27`, so only the countdown can advance it.  On the
 * arm that does advance, it returns what `DemodDataV27` produced; on every
 * other arm it returns zero.
 */
short RxHdxPrtcolV27(void *modem, short *in, short *out,
		     unsigned short *count);

/*
 * The IDLE state: demodulate, report `V27_STATUS_IDLE`, re-read the carrier,
 * and go back to DATA when the equaliser's error has come back down.
 *
 * THE CARRIER FLAG IS CLEARED AND THEN RE-RAISED rather than assigned, exactly
 * as `RxHdxIdleV21` does it, and the restart test READS THE FLAG BACK rather
 * than the call's result (`testb $0x20,0x1d(%esi)` at 0x0a3069).  The two are
 * not the same thing -- the flag is a byte of the instance and the call is a
 * fresh answer -- and the object is what says which one gates the restart.
 */
short RxHdxIdleV27(void *modem, short *in, short *out, unsigned short *count);

/*
 * One block through the receive chain: gain control, an optional tone test,
 * resample, symbol recovery, equalise and slice.
 *
 * THE STRUCTURAL DIFFERENCE FROM V.17 AND V.29 IS THE TONE TEST, and it is
 * the object's rather than an omission here.  Both of those copy the block
 * into a scratch buffer, notch a tone out of the copy with `FPM_TONE_kill`
 * and run the detector over that; V.27ter has NO copy loop, NO `FPM_TONE_kill`
 * and hands `FPM_MTD_detect` the CALLER's buffer directly -- which is also
 * the buffer `FPM_AGC_agc` has just rewritten in place.  Finding F9115.
 *
 * A detection abandons the call: it returns 0 without touching the resampler,
 * the recoverer or the equaliser, and the gain control's effect on the
 * caller's samples stands.
 *
 * `bits` IS `unsigned short *` BY THE CALLEE, not by the object: the argument
 * is passed straight through to `FPM_FSE_receive`, whose fourth parameter is
 * `unsigned short *out`.  Nothing in this function reads it.
 */
unsigned short DemodDataV27(void *modem, short *in, unsigned short *bits,
			    unsigned short count);

/*
 * The scrambler pair, one indirection each and a tail call.
 *
 * `count` IS SIGNED HERE and that is forced: both widen it with `movswl`
 * before handing it on, where the V.17 and V.29 wrappers around the generic
 * scrambler module use `movzwl`.  `sdmv27.h` records what the module makes of
 * a negative one, which is not an early exit.
 */
/*
 * Release the transmitter.
 *
 * Seven calls in one fixed order: the pulse-shaping filter's two histories
 * through `FPM_PPS_free`, the symbol ring's buffer, the transmitter block, the
 * data source's FIFO and its `sgd` through their own deletes, the data-source
 * block, and the instance.  The instance pointer is kept in a
 * register throughout and the two BLOCK pointers are re-read before every use,
 * which is what the object encodes.
 *
 * THE `FPM_PPS_free` CALL IS PASSED A SECOND ARGUMENT, the constant 1, which
 * it does not have -- `V27RX_delete`'s three `_free` calls again, and F8870's
 * pattern.  Not reproduced; cdecl makes it harmless.
 */
void V27TX_delete(void *modem);

/*
 * One block through the transmit chain: map `count` data words to symbols in
 * the ring, then shape the ring into samples.  Two calls and nothing else --
 * the ring is both the encoder's destination and the shaper's source, and the
 * return is the shaper's sample count zero-extended from sixteen bits.
 *
 * `count` is the SAME `count` for both, which is not obvious: `SMC_encoder`
 * consumes data WORDS and `FPM_PPS_filter` consumes SYMBOLS, and the object
 * passes the caller's number to each.  It is the ring's own cursors that keep
 * the two in step.
 */
unsigned short ModDataV27(void *modem, const unsigned short *bits,
			  short *samples, unsigned short count);

void ScrambleDataV27(void *modem, unsigned short *data, short count);
void DescrambleDataV27(void *modem, unsigned short *data, short count);

/* ------------------------------------------------------------------ */
/* The transmit half-duplex machine and its constructor                */

/*
 * Build the transmitter: the handle, the data-source block (a FIFO and an
 * `sgd`), and the private DSP block (the symbol ring, the scrambler, the
 * symbol coder and the pulse shaper).  Returns the handle.
 *
 * BOTH ARGUMENTS MAY BE NULL, exactly as `V27RX_create`'s: a null `modem` is
 * allocated here (44 bytes) with both block pointers cleared; a null
 * `params` means `V27TX_CFG`.
 *
 * THE SYMBOL RING'S LENGTH IS COMPUTED, `V27TX_FRMSIZE[rate] + 2`, and its
 * buffer -- `struct fpm_smc_ring::sym`, the only one of the ring's three
 * buffer fields this modem allocates; `i` and `q` are zeroed and never
 * `sysdep_malloc`'d, because `ModDataV27` runs the pulse shaper in MAPPED
 * mode.  `V27TX_delete` frees exactly this one buffer, matching.
 *
 * A NEW HANDLE'S `fresh` FLAG THREADS ALL THE WAY TO `FPM_PPS_init`, the same
 * shape as `V29TX_create`; a caller re-initialising an existing handle keeps
 * every sub-object's own memory.
 */
void *V27TX_create(void *modem, const struct v27tx_cfg *params);

/*
 * Run the half-duplex machine until `*count` samples have been produced (or
 * the FIFO cannot keep up), `V29TX_modem`'s own do/while shape one
 * modulation over: fill the FIFO from `in` (unless `V27TXP_INT_0008` is
 * non-zero, in which case `*count` is taken as already queued), then call
 * the installed handler in a loop seeded with `V27TX_FRMSIZE[rate]` budget,
 * accumulating what each call returns into `*count`'s own out-value and
 * advancing `out`.  `in` is NOT advanced across calls -- passed unchanged to
 * every one, exactly as the `TxHdx*V27` family's own scratch-buffer use of
 * it expects.
 */
int V27TX_modem(void *modem, unsigned short *in, short *out,
		unsigned short *count);

/*
 * Advance the transmit machine one step, from whatever `V27TXP_STATE` says.
 *
 * `ja default` bounds an 11-entry jump table (`V27TX_STATE_*`), and it is ONE
 * OF THE PROJECT'S STORED-FUNCTION-POINTER CYCLES (F8492/F8493): every arm
 * but SCR1's own transition installs the next state's `TxHdx*V27` handler by
 * address and sets `V27TXP_STATE`; SCR1's OWN arm additionally calls
 * `SetScramblerV27`.  No proper subset of `TxNextStateV27` and the seven
 * `TxHdx*V27` states links, so they are written together.
 *
 * EVERY ARM CLEARS `V27TX_RESULT_B1_BIT0` ON ITS WAY OUT except SCR1's, which
 * SETS it and returns immediately rather than falling into the shared tail --
 * the same asymmetry `TxNextStateV29`'s own SCR1 arm has, one state earlier
 * in that machine's own numbering.
 */
void TxNextStateV27(void *modem);

/* Wait for a carrier: nothing but the transition.  21 bytes. */
short TxHdxStartV27(void *modem, unsigned short *in, short *out,
		    short *budget);

/*
 * Fill the budget with `TxNoCarrierV27` while `V27TXP_COUNTDOWN` runs down,
 * then transition.  `TxHdxAltV27`'s and `TxHdxQuietV27`'s shapes are
 * identical but for what they call once the budget is taken -- silence here,
 * the ALT pattern there.
 */
short TxHdxQuietV27(void *modem, unsigned short *in, short *out,
		    short *budget);

/*
 * Generate the alternating pattern with `SGD_symbol_gen` and modulate it,
 * budget-limited the same way `TxHdxQuietV27` is.
 */
short TxHdxAltV27(void *modem, unsigned short *in, short *out,
		  short *budget);

/*
 * Equaliser conditioning.  Fills `in` with the literal 7, scrambles it, then
 * walks the scrambled buffer choosing `V27TX_PATTERN_ALT` or
 * `V27TX_PATTERN_CARR` per element from bit 2 of the FOLLOWING scrambled
 * element -- a one-ahead read that touches `in[taken]` on its last iteration,
 * one element past what was filled.  Reproduced as a full-word test
 * (`in[i+1] & 0x04`) rather than the object's byte test; behaviourally
 * identical for every value 0x04 can appear in, since x86 is little-endian
 * and the low byte carries that bit either way.
 */
short TxHdxEQCondV27(void *modem, unsigned short *in, short *out,
		     short *budget);

/*
 * The scrambled-1s training pattern: `SGD_symbol_gen`, `ScrambleDataV27`,
 * `ModDataV27`.  `TxNextStateV27`'s SCR1 arm seeds `V27TXP_COUNTDOWN` to 1 and
 * does NOT overwrite it with a table lookup the way every other arm does, so
 * this handler's very first call is what carries the machine into DATA.
 */
short TxHdxSCR1V27(void *modem, unsigned short *in, short *out,
		   short *budget);

/*
 * The DATA state: drain the FIFO through `FIFO_read`, scramble, modulate.
 *
 * ONE-TIME ENTRY STATUS.  `V27TXP_COUNTDOWN` is nonzero exactly once, on the
 * call that follows the SCR1->DATA transition (SCR1's arm seeds it to 1 and
 * never clears it), and this function reads `V27TXP_RATE` at that one call to
 * report `V27TX_STATUS_ENTER_DATA_2400`/`_4800` before clearing the field.
 *
 * THE UNDERRUN ARM `FIFO_read` GATES CANNOT BE REACHED FROM `V27TX_modem`'s
 * OWN LOOP: `FIFO_read` never returns more than it is asked for, and this
 * function asks for exactly `*budget`, so the "got > *budget" branch the
 * object encodes is dead in every path this modem can drive itself -- the
 * SAME shape `t_v29txcreate.c` documents for `TxHdxDataV29`'s own
 * `V29TXP_INT_0008` arm.  Reached (if at all) by calling this handler
 * directly with `V27TXP_INT_0008` poked non-zero, `t_v29txcreate.c`'s own
 * idiom.
 */
short TxHdxDataV27(void *modem, unsigned short *in, short *out,
		   short *budget);

/*
 * Spend the FIFO-EMPTY block on `TxNoCarrierV27`, or transition if the FIFO
 * has data waiting.  Sets `V27TX_STATUS_IDLE` unconditionally on entry, even
 * on the transition arm, which the object does not undo.
 */
short TxHdxIdleV27(void *modem, unsigned short *in, short *out,
		   short *budget);

/*
 * Fill `count` symbol-ring slots with `V27TX_NOCARR_SYMBOL[rate]` -- wrapping
 * `widx` against `struct fpm_smc_ring::len` by hand, one slot at a time,
 * rather than through `FPM_SMC_encoder` -- then run the pulse shaper over
 * `count` samples.  `count` is a VALUE here, not `*budget`: every caller
 * passes what it already read out of `*budget`, so this takes the plain
 * `unsigned short` rather than the `v27tx_process_fn` pointer shape.
 */
short TxNoCarrierV27(void *modem, unsigned short *in, short *out,
		     unsigned short count);

/*
 * Reseed the scrambler for a rate change: build an `sdmv27_cfg` from
 * `V27TX_SDM_NUM_BITS[rate]`, save `struct sdmv27::reg` across `SDMv27_init`
 * and put it back by hand afterward -- `sdmv27.h`'s own account of this
 * function, written before this file reconstructed it.  `TxNextStateV27`'s
 * EQCOND arm is the only caller.
 */
void SetScramblerV27(void *modem);

/*
 * The same equaliser-conditioning sequence `TxHdxEQCondV27` builds inline --
 * fill with the literal 7, scramble, then choose `V27TX_PATTERN_ALT[rate]`
 * or `V27TX_PATTERN_CARR[rate]` per element from bit 2 of the FOLLOWING
 * scrambled element -- as a free-standing generator over a caller-supplied
 * buffer and count rather than `*budget`.  No reconstructed caller reaches
 * it; `docs/remaining.md`'s reverse-edge probe already found it has none in
 * the object either (F8320).
 */
void GenEQTrnSequenceV27(void *modem, unsigned short *buf,
			 unsigned short count);

#endif /* DSPLIB_V27FAX_H */

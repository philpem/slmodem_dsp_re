#include "dsplib/period_byte_layout.h"
/**
 * @file v27fax.h
 * @brief ITU-T V.27ter (fax): the receiver's primitives, the two
 *        transmit-chain drivers, and both lifecycles.
 *
 * Sixteen functions sit directly on the FPM layer here.  Most of them reach
 * into the V.27ter modem instance, pick a sub-object out of it, and either
 * hand that sub-object to the module that owns it or read one field back
 * out; two are equaliser slicers, two drive a whole chain end to end, one is
 * a constant, and two are one-line wrappers around the V.27ter scrambler
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
 *   V27RX_eq_train        the equaliser's training slicer, and the handover
 *   QualityDetectV27      smooth the equaliser's MSE and grade it
 *   DataCarrierDetectV27  carrier up/down, and the V.21 escape
 *   DemodDataV27          AGC -> resample -> symbol recovery -> equalise
 *   ScrambleDataV27       the transmitter's scrambler, by one indirection
 *   DescrambleDataV27     the receiver's descrambler, by one indirection
 *
 * `tools/service.py` puts all of them on the FAX side.
 *
 * And the half-duplex receive machine itself, six symbols and one cycle:
 *
 *   RxNextStateV27        the transition table -- six states, five arms
 *   RxHdxStartV27         wait for a carrier
 *   RxHdxEpochDetV27      wait for the equaliser's epoch
 *   RxHdxPrtcolV27        train, descramble, hand over to DATA
 *   RxHdxIdleV27          carrier lost; wait for the error to come back down
 *   RxHdxErrorV27         demodulate and complain
 *
 * `RxNextStateV27` stores the addresses of four of them and each of the four
 * calls it back, so no proper subset of the five links -- see the note at
 * the top of `src/fax/v27.c`.
 *
 * "The instance" is two objects, not one: the RECEIVE handle and the
 * TRANSMIT handle are different allocations with different layouts.
 * `V27RX_create` allocates 0x58 bytes and stores the shared block at +0x50,
 * the receive block at +0x54; `V27TX_delete` frees `*(h + 0x28) + 0x5c`
 * through `FPM_PPS_free` and `*(h + 0x24)`'s FIFO and `sgd`.  They cannot be
 * the same handle -- on the receive one, +0x28 and +0x24 fall inside the
 * equaliser, and both deletes free the handle itself, which on one object
 * would be a double free.  So `V27_OBJ_TX`, `V27_OBJ_TXDATA` and the
 * `V27TX_*` offsets below belong to the transmit handle, and
 * `V27_OBJ_SHARED`, `V27_OBJ_RX`, `V27_OBJ_STATUS` and the `V27RXH_*`
 * offsets to the receive one.  Finding F9304.
 *
 * The receive handle's first 28 bytes are a `struct v27rx_cfg`, copied
 * wholesale from the caller's config (or `V27RX_CFG` when none is given);
 * `faxcfg.h` had already modelled that structure from the other end, in
 * `init_vmi_v27rx`.  The accessors below stay `void *` plus named offsets
 * rather than becoming a `struct`, because every reconstructed V.27ter
 * function already spells them that way and a rewrite would move code
 * generation everywhere at once for no evidence.
 *
 * The receiver block is not opaque all the way down: four of the library's
 * own modules -- `struct fpm_mrf`, `struct fpm_agc`, `struct fpm_sre` and
 * `struct fpm_fse` -- are embedded in it at fixed offsets, each named by a
 * callee (`V27RX_delete`'s three frees and `DemodDataV27`'s `FPM_AGC_agc`
 * call) and confirmed by tiling the block with no gap and no overlap.
 * Every offset the five reading functions touch between +0x4c and +0x4f3c
 * therefore lands on a named field of one of those four structures.
 * Finding F8862.
 *
 * The decoder block (`rx + V27RX_DEC`) is confirmed the same way twice
 * over: `V27RX_decision` works relative to `state->cfg.owner`, and
 * `V27RX_create` both sets `cfg.owner` to that address and fills every one
 * of the decoder's own fields at the matching rx-relative offset, so each
 * one is confirmed by the function that reads it and by the one that
 * writes it.  Finding F8863.
 */

#ifndef DSPLIB_V27FAX_H
#define DSPLIB_V27FAX_H

#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/sdmv27.h"
#include "dsplib/sgd.h"

struct fpm_mtd;

/*
 * The transmit handle's first 32 bytes, `V27TX_create` copies wholesale from
 * the caller's `params` or, when NULL, from `V27TX_CFG` -- eight dwords, one
 * more than `struct v29tx_cfg`'s seven at the identical role: V.27ter's
 * `int_001c` is its own field, where V.29's `int_0018` fills the same role
 * (`FPM_PPS_CFG`'s `aux`) one field earlier.
 *
 * `protocol`/`bitrate` are rank 2: `V27TX_status` reads the transmit
 * handle's own +0x00/+0x02 back as `V27STAT_PROTOCOL`/`V27STAT_TX_BPS`, and
 * `V27TX_create`'s only branch on the config tests +0x02 against 2400 and
 * 4800.  `flags`/`short_0012` split the fifth dword because
 * `V27TX_status` reads the handle's own +0x10 back as a short
 * (`V27TX_HANDLE_FLAGS`); nothing splits the others, so they stay one `int`
 * each.  Usage inference for the rest, flagged per CLAUDE.md.
 *
 * `scale_mul` (this wave, was `int_000c`) IS RANK 2, not usage inference:
 * `V27TX_create` reads it back directly into the pulse shaper's own
 * configuration, `pcfg.scale = V27TX_PPS_SCALE[rate] * modem->int_000c`
 * (`src/fax/v27.c`), and `struct v27tx_ctl::scale_mul` -- already named,
 * below -- is this SAME multiplication driven by the runtime request
 * instead of the handle's own field; that field's own comment already drew
 * the parallel ("here driven by the request instead of the handle's own
 * int_000c") before this struct's own field had caught up to it, the
 * evidence-stranded-in-one-file shape F10139/F10140/F10169 keep finding.
 *
 * `flags` (this wave, was `flags_0010`) is the same "genuine flags word"
 * case CLAUDE.md's four-state ledger allows a plain name for even without
 * every bit resolved: `V27TX_create` sets its bit 2 unconditionally
 * (`*FIELD(modem, V27TX_HANDLE_FLAGS) |= 0x04;`) and `V27TX_status` reads
 * it back as `V27STAT_FLAGS_FROM_TX`, so the WORD's role (a flags short
 * `V27TX_create` seeds and `V27TX_status` reports) is established even
 * though no other bit is.  `struct v21_status`'s own `flags`/`flags1` and
 * `struct v27rx_ctl`/`v27tx_ctl`'s own `flags` are the same move.
 *
 * `fifo_size_factor` (this wave, was `int_0014`) is TWIN-CLASS EVIDENCE
 * (F10177's technique) from `v17fax.h`'s own `struct v17tx_cfg::
 * fifo_size_factor`, already real-named there (finding F10144): both are
 * "the transmit FIFO's element count is this field times a per-rate/fixed
 * factor", read back by the respective `V??TX_create` to size the FIFO, and
 * V.17's own field comment already anticipates the match ("V.29's own copy
 * is left as a candidate for whoever visits it" -- this wave visits V.27ter's
 * instead, on the identical role, and V.29's own copy below).
 */
struct v27tx_cfg {
	short	protocol;	/* +0x00                                     */
	short	bitrate;	/* +0x02  2400 or 4800                       */
	int	int_0004;	/* +0x04                                     */
	int	int_0008;	/* +0x08  60000, `v27rx_cfg`'s own value      */
	int	scale_mul;	/* +0x0c  1; multiplies into the PPS gain,
					same role as `v27tx_ctl::scale_mul`  */
	short	flags;		/* +0x10  `V27TX_HANDLE_FLAGS`                */
	short	short_0012;	/* +0x12                                     */
	int	fifo_size_factor; /* +0x14  the transmit FIFO's element count
					  is this * `V27TX_FRMSIZE[rate]`;
					  `v17tx_cfg`'s own field, same name */
	int	int_0018;	/* +0x18  `== 0` seeds `V27TXP_TRAIN_LONG`,
					the same role `V27RX_create` derives
					`V27SH_TRAIN_LONG` from out of
					`faxcfg.h`'s own `v27rx_cfg::int_0014`
					(out of this wave's scope) -- checked
					and left neutral, since even that
					established sibling stays unnamed   */
	int	int_001c;	/* +0x1c  `FPM_PPS_CFG`'s `aux`, across the
					`(void *)(long)` idiom D1250 names   */
};

union v27_status_word {
	int word;
	struct {
		unsigned char status;
		unsigned char flags;
		unsigned char flags2;
		unsigned char byte3;
	} byte;
};

/* Observed caller-owned status prefix; this does not bound the allocation. */
struct v27_status_prefix {
	unsigned short protocol, tx_bps, rx_bps, quality;
	unsigned short zero_08, zero_0a, zero_0c, short_0e;
	unsigned short word_10, zero_12;
	unsigned char flags, flags2;
	short short_16;
	int word_18;
};

struct v27_rx_decoder {
	unsigned short epoch_i0;
	unsigned short epoch_q0;
	unsigned short epoch_i1;
	unsigned short epoch_q1;
	unsigned short epoch_i2;
	unsigned short epoch_q2;
	int eight_phase;
	int train_short;
	unsigned short phase_mask;
	short last;
	const short *pmap;
	unsigned short train_count;
	short epoch_avg;
	const short *angles;
	short angle_prev;
	unsigned short sym_count;
};

struct v27_rx_block {
	int int_0000;
	int en_sre_adapt;
	int en_fse_pll;
	int int_000c;
	int en_fse_lms;
	struct v27_rx_decoder dec;
	struct sdmv27 sdm;
	unsigned char pad_004a[2];
	struct fpm_mrf mrf;
	struct fpm_agc agc;
	struct fpm_sre sre;
	struct fpm_fse fse;
	unsigned char pad_4f3c[4];
	short *buf_a;
	short *buf_b;
	short q_acc;
	unsigned short q_count;
	unsigned short q_limit;
	short q_flag;
	short rms_on;
	short rms_ref;
	unsigned short rms_count;
	unsigned char pad_4f56[2];
};

struct v27_rx_shared {
	struct fpm_mtd *mtd;
	int int_0004;
	short rate;
	short train_long;
	/**
	 * @brief Run the active receive state.
	 * @param modem Owning v27_rx, not this shared block.
	 * @param in Input samples.
	 * @param[out] out Decoded data words.
	 * @param[in,out] count Available samples on entry, unconsumed on return.
	 * @return Data words written. The dispatcher accumulates this separately.
	 */
	short (*handler)(void *modem, short *in, short *out,
			 unsigned short *count);
	short rx_state;
	unsigned short countdown;
	unsigned short v21_watch;
	unsigned char pad_0016[2];
	struct fpm_mtd *mtd_v21;
	short *buf;
	unsigned short v21_samples;
	short v21_armed;
	struct fpm_agc agc;
};

struct v27_rx {
	struct v27rx_cfg cfg;
	union v27_status_word result;
	short *eq_out_i;
	short *eq_out_q;
	unsigned short *eq_n_out;
	short *eq_icoeff;
	short *eq_qcoeff;
	unsigned short eq_taps;
	unsigned char pad_0036[2];
	int int_0038;
	int int_003c;
	short short_0040;
	unsigned char pad_0042[2];
	int int_0044;
	int int_0048;
	short short_004c;
	unsigned char pad_004e[2];
	struct v27_rx_shared *shared;
	struct v27_rx_block *rx;
};

struct v27_tx_source {
	struct fax_fifo *fifo;
	struct sgd *sgd;
	int int_0008;
	short rate;
	short train_long;
	/**
	 * @brief Run the active transmit state.
	 * @param modem Owning v27_tx, not this source block.
	 * @param in Input-word buffer.
	 * @param[out] out Shaped output samples.
	 * @param[in,out] budget Remaining symbols, not samples.
	 * @return Samples written; a state-only transition may return zero.
	 */
	short (*handler)(void *modem, unsigned short *in, short *out,
			 short *budget);
	short state;
	short countdown;
};

struct v27_tx_block {
	unsigned char pad_0000[8];
	struct fpm_smc_ring ring;
	struct sdmv27 sdm;
	unsigned char pad_002a[2];
	struct fpm_smc smc;
	struct fpm_pps pps;
};

struct v27_tx {
	struct v27tx_cfg cfg;
	union v27_status_word result;
	struct v27_tx_source *source;
	struct v27_tx_block *tx;
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
 * The transmitter block, named by what its contents are rather than by
 * position: `V27TX_delete` releases `*(modem + 0x28) + 0x5c` through
 * `FPM_PPS_free`, and `ModDataV27` hands `*(modem + 0x28) + 0x2c` to
 * `SMC_encoder` and `+ 0x5c` to `FPM_PPS_filter` -- a symbol-mapping encoder
 * and a pulse-shaping filter, the transmit chain and nothing else.
 * `ScrambleDataV27` takes its scrambler out of the same block; the
 * descrambler comes out of `V27_OBJ_RX`.
 */
#define V27_OBJ_TX		0x28	/* the transmitter block         */

/*
 * The block that holds the transmit data source: a byte FIFO and an `sgd`,
 * typed by its two members and by nothing else -- `V27TX_delete` hands +0x00
 * to `FIFO_delete` and +0x04 to `SGD_delete`.  That the block belongs to the
 * transmit side is inference from `V27TX_delete` being the only
 * reconstructed function that reaches it; `V27RX_delete` does not free it,
 * so the two deletes do not overlap.  Its extent is not established --
 * nothing reconstructed reads past +0x07.
 */
#define V27_OBJ_TXDATA		0x24
#define V27TXD_FIFO		0x00	/* struct fax_fifo * */
#define V27TXD_SGD		0x04	/* struct sgd *      */

/*
 * Its extent IS established, though: `V27TX_create` allocates it at 24
 * bytes and the half-duplex machine (`TxNextStateV27`, `SetScramblerV27` and
 * every `TxHdx*V27`) fills every byte of it.  Six more fields, all found
 * against those functions rather than assumed from V.29's layout at the
 * same offsets -- V.27ter's own field widths and roles differ, most visibly
 * the two-int underrun/rate pair where V.29 has one.
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
 * jump table, named from the object's own debug strings, each naming the
 * state being left (CLAUDE.md's evidence rank 1, the same technique
 * findings F9320/F9701 used for V.29's machine).  The function names and
 * these state names disagree in places, exactly as V29TX_STATE_ALT and
 * TxHdxABV29 do; not reconciled, see `TxNextStateV27`'s own comment in
 * `v27.c`.
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
 * The result word `V27TX_modem` returns, and the two flag bytes beside it --
 * `V27TX_create` zeroes all three with one store and the half-duplex machine
 * reads and writes them as bytes throughout, the same one-word-not-three-
 * fields shape `V27_OBJ_STATUS` has on the receive side.
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
 * One 32-bit word, not two fields: `V27RX_create` zeroes all four bytes at
 * +0x1c with a single store, then ORs a seed into the byte at +0x1d and
 * stores 2 into the byte at +0x1c -- so +0x1d is byte 1 of the word at
 * +0x1c.  `V27RX_modem` clears one bit of that byte and returns the whole
 * word.
 */
#define V27_OBJ_STATUS		0x1c	/* int, returned by V27RX_modem  */
#define V27_OBJ_STATUS_FLAGS	0x1d	/* byte 1 of the word above      */

/*
 * Byte 2 of the same word.  It was not modelled while `RxNextStateV27` was
 * unwritten because nothing else in the object touches it: every one of
 * its six sites is in the state machine.  `V27RX_create`'s single zeroing
 * store clears it along with the rest of the word, which is what puts it
 * inside the word rather than beside it -- the same argument
 * `V27_OBJ_STATUS_FLAGS` rests on.
 */
#define V27_OBJ_STATUS_FLAGS2	0x1e	/* byte 2 of the word above      */

/*
 * The four named bits of `V27_OBJ_STATUS_FLAGS`.  `V27_STATUS_FLAG_02`, an
 * earlier neutral name for one of them, is retired: enumerating every
 * read-modify-write of the byte across all 40 V.27ter symbols (23 sites)
 * settled what each bit is.  Findings F9230 and F9231.  The enumeration is
 * this modem's own; V.17's byte at `obj + 0x29` and V.21's at `rx + 0x19`
 * carry the same three bits in the same three roles, which corroborates
 * without being the derivation.
 *
 * ERROR (0x02) -- set by `RxHdxErrorV27`, by `RxNextStateV27`'s default arm,
 *   and by the two transitions that install `RxHdxErrorV27`.  Cleared by
 *   `V27RX_modem` alone, at the top of every block.  Seeded set by
 *   `V27RX_create`.  Nothing in the object reads it back; it leaves through
 *   the word `V27RX_modem` returns.
 *
 * CARRIER (0x20) -- cleared and then set again iff `CarrierDetectV27`
 *   answers non-zero, in `RxHdxIdleV27`; also cleared by `RxHdxStartV27` and
 *   set by `RxHdxPrtcolV27` and `RxHdxEpochDetV27`.  `RxHdxDataV27` sets it
 *   on entry and clears it when `DataCarrierDetectV27` says the carrier has
 *   gone.  Read by `RxHdxIdleV27` to gate its look at the equaliser's mse --
 *   both ends measured.
 *
 * LOW_SNR (0x80) -- cleared by `RxHdxDataV27` and set there iff `GetSNRV27`
 *   came back at or below `V27RX_SNR_THRESHOLD`.  Nothing reads it back, so
 *   unlike V.17's the read end is not measured; the name rests on the
 *   setter's own condition. The setter is in fact unreachable: `GetSNRV27`
 *   is a constant, so `GetSNRV27() <= 8` is false for every input the object
 *   can present and the bit is never raised in practice.  The comparison is
 *   reproduced because it is there; `t_v27fax.c` records the arm as not
 *   reached rather than pretending to cover it.  Finding F9233.
 *
 * DATA (0x01, owned entirely by the state machine) -- set by the two
 *   transitions that install `RxHdxDataV27`; cleared by every other arm of
 *   `RxNextStateV27` and by nothing outside it, so it is raised exactly
 *   while the machine is in `V27RX_STATE_DATA`.  Nothing reads it back
 *   either; like the other three it leaves through the word `V27RX_modem`
 *   returns.  `V21RX_FLAG_DATA` (`v21fax.h`) is bit 0 of the same byte of
 *   the same machine, and V.21's copy IS read -- a corroboration of the
 *   name, not its derivation.
 *
 * `V27RX_create` also sets bits 4 and 6, which nothing else touches.
 */
#define V27_STATUS_FLAG_DATA	(1 << 0)
#define V27_STATUS_FLAG_ERROR	(1 << 1)
#define V27_STATUS_FLAG_CARRIER	(1 << 5)
#define V27_STATUS_FLAG_LOW_SNR	(1 << 7)

/*
 * The one bit of `V27_OBJ_STATUS_FLAGS2` anything touches, and it is the
 * exact complement of `V27_STATUS_FLAG_DATA`: every arm of `RxNextStateV27`
 * writes both, and never the same way.  The DATA -> IDLE transition sets
 * this and clears that; all five other arms do the reverse.  So it is
 * raised exactly while the machine is in `V27RX_STATE_IDLE`.
 *
 * `V21RX_FLAG1_IDLE` is bit 0 of V.21's second flags byte with the same six
 * sites and the same pairing.  Nothing reads either.
 */
#define V27_STATUS_FLAG2_IDLE	(1 << 0)

/* See V27_STATUS_FLAG_LOW_SNR: a signed 16-bit compare, strictly greater. */
#define V27RX_SNR_THRESHOLD	8

/*
 * The status byte at `V27_OBJ_STATUS`, all eight values.  Nothing in the
 * object reads any of them -- the byte leaves through the word
 * `V27RX_modem` returns -- so a name can only be the site that writes it:
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
 * 6 and 7 are one site-pair and one selector: `RxHdxPrtcolV27`'s
 * countdown-expiry arm and `RxNextStateV27`'s IDLE arm both compute
 * `V27SH_RATE < 1 ? 6 : 7`, immediately before the transition that installs
 * `RxHdxDataV27` -- so the pair reports the rate the machine is about to
 * carry data at.  V.21's byte carries 0, 3, 4 and 5 in exactly these four
 * roles (`v21fax.h`), which corroborates those four and says nothing about
 * 1, 2, 6 or 7 -- the two modems do not have the same states.  Finding
 * F9302.
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
 * The receive handle, +0x20 .. +0x54, and every one of these is written by
 * `V27RX_create` and by nothing else in the object.
 *
 * Five of them are a view of the equaliser, taken once at the end of
 * construction and never refreshed: `FPM_FSE_init` has just allocated
 * those four buffers, so the pointers stay valid for the life of the
 * object, and `n_out` is handed over by address rather than by value
 * (`V27RX_FSE + offsetof(struct fpm_fse, n_out)`).  What reads them is
 * outside this object -- nothing reconstructed does -- so the name says
 * which field of `struct fpm_fse` each one is and no more than that.
 */
#define V27RXH_EQ_OUT_I		0x20	/* fse->out_i                      */
#define V27RXH_EQ_OUT_Q		0x24	/* fse->out_q                      */
#define V27RXH_EQ_N_OUT		0x28	/* &fse->n_out, the address        */
#define V27RXH_EQ_ICOEFF	0x2c	/* fse->icoeff                     */
#define V27RXH_EQ_QCOEFF	0x30	/* fse->qcoeff                     */
#define V27RXH_EQ_TAPS		0x34	/* short: fse->cfg.taps            */

/*
 * The six `V27RX_create` zeroes and nothing else in the object touches.
 * Their widths are the object's -- three ints and two shorts -- and their
 * meanings are not established, so they are named by offset.
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
 * The two bits `V27RX_create` raises here as its seed value.  Nothing else
 * in the object sets, clears or reads either, so this is the seed and not a
 * flag pair -- naming the bits would be inventing two meanings out of one
 * store.
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
 * `V27RX_create` sets all three to 1, together with a fourth at +0x0c set
 * to 0 which nothing in this file reads.
 */
#define V27RX_EN_SRE_ADAPT	0x04
#define V27RX_EN_FSE_PLL	0x08
#define V27RX_EN_FSE_LMS	0x10

/*
 * A fourth caller-owned enable, at +0x00 -- `V27RX_create` sets it to 1
 * alongside the three above, and `V27RX_control` is the only function that
 * clears it, gated on its own request's mask byte.  Neither function types
 * what it selects; usage inference only.
 */
#define V27RX_EN_00		0x00

/* The decoder block; see the header comment. */
#define V27RX_DEC		0x14

/*
 * The receiver's `struct sdmv27`, and the transmitter's -- typed by the
 * callee and by nothing else: `DescrambleDataV27` hands `rx + 0x3c` to
 * `SDMv27_descrambler` and `ScrambleDataV27` hands `tx + 0x1c` to
 * `SDMv27_scrambler`.  The receiver's copy tiles 0x3c..0x4a exactly, leaving
 * nothing over before `V27RX_MRF` at 0x4c.
 */
#define V27RX_SDM		0x3c	/* struct sdmv27, the descrambler  */
#define V27TX_SDM		0x1c	/* struct sdmv27, the scrambler    */

/*
 * The transmitter block tiles exactly, the same way the receiver's does,
 * and that is what closes it rather than a plausible-looking list of
 * offsets: three of the four regions are typed by a callee
 * (`SMC_encoder`/`FPM_PPS_filter`/`FPM_PPS_free` via `ModDataV27` and
 * `V27TX_delete`) and the fourth by `ScrambleDataV27`, and the four sizes
 * tile the block with one two-byte alignment gap and no overlap.  Finding
 * F9121.
 *
 * That arithmetic is what identifies `tx + 0x10`, and the first reading of
 * it was wrong: `V27TX_delete` frees `*(int *)(tx + 0x10)`, and the obvious
 * reading is a scratch allocation of the transmitter's own -- but 0x10 is
 * `fpm_smc_ring::sym`, the symbol-index buffer the encoder writes and the
 * shaper reads, with no room there for a field of its own.  So the delete
 * releases the ring's buffer.  Finding F9121.
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
 * that clears carrier prints "sudden energy drop > 8[dB], no carrier", and
 * the comparison it prints on is `FPM_rms(block) < (ref * 0x32fe) >> 15` --
 * 0x32fe / 32768 is 0.3984, and 20*log10(0.3984) is -8.0 dB, so the constant
 * and the string agree to the digit the string quotes.  `V27RX_create` sets
 * `enable` and `ref` together.
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
 * The third `fpm_fse::mse` threshold, and the only one compared with `<=`.
 *
 * `RxHdxIdleV27` restarts the machine when the carrier is up and the error
 * has come back down to this, and the author names the branch: the object's
 * own string "Decision error is small back to DATA mode !!!\n" is printed
 * on it and nowhere else.  It is exactly half `V27RX_MSE_NO_CARRIER`;
 * nothing says the two are related beyond that and this does not claim they
 * are.
 */
#define V27RX_MSE_IDLE_OK	0x1fff

/*
 * The one value `QualityDetectV27` returns that is a code rather than the
 * carrier term itself, and the one value `RxHdxDataV27` tests it against.
 * Two functions, one number, and the second is what makes it a shared
 * constant rather than a literal in the first.  `V17_QUALITY_UNRELIABLE` in
 * `v17fax.h` is the same code in the same place.
 */
#define V27_QUALITY_UNRELIABLE	2

/*
 * How many decisions must have been taken before the mse test is allowed to
 * drop carrier, compared against the decoder's own symbol counter.
 *
 * Neutral on purpose: what is established is that `DataCarrierDetectV27`
 * gates on `dec->sym_count > 0x5db` and that `V27RX_decision` is the only
 * thing that advances that counter.  Whether 1500 symbols is a training
 * length, a timeout or something else is not read off anything.
 */
#define V27RX_DEC_SETTLED	0x5db

/*
 * The largest symbol count `DemodDataV27` will accept from the symbol
 * recoverer without complaining, and the author's own words for what a
 * larger one is: the object's own string "ERROR: SRE buffer violation(%d)".
 *
 * That the buffer named is `V27RX_BUF_B` is inference and not measured: it
 * is the destination `FPM_SRE_recover` was given, and nothing reconstructed
 * allocates it.  V.29's demodulator carries the identical check against the
 * identical string, which is why the constant is stated per modem rather
 * than shared: nothing establishes that the two buffers are the same size,
 * only that the two messages are the same message.
 */
#define V27RX_SRE_MAX		0xa4

/* ------------------------------------------------------------------ */
/* The decoder block, at rx + V27RX_DEC (and at fse->cfg.owner)          */

/*
 * The epoch detector's six-short history, dec + 0x00 .. dec + 0x0b.
 * `V27RX_epoch_det` is the only thing in the object that touches any of the
 * six, and it uses them as three consecutive constellation points: the
 * newest pair at +0x00/+0x02, one symbol back at +0x04/+0x06 and two
 * symbols back at +0x08/+0x0a.  Every call shifts the pairs along by one
 * and drops the oldest.
 *
 * That the pairs are (I, Q) is rank 2 and not a guess: the values written
 * into +0x00 and +0x02 are `state->out_i[n]` and `state->out_q[n]`,
 * `fpm_fse.h`'s own fields.  Which pair is "one back" and which is "two
 * back" follows from the shift the function performs and from nothing
 * else.
 *
 * All six are loaded in a way that makes the `unsigned short` here free
 * either way (findings F614/F7803); the two samples differenced against
 * them are a different matter, since they are squared and so their
 * extension is forced and measured.  Finding F9234.
 *
 * The block is 12 bytes and `V27DEC_EIGHT_PHASE` is at 0x0c, so the six
 * tile the whole gap below it with nothing over.
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
 * `V27RX_decision` computes `n = dec->eight_phase ? 8 : 4`, GCC's branchless
 * form of a two-constant conditional.  `V27RX_create` fills it with
 * `params->f8 == 1`, and the same `params->f8` indexes `V27RX_DEC_PMAP` and
 * `V27RX_DEC_LAST_PHASE`, whose entry 1 points at eight-entry tables and
 * whose entry 0 points at four-entry ones.  So the field is the rate and
 * the count follows it.
 */
#define V27DEC_EIGHT_PHASE	0x0c	/* int    */

/*
 * Which of two equaliser training lengths `V27RX_eq_train` waits out.
 * Non-zero selects the short one: the object's branchless two-constant
 * conditional gives 0x32 when the field is non-zero and 0x3e8 when it is
 * zero.
 *
 * `V27RX_create` fills it from `(caller's field == 0)`, stored as a full
 * 32-bit word, which is what makes it an `int` rather than a flag byte.
 * What `params->f0a` means is not established and this header does not
 * guess: what is established is that one value of it costs fifty symbols
 * of training and the other a thousand.  The word "training" is the
 * author's -- the function reading this field is called `V27RX_eq_train`
 * -- and not ours.
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
 * Symbols the current slicer has taken, a separate counter from
 * `V27DEC_SYM_COUNT` below: this one is zeroed by `V27RX_create`, while
 * `V27DEC_SYM_COUNT` is advanced by every slicer and saturates.
 *
 * It is shared between two slicers and one of them resets it, which the
 * name "train count" alone does not say.  `V27RX_epoch_det` -- the slicer
 * `V27RX_create` installs first -- increments it on every call and compares
 * it against `V27EPOCH_SYMS_SHORT`/`_LONG`; on the call where it hands over
 * to `V27RX_eq_train` it stores 0xffff and then falls into the same
 * unconditional increment, so the counter comes out at zero and the
 * training slicer starts from a clean count.  `V27RX_eq_train` then
 * increments it in turn and compares it against
 * `V27DEC_TRAIN_SYMS_SHORT`/`_LONG`.  So the object has a three-stage
 * slicer chain -- epoch detect, train, run -- and this one counter times
 * the first two.  Finding F9234.
 *
 * Read signed and written unsigned, by both functions; finding F614 is why
 * that is not a contradiction (the increment's extension is dead, the
 * compare's is not).  Both spellings are reproduced.
 */
#define V27DEC_TRAIN_COUNT	0x1c	/* unsigned short */

/*
 * The epoch detector's leaky energy average, and the only field of the
 * decoder block that `V27RX_create` seeds with something other than zero
 * or a table entry.  The coincidence with `V27DEC_MAG`'s value below is
 * exactly that, a coincidence -- see the note there.
 *
 * `V27RX_epoch_det` updates it as `(31 * avg) >> 5 + (energy >> 5)`, an
 * arithmetic shift and not a division: the object emits the shift/subtract/
 * shift sequence with no rounding correction anywhere.
 */
#define V27DEC_EPOCH_AVG	0x1e	/* short */

/*
 * How many symbols `V27RX_epoch_det` waits before it will judge, chosen by
 * the same `V27DEC_TRAIN_SHORT` that chooses `V27RX_eq_train`'s two lengths
 * and by the same branchless idiom: 0x0a when the field is non-zero and
 * 0x28 when it is zero.  Non-zero selects the short one, as it does there.
 * The comparison is strictly `count > limit`.
 */
#define V27EPOCH_SYMS_SHORT	10
#define V27EPOCH_SYMS_LONG	40

/*
 * The two constants of the leaky average, and the trigger.
 *
 * The average is `(31 * avg) / 32 + energy / 32` written as shifts, so
 * `V27EPOCH_AVG_SHIFT` is both the divisor's log2 and the multiplier's
 * complement.  The epoch is declared when the summed squared difference
 * strictly exceeds `V27EPOCH_TRIGGER` times the freshly updated average.
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
 * It is not `V27DEC_LAST` spelled twice: `V27DEC_LAST` is the phase index
 * and this is the angle that index selects; the training slicer writes
 * both on the same pass, from the same table load, to two different
 * offsets.  `V27RX_decision` does not read or write it -- it subtracts
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
 * The half-duplex receive machine and the tone detectors live together
 * here.  `V27RX_delete` releases +0x00, +0x18 and +0x1c and then the block
 * itself, which fixes what it owns; `V27RX_modem` calls through +0x0c; and
 * `V27RX_create` stores `RxHdxStartV27` into +0x0c, which is what makes
 * +0x0c the current state handler rather than a vtable slot.
 */
#define V27SH_MTD		0x00	/* struct fpm_mtd *, deleted first */
#define V27SH_STATE		0x0c	/* v27_rx_state_fn                 */

/*
 * An int `RxHdxDataV27` requires to be zero before it will demodulate, and
 * the second half of its gate: the carrier must be up and this must be
 * clear.  Neutral: it has exactly one reader in the whole object and no
 * writer at all, so what sets it is outside what has been read.
 *
 * It is not `V27RX_EN_SRE_ADAPT`, which is a different block:
 * `DemodDataV27` reads +0x04 of the receiver block (`V27_OBJ_RX`) for that
 * one, and this is +0x04 of the shared block (`V27_OBJ_SHARED`).  The two
 * offsets are equal and the two fields are not.  Finding F9232.
 */
#define V27SH_INT_0004		0x04	/* int */

/*
 * The receive state number.  `V27SH_SKIP_TONE` is retired: an earlier pass
 * named this field for the one thing that read it -- `DemodDataV27` skips
 * its tone test while it is zero -- but the rename belongs with the pass
 * that wrote `RxNextStateV27`, and that pass is done.
 *
 * It is the author's own word, rank 1: `RxNextStateV27` switches on this
 * field, a five-way jump plus a default arm, and each of the five arms
 * begins by printing the state's name from the object's own strings --
 * `V27RX_STATE_START`, `_EPOCH_DET`, `_PROTOCOL`, `_DATA`, `_IDLE` for
 * indices 0..4, and `V27RX_DEFAULT, %d` for the default arm -- so the five
 * names and their five numbers come off the object's own text.
 *
 * Each arm also stores the next state's number and the next state's
 * handler together, and the two agree 1:1 across all five: 1 goes with
 * `RxHdxEpochDetV27`, 2 with `RxHdxPrtcolV27`, 3 with `RxHdxDataV27` and 4
 * with `RxHdxIdleV27`; `V27RX_create` stores 0 with `RxHdxStartV27`.
 * Finding F9300.  Read signed, so a value above 4 or below 0 takes the
 * default arm.
 */
#define V27SH_RX_STATE		0x10	/* short: V27RX_STATE_*            */

/*
 * Blocks left in the current state.  `RxHdxPrtcolV27` and
 * `RxHdxEpochDetV27` are the only two handlers that count it: each
 * decrements it once per block on its carrier-present arm and hands over
 * to `RxNextStateV27` when the result is at or below zero.  Every arm of
 * `RxNextStateV27` seeds it for the state it is entering, and
 * `V27RX_create` zeroes it beside the state number.
 *
 * Written and compared in sixteen bits, and the two readings are not the
 * same: the count is unsigned in memory and the exhaustion test is signed.
 * Both spellings are reproduced.  `struct v21_rx_hdx::countdown` is the
 * same field of the same machine, read the same two ways.
 */
#define V27SH_COUNTDOWN		0x12	/* unsigned short */

/*
 * The bit rate, as an index, and the two numbers are the author's:
 * `V27RX_create` compares the caller's bit-rate field against two literals
 * before it writes this field:
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
 * `V27RX_create` reads it signed at nine sites and unsigned at none, which
 * is why it is a `short` here; the state machine's own two reads are
 * unsigned against a 16-bit compare, where the extension is dead (finding
 * F614).
 */
#define V27SH_RATE		0x08	/* short: V27SH_RATE_*             */
#define V27SH_RATE_2400		0
#define V27SH_RATE_4800		1

/*
 * Long training rather than short, and it is one flag with two readers.
 * `V27RX_create` sets it from the caller's config, raised when that word is
 * zero.  The two readers:
 *
 *   - `V27RX_create` itself, where `V27DEC_TRAIN_SHORT` is set to `this
 *     field == 0`.  So this field non-zero means the equaliser takes
 *     `V27DEC_TRAIN_SYMS_LONG` (1000 symbols) rather than `..._SHORT` (50),
 *     and `V27RX_epoch_det` waits `V27EPOCH_SYMS_LONG` rather than `_SHORT`.
 *   - `RxNextStateV27`'s EPOCH_DET arm, where it chooses the PROTOCOL
 *     state's countdown: 1 or 2 blocks when it is clear, 33 or 45 when it
 *     is set.
 *
 * Both readers make the same choice between a short timing set and a long
 * one, and the word "training" is the author's -- it is what
 * `V27RX_eq_train` and `V27DEC_TRAIN_SHORT`'s own derivation are named
 * from.  What the caller's field means to it is still not established and
 * this does not guess.  Finding F9301.
 */
#define V27SH_TRAIN_LONG	0x0a	/* short */

/*
 * The PROTOCOL state's countdown, in blocks, by rate and by training
 * length -- `RxNextStateV27`'s EPOCH_DET arm and nothing else.  The short
 * pair is computed as `(rate != 1) + 1` and the long pair is selected by
 * `rate == 1`, which is why the two are spelled as two different idioms
 * below.
 */
/* The EPOCH_DET state's countdown: a literal 2 whatever the rate and
 * whatever the training length, the only seed in the function that is not
 * selected by anything. */
#define V27SH_EPOCH_DET_BLOCKS		2

#define V27SH_PROTOCOL_SHORT_4800	1
#define V27SH_PROTOCOL_SHORT_2400	2
#define V27SH_PROTOCOL_LONG_4800	0x21
#define V27SH_PROTOCOL_LONG_2400	0x2d

/*
 * The state numbers.  0..4 are the author's own words; see
 * `V27SH_RX_STATE`.  5 is not: `RxHdxPrtcolV27` and `RxHdxEpochDetV27`
 * store it beside the store that installs `RxHdxErrorV27`, and
 * `RxNextStateV27` has no arm for it -- so a machine that reaches it and is
 * then advanced takes the default arm.  The name is the handler's and
 * nothing more.
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
 * Every one of these is a literal in the object and none of them is read
 * anywhere else, so the names below say which field of which module's
 * config the literal lands in -- measured, because `V27RX_create` copies
 * the library's own `*_CFG` onto the stack and then patches named offsets
 * of it.  What the values mean is the module's business and its header's;
 * this one only records where V.27ter puts them.
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
 * The gain control's measurement block at 4800 bit/s -- and it is a no-op.
 * `V27RX_create` stores it into the live `fpm_agc_cfg::block_len` on the
 * 4800 arm only, but `AGCv27_CFG.block_len` is already 40 (0x28), so the
 * store puts back the value that is already there and both rates run on a
 * 40-sample block.  `t_v27fax.c` asserts that the variant doing it on both
 * arms separates nothing, which is what makes the deadness measured rather
 * than argued.  Finding F9307, deviation D1162.
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
 * `rms_min` is not a literal: it is `AGCv27_CFG.ref_level / 6`, read back
 * out of the gain control this function has just initialised.  `rms_len`
 * is `3 * V27RX_SAMP_PER_BAUD[rate]`, three symbols' worth.
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

/**
 * @brief The signal-to-noise ratio, in whatever units the caller's family
 *        uses.
 *
 * V.27ter's answer is a constant.  The arity is not settled by the object
 * and this is the least claim compatible with the family: `GetSNRV17` and
 * `GetSNRV29` are the same accessor written out, subtracting from 13 and
 * 14 respectively, so the family takes the modem and returns `short`;
 * V.27ter's answer happens not to depend on either.
 *
 * @param modem  The V.27ter modem object.
 * @return The SNR estimate.
 */
short GetSNRV27(void *modem);

/**
 * @brief Fill a receive status block -- or rather, report whether there
 *        was one.
 *
 * The second argument decides the answer and the first is not read at all.
 * The first is still declared, because `v27rx_status` loads it before
 * tail-jumping here, exactly as `v27tx_status` does for `V27TX_status`,
 * which does read it.
 *
 * @param rx      Unread.
 * @param status  Destination status block; NULL reports no block supplied.
 * @return 1 if `status` is non-NULL, else 0.
 */
int V27RX_status(void *rx, void *status);

/*
 * `V27RX_control`'s own request, byte offsets only -- CLAUDE.md's "the
 * instance is not modelled" convention, because nothing establishes the
 * request's size or any field before +0x04.  Usage inference throughout:
 * every field's role is read off what `V27RX_control` does with it, not off
 * a name or a typed caller/callee.
 *
 *   +0x04  int     copied straight into the handle's own `int_0008`
 *                   (`struct v27rx_cfg`'s field of that name)
 *   +0x0c  byte    a mask, tested bit by bit against the receive block's
 *                  enables
 *   +0x0d  byte    flags: bit 0x10 forces `V27SH_INT_0004` (the field
 *                  `RxHdxDataV27`'s own comment calls "planted from
 *                  outside"); bit 0x02 re-runs `V27RX_create(rx, rx)` --
 *                  the handle's own first 28 bytes are its config, so this
 *                  reinitialises from whatever is already there
 */
/*
 * `V27RX_CTL`'s own type, added for a different reader than `V27RX_control`
 * itself: `_init_receiver`'s reinit path (`class1rx.c`) builds its own
 * request from this `.data` object as a template, so it needs a type even
 * though `V27RX_control` correctly stays untyped above.  Same shape
 * `v17fax.h`'s `struct v17rx_ctl` uses for the identical reason: an
 * `unmapped_NNNN` name where nothing establishes the field, a real name
 * where the `V27RXCTL_*` macros above already do.  `unmapped_0000`'s own
 * upper 16 bits hold a per-modulation placeholder bit rate the caller
 * overwrites with the live one; `flags`'s `V27RXCTL_FLAGS_REINIT` bit is
 * OR'd in at runtime, not baked into the constant.  `unmapped_0010` is read
 * by neither `V27RX_control` nor `_init_receiver` -- the object still
 * reserves it, all zero, matching `v17rx_ctl`'s `int_0010` at the identical
 * offset.
 */
struct v27rx_ctl {
	unsigned char	unmapped_0000[4];
	int		int_0004;
	unsigned char	unmapped_0008[4];
	unsigned char	mask;
	unsigned char	flags;
	unsigned char	unmapped_000e[2];
	unsigned char	unmapped_0010[4];
};

#define V27RXCTL_INT_0004		0x04
#define V27RXCTL_MASK			0x0c
#define V27RXCTL_FLAGS			0x0d

#define V27RXCTL_MASK_DISABLE_00	(1 << 3)
#define V27RXCTL_MASK_DISABLE_FSE_LMS	(1 << 5)
#define V27RXCTL_FLAGS_FORCE_NOCARRIER	(1 << 4)
#define V27RXCTL_FLAGS_REINIT		(1 << 1)

/**
 * @brief Apply a runtime reconfiguration request to a V.27ter receiver.
 *
 * Plants `V27SH_INT_0004` and/or two of the receive block's enables from
 * the caller's request, and optionally re-runs `V27RX_create` over the
 * handle's own current config. The two gates the object encodes as one
 * nested branch collapse to two independent tests here, which changes no
 * behaviour for any input: `int_0004` and the two enable-disables are
 * unconditional on the request's other fields.
 *
 * @param rx   The receiver handle.
 * @param req  The request (`struct v27rx_ctl`); NULL is a no-op.
 * @return 0 if `req` is NULL, else 1.
 */
int V27RX_control(void *rx, void *req);

/*
 * `V27TX_control`'s own request -- byte offsets only, `V27RX_control`'s own
 * convention, and again entirely usage inference:
 *
 *   +0x04  int     copied straight into the handle's own `int_0008`
 *   +0x08  int     multiplied by `V27TX_PPS_SCALE[rate]` into the pulse
 *                  shaper's live `cfg.scale` -- the same table
 *                  `V27TX_create` seeds `scale` from at construction time,
 *                  here driven by the request instead of the handle's own
 *                  `int_000c`
 *   +0x0c  byte    a mask, bit 0x04 tested against `V27TX_HANDLE_FLAGS`
 *   +0x0d  byte    flags: bit 0x10 forces `V27TXP_INT_0008`; bit 0x02
 *                  re-runs `V27TX_create(modem, modem)` -- the transmit
 *                  handle's own first 32 bytes are its config, the same
 *                  self-reinit idiom `V27RX_control` uses
 *   +0x10  int     copied straight into the handle's own `int_0018`
 */
/*
 * `V27TX_CTL`'s own type, the transmit twin of `struct v27rx_ctl` above and
 * for the identical reason: `_init_transmitter`'s reinit path builds its
 * own request from this `.data` object as a template.  `unmapped_0000`'s
 * own low 16 bits hold the placeholder rate here (the transmit-side
 * templates put it low, the receive-side ones put it high); `flags`'s
 * `V27TXCTL_FLAGS_REINIT` bit is OR'd in at runtime, not baked into the
 * constant.  `int_0010` IS read by `V27TX_control` (unlike the receive
 * twin's own unmapped final dword), so it is a real field here, matching
 * the macro below.
 */
struct v27tx_ctl {
	unsigned char	unmapped_0000[4];
	int		int_0004;
	int		scale_mul;
	unsigned char	mask;
	unsigned char	flags;
	unsigned char	unmapped_000e[2];
	int		int_0010;
};

#define V27TXCTL_INT_0004		0x04
#define V27TXCTL_SCALE_MUL		0x08
#define V27TXCTL_MASK			0x0c
#define V27TXCTL_FLAGS			0x0d
#define V27TXCTL_INT_0010		0x10

#define V27TXCTL_MASK_HANDLE_FLAG_04	(1 << 2)
#define V27TXCTL_FLAGS_FORCE_INT_0008	(1 << 4)
#define V27TXCTL_FLAGS_REINIT		(1 << 1)

/**
 * @brief Apply a runtime reconfiguration request to a V.27ter transmitter.
 *
 * Retunes the pulse shaper's live gain, plants `int_0008`/`int_0018` from
 * the request, and optionally re-runs `V27TX_create` over the handle's own
 * current config.  `V27RX_control`'s own shape, transmit side.
 *
 * @param modem  The transmit handle.
 * @param req    The request (`struct v27tx_ctl`); NULL is a no-op.
 * @return 0 if `req` is NULL, else 1.
 */
int V27TX_control(void *modem, void *req);

/**
 * @brief Has the equaliser been told to adapt regardless of its own gate?
 *
 * Reads `fpm_fse::lms_force` and returns it as 0 or 1.  The name is the
 * author's and the field is `fpm_fse.h`'s; what connects "epoch" to
 * `lms_force` is not established, so nothing here claims it does.
 *
 * @param modem  The V.27ter modem object.
 * @return 0 or 1.
 */
int EpochDetectV27(void *modem);

/**
 * @brief Is there a carrier: `fpm_agc::signal & fpm_sre::active`.
 *
 * Both are 32-bit and the AND is 32-bit, so the result is not reduced to
 * 0/1 -- unlike `EpochDetectV27` immediately above it in the object, which
 * is the same shape and does reduce.  That difference is the object's.
 *
 * @param modem  The V.27ter modem object.
 * @return Non-zero while both the AGC's signal gate and the symbol
 *         recovery are active.
 */
int CarrierDetectV27(void *modem);

/* ------------------------------------------------------------------ */
/* The status block `V27TX_status` fills                                */

/*
 * Not the modem instance: this is the caller's own block, and it is
 * already modelled twice in this tree -- `struct v22_status`
 * (`v22status.h`) and `struct v32_status` (`v32fpstat.h`), both from
 * datapumps that fill the same block with the same fields at the same
 * offsets.  Those are evidence class 2 for the four names below.
 *
 * This does not define a third structure, deliberately: V.27ter writes
 * +0x0c and +0x18, both outside what `struct v22_status` models, so a
 * `struct v27_status` would assert an extent nothing here can bound -- the
 * block is the caller's and no reconstructed function allocates it.  A
 * parallel V.21 pass has reached the same block from the other side;
 * whoever merges the two should reconcile the spellings rather than let a
 * fourth accumulate.  Finding F8872.
 *
 * The rate field is rank 1 as well as rank 2: `V21TX_status` stores the
 * literal 300 rather than copying it from anywhere, and 300 is V.21's bit
 * rate to the digit.  V.17, V.27ter and V.29 fill the same slot from their
 * own handle instead, which is what a rate-selectable modem does with a
 * field a fixed-rate one can write as a constant.  `v22status.h` calls it
 * `tx_bps` independently.
 *
 * V.27ter's zeroes are informative: it zeroes `rx_bps` and `quality` while
 * filling `tx_bps`, which is what a half-duplex transmitter's status would
 * carry -- there is no receive rate to report and no equaliser to grade.
 * V.22's full-duplex `V22_status` fills all four.
 *
 * +0x10 is bounded and not named.  V.17, V.27ter and V.29 give it the same
 * value they gave +0x02, read a second time rather than reused; V.21 gives
 * it zero where it gave +0x02 its 300.  So it is not simply a copy of the
 * first, since the one modem that knows its rate statically writes two
 * different numbers into them.  `v22status.h` leaves it unnamed too.
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
 * Named by bit value and not by meaning, because V.22's assignment of
 * these bits is V.22's: `v22status.h` reads bit 0 as its scrambler enable
 * off five datapump words this modem does not have.  What is established
 * here is that bit 0 comes out set for V.27ter and clear for the other
 * three, that bit 1 is cleared by all four, and that bit 2 is copied from
 * the transmitter handle's own +0x10.
 */
#define V27STAT_FLAGS_BIT0	0x01
#define V27STAT_FLAGS_BIT1	0x02
#define V27STAT_FLAGS_FROM_TX	0x04	/* the bit taken from tx + 0x10    */
#define V27STAT_FLAGS2_BIT0	0x01

/*
 * The byte of the transmitter's handle that supplies V27STAT_FLAGS_FROM_TX.
 */
#define V27TX_HANDLE_FLAGS	0x10

/**
 * @brief Copy the transmitter's status into the caller's block.
 *
 * The odd one of four, and the asymmetry is real: `V17TX_status`,
 * `V29TX_status` and this function are otherwise the same code, but where
 * the other two clear bits 0 and 1 of the destination's flags byte, this
 * one sets bit 0 and then clears bit 1.  Finding F8866 records it and the
 * deviation register carries it as D1033, because the store it makes is
 * dead unless the two blocks overlap.
 *
 * @param tx      The transmit handle.
 * @param status  Destination status block; NULL reports no block supplied.
 * @return 1 if `status` is non-NULL, else 0.
 */
int V27TX_status(const void *tx, void *status);

/**
 * @brief Run the receive state machine until it has consumed `*count`
 *        samples, then report the status word.
 *
 * `count` is in/out in a second sense: on the way in it is the number of
 * input samples, on the way out it is the number of output samples
 * produced, accumulated as a `short`.  The loop is a do-while -- the
 * handler is called once even for a count of zero -- and `in` advances by
 * what the handler consumed while `out` advances by what it returned.
 *
 * @param modem  The V.27ter modem object.
 * @param in     Input samples.
 * @param out    Output samples.
 * @param count  In: samples available; out: samples produced.
 * @return The status word at `V27_OBJ_STATUS`.
 */
int V27RX_modem(void *modem, short *in, short *out, unsigned short *count);

/**
 * @brief Release the receiver.
 *
 * Eleven calls in one fixed order: the equaliser, the symbol recovery and
 * the resampler give their buffers back through their own `_free`; the two
 * scratch buffers and the receive block go to `sysdep_free`; then the
 * shared block's two tone detectors, its own scratch buffer and itself;
 * then the instance.
 *
 * @param modem  The V.27ter modem object.
 */
void V27RX_delete(void *modem);

/**
 * @brief Build the receiver: the handle, the shared block, the receive
 *        block, five module configurations and the decoder's own state.
 *
 * Both arguments may be NULL and the two NULLs mean different things: a
 * null `modem` is allocated here with its two block pointers cleared,
 * which is what makes the "allocate if absent" tests fire; a null `cfg`
 * means `V27RX_CFG`, the library's own defaults.  Neither `sysdep_malloc`
 * result is checked, here or in `V27RX_delete`.
 *
 * `fresh` is two different flags, and which one each module gets is the
 * object's: the AGC inside the shared block is initialised with "the
 * shared block was allocated by this call"; the resampler, the AGC inside
 * the receive block, the symbol recovery and the equaliser are all
 * initialised with "the handle was allocated by this call".  So
 * re-creating over a live handle whose receive block was allocated
 * elsewhere reuses those four modules' buffers.
 *
 * It does not clear the receive block: everything the reconstructed
 * functions read is written here, but the block comes from
 * `sysdep_malloc` and the parts no module claims keep whatever was in the
 * heap.
 *
 * @param modem  An existing handle to reinitialise, or NULL to allocate one.
 * @param cfg    Configuration to copy in, or NULL for `V27RX_CFG`.
 * @return The handle.
 */
void *V27RX_create(void *modem, const struct v27rx_cfg *cfg);

/**
 * @brief The equaliser's slicer: `fpm_fse_decision` for V.27ter.
 *
 * Subtracts the previous symbol's phase from the measured angle, folds the
 * difference into one revolution, finds the nearest of the four or eight
 * legal phase steps, advances the DPSK reference by that step, and writes
 * the constellation point back: the angle becomes the new reference phase
 * and the magnitude becomes the constant `V27DEC_MAG`.
 *
 * @param state  The equaliser state (`cfg.owner` reaches the decoder block).
 * @param angle  In: measured angle. Out: new reference phase.
 * @param mag    Out: `V27DEC_MAG`.
 * @return The bit pattern the step carries.
 */
unsigned short V27RX_decision(struct fpm_fse *state, short *angle, short *mag);

/**
 * @brief Grade the data: 1 while carrier holds, 2 when it does not.
 *
 * Also drives the mse smoother described at `V27RX_Q_ACC`, which runs
 * whether or not carrier is up, and whose verdict is left in
 * `V27RX_Q_FLAG` for someone else to read.  Returns `short`: the carrier
 * term is `(short)(signal & active)` and the function returns that when it
 * is non-zero, so the value is not reduced to a code -- 2 is a code and is
 * returned only for the zero case.
 *
 * @param modem  The V.27ter modem object.
 * @return The carrier term, or 2 when it is zero.
 */
short QualityDetectV27(void *modem);

/**
 * @brief Is the far end still there?
 *
 * Three tests, and which of them run depends on `V27SH_V21_WATCH` and
 * `V27RX_RMS_ON`: the equaliser's mse against `V27RX_MSE_NO_CARRIER`, a
 * V.21 tone detector run over a gain-controlled copy of the block, and the
 * energy-drop detector at `V27RX_RMS_REF`.  Any of them may clear the
 * answer; none of them sets it.  Returns `short`, and not a code: three of
 * the six paths out return the carrier term `(short)(signal & active)`
 * unchanged; only the paths that deny carrier produce a constant.
 *
 * @param modem    The V.27ter modem object.
 * @param samples  The block just demodulated.
 * @param count    Number of samples in the block.
 * @return Non-zero while carrier holds, a constant when it is denied.
 */
short DataCarrierDetectV27(void *modem, short *samples, unsigned short count);

/**
 * @brief The equaliser's training slicer, and the only thing that installs
 *        the running one.
 *
 * Same signature as `V27RX_decision` because it fills the same slot -- it
 * is an `fpm_fse_decision`, and the object proves it by storing
 * `V27RX_decision` into `state->cfg.decision` from inside it.  What it
 * decides is much less: the transmitted training symbol alternates between
 * two constellation points half a revolution apart, so all it has to do is
 * notice when the measured angle is more than a quarter of a revolution
 * from the reference and advance the reference by half the constellation.
 * It returns 0xffff on every path, which is not a symbol; whoever reads
 * the equaliser's output during training is expected to discard it.
 *
 * It also drives the handover: every call clears `fpm_fse::mu_sel`, and
 * the call on which its own symbol counter reaches
 * `V27DEC_TRAIN_SYMS_SHORT` or `..._LONG` clears `fpm_fse::lms_force`,
 * sets `mu_sel` to 1 -- the next LMS step size in `cfg.mu[]` -- and
 * replaces itself in `cfg.decision` with `V27RX_decision`.  So the
 * equaliser trains with one gain and runs with another, and this function
 * is what switches it.
 *
 * @param state  The equaliser state.
 * @param angle  In: measured angle.
 * @param mag    Unused.
 * @return 0xffff, always.
 */
unsigned short V27RX_eq_train(struct fpm_fse *state, short *angle, short *mag);

/**
 * @brief The equaliser's first slicer, and the one `V27RX_create` installs.
 *
 * It is an `fpm_fse_decision` and the object proves it the same way it
 * proves `V27RX_eq_train` is one: `V27RX_create` stores this function's
 * address into the stack `struct fpm_fse_cfg` a few instructions before
 * handing that cfg to `FPM_FSE_init`.  So the chain is epoch detect ->
 * `V27RX_eq_train` -> `V27RX_decision`, each stage installing the next.
 *
 * It decides nothing: neither `angle` nor `mag` is read or written and the
 * return is 0xffff on every path, exactly as `V27RX_eq_train`'s is.  What
 * it does instead is watch the equaliser's own output pair
 * `out_i[n_out]`/`out_q[n_out]` for a discontinuity: it keeps three
 * consecutive points (`V27DEC_EPOCH_I0`..`_Q2`), sums the squared distance
 * from the newest point to the one two symbols back with the squared
 * distance across the other pair, and compares that against
 * `V27EPOCH_TRIGGER` times a leaky average of the point's own energy.
 *
 * The handover sets `lms_force`, where `V27RX_eq_train`'s clears it:
 * together the two bracket the training run, this one forcing the
 * equaliser to adapt and installing the training slicer, and the training
 * slicer withdrawing the force and installing the running one.
 *
 * @param state  The equaliser state.
 * @param angle  Unused.
 * @param mag    Unused.
 * @return 0xffff, always.
 */
unsigned short V27RX_epoch_det(struct fpm_fse *state, short *angle,
			       short *mag);

/**
 * @brief The ERROR state: raise `V27_STATUS_FLAG_ERROR`, run the block
 *        through the demodulator anyway so the filters keep their history,
 *        and consume it.
 *
 * Nothing here advances the state.  The flag is a one-shot -- `V27RX_modem`
 * clears it at the top of every block.  `RxHdxErrorV17` and `RxHdxErrorV29`
 * are the same shape over a different flag-byte offset.
 *
 * @param modem  The V.27ter modem object.
 * @param in     Input samples.
 * @param out    Output samples (always none).
 * @param count  In/out sample count.
 * @return 0.
 */
short RxHdxErrorV27(void *modem, short *in, short *out, unsigned short *count);

/**
 * @brief The DATA state: demodulate while the carrier is up, descramble,
 *        and grade what came out.
 *
 * `RxHdxDataV17` and `RxHdxDataV29` are the same function over three
 * different offsets and three different sets of callees.  The carrier flag
 * is raised unconditionally on entry and lowered again on the deny arm,
 * which is not the same as assigning it.  Nothing advances the state on
 * either arm.
 *
 * The gate is two terms and the second is `V27SH_INT_0004`, which nothing
 * in the object writes -- so the demodulating arm is reached only when
 * something outside has left that field zero.  The test plants it rather
 * than reaching it.
 *
 * @param modem  The V.27ter modem object.
 * @param in     Input samples.
 * @param out    Output samples.
 * @param count  In/out sample count.
 * @return The number of decoded output samples, or 0 when carrier is
 *         denied, the gate is shut, or the quality grade is unreliable.
 */
short RxHdxDataV27(void *modem, short *in, short *out, unsigned short *count);

/**
 * @brief Advance the receive machine one step, from whatever
 *        `V27SH_RX_STATE` says.
 *
 * Each arm installs the next state's handler, writes the next state's
 * number and seeds `V27SH_COUNTDOWN` for it; the two flag bits the machine
 * owns (`V27_STATUS_FLAG_DATA` and `V27_STATUS_FLAG2_IDLE`) are written by
 * every arm, always the opposite way round.  The default arm installs
 * nothing: it reports `V27_STATUS_DEFAULT`, raises `V27_STATUS_FLAG_ERROR`
 * and clears the carrier, so the machine keeps whatever handler it had.
 *
 * Only the EPOCH_DET arm touches the DSP: it steps the AGC's two smoother
 * coefficient pointers on by one `short` each, and the PROTOCOL arm
 * freezes the AGC's gain outright.  Neither happens anywhere else in the
 * object.
 *
 * @param modem  The V.27ter modem object.
 */
void RxNextStateV27(void *modem);

/**
 * @brief The START state, which is the one `V27RX_create` installs.
 *
 * Lowers the carrier flag, reports `V27_STATUS_START`, demodulates the
 * block and advances the machine as soon as `CarrierDetectV27` answers.
 * The demodulator's return is discarded and this handler always reports
 * zero output samples, so nothing it produced reaches `V27RX_modem`'s
 * caller.
 *
 * @param modem  The V.27ter modem object.
 * @param in     Input samples.
 * @param out    Output samples (always none).
 * @param count  In/out sample count.
 * @return 0.
 */
short RxHdxStartV27(void *modem, short *in, short *out, unsigned short *count);

/**
 * @brief The EPOCH_DET state: wait for the equaliser to find its epoch.
 *
 * Carrier gone -> install `RxHdxErrorV27`, state `V27RX_STATE_ERROR`,
 * report `V27_STATUS_ERROR` and raise `V27_STATUS_FLAG_ERROR`.  Carrier up
 * -> count `V27SH_COUNTDOWN` down and advance either when it is exhausted
 * or when `EpochDetectV27` answers, whichever comes first.  The
 * demodulator's return is discarded and the handler always reports zero
 * output samples.
 *
 * @param modem  The V.27ter modem object.
 * @param in     Input samples.
 * @param out    Output samples (always none).
 * @param count  In/out sample count.
 * @return 0.
 */
short RxHdxEpochDetV27(void *modem, short *in, short *out,
		       unsigned short *count);

/**
 * @brief The PROTOCOL state: the only handler besides `RxHdxDataV27` that
 *        descrambles, and the only one that reports a non-zero sample
 *        count.
 *
 * It is `RxHdxEpochDetV27`'s carrier arm with two additions -- the
 * descramble, and `V27_STATUS_ENTER_DATA_*` written on the way out -- and
 * one subtraction: there is no `EpochDetectV27`, so only the countdown can
 * advance it.
 *
 * @param modem  The V.27ter modem object.
 * @param in     Input samples.
 * @param out    Output samples.
 * @param count  In/out sample count.
 * @return What `DemodDataV27` produced on the arm that advances, else 0.
 */
short RxHdxPrtcolV27(void *modem, short *in, short *out,
		     unsigned short *count);

/**
 * @brief The IDLE state: demodulate, report `V27_STATUS_IDLE`, re-read the
 *        carrier, and go back to DATA when the equaliser's error has come
 *        back down.
 *
 * The carrier flag is cleared and then re-raised rather than assigned,
 * exactly as `RxHdxIdleV21` does it, and the restart test reads the flag
 * back rather than the call's result.  The two are not the same thing --
 * the flag is a byte of the instance and the call is a fresh answer -- and
 * the object is what says which one gates the restart.
 *
 * @param modem  The V.27ter modem object.
 * @param in     Input samples.
 * @param out    Output samples (always none).
 * @param count  In/out sample count.
 * @return 0.
 */
short RxHdxIdleV27(void *modem, short *in, short *out, unsigned short *count);

/**
 * @brief One block through the receive chain: gain control, an optional
 *        tone test, resample, symbol recovery, equalise and slice.
 *
 * The structural difference from V.17 and V.29 is the tone test, and it is
 * the object's rather than an omission here.  Both of those copy the block
 * into a scratch buffer, notch a tone out of the copy, and run the
 * detector over that; V.27ter hands the tone detector the caller's buffer
 * directly -- which is also the buffer the AGC has just rewritten in
 * place.  Finding F9115.
 *
 * A detection abandons the call: it returns 0 without touching the
 * resampler, the recoverer or the equaliser, and the gain control's effect
 * on the caller's samples stands.
 *
 * @param modem  The V.27ter modem object.
 * @param in     Input samples.
 * @param bits   Destination for the sliced bits (`fpm_fse`'s own type).
 * @param count  Number of input samples.
 * @return The number of bits produced, or 0 on a tone detection.
 */
unsigned short DemodDataV27(void *modem, short *in, unsigned short *bits,
			    unsigned short count);

/*
 * The scrambler pair, one indirection each and a tail call.  `count` is
 * signed here and that is forced: both widen it before handing it on,
 * where the V.17 and V.29 wrappers around the generic scrambler module
 * widen unsigned.  `sdmv27.h` records what the module makes of a negative
 * one, which is not an early exit.
 */
/**
 * @brief Release the transmitter.
 *
 * Seven calls in one fixed order: the pulse-shaping filter's two
 * histories, the symbol ring's buffer, the transmitter block, the data
 * source's FIFO and its `sgd` through their own deletes, the data-source
 * block, and the instance.
 *
 * @param modem  The V.27ter modem object.
 */
void V27TX_delete(void *modem);

/**
 * @brief One block through the transmit chain: map `count` data words to
 *        symbols in the ring, then shape the ring into samples.
 *
 * Two calls and nothing else -- the ring is both the encoder's destination
 * and the shaper's source.  `count` is the same value for both, which is
 * not obvious: `SMC_encoder` consumes data words and `FPM_PPS_filter`
 * consumes symbols, and the object passes the caller's number to each. It
 * is the ring's own cursors that keep the two in step.
 *
 * @param modem    The V.27ter modem object.
 * @param bits     Data words to encode.
 * @param samples  Destination for the shaped samples.
 * @param count    Number of data words (and symbols).
 * @return The number of samples written.
 */
unsigned short ModDataV27(void *modem, const unsigned short *bits,
			  short *samples, unsigned short count);

/**
 * @brief Scramble `count` data words in place through the transmit
 *        scrambler, by one indirection.
 * @param modem  The V.27ter modem object.
 * @param data   Data words, scrambled in place.
 * @param count  Number of words.
 */
void ScrambleDataV27(void *modem, unsigned short *data, short count);
/**
 * @brief Descramble `count` data words in place through the receive
 *        descrambler, by one indirection.
 * @param modem  The V.27ter modem object.
 * @param data   Data words, descrambled in place.
 * @param count  Number of words.
 */
void DescrambleDataV27(void *modem, unsigned short *data, short count);

/* ------------------------------------------------------------------ */
/* The transmit half-duplex machine and its constructor                */

/**
 * @brief Build the transmitter: the handle, the data-source block (a FIFO
 *        and an `sgd`), and the private DSP block (the symbol ring, the
 *        scrambler, the symbol coder and the pulse shaper).
 *
 * Both arguments may be NULL, exactly as `V27RX_create`'s: a null `modem`
 * is allocated here (44 bytes) with both block pointers cleared; a null
 * `params` means `V27TX_CFG`.
 *
 * The symbol ring's length is computed, `V27TX_FRMSIZE[rate] + 2`, and its
 * buffer -- `struct fpm_smc_ring::sym`, the only one of the ring's three
 * buffer fields this modem allocates; `i` and `q` are zeroed and never
 * allocated, because `ModDataV27` runs the pulse shaper in mapped mode.
 * `V27TX_delete` frees exactly this one buffer, matching.
 *
 * A new handle's `fresh` flag threads all the way to `FPM_PPS_init`, the
 * same shape as `V29TX_create`; a caller re-initialising an existing
 * handle keeps every sub-object's own memory.
 *
 * @param modem   An existing handle to reinitialise, or NULL to allocate one.
 * @param params  Configuration to copy in, or NULL for `V27TX_CFG`.
 * @return The handle.
 */
void *V27TX_create(void *modem, const struct v27tx_cfg *params);

/**
 * @brief Run the half-duplex machine until `*count` samples have been
 *        produced (or the FIFO cannot keep up).
 *
 * `V29TX_modem`'s own do/while shape one modulation over: fills the FIFO
 * from `in` (unless `V27TXP_INT_0008` is non-zero, in which case `*count`
 * is taken as already queued), then calls the installed handler in a loop
 * seeded with `V27TX_FRMSIZE[rate]` budget, accumulating what each call
 * returns into `*count`'s own out-value and advancing `out`.  `in` is not
 * advanced across calls -- passed unchanged to every one, exactly as the
 * `TxHdx*V27` family's own scratch-buffer use of it expects.
 *
 * @param modem  The transmit handle.
 * @param in     Data words to transmit.
 * @param out    Destination for the shaped samples.
 * @param count  In: samples requested; out: samples produced.
 * @return The status word at `V27TX_OBJ_RESULT`.
 */
int V27TX_modem(void *modem, unsigned short *in, short *out,
		unsigned short *count);

/**
 * @brief Advance the transmit machine one step, from whatever
 *        `V27TXP_STATE` says.
 *
 * An 11-entry jump table, one of the project's stored-function-pointer
 * cycles (findings F8492/F8493): every arm but SCR1's own transition
 * installs the next state's `TxHdx*V27` handler by address and sets
 * `V27TXP_STATE`; SCR1's own arm additionally calls `SetScramblerV27`.  No
 * proper subset of `TxNextStateV27` and the seven `TxHdx*V27` states
 * links, so they are written together.
 *
 *   START     installs TxHdxQuietV27, budget FRMSIZE[rate]
 *   QUIET     seeds the carrier pattern, installs TxHdxAltV27,
 *             budget FRMSIZE[rate]*10
 *   CARR      installs TxHdxQuietV27, budget FRMSIZE[rate]
 *   NOCARR    seeds the ALT pattern, installs TxHdxAltV27,
 *             budget ALT_COUNT[train_long]
 *   ALT       installs TxHdxEQCondV27, budget EQCOND_COUNT[train_long]
 *   EQCOND    seeds the SCR1 pattern, installs TxHdxSCR1V27, budget 8,
 *             calls SetScramblerV27, returns (bypasses the shared tail)
 *   SCR1      installs TxHdxDataV27, budget 1, sets RESULT_B1_BIT0, returns
 *   DATA      seeds the SCR1 pattern, installs TxHdxSCR1V27,
 *             budget FRMSIZE[rate] -- reached only from TxHdxDataV27's own
 *             underrun-bypass arm, which nothing reconstructed drives
 *   TURNOFF   installs TxHdxQuietV27, budget FRMSIZE[rate]
 *   NOENG     installs TxHdxIdleV27, budget 0, sets RESULT_B2_BIT0
 *   IDLE      installs TxHdxStartV27, budget 0 -- wraps to START
 *
 * Every arm clears `V27TX_RESULT_B1_BIT0` on its way out except SCR1's,
 * which sets it and returns immediately rather than falling into the
 * shared tail -- the same asymmetry `TxNextStateV29`'s own SCR1 arm has,
 * one state earlier in that machine's own numbering.  `NOCARR`/`ALT` index
 * by `V27TXP_TRAIN_LONG`, not `V27TXP_RATE` -- the one place this machine
 * differs from `V27TX_FRMSIZE`'s own rate indexing.
 *
 * @param modem  The V.27ter modem object.
 */
void TxNextStateV27(void *modem);

/**
 * @brief Wait for a carrier: nothing but the transition.
 * @param modem   The V.27ter modem object.
 * @param in      Unused.
 * @param out     Unused.
 * @param budget  Unused.
 * @return 0.
 */
short TxHdxStartV27(void *modem, unsigned short *in, short *out,
		    short *budget);

/**
 * @brief Fill the budget with `TxNoCarrierV27` while `V27TXP_COUNTDOWN`
 *        runs down, then transition.
 *
 * `TxHdxAltV27`'s and `TxHdxQuietV27`'s shapes are identical but for what
 * they call once the budget is taken -- silence here, the ALT pattern
 * there.
 *
 * @param modem   The V.27ter modem object.
 * @param in      Unused.
 * @param out     Destination for the shaped samples.
 * @param budget  In/out remaining sample budget for this state.
 * @return The number of samples written.
 */
short TxHdxQuietV27(void *modem, unsigned short *in, short *out,
		    short *budget);

/**
 * @brief Generate the alternating pattern with `SGD_symbol_gen` and
 *        modulate it, budget-limited the same way `TxHdxQuietV27` is.
 * @param modem   The V.27ter modem object.
 * @param in      Unused.
 * @param out     Destination for the shaped samples.
 * @param budget  In/out remaining sample budget for this state.
 * @return The number of samples written.
 */
short TxHdxAltV27(void *modem, unsigned short *in, short *out,
		  short *budget);

/**
 * @brief Equaliser conditioning.
 *
 * Fills `in` with the literal 7, scrambles it, then walks the scrambled
 * buffer choosing `V27TX_PATTERN_ALT` or `V27TX_PATTERN_CARR` per element
 * from bit 2 of the following scrambled element -- a one-ahead read that
 * touches `in[taken]` on its last iteration, one element past what was
 * filled.
 *
 * @param modem   The V.27ter modem object.
 * @param in      Scratch buffer, filled and scrambled here.
 * @param out     Destination for the shaped samples.
 * @param budget  In/out remaining sample budget for this state.
 * @return The number of samples written.
 */
short TxHdxEQCondV27(void *modem, unsigned short *in, short *out,
		     short *budget);

/**
 * @brief The scrambled-1s training pattern: `SGD_symbol_gen`,
 *        `ScrambleDataV27`, `ModDataV27`.
 *
 * `TxNextStateV27`'s SCR1 arm seeds `V27TXP_COUNTDOWN` to 1 and does not
 * overwrite it with a table lookup the way every other arm does, so this
 * handler's very first call is what carries the machine into DATA.
 *
 * @param modem   The V.27ter modem object.
 * @param in      Scratch buffer for the generated pattern.
 * @param out     Destination for the shaped samples.
 * @param budget  In/out remaining sample budget for this state.
 * @return The number of samples written.
 */
short TxHdxSCR1V27(void *modem, unsigned short *in, short *out,
		   short *budget);

/**
 * @brief The DATA state: drain the FIFO through `FIFO_read`, scramble,
 *        modulate.
 *
 * One-time entry status: `V27TXP_COUNTDOWN` is nonzero exactly once, on
 * the call that follows the SCR1->DATA transition (SCR1's arm seeds it to
 * 1 and never clears it), and this function reads `V27TXP_RATE` at that
 * one call to report `V27TX_STATUS_ENTER_DATA_2400`/`_4800` before
 * clearing the field.
 *
 * The underrun arm `FIFO_read` gates cannot be reached from
 * `V27TX_modem`'s own loop: `FIFO_read` never returns more than it is
 * asked for, and this function asks for exactly `*budget`, so that branch
 * is dead in every path this modem can drive itself -- the same shape
 * `t_v29txcreate.c` documents for `TxHdxDataV29`'s own `V29TXP_INT_0008`
 * arm.
 *
 * @param modem   The V.27ter modem object.
 * @param in      Scratch buffer for the drained data.
 * @param out     Destination for the shaped samples.
 * @param budget  In/out remaining sample budget for this state.
 * @return The number of samples written.
 */
short TxHdxDataV27(void *modem, unsigned short *in, short *out,
		   short *budget);

/**
 * @brief Spend the FIFO-empty block on `TxNoCarrierV27`, or transition if
 *        the FIFO has data waiting.
 *
 * Sets `V27TX_STATUS_IDLE` unconditionally on entry, even on the
 * transition arm, which the object does not undo.
 *
 * @param modem   The V.27ter modem object.
 * @param in      Unused.
 * @param out     Destination for the shaped samples.
 * @param budget  In/out remaining sample budget for this state.
 * @return The number of samples written.
 */
short TxHdxIdleV27(void *modem, unsigned short *in, short *out,
		   short *budget);

/**
 * @brief Fill `count` symbol-ring slots with `V27TX_NOCARR_SYMBOL[rate]`,
 *        then run the pulse shaper over `count` samples.
 *
 * `count` is a value here, not `*budget`: every caller passes what it
 * already read out of `*budget`, so this takes the plain `unsigned short`
 * rather than the `v27tx_process_fn` pointer shape.
 *
 * @param modem  The V.27ter modem object.
 * @param in     Unused.
 * @param out    Destination for the shaped samples.
 * @param count  Number of symbol slots to fill.
 * @return The number of samples written.
 */
short TxNoCarrierV27(void *modem, unsigned short *in, short *out,
		     unsigned short count);

/**
 * @brief Reseed the scrambler for a rate change.
 *
 * Builds an `sdmv27_cfg` from `V27TX_SDM_NUM_BITS[rate]`, saves
 * `struct sdmv27::reg` across `SDMv27_init` and puts it back by hand
 * afterward.  `TxNextStateV27`'s EQCOND arm is the only caller.
 *
 * @param modem  The V.27ter modem object.
 */
void SetScramblerV27(void *modem);

/**
 * @brief Emit `count` symbols of the equaliser-conditioning pattern.
 *
 * The same sequence `TxHdxEQCondV27` builds inline -- fill with the
 * literal 7, scramble, then choose `V27TX_PATTERN_ALT[rate]` or
 * `V27TX_PATTERN_CARR[rate]` per element from bit 2 of the following
 * scrambled element -- as a free-standing generator over a caller-supplied
 * buffer and count rather than `*budget`. No reconstructed caller reaches
 * it, and finding F8320's reverse-edge probe found it has none in the
 * object either.
 *
 * @param modem  The V.27ter modem object.
 * @param buf    Destination for the pattern.
 * @param count  Number of symbols to emit.
 */
void GenEQTrnSequenceV27(void *modem, unsigned short *buf,
			 unsigned short count);

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V27_LAYOUT_ASSERT(name, expr) typedef char name[(expr) ? 1 : -1]
V27_LAYOUT_ASSERT(v27_rx_decoder_size, sizeof(struct v27_rx_decoder) == 0x28);
V27_LAYOUT_ASSERT(v27_rx_block_size, sizeof(struct v27_rx_block) == 0x4f58);
V27_LAYOUT_ASSERT(v27_rx_shared_size, sizeof(struct v27_rx_shared) == 0x50);
V27_LAYOUT_ASSERT(v27_rx_size, sizeof(struct v27_rx) == 0x58);
V27_LAYOUT_ASSERT(v27_tx_source_size, sizeof(struct v27_tx_source) == 0x18);
V27_LAYOUT_ASSERT(v27_tx_block_size, sizeof(struct v27_tx_block) == 0x94);
V27_LAYOUT_ASSERT(v27_tx_size, sizeof(struct v27_tx) == 0x2c);
V27_LAYOUT_ASSERT(v27_status_word_size, sizeof(union v27_status_word) == 4);
V27_LAYOUT_ASSERT(v27_rx_shared_handler_off,
	__builtin_offsetof(struct v27_rx_shared, handler) == 0x0c);
V27_LAYOUT_ASSERT(v27_rx_block_fse_off,
	__builtin_offsetof(struct v27_rx_block, fse) == 0x124);
V27_LAYOUT_ASSERT(v27_rx_block_buf_a_off,
	__builtin_offsetof(struct v27_rx_block, buf_a) == 0x4f40);
V27_LAYOUT_ASSERT(v27_rx_shared_off,
	__builtin_offsetof(struct v27_rx, shared) == 0x50);
V27_LAYOUT_ASSERT(v27_rx_block_off,
	__builtin_offsetof(struct v27_rx, rx) == 0x54);
V27_LAYOUT_ASSERT(v27_tx_source_handler_off,
	__builtin_offsetof(struct v27_tx_source, handler) == 0x10);
V27_LAYOUT_ASSERT(v27_tx_private_pps_off,
	__builtin_offsetof(struct v27_tx_block, pps) == 0x5c);
V27_LAYOUT_ASSERT(v27_tx_source_off,
	__builtin_offsetof(struct v27_tx, source) == 0x24);
V27_LAYOUT_ASSERT(v27_tx_private_off,
	__builtin_offsetof(struct v27_tx, tx) == 0x28);
#undef V27_LAYOUT_ASSERT
#endif

#endif /* DSPLIB_V27FAX_H */

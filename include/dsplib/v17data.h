/**
 * @file v17data.h
 * @brief ITU-T V.17 (fax): the transmitter's data-path leaves.
 *
 * Two functions that do nothing themselves except reach into the V.17
 * transmitter instance, pick a sub-object out of it, and hand that
 * sub-object to the module that owns it:
 *
 *   ModDataV17       -> one of three SMCv17 encoders, then FPM_PPS_filter
 *   TxNoCarrierV17   -> fills the symbol ring by hand, then FPM_PPS_filter
 *
 * `tools/service.py` puts both on the FAX side: nothing in data mode
 * reaches them.  They are here because they sit directly on `FPM_PPS_filter`
 * and became startable with it, not because V.17 is data mode.
 *
 * The instance is not modelled, following the ruling `v22data.h` sets out:
 * `V17TX_create` (1,043 bytes) is not reconstructed, so naming its fields now
 * would mean guessing.  The parameter is `void *` and the offsets below are
 * named constants, each confirmed twice over -- once by the function that
 * uses it and once by `V17TX_create`, which lays out the same sub-object at
 * the same offset: the ring at `fp + 0x08` is `struct fpm_smc_ring` field for
 * field (finding F3640).  `V17TX_create` also builds an `SDM` at `fp + 0x1c`;
 * neither function here touches it.
 *
 * The symbol ring carries constellation-point indices, not I/Q rails:
 * `V17TX_create` configures the shaper with `mapped = 1`, `imap =
 * SMCv17_IMAP4`, `qmap = SMCv17_QMAP4` (which `fpm_pps.h` says selects the
 * indexed form), and independently `TxNoCarrierV17` writes only the ring's
 * `sym` field and never `i`/`q` -- the two readings agree without either
 * being derived from the other (finding F3641).
 */

#ifndef DSPLIB_V17DATA_H
#define DSPLIB_V17DATA_H

#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"

/*
 * Offsets in the V.17 transmitter instance.
 */

/*
 * The transmitter's private block, 0x90 bytes from `sysdep_malloc`.  Every
 * V17FP_* below is relative to what this field points at.
 */
#define V17TX_OBJ_FP		0x28

/*
 * A control block the transmitter owns (not merely points at, as this
 * header used to say): `V17TX_delete` frees its `fax_fifo` and `sgd` and
 * then the block itself, and `V17TX_modem` reads an int from it to choose
 * between two arms and calls through a dispatch slot at +0x14 -- the
 * transmit-side analogue of `V17RX_OBJ_CTL`.  `V17TX_create` also reads a
 * rate index from its +0x10 to select the shaper's output gain out of
 * `V17TX_PPS_SCALE`, and `TxNoCarrierV17` reads +0x1e.  The name stays
 * neutral because it is shared with `v17fax.h`, which this header does not
 * own; see finding F9106 for the ownership correction and its offsets.
 */
#define V17TX_OBJ_PARAMS	0x24

/*
 * The symbol index `TxNoCarrierV17` fills the ring with, in the block above.
 *
 * Named from the function's own name, which is the author's.  What the object
 * establishes is narrower: this is the only field of that block either
 * function reads, and its value goes into every symbol slot the no-carrier
 * transmit path produces.  It is loaded `movzwl` but only `%ax` is used, so
 * the signedness is free and `unsigned short` here follows the ring's element
 * being an index rather than being measured.
 */
#define V17TXP_NOCARRIER_SYM	0x1e

#define V17FP_SMC_RING		0x08	/* struct fpm_smc_ring              */
#define V17FP_SMC		0x34	/* the SMCv17 coder state, unmodelled */
#define V17FP_PPS		0x48	/* struct fpm_pps                    */

/*
 * More of `V17FP_SMC`, all written by `SetTxModeV17` and none of it any
 * better established than `V17FP_SMC_SHORT_06` above -- neutral names, on
 * the same ground.
 *
 * `V17FP_SMC_SHORT_00` is `V17FP_SMC` itself, so it has no separate name;
 * `SetTxModeV17` copies `SMCv17_CFG` -- two shorts, one dword -- over
 * `V17FP_SMC`/`V17FP_SMC_SHORT_02` in one move (`0xa0b89`), then every
 * recognised mode overwrites just `V17FP_SMC` with a literal (3, 2, 4, 5 for
 * modes 0..3) while `V17FP_SMC_SHORT_02` is separately and unconditionally
 * pinned to 2 a few bytes later (`0xa0bad`) -- so `SMCv17_CFG`'s second short
 * never survives past construction and only its first is ever read back, on
 * whatever mode falls through to `default`.  `V17FP_SMC_SHORT_12` is the one
 * literal that follows `mode` cleanly: 1, 2, 3, 4 for modes 0..3
 * (`0xa0cb8`, `0xa0c11`, `0xa0c60`, `0xa0ce7`).
 */
#define V17FP_SMC_SHORT_02	0x36
#define V17FP_SMC_SHORT_08	0x3c
#define V17FP_SMC_SHORT_0C	0x40
#define V17FP_SMC_SHORT_0E	0x42
#define V17FP_SMC_SHORT_10	0x44
#define V17FP_SMC_SHORT_12	0x46

/*
 * The constellation maps `SetTxModeV17` selects per mode, typed by their
 * callers: every relocation at these two offsets names a `VTBv17_{I,Q}MAP*`
 * table from `v17cfg.h` -- mode 0 (16T) at `0xa0ccb`/`0xa0cd5`, mode 1 (32) at
 * `0xa0c24`/`0xa0c2e`, mode 2 (64) at `0xa0c73`/`0xa0c7a`, mode 3 (128) at
 * `0xa0cfa`/`0xa0d04`.  The same four tables back `RxNextStateV17`'s VTB
 * switch on the receive side of this object; the two paths do not share code,
 * only the constants.  Rank 2 evidence: the pointer's own type, not usage.
 */
#define V17FP_SMC_IMAP		0x58
#define V17FP_SMC_QMAP		0x5c

/*
 * `V17TX_create`'s own pulse-shaper setup (not itself reconstructed): it
 * copies `FPM_PPS_CFG` (`fpm_pps.h`) onto its stack and patches `imap`/`qmap`
 * from these two tables, `coeff_i`/`coeff_q` from the pair below, and `scale`
 * from `V17TX_PPS_SCALE[rate]` -- an `int` table, not a `short` one, forced
 * by the signed 32-bit `imul` that reads it.
 */
extern const short SMCv17_IMAP4[5];
extern const short SMCv17_QMAP4[5];
extern const int V17TX_PPS_SCALE[4];

/*
 * `TxNextStateV17`'s scrambler-pattern table (not itself reconstructed;
 * three read sites, all signed, forced).  Needed for `V17TX_create`'s
 * closure rather than its own body: `V17TX_create` starts the state machine
 * `TxNextStateV17` implements, and that machine's ten-symbol batch (finding
 * F9600) is what actually reads this table.
 */
extern const short V17TX_PATTERN_SCR1[4];

/*
 * The pulse shaper's own coefficient pair, `V17TX_create`'s `coeff_i` /
 * `coeff_q` (see above).  120 entries each -- `coeffs / phases` = 12 taps at
 * `FPM_PPS_CFG.phases` = 10 -- and, like V.32's `PPSv32_ICOFFS`/
 * `PPSv32_QCOFFS`, the passband pair a quadrature pulse shaper always is: I
 * is even about the centre, Q is odd.  `test/unit/t_v17ppstab.c` checks both.
 */
extern const short PPSv17_ICOFFS[120];
extern const short PPSv17_QCOFFS[120];

/*
 * `SetTxModeV17`'s own two `.rodata` tables.
 *
 * `V17TX_SYM_SIZE` is indexed by `mode`, one entry per mode (3, 4, 5, 6
 * bits/symbol), and its value becomes both `struct sgd_cfg::sym_bits` and
 * `struct fpm_sdm_cfg::nbits` for the same call -- one load, spilled to the
 * stack and read back rather than recomputed.
 *
 * `SMCv17_CFG` is the two shorts `SetTxModeV17` copies over `V17FP_SMC` /
 * `V17FP_SMC_SHORT_02` in a single dword move before the per-mode switch
 * (see the block above).  Declared as an array of two, not a struct:
 * nothing here establishes a role for either half beyond "the value
 * `V17FP_SMC` starts with", and giving them field names would claim more
 * than the object does.
 */
extern const short V17TX_SYM_SIZE[4];
extern const short SMCv17_CFG[2];

/**
 * @brief (Re)configure the V.17 transmitter's SGD, descrambler and SMCv17
 *        coder state for one symbol rate.
 *
 * `mode` 0..3 select the 16T/32/64/128-point constellations (3/4/5/6
 * bits/symbol); anything else writes the same "unsupported" status pair
 * `V17TX_modem` writes on a short FIFO write, with one extra bit set (see
 * #V17TX_RESULT_BYTE_07 in v17fax.h).  Reuses an existing SGD object if the
 * caller already built one.  The descrambler's shift register survives its
 * own re-init: this function reads it before calling `SDM_init` and writes
 * the same value back afterward, so a rate change keeps the running state
 * rather than restarting it at zero -- reproduced as the object does it,
 * without a stated reason.
 *
 * @param modem  The V.17 modem object.
 * @param mode   Constellation/rate selector, 0..3.
 */
void SetTxModeV17(void *modem, short mode);

/*
 * The encoder table and its selector.
 *
 * Three function pointers, laid down by `V17TX_create` in the order
 * dif / abs / tcm, and a `short` immediately after them that `ModDataV17`
 * loads and scales by four.  The load is signed and the 32-bit result is
 * used as the index, so `short` is forced -- CLAUDE.md's rule for reading a
 * codegen difference, applied the way round it is meant to be.  `V17TX_create`
 * never writes the selector, so whatever sets it is outside what has been
 * read; `SetEncoderV17` (`v17fax.h`) is what bounds it to 0, 1 or 2 (finding
 * F8852).
 */
#define V17FP_ENCODERS		0x80
#define V17FP_ENCODER_SEL	0x8c
#define V17FP_ENCODERS_N	3

/*
 * What the three table entries are.
 *
 * `SMCv17_encoder_dif` loads each input word with `movzwl (%edi)` into a
 * 32-bit value it then shifts and masks, so the element type is
 * `unsigned short` and that is forced.  (`SMCv32_encoder_dif`, the same
 * layer for V.32, loads `movswl` and `v32smc.h` declares `const short *`
 * accordingly -- the two families genuinely differ here.)
 *
 * The first parameter is the coder state at V17FP_SMC, which nothing
 * reconstructed models, so it is `void *`.
 */
typedef void (*v17_encoder_fn)(void *smc, struct fpm_smc_ring *ring,
			       const unsigned short *data,
			       unsigned short count);

/* The V.17-specific symbol coder.  This is not `struct fpm_smc`. */
struct v17_smc {
	union {
		short word;
		struct {
			signed char value;
			unsigned char r01;
		} byte;
	} mode;			/* +0x00 whole-word stores, low-byte reads */
	short r02;		/* +0x02 copied from SMCv17_CFG            */
	/* +0x04 is in no dataflow at all -- it is absent from the field map
	 * above and written and read by nothing reconstructed -- retained
	 * neutral (Batch 28). */
	short r04;		/* +0x04 unmodelled                        */
	short quad;		/* +0x06                                   */
	short state;		/* +0x08 differential state                */
	/* +0x0a, like +0x04, is in no dataflow; no reader and no writer --
	 * retained neutral (Batch 28). */
	short r0a;		/* +0x0a unmodelled                        */
	short trellis;		/* +0x0c                                   */
	short prev;		/* +0x0e previous trellis state            */
	short r10;		/* +0x10 neutral, cleared by init           */
	unsigned short nbits;	/* +0x12                                   */
};

/* V17TX_create's 0x90-byte fixed-point block. */
struct v17tx_fp {
	/* +0x00 is an eight-byte head nothing reconstructed touches.  The
	 * sibling transmit blocks (`v27_tx_block`, `v32_symout`) call an
	 * identical head padding, but this one's content is not established
	 * and giving it the sibling's name would claim more than the object
	 * says -- retained neutral (Batch 28). */
	unsigned char r00[8];		/* +0x00 unmodelled                   */
	struct fpm_smc_ring ring;	/* +0x08 shared mapper/shaper ring    */
	struct fpm_sdm sdm;		/* +0x1c transmit scrambler           */
	struct v17_smc smc;		/* +0x34 V.17-specific symbol coder   */
	struct fpm_pps pps;		/* +0x48 pulse shaper                 */
	v17_encoder_fn encoders[V17FP_ENCODERS_N]; /* +0x80              */
	short encoder_sel;		/* +0x8c                              */
	/* +0x8e, the block's trailing short, is written and read by nothing
	 * reconstructed -- retained neutral (Batch 28). */
	short r8e;			/* +0x8e unmodelled                   */
};

/**
 * @brief Initialise one SMCv17 coder state.
 *
 * Loads `cfg` (or `SMCv17_CFG` when `cfg` is NULL) into the leading dword of
 * `smc` -- `V17FP_SMC` and `V17FP_SMC_SHORT_02` together, one dword move,
 * exactly what `SetTxModeV17` also copies from the same source -- and
 * clears the five neutral shorts above it, `V17FP_SMC_SHORT_06` through
 * `V17FP_SMC_SHORT_10`.  A second, independent confirmation of all five
 * offsets: the same five are zeroed here, by a different function, agreeing
 * with `SetTxModeV17` without either being derived from the other.
 *
 * @param smc  The SMCv17 coder state (`V17FP_SMC`).
 * @param cfg  Two shorts to seed it with, or NULL for #SMCv17_CFG.
 */
void SMCv17_init(void *smc, const short *cfg);

/*
 * What the three encoders' fields are, from their own dataflow.
 *
 * Each encoder's first parameter IS `V17FP_SMC` -- so inside them, a field's
 * offset is `V17FP_SMC_SHORT_NN - V17FP_SMC`, not `V17FP_SMC_SHORT_NN`
 * itself.  Usage inference (CLAUDE.md's weakest evidence tier), but taken
 * from three independent functions that all agree, and cross-checked
 * against a fourth family: V.32's homologous coder (`v32smc.c`) uses the
 * same three-arm shape -- differential accumulator, absolute table, trellis
 * state plus a "previous transition" test against 3 -- at different offsets
 * in its own (modelled) struct, and its `TrellisEncodeDifTable`,
 * `TrellisTransitionTable` and `SMCv32_MOD` are byte-identical to the three
 * tables V.17's trellis arm indexes below.  Neither family's naming was
 * copied onto the other; the offsets differ (V.17 keeps "trellis" at
 * `V17FP_SMC_SHORT_0C` where V.32 keeps it at its own `f0e`) and only the
 * table CONTENTS and the algorithm's SHAPE repeat.
 *
 *   smc + 0x00  the byte `SetEncoderV17`/`SetTxModeV17` write as `V17FP_SMC`
 *               itself: `SMCv17_encoder_tcm`'s tag, read `movsbl` (signed,
 *               free -- only its low 8 bits reach the 16-bit store)
 *   smc + 0x06  V17FP_SMC_SHORT_06 -- "quad": `(quad + 3) & 3` every symbol
 *               in all three encoders; `SetEncoderV17`'s `arg` seeds it
 *               directly on the DIF and TCM arms
 *   smc + 0x08  V17FP_SMC_SHORT_08 -- the differential accumulator
 *               `SMCv17_encoder_dif` alone reads and writes
 *   smc + 0x0c  V17FP_SMC_SHORT_0C -- the trellis state `SMCv17_encoder_tcm`
 *               alone reads and writes, indexing `TrellisEncodeDifTable`
 *   smc + 0x0e  V17FP_SMC_SHORT_0E -- the previous trellis transition,
 *               `SMCv17_encoder_tcm` alone; compared against 3 before each
 *               symbol (`cmpw $0x3` / `jle`) and replaced from
 *               `TrellisTransitionTable`
 *   smc + 0x12  V17FP_SMC_SHORT_12 -- the shift `SetTxModeV17` sets per mode
 *               (1, 2, 3, 4), UNSIGNED (`movzwl`), used both as a shift
 *               count and to build the three masks `SMCv17_encoder_tcm`
 *               derives from it once, before its loop
 *
 * `smc + 0x04` and `smc + 0x0a` appear in none of that dataflow -- no
 * reader and no writer reconstructed -- and are retained neutral (Batch 28).
 */
extern const unsigned short SMCv17_PMAP4[4];	/* differential quadrant map,
						 * SMCv17_encoder_dif only    */
extern const unsigned short SMCv17_ABS4[4];	/* absolute quadrant map,
						 * SMCv17_encoder_abs only    */
extern const unsigned short SMCv17_MOD[8];	/* trellis rotation table --
						 * bytewise == V.32's
						 * SMCv32_MOD, see v32smc.c   */

/**
 * @brief The differential SMCv17 encoder: `state = (state +
 *        PMAP4[in & 3]) & 3; point = (state + quad + 1) & 3`.
 *
 * Matches #v17_encoder_fn: `data` is read `movzwl` (forced unsigned) and
 * neither this nor SMCv17_encoder_abs() writes through it.
 *
 * @param smc    The SMCv17 coder state (`V17FP_SMC`).
 * @param ring   The symbol ring to write decided points into.
 * @param data   `count` input data words (only their low two bits matter).
 * @param count  Number of symbols to encode.
 */
void SMCv17_encoder_dif(void *smc, struct fpm_smc_ring *ring,
			const unsigned short *data, unsigned short count);
/**
 * @brief The absolute SMCv17 encoder: `point = (quad + ABS4[in & 3]) & 3`,
 *        with no accumulator.
 *
 * @param smc    The SMCv17 coder state (`V17FP_SMC`).
 * @param ring   The symbol ring to write decided points into.
 * @param data   `count` input data words (only their low two bits matter).
 * @param count  Number of symbols to encode.
 */
void SMCv17_encoder_abs(void *smc, struct fpm_smc_ring *ring,
			const unsigned short *data, unsigned short count);

/**
 * @brief The trellis (TCM) SMCv17 encoder.
 *
 * Not #v17_encoder_fn-shaped, on the object's own evidence: it masks each
 * input word and writes the result back in place before the loop ever reads
 * that slot again, so `data` cannot be `const`.  `SMCv32_encoder_tcm` breaks
 * from its own family's typedef the same way and for the same reason (see
 * `v32smc.c`).  Whether `V17TX_create` stores this function through
 * #v17_encoder_fn regardless, or through a wider type, is not established --
 * that function is not yet reconstructed.  Exactly `SMCv32_encoder_tcm`'s
 * algorithm, reusing that file's `TrellisEncodeDifTable`/
 * `TrellisTransitionTable` tables; only the state's offsets differ.
 *
 * @param smc    The SMCv17 coder state (`V17FP_SMC`).
 * @param ring   The symbol ring to write decided points into.
 * @param data   `count` input data words, masked and written back in place.
 * @param count  Number of symbols to encode.
 */
void SMCv17_encoder_tcm(void *smc, struct fpm_smc_ring *ring,
			unsigned short *data, unsigned short count);

/**
 * @brief Encode `count` data words and shape them into `out`.
 *
 * Dispatches through the instance's own three-entry encoder table
 * (#V17FP_ENCODERS, selected by #V17FP_ENCODER_SEL) to fill the symbol
 * ring, then hands the ring to `FPM_PPS_filter`, the same shaper
 * `TxNoCarrierV17()` also drives.  The instance pointer is read again after
 * the encoder returns rather than kept in a register -- not observable
 * through any call this function can make, and what the compiler was
 * forced to encode (the same idiom `v22data.c` records for `ModDataV22`).
 *
 * @param modem  The V.17 modem object.
 * @param data   `count` data words to encode.
 * @param out    Destination for the shaped output samples.
 * @param count  Number of data words to encode.
 * @return The number of samples written to `out`.
 */
unsigned short ModDataV17(void *modem, const unsigned short *data, short *out,
			  unsigned short count);

/**
 * @brief Fill `count` symbol slots with the fixed no-carrier index and
 *        shape them into `out`.
 *
 * Writes the ring's `sym` field directly, one constant index
 * (#V17TXP_NOCARRIER_SYM) per symbol, re-reading that field on every
 * iteration rather than hoisting it (finding F3644) -- and never touches
 * the SMCv17 coder.  `data` is never read: the object touches three of its
 * four arguments and ignores the second; this is declared with
 * `ModDataV17()`'s type because the two are the transmit side of one
 * interface, which the object does not itself settle and cdecl makes
 * harmless either way.  The ring's write cursor is written back after the
 * shaper runs, through a fresh read of the instance pointer.
 *
 * @param modem  The V.17 modem object.
 * @param data   Unused.
 * @param out    Destination for the shaped output samples.
 * @param count  Number of symbol slots to fill.
 * @return The number of samples written to `out`.
 */
unsigned short TxNoCarrierV17(void *modem, const unsigned short *data,
			      short *out, unsigned short count);

#endif /* DSPLIB_V17DATA_H */

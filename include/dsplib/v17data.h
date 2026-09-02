/*
 * v17data.h -- ITU-T V.17 (fax): the transmitter's data-path leaves.
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
 * THE INSTANCE IS NOT MODELLED, and this header follows the ruling
 * `include/dsplib/v22data.h` sets out: `V17TX_create` -- 1,043 bytes that lay
 * the instance out -- is not reconstructed, so naming its fields now would
 * mean guessing.  The parameter is `void *` and the offsets are named
 * constants with the evidence beside each.
 *
 * ---------------------------------------------------------------------------
 * EVERY OFFSET BELOW IS CONFIRMED TWICE
 *
 * Once by the function that uses it, and once by `V17TX_create`, which
 * constructs the same sub-object at the same offset (addresses are into
 * dsplibs.o):
 *
 *   98cf3  sysdep_malloc(0x90)  -> obj + 0x28     the block every offset
 *                                                 below is relative to
 *   98d04  sysdep_malloc(0x64)  -> fp + 0x10      100 bytes = 50 shorts,
 *                                                 the ring's `sym` array
 *   98b59  fp + 0x14 = 0, fp + 0x16 = 0, fp + 0x18 = 0x32, fp + 0x8 = NULL,
 *          fp + 0xc = NULL, and sym[0..49] cleared -- which is
 *          `struct fpm_smc_ring` at fp + 0x08, field for field, with a
 *          length of 50 that the clear loop's `cmp $0x31` confirms
 *   98bde  SMCv17_init          -> fp + 0x34
 *   98c7e  FPM_PPS_init         -> fp + 0x48
 *   98c90  fp + 0x80 = SMCv17_encoder_dif
 *   98c98  fp + 0x84 = SMCv17_encoder_abs
 *   98c9e  fp + 0x88 = SMCv17_encoder_tcm
 *
 * so the pairing of offset to module is not inferred from a callee's name
 * alone.  `V17TX_create` also builds an `SDM` at fp + 0x1c; neither function
 * here touches it.
 *
 * ---------------------------------------------------------------------------
 * THE RING CARRIES INDICES, NOT RAILS, AND THAT IS STATED TWICE TOO
 *
 * `V17TX_create` builds the shaper's configuration on the stack at 98be3 with
 * `mapped` = 1, `imap` = SMCv17_IMAP4 and `qmap` = SMCv17_QMAP4, which
 * `fpm_pps.h` says selects the form where the ring entry's low byte indexes
 * the two maps.  Independently, `TxNoCarrierV17` writes `sym` (ring + 0x08)
 * and leaves `i` and `q` alone -- and `V29TX_create`, whose `mapped` is 0,
 * has `TxNoCarrierV29` writing `i` and `q` and leaving `sym` alone.  The two
 * readings agree without either being derived from the other.
 */

#ifndef DSPLIB_V17DATA_H
#define DSPLIB_V17DATA_H

struct fpm_smc_ring;

/*
 * Offsets in the V.17 transmitter instance.
 */

/*
 * The transmitter's private block, 0x90 bytes from `sysdep_malloc`.  Every
 * V17FP_* below is relative to what this field points at.
 */
#define V17TX_OBJ_FP		0x28

/*
 * A parameter block the instance points at rather than owns.  NEUTRAL NAME
 * AND DELIBERATELY SO: what is established is that `V17TX_create` reads a
 * rate index from its +0x10 to select the shaper's output gain out of
 * `V17TX_PPS_SCALE`, and that `TxNoCarrierV17` reads +0x1e.  Nothing traced
 * writes either, so the block's extent and owner are unknown.
 */
#define V17TX_OBJ_PARAMS	0x24

/*
 * The symbol index `TxNoCarrierV17` fills the ring with, in the block above.
 *
 * Named from the function's own name, which is the author's.  What the object
 * establishes is narrower: this is the only field of that block either
 * function reads, and its value goes into every symbol slot the no-carrier
 * transmit path produces.  It is loaded `movzwl` but only `%ax` is used, so
 * the SIGNEDNESS IS FREE and `unsigned short` here follows the ring's element
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
 * `V17FP_SMC_SHORT_00` IS `V17FP_SMC` ITSELF, so it has no separate name;
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
 * `SetTxModeV17`'s own two `.rodata` tables.
 *
 * `V17TX_SYM_SIZE` is indexed by `mode` (`movswl 0x0(%ebp,%ebp,1),%edx` at
 * `0xa0b03`) and its value becomes both `struct sgd_cfg::sym_bits` and
 * `struct fpm_sdm_cfg::nbits` for the same call -- one load, spilled to the
 * stack and read back rather than recomputed (`0xa0b19`, `0xa0b41`).  Four
 * entries, one per mode, `.rodata` bytes `03 00 04 00 05 00 06 00`.
 *
 * `SMCv17_CFG` is the two shorts `SetTxModeV17` copies over `V17FP_SMC` /
 * `V17FP_SMC_SHORT_02` in a single dword move before the per-mode switch
 * (see the block above) -- `.rodata` bytes `00 00 01 00`.  Declared as an
 * array of two, not a struct: nothing here establishes a role for either
 * half beyond "the value `V17FP_SMC` starts with", and giving them field
 * names would claim more than the object does.
 */
extern const short V17TX_SYM_SIZE[4];
extern const short SMCv17_CFG[2];

/*
 * SetTxModeV17 -- .text 0x0a0ac0, 625 bytes.  See v17fax.h for what each
 * mode selects and for the register `SDM_init` clears and this restores.
 */
void SetTxModeV17(void *modem, short mode);

/*
 * The encoder table and its selector.
 *
 * Three function pointers, laid down by `V17TX_create` in the order
 * dif / abs / tcm, and a `short` immediately after them that `ModDataV17`
 * loads with `movswl` and scales by four.  THE LOAD IS SIGNED AND THE 32-BIT
 * RESULT IS USED as the index, so `short` is forced -- CLAUDE.md's rule for
 * reading a codegen difference, applied the way round it is meant to be.
 * Nothing here bounds the index; `V17TX_create` never writes it, so whatever
 * sets it is outside what has been read.
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

/*
 * SMCv17_init -- .text 0x0a0a60, 87 bytes.
 *
 * Loads `cfg` (or `SMCv17_CFG` when `cfg` is NULL) into the leading dword of
 * `smc` -- `V17FP_SMC` and `V17FP_SMC_SHORT_02` together, one dword move,
 * exactly what `SetTxModeV17` also copies from the same source at `0xa0b89`
 * -- and clears the five neutral shorts above it, `V17FP_SMC_SHORT_06`
 * through `V17FP_SMC_SHORT_10`.  A SECOND, independent confirmation of every
 * one of those five offsets: the same five are zeroed here, by a different
 * function, from a different address, agreeing with `SetTxModeV17` without
 * either being derived from the other.
 */
void SMCv17_init(void *smc, const short *cfg);

/*
 * WHAT THE THREE ENCODERS' FIELDS ARE, FROM THEIR OWN DATAFLOW
 * ---------------------------------------------------------------------
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
 */
extern const unsigned short SMCv17_PMAP4[4];	/* differential quadrant map,
						 * SMCv17_encoder_dif only    */
extern const unsigned short SMCv17_ABS4[4];	/* absolute quadrant map,
						 * SMCv17_encoder_abs only    */
extern const unsigned short SMCv17_MOD[8];	/* trellis rotation table --
						 * bytewise == V.32's
						 * SMCv32_MOD, see v32smc.c   */

/*
 * SMCv17_encoder_dif -- .text 0x09fcc0, 164 bytes.
 * SMCv17_encoder_abs -- .text 0x09fd70, 135 bytes.
 *
 * Both match `v17_encoder_fn`: `data` is read `movzwl` (forced UNSIGNED) and
 * neither writes through it.
 */
void SMCv17_encoder_dif(void *smc, struct fpm_smc_ring *ring,
			const unsigned short *data, unsigned short count);
void SMCv17_encoder_abs(void *smc, struct fpm_smc_ring *ring,
			const unsigned short *data, unsigned short count);

/*
 * SMCv17_encoder_tcm -- .text 0x09fe00, 374 bytes.
 *
 * NOT `v17_encoder_fn`-shaped, on the object's own evidence: it masks each
 * input word and WRITES THE RESULT BACK in place (`mov %ax,(%edx)` at
 * `0x9fea8`) before the loop ever reads that slot again, so `data` cannot be
 * `const`.  `SMCv32_encoder_tcm` breaks from its own family's typedef the
 * same way and for the same reason (see `v32smc.c`).  Whether `V17TX_create`
 * stores this function through `v17_encoder_fn` regardless, or through a
 * wider type, is not established -- that function is not yet reconstructed.
 */
void SMCv17_encoder_tcm(void *smc, struct fpm_smc_ring *ring,
			unsigned short *data, unsigned short count);

/*
 * Modulate `count` data words into `out`, returning the number of samples
 * written.
 *
 * The instance pointer is READ AGAIN after the encoder returns -- `mov
 * 0x28(%esi),%eax` at 0xa0d84, not a reload from the stack -- which is the
 * same idiom `v22data.c` records for `ModDataV22`.  It is not observable
 * through any call this function can make, and it is what the compiler was
 * forced to encode.
 *
 * RETURNS `unsigned short`: the object zero-extends the shaper's return with
 * `movzwl %ax,%eax` before the epilogue.  `FPM_PPS_filter` already returns
 * `unsigned short`, so unlike `ModDataV22` -- whose callee returns `short` --
 * there is no truncation of meaning here, only the ABI's extension.
 */
unsigned short ModDataV17(void *modem, const unsigned short *data, short *out,
			  unsigned short count);

/*
 * Fill `count` symbol slots with the no-carrier index and shape them into
 * `out`, returning the number of samples written.
 *
 * THE SECOND ARGUMENT IS NEVER READ.  Nothing at 0x34(%esp) is touched, while
 * 0x30, 0x38 and 0x3c all are, so the function takes four arguments and
 * ignores the second.  It is declared with `ModDataV17`'s type because the
 * two are the transmit side of one interface; the object does not settle
 * that and cdecl makes it harmless either way.
 *
 * The ring's write cursor is written back AFTER the shaper runs, and the
 * instance pointer is read again to do it.  That ordering is not observable:
 * `FPM_PPS_filter` writes only `ridx` of the ring it is given.
 */
unsigned short TxNoCarrierV17(void *modem, const unsigned short *data,
			      short *out, unsigned short count);

#endif /* DSPLIB_V17DATA_H */

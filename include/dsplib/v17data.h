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

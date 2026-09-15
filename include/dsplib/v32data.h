/*
 * v32data.h -- ITU-T V.32 / V.32bis: the transmitter's data-path leaves.
 *
 *   ModDataV32       -> one of three SMCv32 encoders, then FPM_PPS_filter
 *   TxNoCarrierV32   -> fills the symbol ring by hand, then FPM_PPS_filter
 *
 * `tools/service.py` puts both in DATA MODE, which is why they live in
 * `src/pump/v32/` while the V.17 and V.29 functions of the same shape do not.
 * `make worklist` disagrees and is not the authority: it brackets these two
 * into `Dialer.c +18`, a span over nineteen translation units, and they are
 * V.32 and not dialling.
 *
 * THE INSTANCE IS NOT MODELLED; the parameter is `void *` and the offsets are
 * named constants, following `include/dsplib/v22data.h`'s ruling.  The
 * SUB-OBJECTS are modelled, and by somebody else: `struct v32_smc` and
 * `struct v32_symout` are `v32smc.h`'s, written from the three encoders.
 *
 * ---------------------------------------------------------------------------
 * EVERY OFFSET CONFIRMED TWICE, THE SECOND TIME BY `V32FP_recreate`
 *
 *   7eb27  fp + 0x60   FPM_PPS_init
 *   7ef78  fp + 0x98 = SMCv32_encoder_dif
 *   7ef8c  fp + 0x9c = SMCv32_encoder_abs
 *   7efc0  fp + 0xa0 = SMCv32_encoder_tcm
 *   7efb2  fp + 0xa4 = 0                    the selector, cleared
 *   7ef6d  fp + 0xb8 = the symbol buffer    (= symout + 0x08)
 *   7ef50  fp + 0xbc = 0                    (= symout + 0x0c, `widx`)
 *   7efab  fp + 0xc0 = V32_SYMBOL_LEN[rate] (= symout + 0x10, `limit`)
 *
 * and `fp` is `obj + 0x68` at every one of those sites.  `V32FP_recreate` does
 * not call an `SMC_init`; the coder at fp + 0x48 is established instead by
 * `ModDataV32` handing that address to an `SMCv32_encoder_*`, and confirmed by
 * `TxNoCarrierV32` reaching fp + 0x4e, which is `struct v32_smc`'s `quad`.
 */

#ifndef DSPLIB_V32DATA_H
#define DSPLIB_V32DATA_H

#include "dsplib/v32struct.h"

struct fpm_smc_ring;

/* The V.32 datapump's private block; every V32FP_* is relative to it. */
#define V32_OBJ_FP		0x68

#define V32FP_SMC		0x48	/* struct v32_smc                    */
#define V32FP_PPS		0x60	/* struct fpm_pps                    */
#define V32FP_ENCODERS		0x98	/* dif, abs, tcm                     */
#define V32FP_ENCODER_SEL	0xa4	/* short, `movswl`-loaded index      */
#define V32FP_ENCODERS_N	3
#define V32FP_SYMOUT		0xb0	/* struct v32_symout                 */

/*
 * The constellation index `TxNoCarrierV32` fills the ring with.
 *
 * `SMCv32_IMAP16` and `SMCv32_QMAP16` are 0x22 bytes each -- SEVENTEEN
 * shorts, one past the sixteen points a V.32bis constellation has -- and
 * `FPM_PPS_filter` indexes them with the ring entry's low byte.  So 0x10 is
 * the seventeenth entry, an extra point appended to both maps for exactly
 * this path.  The map SIZE is where that comes from; the object writes the
 * bare literal.
 */
#define V32_SYMBOL_NOCARRIER	0x10

/*
 * The three arms of the encoder table.
 *
 * `short *` AND NOT `const short *`: `SMCv32_encoder_tcm` masks each input
 * word in place, so the one type the table can hold is the non-const one.
 * `v32smc.h` declares the other two `const short *` from their own reading and
 * both are true -- the table's element type is what a caller must satisfy.
 */

/**
 * @brief Modulate `count` V.32 data words into `out`.
 *
 * Encodes through whichever of the three SMCv32 encoders is selected
 * (V32FP_ENCODER_SEL), then shapes through `FPM_PPS_filter`.
 *
 * The instance pointer is read again after the encoder returns (`mov
 * 0x68(%edi),%eax` at 0x81bce), the same idiom `v22data.c` records.
 *
 * @p data is not const, and that is the trellis arm's doing rather than
 * this function's: two of the three encoders only read it.
 *
 * @param modem  The V.32 datapump instance.
 * @param data   The data words to modulate.
 * @param out    Output for the modulated samples.
 * @param count  How many data words.
 * @return The number of samples written.
 */
unsigned short ModDataV32(void *modem, short *data, short *out,
			  unsigned short count);

/**
 * @brief Modulate `count` symbols of the fixed no-carrier pattern into `out`.
 *
 * Fills @p count symbol slots with V32_SYMBOL_NOCARRIER and shapes them
 * into @p out.
 *
 * The second argument is never read -- nothing at `0x34(%esp)` is touched.
 * It also steps the coder's quadrant accumulator backwards, once per
 * symbol, and never reads it: `quad = (quad + 3) & 3`. What that is for
 * is not established and is not claimed here; `quad` is `v32smc.h`'s
 * name, given by the three encoders that do read it, and all this
 * function does is move it.
 *
 * Unlike the V.17 and V.29 functions of the same shape, both write-backs
 * happen before the shaper runs and the instance pointer is not re-read.
 *
 * @param modem  The V.32 datapump instance.
 * @param data   Unread.
 * @param out    Output for the modulated (silent-pattern) samples.
 * @param count  How many symbols.
 * @return The number of samples written.
 */
unsigned short TxNoCarrierV32(void *modem, const short *data, short *out,
			      unsigned short count);

#endif /* DSPLIB_V32DATA_H */

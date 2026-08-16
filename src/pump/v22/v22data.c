/*
 * v22data.c -- V.22 / V.22bis: the datapump's small data-path leaves.
 *
 * Reconstructed from dsplibs.o:
 *
 *   Detect_v22          .text 0x08c1c0  375   the 0x8bd50..0x8c5a0 block,
 *                                             with the rest of the Detect_*
 *                                             family and v22prc.c's TxNOP
 *
 *   ScrambleDataV22     .text 0x08e2d0   28   the 0x8e120..0x8e669 block,
 *   DescrambleDataV22   .text 0x08e2f0   30   with DemodDataV22 and
 *   ModDataV22          .text 0x08e310   95   v22prc.c's TxClockSync
 *
 * Kept together because they are one layer and not one translation unit:
 * every one of them is a wrapper that picks a sub-object out of the datapump
 * instance and calls the module that owns it.  `tools/tumap.py` puts thirteen
 * V.22 translation units in one bracket and cannot say which of them either
 * address block is.
 *
 * ---------------------------------------------------------------------------
 * THE INSTANCE POINTER IS READ AGAIN AFTER EVERY CALL, AND THAT IS THE SOURCE
 *
 * `ModDataV22` reloads `0x54(%esi)` between its two calls, and `Detect_v22`
 * reloads `0x50(%edx)` between each pair of detector calls, with the instance
 * pointer itself live in a callee-saved register the whole time.  A local
 * holding the sub-object pointer across a call would have been SPILLED and
 * reloaded from the stack -- `mov 0x10(%esp),%eax`, not `mov 0x54(%esi),%eax`
 * -- so the original reads the field once per use.  Written that way here.
 * It is not observable through any call the object can make (no callee writes
 * either field), but it is what the compiler was forced to encode.
 */

#include "dsplib/v22data.h"

#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/v22_pps.h"

/* The object is not modelled; see v22data.h.  These are the only accessors. */
#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))

/*
 * Both scramblers are a two-instruction wrapper and a tail jump.  The only
 * thing that distinguishes them is the offset -- +0x30 for the transmit
 * scrambler, +0x1cc for the receive descrambler -- and which of the two
 * FPM_SDM entry points they jump to.  `V22FP_create` initialises an
 * `fpm_sdm` at each of those two offsets, so the pairing is confirmed
 * independently of these functions' names.
 */
void
ScrambleDataV22(void *modem, unsigned short *data, unsigned short count)
{
	void *fp = FIELD_PTR(modem, V22_OBJ_FP);

	FPM_SDM_scrambler((struct fpm_sdm *)FIELD(fp, V22FP_SDM_TX),
			  data, count);
}

void
DescrambleDataV22(void *modem, unsigned short *data, unsigned short count)
{
	void *fp = FIELD_PTR(modem, V22_OBJ_FP);

	FPM_SDM_descrambler((struct fpm_sdm *)FIELD(fp, V22FP_SDM_RX),
			    data, count);
}

/*
 * Bits to samples, in two stages that share one ring.
 *
 * THE RING IS THE POINT.  Both calls are handed `fp + 0xa0` as their second
 * argument -- the object computes `lea 0xa0(%eax),%edx` twice, once before
 * each -- so the symbol indices `FPM_SMC_encoder` writes are exactly the ones
 * `V22_PPS_filter` reads back.  `fpm_smc.h` records that two sessions
 * modelled half of that ring each, the producer's cursor and the consumer's;
 * this function is where the two halves meet.
 *
 * `count` is in DATA WORDS for the encoder and in SYMBOLS for the filter, and
 * the object passes the same value to both because the encoder makes exactly
 * one symbol per word.
 */
unsigned short
ModDataV22(void *modem, const unsigned short *data, short *out,
	   unsigned short count)
{
	void *fp;

	fp = FIELD_PTR(modem, V22_OBJ_FP);
	FPM_SMC_encoder((struct fpm_smc *)FIELD(fp, V22FP_SMC),
			(struct fpm_smc_ring *)FIELD(fp, V22FP_SMC_RING),
			data, count);

	fp = FIELD_PTR(modem, V22_OBJ_FP);
	return (unsigned short)V22_PPS_filter(
			(struct v22_pps *)FIELD(fp, V22FP_PPS),
			(struct fpm_smc_ring *)FIELD(fp, V22FP_SMC_RING),
			out, count);
}

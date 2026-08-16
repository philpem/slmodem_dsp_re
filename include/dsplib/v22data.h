/*
 * v22data.h -- V.22 / V.22bis: the datapump's small data-path leaves.
 *
 * Four functions that do nothing themselves except reach into the datapump
 * instance, pick a sub-object out of it, and hand that sub-object to the DSP
 * module that owns it:
 *
 *   ScrambleDataV22     -> FPM_SDM_scrambler
 *   DescrambleDataV22   -> FPM_SDM_descrambler
 *   ModDataV22          -> FPM_SMC_encoder then V22_PPS_filter
 *   Detect_v22          -> FPM_AGC_agc then FPM_MTD_detect, four times over
 *
 * THE OBJECT IS NOT MODELLED, DELIBERATELY, and this header follows the
 * ruling `include/dsplib/v22prc.h` sets out in full: `V22FP_create` -- the
 * 2,449-byte function that lays the instance out -- is not reconstructed, so
 * naming fields now would mean guessing.  The parameters are `void *` and the
 * offsets are named constants with the evidence beside each.
 *
 * `V22_OBJ_FP` and `V22_OBJ_GTIMER` are v22prc.h's and are used from there
 * rather than spelled a second time, which is why this header includes it.
 *
 * ---------------------------------------------------------------------------
 * EVERY OFFSET BELOW IS CONFIRMED TWICE
 *
 * Once by the function that uses it, and once by `V22FP_create`, which
 * constructs each sub-object at the same offset:
 *
 *     87da0  FPM_SDM_init   fp + 0x30      ScrambleDataV22    add $0x30
 *     87db7  FPM_SMC_init   fp + 0x48      ModDataV22         add $0x48
 *     87eaa  V22_PPS_init   fp + 0x78      ModDataV22         add $0x78
 *     87fd3  FPM_AGC_init   fp + 0xfc      Detect_v22         add $0xfc
 *     880bb  FPM_SDM_init   fp + 0x1cc     DescrambleDataV22  add $0x1cc
 *
 * so the pairing of offset to module is not inferred from the callee's name
 * alone.  (`V22FP_create` also builds a V22_MRF at +0xbc, a V22_SRE at +0x128
 * and a second FPM_AGC at +0xd0; none of the four functions here touches
 * them.)  The symbol ring at +0xa0 has no init call of its own and rests on
 * ModDataV22 handing the same pointer to both callees.
 */

#ifndef DSPLIB_V22DATA_H
#define DSPLIB_V22DATA_H

#include "dsplib/v22prc.h"

/*
 * Offsets in the object at V22_OBJ_FP.
 */
#define V22FP_SDM_TX		0x30	/* struct fpm_sdm,  the scrambler   */
#define V22FP_SMC		0x48	/* struct fpm_smc,  symbol coder    */

/*
 * struct v22_pps, the transmit pulse shaper.
 *
 * SAME FIELD AS v22prc.h's `V22FP_TX_CLOCK`, AND THAT NAME PREDATES THIS
 * EVIDENCE.  `TxClockSync` stores a short at fp + 0x78, which is the first
 * two bytes of the v22_pps state -- `cfg.step`, the number added to the
 * shaper's nominal phase step of 3.  Both names are kept: v22prc.h's because
 * it is what that function's own store licenses, this one because
 * `V22FP_create` calls `V22_PPS_init` on the same address.  They are one
 * address and one field, not two.  See the note in the source and the
 * finding, which also retires `v22_pps.h`'s claim that nothing in the object
 * ever assigns `step`.
 */
#define V22FP_PPS		0x78

#define V22FP_SMC_RING		0xa0	/* struct fpm_smc_ring, shared      */
#define V22FP_DET_AGC		0xfc	/* struct fpm_agc, built from
					 *      AGCv22_CFG2 at 87fba        */
#define V22FP_SDM_RX		0x1cc	/* struct fpm_sdm, the descrambler  */

/*
 * Offsets in the object at V22_OBJ_GTIMER.
 *
 * THAT POINTER IS NOT AN `int *`.  v22prc.h names +0x50 `V22_OBJ_GTIMER`
 * because `ReadGTimer` steps an `int` through it, and it does -- but
 * `Detect_v22` loads two POINTERS out of the same block, at +0x18 and +0x20,
 * and hands each to `FPM_MTD_detect`.  So +0x50 points at a block shared
 * across the V.22 modem whose +0x00 is the millisecond counter and whose
 * +0x18 and +0x20 are `struct fpm_mtd *`.  Nothing reconstructed builds that
 * block, so its extent and the rest of its fields are unknown; the two names
 * below are POSITIONAL and say nothing about which tone either detector
 * watches.
 */
#define V22SHR_MTD_A		0x18	/* first  detector Detect_v22 runs  */
#define V22SHR_MTD_B		0x20	/* second detector Detect_v22 runs  */

/*
 * Detect_v22's block structure.  One call gain-controls 160 samples and then
 * runs both detectors over four consecutive 40-sample sub-blocks of the same
 * buffer.  Every one of these is a literal in the object, not a parameter.
 */
#define V22_DETECT_BLOCK	160	/* 0xa0, the count given to the AGC */
#define V22_DETECT_SUBBLOCK	40	/* 0x28, the count given to each MTD */
#define V22_DETECT_SUBBLOCKS	4
/* Strictly more than this many consecutive zero verdicts is a detection. */
#define V22_DETECT_THRESHOLD	2
/* The sub-block is zeroed while the first counter is at most this. */
#define V22_DETECT_CLEAR_HOLD	1

/*
 * Scramble / descramble `count` data words in place.
 *
 * DECLARED void, AND THE OBJECT DOES NOT SETTLE THAT -- both are tail jumps
 * into a `void` callee, so nothing is left in %eax deliberately and nothing
 * extends it either way.  Same situation as `TxClockSync` in v22prc.h, and
 * recorded for the same reason.
 *
 * `count` is sixteen bits and unsigned: the object reloads the incoming word
 * with `movzwl` and stores the widened value back into its own outgoing
 * argument slot before jumping, which a 32-bit parameter would never need.
 */
void ScrambleDataV22(void *modem, unsigned short *data, unsigned short count);
void DescrambleDataV22(void *modem, unsigned short *data, unsigned short count);

/*
 * Modulate `count` data words into `out`, returning the number of samples
 * written.
 *
 * RETURNS `unsigned short`, WHICH IS NOT `V22_PPS_filter`'s RETURN TYPE.  The
 * filter returns `short` (see v22_pps.h) and the object zero-extends it --
 * `movzwl %ax,%eax` at 0x8e369, the last thing before the epilogue -- so a
 * count high enough to write more than 32,767 samples comes back as a large
 * positive number here and as a negative one from the filter.  v22_pps.h is
 * left alone: the truncation is this function's, and it is the extension on a
 * result whose 32-bit value IS used, which CLAUDE.md's rule says to act on.
 */
unsigned short ModDataV22(void *modem, const unsigned short *data, short *out,
			  unsigned short count);

/*
 * Gain-control one 160-sample block in place, run both tone detectors over
 * its four 40-sample sub-blocks, and report whether either detector has been
 * quiet for more than V22_DETECT_THRESHOLD consecutive sub-blocks.
 *
 * `data` is IN AND OUT: the AGC scales it, and each sub-block is zeroed
 * unless the first detector has already been quiet twice running.
 *
 * TWO ARGUMENTS, and that is what the object reads -- nothing above
 * 0x34(%esp) is touched.  The Detect_* family is reached through a table
 * elsewhere in the object, so a wider uniform signature is possible and would
 * be invisible here; cdecl makes the difference harmless either way.
 */
int Detect_v22(void *modem, short *data);

#endif /* DSPLIB_V22DATA_H */

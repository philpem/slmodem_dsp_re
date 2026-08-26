/*
 * v32demod.h -- ITU-T V.32 / V.32bis: the receive chain leaf.
 *
 *   DemodDataV32   .text 0x081c00   604 bytes
 *
 * One function, and it is the whole receive signal path between the line and
 * the equaliser: resample, cancel the echo, take a copy for the diagnostics,
 * gate on energy, apply the AGC, recover the symbol clock, then equalise.
 *
 * THE INSTANCE IS NOT MODELLED; the parameter is `void *` and the offsets are
 * named constants -- `v32data.h`'s ruling, and `v32hdx.h` follows it too.
 * The SUB-OBJECTS are modelled, and by somebody else: every one of the five
 * DSP blocks below already has a header and a reconstruction, and this file
 * only says where in `fp` each one sits.
 *
 * ---------------------------------------------------------------------------
 * THE FIVE SUB-OBJECTS, AND HOW EACH OFFSET WAS FIXED
 *
 * Each base is read off the argument the object hands its `FPM_*` callee, and
 * every field this function touches beyond that base lands inside the struct
 * that callee's header already declares -- which is the check that the base
 * is right, not merely consistent:
 *
 *   fp + 0x0c4  struct fpm_mrf   81c2b  add $0xc4  -> FPM_MRF_filter
 *   fp + 0x0e0  struct fpm_ecc   81c63  lea 0xe0   -> FPM_ECC_cancel
 *                                 +0x28 adapt_near, +0x2c adapt_far
 *   fp + 0x148  struct fpm_sre   81d3c  add $0x148 -> FPM_SRE_recover
 *                                 +0x38 mode, +0x40 active, +0x48 adapt
 *   fp + 0x1d8  struct fpm_agc   81ce0  lea 0x1d8  -> FPM_AGC_agc
 *                                 +0x18 f18, +0x1c signal
 *   fp + 0x204  struct fpm_fse   81daa  lea 0x204  -> FPM_FSE_receive
 *                                 +0x44 pll_on, +0x48 tilt_on, +0x4c lms_on
 *
 * ---------------------------------------------------------------------------
 * THE TWO BUFFERS, NAMED BY AN ACCESSOR THE AUTHOR WROTE
 *
 * `V32FP_recreate` gives both the same 0x154 bytes -- 170 shorts -- at 7f6dc
 * and 7f6f1, and `V32FP_GetCleanedSamples` (7f910) returns fp + 0x50d0 with
 * fp + 0x50d4 as its length, clamped to 0xa0.  So the copy this function
 * takes straight after the echo canceller is what the author calls the
 * CLEANED samples, and that is a name off the object rather than an
 * inference from what the copy is downstream of.
 *
 *   fp + 0x50c8  short           the working buffer's length
 *   fp + 0x50cc  short *         the working buffer, 170 shorts
 *   fp + 0x50d0  short *         the cleaned copy, 170 shorts
 *   fp + 0x50d4  short           its length
 *
 * +0x50c8 is `short` because `RxHdxSTone` (84362) loads it with `movswl` and
 * passes the 32-bit result as a count -- forced, per finding F614.  +0x50d4's
 * signedness is NOT forced: its one other reader (7f91b) loads it `movzwl`
 * and then overwrites the upper half with `cwtl`, which is the dead-extension
 * case finding F7803 rules on, and this function stores both as sixteen bits
 * either way.  Declared `short` for its sibling's sake and not claimed.
 *
 * ---------------------------------------------------------------------------
 * THE FOUR ENABLES, AND THE GATE OVER ALL OF THEM
 *
 * fp + 0x04, 0x08, 0x0c and 0x10 are four `int`s the instance owns, and this
 * function is the only thing that gives them meaning: each is copied into a
 * named enable of a sub-object, so the CALLEE'S OWN FIELD NAME types it.
 *
 *   fp + 0x04 -> sre.adapt      "caller's enable for the phase update"
 *   fp + 0x08 -> fse.pll_on     run carrier recovery
 *   fp + 0x0c -> fse.tilt_on    run the 4-tap error filter
 *   fp + 0x10 -> fse.lms_on     adapt the coefficients
 *
 * Three of the four are ANDed with `agc.f18` first.  `fpm_agc.h` says of that
 * field: "set to 1 by init on reset; agc() never reads it -- a caller must".
 * This is that caller, and what it does with it is gate the receive loop, so
 * +0x18 is an enable published by the AGC and consumed here.  Renaming it
 * belongs to `fpm_agc.h`'s owner and is not done from this batch.
 *
 * The FOURTH is not masked, and that asymmetry is D491.
 *
 * ---------------------------------------------------------------------------
 * WHY THERE ARE THREE WAYS TO RETURN ZERO, AND WHAT SEPARATES THEM
 *
 * A test that watches only the return value and the status byte cannot tell
 * the low-energy path from the no-carrier path: both return 0 and both clear
 * the same bit.  What separates them is how far down the chain the block got
 * -- the low-energy return happens BEFORE `FPM_AGC_agc`, so the AGC, the SRE
 * and the FSE have not moved.  Compare the whole `fp` block, not the return.
 */

#ifndef DSPLIB_V32DEMOD_H
#define DSPLIB_V32DEMOD_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The RMS floor the mode-6 receive path gates on.
 *
 * A `short` in the instance, compared against `FPM_rms` of the cancelled
 * block; below it the object clears the carrier bit and prints
 * "v32 low sig energy".  Named from that string, which is the author's own
 * word for what the comparison decides, and matching `fpm_sre.h`'s `rms_min`
 * for a floor of the same shape.  The comparison is the only site in this
 * function that reads it and no format string prints the value, so the SCALE
 * is not established here.
 */
#define V32_OBJ_RMS_MIN		0x28

/*
 * The instance's FLAGS byte.  Eight bits, of which this function owns two.
 *
 * Named by BIT VALUE and kept 1:1 with the object, per CLAUDE.md: a macro is
 * a compile-time substitution and cannot move code generation.
 *
 * THIS USED TO BE SPELLED `V32_OBJ_STATUS` AND THE RENAME IS NOT COSMETIC.
 * `include/dsplib/v32fpctl.h`, written in parallel, gives that same name to
 * obj + 0x30 -- one macro, two offsets, two headers, and legal C right up to
 * the first translation unit that includes both.  0x30 is a CODE and 0x31 a
 * bit field: `RxHdxNull` writes 0x10 to 0x30 beside the string
 * `V32_MSG_NO_CARRIER` and ORs 0x02 into 0x31, and `V32FP_recreate` clears
 * 0x30 as an int, stores a byte 1 into it, and ORs 0x40 into 0x31.  So the
 * split is the object's and this side is what moves.  See F8206.
 */
#define V32_OBJ_FLAGS		0x31

/*
 * Set from `sre.active`, which `fpm_sre.h` calls "the squelch let the PLL
 * run", and cleared on the path that prints "sre no carrier".  The string is
 * the author's own name for the condition that clears it.
 */
#define V32_FLAG_CARRIER	0x20

/*
 * Set when `agc.signal` is ZERO -- note the inversion.  `fpm_agc.h` types
 * that field "output: more than half the blocks in the last call were above
 * the gate", so the bit stands for the AGC having seen nothing.  Evidence is
 * the callee's own field name and its documented sense; no string prints it.
 */
#define V32_FLAG_SILENCE	0x40

/* The four enables, typed by the sub-object field each is copied into. */
#define V32FP_SRE_ADAPT_EN	0x04
#define V32FP_FSE_PLL_EN	0x08
#define V32FP_FSE_TILT_EN	0x0c
#define V32FP_FSE_LMS_EN	0x10

/* The five DSP blocks. */
#define V32FP_MRF		0xc4
#define V32FP_ECC		0xe0
#define V32FP_SRE		0x148
#define V32FP_AGC		0x1d8
#define V32FP_FSE		0x204

/* The working buffer and the cleaned copy. */
#define V32FP_RXLEN		0x50c8
#define V32FP_RXBUF		0x50cc
#define V32FP_CLEAN		0x50d0
#define V32FP_CLEANLEN		0x50d4

/*
 * The one value of hdx + 0x76 this function tests for.
 *
 * DELIBERATELY NOT NAMED FOR WHAT IT MEANS, because that is not established
 * and a wrong name would be believed.  What IS established: +0x76 is the mode
 * that indexes `V32NextState`, whose six slots the object names five of (Org,
 * Ans, LocLoop, LocLoop, RngInit); 6 is outside the table altogether;
 * `V32FP_modem` (82735) writes it together with `hdx->state =
 * V32_STATE_DONT_CARE` when bit 0 of the status byte is set; and on this path
 * the receive chain takes its equaliser enables from the instance instead of
 * forcing them on.  D492 records the out-of-table index.
 */
#define V32_MODE_6		6

/*
 * Demodulate one block.
 *
 * `in` is written as well as read: it carries the line samples in, and
 * `FPM_SRE_recover` puts the recovered symbols back into it for
 * `FPM_FSE_receive` to consume.  `out` takes the equaliser's decisions and is
 * `unsigned short *` because that is what `FPM_FSE_receive` declares.
 *
 * Returns the number of symbols the equaliser produced, or zero on any of the
 * three paths that decline the block.
 */
unsigned short DemodDataV32(void *modem, short *in, unsigned short *out,
			    unsigned short count);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32DEMOD_H */

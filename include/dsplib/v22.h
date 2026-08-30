/*
 * v22.h -- V.22 / V.22bis / Bell 212: the datapump's core-facing object.
 *
 * The modem core sees a `struct dp`; this module sees the block that begins
 * with one.  Same shape as `struct b103_dp` in b103.h, from the same author,
 * and `v22_delete` casts between them the same way.
 *
 * Reconstructed from dsplibs.o:
 *
 *   v22_delete  .text 0x0050e0   72 bytes
 *
 * ---------------------------------------------------------------------------
 * THE LAYOUT IS READ FROM `v22_create`, WHICH IS NOT WRITTEN YET
 *
 * `v22_create` (0x4fb0, 300 bytes) allocates 0x344 bytes, zeroes them, and
 * stores five things: the datapump id at +0x00, the modem at +0x04, the
 * operations table at +0x0c, the wrapper at BOTH +0x10 and +0x20, and the
 * V22FP object at +0x1c.  Those five are what is modelled here.  Everything
 * from +0x14 to +0x1b and everything above +0x24 is left unmapped, because
 * `v22_process` is what would name it and `v22_process` has not been read.
 *
 * So this is a PARTIAL model of a struct whose size is known exactly, and the
 * two `unmapped_*` runs are the honest statement of that.  Compare
 * `struct b103_dp`, where the same author's other datapump has `caller`,
 * `tx_bits_wanted` and `last_status` in the corresponding gap and two
 * hundred-entry bit buffers above -- a plausible guide to what will turn up
 * here, and NOT evidence for any of it.
 *
 * ---------------------------------------------------------------------------
 * WHAT `v22_create` SETTLES ABOUT THE CONFIGURATION IT BUILDS
 *
 * Read while modelling the layout, and recorded here because it corroborates
 * `v22fp.h`'s reading of `struct v22fp_cfg` from the other end:
 *
 *   cfg.mode = (caller == 0)      so mode 1 -- v22_answer, by F8529's table
 *                                 -- is the NOT-caller, and mode 0 is
 *                                 v22_originate.  The two agree.
 *   cfg.rate = 2 for id 212, 1 for id 22, 0 for anything else (id 122).
 *                                 v22fp.h has rate 0 giving 2400 and 1 and 2
 *                                 both giving 1200, so V.22bis gets 2400 and
 *                                 V.22 and Bell 212 get 1200.
 *   cfg.f08 = 60000, f0c = 0, f10 = 700, f14 = 0, f18 = 1
 *                                 exactly the constants v22fp.h names as
 *                                 coming from this caller.
 *
 * `dp_wrapper_create` is handed the object, `v22_process`, a fragment of 160
 * and a datapump rate of 8000 -- the same geometry B103 uses.
 */

#ifndef DSPLIB_V22_H
#define DSPLIB_V22_H

#include "dsplib/dp.h"

struct dp_wrapper;
struct v22fp;

/*
 * 836 bytes, of which the first 20 are the `struct dp` the modem core sees --
 * so a `struct dp *` from this module can be cast back to one of these, which
 * is what `v22_delete` does.
 */
struct v22_dp {
	struct dp dp;			/* +0x000 .. +0x013                */
	unsigned char unmapped_0014[0x1c - 0x14];
	struct v22fp *fp;		/* +0x01c the modulation           */
	struct dp_wrapper *wrapper;	/* +0x020                          */
	unsigned char unmapped_0024[0x344 - 0x24];
};

/* The datapump runs at 8 kHz in 160-sample fragments, as B103 does. */
#define V22_DP_SRATE	8000
#define V22_DP_FRAG	160

/*
 * The three ids `dp_v22_init` registers this datapump under -- 122, 22 and
 * 212, in that order -- and the two `v22_create` tests for when it picks a
 * rate.  slmodemd's `modem_defs.h` spells the first `DP_V22BIS = 122`.
 */
#define V22_DP_ID_V22BIS	122
#define V22_DP_ID_V22		22
#define V22_DP_ID_BELL212	212

/*
 * Tear one down.  Takes the `struct dp *` the core holds and reaches the
 * object back through the wrapper rather than casting directly -- see the
 * note in v22.c, which is `b103_delete`'s note too.
 */
int v22_delete(struct dp *dp);

#endif /* DSPLIB_V22_H */

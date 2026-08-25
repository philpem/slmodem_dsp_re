/*
 * v29data.h -- ITU-T V.29 (fax): the transmitter's no-carrier leaf.
 *
 * `TxNoCarrierV29` is `TxNoCarrierV17` with one thing changed, and that one
 * thing is the whole point of keeping the two apart: V.29's shaper is
 * configured with `cfg.mapped` CLEAR, so the ring carries I and Q directly
 * and this function zeroes both rails where V.17 writes a constellation
 * index into `sym`.  Finding F3641 is the pair of independent statements that
 * settle which is which.
 *
 * `tools/service.py` puts it on the FAX side.
 *
 * THE TRANSMITTER'S BLOCK IS NOW MODELLED, and this file is the worked
 * example for the four other objects still reached through `FIELD()` macros.
 * `v22data.h`'s ruling -- offsets as named constants against a `void *` --
 * records WHERE a field is and refuses to say WHAT it is, which is the
 * access-site form of a `pad_NNNN`.  Here the block turned out to be fully
 * determined, so there was nothing left to refuse.
 *
 * WHAT MADE IT DETERMINED, and it is the test to apply before converting any
 * of the others: the block's SIZE is known from its own allocation
 * (`sysdep_malloc(0x9c)` at 9bd69), and the three sub-objects the constructor
 * builds inside it are already-modelled types whose sizes are MEASURED --
 *
 *     struct fpm_smc_ring   0x14   at +0x08
 *     struct fpm_smc        0x30   at +0x34   (fpm_smc_cfg 0x2c + two shorts)
 *     struct fpm_pps        0x38   at +0x64   (asserted in src/dsp/fpm_pps.c)
 *
 * -- and 0x64 + 0x38 is exactly 0x9c, so the last one closes the block.  Two
 * gaps remain, +0x00..+0x07 and +0x1c..+0x33, and they stay `pad_` because
 * nothing reconstructed touches them.  A struct that guessed at those 32
 * bytes would be worse than the macros it replaced.
 *
 * IT IS CODEGEN-NEUTRAL, MEASURED BOTH WAYS.  `TxNoCarrierV29` compiles to
 * the same 9-byte-different shape against the object before and after this
 * change -- so the modelling neither costs nor buys anything on the codegen
 * tier, and the residual difference is about how `ring` is held, not about
 * how the block is spelled.
 *
 * THE CONTAINING MODEM OBJECT IS STILL NOT MODELLED.  Its size is unknown and
 * one field of it is reached from here, so it keeps a single typed accessor
 * rather than a struct with a speculative tail.
 *
 * ---------------------------------------------------------------------------
 * THE OFFSETS, CONFIRMED TWICE
 *
 * Once by this function and once by `V29TX_create` (addresses into dsplibs.o):
 *
 *   9bd69  sysdep_malloc(0x9c)  -> obj + 0x24     the transmitter's block
 *   9bd81  sysdep_malloc(0x64)  -> fp + 0x08      100 bytes = 50 shorts, the
 *   9bd93  sysdep_malloc(0x64)  -> fp + 0x0c      ring's two RAILS
 *   9bcba  SMC_init             -> fp + 0x34
 *   9bd57  FPM_PPS_init         -> fp + 0x64, from a configuration whose
 *                                  `mapped` is 0 (xor %ebx,%ebx at 9bce8)
 *                                  and whose coefficients are
 *                                  V29TX_PPS_IFILT / V29TX_PPS_QFILT
 *
 * `struct fpm_smc_ring` at fp + 0x08 puts those two allocations at `i` and
 * `q`, the cursor this function advances at `widx` (fp + 0x14) and its bound
 * at `len` (fp + 0x18).  `sym` (fp + 0x10) is never allocated by the
 * constructor and never read by the shaper on this configuration, which is
 * consistent and is why the direct form has to be the one in use.
 */

#ifndef DSPLIB_V29DATA_H
#define DSPLIB_V29DATA_H

#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"

/* The modem object is not modelled; this is the one field reached from here. */
#define V29TX_OBJ_FP		0x24

/*
 * The transmitter's private block, 0x9c bytes from `sysdep_malloc`.  Every
 * offset is confirmed twice, by `TxNoCarrierV29` and by `V29TX_create`.
 */
/*
 * THE OFFSET CONSTANTS SURVIVE THE MODELLING, AND THAT IS NOT LEFTOVER.
 * `test/unit/t_v29data.c` builds both sides of its differential by poking raw
 * bytes at offsets, which is what a fixture over an object the harness
 * allocates itself has to do.  If those pokes were written as
 * `offsetof(struct v29tx, ring)` the test would agree with the struct BY
 * CONSTRUCTION, and a wrong struct would be confirmed by a test that inherited
 * its error.  So the constants stay as an INDEPENDENT statement of the layout,
 * the test uses them, and the assertions at the foot of `v29data.c` check the
 * struct against them rather than the other way round.
 */
#define V29FP_SMC_RING		0x08	/* struct fpm_smc_ring              */
#define V29FP_SMC		0x34	/* struct fpm_smc, SMC_init at 9bcba */
#define V29FP_PPS		0x64	/* struct fpm_pps                    */

struct v29tx {
	unsigned char		pad_00[0x08 - 0x00];
	struct fpm_smc_ring	ring;		/* +0x08  0x14 bytes        */
	unsigned char		pad_1c[0x34 - 0x1c];
	struct fpm_smc		smc;		/* +0x34  SMC_init at 9bcba */
	struct fpm_pps		pps;		/* +0x64  0x38, closes 0x9c */
};

/*
 * The block is reached through the modem object's +0x24, which lives in an
 * object this tree has not modelled.  One accessor, so the cast is in one
 * place and the FRESH re-read the object performs after the shaper stays
 * visible at the call site.
 */
#define V29TX(modem) \
	(*(struct v29tx **)(void *)((unsigned char *)(modem) + V29TX_OBJ_FP))

/*
 * Zero `count` symbol slots on both rails and shape them into `out`,
 * returning the number of samples written.
 *
 * THE SECOND ARGUMENT IS NEVER READ -- nothing at 0x34(%esp) is touched while
 * 0x30, 0x38 and 0x3c are -- so the function takes four arguments and ignores
 * the second, exactly as `TxNoCarrierV17` does.
 *
 * The cursor is written back AFTER the shaper runs, through a fresh read of
 * the instance pointer.  Not observable: `FPM_PPS_filter` writes only `ridx`
 * of the ring it is given.
 */
unsigned short TxNoCarrierV29(void *modem, const unsigned short *data,
			      short *out, unsigned short count);

#endif /* DSPLIB_V29DATA_H */

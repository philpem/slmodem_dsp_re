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
 * (`sysdep_malloc(0x9c)` at 9bd69), and the sub-objects the constructor
 * builds inside it are already-modelled types whose sizes are MEASURED --
 *
 *     struct fpm_sdm        0x18   at +0x1c   (SDM_init at 9bc28)
 *     struct fpm_smc_ring   0x14   at +0x08
 *     struct fpm_smc        0x30   at +0x34   (fpm_smc_cfg 0x2c + two shorts)
 *     struct fpm_pps        0x38   at +0x64   (asserted in src/dsp/fpm_pps.c)
 *
 * -- and 0x64 + 0x38 is exactly 0x9c, so the last one closes the block, and
 * 0x1c + 0x18 is exactly 0x34, `smc`'s own offset, so `sdm` closes what used
 * to be `pad_1c` WITH NO GAP LEFT AT ALL: `V29TX_create` confirmed
 * `V29TX_SDM` (already named in v29fax.h from `ScrambleDataV29`'s own `add
 * $0x1c,%eax`, but not yet folded into this struct) as the `struct fpm_sdm`
 * `SDM_init` builds there, and `struct fpm_sdm` is 0x18 bytes -- `cfg`
 * (0x06) + `pad06` (0x02) + `mask`/`notmask`/`reg` (0x0c) + `shift1`/
 * `shift2` (0x04) -- not the 0x0c a first read of `sdm.h`'s own three-field
 * `struct fpm_sdm_cfg` suggests; `fpm_sdm.h`'s `struct fpm_sdm` carries four
 * more fields past the config `FPM_SDM_init` derives.  ONLY +0x00..+0x07
 * REMAINS UNMODELLED NOW, and stays `pad_` because nothing reconstructed
 * touches it; a struct that guessed at those bytes would be worse than the
 * macro it replaced.  Finding F9701.
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
#include "dsplib/sdm.h"

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

/*
 * The transmit scrambler.  Named already in v29fax.h (`V29TX_SDM`, 0x1c)
 * from `ScrambleDataV29`'s `add $0x1c,%eax`; `V29TX_create`'s own `SDM_init`
 * at 0x9bc28 is the second, independent statement -- `nbits` is `sdm.h`'s
 * "V29TX_create 0x09bc28 nbits = <rate> + 3", `tap1`/`tap2` are the literals
 * 0x12/0x17.  Reached here through the struct field below, at the same
 * offset `V29TX_SDM` names.
 */

struct v29tx {
	unsigned char		pad_00[0x08 - 0x00];
	struct fpm_smc_ring	ring;		/* +0x08  0x14 bytes        */
	struct fpm_sdm		sdm;		/* +0x1c  0x18, SDM_init at
						 * 9bc28 -- V29TX_SDM.  Closes
						 * the gap exactly: 0x1c +
						 * 0x18 = 0x34, `smc`'s own
						 * offset.                   */
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

/*
 * ---------------------------------------------------------------------------
 * The equaliser-training generator, and the OTHER block.
 *
 * `GenEQTrnSequenceV29` reaches the modem object's +0x20 -- not the +0x24
 * transmitter block above -- and touches exactly one short of it, a 7-bit
 * LFSR at +0x18.  That block is otherwise unmodelled, so per this header's
 * own ruling it gets offset constants and a `void *`, not a struct with a
 * speculative tail.
 */
#define V29TX_OBJ_SCRAM		0x20	/* the block holding the LFSR       */
#define V29SCRAM_SR		0x18	/* short: the 7-bit shift register  */

/*
 * Emit `n` symbols of the V.29 equaliser-training sequence: each step
 * feeds back bit0 XOR bit1 into the top of a 7-bit register and answers
 * symbol 0xb for a 1 bit, 0 for a 0 bit.  The register persists in the
 * object, so successive calls continue the sequence.  `n` is read
 * zero-extended from a 16-bit slot, hence its type.
 */
void GenEQTrnSequenceV29(void *modem, unsigned short *out, unsigned short n);

/*
 * ---------------------------------------------------------------------------
 * V29TX_create's own tables and configuration.
 *
 * `struct v29tx_cfg` is the handle's first 28 bytes -- gapless with
 * `V29TX_OBJ_RESULT` at +0x1c, exactly as `v21fax.h`'s `struct v21tx_cfg`
 * abuts `V21TX_OBJ_RESULT` -- copied wholesale by `V29TX_create` from either
 * the caller's `params` or, when NULL, from `V29TX_CFG` below.  Two fields
 * are typed by a callee: `protocol`/`bitrate` are `V29TXS_PROTOCOL`/
 * `V29TXS_BITRATE` (v29fax.h), read back out by `V29TX_status`, and
 * `flags_10` is `V29TXS_FLAGS_10`, tested there as a bit mask.  `int_0008` is
 * 60000 in `V29TX_CFG`, `v21tx_cfg`'s own value at the same offset -- named
 * on that precedent, not re-derived.  The rest are usage inference: nothing
 * reconstructed reads them back.
 */
struct v29tx_cfg {
	short	protocol;	/* +0x00  0                                  */
	short	bitrate;	/* +0x02  9600                               */
	short	short_0004;	/* +0x04  0                                  */
	short	short_0006;	/* +0x06  0                                  */
	int	int_0008;	/* +0x08  60000, as in every sibling table   */
	int	int_000c;	/* +0x0c  1                                  */
	short	flags_10;	/* +0x10  0; V29TXS_FLAGS_10                 */
	short	short_0012;	/* +0x12  0                                  */
	int	int_0014;	/* +0x14  1                                  */
	int	int_0018;	/* +0x18  0                                  */
};

extern struct v29tx_cfg V29TX_CFG;

/*
 * The negotiated bit rate as an index, 0 = 7200 and 1 = 9600 -- the same
 * shape and the same two values as `V29DET_RATE` on the receive side
 * (v29fax.h), and named on that precedent.
 *
 * `V29TX_create` DERIVES IT FROM THE JUST-COPIED CONFIG'S OWN `bitrate`
 * FIELD, the identical three-way compare `V29RX_create` runs for
 * `V29DET_RATE`: `cmp $0x1c20,%edx` (V29_BPS_7200) stores 0, `cmp
 * $0x2580,%edx` (V29_BPS_9600) stores 1, and ANYTHING ELSE ALSO STORES 1 --
 * and on that third arm ALSO raises `V29TX_RESULT_B1_BIT1` and reports
 * `V29TX_STATUS_DEFAULT`, which the two recognised arms do not (0x9bb41
 * through 0x9bb63, 0x9be66, 0x9be71).  `sdm.h`'s own note on `SDM_CFG`,
 * written before `V29TX_create` itself was read, already states
 * "V29TX_create 0x09bc28 nbits = <rate> + 3" -- independently confirmed
 * here, and it is what fixes the descrambler's `nbits` (3 or 4).  It also
 * selects `V29TX_PATTERN_SCR1` and `V29TX_PPS_SCALE` (both below), read by
 * `TxNextStateV29`'s SCR1 arm (0xa4923) and by `V29TX_create` itself
 * (0x9bd15) respectively, and `TxHdxDataV29` (0xa4b4d) reads it once more
 * to choose which status it reports.  `V29TX_control` (0xa4fb0, not
 * reconstructed, outside every wave's scope so far) is the only other
 * reader traced.  Finding F9701.
 */
#define V29TXP_RATE		0x0c	/* short: 0 = 7200, 1 = 9600, per
					 * V29_RATE_7200/V29_RATE_9600 (v29fax.h) */

/* Indexed by V29TXP_RATE.  0x9bd25. */
extern int V29TX_PPS_SCALE[2];

/* Indexed by V29TXP_RATE, read inside TxNextStateV29 itself (0xa4937). */
extern short V29TX_PATTERN_SCR1[2];

/* The transmit pulse shaper's I/Q coefficient tables, 120 shorts each,
 * fed to FPM_PPS_init at 0x9bd57. */
extern const short V29TX_PPS_IFILT[120];
extern const short V29TX_PPS_QFILT[120];

/*
 * The symbol coder's carrier phasor (24 steps) and constellation maps, fed
 * to SMC_init at 0x9bcba.  `smc.h` names the pointer slots these fill;
 * V29TX_create is the "author's own words" it cites for f20/f24's names.
 */
extern const short V29TX_SMC_COSINE[24];
extern const short V29TX_SMC_SINE[24];
extern const short V29TX_SMC_IMAP[16];
extern const short V29TX_SMC_QMAP[16];
/* unsigned short, matching fpm_smc_cfg::pmap's own declared type. */
extern const unsigned short V29TX_SMC_PMAP[8];

#endif /* DSPLIB_V29DATA_H */

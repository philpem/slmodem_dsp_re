/**
 * @file v29data.h
 * @brief ITU-T V.29 (fax): the transmitter's no-carrier leaf.
 *
 * `TxNoCarrierV29` is `TxNoCarrierV17` with one thing changed, and that one
 * thing is the whole point of keeping the two apart: V.29's shaper is
 * configured with `cfg.mapped` clear, so the ring carries I and Q directly
 * and this function zeroes both rails where V.17 writes a constellation
 * index into `sym` (finding F3641). `tools/service.py` puts it on the FAX
 * side.
 *
 * The transmitter's block (`struct v29tx` below) is fully modelled rather
 * than reached through `FIELD()` macros over a `void *` -- the treatment
 * `v22data.h` uses where a field's role isn't established. What makes a
 * block safe to model this way: its size is known from its own allocation
 * (`sysdep_malloc(0x9c)`), and every sub-object the constructor builds
 * inside it is an already-modelled type whose size is independently
 * measured (`struct fpm_sdm` at +0x1c, `struct fpm_smc_ring` at +0x08,
 * `struct fpm_smc` at +0x34, `struct fpm_pps` at +0x64) -- and those four
 * sizes tile the block exactly, with no gap and nothing left over. Only
 * +0x00..+0x07 remains unmodelled, and stays `pad_` because nothing
 * reconstructed touches it. Finding F9701 has the offset arithmetic in
 * full, confirmed independently by both `TxNoCarrierV29` and
 * `V29TX_create`.
 *
 * Modelling the block is codegen-neutral: `TxNoCarrierV29` compiles to the
 * same 9-byte-different shape against the object before and after, so this
 * costs and buys nothing on the codegen tier.
 *
 * The containing modem object is still not modelled -- its size is unknown
 * and only one field of it is reached from here -- so it keeps a single
 * typed accessor rather than a struct with a speculative tail.
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
 *
 * The offset constants below survive the modelling on purpose, and that is
 * not leftover: `test/unit/t_v29data.c` builds both sides of its
 * differential by poking raw bytes at offsets, which is what a fixture over
 * an object the harness allocates itself has to do. If those pokes were
 * written as `offsetof(struct v29tx, ring)` the test would agree with the
 * struct by construction, and a wrong struct would be confirmed by a test
 * that inherited its error. So the constants stay as an independent
 * statement of the layout, the test uses them, and the assertions at the
 * foot of `v29data.c` check the struct against them rather than the other
 * way round.
 */
#define V29FP_SMC_RING		0x08	/* struct fpm_smc_ring              */
#define V29FP_SMC		0x34	/* struct fpm_smc, SMC_init at 9bcba */
#define V29FP_PPS		0x64	/* struct fpm_pps                    */

/*
 * The transmit scrambler, `V29TX_SDM`.  Named in v29fax.h from
 * `ScrambleDataV29`'s own reference; `V29TX_create`'s `SDM_init` call is the
 * independent second statement, and fixes `nbits`/`tap1`/`tap2` (`sdm.h`
 * has the derivation).  Reached here through the struct field below, at the
 * same offset.
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

/**
 * @brief Emit `count` no-carrier samples on the V.29 transmit shaper.
 *
 * Zeroes `count` symbol slots on both of the ring's rails (V.29's shaper
 * runs unmapped, so this is how the modulation says "no symbol" -- see the
 * file comment) and shapes them into `out`. The cursor is written back
 * after the shaper runs, through a fresh read of the instance pointer.
 *
 * @param modem  The V.29 modem object.
 * @param data   Never read; declared only so this matches `TxNoCarrierV17`'s
 *               signature.
 * @param out    Destination for the shaped samples.
 * @param count  Number of symbol slots to fill.
 * @return The number of samples written.
 */
unsigned short TxNoCarrierV29(void *modem, const unsigned short *data,
			      short *out, unsigned short count);

/*
 * The equaliser-training generator reaches the modem object's +0x20 --
 * a different, otherwise-unmodelled block from the +0x24 transmitter block
 * above -- and touches exactly one short of it, a 7-bit LFSR at +0x18.
 */
#define V29TX_OBJ_SCRAM		0x20	/* the block holding the LFSR       */
#define V29SCRAM_SR		0x18	/* short: the 7-bit shift register  */

/**
 * @brief Emit `n` symbols of the V.29 equaliser-training sequence.
 *
 * Each step feeds a 7-bit LFSR back with bit0 XOR bit1 and answers
 * constellation index 0xb for a 1 bit it produced, 0 for a 0 bit. The
 * register persists in the object, so successive calls continue the
 * sequence.
 *
 * @param modem  The V.29 modem object.
 * @param out    Destination for the `n` constellation indices.
 * @param n      Number of symbols to emit.
 */
void GenEQTrnSequenceV29(void *modem, unsigned short *out, unsigned short n);

/*
 * `V29TX_create`'s own tables and configuration.
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
 *
 * `int_0008` gets a second writer: `V29TX_control` (`v29fax.h`) copies its
 * own request's `+0x04` straight into this field, independently of the
 * constructor's whole-struct copy -- the same shape `V29RX_control` gave
 * `V29_OBJ_INT_0008` (finding F10103).  Still nothing reconstructed reads it
 * back.  Finding F10107.
 *
 * `flags` (this wave, was `flags_10`) IS RANK 2, not usage inference: it is
 * exactly `v29fax.h`'s own `V29TXS_FLAGS_10`, "tested there as a bit mask"
 * per this file's own note above, and `V29TX_control`'s request sets its
 * bit 2 independently of the constructor's copy (`v29fax.h`'s
 * `V29TXCTL_CTL0_BIT2` comment).  The same "genuine flags word" case
 * `struct v27tx_cfg::flags` is this wave, one modulation over.
 *
 * `fifo_size_factor` (this wave, was `int_0014`) CORRECTS A STALE FILE
 * COMMENT, not merely a twin-class import: this struct's own header used to
 * say "the rest are usage inference: nothing reconstructed reads them
 * back", but `V29TX_create` (`src/fax/v29.c`) reads this field directly --
 * `n = ((struct v29tx_cfg *)modem)->int_0014` -- to size the transmit FIFO
 * at `n * 3 * 16`, matching the object's own comment there ("THE TRANSMIT
 * FIFO'S SIZE IS COMPUTED, NOT A LITERAL -- int_0014 * 3 * 16").  That is
 * `v17fax.h`'s own `struct v17tx_cfg::fifo_size_factor` role and default
 * (1, "* 3 * 16") exactly, and `v17fax.h`'s own field comment (finding
 * F10144) already named V.29's copy "a candidate for whoever visits it" --
 * this wave does.  `struct v27tx_cfg::fifo_size_factor` is the third
 * sibling, renamed the same wave on the same evidence (though V.27ter's own
 * multiplier is `V27TX_FRMSIZE[rate]` rather than the fixed 3*16, so the
 * SHAPE matches and the constant does not).
 */
struct v29tx_cfg {
	short	protocol;	/* +0x00  0                                  */
	short	bitrate;	/* +0x02  9600                               */
	short	short_0004;	/* +0x04  0                                  */
	short	short_0006;	/* +0x06  0                                  */
	int	int_0008;	/* +0x08  60000, as in every sibling table   */
	int	int_000c;	/* +0x0c  1                                  */
	short	flags;		/* +0x10  0; V29TXS_FLAGS_10                 */
	short	short_0012;	/* +0x12  0                                  */
	int	fifo_size_factor; /* +0x14  1 -> FIFO capacity, * 3 * 16,
					  `v17tx_cfg`'s own field, same name */
	int	int_0018;	/* +0x18  0                                  */
};

extern struct v29tx_cfg V29TX_CFG;

/*
 * The negotiated bit rate as an index, 0 = 7200 and 1 = 9600 -- the same
 * shape and the same two values as `V29DET_RATE` on the receive side
 * (v29fax.h), and named on that precedent.
 *
 * `V29TX_create` derives it from the just-copied config's own `bitrate`
 * field, the identical three-way compare `V29RX_create` runs for
 * `V29DET_RATE`: 7200 stores 0, 9600 stores 1, and anything else also
 * stores 1 but additionally raises `V29TX_RESULT_B1_BIT1` and reports
 * `V29TX_STATUS_DEFAULT`, which the two recognised rates do not.  It fixes
 * the descrambler's `nbits` (3 or 4, `sdm.h`), and selects
 * `V29TX_PATTERN_SCR1` and `V29TX_PPS_SCALE` (both below), read respectively
 * by `TxNextStateV29`'s SCR1 arm and by `V29TX_create` itself; `TxHdxDataV29`
 * reads it once more to choose which status it reports.  `V29TX_control`
 * is the only other reader traced.  Finding F9701.
 */
#define V29TXP_RATE		0x0c	/* short: 0 = 7200, 1 = 9600, per
					 * V29_RATE_7200/V29_RATE_9600 (v29fax.h) */

/* Indexed by V29TXP_RATE. */
extern int V29TX_PPS_SCALE[2];

/* Indexed by V29TXP_RATE, read inside TxNextStateV29 itself. */
extern short V29TX_PATTERN_SCR1[2];

/* The transmit pulse shaper's I/Q coefficient tables, 120 shorts each,
 * fed to FPM_PPS_init. */
extern const short V29TX_PPS_IFILT[120];
extern const short V29TX_PPS_QFILT[120];

/*
 * The symbol coder's carrier phasor (24 steps) and constellation maps, fed
 * to SMC_init.  `smc.h` names the pointer slots these fill; `V29TX_create`
 * is the "author's own words" it cites for f20/f24's names.
 */
extern const short V29TX_SMC_COSINE[24];
extern const short V29TX_SMC_SINE[24];
extern const short V29TX_SMC_IMAP[16];
extern const short V29TX_SMC_QMAP[16];
/* unsigned short, matching fpm_smc_cfg::pmap's own declared type. */
extern const unsigned short V29TX_SMC_PMAP[8];

#endif /* DSPLIB_V29DATA_H */

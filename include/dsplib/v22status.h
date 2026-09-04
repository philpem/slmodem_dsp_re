/*
 * v22status.h -- V.22 / V.22bis: the connection status report.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V22_status  .text 0x08c450  310 bytes
 *   PROTOCOL    .rodata 0x08c0c  14 bytes, LOCAL
 *
 * `V22_status` fills a caller-owned block with what the datapump currently
 * is: which protocol it settled on, the two bit rates, a quality number, and
 * eight flag bits that are the inverse-or-not of five of the datapump's own
 * enable words.  It returns a literal 1 on every path.
 *
 * ---------------------------------------------------------------------------
 * `PROTOCOL` IS A SEVEN-ENTRY TABLE AND ITS VALUES ARE NOT DECODED
 *
 * The object indexes it with `hdx->protocol` -- sign-extended, and with NO BOUNDS
 * CHECK -- and stores the entry as the report's first word.
 *
 * WHAT THE INDEX IS, IS SETTLED.  `hdx->protocol` is what `V22FP_modem` indexes
 * `V22_PROTOCOL` with, and that table's seven entries carry RELOCATIONS
 * naming the seven V.22 protocol handlers, so the index is the protocol state
 * and the object tells us which state each value is (finding F8529):
 *
 *     index    0             1              2            3
 *     handler  v22_data      v22_originate  v22_answer   v22_local_loop
 *     value    3             0              1            2
 *
 *     index    4                5                6
 *     handler  v22_org_rmloop2  v22_ans_rmloop2  v22_retrain
 *     value    7                8                5
 *
 * WHAT THE VALUES MEAN IS STILL NOT ESTABLISHED.  slmodemd's `modem_defs.h`
 * -- the vendored copy in third_party/slmodem -- has no enumeration whose
 * members are 0, 1, 2, 3, 5, 7 and 8, and there is no format string anywhere
 * that prints one.  They are carried as the object's own numbers, and the
 * table above is the mapping they are a mapping FROM, not a decoding of them.
 *
 * The symbol is LOCAL in the object (`nm` shows a lower-case `r`), so it is
 * `static` here.  There is a SECOND local symbol also called `PROTOCOL`, 18
 * bytes in `.data` at 0x7768, belonging to some other translation unit; the
 * two are unrelated and only the `.rodata` one is this file's.
 *
 * ---------------------------------------------------------------------------
 * THE FLAG BYTE IS WRITTEN SEVEN TIMES, ONE BIT AT A TIME
 *
 * The object loads +0x14 once, then masks one bit out and ORs one bit in and
 * STORES THE WHOLE BYTE, seven times over.  That is what a run of bitfield
 * assignments compiles to, and it is the pattern CLAUDE.md says to settle per
 * site rather than by preference.
 *
 * It is still written here with explicit masks, and the reason is that the
 * masks reproduce the same instruction shape -- an `and $imm8` and an `or` per
 * bit -- WITHOUT claiming a packing order for a struct the host owns.  A
 * bitfield declaration would additionally assert that the author's bit 0 is
 * the byte's least significant bit; that happens to be true for GCC on x86 and
 * it is not something this object can tell us.
 *
 * All eight bits of +0x14 are written, so the caller's value there does not
 * survive.  Only bit 0 of +0x15 is, so the caller's other seven bits DO.
 */

#ifndef DSPLIB_V22STATUS_H
#define DSPLIB_V22STATUS_H

struct v22fp;

/*
 * The report block.  At least 0x16 bytes; the object writes ten fields and
 * reads two of them back, and touches nothing above +0x15.  +0x0c and +0x0e
 * are not written at all and are left unmodelled rather than named.
 */
struct v22_status {
	short protocol;		/* +0x00 PROTOCOL[hdx->protocol]                */
	short tx_bps;		/* +0x02 1200, or 2400 when dsp->r28       */
	short rx_bps;		/* +0x04 1200, or 2400 when dsp->r2a       */
	short quality;		/* +0x06 see V22_STATUS_QUALITY_* below    */
	short short_08;		/* +0x08 written 0, read by nothing        */
	short short_0a;		/* +0x0a written 0, read by nothing        */
	unsigned char unmapped_000c[0x10 - 0x0c];
	short short_10;		/* +0x10 written 0, read by nothing        */
	short short_12;		/* +0x12 written 0, read by nothing        */
	unsigned char flags;	/* +0x14 all eight bits written            */
	unsigned char flags2;	/* +0x15 bit 0 written, the rest preserved */
};

/* The two rates the report can carry, in bit/s. */
#define V22_STATUS_BPS_1200	1200
#define V22_STATUS_BPS_2400	2400

/*
 * The quality number counts DOWN from 2048 as the equaliser's mean-square
 * error grows -- `0x800 - err`, where `err` comes from `fse.mse` at +0x22,
 * the same field `v22prc.h`'s `GetSignalQuality` returns, read here as a
 * SIGNED short.  There is no clamp: the report goes negative.
 *
 * THE Q14 SCALE IS APPLIED ON THE 1200 ARM ONLY.  When `dsp->r2a` says 2400,
 * the object jumps into the middle of the 1200 path with the RAW mse in the
 * register the scaled value would have been in, so `err` is `mse` unscaled
 * and the report falls three times as fast.  That asymmetry is easy to miss
 * -- the two arms share the subtraction, the store and everything after it,
 * and only the four instructions before the join differ -- and a sweep that
 * left `r2a` at whatever the constructor set would never see it.  It is the
 * object's; finding F8528.
 *
 * 0x143c/16384 is 0.31616, so the 1200 report reaches zero at an mse of 6478
 * and the 2400 report at 2048.
 */
#define V22_STATUS_QUALITY_BASE		0x800
#define V22_STATUS_QUALITY_SCALE_Q14	0x143c

/*
 * +0x14.  Bits 0 to 2 report three of the datapump's enable words directly;
 * bits 3 to 5 report three more INVERTED, i.e. they are set when the thing is
 * off.  Bit 6 is set unconditionally.  Bit 7 comes from the parameter block.
 *
 * `r18` and `r1c` are named here for what `ScramblerOn` and `DescramblerOn`
 * return (finding F8526); `eq_adapt` is v22fp.h's own name.  `r20` and `r0c`
 * are not established and keep their offsets.
 */
#define V22_STATUS_SCRAMBLER	(1 << 0)	/* dsp->scrambler_on bit 0           */
#define V22_STATUS_DESCRAMBLER	(1 << 1)	/* dsp->descrambler_on bit 0           */
#define V22_STATUS_R20		(1 << 2)	/* dsp->r20 bit 0           */
#define V22_STATUS_R00_OFF	(1 << 3)	/* set when dsp->r00 == 0   */
#define V22_STATUS_R0C_OFF	(1 << 4)	/* set when dsp->r0c == 0   */
#define V22_STATUS_EQ_FROZEN	(1 << 5)	/* set when eq_adapt == 0   */
#define V22_STATUS_ALWAYS	(1 << 6)	/* unconditional            */
#define V22_STATUS_PARAM_BIT9	(1 << 7)	/* params.flags bit 9       */

/* +0x15.  One bit, from the parameter block; the other seven are untouched. */
#define V22_STATUS2_PARAM_BIT10	(1 << 0)

/* The two parameter-block bits the report copies out. */
#define V22_PARAMS_BIT9		(1u << 9)
#define V22_PARAMS_BIT10	(1u << 10)

/**
 * @brief Fill a V.22 connection status report from the datapump instance.
 * @param fp  The V.22 datapump instance.
 * @param st  Output: the status report.
 * @return Always 1 -- a literal on every path, not a status.
 */
int V22_status(struct v22fp *fp, struct v22_status *st);

#endif /* DSPLIB_V22STATUS_H */

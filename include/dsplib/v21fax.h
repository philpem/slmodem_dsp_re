/*
 * v21fax.h -- ITU-T V.21 (the fax control channel): the receive and transmit
 * primitives.
 *
 * V.21 in this object is the 300 baud FSK channel T.30 signals over, and it
 * is built out of the same `FPM_*` blocks Bell 103 is: an FSK modulator
 * (`struct fpm_fsm`) and a rate converter (`struct fpm_mrf`) on the way out,
 * a rate converter and an FSK demodulator (`struct fpm_fsd`) on the way in.
 * `tools/service.py` puts all of it on the FAX side.
 *
 * ---------------------------------------------------------------------------
 * THE TRANSMITTER AND THE RECEIVER ARE TWO OBJECTS, NOT ONE
 *
 * The object has `V21TX_create` (0x0992f0) and `V21RX_create` (0x098e70) as
 * separate constructors with separate deletes, and `faxvmi.h` records that
 * FAXVMI gives them separate slots (5 v21tx, 6 v21rx).  So an offset read by
 * `ModDataV21` and an offset read by `V21RX_modem` are offsets in DIFFERENT
 * blocks and must not be collected into one struct.  They are kept apart
 * here, and the two handles are `void *`.
 *
 * NEITHER CONSTRUCTOR IS RECONSTRUCTED, so neither object is modelled.  This
 * header follows the ruling `include/dsplib/v17data.h` sets out: the handle
 * is `void *` and every offset into it is a named constant with the evidence
 * beside it.  What IS modelled is the two DSP sub-blocks the handles point
 * at, because every field in them is forced by the type of the callee it is
 * handed to -- which is CLAUDE.md's second-strongest class of evidence, and
 * is how `b103fp.h` came by the same layout for Bell 103.
 *
 * ---------------------------------------------------------------------------
 * WHERE EACH OFFSET COMES FROM
 *
 * Transmitter (addresses are into dsplibs.o):
 *
 *   a5895  ModDataV21      tx + 0x24 -> the transmit DSP block
 *   a58a6  ModDataV21      FPM_FSM_modulate(dsp + 0x00, ...) types dsp + 0
 *   a58ca  ModDataV21      FPM_MRF_filter(dsp + 0x10, ...) types dsp + 0x10
 *   a5898  ModDataV21      dsp + 0x2c is passed as FPM_FSM_modulate's output
 *                          AND as FPM_MRF_filter's input: one shared buffer
 *   a58f1  TxNoCarrierV21  the short at dsp + 0x06, which is `fpm_fsm`'s
 *                          `cfg.scale` -- saved, zeroed and restored
 *
 * `struct fpm_fsm` is 0x10 bytes and `struct fpm_mrf` is 0x1c, so 0x00, 0x10
 * and 0x2c abut with no gap: the block is exactly those three things.
 *
 * Receiver:
 *
 *   a1c5d  V21RX_modem     the flag byte at rx + 0x19
 *   a1c77  V21RX_modem     rx + 0x4c -> the half-duplex context
 *   a1c89  V21RX_modem     hdx + 0x04 is called (rx, in, out, count)
 *   a1cb4  V21RX_modem     the 32-bit word at rx + 0x18 is the return
 *   a5824  CarrierDetectV21, GetSNRV21, V21RX_delete
 *                          rx + 0x50 -> the receive DSP block
 *   99284  V21RX_delete    FPM_MTD_delete(dsp + 0x8c) types that pointer
 *   9929b  V21RX_delete    FPM_FSD_free(dsp + 0x54) types dsp + 0x54
 *   992b2  V21RX_delete    FPM_MRF_free(dsp + 0x38) types dsp + 0x38
 *   992c3  V21RX_delete    sysdep_free(dsp + 0x90): a block the dsp owns
 *   a583c  GetSNRV21       dsp + 0x70 and dsp + 0x74, which are `fsd.trace`
 *                          and `fsd.last_count` under the 0x54 above -- two
 *                          readings that agree without either being derived
 *                          from the other
 *
 * `struct fpm_mrf` is 0x1c and `struct fpm_fsd` is 0x38, so 0x38, 0x54, 0x8c
 * and 0x90 abut too, and the only unmodelled span is 0x0c..0x37.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS NOT ESTABLISHED, AND IS THEREFORE NOT NAMED
 *
 * `dsp + 0x04` and `dsp + 0x08` are the two ints `CarrierDetectV21` ANDs.
 * `struct b103_dsp` has `rx_energy` and `rx_tone` at exactly those offsets
 * and `CarrierDetectB103` is the same two-load AND -- but that is a PARALLEL
 * in a different datapump, not a measurement here, and nothing reconstructed
 * writes either field.  They keep their offsets.  The same goes for the
 * 0x2c-byte gap at dsp + 0x0c, which is the right size for a `struct
 * fpm_agc` and is left as unmodelled space rather than named on that basis.
 */

#ifndef DSPLIB_V21FAX_H
#define DSPLIB_V21FAX_H

#include "dsplib/fpm_fsd.h"
#include "dsplib/fpm_fsm.h"
#include "dsplib/fpm_mrf.h"

struct fpm_mtd;

/* ------------------------------------------------------------------------ */
/* The transmitter                                                          */

/*
 * The transmit DSP block, at V21TX_OBJ_DSP of the transmitter handle.
 *
 * The modulator writes into `scratch` at its own sample rate and the rate
 * converter lifts that into the caller's buffer, which is Bell 103's
 * arrangement exactly (`b103fp.h`, findings F17 and F24).
 */
struct v21_tx_dsp {
	struct fpm_fsm	fsm;		/* +0x00 the FSK modulator          */
	struct fpm_mrf	mrf;		/* +0x10 modulator rate -> 8 kHz    */
	short		*scratch;	/* +0x2c the shared intermediate    */
};

/* The transmit DSP block's home in the transmitter handle. */
#define V21TX_OBJ_DSP		0x24

#define V21TX_DSP(m) \
	(*(struct v21_tx_dsp **)(void *)((char *)(m) + V21TX_OBJ_DSP))

/* ------------------------------------------------------------------------ */
/* The receiver                                                             */

/*
 * The receive DSP block, at V21RX_OBJ_DSP of the receiver handle.
 */
struct v21_rx_dsp {
	int		int_0000;	/* +0x00 read by nothing traced     */
	int		int_0004;	/* +0x04 CarrierDetectV21, and see
					 *       the header note above      */
	int		int_0008;	/* +0x08 CarrierDetectV21           */
	unsigned char	unmapped_000c[0x2c];
					/* +0x0c                            */
	struct fpm_mrf	mrf;		/* +0x38 8 kHz -> demodulator rate  */
	struct fpm_fsd	fsd;		/* +0x54 the FSK demodulator        */
	struct fpm_mtd	*mtd;		/* +0x8c the multi-tone detector    */
	short		*mag;		/* +0x90 fsd.last_count entries;
					 *       GetSNRV21 fills it         */
};

/*
 * The half-duplex context, at V21RX_OBJ_HDX of the receiver handle.
 *
 * Only the one slot is reached from here.  `RxNextStateV21` (0x0a1d60) is
 * what installs the handlers, and it is not reconstructed, so nothing else
 * in this block is modelled.
 */
struct v21_rx_hdx {
	unsigned char	unmapped_0000[4];
					/* +0x00                            */
	short		(*handler)(void *rx, short *in, short *out,
				   short *count);
					/* +0x04 the current receive state  */
};

/*
 * Offsets in the receiver handle.
 *
 * +0x18 is read as one 32-bit word and returned; +0x19 is written as a byte.
 * That is `b103fp.h`'s +0x1c/+0x1d shape -- a status byte with a flags byte
 * above it, read out together -- and it is why the word is spelled as a
 * `memcpy` in the source rather than as a cast.
 */
#define V21RX_OBJ_STATUS	0x18
#define V21RX_OBJ_FLAGS		0x19
#define V21RX_OBJ_HDX		0x4c
#define V21RX_OBJ_DSP		0x50

#define V21RX_DSP(m) \
	(*(struct v21_rx_dsp **)(void *)((char *)(m) + V21RX_OBJ_DSP))
#define V21RX_HDX(m) \
	(*(struct v21_rx_hdx **)(void *)((char *)(m) + V21RX_OBJ_HDX))
#define V21RX_FLAGS(m) \
	(*(unsigned char *)((char *)(m) + V21RX_OBJ_FLAGS))
#define V21RX_STATUS_AT(m) \
	((const void *)((const char *)(m) + V21RX_OBJ_STATUS))

/*
 * Bit 1 of the receiver's flags byte.
 *
 * MEASURED, not inferred from Bell 103's identically placed one-shot.  Every
 * instruction in the object that writes rx + 0x19 was enumerated: the only
 * SET of this bit is `orb $0x2` at 0x0a1ccc inside `RxHdxErrorV21`, and the
 * only CLEAR is `V21RX_modem`'s at the top of every block.  So it is an
 * error event a caller must read each block or lose.  (Bits 0x01, 0x20 and
 * 0x80 are also live in that byte -- 0x80 is what `V21RX_status` tests --
 * and they are not this file's to name.)
 */
#define V21RX_FLAG_ERROR	(1 << 1)

/* ------------------------------------------------------------------------ */
/* The status report                                                        */

/*
 * What `V21TX_status` fills in.
 *
 * THE LAYOUT IS NOT THIS FILE'S DISCOVERY.  It is the block `V22_status`
 * (`v22status.h`) and `V32FP_status` (`v32fpstat.h`) already fill, field for
 * field: a protocol word, two rates in bit/s, a quality word, two flag bytes
 * at +0x14 and +0x15, and an int at +0x18.  This is a THIRD definition of one
 * host-facing type and it should become one; unifying the three is deliberately
 * NOT done here, because the other two are other files' and three agents were
 * writing this span at once.  See finding F8886.
 *
 * `V21TX_status` writes +0x00 through +0x0c, +0x10, +0x12, +0x14 and +0x15,
 * and touches neither +0x0e nor +0x16 nor +0x18.
 */
struct v21_status {
	short		protocol;	/* +0x00 <- the transmitter's +0x00 */
	short		tx_bps;		/* +0x02 always V21_STATUS_BPS      */
	short		rx_bps;		/* +0x04 written 0                  */
	short		quality;	/* +0x06 written 0                  */
	short		snr;		/* +0x08 written 0                  */
	short		short_0a;	/* +0x0a written 0                  */
	short		short_0c;	/* +0x0c written 0                  */
	short		short_0e;	/* +0x0e NOT written                */
	short		short_10;	/* +0x10 written 0                  */
	short		short_12;	/* +0x12 written 0                  */
	unsigned char	flags;		/* +0x14 see below                  */
	unsigned char	flags1;		/* +0x15 bit 0 cleared, rest kept   */
	short		short_16;	/* +0x16 NOT written                */
	int		int_18;		/* +0x18 NOT written                */
};

/* The only rate V.21 has, in bit/s, and the object's own literal 0x12c. */
#define V21_STATUS_BPS		300

/*
 * The two bits `V21TX_status` clears in `flags`, and the one it clears in
 * `flags1`.  They are named by BIT VALUE and by nothing else: the object
 * clears them and no reconstructed code reads them, so their meanings are
 * not established.  In `struct v22_status`'s numbering the same positions
 * are the scrambler and descrambler bits; that is a coincidence of layout,
 * not evidence about V.21, and it is not asserted here.
 */
#define V21_STATUS_BIT0		(1 << 0)
#define V21_STATUS_BIT1		(1 << 1)
#define V21_STATUS1_BIT0	(1 << 0)

/*
 * The one bit the report actually carries, copied out of the byte at +0x10
 * of the transmitter handle.  See D1037 for what the object does to the rest
 * of the byte on the way.
 */
#define V21_STATUS_BIT2		(1 << 2)

/*
 * Offsets in the TRANSMITTER handle that `V21TX_status` reads.
 *
 * +0x00 is loaded `movzwl` and only `%ax` is used, so its SIGNEDNESS IS FREE
 * (finding F614); it is spelled `unsigned short` here to match the load and
 * narrowed at the store, which is what the object does.
 */
#define V21TX_OBJ_PROTOCOL	0x00	/* unsigned short */
#define V21TX_OBJ_FLAGS		0x10	/* unsigned char  */

#define V21TX_PROTOCOL(m) \
	(*(unsigned short *)(void *)((char *)(m) + V21TX_OBJ_PROTOCOL))
#define V21TX_FLAGS(m) \
	(*(unsigned char *)((char *)(m) + V21TX_OBJ_FLAGS))

/* ------------------------------------------------------------------------ */
/* The functions                                                            */

/*
 * Modulate `nbits` bits into `out` and return the number of samples written.
 *
 * Two stages, as Bell 103's `ModDataB103` is: the modulator fills the shared
 * scratch buffer at its own rate and the converter lifts that into `out`, so
 * the bit count and the sample count are different numbers and the return is
 * the second one.  Nothing here bounds `nbits` against the scratch buffer and
 * the object has no guard either.
 *
 * THE HANDLE IS RE-READ after the modulator returns -- `mov 0x24(%ebx),%eax`
 * at 0xa58ba, not a reload of a cached local -- which is the idiom
 * `v17data.h` records for `ModDataV17` and `v22data.c` for `ModDataV22`.  It
 * is not observable through any call this function can make.
 *
 * RETURNS `unsigned short`: the object zero-extends the converter's return
 * with `movzwl %ax,%eax` before the epilogue, and `FPM_MRF_filter` returns
 * `short`, so the narrowing is the author's and the extension is the ABI's.
 */
unsigned short ModDataV21(void *modem, const unsigned short *bits, short *out,
			  unsigned short nbits);

/*
 * The same with the carrier off: identical timing, identical sample count,
 * silence.
 *
 * The modulator's output scale is forced to zero across the modulate call and
 * restored afterwards, so the phase accumulator, the symbol counter and the
 * converter history all advance exactly as they would have.  The converter
 * runs with whatever scale it was given, which by then is silence anyway.
 *
 * The second argument IS read, unlike `TxNoCarrierV17`'s: it is handed
 * straight to the modulator, which is still asked to modulate real bits.
 */
unsigned short TxNoCarrierV21(void *modem, const unsigned short *bits,
			      short *out, unsigned short nbits);

/* Carrier present: the receive block's two words at once. */
int CarrierDetectV21(void *modem);

/*
 * Rectify the demodulator's trace into the block's own buffer, and return 0.
 *
 * `fsd.trace` is "the lowpass output, one word per input sample" (fpm_fsd.h)
 * and `fsd.last_count` is how many of them the last call wrote, so this walks
 * exactly the samples the demodulator just produced and stores their absolute
 * values into `mag`.
 *
 * IT RETURNS A LITERAL 0 AND COMPUTES NO RATIO.  The object then runs a
 * SECOND loop over the same bound with an empty body; the natural reading is
 * an accumulation whose result became dead, but the object does not say so
 * and nothing here claims it.  The loop is reproduced because it is in the
 * object; it has no observable effect.  See D1038.
 *
 * The absolute value is the branchless `cltd; xor; sub` form, so an input of
 * -32768 comes back as -32768.  That is not a bug being introduced here --
 * it is what the object computes -- and the differential test drives it.
 */
int GetSNRV21(void *modem);

/*
 * Fill `st` from the transmitter handle.  Returns 1, or 0 for a NULL `st`.
 */
int V21TX_status(void *modem, struct v21_status *st);

/*
 * Run the receiver over one block.
 *
 * `count` is in-out and CHANGES UNITS: it goes in as the number of input
 * samples available and comes back as the number of output units produced.
 * That is the same in-out convention `B103FP_modem` uses for `n_rx`, and it
 * is the shape finding F8607 / deviation D956 is about -- there is no clamp
 * anywhere, so a caller's output buffer must be sized from what the state
 * handlers can produce and not from the input count.
 *
 * The half-duplex handler at `hdx->handler` is called repeatedly with the
 * cursors advanced -- the input by however many samples the handler consumed
 * (which it reports by decrementing `*count`), the output by however many
 * units it returned -- until `*count` reaches zero.  IT IS A DO-WHILE: a
 * handler is dispatched even when `*count` is zero on entry.
 *
 * The running total is truncated to a `short` on every iteration, so a block
 * that produces more than 32,767 units wraps.
 *
 * Returns the 32-bit word at V21RX_OBJ_STATUS -- status in the low byte,
 * flags in the next.
 */
int V21RX_modem(void *modem, short *in, short *out, short *count);

/*
 * Tear the receiver down.
 *
 * Frees the demodulator's and converter's buffers, the tone detector, the
 * magnitude buffer, the DSP block, the half-duplex context and the handle,
 * in that order, with NO NULL GUARD anywhere and no check that the caller
 * supplied the handle rather than the constructor.  See D1039.
 */
void V21RX_delete(void *modem);

#endif /* DSPLIB_V21FAX_H */

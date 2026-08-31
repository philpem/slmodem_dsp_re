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
 * and 0x90 abut too, and the block is now gapless end to end:
 *
 *   a576a  DemodDataV21    FPM_AGC_agc(dsp + 0x0c, ...) types dsp + 0x0c,
 *                          and `sizeof(struct fpm_agc)` is 0x2c, which is
 *                          exactly the span that was unmodelled.  Finding F8894.
 *   a57d2  DemodDataV21    dsp + 0x90 is FPM_MRF_filter's OUTPUT and then
 *                          FPM_FSD_demodulate's INPUT, so `mag` is a shared
 *                          intermediate and not only GetSNRV21's destination
 *   a579d  DemodDataV21    rx + 0x4c -> the half-duplex context, whose
 *                          handler slot is compared against RxHdxDataV21
 *
 * ---------------------------------------------------------------------------
 * WHAT IS NOT ESTABLISHED, AND IS THEREFORE NOT NAMED
 *
 * `dsp + 0x04` and `dsp + 0x08` are the two ints `CarrierDetectV21` ANDs, and
 * `DemodDataV21` is what writes them.  What it writes is now known exactly --
 * +0x04 takes the value `FPM_AGC_agc` leaves behind, which is `agc.signal`
 * (the same reading `src/pump/v23/bwchdem.c` takes at its own call site), and
 * +0x08 is 1 unless `FPM_MTD_detect` returned something other than
 * `FPM_MTD_ABSENT`, in which case 0.  `struct b103_dsp` has `rx_energy` and
 * `rx_tone` at exactly those offsets, and B.103's `rx_energy` is literally
 * `dsp->agc.signal` too -- but B.103's `rx_tone` is the OPPOSITE polarity to
 * this field, so the parallel names one of the pair and mis-names the other.
 * They keep their neutral names until the pair can be named together; the
 * derivation above is the record.  See finding F8895.
 *
 * `hdx + 0x00` is an int `RxHdxDataV21` tests and the transition into the
 * IDLE state clears.  Nothing reconstructed sets it, so it is not named.
 */

#ifndef DSPLIB_V21FAX_H
#define DSPLIB_V21FAX_H

#include "dsplib/fpm_agc.h"
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
	struct fpm_agc	agc;		/* +0x0c DemodDataV21 hands this to
					 *       FPM_AGC_agc; sizeof is 0x2c
					 *       and the span was 0x2c      */
	struct fpm_mrf	mrf;		/* +0x38 8 kHz -> demodulator rate  */
	struct fpm_fsd	fsd;		/* +0x54 the FSK demodulator        */
	struct fpm_mtd	*mtd;		/* +0x8c the multi-tone detector    */
	short		*mag;		/* +0x90 the shared intermediate:
					 *       FPM_MRF_filter writes it and
					 *       FPM_FSD_demodulate reads it,
					 *       and GetSNRV21 rectifies the
					 *       fsd's trace into it        */
};

/*
 * The half-duplex context, at V21RX_OBJ_HDX of the receiver handle.
 *
 * `state` and `countdown` come from the state-advance block that the object
 * carries once out of line as `RxNextStateV21` (0x0a1d60) and three more
 * times inlined, in `RxHdxStartV21`, `RxHdxWaitV21` and `RxHdxDataV21`.  All
 * four copies switch on the sign-extended short at +0x08 and write the
 * handler at +0x04 beside it, which is what pairs the two.
 *
 * +0x0c and +0x0e are two more shorts, counted up by `RxHdxStartV21` alone;
 * that function is not reconstructed and they are not modelled here.
 */
struct v21_rx_hdx {
	int		int_0000;	/* +0x00 RxHdxDataV21 refuses to
					 *       demodulate while this is
					 *       non-zero; the transition into
					 *       the IDLE state clears it.
					 *       Nothing written sets it.    */
	short		(*handler)(void *rx, short *in, short *out,
				   short *count);
					/* +0x04 the current receive state  */
	short		state;		/* +0x08 V21RX_STATE_*              */
	unsigned short	countdown;	/* +0x0a blocks left in this state;
					 *       only RxHdxWaitV21 counts it
					 *       down.  Loaded `movzwl` and
					 *       tested `jle` on the low 16
					 *       bits, so the two readings
					 *       agree over the whole range */
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
#define V21RX_OBJ_FLAGS1	0x1a
#define V21RX_OBJ_HDX		0x4c
#define V21RX_OBJ_DSP		0x50

#define V21RX_DSP(m) \
	(*(struct v21_rx_dsp **)(void *)((char *)(m) + V21RX_OBJ_DSP))
#define V21RX_HDX(m) \
	(*(struct v21_rx_hdx **)(void *)((char *)(m) + V21RX_OBJ_HDX))
#define V21RX_STATUS(m) \
	(*(unsigned char *)((char *)(m) + V21RX_OBJ_STATUS))
#define V21RX_FLAGS(m) \
	(*(unsigned char *)((char *)(m) + V21RX_OBJ_FLAGS))
#define V21RX_FLAGS1(m) \
	(*(unsigned char *)((char *)(m) + V21RX_OBJ_FLAGS1))
#define V21RX_STATUS_AT(m) \
	((const void *)((const char *)(m) + V21RX_OBJ_STATUS))

/*
 * The receiver's flags byte, rx + 0x19.
 *
 * EVERY BIT BELOW IS NAMED FROM AN ENUMERATION of its setters, its clearers
 * and its readers over the whole object, never from Bell 103's identically
 * placed byte -- see finding F8896 for the enumeration and finding F8889 for the
 * rule.  `V21RX_create` seeds the byte with `orb $0x50`, and bits 4 and 6 are
 * touched by nothing else in the object, so they stay unnamed.
 *
 * ERROR (0x02) -- set by `orb $0x2` at 0x0a1ccc inside `RxHdxErrorV21` and
 *   by the default arm of the state advance; cleared by `V21RX_modem` at the
 *   top of every block.  An event a caller must read each block or lose.
 *
 * DATA (0x01) -- set at every one of the four transitions that installs
 *   `RxHdxDataV21` (0x0a1dc6, 0x0a1fce, 0x0a21d7, 0x0a2351) and at no other
 *   site; cleared at every one of the four that installs `RxHdxIdleV21` and
 *   in the four default arms.  It is not touched by the transition INTO the
 *   WAIT state, so it reads as "the data state has been entered and has not
 *   ended" rather than "the data handler is installed".
 *
 * CARRIER (0x20) -- cleared and then set again if and only if
 *   `CarrierDetectV21` returns non-zero, in `RxHdxIdleV21` (0x0a1d31 /
 *   0x0a1d45), in `RxHdxWaitV21` (via the error arm) and in
 *   `RxHdxStartV21`; `RxHdxDataV21` sets it on entry and clears it on the
 *   arm where the carrier has gone.  It is the same bit and the same role as
 *   `B103_FLAG_CARRIER`, and that is a corroboration and not the derivation.
 *
 * LOW_SNR (0x80) -- cleared at 0x0a22bb and set at 0x0a22cd if and only if
 *   `GetSNRV21` returned at most `V21RX_SNR_THRESHOLD`; the one bit anything
 *   READS, at 0x0a2487 in `V21RX_status`, where a SET bit makes the reported
 *   `quality` zero and a clear one makes it 1.  So both ends are measured.
 */
#define V21RX_FLAG_DATA		(1 << 0)
#define V21RX_FLAG_ERROR	(1 << 1)
#define V21RX_FLAG_CARRIER	(1 << 5)
#define V21RX_FLAG_LOW_SNR	(1 << 7)

/*
 * The second flags byte, rx + 0x1a.  It is the third byte of the 32-bit word
 * `V21RX_modem` returns, so it does leave the library.
 *
 * Bit 0 is set at every transition that installs `RxHdxIdleV21` (0x0a1df6,
 * 0x0a1ffe, 0x0a21ae, 0x0a23b2) and at no other site, and cleared in the four
 * default arms and nowhere else.  Nothing in the object reads it, so the name
 * is from the transition it accompanies and from nothing else.
 */
#define V21RX_FLAG1_IDLE	(1 << 0)

/*
 * The receive state, `hdx->state`.
 *
 * 0, 1 and 2 ARE THE AUTHOR'S OWN WORDS: the state-advance block prints
 * "V21RX_STATE_START\n", "V21RX_STATE_WAIT\n" and "V21RX_STATE_DATA\n" from
 * .rodata.str1.1 at 0x4b3a, 0x4b16 and 0x4b28 in the case arms for those
 * three values, and "V21RX_DEFAULT, %d\n" at 0x4b03 for anything else.
 *
 * 3 and 4 have no string, and they are named from the one rule the object
 * makes forced: the arm for state N installs the handler whose name is state
 * N+1's, so START installs `RxHdxWaitV21`, WAIT installs `RxHdxDataV21` and
 * DATA installs `RxHdxIdleV21` with state 3.  4 is what `RxHdxWaitV21` writes
 * beside `hdx->handler = RxHdxErrorV21` at 0x0a2118.  The rule closes over
 * all four handlers the object defines.  Finding F8897.
 */
#define V21RX_STATE_START	0
#define V21RX_STATE_WAIT	1
#define V21RX_STATE_DATA	2
#define V21RX_STATE_IDLE	3
#define V21RX_STATE_ERROR	4

/*
 * The status byte, rx + 0x18, low byte of the word `V21RX_modem` returns.
 *
 * Seven values are written and NOTHING IN THE OBJECT READS ANY OF THEM, so
 * each name below is the site that writes it and no more than that.  DEFAULT
 * is the author's word, from the "V21RX_DEFAULT" string above.
 */
#define V21RX_STATUS_DATA	0	/* RxHdxDataV21, every block         */
#define V21RX_STATUS_START	1	/* RxHdxStartV21; also V21RX_create  */
#define V21RX_STATUS_WAIT	2	/* RxHdxWaitV21, carrier present     */
#define V21RX_STATUS_DEFAULT	3	/* the default arm of the advance    */
#define V21RX_STATUS_ERROR	4	/* RxHdxWaitV21, carrier gone        */
#define V21RX_STATUS_IDLE	5	/* RxHdxIdleV21, every block         */
#define V21RX_STATUS_TIMEOUT	6	/* RxHdxWaitV21, countdown expired   */

/*
 * `RxHdxDataV21` raises V21RX_FLAG_LOW_SNR when `GetSNRV21` comes back at or
 * below this.  The compare in the object is 16 bits wide (`cmpw $0x5,%ax` at
 * 0x0a22c7) even though `GetSNRV21` is declared to return `int` here, which
 * is why the call site below narrows before comparing.
 */
#define V21RX_SNR_THRESHOLD	5

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

/* ------------------------------------------------------------------------ */
/* The receive data path                                                    */

/*
 * One block through the receive chain, and the only function in this file
 * that touches the demodulator.
 *
 * Gain-control `count` samples of `in` in place, ask the tone detector about
 * the same block, resample what is left into the DSP block's own `mag`
 * buffer and demodulate that into `bits`.  Returns the number of bits the
 * demodulator wrote.
 *
 * THE OBJECT PASSES `FPM_AGC_agc` A FOURTH ARGUMENT, the constant 1, and
 * USES THE VALUE LEFT IN %eax -- neither of which that function has.  This is
 * the third site in the tree with the same shape (`src/pump/v23/bwchdem.c`
 * and `src/pump/v22/v22data.c` are the others) and it is answered the same
 * way: the extra argument has no observable effect and is dropped, and the
 * returned value is `agc.signal`, which is read out of the state instead.
 *
 * `bits` is spelled `short *` because that is what the half-duplex handler
 * signature carries; `FPM_FSD_demodulate` wants `unsigned short *` and the
 * cast is made here, once, rather than at each of the four call sites.
 *
 * THE INPUT BLOCK IS SILENCED IN PLACE when the tone detector returns
 * anything other than `FPM_MTD_ABSENT` and the installed handler is not
 * `RxHdxDataV21`.  That comparison is a DATA reference to `RxHdxDataV21`
 * (`R_386_32` on the `cmpl` at 0x0a57a3), which is why these five symbols are
 * one indivisible unit -- findings F8492 and F8493.
 */
unsigned short DemodDataV21(void *modem, short *in, short *bits,
			    unsigned short count);

/*
 * The four half-duplex receive states.  Each has the shape the handler slot
 * declares -- `(rx, in, out, count)` returning the number of output units --
 * and each consumes the WHOLE block, storing zero into `*count` before it
 * returns, so `V21RX_modem`'s loop runs a handler once per call.
 *
 * ERROR   demodulates the block, raises V21RX_FLAG_ERROR and returns 0.
 *         It does not advance the state, so the machine stays here.
 * IDLE    demodulates the block, reports V21RX_STATUS_IDLE and re-reads the
 *         carrier.  Also does not advance.
 * WAIT    demodulates the block; with no carrier it installs RxHdxErrorV21
 *         and reports V21RX_STATUS_ERROR, and with a carrier it counts
 *         `hdx->countdown` down and advances the state when it reaches zero.
 * DATA    demodulates the block only while the carrier is up and
 *         `hdx->int_0000` is clear, and reports the demodulator's SNR
 *         through V21RX_FLAG_LOW_SNR.  Otherwise it advances the state.
 */
short RxHdxErrorV21(void *modem, short *in, short *out, short *count);
short RxHdxIdleV21(void *modem, short *in, short *out, short *count);
short RxHdxWaitV21(void *modem, short *in, short *out, short *count);
short RxHdxDataV21(void *modem, short *in, short *out, short *count);

#endif /* DSPLIB_V21FAX_H */

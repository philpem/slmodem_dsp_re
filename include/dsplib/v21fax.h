#include "dsplib/period_byte_layout.h"
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
 * `V21RX_create` AND `V21TX_create` ARE BOTH RECONSTRUCTED NOW (F9500), so
 * both handles' LAYOUTS are the constructors' own.  Both are `void *` all the
 * same, and this header follows the ruling `include/dsplib/v17data.h` sets
 * out: the handle is `void *` and every offset into it is a named constant
 * with the evidence beside it.  `V21RX_create` fixes the receive handle at
 * `V21RX_OBJ_SIZE` bytes and fixes the WIDTH of every field in it, which is
 * why the offsets below now run to +0x4b instead of stopping at +0x1a; what
 * it does NOT do is say what most of them mean, since it is the only thing in
 * the object that writes them.  `V21TX_create` fixes the transmit handle at
 * `V21TX_OBJ_SIZE` (0x28) bytes the same way -- its own config area
 * (+0x00..+0x1b) is likewise WRITE-ONLY, read back by nothing reconstructed,
 * and kept as `struct v21tx_cfg` in `v21cfg.h` for that reason rather than
 * individually-named fields.  What IS modelled is the two DSP sub-blocks the
 * handles point at, because every field in them is forced by the type of the
 * callee it is handed to -- which is CLAUDE.md's second-strongest class of
 * evidence, and is how `b103fp.h` came by the same layout for Bell 103.
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
 * Added by F9500, once `V21TX_create` and the transmit half-duplex machine
 * were reconstructed:
 *
 *   992ed  V21TX_create    tx + 0x20 -> the parameter/half-duplex block
 *   99595  V21TX_create    sysdep_malloc(0x28) is the whole handle
 *   9939f  V21TX_create    the block at tx+0x20 is 0x10 bytes
 *   993a8  V21TX_create    tx+0x20+0x08 <- TxHdxStartV21 (R_386_32, a DATA
 *                          reference -- F8493's hazard, not a call)
 *   a26ed  TxHdxStartV21   FIFO_read(params->fifo, ...) types params+0x00
 *   a2867  TxHdxIdleV21    fifo->+0x0c is tested for zero -- `fax_fifo`'s
 *                          own `count`, so the FIFO pointer at params+0x00
 *                          is typed `struct fax_fifo *` independently of
 *                          `V21TX_delete`'s `FIFO_delete` call on it
 *   a2723  TxHdxStartV21   params+0x0c read `movswl`, switched on 0/1/2
 *   a25b8  TxNextStateV21  the canonical copy of that switch, out of line
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
#include "dsplib/v21cfg.h"

struct fpm_mtd;
struct fax_fifo;

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

struct v21_tx_hdx {
	struct fax_fifo	*fifo;
	int		int_0004;
	short		(*handler)(void *modem, unsigned short *in,
				   short *out, short *budget);
	short		state;
	short		short_000e;
};

union v21_tx_result {
	int word;
	struct {
		unsigned char status;
		unsigned char flags1;
		unsigned char flags2;
		unsigned char byte3;
	} byte;
};

struct v21_tx {
	struct v21tx_cfg		cfg;
	union v21_tx_result	result;
	struct v21_tx_hdx	*hdx;
	struct v21_tx_dsp	*dsp;
};

/*
 * `V21TX_create`'s literal at 0x0994c3: 320 bytes, 160 shorts.  Established
 * by F9500; unlike the receiver's `V21RX_MAG_BYTES` this one has no second
 * independent reading (nothing reconstructed reads a symbol-carried length
 * for it) so it is recorded as a bare literal rather than tied to a
 * modulator field.
 */
#define V21TX_SCRATCH_BYTES	0x140

/* The transmit DSP block's home in the transmitter handle. */
#define V21TX_OBJ_DSP		0x24

#define V21TX_DSP(m) (((struct v21_tx *)(m))->dsp)

/*
 * The whole transmit handle, which `V21TX_create` (0x0992f0) fixes at 0x28
 * bytes: the literal `sysdep_malloc` is given at 0x099595 when the caller
 * passes NULL.  It is gapless end to end: +0x00..+0x1b is `struct v21tx_cfg`
 * (v21cfg.h), +0x1c..+0x1f is the result word `V21TX_OBJ_RESULT` already
 * names, +0x20 is `V21TX_OBJ_PARAMS` and +0x24 is `V21TX_OBJ_DSP`.
 */
#define V21TX_OBJ_SIZE		0x28

/*
 * `V21TX_control`'s second argument, at 0x0a2ba0.  Reads four fields and
 * nothing past +0x0d, the same "read what the loads force" rule
 * `v22ctl.h`'s `struct v22fp_ctl` states for the identical shape.  NOTHING
 * IN THE OBJECT CALLS `V21TX_control` directly; its only referrer is the
 * lowercase adapter `v21tx_control` (faxadapt.c), which forwards `arg`
 * unchanged from its own caller.
 *
 * `int_0004` and `scale` are `int`, both loaded and stored whole; `scale`
 * is then NARROWED to `short` on its way into `dsp->fsm.cfg.scale`
 * (`mov %dx,0x6(%ecx)`, the object's own truncation, so the source keeps the
 * cast rather than declaring the field `short` and losing the wide load).
 * `mask` and `flags` are `unsigned char`, each read once with
 * `movzbl` and tested bit by bit.
 *
 * `scale` IS RANK 2: `V21TX_control` (`src/fax/v21.c`) assigns it straight
 * into `dsp->fsm.cfg.scale`, `struct fpm_fsm_cfg`'s own already-named field
 * (`fpm_fsm.h`), narrowed exactly the way the object narrows it.
 *
 * `mask` AND `flags` ARE FOUND BY TWIN-CLASS DIFFING against `struct
 * v27tx_ctl` (this wave), not usage inference alone: `V27TX_control` tests
 * `mask`'s bit 2 against `V27TX_HANDLE_FLAGS`' own bit 2 and ORs it in on a
 * hit, exactly the shape `V21TX_control` runs here (`arg->mask &
 * V21TXCTL_SET_TXFLAGS_BIT2` -> `V21TX_FLAGS(modem) |=
 * V21TXCTL_SET_TXFLAGS_BIT2`, same bit position, same "test a mask bit,
 * OR the matching handle-flag bit" shape) -- so this byte is the same role
 * as V.27ter's `mask`, not a second `flags` byte.  `flags`'s own REINIT bit
 * sits at bit 1 in both `struct v21tx_ctl` and `struct v27tx_ctl`
 * (`V21TXCTL_REINIT`/`V27TXCTL_FLAGS_REINIT`), and its "force a params-block
 * int" bit sits at bit 4 in both (`V21TXCTL_SET_PARAMS_INT0004`/
 * `V27TXCTL_FLAGS_FORCE_INT_0008`) -- three independent bit-position matches
 * across two structs from two different modulations, which is what makes
 * this a real name and not a guess: per F10177-F10179's ruling, offset and
 * type agreement alone are the weakest of the three checks, and what closes
 * it here is the SITE -- both functions branch on the bit the same way.
 */
struct v21tx_ctl {
	unsigned char	unmapped_0000[0x04];
	int		int_0004;	/* +0x04 -> cfg->int_0008           */
	int		scale;		/* +0x08 -> dsp->fsm.cfg.scale, narrowed */
	unsigned char	mask;		/* +0x0c bit 2 -> V21TX_FLAGS bit 2 */
	unsigned char	flags;		/* +0x0d bit 1 REINIT, bit 4 forces
					 *       V21TXP_INT_0004           */
	unsigned char	unmapped_000e[0x02];	/* +0x0e                     */
	unsigned char	unmapped_0010[0x04];	/* +0x10                     */
};

/*
 * `class1tx.c`'s own `V21TX_CTL` -- the REINIT request template
 * `cHDLCtx_preamble_state_init` merges with `FAXVMI_CTL` -- is 20 bytes
 * (`nm -S`), four past `flags_0d`, matching `v17fax.h`'s `struct v17rx_ctl`
 * own trailing `unmapped_000e`/`int_0010` shape for the identical reason: the
 * object's `.data` template is genuinely that size and `cHDLCtx_preamble_
 * state_init` copies all of it (a whole-struct assignment reproduces that),
 * even though nothing reconstructed reads past `+0x0d`.  `unmapped_000e` and
 * `unmapped_0010` carry no claim beyond size.
 */

/*
 * `mask` bit 2 is ORed into `V21TX_FLAGS(modem)`; nothing pairs that
 * byte with a reader that would type any of its OTHER bits (v21fax.h's own
 * note on `V21TX_OBJ_FLAGS` above), so the name states only which bit this
 * function tests -- see `struct v21tx_ctl`'s own comment for why the byte
 * itself is named `mask` rather than left as `flags_0c`.  `flags` bit 4 sets
 * `V21TXP_INT_0004` -- an already-named field ("int: zero selects the FIFO
 * arm") -- as a boolean; bit 1 gates the self-referential `V21TX_create
 * (modem, modem)` reinit, the same move `V17RX_control` makes (finding
 * F9900).
 */
#define V21TXCTL_SET_TXFLAGS_BIT2	(1 << 2)	/* 0x04 */
#define V21TXCTL_SET_PARAMS_INT0004	(1 << 4)	/* 0x10 */
#define V21TXCTL_REINIT			(1 << 1)	/* 0x02 */

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
					 *       `V21RX_control` is the setter
					 *       (0x0a2437): a boolean from bit
					 *       4 of its own argument's
					 *       `flags_0d`.  See `struct
					 *       v21rx_ctl` below.           */
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
	unsigned short	ones_run;	/* +0x0c length of the current run of
					 *       non-zero demodulated units.
					 *       Only RxHdxStartV21 touches it;
					 *       V21RX_create zeroes it with a
					 *       `movw` at 0x098ef6, which is
					 *       what fixes the width.        */
	unsigned short	mark_seq;	/* +0x0e how many times `ones_run`
					 *       stood at exactly 6 and the
					 *       next unit was zero.  Zeroed by
					 *       `movw` at 0x098efc; read only
					 *       by RxHdxStartV21's `cmpw $0x4`
					 *       at 0x0a1f2b.  See the header
					 *       note on RxHdxStartV21 for what
					 *       is measured and what is not. */
};

union v21_rx_status_word {
	int word;
	struct {
		unsigned char status;
		unsigned char flags;
		unsigned char flags1;
		unsigned char byte3;
	} byte;
};

struct v21_rx {
	struct v21rx_cfg		cfg;
	union v21_rx_status_word status;
	short			*ptr_001c;
	int			int_0020;
	short			*ptr_0024;
	int			int_0028;
	int			int_002c;
	short			short_0030;
	unsigned char		pad_0032[2];
	int			int_0034;
	int			int_0038;
	short			short_003c;
	unsigned char		pad_003e[2];
	int			int_0040;
	int			int_0044;
	short			short_0048;
	unsigned char		pad_004a[2];
	struct v21_rx_hdx	*hdx;
	struct v21_rx_dsp	*dsp;
};

/*
 * `V21RX_control`'s second argument, at 0x0a2410.  Reads two fields and
 * nothing past +0x0d, the same "read what the loads force" rule
 * `v22ctl.h`'s `struct v22fp_ctl` states for the identical shape -- and the
 * same shape `v17fax.h`'s `struct v17rx_ctl` documents for `V17RX_control`
 * (finding F9900).  NOTHING IN THE OBJECT CALLS `V21RX_control` directly;
 * its only referrer is the lowercase adapter `v21rx_control` (faxadapt.c),
 * which forwards this argument unchanged from ITS OWN caller.
 *
 * `int_0004` is `int`, loaded and stored whole.  `flags` is `unsigned
 * char`, read once with `movzbl` and tested bit by bit.
 *
 * `flags` IS RENAMED FROM `flags_0d` BY TWIN-CLASS DIFFING (this wave),
 * against `struct v27rx_ctl`'s own already-real-named `flags` field: both
 * put their REINIT bit at bit 1 (`V21RXCTL_REINIT`/`V27RXCTL_FLAGS_REINIT`)
 * and both put a "force" bit at bit 4
 * (`V21RXCTL_SET_HDX_INT0000`/`V27RXCTL_FLAGS_FORCE_NOCARRIER`) -- the same
 * byte shape as `struct v21tx_ctl`'s own `flags` above, which the same
 * technique renamed from the transmit side.  The two bits' MEANINGS still
 * differ per modulation (one forces a stored hdx field, the other forces
 * "no carrier"), which is exactly why only the byte's ROLE -- "a control
 * flags byte with REINIT at bit 1" -- is asserted here, not a bit-for-bit
 * identity.
 */
struct v21rx_ctl {
	unsigned char	unmapped_0000[0x04];
	int		int_0004;	/* +0x04 -> cfg->int_0008           */
	unsigned char	unmapped_0008[0x05];
	unsigned char	flags;		/* +0x0d                            */
};

/*
 * Bit 4 sets `struct v21_rx_hdx::int_0000`; nothing pairs that field with a
 * reader that would type its MEANING (see the field's own comment), so the
 * name states only which bit reaches it.  Bit 1 gates the self-referential
 * `V21RX_create(modem, modem)` reinit -- the same move `V17RX_control`
 * makes, finding F9900.
 */
#define V21RXCTL_SET_HDX_INT0000	(1 << 4)	/* 0x10 */
#define V21RXCTL_REINIT			(1 << 1)	/* 0x02 */

/*
 * Offsets in the receiver handle.
 *
 * +0x18 is read as one 32-bit word and returned; +0x19 is written as a byte.
 * That is `b103fp.h`'s +0x1c/+0x1d shape -- a status byte with a flags byte
 * above it, read out together -- and it is why the word is spelled as a
 * `memcpy` in the source rather than as a cast.
 */
/*
 * The receiver's protocol word, read by `V21RX_status` alone
 * (`movzwl (%esi),%ecx` at 0x0a2473) and copied to the report's +0x00.
 * `V21TX_status` reads the transmitter's +0x00 for the same slot, so the
 * two handles agree about where this lives.
 */
#define V21RX_OBJ_PROTOCOL	0x00

#define V21RX_OBJ_STATUS	0x18
#define V21RX_OBJ_FLAGS		0x19
#define V21RX_OBJ_FLAGS1	0x1a
#define V21RX_OBJ_HDX		0x4c
#define V21RX_OBJ_DSP		0x50

/*
 * The whole handle, which `V21RX_create` (0x098e70) fixes at 0x54 bytes: it
 * is the literal `sysdep_malloc` is given at 0x099139 when the caller passes
 * NULL.
 */
#define V21RX_OBJ_SIZE		0x54

/*
 * The shared intermediate buffer `V21RX_create` hangs off the DSP block, the
 * literal at 0x099210.  320 bytes is 160 shorts, which is
 * `FPM_FSD_CFG.trace_len` -- the demodulator writes one trace word per input
 * sample and `GetSNRV21` rectifies that trace into `mag`, so the two buffers
 * are the same length and the count has two independent readings.
 */
#define V21RX_MAG_BYTES		0x140

/*
 * +0x00 .. +0x17 IS THE CONFIGURATION, and it is `struct v21rx_cfg` -- see
 * `v21cfg.h`.  `V21RX_create` copies twenty-four bytes over the head of the
 * handle, from the caller's table or from `V21RX_CFG` when the caller passes
 * none, and every later read of a config field reads it here.
 *
 * That also identifies the word `V21RX_status` reports as the "protocol":
 * `movzwl (%esi),%ecx` at 0x0a2473 reads +0x00, which is `chan2`.  The two
 * readings are independent and they agree, so the handle's head and the
 * config table are the same twenty-four bytes.
 */
#define V21RX_OBJ_CFG		0x00

/*
 * +0x1c .. +0x4b, WRITTEN ONLY BY `V21RX_create` AND READ BY NOTHING THAT IS
 * RECONSTRUCTED.  The widths are the constructor's own stores and the three
 * two-byte gaps are its own silence; the MEANINGS are not established, so the
 * names are `type_NNNN` and stay that way until something reads them.
 *
 * What IS measured is where two of them come from.  +0x1c takes the FSD's
 * trace buffer and +0x24 takes the ADDRESS of the FSD's `last_count`, both
 * read out of the DSP block at 0x0990a3 and 0x0990a6 -- so this looks like a
 * diagnostic export of the demodulator's trace, and "looks like" is exactly
 * why neither is named for it.
 *
 * +0x28 through +0x4b are three identical twelve-byte groups: an `int`, an
 * `int`, a `short`, and two bytes the constructor does not touch.  The
 * repetition is real -- 0x28/0x2c/0x30, 0x34/0x38/0x3c and 0x40/0x44/0x48,
 * with `movl`, `movl`, `movw` each time -- but three groups of the same shape
 * is not enough to say they are an ARRAY rather than three fields that happen
 * to match, and nothing reads them to settle it.  They are spelled out one at
 * a time for that reason.
 */
#define V21RX_OBJ_TRACE		0x1c	/* short *: dsp->fsd.trace          */
#define V21RX_OBJ_INT_0020	0x20
#define V21RX_OBJ_COUNT_AT	0x24	/* short *: &dsp->fsd.last_count    */
#define V21RX_OBJ_INT_0028	0x28
#define V21RX_OBJ_INT_002C	0x2c
#define V21RX_OBJ_SHORT_0030	0x30
#define V21RX_OBJ_INT_0034	0x34
#define V21RX_OBJ_INT_0038	0x38
#define V21RX_OBJ_SHORT_003C	0x3c
#define V21RX_OBJ_INT_0040	0x40
#define V21RX_OBJ_INT_0044	0x44
#define V21RX_OBJ_SHORT_0048	0x48

#define V21RX_DSP(m) (((struct v21_rx *)(m))->dsp)
#define V21RX_HDX(m) (((struct v21_rx *)(m))->hdx)
#define V21RX_STATUS(m) (((struct v21_rx *)(m))->status.byte.status)
#define V21RX_FLAGS(m) (((struct v21_rx *)(m))->status.byte.flags)
#define V21RX_FLAGS1(m) (((struct v21_rx *)(m))->status.byte.flags1)
#define V21RX_STATUS_AT(m) ((const void *)&((struct v21_rx *)(m))->status)

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
 * The two bits `V21RX_create` seeds, and the two the note above says nothing
 * else in the object touches.  `orb $0x50,0x19(%esi)` at 0x099095 sets both
 * and there is no other setter, no clearer and no reader anywhere in the 1.2
 * MB, so they are named BY BIT VALUE and by nothing else -- which is the only
 * honest name available when a bit is written once and never consulted.
 *
 * They do leave the library: +0x19 is the second byte of the 32-bit word
 * `V21RX_modem` returns, so a host could be reading them even though the
 * object does not.
 */
#define V21RX_FLAG_BIT4		(1 << 4)
#define V21RX_FLAG_BIT6		(1 << 6)

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

/*
 * `RxHdxStartV21`'s two constants, both read straight off its instructions.
 *
 * V21RX_MARK_RUN is the `cmpw $0x6,0xc(%ecx)` at 0x0a1ee0 -- the run length at
 * which the NEXT zero unit counts as a sequence.  V21RX_MARK_SEQ_THRESHOLD is
 * the `cmpw $0x4,0xe(%edx)` at 0x0a1f2b, taken with `jle`, so the state
 * advances on the FIFTH sequence and not the fourth.
 */
#define V21RX_MARK_RUN			6
#define V21RX_MARK_SEQ_THRESHOLD	4

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
	short		short_0e;	/* +0x0e written 0 by V21RX_status;
					 *      NOT written by V21TX_status */
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

#define V21TX_PROTOCOL(m) ((unsigned short)((struct v21_tx *)(m))->cfg.short_0000)
#define V21TX_FLAGS(m) (((unsigned char *)(void *)&((struct v21_tx *)(m))->cfg)[V21TX_OBJ_FLAGS])

/* ------------------------------------------------------------------------ */
/* The rest of the TRANSMITTER handle, from V21TX_modem and V21TX_delete    */

/*
 * The int `V21TX_modem` returns, and the flag byte inside it.
 *
 * The object clears one bit of the BYTE at +0x1d on entry (`andb $0xfd`,
 * 0x0a2502), sets that same bit and stores a literal 4 into the BYTE at +0x1c
 * on one condition (0x0a2560, 0x0a2564), and returns the INT at +0x1c
 * (0x0a256f).  So +0x1d is byte 1 of the four bytes at +0x1c and the function
 * both modifies and returns one word -- `v17fax.h`'s `V17TX_OBJ_RESULT` /
 * `_B1` pair, byte for byte, and `V29TX_modem`'s +0x1c/+0x1d likewise.
 *
 * NEUTRAL, because nothing reconstructed reads either.  What the bit indicates
 * is not established and neither is what the 4 means; it is spelled as the
 * object spells it, a BYTE store, which is why it cannot go through the int.
 *
 * The CONDITION is established: both are written only when the word count the
 * caller asked for differs from what `FIFO_write` accepted, so they report a
 * transmit queue that would not take the whole block.  On the non-FIFO arm the
 * two are equal by construction and neither is ever written.
 */
#define V21TX_OBJ_RESULT	0x1c
#define V21TX_OBJ_RESULT_B1	0x1d
#define V21TX_RESULT_B1_BIT1	0x02
#define V21TX_RESULT_BYTE_04	4

/*
 * Byte 2 of the same word, +0x1e -- established by F9500, not by
 * `V21TX_modem`, which never touches it.  `TxNextStateV21`'s three valid-state
 * arms and its default arm all write exactly one bit here or in
 * `V21TX_OBJ_RESULT_B1`, never both: V21TX_STATE_DATA's arm sets THIS byte's
 * bit 0 and clears `V21TX_OBJ_RESULT_B1`'s; the other two arms (and the
 * default) do the opposite.  Nothing reconstructed reads either bit, so both
 * are named by BIT VALUE alone.
 */
#define V21TX_OBJ_RESULT_B2	0x1e
#define V21TX_RESULT_B1_BIT0	0x01
#define V21TX_RESULT_B2_BIT0	0x01

/*
 * `V21TXP_STATE`'s three values, named from the author's own debug strings
 * -- see the field's own comment above.  `TxHdxStartV21` is installed at
 * V21TX_STATE_START (what `V21TX_create` seeds), `TxHdxDataV21` at
 * V21TX_STATE_DATA and `TxHdxIdleV21` at V21TX_STATE_IDLE: the CURRENT
 * handler when the state machine is asked to advance, cycling
 * START -> DATA -> IDLE -> START.
 */
#define V21TX_STATE_START	0
#define V21TX_STATE_DATA	1
#define V21TX_STATE_IDLE	2

/*
 * The status byte, `V21TX_OBJ_RESULT`'s low byte, tx + 0x1c -- established by
 * F9500 and by nothing older, since `V21TX_status` never reads it either.
 * Named by SITE, the same rule `V21RX_STATUS_*` uses, because nothing
 * reconstructed reads any of them:
 *
 *   V21TX_STATUS_DATA        0   TxHdxDataV21, satisfied or forced-full read
 *   V21TX_STATUS_IDLE        1   TxHdxIdleV21, FIFO empty (TxNoCarrierV21)
 *   V21TX_STATUS_UNDERRUN    3   TxHdxDataV21's FIFO-underrun, non-bypass arm
 *                                -- MOMENTARY, overwritten to DATA (0) two
 *                                statements later at the same call; kept
 *                                because the object writes it, D1240
 *   V21TX_STATUS_DEFAULT     2   the unknown-state arm, all four functions
 *   V21TX_STATUS_START       5   TxHdxStartV21, EVERY exit unconditionally
 */
#define V21TX_STATUS_DATA	0
#define V21TX_STATUS_IDLE	1
#define V21TX_STATUS_DEFAULT	2
#define V21TX_STATUS_UNDERRUN	3
#define V21TX_STATUS_START	5

/*
 * The parameter block the transmitter owns, and the three fields of it that
 * are reached.
 *
 * `V21TX_delete` releases the block's `fax_fifo` and then the block, so the
 * transmitter owns it -- the same correction `v17fax.h` records for V.17's.
 *
 * +0x00 is TYPED BY ITS CALLEES and that is rank 2: it is `FIFO_write`'s
 * first argument at 0x0a2592 and `FIFO_delete`'s at 0x0a263b.
 *
 * +0x04 and +0x08 are TYPED BY THE CONSTRUCTOR, which is the same rank: at
 * 0x0993a1 `V21TX_create` writes `movl $0x0,0x4(%edi)` and at 0x0993a8
 * `movl $TxHdxStartV21,0x8(%edi)` -- an `R_386_32` against a function, so the
 * slot holds a function pointer and the word beside it is an `int` seeded to
 * zero.  `V21TX_modem` reads the first to choose an arm and calls through the
 * second.  What the int MEANS is not established, so it keeps its offset name.
 */
#define V21TX_OBJ_PARAMS	0x20

#define V21TXP_FIFO		0x00	/* struct fax_fifo *                 */
#define V21TXP_INT_0004		0x04	/* int: zero selects the FIFO arm    */
#define V21TXP_PROCESS		0x08	/* the dispatch slot                 */

/*
 * The two shorts beyond the dispatch slot, established by F9500's reading of
 * `TxHdxStartV21`/`TxHdxIdleV21`/`TxHdxDataV21`/`TxNextStateV21` -- all four
 * are one state machine, instruction-identical at every site, exactly as
 * `RxNextStateV21` was four copies of one function on the receive side.
 *
 * `V21TXP_STATE` is switched on (sign-extended, `movswl`) by all four and
 * cycles V21TX_STATE_START -> DATA -> IDLE -> START as each transition
 * installs the NEXT handler at `V21TXP_PROCESS` and its own value here; the
 * three debug strings "V21TX_STATE_DATA/IDLE/START\n" (relocscan at
 * .rodata.str1.1:0x4b60/0x4b72/0x4b84) are the author's own names for the
 * CURRENT state at each arm, and "V21TX_DEFAULT, %d\n" (0x4b4d) for anything
 * else.  `V21TX_create` seeds it to V21TX_STATE_START alongside installing
 * `TxHdxStartV21`.
 *
 * `V21TXP_SHORT_000E` is zeroed by the START and IDLE transition arms (the
 * ones that install TxHdxDataV21 and TxHdxStartV21 respectively) and by
 * NOTHING else in the object, so it is not named further.
 */
#define V21TXP_STATE		0x0c	/* short: V21TX_STATE_*              */
#define V21TXP_SHORT_000E	0x0e

/*
 * What `V21TX_modem` initialises its inner loop's budget to, ONCE, before the
 * loop rather than per iteration -- `movw $0x6,0x1a(%esp)` at 0x0a2518.  The
 * dispatch slot is what decrements it, and the loop runs while it is STRICTLY
 * POSITIVE as a signed short (`cmpw $0x0` with `jg`), so a slot that overshot
 * into negative territory stops the loop rather than wrapping it.
 *
 * V.29's is 0x30 and V.17's is 0x30; this one is 6, which is the whole
 * difference in that statement between the three.
 */
#define V21TX_MODEM_BUDGET	6

/*
 * `V21TX_modem`'s inner call, spelled from the object's own argument set:
 * four slots written, the instance first, then the caller's two buffers
 * UNCHANGED, and fourth `lea 0x1a(%esp)` -- the address of a `short` LOCAL,
 * not the caller's count.  So the slot is handed a per-call budget the caller
 * never sees.
 *
 * `in` IS NOT ADVANCED between iterations and `out` IS: `0x34(%esp)` is
 * reloaded unchanged every time round (0x0a252e) while the output pointer
 * accumulates `2 * got` (0x0a254a).  The result is sign-extended with `cwtl`
 * before it joins the running total, so it is `short` and that is forced.
 *
 * `in` is `unsigned short *` because `FIFO_write` -- handed the very same
 * pointer on the other arm -- declares its source that way.  Rank 2.
 */
typedef short (*v21tx_process_fn)(void *modem, unsigned short *in, short *out,
				  short *budget);

/* ------------------------------------------------------------------------ */
/* The functions                                                            */

/**
 * @brief Modulate `nbits` V.21 transmit bits into `out`.
 *
 * Two stages, as Bell 103's `ModDataB103` is: the modulator fills the shared
 * scratch buffer at its own rate and the converter lifts that into `out`, so
 * the bit count and the sample count are different numbers and the return
 * is the second one. Nothing here bounds `nbits` against the scratch buffer
 * and the object has no guard either.
 *
 * The handle is re-read after the modulator returns (`mov 0x24(%ebx),%eax`
 * at 0xa58ba, not a reload of a cached local) -- the same idiom `v17data.h`
 * records for `ModDataV17` and `v22data.c` for `ModDataV22`. It is not
 * observable through any call this function can make.
 *
 * @param modem  The V.21 transmitter handle.
 * @param bits   The bits to modulate.
 * @param out    Destination for the modulated samples.
 * @param nbits  How many bits.
 * @return The number of samples written, zero-extended from `FPM_MRF_filter`'s `short` return.
 */
unsigned short ModDataV21(void *modem, const unsigned short *bits, short *out,
			  unsigned short nbits);

/**
 * @brief Modulate `nbits` bits with the carrier held off: identical timing, identical sample count, silence.
 *
 * The modulator's output scale is forced to zero across the modulate call
 * and restored afterwards, so the phase accumulator, the symbol counter and
 * the converter history all advance exactly as they would have. The
 * converter runs with whatever scale it was given, which by then is silence
 * anyway.
 *
 * `bits` IS read here, unlike `TxNoCarrierV17`'s equivalent argument: it is
 * handed straight to the modulator, which is still asked to modulate real
 * bits.
 *
 * @param modem  The V.21 transmitter handle.
 * @param bits   The bits to modulate (silently).
 * @param out    Destination; filled with silence.
 * @param nbits  How many bits.
 * @return The number of samples written -- the same count ModDataV21() would produce.
 */
unsigned short TxNoCarrierV21(void *modem, const unsigned short *bits,
			      short *out, unsigned short nbits);

/**
 * @brief Is a V.21 carrier present on the receive side?
 * @param modem  The V.21 receiver handle.
 * @return Non-zero if the receive block's two status words (dsp+0x04, dsp+0x08) indicate carrier.
 */
int CarrierDetectV21(void *modem);

/**
 * @brief Rectify the V.21 demodulator's trace into the receiver's own magnitude buffer.
 *
 * `fsd.trace` is "the lowpass output, one word per input sample" (fpm_fsd.h)
 * and `fsd.last_count` is how many of them the last call wrote, so this
 * walks exactly the samples the demodulator just produced and stores their
 * absolute values into `mag`.
 *
 * It returns a literal 0 and computes no ratio. The object then runs a
 * second loop over the same bound with an empty body; the natural reading
 * is an accumulation whose result became dead, but the object does not say
 * so and nothing here claims it. The loop is reproduced because it is in
 * the object; it has no observable effect. See D1038.
 *
 * The absolute value is the branchless `cltd; xor; sub` form, so an input
 * of -32768 comes back as -32768. That is not a bug being introduced here
 * -- it is what the object computes -- and the differential test drives it.
 *
 * @param modem  The V.21 receiver handle.
 * @return Always 0.
 */
int GetSNRV21(void *modem);

/**
 * @brief Fill a status report from the V.21 TRANSMITTER handle.
 * @param modem  The V.21 transmitter handle.
 * @param st     Output: the status report.
 * @return 1, or 0 for a NULL @p st.
 */
int V21TX_status(void *modem, struct v21_status *st);

/**
 * @brief Run the V.21 receiver over one block.
 *
 * `count` is in-out and changes units: it goes in as the number of input
 * samples available and comes back as the number of output units produced.
 * That is the same in-out convention `B103FP_modem` uses for `n_rx`, and it
 * is the shape finding F8607 / deviation D956 is about -- there is no clamp
 * anywhere, so a caller's output buffer must be sized from what the state
 * handlers can produce and not from the input count.
 *
 * The half-duplex handler at `hdx->handler` is called repeatedly with the
 * cursors advanced -- the input by however many samples the handler
 * consumed (which it reports by decrementing `*count`), the output by
 * however many units it returned -- until `*count` reaches zero. It is a
 * do-while: a handler is dispatched even when `*count` is zero on entry.
 *
 * The running total is truncated to a `short` on every iteration, so a
 * block that produces more than 32,767 units wraps.
 *
 * @param modem  The V.21 receiver handle.
 * @param in     Input samples.
 * @param out    Output units.
 * @param count  In/out: input samples available, then output units produced (see above).
 * @return The 32-bit word at V21RX_OBJ_STATUS -- status in the low byte, flags in the next.
 */
int V21RX_modem(void *modem, short *in, short *out, short *count);

/**
 * @brief Build a V.21 receiver, or re-initialise one the caller already has.
 *
 * `modem` NULL allocates a `V21RX_OBJ_SIZE` handle and, with it, every
 * buffer the DSP blocks need; a non-NULL one is re-initialised in place,
 * keeping whatever `hdx` and `dsp` allocations it already carries. That
 * distinction is the third argument to `FPM_MRF_init`, `FPM_AGC_init` and
 * `FPM_FSD_init`, so passing a handle whose `dsp` pointer is uninitialised
 * garbage is a crash rather than a fresh start -- the object has no guard.
 *
 * `params` NULL takes `V21RX_CFG`, which selects channel 2. The table is
 * copied over the handle's head, so the caller's may be a temporary.
 *
 * @param modem   NULL to allocate, or an existing handle to re-initialise in place.
 * @param params  Configuration to copy in, or NULL for the built-in `V21RX_CFG`.
 * @return The handle, allocated or not. There is no failure return: the object does not check `sysdep_malloc`.
 */
void *V21RX_create(void *modem, const struct v21rx_cfg *params);

/**
 * @brief Tear a V.21 receiver down.
 *
 * Frees the demodulator's and converter's buffers, the tone detector, the
 * magnitude buffer, the DSP block, the half-duplex context and the handle,
 * with no NULL guard anywhere and no check that the caller supplied the
 * handle rather than the constructor. See D1039.
 *
 * The order above is read from the object's call sequence (0x099284
 * through the tail call at 0x0992e6) and is not verified by any test. It is
 * stated because the disassembly states it, and the distinction matters:
 * `t_v21fax` compares a liveness vector over the ten allocations, and a
 * liveness vector is a set. Both sides call the same `sysdep_free` and the
 * harness records no sequence, so two implementations that free the same
 * blocks in different orders are indistinguishable to it. A parallel V.29
 * pass measured exactly that -- a hand mutation swapping two frees was the
 * one survivor of its set -- so this is a bound on the technique, not a
 * suspicion about it.
 *
 * Nothing here depends on the order being right: every callee takes one
 * pointer, none reads another's block, and the handle is released last on
 * both readings. A free log in the harness would close it for every delete
 * function in the tree at once, and is not this file's to add.
 *
 * @param modem  The V.21 receiver handle to tear down.
 */
void V21RX_delete(void *modem);

/*
 * Build a V.21 transmitter, or re-initialise one the caller already has.
 *
 * `modem` NULL allocates a `V21TX_OBJ_SIZE` handle (0x099595's own literal
 * 0x28) and, with it, every buffer the DSP block and the transmit FIFO need;
 * a non-NULL one is re-initialised IN PLACE, keeping whatever `params` and
 * `dsp` allocations it already carries -- the same contract `V21RX_create`
 * has for `hdx`/`dsp`, checked the same way (each sub-pointer tested for
 * NULL independently of the top-level handle).
 *
 * `params` NULL takes `V21TX_CFG` (v21cfg.h); the table is COPIED over the
 * handle's head, so the caller's may be a temporary.  NOTHING reconstructed
 * reads any field of it back out of the handle -- see `v21cfg.h`'s own
 * derivation note.
 *
 * ALWAYS INSTALLS `TxHdxStartV21` AT `V21TX_STATE_START`, whether or not
 * `params`/`dsp` already existed -- 0x0993a8/0x0993af are unconditional, past
 * the branch that skips (re)allocating the DSP block.  So a caller
 * re-initialising a handle mid-transmission resets the half-duplex machine
 * to its first state; there is no path that resumes one.
 *
 * `FPM_FSM_init`'s frequencies and `samples_per_sym` are `FPM_FSM_CFG`'s own
 * (V.21 channel 2, 1850/1650 Hz, 24 samples/symbol) UNCONDITIONALLY --
 * `V21TX_create` writes two DIFFERENT literal frequency pairs first (1180/980
 * for `V21TX_CFG.short_0000 == 0`, 0/0 otherwise, 1850/1650 for `== 1`) and
 * every one of the three is immediately overwritten by the same 32-bit load
 * off `FPM_FSM_CFG` at 0x099409, so none is ever observed.  `scale` alone
 * survives as an override, to 0x1900 (6400) rather than the library's 32767.
 * D1241 records the dead literals; the net effect is reproduced without
 * writing dead code for it.  `V21TX_CFG.short_0000` still selects which of
 * three (near-identical, past the dead writes) paths runs, and only the
 * "neither 0 nor 1" one has an observable side effect: it raises
 * `V21TX_RESULT_B1_BIT1` and sets `V21TX_OBJ_RESULT` to `V21TX_STATUS_DEFAULT`
 * before falling into the shared tail.
 *
 * The resampler is 10:9 (branches:decimate) here against the receiver's 9:10
 * -- the opposite direction, as the two sides convert opposite ways -- over
 * `V21_MRF_FILT`'s same 360 taps, shared with the receiver's own instance.
 *
 * There is no failure return: the object does not check `sysdep_malloc`.
 */
void *V21TX_create(void *modem, const struct v21tx_cfg *params);

/**
 * @brief Tear a V.21 transmitter down.
 *
 * Seven releases, in the object's order (0x0995f8 through the sibling
 * `jmp` at 0x099653): the modulator, the rate converter, the shared
 * scratch buffer and then the DSP block itself, then the `fax_fifo` the
 * parameter block owns and that block, and the handle last.
 *
 * It is also the second, independent statement of `struct v21_tx_dsp`.
 * `ModDataV21` establishes +0x00, +0x10 and +0x2c by which module each is
 * handed to; this function establishes the same three by which module
 * releases each -- `FPM_FSM_delete`, `FPM_MRF_free` and a plain
 * `sysdep_free` of the pointer at +0x2c. Two readings of one block, neither
 * derived from the other, and they agree. Finding F9252.
 *
 * The literal 1 in the second argument slot is not reproduced. The object
 * plants one at 0x099603 before `FPM_MRF_free`, which takes a single
 * argument and reads no frame slot past the first. Finding F8876, and
 * `V17TX_delete` and `V21RX_delete` both carry the note.
 *
 * There is no NULL guard on anything and the handle is released
 * unconditionally by a sibling `jmp`, so a caller that supplied the storage
 * does not get it back. Both reproduced; see docs/deviations.md D1150,
 * which is this pair's entry and points back at D1039 for the receive side.
 *
 * @param modem  The V.21 transmitter handle to tear down.
 */
void V21TX_delete(void *modem);

/**
 * @brief Drive the V.21 transmitter for one caller block.
 *
 * Two arms on the way in, chosen by `V21TXP_INT_0004`. Zero queues the
 * caller's `count` words through `FIFO_write` and remembers how many it
 * took; non-zero remembers `count` itself and touches the FIFO not at all.
 * What is remembered is compared against `*count` after the loop, and a
 * mismatch is what sets `V21TX_RESULT_B1_BIT1` and writes
 * `V21TX_RESULT_BYTE_04` -- so on the second arm the comparison is between
 * a value and itself and neither is ever written.
 *
 * The loop is a do/while on a LOCAL, not on the caller's count. See
 * `v21tx_process_fn`: the budget starts at `V21TX_MODEM_BUDGET`, is set
 * once before the loop, and the slot decrements it.
 *
 * `count` is in/out and changes units across the call -- on entry the
 * number of input words, on return the total the slot produced -- and
 * nothing clamps what the slot writes through `out`. That is the shape
 * deviation D956 is about; a caller's output buffer must be sized from what
 * the slot can produce over `V21TX_MODEM_BUDGET` and not from the input
 * count.
 *
 * The running total is a `short` and the object re-narrows it with `cwtl`
 * on every iteration, so a block producing more than 32,767 units wraps.
 * Reproduced, not corrected; docs/deviations.md D1148.
 *
 * @param modem  The V.21 transmitter handle.
 * @param in     Input words to transmit.
 * @param out    Output samples.
 * @param count  In/out: input word count, then samples produced (see above).
 * @return The 32-bit word at V21TX_OBJ_RESULT.
 */
int V21TX_modem(void *modem, unsigned short *in, short *out,
		unsigned short *count);

/*
 * Reconfigure the TRANSMITTER in place, or report there was nothing to do.
 *
 * See `struct v21tx_ctl` above for the argument.  The self-referential
 * `V21TX_create(modem, modem)` reinit this can trigger is the same move
 * `V17RX_control` makes and for the same reason (finding F9900): the
 * transmit handle's head, byte for byte, IS its own `struct v21tx_cfg`.
 */
int V21TX_control(void *modem, const struct v21tx_ctl *arg);

/* ------------------------------------------------------------------------ */
/* The receive data path                                                    */

/**
 * @brief One block through the V.21 receive chain: the only function in this file that touches the demodulator.
 *
 * Gain-controls `count` samples of `in` in place, asks the tone detector
 * about the same block, resamples what is left into the DSP block's own
 * `mag` buffer and demodulates that into `bits`.
 *
 * The object passes `FPM_AGC_agc` a fourth argument, the constant 1, and
 * uses the value left in `%eax` -- neither of which that function has. This
 * is the third site in the tree with the same shape (`src/pump/v23/
 * bwchdem.c` and `src/pump/v22/v22data.c` are the others) and it is
 * answered the same way: the extra argument has no observable effect and
 * is dropped, and the returned value is `agc.signal`, which is read out of
 * the state instead.
 *
 * `bits` is spelled `short *` because that is what the half-duplex handler
 * signature carries; `FPM_FSD_demodulate` wants `unsigned short *` and the
 * cast is made here, once, rather than at each of the four call sites.
 *
 * The input block is silenced in place when the tone detector returns
 * anything other than `FPM_MTD_ABSENT` and the installed handler is not
 * `RxHdxDataV21`. That comparison is a data reference to `RxHdxDataV21`
 * (`R_386_32` on the `cmpl` at 0x0a57a3), which is why these five symbols
 * are one indivisible unit -- findings F8492 and F8493.
 *
 * @param modem  The V.21 receiver handle.
 * @param in     Input samples, gain-controlled in place.
 * @param bits   Output for the demodulated bits.
 * @param count  How many input samples.
 * @return The number of bits the demodulator wrote.
 */
unsigned short DemodDataV21(void *modem, short *in, short *bits,
			    unsigned short count);

/*
 * The four half-duplex receive states below share one shape: each has the
 * signature the handler slot declares -- `(rx, in, out, count)` returning
 * the number of output units -- and each consumes the WHOLE block, storing
 * zero into `*count` before it returns, so `V21RX_modem`'s loop runs a
 * handler once per call. See each function's own @brief for what it does.
 */
/*
 * Reconfigure the RECEIVER in place, or report there was nothing to do.
 *
 * See `struct v21rx_ctl` above for the argument and `V17RX_control`'s much
 * longer comment (v17fax.h, finding F9900) for why the self-referential
 * `V21RX_create(modem, modem)` reinit this makes is legitimate rather than
 * an aliasing accident: the receive handle's head IS its own `struct
 * v21rx_cfg`.
 */
int V21RX_control(void *modem, const struct v21rx_ctl *arg);

/*
 * Fill a status report from the RECEIVER, and say whether there was one to
 * fill: 0 for a null pointer, 1 otherwise.
 *
 * It is not `V21TX_status` with the handle changed.  It reports the rate in
 * `rx_bps` rather than `tx_bps`, it calls `GetSNRV21` and puts the answer in
 * `snr` where the transmit side writes a literal 0, it derives `quality`
 * from V21RX_FLAG_LOW_SNR, it zeroes +0x0e rather than +0x0c, and its
 * `flags` byte is written as a literal 0 rather than merged.
 *
 * `short_12` IS COMPUTED, and it is the only arithmetic in the function:
 *
 *     (2 - 2 * fsd.f22 / fsd.cfg.bit_samples) * V21_STATUS_BPS
 *
 * `f22` is `bit_samples / 2`, set by `FPM_FSD_init` -- so for an even
 * `bit_samples` the quotient is 1 and the field comes out at 300, the same
 * number `rx_bps` gets from a literal.  See F9132 and D1098: the divide is
 * unguarded.
 */
int V21RX_status(void *modem, struct v21_status *st);

/**
 * @brief V.21 receive state ERROR: demodulate the block, raise V21RX_FLAG_ERROR.
 *
 * Does not advance the state, so the machine stays here.
 *
 * @param rx     The V.21 receiver handle.
 * @param in     Input samples.
 * @param out    Output units.
 * @param count  In/out sample count; zeroed before return (see the family note above).
 * @return Always 0.
 */
short RxHdxErrorV21(void *modem, short *in, short *out, short *count);

/**
 * @brief V.21 receive state IDLE: demodulate the block, report V21RX_STATUS_IDLE.
 *
 * Re-reads the carrier. Also does not advance the state.
 *
 * @param rx     The V.21 receiver handle.
 * @param in     Input samples.
 * @param out    Output units.
 * @param count  In/out sample count; zeroed before return (see the family note above).
 * @return The number of demodulated units.
 */
short RxHdxIdleV21(void *modem, short *in, short *out, short *count);

/**
 * @brief V.21 receive state WAIT: demodulate the block and watch for carrier.
 *
 * With no carrier it installs RxHdxErrorV21 and reports
 * V21RX_STATUS_ERROR; with a carrier it counts `hdx->countdown` down and
 * advances the state when it reaches zero.
 *
 * @param rx     The V.21 receiver handle.
 * @param in     Input samples.
 * @param out    Output units.
 * @param count  In/out sample count; zeroed before return (see the family note above).
 * @return The number of demodulated units.
 */
short RxHdxWaitV21(void *modem, short *in, short *out, short *count);

/**
 * @brief V.21 receive state DATA: demodulate the block while carrier holds.
 *
 * Demodulates only while the carrier is up and `hdx->int_0000` is clear,
 * and reports the demodulator's SNR through V21RX_FLAG_LOW_SNR. Otherwise
 * it advances the state.
 *
 * @param rx     The V.21 receiver handle.
 * @param in     Input samples.
 * @param out    Output units.
 * @param count  In/out sample count; zeroed before return (see the family note above).
 * @return The number of demodulated units.
 */
short RxHdxDataV21(void *modem, short *in, short *out, short *count);

/**
 * @brief Fill a status report from the V.21 RECEIVER handle.
 *
 * Not `V21TX_status` with the handle changed: it reports the rate in
 * `rx_bps` rather than `tx_bps`, it calls `GetSNRV21` and puts the answer
 * in `snr` where the transmit side writes a literal 0, it derives
 * `quality` from V21RX_FLAG_LOW_SNR, it zeroes +0x0e rather than +0x0c, and
 * its `flags` byte is written as a literal 0 rather than merged.
 *
 * `short_12` is computed, and it is the only arithmetic in the function:
 *
 *     (2 - 2 * fsd.f22 / fsd.cfg.bit_samples) * V21_STATUS_BPS
 *
 * `f22` is `bit_samples / 2`, set by `FPM_FSD_init` -- so for an even
 * `bit_samples` the quotient is 1 and the field comes out at 300, the same
 * number `rx_bps` gets from a literal. See F9132 and D1098: the divide is
 * unguarded.
 *
 * @param modem  The V.21 receiver handle.
 * @param st     Output: the status report.
 * @return 1, or 0 for a NULL @p st.
 */
int V21RX_status(void *modem, struct v21_status *st);

/**
 * @brief V.21 receive state START: the fifth state, and the one V21RX_create() installs.
 *
 * `V21RX_create` installs this with `movl $RxHdxStartV21,0x4(%eax)` at
 * 0x098ee1, with `state` and all three counters zeroed around it, so START
 * is where every receiver begins.
 *
 * What it counts, stated as the instructions state it: after demodulating
 * the block it walks the demodulated units and maintains `hdx->ones_run`
 * as the length of the current run of non-zero ones; each time that run
 * stands at exactly 6 and the next unit is zero, `hdx->mark_seq` is
 * incremented. The state advances only once `mark_seq` exceeds 4 AND
 * `CarrierDetectV21` answers.
 *
 * Six ones followed by a zero is the HDLC flag 0x7e, and five of them is a
 * preamble -- but that identification is usage inference and nothing in
 * the object says it. There is no format string for either field and no
 * other referent to type them, so the names above describe the arithmetic
 * and the interpretation is left in this comment where a later reader can
 * weigh it. Finding F9090.
 *
 * @param rx     The V.21 receiver handle.
 * @param in     Input samples.
 * @param out    Output units.
 * @param count  In/out sample count; zeroed before return.
 * @return The number of demodulated units.
 */
short RxHdxStartV21(void *modem, short *in, short *out, short *count);

/**
 * @brief Advance the V.21 receive state machine one step.
 *
 * The object carries this block four times: once out of line under this
 * name at 0x0a1d60, and three more times inlined into `RxHdxStartV21`,
 * `RxHdxWaitV21` and `RxHdxDataV21`, instruction for instruction. That is
 * what GCC 3.4.2 at `-O3` does with an externally visible function whose
 * body it can see, so the author wrote one function and four copies came
 * out. Finding F8898 recorded it while the symbol was still `static` here
 * for want of its third caller; `RxHdxStartV21` is that caller and the
 * symbol is now claimed.
 *
 * @param modem  The V.21 receiver handle.
 */
void RxNextStateV21(void *modem);

/* ------------------------------------------------------------------------ */
/* The transmit data path                                                   */

/*
 * Advance the transmit state machine one step: switch on `V21TXP_STATE`,
 * install the next handler at `V21TXP_PROCESS`, and set `V21TXP_STATE` to
 * match -- see the field's own comment for the cycle and the strings that
 * name it.
 *
 * The object carries this block four times, byte for byte: once out of line
 * as `TxNextStateV21` (0x0a25b0, 280 bytes) and three more times inlined,
 * once each into `TxHdxStartV21`, `TxHdxIdleV21` and `TxHdxDataV21` -- the
 * transmit-side twin of `RxNextStateV21`'s situation (F8898/F9091), except
 * all three inlining call sites are written together here, so there was no
 * intermediate `static` step.
 */
void TxNextStateV21(void *modem);

/*
 * The three half-duplex TRANSMIT states.  Each has the shape
 * `v21tx_process_fn` declares -- `(modem, in, out, budget)` returning the
 * number of samples produced -- and each is installed at `V21TXP_PROCESS`
 * by `V21TX_create` or by one another through `TxNextStateV21`.
 *
 * START  reads up to `*budget` elements out of the FIFO into `in`, modulates
 *        what it got, decrements `*budget` by the amount taken, calls
 *        `TxNextStateV21`, and unconditionally reports V21TX_STATUS_START --
 *        overwriting whatever `TxNextStateV21` itself wrote to the status
 *        byte, including its own V21TX_STATUS_DEFAULT.  Installed by
 *        `V21TX_create` and by IDLE's own default arm.
 *
 * DATA   reads up to `*budget` elements.  If the FIFO supplied the whole
 *        request, modulates `taken` bits, zeroes `*budget` (taken equals the
 *        request by construction) and reports V21TX_STATUS_DATA.  On an
 *        underrun with `V21TXP_INT_0004 == 0`, modulates the FULL requested
 *        budget anyway (not just what the FIFO supplied), forces `*budget`
 *        to zero, and reports V21TX_STATUS_DATA too -- V21TX_STATUS_UNDERRUN
 *        is written and then immediately overwritten at the same call
 *        (D1240).  On an underrun with `V21TXP_INT_0004 != 0`, modulates only
 *        `taken`, LEAVES the remainder in `*budget` (does not force it to
 *        zero), calls `TxNextStateV21`, and reports whatever that installed.
 *
 * IDLE   with the FIFO empty, calls `TxNoCarrierV21` for the WHOLE current
 *        budget in one call, forces `*budget` to zero, reports
 *        V21TX_STATUS_IDLE and returns the sample count.  With the FIFO
 *        non-empty, does NOT modulate at all: calls `TxNextStateV21` and
 *        returns 0.
 */
short TxHdxStartV21(void *modem, unsigned short *in, short *out,
		    short *budget);
short TxHdxIdleV21(void *modem, unsigned short *in, short *out,
		   short *budget);
short TxHdxDataV21(void *modem, unsigned short *in, short *out,
		   short *budget);

#endif /* DSPLIB_V21FAX_H */

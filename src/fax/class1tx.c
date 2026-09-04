/*
 * class1tx.c -- Class 1 fax: the message reporters and two machine leaves.
 *
 * Reconstructed from dsplibs.o's class1tx.c +94 span (the split into author
 * files inside that span is NOT established -- see class1tx.h):
 *
 *   v17tx_message / v17rx_message   .text 0x09c270 / 0x09c270   39 each
 *   v21tx_message / v21rx_message   .text 0x09c510 / 0x09c510   39 each
 *   v27tx_message / v27rx_message   .text 0x09c810 / 0x09c810   39 each
 *   v29tx_message / v29rx_message   .text 0x09cad0 / 0x09cad0   39 each
 *   _init_tx_nulls_state            .text 0x09cf60              15
 *   _hdlc_receive_state_init        .text 0x09d880              79
 *   _send_hdlc_between_buffer_state_init .text 0x09e450         17
 *   _handle_data_input              .text 0x09eb60             321
 *   _handle_hdlc_input_open         .text 0x09ecb0              18
 *   _handle_hdlc_input_close        .text 0x09ecd0              34
 *   _handle_hdlc_input              .text 0x09ed00             238
 *   cTOOLS_handle_data_output_reset .text 0x09edf0              24
 *   _handle_data_output             .text 0x09ee10             255
 *   null_message                    .text 0x09f140              11
 *   aReversedCharsArray             .rodata 0xba40             256
 *
 * and the eight `.data` tables the reporters index.  Everything here is
 * finding F8320's no-entry-point bucket -- this is not the fax phase.
 * `_send_hdlc_between_buffer_state_init` (0x09e450) was left out of the leaf
 * pass because it tail-calls the then-unreconstructed
 * `_handle_hdlc_input_open` (F215: no scaffold).  Writing that callee here
 * unblocked it and both are now in, which is the link constraint working the
 * way round it is supposed to -- F9198.  `cHDLCtx_off_init` was the same
 * shape one step further out: F8492 called it blocked on `FAXVMI_control`
 * alone, but `FAXVMI_control` (landed wave 11) itself recurses through the
 * `vxx_control` table, so the true dependency was the whole eight-function
 * `v??tx_control`/`v??rx_control` family -- all landed by wave 10-11, so it
 * is written below alongside its sibling `_cHDLCrx_init_from_idle`.  The V21
 * next-state pair, which store six unreconstructed handler addresses, are
 * still out.  Findings F8492/F8493.
 *
 * THE STRINGS ARE THE AUTHOR'S, byte for byte: "Protocal", "Transmition"
 * and V29TX's "7600 bps" are the object's spellings, and fixing them would
 * change .rodata.  The guard in every reporter allows one code past the end
 * of its table -- see D951 -- and the in-range/above-range behaviour is
 * what the differential test pins (the one-past read lands in whatever the
 * link put next, there as here).
 */

#include <stddef.h>
#include <unistd.h>

#include "dsplib/class1.h"
#include "dsplib/class1rx.h"
#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/faxvmi.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sysdep.h"
#include "dsplib/t30frame.h"
#include "dsplib/v17fax.h"
#include "dsplib/v21fax.h"
#include "dsplib/v27fax.h"
#include "dsplib/v29data.h"
#include "dsplib/v29fax.h"

/*
 * `aReversedCharsArray` -- .rodata 0xba40, 256 bytes, GLOBAL.  Entry `i` is
 * `i` with its eight bits reversed, and that is CHECKED rather than assumed:
 * the test compares all 256 bytes against the object's copy, and this file
 * carries the bytes rather than a loop because the object carries bytes.
 *
 * WHY IT IS IN THIS FILE, which is an inference and is the weakest thing on
 * this page.  Five relocations name it: one from `GetT30FrameIDFromBuffer`
 * (0x96e9e) and FOUR from `cTOOLS_handle_hdlc_output` (0x9f029, 0x9f051,
 * 0x9f066, 0x9f08a), which sits at 0x9ef10, inside this file's span.  Its
 * `.rodata` address is also far past the FAXVMI/FIFO cluster's (0x9490-0x9660)
 * and in the range this span's other constants occupy.  Neither argument is
 * decisive on its own; together they put it here rather than in
 * `t30frame.c`, whose one reference is the minority.
 */
const unsigned char aReversedCharsArray[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

char *V17RX_MESG[10] = {
	"V.17 Receive Data Mode",
	"V.17 Receive Protocol Mode",
	"V.17 Rx Start of Protocol",
	"ERROR: V.17 Rx Internal Error",
	"ERROR: Loss Carrier In V.17 Rx Protocol",
	"V.17 Rx Idle (End of Transmition)",
	"CONNECT: V.17 Receive 14400 bps",
	"CONNECT: V.17 Receive 12000 bps",
	"CONNECT: V.17 Receive 9600 bps",
	"CONNECT: V.17 Receive 7200 bps",
};

char *V17TX_MESG[10] = {
	"V.17 Transmit Data Mode",
	"V.17 Protocal Transmit Mode",
	"CONNECT: V.17 Transmit 14400 bps",
	"CONNECT: V.17 Transmit 12000 bps",
	"CONNECT: V.17 Transmit 9600 bps",
	"CONNECT: V.17 Transmit 7200 bps",
	"V.17 Transmit Idle Mode",
	"ERROR: V.17 Tx Internal Error",
	"ERROR: Transmit Input Queue Under-run",
	"ERROR: Transmit Input Queue Over-run",
};

char *V21RX_MESG[7] = {
	"V.21 Receive Data Mode",
	"V.21 Rx Start of Protocol",
	"V.21 Rx Wait for Valid Data Protocol",
	"ERROR: V.21 Rx Internal Error",
	"ERROR: Loss Carrier In V.21 Rx Protocol",
	"V.21 Rx Idle (End of Transmition)",
	"CONNECT: V.21 Receive 300 bps",
};

char *V21TX_MESG[6] = {
	"V.21 Transmit Data Mode",
	"V.21 Transmit Idle Mode",
	"ERROR: V.21 Tx Internal Error",
	"ERROR: Transmit Input Queue Under-run",
	"ERROR: Transmit Input Queue Over-run",
	"CONNECT: V.21 Transmit 300 bps",
};

char *V27RX_MESG[8] = {
	"V.27 Receive Data Mode",
	"V.27 Receive Protocol Mode",
	"V.27 Rx Start of Protocol",
	"ERROR: V.27 Rx Internal Error",
	"ERROR: Loss Carrier In V.27 Rx Protocol",
	"V.27 Rx Idle (End of Transmition)",
	"CONNECT: V.27 Receive 2400 bps",
	"CONNECT: V.27 Receive 4800 bps",
};

char *V27TX_MESG[8] = {
	"V.27ter Transmit Data Mode",
	"V.27ter Protocal Transmit Mode",
	"CONNECT: V.27ter Transmit 4800 bps",
	"CONNECT: V.27ter Transmit 2400 bps",
	"V.27ter Transmit Idle Mode",
	"ERROR: V.27ter Tx Internal Error",
	"ERROR: Transmit Input Queue Under-run",
	"ERROR: Transmit Input Queue Over-run",
};

char *V29RX_MESG[8] = {
	"V.29 Receive Data Mode",
	"V.29 Receive Protocol Mode",
	"V.29 Rx Start of Protocol",
	"ERROR: V.29 Rx Internal Error",
	"ERROR: Loss Carrier In V.29 Rx Protocol",
	"V.29 Rx Idle (End of Transmition)",
	"CONNECT: V.29 Receive 9600 bps",
	"CONNECT: V.29 Receive 7200 bps",
};

char *V29TX_MESG[8] = {
	"V.29 Transmit Data Mode",
	"V.29 Protocal Transmit Mode",
	"CONNECT: V.29 Transmit 9600 bps",
	"CONNECT: V.29 Transmit 7600 bps",	/* sic: 7200 misspelled */
	"V.29 Transmit Idle Mode",
	"ERROR: V.29 Tx Internal Error",
	"ERROR: Transmit Input Queue Under-run",
	"ERROR: Transmit Input Queue Over-run",
};

/*
 * One shape, eight times: the code is guarded UNSIGNED against the table's
 * entry count (so a negative code answers NULL) and indexed through an
 * (unsigned char) cast -- both the object's, including the off-by-one guard
 * D951 describes.  The handle argument is part of the vxx_* dispatch
 * contract (see faxvmi.h) and is read by none of them.
 */
#define MESSAGE_FN(name, table, guard)					\
	void								\
	name(void *handle, int code, char **out)			\
	{								\
		(void)handle;						\
		if ((unsigned)code > (guard))				\
			*out = NULL;					\
		else							\
			*out = (table)[(unsigned char)code];		\
	}

MESSAGE_FN(v17tx_message, V17TX_MESG, 10)
MESSAGE_FN(v17rx_message, V17RX_MESG, 10)
MESSAGE_FN(v21tx_message, V21TX_MESG, 6)
MESSAGE_FN(v21rx_message, V21RX_MESG, 7)
MESSAGE_FN(v27tx_message, V27TX_MESG, 8)
MESSAGE_FN(v27rx_message, V27RX_MESG, 8)
MESSAGE_FN(v29tx_message, V29TX_MESG, 8)
MESSAGE_FN(v29rx_message, V29RX_MESG, 8)

void
null_message(void *handle, int code, char **out)
{
	(void)handle;
	(void)code;
	*out = NULL;
}

int
_init_tx_nulls_state(struct fax_class1 *ctx)
{
	ctx->countdown = 0;
	return 0;
}

/*
 * Nothing but a log line and one cleared field.  The message is the author's,
 * byte for byte, and it names the function -- which is how this timestamp
 * pair got its name in class1.h.
 */
int
_hdlc_receive_state_init(struct fax_class1 *ctx)
{
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf(
		    "At %2d.%02d[sec] hdlc_receive_state_init\n",
		    ctx->clock_sec, ctx->clock_frac);
	ctx->countdown = 0;
	return 0;
}

/*
 * Clear the countdown and open a frame.  The object TAIL-CALLS
 * `_handle_hdlc_input_open` (`jmp`, 0x9e45c), so this returns whatever that
 * returns, which is 0.
 */
int
_send_hdlc_between_buffer_state_init(struct fax_class1 *ctx)
{
	ctx->countdown = 0;
	return _handle_hdlc_input_open(ctx);
}

/*
 * ------------------------------------------------------------------
 * The four remaining leaves, unblocked once `FAXVMI_control` landed
 * (F10115).  `V21RX_CTL` (0x7aa4, 16 bytes) and `V21TX_CTL` (0x7ae4, 20
 * bytes) are their own REINIT request templates, the V.21 control channel's
 * counterpart to `class1rx.c`'s `V17RX_CTL`/`V27RX_CTL`/`V29RX_CTL` -- raw
 * bytes taken with `objdump -s -j .data` against `ref/slmodemd/dsplibs.o`
 * and matched field-by-field against `v21fax.h`'s already-established
 * `struct v21rx_ctl`/`struct v21tx_ctl` (both typed from `V21RX_control`'s
 * and `V21TX_control`'s own reads, an earlier wave).  Zero relocations in
 * either template.  `unmapped_0000`'s leading dword is 300 (0x12c) on BOTH
 * -- V.21's own fixed 300 baud/bps rate, not a per-modulation placeholder
 * the caller overwrites the way the data modes' own templates are (F10116);
 * nothing here patches it.
 */
const struct v21rx_ctl V21RX_CTL = {
	{ 0x00, 0x00, 0x2c, 0x01 },	/* unmapped_0000 */
	60000,				/* int_0004      */
	{ 0, 0, 0, 0, 0 },		/* unmapped_0008 */
	0x00,				/* flags_0d      */
};

const struct v21tx_ctl V21TX_CTL = {
	{ 0x2c, 0x01, 0x00, 0x00 },	/* unmapped_0000 */
	60000,				/* int_0004      */
	3200,				/* int_0008      */
	0x00,				/* flags_0c      */
	0x00,				/* flags_0d      */
	{ 0, 0 },			/* unmapped_000e */
	{ 0, 0, 0, 0 },			/* unmapped_0010 */
};

/*
 * `_rx_look_carrier_init`, 0x9cb00, 45 bytes.  Three calls and one store,
 * nothing else: reinit the data-mode receiver, reset the async octet
 * recovery search, clear `countdown`.  Returns 0.
 */
int
_rx_look_carrier_init(struct fax_class1 *ctx, int rate_code)
{
	_init_receiver(ctx, rate_code);
	cTOOLS_handle_data_output_reset(ctx);
	ctx->countdown = 0;
	return 0;
}

/*
 * Forward tentative definition of `DATAtx_counter` -- its full comment and
 * the OTHER two readers/writers sharing it (`_tx_scrambled_ones_state`,
 * `_tx_data_state`) sit much further down this file, near where the object
 * itself is address-contiguous with them.  `_tx_scrambled_ones_init` needs
 * to clear it and is written up here beside the rest of the newly-landed
 * leaves; a second file-scope tentative definition of the same static is
 * ordinary C and resolves to the one object either way.
 */
static int DATAtx_counter;

/*
 * `_tx_scrambled_ones_init`, 0x9cf70, 193 bytes.  Reinit the data-mode
 * transmitter, then (re)build the transmit FIFO at a fixed 0x800-element
 * capacity -- `local = FIFO_CFG; local.size = 0x800; local.fill = 0;` is the
 * object's own field-by-field shape (a 32-bit copy of `FIFO_CFG`'s leading
 * `word0`/`size` pair, THEN both overridden, matching `faxfifo.h`'s own note
 * on that struct's aligned pair) -- and derive `ctx->tx_bytes_per_block` from the just-set
 * `ctx->tx_rate` as a plain signed divide by 400 (the object's own
 * `imul $0x51eb851f` / `sar $7` / sign-correct reciprocal for exactly that
 * divisor, independently re-derived rather than guessed).  Clears `tx_connect_countdown`
 * (one-shot connect countdown), `tx_connect_latch`, `transmit_enabled`, `tx_fifo_ready`,
 * `data_input_closed` and the file-static `DATAtx_counter`
 * (`_tx_scrambled_ones_state`'s own counter, above).
 */
int
_tx_scrambled_ones_init(struct fax_class1 *ctx, int rate_code)
{
	struct fifo_cfg local = FIFO_CFG;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("_tx_scrambled_ones_init\n");

	_init_transmitter(ctx, rate_code);

	local.size = 0x800;
	local.fill = 0;
	ctx->tx_connect_countdown = 0;
	ctx->tx_fifo = FIFO_create(ctx->tx_fifo, &local);

	ctx->tx_connect_latch = 0;
	ctx->transmit_enabled = 0;
	ctx->tx_fifo_ready = 0;
	ctx->tx_bytes_per_block = ctx->tx_rate / 400;
	ctx->data_input_closed = 0;
	DATAtx_counter = 0;
	return 0;
}

/*
 * `cHDLCtx_preamble_state_init`, 0x9e380, 193 bytes.  Merge `V21TX_CTL`
 * (REINIT bit OR'd into `flags_0d`) into a plain copy of `FAXVMI_CTL` --
 * `int_0014` is the only field this one touches, so no ring-clear, framer
 * reset or mode change reaches `FAXVMI_control`, unlike the RX-side sibling
 * below -- and send it to `ctx->vmi_c`, the V.21 TX handle.  Opens an HDLC
 * frame (return discarded) and resets the session to
 * `CLASS1_T30_SILENCE_BEFORE_PREAMBLE_STATE`.
 */
int
cHDLCtx_preamble_state_init(struct fax_class1 *ctx)
{
	struct v21tx_ctl req = V21TX_CTL;
	struct faxvmi_ctl ctl = FAXVMI_CTL;

	req.flags_0d |= V21TXCTL_REINIT;
	ctl.int_0014 = (int)(long)&req;

	FAXVMI_control(ctx->vmi_c, &ctl);
	_handle_hdlc_input_open(ctx);

	ctx->countdown = 0;
	ctx->state = CLASS1_T30_SILENCE_BEFORE_PREAMBLE_STATE;
	ctx->hdlc_frame_done = 0;
	ctx->buffers_sent = 0;
	ctx->frame_end_latch = 0;
	return 0;
}

/*
 * `_cHDLCrx_init_from_idle`, 0x9d790, 226 bytes.  TWO ARGUMENTS -- both
 * callers (`fax_class1_create`, `fax_class1_command`) supply a real second
 * one, and it is read: `arg2 == 3` both sets `state` to
 * `CLASS1_HDLC_RECEIVE_LOOK_CARRIER_STATE` (4) AND becomes this function's
 * own return value, discarding whatever `FAXVMI_control` returned -- a real
 * property of the object (`mov $0x4,%eax` on that path, untouched before
 * either `ret`), not a guess.  Merges `V21RX_CTL` (REINIT bit OR'd in) into a
 * `FAXVMI_ctl` that ALSO forces a full framer reset (`int_000c = 1`,
 * `short_0010 = 2`) and empties the ring (`ptr_0000 = (void *)1`), unlike the
 * TX-side sibling above, and sends it to `ctx->vmi_a`, the V.21 RX handle.
 * Clears `countdown` and `delayed_status_countdown` unconditionally and logs
 * "At %2d.%02d[sec]  HDLCrx_init_from_idle\n" (double space, the object's
 * own) at debug level > 1.
 */
int
_cHDLCrx_init_from_idle(struct fax_class1 *ctx, int arg2)
{
	struct v21rx_ctl req = V21RX_CTL;
	struct faxvmi_ctl ctl = FAXVMI_CTL;
	int ret;

	req.flags_0d |= V21RXCTL_REINIT;

	ctl.ptr_0000 = (void *)1;
	ctl.int_000c = 1;
	ctl.short_0010 = 2;
	ctl.int_0014 = (int)(long)&req;

	ret = FAXVMI_control(ctx->vmi_a, &ctl);

	if (arg2 == 3) {
		ctx->state = CLASS1_HDLC_RECEIVE_LOOK_CARRIER_STATE;
		ret = 4;
	}

	ctx->countdown = 0;
	ctx->delayed_status_countdown = 0;
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf(
		    "At %2d.%02d[sec]  HDLCrx_init_from_idle\n",
		    ctx->clock_sec, ctx->clock_frac);
	return ret;
}

/*
 * `cHDLCtx_off_init`, 0x9e9d0, 149 bytes.  THE ONE FINDING F8320'S BUCKET
 * ACTUALLY DESCRIBES: `objdump -r` over the whole 1.2 MB names it from
 * NOWHERE, no call and no stored handler address either (F8493's pair), so it
 * is orphaned exported API surface rather than a fax entry point's callee --
 * see class1tx.h for the reachability note.  Its own body is the exact
 * quiescent HALF of `_cHDLCrx_init_from_idle` immediately above: the same
 * `V21RX_CTL`-sourced `req` with the same `V21RXCTL_REINIT` bit forced, the
 * same full-framer-reset `ctl` (`ptr_0000 = 1`, `int_000c = 1`,
 * `short_0010 = 2`, `int_0014 = &req`) sent to the same `ctx->vmi_a`, but
 * WITHOUT the `arg2 == 3` state transition, WITHOUT touching
 * `delayed_status_countdown`, and WITHOUT the debug line -- `dis.py` shows no
 * second argument at all (one push, one `sub $0x48,%esp`, no comparison
 * against 3 anywhere in the 149 bytes) and no read of
 * `dsplibs_debug_level`.  `FAXVMI_control`'s return value is passed straight
 * through: the object's `%eax` is never touched between the `call` and the
 * final `ret`.  The only other effect is `ctx->countdown = 0` (`+0x1228`,
 * matching `_cHDLCrx_init_from_idle`'s own clear of the same field).
 */
int
cHDLCtx_off_init(struct fax_class1 *ctx)
{
	struct v21rx_ctl req = V21RX_CTL;
	struct faxvmi_ctl ctl = FAXVMI_CTL;
	int ret;

	req.flags_0d |= V21RXCTL_REINIT;

	ctl.ptr_0000 = (void *)1;
	ctl.int_000c = 1;
	ctl.short_0010 = 2;
	ctl.int_0014 = (int)(long)&req;

	ret = FAXVMI_control(ctx->vmi_a, &ctl);

	ctx->countdown = 0;
	return ret;
}

/*
 * A file-static the object increments once per byte examined and NEVER READS
 * -- `temp.0` at .bss+0x8c4, four bytes, with exactly one relocation against
 * it in the whole 1.2 MB (the `incl` at 0x09ebd5).  Kept because it is the
 * author's; it carries nothing.
 */
static int temp;

/*
 * DLE-unstuff the host's transmit buffer into 16-bit elements.
 *
 * `*count` is IN AND OUT: in it is how many bytes `src` holds, out it is how
 * many elements were written -- and once DLE ETX has been seen it answers
 * zero for ever after, without touching `dst`, because `data_input_closed`
 * stays set.
 *
 * TWO THINGS THAT LOOK LIKE MISTAKES AND ARE THE OBJECT'S.  The last byte of
 * the block is latched into `last_in_byte` BEFORE the loop, so it is recorded
 * even when the loop stops early at DLE ETX (D1054).  And after DLE ETX the
 * function appends twenty zero elements, each guarded by a limit on the
 * OUTPUT index rather than on the destination's size, so the destination must
 * hold `*count + 20` elements and the guard cannot help a smaller one
 * (D1055).
 */
int
_handle_data_input(struct fax_class1 *ctx, const unsigned char *src,
		   unsigned short *dst, int *count)
{
	int out = 0;
	int i;

	if (ctx->data_input_closed != 0) {
		*count = 0;
		return 0;
	}
	if (*count > 0)
		ctx->last_in_byte = src[*count - 1];

	for (i = 0; i < *count; i++) {
		temp++;
		if (ctx->dle_seen != 0) {
			if (dsplibs_debug_level > 2)
				dsplibs_debug_printf(
				    "CLASS1: DLE %1X in data\n", src[i]);
			ctx->dle_seen = 0;
			if (src[i] == CLASS1_DLE) {
				dst[out] = CLASS1_DLE;
				out++;
				continue;
			}
			if (src[i] == CLASS1_ETX) {
				int k;

				ctx->data_input_closed = 1;
				for (k = CLASS1_ETX_PAD_ELEMENTS - 1;
				     k >= 0; k--) {
					if (out > CLASS1_ETX_PAD_LIMIT)
						continue;
					dst[out] = 0;
					out++;
				}
				break;
			}
			continue;
		}
		if (src[i] == CLASS1_DLE) {
			ctx->dle_seen = 1;
			continue;
		}
		dst[out] = src[i];
		out++;
	}
	*count = out;
	return 0;
}

/*
 * Arm the frame cursor at ONE, not zero.  `_handle_hdlc_input` writes the
 * first octet at `dst[1]` and both it and `_handle_hdlc_input_close` report
 * `hdlc_write_cursor - 1` as the length, so element zero is the length slot the frame is
 * eventually length-prefixed with -- the same layout `faxvmi_frame_reverse`
 * and `faxvmi_write_frame` walk.  Five instructions, and it touches nothing
 * else.
 */
int
_handle_hdlc_input_open(struct fax_class1 *ctx)
{
	ctx->hdlc_write_cursor = 1;
	return 0;
}

int
_handle_hdlc_input_close(struct fax_class1 *ctx)
{
	ctx->scratch_frame_len = (short)(ctx->hdlc_write_cursor - 1);
	if (ctx->flags004 & CLASS1_FLAG_FRAME_END_LATCH)
		ctx->frame_end_latch = 1;
	return 0;
}

/*
 * The same unstuffing for an HDLC frame, and three things make it a different
 * function rather than a mode of the one above.
 *
 *   - The write cursor is the SESSION's (`hdlc_write_cursor`), not a local, so a frame
 *     accumulates across calls and the caller's `dst` is indexed from
 *     wherever the last call left off.  `dst` must therefore be sized for the
 *     whole frame, not for one block.
 *   - DLE ETX ends the FRAME: it does the same two stores
 *     `_handle_hdlc_input_close` does -- the length into `scratch_frame_len`, and `frame_end_latch`
 *     when the flag is on -- and reports 1.
 *   - `*count` comes back as the frame length on that call and as ZERO
 *     otherwise, which is what tells the caller a frame is not finished yet.
 *
 * A byte that is neither DLE nor ETX after a DLE is DROPPED, silently and
 * without the log line the data path prints. That is the object's.
 */
int
_handle_hdlc_input(struct fax_class1 *ctx, const unsigned char *src,
		   unsigned short *dst, int *count)
{
	int done = 0;
	int out = ctx->hdlc_write_cursor;
	int i;

	for (i = 0; i < *count; i++) {
		if (ctx->dle_seen != 0) {
			ctx->dle_seen = 0;
			if (src[i] == CLASS1_DLE) {
				dst[out] = CLASS1_DLE;
				out++;
				continue;
			}
			if (src[i] == CLASS1_ETX) {
				ctx->hdlc_write_cursor = out;
				ctx->scratch_frame_len = (short)(out - 1);
				if (ctx->flags004 & CLASS1_FLAG_FRAME_END_LATCH)
					ctx->frame_end_latch = 1;
				*count = out;
				done = 1;
				break;
			}
			continue;
		}
		if (src[i] == CLASS1_DLE) {
			ctx->dle_seen = 1;
			continue;
		}
		dst[out] = src[i];
		out++;
	}

	ctx->hdlc_write_cursor = out;
	if (done)
		return 1;
	*count = 0;
	return 0;
}

/*
 * The other direction: recover an octet from each received element, DLE-stuff
 * it, and hand the host a byte stream.  Returns how many bytes were written.
 *
 * THE OCTET RECOVERY IS AN ASYNCHRONOUS START-BIT SEARCH, and it is worth
 * stating exactly, because the arithmetic looks arbitrary otherwise.  Each
 * element contributes its LOW BYTE to the top of a 32-bit window whose lower
 * three bytes are the previous three (`async_window`).  While the alignment
 * is not locked, the search walks up from bit 16 for the first ZERO bit,
 * moving `async_mask` and `async_shift` with it, and the recovered octet is
 * the eight bits starting AT that zero -- so its least significant bit is the
 * start bit itself, which is the object's arrangement and not a slip in the
 * reading. Eight bits are tried; if all of them are one the search gives up,
 * the octet is 0xff, the alignment is NOT locked, and the mask and shift are
 * left where the search abandoned them for the next element to inherit
 * (D1056).
 *
 * Once locked, the mask and shift are frozen and every later element is
 * extracted through them, so the recovery costs one search per carrier.
 *
 * The stuffing is the standard one: a recovered 0x10 is written twice, and
 * `terminate` appends DLE ETX.  So the destination must hold
 * `2 * count + 2` bytes -- sized from the COUNT, never from `count`
 * (F8607/D956's shape).
 */
/*
 * Drop the start-bit search back to "not locked yet".  `async_locked` goes to
 * zero and `async_window` to -1, which is the all-ones history the search
 * wants before the first element arrives; `async_shift` and `async_mask` are
 * NOT touched, because `_handle_data_output` recomputes both on the call that
 * locks.  Void: the object leaves eax holding the argument and no caller can
 * be relying on that.
 */
void
cTOOLS_handle_data_output_reset(struct fax_class1 *ctx)
{
	ctx->async_locked = 0;
	ctx->async_window = 0xffffffffu;
}

int
_handle_data_output(struct fax_class1 *ctx, const unsigned short *src,
		    unsigned char *dst, int count, int terminate)
{
	int out = 0;
	int i;

	for (i = 0; i < count; i++) {
		unsigned char octet = 0xff;
		unsigned int window;

		window = ((unsigned int)(unsigned char)src[i] << 24)
		       | ctx->async_window;

		if (ctx->async_locked == 0) {
			unsigned int probe = 0x10000;
			int tried = 0;

			ctx->async_shift = 16;
			ctx->async_mask = 0xff0000;
			while ((window & probe) != 0) {
				ctx->async_mask <<= 1;
				tried++;
				probe += probe;
				ctx->async_shift++;
				if (tried > 7)
					goto emit;
			}
			ctx->async_locked = 1;
		}
		octet = (unsigned char)((window & ctx->async_mask)
					>> ctx->async_shift);
	emit:
		ctx->async_window = window >> 8;
		dst[out] = octet;
		out++;
		if (octet == CLASS1_DLE) {
			dst[out] = CLASS1_DLE;
			out++;
		}
	}

	if (terminate != 0) {
		dst[out] = CLASS1_DLE;
		dst[out + 1] = CLASS1_ETX;
		out += 2;
	}
	return out;
}

/*
 * The other other direction: recover an octet from each of `count` elements,
 * as `_handle_data_output` does, but WITHOUT its async start-bit search --
 * these are already-aligned HDLC receive elements, so the low byte of each
 * one is the octet.  DLE-stuff them into `dst` and append DLE ETX when
 * `terminate` is set.  `ctx` is read nowhere in the object; it is carried
 * only because every other member of this family takes it.
 *
 * FOUR `.rodata.str1.1` STRINGS, the author's own, gate the debug blocks
 * that the differential test cannot see but that are reproduced anyway --
 * they are the evidence for the function's DIRECTION, which its own name
 * does not give away:
 *
 *   "FCL1: FRAME RECEIVED (%s)\n"    (0x4926) -- printed once, when `count`
 *       is over 2 and the level allows it, naming the frame from its first
 *       three elements via `GetT30FrameIDFromBuffer`/`GetT30FrameNameByID`
 *       -- the same idiom `faxvmi_hdlc_frame`'s debug line uses on
 *       transmit.  So despite the symbol's own name, "hdlc_output" means
 *       OUTPUT TO THE HOST of a frame the modem RECEIVED, not one being
 *       sent -- the mirror of `_handle_hdlc_input`, which takes the HOST's
 *       bytes in.
 *   "HDLC Recieved Frame of %d: \n"  (0x4941, "Recieved" is the object's) --
 *       printed whenever the level allows it, whatever `count` is.
 *   "%02X,"                          (0x495e) -- once per output byte, loop
 *       and terminator alike.
 *   "%02X\n"                         (0x4964) -- once, ending the line, only
 *       when a terminator is actually appended.
 *
 * `dis.py` shows the "Recieved Frame" print (0x09f00a, one call site) reached
 * both from the `count > 2` arm, after the frame-decode print, AND from its
 * `else`, each guarded by its OWN `cmpl $0x1,dsplibs_debug_level` (0x09ef2e
 * and 0x09eff0 -- a direct compare-to-memory in one arm, a load into a
 * register first in the other) rather than one shared test after an
 * if/else, which is why the level is checked twice below rather than once.
 *
 * THE TERMINATOR WRITE IS GATED BY `terminate` ALONE, NOT BY `count` TOO
 * (F10100).  `count == 0` jumps STRAIGHT to the terminator check
 * (0x09ef21 `test %ebp,%ebp; je 0x9ef90`, and 0x9ef90 is exactly the
 * `terminate` test) -- `count != 0 && terminate != 0` gates only the LAST
 * TWO debug prints, a narrower condition folded from `setne`/`setne`/`test`
 * at 0x9ef6b-0x9ef7a.  A first pass wrote the write itself under the same
 * combined condition as the debug prints, which is wrong whenever `count`
 * is 0 and `terminate` is not -- caught by `t_class1hdlcemu.c` disagreeing
 * with the blob (word7 0 where the blob leaves 2: a zero-length record
 * still gets its DLE ETX), not by re-reading the disassembly a second time.
 */
int
cTOOLS_handle_hdlc_output(struct fax_class1 *ctx, const unsigned short *src,
			  unsigned char *dst, int count, int terminate)
{
	int out = 0;
	int i;

	(void)ctx;

	if (count != 0) {
		if (count > 2) {
			if (dsplibs_debug_level > 1) {
				int id = GetT30FrameIDFromBuffer(
				    (unsigned char)src[0],
				    (unsigned char)src[1],
				    (unsigned char)src[2]) & 0xffff7fff;

				dsplibs_debug_printf(
				    "FCL1: FRAME RECEIVED (%s)\n",
				    GetT30FrameNameByID(id));
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "HDLC Recieved Frame of %d: \n",
					    count);
			}
		} else if (dsplibs_debug_level > 1) {
			dsplibs_debug_printf(
			    "HDLC Recieved Frame of %d: \n", count);
		}

		for (i = 0; i < count; i++) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf("%02X,",
				    aReversedCharsArray[(unsigned char)src[i]]);
			dst[out++] = (unsigned char)src[i];
			if (src[i] == CLASS1_DLE) {
				dst[out++] = CLASS1_DLE;
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf("%02X,",
					    aReversedCharsArray[CLASS1_DLE]);
			}
		}
	}

	if (count != 0 && terminate != 0 && dsplibs_debug_level > 1) {
		dsplibs_debug_printf("%02X,", aReversedCharsArray[CLASS1_DLE]);
		dsplibs_debug_printf("%02X\n", aReversedCharsArray[CLASS1_ETX]);
	}
	if (terminate != 0) {
		dst[out] = CLASS1_DLE;
		dst[out + 1] = CLASS1_ETX;
		out += 2;
	}
	return out;
}

/*
 * ------------------------------------------------------------------
 * The transmit-side VMI constructors, `.text` 0x094870/0x094970/0x094a70 --
 * see class1tx.h for why they belong here rather than in `class1rx.c`
 * beside their RX siblings.
 *
 * SAME SHAPE AS `class1rx.c`'s trio: allocate a config of exactly its
 * table's size, announce at `DSPLIB_DEBUG_VERBOSE()`, copy the table over
 * the allocation, override `bitrate` and the caller's fourth argument, then
 * copy `FAXVMI_CFG` over the caller's VMI and override seven fields
 * (`short_0008`/`short_000a` differ by modulation; `slot` is what makes
 * them three functions).  Neither allocation is checked for NULL, as on
 * the RX side.
 *
 * WHAT DIFFERS FROM THE RX TRIO.  Every RX constructor stores the fourth
 * argument into the VMI's own `ptr_0014` (and, for V.17, into a *config*
 * field too); all three TX constructors ALSO store it into the config's
 * OWN LAST FIELD (`int_001c`/`int_0018`, `struct v29tx_cfg`'s `int_0018`
 * one field earlier, per `v27fax.h`'s own comment on the shape) -- the
 * `(void *)(long)` idiom faxadapt.h names, since that field is declared
 * `int` in each config struct and not a pointer.  And each RETURNS
 * `(int)cfg->bitrate`, reloaded from the freshly-built config right before
 * `ret` -- dead code under `-O3` unless the source has an explicit `return`,
 * so unlike the RX trio (`void`) these three are `int`.
 *
 * A SECOND HARDCODED FIELD, missed on the first read of the disassembly and
 * caught by `t_class1txvmi.c` disagreeing with the blob rather than assumed
 * absent: after the six/seven-dword table copy, all three OVERRIDE the
 * FIFO size factor's low 16 bits with a literal 16-bit store (`movw`) --
 * `struct v17tx_cfg::fifo_size_factor` here, still `int_0014` at the same
 * offset in `struct v27tx_cfg`/`struct v29tx_cfg` (neither renamed by this
 * pass; see `v17fax.h`'s own note on why V.17's copy was and V.29's was
 * not, finding F10144) -- `0x1` for V.17, `0x2` for V.27ter and V.29.
 * V.17's table default is already 1, so that one is invisible to any test
 * that only checks the FINAL value; V.27ter's and V.29's tables are also 1
 * (`tabdump.py` over the blob's own `.data` confirms it, independent of
 * either reconstructed table), so their override to 2 is a real, visible
 * change from the default. `cfg->fifo_size_factor = <value>;` (or
 * `cfg->int_0014 = <value>;` for the other two) after the table copy
 * reproduces this -- a plain `int` assignment stores the same final 32 bits
 * as the object's narrower `movw`, since the upper 16 bits are already zero
 * from the dword copy, so no encoding trick is needed for behavioural
 * fidelity.
 */
int
init_vmi_v17tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
	       int arg_2, void *arg_3)
{
	struct v17tx_cfg *cfg = sysdep_malloc(sizeof(struct v17tx_cfg));

	(void)arg_2;

	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf(
			"Initializing VMI_V17_TX Modem No ECM "
			"(Simple Packing)\n");

	*cfg = V17TX_CFG;
	cfg->bitrate = bit_rate;
	cfg->fifo_size_factor = 1;
	cfg->int_0018 = 0;
	cfg->int_001c = (int)(long)arg_3;

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->short_0000 = 0;
	vmi->int_0004 = 1;
	vmi->short_0008 = 0x60;
	vmi->short_000a = 0x30;
	vmi->short_000c = 0;
	vmi->slot = VMI_SLOT_V17TX;
	vmi->modem_cfg = cfg;

	return cfg->bitrate;
}

int
init_vmi_v29tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
	       int arg_2, void *arg_3)
{
	struct v29tx_cfg *cfg = sysdep_malloc(sizeof(struct v29tx_cfg));

	(void)arg_2;

	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf(
			"Initializing VMI_V29_TX Modem No ECM "
			"(Simple Packing)\n");

	*cfg = V29TX_CFG;
	cfg->bitrate = bit_rate;
	cfg->int_0014 = 2;
	cfg->int_0018 = (int)(long)arg_3;

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->short_0000 = 0;
	vmi->int_0004 = 1;
	vmi->short_0008 = 0x60;
	vmi->short_000a = 0x35;
	vmi->short_000c = 0;
	vmi->slot = VMI_SLOT_V29TX;
	vmi->modem_cfg = cfg;

	return cfg->bitrate;
}

int
init_vmi_v27tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
	       int arg_2, void *arg_3)
{
	struct v27tx_cfg *cfg = sysdep_malloc(sizeof(struct v27tx_cfg));

	(void)arg_2;

	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf(
			"Initializing VMI_V27_TX Modem No ECM "
			"(Simple Packing)\n");

	*cfg = V27TX_CFG;
	cfg->bitrate = bit_rate;
	cfg->int_0014 = 2;
	cfg->int_001c = (int)(long)arg_3;

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->short_0000 = 0;
	vmi->int_0004 = 1;
	vmi->short_0008 = 0x40;
	vmi->short_000a = 0x25;
	vmi->short_000c = 0;
	vmi->slot = VMI_SLOT_V27TX;
	vmi->modem_cfg = cfg;

	return cfg->bitrate;
}

/*
 * Tear the transmit-side data modem down.  The object's own order: free the
 * config, free the VMI block, clear `ctx->modem_vmi`, `FAXVMI_delete` the
 * handle at `ctx->vmi_b`, clear `ctx->vmi_b`, and only THEN look at
 * `ctx->tx_fifo` (a FIFO) -- `FIFO_delete` it and clear the field when it is
 * non-null, or just clear it when it is already null.  No modulation's
 * config here needs a sub-allocation freed first, unlike the RX side's
 * V.17.
 */
void
_delete_data_tx_modem(struct fax_class1 *ctx)
{
	struct faxvmi_cfg *vmi = ctx->modem_vmi;
	struct faxvmi *handle;

	sysdep_free(vmi->modem_cfg);
	vmi = ctx->modem_vmi;
	sysdep_free(vmi);

	handle = ctx->vmi_b;
	ctx->modem_vmi = NULL;
	FAXVMI_delete(handle);
	ctx->vmi_b = NULL;

	if (ctx->tx_fifo != NULL)
		FIFO_delete(ctx->tx_fifo);
	ctx->tx_fifo = NULL;
}

/*
 * `init_vmi_data_tx_modem[mod]`, `.data` 0x792c, 12 bytes -- LOCAL in the
 * object (`d`, `nm`), so `static` here.  Three `R_386_32` relocations,
 * `nm`-resolved: index 0 V.27ter, 1 V.29, 2 V.17, the same order
 * `_init_transmitter`'s own inlined `_set_modem_rate`-shaped derivation
 * below produces.  Independently re-confirmed against `objdump -r`
 * (F10109 first reported this table; not taken on that report alone here).
 */
static int (*const init_vmi_data_tx_modem[3])(struct faxvmi_cfg *,
					       unsigned short, int, void *) = {
	init_vmi_v27tx,
	init_vmi_v29tx,
	init_vmi_v17tx,
};

/*
 * The three per-modulation control-request templates `_init_transmitter`
 * merges with the all-zero `FAXVMI_CTL` before calling `FAXVMI_control` --
 * ONLY for a V.17 SHORT-TRAINING rate code (`ebp` in the disassembly), and
 * then regardless of whether the modem was just freshly built or already
 * existed (see `_init_transmitter`'s own derivation).  Raw bytes taken with
 * `objdump -s -j .data` against `ref/slmodemd/dsplibs.o` and matched
 * field-by-field against each type's own established offsets (`v17fax.h`'s
 * `struct v17tx_control_req`, `v27fax.h`'s new `struct v27tx_ctl`,
 * `v29fax.h`'s `struct v29tx_control_req`).  Zero relocations in any of the
 * three (F10109).
 *
 * `pad_0000`/`unmapped_0000`'s own LOW 16 bits hold a per-modulation
 * PLACEHOLDER bit rate (0x3840/0x2580/0x2580 -- V.17/V.27ter/V.29 -- note
 * this is the OPPOSITE half from the receive-side templates, which put the
 * placeholder in the UPPER 16 bits) that `_init_transmitter` overwrites with
 * the live negotiated rate (`mov %di,...` on the HIGH 16 this time); the
 * flags/ctl1 byte's REINIT bit is OR'd in at runtime, not baked into the
 * constant.
 */
const struct v17tx_control_req V17TX_CTL = {
	{ 0x40, 0x38, 0x00, 0x00 },	/* pad_0000  */
	60000,				/* int_0004  */
	1,				/* scale_mul */
	0x00,				/* ctl0      */
	0x00,				/* ctl1      */
	{ 0, 0 },			/* pad_000e  */
	0,				/* int_0010  */
};

const struct v27tx_ctl V27TX_CTL = {
	{ 0x80, 0x25, 0x00, 0x00 },	/* unmapped_0000 */
	60000,				/* int_0004      */
	1,				/* scale_mul     */
	0x00,				/* mask          */
	0x00,				/* flags         */
	{ 0, 0 },			/* unmapped_000e */
	0,				/* int_0010      */
};

const struct v29tx_control_req V29TX_CTL = {
	{ 0x80, 0x25, 0x00, 0x00 },	/* pad_0000 */
	60000,				/* int_0004 */
	1,				/* int_0008 */
	0x00,				/* ctl0     */
	0x00,				/* ctl1     */
};

/*
 * `_init_transmitter`, 0x094bf0, 1,326 bytes.  See `class1tx.h` for the
 * derivation summary; this is the object's own control flow read straight
 * off `dis.py`, not tidied.  `rate_code` is the same T.30 modem-rate code
 * space `_init_receiver` (class1rx.c) reads, and the mod/rate/`f1230`/
 * `f1234` derivation duplicates `_set_modem_rate`'s own inline shape again
 * (no relocation to that function in this range).
 *
 * UNLIKE THE RECEIVE SIDE, there is no `ctx->current_mod` check here at
 * all: whenever `ctx->modem_vmi` AND `ctx->vmi_b` are both already set, the
 * existing modem is ALWAYS torn down and rebuilt fresh, regardless of
 * whether the newly requested modulation is the same one -- confirmed by
 * grepping this function's own disassembly for every reference to
 * `ctx->current_mod` (0x1248): the only two are the diagnostic print at
 * entry and the write at the very end.  `ebp` (a V.17 SHORT-TRAINING rate
 * code, 0x4a/0x62/0x7a/0x92) instead gates a SEPARATE, optional
 * control-request call that runs regardless of fresh-vs-existing, targeting
 * whichever modem is live after the (possible) rebuild above it.
 */
void
_init_transmitter(struct fax_class1 *ctx, int rate_code)
{
	int mod = 0;
	int rate = 0;
	int is_v17_short = 0;
	struct faxvmi_cfg *cfg;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"%2d.%02d[sec] Initializing TX modem, "
			"MODEM_IDX = %d, silence %d ms\n",
			ctx->clock_sec, ctx->clock_frac, ctx->current_mod,
			ctx->silence_blocks);

	if (rate_code == 0x4a || rate_code == 0x62 || rate_code == 0x7a ||
	    rate_code == 0x92) {
		is_v17_short = 1;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"Short train in V.17 mode is selected\n");
	}

	/* `_set_modem_rate`'s own shape (class1.c), inlined, extended with
	 * the `f1230`/`f1234` writes this function alone makes             */
	if ((unsigned)(rate_code - 0x91) <= 1) {
		ctx->f1230 = 0x30;
		ctx->f1234 = 0x8000;
		rate = 0x3840;
		mod = 2;
	}
	if ((unsigned)(rate_code - 0x79) <= 1) {
		ctx->f1230 = 0x30;
		ctx->f1234 = 0x8000;
		rate = 0x2ee0;
		mod = 2;
	}
	if ((unsigned)(rate_code - 0x61) <= 1) {
		ctx->f1230 = 0x18;
		ctx->f1234 = 0x8000;
		rate = 0x2580;
		mod = 2;
	}
	if ((unsigned)(rate_code - 0x49) <= 1) {
		ctx->f1230 = 0x18;
		ctx->f1234 = 0x8000;
		rate = 0x1c20;
		mod = 2;
	}
	if (rate_code == 0x60) {
		ctx->f1234 = 0x8000;
		ctx->f1230 = 0x18;
		rate = 0x2580;
		mod = 1;
	} else if (rate_code == 0x48) {
		ctx->f1234 = 0x75a2;
		ctx->f1230 = 0x18;
		rate = 0x1c20;
		mod = 1;
	} else if (rate_code == 0x30) {
		ctx->f1234 = 0x4000;
		ctx->f1230 = 0x0c;
		rate = 0x12c0;
		mod = 0;
	} else if (rate_code == 0x18) {
		ctx->f1234 = 0x4000;
		ctx->f1230 = 0x06;
		rate = 0x960;
		mod = 0;
	}

	if (ctx->modem_vmi != NULL && ctx->vmi_b != NULL) {
		/* ALWAYS torn down and rebuilt -- no modulation-match check */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"%2d.%02d[sec] New TX Modem... "
				"Deleting previous existing one\n",
				ctx->clock_sec, ctx->clock_frac);

		sysdep_free(ctx->modem_vmi->modem_cfg);
		sysdep_free(ctx->modem_vmi);
		ctx->modem_vmi = NULL;
		FAXVMI_delete(ctx->vmi_b);
		ctx->vmi_b = NULL;

		if (ctx->tx_fifo != NULL)
			FIFO_delete(ctx->tx_fifo);
		ctx->tx_fifo = NULL;
	}

	if (ctx->vmi_b == NULL) {
		if (ctx->modem_vmi == NULL)
			ctx->modem_vmi = sysdep_malloc(
				sizeof(struct faxvmi_cfg));

		init_vmi_data_tx_modem[mod](ctx->modem_vmi,
					    (unsigned short)rate, 0, NULL);

		cfg = ctx->modem_vmi;
		ctx->vmi_b = FAXVMI_create(NULL, cfg);
	}

	if (is_v17_short) {
		struct faxvmi_ctl ctl = FAXVMI_CTL;

		cfg = ctx->modem_vmi;

		if (cfg->slot == VMI_SLOT_V17TX) {
			struct v17tx_control_req req = V17TX_CTL;

			*(short *)((char *)&req + 2) = (short)rate;
			req.ctl1 |= V17TXCTL_CTL1_BIT1;
			ctl.int_0014 = (int)(long)&req;
			FAXVMI_control(ctx->vmi_b, &ctl);
		} else if (cfg->slot == VMI_SLOT_V29TX) {
			struct v29tx_control_req req = V29TX_CTL;

			*(short *)((char *)&req + 2) = (short)rate;
			req.ctl1 |= V29TXCTL_CTL1_BIT1;
			ctl.int_0014 = (int)(long)&req;
			FAXVMI_control(ctx->vmi_b, &ctl);
		} else {
			struct v27tx_ctl req = V27TX_CTL;

			*(short *)((char *)&req + 2) = (short)rate;
			req.flags |= V27TXCTL_FLAGS_REINIT;
			ctl.int_0014 = (int)(long)&req;
			FAXVMI_control(ctx->vmi_b, &ctl);
		}
	}

	ctx->tx_rate = rate;
	ctx->state = (ctx->silence_blocks == 0)
			     ? CLASS1_TX_SCRAMBLED_ONES_STATE
			     : CLASS1_TX_SILENCE_BEFORE_SCRM_ONES;
	ctx->current_mod = mod;
	ctx->countdown = 0;
}

/*
 * TX_SILENCE_BEFORE_SCRM_ONES.  `.text` 0x09d720, 111 bytes.  Nothing but a
 * countdown bump, an optional debug line (the object's own literal string,
 * no format arguments), a silence block, and a state transition once
 * `countdown` catches up with `silence_blocks` -- both compared as
 * `unsigned` (the object's `cmp`/`jb`), which the usual arithmetic
 * conversions give for free since `silence_blocks` is already
 * `unsigned int`.
 */
int
_tx_silence_before_scrm_ones(struct fax_class1 *ctx, const short *rx,
			     short *tx, int word3, int word4,
			     int *rx_count, int *tx_count, int word7,
			     int *word8)
{
	(void)rx;
	(void)word3;
	(void)word4;
	(void)rx_count;
	(void)word7;

	ctx->countdown += 20;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("Tx silence before scrambled ones ...\n");

	_put_silence(tx, CLASS1_BLOCK_SAMPLES);
	*tx_count = CLASS1_BLOCK_SAMPLES;

	if (ctx->countdown >= ctx->silence_blocks)
		ctx->state = CLASS1_TX_SCRAMBLED_ONES_STATE;

	*word8 = 0;
	return 0;
}

/*
 * T30_SILENCE_BEFORE_TX_STATE.  `.text` 0x09e590, 194 bytes.
 *
 * `_put_silence`'s return value is COMPUTED and then discarded -- the very
 * next instruction (`mov 0x1228(%ebx),%eax`, ctx->countdown reloaded for
 * the threshold compare below) overwrites the register holding it before
 * anything reads it, on every path.  An earlier reading of this function
 * mistook that reload for the call's return value surviving to the final
 * store and got `n + *tx_count` (320 on the common path) where the object
 * gives a plain accumulation (160) -- caught by `t_class1delete.c`
 * disagreeing with the blob, not assumed correct from the disassembly
 * alone.
 *
 * The comparison against 400 is `unsigned` (`cmp`/`jbe`/`ja`), the same
 * shape `_recieve_silence_state`'s own `countdown` compare already
 * established needs an explicit cast to reach from a plain `int`.
 */
int
_t30_silence_before_tx_state(struct fax_class1 *ctx, const short *rx,
			     short *tx, int word3, int word4,
			     int *rx_count, int *tx_count, int word7,
			     int *word8)
{
	(void)rx;
	(void)word3;
	(void)word4;
	(void)rx_count;
	(void)word7;
	(void)word8;

	_put_silence(tx, CLASS1_BLOCK_SAMPLES);
	*tx_count = CLASS1_BLOCK_SAMPLES;

	if ((unsigned int)ctx->countdown > 400) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "%2d.%02d[sec], Elapsed 50MS second, "
			    "send preamble\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->state = CLASS1_T30_PREAMBLE_STATE;
		ctx->countdown = 0;
		ctx->status = FAX_CLASS1_CONNECT;
	}
	ctx->countdown += *tx_count;
	return 0;
}

/*
 * HDLC_EMULATE_RECEIVE_STATE.  `.text` 0x09e1b0, 452 bytes.  See class1tx.h
 * for the shape and class1.h for `superframe`/`superframe_countdown`/`superframe_read_idx`/`superframe_len`.
 *
 * FIRST, PARSE.  Walk `ctx->superframe` from the start, `count` records of
 * `ctx->superframe_len` bytes total -- entry `i` is a length, the data follows, the
 * next record starts right after.  Each length is banked into `lens[]` (see
 * `CLASS1_EMU_MAX_FRAMES`'s own comment for why it is exactly twelve long
 * and unguarded) so the SECOND walk below, to find where record `next`
 * starts, does not have to re-read the buffer.
 *
 * `ctx->prev_state != CLASS1_HDLC_EMULATE_RECEIVE_STATE` is "is this the
 * first tick since some other state entered this one" -- on that tick only,
 * a still-unsent record (`next < count`) reports FAX_CLASS1_CONNECT once.
 *
 * THE COUNTDOWN, next.  `old` is `superframe_countdown` BEFORE this call's decrement; a
 * value that was already <= 0 is what fires the next record (or, with
 * nothing left to send, the transition to IDLE_STATE with
 * FAX_CLASS1_NO_CARRIER_NO_MESSAGE -- both spelled `8` in the object, one
 * a state and the other a status, and that coincidence is why a single
 * `mov $0x8` in the disassembly feeds two different stores).  Emitting a
 * record calls `cTOOLS_handle_hdlc_output` on it, writes the byte count
 * through `word7`, arms a two-tick delayed status, advances `superframe_read_idx`, and
 * also moves to IDLE_STATE.
 *
 * FINALLY, the object's own tail runs whether or not anything fired above:
 * once `next` has caught up to `count`, the whole buffer resets (`superframe_len` to
 * 0, `superframe_countdown` to 2, `superframe_read_idx` to 0) -- note this is an EQUALITY test in the
 * object (`je`), not `next >= count`, so it is written that way here too --
 * and every path ends the same: a block of silence out, `*tx_count` set to
 * it, return 0.
 */
int
_hdlc_emulate_receive_state(struct fax_class1 *ctx, const short *rx,
			    short *tx, int word3, int word4, int *rx_count,
			    int *tx_count, int word7, int *word8)
{
	int total = ctx->superframe_len;
	int idx = 0;
	int count = 0;
	int lens[CLASS1_EMU_MAX_FRAMES];
	int next;
	int old;

	(void)rx;
	(void)word4;
	(void)rx_count;
	(void)word8;

	while (idx < total) {
		int len = ctx->superframe[idx];

		lens[count] = len;
		count++;
		idx += len + 1;
	}

	next = ctx->superframe_read_idx;
	if (ctx->prev_state != CLASS1_HDLC_EMULATE_RECEIVE_STATE) {
		if (next < count)
			ctx->status = FAX_CLASS1_CONNECT;
	}

	old = ctx->superframe_countdown;
	ctx->superframe_countdown = old - 1;

	if (old <= 0) {
		if (next >= count) {
			ctx->status = FAX_CLASS1_NO_CARRIER_NO_MESSAGE;
			ctx->state = CLASS1_IDLE_STATE;
		} else {
			int start = 0, i;
			int n;

			for (i = 0; i < next; i++)
				start += lens[i] + 1;

			n = cTOOLS_handle_hdlc_output(ctx,
			    &ctx->superframe[start + 1],
			    (unsigned char *)(long)word3,
			    ctx->superframe[start], 1);
			*(int *)(long)word7 = n;
			ctx->delayed_status = 1;
			ctx->delayed_status_countdown = 2;
			next++;
			ctx->superframe_read_idx = next;
			ctx->state = CLASS1_IDLE_STATE;
		}
	}

	if (next == count) {
		ctx->superframe_len = 0;
		ctx->superframe_countdown = 2;
		ctx->superframe_read_idx = 0;
	}

	_put_silence(tx, CLASS1_BLOCK_SAMPLES);
	*tx_count = CLASS1_BLOCK_SAMPLES;
	return 0;
}

/*
 * ------------------------------------------------------------------
 * THE REMAINING TWELVE.  Inserted here, after `_hdlc_emulate_receive_state`
 * (this file's last function by both position and, until now, by address) --
 * this file is not in whole-file address order already (the VMI-constructor
 * trio above sits far out of the order its own 0x0948xx addresses would
 * imply, per that section's own comment), so a single clean append at EOF is
 * the insertion point that disturbs the least of what is already here.
 * Internally the twelve are grouped by RELATED FUNCTION rather than strict
 * ascending address (`_tx_scrambled_ones_state` and `_tx_data_state` in
 * particular share the `DATAtx_counter` static and sit together for that
 * reason, out of strict order) -- each function's own `.text` address and
 * size is stated in its own comment below and is what is authoritative, not
 * its position in the file.  Every one of the twelve is a `class1_state_fn`
 * (class1.h); addresses and sizes are confirmed against
 * `nm -S ref/slmodemd/dsplibs.o`.
 *
 * A raw status bit shared by five of them (`_rx_look_carrier_state` twice,
 * `_rx_data_state`, `cHDLCtx_off`, `_hdlc_receive_state`, `_hdlc_receive_
 * between_buffers_state`): `test $0x20,%ah` on `FAXVMI_process`'s raw `int`
 * return, i.e. bit 0x2000.  `faxvmi.h`'s own note on `FAXVMI_process` says
 * the low 24 bits of that return are the WRAPPED MODULATION's own result
 * word, untouched by `FAXVMI_process` itself -- so this is some status bit
 * of whichever modem the VMI handle in play currently wraps.  Its ROLE is
 * not uniform across call sites (in `cHDLCtx_off` it swaps which of two
 * countdown thresholds applies; elsewhere SET reads as "carrier/frame still
 * present", CLEAR as "lost"), so it is named only as a bit position, not a
 * meaning -- evidence class 3, usage inference, corroborated by nothing
 * stronger.
 */
#define FAXVMI_RESULT_BIT_2000	0x2000

/*
 * RX_LOOK_CARRIER (12).  `.text` 0x0009cb30, 624 bytes.
 *
 * Drives `ctx->vmi_b` every call and, only when `vmi_b`'s own
 * `FAXVMI_RESULT_BIT_2000` comes back CLEAR, polls `ctx->vmi_a` too (both
 * calls pass `ctx` itself, cast `(unsigned short *)(void *)ctx`, as
 * FAXVMI_process's `data` argument with `count` seeded 0 -- inert, the same
 * idiom `cHDLCtx_off` already established).  If `vmi_a`'s own bit comes back
 * SET, that reads as "an HDLC frame arrived while looking for a DATA
 * carrier" (the object's own debug line, quoted below) -- transition to
 * HDLC_RECEIVE_STATE with FAX_CLASS1_OTHER_CARRIER, but keep going: the
 * object falls through into the SAME `status1`-bit-8/9 test below whichever
 * way the `vmi_a` poll went, using `status1` (the FIRST call's return, never
 * overwritten by the second).
 *
 * `(status1 >> 8) & 0x3 == 1` reads as "V.21 detected a valid start of
 * protocol" -- one call site, one value tested, so this stays a bare
 * expression rather than a named bit (usage inference, too weak to
 * generalise).  On a match: FAX_CLASS1_CONNECT, RX_DATA_STATE.
 *
 * THE S7 (CARRIER-WAIT) TIMEOUT.  `ctx->s7_timeout * 8000 / *rx_count`
 * (or, when `*rx_count == 0`, the object's own divide-by-zero guard,
 * `ctx->s7_timeout * 50` -- exactly what the general formula reduces to at
 * the usual 160-sample block and an implied 8 kHz rate, corroborating
 * rather than a second derivation) is compared against `ctx->countdown`
 * (incremented once per call, UNSIGNED per the object's own `cmp`/`jbe`);
 * once countdown exceeds it AND `status1`'s bit is still clear, the object's
 * own line is "S7 time elapsed in look carrier\n" -- FAX_CLASS1_NO_CARRIER,
 * IDLE_STATE.  A zero limit skips this block entirely (object's own `test
 * edi,edi`/`je`).
 *
 * `*word8 != 0` on entry aborts to the host: IDLE_STATE, `_idle_state_init`,
 * a zero-length `cTOOLS_handle_hdlc_output` (DLE ETX only, from `ctx+2` --
 * inert, `count` is 0) whose byte count goes through `word7`
 * (`(int *)(long)word7`, the same idiom `_hdlc_emulate_receive_state`
 * established), and FAX_CLASS1_OK.
 *
 * Every path ends the same: `*word8 = 5` (the SAME magic value
 * `_recieve_silence_state` already writes there, per class1.h's own
 * unresolved note on what word8 is -- corroborating, not resolving, that
 * note), `_put_silence(tx, *rx_count)` -- NOT the `CLASS1_BLOCK_SAMPLES`
 * constant, the caller's own count -- and `*tx_count = *rx_count`.
 *
 * FORMAT STRINGS, verified byte for byte against `.rodata.str1.4`:
 *   0x12210  "TxDatCnt !=0 in _rx_look_carrier_state... abort to command mode\n"
 *   0x12254  "At %2d.%02d[sec] Data RX connect in _rx_look_carrier_state\n"
 *   0x12290  "At %2d.%02d[sec] HDLC frame detected during look for DATA carrier !!!\n"
 *   0x122d8  "S7 time elapsed in look carrier\n"
 */
int
_rx_look_carrier_state(struct fax_class1 *ctx, const short *rx, short *tx,
		       int word3, int word4, int *rx_count, int *tx_count,
		       int word7, int *word8)
{
	short cnt1 = 0, cnt2 = 0;
	unsigned short result1 = (unsigned short)*rx_count;
	unsigned short result2 = (unsigned short)*rx_count;
	int status1, status2;
	int limit;
	int sub;

	(void)word4;

	limit = (*rx_count == 0) ? ctx->s7_timeout * 50
				: ctx->s7_timeout * 8000 / *rx_count;
	ctx->countdown++;

	status1 = FAXVMI_process(ctx->vmi_b, (unsigned short *)(void *)ctx,
				 (short *)rx, &cnt1, &result1);

	if (!(status1 & FAXVMI_RESULT_BIT_2000)) {
		status2 = FAXVMI_process(ctx->vmi_a,
					 (unsigned short *)(void *)ctx,
					 (short *)rx, &cnt2, &result2);
		if (status2 & FAXVMI_RESULT_BIT_2000) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "At %2d.%02d[sec] HDLC frame detected "
				    "during look for DATA carrier !!!\n",
				    ctx->clock_sec, ctx->clock_frac);
			_hdlc_receive_state_init(ctx);
			ctx->state = CLASS1_HDLC_RECEIVE_STATE;
			ctx->status = FAX_CLASS1_OTHER_CARRIER;
		}
	}

	sub = (status1 >> 8) & 0x3;
	if (sub == 1) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec] Data RX connect in "
			    "_rx_look_carrier_state\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->status = FAX_CLASS1_CONNECT;
		ctx->state = CLASS1_RX_DATA_STATE;
	}

	if (limit != 0 &&
	    (unsigned int)ctx->countdown > (unsigned int)limit &&
	    !(status1 & FAXVMI_RESULT_BIT_2000)) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("S7 time elapsed in look "
					     "carrier\n");
		ctx->status = FAX_CLASS1_NO_CARRIER;
		ctx->state = CLASS1_IDLE_STATE;
	}

	if (*word8 != 0) {
		int n;

		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "TxDatCnt !=0 in _rx_look_carrier_state... "
			    "abort to command mode\n");
		ctx->state = CLASS1_IDLE_STATE;
		_idle_state_init(ctx);
		n = cTOOLS_handle_hdlc_output(ctx,
		    (const unsigned short *)((char *)ctx + 2),
		    (unsigned char *)(long)word3, 0, 1);
		*(int *)(long)word7 = n;
		ctx->status = FAX_CLASS1_OK;
	}

	*word8 = 5;
	_put_silence(tx, *rx_count);
	*tx_count = *rx_count;
	return 0;
}

/*
 * RX_DATA_STATE (13).  `.text` 0x0009cda0, 433 bytes.
 *
 * Drives `ctx->vmi_b` with `ctx` as `data` (the same idiom, but NOT inert
 * here: `word7`'s target seeds the call's `count`, so `FAXVMI_process`'s
 * unpack step really does write demodulated elements into `ctx`'s own
 * leading bytes -- `_handle_data_output` then reads them straight back out
 * as its own `src`, both within `pad_005[0xffb]` for any plausible block
 * size).  `word7` is read (`*(int *)(long)word7`) to seed the call, and
 * WRITTEN at the end with the byte count `_handle_data_output`/
 * `cTOOLS_handle_hdlc_output` returns -- the same `int *` role
 * `_hdlc_emulate_receive_state` established, confirmed here independently.
 *
 * `FAXVMI_RESULT_BIT_2000` SET reads as "carrier present, keep streaming":
 * `_handle_data_output(ctx, ctx, word3, cnt, terminate=0)`.  CLEAR reads as
 * "carrier lost": log, IDLE_STATE, `_idle_state_init`, the SAME call with
 * `terminate=1` (closing the host stream with DLE ETX), a delayed
 * FAX_CLASS1_NO_CARRIER two calls out, and -- ONLY on this branch, nested
 * inside it, not a sibling -- the same `*word8 != 0` "abort to command mode"
 * block `_rx_look_carrier_state` has (IDLE_STATE, `_idle_state_init`, a
 * zero-length `cTOOLS_handle_hdlc_output` from `ctx+2`, FAX_CLASS1_OK).  The
 * object's own control flow makes the abort check UNREACHABLE when the bit
 * is SET, and this is written the same way.
 *
 * Same tail as `_rx_look_carrier_state`: `*word8 = 5`,
 * `_put_silence(tx, *rx_count)`, `*tx_count = *rx_count`.
 *
 * FORMAT STRINGS:
 *   0x122fc  "At %2d.%02d[sec] No carrier in _rx_data_state, move to idle\n"
 *   0x1233c  "TxDatCnt !=0 in _rx_data_state... abort to command mode\n"
 */
int
_rx_data_state(struct fax_class1 *ctx, const short *rx, short *tx,
	      int word3, int word4, int *rx_count, int *tx_count,
	      int word7, int *word8)
{
	short cnt = (short)*(int *)(long)word7;
	unsigned short result = (unsigned short)*rx_count;
	int status;
	int n;

	(void)word4;

	ctx->countdown++;
	status = FAXVMI_process(ctx->vmi_b, (unsigned short *)(void *)ctx,
				(short *)rx, &cnt, &result);

	if (status & FAXVMI_RESULT_BIT_2000) {
		n = _handle_data_output(ctx, (unsigned short *)(void *)ctx,
		    (unsigned char *)(long)word3, cnt, 0);
		cnt = (short)n;
	} else {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec] No carrier in _rx_data_state, "
			    "move to idle\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->state = CLASS1_IDLE_STATE;
		_idle_state_init(ctx);

		n = _handle_data_output(ctx, (unsigned short *)(void *)ctx,
		    (unsigned char *)(long)word3, cnt, 1);
		cnt = (short)n;

		ctx->delayed_status_countdown = 2;
		ctx->delayed_status = FAX_CLASS1_NO_CARRIER;

		if (*word8 != 0) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "TxDatCnt !=0 in _rx_data_state... "
				    "abort to command mode\n");
			ctx->state = CLASS1_IDLE_STATE;
			_idle_state_init(ctx);
			n = cTOOLS_handle_hdlc_output(ctx,
			    (const unsigned short *)((char *)ctx + 2),
			    (unsigned char *)(long)word3, 0, 1);
			cnt = (short)n;
			ctx->status = FAX_CLASS1_OK;
		}
	}

	*(int *)(long)word7 = cnt;
	*word8 = 5;
	_put_silence(tx, *rx_count);
	*tx_count = *rx_count;
	return 0;
}

/*
 * TX_NULLS_STATE (11).  `.text` 0x0009d040, 435 bytes.
 *
 * REPLACES A FIRST INTEGRATION PASS whose cited format strings did not
 * survive an `objdump -s -j .rodata.str1.4` re-check (they read as
 * plausible paraphrases, not the object's own bytes) -- rewritten from a
 * fresh `dis.py` trace with every string confirmed by address.
 *
 * `ctx->countdown++`; past 250 (unsigned), log ("CURRENT_STATE_TIMER > "
 * "FIVE_SECONDS in _tx_nulls_state\n", 0 args) and set IDLE_STATE /
 * FAX_CLASS1_ERROR_ON_HOOK -- but keep going into the block below either way
 * (this is not a return).
 *
 * `*word8 > 0`: log ("Back to TX_DATA_STATE in _tx_nulls_state\n"), set
 * `ctx->state = CLASS1_TX_DATA_STATE`, unstuff `word4` through
 * `_handle_data_input` into `ctx` itself (the scratch-buffer idiom, `dst`
 * cast from `ctx`) with `count = word8`, then `FIFO_write` the just-decoded
 * run into `ctx->tx_fifo`, logging a shortfall ("Fifo is full in
 * _tx_nulls_state\n"), THEN -- still inside this same block, not a shared
 * step -- `FIFO_read(ctx->tx_fifo, ctx, ctx->tx_bytes_per_block)` back into `ctx`, `*word8`
 * set to the return, a shortfall against `ctx->tx_bytes_per_block` logged ("class1
 * object fifo under run in _tx_nulls_state !!!\n").  On the `*word8 <= 0`
 * path NONE of this runs -- the object's own `jle` jumps straight past the
 * whole block (confirmed by address: `9d088`'s `jle` target is `9d120`, the
 * `FAXVMI_process`-setup label, not `9d0e2` where `FIFO_read` lives) -- so
 * `*word8` there is left holding whatever the CALLER passed in, unread.
 *
 * Either way: `FAXVMI_process(ctx->vmi_b, ctx, tx, &cnt, &result)` with
 * `cnt` seeded from the CURRENT `*word8` (freshly read, or the caller's
 * original value) and `result` from `*tx_count`.
 *
 * Tail: `*word8 = ctx->tx_fifo->size - ctx->tx_fifo->count - 1` (the free-room-
 * minus-one formula `_tx_data_state`/`_tx_scrambled_ones_state` also end
 * with).  `*tx_count` is READ, never written, on any path.
 *
 * FORMAT STRINGS, verified against `.rodata.str1.4` (`objdump -s`):
 *   0x12378  "Fifo is full in _tx_nulls_state\n"
 *   0x1239c  "At %2d.%02d[sec] Back to TX_DATA_STATE in _tx_nulls_state\n"
 *   0x123d8  "CURRENT_STATE_TIMER > FIVE_SECONDS in _tx_nulls_state\n"
 *   0x12410  "class1 object fifo under run in _tx_nulls_state !!!\n"
 */
int
_tx_nulls_state(struct fax_class1 *ctx, const short *rx, short *tx,
		int word3, int word4, int *rx_count, int *tx_count,
		int word7, int *word8)
{
	short cnt;
	unsigned short result;

	(void)rx;
	(void)word3;
	(void)rx_count;
	(void)word7;

	ctx->countdown++;
	if ((unsigned int)ctx->countdown > 250) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "CURRENT_STATE_TIMER > FIVE_SECONDS in "
			    "_tx_nulls_state\n");
		ctx->state = CLASS1_IDLE_STATE;
		ctx->status = FAX_CLASS1_ERROR_ON_HOOK;
	}

	if (*word8 > 0) {
		int n;

		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec] Back to TX_DATA_STATE in "
			    "_tx_nulls_state\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->state = CLASS1_TX_DATA_STATE;

		_handle_data_input(ctx, (const unsigned char *)(long)word4,
		    (unsigned short *)(void *)ctx, word8);

		n = FIFO_write(ctx->tx_fifo, (unsigned short *)(void *)ctx,
		    (unsigned short)*word8);
		if (*word8 > n) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "Fifo is full in _tx_nulls_state\n");
		}

		/*
		 * `FIFO_read` is INSIDE this block, not a shared unconditional
		 * step -- the object's own `jle` at `*word8 <= 0` jumps
		 * straight past it to the `FAXVMI_process` setup, so on that
		 * path `cnt` below is seeded from `*word8`'s ORIGINAL
		 * (unread) value, not a fresh read.  A first version of this
		 * function ran `FIFO_read` unconditionally, which reads as
		 * "ctx (as data) is FIFO-read-filled on every call" and is
		 * wrong on any call where `*word8 <= 0` -- caught by this
		 * wave's own `t_class1txstates.c` disagreeing with the blob
		 * (a zeroed ctx-as-buffer where the object leaves it
		 * untouched), not assumed correct from a first disassembly
		 * pass.
		 */
		*word8 = FIFO_read(ctx->tx_fifo, (unsigned short *)(void *)ctx,
		    ctx->tx_bytes_per_block);
		if (*word8 < ctx->tx_bytes_per_block) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "class1 object fifo under run in "
				    "_tx_nulls_state !!!\n");
		}
	}

	cnt = (short)*word8;
	result = (unsigned short)*tx_count;
	FAXVMI_process(ctx->vmi_b, (unsigned short *)(void *)ctx, tx, &cnt,
	    &result);

	*word8 = ctx->tx_fifo->size - ctx->tx_fifo->count - 1;
	return 0;
}

/*
 * TX_DATA_STATE (10).  `.text` 0x0009d4b0, 618 bytes.
 *
 * `*word8 > 0` unstuffs `word4` through `_handle_data_input` into `ctx`,
 * `count = word8`, then `FIFO_write`s it into `ctx->tx_fifo`; a shortfall
 * ("Fifo is full in _tx_data_state(%d=>%d>%d)\n") is logged but does NOT
 * skip the read below (falls straight through).  Either way, `FIFO_read`s
 * `ctx->tx_bytes_per_block` elements from `ctx->tx_fifo` into `ctx`, logs a shortfall
 * ("fifo underrun in _tx_data_state, count %d < %d\n") but keeps going, and
 * drives `FAXVMI_process(ctx->vmi_b, ctx, tx, &count, &result)` with
 * `count` seeded from the read and `result` seeded from `*tx_count` (NOT
 * zero -- the one difference from the TX pair above).
 *
 * The raw status is tested against the two ALREADY-NAMED `FAXVMI_STATUS_*`
 * bits (`faxvmi.h`): `FAXVMI_STATUS_UNDERRUN` (0x01000000) takes priority
 * over `FAXVMI_STATUS_FULL` (0x02000000), which is only checked (for a log
 * line, "Queue is full in _tx_data_state\n") when underrun is clear.
 *
 * UNDERRUN, `ctx->last_in_byte != 0` (an ALREADY-established field): log
 * ("Queue underrun, Stop TX\n"), IDLE_STATE, FAX_CLASS1_OK_NO_CARRIER.
 * UNDERRUN, `ctx->last_in_byte == 0`: log ("Queue underrun, Continue
 * NULLS\n"), TX_NULLS_STATE, `ctx->countdown = 0`, FAX_CLASS1_CONNECT.
 * Both then fall through to the (possibly-logged) FULL check before the
 * common tail.
 *
 * Tail: the SAME free-room-minus-one formula, `*word8 = ctx->tx_fifo->size -
 * ctx->tx_fifo->count - 1`.  `*tx_count` is never written past its `result`
 * seed being read back into it -- i.e. never explicitly re-stored, matching
 * the object, which only ever writes `*word8`.
 *
 * FORMAT STRINGS, verified against `.rodata.str1.1`/`.rodata.str1.4`:
 *   0x48c2 (.str1.1)  "cDATAtx_counter %d\n" (shares `DATAtx_counter` above)
 *   0x12504  "At %2d.%02d[sec] Fifo is full in _tx_data_state(%d=>%d>%d)\n"
 *   0x12540  "At %2d.%02d[sec] Queue is full in _tx_data_state\n"
 *   0x12574  "At %2d.%02d[sec] fifo underrun in _tx_data_state, "
 *            "count %d < %d\n"
 *   0x125b8  "At %2d.%02d[sec] Queue underrun, Stop TX\n"
 *   0x125e4  "At %2d.%02d[sec] Queue underrun, Continue NULLS\n"
 */

/*
 * A `.bss` counter shared by `_tx_scrambled_ones_state` (below) and
 * `_tx_data_state` (here) -- one relocation from each, both against the
 * SAME four-byte object.  The object's own debug line names it:
 * "cDATAtx_counter %d\n" (evidence class 1; the leading `c` is the line's
 * own first character, kept verbatim).  `_tx_scrambled_ones_state` also
 * reads it back to fire a one-time CONNECT on the session's first call
 * into this pair; `_tx_data_state` only increments it.
 */
static int DATAtx_counter;

int
_tx_data_state(struct fax_class1 *ctx, const short *rx, short *tx,
	       int word3, int word4, int *rx_count, int *tx_count,
	       int word7, int *word8)
{
	short cnt;
	unsigned short result = (unsigned short)*tx_count;
	int status;
	int orig_word8 = *word8;

	(void)rx;
	(void)word3;
	(void)rx_count;
	(void)word7;

	if (dsplibs_debug_level > 2)
		dsplibs_debug_printf("cDATAtx_counter %d\n", DATAtx_counter);
	DATAtx_counter++;

	if (*word8 > 0) {
		int n;

		_handle_data_input(ctx, (const unsigned char *)(long)word4,
		    (unsigned short *)(void *)ctx, word8);

		n = (short)FIFO_write(ctx->tx_fifo, (unsigned short *)(void *)ctx,
		    (unsigned short)*word8);
		if (n < *word8) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "At %2d.%02d[sec] Fifo is full in "
				    "_tx_data_state(%d=>%d>%d)\n",
				    ctx->clock_sec, ctx->clock_frac,
				    orig_word8, *word8, n);
		}
	}

	{
		int rd = (short)FIFO_read(ctx->tx_fifo,
		    (unsigned short *)(void *)ctx, ctx->tx_bytes_per_block);

		if (rd < ctx->tx_bytes_per_block) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "At %2d.%02d[sec] fifo underrun in "
				    "_tx_data_state, count %d < %d\n",
				    ctx->clock_sec, ctx->clock_frac, rd,
				    ctx->tx_bytes_per_block);
		}
		cnt = (short)rd;
	}

	status = FAXVMI_process(ctx->vmi_b, (unsigned short *)(void *)ctx, tx,
	    &cnt, &result);

	if (status & FAXVMI_STATUS_UNDERRUN) {
		if (ctx->last_in_byte != 0) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "At %2d.%02d[sec] Queue underrun, Stop "
				    "TX\n",
				    ctx->clock_sec, ctx->clock_frac);
			ctx->state = CLASS1_IDLE_STATE;
			ctx->status = FAX_CLASS1_OK_NO_CARRIER;
		} else {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "At %2d.%02d[sec] Queue underrun, "
				    "Continue NULLS\n",
				    ctx->clock_sec, ctx->clock_frac);
			ctx->state = CLASS1_TX_NULLS_STATE;
			ctx->countdown = 0;
			ctx->status = FAX_CLASS1_CONNECT;
		}
	}

	if (status & FAXVMI_STATUS_FULL) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec] Queue is full in "
			    "_tx_data_state\n",
			    ctx->clock_sec, ctx->clock_frac);
	}

	*word8 = ctx->tx_fifo->size - ctx->tx_fifo->count - 1;
	return 0;
}

/*
 * HDLC_RECEIVE_STATE (5).  `.text` 0x0009d8d0, 738 bytes.
 *
 * A previous entry via HDLC_RECEIVE_BETWEEN_BUFFERS_STATE
 * (`ctx->prev_state == CLASS1_HDLC_RECEIVE_BETWEEN_BUFFERS_STATE`)
 * pre-sets `ctx->status = FAX_CLASS1_CONNECT` before falling into the
 * common body.  `ctx->countdown++` guards against the object's own
 * (essentially unreachable in practice -- it requires `countdown == -1`
 * going in) `!= 0` check; reproduced as written rather than simplified,
 * since simplifying it would be editing the object's logic rather than its
 * expression.
 *
 * `ctx->scratch_frame_len` is cleared to 0, then `FAXVMI_process(ctx->vmi_a, ctx, rx,
 * &count, &result)` is driven with `result` seeded from `*rx_count` and
 * `count` seeded 0 -- so a nonzero `count` on return means the unpack step
 * really did write a length-prefixed record into `ctx` (element 0 is the
 * length, matching the SAME convention `ctx->superframe` uses for
 * `_hdlc_emulate_receive_state`).
 *
 * `FAXVMI_RESULT_BIT_2000` CLEAR: log ("No carrier in HDLC receive
 * state\n"), IDLE_STATE, a zero-length `cTOOLS_handle_hdlc_output` (DLE ETX
 * only) whose count goes through `word7`, a delayed FAX_CLASS1_NO_CARRIER
 * two calls out -- then re-tests `count` exactly as the SET arm below (the
 * object's own `jmp` back into that test).
 *
 * BIT SET, `count == 0`: nothing more to do this call.
 * BIT SET, `count != 0`, `ctx->scratch_frame_len == 0` (the length prefix, now the
 * FIRST unpacked element): a framing/CRC ERROR -- log ("Receive buffer with
 * error in _hdlc_receive_state\n"), a delayed FAX_CLASS1_ERROR two calls
 * out, then join the OK arm's tail (state = HDLC_RECEIVE_BETWEEN_BUFFERS_
 * STATE) WITHOUT the `rx_agc_mult`/`rx_agc_shift` pointer chase below.
 * BIT SET, `count != 0`, `ctx->scratch_frame_len != 0`: OK -- log ("Receive buffer OK in
 * _hdlc_receive_state\n"), `cTOOLS_handle_hdlc_output(ctx, ctx+2, word3,
 * ctx->scratch_frame_len, 1)` (the frame's own bytes, length-prefixed the same way
 * `_hdlc_emulate_receive_state`'s records are) whose count goes through
 * `word7`, a delayed FAX_CLASS1_OK two calls out, THEN the pointer chase:
 * `ctx->vmi_a->link->int_0014` (an untyped "active modem" handle per
 * `faxvmi.h`'s own note) to its own +0x50, and the sign-extended shorts at
 * +0x30/+0x32 of THAT into `ctx->rx_agc_mult`/`rx_agc_shift` -- V.21RX's internal layout
 * at those two offsets is out of this batch's scope, so this is raw offset
 * arithmetic, not a named struct access; evidence class 3.  Either way,
 * `ctx->state = CLASS1_HDLC_RECEIVE_BETWEEN_BUFFERS_STATE`.
 *
 * `*word8 != 0` (checked AFTER all of the above): IDLE_STATE, `_idle_state_
 * init`, a zero-length `cTOOLS_handle_hdlc_output`, FAX_CLASS1_OK.  Tail:
 * `*word8 = 5`, `_put_silence(tx, CLASS1_BLOCK_SAMPLES)`, `*tx_count =
 * CLASS1_BLOCK_SAMPLES`.
 *
 * FORMAT STRINGS, verified against `.rodata.str1.4`:
 *   0x12698  "%2d.%02d[sec] TxDatCnt>0 in _hdlc_receive_state... "
 *            "abort command mode\n"
 *   0x126e0  "At %2d.%02d[sec] No carrier in HDLC receive state\n"
 *   0x12714  "%2d.%02d[sec] Receive buffer OK in _hdlc_receive_state\n"
 *   0x1274c  "%2d.%02d[sec] Receive buffer with error in "
 *            "_hdlc_receive_state\n"
 */
int
_hdlc_receive_state(struct fax_class1 *ctx, const short *rx, short *tx,
		    int word3, int word4, int *rx_count, int *tx_count,
		    int word7, int *word8)
{
	short cnt = 0;
	unsigned short result = (unsigned short)*rx_count;
	int status;

	(void)word4;
	(void)rx_count;

	if (ctx->prev_state == CLASS1_HDLC_RECEIVE_BETWEEN_BUFFERS_STATE)
		ctx->status = FAX_CLASS1_CONNECT;

	if (++ctx->countdown != 0) {
		ctx->scratch_frame_len = 0;

		status = FAXVMI_process(ctx->vmi_a,
		    (unsigned short *)(void *)ctx, (short *)rx, &cnt, &result);

		if (!(status & FAXVMI_RESULT_BIT_2000)) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "At %2d.%02d[sec] No carrier in HDLC "
				    "receive state\n",
				    ctx->clock_sec, ctx->clock_frac);
			ctx->state = CLASS1_IDLE_STATE;
			*(int *)(long)word7 = cTOOLS_handle_hdlc_output(ctx,
			    (const unsigned short *)((char *)ctx + 2),
			    (unsigned char *)(long)word3, 0, 1);
			ctx->delayed_status_countdown = 2;
			ctx->delayed_status = FAX_CLASS1_NO_CARRIER;
		}

		if (cnt != 0) {
			unsigned short len = *(unsigned short *)(void *)ctx;

			if (len == 0) {
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "%2d.%02d[sec] Receive buffer "
					    "with error in "
					    "_hdlc_receive_state\n",
					    ctx->clock_sec, ctx->clock_frac);
				ctx->delayed_status_countdown = 2;
				ctx->delayed_status = FAX_CLASS1_ERROR;
			} else {
				void *modem;
				char *p;

				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "%2d.%02d[sec] Receive buffer OK "
					    "in _hdlc_receive_state\n",
					    ctx->clock_sec, ctx->clock_frac);
				*(int *)(long)word7 =
				    cTOOLS_handle_hdlc_output(ctx,
				    (const unsigned short *)((char *)ctx + 2),
				    (unsigned char *)(long)word3, len, 1);
				ctx->delayed_status_countdown = 2;
				ctx->delayed_status = FAX_CLASS1_OK;

				modem = (void *)(long)ctx->vmi_a->link->int_0014;
				p = *(char **)((char *)modem + 0x50);
				ctx->rx_agc_mult = *(short *)(p + 0x30);
				ctx->rx_agc_shift = *(short *)(p + 0x32);
			}
			ctx->state = CLASS1_HDLC_RECEIVE_BETWEEN_BUFFERS_STATE;
		}
	}

	if (*word8 != 0) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "%2d.%02d[sec] TxDatCnt>0 in "
			    "_hdlc_receive_state... abort command mode\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->state = CLASS1_IDLE_STATE;
		_idle_state_init(ctx);
		*(int *)(long)word7 = cTOOLS_handle_hdlc_output(ctx,
		    (const unsigned short *)((char *)ctx + 2),
		    (unsigned char *)(long)word3, 0, 1);
		ctx->status = FAX_CLASS1_OK;
	}

	*word8 = 5;
	_put_silence(tx, CLASS1_BLOCK_SAMPLES);
	*tx_count = CLASS1_BLOCK_SAMPLES;
	return 0;
}

/*
 * HDLC_RECEIVE_BETWEEN_BUFFERS_STATE (6).  `.text` 0x0009dbc0, 566 bytes.
 *
 * `ctx->scratch_frame_len = 0` unconditionally, BEFORE the call -- the SAME pre-clear
 * `_hdlc_receive_state` does (a first integration pass read this as "NOT
 * pre-cleared here", which a fresh `dis.py` re-check does not support: see
 * this function's own code comment for the address).  Then
 * `FAXVMI_process(ctx->vmi_a, ctx, rx, &count, &result)`, `result` seeded
 * from `*rx_count`, `count` seeded 0.
 *
 * `count != 0`: this is the WRITER side of `ctx->superframe`'s length-prefixed
 * record convention `_hdlc_emulate_receive_state` already reads --
 * append the just-unpacked record (length `ctx->scratch_frame_len`, elements from
 * `ctx+2`) to `ctx->superframe` at cursor `ctx->superframe_len`, bounds-checked against
 * 0xff total bytes (the object's own limit; over it, log "SuperFrame full,
 * skipping HDLC frame!\n" -- the author's own name for `ctx->superframe`,
 * `.rodata` evidence -- and drop the record without advancing the cursor).
 *
 * `FAXVMI_RESULT_BIT_2000` CLEAR: FAX_CLASS1_NO_CARRIER_NO_MESSAGE (8),
 * IDLE_STATE (8) -- the SAME coincidence `_hdlc_emulate_receive_state`'s own
 * comment already notes.
 *
 * `*word8 != 0`: log ("Missing HDLC frame of %d during command mode(%d
 * already in)!\n" -- printed EARLIER, gated on `count != 0` and debug > 1,
 * with `ctx->scratch_frame_len` and the PRE-append `ctx->superframe_len` as its two `%d`s; kept
 * here as a plain debug line since it does not gate any behaviour), then
 * IDLE_STATE, `_idle_state_init`, a zero-length `cTOOLS_handle_hdlc_output`,
 * FAX_CLASS1_OK.
 *
 * Tail: `*word8 = 5`, `_put_silence(tx, CLASS1_BLOCK_SAMPLES)`, `*tx_count =
 * CLASS1_BLOCK_SAMPLES`.
 *
 * FORMAT STRINGS, verified against `.rodata.str1.4`:
 *   0x1278c  "SuperFrame full, skipping HDLC frame!\n"
 *   0x127b4  "%2d.%02d[sec] Missing HDLC frame of %d during command "
 *            "mode(%d already in)!\n"
 *   0x12800  "%2d.%02d[sec] No carrier during command mode, "
 *            "NO MESSAGE****\n"
 *   0x12840  "%2d.%02d[sec] TxDatCnt>0 in "
 *            "_hdlc_receive_between_buffers_state abort command mode.\n"
 */
int
_hdlc_receive_between_buffers_state(struct fax_class1 *ctx, const short *rx,
				    short *tx, int word3, int word4,
				    int *rx_count, int *tx_count, int word7,
				    int *word8)
{
	short cnt = 0;
	unsigned short result = (unsigned short)*rx_count;
	int status;

	(void)word4;
	(void)rx_count;

	/*
	 * `ctx->scratch_frame_len = 0` unconditionally, BEFORE the call -- confirmed by a
	 * fresh `dis.py` re-check (`9dbea: movw $0x0,(%edi)`, %edi = ctx,
	 * ahead of the `FAXVMI_process` setup).  A first integration pass
	 * read this as "not pre-cleared here, unlike `_hdlc_receive_state`",
	 * which is wrong -- caught by this wave's own `t_class1txstates.c`
	 * disagreeing with the blob (garbage `fill()` bytes surviving at
	 * ctx+0 where the object leaves zero), not assumed correct from a
	 * first disassembly pass.
	 */
	ctx->scratch_frame_len = 0;

	status = FAXVMI_process(ctx->vmi_a, (unsigned short *)(void *)ctx, (short *)rx,
				&cnt, &result);

	if (cnt != 0) {
		unsigned short len = *(unsigned short *)(void *)ctx;
		int new_len = ctx->superframe_len + len + 1;

		/*
		 * Printed whenever `cnt != 0`, BEFORE the bounds check below
		 * -- the object's own debug line is not conditional on
		 * whether the copy that follows actually fits (traced
		 * instruction by instruction: the `call` to
		 * `dsplibs_debug_printf` precedes the `cmp`/bounds branch in
		 * the object).  A first integration pass had this nested
		 * inside the "fits" arm only, which is wrong -- fixed here.
		 */
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "%2d.%02d[sec] Missing HDLC frame of "
			    "%d during command mode(%d already in)!\n",
			    ctx->clock_sec, ctx->clock_frac, len,
			    ctx->superframe_len);

		if (new_len > 0xff) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "SuperFrame full, skipping HDLC "
				    "frame!\n");
		} else {
			unsigned short *sf = ctx->superframe + ctx->superframe_len;
			unsigned short *src = (unsigned short *)(void *)ctx
					     + 1;
			int i;

			*sf = len;
			for (i = 0; i < len; i++)
				sf[1 + i] = src[i];
			ctx->superframe_len += len + 1;
		}
	}

	if (!(status & FAXVMI_RESULT_BIT_2000)) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "%2d.%02d[sec] No carrier during command "
			    "mode, NO MESSAGE****\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->status = FAX_CLASS1_NO_CARRIER_NO_MESSAGE;
		ctx->state = CLASS1_IDLE_STATE;
	}

	if (*word8 != 0) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "%2d.%02d[sec] TxDatCnt>0 in "
			    "_hdlc_receive_between_buffers_state abort "
			    "command mode.\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->state = CLASS1_IDLE_STATE;
		_idle_state_init(ctx);
		*(int *)(long)word7 = cTOOLS_handle_hdlc_output(ctx,
		    (const unsigned short *)((char *)ctx + 2),
		    (unsigned char *)(long)word3, 0, 1);
		ctx->status = FAX_CLASS1_OK;
	}

	*word8 = 5;
	_put_silence(tx, CLASS1_BLOCK_SAMPLES);
	*tx_count = CLASS1_BLOCK_SAMPLES;
	return 0;
}

/*
 * A signal-level threshold table, `.rodata` 0xba22, four `unsigned short`
 * entries -- `514, 727, 1026, 1450` -- read off the object's own bytes
 * (`objdump -s`).  Referenced only from `_hdlc_receive_look_carrier_state`
 * this batch, so kept `static` rather than declared in a header; nothing
 * else here reaches it.
 */
static const unsigned short HDLC_LOOK_CARRIER_LEVELS[4] = {
	514, 727, 1026, 1450,
};

/*
 * HDLC_RECEIVE_LOOK_CARRIER_STATE (4).  `.text` 0x0009de00, 943 bytes -- the
 * largest of the twelve.
 *
 * THE S7 TIMEOUT.  Same formula and same field as `_rx_look_carrier_state`:
 * `ctx->s7_timeout * 8000 / *rx_count` (or `* 50` when `*rx_count == 0`).
 * `ctx->countdown++`, then `FAXVMI_process(ctx->vmi_a, ctx, rx, &count,
 * &result)`, `result` seeded from `*rx_count`, `count` seeded 0.
 *
 * `FAXVMI_RESULT_BIT_2000` SET: skip the S7 check, go straight to the
 * tone-cadence tail.  CLEAR: if `limit != 0` and `ctx->countdown > limit`
 * (unsigned) and `ctx->cng_enabled == 0`, log ("S7 time elapsed in look
 * carrier\n"), FAX_CLASS1_NO_CARRIER, IDLE_STATE, `ctx->cng_enabled = 0`
 * (redundant, already 0); either way fall to the tone-cadence tail.
 *
 * THE TONE-CADENCE TAIL (see class1.h for `tone_cadence_phase`/`tone_cadence_timer`/`cng_enabled`).
 *   `cng_enabled == 0`: plain `_put_silence(tx, CLASS1_BLOCK_SAMPLES)`.
 *   `cng_enabled != 0`, `tone_cadence_phase == 0` (silence phase): `_put_silence`, then
 *     accumulate `tone_cadence_timer += CLASS1_BLOCK_SAMPLES`; once it exceeds 0x5dc0
 *     (24000), reset `tone_cadence_timer = 0` and flip `tone_cadence_phase = 1`.
 *   `cng_enabled != 0`, `tone_cadence_phase != 0` (tone phase): `FPM_TONE_generate(ctx->tone,
 *     tx, CLASS1_BLOCK_SAMPLES)` instead of silence, then accumulate
 *     `tone_cadence_timer` the same way against 0xfa0 (4000), flipping `tone_cadence_phase` back to 0
 *     on overflow.  Either phase sets `*tx_count = CLASS1_BLOCK_SAMPLES`.
 *
 * `FAXVMI_RESULT_BIT_2000` SET (checked separately, right after the
 * countdown/`FAXVMI_process` call, BEFORE the S7 logic above -- the object's
 * own branch order): if the raw status's bit 0/1 pair `(status >> 8) & 0x3`
 * -- no, this function does NOT read that pair; instead it walks
 * `ctx->vmi_a->link->int_0014`'s own +0x50, up to FOUR signed 16-bit reads
 * at +0x2c of the pointer chain (re-chased each iteration, since the object
 * re-reads it every loop pass rather than hoisting it), each compared
 * against `HDLC_LOOK_CARRIER_LEVELS[i]` -- the FIRST index `i` (0..3) where
 * the chased value is LESS than the table entry stops the scan; ELSE (no
 * match in 4 tries) the scan is abandoned silently and this whole "CONNECT"
 * arm is skipped.  On a match: `ctx->gain_attenuation_db = 12 - 3*i` (the object's own
 * countdown, ecx, of 12/9/6/3/0) is stored (an ALREADY-established field,
 * `fax_class1_info(0)`'s own; this is a SECOND writer, evidence class 3 --
 * see class1.h's existing note on it), logged ("Gain Attenuation Reuqest:
 * +%d[dB], avg_rms = %d.\n", `ecx`, sign-extended low byte of the chased
 * value -- object's own typo "Reuqest" kept verbatim), and THEN (whether or
 * not that inner debug line fired): log ("Carrier Detected in
 * _hdlc_receive_look_carrier_state\n"), `ctx->status = FAX_CLASS1_CONNECT`,
 * log ("At %2d.%02d[sec] hdlc_receive_state_init\n" -- narrating a
 * transition this function inlines rather than calling out to), `ctx->
 * countdown = 0`, `ctx->state = CLASS1_HDLC_RECEIVE_STATE`, `ctx->cng_enabled =
 * 0`, and the SAME `ctx->vmi_a->link->int_0014`/+0x50 chase
 * `_hdlc_receive_state` makes, but reading +0x2c this time (once, not per
 * table entry) -- discarded here, spent only on the table scan above; no
 * ctx field receives it in THIS function.
 *
 * `*word8 != 0`: IDLE_STATE, `_idle_state_init`, a zero-length
 * `cTOOLS_handle_hdlc_output` from `ctx+2` whose count goes through
 * `word7`, FAX_CLASS1_OK.  Tail: `*word8 = 5`, return 0.
 *
 * FORMAT STRINGS, verified against `.rodata.str1.4`:
 *   0x1266c  "At %2d.%02d[sec] hdlc_receive_state_init\n"
 *   0x12898  "Gain Attenuation Reuqest: +%d[dB], avg_rms = %d"
 *   0x128c8  "At %2d.%02d[sec] Carrier Detected in "
 *            "_hdlc_receive_look_carrier_state\n"
 *   0x12910  "At %2d.%02d[sec] TxDatCnt>0 in "
 *            "_hdlc_receive_look_carrier_state abort command mode.\n"
 *   0x12968  "At %2d.%02d[sec], curent_timeout = %d, No carrier in "
 *            "_hdlc_receive_look_carrier_state, S7 = %d[sec]\n"
 *
 * THE POINTER CHASE (`ctx->vmi_a->link->int_0014`, then that pointer's own
 * +0x50, then +0x2c or +0x30/+0x32 of THAT) is the same shape
 * `_hdlc_receive_state` uses; see that function's own note.  V.21RX's
 * internal layout at these offsets is out of this batch's scope, so this is
 * raw offset arithmetic, not a named struct access -- evidence class 3.
 */
int
_hdlc_receive_look_carrier_state(struct fax_class1 *ctx, const short *rx,
				 short *tx, int word3, int word4,
				 int *rx_count, int *tx_count, int word7,
				 int *word8)
{
	short cnt = 0;
	unsigned short result = (unsigned short)*rx_count;
	int status;
	int limit;

	(void)word4;

	limit = (*rx_count == 0) ? ctx->s7_timeout * 50
				: ctx->s7_timeout * 8000 / *rx_count;
	ctx->countdown++;

	status = FAXVMI_process(ctx->vmi_a, (unsigned short *)(void *)ctx, (short *)rx,
				&cnt, &result);

	if (status & FAXVMI_RESULT_BIT_2000) {
		void *modem;
		char *p;
		int i;

		/*
		 * UNCONDITIONAL on entering this arm, regardless of whether
		 * the table scan below finds anything -- the object sets
		 * these BEFORE the scan even starts (0x9df14..0x9df5f), not
		 * as a consequence of a match.
		 */
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec] Carrier Detected in "
			    "_hdlc_receive_look_carrier_state\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->status = FAX_CLASS1_CONNECT;
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec] hdlc_receive_state_init\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->countdown = 0;
		ctx->state = CLASS1_HDLC_RECEIVE_STATE;
		ctx->cng_enabled = 0;

		/*
		 * THE SCAN.  Re-chased every iteration (the object re-reads
		 * the whole pointer chain each pass rather than hoisting
		 * it).  A match stores `gain_attenuation_db` and logs; running out of
		 * table entries (4 tries) abandons the scan silently -- either
		 * way execution falls through to the tone-cadence tail below.
		 */
		for (i = 0; i <= 3; i++) {
			short v;

			modem = (void *)(long)ctx->vmi_a->link->int_0014;
			p = *(char **)((char *)modem + 0x50);
			v = *(short *)(p + 0x2c);

			if (v < (short)HDLC_LOOK_CARRIER_LEVELS[i]) {
				ctx->gain_attenuation_db = 12 - 3 * i;
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "Gain Attenuation Reuqest: "
					    "+%d[dB], avg_rms = %d",
					    ctx->gain_attenuation_db, (int)(signed char)v);
				break;
			}
		}
	} else if (limit != 0 && (unsigned int)ctx->countdown >
		   (unsigned int)limit && ctx->cng_enabled == 0) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec], curent_timeout = %d, No "
			    "carrier in _hdlc_receive_look_carrier_state, "
			    "S7 = %d[sec]\n",
			    ctx->clock_sec, ctx->clock_frac, limit,
			    ctx->s7_timeout);
		ctx->status = FAX_CLASS1_NO_CARRIER;
		ctx->state = CLASS1_IDLE_STATE;
		ctx->cng_enabled = 0;
	}

	if (ctx->cng_enabled == 0) {
		_put_silence(tx, CLASS1_BLOCK_SAMPLES);
	} else if (ctx->tone_cadence_phase == 0) {
		_put_silence(tx, CLASS1_BLOCK_SAMPLES);
		ctx->tone_cadence_timer += CLASS1_BLOCK_SAMPLES;
		if (ctx->tone_cadence_timer > 0x5dc0) {
			ctx->tone_cadence_timer = 0;
			ctx->tone_cadence_phase = 1;
		}
	} else {
		FPM_TONE_generate(ctx->tone, tx, CLASS1_BLOCK_SAMPLES);
		ctx->tone_cadence_timer += CLASS1_BLOCK_SAMPLES;
		if (ctx->tone_cadence_timer > 0xfa0) {
			ctx->tone_cadence_timer = 0;
			ctx->tone_cadence_phase = 0;
		}
	}
	*tx_count = CLASS1_BLOCK_SAMPLES;

	if (*word8 != 0) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec] TxDatCnt>0 in "
			    "_hdlc_receive_look_carrier_state abort command "
			    "mode.\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->state = CLASS1_IDLE_STATE;
		_idle_state_init(ctx);
		*(int *)(long)word7 = cTOOLS_handle_hdlc_output(ctx,
		    (const unsigned short *)((char *)ctx + 2),
		    (unsigned char *)(long)word3, 0, 1);
		ctx->status = FAX_CLASS1_OK;
	}

	*word8 = 5;
	return 0;
}

/*
 * TX_SCRAMBLED_ONES_STATE (9).  `.text` 0x0009d200, 681 bytes.
 *
 * THE CRASH THAT BLOCKED THIS FUNCTION WAS `ctx->tx_bytes_per_block`'S WIDTH, NOT
 * ANYTHING HERE (finding in this batch's own entry).  `ref__tx_scrambled_
 * ones_state`'s own fill loop reads `ctx->tx_bytes_per_block` with a plain 32-bit `mov`
 * (0x9d200+0xa0) and uses the whole register as a loop bound; modelled as
 * `unsigned short` plus two bytes of `pad_1292`, a test's random fill of
 * those two "pad" bytes turned the bound into a value near 2^29 and walked
 * the write loop off the end of the struct.  `class1.h`'s `tx_bytes_per_block` is now a
 * full `int` (matching a second, independent access in `_tx_nulls_state` at
 * the same offset -- see that field's own comment) and the crash is gone;
 * this function itself was correctly decoded from the start.
 *
 * `DATAtx_counter` (above, shared with `_tx_data_state`) gates a ONE-TIME
 * `ctx->status = FAX_CLASS1_CONNECT` on the session's very first call into
 * this pair.
 *
 * `ctx->tx_connect_countdown` is a one-shot countdown, decremented once per call while
 * positive; reaching exactly 0 fires "At %2d.%02d[sec] ENABLE_TRANSMIT in
 * _tx_scrambled_ones_state\n" and sets `ctx->transmit_enabled = 1`.
 *
 * `*word8 > 0` unstuffs `word4` through `_handle_data_input` into `ctx`
 * (the shared scratch-buffer idiom), then `FIFO_write`s the result into
 * `ctx->tx_fifo`, logging a shortfall ("Fifo is full in
 * _tx_scrambled_ones_state").  `ctx->tx_fifo_ready` is then recomputed: 1 when
 * `ctx->tx_fifo->count >= ctx->tx_bytes_per_block` (the FIFO already holds a whole read's
 * worth), else 0 -- but ONLY inside this `*word8 > 0` block; on a call
 * where it does not run, `tx_fifo_ready` is left at whatever the last call set.
 *
 * `ctx->tx_bytes_per_block` elements of `ctx` are then filled with the literal `0xff`
 * (the "scrambled ones" this state's name promises) UNCONDITIONALLY, and
 * `*word8` is set to that same count.
 *
 * If `ctx->transmit_enabled != 0 && ctx->tx_fifo_ready != 0`: `ctx->state` becomes
 * `CLASS1_TX_DATA_STATE`, and the 0xFF filler is immediately overwritten by
 * a REAL `FIFO_read` into the same buffer -- `*word8` becomes that read's
 * return, logging an underrun ("class1 object fifo under run in
 * _tx_scrambled_ones_state !!!") without undoing the state change.
 *
 * `cnt` (FAXVMI_process's `count`) is seeded from `*word8` AS IT STANDS AT
 * THAT POINT -- the fill count or the FIFO_read's return, whichever path
 * ran -- read directly off the object's own `mov %ax,0x22(%esp)` at
 * 0x9d2e7, which is fed by whatever is still in `%eax` from the join above
 * it (NOT a fresh reload of `ctx->tx_bytes_per_block`, which an earlier draft of this
 * function assumed).  `result` is seeded from `*tx_count` (`mov
 * 0x48(%esp),%ecx; mov (%ecx),%edx; mov %dx,0x20(%esp)`, 0x9d2ec-0x9d2fe),
 * the same convention `_tx_data_state` already uses -- NOT a literal 0, an
 * earlier draft's other assumption.
 *
 * A NEW RAW BIT: `test $0x1,%ah` on `FAXVMI_process`'s raw return, i.e. bit
 * 0x100 -- DIFFERENT from `FAXVMI_RESULT_BIT_2000` this wave's HDLC-side
 * functions use.  SET (and `ctx->tx_connect_latch == 0`) fires "At %2d.%02d[sec] Tx
 * connect\n", arms `ctx->tx_connect_countdown = 2`, and latches `ctx->tx_connect_latch = 1` so the
 * bit is never re-tested once caught.
 *
 * Tail, unconditional: `*word8 = ctx->tx_fifo->size - ctx->tx_fifo->count - 1`
 * -- the same free-room-minus-one formula `_tx_nulls_state`/`_tx_data_state`
 * end with.
 *
 * FORMAT STRINGS, verified against `.rodata.str1.1`/`.rodata.str1.4`:
 *   0x48c2 (.str1.1)  "cDATAtx_counter %d\n" (shares `DATAtx_counter`)
 *   0x48d6 (.str1.1)  "At %2d.%02d[sec] Tx connect\n"
 *   0x12448  "class1 object fifo under run in _tx_scrambled_ones_state !!!\n"
 *   0x12488  "At %2d.%02d[sec] ENABLE_TRANSMIT in _tx_scrambled_ones_state\n"
 *   0x124c8  "At %2d.%02d[sec] Fifo is full in _tx_scrambled_ones_state\n"
 */
#define FAXVMI_PROCESS_BIT_0100	0x100

int
_tx_scrambled_ones_state(struct fax_class1 *ctx, const short *rx, short *tx,
			 int word3, int word4, int *rx_count, int *tx_count,
			 int word7, int *word8)
{
	short cnt;
	unsigned short result = (unsigned short)*tx_count;
	int status;
	int i;

	(void)rx;
	(void)word3;
	(void)rx_count;
	(void)word7;

	if (dsplibs_debug_level > 2)
		dsplibs_debug_printf("cDATAtx_counter %d\n", DATAtx_counter);
	DATAtx_counter++;
	if (DATAtx_counter == 1)
		ctx->status = FAX_CLASS1_CONNECT;

	if (ctx->tx_connect_countdown > 0) {
		ctx->tx_connect_countdown--;
		if (ctx->tx_connect_countdown == 0) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "At %2d.%02d[sec] ENABLE_TRANSMIT in "
				    "_tx_scrambled_ones_state\n",
				    ctx->clock_sec, ctx->clock_frac);
			ctx->transmit_enabled = 1;
		}
	}

	if (*word8 > 0) {
		int n;

		_handle_data_input(ctx, (const unsigned char *)(long)word4,
		    (unsigned short *)(void *)ctx, word8);

		n = (unsigned short)FIFO_write(ctx->tx_fifo,
		    (unsigned short *)(void *)ctx, (unsigned short)*word8);
		if (*word8 > n) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "At %2d.%02d[sec] Fifo is full in "
				    "_tx_scrambled_ones_state\n",
				    ctx->clock_sec, ctx->clock_frac);
		}

		ctx->tx_fifo_ready = (ctx->tx_fifo->count >= (unsigned)ctx->tx_bytes_per_block) ? 1
									  : 0;
	}

	for (i = 0; i < ctx->tx_bytes_per_block; i++)
		((unsigned short *)(void *)ctx)[i] = 0xff;
	*word8 = ctx->tx_bytes_per_block;

	if (ctx->transmit_enabled != 0 && ctx->tx_fifo_ready != 0) {
		int rd;

		ctx->state = CLASS1_TX_DATA_STATE;
		rd = FIFO_read(ctx->tx_fifo, (unsigned short *)(void *)ctx,
		    (unsigned short)ctx->tx_bytes_per_block);
		*word8 = rd;
		if (rd < ctx->tx_bytes_per_block) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "class1 object fifo under run in "
				    "_tx_scrambled_ones_state !!!\n");
		}
	}

	cnt = (short)*word8;
	status = FAXVMI_process(ctx->vmi_b, (unsigned short *)(void *)ctx, tx,
	    &cnt, &result);

	if (ctx->tx_connect_latch == 0 && (status & FAXVMI_PROCESS_BIT_0100) != 0) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec] Tx connect\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->tx_connect_countdown = 2;
		ctx->tx_connect_latch = 1;
	}

	*word8 = (int)(unsigned short)ctx->tx_fifo->size
	       - (int)(unsigned short)ctx->tx_fifo->count - 1;
	return 0;
}

/*
 * SEND_HDLC_BUFFER_STATE (2).  `.text` 0x0009e470, 282 bytes.
 *
 * Drives `ctx->vmi_c` -- purely for its side effects: the call's own
 * `count`/`result` locals (`count` seeded 0, `result` seeded from
 * `*tx_count`) are read by nothing after the call.  Immediately follows
 * with `FAXVMI_status(ctx->vmi_c, &st)`, `st` seeded from `FAXVMI_STS`
 * (class1.h/faxvmi.h's own all-zero template -- the SECOND reference to it
 * this tree reconstructs, per faxvmi.h's own note pointing here).
 *
 * `st.underrun == 1`: log ("%2d.%02d[sec] End of HDLC buffer "
 * "transmission\n"), `ctx->state = CLASS1_SEND_HDLC_BETWEEN_BUFFER_STATE`,
 * `ctx->countdown = 0`, `_handle_hdlc_input_open(ctx)`.  Either way,
 * `*word8 = 0x200` and return 0.  `*tx_count` is NEVER written by this
 * function on ANY path -- confirmed, not an oversight.
 *
 * FORMAT STRING: 0x12a10 "%2d.%02d[sec] End of HDLC buffer "
 * "transmission\n"
 */
int
_send_hdlc_buffer_state(struct fax_class1 *ctx, const short *rx, short *tx,
			int word3, int word4, int *rx_count, int *tx_count,
			int word7, int *word8)
{
	short cnt = 0;
	unsigned short result = (unsigned short)*tx_count;
	struct faxvmi_status st = FAXVMI_STS;

	(void)rx;
	(void)word3;
	(void)word4;
	(void)rx_count;
	(void)word7;

	FAXVMI_process(ctx->vmi_c, (unsigned short *)(void *)ctx, tx, &cnt,
		       &result);
	FAXVMI_status(ctx->vmi_c, &st);

	if (st.underrun == 1) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "%2d.%02d[sec] End of HDLC buffer "
			    "transmission\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->state = CLASS1_SEND_HDLC_BETWEEN_BUFFER_STATE;
		ctx->countdown = 0;
		_handle_hdlc_input_open(ctx);
	}

	*word8 = 0x200;
	return 0;
}

/*
 * T30_PREAMBLE_STATE (1).  `.text` 0x0009e660, 421 bytes.
 *
 * `*tx_count = CLASS1_BLOCK_SAMPLES` is set FIRST, unconditionally, and
 * never changed again -- so `ctx->countdown += *tx_count` at the end always
 * adds `CLASS1_BLOCK_SAMPLES`, whatever path was taken.
 *
 * `*word8 > 0`: (debug>2 log, "Collect data with %d bytes.\n") unstuff
 * `word4` through `_handle_hdlc_input(ctx, word4, ctx, word8)` (dst=`ctx`,
 * the same scratch-buffer idiom) and bank the frame-complete flag (0/1)
 * into `ctx->hdlc_frame_done`.  If it completed a frame,
 * `ctx->buffers_sent++`.
 *
 * `ctx->countdown > 8000` (unsigned) AND `ctx->hdlc_frame_done != 0`: log
 * ("Elapsed 1 second, send %d buffers\n", `ctx->buffers_sent`),
 * `ctx->state = CLASS1_SEND_HDLC_BUFFER_STATE`, and seed the
 * FAXVMI_process `count` local from `ctx->buffers_sent`; on every other
 * path it stays at its ZERO-INITIALISED value (the object's own stack slot
 * IS zeroed by the compiler's usual "cnt = 0" prologue for this local, not
 * left as stack garbage -- a first integration pass claimed the opposite
 * and left this local truly uninitialised in the C, which is undefined
 * behaviour in our reconstruction even where the object's own asm has a
 * definite zero; fixed here).
 *
 * `ctx->countdown > 40000` (unsigned): log ("Elapsed 5 second in "
 * "_t30_preabmle_state\n"), `ctx->status = FAX_CLASS1_ERROR_NO_CARRIER`,
 * `ctx->state = CLASS1_IDLE_STATE`.
 *
 * Either way, `FAXVMI_process(ctx->vmi_c, ctx, tx, &count, &result)` --
 * `result` seeded 0.
 *
 * FORMAT STRINGS, verified against `.rodata.str1.4`:
 *   0x12a74  "%2d.%02d[sec] Collect data with %d bytes.\n"
 *   0x12aa0  "At %2d.%02d[sec] Elapsed 1 second, send %d buffers\n"
 *   0x12ad4  "At %2d.%02d[sec] Elapsed 5 second in _t30_preabmle_state\n"
 */
int
_t30_preabmle_state(struct fax_class1 *ctx, const short *rx, short *tx,
		    int word3, int word4, int *rx_count, int *tx_count,
		    int word7, int *word8)
{
	short cnt = 0;
	unsigned short result = 0;

	(void)rx;
	(void)word3;
	(void)rx_count;
	(void)word7;

	*tx_count = CLASS1_BLOCK_SAMPLES;

	if (*word8 > 0) {
		if (dsplibs_debug_level > 2)
			dsplibs_debug_printf(
			    "%2d.%02d[sec] Collect data with %d bytes.\n",
			    ctx->clock_sec, ctx->clock_frac, *word8);
		ctx->hdlc_frame_done = _handle_hdlc_input(ctx,
		    (const unsigned char *)(long)word4,
		    (unsigned short *)(void *)ctx, word8);
		if (ctx->hdlc_frame_done)
			ctx->buffers_sent++;
	}

	if ((unsigned int)ctx->countdown > 0x1f40) {
		if (ctx->hdlc_frame_done != 0) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "At %2d.%02d[sec] Elapsed 1 second, "
				    "send %d buffers\n",
				    ctx->clock_sec, ctx->clock_frac,
				    ctx->buffers_sent);
			ctx->state = CLASS1_SEND_HDLC_BUFFER_STATE;
			cnt = (short)ctx->buffers_sent;
		}
	}

	if ((unsigned int)ctx->countdown > 0x9c40) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec] Elapsed 5 second in "
			    "_t30_preabmle_state\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->status = FAX_CLASS1_ERROR_NO_CARRIER;
		ctx->state = CLASS1_IDLE_STATE;
	}

	FAXVMI_process(ctx->vmi_c, (unsigned short *)(void *)ctx, tx, &cnt,
		       &result);

	ctx->countdown += *tx_count;
	*word8 = 0x200;
	return 0;
}

/*
 * SEND_HDLC_BETWEEN_BUFFER_STATE (3).  `.text` 0x0009e810, 433 bytes.
 *
 * REPLACES A FIRST INTEGRATION PASS that read this function as driven by
 * `*rx_count`; re-derived from a fresh `dis.py` trace of the function's own
 * argument-offset arithmetic (push esi,ebx; sub $0x24 -> ctx=0x30, tx=0x38,
 * word4=0x40, tx_count=0x48, word8=0x50) and it is driven by `ctx->countdown`
 * and `*word8`, NOT `*rx_count` -- `rx_count` is never read by this function
 * at all.
 *
 * `*tx_count = CLASS1_BLOCK_SAMPLES` first, unconditionally.
 *
 * `ctx->countdown == 2` is a ONE-TIME LATCH (only true on the third call
 * after `_send_hdlc_between_buffer_state_init` zeroes it, since this
 * function increments it by exactly one per call and nothing else here
 * writes it): `ctx->frame_end_latch == 1` -> log ("Idle
 * state\n"), OK_NO_CARRIER, IDLE_STATE, `_idle_state_init`; otherwise ->
 * `ctx->status = FAX_CLASS1_CONNECT`.  Either way, falls through into the
 * body below on the SAME call.
 *
 * BODY: reads more host data only when `*word8 > 0` (signed) AND
 * `(unsigned)countdown > 1`.  `_handle_hdlc_input`'s `src` is `word4`, its
 * `count` is `word8` ITSELF (reused directly, not through a local), and its
 * `dst` is `ctx` (the scratch-buffer idiom, real bytes land there this time
 * since `count` is not forced to zero).  A completed frame (return 1):
 * `state = SEND_HDLC_BUFFER_STATE`, and the LOCAL count fed to the trailing
 * `FAXVMI_process` call below is bumped by one.
 *
 * TAIL (reached from three places -- the body's own fallthrough, and both
 * arms of the host-read branch): `FAXVMI_process(ctx->vmi_c, ctx, tx,
 * &cnt, &result)` (`cnt` 0 unless just bumped, `result` 0); `countdown++`;
 * once `(unsigned)countdown > 250`: log ("CURRENT_STATE_TIMER > 5[sec]\n"),
 * FAX_CLASS1_ERROR_ON_HOOK, IDLE_STATE, `_idle_state_init`.
 *
 * `*word8 = 0x200` unconditionally at the very end, exactly like
 * `_send_hdlc_buffer_state`.
 *
 * FORMAT STRINGS, verified against `.rodata.str1.1`/`.rodata.str1.4`:
 *   0x48f3   "%2d.%02d[sec] Idle state\n"            (.str1.1)
 *   0x12b10  "At %2d.%02d[sec], CURRENT_STATE_TIMER > 5[sec]\n"  (.str1.4)
 */
int
_send_hdlc_between_buffer_state(struct fax_class1 *ctx, const short *rx,
				short *tx, int word3, int word4,
				int *rx_count, int *tx_count, int word7,
				int *word8)
{
	short cnt = 0;
	unsigned short result = 0;

	(void)rx;
	(void)word3;
	(void)rx_count;
	(void)word7;

	*tx_count = CLASS1_BLOCK_SAMPLES;

	if (ctx->countdown == 2) {
		if (ctx->frame_end_latch == 1) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "%2d.%02d[sec] Idle state\n",
				    ctx->clock_sec, ctx->clock_frac);
			ctx->status = FAX_CLASS1_OK_NO_CARRIER;
			ctx->state = CLASS1_IDLE_STATE;
			_idle_state_init(ctx);
		} else {
			ctx->status = FAX_CLASS1_CONNECT;
		}
	}

	if (*word8 > 0 && (unsigned int)ctx->countdown > 1) {
		int done = _handle_hdlc_input(ctx,
		    (const unsigned char *)(long)word4,
		    (unsigned short *)(void *)ctx, word8);

		if (done != 0) {
			ctx->state = CLASS1_SEND_HDLC_BUFFER_STATE;
			cnt++;
		}
	}

	FAXVMI_process(ctx->vmi_c, (unsigned short *)(void *)ctx, tx, &cnt,
	    &result);

	ctx->countdown++;
	if ((unsigned int)ctx->countdown > 250) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec], CURRENT_STATE_TIMER > "
			    "5[sec]\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->status = FAX_CLASS1_ERROR_ON_HOOK;
		ctx->state = CLASS1_IDLE_STATE;
		_idle_state_init(ctx);
	}

	*word8 = 0x200;
	return 0;
}

/*
 * CHDLCTX_OFF_STATE (17).  `.text` 0x0009ea70, 238 bytes.
 *
 * `ctx->countdown++` BEFORE the call.  `FAXVMI_process(ctx->vmi_a, ctx, rx,
 * &count, &result)` -- `count` seeded 0, `result` seeded from `*rx_count`.
 *
 * The raw status's bit 0x2000 (`FAXVMI_RESULT_BIT_2000`) swaps which of TWO
 * countdown thresholds applies -- 15 when SET, 2 when CLEAR -- both
 * against the JUST-incremented `ctx->countdown`.  Either threshold
 * crossed: log ("End of off state, COUNTER %d\n", `ctx->countdown`),
 * `ctx->status = FAX_CLASS1_OK_NO_CARRIER`, `ctx->state =
 * CLASS1_IDLE_STATE`, `_idle_state_init(ctx)`.  Neither: nothing.
 *
 * Tail, unconditional: `_put_silence(tx, CLASS1_BLOCK_SAMPLES)`, `*tx_count
 * = CLASS1_BLOCK_SAMPLES`.
 *
 * FORMAT STRING: 0x12b40 "%2d.%02d[sec] End of off state, COUNTER %d\n"
 */
int
cHDLCtx_off(struct fax_class1 *ctx, const short *rx, short *tx, int word3,
	   int word4, int *rx_count, int *tx_count, int word7, int *word8)
{
	short cnt = 0;
	unsigned short result = (unsigned short)*rx_count;
	int status;
	int over;

	(void)word3;
	(void)word4;
	(void)word7;
	(void)word8;

	ctx->countdown++;

	status = FAXVMI_process(ctx->vmi_a, (unsigned short *)(void *)ctx, (short *)rx,
				&cnt, &result);

	if (status & FAXVMI_RESULT_BIT_2000)
		over = ctx->countdown > 15;
	else
		over = ctx->countdown > 2;

	if (over) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "%2d.%02d[sec] End of off state, COUNTER %d\n",
			    ctx->clock_sec, ctx->clock_frac, ctx->countdown);
		ctx->status = FAX_CLASS1_OK_NO_CARRIER;
		ctx->state = CLASS1_IDLE_STATE;
		_idle_state_init(ctx);
	}

	_put_silence(tx, CLASS1_BLOCK_SAMPLES);
	*tx_count = CLASS1_BLOCK_SAMPLES;
	return 0;
}

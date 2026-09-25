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
	0x00,				/* flags         */
};

const struct v21tx_ctl V21TX_CTL = {
	{ 0x2c, 0x01, 0x00, 0x00 },	/* unmapped_0000 */
	60000,				/* int_0004      */
	3200,				/* scale         */
	0x00,				/* mask          */
	0x00,				/* flags         */
	{ 0, 0 },			/* unmapped_000e */
	{ 0, 0, 0, 0 },			/* unmapped_0010 */
};


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
 * (`fifo_size`/`max_frame` differ by modulation; `slot` is what makes
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
 * `fifo_size_factor` in all three of `struct v17tx_cfg`/`struct v27tx_cfg`/
 * `struct v29tx_cfg` now (the latter two renamed this wave onto V.17's own
 * already-established name, findings F10144 and this wave's own) -- `0x1`
 * for V.17, `0x2` for V.27ter and V.29.  V.17's table default is already 1,
 * so that one is invisible to any test that only checks the FINAL value;
 * V.27ter's and V.29's tables are also 1 (`tabdump.py` over the blob's own
 * `.data` confirms it, independent of either reconstructed table), so their
 * override to 2 is a real, visible change from the default.
 * `cfg->fifo_size_factor = <value>;` after the table copy reproduces this
 * for all three now -- a plain `int` assignment stores the same final 32
 * bits as the object's narrower `movw`, since the upper 16 bits are already
 * zero from the dword copy, so no encoding trick is needed for behavioural
 * fidelity.
 */
static int
init_vmi_v17tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
	       int arg_2, void *arg_3)
{
	struct v17tx_cfg *cfg = sysdep_malloc(sizeof(struct v17tx_cfg));

	(void)arg_2;

	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf(
			"Initializing VMI_V17_TX Modem No ECM " "(Simple Packing)\n");

	*cfg = V17TX_CFG;
	cfg->bitrate = bit_rate;
	cfg->fifo_size_factor = 1;
	cfg->int_0018 = 0;
	cfg->int_001c = (int)(long)arg_3;

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->mode = 0;
	vmi->reverse = 1;
	vmi->fifo_size = 0x60;
	vmi->max_frame = 0x30;
	vmi->frame_size = 0;
	vmi->slot = VMI_SLOT_V17TX;
	vmi->modem_cfg = cfg;

	return cfg->bitrate;
}

static int
init_vmi_v29tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
	       int arg_2, void *arg_3)
{
	struct v29tx_cfg *cfg = sysdep_malloc(sizeof(struct v29tx_cfg));

	(void)arg_2;

	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf(
			"Initializing VMI_V29_TX Modem No ECM " "(Simple Packing)\n");

	*cfg = V29TX_CFG;
	cfg->bitrate = bit_rate;
	cfg->fifo_size_factor = 2;
	cfg->int_0018 = (int)(long)arg_3;

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->mode = 0;
	vmi->reverse = 1;
	vmi->fifo_size = 0x60;
	vmi->max_frame = 0x35;
	vmi->frame_size = 0;
	vmi->slot = VMI_SLOT_V29TX;
	vmi->modem_cfg = cfg;

	return cfg->bitrate;
}

static int
init_vmi_v27tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
	       int arg_2, void *arg_3)
{
	struct v27tx_cfg *cfg = sysdep_malloc(sizeof(struct v27tx_cfg));

	(void)arg_2;

	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf(
			"Initializing VMI_V27_TX Modem No ECM " "(Simple Packing)\n");

	*cfg = V27TX_CFG;
	cfg->bitrate = bit_rate;
	cfg->fifo_size_factor = 2;
	cfg->int_001c = (int)(long)arg_3;

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->mode = 0;
	vmi->reverse = 1;
	vmi->fifo_size = 0x40;
	vmi->max_frame = 0x25;
	vmi->frame_size = 0;
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
	1,				/* scale_mul */
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
			"%2d.%02d[sec] Initializing TX modem, " "MODEM_IDX = %d, silence %d ms\n",
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
				"%2d.%02d[sec] New TX Modem... " "Deleting previous existing one\n",
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
 * ------------------------------------------------------------------
 * THE REMAINING TWELVE.  Inserted here, after `_hdlc_emulate_receive_state`
 * (this file's last function by both position and, until now, by address) --
 * this file is not in whole-file address order already (the VMI-constructor
 * trio above sits far out of the order its own 0x0948xx addresses would
 * imply, per that section's own comment), so a single clean append at EOF is
 * the insertion point that disturbs the least of what is already here.
 * Internally the twelve are grouped by RELATED FUNCTION rather than strict
 * ascending address (`_tx_scrambled_ones_state` and `_tx_data_state` in
 * particular share the `cDATAtx_counter` static and sit together for that
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

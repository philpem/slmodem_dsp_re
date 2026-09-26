/*
 * cHDLCrx.c -- the blob's cHDLCrx.c translation unit (TU-reconciliation, issue #6/#20/#67).
 * Functions moved verbatim from class1tx.c in blob emission order.
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
extern const struct v21tx_ctl V21TX_CTL;

extern const struct v21rx_ctl V21RX_CTL;


/* The object's raw-status mask used by the cHDLCrx receive handlers. */
#define FAXVMI_RESULT_BIT_2000	0x2000

/*
 * A signal-level threshold table, `.rodata` 0xba22, four `unsigned short`
 * entries -- `514, 727, 1026, 1450` -- read off the object's own bytes
 * (`objdump -s`).  Referenced only from `_hdlc_receive_look_carrier_state`,
 * so kept `static` rather than declared in a header; nothing else in this
 * translation unit reaches it.
 */
static const unsigned short HDLC_LOOK_CARRIER_LEVELS[4] = {
	514, 727, 1026, 1450,
};


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

	req.flags |= V21RXCTL_REINIT;

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
				    "At %2d.%02d[sec] No carrier in HDLC " "receive state\n",
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
					    "%2d.%02d[sec] Receive buffer " "with error in "
					    "_hdlc_receive_state\n",
					    ctx->clock_sec, ctx->clock_frac);
				ctx->delayed_status_countdown = 2;
				ctx->delayed_status = FAX_CLASS1_ERROR;
			} else {
				void *modem;
				char *p;

				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "%2d.%02d[sec] Receive buffer OK " "in _hdlc_receive_state\n",
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
				    "SuperFrame full, skipping HDLC " "frame!\n");
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
			    "%2d.%02d[sec] No carrier during command " "mode, NO MESSAGE****\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->status = FAX_CLASS1_NO_CARRIER_NO_MESSAGE;
		ctx->state = CLASS1_IDLE_STATE;
	}

	if (*word8 != 0) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "%2d.%02d[sec] TxDatCnt>0 in "
			    "_hdlc_receive_between_buffers_state abort " "command mode.\n",
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
					    "Gain Attenuation Reuqest: " "+%d[dB], avg_rms = %d",
					    ctx->gain_attenuation_db, (int)(signed char)v);
				break;
			}
		}
	} else if (limit != 0 && (unsigned int)ctx->countdown >
		   (unsigned int)limit && ctx->cng_enabled == 0) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec], curent_timeout = %d, No "
			    "carrier in _hdlc_receive_look_carrier_state, " "S7 = %d[sec]\n",
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
			    "_hdlc_receive_look_carrier_state abort command " "mode.\n",
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

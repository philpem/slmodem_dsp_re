/*
 * cHDLCtx.c -- the blob's cHDLCtx.c translation unit (TU-reconciliation, issue #6/#20/#67).
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


/* The object's raw-status mask used by cHDLCtx_off. */
#define FAXVMI_RESULT_BIT_2000	0x2000


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

	req.flags |= V21TXCTL_REINIT;
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
			    "%2d.%02d[sec] End of HDLC buffer " "transmission\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->state = CLASS1_SEND_HDLC_BETWEEN_BUFFER_STATE;
		ctx->countdown = 0;
		_handle_hdlc_input_open(ctx);
	}

	*word8 = 0x200;
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
			    "%2d.%02d[sec], Elapsed 50MS second, " "send preamble\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->state = CLASS1_T30_PREAMBLE_STATE;
		ctx->countdown = 0;
		ctx->status = FAX_CLASS1_CONNECT;
	}
	ctx->countdown += *tx_count;
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
				    "At %2d.%02d[sec] Elapsed 1 second, " "send %d buffers\n",
				    ctx->clock_sec, ctx->clock_frac,
				    ctx->buffers_sent);
			ctx->state = CLASS1_SEND_HDLC_BUFFER_STATE;
			cnt = (short)ctx->buffers_sent;
		}
	}

	if ((unsigned int)ctx->countdown > 0x9c40) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec] Elapsed 5 second in " "_t30_preabmle_state\n",
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
			    "At %2d.%02d[sec], CURRENT_STATE_TIMER > " "5[sec]\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->status = FAX_CLASS1_ERROR_ON_HOOK;
		ctx->state = CLASS1_IDLE_STATE;
		_idle_state_init(ctx);
	}

	*word8 = 0x200;
	return 0;
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

	req.flags |= V21RXCTL_REINIT;

	ctl.ptr_0000 = (void *)1;
	ctl.int_000c = 1;
	ctl.short_0010 = 2;
	ctl.int_0014 = (int)(long)&req;

	ret = FAXVMI_control(ctx->vmi_a, &ctl);

	ctx->countdown = 0;
	return ret;
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

/*
 * cDATArx.c -- the blob's cDATArx.c translation unit (TU-reconciliation, issue #6/#20/#67).
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

/* The object's raw-status mask used by the cDATArx receive handlers. */
#define FAXVMI_RESULT_BIT_2000	0x2000


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
			    "At %2d.%02d[sec] Data RX connect in " "_rx_look_carrier_state\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->status = FAX_CLASS1_CONNECT;
		ctx->state = CLASS1_RX_DATA_STATE;
	}

	if (limit != 0 &&
	    (unsigned int)ctx->countdown > (unsigned int)limit &&
	    !(status1 & FAXVMI_RESULT_BIT_2000)) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("S7 time elapsed in look " "carrier\n");
		ctx->status = FAX_CLASS1_NO_CARRIER;
		ctx->state = CLASS1_IDLE_STATE;
	}

	if (*word8 != 0) {
		int n;

		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "TxDatCnt !=0 in _rx_look_carrier_state... " "abort to command mode\n");
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
			    "At %2d.%02d[sec] No carrier in _rx_data_state, " "move to idle\n",
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
				    "TxDatCnt !=0 in _rx_data_state... " "abort to command mode\n");
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

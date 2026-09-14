/*
 * fax.c -- the FAX Class 1 service dispatcher.
 *
 * Reconstructed from dsplibs.o fax.c, .text 0x1450..0x2121:
 *
 *   FAX_delete           0x1450   172
 *   FAX_create           0x1500
 *   FAX_class1_command   0x1740
 *   FAX_process          0x1a10
 *
 * `struct fax_ctx` is NOT `struct voice_ctx`; see `fax.h`.  The object keeps
 * these in their own translation unit, immediately after `voice.c`, which is
 * why they are here and not in `src/service/voice.c`, where the locals of the
 * voice codec (`vce_hook_on`/`_off`/`get_sreg`) live.
 */

#include <stdint.h>

#include "dsplib/debug.h"
#include "dsplib/fax.h"
#include "dsplib/class1.h"
#include "dsplib/fixedrc.h"
#include "dsplib/modem_params.h"
#include "dsplib/sysdep.h"

extern int modem_recv_from_tty(void *m, void *buf, int n);
extern int modem_send_to_tty(void *m, const void *buf, int n);


/*
 * `.text` 0x001450, 172 bytes -- see `fax.h` for `struct fax_ctx` and why
 * this sits in a separate header from `struct voice_ctx` despite living in
 * the same TU.  `FAX_create` and `FAX_class1_command`, the object's own
 * neighbours on either side of this group (0x001500 and 0x001740), are now
 * both written too (F10121), below.
 *
 * The object's own order: print "fax: delete...\n" (`.rodata.str1.1`
 * 0x1be) when `dsplibs_debug_level > 1`, then unconditionally check and
 * delete `rc_a`, `rc_b` and `class1` in that order, clear `class1`, and
 * free `ctx`.  Every check is independent -- there is no early return, and
 * the debug branch rejoins the same three checks rather than skipping any
 * of them.
 */
void
FAX_delete(struct fax_ctx *ctx)
{
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("fax: delete...\n");

	if (ctx->rc_a != NULL)
		RcFixed_Delete(ctx->rc_a);
	if (ctx->rc_b != NULL)
		RcFixed_Delete(ctx->rc_b);
	if (ctx->class1 != NULL)
		fax_class1_delete(ctx->class1);

	ctx->class1 = NULL;
	sysdep_free(ctx);
}

/*
 * slmodemd's own S-register reader -- NOT the voice service's own
 * `voice_get_sreg`-shaped function a few hundred lines up in this same file
 * (that one's own banner already says so: "this is NOT a call into it").
 * `FAX_create` is the one traced caller, asking for register 7 (T.30's own
 * S7, the carrier-wait timeout `struct fax_class1_cfg::s7_timeout` carries
 * straight through).  No dsplib header owns it, the same reason
 * `modem_recv_from_tty`/`modem_send_to_tty` are declared locally above.
 */
extern long modem_get_sreg(void *modem, unsigned int reg);

/*
 * `.text` 0x001500, 564 bytes.  See `fax.h`'s own prototype comment for
 * `originate`/`rate`'s meaning; this banner is the CONTROL-FLOW account.
 *
 *   1. Debug print ("fax: create...\n", `.rodata.str1.1` 0x1ce) at debug
 *      level > 1, BEFORE anything else -- even the allocation.
 *   2. `sysdep_malloc(sizeof(struct fax_ctx))`; NULL returns NULL directly
 *      (no debug line, no cleanup -- there is nothing yet to clean up).
 *      `sysdep_memset` the whole thing to 0, then `ctx->modem = modem`.
 *   3. `rate == 8000`: skip step 4 entirely, `rc_a`/`rc_b` stay NULL (the
 *      memset's own zero). Any other value: build `rc_a` (9600 -> converter
 *      3, 48000 -> converter 5, anything else -> NULL) and, if that
 *      succeeded, `rc_b` the same way (9600 -> 2, 48000 -> 4). A NULL
 *      resampler where one was expected -- including the "anything else"
 *      case, which builds neither and always fails here -- jumps straight
 *      to the teardown at step 6.
 *   4. `ctx->host_frame_samples = ctx->out_produced = ctx->out_write_half =
 *      rate * CLASS1_BLOCK_SAMPLES / 8000` (unsigned; the object's own
 *      `mul`/`shr` reciprocal, re-derived rather than assumed to be `/50`
 *      even though the arithmetic reduces to that for every rate this
 *      function accepts).
 *   5. Build the Class 1 session: `local` is a `struct fax_class1_cfg`,
 *      zeroed, with `mode` from `originate` (nonzero ->
 *      `CLASS1_ANS_ORG_NORMAL`, zero -> `CLASS1_ANS_ORG_ANSWER`),
 *      `s7_timeout` from `modem_get_sreg(modem, 7)`, and `iir_enable = 1`
 *      (`answer_tone_ms`/`f08`/`disable_cng` all stay 0, the zeroed
 *      default). Debug-print "fax: fax_class1 will created (ans_org=%d,
 *      s7=%d)\n" at level > 1 with `local.mode` and the raw S7 value, THEN
 *      `ctx->class1 = fax_class1_create(NULL, &local)`. A NULL result also
 *      falls to step 6.
 *   6. TEARDOWN, only reached by a step-3/5 failure: debug-print "fax:
 *      delete...\n" (`FAX_delete`'s own string) at level > 1, delete `rc_a`/
 *      `rc_b`/`class1` exactly as `FAX_delete` does -- INLINE, not by
 *      calling it (no relocation to `FAX_delete` in this range) -- and
 *      `sysdep_free(ctx)`.  Returns NULL.
 *   7. Otherwise returns `ctx`.
 */
struct fax_ctx *
FAX_create(void *modem, int originate, unsigned int rate)
{
	struct fax_ctx *ctx;
	struct fax_class1_cfg local;
	int s7;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("fax: create...\n");

	ctx = sysdep_malloc(sizeof(struct fax_ctx));
	if (ctx == NULL)
		return NULL;
	sysdep_memset(ctx, 0, sizeof(struct fax_ctx));
	ctx->modem = modem;

	if (rate != 8000) {
		if (rate == 9600)
			ctx->rc_a = RcFixed_Create(3);
		else if (rate == 48000)
			ctx->rc_a = RcFixed_Create(5);
		else
			ctx->rc_a = NULL;
		if (ctx->rc_a == NULL)
			goto fail;

		if (rate == 9600)
			ctx->rc_b = RcFixed_Create(2);
		else if (rate == 48000)
			ctx->rc_b = RcFixed_Create(4);
		else
			ctx->rc_b = NULL;
		if (ctx->rc_b == NULL)
			goto fail;
	}

	ctx->host_frame_samples =
	    (int)((rate * (unsigned int)CLASS1_BLOCK_SAMPLES) / 8000U);
	ctx->out_produced = ctx->host_frame_samples;
	ctx->out_write_half = ctx->host_frame_samples;

	sysdep_memset(&local, 0, sizeof(local));
	s7 = modem_get_sreg(modem, 7);
	local.mode = (originate != 0) ? CLASS1_ANS_ORG_NORMAL
				      : CLASS1_ANS_ORG_ANSWER;
	local.s7_timeout = s7;
	local.iir_enable = 1;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf(
		    "fax: fax_class1 will created (ans_org=%d, s7=%d)\n",
		    local.mode, s7);

	ctx->class1 = fax_class1_create(NULL, &local);
	if (ctx->class1 != NULL)
		return ctx;

fail:
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("fax: delete...\n");
	if (ctx->rc_a != NULL)
		RcFixed_Delete(ctx->rc_a);
	if (ctx->rc_b != NULL)
		RcFixed_Delete(ctx->rc_b);
	if (ctx->class1 != NULL)
		fax_class1_delete(ctx->class1);
	ctx->class1 = NULL;
	sysdep_free(ctx);
	return NULL;
}

/*
 * `.text` 0x001740, 708 bytes.  `cmd` is one of the six `FAXC1_*` codes
 * (`fax.h`), a DIFFERENT numbering from `fax_class1_command`'s own
 * `FAX_CLASS1_*_COMMAND` (class1.h) that this function remaps into via its
 * own `switch`.  `arg` rides through as a plain int (`(int)(long)arg`,
 * never dereferenced) -- a T.30 rate code for FTM/FRM, or a raw millisecond/
 * sample count for FTS/FRS (`fax_class1_command`'s own `arg3`).
 *
 *   1. `ctx == NULL || ctx->class1 == NULL` returns -1 immediately.
 *   2. Debug print ("fax: FAX_class1_command: %x\n", `cmd`) at level > 1,
 *      unconditionally, before validating `cmd` at all.
 *   3. `(unsigned)cmd > 5`: debug print ("fax: bad command: %x\n") at
 *      level > 1, return -1.
 *   4. Otherwise dispatch on `cmd`:
 *      FAXC1_FTS/FAXC1_FRS: no validation on `rate` at all; debug print
 *        ("fax:  FAXC1_FTS, %x\n"/"...FRS...") at level > 1 with the raw
 *        value, remap to FAX_CLASS1_TS_COMMAND/RS_COMMAND.
 *      FAXC1_FTM/FAXC1_FRM: debug print FIRST (unconditionally, if level >
 *        1 -- FTM's own print always shows 0 for its second `%d`, since
 *        `extra` is not yet computed at that point), THEN validate `rate`
 *        against the twelve T.30 codes `_set_modem_rate` recognises
 *        (0x18/0x30/0x48/0x49/0x4a/0x60/0x61/0x62/0x79/0x7a/0x91/0x92);
 *        anything else returns -1.  FTM alone also sets a fourth argument
 *        to the LITERAL 80 (`fax_class1_command`'s own `arg4`, read only by
 *        its TM command, into `silence_blocks`) -- every other remap leaves
 *        that argument at whatever this function's own caller happened to
 *        leave in the register, a genuinely uninitialised value the object
 *        itself never reads back for those five commands.
 *      FAXC1_FTH/FAXC1_FRH: debug print FIRST at level > 1, THEN require
 *        `rate == 3` exactly (the V.21 control-channel sentinel); anything
 *        else returns -1.
 *   5. `fax_class1_command(ctx->class1, remapped_cmd, rate, extra)`'s own
 *      return is DISCARDED; this function always returns 1 on a validated
 *      dispatch.
 */
int
FAX_class1_command(struct fax_ctx *ctx, int cmd, void *arg)
{
	int rate = (int)(long)arg;
	int inner_cmd;
	int extra;

	if (ctx == NULL || ctx->class1 == NULL)
		return -1;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("fax: FAX_class1_command: %x\n", cmd);

	if ((unsigned)cmd > 5) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("fax: bad command: %x\n", cmd);
		return -1;
	}

	switch (cmd) {
	case FAXC1_FTS:
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("fax:  FAXC1_FTS, %x\n", rate);
		inner_cmd = FAX_CLASS1_TS_COMMAND;
		break;

	case FAXC1_FRS:
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("fax:  FAXC1_FRS, %x\n", rate);
		inner_cmd = FAX_CLASS1_RS_COMMAND;
		break;

	case FAXC1_FTM:
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("fax:  FAXC1_FTM, %x, %d\n",
					     rate, 0);
		if (rate != 0x18 && rate != 0x30 && rate != 0x48 &&
		    rate != 0x49 && rate != 0x4a && rate != 0x60 &&
		    rate != 0x61 && rate != 0x62 && rate != 0x79 &&
		    rate != 0x7a && rate != 0x91 && rate != 0x92)
			return -1;
		inner_cmd = FAX_CLASS1_TM_COMMAND;
		extra = 0x50;
		break;

	case FAXC1_FRM:
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("fax:  FAXC1_FRM, %x\n", rate);
		if (rate != 0x18 && rate != 0x30 && rate != 0x48 &&
		    rate != 0x49 && rate != 0x4a && rate != 0x60 &&
		    rate != 0x61 && rate != 0x62 && rate != 0x79 &&
		    rate != 0x7a && rate != 0x91 && rate != 0x92)
			return -1;
		inner_cmd = FAX_CLASS1_RM_COMMAND;
		break;

	case FAXC1_FTH:
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("fax:  FAXC1_FTH, %x\n", rate);
		if (rate != 3)
			return -1;
		inner_cmd = FAX_CLASS1_TH_COMMAND;
		break;

	default:	/* FAXC1_FRH */
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("fax:  FAXC1_FRH %x\n", rate);
		if (rate != 3)
			return -1;
		inner_cmd = FAX_CLASS1_RH_COMMAND;
		break;
	}

	(void)fax_class1_command(ctx->class1, inner_cmd, rate, extra);
	return 1;
}

/*
 * `.text` 0x001a10, 1,809 bytes.  See `fax.h`'s struct banner for the
 * field-by-field evidence on every `fax_ctx` member this function reaches;
 * this banner is the CONTROL-FLOW account.
 *
 * OUTER LOOP: while `count > 0`, take `chunk = min(count, ctx->
 * host_frame_samples)` samples' worth of `in`/`out`, process them (below),
 * then `count -= chunk` and advance `in`/`out` by `chunk` -- UNSCALED, the
 * same raw value subtracted from `count`, which is why `in`/`out` are
 * `void *` (`fax.h`'s own note).
 *
 * INNER LOOP, once per outer chunk: copy up to `host_frame_samples` samples
 * from the current `in` position into `in_ring` at `in_write_cursor`
 * (`sysdep_memcpy`), bounded so the copy never runs past `in_ring`'s own
 * end (`2 * host_frame_samples` samples) OR past `out_ring`'s end at the
 * `out_read_cursor` THIS OUTER ITERATION STARTED WITH -- that snapshot is
 * taken once per outer iteration and reused for every inner pass even
 * though the live cursor advances each time, a faithfully-reproduced
 * property of the object and not resolved further.  `in_pending` and
 * `in_write_cursor` are updated to match.
 *
 * When `in_pending` reaches a full `host_frame_samples`, FLUSH:
 *
 *   - Resample `in_ring` (at `in_read_half`) into `rx_resampled` via
 *     `RcFixed_Resample`/`rc_a` when `rc_a != NULL`; when NULL, use the
 *     `in_ring` position directly as `rx` (identity: no rate conversion
 *     needed) and pass `out_ring` (at `out_write_half`) directly as `tx`
 *     instead of the `tx_pump_rate` scratch.
 *   - If the resample's own output count didn't come back as exactly 0xa0
 *     (only reachable when `rc_a != NULL` and `host_frame_samples != 0xa0`,
 *     an edge configuration), log the mismatch and skip `fax_class1_
 *     progress` entirely for this flush, carrying a -1 sentinel through to
 *     the accumulator below.
 *   - Otherwise: poll `modem_recv_from_tty` for up to `host_rx_want`
 *     (clamped to 0x1000) bytes into `host_rx_buf`, gated on `host_rx_
 *     enable` and `host_rx_want` both being nonzero; call `fax_class1_
 *     progress`; store its `word8` result back into `host_rx_want`; and,
 *     when `host_rx_enable` is set and `word7` came back positive, forward
 *     `word7` bytes of `host_tx_buf` via `modem_send_to_tty`.  Dispatch the
 *     call's own `FAX_CLASS1_*` return (table below) to get this flush's
 *     forced status and any `host_rx_enable` transition.
 *   - When `rc_b != NULL`, resample `tx_pump_rate` into the `out_ring`
 *     position via `rc_b` (a no-op when `rc_a == NULL`, since `tx_pump_rate`
 *     was never the target `fax_class1_progress` wrote into).
 *   - A nonzero forced status becomes this OUTER iteration's `last_status`;
 *     `in_pending -= host_frame_samples`, `in_read_half` and `out_
 *     write_half` both toggle between 0 and `host_frame_samples`, and
 *     `out_produced += host_frame_samples`.
 *
 * Then, EVERY inner pass regardless of whether it flushed: drain the same
 * sample count back out of `out_ring` at `out_read_cursor` into the
 * current `out` position, and advance `out_read_cursor`/`out_produced`/
 * `out`/`count` to match.
 *
 * At each outer iteration's end, if that iteration's `last_status` is
 * nonzero, it becomes the function's own running return value -- so the
 * return is the LAST nonzero forced status seen across the whole call, and
 * an iteration that never flushed (or whose flushes were all
 * `FAX_CLASS1_NO_MESSAGE`) leaves the previous iteration's answer standing.
 *
 * THE DISPATCH TABLE, on `fax_class1_progress`'s own return (`class1.h`'s
 * `FAX_CLASS1_*`).  Every debug string below is gated on
 * `DSPLIB_DEBUG_ON()`; the two right columns are the flush's own forced
 * status and its effect on `host_rx_enable`:
 *
 *     status                          forced   host_rx_enable
 *     FAX_CLASS1_NO_MESSAGE       0    (same)   untouched
 *     FAX_CLASS1_OK                1    1        = 0
 *     FAX_CLASS1_ERROR             2    2        = 0
 *     FAX_CLASS1_OK_NO_CARRIER     3    1        = 0
 *     FAX_CLASS1_ERROR_NO_CARRIER  4    2        = 0
 *     FAX_CLASS1_ERROR_ON_HOOK     5    2        = 0
 *     FAX_CLASS1_CONNECT           6    3        = 1
 *     FAX_CLASS1_NO_CARRIER        7    4        = 0
 *     FAX_CLASS1_NO_CARRIER_NO_MESSAGE 8  0      = 0
 *     FAX_CLASS1_OTHER_CARRIER     9    0        = 0
 *     FAX_CLASS1_ACCEPT_RATE      10    0        untouched
 *     (anything else)                   2        untouched, logs
 *                                                 "fax: process: Unknown
 *                                                 status %d\n"
 *
 * `FAX_CLASS1_NO_MESSAGE` prints nothing and changes nothing -- its own
 * table entry lands mid-way into the shared "reload and continue" tail the
 * out-of-range default case also falls into, which is why both share no
 * dedicated code of their own.
 */
int
FAX_process(struct fax_ctx *ctx, const void *in, void *out, int count)
{
	int ret;

	ret = 0;
	while (count > 0) {
		int host_frame, chunk, remaining;
		int out_read_cursor_snap;
		const unsigned char *in_cur;
		unsigned char *out_cur;
		int last_status;

		host_frame = ctx->host_frame_samples;
		chunk = (count < host_frame) ? count : host_frame;

		in_cur = (const unsigned char *)in;
		out_cur = (unsigned char *)out;
		remaining = chunk;
		last_status = 0;
		out_read_cursor_snap = ctx->out_read_cursor;

		while (remaining > 0) {
			int cap, copy, room;
			short *in_ptr, *out_ptr;
			short *rx_buf, *tx_buf;
			int rx_count;
			int ebx;

			cap = 2 * host_frame;

			copy = host_frame;
			if (copy > remaining)
				copy = remaining;
			room = cap - ctx->in_write_cursor;
			if (copy > room)
				copy = room;
			room = cap - out_read_cursor_snap;
			if (copy > room)
				copy = room;

			sysdep_memcpy(&ctx->in_ring[ctx->in_write_cursor],
			    in_cur, copy * (int)sizeof(short));
			in_cur += copy * (int)sizeof(short);

			ctx->in_pending += copy;
			ctx->in_write_cursor =
			    (ctx->in_write_cursor + copy) % cap;

			if (ctx->in_pending >= host_frame) {
				in_ptr = &ctx->in_ring[ctx->in_read_half];
				out_ptr = &ctx->out_ring[ctx->out_write_half];

				if (ctx->rc_a != NULL) {
					int oc = 0xa0;

					RcFixed_Resample(ctx->rc_a, in_ptr,
					    host_frame, ctx->rx_resampled,
					    &oc);
					rx_buf = ctx->rx_resampled;
					rx_count = oc;
					tx_buf = ctx->tx_pump_rate;
				} else {
					rx_buf = in_ptr;
					rx_count = host_frame;
					tx_buf = out_ptr;
				}

				if (rx_count != 0xa0) {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "fax: process: samples count %d != %d\n",
						    rx_count, 0xa0);
					ebx = -1;
				} else {
					int rxc, txc, w7, recv_n;

					rxc = 0xa0;
					txc = 0xa0;
					w7 = 0;
					recv_n = 0;

					if (ctx->host_rx_enable != 0 &&
					    ctx->host_rx_want != 0) {
						int want = ctx->host_rx_want;

						if (want > 0x1000)
							want = 0x1000;
						recv_n = modem_recv_from_tty(
						    ctx->modem,
						    ctx->host_rx_buf, want);
						if (recv_n > 0 &&
						    DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: snd: %d (%d)\n",
							    recv_n,
							    ctx->host_rx_want);
					}

					int status, forced;

					status = fax_class1_progress(
					    ctx->class1, rx_buf, tx_buf,
					    (int)(long)ctx->host_tx_buf,
					    (int)(long)ctx->host_rx_buf,
					    &rxc, &txc, &w7, &recv_n);
					ctx->host_rx_want = recv_n;

					if (ctx->host_rx_enable != 0 &&
					    w7 > 0) {
						modem_send_to_tty(ctx->modem,
						    ctx->host_tx_buf, w7);
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: rcv: %d (%d)\n",
							    recv_n, w7);
					}

					forced = 0;
					switch (status) {
					case FAX_CLASS1_NO_MESSAGE:
						break;
					case FAX_CLASS1_OK:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_OK\n");
						forced = 1;
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_ERROR:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_ERROR\n");
						forced = 2;
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_OK_NO_CARRIER:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_OK_NO_CARRIER\n");
						forced = 1;
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_ERROR_NO_CARRIER:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_ERROR_NO_CARRIER\n");
						forced = 2;
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_ERROR_ON_HOOK:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_ERROR_ON_HOOK\n");
						forced = 2;
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_CONNECT:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_CONNECT\n");
						forced = 3;
						ctx->host_rx_enable = 1;
						break;
					case FAX_CLASS1_NO_CARRIER:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_NO_CARRIER\n");
						forced = 4;
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_NO_CARRIER_NO_MESSAGE:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_NO_CARRIER_NO_MESSAGE\n");
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_OTHER_CARRIER:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_OTHER_CARRIER\n");
						ctx->host_rx_enable = 0;
						break;
					case FAX_CLASS1_ACCEPT_RATE:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: FAX_CLASS1_ACCEPT_RATE\n");
						break;
					default:
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
							    "fax: process: Unknown status %d\n",
							    status);
						forced = 2;
						break;
					}
					ebx = forced;
				}

				if (ctx->rc_b != NULL) {
					int oc2 = 0xa0;

					RcFixed_Resample(ctx->rc_b,
					    ctx->tx_pump_rate, host_frame,
					    out_ptr, &oc2);
				}

				if (ebx != 0)
					last_status = ebx;

				ctx->in_pending -= host_frame;
				ctx->in_read_half =
				    (ctx->in_read_half != 0) ? 0 : host_frame;
				ctx->out_produced += host_frame;
				ctx->out_write_half =
				    (ctx->out_write_half != 0) ? 0 :
				    host_frame;
			}

			sysdep_memcpy(out_cur,
			    &ctx->out_ring[ctx->out_read_cursor],
			    copy * (int)sizeof(short));
			out_cur += copy * (int)sizeof(short);
			ctx->out_produced -= copy;
			ctx->out_read_cursor =
			    (ctx->out_read_cursor + copy) % cap;

			remaining -= copy;
		}

		if (last_status != 0)
			ret = last_status;

		count -= chunk;
		in = (const unsigned char *)in + chunk;
		out = (unsigned char *)out + chunk;
	}

	return ret;
}

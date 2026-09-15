/*
 * cDATAtx.c -- original transmit data-mode translation unit.
 *
 * FILE symbol 567 owns LOCAL cDATAtx_counter (symbol 568, .bss+0x8c0).
 * Its eight reference relocations belong to these three functions.
 * Bodies are retained verbatim; emission order follows their reference
 * addresses: 0x9cf70, 0x9d200, 0x9d4b0.
 */
#include "dsplib/class1.h"
#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/faxfifo.h"
#include "dsplib/faxvmi.h"

static int cDATAtx_counter;

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
 * `data_input_closed` and the file-static `cDATAtx_counter`
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
	cDATAtx_counter = 0;
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
 * `cDATAtx_counter` (above, shared with `_tx_data_state`) gates a ONE-TIME
 * `ctx->status = FAX_CLASS1_CONNECT` on the session's very first call into
 * this pair.
 *
 * `ctx->tx_connect_countdown` is a one-shot countdown, decremented once per call while
 * positive; reaching exactly 0 fires "At %2d.%02d[sec] ENABLE_TRANSMIT in
 * _tx_scrambled_ones_state\n" and sets `ctx->transmit_enabled = 1`.
 *
 * `*tx_data_count > 0` unstuffs `src` through `_handle_data_input` into `ctx`
 * (the shared scratch-buffer idiom), then `FIFO_write`s the result into
 * `ctx->tx_fifo`, logging a shortfall ("Fifo is full in
 * _tx_scrambled_ones_state").  `ctx->tx_fifo_ready` is then recomputed: 1 when
 * `ctx->tx_fifo->count >= ctx->tx_bytes_per_block` (the FIFO already holds a whole read's
 * worth), else 0 -- but ONLY inside this `*tx_data_count > 0` block; on a call
 * where it does not run, `tx_fifo_ready` is left at whatever the last call set.
 *
 * `ctx->tx_bytes_per_block` elements of `ctx` are then filled with the literal `0xff`
 * (the "scrambled ones" this state's name promises) UNCONDITIONALLY, and
 * `*tx_data_count` is set to that same count.
 *
 * If `ctx->transmit_enabled != 0 && ctx->tx_fifo_ready != 0`: `ctx->state` becomes
 * `CLASS1_TX_DATA_STATE`, and the 0xFF filler is immediately overwritten by
 * a REAL `FIFO_read` into the same buffer -- `*tx_data_count` becomes that read's
 * return, logging an underrun ("class1 object fifo under run in
 * _tx_scrambled_ones_state !!!") without undoing the state change.
 *
 * `cnt` (FAXVMI_process's `count`) is seeded from `*tx_data_count` AS IT STANDS AT
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
 * Tail, unconditional: `*tx_data_count = ctx->tx_fifo->size - ctx->tx_fifo->count - 1`
 * -- the same free-room-minus-one formula `_tx_nulls_state`/`_tx_data_state`
 * end with.
 *
 * FORMAT STRINGS, verified against `.rodata.str1.1`/`.rodata.str1.4`:
 *   0x48c2 (.str1.1)  "cDATAtx_counter %d\n" (shares `cDATAtx_counter`)
 *   0x48d6 (.str1.1)  "At %2d.%02d[sec] Tx connect\n"
 *   0x12448  "class1 object fifo under run in _tx_scrambled_ones_state !!!\n"
 *   0x12488  "At %2d.%02d[sec] ENABLE_TRANSMIT in _tx_scrambled_ones_state\n"
 *   0x124c8  "At %2d.%02d[sec] Fifo is full in _tx_scrambled_ones_state\n"
 */
#define FAXVMI_PROCESS_BIT_0100	0x100

int
_tx_scrambled_ones_state(struct fax_class1 *ctx, const short *rx, short *tx,
			 int unused_dst, int src, int *rx_count, int *tx_count,
			 int unused_out_count, int *tx_data_count)
{
	short cnt;
	unsigned short result = (unsigned short)*tx_count;
	int status;
	int i;

	(void)rx;
	(void)unused_dst;
	(void)rx_count;
	(void)unused_out_count;

	if (dsplibs_debug_level > 2)
		dsplibs_debug_printf("cDATAtx_counter %d\n", cDATAtx_counter);
	cDATAtx_counter++;
	if (cDATAtx_counter == 1)
		ctx->status = FAX_CLASS1_CONNECT;

	if (ctx->tx_connect_countdown > 0) {
		ctx->tx_connect_countdown--;
		if (ctx->tx_connect_countdown == 0) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "At %2d.%02d[sec] ENABLE_TRANSMIT in " "_tx_scrambled_ones_state\n",
				    ctx->clock_sec, ctx->clock_frac);
			ctx->transmit_enabled = 1;
		}
	}

	if (*tx_data_count > 0) {
		int n;

		_handle_data_input(ctx, (const unsigned char *)(long)src,
		    (unsigned short *)(void *)ctx, tx_data_count);

		n = (unsigned short)FIFO_write(ctx->tx_fifo,
		    (unsigned short *)(void *)ctx, (unsigned short)*tx_data_count);
		if (*tx_data_count > n) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "At %2d.%02d[sec] Fifo is full in " "_tx_scrambled_ones_state\n",
				    ctx->clock_sec, ctx->clock_frac);
		}

		ctx->tx_fifo_ready = (ctx->tx_fifo->count >= (unsigned)ctx->tx_bytes_per_block) ? 1
									  : 0;
	}

	for (i = 0; i < ctx->tx_bytes_per_block; i++)
		((unsigned short *)(void *)ctx)[i] = 0xff;
	*tx_data_count = ctx->tx_bytes_per_block;

	if (ctx->transmit_enabled != 0 && ctx->tx_fifo_ready != 0) {
		int rd;

		ctx->state = CLASS1_TX_DATA_STATE;
		rd = FIFO_read(ctx->tx_fifo, (unsigned short *)(void *)ctx,
		    (unsigned short)ctx->tx_bytes_per_block);
		*tx_data_count = rd;
		if (rd < ctx->tx_bytes_per_block) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "class1 object fifo under run in " "_tx_scrambled_ones_state !!!\n");
		}
	}

	cnt = (short)*tx_data_count;
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

	*tx_data_count = (int)(unsigned short)ctx->tx_fifo->size
	       - (int)(unsigned short)ctx->tx_fifo->count - 1;
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
 *   0x48c2 (.str1.1)  "cDATAtx_counter %d\n" (shares `cDATAtx_counter` above)
 *   0x12504  "At %2d.%02d[sec] Fifo is full in _tx_data_state(%d=>%d>%d)\n"
 *   0x12540  "At %2d.%02d[sec] Queue is full in _tx_data_state\n"
 *   0x12574  "At %2d.%02d[sec] fifo underrun in _tx_data_state, "
 *            "count %d < %d\n"
 *   0x125b8  "At %2d.%02d[sec] Queue underrun, Stop TX\n"
 *   0x125e4  "At %2d.%02d[sec] Queue underrun, Continue NULLS\n"
 */

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
		dsplibs_debug_printf("cDATAtx_counter %d\n", cDATAtx_counter);
	cDATAtx_counter++;

	if (*word8 > 0) {
		int n;

		_handle_data_input(ctx, (const unsigned char *)(long)word4,
		    (unsigned short *)(void *)ctx, word8);

		n = (short)FIFO_write(ctx->tx_fifo, (unsigned short *)(void *)ctx,
		    (unsigned short)*word8);
		if (n < *word8) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "At %2d.%02d[sec] Fifo is full in " "_tx_data_state(%d=>%d>%d)\n",
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
				    "At %2d.%02d[sec] fifo underrun in " "_tx_data_state, count %d < %d\n",
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
				    "At %2d.%02d[sec] Queue underrun, Stop " "TX\n",
				    ctx->clock_sec, ctx->clock_frac);
			ctx->state = CLASS1_IDLE_STATE;
			ctx->status = FAX_CLASS1_OK_NO_CARRIER;
		} else {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "At %2d.%02d[sec] Queue underrun, " "Continue NULLS\n",
				    ctx->clock_sec, ctx->clock_frac);
			ctx->state = CLASS1_TX_NULLS_STATE;
			ctx->countdown = 0;
			ctx->status = FAX_CLASS1_CONNECT;
		}
	}

	if (status & FAXVMI_STATUS_FULL) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec] Queue is full in " "_tx_data_state\n",
			    ctx->clock_sec, ctx->clock_frac);
	}

	*word8 = ctx->tx_fifo->size - ctx->tx_fifo->count - 1;
	return 0;
}


/**
 * @file class1tx.h
 * @brief Class 1 fax: the per-modulation message reporters, the state
 *        machine's transmit/receive-side handlers, and the two host-link
 *        codecs (`_handle_data_*`/`_handle_hdlc_*`).
 *
 * The fax phase is deliberately last (see CLAUDE.md); everything declared
 * here was originally exported API with no internal referrer (finding
 * F8320's no-entry-point bucket), written on its own merit. The blob's
 * `class1tx.c +94` span cannot be split into author files by
 * `tools/tumap.py`, so the file split on our side follows the address
 * clusters: the message functions and their tables sit together at
 * 0x09bf20..0x09cad0, between the modem wrappers and the HDLC machine.
 *
 * The message contract: every `vNNxx_message(handle, code, out)` ignores
 * its handle and answers through `out`, the code's string from that
 * modulation's table, or NULL above the table's guard. The guard is the
 * entry count, not count-minus-one, in all eight functions -- `code == N`
 * reads one entry past an N-entry table -- and that is the object's own
 * off-by-one, reproduced (docs/deviations.md D951). The tables are
 * exported, writable `.data` in the object and are the same here.
 */

#ifndef DSPLIB_CLASS1TX_H
#define DSPLIB_CLASS1TX_H

struct fax_class1;
struct faxvmi_cfg;
struct v17tx_cfg;
struct v27tx_cfg;
struct v29tx_cfg;

/** The tables: the author's strings, verbatim (typos included). */
extern char *V17RX_MESG[10];
extern char *V17TX_MESG[10];
extern char *V21RX_MESG[7];
extern char *V21TX_MESG[6];
extern char *V27RX_MESG[8];
extern char *V27TX_MESG[8];
extern char *V29RX_MESG[8];
extern char *V29TX_MESG[8];

/** @brief Look up a V.17 receive status message. See this file's message contract. */
void v17rx_message(void *handle, int code, char **out);
/** @brief Look up a V.17 transmit status message. See this file's message contract. */
void v17tx_message(void *handle, int code, char **out);
/** @brief Look up a V.21 receive status message. See this file's message contract. */
void v21rx_message(void *handle, int code, char **out);
/** @brief Look up a V.21 transmit status message. See this file's message contract. */
void v21tx_message(void *handle, int code, char **out);
/** @brief Look up a V.27ter receive status message. See this file's message contract. */
void v27rx_message(void *handle, int code, char **out);
/** @brief Look up a V.27ter transmit status message. See this file's message contract. */
void v27tx_message(void *handle, int code, char **out);
/** @brief Look up a V.29 receive status message. See this file's message contract. */
void v29rx_message(void *handle, int code, char **out);
/** @brief Look up a V.29 transmit status message. See this file's message contract. */
void v29tx_message(void *handle, int code, char **out);

/** @brief The null modem's message lookup: always sets `*out = NULL`, for every code. */
void null_message(void *handle, int code, char **out);

/**
 * Entry `i` is `i` with its eight bits reversed. `.rodata` 0xba40, global,
 * and defined in `class1tx.c` -- see the comment there for why that file and
 * not `t30frame.c`, whose `GetT30FrameIDFromBuffer` is the one reference out
 * of five that is not in this span.
 */
extern const unsigned char aReversedCharsArray[256];

/**
 * @brief TX_NULLS_STATE's own re-init: clear the session countdown.
 *
 * The tx-nulls state itself then runs until told otherwise by another
 * state transition.
 *
 * @param ctx  The fax session.
 * @return Always 0.
 */
int _init_tx_nulls_state(struct fax_class1 *ctx);

/**
 * @brief Open a fresh HDLC receive frame in the session's scratch buffer.
 *
 * Sets `ctx->hdlc_write_cursor = 1`, leaving element zero of the scratch
 * buffer free for the length `_handle_hdlc_input_close()` will report there.
 *
 * @param ctx  The fax session.
 * @return Always 0.
 */
int _handle_hdlc_input_open(struct fax_class1 *ctx);

/**
 * @brief Close an HDLC receive frame in the session's scratch buffer.
 *
 * Records the count of what was received into element zero
 * (`hdlc_write_cursor - 1`) and, when `flags004` bit 4 is set, latches
 * `frame_end_latch`.
 *
 * @param ctx  The fax session.
 * @return Always 0.
 */
int _handle_hdlc_input_close(struct fax_class1 *ctx);

/**
 * @brief HDLC_RECEIVE_STATE's own re-init: clear the session countdown.
 *
 * Logs a debug line first.
 *
 * @param ctx  The fax session.
 * @return Always 0.
 */
int _hdlc_receive_state_init(struct fax_class1 *ctx);

/**
 * @brief SEND_HDLC_BETWEEN_BUFFER_STATE's own re-init.
 *
 * Clears the session countdown and opens an HDLC frame -- a tail call to
 * _handle_hdlc_input_open(), whose return value it passes through.
 *
 * @param ctx  The fax session.
 * @return _handle_hdlc_input_open()'s return, always 0.
 */
int _send_hdlc_between_buffer_state_init(struct fax_class1 *ctx);

/*
 * ------------------------------------------------------------------
 * The four re-init functions below were the last of `class1tx.c +94`'s
 * leaves blocked on `FAXVMI_control`/`vxx_control` landing (F10115). Each
 * merges a per-modulation V.21 `.data` REINIT request template with the
 * all-zero `FAXVMI_CTL` and calls `FAXVMI_control` -- the same shape
 * `class1rx.c`'s `_init_receiver` uses for the three data modulations,
 * here applied to the fixed-rate V.21 control channel. See class1tx.c for
 * the full per-function derivation and for `V21RX_CTL`/`V21TX_CTL`,
 * defined there.
 *
 * `cHDLCtx_off_init` (0x9e9d0, 149 bytes) is not one of these four: nothing
 * in the object reaches it -- `objdump -r` over the whole 1.2 MB shows zero
 * relocations of either kind (F8493's call/data-store pair) naming it --
 * so it was scheduled and written separately, on its own merit, once its
 * `FAXVMI_control` dependency and the seven `v??tx_control`/`v??rx_control`
 * functions behind it existed. It is the quiescent half of
 * `_cHDLCrx_init_from_idle` below.
 */

/**
 * @brief RX_LOOK_CARRIER's own re-init.
 *
 * `.text` 0x9cb00, 45 bytes. Reinitialises the data-mode receiver through
 * _init_receiver() (class1rx.c), resets the async octet-recovery search
 * (cTOOLS_handle_data_output_reset()), and clears `countdown`.
 *
 * @param ctx        The fax session.
 * @param rate_code  A T.30 modem-rate code, forwarded to _init_receiver().
 * @return Always 0.
 */
int _rx_look_carrier_init(struct fax_class1 *ctx, int rate_code);

/**
 * @brief TX_SCRAMBLED_ONES_STATE's own re-init.
 *
 * `.text` 0x9cf70, 193 bytes. Reinitialises the data-mode transmitter
 * through _init_transmitter() (class1tx.c), (re)builds the transmit FIFO
 * (`ctx->tx_fifo`) at a fixed 0x800-element capacity, and derives
 * `ctx->tx_bytes_per_block` as `ctx->tx_rate / 400` (signed division;
 * matches the object's own reciprocal for every rate class1.c's
 * `_sym_size` recognises). Clears `tx_connect_countdown`,
 * `tx_connect_latch`, `transmit_enabled`, `tx_fifo_ready`,
 * `data_input_closed` and the file-static `DATAtx_counter`
 * (`_tx_scrambled_ones_state`'s own counter, class1tx.c).
 *
 * @param ctx        The fax session.
 * @param rate_code  A T.30 modem-rate code, forwarded to _init_transmitter().
 * @return Always 0.
 */
int _tx_scrambled_ones_init(struct fax_class1 *ctx, int rate_code);

/**
 * @brief T30_PREAMBLE_STATE's own re-init, over the V.21 TX control channel.
 *
 * `.text` 0x9e380, 193 bytes. Merges `V21TX_CTL` (the REINIT bit OR'd into
 * `flags_0d`) into `FAXVMI_CTL` and sends it through
 * `FAXVMI_control(ctx->vmi_c, ...)` -- no ring-clear, no framer reset, no
 * mode change; only `int_0014` (the recursion into `v21tx_control`) is
 * nonzero. Opens an HDLC frame (_handle_hdlc_input_open(), return
 * discarded) and resets `countdown`, `state` (to
 * #CLASS1_T30_SILENCE_BEFORE_PREAMBLE_STATE), `hdlc_frame_done`,
 * `buffers_sent` and `frame_end_latch`.
 *
 * @param ctx  The fax session.
 * @return Always 0.
 */
int cHDLCtx_preamble_state_init(struct fax_class1 *ctx);

/**
 * @brief HDLC_RECEIVE_LOOK_CARRIER_STATE's own re-init, from the idle state.
 *
 * `.text` 0x9d790, 226 bytes. Merges `V21RX_CTL` (REINIT bit OR'd in) into
 * a `FAXVMI_ctl` that also forces a full framer reset (`int_000c = 1`,
 * `short_0010 = 2`) and empties the ring (`ptr_0000 = (void *)1`) --
 * unlike the TX-side sibling above, which forces none of that -- and sends
 * it through `FAXVMI_control(ctx->vmi_a, ...)`. Clears `countdown` and
 * `delayed_status_countdown` unconditionally.
 *
 * @param ctx   The fax session.
 * @param arg2  Both callers (fax_class1_create(), fax_class1_command())
 *              pass their own second argument straight through. When
 *              `arg2 == 3`, this also sets `ctx->state` to
 *              #CLASS1_HDLC_RECEIVE_LOOK_CARRIER_STATE and becomes the
 *              function's own return value, overriding whatever
 *              `FAXVMI_control` returned.
 * @return `arg2` when `arg2 == 3`; otherwise whatever `FAXVMI_control` returned.
 */
int _cHDLCrx_init_from_idle(struct fax_class1 *ctx, int arg2);

/**
 * @brief The quiescent half of _cHDLCrx_init_from_idle(). No entry point in
 *        the object reaches it (see the note above).
 *
 * `.text` 0x9e9d0, 149 bytes. The same `V21RX_CTL`-sourced,
 * `V21RXCTL_REINIT`-forced request, merged into the same full-framer-reset
 * `FAXVMI_ctl` and sent to the same `ctx->vmi_a` as
 * _cHDLCrx_init_from_idle(), but with no state transition and no
 * `delayed_status_countdown` clear. Also clears `countdown`.
 *
 * @param ctx  The fax session.
 * @return Whatever `FAXVMI_control` returned.
 */
int cHDLCtx_off_init(struct fax_class1 *ctx);

/*
 * The host link is DLE-stuffed bytes one way and 16-bit elements the
 * other. `_handle_data_input` and `_handle_hdlc_input` take
 * `unsigned char *` at a stride of one and write `unsigned short *` at a
 * stride of two; `_handle_data_output` goes the other way. Every load is a
 * `movzbl`, so only the low eight bits of an element ever carry data.
 *
 * `count` is the destination's SIZE, not a request, in all three -- see the
 * per-function notes below for what each one can write past its input
 * length. Sizing a destination from the input alone is F8607/D956.
 */

/**
 * @brief Unstuff a transmit block from the host link.
 *
 * `*count` in is the byte count, out is the element count -- and is zero
 * from then on once DLE ETX has been seen. Writes at most `*count + 20`
 * elements: the padding after DLE ETX is twenty long.
 *
 * @param ctx    The fax session.
 * @param src    DLE-stuffed input bytes.
 * @param dst    Unstuffed output elements.
 * @param count  In: byte count of `src`. Out: element count written to `dst`.
 * @return Always 0.
 */
int _handle_data_input(struct fax_class1 *ctx, const unsigned char *src,
		       unsigned short *dst, int *count);

/**
 * @brief Unstuff one HDLC frame from the host link, across calls.
 *
 * The same shape as _handle_data_input(), except the write cursor is kept
 * in the session (`hdlc_write_cursor`) so a frame accumulates across calls
 * -- `dst` is the whole frame's buffer, not one block's.
 *
 * @param ctx    The fax session.
 * @param src    DLE-stuffed input bytes.
 * @param dst    The frame buffer being accumulated.
 * @param count  In: byte count of `src`. Out: the frame length on the call
 *               that completes it, else zero.
 * @return 1 on the call that completes a frame, 0 otherwise.
 */
int _handle_hdlc_input(struct fax_class1 *ctx, const unsigned char *src,
		       unsigned short *dst, int *count);

/**
 * @brief Reset _handle_data_output()'s start-bit search.
 *
 * Clears `async_locked` and sets `async_window` to -1. The shift and mask
 * are left where they are.
 *
 * @param ctx  The fax session.
 */
void cTOOLS_handle_data_output_reset(struct fax_class1 *ctx);

/**
 * @brief Recover octets from demodulated data and DLE-stuff them to the host.
 *
 * Recovers an octet from each of `count` elements, DLE-stuffs them into
 * `dst`, and appends DLE ETX when `terminate` is set.
 *
 * @param ctx        The fax session.
 * @param src        Demodulated elements to recover octets from.
 * @param dst        DLE-stuffed output bytes.
 * @param count      Number of elements in `src`.
 * @param terminate  Non-zero to append DLE ETX after the data.
 * @return The byte count written, which can reach `2 * count + 2`.
 */
int _handle_data_output(struct fax_class1 *ctx, const unsigned short *src,
			unsigned char *dst, int count, int terminate);

/**
 * @brief The receive-side mirror of _handle_data_output(), for HDLC frames.
 *
 * Without _handle_data_output()'s async start-bit search: `src`'s elements
 * are already-aligned HDLC receive bytes, so only DLE-stuffing and the DLE
 * ETX terminator are needed. "hdlc_output" here means output to the HOST
 * of a frame the modem RECEIVED -- see class1tx.c.
 *
 * @param ctx        The fax session.
 * @param src        Already-aligned HDLC receive elements.
 * @param dst        DLE-stuffed output bytes.
 * @param count      Number of elements in `src`.
 * @param terminate  Non-zero to append DLE ETX after the data.
 * @return The byte count written, which can reach `2 * count + 2`.
 */
int cTOOLS_handle_hdlc_output(struct fax_class1 *ctx, const unsigned short *src,
			      unsigned char *dst, int count, int terminate);

/*
 * ------------------------------------------------------------------
 * The transmit-side VMI constructors. By address, not by span name: they
 * sit at 0x094870..0x094b6f, immediately after `class1rx.c`'s own
 * `_init_receiver` (0x094240) and immediately before `_delete_data_tx_modem`
 * and `_init_transmitter` -- so this run of `.text` is the TX twin of
 * `class1rx.c`'s RX trio, one span later, and `tools/readyqueue.py` already
 * files all three under `class1tx.c +94` on that same address evidence.
 * See class1tx.c for the derivation.
 *
 * All three are `t` in the object (file-local, only called from
 * `_init_transmitter`, in the same translation unit) and are declared
 * `extern` here, for the identical reason `class1rx.h`'s RX trio already
 * gives (D1081): a `static` spelling would be three functions this tree
 * could neither reach nor test. D1450 records it for this trio; now that
 * `_init_transmitter` is written (class1tx.c) but still in the same file,
 * taking all three back to `static` remains follow-up work, not done here.
 *
 * Same shape as the RX trio, with three differences. `init_vmi_v29tx`'s
 * config is `struct v29tx_cfg`, whose `int_0018` the table's own default (0)
 * fills unread; `init_vmi_v17tx` overrides its `int_0018` with a hardcoded 0
 * rather than reading the table's copy there at all -- so the two constants
 * end up equal but by different means, and `init_vmi_v17tx` is the one
 * spelled as an explicit store. All three also hardcode `cfg->int_0014`
 * after the table copy -- 1 for V.17 (equal to its table default, so
 * invisible to a value-only check), 2 for V.27ter and V.29 (whose tables
 * both default to 1, confirmed against the blob's own `.data` independently
 * of either reconstructed table) -- caught by `t_class1txvmi.c` disagreeing
 * with the blob rather than assumed absent on a first pass over the
 * disassembly. All three return `(int)cfg->bitrate` -- the object reloads
 * it from the freshly-built config right before `ret`, which nothing
 * declared `void` would do -- so unlike the RX trio these are `int`, not
 * `void`.
 */

/**
 * @brief Build a "No ECM (Simple Packing)" V.17 transmit VMI/config pair.
 *
 * @param vmi       The VMI config block to build in place.
 * @param bit_rate  See class1rx.h's init_vmi_v17rx() note on this
 *                  parameter's width.
 * @param arg_2     Unread.
 * @param arg_3     Stored into both the modem config and the VMI block.
 * @return `(int)cfg->bitrate` of the freshly built config.
 */
int init_vmi_v17tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
		   int arg_2, void *arg_3);

/**
 * @brief Build a "No ECM (Simple Packing)" V.27ter transmit VMI/config pair.
 *
 * @param vmi       The VMI config block to build in place.
 * @param bit_rate  See class1rx.h's init_vmi_v17rx() note on this
 *                  parameter's width.
 * @param arg_2     Unread.
 * @param arg_3     Stored into both the modem config and the VMI block.
 * @return `(int)cfg->bitrate` of the freshly built config.
 */
int init_vmi_v27tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
		   int arg_2, void *arg_3);

/**
 * @brief Build a "No ECM (Simple Packing)" V.29 transmit VMI/config pair.
 *
 * @param vmi       The VMI config block to build in place.
 * @param bit_rate  See class1rx.h's init_vmi_v17rx() note on this
 *                  parameter's width.
 * @param arg_2     Unread.
 * @param arg_3     Stored into both the modem config and the VMI block.
 * @return `(int)cfg->bitrate` of the freshly built config.
 */
int init_vmi_v29tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
		   int arg_2, void *arg_3);

/** The `slot` each of the three constructors above plants, from `faxvmi.h`'s slot map. */
#define VMI_SLOT_V21TX		5
#define VMI_SLOT_V27TX		7
#define VMI_SLOT_V29TX		9
#define VMI_SLOT_V17TX		11

/*
 * `VMI_SLOT_V21RX` (6) is not in the three-constructor group above -- V.21
 * receive has no `init_vmi_v21rx` counterpart written here, since
 * `fax_class1_create` builds its one V.21 RX `faxvmi_cfg` inline rather than
 * through a shared constructor (see class1.c). Declared beside its TX twin
 * because `faxvmi.h`'s own slot table (0..4 null, 5 v21tx, 6 v21rx, 7 v27tx,
 * 8 v27rx, 9 v29tx, 10 v29rx, 11 v17tx, 12 v17rx) already establishes it, and
 * class1rx.h's RX trio does not cover V.21 at all.
 */
#define VMI_SLOT_V21RX		6

/**
 * @brief Tear the transmit-side data modem down.
 *
 * Frees the config, the VMI block, and the FAXVMI handle -- and, when
 * `ctx->tx_fifo` is non-null, that too. Unlike the RX twin
 * (_delete_data_rx_modem(), class1rx.h), no modulation ever needs a
 * sub-allocation freed first.
 *
 * @param ctx  The fax session whose current data transmitter is being torn down.
 */
void _delete_data_tx_modem(struct fax_class1 *ctx);

/**
 * @brief Build or reinitialise the data-mode transmitter for a T.30 rate code.
 *
 * `.text` 0x094bf0, 1,326 bytes -- see `class1tx.c` for the full
 * derivation. The transmit-side counterpart of _init_receiver()
 * (class1rx.h); `rate_code` is the same T.30 modem-rate code space.
 *
 * @param ctx        The fax session.
 * @param rate_code  A T.30 modem-rate code, the same code space
 *                   `_set_modem_rate` (class1.c) recognises.
 */
void _init_transmitter(struct fax_class1 *ctx, int rate_code);

/**
 * @brief TX_SILENCE_BEFORE_SCRM_ONES (18). `.text` 0x09e590, 194 bytes.
 *
 * Adds 20 to `countdown`, transmits a block of silence, and moves to
 * #CLASS1_TX_SCRAMBLED_ONES_STATE once `countdown` (unsigned) reaches
 * `silence_blocks`. `*word8` is always cleared to 0.
 */
int _tx_silence_before_scrm_ones(struct fax_class1 *ctx, const short *rx,
				 short *tx, int word3, int word4,
				 int *rx_count, int *tx_count, int word7,
				 int *word8);

/**
 * @brief T30_SILENCE_BEFORE_TX_STATE. `.text` 0x09d720, 111 bytes.
 *
 * Transmits a block of silence (its return value unused), and once
 * `countdown` (unsigned) exceeds 400, logs a "50MS second, send preamble"
 * line at debug level > 1, moves to #CLASS1_T30_PREAMBLE_STATE, sets
 * #FAX_CLASS1_CONNECT and resets `countdown` to 0. Either way,
 * `countdown += CLASS1_BLOCK_SAMPLES` unconditionally at the end.
 */
int _t30_silence_before_tx_state(struct fax_class1 *ctx, const short *rx,
				 short *tx, int word3, int word4,
				 int *rx_count, int *tx_count, int word7,
				 int *word8);

/**
 * @brief HDLC_EMULATE_RECEIVE_STATE (7). `.text` 0x09e1b0, 452 bytes.
 *
 * Replays whatever length-prefixed records sit in `ctx->superframe` (see
 * class1.h) out to the host, one per call, gated by a two-tick countdown --
 * so it looks like the modem received an HDLC frame without a real
 * demodulator running, which is the function's own name.
 *
 * @param ctx       The fax session.
 * @param word3     The host-facing output buffer, cast from the shared
 *                  `int` slot (the same `(T *)(long)` idiom the VMI
 *                  constructors use).
 * @param word7     An `int *` the byte count is written through.
 * @param rx        Unread.
 * @param word4     Unread.
 * @param rx_count  Unread.
 * @param word8     Unread.
 */
int _hdlc_emulate_receive_state(struct fax_class1 *ctx, const short *rx,
				short *tx, int word3, int word4,
				int *rx_count, int *tx_count, int word7,
				int *word8);

/*
 * ------------------------------------------------------------------
 * The V.21 HDLC control-channel state handlers -- SEND_HDLC_BUFFER_STATE (2),
 * SEND_HDLC_BETWEEN_BUFFER_STATE (3) and CHDLCTX_OFF_STATE (17). All three
 * drive `ctx->vmi_c` (SEND_HDLC_BUFFER_STATE) or `ctx->vmi_a` (the other
 * two) through `FAXVMI_process` with `ctx` itself cast to `unsigned short *`
 * as the "data" argument and a zero (or, for cHDLCtx_off, *rx_count-seeded)
 * "count" -- so the call's own unpack step never touches memory, and the
 * call exists purely for its FAXVMI_process/FAXVMI_status side effects. See
 * class1tx.c for each function's own derivation and cited evidence.
 */

/**
 * @brief SEND_HDLC_BUFFER_STATE (2). `.text` 0x0009e470, 282 bytes.
 *
 * Drives `ctx->vmi_c` purely for its side effects, then checks
 * `FAXVMI_status(ctx->vmi_c, ...)`'s underrun flag: set, it logs, moves to
 * #CLASS1_SEND_HDLC_BETWEEN_BUFFER_STATE, resets `countdown`, and opens a
 * fresh HDLC input frame (_handle_hdlc_input_open()). `*word8 = 0x200`
 * always; `*tx_count` is never written on any path.
 */
int _send_hdlc_buffer_state(struct fax_class1 *ctx, const short *rx,
			    short *tx, int word3, int word4, int *rx_count,
			    int *tx_count, int word7, int *word8);

/**
 * @brief SEND_HDLC_BETWEEN_BUFFER_STATE (3). `.text` 0x0009e810, 433 bytes.
 *
 * Sets `*tx_count = CLASS1_BLOCK_SAMPLES`. On the one call where
 * `countdown == 2` (a one-shot latch after the state's own re-init zeroes
 * it), either goes idle (#CLASS1_IDLE_STATE, #FAX_CLASS1_OK_NO_CARRIER) if
 * `frame_end_latch` is set, or marks the session #FAX_CLASS1_CONNECT.
 * Either way, once armed (`*word8 > 0` and `countdown > 1`) unstuffs host
 * data into the open HDLC frame via _handle_hdlc_input(), moving to
 * #CLASS1_SEND_HDLC_BUFFER_STATE on completion. Drives `ctx->vmi_c` every
 * call; a 5-second overall timeout (`countdown > 250`) aborts to idle with
 * #FAX_CLASS1_ERROR_ON_HOOK.
 */
int _send_hdlc_between_buffer_state(struct fax_class1 *ctx, const short *rx,
				    short *tx, int word3, int word4,
				    int *rx_count, int *tx_count, int word7,
				    int *word8);

/**
 * @brief CHDLCTX_OFF_STATE (17). `.text` 0x0009ea70, 238 bytes.
 *
 * Increments `countdown`, then drives `ctx->vmi_a`. The raw status's bit
 * 0x2000 (#FAXVMI_RESULT_BIT_2000) selects which of two countdown
 * thresholds applies -- 15 when set, 2 when clear; crossing it moves to
 * idle with #FAX_CLASS1_OK_NO_CARRIER. Always emits a block of silence.
 */
int cHDLCtx_off(struct fax_class1 *ctx, const short *rx, short *tx,
		int word3, int word4, int *rx_count, int *tx_count,
		int word7, int *word8);

/*
 * ------------------------------------------------------------------
 * The V.21 HDLC receive machine -- HDLC_RECEIVE_LOOK_CARRIER_STATE (4),
 * HDLC_RECEIVE_STATE (5) and HDLC_RECEIVE_BETWEEN_BUFFERS_STATE (6). Drive
 * `ctx->vmi_a` (or, for the look-carrier state, also `ctx->vmi_b` when
 * `vmi_a`'s own carrier bit is clear). `_hdlc_receive_state` and
 * `_hdlc_receive_between_buffers_state` both unpack a length-prefixed HDLC
 * frame into `ctx` itself (the same scratch-buffer idiom `_hdlc_emulate_
 * receive_state` uses on `ctx->superframe`, here applied to `ctx`'s own leading
 * bytes) and either hand it straight to the host (`_hdlc_receive_state`) or
 * bank it into `ctx->superframe` for later replay (`_hdlc_receive_between_
 * buffers_state` -- the writer side `_hdlc_emulate_receive_state` reads).
 * See class1tx.c for the full derivation of each, including the S7
 * (carrier-wait) timeout math shared with `_rx_look_carrier_state`, and the
 * tone-cadence machine (`tone_cadence_phase`/`tone_cadence_timer`/
 * `cng_enabled`, class1.h) unique to the look-carrier state.
 */

/**
 * @brief HDLC_RECEIVE_LOOK_CARRIER_STATE (4). `.text` 0x0009de00, 943 bytes
 *        -- the largest of the twelve state handlers this batch wrote.
 *
 * Drives `ctx->vmi_a`; once its carrier bit sets, declares
 * #FAX_CLASS1_CONNECT and moves to #CLASS1_HDLC_RECEIVE_STATE, scanning the
 * active modem's signal level against `HDLC_LOOK_CARRIER_LEVELS` to set a
 * requested `gain_attenuation_db`. Losing carrier past the S7 timeout
 * (unless `cng_enabled` masks it) moves to idle with #FAX_CLASS1_NO_CARRIER.
 * Otherwise runs the CNG tone-cadence machine every call: plain silence
 * when `cng_enabled` is clear, else alternating blocks of silence and the
 * session's own tone (`ctx->tone`) on a timer. A pending host-side abort
 * (`*word8 != 0`) aborts to idle.
 */
int _hdlc_receive_look_carrier_state(struct fax_class1 *ctx, const short *rx,
				     short *tx, int word3, int word4,
				     int *rx_count, int *tx_count, int word7,
				     int *word8);

/**
 * @brief HDLC_RECEIVE_STATE (5). `.text` 0x0009d8d0, 738 bytes.
 *
 * Drives `ctx->vmi_a` with `ctx` as the receive buffer. Losing carrier
 * moves to idle with a delayed #FAX_CLASS1_NO_CARRIER and closes the host
 * HDLC stream. A completed frame with a zero length prefix
 * (`ctx->scratch_frame_len`) is a framing/CRC error (delayed
 * #FAX_CLASS1_ERROR); a nonzero one is forwarded to the host via
 * cTOOLS_handle_hdlc_output() (delayed #FAX_CLASS1_OK) and refreshes
 * `ctx->rx_agc_mult`/`rx_agc_shift` from the active modem. Either way moves
 * to #CLASS1_HDLC_RECEIVE_BETWEEN_BUFFERS_STATE. A pending host-side abort
 * aborts to idle. Emits a block of silence.
 */
int _hdlc_receive_state(struct fax_class1 *ctx, const short *rx, short *tx,
			int word3, int word4, int *rx_count, int *tx_count,
			int word7, int *word8);

/**
 * @brief HDLC_RECEIVE_BETWEEN_BUFFERS_STATE (6). `.text` 0x0009dbc0, 566 bytes.
 *
 * The writer side of `ctx->superframe`'s length-prefixed record buffer
 * that _hdlc_emulate_receive_state() later replays: banks each frame
 * completed by `ctx->vmi_a` into `superframe`, dropping it with a "SuperFrame
 * full" log line if the buffer (0xff bytes) has no room. Losing carrier
 * moves to idle with #FAX_CLASS1_NO_CARRIER_NO_MESSAGE. A pending host-side
 * abort aborts to idle. Emits a block of silence.
 */
int _hdlc_receive_between_buffers_state(struct fax_class1 *ctx,
					const short *rx, short *tx, int word3,
					int word4, int *rx_count,
					int *tx_count, int word7,
					int *word8);

/**
 * @brief T30_PREAMBLE_STATE (1). `.text` 0x0009e660, 421 bytes.
 *
 * Feeds `_handle_hdlc_input`-decoded elements banked at `ctx` itself into
 * `FAXVMI_process(ctx->vmi_c, ...)` once a frame has completed and a
 * sample-count countdown (not the `s7_timeout` conversion the RX-side
 * states use) clears 8000, moving to #CLASS1_SEND_HDLC_BUFFER_STATE. A
 * 5-second overall timeout moves to idle with #FAX_CLASS1_ERROR_NO_CARRIER.
 */
int _t30_preabmle_state(struct fax_class1 *ctx, const short *rx, short *tx,
			int word3, int word4, int *rx_count, int *tx_count,
			int word7, int *word8);

/*
 * ------------------------------------------------------------------
 * The data-mode state handlers -- RX_LOOK_CARRIER (12), RX_DATA_STATE (13),
 * TX_NULLS_STATE (11), TX_SCRAMBLED_ONES_STATE (9) and TX_DATA_STATE (10).
 * All five drive `ctx->vmi_b`, the CURRENT data modem's handle (shared with
 * `modem_vmi`/`modem_direction` per class1.h, half-duplex), except `_rx_look_carrier_
 * state`'s own second poll of `ctx->vmi_a` when `vmi_b`'s carrier bit is
 * clear. `_rx_look_carrier_state` and `_rx_data_state` share the low-24-bit
 * FAXVMI_process status convention `cHDLCtx_off` already established
 * (`FAXVMI_RESULT_BIT_2000`, class1tx.c); the TX trio drive `ctx->tx_fifo`
 * (the tx FIFO) and `ctx->tx_bytes_per_block` directly around the FAXVMI_process call.
 * See class1tx.c for each function's own derivation.
 */

/**
 * @brief RX_LOOK_CARRIER (12). `.text` 0x0009cb30, 624 bytes.
 *
 * Drives `ctx->vmi_b` every call, and `ctx->vmi_a` too when `vmi_b`'s
 * carrier bit is clear -- an HDLC frame arriving on `vmi_a` here moves to
 * #CLASS1_HDLC_RECEIVE_STATE with #FAX_CLASS1_OTHER_CARRIER. Detecting a
 * valid V.21 start-of-protocol on `vmi_b`'s status moves to
 * #CLASS1_RX_DATA_STATE with #FAX_CLASS1_CONNECT. An S7 carrier-wait
 * timeout with no carrier detected moves to idle with
 * #FAX_CLASS1_NO_CARRIER. A pending host-side abort (`*word8 != 0`) aborts
 * to idle instead. Emits silence for the whole received block
 * (`*rx_count` samples, not a fixed block).
 */
int _rx_look_carrier_state(struct fax_class1 *ctx, const short *rx,
			   short *tx, int word3, int word4, int *rx_count,
			   int *tx_count, int word7, int *word8);

/**
 * @brief RX_DATA_STATE (13). `.text` 0x0009cda0, 433 bytes.
 *
 * Drives `ctx->vmi_b` with `ctx` as the FAXVMI data buffer. When its
 * carrier bit is set, forwards the demodulated bytes to the host via
 * _handle_data_output(). When clear, closes the host stream (DLE ETX),
 * moves to idle, and arms a delayed #FAX_CLASS1_NO_CARRIER two calls out;
 * on that same branch, a pending host-side abort is handled the same way
 * as _rx_look_carrier_state(). Emits silence for the whole received block.
 */
int _rx_data_state(struct fax_class1 *ctx, const short *rx, short *tx,
		   int word3, int word4, int *rx_count, int *tx_count,
		   int word7, int *word8);

/**
 * @brief TX_NULLS_STATE (11). `.text` 0x0009d040, 435 bytes.
 *
 * A 5-second overall timer (`countdown > 250`) that moves to idle with
 * #FAX_CLASS1_ERROR_ON_HOOK if it fires, but keeps processing this call
 * regardless. When `*word8 > 0` (host data available), moves to
 * #CLASS1_TX_DATA_STATE, unstuffs the data via _handle_data_input(), feeds
 * it into `ctx->tx_fifo`, then refills the transmit block from the FIFO.
 * Drives `ctx->vmi_b` either way, and reports remaining FIFO room
 * (`tx_fifo->size - tx_fifo->count - 1`) in `*word8` at the end.
 */
int _tx_nulls_state(struct fax_class1 *ctx, const short *rx, short *tx,
		    int word3, int word4, int *rx_count, int *tx_count,
		    int word7, int *word8);

/**
 * @brief TX_SCRAMBLED_ONES_STATE (9). `.text` 0x0009d200, 681 bytes.
 *
 * Fires a one-time #FAX_CLASS1_CONNECT on the session's first call (shared
 * `DATAtx_counter` static with _tx_data_state()). `tx_connect_countdown` is
 * a one-shot countdown that, reaching 0, sets `transmit_enabled`. When host
 * data is available (`*word8 > 0`), unstuffs and queues it into
 * `ctx->tx_fifo` and recomputes `tx_fifo_ready`. Fills the transmit block
 * with literal 0xFF ("scrambled ones") unconditionally, then, once both
 * `transmit_enabled` and `tx_fifo_ready` are set, moves to
 * #CLASS1_TX_DATA_STATE and overwrites that fill with a real FIFO read.
 * Drives `ctx->vmi_b`, latching `tx_connect_countdown`'s own arming off a
 * status bit distinct from #FAXVMI_RESULT_BIT_2000
 * (#FAXVMI_PROCESS_BIT_0100). Reports remaining FIFO room in `*word8` at
 * the end.
 */
int _tx_scrambled_ones_state(struct fax_class1 *ctx, const short *rx,
			     short *tx, int word3, int word4, int *rx_count,
			     int *tx_count, int word7, int *word8);

/**
 * @brief TX_DATA_STATE (10). `.text` 0x0009d4b0, 618 bytes.
 *
 * When host data is available, unstuffs it via _handle_data_input() and
 * queues it into `ctx->tx_fifo`; either way, refills the transmit block
 * from the FIFO and drives `ctx->vmi_b`. An underrun stops transmission
 * (idle, #FAX_CLASS1_OK_NO_CARRIER) if `ctx->last_in_byte` is set, else
 * falls back to #CLASS1_TX_NULLS_STATE with #FAX_CLASS1_CONNECT; a full
 * FIFO is only logged. Reports remaining FIFO room in `*word8` at the end.
 */
int _tx_data_state(struct fax_class1 *ctx, const short *rx, short *tx,
		   int word3, int word4, int *rx_count, int *tx_count,
		   int word7, int *word8);

#endif /* DSPLIB_CLASS1TX_H */

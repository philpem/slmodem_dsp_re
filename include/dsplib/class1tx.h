/*
 * class1tx.h -- Class 1 fax: the per-modulation message reporters and the
 * two state-machine leaves this batch reaches.
 *
 * THE FAX PHASE IS DELIBERATELY LAST (CLAUDE.md); everything declared here
 * is finding F8320's no-entry-point bucket, written on its own merit.  The
 * blob's `class1tx.c +94` span cannot be split into author files by
 * tools/tumap.py, so the file split on our side follows the address
 * clusters: the message functions and their tables sit together at
 * 0x09bf20..0x09cad0, between the modem wrappers and the HDLC machine.
 *
 * THE MESSAGE CONTRACT.  Every `vNNxx_message(handle, code, out)` ignores
 * its handle and answers through `out`: the code's string from that
 * modulation's table, or NULL above the table's guard.  The guard is the
 * ENTRY COUNT, not count-minus-one, in all eight functions -- code == N
 * reads one entry past an N-entry table -- and that is the object's own
 * off-by-one, reproduced (docs/deviations.md D951).  The tables are
 * exported, writable `.data` in the object and are the same here.
 */

#ifndef DSPLIB_CLASS1TX_H
#define DSPLIB_CLASS1TX_H

struct fax_class1;
struct faxvmi_cfg;
struct v17tx_cfg;
struct v27tx_cfg;
struct v29tx_cfg;

/* The tables: the author's strings, verbatim (typos included). */
extern char *V17RX_MESG[10];
extern char *V17TX_MESG[10];
extern char *V21RX_MESG[7];
extern char *V21TX_MESG[6];
extern char *V27RX_MESG[8];
extern char *V27TX_MESG[8];
extern char *V29RX_MESG[8];
extern char *V29TX_MESG[8];

void v17rx_message(void *handle, int code, char **out);
void v17tx_message(void *handle, int code, char **out);
void v21rx_message(void *handle, int code, char **out);
void v21tx_message(void *handle, int code, char **out);
void v27rx_message(void *handle, int code, char **out);
void v27tx_message(void *handle, int code, char **out);
void v29rx_message(void *handle, int code, char **out);
void v29tx_message(void *handle, int code, char **out);

/* The null modem's: no table, *out = NULL for every code. */
void null_message(void *handle, int code, char **out);

/*
 * Entry `i` is `i` with its eight bits reversed.  `.rodata` 0xba40, global,
 * and defined in `class1tx.c` -- see the comment there for why that file and
 * not `t30frame.c`, whose `GetT30FrameIDFromBuffer` is the one reference out
 * of five that is not in this span.
 */
extern const unsigned char aReversedCharsArray[256];

/* Clear the session countdown; the tx-nulls state runs until told. */
int _init_tx_nulls_state(struct fax_class1 *ctx);

/*
 * Set the HDLC frame write cursor to ONE, leaving element zero of the
 * caller's buffer free for the length the close reports.  Returns 0.
 */
int _handle_hdlc_input_open(struct fax_class1 *ctx);

/*
 * Note the count of what was received into +0x000 (f1250 - 1) and, when
 * flags004 bit 4 is set, latch f1224.  Returns 0.
 */
int _handle_hdlc_input_close(struct fax_class1 *ctx);

/* Clear the session countdown, after one log line.  Returns 0. */
int _hdlc_receive_state_init(struct fax_class1 *ctx);

/*
 * Clear the session countdown and open an HDLC frame -- a tail call to
 * `_handle_hdlc_input_open`, so it returns that function's 0.
 */
int _send_hdlc_between_buffer_state_init(struct fax_class1 *ctx);

/*
 * ------------------------------------------------------------------
 * The four remaining `class1tx.c +94` leaves, unblocked once `FAXVMI_control`
 * landed (F10115).  Each merges a per-modulation V.21 `.data` REINIT request
 * template with the all-zero `FAXVMI_CTL` and calls `FAXVMI_control` -- the
 * exact shape `class1rx.c`'s `_init_receiver` already established for the
 * three data modulations, here applied to the fixed-rate V.21 control
 * channel.  See class1tx.c for the full per-function derivation and for
 * `V21RX_CTL`/`V21TX_CTL`, defined there.
 *
 * `cHDLCtx_off_init` (0x9e9d0, 149 bytes) is NOT one of these four: it is
 * reached from NOWHERE in the object -- `objdump -r` over the whole 1.2 MB
 * shows zero relocations of either kind (F8493's call/data-store pair)
 * naming it -- so it sits in finding F8320's no-entry-point bucket rather
 * than in this fax-reachable closure, matching `service.py`'s own
 * reachability count over `worklist.py`'s span listing.  It was scheduled on
 * its own merit, separately, and IS now written (class1tx.c): the quiescent
 * half of `_cHDLCrx_init_from_idle` below, once that sibling's own
 * `FAXVMI_control` dependency (F10115) and the seven other `v??tx_control`/
 * `v??rx_control` functions behind it existed.
 */

/*
 * `_rx_look_carrier_init`, 0x9cb00, 45 bytes.  Reinit the data-mode receiver
 * through `_init_receiver` (class1rx.c), reset the async octet-recovery
 * search (`cTOOLS_handle_data_output_reset`), and clear `countdown`.  Returns
 * 0 (`xor %eax,%eax` is both the store's value and, unmodified, the return).
 */
int _rx_look_carrier_init(struct fax_class1 *ctx, int rate_code);

/*
 * `_tx_scrambled_ones_init`, 0x9cf70, 193 bytes.  Reinit the data-mode
 * transmitter through `_init_transmitter` (class1tx.c), (re)build the
 * transmit FIFO (`ctx->f1288`) at a fixed 0x800-element capacity, and derive
 * `ctx->f1290` as `ctx->tx_rate / 400` (signed division; the object's own
 * `imul $0x51eb851f` / `sar $7` / sign-correct reciprocal, confirmed against
 * every rate class1.c's `_sym_size` recognises).  Clears `f1270`, `f1294`,
 * `transmit_enabled`, `f1298`, `data_input_closed` and the file-static
 * `DATAtx_counter` (`_tx_scrambled_ones_state`'s own counter, class1tx.c).
 * Returns 0.
 */
int _tx_scrambled_ones_init(struct fax_class1 *ctx, int rate_code);

/*
 * `cHDLCtx_preamble_state_init`, 0x9e380, 193 bytes.  Merge `V21TX_CTL` (the
 * REINIT bit OR'd into `flags_0d`) into `FAXVMI_CTL` and send it through
 * `FAXVMI_control(ctx->vmi_c, ...)` -- no ring-clear, no framer reset, no
 * mode change; only `int_0014` (the recursion into `v21tx_control`) is
 * nonzero.  Opens an HDLC frame (`_handle_hdlc_input_open`, return discarded)
 * and resets `countdown`, `state` (to
 * `CLASS1_T30_SILENCE_BEFORE_PREAMBLE_STATE`), `hdlc_frame_done`,
 * `buffers_sent` and `f1224`.  Returns 0.
 */
int cHDLCtx_preamble_state_init(struct fax_class1 *ctx);

/*
 * `_cHDLCrx_init_from_idle`, 0x9d790, 226 bytes.  TWO ARGUMENTS -- both
 * callers (`fax_class1_create`, `fax_class1_command`) pass their own second
 * one straight through, and the second is read: `arg2 == 3` both sets
 * `ctx->state` to `CLASS1_HDLC_RECEIVE_LOOK_CARRIER_STATE` (4) AND becomes
 * the function's own return value, overriding whatever `FAXVMI_control`
 * returned (a real property, not a guess: the object sets `%eax = 4` on that
 * path and never touches it again before either `ret`).  Merges `V21RX_CTL`
 * (REINIT bit OR'd in) into a `FAXVMI_ctl` that ALSO forces a full framer
 * reset (`int_000c = 1`, `short_0010 = 2`) and empties the ring (`ptr_0000 =
 * (void *)1`) -- unlike the TX-side sibling above, which forces none of
 * that -- and sends it through `FAXVMI_control(ctx->vmi_a, ...)`.  Clears
 * `countdown` and `delayed_status_countdown` unconditionally.
 */
int _cHDLCrx_init_from_idle(struct fax_class1 *ctx, int arg2);

/*
 * `cHDLCtx_off_init`, 0x9e9d0, 149 bytes.  No entry point in the object
 * reaches it (see the note above); it is the quiescent HALF of
 * `_cHDLCrx_init_from_idle` above -- the same `V21RX_CTL`-sourced,
 * `V21RXCTL_REINIT`-forced request merged into the same full-framer-reset
 * `FAXVMI_ctl` and sent to the same `ctx->vmi_a`, but with no `arg2`, no
 * state transition, no `delayed_status_countdown` clear and no debug line.
 * Returns whatever `FAXVMI_control` returned; also clears `countdown`.
 */
int cHDLCtx_off_init(struct fax_class1 *ctx);

/*
 * THE HOST LINK IS DLE-STUFFED BYTES ONE WAY AND 16-BIT ELEMENTS THE OTHER.
 * `_handle_data_input` and `_handle_hdlc_input` take `unsigned char *` at a
 * stride of one and write `unsigned short *` at a stride of two;
 * `_handle_data_output` goes the other way.  Every load is a `movzbl`, so
 * only the low eight bits of an element ever carry data.
 *
 * `count` IS THE DESTINATION'S SIZE, NOT A REQUEST, in all three -- see the
 * per-function notes in class1tx.c for what each one can write past its input
 * length.  Sizing a destination from the input alone is F8607/D956.
 */

/*
 * Unstuff a transmit block.  `*count` in is the byte count, out is the
 * element count -- and is ZERO for ever once DLE ETX has been seen.  Writes
 * at most `*count + 20` elements: the padding after DLE ETX is twenty long.
 * Returns 0.
 */
int _handle_data_input(struct fax_class1 *ctx, const unsigned char *src,
		       unsigned short *dst, int *count);

/*
 * The same for an HDLC frame, with the write cursor kept in the SESSION
 * (`f1250`) so a frame accumulates across calls -- `dst` is the whole frame's
 * buffer, not one block's.  Returns 1 on the call that completes a frame and
 * 0 otherwise; `*count` comes back as the frame length or as zero.
 */
int _handle_hdlc_input(struct fax_class1 *ctx, const unsigned char *src,
		       unsigned short *dst, int *count);

/*
 * Recover an octet from each of `count` elements, DLE-stuff them into `dst`,
 * and append DLE ETX when `terminate` is set.  Returns the byte count, which
 * can reach `2 * count + 2`.
 */
/*
 * Unlock `_handle_data_output`'s start-bit search: `async_locked` to 0 and
 * `async_window` to -1.  The shift and mask are left where they are.
 */
void cTOOLS_handle_data_output_reset(struct fax_class1 *ctx);

int _handle_data_output(struct fax_class1 *ctx, const unsigned short *src,
			unsigned char *dst, int count, int terminate);

/*
 * The receive-side mirror of `_handle_data_output`, without its async
 * start-bit search: `src`'s elements are already-aligned HDLC receive
 * bytes, so only DLE-stuffing and the DLE ETX terminator are needed.
 * Returns the byte count, which can reach `2 * count + 2`.  See
 * class1tx.c for why "hdlc_output" means output to the HOST of a frame
 * the modem RECEIVED.
 */
int cTOOLS_handle_hdlc_output(struct fax_class1 *ctx, const unsigned short *src,
			      unsigned char *dst, int count, int terminate);

/*
 * ------------------------------------------------------------------
 * The transmit-side VMI constructors.  By ADDRESS, not by span name: they
 * sit at 0x094870..0x094b6f, immediately after `class1rx.c`'s own
 * `_init_receiver` (0x094240) and immediately before `_delete_data_tx_modem`
 * and `_init_transmitter` -- so this run of `.text` is the TX twin of
 * `class1rx.c`'s RX trio, one span later, and `tools/readyqueue.py` already
 * files all three under `class1tx.c +94` on that same address evidence. See
 * class1tx.c for the derivation.
 *
 * ALL THREE ARE `t` IN THE OBJECT -- file-local, only called from
 * `_init_transmitter`, still 336+ unwritten symbols away -- AND ARE GLOBAL
 * HERE, for the identical reason `class1rx.h`'s RX trio already gives
 * (D1081): a `static` spelling would be three functions this tree could
 * neither reach nor test. D1450 records it for this trio; the pass that
 * writes `_init_transmitter` should take all three back to `static`.
 *
 * SAME SHAPE AS THE RX TRIO, with three differences.  `init_vmi_v29tx`'s
 * config is `struct v29tx_cfg`, whose `int_0018` the table's own default (0)
 * fills unread; `init_vmi_v17tx` OVERRIDES its `int_0018` with a hardcoded 0
 * rather than reading the table's copy there at all -- so the two constants
 * end up equal but by different means, and `init_vmi_v17tx` is the one
 * spelled as an explicit store.  ALL THREE also hardcode `cfg->int_0014`
 * after the table copy -- 1 for V.17 (equal to its table default, so
 * invisible to a value-only check), 2 for V.27ter and V.29 (whose tables
 * both default to 1, confirmed against the blob's own `.data` with
 * `tabdump.py` independently of either reconstructed table) -- caught by
 * `t_class1txvmi.c` disagreeing with the blob rather than assumed absent
 * on a first pass over the disassembly. All three RETURN `(int)cfg->bitrate`
 * -- the object reloads it from the freshly-built config right before
 * `ret`, which nothing declared `void` would do -- so unlike the RX trio
 * these are `int`, not `void`.
 */
int init_vmi_v17tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
		   int arg_2, void *arg_3);
int init_vmi_v27tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
		   int arg_2, void *arg_3);
int init_vmi_v29tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
		   int arg_2, void *arg_3);

/* The `slot` each of the three plants, from `faxvmi.h`'s slot map. */
#define VMI_SLOT_V21TX		5
#define VMI_SLOT_V27TX		7
#define VMI_SLOT_V29TX		9
#define VMI_SLOT_V17TX		11

/*
 * `VMI_SLOT_V21RX` (6) is not in the three-constructor group above -- V.21
 * receive has no `init_vmi_v21rx` counterpart written here, since
 * `fax_class1_create` builds its one V.21 RX `faxvmi_cfg` inline rather than
 * through a shared constructor (see class1.c).  Declared beside its TX twin
 * because `faxvmi.h`'s own slot table (0..4 null, 5 v21tx, 6 v21rx, 7 v27tx,
 * 8 v27rx, 9 v29tx, 10 v29rx, 11 v17tx, 12 v17rx) already establishes it, and
 * class1rx.h's RX trio does not cover V.21 at all.
 */
#define VMI_SLOT_V21RX		6

/*
 * Tear the transmit-side data modem down: the config, the VMI block, the
 * FAXVMI handle -- and, when `ctx->f1288` (a FIFO) is non-null, that too.
 * Unlike the RX twin, no modulation ever needs a sub-allocation freed first.
 */
void _delete_data_tx_modem(struct fax_class1 *ctx);

/*
 * `_init_transmitter`, 0x094bf0, 1,326 bytes -- see `class1tx.c` for the
 * full derivation.  `rate_code` is the same T.30 modem-rate code space
 * `_init_receiver` (class1rx.c) reads.
 */
void _init_transmitter(struct fax_class1 *ctx, int rate_code);

/*
 * ------------------------------------------------------------------
 * Two more of the nineteen state handlers, unblocked once `_put_silence`
 * landed. `.text` 0x09e590 (194 bytes) and 0x09d720 (111 bytes).
 *
 * TX_SILENCE_BEFORE_SCRM_ONES: add 20 to `countdown`, transmit a block of
 * silence, and move to TX_SCRAMBLED_ONES_STATE once `countdown` (unsigned)
 * reaches `silence_blocks`. `*word8` is always cleared to 0.
 */
int _tx_silence_before_scrm_ones(struct fax_class1 *ctx, const short *rx,
				 short *tx, int word3, int word4,
				 int *rx_count, int *tx_count, int word7,
				 int *word8);

/*
 * T30_SILENCE_BEFORE_TX_STATE: transmit a block of silence (its return
 * value unused -- see class1tx.c), and once `countdown` (unsigned) exceeds
 * 400, log a "50MS second, send preamble" line at debug level > 1, move to
 * T30_PREAMBLE_STATE, set FAX_CLASS1_CONNECT and reset `countdown` to 0.
 * Either way, `countdown += CLASS1_BLOCK_SAMPLES` unconditionally at the
 * end -- a plain accumulation, not the assignment an earlier reading of
 * this function mistook it for (class1tx.c's own note).
 */
int _t30_silence_before_tx_state(struct fax_class1 *ctx, const short *rx,
				 short *tx, int word3, int word4,
				 int *rx_count, int *tx_count, int word7,
				 int *word8);

/*
 * ------------------------------------------------------------------
 * HDLC_EMULATE_RECEIVE_STATE.  `.text` 0x09e1b0, 452 bytes -- flagged READY
 * by wave 8's agent once `_put_silence` and `cTOOLS_handle_hdlc_output`
 * landed, and left for time; both are in now, and this batch closes it.
 *
 * Replays whatever length-prefixed records sit in `ctx->f1000` (see
 * class1.h) OUT to the host, one per call, gated by a two-tick countdown --
 * so it looks like the modem RECEIVED an HDLC frame without a real
 * demodulator running, which is the function's own name.  `word3` is the
 * host-facing output buffer (cast from the shared `int` slot, the same
 * `(T *)(long)` idiom the VMI constructors above already use) and `word7`
 * is an `int *` the byte count is written through -- both established HERE,
 * by this function, for the first time in this batch; no other
 * `class1_state_fn` this tree has written reads either one.
 *
 * `rx`, `word4`, `rx_count` and `word8` are read nowhere in the object.
 */
int _hdlc_emulate_receive_state(struct fax_class1 *ctx, const short *rx,
				short *tx, int word3, int word4,
				int *rx_count, int *tx_count, int word7,
				int *word8);

/*
 * ------------------------------------------------------------------
 * THE V.21 HDLC CONTROL-CHANNEL STATE HANDLERS -- SEND_HDLC_BUFFER_STATE (2),
 * SEND_HDLC_BETWEEN_BUFFER_STATE (3) and CHDLCTX_OFF_STATE (17).  All three
 * drive `ctx->vmi_c` (SEND_HDLC_BUFFER_STATE) or `ctx->vmi_a` (the other
 * two) through `FAXVMI_process` with `ctx` itself cast to `unsigned short *`
 * as the "data" argument and a zero (or, for cHDLCtx_off, *rx_count-seeded)
 * "count" -- so the call's own unpack step never touches memory, and the
 * call exists purely for its FAXVMI_process/FAXVMI_status side effects.  See
 * class1tx.c for each function's own derivation and cited evidence.
 */
int _send_hdlc_buffer_state(struct fax_class1 *ctx, const short *rx,
			    short *tx, int word3, int word4, int *rx_count,
			    int *tx_count, int word7, int *word8);
int _send_hdlc_between_buffer_state(struct fax_class1 *ctx, const short *rx,
				    short *tx, int word3, int word4,
				    int *rx_count, int *tx_count, int word7,
				    int *word8);
int cHDLCtx_off(struct fax_class1 *ctx, const short *rx, short *tx,
		int word3, int word4, int *rx_count, int *tx_count,
		int word7, int *word8);

/*
 * ------------------------------------------------------------------
 * THE V.21 HDLC RECEIVE MACHINE -- HDLC_RECEIVE_LOOK_CARRIER_STATE (4),
 * HDLC_RECEIVE_STATE (5) and HDLC_RECEIVE_BETWEEN_BUFFERS_STATE (6).  Drive
 * `ctx->vmi_a` (or, for the look-carrier state, also `ctx->vmi_b` when
 * `vmi_a`'s own carrier bit is clear).  `_hdlc_receive_state` and
 * `_hdlc_receive_between_buffers_state` both unpack a length-prefixed HDLC
 * frame into `ctx` itself (the same scratch-buffer idiom `_hdlc_emulate_
 * receive_state` uses on `ctx->f1000`, here applied to `ctx`'s own leading
 * bytes) and either hand it straight to the host (`_hdlc_receive_state`) or
 * bank it into `ctx->f1000` for later replay (`_hdlc_receive_between_
 * buffers_state` -- the WRITER `_hdlc_emulate_receive_state`'s own reader
 * side lacked until now).  See class1tx.c for the full derivation of each,
 * including the S7 (carrier-wait) timeout math shared by the look-carrier
 * state and `_rx_look_carrier_state`, and the tone-cadence machine
 * (`f125c`/`f1260`/`cng_enabled`, class1.h) unique to the look-carrier state.
 */
int _hdlc_receive_look_carrier_state(struct fax_class1 *ctx, const short *rx,
				     short *tx, int word3, int word4,
				     int *rx_count, int *tx_count, int word7,
				     int *word8);
int _hdlc_receive_state(struct fax_class1 *ctx, const short *rx, short *tx,
			int word3, int word4, int *rx_count, int *tx_count,
			int word7, int *word8);
int _hdlc_receive_between_buffers_state(struct fax_class1 *ctx,
					const short *rx, short *tx, int word3,
					int word4, int *rx_count,
					int *tx_count, int word7,
					int *word8);

/*
 * T30_PREAMBLE_STATE (1).  Feeds `_handle_hdlc_input`-decoded elements
 * banked at `ctx` itself into `FAXVMI_process(ctx->vmi_c, ...)` once a
 * frame has completed and the S7-style countdown (in raw sample units this
 * time, not the `f12b4`/`s7_timeout` conversion the RX-side states use)
 * clears 8000.  See class1tx.c.
 */
int _t30_preabmle_state(struct fax_class1 *ctx, const short *rx, short *tx,
			int word3, int word4, int *rx_count, int *tx_count,
			int word7, int *word8);

/*
 * ------------------------------------------------------------------
 * THE DATA-MODE STATE HANDLERS -- RX_LOOK_CARRIER (12), RX_DATA_STATE (13),
 * TX_NULLS_STATE (11), TX_SCRAMBLED_ONES_STATE (9) and TX_DATA_STATE (10).
 * All five drive `ctx->vmi_b`, the CURRENT data modem's handle (shared with
 * `modem_vmi`/`f1244` per class1.h, half-duplex), except `_rx_look_carrier_
 * state`'s own second poll of `ctx->vmi_a` when `vmi_b`'s carrier bit is
 * clear.  `_rx_look_carrier_state` and `_rx_data_state` share the low-24-bit
 * FAXVMI_process status convention `cHDLCtx_off` already established
 * (`FAXVMI_RESULT_BIT_2000`, class1tx.c); the TX trio drive `ctx->f1288`
 * (the tx FIFO) and `ctx->f1290` directly around the FAXVMI_process call.
 * See class1tx.c for each function's own derivation.
 */
int _rx_look_carrier_state(struct fax_class1 *ctx, const short *rx,
			   short *tx, int word3, int word4, int *rx_count,
			   int *tx_count, int word7, int *word8);
int _rx_data_state(struct fax_class1 *ctx, const short *rx, short *tx,
		   int word3, int word4, int *rx_count, int *tx_count,
		   int word7, int *word8);
int _tx_nulls_state(struct fax_class1 *ctx, const short *rx, short *tx,
		    int word3, int word4, int *rx_count, int *tx_count,
		    int word7, int *word8);
int _tx_scrambled_ones_state(struct fax_class1 *ctx, const short *rx,
			     short *tx, int word3, int word4, int *rx_count,
			     int *tx_count, int word7, int *word8);
int _tx_data_state(struct fax_class1 *ctx, const short *rx, short *tx,
		   int word3, int word4, int *rx_count, int *tx_count,
		   int word7, int *word8);

#endif /* DSPLIB_CLASS1TX_H */

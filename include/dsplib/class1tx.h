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

#endif /* DSPLIB_CLASS1TX_H */

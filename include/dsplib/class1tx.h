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

/* Clear the session countdown; the tx-nulls state runs until told. */
int _init_tx_nulls_state(struct fax_class1 *ctx);

/*
 * Note the count of what was received into +0x000 (f1250 - 1) and, when
 * flags004 bit 4 is set, latch f1224.  Returns 0.
 */
int _handle_hdlc_input_close(struct fax_class1 *ctx);

#endif /* DSPLIB_CLASS1TX_H */

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
 *   null_message                    .text 0x09f140              11
 *   _handle_hdlc_input_close        .text 0x09ecd0              34
 *
 * and the eight `.data` tables the reporters index.  Everything here is
 * finding F8320's no-entry-point bucket -- this is not the fax phase.
 * `_send_hdlc_between_buffer_state_init` (0x09e450) was in scope and is
 * left out: it tail-calls the unreconstructed `_handle_hdlc_input_open`
 * (F215: no scaffold).  So are `cHDLCtx_off_init` (calls FAXVMI_control)
 * and the V21 next-state pair, which store six unreconstructed handler
 * addresses.  Findings F8492/F8493.
 *
 * THE STRINGS ARE THE AUTHOR'S, byte for byte: "Protocal", "Transmition"
 * and V29TX's "7600 bps" are the object's spellings, and fixing them would
 * change .rodata.  The guard in every reporter allows one code past the end
 * of its table -- see D951 -- and the in-range/above-range behaviour is
 * what the differential test pins (the one-past read lands in whatever the
 * link put next, there as here).
 */

#include <stddef.h>

#include "dsplib/class1.h"
#include "dsplib/class1tx.h"

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

int
_init_tx_nulls_state(struct fax_class1 *ctx)
{
	ctx->countdown = 0;
	return 0;
}

int
_handle_hdlc_input_close(struct fax_class1 *ctx)
{
	ctx->f000 = (short)(ctx->f1250 - 1);
	if (ctx->flags004 & 0x10)
		ctx->f1224 = 1;
	return 0;
}

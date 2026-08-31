/*
 * faxvmi.c -- Class 1 fax: the VMI's message dispatcher and its table.
 *
 * Reconstructed from dsplibs.o's class1tx.c +94 span (Faxvmi cluster,
 * 0x095120..0x0969xx):
 *
 *   FAXVMI_message   .text 0x0957b0    55
 *   vxx_message      .rodata 0x94e0    52  (13 slots)
 *
 * FAXVMI_message is finding F8320's no-entry-point bucket; the table comes
 * with it because the function is the table walk and every one of the
 * thirteen targets is reconstructed (eight modulation reporters and
 * null_message five times over -- the relocation dump is in finding F8491).
 * The rest of the VMI (create/process/status/control) is the fax phase's.
 */

#include <stddef.h>

#include "dsplib/class1tx.h"
#include "dsplib/faxvmi.h"

faxvmi_message_fn const vxx_message[13] = {
	null_message,
	null_message,
	null_message,
	null_message,
	null_message,
	v21tx_message,
	v21rx_message,
	v27tx_message,
	v27rx_message,
	v29tx_message,
	v29rx_message,
	v17tx_message,
	v17rx_message,
};

/*
 * The out-parameter is initialised to NULL BEFORE the dispatch, so a slot
 * whose reporter wrote nothing would still answer NULL -- none of the
 * thirteen leaves it unwritten, but the belt is the object's and is kept.
 * There is NO guard on the slot: a stale index dispatches through whatever
 * follows the table, there as here.
 */
char *
FAXVMI_message(struct faxvmi *vmi, unsigned char code)
{
	char *msg = NULL;

	vxx_message[(unsigned short)vmi->slot](vmi->handle, code, &msg);
	return msg;
}

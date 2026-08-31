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

/*
 * The framing layer's three pure buffer walks.
 *
 *   faxvmi_gen_fcs16     .text 0x096780   108
 *   faxvmi_byte_reverse  .text 0x0967f0    93
 *   faxvmi_frame_reverse .text 0x096850   151
 *
 * All three count DOWN from `count` and stop at zero, so a negative count
 * runs 65536 + count times before the 16-bit counter reaches zero rather
 * than not running at all.  That is the object's and is reproduced; D1052.
 */

/*
 * Two nibble steps per element, high nibble first.  Written as the object
 * computes it: `t` is the polynomial multiplicand already shifted into bits
 * 15..12, so `t >> 11` is the x^5 term, `t >> 12` is the x^0 term, and `t`
 * itself is the x^12 one.  See faxvmi.h for the derivation of 0x1021.
 */
#define FCS16_NIBBLE(fcs, shifted)					\
	do {								\
		unsigned int t_ = ((shifted) ^ (fcs)) & 0xf000u;	\
		(fcs) = (unsigned short)					\
			(((((fcs) ^ (t_ >> 11)) << 4) ^ t_) | (t_ >> 12)); \
	} while (0)

int
faxvmi_gen_fcs16(unsigned short *buf, short count)
{
	unsigned short fcs = 0xffff;
	short i;

	for (i = count; i != 0; i--) {
		unsigned int octet = *buf++;

		FCS16_NIBBLE(fcs, octet << 8);
		FCS16_NIBBLE(fcs, octet << 12);
	}
	return (unsigned short)~fcs;
}

void
faxvmi_byte_reverse(unsigned short *buf, short count)
{
	short i;

	for (i = count; i != 0; i--) {
		unsigned short in = *buf;
		unsigned short out = 0;
		short bit;

		for (bit = 7; bit >= 0; bit--) {
			out = (unsigned short)((out << 1) | (in & 1));
			in >>= 1;
		}
		*buf++ = out;
	}
}

/*
 * The inner walk is faxvmi_byte_reverse's, which the object inlines here
 * while still emitting the standalone copy -- F605's inlining boundary, so a
 * per-function byte comparison will read this function long and that one
 * short.  Written as the call it is.
 */
void
faxvmi_frame_reverse(unsigned short *buf, short count)
{
	short i;

	for (i = count; i != 0; i--) {
		short len = (short)*buf++;

		faxvmi_byte_reverse(buf, len);
		buf += len;
	}
}

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
 *   _hdlc_receive_state_init        .text 0x09d880              79
 *   _handle_data_input              .text 0x09eb60             321
 *   _handle_hdlc_input_close        .text 0x09ecd0              34
 *   _handle_hdlc_input              .text 0x09ed00             238
 *   _handle_data_output             .text 0x09ee10             255
 *   null_message                    .text 0x09f140              11
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
#include "dsplib/debug.h"

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

/*
 * Nothing but a log line and one cleared field.  The message is the author's,
 * byte for byte, and it names the function -- which is how this timestamp
 * pair got its name in class1.h.
 */
int
_hdlc_receive_state_init(struct fax_class1 *ctx)
{
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf(
		    "At %2d.%02d[sec] hdlc_receive_state_init\n",
		    ctx->clock_sec, ctx->clock_frac);
	ctx->countdown = 0;
	return 0;
}

/*
 * A file-static the object increments once per byte examined and NEVER READS
 * -- `temp.0` at .bss+0x8c4, four bytes, with exactly one relocation against
 * it in the whole 1.2 MB (the `incl` at 0x09ebd5).  Kept because it is the
 * author's; it carries nothing.
 */
static int temp;

/*
 * DLE-unstuff the host's transmit buffer into 16-bit elements.
 *
 * `*count` is IN AND OUT: in it is how many bytes `src` holds, out it is how
 * many elements were written -- and once DLE ETX has been seen it answers
 * zero for ever after, without touching `dst`, because `data_input_closed`
 * stays set.
 *
 * TWO THINGS THAT LOOK LIKE MISTAKES AND ARE THE OBJECT'S.  The last byte of
 * the block is latched into `last_in_byte` BEFORE the loop, so it is recorded
 * even when the loop stops early at DLE ETX (D1054).  And after DLE ETX the
 * function appends twenty zero elements, each guarded by a limit on the
 * OUTPUT index rather than on the destination's size, so the destination must
 * hold `*count + 20` elements and the guard cannot help a smaller one
 * (D1055).
 */
int
_handle_data_input(struct fax_class1 *ctx, const unsigned char *src,
		   unsigned short *dst, int *count)
{
	int out = 0;
	int i;

	if (ctx->data_input_closed != 0) {
		*count = 0;
		return 0;
	}
	if (*count > 0)
		ctx->last_in_byte = src[*count - 1];

	for (i = 0; i < *count; i++) {
		temp++;
		if (ctx->dle_seen != 0) {
			if (dsplibs_debug_level > 2)
				dsplibs_debug_printf(
				    "CLASS1: DLE %1X in data\n", src[i]);
			ctx->dle_seen = 0;
			if (src[i] == CLASS1_DLE) {
				dst[out] = CLASS1_DLE;
				out++;
				continue;
			}
			if (src[i] == CLASS1_ETX) {
				int k;

				ctx->data_input_closed = 1;
				for (k = CLASS1_ETX_PAD_ELEMENTS - 1;
				     k >= 0; k--) {
					if (out > CLASS1_ETX_PAD_LIMIT)
						continue;
					dst[out] = 0;
					out++;
				}
				break;
			}
			continue;
		}
		if (src[i] == CLASS1_DLE) {
			ctx->dle_seen = 1;
			continue;
		}
		dst[out] = src[i];
		out++;
	}
	*count = out;
	return 0;
}

int
_handle_hdlc_input_close(struct fax_class1 *ctx)
{
	ctx->f000 = (short)(ctx->f1250 - 1);
	if (ctx->flags004 & CLASS1_FLAG_FRAME_END_LATCH)
		ctx->f1224 = 1;
	return 0;
}

/*
 * The same unstuffing for an HDLC frame, and three things make it a different
 * function rather than a mode of the one above.
 *
 *   - The write cursor is the SESSION's (`f1250`), not a local, so a frame
 *     accumulates across calls and the caller's `dst` is indexed from
 *     wherever the last call left off.  `dst` must therefore be sized for the
 *     whole frame, not for one block.
 *   - DLE ETX ends the FRAME: it does the same two stores
 *     `_handle_hdlc_input_close` does -- the length into `f000`, and `f1224`
 *     when the flag is on -- and reports 1.
 *   - `*count` comes back as the frame length on that call and as ZERO
 *     otherwise, which is what tells the caller a frame is not finished yet.
 *
 * A byte that is neither DLE nor ETX after a DLE is DROPPED, silently and
 * without the log line the data path prints. That is the object's.
 */
int
_handle_hdlc_input(struct fax_class1 *ctx, const unsigned char *src,
		   unsigned short *dst, int *count)
{
	int done = 0;
	int out = ctx->f1250;
	int i;

	for (i = 0; i < *count; i++) {
		if (ctx->dle_seen != 0) {
			ctx->dle_seen = 0;
			if (src[i] == CLASS1_DLE) {
				dst[out] = CLASS1_DLE;
				out++;
				continue;
			}
			if (src[i] == CLASS1_ETX) {
				ctx->f1250 = out;
				ctx->f000 = (short)(out - 1);
				if (ctx->flags004 & CLASS1_FLAG_FRAME_END_LATCH)
					ctx->f1224 = 1;
				*count = out;
				done = 1;
				break;
			}
			continue;
		}
		if (src[i] == CLASS1_DLE) {
			ctx->dle_seen = 1;
			continue;
		}
		dst[out] = src[i];
		out++;
	}

	ctx->f1250 = out;
	if (done)
		return 1;
	*count = 0;
	return 0;
}

/*
 * The other direction: recover an octet from each received element, DLE-stuff
 * it, and hand the host a byte stream.  Returns how many bytes were written.
 *
 * THE OCTET RECOVERY IS AN ASYNCHRONOUS START-BIT SEARCH, and it is worth
 * stating exactly, because the arithmetic looks arbitrary otherwise.  Each
 * element contributes its LOW BYTE to the top of a 32-bit window whose lower
 * three bytes are the previous three (`async_window`).  While the alignment
 * is not locked, the search walks up from bit 16 for the first ZERO bit,
 * moving `async_mask` and `async_shift` with it, and the recovered octet is
 * the eight bits starting AT that zero -- so its least significant bit is the
 * start bit itself, which is the object's arrangement and not a slip in the
 * reading. Eight bits are tried; if all of them are one the search gives up,
 * the octet is 0xff, the alignment is NOT locked, and the mask and shift are
 * left where the search abandoned them for the next element to inherit
 * (D1056).
 *
 * Once locked, the mask and shift are frozen and every later element is
 * extracted through them, so the recovery costs one search per carrier.
 *
 * The stuffing is the standard one: a recovered 0x10 is written twice, and
 * `terminate` appends DLE ETX.  So the destination must hold
 * `2 * count + 2` bytes -- sized from the COUNT, never from `count`
 * (F8607/D956's shape).
 */
int
_handle_data_output(struct fax_class1 *ctx, const unsigned short *src,
		    unsigned char *dst, int count, int terminate)
{
	int out = 0;
	int i;

	for (i = 0; i < count; i++) {
		unsigned char octet = 0xff;
		unsigned int window;

		window = ((unsigned int)(unsigned char)src[i] << 24)
		       | ctx->async_window;

		if (ctx->async_locked == 0) {
			unsigned int probe = 0x10000;
			int tried = 0;

			ctx->async_shift = 16;
			ctx->async_mask = 0xff0000;
			while ((window & probe) != 0) {
				ctx->async_mask <<= 1;
				tried++;
				probe += probe;
				ctx->async_shift++;
				if (tried > 7)
					goto emit;
			}
			ctx->async_locked = 1;
		}
		octet = (unsigned char)((window & ctx->async_mask)
					>> ctx->async_shift);
	emit:
		ctx->async_window = window >> 8;
		dst[out] = octet;
		out++;
		if (octet == CLASS1_DLE) {
			dst[out] = CLASS1_DLE;
			out++;
		}
	}

	if (terminate != 0) {
		dst[out] = CLASS1_DLE;
		dst[out + 1] = CLASS1_ETX;
		out += 2;
	}
	return out;
}

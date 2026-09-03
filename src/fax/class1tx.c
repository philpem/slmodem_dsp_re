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
 *   _send_hdlc_between_buffer_state_init .text 0x09e450         17
 *   _handle_data_input              .text 0x09eb60             321
 *   _handle_hdlc_input_open         .text 0x09ecb0              18
 *   _handle_hdlc_input_close        .text 0x09ecd0              34
 *   _handle_hdlc_input              .text 0x09ed00             238
 *   cTOOLS_handle_data_output_reset .text 0x09edf0              24
 *   _handle_data_output             .text 0x09ee10             255
 *   null_message                    .text 0x09f140              11
 *   aReversedCharsArray             .rodata 0xba40             256
 *
 * and the eight `.data` tables the reporters index.  Everything here is
 * finding F8320's no-entry-point bucket -- this is not the fax phase.
 * `_send_hdlc_between_buffer_state_init` (0x09e450) was left out of the leaf
 * pass because it tail-calls the then-unreconstructed
 * `_handle_hdlc_input_open` (F215: no scaffold).  Writing that callee here
 * unblocked it and both are now in, which is the link constraint working the
 * way round it is supposed to -- F9198.  `cHDLCtx_off_init` (calls
 * FAXVMI_control) and the V21 next-state pair, which store six
 * unreconstructed handler addresses, are still out.  Findings F8492/F8493.
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
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/faxvmi.h"
#include "dsplib/sysdep.h"
#include "dsplib/t30frame.h"
#include "dsplib/v17fax.h"
#include "dsplib/v27fax.h"
#include "dsplib/v29data.h"

/*
 * `aReversedCharsArray` -- .rodata 0xba40, 256 bytes, GLOBAL.  Entry `i` is
 * `i` with its eight bits reversed, and that is CHECKED rather than assumed:
 * the test compares all 256 bytes against the object's copy, and this file
 * carries the bytes rather than a loop because the object carries bytes.
 *
 * WHY IT IS IN THIS FILE, which is an inference and is the weakest thing on
 * this page.  Five relocations name it: one from `GetT30FrameIDFromBuffer`
 * (0x96e9e) and FOUR from `cTOOLS_handle_hdlc_output` (0x9f029, 0x9f051,
 * 0x9f066, 0x9f08a), which sits at 0x9ef10, inside this file's span.  Its
 * `.rodata` address is also far past the FAXVMI/FIFO cluster's (0x9490-0x9660)
 * and in the range this span's other constants occupy.  Neither argument is
 * decisive on its own; together they put it here rather than in
 * `t30frame.c`, whose one reference is the minority.
 */
const unsigned char aReversedCharsArray[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

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
 * Clear the countdown and open a frame.  The object TAIL-CALLS
 * `_handle_hdlc_input_open` (`jmp`, 0x9e45c), so this returns whatever that
 * returns, which is 0.
 */
int
_send_hdlc_between_buffer_state_init(struct fax_class1 *ctx)
{
	ctx->countdown = 0;
	return _handle_hdlc_input_open(ctx);
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

/*
 * Arm the frame cursor at ONE, not zero.  `_handle_hdlc_input` writes the
 * first octet at `dst[1]` and both it and `_handle_hdlc_input_close` report
 * `f1250 - 1` as the length, so element zero is the length slot the frame is
 * eventually length-prefixed with -- the same layout `faxvmi_frame_reverse`
 * and `faxvmi_write_frame` walk.  Five instructions, and it touches nothing
 * else.
 */
int
_handle_hdlc_input_open(struct fax_class1 *ctx)
{
	ctx->f1250 = 1;
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
/*
 * Drop the start-bit search back to "not locked yet".  `async_locked` goes to
 * zero and `async_window` to -1, which is the all-ones history the search
 * wants before the first element arrives; `async_shift` and `async_mask` are
 * NOT touched, because `_handle_data_output` recomputes both on the call that
 * locks.  Void: the object leaves eax holding the argument and no caller can
 * be relying on that.
 */
void
cTOOLS_handle_data_output_reset(struct fax_class1 *ctx)
{
	ctx->async_locked = 0;
	ctx->async_window = 0xffffffffu;
}

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

/*
 * The other other direction: recover an octet from each of `count` elements,
 * as `_handle_data_output` does, but WITHOUT its async start-bit search --
 * these are already-aligned HDLC receive elements, so the low byte of each
 * one is the octet.  DLE-stuff them into `dst` and append DLE ETX when
 * `terminate` is set.  `ctx` is read nowhere in the object; it is carried
 * only because every other member of this family takes it.
 *
 * FOUR `.rodata.str1.1` STRINGS, the author's own, gate the debug blocks
 * that the differential test cannot see but that are reproduced anyway --
 * they are the evidence for the function's DIRECTION, which its own name
 * does not give away:
 *
 *   "FCL1: FRAME RECEIVED (%s)\n"    (0x4926) -- printed once, when `count`
 *       is over 2 and the level allows it, naming the frame from its first
 *       three elements via `GetT30FrameIDFromBuffer`/`GetT30FrameNameByID`
 *       -- the same idiom `faxvmi_hdlc_frame`'s debug line uses on
 *       transmit.  So despite the symbol's own name, "hdlc_output" means
 *       OUTPUT TO THE HOST of a frame the modem RECEIVED, not one being
 *       sent -- the mirror of `_handle_hdlc_input`, which takes the HOST's
 *       bytes in.
 *   "HDLC Recieved Frame of %d: \n"  (0x4941, "Recieved" is the object's) --
 *       printed whenever the level allows it, whatever `count` is.
 *   "%02X,"                          (0x495e) -- once per output byte, loop
 *       and terminator alike.
 *   "%02X\n"                         (0x4964) -- once, ending the line, only
 *       when a terminator is actually appended.
 *
 * `dis.py` shows the "Recieved Frame" print (0x09f00a, one call site) reached
 * both from the `count > 2` arm, after the frame-decode print, AND from its
 * `else`, each guarded by its OWN `cmpl $0x1,dsplibs_debug_level` (0x09ef2e
 * and 0x09eff0 -- a direct compare-to-memory in one arm, a load into a
 * register first in the other) rather than one shared test after an
 * if/else, which is why the level is checked twice below rather than once.
 *
 * THE TERMINATOR WRITE IS GATED BY `terminate` ALONE, NOT BY `count` TOO
 * (F10100).  `count == 0` jumps STRAIGHT to the terminator check
 * (0x09ef21 `test %ebp,%ebp; je 0x9ef90`, and 0x9ef90 is exactly the
 * `terminate` test) -- `count != 0 && terminate != 0` gates only the LAST
 * TWO debug prints, a narrower condition folded from `setne`/`setne`/`test`
 * at 0x9ef6b-0x9ef7a.  A first pass wrote the write itself under the same
 * combined condition as the debug prints, which is wrong whenever `count`
 * is 0 and `terminate` is not -- caught by `t_class1hdlcemu.c` disagreeing
 * with the blob (word7 0 where the blob leaves 2: a zero-length record
 * still gets its DLE ETX), not by re-reading the disassembly a second time.
 */
int
cTOOLS_handle_hdlc_output(struct fax_class1 *ctx, const unsigned short *src,
			  unsigned char *dst, int count, int terminate)
{
	int out = 0;
	int i;

	(void)ctx;

	if (count != 0) {
		if (count > 2) {
			if (dsplibs_debug_level > 1) {
				int id = GetT30FrameIDFromBuffer(
				    (unsigned char)src[0],
				    (unsigned char)src[1],
				    (unsigned char)src[2]) & 0xffff7fff;

				dsplibs_debug_printf(
				    "FCL1: FRAME RECEIVED (%s)\n",
				    GetT30FrameNameByID(id));
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "HDLC Recieved Frame of %d: \n",
					    count);
			}
		} else if (dsplibs_debug_level > 1) {
			dsplibs_debug_printf(
			    "HDLC Recieved Frame of %d: \n", count);
		}

		for (i = 0; i < count; i++) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf("%02X,",
				    aReversedCharsArray[(unsigned char)src[i]]);
			dst[out++] = (unsigned char)src[i];
			if (src[i] == CLASS1_DLE) {
				dst[out++] = CLASS1_DLE;
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf("%02X,",
					    aReversedCharsArray[CLASS1_DLE]);
			}
		}
	}

	if (count != 0 && terminate != 0 && dsplibs_debug_level > 1) {
		dsplibs_debug_printf("%02X,", aReversedCharsArray[CLASS1_DLE]);
		dsplibs_debug_printf("%02X\n", aReversedCharsArray[CLASS1_ETX]);
	}
	if (terminate != 0) {
		dst[out] = CLASS1_DLE;
		dst[out + 1] = CLASS1_ETX;
		out += 2;
	}
	return out;
}

/*
 * ------------------------------------------------------------------
 * The transmit-side VMI constructors, `.text` 0x094870/0x094970/0x094a70 --
 * see class1tx.h for why they belong here rather than in `class1rx.c`
 * beside their RX siblings.
 *
 * SAME SHAPE AS `class1rx.c`'s trio: allocate a config of exactly its
 * table's size, announce at `DSPLIB_DEBUG_VERBOSE()`, copy the table over
 * the allocation, override `bitrate` and the caller's fourth argument, then
 * copy `FAXVMI_CFG` over the caller's VMI and override seven fields
 * (`short_0008`/`short_000a` differ by modulation; `slot` is what makes
 * them three functions).  Neither allocation is checked for NULL, as on
 * the RX side.
 *
 * WHAT DIFFERS FROM THE RX TRIO.  Every RX constructor stores the fourth
 * argument into the VMI's own `ptr_0014` (and, for V.17, into a *config*
 * field too); all three TX constructors ALSO store it into the config's
 * OWN LAST FIELD (`int_001c`/`int_0018`, `struct v29tx_cfg`'s `int_0018`
 * one field earlier, per `v27fax.h`'s own comment on the shape) -- the
 * `(void *)(long)` idiom faxadapt.h names, since that field is declared
 * `int` in each config struct and not a pointer.  And each RETURNS
 * `(int)cfg->bitrate`, reloaded from the freshly-built config right before
 * `ret` -- dead code under `-O3` unless the source has an explicit `return`,
 * so unlike the RX trio (`void`) these three are `int`.
 *
 * A SECOND HARDCODED FIELD, missed on the first read of the disassembly and
 * caught by `t_class1txvmi.c` disagreeing with the blob rather than assumed
 * absent: after the six/seven-dword table copy, all three OVERRIDE
 * `cfg->int_0014`'s low 16 bits with a literal 16-bit store (`movw`) --
 * `0x1` for V.17, `0x2` for V.27ter and V.29.  V.17's table default is
 * already 1, so that one is invisible to any test that only checks the
 * FINAL value; V.27ter's and V.29's tables are also 1 (`tabdump.py` over
 * the blob's own `.data` confirms it, independent of either reconstructed
 * table), so their override to 2 is a real, visible change from the
 * default. `cfg->int_0014 = <value>;` after the table copy reproduces this
 * -- a plain `int` assignment stores the same final 32 bits as the
 * object's narrower `movw`, since the upper 16 bits are already zero from
 * the dword copy, so no encoding trick is needed for behavioural fidelity.
 */
int
init_vmi_v17tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
	       int arg_2, void *arg_3)
{
	struct v17tx_cfg *cfg = sysdep_malloc(sizeof(struct v17tx_cfg));

	(void)arg_2;

	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf(
			"Initializing VMI_V17_TX Modem No ECM "
			"(Simple Packing)\n");

	*cfg = V17TX_CFG;
	cfg->bitrate = bit_rate;
	cfg->int_0014 = 1;
	cfg->int_0018 = 0;
	cfg->int_001c = (int)(long)arg_3;

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->short_0000 = 0;
	vmi->int_0004 = 1;
	vmi->short_0008 = 0x60;
	vmi->short_000a = 0x30;
	vmi->short_000c = 0;
	vmi->slot = VMI_SLOT_V17TX;
	vmi->modem_cfg = cfg;

	return cfg->bitrate;
}

int
init_vmi_v29tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
	       int arg_2, void *arg_3)
{
	struct v29tx_cfg *cfg = sysdep_malloc(sizeof(struct v29tx_cfg));

	(void)arg_2;

	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf(
			"Initializing VMI_V29_TX Modem No ECM "
			"(Simple Packing)\n");

	*cfg = V29TX_CFG;
	cfg->bitrate = bit_rate;
	cfg->int_0014 = 2;
	cfg->int_0018 = (int)(long)arg_3;

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->short_0000 = 0;
	vmi->int_0004 = 1;
	vmi->short_0008 = 0x60;
	vmi->short_000a = 0x35;
	vmi->short_000c = 0;
	vmi->slot = VMI_SLOT_V29TX;
	vmi->modem_cfg = cfg;

	return cfg->bitrate;
}

int
init_vmi_v27tx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
	       int arg_2, void *arg_3)
{
	struct v27tx_cfg *cfg = sysdep_malloc(sizeof(struct v27tx_cfg));

	(void)arg_2;

	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf(
			"Initializing VMI_V27_TX Modem No ECM "
			"(Simple Packing)\n");

	*cfg = V27TX_CFG;
	cfg->bitrate = bit_rate;
	cfg->int_0014 = 2;
	cfg->int_001c = (int)(long)arg_3;

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->short_0000 = 0;
	vmi->int_0004 = 1;
	vmi->short_0008 = 0x40;
	vmi->short_000a = 0x25;
	vmi->short_000c = 0;
	vmi->slot = VMI_SLOT_V27TX;
	vmi->modem_cfg = cfg;

	return cfg->bitrate;
}

/*
 * Tear the transmit-side data modem down.  The object's own order: free the
 * config, free the VMI block, clear `ctx->modem_vmi`, `FAXVMI_delete` the
 * handle at `ctx->vmi_b`, clear `ctx->vmi_b`, and only THEN look at
 * `ctx->f1288` (a FIFO) -- `FIFO_delete` it and clear the field when it is
 * non-null, or just clear it when it is already null.  No modulation's
 * config here needs a sub-allocation freed first, unlike the RX side's
 * V.17.
 */
void
_delete_data_tx_modem(struct fax_class1 *ctx)
{
	struct faxvmi_cfg *vmi = ctx->modem_vmi;
	struct faxvmi *handle;

	sysdep_free(vmi->modem_cfg);
	vmi = ctx->modem_vmi;
	sysdep_free(vmi);

	handle = ctx->vmi_b;
	ctx->modem_vmi = NULL;
	FAXVMI_delete(handle);
	ctx->vmi_b = NULL;

	if (ctx->f1288 != NULL)
		FIFO_delete(ctx->f1288);
	ctx->f1288 = NULL;
}

/*
 * TX_SILENCE_BEFORE_SCRM_ONES.  `.text` 0x09d720, 111 bytes.  Nothing but a
 * countdown bump, an optional debug line (the object's own literal string,
 * no format arguments), a silence block, and a state transition once
 * `countdown` catches up with `silence_blocks` -- both compared as
 * `unsigned` (the object's `cmp`/`jb`), which the usual arithmetic
 * conversions give for free since `silence_blocks` is already
 * `unsigned int`.
 */
int
_tx_silence_before_scrm_ones(struct fax_class1 *ctx, const short *rx,
			     short *tx, int word3, int word4,
			     int *rx_count, int *tx_count, int word7,
			     int *word8)
{
	(void)rx;
	(void)word3;
	(void)word4;
	(void)rx_count;
	(void)word7;

	ctx->countdown += 20;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("Tx silence before scrambled ones ...\n");

	_put_silence(tx, CLASS1_BLOCK_SAMPLES);
	*tx_count = CLASS1_BLOCK_SAMPLES;

	if (ctx->countdown >= ctx->silence_blocks)
		ctx->state = CLASS1_TX_SCRAMBLED_ONES_STATE;

	*word8 = 0;
	return 0;
}

/*
 * T30_SILENCE_BEFORE_TX_STATE.  `.text` 0x09e590, 194 bytes.
 *
 * `_put_silence`'s return value is COMPUTED and then discarded -- the very
 * next instruction (`mov 0x1228(%ebx),%eax`, ctx->countdown reloaded for
 * the threshold compare below) overwrites the register holding it before
 * anything reads it, on every path.  An earlier reading of this function
 * mistook that reload for the call's return value surviving to the final
 * store and got `n + *tx_count` (320 on the common path) where the object
 * gives a plain accumulation (160) -- caught by `t_class1delete.c`
 * disagreeing with the blob, not assumed correct from the disassembly
 * alone.
 *
 * The comparison against 400 is `unsigned` (`cmp`/`jbe`/`ja`), the same
 * shape `_recieve_silence_state`'s own `countdown` compare already
 * established needs an explicit cast to reach from a plain `int`.
 */
int
_t30_silence_before_tx_state(struct fax_class1 *ctx, const short *rx,
			     short *tx, int word3, int word4,
			     int *rx_count, int *tx_count, int word7,
			     int *word8)
{
	(void)rx;
	(void)word3;
	(void)word4;
	(void)rx_count;
	(void)word7;
	(void)word8;

	_put_silence(tx, CLASS1_BLOCK_SAMPLES);
	*tx_count = CLASS1_BLOCK_SAMPLES;

	if ((unsigned int)ctx->countdown > 400) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "%2d.%02d[sec], Elapsed 50MS second, "
			    "send preamble\n",
			    ctx->clock_sec, ctx->clock_frac);
		ctx->state = CLASS1_T30_PREAMBLE_STATE;
		ctx->countdown = 0;
		ctx->status = FAX_CLASS1_CONNECT;
	}
	ctx->countdown += *tx_count;
	return 0;
}

/*
 * HDLC_EMULATE_RECEIVE_STATE.  `.text` 0x09e1b0, 452 bytes.  See class1tx.h
 * for the shape and class1.h for `f1000`/`f12c8`/`f12cc`/`f12d0`.
 *
 * FIRST, PARSE.  Walk `ctx->f1000` from the start, `count` records of
 * `ctx->f12d0` bytes total -- entry `i` is a length, the data follows, the
 * next record starts right after.  Each length is banked into `lens[]` (see
 * `CLASS1_EMU_MAX_FRAMES`'s own comment for why it is exactly twelve long
 * and unguarded) so the SECOND walk below, to find where record `next`
 * starts, does not have to re-read the buffer.
 *
 * `ctx->prev_state != CLASS1_HDLC_EMULATE_RECEIVE_STATE` is "is this the
 * first tick since some other state entered this one" -- on that tick only,
 * a still-unsent record (`next < count`) reports FAX_CLASS1_CONNECT once.
 *
 * THE COUNTDOWN, next.  `old` is `f12c8` BEFORE this call's decrement; a
 * value that was already <= 0 is what fires the next record (or, with
 * nothing left to send, the transition to IDLE_STATE with
 * FAX_CLASS1_NO_CARRIER_NO_MESSAGE -- both spelled `8` in the object, one
 * a state and the other a status, and that coincidence is why a single
 * `mov $0x8` in the disassembly feeds two different stores).  Emitting a
 * record calls `cTOOLS_handle_hdlc_output` on it, writes the byte count
 * through `word7`, arms a two-tick delayed status, advances `f12cc`, and
 * also moves to IDLE_STATE.
 *
 * FINALLY, the object's own tail runs whether or not anything fired above:
 * once `next` has caught up to `count`, the whole buffer resets (`f12d0` to
 * 0, `f12c8` to 2, `f12cc` to 0) -- note this is an EQUALITY test in the
 * object (`je`), not `next >= count`, so it is written that way here too --
 * and every path ends the same: a block of silence out, `*tx_count` set to
 * it, return 0.
 */
int
_hdlc_emulate_receive_state(struct fax_class1 *ctx, const short *rx,
			    short *tx, int word3, int word4, int *rx_count,
			    int *tx_count, int word7, int *word8)
{
	int total = ctx->f12d0;
	int idx = 0;
	int count = 0;
	int lens[CLASS1_EMU_MAX_FRAMES];
	int next;
	int old;

	(void)rx;
	(void)word4;
	(void)rx_count;
	(void)word8;

	while (idx < total) {
		int len = ctx->f1000[idx];

		lens[count] = len;
		count++;
		idx += len + 1;
	}

	next = ctx->f12cc;
	if (ctx->prev_state != CLASS1_HDLC_EMULATE_RECEIVE_STATE) {
		if (next < count)
			ctx->status = FAX_CLASS1_CONNECT;
	}

	old = ctx->f12c8;
	ctx->f12c8 = old - 1;

	if (old <= 0) {
		if (next >= count) {
			ctx->status = FAX_CLASS1_NO_CARRIER_NO_MESSAGE;
			ctx->state = CLASS1_IDLE_STATE;
		} else {
			int start = 0, i;
			int n;

			for (i = 0; i < next; i++)
				start += lens[i] + 1;

			n = cTOOLS_handle_hdlc_output(ctx,
			    &ctx->f1000[start + 1],
			    (unsigned char *)(long)word3,
			    ctx->f1000[start], 1);
			*(int *)(long)word7 = n;
			ctx->delayed_status = 1;
			ctx->delayed_status_countdown = 2;
			next++;
			ctx->f12cc = next;
			ctx->state = CLASS1_IDLE_STATE;
		}
	}

	if (next == count) {
		ctx->f12d0 = 0;
		ctx->f12c8 = 2;
		ctx->f12cc = 0;
	}

	_put_silence(tx, CLASS1_BLOCK_SAMPLES);
	*tx_count = CLASS1_BLOCK_SAMPLES;
	return 0;
}

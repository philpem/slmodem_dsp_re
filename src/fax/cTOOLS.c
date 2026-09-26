/*
 * cTOOLS.c -- the blob's cTOOLS.c translation unit (TU-reconciliation, issue #6/#20/#67).
 * Functions moved verbatim from class1tx.c in blob emission order.
 */
#include <stddef.h>
#include <unistd.h>

#include "dsplib/class1.h"
#include "dsplib/class1rx.h"
#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/faxvmi.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sysdep.h"
#include "dsplib/t30frame.h"
#include "dsplib/v17fax.h"
#include "dsplib/v21fax.h"
#include "dsplib/v27fax.h"
#include "dsplib/v29data.h"
#include "dsplib/v29fax.h"

/* .bss+0x8c4 -- the one relocation against `temp` is in _handle_data_input. */
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
 * `hdlc_write_cursor - 1` as the length, so element zero is the length slot the frame is
 * eventually length-prefixed with -- the same layout `faxvmi_frame_reverse`
 * and `faxvmi_write_frame` walk.  Five instructions, and it touches nothing
 * else.
 */
int
_handle_hdlc_input_open(struct fax_class1 *ctx)
{
	ctx->hdlc_write_cursor = 1;
	return 0;
}


int
_handle_hdlc_input_close(struct fax_class1 *ctx)
{
	ctx->scratch_frame_len = (short)(ctx->hdlc_write_cursor - 1);
	if (ctx->flags004 & CLASS1_FLAG_FRAME_END_LATCH)
		ctx->frame_end_latch = 1;
	return 0;
}


/*
 * The same unstuffing for an HDLC frame, and three things make it a different
 * function rather than a mode of the one above.
 *
 *   - The write cursor is the SESSION's (`hdlc_write_cursor`), not a local, so a frame
 *     accumulates across calls and the caller's `dst` is indexed from
 *     wherever the last call left off.  `dst` must therefore be sized for the
 *     whole frame, not for one block.
 *   - DLE ETX ends the FRAME: it does the same two stores
 *     `_handle_hdlc_input_close` does -- the length into `scratch_frame_len`, and `frame_end_latch`
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
	int out = ctx->hdlc_write_cursor;
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
				ctx->hdlc_write_cursor = out;
				ctx->scratch_frame_len = (short)(out - 1);
				if (ctx->flags004 & CLASS1_FLAG_FRAME_END_LATCH)
					ctx->frame_end_latch = 1;
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

	ctx->hdlc_write_cursor = out;
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

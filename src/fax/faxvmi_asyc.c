/*
 * faxvmi_asyc.c -- Class 1 fax: the VMI's asynchronous framing codec.
 *
 * Split out of the merged faxvmi.c so the definitions sit in the translation
 * unit the object's FILE order and .text addresses give them (finding F11399).
 * The FILE is faxvmi_asyc.c, between faxvmi.c and faxvmi_hdlc.c; its two
 * functions fill [0x957f0, 0x95b10) contiguously.  Bodies moved verbatim.
 */
#include <stddef.h>

#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/nulldp.h"
#include "dsplib/sysdep.h"
#include "dsplib/t30frame.h"

/*
 * Asynchronous framing on the way OUT: each octet from the ring becomes a
 * ten-bit character.
 *
 *	word = ((octet << 1) | 1) & ~0x200,	mask = 0x200
 *
 * so the first bit shifted out is bit 9, forced to ZERO by the AND -- the
 * start bit; then the octet's eight bits, most significant first; then bit 0,
 * set by the OR -- the stop bit.  The `& ~0x200` is a separate instruction in
 * the object (0x95948) and is written as one here.
 *
 * THE REFILL HAS FOUR ARMS AND ONLY TWO OF THEM SET A MASK OF 1.  A zero-bit
 * run in progress emits one zero bit; the run's LAST refill clears
 * `zero_run_send`, sets `pack_word` to 1 and LEAVES THE MASK AT ZERO, so the
 * bit it emits is a space rather than the mark the assignment reads as --
 * D1120, reproduced.  An empty ring emits a single mark bit and raises
 * `vmi->underrun`.
 *
 * `pack_count` output elements are always produced, and that count is what is
 * returned -- re-read from the link object after the trailing write, not from
 * the local the loop counted down.
 */
int
faxvmi_asyc_pack(struct faxvmi *vmi, unsigned short *src, short count)
{
	struct faxvmi_framer *fr;
	struct faxvmi_link *lk;
	unsigned short *out;
	unsigned int word, acc, elem_mask;
	unsigned short mask, bit, width;
	short n, left;

	left = (short)(count - faxvmi_write_fifo(vmi, &src, count));

	lk = vmi->link;
	fr = vmi->framer;
	out = lk->buf;
	word = fr->pack_word;
	mask = (unsigned short)fr->pack_mask;
	n = lk->pack_count;
	width = lk->pack_width;
	bit = fr->pack_bit;
	acc = fr->pack_acc;
	elem_mask = (1u << width) - 1;

	while (n != 0) {
		unsigned int b;

		if (mask == 0) {
			if (fr->zero_run_send != 0) {
				if (fr->zero_run_bits != 0) {
					fr->zero_run_bits = (unsigned short)
					    (fr->zero_run_bits - 1);
					word = 0;
					mask = 1;
				} else {
					/* D1120: no mask, so a space */
					fr->zero_run_send = 0;
					word = 1;
				}
			} else if (fr->count != 0) {
				unsigned short rd = fr->rd;
				unsigned int v = fr->fifo[rd];

				fr->rd = (unsigned short)(rd + 1);
				fr->count = (unsigned short)(fr->count - 1);
				if (fr->rd >= fr->fifo_size)
					fr->rd = 0;
				word = ((v << 1) | 1) & 0xfffffdffu;
				mask = 0x200;
			} else {
				word = 1;
				mask = 1;
				vmi->underrun = 1;
			}
		}
		b = (word & mask) != 0;
		acc = (acc << 1) | b;
		bit = (unsigned short)(bit + 1);
		mask = (unsigned short)(mask >> 1);
		if (bit == width) {
			*out++ = (unsigned short)(acc & elem_mask);
			n = (short)(n - 1);
			bit = 0;
		}
	}

	fr->pack_bit = bit;
	fr->pack_mask = mask;
	fr->pack_acc = acc;
	fr->pack_word = word;

	left = (short)(left - faxvmi_write_fifo(vmi, &src, left));
	vmi->framer->residue = left;
	return vmi->link->pack_count;
}

/*
 * ============================ the unpackers ============================
 *
 * All three share a preamble and a postamble, and they are written out in
 * full in each rather than factored, because the object has them in full in
 * each: there is no shared helper in the disassembly and inventing one would
 * move three functions' code generation at once.
 *
 * The preamble takes the framer's bit position into locals; the postamble
 * puts it back.  `vmi->link->buf` is re-read every call and the advanced
 * cursor is DISCARDED -- see faxvmi.h.
 */

/*
 * Asynchronous framing: hunt for a start bit, then take eight data bits.
 *
 * `hunt` is the object's +0x30, which FAXVMI_create sets to 1.  While it is
 * set, each bit REPLACES it -- so a one keeps hunting and the first zero (the
 * start bit) clears it and the eight bits after that are the character.
 *
 * The two tests on `acc & 0x7fffff` run on every bit in both states.  23
 * zeros in a row means the line has gone hard down: the character in progress
 * is abandoned and the hunt restarts.  The first one bit after such a run --
 * the accumulator reading 22 zeros then a one, which is exactly the value 1 --
 * latches `framer->zero_run_seen`, which is a level the VMI exports and not
 * something this function acts on.
 */
int
faxvmi_asyc_unpack(struct faxvmi *vmi, unsigned short *dst, short count)
{
	struct faxvmi_framer *fr = vmi->framer;
	struct faxvmi_link *lk = vmi->link;
	unsigned int word = fr->unpack_word;
	unsigned short bit = fr->unpack_bit;
	unsigned short mask = fr->unpack_mask;
	unsigned int acc = fr->unpack_acc;
	unsigned short *src = lk->buf;
	unsigned int top = 1u << (lk->unpack_width - 1);
	int hunt = fr->async_hunt;
	int zero_run = 0;
	int overflow = 0;
	short nout = 0;
	short left = count;

	while (left != 0) {
		unsigned int b;

		if (mask == 0) {
			mask = (unsigned short)top;
			word = *src++;
			left = (short)(left - 1);
		}
		b = (word & mask) != 0;
		acc = (acc << 1) | b;
		mask = (unsigned short)(mask >> 1);

		if (hunt != 0) {
			hunt = (int)b;
		} else {
			bit = (unsigned short)(bit + 1);
			if (bit == 8) {
				bit = 0;
				hunt = 1;
				if ((int)nout >= (int)vmi->max_frame)
					overflow = 1;
				else {
					*dst++ = (unsigned short)(acc & 0xff);
					nout = (short)(nout + 1);
				}
			}
		}

		if ((acc & 0x7fffff) == 0) {
			bit = 0;
			hunt = 1;
			continue;
		}
		if ((acc & 0x7fffff) == 1)
			zero_run = 1;
	}

	fr->async_hunt = hunt;
	fr->unpack_mask = mask;
	fr->unpack_acc = acc;
	fr->zero_run_seen = zero_run;
	fr->unpack_word = word;
	fr->unpack_bit = bit;
	vmi->overflow = overflow;
	return nout;
}

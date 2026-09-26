/*
 * faxvmi_pack.c -- Class 1 fax: the VMI's transparent (simple) framing codec.
 *
 * Split out of the merged faxvmi.c; the FILE is faxvmi_pack.c, between
 * faxvmi_hdlc.c and faxvmi_tbls.c; its two functions fill [0x964e0, 0x96780)
 * contiguously.  Bodies moved verbatim (finding F11399).
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
 * Simple framing on the way OUT: no framing.  Each ring element is shifted
 * out from bit 7 down, so the octet's most significant bit goes first, and an
 * empty ring contributes a zero octet.
 *
 * `real` IS NOT A SPELLING CHOICE.  The object holds a 0/1 value in a slot,
 * initialises it to 1, sets it to zero on the ring-empty arm ALONE, and ADDS
 * it to the running count at every emitted element (0x964fb, 0x96589,
 * 0x965c5).  So the return counts the elements produced before the ring first
 * ran dry and stops advancing for the rest of the call, while the block
 * itself is still filled to `pack_count`.  A `nout++` guarded by a test would
 * compile to a branch, which is not what is there; the two also disagree
 * whenever the ring refills after a gap, which the object never lets happen
 * because the flag is never set back to 1.
 */
int
faxvmi_simp_pack(struct faxvmi *vmi, unsigned short *src, short count)
{
	struct faxvmi_framer *fr;
	struct faxvmi_link *lk;
	unsigned short *out;
	unsigned int word, acc, elem_mask;
	unsigned short mask, bit, width;
	short n, nout = 0, left;
	short real = 1;

	left = (short)(count - faxvmi_write_fifo(vmi, &src, count));

	fr = vmi->framer;
	lk = vmi->link;
	mask = (unsigned short)fr->pack_mask;
	out = lk->buf;
	acc = fr->pack_acc;
	n = lk->pack_count;
	width = lk->pack_width;
	word = fr->pack_word;
	bit = fr->pack_bit;
	elem_mask = (1u << width) - 1;

	while (n != 0) {
		unsigned int b;

		if (mask == 0) {
			if (fr->count != 0) {
				unsigned short rd = fr->rd;

				word = fr->fifo[rd];
				fr->rd = (unsigned short)(rd + 1);
				fr->count = (unsigned short)(fr->count - 1);
				if (fr->rd >= fr->fifo_size)
					fr->rd = 0;
			} else {
				word = 0;
				real = 0;
				vmi->underrun = 1;
			}
			mask = 0x80;
		}
		b = (word & mask) != 0;
		acc = (acc << 1) | b;
		bit = (unsigned short)(bit + 1);
		mask = (unsigned short)(mask >> 1);
		if (bit == width) {
			nout = (short)(nout + real);
			*out++ = (unsigned short)(acc & elem_mask);
			bit = 0;
			n = (short)(n - 1);
		}
	}

	fr->pack_mask = mask;
	fr->pack_acc = acc;
	fr->pack_bit = bit;
	fr->pack_word = word;

	left = (short)(left - faxvmi_write_fifo(vmi, &src, left));
	vmi->framer->residue = left;
	return nout;
}

/*
 * Simple framing: no framing.  Eight bits are an octet, first bit taken into
 * bit 7, and the only thing that can go wrong is the destination filling up.
 */
int
faxvmi_simp_unpack(struct faxvmi *vmi, unsigned short *dst, short count)
{
	struct faxvmi_framer *fr = vmi->framer;
	struct faxvmi_link *lk = vmi->link;
	unsigned short mask = fr->unpack_mask;
	unsigned short *src = lk->buf;
	unsigned int word = fr->unpack_word;
	unsigned short bit = fr->unpack_bit;
	unsigned int top = 1u << (lk->unpack_width - 1);
	unsigned int acc = fr->unpack_acc;
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
		bit = (unsigned short)(bit + 1);
		if (bit != 8)
			continue;
		bit = 0;
		if ((int)nout >= (int)vmi->max_frame) {
			overflow = 1;
			continue;
		}
		*dst++ = (unsigned short)(acc & 0xff);
		nout = (short)(nout + 1);
	}

	fr->unpack_mask = mask;
	fr->unpack_bit = bit;
	fr->unpack_word = word;
	fr->unpack_acc = acc;
	vmi->overflow = overflow;
	return nout;
}

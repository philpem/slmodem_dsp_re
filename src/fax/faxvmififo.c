/*
 * faxvmififo.c -- Class 1 fax: the VMI's ring writers and frame reverser.
 *
 * Split out of the merged faxvmi.c; the FILE is faxvmififo.c, between
 * faxvmi_utls.c and T30frames.c; its three functions fill [0x96850, 0x96bb0)
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

/*
 * ========================== the ring writers ==========================
 *
 * THE `taken` FLAG IS NOT A SPELLING CHOICE.  The object loads
 * `vmi->underrun` into a register BEFORE the loop, sets that register to zero
 * inside it, and stores it at the loop's NORMAL exit -- 0x968f6 loads,
 * 0x96961 zeroes, 0x96978 stores -- while the ring-full path returns without
 * passing the store at all.  So the field is cleared when the whole request
 * was accepted, left ALONE when the ring filled part-way, and re-stored
 * unchanged when the count was zero.  A `vmi->underrun = 0;` inside the loop
 * body would agree on the first case and disagree on the second, which is a
 * behavioural difference and not a codegen one.  D1070.
 */

int
faxvmi_write_fifo(struct faxvmi *vmi, unsigned short **src, short count)
{
	struct faxvmi_framer *fr = vmi->framer;
	unsigned short *p = *src;
	int taken = vmi->underrun;
	short n = 0;
	short i;

	for (i = count; i != 0; i--) {
		if (fr->count >= fr->fifo_size) {
			*src = p;
			return n;
		}
		fr->fifo[fr->wr] = *p++;
		fr->wr = (unsigned short)(fr->wr + 1);
		if (fr->wr >= fr->fifo_size)
			fr->wr = 0;
		fr->count = (unsigned short)(fr->count + 1);
		n = (short)(n + 1);
		taken = 0;
	}

	*src = p;
	vmi->underrun = taken;
	return n;
}

/*
 * One frame per iteration, each `*p` elements long with the length in front
 * of it.  Three elements of overhead go into the ring for every frame -- the
 * length and the two FCS octets -- which is where the ring's `+ 3` headroom
 * in FAXVMI_create comes from, and the fit test asks for room for all of them
 * before anything is written.
 *
 * The two FCS elements are appended WITHOUT a fullness test and WITHOUT
 * touching the occupancy, because the occupancy was raised by three up front.
 * If the copy loop still manages to fill the ring -- which it can, since the
 * fit test is signed and a negative length passes it -- the FCS is written
 * over the ring's oldest elements anyway.  That is the object's; D1071.
 */
int
faxvmi_write_frame(struct faxvmi *vmi, unsigned short **src, short count)
{
	struct faxvmi_framer *fr = vmi->framer;
	unsigned short *p = *src;
	short written = 0;
	short i;

	for (i = count; i != 0; i--) {
		short len = (short)*p;
		unsigned short fcs;
		int taken;
		short j;

		if ((int)fr->count + (int)len + 3 >= (int)fr->fifo_size)
			break;

		fr->fifo[fr->wr] = (unsigned short)(len + 2);
		fr->wr = (unsigned short)(fr->wr + 1);
		if (fr->wr >= fr->fifo_size)
			fr->wr = 0;
		fr->count = (unsigned short)(fr->count + 3);
		written = (short)(written + 1);
		p++;

		fcs = (unsigned short)faxvmi_gen_fcs16(p, len);

		taken = vmi->underrun;
		for (j = len; j != 0; j--) {
			if (fr->count >= fr->fifo_size)
				goto put_fcs;
			fr->fifo[fr->wr] = *p++;
			fr->wr = (unsigned short)(fr->wr + 1);
			if (fr->wr >= fr->fifo_size)
				fr->wr = 0;
			fr->count = (unsigned short)(fr->count + 1);
			taken = 0;
		}
		vmi->underrun = taken;

	put_fcs:
		fr->fifo[fr->wr] = (unsigned short)(fcs >> 8);
		fr->wr = (unsigned short)(fr->wr + 1);
		if (fr->wr >= fr->fifo_size)
			fr->wr = 0;
		fr->fifo[fr->wr] = (unsigned short)(fcs & 0xff);
		fr->wr = (unsigned short)(fr->wr + 1);
		if (fr->wr >= fr->fifo_size)
			fr->wr = 0;
	}

	*src = p;
	return written;
}

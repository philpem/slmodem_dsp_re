/*
 * fifo.c -- Class 1 fax: the FIFO of 16-bit elements.
 *
 * Reconstructed from dsplibs.o's class1tx.c +94 span (FIFO cluster,
 * 0x096bb0..0x096df0):
 *
 *   FIFO_read        .text 0x096c60   176
 *   FIFO_write       .text 0x096d10   152
 *   FIFO_delete      .text 0x096db0    32
 *   FIFO_full_test   .text 0x096dd0    33
 *
 * `FIFO_create` (0x096bb0) is deliberately NOT here: it reads the default
 * configuration `FIFO_CFG`, an unwritten data symbol, and this tree links no
 * scaffold, so naming it would fail every binary at link (F8492, F8493).
 * Its instructions are still what establish the layout in faxfifo.h.
 *
 * EVERY STEP IS 16-BIT ON PURPOSE, because the object's is.  In
 * FIFO_full_test the numerator is truncated to a short before the divide (the
 * wrap faxfifo.h describes), the quotient is truncated again, and the
 * threshold compare is on shorts.  In read and write the cursors, the counts
 * and the free-space subtraction are all 16-bit, and the wrap test compares
 * the incremented cursor against `size` UNSIGNED -- the same field
 * FIFO_full_test divides by SIGNED.  A wider spelling anywhere here would fix
 * a bug the object has and fail the differential doing it.
 */

#include "dsplib/faxfifo.h"
#include "dsplib/sysdep.h"

int
FIFO_full_test(struct fax_fifo *f)
{
	short num = (short)(f->count << 14);

	num = (short)(num / f->size);
	return num >= FIFO_FULL_Q14;
}

/*
 * `take` is min(occupancy, count); the balance is padded from `f->fill`, so
 * `count` elements are ALWAYS written.  The fill is re-read inside its loop
 * because the object re-reads it there (dst may alias the object).
 */
int
FIFO_read(struct fax_fifo *f, unsigned short *dst, unsigned short count)
{
	unsigned short size = f->size;
	unsigned short take = f->count;
	unsigned short pad;
	unsigned short rd;
	unsigned short i;
	unsigned short *buf;

	if (take > count)
		take = count;
	rd = f->rd;
	pad = count - take;
	buf = f->buf;

	for (i = take; i != 0; i--) {
		*dst++ = buf[rd];
		rd++;
		if (rd >= size)
			rd = 0;
	}
	for (i = pad; i != 0; i--)
		*dst++ = f->fill;

	f->rd = rd;
	f->count -= take;
	return take;
}

/*
 * `put` is min(count, free), where free is the 16-bit difference of `size`
 * and the occupancy -- see D1051 for what that does when the occupancy is
 * already past `size`.
 */
int
FIFO_write(struct fax_fifo *f, unsigned short *src, unsigned short count)
{
	unsigned short size = f->size;
	unsigned short avail = size - f->count;
	unsigned short put = count;
	unsigned short wr;
	unsigned short i;
	unsigned short *buf;

	if (put > avail)
		put = avail;
	buf = f->buf;
	wr = f->wr;

	for (i = put; i != 0; i--) {
		buf[wr] = *src++;
		wr++;
		if (wr >= size)
			wr = 0;
	}

	f->wr = wr;
	f->count += put;
	return put;
}

void
FIFO_delete(struct fax_fifo *f)
{
	sysdep_free(f->buf);
	sysdep_free(f);
}

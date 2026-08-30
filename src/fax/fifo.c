/*
 * fifo.c -- Class 1 fax: the byte FIFO's occupancy test.
 *
 * Reconstructed from dsplibs.o's class1tx.c +94 span (FIFO cluster,
 * 0x096bb0..0x096df0):
 *
 *   FIFO_full_test   .text 0x096dd0    33
 *
 * Finding F8320's no-entry-point bucket.  The rest of the FIFO
 * (create/read/write/delete) is the fax phase's.
 *
 * EVERY STEP IS 16-BIT ON PURPOSE, because the object's is: the numerator
 * is truncated to a short before the divide (the wrap faxfifo.h describes),
 * the quotient is truncated again, and the threshold compare is on shorts.
 * A wider spelling here would fix a bug the object has and fail the
 * differential doing it.
 */

#include "dsplib/faxfifo.h"

int
FIFO_full_test(struct fax_fifo *f)
{
	short num = (short)(f->count << 14);

	num = (short)(num / f->size);
	return num >= FIFO_FULL_Q14;
}

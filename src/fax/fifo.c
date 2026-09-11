/*
 * fifo.c -- Class 1 fax: the FIFO of 16-bit elements.
 *
 * Reconstructed from dsplibs.o's class1tx.c +94 span (FIFO cluster,
 * 0x096bb0..0x096df0):
 *
 *   FIFO_create      .text 0x096bb0   167
 *   FIFO_read        .text 0x096c60   176
 *   FIFO_write       .text 0x096d10   152
 *   FIFO_delete      .text 0x096db0    32
 *   FIFO_full_test   .text 0x096dd0    33
 *
 * `FIFO_create` WAS DECLINED TWICE for its default configuration `FIFO_CFG`
 * (F9020, F9199); see faxfifo.h for why it now links and for the F9058 trap
 * it sits in.  Findings F9356, F9500.
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

/*
 * The object's own `FIFO_CFG`, `R` at .rodata:0x9654: `00 00 64 00 00 00`.
 * See faxfifo.h for the F9058/F9199/F9356 derivation of WHICH of the blob's
 * two same-named tables this is, and why defining it here does not collide
 * at link.  Findings F9500.
 */
const struct fifo_cfg FIFO_CFG = { 0, 100, 0 };

/*
 * `f` NULL: allocate the 20-byte object.  `cfg` NULL: read `FIFO_CFG`
 * directly rather than through a local copy -- 0x096c1b/0x096c22 are two
 * loads straight off the global, not a `cfg = &FIFO_CFG;` indirection, which
 * is why this is spelled as two parallel reads below rather than a pointer
 * reassignment.
 *
 * The count-down fill loop is a SIGNED comparison against `size` sign
 * extended from the object's own `movswl %ax,%edx` / `cmp %eax,%edx; jl`
 * (0x096c06..0x096c12) -- so it is written as a plain `for` over `short i`
 * against `f->size` rather than the unsigned idiom `FIFO_read`/`FIFO_write`
 * use for their cursors.  Both are the object's; they are different loops.
 */
struct fax_fifo *
FIFO_create(struct fax_fifo *f, const struct fifo_cfg *cfg)
{
	short word0, size;
	unsigned short fill;

	if (cfg != NULL) {
		word0 = cfg->word0;
		size = cfg->size;
		fill = (unsigned short)cfg->fill;
	} else {
		word0 = FIFO_CFG.word0;
		size = FIFO_CFG.size;
		fill = (unsigned short)FIFO_CFG.fill;
	}

	if (f == NULL) {
		f = sysdep_malloc(sizeof(struct fax_fifo));
		f->buf = (unsigned short *)
			sysdep_malloc((unsigned)(unsigned short)size * 2);
	}

	f->short_000 = word0;
	f->size = size;
	f->count = 0;
	f->rd = 0;
	f->fill = fill;
	f->wr = 0;

	{
		short i;

		for (i = 0; i < f->size; i++)
			f->buf[(unsigned short)i] = 0;
	}

	return f;
}

int
FIFO_full_test(struct fax_fifo *f)
{
	short num = (short)(f->count << 14);
	short threshold = FIFO_FULL_Q14;

	num = (short)(num / f->size);
	return num >= threshold;
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

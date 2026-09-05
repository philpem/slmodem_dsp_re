/*
 * fifo8.c -- the byte ring of the `Fdspkrnl.c` span.
 *
 * Emission order follows the object: FIFO8_create 0xaef30, FIFO8_read
 * 0xaefe0, FIFO8_write 0xaf090, FIFO8_delete 0xaf130.
 *
 * Everything here is 16-bit arithmetic and it is not incidental: the object
 * compares cursors with `cmp %ax,%cx`, wraps them with `movzwl`, and counts
 * its loops down through a 16-bit register.  A 32-bit `int` cursor would
 * behave identically until the ring is longer than 32767 bytes, so no test
 * can separate the two -- the disassembly is the only evidence and it is
 * unambiguous.
 */

#include "dsplib/fifo8.h"
#include "dsplib/sysdep.h"

/*
 * The blob's built-in parameters, a LOCAL object in `.data` at 0x83a0 named
 * `FIFO_CFG` -- the author's name, kept.  A 300-byte ring padded with zero.
 * The leading short is zero there and no FIFO8_* function reads it.
 */
static struct fifo8_cfg FIFO_CFG = { 0, 300, 0 };

/*
 * The parameters are taken by VALUE into a local and the local is what the
 * FIFO ends up holding -- the object copies six bytes out of the argument
 * (or out of FIFO_CFG) into its frame, and later copies the same six bytes
 * out of the frame into the object, which is why the default path can share
 * the tail with the supplied-parameters path.
 *
 * `buf` is allocated only when the object itself is: a caller supplying
 * storage supplies the ring with it.
 */
struct fifo8 *
FIFO8_create(struct fifo8 *f, const struct fifo8_cfg *cfg)
{
	struct fifo8_cfg c;
	short i;

	if (cfg != 0)
		c = *cfg;
	else
		c = FIFO_CFG;

	if (f == 0) {
		f = (struct fifo8 *)sysdep_malloc(sizeof(*f));
		f->buf = (unsigned char *)sysdep_malloc(c.size);
	}

	f->cfg = c;
	f->count = 0;
	f->rd = 0;
	f->wr = 0;
	for (i = 0; i < f->cfg.size; i++)
		f->buf[i] = 0;
	return f;
}

/*
 * Up to `n` bytes out of the ring, and then -- this is the part that is not
 * a plain ring read -- the caller's remaining n-cnt bytes are filled with
 * cfg.fill.  The pad byte is re-read from the object on every pass, because
 * the caller's buffer may be the object.
 */
short
FIFO8_read(struct fifo8 *f, unsigned char *dst, unsigned short n)
{
	unsigned short size = f->cfg.size;
	unsigned short cnt = f->count > n ? n : f->count;
	unsigned short pad = n - cnt;
	unsigned short rd = f->rd;
	unsigned char *buf = f->buf;
	unsigned short i;

	i = cnt;
	while (i--) {
		*dst++ = buf[rd];
		rd++;
		if (rd >= size)
			rd = 0;
	}
	i = pad;
	while (i--)
		*dst++ = (unsigned char)f->cfg.fill;

	f->rd = rd;
	f->count -= cnt;
	return (short)cnt;
}

/*
 * Up to `n` bytes in, clipped to the free room.  A full ring takes nothing
 * and returns zero; there is no overwrite-oldest behaviour.
 */
short
FIFO8_write(struct fifo8 *f, const unsigned char *src, unsigned short n)
{
	unsigned short size = f->cfg.size;
	unsigned short room = size - f->count;
	unsigned short cnt = n > room ? room : n;
	unsigned short wr = f->wr;
	unsigned char *buf = f->buf;
	unsigned short i;

	i = cnt;
	while (i--) {
		buf[wr] = *src++;
		wr++;
		if (wr >= size)
			wr = 0;
	}

	f->wr = wr;
	f->count += cnt;
	return (short)cnt;
}

/* The buffer first, then the object -- the second free is a tail call. */
void
FIFO8_delete(struct fifo8 *f)
{
	sysdep_free(f->buf);
	sysdep_free(f);
}

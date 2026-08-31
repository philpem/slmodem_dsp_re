/*
 * fifo8.h -- the byte ring the voice path passes samples through.
 *
 * Four functions, adjacent in the blob (0xaef30..0xaf150) inside the span
 * labelled `Fdspkrnl.c`: create, read, write, delete, in that order.  The
 * layouts below are read from those four and from nothing else, so a field
 * no one of them touches keeps a neutral name.
 *
 * The "8" is the element width: the ring holds bytes, and every index and
 * length in it is a 16-bit quantity.
 */

#ifndef DSPLIB_FIFO8_H
#define DSPLIB_FIFO8_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The creation parameters, copied into the FIFO whole -- the object copies
 * six bytes with one `movl` and one `movw`, which is a struct assignment and
 * not three field stores, and that is what fixes this as a type of its own.
 *
 * The blob's own default instance is a LOCAL `.data` object called
 * `FIFO_CFG`, so the name is the author's; the TYPE name here is ours.
 */
struct fifo8_cfg {
	short		short_00;	/* +0x00 no FIFO8_* function reads it */
	unsigned short	size;		/* +0x02 ring length, in bytes       */
	short		fill;		/* +0x04 byte FIFO8_read pads a short
					 *       read with (low 8 bits used)  */
};

/*
 * The ring itself.  `sizeof` is 0x14, which is what FIFO8_create asks
 * sysdep_malloc for.
 */
struct fifo8 {
	struct fifo8_cfg cfg;		/* +0x00 the creation parameters     */
					/* +0x06 two bytes of alignment      */
	unsigned char	*buf;		/* +0x08 cfg.size bytes              */
	unsigned short	count;		/* +0x0c bytes currently held        */
	unsigned short	rd;		/* +0x0e read cursor                 */
	unsigned short	wr;		/* +0x10 write cursor                */
};

/*
 * Initialise `f` -- allocating both it and its buffer when `f` is NULL --
 * from `cfg`, or from the blob's built-in defaults when `cfg` is NULL.  The
 * ring is zeroed; the cursors and the count are reset.  Returns `f`.
 *
 * Note that the buffer is allocated ONLY on the allocating path: handed a
 * caller-owned struct, FIFO8_create expects `buf` to be set already and
 * clears cfg.size bytes through it.
 */
struct fifo8 *FIFO8_create(struct fifo8 *f, const struct fifo8_cfg *cfg);

/* Free the ring buffer, then the object. */
void FIFO8_delete(struct fifo8 *f);

/*
 * Copy up to `n` bytes in, stopping at the free room (cfg.size - count).
 * Returns how many were taken.
 */
short FIFO8_write(struct fifo8 *f, const unsigned char *src, unsigned short n);

/*
 * Copy up to `n` bytes out.  Fewer than `n` bytes held is not short-changed:
 * the remainder of the caller's `n` is padded with cfg.fill.  Returns how
 * many bytes came out of the RING, not how many were written.
 */
short FIFO8_read(struct fifo8 *f, unsigned char *dst, unsigned short n);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_FIFO8_H */

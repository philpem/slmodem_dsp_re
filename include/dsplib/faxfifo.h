/*
 * faxfifo.h -- Class 1 fax: the FIFO of 16-bit elements.
 *
 * `FIFO_read`, `FIFO_write`, `FIFO_delete` and `FIFO_full_test` are
 * reconstructed; `FIFO_create` (.text 0x096bb0) is not, because its default
 * configuration `FIFO_CFG` is an unwritten data symbol and a reference to it
 * would not link (F8492/F8493).  Its instructions are still the evidence for
 * the layout below and are quoted per field.
 *
 * WHAT `FIFO_create` ESTABLISHES.  It allocates `sysdep_malloc(0x14)` for the
 * object and `sysdep_malloc(size * 2)` for the buffer, so the object is 20
 * bytes and the buffer holds `size` SIXTEEN-BIT elements -- not bytes.  It
 * copies six bytes out of its configuration argument as one 32-bit store to
 * +0x00 and one 16-bit store to +0x04, which is why +0x00 and +0x02 are one
 * aligned pair; then it zeroes +0x0c, +0x0e and +0x10 and clears the whole
 * buffer.
 */

#ifndef DSPLIB_FAXFIFO_H
#define DSPLIB_FAXFIFO_H

struct fax_fifo {
	short short_000;		/* +0x00 config word 0's low half;
					 * no reconstructed function reads
					 * it                              */
	short size;			/* +0x02 capacity, in elements     */
	unsigned short fill;		/* +0x04 what FIFO_read emits once
					 * the FIFO runs dry (usage
					 * inference -- it is the only read
					 * of this field anywhere here)    */
	unsigned char pad_06[2];	/* +0x06                            */
	unsigned short *buf;		/* +0x08 `size` elements            */
	unsigned short count;		/* +0x0c elements held              */
	unsigned short rd;		/* +0x0e read cursor, in elements   */
	unsigned short wr;		/* +0x10 write cursor, in elements  */
	unsigned char pad_12[2];	/* +0x12 pads the 0x14 allocation   */
};

/*
 * Occupancy in Q14: 14747 / 16384 is 0.90008..., so this answers "at least
 * 90% full".
 */
#define FIFO_FULL_Q14	0x399b

/*
 * 1 when count/size >= 90%, 0 below -- as WRITTEN.  As COMPILED the
 * arithmetic is a 16-bit chain: count << 14 is TRUNCATED TO A SHORT before
 * the divide, so a count of 2 lands on -32768 and, for any POSITIVE size,
 * everything above count == 1 answers "not full" through a negative
 * quotient; with a positive size the only input that can answer 1 is
 * count == 1 with size == 1.  Reproduced, not repaired;
 * docs/deviations.md D952.  `size` is read signed, and zero divides.
 */
int FIFO_full_test(struct fax_fifo *f);

/*
 * Take up to `count` elements into `dst` and return HOW MANY WERE REAL.  The
 * shortfall is not left alone: the balance of `count` is filled with
 * `f->fill`, so the destination is always written `count` times.  THAT MAKES
 * `count` THE DESTINATION'S SIZE, not a request -- a caller sizing `dst` from
 * the FIFO's occupancy overruns it (this is D956's shape, so it is called out
 * rather than discovered again).
 *
 * Every cursor step is 16-bit: `rd` advances and wraps against `size` read as
 * an UNSIGNED short, while FIFO_full_test reads the same field signed.  Both
 * readings are the object's; see D1050.
 */
int FIFO_read(struct fax_fifo *f, unsigned short *dst, unsigned short count);

/*
 * Append up to `count` elements from `src`, clamped to the free space, and
 * return how many were taken.  The free space is `(unsigned short)(size -
 * count)`, so a FIFO holding more than `size` wraps to a large free count
 * rather than to zero -- reproduced, D1051.
 */
int FIFO_write(struct fax_fifo *f, unsigned short *src, unsigned short count);

/* Free the buffer, then the object.  The second free is a tail call. */
void FIFO_delete(struct fax_fifo *f);

#endif /* DSPLIB_FAXFIFO_H */

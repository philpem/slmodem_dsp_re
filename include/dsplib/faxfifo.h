/*
 * faxfifo.h -- Class 1 fax: the FIFO of 16-bit elements.
 *
 * `FIFO_read`, `FIFO_write`, `FIFO_delete`, `FIFO_full_test` and now
 * `FIFO_create` (.text 0x096bb0, 167 bytes) are all reconstructed.
 *
 * `FIFO_create` WAS DECLINED TWICE (F9020, F9199) FOR ITS DEFAULT TABLE,
 * `FIFO_CFG`.  The blob defines that name TWICE at DIFFERENT VALUES -- a
 * file-local `d` at .data:0x83a0 holding {0, 300, 0}, and a global `R` at
 * .rodata:0x9654 holding {0, 100, 0} -- so `symmap.py` gives it no `ref_`
 * alias by name (F9058) and a naive `src/` definition looked like a multiple
 * definition of a symbol the blob already has (F9199's second reason).
 *
 * BOTH OF THOSE ARE SETTLED NOW, MEASURED RATHER THAN TAKEN ON TRUST
 * (F9500).  `V21TX_create`'s relocation at 0x099375 NAMES `FIFO_CFG`, and per
 * CLAUDE.md a relocation that names a symbol resolves to the GLOBAL
 * definition -- so `FIFO_create`'s own two loads at 0x096c1b/0x096c22 (also
 * named relocations) read the same global, and the six bytes below are
 * `00 00 64 00 00 00`, the 100-element copy.  And the "multiple definition"
 * fear does not survive checking `build/dsplibs_ref.o`: `tools/symmap.py`'s
 * redefine map renames EVERY symtab entry called `FIFO_CFG`, local and
 * global alike (`nm` shows both `d` and `R` `ref_FIFO_CFG` after the rename),
 * so nothing in the renamed blob is still named plain `FIFO_CFG` and `src/`
 * is free to define it.  An external reference to `ref_FIFO_CFG` binds to the
 * GLOBAL entry only -- local symbols never satisfy another translation
 * unit's undefined reference -- which is what makes the differential test
 * below able to tell 100 from 300 at all.
 *
 * WHAT `FIFO_create` ESTABLISHES.  It allocates `sysdep_malloc(0x14)` for the
 * object and `sysdep_malloc(size * 2)` for the buffer, so the object is 20
 * bytes and the buffer holds `size` SIXTEEN-BIT elements -- not bytes.  With
 * a NULL `cfg` it reads `FIFO_CFG` directly (0x096c1b/0x096c22); otherwise it
 * copies six bytes out of the caller's configuration as one 32-bit store to
 * +0x00 and one 16-bit store to +0x04, which is why +0x00 and +0x02 are one
 * aligned pair; then it zeroes +0x0c, +0x0e and +0x10 and clears the whole
 * buffer with LITERAL ZERO, not `fill` -- `movw $0x0,(%ecx,%edx,2)` at
 * 0x096c00, not a re-read of the fill field.  `fill` is what `FIFO_read`
 * pads a shortfall with once the FIFO runs dry; it plays no part in the
 * buffer's initial contents.
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

/*
 * `FIFO_create`'s configuration argument: the same six bytes as `fax_fifo`'s
 * own +0x00/+0x02/+0x04, but a distinct type -- the created object has ten
 * more bytes (the buffer pointer and three cursors) that a caller supplying
 * a default has no business naming.
 */
struct fifo_cfg {
	short word0;	/* +0x00, copied to fax_fifo's short_000            */
	short size;	/* +0x02, the FIFO's capacity in ELEMENTS            */
	short fill;	/* +0x04, what FIFO_read pads a shortfall with       */
};

/*
 * The object's own `FIFO_CFG`, `R` at .rodata:0x9654, 6 bytes: {0, 100, 0}.
 * See the header note above for why this is the global copy and not the
 * unrelated local one at .data:0x83a0.
 */
extern const struct fifo_cfg FIFO_CFG;

/*
 * Build a FIFO, or re-initialise one the caller already has.
 *
 * `f` NULL allocates a `sizeof(struct fax_fifo)` (0x14-byte) object and, with
 * it, a `size * 2`-byte buffer; a non-NULL one is re-initialised IN PLACE,
 * replacing its configuration and clearing its buffer without reallocating
 * it -- there is no check that the existing buffer is even big enough for
 * the new `size`, which is the object's own contract and not guarded here
 * either.
 *
 * `cfg` NULL takes `FIFO_CFG`.  There is no failure return: the object does
 * not check `sysdep_malloc`.
 */
struct fax_fifo *FIFO_create(struct fax_fifo *f, const struct fifo_cfg *cfg);

#endif /* DSPLIB_FAXFIFO_H */

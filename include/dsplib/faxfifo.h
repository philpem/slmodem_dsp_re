/**
 * @file faxfifo.h
 * @brief Class 1 fax: the FIFO of 16-bit elements.
 *
 * `FIFO_read`, `FIFO_write`, `FIFO_delete`, `FIFO_full_test` and
 * `FIFO_create` (`.text` 0x096bb0, 167 bytes) are all reconstructed.
 *
 * `FIFO_create` was declined twice (F9020, F9199) over its default table,
 * `FIFO_CFG`: the blob defines that name twice at different values -- a
 * file-local `d` at `.data:0x83a0` holding `{0, 300, 0}`, and a global `R`
 * at `.rodata:0x9654` holding `{0, 100, 0}` -- so `symmap.py` gave it no
 * `ref_` alias by name (F9058) and a naive `src/` definition looked like a
 * multiple definition of a symbol the blob already has (F9199's second
 * reason).
 *
 * Both of those are settled now, measured rather than taken on trust
 * (F9500). `V21TX_create`'s relocation at 0x099375 names `FIFO_CFG`, and
 * per CLAUDE.md a relocation that names a symbol resolves to the global
 * definition -- so `FIFO_create`'s own two loads at 0x096c1b/0x096c22
 * (also named relocations) read the same global, and the six bytes there
 * are `00 00 64 00 00 00`, the 100-element copy. The "multiple
 * definition" fear does not survive checking `build/dsplibs_ref.o`
 * either: `tools/symmap.py`'s redefine map renames every symtab entry
 * called `FIFO_CFG`, local and global alike (`nm` shows both `d` and `R`
 * `ref_FIFO_CFG` after the rename), so nothing in the renamed blob is
 * still named plain `FIFO_CFG` and `src/` is free to define it. An
 * external reference to `ref_FIFO_CFG` binds to the global entry only --
 * local symbols never satisfy another translation unit's undefined
 * reference -- which is what lets the differential test tell 100 from
 * 300 at all.
 *
 * What `FIFO_create` establishes: it allocates `sysdep_malloc(0x14)` for
 * the object and `sysdep_malloc(size * 2)` for the buffer, so the object
 * is 20 bytes and the buffer holds `size` sixteen-bit elements, not
 * bytes. With a NULL `cfg` it reads `FIFO_CFG` directly (0x096c1b/
 * 0x096c22); otherwise it copies six bytes out of the caller's
 * configuration as one 32-bit store to +0x00 and one 16-bit store to
 * +0x04, which is why +0x00 and +0x02 are one aligned pair; then it
 * zeroes +0x0c, +0x0e and +0x10 and clears the whole buffer with a
 * literal zero, not `fill` -- `movw $0x0,(%ecx,%edx,2)` at 0x096c00, not
 * a re-read of the fill field. `fill` is what `FIFO_read` pads a
 * shortfall with once the FIFO runs dry; it plays no part in the
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
	unsigned short *buf;		/* +0x08 `size` elements.  The 2-byte
					 * gap `fill` leaves ahead of this
					 * 4-byte-aligned pointer used to be a
					 * named `pad_06[2]`; removed by the
					 * pad-region removal audit (F10145)
					 * -- nothing reconstructed here ever
					 * read or wrote it, and the compiler's
					 * own alignment reproduces the gap
					 * exactly (FAXFIFO_ASSERT_OFF below) */
	unsigned short count;		/* +0x0c elements held              */
	unsigned short rd;		/* +0x0e read cursor, in elements   */
	unsigned short wr;		/* +0x10 write cursor, in elements  */
};
/*
 * The trailing `pad_12[2]` is gone the same way (F10145): `wr` ends at
 * +0x12 and the struct's own alignment (4 bytes, forced by `buf`) rounds
 * `sizeof` up to +0x14 whether or not a member fills the tail, matching
 * `FIFO_create`'s own `sysdep_malloc(0x14)` for the object -- the
 * `fax_fifo_size` assertion below is what proves it stayed 0x14.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define FAXFIFO_ASSERT_OFF(field, off) \
	typedef char fax_fifo_off_##field[ \
		((int)__builtin_offsetof(struct fax_fifo, field) \
			== (off)) ? 1 : -1]
FAXFIFO_ASSERT_OFF(buf, 0x08);
FAXFIFO_ASSERT_OFF(count, 0x0c);
FAXFIFO_ASSERT_OFF(rd, 0x0e);
FAXFIFO_ASSERT_OFF(wr, 0x10);
typedef char fax_fifo_size[(sizeof(struct fax_fifo) == 0x14) ? 1 : -1];
#endif

/**
 * Occupancy threshold in Q14: 14747 / 16384 is 0.90008..., so a
 * #FIFO_full_test result at or above this means "at least 90% full".
 */
#define FIFO_FULL_Q14	0x399b

/**
 * @brief Report whether a FIFO is at least 90% full.
 *
 * As written, this is `count / size >= 90%`. As compiled it is not
 * quite: the arithmetic is a 16-bit chain -- `count << 14` is truncated
 * to a `short` before the divide, so a count of 2 lands on -32768 and,
 * for any positive `size`, everything above `count == 1` answers "not
 * full" through a negative quotient; with a positive `size` the only
 * input that can answer 1 is `count == 1` with `size == 1`. Reproduced,
 * not repaired -- see `docs/deviations.md` D952. `size` is read signed,
 * and a zero `size` divides by zero.
 *
 * @param f  The FIFO to test.
 * @return 1 if (as computed above) at least 90% full, 0 otherwise.
 */
int FIFO_full_test(struct fax_fifo *f);

/**
 * @brief Take up to `count` elements out of a FIFO.
 *
 * The shortfall is not left alone: the balance of `count` is filled with
 * `f->fill`, so `dst` is always written `count` times. That makes
 * `count` the destination's size, not a request -- a caller sizing
 * `dst` from the FIFO's occupancy overruns it (this is D956's shape, so
 * it is called out here rather than rediscovered).
 *
 * Every cursor step is 16-bit: `rd` advances and wraps against `size`
 * read as an unsigned short, while #FIFO_full_test reads the same field
 * signed. Both readings are the object's own; see D1050.
 *
 * @param f      The FIFO to read from.
 * @param dst    Destination buffer, exactly `count` elements.
 * @param count  Elements to write to `dst`.
 * @return How many of those elements were real (the rest were `f->fill`).
 */
int FIFO_read(struct fax_fifo *f, unsigned short *dst, unsigned short count);

/**
 * @brief Append elements to a FIFO, clamped to the free space.
 *
 * The free space is `(unsigned short)(size - count)`, so a FIFO already
 * holding more than `size` wraps to a large free count rather than to
 * zero -- reproduced, D1051.
 *
 * @param f      The FIFO to write to.
 * @param src    Source elements.
 * @param count  How many elements are offered.
 * @return How many elements were actually taken.
 */
int FIFO_write(struct fax_fifo *f, unsigned short *src, unsigned short count);

/**
 * @brief Free a FIFO's buffer, then the FIFO itself.
 *
 * The second free is a tail call.
 *
 * @param f  The FIFO to free.
 */
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

/**
 * @brief Build a FIFO, or re-initialise one the caller already has.
 *
 * `f` NULL allocates a `sizeof(struct fax_fifo)` (0x14-byte) object and,
 * with it, a `size * 2`-byte buffer; a non-NULL one is re-initialised in
 * place, replacing its configuration and clearing its buffer without
 * reallocating it -- there is no check that the existing buffer is even
 * big enough for the new `size`, which is the object's own contract and
 * not guarded here either.
 *
 * `cfg` NULL takes #FIFO_CFG. There is no failure return: the object
 * does not check `sysdep_malloc`.
 *
 * @param f    An existing FIFO to reinitialise, or NULL to allocate one.
 * @param cfg  Configuration to apply, or NULL for #FIFO_CFG.
 * @return The FIFO (`f`, or the newly allocated one).
 */
struct fax_fifo *FIFO_create(struct fax_fifo *f, const struct fifo_cfg *cfg);

#endif /* DSPLIB_FAXFIFO_H */

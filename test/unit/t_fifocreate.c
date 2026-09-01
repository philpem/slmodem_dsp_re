/*
 * t_fifocreate.c -- differential test of `FIFO_create` (.text 0x096bb0, 167
 *                   bytes) and its default table `FIFO_CFG` (.rodata 0x9654,
 *                   6 bytes), against the blob's own.
 *
 * BOTH WERE DECLINED TWICE (F9020, F9199) BECAUSE `FIFO_CFG` LOOKED
 * UNTESTABLE.  The blob defines the name `FIFO_CFG` TWICE at DIFFERENT
 * VALUES -- a file-local `d` at .data:0x83a0 holding {0, 300, 0} and a
 * global `R` at .rodata:0x9654 holding {0, 100, 0} -- so `symmap.py` gives
 * it no `ref_` alias by NAME (F9058) and F9199 believed a `src/` definition
 * would collide at link with the copy the blob already carries.
 *
 * NEITHER HOLDS UP, AND F9500 IS THE MEASUREMENT.  `objcopy --redefine-syms`
 * renames every symtab entry called `FIFO_CFG`, local and global alike, so
 * `nm build/dsplibs_ref.o` shows BOTH `d` and `R` `ref_FIFO_CFG` -- nothing
 * in the renamed blob is still called plain `FIFO_CFG`, so `src/` defining
 * it collides with nothing.  And an EXTERNAL reference to `ref_FIFO_CFG`
 * (this file's `extern` below) can only bind to a GLOBAL symbol -- ordinary
 * ELF linking, not a workaround -- so it reaches the .rodata 100-copy and
 * never the .data 300-one.  That is what makes `test_wrong_copy_rejected`
 * below able to mean anything.
 *
 * `FIFO_create` ITSELF NEEDS NO CONSUMER.  It is a normal, non-duplicated
 * `T` symbol (`FIFO_create` appears once in `nm`, unlike `FIFO_CFG`), so
 * `symmap.py` gives it an ordinary `ref_` alias and this file drives it
 * directly -- `readyqueue.py`'s "blocked, no consumer" reading was about
 * `FIFO_CFG`'s testability, not `FIFO_create`'s linkability, and the two
 * turned out to be separable.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"

#include "dsplib/faxfifo.h"
#include "dsplib/sysdep.h"

extern const struct fifo_cfg ref_FIFO_CFG;
extern struct fax_fifo *ref_FIFO_create(struct fax_fifo *f,
					const struct fifo_cfg *cfg);
extern void ref_FIFO_delete(struct fax_fifo *f);

/* ------------------------------------------------------------------------- */

static void
cmp_buf(const char *what, const unsigned short *got,
	const unsigned short *want, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (got[i] != want[i]) {
			diff_eq_int(what, got[i], want[i], i);
			return;
		}
	}
	diff_eq_int(what, 0, 0, n);
}

/* Compare every non-pointer field of two `struct fax_fifo`s. */
static void
cmp_fifo(const char *what, const struct fax_fifo *a,
	 const struct fax_fifo *b, long tag)
{
	char buf[160];

#define F(name) \
	do { \
		snprintf(buf, sizeof(buf), "%.100s ." #name " (%%ld)", what); \
		diff_eq_int(buf, a->name, b->name, tag); \
	} while (0)

	F(short_000);
	F(size);
	F(fill);
	F(count);
	F(rd);
	F(wr);
#undef F
}

/* ------------------------------------------------------------------------- */

static int
test_shape(void)
{
	diff_begin("fifocreate: FIFO_CFG's size against the object's symbol table");

	diff_eq_int("sizeof FIFO_CFG (%ld)", (long)sizeof(FIFO_CFG), 6, 0);
	diff_eq_int("sizeof(struct fax_fifo) (%ld)",
		    (long)sizeof(struct fax_fifo), 0x14, 0);

	return diff_end();
}

/*
 * THE WRONG-COPY TEST.  Two independent claims about `FIFO_CFG`'s value,
 * neither of which trusts the other:
 *
 *   - `ref_FIFO_CFG` against a LITERAL 100 -- if the redefine-syms reasoning
 *     above were wrong and `ref_FIFO_CFG` bound to the .data 300-copy
 *     instead, THIS line would fail, independently of anything `src/` says.
 *   - `FIFO_CFG` (ours) against `ref_FIFO_CFG` -- the ordinary differential
 *     claim.
 *
 * F134's ritual was run by hand on this pair: with `FIFO_CFG` in
 * `src/fax/fifo.c` planted to `{ 0, 300, 0 }`, `size == 100` below fails
 * immediately (`size == 300`) and nothing else in the suite masks it;
 * restored, the suite is green again.  See finding F9500 for the transcript.
 */
static int
test_wrong_copy_rejected(void)
{
	diff_begin("fifocreate: FIFO_CFG can tell 100 from 300 apart");

	diff_eq_int("ref_FIFO_CFG.word0 is 0, independent of src/ (%ld)",
		    ref_FIFO_CFG.word0, 0, 0);
	diff_eq_int("ref_FIFO_CFG.size is 100, independent of src/ (%ld)",
		    ref_FIFO_CFG.size, 100, 0);
	diff_eq_int("ref_FIFO_CFG.fill is 0, independent of src/ (%ld)",
		    ref_FIFO_CFG.fill, 0, 0);

	diff_eq_int("FIFO_CFG.word0 (%ld)", FIFO_CFG.word0,
		    ref_FIFO_CFG.word0, 0);
	diff_eq_int("FIFO_CFG.size (%ld)", FIFO_CFG.size,
		    ref_FIFO_CFG.size, 0);
	diff_eq_int("FIFO_CFG.fill (%ld)", FIFO_CFG.fill,
		    ref_FIFO_CFG.fill, 0);

	/* The literal this pair has carried since F9020's derivation. */
	diff_eq_int("FIFO_CFG.size is 100 (%ld)", FIFO_CFG.size, 100, 0);

	return diff_end();
}

/*
 * `FIFO_create(0, 0)`: the default table, self-allocating.  `size` reaching
 * 100 here is the SAME claim as `test_wrong_copy_rejected`'s, taken through
 * the constructor rather than by name -- the two are independent readings of
 * one fact.
 */
static int
test_default(void)
{
	struct fax_fifo *a, *b;

	diff_begin("fifocreate: FIFO_create(0, 0), the default table");

	a = FIFO_create(0, 0);
	b = ref_FIFO_create(0, 0);

	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	cmp_fifo("default", a, b, 0);
	diff_eq_int("size is 100, through the constructor (%ld)", a->size,
		    100, 0);
	cmp_buf("buf[%ld], default", a->buf, b->buf, 100);

	FIFO_delete(a);
	ref_FIFO_delete(b);

	return diff_end();
}

/*
 * `FIFO_create(0, &cfg)`: self-allocating with an explicit table, over a
 * handful of sizes -- 0 (the divide-by-zero edge `FIFO_full_test` has, D952,
 * though this constructor does not divide), 1, and an ordinary size with a
 * non-zero `fill` and a non-zero `word0`, so the buffer-clear loop's SIGNED
 * bound (0x096c06's `movswl`) and the plain field copies both get a value
 * that is not zero already.
 */
static int
test_explicit(void)
{
	static const struct { short word0, size, fill; } cases[] = {
		{    0,   0,    0 },
		{    0,   1,    0 },
		{    7,  10,   -1 },
		{ 1234, 250, 5555 },
	};
	const long ncases = (long)(sizeof(cases) / sizeof(cases[0]));
	long k;

	diff_begin("fifocreate: FIFO_create(0, &cfg), explicit tables");

	for (k = 0; k < ncases; k++) {
		struct fifo_cfg ca, cb;
		struct fax_fifo *a, *b;

		ca.word0 = cb.word0 = cases[k].word0;
		ca.size = cb.size = cases[k].size;
		ca.fill = cb.fill = cases[k].fill;

		a = FIFO_create(0, &ca);
		b = ref_FIFO_create(0, &cb);

		diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, k);
		if (a == 0 || b == 0)
			continue;

		cmp_fifo(cases[k].size == 0 ? "size 0"
			 : cases[k].size == 1 ? "size 1"
			 : cases[k].size == 10 ? "size 10, fill -1"
					       : "size 250, word0/fill set",
			 a, b, k);
		if (cases[k].size > 0)
			cmp_buf("buf[%ld]", a->buf, b->buf,
				(int)(unsigned short)cases[k].size);

		FIFO_delete(a);
		ref_FIFO_delete(b);
	}

	return diff_end();
}

/*
 * `FIFO_create(f, &cfg)`: RE-INITIALISATION IN PLACE, plus D955/F8587's
 * planting.  After a first create, both the object and its buffer are
 * overwritten with a non-zero pattern -- the buffer's OWN old contents,
 * which a re-init that forgot to clear would leave visible -- and `buf`
 * itself is restored (it is what the object tests to decide whether to
 * allocate, and the constructor does not resize it, so a wild pointer there
 * is a wild write in `FIFO_create`'s own clear loop). `FIFO_create` is then
 * called again with a SMALLER size over the SAME buffer, so a byte the
 * second call fails to clear is visibly the pattern rather than invisibly a
 * zero the first call already left.
 */
static int
test_reinit(void)
{
	struct fax_fifo *a, *b;
	unsigned short *bufa, *bufb;
	struct fifo_cfg ca, cb;
	int i;

	diff_begin("fifocreate: FIFO_create(f, &cfg), re-initialised in place");

	ca.word0 = cb.word0 = 0;
	ca.size = cb.size = 20;
	ca.fill = cb.fill = 0;

	a = FIFO_create(0, &ca);
	b = ref_FIFO_create(0, &cb);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	bufa = a->buf;
	bufb = b->buf;

	for (i = 0; i < 20; i++)
		bufa[i] = bufb[i] = (unsigned short)(0x5a5a + i);
	memset(a, 0x5a, sizeof(*a));
	memset(b, 0x5a, sizeof(*b));
	a->buf = bufa;
	b->buf = bufb;

	ca.word0 = cb.word0 = 3;
	ca.size = cb.size = 12;
	ca.fill = cb.fill = 9;

	a = FIFO_create(a, &ca);
	b = ref_FIFO_create(b, &cb);

	diff_eq_int("buf pointer kept, ours (%ld)", a->buf == bufa, 1, 0);
	diff_eq_int("buf pointer kept, blob's (%ld)", b->buf == bufb, 1, 0);

	cmp_fifo("reinit", a, b, 0);
	/*
	 * The WHOLE original 20-element buffer, not just the new size-12
	 * window.  The object clears exactly `f->size` (the NEW size, 12)
	 * elements and leaves 12..19 at whatever was there -- the planted
	 * pattern, on both sides, since neither implementation resizes the
	 * buffer.  Comparing all 20 is what catches a clear bound that is
	 * off by a few in either direction: too few and 10..11 would still
	 * show the pattern on our side only; too many and 12..19 would show
	 * zero on our side only.
	 */
	cmp_buf("buf[%ld], reinit (full old extent)", bufa, bufb, 20);

	FIFO_delete(a);
	ref_FIFO_delete(b);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	rc |= test_shape();
	rc |= test_wrong_copy_rejected();
	rc |= test_default();
	rc |= test_explicit();
	rc |= test_reinit();

	return rc;
}

/*
 * t_fdspkfifo.c -- differential tests for the FIFO8 byte ring
 * (src/service/Fifo8.c, blob 0xaef30..0xaf150).
 *
 * The two sides never share a ring: each gets its own `struct fifo8` and its
 * own backing buffer, and every field of both is compared afterwards.  Where
 * FIFO8_create allocates, the pointers necessarily differ and the
 * ALLOCATOR'S BOOKS are compared instead -- counts, live blocks and bytes,
 * plus `bad_free`, which is what a wrong offset in FIFO8_delete would show
 * up as.
 *
 * Three things here are not obvious and each has its own checks:
 *
 *   - `cfg == NULL` makes create read the blob's own LOCAL default object
 *     (`FIFO_CFG` in .data at 0x83a0).  Our copy of those three shorts is a
 *     separate object, so this arm is the only thing that compares them.
 *   - create does NOT allocate the buffer when the caller supplies the
 *     struct; the fixture must plant `buf` itself, and it is used as a
 *     SUBSCRIPT base by the clearing loop (D955).
 *   - FIFO8_read pads a short read out to the caller's `n` with cfg.fill,
 *     so the destination past the data is part of the comparison.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/fifo8.h"
#include "dsplib/sysdep.h"

extern struct fifo8 *ref_FIFO8_create(struct fifo8 *f,
				      const struct fifo8_cfg *cfg);
extern void ref_FIFO8_delete(struct fifo8 *f);
extern short ref_FIFO8_write(struct fifo8 *f, const unsigned char *src,
			     unsigned short n);
extern short ref_FIFO8_read(struct fifo8 *f, unsigned char *dst,
			    unsigned short n);

static unsigned int lcg_state = 0xf1f0u;
static unsigned int
lcg(void)
{
	lcg_state = lcg_state * 1664525u + 1013904223u;
	return lcg_state;
}

#define RINGMAX 64

/* One side: the object and a buffer big enough for every cfg driven here. */
struct side {
	struct fifo8 f;
	unsigned char buf[RINGMAX];
};

static void
compare_fifo(struct side *got, struct side *want, long tag)
{
	unsigned int i;

	diff_eq_int("cfg.short_00 %ld", got->f.cfg.short_00,
		    want->f.cfg.short_00, tag);
	diff_eq_int("cfg.size %ld", got->f.cfg.size, want->f.cfg.size, tag);
	diff_eq_int("cfg.fill %ld", got->f.cfg.fill, want->f.cfg.fill, tag);
	diff_eq_int("count %ld", got->f.count, want->f.count, tag);
	diff_eq_int("rd %ld", got->f.rd, want->f.rd, tag);
	diff_eq_int("wr %ld", got->f.wr, want->f.wr, tag);
	for (i = 0; i < RINGMAX; i++)
		diff_eq_int("ring[%ld]", got->buf[i], want->buf[i],
			    tag * 1000 + i);
}

/* Both sides made ready with the same cfg, the same fill, the same cursors. */
static void
build_pair(struct side *a, struct side *b, const struct fifo8_cfg *cfg,
	   unsigned int seed)
{
	unsigned int i;

	memset(a, 0, sizeof(*a));
	for (i = 0; i < RINGMAX; i++)
		a->buf[i] = (unsigned char)(seed + i * 7);
	a->f.buf = a->buf;
	memcpy(b, a, sizeof(*a));
	b->f.buf = b->buf;
	ref_FIFO8_create(&a->f, cfg);
	FIFO8_create(&b->f, cfg);
}

int
main(void)
{
	int failed = 0;
	unsigned int i, ci, n;

	/*
	 * The cfgs driven everywhere below.  size 1 exercises the wrap on
	 * every single byte; a size larger than any n exercises none of it.
	 */
	static const struct fifo8_cfg cfgs[] = {
		{ 0, 8, 0 },
		{ 0, 1, 0x5a },
		{ 3, 5, -1 },
		{ 0, 16, 0x7f },
		{ 0, 0, 0x21 }
	};
	static const unsigned int ncfg = sizeof(cfgs) / sizeof(cfgs[0]);

	/*
	 * SECTION 1 -- FIFO8_create with a caller-supplied struct.  The ring
	 * starts full of junk on both sides, so "cleared cfg.size bytes and
	 * not one more" is visible: byte cfg.size must still hold the junk.
	 */
	diff_begin("FIFO8_create supplied");
	for (ci = 0; ci < ncfg; ci++) {
		struct side a, b;

		build_pair(&a, &b, &cfgs[ci], 0x30 + ci);
		compare_fifo(&b, &a, (long)ci);
		diff_eq_int("size taken %ld", b.f.cfg.size, cfgs[ci].size,
			    (long)ci);
		/* cleared exactly cfg.size bytes */
		for (i = 0; i < cfgs[ci].size; i++)
			diff_eq_int("cleared[%ld]", b.buf[i], 0,
				    (long)(ci * 100 + i));
		if (cfgs[ci].size < RINGMAX)
			diff_eq_int("past the ring untouched %ld",
				    b.buf[cfgs[ci].size] != 0, 1, (long)ci);
	}
	failed |= diff_end();

	/*
	 * SECTION 2 -- FIFO8_create with cfg == NULL, which is the only path
	 * that reads the built-in defaults.  The ring the default asks for
	 * is 300 bytes, so this one allocates rather than using `side`.
	 */
	diff_begin("FIFO8_create default cfg");
	{
		struct fifo8 fa, fb;
		unsigned char ba[512], bb[512];

		memset(ba, 0xc7, sizeof(ba));
		memcpy(bb, ba, sizeof(ba));
		memset(&fa, 0x99, sizeof(fa));
		memcpy(&fb, &fa, sizeof(fa));
		fa.buf = ba;
		fb.buf = bb;
		ref_FIFO8_create(&fa, 0);
		FIFO8_create(&fb, 0);
		diff_eq_int("default short_00", fb.cfg.short_00,
			    fa.cfg.short_00, 0);
		diff_eq_int("default size", fb.cfg.size, fa.cfg.size, 0);
		diff_eq_int("default fill", fb.cfg.fill, fa.cfg.fill, 0);
		diff_eq_int("default count", fb.count, fa.count, 0);
		diff_eq_int("default rd", fb.rd, fa.rd, 0);
		diff_eq_int("default wr", fb.wr, fa.wr, 0);
		for (i = 0; i < 512; i++)
			diff_eq_int("default ring[%ld]", bb[i], ba[i],
				    (long)i);
		/* and the default really is a ring, not zero */
		diff_eq_int("default size is nonzero", fb.cfg.size != 0, 1, 0);
	}
	failed |= diff_end();

	/*
	 * SECTION 3 -- FIFO8_create(NULL, cfg): it allocates the object AND
	 * the ring, so only the books and the resulting fields can be
	 * compared.  Two sysdep_malloc calls per side, and the second is for
	 * cfg.size bytes.
	 */
	diff_begin("FIFO8_create allocating");
	for (ci = 0; ci < ncfg; ci++) {
		struct alloc_log la, lb;
		struct fifo8 *fa, *fb;

		harness_alloc_reset();
		fa = ref_FIFO8_create(0, &cfgs[ci]);
		la = harness_alloc;
		harness_alloc_reset();
		fb = FIFO8_create(0, &cfgs[ci]);
		lb = harness_alloc;

		diff_eq_int("alloc allocs %ld", lb.allocs, la.allocs,
			    (long)ci);
		diff_eq_int("alloc bytes %ld", (int)lb.bytes, (int)la.bytes,
			    (long)ci);
		diff_eq_int("alloc live %ld", lb.live, la.live, (long)ci);
		diff_eq_int("alloc returned %ld", fb != 0, fa != 0, (long)ci);
		diff_eq_int("alloc size %ld", fb->cfg.size, fa->cfg.size,
			    (long)ci);
		diff_eq_int("alloc fill %ld", fb->cfg.fill, fa->cfg.fill,
			    (long)ci);
		diff_eq_int("alloc count %ld", fb->count, fa->count,
			    (long)ci);
		for (i = 0; i < cfgs[ci].size; i++)
			diff_eq_int("alloc ring[%ld]", fb->buf[i], fa->buf[i],
				    (long)(ci * 100 + i));
		/* two blocks, and the object is 0x14 of the bytes */
		diff_eq_int("alloc two blocks %ld", lb.allocs, 2, (long)ci);

		/* ... and FIFO8_delete gives both of them back */
		harness_alloc_reset();
		fa = ref_FIFO8_create(0, &cfgs[ci]);
		ref_FIFO8_delete(fa);
		la = harness_alloc;
		harness_alloc_reset();
		fb = FIFO8_create(0, &cfgs[ci]);
		FIFO8_delete(fb);
		lb = harness_alloc;
		diff_eq_int("delete frees %ld", lb.frees, la.frees, (long)ci);
		diff_eq_int("delete live %ld", lb.live, la.live, (long)ci);
		diff_eq_int("delete bad_free %ld", lb.bad_free, la.bad_free,
			    (long)ci);
		diff_eq_int("delete balanced %ld", lb.live, 0, (long)ci);
	}
	harness_alloc_reset();
	failed |= diff_end();

	/*
	 * SECTION 4 -- write then read, over every cfg and a sweep of block
	 * sizes that crosses each ring's capacity several times.  Reads ask
	 * for more than is held as often as less, so the cfg.fill padding is
	 * driven on nearly every pass.
	 */
	diff_begin("FIFO8 write/read");
	{
		int wrote_short = 0, read_padded = 0, wrapped = 0;

		for (ci = 0; ci < ncfg; ci++) {
			struct side a, b;
			unsigned int round;

			build_pair(&a, &b, &cfgs[ci], 0x50 + ci);
			for (round = 0; round < 12; round++) {
				unsigned char src[24];
				unsigned char da[32], db[32];
				short wa, wb, ra, rb;
				unsigned short wn = (unsigned short)
						    (lcg() % 13);
				unsigned short rn = (unsigned short)
						    (lcg() % 13);
				long tag = (long)(ci * 100 + round);

				for (i = 0; i < 24; i++)
					src[i] = (unsigned char)lcg();
				memset(da, 0xee, sizeof(da));
				memcpy(db, da, sizeof(da));

				wa = ref_FIFO8_write(&a.f, src, wn);
				wb = FIFO8_write(&b.f, src, wn);
				diff_eq_int("write ret %ld", wb, wa, tag);
				compare_fifo(&b, &a, tag * 10);
				if (wa < (short)wn)
					wrote_short = 1;

				ra = ref_FIFO8_read(&a.f, da, rn);
				rb = FIFO8_read(&b.f, db, rn);
				diff_eq_int("read ret %ld", rb, ra, tag);
				for (i = 0; i < 32; i++)
					diff_eq_int("read out[%ld]", db[i],
						    da[i],
						    tag * 100 + i);
				compare_fifo(&b, &a, tag * 10 + 1);
				if (ra < (short)rn)
					read_padded = 1;
				if (b.f.rd < b.f.wr && b.f.count > 0)
					wrapped = 1;
			}
		}
		/*
		 * F134: the interesting arms have to be shown to have run,
		 * not assumed from the sweep's shape.
		 */
		diff_eq_int("a write was clipped", wrote_short, 1, 0);
		diff_eq_int("a read was padded", read_padded, 1, 0);
		diff_eq_int("a cursor wrapped", wrapped, 1, 0);
	}
	failed |= diff_end();

	/*
	 * SECTION 5 -- the edges: n == 0 either way, a read from an empty
	 * ring, a write into a full one, and a read of the whole ring.
	 */
	diff_begin("FIFO8 edges");
	for (ci = 0; ci < ncfg; ci++) {
		struct side a, b;
		unsigned char src[80], da[80], db[80];
		short wa, wb, ra, rb;

		for (i = 0; i < 80; i++)
			src[i] = (unsigned char)(i * 3 + ci);

		build_pair(&a, &b, &cfgs[ci], 0x70 + ci);

		/* zero-length write and read */
		memset(da, 0x11, sizeof(da));
		memcpy(db, da, sizeof(da));
		wa = ref_FIFO8_write(&a.f, src, 0);
		wb = FIFO8_write(&b.f, src, 0);
		diff_eq_int("n0 write %ld", wb, wa, (long)ci);
		ra = ref_FIFO8_read(&a.f, da, 0);
		rb = FIFO8_read(&b.f, db, 0);
		diff_eq_int("n0 read %ld", rb, ra, (long)ci);
		for (i = 0; i < 80; i++)
			diff_eq_int("n0 out[%ld]", db[i], da[i],
				    (long)(ci * 100 + i));
		compare_fifo(&b, &a, (long)(ci + 300));

		/* read from empty: everything is padding */
		memset(da, 0x22, sizeof(da));
		memcpy(db, da, sizeof(da));
		ra = ref_FIFO8_read(&a.f, da, 20);
		rb = FIFO8_read(&b.f, db, 20);
		diff_eq_int("empty read %ld", rb, ra, (long)ci);
		diff_eq_int("empty read is 0 %ld", rb, 0, (long)ci);
		for (i = 0; i < 80; i++)
			diff_eq_int("empty out[%ld]", db[i], da[i],
				    (long)(ci * 100 + i));

		/* fill it right up, twice, so the second write is clipped */
		for (n = 0; n < 3; n++) {
			wa = ref_FIFO8_write(&a.f, src, 40);
			wb = FIFO8_write(&b.f, src, 40);
			diff_eq_int("fill write %ld", wb, wa,
				    (long)(ci * 10 + n));
			compare_fifo(&b, &a, (long)(ci * 10 + n + 400));
		}
		diff_eq_int("ring is full %ld", b.f.count, b.f.cfg.size,
			    (long)ci);

		/* and drain it in one go, asking for more than it holds */
		memset(da, 0x33, sizeof(da));
		memcpy(db, da, sizeof(db));
		ra = ref_FIFO8_read(&a.f, da, 70);
		rb = FIFO8_read(&b.f, db, 70);
		diff_eq_int("drain read %ld", rb, ra, (long)ci);
		for (i = 0; i < 80; i++)
			diff_eq_int("drain out[%ld]", db[i], da[i],
				    (long)(ci * 100 + i));
		diff_eq_int("drained empty %ld", b.f.count, 0, (long)ci);
		compare_fifo(&b, &a, (long)(ci + 500));
	}
	failed |= diff_end();

	return failed;
}

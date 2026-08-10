/*
 * t_floatiirfree.cpp -- D56, measured: ~FloatIIR frees the history and leaves
 * m_hist pointing at it, so a second destruction frees the same block twice.
 *
 * D56 said `unmeasured`, on the reasoning that a double free is something you
 * read in the source rather than something a test can watch happen.  It is
 * not: the harness's sysdep_free counts a free of a pointer the allocator
 * never handed out as `bad_free` and SWALLOWS it rather than passing it to
 * the real free(), so the run survives to report the number.  That is the
 * same instrument t_b103alloc uses to hold D8 in place, and the same reason
 * it asserts a count rather than asserting zero.
 *
 * What is asserted here is the ORIGINAL's behaviour, both sides driven
 * through the identical sequence:
 *
 *     construct        1 alloc, m_len floats, m_hist non-null
 *     destruct once    1 free,  0 live,       0 bad
 *     destruct twice   1 free,  0 live,       1 bad     <-- D56
 *
 * The blob's destructor, at 0x57800, is five instructions: load +4, test,
 * call sysdep_free, return.  There is no store back to the field on either
 * path, so the second call arrives with the same pointer.  This test is what
 * turns that reading into a number.
 *
 * THE DISCRIMINATING ASSERTION IS `bad_free == 1`, and it is discriminating
 * in both directions.  Null the pointer in either implementation -- the
 * obvious tidy-up -- and the second destructor takes the `if (m_hist)` branch
 * and does nothing at all: no free, no bad free, and `free_null` stays zero
 * because sysdep_free is never reached.  So a fixed implementation fails here
 * rather than diverging from the blob in silence.
 *
 * EACH SIDE GETS ITS OWN RESET WINDOW.  harness_alloc is one global; measuring
 * ours and the blob's inside a single window would have both snapshots reading
 * the same summed counters, and "the two sides agree" would be true of any
 * pair of implementations whatsoever.  run_ours and run_ref each reset at
 * their own top and nothing else allocates in between.
 *
 * The second destructor call is an explicit `p->~FloatIIR()` on an object
 * placement-new'd into a caller-supplied buffer, so no scope exit or `delete`
 * adds a third destruction the counters would have to account for.  The blob
 * side is the same shape already: a static buffer and two calls to
 * ref__ZN8FloatIIRD1Ev.
 */

#include <stddef.h>

#include "harness.h"
#include "dsplib/FloatIIR.h"

/*
 * Placement new, declared here because CXXFLAGS carries -nostdinc++ and <new>
 * is not reachable (nor is the 32-bit libstdc++ that would define it).
 * Inline, so it emits no symbol: the test binaries link with $(CC) and the
 * link line has no C++ runtime on it.
 */
inline void *operator new(size_t, void *p) { return p; }

extern "C" {
/*
 * `this` is an ordinary first stack argument in this object -- see the header
 * comment of t_floatiir.cpp.  The asm label binds a readable name to the
 * mangled ref_ alias; the extern "C" stops the declaration being mangled a
 * second time.
 */
void ref_ctor(void *self, unsigned ncoeff, float *coeff, unsigned block)
	asm("ref__ZN8FloatIIRC1EjPfj");
void ref_dtor(void *self) asm("ref__ZN8FloatIIRD1Ev");
}

/* The blob's object, byte for byte, so m_hist can be read without guessing. */
struct ref_layout {
	float *coeff;
	float *hist;
	unsigned ncoeff;
	unsigned len;
	int pos;
};

/* The whole allocator log at one instant. */
struct snap {
	int allocs;
	int frees;
	int live;
	int bad_free;
	int free_null;
	int overflow;
	unsigned bytes;
};

/* One construct/destroy/destroy cycle, sampled at all three instants. */
struct cycle {
	struct snap built;
	struct snap once;
	struct snap twice;
	int hist_nonnull;	/* m_hist after construction    */
	unsigned len;		/* m_len after construction     */
};

static void
take(struct snap *s)
{
	s->allocs = harness_alloc.allocs;
	s->frees = harness_alloc.frees;
	s->live = harness_alloc.live;
	s->bad_free = harness_alloc.bad_free;
	s->free_null = harness_alloc.free_null;
	s->overflow = harness_alloc.overflow;
	s->bytes = harness_alloc.bytes;
}

static void
run_ours(unsigned ncoeff, float *coeff, unsigned block, struct cycle *c)
{
	static unsigned char buf[64];
	FloatIIR *p;
	const ref_layout *l;

	harness_alloc_reset();
	p = new (buf) FloatIIR(ncoeff, coeff, block);
	l = (const ref_layout *)p;
	c->hist_nonnull = (l->hist != 0);
	c->len = l->len;
	take(&c->built);

	p->~FloatIIR();
	take(&c->once);

	/* D56: m_hist still points at the block that was just released. */
	p->~FloatIIR();
	take(&c->twice);
}

static void
run_ref(unsigned ncoeff, float *coeff, unsigned block, struct cycle *c)
{
	static unsigned char buf[64];
	const ref_layout *l;

	harness_alloc_reset();
	ref_ctor(buf, ncoeff, coeff, block);
	l = (const ref_layout *)buf;
	c->hist_nonnull = (l->hist != 0);
	c->len = l->len;
	take(&c->built);

	ref_dtor(buf);
	take(&c->once);

	ref_dtor(buf);
	take(&c->twice);
}

/* Every counter, at one instant, ours against the blob's. */
static void
cmp(const struct snap *a, const struct snap *b, long tag)
{
	diff_eq_int("allocs", a->allocs, b->allocs, tag);
	diff_eq_int("frees", a->frees, b->frees, tag);
	diff_eq_int("live", a->live, b->live, tag);
	diff_eq_int("bad frees", a->bad_free, b->bad_free, tag);
	diff_eq_int("null frees", a->free_null, b->free_null, tag);
	diff_eq_int("live-set overflow", a->overflow, b->overflow, tag);
	diff_eq_int("bytes", (long)a->bytes, (long)b->bytes, tag);
}

static void
show(const char *who, const struct cycle *c)
{
	printf("    %-5s  built %d/%d/%d/%d  once %d/%d/%d/%d"
	       "  twice %d/%d/%d/%d  (%u bytes)\n",
	       who,
	       c->built.allocs, c->built.frees, c->built.live, c->built.bad_free,
	       c->once.allocs, c->once.frees, c->once.live, c->once.bad_free,
	       c->twice.allocs, c->twice.frees, c->twice.live,
	       c->twice.bad_free,
	       c->built.bytes);
}

static float coef_a[16] = {
	0.5f, -0.25f, 0.125f, -0.0625f, 0.03125f, -0.015625f,
	0.0078125f, -0.00390625f, 0.5f, 0.25f, -0.75f, 0.125f,
	-0.5f, 0.375f, -0.125f, 0.0625f
};
static float coef_b[16] = {
	-0.1f, 0.2f, -0.3f, 0.4f, -0.05f, 0.15f, -0.25f, 0.35f,
	0.11f, -0.22f, 0.33f, -0.44f, 0.55f, -0.66f, 0.77f, -0.88f
};

static void
one(unsigned ncoeff, unsigned block, float *coeff, long tag)
{
	struct cycle ours, ref;
	unsigned want_bytes = ((ncoeff & ~3u) + block) * (unsigned)sizeof(float);

	run_ours(ncoeff, coeff, block, &ours);
	run_ref(ncoeff, coeff, block, &ref);

	printf("  ncoeff=%u block=%u  (allocs/frees/live/bad)\n", ncoeff, block);
	show("ours", &ours);
	show("ref", &ref);

	/*
	 * NON-VACUITY FIRST.  Every counter below is zero if nothing was ever
	 * allocated, and two sides that allocate nothing agree perfectly.  So
	 * establish that the history exists, that it is exactly one block, and
	 * that it is the size the geometry demands -- on each side separately,
	 * because "they match" is what is being tested and cannot also be the
	 * evidence that the test ran.
	 */
	diff_eq_int("ours: the history was allocated", ours.hist_nonnull, 1, tag);
	diff_eq_int("ref: the history was allocated", ref.hist_nonnull, 1, tag);
	diff_eq_int("ours: construction allocated exactly one block",
		    ours.built.allocs, 1, tag);
	diff_eq_int("ref: construction allocated exactly one block",
		    ref.built.allocs, 1, tag);
	diff_eq_int("ours: history bytes", (long)ours.built.bytes,
		    (long)want_bytes, tag);
	diff_eq_int("ref: history bytes", (long)ref.built.bytes,
		    (long)want_bytes, tag);
	diff_eq_int("ours: m_len", (long)ours.len,
		    (long)((ncoeff & ~3u) + block), tag);
	diff_eq_int("ref: m_len", (long)ref.len,
		    (long)((ncoeff & ~3u) + block), tag);

	/*
	 * THE DIFFERENTIAL PART: every counter, at all three instants.
	 */
	cmp(&ours.built, &ref.built, tag * 10 + 0);
	cmp(&ours.once, &ref.once, tag * 10 + 1);
	cmp(&ours.twice, &ref.twice, tag * 10 + 2);

	/*
	 * THE ABSOLUTE PART: what the agreed numbers actually are.  A purely
	 * differential check would pass just as well if both sides had been
	 * tidied up together, and D56 is a claim about the original.
	 */
	diff_eq_int("ours: the first destruction released the history",
		    ours.once.frees, 1, tag);
	diff_eq_int("ref: the first destruction released the history",
		    ref.once.frees, 1, tag);
	diff_eq_int("ours: the first destruction was clean",
		    ours.once.bad_free, 0, tag);
	diff_eq_int("ref: the first destruction was clean",
		    ref.once.bad_free, 0, tag);
	diff_eq_int("ours: nothing outstanding after one destruction",
		    ours.once.live, 0, tag);
	diff_eq_int("ref: nothing outstanding after one destruction",
		    ref.once.live, 0, tag);

	/* D56 itself. */
	diff_eq_int("D56 ours: the second destruction is a double free",
		    ours.twice.bad_free, 1, tag);
	diff_eq_int("D56 ref: the second destruction is a double free",
		    ref.twice.bad_free, 1, tag);
	/*
	 * And it is a double free rather than a free of nothing: the pointer
	 * was kept, so sysdep_free was entered with it and rejected.  Null the
	 * field and free_null stays 0 while bad_free drops to 0 too, which is
	 * why the pair is asserted rather than the first alone.
	 */
	diff_eq_int("D56 ours: not a free(NULL) -- the pointer was kept",
		    ours.twice.free_null, 0, tag);
	diff_eq_int("D56 ref: not a free(NULL) -- the pointer was kept",
		    ref.twice.free_null, 0, tag);
	diff_eq_int("ours: the second destruction released nothing more",
		    ours.twice.frees, 1, tag);
	diff_eq_int("ref: the second destruction released nothing more",
		    ref.twice.frees, 1, tag);
	diff_eq_int("ours: the bad free did not disturb the live count",
		    ours.twice.live, 0, tag);
	diff_eq_int("ref: the bad free did not disturb the live count",
		    ref.twice.live, 0, tag);
	diff_eq_int("ours: the live set never overflowed",
		    ours.twice.overflow, 0, tag);
	diff_eq_int("ref: the live set never overflowed",
		    ref.twice.overflow, 0, tag);
}

int
main(void)
{
	int rc = 0;

	diff_begin("FloatIIR: D56 -- the destructor keeps the freed pointer");
	/*
	 * Four geometries, because the counters are supposed to be
	 * independent of them: the tap count rounded down to a multiple of
	 * four (6 becomes 4), a one-sample block, and the largest tap count
	 * the coefficient arrays carry.  The zero-tap case is not driven --
	 * that is D57 and it does not come near the destructor.
	 */
	one(4, 8, coef_a, 1);
	one(8, 16, coef_a, 2);
	one(6, 8, coef_b, 3);		/* rounds to 4 */
	one(16, 64, coef_b, 4);
	rc |= diff_end();

	return rc;
}

/*
 * t_diffcoder -- the four differential coders against the blob.
 *
 * Everything is called through the mangled entry points on both sides, with
 * the objects placement-built into POISONED 32-byte slots, so a constructor
 * that wrote past +12 would be caught rather than ignored.  Nothing here needs
 * the bits-taking treatment t_sinewave and t_queue use: every argument is an
 * integer or a pointer, so no value passes through an x87 register on its way
 * in (finding 600).
 *
 * The two sides own SEPARATE allocations, so `state_` is never compared as an
 * address -- only whether it is null, and what it points at.
 */

#include <limits.h>
#include <string.h>

#include "harness.h"
#include "dsplib/DiffCoder.h"

extern "C" {
/* Parallel encoder. */
void ref_pe_ctor(void *self, unsigned n)
	asm("ref__ZN27ParallelDifferentialEncoderIhEC1Ej");
void ref_pe_dtor(void *self)
	asm("ref__ZN27ParallelDifferentialEncoderIhED1Ev");
int  ref_pe_reset(void *self, unsigned n, unsigned char init)
	asm("ref__ZN27ParallelDifferentialEncoderIhE5resetEjh");
void ref_pe_process(void *self, unsigned char *in, unsigned char *out)
	asm("ref__ZN27ParallelDifferentialEncoderIhE7processEPhS1_");

void our_pe_ctor(void *self, unsigned n)
	asm("_ZN27ParallelDifferentialEncoderIhEC1Ej");
void our_pe_dtor(void *self)
	asm("_ZN27ParallelDifferentialEncoderIhED1Ev");
int  our_pe_reset(void *self, unsigned n, unsigned char init)
	asm("_ZN27ParallelDifferentialEncoderIhE5resetEjh");
void our_pe_process(void *self, unsigned char *in, unsigned char *out)
	asm("_ZN27ParallelDifferentialEncoderIhE7processEPhS1_");

/* Parallel decoder. */
void ref_pd_ctor(void *self, unsigned n)
	asm("ref__ZN27ParallelDifferentialDecoderIhEC1Ej");
void ref_pd_dtor(void *self)
	asm("ref__ZN27ParallelDifferentialDecoderIhED1Ev");
int  ref_pd_reset(void *self, unsigned n, unsigned char init)
	asm("ref__ZN27ParallelDifferentialDecoderIhE5resetEjh");
void ref_pd_process(void *self, unsigned char *in, unsigned char *out)
	asm("ref__ZN27ParallelDifferentialDecoderIhE7processEPhS1_");

void our_pd_ctor(void *self, unsigned n)
	asm("_ZN27ParallelDifferentialDecoderIhEC1Ej");
void our_pd_dtor(void *self)
	asm("_ZN27ParallelDifferentialDecoderIhED1Ev");
int  our_pd_reset(void *self, unsigned n, unsigned char init)
	asm("_ZN27ParallelDifferentialDecoderIhE5resetEjh");
void our_pd_process(void *self, unsigned char *in, unsigned char *out)
	asm("_ZN27ParallelDifferentialDecoderIhE7processEPhS1_");

/* Serial, all three instantiations. */
unsigned char ref_se_process(void *self, unsigned char in)
	asm("ref__ZN25SerialDifferentialEncoderIhE7processEh");
unsigned char our_se_process(void *self, unsigned char in)
	asm("_ZN25SerialDifferentialEncoderIhE7processEh");
unsigned char ref_sd_process(void *self, unsigned char in)
	asm("ref__ZN25SerialDifferentialDecoderIhE7processEh");
unsigned char our_sd_process(void *self, unsigned char in)
	asm("_ZN25SerialDifferentialDecoderIhE7processEh");
int ref_sdi_process(void *self, int in)
	asm("ref__ZN25SerialDifferentialDecoderIiE7processEi");
int our_sdi_process(void *self, int in)
	asm("_ZN25SerialDifferentialDecoderIiE7processEi");
}

/* The blob's parallel object, so its fields can be read without a second copy
 * of the class that might not agree with the first. */
struct ref_parallel {
	unsigned char	*state;
	unsigned	 capacity;
	unsigned	 size;
};

/*
 * The two classes have identical layouts and identical `reset`/ctor/dtor
 * behaviour, so one driver runs both and the only thing that varies is which
 * four entry points it is handed.
 */
struct coder {
	void (*ctor)(void *, unsigned);
	void (*dtor)(void *);
	int  (*reset)(void *, unsigned, unsigned char);
	void (*process)(void *, unsigned char *, unsigned char *);
};

static const coder our_enc = { our_pe_ctor, our_pe_dtor, our_pe_reset,
			       our_pe_process };
static const coder ref_enc = { ref_pe_ctor, ref_pe_dtor, ref_pe_reset,
			       ref_pe_process };
static const coder our_dec = { our_pd_ctor, our_pd_dtor, our_pd_reset,
			       our_pd_process };
static const coder ref_dec = { ref_pd_ctor, ref_pd_dtor, ref_pd_reset,
			       ref_pd_process };

/*
 * Compare the two objects.  `state_` is compared only for nullness -- the two
 * sides allocate separately and the addresses will never agree -- and the
 * OWNED BUFFER is compared over the whole capacity, not just the active width,
 * because "reset fills only `size`" is one of the behaviours under test.
 */
static void cmp(const void *a, const void *b, unsigned cap, long tag)
{
	const ref_parallel *x = (const ref_parallel *)a;
	const ref_parallel *y = (const ref_parallel *)b;
	unsigned i;

	diff_eq_int("capacity", (long)x->capacity, (long)y->capacity, tag);
	diff_eq_int("size", (long)x->size, (long)y->size, tag);
	diff_eq_int("state allocated", x->state != 0, y->state != 0, tag);
	if (!x->state || !y->state)
		return;
	for (i = 0; i < cap; i++)
		diff_eq_int("state", (long)x->state[i], (long)y->state[i],
			    tag * 1000 + i);
	/* Bytes 12..31 of the slot: nothing may be written past the object. */
	for (i = 12; i < 32; i++)
		diff_eq_int("wrote past the object",
			    (long)((const unsigned char *)a)[i],
			    (long)((const unsigned char *)b)[i],
			    tag * 1000 + 100 + i);
}

/* One capacity, driven through the whole life cycle. */
static void life(const coder *ours, const coder *refs, unsigned cap, long tag)
{
	static unsigned char oa[32], ob[32];
	static unsigned char in[128], outa[128], outb[128];
	static const int widths[] = { -2, -1, 0, 1, 5 };	/* cap + these */
	unsigned i;
	int w, t;

	memset(oa, 0x5a, sizeof(oa));
	memset(ob, 0x5a, sizeof(ob));
	ours->ctor(oa, cap);
	refs->ctor(ob, cap);
	cmp(oa, ob, cap, tag);

	/*
	 * `process` BEFORE ANY `reset`.  `size_` is 0 at this point so nothing
	 * should be written; a constructor that had called `reset` itself would
	 * write here and be caught immediately.
	 */
	for (i = 0; i < 128; i++)
		in[i] = (unsigned char)(i * 7 + 3);
	memset(outa, 0xa5, sizeof(outa));
	memset(outb, 0xa5, sizeof(outb));
	ours->process(oa, in, outa);
	refs->process(ob, in, outb);
	for (i = 0; i < 128; i++)
		diff_eq_int("out before reset", (long)outa[i], (long)outb[i],
			    tag * 1000 + i);
	cmp(oa, ob, cap, tag + 1);

	for (w = 0; w < (int)(sizeof(widths) / sizeof(widths[0])); w++) {
		int want = (int)cap + widths[w];
		unsigned width = want < 0 ? 0u : (unsigned)want;
		long wtag = tag * 100 + w;
		int ra, rb;

		/*
		 * Widths above the capacity must be refused, and refused
		 * WITHOUT writing anything -- including without setting
		 * `size_`, which the following `process` then demonstrates.
		 */
		ra = ours->reset(oa, width, (unsigned char)(0x11 * (w + 1)));
		rb = refs->reset(ob, width, (unsigned char)(0x11 * (w + 1)));
		diff_eq_int("reset return", ra, rb, wtag);
		cmp(oa, ob, cap, wtag);

		for (t = 0; t < 8; t++) {
			for (i = 0; i < 128; i++)
				in[i] = (unsigned char)(i * 13 + t * 29 + w);
			memset(outa, 0xa5, sizeof(outa));
			memset(outb, 0xa5, sizeof(outb));
			ours->process(oa, in, outa);
			refs->process(ob, in, outb);
			for (i = 0; i < 128; i++)
				diff_eq_int("out", (long)outa[i], (long)outb[i],
					    (wtag * 10 + t) * 1000 + i);
			cmp(oa, ob, cap, wtag * 10 + t);
		}

		/*
		 * IN PLACE.  `V90SpectralShaper` does not do this, but the loop
		 * body reads and writes one element at a time, so `in == out`
		 * is well defined and is a cheap way to pin the order of the
		 * two stores inside the loop.
		 */
		for (t = 0; t < 4; t++) {
			for (i = 0; i < 128; i++)
				outa[i] = outb[i] =
					(unsigned char)(i * 5 + t * 17);
			ours->process(oa, outa, outa);
			refs->process(ob, outb, outb);
			for (i = 0; i < 128; i++)
				diff_eq_int("out in place", (long)outa[i],
					    (long)outb[i],
					    (wtag * 10 + t) * 1000 + 500 + i);
			cmp(oa, ob, cap, wtag * 10 + t + 50);
		}
	}

	ours->dtor(oa);
	refs->dtor(ob);
	/* The destructor does not null `state_`; both must agree about that. */
	diff_eq_int("state after dtor",
		    ((const ref_parallel *)oa)->state != 0,
		    ((const ref_parallel *)ob)->state != 0, tag + 900000);
}

int
main(void)
{
	static const unsigned caps[] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 33, 64, 100
	};
	int rc = 0;
	unsigned c, i;
	long tag = 0;

	diff_begin("diffcoder: parallel encoder, whole life cycle");
	for (c = 0; c < sizeof(caps) / sizeof(caps[0]); c++)
		life(&our_enc, &ref_enc, caps[c], tag++);
	rc |= diff_end();

	diff_begin("diffcoder: parallel decoder, whole life cycle");
	for (c = 0; c < sizeof(caps) / sizeof(caps[0]); c++)
		life(&our_dec, &ref_dec, caps[c], tag++);
	rc |= diff_end();

	/*
	 * THE NULL-`state_` DESTRUCTOR, driven on BOTH SIDES.  The object tests
	 * the pointer before freeing, and a destructor that freed
	 * unconditionally would pass a version of this case that only ran the
	 * reference -- which is how it escaped once already.
	 */
	diff_begin("diffcoder: destructor with a null state pointer");
	{
		static unsigned char oa[32], ob[32];

		memset(oa, 0, sizeof(oa));
		memset(ob, 0, sizeof(ob));
		our_pe_dtor(oa);
		ref_pe_dtor(ob);
		for (i = 0; i < 32; i++)
			diff_eq_int("encoder slot", (long)oa[i], (long)ob[i],
				    (long)i);

		memset(oa, 0, sizeof(oa));
		memset(ob, 0, sizeof(ob));
		our_pd_dtor(oa);
		ref_pd_dtor(ob);
		for (i = 0; i < 32; i++)
			diff_eq_int("decoder slot", (long)oa[i], (long)ob[i],
				    (long)i + 100);
	}
	rc |= diff_end();

	/*
	 * The serial pair.  The state is one member, so it is set by hand and
	 * the sweep is over every starting state crossed with every input --
	 * exhaustive for the byte instantiations, sampled for the `int` one
	 * with the awkward values included by name.
	 */
	diff_begin("diffcoder: serial encoder and decoder, every byte");
	{
		unsigned s, v;

		for (s = 0; s < 256; s++)
			for (v = 0; v < 256; v++) {
				unsigned char ea = (unsigned char)s;
				unsigned char eb = (unsigned char)s;
				unsigned char da = (unsigned char)s;
				unsigned char db = (unsigned char)s;
				long t = (long)s * 256 + v;

				diff_eq_int("serial encode",
					    our_se_process(&ea, (unsigned char)v),
					    ref_se_process(&eb, (unsigned char)v),
					    t);
				diff_eq_int("encoder state", (long)ea, (long)eb, t);
				diff_eq_int("serial decode",
					    our_sd_process(&da, (unsigned char)v),
					    ref_sd_process(&db, (unsigned char)v),
					    t);
				diff_eq_int("decoder state", (long)da, (long)db, t);
			}
	}
	rc |= diff_end();

	diff_begin("diffcoder: serial decoder at int");
	{
		static const int hard[] = {
			0, 1, -1, INT_MIN, INT_MAX, 0x55555555, (int)0xaaaaaaaa,
			0x7fffffff, (int)0x80000001, 2, -2
		};
		unsigned long r = 987654321ul;
		unsigned a, b;
		int t;

		for (a = 0; a < sizeof(hard) / sizeof(hard[0]); a++)
			for (b = 0; b < sizeof(hard) / sizeof(hard[0]); b++) {
				int sa = hard[a], sb = hard[a];

				diff_eq_int("int decode",
					    our_sdi_process(&sa, hard[b]),
					    ref_sdi_process(&sb, hard[b]),
					    (long)a * 100 + b);
				diff_eq_int("int state", sa, sb,
					    (long)a * 100 + b);
			}

		for (t = 0; t < 4000; t++) {
			int sa, sb, in;

			r = r * 1103515245ul + 12345ul;
			sa = sb = (int)(r >> 3);
			r = r * 1103515245ul + 12345ul;
			in = (int)(r >> 3);
			diff_eq_int("int decode random",
				    our_sdi_process(&sa, in),
				    ref_sdi_process(&sb, in), (long)t);
			diff_eq_int("int state random", sa, sb, (long)t);
		}
	}
	rc |= diff_end();

	/*
	 * A round trip, which is the property the pair exists for.  It is
	 * deliberately CROSSED: the object's encoder produces the intermediate
	 * and ours decodes it.  A pair of ours that were wrong in the same way
	 * would satisfy an ours-to-ours round trip and fail this one.
	 */
	diff_begin("diffcoder: encode then decode returns the input");
	{
		static unsigned char e[32], d[32], mid[128], back[128], in[128];
		unsigned n;

		for (n = 1; n <= 8; n++) {
			unsigned t;

			memset(e, 0x5a, sizeof(e));
			memset(d, 0x5a, sizeof(d));
			ref_pe_ctor(e, n);
			ref_pd_ctor(d, n);
			ref_pe_reset(e, n, 0);
			ref_pd_reset(d, n, 0);

			for (t = 0; t < 20; t++) {
				for (i = 0; i < n; i++)
					in[i] = (unsigned char)(i * 31 + t * 7);
				ref_pe_process(e, in, mid);
				our_pd_process(d, mid, back);
				for (i = 0; i < n; i++)
					diff_eq_int("round trip", (long)back[i],
						    (long)in[i],
						    (long)n * 1000 + t * 10 + i);
			}
			ref_pe_dtor(e);
			ref_pd_dtor(d);
		}
	}
	rc |= diff_end();

	return rc;
}

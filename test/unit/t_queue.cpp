/*
 * t_queue -- Queue<float> against the blob.
 *
 * The two sides allocate from SEPARATE ARENAS and their pointers are compared
 * as offsets into their own, never as addresses: three of the five object
 * words are pointers into the backing store, so a raw comparison would report
 * a difference on every field for every case.
 *
 * SIGNALLING NaNs ARE IN THE INPUT ON PURPOSE.  The object copies elements
 * with an integer move; a `float` assignment under `-mfpmath=387` compiles to
 * `flds`/`fstps`, and an x87 load-store quietens a signalling NaN.  That is
 * the one input class that tells a faithful copy from a plausible one, so it
 * is driven here rather than avoided.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/Queue.h"
#include "dsplib/sysdep.h"

/*
 * Queue is header-only, so this TU instantiates its constructor/destructor
 * and therefore needs the same replacement `operator delete[]` every source
 * TU that reaches a `delete[]`-using destructor carries.  In src/ TUs this
 * arrives through Scrambler.h; the test harness reaches no such header, so
 * the definition lives here.  This is apparatus, not reconstruction.
 */
inline void operator delete[](void *p) { sysdep_free(p); }

extern "C" {
void ref_qctor(void *self, unsigned n)              asm("ref__ZN5QueueIfEC1Ej");
void ref_qdtor(void *self)                          asm("ref__ZN5QueueIfED1Ev");
void ref_qreset(void *self)                         asm("ref__ZN5QueueIfE5resetEv");
int  ref_qwrite1(void *self, float v)               asm("ref__ZN5QueueIfE5writeEf");
int  ref_qwriteb(void *self, float *p, unsigned n)  asm("ref__ZN5QueueIfE5writeEPfj");
int  ref_qreadb(void *self, float *p, unsigned n)   asm("ref__ZN5QueueIfE4readEPfj");

/*
 * OUR OWN `write(float)` as a free function, so it can be called the same way
 * the reference is.  `this` is a plain first stack argument under cdecl -- the
 * object is not compiled with thiscall -- so this declaration and the member
 * are the same entry point, and the asm label names it rather than letting C++
 * mangle a second, different one.
 */
int  our_qwrite1(void *self, float v)               asm("_ZN5QueueIfE5writeEf");

/*
 * ...and both single-value writes AGAIN, taking the argument as raw bits.  See
 * the long comment in the signalling-NaN case for why that is the only way to
 * put the same stack image in front of both.  Declaring the entry points at
 * the type wanted is better than casting the pointers: a cast between
 * incompatible function types is a warning, and here it would be warning about
 * exactly the thing that makes the test correct.
 */
int  our_qwrite1_bits(void *self, unsigned b)       asm("_ZN5QueueIfE5writeEf");
int  ref_qwrite1_bits(void *self, unsigned b)       asm("ref__ZN5QueueIfE5writeEf");
}

static long bits(float f)
{
	union { float f; unsigned u; } u;

	u.f = f;
	return (long)u.u;
}

/*
 * The blob's object, byte for byte, so its fields can be read without a
 * second copy of the class that might not agree with the first.
 */
struct ref_queue {
	float	*buf;
	float	*last;
	float	*rd;
	float	*wr;
	unsigned size;
};

/*
 * Compare, with the three pointers turned into element offsets from each
 * side's own `buf`.  A null `buf` is compared as null on both sides rather
 * than differenced.
 */
static void compare(Queue<float> *a, const ref_queue *b, long tag,
		    int slots = 1)
{
	const ref_queue *la = (const ref_queue *)a;

	diff_eq_int("size", la->size, b->size, tag);
	diff_eq_int("buf allocated", la->buf != 0, b->buf != 0, tag);
	if (!la->buf || !b->buf)
		return;
	diff_eq_int("last offset", (long)(la->last - la->buf),
		    (long)(b->last - b->buf), tag);
	diff_eq_int("rd offset", (long)(la->rd - la->buf),
		    (long)(b->rd - b->buf), tag);
	diff_eq_int("wr offset", (long)(la->wr - la->buf),
		    (long)(b->wr - b->buf), tag);
	/*
	 * `slots` is 0 after the destructor.  Both sides have freed their
	 * store by then and what an allocator leaves in freed memory is its
	 * own business, not the object's -- comparing it compares the two
	 * arenas rather than the two Queues.  The pointer itself IS compared,
	 * because the object deliberately does not null it and that is a
	 * property worth pinning.
	 */
	if (!slots)
		return;
	for (unsigned i = 0; i < b->size; i++)
		diff_eq_int("slot", bits(la->buf[i]), bits(b->buf[i]),
			    tag * 1000 + i);
}

int
main(void)
{
	static unsigned char refobj[64];
	static float in[64], outa[64], outb[64];
	int rc = 0;
	unsigned n, k, i;

	diff_begin("queue: construct, wrap and drain");
	for (n = 0; n <= 12; n++) {
		Queue<float> q(n);
		ref_queue *r = (ref_queue *)refobj;
		int step;

		memset(refobj, 0, sizeof(refobj));
		ref_qctor(refobj, n);
		compare(&q, r, (long)n);

		/*
		 * Enough single writes and reads to walk the cursor past the
		 * end several times: the wrap is the only interesting part and
		 * a short run never reaches it.
		 */
		for (step = 0; step < 60; step++) {
			float v;
			unsigned bitsv = 0x3f800000u + (unsigned)step;
			long tag = (long)n * 1000 + step;

			memcpy(&v, &bitsv, 4);
			diff_eq_int("write1", q.write(v),
				    ref_qwrite1(refobj, v), tag);
			compare(&q, r, tag);

			if ((step & 3) == 3) {
				diff_eq_int("read1", q.read(outa, 1),
					    ref_qreadb(refobj, outb, 1), tag);
				diff_eq_int("read1 value", bits(outa[0]),
					    bits(outb[0]), tag);
				compare(&q, r, tag);
			}
		}
		q.~Queue();
		ref_qdtor(refobj);
		/* The destructor leaves a stale pointer; both must agree. */
		compare(&q, r, (long)n + 500000, 0);
	}
	rc |= diff_end();

	diff_begin("queue: block writes and reads, including the split");
	for (n = 1; n <= 10; n++)
		for (k = 0; k <= n + 2; k++) {
			Queue<float> q(n);
			ref_queue *r = (ref_queue *)refobj;
			long tag = (long)n * 100 + k;
			unsigned phase;

			memset(refobj, 0, sizeof(refobj));
			ref_qctor(refobj, n);

			/*
			 * Rotate the cursors first, so the block operation
			 * straddles the end of the store rather than always
			 * starting at slot 0.
			 */
			for (phase = 0; phase < n; phase++) {
				float one = 1.0f;

				q.write(one);
				ref_qwrite1(refobj, one);
				q.read(outa, 1);
				ref_qreadb(refobj, outb, 1);
			}

			for (i = 0; i < 64; i++) {
				unsigned b = 0x40000000u + i;

				memcpy(&in[i], &b, 4);
			}
			memset(outa, 0x5a, sizeof(outa));
			memset(outb, 0x5a, sizeof(outb));

			diff_eq_int("block write", q.write(in, k),
				    ref_qwriteb(refobj, in, k), tag);
			compare(&q, r, tag);
			diff_eq_int("block read", q.read(outa, k),
				    ref_qreadb(refobj, outb, k), tag);
			for (i = 0; i < 64; i++)
				diff_eq_int("block out", bits(outa[i]),
					    bits(outb[i]), tag * 100 + i);
			compare(&q, r, tag + 200000);

			q.~Queue();
			ref_qdtor(refobj);
		}
	rc |= diff_end();

	/*
	 * THE ONE THAT DISCRIMINATES.  A signalling NaN survives an integer
	 * copy and is quietened by an x87 load-store, so this case fails
	 * against a `*q++ = *p++` reconstruction and passes against the
	 * object's.  Both payload-bearing patterns and the bare minimum are
	 * driven, either side of the quiet bit.
	 */
	diff_begin("queue: a signalling NaN survives the copy");
	{
		/*
		 * THE SINGLE-VALUE WRITE IS CALLED THROUGH A BITS-TAKING POINTER,
		 * and that is not a shortcut -- it is what makes the case a test of
		 * the object rather than of the compiler.
		 *
		 * `write(T)` takes its argument by value, and on i386 cdecl a
		 * `float` argument and a 4-byte integer argument occupy the same
		 * stack slot.  Which instruction the CALLER uses to fill that slot
		 * is the caller's own choice, and GCC made opposite choices for the
		 * two sides of this test: `flds`/`fstps` into the outgoing slot for
		 * ours and a `push` of the memory word for the blob's.  The x87
		 * store quietens, so the two callees were handed DIFFERENT BITS and
		 * the "difference" reported was manufactured by the test.
		 *
		 * Passing the pattern as an `unsigned` puts a byte-identical stack
		 * image in front of both, so what is compared
		 * afterwards is what each callee did with it.  The block paths need
		 * no such care: they take a pointer, so nothing is in an x87
		 * register at the boundary.
		 */
		static const unsigned pats[] = {
			0x7f800001u, 0xff800001u, 0x7fbfffffu, 0xffbfffffu,
			0x7fc00000u, 0x7f800000u, 0xff800000u, 0x00000001u,
			0x80000000u, 0x7fffffffu
		};

		for (k = 0; k < sizeof(pats) / sizeof(pats[0]); k++) {
			Queue<float> q(8);
			ref_queue *r = (ref_queue *)refobj;
			float v;

			memset(refobj, 0, sizeof(refobj));
			ref_qctor(refobj, 8);
			memcpy(&v, &pats[k], 4);

			for (i = 0; i < 5; i++)
				memcpy(&in[i], &pats[k], 4);

			our_qwrite1_bits(&q, pats[k]);
			ref_qwrite1_bits(refobj, pats[k]);
			compare(&q, r, (long)k);

			q.write(in, 5);
			ref_qwriteb(refobj, in, 5);
			compare(&q, r, (long)k + 100);

			memset(outa, 0, sizeof(outa));
			memset(outb, 0, sizeof(outb));
			q.read(outa, 6);
			ref_qreadb(refobj, outb, 6);
			for (i = 0; i < 6; i++)
				diff_eq_int("nan out", bits(outa[i]),
					    bits(outb[i]), (long)k * 10 + i);

			q.~Queue();
			ref_qdtor(refobj);
		}
	}
	rc |= diff_end();

	/*
	 * `reset` CALLED ON ITS OWN, and until this section it was declared
	 * here and never called.
	 *
	 * The constructor tail-jumps to it, so every trial above drives it --
	 * but only transitively, and `coverage.py` counts a symbol tested when
	 * a test object references its `ref_` alias BY NAME.  A declaration
	 * emits no reference, so `_ZN5QueueIfE5resetEv` read as "translated,
	 * alias exists, and NOT tested" while being exercised on every
	 * construction in the file.  Finding F8326.
	 *
	 * Called on a queue that has been FILLED, which is the part the
	 * constructor's call cannot reach: on a fresh object the cursors are
	 * already where `reset` puts them, so a `reset` that did nothing would
	 * pass a test that only ever ran it at construction.  The stored
	 * samples are compared afterwards as well -- `reset` moves the cursors
	 * and must not touch the store.
	 */
	diff_begin("queue: reset on a filled queue");
	for (n = 1; n <= 10; n++) {
		Queue<float> q(n);
		ref_queue *r = (ref_queue *)refobj;

		memset(refobj, 0, sizeof(refobj));
		ref_qctor(refobj, n);

		for (k = 0; k < n + 3; k++) {
			float v;
			unsigned bitsv = 0x40000000u + k;

			memcpy(&v, &bitsv, 4);
			q.write(v);
			ref_qwrite1(refobj, v);
		}
		compare(&q, r, (long)n);

		q.reset();
		ref_qreset(refobj);
		compare(&q, r, (long)n + 900000);

		/* And it is still usable afterwards, on both sides. */
		diff_eq_int("write after reset", q.write(1.0f),
			    ref_qwrite1(refobj, 1.0f), (long)n + 901000);
		compare(&q, r, (long)n + 901000);

		q.~Queue();
		ref_qdtor(refobj);
	}
	rc |= diff_end();

	return rc;
}

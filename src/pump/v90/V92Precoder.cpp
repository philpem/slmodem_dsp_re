/*
 * V92Precoder.cpp -- V.92 transmit precoder: construction and destruction.
 *
 * Reconstructed from dsplibs.o.  Two of the class's six members;
 * `include/dsplib/V92Precoder.h` carries the object map and the evidence for
 * it.  The other four are declared there and deliberately left undefined.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL, with `this` as the first stack
 * argument (`mov 0x20(%esp),%edi` after a 0x1c-byte frame), so nothing here
 * needs an attribute -- finding F215.
 *
 * THE SUB-OBJECTS ARE BUILT WITH ORDINARY PLACEMENT `new` over an allocator
 * hooked to sysdep_malloc (finding F10155/F10157): this build is
 * `-nostdinc++` with no <new>, so `include/dsplib/sysdep.h` declares the
 * shared non-throw placement `operator new`/`operator delete` pair every
 * such site in this tree uses.  This file used to reach `FloatFIR`'s
 * constructor through a hand-mangled `asm("_ZN8FloatFIRC1EjPfj")` label on
 * the belief (finding F1340) that a user-declared placement `operator new`
 * would force a null test the blob does not have; F10155 retracts that
 * empirically.
 *
 * THIS USED TO END "the instruction sequence is the blob's either way; only
 * the spelling differs", AND THAT HALF IS WITHDRAWN (finding F7816).  It was
 * true of the CONSTRUCTOR too until F10155 -- the asm() label cost nothing
 * either -- but ordinary placement `new` is no worse there and is what this
 * file now uses throughout.  It was already false of the DESTRUCTOR: see the
 * replacement `operator delete` below for the instruction it costs.
 */

#include <stddef.h>

#include "dsplib/V92Precoder.h"
#include "dsplib/sysdep.h"

/*
 * THE REPLACEMENT `operator delete`, AND IT IS READ OFF THE OBJECT.  The
 * comment above already read the blob's destructor as `delete p` over an
 * inline wrapper; what it did NOT know is that writing that shape out by hand
 * is not equivalent.  At the destructor's LAST free the delete-expression
 * emits an ordinary `call sysdep_free`, and the open-coded
 * `p->~FloatFIR(); sysdep_free(p)` emits a sibling `jmp` and drops the frame
 * with it -- 25 instructions and 77 bytes against the blob's 29 and 108.
 * With `delete` it is byte-identical.  Findings F7786 and F7816,
 * refinement.md lever 7.
 */
extern "C" void sysdep_free(void *p);
inline void operator delete(void *p) { sysdep_free(p); }

/*
 * NO SIZED `operator delete` HERE, AND NOWHERE ELSE EITHER.  C++14 made GCC
 * 13 prefer `operator delete(void *, size_t)` for the delete-expressions
 * below -- an undefined `_ZdlPvj` in a tree that links no libstdc++ -- and
 * this file used to carry a `#if __cplusplus >= 201402L` block answering it.
 * The Makefile passes `-fno-sized-deallocation` instead, so the modern build
 * resolves `delete p` the way GCC 3.4.2 resolves it: to the UNSIZED operator
 * just above.  C++14 postdates the object by sixteen years and cannot be the
 * author's, so the shim was apparatus inside the reconstruction; the flag is
 * apparatus where apparatus belongs.  Finding F7900.
 *
 * THE UNSIZED OPERATORS ARE A DIFFERENT CASE AND STAY HERE, one copy per
 * file -- the one above, and any `operator delete[]` this file defines.  The
 * period compiler DOES see those, and an inline definition's POSITION in the
 * translation unit is a lever-3 carrier: consolidating the unsized array form
 * into `sysdep.h` cost eight destructors their byte identity, four of them a
 * previous wave's.  So a local copy is what PRESERVES those symbols, not what
 * costs them, and it is deliberate duplication rather than a tidy-up waiting
 * to happen.  Finding F7815, which says not to consolidate them without
 * re-running the SET diff.
 */

/*
 * Both of these declare their own `extern "C"`.  V92ParamsInfo.h is here
 * because `reset(V92MappingParams *)` DEREFERENCES the parameter block, and a
 * translation unit that dereferences a type must include it (finding F1325);
 * finding F1321 is what says the two names are one 180-byte block.
 */
#include "dsplib/debug.h"
#include "dsplib/V92ParamsInfo.h"

/*
 * Hold the compiler to the map in the header.  Guarded because the object is
 * a 32-bit one and every pointer field moves in the 64-bit build.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V92PRE_OFF(field, off, tag) \
	typedef char v92pre_off_##tag[ \
	    ((int)__builtin_offsetof(V92Precoder, field) == (off)) ? 1 : -1]

V92PRE_OFF(word_00,     0x00, word_00);
V92PRE_OFF(paramsAt9c,  0x04, paramsat9c);
V92PRE_OFF(head,        0x08, head);
V92PRE_OFF(tableA,      0x20, tablea);
V92PRE_OFF(tableB,      0x50, tableb);
V92PRE_OFF(fir1,        0x68, fir1);
V92PRE_OFF(fir2,        0x6c, fir2);
V92PRE_OFF(state0,      0x70, state0);
V92PRE_OFF(state1,      0x74, state1);
V92PRE_OFF(taps1,       0x78, taps1);
V92PRE_OFF(taps2,       0x7c, taps2);
typedef char v92pre_size[(sizeof(V92Precoder) == 0x80) ? 1 : -1];
typedef char v92pre_fir_size[(sizeof(FloatFIR) == 0x14) ? 1 : -1];
#endif

/*
 * Two identical filters, both `FloatFIR(nTaps, 0, 99)`, and nothing else --
 * the other thirty fields of the object are left as they were found and
 * `reset(V92MappingParams *)` is what fills them.
 *
 * The allocation is not checked.  Neither is the blob's; see
 * docs/deviations.md.
 */
V92Precoder::V92Precoder(unsigned int nTaps)
{
	FloatFIR *p;

	/* C1, the complete-object variant, is what a `new` expression uses
	 * and what the relocation at 0x56bdb names.  `sizeof(FloatFIR)` and
	 * not the literal 20 that appears in the object: this file is also
	 * compiled 64-bit for the interop binaries, where 20 is wrong. */
	p = (FloatFIR *)sysdep_malloc(sizeof(FloatFIR));
	new (p) FloatFIR(nTaps, 0, V92PRECODER_BLOCK);
	fir1 = p;

	p = (FloatFIR *)sysdep_malloc(sizeof(FloatFIR));
	new (p) FloatFIR(nTaps, 0, V92PRECODER_BLOCK);
	fir2 = p;
}

/*
 * Both filters, in the order they were allocated.  The null tests are not
 * decoration and they are not free to omit: this tree's `sysdep_free`
 * tolerates NULL, so dropping them leaves every byte of every object
 * unchanged and moves only `harness_alloc.free_null` -- which is why
 * t_v92precoder.cpp asserts that counter on the trials that null a pointer by
 * hand.
 *
 * Nothing is nulled after the free.  The object is left holding two dangling
 * addresses, exactly as the blob leaves it; see docs/deviations.md.
 */
V92Precoder::~V92Precoder()
{
	delete fir1;
	delete fir2;
}

/*
 * reset() -- 0x56d80, 87 bytes, immediately BEFORE reset(V92MappingParams *)
 * in the blob as here.  "reset 2" in its own words, against the other's
 * "reset 1 (with cfg)": no configuration, just both filters cleared.  The
 * object duplicates the two `FloatFIR::reset` calls into each arm of the
 * debug test with the second as a sibling `jmp`; one gated print and two
 * calls is the source both arms are copies of.
 */
void
V92Precoder::reset()
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Precoder: reset 2 called\r\n");

	fir1->reset();
	fir2->reset();
}

/*
 * Refill the object from the parameter block: twenty-five words copied, one
 * pointer taken into the block, and the two carried samples zeroed.  The two
 * filters at +0x68 and +0x6c are the only fields below +0x78 this does NOT
 * write, which is the map's best evidence -- see the header.
 *
 * THE BLOCK IS NOW NAMED, AND THIS FUNCTION READS THE NAMES.  It used to
 * address the eighteen scalars it copies as words inside `pad_00` and
 * `pad_6c`, because V92ParamsInfo.h had names for only the ten pointers.  The
 * unpacker supplied the rest: the twelve are `m[0..11]`, the six are
 * `LC[0..5]`, and the pointer this keeps is into `indexConstel`, which was
 * `pad_9c` and which the header there described as "the eighteen bytes" --
 * 0x18 read as decimal, and twenty-four in fact.
 *
 * `LC` is unsigned in the block, because the unpacker clamps it with `jbe`,
 * and `tableB` is int; the cast below is that narrowing written down rather
 * than left implicit.  Neither the values nor the codegen move: every LC the
 * unpacker leaves is at most 0x80.
 *
 * The store order below is the object's, which interleaves the three groups
 * -- +0x50 before +0x08 before +0x20 -- and is the compiler's scheduling
 * rather than anything the source can have said.
 */
void
V92Precoder::reset(V92MappingParams *params)
{
	struct V92ParamsInfo *p = (struct V92ParamsInfo *)params;
	int i;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Precoder: reset 1 (with cfg) " "called\r\n");

	paramsAt9c = p->indexConstel;

	for (i = 0; i < 6; i++) {
		head[i] = (unsigned int *)p->constellations[i];
		tableB[i] = (int)p->LC[i];
	}

	for (i = 0; i < 12; i++)
		tableA[i] = p->m[i];

	state0 = 0.0f;
	state1 = 0.0f;
}

/*
 * The two filters, then the two counts -- exactly V92PreFilter's shape with
 * one class name changed, and the counts stored after both calls again.
 * Neither filter's return value is looked at.
 */
void
V92Precoder::setCoefficients(float *coefFir1, float *coefFir2,
			     unsigned int taps1Arg, unsigned int taps2Arg)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Precoder: setCoefficients called\r\n");

	fir1->setCoefficients(coefFir1, taps1Arg);
	fir2->setCoefficients(coefFir2, taps2Arg);

	taps1 = taps1Arg;
	taps2 = taps2Arg;
}

/*
 * Four symbols of precoding, and the search that is the whole point of the
 * class.
 *
 * FOR EACH OF THE FOUR, `n = i + 4 * a` selects a step out of `tableA` and a
 * word out of the caller's `in`, and `paramsAt9c[n % 6]` selects both a
 * constellation and the modulus `m = 2 * tableB[selector]`.  The candidates
 * are `k * tableA[n] + in[n]` for every `k` whose point falls inside the
 * modulus -- which is what the two divisions compute -- and the one chosen
 * minimises the square of `state0 + point + state1`.
 *
 * THE FOURTH SYMBOL IS DIFFERENT and the object says so twice, at the bounds
 * and again in the body: its index is doubled and carries the parity of the
 * three indices already chosen plus the caller's `b`, and its interval is
 * over twice the step.  That is the 4D trellis's coset constraint, spent on
 * the last of the four.
 *
 * THE CONSTELLATION IS SYMMETRIC AND ONLY HALF OF IT IS STORED.  A negative
 * index reads `table[-j - 1]` and negates it (`lea 0x4(,%ecx,4); neg; fchs`),
 * so entry 0 is the smallest positive point and there is no zero.  The
 * elements convert with `push $0; push v; fildll`, which is unsigned.
 *
 * `1e12` is the initial minimum, `flds` from .rodata.cst4 -- a float and not
 * a double, so it is written here as one.
 */
/*
 * THE WARNING IS THE POINT, so it is silenced here and recorded rather than
 * repaired: the two locals below are read on a path that may not have written
 * them, exactly as +0x18 and +0x1c are in the object.  Initialising them
 * would be a different function.  The pragma is GCC 4 syntax and this tree's
 * gcc-3.4.3 ignores it with a warning of its own, which `make similarity`
 * tolerates -- see include/dsplib/ResamplerTiming.h for the same trade.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
void
V92Precoder::process(unsigned int *in, int a, int b, int *out, float *outf)
{
	int i;
	float bestPoint;	/* the chosen point, +0x18 in the frame */
	float bestSum;		/* and the sum it produced, +0x1c       */

	for (i = 0; i < 4; i++) {
		/*
		 * UNSIGNED, because the object divides by six with `mul
		 * $0xaaaaaaab; shr $2` and a signed `n` would have produced
		 * `imul $0x2aaaaaab` and a sign fixup.  Every value the only
		 * caller can pass is non-negative, so the two agree on
		 * everything reachable; the width is what the object says.
		 */
		unsigned int n = (unsigned int)(i + 4 * a);
		int sel = paramsAt9c[n % 6];
		int x = (int)in[n];
		int m = 2 * tableB[sel];
		const unsigned int *table = head[sel];
		int step = tableA[n];
		int lo, hi, k;

		/*
		 * `long double`, and the difference is measurable.  The object
		 * loads 1e12 with `flds` -- a float constant -- and then keeps
		 * the running minimum ON THE x87 STACK for the whole search
		 * (`fstp %st(5)` writes it back into a register, never to
		 * memory), so every comparison is at 64-bit mantissa.  Spelt
		 * `float` here it is rounded to 24 bits on each update, and
		 * thirty-five of this file's 2,374 comparisons then choose a
		 * different point among near-equal candidates.  The initial
		 * value is written as a float constant because that is the
		 * one the object holds; widening it is exact.
		 */
		long double best = 1e12f;

		if (i > 2) {
			int parity = (out[0] + out[1] + out[2] + b) & 1;

			lo = -((2 * x + m / 2 + parity) / (2 * step));
			hi = ((m - 1) / 2 - 2 * x) / (2 * step);
		} else {
			lo = -((m / 2 + x) / step);
			hi = ((m - 1) / 2 - x) / step;
		}

		for (k = lo; k <= hi; k++) {
			/* Both on the x87 stack from the `fildll` to the
			 * `fstps`, so both are extended and neither is
			 * rounded before the comparison. */
			long double point, sum;
			int j;

			if (i > 2) {
				int parity = (out[0] + out[1] + out[2] + b)
					     & 1;

				j = 2 * (k * step + x) + parity;
			} else {
				j = k * step + x;
			}

			point = (j < 0) ? -(long double)table[-j - 1]
					: (long double)table[j];
			sum = state0 + point + state1;

			if (sum * sum < best) {
				best = sum * sum;
				bestSum = (float)sum;
				bestPoint = (float)point;
				out[i] = j;
			}
		}

		/*
		 * AND THEY ARE READ WHETHER OR NOT THE SEARCH FOUND ANYTHING.
		 * An empty interval leaves both locals holding the previous
		 * symbol's values, or nothing at all on the first -- and
		 * `out[i]` unwritten.  docs/deviations.md D261.
		 */
		if (taps1 != 0)
			state0 = fir1->process(bestPoint);
		if (taps2 != 0)
			state1 = fir2->process(bestSum);

		outf[i] = bestSum;
	}
}
#pragma GCC diagnostic pop

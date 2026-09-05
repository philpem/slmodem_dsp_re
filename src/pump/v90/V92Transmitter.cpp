/*
 * V92Transmitter.cpp -- the V.92 transmit chain: construction, destruction,
 * reset and the frame loop.
 *
 * Reconstructed from dsplibs.o.  Six symbols, 3,222 bytes:
 *
 *     V92Transmitter::V92Transmitter()   .text+0x53b90 (C1), +0x53c50 (C2)
 *     V92Transmitter::~V92Transmitter()  .text+0x53a30 (D2), +0x53ae0 (D1)
 *     V92Transmitter::reset(V92MappingParams *)    .text+0x53d10
 *     V92Transmitter::process(unsigned char *, unsigned int, short *,
 *                             unsigned int &)      .text+0x54590
 *
 * C1 and C2 are byte-identical bar the register allocation of two argument
 * setups, and so are D1 and D2; GCC emits both from one definition.
 *
 * `include/dsplib/V92Transmitter.h` carries the object map, the 0x60 the
 * allocation gives and the evidence for every field.
 *
 * THE FOUR SUB-OBJECTS ARE BUILT WITH ORDINARY PLACEMENT `new`.  This file
 * used to reach all four constructors through hand-mangled `asm("_ZN...")`
 * labels, on the belief (finding F1340) that a user-declared placement
 * `operator new` would make GCC emit a null test the blob does not have.
 * Finding F10155 retracts that: the check is tied to a `throw()`-declared
 * placement operator, `-fcheck-new` was never in this project's flags, and
 * `include/dsplib/sysdep.h`'s shared non-throw placement `operator new`
 * reproduces the blob's construct-then-check-later shape with no flag
 * changes, verified under the real period compiler (finding F10157).  Each
 * destructor still goes through the explicit destructor-call syntax, which
 * needs no header.
 *
 * THE PRECEDING PARAGRAPH USED TO READ "the instruction sequence is the
 * blob's either way", AND IT IS WITHDRAWN FOR THE DESTRUCTOR (finding
 * F7816).  It holds for `new`, either spelling.  It is FALSE for a free in
 * TAIL POSITION: the delete-expression emits an ordinary `call sysdep_free`
 * and the open-coded `p->~T(); sysdep_free(p)` emits a sibling `jmp`,
 * dropping the frame with it.  So the destructors below use `delete` over an
 * inline replacement `operator delete`, and that is what makes them
 * byte-identical.  refinement.md lever 7.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL, `this` as the first stack argument
 * (`mov 0x20(%esp),%esi` after two pushes and a 0x14-byte frame), so nothing
 * here needs an attribute -- finding F215.
 */

#include <stddef.h>

#include "dsplib/V92Transmitter.h"
#include "dsplib/sysdep.h"

/*
 * THE REPLACEMENT `operator delete`, AND IT IS READ OFF THE OBJECT.  The blob
 * frees each owned sub-object with `if (p) { T::~T(p); sysdep_free(p); }`,
 * which is what GCC emits for `delete p` when `operator delete` is an inline
 * wrapper over `sysdep_free` -- and the blob defines and references no
 * `_ZdlPv` at all, so the codebase replaced the global operator.
 *
 * WRITING IT OUT BY HAND IS NOT EQUIVALENT, and that is the whole of finding
 * F7816's correction to this file's own older comment.  At the destructor's
 * LAST free the delete-expression emits an ordinary `call sysdep_free`; the
 * open-coded `p->~T(); sysdep_free(p);` emits a sibling `jmp` and drops the
 * frame with it.  refinement.md lever 7.
 */
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
 * And the array form, for the POD buffers.  Same evidence, same finding.
 */
inline void operator delete[](void *p) { sysdep_free(p); }
#include "dsplib/V92ModulusEncoder.h"
#include "dsplib/V92ConvolutionEncoder.h"
#include "dsplib/V92Precoder.h"
#include "dsplib/V92PreFilter.h"
#include "dsplib/V92ParamsInfo.h"
#include "dsplib/debug.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` parses
 * `struct name {` out of include/dsplib and cannot see a C++ class, so the
 * class asserts its own (finding F230).  Guarded on a 32-bit pointer because
 * every field from +0x08 on is a pointer or comes after one; `make check64`
 * compiles this file for the host to prove the CODE does not depend on 32-bit.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V92TX_OFF(field, off, tag) \
	typedef char v92tx_off_##tag[ \
	    ((int)__builtin_offsetof(V92Transmitter, field) == (off)) \
	    ? 1 : -1]

V92TX_OFF(pad_00,		0x00, pad00);
V92TX_OFF(K,			0x04, k);
V92TX_OFF(bitBuffer,		0x08, buf08);
V92TX_OFF(bitsBuffered,		0x0c, word0c);
V92TX_OFF(modulusOut,		0x10, pad10);
V92TX_OFF(convEncoderOutput,	0x40, word40);
V92TX_OFF(gain,			0x44, gain);
V92TX_OFF(modulusEncoder,	0x48, modenc);
V92TX_OFF(precoder,		0x4c, precoder);
V92TX_OFF(preFilter,		0x50, prefilter);
V92TX_OFF(convolutionEncoder,	0x54, convenc);
V92TX_OFF(byte_58,		0x58, byte58);
V92TX_OFF(pad_5c,		0x5c, pad5c);

typedef char v92tx_size[(sizeof(V92Transmitter) == 0x60) ? 1 : -1];

/*
 * The five sizes the constructor allocates are the five classes' own, and
 * every one of them is already pinned by its own header from its own
 * allocation site.  Asserting them here is what makes `sizeof(X)` below a
 * faithful transcription of the blob's literal rather than a hope.
 */
typedef char v92tx_modenc_size[(sizeof(V92ModulusEncoder) == 0x54) ? 1 : -1];
typedef char v92tx_convenc_size[
	(sizeof(V92ConvolutionEncoder) == 0x2008) ? 1 : -1];
typedef char v92tx_precoder_size[(sizeof(V92Precoder) == 0x80) ? 1 : -1];
typedef char v92tx_prefilter_size[(sizeof(V92PreFilter) == 0x14) ? 1 : -1];

#endif /* 32-bit */

/*
 * ===========================================================================
 * V92Transmitter::V92Transmitter (.text+0x53b90 / +0x53c50, 180 bytes)
 *
 * Six allocations and three stores of zero, in the object's own order:
 *
 *     malloc(0x50)  -> +0x08 ; +0x0c = 0 ; +0x04 = 0
 *     malloc(0x54)  -> V92ModulusEncoder()      -> +0x48
 *     malloc(1)     -> *p = 0                   -> +0x58
 *     malloc(0x2008)-> V92ConvolutionEncoder()  -> +0x54
 *     malloc(0x80)  -> V92Precoder(0x140)       -> +0x4c
 *     malloc(0x14)  -> V92PreFilter(0x140)      -> +0x50
 *
 * The two zero stores sit BETWEEN the first allocation and the second, which
 * is why they are written where they are: any other placement is a mutation
 * the store-order comparison cannot see, and the ordering is recorded here
 * because it is the object's, not because a test decides it.
 *
 * NOT ONE ALLOCATION IS CHECKED, the blob's included -- a null return walks
 * straight into the constructor call.  See docs/deviations.md.
 * ===========================================================================
 */
V92Transmitter::V92Transmitter()
{
	void *p;

	bitBuffer = (unsigned char *)sysdep_malloc(V92TX_BUF08_BYTES);
	bitsBuffered = 0;
	K = 0;

	/*
	 * Every constructor below is the C1 -- the complete-object variant a
	 * `new` expression uses -- by the names the blob's relocations carry.
	 */
	p = sysdep_malloc(sizeof(V92ModulusEncoder));
	new (p) V92ModulusEncoder();
	modulusEncoder = (V92ModulusEncoder *)p;

	p = sysdep_malloc(1);
	*(unsigned char *)p = 0;
	byte_58 = (unsigned char *)p;

	p = sysdep_malloc(sizeof(V92ConvolutionEncoder));
	new (p) V92ConvolutionEncoder();
	convolutionEncoder = (V92ConvolutionEncoder *)p;

	p = sysdep_malloc(sizeof(V92Precoder));
	new (p) V92Precoder(V92TX_FILTER_TAPS);
	precoder = (V92Precoder *)p;

	p = sysdep_malloc(sizeof(V92PreFilter));
	new (p) V92PreFilter(V92TX_FILTER_TAPS);
	preFilter = (V92PreFilter *)p;
}

/*
 * ===========================================================================
 * V92Transmitter::~V92Transmitter (.text+0x53a30 / +0x53ae0, 173 bytes)
 *
 * Six null-guarded releases, in an order that is NOT the constructor's: the
 * precoder, the pre-filter and the convolution encoder come back in a
 * different sequence from the one they were built in.  Three of the six get a
 * destructor call and three do not, and which three is the header's subject.
 *
 * `precoder = 0` after its free is the object's own and the only one of its
 * kind here; leaving the other five dangling is D210.  The null tests
 * themselves are not decoration and not free to omit -- this tree's
 * `sysdep_free` tolerates NULL, so dropping one leaves every byte of the
 * object unchanged and moves only `harness_alloc.free_null`, which is why
 * t_v92tx.cpp asserts that counter over all sixty-four null combinations.
 *
 * THE DEFINITION STAYS BELOW THE CONSTRUCTOR, AND THAT IS A MEASURED CHOICE
 * RATHER THAN THE OBJECT'S ORDER.  The blob emits this file D2, D1, C1, C2,
 * reset, process; we emit C2, C1, D2, D1, reset, process.  Moving this whole
 * block above the constructor -- refinement.md lever 3 -- reaches D2, D1, C2,
 * C1 and is a NET LOSS OF ONE: D1 gains byte identity (its two residual bytes
 * are a `pop %edx` where we emit `pop %eax`, which is peephole2's
 * esp-adjust-to-pop pattern taking a scratch off the round-robin cursor, so
 * D1 is EXPOSED in lever 3b's sense) and C1 and C2 both LOSE theirs, because
 * the cursor state now arriving at them is the destructors' and not the file
 * head's.  The blob's C1-before-C2 clone order is not reachable from here at
 * all: GCC 3.4.2 emits this class's constructor clones C2-first whatever the
 * source says, so the file cannot hold both.  Reverted, and the measurement
 * kept -- 7797's ruling.  Finding F7842.
 * ===========================================================================
 */
V92Transmitter::~V92Transmitter()
{
	delete[] bitBuffer;

	/*
	 * `modulusEncoder` KEEPS THE EXPLICIT FREE.  The blob makes no
	 * destructor call on it, and `delete` would invent one if
	 * `V92ModulusEncoder` ever gains a non-trivial destructor.  Nothing
	 * may add a call the object does not make.
	 */
	if (modulusEncoder != 0)
		sysdep_free(modulusEncoder);

	delete[] byte_58;

	/*
	 * THE ZERO IS INSIDE THE GUARD, and the object says so: the blob's
	 * `movl $0x0,0x4c(%esi)` sits in the taken arm, after the
	 * `call sysdep_free`, where ours had it on the fall-through path.  The
	 * outer test is redundant to `delete` and GCC 3.4.2 folds it, so this
	 * costs no instruction; what it buys is the store's basic block.
	 * Five spellings compiled on the period compiler -- bare
	 * `delete; zero`, this one, `!= 0` and the implicit test, the
	 * open-coded `~V92Precoder(); sysdep_free()` form, and the guard with
	 * the zero left outside -- giving three distinct emissions, of which
	 * exactly one reaches positional byte identity.  D2 goes EXACT and D1
	 * from 46 differing bytes to 2.  Finding F7841.
	 */
	if (precoder != 0) {
		delete precoder;
		precoder = 0;
	}

	delete preFilter;

	delete convolutionEncoder;
}

/*
 * The float-as-%c%d.%07d idiom, the same three helpers
 * src/pump/v90/V92EchoCanceller.cpp carries.  The scale is 1e7 here and not
 * 1e6 -- `.rodata.cst4` +0x4f0 is 10000000.0 and the five format strings all
 * say %07d.  `frac_of`'s subtraction is `v - (int)v`, which is what `de e1`
 * at .text+0x53e3d does: objdump prints it `fsubp %st,%st(1)` and it IS
 * FSUBRP, so st(1) becomes st(0) - st(1) and st(0) holds the value (finding
 * F245).  The abs() makes the order unobservable (finding F256).
 */
static char
sign_of(float v)
{
	return (0.0f < v) ? '+' : '-';
}

static int
whole_of(float v)
{
	return (int)__builtin_fabsf(v);
}

static int
frac_of(float v)
{
	return __builtin_abs((int)(((long double)v - (long double)(int)v)
				   * 1.0e7f));
}

/*
 * ===========================================================================
 * V92Transmitter::reset (.text+0x53d10, 2,161 bytes)
 *
 * WHAT IT ACTUALLY DOES IS 170 BYTES OF IT.  Two words in from the parameter
 * block, one byte cleared through the one-byte buffer at +0x58, six calls,
 * and two words zeroed on the way out.  Everything between is diagnostics --
 * a header, three banners, the general parameters, and a per-coefficient dump
 * of all four filters -- and every one of the fifty-odd prints reloads
 * `dsplibs_debug_level` for itself, so each gets its own `if
 * (DSPLIB_DEBUG_ON())` and they are not collapsed.
 *
 * THE PARAMETER BLOCK IS `struct V92ParamsInfo` UNDER ANOTHER NAME.  The
 * mangling says `V92MappingParams`; the layout is the one
 * include/dsplib/V92ParamsInfo.h maps, and the cast below is the same one
 * V92Precoder.cpp and V92ModulusEncoder.cpp already make.  This function is
 * half of what proves they are one block: it prints +0x4c, +0x50, +0x54,
 * +0x58 and +0x14 under the very names the unpacker fills them from.
 *
 * WHICH FILTER GETS WHICH PAIR IS FORCED BY THE MANGLING.  The precoder is
 * given (z1, p1, lz1, lp1) and the pre-filter (z2, p2, lz2, lp2), and
 * `setCoefficients` is `EPfS0_jj` on both -- two `float *` and two
 * `unsigned` -- which is what makes the four block pointers `float *` and the
 * four lengths unsigned rather than a guess from the dump loops.
 *
 * THE FOUR DUMP BLOCKS ARE NOT INSIDE A DEBUG GATE, only their contents are:
 * `if (p->lz1 != 0)` at .text+0x53dca is reached from both sides of the
 * level test above it.  So an empty filter prints no rule, and a non-empty
 * one at level 0 costs a load and a branch.  That is the object's shape and
 * it is reproduced.
 * ===========================================================================
 */
void
V92Transmitter::reset(V92MappingParams *params)
{
	struct V92ParamsInfo *p = (struct V92ParamsInfo *)params;
	unsigned int i;

	gain = p->gain;
	K = p->K;

	modulusEncoder->reset(params);

	*byte_58 = 0;

	precoder->reset(params);
	precoder->setCoefficients(p->z1, p->p1, p->lz1, p->lp1);

	preFilter->reset();
	preFilter->setCoefficients(p->z2, p->p2, p->lz2, p->lp2);

	convolutionEncoder->reset(p->trellisType);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("On V92 Transmitter reset, here are "
				     "modulation parameters:\r\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("================== General ============" "======\r\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Gain = %c%d.%07d\r\n", sign_of(gain),
				     whole_of(gain), frac_of(gain));
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("K = %d\r\n", K);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("trellisType = %d\r\n", p->trellisType);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("extendEu = %d\r\n", p->extendEu);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("m0 = %d\r\n", p->m[0]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("m1 = %d\r\n", p->m[1]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("m2 = %d\r\n", p->m[2]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("m3 = %d\r\n", p->m[3]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("m4 = %d\r\n", p->m[4]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("m5 = %d\r\n", p->m[5]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("m6 = %d\r\n", p->m[6]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("m7 = %d\r\n", p->m[7]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("m8 = %d\r\n", p->m[8]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("m9 = %d\r\n", p->m[9]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("m10 = %d\r\n", p->m[10]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("m11 = %d\r\n", p->m[11]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("================== Pre Coder ==========" "======\r\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("lz1 = %d\r\n", p->lz1);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("lp1 = %d\r\n", p->lp1);

	if (p->lz1 != 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("---------------------------" "\r\n");
		for (i = 0; i < p->lz1; i++)
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("z1[%d] = %c%d.%07d\r\n",
						     i, sign_of(p->z1[i]),
						     whole_of(p->z1[i]),
						     frac_of(p->z1[i]));
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("---------------------------" "\r\n");
	}

	if (p->lp1 != 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("---------------------------" "\r\n");
		for (i = 0; i < p->lp1; i++)
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("p1[%d] = %c%d.%07d\r\n",
						     i, sign_of(p->p1[i]),
						     whole_of(p->p1[i]),
						     frac_of(p->p1[i]));
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("---------------------------" "\r\n");
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("================== Pre Filter =========" "======\r\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("lz2 = %d\r\n", p->lz2);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("lp2 = %d\r\n", p->lp2);

	if (p->lz2 != 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("---------------------------" "\r\n");
		for (i = 0; i < p->lz2; i++)
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("z2[%d] = %c%d.%07d\r\n",
						     i, sign_of(p->z2[i]),
						     whole_of(p->z2[i]),
						     frac_of(p->z2[i]));
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("---------------------------" "\r\n");
	}

	if (p->lp2 != 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("---------------------------" "\r\n");
		for (i = 0; i < p->lp2; i++)
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("p2[%d] = %c%d.%07d\r\n",
						     i, sign_of(p->p2[i]),
						     whole_of(p->p2[i]),
						     frac_of(p->p2[i]));
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("---------------------------" "\r\n");
	}

	convEncoderOutput = 0;
	bitsBuffered = 0;
}

/*
 * ===========================================================================
 * V92Transmitter::process (.text+0x54590, 355 bytes)
 *
 * The frame loop, and the only member of this class that produces samples.
 * It is `V92BitsToSymbol::process`'s worker and has no other caller.
 *
 * ONE BIT IN, ONE BYTE OF `bitBuffer`.  The input is bytes -- `movzbl
 * (%ebx,%ebp,1)` -- and every one of them is stored whole into the buffer at
 * `bitsBuffered`, so whatever the caller packs into them travels unexamined.
 * Nothing masks, shifts or tests the value here.
 *
 * `K` IS THE FRAME'S APPETITE, and the comparison against it is UNSIGNED
 * (`cmp 0x4(%esi),%ecx; jb`), which `int K` against an `unsigned` counter
 * gives for free.  When the count reaches it a frame comes out and `K` is
 * SUBTRACTED rather than the count cleared, so an input that overshoots
 * carries its tail into the next frame.  The blob never checks `bitsBuffered`
 * against the 0x50 bytes the buffer has; a `K` above 80 walks off the end,
 * and that is reproduced.  docs/deviations.md D501.
 *
 * THE FOUR RELOADS ARE ALIASING AND NOT SLOPPINESS.  `bits`, `bitBuffer` and
 * `bitsBuffered` are all re-read on every iteration (.text+0x545d2, +0x545d9,
 * +0x545e3) because the store through `unsigned char *` may touch any of
 * them.  Written straight, GCC emits exactly those reloads.
 *
 * THE FRAME, in the object's order:
 *
 *     modulusEncoder->progress(bitBuffer, modulusOut)
 *     for k = 0, 1, 2:                          `cmp $0x2,%edi; jbe`
 *         precoder->process(modulusOut, k, convEncoderOutput,
 *                           precoded, &points[4 * k])
 *         convEncoderOutput = convolutionEncoder->process(precoded)
 *     preFilter->process(points, shaped)
 *     for j = 0 .. 11:  out[n + j] = (short)(shaped[j] * gain)
 *     n += 12
 *
 * `k` IS UNSIGNED AND THAT IS FORCED: the bound is `jbe`, where a signed
 * counter would have been `jle`.  It is passed into a parameter the mangling
 * types `int`, so the conversion is at the call and costs nothing.
 *
 * THE CAST TO `short` IS THE OBJECT'S OWN, not an inference from the output
 * type: `fistps` is a 16-bit store, and it is bracketed by `fldcw` of a
 * control word the function builds itself with `or $0xc00,%bx` -- round
 * toward zero -- which is precisely what a C cast from floating point to
 * integer requires and what the default rounding mode does not give.  The
 * gain is loaded ONCE, before the loop, and multiplied in from %st(1).
 *
 * `nout` IS SET ON EVERY PATH, the `nbits == 0` one included: the count
 * starts at zero and the store at .text+0x546e6 is after the join.
 * ===========================================================================
 */
void
V92Transmitter::process(unsigned char *bits, unsigned int nbits, short *out,
			unsigned int &nout)
{
	unsigned int n = 0;
	unsigned int i;

	for (i = 0; i < nbits; i++) {
		/*
		 * THE BUFFER GOES THROUGH A LOCAL, AND IT IS THE OBJECT'S
		 * ADDRESSING MODE THAT SAYS SO.  The blob stores with
		 * `mov %al,(%ecx,%edi,1)` -- base `bitsBuffered`, index
		 * `bitBuffer` -- and `bitBuffer[bitsBuffered]` gives the two
		 * the other way round.  Both are COMPONENT_REFs, so
		 * `tree_swap_operands_p` canonicalises the PLUS; reading one
		 * into a local makes it a DECL and the swap stops, which is
		 * refinement.md lever 9's own remedy.  Scale is 1, so nothing
		 * about the types forces which register is the base.
		 */
		unsigned char *buf = bitBuffer;

		buf[bitsBuffered] = bits[i];
		bitsBuffered++;

		if (bitsBuffered >= (unsigned int)K) {
			/*
			 * DECLARED points, shaped, precoded -- the frame slots
			 * are the object's, not a preference: the blob builds
			 * the pre-filter's argument with `lea 0x70(%esp),%ebx`
			 * where our old order gave `lea 0x30(%esp)`.  The
			 * cross product of all 3! declaration orders with four
			 * spellings of the store above was compiled on the
			 * period compiler -- 24 cells, TWELVE distinct
			 * emissions, exactly ONE at positional byte identity.
			 * The table separates the two facts: the declaration
			 * order alone takes 110 differing bytes of 355 to ONE,
			 * and the local closes that one.  Finding F7847.
			 */
			float points[V92TX_FRAME_SYMBOLS];
			float shaped[V92TX_FRAME_SYMBOLS];
			int precoded[V92TX_PRECODER_SYMBOLS];
			unsigned int k;
			unsigned int j;

			modulusEncoder->progress(bitBuffer, modulusOut);

			for (k = 0; k < V92TX_PRECODER_STEPS; k++) {
				precoder->process(modulusOut, (int)k,
						  convEncoderOutput, precoded,
						  &points[V92TX_PRECODER_SYMBOLS
							  * k]);
				convEncoderOutput =
					convolutionEncoder->process(precoded);
			}

			preFilter->process(points, shaped);

			for (j = 0; j < V92TX_FRAME_SYMBOLS; j++)
				out[n + j] = (short)(shaped[j] * gain);

			n += V92TX_FRAME_SYMBOLS;

			bitsBuffered -= K;
		}
	}

	nout = n;
}

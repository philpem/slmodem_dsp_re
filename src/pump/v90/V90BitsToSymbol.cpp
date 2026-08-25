/*
 * V90BitsToSymbol.cpp -- V90BitsToSymbol's constructor and destructor.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90BitsToSymbol.h` carries
 * the object map and the argument for the 0x24 size; this file is the two
 * functions and the assertions that hold the compiler to that map.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x20(%esp),%ebx` after a 0x1c-byte frame -- not %ecx, so
 * these are not thiscall and nothing here needs an attribute (finding 215).
 *
 * WHY THE MAPPER IS BUILT THROUGH AN asm() LABEL RATHER THAN `new`.  The
 * blob's constructor is
 *
 *     movl $0x704,(%esp) ; call sysdep_malloc ; call V90Mapper::V90Mapper
 *
 * with no null check between the two, and its destructor is
 *
 *     if (p) { V90Mapper::~V90Mapper(p); sysdep_free(p); }
 *
 * -- which is exactly what GCC emits for `new V90Mapper(...)` and `delete p`
 * when `operator new` and `operator delete` are inline wrappers over
 * sysdep_malloc and sysdep_free.  That is almost certainly the original's
 * source.  It is not what this file can write: the build is `-nostdinc++`,
 * there is no <new>, and C++ has no other syntax for running a constructor
 * over storage that already exists.  Declaring a replacement global
 * `operator new` inline is ill-formed, and a user-declared PLACEMENT form
 * makes GCC emit the null test the blob does not have.  So the constructor
 * calls V90Mapper's by its mangled name and the destructor uses the explicit
 * destructor call, which needs no trick at all.  The instruction sequence is
 * the blob's either way; only the spelling differs.  src/pump/v90/
 * V92Precoder.cpp reaches the same conclusion for FloatFIR.
 *
 * THE LOCAL `m` MATTERS.  The blob keeps the fresh pointer in a register
 * across the constructor call and stores it to +0x00 AFTERWARDS; assigning
 * the member first and passing the member would make GCC store, then reload
 * across the call, because it cannot prove the allocation does not alias
 * `this`.  Same reason the mapper's second argument reads `params` back out
 * of +0x04 rather than reusing the incoming register: the blob does, so this
 * does.
 */

#include <stddef.h>

#include "dsplib/debug.h"
#include "dsplib/sysdep.h"
#include "dsplib/V90BitsToSymbol.h"

/*
 * THE REPLACEMENT `operator delete`, AND IT IS READ OFF THE OBJECT.  The blob
 * frees each owned sub-object with `if (p) { T::~T(p); sysdep_free(p); }`,
 * which is what GCC emits for `delete p` when `operator delete` is an inline
 * wrapper over `sysdep_free` -- and the blob defines and references no
 * `_ZdlPv` at all, so the codebase replaced the global operator.
 *
 * WRITING IT OUT BY HAND IS NOT EQUIVALENT, and that is the whole of finding
 * 7816's correction to this file's own older comment.  At the destructor's
 * LAST free the delete-expression emits an ordinary `call sysdep_free`; the
 * open-coded `p->~T(); sysdep_free(p);` emits a sibling `jmp` and drops the
 * frame with it.  refinement.md lever 7.
 */
extern "C" void sysdep_free(void *p);
inline void operator delete(void *p) { sysdep_free(p); }

/*
 * AND THE SIZED FORM, FOR THE MODERN BUILD ONLY.  C++14 added
 * `operator delete(void *, size_t)`, and GCC 13 calls it for `delete p` on a
 * class with a destructor -- an undefined `_ZdlPvj` in a tree that links no
 * libstdc++, which is the link failure `dsplib/Resampler.h` documents.
 *
 * `__cplusplus >= 201402L` is FALSE under GCC 3.4.2 (199711L), so the compiler
 * that decides byte identity never sees this.  It is portability plumbing and
 * carries no claim about the object.
 */
#if defined(__cplusplus) && __cplusplus >= 201402L
inline void operator delete(void *p, __SIZE_TYPE__) { sysdep_free(p); }
#endif

/*
 * NO LOCAL `operator delete[]` HERE.  This file reaches `dsplib/Scrambler.h`,
 * which carries the one definition, and a second is a redefinition error --
 * finding 7815, where that loud failure is the point.
 */
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Mapper.h"

extern "C" {
/*
 * V90Mapper's constructor, by the name the blob calls.  C1 is the
 * complete-object variant, which is what a `new` expression uses and what the
 * relocation at 0x2f766 names.  `sizeof(V90Mapper)` and not the literal 0x704
 * is what the allocation is spelled with, so the two cannot drift apart.
 */
void v90bts_mapper_ctor(void *self, V90Parameters *params)
	asm("_ZN9V90MapperC1EP13V90Parameters");
}

#if __SIZEOF_POINTER__ == 4
#define V90BTS_OFF(field, off, tag) \
	typedef char v90bts_off_##tag[ \
	    ((int)__builtin_offsetof(V90BitsToSymbol, field) == (off)) ? 1 : -1]

V90BTS_OFF(mapper,		0x00, mapper);
V90BTS_OFF(params,		0x04, params);
V90BTS_OFF(symbols,		0x08, symbols);
V90BTS_OFF(nofSymbols,		0x0c, nofsym);
V90BTS_OFF(symbolsDone,		0x10, done);
V90BTS_OFF(bitsPerFrame,	0x14, bpf);
V90BTS_OFF(extraSymbols,	0x18, extra);
V90BTS_OFF(symbolsBlockSize,	0x1c, block);
V90BTS_OFF(extraSymbolsPending,	0x20, pending);
typedef char v90bts_size[(sizeof(V90BitsToSymbol) == 0x24) ? 1 : -1];
#endif

/*
 * ===========================================================================
 * V90BitsToSymbol::V90BitsToSymbol -- .text+0x2f730 (C2) and +0x2f7a0 (C1),
 * 112 bytes each.
 *
 * `bitsPerFrame` at +0x14 and `extraSymbols` at +0x18 are DELIBERATELY not
 * written: the blob leaves both untouched and the first `reset` fills them
 * in.  Neither allocation is null-checked.
 * ===========================================================================
 */
V90BitsToSymbol::V90BitsToSymbol(unsigned int n, V90Parameters *p)
{
	V90Mapper *m;

	params = p;
	m = (V90Mapper *)sysdep_malloc(sizeof(V90Mapper));
	v90bts_mapper_ctor(m, params);
	mapper = m;
	symbols = (short *)sysdep_malloc(2 * n);
	nofSymbols = n;
	symbolsDone = 0;
	symbolsBlockSize = 0;
	extraSymbolsPending = 1;
}

/*
 * ===========================================================================
 * V90BitsToSymbol::~V90BitsToSymbol -- .text+0x2f810 (D2) and +0x2f870 (D1),
 * 84 bytes each.
 *
 * Neither pointer is nulled after being released, so a second destruction
 * double-frees; that is the blob's behaviour and is left alone.
 * ===========================================================================
 */
V90BitsToSymbol::~V90BitsToSymbol()
{
	delete mapper;
	delete[] symbols;
}

/*
 * ===========================================================================
 * `V90BitsToSymbol::nofBitsForNextTime` -- 134 bytes at 0x2f980.
 *
 * HOW MANY BITS THE CALLER MUST HAND OVER TO FILL THE REST OF THE BLOCK.
 * The symbols still owed are `symbolsBlockSize - symbolsDone`, plus
 * `extraSymbols` while `extraSymbolsPending` is set; six symbols make a
 * frame, and each frame costs `bitsPerFrame` bits.
 *
 * THE TWO ARMS ARE NOT THE SAME EXPRESSION, and that is why they are written
 * out rather than folded into one ceiling.  When the count divides by six the
 * blob multiplies FIRST and divides afterwards -- `imul %ebx,%ecx` then
 * `mul $0xaaaaaaab` / `shr $2` on the product -- and when it does not, it
 * divides first and multiplies the incremented quotient.  The two agree for
 * every value either can be given, so no test can separate them; what
 * separates them is that the object holds both.
 * ===========================================================================
 */
unsigned int
V90BitsToSymbol::nofBitsForNextTime()
{
	unsigned int wanted;

	if (symbolsBlockSize <= symbolsDone)
		return 0;

	wanted = symbolsBlockSize - symbolsDone;
	if (extraSymbolsPending)
		wanted += extraSymbols;

	if (wanted % 6 != 0)
		return (wanted / 6 + 1) * bitsPerFrame;
	return wanted * bitsPerFrame / 6;
}

/*
 * ===========================================================================
 * `V90BitsToSymbol::setSymbolsBlockSize` -- 137 bytes at 0x2fa10.
 *
 * ONE STORE AND A CALL THAT IS NOT THERE.  The blob stores the argument into
 * +0x1c and then runs `nofBitsForNextTime`'s body inline: the same
 * `cmp`/`jbe`, the same `cmpb $0x0,0x20`, the same reciprocal divide and the
 * same two arms, with no `call` anywhere in the 137 bytes.  Three bytes
 * longer than the callee it inlines, which is the store.
 * ===========================================================================
 */
unsigned int
V90BitsToSymbol::setSymbolsBlockSize(unsigned int blockSize)
{
	symbolsBlockSize = blockSize;
	return nofBitsForNextTime();
}

/*
 * ===========================================================================
 * `V90BitsToSymbol::process(unsigned int &, short *)` -- 375 bytes at
 * 0x2fd50.  The other two `process` overloads are NOT written here.
 *
 * THE ANSWER IS A STATUS AND THE STRINGS NAME BOTH OF ITS NON-ZERO VALUES.
 * 1 goes with "SIZE_NOT_SET" and 3 with "BUFFER_UNDERFLOW"; 0 is the silent
 * path and has no message, so the object names two of the three and the
 * third is what is left.
 *
 * THE SHIFT LOOP READS A FIELD THE UNDERFLOW ARM HAS JUST ZEROED, and that is
 * what the blob's constant-propagated `xor %esi,%esi` at 0x2fdf4 is: on the
 * arm that stored `symbolsDone = 0` the compiler knows the loop bound, so it
 * emits the zero rather than a reload, and on the other arm it uses the value
 * loaded at 0x2fdb0.  Two stores to +0x10 on that path -- the zero and then
 * the count of what was kept -- are therefore both in the source and neither
 * is redundant to the compiler.
 *
 * `if (extraSymbolsPending) extraSymbolsPending = 0;` IS THE OBJECT'S, not a
 * clumsy way to write a store.  `cmpb $0x0,0x20(%ebp) ; je ; movb $0x0` --
 * the test is there and a plain assignment would be one `movb`.  It is also
 * on the common path: the size-not-set arm reaches it too.
 * ===========================================================================
 */
unsigned int
V90BitsToSymbol::process(unsigned int &nofBits, short *outSymbols)
{
	unsigned int status = 0;

	if (symbolsBlockSize == 0) {
		status = 1;
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("V90BitsToSymbol - error: "
					     "process called, SIZE_NOT_SET"
					     "\r\n");
	} else {
		unsigned int i, kept;

		if (symbolsDone < symbolsBlockSize) {
			status = 3;
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf("V90BitsToSymbol - "
						     "error: process called, "
						     "BUFFER_UNDERFLOW\r\n");
			for (i = 0; i < symbolsDone; i++)
				outSymbols[i] = symbols[i];
			symbolsDone = 0;
		} else {
			for (i = 0; i < symbolsBlockSize; i++)
				outSymbols[i] = symbols[i];
		}

		kept = 0;
		for (i = symbolsBlockSize; i < symbolsDone; i++)
			symbols[kept++] = symbols[i];
		symbolsDone = kept;

		nofBits = nofBitsForNextTime();
	}

	if (extraSymbolsPending)
		extraSymbolsPending = 0;

	return status;
}

/*
 * ===========================================================================
 * `V90BitsToSymbol::process(unsigned char *, unsigned int)` -- 180 bytes at
 * 0x2fc90.  THE FILL, where the overload above is the drain.
 *
 * ONE CALL AND FOUR FIELD ACCESSES.  The bits are handed straight to the
 * mapper this class owns, together with a write pointer `symbols +
 * symbolsDone` -- `mov 0x10(%ebx),%eax ; mov 0x8(%ebx),%edx ; lea
 * (%edx,%eax,2),%ecx` -- and the address of a stack local for the count the
 * mapper produced.  `symbolsDone` then advances by that count.  So the two
 * overloads share `symbols` and `symbolsDone` and run in opposite
 * directions: this one appends, the other one hands out a block and shifts
 * the remainder down.
 *
 * THE STATUS IS 2 HERE AND 3 THERE, and the strings are why.  The object's
 * three messages are SIZE_NOT_SET at 0x85b4, BUFFER_OVERFLOW at 0x85ec and
 * BUFFER_UNDERFLOW at 0x8628; this function references the first two and the
 * other overload the first and the third.  0 remains the path with no
 * message.
 *
 * THE OVERFLOW TEST IS AGAINST `nofSymbols` AND THE CLAMP IS TO
 * `symbolsBlockSize`, which are two different fields and not a transcription
 * slip: `cmp 0xc(%ebx),%eax ; jbe` compares the new `symbolsDone` against the
 * BUFFER's capacity, and `mov 0x1c(%ebx),%eax ; mov %eax,0x10(%ebx)` on the
 * failing arm sets `symbolsDone` to the BLOCK size.  It is also written after
 * the store of the sum -- the blob stores `%eax` to +0x10 between the compare
 * and the branch -- so the overflowing value is briefly in the field and then
 * replaced.
 *
 * NOTHING IN THIS FUNCTION BOUNDS THE MAPPER'S WRITE.  The symbols are
 * already in the buffer by the time the capacity is looked at, so status 2 is
 * a report and not a guard, and no sequence of `reset` and `process` over a
 * properly constructed object can raise it without the mapper having already
 * written past the `2 * nofSymbols` allocation.  That is what makes it a
 * poked state in the fixture rather than a driven one -- finding 7430, the
 * same shape as 7422 and 7423.
 *
 * `if (extraSymbolsPending) extraSymbolsPending = 0;` IS THE OBJECT'S HERE
 * TOO, `cmpb $0x0,0x20(%ebx) ; je ; movb $0x0`, and it is on the common path
 * of all three arms exactly as in the other overload.
 * ===========================================================================
 */
unsigned int
V90BitsToSymbol::process(unsigned char *bits, unsigned int nofBits)
{
	unsigned int status = 0;

	if (symbolsBlockSize == 0) {
		status = 1;
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("V90BitsToSymbol - error: "
					     "process called, SIZE_NOT_SET"
					     "\r\n");
	} else {
		unsigned int nofOut;

		mapper->process(bits, nofBits, symbols + symbolsDone, nofOut);

		symbolsDone += nofOut;
		if (symbolsDone > nofSymbols) {
			status = 2;
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf("V90BitsToSymbol - "
						     "error: process called, "
						     "BUFFER_OVERFLOW\r\n");
			symbolsDone = symbolsBlockSize;
		}
	}

	if (extraSymbolsPending)
		extraSymbolsPending = 0;

	return status;
}

/*
 * ===========================================================================
 * `V90BitsToSymbol::process(unsigned char *, unsigned int &, short *)` -- 484
 * bytes at 0x2faa0.  THE THIRD OVERLOAD, and the one the transmit chain
 * actually calls: `V90Modulator::progress`'s data phase relocates against
 * `_ZN15V90BitsToSymbol7processEPhRjPs` and against neither sibling.
 *
 * IT IS THE FILL AND THE DRAIN IN ONE CALL and it is NOT a composition of the
 * other two.  Written as `process(bits, nofBits); return process(nofBits,
 * outSymbols);` the statuses would be wrong -- the drain's underflow test
 * would run after the fill's overflow clamp had already forced `symbolsDone`
 * up to `symbolsBlockSize`, so status 3 could never be reached and status 2
 * would be reported by a call that also had to report 3.  Here the three are
 * exclusive arms of one `if`/`else` over the SAME post-fill `symbolsDone`, so
 * this is the only member of the class that can answer any of the three.
 *
 * THE ARM ORDER IS THE OBJECT'S.  0x2fb50 compares the new `symbolsDone`
 * against `symbolsBlockSize` FIRST and takes the underflow arm below it;
 * `nofSymbols` -- the buffer's capacity -- is only looked at on the arm where
 * a whole block is ready.  So a short fill is diagnosed before an overrun,
 * which is why an underflowing call never reports 2 however far past the
 * allocation the mapper wrote.
 *
 * THE OVERFLOW CLAMP RELOADS `symbolsBlockSize` ACROSS THE PRINTF -- `mov
 * 0x1c(%ebp),%edi` at 0x2fc6b, after the call, where the pre-print value is
 * still in the same register.  The compiler cannot prove `dsplibs_debug_printf`
 * leaves `*this` alone.  Nothing about the source changes; it is worth
 * recording only because the reload is what makes the store at 0x2fc22 read
 * as `symbolsDone = symbolsBlockSize` rather than as a spilled temporary.
 *
 * `nofBitsForNextTime` IS INLINED HERE and is a `call` in `V90Modulator::
 * progress`, which is the same split the written siblings show: 0x2fbc1
 * onwards is the reciprocal divide and both of its arms, with no `call`
 * anywhere between the mapper and the return.
 *
 * THE UNDERFLOW ARM EMPTIES THE BUFFER, and then the shared shift loop runs
 * over the zero it just stored, moving nothing.  Two stores to +0x10 on that
 * path, exactly as the `(unsigned int &, short *)` sibling has -- and the
 * compiler's `xor %ebx,%ebx` for the second is constant propagation and not a
 * missing statement.
 * ===========================================================================
 */
unsigned int
V90BitsToSymbol::process(unsigned char *bits, unsigned int &nofBits,
			 short *outSymbols)
{
	unsigned int status = 0;

	if (symbolsBlockSize == 0) {
		status = 1;
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("V90BitsToSymbol - error: "
					     "process called, SIZE_NOT_SET"
					     "\r\n");
	} else {
		unsigned int i, kept, nofOut;

		mapper->process(bits, nofBits, symbols + symbolsDone, nofOut);

		symbolsDone += nofOut;

		if (symbolsDone < symbolsBlockSize) {
			status = 3;
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf("V90BitsToSymbol - "
						     "error: process called, "
						     "BUFFER_UNDERFLOW\r\n");
			for (i = 0; i < symbolsDone; i++)
				outSymbols[i] = symbols[i];
			symbolsDone = 0;
		} else {
			if (symbolsDone > nofSymbols) {
				status = 2;
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "V90BitsToSymbol - error: process "
					    "called, BUFFER_OVERFLOW\r\n");
				symbolsDone = symbolsBlockSize;
			}
			for (i = 0; i < symbolsBlockSize; i++)
				outSymbols[i] = symbols[i];
		}

		kept = 0;
		for (i = symbolsBlockSize; i < symbolsDone; i++)
			symbols[kept++] = symbols[i];
		symbolsDone = kept;

		nofBits = nofBitsForNextTime();
	}

	if (extraSymbolsPending)
		extraSymbolsPending = 0;

	return status;
}

/*
 * ===========================================================================
 * `V90BitsToSymbol::reset` -- 108 bytes at 0x2f8d0
 * `V90BitsToSymbol::resetNoSpectral` -- 58 bytes at 0x2f940
 *
 * THE MAPPER POINTER IS RELOADED FROM THE MEMBER, not carried in a register:
 * both functions push their two arguments and then `mov (%esi),%edx` to fetch
 * `this->mapper` (0x2f8ef, 0x2f95f).  Same reading as the constructor's second
 * argument -- the blob reads the field, so this does.
 *
 * `extraSymbols` IS AN UNSIGNED DIVIDE AND THE ZERO IS GUARDED.  The object
 * builds `6 * shaperId` with `lea (%eax,%eax,2)` and `add %eax,%eax`, then
 * `f7 f3  div %ebx` at 0x2f919 -- `div`, not `idiv`, although
 * `V90MappingParams::shaperSR` is declared `int`.  That is the same reading
 * `V90Mapper::reset` records for its own `6 / shaperSR` and the same one
 * `V90MAPPER_FRAME`'s `u` suffix exists for.  A zero `shaperSR` skips the
 * divide with `%eax` already cleared (`xor %eax,%eax` at 0x2f904, before the
 * test), so the answer is zero and not a trap.
 *
 * WHAT IT COUNTS is the symbols the mapper will swallow while its spectral
 * shaper primes: `V90Mapper::process` suppresses whole frames while +0x6f8
 * counts down and part of one at the end, `shaperId * signBitGroupSize` in
 * all, and `6 * shaperId / shaperSR` is that number FOR EVERY `shaperSR` THAT
 * DIVIDES SIX -- which is every value V.90 uses, and is where the shaper's own
 * block length comes from.  Two classes, two spellings, one quantity; finding
 * 7422 has the algebra and the case that separates them.
 *
 * `bitsPerFrame` AND `extraSymbols` ARE THE TWO FIELDS THE CONSTRUCTOR LEAVES
 * ALONE, so a fixture that never zeroes its storage sees both stores directly.
 * The other three are the constructor's as well as `reset`'s and need the
 * sentinel treatment finding 7105 describes.
 * ===========================================================================
 */
void
V90BitsToSymbol::reset(V90MappingParams *mp, PcmType pcm)
{
	mapper->reset(mp, pcm);

	bitsPerFrame = mp->word_0;

	if (mp->shaperSR != 0)
		extraSymbols = V90MAPPER_FRAME * mp->shaperId / mp->shaperSR;
	else
		extraSymbols = 0;

	symbolsDone = 0;
	symbolsBlockSize = 0;
	extraSymbolsPending = 1;
}

void
V90BitsToSymbol::resetNoSpectral(V90MappingParams *mp, PcmType pcm)
{
	mapper->resetNoSpectral(mp, pcm);

	bitsPerFrame = mp->word_0;
}

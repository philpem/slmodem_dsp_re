/*
 * V90TRN2Designer.cpp -- the constructor and destructor.
 *
 * `include/dsplib/V90TRN2Designer.h` carries the object map, the evidence for
 * the two pointer types and the scope of the size bound.
 *
 * PLAIN CDECL with `this` as the first STACK argument -- `mov 0x4(%esp),%eax`
 * with no frame at all -- so nothing here needs a calling-convention
 * attribute (finding 215).
 */

#include <stddef.h>

#include "dsplib/V90TRN2Designer.h"

/*
 * THE 0x558 `V90Parameters`, NEVER `V90PreFilter.h`'s 0x504 one.  The three
 * members below reach +0x074, +0x078 and +0x080, all of which are inside
 * both definitions, but the choice is not a matter of which fields are
 * reached: mixing the two under-allocates by 84 bytes and passes every test
 * not run under a checking allocator.  `V90ModemCtor.cpp` carries the long
 * version of this comment; `tools/onedef.py` gates it.
 */
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Parameters.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but parses only `struct name {`, so a C++ class asserts
 * its own.
 *
 * GUARDED ON THE POINTER WIDTH, because both members are pointers and
 * `make check64` compiles this file for the native target, where they are
 * eight bytes and +0x04 moves.  The claim is about the 32-bit layout the blob
 * has, so it is asserted only where the compiler lays that layout out.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define TRN2_OFF(field, off, tag) \
	typedef char trn2_off_##tag[ \
	    ((int)__builtin_offsetof(V90TRN2Designer, field) == (off)) ? 1 : -1]

TRN2_OFF(params, 0x00, params);
TRN2_OFF(power,  0x04, power);
typedef char trn2_size[(sizeof(V90TRN2Designer) == 0x08) ? 1 : -1];
#endif

/*
 * Two stores, in the object's order.  The argument names are the mangling's
 * types spelled out; nothing here reads either pointer.
 */
V90TRN2Designer::V90TRN2Designer(V90Parameters *p, V90ConstellationPower *cp)
{
	params = p;
	power = cp;
}

/*
 * One byte, `ret`.  See the header: the symbol exists only because the
 * original declared the destructor, so declaring and emptying it here is the
 * reconstruction and not a placeholder.
 */
V90TRN2Designer::~V90TRN2Designer()
{
}

/*
 * Adopt the configured TRN2 length as the working one.
 *
 * Twenty-four bytes, and every one of them is a decision:
 *
 *     cmpw $0x0,0x8(%esp)      the argument is compared AS A SHORT, in
 *                              memory, before `this` is even loaded
 *     je   +0x17               a zero argument does nothing at all
 *     mov  0x4(%esp),%ecx      this
 *     mov  (%ecx),%edx         this->params
 *     mov  0x80(%edx),%eax
 *     mov  %eax,0x78(%edx)
 *
 * So the argument is a FLAG and not a length: nothing here stores it, and the
 * value that lands in `nofUcodesInTrn2` comes from +0x080.  That asymmetry is
 * what finding 3527 used to call +0x080 the configured value and +0x078 the
 * working one, and it is why this member is named for what it selects rather
 * than for what it is passed.
 */
void
V90TRN2Designer::setNofUcodesInTrn2(short on)
{
	if (on != 0)
		params->nofUcodesInTrn2 = params->unnamed_080;
}

/*
 * Fill all six constellations with a descending run from 78, `nofUcodesInTrn2`
 * entries long.
 *
 * BOTH COUNTERS ARE `short` AND THAT IS FORCED, not a reading of the loop
 * bounds: the inner one is `inc %eax ; cwtl` and the outer is
 * `lea 0x1(%edi),%eax ; movswl %ax,%edi ; cmp $0x5,%di`.  An `int` needs
 * neither sign-extension, and the upper half is USED -- it is the index and
 * the compare operand -- so this is the forced case of the codegen rule and
 * not 614's discarded half.
 *
 * THE BOUND IS RE-READ EVERY ITERATION.  `mov 0x0(%ebp),%ecx` appears at
 * 0x3cb0e before the loop and AGAIN at 0x3cb3a inside it, because the byte
 * stores may alias `this->params`; hoisting the count into a local deletes
 * that reload.  The condition is therefore written through the member.
 *
 * THERE IS NO BOUND AGAINST 128.  The rows are 128 bytes each and the loop
 * runs to `params->nofUcodesInTrn2`, which nothing here checks, so a count
 * above 128 walks into the next row.  The object is written that way; the
 * test drives 128 exactly and no further.
 *
 * The compare is `cmp 0x78(%ecx),%eax ; jge` -- SIGNED, against the `int`
 * field, with the `short` counter promoted.
 */
void
V90TRN2Designer::setTrn2DummyConstel(V90MappingParams *mappingParams)
{
	short k;

	for (k = 0; k <= 5; k++) {
		unsigned char value = 78;
		short i;

		for (i = 0; i < params->nofUcodesInTrn2; i++) {
			mappingParams->constellation[k][i] = value;
			mappingParams->codecConstellation[k][i] = value;
			value--;
		}
	}
}

/*
 * log10() on the coprocessor, as the object computes it: `fldlg2` pushes
 * log10(2) at the register's full 64-bit mantissa and `fyl2x` computes
 * st(1) * log2(st(0)) and pops, so the pair takes one value and leaves one.
 *
 * GCC DOES NOT EMIT THAT SEQUENCE FOR `log10()` AT THIS TREE'S FLAGS, and the
 * flag that would is not the one finding 876 names.  Measured on the period
 * compiler in `tools/toolchain/`, at `build.sh`'s exact flag list plus one:
 *
 *     (nothing)                                       call log10
 *     -funsafe-math-optimizations                     call log10
 *     -funsafe-math-optimizations -fno-math-errno     call log10
 *     -funsafe-math-optimizations -fno-trapping-math  call log10
 *     -ffast-math                                     fldlg2 / fxch / fyl2x
 *
 * so `-funsafe-math-optimizations` is necessary and NOT sufficient for a
 * `double` argument, and nothing narrower than `-ffast-math` reproduces it.
 * That flag is not in this tree's derived set and must not be: it withdraws
 * NaN semantics from the whole translation unit, which CLAUDE.md records
 * breaking eleven other sites.  A call to libm's `log10` is not the same
 * function either -- it is correctly rounded where `fyl2x` is not -- so the
 * sequence is written out.
 *
 * THE COPY IS DELIBERATE AND IT IS THE FOURTH.  `Psd.cpp`, `V90Equalizer.cpp`
 * and `VPcmFloModem.cpp` each carry the same eight lines, and Psd.cpp says why
 * a shared header is a separate concern: a new C++ header has to be added to
 * `offcheck.py`'s SKIP_HEADERS or the `offsets` gate breaks files nobody
 * touched.  Finding 876.
 */
static inline long double
trn2_x87_log10(long double x)
{
	long double r;

	__asm__ ("fldlg2\n\tfxch %%st(1)\n\tfyl2x" : "=t" (r) : "0" (x));
	return r;
}

/*
 * How many bits one frame of the six constellations carries: log2 of the
 * product of their lengths, truncated.
 *
 * `this` IS NOT TOUCHED.  The whole body works through the argument, which is
 * why the class header can bound the object at eight bytes across five of its
 * six members.
 *
 * THE PRODUCT IS A `float`, and that is measured rather than chosen: the zero
 * test is `fcoms 0x2ac` against a FLOAT zero, and a `double` product compiles
 * to `fcoml` against `.rodata.cst8`.  Nothing rounds it to float in between,
 * because -mfpmath=387 keeps it in the register at extended precision until
 * something stores it -- so the six `fildll`/`fmulp` are what a `float`
 * declaration produces here, and the declared type shows up only in the
 * compare.
 *
 * THE CONVERSION IS 64-BIT: `fistpll` with the low word taken, not `fistpl`.
 * That is forced.  What is NOT recoverable is whether the source cast to
 * `long long` and narrowed, or cast to `unsigned int` -- the i386 back end
 * converts to unsigned through DImode and emits the same two instructions,
 * and no caller types the result, because `maxK` has no relocation anywhere
 * in the object and is reached only by being inlined into `V90TRN2Design`.
 * `int` is the neutral choice and the cast is spelled out; the two readings
 * differ only above 2^31 bits per frame.
 *
 * THE EPSILON IS THE POINT OF THE FUNCTION, not noise: with truncation
 * towards zero (`or $0xc00`), an exact power of two whose logarithm lands a
 * few ULP low would otherwise come back one bit short.  1e-6f is the object's
 * constant at .rodata.cst4+0x2b4.
 */
int
V90TRN2Designer::maxK(V90MappingParams *mappingParams)
{
	float product = (float)mappingParams->constellationSize[0] *
			mappingParams->constellationSize[1] *
			mappingParams->constellationSize[2] *
			mappingParams->constellationSize[3] *
			mappingParams->constellationSize[4] *
			mappingParams->constellationSize[5];
	int k = 0;

	if (product != 0.0f) {
		long double logProduct = trn2_x87_log10((long double)product);
		float logTwo = (float)trn2_x87_log10((long double)2.0f);

		k = (int)(long long)(logProduct / logTwo + 1e-6f);
	}
	return k;
}

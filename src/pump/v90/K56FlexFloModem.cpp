/*
 * K56FlexFloModem.cpp -- five members of a class that was never implemented.
 *
 * Reconstructed from dsplibs.o.  Five of the class's seventeen members, and
 * nine of its forty bytes.  `include/dsplib/K56FlexFloModem.h` says why
 * there is no object map: not one instruction in any of the seventeen touches
 * `this`, so there is no displacement to bound a size with.
 *
 * These are the whole functions, byte for byte:
 *
 *     _ZN15K56FlexFloModem21enterPhase3FullDuplexEv   c3
 *     _ZN15K56FlexFloModem14setMinMaxRatesEii         c3
 *     _ZN15K56FlexFloModem13externalResetEv           c3
 *     _ZN15K56FlexFloModem16getK56FlexJaBitsEPs       31 c0 c3
 *     _ZN15K56FlexFloModem16getK56FlexMpBitsEPs       31 c0 c3
 *
 * THE TWO SHAPES ARE THE EVIDENCE FOR THE TWO RETURN TYPES.  Itanium
 * mangling omits return types (docs/v90cpp.md), so `nm` cannot say; but a
 * function returning nothing leaves `%eax` alone and one returning zero sets
 * it, and here two do each.  So the `getK56Flex*Bits` pair returns a value
 * and `setMinMaxRates` and `enterPhase3FullDuplex` do not.  `int` is as far
 * as that goes: `short`, `unsigned` or a null pointer return would compile to
 * the same `xor %eax,%eax`, and no caller in the object distinguishes them.
 *
 * THE `short *` ARGUMENT IS NEVER READ AND NEVER WRITTEN.  Both getters
 * ignore it entirely, which the differential test checks by handing each side
 * a seeded buffer and comparing it afterwards -- the interesting claim about
 * a stub is what it does NOT do.
 *
 * -Wunused-parameter is why the parameters below are unnamed.
 */

#include "dsplib/K56FlexFloModem.h"

#include "dsplib/sysdep.h"

int
K56FlexFloModem::getK56FlexMpBits(short *)
{
	return 0;
}

int
K56FlexFloModem::getK56FlexJaBits(short *)
{
	return 0;
}

void
K56FlexFloModem::setMinMaxRates(int, int)
{
}

void
K56FlexFloModem::enterPhase3FullDuplex()
{
}

/*
 * externalReset -- one byte, `c3`, and `VPcmV34Create` calls it.
 *
 * It is the K56flex half of the pair `VPcmFloModem::externalReset` completes,
 * and the contrast between the two is the whole of finding 1090's point about
 * this build: the V.90 side reinitialises three parameter blocks, twenty-odd
 * flags and a demodulator, and the K56flex side does nothing whatever.
 */
void
K56FlexFloModem::externalReset()
{
}

/*
 * The other reset and the phase-3 entry, .text+0x101e0 and +0x101c0.  One byte
 * of `c3` each, like `externalReset` above and for the same reason: this class
 * is a name list.  D154.
 */
void
K56FlexFloModem::internalReset()
{
}

void
K56FlexFloModem::k56FlexEnterPhase3()
{
}

/*
 * `k56FlexRunDemodulator` -- SIX BYTES, `b8 05 00 00 00 c3`, and the five that
 * are not the `ret` are the whole of what this build's K56flex receiver does:
 * it returns the constant 5 and reads none of its four arguments.
 *
 * THE RETURN TYPE IS MEASURED THE SAME WAY THE TWO BIT GETTERS' ARE.  A
 * function that leaves `%eax` alone returns void and one that sets it returns
 * a value; this sets it to a value that is neither zero nor derived from
 * anything, so the type is as far as `int` and no further -- `short`, `long`
 * or an enum would compile to the same five bytes.
 *
 * WHAT 5 MEANS IS NOT SETTLED HERE and is not guessed.  D155 is the entry that
 * matters: `k56FlexPhase34`'s completion arms test the values these stubs
 * return, so the constant is load-bearing for a caller even though nothing in
 * this class computes it.  The two `int *` outputs are NOT written, which is
 * the interesting claim about a stub and is what the differential test checks
 * -- both sides get a seeded pair and neither may touch it.
 */
int
K56FlexFloModem::k56FlexRunDemodulator(float *, unsigned int, int *, int *)
{
	return 5;
}

/*
 * ---------------------------------------------------------------------------
 * `K56FLEX_Create` and `K56FLEX_Delete`, .text+0x102a0 and +0x102c0.
 *
 * WHY THEY ARE HERE AND NOT BESIDE `src/pump/v34/v34k56.cpp`.  Both readings
 * put them in the same ambiguous bracket -- `tools/tuattrib.py` reports
 * .text+0xa790..+0x10310 as `V34.c|GenericToneDetector.cpp`, which is exactly
 * the bracket `v34k56.cpp`'s header quotes for `k56FlexPhase34`.  What breaks
 * the tie is CONTIGUITY, which is finer than the bracket: `K56FLEX_Create` is
 * the very next symbol after `K56FlexFloModem::getK56MPsReceiver` at +0x10290,
 * and the class's seventeen members run unbroken from +0x10190 up to it.  So
 * the members and these two share a translation unit, and this file is the one
 * this tree gives that class.
 *
 * THEY ARE `extern "C"` BECAUSE THE OBJECT SAYS SO -- both names are exported
 * unmangled, and a member function could not be.  Neither takes a `this`.
 * ---------------------------------------------------------------------------
 */

extern "C" {

/*
 * `K56FLEX_Create` -- twenty bytes of heap, and not one of its four arguments
 * is read.
 *
 * The arity is the CALL SITE's and not the body's: `vpcm_create` sets up four
 * stack slots at .text+0x3b10..+0x3b22 immediately before the call, in the
 * same shape as the `VPCMXF_Create` call eight instructions earlier, and
 * passes NULL, `lea 0x2c(%ebx)`, what `dp_param_get` returned, and a computed
 * count.  The body is `sub`/`movl $0x14`/`call sysdep_malloc`/`add`/`ret` --
 * nineteen bytes that never read 0x10(%esp).  The parameters are therefore
 * unnamed: the declaration exists so that a future `vpcm_create` calls it with
 * the right stack, and naming them would put a meaning in the record that the
 * object does not give.
 *
 * `vpcm_create` DOES check the result -- `test %eax,%eax` at .text+0x3b31 and
 * a branch into the failure unwind -- so the null return matters to the caller
 * even though nothing but a failing allocator can produce one.
 */
void *
K56FLEX_Create(void *, void *, void *, int)
{
	return sysdep_malloc(K56FLEX_OBJECT_SIZE);
}

/*
 * `K56FLEX_Delete` -- the null test is the object's, at .text+0x102c7.
 *
 * Both call sites (`vpcm_create`'s failure unwind at .text+0x3dbf and
 * `vpcm_delete` at +0x3e1a) load the pointer out of the root at +0xac44 and
 * pass it straight in, and neither clears that slot afterwards.  So the test
 * is load bearing rather than defensive: the unwind path runs with the slot
 * holding whatever the failed create left in it.
 */
void
K56FLEX_Delete(void *obj)
{
	if (obj != 0)
		sysdep_free(obj);
}

}

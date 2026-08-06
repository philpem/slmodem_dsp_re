/*
 * K56FlexFloModem.cpp -- four members of a class that was never implemented.
 *
 * Reconstructed from dsplibs.o.  Four of the class's seventeen members, and
 * eight of its forty bytes.  `include/dsplib/K56FlexFloModem.h` says why
 * there is no object map: not one instruction in any of the seventeen touches
 * `this`, so there is no displacement to bound a size with.
 *
 * These are the whole functions, byte for byte:
 *
 *     _ZN15K56FlexFloModem21enterPhase3FullDuplexEv   c3
 *     _ZN15K56FlexFloModem14setMinMaxRatesEii         c3
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

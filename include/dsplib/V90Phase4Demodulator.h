/*
 * V90Phase4Demodulator.h -- the V.90 phase 4 receiver, so far as its
 * CONSTRUCTION PATH settles it.
 *
 * Reconstructed from dsplibs.o.  The class had no header and no .cpp in this
 * tree before this file.  `.symtab` carries `V90Phase4Demodulator.cpp` as a
 * FILE entry (index 231), so the original had a translation unit of its own
 * and this file's placement follows the original's rather than guessing at
 * one -- the same evidence `V90Demapper.h` cites for its own.
 *
 * THE OBJECT IS 0x351c = 13,596 BYTES, AND THAT IS AN ALLOCATION, not a
 * displacement scan -- finding 1107's rule.  `V90Demodulator::V90Demodulator`
 * has
 *
 *     1c8fe:  c7 04 24 1c 35 00 00   movl  $0x351c,(%esp)
 *     1c905:  e8 ..                  call  sysdep_malloc
 *     1c90a:  ...
 *     1c969:  e8 ..                  call  V90Phase4Demodulator::
 *                                             V90Phase4Demodulator(...)
 *     1c96e:  89 b3 e0 01 00 00      mov   %esi,0x1e0(%ebx)
 *
 * and the same sequence again in its C2 twin.  The highest offset the
 * constructor writes is +0x3514, so the last eight bytes are bounded by the
 * allocation alone -- which is the case the rule exists for.
 *
 * NOT POLYMORPHIC.  `nm` gives `D1` at 0x25b50 and `D2` at 0x25b10 and no
 * `D0`; GCC emits a deleting destructor only for a virtual class, so offset 0
 * is a real member and there is no vptr (finding 228).
 *
 * ---------------------------------------------------------------------------
 * THREE EXACT MEETINGS FIX THE THREE EMBEDDED SUBOBJECTS
 *
 * The constructor builds three members in place and the destructor destroys
 * the same three in reverse, so their bases are read and not guessed:
 *
 *     +0x0050  V90Phase4Modulator   sizeof 0x2fac   ends 0x2ffc
 *     +0x2ffc  V90RDetector         sizeof 0x002c   ends 0x3028
 *     +0x3028  V90RDetector         sizeof 0x002c   ends 0x3054
 *
 * Each end is the next base exactly, and 0x3054 is where the first of the
 * four trailing pointers lives.  Three independent bases and three exact
 * meetings, and `V90Phase4Demodulator.cpp` asserts all of them with
 * `__builtin_offsetof` rather than restating them in a comment -- so if
 * either subobject's own size ever moves, the compiler says so.
 *
 * ---------------------------------------------------------------------------
 * THE TWO MAPPING-PARAMETER POINTERS REACH THE MODULATOR SWAPPED
 *
 * The embedded modulator is built with
 *
 *     V90Phase4Modulator(params, mode, NULL, NULL,
 *                        mappingParams2, mappingParams1, NULL, 0xc)
 *
 * -- argument 2 into the modulator's fifth slot and argument 1 into its
 * sixth.  In the object that is `mov 0x48(%esp),%edx` reaching
 * `mov %edx,0x14(%esp)` and `mov 0x44(%esp),%ebp` reaching
 * `mov %ebp,0x18(%esp)`, two loads and two stores with nothing between them
 * that could have reordered a pair of independent stack slots by accident.
 * The demodulator stores them the OTHER way round in its own object
 * (+0x0c gets argument 1, +0x10 argument 2), so this really is a crossing and
 * not a misreading of which is which.
 *
 * A test that passed the same pointer twice could not see this at all, which
 * is why the two must be distinguishable.
 *
 * ---------------------------------------------------------------------------
 * WHY THE CONSTRUCTOR AND DESTRUCTOR ARE DECLARED AND NOT DEFINED
 *
 * Both call `V90Phase4Modulator`'s, and that class has neither written in
 * this tree: `V90Phase4Modulator.h` defines one member out of forty-three and
 * `nm` on our objects finds no `_ZN18V90Phase4ModulatorC1E...` and no
 * `_ZN18V90Phase4ModulatorD1Ev`.  Defining either member below would fail to
 * link.  In the blob those two symbols are also reached from
 * `V90Modulator`'s constructor and destructor -- the TRANSMIT chain -- so the
 * class belongs to whichever batch writes that, and this one records the map
 * rather than adopting it.
 *
 * 277 of the 2,671 bytes of the V.90 receive construction chain are behind
 * that, and another 1,671 behind it in turn: `~V90Demodulator` destroys a
 * `V90Phase4Demodulator`, so the demodulator's own lifecycle pair waits on
 * the same symbol.
 */

#ifndef DSPLIB_V90PHASE4DEMODULATOR_H
#define DSPLIB_V90PHASE4DEMODULATOR_H

#include "dsplib/Scrambler.h"
#include "dsplib/V90Phase4Modulator.h"
#include "dsplib/V90RDetector.h"

/*
 * POINTERS ONLY, so forward declarations are what belong here.  Two
 * incompatible definitions of `V90Parameters` exist in this tree and no
 * translation unit may include both -- finding 1112 -- so the class is
 * declared and never defined here.
 */
class V90Parameters;
class V90MappingParams;
class V90Demapper;
class V90CP;
class V90MP;
class V90ConnectionEvaluator;
class V90Phase3Demodulator;
class V90AutoDigitalImpDetector;

class V90Phase4Demodulator {
public:
	/*
	 * DECLARED AND DELIBERATELY NOT DEFINED; see the file comment for what
	 * blocks them.  The parameter list is the mangling's and not a choice:
	 * an `int` where the original had `unsigned` emits a different symbol.
	 *
	 *     V90Phase4Demodulator(V90MappingParams *, V90MappingParams *,
	 *                          V90Demapper *, V90CP *, V90MP *,
	 *                          Descrambler<unsigned char, int> *,
	 *                          V90ConnectionEvaluator *, V90Parameters *,
	 *                          V90Phase3Demodulator *,
	 *                          V90AutoDigitalImpDetector *, unsigned int)
	 *                                                  C1,C2   225 B
	 *     ~V90Phase4Demodulator()                      D1,D2    52 B
	 *
	 * They are NOT declared as members below, and that is deliberate: a
	 * user-declared constructor removes the default one and a
	 * user-declared destructor makes the class non-trivially destructible,
	 * which would delete the special members of any fixture holding one by
	 * value -- for a pair that cannot be defined yet and so cannot be
	 * tested.  Declaring them the day they are written is the same edit.
	 */

	/*
	 * The rest of the class -- `getV90Decision`, `getV92Decision`,
	 * `reset`, the four state entries, the two detectors and the rest, 24
	 * members and some 6,600 bytes -- is declared nowhere yet and belongs
	 * to whichever batch writes it.
	 *
	 * Data members are public for the reason V90Jd.h gives: the original's
	 * access specifiers are not recoverable from the mangling, and a
	 * single access section is what lets the .cpp assert every offset
	 * below with `__builtin_offsetof`.
	 */

	/*
	 * +0x0000  The constructor's ELEVENTH argument.  `V90Demodulator`
	 * passes its own +0x30 -- the field its `setSessionFlag` writes -- and
	 * hands the same value to `V90Phase3Demodulator`'s third argument,
	 * which that class's header already calls `sessionFlag`.  The name
	 * follows the value, not this constructor.
	 */
	unsigned int sessionFlag;

	/* +0x0004  The constructor's eighth argument. */
	V90Parameters *params;

	/*
	 * +0x0008  NOT WRITTEN BY THE CONSTRUCTOR and reached by nothing
	 * reconstructed here.  It is a gap in the map, not a claim that the
	 * object has one.
	 */
	unsigned char pad_0008[4];

	/* +0x000c  The constructor's FIRST argument. */
	V90MappingParams *mappingParams1;

	/* +0x0010  The constructor's SECOND argument. */
	V90MappingParams *mappingParams2;

	/* +0x0014  The constructor's fourth argument. */
	V90CP *cp;

	/* +0x0018  The constructor's fifth argument. */
	V90MP *mp;

	/* +0x001c  The constructor's ninth argument. */
	V90Phase3Demodulator *phase3Demodulator;

	/*
	 * +0x0020 .. +0x004f  NOT MODELLED.  Nothing the construction path
	 * touches reaches here; the bound is the modulator's base below.
	 */
	unsigned char pad_0020[0x30];

	/*
	 * +0x0050  EMBEDDED, not pointed at: `lea 0x50(%ebx),%edx` in the
	 * constructor and `add $0x50,%ebx` in the destructor, both address
	 * arithmetic rather than a load.  Built with the two mapping-parameter
	 * pointers SWAPPED; see the file comment.
	 */
	V90Phase4Modulator phase4Modulator;

	/*
	 * +0x2ffc and +0x3028  EMBEDDED, one each, both built with `params`
	 * and both destroyed -- second first -- by the destructor.  Which is
	 * which is not established: the two constructor calls are identical
	 * apart from the base, and nothing reconstructed here reads either.
	 */
	V90RDetector rDetector1;
	V90RDetector rDetector2;

	/* +0x3054  The constructor's third argument.  Not owned. */
	V90Demapper *demapper;

	/* +0x3058  The constructor's sixth argument.  Not owned. */
	Descrambler<unsigned char, int> *descrambler;

	/*
	 * +0x305c .. +0x34f7  NOT MODELLED.  The two runs of trailing
	 * pointers are 0x3054..0x305c and 0x34f8..0x3518, and what sits
	 * between them is the phase 4 receiver's own state.
	 */
	unsigned char pad_305c[0x49c];

	/* +0x34f8  The constructor's seventh argument.  Not owned. */
	V90ConnectionEvaluator *connectionEvaluator;

	unsigned char pad_34fc[0x18];	/* +0x34fc  not modelled           */

	/* +0x3514  The constructor's tenth argument.  Not owned. */
	V90AutoDigitalImpDetector *autoDigitalImpDetector;

	/*
	 * +0x3518  NOT MODELLED, and the ONLY thing that bounds it is the
	 * 0x351c allocation: nothing the construction path touches reaches
	 * past +0x3514.
	 */
	unsigned char pad_3518[4];
};

#endif /* DSPLIB_V90PHASE4DEMODULATOR_H */

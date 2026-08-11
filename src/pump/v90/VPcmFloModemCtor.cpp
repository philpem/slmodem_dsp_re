/*
 * VPcmFloModemCtor.cpp -- `VPcmFloModem::VPcmFloModem` (0xfa60 C1, 0xfee0 C2,
 * 651 bytes each).
 *
 * SIX MEMBER OBJECTS, ONE OF WHICH IS THE WHOLE V.90 MODEM, then thirty-odd
 * constant stores and three `sysdep_memset`s.  The object map is in
 * include/dsplib/VPcmFloModem.h; what this file adds to the record is the
 * ARGUMENT WIRING, which is the part no field map can show and the part two
 * batches in this chain have already got wrong (findings 1301 and 1307).
 *
 * ===========================================================================
 * THE DESTRUCTOR IS NOT HERE AND IS NOT WRITTEN ANYWHERE
 * ===========================================================================
 *
 * `_ZN12VPcmFloModemD1Ev` at 0xd0a0 and `D2` at 0xd030 are 0x61 bytes each and
 * are SIX MEMBER DESTRUCTOR CALLS IN REVERSE DECLARATION ORDER and nothing
 * else: `GenericIIR<float,double>`, `SineWave<float,float>`,
 * `ANSamToneDetector`, `V92EchoCanceller`, `V92Modem`, `V90Modem`.  That is
 * exactly what GCC emits for an IMPLICITLY-DECLARED destructor over those six
 * members, so the original declared none, and neither does this tree.  Adding
 * an empty one would be a change: with `-fno-lifetime-dse` in CXXFLAGS a
 * user-written body is not the same thing as no body at all.
 *
 * AND THE SYMBOLS DO NOT EXIST IN OUR OBJECT.  This paragraph used to claim
 * they did and that a file named `t_vpcmflomodemctor.cpp` drove them; `nm`
 * over `build/src/` finds no `_ZN12VPcmFloModemD*` and that file has never
 * existed.  Both sentences were wrong and are retracted here rather than
 * quietly deleted, because the retraction is the finding: an
 * implicitly-declared destructor is implicitly INLINE, our build has exactly
 * one call site for it -- `VPCMXF_Delete` -- and GCC inlines it there and
 * emits no out-of-line copy at all.  The BLOB has both, as ordinary global
 * `T` symbols, so 194 bytes of it are behaviourally reproduced (they are the
 * six calls inside our `VPCMXF_Delete`, instruction for instruction) and
 * symbolically absent.  Deviation D237, and the blob's `D1` is what
 * test/unit/t_vpcmctor.cpp drives on our side's behalf -- through
 * `VPCMXF_Delete`, which is where our copy of the code actually lives.
 *
 * Forcing the symbols out would cost more than it buys.  A destructor
 * declared here and defined out of line cannot be inlined into
 * `VPCMXF_Delete`, which would turn its six calls into one and make THAT
 * function stop matching the blob; and one defined inline in the header comes
 * out weak and in a comdat group, where the blob's are global.  Neither is
 * the object's shape, and the object's shape is the specification.
 *
 * ===========================================================================
 * WHAT THE ARGUMENTS DO
 * ===========================================================================
 *
 *   arg 1  `void *v34Object`      -> +0x0000, and nowhere else.
 *   arg 2  `V90ModemSide side`    -> the V90Modem's first argument UNCHANGED,
 *                                    the V92Modem's first argument as
 *                                    `(side == 1)`, and +0x6120 as the same
 *                                    `(side == 1)`.  `dec %ebp; sete %dl`
 *                                    at 0xfab7 is the whole computation and
 *                                    it is done ONCE, into %esi, which
 *                                    survives to 0xfcbb.
 *   arg 3  `_tagModemParameters *`-> both modems, and then read again at the
 *                                    end to clear one bit of `unnamed_0003`.
 *   arg 4  `unsigned nSamples`    -> the V90Modem, the V92Modem AND the
 *                                    V92EchoCanceller.  Three callees, one
 *                                    value.
 *   arg 5  `V90ComputationalMode` -> the V90Modem only.
 *   arg 6  `V92ComputationalMode` -> the V92Modem only.
 *
 * The V90Modem's SIXTH argument is a literal 1 (`mov $0x1,%ecx` at 0xfa61,
 * into 0x18(%esp)), and its THIRD is `&this->dil` -- `lea 0x4(%ebx),%esi` at
 * 0xfa90, which is an ADD off `this`, so the descriptor the modem is given is
 * the one embedded in this object.  The V92Modem gets the SAME address as its
 * fourth.  Both modems therefore point at one descriptor, which is why
 * `enterPhase3` can pack it and either modem read it.
 *
 * ===========================================================================
 * THE FOUR CONSTANT-ARGUMENT CALLS, AND WHERE THE NUMBERS COME FROM
 * ===========================================================================
 *
 * Every one of these is an immediate in the disassembly and none is derived:
 *
 *   V92EchoCanceller(v92modem.parameters, nSamples, 199)
 *       0xc7 at 0xfac9.  `parameters` is LOADED BACK out of the V92Modem at
 *       0xfaf8 (`mov 0x6128(%ebx),%ecx`) rather than kept from the
 *       construction that just wrote it, so the source reads the member.
 *
 *   ANSamToneDetector(6000, 450, 250000.0f, 1, 0.58f, 9600, 50, 99)
 *       0x1770, 0x1c2, 0x48742400, 1, 0x3f147ae1, 0x2580, 0x32, 0x63.  The
 *       two floats are the bit patterns, decoded: 0x48742400 is exactly
 *       250000.0f and 0x3f147ae1 is the nearest float to 0.58.
 *       ANSamToneDetector.h already records this call site's arguments from
 *       the other end and the values agree.
 *
 *   SineWave<float, float>(4800.0f, 980.0f, 0.0f, 9600.0f)
 *       0x45960000, 0x44750000, 0, 0x46160000.
 *
 *   GenericIIR<float, double>(5, 5, entFiltDen, entFiltNum, 99)
 *       THE COEFFICIENT ORDER IS NOT GUESSED AND IS NOT RE-DERIVED HERE.
 *       include/dsplib/vpcm_tables.h pins it: the DENOMINATOR is the third
 *       argument and the NUMERATOR the fourth, which is what pairs
 *       `entFiltDen` (.data+0x120, the relocation at .text+0xfb7e) with the
 *       third slot and `entFiltNum` (.data+0xe0, .text+0xfbaa) with the
 *       fourth.  Two pointers of one type in adjacent slots is precisely the
 *       shape findings 1301 and 1307 are about, and the tie was already
 *       broken by another file, so it is cited and not re-argued.
 *
 * ===========================================================================
 * THE THREE MEMSETS
 * ===========================================================================
 *
 *     +0x021e, 0x1518   `bitVector`,   0x21e .. 0x1736
 *     +0x6fbc, 0x0e10   `cpBitVector`, 0x6fbc .. 0x7dcc
 *     +0x6c0c, 0x0350   `block_6c0c`,  0x6c0c .. 0x6f5c
 *
 * Each span runs from the start of a modelled field to the start of the next
 * modelled field, so all three are `sizeof` of what they clear and none is a
 * magic number.  The third is the one that identifies `block_6c0c` as this
 * class's own rather than the echo canceller's tail -- see the header.
 *
 * ===========================================================================
 * SIX BYTES COPIED OUT OF .rodata
 * ===========================================================================
 *
 * `flags_0217` is not six stores.  0xfbf5 loads FOUR bytes from .rodata+0x3e0
 * and stores them at +0x217; 0xfc09 loads TWO more from .rodata+0x3e4 and
 * stores them at +0x21b.  The bytes there are 1,1,1,1,1,1 -- all six -- so
 * the source is an initialisation from a constant array and the compiler
 * merged it into two moves.  Written here as the array it is, so that the
 * value and the width are one statement; a run of six `= 1` compiles to six
 * byte stores and would be a different function.
 */

#include <stddef.h>

#include "dsplib/VPcmFloModem.h"

#include "dsplib/debug.h"
#include "dsplib/modem_params.h"
#include "dsplib/sysdep.h"
#include "dsplib/V92Parameters.h"
#include "dsplib/vpcm_tables.h"

/*
 * `V92ComputationalMode` is opaque (V92Modem.h) and this file only forwards a
 * value of it, so nothing more is needed.
 */

/*
 * Hold the compiler to the header's map for the six members and the tail.
 * The offsets the five ALREADY-WRITTEN members establish are asserted in
 * src/pump/v90/VPcmFloModem.cpp and are not repeated; these are the ones this
 * constructor is the evidence for.
 *
 * `sizeof(VPcmFloModem) == 0x7f68` IS THE ONE ASSERTION THIS BATCH ADDS THAT
 * IS A SIZE.  `VPCMXF_Create` allocates 0x7f68 and constructs into it with
 * nothing in between (src/pump/v90/VPcmXfCreate.cpp), so the immediate is the
 * ORIGINAL COMPILER'S OWN `sizeof` -- finding 1246 -- and the header's map
 * has to add up to it or one of the two is wrong.  VPcmFloModem.h used to say
 * "a floor is still not a size, so no size is asserted here"; the floor was
 * 0x7f28 and the allocation is 0x7f68, and the 64 bytes between them are the
 * `GenericIIR` and the three fields after it.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define VPCMC_OFF(field, off, tag) \
	typedef char vpcmc_off_##tag[ \
		((int)__builtin_offsetof(VPcmFloModem, field) == (off)) \
		? 1 : -1]

VPCMC_OFF(v92modem,		0x6124, v92modem);
VPCMC_OFF(echoCanceller,	0x6bd0, echo);
VPCMC_OFF(block_6c0c,		0x6c0c, block6c0c);
VPCMC_OFF(ansam,		0x6f5c, ansam);
VPCMC_OFF(sineWave,		0x6f9c, sinewave);
VPCMC_OFF(entFilt,		0x7f28, entfilt);
VPCMC_OFF(byte_7f5c,		0x7f5c, byte7f5c);
VPCMC_OFF(word_7f60,		0x7f60, word7f60);
VPCMC_OFF(word_7f64,		0x7f64, word7f64);

typedef char vpcmc_size[(sizeof(VPcmFloModem) == 0x7f68) ? 1 : -1];
#endif

/*
 * .rodata+0x3e0, six bytes.  See the file comment: this is one initialisation
 * and not six stores, and the compiler is what turns it into a four-byte move
 * and a two-byte one.
 */
static const unsigned char vpcm_ctor_flags_0217[6] = { 1, 1, 1, 1, 1, 1 };

/*
 * THE MEMBER-INITIALISER LIST IS THE OBJECT'S OWN ORDER, and that it can be
 * is the interesting part.
 *
 * 0xfa7c stores `v34Obj` into +0x0000 BEFORE the call to
 * `_ZN8V90ModemC1E...` at 0xfaaa, and a mem-initialiser list runs before the
 * body entire -- so a `v34Object = v34Obj;` statement in the body would put
 * the store AFTER all six constructions.  Initialising `v34Object` in the
 * list instead gives exactly the object's sequence, because a list is ordered
 * by DECLARATION and `v34Object` is +0x0000, the first member of the class.
 * The result is store, V90Modem, V92Modem, V92EchoCanceller,
 * ANSamToneDetector, SineWave, GenericIIR, body -- which is the disassembly
 * read straight down.
 *
 * `echoCanceller(v92modem.parameters, ...)` READS A MEMBER THAT WAS
 * INITIALISED TWO ENTRIES EARLIER, and that is the object's `mov
 * 0x6128(%ebx),%ecx` at 0xfaf8: it loads the V92Modem's own `parameters`
 * rather than reusing anything the construction left in a register.  It is
 * well defined here for the same reason it is correct there -- `v92modem`
 * precedes `echoCanceller` in the class, so it is fully constructed.
 */
VPcmFloModem::VPcmFloModem(void *v34Obj, V90ModemSide side,
			   _tagModemParameters *modemParams,
			   unsigned int nSamples, V90ComputationalMode v90Mode,
			   V92ComputationalMode v92Mode)
	: v34Object(v34Obj),
	  modem(side, modemParams, &dil, nSamples, v90Mode, 1),
	  v92modem((V92ModemSide)(side == V90_MODEM_SIDE_ANALOG), modemParams,
		   nSamples, &dil, v92Mode),
	  echoCanceller(v92modem.parameters, nSamples, 199),
	  ansam(6000, 450, 250000.0f, 1, 0.58f, 9600, 50, 99),
	  sineWave(4800.0f, 980.0f, 0.0f, 9600.0f),
	  entFilt(5, 5, entFiltDen, entFiltNum, 99)
{
	unsigned int i;

	sysdep_memset(bitVector, 0, sizeof(bitVector));
	nofBits = 0;

	for (i = 0; i < sizeof(flags_0217); i++)
		flags_0217[i] = vpcm_ctor_flags_0217[i];

	word_1740 = 0;

	sysdep_memset(cpBitVector, 0, sizeof(cpBitVector));

	terminateJa = 0;
	terminateCp = 0;
	terminateCpNot = 0;
	cpNotLoaded = 0;

	/*
	 * ZERO HERE AND TWO IN `VPCMXF_Create`, which re-writes five of these
	 * same fields immediately after this constructor returns.  The two
	 * functions disagree on this one byte and on `minNofTransmitSequences`
	 * (0 here, 1 there), so the caller's values are what a constructed
	 * modem actually starts with.  Both are reproduced as found.
	 */
	nofBitsPerSymbol = 0;

	byte_6119 = 0;
	minNofTransmitSequences = 0;
	cpNofBits = 0;
	nofTransmitSequences = 0;

	word_6f98 = 0;
	word_6fb0 = 0;
	word_6fac = 0;
	word_6fb4 = 0;

	/*
	 * `andb $0xfb,0x3(%edi)` at 0xfca5 -- bit 2 of `unnamed_0003`, and
	 * the ONLY thing this constructor writes outside its own object.
	 * modem_params.h's comment on that byte says "low 3 bits cleared",
	 * which is `dp_runtime_create`'s doing; this clears one of the three
	 * again.
	 */
	modemParams->unnamed_0003 &= (unsigned char)~0x04u;

	sysdep_memset(block_6c0c, 0, sizeof(block_6c0c));

	/*
	 * The SAME `(side == analog)` the V92Modem got as its first argument,
	 * computed once at 0xfab7 and kept in %esi across five calls.
	 */
	info0Layout = (side == V90_MODEM_SIDE_ANALOG);
	pcmSessionType = 0;
	byte_6118 = 0;

	byte_7f5c = 0;
	word_7f60 = 0;
	word_7f64 = 0;
}

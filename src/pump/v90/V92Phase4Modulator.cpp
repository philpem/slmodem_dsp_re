/*
 * V92Phase4Modulator.cpp -- construction and destruction of the V.92 phase 4
 * upstream symbol source.
 *
 * Reconstructed from dsplibs.o.  Four symbols, 502 bytes:
 *
 *     V92Phase4Modulator::V92Phase4Modulator(V92Parameters *,
 *         V92BitsToSymbol *, V92CP *, V92MappingParams *)
 *                                       .text+0x17970 (C1), +0x17a20 (C2)
 *     V92Phase4Modulator::~V92Phase4Modulator()
 *                                       .text+0x16de0 (D2), +0x16e40 (D1)
 *
 * Each pair differs only in which registers hold two of the argument setups,
 * which is the register allocator's choice and not the source's; GCC emits
 * both from one definition.  `include/dsplib/V92Phase4Modulator.h` carries
 * the object map, the 0x1cc the allocation gives and the evidence that the
 * class's other twenty-nine members are deliberately absent.
 *
 * Plain cdecl, `this` first on the stack -- `mov 0x20(%esp),%ebx` after a
 * 0x1c-byte frame with three saves in it -- finding 215.
 */

#include <stddef.h>

#include "dsplib/V92Phase4Modulator.h"
#include "dsplib/V92CP.h"
#include "dsplib/V92Mapper.h"

extern "C" {
void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);

/*
 * V92Mapper's complete-object constructor, by the name the relocation at
 * .text+0x179be carries.  Called through an asm() label rather than through
 * `new` for the reason src/pump/v90/V92Precoder.cpp sets out in full: the
 * blob's `sysdep_malloc(n); ctor(p)` with no null test between them is what
 * `new` emits over an inline `operator new`, and this build has no <new>.
 */
void v92p4m_mapper_ctor(void *self) asm("_ZN9V92MapperC1Ev");
}

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V92P4M_OFF(field, off, tag) \
	typedef char v92p4m_off_##tag[ \
	    ((int)__builtin_offsetof(V92Phase4Modulator, field) == (off)) \
	    ? 1 : -1]

V92P4M_OFF(pad_00,		0x000, pad00);
V92P4M_OFF(word_18,		0x018, word18);
V92P4M_OFF(byte_1c,		0x01c, byte1c);
V92P4M_OFF(pad_1d,		0x01d, pad1d);
V92P4M_OFF(mappingParams,	0x048, mappingparams);
V92P4M_OFF(scrambler,		0x04c, scrambler);
V92P4M_OFF(bitsToSymbol,	0x06c, bitstosymbol);
V92P4M_OFF(mapper,		0x070, mapper);
V92P4M_OFF(cp,			0x074, cp);
V92P4M_OFF(pad_78,		0x078, pad78);
V92P4M_OFF(word_1c0,		0x1c0, word1c0);
V92P4M_OFF(word_1c4,		0x1c4, word1c4);
V92P4M_OFF(params,		0x1c8, params);

typedef char v92p4m_size[(sizeof(V92Phase4Modulator) == 0x1cc) ? 1 : -1];
typedef char v92p4m_mapper_size[(sizeof(V92Mapper) == 0x2c) ? 1 : -1];

/*
 * The scrambler is a member and its size is what makes +0x6c meet it, so the
 * map is only self-consistent if this holds.
 */
typedef char v92p4m_scram_size[
	(sizeof(Scrambler<unsigned char, unsigned char>) == 0x20) ? 1 : -1];

/* The word this constructor reaches into somebody else's object to clear. */
typedef char v92p4m_cp_word110[
	((int)__builtin_offsetof(V92CP, word_110) == 0x110) ? 1 : -1];

#endif /* 32-bit */

/*
 * ===========================================================================
 * V92Phase4Modulator::V92Phase4Modulator (.text+0x17970 / +0x17a20, 164 B)
 *
 * The whole body, in the object's own order:
 *
 *     Scrambler<unsigned char,unsigned char>(this + 0x4c, 5, 0x17, 0x63)
 *     malloc(0x2c) -> V92Mapper() -> +0x70
 *     +0x1c0 = 0
 *     +0x48  = mappingParams          (the FOURTH argument)
 *     +0x1c8 = params                 (the FIRST)
 *     +0x1c4 = 0
 *     +0x74  = cp                     (the THIRD)
 *     cp->word_110 = 0
 *     +0x6c  = bitsToSymbol           (the SECOND)
 *     +0x18  = 0
 *     +0x1c  = 0                      (a BYTE store)
 *
 * THE ARGUMENTS ARE NOT STORED IN ORDER, and that is worth stating because a
 * reconstruction that stores them in declaration order produces an object
 * that is wrong in four fields and passes nothing.  The order above is
 * `mov 0x30(%esp)`, `mov 0x24(%esp)`, `mov 0x2c(%esp)`, `mov 0x28(%esp)` --
 * fourth, first, third, second -- read off the frame after a 0x1c-byte
 * subtraction with three register saves inside it.
 *
 * The store into the caller's `V92CP` is the constructor's, not a side
 * effect: %esi is zeroed at .text+0x179e5 for the sole purpose of it.
 * ===========================================================================
 */
V92Phase4Modulator::V92Phase4Modulator(V92Parameters *p, V92BitsToSymbol *bts,
				       V92CP *c, V92MappingParams *mp)
	: scrambler(V92P4M_SCRAM_TAP1, V92P4M_SCRAM_TAP2, V92P4M_SCRAM_SLACK)
{
	void *m;

	m = sysdep_malloc(sizeof(V92Mapper));
	v92p4m_mapper_ctor(m);
	mapper = (V92Mapper *)m;

	word_1c0 = 0;
	mappingParams = mp;
	params = p;
	word_1c4 = 0;
	cp = c;
	cp->word_110 = 0;
	bitsToSymbol = bts;
	word_18 = 0;
	byte_1c = 0;
}

/*
 * ===========================================================================
 * V92Phase4Modulator::~V92Phase4Modulator (.text+0x16de0 / +0x16e40, 87 B)
 *
 * The body is the mapper's release and nothing else; the unconditional
 * `Scrambler<unsigned char,unsigned char>::~Scrambler(this + 0x4c)` that
 * follows it on both paths is the COMPILER'S implicit member destruction, not
 * a statement -- the same reading finding 1256 makes of V92Phase3Modulator's
 * 22-byte destructor, and the same reason its source is an empty body.
 *
 * The mapper pointer is NOT nulled after the free, so a second destruction
 * double-frees it.  Reproduced; see docs/deviations.md.
 * ===========================================================================
 */
V92Phase4Modulator::~V92Phase4Modulator()
{
	if (mapper != 0) {
		mapper->~V92Mapper();
		sysdep_free(mapper);
	}
}

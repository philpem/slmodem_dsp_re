/*
 * V90Phase4Modulator.cpp -- the phase 4 modulator's construction, destruction
 * and session flag.
 *
 * Reconstructed from dsplibs.o.  Three of the class's forty-three members:
 * the constructor, the destructor, and `setSessionFlag`, which is the one
 * `v34handshak` reaches.  `include/dsplib/V90Phase4Modulator.h` carries the
 * object map, the 0x2fac size and the ownership argument.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
 *
 * `V90Phase3Modulator::setSessionFlag` is the same eleven bytes against the
 * same offset in a different class, and src/pump/v90/V90Phase3Modulator.cpp
 * is where that one lives.
 *
 * WHY THE CONVERTER IS BUILT THROUGH AN asm() LABEL RATHER THAN `new`: the
 * argument is src/pump/v90/V90BitsToSymbol.cpp's, and it is the same one --
 * `-nostdinc++` leaves no <new>, a replacement global `operator new` is
 * ill-formed, and a user-declared placement form makes GCC emit a null test
 * the blob does not have.  The instruction sequence is the blob's either way.
 */

#include <stddef.h>

#include "dsplib/sysdep.h"
#include "dsplib/V90BitsToSymbol.h"
#include "dsplib/V90Phase4Modulator.h"

extern "C" {
/*
 * V90BitsToSymbol's constructor, by the name the blob calls.  C1 is the
 * complete-object variant, which is what a `new` expression uses and what the
 * relocation at 0x2d8f5 names.
 */
void v90p4_bts_ctor(void *self, unsigned int nofSymbols, V90Parameters *params)
	asm("_ZN15V90BitsToSymbolC1EjP13V90Parameters");
}

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90P4_OFF(field, off, tag) \
	typedef char v90p4_off_##tag[ \
	    ((int)__builtin_offsetof(V90Phase4Modulator, field) == (off)) \
	    ? 1 : -1]

V90P4_OFF(sessionFlag,		0x0000, sessionflag);
V90P4_OFF(bitsToSymbol,		0x0044, bts);
V90P4_OFF(mp,			0x0048, mp);
V90P4_OFF(mappingParams,	0x004c, mp1);
V90P4_OFF(mappingParams2,	0x0050, mp2);
V90P4_OFF(cp,			0x0054, cp);
V90P4_OFF(scrambler,		0x0058, scrambler);
V90P4_OFF(scrambledBits,	0x0078, bits);
V90P4_OFF(mpBits,		0x2f58, mpbits);
V90P4_OFF(mpBitCount,		0x2f5c, mpcount);
V90P4_OFF(mpSequenceSymbols,	0x2f60, mpsym);
V90P4_OFF(word_2f64,		0x2f64, w2f64);
V90P4_OFF(rdRtSymbols,		0x2f68, rdrt);
V90P4_OFF(rfSymbols,		0x2f74, rf);
V90P4_OFF(cpBits,		0x2f8c, cpbits);
V90P4_OFF(cpBitCount,		0x2f90, cpcount);
V90P4_OFF(cpSequenceSymbols,	0x2f94, cpsym);
/*
 * The one that proves the region was renamed and not resized: everything from
 * here on was named before this pass, so if the fields above have taken one
 * byte too many or too few, this fails.
 */
V90P4_OFF(ctorArg8,		0x2f98, arg8);
V90P4_OFF(cleared_2f9c,		0x2f9c, c2f9c);
V90P4_OFF(cleared_2fa0,		0x2fa0, c2fa0);
V90P4_OFF(externalBitsToSymbol,	0x2fa4, external);
V90P4_OFF(params,		0x2fa8, params);
typedef char v90p4_size[(sizeof(V90Phase4Modulator) == 0x2fac) ? 1 : -1];
#endif

/*
 * ===========================================================================
 * V90Phase4Modulator::V90Phase4Modulator -- .text+0x2d830 (C1) and +0x2d910
 * (C2), 213 bytes each.
 *
 * The scrambler's mem-initializer runs before the body, which is where the
 * blob's leading `lea 0x58(%esi),%edx ; call Scrambler<h,h>::C1` comes from.
 * `pad_0004` and everything from `scrambledBits` to `cpSequenceSymbols` are
 * left exactly as they were found; forty-two unwritten members' state is not
 * this function's business.  (That second region was `pad_0078` until it was
 * named out; the constructor's behaviour is unchanged, which is the point.)
 * ===========================================================================
 */
V90Phase4Modulator::V90Phase4Modulator(V90Parameters *p, unsigned int flag,
				       V90BitsToSymbol *bts, V90MP *mpArg,
				       V90MappingParams *mpsA,
				       V90MappingParams *mpsB, V90CP *cpArg,
				       unsigned int arg8)
	: scrambler(0x12, 0x17, 0x63)
{
	params = p;
	cp = cpArg;
	ctorArg8 = arg8;
	mp = mpArg;
	mappingParams = mpsA;
	sessionFlag = flag;
	cleared_2f9c = 0;
	mappingParams2 = mpsB;
	cleared_2fa0 = 0;
	if (bts) {
		bitsToSymbol = bts;
		externalBitsToSymbol = 1;
	} else {
		V90BitsToSymbol *own;

		own = (V90BitsToSymbol *)
		    sysdep_malloc(sizeof(V90BitsToSymbol));
		v90p4_bts_ctor(own, 0x140, p);
		bitsToSymbol = own;
		externalBitsToSymbol = 0;
	}
}

/*
 * ===========================================================================
 * V90Phase4Modulator::~V90Phase4Modulator -- .text+0x2c5a0 (D2) and +0x2c600
 * (D1), 94 bytes each.
 *
 * The ownership flag is read FIRST and short-circuits the whole release, so a
 * supplied converter is never touched however non-null it is.  Neither the
 * pointer nor the flag is cleared afterwards.
 * ===========================================================================
 */
V90Phase4Modulator::~V90Phase4Modulator()
{
	if (!externalBitsToSymbol && bitsToSymbol) {
		bitsToSymbol->~V90BitsToSymbol();
		sysdep_free(bitsToSymbol);
	}
}

void
V90Phase4Modulator::setSessionFlag(unsigned int flag)
{
	sessionFlag = flag;
}

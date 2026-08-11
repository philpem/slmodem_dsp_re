/*
 * V90Modulator.cpp -- V90Modulator's constructor and destructor.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90Modulator.h` carries the
 * object map, the 0x70 size and the argument-to-offset table; this file is
 * the two functions and the assertions that hold the compiler to that map.
 * `setSessionFlag` stays where it was, in src/pump/v90/V90SessionFlag.cpp.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
 *
 * THREE SUB-OBJECTS ARE OWNED AND FIVE ALLOCATIONS ARE MADE, IN THIS ORDER:
 * a symbol buffer of 2 bytes per symbol, a frame buffer of 8, a
 * `V90BitsToSymbol` of 0x24 built with `3 * nofSymbols + 0x1388` symbols, a
 * `V90Phase3Modulator` of 0x398, and a `V90Phase4Modulator` of 0x2fac.  Each
 * of the last three is `sysdep_malloc(sizeof)` immediately followed by the
 * class's `C1`, which is where those three sizes come from -- finding 1246.
 *
 * THE THREE NESTED CONSTRUCTORS READ THEIR ARGUMENTS BACK OUT OF THE MEMBERS,
 * not out of the registers the arguments arrived in: `mov 0x24(%ebx),%edx`
 * and `mov 0x28(%ebx),%ebp` after each allocator call, which the compiler
 * would not emit if the source had named the parameters, because it cannot
 * prove the allocation does not alias `this`.  The three MALLOC SIZES are the
 * other way round -- `%ebp` is used directly and never reloaded from +0x64 --
 * so those name the parameter.  Neither reading is distinguishable by
 * behaviour; both are written the blob's way because the blob is the
 * specification.
 *
 * THE TWO MAPPING-PARAMETER POINTERS CROSS ON THE WAY DOWN.  Argument 6
 * arrives, is stored at +0x10, and is passed to `V90Phase4Modulator` as its
 * SIXTH argument, which lands at +0x50; argument 7 is stored at +0x14 and
 * passed as the FIFTH, landing at +0x4c.  Written in the order they were
 * received, the two silently swap, and nothing but two distinct addresses in
 * a test can tell.
 *
 * WHY THE SUB-OBJECTS ARE BUILT THROUGH asm() LABELS RATHER THAN `new`: the
 * argument is src/pump/v90/V90BitsToSymbol.cpp's and applies unchanged.  Two
 * of the three classes are not this file's to give an `operator new` to in any
 * case.
 */

#include <stddef.h>

#include "dsplib/sysdep.h"
#include "dsplib/V90BitsToSymbol.h"
#include "dsplib/V90Modulator.h"
#include "dsplib/V90Phase4Modulator.h"

extern "C" {
/*
 * The three constructors, by the names the blob calls at 0x1a628, 0x1a64f and
 * 0x1a6a2.  C1 is the complete-object variant, which is what a `new`
 * expression uses.  Only `V90Phase4Modulator`'s is declared through its class
 * as well, so only that one could have been written any other way.
 */
void v90mod_bts_ctor(void *self, unsigned int nofSymbols,
		     V90Parameters *params)
	asm("_ZN15V90BitsToSymbolC1EjP13V90Parameters");
void v90mod_p3_ctor(void *self, V90Parameters *params, unsigned int flag)
	asm("_ZN18V90Phase3ModulatorC1EP13V90Parametersj");
void v90mod_p4_ctor(void *self, V90Parameters *params, unsigned int flag,
		    V90BitsToSymbol *bts, V90MP *mp,
		    V90MappingParams *mappingParams,
		    V90MappingParams *mappingParams2, V90CP *cp,
		    unsigned int arg8)
	asm("_ZN18V90Phase4ModulatorC1EP13V90ParametersjP15V90BitsToSymbol"
	    "P5V90MPP16V90MappingParamsS7_P5V90CPj");

/* And the two destructors the release path calls, at 0x19bd3 and 0x19bf3. */
void v90mod_p3_dtor(void *self) asm("_ZN18V90Phase3ModulatorD1Ev");
void v90mod_p4_dtor(void *self) asm("_ZN18V90Phase4ModulatorD1Ev");
}

/*
 * The two sizes this file allocates but cannot see a `sizeof` for: the
 * classes are reached through an asm() label and are not declared here.  Both
 * are the blob's own `sizeof`, from the allocation immediately before the
 * constructor call.
 */
#define V90MOD_PHASE3_SIZE	0x398u
#define V90MOD_PHASE4_SIZE	0x2facu

#if __SIZEOF_POINTER__ == 4
#define V90MOD_OFF(field, off, tag) \
	typedef char v90mod_off_##tag[ \
	    ((int)__builtin_offsetof(V90Modulator, field) == (off)) ? 1 : -1]

V90MOD_OFF(phase2Info,		0x00, p2info);
V90MOD_OFF(jd,			0x04, jd);
V90MOD_OFF(v92Jd,		0x08, v92jd);
V90MOD_OFF(dil,			0x0c, dil);
V90MOD_OFF(mappingParams,	0x10, mp1);
V90MOD_OFF(mappingParams2,	0x14, mp2);
V90MOD_OFF(additionalCPinfo,	0x18, acp);
V90MOD_OFF(mp,			0x1c, mp);
V90MOD_OFF(cp,			0x20, cp);
V90MOD_OFF(params,		0x24, params);
V90MOD_OFF(sessionFlag,		0x28, flag);
V90MOD_OFF(phase3Modulator,	0x38, p3mod);
V90MOD_OFF(phase4Modulator,	0x3c, p4mod);
V90MOD_OFF(bitsToSymbol,	0x40, bts);
V90MOD_OFF(scrambler,		0x44, scrambler);
V90MOD_OFF(nofSymbols,		0x64, nofsym);
V90MOD_OFF(symbolBuf,		0x68, symbuf);
V90MOD_OFF(frameBuf,		0x6c, framebuf);
typedef char v90mod_size[(sizeof(V90Modulator) == 0x70) ? 1 : -1];
#endif

/*
 * ===========================================================================
 * V90Modulator::V90Modulator -- .text+0x1a560 (C1) and +0x1a6c0 (C2), 338
 * bytes each.
 *
 * Twelve arguments, eleven of them stored straight through, and five
 * allocations of which none is null-checked.  `pad_2c` is left exactly as it
 * was found.
 * ===========================================================================
 */
V90Modulator::V90Modulator(unsigned int n, V90Phase2Info *p2, V90Jd *jdArg,
			   V92Jd *v92JdArg, tagV90DILdescriptor *dilArg,
			   V90MappingParams *mpsA, V90MappingParams *mpsB,
			   tagV90AdditionalCPinfo *acp, V90CP *cpArg,
			   V90MP *mpArg, V90Parameters *par, unsigned int flag)
	: scrambler(0x12, 0x17, 0x63)
{
	V90BitsToSymbol *bts;
	V90Phase3Modulator *p3;
	V90Phase4Modulator *p4;

	mappingParams = mpsA;
	phase2Info = p2;
	jd = jdArg;
	v92Jd = v92JdArg;
	dil = dilArg;
	mappingParams2 = mpsB;
	params = par;
	cp = cpArg;
	mp = mpArg;
	additionalCPinfo = acp;
	sessionFlag = flag;
	nofSymbols = n;

	symbolBuf = (short *)sysdep_malloc(2 * n);
	frameBuf = sysdep_malloc(8 * n);

	bts = (V90BitsToSymbol *)sysdep_malloc(sizeof(V90BitsToSymbol));
	v90mod_bts_ctor(bts, 2 * n + n + 0x1388, params);
	bitsToSymbol = bts;

	p3 = (V90Phase3Modulator *)sysdep_malloc(V90MOD_PHASE3_SIZE);
	v90mod_p3_ctor(p3, params, sessionFlag);
	phase3Modulator = p3;

	p4 = (V90Phase4Modulator *)sysdep_malloc(V90MOD_PHASE4_SIZE);
	v90mod_p4_ctor(p4, params, sessionFlag, bitsToSymbol, mp,
		       mappingParams2, mappingParams, cp, 0xc);
	phase4Modulator = p4;
}

/*
 * ===========================================================================
 * V90Modulator::~V90Modulator -- .text+0x19b90 (D2) and +0x19c60 (D1), 199
 * bytes each.
 *
 * Five guarded releases in the order the fields sit, then the scrambler's
 * destructor, which the compiler emits after the body.  Nothing is nulled, so
 * a second destruction double-frees; that is the blob's behaviour and is left
 * alone.
 *
 * The phase 4 modulator is released BEFORE the converter it was handed, and
 * it does not release that converter itself -- its ownership flag is 1 here,
 * because this class passed a non-null third argument.  The order is
 * therefore not load-bearing and the guards are what matter.
 * ===========================================================================
 */
V90Modulator::~V90Modulator()
{
	if (phase3Modulator) {
		v90mod_p3_dtor(phase3Modulator);
		sysdep_free(phase3Modulator);
	}
	if (phase4Modulator) {
		v90mod_p4_dtor(phase4Modulator);
		sysdep_free(phase4Modulator);
	}
	if (bitsToSymbol) {
		bitsToSymbol->~V90BitsToSymbol();
		sysdep_free(bitsToSymbol);
	}
	if (symbolBuf)
		sysdep_free(symbolBuf);
	if (frameBuf)
		sysdep_free(frameBuf);
}

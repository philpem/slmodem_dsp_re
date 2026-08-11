/*
 * V90ModemCtor.cpp -- `V90Modem::V90Modem` (0x194e0 C1, 0x19740 C2, 597 B
 * each) and `V90Modem::~V90Modem` (0x192b0 D1, 0x19160 D2, 321 B each).
 *
 * The object map they establish is in include/dsplib/V90Modem.h, which says
 * for every field both what stores it and which argument slot of
 * `V90Modulator` or `V90Demodulator` types it.  This file is the code.
 *
 * ===========================================================================
 * WHY THIS IS NOT IN V90Modem.cpp
 * ===========================================================================
 *
 * Finding 1264: one source file is one mutation suite's namespace, and a
 * shared verbatim block makes an anchor match twice, which `tools/mutate.py`
 * calls UNUSABLE -- and unusable does not fail a run.  `V90Modem.cpp` is nine
 * `edprintf`/`dsplibs_debug_printf` calls and nothing else; this file is six
 * `sysdep_malloc` sites and eight `if (DSPLIB_DEBUG_ON())` gates.  Putting
 * them together would put two `if (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_
 * printf(` blocks in one file with nothing to tell them apart.  The split is
 * about the tier and not about the object; in the object the two are 224
 * bytes apart and plainly one translation unit.
 *
 * ===========================================================================
 * WHICH DEFINITION OF V90Parameters THIS FILE HAS, AND WHY IT MATTERS
 * ===========================================================================
 *
 * There are two (finding 1112): the NAMED map in `V90Parameters.h`, 0x558
 * bytes, and the BLOCK form `V90PreFilter.h` carries, bounded at 0x504.  They
 * cannot both be in one translation unit, and this constructor allocates the
 * thing -- `movl $0x558,(%esp); call sysdep_malloc` at 0x19551 -- so it has to
 * have the one whose `sizeof` is the allocation.  It takes `V90Parameters.h`.
 *
 * The cost is that `V90Demodulator.h` cannot come in, because it reaches
 * `V90PreFilter.h`.  So `V90Demodulator` is forward-declared and its
 * constructor is named by its MANGLED SYMBOL, which is VPcmXfTerm.cpp's
 * device for exactly this collision.  That leaves its allocation size as a
 * literal, and the literal is documented at its use.
 *
 * THE DIRECTION OF THE TRADE IS DELIBERATE.  With `V90Parameters.h` in scope
 * `sizeof(V90Parameters)` is 0x558 and right; with `V90PreFilter.h`'s in
 * scope it would be 0x504 and a later edit replacing the literal with
 * `sizeof` would under-allocate by 84 bytes and pass every test that does not
 * run under a checking allocator.  `V90Demodulator` is INCOMPLETE here, so
 * the same edit against it does not compile at all.  One of the two mistakes
 * is silent and the other is not, and this file is arranged so that only the
 * loud one is reachable.
 */

#include <stddef.h>

#include "dsplib/V90Modem.h"

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/modem_params.h"
#include "dsplib/sysdep.h"
#include "dsplib/V90Jd.h"
#include "dsplib/V90Modulator.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Phase2Info.h"
#include "dsplib/V92Jd.h"

/*
 * `V90PreFilter.h` spells this and this file cannot have that header; an
 * opaque-enum declaration may be repeated as long as the base agrees, and it
 * does.  The value comes from `_tagModemParameters::codecType` at +0x54,
 * which `modem_params.h` types `int`; the object loads it with a plain 32-bit
 * `mov 0x54(%edx),%ecx` and passes it straight on, so the cast below is a
 * conversion the compiler was going to make either way.
 */
enum __tHardwareCodecTypes__ : int;

/*
 * `sizeof(V90Demodulator)`, which this translation unit cannot spell; the
 * constructor's own `movl $0x298,(%esp)` at 0x196a6 is the measurement
 * (finding 291 and finding 1246: the allocation immediately before the
 * constructor IS the original compiler's `sizeof`), and
 * src/pump/v90/V90Demodulator.cpp asserts `sizeof(V90Demodulator) == 0x298`
 * in a translation unit that does have the type.  So the number is checked;
 * it is just not checked here.
 */
#define V90DEMODULATOR_BYTES	0x298

/*
 * The two sub-object constructors and the demodulator's destructor, by the
 * names the relocations carry.  `void *` throughout, which is
 * src/pump/v90/V92Modem.cpp's convention for exactly this: the call is a
 * relocation against a mangled name and nothing here needs the argument types
 * checked twice.
 *
 * AND THE CONSTRUCTORS ARE REACHED THIS WAY RATHER THAN BY PLACEMENT `new`,
 * which is the whole V.92 chain's reason and applies unchanged here:
 * `sysdep_malloc(n)` followed by the constructor with NO null test between
 * them is `new` over an INLINE `operator new`, this build is `-nostdinc++`
 * with no <new>, and a user-declared placement form makes GCC emit a null
 * test the blob does not have.
 */
/* C++ linkage is the default here; the `asm` label supplies the name. */
void v90m_parm_ctor(void *self, void *modemParams)
	asm("_ZN13V90ParametersC1EP19_tagModemParameters");
void v90m_ph2_ctor(void *self, void *params)
	asm("_ZN13V90Phase2InfoC1EP13V90Parameters");
void v90m_jd_ctor(void *self, void *params)
	asm("_ZN5V90JdC1EP13V90Parameters");
void v90m_jd92_ctor(void *self, void *params)
	asm("_ZN5V92JdC1EP13V90Parameters");
void v90m_mod_ctor(void *self, unsigned int nofSymbols, void *phase2Info,
		   void *jd, void *v92Jd, void *dil, void *mappingParams,
		   void *mappingParams2, void *additionalCPinfo, void *cp,
		   void *mp, void *params, unsigned int sessionFlag)
	asm("_ZN12V90ModulatorC1EjP13V90Phase2InfoP5V90JdP5V92JdP19tagV90DIL"
	    "descriptorP16V90MappingParamsS9_P22tagV90AdditionalCPinfoP5V90CP"
	    "P5V90MPP13V90Parametersj");
void v90m_dem_ctor(void *self, unsigned int levels, void *phase2, void *jd,
		   void *jdV92, void *dil, void *mappingParams1,
		   void *mappingParams2, void *cpInfo, void *cp, void *mp,
		   __tHardwareCodecTypes__ codec, void *params,
		   V90ComputationalMode compMode, unsigned int flag)
	asm("_ZN14V90DemodulatorC1EjP13V90Phase2InfoP5V90JdP5V92JdP19tagV90DIL"
	    "descriptorP16V90MappingParamsS9_P22tagV90AdditionalCPinfoP5V90CP"
	    "P5V90MP23__tHardwareCodecTypes__P13V90Parameters20V90Computationa"
	    "lModej");
void v90m_dem_dtor(void *self) asm("_ZN14V90DemodulatorD1Ev");

/*
 * Hold the compiler to the map in the header.  Guarded on a 32-bit pointer
 * because eight of the offsets are pointers and `make check64` lays them out
 * differently -- the layout the blob has is a 32-bit layout, and asserting it
 * on a host that cannot have it is asserting the wrong thing.
 *
 * `sizeof(V90Modem)` is NOT asserted here: src/pump/v90/VPcmFloModem.cpp
 * already asserts it, and it settled the number before any of these fields
 * existed.  A second copy would look like a second measurement.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90M_OFF(field, off, tag) \
	typedef char v90m_off_##tag[ \
		((int)__builtin_offsetof(V90Modem, field) == (off)) ? 1 : -1]

V90M_OFF(modulator,		0x0000, modulator);
V90M_OFF(demodulator,		0x0004, demodulator);
V90M_OFF(phase2Info,		0x0008, phase2info);
V90M_OFF(jd,			0x000c, jd);
V90M_OFF(jd92,			0x0010, jd92);
V90M_OFF(dil,			0x0014, dil);
V90M_OFF(mappingParams,		0x0018, mapparams);
V90M_OFF(mappingParamsAlt,	0x0668, mapparamsalt);
V90M_OFF(additionalCPinfo,	0x0cb8, cpinfo);
V90M_OFF(mp,			0x0cd0, mp);
V90M_OFF(cp,			0x0df4, cp);
V90M_OFF(ptr_49b4,		0x49b4, ptr49b4);
V90M_OFF(sessionFlag,		0x49b8, sessionflag);
V90M_OFF(side,			0x49bc, side);

/*
 * The four sizes this file's allocations depend on, asserted where they are
 * used rather than only in the classes' own translation units.  A batch that
 * moved a field of any of them would otherwise change what this constructor
 * allocates without changing a line of it.
 */
typedef char v90m_parm_size[(sizeof(V90Parameters) == 0x558) ? 1 : -1];
typedef char v90m_ph2_size[(sizeof(V90Phase2Info) == 0x24) ? 1 : -1];
typedef char v90m_jd_size[(sizeof(V90Jd) == 0x90) ? 1 : -1];
typedef char v90m_jd92_size[(sizeof(V92Jd) == 0xdc) ? 1 : -1];
typedef char v90m_mod_size[(sizeof(V90Modulator) == 0x70) ? 1 : -1];
#endif

/*
 * The constructor.
 *
 * TWO MEMBER CONSTRUCTIONS COME FIRST AND THEY ARE NOT STATEMENTS: `V90MP`
 * at +0xcd0 and `V90CP` at +0xdf4 are called before the first diagnostic, in
 * DECLARATION order, which is what a member with a default constructor and no
 * mem-initialiser gives.  The destructor runs them in the opposite order,
 * `~V90CP` then `~V90MP`, which is the other half of the same evidence.
 * Neither appears in the body below because neither is written there.
 *
 * THE SWITCH IS ON THE MEMBER, NOT THE PARAMETER.  0x195de reloads
 * `0x49bc(%esi)` rather than reusing the register it stored from, so the
 * source reads `this->side` back.  V92Modem's constructor does the same
 * (V92Modem.h), and it is the kind of thing that survives only if it is
 * written the way the object has it.
 */
V90Modem::V90Modem(V90ModemSide modemSide, _tagModemParameters *modemParams,
		   tagV90DILdescriptor *dilDescriptor, unsigned int nofSymbols,
		   V90ComputationalMode compMode, unsigned int flag)
{
	void *p;

	/*
	 * "Digital" when the argument is zero and "Analog" otherwise -- and
	 * the test is on the ARGUMENT here, before anything is stored, where
	 * the switch below is on the member.  0x19514 tests `%ebx`, which is
	 * still `0x54(%esp)`.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modem Construction (as %s Modem)\r\n",
				     modemSide ? "Analog" : "Digital");

	printTitle();

	side = modemSide;
	dil = dilDescriptor;
	sessionFlag = flag;

	p = sysdep_malloc(sizeof(V90Parameters));
	v90m_parm_ctor(p, modemParams);
	ptr_49b4 = (V90Parameters *)p;

	/*
	 * The V90Parameters pointer is read back OUT OF THE OBJECT for each
	 * of the next three, not kept in a register: `mov 0x49b4(%esi),%ecx`
	 * at 0x19581, `%edx` at 0x195a4 and `%eax` at 0x195c9.  Three
	 * reloads, three statements.
	 */
	p = sysdep_malloc(sizeof(V90Phase2Info));
	v90m_ph2_ctor(p, ptr_49b4);
	phase2Info = (V90Phase2Info *)p;

	p = sysdep_malloc(sizeof(V90Jd));
	v90m_jd_ctor(p, ptr_49b4);
	jd = (V90Jd *)p;

	p = sysdep_malloc(sizeof(V92Jd));
	v90m_jd92_ctor(p, ptr_49b4);
	jd92 = (V92Jd *)p;

	/*
	 * NEITHER ARM IS THE DEFAULT AND THE DEFAULT WRITES NOTHING.  On any
	 * value but 0 and 1 this returns with `modulator` and `demodulator`
	 * holding whatever the storage held -- see V90Modem.h, and finding
	 * 1323 for V92Modem's identical shape.
	 *
	 * THE TWO ARMS WRITE THEIR NULL AT OPPOSITE ENDS, and that is the
	 * object's order rather than a tidy-up: on the modulator arm
	 * `movl $0x0,0x4(%esi)` at 0x19677 comes AFTER the construction, and
	 * on the demodulator arm `movl $0x0,(%esi)` at 0x196a0 comes BEFORE
	 * the allocation.  Neither store can be moved across the constructor
	 * call, which might alias `*this`, so both are where the source put
	 * them.
	 */
	switch (side) {
	case V90_MODEM_SIDE_DIGITAL:
		p = sysdep_malloc(sizeof(V90Modulator));
		v90m_mod_ctor(p, nofSymbols, phase2Info, jd, jd92, dil,
			      &mappingParams, &mappingParamsAlt,
			      &additionalCPinfo, &cp, &mp, ptr_49b4,
			      sessionFlag);
		modulator = (V90Modulator *)p;
		demodulator = 0;
		break;

	case V90_MODEM_SIDE_ANALOG:
		modulator = 0;
		p = sysdep_malloc(V90DEMODULATOR_BYTES);
		v90m_dem_ctor(p, nofSymbols, phase2Info, jd, jd92, dil,
			      &mappingParams, &mappingParamsAlt,
			      &additionalCPinfo, &cp, &mp,
			      (__tHardwareCodecTypes__)modemParams->codecType,
			      ptr_49b4, compMode, sessionFlag);
		demodulator = (V90Demodulator *)p;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90Modem Constructor: Illegal modemSide\r\n");
		break;
	}
}

/*
 * The destructor.
 *
 * SIX POINTERS IN FORWARD ORDER, then the two embedded members in reverse.
 * The forward order is the giveaway that the six are statements in the body
 * and not member destruction -- a compiler-generated one would run them
 * backwards, as it does for `cp` and `mp` below, which are not written here.
 *
 * `phase2Info` IS FREED WITHOUT A DESTRUCTOR CALL: 0x192ed tests it and
 * 0x193a0 goes straight to `sysdep_free` with no `_ZN13V90Phase2InfoD` in
 * between.  So the class is trivially destructible, which is what
 * V90Phase2Info.h describes, and a `delete`-shaped statement here would emit
 * exactly that.
 */
V90Modem::~V90Modem()
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modem Destruction\r\n");

	/*
	 * `cmpl $0x1,0x49bc(%esi); jbe` -- UNSIGNED, which is why
	 * `V90ModemSide` has an `unsigned int` base.  See V90Modem.h.
	 */
	if (side > 1) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90Modem Destructor: Illegal modemSide\r\n");
	}

	if (modulator != 0) {
		modulator->~V90Modulator();
		sysdep_free(modulator);
	}
	if (demodulator != 0) {
		v90m_dem_dtor(demodulator);
		sysdep_free(demodulator);
	}
	if (phase2Info != 0)
		sysdep_free(phase2Info);
	if (jd != 0) {
		jd->~V90Jd();
		sysdep_free(jd);
	}
	if (jd92 != 0) {
		jd92->~V92Jd();
		sysdep_free(jd92);
	}
	if (ptr_49b4 != 0) {
		ptr_49b4->~V90Parameters();
		sysdep_free(ptr_49b4);
	}
}

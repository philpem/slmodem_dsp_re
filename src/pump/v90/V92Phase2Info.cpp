/*
 * V92Phase2Info.cpp -- what V.92 Phase 2 concluded, as the constructor builds
 * it.
 *
 * Reconstructed from dsplibs.o.  One of the class's three members,
 * `V92Phase2Info(V92Parameters *)` at 0x15f70; `include/dsplib/
 * V92Phase2Info.h` carries the object map and says where each field came
 * from.  `printInfo` and `setToDefault` are not written here.
 *
 * THIS FILE DID NOT EXIST UNTIL THE CONSTRUCTOR DID, and that is why the
 * offset assertions below moved here from src/pump/v90/VPcmFloModem.cpp.
 * They were parked there because it was "the only translation unit that uses
 * the class and it has no .cpp of its own", which stopped being true with
 * this file.  The set is unchanged apart from the four entries the
 * constructor added.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x4(%esp),%edx` with the parameter block at `0x8(%esp)` --
 * not %ecx, so this is not thiscall and nothing here needs an attribute
 * (finding F215).
 *
 * Built -fno-exceptions -fno-rtti -nostdinc++ like the rest of the C++ here;
 * see the Makefile.  No virtuals, no allocation, no static data members.
 */

#include <stddef.h>

#include "dsplib/V92Parameters.h"
#include "dsplib/V92Phase2Info.h"

/*
 * Hold the compiler to the map in the header, the way V90Jd.cpp does:
 * tools/offcheck.py only parses `struct name {` out of include/dsplib, so a
 * C++ class has to assert its own.  This is the check that catches an object
 * right in size and wrong by four in every offset.
 *
 * Guarded on a 32-bit pointer: `params` is a pointer, so on a 64-bit host it
 * sits at +0x30 and the object is 0x38 rather than 0x2c.  `make check64`
 * proves the CODE does not depend on 32-bit; the layout the blob has is a
 * 32-bit layout, and asserting it on a host that cannot have it is asserting
 * the wrong thing.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V92P2I_OFF(field, off, tag) \
	typedef char v92p2i_off_##tag[ \
	    ((int)__builtin_offsetof(V92Phase2Info, field) == (off)) ? 1 : -1]

V92P2I_OFF(pcmType,			0x00, pcmtype);
V92P2I_OFF(rtd,				0x04, rtd);
V92P2I_OFF(Uinfo,			0x08, uinfo);
V92P2I_OFF(maxTxPower,			0x09, maxtxpower);
V92P2I_OFF(txPowerMeasurementPoint,	0x0c, txpmp);
V92P2I_OFF(shortPhase2Local,		0x10, sp2local);
V92P2I_OFF(v92CapabilitiesLocal,	0x11, v92local);
V92P2I_OFF(shortPhase2Remote,		0x12, sp2remote);
V92P2I_OFF(v92CapabilitiesRemote,	0x13, v92remote);
V92P2I_OFF(nofFilterSections,		0x14, nofsections);
V92P2I_OFF(maxTotalNofCoeffs,		0x15, maxtotal);
V92P2I_OFF(maxNofCoeffsInEachSection,	0x16, maxeach);
V92P2I_OFF(v90UseHighCarrier,		0x17, highcarrier);
V92P2I_OFF(array_18,			0x18, a18);
V92P2I_OFF(array_1c,			0x1c, a1c);
V92P2I_OFF(L2,				0x20, l2);
V92P2I_OFF(array_24,			0x24, a24);
V92P2I_OFF(params,			0x28, params);

typedef char v92p2i_size[(sizeof(V92Phase2Info) == 0x2c) ? 1 : -1];

#endif /* 32-bit host */

/*
 * ===========================================================================
 * The constructor, 0x15f70.
 *
 * FOURTEEN STORES, and they fall into four groups.
 *
 * Five copies out of the parameter block, of which two go through a boolean:
 * `pcmType` and `txPowerMeasurementPoint` are `cmpl $0x0` + `setne`, so what
 * lands is 0 or 1 and never the parameter's own value.  `rtd` is a whole word;
 * `Uinfo` and `maxTxPower` are whole-word loads with byte stores, so only the
 * low byte survives.  All five mirror V90Phase2Info's, out of V92Parameters
 * +0x08..+0x18 rather than V90Parameters +0x18..+0x28.
 *
 * Three more copies, same load-word-store-byte shape, into what the header
 * used to call `pad_14[3]`: the V.92 filter geometry.  These are the reason
 * a padding run is only padding until a third function is read.
 *
 * Four constants: the two remote capability bytes and the local short-phase-2
 * byte are cleared, and `v92CapabilitiesLocal` is set to 1.  So a V.92
 * capability is asserted locally at construction and both remote bytes start
 * at "not offered" -- `VPcmFloModem::setPhaseIIinfo` fills the remote pair in
 * later out of INFO0.  `v90UseHighCarrier` is cleared too.
 *
 * And the parameter block itself at +0x28, stored FIRST, which is what sizes
 * the object.
 *
 * The store order below is the object's, including the two places where it
 * interleaves a constant between two copies.  GCC is free to reorder and does;
 * what is not free is which byte gets which value.
 * ===========================================================================
 */
V92Phase2Info::V92Phase2Info(V92Parameters *p)
{
	params = p;
	pcmType = (p->V92_PHASE2_INFO_A_OR_MU != 0);
	rtd = p->V92_PHASE2_INFO_RTD;
	Uinfo = (unsigned char)p->V92_PHASE2_INFO_UINFO;
	maxTxPower = (unsigned char)p->V92_PHASE2_INFO_MAX_TX_POWER;

	shortPhase2Remote = 0;
	v92CapabilitiesRemote = 0;
	shortPhase2Local = 0;
	v92CapabilitiesLocal = 1;

	txPowerMeasurementPoint =
	    (p->V92_PHASE2_INFO_TX_POWER_MEASURE_POINT != 0);

	nofFilterSections = (unsigned char)p->V92_NOF_FILTER_SECTIONS;
	maxTotalNofCoeffs = (unsigned char)p->V92_MAX_TOTAL_NOF_COEFFS;
	v90UseHighCarrier = 0;
	maxNofCoeffsInEachSection =
	    (unsigned char)p->V92_MAX_NOF_COEFFS_IN_EACH_SECTION;
}

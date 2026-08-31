/*
 * V92Phase2Info.cpp -- what V.92 Phase 2 concluded, as the constructor builds
 * it.
 *
 * Reconstructed from dsplibs.o.  All three of the class's members --
 * `setToDefault()` at 0x15f10, `V92Phase2Info(V92Parameters *)` at 0x15f70
 * and `printInfo() const` at 0x16030; `include/dsplib/V92Phase2Info.h`
 * carries the object map and says where each field came from.
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
#include "dsplib/debug.h"
#include "dsplib/encode.h"

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
/*
 * setToDefault -- 0x15f10, 87 bytes, immediately BEFORE the constructor in
 * the blob as here.  The constructor's fourteen stores minus the one that
 * sizes the object: `params` is read back from +0x28 instead of stored.
 * The store order below is the object's, as in the constructor.  The local
 * is `blk` (the constructor's is `p`) and the constants carry offset
 * comments because the mutation suite anchors on the constructor's exact
 * text and an anchor must match exactly once (`make refs`); neither an
 * identifier nor a comment moves codegen.
 */
void
V92Phase2Info::setToDefault()
{
	V92Parameters *blk = params;

	pcmType = (blk->V92_PHASE2_INFO_A_OR_MU != 0);
	rtd = blk->V92_PHASE2_INFO_RTD;
	Uinfo = (unsigned char)blk->V92_PHASE2_INFO_UINFO;
	maxTxPower = (unsigned char)blk->V92_PHASE2_INFO_MAX_TX_POWER;

	shortPhase2Remote = 0;		/* +0x12 */
	v92CapabilitiesRemote = 0;	/* +0x13 */
	shortPhase2Local = 0;		/* +0x10 */
	v92CapabilitiesLocal = 1;	/* +0x11 */

	txPowerMeasurementPoint =
	    (blk->V92_PHASE2_INFO_TX_POWER_MEASURE_POINT != 0);

	nofFilterSections = (unsigned char)blk->V92_NOF_FILTER_SECTIONS;
	maxTotalNofCoeffs = (unsigned char)blk->V92_MAX_TOTAL_NOF_COEFFS;
	v90UseHighCarrier = 0;		/* +0x17 */
	maxNofCoeffsInEachSection =
	    (unsigned char)blk->V92_MAX_NOF_COEFFS_IN_EACH_SECTION;
}

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

/*
 * ===========================================================================
 * printInfo -- 0x16030, 572 bytes.
 *
 * The same char-and-two-ints fixed-point split as V90Phase2Info::printInfo,
 * and the three helpers below are that file's, duplicated because they are
 * file-static there as the original's were here; finding F256 carries the
 * long-double measurement they rest on.
 *
 * THE GATING IS THE OBJECT'S AND IT IS MIXED.  The first four lines go
 * through `dsplibs_debug_printf`, each behind its own `dsplibs_debug_level`
 * test (the object re-reads the level before every one); the middle four go
 * through `edprintf` UNGATED -- edprintf self-gates one level down -- and
 * the L2 loop runs its twenty-one iterations at every level with only the
 * printing gated, the level re-read once per iteration, exactly as
 * V90Phase2Info's loop does.
 *
 * `maxTxPower` prints `(maxTxPower + 1) * -0.5` under a [dBm0] label: the
 * field is a code in half-decibel steps, V90Phase2Info.h's reading, and the
 * product is always negative so its sign prints '-' on every input.
 * ===========================================================================
 */

/* `0 < v`, C0 of an fcom against a pushed zero; see V90Phase2Info.cpp. */
static char
sign_of(float v)
{
	return (0.0f < v) ? '+' : '-';
}

/* `fabs` then a truncating `fistpl`: the magnitude, toward zero. */
static int
whole_of(float v)
{
	return (int)__builtin_fabsf(v);
}

/* The scaled fraction, absolute; V90Phase2Info.cpp's derivation (F256). */
static int
frac_of(float v, float scale)
{
	long double x = (long double)v;
	long double d = (long double)(int)v - x;
	int n = (int)(d * (long double)scale);

	return (n < 0) ? -n : n;
}

void
V92Phase2Info::printInfo() const
{
	unsigned int i;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Phase2Info: pcmType = %s\r\n",
				     pcmType == 1 ? "A_LAW" : "MU_LAW");

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Phase2Info: rtd = %d\r\n", rtd);

	if (DSPLIB_DEBUG_ON()) {
		float p = (float)((int)maxTxPower + 1) * -0.5f;

		dsplibs_debug_printf(
		    "V92Phase2Info: maxTxPower [dBm0]  = %c%d.%01d\r\n",
		    sign_of(p), whole_of(p), frac_of(p, 10.0f));
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V92Phase2Info: txPowerMeasurementPoint = %s\r\n",
		    txPowerMeasurementPoint == 1 ? "CodecOutput"
						 : "DigitalModemTerminal");

	edprintf("V92Phase2Info: ShortPhase2: local=%d , remote=%d\r\n",
		 shortPhase2Local, shortPhase2Remote);
	edprintf("V92Phase2Info: v92Capabilities: local=%d , remote=%d\r\n",
		 v92CapabilitiesLocal, v92CapabilitiesRemote);
	edprintf("V92Phase2Info: v90UseHighCarrier = %d\r\n",
		 v90UseHighCarrier);
	edprintf("V92Phase2Info: Uinfo = %d\r\n", Uinfo);

	for (i = 0; i < V92PHASE2INFO_L2; i++) {
		if (DSPLIB_DEBUG_ON()) {
			float v = L2[i];

			dsplibs_debug_printf(
			    "V92Phase2Info: L2[%d] = %c%d.%03d\r\n", (int)i,
			    sign_of(v), whole_of(v), frac_of(v, 1000.0f));
		}
	}
}

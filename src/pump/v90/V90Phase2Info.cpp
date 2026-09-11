/*
 * V90Phase2Info.cpp -- printing what Phase 2 measured.
 *
 * Reconstructed from dsplibs.o.  One of the class's three members,
 * `printInfo() const`; `include/dsplib/V90Phase2Info.h` carries the object
 * map and says where each field in it came from.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- after `push %esi; push %ebx; sub $0x24,%esp` the function reads
 * it from `0x30(%esp)` -- not %ecx, so this is not thiscall and nothing here
 * needs an attribute (finding F215).
 *
 * SIX CALL SITES, GATED TWO DIFFERENT WAYS, and the difference is the whole
 * shape of the function.
 *
 * Five of them go straight to `dsplibs_debug_printf` behind an explicit
 * `cmpl $0x1,dsplibs_debug_level; jbe` -- DSPLIB_DEBUG_ON().  The sixth, the
 * Uinfo line, calls `edprintf` with no gate at all, because `edprintf`
 * applies its own (include/dsplib/encode.h): at level 0 and 1 it still
 * formats and encodes the message and only the final print is skipped.  That
 * asymmetry is visible in the object as four chained tests before the
 * `edprintf` call and one more inside the loop after it, and it is the reason
 * the four "header" lines and the Uinfo line cannot be collapsed into one
 * gate.
 *
 * The four chained tests are one `if` each in the source and not one `if`
 * with four bodies: GCC re-loads and re-compares `dsplibs_debug_level` before
 * every one of them, which is what a separate statement compiles to and what
 * a shared block would not.  Their order is the order of the fall-through
 * chain -- each test that fails jumps to the `edprintf` at the same label.
 *
 * NO %f ANYWHERE.  The diagnostic channel has no floating-point conversion,
 * so both float values are printed as a sign character, an integer magnitude
 * and a scaled fractional part, exactly as `V90PreFilter::setParamEia6` does
 * it.  See `sign_of`, `whole_of` and `frac_of` below for why the intermediate
 * is a `long double`.
 *
 * Built -fno-exceptions -fno-rtti -nostdinc++ like the rest of the C++ here;
 * see the Makefile.  No virtuals, no allocation, no static data members.
 */

#include <stddef.h>
#include <math.h>

#include "dsplib/V90Phase2Info.h"
#include "dsplib/V90Parameters.h"	/* the constructor's five fields  */
#include "dsplib/V90Phase3Modulator.h"	/* PcmType, and only for that     */
#include "dsplib/debug.h"
#include "dsplib/encode.h"

/*
 * Hold the compiler to the map in the header, the way V90Jd.cpp does:
 * tools/offcheck.py only parses `struct name {` out of include/dsplib, so a
 * C++ class has to assert its own.  This is the check that catches an object
 * right in size and wrong by four in every offset, which is the failure
 * docs/v90cpp.md warns about for the classes that do have a vptr.
 *
 * Guarded on a 32-bit pointer, exactly as V90Phase3Modulator.cpp is: `params`
 * is a pointer, so on a 64-bit host it sits at +0x28 and the object is 0x30
 * rather than 0x24.  `make check64` exists to prove the *code* does not depend
 * on 32-bit; the layout the blob has is a 32-bit layout, and asserting it on a
 * host that cannot have it is asserting the wrong thing.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V90P2I_OFF(field, off, tag) \
	typedef char v90p2i_off_##tag[ \
	    ((int)__builtin_offsetof(V90Phase2Info, field) == (off)) ? 1 : -1]

V90P2I_OFF(pcmType,			0x00, pcmtype);
V90P2I_OFF(rtd,				0x04, rtd);
V90P2I_OFF(Uinfo,			0x08, uinfo);
V90P2I_OFF(maxTxPower,			0x09, maxtxpower);
V90P2I_OFF(txPowerMeasurementPoint,	0x0c, txpmp);
V90P2I_OFF(array_10,			0x10, array10);
V90P2I_OFF(array_14,			0x14, array14);
V90P2I_OFF(L2,				0x18, l2);
V90P2I_OFF(array_1c,			0x1c, array1c);
V90P2I_OFF(params,			0x20, params);

typedef char v90p2i_size[(sizeof(V90Phase2Info) == 0x24) ? 1 : -1];

#endif /* 32-bit host */

/*
 * ===========================================================================
 * The constructor, 0x2a990, and it is what SIZES the object: `params` at
 * +0x20 is a four-byte store and nothing else in the class reaches that far,
 * so 0x24 is the object and 0x1c is only as far as `printInfo` looks.
 *
 * FIVE COPIES, TWO OF THEM THROUGH A BOOLEAN.  `pcmType` and
 * `txPowerMeasurementPoint` are `cmpl $0x0,...; setne %al` -- the parameter
 * is tested, not carried, so a PHASE2_INFO_A_OR_MU of 7 arrives here as 1.
 * The other three are plain copies, and two of those are a whole-word load
 * with a byte store, so only the low byte of `PHASE2_INFO_UINFO` and
 * `PHASE2_INFO_MAX_TX_POWER` survives.
 *
 * `pcmType` IS NOT `PcmType` HERE.  The header spells it `int` and this is
 * why: what the object stores is a `setne` result, and PcmType's own
 * PCM_TYPE_A_LAW happens to be 1, so the two agree on the value and not on
 * the reasoning.
 *
 * The four `pad`/array fields between +0x10 and +0x1f are NOT written here.
 * `VPcmFloModem::setPhaseIIinfo` installs them later; a caller that reads
 * `L2` before that runs reads whatever was in the storage.
 * ===========================================================================
 */
V90Phase2Info::V90Phase2Info(V90Parameters *p)
{
	params = p;
	pcmType = (p->PHASE2_INFO_A_OR_MU != 0);
	rtd = p->PHASE2_INFO_RTD;
	Uinfo = (unsigned char)p->PHASE2_INFO_UINFO;
	maxTxPower = (unsigned char)p->PHASE2_INFO_MAX_TX_POWER;
	txPowerMeasurementPoint = (p->PHASE2_INFO_TX_POWER_MEASURE_POINT != 0);
}

/*
 * setToDefault -- 0x2a950, 49 bytes: the constructor's five copies again,
 * reading the parameter block back from `params` instead of taking it, and
 * storing nothing else -- the four arrays and `params` itself survive.
 * The two `setne` stores and the two byte narrowings are the constructor's,
 * unchanged.  The local is `blk` rather than the constructor's `p` because
 * the mutation suite anchors on the constructor's exact text and an anchor
 * must match exactly once (`make refs`); an identifier moves no codegen.
 */
void
V90Phase2Info::setToDefault()
{
	V90Parameters *blk = params;

	pcmType = (blk->PHASE2_INFO_A_OR_MU != 0);
	rtd = blk->PHASE2_INFO_RTD;
	Uinfo = (unsigned char)blk->PHASE2_INFO_UINFO;
	maxTxPower = (unsigned char)blk->PHASE2_INFO_MAX_TX_POWER;
	txPowerMeasurementPoint =
	    (blk->PHASE2_INFO_TX_POWER_MEASURE_POINT != 0);
}

/*
 * ===========================================================================
 * Printing a float without a %f.
 *
 * The object prints `+d.ddd` from three integer arguments: the sign, the
 * magnitude's whole part, and the fractional part scaled by a power of ten.
 *
 * THE INTERMEDIATE IS A `long double`, AND ON THIS TARGET IT DID NOT HAVE TO
 * BE.  The object computes the scaled fraction entirely on the x87 stack --
 * `fsubp` then `fmuls` then `fistpl` -- so the product is rounded once, to 64
 * significand bits, and only then truncated.  The obvious argument for `long
 * double` is that the same expression in `float` would round the product to 24
 * bits first and a value just under an integer could cross it.  **That
 * argument is wrong here, and it was measured rather than believed:**
 * `-mfpmath=387` with GCC's default excess precision keeps a `float` product
 * in an 80-bit register until the `fistpl` as well, and the two spellings
 * agree on every one of 2,390,535,529 floats sampled across the representable
 * range, at both scales this function is called with.  Finding F256.
 *
 * `long double` stays because it is the spelling that does not depend on the
 * excess precision being there, and because src/pump/v90/V90PreFilter.cpp
 * already uses it for the same job -- not because a test can tell them apart.
 * ===========================================================================
 */

/*
 * `fldz; fcomps; fnstsw; sahf; sbb; and $-2; add $0x2d` -- 0x2d is '-' and
 * 0x2b is '+', and the borrow is C0, which the comparison sets when the zero
 * it pushed is BELOW the value.  So the test is `0 < v` and not `v >= 0`:
 * zero prints as negative, which is a quirk of the object and is reproduced
 * rather than tidied.
 */
static char
sign_of(float v)
{
	return (0.0f < v) ? '+' : '-';
}

/* `fabs` then a truncating `fistpl`: the magnitude, toward zero. */
static int
whole_of(float v)
{
	return (int)fabsf(v);
}

/*
 * The scaled fraction, always non-negative -- the object follows the
 * truncation with `cltd; xor %edx,%eax; sub %edx,%eax`, which is abs().  The
 * subtraction is (int)v - v in both of the object's two copies, so the sign
 * it produces is the opposite of the value's and abs() is what makes the two
 * agree.
 *
 * Which is also why the ORDER of that subtraction is not testable: reversing
 * it negates the product, negates the truncation, and the abs() cancels both.
 * Measured over the same 2,390,535,529 floats, at both scales -- zero
 * disagreements.  The order below is the object's, taken from the
 * disassembly, and it is written that way to match rather than because a test
 * requires it.  What is held fixed is the abs() on the result.  Finding F256.
 */
static int
frac_of(float v, float scale)
{
	long double x = (long double)v;
	long double d = (long double)(int)v - x;
	int n = (int)(d * (long double)scale);

	return (n < 0) ? -n : n;
}

void
V90Phase2Info::printInfo() const
{
	unsigned int i;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Phase2Info: pcmType = %s\r\n",
				     pcmType == PCM_TYPE_A_LAW ? "A_LAW"
							       : "MU_LAW");

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Phase2Info: rtd = %d\r\n", rtd);

	if (DSPLIB_DEBUG_ON()) {
		/*
		 * `flds -0.5; fildl maxTxPower+1; fmul %st(1),%st`.  Both
		 * operands are exact and the product has at most the one
		 * fractional bit, so this is the one float here that no
		 * rounding argument applies to.
		 */
		float p = (float)((int)maxTxPower + 1) * -0.5f;

		dsplibs_debug_printf(
		    "V90Phase2Info: maxTxPower [dBm0]  = %c%d.%01d\r\n",
		    sign_of(p), whole_of(p), frac_of(p, 10.0f));
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90Phase2Info: txPowerMeasurementPoint = %s\r\n",
		    txPowerMeasurementPoint == 1 ? "CodecOutput"
						 : "DigitalModemTerminal");

	/* Not gated here.  `edprintf` gates its own print; see the header. */
	edprintf("V90Phase2Info: Uinfo = %d\r\n", Uinfo);

	/*
	 * `inc %ebx; cmp $0x14,%ebx; jbe` -- unsigned, 0 through 20 inclusive,
	 * and the counter is incremented before the test, so the loop runs its
	 * twenty-one iterations at every debug level.  Only the printing is
	 * gated, and the gate is INSIDE: the object re-reads
	 * `dsplibs_debug_level` once per iteration.
	 */
	for (i = 0; i < V90PHASE2INFO_L2; i++) {
		if (DSPLIB_DEBUG_ON()) {
			float v = L2[i];

			dsplibs_debug_printf(
			    "V90Phase2Info: L2[%d] = %c%d.%03d\r\n", (int)i,
			    sign_of(v), whole_of(v), frac_of(v, 1000.0f));
		}
	}
}

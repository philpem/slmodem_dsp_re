/*
 * v34pcmif.c -- the V.90/K56Flex side's hooks into the V.34 machinery.
 *
 * A cluster of very small functions at .text+0x9230 and +0xa250 onwards,
 * separate from V34RX.c and from the handshake: the PCM modem's view of what
 * V.34 is doing.  Reconstructed one at a time as the V.34 code that calls
 * them arrives, rather than as a module, because that is the order the call
 * graph gives (tools/callgraph.py).
 *
 * They also fix the object's real extent.  `VPcmV34LogTimingOffset` writes
 * at +0xac0c, past where v34fsk.h had the struct ending -- so the V.34
 * object is at least 0xac0e bytes and the tail of it belongs to this
 * interface rather than to the datapump.
 *
 * WHICH TRANSLATION UNIT THIS IS.  All of it belongs to `VPcmV34Main.cpp`:
 * these entry points are interleaved in .text with mangled names from that
 * file (`_Z25SetUpstreamModulationInfoP12tagV34Object` at 0x6200,
 * `_Z14getMPrecvdBitsP12tagV34Object` at 0x9250), so the TU is C++ and every
 * function here is one of its `extern "C"` exports.  The file stays `.c`,
 * which is the choice the tree already made when `VPcmV34LogTimingOffset`
 * was written: nothing about these functions is C++, and splitting the two
 * halves of one TU by language would be a worse map than splitting it by
 * role.  The rest of `VPcmV34Main.cpp` -- the C++ half -- is not
 * reconstructed.
 *
 * The `V34XF_` prefix is the object's, and marks the direction: these are
 * the functions the *V.34* code calls to tell the PCM side something, or to
 * ask it where to put something.  The traffic the other way is `VPcmV34*`.
 */

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34pcmif.h"

void
VPcmV34LogTimingOffset(void *objp, short offset)
{
	struct v34_object *obj = (struct v34_object *)objp;

	obj->fac0c = offset;
}

/*
 * Set the transmit scale.
 *
 * NO PARAMETER AND NO CHOICE: 0x16a1 is built in, stored, and then reported
 * through `edprintf` -- which is not behind a debug-level test here, because
 * `edprintf` applies its own.  Every other diagnostic in this file is gated
 * at its call site; this one is not, and that difference is the object's.
 */
void
VPcmV34SetTxScale(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	obj->f25d4 = 0x16a1;
	edprintf("VPcmV34SetTxScale: tx scale set to %d\r\n", 0x16a1);
}

/*
 * ---------------------------------------------------------------------------
 * Two address handouts and one scalar read.
 *
 * The first two are `return &obj->field` and nothing else -- no bounds, no
 * copy, no state.  They exist because the caller is in another translation
 * unit and the V.34 object's layout is private to this one.
 */

double *
V34XF_GetProbeResultsPtr(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	return obj->probe_results;
}

int *
V34XF_GetInfo0BitsPtr(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	return obj->info0_bits;
}

/*
 * The round-trip delay, plus 480.
 *
 * READ UNSIGNED, RETURNED SIGNED, AND THE WRAP IS REAL: the object does
 * `movzwl` on the field, adds 0x1e0 in 32 bits, then `cwtl` -- so a stored
 * delay above 0xfe20 comes back as a small negative number rather than as
 * anything clamped.  `v34handshak` both writes and reads +0xaa7e as a signed
 * short elsewhere, so the unsigned load here is this function's alone.
 *
 * 480 samples is 60 ms at 8 kHz.  What it is compensating for is on the
 * caller's side and not visible here.
 */
short
V34XF_GetRTD(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	return (short)((unsigned short)obj->rtd + 0x1e0);
}

/*
 * ---------------------------------------------------------------------------
 * Four indications: the V.34 handshake telling the PCM side that a phase-3
 * message has arrived.
 *
 * Each one moves `v90_receiver` (or `k56flex_receiver`) forward, and the
 * numbers are a ratchet rather than a set of flags -- see the TRN2d case,
 * which is the only one that reads the field before writing it.
 */

/*
 * A Jd has been received.
 *
 * `constel` and `silence_scr` are the two bits the message carried, and they
 * pick between three values for +0x382, which nothing in this object reads.
 * The silence flag wins outright: with it set the constellation size is not
 * consulted at all.
 */
void
V34XF_IndicateJdReceived(void *objp, unsigned char constel,
			 unsigned char silence_scr)
{
	struct v34_object *obj = (struct v34_object *)objp;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"V34Main: IndicateJdReceived - constel size = %d, "
			"silence SCR = %d\r\n",
			constel, silence_scr);

	obj->v90_receiver = 3;

	if (silence_scr != 0)
		obj->f382 = 0;
	else if (constel != 0)
		obj->f382 = (short)0x89b0;
	else
		obj->f382 = (short)0x8990;
}

/*
 * A DIL has been received.
 *
 * Same two values for +0x382 on the same constellation-size test, and one
 * extra store: +0x25c0 is cleared.  That field is the transmit block's, and
 * the object reaches it as `0x3a4(obj + 0x221c)` -- through the transmit
 * queue's base rather than the object's -- which is what says it belongs to
 * the transmitter and not here.
 */
void
V34XF_IndicateDilReceived(void *objp, unsigned char constel)
{
	struct v34_object *obj = (struct v34_object *)objp;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"V34Main: IndicateDilReceived - constel size = %d\r\n",
			constel);

	obj->v90_receiver = 6;
	obj->f25c0 = 0;

	if (constel != 0)
		obj->f382 = (short)0x89b0;
	else
		obj->f382 = (short)0x8990;
}

/*
 * A TRN2d has been received.
 *
 * The only one that reads `v90_receiver` first, and it is a RATCHET WITH
 * FOUR RUNGS, not an assignment: whatever the field holds is rounded up to
 * the next of 10, 14, 18, 20 and can never move backwards, because each
 * band's floor is the previous band's value.
 *
 * The last two rungs are one comparison in the object -- `cmp $0x11; setg;
 * lea 0x12(%eax,%eax,1)` computes 18 or 20 branchlessly -- which is the
 * compiler's, not a different rule.
 */
void
V34XF_IndicateTrn2dReceived(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	int v = obj->v90_receiver;

	if (v <= 9)
		v = 10;
	else if (v <= 14)
		v = 14;
	else if (v <= 17)
		v = 18;
	else
		v = 20;

	obj->v90_receiver = v;

	/* Re-read, not `v`: the object loads the field back for the print. */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"IndicateTrn2dReceived called, v90Receiver = %d\r\n",
			obj->v90_receiver);
}

/*
 * K56Flex has settled on a rate.
 *
 * The one indication that touches `k56flex_receiver` rather than
 * `v90_receiver`, and it sets a constant.  Its sibling
 * `V34XF_IndicateK56FlexJdReceived` is not here: it calls `v34setuptxmit`,
 * which is blocked behind `settxlevel`.
 */
void
V34XF_IndicateK56FlexRateDetermined(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"K56Flex rate determined, starting MP "
			"transmission...\r\n");

	obj->k56flex_receiver = 5;
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.  Same argument as dpsk.c's block: these offsets sit in
 * regions that are otherwise padding, so a field that drifted would compile
 * silently.  Guarded to a 32-bit ABI because `struct v34_object` holds
 * pointers and its member offsets are only the object's on that target.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34PCMIF_ASSERT(name, field, off) \
	typedef char v34pcmif_off_##name[ \
		((int)__builtin_offsetof(struct v34_object, field) == (off)) \
		? 1 : -1]

V34PCMIF_ASSERT(txscale, f25d4,           0x25d4);
V34PCMIF_ASSERT(v90rx,   v90_receiver,     0x024c);
V34PCMIF_ASSERT(k56rx,   k56flex_receiver, 0x0250);
V34PCMIF_ASSERT(f382,    f382,             0x0382);
V34PCMIF_ASSERT(f25c0,   f25c0,            0x25c0);
V34PCMIF_ASSERT(probe,   probe_results,    0xa258);
V34PCMIF_ASSERT(info0,   info0_bits,       0xa8a4);
V34PCMIF_ASSERT(rtd,     rtd,              0xaa7e);
V34PCMIF_ASSERT(timeoff, fac0c,            0xac0c);

/*
 * The doubles are the first floating-point member the struct has ever had,
 * and on i386 GCC aligns `double` to 4 inside a struct while a 64-bit target
 * aligns it to 8.  The offset assertions above would catch a shift; this
 * catches the array being the wrong length, which they would not.
 */
typedef char v34pcmif_probe_len[
	(sizeof(((struct v34_object *)0)->probe_results)
	 == V34_PROBE_RESULTS * sizeof(double)) ? 1 : -1];

#endif

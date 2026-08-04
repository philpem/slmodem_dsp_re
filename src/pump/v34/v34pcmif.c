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
#include "dsplib/v34digital.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34recv.h"

void
VPcmV34LogTimingOffset(void *objp, short offset)
{
	struct v34_object *obj = (struct v34_object *)objp;

	obj->fac0c = offset;
}

/*
 * Two reports with no reader.
 *
 * `objp` is deliberately unused: the object writes the format pointer into
 * the incoming argument slot and tail-jumps into `dsplibs_debug_printf`, so
 * the parameter is the slot rather than an input.  Both are gated at the
 * call site, unlike `VPcmV34SetTxScale` below, which lets `edprintf` do it.
 * That inconsistency is the original's.
 */
void
VPcmV34ReportStartOfEchoAdapt(void *objp)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main: Echo adapt start "
				     "reported...\r\n");
}

void
VPcmV34ReportMiddleOfEchoAdapt(void *objp)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main: Echo adapt middle "
				     "reported...\r\n");
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
 * The three entry points that ask for a change of state.  Two of them tear
 * the V.34 handshake down and start it again; the third rebuilds the
 * transmitter for a V.90 rate renegotiation without going near the handshake.
 *
 * ALL THREE FORK ON WHICH MODEM IS ACTUALLY RUNNING, and the test is the same
 * one everywhere: `(unsigned)(status - 1) <= 1`.  Status 1 and 2 mean a PCM
 * receiver has the line, and then the request is handed to the C++ side down
 * a chain of three pointers instead of being acted on here.  The same test
 * picks the `V90Demodulator` branch in `VPcmV34GetCurrentTxBitRate`, which is
 * where the meaning of the two values comes from.
 *
 * WHAT THE CHAIN IS.  `p3548` is the session object; +0x175c of it is the
 * demodulator -- `VPcmV34GetCurrentRxBitRate` passes exactly that field to
 * `V90Demodulator::getBitRate` -- and +0x20c of the demodulator is a
 * sub-object whose +0x8c takes the request code.  None of the three is
 * reconstructed, so the offsets are spelled out rather than dressed in
 * structs that would be guesses.
 */

/*
 * Hang up.
 *
 * The V.34 arm clears the rate REQUEST and both bounds and leaves `rate_now`
 * alone, which is the shape of "stop asking for anything" rather than "forget
 * what we settled on".  Then mode 2 of `v34handshakinit` -- the same mode the
 * renegotiation uses, because from the handshake's point of view a hang-up is
 * a renegotiation that never completes -- and the three receiver scalars at
 * +0x4bc, which are `struct v34_receiver`'s f258, f25a and f25c and not the
 * object's own trio at +0x25c.
 *
 * The PCM arm is the only place in this file that writes the session flag at
 * `p3548 + 0x173e`; the renegotiation below does not, and that byte is the
 * only thing that distinguishes the two functions' PCM paths.
 *
 * THE THREE CLEARS HAPPEN ON BOTH ARMS, before the fork, and the diagnostic
 * before them is not gated on which modem is running either.
 */
void
VPcmV34InitiateHangUp(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	struct v34_receiver *rx = (struct v34_receiver *)(m + 0x264);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"VPcmV34Main: VPcmV34InitiateHangUp called !\r\n");

	obj->rate_want = 0;
	obj->rate_min = 0;
	obj->rate_max = 0;

	if ((unsigned)(obj->status - 1) <= 1) {
		unsigned char *sess = (unsigned char *)obj->p3548;
		unsigned char *demod;

		sess[0x173e] = 1;
		demod = *(unsigned char **)(sess + 0x175c);
		*(int *)(*(unsigned char **)(demod + 0x20c) + 0x8c) = 2;
		return;
	}

	v34handshakinit(obj, 2);

	obj->f0004 = 6;
	*(int *)(m + 0x2218) = 5;

	rx->f258 = 0;
	rx->f25a = 0;
	rx->f25c = 0;
}

/*
 * Ask for a different rate.
 *
 * `req` is the request code, and it is NOT the same enumeration on the two
 * arms: the PCM arm forwards it verbatim to the demodulator's sub-object,
 * while the V.34 arm reads only four of its values.
 *
 *      0, 2, 5   step DOWN one index, and do not go below `rate_min`
 *      3         step UP one index, and do not go above `rate_max`
 *      anything  ask for no particular rate: `rate_want` becomes -1,
 *      else      which is the value `v34handshak` rejects with `js`
 *
 * A step that would leave the bounds writes NOTHING -- `rate_want` keeps
 * whatever it held -- rather than clamping to the bound.  So a renegotiation
 * asked for at the bottom of the range still tears the handshake down and
 * still counts, it just carries the previous request.
 *
 * 0, 2 and 5 share a body because the compiler gave them one; the object
 * tests all three separately and there is no arithmetic relating them.
 */
void
VPcmV34InitiateRateRenegotiation(void *objp, int req)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	struct v34_receiver *rx = (struct v34_receiver *)(m + 0x264);
	int want;

	if ((unsigned)(obj->status - 1) <= 1) {
		unsigned char *sess = (unsigned char *)obj->p3548;
		unsigned char *demod = *(unsigned char **)(sess + 0x175c);

		*(int *)(*(unsigned char **)(demod + 0x20c) + 0x8c) = req;
		return;
	}

	/*
	 * THE STEP WRAPS AND THE COMPARISON DOES NOT.  The object steps with
	 * `dec` and `inc`, which wrap at the ends of the signed range; C's
	 * `- 1` and `+ 1` on a signed int are UNDEFINED there, and an
	 * optimiser is entitled to fold `rate_now + 1 <= rate_max` into
	 * `rate_now < rate_max` on that basis.
	 *
	 * WRITTEN THIS WAY EVEN THOUGH THE TEST CANNOT TELL.  With the plain
	 * `- 1` this file agrees with the blob byte for byte at both extremes
	 * under the compiler and flags this tree builds with -- the mutation
	 * survives.  That agreement is a property of the code generation and
	 * not of the language, so it is not something the differential tier
	 * can protect: the one case it would break is the one case it cannot
	 * see.  The clamp stays signed, because the object's are `jl`/`jg`.
	 *
	 * A rate index is 0..14 in practice, so nothing here is reachable.
	 * It is spelled correctly because it costs one cast.
	 */
	switch (req) {
	case 0:
	case 2:
	case 5:
		want = (int)((unsigned)obj->rate_now - 1u);
		if (want >= obj->rate_min)
			obj->rate_want = want;
		break;
	case 3:
		want = (int)((unsigned)obj->rate_now + 1u);
		if (want <= obj->rate_max)
			obj->rate_want = want;
		break;
	default:
		obj->rate_want = -1;
		break;
	}

	v34handshakinit(obj, 2);

	obj->f0004 = 6;

	rx->f258 = 0;
	rx->f25a = 0;
	rx->f25c = 0;

	*(int *)(m + 0x2218) = 5;

	/* The same event `VPcmV34IndicateLocalRRN` exists to count. */
	obj->rrn_local = (short)(obj->rrn_local + 1);
}

/*
 * Rebuild the transmitter for a V.90 rate renegotiation.
 *
 * NO HANDSHAKE: this is the one of the three that does not call
 * `v34handshakinit`.  What it does instead is `v34handshakinit`'s mode 2
 * block with the state machines left out -- the same four transmit-queue
 * fields, the same mask on `f25c2`, the same `preinitdigital`, the same
 * `f382` pair.  Two independent readings of one block, which is the
 * corroboration that block was read right.
 *
 * IT ALSO WINDS `v90_receiver` BACKWARDS.  That field is documented as a
 * ratchet the phase-3 indications only advance; here it is assigned, so a
 * renegotiation can move it down.  See D48.
 *
 * `rrn_type` is tested against zero only, and `constel_size` likewise -- the
 * two `f382` values differ by 32 and are the pair `V34XF_IndicateJdReceived`
 * chooses between on its own constellation-size bit.  The parameter names are
 * the object's, from the diagnostic below.
 */
void
VPcmV34SetV90RateReneg(void *objp, short rrn_type, unsigned char constel_size)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"setV90RateReneg called, rrn type = %d, "
			"constel size = %d\r\n",
			(int)rrn_type, (int)constel_size);

	/*
	 * UNSIGNED, and that is the whole content of the test: the object
	 * compares with `cmp $1` and reads the borrow, so only zero takes the
	 * low arm.  A negative `rrn_type` takes the high one.
	 */
	obj->v90_receiver = (rrn_type != 0) ? 15 : 11;

	obj->f25c6 = 0;
	obj->f25c0 = 0;
	obj->f25cc = 0;
	obj->f25c2 = (short)((obj->f25c2 & ~0x4018) | 0x2000);

	preinitdigital(obj);

	/* The `[1]` counter every handshake trace prints; see v34hshak.c. */
	*(short *)(m + 0x2aa2) = 0;

	obj->f0004 = 6;
	*(int *)(m + 0x2218) = 5;

	obj->f382 = (short)(constel_size != 0 ? 0x89b0 : 0x8990);

	/*
	 * The timer, reset: the same three fields and the same two constants
	 * as `v34handshakinit`'s guard writes when it rejects the span, with
	 * +0x244 left alone here and written there.
	 */
	*(int *)(m + 0x238) = 0;
	*(int *)(m + 0x248) = (int)0xfff15a00;
	*(int *)(m + 0x23c) = 0x69780;
}

/*
 * ---------------------------------------------------------------------------
 * Cap the V.34 symbol rate by doctoring the line probe.
 *
 * `chkForceBaudRate` is the one function in this file that does not touch the
 * V.34 object at all -- it reads two of its fields and writes the DFT bank the
 * caller hands it.  `probeselect` calls it twice, both times with
 * `obj->probe_bins`, and then goes on to pick a symbol rate from the very
 * energies and shifts this has just adjusted.  So the mechanism is indirect:
 * there is no "forced rate" variable anywhere, only a probe measurement that
 * has been made to look as though the high bins were never received.
 *
 * WHICH BIN STANDS FOR WHICH RATE.  The six entries of `allow` are the six
 * V.34 symbol rates in the standard's order -- 2400, 2743, 2800, 3000, 3200
 * and 3429 baud -- and the object maps the top four onto bins whose centres
 * are the band edge each rate needs, one bin being 150 Hz:
 *
 *     allow[5]  3429 baud  ->  bins[24]   3750 Hz   shift := 11
 *     allow[4]  3200 baud  ->  bins[22]   3450 Hz   shift :=  7
 *     allow[3]  3000 baud  ->  bins[21]   3300 Hz   shift :=  7
 *                               bins[ 2]   450 Hz   shift :=  7
 *     allow[2]  2800 baud  ->  bins[20]   3150 Hz   shift :=  7
 *                               bins[19]  3000 Hz   shift :=  7
 *
 * `allow[0]` and `allow[1]` are written and never read: the two lowest rates
 * have no bin to spoil, because nothing about them is out at the edge.  The
 * rate-to-bin assignment is the object's; the frequencies are this
 * reconstruction's arithmetic on the 150 Hz spacing finding 212 establishes,
 * and the pairing of two bins with 3000 and 2800 is not explained by it.
 *
 * WHERE THE LIMIT COMES FROM, AND WHERE IT IS APPLIED, ARE DIFFERENT PLACES.
 * The limit is the top three bits of a configuration byte at `pac3c + 0x50`.
 * What it is applied TO depends on which PCM receiver is running:
 *
 *   - V.90 active (`v90_receiver` set): the six flags live in the SESSION
 *     object, at `p3548 + 0x217`, and this edits them in place -- so the
 *     V.90 side's own idea of which rates are on the table both feeds this
 *     decision and is narrowed by it.
 *   - otherwise: a local array, seeded 1,1,1,1,1,x, so the cap is the only
 *     thing that can clear anything and the decision is made afresh.
 *
 * The `x` is 0 when K56Flex is running and 1 when neither is, which is the
 * only thing the K56Flex arm changes.
 *
 * `allow[5] = 1` on the V.90 arm is a DEAD STORE -- `sel` points at the
 * session object there, and the local is not read again.  Reproduced because
 * it is what the object does; see D49.
 */
void
chkForceBaudRate(void *objp, struct v34_dftbin *bins)
{
	struct v34_object *obj = (struct v34_object *)objp;
	const unsigned char *cfg = (const unsigned char *)obj->pac3c;
	unsigned char allow[6] = { 0 };
	unsigned char *sel;
	int maxidx;

	maxidx = cfg[0x50] >> 5;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VpcmV34Main: max V34 baud rate index "
				     "= %d\r\n", maxidx);

	allow[0] = 1;
	allow[1] = 1;
	allow[2] = 1;
	allow[3] = 1;
	allow[4] = 1;

	if (obj->v90_receiver) {
		allow[5] = 1;			/* dead -- see the note above */
		sel = (unsigned char *)obj->p3548 + 0x217;
	} else if (obj->k56flex_receiver) {
		allow[5] = 0;
		sel = allow;
	} else {
		allow[5] = 1;
		sel = allow;
	}

	/*
	 * Index 0 CLEARS NOTHING, and it is guarded separately rather than
	 * falling out of the chain: with `maxidx` zero every comparison below
	 * would hold and every rate would be barred.  So zero means "no cap
	 * configured" and not "cap at the lowest rate".
	 */
	if (maxidx != 0) {
		if (maxidx <= 1)
			sel[1] = 0;
		if (maxidx <= 2)
			sel[2] = 0;
		if (maxidx <= 3)
			sel[3] = 0;
		if (maxidx <= 4)
			sel[4] = 0;
		if (maxidx <= 5)
			sel[5] = 0;
	}

	if (sel[5] == 0)
		bins[24].shift = 11;
	if (sel[4] == 0)
		bins[22].shift = 7;
	if (sel[3] == 0) {
		bins[2].shift = 7;
		bins[21].shift = 7;
	}
	if (sel[2] == 0) {
		bins[20].shift = 7;
		bins[19].shift = 7;
	}
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
V34PCMIF_ASSERT(f0004,   f0004,            0x0004);
V34PCMIF_ASSERT(rmin,    rate_min,         0x0220);
V34PCMIF_ASSERT(rmax,    rate_max,         0x0224);
V34PCMIF_ASSERT(rnow,    rate_now,         0x0228);
V34PCMIF_ASSERT(rwant,   rate_want,        0x022c);
V34PCMIF_ASSERT(rrnloc,  rrn_local,        0xac0e);
V34PCMIF_ASSERT(rrnrem,  rrn_remote,       0xac10);
V34PCMIF_ASSERT(p3548,   p3548,            0x3548);
V34PCMIF_ASSERT(pac3c,   pac3c,            0xac3c);
V34PCMIF_ASSERT(pbins,   probe_bins,       0xa320);

/*
 * And the three receiver scalars the two Initiate entry points clear, which
 * they reach as `obj + 0x264 + 0x258`.  Asserted as a sum so that a change to
 * either struct breaks here rather than moving the clear into the queue.
 */
#define V34PCMIF_RXASSERT(name, field, off) \
	typedef char v34pcmif_rxoff_##name[ \
		((int)(__builtin_offsetof(struct v34_object, rxq) \
		       + __builtin_offsetof(struct v34_receiver, field)) \
		 == (off)) ? 1 : -1]

V34PCMIF_RXASSERT(f258, f258, 0x4bc);
V34PCMIF_RXASSERT(f25a, f25a, 0x4be);
V34PCMIF_RXASSERT(f25c, f25c, 0x4c0);

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

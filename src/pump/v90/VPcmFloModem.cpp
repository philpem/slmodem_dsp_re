/*
 * VPcmFloModem.cpp -- six of the twenty-six members of the PCM modem class.
 *
 * `include/dsplib/VPcmFloModem.h` carries the object map, the argument for
 * why the object really is 32 KB, and the three measurements that put a
 * V90Modem inside it at +0x1758.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding F215).
 *
 * WHAT THE FIVE ARE, in the order they were written:
 *
 *   getV90JaBits       two bits out of `bitVector` into one short, wrapping
 *                      the pointer at `nofBits`; no calls at all
 *   getV90CpBits       `nofBitsPerSymbol` bits out of `cpBitVector`, then the
 *                      end-of-sequence bookkeeping and the one-shot switch
 *                      from CP to CPnot
 *   setPcmSessionType  a diagnostic, two stores and a tail call into the
 *                      embedded V90Modem
 *   setPhaseIIinfo     the whole Phase 2 record, out of the 41 INFO0 bits,
 *                      into both a V90Phase2Info and a V92Phase2Info -- and
 *                      it ends by re-applying setPcmSessionType, which the
 *                      object has inlined
 *   getUinfoValue      clear the four float arrays, turn 25 probe magnitudes
 *                      into 21 dB figures, then setPhaseIIinfo
 *   enterPhase3        twenty-one constant stores, then the demodulator's own
 *                      enterPhase3 and DILdescriptorPacker over the embedded
 *                      descriptor at +0x004
 *
 * TWO DEAD THINGS ARE DELIBERATELY NOT REPRODUCED, and are recorded here so
 * that nobody "restores" them from the disassembly later:
 *
 *   - `setPhaseIIinfo` contains `inc %eax; cmp $0x3,%eax; jle` at 0xec90,
 *     a four-iteration loop WITH NO BODY.  It computes nothing and touches
 *     no memory.
 *   - `getUinfoValue` keeps a 25-entry float array on its own stack and
 *     stores into it twice per iteration (`fsts 0x30(%esp,%edx,4)`) and
 *     never reads it back.
 *
 * Neither is observable, and a differential test cannot see either, which is
 * exactly why they are written down rather than left to be rediscovered.
 */

#include "dsplib/debug.h"
#include "dsplib/DILdescriptorPacker.h"
#include "dsplib/encode.h"
#include "dsplib/int_complex.h"
#include "dsplib/V90Equalizer.h"
#include "dsplib/v34pcmif.h"
/*
 * `V92Parameters::init()` is one of the three `externalReset` calls.  This
 * header defines nothing else and includes nothing, so it cannot collide with
 * the `V90Parameters` this file already has through `V90SessionFlag.h`.
 */
#include "dsplib/V92Parameters.h"
/*
 * `runPcmModem` reaches through both embedded modems into their sub-objects,
 * so it needs the definitions of everything the two carry pointers to.
 * `V92CPUnPck.h` and `V92ParamsInfo.h` are the two C blocks the V.92 CP
 * unpacker takes; see the comment on the two calls to it below.
 */
#include "dsplib/modem_params.h"
#include "dsplib/ResamplerTimingOffset.h"
#include "dsplib/V90ConnectionEvaluator.h"
#include "dsplib/V90CP.h"
/*
 * `v90RunDemodulator` needs three more complete types than `runPcmModem` did:
 * the V.90 CP encoder's prototype, the V.90 Jd detector it asks for the two
 * constellation codes, and the decoded MP message it copies out for the V.34
 * interface.
 *
 * THE FOURTH IT NEEDS IS `V90Phase4Demodulator` AND IT MUST NOT BE INCLUDED
 * HERE.  That class is `tools/onedef.py`'s one carried duplicate, and the
 * definition this translation unit already has is V90SessionFlag.h's partial
 * model, arriving through VPcmFloModem.h.  Including the fuller header
 * alongside it is a redefinition the compiler rejects outright, so the two
 * fields the MPnot arm reads were carved out of that model's own pad instead;
 * V90SessionFlag.h says so at the site.
 */
#include "dsplib/V90CPpck.h"
#include "dsplib/V90Jd.h"
#include "dsplib/V90MP.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V92CPUnPck.h"
#include "dsplib/V92Jd.h"
#include "dsplib/V92Modulator.h"
#include "dsplib/V92ParamsInfo.h"
#include "dsplib/V92Phase4Modulator.h"
#include "dsplib/VPcmFloModem.h"

/*
 * Hold the compiler to the map in the header.  tools/offcheck.py parses only
 * `struct name {` out of include/dsplib, so a C++ class asserts its own.
 * Guarded on a 32-bit pointer because the class carries pointers and
 * `make check64` lays them out differently -- the layout the blob has is a
 * 32-bit layout.
 *
 * OFFSETS, AND ONE BORROWED SIZE.  No size is asserted for VPcmFloModem: the
 * five members here reach +0x7f27 and that is a floor.  `sizeof(V90Modem)`
 * IS asserted, and it is not a claim about the blob -- it is the hook that
 * makes every offset past +0x6118 fail loudly if a later batch gives
 * V90Modem more modelled prefix.  V90SessionFlag.h deliberately asserts no
 * size of its own, so this file has to.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define VPCM_OFF(field, off, tag) \
	typedef char vpcm_off_##tag[ \
	    ((int)__builtin_offsetof(VPcmFloModem, field) == (off)) ? 1 : -1]

VPCM_OFF(v34Object,		0x0000, v34obj);
VPCM_OFF(dil,			0x0004, dil);
VPCM_OFF(v34BaudAllow,		0x0217, flags217);
VPCM_OFF(bitVector,		0x021e, bitvec);
VPCM_OFF(nofBits,		0x1736, nofbits);
VPCM_OFF(bitPointer,		0x1738, bitptr);
VPCM_OFF(trainConstel,		0x173a, trainconstel);
VPCM_OFF(rrnConstel,		0x173b, rrnconstel);
VPCM_OFF(byte_173c,		0x173c, byte173c);
VPCM_OFF(droppedToV34,		0x173d, flag173d);
VPCM_OFF(clr,		0x173e, flag173e);
VPCM_OFF(sweepCounter,		0x1740, sweep);
VPCM_OFF(modem,			0x1758, modem);
VPCM_OFF(pcmSessionType,	0x611c, sesstype);
VPCM_OFF(progressState,		0x6118, byte6118);
VPCM_OFF(retrainLatch,		0x6119, byte6119);
VPCM_OFF(info0Layout,		0x6120, layout);
VPCM_OFF(v92modem.parameters,	0x6128, v92params);
VPCM_OFF(qcVerifyState,		0x6f98, qcstate);
VPCM_OFF(qcSampleCount,		0x6fac, qcsamples);
VPCM_OFF(qcTerminateRequested,	0x6fb0, qcterm);
VPCM_OFF(verificationStatus,	0x6fb4, verifstatus);
VPCM_OFF(v92modem.phase2Info,	0x612c, p92);
VPCM_OFF(cpBitVector,		0x6fbc, cpbitvec);
VPCM_OFF(cpNofBits,		0x7dcc, cpnofbits);
VPCM_OFF(terminateJa,		0x7dce, termja);
VPCM_OFF(terminateCp,		0x7dcf, termcp);
VPCM_OFF(terminateCpNot,	0x7dd0, termcpnot);
VPCM_OFF(cpNotLoaded,		0x7dd1, cpnotloaded);
VPCM_OFF(nofBitsPerSymbol,	0x7dd2, bitspersym);
VPCM_OFF(nofTransmitSequences,	0x7dd4, nseq);
VPCM_OFF(minNofTransmitSequences, 0x7dd6, minseq);
VPCM_OFF(array_7dd8,		0x7dd8, a7dd8);
VPCM_OFF(array_7e2c,		0x7e2c, a7e2c);
VPCM_OFF(L2,			0x7e80, l2);
VPCM_OFF(array_7ed4,		0x7ed4, a7ed4);
VPCM_OFF(ecMode,		0x7f60, ecmode);

/* The three the map depends on being where V90SessionFlag.h puts them. */
VPCM_OFF(modem.demodulator,	0x175c, mdmdem);
VPCM_OFF(modem.phase2Info,	0x1760, mdmp2i);
VPCM_OFF(modem.ptr_49b4,	0x610c, mdm49b4);

typedef char vpcm_modem_size[(sizeof(V90Modem) == 0x49c0) ? 1 : -1];

/*
 * The embedded DIL descriptor's size, for the same reason: `dil` is placed at
 * +0x004 and `v34BaudAllow` at +0x217, and what makes those two consistent is
 * that `tagV90DILdescriptor` is exactly 0x213 bytes.  If a later batch gives
 * the descriptor another field, this fails here instead of silently shifting
 * everything from +0x217 to +0x7f27.
 */
typedef char vpcm_dil_size[(sizeof(tagV90DILdescriptor) == 0x213) ? 1 : -1];

/*
 * V92Phase2Info's assertions USED TO BE HERE, parked in this file because it
 * was "the only translation unit that uses the class and it has no .cpp of
 * its own".  It has one now -- src/pump/v90/V92Phase2Info.cpp, added with the
 * constructor -- so they live beside the class they describe, with four more
 * that the constructor earned: the three fields at +0x14..+0x16 that this
 * header used to call `pad_14[3]`, and `params` at +0x28, which makes the
 * object 0x2c and not the 0x28 the four array pointers alone suggested.
 * Finding F1222.
 */
#endif /* 32-bit host */

/*
 * log10() on the coprocessor, as the object computes it.
 *
 * `fldlg2` pushes log10(2) at the register's full 64-bit mantissa and `fyl2x`
 * computes st(1) * log2(st(0)) and pops, so the sequence takes one value and
 * leaves one -- net stack effect zero, which is what makes the "=t"/"0" tie
 * legal.  glibc's log10() is a polynomial and differs from this in the last
 * place often enough to matter once the result is scaled by ten.
 *
 * THIS IS A COPY of x87_log10 in src/pump/v90/V90Equalizer.cpp, deliberately:
 * hoisting it into a shared header from this worktree would touch a file
 * another batch owns for no behavioural gain.  Recorded so that a later
 * cleanup can collapse the two.
 */
static inline long double
x87_log10(long double x)
{
	long double r;

	__asm__ ("fldlg2\n\tfxch %%st(1)\n\tfyl2x" : "=t" (r) : "0" (x));
	return r;
}

/*
 * `cltd; xor %edx,%eax; sub %edx,%eax`, which is what GCC emits for abs() and
 * what the object has.  Written out rather than called, because C's abs() is
 * undefined at INT_MIN and this is not: it returns INT_MIN, exactly as the
 * three instructions do.
 */
static inline int
x86_abs(int v)
{
	int m = v >> 31;

	return (v ^ m) - m;
}

/*
 * ===========================================================================
 * getV90JaBits -- two bits per call, wrapping at nofBits
 * ===========================================================================
 *
 * The two fetches are written as two statements because the object writes the
 * caller's short between them and reads it back (`mov %ax,(%ebx)` then
 * `movzwl (%ebx),%edx`); the second bit is OR-ed into what is in memory, not
 * into a register that never left.  With a `short *` that no other thread
 * touches the two are the same, and the shape is kept because it is the
 * object's.
 *
 * BOTH RESETS OF THE POINTER ARE REAL AND THEY ARE NOT THE SAME ONE.  Filling
 * the vector wraps to 0 and returns 0; `terminateJa` wraps to 0 and returns 1;
 * and a call can do both, because the wrap test falls through into the
 * terminate test rather than returning.
 *
 * `done = 1` COMES BEFORE `bitPointer = 0` AND THE OBJECT EMITS THEM THE OTHER
 * WAY ROUND -- 7770's two-element decoding again.  The blob's terminate arm is
 * `xor %eax,%eax; mov $0x1,%esi; mov %ax,0x1738(%ecx)`: a SECOND zero register,
 * because `%esi` is already carrying the 1 by the time the store issues.  With
 * the two statements written the object's way round we emit
 * `xor %esi,%esi; mov %si,0x1738(%ecx); mov $0x1,%esi` -- 11 bytes wrong, and
 * the register difference is a consequence of the order rather than a second
 * defect.  Both members of the family were compiled; the other is EXACT.
 */
int
VPcmFloModem::getV90JaBits(short *bits)
{
	int done = 0;

	if (nofBits != 0) {
		*bits = bitVector[bitPointer++];
		*bits |= (short)(bitVector[bitPointer++] << 1);

		if (bitPointer == nofBits)
			bitPointer = 0;
	}

	if (terminateJa != 0) {
		done = 1;
		bitPointer = 0;
	}

	return done;
}

/*
 * ===========================================================================
 * getV90CpBits -- nofBitsPerSymbol bits per call, and the CP -> CPnot switch
 * ===========================================================================
 *
 * THE SHIFT COUNT IS MASKED TO FIVE BITS BY THE HARDWARE.  `nofBitsPerSymbol`
 * is a byte, so the loop can reach 255 while `shl %cl,%eax` only ever shifts
 * by `n & 31`; C leaves `x << n` undefined outside 0..31, so the mask is
 * written out rather than left to chance.  Same reasoning as the shift in
 * V90Equalizer.cpp.
 *
 * THE SWITCH IS A ONE-SHOT.  `cpNotLoaded` is both the guard and the flag:
 * CP gives way to CPnot only when `terminateCp` is set, `cpNotLoaded` is
 * clear and the sequence counter has reached its minimum -- and the first
 * thing the switch does is set `cpNotLoaded`.
 *
 * `terminateCpNot` outranks all of that: it returns 1 without loading
 * anything, and it is the only path that reports termination.
 */
int
VPcmFloModem::getV90CpBits(short *bits)
{
	V90ConnectionEvaluator *seen;
	unsigned short n;
	int done = 0;

	if (cpNofBits == 0)
		return done;

	*bits = 0;
	n = 0;
	while (n < nofBitsPerSymbol) {
		*bits |= (short)(cpBitVector[bitPointer++] << (n & 31));
		n = (unsigned short)(n + 1);
	}

	if (bitPointer != cpNofBits)
		return done;

	nofTransmitSequences++;

	seen = modem.demodulator->connectionEvaluator;
	seen->delayedRetrainArmed = seen->delayedRetrainRequest;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("End of CP #%d tx.... (terminateCp=%d, "
				     "terminateCpNot=%d)\n",
				     nofTransmitSequences, terminateCp,
				     terminateCpNot);

	if (terminateCpNot != 0) {
		done = 1;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("GetV90CpBits: Indicating CP " "termination !!!!\r\n");
		bitPointer = 0;
		return done;
	}

	if (terminateCp != 0 && cpNotLoaded == 0 &&
	    nofTransmitSequences >= minNofTransmitSequences) {
		short live = nofBits;
		int i;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("OK Time to move to CPNot " "TX...\r\n");

		for (i = 0; i < live; i++)
			cpBitVector[i] = bitVector[i];

		cpNofBits = live;
		cpNotLoaded = 1;
		nofTransmitSequences = 0;
	}

	bitPointer = 0;
	return done;
}

/*
 * ===========================================================================
 * setPcmSessionType -- V.90 or V.92, and tell the modem
 * ===========================================================================
 *
 * The diagnostic goes through `edprintf`, which gates itself, so there is no
 * `dsplibs_debug_level` test here; and it prints 92 or 90 as a NUMBER, from
 * `cmp $0x1; sbb; and $0xfffffffe; add $0x5c` -- an unsigned comparison, so
 * every non-zero argument including a negative one prints 92.
 *
 * THE FIELD AND THE ARGUMENT ARE NOT THE SAME VALUE.  +0x611c gets `(arg !=
 * 0)` and the V.92 record's byte gets the argument's low byte, so
 * setPcmSessionType(5) leaves 1 in one and 5 in the other.  The tail call
 * passes the argument, not the field.
 */
void
VPcmFloModem::setPcmSessionType(int sessionType)
{
	V92Phase2Info *p92;

	edprintf("VPcmFloModem: setting PCM session to V.%d\n",
		 sessionType != 0 ? 92 : 90);

	/*
	 * THE FLAG IS SET BEFORE THE POINTER IS FETCHED, and that order is
	 * decoded rather than transcribed.  Nothing in the emission says it:
	 * the object stores +0x611c and +0x11 in the order this reads and the
	 * two are independent, so the only trace the choice leaves is which
	 * register the allocator hands each -- `%eax` for the flag's `setne`
	 * and `%edx` for the fetched pointer, where writing the fetch first
	 * gets them the other way round.  That is five bytes of a 98-byte
	 * function and `alpha_equal` ACCEPTS it as a renaming, which is
	 * exactly why it needed enumerating rather than reading.
	 *
	 * The domain is every order of these three (the two that dereference
	 * `p92` before assigning it are not orders, so six become four)
	 * crossed with the four positions of the diagnostic: 12 cells, 10
	 * distinct emissions, ONE reaching the object.  A unique preimage.
	 * The arrangement written before this was 5 differing bytes and the
	 * next nearest cell is 8.  Finding F8066.
	 */
	pcmSessionType = (sessionType != 0);
	p92 = v92modem.phase2Info;
	p92->v92CapabilitiesLocal = (unsigned char)sessionType;

	modem.setSessionFlag((unsigned int)sessionType);
}

/*
 * ===========================================================================
 * setPhaseIIinfo -- the Phase 2 record, out of the INFO0 bits
 * ===========================================================================
 *
 * `info0` is the 41-int bit vector `V34XF_GetInfo0BitsPtr` hands out, one bit
 * per int.  Bits 33..37 are the transmit power code, weighted 1, 2, 4, 8, 16.
 * The weights come out of a SEVEN-entry table of which the object uses five;
 * that is the original's shape and it is kept, because a seven-bit table
 * summed over five entries says something a `<< i` would not.
 *
 * `info0Layout` gates two separate things and nothing else: whether bits 38
 * and 39 are stored at all, and whether bit 26 or bit 27 is the short-phase-2
 * remote byte.  The second is a SWAP, not a skip -- both bytes are written
 * either way.
 *
 * THE LAST THING IT DOES IS setPcmSessionType.  The object has that inlined
 * -- the same `edprintf`, the same two stores, the same tail call -- and it
 * feeds it the field it is about to overwrite, so the value that reaches the
 * modem is the OLD +0x611c and the field ends up holding `(old != 0)`.
 * Calling the method reproduces that exactly and says where the code came
 * from; writing it out again would not.
 */
void
VPcmFloModem::setPhaseIIinfo(int *info0, int rtd)
{
	const int weight[7] = { 1, 2, 4, 8, 16, 32, 64 };
	V90Phase2Info *p90;
	V92Phase2Info *p92;
	int i, power;

	power = 0;
	for (i = 0; i <= 4; i++)
		power += info0[33 + i] * weight[i];

	p90 = modem.phase2Info;
	p90->maxTxPower = (unsigned char)power;

	if (info0Layout != 0) {
		p90->txPowerMeasurementPoint = (info0[38] != 0);
		p90->pcmType = (info0[39] != 0);
	}

	p92 = v92modem.phase2Info;
	p92->maxTxPower = p90->maxTxPower;
	p92->txPowerMeasurementPoint = p90->txPowerMeasurementPoint;
	p92->pcmType = p90->pcmType;

	if (info0Layout != 0) {
		p92->shortPhase2Remote = (unsigned char)info0[26];
		p92->v92CapabilitiesRemote = (unsigned char)info0[27];
	} else {
		p92->shortPhase2Remote = (unsigned char)info0[27];
		p92->v92CapabilitiesRemote = (unsigned char)info0[26];
	}

	/*
	 * Five separate gates, each re-reading the level, exactly as the
	 * object has them.  The second prints the RAW bit, not the 0/1 the
	 * field above got.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("P2 REPORT : A_LAW or MU_LAW %d "
				     "( 0 = Mu, 1 = A)\r\n",
				     p90->pcmType == 1);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("P2 REPORT : Tx power measurment point "
				     "= %d ( 0 = digital modem terminal, "
				     "1 = codec output)\r\n", info0[38]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("P2 REPORT : Max tx power = %d\r\n",
				     p90->maxTxPower);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("P2 REPORT : short phase2: local=%d , "
				     "remote=%d\r\n", p92->shortPhase2Local,
				     p92->shortPhase2Remote);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("P2 REPORT : V92 capabilities: local=%d , "
				     "remote=%d , selected=%d\r\n",
				     p92->v92CapabilitiesLocal,
				     p92->v92CapabilitiesRemote,
				     pcmSessionType != 0 ? 92 : 90);

	/*
	 * The four arrays go into the V.90 record first and are read back out
	 * of it into the V.92 one -- which is what the object does, and the
	 * reason it matters is that it makes the V.92 record's three later
	 * pointers depend on the V.90 record's having been written.
	 */
	p90->array_10 = array_7dd8;
	p90->array_14 = array_7e2c;
	p90->L2 = L2;
	p90->array_1c = array_7ed4;

	p92->array_18 = array_7dd8;

	p90->rtd = rtd;
	p92->rtd = rtd;

	p92->array_1c = p90->array_14;
	p92->L2 = p90->L2;
	p92->array_24 = p90->array_1c;

	setPcmSessionType(pcmSessionType);
}

/*
 * ===========================================================================
 * getUinfoValue -- the probe becomes L2, and the Phase 2 record follows
 * ===========================================================================
 *
 * L2[j] = 60 + 10 * log10(probe[i] / 16384), over the 21 of the 25 probe
 * tones the mask selects, and the four arrays are cleared first whether or
 * not the computation runs.  2^-14, 10 and 60 are all exact in a float, so
 * how they are spelled cannot change a result; where the ROUNDINGS fall can,
 * and there are exactly three:
 *
 *     probe[i] * 2^-14   ->  float   (fstps/flds)
 *     log10 of that      ->  float   (fstps/flds)
 *     10 * that + 60     ->  float   (fsts into the array)
 *
 * The multiply-and-add in the third happens at the register's own precision
 * with no float in the middle, which is why it is one expression in a
 * `long double` and not two assignments to a `float`.
 *
 * THE MASK'S POSITIONS ARE LOAD-BEARING, NOT JUST ITS COUNT.  Twenty-one ones
 * for twenty-one destination slots is a consistency check and not much more;
 * what pins the four zeros to indices 5, 7, 11 and 15 is that the probe
 * magnitudes differ per index, so the test seeds them all differently.
 *
 * THE DIAGNOSTIC LOOP RUNS WHATEVER THE LEVEL IS.  15..20 is stepped every
 * time and the body is gated inside it, so at level 0 or 1 this is six turns
 * of an empty loop; the object is built that way and it costs nothing.  The
 * value printed is L2[14] - L2[i], and it never reaches memory: it stays in a
 * register at extended precision through the sign test, the truncation and
 * the fractional part, so it is a `long double` here.
 */
int
VPcmFloModem::getUinfoValue(short skipProbe)
{
	/*
	 * Which of the 25 probe tones contributes an L2 entry.  Twenty-one
	 * ones, and VPCM_L2 is 21.
	 */
	unsigned char use[VPCM_PROBE_TONES] = {
		1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1,
		1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1
	};
	double *probe;
	int *info0;
	short rtd;
	int i, j;
	int uinfo;

	probe = V34XF_GetProbeResultsPtr(v34Object);

	for (i = 0; i < VPCM_L2; i++) {
		array_7dd8[i] = 0.0f;
		array_7e2c[i] = 0.0f;
		L2[i] = 0.0f;
		array_7ed4[i] = 0.0f;
	}

	if (skipProbe == 0) {
		j = 0;
		for (i = 0; i < VPCM_PROBE_TONES; i++) {
			float scaled;
			float decade;

			scaled = (float)((long double)probe[i]
					 * 6.103515625e-05L);
			decade = (float)x87_log10((long double)scaled);

			if (use[i])
				L2[j++] = (float)((long double)decade * 10.0L
						  + 60.0L);
		}

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Final L2 Probing signal:\r\n");

		for (i = 15; i <= 20; i++) {
			if (DSPLIB_DEBUG_ON()) {
				long double d;
				long double frac;

				d = (long double)L2[14] - (long double)L2[i];
				frac = d - (long double)(int)d;

				dsplibs_debug_printf(
				    "L2[%d]    = %c%d.%05d\r\n", i,
				    d > 0.0L ? '+' : '-',
				    (int)__builtin_fabsl(d),
				    x86_abs((int)(frac * 100000.0L)));
			}
		}
	}

	rtd = V34XF_GetRTD(v34Object);
	info0 = V34XF_GetInfo0BitsPtr(v34Object);
	setPhaseIIinfo(info0, rtd);

	/*
	 * The value, if the modem has one.  `droppedToV34` short-circuits the
	 * lookup entirely; so does a zero coming back from it, and both fall
	 * into the same six default flags.
	 */
	uinfo = 0;
	if (droppedToV34 == 0) {
		uinfo = *(short *)((unsigned char *)modem.ptr_49b4 + 0x20);
		if (uinfo != 0)
			return uinfo;
	}

	v34BaudAllow[0] = 1;
	v34BaudAllow[1] = 0;
	v34BaudAllow[2] = 1;
	v34BaudAllow[3] = 1;
	v34BaudAllow[4] = 1;
	v34BaudAllow[5] = 1;

	return uinfo;
}

/*
 * ===========================================================================
 * enterPhase3 -- clear the transmit side, then build the JA vector
 * ===========================================================================
 *
 * Twenty-one stores, every one of a CONSTANT, then two calls and two
 * diagnostics.  Nothing it stores depends on anything it reads, so the only
 * things a differential test can be about are which constant lands at which
 * offset, which object the demodulator call gets, and what the packer is
 * handed.
 *
 * THE SIX FLAGS ARE NOT getUinfoValue's SIX.  Both write +0x217..+0x21c;
 * getUinfoValue writes 1, 0, 1, 1, 1, 1 and this writes 1, 0, 1, 1, 1, 0.
 * The two agreeing about the first five and differing about the last is the
 * evidence that the run really is six bytes and not five -- one function
 * alone could not distinguish "six flags" from "five flags and a neighbour".
 *
 * THE PACKER IS HANDED THE OBJECT'S OWN DESCRIPTOR.  `lea 0x4(%ebx),%eax`
 * before the call is an ADD, not a load, so +0x004 is a `tagV90DILdescriptor`
 * inside this object rather than a pointer to one elsewhere; see the header.
 * `nofBits` is passed by address and comes back holding the packed length,
 * which is what the closing diagnostic prints -- so the read at the end is of
 * the callee's output, not of the zero stored at the top.
 *
 * THE DIAGNOSTICS SIT AT TWO DIFFERENT LEVELS, deliberately: the opening one
 * is `cmpl $0x1,dsplibs_debug_level; ja`, so level 2 and above, and the
 * closing one goes through `edprintf`, which gates itself.  A test that only
 * ran at one level could not tell them apart.
 *
 * ORDER.  The object interleaves the `cmpl` and the branch with the last four
 * stores -- the compare is at 0xf327, between two of them, and the `ja` at
 * 0xf34f after all of them.  That is scheduling, not semantics: the stores
 * are to memory the gate does not read, and the branch is taken once they are
 * all done.  Written here in the order the object performs them.
 */
/*
 * `_tagModemParameters::unnamed_0003`, and the two bits BOTH entry points
 * touch.  The names are `src/pump/v34/v34pcmmain.cpp`'s, for the same byte
 * reached the other way round -- that file gets there as `obj->pac3c + 3` and
 * these two as `modem.ptr_49b4->modemParams->unnamed_0003`, and they are the
 * same storage.  Bit 2 is the one `VPcmFloModemCtor.cpp` clears at 0xfca5 and
 * `v90RateRenegSilence` clears again after printing "disabling SAS detector
 * on silence".
 *
 * `v90RunDemodulator` sets the retrain bit at three sites (arms 0x03, 0x17
 * and 0x1e), clears it at one (arm 0x08, under the phase-2 guard), and
 * `runPcmModem` does the same two things once each.  `vPcmResetPhase3Modem`
 * below is a FOURTH clearer, and the reason the block moved up the file from
 * beside `v90RunDemodulator` is that it is now the first user.
 */
#define CFG_FLAG3_PHASE2	0x02
#define CFG_FLAG3_RETRAIN	0x04

/*
 * ===========================================================================
 * `vPcmResetPhase3Modem` -- .text+0xf200, 0x95 = 149 bytes
 * ===========================================================================
 *
 * ONE OF THE FOUR MEMBERS `VPcmV34Progress` CALLS AND NOTHING ELSE DOES, and
 * the only one of the four that returns nothing -- VPcmFloModem.h's note on
 * the group has the argument.  It puts the V.90 half back to the state a
 * phase 3 can start from, and it is a straight line: no branch, no loop, one
 * `V90Parameters::init`, four `reset`s and six stores.
 *
 * IT RESETS THE MODEM WITH `qcFlag` ZERO, UNCONDITIONALLY.  `xor %eax,%eax`
 * at 0xf23a into the second argument slot -- there is no test of
 * `pcmSessionType`, of `qcFlags` or of anything else, and this is the ONLY
 * caller of `V90Modem::reset` in the whole object.  So the quick-connect DIL
 * descriptor is never selected from here; `V90Modem::reset`'s `qcFlag` arm
 * is reachable only from the blob's own callers of that member.
 *
 * BUT `setSessionFlag` IS CALLED WITH `pcmSessionType`, which is not
 * constant, and it is called BEFORE `reset`.  The two argument shapes are
 * next to each other in the disassembly (0xf22e and 0xf242) and are easy to
 * read as one; a reconstruction that passed the session type to both, or
 * zero to both, agrees with the object on every store either makes and
 * differs on the modulator/demodulator the reset reaches.
 *
 * `V90Parameters::init` REBUILDS THE WHOLE PARAMETER BLOCK -- `setToDefault`,
 * then the parameter file if there is one, then `loadModemParamsData` -- so
 * every V.90 tunable goes back to its default here and not just the phase 3
 * ones.  It is called on `modem.ptr_49b4`, the block the V90Modem owns.
 *
 * THE LAST TWO STORES ARE THE HAND-BACK TO THE V.34 SIDE.  `CFG_FLAG3_RETRAIN`
 * is cleared out of `_tagModemParameters::unnamed_0003` -- `andb $0xfb,0x3
 * (%edx)` at 0xf27e, through TWO pointers, `modem.ptr_49b4->modemParams` --
 * which withdraws any retrain this session had asked for, and `progressState` is
 * set to 1, which is the dispatch value `runPcmModem` and `v90RunDemodulator`
 * both read first and both turn into a return of 1.
 *
 * AND `+0x6fa4` IS NOT A WORD OF ITS OWN -- it is `sineWave.phase`, the third
 * of the four `Tparam`s of the `SineWave<float, float>` embedded at +0x6f9c,
 * and 0x6f9c + 8 is 0x6fa4.  The store is an integer `mov $0x0`, which is
 * what GCC emits for `= 0.0f` because the bit pattern is zero, so nothing in
 * the instruction says "float" and only the field map does.  It restarts the
 * TONEq oscillator, which `qcLineVerification` is the only caller of.
 *
 * ORDER.  0xf277's `mov $0x0,%eax` is a second zero register, not a store,
 * and 0xf271 and 0xf282 write `ecMode` and `sineWave.phase` from two
 * different registers holding the same zero.
 *
 * AND THE ORDER OF THE TWO `word_7f6x` STORES IS DECODED, NOT TRANSCRIBED --
 * 7770's argument, with the map measured in this function.  The object emits
 * `0x7f64` then `0x7f60`, and writing that order in the source does NOT
 * reproduce it: GCC transposes the pair.  Both members of the two-element
 * family were compiled and the map is a bijection, so the object's emission
 * has a unique preimage:
 *
 *     source (7f64, 7f60)  ->  emitted (7f60, 7f64)   -- 2 bytes wrong
 *     source (7f60, 7f64)  ->  emitted (7f64, 7f60)   -- EXACT
 *
 * The order below is therefore the author's, within the family of texts that
 * differ from this one only in that transposition.  Nothing here reads
 * anything else here, so it is not forced by semantics; it is forced by the
 * bytes.
 */
void
VPcmFloModem::vPcmResetPhase3Modem()
{
	sweepCounter = 0;			/* +0x1740 */

	modem.ptr_49b4->init();
	modem.setSessionFlag((unsigned int)pcmSessionType);
	modem.reset(0);
	v92modem.reset();
	echoCanceller.reset();

	ecMode = 0;
	ecRampCounter = 0;
	modem.ptr_49b4->modemParams->unnamed_0003 &=
	    (unsigned char)~CFG_FLAG3_RETRAIN;
	sineWave.phase = 0.0f;
	progressState = 1;
}

void
VPcmFloModem::enterPhase3()
{
	trainConstel = 0;
	rrnConstel = 0;
	byte_173c = 0;
	droppedToV34 = 0;
	clr = 0;

	v34BaudAllow[0] = 1;
	v34BaudAllow[1] = 0;
	v34BaudAllow[2] = 1;
	v34BaudAllow[3] = 1;
	v34BaudAllow[4] = 1;
	v34BaudAllow[5] = 0;

	terminateJa = 0;
	terminateCp = 0;
	terminateCpNot = 0;
	cpNotLoaded = 0;
	nofBitsPerSymbol = 2;

	nofBits = 0;
	cpNofBits = 0;
	bitPointer = 0;
	nofTransmitSequences = 0;
	minNofTransmitSequences = 1;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmFloModem: enterPhase3 called.\r\n");

	modem.demodulator->enterPhase3();

	DILdescriptorPacker(&dil, bitVector, &nofBits);

	edprintf("VPcmFloModem: enterPhase3: Ja length = %d\r\n", nofBits);
}

/*
 * externalReset -- `VPcmV34Create`'s way of putting a constructed modem back
 * to its starting state.
 *
 * THE SIX FLAGS AT +0x217 ARE WRITTEN TWICE, with the same six values, once
 * before the three parameter-block calls and once after.  That is what the
 * object does -- 0xd6d8..0xd701 and 0xd786..0xd7a9, two runs of six `movb`
 * with nothing between them that could touch the fields -- and it is left as
 * two runs here rather than folded into one.  The most likely reading is that
 * the source calls a small helper twice and GCC inlined both, but nothing in
 * the object names it, so the duplication is transcribed and not explained.
 *
 * THE V.90 DEMODULATOR IS REINITIALISED ONLY WHEN `info0Layout` IS NON-ZERO,
 * which is the one branch in the function.  `info0Layout` is otherwise a
 * `setPhaseIIinfo` selector, so this couples the INFO0 layout to whether
 * there is a demodulator worth re-initialising; that reading is not
 * established, and the field keeps the name the earlier batch gave it.
 *
 * THE LAST DIAGNOSTIC IS A TAIL CALL and the first is not, which is why they
 * are in this order: "reinitializing parameters" is printed at the TOP of the
 * flag work and "external reset called" at the very end.
 *
 * THE ORDER OF THE THREE SHORT STORES IS DECODED AND IS NOT THE OBJECT'S
 * EMITTED ORDER.  7770's argument, over a three-element family rather than a
 * two-element one.  The object emits `0x1738, 0x1736, 0x7dcc`, and writing
 * THAT in the source emits `0x7dcc, 0x1738, 0x1736` -- GCC rotates it.  All
 * six orders were compiled, holding the surrounding `movb`s fixed (which is
 * what licenses the family bound: those already match position for position
 * on both sides).  Differing bytes of 387:
 *
 *     bitPointer, nofBits,    cpNofBits    5      <- the object's own order
 *     bitPointer, cpNofBits,  nofBits      2
 *     nofBits,    bitPointer, cpNofBits    4
 *     nofBits,    cpNofBits,  bitPointer   0      <- written below
 *     cpNofBits,  bitPointer, nofBits      5
 *     cpNofBits,  nofBits,    bitPointer   4
 *
 * The six emissions are SIX DISTINCT texts, so the map is injective and the
 * preimage is unique -- 7771 found a pair of cells that collided and warns
 * that injectivity is checked, not assumed.
 */
void
VPcmFloModem::externalReset()
{
	v34BaudAllow[0] = 1;		/* +0x217 */
	v34BaudAllow[1] = 0;		/* +0x218 */
	v34BaudAllow[2] = 1;		/* +0x219 */
	v34BaudAllow[3] = 1;		/* +0x21a */
	v34BaudAllow[4] = 1;		/* +0x21b */
	v34BaudAllow[5] = 0;		/* +0x21c */

	modem.ptr_49b4->initSession();
	modem.ptr_49b4->init();
	v92modem.parameters->init();

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90_V34_Main: reinitializing parameters.\r\n");

	trainConstel = 0;		/* +0x173a */
	nofBits = 0;			/* +0x1736 */
	cpNofBits = 0;			/* +0x7dcc */
	bitPointer = 0;			/* +0x1738 */
	rrnConstel = 0;		/* +0x173b */
	byte_173c = 0;		/* +0x173c */
	droppedToV34 = 0;			/* +0x173d */
	clr = 0;			/* +0x173e */

	/* The second of the two runs; the same six values as above. */
	v34BaudAllow[0] = 1;		/* +0x217 again */
	v34BaudAllow[1] = 0;		/* +0x218 again */
	v34BaudAllow[2] = 1;		/* +0x219 again */
	v34BaudAllow[3] = 1;		/* +0x21a again */
	v34BaudAllow[4] = 1;		/* +0x21b again */
	v34BaudAllow[5] = 0;		/* +0x21c again */

	terminateJa = 0;		/* +0x7dce */
	terminateCp = 0;		/* +0x7dcf */
	terminateCpNot = 0;		/* +0x7dd0 */
	cpNotLoaded = 0;		/* +0x7dd1 */
	nofBitsPerSymbol = 2;		/* +0x7dd2 */
	nofTransmitSequences = 0;	/* +0x7dd4 */
	minNofTransmitSequences = 1;	/* +0x7dd6 */

	if (info0Layout != 0)
		modem.demodulator->reInit();

	progressState = 0;
	retrainLatch = 0;
	qcVerifyState = 0;
	qcTerminateRequested = 0;
	qcSampleCount = 0;
	verificationStatus = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90_V34_Main: external reset called.\r\n");
}

/*
 * ---------------------------------------------------------------------------
 * setV34BaudForV90, setV34BaudForV34 -- which V.34 symbol rates the next
 * phase 2 is allowed to settle on.
 *
 * SIX STORES EACH AND NOTHING ELSE.  No load, no call, no register left in
 * %eax; the two are byte-identical bar the last store, so the pair exists to
 * say "and the fastest rate is/is not on the table".
 *
 *     d494  movb $0x1,0x217(%eax)      d4c4  movb $0x1,0x217(%eax)
 *     d49b  movb $0x0,0x218(%eax)      d4cb  movb $0x0,0x218(%eax)
 *     d4a2  movb $0x1,0x219(%eax)      d4d2  movb $0x1,0x219(%eax)
 *     d4a9  movb $0x1,0x21a(%eax)      d4d9  movb $0x1,0x21a(%eax)
 *     d4b0  movb $0x1,0x21b(%eax)      d4e0  movb $0x1,0x21b(%eax)
 *     d4b7  movb $0x0,0x21c(%eax)      d4e7  movb $0x1,0x21c(%eax)
 *              (for V.90)                       (for V.34)
 *
 * ENTRY 1 IS ZERO ON BOTH, and on every other site in this tree that writes
 * the array -- `getUinfoValue`, `enterPhase3` and `externalReset`.  Five
 * writers and not one of them ever sets it, so whatever index 1 selects is
 * barred unconditionally by this build; that is an observation about the
 * writers and not a claim about which rate it is.
 *
 * NOBODY IN THE OBJECT CALLS EITHER.  Both are `T` and neither has an
 * incoming relocation, so they are the out-of-line copies of something whose
 * every call site was inlined, or of an interface the build does not use.
 * `setScramble` and `scaleVector` in v34shell.h are the same situation.
 */
void
VPcmFloModem::setV34BaudForV90()
{
	v34BaudAllow[0] = 1;		/* +0x217 */
	v34BaudAllow[1] = 0;		/* +0x218 */
	v34BaudAllow[2] = 1;		/* +0x219 */
	v34BaudAllow[3] = 1;		/* +0x21a */
	v34BaudAllow[4] = 1;		/* +0x21b */
	v34BaudAllow[5] = 0;		/* +0x21c */
}

void
VPcmFloModem::setV34BaudForV34()
{
	v34BaudAllow[0] = 1;		/* +0x217 */
	v34BaudAllow[1] = 0;		/* +0x218 */
	v34BaudAllow[2] = 1;		/* +0x219 */
	v34BaudAllow[3] = 1;		/* +0x21a */
	v34BaudAllow[4] = 1;		/* +0x21b */
	v34BaudAllow[5] = 1;		/* +0x21c */
}

/*
 * ===========================================================================
 * THE THREE VISUAL DIAGNOSTICS
 * ===========================================================================
 *
 * `VPcmV34GetVisualDiagnostics` (src/pump/v34/v34diag.cpp) dispatches to these
 * three when a PCM receiver is running; the V.34 and K56flex arms of that
 * function answer the same three selectors from elsewhere.  Each fills an
 * array of `int_complex` and returns how many it filled.
 *
 * THE GUARD IS THE SAME IN ALL THREE and it is the object's, not a copy of
 * one written here: `if (pcmSessionType != 0 && info0Layout == 0) return 0`.
 * Read it as "a V.92 session that is not the analog end has nothing to show":
 * the demodulator these read through exists only on the analog side, and a
 * V.90 session (`pcmSessionType == 0`) skips the second test entirely.
 *
 * WHAT IS FORCED IN THEM
 *
 * 1. THE TWO EQUALISER SCALES ARE FLOATS AND THE CONSTELLATION SCALE IS A
 *    DOUBLE.  `flds` against .rodata.cst4 in the first two, `fldl` against
 *    .rodata.cst8+0x8 in the third; a `1.7f` would have been a `flds` and a
 *    `10000.0` a `fldl`.  Under `-mfpmath=387` the whole product stays on the
 *    x87 stack at 80 bits either way, so the difference is in the CONSTANT
 *    and not in the precision of the multiply.
 *
 * 2. EVERY CONVERSION TO int ROUNDS TOWARD ZERO.  `fnstcw`, `or $0xc00`,
 *    `fldcw`, `fistpl`, `fldcw` is what a C cast compiles to, and there is no
 *    rounding term added anywhere: a coefficient of 1.99999 reports 19999 and
 *    not 20000.
 *
 * 3. THE COUNT IS CLAMPED UNSIGNED.  `cmp; ja` against `maxCount`, so a length
 *    field is compared as the `unsigned int` it is declared to be.
 *
 * 4. `sweepCounter` IS DIVIDED SIGNED, which is what retyped it; see
 *    include/dsplib/VPcmFloModem.h.
 */

/*
 * The two equaliser getters scale their coefficients by ten thousand, and the
 * object has the constant TWICE in .rodata.cst4 (+0x44 and +0x48) -- one slot
 * per function, which is what says the literal is written out in each rather
 * than shared through a variable.  Spelled once here; a macro is a
 * compile-time substitution and cannot move code generation.
 */
#define VPCM_EQU_SCALE		10000.0f

/*
 * The constellation getter's scale, and it is a `double`: `fldl` against
 * .rodata.cst8+0x8, which holds 1.7 exactly.
 */
#define VPCM_CONSTEL_SCALE	1.7

/*
 * ---------------------------------------------------------------------------
 * THE HORIZONTAL AXIS OF THE CONSTELLATION TRACE
 *
 * `getConstellation` does not return the constellation POINT the demodulator
 * decided; it returns a strip-chart.  The imaginary half of each point is the
 * sample, and the real half is a horizontal coordinate synthesised from
 * `sweepCounter`, which advances once per point and never resets.  Two
 * geometries, chosen by `inPhase3`:
 *
 *   phase 3    x = 35 * ((n / 5) % 750) - 14000
 *              one sweep of 750 positions, 35 units apart, advancing every
 *              fifth point: a span of 26,215 units starting at -14,000.
 *
 *   otherwise  x = 35 * ((n / 15) % 100 + 140 * ((word_260 + i) % 6)) - 14000
 *              SIX LANES, one per value of `(word_260 + i) % 6`, 140 * 35 =
 *              4,900 units apart, each carrying 100 positions (3,465 units)
 *              that advance every fifteenth point.
 *
 * The six lanes are why the data-phase trace is not one line: the V.90 frame
 * has six phases and the sample's phase picks its lane, so the display shows
 * each frame phase separately.  That is what `word_260` is being used FOR
 * here; it stays offset-named because being a phase counter's base is not the
 * same as being established as one, and this is its only reconstructed
 * reader.
 *
 * The constants are the object's, at 0xf471/0xf4a1/0xf4ad and
 * 0xf555/0xf560.  Named because a bare 0x8c and a bare 0x36b0 state a number
 * and hide a geometry.
 */
#define VPCM_TRACE_X_STEP	35	/* horizontal units per position    */
#define VPCM_TRACE_X_ORIGIN	14000	/* subtracted: the left-hand edge   */
#define VPCM_TRACE_P3_POSITIONS	750	/* positions in the phase-3 sweep   */
#define VPCM_TRACE_P3_DIVISOR	5	/* points per phase-3 position      */
#define VPCM_TRACE_POSITIONS	100	/* positions in one data-phase lane */
#define VPCM_TRACE_DIVISOR	15	/* points per data-phase position   */
#define VPCM_TRACE_LANE_PITCH	140	/* positions between lanes          */

/*
 * The number of lanes, and the divisor of the `% 6` at 0xf48a.  It is the
 * unsigned reciprocal `0xaaaaaaab >> 2`, so both the modulus and its
 * signedness are read off the instruction rather than assumed.
 */
#define VPCM_TRACE_LANES	6u

/* `inPhase3`'s value for phase 3, as `cmpl $0x1,0x34(%eax)` tests it. */
#define VPCM_IN_PHASE3		1u

unsigned long
VPcmFloModem::getConstellation(int_complex *points, unsigned long maxCount)
{
	V90Demodulator *dem;
	const float *values;
	unsigned int lane;
	unsigned long n, i;

	if (pcmSessionType != 0 && info0Layout == 0)
		return 0;

	dem = modem.demodulator;

	n = dem->nofSymbols;
	if (n > maxCount)
		n = maxCount;

	/*
	 * `array_254` is a `void *` in V90Demodulator.h because the code that
	 * ALLOCATES it sizes it at eight bytes an element, and this reader
	 * steps it by four.  The two are not in contradiction -- `nofSymbols` is
	 * a running fill level and not the allocated length -- but nothing
	 * settles which of the two strides is the element, so the declaration
	 * stays neutral and the cast is here with the reason on it.
	 */
	values = (const float *)dem->array_254;
	lane = dem->word_260;

	if (dem->inPhase3 == VPCM_IN_PHASE3) {
		for (i = 0; i < n; i++) {
			int t = sweepCounter++;

			points[i].re = VPCM_TRACE_X_STEP *
			    (t / VPCM_TRACE_P3_DIVISOR % VPCM_TRACE_P3_POSITIONS)
			    - VPCM_TRACE_X_ORIGIN;
			points[i].im = (int)(VPCM_CONSTEL_SCALE * values[i]);
		}
	} else {
		for (i = 0; i < n; i++) {
			int t = sweepCounter++;

			points[i].re = VPCM_TRACE_X_STEP *
			    (t / VPCM_TRACE_DIVISOR % VPCM_TRACE_POSITIONS +
			     VPCM_TRACE_LANE_PITCH * (int)((lane +
			      (unsigned int)i) % VPCM_TRACE_LANES))
			    - VPCM_TRACE_X_ORIGIN;
			points[i].im = (int)(VPCM_CONSTEL_SCALE * values[i]);
		}
	}

	return n;
}

/*
 * The linear equaliser's taps and the decision-feedback filter's, and the two
 * functions are the same fifteen lines against two different pairs of
 * `V90Equalizer` fields.  They are written out twice because the object has
 * them twice, with a scale constant each.
 *
 * BOTH PUT THE COEFFICIENT IN THE IMAGINARY HALF and zero in the real one.
 * V.90's downstream signal is real, so the taps have no imaginary part and the
 * author had a slot to spare; which slot he used is recorded rather than
 * explained (include/dsplib/int_complex.h).
 */
unsigned long
VPcmFloModem::getLinearEqualizer(int_complex *points, unsigned long maxCount)
{
	V90Equalizer *eq;
	const float *coefs;
	unsigned long n, i;

	if (pcmSessionType != 0 && info0Layout == 0)
		return 0;

	eq = modem.demodulator->equalizer;

	n = eq->linearEquLength;
	if (n > maxCount)
		n = maxCount;
	coefs = eq->linearEquCoefs;

	for (i = 0; i < n; i++) {
		points[i].re = 0;
		points[i].im = (int)(coefs[i] * VPCM_EQU_SCALE);
	}

	return n;
}

unsigned long
VPcmFloModem::getDFE(int_complex *points, unsigned long maxCount)
{
	V90Equalizer *eq;
	const float *coefs;
	unsigned long n, i;

	if (pcmSessionType != 0 && info0Layout == 0)
		return 0;

	eq = modem.demodulator->equalizer;

	n = eq->dfeLength;
	if (n > maxCount)
		n = maxCount;
	coefs = eq->dfeCoefs;

	for (i = 0; i < n; i++) {
		points[i].re = 0;
		points[i].im = (int)(coefs[i] * VPCM_EQU_SCALE);
	}

	return n;
}

/*
 * ===========================================================================
 * SEVEN SMALL MEMBERS `v90RunDemodulator` INLINES
 * ===========================================================================
 *
 * Each is its own `T` symbol in the blob and NONE of them has an incoming
 * relocation anywhere in the object, so every call site the author wrote was
 * inlined and only the out-of-line copy survives to be named.  Each body
 * below is that out-of-line copy read instruction for instruction, and each
 * one appears open-coded inside `v90RunDemodulator` between one and three
 * times.
 *
 * THE `inline` IS NOW DROPPED AND THE SEVEN SYMBOLS ARE CLAIMED -- the move
 * `include/dsplib/VPcmFloModem.h` priced ("451 bytes and seven differential
 * tests") has been made, and t_vpcmleaves.cpp is those tests.  Dropping the
 * keyword changes nothing about the fourteen inlined sites: GCC still
 * inlines a same-TU callee at -O3, and now also emits the out-of-line copy
 * the blob has.
 *
 * WHY THEY ARE FUNCTIONS AND NOT OPEN-CODED HERE TOO.  The alternative
 * spelling -- the same statements written out at each of the fourteen sites
 * -- compiles to the same instructions, so no test can separate the two.
 * What separates them is that the object HAS the seven symbols, at the sizes
 * above, with bodies that are these statements and nothing else; that is the
 * author's own factoring, recoverable from the blob, and it is the same class
 * of evidence 7570's `setDataBitRateInline` rests on.
 */

/*
 * d110, d140, d170 -- 45 bytes each, and byte-identical bar the destination
 * and the string.  Note the SHAPE: the store happens first and unconditionally,
 * the diagnostic is a tail call, and the argument is re-widened from the byte
 * that was just stored (`movzbl %dl,%eax`), not reloaded.
 */
void
VPcmFloModem::setTerminateJaFlag(unsigned char v)
{
	terminateJa = v;			/* +0x7dce */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Ja Flag set to %d\r\n", v);
}

void
VPcmFloModem::setTerminateCpFlag(unsigned char v)
{
	terminateCp = v;			/* +0x7dcf */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CP Flag set to %d\r\n", v);
}

void
VPcmFloModem::setTerminateCpNotFlag(unsigned char v)
{
	terminateCpNot = v;			/* +0x7dd0 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CPnot Flag set to %d\r\n", v);
}

/*
 * d1a0, 26 bytes.  IT ZEROES THE COUNTER TOO, which is the whole reason the
 * two halves at +0x7dd4 and +0x7dd6 are always seen written together: the
 * setter is `nofTransmitSequences = 0` followed by the assignment its name
 * describes, and no caller has to say so.
 */
void
VPcmFloModem::setMinNofTransmitSequences(unsigned short n)
{
	nofTransmitSequences = 0;		/* +0x7dd4 */
	minNofTransmitSequences = n;		/* +0x7dd6 */
}

/*
 * d1c0, 56 bytes.  THE CONDITIONAL IS THE CALLEE'S, NOT THE CALLER'S, and
 * that is what the out-of-line copy settles: `cmpl $0x1,0x8(%esp)` is a test
 * on the ARGUMENT SLOT, so the three sites in `v90RunDemodulator` that carry
 * this `sbb`/`and $0xfe`/`add $0x4` sequence are passing a raw constellation
 * code and this body is turning it into a bit count.
 *
 * The comparison is UNSIGNED (`cmpl $1` with `sbb`, giving -1 below one and 0
 * at or above), which is `unsigned int` in the mangling -- `Ej`.  Zero means
 * the four-point constellation and two bits a symbol; anything else means
 * sixteen points and four, which is what the Jd diagnostic's own
 * "(0 = 4 points / 1 = 16 points)" says.
 */
void
VPcmFloModem::setNofBitsPhase4(unsigned int constel)
{
	nofBitsPerSymbol = (constel == 0) ? 2 : 4;	/* +0x7dd2 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "SetNofBitsPhase4 - nof bits each call = %d\r\n",
		    nofBitsPerSymbol);
}

/*
 * d200, 51 bytes.  Six stores and no read, and the name is the object's own;
 * what it resets is the whole transmit bookkeeping and not just the pointer.
 * Note that it does NOT touch `minNofTransmitSequences`, which is why the two
 * arms that want it at 1 call `setMinNofTransmitSequences` afterwards.
 */
void
VPcmFloModem::resetBitPointer()
{
	bitPointer = 0;				/* +0x1738 */
	terminateJa = 0;			/* +0x7dce */
	terminateCp = 0;			/* +0x7dcf */
	terminateCpNot = 0;			/* +0x7dd0 */
	cpNotLoaded = 0;			/* +0x7dd1 */
	nofTransmitSequences = 0;		/* +0x7dd4 */
}

/*
 * d4f0, 165 bytes, between `setV34BaudForV34` (d4c0) and
 * `copyMpInfoForInterface` (d5a0) in the blob.  The transmit-side subset of
 * `enterPhase3`'s constant stores: the two bit-vector counts and the bit
 * pointer, the five flag bytes at +0x173a, the V.34 baud allow list (the
 * `setV34BaudForV90` values -- index 5, the 3429 rate, barred), the four
 * termination/CPnot bytes, `nofBitsPerSymbol` back to 2, and the sequence
 * counter pair to 0 and 1.  Every store is a constant; nothing is read.
 */
void
VPcmFloModem::internalReset()
{
	nofBits = 0;				/* +0x1736 */
	cpNofBits = 0;				/* +0x7dcc */
	bitPointer = 0;				/* +0x1738 */
	trainConstel = 0;		/* +0x173a */
	rrnConstel = 0;		/* +0x173b */
	byte_173c = 0;		/* +0x173c */
	droppedToV34 = 0;			/* +0x173d */
	clr = 0;			/* +0x173e */
	v34BaudAllow[0] = 1;		/* +0x217: the V.90 run of six  */
	v34BaudAllow[1] = 0;		/* +0x218 */
	v34BaudAllow[2] = 1;		/* +0x219 */
	v34BaudAllow[3] = 1;		/* +0x21a */
	v34BaudAllow[4] = 1;		/* +0x21b */
	v34BaudAllow[5] = 0;		/* +0x21c: 3429 baud barred     */
	terminateJa = 0;		/* +0x7dce */
	terminateCp = 0;		/* +0x7dcf */
	terminateCpNot = 0;		/* +0x7dd0 */
	cpNotLoaded = 0;		/* +0x7dd1 */
	nofBitsPerSymbol = 2;		/* +0x7dd2 */
	nofTransmitSequences = 0;	/* +0x7dd4 */
	minNofTransmitSequences = 1;	/* +0x7dd6 */
}

/*
 * d5a0, 183 bytes.  Thirteen fields of the decoded MP message out of
 * `modem.mp` and into the block at +0x1744 that `getMPrecvdBits` reads back;
 * include/dsplib/VPcmFloModem.h carries the derivation of every name and of
 * the one doubling.
 *
 * NOT ONE OF THE THIRTEEN LOADS IS FORCED, INCLUDING `mpRateMask`'s.  The six
 * bytes are read `movzbl`, the six halves `movzwl` and the rate mask
 * `movswl`, but every store is 8 or 16 bits wide, so only the low 8 or 16
 * bits of each load can reach memory: the extension is CLAUDE.md's free
 * column (finding F614) at all thirteen sites.
 *
 * THIS COMMENT SAID THE OPPOSITE AND THE MUTATION SET CORRECTED IT.  It read
 * "`mpRateMask` is the exception and it is the one field whose load IS
 * forced, `movswl`, because the doubling is done on the widened value" -- but
 * `(short)(x * 2)` keeps only the low sixteen bits of the product, and those
 * depend only on the low sixteen bits of `x`.  The row
 * `copyMpInfoForInterface reads the rate mask UNSIGNED before doubling` came
 * back NOT CAUGHT against a fixture that seeds a negative rate mask ON
 * PURPOSE, which is what a wrong claim looks like from the outside rather
 * than a weak trial.  Findings F7585 and F6103.
 */
void
VPcmFloModem::copyMpInfoForInterface()
{
	mpType = modem.mp.Type;
	mpRate = modem.mp.Rate;
	mpTrellis = modem.mp.Trellis;
	mpNonLin = modem.mp.NonLin;
	mpShaping = modem.mp.Shaping;
	mpCPack = modem.mp.CPack;
	mpRateMask = (short)(modem.mp.rateMask * 2);
	mpH1Real = modem.mp.h1Real;
	mpH1Imag = modem.mp.h1Imag;
	mpH2Real = modem.mp.h2Real;
	mpH2Imag = modem.mp.h2Imag;
	mpH3Real = modem.mp.h3Real;
	mpH3Imag = modem.mp.h3Imag;
}

/*
 * ===========================================================================
 * `VPcmFloModem::v90RunDemodulator` -- .text+0xd860, 0xbc5 = 3,013 bytes
 * ===========================================================================
 *
 * ONE BLOCK OF SAMPLES THROUGH A V.90 SESSION'S RECEIVER, and it is the
 * ANALOGUE end: the modem it drives is the embedded `V90Modem`, whose
 * `progress` fans out to `V90Demodulator::progress` on this side.
 * `VPcmV34Progress` turns the small code it returns into a new `progress`, the
 * same way it does for `runPcmModem`.
 *
 * ===========================================================================
 * WHAT THIS DOES THAT `runPcmModem` DOES NOT, AND VICE VERSA
 * ===========================================================================
 *
 * The two are the class's two entry points and they look alike for about
 * fifteen instructions.  They are not the same function and four of the
 * differences are load-bearing:
 *
 *   - THERE IS NO TRANSMIT HALF HERE.  `runPcmModem` runs the echo canceller,
 *     `V92Modem::progress`, the 0.4f output scaling, `updateEchoHistory` and
 *     a third dispatch on the modulator's `word_34`.  This function calls
 *     `V90Modem::progress` and nothing else: it takes `float *in` and no
 *     `out`, and the mangling says so (`PfjPiS1_` against `PfS0_jPiS1_S1_S1_`).
 *   - THERE IS NO `info0Layout` MASTER GATE.  `runPcmModem` returns dispatch
 *     1's seed without touching anything when +0x6120 is zero (`test`/`je` at
 *     0xe470).  Here `V90Modem::progress` is called unconditionally at 0xd8b9
 *     and `info0Layout` is read at ONE site only, inside dispatch 1's case 3.
 *   - AND AT THAT SITE THE POLARITY IS THE OTHER WAY ROUND.  See the comment
 *     on case 3 below; this is the difference the two functions were most
 *     likely to be shipped wrong on.
 *   - THE EVENT TABLE IS SHORTER AND IT IS A DIFFERENT TABLE.  0x00..0x2b, 44
 *     entries at .rodata+0x3fc, against `runPcmModem`'s 0x00..0x35, 54 entries
 *     at .rodata+0x4c0.  Twenty-three of the 44 are live here.  No case label
 *     means the same thing in both: 0x16 is "end of CPt" for the V.92 driver
 *     and "progressState = 2, return 1" here, and the arms this function's own
 *     table sends somewhere are 0x1a, 0x1b, 0x28 and 0x2a, which the V.92
 *     table sends to its default.
 *
 * THE RETURN SET IS {0, 1, 2, 3, 5, 7, 8}.  6 is `runPcmModem`'s alone -- it
 * is the V.90 fallback report out of dispatch 3, and there is no dispatch 3
 * here.  4 is in neither.
 *
 * ===========================================================================
 * THE SHAPE IS TWO DISPATCHES
 * ===========================================================================
 *
 *   1. `progressState`, 0..4, the jump table at .rodata+0x3e8 -- five entries,
 *      `[0]` and `[1]` sharing an arm, `[4]` entering `[3]`'s tail one store
 *      in.  It runs BEFORE anything else, its only job is to seed the return
 *      value, and nothing above 4 is an error: the `ja` at 0xd875 falls
 *      straight through to the join with the return still 0.
 *   2. `modem.demodulator->word_3c`, 0..0x2b, the jump table at .rodata+0x3fc
 *      -- 44 entries of which 21 are the shared default at 0xda10, which is
 *      the epilogue itself.  This is the demodulator reporting what it just
 *      saw on the line; finding F7571 is where that field's role was
 *      established, from `runPcmModem`'s 54-arm dispatch on the same word.
 *
 * THE RETURN VALUE LIVES IN A REGISTER, %esi, cleared by the `xor` at 0xd861
 * before the prologue is even finished, where `runPcmModem` keeps its in a
 * stack slot.  That is register allocation and free; what is not free is that
 * every store to it is written below where the object stores it.
 *
 * ===========================================================================
 * WHAT `V90CPPacker` IS CALLED FOR, AND FIVE TIMES WITH FOUR SHAPES
 * ===========================================================================
 *
 * This is the analogue side's CP encoder -- V.90 section 9.4.2.3 -- and
 * finding F7000 records that it is a FREE function outside the `V90CP` class,
 * which is why a caller map over that class never found it.  Every call takes
 * (`V90MappingParams *`, `tagV90AdditionalCPinfo *`, `short *`, `int`) and
 * returns the length in symbols, and the five sites differ in all four:
 *
 *   arm    params           info.word_00  destination        the clear flag
 *   0x12   mappingParams    left alone    cpBitVector        0
 *   0x19   mappingParamsAlt 0             cpBitVector        clr
 *   0x2a   mappingParamsAlt 0, and +0x10  cpBitVector        clr
 *   0x1a   mappingParamsAlt 1             bitVector          clr
 *   0x1b   mappingParamsAlt 1             bitVector          0
 *
 * The two destinations are the two vectors `getV90CpBits` and `getV90JaBits`
 * transmit from, and the two lengths that receive the result are their two
 * counters: `cpNofBits` at +0x7dcc for the first three and `nofBits` at
 * +0x1736 for the last two.  So "which vector" and "which length" travel
 * together and a swap of either is visible.
 *
 * THE V.90 MAPPING BLOCK'S UNPACKER IS NOT CALLED FROM HERE AND THAT IS
 * MEASURED, NOT OBSERVED.  `setParamsInfoFromCPUnPck` is finding F7570's
 * orphan; its V.92 twin has exactly two callers and both are arms 0x2d and
 * 0x2e of `runPcmModem`'s table, which is 0x2b + 2 and 0x2b + 3 -- PAST THE
 * END of this function's 44-entry table.  There is no arm here that could
 * hold the call without the table growing, so the absence is a property of
 * the dispatch and not of what this function happens to do.  Finding F7581.
 *
 * ===========================================================================
 * DEBUG GATES ARE PER SITE
 * ===========================================================================
 *
 * Nineteen `cmpl $0x1,dsplibs_debug_level` in 3,013 bytes, each re-reading
 * the level; src/pump/v90/V92ParamsInfo.c's head is this tree's worked
 * statement of why they are not collapsed.  Five more diagnostics go through
 * `edprintf`, which gates itself.
 */

/*
 * `flds`/`fmuls` .rodata.cst4+0x28 -- 0x41200000, and `runPcmModem` has its
 * own copy of the same value at +0x2c.  GCC 3.4.2 emits the constant pool per
 * FUNCTION, so two uses of 10.0f in one translation unit are two entries;
 * .rodata.cst4's order (0x28 before 0x2c) agrees with .text's (0xd860 before
 * 0xe430), which is why this definition is placed here in the file.
 */
#define VPCM_V90_TIMING_LOG_SCALE	10.0f

int
VPcmFloModem::v90RunDemodulator(float *in, unsigned int n, int *rxbits,
				int *nrx)
{
	unsigned char *cfgFlags;
	unsigned char silenceScr;
	int ret = 0;

	switch (progressState) {
	case 0:				/* 0xda57 */
	case 1:
		ret = 0;
		break;

	case 2:				/* 0xd880 */
		ret = 1;
		break;

	/*
	 * 0xda18, AND THE POLARITY IS THE OPPOSITE OF `runPcmModem`'s CASE 3.
	 *
	 * Both arms read the same three things in the same order.  There:
	 *
	 *     e6b9  mov 0x6120(%esi),%edx ; test %edx,%edx ; je 0xe6cf
	 *
	 * and 0xe6cf sets the return to 2.  Here:
	 *
	 *     da18  mov 0x6120(%ebx),%eax ; test %eax,%eax ; je 0xda46
	 *
	 * and 0xda46 sets `progressState` to 4 and the return to 3.  So a session
	 * with no INFO0 layout selected takes the RETRAIN exit here and the
	 * plain one there, and the two functions are one `!` apart at a site
	 * where the surrounding twelve instructions are the same.  Neither a
	 * mnemonic comparison nor a suite that scored the two functions
	 * against each other could tell them apart; only the branch target
	 * does.
	 */
	case 3:
		if (info0Layout == 0
		    || (modem.demodulator->inPhase3 == 4
			&& modem.ptr_49b4->ENABLE_ERROR_CORRECTION_RRN != 0)) {
			progressState = 4;
			ret = 3;
		} else {
			ret = 2;
		}
		break;

	case 4:				/* 0xda4d, and it is 3's tail */
		ret = 3;
		break;

	default:
		break;
	}

	/* 0xd890.  Unconditional; there is no gate in front of it. */
	modem.progress(rxbits, *(unsigned int *)nrx, in, n);

	switch (modem.demodulator->word_3c) {
	/*
	 * 0xde38.  SD detected: stop sending Ja and report nothing this
	 * block.  The two `edprintf` messages of arms 1 and 2 are the object's
	 * own names for the two outcomes.
	 */
	case 0x01:
		progressState = 1;
		ret = 0;
		edprintf("VPcmFloModem (V90): DemodSdDetected\r\n");
		setTerminateJaFlag(1);
		break;

	case 0x02:			/* 0xdd91 */
		edprintf("VPcmFloModem (V90): DemodSdNotDetected\r\n");
		break;

	/*
	 * 0xde1a.  TRN1d has started.  `retrainLatch` is the same "the retrain
	 * bit has to go back on" latch `runPcmModem`'s CPt arm uses, and arm
	 * 0x1e below is what sets it.
	 */
	case 0x03:
		if (retrainLatch != 0) {
			edprintf("VPcmFloModem (V90): ON Start TRN1d " "restoring SAS detector\n");
			modem.ptr_49b4->modemParams->unnamed_0003 |=
			    CFG_FLAG3_RETRAIN;
		}
		break;

	/*
	 * 0xdcab.  Jd detected.  The two constellation codes come out of the
	 * V.90 Jd detector (`modem.jd`, +0x0c of the V90Modem -- NOT `jd92`
	 * at +0x10, which is what `runPcmModem`'s Jd arm reads), the training
	 * one sets the phase 4 bit count, and both are announced upward.
	 *
	 * THE SILENCE-SCR BIT IS A PARAMETER THAT THE MEASURED ROUND TRIP CAN
	 * OVERRIDE, and both names in that sentence are the object's own:
	 * `V90Parameters::SILENCE_SCR` and
	 * `::MINIMUM_RTD_FOR_NON_SILENCE_SCR`, with the diagnostic between
	 * them spelling out why.
	 *
	 * TWO CONVERSIONS ARE FORCED HERE.  `SILENCE_SCR` is an `int` read
	 * with `movzbl` (0xdce0) because the local it lands in is the
	 * `unsigned char` `V34XF_IndicateJdReceived` takes, and the round trip
	 * is compared with `ja` (0xdcfe) -- UNSIGNED, where two `int`s give
	 * `jg`.  V90Phase2Info.h's +0x04 says the field really is unsigned and
	 * that the tree keeps the `int` spelling with the cast at the use
	 * site (finding F4903); this is a use site.
	 */
	case 0x06:
		modem.jd->getConstelationSize(&trainConstel, &rrnConstel);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "VPcmFloModem (V90): Train constellation : %d  "
			    "RRN constellation : %d " "(0 = 4 points / 1 = 16 points)\r\n",
			    trainConstel, rrnConstel);

		silenceScr = (unsigned char)modem.ptr_49b4->SILENCE_SCR;
		if (silenceScr != 0
		    && (unsigned int)modem.phase2Info->rtd
		       > (unsigned int)
			 modem.ptr_49b4->MINIMUM_RTD_FOR_NON_SILENCE_SCR) {
			edprintf("VPcmFloModem (V90): phase3 SCR set to NOT "
				 "silence, due to RTD %d\r\n",
				 (unsigned int)modem.phase2Info->rtd);
			silenceScr = 0;
		}

		setNofBitsPhase4(trainConstel);
		V34XF_IndicateJdReceived(v34Object, trainConstel, silenceScr);
		VPcmV34LogTimingOffset(v34Object,
		    (short)(modem.demodulator->resampler.getTimingOffsetPPM()
			    * VPCM_V90_TIMING_LOG_SCALE));
		break;

	/*
	 * 0xdc8b.  The same clear of the retrain bit `runPcmModem`'s Jd arm
	 * ends with, on its own here and under the same phase-2 guard.
	 */
	case 0x08:
		cfgFlags = &modem.ptr_49b4->modemParams->unnamed_0003;
		if ((*cfgFlags & CFG_FLAG3_PHASE2) == 0)
			*cfgFlags &= (unsigned char)~CFG_FLAG3_RETRAIN;
		break;

	/*
	 * 0xdbfc.  DIL received: tell the V.34 shell, then build CPt out of
	 * the FIRST mapping block into the CP vector and start the sequence
	 * again.  This is the only `V90CPPacker` site that leaves
	 * `additionalCPinfo.word_00` alone.
	 */
	case 0x12:
		V34XF_IndicateDilReceived(v34Object, trainConstel);
		cpNofBits = V90CPPacker(&modem.mappingParams,
					&modem.additionalCPinfo,
					cpBitVector, 0);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V90): Building "
			    "CPt, CPt length = %d\r\n", cpNofBits);
		resetBitPointer();
		VPcmV34LogTimingOffset(v34Object,
		    (short)(modem.demodulator->resampler.getTimingOffsetPPM()
			    * VPCM_V90_TIMING_LOG_SCALE));
		break;

	case 0x15:			/* 0xdbd9 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V90): " "DemodPhase3Error !!! \r\n");
		ret = 5;
		break;

	case 0x16:			/* 0xdacc */
		progressState = 2;
		ret = 1;
		break;

	/*
	 * 0xe131.  Stop sending CPnot, and put the retrain bit back if the
	 * data phase has been reached once.
	 */
	case 0x17:
		setTerminateCpNotFlag(1);
		if (retrainLatch != 0)
			modem.ptr_49b4->modemParams->unnamed_0003 |=
			    CFG_FLAG3_RETRAIN;
		break;

	/*
	 * 0xdf74.  End of TRN2d: build CP out of the ALT mapping block and
	 * restart the sequence with one transmission required.
	 */
	case 0x19:
		modem.additionalCPinfo.word_00 = 0;
		cpNofBits = V90CPPacker(&modem.mappingParamsAlt,
					&modem.additionalCPinfo,
					cpBitVector, clr);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V90): Building "
			    "CP, CP length = %d (clr=%d)\r\n",
			    cpNofBits, clr);
		resetBitPointer();
		setMinNofTransmitSequences(1);
		V34XF_IndicateTrn2dReceived(v34Object);
		VPcmV34LogTimingOffset(v34Object,
		    (short)(modem.demodulator->resampler.getTimingOffsetPPM()
			    * VPCM_V90_TIMING_LOG_SCALE));
		break;

	/*
	 * 0xe006 and 0xd8d7.  MP AND MPnot, AND THEY ARE THE PAIR THIS
	 * FUNCTION IS MOST LIKELY TO BE SHIPPED WRONG ON.
	 *
	 * Both copy the decoded message out for the V.34 interface and then
	 * build CPnot into `bitVector`.  Four things differ and every one of
	 * them is one token:
	 *
	 *   - the clear flag handed to the packer is `clr` for MP and a
	 *     literal 0 for MPnot (0xe0e3 against 0xe2a0);
	 *   - the diagnostic says "on MP receive" or "on MPnot receive";
	 *   - MPnot goes on to test `SENSITIVE_ISP_DETECTED` and terminate
	 *     CPnot where it is set; MP does not;
	 *   - MPnot has an ELSE arm and MP does not.
	 *
	 * Nothing about the two blocks' size, shape or call sequence separates
	 * them, which is exactly the failure mode CLAUDE.md records for the MP
	 * CRC pair and for `initiateRRN`/`initiateFPE`.
	 */
	case 0x1a:
		copyMpInfoForInterface();
		if (terminateCp == 0) {
			modem.additionalCPinfo.word_00 = 1;
			nofBits = V90CPPacker(&modem.mappingParamsAlt,
					      &modem.additionalCPinfo,
					      bitVector, clr);
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("VPcmFloModem (V90): "
				    "Building CPnot on MP receive, CPnot " "length = %d\r\n", nofBits);
			setTerminateCpFlag(1);
		}
		break;

	/*
	 * 0xd8d7.  MPnot.  THE ELSE ARM IS WHERE CPnot IS GIVEN UP, and its
	 * three-way test is the same conjunction `V90Demodulator::enterRRN`
	 * records as the moment a rate renegotiation is real -- the connection
	 * evaluator's counter and the phase 4 demodulator's pair at +0x3c and
	 * +0x38, in that order.  Here it is negated: CPnot is terminated
	 * unless all three say a renegotiation is under way.
	 *
	 * The two `setTerminateCpNotFlag(1); setMinNofTransmitSequences(1);`
	 * tails are one block in the object, entered from both paths
	 * (`jmp 0xd9d9` at 0xe302), which is GCC cross-jumping two copies of
	 * the same two statements.
	 */
	case 0x1b:
		copyMpInfoForInterface();
		if (terminateCp == 0) {
			modem.additionalCPinfo.word_00 = 1;
			nofBits = V90CPPacker(&modem.mappingParamsAlt,
					      &modem.additionalCPinfo,
					      bitVector, 0);
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("VPcmFloModem (V90): "
				    "Building CPnot on MPnot receive, CPnot " "length = %d\r\n", nofBits);
			setTerminateCpFlag(1);
			if (modem.ptr_49b4->SENSITIVE_ISP_DETECTED != 0) {
				edprintf("VPcmFloModem (V90): on sensitive "
					 "ISP, after one CPnot supposed to " "move to E...\r\n");
				setTerminateCpNotFlag(1);
				setMinNofTransmitSequences(1);
			}
		} else {
			if (cpNotLoaded != 0 && terminateCpNot == 0
			    && (modem.demodulator->connectionEvaluator->word_90
				    == 0
				|| modem.demodulator->phase4Demodulator
				       ->int_003c == 0
				|| modem.demodulator->phase4Demodulator
				       ->int_0038 == 0)) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "VPcmFloModem (V90): Going to " "terminate CPnot...\r\n");
				setTerminateCpNotFlag(1);
				setMinNofTransmitSequences(1);
			}
		}
		break;

	/*
	 * 0xddc0.  Ed received.  `clr` -- the same byte three of the
	 * packer calls hand across as the clear flag -- is what turns this
	 * into a cleardown report rather than a silent one.
	 */
	case 0x1c:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V90): " "Ed Received !!!\r\n");
		setTerminateCpNotFlag(1);
		setMinNofTransmitSequences(1);
		if (clr != 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("VPcmFloModem (V90): " "Indicating Cleardown !\r\n");
			ret = 8;
		}
		break;

	case 0x1d:			/* 0xdda2 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V90): " "Phase4 terminated.\r\n");
		break;

	/*
	 * 0xde7b.  The data phase.  `retrainLatch` is raised here and nowhere
	 * else in this function, and arms 0x03 and 0x17 are its two readers.
	 */
	case 0x1e:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V90): " "Enter Data Phase\r\n");
		progressState = 3;
		ret = 2;
		retrainLatch = 1;
		modem.ptr_49b4->modemParams->unnamed_0003 |= CFG_FLAG3_RETRAIN;
		VPcmV34LogTimingOffset(v34Object,
		    (short)(modem.demodulator->resampler.getTimingOffsetPPM()
			    * VPCM_V90_TIMING_LOG_SCALE));
		break;

	/*
	 * 0xdd49.  V.34 fallback, AND THE SIX STORES ARE
	 * `setV34BaudForV34`'s, byte for byte (.text+0xd4c0).  The comment on
	 * `runPcmModem`'s case 0x1f says its identical six "are not either of
	 * `setV34BaudForV90`'s or `setV34BaudForV34`'s"; that is wrong and
	 * finding F7582 retracts it.  1,0,1,1,1,1 IS the V.34 setter, and it
	 * is the V.90 one that ends in a zero.
	 */
	case 0x1f:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V90): "
					     "drop to V34 requested !! \r\n");
		droppedToV34 = 1;
		ret = 7;
		setV34BaudForV34();
		break;

	case 0x21:			/* 0xdb53 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V90): " "retrain requested !!\r\n");
		ret = 5;
		break;

	/*
	 * 0xdae7 and 0xdecd.  THE TWO RATE-RENEGOTIATION ARMS: 0x22 is this
	 * end asking and 0x23 is the far end asking.  They share eleven
	 * instructions and differ in four things:
	 *
	 *   - 0x22 raises `byte_173c`; 0x23 tests it, and where it is set
	 *     clears it and does nothing else.  So a remote report that
	 *     follows our own request is swallowed once.
	 *   - the diagnostics are "requested" and "detected";
	 *   - 0x22 calls `VPcmV34IndicateLocalRRN`, 0x23
	 *     `VPcmV34IndicateRemoteRRN`;
	 *   - 0x23 additionally calls `V90Demodulator::indicateRemoteRateReneg`
	 *     (0xdac7), which 0x22 jumps past.
	 *
	 * `byte_173c` KEEPS ITS OFFSET NAME.  What is established is the
	 * latch's behaviour, which is recorded in finding F7583; nothing
	 * names it beyond that (see VPcmFloModem.h). Its two neighbours,
	 * `trainConstel`/`rrnConstel`, were split out of the same array this
	 * batch (Wave 6) on the format string F7583 already cites -- see the
	 * header for why this third byte did not follow them.
	 *
	 * THE NARROWING IS FORCED AND THE CAST THAT SPELLS IT IS NOT.  0xda78
	 * and 0xdb01 are `movswl 0x90(...)`, sixteen bits sign-extended, where
	 * the field is a whole word; that is what `VPcmV34SetV90RateReneg`'s
	 * `short rrn_type` parameter costs, and v34pcmif.h's prototype
	 * performs exactly the same conversion whether or not the call site
	 * says so.  The `(short)` below is therefore DOCUMENTARY -- the
	 * mutation set proved it, by scoring its removal `equivalent` -- and
	 * it is kept because the narrowing is the interesting thing about
	 * this argument and a reader should not have to open another header
	 * to see it.
	 */
	case 0x22:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V90): rate "
			    "renegotiation requested !!\r\n");
		VPcmV34SetV90RateReneg(v34Object,
		    (short)modem.demodulator->connectionEvaluator->word_90,
		    rrnConstel);
		setNofBitsPhase4(rrnConstel);
		byte_173c = 1;
		VPcmV34IndicateLocalRRN(v34Object);
		progressState = 2;
		ret = 1;
		break;

	case 0x23:
		if (byte_173c != 0) {
			byte_173c = 0;
		} else {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("VPcmFloModem (V90): "
				    "rate renegotiation detected !!\r\n");
			VPcmV34SetV90RateReneg(v34Object,
			    (short)modem.demodulator->connectionEvaluator
				       ->word_90,
			    rrnConstel);
			setNofBitsPhase4(rrnConstel);
			VPcmV34IndicateRemoteRRN(v34Object);
			modem.demodulator->indicateRemoteRateReneg();
			progressState = 2;
			ret = 1;
		}
		break;

	case 0x26:			/* 0xdeac */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V90): " "energy drop detected !!\r\n");
		ret = 5;
		VPcmV34SetIndicationOfRemoteRetrain(v34Object);
		break;

	case 0x28:			/* 0xe15f */
		edprintf("VPcmFloModem (V90): restoring SAS detector " "(RtNot)\n");
		modem.ptr_49b4->modemParams->unnamed_0003 |= CFG_FLAG3_RETRAIN;
		break;

	/*
	 * 0xdee6.  Rebuild CP after a silent rate renegotiation.  It is arm
	 * 0x19's block with two differences: `additionalCPinfo.word_10` --
	 * the flag `V90Demodulator::enterRRN` raises when it decides a
	 * renegotiation is real -- is cleared as well, and neither
	 * `setMinNofTransmitSequences` nor the timing-offset line is here.
	 */
	case 0x2a:
		modem.additionalCPinfo.word_00 = 0;
		modem.additionalCPinfo.word_10 = 0;
		cpNofBits = V90CPPacker(&modem.mappingParamsAlt,
					&modem.additionalCPinfo,
					cpBitVector, clr);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V90): Rebuilding "
			    "CP after silence rrn, CP length = %d\r\n",
			    cpNofBits);
		resetBitPointer();
		V34XF_IndicateTrn2dReceived(v34Object);
		break;

	case 0x2b:			/* 0xdb6a */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V90): terminate "
					     "session requested !!\r\n");
		ret = 8;
		VPcmV34LogTimingOffset(v34Object,
		    (short)(modem.demodulator->resampler.getTimingOffsetPPM()
			    * VPCM_V90_TIMING_LOG_SCALE));
		break;

	default:			/* 0xda10, and 21 of the 44 entries */
		break;
	}

	return ret;			/* 0xda13 */
}

/*
 * ===========================================================================
 * `VPcmFloModem::runPcmModem` -- .text+0xe430, 0x7f9 = 2,041 bytes
 * ===========================================================================
 *
 * ONE BLOCK OF SAMPLES THROUGH A V.92 SESSION.  `VPcmV34Progress`'s arm 2
 * calls it and turns the small code it returns into a new `progress`; the
 * argument order is the mangling's and the names are the header's.  `in` is
 * the line signal, `out` is the block this session transmits, and the four
 * `int *` are the two bit pipes and their two counts.
 *
 * THE SHAPE IS THREE DISPATCHES, and only the middle one is large:
 *
 *   1. `progressState`, 0..4, the jump table at .rodata+0x4ac -- five entries,
 *      `[0]` and `[1]` sharing an arm.  It runs BEFORE anything else and its
 *      only job is to seed the return value; nothing above 4 is an error and
 *      the `ja` falls straight through.
 *   2. `modem.demodulator->word_3c`, 0..0x35, the jump table at .rodata+0x4c0
 *      -- 54 entries of which 33 are the shared default at 0xe5f0.  This is
 *      the demodulator telling the layer above what it just saw on the line,
 *      and each arm is what the transmitter and the V.34 shell have to do
 *      about it.
 *   3. `v92modem.modulator->word_34` over {2, 3, 10}, a compare chain rather
 *      than a table, AFTER the transmit block has been produced.
 *
 * `info0Layout` IS THE MASTER GATE.  `mov 0x6120(%esi); test; je` at 0xe470
 * returns whatever dispatch 1 seeded and does nothing else -- no echo
 * canceller, no demodulator, no transmitter.  Dispatch 1's case 3 tests it a
 * second time before it reaches for the demodulator, which is why that arm
 * re-enters the gate at 0xe476 rather than at 0xe470: GCC kept the load.
 *
 * THE RETURN VALUE lives in one stack slot, `0x20(%esp)`, cleared at entry
 * and read back at 0xe530.  Eight values reach it -- 0, 1, 2, 3, 5, 6, 7 and
 * 8 -- and 4 is not among them.  Every store is written where the object
 * stores it and nowhere else.
 *
 * THE COMPARE AT 0xe65b IS SIGNED (`cmp $0x3; je; jg`), which is why dispatch
 * 3 goes through an `int` local.  `V92Modulator::word_34` is declared
 * `unsigned int` and an unsigned switch over {2, 3, 10} compiles to `ja`, not
 * `jg`; that is CLAUDE.md's forced column, and the local states the reading
 * this site makes without moving a declaration eleven other files share.
 *
 * FIVE ARMS END IN THE SAME TIMING-OFFSET LINE and the object has it once,
 * at 0xe7eb, reached by `jmp` from all five.  That is GCC cross-jumping five
 * copies of one statement, not a helper: case 0x2b enters it at 0xe7f6,
 * halfway in, because it had already computed the address the first two
 * instructions compute.  The five copies are written out below.
 *
 * DEBUG GATES ARE PER SITE.  Five `cmpl $0x1,dsplibs_debug_level` at 0xe584,
 * 0xe679, 0xe6dd, 0xe952 and 0xe995, each re-reading the level; the head of
 * src/pump/v90/V92ParamsInfo.c is this tree's worked statement of why they
 * are not collapsed.  The sixth diagnostic, at 0xe726, goes through
 * `edprintf`, which gates itself.
 */

/*
 * `flds 0x30` in .rodata.cst4 -- 0x3ecccccd.  Every transmitted sample is
 * scaled by it on the way out, once the V.92 modem has produced the block.
 */
#define VPCM_TX_SCALE		0.4f

/*
 * `fmuls 0x2c` -- 0x41200000.  The timing offset is logged in tenths of a
 * part per million, as a `short`: `fistps` writes 16 bits and the `cwtl`
 * after it sign-extends them into the argument slot.
 */
#define VPCM_TIMING_LOG_SCALE	10.0f

/*
 * The two counters at +0x7f60 and +0x7f64 drive the echo canceller's
 * sentinel.  `V92EchoCanceller::process` passes its input through untouched
 * when `out[0]` is exactly 177.0f, and 177 is 0xb1: the ramp starts at 0xaa
 * when the silence arm sets the mode, steps by one per block, and stops at
 * the sentinel.  Any other mode writes 0.0f there, which is not the sentinel,
 * and the canceller filters.
 */
#define VPCM_EC_RAMP_MODE	0x21
#define VPCM_EC_RAMP_START	0xaa
#define VPCM_EC_RAMP_SENTINEL	0xb1

int
VPcmFloModem::runPcmModem(float *in, float *out, unsigned int n, int *rxbits,
			  int *nrx, int *txbits, int *nbits)
{
	/*
	 * +0x6c0c, the 848-byte block the constructor clears.  It is the
	 * echo-cancelled receive buffer: `V92EchoCanceller::process` writes it
	 * and `V90Modem::progress` reads it, and its first element carries the
	 * sentinel described above.
	 */
	float *rx = (float *)block_6c0c;
	unsigned char *cfgFlags;
	unsigned int i;
	int event;
	int ret = 0;

	switch (progressState) {
	case 0:				/* 0xe6a0 */
	case 1:
		ret = 0;
		break;

	case 2:				/* 0xe6ab */
		ret = 1;
		break;

	/*
	 * 0xe6b9.  The one arm that reads anything: a rate renegotiation is
	 * outstanding, and it becomes an error-correction retrain only if the
	 * receiver is in phase 4 and the parameter block allows it.
	 */
	case 3:
		if (info0Layout != 0
		    && modem.demodulator->inPhase3 == 4
		    && modem.ptr_49b4->ENABLE_ERROR_CORRECTION_RRN != 0) {
			progressState = 4;
			ret = 3;
		} else {
			ret = 2;
		}
		break;

	case 4:				/* 0xe460, and it falls through */
		ret = 3;
		break;

	default:
		break;
	}

	if (info0Layout == 0)		/* 0xe470 */
		return ret;

	if (ecMode == VPCM_EC_RAMP_MODE) {			/* 0xe53c */
		if (ecRampCounter != VPCM_EC_RAMP_SENTINEL)
			ecRampCounter++;
		rx[0] = (float)ecRampCounter;
	} else {						/* 0xe48b */
		rx[0] = 0.0f;
	}

	echoCanceller.process(in, rx, n);
	modem.progress(rxbits, *(unsigned int *)nrx, rx, n);

	/*
	 * 0xe4f5.  The receiver's recovered timing offset, NEGATED, becomes
	 * the transmitter's -- the two ends of the same loop, so what the
	 * demodulator had to add the modulator has to subtract.
	 */
	v92modem.modulator->float_28 =
	    -modem.demodulator->resampler.getTimingOffsetPPM();

	switch (modem.demodulator->word_3c) {
	/*
	 * 0xe952.  Rate renegotiation with silence: stop cancelling echo (the
	 * ramp above walks `rx[0]` to the sentinel) and stop sending Ja.
	 */
	case 1:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(" *** Echo Silence ON *** \n");
		ecMode = VPCM_EC_RAMP_MODE;
		ecRampCounter = VPCM_EC_RAMP_START;
		progressState = 1;
		ret = 0;
		v92modem.modulator->exitJa();
		break;

	/* 0xe995.  The other end of it, and it logs the timing offset. */
	case 6:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(" *** Echo Silence Off *** \n");
		ecMode = 1;
		ecRampCounter = VPCM_EC_RAMP_START;
		v92modem.modulator->exitSilence();
		VPcmV34LogTimingOffset(v34Object,
		    (short)(modem.demodulator->resampler.getTimingOffsetPPM()
			    * VPCM_TIMING_LOG_SCALE));
		break;

	/*
	 * 0xe562.  Jd detected.  The two constellation sizes go to the
	 * transmitter, the phase goes to its resampler, and the retrain bit
	 * in the shared flag byte is cleared unless phase 2 is in progress.
	 *
	 * THE FORMAT STRING NAMES THE TWO BYTES: "trainConstel" and
	 * "rrnConstel" are their real names now (Wave 6) -- see
	 * VPcmFloModem.h for why the third byte of the same run,
	 * `byte_173c`, did not follow them.
	 */
	case 7:
		modem.jd92->getConstelationSize(&trainConstel,
						&rrnConstel);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V92): Jd Detected:"
			    " trainConstel = %d, rrnConstel =%d\r\n",
			    trainConstel, rrnConstel);
		v92modem.modulator->byte_0c = trainConstel;
		v92modem.modulator->byte_0d = rrnConstel;
		v92modem.modulator->resamplerPhaseOffset =
		    modem.jd92->getJdPhase();
		v92modem.modulator->exitSuSecond();

		cfgFlags = &modem.ptr_49b4->modemParams->unnamed_0003;
		if ((*cfgFlags & CFG_FLAG3_PHASE2) == 0)
			*cfgFlags &= (unsigned char)~CFG_FLAG3_RETRAIN;
		break;

	/* 0xe7bb.  End of TRN1u: pack the CP message from the first set. */
	case 0x14:
		setV92CPpckFromParamsInfo(&modem.mappingParams,
					  &modem.additionalCPinfo,
					  v92modem.cp);
		v92modem.modulator->exitTRN1uSecond();
		VPcmV34LogTimingOffset(v34Object,
		    (short)(modem.demodulator->resampler.getTimingOffsetPPM()
			    * VPCM_TIMING_LOG_SCALE));
		break;

	case 0x15:			/* 0xe786 */
	case 0x21:
		ret = 5;
		break;

	/*
	 * 0xe83c.  End of CPt.  `retrainLatch` is the flag that says the retrain
	 * bit has to go back on in the shared byte.
	 */
	case 0x16:
		progressState = 2;
		ret = 1;
		v92modem.modulator->exitCPt();
		if (retrainLatch != 0)
			modem.ptr_49b4->modemParams->unnamed_0003 |=
			    CFG_FLAG3_RETRAIN;
		break;

	/* 0xe878.  End of TRN2u, and the CP message comes from the ALT set. */
	case 0x19:
		setV92CPpckFromParamsInfo(&modem.mappingParamsAlt,
					  &modem.additionalCPinfo,
					  v92modem.cp);
		v92modem.modulator->phase4Modulator->exitTRN2u();
		VPcmV34LogTimingOffset(v34Object,
		    (short)(modem.demodulator->resampler.getTimingOffsetPPM()
			    * VPCM_TIMING_LOG_SCALE));
		break;

	case 0x1c:			/* 0xe8b0 */
		v92modem.modulator->phase4Modulator->recivedEd();
		break;

	/* 0xe8c6.  Retrain: set the bit unconditionally this time. */
	case 0x1e:
		progressState = 3;
		ret = 2;
		modem.ptr_49b4->modemParams->unnamed_0003 |=
		    CFG_FLAG3_RETRAIN;
		VPcmV34LogTimingOffset(v34Object,
		    (short)(modem.demodulator->resampler.getTimingOffsetPPM()
			    * VPCM_TIMING_LOG_SCALE));
		break;

	/*
	 * 0xe913.  V.34 fallback is being asked for, so the handshake's
	 * per-baud permissions are rewritten -- and this pattern is not
	 * either of `setV34BaudForV90`'s or `setV34BaudForV34`'s: index 1 is
	 * barred and index 5 is allowed, which is the opposite of both.
	 */
	case 0x1f:
		droppedToV34 = 1;
		ret = 7;
		v34BaudAllow[0] = 1;
		v34BaudAllow[1] = 0;
		v34BaudAllow[2] = 1;
		v34BaudAllow[3] = 1;
		v34BaudAllow[4] = 1;
		v34BaudAllow[5] = 1;
		break;

	case 0x20:			/* 0xea4b */
		ret = 6;
		break;

	/*
	 * 0xea59 and 0xea88.  Local and remote rate renegotiation, the same
	 * arm twice but for which of the two V.34 shell entry points it
	 * calls.  The transmitter is only told to start RRN if it is in phase
	 * 3, and the threshold the phase 4 modulator will use comes from the
	 * connection evaluator.
	 */
	case 0x22:
		progressState = 2;
		ret = 1;
		if (v92modem.modulator->phase == V92MOD_PHASE_DATA) {
			v92modem.modulator->initiateRRN();
			v92modem.modulator->phase4Modulator->word_2c =
			    modem.demodulator->connectionEvaluator->word_90;
		}
		VPcmV34IndicateLocalRRN(v34Object);
		break;

	case 0x23:
		progressState = 2;
		ret = 1;
		if (v92modem.modulator->phase == V92MOD_PHASE_DATA) {
			v92modem.modulator->initiateRRN();
			v92modem.modulator->phase4Modulator->word_2c =
			    modem.demodulator->connectionEvaluator->word_90;
		}
		VPcmV34IndicateRemoteRRN(v34Object);
		break;

	/* 0xe759.  Fast phase exchange, and the same phase-3 guard. */
	case 0x24:
	case 0x25:
		progressState = 2;
		ret = 1;
		if (v92modem.modulator->phase == V92MOD_PHASE_DATA)
			v92modem.modulator->initiateFPE();
		break;

	case 0x26:			/* 0xea1d */
		ret = 5;
		VPcmV34SetIndicationOfRemoteRetrain(v34Object);
		break;

	case 0x27:			/* 0xe7a5 */
		v92modem.modulator->phase4Modulator->recivedRt();
		break;

	/* 0xeab7.  Nothing but the code and the timing-offset line. */
	case 0x2b:
		ret = 8;
		VPcmV34LogTimingOffset(v34Object,
		    (short)(modem.demodulator->resampler.getTimingOffsetPPM()
			    * VPCM_TIMING_LOG_SCALE));
		break;

	/*
	 * 0xeacb and 0xeb14.  THE TWO CALLS INTO THE V.92 CP UNPACKER.  Both
	 * refill `V92Modem::mappingParams` -- the 180-byte block at +0x6bc4,
	 * which `V92ParamsInfo.h` shows is the same block the mangling calls
	 * `V92MappingParams` -- from the CP message the demodulator has just
	 * finished collecting.  That message is `modem.cp`, the `V90CP`
	 * embedded at +0x254c, read through the field map
	 * `include/dsplib/V92CPUnPck.h` carries; both are opaque to the C++
	 * side, so both arguments are cast, which is the arrangement
	 * `V92ParamsInfo.h` already licenses for this pair of types.
	 *
	 * The two differ in ONE thing: the tagged form does not test
	 * `extendEu`.  `cmpb $0x0,0x255e(%esi)` at 0xeae3 is +0x12 of the
	 * block, which the unpacker has just written, and a non-zero one sets
	 * the phase 4 modulator's `e2uExtended`.
	 */
	case 0x2d:
		V92setParamsInfoFromCPUnPck(
		    (struct V92ParamsInfo *)v92modem.mappingParams,
		    (struct V92CPUnPck *)&modem.cp);
		if (((struct V92CPUnPck *)&modem.cp)->extendEu != 0)
			v92modem.modulator->phase4Modulator->e2uExtended = 1;
		v92modem.modulator->phase4Modulator->recivedCP();
		break;

	case 0x2e:
		V92setParamsInfoFromCPUnPck(
		    (struct V92ParamsInfo *)v92modem.mappingParams,
		    (struct V92CPUnPck *)&modem.cp);
		v92modem.modulator->phase4Modulator->recivedCPtag();
		break;

	case 0x2f:			/* 0xeb42 */
		v92modem.modulator->phase4Modulator->recivedSUV();
		break;

	case 0x30:			/* 0xeb58 */
		v92modem.modulator->phase4Modulator->recivedSUVtag();
		break;

	/*
	 * 0xe9cb and 0xe9f4.  The first half of a silent RRN SUV.  The
	 * modulator is told to expect one section and how long the received
	 * CP's counted block was; `word_ca0` is reached through
	 * `demodulator->cp`, which is the same storage as `modem.cp` above.
	 */
	case 0x31:
		v92modem.modulator->phase4Modulator->word_30 = 1;
		v92modem.modulator->phase4Modulator->word_38 =
		    modem.demodulator->cp->word_ca0;
		v92modem.modulator->phase4Modulator
		    ->recivedPartOneSilenceRrnSUV();
		break;

	case 0x33:
		v92modem.modulator->phase4Modulator->word_30 = 1;
		v92modem.modulator->phase4Modulator->word_38 =
		    modem.demodulator->cp->word_ca0;
		v92modem.modulator->phase4Modulator
		    ->recivedPartOneSilenceRrnSUVtag();
		break;

	case 0x32:			/* 0xea35 */
		v92modem.modulator->phase4Modulator
		    ->recivedPartTwoSilenceRrnSUV();
		break;

	case 0x34:			/* 0xe8e7 */
		v92modem.modulator->phase4Modulator
		    ->recivedPartTwoSilenceRrnSUVtag();
		break;

	case 0x35:			/* 0xe8fd */
		v92modem.modulator->phase4Modulator->recivedFirstRrnEd();
		break;

	default:			/* 0xe5f0, and 33 of the 54 entries */
		break;
	}

	/* 0xe5f0.  The transmit half, and every arm above arrives here. */
	v92modem.progress(txbits, *(unsigned int *)nbits, out, n);

	for (i = 0; i < n; i++)
		out[i] *= VPCM_TX_SCALE;

	echoCanceller.updateEchoHistory(out, n);

	event = (int)v92modem.modulator->word_34;
	switch (event) {
	/*
	 * 0xe672.  The transmitter has started sending, so the canceller can
	 * begin measuring the delay.
	 */
	case 2:
		progressState = 1;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V92): Starting "
					     "echo canceller training...\r\n");
		echoCanceller.setState(V92_ECHO_COUNT_DELAY);
		break;

	/*
	 * 0xe6dd.  Freeze it instead, hand the receiver back to phase 3, and
	 * -- if the receiver answers that it is falling back -- say so and
	 * return 6.
	 */
	case 3:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V92): Freezing "
					     "echo canceller...\r\n");
		echoCanceller.setState(V92_ECHO_FILTER_ONLY);
		modem.demodulator->enterPhase3();
		if (modem.demodulator->word_3c == 0x20) {
			edprintf("VPcmFloModem (V92): got V90 Fallback " "request...\r\n");
			ret = 6;
		}
		break;

	case 10:			/* 0xe518 */
		progressState = 3;
		break;

	default:
		break;
	}

	return ret;			/* 0xe530 */
}

/*
 * ===========================================================================
 * `VPcmFloModem::qcLineVerification` -- .text+0xf750, 0x30b = 779 bytes
 * ===========================================================================
 *
 * THE THIRD OF THE CLASS'S THREE `VPcmV34Progress` ENTRY POINTS, and the one
 * that runs during quick connect's LINE VERIFICATION period: the analogue
 * modem listens to the ANSpcm the far end is dropping, then transmits a tone
 * -- the object calls it TONEq -- for at least 50 ms, then 40 ms of silence,
 * and reports 1 exactly once when that is over.  `v34pcmmain.cpp` turns the 1
 * into "Line verification period completed !!!" and decides short or full
 * phase 2 on `verificationStatus`.
 *
 * ITS CLOSURE IS ITSELF ALONE.  Every one of its four callees --
 * `V90Modem::progress`, `SineWave<float,float>::generate`,
 * `V90Phase3Demodulator::enterWaitForANSpcmDrop` and `dsplibs_debug_printf`
 * -- was already written, which is why it could be landed beside a batch it
 * shares no code with.
 *
 * ===========================================================================
 * IT HAS `runPcmModem`'s SIGNATURE AND USES SIX OF THE SEVEN
 * ===========================================================================
 *
 * `PfS0_jPiS1_S1_S1_`, the same mangling, so the header declares the same
 * seven parameters.  `txbits` IS NEVER READ: there is no reference to
 * `0x48(%esp)` anywhere in the 779 bytes.  That is not a hazard and not a
 * defect -- it is what an entry point with a fixed argument list looks like
 * when one of the entries has nothing to say -- but it does mean no mutation
 * can ever be caught on it, and finding F7604 records that rather than leaving
 * a fixture looking incomplete.
 *
 * `nrx` IS PASSED TO `V90Modem::progress` AS ITS `unsigned int &` and then
 * OVERWRITTEN WITH ZERO before the return, exactly as `*nbits` is.  So the
 * bit count the demodulator hands back is discarded on every path, and a test
 * that watched `*nrx` for evidence that `progress` ran would be watching the
 * one word this function guarantees is zero.  Use the demodulator's own state
 * instead.
 *
 * ===========================================================================
 * THE TWO EVENT CODES, AND THE COMMON TAIL THEY BOTH FALL INTO
 * ===========================================================================
 *
 * The dispatch is `modem.demodulator->word_3c` again -- finding F7571's field,
 * `v90RunDemodulator`'s 44-arm table and `runPcmModem`'s 54-arm one -- but
 * here it is TWO `cmp`s and no table, and both codes are past the end of
 * either of those tables: 0x3a and 0x3b against 0x2b and 0x35.  They are this
 * period's alone.
 *
 *   0x3a  the ANSpcm demodulation finished normally.  Copy the verification
 *         status out of the phase 3 demodulator, start TONEq with NO
 *         termination pending, and put the phase 3 demodulator into
 *         `enterWaitForANSpcmDrop`.
 *   0x3b  the ANSpcm drop was detected.  If TONEq is already running this is
 *         the request to stop it -- honoured at once if 50 ms have passed and
 *         latched otherwise.  If it is NOT running, the object says so
 *         ("...with no verification completion status !!!") and starts TONEq
 *         anyway, with the termination already latched.
 *
 * WHY THE 0x3b/RUNNING/EXPIRED ARM REPEATS THE TAIL'S OWN ENDING.  It prints
 * the same message and makes the same two stores as the `qcTerminateRequested`
 * test below, and then falls into the SILENCE half rather than the tone half
 * -- 0xf95f jumps to 0xf7d3, past the `qcVerifyState == 1` test, because the
 * store it just made settles that test.  So the two sites are two source
 * statements and not one: after this one the buffer is zeroed, after the
 * other one it has just been filled.
 *
 * THE TWO CONSTANTS ARE THE OBJECT'S MESSAGE.  "still bellow 50mS" (the
 * author's spelling) beside `cmp $0x1df` gives 480 samples = 50 ms, and the
 * sample rate that makes that true, 9600 Hz, is the one the constructor
 * builds this class's `SineWave` with.  The silence is `mov $0xfffffe80` --
 * -384, so 40 ms at the same rate, counted UP through zero, which is why
 * `qcSampleCount` has to be signed.
 *
 * EIGHT DEBUG GATES, ALL `> 1`, each re-reading the level.  A sweep over
 * {0, 2} cannot separate `> 1` from `> 0`; `t_v90modchain`'s three-level
 * sweep is the shape this needs.
 *
 * THE DISPATCH VALUE IS READ INTO A NAMED LOCAL BEFORE `qcSampleCount` IS
 * ADVANCED, and the object is what says so.  Written the obvious way --
 * `qcSampleCount += n;` and then `switch (modem.demodulator->word_3c)` -- GCC
 * splits the load chain around the store:
 *
 *     blob   mov 0x175c(%esi),%edx    ours   mov 0x6fac(%esi),%ebx
 *            mov 0x6fac(%esi),%ebx           mov 0x175c(%esi),%edx
 *            mov 0x3c(%edx),%eax             add %edi,%ebx
 *            add %edi,%ebx                   mov %ebx,0x6fac(%esi)
 *            mov %ebx,0x6fac(%esi)           mov 0x3c(%edx),%eax
 *
 * -- 17 bytes of 779, and the blob completes the whole `word_3c` load before
 * the store.  With the value in a local declared ahead of the `+=` it is
 * EXACT.  The local also has to sit where it is for a reason that is not
 * cosmetic: `V90Modem::progress` is what RUNS the demodulator, so it is what
 * sets `word_3c`, and the read cannot be hoisted above the call.
 *
 * (7770 records a named intermediate as its better-motivated hypothesis that
 * FAILED, reaching 2 bytes and not 0.  It is the same construct and this time
 * it lands; the difference is that there the object's shape was a scheduling
 * sink and here it is a load the object completes early.)
 * ===========================================================================
 */

/* 0xf89d and 0xf931: `cmp $0x1df`, and the message beside it says 50 ms. */
#define QC_TONEQ_MIN_SAMPLES	480

/* 0xf8b5 and 0xf944: `mov $0xfffffe80`, 40 ms at the same 9600 Hz. */
#define QC_SILENCE_SAMPLES	384

int
VPcmFloModem::qcLineVerification(float *in, float *out, unsigned int n,
				 int *rxbits, int *nrx, int *txbits,
				 int *nbits)
{
	unsigned int i;
	int ret = 0;

	(void)txbits;			/* never read; see the file comment */

	modem.progress(rxbits, *(unsigned int *)nrx, in, n);

	unsigned int state = modem.demodulator->word_3c;

	qcSampleCount += (int)n;

	switch (state) {
	case 0x3a:			/* 0xf8d0 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "VPcmFloModem (QC LineVerify): ANSpcm demod over "
			    "(after %d samples), start TONEq...\r\n",
			    qcSampleCount);

		verificationStatus = (unsigned short)
		    modem.demodulator->phase3Demodulator->verificationStatus;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "VPcmFloModem (QC LineVerify): Channel "
			    "verification status is: %d\r\n",
			    verificationStatus);

		qcVerifyState = 1;
		qcTerminateRequested = 0;
		qcSampleCount = 0;
		modem.demodulator->phase3Demodulator
		    ->enterWaitForANSpcmDrop();
		break;

	case 0x3b:			/* 0xf819 */
		if (qcVerifyState == 1) {
			if (qcSampleCount >= QC_TONEQ_MIN_SAMPLES) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "VPcmFloModem (QC LineVerify): " "TONEq mod over (after %d "
					    "samples), tx silence...\r\n",
					    qcSampleCount);

				qcSampleCount = -QC_SILENCE_SAMPLES;
				qcVerifyState = 2;
			} else {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "VPcmFloModem (QC LineVerify): " "TONEq termination requested, "
					    "still bellow 50mS (nof samples " "= %d)...\r\n", qcSampleCount);

				qcTerminateRequested = 1;
			}
		} else {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "VPcmFloModem (QC LineVerify): got ANSpcm "
				    "drop detection, with no verification " "completion status !!!\r\n");

			verificationStatus = (unsigned short)modem.demodulator
			    ->phase3Demodulator->verificationStatus;

			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "VPcmFloModem (QC LineVerify): " "demodulator verification status is "
				    "%d\r\n", verificationStatus);

			qcSampleCount = 0;
			qcVerifyState = 1;
			qcTerminateRequested = 1;
		}
		break;

	default:
		break;
	}

	if (qcVerifyState == 1) {	/* 0xf873 */
		sineWave.generate(out, n);

		if (qcTerminateRequested
		    && qcSampleCount >= QC_TONEQ_MIN_SAMPLES) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "VPcmFloModem (QC LineVerify): TONEq mod "
				    "over (after %d samples), tx " "silence...\r\n", qcSampleCount);

			qcSampleCount = -QC_SILENCE_SAMPLES;
			qcVerifyState = 2;
		}
	} else {			/* 0xf7d3 */
		for (i = 0; i < n; i++)
			out[i] = 0.0f;

		if (qcVerifyState == 2 && qcSampleCount >= 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "VPcmFloModem (QC LineVerify): Silence "
				    "after TONEq over, move to phase2...\r\n");

			ret = 1;
		}
	}

	*nrx = 0;			/* 0xf7f9 */
	*nbits = 0;			/* 0xf7ff */

	return ret;
}

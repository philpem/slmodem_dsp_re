/*
 * VPcmFloModem.cpp -- six of the twenty-six members of the PCM modem class.
 *
 * `include/dsplib/VPcmFloModem.h` carries the object map, the argument for
 * why the object really is 32 KB, and the three measurements that put a
 * V90Modem inside it at +0x1758.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215).
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
VPCM_OFF(flags_173a,		0x173a, flags173a);
VPCM_OFF(flag_173d,		0x173d, flag173d);
VPCM_OFF(flag_173e,		0x173e, flag173e);
VPCM_OFF(modem,			0x1758, modem);
VPCM_OFF(pcmSessionType,	0x611c, sesstype);
VPCM_OFF(byte_6118,		0x6118, byte6118);
VPCM_OFF(byte_6119,		0x6119, byte6119);
VPCM_OFF(info0Layout,		0x6120, layout);
VPCM_OFF(v92modem.parameters,	0x6128, v92params);
VPCM_OFF(word_6f98,		0x6f98, word6f98);
VPCM_OFF(word_6fac,		0x6fac, word6fac);
VPCM_OFF(word_6fb0,		0x6fb0, word6fb0);
VPCM_OFF(word_6fb4,		0x6fb4, word6fb4);
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
 * Finding 1222.
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
		bitPointer = 0;
		done = 1;
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
	seen->word_7c = seen->word_78;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("End of CP #%d tx.... (terminateCp=%d, "
				     "terminateCpNot=%d)\n",
				     nofTransmitSequences, terminateCp,
				     terminateCpNot);

	if (terminateCpNot != 0) {
		done = 1;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("GetV90CpBits: Indicating CP "
					     "termination !!!!\r\n");
		bitPointer = 0;
		return done;
	}

	if (terminateCp != 0 && cpNotLoaded == 0 &&
	    nofTransmitSequences >= minNofTransmitSequences) {
		short live = nofBits;
		int i;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("OK Time to move to CPNot "
					     "TX...\r\n");

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

	p92 = v92modem.phase2Info;
	pcmSessionType = (sessionType != 0);
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
	 * The value, if the modem has one.  `flag_173d` short-circuits the
	 * lookup entirely; so does a zero coming back from it, and both fall
	 * into the same six default flags.
	 */
	uinfo = 0;
	if (flag_173d == 0) {
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
void
VPcmFloModem::enterPhase3()
{
	flags_173a[0] = 0;
	flags_173a[1] = 0;
	flags_173a[2] = 0;
	flag_173d = 0;
	flag_173e = 0;

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

	flags_173a[0] = 0;		/* +0x173a */
	bitPointer = 0;			/* +0x1738 */
	nofBits = 0;			/* +0x1736 */
	cpNofBits = 0;			/* +0x7dcc */
	flags_173a[1] = 0;		/* +0x173b */
	flags_173a[2] = 0;		/* +0x173c */
	flag_173d = 0;			/* +0x173d */
	flag_173e = 0;			/* +0x173e */

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

	byte_6118 = 0;
	byte_6119 = 0;
	word_6f98 = 0;
	word_6fb0 = 0;
	word_6fac = 0;
	word_6fb4 = 0;

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

	n = dem->word_258;
	if (n > maxCount)
		n = maxCount;

	/*
	 * `array_254` is a `void *` in V90Demodulator.h because the code that
	 * ALLOCATES it sizes it at eight bytes an element, and this reader
	 * steps it by four.  The two are not in contradiction -- `word_258` is
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
 * `VPcmFloModem::runPcmModem` -- .text+0xe430, 0x7f9 = 2,041 bytes
 * ===========================================================================
 *
 * ONE BLOCK OF SAMPLES THROUGH A V.92 SESSION.  `VPcmV34Progress`'s arm 2
 * calls it and turns the small code it returns into a new `f0004`; the
 * argument order is the mangling's and the names are the header's.  `in` is
 * the line signal, `out` is the block this session transmits, and the four
 * `int *` are the two bit pipes and their two counts.
 *
 * THE SHAPE IS THREE DISPATCHES, and only the middle one is large:
 *
 *   1. `byte_6118`, 0..4, the jump table at .rodata+0x4ac -- five entries,
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
 * `_tagModemParameters::unnamed_0003`, and the two bits this function
 * touches.  The names are `src/pump/v34/v34pcmmain.cpp`'s, for the same byte
 * reached the other way round -- that file gets there as `obj->pac3c + 3` and
 * this one as `modem.ptr_49b4->modemParams->unnamed_0003`, and they are the
 * same storage.  Bit 2 is the one `VPcmFloModemCtor.cpp` clears at 0xfca5 and
 * `v90RateRenegSilence` clears again after printing "disabling SAS detector
 * on silence".
 */
#define CFG_FLAG3_PHASE2	0x02
#define CFG_FLAG3_RETRAIN	0x04

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

	switch (byte_6118) {
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
			byte_6118 = 4;
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

	if (word_7f60 == VPCM_EC_RAMP_MODE) {			/* 0xe53c */
		if (word_7f64 != VPCM_EC_RAMP_SENTINEL)
			word_7f64++;
		rx[0] = (float)word_7f64;
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
		word_7f60 = VPCM_EC_RAMP_MODE;
		word_7f64 = VPCM_EC_RAMP_START;
		byte_6118 = 1;
		ret = 0;
		v92modem.modulator->exitJa();
		break;

	/* 0xe995.  The other end of it, and it logs the timing offset. */
	case 6:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(" *** Echo Silence Off *** \n");
		word_7f60 = 1;
		word_7f64 = VPCM_EC_RAMP_START;
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
	 * THE FORMAT STRING NAMES THE TWO BYTES: "trainConstel" is
	 * `flags_173a[0]` and "rrnConstel" is `flags_173a[1]`.  The array
	 * keeps its offset name because `enterPhase3`, `externalReset` and
	 * `VPcmXfCreate` clear all three of it as a run and splitting it is
	 * not this batch's change.
	 */
	case 7:
		modem.jd92->getConstelationSize(&flags_173a[0],
						&flags_173a[1]);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmFloModem (V92): Jd Detected:"
			    " trainConstel = %d, rrnConstel =%d\r\n",
			    flags_173a[0], flags_173a[1]);
		v92modem.modulator->byte_0c = flags_173a[0];
		v92modem.modulator->byte_0d = flags_173a[1];
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
	 * 0xe83c.  End of CPt.  `byte_6119` is the flag that says the retrain
	 * bit has to go back on in the shared byte.
	 */
	case 0x16:
		byte_6118 = 2;
		ret = 1;
		v92modem.modulator->exitCPt();
		if (byte_6119 != 0)
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
		byte_6118 = 3;
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
		flag_173d = 1;
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
		byte_6118 = 2;
		ret = 1;
		if (v92modem.modulator->phase == V92MOD_PHASE_DATA) {
			v92modem.modulator->initiateRRN();
			v92modem.modulator->phase4Modulator->word_2c =
			    modem.demodulator->connectionEvaluator->word_90;
		}
		VPcmV34IndicateLocalRRN(v34Object);
		break;

	case 0x23:
		byte_6118 = 2;
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
		byte_6118 = 2;
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
		byte_6118 = 1;
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
			edprintf("VPcmFloModem (V92): got V90 Fallback "
				 "request...\r\n");
			ret = 6;
		}
		break;

	case 10:			/* 0xe518 */
		byte_6118 = 3;
		break;

	default:
		break;
	}

	return ret;			/* 0xe530 */
}

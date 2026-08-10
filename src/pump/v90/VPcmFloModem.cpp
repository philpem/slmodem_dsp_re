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
#include "dsplib/v34pcmif.h"
/*
 * `V92Parameters::init()` is one of the three `externalReset` calls.  This
 * header defines nothing else and includes nothing, so it cannot collide with
 * the `V90Parameters` this file already has through `V90SessionFlag.h`.
 */
#include "dsplib/V92Parameters.h"
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
VPCM_OFF(flags_0217,		0x0217, flags217);
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
VPCM_OFF(v92Params,		0x6128, v92params);
VPCM_OFF(word_6f98,		0x6f98, word6f98);
VPCM_OFF(word_6fac,		0x6fac, word6fac);
VPCM_OFF(word_6fb0,		0x6fb0, word6fb0);
VPCM_OFF(word_6fb4,		0x6fb4, word6fb4);
VPCM_OFF(v92Phase2Info,		0x612c, p92);
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
 * +0x004 and `flags_0217` at +0x217, and what makes those two consistent is
 * that `tagV90DILdescriptor` is exactly 0x213 bytes.  If a later batch gives
 * the descriptor another field, this fails here instead of silently shifting
 * everything from +0x217 to +0x7f27.
 */
typedef char vpcm_dil_size[(sizeof(tagV90DILdescriptor) == 0x213) ? 1 : -1];

/*
 * V92Phase2Info's, because this is the only translation unit that uses the
 * class and it has no .cpp of its own -- it is data-only, none of its three
 * members in the blob is written here, and a header with no source file has
 * nowhere else to put an assertion.  Without these, `pad_0a[2]` and
 * `pad_14[3]` are load-bearing and unguarded.
 *
 * FOUR OF THESE ARE NOT PINNED BY THE DIFFERENTIAL TEST, and that is why they
 * are worth asserting: nothing here reads `Uinfo`, `shortPhase2Local` or
 * `v90UseHighCarrier`, and their offsets come from V92Phase2Info::printInfo's
 * disassembly rather than from anything that runs.
 */
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
V92P2I_OFF(v90UseHighCarrier,		0x17, highcarrier);
V92P2I_OFF(array_18,			0x18, a18);
V92P2I_OFF(array_1c,			0x1c, a1c);
V92P2I_OFF(L2,				0x20, l2);
V92P2I_OFF(array_24,			0x24, a24);
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

	p92 = v92Phase2Info;
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

	p92 = v92Phase2Info;
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

	flags_0217[0] = 1;
	flags_0217[1] = 0;
	flags_0217[2] = 1;
	flags_0217[3] = 1;
	flags_0217[4] = 1;
	flags_0217[5] = 1;

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

	flags_0217[0] = 1;
	flags_0217[1] = 0;
	flags_0217[2] = 1;
	flags_0217[3] = 1;
	flags_0217[4] = 1;
	flags_0217[5] = 0;

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
	flags_0217[0] = 1;		/* +0x217 */
	flags_0217[1] = 0;		/* +0x218 */
	flags_0217[2] = 1;		/* +0x219 */
	flags_0217[3] = 1;		/* +0x21a */
	flags_0217[4] = 1;		/* +0x21b */
	flags_0217[5] = 0;		/* +0x21c */

	modem.ptr_49b4->initSession();
	modem.ptr_49b4->init();
	v92Params->init();

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
	flags_0217[0] = 1;		/* +0x217 again */
	flags_0217[1] = 0;		/* +0x218 again */
	flags_0217[2] = 1;		/* +0x219 again */
	flags_0217[3] = 1;		/* +0x21a again */
	flags_0217[4] = 1;		/* +0x21b again */
	flags_0217[5] = 0;		/* +0x21c again */

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

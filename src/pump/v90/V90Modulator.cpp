/*
 * V90Modulator.cpp -- V90Modulator's constructor and destructor.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90Modulator.h` carries the
 * object map, the 0x70 size and the argument-to-offset table; this file is
 * the two functions and the assertions that hold the compiler to that map.
 * `setSessionFlag` stays where it was, in src/pump/v90/V90SessionFlag.cpp.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding F215).
 *
 * THREE SUB-OBJECTS ARE OWNED AND FIVE ALLOCATIONS ARE MADE, IN THIS ORDER:
 * a symbol buffer of 2 bytes per symbol, a frame buffer of 8, a
 * `V90BitsToSymbol` of 0x24 built with `3 * nofSymbols + 0x1388` symbols, a
 * `V90Phase3Modulator` of 0x398, and a `V90Phase4Modulator` of 0x2fac.  Each
 * of the last three is `sysdep_malloc(sizeof)` immediately followed by the
 * class's `C1`, which is where those three sizes come from -- finding F1246.
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

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/sysdep.h"
#include "dsplib/V90BitsToSymbol.h"
#include "dsplib/V90CP.h"
#include "dsplib/V90Mapper.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Modulator.h"
#include "dsplib/V90MP.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Phase2Info.h"
#include "dsplib/V90Phase3Modulator.h"
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
V90MOD_OFF(state,		0x2c, state);
V90MOD_OFF(symbolCount,		0x30, symcount);
V90MOD_OFF(eventCode,		0x34, eventcode);
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
 * allocations of which none is null-checked.  +0x2c..+0x37 is left exactly as
 * it was found; `reset` is the member that clears it.
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
	frameBuf = (unsigned char *)sysdep_malloc(8 * n);

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

/*
 * ===========================================================================
 * V90Modulator::reset -- .text+0x1a510, 78 bytes
 *
 * The class's last member, and the smallest: one gated diagnostic, one call
 * into the embedded scrambler, and three words cleared.
 *
 * THE DIAGNOSTIC IS `dsplibs_debug_printf` BEHIND `DSPLIB_DEBUG_ON()` AND NOT
 * `edprintf`.  0x1a518 is `cmpl $0x1,dsplibs_debug_level` with `ja`, so the
 * call only happens above level 1 -- unlike the V.90 spectral group, whose
 * diagnostics go through `edprintf` and run at every level because `edprintf`
 * tests the level after it has already formatted and encoded.  The two are
 * not interchangeable and the object picks one per site.
 *
 * THE PRINT COMES FIRST IN THE OBJECT AND FIRST HERE, but the compiler has
 * moved its BODY out of line to 0x1a550 and jumps back -- the ordinary layout
 * for an unlikely arm, and not a statement about order.
 *
 * THE SCRAMBLER IS RESET, NOT RECONSTRUCTED.  `lea 0x44(%ebx),%eax` and a
 * call to `Scrambler<int,unsigned char>::reset(int)` with 0; the taps set by
 * the constructor's member-initialiser are untouched, which is what makes
 * this a per-connection reset rather than a rebuild.
 * ===========================================================================
 */
void
V90Modulator::reset()
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modulator reset\r\n");

	scrambler.reset(0);

	state = 0;
	symbolCount = 0;
	eventCode = 0;
}

/*
 * ===========================================================================
 * THE ELEVEN PHASE EDGES, in the order the object lays them out:
 * .text+0x19d70 through +0x1a2b5, then `initiateFPE` at +0x1a400 after
 * `initiateRRN`.
 *
 * NOTHING IN THIS CLASS CALLS ANY OF THEM, checked over all SEVENTEEN members
 * and not over the five that happened to be written first -- which is the
 * mistake finding F7520 recorded against itself.  `progress` open-codes the
 * phase-3-to-4 move rather than calling `enterPhase4`; none of the eleven
 * calls a sibling; and `setSessionFlag`, the one member a range disassembly of
 * this span misses because it lives in V90SessionFlag.cpp, relocates only
 * against the two sub-modulators' own `setSessionFlag` (0x19d50 and 0x19d6b).
 * So each of the eleven is a separate entry point for whatever above this
 * class reads the far end.
 *
 * THE GATED MESSAGES ARE `dsplibs_debug_printf` AND THE SPECTRAL ONES ARE
 * `edprintf`, per site, exactly as `reset` and `initiateRRN` already split
 * them.  The spectral group prints at every level because `edprintf` tests
 * the level after formatting; a `cmpl $0x1,dsplibs_debug_level` in front of a
 * call is the other kind.  Both kinds appear inside single functions here --
 * `exitRi`, `enterDataPhase` and `initiateFPE` each carry one of each.
 * ===========================================================================
 */

/*
 * ENTER PHASE 3 -- .text+0x19d70, 130 bytes.  Idempotent: already being in
 * phase 3 is the whole guard, and the object tests it before the message so a
 * repeated entry is silent as well as inert.
 *
 * THE PHASE 3 MODULATOR IS REBUILT FROM `phase2Info` AND THE THREE SEQUENCE
 * OBJECTS.  Its fourth argument -- `nSymbols`, the warm-up count `reset`'s
 * trailing loop generates immediately -- is ZERO here, so nothing is emitted
 * inside the reset; the eighth is `phase2Info->rtd`, which lands in
 * `timeoutBase` and is what both phase 3 timeouts are measured from.
 *
 * The starting state is `P3M_STATE_SD`, which is 0 and is also what a fresh
 * object holds; the object still passes it explicitly (`xor %ecx,%ecx` into
 * 0xc(%esp) at 0x19dab).
 */
void
V90Modulator::enterPhase3()
{
	if (state == 1)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modulator enter Phase 3\r\n");

	phase3Modulator->reset((PcmType)phase2Info->pcmType, phase2Info->Uinfo,
			       P3M_STATE_SD, 0, jd, v92Jd, dil,
			       phase2Info->rtd);

	state = 1;
	symbolCount = 0;
	eventCode = 0;
}

/*
 * LEAVE Jd -- .text+0x19e00, 68 bytes -- and LEAVE JdPhase, +0x19e50, the
 * same 68.  Near-twins, and the two differences are the guard value and which
 * member of the phase 3 modulator is called.
 *
 * THE GUARD IS THE SUB-MODULATOR'S STATE AND THE CALLEE RE-TESTS IT.  Both
 * `V90Phase3Modulator::exitJd` and `::exitJdPhase` open with the same
 * comparison, so through this path the callee's guard can never be false --
 * which is why the fixture also drives those two directly.
 *
 * `eventCode` is cleared AFTER the call and unconditionally within the arm:
 * `movl $0x0,0x34(%ebx)` at 0x19e27 sits on the path both the gated and the
 * ungated entry reach.
 */
void
V90Modulator::exitJd()
{
	if (phase3Modulator->state != P3M_STATE_JD)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modulator: exit Jd\r\n");

	phase3Modulator->exitJd();
	eventCode = 0;
}

void
V90Modulator::exitJdPhase()
{
	if (phase3Modulator->state != P3M_STATE_JD_PHASE)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modulator: exit JdPhase\r\n");

	phase3Modulator->exitJdPhase();
	eventCode = 0;
}

/*
 * LEAVE Ri -- .text+0x19ea0, 169 bytes.  The widest of the edges and the only
 * member of the class that reads `mappingParams` (+0x10) rather than
 * `mappingParams2` (+0x14).  Every load in this function is `0x10(%ebx)`:
 * 0x19eca for `setMappingParams`, 0x19ee8 for `displaySpectralParams`,
 * 0x19ef3 for the message argument, and 0x19f0f / 0x19f22 for the store.
 * `progress`, `initiateRRN`, `enterDataPhase` and `initiateFPE` all take the
 * other one.
 *
 * SO THE TWO SIDES OF THE Ri EXIT ARE NOT SPELLED THE SAME.  Phase 4's
 * TRN2d/MP half is configured from block 6 here; the data phase and both
 * renegotiation edges take block 7.  Nothing in the object says why, and this
 * reconstruction does not invent a reason -- but a fixture whose two blocks
 * hold the same numbers cannot see the difference at all, so t_v90modprog.cpp
 * gives them different first words before it calls this.
 *
 * THE MESSAGE AND THE STORE CARRY THE SAME VALUE, `mappingParams->word_0`,
 * which is the block's bit count per frame.  "TRN2d D = %d" prints it; the
 * store puts it in the message object's `calcSequenceLength` divisor, so the
 * MP or CP sequence is padded out to a whole number of frames.  Which object
 * gets it is `sessionFlag`'s fork, the same fork `initiateRRN` makes and with
 * the same polarity: nonzero is V.92 and writes the `V90CP`, zero is V.90 and
 * writes the `V90MP`.
 *
 * The three field names stay as their own headers have them.  `word_3ba8`,
 * `word_114` and `word_0` are each already commented with their role; what
 * this edge adds is where the value comes from, which is recorded in the
 * finding rather than by renaming three fields in three files this batch does
 * not own.
 */
void
V90Modulator::exitRi()
{
	if (phase4Modulator->state != P4M_STATE_RI)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modulator: exit Ri\r\n");

	phase4Modulator->exitRi();
	eventCode = 0;

	phase4Modulator->setMappingParams(mappingParams);

	edprintf("V90Modulator: TRN2d spectral parameters:\r\n");
	displaySpectralParams(mappingParams);
	edprintf("V90Modulator: TRN2d D = %d\r\n", mappingParams->word_0);

	if (sessionFlag)
		cp->word_3ba8 = mappingParams->word_0;
	else
		mp->word_114 = mappingParams->word_0;
}

/*
 * CP RECEIVED -- .text+0x19f50, 110 bytes.
 *
 * THE MP IS RE-ENCODED BEFORE ANY STATE IS LOOKED AT.  `CPack` goes to 1 --
 * the acknowledge bit, bits[0x21] of the packed message -- and `infoToBits`
 * runs, both unconditionally; only then is the phase 4 state consulted.  So a
 * CP arriving in the wrong state still changes what this modem transmits next
 * time it sends an MP, which is the object's behaviour and not obviously
 * intended.
 *
 * `setNextStateAfterTRN2d(P4M_STATE_MP_NOT)` is likewise unconditional, and
 * it is a store into the phase 4 modulator's +0x10 that only takes effect if
 * TRN2d is still to come.
 */
void
V90Modulator::acknowledgeCPReception()
{
	mp->CPack = 1;
	mp->infoToBits();

	phase4Modulator->setNextStateAfterTRN2d(P4M_STATE_MP_NOT);

	if (phase4Modulator->state != P4M_STATE_MP)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modulator: exit MP due to CP "
				     "reception\r\n");

	phase4Modulator->exitMP();
	eventCode = 0;
}

/*
 * CPnot RECEIVED -- .text+0x19fc0, 167 bytes -- and E RECEIVED, +0x1a070,
 * 110.  Two chains over the phase 4 state, and the ORDER the object tests
 * them in is the source's: 0x19fce compares 0x0e, then 0x04, then 0x0d, and
 * 0x1a07e compares 0x0e then 0x0d.  Neither order is sorted, so neither is a
 * switch's decision tree; they are `else if` chains written the way they are
 * read here.
 *
 * THE 0x0d ARM IS A DELAY, NOT AN EXIT.  `byte_0014 = 1` and a message; the
 * state is left alone and `eventCode` is NOT cleared.  0x0d is
 * `V90Phase4Modulator.h`'s "`exitMP`'s non-boundary arm" -- MP has been left
 * but the symbol count has not reached a repetition boundary yet -- so the
 * request is recorded for whoever crosses that boundary.  It is also the only
 * arm whose whole effect is one byte: below the gate nothing else happens,
 * which is what makes both debug levels load-bearing for it.
 *
 * The two functions share the 0x0e arm exactly and share the 0x0d arm apart
 * from its message.  `acknowledgeCPNotReception` has the third arm, 0x04,
 * which re-encodes the MP first -- the same two statements
 * `acknowledgeCPReception` opens with, and here they are inside a case.
 */
void
V90Modulator::acknowledgeCPNotReception()
{
	if (phase4Modulator->state == P4M_STATE_MP_NOT) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Modulator: exit MPNot due to "
					     "CPNot reception\r\n");

		phase4Modulator->exitMPNot();
		eventCode = 0;
	} else if (phase4Modulator->state == P4M_STATE_MP) {
		mp->CPack = 1;
		mp->infoToBits();

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Modulator: exit MP due to "
					     "CPNot reception\r\n");

		phase4Modulator->exitMP();
		eventCode = 0;
	} else if (phase4Modulator->state == P4M_STATE_UNNAMED_0D) {
		phase4Modulator->byte_0014 = 1;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Modulator: setting delayed "
					     "MPNot exit due to CPNot "
					     "reception\r\n");
	}
}

void
V90Modulator::acknowledgeEReception()
{
	if (phase4Modulator->state == P4M_STATE_MP_NOT) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Modulator: exit MPNot due to "
					     "E reception\r\n");

		phase4Modulator->exitMPNot();
		eventCode = 0;
	} else if (phase4Modulator->state == P4M_STATE_UNNAMED_0D) {
		phase4Modulator->byte_0014 = 1;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Modulator: setting delayed "
					     "MPNot exit due to E "
					     "reception\r\n");
	}
}

/*
 * ENTER THE DATA PHASE -- .text+0x1a0e0, 190 bytes.  The same eight
 * statements `progress`'s phase 4 arm reaches when the phase terminates, with
 * an idempotence guard in front of them: the block at 0x1a0ee..0x1a14d and
 * the one at 0x1a9a1..0x1aa00 are the same code twice in the object, not a
 * shared helper, and this reconstruction does not make them one.
 *
 * Every part of the rate expression is forced and the derivation is in
 * `progress`'s comment: `bitsToSymbol->mapper->bitsPerFrame` two levels down
 * and not the converter's same-named field, an INTEGER multiply by 8000
 * widened unsigned, a multiply by the float nearest 1/6 rather than a divide,
 * and `fistpll` with the low word taken, which is a conversion to
 * `unsigned int`.
 *
 * THE TERNARY IS ALWAYS TRUE WHERE IT IS EVALUATED, for the reason it is in
 * `progress`: `setSymbolsBlockSize` sits between the store and the test and
 * may alias `*this`, so the compiler cannot fold it -- but that explains why
 * the test survives, not why it is there, and the source has to contain it.
 */
void
V90Modulator::enterDataPhase()
{
	if (state == 3)
		return;

	edprintf("V90Modulator: Data Phase spectral parameters:\r\n");
	displaySpectralParams(mappingParams2);

	state = 3;
	symbolCount = 0;
	eventCode = 8;

	bitsToSymbol->setSymbolsBlockSize(nofSymbols);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modulator: enter Data Phase, "
				     "Rate = %d [bps]\r\n",
				     state == 3
					 ? (unsigned int)(0.5f +
					       (8000 *
						bitsToSymbol->mapper->
						    bitsPerFrame) *
					       (1.0f / 6.0f))
					 : 0u);
}

/*
 * ENTER PHASE 4 -- .text+0x1a1a0, 110 bytes.  Idempotent on `state == 2`, and
 * its body is the third copy of the phase 4 entry: `progress`'s inner arm and
 * `exitDIL`'s tail are the other two, all three with the same message, the
 * same `reset` arguments and the same three closing stores.
 *
 * `P4M_STATE_RI` is 0 and `reset`'s fourth argument is 0 as well; the object
 * stores both explicitly (`xor %eax,%eax` and `xor %ecx,%ecx` at 0x1a1b9 and
 * 0x1a1c6).  Ri is the first thing the digital modem sends in phase 4.
 */
void
V90Modulator::enterPhase4()
{
	if (state == 2)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modulator: enter Phase 4\r\n");

	phase4Modulator->reset((PcmType)phase2Info->pcmType, phase2Info->Uinfo,
			       P4M_STATE_RI, 0, phase2Info->rtd);

	state = 2;
	symbolCount = 0;
	eventCode = 0;
}

/*
 * LEAVE DIL -- .text+0x1a210, 166 bytes.  Two halves: the exit itself, and
 * then a conditional move into phase 4 that the exit may have earned.
 *
 * THE SECOND HALF TESTS THE SUB-MODULATOR'S OWN `eventCode`, NOT OURS.
 * 0x1a237 reloads +0x38 and reads `0x1c(%eax)`, and 6 is the value
 * `V90Phase3Modulator::exitDIL` writes when the DIL sequence really ends
 * (it leaves the event alone on its DIL_END arm).  Nothing copies that event
 * into `V90Modulator::eventCode`; this member only ever writes a zero there,
 * and only on the phase 4 path.
 *
 * `state != 2` is then the same second guard `progress`'s phase 3 arm uses,
 * and for the same reason: the move into phase 4 must not be made twice.
 */
void
V90Modulator::exitDIL()
{
	if (phase3Modulator->state != P3M_STATE_DIL)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modulator: exit DIL\r\n");

	phase3Modulator->exitDIL();

	if (phase3Modulator->eventCode != 6)
		return;
	if (state == 2)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modulator: enter Phase 4\r\n");

	phase4Modulator->reset((PcmType)phase2Info->pcmType, phase2Info->Uinfo,
			       P4M_STATE_RI, 0, phase2Info->rtd);

	state = 2;
	symbolCount = 0;
	eventCode = 0;
}

/*
 * ===========================================================================
 * V90Modulator::initiateRRN -- .text+0x1a2c0, 308 bytes
 *
 * RATE RENEGOTIATION, REQUESTED FROM OUTSIDE.  The only approved state is the
 * data phase; from anywhere else this answers -1 and does nothing, which is
 * what the second message calls "requested but NOT approved".  The two
 * returns are `mov $0x0,%eax` at 0x1a38f and `mov $0xffffffff,%eax` at
 * 0x1a3da and 0x1a3ed -- both arranged, so the `int` is real.
 *
 * IT IS ALSO CALLED FROM `progress`, on the data phase's own timer, so the
 * `state != 3` guard is not dead there either: `progress` reaches it only
 * from `case 3`, and every other caller is outside this object.
 *
 * THE BLOCK SIZE GOES TO ONE AND THE ANSWER IS ASKED FOR TWICE.
 * `setSymbolsBlockSize` RETURNS `nofBitsForNextTime()` -- the blob inlines
 * the whole of the second into the first (V90BitsToSymbol.cpp) -- and this
 * function then calls `nofBitsForNextTime` separately anyway, at 0x1a2fd and
 * 0x1a308.  Two calls, two `call` relocations, and the first one's result is
 * dropped on the floor.  That is the object's and it is written that way.
 *
 * WHICH PHASE 4 STATE IS CHOSEN BY THAT ANSWER, and the two messages name
 * both: nonzero -- bits still owed for the block just sized -- enters
 * `RdModulation`, and zero enters `DataToRdModulation`.  Those are the
 * object's words for 0x15 and 0x14, and 0x14 is the enumerator
 * `V90Phase4Modulator.h` deliberately left unnamed because the only evidence
 * for it was a jump-table slot.  The name is NOT taken here: promoting it is
 * a change to a 1,829-line file this batch does not own, and the message
 * belongs to `V90Modulator` rather than to the state's own class.  Recorded
 * in finding F7520 as evidence available to whoever does own it.
 *
 * THE TWO MESSAGES ARE `edprintf` AND ARE NOT GATED.  0x1a321 is a bare
 * `call` with no `cmpl $0x1,dsplibs_debug_level` in front of it, unlike the
 * two `dsplibs_debug_printf` sites at either end of the function.  So a real
 * session prints these two whatever the level is.
 *
 * THE CP/MP FORK IS `sessionFlag` AND IT PICKS THE MESSAGE CLASS.  Nonzero
 * (V.92) clears `V90CP::byte_13` -- bits[0x21] -- and re-encodes the CP;
 * zero (V.90) clears `V90MP::CPack`, the MP/MPnot discriminator, and
 * re-encodes the MP.  Two fields at two different offsets in two different
 * classes, each cleared immediately before its own `infoToBits`.
 * ===========================================================================
 */
int
V90Modulator::initiateRRN()
{
	Phase4ModulatorState p4state;

	if (state != 3) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Modulator: RRN requested but "
					     "NOT approved\r\n");
		return -1;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modulator: RRN requested, enter "
				     "Phase 4\r\n");

	state = 2;
	symbolCount = 0;

	bitsToSymbol->setSymbolsBlockSize(1);

	if (bitsToSymbol->nofBitsForNextTime() != 0) {
		edprintf("V90Modulator: Phase4Modulator state initialized to "
			 "RdModulation\r\n");
		p4state = P4M_STATE_RD;
	} else {
		edprintf("V90Modulator: Phase4Modulator state initialized to "
			 "DataToRdModulation\r\n");
		p4state = P4M_STATE_UNNAMED_14;
	}

	phase4Modulator->reset((PcmType)phase2Info->pcmType, phase2Info->Uinfo,
			       p4state, 0, phase2Info->rtd);

	eventCode = 0;

	phase4Modulator->resetBeforRRN();
	phase4Modulator->setRdRtSymbols(mappingParams2);

	if (sessionFlag) {
		cp->byte_13 = 0;
		cp->infoToBits();
	} else {
		mp->CPack = 0;
		mp->infoToBits();
	}

	return 0;
}

/*
 * ===========================================================================
 * V90Modulator::initiateFPE -- .text+0x1a400, 260 bytes
 *
 * FAST PHASE EXCHANGE, and `initiateRRN` FOUR DIFFERENCES AWAY.  Same guard,
 * same two returns, same block-size-to-one and the same question asked of
 * `nofBitsForNextTime`, and then:
 *
 *   - the two states are 0x1d and 0x1c, where RRN picks 0x15 and 0x14;
 *   - there is NO `resetBeforRRN` and no member of that shape at all;
 *   - the mapping block goes to `setRfSymbols`, where RRN uses
 *     `setRdRtSymbols`;
 *   - THE CP IS RE-ENCODED UNCONDITIONALLY.  RRN forks on `sessionFlag` and
 *     touches the `V90MP` on the V.90 arm; this one clears `V90CP::byte_13`
 *     and calls `V90CP::infoToBits` on every path, so a V.90 session's MP is
 *     never touched by an FPE.  0x1a4a7 has no test in front of it.
 *
 * V.92's own pair is the same shape one modulation up (finding F7541), and it
 * has a fifth difference this one does not: no store into the modulus
 * encoder.
 *
 * WHICH STATE THE BIT COUNT CHOOSES, and the two messages name both:
 * NON-ZERO -- nothing banked in the converter -- enters `RfModulation`,
 * 0x1d, which is already `P4M_STATE_RF`; ZERO enters "DataToRfModulation",
 * which is 0x1c and which `V90Phase4Modulator.h` calls
 * `P4M_STATE_UNNAMED_1C`.  That enumerator is NOT promoted here, on the same
 * two grounds finding F7520 gave for 0x14: the message belongs to this class
 * rather than to the state's own, and the rename is a change to an
 * 1,829-line file this batch does not own.  The evidence is recorded so it
 * does not have to be re-derived.
 *
 * `symbolCount` IS CLEARED AND `eventCode` IS NOT, until after the reset --
 * 0x1a42c and 0x1a48e, with the whole of `setSymbolsBlockSize`,
 * `nofBitsForNextTime`, the message and `V90Phase4Modulator::reset` between
 * them.  `initiateRRN` sequences its three the same way.
 * ===========================================================================
 */
int
V90Modulator::initiateFPE()
{
	Phase4ModulatorState p4state;

	if (state != 3) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Modulator: FPE requested but "
					     "NOT approved\r\n");
		return -1;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modulator: FPE requested, enter "
				     "Phase 4\r\n");

	state = 2;
	symbolCount = 0;

	bitsToSymbol->setSymbolsBlockSize(1);

	if (bitsToSymbol->nofBitsForNextTime() != 0) {
		edprintf("V90Modulator: Phase4Modulator state initialized to "
			 "RfModulation\r\n");
		p4state = P4M_STATE_RF;
	} else {
		edprintf("V90Modulator: Phase4Modulator state initialized to "
			 "DataToRfModulation\r\n");
		p4state = P4M_STATE_UNNAMED_1C;
	}

	phase4Modulator->reset((PcmType)phase2Info->pcmType, phase2Info->Uinfo,
			       p4state, 0, phase2Info->rtd);

	eventCode = 0;

	phase4Modulator->setRfSymbols(mappingParams2);

	cp->byte_13 = 0;
	cp->infoToBits();

	return 0;
}

/*
 * ===========================================================================
 * V90Modulator::progress -- .text+0x1a820, 780 bytes
 *
 * THE TRANSMIT CHAIN'S PER-BLOCK ENTRY POINT.  One switch over `state`, four
 * live arms and a default, and a shared tail that widens `symbolBuf` into the
 * caller's `float *`.  `void`: every arm reaches the same epilogue at 0x1a89c
 * and none of them arranges %eax, and `V90Modem::progress` tail-JUMPS here,
 * so whatever this returns is what that returns.
 *
 * THE SWITCH SELECTOR IS SIGNED and that is the one thing about this function
 * a behavioural test can only see through a negative state.  0x1a848 is
 * `cmp $0x1,%eax ; je ; jle`, and the `jle` arm then does
 * `test %eax,%eax ; jne <default>` -- so a NEGATIVE state is the default and
 * 0 is the silence arm.  With an `unsigned int` member GCC emits `jbe` there
 * and folds the two into one edge.  See V90Modulator.h.
 *
 * `state == 1` IS RE-TESTED INSIDE THE PHASE 3 LOOP, and it is not redundant:
 * the phase 3 arm can move the state to 2 part way through a block, and the
 * remaining symbols of that same block then come from the phase 4 modulator.
 * The object reloads +0x2c at 0x1a8da on every iteration, which is what a
 * compiler that cannot see across `generateSymbol` has to do -- but the
 * SOURCE has to contain the test for the arm to exist at all.
 *
 * THE TWO EVENT FIELDS ARE READ THROUGH SEPARATE MEMBER LOADS.  `mov 0x3c
 * (%ebx),%edx` after the call and then `mov 0xc(%edx),%eax`, rather than the
 * pointer being kept across it: the callee may have moved it.  Same in the
 * phase 3 arm with +0x38 and +0x1c.
 *
 * THE RATE MESSAGE READS `bitsToSymbol->mapper->bitsPerFrame`, TWO LEVELS
 * DOWN, AND NOT `bitsToSymbol->bitsPerFrame`.  0x1aae8 is `mov 0x40(%ebx),
 * %edx ; mov (%edx),%eax ; imul $0x1f40,0x4(%eax),%esi`: +0x40 is the
 * converter, +0x00 of that is its mapper, +0x04 of that is the mapper's own
 * `bitsPerFrame`.  The converter has a field of the same name at ITS +0x14
 * and both are seeded from the mapping parameters' first word, so in any
 * naturally reset object the two hold the same number and the wrong one
 * passes every test.  test/unit/t_v90modprog.cpp drives them apart on
 * purpose.
 *
 * THE RATE ITSELF is `0.5f + (float)(8000 * bitsPerFrame) * (1.0f / 6.0f)`
 * truncated to `unsigned int`: six symbols to a frame at 8 kHz, and the 0.5
 * makes the truncation a round.  Every part of that spelling is forced.
 * `imul $0x1f40` then `push $0 ; push %esi ; fildll` is an INTEGER product
 * widened as UNSIGNED, so the multiply by 8000 is integer and the operand is
 * unsigned; `fmuls` against 0x3e2aaaab is a multiply by the float nearest 1/6
 * and not a divide by 6.0f, which GCC cannot introduce; and `fistpll` into
 * eight bytes with the low word taken is float -> `unsigned int`, where a
 * cast to `int` emits `fistpl`.
 *
 * THE PRINTF ARGUMENT IS GUARDED BY A TEST THAT IS ALWAYS TRUE.  `state` was
 * just set to 3 eight instructions earlier, and 0x1a9ea tests it against 3
 * again and passes 0 if it fails.  `setSymbolsBlockSize` sits between the
 * store and the test and might alias `*this`, so the compiler cannot fold it
 * -- but that only explains why the test SURVIVES, not why it is there, and
 * the source must contain it.
 *
 * WHAT IT DOES NOT DO IS CLEAR `symbolBuf` OUTSIDE THE SILENCE ARM.  The
 * float tail always copies `nofSymbols` entries out of it, so an arm that
 * wrote fewer than it hands out leaves the previous block's symbols in the
 * gap.  Only `case 0` fills the whole buffer; `case 3` writes whatever
 * `V90BitsToSymbol::process` handed out, which is `symbolsBlockSize` and not
 * this call's `n`.  That is the object's behaviour, reproduced.
 * ===========================================================================
 */
void
V90Modulator::progress(int *bits, unsigned int &nofBits, float *out,
		       unsigned int n)
{
	unsigned int i;

	symbolCount += n;
	eventCode = 0;

	switch (state) {
	case 0:
		/*
		 * Silence.  The whole buffer, not `symbolsBlockSize` of it.
		 */
		for (i = 0; i < n; i++)
			symbolBuf[i] = 0;
		nofBits = 0;
		break;

	case 1:
		for (i = 0; i < n; i++) {
			if (state == 1) {
				symbolBuf[i] =
				    (short)phase3Modulator->generateSymbol();

				if (phase3Modulator->eventCode != 0)
					eventCode = phase3Modulator->eventCode;

				if (eventCode == 6 && state != 2) {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "V90Modulator: enter "
						    "Phase 4\r\n");

					phase4Modulator->reset(
					    (PcmType)phase2Info->pcmType,
					    phase2Info->Uinfo, P4M_STATE_RI, 0,
					    phase2Info->rtd);

					state = 2;
					symbolCount = 0;
					eventCode = 0;
				}
			} else {
				symbolBuf[i] =
				    (short)phase4Modulator->generateSymbol();

				if (phase4Modulator->word_000c != 0)
					eventCode = phase4Modulator->word_000c;
			}
		}
		nofBits = 0;
		break;

	case 2:
		for (i = 0; i < n; i++) {
			symbolBuf[i] =
			    (short)phase4Modulator->generateSymbol();

			if (phase4Modulator->word_000c != 0)
				eventCode = phase4Modulator->word_000c;
		}

		if (eventCode != 7) {
			nofBits = 0;
			break;
		}

		if (state != 3) {
			edprintf("V90Modulator: Data Phase spectral "
				 "parameters:\r\n");
			displaySpectralParams(mappingParams2);

			state = 3;
			symbolCount = 0;
			eventCode = 8;

			bitsToSymbol->setSymbolsBlockSize(nofSymbols);

			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V90Modulator: enter Data Phase, "
				    "Rate = %d [bps]\r\n",
				    state == 3
					? (unsigned int)(0.5f +
					      (8000 *
					       bitsToSymbol->mapper->
						   bitsPerFrame) *
					      (1.0f / 6.0f))
					: 0u);
		}

		nofBits = bitsToSymbol->nofBitsForNextTime();
		break;

	case 3:
		scrambler.process(bits, frameBuf, nofBits);
		bitsToSymbol->process(frameBuf, nofBits, symbolBuf);

		/*
		 * THE COMPARISON IS UNSIGNED AND THE CAST IS WHAT MAKES IT
		 * SAY SO.  0x1aab2 is `cmp %ecx,0x30(%ebx) ; jbe`, and the
		 * parameter is declared `int` -- so the unsignedness comes
		 * from `symbolCount`, and C++'s usual arithmetic conversions
		 * would produce exactly this with no cast at all.  It is
		 * spelled because `-Wsign-compare` is on and a warning here
		 * would read as a defect rather than as the object's own
		 * shape; the cast is the conversion the language already
		 * performs and moves no instruction.  Finding F4903's rule.
		 */
		if (params->DEBUG_DIGITAL_MODEM_INITIATE_RRN &&
		    symbolCount > (unsigned int)
			params->DEBUG_DIGITAL_MODEM_INITIATE_RRN_TIME) {
			initiateRRN();
			eventCode = 9;
		}
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Modulator progress: Illegal "
					     "state\r\n");
		break;
	}

	for (i = 0; i < n; i++)
		out[i] = symbolBuf[i];
}

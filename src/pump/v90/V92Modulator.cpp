/*
 * V92Modulator.cpp -- the V.92 upstream modulator: the object that owns the
 * whole transmit graph, and the phase machine that drives it.
 *
 * Reconstructed from dsplibs.o.  ALL EIGHTEEN of the class's symbols,
 * 5,614 bytes -- 2,474 in the two constructor and two destructor variants and
 * 3,140 in the fourteen members:
 *
 *     V92Modulator::V92Modulator(unsigned, V92Phase2Info *, V92Ja *,
 *         tagV90DILdescriptor *, V92CP *, V92MappingParams *,
 *         V92Parameters *)          .text+0x15120 (C1), +0x15400 (C2)
 *     V92Modulator::~V92Modulator() .text+0x14050 (D2), +0x14250 (D1)
 *     getV92TxFilterDelay           +0x14450    21 B
 *     enterPhase3                   +0x14470   126 B
 *     exitJa                        +0x144f0    68 B
 *     exitSilence                   +0x14540    68 B
 *     exitSuSecond                  +0x14590    68 B
 *     exitTRN1uSecond               +0x145e0    68 B
 *     exitCPt                       +0x14630    80 B
 *     initiateRRN                   +0x14680   260 B
 *     initiateFPE                   +0x14790   276 B
 *     enterPhase4                   +0x148b0   113 B
 *     enterDataPhase                +0x14930    69 B
 *     mkResampledSignal             +0x14980   662 B
 *     progress                      +0x14c20 1,075 B
 *     reset                         +0x15060   186 B
 *
 * Each constructor and destructor pair is byte-identical bar register
 * allocation; GCC emits both from one definition.
 * `include/dsplib/V92Modulator.h` carries the object map, the 0x90 the
 * allocation gives, the eleven owned pieces and the one word nobody writes.
 *
 * THE CONSTRUCTOR ENDS BY CALLING `reset()`, and that it is INLINED there is
 * measured rather than assumed: `V92Modulator::reset` is its own 186-byte
 * symbol at .text+0x15060, and the constructor's last 152 bytes are its body
 * statement for statement -- the same `Scrambler<int,unsigned char>::reset(
 * this+0x54, 0)`, the same five clears, the same `Queue<float>::reset`, the
 * same `params->MODULATOR_QUEUE_LENGTH >> 1` and priming loop, and the same
 * closing `FloatFIR::reset` -- down to the "V92Modulator reset" the two both
 * print.  Finding 1283.  `V92Phase3Modulator`'s constructor calls its own
 * `reset` the same way, and is the precedent this file now follows.
 *
 * THE PHASE MACHINE, which the fourteen members below are the whole of.
 * `phase` is 0 out of `reset`, and three members move it:
 *
 *     enterPhase3     -> 1, and resets the phase 3 modulator
 *     enterPhase4     -> 2, and resets the phase 4 modulator
 *     enterDataPhase  -> 3, and hands the block size to the bit-to-symbol
 *                          stage
 *
 * THE ONLY CALLER OF TWO OF THE THREE IS `progress`, and it makes both
 * transitions from inside its own symbol loop: on the phase 3 modulator's
 * event code 8 it enters phase 4, and on the phase 4 modulator's code 9 it
 * enters the data phase.  So the machine drives itself once `enterPhase3` has
 * started it, and the outside only has to keep calling `progress`.  Both calls
 * are INLINED in the object and are still calls; see `progress`.
 *
 * `initiateRRN` and `initiateFPE` are the two requests that go the other way:
 * they take a modulator that has REACHED the data phase back to phase 4 for a
 * rate renegotiation, and refuse from anywhere else.  Nothing in the object
 * calls either, so they are the class's outward-facing surface along with
 * `progress` itself.
 *
 * The five `exit*` members are a different shape: each is a REQUEST that is
 * ignored unless the sub-modulator it forwards to is in the one state that
 * member is about, so all five are safe to call at any time.  Four go to the
 * phase 3 modulator on states 4, 6, 9 and 12; `exitCPt` goes to the phase 4
 * modulator on state 0.  Every one of them clears `word_34` on the way out.
 *
 * WHY THE SUB-OBJECTS ARE BUILT THROUGH asm() LABELS: the reason
 * src/pump/v90/V92Precoder.cpp gives in full -- `sysdep_malloc(n); ctor(p)`
 * with no null test between them is `new` over an inline `operator new`, this
 * build is -nostdinc++ with no <new>, and a placement form would add the null
 * test the blob does not have.
 *
 * AND WHY ONE OF THE SIX IS `delete` AFTER ALL.  `ResamplerTimingOffset` has
 * a virtual destructor and inherits `Resampler`'s member `operator delete`,
 * so the blob releases it with `mov (%edx),%eax; call *0x4(%eax)` -- vtable
 * slot one, the deleting destructor, and no `sysdep_free` of its own.  The
 * only C++ that emits that is `delete p`, and it costs no libstdc++ because
 * the `operator delete` it reaches is the member one.  The other five have no
 * `operator delete` of their own, so `delete` on them would reference the
 * global form and break every test binary's link.
 *
 * Plain cdecl, `this` first on the stack -- `mov 0x50(%esp),%esi` after four
 * pushes and a 0x3c-byte frame -- finding 215.
 */

#include <stddef.h>

extern "C" {
#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/vpcm_tables.h"
}

#include "dsplib/V92Modulator.h"
#include "dsplib/V92BitsToSymbol.h"
#include "dsplib/V92CP.h"
#include "dsplib/V92ModulusEncoder.h"
#include "dsplib/V92Parameters.h"
#include "dsplib/V92Phase2Info.h"
#include "dsplib/V92Phase3Modulator.h"
#include "dsplib/V92Phase4Modulator.h"
#include "dsplib/V92Transmitter.h"
#include "dsplib/ResamplerTimingOffset.h"
#include "dsplib/Queue.h"
#include "dsplib/FloatFIR.h"

extern "C" {
void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);

/* The six complete-object constructors, by the names the relocations carry. */
void v92mod_bts_ctor(void *self, unsigned int n, void *params)
	asm("_ZN15V92BitsToSymbolC1EjP13V92Parameters");
void v92mod_rto_ctor(void *self, unsigned int phases, float ppmScale,
		     unsigned int taps, float cutoff, float ppm,
		     unsigned int minHistory)
	asm("_ZN21ResamplerTimingOffsetC1Ejfjffj");
void v92mod_p3m_ctor(void *self, void *params)
	asm("_ZN18V92Phase3ModulatorC1EP13V92Parameters");
void v92mod_p4m_ctor(void *self, void *params, void *bitsToSymbol, void *cp,
		     void *mappingParams)
	asm("_ZN18V92Phase4ModulatorC1EP13V92ParametersP15V92BitsToSymbolP5V92C"
	    "PP16V92MappingParams");
void v92mod_queue_ctor(void *self, unsigned int n) asm("_ZN5QueueIfEC1Ej");
void v92mod_fir_ctor(void *self, unsigned int nTaps, float *coef,
		     unsigned int blockSize) asm("_ZN8FloatFIRC1EjPfj");
}

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V92MOD_OFF(field, off, tag) \
	typedef char v92mod_off_##tag[ \
	    ((int)__builtin_offsetof(V92Modulator, field) == (off)) ? 1 : -1]

V92MOD_OFF(blockSize,		0x00, blocksize);
V92MOD_OFF(queuePrime,		0x04, queueprime);
V92MOD_OFF(blockRemaining,	0x08, blockremaining);
V92MOD_OFF(byte_0c,		0x0c, byte0c);
V92MOD_OFF(byte_0d,		0x0d, byte0d);
V92MOD_OFF(phase2Info,		0x10, phase2info);
V92MOD_OFF(ja,			0x14, ja);
V92MOD_OFF(dil,			0x18, dil);
V92MOD_OFF(mappingParams,	0x1c, mappingparams);
V92MOD_OFF(cp,			0x20, cp);
V92MOD_OFF(resamplerPhaseOffset,	0x24, phaseoffset);
V92MOD_OFF(float_28,		0x28, float28);
V92MOD_OFF(phase,		0x2c, phase);
V92MOD_OFF(word_30,		0x30, word30);
V92MOD_OFF(word_34,		0x34, word34);
V92MOD_OFF(resamplerPhaseChange,	0x38, phasechange);
V92MOD_OFF(resamplerPhaseChangeAt,	0x3c, phasechangeat);
V92MOD_OFF(params,		0x40, params);
V92MOD_OFF(phase3Modulator,	0x44, phase3);
V92MOD_OFF(phase4Modulator,	0x48, phase4);
V92MOD_OFF(bitsToSymbol,	0x4c, bitstosymbol);
V92MOD_OFF(resampler,		0x50, resampler);
V92MOD_OFF(scrambler,		0x54, scrambler);
V92MOD_OFF(queue,		0x74, queue);
V92MOD_OFF(txFilter,		0x78, txfilter);
V92MOD_OFF(buf_7c,		0x7c, buf7c);
V92MOD_OFF(resampleIn,		0x80, resamplein);
V92MOD_OFF(resampleOut,		0x84, resampleout);
V92MOD_OFF(buf_88,		0x88, buf88);
V92MOD_OFF(resampleTail,		0x8c, resampletail);

typedef char v92mod_size[(sizeof(V92Modulator) == 0x90) ? 1 : -1];

/*
 * The six `sizeof`s the constructor allocates with are the six classes' own,
 * every one already pinned by its own header from its own allocation site.
 * Asserting them here is what makes each `sysdep_malloc(sizeof(X))` below a
 * transcription of the blob's literal rather than a hope.
 */
typedef char v92mod_bts_size[(sizeof(V92BitsToSymbol) == 0x20) ? 1 : -1];
typedef char v92mod_rto_size[
	(sizeof(ResamplerTimingOffset) == 0x4c) ? 1 : -1];
typedef char v92mod_p3m_size[(sizeof(V92Phase3Modulator) == 0x50) ? 1 : -1];
typedef char v92mod_p4m_size[(sizeof(V92Phase4Modulator) == 0x1cc) ? 1 : -1];
typedef char v92mod_queue_size[(sizeof(Queue<float>) == 0x14) ? 1 : -1];
typedef char v92mod_fir_size[(sizeof(FloatFIR) == 0x14) ? 1 : -1];
typedef char v92mod_scram_size[
	(sizeof(Scrambler<int, unsigned char>) == 0x20) ? 1 : -1];

#endif /* 32-bit */

/*
 * ===========================================================================
 * V92Modulator::reset (.text+0x15060, 186 bytes), which the constructor
 * inlines and which is also its own symbol.
 *
 * The whole thing, with nothing elided and in the object's order:
 *
 *     if (dsplibs_debug_level > 1) printf("V92Modulator reset\r\n")
 *     scrambler.reset(0)
 *     +0x2c = 0 ; +0x30 = 0 ; +0x08 = +0x00 ; +0x34 = 0 ; +0x38 = 0
 *     queue->reset()
 *     +0x0c = 0 ; +0x0d = 0                     (both BYTE stores)
 *     +0x28 = 0.0f
 *     +0x04 = params->MODULATOR_QUEUE_LENGTH >> 1
 *     for (i = 0; i < +0x04; i++) queue->write(0.0f)
 *     txFilter->reset()
 *
 * THE SHIFT IS ARITHMETIC AND THE LOOP'S COMPARISON IS UNSIGNED.  `sar $1`
 * with no sign fixup is what GCC emits for `>> 1` over a SIGNED int, and the
 * parameter really is one -- `MODULATOR_QUEUE_LENGTH` is `int` in
 * V92Parameters.h, from a block of 54 four-byte slots.  The loop that follows
 * closes with `cmp %ebx,0x4(%esi); ja`, which is UNSIGNED, so the field it
 * lands in is unsigned and a negative parameter would prime the queue about
 * two billion times.  Finding 1284; the fixture keeps the parameter small and
 * positive for exactly that reason.
 *
 * The bound and the queue pointer are both re-read from the object on every
 * iteration, which is what the object does and what a member access through
 * `this` compiles to when the call between them might alias.
 *
 * `phase` GOES TO ZERO, WHICH IS NOT ONE OF ITS THREE NAMED VALUES, so after a
 * reset all three `enter*` members will act -- their early return tests for
 * their own value, not for "already entered".
 *
 * THE CONSTRUCTOR CALLS THIS and GCC 3.4.2 at -O3 inlines it, which is what
 * the object holds: the standalone symbol at +0x15060 and the constructor's
 * closing 152 bytes are the same statements in the same order (finding 1283).
 * ===========================================================================
 */
void
V92Modulator::reset()
{
	unsigned int i;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modulator reset\r\n");

	scrambler.reset(0);
	phase = 0;
	word_30 = 0;
	blockRemaining = blockSize;
	word_34 = 0;
	resamplerPhaseChange = V92MOD_PHASECHG_NONE;
	queue->reset();
	byte_0c = 0;
	byte_0d = 0;
	float_28 = 0.0f;
	queuePrime = (unsigned int)(params->MODULATOR_QUEUE_LENGTH >> 1);

	for (i = 0; i < queuePrime; i++)
		queue->write(0.0f);

	txFilter->reset();
}

/*
 * ===========================================================================
 * V92Modulator::V92Modulator (.text+0x15120 / +0x15400, 734 bytes)
 *
 * The scrambler in the member-initialiser list, then a debug line, then the
 * seven arguments filed away, then eleven allocations, then the inlined
 * `reset`.
 *
 * `blockSize` IS COMPUTED IN FLOATING POINT AND TRUNCATED.  The object does
 * `fildll` over a zero-extended 64-bit push of the argument -- the unsigned
 * to float conversion -- then `fmuls 0x3f555555`, `fadds 0.5f`, and a
 * `fistpll` with the x87 control word forced to round-toward-zero whose low
 * half is kept.  That last pair is GCC's float-to-UNSIGNED sequence, so both
 * ends of the conversion are unsigned and neither is a guess.  There is no
 * intermediate store, so the multiply and the add happen in the x87's
 * extended precision and the `f` suffixes on the two constants control only
 * which values are loaded.
 *
 * `3 * blockSize` for the bit-to-symbol stage is `lea (%eax,%eax,2)` over the
 * value RE-READ from +0x00, and the two buffers at +0x84 and +0x8c are sized
 * from the ARGUMENT rather than from `blockSize`.  Getting those two the same
 * way round is the difference between a graph that is right and one that is
 * plausible.
 *
 * NOT ONE OF THE ELEVEN ALLOCATIONS IS CHECKED, the blob's included.
 * ===========================================================================
 */
V92Modulator::V92Modulator(unsigned int nSamples, V92Phase2Info *p2,
			   V92Ja *j, tagV90DILdescriptor *d, V92CP *c,
			   V92MappingParams *mp, V92Parameters *pp)
	: scrambler(V92MOD_SCRAM_TAP1, V92MOD_SCRAM_TAP2, V92MOD_SCRAM_SLACK)
{
	void *p;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modulator constraction\r\n");

	phase2Info = p2;
	blockSize = (unsigned int)(nSamples
				   * (V92MOD_RATE_NUM / V92MOD_RATE_DEN)
				   + 0.5f);
	ja = j;
	cp = c;
	dil = d;
	params = pp;
	mappingParams = mp;

	buf_7c = (short *)sysdep_malloc((blockSize + V92MOD_BUF_SLACK)
					* sizeof(short));
	resampleIn = (float *)sysdep_malloc((blockSize + V92MOD_BUF_SLACK)
					    * sizeof(float));
	buf_88 = (unsigned char *)sysdep_malloc(blockSize * 8);
	resampleOut = (float *)sysdep_malloc((nSamples + V92MOD_BUF_SLACK)
					     * sizeof(float));
	resampleTail = (float *)sysdep_malloc((nSamples + V92MOD_BUF_SLACK)
					      * sizeof(float));

	p = sysdep_malloc(sizeof(V92BitsToSymbol));
	v92mod_bts_ctor(p, 3 * blockSize, params);
	bitsToSymbol = (V92BitsToSymbol *)p;

	p = sysdep_malloc(sizeof(ResamplerTimingOffset));
	v92mod_rto_ctor(p, V92MOD_RS_PHASES, V92MOD_RS_PPMSCALE,
			V92MOD_RS_TAPS, V92MOD_RS_CUTOFF, V92MOD_RS_PPM,
			V92MOD_RS_MINHISTORY);
	resampler = (ResamplerTimingOffset *)p;

	p = sysdep_malloc(sizeof(V92Phase3Modulator));
	v92mod_p3m_ctor(p, params);
	phase3Modulator = (V92Phase3Modulator *)p;

	p = sysdep_malloc(sizeof(V92Phase4Modulator));
	v92mod_p4m_ctor(p, params, bitsToSymbol, cp, mappingParams);
	phase4Modulator = (V92Phase4Modulator *)p;

	p = sysdep_malloc(sizeof(Queue<float>));
	v92mod_queue_ctor(p, (unsigned int)params->MODULATOR_QUEUE_LENGTH);
	queue = (Queue<float> *)p;

	p = sysdep_malloc(sizeof(FloatFIR));
	v92mod_fir_ctor(p, V92_TXPREFILTER_TAPS, v92TxPreFilter,
			V92MOD_FIR_BLOCK);
	txFilter = (FloatFIR *)p;

	reset();
}

/*
 * ===========================================================================
 * V92Modulator::~V92Modulator (.text+0x14050 / +0x14250, 503 bytes)
 *
 * Eleven null-guarded releases and then the scrambler, which is the
 * compiler's implicit member destruction and not a statement -- finding
 * 1256's reading, and the same one V92Phase4Modulator's destructor gets.
 *
 * THE SIX OBJECTS COME BACK IN A DIFFERENT ORDER FROM THE ONE THEY WERE BUILT
 * IN, AND THE FIVE RAW BUFFERS DO NOT.  The constructor allocates the buffers
 * +0x7c, +0x80, +0x88, +0x84, +0x8c and the destructor frees them +0x7c,
 * +0x80, +0x88, +0x84, +0x8c -- the same, +0x88 before +0x84 in both.  The
 * objects are built bit-to-symbol, resampler, phase 3, phase 4, queue, filter
 * and released phase 3, phase 4, bit-to-symbol, resampler, queue, filter.
 * Reproduced as found; nothing in the test tier sequences a free, so this is
 * the disassembly's word and not a measurement.
 *
 * NOTHING IS NULLED after its free, so a second destruction double-frees all
 * eleven; see docs/deviations.md D212.
 * ===========================================================================
 */
V92Modulator::~V92Modulator()
{
	if (phase3Modulator != 0) {
		phase3Modulator->~V92Phase3Modulator();
		sysdep_free(phase3Modulator);
	}
	if (phase4Modulator != 0) {
		phase4Modulator->~V92Phase4Modulator();
		sysdep_free(phase4Modulator);
	}
	if (bitsToSymbol != 0) {
		bitsToSymbol->~V92BitsToSymbol();
		sysdep_free(bitsToSymbol);
	}
	if (resampler != 0)
		delete resampler;
	if (queue != 0) {
		queue->~Queue();
		sysdep_free(queue);
	}
	if (txFilter != 0) {
		txFilter->~FloatFIR();
		sysdep_free(txFilter);
	}
	if (buf_7c != 0)
		sysdep_free(buf_7c);
	if (resampleIn != 0)
		sysdep_free(resampleIn);
	if (buf_88 != 0)
		sysdep_free(buf_88);
	if (resampleOut != 0)
		sysdep_free(resampleOut);
	if (resampleTail != 0)
		sysdep_free(resampleTail);
}

/*
 * ===========================================================================
 * V92Modulator::getV92TxFilterDelay (.text+0x14450, 21 bytes)
 *
 * The delay the transmit shaping filter adds, in samples, or zero when the
 * filter is switched off.  Twenty-one bytes and no branch: the object does
 * `cmp $0x1,%eax; sbb %eax,%eax; not %eax; and $0x12,%eax`, which is GCC's
 * branchless spelling of `x ? 18 : 0` -- `sbb` after `cmp $1` leaves -1
 * exactly when the value is zero.
 *
 * The 18 is a literal in the object.  V92Modulator.h says why it is spelled
 * `V92MOD_TX_FILTER_DELAY` and why that name is inference.
 * ===========================================================================
 */
int
V92Modulator::getV92TxFilterDelay() const
{
	return params->V92_APPLY_TX_SHAPING_FILTER ? V92MOD_TX_FILTER_DELAY : 0;
}

/*
 * ===========================================================================
 * V92Modulator::enterPhase3 (.text+0x14470, 126 bytes)
 *
 * Enter phase 3: reset the phase 3 modulator to its first state and take the
 * phase counter to 1.  A no-op if `phase` is already 1, which is the object's
 * `cmpl $0x1,0x2c(%ebx); je` at the very top -- before the diagnostic, so a
 * second call prints nothing either.
 *
 * THE SIX ARGUMENTS ARE THE OBJECT'S.  4000 is the phase 3 modulator's dead
 * first parameter (V92Phase3Modulator.h says why it is kept); state 0 is `Ru`;
 * the third argument and the symbol count are zero; `ja` and `dil` are handed
 * on unchanged; and the last is `phase2Info->rtd`, which `enterPhase4` passes
 * to the phase 4 modulator's `reset` in the same slot.
 *
 * `word_30` AND `word_34` ARE CLEARED AFTER THE CALL, not before, and
 * `resamplerPhaseChange` with them -- so a phase change staged by `progress`
 * and not yet applied is dropped by entering phase 3.
 * ===========================================================================
 */
void
V92Modulator::enterPhase3()
{
	if (phase == V92MOD_PHASE_3)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modulator enter Phase 3\r\n");

	phase3Modulator->reset(4000, V92P3M_STATE_RU, 0, ja, dil,
			       (unsigned int)phase2Info->rtd);
	phase = V92MOD_PHASE_3;
	word_30 = 0;
	word_34 = 0;
	resamplerPhaseChange = V92MOD_PHASECHG_NONE;
}

/*
 * ===========================================================================
 * V92Modulator::enterPhase4 (.text+0x148b0, 113 bytes)
 *
 * `enterPhase3`'s shape with the PHASE 4 modulator as its subject and one
 * statement fewer: no `resamplerPhaseChange` clear.  A no-op if `phase` is
 * already 2, and the guard is again before the diagnostic.
 *
 * THE FIVE ARGUMENTS ARE THE OBJECT'S.  4000 is the amplitude; `byte_0c` is
 * the bit count, zero-extended (`movzbl 0xc(%ebx)`); the state is a literal
 * ZERO, which is one of the fifteen `V92Phase4Modulator` codes nothing names
 * and is why `V92Phase4ModulatorState` carries that one enumerator and no
 * others; the symbol count is zero; and the last is `phase2Info->rtd` in the
 * same slot `enterPhase3` puts it in.
 *
 * IT IS `byte_0c` HERE AND `byte_0d` IN THE TWO `initiate` MEMBERS, which is
 * the only thing separating the three call sites' argument lists.  Neither
 * byte is written by anything but `reset`, which clears both.
 *
 * THIS BODY IS ALSO INSIDE `progress`, and as a CALL that GCC inlined rather
 * than as a repeated statement -- .text+0x14e78..+0x14ecb is this function
 * with the same guard, the same message and the same five arguments, and the
 * guard's early exit lands on the join of the statement that FOLLOWS the call.
 * See `progress`.
 * ===========================================================================
 */
void
V92Modulator::enterPhase4()
{
	if (phase == V92MOD_PHASE_4)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modulator: enter Phase 4\r\n");

	phase4Modulator->reset(4000, byte_0c, V92P4M_RESET_STATE_ZERO, 0,
			       (unsigned int)phase2Info->rtd);
	phase = V92MOD_PHASE_4;
	word_30 = 0;
	word_34 = 0;
}

/*
 * ===========================================================================
 * V92Modulator::enterDataPhase (.text+0x14930, 69 bytes)
 *
 * Enter the data phase: tell the bit-to-symbol stage how many symbols a block
 * holds and take the phase counter to 3.  A no-op if `phase` is already 3.
 *
 * ITS DIAGNOSTIC IS NOT GATED AT THIS LEVEL, AND WHAT THAT COSTS IS NOT A LINE.
 * The other nine members here test `dsplibs_debug_level > 1` before they print;
 * this one calls `edprintf` unconditionally, and the object has no
 * `cmpl $0x1,0x0` anywhere in its 69 bytes, so the difference is the author's.
 * `edprintf` FORMATS AND ENCODES whatever the level is -- resetting and then
 * moving its shared key -- and gates only the `dsplibs_debug_printf` at its end,
 * at the same `> 1` (src/core/encode.c).  So the transcript is silent below
 * level 2 exactly as the other nine are, and the visible effect of the missing
 * gate is on the key a later `cEncodeChar` caller would see.
 *
 * `word_34` GOES TO 10 AND NOT TO ZERO, the only member written that stores it
 * anything but zero.  What the code means belongs to `progress`.
 *
 * THE BLOCK SIZE IS RE-READ FROM THE OBJECT (`mov (%ebx),%edx`) rather than
 * from `blockRemaining`, so it is the constructor's derived figure and not
 * whatever is left of the current block.
 * ===========================================================================
 */
void
V92Modulator::enterDataPhase()
{
	if (phase == V92MOD_PHASE_DATA)
		return;

	edprintf("V92Modulator:enter  Data Phase:\r\n");

	phase = V92MOD_PHASE_DATA;
	word_30 = 0;
	word_34 = 10;
	bitsToSymbol->setSymbolsBlockSize(blockSize);
}

/*
 * ===========================================================================
 * V92Modulator::initiateRRN (.text+0x14680, 260 bytes)
 * V92Modulator::initiateFPE (.text+0x14790, 276 bytes)
 *
 * THE TWO V.92 UPSTREAM RENEGOTIATION REQUESTS, and they are the same function
 * with four differences.  Both are ASKED FOR FROM OUTSIDE -- nothing in the
 * object calls either -- and both refuse unless the modulator is in the data
 * phase, which is the whole of the "approved" in their own messages:
 *
 *     if (phase != 3) { print "... requested but NOT approved"; return -1; }
 *     print "... requested, enter Phase 4"
 *     phase = 2 ; word_30 = 0
 *     bitsToSymbol->setSymbolsBlockSize(1)
 *     <FPE only: the modulus encoder's selector goes to 1>
 *     state = bitsToSymbol->nofBitsForNextTime() ? <signal> : <data-to-signal>
 *     phase4Modulator->reset(4000, byte_0d, state, 0, phase2Info->rtd)
 *     word_34 = 0
 *     phase4Modulator->resetBefor<RRN|FPE>()
 *     cp->byte_04 = 0
 *     cp->infoToBits()
 *     return 0
 *
 * THE FOUR DIFFERENCES, and nothing else in 536 bytes separates them:
 *
 *   1. the two state codes -- 18/19 for RRN, 25/26 for FPE
 *   2. `resetBeforRRN` against `resetBeforFPE`
 *   3. FPE writes `bitsToSymbol->transmitter->modulusEncoder->field_50 = 1`
 *      and RRN does not, three levels down through pointers
 *   4. the four messages
 *
 * The third is the one no fixture sees by accident: it is a store into a
 * fourth-level sub-object, so a suite that compares only the modulator and its
 * own pieces scores the two bodies as interchangeable.  t_v92modstate.cpp
 * compares the modulus encoder for that reason and the mutation set swaps the
 * two bodies to prove it.
 *
 * THEY TAKE THE PHASE TO 4 WITHOUT CALLING `enterPhase4`, and that is measured
 * rather than a reading: `enterPhase4` would have reset the phase 4 modulator
 * with `byte_0c` and state ZERO, and these reset it with `byte_0d` and a state
 * chosen from the bit count.  The three stores that follow are the same three
 * that function ends with, in the same order, and that is all they share.
 *
 * THE RETURN IS A STATUS AND `int` IS MEASURED -- see V92Modulator.h.  `0` is
 * `xor %eax,%eax` at +0x14736 and +0x14852; `-1` is a full
 * `mov $0xffffffff,%eax`, twice in each function because the debug gate splits
 * the refusal path.
 *
 * WHICH WAY ROUND THE BIT COUNT GOES, and it is the opposite of the obvious
 * reading.  `setSymbolsBlockSize(1)` has just set the block to ONE symbol, so
 * `nofBitsForNextTime` returns zero exactly when the bit-to-symbol stage
 * already has that symbol banked -- `symbolsDone >= symbolsBlockSize`.
 *
 *     != 0   nothing banked   ->  RuModulation / RmModulation, state 19 / 26
 *     == 0   data in flight   ->  DataToRuModulation / DataToRmModulation,
 *                                 state 18 / 25
 *
 * so a NON-ZERO count starts the renegotiation signal at once and a ZERO one
 * enters the state that finishes the data first.  The names are the object's
 * own, from the four messages; which arm each belongs to is the `jne` at
 * .text+0x146cf and +0x147eb.  The fixture drives both, and got this backwards
 * first time round.
 * ===========================================================================
 */
int
V92Modulator::initiateRRN()
{
	int state;

	if (phase != V92MOD_PHASE_DATA) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V92Modulator: RRN requested but NOT approved\r\n");
		return -1;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V92Modulator: RRN requested, enter Phase 4\r\n");

	phase = V92MOD_PHASE_4;
	word_30 = 0;
	bitsToSymbol->setSymbolsBlockSize(1);

	if (bitsToSymbol->nofBitsForNextTime() != 0) {
		edprintf("V92Modulator: Phase4Modulator state initialized to "
			 "RuModulation\r\n");
		state = V92P4M_STATE_RU;
	} else {
		edprintf("V92Modulator: Phase4Modulator state initialized to "
			 "DataToRuModulation\r\n");
		state = V92P4M_STATE_DATA_TO_RU;
	}

	phase4Modulator->reset(4000, byte_0d, (V92Phase4ModulatorState)state, 0,
			       (unsigned int)phase2Info->rtd);
	word_34 = 0;
	phase4Modulator->resetBeforRRN();
	cp->byte_04 = 0;
	cp->infoToBits();

	return 0;
}

int
V92Modulator::initiateFPE()
{
	int state;

	if (phase != V92MOD_PHASE_DATA) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V92Modulator: FPE requested but NOT approved\r\n");
		return -1;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V92Modulator: FPE requested, enter Phase 4\r\n");

	phase = V92MOD_PHASE_4;
	word_30 = 0;
	bitsToSymbol->setSymbolsBlockSize(1);

	/*
	 * THE ONE STORE THAT IS NOT IN `initiateRRN`, and it goes three levels
	 * down: `mov 0x4c(%ebx),%edx; mov (%edx),%ecx; mov 0x48(%ecx),%eax;
	 * movl $0x1,0x50(%eax)` at .text+0x147d2.  +0x50 is
	 * `V92ModulusEncoder::progress`'s own selector, so this switches the
	 * encoder into the mode the FPE signal needs before a symbol is asked
	 * for.  It sits BETWEEN the block-size call and the bit-count call,
	 * which is where the object puts it.
	 */
	bitsToSymbol->transmitter->modulusEncoder->field_50 = 1;

	if (bitsToSymbol->nofBitsForNextTime() != 0) {
		edprintf("V92Modulator: Phase4Modulator state initialized to "
			 "RmModulation\r\n");
		state = V92P4M_STATE_RM;
	} else {
		edprintf("V92Modulator: Phase4Modulator state initialized to "
			 "DataToRmModulation\r\n");
		state = V92P4M_STATE_DATA_TO_RM;
	}

	phase4Modulator->reset(4000, byte_0d, (V92Phase4ModulatorState)state, 0,
			       (unsigned int)phase2Info->rtd);
	word_34 = 0;
	phase4Modulator->resetBeforFPE();
	cp->byte_04 = 0;
	cp->infoToBits();

	return 0;
}

/*
 * ===========================================================================
 * The four phase 3 exits (.text+0x144f0, +0x14540, +0x14590, +0x145e0),
 * 68 bytes each.
 *
 * One shape, four times, with one state code and one callee different in each:
 *
 *     if (phase3Modulator->state != <code>) return;
 *     if (dsplibs_debug_level > 1) printf(<message>);
 *     phase3Modulator-><exit>();
 *     word_34 = 0;
 *
 * The guard is READ FROM THE SUB-OBJECT, so these are requests rather than
 * commands: three of the four are safe to call in any state and do nothing.
 * The phase 3 modulator's own exit members guard again on the same code
 * (V92Phase3Modulator.h's state table), so the test is duplicated on purpose
 * -- what this layer adds is the `word_34 = 0`, which only happens when the
 * transition really is taken.
 *
 * FOUR SEPARATE BODIES AND NOT A HELPER.  Each is its own blob symbol at its
 * own address; factoring them into one static would leave four symbols with no
 * bytes of their own and make every per-function count meaningless (findings
 * 605, 610).  The duplication is the object's.
 * ===========================================================================
 */
void
V92Modulator::exitJa()
{
	if (phase3Modulator->state != V92P3M_STATE_JA)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modulator: exit Ja\r\n");

	phase3Modulator->exitJa();
	word_34 = 0;
}

void
V92Modulator::exitSilence()
{
	if (phase3Modulator->state != V92P3M_STATE_SILENCE)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modulator: exit Silence\r\n");

	phase3Modulator->exitSilence();
	word_34 = 0;
}

void
V92Modulator::exitSuSecond()
{
	if (phase3Modulator->state != V92P3M_STATE_SU_SECOND)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modulator: exit SuSecond\r\n");

	phase3Modulator->exitSuSecond();
	word_34 = 0;
}

/*
 * The member is `exitTRN1uSecond` and the callee is `exitTRN1u`: the phase 3
 * modulator has one exit for both TRN1u segments and acts on the SECOND one
 * (state 12) only.  Both names are the mangling's.
 */
void
V92Modulator::exitTRN1uSecond()
{
	if (phase3Modulator->state != V92P3M_STATE_TRN1U_SECOND)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modulator: exit TRN1uSecond\r\n");

	phase3Modulator->exitTRN1u();
	word_34 = 0;
}

/*
 * ===========================================================================
 * V92Modulator::exitCPt (.text+0x14630, 80 bytes)
 *
 * The same shape as the four above with the PHASE 4 modulator as its subject,
 * and twelve bytes longer because its guard loads through a second pointer.
 * State 0 is one of the fifteen `V92Phase4Modulator` codes nothing names, so
 * the literal stays a literal -- V92Phase4Modulator.h's rule, and a guessed
 * enumerator here would be exactly the wrong name no test can fail on.
 * ===========================================================================
 */
void
V92Modulator::exitCPt()
{
	if (phase4Modulator->state != 0)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modulator: exit CPt\r\n");

	phase4Modulator->exitCPt();
	word_34 = 0;
}

/*
 * The float-as-%c%d.%05d idiom, the same three helpers
 * src/pump/v90/V92Transmitter.cpp and src/pump/v90/V92EchoCanceller.cpp carry.
 * The scale is 1e5 here: `.rodata.cst4+0xb8` is 100000.0 and the format string
 * says %05d.
 *
 * `frac_of`'s subtraction is `v - (int)v`, which is what `de e1` at
 * .text+0x14bb6 does -- objdump prints it `fsubp %st,%st(1)` and it IS FSUBRP,
 * so st(1) becomes st(0) - st(1) and st(0) holds the value (finding 245).  The
 * abs() makes the operand order unobservable either way (finding 256).
 *
 * The `long double` casts are the x87's: the object loads the field once and
 * never stores the intermediate, so the subtraction and the multiply both
 * happen in extended precision.
 *
 * `sign_of` puts ZERO ON THE LEFT -- `fldz; fcomps 0x24(%ebx)` at
 * .text+0x14bcc -- so a phase offset of exactly 0.0f prints '-'.
 */
static char
sign_of(float v)
{
	return (0.0f < v) ? '+' : '-';
}

static int
whole_of(float v)
{
	return (int)__builtin_fabsf(v);
}

static int
frac_of(float v)
{
	return __builtin_abs((int)(((long double)v - (long double)(int)v)
				   * V92MOD_PHASE_PRINT_SCALE));
}

/*
 * ===========================================================================
 * V92Modulator::mkResampledSignal (.text+0x14980, 662 bytes)
 *
 * Run `blockRemaining` samples of `resampleIn` through the resampler into
 * `resampleOut`, and report how many came out.  Three quarters of the function
 * exists for the ONE case that is not that: a resampler phase change that has
 * to take effect part way through a block.
 *
 * THE SIMPLE PATH, which is both the not-in-phase-3 case and the
 * nothing-pending case:
 *
 *     resampler->resample(resampleIn, blockRemaining, resampleOut, n)
 *
 * and nothing else -- not even a store to `n`, which the resampler's own
 * reference parameter fills.
 *
 * THE SPLIT PATH runs when `phase == 1` AND a change is staged.  The block is
 * resampled in two pieces around the change:
 *
 *     resample(resampleIn,        changeAt,                 resampleOut, n1)
 *     <apply the phase change>
 *     resample(resampleIn + changeAt, blockRemaining - changeAt,
 *                                                          resampleTail, n2)
 *     <join resampleTail onto the end of resampleOut>
 *     resamplerPhaseChange = 0 ; resamplerPhaseChangeAt = 0
 *
 * and `blockRemaining - changeAt` is computed as written -- an unsigned
 * subtraction with no test, so a `changeAt` past the end of the block asks the
 * resampler for about four billion samples.  `progress` is the only writer of
 * the pair and is unwritten; nothing here bounds it.  D801.
 *
 * ---------------------------------------------------------------------------
 * THE JOIN IS TWO DIFFERENT LOOPS AND THE DIFFERENCE IS THE WHOLE POINT
 *
 * If the phase wrapped past 1.0 the second segment's FIRST output sample is
 * dropped and the joined length is `n1 + n2 - 1`; if it did not, every sample
 * is kept and the length is `n1 + n2`.  The object writes the length before
 * either loop runs and the two loops differ in exactly three places -- the
 * source index starts at 1 rather than 0, the trip count is `n2 - 1` rather
 * than `n2`, and the object spells the first one `mov 0x4(%esi,%ecx,4)`.
 *
 * A wrapped phase means the resampler has already advanced a whole output
 * sample beyond where the first segment left off, so the sample it would emit
 * first is the one the first segment's last output already covers.  That is
 * the reading; what is measured is the two loops.
 *
 * BOTH LOOPS COPY WITH A 32-BIT INTEGER MOVE (`mov (%esi,%ecx,4),%eax; mov
 * %eax,(%edx)`) and not through the x87, which is what GCC emits for a float
 * assignment it does not have to round.  Written as the float assignment it
 * is; the width is the same and the value is bit-exact either way.
 *
 * ---------------------------------------------------------------------------
 * THE PHASE CHANGE ITSELF, AND WHY IT IS A `switch` AND NOT AN `if/else`
 *
 *     HALF    phase = getNormalizedPhase() + 0.5f
 *     OFFSET  phase = getNormalizedPhase() + resamplerPhaseOffset
 *     both    if (phase >= 1.0f) { phase -= 1.0f; wrapped = 1; }
 *             setNormalizedPhase(phase)
 *
 * The object dispatches `cmp $0x2; je; dec %eax; je` and FALLS THROUGH to the
 * second resample when the code is neither -- so a `resamplerPhaseChange` that
 * is not 1 or 2 still splits the block and changes no phase at all, where an
 * `if (== 2) ... else ...` would treat every such value as the HALF arm.  Only
 * `progress` writes the field and it writes only 1 and 2, so no test can
 * separate the two readings; the dispatch is what says which is right.
 *
 * The two arms share the wrap and the store IN THE OBJECT and not in the
 * source: the OFFSET arm's tail jumps into the HALF arm's `fld1` at
 * .text+0x14ae9 from +0x14b70, which is the compiler cross-jumping two
 * identical tails.  Writing it as one shared tail after the switch would run
 * `setNormalizedPhase` on an uninitialised phase for every other code.
 *
 * `wrapped` IS ONE BYTE, and that is forced: `movb $0x0,0x1f(%esp)`,
 * `movb $0x1,0x1f(%esp)` and `cmpb $0x0,0x1f(%esp)` at .text+0x149de, +0x14af6
 * and +0x14a5d.  An `int` would be a register or a 32-bit slot.
 *
 * `resamplerPhaseOffset` IS READ AND NEVER WRITTEN by any of the class's
 * eighteen symbols -- V92Modulator.h and D800.  This is the site that reads
 * it.
 *
 * THE COMPARISON IS ORDERED AND THE OBJECT'S OPERAND ORDER IS `1.0f` FIRST:
 * `fld1; fcom %st(1); fnstsw; sahf; ja` takes the branch when 1.0 is above the
 * phase, so the arm that subtracts is the `else`.  Written as `>=` on the
 * phase, which is the same predicate; `-mno-ieee-fp` is what makes a single
 * `fcom` with no parity test reachable at all (finding 1990).
 *
 * ---------------------------------------------------------------------------
 * THE DIAGNOSTIC, which is a fixed-point print of a float and 80 of the 662
 * bytes
 *
 *     "V92Modulator: setPhase = 0.5"              -- the HALF arm, no
 *                                                    conversions at all
 *     "V92Modulator: setPhase = %c%d.%05d"        -- the OFFSET arm
 *
 * The second is the object printing a float without a `%f`: a sign character
 * from an ordered compare against zero, `(int)fabs(x)` for the whole part, and
 * `abs((int)((x - (int)x) * 100000.0f))` for five fractional digits.  Every one
 * of those conversions is a truncating `fistp` with the x87 control word
 * forced to round-toward-zero and restored after, which is what GCC emits for
 * a C cast to `int`, and `x - (int)x` keeps the sign of `x` so the `abs` is
 * load-bearing.  The sign is `0.0f < x` and NOT `x >= 0`: `fcomps 0x24(%ebx)`
 * puts zero on the left, so a phase offset of exactly zero prints '-'.
 * ===========================================================================
 */
void
V92Modulator::mkResampledSignal(unsigned int &n)
{
	unsigned int n1, n2, i;
	char wrapped;
	float phaseNow;

	if (phase != V92MOD_PHASE_3
	    || resamplerPhaseChange == V92MOD_PHASECHG_NONE) {
		resampler->resample(resampleIn, blockRemaining, resampleOut, n);
		return;
	}

	wrapped = 0;
	resampler->resample(resampleIn, resamplerPhaseChangeAt, resampleOut,
			    n1);

	switch (resamplerPhaseChange) {
	case V92MOD_PHASECHG_OFFSET:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V92Modulator: setPhase = %c%d.%05d\r\n",
			    sign_of(resamplerPhaseOffset),
			    whole_of(resamplerPhaseOffset),
			    frac_of(resamplerPhaseOffset));
		phaseNow = resampler->getNormalizedPhase()
			 + resamplerPhaseOffset;
		if (phaseNow >= 1.0f) {
			wrapped = 1;
			phaseNow -= 1.0f;
		}
		resampler->setNormalizedPhase(phaseNow);
		break;

	case V92MOD_PHASECHG_HALF:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V92Modulator: setPhase = 0.5\r\n");
		phaseNow = resampler->getNormalizedPhase()
			 + V92MOD_PHASECHG_HALF_STEP;
		if (phaseNow >= 1.0f) {
			wrapped = 1;
			phaseNow -= 1.0f;
		}
		resampler->setNormalizedPhase(phaseNow);
		break;
	}

	resampler->resample(resampleIn + resamplerPhaseChangeAt,
			    blockRemaining - resamplerPhaseChangeAt,
			    resampleTail, n2);

	if (wrapped) {
		n = n1 + n2 - 1;
		for (i = 0; i < n2 - 1; i++)
			resampleOut[n1 + i] = resampleTail[i + 1];
	} else {
		n = n1 + n2;
		for (i = 0; i < n2; i++)
			resampleOut[n1 + i] = resampleTail[i];
	}

	resamplerPhaseChange = V92MOD_PHASECHG_NONE;
	resamplerPhaseChangeAt = 0;
}

/*
 * ===========================================================================
 * V92Modulator::progress (.text+0x14c20, 1,075 bytes) -- ONE BLOCK OF UPSTREAM
 *
 * The function the other seventeen exist for.  Given `nSamples` of output to
 * fill, it works out how many SYMBOLS that is, generates them from whichever
 * sub-modulator the phase selects, resamples them up to the sample rate,
 * pushes them through the queue and hands `nSamples` back out of it.
 *
 * The four arguments are the mangling's:
 *
 *     bits     the data-phase arm's input words, `Scrambler<int,h>::process`'s
 *              `const int *`
 *     nbits    IN for the data phase, OUT for everything else, and the one
 *              thing the caller is told besides `word_34`
 *     out      where `nSamples` floats are handed back
 *     nSamples how many of them, and the ONLY thing the symbol count is
 *              derived from
 *
 * ---------------------------------------------------------------------------
 * THE SYMBOL COUNT IS THE CONSTRUCTOR'S EXPRESSION AGAIN
 *
 *     n = (unsigned)(nSamples * 5/6 + 0.5f)
 *
 * -- the same `fildll` / `fmuls 0x3f555555` / `fadds 0.5f` / truncating
 * `fistpll` the constructor uses for `blockSize`, from its own slot of the
 * constant pool (.rodata.cst4+0xc0 rather than +0xc8; three copies of the pair
 * are in the section).  So `progress` re-derives per call what the constructor
 * derived once, and the two agree only when the caller asks for the size the
 * modulator was built for.
 *
 * ---------------------------------------------------------------------------
 * HOW MANY SYMBOLS TO MAKE: THE QUEUE DECIDES, EXCEPT AT PHASE ZERO
 *
 *     blockRemaining = queuePrime - queue->count() + n
 *
 * `queuePrime` is the level `reset` primes the queue to, so the expression is
 * "what it takes to put the queue back where it started, plus this block".
 * It is computed BEFORE the dispatch and on every path -- the object's `div`
 * at +0x14c92 is above the switch, and GCC does not speculate a division past
 * a branch, so the source computes it unconditionally and that is why it is a
 * local here.
 *
 * PHASE ZERO IS THE EXCEPTION and uses `n` flat, then fills the symbol block
 * with silence.  Straight after a `reset` the queue is at exactly `queuePrime`
 * and the two expressions agree, which is what makes that arm invisible to a
 * fixture that only ever calls `progress` once -- see t_v92modstate.cpp.
 *
 * ---------------------------------------------------------------------------
 * THE FIVE ARMS
 *
 *   0  silence: `buf_7c[0 .. blockRemaining) = 0`, and `nbits = 0`.
 *
 *   1  phase 3, and the arm re-tests `phase` ON EVERY SYMBOL -- the object
 *      reloads +0x2c at the top of each iteration (+0x14e3b) because the
 *      body can move it.  Each symbol latches `phase3Modulator->eventCode`
 *      into `word_34` and then acts on three of its codes:
 *
 *          5  entering SuSecond      -> stage a HALF-sample phase change
 *          7  entering TRN1uSecond   -> stage the OFFSET phase change
 *          8  entering End           -> enterPhase4()
 *
 *      and the sample index the code arrived at is what `resamplerPhaseChangeAt`
 *      is set to, so `mkResampledSignal` splits the block exactly there.  That
 *      is the whole reason that function has a split path.  The three codes are
 *      V92Phase3Modulator's alphabet and stay bare numbers here; its own
 *      `generateSymbol` writes them the same way.
 *
 *      Once code 8 has fired, `phase` is 2 and every REMAINING symbol of the
 *      same block comes from the phase 4 modulator instead -- which is what the
 *      per-symbol reload buys and why this is one loop and not two.
 *
 *   2  phase 4: symbols from the phase 4 modulator, latching its `word_0c`.
 *      Code 9 at the end of the block enters the data phase and then reports
 *      the bit count the next call has to bring; anything else reports zero.
 *
 *   3  data: scramble the caller's words into `buf_88` and let the
 *      bit-to-symbol stage turn them into `buf_7c`.  `nbits` is IN here and
 *      the callee writes it back out.
 *
 *   default: print, and fall into the tail with `nbits` untouched.
 *
 * ---------------------------------------------------------------------------
 * THE TAIL, WHICH EVERY ARM JOINS
 *
 *     resampleIn[i] = (float)buf_7c[i]                 for blockRemaining
 *     resampler->setTimingOffset(float_28)
 *     mkResampledSignal(nOut)
 *     if (params->V92_APPLY_TX_SHAPING_FILTER)
 *             txFilter->process(resampleOut, resampleOut, nOut)   in place
 *     queue->write(resampleOut, nOut)
 *     queue->read(out, nSamples)
 *     blockRemaining = queuePrime - queue->count() + blockSize
 *     if (phase == 3) nbits = bitsToSymbol->setSymbolsBlockSize(blockRemaining)
 *     if (queue->isEmpty() || queue->isFull()) { print; word_34 = 1; }
 *
 * THE SECOND OCCUPANCY EXPRESSION ADDS `blockSize` AND NOT `n`, from +0x00 and
 * not from the argument (`mov (%esi),%ebx` at +0x14d79).  The two differ
 * whenever the caller asks for a block the modulator was not built for, and
 * that difference is the object's.
 *
 * `float_28` REACHES `setTimingOffset(float ppm)`, which is the callee typing
 * it: the word `reset` clears is a timing offset in parts per million and the
 * resampler scales it by `ppmScale * 1e-6f`.  The name is left as the offset it
 * was -- three mutation entries and an anchor key on the current spelling and
 * nothing in these five functions needs it changed.
 *
 * ---------------------------------------------------------------------------
 * TWO CALLS THE OBJECT INLINED, AND THEY ARE CALLS
 *
 * `enterPhase4()` at +0x14e78 and `enterDataPhase()` at +0x14f2c are those
 * functions' bodies, guard included, with the guard's early exit landing on the
 * join of the NEXT statement rather than on a shared tail.  Both are their own
 * symbols elsewhere in the object, so GCC emitted each twice, and both are
 * written here as the calls they are.  Writing either out longhand would put a
 * second spelling of a phase transition in the tree and would not change a byte
 * of what the compiler emits.
 * ===========================================================================
 */
void
V92Modulator::progress(int *bits, unsigned int &nbits, float *out,
		       unsigned int nSamples)
{
	unsigned int n;
	unsigned int want;
	unsigned int nOut;
	unsigned int i;

	n = (unsigned int)(nSamples * (V92MOD_RATE_NUM / V92MOD_RATE_DEN)
			   + 0.5f);

	word_34 = 0;
	word_30 += n;

	want = queuePrime - queue->count() + n;

	switch (phase) {
	case V92MOD_PHASE_RESET:
		blockRemaining = n;
		for (i = 0; i < blockRemaining; i++)
			buf_7c[i] = 0;
		nbits = 0;
		break;

	case V92MOD_PHASE_3:
		blockRemaining = want;
		for (i = 0; i < blockRemaining; i++) {
			if (phase == V92MOD_PHASE_3) {
				buf_7c[i] = (short)
				    phase3Modulator->generateSymbol();
				if (phase3Modulator->eventCode != 0) {
					word_34 = phase3Modulator->eventCode;
					/* entering SuSecond */
					if (word_34 == 5) {
						resamplerPhaseChange =
						    V92MOD_PHASECHG_HALF;
						resamplerPhaseChangeAt = i;
					/* entering TRN1uSecond */
					} else if (word_34 == 7) {
						resamplerPhaseChange =
						    V92MOD_PHASECHG_OFFSET;
						resamplerPhaseChangeAt = i;
					}
				}
				/* entering End */
				if (word_34 == 8) {
					enterPhase4();
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "V92Modulator progress: "
						    "Enter phase4\r\n");
				}
			} else {
				buf_7c[i] = (short)
				    phase4Modulator->generateSymbol();
				if (phase4Modulator->word_0c != 0)
					word_34 = phase4Modulator->word_0c;
			}
		}
		nbits = 0;
		break;

	case V92MOD_PHASE_4:
		blockRemaining = want;
		for (i = 0; i < blockRemaining; i++) {
			buf_7c[i] = (short)
			    phase4Modulator->generateSymbol();
			if (phase4Modulator->word_0c != 0)
				word_34 = phase4Modulator->word_0c;
		}
		if (word_34 == 9) {
			enterDataPhase();
			nbits = bitsToSymbol->nofBitsForNextTime();
		} else {
			nbits = 0;
		}
		break;

	case V92MOD_PHASE_DATA:
		blockRemaining = want;
		scrambler.process(bits, buf_88, nbits);
		bitsToSymbol->process(buf_88, nbits, buf_7c);
		break;

	default:
		blockRemaining = want;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V92Modulator progress: Illegal state\r\n");
		break;
	}

	for (i = 0; i < blockRemaining; i++)
		resampleIn[i] = (float)buf_7c[i];

	resampler->setTimingOffset(float_28);
	mkResampledSignal(nOut);

	if (params->V92_APPLY_TX_SHAPING_FILTER)
		txFilter->process(resampleOut, resampleOut, nOut);

	queue->write(resampleOut, nOut);
	queue->read(out, nSamples);

	blockRemaining = queuePrime - queue->count() + blockSize;

	if (phase == V92MOD_PHASE_DATA)
		nbits = bitsToSymbol->setSymbolsBlockSize(blockRemaining);

	if (queue->isEmpty() || queue->isFull()) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V92Modulator: Queue is Empty/Full !!!\r\n");
		word_34 = V92MOD_STATUS_QUEUE_LIMIT;
	}
}

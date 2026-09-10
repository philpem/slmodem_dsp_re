/**
 * @file V92Modulator.h
 * @brief The V.92 upstream modulator: the object that owns the whole
 *        transmit graph.
 *
 * Reconstructed from dsplibs.o. The class has eighteen symbols and all
 * eighteen are written in src/pump/v90/V92Modulator.cpp -- the constructor
 * (C1 at .text+0x15120 and C2 at +0x15400, 734 bytes each), the destructor
 * (D2 at +0x14050 and D1 at +0x14250, 503 bytes each), `reset`, the four
 * `exit*` members, `enterPhase3`, `enterPhase4`, `enterDataPhase`, `exitCPt`,
 * `getV92TxFilterDelay`, `mkResampledSignal`, `initiateRRN`, `initiateFPE`
 * and `progress`, the last of which is 1,075 bytes on its own.
 *
 * The two words an earlier pass could not name are named by
 * `mkResampledSignal` (finding F5900): +0x3c is the sample index a resampler
 * phase change takes effect at, and +0x24 is the phase step itself -- and
 * +0x24 is the one word no member of this class writes; see the note below.
 *
 * The object is 0x90 bytes, and that is the allocation, not a bound.
 * `V92Modem::V92Modem` builds it:
 *
 *     13e6a:  c7 04 24 90 00 00 00   movl $0x90,(%esp)
 *     13e71:  e8 ..                  call sysdep_malloc
 *     13eb0:  e8 ..                  call V92Modulator::V92Modulator
 *
 * the original compiler's own `sizeof` (finding F1249's oracle). The furthest
 * field is the four bytes at +0x8c, and 0x8c + 4 == 0x90 exactly.
 *
 * ### Two words are left uninitialised, and one of them is never written at all
 *
 * +0x24 and +0x3c are written by neither the constructor nor `reset`, so a
 * freshly built V92Modulator carries whatever `sysdep_malloc` left in them --
 * finding F1240's shape in a third class, and `t_v92mod.cpp` relies on it:
 * both sides are seeded with the same bytes and never zeroed, so the two
 * words compare equal because nobody wrote them, and a reconstruction that
 * helpfully cleared either would fail.
 *
 * +0x3c *is* written, by `progress` (`movl $0x1,0x38(%esi); mov %ebx,0x3c(%esi)`
 * at .text+0x14fb4, and the same pair with a 2 at +0x15033), and cleared by
 * `mkResampledSignal` once the change has been applied.
 *
 * +0x24, though, is read by `mkResampledSignal` and written by nothing in the
 * class: all eighteen symbols were swept for a store to it and there is none,
 * and `progress` -- the last of the eighteen to be written -- confirms it
 * rather than closing it (its four `0x24(%esp)` are the frame slot holding
 * the `n` reference parameter, not the object's +0x24). So the
 * `resamplerPhaseChange == 2` arm adds an uninitialised float to the
 * resampler's phase unless something outside the class has filled the word
 * first. Reproduced with no initialisation added -- docs/deviations.md D800.
 *
 * ### The scrambler at +0x54, and its place in a family of three
 *
 * `Scrambler<int, unsigned char>::Scrambler(this + 0x54, 5, 0x17, 0x63)` opens
 * the constructor, `::reset(this + 0x54, 0)` opens the inlined `reset`, and
 * `::~Scrambler(this + 0x54)` closes the destructor unconditionally -- the D1
 * variant, for a member rather than a non-virtual base. 0x20 bytes, so
 * +0x54..+0x73, meeting the next field at +0x74 exactly.
 *
 * The taps (5, 23) and the slack 99 are the same three numbers
 * `V92Phase3Modulator` and `V92Phase4Modulator` use. The three instantiations
 * differ only in their type arguments -- `<i,h>` here, `<h,i>` there, `<h,h>`
 * in phase 4 -- and every one of those is the mangling's rather than a
 * choice.
 *
 * ### What the object owns: eleven allocations, six of them objects
 *
 *     +0x44  0x50    V92Phase3Modulator(params)
 *     +0x48  0x1cc   V92Phase4Modulator(params, bitsToSymbol, cp, mapParams)
 *     +0x4c  0x20    V92BitsToSymbol(3 * blockSize, params)
 *     +0x50  0x4c    ResamplerTimingOffset(120, 100.0f, 16, 0.98f, 0.0f, 0)
 *     +0x74  0x14    Queue<float>(params->MODULATOR_QUEUE_LENGTH)
 *     +0x78  0x14    FloatFIR(36, v92TxPreFilter, 99)
 *     +0x7c  (blockSize + 10) * 2
 *     +0x80  (blockSize + 10) * 4
 *     +0x88  blockSize * 8
 *     +0x84  (nSamples + 10) * 4
 *     +0x8c  (nSamples + 10) * 4
 *
 * `nSamples` is the constructor's first argument and `blockSize` is derived
 * from it; the two are not interchangeable, and the last three sizes are what
 * says so. The destructor releases all eleven, each behind a null test, in
 * the order +0x44, +0x48, +0x4c, +0x50, +0x74, +0x78, +0x7c, +0x80, +0x88,
 * +0x84, +0x8c. The five raw buffers come back in exactly the order they were
 * taken in, and the six objects do not: the constructor builds them +0x4c,
 * +0x50, +0x44, +0x48, +0x74, +0x78, and the destructor releases them +0x44,
 * +0x48, +0x4c, +0x50, +0x74, +0x78. The transmitter has the same asymmetry
 * one level down (finding F1281), and both are reproduced as found.
 *
 * The resampler is released through its vtable, and the other five objects
 * are not: `mov (%edx),%eax; call *0x4(%eax)` with no `sysdep_free` after it
 * is `delete p` over a class with a virtual destructor and a member
 * `operator delete`; the other five are `if (p) { p->~T(); sysdep_free(p); }`,
 * the same expression over a class that has neither. Both spellings are in
 * the .cpp for that reason, and the difference is the object's.
 *
 * Data member names are invented and descriptive (finding F226).
 */

#ifndef DSPLIB_V92MODULATOR_H
#define DSPLIB_V92MODULATOR_H

#include "dsplib/Scrambler.h"

class FloatFIR;
class ResamplerTimingOffset;
class V92BitsToSymbol;
class V92CP;
class V92Ja;
class V92MappingParams;
class V92Parameters;
class V92Phase2Info;
class V92Phase3Modulator;
class V92Phase4Modulator;
struct tagV90DILdescriptor;
template <class T> class Queue;

/*
 * The scrambler's three constructor arguments: the near tap, the far tap and
 * the slack below the restart point. `mov $0x5`, `mov $0x17`, `mov $0x63` at
 * .text+0x15121..+0x1512c.
 */
#define V92MOD_SCRAM_TAP1	5
#define V92MOD_SCRAM_TAP2	23
#define V92MOD_SCRAM_SLACK	99

/*
 * `blockSize = (unsigned)(nSamples * 0.83333331f + 0.5f)`. The multiplier is
 * `.rodata.cst4+0xc8` = 0x3f555555, which is five sixths rounded to a float,
 * and the addend is `+0xcc` = 0.5f. Both are loaded with `fmuls`/`fadds`, so
 * both are single precision and the `f` suffixes are load-bearing; the
 * conversion back is `fistpll` with the control word forced to truncate and
 * only the low half kept, which is what GCC emits for float -> unsigned.
 */
#define V92MOD_RATE_NUM		5.0f
#define V92MOD_RATE_DEN		6.0f

/* Exact binary32 multiplier loaded by the constructor (+0xc8 above) and
 * progress (.rodata.cst4+0xc0). Spell the rounded value directly: the period
 * compiler's fast-math folding of 5.0f / 6.0f instead produces 0x3f555556.
 * This records the binary value, not the original author's source spelling. */
#define V92MOD_RATE_RATIO	0.833333313465118408203125f

/* ResamplerTimingOffset(120, 100.0f, 16, 0.98f, 0.0f, 0): six literals, from
 * .text+0x1525f..+0x1528d. 0x42c80000 is 100.0f and 0x3f7ae148 is 0.98f. */
#define V92MOD_RS_PHASES	120
#define V92MOD_RS_PPMSCALE	100.0f
#define V92MOD_RS_TAPS		16
#define V92MOD_RS_CUTOFF	0.98f
#define V92MOD_RS_PPM		0.0f
#define V92MOD_RS_MINHISTORY	0

/* FloatFIR(36, v92TxPreFilter, 99) -- the tap count is the table's own, which
 * include/dsplib/vpcm_tables.h carries, and 99 is the block slack. */
#define V92MOD_FIR_BLOCK	99

/* The slack every derived buffer but one carries: `add $0xa` and
 * `lea 0x28(,%ebx,4)` are the same ten elements spelled two ways. */
#define V92MOD_BUF_SLACK	10

/*
 * Which phase the modulator is in -- `phase`, +0x2c. Each of the three named
 * values is a literal stored on the instruction after a message that names
 * it, which is CLAUDE.md's strongest evidence tier:
 *
 *     "V92Modulator enter Phase 3"       ->  1    enterPhase3, +0x144bf
 *     "V92Modulator: enter Phase 4"      ->  2    enterPhase4, +0x148f9
 *     "V92Modulator:enter  Data Phase:"  ->  3    enterDataPhase, +0x1494a
 *
 * `initiateRRN` and `initiateFPE` also store 2, from a guard on 3, and
 * `progress` stores both 2 and 3 -- by calling `enterPhase4` and
 * `enterDataPhase`, which GCC inlines at both sites; see the .cpp.
 *
 * Zero is the fourth value, and it is `reset`'s: named for the member that
 * stores it and for nothing else. `progress` carries an arm for it that
 * fills the symbol block with zeros, which is a reading of the arm and not
 * of the name, so the name stays the factual one.
 *
 * The field is a signed `int`, and that is forced: `progress` lowers its
 * five-way dispatch as `cmp $0x1,%edx; je; jle` at .text+0x14c9e -- a signed
 * branch, which GCC cannot emit for an unsigned switch value; an unsigned
 * one would have been `jbe`. Every other member compares it for equality
 * only, so nothing else in the class could have settled it. (Finding F5900.)
 */
#define V92MOD_PHASE_RESET	0
#define V92MOD_PHASE_3		1
#define V92MOD_PHASE_4		2
#define V92MOD_PHASE_DATA	3

/*
 * What `mkResampledSignal` does to the resampler's phase -- the three values
 * of `resamplerPhaseChange`, +0x38. `progress` is the only writer of 1 and 2
 * (.text+0x14fb4 and +0x15033, each storing `resamplerPhaseChangeAt` in the
 * same breath); `reset`, `enterPhase3` and `mkResampledSignal` itself store 0.
 *
 * The two non-zero arms are named by the two messages the arms themselves
 * print: "V92Modulator: setPhase = 0.5" against the literal 0.5f, and
 * "V92Modulator: setPhase = %c%d.%05d" against `resamplerPhaseOffset`.
 */
#define V92MOD_PHASECHG_NONE	0
#define V92MOD_PHASECHG_HALF	1
#define V92MOD_PHASECHG_OFFSET	2

/* The half-sample step the `HALF` arm applies: `fadds .rodata.cst4+0xbc`,
 * which is 0x3f000000. */
#define V92MOD_PHASECHG_HALF_STEP	0.5f

/* The scale the diagnostic prints the fractional part of the phase step at --
 * `flds .rodata.cst4+0xb8` = 0x47c35000 -- and it is the five digits of the
 * message's own `%05d`. */
#define V92MOD_PHASE_PRINT_SCALE	100000.0f

/*
 * What `getV92TxFilterDelay` reports: 18 when `V92_APPLY_TX_SHAPING_FILTER`
 * is set and 0 when it is not. `and $0x12,%eax` at .text+0x14461 is the whole
 * of it, so 18 is a literal in the object and the derivation below is
 * inference, not measurement: the shaping filter is
 * `FloatFIR(36, v92TxPreFilter, 99)`, and a linear-phase FIR of 36 taps has a
 * group delay of 18 samples. The name records the role the object gives it;
 * the number is the object's.
 */
#define V92MOD_TX_FILTER_DELAY	18

/*
 * The one `eventCode` code this class originates, and it is named the way
 * the phase codes are: `progress` prints "V92Modulator: Queue is Empty/Full
 * !!!" (.rodata.str1.4+0x3880) and the very next instruction stores 1
 * (.text+0x14dba). The author's own words for the condition.
 *
 * Every other non-zero value the field holds is copied in from a
 * sub-modulator -- `V92Phase3Modulator::eventCode` or
 * `V92Phase4Modulator::eventCode` -- so those codes belong to those classes'
 * alphabets and are not respelled here. `progress` compares against 5, 7, 8
 * and 9 as bare numbers for that reason, with the transition each one is
 * beside the comparison. 10 is `enterDataPhase`'s and is likewise written
 * where it is stored.
 */
#define V92MOD_STATUS_QUEUE_LIMIT	1

class V92Modulator {
public:
	/**
	 * @brief Build the transmit graph: allocate and construct the phase
	 *        3/4 sub-modulators, the bit-to-symbol stage, the resampler,
	 *        the sample queue and the shaping filter, then run `reset`
	 *        inline.
	 * @param nSamples      Samples per `progress()` call this modulator
	 *                       is sized for; also sizes the resampler's
	 *                       output buffers directly (not via `blockSize`).
	 * @param phase2Info    Stored, not owned; `rtd` is threaded on to
	 *                       every phase 3/4 `reset`.
	 * @param ja            Stored, not owned; handed to the phase 3
	 *                       sub-modulator.
	 * @param dil           Stored, not owned; handed to the phase 3
	 *                       sub-modulator.
	 * @param cp            Stored, not owned; handed to the phase 4
	 *                       sub-modulator.
	 * @param mappingParams Stored, not owned; handed to the phase 4
	 *                       sub-modulator.
	 * @param params        Stored, not owned; read back for `blockSize`,
	 *                       the queue length and both sub-modulators.
	 */
	V92Modulator(unsigned int nSamples, V92Phase2Info *phase2Info,
		     V92Ja *ja, tagV90DILdescriptor *dil, V92CP *cp,
		     V92MappingParams *mappingParams, V92Parameters *params);

	/**
	 * @brief Release the eleven owned allocations (six sub-objects, five
	 *        raw buffers), each behind a null test; the resampler is
	 *        released through its vtable, the rest directly.
	 */
	~V92Modulator();

	/*
	 * Argument types below are the mangling's and exact; return types are
	 * not mangled, so `void` on most of these is measured -- each leaves
	 * nothing in %eax -- and `getV92TxFilterDelay`'s `int` and the two
	 * `initiate*` members' `int` are measured the other way, ending in a
	 * value left in %eax.
	 */

	/**
	 * @brief Reset the transmit graph to its idle state: rearm the
	 *        scrambler, clear `phase`/`symbolCount`/`eventCode`/
	 *        `resamplerPhaseChange`, reset and re-prime the sample queue,
	 *        and reset the shaping filter. The constructor calls this and
	 *        GCC inlines it (finding F1283).
	 */
	void reset();

	/**
	 * @brief Enter phase 3: reset the phase 3 sub-modulator to its first
	 *        state and take `phase` to 1. A no-op if already in phase 3.
	 */
	void enterPhase3();

	/**
	 * @brief Enter phase 4: reset the phase 4 sub-modulator to state 0
	 *        and take `phase` to 2. A no-op if already in phase 4. Also
	 *        runs, as an inlined call, inside `progress`.
	 */
	void enterPhase4();

	/**
	 * @brief Enter the data phase: tell the bit-to-symbol stage the
	 *        current block size and take `phase` to 3. A no-op if
	 *        already in the data phase.
	 */
	void enterDataPhase();

	/**
	 * @brief Request the phase 3 sub-modulator's Ja exit if it is
	 *        currently in the Ja state; a no-op otherwise.
	 */
	void exitJa();

	/**
	 * @brief Request the phase 3 sub-modulator's Silence exit if it is
	 *        currently in that state; a no-op otherwise.
	 */
	void exitSilence();

	/**
	 * @brief Request the phase 3 sub-modulator's SuSecond exit if it is
	 *        currently in that state; a no-op otherwise.
	 */
	void exitSuSecond();

	/**
	 * @brief Request the phase 3 sub-modulator's TRN1uSecond exit if it
	 *        is currently in that state; a no-op otherwise.
	 */
	void exitTRN1uSecond();

	/**
	 * @brief Request the phase 4 sub-modulator's CPt exit if it is
	 *        currently in state 0; a no-op otherwise. Same one-shape,
	 *        guarded-request pattern as the four phase 3 exits above,
	 *        each its own blob symbol (findings F605, F610).
	 */
	void exitCPt();

	/**
	 * @brief Resample one block, splitting it at the pending phase change
	 *        (if any) so the first segment resamples at the old phase and
	 *        the second at the new one.
	 * @param n Set to the number of output samples produced.
	 */
	void mkResampledSignal(unsigned int &n);

	/**
	 * @brief Report the transmit shaping filter's group delay in
	 *        samples.
	 * @return #V92MOD_TX_FILTER_DELAY when the shaping filter is enabled
	 *         (`params->V92_APPLY_TX_SHAPING_FILTER`), 0 otherwise.
	 */
	int getV92TxFilterDelay() const;

	/**
	 * @brief Advance the transmit graph by one call's worth of samples:
	 *        generate the next block of upstream symbols for the current
	 *        phase, resample, shape, queue and drain into @p out.
	 * @param bits     Phase-3-only: bit source consumed via the phase 3
	 *                  sub-modulator.
	 * @param nbits    Set (in phase 3) to how many bits the next call
	 *                  will need.
	 * @param out      Output sample buffer.
	 * @param nSamples How many output samples to produce.
	 */
	void progress(int *bits, unsigned int &nbits, float *out,
		      unsigned int nSamples);

	/**
	 * @brief Ask the modulator to enter phase 4 and start the RRN
	 *        (retrain) renegotiation signal. Refused unless currently in
	 *        the data phase; nothing in the object calls this, it is an
	 *        external request.
	 * @return 0 if the request was approved and phase 4 entered, -1 if
	 *         refused (not in the data phase).
	 */
	int initiateRRN();

	/**
	 * @brief Ask the modulator to enter phase 4 and start the FPE
	 *        (fast probing exchange) renegotiation signal. Shares
	 *        `initiateRRN`'s shape, differing only in the state codes,
	 *        the `resetBefor*` callee, one extra store into the modulus
	 *        encoder, and the diagnostic text.
	 * @return 0 if the request was approved and phase 4 entered, -1 if
	 *         refused (not in the data phase).
	 */
	int initiateFPE();

	/* Public for `offsetof`; one access section, as everywhere here. */

	/*
	 * +0x00  `(unsigned)(nSamples * 5/6 + 0.5f)`. Read back out of the
	 * object three times by the constructor itself to size buffers, and
	 * copied to +0x08 by `reset`.
	 */
	unsigned int blockSize;

	/*
	 * +0x04  `params->MODULATOR_QUEUE_LENGTH >> 1`, the bound of the loop
	 * that primes the queue. Unsigned, despite the signed parameter it is
	 * shifted from (finding F1284) -- a negative parameter would prime
	 * the queue about two billion times.
	 */
	unsigned int queuePrime;

	/* +0x08  Set to `blockSize` by `reset`; a running copy of it. */
	unsigned int blockRemaining;

	/* +0x0c  Cleared by `reset`. One byte. */
	unsigned char byte_0c;

	/* +0x0d  Cleared by `reset`. One byte. */
	unsigned char byte_0d;

	/*
	 * +0x0e was `pad_0e[2]` -- removed (finding F10151). Already
	 * correctly described as alignment; proved mechanically by the
	 * existing offset assertions on `byte_0d` (0x0d) and `phase2Info`
	 * (0x10), and by `dis.py` over all eighteen `V92Modulator` methods
	 * finding no access to offset 0x0e/0x0f.
	 */

	/* +0x10  The constructor's SECOND argument, stored and not owned. */
	V92Phase2Info *phase2Info;

	/* +0x14  The constructor's THIRD argument, stored and not owned. */
	V92Ja *ja;

	/* +0x18  The constructor's FOURTH argument, stored and not owned. */
	tagV90DILdescriptor *dil;

	/* +0x1c  The constructor's SIXTH argument; handed on to the phase 4
	 * modulator and not owned. */
	V92MappingParams *mappingParams;

	/* +0x20  The constructor's FIFTH argument; handed on to the phase 4
	 * modulator and not owned. */
	V92CP *cp;

	/*
	 * +0x24  The phase step the OFFSET arm of `mkResampledSignal` adds to
	 * the resampler's normalised phase, printed by its diagnostic as
	 * `setPhase = %c%d.%05d`. A float, forced by three single-precision
	 * loads/stores in `mkResampledSignal` (finding F5900).
	 *
	 * Nothing in the class writes it -- see the note at the top of this
	 * file, and docs/deviations.md D800. The hole is still the claim; it
	 * is now a claim about all eighteen symbols rather than about two.
	 */
	float resamplerPhaseOffset;

	/* +0x28  Cleared by `reset` -- a 32-bit zero, and the same zero is
	 * passed to `Queue<float>::write`, so it is a float. */
	float float_28;

	/*
	 * +0x2c  Which phase is running -- 1, 2 or 3, named by the message
	 * printed on the instruction before the store; see the
	 * `V92MOD_PHASE_*` block above, including why the type is signed.
	 * Every `enter*` member returns early when it is already at its own
	 * value, so each transition happens at most once, and
	 * `mkResampledSignal` takes its split path only in phase 3. Cleared
	 * by `reset`.
	 */
	int phase;

	/*
	 * +0x30  Cleared by `reset`, `enterPhase3`, `enterPhase4` and
	 * `enterDataPhase`, and added to by `progress` -- `add %edi,0x30(%esi)`
	 * at .text+0x14c80, where %edi is the symbol count the call was asked
	 * for. So it accumulates symbols since the last phase transition,
	 * nothing in this class's own object reads it back, and static usage
	 * inference alone would stop there.
	 *
	 * Named `symbolCount` on a matched-sibling cross-check instead: this
	 * class's own `V90Modulator` twin (`include/dsplib/V90Modulator.h`)
	 * carries the identical accumulator, at the identical +0x30, with the
	 * identical name and the identical derivation ("adds its symbol count
	 * to it on entry... exactly `V90Phase3Modulator::symbolCount` and
	 * `V90Phase4Modulator::symbolCount`, which sit in the same role").
	 * Neither twin was formally derived from the other -- `V92Modulator`
	 * does not inherit from `V90Modulator` -- so this is rank-3 evidence
	 * (structural correspondence between two independently-written but
	 * closely-related classes), not a type guarantee. It is offered here
	 * because the role, offset, width and clearing sites all agree; a
	 * name is not applied on offset agreement alone (see `word_34`
	 * catching a real value directly, and `V92Phase3Modulator::word_00`
	 * -- +0x00 in that class's own twin pair -- being checked and
	 * declined for exactly this reason: it turns out to hold
	 * `phase2Info->rtd`, not a session flag, despite sitting at the same
	 * offset the twin's session flag occupies).
	 */
	unsigned int symbolCount;

	/*
	 * +0x34  Cleared by `reset`, by all three `enter` members, by all
	 * five `exit` members and by both `initiate` members; set to 10 by
	 * `enterDataPhase`.
	 *
	 * `progress` is what it is for. It clears the field on entry, latches
	 * `V92Phase3Modulator::eventCode` or `V92Phase4Modulator::eventCode`
	 * into it once per symbol, and then dispatches on its own copy: 5
	 * and 7 stage a resampler phase change at that symbol, 8 enters
	 * phase 4, 9 enters the data phase, and 1 is written on the way out
	 * when the queue is empty or full. So it is a per-block status,
	 * produced here and read by the layer above.
	 *
	 * Only the 1 is this class's own code; see `V92MOD_STATUS_QUEUE_LIMIT`
	 * for why the others are left as bare numbers. Named `eventCode` to
	 * match `V90Modulator::eventCode` (same +0x34, same role, same
	 * "copied in from the sub-modulator's own eventCode" shape) -- this
	 * class's own header already named the exact source fields it copies
	 * from (`V92Phase3Modulator::eventCode`, `V92Phase4Modulator`'s own
	 * field, itself renamed alongside this one on the same cross-check),
	 * it just had not carried the name across to its own destination.
	 */
	unsigned int eventCode;

	/*
	 * +0x38  Which resampler phase change is pending: 0, 1 or 2, and the
	 * `V92MOD_PHASECHG_*` block above says how each was named.
	 * `mkResampledSignal` acts on it and clears it, `progress` sets it,
	 * and `reset` and `enterPhase3` clear it.
	 */
	unsigned int resamplerPhaseChange;

	/*
	 * +0x3c  How many input samples of the block are resampled before the
	 * pending change takes effect. `mkResampledSignal` resamples
	 * `resampleIn[0 .. n)` at the old phase, changes the phase, resamples
	 * `resampleIn[n .. blockRemaining)` at the new one, and then clears
	 * both this and `resamplerPhaseChange`. Written only by `progress`,
	 * alongside the code above. Usage inference; nothing prints it.
	 */
	unsigned int resamplerPhaseChangeAt;

	/* +0x40  The constructor's SEVENTH argument. Read back by the
	 * constructor itself for the phase 3 and phase 4 modulators and for
	 * the queue's length, and by `reset` for the queue again. */
	V92Parameters *params;

	/* +0x44  Owned: `V92Phase3Modulator(params)`. */
	V92Phase3Modulator *phase3Modulator;

	/* +0x48  Owned: `V92Phase4Modulator(params, bitsToSymbol, cp,
	 * mappingParams)`. */
	V92Phase4Modulator *phase4Modulator;

	/* +0x4c  Owned: `V92BitsToSymbol(3 * blockSize, params)`. */
	V92BitsToSymbol *bitsToSymbol;

	/* +0x50  Owned, and the only one released through a vtable. */
	ResamplerTimingOffset *resampler;

	/* +0x54  The upstream scrambler, built (5, 23, 99). A member. */
	Scrambler<int, unsigned char> scrambler;

	/* +0x74  Owned: `Queue<float>(params->MODULATOR_QUEUE_LENGTH)`. */
	Queue<float> *queue;

	/* +0x78  Owned: `FloatFIR(36, v92TxPreFilter, 99)`. */
	FloatFIR *txFilter;

	/*
	 * +0x7c  `(blockSize + 10) * 2` bytes. The element width is the
	 * multiplier -- `lea 0x14(%ebp,%ebp,1)`, which is `(n + 10) * 2` --
	 * and `short` is the narrowest type that gives it.
	 */
	short *buf_7c;

	/*
	 * +0x80  The resampler's input, `(blockSize + 10) * 4` bytes:
	 * `add $0xa; shl $2`. `float *` is the callee's -- all three of
	 * `mkResampledSignal`'s calls pass it as `Resampler::resample`'s
	 * `const float *in`, and the second passes `resampleIn + n`, so the
	 * element width is four and the element type is float (finding
	 * F5900). Sized from `blockSize`, which is the rate the resampler
	 * reads at.
	 */
	float *resampleIn;

	/*
	 * +0x84  The resampler's output, `(nSamples + 10) * 4` bytes, from
	 * the constructor argument and not from `blockSize`:
	 * `lea 0x28(,%ebx,4)` with %ebx holding the argument, computed once
	 * and used for this and for +0x8c. `nSamples` is the rate the
	 * resampler writes at, which is the second thing saying which of the
	 * pair is the input.
	 */
	float *resampleOut;

	/*
	 * +0x88  The scrambled bit block, `blockSize * 8` bytes: `shl $0x3`
	 * and no slack, so eight bytes per block element.
	 *
	 * The element type is the callees' and not a reading of the shift.
	 * `progress`'s data-phase arm passes it twice --
	 * `Scrambler<int, unsigned char>::process(const int *, unsigned char *,
	 * unsigned)` at .text+0x15005 and `V92BitsToSymbol::process(unsigned
	 * char *, unsigned int &, short *)` at +0x15029 -- and the mangling
	 * spells both parameters `unsigned char *`.
	 *
	 * Eight bytes per block element is then eight bits per symbol, which
	 * is what the scrambler unpacks each input word into; the .cpp still
	 * allocates the byte count the object computes.
	 */
	unsigned char *buf_88;

	/*
	 * +0x8c  Where the second segment is resampled to, `(nSamples + 10) * 4`
	 * bytes -- the same expression as +0x84 and the same register; the
	 * last four bytes of the object. `mkResampledSignal` resamples the
	 * tail of the block here and then copies it up behind the head in
	 * `resampleOut`, which is why the two are the same size (finding
	 * F5900).
	 */
	float *resampleTail;
};

#endif /* DSPLIB_V92MODULATOR_H */

/**
 * @file V90Modulator.h
 * @brief `V90Modulator`, the V.90 downstream modulator: the top of the
 *        transmit chain, dispatching each block to whichever phase
 *        sub-modulator is active and driving the eleven phase transitions
 *        between them.
 *
 * Seventeen members and 3,354 bytes of code, and the class is complete: the
 * constructor, the destructor, `setSessionFlag`, `reset`, `progress`,
 * `initiateRRN` and the eleven phase transitions -- `enterPhase3`,
 * `enterPhase4`, `enterDataPhase`, `exitJd`, `exitJdPhase`, `exitDIL`,
 * `exitRi`, `acknowledgeCPReception`, `acknowledgeCPNotReception`,
 * `acknowledgeEReception` and `initiateFPE`.
 *
 * This class used to be a partial map -- `pad_00[0x28]`, `sessionFlag`,
 * `pad_2c[0x0c]`, then the two modulator pointers -- declared in
 * `V90SessionFlag.h`, derived from `mov 0x38(%esi),%edx` being a load where
 * the two demodulators embed their phase blocks. It has moved here and been
 * filled in, exactly as `V90Phase3Demodulator` and `V90Demodulator` moved
 * out of that file before it; `src/pump/v90/V90SessionFlag.cpp` asserts the
 * three offsets it named and they are unchanged.
 *
 * Not polymorphic: `~V90Modulator` is listed with `D1` and `D2` and no
 * `D0`, so offset 0 is a real member and there is no vptr.
 *
 * The size is 0x70, the original compiler's own `sizeof`: `V90Modem`'s
 * constructor does
 *
 *     movl $0x70,(%esp) ; call sysdep_malloc ; ... ; call V90Modulator::C1
 *
 * at .text+0x19604, which is finding F1246's oracle -- the allocation is
 * `sizeof(V90Modulator)` written by the compiler that laid the class out.
 * The highest field the constructor writes is the pointer at +0x6c, which
 * ends at 0x70 exactly, so nothing is unaccounted for.
 *
 * Twelve constructor arguments, and the order they land in is not the order
 * they arrive in: arguments 2..5 go to +0x00..+0x0c in order, argument 6 to
 * +0x10 and 7 to +0x14, and then 8, 9, 10, 11 go to +0x18, +0x1c, +0x20,
 * +0x24 with the `V90CP` (argument 9) landing above the `V90MP` (argument
 * 10). Argument 1 goes to +0x64, not +0x00, and argument 12 to +0x28,
 * the session flag. Nothing about this is inferable from the signature.
 *
 * The scrambler is embedded, not pointed at: `lea 0x44(%ebx),%eax` before
 * both `Scrambler<int,unsigned char>::Scrambler` in the constructor and
 * `::~Scrambler` in the destructor. Its taps are (0x12, 0x17, 0x63) --
 * V.90's 18 and 23, the same three constants `V90Phase3Demodulator` builds
 * its descrambler with.
 *
 * +0x2c..+0x37 are three words, established by `reset`: they were
 * `pad_2c[0x0c]` while only the constructor was written; `reset` stores a
 * separate `movl $0x0` into each of +0x2c, +0x30 and +0x34 (0x1a532,
 * 0x1a539, 0x1a540), which fixes the width at four bytes and the count at
 * three. `progress` and `initiateRRN` are what earned them names (finding
 * F7520):
 *
 *   - +0x2c `state`.  The object's own word: the `default` arm of `progress`'s
 *     switch prints "V90Modulator progress: Illegal state".  1 is phase 3, 2
 *     is phase 4, 3 is the data phase and 0 emits silence.  It is `int` and
 *     not `unsigned int`, which is MEASURED: the switch at 0x1a848 is
 *     `cmp $0x1 ; je ; jle`, and a `jle` is the signed decision tree -- an
 *     unsigned selector gives `jbe` there.  Nothing else in the class compares
 *     it with anything but `==`, so this one site is the whole evidence and it
 *     is enough.
 *   - +0x30 `symbolCount`.  `progress` adds its symbol count to it on entry
 *     and every state transition clears it, so it counts symbols since the
 *     state was entered -- exactly `V90Phase3Modulator::symbolCount` (+0x018)
 *     and `V90Phase4Modulator::symbolCount` (+0x0008), which sit in the same
 *     relative position in their own triples.  It stays UNSIGNED: 0x1aab2
 *     compares it against `V90Parameters::DEBUG_DIGITAL_MODEM_INITIATE_RRN_
 *     TIME`, which is declared `int`, and the object's `jbe` is unsigned --
 *     so the unsignedness has to come from this side of the comparison.
 *   - +0x34 `eventCode`.  Every value in it is COPIED OUT OF a field already
 *     named that: `V90Phase3Modulator::eventCode` (+0x01c), whose header says
 *     "the caller's per-symbol notification and nothing reads it here", and
 *     `V90Phase4Modulator::word_000c` (+0x000c), whose header says "what reads
 *     it is outside this class".  `progress` is that caller and that reader.
 *     It acts on 6 (phase 3 terminated -> enter phase 4) and 7 (phase 4
 *     terminated -> enter the data phase) and sets 8 and 9 of its own.
 *
 * The constructor still does not touch any of the three; `reset` is what
 * clears them, and `progress` is undefined before the first `reset`.
 *
 * Data member names are invented; the mangling never carries one (finding
 * F226).  `sessionFlag`, `phase3Modulator` and `phase4Modulator` keep the names
 * V90SessionFlag.h gave them.
 */

#ifndef DSPLIB_V90MODULATOR_H
#define DSPLIB_V90MODULATOR_H

#include "dsplib/Scrambler.h"		/* embedded at +0x44, 0x20 bytes */

class V90BitsToSymbol;
class V90CP;
class V90Jd;
class V90MP;
class V90MappingParams;
class V90Parameters;
class V90Phase2Info;
class V90Phase3Modulator;
class V90Phase4Modulator;
class V92Jd;
struct tagV90AdditionalCPinfo;
struct tagV90DILdescriptor;

class V90Modulator {
public:
	/**
	 * @brief Construct the modulator and its three owned sub-objects.
	 *
	 * Stores all twelve arguments (see the file comment for the
	 * offset-vs-argument-order mapping), allocates the symbol and frame
	 * buffers, and builds `bitsToSymbol` (with capacity
	 * `3 * nofSymbols + 0x1388`), `phase3Modulator` and `phase4Modulator`
	 * in that order. `mappingParams` and `mappingParams2` are passed to
	 * the phase 4 sub-modulator swapped relative to how they arrived
	 * (finding, see .cpp).
	 *
	 * @param nofSymbols          Symbol-buffer capacity.
	 * @param phase2Info          Shared phase 2 connection info.
	 * @param jd                  The V.90 Jd message.
	 * @param v92Jd               The V.92 Jd message.
	 * @param dil                 The DIL descriptor.
	 * @param mappingParams       Mapping block for TRN2d/MP (see exitRi()).
	 * @param mappingParams2      Mapping block for everything else.
	 * @param additionalCPinfo    The additional-CP-info record.
	 * @param cp                  The V.90 CP message.
	 * @param mp                  The V.90 MP message.
	 * @param params              The V.90 parameter block.
	 * @param sessionFlag         Nonzero for V.92, zero for V.90.
	 */
	V90Modulator(unsigned int nofSymbols, V90Phase2Info *phase2Info,
		     V90Jd *jd, V92Jd *v92Jd, tagV90DILdescriptor *dil,
		     V90MappingParams *mappingParams,
		     V90MappingParams *mappingParams2,
		     tagV90AdditionalCPinfo *additionalCPinfo, V90CP *cp,
		     V90MP *mp, V90Parameters *params, unsigned int sessionFlag);
	/** @brief Destroy the modulator and all five owned allocations, none null-checked. */
	~V90Modulator();

	/**
	 * @brief Per-connection reset.
	 *
	 * Resets the embedded scrambler to its constructor taps (does not
	 * rebuild it) and clears `state`, `symbolCount` and `eventCode`.
	 */
	void reset();

	/**
	 * @brief Produce one block of transmit symbols.
	 *
	 * The transmit chain's per-block entry point and the only thing
	 * `V90Modem::progress` calls on the digital side. One switch over
	 * `state` (silence, phase 3, phase 4, data phase, or "Illegal
	 * state") and a shared tail that widens the finished `symbolBuf`
	 * into @p out. @p bits is only touched in the data phase, where it
	 * goes straight to the scrambler. The phase 3 arm re-tests `state`
	 * on every symbol, since a phase-3-to-4 boundary can fall mid-block;
	 * the data phase arm can trigger an automatic `initiateRRN()` once
	 * `symbolCount` passes `params->DEBUG_DIGITAL_MODEM_INITIATE_RRN_TIME`.
	 *
	 * @param bits        Scrambler input, used only in the data phase.
	 * @param nofBits     Receives the bit demand for the next call.
	 * @param out         Receives `nofSymbols` output samples.
	 * @param nofSymbols  How many symbols to produce this call.
	 */
	void progress(int *bits, unsigned int &nofBits, float *out,
		      unsigned int nofSymbols);

	/**
	 * @brief Move from phase 2 into phase 3. Idempotent.
	 * Rebuilds `phase3Modulator` from `phase2Info` and the message
	 * objects, with a zero warm-up count and starting state `P3M_STATE_SD`.
	 */
	void enterPhase3();
	/**
	 * @brief Move into phase 4. Idempotent.
	 * Resets `phase4Modulator` to `P4M_STATE_RI`, the first thing the
	 * digital modem sends in phase 4.
	 */
	void enterPhase4();
	/**
	 * @brief Move into the data phase. Idempotent.
	 * Sets `symbolsBlockSize` to `nofSymbols` and logs the negotiated
	 * bit rate, computed from `bitsToSymbol->mapper->bitsPerFrame`.
	 */
	void enterDataPhase();
	/** @brief Leave the Jd phase 3 sub-state, guarded on `phase3Modulator->state`. */
	void exitJd();
	/** @brief Leave the JdPhase phase 3 sub-state, guarded on `phase3Modulator->state`. */
	void exitJdPhase();
	/**
	 * @brief Leave the DIL phase 3 sub-state, and enter phase 4 if that
	 *        completed the DIL sequence.
	 * Guarded on `phase3Modulator->state`; the follow-on move into phase
	 * 4 is guarded separately on the sub-modulator's own `eventCode`
	 * reading 6 (DIL sequence really ended) and on `state != 2`.
	 */
	void exitDIL();
	/**
	 * @brief Leave the Ri phase 4 sub-state.
	 *
	 * The one member of the class that reads `mappingParams` (+0x10)
	 * rather than `mappingParams2` (+0x14) -- every other member that
	 * reaches a mapping block (progress(), initiateRRN(),
	 * enterDataPhase(), initiateFPE()) takes the other one. Installs the
	 * mapping block into `phase4Modulator`, logs its spectral parameters
	 * and bit count, and stores that bit count into the V.92 CP's or
	 * V.90 MP's own copy, per `sessionFlag`.
	 */
	void exitRi();
	/**
	 * @brief Handle a received CP: re-encode the MP, then leave the MP
	 *        phase 4 sub-state if it was active.
	 * The MP re-encode and the "MP not yet sent" flag are updated
	 * unconditionally, before the phase 4 state is even looked at.
	 */
	void acknowledgeCPReception();
	/**
	 * @brief Handle a received CPnot.
	 * Three phase 4 states, each handled differently: leaves MPnot if
	 * that was active; re-encodes the MP and leaves MP if that was
	 * active; or, in the mid-repetition state, only records the request
	 * (`byte_0014 = 1`) for whoever crosses the next boundary.
	 */
	void acknowledgeCPNotReception();
	/**
	 * @brief Handle a received E.
	 * Two of acknowledgeCPNotReception()'s three phase 4 states (no MP
	 * re-encode arm): leaves MPnot if active, or records a delayed
	 * MPnot exit in the mid-repetition state.
	 */
	void acknowledgeEReception();

	/**
	 * @brief Start fast phase exchange from the data phase.
	 *
	 * initiateRRN()'s near-twin: same guard and same two returns, same
	 * block-size-to-one probe of `nofBitsForNextTime()`, but enters
	 * `P4M_STATE_RF`/`P4M_STATE_UNNAMED_1C` (not RRN's RD/0x14), calls
	 * `setRfSymbols` (not `setRdRtSymbols`), has no `resetBeforRRN`
	 * step, and re-encodes the V.90 CP unconditionally rather than
	 * forking on `sessionFlag` -- a V.90 session's `V90MP` is never
	 * touched by this edge.
	 *
	 * @return 0 if it acted, -1 if `state` was not the data phase.
	 */
	int initiateFPE();

	/**
	 * @brief Start rate renegotiation from the data phase.
	 *
	 * Only acts from the data phase; otherwise logs "requested but NOT
	 * approved" and returns -1. Otherwise: moves to phase 4, sizes the
	 * converter's block to one symbol, and chooses
	 * `P4M_STATE_RD`/`P4M_STATE_UNNAMED_14` by whether bits are still
	 * owed for that block; resets `phase4Modulator` into that state,
	 * calls `resetBeforRRN()` and `setRdRtSymbols()`; then forks on
	 * `sessionFlag` to re-encode either the V.92 CP or the V.90 MP.
	 * Also called from progress() on the data phase's own timer.
	 *
	 * @return 0 if it acted, -1 if `state` was not the data phase.
	 */
	int initiateRRN();

	/** @brief Propagate a new session flag; defined in src/pump/v90/V90SessionFlag.cpp. */
	void setSessionFlag(unsigned int flag);

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	V90Phase2Info *phase2Info;		/* +0x00 argument 2       */
	V90Jd *jd;				/* +0x04 argument 3       */
	V92Jd *v92Jd;				/* +0x08 argument 4       */
	tagV90DILdescriptor *dil;		/* +0x0c argument 5       */
	V90MappingParams *mappingParams;	/* +0x10 argument 6       */
	V90MappingParams *mappingParams2;	/* +0x14 argument 7       */
	tagV90AdditionalCPinfo *additionalCPinfo; /* +0x18 argument 8     */
	V90MP *mp;				/* +0x1c argument 10      */
	V90CP *cp;				/* +0x20 argument 9       */
	V90Parameters *params;			/* +0x24 argument 11      */
	unsigned int sessionFlag;		/* +0x28 argument 12      */
	int state;				/* +0x2c reset clears     */
	unsigned int symbolCount;		/* +0x30 reset clears     */
	unsigned int eventCode;			/* +0x34 reset clears     */
	V90Phase3Modulator *phase3Modulator;	/* +0x38 owned, 0x398     */
	V90Phase4Modulator *phase4Modulator;	/* +0x3c owned, 0x2fac    */
	V90BitsToSymbol *bitsToSymbol;		/* +0x40 owned, 0x24      */
	Scrambler<int, unsigned char> scrambler; /* +0x44 32 bytes        */
	unsigned int nofSymbols;		/* +0x64 argument 1       */
	short *symbolBuf;			/* +0x68 2 per symbol     */

	/*
	 * +0x6c, 8 bytes per symbol.  WAS `void *`, and the type is forced
	 * rather than chosen: `progress` hands it to
	 * `Scrambler<int,unsigned char>::process` as its `unsigned char *`
	 * output and to `V90BitsToSymbol::process` as its `unsigned char *`
	 * input, and C++ converts neither from `void *` without a cast.  One
	 * scrambled BIT PER BYTE, which is what makes 8 per symbol the right
	 * size -- `V90Mapper::process` reads the same buffer a byte to a bit.
	 */
	unsigned char *frameBuf;		/* +0x6c 8 per symbol     */
};

#endif /* DSPLIB_V90MODULATOR_H */

/**
 * @file V90Phase4Modulator.h
 * @brief `V90Phase4Modulator` -- the V.90/V.92 phase 4 downstream symbol
 *        source: a state machine that drains scrambled bits through a
 *        `V90BitsToSymbol` converter to produce the trained data-mode
 *        symbol stream, plus the training and message sequences (Ri, TRN2d,
 *        B1d, Ed, MP, CPd, SUVd, Rd/Rt/Rf and their `*Not` counterparts)
 *        that lead up to it.
 *
 * Not polymorphic (the destructor is `D1`/`D2` with no `D0`, so there is no
 * vptr and offset 0 is a real member). The object is 12,204 bytes (0x2fac)
 * -- a displacement bound (finding F215) independently confirmed to the
 * byte by `V90Modulator`'s own `sysdep_malloc(0x2fac)` before it constructs
 * this class (finding F1293). The 12,204 bytes are two dense runs of small
 * fields, +0x0000..+0x0043 and +0x2f64..+0x2fab, with a 12,000-byte
 * scratch buffer between them (findings F4930-F4932, F3120).
 *
 * The constructor stores its eight arguments (five borrowed pointers, two
 * flags, one `V90Parameters *`) into the object's small fields, embeds a
 * `Scrambler<unsigned char, unsigned char>` at +0x0058 built with V.90's
 * taps 18 and 23, and either takes ownership of a supplied
 * `V90BitsToSymbol` or allocates and constructs its own -- `externalBits
 * ToSymbol` (+0x2fa4) records which, and the destructor checks it first and
 * skips releasing a supplied converter entirely.
 *
 * The 12,000-byte buffer at +0x0078 (`scrambledBits`, `#V90P4M_BITS` long)
 * is the scrambler's output and the bits-to-symbol converter's input, one
 * byte per bit: every `generate*` member that transmits a message or a
 * training sequence scrambles into it first (`Scrambler::process` for a
 * real bit vector, `processAllOnes`/`processAllZeros` for training) and
 * then hands it to `V90BitsToSymbol::process`. Its length matches
 * `V90CP_BITS` exactly, because the longest thing it ever holds is a
 * scrambled copy of `V90CP`'s own bit vector.
 *
 * The tail past the buffer holds two parallel (bit vector, bit count,
 * sequence-length-in-symbols) triples, one for the MP message
 * (`mpBits`/`mpBitCount`/`mpSequenceSymbols`) and one shared by CP, SUVd and
 * FinalSUVd (`cpBits`/`cpBitCount`/`cpSequenceSymbols`), each rebuilt by
 * `6 * bitCount / groupSize` whenever its source message changes, plus the
 * remaining constructor arguments and a handful of fields whose role is
 * bounded but not established (kept under neutral `wordNNNN` names).
 *
 * `sessionFlag` at +0x0000 is named for the method that writes it and reads
 * exactly as `V90Phase3Modulator`'s and `V90Modulator`'s own +0x0000:
 * nonzero selects V.92.
 */

#ifndef DSPLIB_V90PHASE4MODULATOR_H
#define DSPLIB_V90PHASE4MODULATOR_H

#include "dsplib/Scrambler.h"		/* embedded at +0x58, 0x20 bytes */
#include "dsplib/V90Phase3Modulator.h"	/* for `PcmType`; see +0x0038 below */

class V90BitsToSymbol;
class V90CP;
class V90MP;
class V90MappingParams;
class V90Parameters;

/**
 * @brief The phase 4 modulator's own state enumeration.
 *
 * `Phase4ModulatorState` is the object's own type name: it appears in the
 * mangling of `setNextStateAfterTRN2d` and `reset`. The enumerator names are
 * not the object's and are derived one at a time; the derivation is beside
 * each, and the method used depends on what evidence exists for it:
 *
 * - Where a name comes from a string, that is the strongest evidence class
 *   this tree recognises (CLAUDE.md's evidence order, rule 1): the
 *   assignment (`movl $N,0x4(%reg)`) and the `edprintf` that names the
 *   state are in the same straight-line run with no branch between them, so
 *   the message and the store cannot be paired wrongly. Checked
 *   mechanically over all forty-five members of the class, not by eye.
 * - Where a name comes from a method name (0x00, 0x04, 0x08), the argument
 *   is that `exitX()` acts only when the field holds one particular value,
 *   corroborated rather than assumed: `exitMPNot()` acts only on 0x0e, and
 *   0x0e is independently named "enter MPNot" by a string. The same shape
 *   then reads `exitMP()` on 0x04 as MP and `exitRi()` on 0x00 as Ri;
 *   `enterRepeatedCPd()` assigns 0x08 and nothing else does under V.90.
 * - Where there is neither, the value keeps an offset name. Nine do. Eight
 *   are the same shape -- a state entered when the symbol counter is not
 *   yet on a sequence boundary, which goes on emitting what it was emitting
 *   and then hands on, the same role `V90Phase3Modulator`'s `_END` states
 *   play -- but naming them `_END` here would be usage inference dressed as
 *   a derivation, and finding F3120's rule says a wrong name is worse than
 *   an offset (`P4D_STATE_UNNAMED_11` is the precedent for the spelling).
 *
 * 0x05 is the one to be careful about: three different messages precede an
 * assignment of 5, and one of them ("CPd Terminated") names the state being
 * left rather than the one being entered. Two independent sites naming it
 * SUVd is what carries it, corroborated by `recivedSUV()` acting only when
 * the field is 5.
 *
 * 0x14 and 0x1c are real states even though no member stores or compares
 * them directly: `generateV90Symbol`'s jump table (`.rodata+0xa94`) runs
 * 0x00..0x1b and `generateV92Symbol`'s (`+0xb04`) runs 0x00..0x1e, and the
 * entry at 0x14 in both -- and at 0x1c in the second -- is a distinct arm
 * rather than the default edge, which makes it a real `case` label. 0x1f is
 * past the end of the larger table and mentioned nowhere else, so it stays
 * out; a C++ enumeration does not have to be contiguous, and inventing an
 * enumerator to make it look tidy would be inventing a name.
 *
 * The base is pinned signed, and that is measured, not stylistic:
 * `recivedCPtag`, `recivedSUVtag` and `recivedE2u` all dispatch with a
 * signed `jl`, which an all-non-negative enumeration would compile as the
 * unsigned `jb` instead -- a codegen difference at three sites. One
 * negative enumerator (`_BASE_PIN`, ours; the object names no such value)
 * makes the base signed and every `int` representable, the same fix as
 * `__tHardwareCodecTypes__`'s and `V90Phase4Demodulator`'s own state enum.
 */
enum Phase4ModulatorState {
	P4M_STATE_RI = 0x00,		/* `exitRi` acts only on 0         */
	P4M_STATE_UNNAMED_01 = 0x01,	/* `exitRi`'s non-boundary arm     */
	P4M_STATE_RI_NOT = 0x02,	/* "enter RiNot @ %d"              */
	P4M_STATE_TRN2D = 0x03,		/* "enter TRN2d @ %d"              */
	P4M_STATE_MP = 0x04,		/* `exitMP` acts only on 4         */
	P4M_STATE_SUVD = 0x05,		/* "enter SUVd @ %d"; see above    */
	P4M_STATE_UNNAMED_06 = 0x06,	/* `recivedSUV`'s non-boundary arm */
	P4M_STATE_CPD = 0x07,		/* "enter CPd @ %d"                */
	P4M_STATE_REPEATED_CPD = 0x08,	/* `enterRepeatedCPd` assigns it   */
	P4M_STATE_UNNAMED_09 = 0x09,	/* from CPd, not yet on a boundary */
	P4M_STATE_UNNAMED_0A = 0x0a,	/* from SUVd, likewise             */
	P4M_STATE_FINAL_SUVD = 0x0b,	/* "enter FinalSUVd @ %d"          */
	P4M_STATE_UNNAMED_0C = 0x0c,	/* `recivedCPtag`'s non-boundary   */
	P4M_STATE_UNNAMED_0D = 0x0d,	/* `exitMP`'s non-boundary arm     */
	P4M_STATE_MP_NOT = 0x0e,	/* "enter MPNot @ %d"              */
	P4M_STATE_UNNAMED_0F = 0x0f,	/* `exitMPNot`'s non-boundary arm  */
	P4M_STATE_ED = 0x10,		/* "enter Ed @ %d"                 */
	P4M_STATE_B1D = 0x11,		/* "enter B1d @ %d"                */
	P4M_STATE_TERMINATED = 0x12,	/* "Phase4 Terminated @ %d"        */
	P4M_STATE_UNNAMED_13 = 0x13,	/* both symbol pumps, no message   */
	/*
	 * 0x14  Both pumps dispatch it to `generateDataSymbolBeforeRRN`, and
	 * that member is where the "enter Rd @ %d" message and the move to
	 * 0x15 live.  Naming the state for the member would be one inferential
	 * step past what the dispatch proves, so it keeps the offset (3120).
	 */
	P4M_STATE_UNNAMED_14 = 0x14,
	P4M_STATE_RD = 0x15,		/* "enter Rd @ %d"                 */
	P4M_STATE_RD_NOT = 0x16,	/* "enter RdNot @ %d"              */
	P4M_STATE_UNNAMED_17 = 0x17,	/* tested by `exitSilence`, never  */
	P4M_STATE_UNNAMED_18 = 0x18,	/*   assigned anywhere in the class*/
	P4M_STATE_UNNAMED_19 = 0x19,	/* `exitSilence`'s non-boundary arm*/
	P4M_STATE_RT = 0x1a,		/* "enter Rt @ %d"                 */
	P4M_STATE_RT_NOT = 0x1b,	/* "enter RtNot @ %d"              */
	/*
	 * 0x1c  `generateV92Symbol` alone, and the V.90 pump's table stops one
	 * short of it.  It dispatches to `generateDataSymbolBeforeFPE`, which
	 * carries the "enter Rf @ %d" message and the move to 0x1d.  Same
	 * reading and same restraint as 0x14 above.
	 */
	P4M_STATE_UNNAMED_1C = 0x1c,
	P4M_STATE_RF = 0x1d,		/* "enter Rf @ %d"                 */
	P4M_STATE_RF_NOT = 0x1e,	/* "enter RfNot @ %d"              */
	P4M_STATE_BASE_PIN = -0x7fffffff - 1	/* ours; pins the base     */
};

typedef char v90p4m_state_is_signed[
    ((Phase4ModulatorState)-1 < (Phase4ModulatorState)0) ? 1 : -1];

/*
 * The scrambled-bit buffer's extent: +0x0078 up to the first field of the
 * tail. Numerically identical to `V90CP_BITS`, and spelled here rather than
 * shared because this header must not include `V90CP.h` (V90CP stays a
 * forward declaration). If the two ever disagree, this one is wrong: the
 * buffer receives a scrambled copy of `V90CP::bits`.
 */
#define V90P4M_BITS		0x2ee0	/* +0x0078 .. +0x2f57, 12,000 bytes */

/*
 * The two symbol tables' lengths, measured from their indexing rather than
 * counted off the stores: `generateRdRt`/`generateRdRtNot` index +0x2f68
 * with an unsigned-division-by-6 idiom on `(symbolCounter - 1)`, and
 * `generateRf`/`generateRfNot` index +0x2f74 the same way for 12.
 * `setRdRtSymbols` writes exactly six shorts and `setRfSymbols` exactly
 * twelve, and 6 * 2 lands the second array on +0x2f74 exactly.
 */
#define V90P4M_RDRT_SYMBOLS	6
#define V90P4M_RF_SYMBOLS	12

class V90Phase4Modulator {
public:
	/**
	 * @brief Construct the modulator: store the borrowed pointers and
	 *        flags, embed a `Scrambler` built for V.90's taps (18, 23),
	 *        and either adopt the supplied bits-to-symbol converter or
	 *        allocate and construct a private one of block size 0x140.
	 * @param params          The session's parameters (kept as `params`).
	 * @param sessionFlag     Nonzero selects V.92.
	 * @param bitsToSymbol    A converter to use, or null to have one
	 *                        allocated and owned by this object.
	 * @param mp              The MP message source.
	 * @param mappingParams   The primary constellation mapping.
	 * @param mappingParams2  The secondary constellation mapping (used by
	 *                        V.92's RdNot arm; see V90Phase4Modulator.cpp).
	 * @param cp              The CP message source.
	 * @param ctorArg8        Stored verbatim at +0x2f98; role not
	 *                        established by anything this class does with
	 *                        it.
	 */
	V90Phase4Modulator(V90Parameters *params, unsigned int sessionFlag,
			   V90BitsToSymbol *bitsToSymbol, V90MP *mp,
			   V90MappingParams *mappingParams,
			   V90MappingParams *mappingParams2, V90CP *cp,
			   unsigned int ctorArg8);

	/**
	 * @brief Release the bits-to-symbol converter, but only if this
	 *        object allocated it itself (`externalBitsToSymbol == 0`); a
	 *        supplied converter is left untouched.
	 */
	~V90Phase4Modulator();

	/** @brief Set `sessionFlag` (nonzero selects V.92). */
	void setSessionFlag(unsigned int flag);

	/**
	 * @brief Reinitialise the modulator for a new session: store the
	 *        companding law and expand `code` through it into
	 *        `codeLevel`, reset the state machine to `st` with the
	 *        symbol counter and per-repetition flags cleared, reset the
	 *        scrambler, and then run the phase 4 symbol pump (V.90's or
	 *        V.92's, per `sessionFlag`) for `nofSymbols` iterations to
	 *        warm it up.
	 * @param law         The companding law (A-law or mu-law).
	 * @param code        A G.711 code, expanded into `codeLevel`.
	 * @param st           The state to reset into.
	 * @param nofSymbols  How many warm-up symbols to pump.
	 * @param arg5        Stored verbatim into `word_0040`; not otherwise
	 *                    read by this class.
	 */
	void reset(PcmType law, unsigned char code, Phase4ModulatorState st,
		   unsigned int nofSymbols, unsigned int arg5);

	/*
	 * The rest of the class (V90Phase4Modulator.cpp's second half): six
	 * members read a symbol out of a table, two fill those tables, and
	 * the rest are the state machine's edges -- an `exitX` leaves a state
	 * once the symbol counter reaches the end of a repetition, and a
	 * `recivedX` is the demodulator's news arriving.
	 */
	/** @brief Return the next Rd/Rt table symbol (`(symbolCount-1) % 6`). */
	short generateRdRt();
	/** @brief `generateRdRt()`, negated. */
	short generateRdRtNot();
	/** @brief Return the next Rf table symbol (`(symbolCount-1) % 12`). */
	short generateRf();
	/** @brief `generateRf()`, negated. */
	short generateRfNot();

	/** @brief Return the Ri amplitude (`codeLevel`, sign pattern
	 *  +,+,+,-,-,- over a period of six symbols). */
	short generateRi();
	/** @brief `generateRi()`, negated. */
	short generateRiNot();

	/**
	 * @brief Ask the converter how many bits it needs; if it wants a
	 *        constant-ones run, scramble one and feed it, then drain and
	 *        return one symbol. Used for the B1d/TRN2d training states.
	 */
	short generateB1d();
	/** @copydoc generateB1d */
	short generateTRN2d();
	/**
	 * @brief Same as generateB1d(), but scrambles a constant-zeros run
	 *        (the Ed training state).
	 */
	short generateEd();

	/**
	 * @brief Drain one converter symbol for a data-mode state whose
	 *        boundary is not yet reached (FPE side); moves to Rf on the
	 *        converter's block-complete signal.
	 */
	short generateDataSymbolBeforeFPE();
	/**
	 * @brief Same as generateDataSymbolBeforeFPE(), moving to Rd instead
	 *        of Rf (RRN side).
	 */
	short generateDataSymbolBeforeRRN();

	/**
	 * @brief The five-byte sibling call whose whole body is
	 *        `recivedSUVtag()`; kept as a distinct member because the
	 *        object emits it as one (`generateB1d`'s cluster in the
	 *        blob's layout, not `recivedSUVtag`'s).
	 */
	void recivedPartTwoSilenceRrnSUVtag();

	/**
	 * @brief Ask the converter how many bits it needs; if nonzero,
	 *        scramble the MP message's bit vector and feed it, then
	 *        drain and return one symbol. Callerless in the object
	 *        (no relocation anywhere names it); reconstructed and left
	 *        that way rather than wired to a caller with no blob
	 *        behaviour to compare against.
	 */
	short generateMP();
	/**
	 * @brief Same as generateMP(), transmitting the CP message's bit
	 *        vector. Byte-identical to generateSUVd() in the object;
	 *        kept as two members because CPd and SUVd are two states.
	 *        Also callerless.
	 */
	short generateCPd();
	/** @copydoc generateCPd */
	short generateSUVd();

	/**
	 * @brief The V.90 phase 4 symbol pump: one `switch` over `state`
	 *        covering the MP ladder (no SUVd/CPd/silence/Rt/Rf states).
	 *        Increments `symbolCount`, dispatches on `state`, and
	 *        returns the symbol the active state produces.
	 */
	short generateV90Symbol();
	/**
	 * @brief The V.92 phase 4 symbol pump: the same shape as
	 *        generateV90Symbol() with the SUVd/CPd rate-renegotiation
	 *        ladder and the silence/Rt/Rf ladder in place of the MP
	 *        states (V90Phase4Modulator.cpp documents the seven
	 *        differences in full).
	 */
	short generateV92Symbol();

	/**
	 * @brief Dispatch to generateV92Symbol() or generateV90Symbol() by
	 *        `sessionFlag`, the same fork reset() makes for its warm-up
	 *        loop.
	 * @return The pumped symbol, widened from the callee's `short`.
	 */
	int generateSymbol();

	/**
	 * @brief Fill the six Rd/Rt symbols from a constellation's six
	 *        levels (last three negated), companded by `pcmType`.
	 */
	void setRdRtSymbols(V90MappingParams *mapping);
	/**
	 * @brief Fill the twelve Rf symbols by cycling the same six sources
	 *        twice with a different negation pattern, companded by
	 *        `pcmType`.
	 */
	void setRfSymbols(V90MappingParams *mapping);
	/** @brief Set `nextStateAfterTRN2d`. */
	void setNextStateAfterTRN2d(Phase4ModulatorState next);

	/**
	 * @brief Hand a constellation block to the bits-to-symbol converter
	 *        (`reset` then `setSymbolsBlockSize(1)`); a null block only
	 *        logs an error. Does not store the pointer anywhere in this
	 *        object.
	 * @return Unspecified: the object tail-jumps into
	 *         `setSymbolsBlockSize` on the live path and falls through
	 *         to a bare `ret` on the null path, so whatever is left in
	 *         the return register is incidental, not a designed value.
	 */
	void setMappingParams(V90MappingParams *mapping);

	/** @brief Clear the RRN-related latches ahead of an RRN sequence. */
	void resetBeforRRN();
	/** @brief The same clear one field along, plus the CP tag byte. */
	void resetRRNSecondSection();
	/** @brief Enter the repeated-CPd state, rebuilding the CP sequence. */
	void enterRepeatedCPd();

	/** @brief Leave MP for MPNot once the MP sequence completes. */
	void exitMP();
	/** @brief Leave MPNot for Ed once the MP sequence completes again. */
	void exitMPNot();
	/** @brief Leave Ri for RiNot once its six-symbol period completes. */
	void exitRi();
	/** @brief Leave the silence pair for Rt once its period completes. */
	void exitSilence();

	/** @brief Record that a CP tag has arrived (sets the CP latch). */
	void recivedCP();
	/**
	 * @brief The largest of the fifteen state-machine edges: acts only
	 *        while `word_0020` is set, and is the only member that sets
	 *        `cp->word_00` to select the short form of the CP message.
	 */
	void recivedCPtag();
	/** @brief Boundary-gated transition into Ed from an E2u arrival. */
	void recivedE2u();
	/** @brief `recivedE2u()` with its own "enter Ed first at RRN" message. */
	void recivedFirstRrnE2u();
	/**
	 * @brief The silence-pair exit and the SUVd-to-CPd transition, as
	 *        the two arms of one test on `state`.
	 */
	void recivedFirstSUVuPartTwoRrn();
	/** @brief Raise the CP tag byte ahead of a silence/RRN/SUV sequence. */
	void recivedPartOneSilenceRrnSUV();
	/** @brief `recivedFirstRrnE2u()`'s shape, with the CP tag raised first. */
	void recivedPartOneSilenceRrnSUVtag();
	/** @brief `recivedSUV()`'s body behind two extra (redundant) range
	 *  guards implied by the final state test; see V90Phase4Modulator.cpp. */
	void recivedPartTwoSilenceRrnSUV();
	/**
	 * @brief Boundary-gated transition from SUVd into CPd: rebuilds the
	 *        CP sequence and sets `word_2fa0` once.
	 */
	void recivedSUV();
	/** @brief Boundary-gated transition into Ed from SUVd or (Repeated)CPd. */
	void recivedSUVtag();

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	unsigned int sessionFlag;		/* +0x0000 argument 2      */

	/*
	 * +0x0004  The phase 4 state. Typed `Phase4ModulatorState` by
	 * `reset`'s third-argument mangling, not inferred from the constants
	 * stored into it (finding F4931).
	 */
	Phase4ModulatorState state;

	/*
	 * +0x0008  Symbols emitted since the current state was entered: what
	 * every one of the class's "@ %d" messages prints, cleared beside
	 * almost every assignment to `state`, and the dividend of the fifteen
	 * `divl` sites asking `symbolCount % cpSequenceSymbols` (`divl`, not
	 * `idivl`, is what makes it unsigned).
	 */
	unsigned int symbolCount;

	/*
	 * +0x000c  Written by three members (`reset` and both symbol pumps
	 * clear it on entry; `generateV92Symbol` also sets it to 4 on both
	 * TRN2d exits, and either pump sets it to 7 on "Phase4 Terminated")
	 * and read by none of the forty-five -- so whatever consumes it is
	 * outside this class.
	 *
	 * Named by the same sibling-class symmetry that already named
	 * `V90Phase3Modulator::eventCode` and `V90Phase3Demodulator::
	 * eventCode`: cleared on (almost) every state entry, set to a small
	 * constant only by the arms that have news, read by nothing inside
	 * the class, and copied into the caller's own `eventCode` field --
	 * `V90Modulator::progress` copies all three phase objects'
	 * `eventCode` the same way, in the same function. Naming the channel
	 * does not require naming every value on it: what 4 and 7 mean beyond
	 * "TRN2d exited" and "Phase4 Terminated" is still not established.
	 * Findings F7520 and F10140.
	 */
	unsigned int eventCode;

	/*
	 * +0x0010  Named for `setNextStateAfterTRN2d`, whose whole body is
	 * storing its argument here. `reset` seeds it branchlessly from
	 * `sessionFlag`: MP under V.90, SUVd under V.92 -- the two states
	 * TRN2d hands on to.
	 */
	Phase4ModulatorState nextStateAfterTRN2d;

	/*
	 * +0x0014  Cleared by `reset`; consumed (cleared, and `exitMPNot()`
	 * called) by both symbol pumps' `P4M_STATE_MP_NOT` arm; and set from
	 * OUTSIDE this class by `V90Modulator::acknowledgeCPNotReception` and
	 * `::acknowledgeEReception`, both printing "setting delayed MPNot
	 * exit" as they do it. So it is a request recorded while the state
	 * machine is off a repetition boundary, honoured the next time this
	 * class's own symbol pump revisits MPNot. Named directly from that
	 * message -- CLAUDE.md's strongest evidence class. Finding F10140.
	 */
	unsigned char delayedMpNotExit;

	/*
	 * +0x0015..+0x0017 was `pad_0015[3]`: `delayedMpNotExit` ends at
	 * +0x0015 and `word_0018` below is a 4-byte-aligned `unsigned int`, so
	 * natural alignment inserts exactly these three bytes with the member
	 * deleted -- proved by adding `V90P4_OFF(word_0018, 0x0018, w0018)`
	 * (already present, V90Phase4Modulator.cpp). Zero readers/writers
	 * anywhere in the object (`tools/dis.py` over every
	 * `V90Phase4Modulator::` member function, `0x2c5a0..0x2f730`); removed
	 * F10150.
	 */

	/*
	 * +0x0018 and +0x001c  Written together and only ever to zero, by
	 * `reset`, `resetRRNSecondSection`, `enterRepeatedCPd`,
	 * `recivedCPtag` and `recivedSUVtag`; read by neither those nor
	 * anything else in this file. The widths are the stores' (`movl` and
	 * `movb`). Nothing establishes a meaning, so they keep offset names.
	 */
	unsigned int word_0018;
	unsigned char byte_001c;

	/*
	 * +0x001d..+0x001f was `pad_001d[3]`: `byte_001c` ends at +0x001d and
	 * `word_0020` below is a 4-byte-aligned `unsigned int`, so natural
	 * alignment inserts exactly these three bytes with the member deleted
	 * -- proved by the existing `V90P4_OFF(word_0020, 0x0020, w0020)`.
	 * Zero readers/writers anywhere in the object (same sweep as above);
	 * removed F10150.
	 */

	/*
	 * +0x0020  Set to 1 on every path of `recivedCPtag`, `recivedE2u`,
	 * `recivedFirstRrnE2u`, `recivedPartOneSilenceRrnSUVtag` and
	 * `recivedSUVtag` that moves `state`, and tested at the top of the
	 * first four as a reason to do nothing. So it latches "this has
	 * already been acted on"; `reset`, `resetBeforRRN` and
	 * `resetRRNSecondSection` clear it. That bounds the role without
	 * establishing it -- nothing in the object says what "this" is -- so
	 * the name stays the offset's.
	 */
	unsigned int word_0020;

	/*
	 * +0x0024 .. +0x0034  `resetBeforRRN` writes all six (1 into +0x24,
	 * zero into the rest) and `reset` writes four of them; the reads are
	 * in members this batch has not written. Offset names, `movl` widths.
	 */
	unsigned int word_0024;
	unsigned int word_0028;
	unsigned int word_002c;
	unsigned int word_0030;
	unsigned int word_0034;

	/*
	 * +0x0038  The companding law. Typed `PcmType` (V90Phase3Modulator.h's
	 * enum) by `reset`'s first-argument mangling. Every read in this
	 * class is the same `!= 0` choosing A-law that header already
	 * measured.
	 */
	PcmType pcmType;

	/*
	 * +0x003c  The linear level of `reset`'s second argument, a G.711
	 * code: `reset` expands it through `alaw2linear`/`ulaw2linear` per
	 * `pcmType` and stores 16 bits of the result. `generateRi` returns it
	 * and `generateRiNot` returns its negation, both sign-extended, which
	 * is what makes it signed. Named for `V90Phase3Modulator::codeLevel`,
	 * the same field filled the same way by that class's own `reset`.
	 */
	short codeLevel;

	/*
	 * +0x003e..+0x003f was `pad_003e[2]`: `codeLevel` ends at +0x003e and
	 * `word_0040` below is a 4-byte-aligned `unsigned int`, so natural
	 * alignment inserts exactly these two bytes with the member deleted --
	 * proved by the existing `V90P4_OFF(word_0040, 0x0040, w0040)`. Zero
	 * readers/writers anywhere in the object (same sweep as above);
	 * removed F10150.
	 */

	/*
	 * +0x0040  `reset`'s fifth argument, stored and not otherwise touched
	 * by anything this batch wrote. `unsigned int` is the mangling's; the
	 * meaning is not established.
	 */
	unsigned int word_0040;

	V90BitsToSymbol *bitsToSymbol;		/* +0x0044 argument 3      */
	V90MP *mp;				/* +0x0048 argument 4      */
	V90MappingParams *mappingParams;	/* +0x004c argument 5      */
	V90MappingParams *mappingParams2;	/* +0x0050 argument 6      */
	V90CP *cp;				/* +0x0054 argument 7      */
	Scrambler<unsigned char, unsigned char> scrambler;
						/* +0x0058 32 bytes        */
	/*
	 * +0x0078  The scrambler's output and the bits-to-symbol converter's
	 * input, one byte per bit.  See V90P4M_BITS above for why the length
	 * is the whole span to +0x2f58 and what that claim rests on.
	 */
	unsigned char scrambledBits[V90P4M_BITS];

	/*
	 * +0x2f58  What `V90MP::getBitVector` returned.  Borrowed: it points
	 * into the `V90MP` at +0x0048 and nothing here frees it.
	 */
	unsigned char *mpBits;

	/*
	 * +0x2f5c  The length `V90MP::getBitVector` reported. `unsigned int`
	 * is the mangling's (`getBitVector` takes `unsigned int &`), and this
	 * field is what the reference is bound to.
	 */
	unsigned int mpBitCount;

	/*
	 * +0x2f60  `6 * mpBitCount / mp->groupSize`, the MP sequence's length
	 * in symbols.  Read only as `symbolCounter % mpSequenceSymbols` in
	 * `exitMP`, `exitMPNot` and `generateV90Symbol`; `divl` makes it
	 * unsigned.
	 */
	unsigned int mpSequenceSymbols;

	/*
	 * +0x2f64  Set to `bitsToSymbol->extraSymbols + 12` on entry to state
	 * 0x10 -- the state whose debug line is "enter Ed @ %d" -- by
	 * `exitMPNot` (+0x2c96b), `recivedCPtag` (+0x2cd28), `recivedE2u`
	 * (+0x2cdf7) and both symbol pumps, and read only as
	 * `symbolCounter == word_2f64`, which ends that state.  A deadline in
	 * symbols; what the twelve is is not established, so the name stays
	 * neutral.
	 */
	unsigned int word_2f64;

	/*
	 * +0x2f68  Six symbols, indexed `(symbolCounter - 1) % 6`.
	 * `setRdRtSymbols` fills them from a `V90MappingParams` through
	 * `alaw2linear`/`ulaw2linear`, negating the last three, and
	 * `generateRdRt`/`generateRdRtNot` return one.  `short` is forced:
	 * the loads are `movswl` whose 32-bit result is the return value
	 * (finding F613's case), and the `neg %eax` before the stores to
	 * +0x2f6e, +0x2f70 and +0x2f72 puts negative values in them.
	 */
	short rdRtSymbols[V90P4M_RDRT_SYMBOLS];

	/*
	 * +0x2f74  Twelve symbols, indexed `(symbolCounter - 1) % 12`, filled
	 * by `setRfSymbols` and read by `generateRf`/`generateRfNot`.  Same
	 * two proofs of the signed type.
	 */
	short rfSymbols[V90P4M_RF_SYMBOLS];

	/*
	 * +0x2f8c  What `V90CP::getBitVector` returned; the CP half of the
	 * same triple.  Borrowed from the `V90CP` at +0x0054.
	 */
	unsigned char *cpBits;

	/* +0x2f90  The length it reported; the reference's home. */
	unsigned int cpBitCount;

	/*
	 * +0x2f94  `6 * cpBitCount / cp->word_3ba8`, the CP sequence's length
	 * in symbols, and the most-read field in the class: fifteen `divl`
	 * sites take `symbolCounter % cpSequenceSymbols`.
	 */
	unsigned int cpSequenceSymbols;

	unsigned int ctorArg8;			/* +0x2f98 argument 8      */

	/*
	 * +0x2f9c  Zeroed by the constructor, by `reset`, by `resetBeforRRN`
	 * and by `resetRRNSecondSection`; set to 1 by `recivedCP`, and by
	 * `recivedCPtag` on the one path where it was still zero -- the path
	 * that also sets `cp->word_00` and the CP byte at +0x13 and rebuilds
	 * the CP sequence.  Read as a gate by `recivedCPtag` and
	 * `recivedSUVtag`.  A latch, and the object does not say for what, so
	 * it keeps the offset name; it was `cleared_2f9c`, which recorded
	 * only that the constructor cleared it.
	 */
	unsigned int word_2f9c;

	/*
	 * +0x2fa0  The same shape one step later: cleared by the same four,
	 * set to 1 by `recivedSUV`, `recivedPartTwoSilenceRrnSUV` and
	 * `recivedFirstSUVuPartTwoRrn` immediately after each has called
	 * `V90CP::infoToBits` and recomputed `cpSequenceSymbols`, and read by
	 * the last two as a reason not to do that twice.  Was `cleared_2fa0`.
	 */
	unsigned int word_2fa0;
	unsigned int externalBitsToSymbol;	/* +0x2fa4 1 = not ours    */
	V90Parameters *params;			/* +0x2fa8 argument 1      */
};

#endif /* DSPLIB_V90PHASE4MODULATOR_H */

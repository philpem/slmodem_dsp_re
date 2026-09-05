/**
 * @file V90Phase3Modulator.h
 * @brief V.90/V.92 phase 3 downstream symbol source.
 *
 * `V90Phase3Modulator` runs the modem's phase-3 training sequence outward:
 * Sd, its inversion, a scrambled all-ones tone, the Jd/JdPhase/DIL learning
 * sequences, and their timeouts. `generateV90Symbol()`/`generateV92Symbol()`
 * each return one linear-level symbol per call, advancing a sixteen-state
 * machine (state, symbolCount) shared between the two protocol variants.
 *
 * `V90Phase3Modulator` is not polymorphic: `tools/cppstruct.py` lists its
 * destructor with the `D1`/`D2` variants and no `D0`, which GCC only emits
 * for a virtual destructor, so offset 0 is a real member and there is no
 * vptr (finding F228 names the four classes where that is not true).
 *
 * The object is 920 bytes (0x398): the largest `this`-relative displacement
 * any method uses is +0x394, a one-byte store, so the object ends at 0x395
 * and rounds up to 0x398 for the four-byte members before it -- a
 * displacement is not a size (finding F229's last section) -- and the .cpp
 * asserts both the size and every offset below.
 *
 * Twenty-three members are declared and twelve are defined: `setSessionFlag`,
 * `resetDILGenerator`, `reset`, `generateV90Symbol`, `generateV92Symbol`,
 * `generateSymbol`, `generateDIL`, `exitDIL`, `exitJd`, `exitJdPhase`, the
 * constructor and the destructor. Everything else is declared for the
 * record and deliberately left undefined, because defining a method whose
 * callees are not written breaks the link for the entire test suite
 * (docs/v90cpp.md); nothing defined here calls an undefined one. Count the
 * defined set in the .cpp rather than trusting this paragraph -- it has
 * gone stale once already. `generateDIL` is the one defined member with no
 * caller in the object, deliberate on the vendor's part; see its
 * declaration below.
 *
 * The scrambler is a subobject, not a pointer: `reset` takes the address of
 * `this + 0x20` and passes it to `Scrambler<unsigned char, int>::reset`, and
 * the two `generate*Symbol` bodies pass the same address to `process`. Those
 * four weak template members are part of this batch even though
 * `tools/callgraph.py` does not list them; see include/dsplib/Scrambler.h.
 * The constructor does the same thing as its third statement: rather than
 * storing a pointer at +0x20, it takes `lea 0x20(%ebx),%edx` and hands that
 * to `Scrambler<unsigned char, int>::Scrambler`.
 *
 * The constructor and destructor are declared (finding F871): a union
 * holding a class with no default constructor and a non-trivial destructor
 * loses both of its own, and the fixtures for this class are exactly such
 * unions -- already paid for, since `Scrambler` carries its own constructor
 * and destructor and every one of those unions has the two-line empty pair
 * that restores them. The constructor has two callers in the blob, found by
 * scanning .text relocations rather than assumed: `V90Modulator::
 * V90Modulator` and, less obviously, `V90Phase3Demodulator::
 * V90Phase3Demodulator`, which builds and destroys one of these too
 * (finding F1258).
 *
 * Data member names below are invented and descriptive: the mangling
 * preserves method names and type names but never a data member's name
 * (finding F226). Where a field's purpose is not established by a function
 * this batch reconstructed, it carries an offset-derived name or is `pad_`.
 */

#ifndef DSPLIB_V90PHASE3MODULATOR_H
#define DSPLIB_V90PHASE3MODULATOR_H

#include "dsplib/Scrambler.h"
#include "dsplib/V90Jd.h"
#include "dsplib/V92Jd.h"

/**
 * @brief Forward declaration of the constructor's parameter type.
 *
 * `params` is stored and nothing else -- the whole use is `mov
 * 0x34(%esp),%eax; mov %eax,0x50(%ebx)` at .text+0x2c4ce -- so an incomplete
 * type is all this header needs, and forward-declaring it rather than
 * including V90Parameters.h keeps this header's include set as it was. The
 * tag is `class` to agree with include/dsplib/V90Parameters.h; the mangling
 * is `P13V90Parameters` either way.
 */
class V90Parameters;

/**
 * @brief The companding law in force.
 *
 * The mangling names the type; the enumerator names are invented. The
 * values are measured: `V90Phase3Modulator::reset` stores the argument at
 * +0x04 and every later test is `!= 0` choosing A-law, and
 * `calculateDilLength` indexes `codeSegmentsBoundriesLookupTable` at `8 *
 * pcmType` and separately compares the argument against the literal 1, so
 * the type has exactly the two values 0 and 1 and 1 is A-law.
 */
enum PcmType {
	PCM_TYPE_MU_LAW = 0,
	PCM_TYPE_A_LAW = 1
};

/**
 * @brief The modulator's phase 3 state.
 *
 * Sixteen values, 0 through 15: both `generate*Symbol` bodies dispatch on
 * the field at +0x14 and between them assign every one of 1, 2, 3, 5, 8, 9,
 * 11, 12, 13, 14 and 15 to it, with 0 reachable as the initial value
 * `reset` copies in.
 *
 * The names are the object's own, not invented. Each generating state's
 * body is inlined verbatim from one of the small `generate*` methods the
 * mangling already names, and matching the two is a byte comparison rather
 * than a guess:
 *
 *   0  `generateSd`      -- its six-entry .rodata jump table at 0x984 holds
 *                           exactly the sequence generateV90Symbol's own
 *                           table at 0x9f4 holds
 *   1  `generateSdNot`   -- likewise 0x99c against 0xa0c, the inversion
 *   2  `generateTRN1d`   -- process(1), sign selects +/- codeLevel
 *   3  `generateJd` (V.90, jdBits) / `generateV92Jd` (V.92, jdV92Bits)
 *   5  `generateJdPhase` -- jdV92PhaseBits; V.92 only
 *   8  `generateJdNot`   -- process(0)
 *   9  `generateDIL`
 *
 * and the four `exit*` methods name the rest.  `exitJd` moves state 3 to 7
 * under V.90 and to 4 under V.92; `exitJdPhase` moves 5 to 6; `exitDIL` moves
 * 9 to 10, or straight to 11 with the "Phase3 Terminated" message.  Those
 * three "_END" states carry on emitting the same thing until the symbol count
 * reaches a multiple of 72 -- the end of the current repetition -- and then
 * hand on.  The four terminal states are named after the message that enters
 * them: "Jd TimeOut", "V92JdPhase TimeOut", "DIL TimeOut", and the two
 * "ERROR: Null ..." sites.
 *
 * V.90 and V.92 do not use the same subset.  4, 5, 6 and 13 belong to V.92
 * and `generateV90Symbol` treats them as illegal; 7 belongs to V.90 and
 * `generateV92Symbol` treats it as illegal.
 */
enum Phase3ModulatorState {
	P3M_STATE_SD = 0,		/* six-symbol Sd pattern           */
	P3M_STATE_SD_NOT = 1,		/* its inversion                   */
	P3M_STATE_TRN1D = 2,		/* scrambled all-ones              */
	P3M_STATE_JD = 3,		/* Jd / V92Jd, with a timeout      */
	P3M_STATE_V92JD_END = 4,	/* V.92: V92Jd to the 72-boundary  */
	P3M_STATE_JD_PHASE = 5,		/* V.92: JdPhase, with a timeout   */
	P3M_STATE_JD_PHASE_END = 6,	/* V.92: JdPhase to the boundary   */
	P3M_STATE_JD_END = 7,		/* V.90: Jd to the 72-boundary     */
	P3M_STATE_JD_NOT = 8,		/* scrambled all-zeros             */
	P3M_STATE_DIL = 9,		/* DIL, with a timeout             */
	P3M_STATE_DIL_END = 10,		/* DIL to the end of its segment   */
	P3M_STATE_TERMINATED = 11,	/* "Phase3 Terminated @ %d"        */
	P3M_STATE_JD_TIMEOUT = 12,	/* "Jd TimeOut" / "V92Jd TimeOut"  */
	P3M_STATE_JD_PHASE_TIMEOUT = 13,/* "V92JdPhase TimeOut"            */
	P3M_STATE_DIL_TIMEOUT = 14,	/* "DIL TimeOut"                   */
	P3M_STATE_ERROR = 15		/* the two "ERROR: Null ..." sites */
};

/**
 * @brief The DIL (digital impairment learning) sequence descriptor, as
 * supplied to resetDILGenerator().
 *
 * The name is the original's, from the mangling of `resetDILGenerator` and
 * `calculateDilLength`; the layout is read out of the displacements those
 * two take off the pointer, and every field below is touched by one of
 * them.
 *
 * `dilCode` is sized 256 because `dilCount` is a byte and
 * `V90Phase3Modulator` gives the array it fills from `dilCode` exactly 512
 * bytes -- 256 shorts -- between +0x188 and +0x388. That makes 256 an upper
 * bound that is also the smallest one consistent with the modulator's own
 * layout; nothing in this batch reads past `dilCount` entries, so the
 * struct's total size is a lower bound rather than a measurement.
 */
struct tagV90DILdescriptor {
	unsigned char dilCount;		/* +0x000 entries in dilCode      */
	unsigned char seq1Length;	/* +0x001 bytes used of seq1      */
	unsigned char seq2Length;	/* +0x002 bytes used of seq2      */
	unsigned char seq1[128];	/* +0x003                         */
	unsigned char seq2[128];	/* +0x083                         */
	unsigned char segmentSize[8];	/* +0x103 length code per segment */
	unsigned char segmentCode[8];	/* +0x10b PCM code per segment    */
	unsigned char dilCode[256];	/* +0x113 PCM code per DIL entry  */
};

class V90Phase3Modulator {
public:
	/**
	 * @brief Construct a phase 3 modulator idle in the Sd state.
	 *
	 * Builds the `Scrambler<unsigned char, int>` subobject with V.90's
	 * taps `(18, 23)` and a history buffer 99 bytes deep -- the same pair
	 * `V90Phase3Demodulator` gives its descrambler -- stores @p params
	 * and @p sessionFlag, and then calls reset() with mu-law, code 0x40,
	 * #P3M_STATE_SD, zero warm-up symbols and every pointer NULL, so the
	 * warm-up loop never runs and resetDILGenerator() clears `dilCount`
	 * without touching the DIL tables.
	 *
	 * `sessionFlag` must be stored before reset() runs: reset() branches
	 * on it to choose which bit-vector pointers to write, so storing it
	 * afterwards would take the wrong arm for every nonzero flag -- caught
	 * by a mutation in test/mutations/v90p3mod.json.
	 *
	 * @param params       The owning modem's parameter block; stored and
	 *                     never read by this class (finding F1257).
	 * @param sessionFlag  Nonzero selects V.92 message content and the
	 *                     V.92 generator over V.90's.
	 */
	V90Phase3Modulator(V90Parameters *params, unsigned int sessionFlag);

	/**
	 * @brief Destroy a phase 3 modulator.
	 *
	 * The body is empty; the scrambler subobject's non-trivial destructor
	 * is what actually frees its history buffer, appended implicitly by
	 * the compiler. Nothing else here owns memory.
	 */
	~V90Phase3Modulator();

	/** @brief Set `sessionFlag`, selecting V.92 message content when nonzero. */
	void setSessionFlag(unsigned int flag);

	/**
	 * @brief Load a DIL sequence descriptor and expand it into linear
	 * levels ready for generateDIL() to step through.
	 *
	 * Copies the two bit sequences verbatim, turns each segment length
	 * code into `6 * size + 6` symbols, and compands every PCM code
	 * (segment levels and the DIL table) to a linear level using the law
	 * already set in `pcmType`. Resets all four DIL cursors to zero and
	 * computes the initial `segmentIndex` from `dilLevel[0]`.
	 *
	 * @param d  The descriptor to load, or NULL to clear `dilCount` and
	 *           leave every other DIL field untouched -- the whole of
	 *           this function's error handling.
	 */
	void resetDILGenerator(const tagV90DILdescriptor *d);

	/**
	 * @brief (Re)start phase 3 transmission, optionally running it
	 * forward by a warm-up count.
	 *
	 * Resets the scrambler, stores the companding law and initial state,
	 * computes `codeLevel`/`codeLevelAlt`/`idleLevel` from @p code by the
	 * law in force, installs the Jd/JdPhase bit-vector pointers `jd` or
	 * `jd92` provide (whichever `sessionFlag` selects -- only the V.92
	 * path also nulls both of its pointers when `jd92` is NULL), calls
	 * resetDILGenerator() with @p d, and then generates @p nSymbols
	 * symbols immediately through whichever of generateV90Symbol() /
	 * generateV92Symbol() `sessionFlag` selects.
	 *
	 * @param law       The companding law for every level this call
	 *                  computes.
	 * @param code      The Jd/TRN1d tone's PCM code; `code + 0x10` gives
	 *                  `codeLevelAlt`.
	 * @param st        The state to start in (normally #P3M_STATE_SD).
	 * @param nSymbols  Symbols to generate immediately as a warm-up.
	 * @param jd        V.90 Jd bit vector source, or NULL.
	 * @param jd92      V.92 Jd/JdPhase bit vector source, or NULL.
	 * @param d         DIL descriptor to load, or NULL.
	 * @param base      `timeoutBase`: the symbol count the Jd and DIL
	 *                  timeouts are measured from.
	 */
	void reset(PcmType law, unsigned char code, Phase3ModulatorState st,
		   unsigned int nSymbols, V90Jd *jd, V92Jd *jd92,
		   const tagV90DILdescriptor *d, unsigned int base);

	/**
	 * @brief Produce one downstream symbol of the V.90 phase 3 sequence
	 * and advance the state machine.
	 *
	 * Increments `symbolCount`, dispatches on `state` through Sd, SdNot,
	 * TRN1d, Jd (with `jdBits`), JdNot and DIL, and moves to the next
	 * state when each one's boundary is reached. States 4, 5, 6 and 13
	 * belong to V.92 and are treated as illegal here.
	 *
	 * @return The symbol, as a linear PCM level.
	 */
	int generateV90Symbol();

	/**
	 * @brief Produce one downstream symbol of the V.92 phase 3 sequence
	 * and advance the state machine.
	 *
	 * The same machine as generateV90Symbol(), with the V.92 message
	 * sequence in place of V.90's: states 3/4 carry `jdV92Bits` where
	 * V.90's 3/7 carry `jdBits`, and states 5/6 (`jdV92PhaseBits`) have no
	 * V.90 equivalent at all. State 7 belongs to V.90 and is treated as
	 * illegal here. The JdPhase timeout is an absolute 24804 symbols with
	 * no `timeoutBase` term, unlike the Jd timeout two states earlier.
	 *
	 * @return The symbol, as a linear PCM level.
	 */
	int generateV92Symbol();

	/**
	 * @brief Produce one downstream symbol through whichever generator
	 * `sessionFlag` selects.
	 * @return The symbol, as a linear PCM level.
	 */
	int generateSymbol();

	/**
	 * @brief Leave the DIL state, ending phase 3 or running the current
	 * segment out.
	 *
	 * A no-op unless `state` is #P3M_STATE_DIL and at least one DIL
	 * symbol has been sent. Mid-segment (`segmentPos != 0`) moves to
	 * #P3M_STATE_DIL_END to finish the segment; on a segment boundary,
	 * phase 3 is over: logs "Phase3 Terminated", moves to
	 * #P3M_STATE_TERMINATED, and raises event code 6 -- the only one of
	 * the four `exit*` methods that raises an event.
	 */
	void exitDIL();

	/**
	 * @brief Produce one symbol of the DIL learning sequence and step its
	 * four cursors.
	 *
	 * This is the one member of the class the blob defines but never
	 * calls (zero relocations name it anywhere in the 1.2 MB object): it
	 * is inlined verbatim into both generateV90Symbol() and
	 * generateV92Symbol(), which is reproduced here by defining the
	 * method and calling it from both, letting the compiler inline it at
	 * every call site while still emitting the standalone symbol. No
	 * caller or dispatch arm may be added beyond those two inlined call
	 * sites -- a source call the compiler declined to inline would leave
	 * a relocation the object does not have.
	 *
	 * `seq2[seq2Index] == 0` selects the segment's own boundary level
	 * over the DIL table entry; `seq1[seq1Index] == 0` negates it. The
	 * PCM code written to `dilPcmCode` always reflects the DIL table
	 * entry, independent of which level was actually emitted. Reaching
	 * the end of the current segment (`segmentPos + 1 ==
	 * segmentLength[segmentIndex]`) resets all three sequence cursors,
	 * advances `dilIndex` modulo `dilCount`, and recomputes
	 * `segmentIndex` from the new DIL level.
	 *
	 * @return The symbol, as a linear PCM level.
	 */
	int generateDIL();

	/*
	 * The eight leaf methods below have `int` return -- not mangled, and
	 * measured off their standalone bodies: each ends by widening a
	 * short into %eax itself (`movswl`), which a `void` or
	 * caller-widened `short` return would not. Each is a real symbol the
	 * blob also inlines verbatim into generateV90Symbol()/
	 * generateV92Symbol(); this tree reproduces both by defining the
	 * method and calling it from the file-static helper the two
	 * generators share, which the compiler inlines back at every call
	 * site.
	 */

	/** @brief One symbol of the six-symbol Sd pattern. @return The symbol, as a linear PCM level. */
	int generateSd();
	/** @brief One symbol of Sd's inversion. @return The symbol, as a linear PCM level. */
	int generateSdNot();
	/** @brief One scrambled, differentially-encoded symbol of the V.90 Jd vector (`jdBits`). @return The symbol, as a linear PCM level. */
	int generateJd();
	/** @brief One scrambled, differentially-encoded symbol with the scrambler fed a constant 0 bit. @return The symbol, as a linear PCM level. */
	int generateJdNot();
	/** @brief One scrambled, differentially-encoded symbol of the V.92 JdPhase vector (`jdV92PhaseBits`). @return The symbol, as a linear PCM level. */
	int generateJdPhase();
	/** @brief One scrambled, differentially-encoded symbol of the V.92 Jd vector (`jdV92Bits`). @return The symbol, as a linear PCM level. */
	int generateV92Jd();
	/**
	 * @brief One symbol of the scrambled all-ones tone.
	 *
	 * Unlike the other scrambled symbols, the scrambler's raw output bit
	 * picks the sign directly -- `polarity` is neither read nor written.
	 * @return The symbol, as a linear PCM level.
	 */
	int generateTRN1d();
	/** @brief Recompute `segmentIndex` from the DIL level at `dilLevel[dilIndex]`. */
	void updateCodeSegmentPointer();

	/**
	 * @brief Leave the Jd state, ending the current 72-symbol repetition
	 * or handing straight on.
	 *
	 * A no-op unless `state` is #P3M_STATE_JD and at least one Jd symbol
	 * has been sent. Off a 72-symbol boundary, moves to the matching
	 * "_END" state (#P3M_STATE_JD_END for V.90, #P3M_STATE_V92JD_END for
	 * V.92) to finish the repetition; on a boundary, skips straight to
	 * the next state (#P3M_STATE_JD_NOT for V.90, #P3M_STATE_JD_PHASE for
	 * V.92). `sessionFlag` selects which pair of states applies.
	 */
	void exitJd();

	/**
	 * @brief Leave the JdPhase state (V.92 only), ending the current
	 * 72-symbol repetition or handing straight on.
	 *
	 * A no-op unless `state` is #P3M_STATE_JD_PHASE and at least one
	 * JdPhase symbol has been sent. Off a boundary, moves to
	 * #P3M_STATE_JD_PHASE_END; on a boundary, straight to
	 * #P3M_STATE_JD_NOT. JdPhase has only one successor, unlike exitJd().
	 */
	void exitJdPhase();

	/**
	 * @brief The G.711 segment boundaries, as two rows of eight indexed
	 * by PcmType.
	 *
	 * Row 0 (mu-law) is `124 + 256 * (2**k - 1)`; row 1 (A-law) is
	 * `256 << k` -- the same 16-bit-scale endpoints `ulaw2linear()`/
	 * `alaw2linear()` produce. A defined data symbol in the blob (`D`),
	 * so it must exist here or the whole suite fails to link; signed
	 * `int` because the comparison against it is `jle` and the last
	 * A-law entry, 32768, does not fit a short.
	 */
	static int codeSegmentsBoundriesLookupTable[2][8];

	/* --- data members; see the file comment on the naming --- */

	unsigned int sessionFlag;	/* +0x000 nonzero selects V.92     */
	PcmType pcmType;		/* +0x004                          */

	/*
	 * The symbol count the two long timeouts are measured from: the Jd
	 * state fires at `timeoutBase + 24804` and the DIL state at
	 * `timeoutBase + 40000`, and it is used for nothing else.
	 */
	unsigned int timeoutBase;	/* +0x008 reset's last argument    */

	short codeLevel;		/* +0x00c linear level of the code */
	short codeLevelAlt;		/* +0x00e ... and of code + 0x10   */
	/*
	 * +0x010..+0x011  NOT REMOVABLE under the pad-removal workstream
	 * (F10150): `codeLevelAlt` ends at +0x010, already 2-byte (and
	 * 4-byte) aligned, and `idleLevel` below needs only 2-byte alignment
	 * -- a field-to-field gap here would be 0 bytes, not 2, if the member
	 * vanished.  The compiler's own implicit padding does not reproduce
	 * this span (confirmed: deleting it and recompiling makes the
	 * existing `P3M_OFF(idleLevel, 0x012, idlelevel)` assertion fail),
	 * same shape as `VPcmFloModem::pad_6fb8` -- stays explicit.
	 */
	unsigned char pad_10[2];	/* +0x010                          */
	short idleLevel;		/* +0x012 linear level of silence  */
	Phase3ModulatorState state;	/* +0x014                          */
	unsigned int symbolCount;	/* +0x018 counts within a state    */

	/*
	 * Written on every path of both `generate*Symbol`, zero unless this
	 * symbol was the last of a state: 1 entering TRN1d, 2 entering Jd, 3
	 * entering JdPhase, 6 entering the terminated state -- which is also
	 * the one `exitDIL` sets. It is the caller's per-symbol notification
	 * and nothing reads it here.
	 */
	unsigned int eventCode;		/* +0x01c                          */

	Scrambler<unsigned char, int> scrambler;	/* +0x020 32 bytes */

	/*
	 * The differential encoder's running sign. Every scrambled state
	 * does `polarity ^= scrambler.process(bit)` and then emits
	 * `polarity ? codeLevel : -codeLevel`; TRN1d seeds it from the sign
	 * of the symbol it ends on.
	 */
	unsigned int polarity;		/* +0x040                          */

	unsigned char *jdBits;		/* +0x044 V90Jd::getBitVector()    */
	unsigned char *jdV92Bits;	/* +0x048 V92Jd::getJdBitVector()  */
	unsigned char *jdV92PhaseBits;	/* +0x04c ...getJdPhaseBitVector() */

	/*
	 * The constructor's `V90Parameters *`. No symbol of this class reads
	 * it back -- confirmed by disassembling all nineteen -- so what, if
	 * anything, reads it from outside was not looked for (finding F1257).
	 */
	V90Parameters *params;		/* +0x050                          */

	/* The DIL generator's copy of the descriptor, expanded to levels. */
	unsigned char dilCount;		/* +0x054                          */
	unsigned char seq1Length;	/* +0x055                          */
	unsigned char seq2Length;	/* +0x056                          */
	unsigned char seq1[128];	/* +0x057                          */
	unsigned char seq2[128];	/* +0x0d7                          */

	/*
	 * +0x157 was `pad_157[1]`: `seq2` ends at +0x157 and `segmentLength`
	 * below is a 4-byte-aligned `unsigned int[]`, so natural alignment
	 * inserts exactly this one byte with the member deleted -- the
	 * existing `P3M_OFF(segmentLength, 0x158, segmentlength)`
	 * (V90Phase3Modulator.cpp) is what proves it. Zero readers/writers
	 * anywhere in the object (`tools/dis.py` over every
	 * `V90Phase3Modulator::` member function, `0x2ac10..0x2c5a0`); removed
	 * F10150.
	 */
	unsigned int segmentLength[8];	/* +0x158 6 * size + 6             */
	short segmentLevel[8];		/* +0x178                          */
	short dilLevel[256];		/* +0x188                          */

	/*
	 * The DIL generator's four cursors, all established by the two DIL
	 * states. `seq1Index` and `seq2Index` step through `seq1` and `seq2`
	 * modulo their lengths -- seq1 chooses the sign of the level, seq2
	 * chooses between the DIL level and the segment level. `dilIndex`
	 * steps through `dilLevel` modulo `dilCount`, one step per segment.
	 * `segmentPos` counts symbols within the current segment and is
	 * compared against `segmentLength[segmentIndex]`; reaching it clears
	 * all four of the first three and advances `dilIndex`.
	 */
	unsigned char seq1Index;	/* +0x388                          */
	unsigned char seq2Index;	/* +0x389                          */
	unsigned char dilIndex;		/* +0x38a                          */

	/*
	 * +0x38b was `pad_38b[1]`: `dilIndex` ends at +0x38b and `segmentPos`
	 * below is a 4-byte-aligned `unsigned int`, so natural alignment
	 * inserts exactly this one byte with the member deleted -- the
	 * existing `P3M_OFF(segmentPos, 0x38c, segmentpos)` is what proves it.
	 * Zero readers/writers anywhere in the object (same sweep as above);
	 * removed F10150.
	 */
	unsigned int segmentPos;	/* +0x38c                          */

	unsigned char segmentIndex;	/* +0x390 row index into the table */

	/*
	 * +0x391 was `pad_391[1]`: `segmentIndex` ends at +0x391 and
	 * `usingSegmentLevel` below is a 2-byte-aligned `short`, so natural
	 * alignment inserts exactly this one byte with the member deleted --
	 * the existing `P3M_OFF(usingSegmentLevel, 0x392, usingsegmentlevel)`
	 * is what proves it. Zero readers/writers anywhere in the object (same
	 * sweep as above); removed F10150.
	 */

	/*
	 * Whether this DIL symbol took its level from `segmentLevel` rather
	 * than from `dilLevel` -- the same `seq2[seq2Index] == 0` test that
	 * chose it, stored again as a short.
	 */
	short usingSegmentLevel;	/* +0x392                          */

	/*
	 * The G.711 code of `dilLevel[dilIndex]`, companded by the law in
	 * force and with the sign/company bits stripped the same way
	 * `resetDILGenerator` supplies them -- `^ 0xd5` for A-law, `~` for
	 * mu-law. Written on every DIL symbol whatever level was emitted.
	 */
	unsigned char dilPcmCode;	/* +0x394                          */

	/*
	 * +0x395..+0x397 was `pad_395[3]`: this is the class's own TAIL
	 * padding, forced by the struct's alignment (its most-aligned member
	 * is a 4-byte `unsigned int`) rounding `sizeof` up from 0x395 to the
	 * next multiple of 4, 0x398 -- with the member deleted, the compiler
	 * inserts this same tail implicitly, proved by the existing
	 * `typedef char v90p3m_size[(sizeof(V90Phase3Modulator) == 0x398) ?
	 * 1 : -1]` (V90Phase3Modulator.cpp). Zero readers/writers anywhere in
	 * the object (same sweep as above); removed F10150.
	 */
};

#endif /* DSPLIB_V90PHASE3MODULATOR_H */

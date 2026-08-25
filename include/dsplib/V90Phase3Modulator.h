/*
 * V90Phase3Modulator.h -- the V.90 / V.92 phase 3 downstream symbol source.
 *
 * Reconstructed from dsplibs.o.  `V90Phase3Modulator` is NOT polymorphic --
 * tools/cppstruct.py lists its destructor with the `D1` and `D2` variants and
 * no `D0`, and GCC emits a deleting destructor only for a virtual one -- so
 * offset 0 is a real member and there is no vptr.  Finding F228 is the four
 * classes where that is not true.
 *
 * THE OBJECT IS 920 BYTES.  The largest `this`-relative displacement any
 * method uses is +0x394 and the store there is one byte, so the object ends
 * at 0x395 and rounds up to 0x398 for the four-byte members before it.  A
 * displacement is not a size (finding F229's last section); the .cpp asserts
 * both the size and every offset below.
 *
 * Twenty-three members are declared and TWELVE are defined -- count them in
 * the .cpp rather than here, because this sentence has gone stale once
 * already (it read SEVEN while eleven were defined): `setSessionFlag`,
 * `resetDILGenerator`, `reset`, `generateV90Symbol`, `generateV92Symbol`,
 * `generateSymbol`, `generateDIL`, `exitDIL`, `exitJd`, `exitJdPhase`, the
 * constructor and the destructor.
 * Everything else is declared for the record and deliberately left undefined,
 * because defining a method whose callees are not written breaks the link for
 * the entire test suite (docs/v90cpp.md).  Nothing defined here calls an
 * undefined one.
 *
 * `generateDIL` IS THE ONE DEFINED MEMBER WITH NO CALLER IN THE OBJECT, and
 * that is deliberate on the vendor's part rather than an accident of ours;
 * see its declaration below.
 *
 * THE SCRAMBLER IS A SUBOBJECT, NOT A POINTER.  `reset` takes the address of
 * `this + 0x20` and passes it to `Scrambler<unsigned char, int>::reset`, and
 * the two `generate*Symbol` bodies pass the same address to `process`.  Those
 * four weak template members are part of this batch even though
 * `tools/callgraph.py` does not list them; see include/dsplib/Scrambler.h.
 * The constructor is the third statement of the same thing: it does not store
 * a pointer at +0x20, it takes `lea 0x20(%ebx),%edx` and hands that to
 * `Scrambler<unsigned char, int>::Scrambler`.
 *
 * THE CONSTRUCTOR AND THE DESTRUCTOR ARE NOW DECLARED, and this file used to
 * say the opposite.  The reason it gave was finding F871's, and finding F871
 * split it in two: the UNION half was real -- a union holding a class with no
 * default constructor and a non-trivial destructor loses both of its own, and
 * the fixtures for this class are exactly such unions -- and it is already
 * paid for, because `Scrambler` acquired a constructor and a destructor first
 * and every one of those unions carries the two-line empty pair that restores
 * them.  The `__builtin_offsetof` half was STALE: `offsetof` wants STANDARD
 * LAYOUT, which a user-provided constructor does not affect, and every
 * assertion in the .cpp still compiles.  So the old reasoning costs nothing
 * to drop, and it has to be dropped: the constructor has callers, so the
 * symbol must exist for their link closures to close.  There are TWO, taken
 * by scanning the blob's .text relocations rather than assumed --
 * `V90Modulator::V90Modulator` and, less obviously,
 * `V90Phase3Demodulator::V90Phase3Demodulator`, which builds and destroys one
 * of these as well (finding F1258).
 *
 * Data member names below are invented and descriptive: the mangling
 * preserves method names and type names but never a data member's name
 * (finding F226).  Where a field's purpose is not established by a function
 * this batch reconstructed, it carries an offset-derived name or is `pad_`.
 */

#ifndef DSPLIB_V90PHASE3MODULATOR_H
#define DSPLIB_V90PHASE3MODULATOR_H

#include "dsplib/Scrambler.h"
#include "dsplib/V90Jd.h"
#include "dsplib/V92Jd.h"

/*
 * The constructor's first parameter.  It is STORED AND NOTHING ELSE -- the
 * whole use is `mov 0x34(%esp),%eax; mov %eax,0x50(%ebx)` at .text+0x2c4ce --
 * so an incomplete type is all this header needs, and forward-declaring it
 * rather than including V90Parameters.h keeps this header's include set as it
 * was.  The tag is `class` to agree with include/dsplib/V90Parameters.h; the
 * mangling is `P13V90Parameters` either way.
 */
class V90Parameters;

/*
 * The companding law in force.  The mangling names the type; the enumerator
 * names are invented.  The VALUES are measured: `V90Phase3Modulator::reset`
 * stores the argument at +0x04 and every later test is `!= 0` choosing
 * A-law, and `calculateDilLength` indexes `codeSegmentsBoundriesLookupTable`
 * at `8 * pcmType` and separately compares the argument against the literal
 * 1, so the type has exactly the two values 0 and 1 and 1 is A-law.
 */
enum PcmType {
	PCM_TYPE_MU_LAW = 0,
	PCM_TYPE_A_LAW = 1
};

/*
 * The modulator's phase 3 state.  Sixteen values, 0 through 15: both
 * `generate*Symbol` bodies dispatch on the field at +0x14 and between them
 * assign every one of 1, 2, 3, 5, 8, 9, 11, 12, 13, 14 and 15 to it, with 0
 * reachable as the initial value `reset` copies in.
 *
 * THE NAMES ARE THE OBJECT'S OWN, not invented.  Each generating state's body
 * is inlined verbatim from one of the small `generate*` methods the mangling
 * already names, and matching the two is a byte comparison rather than a
 * guess:
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

/*
 * The DIL (digital impairment learning) sequence descriptor.  The name is the
 * original's, from the mangling of `resetDILGenerator` and
 * `calculateDilLength`; the layout is read out of the displacements those two
 * take off the pointer, and every field below is touched by one of them.
 *
 * `dilCode` is sized 256 because `dilCount` is a byte and
 * `V90Phase3Modulator` gives the array it fills from `dilCode` exactly 512
 * bytes -- 256 shorts -- between +0x188 and +0x388.  That makes 256 an upper
 * bound that is also the smallest one consistent with the modulator's own
 * layout; nothing in this batch reads past `dilCount` entries, so the struct's
 * total size is a lower bound rather than a measurement.
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
	/*
	 * .text+0x2c4a0, 123 bytes.  Three statements and no branch:
	 *
	 *   - the `Scrambler<unsigned char, int>` subobject at +0x20 is built
	 *     `(0x12, 0x17, 0x63)` -- V.90's taps 18 and 23, and the same 99
	 *     `V90Phase3Demodulator` gives its descrambler;
	 *   - `params` (+0x50) and then `sessionFlag` (+0x00) are stored;
	 *   - `reset(PCM_TYPE_MU_LAW, 0x40, P3M_STATE_SD, 0, NULL, NULL,
	 *     NULL, 0)`.
	 *
	 * THE ORDER OF THE LAST TWO IS FORCED, not a reading of the store
	 * order: `reset` branches on `sessionFlag` to choose which of the
	 * three bit-vector pointers it writes, so a constructor that stored
	 * the flag afterwards would take the wrong arm for every nonzero
	 * argument.  That is the mutation `store the session flag after
	 * reset` in test/mutations/v90p3mod.json, and it is caught.
	 *
	 * `nSymbols` is 0, so the warm-up loop never runs and neither
	 * generator is reachable from here; `d` is NULL, so
	 * `resetDILGenerator` clears `dilCount` and returns without touching
	 * any of the 800-odd DIL bytes.
	 */
	V90Phase3Modulator(V90Parameters *, unsigned int);

	/*
	 * .text+0x2ac30, 22 bytes, AND IT IS NOT EMPTY: the whole body is
	 * `Scrambler<unsigned char, int>::~Scrambler` called on `this + 0x20`,
	 * which is what the compiler emits for a destructor whose own body is
	 * empty over a class with one non-trivially-destructible member.  It
	 * frees the scrambler's history buffer and nothing else.
	 */
	~V90Phase3Modulator();

	/* The five that batch 2 defines. */
	void setSessionFlag(unsigned int);
	void resetDILGenerator(const tagV90DILdescriptor *);
	void reset(PcmType, unsigned char, Phase3ModulatorState, unsigned int,
		   V90Jd *, V92Jd *, const tagV90DILdescriptor *,
		   unsigned int);

	/*
	 * These two RETURN the symbol, as a linear level.  A return type is
	 * not mangled, so it has to be measured, and `int` is what the object
	 * says: every path ends `movswl %si,%esi; mov %esi,%eax`, and the
	 * one-line `generateTRN1d` spells the same conversion as a bare
	 * `cwtl` before its `ret`.  A `short` return would need neither.  The
	 * value itself is a `short` throughout -- each negation is taken
	 * modulo 2**16 before the widening.
	 */
	int generateV90Symbol();
	int generateV92Symbol();

	/*
	 * The dispatcher over the two above, and `int` for the same reason
	 * they are: it ends `cwtl; ret`.  It sign-extends the callee's value
	 * a SECOND time, which is one instruction our build does not emit --
	 * the original declared the two it calls as returning `short` where
	 * this tree declares them `int` (see just above), and the extension
	 * is a consequence of that recorded choice rather than of behaviour.
	 * Both spellings return the same number: the callees' own `cwtl` has
	 * already made the value a sign-extended short.
	 */
	int generateSymbol();

	/*
	 * The one state exit that carries a message.  Returns nothing: the
	 * object's four exits are the object's four exits, and `exitDIL`
	 * ends with a bare `ret` and eax carrying whatever the last store
	 * left there.
	 */
	void exitDIL();

	/*
	 * `generateDIL` IS DEFINED, and it is the one member of this class
	 * that is defined without being called.  The blob has it as a `T`
	 * symbol that nothing reaches -- zero relocations name it anywhere in
	 * the 1.2 MB object -- while inlining the same body into the two
	 * symbol generators, and .cpp reproduces both halves by defining the
	 * method and calling it from the generators, where GCC inlines it.
	 * NO CALLER AND NO DISPATCH ARM MAY BE ADDED: a call the compiler
	 * declined to inline would put a relocation on the symbol that the
	 * object does not have.
	 *
	 * `int` for the reason `generateV90Symbol` is, and the .cpp carries
	 * the derivation: the value is a `short` throughout -- the negation is
	 * taken modulo 2**16 -- and is then widened back to thirty-two bits
	 * by `movswl %cx,%edi` for the `mov %edi,%eax` that returns it.  A
	 * `short` return leaves the upper half of %eax dead and would not
	 * need that widening.
	 */
	int generateDIL();

	/*
	 * Declared, not defined -- see the file comment.  A return type is not
	 * mangled, so it is unknown for all of them.
	 */
	void generateSd();
	void generateSdNot();
	void generateJd();
	void generateJdNot();
	void generateJdPhase();
	void generateV92Jd();
	void generateTRN1d();
	void updateCodeSegmentPointer();
	void exitJd();
	void exitJdPhase();

	/*
	 * The G.711 segment boundaries, as two rows of eight indexed by
	 * `PcmType`: row 0 is 124 + 256 * (2**k - 1) and row 1 is 256 << k.
	 * A defined data symbol in the blob, so it is renamed `ref_*` in the
	 * reference object and must exist on our side or nothing links.
	 * Signed `int` because the comparison against it is `jle` and because
	 * the last entry, 32768, does not fit a short.
	 */
	static int codeSegmentsBoundriesLookupTable[2][8];

	/* --- data members; see the file comment on the naming --- */

	unsigned int sessionFlag;	/* +0x000 nonzero selects V.92     */
	PcmType pcmType;		/* +0x004                          */

	/*
	 * The symbol count the two long timeouts are measured from: the Jd
	 * state fires at `timeoutBase + 24804` and the DIL state at
	 * `timeoutBase + 40000`, and it is used for nothing else.  Was
	 * `word_08`.
	 */
	unsigned int timeoutBase;	/* +0x008 reset's last argument    */

	short codeLevel;		/* +0x00c linear level of the code */
	short codeLevelAlt;		/* +0x00e ... and of code + 0x10   */
	unsigned char pad_10[2];	/* +0x010                          */
	short idleLevel;		/* +0x012 linear level of silence  */
	Phase3ModulatorState state;	/* +0x014                          */
	unsigned int symbolCount;	/* +0x018 counts within a state    */

	/*
	 * Written on every path of both `generate*Symbol`, zero unless this
	 * symbol was the last of a state: 1 entering TRN1d, 2 entering Jd, 3
	 * entering JdPhase, 6 entering the terminated state -- which is also
	 * the one `exitDIL` sets.  It is the caller's per-symbol notification
	 * and nothing reads it here.  Was `word_1c`.
	 */
	unsigned int eventCode;		/* +0x01c                          */

	Scrambler<unsigned char, int> scrambler;	/* +0x020 32 bytes */

	/*
	 * The differential encoder's running sign.  Every scrambled state
	 * does `polarity ^= scrambler.process(bit)` and then emits
	 * `polarity ? codeLevel : -codeLevel`; TRN1d seeds it from the sign
	 * of the symbol it ends on.  Was `word_40`.
	 */
	unsigned int polarity;		/* +0x040                          */

	unsigned char *jdBits;		/* +0x044 V90Jd::getBitVector()    */
	unsigned char *jdV92Bits;	/* +0x048 V92Jd::getJdBitVector()  */
	unsigned char *jdV92PhaseBits;	/* +0x04c ...getJdPhaseBitVector() */

	/*
	 * The constructor's `V90Parameters *`.  It was `pad_50` until the
	 * constructor was reconstructed; `mov 0x34(%esp),%eax; mov
	 * %eax,0x50(%ebx)` at .text+0x2c4ce is the whole of the evidence, and
	 * it fixes the width at four bytes and the type at the one the
	 * mangling names.
	 *
	 * NO SYMBOL OF THIS CLASS READS IT, and that is a sweep: all nineteen
	 * were disassembled and searched for a `0x50` displacement, and the
	 * only hits are the two constructor copies' stores and `reset`'s `mov
	 * 0x50(%esp),%ebp`, which is a stack slot.  What reads it from
	 * outside, if anything, was not looked for -- D190, finding F1257.
	 */
	V90Parameters *params;		/* +0x050                          */

	/* The DIL generator's copy of the descriptor, expanded to levels. */
	unsigned char dilCount;		/* +0x054                          */
	unsigned char seq1Length;	/* +0x055                          */
	unsigned char seq2Length;	/* +0x056                          */
	unsigned char seq1[128];	/* +0x057                          */
	unsigned char seq2[128];	/* +0x0d7                          */
	unsigned char pad_157[1];	/* +0x157 alignment                */
	unsigned int segmentLength[8];	/* +0x158 6 * size + 6             */
	short segmentLevel[8];		/* +0x178                          */
	short dilLevel[256];		/* +0x188                          */

	/*
	 * The DIL generator's four cursors, all established by the two DIL
	 * states.  `seq1Index` and `seq2Index` step through `seq1` and `seq2`
	 * modulo their lengths -- seq1 chooses the sign of the level, seq2
	 * chooses between the DIL level and the segment level.  `dilIndex`
	 * steps through `dilLevel` modulo `dilCount`, one step per segment.
	 * `segmentPos` counts symbols within the current segment and is
	 * compared against `segmentLength[segmentIndex]`; reaching it clears
	 * all four of the first three and advances `dilIndex`.  Were
	 * `byte_388`, `byte_389`, `byte_38a` and `word_38c`.
	 */
	unsigned char seq1Index;	/* +0x388                          */
	unsigned char seq2Index;	/* +0x389                          */
	unsigned char dilIndex;		/* +0x38a                          */
	unsigned char pad_38b[1];	/* +0x38b alignment                */
	unsigned int segmentPos;	/* +0x38c                          */

	unsigned char segmentIndex;	/* +0x390 row index into the table */
	unsigned char pad_391[1];	/* +0x391 alignment                */

	/*
	 * Whether this DIL symbol took its level from `segmentLevel` rather
	 * than from `dilLevel` -- the same `seq2[seq2Index] == 0` test that
	 * chose it, stored again as a short.  Was `short_392`.
	 */
	short usingSegmentLevel;	/* +0x392                          */

	/*
	 * The G.711 code of `dilLevel[dilIndex]`, companded by the law in
	 * force and with the sign/company bits stripped the same way
	 * `resetDILGenerator` supplies them -- `^ 0xd5` for A-law, `~` for
	 * mu-law.  Written on every DIL symbol whatever level was emitted.
	 * Was `byte_394`.
	 */
	unsigned char dilPcmCode;	/* +0x394                          */

	unsigned char pad_395[3];	/* +0x395 tail padding to 0x398    */
};

#endif /* DSPLIB_V90PHASE3MODULATOR_H */

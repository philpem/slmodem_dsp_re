/*
 * V90Phase3Modulator.h -- the V.90 / V.92 phase 3 downstream symbol source.
 *
 * Reconstructed from dsplibs.o.  `V90Phase3Modulator` is NOT polymorphic --
 * tools/cppstruct.py lists its destructor with the `D1` and `D2` variants and
 * no `D0`, and GCC emits a deleting destructor only for a virtual one -- so
 * offset 0 is a real member and there is no vptr.  Finding 228 is the four
 * classes where that is not true.
 *
 * THE OBJECT IS 920 BYTES.  The largest `this`-relative displacement any
 * method uses is +0x394 and the store there is one byte, so the object ends
 * at 0x395 and rounds up to 0x398 for the four-byte members before it.  A
 * displacement is not a size (finding 229's last section); the .cpp asserts
 * both the size and every offset below.
 *
 * Twenty-one members are declared and five are defined -- the five that make
 * up task #60's batch 2.  Everything else is declared for the record and
 * deliberately left undefined, because defining a method whose callees are
 * not written breaks the link for the entire test suite (docs/v90cpp.md).
 * Nothing defined here calls an undefined one.
 *
 * THE SCRAMBLER IS A SUBOBJECT, NOT A POINTER.  `reset` takes the address of
 * `this + 0x20` and passes it to `Scrambler<unsigned char, int>::reset`, and
 * the two `generate*Symbol` bodies pass the same address to `process`.  Those
 * four weak template members are part of this batch even though
 * `tools/callgraph.py` does not list them; see include/dsplib/Scrambler.h.
 *
 * Data member names below are invented and descriptive: the mangling
 * preserves method names and type names but never a data member's name
 * (finding 226).  Where a field's purpose is not established by a function
 * this batch reconstructed, it carries an offset-derived name or is `pad_`.
 */

#ifndef DSPLIB_V90PHASE3MODULATOR_H
#define DSPLIB_V90PHASE3MODULATOR_H

#include "dsplib/Scrambler.h"
#include "dsplib/V90Jd.h"
#include "dsplib/V92Jd.h"

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
 * reachable as the initial value `reset` copies in.  The enumerator names are
 * invented and offset-free on purpose -- what each state emits is legible in
 * `generateV90Symbol` and `generateV92Symbol`, and a name that claimed more
 * than that would be a guess.
 */
enum Phase3ModulatorState {
	P3M_STATE_0 = 0,
	P3M_STATE_1 = 1,
	P3M_STATE_2 = 2,
	P3M_STATE_3 = 3,
	P3M_STATE_4 = 4,
	P3M_STATE_5 = 5,
	P3M_STATE_6 = 6,
	P3M_STATE_7 = 7,
	P3M_STATE_8 = 8,
	P3M_STATE_9 = 9,
	P3M_STATE_10 = 10,
	P3M_STATE_11 = 11,
	P3M_STATE_12 = 12,
	P3M_STATE_13 = 13,
	P3M_STATE_14 = 14,
	P3M_STATE_15 = 15
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
	/* The five that batch 2 defines. */
	void setSessionFlag(unsigned int);
	void resetDILGenerator(const tagV90DILdescriptor *);
	void reset(PcmType, unsigned char, Phase3ModulatorState, unsigned int,
		   V90Jd *, V92Jd *, const tagV90DILdescriptor *,
		   unsigned int);
	void generateV90Symbol();
	void generateV92Symbol();

	/*
	 * Declared, not defined -- see the file comment.  A return type is not
	 * mangled, so it is unknown for all of them.
	 *
	 * The constructor `V90Phase3Modulator(V90Parameters *, unsigned int)`
	 * and the destructor are NOT declared, deliberately: declaring either
	 * makes the class non-trivial, which deletes the default members of a
	 * union holding one -- and the fixture is exactly such a union -- and
	 * makes `__builtin_offsetof` conditionally supported.  Their
	 * signatures stay on the record in docs/v90cpp.md.
	 */
	void generateDIL();
	void generateSd();
	void generateSdNot();
	void generateJd();
	void generateJdNot();
	void generateJdPhase();
	void generateV92Jd();
	void generateSymbol();
	void generateTRN1d();
	void updateCodeSegmentPointer();
	void exitDIL();
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
	unsigned int word_08;		/* +0x008 reset's last argument    */
	short codeLevel;		/* +0x00c linear level of the code */
	short codeLevelAlt;		/* +0x00e ... and of code + 0x10   */
	unsigned char pad_10[2];	/* +0x010                          */
	short idleLevel;		/* +0x012 linear level of silence  */
	Phase3ModulatorState state;	/* +0x014                          */
	unsigned int symbolCount;	/* +0x018 counts within a state    */
	unsigned int word_1c;		/* +0x01c                          */
	Scrambler<unsigned char, int> scrambler;	/* +0x020 32 bytes */
	unsigned int word_40;		/* +0x040                          */
	unsigned char *jdBits;		/* +0x044 V90Jd::getBitVector()    */
	unsigned char *jdV92Bits;	/* +0x048 V92Jd::getJdBitVector()  */
	unsigned char *jdV92PhaseBits;	/* +0x04c ...getJdPhaseBitVector() */
	unsigned char pad_50[4];	/* +0x050                          */

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

	unsigned char byte_388;		/* +0x388                          */
	unsigned char byte_389;		/* +0x389                          */
	unsigned char byte_38a;		/* +0x38a                          */
	unsigned char pad_38b[1];	/* +0x38b alignment                */
	unsigned int word_38c;		/* +0x38c                          */
	unsigned char segmentIndex;	/* +0x390 row index into the table */
	unsigned char pad_391[1];	/* +0x391 alignment                */
	short short_392;		/* +0x392                          */
	unsigned char byte_394;		/* +0x394                          */
	unsigned char pad_395[3];	/* +0x395 tail padding to 0x398    */
};

#endif /* DSPLIB_V90PHASE3MODULATOR_H */

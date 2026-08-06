/*
 * V90AutoDigitalImpDetector.h -- the V.90 downstream digital-impairment
 * detector's object map.
 *
 * Reconstructed from dsplibs.o.  `V90AutoDigitalImpDetector` is NOT
 * polymorphic -- tools/cppstruct.py lists its destructor with the `D1` and
 * `D2` variants and no `D0`, and GCC emits a deleting destructor only for a
 * virtual one -- so offset 0 is a real member and there is no vptr.  Finding
 * 228 is the four classes where that is not true.
 *
 * THE OBJECT IS 43,440 BYTES (0xa9b0).  The largest `this`-relative
 * displacement any of the class's thirty-two members uses is +0xa9ae, and it
 * is a two-byte access -- `mov %ax,0xa9ae(%ebx)` in `resetStudyUrefHandler`
 * and `filds 0xa9ae(%esi)` in `porcessFirstStudy`, whose prologues load
 * `this` into those registers from the first stack argument -- so the object
 * ends at 0xa9b0, which is already four-byte aligned.  A displacement is not
 * a size (finding 215); the .cpp asserts both the size and every offset
 * below.
 *
 * The bound is the maximum over ALL thirty-two members, not just the two
 * written here: the two written here reach only +0xa980 and +0xa96c.  It was
 * measured by disassembling every `_ZN25V90AutoDigitalImpDetector*` symbol
 * and taking the largest displacement in each, then checking by hand that the
 * base register of the winner is `this`.  Finding 251.
 *
 * ONLY TWO OF THE THIRTY-TWO ARE DEFINED -- `reset` and `resetLinearMapping`.
 * Everything else is declared for the record and deliberately left undefined,
 * because defining a method whose callees are not written breaks the link for
 * the entire test suite (docs/v90cpp.md).  Neither of the two defined ones
 * calls an undefined one; between them they call only `alaw2linear` and
 * `ulaw2linear`, which src/service/pcm.c already provides.
 *
 * THE OBJECT IS MOSTLY SIX-BY-ONE-HUNDRED-AND-TWENTY-EIGHT ARRAYS.  Six is
 * the number of RBS phases -- every loop in the class runs a `short` index
 * from 0 to 5 inclusive -- and 128 is the seven-bit PCM code magnitude, which
 * is why `reset` masks its `unsigned char` argument with 0x7f before
 * companding it.  The arrays tile the object exactly:
 *
 *     +0x0000  short[6][128]   linMapp        cleared by resetLinearMapping
 *     +0x0600  short[6][128]   linMappAlt     cleared by resetLinearMapping
 *     +0x0d00  uchar[6][128]   set to 1 by reset
 *     +0x1000  int[6][128]     cleared by reset
 *     +0x1c00  int[6][128]     cleared by reset
 *     +0x8b00  short[6][128]   cleared by reset
 *     +0x9118  float[6][128]   cleared by reset
 *     +0x9d48  float[6][128]   cleared by reset
 *
 * and five per-phase scalars at +0x2800, +0x280c, +0x9100, +0x9d18 and
 * +0x9d30 are cleared alongside them.
 *
 * Data member names below are invented and mostly offset-derived: the
 * mangling preserves method names and type names but never a data member's
 * name (finding 226).  `linMapp` and `linMappAlt` are named for the method
 * whose entire body is clearing them, and `params`, `pcmType`, `ucode` and
 * `ucodeLevel` for what `reset` puts in them.  Everything else keeps an
 * offset-derived name or is `pad_`, and every `pad_` region is memory this
 * batch did not model rather than memory that is known to be unused.
 */

#ifndef DSPLIB_V90AUTODIGITALIMPDETECTOR_H
#define DSPLIB_V90AUTODIGITALIMPDETECTOR_H

/* For `PcmType`.  This header is included, never edited. */
#include "dsplib/V90Phase3Modulator.h"

/*
 * The parameter block the constructor is handed, held at +0x2814.  Not
 * modelled here -- `reset` reads exactly one field of it, the `short` at
 * +0x0c -- and forward-declared rather than included so that this header does
 * not depend on V90PreFilter.h, which declares the same class as a word
 * block.
 */
class V90Parameters;

/* Six RBS phases, and the seven-bit code magnitude they are indexed by. */
#define V90ADID_PHASES	6
#define V90ADID_CODES	128

class V90AutoDigitalImpDetector {
public:
	/* The two this batch defines. */

	/*
	 * Clear the per-phase measurement state and install the session's
	 * companding law, its reference code and its "alternate RBS expected"
	 * flag.  Does NOT touch `linMapp` or `linMappAlt`; those belong to
	 * `resetLinearMapping`, which is a separate call.
	 */
	void reset(unsigned char ucode, PcmType pcmType, short altRbs);

	/*
	 * Clear both linear-mapping tables and seed each phase's entry for the
	 * reference code with the reference level.
	 */
	void resetLinearMapping();

	/*
	 * Declared, not defined -- see the file comment.  Their signatures are
	 * the mangling's, so this list is a specification rather than a guess;
	 * a return type is not mangled and is therefore unknown for all of
	 * them.  The two misspellings are the original author's.
	 *
	 * The constructor `V90AutoDigitalImpDetector(V90Parameters *)` and the
	 * destructor are NOT declared, deliberately: declaring either makes
	 * the class non-trivial, which deletes the default members of a union
	 * holding one -- and the test fixture is exactly such a union -- and
	 * makes `__builtin_offsetof` conditionally supported.  Their
	 * signatures stay on the record in docs/findings.md.
	 */
	void addReceivedSampleToStorage(short, unsigned char, float);
	void adjustUinfoToPhaseOffset(short);
	void applyPadGainToLinMapp();
	void calculateLinearMeanAndVar(short, short, unsigned int);
	void calculateLinearMeanAndVarAlt(short, unsigned int);
	void clearCamulativeAltVal(short, short);
	void clearCamulativeVal(short, short);
	void determineMaxUcode(short);
	void findPadGain();
	void getAltVarThresh(float *, float);
	void isAltRbs(short, short, float);
	void isThereAnyAltRbsPhase();
	void porcessFirstStudy();
	void porcessSecondStudy();
	void resetStudyUrefHandler(unsigned int);
	void setConnectionType(short);
	void setMaxUcodeArray(unsigned char *);
	void setPrevSessionLinearMapping(short *);
	void setQcLinearMapping();
	void studyUrefHandler(float, unsigned int);
	void uniteLinMappInfoOfUnsuspectedPhases(unsigned char);
	void unitePhasesInfoOfUref(short);
	void unSuspectedPhaseNearestLinMapp(short, short);
	void updateAltRbsPhaseInDil();
	void updateLinMappMeanAndVar(short, short);
	void updateLinMappMeanAndVarAlt(short, short);
	void updateUref();
	void updateUrefAlt();

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable from the mangling (tools/cppstruct.py says so), and
	 * because a single access section is what keeps the class
	 * standard-layout and `__builtin_offsetof` well defined -- the .cpp
	 * asserts every offset below against what the compiler lays out.
	 */

	/*
	 * The two linear-mapping tables, one linear level per RBS phase per
	 * seven-bit code.  `resetLinearMapping` clears both and then writes
	 * `ucodeLevel` into `[phase][ucode]` of each; the "Alt" copy is the
	 * alternate-RBS hypothesis, the same split the class's
	 * `*MeanAndVar` / `*MeanAndVarAlt` and `clearCamulative*` pairs carry
	 * everywhere else.
	 */
	short linMapp[V90ADID_PHASES][V90ADID_CODES];		/* +0x0000 */
	short linMappAlt[V90ADID_PHASES][V90ADID_CODES];	/* +0x0600 */

	/*
	 * 256 bytes neither method here writes.  `setPrevSessionLinearMapping`
	 * stores shorts at +0xc00 and `setQcLinearMapping` reads them, so it
	 * is one more code-indexed table -- but its extent is not measured by
	 * this batch, so it stays padding rather than becoming a guessed
	 * field.
	 */
	unsigned char pad_0c00[0x100];				/* +0x0c00 */

	/*
	 * One flag per phase per code, set to 1 by `reset`.
	 * `determineMaxUcode` is the only other member that touches it and it
	 * both tests it against 0 and writes 0 and 1, so it is a boolean; what
	 * it means is not established here.
	 */
	unsigned char byte_0d00[V90ADID_PHASES][V90ADID_CODES];	/* +0x0d00 */

	/* Cleared by `reset`; every other user is a plain 32-bit move. */
	int int_1000[V90ADID_PHASES][V90ADID_CODES];		/* +0x1000 */
	int int_1c00[V90ADID_PHASES][V90ADID_CODES];		/* +0x1c00 */

	/*
	 * Per-phase.  `short_2800` is compared against 0 by nine members and
	 * is the phase's "suspected" flag; `byte_280c` likewise.  Both are
	 * cleared by `reset` and neither is named here beyond its offset.
	 */
	short short_2800[V90ADID_PHASES];			/* +0x2800 */
	unsigned char byte_280c[V90ADID_PHASES];		/* +0x280c */
	unsigned char pad_2812[2];				/* +0x2812 */

	/* The constructor's only argument.  `reset` reads its +0x0c. */
	V90Parameters *params;					/* +0x2814 */

	/*
	 * 25,320 bytes this batch did not model.  Nothing `reset` or
	 * `resetLinearMapping` touches lives here; the members that do are
	 * `studyUrefHandler`, `findPadGain` and the rest of the study path.
	 */
	unsigned char pad_2818[0x62e8];				/* +0x2818 */

	short short_8b00[V90ADID_PHASES][V90ADID_CODES];	/* +0x8b00 */
	int int_9100[V90ADID_PHASES];				/* +0x9100 */

	/*
	 * Float from every other user -- `fadds`, `fstps`, `fmuls` -- and
	 * cleared by `reset` with a 32-bit zero, which is 0.0f.
	 */
	float float_9118[V90ADID_PHASES][V90ADID_CODES];	/* +0x9118 */
	float float_9d18[V90ADID_PHASES];			/* +0x9d18 */

	/* Integer: `calculateLinearMeanAndVarAlt` increments it with `incl`. */
	int int_9d30[V90ADID_PHASES];				/* +0x9d30 */

	float float_9d48[V90ADID_PHASES][V90ADID_CODES];	/* +0x9d48 */

	/* Cleared by `reset`; `studyUrefHandler` is the only other writer. */
	short short_a948;					/* +0xa948 */
	unsigned char pad_a94a[2];				/* +0xa94a */

	/*
	 * Set to 1.0f by `reset`.  `findPadGain` stores to it and
	 * `applyPadGainToLinMapp` divides by it, so it is the pad gain the two
	 * method names are about; the name here stays offset-derived because
	 * neither of those is written.
	 */
	float float_a94c;					/* +0xa94c */

	unsigned char pad_a950[0x0c];				/* +0xa950 */

	/* The companding law, stored as a full 32-bit copy of the argument. */
	PcmType pcmType;					/* +0xa95c */

	unsigned char pad_a960[0x0b];				/* +0xa960 */

	/*
	 * The reference PCM code and the linear level it companded to.  `reset`
	 * takes the code as its first argument, masks it to seven bits,
	 * supplies the sign/company bits the same way V90Phase3Modulator's DIL
	 * expansion does -- `^ 0xd5` for A-law, `^ 0xff` for mu-law -- and
	 * stores the result of the conversion here.
	 */
	unsigned char ucode;					/* +0xa96b */
	short ucodeLevel;					/* +0xa96c */

	/*
	 * `reset`'s third argument, and the only thing it selects: nonzero
	 * puts 5.0f in `float_a970` where zero puts 1.5f.
	 */
	short short_a96e;					/* +0xa96e */

	float float_a970;					/* +0xa970 */
	float float_a974;					/* +0xa974 */

	/*
	 * The four `reset` sets from `params->[+0x0c] == 2`: (1, 88, 0.35f,
	 * 1.75f) when it is 2 and (0, 80, 0.25f, 1.5f) when it is not.
	 */
	short short_a978;					/* +0xa978 */
	short short_a97a;					/* +0xa97a */
	float float_a97c;					/* +0xa97c */
	float float_a980;					/* +0xa980 */

	/*
	 * 44 bytes this batch did not model, up to the object's measured end.
	 * `resetStudyUrefHandler` fills +0xa98c..+0xa998 from the parameter
	 * block and writes the shorts at +0xa9a4, +0xa9a6 and +0xa9ae and the
	 * float at +0xa9a8; +0xa9ae is the displacement the object's size
	 * comes from.
	 */
	unsigned char pad_a984[0x2c];				/* +0xa984 */
};

#endif /* DSPLIB_V90AUTODIGITALIMPDETECTOR_H */

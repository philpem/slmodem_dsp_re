/*
 * V90PreFilter.h -- the V.90 receive pre-filter: a FloatFIR that chooses its
 * own coefficients from what Phase 2 measured about the line.
 *
 * Reconstructed from dsplibs.o V90PreFilter.cpp.  Twenty-four members in the
 * blob; batch 3 of task #60 writes five of them (`selectFilter`,
 * `setParamEia6`, `autoSelection`, `isV90WithEia6`, `displayParamEia6`) and
 * all ten static data members, 23,860 bytes of them.  The rest are declared
 * here and deliberately left undefined -- defining a method whose callees are
 * not written breaks the link for the whole test suite (docs/v90cpp.md).
 *
 * NOT POLYMORPHIC.  tools/cppstruct.py lists the destructor with the two
 * ordinary variants and not the deleting `D0`, so there is no vptr and offset
 * 0 is a real member (finding F228).
 *
 * THE OBJECT IS FORTY BYTES, NOT 1,280.  docs/v90cpp.md carried 1,280 as a
 * bound taken from the largest displacement any of these methods uses.  That
 * displacement is not off `this`: `setParamEia6` touches `this` at exactly
 * one offset, +0x1c, and then works entirely inside the V90Parameters block
 * that lives there, where it reaches +0x490; `isV90WithEia6` reads +0x500 of
 * the same block.  Across all twenty-four members the largest `this`
 * displacement is +0x24, and the store there is four bytes, so the object is
 * 0x28.  (Finding F234.)
 *
 * FloatFIR IS A PUBLIC BASE CLASS, AND THE BLOB SAYS SO OUTRIGHT.  This
 * paragraph used to read "might be a base class ... nothing in the blob
 * distinguishes them", and that was wrong: `setCoefficients` passing `this`
 * unadjusted is indeed ambiguous, but the CONSTRUCTOR AND DESTRUCTOR VARIANT
 * the object references is not.  GCC uses the base-object variants C2/D2 for
 * a base subobject and the complete-object variants C1/D1 for a member, and
 * the blob picks the base ones at every site:
 *
 *     44d86  V90PreFilter::C2  ->  R_386_PC32  _ZN8FloatFIRC2EjPfj
 *     449fa  V90PreFilter::D2  ->  R_386_PC32  _ZN8FloatFIRD2Ev
 *     44a1a  V90PreFilter::D1  ->  R_386_PC32  _ZN8FloatFIRD2Ev
 *
 * and all four of FloatFIR's C1/C2/D1/D2 are distinct symbols at distinct
 * addresses in the blob, so the choice is a real one and not an alias.  With
 * `fir` written as a member we emitted `_ZN8FloatFIRC1EjPfj` and
 * `_ZN8FloatFIRD1Ev` instead, which is what held both destructors in the
 * RELOC bucket at one differing byte of nineteen.  Finding F8080.
 *
 * The cost is that V90PreFilter is no longer standard-layout, so the
 * `__builtin_offsetof` assertions in the .cpp are conditionally supported
 * rather than well defined.  GCC accepts them on both compilers and the
 * assertion on `fir` itself is replaced by one on `sizeof(FloatFIR)`, which
 * pins the same fact -- see the .cpp.
 */

#ifndef DSPLIB_V90PREFILTER_H
#define DSPLIB_V90PREFILTER_H

#include "dsplib/FloatFIR.h"

/*
 * One reference loop: a name, the six-point signature Phase 2's measurement
 * is matched against, and what to do when it wins.
 *
 * The shape is out of the code, not out of a document.  `autoSelection` steps
 * the array 0x44 at a time, stops at a record whose first byte is zero, sums
 * the squared differences of six floats at +0x20 against the measurement, and
 * returns the int at +0x3c for the closest.  `selectFilter`, `setFilter` and
 * `getFilterPointer` all switch on the int at +0x38 to pick which of the
 * three coefficient banks to use -- and `edprintf` prints it as "Pre Filter
 * Coeffs Type array %d", which is where the field's meaning comes from.
 * `isV90WithEia6` and `getV90Capability` test the int at +0x40 against 2.
 */
struct V90RefLoop {
	char name[32];		/* +0x00 zero-length ends the array         */
	float signature[6];	/* +0x20 what autoSelection matches against */
	int coefType;		/* +0x38 1, 2 or 3: which bank              */
	int gain;		/* +0x3c the row within it                  */
	int capability;		/* +0x40 2 means EIA-6                      */
};

/*
 * One hardware codec: its name, and the reference loops measured for it.
 * The constructor prints the name with "HardwareCodecType: %s" and counts the
 * table by walking until a name's first byte is zero.
 */
struct V90CodecEntry {
	char name[32];		/* +0x00 */
	V90RefLoop *loops;	/* +0x20 */
};

/*
 * V90Phase2Info WAS STUBBED HERE TOO, as an opaque 0x1c-byte block whose own
 * comment asked the batch that modelled it to replace the declaration rather
 * than add a second one.  Finding F255 modelled it; this is that replacement.
 *
 * The 0x1c was the furthest `autoSelection` reaches.  The real object is 0x24:
 * the constructor and `setToDefault` reach +0x20, and neither is a member of
 * this class.  A bound taken from the members you happen to be writing is
 * finding F215's mistake, and it is short here by eight bytes.
 */
#include "dsplib/V90Phase2Info.h"

/*
 * V90Parameters IS MODELLED, and this header used to carry a SECOND, SMALLER
 * definition of it -- 0x504 against the real 0x558, from a scan of how far
 * five methods happened to reach.  Two classes of different size under one
 * name is undefined behaviour wherever both are visible, and it is what
 * V90ModemCtor.cpp's long comment about which header it may not include is
 * about.  One definition now; the raw word and float views the block form
 * provided live in V90Parameters.h as V90PW()/V90PF()/V90PB().  Task #116,
 * finding F1112.
 */
#include "dsplib/V90Parameters.h"

/*
 * NOT A SIZE, AND IT NEVER WAS: the furthest the five V90PreFilter methods
 * reach into V90Parameters, `isV90WithEia6` reading +0x500.  `sizeof
 * (V90Parameters)` is 0x558.  The tests use this as the boundary of the
 * region they exercise -- everything above it is a guard they assert stays
 * untouched -- which is a real and different job from a size, and why it
 * survives the reconciliation.
 */
#define V90PARAMETERS_BOUND 0x504

/*
 * `__tHardwareCodecTypes__` has its own header because V90ModemCtor.cpp needs
 * the type and must not have this one -- see V90CodecType.h.
 */
#include "dsplib/V90CodecType.h"

/*
 * Named by setFilter's mangling.  A definition rather than an opaque
 * declaration because C++98 has none and the author's compiler was C++98;
 * `_BASE_PIN` is ours and fixes only the underlying type.
 * docs/method/compilers.md, V2.
 *
 * THREE VALUES ARE NOW RECOVERED AND THE NAMES STILL ARE NOT.  Every member
 * that dispatches on this type -- `setFilter(PreFilterCoefType, unsigned)`,
 * and the `V90RefLoop::coefType` switches in `setFilter(unsigned)`,
 * `getFilterPointer`, `getFilterLength` and `selectFilter` -- tests against
 * exactly 2 and 3 and treats 1 as its own arm before falling to a default
 * that complains.  So the enumerator VALUES are 1, 2 and 3, each selecting
 * the coefficient bank of the same number, and every one of the five sites
 * has a default arm, so there may be more.
 *
 * NO ENUMERATORS ARE ADDED FOR THEM.  What each value selects is known; what
 * the author CALLED it is not, and a name invented here would be believed by
 * every later reader and could never fail a test (CLAUDE.md, "Naming
 * something wrongly is worse than leaving it padded").  The case labels in
 * the .cpp are therefore integers, exactly as the constructor's sixteen-arm
 * `__tHardwareCodecTypes__` switch spells its own.
 *
 * `_BASE_PIN` is load-bearing for the differential test as well as for the
 * type: it is what makes `(PreFilterCoefType)7` a value of the enumeration
 * rather than undefined, so the sweep can drive the default arm.
 */
enum PreFilterCoefType { PreFilterCoefType_BASE_PIN = -0x7fffffff - 1 };

class V90PreFilter : public FloatFIR {
public:
	/* Written -- batch 3. */
	void selectFilter();
	void setParamEia6();
	int autoSelection();
	int isV90WithEia6() const;
	void displayParamEia6();

	/*
	 * Written -- the lifecycle batch, finding F1233.  The constructor's
	 * signature is the mangling's, so it is a specification and not a
	 * guess; a return type is never mangled, and both of these have none
	 * to recover.
	 *
	 * The constructor is what makes `fir` a MEMBER rather than a base as
	 * far as this reconstruction is concerned: it is initialised in the
	 * member initialiser list with (40, NULL, 99), which is what the blob
	 * does first and with `this` unadjusted.  Nothing in the object
	 * distinguishes the two spellings; the file comment above says why
	 * this one is chosen.
	 */
	V90PreFilter(__tHardwareCodecTypes__ codec, V90Phase2Info *info,
		     V90Parameters *params);
	~V90PreFilter();

	/* Written -- batch 3. */
	void reset();

	/*
	 * Written -- the filter-accessor batch.  The argument lists are the
	 * mangling's, so they are a specification and not a guess.
	 *
	 * RETURN TYPES ARE NOT MANGLED, and four of these six are settled
	 * anyway -- two of them by a CALLEE's mangling, which is the stronger
	 * kind of evidence:
	 *
	 *   getFilterPointer  `float *`.  Its value is passed straight to
	 *                     `_ZN8FloatFIR15setCoefficientsEPfj`, whose first
	 *                     parameter the mangling spells `Pf`.
	 *   getFilterLength   `unsigned int`.  Same call, second parameter,
	 *                     spelled `j`.
	 *   getV90Capability  `int`.  It returns either the literal 1 or the
	 *                     `int` at V90RefLoop +0x40, in %eax.
	 *   getNofRefLoops    `int`.  A count of table entries, in %eax.
	 *
	 * Both `setFilter`s are spelled `void` FOR WANT OF EVIDENCE, which is
	 * this file's default: neither ever sets %eax deliberately.  The
	 * one-argument one falls out of its epilogue with whatever the last
	 * inlined body left there, and the two-argument one TAIL JUMPS to
	 * `setCoefficients` on two arms and returns after it on the third, so
	 * a caller reading %eax would get `setCoefficients`'s `int` from two
	 * arms and something else from the other.  Nothing calls either in the
	 * written tree, so nothing narrows it further.
	 */
	void setFilter(unsigned int gain);
	void setFilter(PreFilterCoefType type, unsigned int gain);
	float *getFilterPointer(unsigned int gain);
	unsigned int getFilterLength(unsigned int gain);
	int getV90Capability();
	int getNofRefLoops() const;

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable, and because one access section keeps the class
	 * standard-layout.  The names are invented; the mangling never carries
	 * a data member's name.
	 */
	/*
	 * +0x00 to +0x13 is the FloatFIR base subobject, twenty bytes; see
	 * the note at the top of this file for why it is a base and not a
	 * member.  Its own fields are inherited, so `setCoefficients`,
	 * `process` and `FloatFIR::reset` are called unqualified here.
	 */
	int codecType;			/* +0x14 index into dataBase          */
	V90Phase2Info *phase2;		/* +0x18 the constructor's 2nd argument */
	V90Parameters *params;		/* +0x1c the constructor's 3rd argument */
	int gain;			/* +0x20 "Filter Gain", the bank row  */
	int refLoop;			/* +0x24 index into the loop array,
					 *       -1 for none               */

	/*
	 * The ten static members.  All are `D` in the blob, so none is const,
	 * and `float *` is what setCoefficients takes anyway.
	 *
	 * The six refLoopsType* tables are here rather than with #59's
	 * `VPcmV34InitiateRetrain`, which is the only function that names them
	 * in .text, because `dataBase` points at them: its definition carries
	 * sixteen relocations into these six, so the batch that defines
	 * `dataBase` defines them too (finding F234).  There is no
	 * refLoopsType3.
	 */
	static float preFilterCoefType1[31][20];
	static float preFilterCoefType2[31][20];
	static float preFilterCoefType3[31][40];
	static V90CodecEntry dataBase[17];
	static V90RefLoop refLoopsType1[23];
	static V90RefLoop refLoopsType2[34];
	static V90RefLoop refLoopsType4[36];
	static V90RefLoop refLoopsType5[36];
	static V90RefLoop refLoopsType6[34];
	static V90RefLoop refLoopsType7[34];
};

#endif /* DSPLIB_V90PREFILTER_H */

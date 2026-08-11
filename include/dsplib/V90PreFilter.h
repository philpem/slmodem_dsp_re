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
 * 0 is a real member (finding 228).
 *
 * THE OBJECT IS FORTY BYTES, NOT 1,280.  docs/v90cpp.md carried 1,280 as a
 * bound taken from the largest displacement any of these methods uses.  That
 * displacement is not off `this`: `setParamEia6` touches `this` at exactly
 * one offset, +0x1c, and then works entirely inside the V90Parameters block
 * that lives there, where it reaches +0x490; `isV90WithEia6` reads +0x500 of
 * the same block.  Across all twenty-four members the largest `this`
 * displacement is +0x24, and the store there is four bytes, so the object is
 * 0x28.  (Finding 234.)
 *
 * FloatFIR IS AT OFFSET ZERO AND MIGHT BE A BASE CLASS.  Every call the
 * object makes to `FloatFIR::setCoefficients` passes `this` unadjusted, which
 * is what both a first member and a public base look like; nothing in the
 * blob distinguishes them.  It is written as a member because that keeps
 * V90PreFilter standard-layout, so `__builtin_offsetof` in the .cpp is well
 * defined rather than merely supported.
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
 * than add a second one.  Finding 255 modelled it; this is that replacement.
 *
 * The 0x1c was the furthest `autoSelection` reaches.  The real object is 0x24:
 * the constructor and `setToDefault` reach +0x20, and neither is a member of
 * this class.  A bound taken from the members you happen to be writing is
 * finding 215's mistake, and it is short here by eight bytes.
 */
#include "dsplib/V90Phase2Info.h"

/*
 * V90Parameters IS STILL NOT MODELLED.  The name is the original's, out of
 * the constructor's mangling; what is inside it is not recovered here, and the
 * number below is a BOUND -- the furthest these five methods reach -- rather
 * than a size.  A later batch that models it should replace this declaration
 * rather than add a second one.
 *
 * It is a word block because that is how the object treats it: `setParamEia6`
 * is forty-odd whole-word copies between fixed offsets inside V90Parameters
 * and one float store, and nothing here knows what any of them mean.  Keeping
 * the offsets numeric is the honest spelling.
 *
 * `V90Phase2Info.h` forward-declares this class, which is why including it
 * above and defining the class here are compatible: a declaration may precede
 * a definition, and `V90Phase2Info::params` is only ever a pointer.
 */
#define V90PARAMETERS_BOUND 0x504	/* isV90WithEia6 reads +0x500 */

class V90Parameters {
public:
	/*
	 * DECLARED HERE AND DEFINED IN src/pump/v90/V90Parameters.cpp,
	 * against the OTHER definition of this class.  That is not a
	 * contradiction: the two headers describe the same class -- the same
	 * allocation, the same mangled names -- one of them as a named map and
	 * one as a block, and a member takes only `this`.  So a translation
	 * unit that has the block form can still call a member the named form
	 * defines, and this is how `VPcmFloModem::externalReset` reaches
	 * `initSession` and `init` without pulling in a second definition of
	 * the class it is embedded in.  Finding 1112 is the duplication
	 * itself, which is a wart and not a design.
	 */
	void	init();
	void	initSession();

	union {
		unsigned char b[V90PARAMETERS_BOUND];
		int w[V90PARAMETERS_BOUND / 4];
		float f[V90PARAMETERS_BOUND / 4];
	};
};

/* Named by the constructor's and setFilter's manglings; values not recovered. */
enum __tHardwareCodecTypes__ : int;
enum PreFilterCoefType : int;

class V90PreFilter {
public:
	/* Written -- batch 3. */
	void selectFilter();
	void setParamEia6();
	int autoSelection();
	int isV90WithEia6() const;
	void displayParamEia6();

	/*
	 * Written -- the lifecycle batch, finding 1233.  The constructor's
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
	 * Declared, not defined.  The signatures are the mangling's, so this
	 * is a specification and not a guess; return types are not mangled and
	 * are therefore unknown for all of them.
	 */
	void setFilter(unsigned int gain);
	void setFilter(PreFilterCoefType type, unsigned int gain);
	void getFilterPointer(unsigned int gain);
	void getFilterLength(unsigned int gain);
	void getV90Capability();
	void getNofRefLoops() const;

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable, and because one access section keeps the class
	 * standard-layout.  The names are invented; the mangling never carries
	 * a data member's name.
	 */
	FloatFIR fir;			/* +0x00 20 bytes, see the note above */
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
	 * `dataBase` defines them too (finding 234).  There is no
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

/*
 * V90Mapper.h -- the V.90 downstream constellation mapper.
 *
 * Reconstructed from dsplibs.o.  Five members and 2,400 bytes of code, of
 * which the constructor and the destructor are written here; `reset`,
 * `resetNoSpectral` and `process` are not.
 *
 * NOT POLYMORPHIC: `~V90Mapper` is listed with `D1` and `D2` and no `D0`, so
 * offset 0 is a real member and there is no vptr.
 *
 * THE SIZE IS 0x704, AND IT IS THE ORIGINAL COMPILER'S OWN `sizeof`.
 * `V90BitsToSymbol`'s constructor is
 *
 *     movl $0x704,(%esp) ; call sysdep_malloc ; ... ; call V90Mapper::C1
 *
 * -- the allocation is `sizeof(V90Mapper)` written by the compiler that laid
 * the class out, which is better evidence than any displacement (finding
 * 1246).  Every field below is inside it and the last one ends at 0x6fd, so
 * the class is 0x704 with six bytes of tail padding and one member of unknown
 * width at 0x700 -- either reading is consistent and nothing distinguishes
 * them.
 *
 * THE TWO EMBEDDED SUBOBJECTS ARE FIXED POINTS, NOT GUESSES.  The constructor
 * does `lea 0x670(%ebx),%edx ; call ModulusEncoder::C1` and `lea
 * 0x68c(%ebx),%eax ; call V90SpectralShaper::C1` -- a `lea` and not a load,
 * so they are embedded and not pointed at.  `sizeof(ModulusEncoder)` is 0x1c
 * (asserted in src/pump/v90/ModulusCoder.cpp) and 0x670 + 0x1c = 0x68c, so
 * they abut.  `sizeof(V90SpectralShaper)` is 0x6c (asserted in
 * src/pump/v90/V90SpectralShaper.cpp, and derived independently from inside
 * that class) and 0x68c + 0x6c = 0x6f8, which is exactly where the next field
 * the constructor writes sits.  Three sizes settled elsewhere and one
 * displacement here agree to the byte.
 *
 * THE DESTRUCTOR DOES NOT DESTROY THE MODULUS ENCODER.  It calls
 * `V90SpectralShaper::~V90SpectralShaper` and nothing else, which is what a
 * compiler emits when the other member is trivially destructible --
 * `ModulusEncoder` has a constructor and no destructor, and `nm` on the blob
 * shows no `_ZN14ModulusEncoderD*` symbol at all.
 *
 * Data member names are invented; the mangling never carries one (finding
 * 226).  The seven `cleared_*` fields are named for the only thing known
 * about them -- the constructor stores zero over each -- rather than for a
 * meaning that would have to come from `reset` and `process`, which are not
 * written here.
 */

#ifndef DSPLIB_V90MAPPER_H
#define DSPLIB_V90MAPPER_H

#include "dsplib/ModulusCoder.h"	/* ModulusEncoder, embedded at +0x670 */
#include "dsplib/V90SpectralShaper.h"	/* embedded at +0x68c               */

class V90Parameters;

class V90Mapper {
public:
	/* Defined in src/pump/v90/V90Mapper.cpp. */
	V90Mapper(V90Parameters *params);
	~V90Mapper();

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	V90Parameters *params;			/* +0x000 the argument   */
	unsigned int cleared_004;		/* +0x004 zeroed by ctor */
	unsigned int cleared_008;		/* +0x008 zeroed by ctor */
	unsigned int cleared_00c;		/* +0x00c zeroed by ctor */
	unsigned int cleared_010;		/* +0x010 zeroed by ctor */
	unsigned int cleared_014;		/* +0x014 zeroed by ctor */
	void *buf;				/* +0x018 malloc(0x50)   */
	unsigned int cleared_01c;		/* +0x01c zeroed by ctor */
	unsigned char pad_020[0x638];		/* +0x020 not modelled   */
	unsigned int cleared_658[6];		/* +0x658 zeroed by ctor */
	ModulusEncoder modulusEncoder;		/* +0x670 0x1c bytes     */
	V90SpectralShaper spectralShaper;	/* +0x68c 0x6c bytes     */
	unsigned int cleared_6f8;		/* +0x6f8 zeroed by ctor */
	unsigned char cleared_6fc;		/* +0x6fc zeroed by ctor */
	unsigned char pad_6fd[7];		/* +0x6fd tail padding   */
};

#endif /* DSPLIB_V90MAPPER_H */

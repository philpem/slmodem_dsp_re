/*
 * ANSamToneDetector.h -- the V.25/V.8 answer-tone detector, which is a
 * `GenericToneDetector` with its filter chosen for it.
 *
 * Reconstructed from dsplibs.o.  Two members, and they are both of them: the
 * constructor (0x108b0 / 0x10950, 146 bytes) and the destructor (0x109f0 /
 * 0x10a10, 19 bytes).  There is no third symbol anywhere in the object -- no
 * `reset`, no `process` -- so the class adds NO behaviour to its base; what
 * it adds is a table.
 *
 * IT DERIVES, IT DOES NOT CONTAIN, AND THE VARIANT IS WHAT SAYS SO.  The
 * constructor calls `_ZN19GenericToneDetectorC2EjjPdS0_jjfjfjj` -- the C2,
 * BASE-object constructor -- and the destructor calls
 * `_ZN19GenericToneDetectorD2Ev`, the D2.  A GenericToneDetector held as a
 * MEMBER at offset zero would be built with C1 and destroyed with D1, because
 * a member is a complete object; only a base subobject uses the C2/D2 pair.
 * The `this` handed over is unchanged (`mov 0x40(%esp),%edx; mov %edx,(%esp)`),
 * so the base is at offset zero, which is also the only place a single
 * non-virtual base can be.
 *
 * NOT POLYMORPHIC: `D1` and `D2` with no `D0`, and neither constructor writes
 * a vptr -- the constructor's only stores are the eleven argument slots it
 * builds for the base call.
 *
 * NO MEMBER OF ITS OWN IS MODELLED, and the object is why.  The constructor
 * writes nothing into `*this` at all; every store it makes is to its own
 * outgoing argument area, and the base constructor initialises all fifteen of
 * the base's fields.  A member added here would therefore be a member no
 * constructor initialises, which is a claim the object does not support, so
 * the class is the base and nothing more and `sizeof` stays at the base's
 * 0x3c.  That is a reconstruction choice about a thing the object leaves
 * silent, stated rather than implied.
 *
 * THE SIXTH ARGUMENT IS A SELECTOR AND IS NEVER FORWARDED.  It is compared
 * against 8000 three separate times -- `cmp $0x1f40,%edx` at +0x17, +0x55 and
 * +0x6d -- and each comparison chooses one of a pair:
 *
 *     nden = nnum = 8000 ? 13 : 11        (`sete`, then `lea 0xb(%eax,%eax,1)`)
 *     den        = 8000 ? .data+0x2a0 : .data+0x240
 *     num        = 8000 ? .data+0x1c0 : .data+0x160
 *
 * The four tables are 11 and 13 `double`s, they are file-local (the
 * relocations are against the .data SECTION symbol with the offset as an
 * inline addend -- finding 604), and they are transcribed in
 * src/dsp/ANSamToneDetector.cpp in the order the object lays them out.  The
 * other seven arguments go straight through in order.
 *
 * WHICH RATE IS "NOT 8000" IS NOT RECOVERABLE HERE.  Nothing in the object
 * constructs an ANSamToneDetector -- there is no relocation against either
 * constructor -- so the argument's provenance is unknown and the two arms are
 * named by their tap count rather than by a rate this file cannot prove.
 *
 * NO x87 IS INVOLVED IN EITHER MEMBER.  The two float arguments are copied
 * from one stack slot to another with 32-bit integer `mov`s
 * (`mov 0x4c(%esp),%eax; mov %eax,0x1c(%esp)`), never loaded onto the x87
 * stack, so the constructor cannot round, quieten or flush anything.  The
 * base makes the same promise for the same reason (GenericToneDetector.h);
 * the sweep in the test is over bit patterns for exactly that claim.
 */

#ifndef DSPLIB_ANSAMTONEDETECTOR_H
#define DSPLIB_ANSAMTONEDETECTOR_H

#include "dsplib/GenericToneDetector.h"

class ANSamToneDetector : public GenericToneDetector {
public:
	/*
	 * EIGHT ARGUMENTS, seven of which are the base's.  The mangling is
	 * `_ZN17ANSamToneDetectorC1Ejjfjfjjj`, so the shape is a
	 * specification; the names below are the base's names for the
	 * arguments it receives them as, which is all that is recoverable
	 * (finding 226).
	 *
	 *   samples1, samples2   the base divides these by `blockLen`
	 *   threshold, ratio     the two floats, copied
	 *   flag                 the base's +0x34
	 *   sampleRate           THE SELECTOR.  8000 picks the 13-tap pair.
	 *                        Not stored anywhere.
	 *   blockLen, blockSize  the base's divisor and the filter's slack
	 */
	ANSamToneDetector(unsigned int samples1, unsigned int samples2,
			  float threshold, unsigned int flag, float ratio,
			  unsigned int sampleRate, unsigned int blockLen,
			  unsigned int blockSize);

	/*
	 * NINETEEN BYTES AND AN EMPTY BODY.  `sub $0xc,%esp`, the argument,
	 * `call GenericToneDetector::~GenericToneDetector`, `add`, `ret`:
	 * that is the whole function, which is what a destructor with no
	 * statements and one base compiles to.  It is DECLARED rather than
	 * left implicit because the object's `D1` and `D2` are ordinary
	 * global `T` symbols; an implicit destructor is emitted weak, in a
	 * comdat group, or not at all.
	 */
	~ANSamToneDetector();
};

#endif /* DSPLIB_ANSAMTONEDETECTOR_H */

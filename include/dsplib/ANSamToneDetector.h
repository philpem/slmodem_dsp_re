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
 * NO MEMBER OF ITS OWN, AND THE SIZE IS MEASURED RATHER THAN CHOSEN.  Two
 * things say so.  The constructor writes nothing into `*this` at all -- every
 * store it makes is to its own outgoing argument area, and the base
 * constructor initialises all fifteen of the base's fields -- so a member
 * added here would be a member no constructor initialises.  And
 * `V90Phase3Demodulator`'s constructor allocates one on the heap:
 * `movl $0x3c,(%esp); call sysdep_malloc` at 0x21377, the result into `%esi`,
 * and `%esi` is the `this` of the `C1` call twenty instructions later.  Sixty
 * bytes is `sizeof(GenericToneDetector)` exactly, so the derived class adds
 * nothing to it.
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
 * inline addend -- finding F604), and they are transcribed in
 * src/dsp/AnsamToneDetector.cpp in the order the object lays them out.  The
 * other seven arguments go straight through in order.
 *
 * THE OTHER RATE IS 9600, AND BOTH CALL SITES SAY SO.  Four relocations name
 * `_ZN17ANSamToneDetectorC1Ejjfjfjjj`, and they are two constructors emitted
 * twice each -- `VPcmFloModem`'s (0xfb61 in C1, 0xffe1 in C2) and
 * `V90Phase3Demodulator`'s (0x213ce, 0x2153e).  Both use `C1`, because both
 * build a COMPLETE object: the first embeds one (`lea 0x6f5c(%ebx),%ecx`), the
 * second owns one on the heap (`sysdep_malloc(0x3c)`).  Both spell every
 * argument as a constant:
 *
 *   VPcmFloModem, embedded at +0x6f5c   6000, 450, 0x48742400, 1,
 *                                       0x3f147ae1, 9600, 50, 99
 *   V90Phase3Demodulator, heap          400, 100, 0x48960000, 0,
 *                                       0.5f, 8000, 50, 99
 *
 * So the 13-tap pair is the V.90 phase 3 demodulator's at 8000 and the 11-tap
 * pair is the PCM modem's at 9600 -- `mov $0x2580,%edi` at 0xfafe, spilled to
 * the selector slot at 0xfb2b.  The tables below are still NAMED by their tap
 * count rather than by a rate, because the tap count is what the object's own
 * arithmetic produces and a third caller at a third rate would take the
 * 11-tap arm too.
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
	/**
	 * @brief Construct an answer-tone detector: a GenericToneDetector
	 * base, plus a filter chosen by @p sampleRate.
	 *
	 * Eight arguments, seven of which are the base's -- the names are the
	 * base's own names for the arguments it receives (finding F226); this
	 * class adds no member and no method of its own.
	 *
	 * @param samples1    Divided by @p blockLen; forwarded to the base.
	 * @param samples2    Divided by @p blockLen; forwarded to the base.
	 * @param threshold   Forwarded to the base unchanged.
	 * @param flag        Forwarded to the base's +0x34.
	 * @param ratio       Forwarded to the base unchanged.
	 * @param sampleRate  THE SELECTOR, not stored anywhere: 8000 picks
	 *                    the 13-tap filter pair (V.90 phase 3's case),
	 *                    anything else picks the 11-tap pair (V.PCM's
	 *                    9600 Hz case).
	 * @param blockLen    The base's divisor.
	 * @param blockSize   The filter's history slack.
	 */
	ANSamToneDetector(unsigned int samples1, unsigned int samples2,
			  float threshold, unsigned int flag, float ratio,
			  unsigned int sampleRate, unsigned int blockLen,
			  unsigned int blockSize);

	/**
	 * @brief Destroy the base GenericToneDetector. Declared explicitly
	 * (rather than left implicit) because the object's D1/D2 are ordinary
	 * global symbols, which an implicit destructor never emits that way.
	 */
	~ANSamToneDetector();
};

#endif /* DSPLIB_ANSAMTONEDETECTOR_H */

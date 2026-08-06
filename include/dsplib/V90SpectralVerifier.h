/*
 * V90SpectralVerifier.h -- the V.90 received-spectrum accumulator's state.
 *
 * Reconstructed from dsplibs.o.  Twelve members, 2,764 bytes; `reset()` is
 * the one written here and the one `v34handshak` reaches.
 *
 * NOT POLYMORPHIC: `~V90SpectralVerifier` is listed with `D1` and `D2` and no
 * `D0`, so there is no vptr.
 *
 * THE OBJECT IS 44 BYTES.  The largest `this`-relative displacement across
 * all fourteen defined members is +0x28, four bytes wide, so the object ends
 * at 0x2c.  `process(float *, unsigned int)` reaches +0x2c0 and +0x2a4, but
 * not through `this`: both are inside the block whose address `this + 0x00`
 * holds, which is the trap finding 234 recorded for `V90PreFilter`.
 *
 * WHAT `reset()` WRITES.  Three consecutive words, all set to zero, and
 * nothing else -- the whole body is one `edprintf` and three stores.  Two of
 * the three are named from what other members do with them, which was
 * measured while bounding the object:
 *
 *     +0x20  `process` loads it and compares it against +0x10 before doing
 *            any work; `startAccumulation` also sets it to 0.  A count of
 *            what has been accumulated so far, against a target at +0x10.
 *     +0x24  `startAccumulation` returns immediately unless it is 1, then
 *            sets it to 1; `process` returns immediately unless it is 1.
 *            The gate that says an accumulation is running.
 *     +0x28  stored zero HERE AND NOWHERE ELSE.  No member of the class
 *            reads it and no other writes it, so it gets an offset-derived
 *            name: naming it for a purpose would be inventing one.
 *
 * Everything below +0x20 is `pad_`.  It holds at least a pointer to the
 * parameter block (+0x00), a float the frequency-to-bin arithmetic divides by
 * (+0x14) and the spectrum buffer (+0x1c, indexed with `flds (%edx,%eax,4)`
 * by `getSpectrumOfBin`), none of which `reset()` touches.
 */

#ifndef DSPLIB_V90SPECTRALVERIFIER_H
#define DSPLIB_V90SPECTRALVERIFIER_H

class V90SpectralVerifier {
public:
	/* Defined in src/pump/v90/V90SpectralVerifier.cpp. */
	void reset();

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	unsigned char pad_00[0x20];	/* +0x00 params, scale, spectrum    */
	unsigned int accumCount;	/* +0x20 progress toward +0x10      */
	unsigned int accumulating;	/* +0x24 1 while an accumulation runs */
	unsigned int word_28;		/* +0x28 written only by reset()    */
};

#endif /* DSPLIB_V90SPECTRALVERIFIER_H */

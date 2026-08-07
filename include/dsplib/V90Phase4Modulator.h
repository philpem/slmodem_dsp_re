/*
 * V90Phase4Modulator.h -- the V.90 / V.92 phase 4 downstream symbol source.
 *
 * Reconstructed from dsplibs.o.  Forty-three members and 12,078 bytes of
 * code, of which ONE is written here: `setSessionFlag`, the only member of
 * the class `v34handshak` reaches.  Its whole body is three instructions.
 *
 * NOT POLYMORPHIC: `~V90Phase4Modulator` is listed with `D1` and `D2` and no
 * `D0`, so offset 0 is a real member and there is no vptr.
 *
 * THE OBJECT IS 12,204 BYTES, and the shape of the evidence matters because
 * the number is so much larger than its siblings'.  The largest
 * `this`-relative displacement across all forty-five defined members is
 * +0x2fa8, four bytes wide, so the object ends at 0x2fac.  The displacements
 * are not scattered: they fall in two dense runs, +0x00..+0x58 in fours and
 * +0x2f64..+0x2fa8 in twos and then fours, with nothing between.  A run of
 * consecutive two-byte fields at +0x2f68..+0x2f8c, written by the constructor
 * and read by `recivedCPtag`, `recivedSUV`, `resetBeforRRN`, `reset` and the
 * destructor, is a field block and not an artefact of losing track of which
 * register held `this`.  The ~12 KB between the two runs is the constellation
 * the modulator maps into.
 *
 * That is a BOUND on the object, arrived at the same way as every other size
 * in this task (finding 215): the maximum displacement plus the width of what
 * sits at it.  It is not a claim that +0x5c..+0x2f63 contains no larger
 * member -- nothing reaches past +0x2fab, which is all a displacement scan
 * can say.
 *
 * `sessionFlag` at +0x00 is named for the method that writes it, exactly as
 * `V90Phase3Modulator::setSessionFlag` and `V90Modulator::setSessionFlag`
 * write their own classes' +0x000; in `V90Phase3Modulator` the field's
 * meaning is settled -- nonzero selects V.92 -- and this class's setter has
 * the same signature and the same one-line body.  Everything after it is
 * `pad_`, because forty-two unwritten members' state is not something a
 * scan of displacements can name.
 */

#ifndef DSPLIB_V90PHASE4MODULATOR_H
#define DSPLIB_V90PHASE4MODULATOR_H

class V90Phase4Modulator {
public:
	/* Defined in src/pump/v90/V90Phase4Modulator.cpp. */
	void setSessionFlag(unsigned int);

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	unsigned int sessionFlag;	/* +0x0000                          */
	unsigned char pad_0004[0x2fa8];	/* +0x0004 forty-two members' state */
};

#endif /* DSPLIB_V90PHASE4MODULATOR_H */

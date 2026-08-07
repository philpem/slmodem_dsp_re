/*
 * V92EchoCanceller.h -- the V.92 upstream echo canceller's state.
 *
 * Reconstructed from dsplibs.o.  Twelve members, 3,622 bytes; `setEchoDelay`
 * is the one written here and the one `v34handshak` reaches.
 *
 * NOT POLYMORPHIC: `~V92EchoCanceller` is listed with `D1` and `D2` and no
 * `D0`, so there is no vptr.
 *
 * THE OBJECT IS 60 BYTES.  The largest `this`-relative displacement across
 * all fourteen defined members is +0x38, four bytes wide, so the object ends
 * at 0x3c.
 *
 * +0x2c IS A TAP COUNT, NOT A POINTER, and the object's own arithmetic is
 * what settles it.  `setEchoDelay` does
 *
 *     add %ecx,0x2c(%eax)          ecx = newDelay - oldDelay
 *
 * with the delta UNSCALED.  Had +0x2c been a `float *` into the coefficient
 * array, C++ pointer arithmetic would have multiplied the delta by four and
 * GCC would have emitted the shift; it does not.  `resetEchoHistory` then
 * builds the same field from scratch --
 *
 *     ecx = (this->+0x14 >> 1) + this->echoDelay + this->params->+0x74
 *
 * -- and immediately uses it as the bound of a loop that zeroes `+0x24[i]`
 * with a 4-byte stride.  So it counts the active taps, it moves one for one
 * with the delay, and `setEchoDelay` is keeping it in step rather than
 * recomputing it.
 *
 * +0x38 IS `echoDelay`, from the method name and from the diagnostic the
 * method ends with, which prints the value just stored.
 *
 * Everything else is `pad_`: the region holds the parameter-block pointer
 * (+0x00), two buffers (+0x14, +0x24), a write index (+0x28) and two floats
 * `setEchoBeta` and `setDecayFactor` guard with `fcomps` (+0x30, +0x34), none
 * of which this method touches.
 */

#ifndef DSPLIB_V92ECHOCANCELLER_H
#define DSPLIB_V92ECHOCANCELLER_H

class V92EchoCanceller {
public:
	/* Defined in src/pump/v90/V92EchoCanceller.cpp. */
	void setEchoDelay(unsigned int);

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	unsigned char pad_00[0x2c];	/* +0x00 params, buffers, index     */
	unsigned int echoLength;	/* +0x2c active taps               */
	unsigned char pad_30[0x08];	/* +0x30 beta, decay factor        */
	unsigned int echoDelay;		/* +0x38                            */
};

#endif /* DSPLIB_V92ECHOCANCELLER_H */

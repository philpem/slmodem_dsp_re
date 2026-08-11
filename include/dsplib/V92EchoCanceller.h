/*
 * V92EchoCanceller.h -- the V.92 upstream echo canceller's state.
 *
 * Reconstructed from dsplibs.o.  Twelve members, 3,622 bytes; three of them
 * are written here -- `setEchoDelay`, which is the one `v34handshak` reaches,
 * `reset`, and the destructor, which the object emits as the byte-identical
 * pair `D1` and `D2` (finding 1270).
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
 * THE `pad_00` REGION IS NOW SEVEN NAMED FIELDS AND THREE HOLES, and every
 * name below is either a method name's or a diagnostic string's.  `reset()`
 * and `~V92EchoCanceller()` between them touch +0x00, +0x04, +0x08, +0x14,
 * +0x20, +0x24, +0x28, +0x30 and +0x34, and what each one is is forced by
 * how it is used rather than by what it is called:
 *
 *   +0x00  dereferenced for a 4-byte read at its +0x74, which is
 *          `V92_ECHO_DELAY_OFFSET` in V92Parameters.h.
 *   +0x04  handed to `FloatARMA::reset` by `reset`, and to
 *          `FloatARMA::~FloatARMA` and then `sysdep_free` by the destructor
 *          -- so it is an OWNED pointer to a FloatARMA, not an embedded one.
 *   +0x14  the bound of the loop that zeroes +0x20, and the `>> 1` term of
 *          `echoLength`.  D72 reads it as `V92_ECHO_FILTER_LENGTH & ~3`.
 *   +0x20  a 4-byte-stride array zeroed for +0x14 entries by the body the
 *          object also emits standalone as `zeroEchoCoeff`.
 *   +0x24  a 4-byte-stride array zeroed for `echoLength` entries by the body
 *          the object also emits standalone as `resetEchoHistory`.
 *   +0x28  set to zero beside +0x24's loop.  `updateEchoHistory` is what
 *          would prove it is the write cursor; the name is invented on the
 *          strength of the pairing alone (finding 226).
 *   +0x30, +0x34  `echoBeta` and `echoBetaDecay`, NAMED BY THE OBJECT: the
 *          format strings `setEchoBeta` and `setDecayFactor` print are
 *          "V92EchoCanceller: echoBeta = %c%d.%06d\r\n" and
 *          "V92EchoCanceller: echoBetaDecay = %c%d.%06d\r\n".
 *
 * The three holes stay `pad_`.  D72 records +0x1c as the history's ALLOCATED
 * length, from a constructor nothing here has read; it is left unnamed rather
 * than transcribed, which is the same rule the rest of this file follows.
 */

#ifndef DSPLIB_V92ECHOCANCELLER_H
#define DSPLIB_V92ECHOCANCELLER_H

class FloatARMA;
class V92Parameters;

class V92EchoCanceller {
public:
	/* Defined in src/pump/v90/V92EchoCanceller.cpp. */
	void setEchoDelay(unsigned int);
	void reset();

	/*
	 * DECLARING THIS COSTS THE CLASS ITS TRIVIALITY, and that is not free:
	 * a class with a user-declared destructor cannot be a union member, so
	 * `test/unit/t_v90leaves.cpp`'s `union ec_slot` had to become a byte
	 * array with a cast.  It is declared anyway because the object has one
	 * -- `D1` and `D2`, 195 bytes each and byte-identical -- and because
	 * one C++ definition is what produces exactly that pair.
	 */
	~V92EchoCanceller();

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	V92Parameters *params;		/* +0x00 not owned                  */
	FloatARMA *arma;		/* +0x04 OWNED; freed by ~this       */
	unsigned int word_08;		/* +0x08 cleared by `reset`          */
	unsigned char pad_0c[0x08];	/* +0x0c                             */
	unsigned int filterLength;	/* +0x14 taps in `echoCoeff`         */
	unsigned char pad_18[0x08];	/* +0x18 (+0x1c: see D72)            */
	float *echoCoeff;		/* +0x20 OWNED; freed by ~this       */
	float *echoHistory;		/* +0x24 OWNED; freed by ~this       */
	unsigned int historyIndex;	/* +0x28 invented; see the comment   */
	unsigned int echoLength;	/* +0x2c active taps               */
	float echoBeta;			/* +0x30                             */
	float echoBetaDecay;		/* +0x34                             */
	unsigned int echoDelay;		/* +0x38                            */
};

#endif /* DSPLIB_V92ECHOCANCELLER_H */

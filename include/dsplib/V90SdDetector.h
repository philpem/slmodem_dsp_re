/*
 * V90SdDetector.h -- the V.90 SD (signalling-drop?) detector's state.
 *
 * Reconstructed from dsplibs.o.  Four members, 353 bytes; `reset()` is the
 * one written here and the one `v34handshak` reaches.
 *
 * NOT POLYMORPHIC: `tools/cppstruct.py` lists `~V90SdDetector` with `D1` and
 * `D2` and no `D0`, so there is no vptr and offset 0 is a real member.
 *
 * THE OBJECT IS 28 BYTES.  The largest `this`-relative displacement across
 * all six defined members (the four in cppstruct's list plus the C1/C2 and
 * D1/D2 pairs) is +0x18, four bytes wide, so the object ends at 0x1c.
 *
 * EVERY WORD IS NAMED, and the constructor is what named the middle four.
 * `reset()` alone reached +0x00, +0x14 and +0x18 and left +0x04..+0x10 as a
 * `pad_`; the constructor stores all four, from its own arguments, so the
 * region is now fields.  What the OTHER members do with them was measured
 * while bounding the object and is recorded rather than declared:
 *
 *     +0x00  `process(float)` loads it, increments it, compares it against
 *            +0x04 with `jb` -- an UNSIGNED compare -- and stores it back;
 *            on the failing path it stores 0.  A run-length counter, and the
 *            one field `reset()` clears by name.
 *     +0x04  the limit +0x00 is compared against.  CONSTRUCTOR ARGUMENT 4,
 *            which the mangling types `unsigned int`.
 *     +0x08  CONSTRUCTOR ARGUMENT 1.  `flds`, compared against an energy
 *            accumulator; the comparison failing is what zeroes +0x00.
 *     +0x0c  CONSTRUCTOR ARGUMENT 2.  `flds`, compared against a quotient
 *            formed from that accumulator.
 *     +0x10  CONSTRUCTOR ARGUMENT 3, and read by none of the six.
 *
 * ARGUMENTS 3 AND 4 ARE STORED OUT OF ORDER -- argument 4 into +0x04 and
 * argument 3 into +0x10 -- which is why the test sweeps three DISTINCT float
 * values every trial: a pair of equal arguments makes a swap invisible.
 *
 * THE HISTORY IS TWELVE FLOATS WHATEVER THE ARGUMENTS SAY.  The constructor
 * stores the constant 12 into `historyLength` and asks for `0x30` bytes; no
 * argument reaches either.  `reset()` clears `historyLength` of them, so the
 * length is a field and not a constant, but the constructor never varies it.
 *
 * THE HISTORY IS A HEAP BUFFER OF FLOATS.  `process` shifts it up by one
 * element with a 4-byte stride, writes the incoming sample at [0], and then
 * correlates `history[i]` against `history[i + 6]` for i in 0..5 with `flds`
 * and `fmuls` -- so the element type is `float` and the constructor's
 * `movl $0xc,0x18(%ebx)` (twelve) is exactly the two lags that correlation
 * needs.  `reset()` clears `historyLength` of them, not twelve, which is why
 * the length is a field and not a constant.
 */

#ifndef DSPLIB_V90SDDETECTOR_H
#define DSPLIB_V90SDDETECTOR_H

class V90SdDetector {
public:
	/* Defined in src/pump/v90/V90SdDetector.cpp. */
	V90SdDetector(float thresh08, float thresh0c, float value10,
		      unsigned int limit);
	~V90SdDetector();

	void reset();

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	unsigned int count;		/* +0x00 the run-length counter     */
	unsigned int limit;		/* +0x04 what +0x00 is compared to  */
	float thresh_08;		/* +0x08 argument 1                 */
	float thresh_0c;		/* +0x0c argument 2                 */
	float value_10;			/* +0x10 argument 3, read by nothing */
	float *history;			/* +0x14 the correlator's history   */
	unsigned int historyLength;	/* +0x18 elements in it; ctor sets 12 */
};

#endif /* DSPLIB_V90SDDETECTOR_H */

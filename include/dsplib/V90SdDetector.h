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
 * WHAT `reset()` NEEDS, and nothing more.  Three offsets are modelled as
 * fields because `reset()` touches them; the rest is `pad_`, because the rule
 * here is that an unmodelled region stays `pad_` rather than being guessed
 * into fields (docs/v90cpp.md).  What the OTHER members do to that region was
 * measured while bounding the object and is recorded here rather than
 * declared, so the next agent does not have to re-derive it:
 *
 *     +0x00  `process(float)` loads it, increments it, compares it against
 *            +0x04 with `jb` -- an UNSIGNED compare -- and stores it back;
 *            on the failing path it stores 0.  A run-length counter, and the
 *            one field `reset()` clears by name.
 *     +0x04  the limit +0x00 is compared against.  Constructor argument 4,
 *            which the mangling types `unsigned int`.
 *     +0x08  `flds`, compared against an energy accumulator; the comparison
 *            failing is what zeroes +0x00.
 *     +0x0c  `flds`, compared against a quotient formed from that
 *            accumulator.
 *     +0x10  written by the constructor and read by none of the six.
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
	void reset();

	/* Public for offsetof; see V90ConstellationDesigner.h. */
	unsigned int count;		/* +0x00 the run-length counter     */
	unsigned char pad_04[0x10];	/* +0x04 limit, two thresholds, one */
					/*       field nothing reads        */
	float *history;			/* +0x14 the correlator's history   */
	unsigned int historyLength;	/* +0x18 elements in it; ctor sets 12 */
};

#endif /* DSPLIB_V90SDDETECTOR_H */

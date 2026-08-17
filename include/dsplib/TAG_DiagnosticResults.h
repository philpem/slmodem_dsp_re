/*
 * TAG_DiagnosticResults.h -- the record the data-mode diagnostics API fills
 * in, shared by V.34, V.90 and K56Flex.
 *
 * Reconstructed from dsplibs.o.  The name is the object's own, out of the
 * mangling of the only member that names it in a signature:
 *
 *     _ZNK14V90Demodulator8getAT_UDEP21TAG_DiagnosticResults
 *      -> V90Demodulator::getAT_UD(TAG_DiagnosticResults *) const
 *
 * `struct` rather than `class` is want of evidence -- the mangling carries no
 * class-key -- and follows the object's other `tag`-prefixed records
 * (`tagV90DILdescriptor`, `tagV90AdditionalCPinfo`).  It changes nothing:
 * every member here is public data either way.
 *
 * ---------------------------------------------------------------------------
 * THE SIZE IS A LOWER BOUND AND IS DECLARED AS ONE, THE WAY `v34_object`'S
 * TAIL WAS
 *
 * There is no allocation site for this record anywhere in the object.  Every
 * function that fills one receives a pointer from OUTSIDE dsplibs.o -- the
 * three entry points are `VPcmV34GetDiagnostics`, `VPcmV34GetVisualDiagnostics`
 * and this class's `getAT_UD`, and the first two are the library's external
 * API, so the caller that sizes the buffer is in the application.  `sizeof`
 * is therefore NOT recoverable and nothing here should be read as claiming it.
 *
 * What IS measured is the highest offset any writer in the object touches:
 * `VPcmV34GetDiagnostics` stores a word at +0x228, which ends at 0x22c.  So
 * the record is AT LEAST 0x22c bytes and the trailing `pad_` runs to exactly
 * there and no further.  If a later batch finds a writer past it, extend the
 * pad and say so; do not treat 0x22c as a size that was established.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS MODELLED, AND WHY THE REST IS PAD
 *
 * Two functions write this record and only one of them is reconstructed.  The
 * fields below are the union of what both TOUCH, typed from the instruction
 * that touches them; everything else is `pad_`, which is CLAUDE.md's "we do
 * not know how many fields are in it" and not a claim that the space is
 * unused.  The offsets, and who writes each:
 *
 *     +0x068  float   V.90 only
 *     +0x06c  float   V.34 only
 *     +0x070  float   both
 *     +0x074  float   both      -- V.34 stores it with `fsts`, V.90 with a
 *                                  32-bit `mov` of the same bit pattern,
 *                                  which is how a float copy compiles.  Two
 *                                  independent writers agreeing on the type
 *                                  is the check that it is one.
 *     +0x080  word    V.34 only
 *     +0x084  word    both
 *     +0x0b0  word    V.34 only
 *     +0x0b4  word    both
 *     +0x0b8  word    V.34 only
 *     +0x0bc  word    both
 *     +0x0c0  word    V.34 only
 *     +0x0c4  word    V.34 only
 *     +0x0e8  word    V.34 only
 *     +0x0ec  word    both
 *     +0x0f0  word    both
 *     +0x0f4  word    both
 *     +0x0f8  word    V.34 only
 *     +0x0fc  word    both
 *     +0x100  word    V.34 only
 *     +0x220  word    V.90 only
 *     +0x228  word    V.34 only
 *
 * The V.34 half is listed for the record and is NOT modelled as fields here.
 * `VPcmV34GetDiagnostics` is unwritten, so nothing in this tree can be tested
 * against those offsets, and a field declared from a disassembly nobody has
 * reconstructed is exactly the wrong-but-plausible this project refuses.  The
 * batch that writes that function models them, per docs/plan.md section 3.
 *
 * ---------------------------------------------------------------------------
 * NAMING
 *
 * Two of the three named fields come from a format string, which is the
 * strongest evidence CLAUDE.md recognises, and the third from a callee's
 * mangling plus an arithmetic identity.  Everything else keeps an offset name.
 * See the comment on each.
 */

#ifndef DSPLIB_TAG_DIAGNOSTICRESULTS_H
#define DSPLIB_TAG_DIAGNOSTICRESULTS_H

struct TAG_DiagnosticResults {
	unsigned char pad_000[0x68];		/* +0x000                    */

	/*
	 * +0x068  `10.0f * log10f(x)`, so a level in dB.  V.90 fills it from
	 * `V90Demodulator`'s AGC gain at its +0x5c; nothing else writes it.
	 * WHAT the level is of is not settled -- no format string prints this
	 * offset -- so the name stays an offset name.
	 */
	float float_068;

	float float_06c;			/* +0x06c  V.34 only         */

	/*
	 * +0x070  Also `10.0f * log10f(x)`, and of the value stored raw at
	 * +0x074 below, so the two are the same quantity in dB and linear.
	 * Same want of evidence for what it measures.
	 */
	float float_070;

	/*
	 * +0x074  The linear value whose dB form is +0x070.  V.90 copies it
	 * out of `V90Equalizer` +0x80.
	 */
	float float_074;

	unsigned char pad_078[8];		/* +0x078                    */

	unsigned int word_080;			/* +0x080  V.34 only         */

	/*
	 * +0x084  THE ROUND TRIP DELAY, and the name is the object's own:
	 * `getAT_UD` computes it from `V90Phase2Info::rtd`, which
	 * `V90Phase2Info::printInfo` prints under that label.  The scaling is
	 * `rtd * 10 / 96` -- exactly `rtd / 9.6` -- computed UNSIGNED (see the
	 * .cpp; the absence of a sign correction is what says so).
	 *
	 * 9.6 samples per unit is what a 9600 Hz clock gives per millisecond,
	 * so this is very probably milliseconds, but that is usage inference
	 * and no format string in the object prints this offset with a unit.
	 * The name states the quantity, which IS established, and stops short
	 * of the unit, which is not.
	 */
	unsigned int roundTripDelay;

	unsigned char pad_088[0x28];		/* +0x088                    */

	unsigned int word_0b0;			/* +0x0b0  V.34 only         */

	/*
	 * +0x0b4  V.90 stores the literal 8000 here and V.34 stores a 16-bit
	 * field widened.  8000 is the V.90 downstream symbol rate and V.34's
	 * symbol rates all fit a short, so a baud figure is the obvious
	 * reading -- but `getAT_UD` is the receive side only and the V.34 arm
	 * writes +0x0b0 and +0x0b4 from two different fields, so which of the
	 * pair is transmit and which receive is not settled by anything in
	 * this batch.  Offset name, and the derivation is here rather than in
	 * a guessed one.
	 */
	unsigned int word_0b4;

	unsigned int word_0b8;			/* +0x0b8  V.34 only         */
	unsigned int word_0bc;			/* +0x0bc  V.90 stores 0     */
	unsigned char pad_0c0[0x2c];		/* +0x0c0                    */
	unsigned int word_0ec;			/* +0x0ec                    */
	unsigned int word_0f0;			/* +0x0f0                    */
	unsigned int word_0f4;			/* +0x0f4                    */
	unsigned char pad_0f8[4];		/* +0x0f8  V.34 only         */

	/*
	 * +0x0fc  THE DATA RATE IN BIT/S, and a format string names it.
	 * `getAT_UD` stores exactly what `V90Demodulator::getBitRate()`
	 * returns, and `V90Demodulator::enterDataPhase` passes that same
	 * expression to
	 *
	 *     "V90Demodulator: enter Data Phase, Rate = %d [bps]\r\n"
	 *
	 * -- the author's own words for the quantity and its unit.  The V.34
	 * arm writes a multiple of 2400 into the same slot, which is a V.34
	 * data rate, so the two writers agree.
	 */
	unsigned int dataRate;

	unsigned char pad_100[0x120];		/* +0x100                    */

	/*
	 * +0x220  THE ROBBED-BIT-SIGNALLING PATTERN, six bits wide, and both
	 * halves of the name are the object's own.  `getAT_UD` builds it out
	 * of the six bytes of `V90AutoDigitalImpDetector::byte_280c` -- one
	 * per frame phase, `V90ADID_PHASES` is 6 -- as
	 *
	 *     b[0] + 2*b[1] + 4*b[2] + 8*b[3] + 16*b[4] + 32*b[5]
	 *
	 * and prints it beside the six bytes it was built from:
	 *
	 *     "RBS : %d (%d%d%d%d%d%d)\r\n"
	 *
	 * So the label names the field, the parenthesised six name the width,
	 * and the packing says bit N is frame phase N.  That the source bytes
	 * are the digital-impairment detector's per-phase "suspected" flags is
	 * what makes RBS the right reading of the label rather than a
	 * coincidence of initials.
	 */
	unsigned int rbsPattern;

	/*
	 * +0x224 .. +0x22b  The tail, and the reason the record is known to
	 * reach 0x22c: `VPcmV34GetDiagnostics` stores a word at +0x228.
	 * Nothing bounds it from above -- see the file comment.
	 */
	unsigned char pad_224[8];
};

#endif /* DSPLIB_TAG_DIAGNOSTICRESULTS_H */

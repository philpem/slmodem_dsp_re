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
 * ALL TWENTY-TWO WRITTEN OFFSETS ARE FIELDS NOW.  Both writers are
 * reconstructed -- `V90Demodulator::getAT_UD` (src/pump/v90/V90Demodulator.cpp)
 * and `VPcmV34GetDiagnostics` (src/pump/v34/v34diag.cpp) -- so the eleven that
 * were listed here "for the record" while only one writer existed are typed
 * from the instruction that touches them, like the other eleven.  Finding
 * F5501.
 *
 * Everything between them is still `pad_`, which is CLAUDE.md's "we do not
 * know how many fields are in it" and not a claim that the space is unused:
 * 0x22c bytes hold far more than twenty-two words, and the application that
 * allocates the record reads the rest.
 *
 * ---------------------------------------------------------------------------
 * NAMING, AND THE TWO PLACES THE TWO WRITERS DISAGREE
 *
 * The evidence order is CLAUDE.md's.  Nine fields have names; thirteen keep an
 * offset name with the derivation on them, and two of those thirteen keep it
 * for a reason worth stating up front, because it is a RESULT and not a gap:
 *
 *   +0x074  V.90 stores a linear quantity and V.34 stores a dB one.  It is
 *           not one quantity, so it cannot have one name.
 *   +0x070  Both writers store a dB figure, and of quantities with OPPOSITE
 *           polarity -- V.90's is a mean error energy (bigger is worse),
 *           V.34's is a signal-to-noise ratio (bigger is better).
 *
 * See the comments on each, and finding F5502.
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

	/*
	 * +0x06c  V.34 and the two ANALOG PCM arms only, and the derivation
	 * is complete without the unit being settled.
	 *
	 * The V.34 arm stores `-12.0f - (float)v34_object::tx_pwr_reduction`, and
	 * `tx_pwr_reduction` is "the transmit power reduction in WHOLE dB" -- its own
	 * diagnostic says so, "power reduction requested by remote modem is
	 * %d dB".  The V.92 analog arm stores the constant -12.0f with no
	 * reduction subtracted at all.  So this is a transmit level in dB,
	 * relative to whatever -12 is a level of, and the two writers agree
	 * on the shape.
	 *
	 * WHAT -12 IS A LEVEL OF IS NOT ESTABLISHED, and that is the whole
	 * reason for the offset name: dBm is the obvious reading for a modem
	 * transmit level and nothing in the object states it.  3120's
	 * precedent -- the derivation lives here rather than in a name that
	 * would be believed.
	 */
	float float_06c;

	/*
	 * +0x070  A dB FIGURE UNDER BOTH WRITERS AND NOT THE SAME QUANTITY,
	 * which is why it keeps an offset name (finding F5502).
	 *
	 * V.90: `10.0f * log10f(V90Equalizer::meanErrorEnergyCurrent)` -- a
	 * mean ERROR energy, so larger means a worse line.
	 *
	 * V.34: the integer dB count `VPcmV34GetSNR` returns, converted to
	 * float.  `VPcmV34GetDiagnostics` inlines that function's body
	 * verbatim -- the same `f248 / equerr` ratio, the same 0x1013 and
	 * 0x32d6 reciprocal steps -- and the object's own name for the
	 * function computing it is `GetSNR`, so larger means a BETTER line.
	 *
	 * Two writers, one offset, opposite polarity.  Naming it for either
	 * would be believed by every future reader of the other.
	 */
	float float_070;

	/*
	 * +0x074  NOT ONE QUANTITY EITHER, and this comment used to claim it
	 * was: it read "the linear value whose dB form is +0x070", which was
	 * true of the only writer known when it was written.
	 *
	 * V.90 stores `V90Equalizer::meanErrorEnergyCurrent` raw, so under
	 * that writer it IS the linear partner of +0x070.  V.34 stores the
	 * same integer dB count it puts in +0x070, with one `fsts` and one
	 * `fstps` off a single x87 value -- so under that writer the two
	 * offsets hold the same number and neither is linear.
	 *
	 * The float type survives both readings and is doubly witnessed: V.34
	 * stores it with `fsts` and V.90 with a 32-bit `mov` of the same bit
	 * pattern, which is how a float copy compiles.
	 */
	float float_074;

	unsigned char pad_078[8];		/* +0x078                    */

	/*
	 * +0x080  V.34 only, and it takes `v34_object::rtd` -- the round-trip
	 * delay in samples -- UNSCALED, in the same statement that puts the
	 * same value in `roundTripDelay` below.
	 *
	 * OFFSET-NAMED DESPITE THE SOURCE BEING NAMED.  What this offset is
	 * FOR is not established: the only thing distinguishing it from
	 * +0x084 is that the V.90 writer skips it, which is a shape and not a
	 * meaning, and the V.34 writer gives the two fields the same value in
	 * units +0x084's other writer does not use (see there).
	 */
	unsigned int word_080;

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
	 *
	 * AND THE SECOND WRITER MAKES THAT RESTRAINT LOAD-BEARING.  V.34
	 * stores its own `v34_object::rtd` here with NO scaling at all, so
	 * the two writers disagree about the unit by a factor of 9.6.  The
	 * quantity is the same under both; the unit was never one thing.
	 * Finding F5502.
	 */
	unsigned int roundTripDelay;

	unsigned char pad_088[0x28];		/* +0x088                    */

	/*
	 * +0x0b0 .. +0x0bc  THE FOUR SYMBOL-RATE AND CARRIER FIELDS, and all
	 * four are named by a CALLEE'S OWN FIELD NAMES -- CLAUDE.md's second
	 * evidence tier -- rather than by the layout they happen to sit in.
	 *
	 * The V.34 arm of `VPcmV34GetDiagnostics` copies four members of
	 * `struct v34_ratecfg` (v34fsk.h, at `v34_object + V34_RATECFG`)
	 * into them, one for one:
	 *
	 *     +0x0b0 <- cfg->baud        +0x0b4 <- cfg->rx_baud
	 *     +0x0b8 <- cfg->carrier     +0x0bc <- cfg->rx_carrier
	 *
	 * and those four members were themselves named by four exported
	 * getters that state the direction in their own names:
	 * `VPcmV34GetCurrentTxBaudRate` returns `cfg->baud`,
	 * `...GetCurrentRxBaudRate` returns `cfg->rx_baud`,
	 * `...GetCurrentTxCarrier` returns `cfg->carrier` and
	 * `...GetCurrentRxCarrier` returns `cfg->rx_carrier`.
	 *
	 * +0x0b4 WAS DECLINED BY THE `getAT_UD` BATCH and is settled now.
	 * That batch could see only that V.90 stored the literal 8000 into
	 * +0x0b4 and that V.34 stored a widened short into both +0x0b0 and
	 * +0x0b4, and it correctly refused to guess which of the pair was
	 * which.  The two independent confirmations are:
	 *
	 *   - `getAT_UD` is the RECEIVE side and writes +0x0b4 = 8000, which
	 *     is the V.90 downstream symbol rate;
	 *   - the V.92 analog arm of `VPcmV34GetDiagnostics` writes
	 *     +0x0b0 = 8000, which is the V.92 UPSTREAM symbol rate -- the
	 *     transmit direction seen from the same analog modem.
	 *
	 * Finding F5500.  `PCMIF_PCM_BAUD` in src/pump/v34/v34pcmif.c is the
	 * same 8000 under the same reading.
	 */
	unsigned int txBaudRate;		/* +0x0b0 */
	unsigned int rxBaudRate;		/* +0x0b4 */
	unsigned int txCarrier;			/* +0x0b8 */
	unsigned int rxCarrier;			/* +0x0bc */

	/*
	 * +0x0c0, +0x0c4  The negotiated bit rates as they were LATCHED, and
	 * the names come from the fields copied in: `VPcmV34GetDiagnostics`
	 * stores `v34_object::tx_bps` and `::rx_bps` here verbatim, on every
	 * path, and v34fsk.h has those two as "the negotiated rates in bits
	 * per second -- 2400 times the counts in the rate config -- published
	 * once and latched, so a second negotiation does not overwrite them".
	 *
	 * `Latched` is in the name because it is the ONLY thing separating
	 * this pair from `txDataRate`/`dataRate` below, which the same
	 * function fills from the live rate config in the same call.  A
	 * reader who cannot tell the two pairs apart has two fields that look
	 * like duplicates.
	 */
	unsigned int txBpsLatched;		/* +0x0c0 */
	unsigned int rxBpsLatched;		/* +0x0c4 */

	unsigned char pad_0c8[0x20];		/* +0x0c8                    */

	/*
	 * +0x0e8, +0x0f0, +0x100  ONE SOURCE, THREE DESTINATIONS.  The V.34
	 * writer loads `v34_object + 0xac12` as a signed short, once, and
	 * stores the widened result into all three of these offsets; the V.90
	 * writer puts `V90Demodulator::word_268` in +0x0f0 and touches
	 * neither of the others.
	 *
	 * +0xac12 has no name in v34fsk.h either -- `VPcmV34Create` clears it
	 * and `v34handshakinit` reads it back and rewrites it, and no format
	 * string in the object prints it -- so there is nothing to inherit
	 * and all three keep offset names.
	 */
	unsigned int word_0e8;			/* +0x0e8 */

	/*
	 * +0x0ec  THE TOTAL NUMBER OF RATE RENEGOTIATIONS.  V.34 stores
	 * `rrn_local + rrn_remote`, both signed shorts of `v34_object`, and
	 * those two are named from whole functions: `VPcmV34IndicateLocalRRN`
	 * is nothing but the increment of the first and
	 * `VPcmV34IndicateRemoteRRN` nothing but the increment of the second.
	 *
	 * OFFSET-NAMED ALL THE SAME, because the V.90 writer puts
	 * `V90Demodulator::word_264` here instead and nothing establishes
	 * that the two mean one thing.  The V.34 derivation is recorded; a
	 * name would claim it held for both.
	 *
	 * The sum is computed on the SIGNED shorts, so a session past 32,767
	 * local renegotiations reports a negative count.  Not entered as a
	 * deviation: v34fsk.h already declined the same one at the source, on
	 * the ground that it takes a run no real call would reach.
	 */
	unsigned int word_0ec;

	unsigned int word_0f0;			/* +0x0f0  see +0x0e8        */

	/*
	 * +0x0f4  V.34 stores `v34_object + 0xac14` widened from a signed
	 * short; V.90 stores `V90Demodulator::word_26c`.  +0xac14 is the
	 * neighbour of +0xac12 above and is unnamed for the same reason.
	 */
	unsigned int word_0f4;

	/*
	 * +0x0f8  THE TRANSMIT DATA RATE IN BIT/S, and it is the twin of
	 * `dataRate` below rather than a second reading of it.
	 *
	 * V.34 stores `2400 * cfg->txbits` here and `2400 * cfg->rxbits` in
	 * `dataRate`, from the same `struct v34_ratecfg` whose members the
	 * four rate/carrier fields above are named from; `txbits` is
	 * v34fsk.h's "+0x04, in units of 2400 bps" and is what
	 * `VPcmV34GetCurrentTxBitRate` reads.  The V.92 analog arm computes
	 * an upstream figure for this offset and leaves `dataRate` to
	 * `getAT_UD`, which is the receive side -- so the pairing holds
	 * across all three writers.
	 */
	unsigned int txDataRate;

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
	 *
	 * IT IS THE RECEIVE ONE.  That was not visible with a single writer;
	 * `txDataRate` above is the transmit twin and the derivation is
	 * there.  The name is left as it was rather than made `rxDataRate`:
	 * it is the name the format string licensed, it is referenced from
	 * three other files, and the direction is now recorded where a reader
	 * of either field will meet it.
	 */
	unsigned int dataRate;

	unsigned int word_100;			/* +0x100  see +0x0e8        */

	unsigned char pad_104[0x11c];		/* +0x104                    */

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

	unsigned char pad_224[4];		/* +0x224                    */

	/*
	 * +0x228  The last offset any writer in the object reaches, and the
	 * reason the record is known to run to 0x22c.
	 *
	 * V.34 only, and the store is conditional in a way worth keeping in
	 * one place: `v34_object::v90_timing_offset` is tested as a short and, when it is
	 * non-zero, sign-extended into this word; when it is zero, -1 is
	 * stored instead.  `v90_timing_offset` is where the V.90 side is told the
	 * recovered timing offset -- `VPcmV34LogTimingOffset` is its writer
	 * and names it -- so -1 here is "no timing offset has been logged"
	 * rather than a value.
	 *
	 * OFFSET-NAMED HERE STILL, even though `v90_timing_offset` itself has
	 * since been named in v34fsk.h (it was `fac0c` when this paragraph was
	 * written): nothing in this tree reads this word back, so there is no
	 * caller here to derive a name for it from, only for its source.
	 */
	unsigned int word_228;
};

#endif /* DSPLIB_TAG_DIAGNOSTICRESULTS_H */

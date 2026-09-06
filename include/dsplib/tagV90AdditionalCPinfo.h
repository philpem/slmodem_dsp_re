/*
 * tagV90AdditionalCPinfo.h -- the 0x18-byte record both V.90 sides carry a
 * pointer to and `V90Modem` embeds.
 *
 * Reconstructed from dsplibs.o.  The name is the object's own, out of the
 * mangling of both `V90Modulator` constructors and `V90Demodulator`'s
 * (`P22tagV90AdditionalCPinfo`).
 *
 * THIS DEFINITION USED TO LIVE IN V90Modem.h, and it moved here for the
 * reason `V90ConnectionEvaluator`'s did: a second file now needs the complete
 * type, and "one type, one home" means the home moves rather than the
 * definition being copied.  V90Modem.h includes this and its embedded member
 * at +0xcb8 is unchanged; V90Modulator.h and V90Demodulator.h still only
 * forward-declare, which a pointer member does not need more than.
 *
 * The comment V90Modem.h carried said "NOTHING IS KNOWN ABOUT ITS CONTENTS --
 * `V90Modem::V90Modem` takes its address and passes it on; it neither reads
 * nor writes a byte of it, and nothing else in this tree touches it."  That
 * was true of the tree when it was written.  `V90Demodulator::enterRRN` now
 * writes +0x10, which is what carved that field out of the pad.
 *
 * AND THE OTHER 0x10 BYTES ARE MODELLED NOW TOO, by the first READER anyone
 * has written: `setV92CPpckFromParamsInfo` (.text+0x33920) copies five dwords
 * out of this record into a `V92CP`, and each copy's width and destination is
 * what types the field.  THAT THE RECORD IS THIS ONE is three independent
 * things agreeing:
 *
 *   - `VPcmFloModem::runPcmModem` passes `this + 0x2410` as argument 2, and
 *     `VPcmFloModem` embeds a `V90Modem` at +0x1758 whose `additionalCPinfo`
 *     is at +0xcb8.  0x1758 + 0xcb8 = 0x2410 exactly.  Argument 1 is
 *     `this + 0x1770` or `this + 0x1dc0`, which are 0x1758 + 0x18 and
 *     0x1758 + 0x668 -- `mappingParams` and `mappingParamsAlt`.
 *   - the V.90 twin of that function is
 *     `V90CPPacker(V90MappingParams *, tagV90AdditionalCPinfo *, short *,
 *     int)`, whose mangling gives the same two types in the same order.
 *   - the five dwords end exactly at +0x14, where `pad_14` already was.
 *
 * NONE OF THE FIVE IS NAMED.  No format string prints any of them, and their
 * destinations in `V92CP` are themselves offset-named for the reason that
 * header sets out at length; carrying an offset name across would be
 * adjacency rather than evidence.  What each one's comment records instead is
 * where it goes, which is recoverable and is what a future reader needs.
 *
 * THE SIZE IS ADJACENCY AND IS NOT ASSERTED.  0xcd0 - 0xcb8 is where 0x18
 * comes from, and no allocation confirms it.
 */

#ifndef DSPLIB_TAGV90ADDITIONALCPINFO_H
#define DSPLIB_TAGV90ADDITIONALCPINFO_H

struct tagV90AdditionalCPinfo {
	/*
	 * +0x00  Loaded whole and stored as ONE BYTE into `V92CP::byte_04` --
	 * `mov (%ecx),%ebx; mov %bl,0x4(%edx)`.  So the field is four bytes
	 * and only its low one survives the copy.  `byte_04` reaches the
	 * message as `bits[33]` and separately gates `V92CP::word_110`.
	 *
	 * FIELD NAMING WAVE 7 -- CHECKED, NOT NAMED.  The V.90-side twin of
	 * `setV92CPpckFromParamsInfo`, `V90CPPacker` (`src/pump/v90/
	 * V90CPpck.cpp`), reaches this same field as `info->word_00` and
	 * types it as a BOOLEAN: bit 33 of the message, and its own debug
	 * line builds the packed message's name as "CP%s%s%s" with
	 * `info->word_00 != 0` selecting the third `%s`, a literal `'`
	 * (so a message with the bit set prints as "...CP'").  That is a
	 * format string (evidence rule 1) for the SHAPE -- a one-bit
	 * message-variant selector -- but not for what setting the bit
	 * MEANS; V90CPpck.cpp's own header comment says as much ("the
	 * object states no field names") and this wave found nothing to
	 * add past that.  Left unnamed rather than guessed at ITU-T's own
	 * CP/CP' distinction from memory.
	 */
	unsigned int word_00;			/* +0x00                  */

	/*
	 * +0x04  The same shape, into `V92CP::char_01`, which that header
	 * measures as SIGNED from its own readers (`jle`, `sar $1`).  The
	 * copy says nothing about the sign of THIS field, so it keeps the
	 * unsigned spelling its neighbours have and the conversion is stated
	 * at the one site that performs it.  `char_01` is what selects the
	 * long form of the CP message, and it also selects between the two
	 * constants `setV92CPpckFromParamsInfo` subtracts at the end.
	 *
	 * FIELD NAMING WAVE 7 -- CHECKED, NOT NAMED.  `V90CPPacker` reaches
	 * it as `info->word_04`: stored whole into `bits[19]` (NOT a
	 * boolean there -- "TRUNCATED TO 16 BITS, not a boolean" per that
	 * function's own header comment) and, separately, tested for zero
	 * to select the same "CP%s%s%s" message name's FIRST `%s` (a
	 * literal `t`, inverted -- `== 0` prints "t") and to pick which of
	 * two `getDataBitRate` branches runs.  So the object reuses this
	 * one field as both a rate-selecting index into `bits[19..24]` and
	 * a boolean gate over the SAME comparison -- two roles on one
	 * field, exactly the shape CLAUDE.md's dual-role rule declines a
	 * single name for (cf. `f208`/`f20a` in `v34recv.h`).  Left
	 * unnamed.
	 */
	unsigned int word_04;			/* +0x04                  */

	/*
	 * +0x08  A FLOAT, and it is the destination that types it: the four
	 * bytes go to `V92CP::flt_10`, which `V92CP::infoToBits` sends as
	 * sixteen magnitude entries weighted 4 down to 2^-13 off `fltTable_2`.
	 * The copy itself is a `movl` -- GCC 3.4.2 copies a float that way at
	 * `-O3`, which was probed rather than assumed (finding F5820) -- so the
	 * width is forced and the type comes from the other end.
	 *
	 * FIELD NAMING WAVE 7 -- CHECKED, NOT NAMED.  `V90CPPacker` reaches
	 * it as `info->float_08`, encoded through `float2Bits` as Q3.13
	 * across bits[52..67] (blocks 3/4) alongside the shaper
	 * coefficients -- consistent with a signal-level magnitude, matching
	 * `flt_10`'s own role on the V.92 side, but no format string or
	 * typed callee on either side names WHICH magnitude.  Left unnamed.
	 */
	float float_08;				/* +0x08                  */

	/*
	 * +0x0c  Four bytes, low one into `V92CP::byte_03`, which is
	 * `bits[35]` stored whole.
	 *
	 * FIELD NAMING WAVE 7 -- CHECKED, NOT NAMED.  `V90CPPacker` reaches
	 * it as `info->word_0c`, stored whole (truncated to a `short`) into
	 * `bits[35]` with no test of its own -- the same "stored, not
	 * branched on" shape as `V92CP::byte_03`, so nothing on this side
	 * adds a boolean or magnitude reading either.
	 */
	unsigned int word_0c;			/* +0x0c                  */

	/*
	 * +0x10  MODELLED, UNNAMED.  `V90Demodulator::enterRRN` stores a 0 or
	 * a 1 here.  The 1 is reached only when the connection evaluator's
	 * +0x90 and both of `V90Phase4Demodulator`'s +0x3c and +0x38 are
	 * non-zero, so it is a conjunction of three other flags recorded at
	 * the moment a rate renegotiation is detected.  The width is the
	 * store's (`mov %edx,0x10(%ecx)`).
	 *
	 * "NOTHING READS IT IN THE OBJECT" USED TO END THAT SENTENCE AND IS
	 * RETRACTED.  `setV92CPpckFromParamsInfo` copies it whole into
	 * `V92CP::suv` (+0x108), which `V92CP::setSUV` also writes and which
	 * `infoToBits` sends as `bits[32]` -- its low byte, whole.  So a flag
	 * raised when a renegotiation is detected reaches the V.92 CP message
	 * as one bit.  That is still not enough to NAME either end: `suv` is
	 * named for its writer and not for its meaning, and finding F4342's
	 * rule is why the retraction is spelled out rather than the sentence
	 * simply deleted.
	 *
	 * FIELD NAMING WAVE 7 -- CHECKED, NOT NAMED.  `V90CPPacker` reaches
	 * it as `info->word_10`, stored into `bits[30]` and ALSO tested for
	 * non-zero to select "CP%s%s%s"'s second `%s`, a literal `s` (a
	 * message with the bit set prints "...CPs...").  So the V.90 side
	 * agrees with the RRN-detected-flag origin above (both reach the
	 * message as one bit set at the same moment) but still names only
	 * the SHAPE (a boolean selecting a CP message variant), not what
	 * "CPs" denotes.  Left unnamed.
	 */
	unsigned int word_10;			/* +0x10                  */

	/*
	 * +0x14  TWO OF `pad_14`'s FOUR BYTES, and `V90Demodulator::
	 * exitPhase3` is the writer that carved them out:
	 * `mov 0x48(%esi),%ebx ; mov %bx,0x14(%ecx)` at 0x1bc43, a whole-word
	 * load of `V90Parameters::ANALOG_RATE_MASK` narrowed to a SIXTEEN-BIT
	 * store.  So the width is forced and the value's origin is
	 * recoverable.
	 *
	 * IT KEEPS AN OFFSET NAME, deliberately, and on this file's own
	 * stated terms: nothing in the object READS it, so "the analog rate
	 * mask" is where the four bytes came from and not what the field is
	 * for -- exactly the adjacency the paragraph above declines for the
	 * five dwords.  CLAUDE.md's 3120 rule; the derivation is here instead
	 * of in a name every later reader would believe.
	 */
	short short_14;				/* +0x14                  */
	/*
	 * +0x16 was `pad_16[2]`, the struct's LAST member -- REMOVED
	 * (finding F10151).  This is trailing padding rather than a gap
	 * before a named field: the struct's own alignment (forced to 4 by
	 * its four `unsigned int`/`float` members) rounds `sizeof` up from
	 * `short_14`'s end at +0x16 to +0x18 with no member needed to name
	 * the gap.  `V90Modem.h` embeds this struct BY VALUE immediately
	 * followed by `V90MP mp` with no pad between them, and
	 * `V90ModemCtor.cpp`'s existing `V90M_OFF(additionalCPinfo, 0x0cb8,
	 * cpinfo)`/`V90M_OFF(mp, 0x0cd0, mp)` pair is a stronger proof than a
	 * hand-added `sizeof` assertion would be: if the compiler's natural
	 * padding did not land `sizeof(tagV90AdditionalCPinfo)` at exactly
	 * 0x18, `mp`'s offset assertion would fail to compile outright.
	 * This does not resolve the file's own "size is adjacency, not
	 * asserted" caveat about whether 0x18 is the ORIGINAL author's size
	 * -- that question is unchanged by this edit either way.
	 *
	 * Negative check found one false-positive worth recording: `dis.py`
	 * over `V90CPPacker` shows a `movswl 0x16(%ebx)` at .text+0x3c8ea,
	 * but tracing `%ebx` (set at +0x3c8b8..+0x3c8bf as `arg3 + 0x22`,
	 * the caller's `short *` output buffer) shows it is unrelated to
	 * this struct's own pointer (arg2, loaded separately from
	 * `0x184(%esp)`).  No genuine access to offset 0x16/0x17 of a
	 * `tagV90AdditionalCPinfo *` was found in `V90CPPacker`,
	 * `setV92CPpckFromParamsInfo`, `V90Demodulator::enterRRN`, or either
	 * `V90Modulator`/`V90Demodulator` constructor.
	 */
};

#endif /* DSPLIB_TAGV90ADDITIONALCPINFO_H */

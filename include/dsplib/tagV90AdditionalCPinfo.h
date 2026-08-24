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
	 */
	unsigned int word_04;			/* +0x04                  */

	/*
	 * +0x08  A FLOAT, and it is the destination that types it: the four
	 * bytes go to `V92CP::flt_10`, which `V92CP::infoToBits` sends as
	 * sixteen magnitude entries weighted 4 down to 2^-13 off `fltTable_2`.
	 * The copy itself is a `movl` -- GCC 3.4.2 copies a float that way at
	 * `-O3`, which was probed rather than assumed (finding 5820) -- so the
	 * width is forced and the type comes from the other end.
	 */
	float float_08;				/* +0x08                  */

	/*
	 * +0x0c  Four bytes, low one into `V92CP::byte_03`, which is
	 * `bits[35]` stored whole.
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
	 * named for its writer and not for its meaning, and finding 4342's
	 * rule is why the retraction is spelled out rather than the sentence
	 * simply deleted.
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
	unsigned char pad_16[2];		/* +0x16 not modelled     */
};

#endif /* DSPLIB_TAGV90ADDITIONALCPINFO_H */

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
 * writes +0x10, which is what carved the field below out of the pad; the
 * other 0x14 bytes are still untouched by anything reconstructed.
 *
 * THE SIZE IS ADJACENCY AND IS NOT ASSERTED.  0xcd0 - 0xcb8 is where 0x18
 * comes from, and no allocation confirms it.
 */

#ifndef DSPLIB_TAGV90ADDITIONALCPINFO_H
#define DSPLIB_TAGV90ADDITIONALCPINFO_H

struct tagV90AdditionalCPinfo {
	unsigned char pad_00[0x10];		/* +0x00 not modelled     */

	/*
	 * +0x10  MODELLED, UNNAMED.  `V90Demodulator::enterRRN` stores a 0 or
	 * a 1 here and is the only access to this record anywhere in the
	 * object.  The 1 is reached only when the connection evaluator's
	 * +0x90 and both of `V90Phase4Demodulator`'s +0x3c and +0x38 are
	 * non-zero, so it is a conjunction of three other flags recorded at
	 * the moment a rate renegotiation is detected -- but nothing READS it
	 * in the object, so what it is for is not recoverable here and a name
	 * would be a guess.  The width is the store's (`mov %edx,0x10(%ecx)`).
	 */
	unsigned int word_10;			/* +0x10                  */

	unsigned char pad_14[4];		/* +0x14 not modelled     */
};

#endif /* DSPLIB_TAGV90ADDITIONALCPINFO_H */

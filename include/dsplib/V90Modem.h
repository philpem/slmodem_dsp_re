/*
 * V90Modem.h -- ONE MEMBER OF A CLASS NOBODY HAS RECONSTRUCTED.
 *
 * **This is not an object map and must not be read as one.**  `V90Modem` is a
 * large class -- its constructor is
 * `_ZN8V90ModemC1E12V90ModemSideP19_tagModemParametersP19tagV90DILdescriptorj20V90ComputationalModej`
 * at 0x194e0, 600 bytes of it -- and not one field of it has been read off the
 * disassembly.  The declaration below has NO DATA MEMBERS, so `sizeof` is
 * wrong, and any offset computed through it is meaningless.
 *
 * IT IS SAFE FOR THIS MEMBER AND FOR NOTHING ELSE, because `printTitle` never
 * touches `this`.  0x19400-0x194d2 reads the incoming argument slot exactly
 * never, and its tail call at 0x19475 OVERWRITES that slot with a string
 * pointer before jumping -- which is only correct if the value there was
 * already dead.  So the member is a banner printer that happens to be a
 * member, and calling it through a pointer to nothing works.
 *
 * The class is not polymorphic: two destructors, D1 at 0x192b0 and D2 at
 * 0x19160, and no D0.  A deleting destructor is what GCC emits for a virtual
 * one (finding 228), so there is no vptr at offset 0.  That is stated because
 * it is the one layout fact the object does give away, not because anything
 * here depends on it.
 *
 * WHOEVER RECONSTRUCTS THE REST replaces this file wholesale.  Adding fields
 * to it one at a time, around a declaration written for a member that ignores
 * them, is how a header ends up half describing an object.
 */

#ifndef DSPLIB_V90MODEM_H
#define DSPLIB_V90MODEM_H

class V90Modem {
public:
	/*
	 * Nine messages: three ungated banner lines through `edprintf`, three
	 * gated ones through `dsplibs_debug_printf`, then three more ungated
	 * and a gated banner to close.  See src/pump/v90/V90Modem.cpp for the
	 * order, which is not the order the disassembly is laid out in.
	 */
	void printTitle();
};

#endif /* DSPLIB_V90MODEM_H */

/*
 * V92Modem.h -- ONE MEMBER OF A CLASS NOBODY HAS RECONSTRUCTED.
 *
 * The same caveat as include/dsplib/V90Modem.h, in full: **this is not an
 * object map.**  `V92Modem`'s constructor is
 * `_ZN8V92ModemC1E12V92ModemSideP19_tagModemParametersjP19tagV90DILdescriptor20V92ComputationalMode`
 * at 0x13d30 and no field of the class has been read.  The declaration below
 * has no data members, `sizeof` is wrong, and any offset through it is
 * meaningless.
 *
 * `printTitle` is safe to call through a pointer to nothing for the reason
 * given there: 0x13bf0-0x13c9c never reads the incoming argument slot, and
 * its tail call at 0x13c46 overwrites that slot before jumping.
 *
 * WHOEVER RECONSTRUCTS THE REST replaces this file wholesale.
 */

#ifndef DSPLIB_V92MODEM_H
#define DSPLIB_V92MODEM_H

class V92Modem {
public:
	/*
	 * Eight messages, and the shape differs from `V90Modem::printTitle` in
	 * two ways that are easy to miss: there is no "Components:" line, and
	 * the closing banner is UNGATED here and gated there.  See
	 * src/pump/v90/V92Modem.cpp.
	 */
	void printTitle();
};

#endif /* DSPLIB_V92MODEM_H */

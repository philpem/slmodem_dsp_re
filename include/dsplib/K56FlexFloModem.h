/*
 * K56FlexFloModem.h -- the K56flex modem, which in this object does nothing.
 *
 * Reconstructed from dsplibs.o.  Seventeen members, FORTY BYTES of code
 * between them, and not one instruction in any of them touches `this`:
 *
 *     setMinMaxRates(int, int)      c3                    ret
 *     enterPhase3FullDuplex()       c3                    ret
 *     getK56FlexJaBits(short *)     31 c0 c3              xor %eax,%eax; ret
 *     getK56FlexMpBits(short *)     31 c0 c3              xor %eax,%eax; ret
 *
 * Every other member is 1, 3 or 6 bytes and has the same shape.  This is a
 * class that was declared, given a full method list, and left unimplemented
 * -- the K56flex path is present in the interface and absent from the build.
 *
 * SO THE OBJECT SIZE IS NOT DERIVABLE, and this header does not claim one.
 * Every other class in task #60 is bounded by the largest `this`-relative
 * displacement its members use (finding 215); here that set is empty, so
 * there is no measurement to bound anything with and no data member is
 * declared.  `sizeof(K56FlexFloModem)` is therefore 1, which is C++'s rule
 * for a class with no members and NOT a claim about the blob.  What the
 * differential test can and does check is the complementary statement: that
 * these four write nothing at all through the pointer they are handed, for
 * as far either side of it as the test looks.
 *
 * RETURN TYPES ARE NOT MANGLED (docs/v90cpp.md).  `int` below is read off
 * `xor %eax,%eax` -- the two `getK56Flex*Bits` set the whole return register
 * to zero and the other two set nothing, which distinguishes "returns 0" from
 * "returns void" and no further.  A `short` or an `unsigned` return would
 * compile to the same two bytes.
 *
 * The members below that are declared and not defined are here for the
 * record; nothing defined here calls one, which is what keeps the batch
 * closed (docs/v90cpp.md).  Declaring a constructor or destructor is
 * deliberately avoided: it would make the class non-trivial and delete the
 * defaulted members of the union the test fixture puts it in.
 */

#ifndef DSPLIB_K56FLEXFLOMODEM_H
#define DSPLIB_K56FLEXFLOMODEM_H

struct int_complex;
struct _tagModemParameters;

class K56FlexFloModem {
public:
	/* Defined in src/pump/v90/K56FlexFloModem.cpp.  All four are stubs. */
	int getK56FlexMpBits(short *);
	int getK56FlexJaBits(short *);
	void setMinMaxRates(int, int);
	void enterPhase3FullDuplex();

	/*
	 * Declared, not defined.  Signatures are the mangling's, so this is a
	 * specification and not a guess; return types are unrecoverable and
	 * are spelled `void` here to say exactly that -- no `void` below was
	 * measured.
	 */
	void k56FlexRunDemodulator(float *, unsigned int, int *, int *);
	void getConstellation(int_complex *, unsigned long);
	void getDFE(int_complex *, unsigned long);
	void getDecisionErrors(int_complex *, unsigned long);
	void getK56MPsReceiver();
	void getLinearEqualizer(int_complex *, unsigned long);
	void getResamplerOffset(int_complex *, unsigned long);
	void getResamplerPhase(int_complex *, unsigned long);
	void externalReset();
	void internalReset();
	void k56FlexEnterPhase3();
};

#endif /* DSPLIB_K56FLEXFLOMODEM_H */

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
	/* Defined in src/pump/v90/K56FlexFloModem.cpp.  All thirteen are stubs. */
	int getK56FlexMpBits(short *);
	int getK56FlexJaBits(short *);
	void setMinMaxRates(int, int);
	void enterPhase3FullDuplex();
	void externalReset();
	void internalReset();
	void k56FlexEnterPhase3();

	/*
	 * The demodulator, and the one member of the class that returns
	 * something other than zero: `mov $0x5,%eax; ret`.  `int` is measured
	 * the same way the two bit getters' return type is and goes no further.
	 */
	int k56FlexRunDemodulator(float *, unsigned int, int *, int *);

	/*
	 * --- THE SIX VISUAL DIAGNOSTICS -------------------------------------
	 *
	 * Three bytes each, `31 c0 c3`, at .text+0x10210 through +0x10260 on a
	 * 0x10 stride, and there is nothing else in any of them:
	 *
	 *     xor %eax,%eax
	 *     ret
	 *
	 * So each one returns zero and touches neither `this` nor the array it
	 * is handed.  Written as `return 0;` because that is the whole of what
	 * the object does, and NOT as a filter, a clamp or an empty loop: the
	 * bytes bound the body at two instructions and there is no room for
	 * structure to have been optimised away.
	 *
	 * `int` RATHER THAN `void`, and it is measured the same way the two
	 * `getK56Flex*Bits` above are: a function returning nothing leaves
	 * %eax alone and these set it.  `VPcmV34GetVisualDiagnostics` reads
	 * the result of five of the six back and returns it, which is the
	 * caller-side half of the same statement.  The WIDTH and the
	 * SIGNEDNESS are not recoverable -- `short`, `unsigned` or a null
	 * pointer return would compile to the same two bytes -- and `int` is
	 * this file's existing convention for exactly that.
	 *
	 * Argument types are the mangling's and exact.
	 */
	int getConstellation(int_complex *, unsigned long);
	int getDFE(int_complex *, unsigned long);
	int getDecisionErrors(int_complex *, unsigned long);
	int getLinearEqualizer(int_complex *, unsigned long);
	int getResamplerOffset(int_complex *, unsigned long);
	int getResamplerPhase(int_complex *, unsigned long);

	/*
	 * Declared, not defined.  The signature is the mangling's, so this is
	 * a specification and not a guess; the return type is unrecoverable
	 * and is spelled `void` to say exactly that -- no `void` here was
	 * measured.
	 */
	void getK56MPsReceiver();
};

/*
 * The two C-linkage helpers that share the class's translation unit; see
 * src/pump/v90/K56FlexFloModem.cpp for why they are attributed there.
 *
 * `K56FLEX_OBJECT_SIZE` is the `movl $0x14,(%esp)` at .text+0x102a3 and
 * nothing else is known about the twenty bytes: `K56FLEX_Create` does not
 * write them, `K56FLEX_Delete` does not read them, and `vpcm_create` only
 * stores the pointer at root +0xac44 and tests it for null.  Whether the block
 * is `K56FlexFloModem` itself is NOT settled by this -- the class's own header
 * says not one of its seventeen members touches `this`, so no member bounds a
 * size to compare against twenty.
 */
#define K56FLEX_OBJECT_SIZE	0x14

extern "C" {
void *K56FLEX_Create(void *, void *, void *, int);
void K56FLEX_Delete(void *obj);
}

#endif /* DSPLIB_K56FLEXFLOMODEM_H */

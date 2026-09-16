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
 * displacement its members use (finding F215); here that set is empty, so
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
 * Every member below is now defined in src/pump/v90/K56FlexFloModem.cpp;
 * nothing defined there calls another, which is what keeps the batch closed
 * (docs/v90cpp.md).  The constructor and destructor ARE declared now --
 * this used to be avoided for a union fixture's sake, but no fixture puts
 * the class in a union today (every test reaches it through a cast), and
 * the blob has all four symbols (C1/C2 at 0x10170/0x10180, D1/D2 at
 * 0x10190/0x101a0, one `ret` each).
 */

#ifndef DSPLIB_K56FLEXFLOMODEM_H
#define DSPLIB_K56FLEXFLOMODEM_H

struct int_complex;
struct _tagModemParameters;

class K56FlexFloModem {
public:
	/**
	 * @brief Construct a K56flex modem object. Initialises nothing.
	 *
	 * Defined in src/pump/v90/NoK56Flex.cpp; body is empty. The
	 * three parameters are the mangling's and never read.
	 * @param unused0 Ignored pointer; no semantic role established.
	 * @param unused1 Ignored integer; no semantic role established.
	 * @param unusedParams Ignored parameter block; not stored or owned.
	 */
	K56FlexFloModem(void *unused0, int unused1,
			_tagModemParameters *unusedParams);

	/** @brief Destroy a K56flex modem object. Releases nothing (empty body). */
	~K56FlexFloModem();

	/**
	 * @brief Stub. Reads nothing, writes nothing through @p out.
	 * @param out Unused.
	 * @return Always 0.
	 */
	int getK56FlexMpBits(short *out);

	/**
	 * @brief Stub. Reads nothing, writes nothing through @p out.
	 * @param out Unused.
	 * @return Always 0.
	 */
	int getK56FlexJaBits(short *out);

	/**
	 * @brief Stub; does not change either rate limit.
	 * @param unused0 Ignored first rate argument.
	 * @param unused1 Ignored second rate argument.
	 */
	void setMinMaxRates(int unused0, int unused1);

	/** @brief Stub. Empty body. */
	void enterPhase3FullDuplex();

	/**
	 * @brief Stub for the K56flex side of the modem's external reset.
	 *
	 * Empty body -- contrast VPcmFloModem::externalReset(), which
	 * reinitialises three parameter blocks, twenty-odd flags and a
	 * demodulator for the V.90 side (finding F1090). `VPcmV34Create`
	 * calls this.
	 */
	void externalReset();

	/** @brief Stub. Empty body. */
	void internalReset();

	/** @brief Stub. Empty body. */
	void k56FlexEnterPhase3();

	/**
	 * @brief Stub K56flex demodulator entry point.
	 *
	 * Reads none of its four arguments and does not write through either
	 * `int *` output. Always reports a fixed status of 5 -- not zero and
	 * not derived from anything -- which `k56FlexPhase34`'s completion
	 * arms elsewhere test as load-bearing even though nothing in this
	 * class computes it (deviation D155).
	 *
	 * @return Always 5.
	 */
	int k56FlexRunDemodulator(float *unused0, unsigned int unused1,
				 int *unused2, int *unused3);

	/*
	 * THE SIX VISUAL DIAGNOSTICS.  Each is `xor %eax,%eax; ret` and
	 * nothing else -- returns zero and touches neither `this` nor the
	 * array it is handed, reporting "nothing to show" for a K56flex
	 * session's constellation, equaliser, DFE or decision errors.
	 * `VPcmV34GetVisualDiagnostics` reads back and returns the result of
	 * four of these; the two resampler ones below are called but their
	 * answer is discarded by the dispatcher (see src/pump/v34/v34diag.cpp).
	 * Argument types are the mangling's and exact.
	 */

	/**
	 * @brief Stub. Reports no constellation points.
	 * @return Always 0.
	 */
	int getConstellation(int_complex *, unsigned long);

	/**
	 * @brief Stub. Reports no DFE taps.
	 * @return Always 0.
	 */
	int getDFE(int_complex *, unsigned long);

	/**
	 * @brief Stub. Reports no decision-error points.
	 * @return Always 0.
	 */
	int getDecisionErrors(int_complex *, unsigned long);

	/**
	 * @brief Stub. Reports no linear-equalizer taps.
	 * @return Always 0.
	 */
	int getLinearEqualizer(int_complex *, unsigned long);

	/**
	 * @brief Stub. Called by the dispatcher, but its answer is discarded
	 *        there in favour of a fixed reply.
	 * @return Always 0.
	 */
	int getResamplerOffset(int_complex *, unsigned long);

	/**
	 * @brief Stub. Called by the dispatcher, but its answer is discarded
	 *        there in favour of a fixed reply.
	 * @return Always 0.
	 */
	int getResamplerPhase(int_complex *, unsigned long);

	/** @brief Stub. @return Always 0. */
	int getK56MPsReceiver();
};

/*
 * The three C-linkage helpers that share the class's translation unit; see
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
/**
 * @brief Allocate the K56flex side's opaque object.
 * @param unused0 Ignored pointer; not retained.
 * @param unused1 Ignored pointer; not retained.
 * @param unused2 Ignored pointer; not retained.
 * @param unused3 Ignored integer; does not affect allocation size.
 * @return A newly allocated, `K56FLEX_OBJECT_SIZE`-byte block.
 */
void *K56FLEX_Create(void *unused0, void *unused1, void *unused2, int unused3);

/**
 * @brief Free a K56flex object allocated by K56FLEX_Create(), if non-NULL.
 * @param obj The object to free, or NULL (a no-op).
 */
void K56FLEX_Delete(void *obj);

/**
 * @brief K56flex session-termination hook. Reads nothing and does nothing.
 * @return Always 0.
 */
int K56FLEX_SessionTermination(void);
}

#endif /* DSPLIB_K56FLEXFLOMODEM_H */

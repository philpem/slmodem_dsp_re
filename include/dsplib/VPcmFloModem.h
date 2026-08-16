/*
 * VPcmFloModem.h -- the V.90/V.92 modem's face to the V.34 handshake.
 *
 * Reconstructed from dsplibs.o.  The class has twenty-six members in the blob
 * and this tree writes SIX of them: `getUinfoValue`, `setPhaseIIinfo`,
 * `getV90CpBits`, `getV90JaBits`, `setPcmSessionType` and `enterPhase3`.
 * Everything else is left undeclared rather than declared-and-undefined,
 * because nothing here calls it and a declaration nobody needs is a claim
 * nobody checked.
 *
 * NOT POLYMORPHIC.  tools/cppstruct.py lists the destructor with the `D1` and
 * `D2` variants and no `D0`, and GCC emits a deleting destructor only for a
 * virtual one, so +0x00 is a real member and there is no vptr (finding 228 is
 * the four classes where that is not true).
 *
 * ===========================================================================
 * THE OBJECT REALLY IS THIRTY-TWO KILOBYTES
 * ===========================================================================
 *
 * docs/v90cpp.md used to say that VPcmFloModem "reaches +32,612, which almost
 * certainly means it indexes *through* `this` into an enclosing session object
 * rather than being that large".  That is finding 268's warning applied in
 * good faith, and for this class it is WRONG.  Run finding 268's own check --
 * trace the base register of every large displacement back to the prologue --
 * and every one of them comes off the `this` stack slot:
 *
 *     getV90JaBits       mov 0xc(%esp),%ecx    -> cmpb 0x7dce(%ecx)
 *     getV90CpBits       mov 0x20(%esp),%ebx   -> movzwl 0x7dd6(%ebx)
 *     setPcmSessionType  mov 0x20(%esp),%esi   -> mov 0x612c(%esi)
 *     setPhaseIIinfo     mov 0x40(%esp),%esi   -> lea 0x7ed4(%esi)
 *     getUinfoValue      mov 0xd0(%esp),%ebx   -> movl $0x0,0x7ed4(%ebx,%edx,4)
 *
 * There is no intervening load.  So these five alone put the object at
 * **0x7f28 = 32,552 bytes at least** -- the last of the four float arrays ends
 * at +0x7f27 -- and the 32,612 the whole class reaches is 60 bytes further on.
 *
 * A FLOOR IS STILL NOT A SIZE, so no size is asserted here or in the .cpp.
 * Twenty-one of the twenty-six members have not been read, and the four
 * arrays at +0x7dd8 are only known to be 21 entries because that is how many
 * `getUinfoValue` clears.  The .cpp asserts OFFSETS; the test allocates a slot
 * larger than the floor and compares the whole slot, which catches a store
 * past the last modelled field as well as one inside it.
 *
 * ===========================================================================
 * A V90Modem IS EMBEDDED AT +0x1758, AND THAT IS MEASURED THREE WAYS
 * ===========================================================================
 *
 *   1. `setPcmSessionType` ends `lea 0x1758(%esi),%edi ... jmp
 *      V90Modem::setSessionFlag` -- an ADD before the call, not a load, which
 *      is the same signature finding 268 used for the two demodulators.
 *   2. `getV90CpBits` reads +0x175c as a V90Demodulator*, and V90Modem's
 *      `demodulator` is at +0x04.  0x1758 + 4 = 0x175c.
 *   3. `setPhaseIIinfo` reads +0x1760 as a V90Phase2Info*, and V90Modem's
 *      declared prefix ends at 0x49c0; 0x1758 + 0x49c0 = 0x6118, immediately
 *      before the first field after it that anything here touches, +0x611c.
 *
 * The third is also the weak point: V90Modem asserts no size either, so the
 * four bytes at +0x6118 could belong to either object.  What holds the map
 * together is that the .cpp asserts `sizeof(V90Modem) == 0x49c0` alongside
 * the offsets, so a later batch that gives V90Modem more prefix breaks the
 * build here instead of silently shifting every offset past +0x6118.
 *
 * Two fields inside that V90Modem were carved out of its `pad_08` for this
 * batch: `phase2Info` at +0x08 (this + 0x1760) and `ptr_49b4` at +0x49b4
 * (this + 0x610c).  See include/dsplib/V90SessionFlag.h.
 *
 * ===========================================================================
 * NAMES
 * ===========================================================================
 *
 * The mangling never carries a data member's name (finding 226).  Where the
 * blob names a field some other way the name below is the object's own:
 *
 *   +0x7dce, +0x7dcf, +0x7dd0   `setTerminateJaFlag`, `setTerminateCpFlag`
 *                               and `setTerminateCpNotFlag` are three of the
 *                               twenty-six members, and getV90CpBits prints
 *                               "(terminateCp=%d, terminateCpNot=%d)" from
 *                               +0x7dcf and +0x7dd0 in that order -- which
 *                               leaves +0x7dce, the one getV90JaBits tests,
 *                               for the Ja flag.
 *   +0x7dd6                     `setMinNofTransmitSequences(unsigned short)`
 *                               is a member and this is the unsigned short
 *                               the sequence counter is compared against.
 *   +0x7dd4                     the "#%d" in "End of CP #%d tx....".
 *   +0x7e80                     `L2`: setPhaseIIinfo installs it in
 *                               V90Phase2Info's `L2` slot and getUinfoValue
 *                               prints it as "L2[%d]".
 *
 * Everything else is offset-named or descriptive-and-hedged, and each says
 * below which it is.
 */

#ifndef DSPLIB_VPCMFLOMODEM_H
#define DSPLIB_VPCMFLOMODEM_H

#include "dsplib/ANSamToneDetector.h"	/* embedded at +0x6f5c, 0x3c    */
#include "dsplib/GenericIIR.h"		/* embedded at +0x7f28, 0x34    */
#include "dsplib/SineWave.h"		/* embedded at +0x6f9c, 0x10    */
#include "dsplib/V90SessionFlag.h"	/* V90Modem, and V90Phase2Info */
#include "dsplib/V90Phase3Modulator.h"	/* tagV90DILdescriptor          */
#include "dsplib/V92EchoCanceller.h"	/* embedded at +0x6bd0, 0x3c    */
#include "dsplib/V92Modem.h"		/* embedded at +0x6124, 0xaac   */
#include "dsplib/V92Phase2Info.h"

/* A pointer only; src/pump/v90/VPcmFloModem.cpp includes the definition. */
class V92Parameters;

/*
 * How many entries of each of the four float arrays `getUinfoValue` clears:
 * `inc %edx; cmp $0x14,%edx; jle` is 0 through 20 inclusive.  The same bound
 * V90PHASE2INFO_L2 and V92PHASE2INFO_L2 record from the two printers, from
 * two other translation units.
 */
#define VPCM_L2			21

/*
 * How many probe tones `getUinfoValue` walks: `cmp $0x18,%edx; jle` is 0
 * through 24, and V34_PROBE_RESULTS in dsplib/v34fsk.h is 25.
 */
#define VPCM_PROBE_TONES	25

class VPcmFloModem {
public:
	/*
	 * 0xfa60 (C1) and 0xfee0 (C2), 0x28b = 651 bytes each.  Six member
	 * objects and the wiring between them; see
	 * src/pump/v90/VPcmFloModemCtor.cpp, which is where every argument
	 * this hands on is accounted for.
	 *
	 * THERE IS NO DECLARED DESTRUCTOR AND THAT IS DELIBERATE.  `D1` at
	 * 0xd0a0 and `D2` at 0xd030 are six member destructor calls in
	 * reverse declaration order and nothing else, which is exactly what
	 * GCC emits for an IMPLICITLY-DECLARED one over these six members.
	 * Declaring an empty one would not be the same function: CXXFLAGS
	 * carries `-fno-lifetime-dse`, so a written body is not elided.
	 *
	 * THE PRICE IS THAT OUR OBJECT HAS NEITHER SYMBOL.  An implicit
	 * destructor is implicitly inline, our build has one call site for it
	 * (`VPCMXF_Delete`), and GCC inlines it there and emits no out-of-line
	 * copy -- so 194 bytes of the blob are behaviourally reproduced and
	 * symbolically absent.  Deviation D237, and
	 * src/pump/v90/VPcmFloModemCtor.cpp says why the two ways of forcing
	 * the symbols out would each break something that currently matches.
	 */
	VPcmFloModem(void *v34Object, V90ModemSide side,
		     _tagModemParameters *modemParams, unsigned int nSamples,
		     V90ComputationalMode v90Mode,
		     V92ComputationalMode v92Mode);

	/*
	 * Turn the V.34 line probe into the Phase 2 record, and report
	 * whatever the modem already knows about Uinfo.
	 *
	 * Returns 0 unless it finds a non-zero short through the modem; the
	 * return type is not mangled, and `int` is what the object leaves in
	 * %eax on every path (a `movswl`, or a cleared register).
	 */
	int getUinfoValue(short probeValid);

	/*
	 * Fill both Phase 2 records out of the 41 INFO0 bits, install the
	 * four float arrays in them, and re-apply the PCM session type.
	 *
	 * `int *`, not `const int *`: the mangling is `PKi` for a const one
	 * and this symbol is `Pi`.
	 */
	void setPhaseIIinfo(int *info0, int rtd);

	/* Pack the next CP symbol.  Returns 1 once CP is to be terminated. */
	int getV90CpBits(short *bits);

	/* Pack the next JA symbol.  Returns 1 once JA is to be terminated. */
	int getV90JaBits(short *bits);

	/* Record V.90 (0) or V.92 (non-zero) and tell the modem. */
	void setPcmSessionType(int sessionType);

	/*
	 * Clear the transmit bookkeeping, hand phase 3 to the demodulator,
	 * and pack the DIL descriptor into `bitVector` as the JA vector.
	 *
	 * Twenty-one constant stores, two calls and two diagnostics; it reads
	 * nothing but `modem.demodulator`, `dil` and the `nofBits` the packer
	 * has just written.  The return type is not mangled and nothing here
	 * establishes it: `void` is what the object supports, since every
	 * path falls into the tail of `edprintf` and %eax is never set.
	 */
	void enterPhase3();

	/*
	 * `_ZN12VPcmFloModem13externalResetEv`, added by task #88.  It is
	 * `VPcmV34Create`'s way of putting a constructed modem back to its
	 * starting state, and its shape is nearly `enterPhase3`'s: the same
	 * six flags, the same five cleared bytes, the same three CP fields.
	 * Falls off the end into `dsplibs_debug_printf`'s tail, so `void`.
	 */
	void externalReset();

	/*
	 * Set `v34BaudAllow` for a V.90 session and for a V.34 one.
	 *
	 * Six `movb` each and nothing else -- no read, no call, no return
	 * value set -- so `void` is what the object supports and the only
	 * difference between the two is the last entry.  The names are the
	 * object's own, and they are what settles that the array is per-baud;
	 * see the comment on `v34BaudAllow` for what that does and does not
	 * claim.  Nothing in the object calls either, so both survive only as
	 * the out-of-line copy, exactly like `setScramble` in v34shell.h.
	 */
	void setV34BaudForV90();
	void setV34BaudForV34();

	/*
	 * --- FOUR MEMBERS THIS TREE HAS NOT WRITTEN --------------------------
	 *
	 * `VPcmV34Progress` calls all four and nothing else does, so they are
	 * declared here and defined nowhere.  They are marked WEAK in the one
	 * translation unit that calls them -- `src/pump/v34/v34pcmmain.cpp`
	 * defines `DSPLIB_VPCMFLO_UNWRITTEN` before including this file -- so
	 * the reference resolves to zero rather than failing the link of all
	 * 78 test binaries, and the caller tests the pointer-to-member before
	 * it calls through it.  `include/dsplib/vpcm.h` carries the same
	 * arrangement for the five `VPcmV34*` entry points and says why at
	 * length; the rule is the same one, one level further down.
	 *
	 * A TU that DEFINES one of these must not define the macro, or the
	 * definition itself becomes weak.
	 *
	 * The signatures are the manglings and nothing else:
	 *
	 *   _ZN12VPcmFloModem11runPcmModemEPfS0_jPiS1_S1_S1_       2,041 B
	 *   _ZN12VPcmFloModem17v90RunDemodulatorEPfjPiS1_          3,013 B
	 *   _ZN12VPcmFloModem18qcLineVerificationEPfS0_jPiS1_S1_S1_  779 B
	 *   _ZN12VPcmFloModem20vPcmResetPhase3ModemEv               149 B
	 *
	 * The return types are not mangled; `int` is what `VPcmV34Progress`
	 * switches on for the first three (`cmp $0x8,%eax` and friends at
	 * .text+0xbc30, 0xc7fc, 0xba46) and `vPcmResetPhase3Modem`'s result is
	 * discarded, so it is spelled `void`.
	 */
#ifndef DSPLIB_VPCMFLO_UNWRITTEN
#define DSPLIB_VPCMFLO_UNWRITTEN
#endif
	int runPcmModem(float *in, float *out, unsigned int n, int *rxbits,
			int *nrx, int *txbits, int *nbits)
		DSPLIB_VPCMFLO_UNWRITTEN;
	int v90RunDemodulator(float *in, unsigned int n, int *rxbits, int *nrx)
		DSPLIB_VPCMFLO_UNWRITTEN;
	int qcLineVerification(float *in, float *out, unsigned int n,
			       int *rxbits, int *nrx, int *txbits, int *nbits)
		DSPLIB_VPCMFLO_UNWRITTEN;
	void vPcmResetPhase3Modem() DSPLIB_VPCMFLO_UNWRITTEN;

	/* --- data members; see the file comment on the naming --- */

	/*
	 * +0x0000  The V.34 object, handed to the constructor as its `void *`
	 * first argument and passed straight back to `V34XF_GetRTD`,
	 * `V34XF_GetInfo0BitsPtr` and `V34XF_GetProbeResultsPtr`, all three of
	 * which take a `void *` (include/dsplib/v34pcmif.h).
	 */
	void *v34Object;

	/*
	 * +0x0004  The DIL descriptor, EMBEDDED, and it is the JA vector's
	 * source: `enterPhase3` ends
	 *
	 *     lea 0x4(%ebx),%eax   -> arg 1 of DILdescriptorPacker
	 *     lea 0x21e(%ebx),%edx -> arg 2, `bitVector`
	 *     lea 0x1736(%ebx),%ecx-> arg 3, `&nofBits`
	 *
	 * an ADD off `this` for the descriptor, not a load, so the block is
	 * in the object.  Its TYPE is settled by two independent facts:
	 * `DILdescriptorPacker` takes a `const tagV90DILdescriptor *`
	 * (include/dsplib/DILdescriptorPacker.h), and
	 * `sizeof(tagV90DILdescriptor)` is 0x213, which runs from +0x004 to
	 * +0x216 and stops exactly where the next measured field, +0x217,
	 * begins.  The span this replaced was 0x213 bytes of `pad_0004`.
	 */
	tagV90DILdescriptor dil;

	/*
	 * +0x0217  WHICH OF V.34's SIX SYMBOL RATES THIS SESSION WILL ACCEPT,
	 * one byte each, 1 for allowed.
	 *
	 * It used to be `flags_0217`, offset-named because "nothing
	 * establishes what they select".  Three readings settle it now and
	 * none of them is this array's own writers:
	 *
	 *   - `chkForceBaudRate` (v34pcmif.c) takes `p3548 + 0x217` as `sel`
	 *     when a V.90 receiver is up, and then INDEXES IT 1..5 against a
	 *     cap it prints as "max V34 baud rate index = %d", clearing every
	 *     entry at or above the cap.  An index that runs 0..5 against a
	 *     quantity the object itself calls a baud rate index is what
	 *     names the array; a write of six literals never could.
	 *   - In its other two arms the same function indexes a LOCAL
	 *     `unsigned char allow[6]` through the same `sel`, so the two are
	 *     the same shape by construction.
	 *   - `VPcmFloModem::setV34BaudForV90` and `::setV34BaudForV34` are
	 *     six stores to this array and nothing else, and the object's own
	 *     names for them say the six are V.34 baud.
	 *
	 * The two `setV34BaudFor*` differ in the LAST entry alone -- V.90
	 * bars it, V.34 allows it -- which is the same entry `enterPhase3`
	 * and `externalReset` bar and `getUinfoValue` allows.  V.34 has
	 * exactly six symbol rates (2400, 2743, 2800, 3000, 3200, 3429) and
	 * index 5 is the fastest, so every one of those five sites reads as
	 * "3429 baud off".  WHAT IS NOT ESTABLISHED is the index-to-rate
	 * mapping itself: `chkForceBaudRate` bars rates in ascending index
	 * order, which fixes the direction but not the first entry, and
	 * nothing read here states it.  The name claims the array is per-baud
	 * and does not claim which baud.
	 */
	unsigned char v34BaudAllow[6];

	unsigned char pad_021d[1];		/* +0x021d not modelled */

	/*
	 * +0x021e  The bit vector currently being transmitted, indexed by
	 * `bitPointer` and `nofBits` long.  `getV90JaBits` reads it two bits
	 * at a time; `getV90CpBits` copies `nofBits` of it into
	 * `cpBitVector` when CP gives way to CPnot.
	 *
	 * THE DECLARED LENGTH IS A SPAN, NOT THE ORIGINAL'S BOUND.  Nothing
	 * read here bounds the array: it is indexed by a count that lives in
	 * `nofBits`, which these five methods only read.  The declaration
	 * runs to the next offset that IS measured, so a store anywhere in
	 * the span is inside a modelled field rather than off the end of one.
	 */
	short bitVector[(0x1736 - 0x21e) / 2];

	/*
	 * +0x1736  How many entries of `bitVector` are live.  Read as a
	 * signed short (`movswl`) by both getters and compared against zero
	 * as a 16-bit quantity (`cmpw`).
	 */
	short nofBits;

	/*
	 * +0x1738  Where the next bit comes from.  Read with `movzwl` every
	 * time, so unsigned, and `resetBitPointer()` is one of the class's
	 * twenty-six members.
	 */
	unsigned short bitPointer;

	/*
	 * +0x173a  Three bytes `enterPhase3` clears, immediately before
	 * `flag_173d` and `flag_173e`, which it clears in the same run of
	 * five `movb $0x0`.  Offset-named: nothing reads them here.
	 */
	unsigned char flags_173a[3];

	/*
	 * +0x173d  A byte `getUinfoValue` tests: non-zero skips the lookup
	 * through the modem entirely and takes the default-flags path.
	 * `enterPhase3` clears it, so entering phase 3 restores the lookup.
	 * Offset-named.
	 */
	unsigned char flag_173d;

	/*
	 * +0x173e  The fifth of `enterPhase3`'s run of five cleared bytes.
	 * Offset-named; nothing else this tree has read touches it.
	 */
	unsigned char flag_173e;

	unsigned char pad_173f[1];			/* +0x173f         */

	/*
	 * +0x1740  Four bytes the constructor zeroes
	 * (`mov %ebp,0x1740(%ebx)` at 0xfc10, with %ebp zero) and nothing
	 * else in this tree touches.  Carved out of `pad_173f`, which used to
	 * run from +0x173f to +0x1758; the two spans either side of it are
	 * still unmodelled.  Offset-named -- a store of zero says a field is
	 * there and four bytes wide, and nothing else.
	 */
	unsigned int word_1740;				/* +0x1740         */
	unsigned char pad_1744[0x1758 - 0x1744];	/* +0x1744         */

	/*
	 * +0x1758  The V90Modem, EMBEDDED.  See the file comment for the
	 * three independent measurements, and the .cpp for the size
	 * assertion that keeps every offset below it honest.
	 */
	V90Modem modem;

	/*
	 * +0x6118  Four bytes between the end of V90Modem's modelled prefix
	 * and the first field after it.  Which object they belong to is not
	 * settled -- V90Modem's size is a floor, not a measurement -- and
	 * `externalReset` does not settle it either: it clears the first two
	 * with two `movb $0x0`, through the VPcmFloModem and not through the
	 * V90Modem, which is what the compiler emits either way.
	 */
	unsigned char byte_6118;
	unsigned char byte_6119;
	unsigned char pad_611a[2];

	/*
	 * +0x611c  V.90 or V.92, as a 0/1 int: `setPcmSessionType` stores
	 * `(arg != 0)` here and `setPhaseIIinfo` reads it back to re-apply
	 * it.  Both print "setting PCM session to V.%d" with 92 for non-zero
	 * and 90 for zero, which is where the name comes from.
	 */
	int pcmSessionType;

	/*
	 * +0x6120  Which layout `setPhaseIIinfo` reads the INFO0 bits in.
	 * It gates exactly two things and nothing else: whether INFO0 bits 38
	 * and 39 are stored as `txPowerMeasurementPoint` and `pcmType` at
	 * all, and whether INFO0 bit 26 or bit 27 is the "short phase 2
	 * remote" byte.  Descriptive and hedged -- the object never names it.
	 */
	int info0Layout;

	/*
	 * +0x6124  The V92Modem, EMBEDDED, 0xaac bytes.  The constructor
	 * calls `_ZN8V92ModemC1E...` on `this + 0x6124` -- an ADD off `this`
	 * and not a load -- and `VPCMXF_Delete` and `~VPcmFloModem` both call
	 * `_ZN8V92ModemD1Ev` on the same address.  `sizeof(V92Modem)` is
	 * 0xaac (src/pump/v90/V92Modem.cpp), and 0x6bd0 - 0x6124 is 0xaac, so
	 * the two agree and the object stops exactly where the next member
	 * begins.  This is finding 1320's technique and V92Modem.h's own
	 * upper bound, seen from the other side.
	 *
	 * THIS SPAN USED TO BE `pad_6124[4]`, `v92Params`, `v92Phase2Info`
	 * and `pad_6130[0x6f98 - 0x6130]`.  Those two pointers were read
	 * correctly and are still at exactly the offsets they were recorded
	 * at: they are `V92Modem::parameters` (+0x004 of the V92Modem, so
	 * +0x6128) and `V92Modem::phase2Info` (+0x008, so +0x612c).  What
	 * changed is that they are now reached through the member that owns
	 * them, which is what makes the constructor's
	 * `mov 0x6128(%ebx),%ecx` -- a load of the V92Modem's OWN field to
	 * pass to `V92EchoCanceller` -- readable as what it is.
	 */
	V92Modem v92modem;

	/*
	 * +0x6bd0  The V92EchoCanceller, EMBEDDED and 0x3c bytes; the
	 * constructor builds it on `this + 0x6bd0` with the V92Modem's
	 * `parameters`, and both destructors run `_ZN16V92EchoCancellerD1Ev`
	 * there.  `sizeof(V92EchoCanceller)` is 0x3c
	 * (src/pump/v90/V92EchoCanceller.cpp).
	 */
	V92EchoCanceller echoCanceller;

	/*
	 * +0x6c0c  848 bytes the constructor CLEARS and nothing else in this
	 * tree touches: `lea 0x6c0c(%ebx),%eax` then
	 * `sysdep_memset(p, 0, 0x350)`.
	 *
	 * IT IS NOT PART OF THE ECHO CANCELLER, and the arithmetic is the
	 * proof rather than the guess: 0x6c0c is 0x6bd0 + 0x3c, which is one
	 * past the last byte of a `V92EchoCanceller`, and 0x6c0c + 0x350 is
	 * 0x6f5c, which is exactly where the `ANSamToneDetector` below
	 * begins.  So the span is bounded on both sides by objects whose
	 * sizes are asserted elsewhere, and it belongs to this class.
	 * Offset-named: a memset says how big a thing is and nothing about
	 * what it holds.
	 */
	unsigned char block_6c0c[0x6f5c - 0x6c0c];	/* +0x6c0c         */

	/*
	 * +0x6f5c  The ANSamToneDetector, EMBEDDED and 0x3c bytes
	 * (src/pump/v90/ANSamToneDetector.cpp asserts it).  0x6f5c + 0x3c is
	 * 0x6f98, which is the next field, so the two bound each other.
	 */
	ANSamToneDetector ansam;

	/*
	 * +0x6f98, +0x6fac, +0x6fb0, +0x6fb4  Four words `externalReset`
	 * zeroes and the constructor zeroes again, and the only four things
	 * either touches between the V.92 modem and the CP bit vector.
	 * Nothing reconstructed reads any of them, so they are offset-named.
	 */
	unsigned int word_6f98;				/* +0x6f98         */

	/*
	 * +0x6f9c  A SineWave<float, float>, EMBEDDED and 16 bytes -- four
	 * `Tparam`s, and `Tparam` is `float` here.  The constructor builds it
	 * with (4800.0f, 980.0f, 0.0f, 9600.0f) and both destructors run
	 * `_ZN8SineWaveIffED1Ev` on `this + 0x6f9c`.  0x6f9c + 0x10 is
	 * 0x6fac, the next field, which is finding 1320's bound again and
	 * agrees with SineWave.h's own four-field map.
	 */
	SineWave<float, float> sineWave;

	unsigned int word_6fac;				/* +0x6fac         */
	unsigned int word_6fb0;				/* +0x6fb0         */
	unsigned int word_6fb4;				/* +0x6fb4         */
	unsigned char pad_6fb8[0x6fbc - 0x6fb8];	/* +0x6fb8         */

	/*
	 * +0x6fbc  The CP bit vector, `cpNofBits` long.  Filled by
	 * `getV90CpBits` from `bitVector` at the CP-to-CPnot transition and
	 * read by it on every call.  A SPAN, like `bitVector` above.
	 */
	short cpBitVector[(0x7dcc - 0x6fbc) / 2];

	/* +0x7dcc  How many entries of `cpBitVector` are live.  `movswl`. */
	short cpNofBits;

	/*
	 * +0x7dce, +0x7dcf, +0x7dd0  The three termination requests, named by
	 * the three `setTerminate*Flag(unsigned char)` members and by the
	 * order they appear in getV90CpBits's own message.  All three are
	 * tested with `cmpb $0x0`.
	 */
	unsigned char terminateJa;
	unsigned char terminateCp;
	unsigned char terminateCpNot;

	/*
	 * +0x7dd1  Set to 1 by `getV90CpBits` when it loads the CPnot vector,
	 * and required to be 0 for it to do so -- so the switch happens once
	 * per object.  Descriptive and hedged; the object never names it.
	 */
	unsigned char cpNotLoaded;

	/*
	 * +0x7dd2  How many bits `getV90CpBits` packs into one output word.
	 * `movzbl`, and the shift count is masked to five bits by the
	 * hardware.  `enterPhase3` sets it to 2.  Descriptive and hedged.
	 */
	unsigned char nofBitsPerSymbol;

	unsigned char pad_7dd3[1];		/* +0x7dd3 not modelled */

	/*
	 * +0x7dd4, +0x7dd6  The completed-sequence counter and the minimum it
	 * must reach before CP can give way to CPnot.  Both `movzwl`, and the
	 * comparison between them is unsigned.  `setMinNofTransmitSequences`
	 * is a member of this class and takes an `unsigned short`.
	 * `enterPhase3` sets the counter to 0 and the minimum to 1, so one
	 * completed sequence is enough unless something raises it afterwards.
	 */
	unsigned short nofTransmitSequences;
	unsigned short minNofTransmitSequences;

	/*
	 * +0x7dd8, +0x7e2c, +0x7e80, +0x7ed4  Four parallel float arrays of
	 * VPCM_L2 entries.  `getUinfoValue` clears all four and fills only
	 * the third; `setPhaseIIinfo` installs all four into both Phase 2
	 * records, in this order, and the third lands in the slot both
	 * printers call `L2`.  The other three are offset-named: nothing
	 * establishes what they hold.
	 */
	float array_7dd8[VPCM_L2];
	float array_7e2c[VPCM_L2];
	float L2[VPCM_L2];
	float array_7ed4[VPCM_L2];

	/*
	 * +0x7f28  A GenericIIR<float, double>, EMBEDDED and 0x34 bytes
	 * (include/dsplib/GenericIIR.h reaches the same 52 from
	 * `GenericToneDetector`'s heap allocation of it).  The constructor
	 * builds it with (5, 5, entFiltDen, entFiltNum, 99) and both
	 * destructors run `_ZN10GenericIIRIfdED1Ev` on `this + 0x7f28`.
	 *
	 * THE LAST FLOAT ARRAY ENDS EXACTLY HERE: 0x7ed4 + 21*4 is 0x7f28.
	 * That was already the map; what is new is that the filter fills the
	 * span from there to +0x7f5c, and 0x7f28 + 0x34 is 0x7f5c.
	 */
	GenericIIR<float, double> entFilt;

	/*
	 * +0x7f5c, +0x7f60, +0x7f64  A byte and two words the constructor
	 * clears last, after every member is built.  They are the reason
	 * `sizeof` is 0x7f68 and not 0x7f5c, and they are what turns the
	 * allocation size into a field map: the three of them plus three
	 * bytes of alignment fill the object exactly.  Offset-named.
	 */
	unsigned char byte_7f5c;			/* +0x7f5c         */
	unsigned char pad_7f5d[3];			/* +0x7f5d         */
	unsigned int word_7f60;				/* +0x7f60         */
	unsigned int word_7f64;				/* +0x7f64         */
};

#endif /* DSPLIB_VPCMFLOMODEM_H */
